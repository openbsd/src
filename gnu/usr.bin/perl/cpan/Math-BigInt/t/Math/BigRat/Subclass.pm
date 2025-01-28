# -*- mode: perl; -*-

# test subclassing Math::BigRat

package Math::BigRat::Subclass;

use strict;
use warnings;

use Math::BigRat;

our @ISA = qw(Math::BigRat);

our $VERSION = '0.04';

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

# We override new()

sub new {
    my $proto  = shift;
    my $class  = ref($proto) || $proto;

    my $self = $class -> SUPER::new(@_);
    $self->{'_custom'} = 1;     # attribute specific to this subclass
    bless $self, $class;
}

# Any other methods to override can go here:

# sub method {
#     ...
# }

1;
