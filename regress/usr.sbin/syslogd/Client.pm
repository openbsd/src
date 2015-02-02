#	$OpenBSD: Client.pm,v 1.3 2015/02/02 17:40:24 bluhm Exp $

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

package Client;
use parent 'Proc';
use Carp;
use IO::Socket::INET6;
use Sys::Syslog qw(:standard :extended :macros);

sub new {
	my $class = shift;
	my %args = @_;
	$args{ktracefile} ||= "client.ktrace";
	$args{logfile} ||= "client.log";
	$args{up} ||= "Openlog";
	my $self = Proc::new($class, %args);
	$self->{connectproto} ||= "udp";
	return $self;
}

sub child {
	my $self = shift;

	if (defined($self->{connectdomain})) {
		my $cs;
		if ($self->{connectdomain} == AF_UNIX) {
			$cs = IO::Socket::UNIX->new(
			    Type => SOCK_DGRAM,
			    Peer => $self->{connectpath} || "/dev/log",
			) or die ref($self), " socket unix failed: $!";
			$cs->setsockopt(SOL_SOCKET, SO_SNDBUF, 10000)
			    or die ref($self), " setsockopt failed: $!";
		} else {
			$cs = IO::Socket::INET6->new(
			    Proto               => $self->{connectproto},
			    Domain              => $self->{connectdomain},
			    PeerAddr            => $self->{connectaddr},
			    PeerPort            => $self->{connectport},
			) or die ref($self), " socket connect failed: $!";
			print STDERR "connect sock: ",$cs->sockhost()," ",
			    $cs->sockport(),"\n";
			print STDERR "connect peer: ",$cs->peerhost()," ",
			    $cs->peerport(),"\n";
		}

		*STDIN = *STDOUT = $self->{cs} = $cs;
	}

	if ($self->{logsock}) {
		setlogsock($self->{logsock})
		    or die ref($self), " setlogsock failed: $!";
	}
	# we take LOG_UUCP as it is not used nowadays
	openlog("syslogd-regress", "ndelay,perror,pid", LOG_UUCP);
}

1;
