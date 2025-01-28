package Test2::Tools::Basic;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;
use Test2::API qw/context/;

our @EXPORT = qw{
    ok pass fail diag note todo skip
    plan skip_all done_testing bail_out
};
use base 'Exporter';

sub ok($;$@) {
    my ($bool, $name, @diag) = @_;
    my $ctx = context();
    $ctx->ok($bool, $name, \@diag);
    $ctx->release;
    return $bool ? 1 : 0;
}

sub pass {
    my ($name) = @_;
    my $ctx = context();
    $ctx->ok(1, $name);
    $ctx->release;
    return 1;
}

sub fail {
    my ($name, @diag) = @_;
    my $ctx = context();
    $ctx->ok(0, $name, \@diag);
    $ctx->release;
    return 0;
}

sub diag {
    my $ctx = context();
    $ctx->diag( join '', grep { defined $_ } @_ );
    $ctx->release;
    return 0;
}

sub note {
    my $ctx = context();
    $ctx->note( join '', grep { defined $_ } @_ );
    $ctx->release;
}

sub todo {
    my $reason = shift;
    my $code   = shift;

    require Test2::Todo unless $INC{'Test2/Todo.pm'};
    my $todo = Test2::Todo->new(reason => $reason);

    return $code->() if $code;

    croak "Cannot use todo() in a void context without a codeblock"
        unless defined wantarray;

    return $todo;
}

sub skip {
    my ($why, $num) = @_;
    $num ||= 1;
    my $ctx = context();
    $ctx->skip("skipped test", $why) for 1 .. $num;
    $ctx->release;
    no warnings 'exiting';
    last SKIP;
}

sub plan {
    my $plan = shift;
    my $ctx = context();

    if ($plan && $plan =~ m/[^0-9]/) {
        if ($plan eq 'tests') {
            $plan = shift;
        }
        elsif ($plan eq 'skip_all') {
            skip_all(@_);
            $ctx->release;
            return;
        }
    }

    $ctx->plan($plan);
    $ctx->release;
}

sub skip_all {
    my ($reason) = @_;
    my $ctx = context();
    $ctx->plan(0, SKIP => $reason);
    $ctx->release if $ctx;
}

sub done_testing {
    my $ctx = context();
    $ctx->hub->finalize($ctx->trace, 1);
    $ctx->release;
}

sub bail_out {
    my ($reason) = @_;
    my $ctx = context();
    $ctx->bail($reason);
    $ctx->release if $ctx;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Basic - Test2 implementation of the basic testing tools.

=head1 DESCRIPTION

This is a L<Test2> based implementation of the more basic tools originally
provided by L<Test::More>. Not all L<Test::More> tools are provided by this
package, only the basic/simple ones. Some tools have been modified for better
diagnostics capabilities.

=head1 SYNOPSIS

    use Test2::Tools::Basic;

    ok($x, "simple test");

    if ($passing) {
        pass('a passing test');
    }
    else {
        fail('a failing test');
    }

    diag "This is a diagnostics message on STDERR";
    note "This is a diagnostics message on STDOUT";

    {
        my $todo = todo "Reason for todo";
        ok(0, "this test is todo");
    }

    ok(1, "this test is not todo");

    todo "reason" => sub {
        ok(0, "this test is todo");
    };

    ok(1, "this test is not todo");

    SKIP: {
        skip "This will wipe your drive";

        # This never gets run:
        ok(!system('sudo rm -rf /'), "Wipe drive");
    }

    done_testing;

=head1 EXPORTS

All subs are exported by default.

=head2 PLANNING

=over 4

=item plan($num)

=item plan('tests' => $num)

=item plan('skip_all' => $reason)

Set the number of tests that are expected. This must be done first or last,
never in the middle of testing.

For legacy compatibility you can specify 'tests' as the first argument before
the number. You can also use this to skip all with the 'skip_all' prefix,
followed by a reason for skipping.

=item skip_all($reason)

Set the plan to 0 with a reason, then exit true. This should be used before any
tests are run.

=item done_testing

Used to mark the end of testing. This is a safe way to have a dynamic or
unknown number of tests.

=item bail_out($reason)

Invoked when something has gone horribly wrong: stop everything, kill all threads and
processes, end the process with a false exit status.

=back

=head2 ASSERTIONS

=over 4

=item ok($bool)

=item ok($bool, $name)

=item ok($bool, $name, @diag)

Simple assertion. If C<$bool> is true the test passes, and if it is false the test
fails. The test name is optional, and all arguments after the name are added as
diagnostics message if and only if the test fails. If the test passes all the
diagnostics arguments will be ignored.

=item pass()

=item pass($name)

Fire off a passing test (a single Ok event). The name is optional

=item fail()

=item fail($name)

=item fail($name, @diag)

Fire off a failing test (a single Ok event). The name and diagnostics are optional.

=back

=head2 DIAGNOSTICS

=over 4

=item diag(@messages)

Write diagnostics messages. All items in C<@messages> will be joined into a
single string with no separator. When using TAP, diagnostics are sent to STDERR.

Returns false, so as to preserve failure.

=item note(@messages)

Write note-diagnostics messages. All items in C<@messages> will be joined into
a single string with no separator. When using TAP, notes are sent to STDOUT.

=back

=head2 META

=over 4

=item $todo = todo($reason)

=item todo $reason => sub { ... }

This is used to mark some results as TODO. TODO means that the test may fail,
but will not cause the overall test suite to fail.

There are two ways to use this. The first is to use a codeblock, and the TODO will
only apply to the codeblock.

    ok(1, "before"); # Not TODO

    todo 'this will fail' => sub {
        # This is TODO, as is any other test in this block.
        ok(0, "blah");
    };

    ok(1, "after"); # Not TODO

The other way is to use a scoped variable. TODO will end when the variable is
destroyed or set to undef.

    ok(1, "before"); # Not TODO

    {
        my $todo = todo 'this will fail';

        # This is TODO, as is any other test in this block.
        ok(0, "blah");
    };

    ok(1, "after"); # Not TODO

This is the same thing, but without the C<{...}> scope.

    ok(1, "before"); # Not TODO

    my $todo = todo 'this will fail';

    ok(0, "blah"); # TODO

    $todo = undef;

    ok(1, "after"); # Not TODO

=item skip($why)

=item skip($why, $count)

This is used to skip some tests. This requires you to wrap your tests in a
block labeled C<SKIP:>. This is somewhat magical. If no C<$count> is specified
then it will issue a single result. If you specify C<$count> it will issue that
many results.

    SKIP: {
        skip "This will wipe your drive";

        # This never gets run:
        ok(!system('sudo rm -rf /'), "Wipe drive");
    }

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
