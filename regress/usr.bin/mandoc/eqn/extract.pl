#!/usr/bin/perl
use warnings;
use strict;

my ($begun, $ended);

while (<>) {
	chomp;
	if (not $begun) {
		s/.*<math class="eqn">// or next;
		$begun = 1;
		next unless length;
	}
	s/<\/math>.*// and $ended = 1;
	s/^ *//;
	print "$_\n" if length;
	exit 0 if $ended;
}

die "unexpected end of file";
