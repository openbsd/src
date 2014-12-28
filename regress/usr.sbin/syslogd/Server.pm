#	$OpenBSD: Server.pm,v 1.3 2014/12/28 14:08:01 bluhm Exp $

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

package Server;
use parent 'Proc';
use Carp;
use Socket;
use Socket6;
use IO::Socket;
use IO::Socket::INET6;
use IO::Socket::SSL;

sub new {
	my $class = shift;
	my %args = @_;
	$args{ktracefile} ||= "server.ktrace";
	$args{logfile} ||= "server.log";
	$args{up} ||= "Accepted";
	my $self = Proc::new($class, %args);
	$self->{listenproto} ||= "udp";
	defined($self->{listendomain})
	    or croak "$class listen domain not given";
	$SSL_ERROR = "";
	my $iosocket = $self->{listenproto} eq "tls" ?
	    "IO::Socket::SSL" : "IO::Socket::INET6";
	my $proto = $self->{listenproto};
	$proto = "tcp" if $proto eq "tls";
	my $ls = $iosocket->new(
	    Proto		=> $proto,
	    ReuseAddr		=> 1,
	    Domain		=> $self->{listendomain},
	    $self->{listenaddr} ? (LocalAddr => $self->{listenaddr}) : (),
	    $self->{listenport} ? (LocalPort => $self->{listenport}) : (),
	    SSL_key_file	=> "server-key.pem",
	    SSL_cert_file	=> "server-cert.pem",
	    SSL_verify_mode	=> SSL_VERIFY_NONE,
	) or die ref($self), " $iosocket socket listen failed: $!,$SSL_ERROR";
	if ($self->{listenproto} eq "tcp") {
		listen($ls, 1)
		    or die ref($self), " socket failed: $!";
	}
	my $log = $self->{log};
	print $log "listen sock: ",$ls->sockhost()," ",$ls->sockport(),"\n";
	$self->{listenaddr} = $ls->sockhost() unless $self->{listenaddr};
	$self->{listenport} = $ls->sockport() unless $self->{listenport};
	$self->{ls} = $ls;
	return $self;
}

sub child {
	my $self = shift;

	my $iosocket = $self->{ssl} ? "IO::Socket::SSL" : "IO::Socket::INET6";
	my $as = $self->{ls};
	if ($self->{listenproto} ne "udp") {
		$as = $self->{ls}->accept()
		    or die ref($self), " $iosocket socket accept failed: $!";
		print STDERR "accept sock: ",$as->sockhost()," ",
		    $as->sockport(),"\n";
		print STDERR "accept peer: ",$as->peerhost()," ",
		    $as->peerport(),"\n";
	}

	*STDIN = *STDOUT = $self->{as} = $as;
}

1;
