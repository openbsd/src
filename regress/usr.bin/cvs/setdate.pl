#!/usr/bin/perl
# $OpenBSD: setdate.pl,v 1.1 2008/03/10 20:03:21 tobias Exp $
#
# Sets "date x;" of specified revision in rcsfile to date.
# This script is needed to make -D checks available.  CVS adjusts dates
# to gmt, so it is impossible to write tests which can be used in
# all timezones.
#
# usage: setdate.pl rcsfile revision date

my $gotrev;
my @lines;

die "usage: setdate.pl file revision date\n" if ($#ARGV != 2);

open FILE, "< $ARGV[0]" or die "cannot open file $ARGV[0] for reading\n";
@lines = <FILE>;
close FILE;

$gotrev = 0;
open FILE, "> $ARGV[0]" or die "cannot open file $ARGV[0] for writing\n";
for (@lines) {
	if ($gotrev) {
		if (m/^date\s+(.*?);/) {
			s/$1/$ARGV[2]/;
			break;
		}
		$gotrev = 0;
	}
	$gotrev = 1 if (m/^$ARGV[1]\n$/);
	print FILE "$_";
}
close FILE;

