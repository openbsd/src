package Test2::Bundle::Simple;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::Plugin::ExitSummary;

use Test2::Tools::Basic qw/ok plan done_testing skip_all/;

our @EXPORT = qw/ok plan done_testing skip_all/;
use base 'Exporter';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Bundle::Simple - ALMOST a drop-in replacement for Test::Simple.

=head1 DESCRIPTION

This bundle is intended to be a (mostly) drop-in replacement for
L<Test::Simple>. See L<"KEY DIFFERENCES FROM Test::Simple"> for details.

=head1 SYNOPSIS

    use Test2::Bundle::Simple;

    ok(1, "pass");

    done_testing;

=head1 PLUGINS

This loads L<Test2::Plugin::ExitSummary>.

=head1 TOOLS

These are all from L<Test2::Tools::Basic>.

=over 4

=item ok($bool, $name)

Run a test. If bool is true, the test passes. If bool is false, it fails.

=item plan($count)

Tell the system how many tests to expect.

=item skip_all($reason)

Tell the system to skip all the tests (this will exit the script).

=item done_testing();

Tell the system that all tests are complete. You can use this instead of
setting a plan.

=back

=head1 KEY DIFFERENCES FROM Test::Simple

=over 4

=item You cannot plan at import.

THIS WILL B<NOT> WORK:

    use Test2::Bundle::Simple tests => 5;

Instead you must plan in a separate statement:

    use Test2::Bundle::Simple;
    plan 5;

=item You have three subs imported for use in planning

Use C<plan($count)>, C<skip_all($reason)>, or C<done_testing()> for your
planning.

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
