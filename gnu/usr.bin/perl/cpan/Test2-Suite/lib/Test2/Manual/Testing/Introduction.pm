package Test2::Manual::Testing::Introduction;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Testing::Introduction - Introduction to testing with Test2.

=head1 DESCRIPTION

This tutorial is a beginners introduction to testing. This will take you
through writing a test file, making assertions, and running your test.

=head1 BOILERPLATE

=head2 THE TEST FILE

Test files typically are placed inside the C<t/> directory, and end with the
C<.t> file extension.

C<t/example.t>:

    use Test2::V0;

    # Assertions will go here

    done_testing;

This is all the boilerplate you need.

=over 4

=item use Test2::V0;

This loads a collection of testing tools that will be described later in the
tutorial. This will also turn on C<strict> and C<warnings> for you.

=item done_testing;

This should always be at the end of your test files. This tells L<Test2> that
you are done making assertions. This is important as C<test2> will assume the
test did not complete successfully without this, or some other form of test
"plan".

=back

=head2 DIST CONFIG

You should always list bundles and tools directly. You should not simply list
L<Test2::Suite> and call it done, bundles and tools may be moved out of
L<Test2::Suite> to their own dists at any time.

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

=head1 MAKING ASSERTIONS

The most simple tool for making assertions is C<ok()>. C<ok()> lets you assert
that a condition is true.

    ok($CONDITION, "Description of the condition");

Here is a complete C<t/example.t>:

    use Test2::V0;

    ok(1, "1 is true, so this will pass");

    done_testing;

=head1 RUNNING THE TEST

Test files are simply scripts. Just like any other script you can run the test
directly with perl. Another option is to use a test "harness" which runs the
test for you, and provides extra information and checks the scripts exit value
for you.

=head2 RUN DIRECTLY

    $ perl -Ilib t/example.t

Which should produce output like this:

    # Seeded srand with seed '20161028' from local date.
    ok 1 - 1 is true, so this will pass
    1..1

If the test had failed (C<ok(0, ...)>) it would look like this:

    # Seeded srand with seed '20161028' from local date.
    not ok 1 - 0 is false, so this will fail
    1..1

Test2 will also set the exit value of the script, a successful run will have an
exit value of 0, a failed run will have a non-zero exit value.

=head2 USING YATH

The C<yath> command line tool is provided by L<Test2::Harness> which you may
need to install yourself from cpan. C<yath> is the harness written specifically
for L<Test2>.

    $ yath -Ilib t/example.t

This will produce output similar to this:

    ( PASSED )  job  1    t/example.t

    ================================================================================

    Run ID: 1508027909

    All tests were successful!

You can also request verbose output with the C<-v> flag:

    $ yath -Ilib -v t/example.t

Which produces:

    ( LAUNCH )  job  1    example.t
    (  NOTE  )  job  1    Seeded srand with seed '20171014' from local date.
    [  PASS  ]  job  1  + 1 is true, so this will pass
    [  PLAN  ]  job  1    Expected asserions: 1
    ( PASSED )  job  1    example.t

    ================================================================================

    Run ID: 1508028002

    All tests were successful!

=head2 USING PROVE

The C<prove> command line tool is provided by the L<Test::Harness> module which
comes with most versions of perl. L<Test::Harness> is dual-life, which means
you can also install the latest version from cpan.

    $ prove -Ilib t/example.t

This will produce output like this:

    example.t .. ok
    All tests successful.
    Files=1, Tests=1,  0 wallclock secs ( 0.01 usr  0.00 sys +  0.05 cusr  0.00 csys =  0.06 CPU)
    Result: PASS

You can also request verbose output with the C<-v> flag:

    $ prove -Ilib -v t/example.t

The verbose output looks like this:

    example.t ..
    # Seeded srand with seed '20161028' from local date.
    ok 1 - 1 is true, so this will pass
    1..1
    ok
    All tests successful.
    Files=1, Tests=1,  0 wallclock secs ( 0.02 usr  0.00 sys +  0.06 cusr  0.00 csys =  0.08 CPU)
    Result: PASS

=head1 THE "PLAN"

All tests need a "plan". The job of a plan is to make sure you ran all the
tests you expected. The plan prevents a passing result from a test that exits
before all the tests are run.

There are 2 primary ways to set the plan:

=over 4

=item done_testing()

The most common, and recommended way to set a plan is to add C<done_testing> at
the end of your test file. This will automatically calculate the plan for you
at the end of the test. If the test were to exit early then C<done_testing>
would not run and no plan would be found, forcing a failure.

=item plan($COUNT)

The C<plan()> function allows you to specify an exact number of assertions you
want to run. If you run too many or too few assertions then the plan will not
match and it will be counted as a failure. The primary problem with this way of
planning is that you need to add up the number of assertions, and adjust the
count whenever you update the test file.

C<plan()> must be used before all assertions, or after all assertions, it
cannot be done in the middle of making assertions.

=back

=head1 ADDITIONAL ASSERTION TOOLS

The L<Test2::V0> bundle provides a lot more than C<ok()>,
C<plan()>, and C<done_testing()>. The biggest tools to note are:

=over 4

=item is($a, $b, $description)

C<is()> allows you to compare 2 structures and insure they are identical. You
can use it for simple string comparisons, or even deep data structure
comparisons.

    is("foo", "foo", "Both strings are identical");

    is(["foo", 1], ["foo", 1], "Both arrays contain the same elements");

=item like($a, $b, $description)

C<like()> is similar to C<is()> except that it only checks items listed on the
right, it ignores any extra values found on the left.

    like([1, 2, 3, 4], [1, 2, 3], "Passes, the extra element on the left is ignored");

You can also used regular expressions on the right hand side:

    like("foo bar baz", qr/bar/, "The string matches the regex, this passes");

You can also nest the regexes:

    like([1, 2, 'foo bar baz', 3], [1, 2, qr/bar/], "This passes");

=back

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
