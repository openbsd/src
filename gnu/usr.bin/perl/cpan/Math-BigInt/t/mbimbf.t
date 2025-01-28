# -*- mode: perl; -*-

# test rounding, accuracy, precision and fallback, round_mode and mixing
# of classes

use strict;
use warnings;

use Test::More tests => 712             # tests in require'd file
                        + 52;           # tests in this file

use Math::BigInt only => 'Calc';
use Math::BigFloat;

our $mbi = 'Math::BigInt';
our $mbf = 'Math::BigFloat';

require './t/mbimbf.inc';

# some tests that won't work with subclasses, since the things are only
# guaranteed in the Math::Big(Int|Float) (unless subclass chooses to support
# this)

Math::BigInt->round_mode("even");       # reset for tests
Math::BigFloat->round_mode("even");     # reset for tests

is($Math::BigInt::rnd_mode,   "even", '$Math::BigInt::rnd_mode = "even"');
is($Math::BigFloat::rnd_mode, "even", '$Math::BigFloat::rnd_mode = "even"');

my $x = eval '$mbi->round_mode("huhmbi");';
like($@, qr/^Unknown round mode 'huhmbi' at/,
     '$mbi->round_mode("huhmbi")');

$x = eval '$mbf->round_mode("huhmbf");';
like($@, qr/^Unknown round mode 'huhmbf' at/,
     '$mbf->round_mode("huhmbf")');

# old way (now with test for validity)
$x = eval '$Math::BigInt::rnd_mode = "huhmbi";';
like($@, qr/^Unknown round mode 'huhmbi' at/,
     '$Math::BigInt::rnd_mode = "huhmbi"');
$x = eval '$Math::BigFloat::rnd_mode = "huhmbf";';
like($@, qr/^Unknown round mode 'huhmbf' at/,
     '$Math::BigFloat::rnd_mode = "huhmbf"');

# see if accessor also changes old variable
$mbi->round_mode('odd');
is($Math::BigInt::rnd_mode, 'odd', '$Math::BigInt::rnd_mode = "odd"');

$mbf->round_mode('odd');
is($Math::BigInt::rnd_mode, 'odd', '$Math::BigInt::rnd_mode = "odd"');

foreach my $class (qw/Math::BigInt Math::BigFloat/) {
    is($class->accuracy(5),  5,     "set A ...");
    is($class->precision(),  undef, "... and now P must be cleared");
    is($class->precision(5), 5,     "set P ...");
    is($class->accuracy(),   undef, "... and now A must be cleared");
}

foreach my $class (qw/Math::BigInt Math::BigFloat/)  {
    my $x;

    # Accuracy

    # set and check the class accuracy
    $class->accuracy(1);
    is($class->accuracy(), 1, "$class has A of 1");

    # a new instance gets the class accuracy
    $x = $class->new(123);
    is($x->accuracy(), 1, '$x has A of 1');

    # set and check the instance accuracy
    $x->accuracy(2);
    is($x->accuracy(), 2, '$x has A of 2');

    # change the class accuracy
    $class->accuracy(3);
    is($class->accuracy(), 3, "$class has A of 3");

    # verify that the instance accuracy hasn't changed
    is($x->accuracy(), 2, '$x still has A of 2');

    # change the instance accuracy
    $x->accuracy(undef);
    is($x->accuracy(), undef, '$x now has A of undef');

    # check the class accuracy
    is($class->accuracy(), 3, "$class still has A of 3");

    # change the class accuracy again
    $class->accuracy(undef);
    is($class->accuracy(), undef, "$class now has A of undef");

    # Precision

    # set and check the class precision
    $class->precision(1);
    is($class->precision(), 1, "$class has A of 1");

    # a new instance gets the class precision
    $x = $class->new(123);
    is($x->precision(), 1, '$x has A of 1');

    # set and check the instance precision
    $x->precision(2);
    is($x->precision(), 2, '$x has A of 2');

    # change the class precision
    $class->precision(3);
    is($class->precision(), 3, "$class has A of 3");

    # verify that the instance precision hasn't changed
    is($x->precision(), 2, '$x still has A of 2');

    # change the instance precision
    $x->precision(undef);
    is($x->precision(), undef, '$x now has A of undef');

    # check the class precision
    is($class->precision(), 3, "$class still has A of 3");

    # change the class precision again
    $class->precision(undef);
    is($class->precision(), undef, "$class now has A of undef");
}

# bug with blog(Math::BigFloat, Math::BigInt)
$x = Math::BigFloat->new(100);
$x = $x->blog(Math::BigInt->new(10));

is($x, 2, 'bug with blog(Math::BigFloat, Math::BigInt)');

# bug until v1.88 for sqrt() with enough digits
for my $i (80, 88, 100) {
    $x = Math::BigFloat->new("1." . ("0" x $i) . "1");
    $x = $x->bsqrt;
    is($x, 1, '$x->bsqrt() with many digits');
}
