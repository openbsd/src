package Test2::AsyncSubtest;
use strict;
use warnings;

use Test2::IPC;

our $VERSION = '0.000162';

our @CARP_NOT = qw/Test2::Util::HashBase/;

use Carp qw/croak cluck confess/;
use Test2::Util qw/get_tid CAN_THREAD CAN_FORK/;
use Scalar::Util qw/blessed weaken/;
use List::Util qw/first/;

use Test2::API();
use Test2::API::Context();
use Test2::Util::Trace();
use Test2::Util::Guard();
use Time::HiRes();

use Test2::AsyncSubtest::Hub();
use Test2::AsyncSubtest::Event::Attach();
use Test2::AsyncSubtest::Event::Detach();

use Test2::Util::HashBase qw{
    name hub
    trace frame send_to
    events
    finished
    active
    stack
    id cid uuid
    children
    _in_use
    _attached pid tid
    start_stamp stop_stamp
};

sub CAN_REALLY_THREAD {
    return 0 unless CAN_THREAD;
    return 0 unless eval { require threads; threads->VERSION('1.34'); 1 };
    return 1;
}


my $UUID_VIA = Test2::API::_add_uuid_via_ref();
my $CID = 1;
my @STACK;

sub TOP { @STACK ? $STACK[-1] : undef }

sub init {
    my $self = shift;

    croak "'name' is a required attribute"
        unless $self->{+NAME};

    my $to = $self->{+SEND_TO} ||= Test2::API::test2_stack()->top;

    $self->{+STACK} = [@STACK];
    $_->{+_IN_USE}++ for reverse @STACK;

    $self->{+TID}       = get_tid;
    $self->{+PID}       = $$;
    $self->{+CID}       = 'AsyncSubtest-' . $CID++;
    $self->{+ID}        = 1;
    $self->{+FINISHED}  = 0;
    $self->{+ACTIVE}    = 0;
    $self->{+_IN_USE}   = 0;
    $self->{+CHILDREN}  = [];
    $self->{+UUID} = ${$UUID_VIA}->() if defined $$UUID_VIA;

    unless($self->{+HUB}) {
        my $ipc = Test2::API::test2_ipc();
        my $formatter = Test2::API::test2_stack->top->format;
        my $args = delete $self->{hub_init_args} || {};
        my $hub = Test2::AsyncSubtest::Hub->new(
            %$args,
            ipc       => $ipc,
            nested    => $to->nested + 1,
            buffered  => 1,
            formatter => $formatter,
        );
        weaken($hub->{ast} = $self);
        $self->{+HUB} = $hub;
    }

    $self->{+TRACE} ||= Test2::Util::Trace->new(
        frame    => $self->{+FRAME} || [caller(1)],
        buffered => $to->buffered,
        nested   => $to->nested,
        cid      => $self->{+CID},
        uuid     => $self->{+UUID},
        hid      => $to->hid,
        huuid    => $to->uuid,
    );

    my $hub = $self->{+HUB};
    $hub->set_ast_ids({}) unless $hub->ast_ids;
    $hub->listen($self->_listener);
}

sub _listener {
    my $self = shift;

    my $events = $self->{+EVENTS} ||= [];

    sub { push @$events => $_[1] };
}

sub context {
    my $self = shift;

    my $send_to = $self->{+SEND_TO};

    confess "Attempt to close AsyncSubtest when original parent hub (a non async-subtest?) has ended"
        if $send_to->ended;

    return Test2::API::Context->new(
        trace => $self->{+TRACE},
        hub   => $send_to,
    );
}

sub _gen_event {
    my $self = shift;
    my ($type, $id, $hub) = @_;

    my $class = "Test2::AsyncSubtest::Event::$type";

    return $class->new(
        id    => $id,
        trace => Test2::Util::Trace->new(
            frame    => [caller(1)],
            buffered => $hub->buffered,
            nested   => $hub->nested,
            cid      => $self->{+CID},
            uuid     => $self->{+UUID},
            hid      => $hub->hid,
            huuid    => $hub->uuid,
        ),
    );
}

