#
# Trigonometric functions, mostly inherited from Math::Complex.
# -- Jarkko Hietaniemi, April 1997
# -- Raphael Manfredi, September 1996 (indirectly: because of Math::Complex)
#

require Exporter;
package Math::Trig;

use strict;

use Math::Complex qw(:trig);

use vars qw($VERSION $PACKAGE
	    @ISA
	    @EXPORT);

@ISA = qw(Exporter);

$VERSION = 1.00;

my @angcnv = qw(rad2deg rad2grad
	     deg2rad deg2grad
	     grad2rad grad2deg);

@EXPORT = (@{$Math::Complex::EXPORT_TAGS{'trig'}},
	   @angcnv);

use constant pi2 => 2 * pi;
use constant DR  => pi2/360;
use constant RD  => 360/pi2;
use constant DG  => 400/360;
use constant GD  => 360/400;
use constant RG  => 400/pi2;
use constant GR  => pi2/400;

#
# Truncating remainder.
#

sub remt ($$) {
    # Oh yes, POSIX::fmod() would be faster. Possibly. If it is available.
    $_[0] - $_[1] * int($_[0] / $_[1]);
}

#
# Angle conversions.
#

sub rad2deg ($)  { remt(RD * $_[0], 360) }

sub deg2rad ($)  { remt(DR * $_[0], pi2) }

sub grad2deg ($) { remt(GD * $_[0], 360) }

sub deg2grad ($) { remt(DG * $_[0], 400) }

sub rad2grad ($) { remt(RG * $_[0], 400) }

sub grad2rad ($) { remt(GR * $_[0], pi2) }

=head1 NAME

Math::Trig - trigonometric functions

=head1 SYNOPSIS

	use Math::Trig;
	
	$x = tan(0.9);
	$y = acos(3.7);
	$z = asin(2.4);
	
	$halfpi = pi/2;

	$rad = deg2rad(120);

=head1 DESCRIPTION

C<Math::Trig> defines many trigonometric functions not defined by the
core Perl which defines only the C<sin()> and C<cos()>.  The constant
B<pi> is also defined as are a few convenience functions for angle
conversions.

=head1 TRIGONOMETRIC FUNCTIONS

The tangent

	tan

The cofunctions of the sine, cosine, and tangent (cosec/csc and cotan/cot
are aliases)

	csc cosec sec cot cotan

The arcus (also known as the inverse) functions of the sine, cosine,
and tangent

	asin acos atan

The principal value of the arc tangent of y/x

	atan2(y, x)

The arcus cofunctions of the sine, cosine, and tangent (acosec/acsc
and acotan/acot are aliases)

	acsc acosec asec acot acotan

The hyperbolic sine, cosine, and tangent

	sinh cosh tanh

The cofunctions of the hyperbolic sine, cosine, and tangent (cosech/csch
and cotanh/coth are aliases)

	csch cosech sech coth cotanh

The arcus (also known as the inverse) functions of the hyperbolic
sine, cosine, and tangent

	asinh acosh atanh

The arcus cofunctions of the hyperbolic sine, cosine, and tangent
(acsch/acosech and acoth/acotanh are aliases)

	acsch acosech asech acoth acotanh

The trigonometric constant B<pi> is also defined.

	$pi2 = 2 * pi;

=head2 ERRORS DUE TO DIVISION BY ZERO

The following functions

	tan
	sec
	csc
	cot
	asec
	acsc
	tanh
	sech
	csch
	coth
	atanh
	asech
	acsch
	acoth

cannot be computed for all arguments because that would mean dividing
by zero or taking logarithm of zero. These situations cause fatal
runtime errors looking like this

	cot(0): Division by zero.
	(Because in the definition of cot(0), the divisor sin(0) is 0)
	Died at ...

or

	atanh(-1): Logarithm of zero.
	Died at...

For the C<csc>, C<cot>, C<asec>, C<acsc>, C<acot>, C<csch>, C<coth>,
C<asech>, C<acsch>, the argument cannot be C<0> (zero).  For the
C<atanh>, C<acoth>, the argument cannot be C<1> (one).  For the
C<atanh>, C<acoth>, the argument cannot be C<-1> (minus one).  For the
C<tan>, C<sec>, C<tanh>, C<sech>, the argument cannot be I<pi/2 + k *
pi>, where I<k> is any integer.

=head2 SIMPLE (REAL) ARGUMENTS, COMPLEX RESULTS

Please note that some of the trigonometric functions can break out
from the B<real axis> into the B<complex plane>. For example
C<asin(2)> has no definition for plain real numbers but it has
definition for complex numbers.

In Perl terms this means that supplying the usual Perl numbers (also
known as scalars, please see L<perldata>) as input for the
trigonometric functions might produce as output results that no more
are simple real numbers: instead they are complex numbers.

The C<Math::Trig> handles this by using the C<Math::Complex> package
which knows how to handle complex numbers, please see L<Math::Complex>
for more information. In practice you need not to worry about getting
complex numbers as results because the C<Math::Complex> takes care of
details like for example how to display complex numbers. For example:

	print asin(2), "\n";
    
should produce something like this (take or leave few last decimals):

	1.5707963267949-1.31695789692482i

That is, a complex number with the real part of approximately C<1.571>
and the imaginary part of approximately C<-1.317>.

=head1 ANGLE CONVERSIONS

(Plane, 2-dimensional) angles may be converted with the following functions.

	$radians  = deg2rad($degrees);
	$radians  = grad2rad($gradians);
	
	$degrees  = rad2deg($radians);
	$degrees  = grad2deg($gradians);
	
	$gradians = deg2grad($degrees);
	$gradians = rad2grad($radians);

The full circle is 2 I<pi> radians or I<360> degrees or I<400> gradians.

=head1 BUGS

Saying C<use Math::Trig;> exports many mathematical routines in the
caller environment and even overrides some (C<sin>, C<cos>).  This is
construed as a feature by the Authors, actually... ;-)

The code is not optimized for speed, especially because we use
C<Math::Complex> and thus go quite near complex numbers while doing
the computations even when the arguments are not. This, however,
cannot be completely avoided if we want things like C<asin(2)> to give
an answer instead of giving a fatal runtime error.

=head1 AUTHORS

Jarkko Hietaniemi <F<jhi@iki.fi>> and 
Raphael Manfredi <F<Raphael_Manfredi@grenoble.hp.com>>.

=cut

# eof
