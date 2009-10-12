package XS::APItest;

use 5.008;
use strict;
use warnings;
use Carp;

use base qw/ DynaLoader Exporter /;

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# Export everything since these functions are only used by a test script
our @EXPORT = qw( print_double print_int print_long
		  print_float print_long_double have_long_double print_flush
		  mpushp mpushn mpushi mpushu
		  mxpushp mxpushn mxpushi mxpushu
		  call_sv call_pv call_method eval_sv eval_pv require_pv
		  G_SCALAR G_ARRAY G_VOID G_DISCARD G_EVAL G_NOARGS
		  G_KEEPERR G_NODEBUG G_METHOD G_WANT
		  apitest_exception mycroak strtab
		  my_cxt_getint my_cxt_getsv my_cxt_setint my_cxt_setsv
		  sv_setsv_cow_hashkey_core sv_setsv_cow_hashkey_notcore
		  rmagical_cast rmagical_flags
		  DPeek
);

our $VERSION = '0.15';

use vars '$WARNINGS_ON_BOOTSTRAP';
use vars map "\$${_}_called_PP", qw(BEGIN UNITCHECK CHECK INIT END);

BEGIN {
    # This is arguably a hack, but it disposes of the UNITCHECK block without
    # needing to preprocess the source code
    if ($] < 5.009) {
       eval 'sub UNITCHECK (&) {}; 1' or die $@;
    }
}

# Do these here to verify that XS code and Perl code get called at the same
# times
BEGIN {
    $BEGIN_called_PP++;
}
UNITCHECK {
    $UNITCHECK_called_PP++;
};
{
    # Need $W false by default, as some tests run under -w, and under -w we
    # can get warnings about "Too late to run CHECK" block (and INIT block)
    no warnings 'void';
    CHECK {
	$CHECK_called_PP++;
    }
    INIT {
	$INIT_called_PP++;
    }
}
END {
    $END_called_PP++;
}

if ($WARNINGS_ON_BOOTSTRAP) {
    bootstrap XS::APItest $VERSION;
} else {
    # More CHECK and INIT blocks that could warn:
    local $^W;
    bootstrap XS::APItest $VERSION;
}

1;
__END__

=head1 NAME

XS::APItest - Test the perl C API

=head1 SYNOPSIS

  use XS::APItest;
  print_double(4);

=head1 ABSTRACT

This module tests the perl C API. Currently tests that C<printf>
works correctly.

=head1 DESCRIPTION

This module can be used to check that the perl C API is behaving
correctly. This module provides test functions and an associated
test script that verifies the output.

This module is not meant to be installed.

=head2 EXPORT

Exports all the test functions:

=over 4

=item B<print_double>

Test that a double-precision floating point number is formatted
correctly by C<printf>.

  print_double( $val );

Output is sent to STDOUT.

=item B<print_long_double>

Test that a C<long double> is formatted correctly by
C<printf>. Takes no arguments - the test value is hard-wired
into the function (as "7").

  print_long_double();

Output is sent to STDOUT.

=item B<have_long_double>

Determine whether a C<long double> is supported by Perl.  This should
be used to determine whether to test C<print_long_double>.

  print_long_double() if have_long_double;

=item B<print_nv>

Test that an C<NV> is formatted correctly by
C<printf>.

  print_nv( $val );

Output is sent to STDOUT.

=item B<print_iv>

Test that an C<IV> is formatted correctly by
C<printf>.

  print_iv( $val );

Output is sent to STDOUT.

=item B<print_uv>

Test that an C<UV> is formatted correctly by
C<printf>.

  print_uv( $val );

Output is sent to STDOUT.

=item B<print_int>

Test that an C<int> is formatted correctly by
C<printf>.

  print_int( $val );

Output is sent to STDOUT.

=item B<print_long>

Test that an C<long> is formatted correctly by
C<printf>.

  print_long( $val );

Output is sent to STDOUT.

=item B<print_float>

Test that a single-precision floating point number is formatted
correctly by C<printf>.

  print_float( $val );

Output is sent to STDOUT.

=item B<call_sv>, B<call_pv>, B<call_method>

These exercise the C calls of the same names. Everything after the flags
arg is passed as the the args to the called function. They return whatever
the C function itself pushed onto the stack, plus the return value from
the function; for example

    call_sv( sub { @_, 'c' }, G_ARRAY,  'a', 'b'); # returns 'a', 'b', 'c', 3
    call_sv( sub { @_ },      G_SCALAR, 'a', 'b'); # returns 'b', 1

=item B<eval_sv>

Evaluates the passed SV. Result handling is done the same as for
C<call_sv()> etc.

=item B<eval_pv>

Exercises the C function of the same name in scalar context. Returns the
same SV that the C function returns.

=item B<require_pv>

Exercises the C function of the same name. Returns nothing.

=back

=head1 SEE ALSO

L<XS::Typemap>, L<perlapi>.

=head1 AUTHORS

Tim Jenness, E<lt>t.jenness@jach.hawaii.eduE<gt>,
Christian Soeller, E<lt>csoelle@mph.auckland.ac.nzE<gt>,
Hugo van der Sanden E<lt>hv@crypt.compulink.co.ukE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2002,2004 Tim Jenness, Christian Soeller, Hugo van der Sanden.
All Rights Reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
