#	$OpenBSD: funcs.pl,v 1.5 2011/09/06 23:25:27 bluhm Exp $

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
	my $method = $self->{method} || "GET";

	foreach my $len (@lengths) {
		my $path = ref($len) eq 'ARRAY' ? join("/", @$len) : $len;
		{
			local $\ = "\r\n";
			print "$method /$path HTTP/$vers";
			print "Host: foo.bar";
			print "Content-Length: $len"
			    if $vers eq "1.1" && $method eq "PUT";
			print "";
		}
		write_char($self, $len) if $method eq "PUT";
		IO::Handle::flush(\*STDOUT);

		my $chunked = 0;
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
				if (/^Transfer-Encoding: chunked$/) {
					$chunked = 1;
				}
			}
		}
		if ($chunked) {
			read_chunked($self);
		} else {
			read_char($self, $vers eq "1.1" ? $len : undef)
			    if $method eq "GET";
		}
	}
}

sub read_chunked {
	my $self = shift;

	for (;;) {
		my $len;
		{
			local $\ = "\n";
			local $/ = "\r\n";
			local $_ = <STDIN>;
			defined or die ref($self), " missing chunk size";
			chomp;
			print STDERR;
			/^[[:xdigit:]]+$/
			    or die ref($self), " chunk size not hex: $_";
			$len = hex;
		}
		last unless $len > 0;
		read_char($self, $len);
		{
			local $\ = "\n";
			local $/ = "\r\n";
			local $_ = <STDIN>;
			defined or die ref($self), " missing chunk data end";
			chomp;
			/^$/ or die ref($self), " no chunk data end: $_";
		}
	}
	{
		local $\ = "\n";
		local $/ = "\r\n";
		while (<STDIN>) {
			chomp;
			last if /^$/;
			print STDERR;
		}
		defined or die ref($self), " missing chunk trailer";
	}
}

sub errignore {
	$SIG{PIPE} = 'IGNORE';
	$SIG{__DIE__} = sub {
		die @_ if $^S;
		warn @_;
		my $soerror;
		$soerror = getsockopt(STDIN, SOL_SOCKET, SO_ERROR);
		print STDERR "ERROR IN: ", unpack('i', $soerror), "\n";
		$soerror = getsockopt(STDOUT, SOL_SOCKET, SO_ERROR);
		print STDERR "ERROR OUT: ", unpack('i', $soerror), "\n";
		IO::Handle::flush(\*STDERR);
		POSIX::_exit(0);
	};
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

	my($method, $url, $vers);
	do {
		my $len;
		{
			local $\ = "\n";
			local $/ = "\r\n";
			local $_ = <STDIN>;
			return unless defined $_;
			chomp;
			print STDERR;
			($method, $url, $vers) = m{^(\w+) (.*) HTTP/(1\.[01])$}
			    or die ref($self), " http request not ok";
			$method =~ /^(GET|PUT)$/
			    or die ref($self), " unknown method: $method";
			($len, my @chunks) = $url =~ /(\d+)/g;
			$len = [ $len, @chunks ] if @chunks;
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
		read_char($self, $vers eq "1.1" ? $len : undef)
		    if $method eq "PUT";

		{
			local $\ = "\r\n";
			print "HTTP/$vers 200 OK";
			if (ref($len) eq 'ARRAY') {
				print "Transfer-Encoding: chunked"
				    if $vers eq "1.1";
			} else {
				print "Content-Length: $len"
				    if $vers eq "1.1" && $method eq "GET";
			}
			print "";
		}
		if (ref($len) eq 'ARRAY') {
			write_chunked($self, @$len);
		} else {
			write_char($self, $len) if $method eq "GET";
		}
		IO::Handle::flush(\*STDOUT);
	} while ($vers eq "1.1");
}

sub write_chunked {
	my $self = shift;
	my @chunks = @_;

	foreach my $len (@chunks) {
		printf "%x\r\n", $len;
		write_char($self, $len);
		print "\r\n";
	}
	print "0\r\n";
	print "X-Chunk-Trailer: @chunks\r\n";
	print "\r\n";
}

1;
