package Test2::Manual::Tooling::Testing;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Testing - Tutorial on how to test your testing tools.

=head1 DESCRIPTION

Testing your test tools used to be a complex and difficult prospect. The old
tools such as L<Test::Tester> and L<Test::Builder::Tester> were limited, and
fragile. Test2 on the other hand was designed from the very start to be easily
tested! This tutorial shows you how.

=head1 THE HOLY GRAIL OF TESTING YOUR TOOLS

The key to making Test2 easily testable (specially when compared to
Test::Builder) is the C<intercept> function.

    use Test2::API qw/intercept/;

    my $events = intercept {
        ok(1, "pass");
        ok(0, "fail");

        diag("A diag");
    };

The intercept function lets you use any test tools you want inside a codeblock.
No events or contexts generated within the intercept codeblock will have any
effect on the outside testing state. The C<intercept> function completely
isolates the tools called within.

B<Note:> Plugins and things that effect global API state may not be fully
isolated. C<intercept> is intended specifically for event isolation.

The C<intercept> function will return an arrayref containing all the events
that were generated within the codeblock. You can now make any assertions you
want about the events you expected your tools to generate.

    [
        bless({...}, 'Test2::Event::Ok'),   # pass
        bless({...}, 'Test2::Event::Ok'),   # fail
        bless({...}, 'Test2::Event::Diag'), # Failure diagnostics (not always a second event)
        bless({...}, 'Test2::Event::Diag'), # custom 'A diag' message
    ]

Most test tools eventually produce one or more events. To effectively verify
the events you get from intercept you really should read up on how events work
L<Test2::Manual::Anatomy::Event>. Once you know about events you can move on to
the next section which points you at some helpers.

=head1 ADDITIONAL HELPERS

=head2 Test2::Tools::Tester

This is the most recent set of tools to help you test your events. To really
understand these you should familiarize yourself with
L<Test2::Manual::Anatomy::Event>. If you are going to be writing anything more
than the most simple of tools you should know how events work.

The L<Test2::Tools::Tester> documentation is a good place for further reading.

=head2 Test2::Tools::HarnessTester

The L<Test2::Tools::HarnessTester> can export the C<summarize_events()> tool.
This tool lets you run your event arrayref through L<Test2::Harness> so that you
can get a pass/fail summary.

    my $summary = summarize_events($events);

The summary looks like this:

    {
        plan       => $plan_facet,         # the plan event facet
        pass       => $bool,               # true if the events result in a pass
        fail       => $bool,               # true if the events result in a fail
        errors     => $error_count,        # Number of error facets seen
        failures   => $failure_count,      # Number of failing assertions seen
        assertions => $assertion_count,    # Total number of assertions seen
    }

=head2 Test2::Tools::Compare

B<DEPRECATED> These tools were written before the switch to faceted events.
These will still work, but are no longer the recommended way to test your
tools.

The L<Test2::Tools::Compare> library exports a handful of extras to help test
events.

=over 4

=item event $TYPE => ...

Use in an array check against $events to check for a specific type of event
with the properties you specify.

=item fail_events $TYPE => ...

Use when you expect a failing assertion of $TYPE. This will automatically check
that the next event following it is a diagnostics message with the default
failure text.

B<Note:> This is outdated as a single event may now possess both the failing
assertion AND the failing text, such events will fail this test.

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
