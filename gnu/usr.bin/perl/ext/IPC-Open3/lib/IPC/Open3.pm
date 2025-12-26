package IPC::Open3;

use strict;
no strict 'refs'; # because users pass me bareword filehandles

use Exporter 'import';

use Carp;
use Symbol qw(gensym qualify);

our $VERSION = '1.24';
our @EXPORT  = qw(open3);

=head1 NAME

IPC::Open3 - open a process for reading, writing, and error handling using open3()

=head1 SYNOPSIS

    use Symbol 'gensym'; # vivify a separate handle for STDERR
    my $pid = open3(my $chld_in, my $chld_out, my $chld_err = gensym,
		    'some', 'cmd', 'and', 'args');
    # or pass the command through the shell
    my $pid = open3(my $chld_in, my $chld_out, my $chld_err = gensym,
		    'some cmd and args');

    # read from parent STDIN
    # send STDOUT and STDERR to already open handle
    open my $outfile, '>>', 'output.txt' or die "open failed: $!";
    my $pid = open3(['&', *STDIN], ['&', $outfile], undef,
		    'some', 'cmd', 'and', 'args');

    # write to parent STDOUT and STDERR
    my $pid = open3(my $chld_in, ['&', *STDOUT], ['&', *STDERR],
		    'some', 'cmd', 'and', 'args');

    # reap zombie and retrieve exit status
    waitpid( $pid, 0 );
    my $child_exit_status = $? >> 8;

=head1 DESCRIPTION

Extremely similar to C<open2> from L<IPC::Open2>, C<open3> spawns the given
command and provides filehandles for interacting with the command's standard
I/O streams.

    my $pid = open3($chld_in, $chld_out, $chld_err, @command_and_args);

It connects C<$chld_in> for writing to the child's standard input, C<$chld_out>
for reading from the child's standard output, and C<$chld_err> for reading from
the child's standard error stream.  If C<$chld_err> is false, or the same file
descriptor as C<$chld_out>, then C<STDOUT> and C<STDERR> of the child are on
the same filehandle.  This means that you cannot pass an uninitialized variable
for C<$chld_err> and have C<open3> auto-generate a filehandle for you, but
gensym from L<Symbol> can be used to vivify a new glob reference; see
L</SYNOPSIS>.  The C<$chld_in> handle will have autoflush turned on.

By default, the filehandles you pass in are used as output parameters.
C<open3> internally creates three pipes.  The write end of the first pipe and
the read ends of the other pipes are connected to the command's standard
input/output/error, respectively.  The corresponding read and write ends are
placed in the first three argument to C<open3>.

The filehandle arguments can take the following forms:

=over

=item *

An uninitialized variable (technically, either C<undef> or the empty string
will work):  C<open3> generates a fresh filehandle and assigns it to the
argument, which must be a modifiable variable for this work (otherwise an
exception will be raised).

This does not work for C<$chld_err>, however:  If the C<$chld_err> argument is
a false value, the child's error stream is automatically redirected to its
standard output.

=item *

An existing handle in the form of a typeglob like C<*STDIN> or C<*FOO> or a
reference to such:  C<open3> places the filehandle in the C<IO> slot of the
typeglob, which means the corresponding bareword filehandle (like C<STDIN> or
C<FOO>) can be used for I/O from/to the child process.  (If the handle is
already open, it is automatically closed first.)

=item *

A string containing the name of a bareword handle (like C<'STDIN'> or
C<'FOO'>):  Such strings are resolved to typeglobs at runtime and then act like
the case described above.

=back

However, it is possible to make C<open3> use an existing handle directly (as an
input argument) and skip the creation of a pipe.  To do this, the filehandle
argument must have one of the following two forms:

=over

=item *

An array reference like C<['&', $fh]>, i.e. the first element is the string
C<'&'> and the second element is the existing handle to use in the child
process.

=item *

A string of the form C<< '<&FOO' >> or C<< '>&FOO' >>, i.e. a string starting
with the two characters C<< <& >> (for input) or C<< >& >> (for output),
followed by the name of a bareword filehandle.  (The string form cannot be used
with handles stored in variables.)

=back

If you use this form for C<$chld_in>, the filehandle will be closed in the
parent process.

The filehandles may also be integers, in which case they are understood
as file descriptors.

C<open3> returns the process ID of the child process.  It doesn't return on
failure: it just raises an exception matching C</^open3:/>.  However,
C<exec> failures in the child (such as no such file or permission denied),
are just reported to C<$chld_err> under Windows and OS/2, as it is not possible
to trap them.

