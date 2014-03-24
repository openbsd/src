#!perl -w
use strict;

use Test::More;
use Config;
use XS::APItest;
use feature 'switch';
no warnings 'experimental::smartmatch';
use constant TRUTH => '0 but true';

# Tests for grok_number. Not yet comprehensive.
foreach my $leader ('', ' ', '  ') {
    foreach my $trailer ('', ' ', '  ') {
	foreach ((map {"0" x $_} 1 .. 12),
		 (map {("0" x $_) . "1"} 0 .. 12),
		 (map {"1" . ("0" x $_)} 1 .. 9),
		 (map {1 << $_} 0 .. 31),
		 (map {1 << $_} 0 .. 31),
		 (map {0xFFFFFFFF >> $_} reverse (0 .. 31)),
		) {
	    foreach my $sign ('', '-', '+') {
		my $string = $leader . $sign . $_ . $trailer;
		my ($flags, $value) = grok_number($string);
		is($flags & IS_NUMBER_IN_UV, IS_NUMBER_IN_UV,
		   "'$string' is a UV");
		is($flags & IS_NUMBER_NEG, $sign eq '-' ? IS_NUMBER_NEG : 0,
		   "'$string' sign");
		is($value, abs $string, "value is correct");
	    }
	}

	{
	    my (@UV, @NV);
	    given ($Config{ivsize}) {
		when (4) {
		    @UV = qw(429496729  4294967290 4294967294 4294967295);
		    @NV = qw(4294967296 4294967297 4294967300 4294967304);
		}
		when (8) {
		    @UV = qw(1844674407370955161  18446744073709551610
			     18446744073709551614 18446744073709551615);
		    @NV = qw(18446744073709551616 18446744073709551617
			     18446744073709551620 18446744073709551624);
		}
		default {
		    die "Unknown IV size $_";
		}
	    }
	    foreach (@UV) {
		my $string = $leader . $_ . $trailer;
		my ($flags, $value) = grok_number($string);
		is($flags & IS_NUMBER_IN_UV, IS_NUMBER_IN_UV,
		   "'$string' is a UV");
		is($value, abs $string, "value is correct");
	    }
	    foreach (@NV) {
		my $string = $leader . $_ . $trailer;
		my ($flags, $value) = grok_number($string);
		is($flags & IS_NUMBER_IN_UV, 0, "'$string' is an NV");
		is($value, undef, "value is correct");
	    }
	}

	my $string = $leader . TRUTH . $trailer;
	my ($flags, $value) = grok_number($string);

	if ($string eq TRUTH) {
	    is($flags & IS_NUMBER_IN_UV, IS_NUMBER_IN_UV, "'$string' is a UV");
	    is($value, 0);
	} else {
	    is($flags, 0, "'$string' is not a number");
	    is($value, undef);
	}
    }
}

done_testing();
