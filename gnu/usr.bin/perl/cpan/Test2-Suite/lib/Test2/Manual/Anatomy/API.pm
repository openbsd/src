package Test2::Manual::Anatomy::API;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Anatomy::API - Internals documentation for the API.

=head1 DESCRIPTION

This document covers some of the internals of L<Test2::API>.

=head1 IMPLEMENTATION DETAILS

=head2 Test2::API

L<Test2::API> provides a functional interface to any test2 global state. This
API should be preserved regardless of internal details of how and where the
global state is stored.

This module itself does not store any state (with a few minor exceptions) but
instead relies on L<Test2::API::Instance> to store state. This module is really
intended to be the layer between the consumer and the implementation details.
Ideally the implementation details can change any way they like, and this
module can be updated to use the new details without breaking anything.

=head2 Test2::API::Instance

L<Test2::API::Instance> is where the global state is actually managed. This is
an implementation detail, and should not be relied upon. It is entirely
possible that L<Test2::API::Instance> could be removed completely, or changed
in incompatible ways. Really these details are free to change so long as
L<Test2::API> is not broken.

L<Test2::API::Instance> is fairly well documented, so no additionally
documentation is needed for this manual page.

=head1 SEE ALSO

L<Test2::Manual> - Primary index of the manual.

=head1 SOURCE

The source code repository for Test2-Manual can be found at
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
