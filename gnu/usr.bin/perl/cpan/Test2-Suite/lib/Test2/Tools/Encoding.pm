package Test2::Tools::Encoding;
use strict;
use warnings;

use Carp qw/croak/;

use Test2::API qw/test2_stack/;

use base 'Exporter';

our $VERSION = '0.000162';

our @EXPORT = qw/set_encoding/;

sub set_encoding {
    my $enc = shift;
    my $format = test2_stack->top->format;

    unless ($format && eval { $format->can('encoding') }) {
        $format = '<undef>' unless defined $format;
        croak "Unable to set encoding on formatter '$format'";
    }

    $format->encoding($enc);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Encoding - Tools for managing the encoding of L<Test2> based
tests.

=head1 DESCRIPTION

This module exports a function that lets you dynamically change the output
encoding at will.

=head1 SYNOPSIS

    use Test2::Tools::Encoding;

    set_encoding('utf8');

=head1 EXPORTS

All subs are exported by default.

=over 4

=item set_encoding($encoding)

This will set the encoding to whatever you specify. This will only affect the
output of the current formatter, which is usually your TAP output formatter.

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
