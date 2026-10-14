#!/usr/bin/perl
##########
#
# make_zones.pl -- build SolTime's default zone list (spec §8):  every city of 100,000+ people
# (GeoNames places.tsv), each with its own city radius;  the zone reaches 1.5x beyond it.
# A city, not a part of one:  GeoNames' sections of cities (feature PPLX:  neighborhoods,
# districts, boroughs' wards), historical and abandoned places (PPLH, PPLQ) are left out.
#
# Usage:  make_zones.pl <places.tsv> <countries.tsv> <GHSL general-characteristics gpkg> <Census places gazetteer> <out zones.tsv>
#
# The city radius (RCH Oct.03.2026):
#   - U.S.:       the larger of the city limits (Census Gazetteer land area) and the dense urban
#                 core (GHSL Urban Centre Database);
#   - elsewhere:  the GHSL dense urban core;
#   - neither:    estimated from population at the country's median urban-core density.
#   radius = sqrt(area / pi);  a city can always declare its own (edit the row:  source "declared").
#
# Matching:  a GHSL urban centre goes to the most populous GeoNames place of its country, near
# its centroid (within 25 km + twice the centre's radius), whose name is the centre's main name
# (else a name from its list);  a Census place goes to the GeoNames place of the same state whose
# name it carries, nearest within 30 km (+ twice the place's radius).
#
# Duplicates:  GeoNames lists some cities twice.  Two rows of one state (province) and name whose
# discs overlap are one city (the more populous row stays);  two of one country, far apart, with
# the very same population are one city and a copy (the row GHSL or the Census confirms stays;  if
# neither or both are confirmed, both stay and the run reports them).
#
# The GHSL GeoPackage is read directly (a GeoPackage is an SQLite file;  iSqlite* below reads its
# tables):  the urban centres' names, country and area, and their centroids (World Mollweide).
#
# Sources (see ATTRIBUTION.md):  GeoNames (CC BY 4.0);  GHSL UCDB R2024A general characteristics,
# (c) European Union, CC BY 4.0;  U.S. Census Bureau Gazetteer (public domain).
#
# The ramp and jump columns are left "-":  soltime_zonetool fills them (the registry computed once)
# and adds the zone column (the merged zone each city keeps the clock of).  The rebuild:
#
#   GHSL:    https://jeodpp.jrc.ec.europa.eu/ftp/jrc-opendata/GHSL/GHS_UCDB_GLOBE_R2024A/GHS_UCDB_THEME_GLOBE_R2024A/
#            GHS_UCDB_THEME_GENERAL_CHARACTERISTICS_GLOBE_R2024A/V1-2/GHS_UCDB_THEME_GENERAL_CHARACTERISTICS_GLOBE_R2024A_V1_2.zip
#   Census:  https://www2.census.gov/geo/docs/maps-data/data/gazetteer/2023_Gazetteer/2023_Gaz_place_national.zip
#
#   perl make_zones.pl places.tsv countries.tsv GHS_UCDB_THEME_GENERAL_CHARACTERISTICS_GLOBE_R2024A.gpkg \
#        2023_Gaz_place_national.txt zones_raw.tsv
#   soltime_zonetool zones_raw.tsv zones.tsv
#
######
use strict;
use warnings;
use utf8;

my ($fPlaces, $fCountries, $fGhsl, $fCensus, $fOut) = @ARGV;
die "usage:  make_zones.pl <places.tsv> <countries.tsv> <ghsl.gpkg> <census gazetteer> <out zones.tsv>\n" unless defined $fOut;

my $MIN_POP = 100000;
my $PI      = 3.14159265358979;


