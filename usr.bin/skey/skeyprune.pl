#!/usr/bin/perl -w
#
# Copyright (c) 1996, 2001 Todd C. Miller <Todd.Miller@courtesan.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Prune commented out, bogus, and crufty entries from /etc/skeykeys
# Usage: skeyprune [days]
#
# $OpenBSD: skeyprune.pl,v 1.2 2001/06/20 22:19:58 millert Exp $
#

use File::Temp qw(:mktemp);
use Fcntl qw(:DEFAULT :flock);
use Time::Local;

# Keep out the stupid
die "Only root may run $0.\n" if $>;
die "Usage: $0 [days]\n" if $#ARGV > 0;

# Pathnames
$keyfile = '/etc/skeykeys';
$template = "$keyfile.XXXXXXXX";

# Quick mapping of month name -> number
%months = ('Jan', 0, 'Feb', 1, 'Mar', 2, 'Apr', 3, 'May', 4,  'Jun', 5,
	   'Jul', 6, 'Aug', 7, 'Sep', 8, 'Oct', 9, 'Nov', 10, 'Dec', 11);

# Remove entries that haven't been modified in this many days.
$days_old = $ARGV[0] || -1;

# Safe umask
umask(077);

# Open and lock the current key file
open(OLD, $keyfile) || die "$0: Can't open $keyfile: $!\n";
flock(OLD, LOCK_EX) || die "$0: Can't lock $keyfile: $!\n";

# Safely open temp file
($NEW, $temp) = mkstemp($template);
die "$0: Can't open tempfile $template: $!\n" unless $temp;

# Run at a high priority so we don't keep things locked for too long
setpriority(0, 0, -4);

while (<OLD>) {
	chomp();

	# Valid entry: 'username hash seq seed key date"
	if ( /^[^\s#]+\s+(\S+\s+)?[0-9]+\s+[A-z0-9]+\s+[a-f0-9]+\s+(Jan|Feb|Mar|Apr|May|Ju[nl]|Aug|Sep|Oct|Nov|Dec)\s+[0-9]+,\s*[0-9]+\s+[0-9]+:[0-9]+:[0-9]+$/ ) {

		@entry = split(/[\s,:]+/, $_);
		# Prune out old entries if asked to
		if ($days_old > 0) {
			# build up time based on date string
			$sec = $date[10];
			$min = $date[9];
			$hours = $date[8];
			$mday = $date[6] - 1;
			$mon = $months{$date[5]};
			$year = $date[7] - 1900;

			$now = time();
			$then = timelocal($sec,$min,$hours,$mday,$mon,$year);
			if (($now - $then) / (60 * 60 * 24) - 1 > $days_old) {
				next;	# too old
			}
		}

		# Missing hash type?  Must be md4...
		if ($entry[1] =~ /^\d/) {
			splice(@entry, 1, 0, "md4");
		}

		printf $NEW "%s %s %04d %-16s %s %4s %02d,%-4d %02d:%02d:%02d\n",
		    $entry[0], $entry[1], $entry[2], $entry[3], $entry[4],
		    $entry[5], $entry[6], $entry[7], $entry[8], $entry[9],
		    $entry[10] || do {
			warn "Can't write to $temp: $!\n";
			unlink($temp);
			exit(1);
		};
	}
}
close(OLD);
close($NEW);

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
if ($nlink != 1) {
	$nlink--;
	warn "$0: Old $keyfile had $nlink hard links, those will be broken\n";
}
# Leave temp file in place if rename fails.  Might help in debugging.
rename($temp, $keyfile) || die "$0: Unable to rename $temp to $keyfile: $!\n";

exit(0);
