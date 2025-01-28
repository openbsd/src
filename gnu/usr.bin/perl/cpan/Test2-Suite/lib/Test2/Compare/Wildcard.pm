package Test2::Compare::Wildcard;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/expect/;

use Carp qw/croak/;

sub init {
    my $self = shift;
    croak "'expect' is a require attribute"
        unless exists $self->{+EXPECT};

    $self->SUPER::init();
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Wildcard - Placeholder check.

=head1 DESCRIPTION

This module is used as a temporary placeholder for values that still need to be
converted. This is necessary to carry forward the filename and line number which
would be lost in the conversion otherwise.

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
