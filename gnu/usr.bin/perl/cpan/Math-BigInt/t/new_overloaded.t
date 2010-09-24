#!/usr/bin/perl -w

# Math::BigFloat->new had a bug where it would assume any object is a
# BigInt which broke overloaded non-BigInts.

use Test::More tests => 4;


package Overloaded::Num;

use overload '0+' => sub { ${$_[0]} },
             fallback => 1;
sub new {
    my($class, $num) = @_;
    return bless \$num, $class;
}


package main;

use Math::BigFloat;

my $overloaded_num = Overloaded::Num->new(2.23);
is $overloaded_num, 2.23;

my $bigfloat = Math::BigFloat->new($overloaded_num);
is $bigfloat, 2.23, 'BigFloat->new accepts overloaded numbers';

my $bigint = Math::BigInt->new(Overloaded::Num->new(3));
is $bigint, 3, 'BigInt->new accepts overloaded numbers';

is( Math::BigFloat->new($bigint), 3, 'BigFloat from BigInt' );
