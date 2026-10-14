#!/usr/bin/perl
##########
#
# make_places.pl -- build SolTime's place list from a GeoNames dump (spec §11).
#
# Usage:  make_places.pl <dir with cities15000.txt, admin1CodesASCII.txt, countryInfo.txt> <out dir>
# Source:  https://download.geonames.org/export/dump/  (CC BY 4.0 -- see ATTRIBUTION.md)
#
# Writes:
#   places.tsv     name, ascii name, country code, admin1 (state/province) ascii name, lat, lon, population,
#                  feature (the GeoNames feature code:  PPL a city or town, PPLA* a seat, PPLC a capital,
#                  PPLX a section of a city -- a neighborhood or district ...) -- sorted by country,
#                  then population (largest first)
#   countries.tsv  country code, ascii country name
#
######
use strict;
use warnings;

my ($src, $out) = @ARGV;
die "usage:  make_places.pl <geonames dir> <out dir>\n" unless defined $out;


##########
# Countries:  ISO code -> name
#####
    my %country;
    open(my $fc, '<:encoding(UTF-8)', "$src/countryInfo.txt") or die "countryInfo.txt:  $!\n";
    while (my $line = <$fc>)
    {
        next if $line =~ /^#/;
        chomp $line;
        my @f = split /\t/, $line, -1;
        next unless @f > 4 && length $f[0] == 2;
        $country{$f[0]} = $f[4];
    }
    close $fc;


##########
# Admin1:  "CC.code" -> ascii name
#####
    my %admin1;
    open(my $fa, '<:encoding(UTF-8)', "$src/admin1CodesASCII.txt") or die "admin1CodesASCII.txt:  $!\n";
    while (my $line = <$fa>)
    {
        chomp $line;
        my @f = split /\t/, $line, -1;
        next unless @f >= 3;
        $admin1{$f[0]} = $f[2];
    }
    close $fa;


##########
# Places
#####
    my @rows;
    open(my $fp, '<:encoding(UTF-8)', "$src/cities15000.txt") or die "cities15000.txt:  $!\n";
    while (my $line = <$fp>)
    {
        chomp $line;
        my @f = split /\t/, $line, -1;
        next unless @f >= 15;
        my ($name, $ascii, $lat, $lon, $feature, $cc, $a1, $pop) = @f[1, 2, 4, 5, 7, 8, 10, 14];
        next unless length $cc;
        $ascii = $name if !length $ascii;
        my $a1name = $admin1{"$cc.$a1"} // '';
        for ($name, $ascii, $a1name) { s/[\t\r\n]/ /g; }
        push @rows, [ $name, $ascii, $cc, $a1name, $lat + 0, $lon + 0, $pop + 0, $feature ];
    }
    close $fp;

    @rows = sort { $a->[2] cmp $b->[2] or $b->[6] <=> $a->[6] or $a->[1] cmp $b->[1] } @rows;

    open(my $op, '>:encoding(UTF-8)', "$out/places.tsv") or die "places.tsv:  $!\n";
    print $op "# SolTime place list -- GeoNames cities15000 (CC BY 4.0, geonames.org); see ATTRIBUTION.md\n";
    print $op "# name\tascii\tcountry\tadmin1\tlat\tlon\tpopulation\tfeature\n";
    printf $op "%s\t%s\t%s\t%s\t%.5f\t%.5f\t%d\t%s\n", @$_ for @rows;
    close $op;

    open(my $oc, '>:encoding(UTF-8)', "$out/countries.tsv") or die "countries.tsv:  $!\n";
    print $oc "# SolTime country list -- GeoNames countryInfo (CC BY 4.0, geonames.org); see ATTRIBUTION.md\n";
    print $oc "# code\tname\n";
    print $oc "$_\t$country{$_}\n" for sort keys %country;
    close $oc;

    printf "places %d, countries %d\n", scalar @rows, scalar keys %country;
