# &open3: Marc Horowitz <marc@mit.edu>
# derived mostly from &open2 by tom christiansen, <tchrist@convex.com>
#
# $Id: open3.pl,v 1.1 1993/11/23 06:26:15 marc Exp $
#
# usage: $pid = open3('wtr', 'rdr', 'err' 'some cmd and args', 'optarg', ...);
#
# spawn the given $cmd and connect rdr for
# reading, wtr for writing, and err for errors.
# if err is '', or the same as rdr, then stdout and
# stderr of the child are on the same fh.  returns pid
# of child, or 0 on failure.


# if wtr begins with '>&', then wtr will be closed in the parent, and
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

package open3;

$fh = 'FHOPEN000';  # package static in case called more than once

sub main'open3 {
    local($kidpid);
    local($dad_wtr, $dad_rdr, $dad_err, @cmd) = @_;
    local($dup_wtr, $dup_rdr, $dup_err);

    $dad_wtr			|| die "open3: wtr should not be null";
    $dad_rdr			|| die "open3: rdr should not be null";
    $dad_err = $dad_rdr if ($dad_err eq '');

    $dup_wtr = ($dad_wtr =~ s/^\>\&//);
    $dup_rdr = ($dad_rdr =~ s/^\>\&//);
    $dup_err = ($dad_err =~ s/^\>\&//);

    # force unqualified filehandles into callers' package
    local($package) = caller;
    $dad_wtr =~ s/^[^']+$/$package'$&/;
    $dad_rdr =~ s/^[^']+$/$package'$&/;
    $dad_err =~ s/^[^']+$/$package'$&/;

    local($kid_rdr) = ++$fh;
    local($kid_wtr) = ++$fh;
    local($kid_err) = ++$fh;

    if (!$dup_wtr) {
	pipe($kid_rdr, $dad_wtr)    || die "open3: pipe 1 (stdin) failed: $!";
    }
    if (!$dup_rdr) {
	pipe($dad_rdr, $kid_wtr)    || die "open3: pipe 2 (stdout) failed: $!";
    }
    if ($dad_err ne $dad_rdr && !$dup_err) {
	pipe($dad_err, $kid_err)    || die "open3: pipe 3 (stderr) failed: $!";
    }

    if (($kidpid = fork) < 0) {
        die "open2: fork failed: $!";
    } elsif ($kidpid == 0) {
	if ($dup_wtr) {
	    open(STDIN,  ">&$dad_wtr") if (fileno(STDIN) != fileno($dad_wtr));
	} else {
	    close($dad_wtr);
	    open(STDIN,  ">&$kid_rdr");
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
	exec @cmd;
        die "open2: exec of @cmd failed";
    }

    close $kid_rdr; close $kid_wtr; close $kid_err;
    if ($dup_wtr) {
	close($dad_wtr);
    }

    select((select($dad_wtr), $| = 1)[0]); # unbuffer pipe
    $kidpid;
}
1; # so require is happy
