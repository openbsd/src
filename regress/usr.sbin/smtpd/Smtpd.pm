#	$OpenBSD $

# Copyright (c) 2026 doom <doom@tagram.loom>
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

package Smtpd;
use parent 'Proc';
use Carp;
use Cwd;
use File::Basename;
use File::Copy;
use File::Temp qw/tempdir/;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile}  ||= "smtpd.log";
	$args{up}       ||= "starting";   # matches "OpenSMTPD x.y.z starting"
	$args{down}     ||= "Exiting";
	$args{func}     = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "smtpd.conf";
	$args{listenaddr}
	    or croak "$class listen addr not given";
	$args{listenport}
	    or croak "$class listen port not given";
	$args{connectaddr}
	    or croak "$class connect addr not given";
	$args{connectport}
	    or croak "$class connect port not given";

	my $self = Proc::new($class, %args);
	ref($self->{config}) eq 'ARRAY'
	    or $self->{config} = [ split("\n", $self->{config} || "") ];

	# Variables available for substitution in config lines and file content.
	my $listenaddr  = $self->{listenaddr};
	my $listenport  = $self->{listenport};
	my $connectaddr = $self->{connectaddr};
	my $connectport = $self->{connectport};
	my $objdir      = getcwd();
	my $curdir      = $ENV{'CURDIR'};

	# Some files (tables, filters) need to be readable / executables
	# by the _smtpd user, which may not traverse /usr/obj/.
	# Setup a temp directory for this stuff.
	my $tempdir = tempdir('smtpd-regress-XXXXXXXXXX', TMPDIR => 1,
	    CLEANUP => 1);
	chmod(0755, $tempdir)
	    or die ref($self), "chmod $tempdir failed: $!";

	# copy filters to $tempdir
	if (ref($self->{filter_scripts}) eq 'ARRAY'
	    && @{$self->{filter_scripts}}) {
		for my $src (@{$self->{filter_scripts}}) {
			my $dst = "$tempdir/" . basename($src);
			copy($curdir .'/'. $src, $dst)
			    or die ref($self), " copy $src -> $dst failed: $!";
			chmod(0755, $dst)
			    or die ref($self), " chmod $dst failed: $!";
		}
	}

	# Write any ancillary data files the test needs (e.g. table backends).
	# Keys are filenames relative to objdir; values are file content.
	if (ref($self->{files}) eq 'HASH') {
		while (my ($name, $content) = each %{$self->{files}}) {
			(my $c = $content) =~ s/(\$[a-z]+)/$1/eeg;
			open(my $fh, '>', "$objdir/$name")
			    or die ref($self), " file $name create failed: $!";
			print $fh $c;
			close $fh;
		}
	}

	# Setup makemaps
	if (ref($self->{makemap}) eq 'ARRAY') {
		for my $map (@{$self->{makemap}}) {
			my $dst = "$tempdir/" . basename($map) . ".db";
			system 'makemap', '-o', $dst, "$objdir/$map"
		}
	}

	# Run setup commands (e.g. makemap to build .db files) after writing
	# data files but before generating smtpd.conf.
	if (ref($self->{commands}) eq 'ARRAY') {
		for my $cmd (@{$self->{commands}}) {
			my @c = map { (my $a = $_) =~ s/(\$[a-z]+)/$1/eeg; $a } @$cmd;
			system(@c) == 0
			    or die ref($self), " command '@c' failed: $?";
		}
	}

	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " conf file $self->{conffile} create failed: $!";

	# Config lines come BEFORE the listen directive.
	#
	# Substituted variables: $listenaddr $listenport $connectaddr
	# $connectport $objdir $tempdir
	foreach my $line (@{$self->{config}}) {
		(my $l = $line) =~ s/(\$[a-z]+)/$1/eeg;
		print $fh "$l\n";
	}

	# Listen directive optionally attaching a named filter chain.
	print $fh "listen on $listenaddr port $listenport";
	if ($self->{listen_filter}) {
		print $fh " filter \"$self->{listen_filter}\"";
	}
	print $fh "\n";

	close $fh;
	return $self;
}

sub child {
	my $self = shift;
	my @sudo   = $ENV{SUDO}   ? ($ENV{SUDO})         : ();
	my @ktrace = $ENV{KTRACE} ? ($ENV{KTRACE}, "-i") : ();
	my $smtpd  = $ENV{SMTPD}  || "smtpd";
	my @cmd    = (@sudo, @ktrace, $smtpd, "-dv", "-f", $self->{conffile});
	print STDERR "execute: @cmd\n";
	exec @cmd;
	die ref($self), " exec '@cmd' failed: $!";
}

1;
