#!perl -w

# test the MULTICALL macros
# Note: as of Oct 2010, there are not yet comprehensive tests
# for these macros.

use warnings;
use strict;

use Test::More tests => 7;
use XS::APItest;


{
    my $sum = 0;
    sub add { $sum += $_++ }

    my @a = (1..3);
    XS::APItest::multicall_each \&add, @a;
    is($sum, 6, "sum okay");
    is($a[0], 2, "a[0] okay");
    is($a[1], 3, "a[1] okay");
    is($a[2], 4, "a[2] okay");
}

# [perl #78070]
# multicall using a sub that aleady has CvDEPTH > 1 caused sub
# to be prematurely freed

{
    my $destroyed = 0;
    sub REC::DESTROY { $destroyed = 1 }

    my $closure_var;
    {
	my $f = sub {
	    no warnings 'void';
	    $closure_var;
	    my $sub = shift;
	    if (defined $sub) {
		XS::APItest::multicall_each \&$sub, 1,2,3;
	    }
	};
	bless $f,  'REC';
	$f->($f);
	is($destroyed, 0, "f not yet destroyed");
    }
    is($destroyed, 1, "f now destroyed");

}

# [perl #115602]
# deep recursion realloced the CX stack, but the dMULTICALL local var
# 'cx' still pointed to the old one.
# Thius doesn;t actually test the failure (I couldn't think of a way to
# get the failure to show at the perl level) but it allows valgribnd or
# similar to spot any errors.

{
    sub rec { my $c = shift; rec($c-1) if $c > 0 };
    my @r = XS::APItest::multicall_each { rec(90) } 1,2,3;
    pass("recursion");
}
