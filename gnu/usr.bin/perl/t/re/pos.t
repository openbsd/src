#!./perl

# Make sure pos / resetting pos on failed match works

use strict;
use warnings;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 8;

##  Early bailout of pp_match because matchlen > stringlen

# With a var
{
	my $str = "bird";

	$str =~ /i/g;

	is(pos($str),  2, 'pos correct');

	$str =~ /toolongtomatch/g;

	is(pos($str), undef, 'pos undef after failed match');
}

# With $_
{
	$_ = "bird";

	m/i/g;

	is(pos, 2, 'pos correct');

	m/toolongtomatch/g;

	is(pos, undef, 'pos undef after failed match');
}

## Early bail out of pp_match because ?? already matched

# With a var
{
	my $str = "bird";

	for (1..2) {
		if ($str =~ m?bird?g) {
			is(pos($str),  4, 'pos correct');
		} else {
			is(pos($str), undef, 'pos undef after failed match');
		}
	}
}

# With $_
{
	for (1..2) {
		if (m?\d?g) {
			is(pos,  1, 'pos correct');
		} else {
			is(pos, undef, 'pos undef after failed match');
		}
	}
}