sub cleave {
    my $self = shift;
    my $id = $self->{+ID}++;
    $self->{+HUB}->ast_ids->{$id} = 0;
    return $id;
}

sub attach {
    my $self = shift;
    my ($id) = @_;

    croak "An ID is required" unless $id;

    croak "ID $id is not valid"
        unless defined $self->{+HUB}->ast_ids->{$id};

    croak "ID $id is already attached"
        if $self->{+HUB}->ast_ids->{$id};

    croak "You must attach INSIDE the child process/thread"
        if $self->{+HUB}->is_local;

    $self->{+_ATTACHED} = [ $$, get_tid, $id ];
    $self->{+HUB}->send($self->_gen_event('Attach', $id, $self->{+HUB}));
}

sub detach {
    my $self = shift;

    if ($self->{+PID} == $$ && $self->{+TID} == get_tid) {
        cluck "You must detach INSIDE the child process/thread ($$, " . get_tid . " instead of $self->{+PID}, $self->{+TID})";
        return;
    }

    my $att = $self->{+_ATTACHED}
        or croak "Not attached";

    croak "Attempt to detach from wrong child"
        unless $att->[0] == $$ && $att->[1] == get_tid;

    my $id = $att->[2];

    $self->{+HUB}->send($self->_gen_event('Detach', $id, $self->{+HUB}));

    delete $self->{+_ATTACHED};
}

sub ready { return !shift->pending }
sub pending {
    my $self = shift;
    my $hub = $self->{+HUB};
    return -1 unless $hub->is_local;

    $hub->cull;

    return $self->{+_IN_USE} + keys %{$self->{+HUB}->ast_ids};
}

sub run {
    my $self = shift;
    my ($code, @args) = @_;

    croak "AsyncSubtest->run() takes a codeblock as the first argument"
        unless $code && ref($code) eq 'CODE';

    $self->start;

    my ($ok, $err, $finished);
    T2_SUBTEST_WRAPPER: {
        $ok = eval { $code->(@args); 1 };
        $err = $@;

        # They might have done 'BEGIN { skip_all => "whatever" }'
        if (!$ok && $err =~ m/Label not found for "last T2_SUBTEST_WRAPPER"/) {
            $ok  = undef;
            $err = undef;
        }
        else {
            $finished = 1;
        }
    }

    $self->stop;

    my $hub = $self->{+HUB};

    if (!$finished) {
        if(my $bailed = $hub->bailed_out) {
            my $ctx = $self->context;
            $ctx->bail($bailed->reason);
            return;
        }
        my $code = $hub->exit_code;
        $ok = !$code;
        $err = "Subtest ended with exit code $code" if $code;
    }

    unless ($ok) {
        my $e = Test2::Event::Exception->new(
            error => $err,
            trace => Test2::Util::Trace->new(
                frame    => [caller(0)],
                buffered => $hub->buffered,
                nested   => $hub->nested,
                cid      => $self->{+CID},
                uuid     => $self->{+UUID},
                hid      => $hub->hid,
                huuid    => $hub->uuid,
            ),
        );
        $hub->send($e);
    }

    return $hub->is_passing;
}

sub start {
    my $self = shift;

    croak "Subtest is already complete"
        if $self->{+FINISHED};

    $self->{+START_STAMP} = Time::HiRes::time() unless defined $self->{+START_STAMP};

    $self->{+ACTIVE}++;

    push @STACK => $self;
    my $hub = $self->{+HUB};
    my $stack = Test2::API::test2_stack();
    $stack->push($hub);

    return $hub->is_passing;
}

sub stop {
    my $self = shift;

    croak "Subtest is not active"
        unless $self->{+ACTIVE}--;

    croak "AsyncSubtest stack mismatch"
        unless @STACK && $self == $STACK[-1];

    $self->{+STOP_STAMP} = Time::HiRes::time();

    pop @STACK;

    my $hub = $self->{+HUB};
    my $stack = Test2::API::test2_stack();
    $stack->pop($hub);
    return $hub->is_passing;
}

