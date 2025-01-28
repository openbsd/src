package Test2::Manual::Testing::Todo;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Testing::Todo - Tutorial for marking tests as TODO.

=head1 DESCRIPTION

This tutorial covers the process of marking tests as TODO. It also describes
how TODO works under the hood.

=head1 THE TOOL

    use Test2::Tools::Basic qw/todo/;

=head2 TODO BLOCK

This form is low-magic. All tests inside the block are marked as todo, tests
outside the block are not todo. You do not need to do any variable management.
The flaw with this form is that it adds a couple levels to the stack, which can
break some high-magic tests.

Overall this is the preferred form unless you have a special case that requires
the variable form.

    todo "Reason for the todo" => sub {
        ok(0, "fail but todo");
        ...
    };

=head2 TODO VARIABLE

This form maintains the todo scope for the life of the variable. This is useful
for tests that are sensitive to scope changes. This closely emulates the
L<Test::More> style which localized the C<$TODO> package variable. Once the
variable is destroyed (set it to undef, scope end, etc) the TODO state ends.

    my $todo = todo "Reason for the todo";
    ok(0, "fail but todo");
    ...
    $todo = undef;

=head1 MANUAL TODO EVENTS

    use Test2::API qw/context/;

    sub todo_ok {
        my ($bool, $name, $todo) = @_;

        my $ctx = context();
        $ctx->send_event('Ok', pass => $bool, effective_pass => 1, todo => $todo);
        $ctx->release;

        return $bool;
    }

The L<Test2::Event::Ok> event has a C<todo> field which should have the todo
reason. The event also has the C<pass> and C<effective_pass> fields. The
C<pass> field is the actual pass/fail value. The C<effective_pass> is used to
determine if the event is an actual failure (should always be set tot true with
todo).

=head1 HOW THE TODO TOOLS WORK UNDER THE HOOD

The L<Test2::Todo> library gets the current L<Test2::Hub> instance and adds a
filter. The filter that is added will set the todo and effective pass fields on
any L<Test2::Event::Ok> events that pass through the hub. The filter also
converts L<Test2::Event::Diag> events into L<Test2::Event::Note> events.

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
