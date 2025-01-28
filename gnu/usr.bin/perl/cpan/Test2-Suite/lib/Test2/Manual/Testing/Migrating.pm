package Test2::Manual::Testing::Migrating;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

=head1 NAME

Test2::Manual::Testing::Migrating - How to migrate existing tests from
Test::More to Test2.

=head1 DESCRIPTION

This tutorial covers the conversion of an existing test. This tutorial assumes
you have a test written using L<Test::More>.

=head1 LEGACY TEST

This tutorial will be converting this example test one section at a time:

C<t/example.t>:

    #####################
    # Boilerplate

    use strict;
    use warnings;

    use Test::More tests => 14;

    use_ok 'Scalar::Util';
    require_ok 'Exporter';

    #####################
    # Simple assertions (no changes)

    ok(1, "pass");

    is("apple", "apple", "Simple string compare");

    like("foo bar baz", qr/bar/, "Regex match");

    #####################
    # Todo

    {
        local $TODO = "These are todo";

        ok(0, "oops");
    }

    #####################
    # Deep comparisons

    is_deeply([1, 2, 3], [1, 2, 3], "Deep comparison");

    #####################
    # Comparing references

    my $ref = [1];
    is($ref, $ref, "Check that we have the same ref both times");

    #####################
    # Things that are gone

    ok(eq_array([1], [1]), "array comparison");
    ok(eq_hash({a => 1}, {a => 1}), "hash comparison");
    ok(eq_set([1, 3, 2], [1, 2, 3]), "set comparison");

    note explain([1, 2, 3]);

    {
        package THING;
        sub new { bless({}, shift) }
    }

    my $thing = new_ok('THING');

    #####################
    # Tools that changed

    isa_ok($thing, 'THING', '$thing');

    can_ok(__PACKAGE__, qw/ok is/);

=head1 BOILERPLATE

BEFORE:

    use strict;
    use warnings;

    use Test::More tests => 14;

    use_ok 'Scalar::Util';
    require_ok 'Exporter';

AFTER:

    use Test2::V0;
    plan(11);

    use Scalar::Util;
    require Exporter;

=over 4

=item Replace Test::More with Test2::V0

L<Test2::V0> is the recommended bundle. In a full migration you
will want to replace L<Test::More> with the L<Test2::V0> bundle.

B<Note:> You should always double check the latest L<Test2> to see if there is
a new recommended bundle. When writing a new test you should always use the
newest Test::V# module. Higher numbers are newer version.

=item NOTE: srand

When srand is on (default) it can cause problems with things like L<File::Temp>
which will end up attempting the same "random" filenames for every test process
started on a given day (or sharing the same seed).

If this is a problem for you then please disable srand when loading
L<Test2::V0>:

    use Test2::V0 -no_srand => 1;

=item Stop using use_ok()

C<use_ok()> has been removed. a C<use MODULE> statement will throw an exception
on failure anyway preventing the test from passing.

If you I<REALLY> want/need to assert that the file loaded you can use the L<ok>
module:

    use ok 'Scalar::Util';

The main difference here is that there is a space instead of an underscore.

=item Stop using require_ok()

C<require_ok> has been removed just like C<use_ok>. There is no L<ok> module
equivalent here. Just use C<require>.

=item Remove strict/warnings (optional)

The L<Test2::V0> bundle turns strict and warnings on for you.

=item Change where the plan is set

Test2 does not allow you to set the plan at import. In the old code you would
pass C<< tests => 11 >> as an import argument. In L<Test2> you either need to
use the C<plan()> function to set the plan, or use C<done_testing()> at the end
of the test.

If your test already uses C<done_testing()> you can keep that and no plan
changes are necessary.

B<Note:> We are also changing the plan from 14 to 11, that is because we
dropped C<use_ok>, C<require_ok>, and we will be dropping one more later on.
This is why C<done_testing()> is recommended over a set plan.

=back

=head1 SIMPLE ASSERTIONS

The vast majority of assertions will not need any changes:

    #####################
    # Simple assertions (no changes)

    ok(1, "pass");

    is("apple", "apple", "Simple string compare");

    like("foo bar baz", qr/bar/, "Regex match");

=head1 TODO

    {
        local $TODO = "These are todo";

        ok(0, "oops");
    }

The C<$TODO> package variable is gone. You now have a C<todo()> function.

There are 2 ways this can be used:

=over 4

=item todo $reason => sub { ... }

    todo "These are todo" => sub {
        ok(0, "oops");
    };

This is the cleanest way to do a todo. This will make all assertions inside the
codeblock into TODO assertions.

=item { my $TODO = todo $reason; ... }

    {
        my $TODO = todo "These are todo";

        ok(0, "oops");
    }

This is a system that emulates the old way. Instead of modifying a global
C<$TODO> variable you create a todo object with the C<todo()> function and
assign it to a lexical variable. Once the todo object falls out of scope the
TODO ends.

