package Test2::Manual::Anatomy::EndToEnd;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::EndToEnd - Overview of Test2 from load to finish.

=head1 DESCRIPTION

This is a high level overview of everything from loading Test2 through the end
of a test script.

=head1 WHAT HAPPENS WHEN I LOAD THE API?

    use Test2::API qw/context/;

=over 4

=item A singleton instance of Test2::API::Instance is created.

You have no access to this, it is an implementation detail.

=item Several API functions are defined that use the singleton instance.

You can import these functions, or use them directly.

=item Then what?

It waits...

The API intentionally does as little as possible. At this point something can
still change the formatter, load L<Test2::IPC>, or have other global effects
that need to be done before the first L<Test2::API::Context> is created. Once
the first L<Test2::API::Context> is created the API will finish initialization.

See L</"WHAT HAPPENS WHEN I ACQUIRE A CONTEXT?"> for more information.

=back

=head1 WHAT HAPPENS WHEN I USE A TOOL?

This section covers the basic workflow all tools such as C<ok()> must follow.

    sub ok($$) {
        my ($bool, $name) = @_;

        my $ctx = context();

        my $event = $ctx->send_event('Ok', pass => $bool, name => $name);

        ...

        $ctx->release;
        return $bool;
    }

    ok(1, "1 is true");

=over 4

=item A tool function is run.

    ok(1, "1 is true");

=item The tool acquires a context object.

    my $ctx = context();

See L</"WHAT HAPPENS WHEN I ACQUIRE A CONTEXT?"> for more information.

=item The tool uses the context object to create, send, and return events.

See L</"WHAT HAPPENS WHEN I SEND AN EVENT?"> for more information.

    my $event = $ctx->send_event('Ok', pass => $bool, name => $name);

=item When done the tool MUST release the context.

See L</"WHAT HAPPENS WHEN I RELEASE A CONTEXT?"> for more information.

    $ctx->release();

=item The tool returns.

    return $bool;

=back

=head1 WHAT HAPPENS WHEN I ACQUIRE A CONTEXT?

    my $ctx = context();

These actions may not happen exactly in this order, but that is an
implementation detail. For the purposes of this document this order is used to
help the reader understand the flow.

=over 4

=item $!, $@, $? and $^E are captured and preserved.

Test2 makes a point to preserve the values of $!, $@, $? and $^E such that the test
tools do not modify these variables unexpectedly. They are captured first thing
so that they can be restored later.

=item The API state is changed to 'loaded'.

The 'loaded' state means that test tools have already started running. This is
important as some plugins need to take effect before any tests are run. This
state change only happens the first time a context is acquired, and may trigger
some hooks defined by plugins to run.

=item The current hub is found.

A context attaches itself to the current L<Test2::Hub>. If there is no current
hub then the root hub will be initialized. This will also initialize the hub
stack if necessary.

=item Context acquire hooks fire.

It is possible to create global, or hub-specific hooks that fire whenever a
context is acquired, these hooks will fire now. These hooks fire even if there
is an existing context.

=item Any existing context is found.

If the current hub already has a context then a clone of it will be used
instead of a completely new context. This is important because it allows nested
tools to inherit the context used by parent tools.

=item Stack depth is measured.

Test2 makes a point to catch mistakes in how the context is used. The stack
depth is used to accomplish this. If there is an existing context the depth
will be checked against the one found here. If the old context has the same
stack depth, or a shallower one, it means a tool is misbehaving and did not
clean up the context when it was done, in which case the old context will be
cleaned up, and a warning issued.

=item A new context is created (if no existing context was found)

If there is no existing context, a new one will be created using the data
collected so far.

=item Context init hooks fire (if no existing context was found)

If a new context was created, context-creation hooks will fire.

=item $!, $@, $?, and $^E are restored.

We make sure $!, $@, $?, and $^E are unchanged at this point so that changes we
made will not effect anything else. This is done in case something inside the
context construction accidentally changed these vars.

=item The context is returned.

You have a shiney new context object, or a clone of the existing context.

=back

=head1 WHAT HAPPENS WHEN I SEND AN EVENT?

    my $event = $ctx->send_event('Ok', pass => $bool, name => $name);

=over 4

=item The Test2::Event::Ok module is loaded.

