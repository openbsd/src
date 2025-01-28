package Test2::Manual::Anatomy::Context;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Anatomy::Context - Internals documentation for the Context
objects.

=head1 DESCRIPTION

This document explains how the L<Test2::API::Context> object works.

=head1 WHAT IS THE CONTEXT OBJECT?

The context object is one of the key components of Test2, and makes many
features possible that would otherwise be impossible. Every test tool starts by
getting a context, and ends by releasing the context. A test tool does all its
work between getting and releasing the context. The context instance is the
primary interface for sending events to the Test2 stack. Finally the context
system is responsible for tracking what file and line number a tool operates
on, which is critical for debugging.

=head2 PRIMARY INTERFACE FOR TEST TOOLS

Nearly every Test2 based tool should start by calling C<$ctx =
Test2::API::context()> in order to get a context object, and should end by
calling C<< $ctx->release() >>. Once a tool has its context object it can call
methods on the object to send events or have other effects. Nearly everything a
test tool needs to do should be done through the context object.

=head2 TRACK FILE AND LINE NUMBERS FOR ERROR REPORTING

When you call C<Test2::API::Context> a new context object will be returned. If
there is already a context object in effect (from a different point in the
stack) you will get a clone of the existing one. If there is not already a
current context then a completely new one will be generated. When a new context
is generated Test2 will determine the file name and line number for your test
code, these will be used when reporting any failures.

Typically the file and line number will be determined using C<caller()> to look
at your tools caller. The C<$Test::Builder::Level> will be respected if
detected, but is discouraged in favor of just using context objects at every
level.

When calling C<Test2::API::Context()> you can specify the
C<< level => $count >> arguments if you need to look at a deeper caller.

=head2 PRESERVE $?, $!, $^E AND $@

When you call C<Test2::API::context()> the current values of C<$?>, C<$!>,
C<$^E>, and C<$@> are stored in the context object itself. Whenever the context
is released the original values of these variables will be restored. This
protects the variables from any side effects caused by testing tools.

=head2 FINALIZE THE API STATE

L<Test2::API> works via a hidden singleton instance of L<Test2::API::Instance>.
The singleton has some state that is not set in stone until the last possible
minute. The last possible minute happens to be the first time a context is
acquired. State includes IPC instance, Formatter class, Root PID, etc.

=head2 FIND/CREATE THE CURRENT/ROOT HUB

L<Test2> has a stack of hubs, the stack can be accessed via
L<Test2::API::test2_stack>. When you get a context it will find the current
hub, if there is no current hub then the root one will be initialized.

=head2 PROVIDE HOOKS

There are hooks that run when contexts are created, found, and released. See
L<Test2::API> for details on these hooks and how to use them.

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
