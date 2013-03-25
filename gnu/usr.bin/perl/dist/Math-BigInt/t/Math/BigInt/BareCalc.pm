package Math::BigInt::BareCalc;

use 5.005;
use strict;
# use warnings;	# dont use warnings for older Perls

require Exporter;
use vars qw/@ISA $VERSION/;
@ISA = qw(Exporter);

$VERSION = '0.05';

sub api_version () { 1; }

# Package to to test Bigint's simulation of Calc

# uses Calc, but only features the strictly necc. methods.

use Math::BigInt::Calc '0.51';

BEGIN
  {
  no strict 'refs';
  foreach (qw/	
	base_len new zero one two ten copy str num add sub mul div mod inc dec
	acmp alen len digit zeros
	rsft lsft
	fac pow gcd log_int sqrt root
	is_zero is_one is_odd is_even is_one is_two is_ten check
	as_hex as_bin as_oct from_hex from_bin from_oct
	modpow modinv
	and xor or
	/)
    {
    my $name  = "Math::BigInt::Calc::_$_";
    *{"Math::BigInt::BareCalc::_$_"} = \&$name;
    }
  print "# BareCalc using Calc v$Math::BigInt::Calc::VERSION\n";
  }

# catch and throw away
sub import { }

1;
