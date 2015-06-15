#	$OpenBSD: Syslogd.pm,v 1.12 2015/06/15 21:44:57 bluhm Exp $

# Copyright (c) 2010-2015 Alexander Bluhm <bluhm@openbsd.org>
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
use Time::HiRes qw(time alarm sleep);

sub new {
	my $class = shift;
	my %args = @_;
	$args{ktracefile} ||= "syslogd.ktrace";
	$args{fstatfile} ||= "syslogd.fstat";
	$args{logfile} ||= "syslogd.log";
	$args{up} ||= "syslogd: started";
	$args{down} ||= "syslogd: exiting";
	$args{up} = $args{down} = "execute:"
	    if $args{foreground} || $args{daemon};
	$args{foreground} && $args{daemon}
	    and croak "$class cannot run in foreground and as daemon";
	$args{func} = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "syslogd.conf";
	$args{outfile} ||= "file.log";
	$args{outpipe} ||= "pipe.log";
	if ($args{memory}) {
		$args{memory} = {} unless ref $args{memory};
		$args{memory}{name} ||= "memory";
		$args{memory}{size} //= 1;
	}
	my $self = Proc::new($class, %args);
	$self->{connectaddr}
	    or croak "$class connect addr not given";

	_make_abspath(\$self->{$_}) foreach (qw(conffile outfile outpipe));

	# substitute variables in config file
	my $connectdomain = $self->{connectdomain};
	my $connectaddr = $self->{connectaddr};
	my $connectproto = $self->{connectproto};
	my $connectport = $self->{connectport};

	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " create conf file $self->{conffile} failed: $!";
	print $fh "*.*\t$self->{outfile}\n";
	print $fh "*.*\t|dd of=$self->{outpipe}\n";
	my $memory = $self->{memory};
	print $fh "*.*\t:$memory->{size}:$memory->{name}\n" if $memory;
	my $loghost = $self->{loghost};
	if ($loghost) {
		$loghost =~ s/(\$[a-z]+)/$1/eeg;
	} else {
		$loghost = "\@$connectaddr";
		$loghost .= ":$connectport" if $connectport;
	}
	print $fh "*.*\t$loghost\n";
	print $fh $self->{conf} if $self->{conf};
	close $fh;

	return $self->create_out();
}

sub create_out {
	my $self = shift;

	open(my $fh, '>', $self->{outfile})
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
	my @ktrace = $ENV{KTRACE} || ();
	@ktrace = "ktrace" if $self->{ktrace} && !@ktrace;
	push @ktrace, "-i", "-f", $self->{ktracefile} if @ktrace;
	my $syslogd = $ENV{SYSLOGD} ? $ENV{SYSLOGD} : "syslogd";
	my @cmd = (@sudo, @libevent, @ktrace, $syslogd,
	    "-f", $self->{conffile});
	push @cmd, "-d" if !$self->{foreground} && !$self->{daemon};
	push @cmd, "-F" if $self->{foreground};
	push @cmd, "-V" unless $self->{cacrt};
	push @cmd, "-C", $self->{cacrt}
	    if $self->{cacrt} && $self->{cacrt} ne "default";
	push @cmd, "-s", $self->{ctlsock} if $self->{ctlsock};
	push @cmd, @{$self->{options}} if $self->{options};
	print STDERR "execute: @cmd\n";
	exec @cmd;
	die ref($self), " exec '@cmd' failed: $!";
}

sub up {
	my $self = Proc::up(shift, @_);
	my $timeout = shift || 10;

	my $end = time() + $timeout;

	while ($self->{fstat}) {
		$self->fstat();
		last unless $self->{foreground} || $self->{daemon};

		# in foreground mode and as daemon we have no debug output
		# check fstat kqueue entry to detect statup
		open(my $fh, '<', $self->{fstatfile}) or die ref($self),
		    " open $self->{fstatfile} for reading failed: $!";
		last if grep { /kqueue/ } <$fh>;
		time() < $end
		    or croak ref($self), " no 'kqueue' in $self->{fstatfile} ".
		    "after $timeout seconds";
		sleep .1;
	}
	return $self;
}

sub fstat {
	my $self = shift;

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

sub _make_abspath {
	my $file = ref($_[0]) ? ${$_[0]} : $_[0];
	if (substr($file, 0, 1) ne "/") {
		$file = getcwd(). "/". $file;
		${$_[0]} = $file if ref($_[0]);
	}
	return $file;
}

sub kill_privsep {
	return Proc::kill(@_);
}

sub kill_syslogd {
	my $self = shift;
	my $sig = shift // 'TERM';
	my $ppid = shift // $self->{pid};

	# find syslogd child of privsep parent
	my @cmd = ("ps", "-ww", "-p", $ppid, "-U", "_syslogd",
	    "-o", "pid,ppid,comm", );
	open(my $ps, '-|', @cmd)
	    or die ref($self), " open pipe from '@cmd' failed: $!";
	my @pslist;
	my @pshead = split(' ', scalar <$ps>);
	while (<$ps>) {
		s/\s+$//;
		my %h;
		@h{@pshead} = split(' ', $_, scalar @pshead);
		push @pslist, \%h;
	}
	close($ps) or die ref($self), $! ?
	    " close pipe from '@cmd' failed: $!" :
	    " command '@cmd' failed: $?";
	my @pschild =
	    grep { $_->{PPID} == $ppid && $_->{COMMAND} eq "syslogd" } @pslist;
	@pschild == 1
	    or die ref($self), " not one privsep child: ",
	    join(" ", map { $_->{PID} } @pschild);

	return Proc::kill($self, $sig, $pschild[0]{PID});
}

my $rotate_num = 0;
sub rotate {
	my $self = shift;

	$self->loggrep("bytes transferred", 1) or sleep 1;
	foreach my $name (qw(file pipe)) {
		my $file = $self->{"out$name"};
		for (my $i = $rotate_num; $i >= 0; $i--) {
			my $new = $file. ".$i";
			my $old = $file. ($i > 0 ? ".".($i-1) : "");

			rename($old, $new) or die ref($self),
			    " rename from '$old' to '$new' failed: $!";
		}
	}
	$rotate_num++;
	return $self->create_out();
};

1;
