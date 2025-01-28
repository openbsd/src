package Test2::Suite;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Suite - Distribution with a rich set of tools built upon the Test2
framework.

=head1 DESCRIPTION

Rich set of tools, plugins, bundles, etc built upon the L<Test2> testing
library. If you are interested in writing tests, this is the distribution for
you.

=head2 WHAT ARE TOOLS, PLUGINS, AND BUNDLES?

=over 4

=item TOOLS

Tools are packages that export functions for use in test files. These functions
typically generate events. Tools B<SHOULD NEVER> alter behavior of other tools,
or the system in general.

=item PLUGINS

Plugins are packages that produce effects, or alter behavior of tools. An
example would be a plugin that causes the test to bail out after the first
failure. Plugins B<SHOULD NOT> export anything.

=item BUNDLES

Bundles are collections of tools and plugins. A bundle should load and
re-export functions from Tool packages. A bundle may also load and configure
any number of plugins.

=back

If you want to write something that both exports new functions, and effects
behavior, you should write both a Tools distribution, and a Plugin distribution,
then a Bundle that loads them both. This is important as it helps avoid the
problem where a package exports much-desired tools, but
also produces undesirable side effects.

=head1 INCLUDED BUNDLES

=over 4

=item Test2::V#

These do not live in the bundle namespace as they are the primary ways to use
Test2::Suite.

The current latest is L<Test2::V0>.

    use Test2::V0;
    # strict and warnings are on for you now.

    ok(...);

    # Note: is does deep checking, unlike the 'is' from Test::More.
    is(...);

    ...

    done_testing;

This bundle includes every tool listed in the L</INCLUDED TOOLS> section below,
except for L<Test2::Tools::ClassicCompare>. This bundle provides most of what
anyone writing tests could need. This is also the preferred bundle/toolset of
the L<Test2> author.

See L<Test2::V0> for complete documentation.

=item Extended

B<** Deprecated **> See L<Test2::V0>

    use Test2::Bundle::Extended;
    # strict and warnings are on for you now.

    ok(...);

    # Note: is does deep checking, unlike the 'is' from Test::More.
    is(...);

    ...

    done_testing;

This bundle includes every tool listed in the L</INCLUDED TOOLS> section below,
except for L<Test2::Tools::ClassicCompare>. This bundle provides most of what
anyone writing tests could need. This is also the preferred bundle/toolset of
the L<Test2> author.

See L<Test2::Bundle::Extended> for complete documentation.

=item More

    use Test2::Bundle::More;
    use strict;
    use warnings;

    plan 3; # Or you can use done_testing at the end

    ok(...);

    is(...); # Note: String compare

    is_deeply(...);

    ...

    done_testing; # Use instead of plan

This bundle is meant to be a I<mostly> drop-in replacement for L<Test::More>.
There are some notable differences to be aware of however. Some exports are
missing: C<eq_array>, C<eq_hash>, C<eq_set>, C<$TODO>, C<explain>, C<use_ok>,
C<require_ok>. As well it is no longer possible to set the plan at import:
C<< use .. tests => 5 >>. C<$TODO> has been replaced by the C<todo()>
function. Planning is done using C<plan>, C<skip_all>, or C<done_testing>.

See L<Test2::Bundle::More> for complete documentation.

=item Simple

    use Test2::Bundle::Simple;
    use strict;
    use warnings;

    plan 1;

    ok(...);

This bundle is meant to be a I<mostly> drop-in replacement for L<Test::Simple>.
See L<Test2::Bundle::Simple> for complete documentation.

=back

=head1 INCLUDED TOOLS

=over 4

=item Basic

Basic provides most of the essential tools previously found in L<Test::More>.
However it does not export any tools used for comparison. The basic C<pass>,
C<fail>, C<ok> functions are present, as are functions for planning.

See L<Test2::Tools::Basic> for complete documentation.

=item Compare

This provides C<is>, C<like>, C<isnt>, C<unlike>, and several additional
helpers. B<Note:> These are all I<deep> comparison tools and work like a
combination of L<Test::More>'s C<is> and C<is_deeply>.

See L<Test2::Tools::Compare> for complete documentation.

=item ClassicCompare

This provides L<Test::More> flavored C<is>, C<like>, C<isnt>, C<unlike>, and
C<is_deeply>. It also provides C<cmp_ok>.

