#!/usr/bin/perl
#
# Prune commented out and crufty entries from skeykeys
# Usage: skeyprune [days]
#
# Todd C. Miller <Todd.Miller@courtesan.com>
# $OpenBSD: skeyprune.pl,v 1.1 1996/09/28 00:00:41 millert Exp $

# We need to be able convert to time_t
require 'timelocal.pl';

# Keep out the stupid
die "Only root may run $0.\n" if $>;
die "Usage: $0 [days]\n" if $#ARGC > 0;

# Pathnames
$keyfile = '/etc/skeykeys';
$temp = "$keyfile.tmp$$";

# Quick mapping of month name -> number
%months = ('Jan', 0, 'Feb', 1, 'Mar', 2, 'Apr', 3, 'May', 4,  'Jun', 5,
	   'Jul', 6, 'Aug', 7, 'Sep', 8, 'Oct', 9, 'Nov', 10, 'Dec', 11);

# Remove entries that haven't been modified in this many days.
$days_old = $ARGV[0] || -1;

# Open current key file
open(OLD, $keyfile) || die "$0: Can't open $keyfile: $!\n";

# Safely open temp file
umask(077);
unlink($temp);
open(NEW, ">$temp") || die "$0: Can't open tempfile $temp: $!\n";

# We need to be extra speedy to close the window where someone can hose us.
setpriority(0, 0, -4);

while (<OLD>) {
    # Ignore commented out entries
    if ( ! /^#[^\s#]+\s+(MD[0-9]+\s+)?[0-9]+\s+[A-z0-9_-]+\s+[a-f0-9]+\s+(Jan|Feb|Mar|Apr|May|Ju[nl]|Aug|Sep|Oct|Nov|Dec)\s+[0-9]+,\s*[0-9]+\s+[0-9]+:[0-9]+:[0-9]+$/ ) {
	/((Jan|Feb|Mar|Apr|May|Ju[nl]|Aug|Sep|Oct|Nov|Dec)\s+[0-9]+,\s*[0-9]+\s+[0-9]+:[0-9]+:[0-9]+)$/;

	# Prune out old entries if asked to
	if ($days_old > 0) {
	    # build up time based on date string
	    @date = split(/[\s,:]/, $1);
	    $sec = $date[5];
	    $min = $date[4];
	    $hours = $date[3];
	    $mday = $date[1] - 1;
	    $mon = $months{$date[0]};
	    $year = $date[2] - 1900;

	    $now = time();
	    $then = &timelocal($sec,$min,$hours,$mday,$mon,$year);
	    if (($now - $then) / (60 * 60 * 24) - 1 <= $days_old) {
		print NEW $_ || do {
		    warn "Can't write to $temp: $!\n";
		    unlink($temp);
		};
	    }
	} else {
	    print NEW $_ || do {
		warn "Can't write to $temp: $!\n";
		unlink($temp);
	    };
	}
    }
}
close(OLD);
close(NEW);

# Set owner/group/mode on tempfile and move to real location.
($mode, $nlink, $uid, $gid) = (stat($keyfile))[2..5];
if (!defined($mode)) {
    unlink($temp);
    die "$0: Unable to stat $keyfile: $!\n";
}
if (!chmod($mode, $temp)) {
    unlink($temp);
    die "$0: Unable to set mode of $temp to $mode: $!\n";
}
if (!chown($uid, $gid, $temp)) {
    unlink($temp);
    die "$0: Unable to set owner of $temp to ($uid, $gid): $!\n";
}
# Leave temp file in place if rename fails.  Might help in debugging.
rename($temp, $keyfile) || die "$0: Unable to rename $temp to $keyfile: $!\n";

exit(0);
