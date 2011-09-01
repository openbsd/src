#	$OpenBSD: Relayd.pm,v 1.1 2011/09/01 17:33:17 bluhm Exp $

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

package Relayd;
use parent 'Proc';
use Carp;
use File::Basename;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "relayd.log";
	$args{up} ||= "Started";
	$args{down} ||= "parent terminating";
	$args{func} = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "relayd.conf";
	$args{forward}
	    or croak "$class forward not given";
	my $self = Proc::new($class, %args);
	ref($self->{protocol}) eq 'ARRAY'
	    or $self->{protocol} = [ split("\n", $self->{protocol} || "") ];
	ref($self->{relay}) eq 'ARRAY'
	    or $self->{relay} = [ split("\n", $self->{relay} || "") ];
	$self->{listenaddr}
	    or croak "$class listen addr not given";
	$self->{listenport}
	    or croak "$class listen port not given";
	$self->{connectaddr}
	    or croak "$class connect addr not given";
	$self->{connectport}
	    or croak "$class connect port not given";

	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " conf file $self->{conffile} create failed: $!";
	my $test = basename($self->{test} || "");

	my @protocol = @{$self->{protocol}};
	my $proto = shift @protocol;
	$proto = defined($proto) ? "$proto " : "";
	unshift @protocol,
	    $self->{forward} eq "splice" ? "tcp splice" :
	    $self->{forward} eq "copy"   ? "tcp no splice" :
	    die ref($self), " invalid forward $self->{forward}"
	    unless grep { /splice/ } @protocol;
	print $fh "${proto}protocol proto-$test {";
	print $fh  map { "\n\t$_" } @protocol;
	print $fh  "\n}\n";

	my @relay = @{$self->{relay}};
	print $fh  "relay relay-$test {";
	print $fh  "\n\tprotocol proto-$test"
	    unless grep { /^protocol / } @relay;
	my $ssl = $self->{listenssl} ? " ssl" : "";
	print $fh  "\n\tlisten on $self->{listenaddr} ".
	    "port $self->{listenport}$ssl" unless grep { /^listen / } @relay;
	my $withssl = $self->{forwardssl} ? " with ssl" : "";
	print $fh  "\n\tforward$withssl to $self->{connectaddr} ".
	    "port $self->{connectport}" unless grep { /^forward / } @relay;
	print $fh  map { "\n\t$_" } @relay;
	print $fh  "\n}\n";

	return $self;
}

sub up {
	my $self = Proc::up(shift, @_);
	my $timeout = shift || 10;
	my $lsock = $self->loggrep(qr/relay_launch: /, $timeout)
	    or croak ref($self), " no relay_launch in $self->{logfile} ".
		"after $timeout seconds";
	return $self;
}

sub down {
	my $self = shift;
	my @sudo = $ENV{SUDO} || ();
	my @cmd = (@sudo, '/bin/kill', $self->{pid});
	system(@cmd);
	return Proc::down($self, @_);
}

sub child {
	my $self = shift;
	print STDERR $self->{up}, "\n";
	my @sudo = $ENV{SUDO} || ();
	my $relayd = $ENV{RELAYD} || "relayd";
	my @cmd = (@sudo, $relayd, '-dvv', '-f', $self->{conffile});
	exec @cmd;
	die "Exec @cmd failed: $!";
}

1;