The C<send_event()> method will automatically load any Event package necessary.
Normally C<send_event()> will assume the first argument is an event class
without the C<Test2::Event::> prefix, which it will add for you. If you want to
use an event class that is in a different namespace you can prefix the class
name with a C<+> to tell the tool that you are giving a fully qualified class
name:

    my $event = $ctx->send_event('+Fully::Qualified::Event', pass => $bool, name => $name);

=item A new instance of Test2::Event::Ok is created.

The event object is instantiated using the provided parameters.

=item The event object is sent to the hub.

The hub takes over from here.

=item The hub runs the event through any filters.

Filters are able to modify or remove events. Filters are run first, before the
event can modify global test state.

=item The global test state is updated to reflect the event.

If the event effects test count then the count will be incremented. If the
event causes failure then the failure count will be incremented. There are a
couple other ways the global state can be effected as well.

=item The event is sent to the formatter

After the state is changed the hub will send the event to the formatter for
rendering. This is where TAP is normally produced.

=item The event is sent to all listeners.

There can be any number of listeners that take action when events are
processed, this happens now.

=back

=head1 WHAT HAPPENS WHEN I RELEASE A CONTEXT?

    $ctx->release;

=over 4

=item The current context clone is released.

If your tool is nested inside another, then releasing will simply destroy the
copy of the context, nothing else will happen.

=item If this was the canonical context, it will actually release

When a context is created it is considered 'canon'. Any context obtained by a
nested tool will be considered a child context linked to the canonical one.
Releasing child contexts does not do anything of note (but is still required).

=item Release hooks are called

Release hooks are the main motivation behind making the C<release()> method,
and making it a required action on the part of test tools. These are hooks that
we can have called when a tool is complete. This is how plugins like
L<Test2::Plugin::DieOnFail> are implemented. If we simply had a destructor call
the hooks then we would be unable to write this plugin as a C<die> inside of a
destructor is useless.

=item The context is cleared

The main context data is cleared allowing the next tool to create a new
context. This is important as the next tool very likely has a new line number.

=item $!, $@, $?, and $^E are restored

When a Test2 tool is complete it will restore $@, $!, $? and $^E to avoid action at
a distance.

=back

=head1 WHAT HAPPENS WHEN I USE done_testing()?

    done_testing();

=over 4

=item Any pending IPC events will be culled.

If IPC is turned on, a final culling will take place.

=item Follow-up hooks are run

The follow-up hooks are a way to run actions when a hub is complete. This is
useful for adding cleanup tasks, or final tests to the end of a test.

=item The final plan event is generated and processed.

The final plan event will be produced using the current test count as the
number of tests planned.

=item The current hub is finalized.

This will mark the hub is complete, and will not allow new events to be
processed.

=back

=head1 WHAT HAPPENS WHEN A TEST SCRIPT IS DONE?

Test2 has some behaviors it runs in an C<END { ... }> block after tests are
done running. This end block does some final checks to warn you if something
went wrong. This end block also sets the exit value of the script.

=over 4

=item API Versions are checked.

A warning will be produced if L<Test::Builder> is loaded, but has a different
version compared to L<Test2::API>. This situation can happen if you downgrade
to an older Test-Simple distribution, and is a bad situation.

=item Any remaining context objects are cleaned up.

If there are leftover context objects they will need to be cleaned up. A
leftover context is never a good thing, and usually requires a warning. A
leftover context could also be the result of an exception being thrown which
terminates the script, L<Test2> is fairly good at noticing this and not warning
in these cases as the warning would simply be noise.

=item Child processes are sent a 'waiting' event.

If IPC is active, a waiting event is sent to all child processes.

=item The script will wait for all child processes and/or threads to complete.

This happens only when IPC is loaded, but Test::Builder is not. This behavior
is useful, but would break compatibility for legacy tests.

=item The hub stack is cleaned up.

All hubs are finalized starting from the top. Leftover hubs are usually a bad
thing, so a warning is produced if any are found.

=item The root hub is finalized.

This step is a no-op if C<done_testing()> was used. If needed this will mark
the root hub as finished.

=item Exit callbacks are called.

This is a chance for plugins to modify the final exit value of the script.

=item The scripts exit value ($?) is set.

If the test encountered any failures this will be set to a non-zero value. If
possible this will be set to the number of failures, or 255 if the number is
larger than 255 (the max value allowed).

=item Broken module diagnostics

Test2 is aware of many modules which were broken by Test2's release. At this
point the script will check if any known-broken modules were loaded, and warn
you if they were.

B<Note:> This only happens if there were test failures. No broken module
warnings are produced on a success.

=back

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
