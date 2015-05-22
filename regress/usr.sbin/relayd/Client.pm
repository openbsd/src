#	$OpenBSD: Client.pm,v 1.10 2015/05/22 19:09:18 bluhm Exp $

# Copyright (c) 2010-2015 Alexander Bluhm <bluhm@openbsd.org>
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

package Client;
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
	$args{logfile} ||= "client.log";
	$args{up} ||= "Connected";
	$args{timefile} //= "time.log";
	my $self = Proc::new($class, %args);
	$self->{connectdomain}
	    or croak "$class connect domain not given";
	$self->{connectaddr}
	    or croak "$class connect addr not given";
	$self->{connectport}
	    or croak "$class connect port not given";
	return $self;
}

sub child {
	my $self = shift;

	# in case we redo the connect, shutdown the old one
	shutdown(\*STDOUT, SHUT_WR);
	delete $self->{cs};

	$SSL_ERROR = "";
	my $iosocket = $self->{ssl} ? "IO::Socket::SSL" : "IO::Socket::INET6";
	my $cs = $iosocket->new(
	    Proto		=> "tcp",
	    Domain		=> $self->{connectdomain},
	    PeerAddr		=> $self->{connectaddr},
	    PeerPort		=> $self->{connectport},
	    SSL_verify_mode	=> SSL_VERIFY_NONE,
	) or die ref($self), " $iosocket socket connect failed: $!,$SSL_ERROR";
	print STDERR "connect sock: ",$cs->sockhost()," ",$cs->sockport(),"\n";
	print STDERR "connect peer: ",$cs->peerhost()," ",$cs->peerport(),"\n";
	if ($self->{ssl}) {
		print STDERR "ssl version: ",$cs->get_sslversion(),"\n";
		print STDERR "ssl cipher: ",$cs->get_cipher(),"\n";
		print STDERR "ssl peer certificate:\n",
		    $cs->dump_peer_certificate();
	}

	*STDIN = *STDOUT = $self->{cs} = $cs;
}

1;