If the child process dies for any reason, the next write to C<$chld_in> is
likely to generate a SIGPIPE in the parent, which is fatal by default,
So you may wish to handle this signal.

Note: if you specify C<-> as the command, in an analogous fashion to
C<open(my $fh, "-|")> the child process will just be the forked Perl
process rather than an external command.  This feature isn't yet
supported on Win32 platforms.

C<open3> does not wait for and reap the child process after it exits.
Except for short programs where it's acceptable to let the operating system
take care of this, you need to do this yourself.  This is normally as
simple as calling C<waitpid $pid, 0> when you're done with the process.
Failing to do this can result in an accumulation of defunct or "zombie"
processes.  See L<perlfunc/waitpid> for more information.

If you try to read from the child's stdout writer and their stderr
writer, you'll have problems with blocking, which means you'll want
to use C<select> or L<IO::Select>, which means you'd best use
C<sysread> instead of C<readline> for normal stuff.

This is very dangerous, as you may block forever.  C<open3> assumes it's
going to talk to something like L<bc(1)>, both writing to it and reading
from it.  This is presumably safe because you "know" that commands
like L<bc(1)> will read a line at a time and output a line at a time.
Programs like L<sort(1)> that read their entire input stream first,
however, are quite apt to cause deadlock.

The big problem with this approach is that if you don't have control
over source code being run in the child process, you can't control
what it does with pipe buffering.  Thus you can't just open a pipe to
C<cat -v> and continually read and write a line from it.

=head1 See Also

=over 4

=item L<IPC::Open2>

Like L<IPC::Open3> but without C<STDERR> capture.

=item L<IPC::Run>

This is a CPAN module that has better error handling and more facilities
than L<IPC::Open3>.

=back

=head1 WARNING

The order of arguments differs from that of C<open2>.

=cut

# &open3: Marc Horowitz <marc@mit.edu>
# derived mostly from &open2 by tom christiansen, <tchrist@convex.com>
# fixed for 5.001 by Ulrich Kunitz <kunitz@mai-koeln.com>
# ported to Win32 by Ron Schmidt, Merrill Lynch almost ended my career
# fixed for autovivving FHs, tchrist again
# allow fd numbers to be used, by Frank Tobin
# allow '-' as command (c.f. open "-|"), by Adam Spiers <perl@adamspiers.org>
#
# usage: $pid = open3('wtr', 'rdr', 'err' 'some cmd and args', 'optarg', ...);
#
# spawn the given $cmd and connect rdr for
# reading, wtr for writing, and err for errors.
# if err is '', or the same as rdr, then stdout and
# stderr of the child are on the same fh.  returns pid
# of child (or dies on failure).


# if wtr begins with '<&', then wtr will be closed in the parent, and
# the child will read from it directly.  if rdr or err begins with
# '>&', then the child will send output directly to that fd.  In both
# cases, there will be a dup() instead of a pipe() made.


# WARNING: this is dangerous, as you may block forever
# unless you are very careful.
#
# $wtr is left unbuffered.
#
# abort program if
#   rdr or wtr are null
#   a system call fails

our $Me = 'open3 (bug)';	# you should never see this, it's always localized

# Fatal.pm needs to be fixed WRT prototypes.

sub xpipe {
    pipe $_[0], $_[1] or croak "$Me: pipe($_[0], $_[1]) failed: $!";
}

# I tried using a * prototype character for the filehandle but it still
# disallows a bareword while compiling under strict subs.

