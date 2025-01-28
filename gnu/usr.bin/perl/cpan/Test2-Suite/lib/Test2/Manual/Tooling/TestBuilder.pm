package Test2::Manual::Tooling::TestBuilder;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::TestBuilder - This section maps Test::Builder methods
to Test2 concepts.

=head1 DESCRIPTION

With Test::Builder tools were encouraged to use methods on the Test::Builder
singleton object. Test2 has a different approach, every tool should get a new
L<Test2::API::Context> object, and call methods on that. This document maps
several concepts from Test::Builder to Test2.

=head1 CONTEXT

First thing to do, stop using the Test::Builder singleton, in fact stop using
or even loading Test::Builder. Instead of Test::Builder each tool you write
should follow this template:

    use Test2::API qw/context/;

    sub my_tool {
        my $ctx  = context();

        ... do work ...

        $ctx->ok(1, "a passing assertion");

        $ctx->release;

        return $whatever;
    }

The original Test::Builder style was this:

    use Test::Builder;
    my $tb = Test::Builder->new; # gets the singleton

    sub my_tool {
        ... do work ...

        $tb->ok(1, "a passing assertion");

        return $whatever;
    }

=head1 TEST BUILDER METHODS

=over 4

=item $tb->BAIL_OUT($reason)

The context object has a 'bail' method:

    $ctx->bail($reason)

=item $tb->diag($string)

=item $tb->note($string)

The context object has diag and note methods:

    $ctx->diag($string);
    $ctx->note($string);

=item $tb->done_testing

The context object has a done_testing method:

    $ctx->done_testing;

Unlike the Test::Builder version, no arguments are allowed.

=item $tb->like

=item $tb->unlike

These are not part of context, instead look at L<Test2::Compare> and
L<Test2::Tools::Compare>.

=item $tb->ok($bool, $name)

    # Preferred
    $ctx->pass($name);
    $ctx->fail($name, @diag);

    # Discouraged, but supported:
    $ctx->ok($bool, $name, \@failure_diags)

=item $tb->subtest

use the C<Test2::API::run_subtest()> function instead. See L<Test2::API> for documentation.

=item $tb->todo_start

=item $tb->todo_end

See L<Test2::Tools::Todo> instead.

=item $tb->output, $tb->failure_output, and $tb->todo_output

These are handled via formatters now. See L<Test2::Formatter> and
L<Test2::Formatter::TAP>.

=back

=head1 LEVEL

L<Test::Builder> had the C<$Test::Builder::Level> variable that you could
modify in order to set the stack depth. This was useful if you needed to nest
tools and wanted to make sure your file and line number were correct. It was
also frustrating and prone to errors. Some people never even discovered the
level variable and always had incorrect line numbers when their tools would
fail.

L<Test2> uses the context system, which solves the problem a better way. The
top-most tool get a context, and holds on to it until it is done. Any tool
nested under the first will find and use the original context instead of
generating a new one. This means the level problem is solved for free, no
variables to mess with.

L<Test2> is also smart enough to honor C<$Test::Builder::Level> if it is set.

=head1 TODO

L<Test::Builder> used the C<$TODO> package variable to set the TODO state. This
was confusing, and easy to get wrong. See L<Test2::Tools::Todo> for the modern
way to accomplish a TODO state.

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
