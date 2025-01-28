package Test2::Manual::Anatomy::Utilities;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Anatomy::Utilities - Overview of utilities for Test2.

=head1 DESCRIPTION

This is a brief overview of the utilities provided by Test2.

=head1 Test2::Util

L<Test2::Util> provides functions to help you find out about the current
system, or to run generic tasks that tend to be Test2 specific.

This utility provides things like an internal C<try {...}> implementation, and
constants for things like threading and forking support.

=head1 Test2::Util::ExternalMeta

L<Test2::Util::ExternalMeta> allows you to quickly and easily attach meta-data
to an object class.

=head1 Test2::Util::Facets2Legacy

L<Test2::Util::Facets2Legacy> is a set of functions you can import into a more
recent event class to provide the classic event API.

=head1 Test2::Util::HashBase

L<Test2::Util::HashBase> is a local copy of L<Object::HashBase>. All object
classes provided by L<Test2> use this to generate methods and accessors.

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
