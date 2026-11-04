#	$OpenBSD $

# Copyright (c) 2026 doom <doom@tagram.loom>
# Copyright (c) 2010-2021 Alexander Bluhm <bluhm@openbsd.org>
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
use IO::Socket;
use IO::Socket::IP;
use Socket6;
use Sys::Hostname;

sub find_ports {
	my %args = @_;
	my $num    = delete $args{num}    // 1;
	my $domain = delete $args{domain} // AF_INET;
	my $addr   = delete $args{addr}   // "127.0.0.1";

	my @sockets = (1..$num);
	foreach my $s (@sockets) {
		$s = IO::Socket::IP->new(
		    Proto  => "tcp",
		    Family => $domain,
		    $addr ? (LocalAddr => $addr) : (),
		) or die "find_ports: create and bind socket failed: $!";
	}
	my @ports = map { $_->sockport() } @sockets;

	return @ports;
}

########################################################################
# Script funcs
########################################################################

sub check_logs {
	my ($c, $d, $s, %args) = @_;

	return if $args{nocheck};

	check_loggrep($c, $d, $s, %args);
}

sub check_loggrep {
	my ($c, $d, $s, %args) = @_;

	my %name2proc = (client => $c, smtpd => $d, server => $s);
	foreach my $name (qw(client smtpd server)) {
		my $p = $name2proc{$name} or next;
		my $pattern = $args{$name}{loggrep} or next;
		$pattern = [ $pattern ] unless ref($pattern) eq 'ARRAY';
		foreach my $pat (@$pattern) {
			if (ref($pat) eq 'HASH') {
				while (my($re, $num) = each %$pat) {
					my @matches = $p->loggrep($re);
					@matches == $num
					    or die "$name matches '@matches': ",
					    "'$re' => $num";
				}
			} else {
				$p->loggrep($pat)
				    or die "$name log missing pattern: '$pat'";
			}
		}
	}
}

########################################################################
# SMTP low-level I/O; used by both smtp_client and lmtp_server
########################################################################

sub _smtp_readline {
	# Read one CRLF-terminated line from STDIN; log and return without CRLF.
	local $/ = "\r\n";
	my $line = <STDIN>;
	return undef unless defined $line;
	$line =~ s/\r\n$//;
	print STDERR "<<< $line\n";
	return $line;
}

sub _smtp_send {
	# Write one SMTP line (appends CRLF) to STDOUT and log it.
	my $line = shift;
	print STDERR ">>> $line\n";
	print STDOUT "$line\r\n"
	    or die "_smtp_send: print failed: $!";
	IO::Handle::flush(\*STDOUT);
}

sub _smtp_read_response {
	# Read a complete SMTP response (possibly multi-line).
	# Returns (numeric-code, arrayref-of-lines).
	# Dies on EOF.
	my @lines;
	while (1) {
		my $line = _smtp_readline();
		defined($line) or die "_smtp_read_response: unexpected EOF";
		push @lines, $line;
		# Multi-line replies use "NNN-text"; final line uses "NNN text".
		last unless $line =~ /^\d{3}-/;
	}
	my $code = substr($lines[-1], 0, 3);
	return ($code, \@lines);
}

########################################################################
# Client funcs
########################################################################

