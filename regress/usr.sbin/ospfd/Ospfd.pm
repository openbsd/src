#	$OpenBSD: Ospfd.pm,v 1.2 2014/07/11 22:28:51 bluhm Exp $

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

package Ospfd;
use parent 'Proc';
use Carp;
use File::Basename;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "ospfd.log";
	$args{up} ||= "Started";
	$args{down} ||= "terminating";
	$args{func} = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "ospfd.conf";
	my $self = Proc::new($class, %args);

	# generate ospfd.conf from config keys in %args
	unlink $self->{conffile};
	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " conf file $self->{conffile} create failed: $!";
	my %global_conf = %{$args{conf}{global}};
	foreach my $key (keys %global_conf) {
		print $fh "$key $global_conf{$key}\n";
	}
	my %area_conf = %{$args{conf}{areas}};
	foreach my $area (keys %area_conf) {
		print $fh "area $area {\n";
		foreach my $if (keys %{${area_conf}{$area}}) {
			print $fh "\tinterface $if {\n";
			foreach my $if_opt (keys %{${area_conf}{$area}{$if}}) {
				print $fh "\t\t$if_opt "
				    . "$area_conf{$area}{$if}{$if_opt}\n";
			}
			print $fh "\t}\n";
		}
		print $fh "}\n";
	}
	close $fh;
	chmod(0600, $self->{conffile});
	my @sudo = $ENV{SUDO} ? $ENV{SUDO} : ();
	my @cmd = (@sudo, "/sbin/chown", "0:0", $self->{conffile});
	system(@cmd)
	    and die "System '@cmd' failed: $?";

	return $self;
}

sub up {
	my $self = Proc::up(shift, @_);
	my $timeout = shift || 10;
	my $regex = "startup";
	$self->loggrep(qr/$regex/, $timeout)
	    or croak ref($self), " no $regex in $self->{logfile} ".
		"after $timeout seconds";
	return $self;
}

sub child {
	my $self = shift;
	print STDERR $self->{up}, "\n";
	my @sudo = $ENV{SUDO} ? $ENV{SUDO} : ();
	my @ktrace = $ENV{KTRACE} ? ($ENV{KTRACE}, "-i") : ();
	my $ospfd = $ENV{OSPFD} ? $ENV{OSPFD} : "ospfd";
	my @cmd = (@sudo, @ktrace, $ospfd, '-d', '-v', '-f', $self->{conffile});
	print STDERR "execute: @cmd\n";
	exec @cmd;
	die "Exec @cmd failed: $!";
}

1;
