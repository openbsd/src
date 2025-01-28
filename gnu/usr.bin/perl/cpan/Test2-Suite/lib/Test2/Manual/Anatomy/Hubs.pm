package Test2::Manual::Anatomy::Hubs;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Anatomy::Hubs - Internals documentation for the hub stack, and
hubs.

=head1 DESCRIPTION

This document describes the hub stack, and the hubs it contains. It explains
why we have a stack, and when to add/remove hubs from it.

=head1 WHAT IS A HUB?

Test2 is an event system, tools generate events, those events are then
processed to modify the testing state (number of tests, number of failures,
etc). The hub is responsible for receiving and processing events to record the
change in state. All events should eventually reach a destination hub.

The base hub is L<Test2::Hub>. All hub classes should inherit from the base hub
class. The base hub class provides several hooks that allow you to monitor or
modify events. Hubs are also responsible for forwarding events to the output
formatter.

=head1 WHY DO WE HAVE A HUB STACK?

There are cases where it makes sense to have more than one hub:

=over 4

=item subtests

In Test2 subtests are implemented using the hub stack. When you start a subtest
a new L<Test2::Hub::Subtest> instance is created and pushed to the stack. Once
this is done all calls to C<Test2::API::context> will find the new hub and send
all events to it. When the subtest tool is complete it will remove the new hub,
and send a final subtest event to the parent hub.

=item testing your test tools

C<Test2::API::intercept()> is implemented using the hub stack. The
C<Test2::API::intercept()> function will add an L<Test2::Hub::Interceptor>
instance to the stack, any calls to L<Test2::API::context()> will find the new
hub, and send it all events. The intercept hub is special in that is has no
connection to the parent hub, and usually does not have a formatter.

=back

=head1 WHEN SHOULD I ADD A HUB TO THE STACK?

Any time you want to intercept or block events from effecting the test state.
Adding a new hub is essentially a way to create a sandbox where you have
absolute control over what events do. Adding a new hub insures that the main
test state will not be effected.

=head1 WHERE IS THE STACK?

The stack is an instance of L<Test2::API::Stack>. You can access the global hub
stack using C<Test2::API::test2_stack>.

=head1 WHAT ABOUT THE ROOT HUB?

The root hub is created automatically as needed. A call to
C<< Test2::API::test2_stack->top() >> will create the root hub if it does not
already exist.

=head1 HOW DO HUBS HANDLE IPC?

If the IPC system (L<Test2::IPC>) was not loaded, then IPC is not handled at
all. Forking or creating new threads without the IPC system can cause
unexpected problems.

All hubs track the PID and Thread ID that was current when they were created.
If an event is sent to a hub in a new process/thread the hub will detect this
and try to forward the event along to the correct process/thread. This is
accomplished using the IPC system.

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
