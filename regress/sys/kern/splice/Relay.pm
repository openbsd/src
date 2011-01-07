#	$OpenBSD: Relay.pm,v 1.1 2011/01/07 22:06:08 bluhm Exp $

# Copyright (c) 2010 Alexander Bluhm <bluhm@openbsd.org>
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

package Relay;
use parent 'Proc';
use Carp;
use Socket qw(IPPROTO_TCP TCP_NODELAY);
use Socket6;
use IO::Socket;
use IO::Socket::INET6;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "relay.log";
	$args{up} ||= "Connected";
	my $self = Proc::new($class, %args);
	$self->{listendomain}
	    or croak "$class listen domain not given";
	$self->{connectdomain}
	    or croak "$class connect domain not given";
	$self->{connectaddr}
	    or croak "$class connect addr not given";
	$self->{connectport}
	    or croak "$class connect port not given";
	my $ls = IO::Socket::INET6->new(
	    Proto	=> "tcp",
	    ReuseAddr	=> 1,
	    Domain	=> $self->{listendomain},
	    $self->{listenaddr} ? (LocalAddr => $self->{listenaddr}) : (),
	    $self->{listenport} ? (LocalPort => $self->{listenport}) : (),
	) or die ref($self), " socket failed: $!";
	if ($self->{oobinline}) {
		setsockopt($ls, SOL_SOCKET, SO_OOBINLINE, pack('i', 1))
		    or die ref($self), " set oobinline listen failed: $!";
	}
	if ($self->{sndbuf}) {
		setsockopt($ls, SOL_SOCKET, SO_SNDBUF,
		    pack('i', $self->{sndbuf}))
		    or die ref($self), " set sndbuf listen failed: $!";
	}
	if ($self->{rcvbuf}) {
		setsockopt($ls, SOL_SOCKET, SO_RCVBUF,
		    pack('i', $self->{rcvbuf}))
		    or die ref($self), " set rcvbuf listen failed: $!";
	}
	setsockopt($ls, IPPROTO_TCP, TCP_NODELAY, pack('i', 1))
	    or die ref($self), " set nodelay listen failed: $!";
	listen($ls, 1)
	    or die ref($self), " listen failed: $!";
	my $log = $self->{log};
	print $log "listen sock: ",$ls->sockhost()," ",$ls->sockport(),"\n";
	$self->{listenaddr} = $ls->sockhost() unless $self->{listenaddr};
	$self->{listenport} = $ls->sockport() unless $self->{listenport};
	$self->{ls} = $ls;
	return $self;
}

sub child {
	my $self = shift;

	my $as = $self->{ls}->accept()
	    or die ref($self), " socket accept failed: $!";
	print STDERR "accept sock: ",$as->sockhost()," ",$as->sockport(),"\n";
	print STDERR "accept peer: ",$as->peerhost()," ",$as->peerport(),"\n";
	$as->blocking($self->{nonblocking} ? 0 : 1)
	    or die ref($self), " non-blocking accept failed: $!";

	open(STDIN, '<&', $as)
	    or die ref($self), " dup STDIN failed: $!";
	print STDERR "Accepted\n";

	my $cs = IO::Socket::INET6->new(
	    Proto	=> "tcp",
	    Domain	=> $self->{connectdomain},
	    Blocking	=> ($self->{nonblocking} ? 0 : 1),
	) or die ref($self), " socket connect failed: $!";
	if ($self->{oobinline}) {
		setsockopt($cs, SOL_SOCKET, SO_OOBINLINE, pack('i', 1))
		    or die ref($self), " set oobinline connect failed: $!";
	}
	if ($self->{sndbuf}) {
		setsockopt($cs, SOL_SOCKET, SO_SNDBUF,
		    pack('i', $self->{sndbuf}))
		    or die ref($self), " set sndbuf connect failed: $!";
	}
	if ($self->{rcvbuf}) {
		setsockopt($cs, SOL_SOCKET, SO_RCVBUF,
		    pack('i', $self->{rcvbuf}))
		    or die ref($self), " set rcvbuf connect failed: $!";
	}
	setsockopt($cs, IPPROTO_TCP, TCP_NODELAY, pack('i', 1))
	    or die ref($self), " set nodelay connect failed: $!";
	my @rres = getaddrinfo($self->{connectaddr}, $self->{connectport},
	    $self->{connectdomain}, SOCK_STREAM);
	$cs->connect($rres[3])
	    or die ref($self), " connect failed: $!";
	print STDERR "connect sock: ",$cs->sockhost()," ",$cs->sockport(),"\n";
	print STDERR "connect peer: ",$cs->peerhost()," ",$cs->peerport(),"\n"
	    unless $self->{nonblocking};

	open(STDOUT, '>&', $cs)
	    or die ref($self), " dup STDOUT failed: $!";
}

1;
