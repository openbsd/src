package IPC::Open2;

use strict;

require 5.006;
use Exporter 'import';

our $VERSION = 1.08;
our @EXPORT  = qw(open2);

=head1 NAME

IPC::Open2 - open a process for both reading and writing using open2()

=head1 SYNOPSIS

    use IPC::Open2;

    my $pid = open2(my $chld_out, my $chld_in,
      'some', 'cmd', 'and', 'args');
    # or passing the command through the shell
    my $pid = open2(my $chld_out, my $chld_in, 'some cmd and args');

    # read from parent STDIN and write to already open handle
    open my $outfile, '>', 'outfile.txt' or die "open failed: $!";
    my $pid = open2(['&', $outfile], ['&', *STDIN],
      'some', 'cmd', 'and', 'args');

    # read from already open handle and write to parent STDOUT
    open my $infile, '<', 'infile.txt' or die "open failed: $!";
    my $pid = open2(['&', *STDOUT], ['&', $infile],
      'some', 'cmd', 'and', 'args');

    # reap zombie and retrieve exit status
    waitpid( $pid, 0 );
    my $child_exit_status = $? >> 8;

=head1 DESCRIPTION

The C<open2()> function runs the given command and connects C<$chld_out> for
reading and C<$chld_in> for writing.  It's what you think should work
when you try

    my $pid = open(my $fh, "|cmd args|");  # ERROR

but you have to write it as:

    my $pid = open2($chld_out, $chld_in, @command_and_args);

The C<$chld_in> filehandle will have autoflush turned on.

By default, the filehandles you pass in are used as output parameters.
C<open2> internally creates two pipes.  The write end of the first pipe and the
read end of the second pipe are connected to the command's standard output and
input, respectively.  The corresponding read and write ends are placed in the
first and second argument to C<open2>.

The filehandle arguments can take the following forms:

=over

=item *

An uninitialized variable (technically, either C<undef> or the empty string
will work):  C<open2> generates a fresh filehandle and assigns it to the
argument, which must be a modifiable variable for this work (otherwise an
exception will be raised).

=item *

An existing handle in the form of a typeglob like C<*STDIN> or C<*FOO> or a
reference to such:  C<open2> places the filehandle in the C<IO> slot of the
typeglob, which means the corresponding bareword filehandle (like C<STDIN> or
C<FOO>) can be used for I/O from/to the child process.  (If the handle is
already open, it is automatically closed first.)

=item *

A string containing the name of a bareword handle (like C<'STDIN'> or
C<'FOO'>):  Such strings are resolved to typeglobs at runtime and then act like
the case described above.

=back

However, it is possible to make C<open2> use an existing handle directly (as an
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

C<open2> returns the process ID of the child process.  It doesn't return on
failure: it just raises an exception matching C</^open2:/>.  However,
C<exec> failures in the child are not detected.  You'll have to
trap SIGPIPE yourself.

C<open2> does not wait for and reap the child process after it exits.
Except for short programs where it's acceptable to let the operating system
take care of this, you need to do this yourself.  This is normally as
simple as calling C<waitpid $pid, 0> when you're done with the process.
Failing to do this can result in an accumulation of defunct or "zombie"
processes.  See L<perlfunc/waitpid> for more information.

This whole affair is quite dangerous, as you may block forever.  It
assumes it's going to talk to something like L<bc(1)>, both writing
to it and reading from it.  This is presumably safe because you
"know" that commands like L<bc(1)> will read a line at a time and
output a line at a time.  Programs like L<sort(1)> that read their
entire input stream first, however, are quite apt to cause deadlock.

The big problem with this approach is that if you don't have control 
over source code being run in the child process, you can't control
what it does with pipe buffering.  Thus you can't just open a pipe to
C<cat -v> and continually read and write a line from it.

The L<IO::Pty> and L<Expect> modules from CPAN can help with this, as
they provide a real tty (well, a pseudo-tty, actually), which gets you
back to line buffering in the invoked command again.

=head1 WARNING 

The order of arguments differs from that of C<open3> from L<IPC::Open3>.

=head1 SEE ALSO

See L<IPC::Open3> for an alternative that handles C<STDERR> as well.  This
function is really just a wrapper around C<open3>.

=cut

# &open2: tom christiansen, <tchrist@convex.com>
#
# usage: $pid = open2('rdr', 'wtr', 'some cmd and args');
#    or  $pid = open2('rdr', 'wtr', 'some', 'cmd', 'and', 'args');
#
# spawn the given $cmd and connect $rdr for
# reading and $wtr for writing.  return pid
# of child, or 0 on failure.  
# 
# WARNING: this is dangerous, as you may block forever
# unless you are very careful.  
# 
# $wtr is left unbuffered.
# 
# abort program if
#	rdr or wtr are null
# 	a system call fails

require IPC::Open3;

sub open2 {
    local $Carp::CarpLevel = $Carp::CarpLevel + 1;
    return IPC::Open3::_open3('open2', $_[1], $_[0], '>&STDERR', @_[2 .. $#_]);
}

1