sub finish {
    my $self = shift;
    my %params = @_;

    my $hub = $self->hub;

    croak "Subtest is already finished"
        if $self->{+FINISHED}++;

    croak "Subtest can only be finished in the process/thread that created it"
        unless $hub->is_local;

    croak "Subtest is still active"
        if $self->{+ACTIVE};

    $self->wait;
    $self->{+STOP_STAMP} = Time::HiRes::time() unless defined $self->{+STOP_STAMP};
    my $stop_stamp = $self->{+STOP_STAMP};

    my $todo       = $params{todo};
    my $skip       = $params{skip};
    my $empty      = !@{$self->{+EVENTS}};
    my $no_asserts = !$hub->count;
    my $collapse   = $params{collapse};
    my $no_plan    = $params{no_plan} || ($collapse && $no_asserts) || $skip;

    my $trace = Test2::Util::Trace->new(
        frame    => $self->{+TRACE}->{frame},
        buffered => $hub->buffered,
        nested   => $hub->nested,
        cid      => $self->{+CID},
        uuid     => $self->{+UUID},
        hid      => $hub->hid,
        huuid    => $hub->uuid,
    );

    $hub->finalize($trace, !$no_plan)
        unless $hub->no_ending || $hub->ended;

    if ($hub->ipc) {
        $hub->ipc->drop_hub($hub->hid);
        $hub->set_ipc(undef);
    }

    return $hub->is_passing if $params{silent};

    my $ctx = $self->context;

    my $pass = 1;
    if ($skip) {
        $ctx->skip($self->{+NAME}, $skip);
    }
    else {
        if ($collapse && $empty) {
            $ctx->ok($hub->is_passing, $self->{+NAME});
            return $hub->is_passing;
        }

        if ($collapse && $no_asserts) {
            push @{$self->{+EVENTS}} => Test2::Event::Plan->new(trace => $trace, max => 0, directive => 'SKIP', reason => "No assertions");
        }

        my $e = $ctx->build_event(
            'Subtest',
            pass         => $hub->is_passing,
            subtest_id   => $hub->id,
            subtest_uuid => $hub->uuid,
            name         => $self->{+NAME},
            buffered     => 1,
            subevents    => $self->{+EVENTS},
            start_stamp  => $self->{+START_STAMP},
            stop_stamp   => $self->{+STOP_STAMP},
            $todo ? (
                todo => $todo,
                effective_pass => 1,
            ) : (),
        );

        $ctx->hub->send($e);

        unless ($e->effective_pass) {
            $ctx->failure_diag($e);

            $ctx->diag("Bad subtest plan, expected " . $hub->plan . " but ran " . $hub->count)
                if $hub->plan && !$hub->check_plan && !grep {$_->causes_fail} @{$self->{+EVENTS}};
        }

        $pass = $e->pass;
    }

    $_->{+_IN_USE}-- for reverse @{$self->{+STACK}};

    return $pass;
}

sub wait {
    my $self = shift;

    my $hub = $self->{+HUB};
    my $children = $self->{+CHILDREN};

    while (@$children) {
        $hub->cull;
        if (my $child = pop @$children) {
            if (blessed($child)) {
                $child->join;
            }
            else {
                waitpid($child, 0);
            }
        }
        else {
            Time::HiRes::sleep('0.01');
        }
    }

    $hub->cull;

    cluck "Subtest '$self->{+NAME}': All children have completed, but we still appear to be pending"
        if $hub->is_local && keys %{$self->{+HUB}->ast_ids};
}

sub fork {
    croak "Forking is not supported" unless CAN_FORK;
    my $self = shift;
    my $id = $self->cleave;
    my $pid = CORE::fork();

    unless (defined $pid) {
        delete $self->{+HUB}->ast_ids->{$id};
        croak "Failed to fork";
    }

    if($pid) {
        push @{$self->{+CHILDREN}} => $pid;
        return $pid;
    }

    $self->attach($id);

    return $self->_guard;
}

