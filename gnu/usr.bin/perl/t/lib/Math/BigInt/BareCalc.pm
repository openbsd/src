package Math::BigInt::BareCalc;

use 5.005;
use strict;
# use warnings;	# dont use warnings for older Perls

require Exporter;
use vars qw/@ISA $VERSION/;
@ISA = qw(Exporter);

$VERSION = '0.02';

# Package to to test Bigint's simulation of Calc

# uses Calc, but only features the strictly necc. methods.

use Math::BigInt::Calc '0.29';

BEGIN
  {
  no strict 'refs';
  foreach (qw/	base_len new zero one two copy str num add sub mul div inc dec
		acmp len digit zeros
		is_zero is_one is_odd is_even is_one check
		to_small to_large
		/)
    {
    my $name  = "Math::BigInt::Calc::_$_";
    *{"Math::BigInt::BareCalc::_$_"} = \&$name;
    }
  }

# catch and throw away
sub import { }

1;
