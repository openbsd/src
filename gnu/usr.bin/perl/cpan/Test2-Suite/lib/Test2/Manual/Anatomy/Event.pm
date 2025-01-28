package Test2::Manual::Anatomy::Event;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Anatomy::Event - The internals of events

=head1 DESCRIPTION

Events are how tools effect global state, and pass information along to the
harness, or the human running the tests.

=head1 HISTORY

Before proceeding it is important that you know some history of events.
Initially there was an event API, and an event would implement the API to
produce an effect. This API proved to be lossy and inflexible. Recently the
'facet' system was introduced, and makes up for the shortcoming and
inflexibility of the old API.

All events must still implement the old API, but that can be largely automated
if you use the facet system effectively. Likewise essential facets can often be
deduced from events that only implement the old API, though their information
maybe less complete.

=head1 THE EVENT OBJECT

All event objects must subclass L<Test2::Event>. If you inherit from this base
class, and implement the old API properly, facets will be generated for you for
free. On the other hand you can inherit from this, and also import
L<Test2::Util::Facets2Legacy> which will instead rely on your facet data, and
deduce the old API from them.

All new events C<MUST> implement both APIs one way or the other. A common way
to do this is to simply implement both APIs directly in your event.

Here is a good template for a new event:

    package Test2::Event::Mine;
    use strict;
    use warnings;

    use parent 'Test2::Event';
    use Test2::Util::Facets2Legacy ':ALL';

    sub facet_data {
        my $self = shift;

        # Adds 'about', 'amnesty', and 'trace' facets
        my $out = $self->common_facet_data;

        # Add any additional facets to the $out hashref
        ...

        return $out;
    }

    1;

=head1 THE FACET API

The new API is a single method: C<facet_data()>. This method must return a
hashref where each key is specific to a facet type, and the value is either a
facet hashref, or an array of hashrefs. Some facets C<MUST> be lone hashrefs,
others C<MUST> be hashrefs inside an arrayref.

The I<standard> facet types are as follows:

=over 4

=item assert => {details => $name, pass => $bool, no_debug => $bool, number => $maybe_int}

Documented in L<Test2::EventFacet::Assert>. An event may only have one.

The 'details' key is the name of the assertion.

The 'pass' key denotes a passing or failing assertion.

The 'no_debug' key tells any harness or formatter that diagnostics should not
be added automatically to a failing assertion (used when there are custom
diagnostics instead).

The 'number' key is for harness use, never set it yourself.

=item about => {details => $string, no_display => $bool, package => $pkg}

Documented in L<Test2::EventFacet::About>. An event may only have one.

'details' is a human readable string describing the overall event.

'no_display' means that a formatter/harness should hide the event.

'package' is the package of the event the facet describes (IE: L<Test2::Event::Ok>)

=item amnesty => [{details => $string, tag => $short_string, inherited => $bool}]

Documented in L<Test2::EventFacet::Amnesty>. An event may have multiple.

This event is how things like 'todo' are implemented. Amnesty prevents a
failing assertion from causing a global test failure.

'details' is a human readable description of why the failure is being granted
amnesty (IE The 'todo' reason)

'tag' is a short human readable string, or category for the amnesty. This is
typically 'TODO' or 'SKIP'.

'inherited' is true if the amnesty was applied in a parent context (true if
this test is run in a subtest that is marked todo).

=item control => {details => $string, global => $bool, terminate => $maybe_int, halt => $bool,  has_callback => $bool, encoding => $enc}

Documented in L<Test2::EventFacet::Control>. An event may have one.

This facet is used to apply extra behavior when the event is processed.

'details' is a human readable explanation for the behavior.

'global' true if this event should be forwarded to, and processed by, all hubs
everywhere. (bail-out uses this)

'terminate' this should either be undef, or an integer. When defined this will
cause the test to exit with the specific exit code.

'halt' is used to signal any harness that no further test files should be run
(bail-out uses this).

'has_callback' is set to true if the event has a callback sub defined.

'encoding' used to tell the formatter what encoding to use.

=item errors => [{details => $string, tag => $short_string, fail => $bool}]

Documented in L<Test2::EventFacet::Error>. An event may have multiple.

'details' is a human readable explanation of the error.

'tag' is a short human readable category for the error.

'fail' is true if the error should cause test failure. If this is false the
error is simply informative, but not fatal.

=item info => [{details => $string, tag => $short_string, debug => $bool, important => $bool}]

Documented in L<Test2::EventFacet::Info>. An event may have multiple.

This is how diag and note are implemented.

'details' human readable message.

'tag' short category for the message, such as 'diag' or 'note'.

'debug' is true if the message is diagnostics in nature, this is the main
difference between a note and a diag.

'important' is true if the message is not diagnostics, but is important to have
it shown anyway. This is primarily used to communicate with a harness.

=item parent => {details => $string, hid => $hid, children => [...], buffered => 1}

Documented in L<Test2::EventFacet::Parent>. An event may have one.

This is used by subtests.

'details' human readable name of the subtest.

'hid' subtest hub id.

'children' an arrayref containing facet_data instances from all child events.

'buffered' true if it was a buffered subtest.

=item plan => {details => $string, count => $int, skip => $bool, none => $bool}

