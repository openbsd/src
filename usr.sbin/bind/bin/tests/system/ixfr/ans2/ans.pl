#!/usr/bin/perl
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

# $ISC: ans.pl,v 1.2 2001/05/30 20:30:24 bwelling Exp $

#
# This is the name server from hell.  It provides canned
# responses based on pattern matching the queries, and
# can be reprogrammed on-the-fly over a TCP connection.
#
# The server listens for control connections on port 5301.
# A control connection is a TCP stream of lines like
#
#  /pattern/
#  name ttl type rdata
#  name ttl type rdata
#  ...
#  /pattern/
#  name ttl type rdata
#  name ttl type rdata
#  ...
#
# There can be any number of patterns, each associated
# with any number of response RRs.  Each pattern is a
# Perl regular expression.
#
# Each incoming query is converted into a string of the form
# "qname qtype" (the printable query domain name, space,
# printable query type) and matched against each pattern.
#
# The first pattern matching the query is selected, and
# the RR following the pattern line are sent in the
# answer section of the response.
#
# Each new control connection causes the current set of
# patterns and responses to be cleared before adding new
# ones.
#
# The server handles UDP and TCP queries.  Zone transfer
# responses work, but must fit in a single 64 k message.
#

use IO::File;
use IO::Socket;
use Net::DNS;
use Net::DNS::Packet;

my $ctlsock = IO::Socket::INET->new(LocalAddr => "10.53.0.2",
   LocalPort => 5301, Proto => "tcp", Listen => 5, Reuse => 1) or die "$!";

my $udpsock = IO::Socket::INET->new(LocalAddr => "10.53.0.2",
   LocalPort => 5300, Proto => "udp", Reuse => 1) or die "$!";

my $tcpsock = IO::Socket::INET->new(LocalAddr => "10.53.0.2",
   LocalPort => 5300, Proto => "tcp", Listen => 5, Reuse => 1) or die "$!";

my $pidf = new IO::File "ans.pid", "w" or die "cannot write pid file: $!";
print $pidf "$$\n";
$pidf->close;
sub rmpid { unlink "ans.pid"; exit 1; };

$SIG{INT} = \&rmpid;
$SIG{TERM} = \&rmpid;

my @answers = ();

sub handle {
	my ($buf) = @_;

	my ($packet, $err) = new Net::DNS::Packet(\$buf, 0);
	$err and die $err;
	
	$packet->header->qr(1);
	$packet->header->aa(1);
	
	my @questions = $packet->question;
	my $qname = $questions[0]->qname;
	my $qtype = $questions[0]->qtype;

	my $r;
	foreach $r (@rules) {
		my $pattern = $r->{pattern};
		warn "match $qname $qtype == $pattern";
		if ("$qname $qtype" =~ /$pattern/) {
			my $a;
			foreach $a (@{$r->{answer}}) {
				$packet->push("answer", $a);
			}
			last;
		}
	}

	# $packet->print;
	
	return $packet->data;
}

for (;;) {
	$rin = '';
	vec($rin, fileno($ctlsock), 1) = 1;
	vec($rin, fileno($tcpsock), 1) = 1;
	vec($rin, fileno($udpsock), 1) = 1;

	select($rout = $rin, undef, undef, undef);

	if (vec($rout, fileno($ctlsock), 1)) {
		warn "ctl conn";
		my $conn = $ctlsock->accept;
		@rules = ();
		while (my $line = $conn->getline) {
			chomp $line;
			if ($line =~ m!^/(.*)/$!) {
				$rule = { pattern => $1, answer => [] };
				push(@rules, $rule);
			} else {
				push(@{$rule->{answer}},
				     new Net::DNS::RR($line));
			}

		}
		$conn->close;
	} elsif (vec($rout, fileno($udpsock), 1)) {
		printf "UDP request\n";
		$udpsock->recv($buf, 512);
		$response = handle($buf);
		$udpsock->send($response);
	} elsif (vec($rout, fileno($tcpsock), 1)) {
		my $conn = $tcpsock->accept;
		for (;;) {
			printf "TCP request\n";
			my $n = $conn->sysread($lenbuf, 2);
			last unless $n == 2;
			my $len = unpack("n", $lenbuf);
			$n = $conn->sysread($buf, $len);
			last unless $n == $len;
			$response = handle($buf);
			$len = length($response);
			$n = $conn->syswrite(pack("n", $len), 2);
			$n = $conn->syswrite($response, $len);
		}
		$conn->close;
	}
}
