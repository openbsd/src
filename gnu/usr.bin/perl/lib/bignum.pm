package bignum;
require 5.005;

$VERSION = '0.11';
use Exporter;
@ISA =       qw( Exporter );
@EXPORT_OK = qw( ); 

use strict;

############################################################################## 

# These are all alike, and thus faked by AUTOLOAD

my @faked = qw/round_mode accuracy precision div_scale/;
use vars qw/$VERSION $AUTOLOAD $_lite/;		# _lite for testsuite

sub AUTOLOAD
  {
  my $name = $AUTOLOAD;

  $name =~ s/.*:://;    # split package
  no strict 'refs';
  foreach my $n (@faked)
    {
    if ($n eq $name)
      {
      *{"bignum::$name"} = sub 
        {
        my $self = shift;
        no strict 'refs';
        if (defined $_[0])
          {
          Math::BigInt->$name($_[0]);
          Math::BigFloat->$name($_[0]);
          }
        return Math::BigInt->$name();
        };
      return &$name;
      }
    }
 
  # delayed load of Carp and avoid recursion
  require Carp;
  Carp::croak ("Can't call bignum\-\>$name, not a valid method");
  }

sub upgrade
  {
  my $self = shift;
  no strict 'refs';
#  if (defined $_[0])
#    {
#    $Math::BigInt::upgrade = $_[0];
#    $Math::BigFloat::upgrade = $_[0];
#    }
  return $Math::BigInt::upgrade;
  }

sub import 
  {
  my $self = shift;

  # some defaults
  my $lib = 'Calc';
  my $upgrade = 'Math::BigFloat';
  my $downgrade = 'Math::BigInt';

  my @import = ( ':constant' );				# drive it w/ constant
  my @a = @_; my $l = scalar @_; my $j = 0;
  my ($ver,$trace);					# version? trace?
  my ($a,$p);						# accuracy, precision
  for ( my $i = 0; $i < $l ; $i++,$j++ )
    {
    if ($_[$i] eq 'upgrade')
      {
      # this causes upgrading
      $upgrade = $_[$i+1];		# or undef to disable
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s; $i++;
      }
    elsif ($_[$i] eq 'downgrade')
      {
      # this causes downgrading
      $downgrade = $_[$i+1];		# or undef to disable
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s; $i++;
      }
    elsif ($_[$i] =~ /^(l|lib)$/)
      {
      # this causes a different low lib to take care...
      $lib = $_[$i+1] || '';
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s; $i++;
      }
    elsif ($_[$i] =~ /^(a|accuracy)$/)
      {
      $a = $_[$i+1];
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s; $i++;
      }
    elsif ($_[$i] =~ /^(p|precision)$/)
      {
      $p = $_[$i+1];
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s; $i++;
      }
    elsif ($_[$i] =~ /^(v|version)$/)
      {
      $ver = 1;
      splice @a, $j, 1; $j --;
      }
    elsif ($_[$i] =~ /^(t|trace)$/)
      {
      $trace = 1;
      splice @a, $j, 1; $j --;
      }
    else { die "unknown option $_[$i]"; }
    }
  my $class;
  $_lite = 0;					# using M::BI::L ?
  if ($trace)
    {
    require Math::BigInt::Trace; $class = 'Math::BigInt::Trace';
    $upgrade = 'Math::BigFloat::Trace';	
    }
  else
    {
    # see if we can find Math::BigInt::Lite
    if (!defined $a && !defined $p)		# rounding won't work to well
      {
      eval 'require Math::BigInt::Lite;';
      if ($@ eq '')
        {
        @import = ( );				# :constant in Lite, not MBI
        Math::BigInt::Lite->import( ':constant' );
        $_lite= 1;				# signal okay
        }
      }
    require Math::BigInt if $_lite == 0;	# not already loaded?
    $class = 'Math::BigInt';			# regardless of MBIL or not
    } 
  # Math::BigInt::Trace or plain Math::BigInt
  $class->import(@import, upgrade => $upgrade, lib => $lib);

  if ($trace)
    {
    require Math::BigFloat::Trace; $class = 'Math::BigFloat::Trace';
    $downgrade = 'Math::BigInt::Trace';	
    }
  else
    {
    require Math::BigFloat; $class = 'Math::BigFloat';
    }
  $class->import(':constant','downgrade',$downgrade);

  bignum->accuracy($a) if defined $a;
  bignum->precision($p) if defined $p;
  if ($ver)
    {
    print "bignum\t\t\t v$VERSION\n";
    print "Math::BigInt::Lite\t v$Math::BigInt::Lite::VERSION\n" if $_lite;
    print "Math::BigInt\t\t v$Math::BigInt::VERSION";
    my $config = Math::BigInt->config();
    print " lib => $config->{lib} v$config->{lib_version}\n";
    print "Math::BigFloat\t\t v$Math::BigFloat::VERSION\n";
    exit;
    }
  }

