package Test2::Manual::Concurrency;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Concurrency - Documentation for Concurrency support.

=head1 FORKING

=head2 Test2

Test2 supports forking. For forking to work you need to load L<Test2::IPC>.

=head2 Test::Builder

L<Test::Builder> Did not used to support forking, but now that it is based on
L<Test2> it does. L<Test2::IPC> must be loaded just as with L<Test2>.

=head2 Test2::Suite

L<Test2::Suite> tools should all work fine with I<true> forking unless
otherwise noted. Pseudo-fork via threads (Windows and a few others) is not
supported, but may work.

Patches will be accepted to repair any pseudo-fork issues, but for these to be
used or tested they must be requested. Fork tests should not run on pseudo-fork
systems unless they are requested with an environment var, or the
AUTHOR_TESTING var. Pseudo-fork is fragile, and we do not want to block install
due to a pseudo-fork flaw.

=head2 Test::SharedFork

L<Test::SharedFork> is currently support and maintained, though it is no longer
necessary thanks to L<Test2::IPC>. If usage ever drops off then the module may
be deprecated, but for now the policy is to not let it break. Currently it
simply loads L<Test2::IPC> if it can, and falls back to the old methods on
legacy installs.

=head2 Others

Individual authors are free to support or not support forking as they see fit.

=head1 THREADING

B<Note> This only applies to ithreads.

=head2 Test2

The core of Test2 supports threading so long as L<Test2::IPC> is loaded. Basic
threading support (making sure events make it to the parent thread) is fully
supported, and must not be broken.

Some times perl installs have broken threads (Some 5.10 versions compiled on
newer gcc's will segv by simply starting a thread). This is beyond Test2's
control, and not solvable in Test2. That said we strive for basic threading
support on perl 5.8.1+.

If Test2 fails for threads on any perl 5.8 or above, and it is reasonably
possible for Test2 to work around the issue, it should. (Patches and bug
reports welcome).

=head2 Test::Builder

L<Test::Builder> has had thread support for a long time. With Test2 the
mechanism for thread support was switched to L<Test2::IPC>. L<Test::Builder>
should still support threads as much as it did before the switch to Test2.
Support includes auto-enabling thread support if L<threads> is loaded before
Test::Builder.

If there is a deviation between the new and old threading behavior then it is a
bug (unless the old behavior itself can be classified as a bug.) Please report
(or patch!) any such threading issues.

=head2 Test2::Suite

Tools in L<Test2::Suite> have minimal threading support. Most of these tools do
not care/notice threading and simply work because L<Test2::IPC> handles it.
Feel free to report any thread related bugs in Test2::Suite. Be aware though
that these tools are not legacy, and have no pre-existing thread support, we
reserve the right to refuse adding thread support to them.

=head3 Test2::Workflow

L<Test2::Workflow> has been merged into L<Test2::Suite>, so it gets addressed
by this policy.

L<Test2::Workflow> has thread support, but you must ask for it. Thread tests
for Test2::Workflow do not event run without setting either the AUTHOR_TESTING
env var, or the T2_DO_THREAD_TESTS env var.

To use threads with Test2::Workflow you must set the T2_WORKFLOW_USE_THREADS
env var.

If you do rely on threads with Test2::Workflow and find a bug please report it,
but it will be given an ultra-low priority. Merging patches that fix threading
issues will be given normal priority.

=head1 SEE ALSO

L<Test2> - Test2 itself.

L<Test2::Suite> - Initial tools built using L<Test2>.

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
