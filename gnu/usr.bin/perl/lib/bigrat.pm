package bigrat;
require 5.005;

$VERSION = '0.04';
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
      *{"bigrat::$name"} = sub 
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
  Carp::croak ("Can't call bigrat\-\>$name, not a valid method");
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

  # see also bignum->import() for additional comments

  # some defaults
  my $lib = 'Calc'; my $upgrade = 'Math::BigFloat';

  my @import = ( ':constant' );				# drive it w/ constant
  my @a = @_; my $l = scalar @_; my $j = 0;
  my ($a,$p);
  my ($ver,$trace);					# version? trace?
  for ( my $i = 0; $i < $l ; $i++,$j++ )
    {
    if ($_[$i] eq 'upgrade')
      {
      # this causes upgrading
      $upgrade = $_[$i+1];		# or undef to disable
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s;
      }
    elsif ($_[$i] =~ /^(l|lib)$/)
      {
      # this causes a different low lib to take care...
      $lib = $_[$i+1] || '';
      my $s = 2; $s = 1 if @a-$j < 2;	# avoid "can not modify non-existant..."
      splice @a, $j, $s; $j -= $s;
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
    else
      {
      die ("unknown option $_[$i]");
      }
    }
  my $class;
  $_lite = 0;                                   # using M::BI::L ?
  if ($trace)
    {
    require Math::BigInt::Trace; $class = 'Math::BigInt::Trace';
    $upgrade = 'Math::BigFloat::Trace';
    }
  else
    {
    # see if we can find Math::BigInt::Lite
    if (!defined $a && !defined $p)             # rounding won't work to well
      {
      eval 'require Math::BigInt::Lite;';
      if ($@ eq '')
        {
        @import = ( );                          # :constant in Lite, not MBI
        Math::BigInt::Lite->import( ':constant' );
        $_lite= 1;                              # signal okay
        }
      }
    require Math::BigInt if $_lite == 0;        # not already loaded?
    $class = 'Math::BigInt';                    # regardless of MBIL or not
    }
  # Math::BigInt::Trace or plain Math::BigInt
  $class->import(@import, upgrade => $upgrade, lib => $lib);

  require Math::BigFloat;
  Math::BigFloat->import( upgrade => 'Math::BigRat', ':constant' );
  require Math::BigRat;
  if ($ver)
    {
    print "bigrat\t\t\t v$VERSION\n";
    print "Math::BigInt::Lite\t v$Math::BigInt::Lite::VERSION\n" if $_lite;  
    print "Math::BigInt\t\t v$Math::BigInt::VERSION";
    my $config = Math::BigInt->config();
    print " lib => $config->{lib} v$config->{lib_version}\n";
    print "Math::BigFloat\t\t v$Math::BigFloat::VERSION\n";
    print "Math::BigRat\t\t v$Math::BigRat::VERSION\n";
    exit;
    }
  }

1;

__END__

=head1 NAME

bigrat - Transparent BigNumber/BigRational support for Perl

=head1 SYNOPSIS

  use bigrat;

  $x = 2 + 4.5,"\n";			# BigFloat 6.5
  print 1/3 + 1/4,"\n";			# produces 7/12

=head1 DESCRIPTION

All operators (inlcuding basic math operations) are overloaded. Integer and
floating-point constants are created as proper BigInts or BigFloats,
respectively.

Other than L<bignum>, this module upgrades to Math::BigRat, meaning that
instead of 2.5 you will get 2+1/2 as output.

=head2 MODULES USED

C<bigrat> is just a thin wrapper around various modules of the Math::BigInt
family. Think of it as the head of the family, who runs the shop, and orders
the others to do the work.

The following modules are currently used by bignum:

        Math::BigInt::Lite      (for speed, and only if it is loadable)
        Math::BigInt
        Math::BigFloat
        Math::BigRat

=head2 MATH LIBRARY

Math with the numbers is done (by default) by a module called
Math::BigInt::Calc. This is equivalent to saying:

	use bigrat lib => 'Calc';

You can change this by using:

	use bigrat lib => 'BitVect';

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use bigrat lib => 'Foo,Math::BigInt::Bar';

Please see respective module documentation for further details.

=head2 SIGN

The sign is either '+', '-', 'NaN', '+inf' or '-inf' and stored seperately.

A sign of 'NaN' is used to represent the result when input arguments are not
numbers or as a result of 0/0. '+inf' and '-inf' represent plus respectively
minus infinity. You will get '+inf' when dividing a positive number by 0, and
'-inf' when dividing any negative number by 0.

=head2 METHODS

Since all numbers are not objects, you can use all functions that are part of
the BigInt or BigFloat API. It is wise to use only the bxxx() notation, and not
the fxxx() notation, though. This makes you independed on the fact that the
underlying object might morph into a different class than BigFloat.

=head1 EXAMPLES
 
	perl -Mbigrat -le 'print sqrt(33)'
	perl -Mbigrat -le 'print 2*255'
	perl -Mbigrat -le 'print 4.5+2*255'
	perl -Mbigrat -le 'print 3/7 + 5/7 + 8/3'	
	perl -Mbigrat -le 'print 12->is_odd()';

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

Especially L<bignum>.

L<Math::BigFloat>, L<Math::BigInt>, L<Math::BigRat> and L<Math::Big> as well
as L<Math::BigInt::BitVect>, L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

=head1 AUTHORS

(C) by Tels L<http://bloodgate.com/> in early 2002.

=cut
