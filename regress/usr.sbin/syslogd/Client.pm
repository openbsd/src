#	$OpenBSD: Client.pm,v 1.2 2014/08/25 17:55:27 bluhm Exp $

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
	return $self;
}

sub child {
	my $self = shift;

	if (defined($self->{connectdomain})) {
		my $cs = IO::Socket::INET6->new(
		    Proto               => "udp",
		    Domain              => $self->{connectdomain},
		    PeerAddr            => $self->{connectaddr},
		    PeerPort            => $self->{connectport},
		) or die ref($self), " socket connect failed: $!";
		print STDERR "connect sock: ",$cs->sockhost()," ",
		    $cs->sockport(),"\n";
		print STDERR "connect peer: ",$cs->peerhost()," ",
		    $cs->peerport(),"\n";

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
