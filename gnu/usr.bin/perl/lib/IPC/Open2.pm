package IPC::Open2;
require 5.000;
require Exporter;
use Carp;

=head1 NAME

IPC::Open2, open2 - open a process for both reading and writing

=head1 SYNOPSIS

    use IPC::Open2;
    $pid = open2(\*RDR, \*WTR, 'some cmd and args');
      # or
    $pid = open2(\*RDR, \*WTR, 'some', 'cmd', 'and', 'args');

=head1 DESCRIPTION

The open2() function spawns the given $cmd and connects $rdr for
reading and $wtr for writing.  It's what you think should work 
when you try

    open(HANDLE, "|cmd args");

open2() returns the process ID of the child process.  It doesn't return on
failure: it just raises an exception matching C</^open2:/>.

=head1 WARNING 

It will not create these file handles for you.  You have to do this yourself.
So don't pass it empty variables expecting them to get filled in for you.

Additionally, this is very dangerous as you may block forever.
It assumes it's going to talk to something like B<bc>, both writing to
it and reading from it.  This is presumably safe because you "know"
that commands like B<bc> will read a line at a time and output a line at
a time.  Programs like B<sort> that read their entire input stream first,
however, are quite apt to cause deadlock.  

The big problem with this approach is that if you don't have control 
over source code being run in the the child process, you can't control what it does 
with pipe buffering.  Thus you can't just open a pipe to C<cat -v> and continually
read and write a line from it.

=head1 SEE ALSO

See L<open3> for an alternative that handles STDERR as well.

=cut

@ISA = qw(Exporter);
@EXPORT = qw(open2);

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
# 	pipe or fork or exec fails

$fh = 'FHOPEN000';  # package static in case called more than once

sub open2 {
    local($kidpid);
    local($dad_rdr, $dad_wtr, @cmd) = @_;

    $dad_rdr ne '' 		|| croak "open2: rdr should not be null";
    $dad_wtr ne '' 		|| croak "open2: wtr should not be null";

    # force unqualified filehandles into callers' package
    local($package) = caller;
    $dad_rdr =~ s/^[^']+$/$package'$&/ unless ref $dad_rdr;
    $dad_wtr =~ s/^[^']+$/$package'$&/ unless ref $dad_wtr;

    local($kid_rdr) = ++$fh;
    local($kid_wtr) = ++$fh;

    pipe($dad_rdr, $kid_wtr) 	|| croak "open2: pipe 1 failed: $!";
    pipe($kid_rdr, $dad_wtr) 	|| croak "open2: pipe 2 failed: $!";

    if (($kidpid = fork) < 0) {
	croak "open2: fork failed: $!";
    } elsif ($kidpid == 0) {
	close $dad_rdr; close $dad_wtr;
	open(STDIN,  "<&$kid_rdr");
	open(STDOUT, ">&$kid_wtr");
	warn "execing @cmd\n" if $debug;
	exec @cmd
	    or croak "open2: exec of @cmd failed";   
    } 
    close $kid_rdr; close $kid_wtr;
    select((select($dad_wtr), $| = 1)[0]); # unbuffer pipe
    $kidpid;
}
1; # so require is happy

