package Test2::Plugin;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Plugin - Documentation for plugins

=head1 DESCRIPTION

Plugins are packages that cause behavior changes, or other side effects for the
test file that loads them. They should not export any functions, or provide any
tools. Plugins should be distinct units of functionality.

If you wish to combine behavior changes with tools then you should write a
Plugin, a Tools module, and a bundle that loads them both.

=head1 FAQ

=over 4

=item Should I subclass Test2::Plugin?

No. Currently this class is empty. Eventually we may want to add behavior, in
which case we do not want anyone to already be subclassing it.

=back

=head1 HOW DO I WRITE A PLUGIN?

Writing a plugin is not as simple as writing an L<Test2::Bundle>, or writing
L<Test2::Tools>. Plugins alter behavior, or cause desirable side-effects. To
accomplish this you typically need a custom C<import()> method that calls one
or more functions provided by the L<Test2::API> package.

If you want to write a plugin you should look at existing plugins, as well as
the L<Test2::API> and L<Test2::Hub> documentation. There is no formula for a
Plugin, they are generally unique, however consistent rules are that they
should not load other plugins, or export any functions.

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
