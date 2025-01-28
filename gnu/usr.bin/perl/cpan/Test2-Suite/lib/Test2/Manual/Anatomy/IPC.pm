package Test2::Manual::Anatomy::IPC;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Anatomy::IPC - Manual for the IPC system.

=head1 DESCRIPTION

This document describes the IPC system.

=head1 WHAT IS THE IPC SYSTEM

The IPC system is activated by loading L<Test2::IPC>. This makes hubs
process/thread aware, and makes them forward events along to the parent
process/thread as necessary.

=head1 HOW DOES THE IPC SYSTEM EFFECT EVERYTHING?

L<Test2::API> and L<Test2::API::Instance> have some behaviors that trigger if
L<Test2::IPC> is loaded before the global state is initialized. Mainly an IPC
driver will be initiated and stored in the global state.

If an IPC driver is initialized then all hubs will be initialized with a
reference to the driver instance. If a hub has an IPC driver instance it will
use it to forward events to parent processes and threads.

=head1 WHAT DOES AN IPC DRIVER DO?

An L<Test2::IPC::Driver> provides a way to send event data to a destination
process+thread+hub (or to all globally). The driver must also provide a way for
a process/thread/hub to read in any pending events that have been sent to it.

=head1 HOW DOES THE DEFAULT IPC DRIVER WORK?

The default IPC driver is L<Test2::API::Driver::Files>. This default driver,
when initialized, starts by creating a temporary directory. Any time an event
needs to be sent to another process/thread/hub, the event will be written to a
file using L<Storable>. The file is written with the destination process,
thread, and hub as part of the filename. All hubs will regularly check for
pending IPC events and will process them.

This driver is further optimized using a small chunk of SHM. Any time a new
event is sent via IPC the shm is updated to have a new value. Hubs will not
bother checking for new IPC events unless the shm value has changed since their
last poll. A result of this is that the IPC system is surprisingly fast, and
does not waste time polling the hard drive when there are no pending events.

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
