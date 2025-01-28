# -*- mode: perl; -*-

# test subclassing Math::BigInt

package Math::BigInt::Subclass;

use strict;
use warnings;

use Math::BigInt;

our @ISA = qw(Math::BigInt);

our $VERSION = "0.08";

use overload;                   # inherit overload

# Global variables. The values can be specified explicitly or obtained from the
# superclass.

our $accuracy   = undef;        # or Math::BigInt::Subclass -> accuracy();
our $precision  = undef;        # or Math::BigInt::Subclass -> precision();
our $round_mode = "even";       # or Math::BigInt::Subclass -> round_mode();
our $div_scale  = 40;           # or Math::BigInt::Subclass -> div_scale();

BEGIN {
    *objectify = \&Math::BigInt::objectify;
}

# We override new().

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;

    my $self = $class -> SUPER::new(@_);
    $self->{'_custom'} = 1;     # attribute specific to this subclass
    bless $self, $class;
}

# We override import(). This is just for a sample for demonstration.

sub import {
    my $self  = shift;
    my $class = ref($self) || $self;

    my @a;                      # unrecognized arguments
    while (@_) {
        my $param = shift;

        # The parameter "this" takes an option.

        if ($param eq 'something') {
            $self -> {$param} = shift;
            next;
        }

        push @a, $_;
    }

    $self -> SUPER::import(@a);         # need it for subclasses
}

# Any other methods to override can go here:

# sub method {
#     ...
# }

1;
