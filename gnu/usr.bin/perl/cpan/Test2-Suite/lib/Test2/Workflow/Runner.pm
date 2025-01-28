package Test2::Workflow::Runner;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::API();
use Test2::Todo();
use Test2::AsyncSubtest();

use Test2::Util qw/get_tid CAN_REALLY_FORK/;

use Scalar::Util qw/blessed/;
use Time::HiRes qw/sleep/;
use List::Util qw/shuffle min/;
use Carp qw/confess/;

use Test2::Util::HashBase qw{
    stack no_fork no_threads max slots pid tid rand subtests filter
};

use overload(
    'fallback' => 1,
    '&{}' => sub {
        my $self = shift;

        sub {
            @_ = ($self);
            goto &run;
        }
    },
);

sub init {
    my $self = shift;

    $self->{+STACK}    = [];
    $self->{+SUBTESTS} = [];

    $self->{+PID} = $$;
    $self->{+TID} = get_tid();

    $self->{+NO_FORK} ||= $ENV{T2_WORKFLOW_NO_FORK} || !CAN_REALLY_FORK();

    my $can_thread = Test2::AsyncSubtest->CAN_REALLY_THREAD();
    my $should_thread = ($ENV{T2_WORKFLOW_USE_THREADS} || $ENV{T2_DO_THREAD_TESTS}) && !$ENV{T2_WORKFLOW_NO_THREADS};
    $self->{+NO_THREADS} ||= !($can_thread && $should_thread);

    $self->{+RAND} = 1 unless defined $self->{+RAND};

    my @max = grep {defined $_} $self->{+MAX}, $ENV{T2_WORKFLOW_ASYNC};
    my $max = @max ? min(@max) : 3;
    $self->{+MAX} = $max;
    $self->{+SLOTS} = [] if $max;

    unless(defined($self->{+FILTER})) {
        if (my $raw = $ENV{T2_WORKFLOW}) {
            my ($file, $line, $name);
            if ($raw =~ m/^(.*)\s+(\d+)$/) {
                ($file, $line) = ($1, $2);
            }
            elsif($raw =~ m/^(\d+)$/) {
                $line = $1;
            }
            else {
                $name = $raw;
            }

            $self->{+FILTER} = {
                file => $file,
                line => $line,
                name => $name,
            };
        }
    }

    if (my $task = delete $self->{task}) {
        $self->push_task($task);
    }
}

sub is_local {
    my $self = shift;
    return 0 unless $self->{+PID} == $$;
    return 0 unless $self->{+TID} == get_tid();
    return 1;
}

sub send_event {
    my $self = shift;
    my ($type, %params) = @_;

    my $class;
    if ($type =~ m/\+(.*)$/) {
        $class = $1;
    }
    else {
        $class = "Test2::Event::$type";
    }

    my $hub = Test2::API::test2_stack()->top();

    my $e = $class->new(
        trace => Test2::Util::Trace->new(
            frame => [caller(0)],
            buffered => $hub->buffered,
            nested   => $hub->nested,
            hid      => $hub->hid,
            huuid    => $hub->uuid,
            #cid      => $self->{+CID},
            #uuid     => $self->{+UUID},
        ),

        %params,
    );

    $hub->send($e);
}

sub current_subtest {
    my $self = shift;
    my $stack = $self->{+STACK} or return undef;

    for my $state (reverse @$stack) {
        next unless $state->{subtest};
        return $state->{subtest};
    }

    return undef;
}

