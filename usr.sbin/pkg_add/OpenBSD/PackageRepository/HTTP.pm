#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: HTTP.pm,v 1.6 2011/07/18 20:21:40 espie Exp $
#
# Copyright (c) 2011 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Repository::HTTP;
sub urlscheme
{
	return 'http';
}

sub initiate
{
	my $self = shift;
	my ($rdfh, $wrfh);
	pipe($self->{getfh}, $rdfh);
	pipe($wrfh, $self->{cmdfh});
	my $pid = fork();
	if ($pid == 0) {
		close($self->{getfh});
		close($self->{cmdfh});
		close(STDOUT);
		close(STDIN);
		open(STDOUT, '>&', $wrfh);
		open(STDIN, '<&', $rdfh);
		_Proxy::main($self);
	} else {
		close($rdfh);
		close($wrfh);
		$self->{controller} = $pid;
	}
}

package _Proxy::Connection;
sub new
{
	my ($class, $host, $port) = @_;
	require IO::Socket::INET;
	my $o = IO::Socket::INET->new(
		PeerHost => $host,
		PeerPort => $port);
	bless {fh => $o, host => $host, buffer => ''}, $class;
}

sub getline
{
	my $self = shift;
	while (1) {
		if ($self->{buffer} =~ s/^(.*?)\015\012//) {
			return $1;
		}
		my $buffer;
		$self->{fh}->recv($buffer, 1024);
		$self->{buffer}.=$buffer;
    	}
}

sub retrieve
{
	my ($self, $sz) = @_;
	while(length($self->{buffer}) < $sz) {
		my $buffer;
		$self->{fh}->recv($buffer, $sz - length($self->{buffer}));
		$self->{buffer}.=$buffer;
	}
	my $result= substr($self->{buffer}, 0, $sz);
	$self->{buffer} = substr($self->{buffer}, $sz);
	return $result;
}

sub retrieve_chunked
{
	my $self = shift;
	my $result = '';
	while (1) {
		my $sz = $self->getline;
		if ($sz =~ m/^([0-9a-fA-F]+)/) {
			my $realsize = hex($1);
			last if $realsize == 0;
			$result .= $self->retrieve($realsize);
		}
	}
	return $result;
}

sub retrieve_response
{
	my ($self, $h) = @_;

	if ($h->{'Transfer-Encoding'} eq 'chunked') {
		return $self->retrieve_chunked;
	}
	if (defined $h->{'Content-Length'}) {
		return $self->retrieve($h->{'Content-Length'});
	}
	return undef;
}

sub print
{
	my ($self, @l) = @_;
	print {$self->{fh}} @l;
}

package _Proxy;

my $pid;
my $token = 0;

sub batch(&)
{
	my $code = shift;
	if (defined $pid) {
		waitpid($pid, 0);
		undef $pid;
	}
	$token++;
	$pid = fork();
	if (!defined $pid) {
		print "ERROR: fork failed: $!\n";
	}
	if ($pid == 0) {
		&$code();
		exit(0);
	}
}

sub abort_batch()
{
	if (defined $pid) {
		kill 1, $pid;
		waitpid($pid, 0);
		undef $pid;
	}
	print "\nABORTED $token\n";
}

sub get_directory
{
	my ($o, $dname) = @_;
	my $crlf="\015\012";
	$o->print("GET $dname/ HTTP/1.1", $crlf,
	    "Host: ", $o->{host}, $crlf, $crlf);
	# get header

	my $_ = $o->getline;
	if (!m,^HTTP/1\.1\s+(\d\d\d),) {
		print "ERROR\n";
		return;
	}
	my $code = $1;
	my $h = {};
	while ($_ = $o->getline) {
		last if m/^$/;
		if (m/^([\w\-]+)\:\s*(.*)$/) {
			print STDERR "$1 => $2\n";
			$h->{$1} = $2;
		} else {
			print STDERR "unknown line: $_\n";
		}
	}
	my $r = $o->retrieve_response($h);
	if (!defined $r) {
		print "ERROR: can't decode response\n";
	}
	if ($code != 200) {
			print "ERROR: code was $code\n";
			return;
	}
	print "SUCCESS: directory $dname\n";
	for my $pkg ($r =~ m/\<A\s+HREF=\"(.+?)\.tgz\"\>/gio) {
		$pkg = $1 if $pkg =~ m|^.*/(.*)$|;
		# decode uri-encoding; from URI::Escape
		$pkg =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
		print $pkg, "\n";
	}
	print "\n";
	return;
}

sub get_file
{
	my ($o, $fname) = @_;
	my $crlf="\015\012";
	$o->print("GET $fname HTTP/1.1", $crlf,
	    "Host: ", $o->{host}, $crlf, $crlf);
	# get header

	my $_ = $o->getline;
	if (!m,^HTTP/1\.1\s+(\d\d\d),) {
		print "ERROR\n";
		return;
	}
	my $code = $1;
	my $h = {};
	while ($_ = $o->getline) {
		last if m/^$/;
		if (m/^([\w\-]+)\:\s*(.*)$/) {
			print STDERR "$1 => $2\n";
			$h->{$1} = $2;
		} else {
			print STDERR "unknown line: $_\n";
		}
	}
	my $r = $o->retrieve_response($h);
	if (!defined $r) {
		print "ERROR: can't decode response\n";
	}
	if ($code != 200) {
		print "ERROR: code was $code\n";
		return;
	}
}

sub main
{
	my $self = shift;
	my $o = _Proxy::Connection->new($self->{host}, "www");
	while (<STDIN>) {
		chomp;
		if (m/^LIST\s+(.*)$/o) {
			my $dname = $1;
			batch(sub {get_directory($o, $dname);});
		} elsif (m/^GET\s+(.*)$/o) {
			my $fname = $1;
			batch(sub { get_file($o, $fname);});
		} elsif (m/^BYE$/o) {
			exit(0);
		} elsif (m/^ABORT$/o) {
			abort_batch();
		} else {
			print "ERROR: Unknown command\n";
		}
	}
}


sub todo
{
	my ($o, $file) = @_;
	my $crlf="\015\012";
	open my $fh, '>', $file;

	my $start = 0;
	my $end = 4000;
	my $total_size = 0;

	do {
		$end *= 2;
		$o->print("GET /pub/OpenBSD/snapshots/packages/amd64/$file HTTP/1.1$crlf",
    "Host: www.w3.org$crlf",
		"Range: bytes=",$start, "-", $end-1, $crlf, $crlf);

		# get header

		my $_ = $o->getline;
		if (m,^HTTP/1\.1\s+(\d\d\d),) {
			my $code = $1;
			print "Code: $code\n";
		} else {
			print $_, "\n";
		}
		my $h = {};
		while ($_ = $o->getline) {
			last if m/^$/;
			if (m/^([\w\-]+)\:\s*(.*)$/) {
				print "$1 => $2\n";
				$h->{$1} = $2;
			} else {
				print "unknown line: $_\n";
			}
		}

		if (defined $h->{'Content-Range'} && $h->{'Content-Range'} =~ 
			m/^bytes\s+\d+\-\d+\/(\d+)/) {
				$total_size = $1;
		}
		print "END OF HEADER\n";

		if (defined $h->{'Content-Length'}) {
			my $v = $o->retrieve($h->{'Content-Length'});
			print $fh $v;
		}
		$start = $end;
	} while ($end < $total_size);
}

1;
