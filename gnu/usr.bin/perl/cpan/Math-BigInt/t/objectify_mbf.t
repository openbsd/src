#!perl
#
# Verify that objectify() is able to convert a "foreign" object into what we
# want, when what we want is Math::BigFloat or subclass thereof.

use strict;
use warnings;

package main;

use Test::More tests => 6;

use Math::BigFloat;

###############################################################################

for my $class ('Math::BigFloat', 'Math::BigFloat::Subclass') {

    # This object defines what we want.

    my $float = $class -> new(10);

    # Create various objects that should work with the object above after
    # objectify() has done its thing.

    my $float_percent1 = My::Percent::Float1 -> new(100);
    is($float * $float_percent1, 10,
       qq|\$float = $class -> new(10);|
       . q| $float_percent1 = My::Percent::Float1 -> new(100);|
       . q| $float * $float_percent1;|);

    my $float_percent2 = My::Percent::Float2 -> new(100);
    is($float * $float_percent2, 10,
       qq|\$float = $class -> new(10);|
       . q| $float_percent2 = My::Percent::Float2 -> new(100);|
       . q| $float * $float_percent2;|);

    my $float_percent3 = My::Percent::Float3 -> new(100);
    is($float * $float_percent3, 10,
       qq|\$float = $class -> new(10);|
       . q| $float_percent3 = My::Percent::Float3 -> new(100);|
       . q| $float * $float_percent3;|);
}

###############################################################################
# Class supports as_float(), which returns a Math::BigFloat.

package My::Percent::Float1;

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_float {
    my $self = shift;
    return Math::BigFloat -> new($$self / 100);
}

###############################################################################
# Class supports as_float(), which returns a scalar.

package My::Percent::Float2;

sub new {
    my $class = shift;
    my $num = shift;
    return bless \$num, $class;
}

sub as_float {
    my $self = shift;
    return $$self / 100;
}

###############################################################################
# Class does not support as_float().

package My::Percent::Float3;

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

package Math::BigFloat::Subclass;

use base 'Math::BigFloat';
