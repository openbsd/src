#!/usr/bin/perl -w
#
# Copyright (c) 1996, 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
# $OpenBSD: skeyprune.pl,v 1.3 2002/05/16 18:27:34 millert Exp $
#

use POSIX qw(S_ISREG);
use Fcntl qw(:DEFAULT :flock);

# Keep out the stupid
die "Only root may run $0.\n" if $>;
die "Usage: $0 [days]\n" if $#ARGV > 0;

# Pathnames
$skeydir = '/etc/skey';

# Remove entries that haven't been modified in this many days.
$days_old = $ARGV[0] || -1;

# Safe umask
umask(077);

# Current time
$now = time();

# Slurp mode
undef $/;

chdir($skeydir) || die "$0: Can't cd to $skeydir: $!\n";
opendir(SKEYDIR, ".") || die "$0: Can't open $skeydir: $!\n";
while (defined($user = readdir(SKEYDIR))) {
	next if $user =~ /^\./;
	if (!sysopen(SKEY, $user, 0, O_RDWR | O_NONBLOCK | O_NOFOLLOW)) {
	    warn "$0: Can't open $user: $!\n";
	    next;
	}
	if (!flock(SKEY, LOCK_EX)) {
		warn "$0: Can't lock $user: $!\n";
		close(SKEY);
		next;
	}

	if (!stat(SKEY)) {
		warn "$0: Can't stat $user: $!\n";
		close(SKEY);
		next;
	}

	# Sanity checks.
	if (!S_ISREG((stat(_))[2])) {
		warn "$0: $user is not a regular file\n";
		close(SKEY);
		next;
	}
	if (((stat(_))[2] & 07777) != 0600) {
		printf STDERR ("%s: Bad mode for %s: 0%o\n", $0, $user,
		    (stat(_))[2]);
		close(SKEY);
		next;
	}
	if ((stat(_))[3] != 1) {
		printf STDERR ("%s: Bad link count for %s: %d\n", $0, $user,
		    (stat(_))[3]);
		close(SKEY);
		next;
	}

	# Remove zero size entries
	if (-z _) {
		unlink($user) || warn "$0: Can't unlink $user: $!\n";
		close(SKEY);
		next;
	}

	# Prune out old entries if asked to
	if ($days_old > 0) {
		$then = (stat(_))[9];
		if (($now - $then) / (60 * 60 * 24) - 1 > $days_old) {
			unlink($user) || warn "$0: Can't unlink $user: $!\n";
			close(SKEY);
			next;
		}
	}

	# Read in the entry and check its contents.
	$entry = <SKEY>;
	if ($entry !~ /^\S+[\r\n]+\S+[\r\n]+\d+[\r\n]+[A-z0-9]+[\r\n]+[a-f0-9]+[\r\n]+$/) {
		warn "$0: Invalid entry for $user:\n$entry";
	}

	close(SKEY);
}
exit(0);
