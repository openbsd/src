#!/usr/bin/perl -w
#
# Copyright (C) 2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: stop.pl,v 1.4 2001/03/08 02:34:01 neild Exp $

# Framework for stopping test servers
# Based on the type of server specified, signal the server to stop, wait
# briefly for it to die, and then kill it if it is still alive.
# If a server is specified, stop it. Otherwise, stop all servers for test.

use strict;
use Cwd 'abs_path';

# Option handling
#   [--use-rndc] test [server]
#
#   test - name of the test directory
#   server - name of the server directory

my $usage = "usage: $0 [--use-rndc] test-directory [server-directory]";
my $use_rndc;

while (@ARGV && $ARGV[0] =~ /^-/) {
	my $opt = shift @ARGV;
	if ($opt eq '--use-rndc') {
		$use_rndc = 1;
	} else {
		die "$usage\n";
	}
}

my $test = $ARGV[0];
my $server = $ARGV[1];

my $errors = 0;

die "$usage\n" unless defined($test);
die "No test directory: \"$test\"\n" unless (-d $test);
die "No server directory: \"$server\"\n" if (defined($server) && !-d $server);
    
# Global variables
my $testdir = abs_path($test);
my @servers;


# Determine which servers need to be stopped.
if (defined $server) {
	@servers = ($server);
} else {
	local *DIR;
	opendir DIR, $testdir or die "$testdir: $!\n";
	my @files = sort readdir DIR;
	closedir DIR;

	my @ns = grep /^ns[0-9]*$/, @files;
	my @lwresd = grep /^lwresd[0-9]*$/, @files;
	my @ans = grep /^ans[0-9]*$/, @files;
	
	push @servers, @ns, @lwresd, @ans;
}


# Stop the server(s), pass 1: rndc.
if ($use_rndc) {
	foreach my $server (grep /^ns/, @servers) {
		stop_rndc($server);
	}

	wait_for_servers(5, grep /^ns/, @servers);
}


# Pass 2: SIGTERM
foreach my $server (@servers) {
	stop_signal($server, "TERM");
}
wait_for_servers(5, @servers);


# Pass 3: SIGKILL
foreach my $server (@servers) {
	stop_signal($server, "KILL");
}

exit($errors ? 1 : 0);

# Subroutines

# Return the full path to a given server's PID file.
sub server_pid_file {
	my($server) = @_;

	my $pid_file;
	if ($server =~ /^ns/) {
		$pid_file = "named.pid";
	} elsif ($server =~ /^lwresd/) {
		$pid_file = "lwresd.pid";
	} elsif ($server =~ /^ans/) {
		$pid_file = "ans.pid";
	} else {
		print "I:Unknown server type $server\n";
		exit 1;
	}
	$pid_file = "$testdir/$server/$pid_file";
}

# Read a PID.
sub read_pid {
	my($pid_file) = @_;

	local *FH;
	my $result = open FH, "< $pid_file";
	if (!$result) {
		print "I:$pid_file: $!\n";
		unlink $pid_file;
		return;
	}

	my $pid = <FH>;
	chomp($pid);
	return $pid;
}

# Stop a named process with rndc.
sub stop_rndc {
	my($server) = @_;

	return unless ($server =~ /^ns(\d+)$/);
	my $ip = "10.53.0.$1";

	# Ugly, but should work.
	system("$ENV{RNDC} -c $testdir/../common/rndc.conf -s $ip -p 9953 stop | sed 's/^/I:$server /'");
	return;
}

# Stop a server by sending a signal to it.
sub stop_signal {
	my($server, $sig) = @_;

	my $pid_file = server_pid_file($server);
	return unless -f $pid_file;
	
	my $pid = read_pid($pid_file);
	return unless defined($pid);

	print "I:$server didn't die when sent a SIGTERM\n" if ($sig eq 'KILL');

	my $result = kill $sig, $pid;
	if (!$result) {
		print "I:$server died before a SIG$sig was sent\n";
		unlink $pid_file;
		$errors++;
	}

	return;
}

sub wait_for_servers {
	my($timeout, @servers) = @_;

	my @pid_files = grep { defined($_) }
	                map  { server_pid_file($_) } @servers;

	while ($timeout > 0 && @pid_files > 0) {
		@pid_files = grep { -f $_ } @pid_files;
		sleep 1 if (@pid_files > 0);
		$timeout--;
	}

	return;
}