Documented in L<Test2::EventFacet::Plan>. An event may have one.

'details' is a human readable string describing the plan (for instance, why a
test is skipped)

'count' is the number of expected assertions (0 for skip)

'skip' is true if the plan is to skip the test.

'none' used for Test::More's 'no_plan' plan.

=item trace => {details => $string, frame => [$pkg, $file, $line, $sub], pid => $int, tid => $int, cid => $cid, hid => $hid, nested => $int, buffered => $bool}

Documented in L<Test2::EventFacet::Trace>. An event may have one.

This is how debugging information is tracked. This is taken from the context
object at event creation.

'details' human readable debug message (otherwise generated from frame)

'frame' first 4 fields returned by caller:
C<[$package, $file, $line, $subname]>.

'pid' the process id in which the event was created.

'tid' the thread is in which the event was created.

'cid' the id of the context used to create the event.

'hid' the id of the hub to which the event was sent.

'nest' subtest nesting depth of the event.

'buffered' is true if the event was generated inside a buffered subtest.

=back

Note that ALL facet types have a 'details' key that may have a string. This
string should always be human readable, and should be an explanation for the
facet. For an assertion this is the test name. For a plan this is the reason
for the plan (such as skip reason). For info it is the human readable
diagnostics message.

=head2 CUSTOM FACETS

You can write custom facet types as well, simply add a new key to the hash and
populated it. The general rule is that any code looking at the facets should
ignore any it does not understand.

Optionally you can also create a package to document your custom facet. The
package should be proper object, and may have additional methods to help work
with your facet.

    package Test2::EventFacet::MyFacet;

    use parent 'Test2::EventFacet';

    sub facet_key { 'myfacet' }
    sub is_list { 0 }

    1;

Your facet package should always be under the Test2::EventFacet:: namespace if
you want any tools to automatically find it. The last part of the namespace
should be the non-plural name of your facet with only the first word
capitalized.

=over 4

=item $string = $facet_class->facet_key

The key for your facet should be the same as the last section of
the namespace, but all lowercase. You I<may> append 's' to the key if your
facet is a list type.

=item $bool = $facet_class->is_list

True if an event should put these facets in a list:

    { myfacet => [{}, {}] }

False if an event may only have one of this type of facet at a time:

    { myfacet => {} }

=back

=head3 EXAMPLES

The assert facet is not a list type, so its implementation would look like this:

    package Test2::EventFacet::Assert;
    sub facet_key { 'assert' }
    sub is_list { 0 }

The amnesty facet is a list type, but amnesty does not need 's' appended to
make it plural:

    package Test2::EventFacet::Amnesty;
    sub facet_key { 'amnesty' }
    sub is_list { 1 }

The error facet is a list type, and appending 's' makes error plural as errors.
This means the package name is '::Error', but the key is 'errors'.

    package Test2::EventFacet::Error;
    sub facet_key { 'errors' }
    sub is_list { 1 }

B<Note2:> In practice most tools completely ignore the facet packages, and work
with the facet data directly in its raw structure. This is by design and
recommended. The facet data is intended to be serialized frequently and passed
around. When facets are concerned, data is important, classes and methods are
not.

=head1 THE OLD API

The old API was simply a set of methods you were required to implement:

=over 4

=item $bool = $e->causes_fail

Returns true if this event should result in a test failure. In general this
should be false.

=item $bool = $e->increments_count

Should be true if this event should result in a test count increment.

=item $e->callback($hub)

If your event needs to have extra effects on the L<Test2::Hub> you can override
this method.

This is called B<BEFORE> your event is passed to the formatter.

=item $num = $e->nested

If this event is nested inside of other events, this should be the depth of
nesting. (This is mainly for subtests)

=item $bool = $e->global

Set this to true if your event is global, that is ALL threads and processes
should see it no matter when or where it is generated. This is not a common
thing to want, it is used by bail-out and skip_all to end testing.

=item $code = $e->terminate

This is called B<AFTER> your event has been passed to the formatter. This
should normally return undef, only change this if your event should cause the
test to exit immediately.

If you want this event to cause the test to exit you should return the exit
code here. Exit code of 0 means exit success, any other integer means exit with
failure.

This is used by L<Test2::Event::Plan> to exit 0 when the plan is
'skip_all'. This is also used by L<Test2::Event:Bail> to force the test
to exit with a failure.

This is called after the event has been sent to the formatter in order to
ensure the event is seen and understood.

=item $msg = $e->summary

This is intended to be a human readable summary of the event. This should
ideally only be one line long, but you can use multiple lines if necessary. This
is intended for human consumption. You do not need to make it easy for machines
to understand.

The default is to simply return the event package name.

=item ($count, $directive, $reason) = $e->sets_plan()

Check if this event sets the testing plan. It will return an empty list if it
does not. If it does set the plan it will return a list of 1 to 3 items in
order: Expected Test Count, Test Directive, Reason for directive.

=item $bool = $e->diagnostics

True if the event contains diagnostics info. This is useful because a
non-verbose harness may choose to hide events that are not in this category.
Some formatters may choose to send these to STDERR instead of STDOUT to ensure
they are seen.

=item $bool = $e->no_display

False by default. This will return true on events that should not be displayed
by formatters.

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
