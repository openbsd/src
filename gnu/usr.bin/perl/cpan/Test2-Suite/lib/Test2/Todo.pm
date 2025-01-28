package Test2::Todo;
use strict;
use warnings;

use Carp qw/croak/;
use Test2::Util::HashBase qw/hub _filter reason/;

use Test2::API qw/test2_stack/;

use overload '""' => \&reason, fallback => 1;

our $VERSION = '0.000162';

sub init {
    my $self = shift;

    my $reason = $self->{+REASON};
    croak "The 'reason' attribute is required" unless defined $reason;

    my $hub = $self->{+HUB} ||= test2_stack->top;

    $self->{+_FILTER} = $hub->pre_filter(
        sub {
            my ($active_hub, $event) = @_;

            # Turn a diag into a note
            return Test2::Event::Note->new(%$event) if ref($event) eq 'Test2::Event::Diag';

            if ($active_hub == $hub) {
                $event->set_todo($reason) if $event->can('set_todo');
                $event->add_amnesty({tag => 'TODO', details => $reason});
                $event->set_effective_pass(1) if $event->isa('Test2::Event::Ok');
            }
            else {
                $event->add_amnesty({tag => 'TODO', details => $reason, inherited => 1});
            }

            return $event;
        },
        inherit => 1,
        todo => $reason,
    );
}

sub end {
    my $self = shift;
    my $hub = $self->{+HUB} or return;

    $hub->pre_unfilter($self->{+_FILTER});
    delete $self->{+HUB};
    delete $self->{+_FILTER};
}

sub DESTROY {
    my $self = shift;
    $self->end;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Todo - TODO extension for Test2.

=head1 DESCRIPTION

This is an object that lets you create and manage TODO states for tests. This
is an extension, not a plugin or a tool. This library can be used by plugins
and tools to manage todo states.

If you simply want to write a todo test then you should look at the C<todo>
function provided by L<Test2::Tools::Basic>.

=head1 SYNOPSIS

    use Test2::Todo;

    # Start the todo
    my $todo = Test2::Todo->new(reason => 'Fix later');

    # Will be considered todo, so suite still passes
    ok(0, "oops");

    # End the todo
    $todo->end;

    # TODO has ended, this test will actually fail.
    ok(0, "oops");

=head1 CONSTRUCTION OPTIONS

=over 4

=item reason (required)

The reason for the todo, this can be any defined value.

=item hub (optional)

The hub to which the TODO state should be applied. If none is provided then the
current global hub is used.

=back

=head1 INSTANCE METHODS

=over 4

=item $todo->end

End the todo state.

=back

=head1 CLASS METHODS

=over 4

=item $count = Test2::Todo->hub_in_todo($hub)

If the hub has any todo objects this will return the total number of them. If
the hub has no todo objects it will return 0.

=back

=head1 OTHER NOTES

=head2 How it works

When an instance is created a filter sub is added to the L<Test2::Hub>. This
filter will set the C<todo> and C<diag_todo> attributes on all events as they
come in. When the instance is destroyed, or C<end()> is called, the filter is
removed.

When a new hub is pushed (such as when a subtest is started) the new hub will
inherit the filter, but it will only set C<diag_todo>, it will not set C<todo>
on events in child hubs.

=head2 $todo->end is called at destruction

If your C<$todo> object falls out of scope and gets garbage collected, the todo
will end.

=head2 Can I use multiple instances?

Yes. The most recently created one that is still active will win.

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