##########
# Places and countries
#####
    my (%countryName, %countryOf);
    open(my $fc, '<:encoding(UTF-8)', $fCountries) or die "$fCountries:  $!\n";
    while (<$fc>)
    {
        next if /^#/;
        chomp;
        my ($cc, $name) = split /\t/;
        next unless defined $name;
        $countryName{$cc}     = $name;
        $countryOf{lc $name}  = $cc;
    }
    close $fc;

    my %notCity = map { $_ => 1 } qw(PPLX PPLH PPLQ);
    my @places;                 # [ name, ascii, cc, admin1, lat, lon, pop, feature ]
    open(my $fp, '<:encoding(UTF-8)', $fPlaces) or die "$fPlaces:  $!\n";
    while (<$fp>)
    {
        next if /^#/;
        chomp;
        my @f = split /\t/, $_, -1;
        next unless @f >= 8;
        push @places, [ @f[0 .. 7] ];
    }
    close $fp;

    # Name lookups:  (country, lower-cased name or ascii name) -> place indices
    my %byName;
    for my $i (0 .. $#places)
    {
        my $p = $places[$i];
        next if $notCity{ $p->[7] };
        push @{ $byName{"$p->[2]|" . lc $p->[0]} }, $i;
        push @{ $byName{"$p->[2]|" . lc $p->[1]} }, $i if lc $p->[1] ne lc $p->[0];
    }


##########
# GHSL urban centres:  area (km2) by place, and each country's median core density
#####
    my $gpkg    = iSqliteOpen($fGhsl);
    my %tables  = iSqliteTables($gpkg);
    my ($ucTable) = grep { /^GHSL_UCDB_THEME_GENERAL_CHARACTERISTICS/ } keys %tables;
    die "$fGhsl:  no urban centre table\n"  unless defined $ucTable;
    die "$fGhsl:  no UC_centroids table\n"  unless defined $tables{UC_centroids};

    # Centroids:  World Mollweide (ESRI:54009) -> degrees
    my %ucAt;                   # ID_UC_G0 -> [ lat, lon ]
    {
        my $ci = $tables{UC_centroids}{col};
        iSqliteScan($gpkg, $tables{UC_centroids}{root}, sub
        {
            my ($rowid, $v) = @_;
            $ucAt{ $v->[ $ci->{ID_UC_G0} ] } = [ iMollweideInv($v->[ $ci->{GC_UCC_LON_2025} ], $v->[ $ci->{GC_UCC_LAT_2025} ]) ];
        });
    }

    my (%ghslArea, %densities);
    my ($ucCount, $ucMatched) = (0, 0);
    my $uc = $tables{$ucTable}{col};
    iSqliteScan($gpkg, $tables{$ucTable}{root}, sub
    {
        my ($rowid, $v) = @_;
        my ($id, $main, $list, $country, $area, $pop) = map { $v->[ $uc->{$_} ] } qw(ID_UC_G0 GC_UCN_MAI_2025 GC_UCN_LIS_2025 GC_CNT_GAD_2025 GC_UCA_KM2_2025 GC_POP_TOT_2025);
        ++$ucCount;
        my $cc = defined $country ? $countryOf{lc $country} : undef;
        return unless defined $cc && defined $area && $area > 0 && $ucAt{$id};
        push @{ $densities{$cc} }, $pop / $area if defined $pop && $pop > 0;

        # Near the centroid:  the most populous place with the main name, else one from the list
        my ($cLat, $cLon) = @{ $ucAt{$id} };
        my $reach = 25.0 + 2.0 * sqrt($area / $PI);
        my @names = ($main // '', split /\s*;\s*/, ($list // ''));
        my $best;
        for my $nm (@names)
        {
            next unless length $nm;
            for my $i (@{ $byName{"$cc|" . lc $nm} // [] })
            {
                next if iKm($cLat, $cLon, $places[$i][4], $places[$i][5]) > $reach;
                $best = $i if !defined $best || $places[$i][6] > $places[$best][6];
            }
            last if defined $best;
        }
        return unless defined $best;
        ++$ucMatched;
        $ghslArea{$best} = $area if !defined $ghslArea{$best} || $area > $ghslArea{$best};
    });

    my %medianDensity;
    my @all;
    for my $cc (keys %densities)
    {
        my @d = sort { $a <=> $b } @{ $densities{$cc} };
        $medianDensity{$cc} = $d[int(@d / 2)];
        push @all, @d;
    }
    @all = sort { $a <=> $b } @all;
    my $globalDensity = $all[int(@all / 2)];


##########
# Census places (U.S.):  land area (km2) by place
#####
    my %stateCode = (
        'Alabama' => 'AL', 'Alaska' => 'AK', 'Arizona' => 'AZ', 'Arkansas' => 'AR', 'California' => 'CA',
        'Colorado' => 'CO', 'Connecticut' => 'CT', 'Delaware' => 'DE', 'District of Columbia' => 'DC',
        'Washington, D.C.' => 'DC', 'Florida' => 'FL', 'Georgia' => 'GA', 'Hawaii' => 'HI', 'Idaho' => 'ID',
        'Illinois' => 'IL', 'Indiana' => 'IN', 'Iowa' => 'IA', 'Kansas' => 'KS', 'Kentucky' => 'KY',
        'Louisiana' => 'LA', 'Maine' => 'ME', 'Maryland' => 'MD', 'Massachusetts' => 'MA', 'Michigan' => 'MI',
        'Minnesota' => 'MN', 'Mississippi' => 'MS', 'Missouri' => 'MO', 'Montana' => 'MT', 'Nebraska' => 'NE',
        'Nevada' => 'NV', 'New Hampshire' => 'NH', 'New Jersey' => 'NJ', 'New Mexico' => 'NM', 'New York' => 'NY',
        'North Carolina' => 'NC', 'North Dakota' => 'ND', 'Ohio' => 'OH', 'Oklahoma' => 'OK', 'Oregon' => 'OR',
        'Pennsylvania' => 'PA', 'Rhode Island' => 'RI', 'South Carolina' => 'SC', 'South Dakota' => 'SD',
        'Tennessee' => 'TN', 'Texas' => 'TX', 'Utah' => 'UT', 'Vermont' => 'VT', 'Virginia' => 'VA',
        'Washington' => 'WA', 'West Virginia' => 'WV', 'Wisconsin' => 'WI', 'Wyoming' => 'WY',
    );

    my %censusArea;
    my ($cenMatched) = (0);
    open(my $fz, '<:encoding(UTF-8)', $fCensus) or die "$fCensus:  $!\n";
    <$fz>;
    my %census;                 # "ST|name" -> [ [lat, lon, km2], ... ]
    while (my $line = <$fz>)
    {
        chomp $line;
        my @f = split /\t/, $line;
        next unless @f >= 12;
        my ($st, $name, $aland, $lat, $lon) = ($f[0], $f[3], $f[6], $f[10], $f[11]);
        $lon =~ s/\s+$//;
        $name =~ s/\s+\((?:balance)\)$//;
        $name =~ s/\s+(?:city|town|village|municipality|borough|CDP|consolidated government|metro government|metropolitan government|unified government|urban county|charter township|township)$//i;
        push @{ $census{"$st|" . lc $name} }, [ $lat, $lon, $aland / 1.0e6 ];
    }
    close $fz;

    for my $i (0 .. $#places)
    {
        my $p = $places[$i];
        next unless $p->[2] eq 'US' && $p->[6] >= $MIN_POP;
        my $st = $stateCode{$p->[3]} or next;
        # Within 30 km, or twice the place's own radius (a big municipality's interior point can
        # sit far from downtown:  Anchorage)
        my $best;
        my $bestD = 1.0e9;
        (my $stName = lc $p->[1]) =~ s/^saint /st. /;				# The Census writes "St. Paul"
        for my $key ("$st|" . lc $p->[1], "$st|" . lc $p->[0], "$st|$stName")
        {
            for my $c (@{ $census{$key} // [] })
            {
                my $d = iKm($p->[4], $p->[5], $c->[0], $c->[1]);
                my $r = sqrt($c->[2] / $PI);
                if ($d < 30.0 + 2.0 * $r && $d < $bestD)    { $bestD = $d;  $best = $c; }
            }
        }
        next unless $best;
        $censusArea{$i} = $best->[2];
        ++$cenMatched;
    }


##########
# The zone list:  every place of 100,000+
#####
    my (%bySource, @zone);       # [ place index, city km, source ]
    my %parts;
    for my $i (0 .. $#places)
    {
        my $p = $places[$i];
        next unless $p->[6] >= $MIN_POP;
        if ($notCity{ $p->[7] })	{ ++$parts{ $p->[7] };  next; }

        my ($area, $src);
        if ($p->[2] eq 'US' && (defined $censusArea{$i} || defined $ghslArea{$i}))
        {
            my $c = $censusArea{$i} // 0;
            my $g = $ghslArea{$i}   // 0;
            ($area, $src) = ($c >= $g) ? ($c, 'census') : ($g, 'ghsl');
        }
        elsif (defined $ghslArea{$i})
        {
            ($area, $src) = ($ghslArea{$i}, 'ghsl');
        }
        else
        {
            my $dens = $medianDensity{$p->[2]} // $globalDensity;
            ($area, $src) = ($p->[6] / $dens, 'estimated');
        }
        push @zone, [ $i, sqrt($area / $PI), $src ];
    }

    # Duplicates:  one country, one name
    my (%sameName, %drop, @dropped, @unresolved);
    for my $z (0 .. $#zone)
    {
        my $p = $places[ $zone[$z][0] ];
        push @{ $sameName{"$p->[2]|" . lc $p->[1]} }, $z;
    }
    for my $key (sort keys %sameName)
    {
        my @z = @{ $sameName{$key} };
        next unless @z > 1;
        for my $x (0 .. $#z - 1)
        {
            for my $y ($x + 1 .. $#z)
            {
                my ($zx, $zy) = ($z[$x], $z[$y]);
                next if $drop{$zx} || $drop{$zy};
                my ($px, $py) = ($places[ $zone[$zx][0] ], $places[ $zone[$zy][0] ]);
                my $d       = iKm($px->[4], $px->[5], $py->[4], $py->[5]);
                my $overlap = ($d < 1.5 * ($zone[$zx][1] + $zone[$zy][1])) && ($px->[3] eq $py->[3]);		# Kansas City, Kansas is not Kansas City, Missouri
                my ($mx, $my) = (($zone[$zx][2] ne 'estimated') ? 1 : 0, ($zone[$zy][2] ne 'estimated') ? 1 : 0);
                my $loser;
                if ($overlap)
                {
                    # One city:  the more populous row stays (then the measured one, then the first)
                    $loser = ($px->[6] != $py->[6]) ? (($px->[6] > $py->[6]) ? $zy : $zx) : ($mx != $my) ? ($mx ? $zy : $zx) : $zy;
                }
                elsif ($px->[6] == $py->[6])
                {
                    # A copy far away:  the confirmed row stays
                    if ($mx != $my)		{ $loser = $mx ? $zy : $zx; }
                    else				{ push @unresolved, sprintf("%s, %s (%.0f km apart, both %s)", $px->[1], $px->[2], $d, $mx ? "measured" : "estimated");  next; }
                }
                next unless defined $loser;
                $drop{$loser} = 1;
                my $pl = $places[ $zone[$loser][0] ];
                push @dropped, sprintf("%s, %s, %s (%.5f %.5f, %d):  %s", $pl->[1], $pl->[3], $pl->[2], $pl->[4], $pl->[5], $pl->[6], $overlap ? sprintf("the same city, %.1f km away", $d) : sprintf("a copy %.0f km away (same population)", $d));
            }
        }
    }

    my @rows;
    for my $z (0 .. $#zone)
    {
        next if $drop{$z};
        my ($i, $cityKm, $src) = @{ $zone[$z] };
        my $p = $places[$i];
        ++$bySource{$src};
        push @rows, sprintf("%s\t%s\t%s\t%.5f\t%.5f\t%d\t%.3f\t%s\t%.3f\t-\t-", $p->[1], $p->[2], $p->[3], $p->[4], $p->[5], $p->[6], $cityKm, $src, 1.5 * $cityKm);
    }

    open(my $fo, '>:encoding(UTF-8)', $fOut) or die "$fOut:  $!\n";
    print $fo "# SolTime zone registry (spec §8):  every city of 100,000+ people; each zone reaches 1.5x the city's radius.\n";
    print $fo "# Overlapping zones merge into one area on the most populous city's clock.  ramp_km / jump_s are the\n";
    print $fo "# registry computed once (soltime_zonetool);  \"-\" = not yet computed.  Rebuild:  make_zones.pl (see its header).\n";
    print $fo "# Sources:  GeoNames, GHSL UCDB R2024A ((c) European Union), U.S. Census Gazetteer -- see ATTRIBUTION.md\n";
    print $fo "# name\tcountry\tadmin1\tlat\tlon\tpopulation\tcity_radius_km\tradius_source\tzone_radius_km\tramp_km\tjump_s\n";
    print $fo "$_\n" for @rows;
    close $fo;

    printf "zones %d  (GHSL urban centres %d, matched %d;  Census places matched %d)\n", scalar @rows, $ucCount, $ucMatched, $cenMatched;
    printf "  radius source:  %s\n", join('   ', map { "$_ $bySource{$_}" } sort keys %bySource);
    printf "  median core density:  global %.0f per km2\n", $globalDensity;
    printf "  not cities (left out):  %s\n", join('   ', map { "$_ $parts{$_}" } sort keys %parts);
    printf "  duplicates dropped %d:\n", scalar @dropped;
    print  "    $_\n" for @dropped;
    printf "  same name and population, far apart, unresolved %d (both kept):\n", scalar @unresolved;
    print  "    $_\n" for @unresolved;


##########
# Helpers
#####
    # Inverse World Mollweide (ESRI:54009:  a sphere of the WGS 84 semi-major axis) -> (lat, lon) degrees
    sub iMollweideInv
    {
        my ($x, $y) = @_;
        my $R  = 6378137.0;
        my $th = iAsin($y / ($R * sqrt(2.0)));
        my $lat = iAsin((2.0 * $th + sin(2.0 * $th)) / $PI);
        my $lon = $PI * $x / (2.0 * $R * sqrt(2.0) * cos($th));
        return ($lat * 180.0 / $PI, $lon * 180.0 / $PI);
    }

    sub iAsin
    {
        my ($s) = @_;
        $s = 1.0 if $s > 1.0;
        $s = -1.0 if $s < -1.0;
        return atan2($s, sqrt(1.0 - $s * $s));
    }


##########
# SQLite, read-only (a GeoPackage is an SQLite file;  no DBD::SQLite here):  the file format's
# table b-trees, records and overflow pages -- https://www.sqlite.org/fileformat.html
#####
    sub iSqliteOpen
    {
        my ($path) = @_;
        open(my $h, '<:raw', $path) or die "$path:  $!\n";
        local $/;
        my $db = <$h>;
        close $h;
        die "$path:  not an SQLite file\n" unless substr($db, 0, 16) eq "SQLite format 3\0";
        my $ps = unpack('n', substr($db, 16, 2));
        $ps = 65536 if $ps == 1;
        return { db => \$db, ps => $ps, u => $ps - ord(substr($db, 20, 1)) };
    }

    # A varint at offset o:  (value, the offset after it)
    sub iSqliteVarint
    {
        my ($r, $o) = @_;
        my $v = 0;
        for (1 .. 8)
        {
            my $b = ord(substr($$r, $o++, 1));
            $v = ($v << 7) | ($b & 0x7F);
            return ($v, $o) unless $b & 0x80;
        }
        return (($v << 8) | ord(substr($$r, $o++, 1)), $o);
    }

    # A record:  its values, in column order (an INTEGER PRIMARY KEY column reads as undef)
    sub iSqliteRecord
    {
        my ($p) = @_;
        my ($hlen, $o) = iSqliteVarint(\$p, 0);
        my @types;
        while ($o < $hlen)
        {
            my $st;
            ($st, $o) = iSqliteVarint(\$p, $o);
            push @types, $st;
        }
        my ($b, @v) = ($hlen);
        for my $st (@types)
        {
            if ($st == 0)                       { push @v, undef; }
            elsif ($st == 6)                    { push @v, unpack('q>', substr($p, $b, 8));  $b += 8; }
            elsif ($st >= 1 && $st <= 5)
            {
                my $len = (0, 1, 2, 3, 4, 6)[$st];
                my $x   = 0;
                $x = ($x << 8) | ord(substr($p, $b + $_, 1)) for 0 .. $len - 1;
                $x -= 2 ** (8 * $len) if $x >= 2 ** (8 * $len - 1);
                push @v, $x;
                $b += $len;
            }
            elsif ($st == 7)                    { push @v, unpack('d>', substr($p, $b, 8));  $b += 8; }
            elsif ($st == 8 || $st == 9)        { push @v, $st - 8; }
            elsif ($st >= 12)
            {
                my $len = int(($st - 12) / 2);
                my $s   = substr($p, $b, $len);
                $b += $len;
                utf8::decode($s) if $st % 2;
                push @v, $s;
            }
        }
        return \@v;
    }

    # Every row of the table b-tree rooted at page:  cb->(rowid, \@values)
    sub iSqliteScan
    {
        my ($s, $page, $cb) = @_;
        my ($r, $ps, $U) = ($s->{db}, $s->{ps}, $s->{u});
        my $base = ($page - 1) * $ps;
        my $hdr  = $base + (($page == 1) ? 100 : 0);
        my $type = ord(substr($$r, $hdr, 1));
        my $n    = unpack('n', substr($$r, $hdr + 3, 2));
        die "SQLite:  page $page is not a table b-tree page (type $type)\n" unless $type == 5 || $type == 13;
        my $hsz  = ($type == 5) ? 12 : 8;
        for my $c (0 .. $n - 1)
        {
            my $cp = $base + unpack('n', substr($$r, $hdr + $hsz + 2 * $c, 2));
            if ($type == 5)
            {
                iSqliteScan($s, unpack('N', substr($$r, $cp, 4)), $cb);
                next;
            }
            my ($P, $o)     = iSqliteVarint($r, $cp);
            my ($rowid, $q) = iSqliteVarint($r, $o);
            my $X = $U - 35;
            my $payload;
            if ($P <= $X)
            {
                $payload = substr($$r, $q, $P);
            } else {
                # Overflow:  the local part, then the chain of overflow pages
                my $M     = int(($U - 12) * 32 / 255) - 23;
                my $K     = $M + (($P - $M) % ($U - 4));
                my $local = ($K <= $X) ? $K : $M;
                $payload  = substr($$r, $q, $local);
                my $ov    = unpack('N', substr($$r, $q + $local, 4));
                while ($ov && length($payload) < $P)
                {
                    my $ob   = ($ov - 1) * $ps;
                    my $take = $P - length($payload);
                    $take    = $U - 4 if $take > $U - 4;
                    $payload .= substr($$r, $ob + 4, $take);
                    $ov      = unpack('N', substr($$r, $ob, 4));
                }
            }
            $cb->($rowid, iSqliteRecord($payload));
        }
        iSqliteScan($s, unpack('N', substr($$r, $hdr + 8, 4)), $cb) if $type == 5;
    }

    # The tables:  name -> { root => page, col => { column name -> index } }
    sub iSqliteTables
    {
        my ($s) = @_;
        my %t;
        iSqliteScan($s, 1, sub
        {
            my ($rowid, $v) = @_;
            my ($type, $name, $tbl, $root, $sql) = @$v;
            return unless defined $type && $type eq 'table' && defined $sql;
            # The column definitions:  the top-level comma list inside CREATE TABLE ( ... )
            my $body = ($sql =~ /\((.*)\)\s*$/s) ? $1 : '';
            my ($depth, $cur, @defs) = (0, '');
            for my $ch (split //, $body)
            {
                $depth++ if $ch eq '(';
                $depth-- if $ch eq ')';
                if ($ch eq ',' && $depth == 0)	{ push @defs, $cur;  $cur = '';  next; }
                $cur .= $ch;
            }
            push @defs, $cur if length $cur;
            my (%col, $k);
            $k = 0;
            for my $d (@defs)
            {
                $d =~ s/^\s+//;
                next if $d =~ /^(?:CONSTRAINT|PRIMARY|UNIQUE|CHECK|FOREIGN)\b/i;
                my ($cn) = $d =~ /^["`\[]?([^"`\]\s]+)/;
                $col{$cn} = $k++ if defined $cn;
            }
            $t{$name} = { root => $root, col => \%col };
        });
        return %t;
    }

    sub iKm
    {
        my ($la1, $lo1, $la2, $lo2) = @_;
        my $r  = $PI / 180.0;
        my $sp = sin(($la2 - $la1) * $r / 2);
        my $sl = sin(($lo2 - $lo1) * $r / 2);
        my $h  = $sp * $sp + cos($la1 * $r) * cos($la2 * $r) * $sl * $sl;
        return 2 * 6371.0 * atan2(sqrt($h), sqrt(1 - $h));
    }
