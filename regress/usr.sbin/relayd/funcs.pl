#	$OpenBSD: funcs.pl,v 1.2 2011/09/02 10:45:36 bluhm Exp $

# Copyright (c) 2010,2011 Alexander Bluhm <bluhm@openbsd.org>
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
use feature 'switch';
use Errno;
use Digest::MD5;
use Socket;
use Socket6;
use IO::Socket;
use IO::Socket::INET6;

sub find_ports {
	my %args = @_;
	my $num    = delete $args{num}    // 1;
	my $domain = delete $args{domain} // AF_INET;
	my $addr   = delete $args{addr}   // "127.0.0.1";

	my @sockets = (1..$num);
	foreach my $s (@sockets) {
		$s = IO::Socket::INET6->new(
		    Proto  => "tcp",
		    Domain => $domain,
		    $addr ? (LocalAddr => $addr) : (),
		) or die "find_ports: create and bind socket failed: $!";
	}
	my @ports = map { $_->sockport() } @sockets;

	return @ports;
}

########################################################################
# Client funcs
########################################################################

sub write_char {
	my $self = shift;
	my $len = shift // $self->{len} // 251;
	my $sleep = $self->{sleep};

	my $ctx = Digest::MD5->new();
	my $char = '0';
	for (my $i = 1; $i < $len; $i++) {
		$ctx->add($char);
		print $char
		    or die ref($self), " print failed: $!";
		given ($char) {
			when(/9/)	{ $char = 'A' }
			when(/Z/)	{ $char = 'a' }
			when(/z/)	{ $char = "\n" }
			when(/\n/)	{ print STDERR "."; $char = '0' }
			default		{ $char++ }
		}
		if ($self->{sleep}) {
			IO::Handle::flush(\*STDOUT);
			sleep $self->{sleep};
		}
	}
	if ($len) {
		$char = "\n";
		$ctx->add($char);
		print $char
		    or die ref($self), " print failed: $!";
		print STDERR ".\n";
	}
	IO::Handle::flush(\*STDOUT);

	print STDERR "LEN: ", $len, "\n";
	print STDERR "MD5: ", $ctx->hexdigest, "\n";
}

sub http_client {
	my $self = shift;
	my @lengths = @{$self->{lengths} || [ shift // $self->{len} // 251 ]};
	my $vers = $self->{lengths} ? "1.1" : "1.0";

	foreach my $len (@lengths) {
		{
			local $\ = "\r\n";
			print "GET /$len HTTP/$vers";
			print "Host: foo.bar";
			print "";
		}
		IO::Handle::flush(\*STDOUT);

		{
			local $\ = "\n";
			local $/ = "\r\n";
			local $_ = <STDIN>;
			chomp;
			print STDERR;
			m{^HTTP/$vers 200 OK$}
			    or die ref($self), " http response not ok";
			while (<STDIN>) {
				chomp;
				last if /^$/;
				print STDERR;
				if (/^Content-Length: (.*)/) {
					$1 == $len or die ref($self),
					    " bad content length $1";
				}
			}
		}
		read_char($self, $vers eq "1.1" ? $len : undef);
	}
}

########################################################################
# Server funcs
########################################################################

sub read_char {
	my $self = shift;
	my $max = shift // $self->{max};

	my $ctx = Digest::MD5->new();
	my $len = 0;
	if (defined($max) && $max == 0) {
		print STDERR "Max\n";
	} else {
		while (<STDIN>) {
			$len += length($_);
			$ctx->add($_);
			print STDERR ".";
			if (defined($max) && $len >= $max) {
				print STDERR "\nMax";
				last;
			}
		}
		print STDERR "\n";
	}

	print STDERR "LEN: ", $len, "\n";
	print STDERR "MD5: ", $ctx->hexdigest, "\n";
}

sub http_server {
	my $self = shift;

	my($url, $vers);
	do {
		{
			local $\ = "\n";
			local $/ = "\r\n";
			local $_ = <STDIN>;
			return unless defined $_;
			chomp;
			print STDERR;
			($url, $vers) = m{^GET (.*) HTTP/(1\.[01])$}
			    or die ref($self), " http request not ok";
			while (<STDIN>) {
				chomp;
				last if /^$/;
				print STDERR;
			}
		}

		$url =~ /(\d+)$/;
		my $len = $1;
		{
			local $\ = "\r\n";
			print "HTTP/$vers 200 OK";
			print "Content-Length: $len" if $vers eq "1.1";
			print "";
		}
		write_char($self, $len);
		IO::Handle::flush(\*STDOUT);
	} while ($vers eq "1.1");
}

1;