See L<Test2::Tools::ClassicCompare> for complete documentation.

=item Class

This provides functions for testing objects and classes, things like C<isa_ok>.

See L<Test2::Tools::Class> for complete documentation.

=item Defer

This provides functions for writing test functions in one place, but running
them later. This is useful for testing things that run in an altered state.

See L<Test2::Tools::Defer> for complete documentation.

=item Encoding

This exports a single function that can be used to change the encoding of all
your test output.

See L<Test2::Tools::Encoding> for complete documentation.

=item Exports

This provides tools for verifying exports. You can verify that functions have
been imported, or that they have not been imported.

See L<Test2::Tools::Exports> for complete documentation.

=item Mock

This provides tools for mocking objects and classes. This is based largely on
L<Mock::Quick>, but several interface improvements have been added that cannot
be added to Mock::Quick itself without breaking backwards compatibility.

See L<Test2::Tools::Mock> for complete documentation.

=item Ref

This exports tools for validating and comparing references.

See L<Test2::Tools::Ref> for complete documentation.

=item Spec

This is an RSPEC implementation with concurrency support.

See L<Test2::Tools::Spec> for more details.

=item Subtest

This exports tools for running subtests.

See L<Test2::Tools::Subtest> for complete documentation.

=item Target

This lets you load the package(s) you intend to test, and alias them into
constants/package variables.

See L<Test2::Tools::Target> for complete documentation.

=back

=head1 INCLUDED PLUGINS

=over 4

=item BailOnFail

The much requested "bail-out on first failure" plugin. When this plugin is
loaded, any failure will cause the test to bail out immediately.

See L<Test2::Plugin::BailOnFail> for complete documentation.

=item DieOnFail

The much requested "die on first failure" plugin. When this plugin is
loaded, any failure will cause the test to die immediately.

See L<Test2::Plugin::DieOnFail> for complete documentation.

=item ExitSummary

This plugin gives you statistics and diagnostics at the end of your test in the
event of a failure.

See L<Test2::Plugin::ExitSummary> for complete documentation.

=item SRand

Use this to set the random seed to a specific seed, or to the current date.

See L<Test2::Plugin::SRand> for complete documentation.

=item UTF8

Turn on utf8 for your testing. This sets the current file to be utf8, it also
sets STDERR, STDOUT, and your formatter to all output utf8.

See L<Test2::Plugin::UTF8> for complete documentation.

=back

=head1 INCLUDED REQUIREMENT CHECKERS

=over 4

=item AuthorTesting

Using this package will cause the test file to be skipped unless the
AUTHOR_TESTING environment variable is set.

See L<Test2::Require::AuthorTesting> for complete documentation.

=item EnvVar

Using this package will cause the test file to be skipped unless a custom
environment variable is set.

See L<Test2::Require::EnvVar> for complete documentation.

=item Fork

Using this package will cause the test file to be skipped unless the system is
capable of forking (including emulated forking).

See L<Test2::Require::Fork> for complete documentation.

=item RealFork

Using this package will cause the test file to be skipped unless the system is
capable of true forking.

See L<Test2::Require::RealFork> for complete documentation.

=item Module

Using this package will cause the test file to be skipped unless the specified
module is installed (and optionally at a minimum version).

See L<Test2::Require::Module> for complete documentation.

=item Perl

Using this package will cause the test file to be skipped unless the specified
minimum perl version is met.

See L<Test2::Require::Perl> for complete documentation.

=item Threads

Using this package will cause the test file to be skipped unless the system has
threading enabled.

B<Note:> This will not turn threading on for you.

See L<Test2::Require::Threads> for complete documentation.

=back

=head1 SEE ALSO

See the L<Test2> documentation for a namespace map. Everything in this
distribution uses L<Test2>.

L<Test2::Manual> is the Test2 Manual.

=head1 CONTACTING US

Many Test2 developers and users lurk on L<irc://irc.perl.org/#perl>. We also
have a slack team that can be joined by anyone with an C<@cpan.org> email
address L<https://perl-test2.slack.com/> If you do not have an C<@cpan.org>
email you can ask for a slack invite by emailing Chad Granum
E<lt>exodist@cpan.orgE<gt>.

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