sub run_fork {
    my $self = shift;
    my ($code, @args) = @_;

    my $f = $self->fork;
    return $f unless blessed($f);

    $self->run($code, @args);

    $self->detach();
    $f->dismiss();
    exit 0;
}

sub run_thread {
    croak "Threading is not supported"
        unless CAN_REALLY_THREAD;

    my $self = shift;
    my ($code, @args) = @_;

    my $id = $self->cleave;
    my $thr =  threads->create(sub {
        $self->attach($id);

        $self->run($code, @args);

        $self->detach(get_tid);
        return 0;
    });

    push @{$self->{+CHILDREN}} => $thr;

    return $thr;
}

sub _guard {
    my $self = shift;

    my ($pid, $tid) = ($$, get_tid);

    return Test2::Util::Guard->new(sub {
        return unless $$ == $pid && get_tid == $tid;

        my $error = "Scope Leak";
        if (my $ex = $@) {
            chomp($ex);
            $error .= " ($ex)";
        }

        cluck $error;

        my $e = $self->context->build_event(
            'Exception',
            error => "$error\n",
        );
        $self->{+HUB}->send($e);
        $self->detach();
        exit 255;
    });
}

sub DESTROY {
    my $self = shift;
    return unless $self->{+NAME};

    if (my $att = $self->{+_ATTACHED}) {
        return unless $self->{+HUB};
        eval { $self->detach() };
    }

    return if $self->{+FINISHED};
    return unless $self->{+PID} == $$;
    return unless $self->{+TID} == get_tid;

    local $@;
    eval { $_->{+_IN_USE}-- for reverse @{$self->{+STACK}} };

    warn "Subtest $self->{+NAME} did not finish!";
    exit 255;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::AsyncSubtest - Object representing an async subtest.

=head1 DESCRIPTION

Regular subtests have a limited scope, they start, events are generated, then
they close and send an L<Test2::Event::Subtest> event. This is a problem if you
want the subtest to keep receiving events while other events are also being
generated. This class implements subtests that stay open until you decide to
close them.

This is mainly useful for tools that start a subtest in one process and then
spawn children. In many cases it is nice to let the parent process continue
instead of waiting on the children.

=head1 SYNOPSIS

    use Test2::AsyncSubtest;

    my $ast = Test2::AsyncSubtest->new(name => foo);

    $ast->run(sub {
        ok(1, "Event in parent" );
    });

    ok(1, "Event outside of subtest");

    $ast->run_fork(sub {
        ok(1, "Event in child process");
    });

    ...

    $ast->finish;

    done_testing;

=head1 CONSTRUCTION

    my $ast = Test2::AsyncSubtest->new( ... );

=over 4

=item name => $name (required)

Name of the subtest. This construction argument is required.

=item send_to => $hub (optional)

Hub to which the final subtest event should be sent. This must be an instance
of L<Test2::Hub> or a subclass. If none is specified then the current top hub
will be used.

=item trace => $trace (optional)

File/Line to which errors should be attributed. This must be an instance of
L<Test2::Util::Trace>. If none is specified then the file/line where the
constructor was called will be used.

=item hub => $hub (optional)

Use this to specify a hub the subtest should use. By default a new hub is
generated. This must be an instance of L<Test2::AsyncSubtest::Hub>.

=back

=head1 METHODS

=head2 SIMPLE ACCESSORS

=over 4

=item $bool = $ast->active

True if the subtest is active. The subtest is active if its hub appears in the
global hub stack. This is true when C<< $ast->run(...) >> us running.

=item $arrayref = $ast->children

Get an arrayref of child processes/threads. Numerical items are PIDs, blessed
items are L<threads> instances.

=item $arrayref = $ast->events

Get an arrayref of events that have been sent to the subtests hub.

=item $bool = $ast->finished

True if C<finished()> has already been called.

=item $hub = $ast->hub

The hub created for the subtest.

=item $int = $ast->id

Attach/Detach counter. Used internally, not useful to users.

=item $str = $ast->name

Name of the subtest.

=item $pid = $ast->pid

PID in which the subtest was created.

=item $tid = $ast->tid

Thread ID in which the subtest was created.

=item $hub = $ast->send_to

Hub to which the final subtest event should be sent.

=item $arrayref = $ast->stack

Stack of async subtests at the time this one was created. This is mainly for
internal use.

=item $trace = $ast->trace

L<Test2::Util::Trace> instance used for error reporting.

=back

=head2 INTERFACE

=over 4

=item $ast->attach($id)

Attach a subtest in a child/process to the original.

B<Note:> C<< my $id = $ast->cleave >> must have been called in the parent
process/thread before the child was started, the id it returns must be used in
the call to C<< $ast->attach($id) >>

=item $id = $ast->cleave

Prepare a slot for a child process/thread to attach. This must be called BEFORE
the child process or thread is started. The ID returned is used by C<attach()>.

This must only be called in the original process/thread.

=item $ctx = $ast->context

Get an L<Test2::API::Context> instance that can be used to send events to the
context in which the hub was created. This is not a canonical context, you
should not call C<< $ctx->release >> on it.

=item $ast->detach

Detach from the parent in a child process/thread. This should be called just
before the child exits.

=item $ast->finish

=item $ast->finish(%options)

Finish the subtest, wait on children, and send the final subtest event.

This must only be called in the original process/thread.

B<Note:> This calls C<< $ast->wait >>.

These are the options:

=over 4

=item collapse => 1

This intelligently allows a subtest to be empty.

If no events bump the test count then the subtest no final plan will be added.
The subtest will not be considered a failure (normally an empty subtest is a
failure).

If there are no events at all the subtest will be collapsed into an
L<Test2::Event::Ok> event.

=item silent => 1

This will prevent finish from generating a final L<Test2::Event::Subtest>
event. This effectively ends the subtest without it effecting the parent
subtest (or top level test).

=item no_plan => 1

This will prevent a final plan from being added to the subtest for you when
none is directly specified.

=item skip => "reason"

This will issue an L<Test2::Event::Skip> instead of a subtest. This will throw
an exception if any events have been seen, or if state implies events have
occurred.

=back

=item $out = $ast->fork

This is a slightly higher level interface to fork. Running it will fork your
code in-place just like C<fork()>. It will return a pid in the parent, and an
L<Test2::Util::Guard> instance in the child. An exception will be thrown if
fork fails.

It is recommended that you use C<< $ast->run_fork(sub { ... }) >> instead.

=item $bool = $ast->pending

True if there are child processes, threads, or subtests that depend on this
one.

=item $bool = $ast->ready

This is essentially C<< !$ast->pending >>.

=item $ast->run(sub { ... })

Run the provided codeblock inside the subtest. This will push the subtest hub
onto the stack, run the code, then pop the hub off the stack.

=item $pid = $ast->run_fork(sub { ... })

Same as C<< $ast->run() >>, except that the codeblock is run in a child
process.

You do not need to directly call C<wait($pid)>, that will be done for you when
C<< $ast->wait >>, or C<< $ast->finish >> are called.

=item my $thr = $ast->run_thread(sub { ... });

B<** DISCOURAGED **> Threads cause problems. This method remains for anyone who
REALLY wants it, but it is no longer supported. Tests for this functionality do
not even run unless the AUTHOR_TESTING or T2_DO_THREAD_TESTS env vars are
enabled.

Same as C<< $ast->run() >>, except that the codeblock is run in a child
thread.

You do not need to directly call C<< $thr->join >>, that is done for you when
C<< $ast->wait >>, or C<< $ast->finish >> are called.

=item $passing = $ast->start

Push the subtest hub onto the stack. Returns the current pass/fail status of
the subtest.

=item $ast->stop

Pop the subtest hub off the stack. Returns the current pass/fail status of the
subtest.

=item $ast->wait

Wait on all threads/processes that were started using C<< $ast->fork >>,
C<< $ast->run_fork >>, or C<< $ast->run_thread >>.

=back

=head1 SOURCE

The source code repository for Test2-AsyncSubtest can be found at
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

Copyright 2018 Chad Granum E<lt>exodist7@gmail.comE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