=back

=head1 DEEP COMPARISONS

    is_deeply([1, 2, 3], [1, 2, 3], "Deep comparison");

Deep comparisons are easy, simply replace C<is_deeply()> with C<is()>.

    is([1, 2, 3], [1, 2, 3], "Deep comparison");

=head1 COMPARING REFERENCES

    my $ref = [1];
    is($ref, $ref, "Check that we have the same ref both times");

The C<is()> function provided by L<Test::More> forces both arguments into
strings, which makes this a comparison of the reference addresses. L<Test2>'s
C<is()> function is a deep comparison, so this will still pass, but fails to
actually test what we want (that both references are the same exact ref, not
just identical structures.)

We now have the C<ref_is()> function that does what we really want, it ensures
both references are the same reference. This function does the job better than
the original, which could be thrown off by string overloading.

    my $ref = [1];
    ref_is($ref, $ref, "Check that we have the same ref both times");

=head1 TOOLS THAT ARE GONE

    ok(eq_array([1], [1]), "array comparison");
    ok(eq_hash({a => 1}, {a => 1}), "hash comparison");
    ok(eq_set([1, 3, 2], [1, 2, 3]), "set comparison");

    note explain([1, 2, 3]);

    {
        package THING;
        sub new { bless({}, shift) }
    }

    my $thing = new_ok('THING');

C<eq_array>, C<eq_hash> and C<eq_set> have been considered deprecated for a
very long time, L<Test2> does not provide them at all. Instead you can just use
C<is()>:

    is([1], [1], "array comparison");
    is({a => 1}, {a => 1}, "hash comparison");

C<eq_set> is a tad more complicated, see L<Test2::Tools::Compare> for an
explanation:

    is([1, 3, 2], bag { item 1; item 2; item 3; end }, "set comparison");

C<explain()> has a rocky history. There have been arguments about how it should
work. L<Test2> decided to simply not include C<explain()> to avoid the
arguments. You can instead directly use Data::Dumper:

    use Data::Dumper;
    note Dumper([1, 2, 3]);

C<new_ok()> is gone. The implementation was complicated, and did not add much
value:

    {
        package THING;
        sub new { bless({}, shift) }
    }

    my $thing = THING->new;
    ok($thing, "made a new thing");

The complete section after the conversion is:

    is([1], [1], "array comparison");
    is({a => 1}, {a => 1}, "hash comparison");
    is([1, 3, 2], bag { item 1; item 2; item 3; end }, "set comparison");

    use Data::Dumper;
    note Dumper([1, 2, 3]);

    {
        package THING;
        sub new { bless({}, shift) }
    }

    my $thing = THING->new;
    ok($thing, "made a new thing");

=head1 TOOLS THAT HAVE CHANGED

    isa_ok($thing, 'THING', '$thing');

    can_ok(__PACKAGE__, qw/ok is/);

In L<Test::More> these functions are very confusing, and most people use them
wrong!

C<isa_ok()> from L<Test::More> takes a thing, a class/reftype to check, and
then uses the third argument as an alternative display name for the first
argument (NOT a test name!).

C<can_ok()> from L<Test::More> is not consistent with C<isa_ok> as all
arguments after the first are subroutine names.

L<Test2> fixes this by making both functions consistent and obvious:

    isa_ok($thing, ['THING'], 'got a THING');

    can_ok(__PACKAGE__, [qw/ok is/], "have expected subs");

You will note that both functions take a thing, an arrayref as the second
argument, then a test name as the third argument.

=head1 FINAL VERSION

    #####################
    # Boilerplate

    use Test2::V0;
    plan(11);

    use Scalar::Util;
    require Exporter;

    #####################
    # Simple assertions (no changes)

    ok(1, "pass");

    is("apple", "apple", "Simple string compare");

    like("foo bar baz", qr/bar/, "Regex match");

    #####################
    # Todo

    todo "These are todo" => sub {
        ok(0, "oops");
    };

    #####################
    # Deep comparisons

    is([1, 2, 3], [1, 2, 3], "Deep comparison");

    #####################
    # Comparing references

    my $ref = [1];
    ref_is($ref, $ref, "Check that we have the same ref both times");

    #####################
    # Things that are gone

    is([1], [1], "array comparison");
    is({a => 1}, {a => 1}, "hash comparison");

    is([1, 3, 2], bag { item 1; item 2; item 3; end }, "set comparison");

    use Data::Dumper;
    note Dumper([1, 2, 3]);

    {
        package THING;
        sub new { bless({}, shift) }
    }

    my $thing = THING->new;

    #####################
    # Tools that changed

    isa_ok($thing, ['THING'], 'got a THING');

    can_ok(__PACKAGE__, [qw/ok is/], "have expected subs");

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