sub smtp_client {
	my $self = shift;
	my $from = $self->{from} || 'sender@example.com';
	my $to   = $self->{to}   || 'user@localhost';
	my $subj = $self->{subject} || 'smtpd-regress test';
	my $body = $self->{body}    || "This is a test message from smtpd-regress.\r\n";

	# Greeting
	my ($code, $lines) = _smtp_read_response();
	$code eq '220'
	    or die ref($self), " expected 220 greeting, got: $lines->[-1]";

	# EHLO
	my $hostname = hostname();
	_smtp_send("EHLO $hostname");
	($code, $lines) = _smtp_read_response();
	$code eq '250'
	    or die ref($self), " EHLO failed: $lines->[-1]";

	# MAIL FROM; a 4xx/5xx here means the server (or a filter) rejected
	# the sender.  Log it and return cleanly so loggrep can assert on it.
	_smtp_send("MAIL FROM:<$from>");
	($code, $lines) = _smtp_read_response();
	unless ($code eq '250') {
		print STDERR "smtp_client: MAIL FROM rejected: $lines->[-1]\n";
		_smtp_send("QUIT");
		_smtp_read_response();
		return;
	}

	# RCPT TO; same: rejection is a valid test outcome.
	_smtp_send("RCPT TO:<$to>");
	($code, $lines) = _smtp_read_response();
	unless ($code eq '250') {
		print STDERR "smtp_client: RCPT TO rejected: $lines->[-1]\n";
		_smtp_send("QUIT");
		_smtp_read_response();
		return;
	}

	# DATA
	_smtp_send("DATA");
	($code, $lines) = _smtp_read_response();
	$code eq '354'
	    or die ref($self), " DATA failed: $lines->[-1]";

	# Headers + body.  Dot-stuff any body lines starting with '.'.
	print STDOUT "From: $from\r\n";
	print STDOUT "To: $to\r\n";
	print STDOUT "Subject: $subj\r\n";
	print STDOUT "\r\n";
	foreach my $line (split(/\r?\n/, $body)) {
		$line = ".$line" if $line =~ /^\./;
		print STDOUT "$line\r\n";
	}
	_smtp_send(".");
	($code, $lines) = _smtp_read_response();
	unless ($code eq '250') {
		print STDERR "smtp_client: message rejected after DATA: $lines->[-1]\n";
		_smtp_send("QUIT");
		_smtp_read_response();
		return;
	}

	# QUIT
	_smtp_send("QUIT");
	($code, $lines) = _smtp_read_response();
	$code eq '221'
	    or die ref($self), " QUIT failed: $lines->[-1]";
}

########################################################################
# Server funcs
########################################################################

sub lmtp_server {
	my $self = shift;
	my $hostname = hostname();

	# Greeting
	_smtp_send("220 $hostname LMTP $hostname ready");

	# LHLO
	my $cmd = _smtp_readline();
	defined($cmd) or die ref($self), " unexpected EOF waiting for LHLO";
	$cmd =~ /^LHLO\s/i
	    or die ref($self), " expected LHLO, got: $cmd";
	_smtp_send("250-$hostname");
	_smtp_send("250-8BITMIME");
	_smtp_send("250-ENHANCEDSTATUSCODES");
	_smtp_send("250 Ok");

	# MAIL FROM
	$cmd = _smtp_readline();
	defined($cmd) or die ref($self), " unexpected EOF waiting for MAIL FROM";
	$cmd =~ /^MAIL FROM:/i
	    or die ref($self), " expected MAIL FROM, got: $cmd";
	my ($from) = $cmd =~ /^MAIL FROM:<([^>]*)>/i;
	print STDERR "MAIL: $cmd\n";
	_smtp_send("250 2.1.0 Ok");

	# One or more RCPT TO
	my @rcpts;
	while (1) {
		$cmd = _smtp_readline();
		defined($cmd) or die ref($self), " unexpected EOF waiting for RCPT/DATA";
		last unless $cmd =~ /^RCPT TO:/i;
		my ($rcpt) = $cmd =~ /^RCPT TO:<([^>]*)>/i;
		push @rcpts, $rcpt;
		print STDERR "RCPT: $cmd\n";
		_smtp_send("250 2.1.5 Ok");
	}

	# DATA
	$cmd =~ /^DATA$/i
	    or die ref($self), " expected DATA, got: $cmd";
	_smtp_send("354 End data with <CR><LF>.<CR><LF>");

	# Receive message body until lone '.'
	my @body;
	while (1) {
		my $line = _smtp_readline();
		defined($line) or die ref($self), " unexpected EOF in message body";
		last if $line eq '.';
		$line =~ s/^\.(.)/.$1/ if $line =~ /^\.\./;  # dot-unstuff
		push @body, $line;
	}
	print STDERR "DATA: ", scalar(@body), " lines received\n";
	_smtp_send("250 2.0.0 Ok: message delivered");

	# QUIT
	$cmd = _smtp_readline();
	defined($cmd) or die ref($self), " unexpected EOF waiting for QUIT";
	$cmd =~ /^QUIT$/i
	    or die ref($self), " expected QUIT, got: $cmd";
	_smtp_send("221 2.0.0 Bye");
}

1;