sub run {
    my $self = shift;

    my $stack = $self->stack;

    my $c = 0;
    while (@$stack) {
        $self->cull;

        my $state  = $stack->[-1];
        my $task   = $state->{task};

        unless($state->{started}++) {
            my $skip = $task->skip;

            my $filter;
            if (my $f = $self->{+FILTER}) {
                my $in_var = grep { $_->{filter_satisfied} } @$stack;

                $filter = $task->filter($f) unless $in_var;
                $state->{filter_satisfied} = 1 if $filter->{satisfied};
            }

            $skip ||= $filter->{skip} if $filter;

            if ($skip) {
                $state->{ended}++;
                $self->send_event(
                    'Skip',
                    reason         => $skip || $filter,
                    name           => $task->name,
                    pass           => 1,
                    effective_pass => 1,
                );
                pop @$stack;
                next;
            }

            if ($task->flat) {
                my $st = $self->current_subtest;
                my $hub = $st ? $st->hub : Test2::API::test2_stack->top;

                $state->{todo} = Test2::Todo->new(reason => $task->todo, hub => $hub)
                    if $task->todo;

                $hub->send($_) for @{$task->events};
            }
            else {
                my $st = Test2::AsyncSubtest->new(
                    name  => $task->name,
                    frame => $task->frame,
                );
                $state->{subtest} = $st;

                $state->{todo} = Test2::Todo->new(reason => $task->todo, hub => $st->hub)
                    if $task->todo;

                for my $e (@{$task->events}) {
                    my $hub = $st->hub;

                    $e->trace->{buffered} = $hub->buffered;
                    $e->trace->{nested}   = $hub->nested;
                    $e->trace->{hid}      = $hub->hid;
                    $e->trace->{huuid}    = $hub->uuid;

                    $hub->send($e);
                }

                my $slot = $self->isolate($state);

                # if we forked/threaded then this state has ended here.
                if (defined($slot)) {
                    push @{$self->{+SUBTESTS}} => [$st, $task] unless $st->finished;
                    $state->{subtest} = undef;
                    $state->{ended} = 1;
                }
            }
        }

        if ($state->{ended}) {
            $state->{todo}->end() if $state->{todo};
            $state->{subtest}->stop() if $state->{subtest};

            return if $state->{in_thread};
            if(my $guard = delete $state->{in_fork}) {
                $state->{subtest}->detach;
                $guard->dismiss;
                exit 0;
            }

            pop @$stack;
            next;
        }

        if($state->{subtest} && !$state->{subtest_started}++) {
            push @{$self->{+SUBTESTS}} => [$state->{subtest}, $task];
            $state->{subtest}->start();
        }

        if ($task->isa('Test2::Workflow::Task::Action')) {
            $state->{PID} = $$;
            my $ok = eval { $task->code->($self); 1 };

            unless ($state->{PID} == $$) {
                print STDERR "Task '" . $task->name . "' started in pid $state->{PID}, but ended in pid $$, did you forget to exit after forking?\n";
                exit 255;
            }

            $task->exception($@) unless $ok;
            $state->{ended} = 1;

            next;
        }

        if (!$state->{stage} || $state->{stage} eq 'BEFORE') {
            $state->{before}  = (defined $state->{before}) ? $state->{before} : 0;

            if (my $add = $task->before->[$state->{before}++]) {
                if ($add->around) {
                    $state->{PID} = $$;
                    my $ok = eval { $add->code->($self); 1 };
                    my $err = $@;
                    my $complete = $state->{stage} && $state->{stage} eq 'AFTER';

                    unless ($state->{PID} == $$) {
                        print STDERR "Task '" . $task->name . "' started in pid $state->{PID}, but ended in pid $$, did you forget to exit after forking?\n";
                        exit 255;
                    }

                    unless($ok && $complete) {
                        $state->{ended} = 1;
                        $state->{stage} = 'AFTER';
                        $task->exception($ok ? "'around' task failed to continue into the workflow chain.\n" : $err);
                    }
                }
                else {
                    $self->push_task($add);
                }
            }
            else {
                $state->{stage} = 'VARIANT';
            }
        }
        elsif ($state->{stage} eq 'VARIANT') {
            if (my $v = $task->variant) {
                $self->push_task($v);
            }
            $state->{stage} = 'PRIMARY';
        }
        elsif ($state->{stage} eq 'PRIMARY') {
            unless (defined $state->{order}) {
                my $rand = defined($task->rand) ? $task->rand : $self->rand;
                $state->{order} = [0 .. scalar(@{$task->primary}) - 1];
                @{$state->{order}} = shuffle(@{$state->{order}})
                    if $rand;
            }
            my $num = shift @{$state->{order}};
            if (defined $num) {
                $self->push_task($task->primary->[$num]);
            }
            else {
                $state->{stage} = 'AFTER';
            }
        }
        elsif ($state->{stage} eq 'AFTER') {
            $state->{after}  = (defined $state->{after}) ? $state->{after} : 0;
            if (my $add = $task->after->[$state->{after}++]) {
                return if $add->around;
                $self->push_task($add);
            }
            else {
                $state->{ended} = 1;
            }
        }
    }

    $self->finish;
}

