package Test2::Compare::Negatable;
use strict;
use warnings;

our $VERSION = '0.000162';

require overload;
require Test2::Util::HashBase;

sub import {
    my ($pkg, $file, $line) = caller;

    my $sub = eval <<"    EOT" or die $@;
package $pkg;
#line $line "$file"
sub { overload->import('!' => 'clone_negate', fallback => 1); Test2::Util::HashBase->import('negate')}
    EOT

    $sub->();

    no strict 'refs';
    *{"$pkg\::clone_negate"} = \&clone_negate;
    *{"$pkg\::toggle_negate"} = \&toggle_negate;
}

sub clone_negate {
    my $self = shift;
    my $clone = $self->clone;
    $clone->toggle_negate;
    return $clone;
}

sub toggle_negate {
    my $self = shift;
    $self->set_negate($self->negate ? 0 : 1);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Negatable - Poor mans 'role' for compare objects that can be negated.

=head1 DESCRIPTION

Using this package inside an L<Test2::Compare::Base> subclass will overload
C<!$obj> and import C<clone_negate()> and C<toggle_negate()>.

=head1 WHY?

Until perl 5.18 the 'fallback' parameter to L<overload> would not be inherited,
so we cannot use inheritance for the behavior we actually want. This module
works around the problem by emulating the C<use overload> call we want for each
consumer class.

=head1 ATTRIBUTES

=over 4

=item $bool = $obj->negate

=item $obj->set_negate($bool)

=item $attr = NEGATE()

The NEGATE attribute will be added via L<Test2::Util::HashBase>.

=back

=head1 METHODS

=over 4

=item $clone = $obj->clone_negate()

Create a shallow copy of the object, and call C<toggle_negate> on it.

=item $obj->toggle_negate()

Toggle the negate attribute. If the attribute was on it will now be off, if it
was off it will now be on.

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
