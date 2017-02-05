#!perl
#
# Verify that objectify() is able to convert a "foreign" object into what we
# want, when what we want is Math::BigInt or subclass thereof.

use strict;
use warnings;

package main;

use Test::More tests => 10;

use Math::BigInt;

###############################################################################

for my $class ('Math::BigInt', 'Math::BigInt::Subclass') {

    # This object defines what we want.

    my $int = $class -> new(10);

    # Create various objects that should work with the object above after
    # objectify() has done its thing.

    my $int_percent1 = My::Percent::Int1 -> new(100);
    is($int * $int_percent1, 10,
       qq|\$class -> new(10);|
       . q| $int_percent1 = My::Percent::Int1 -> new(100);|
       . q| $int * $int_percent1|);

    my $int_percent2 = My::Percent::Int2 -> new(100);
    is($int * $int_percent2, 10,
       qq|\$class -> new(10);|
       . q| $int_percent2 = My::Percent::Int2 -> new(100);|
       . q| $int * $int_percent2|);

    my $int_percent3 = My::Percent::Int3 -> new(100);
    is($int * $int_percent3, 10,
       qq|\$class -> new(10);|
       . q| $int_percent3 = My::Percent::Int3 -> new(100);|
       . q| $int * $int_percent3|);

    my $int_percent4 = My::Percent::Int4 -> new(100);
    is($int * $int_percent4, 10,
       qq|\$class -> new(10);|
       . q| $int_percent4 = My::Percent::Int4 -> new(100);|
       . q| $int * $int_percent4|);

    my $int_percent5 = My::Percent::Int5 -> new(100);
    is($int * $int_percent5, 10,
       qq|\$class -> new(10);|
       . q| $int_percent5 = My::Percent::Int5 -> new(100);|
       . q| $int * $int_percent5|);
}

###############################################################################
# Class supports as_int(), which returns a Math::BigInt.

package My::Percent::Int1;

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_int {
    my $self = shift;
    return Math::BigInt -> new($$self / 100);
}

###############################################################################
# Class supports as_int(), which returns a scalar.

package My::Percent::Int2;

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_int {
    my $self = shift;
    return $$self / 100;
}

###############################################################################
# Class does not support as_int(), but supports as_number(), which returns a
# Math::BigInt.

package My::Percent::Int3;

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_number {
    my $self = shift;
    return Math::BigInt -> new($$self / 100);
}

###############################################################################
# Class does  not support as_int(),  but supports as_number(), which  returns a
# scalar.

package My::Percent::Int4;

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_number {
    my $self = shift;
    return $$self / 100;
}

###############################################################################
# Class supports neither as_int() or as_number().

package My::Percent::Int5;

use overload '""' => sub { $_[0] -> as_string(); };

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_string {
    my $self = shift;
    return $$self / 100;
}

###############################################################################

package Math::BigInt::Subclass;

use base 'Math::BigInt';