sub push_task {
    my $self = shift;
    my ($task) = @_;

    confess "No Task!" unless $task;
    confess "Bad Task ($task)!" unless blessed($task) && $task->isa('Test2::Workflow::Task');

    if ($task->isa('Test2::Workflow::Build')) {
        confess "Can only push a Build instance when initializing the stack"
            if @{$self->{+STACK}};
        $task = $task->compile();
    }

    push @{$self->{+STACK}} => {
        task => $task,
        name => $task->name,
    };
}

sub add_mock {
    my $self = shift;
    my ($mock) = @_;
    my $stack = $self->{+STACK};

    confess "Nothing on the stack!"
        unless $stack && @$stack;

    my ($state) = grep { !$_->{task}->scaffold} reverse @$stack;
    push @{$state->{mocks}} => $mock;
}

sub isolate {
    my $self = shift;
    my ($state) = @_;

    return if $state->{task}->skip;

    my $iso   = $state->{task}->iso;
    my $async = $state->{task}->async;

    # No need to isolate
    return undef unless $iso || $async;

    # Cannot isolate
    unless($self->{+MAX} && $self->is_local) {
        # async does not NEED to be isolated
        return undef unless $iso;
    }

    # Wait for a slot, if max is set to 0 then we will not find a slot, instead
    # we use '0'.  We need to return a defined value to let the stack know that
    # the task has ended.
    my $slot = 0;
    while($self->{+MAX} && $self->is_local) {
        $self->cull;
        for my $s (1 .. $self->{+MAX}) {
            my $st = $self->{+SLOTS}->[$s];
            next if $st && !$st->finished;
            $self->{+SLOTS}->[$s] = undef;
            $slot = $s;
            last;
        }
        last if $slot;
        sleep(0.02);
    }

    my $st = $state->{subtest}
        or confess "Cannot isolate a task without a subtest";

    if (!$self->no_fork) {
        my $out = $st->fork;
        if (blessed($out)) {
            $state->{in_fork} = $out;

            # drop back out to complete the task.
            return undef;
        }
        else {
            $self->send_event(
                'Note',
                message => "Forked PID $out to run: " . $state->{task}->name,
            );
            $state->{pid} = $out;
        }
    }
    elsif (!$self->no_threads) {
        $state->{in_thread} = 1;
        my $thr = $st->run_thread(\&run, $self);
        $state->{thread} = $thr;
        delete $state->{in_thread};
        $self->send_event(
            'Note',
            message => "Started Thread-ID " . $thr->tid . " to run: " . $state->{task}->name,
        );
    }
    else {
        $st->finish(skip => "No isolation method available");
        return 0;
    }

    if($slot) {
        $self->{+SLOTS}->[$slot] = $st;
    }
    else {
        $st->finish;
    }

    return $slot;
}

sub cull {
    my $self = shift;

    my $subtests = delete $self->{+SUBTESTS} || return;
    my @new;

    # Cull subtests in reverse order, Nested subtests end before their parents.
    for my $set (reverse @$subtests) {
        my ($st, $task) = @$set;
        next if $st->finished;
        if (!$st->active && $st->ready) {
            $st->finish();
            next;
        }

        # Use unshift to preserve order.
        unshift @new => $set;
    }

    $self->{+SUBTESTS} = \@new;

    return;
}

sub finish {
    my $self = shift;
    while(@{$self->{+SUBTESTS}}) {
        $self->cull;
        sleep(0.02) if @{$self->{+SUBTESTS}};
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Workflow::Runner - Runs the workflows.

=head1 SOURCE

The source code repository for Test2-Workflow can be found at
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

