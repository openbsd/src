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

# $ISC: send.pl,v 1.2 2001/05/30 20:30:22 bwelling Exp $

#
# Send a file to a given address and port using TCP.  Used for
# configuring the test server in ixfr/ans2/ans.pl.
#

use IO::File;
use IO::Socket;

@ARGV == 2 or die "usage: send.pl host port [file ...]\n";

my $host = shift @ARGV;
my $port = shift @ARGV;

my $sock = IO::Socket::INET->new(PeerAddr => $host, PeerPort => $port,
				 Proto => "tcp",) or die "$!";
while (<>) {
	$sock->syswrite($_, length $_);
}

$sock->close;
