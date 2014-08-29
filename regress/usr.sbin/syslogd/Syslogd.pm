#	$OpenBSD: Syslogd.pm,v 1.2 2014/08/29 21:55:55 bluhm Exp $

# Copyright (c) 2010-2014 Alexander Bluhm <bluhm@openbsd.org>
# Copyright (c) 2014 Florian Riehm <mail@friehm.de>
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

package Syslogd;
use parent 'Proc';
use Carp;
use Cwd;
use File::Basename;

sub new {
	my $class = shift;
	my %args = @_;
	$args{fstatfile} ||= "syslogd.fstat";
	$args{logfile} ||= "syslogd.log";
	$args{up} ||= "syslogd: started";
	$args{down} ||= "syslogd: exiting";
	$args{func} = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "syslogd.conf";
	$args{outfile} ||= "file.log";
	$args{outpipe} ||= "pipe.log";
	my $self = Proc::new($class, %args);
	$self->{connectaddr}
	    or croak "$class connect addr not given";

	_make_abspath(\$self->{$_}) foreach (qw(conffile outfile outpipe));

	# substitute variables in config file
	my $connectprotocol = $self->{connectprotocol};
	my $connectdomain = $self->{connectdomain};
	my $connectaddr = $self->{connectaddr};
	my $connectport = $self->{connectport};

	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " create conf file $self->{conffile} failed: $!";
	print $fh "*.*\t$self->{outfile}\n";
	print $fh "*.*\t|dd of=$self->{outpipe} status=none\n";
	my $loghost = $self->{loghost};
	if ($loghost) {
		$loghost =~ s/(\$[a-z]+)/$1/eeg;
	} else {
		$loghost = "\@$connectaddr";
		$loghost .= ":$connectport" if $connectport;
	}
	print $fh "*.*\t$loghost\n";
	close $fh;

	open($fh, '>', $self->{outfile})
	    or die ref($self), " create log file $self->{outfile} failed: $!";
	close $fh;

	open($fh, '>', $self->{outpipe})
	    or die ref($self), " create pipe file $self->{outpipe} failed: $!";
	close $fh;
	chmod(0666, $self->{outpipe})
	    or die ref($self), " chmod pipe file $self->{outpipe} failed: $!";

	return $self;
}

sub child {
	my $self = shift;
	my @sudo = $ENV{SUDO} ? $ENV{SUDO} : ();

	my @pkill = (@sudo, "pkill", "-x", "syslogd");
	my @pgrep = ("pgrep", "-x", "syslogd");
	system(@pkill) && $? != 256
	    and die ref($self), " system '@pkill' failed: $?";
	while ($? == 0) {
		print STDERR "syslogd still running\n";
		system(@pgrep) && $? != 256
		    and die ref($self), " system '@pgrep' failed: $?";
	}
	print STDERR "syslogd not running\n";

	my @libevent;
	foreach (qw(EVENT_NOKQUEUE EVENT_NOPOLL EVENT_NOSELECT)) {
		push @libevent, "$_=$ENV{$_}" if $ENV{$_};
	}
	push @libevent, "EVENT_SHOW_METHOD=1" if @libevent;
	my @ktrace = $ENV{KTRACE} ? ($ENV{KTRACE}, "-i") : ();
	my $syslogd = $ENV{SYSLOGD} ? $ENV{SYSLOGD} : "syslogd";
	my @cmd = (@sudo, @libevent, @ktrace, $syslogd, "-d",
	    "-f", $self->{conffile});
	push @cmd, @{$self->{options}} if $self->{options};
	print STDERR "execute: @cmd\n";
	exec @cmd;
	die ref($self), " exec '@cmd' failed: $!";
}

sub up {
	my $self = Proc::up(shift, @_);

	if ($self->{fstat}) {
		open(my $fh, '>', $self->{fstatfile}) or die ref($self),
		    " open $self->{fstatfile} for writing failed: $!";
		my @cmd = ("fstat");
		open(my $fs, '-|', @cmd)
		    or die ref($self), " open pipe from '@cmd' failed: $!";
		print $fh grep { /^\w+ *syslogd *\d+/ } <$fs>;
		close($fs) or die ref($self), $! ?
		    " close pipe from '@cmd' failed: $!" :
		    " command '@cmd' failed: $?";
		close($fh)
		    or die ref($self), " close $self->{fstatfile} failed: $!";
	}
}

sub _make_abspath {
	my $file = ref($_[0]) ? ${$_[0]} : $_[0];
	if (substr($file, 0, 1) ne "/") {
		$file = getcwd(). "/". $file;
		${$_[0]} = $file if ref($_[0]);
	}
	return $file;
}

1;
