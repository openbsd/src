#	$OpenBSD: Switchd.pm,v 1.1 2017/06/22 20:06:14 bluhm Exp $

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

package Switchd;
use parent 'Proc';
use Carp;
use Cwd;
use Sys::Hostname;
use File::Basename;

sub new {
	my $class = shift;
	my %args = @_;
	$args{logfile} ||= "switchd.log";
	$args{up} ||= $args{dryrun} || "listen on ";
	$args{down} ||= $args{dryrun} ? "switchd.conf:" : "parent terminating";
	$args{func} = sub { Carp::confess "$class func may not be called" };
	$args{conffile} ||= "switchd.conf";
	my $self = Proc::new($class, %args);
	$self->{listenaddr}
	    or croak "$class listen addr not given";
	$self->{listenport}
	    or croak "$class listen port not given";

	# substitute variables in config file
	my $curdir = dirname($0) || ".";
	my $objdir = getcwd();
	my $hostname = hostname();
	(my $host = $hostname) =~ s/\..*//;
	my $listenaddr = $self->{listenaddr};
	my $listenport = $self->{listenport};

	my $test = basename($self->{testfile} || "");
	open(my $fh, '>', $self->{conffile})
	    or die ref($self), " conf file $self->{conffile} create failed: $!";

        my $config = "# regress $test\n";
	$config .= "listen on $self->{listenaddr} port $self->{listenport}\n"
	    unless $self->{conf} && $self->{conf} =~ /^listen /;
        $config .= $self->{conf} if $self->{conf};
        $config =~ s/(\$[a-z]+)/$1/eeg;
	print $fh $config;
        close $fh;

	return $self;
}

sub child {
	my $self = shift;
	my @sudo = $ENV{SUDO} ? $ENV{SUDO} : ();
	my @ktrace = $ENV{KTRACE} ? ($ENV{KTRACE}, "-i") : ();
	my $switchd = $ENV{SWITCHD} ? $ENV{SWITCHD} : "switchd";
	my @cmd = (@sudo, @ktrace, $switchd, "-dvv", "-f", $self->{conffile});
	print STDERR "execute: @cmd\n";
	exec @cmd;
	die ref($self), " exec '@cmd' failed: $!";
}

1;
