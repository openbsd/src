#	$OpenBSD: Packet.pm,v 1.2 2017/09/04 22:40:01 bluhm Exp $

# Copyright (c) 2010-2017 Alexander Bluhm <bluhm@openbsd.org>
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

package Packet;
use parent 'Proc';
use Carp;
use Socket;
use Socket6;
use IO::Socket;
use IO::Socket::INET6;

use constant IPPROTO_DIVERT => 258;
use constant IP_DIVERTFL => 0x1022;
use constant IPPROTO_DIVERT_RESP => 0x1;
use constant IPPROTO_DIVERT_INIT => 0x2;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "packet.log";
	$args{up} ||= "Bound";
	$args{down} ||= "Shutdown $class";
	my $self = Proc::new($class, %args);
	$self->{domain}
	    or croak "$class domain not given";
	my $ds = do { local $> = 0; IO::Socket::INET6->new(
	    Type	=> Socket::SOCK_RAW,
	    Proto	=> IPPROTO_DIVERT,
	    Domain	=> $self->{domain},
	) } or die ref($self), " socket failed: $!";
	my $sa;
	$sa = pack_sockaddr_in($self->{bindport}, Socket::INADDR_ANY)
	    if $self->{af} eq "inet";
	$sa = pack_sockaddr_in6($self->{bindport}, Socket::IN6ADDR_ANY)
	    if $self->{af} eq "inet6";
	$ds->bind($sa)
	    or die ref($self), " bind failed: $!";
	my $log = $self->{log};
	print $log "divert sock: ",$ds->sockhost()," ",$ds->sockport(),"\n";
	$self->{divertaddr} = $ds->sockhost();
	$self->{divertport} = $ds->sockport();
	my $divertdir = 0;
	$divertdir |= IPPROTO_DIVERT_INIT if $self->{divertinit};
	$divertdir |= IPPROTO_DIVERT_RESP if $self->{divertresp};
	my $level = $self->{af} eq "inet" ? IPPROTO_IP :
	    $self->{af} eq "inet6" ? IPPROTO_IPV6 : undef;
	if ($divertdir) {
		setsockopt($ds, $level, IP_DIVERTFL, pack('i', $divertdir))
		    or die ref($self), " set divert flag failed: $!";
	}
	$self->{ds} = $ds;
	return $self;
}

sub child {
	my $self = shift;
	my $ds = $self->{ds};

	open(STDIN, '<&', $ds)
	    or die ref($self), " dup STDIN failed: $!";
	open(STDOUT, '>&', $ds)
	    or die ref($self), " dup STDOUT failed: $!";
}

1;
