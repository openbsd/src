#!perl

# test calling conventions, and :constant overloading

use strict;
use warnings;
use lib 't';

my $VERSION = '1.999715';       # adjust manually to match latest release
$VERSION = eval $VERSION;

use Test::More tests => 161;

##############################################################################

package Math::BigInt::Test;

use Math::BigInt;
our @ISA = qw/Math::BigInt/;            # subclass of MBI
use overload;

##############################################################################

package Math::BigFloat::Test;

use Math::BigFloat;
our @ISA = qw/Math::BigFloat/;          # subclass of MBI
use overload;

##############################################################################

package main;

use Math::BigInt try => 'Calc';
use Math::BigFloat;

my ($x, $y, $z, $u);

###############################################################################
# check whether op's accept normal strings, even when inherited by subclasses

# do one positive and one negative test to avoid false positives by "accident"

my ($method, $expected);
while (<DATA>) {
    s/#.*$//;                   # remove comments
    s/\s+$//;                   # remove trailing whitespace
    next unless length;         # skip empty lines

    if (s/^&//) {
        $method = $_;
        next;
    }

    my @args = split /:/, $_, 99;
    $expected = pop @args;
    foreach my $class (qw/
                             Math::BigInt Math::BigFloat
                             Math::BigInt::Test Math::BigFloat::Test
                         /)
    {
        my $arg = $args[0] =~ /"/ || $args[0] eq "" ? $args[0]
                                                    : qq|"$args[0]"|;
        my $try = "$class\->$method($arg);";
        my $got = eval $try;
        is($got, $expected, $try);
    }
}

my $class = 'Math::BigInt';

my $try;

# test whether use Math::BigInt qw/VERSION/ works
$try = "use $class (" . ($VERSION . '1') .");";
$try .= ' $x = $class->new(123); $x = "$x";';
eval $try;
like($@, qr/ ^ Math::BigInt \s+ ( version \s+ )? \S+ \s+ required--this \s+
             is \s+ only \s+ version \s+ \S+ /x,
     $try);

# test whether fallback to calc works
$try = qq|use $class ($VERSION, "try", "foo, bar, ");|
     . qq| $class\->config()->{lib};|;
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

# all done

__END__
&is_zero
1:0
0:1
&is_one
1:1
0:0
&is_positive
1:1
-1:0
&is_negative
1:0
-1:1
&is_nan
abc:1
1:0
&is_inf
inf:1
0:0
&bstr
5:5
10:10
-10:-10
abc:NaN
"+inf":inf
"-inf":-inf
&bsstr
1:1e+0
0:0e+0
2:2e+0
200:2e+2
-5:-5e+0
-100:-1e+2
abc:NaN
"+inf":inf
&babs
-1:1
1:1
&bnot
-2:1
1:-2
&bzero
:0
&bnan
:NaN
abc:NaN
&bone
:1
"+":1
"-":-1
&binf
:inf
"+":inf
"-":-inf
