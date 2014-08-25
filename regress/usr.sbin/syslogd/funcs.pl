#	$OpenBSD: funcs.pl,v 1.2 2014/08/25 17:55:27 bluhm Exp $

# Copyright (c) 2010-2014 Alexander Bluhm <bluhm@openbsd.org>
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

use strict;
use warnings;
use Errno;
use List::Util qw(first);
use Socket;
use Socket6;
use Sys::Syslog qw(:standard :extended :macros);
use IO::Socket;
use IO::Socket::INET6;

my $testlog = "syslogd regress test log message";
my $downlog = "syslogd regress client shutdown";

########################################################################
# Client funcs
########################################################################

sub write_log {
	my $self = shift;

	if (defined($self->{connectdomain})) {
		print $testlog;
		print STDERR $testlog, "\n";
	} else {
		syslog(LOG_INFO, $testlog);
	}
	write_shutdown($self, @_);
}

sub write_shutdown {
	my $self = shift;

	setlogsock("native")
	    or die ref($self), " setlogsock native failed: $!";
	syslog(LOG_NOTICE, $downlog);
}

########################################################################
# Server funcs
########################################################################

sub read_log {
	my $self = shift;

	for (;;) {
		defined(sysread(STDIN, my $line, 8194))
		    or die ref($self), " read log line failed: $!";
		chomp $line;
		print STDERR ">>> $line\n";
		last if $line =~ /$downlog/;
	}
}

########################################################################
# Script funcs
########################################################################

sub get_log {
	return $testlog;
}

sub check_logs {
	my ($c, $r, $s, %args) = @_;

	return if $args{nocheck};

	check_log($c, $r, $s, %args);
	check_out($r, %args);
	check_stat($r, %args);
	check_kdump($c, $s, %args);
}

sub check_pattern {
	my ($name, $proc, $pattern, $func) = @_;

	$pattern = [ $pattern ] unless ref($pattern) eq 'ARRAY';
	foreach my $pat (@$pattern) {
		if (ref($pat) eq 'HASH') {
			while (my($re, $num) = each %$pat) {
				my @matches = $func->($proc, $re);
				@matches == $num
				    or die "$name matches '@matches': ",
				    "'$re' => $num";
			}
		} else {
			$func->($proc, $pat)
			    or die "$name log missing pattern: $pat";
		}
	}
}

sub check_log {
	my ($c, $r, $s, %args) = @_;

	my %name2proc = (client => $c, syslogd => $r, server => $s);
	foreach my $name (qw(client syslogd server)) {
		next if $args{$name}{nocheck};
		my $p = $name2proc{$name} or next;
		my $pattern = $args{$name}{loggrep} || $testlog;
		check_pattern($name, $p, $pattern, \&loggrep);
	}
}

sub loggrep {
	my ($proc, $pattern) = @_;

	return $proc->loggrep($pattern);
}

sub check_out {
	my ($r, %args) = @_;

	foreach my $name (qw(file pipe)) {
		next if $args{$name}{nocheck};
		my $file = $r->{"out$name"} or next;
		my $pattern = $args{$name}{loggrep} || $testlog;
		check_pattern($name, $file, $pattern, \&filegrep);
	}
}

sub check_stat {
	my ($r, %args) = @_;

	foreach my $name (qw(fstat)) {
		next if $args{$name}{nocheck};
		my $file = $r->{$name} && $r->{"${name}file"} or next;
		my $pattern = $args{$name}{loggrep} or next;
		check_pattern($name, $file, $pattern, \&filegrep);
	}
}

sub filegrep {
	my ($file, $pattern) = @_;

	open(my $fh, '<', $file)
	    or die "Open file $file for reading failed: $!";
	return wantarray ?
	    grep { /$pattern/ } <$fh> : first { /$pattern/ } <$fh>;
}

sub check_kdump {
	my ($c, $s, %args) = @_;

	my %name2proc = (client => $c, server => $s);
	foreach my $name (qw(client server)) {
		next unless $args{$name}{ktrace};
		my $p = $name2proc{$name} or next;
		my $file = $p->{ktracefile} or next;
		my $pattern = $args{$name}{kdump} or next;
		check_pattern($name, $file, $pattern, \&kdumpgrep);
	}
}

sub kdumpgrep {
	my ($file, $pattern) = @_;

	my @cmd = ("kdump", "-f", $file);
	open(my $fh, '-|', @cmd)
	    or die "Open pipe from '@cmd' failed: $!";
	my @matches = grep { /$pattern/ } <$fh>;
	close($fh) or die $! ?
	    "Close pipe from '@cmd' failed: $!" :
	    "Command '@cmd' failed: $?";
	return wantarray ? @matches : $matches[0];
}

1;
