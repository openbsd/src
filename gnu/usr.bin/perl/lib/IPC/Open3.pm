package IPC::Open3;
require 5.001;
require Exporter;
use Carp;

=head1 NAME

IPC::Open3, open3 - open a process for reading, writing, and error handling

=head1 SYNOPSIS

    $pid = open3(\*WTRFH, \*RDRFH, \*ERRFH 
		    'some cmd and args', 'optarg', ...);

=head1 DESCRIPTION

Extremely similar to open2(), open3() spawns the given $cmd and
connects RDRFH for reading, WTRFH for writing, and ERRFH for errors.  If
ERRFH is '', or the same as RDRFH, then STDOUT and STDERR of the child are
on the same file handle.

If WTRFH begins with "<&", then WTRFH will be closed in the parent, and
the child will read from it directly.  If RDRFH or ERRFH begins with
">&", then the child will send output directly to that file handle.  In both
cases, there will be a dup(2) instead of a pipe(2) made.

If you try to read from the child's stdout writer and their stderr
writer, you'll have problems with blocking, which means you'll
want to use select(), which means you'll have to use sysread() instead
of normal stuff.

All caveats from open2() continue to apply.  See L<open2> for details.

=cut

@ISA = qw(Exporter);
@EXPORT = qw(open3);

# &open3: Marc Horowitz <marc@mit.edu>
# derived mostly from &open2 by tom christiansen, <tchrist@convex.com>
# fixed for 5.001 by Ulrich Kunitz <kunitz@mai-koeln.com>
#
# $Id: Open3.pm,v 1.1.1.1 1996/08/19 10:12:45 downsj Exp $
#
# usage: $pid = open3('wtr', 'rdr', 'err' 'some cmd and args', 'optarg', ...);
#
# spawn the given $cmd and connect rdr for
# reading, wtr for writing, and err for errors.
# if err is '', or the same as rdr, then stdout and
# stderr of the child are on the same fh.  returns pid
# of child, or 0 on failure.


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
#   pipe or fork or exec fails

$fh = 'FHOPEN000';  # package static in case called more than once

sub open3 {
    my($kidpid);
    my($dad_wtr, $dad_rdr, $dad_err, @cmd) = @_;
    my($dup_wtr, $dup_rdr, $dup_err);

    $dad_wtr			|| croak "open3: wtr should not be null";
    $dad_rdr			|| croak "open3: rdr should not be null";
    $dad_err = $dad_rdr if ($dad_err eq '');

    $dup_wtr = ($dad_wtr =~ s/^[<>]&//);
    $dup_rdr = ($dad_rdr =~ s/^[<>]&//);
    $dup_err = ($dad_err =~ s/^[<>]&//);

    # force unqualified filehandles into callers' package
    my($package) = caller;
    $dad_wtr =~ s/^[^:]+$/$package\:\:$&/ unless ref $dad_wtr;
    $dad_rdr =~ s/^[^:]+$/$package\:\:$&/ unless ref $dad_rdr;
    $dad_err =~ s/^[^:]+$/$package\:\:$&/ unless ref $dad_err;

    my($kid_rdr) = ++$fh;
    my($kid_wtr) = ++$fh;
    my($kid_err) = ++$fh;

    if (!$dup_wtr) {
	pipe($kid_rdr, $dad_wtr)    || croak "open3: pipe 1 (stdin) failed: $!";
    }
    if (!$dup_rdr) {
	pipe($dad_rdr, $kid_wtr)    || croak "open3: pipe 2 (stdout) failed: $!";
    }
    if ($dad_err ne $dad_rdr && !$dup_err) {
	pipe($dad_err, $kid_err)    || croak "open3: pipe 3 (stderr) failed: $!";
    }

    if (($kidpid = fork) < 0) {
        croak "open3: fork failed: $!";
    } elsif ($kidpid == 0) {
	if ($dup_wtr) {
	    open(STDIN,  "<&$dad_wtr") if (fileno(STDIN) != fileno($dad_wtr));
	} else {
	    close($dad_wtr);
	    open(STDIN,  "<&$kid_rdr");
	}
	if ($dup_rdr) {
	    open(STDOUT, ">&$dad_rdr") if (fileno(STDOUT) != fileno($dad_rdr));
	} else {
	    close($dad_rdr);
	    open(STDOUT, ">&$kid_wtr");
	}
	if ($dad_rdr ne $dad_err) {
	    if ($dup_err) {
		open(STDERR, ">&$dad_err")
		    if (fileno(STDERR) != fileno($dad_err));
	    } else {
		close($dad_err);
		open(STDERR, ">&$kid_err");
	    }
	} else {
	    open(STDERR, ">&STDOUT") if (fileno(STDERR) != fileno(STDOUT));
	}
	local($")=(" ");
	exec @cmd
	    or croak "open3: exec of @cmd failed";
    }

    close $kid_rdr; close $kid_wtr; close $kid_err;
    if ($dup_wtr) {
	close($dad_wtr);
    }

    select((select($dad_wtr), $| = 1)[0]); # unbuffer pipe
    $kidpid;
}
1; # so require is happy

