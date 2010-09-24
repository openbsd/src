=head1 NAME

XS::APItest::KeywordRPN - write arithmetic expressions in RPN

=head1 SYNOPSIS

	use XS::APItest::KeywordRPN qw(rpn calcrpn);

	$triangle = rpn($n $n 1 + * 2 /);

	calcrpn $triangle { $n $n 1 + * 2 / }

=head1 DESCRIPTION

This module supplies plugged-in keywords, using the new mechanism in Perl
5.11.2, that allow arithmetic to be expressed in reverse Polish notation,
in an otherwise Perl program.  This module has serious limitations and
is not intended for real use: its purpose is only to test the keyword
plugin mechanism.  For that purpose it is part of the Perl core source
distribution, and is not meant to be installed.

=head2 RPN expression syntax

Tokens of an RPN expression may be separated by whitespace, but such
separation is usually not required.  It is required only where unseparated
tokens would look like a longer token.  For example, C<12 34 +> can be
written as C<12 34+>, but not as C<1234 +>.

An RPN expression may be any of:

=over

=item C<1234>

A sequence of digits is an unsigned decimal literal number.

=item C<$foo>

An alphanumeric name preceded by dollar sign refers to a Perl scalar
variable.  Only variables declared with C<my> or C<state> are supported.
If the variable's value is not a native integer, it will be converted
to an integer, by Perl's usual mechanisms, at the time it is evaluated.

=item I<A> I<B> C<+>

Sum of I<A> and I<B>.

=item I<A> I<B> C<->

Difference of I<A> and I<B>, the result of subtracting I<B> from I<A>.

=item I<A> I<B> C<*>

Product of I<A> and I<B>.

=item I<A> I<B> C</>

Quotient when I<A> is divided by I<B>, rounded towards zero.
Division by zero generates an exception.

=item I<A> I<B> C<%>

Remainder when I<A> is divided by I<B> with the quotient rounded towards zero.
Division by zero generates an exception.

=back

Because the arithmetic operators all have fixed arity and are postfixed,
there is no need for operator precedence, nor for a grouping operator
to override precedence.  This is half of the point of RPN.

An RPN expression can also be interpreted in another way, as a sequence
of operations on a stack, one operation per token.  A literal or variable
token pushes a value onto the stack.  A binary operator pulls two items
off the stack, performs a calculation with them, and pushes the result
back onto the stack.  The stack starts out empty, and at the end of the
expression there must be exactly one value left on the stack.

=cut

package XS::APItest::KeywordRPN;

{ use 5.011001; }
use warnings;
use strict;

our $VERSION = "0.003";

require XSLoader;
XSLoader::load(__PACKAGE__, $VERSION);

=head1 OPERATORS

These are the operators being added to the Perl language.

=over

=item rpn(EXPRESSION)

This construct is a Perl expression.  I<EXPRESSION> must be an RPN
arithmetic expression, as described above.  The RPN expression is
evaluated, and its value is returned as the value of the Perl expression.

=item calcrpn VARIABLE { EXPRESSION }

This construct is a complete Perl statement.  (No semicolon should
follow the closing brace.)  I<VARIABLE> must be a Perl scalar C<my>
variable, and I<EXPRESSION> must be an RPN arithmetic expression as
described above.  The RPN expression is evaluated, and its value is
assigned to the variable.

=back

=head1 BUGS

This module only performs arithmetic on native integers, and only a
small subset of the arithmetic operations that Perl offers.  This is
due to it being intended only for demonstration and test purposes.

The RPN parser is liable to leak memory when a parse error occurs.
It doesn't leak on success, however.

=head1 SEE ALSO

L<Devel::Declare>,
L<perlapi/PL_keyword_plugin>

=head1 AUTHOR

Andrew Main (Zefram) <zefram@fysh.org>

=head1 COPYRIGHT

Copyright (C) 2009 Andrew Main (Zefram) <zefram@fysh.org>

=head1 LICENSE

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

1;
