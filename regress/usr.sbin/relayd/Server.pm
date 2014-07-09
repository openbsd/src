#	$OpenBSD: Server.pm,v 1.5 2014/07/09 16:48:55 reyk Exp $

# Copyright (c) 2010-2012 Alexander Bluhm <bluhm@openbsd.org>
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
use Socket qw(IPPROTO_TCP TCP_NODELAY);
use Socket6;
use IO::Socket;
use IO::Socket::INET6;
use IO::Socket::SSL;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "server.log";
	$args{up} ||= "Accepted";
	my $self = Proc::new($class, %args);
	$self->{listendomain}
	    or croak "$class listen domain not given";
	$SSL_ERROR = "";
	my $iosocket = $self->{ssl} ? "IO::Socket::SSL" : "IO::Socket::INET6";
	my $ls = $iosocket->new(
	    Proto		=> "tcp",
	    ReuseAddr		=> 1,
	    Domain		=> $self->{listendomain},
	    Listen		=> 1,
	    $self->{listenaddr} ? (LocalAddr => $self->{listenaddr}) : (),
	    $self->{listenport} ? (LocalPort => $self->{listenport}) : (),
	    SSL_key_file	=> "server-key.pem",
	    SSL_cert_file	=> "server-cert.pem",
	    SSL_verify_mode	=> SSL_VERIFY_NONE,
	) or die ref($self), " $iosocket socket listen failed: $!,$SSL_ERROR";
	my $log = $self->{log};
	print $log "listen sock: ",$ls->sockhost()," ",$ls->sockport(),"\n";
	$self->{listenaddr} = $ls->sockhost() unless $self->{listenaddr};
	$self->{listenport} = $ls->sockport() unless $self->{listenport};
	$self->{ls} = $ls;
	return $self;
}

sub child {
	my $self = shift;

	if ($self->{mreqs}) {
		print STDERR "connection per request\n";
		return;
	}
	my $iosocket = $self->{ssl} ? "IO::Socket::SSL" : "IO::Socket::INET6";
	my $as = $self->{ls}->accept()
	    or die ref($self), " $iosocket socket accept failed: $!";
	print STDERR "accept sock: ",$as->sockhost()," ",$as->sockport(),"\n";
	print STDERR "accept peer: ",$as->peerhost()," ",$as->peerport(),"\n";
	print STDERR "single connection\n";

	*STDIN = *STDOUT = $self->{as} = $as;
}

1;
