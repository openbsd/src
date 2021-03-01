#!perl

# test calling conventions, and :constant overloading

use strict;
use warnings;
use lib 't';

my $VERSION = '1.999818';       # adjust manually to match latest release

use Test::More tests => 5;

##############################################################################

package Math::BigInt::Test;

use Math::BigInt;
our @ISA = qw/Math::BigInt/;            # subclass of MBI
use overload;

##############################################################################

package Math::BigFloat::Test;

use Math::BigFloat;
our @ISA = qw/Math::BigFloat/;          # subclass of MBF
use overload;

##############################################################################

package main;

use Math::BigInt try => 'Calc';
use Math::BigFloat;

my ($x, $expected, $try);

my $class = 'Math::BigInt';

# test whether use Math::BigInt qw/VERSION/ works
$try = "use $class (" . ($VERSION . '1') .");";
$try .= ' $x = $class->new(123); $x = "$x";';
eval $try;
like($@, qr/ ^ Math::BigInt \s+ ( version \s+ )? \S+ \s+ required--this \s+
             is \s+ only \s+ version \s+ \S+ /x,
     $try);

# test whether fallback to calc works
$try = qq|use $class ($VERSION, "try", "foo, bar, ");|
     . qq| $class\->config('lib');|;
$expected = eval $try;
like($expected, qr/^Math::BigInt::(Fast)?Calc\z/, $try);

# test whether constant works or not, also test for qw($VERSION)
# bgcd() is present in subclass, too
$try = qq|use $class ($VERSION, "bgcd", ":constant");|
     . q| $x = 2**150; bgcd($x); $x = "$x";|;
$expected = eval $try;
is($expected, "1427247692705959881058285969449495136382746624", $try);

# test whether Math::BigInt::Scalar via use works (w/ dff. spellings of calc)
$try = qq|use $class ($VERSION, "lib", "Scalar");|
     . q| $x = 2**10; $x = "$x";|;
$expected = eval $try;
is($expected, "1024", $try);

$try = qq|use $class ($VERSION, "lib", "$class\::Scalar");|
     . q| $x = 2**10; $x = "$x";|;
$expected = eval $try;
is($expected, "1024", $try);
