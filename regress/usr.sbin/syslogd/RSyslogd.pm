#	$OpenBSD: RSyslogd.pm,v 1.1 2014/12/28 14:08:01 bluhm Exp $

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

package RSyslogd;
use parent 'Proc';
use Carp;
use Cwd;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "rsyslogd.log";
	$args{up} ||= "calling select";
	$args{down} ||= "Clean shutdown completed";
	$args{func} = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "rsyslogd.conf";
	$args{pidfile} ||= "rsyslogd.pid";
	$args{outfile} ||= "rsyslogd.out";
	my $self = Proc::new($class, %args);

	_make_abspath(\$self->{$_}) foreach (qw(conffile pidfile outfile));

	# substitute variables in config file
	my $listendomain = $self->{listendomain}
	    or croak "$class listen domain not given";
	my $listenaddr = $self->{listenaddr}
	    or croak "$class listen address not given";
	my $listenproto = $self->{listenproto}
	    or croak "$class listen protocol not given";
	my $listenport = $self->{listenport}
	    or croak "$class listen port not given";

	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " create conf file $self->{conffile} failed: $!";
	if ($listenproto eq "udp") {
		print $fh "\$ModLoad imudp\n";
		print $fh "\$UDPServerRun $listenport\n";
	}
	if ($listenproto eq "tcp") {
		print $fh "\$ModLoad imtcp\n";
		print $fh "\$InputTCPServerRun $listenport\n";
	}
	print $fh "*.*	$self->{outfile}\n";
	print $fh $self->{conf} if $self->{conf};
	close $fh;

	unlink($self->{outfile});
	return $self;
}

sub child {
	my $self = shift;

	my @cmd = ("rsyslogd", "-dn", "-c4", "-f", $self->{conffile},
	    "-i", $self->{pidfile});
	print STDERR "execute: @cmd\n";
	exec @cmd;
	die ref($self), " exec '@cmd' failed: $!";
}

sub _make_abspath {
	my $file = ref($_[0]) ? ${$_[0]} : $_[0];
	if (substr($file, 0, 1) ne "/") {
		$file = getcwd(). "/". $file;
		${$_[0]} = $file if ref($_[0]);
	}
	return $file;
}

sub down {
	my $self = shift;

	$self->kill();
	return Proc::down($self);
}

1;
