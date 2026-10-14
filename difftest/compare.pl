#!/usr/bin/perl
##########
#
# compare.pl -- hold the C and Java outputs of the differential test together.
#
# Usage:  compare.pl c.txt java.txt
# Every line is  TAG index values...;  integers must match exactly, reals within the tag's
# tolerance (absolute, seconds or km).  Prints a per-tag summary; exit 0 = they agree.
#
######
use strict;
use warnings;

my ($fc, $fj) = @ARGV;
die "usage:  compare.pl c.txt java.txt\n" unless defined $fj;

# Tolerances (absolute).  Root-found values (radii, ramps, jumps) stop at a 0.1 m bracket;
# clock runs make the same decisions only while both sides land on the same side of every
# comparison, so they get a little room.
my %tol = (
    CALENDAR => 2e-6, UNIXLAYER => 2e-6, UNIXNEWYEAR => 1e-3, UNIXDATELINE => 1e-3,
    FIELD    => 1e-6,   SUN     => 1e-6,   LAYERED => 1e-6,
    RADIUS   => 1e-3,   ZONE    => 1e-3,   ZONESAT => 1e-6,
    CZONE    => 1e-3,   CLAYERED => 1e-6,  CZONESAT => 1e-6,
    STILL    => 1e-3,   DRIVE   => 1e-3,   JET     => 1e-3,   JUMP => 1e-3,   FOLLOW => 1e-3,
);

open(my $hc, '<', $fc) or die "$fc:  $!\n";
open(my $hj, '<', $fj) or die "$fj:  $!\n";
my @c = <$hc>;
my @j = <$hj>;
close $hc;
close $hj;

my (%lines, %worst, %bad, %first);
my $fail = 0;
if (@c != @j)
{
    printf "LINE COUNT DIFFERS:  C %d, Java %d\n", scalar @c, scalar @j;
    $fail = 1;
}

my $n = (@c < @j) ? scalar @c : scalar @j;
for my $i (0 .. $n - 1)
{
    my @a = split ' ', $c[$i];
    my @b = split ' ', $j[$i];
    my $tag = $a[0];
    $lines{$tag}++;
    if (@a != @b || $a[0] ne $b[0] || $a[1] ne $b[1])
    {
        $bad{$tag}++;
        $first{$tag} //= "line " . ($i + 1) . ":  C [$c[$i]]  Java [$j[$i]]";
        next;
    }
    for my $k (2 .. $#a)
    {
        my ($x, $y) = ($a[$k], $b[$k]);
        my $xn = ($x =~ /nan/i), my $yn = ($y =~ /nan/i);
        if ($xn || $yn)
        {
            next if $xn && $yn;
            $bad{$tag}++;
            $first{$tag} //= "line " . ($i + 1) . ":  C [$x]  Java [$y]";
            next;
        }
        my $isInt = ($x =~ /^-?\d+$/ && $y =~ /^-?\d+$/);
        my $d     = abs($x - $y);
        $worst{$tag} = $d if !defined $worst{$tag} || $d > $worst{$tag};
        if (($isInt && $x != $y) || (!$isInt && $d > ($tol{$tag} // 1e-6)))
        {
            $bad{$tag}++;
            $first{$tag} //= "line " . ($i + 1) . ":  C [$x]  Java [$y]  (diff $d)";
        }
    }
}

for my $tag (sort keys %lines)
{
    printf "%-9s %6d lines   worst |C - Java| %-10.3g   %s\n", $tag, $lines{$tag}, $worst{$tag} // 0,
        $bad{$tag} ? "$bad{$tag} OVER TOLERANCE ($tol{$tag})" : "ok";
    print "          $first{$tag}\n" if $bad{$tag};
    $fail = 1 if $bad{$tag};
}
print $fail ? "DIFFERENTIAL:  C and Java DISAGREE\n" : "DIFFERENTIAL:  C and Java agree\n";
exit $fail;
