package Test2::Env;
use strict;
use warnings;

our $VERSION = '1.302210';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Env - Documentation for environment variables used or set by Test2.

=head1 DESCRIPTION

This is a list of environment variables that are either set by, or read by Test2.

=head1 AUTHOR_TESTING

This env var is read by Test2. When set Test2 will run tests that are normally
skipped unless a module author is doing extra author-specific testing.

=head1 AUTOMATED_TESTING

This env var is read by Test2. When set this indicates the tests are run by an
automated system and no human interaction is possible.

See L<Test2::Require::AuthorTesting>.

=head1 EXTENDED_TESTING

This env var is read by Test2. When set it indicates some extended testing that
should normally be skipped will be run.

See L<Test2::Require::ExtendedTesting>.

=head1 HARNESS_ACTIVE

This env var is read by Test2. It is usually set by C<prove> (L<Test::Harness>)
or C<yath> (L<App::Yath>).

=head1 NONINTERACTIVE_TESTING

This env var is read by Test2. When set this indicates the testing will not be
interactive.

See L<Test2::Require::NonInteractiveTesting>.

=head1 RELEASE_TESTING

This env var is read by Test2. When set this indicates that release testing is
being done, which may run more tests than normal.

See L<Test2::Require::ReleaseTesting>.

=head1 T2_FORMATTER

This can be used to set the formatter that Test2 will use. If set to a string
without a '+' prefix, then 'Test2::Formatter::' will be added to the start of
the module name. If '+' is present it will be stripped and no further
modification will be made to the module name.

=head1 T2_IN_PRELOAD

Test2 sets this when preload mode is active. This is mainly used by
L<App::Yath> and similar tools that preload Test2, then fork to run tests.

=head1 TABLE_TERM_SIZE

This is used to set a terminal width for things like diagnostic message tables.

=head1 TEST2_ACTIVE

Test2 sets this variable when tests are running.

=head1 TEST2_ENABLE_PLUGINS

This can be used to force plugins to be loaded whent he Test2 API is loaded. It
takes a list of one or more plugin names seperated by comma. If the module name
does not have a '+' in front of it then the C<Test2::Plugin::> namespace is
assumed and added. If a '+' is present at the start of a module name it will be
stripped and no further modification will be made.

Examples:

    TEST2_ENABLE_PLUGINS=BailOnFail
    Test2_ENABLE_PLUGINS=SRand,+My::Plugin::Name

=head1 TEST_ACTIVE

Set by Test2 when tests are running.

=head1 TS_MAX_DELTA

Used to determine how many max lines of output will be provided when is() finds
a deep data strucgture mismatch.

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/test-more/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