sub xopen {
    open $_[0], $_[1], @_[2..$#_] and return;
    local $" = ', ';
    carp "$Me: open(@_) failed: $!";
}

sub xclose {
    $_[0] =~ /\A=?(\d+)\z/
	? do { my $fh; open($fh, $_[1] . '&=' . $1) and close($fh); }
	: close $_[0]
	or croak "$Me: close($_[0]) failed: $!";
}

sub xfileno {
    return $1 if $_[0] =~ /\A=?(\d+)\z/;  # deal with fh just being an fd
    return fileno $_[0];
}

use constant FORCE_DEBUG_SPAWN => 0;
use constant DO_SPAWN => $^O eq 'os2' || $^O eq 'MSWin32' || FORCE_DEBUG_SPAWN;

sub _open3 {
    local $Me = shift;

    # simulate autovivification of filehandles because
    # it's too ugly to use @_ throughout to make perl do it for us
    # tchrist 5-Mar-00

    # Historically, open3(undef...) has silently worked, so keep
    # it working.
    splice @_, 0, 1, undef if \$_[0] == \undef;
    splice @_, 1, 1, undef if \$_[1] == \undef;
    unless (eval  {
	$_[0] = gensym unless defined $_[0] && length $_[0];
	$_[1] = gensym unless defined $_[1] && length $_[1];
	1; })
    {
	# must strip crud for croak to add back, or looks ugly
	$@ =~ s/(?<=value attempted) at .*//s;
	croak "$Me: $@";
    }

    my @handles = ({ mode => '<', handle => \*STDIN },
		   { mode => '>', handle => \*STDOUT },
		   { mode => '>', handle => \*STDERR },
		  );

    foreach (@handles) {
	$_->{parent} = shift;
	$_->{open_as} = gensym;
    }

    if (@_ > 1 and $_[0] eq '-') {
	croak "Arguments don't make sense when the command is '-'"
    }

    $handles[2]{parent} ||= $handles[1]{parent};
    $handles[2]{dup_of_out} = $handles[1]{parent} eq $handles[2]{parent};

    my $package;
    foreach (@handles) {
        if (ref($_->{parent}) eq 'ARRAY') {
            if ($_->{parent}[0] eq '&') {
                $_->{dup} = 1;
                $_->{parent} = $_->{parent}[1];
            } else {
                croak "$Me: Invalid dup mode: $_->{parent}[0]";
            }
        } else {
            $_->{dup} = ($_->{parent} =~ s/^[<>]&//);

            if ($_->{parent} !~ /\A=?(\d+)\z/) {
                # force unqualified filehandles into caller's package
                $package //= caller 1;
                $_->{parent} = qualify $_->{parent}, $package;
            }

            next if $_->{dup} or $_->{dup_of_out};
            if ($_->{mode} eq '<') {
                xpipe $_->{open_as}, $_->{parent};
            } else {
                xpipe $_->{parent}, $_->{open_as};
            }
        }
    }

    my $kidpid;
    if (!DO_SPAWN) {
	# Used to communicate exec failures.
	xpipe my $stat_r, my $stat_w;

	$kidpid = fork;
	croak "$Me: fork failed: $!" unless defined $kidpid;
	if ($kidpid == 0) {  # Kid
	    eval {
		# A tie in the parent should not be allowed to cause problems.
		untie *STDIN;
		untie *STDOUT;
		untie *STDERR;

		close $stat_r;
		require Fcntl;
		my $flags = fcntl $stat_w, &Fcntl::F_GETFD, 0;
		croak "$Me: fcntl failed: $!" unless $flags;
		fcntl $stat_w, &Fcntl::F_SETFD, $flags|&Fcntl::FD_CLOEXEC
		    or croak "$Me: fcntl failed: $!";

		# If she wants to dup the kid's stderr onto her stdout I need to
		# save a copy of her stdout before I put something else there.
		if (!$handles[2]{dup_of_out} && $handles[2]{dup}
			&& xfileno($handles[2]{parent}) == fileno \*STDOUT) {
		    my $tmp = gensym;
		    xopen($tmp, '>&', $handles[2]{parent});
		    $handles[2]{parent} = $tmp;
		}

		foreach (@handles) {
		    if ($_->{dup_of_out}) {
			xopen \*STDERR, '>&', *STDOUT
			    if defined fileno STDERR && fileno STDERR != fileno STDOUT;
		    } elsif ($_->{dup}) {
			xopen $_->{handle}, $_->{mode} . '&', $_->{parent}
			    if fileno $_->{handle} != xfileno($_->{parent});
		    } else {
			xclose $_->{parent}, $_->{mode};
			xopen $_->{handle}, $_->{mode} . '&=',
			    fileno $_->{open_as};
		    }
		}
		return 1 if ($_[0] eq '-');
		exec @_ or do {
		    local($")=(" ");
		    croak "$Me: exec of @_ failed: $!";
		};
	    } and do {
                close $stat_w;
                return 0;
            };

	    my $bang = 0+$!;
	    my $err = $@;
	    utf8::encode $err if $] >= 5.008;
	    print $stat_w pack('IIa*', $bang, length($err), $err);
	    close $stat_w;

	    eval { require POSIX; POSIX::_exit(255); };
	    exit 255;
	}
	else {  # Parent
	    close $stat_w;
	    my $to_read = length(pack('I', 0)) * 2;
	    my $bytes_read = read($stat_r, my $buf = '', $to_read);
	    if ($bytes_read) {
		(my $bang, $to_read) = unpack('II', $buf);
		read($stat_r, my $err = '', $to_read);
		waitpid $kidpid, 0; # Reap child which should have exited
		if ($err) {
		    utf8::decode $err if $] >= 5.008;
		} else {
		    $err = "$Me: " . ($! = $bang);
		}
		$! = $bang;
		die($err);
	    }
	}
    }
    else {  # DO_SPAWN
	# All the bookkeeping of coincidence between handles is
	# handled in spawn_with_handles.

	my @close;

	foreach (@handles) {
	    if ($_->{dup_of_out}) {
		$_->{open_as} = $handles[1]{open_as};
	    } elsif ($_->{dup}) {
		$_->{open_as} = $_->{parent} =~ /\A[0-9]+\z/
		    ? $_->{parent} : \*{$_->{parent}};
		push @close, $_->{open_as};
	    } else {
		push @close, \*{$_->{parent}}, $_->{open_as};
	    }
	}
	require IO::Pipe;
	$kidpid = eval {
	    spawn_with_handles(\@handles, \@close, @_);
	};
	die "$Me: $@" if $@;
    }

    foreach (@handles) {
	next if $_->{dup} or $_->{dup_of_out};
	xclose $_->{open_as}, $_->{mode};
    }

    # If the write handle is a dup give it away entirely, close my copy
    # of it.
    xclose $handles[0]{parent}, $handles[0]{mode} if $handles[0]{dup};

    select((select($handles[0]{parent}), $| = 1)[0]); # unbuffer pipe
    $kidpid;
}

sub open3 {
    if (@_ < 4) {
	local $" = ', ';
	croak "open3(@_): not enough arguments";
    }
    return _open3 'open3', @_
}

sub spawn_with_handles {
    my $fds = shift;		# Fields: handle, mode, open_as
    my $close_in_child = shift;
    my ($fd, %saved, @errs);

    foreach $fd (@$fds) {
	$fd->{tmp_copy} = IO::Handle->new_from_fd($fd->{handle}, $fd->{mode});
	$saved{fileno $fd->{handle}} = $fd->{tmp_copy} if $fd->{tmp_copy};
    }
    foreach $fd (@$fds) {
	bless $fd->{handle}, 'IO::Handle'
	    unless eval { $fd->{handle}->isa('IO::Handle') } ;
	# If some of handles to redirect-to coincide with handles to
	# redirect, we need to use saved variants:
    my $open_as = $fd->{open_as};
    my $fileno = fileno($open_as);
    $fd->{handle}->fdopen(defined($fileno)
                  ? $saved{$fileno} || $open_as
                  : $open_as,
                  $fd->{mode});
    }
    unless ($^O eq 'MSWin32') {
	require Fcntl;
	# Stderr may be redirected below, so we save the err text:
	foreach $fd (@$close_in_child) {
	    next unless fileno $fd;
	    fcntl($fd, Fcntl::F_SETFD(), 1) or push @errs, "fcntl $fd: $!"
		unless $saved{fileno $fd}; # Do not close what we redirect!
	}
    }

    my $pid;
    unless (@errs) {
	if (FORCE_DEBUG_SPAWN) {
	    pipe my $r, my $w or die "Pipe failed: $!";
	    $pid = fork;
	    die "Fork failed: $!" unless defined $pid;
	    if (!$pid) {
		{ no warnings; exec @_ }
		print $w 0 + $!;
		close $w;
		require POSIX;
		POSIX::_exit(255);
	    }
	    close $w;
	    my $bad = <$r>;
	    if (defined $bad) {
		$! = $bad;
		undef $pid;
	    }
	} else {
	    $pid = eval { system 1, @_ }; # 1 == P_NOWAIT
	}
	if($@) {
	    push @errs, "IO::Pipe: Can't spawn-NOWAIT: $@";
	} elsif(!$pid || $pid < 0) {
	    push @errs, "IO::Pipe: Can't spawn-NOWAIT: $!";
	}
    }

    # Do this in reverse, so that STDERR is restored first:
    foreach $fd (reverse @$fds) {
	$fd->{handle}->fdopen($fd->{tmp_copy}, $fd->{mode});
    }
    foreach (values %saved) {
	$_->close or croak "Can't close: $!";
    }
    croak join "\n", @errs if @errs;
    return $pid;
}

1; # so require is happy
