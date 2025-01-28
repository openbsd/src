package Test2::Bundle;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Bundle - Documentation for bundles.

=head1 DESCRIPTION

Bundles are collections of Tools and Plugins. Bundles should not provide any
tools or behaviors of their own, they should simply combine the tools and
behaviors of other packages.

=head1 FAQ

=over 4

=item Should my bundle subclass Test2::Bundle?

No. Currently this class is empty. Eventually we may want to add behavior, in
which case we do not want anyone to already be subclassing it.

=back

=head1 HOW DO I WRITE A BUNDLE?

Writing a bundle can be very simple:

    package Test2::Bundle::MyBundle;
    use strict;
    use warnings;

    use Test2::Plugin::ExitSummary; # Load a plugin

    use Test2::Tools::Basic qw/ok plan done_testing/;

    # Re-export the tools
    our @EXPORTS = qw/ok plan done_testing/;
    use base 'Exporter';

    1;

If you want to do anything more complex you should look into L<Import::Into>
and L<Symbol::Move>.

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
