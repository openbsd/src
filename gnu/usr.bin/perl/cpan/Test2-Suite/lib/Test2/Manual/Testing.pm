package Test2::Manual::Testing;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Testing - Hub for documentation about writing tests with Test2.

=head1 DESCRIPTION

This document outlines all the tutorials and POD that cover writing tests. This
section does not cover any Test2 internals, nor does it cover how to write new
tools, for that see L<Test2::Manual::Tooling>.

=head1 NAMESPACE MAP

When writing tests there are a couple namespaces to focus on:

=over 4

=item Test2::Tools::*

This is where toolsets can be found. A toolset exports functions that help you
make assertions about your code. Toolsets will only export functions, they
should not ever have extra/global effects.

=item Test2::Plugins::*

This is where plugins live. Plugins should not export anything, but instead
will introduce or alter behaviors for Test2 in general. These behaviors may be
lexically scoped, or they may be global.

=item Test2::Bundle::*

Bundles combine toolsets and plugins together to reduce your boilerplate. First
time test writers are encouraged to start with the L<Test2::V0> bundle (which
is an exception to the namespace rule as it does not live under
C<Test2::Bundle::>). If you find yourself loading several plugins and toolsets
over and over again you could benefit from writing your own bundle.

=item Test2::Require::*

This namespace contains modules that will cause a test to skip if specific
conditions are not met. Use this if you have tests that only run on specific
perl versions, or require external libraries that may not always be available.

=back

=head1 LISTING DEPENDENCIES

When you use L<Test2>, specifically things included in L<Test2::Suite> you need
to list them in your modules test dependencies. It is important to note that
you should list the tools/plugins/bundles you need, you should not simply list
L<Test2::Suite> as your dependency. L<Test2::Suite> is a living distribution
intended to represent the "current" best practices. As tools, plugins, and
bundles evolve, old ones will become discouraged and potentially be moved from
L<Test2::Suite> into their own distributions.

One goal of L<Test2::Suite> is to avoid breaking backwards compatibility.
Another goal is to always improve by replacing bad designs with better ones.
When necessary L<Test2::Suite> will break old modules out into separate dists
and define new ones, typically with a new bundle. In short, if we feel the need
to break something we will do so by creating a new bundle, and discouraging the
old one, but we will not break the old one.

So for example, if you use L<Test2::V0>, and L<Dist::Zilla> you
should have this in your config:

    [Prereqs / TestRequires]
    Test2::V0 = 0.000060

You B<SHOULD NOT> do this:

    [Prereqs / TestRequires]
    Test2::Suite = 0.000060

Because L<Test2::V0> might not always be part of L<Test2::Suite>.

When writing new tests you should often check L<Test2::Suite> to see what the
current recommended bundle is.

=head3 Dist::Zilla

    [Prereqs / TestRequires]
    Test2::V0 = 0.000060

=head3 ExtUtils::MakeMaker

    my %WriteMakefileArgs = (
      ...,
      "TEST_REQUIRES" => {
        "Test2::V0" => "0.000060"
      },
      ...
    );

=head3 Module::Install

    test_requires 'Test2::V0' => '0.000060';

=head3 Module::Build

    my $build = Module::Build->new(
        ...,
        test_requires => {
            "Test2::V0" => "0.000060",
        },
        ...
    );

=head1 TUTORIALS

=head2 SIMPLE/INTRODUCTION TUTORIAL

L<Test2::Manual::Testing::Introduction> is an introduction to writing tests
using the L<Test2> tools.

=head2 MIGRATING FROM TEST::BUILDER and TEST::MORE

L<Test2::Manual::Testing::Migrating> Is a tutorial for converting old tests
that use L<Test::Builder> or L<Test::More> to the newer L<Test2> way of doing
things.

=head2 ADVANCED PLANNING

L<Test2::Manual::Testing::Planning> is a tutorial on the many ways to set a
plan.

=head2 TODO TESTS

L<Test2::Manual::Testing::Todo> is a tutorial for markings tests as TODO.

=head2 SUBTESTS

COMING SOON.

=head2 COMPARISONS

COMING SOON.

=head3 SIMPLE COMPARISONS

COMING SOON.

=head3 ADVANCED COMPARISONS

COMING SOON.

=head2 TESTING EXPORTERS

COMING SOON.

=head2 TESTING CLASSES

COMING SOON.

=head2 TRAPPING

COMING SOON.

=head3 TRAPPING EXCEPTIONS

COMING SOON.

=head3 TRAPPING WARNINGS

COMING SOON.

=head2 DEFERRED TESTING

COMING SOON.

=head2 MANAGING ENCODINGS

COMING SOON.

=head2 AUTO-ABORT ON FAILURE

COMING SOON.

=head2 CONTROLLING RANDOM BEHAVIOR

COMING SOON.

=head2 WRITING YOUR OWN BUNDLE

COMING SOON.

=head1 TOOLSET DOCUMENTATION

COMING SOON.

=head1 PLUGIN DOCUMENTATION

COMING SOON.

=head1 BUNDLE DOCUMENTATION

COMING SOON.

=head1 REQUIRE DOCUMENTATION

COMING SOON.

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
