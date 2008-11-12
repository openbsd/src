#!/usr/bin/perl -w
#
# Copyright (c) 1996, 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Sponsored in part by the Defense Advanced Research Projects
# Agency (DARPA) and Air Force Research Laboratory, Air Force
# Materiel Command, USAF, under agreement number F39502-99-1-0512.
#
# Prune commented out, bogus, and crufty entries from /etc/skeykeys
# usage: skeyprune [days]
#
# $OpenBSD: skeyprune.pl,v 1.6 2008/11/12 16:13:46 sobrado Exp $
#

use POSIX qw(S_ISREG);
use Fcntl qw(:DEFAULT :flock);

# Keep out the stupid
die "Only root may run $0.\n" if $>;
die "usage: $0 [days]\n" if $#ARGV > 0;

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
