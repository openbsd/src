#!perl

# test the helper math routines in Math::BigFloat

use strict;
use warnings;

use Test::More tests => 26;

use Math::BigFloat lib => 'Calc';

#############################################################################
# add

{
    my $a = Math::BigInt::Calc->_new("123");
    my $b = Math::BigInt::Calc->_new("321");

    test_add(123, 321, '+', '+');
    test_add(123, 321, '+', '-');
    test_add(123, 321, '-', '+');

    test_add(321, 123, '-', '+');
    test_add(321, 123, '+', '-');

    test_add(10,  1, '+', '-');
    test_add(10,  1, '-', '+');
    test_add( 1, 10, '-', '+');

  SKIP: {
        skip q|$x -> _zero() does not (yet?) modify the first argument|, 2;

        test_add(123, 123, '-', '+');
        test_add(123, 123, '+', '-');
    }

    test_add(123, 123, '+', '+');
    test_add(123, 123, '-', '-');

    test_add(0, 0, '-', '+');
    test_add(0, 0, '+', '-');
    test_add(0, 0, '+', '+');
    test_add(0, 0, '-', '-');          # gives "-0"! TODO: fix this!
}

#############################################################################
# sub

{
    my $a = Math::BigInt::Calc->_new("123");
    my $b = Math::BigInt::Calc->_new("321");

    test_sub(123, 321, '+', '-');
    test_sub(123, 321, '-', '+');

    test_sub(123, 123, '-', '+');
    test_sub(123, 123, '+', '-');

  SKIP: {
        skip q|$x -> _zero() does not (yet?) modify the first argument|, 2;

        test_sub(123, 123, '+', '+');
        test_sub(123, 123, '-', '-');
    }

    test_sub(0, 0, '-', '+');          # gives "-0"! TODO: fix this!
    test_sub(0, 0, '+', '-');
    test_sub(0, 0, '+', '+');
    test_sub(0, 0, '-', '-');
}

###############################################################################

sub test_add {
    my ($a, $b, $as, $bs) = @_;

    my $aa = Math::BigInt::Calc -> _new($a);
    my $bb = Math::BigInt::Calc -> _new($b);
    my ($x, $xs) = Math::BigFloat::_e_add($aa, $bb, "$as", "$bs");
    my $got = $xs . Math::BigInt::Calc->_str($x);

    my $expected = sprintf("%+d", "$as$a" + "$bs$b");

    subtest qq|Math::BigFloat::_e_add($a, $b, "$as", "$bs");|
      => sub {
          plan tests => 2;

          is($got, $expected, 'output has the correct value');
          is(Math::BigInt::Calc->_str($x),
             Math::BigInt::Calc->_str($aa),
             'first operand to _e_add() is modified'
            );
      };
}

sub test_sub {
    my ($a, $b, $as, $bs) = @_;

    my $aa = Math::BigInt::Calc -> _new($a);
    my $bb = Math::BigInt::Calc -> _new($b);
    my ($x, $xs) = Math::BigFloat::_e_sub($aa, $bb, "$as", "$bs");
    my $got = $xs . Math::BigInt::Calc->_str($x);

    my $expected = sprintf("%+d", "$as$a" - "$bs$b");

    subtest qq|Math::BigFloat::_e_sub($a, $b, "$as", "$bs");|
      => sub {
          plan tests => 2;

          is($got, $expected, 'output has the correct value');
          is(Math::BigInt::Calc->_str($x),
             Math::BigInt::Calc->_str($aa),
             'first operand to _e_sub() is modified'
            );
      };
}