1;

__END__

=head1 NAME

bignum - Transparent BigNumber support for Perl

=head1 SYNOPSIS

  use bignum;

  $x = 2 + 4.5,"\n";			# BigFloat 6.5
  print 2 ** 512 * 0.1;			# really is what you think it is

=head1 DESCRIPTION

All operators (including basic math operations) are overloaded. Integer and
floating-point constants are created as proper BigInts or BigFloats,
respectively.

=head2 OPTIONS

bignum recognizes some options that can be passed while loading it via use.
The options can (currently) be either a single letter form, or the long form.
The following options exist:

=over 2

=item a or accuracy

This sets the accuracy for all math operations. The argument must be greater
than or equal to zero. See Math::BigInt's bround() function for details.

	perl -Mbignum=a,50 -le 'print sqrt(20)'

=item p or precision

This sets the precision for all math operations. The argument can be any
integer. Negative values mean a fixed number of digits after the dot, while
a positive value rounds to this digit left from the dot. 0 or 1 mean round to
integer. See Math::BigInt's bfround() function for details.

	perl -Mbignum=p,-50 -le 'print sqrt(20)'

=item t or trace

This enables a trace mode and is primarily for debugging bignum or
Math::BigInt/Math::BigFloat.

=item l or lib

Load a different math lib, see L<MATH LIBRARY>.

	perl -Mbignum=l,GMP -e 'print 2 ** 512'

Currently there is no way to specify more than one library on the command
line. This will be hopefully fixed soon ;)

=item v or version

This prints out the name and version of all modules used and then exits.

	perl -Mbignum=v -e ''

=head2 MATH LIBRARY

Math with the numbers is done (by default) by a module called
Math::BigInt::Calc. This is equivalent to saying:

	use bignum lib => 'Calc';

You can change this by using:

	use bignum lib => 'BitVect';

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use bignum lib => 'Foo,Math::BigInt::Bar';

Please see respective module documentation for further details.

=head2 INTERNAL FORMAT

The numbers are stored as objects, and their internals might change at anytime,
especially between math operations. The objects also might belong to different
classes, like Math::BigInt, or Math::BigFLoat. Mixing them together, even
with normal scalars is not extraordinary, but normal and expected.

You should not depend on the internal format, all accesses must go through
accessor methods. E.g. looking at $x->{sign} is not a bright idea since there
is no guaranty that the object in question has such a hashkey, nor is a hash
underneath at all.

=head2 SIGN

The sign is either '+', '-', 'NaN', '+inf' or '-inf' and stored seperately.
You can access it with the sign() method.

A sign of 'NaN' is used to represent the result when input arguments are not
numbers or as a result of 0/0. '+inf' and '-inf' represent plus respectively
minus infinity. You will get '+inf' when dividing a positive number by 0, and
'-inf' when dividing any negative number by 0.

=head2 METHODS

Since all numbers are now objects, you can use all functions that are part of
the BigInt or BigFloat API. It is wise to use only the bxxx() notation, and not
the fxxx() notation, though. This makes it possible that the underlying object
might morph into a different class than BigFloat.

=head1 MODULES USED

C<bignum> is just a thin wrapper around various modules of the Math::BigInt
family. Think of it as the head of the family, who runs the shop, and orders
the others to do the work.

The following modules are currently used by bignum:

	Math::BigInt::Lite	(for speed, and only if it is loadable)
	Math::BigInt
	Math::BigFloat

=head1 EXAMPLES

Some cool command line examples to impress the Python crowd ;)
 
	perl -Mbignum -le 'print sqrt(33)'
	perl -Mbignum -le 'print 2*255'
	perl -Mbignum -le 'print 4.5+2*255'
	perl -Mbignum -le 'print 3/7 + 5/7 + 8/3'
	perl -Mbignum -le 'print 123->is_odd()'
	perl -Mbignum -le 'print log(2)'
	perl -Mbignum -le 'print 2 ** 0.5'
	perl -Mbignum=a,65 -le 'print 2 ** 0.2'

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

Especially L<bigrat> as in C<perl -Mbigrat -le 'print 1/3+1/4'>.

L<Math::BigFloat>, L<Math::BigInt>, L<Math::BigRat> and L<Math::Big> as well
as L<Math::BigInt::BitVect>, L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

=head1 AUTHORS

(C) by Tels L<http://bloodgate.com/> in early 2002.

=cut
