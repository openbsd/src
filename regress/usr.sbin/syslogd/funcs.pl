#	$OpenBSD: funcs.pl,v 1.8 2014/12/28 14:08:01 bluhm Exp $

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

my $firstlog = "syslogd regress test first message";
my $testlog = "syslogd regress test log message";
my $downlog = "syslogd regress client shutdown";

sub find_ports {
	my %args = @_;
	my $num    = delete $args{num}    // 1;
	my $domain = delete $args{domain} // AF_INET;
	my $addr   = delete $args{addr}   // "127.0.0.1";
	my $proto  = delete $args{proto}  // "udp";

	my @sockets = (1..$num);
	foreach my $s (@sockets) {
		$s = IO::Socket::INET6->new(
		    Domain    => $domain,
		    LocalAddr => $addr,
		    Proto     => $proto,
		) or die "find_ports: create and bind socket failed: $!";
	}
	my @ports = map { $_->sockport() } @sockets;

	return wantarray ? @ports : $ports[0];
}

########################################################################
# Client funcs
########################################################################

sub write_log {
	my $self = shift;

	write_message($self, $testlog);
	write_shutdown($self, @_);
}

sub write_between2logs {
	my $self = shift;
	my $func = shift;

	write_message($self, $firstlog);
	$func->($self, @_);
	write_message($self, $testlog);
	write_shutdown($self, @_);
}

sub write_message {
	my $self = shift;

	if (defined($self->{connectdomain})) {
		print @_;
		print STDERR @_, "\n";
	} else {
		syslog(LOG_INFO, @_);
	}
}

sub write_shutdown {
	my $self = shift;

	setlogsock("native")
	    or die ref($self), " setlogsock native failed: $!";
	syslog(LOG_NOTICE, $downlog);
}

sub write_unix {
	my $self = shift;
	my $path = shift || "/dev/log";

	my $u = IO::Socket::UNIX->new(
	    Type  => SOCK_DGRAM,
	    Peer => $path,
	) or die ref($self), " connect to $path unix socket failed: $!";
	my $msg = get_testlog(). " $path unix socket";
	print $u $msg;
	print STDERR $msg, "\n";
}

########################################################################
# Server funcs
########################################################################

sub read_log {
	my $self = shift;

	read_message($self, $downlog, @_);
}

sub read_between2logs {
	my $self = shift;
	my $func = shift;

	read_message($self, $firstlog, @_);
	$func->($self, @_);
	read_message($self, $testlog, @_);
	read_message($self, $downlog, @_);
}

sub read_message {
	my $self = shift;
	my $regex = shift;

	local $_;
	for (;;) {
		# reading udp packets works only with sysread()
		defined(sysread(STDIN, $_, 8194))
		    or die ref($self), " read log line failed: $!";
		chomp;
		print STDERR ">>> $_\n";
		last if /$regex/;
	}
}

########################################################################
# Script funcs
########################################################################

sub get_testlog {
	return $testlog;
}

sub get_between2loggrep {
	return (
	    qr/$firstlog/ => 1,
	    qr/$testlog/ => 1,
	);
}

sub get_downlog {
	return $downlog;
}

sub check_logs {
	my ($c, $r, $s, $m, %args) = @_;

	return if $args{nocheck};

	check_log($c, $r, $s, @$m);
	check_out($r, %args);
	check_stat($r, %args);
	check_kdump($c, $r, $s);
	if (my $file = $s->{"outfile"}) {
		my $pattern = $s->{filegrep} || $testlog;
		check_pattern(ref $s, $file, $pattern, \&filegrep);
	}
}

sub compare($$) {
	local $_ = $_[1];
	if (/^\d+/) {
		return $_[0] == $_;
	} elsif (/^==(\d+)/) {
		return $_[0] == $1;
	} elsif (/^!=(\d+)/) {
		return $_[0] != $1;
	} elsif (/^>=(\d+)/) {
		return $_[0] >= $1;
	} elsif (/^<=(\d+)/) {
		return $_[0] <= $1;
	}
	die "bad compare operator: $_";
}

sub check_pattern {
	my ($name, $proc, $pattern, $func) = @_;

	$pattern = [ $pattern ] unless ref($pattern) eq 'ARRAY';
	foreach my $pat (@$pattern) {
		if (ref($pat) eq 'HASH') {
			while (my($re, $num) = each %$pat) {
				my @matches = $func->($proc, $re);
				compare(@matches, $num)
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
	foreach my $proc (@_) {
		next unless $proc && !$proc->{nocheck};
		my $pattern = $proc->{loggrep} || $testlog;
		check_pattern(ref $proc, $proc, $pattern, \&loggrep);
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
		my $file = $r->{"out$name"} or die;
		my $pattern = $args{$name}{loggrep} || $testlog;
		check_pattern($name, $file, $pattern, \&filegrep);
	}
}

sub check_stat {
	my ($r, %args) = @_;

	foreach my $name (qw(fstat)) {
		next unless $r && $r->{$name};
		my $file = $r->{"${name}file"} or die;
		my $pattern = $args{$name}{loggrep} or die;
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
	foreach my $proc (@_) {
		next unless $proc && $proc->{ktrace};
		my $file = $proc->{ktracefile} or die;
		my $pattern = $proc->{kdump} or die;
		check_pattern(ref $proc, $file, $pattern, \&kdumpgrep);
	}
}

sub kdumpgrep {
	my ($file, $pattern) = @_;

	my @sudo = ! -r $file && $ENV{SUDO} ? $ENV{SUDO} : ();
	my @cmd = (@sudo, "kdump", "-f", $file);
	open(my $fh, '-|', @cmd)
	    or die "Open pipe from '@cmd' failed: $!";
	my @matches = grep { /$pattern/ } <$fh>;
	close($fh) or die $! ?
	    "Close pipe from '@cmd' failed: $!" :
	    "Command '@cmd' failed: $?";
	return wantarray ? @matches : $matches[0];
}

1;
