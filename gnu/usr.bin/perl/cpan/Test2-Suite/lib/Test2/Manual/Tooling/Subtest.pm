package Test2::Manual::Tooling::Subtest;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Subtest - How to implement a tool that makes use of
subtests.

=head1 DESCRIPTION

Subtests are a nice way of making related events visually, and architecturally
distinct.

=head1 WHICH TYPE OF SUBTEST DO I NEED?

There are 2 types of subtest. The first type is subtests with user-supplied
coderefs, such as the C<subtest()> function itself. The second type is subtest
that do not have any user supplied coderefs.

So which type do you need? The answer to that is simple, if you are going to
let the user define the subtest with their own codeblock, you have the first
type, otherwise you have the second.

In either case, you will still need use the same API function:
C<Test2::API::run_subtest>.

=head2 SUBTEST WITH USER SUPPLIED CODEREF

This example will emulate the C<subtest> function.

    use Test2::API qw/context run_subtest/;

    sub my_subtest {
        my ($name, $code) = @_;

        # Like any other tool, you need to acquire a context, if you do not then
        # things will not report the correct file and line number.
        my $ctx = context();

        my $bool = run_subtest($name, $code);

        $ctx->release;

        return $bool;
    }

This looks incredibly simple... and it is. C<run_subtest()> does all the hard
work for you. This will issue an L<Test2::Event::Subtest> event with the
results of the subtest. The subtest event itself will report to the proper file
and line number due to the context you acquired (even though it does not I<look>
like you used the context.

C<run_subtest()> can take additional arguments:

    run_subtest($name, $code, \%params, @args);

=over 4

=item @args

This allows you to pass arguments into the codeblock that gets run.

=item \%params

This is a hashref of parameters. Currently there are 3 possible parameters:

=over 4

=item buffered => $bool

This will turn the subtest into the new style buffered subtest. This type of
subtest is recommended, but not default.

=item inherit_trace => $bool

This is used for tool-side coderefs.

=item no_fork => $bool

react to forking/threading inside the subtest itself. In general you are
unlikely to need/want this parameter.

=back

=back

=head2 SUBTEST WITH TOOL-SIDE CODEREF

This is particularly useful if you want to turn a tool that wraps other tools
into a subtest. For this we will be using the tool we created in
L<Test2::Manual::Tooling::Nesting>.

    use Test2::API qw/context run_subtest/;

    sub check_class {
        my $class = shift;

        my $ctx = context();

        my $code = sub {
            my $obj = $class->new;
            is($obj->foo, 'foo', "got foo");
            is($obj->bar, 'bar', "got bar");
        };

        my $bool = run_subtest($class, $code, {buffered => 1, inherit_trace => 1});

        $ctx->release;

        return $bool;
    }

The C<run_subtest()> function does all the heavy lifting for us. All we need
to do is give the function a name, a coderef to run, and the
C<< inherit_trace => 1 >> parameter. The C<< buffered => 1 >> parameter is
optional, but recommended.

The C<inherit_trace> parameter tells the subtest tool that the contexts acquired
inside the nested tools should use the same trace as the subtest itself. For
user-supplied codeblocks you do not use inherit_trace because you want errors
to report to the user-supplied file+line.

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
