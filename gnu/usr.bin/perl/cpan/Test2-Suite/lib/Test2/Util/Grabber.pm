package Test2::Util::Grabber;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::Hub::Interceptor();
use Test2::EventFacet::Trace();

use Test2::API qw/test2_stack test2_ipc/;

use Test2::Util::HashBase qw/hub finished _events term_size <state <trace/;

sub init {
    my $self = shift;

    # Make sure we have a hub on the stack
    test2_stack->top();

    my $hub = test2_stack->new_hub(
        class => 'Test2::Hub::Interceptor',
        formatter => undef,
        no_ending => 1,
    );

    $self->{+HUB} = $hub;

    my @events;
    $hub->listen(sub { push @events => $_[1] });

    $self->{+_EVENTS} = \@events;

    $self->{+TERM_SIZE} = $ENV{TS_TERM_SIZE};
    $ENV{TS_TERM_SIZE} = 80;

    my $trace = $self->{+TRACE} ||= Test2::EventFacet::Trace->new(frame => [caller(1)]);
    my $state = $self->{+STATE} ||= {};
    $hub->clean_inherited(trace => $trace, state => $state);

    return;
}

sub flush {
    my $self = shift;
    my $out = [@{$self->{+_EVENTS}}];
    @{$self->{+_EVENTS}} = ();
    return $out;
}

sub events {
    my $self = shift;
    # Copy
    return [@{$self->{+_EVENTS}}];
}

sub finish {
    my ($self) = @_; # Do not shift;
    $_[0] = undef;

    if (defined $self->{+TERM_SIZE}) {
        $ENV{TS_TERM_SIZE} = $self->{+TERM_SIZE};
    }
    else {
        delete $ENV{TS_TERM_SIZE};
    }

    my $hub = $self->{+HUB};

    $self->{+FINISHED} = 1;
    test2_stack()->pop($hub);

    my $trace = $self->{+TRACE} ||= Test2::EventFacet::Trace->new(frame => [caller(1)]);
    my $state = $self->{+STATE} ||= {};
    $hub->clean_inherited(trace => $trace, state => $state);

    my $dbg = Test2::EventFacet::Trace->new(
        frame => [caller(0)],
    );
    $hub->finalize($dbg, 1)
        if !$hub->no_ending
        && !$hub->state->ended;

    return $self->flush;
}

sub DESTROY {
    my $self = shift;
    return if $self->{+FINISHED};
    test2_stack->pop($self->{+HUB});
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Grabber - Object used to temporarily intercept all events.

=head1 DESCRIPTION

Once created this object will intercept and stash all events sent to the shared
L<Test2::Hub> object. Once the object is destroyed, events will once
again be sent to the shared hub.

=head1 SYNOPSIS

    use Test2 qw/Core Grab/;

    my $grab = grab();

    # Generate some events, they are intercepted.
    ok(1, "pass");
    ok(0, "fail");

    my $events_a = $grab->flush;

    # Generate some more events, they are intercepted.
    ok(1, "pass");
    ok(0, "fail");

    # Same as flush, except it destroys the grab object.
    my $events_b = $grab->finish;

After calling C<finish()> the grab object is destroyed and C<$grab> is set to
undef. C<$events_a> is an arrayref with the first two events. C<$events_b> is an
arrayref with the second two events.

=head1 EXPORTS

=over 4

=item $grab = grab()

This lets you intercept all events for a section of code without adding
anything to your call stack. This is useful for things that are sensitive to
changes in the stack depth.

    my $grab = grab();
        ok(1, 'foo');
        ok(0, 'bar');

    # $grab is magically undef after this.
    my $events = $grab->finish;

    is(@$events, 2, "grabbed two events.");

When you call C<finish()> the C<$grab> object will automagically undef itself,
but only for the reference used in the method call. If you have other
references to the C<$grab> object they will not be set to undef.

If the C<$grab> object is destroyed without calling C<finish()>, it will
automatically clean up after itself and restore the parent hub.

    {
        my $grab = grab();
        # Things are grabbed
    }
    # Things are back to normal

By default the hub used has C<no_ending> set to true. This will prevent the hub
from enforcing that you issued a plan and ran at least one test. You can turn
enforcement back one like this:

    $grab->hub->set_no_ending(0);

With C<no_ending> turned off, C<finish> will run the post-test checks to
enforce the plan and that tests were run. In many cases this will result in
additional events in your events array.

=back

=head1 METHODS

=over 4

=item $grab = $class->new()

Create a new grab object, immediately starts intercepting events.

=item $ar = $grab->flush()

Get an arrayref of all the events so far, clearing the grab objects internal
list.

=item $ar = $grab->events()

Get an arrayref of all events so far. Does not clear the internal list.

=item $ar = $grab->finish()

Get an arrayref of all the events, then destroy the grab object.

=item $hub = $grab->hub()

Get the hub that is used by the grab event.

=back

=head1 ENDING BEHAVIOR

By default the hub used has C<no_ending> set to true. This will prevent the hub
from enforcing that you issued a plan and ran at least one test. You can turn
enforcement back one like this:

    $grab->hub->set_no_ending(0);

With C<no_ending> turned off, C<finish> will run the post-test checks to
enforce the plan and that tests were run. In many cases this will result in
additional events in your events array.

=head1 SEE ALSO

L<Test2::Tools::Intercept> - Accomplish the same thing, but using
blocks instead.

=head1 SOURCE

The source code repository for Test2 can be found at
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
