# $OpenBSD: PackageLocator.pm,v 1.4 2003/10/31 18:42:51 espie Exp $
#
# Copyright (c) 2003 Marc Espie.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
# PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;

# XXX we don't want to load Ustar all the time
package OpenBSD::Ustar;

our $AUTOLOAD;

sub AUTOLOAD {
	eval { require OpenBSD::Ustar;
	};
	goto &$AUTOLOAD;
}

package OpenBSD::PackageLocation;

sub _new
{
	my ($class, $location) = @_;
	bless { location => $location }, $class;
}

sub new
{
	my ($class, $location) = @_;
	if ($location =~ m/^ftp\:/i) {
		return OpenBSD::PackageLocation::FTP->_new($location);
	} elsif ($location =~ m/^http\:/i) {
		return OpenBSD::PackageLocation::HTTP->_new($location);
	} elsif ($location =~ m/^scp\:/i) {
		return OpenBSD::PackageLocation::SCP->_new($location);
	} else {
		return OpenBSD::PackageLocation::Local->_new($location);
	}
}

package OpenBSD::PackageLocation::SCP;
our @ISA=qw(OpenBSD::PackageLocation OpenBSD::PackageLocation::FTPorSCP);

sub _new
{
	my ($class, $location) = @_;
	$location =~ s/scp\:\/\///i;
	$location =~ m/\//;
	bless {	host => $`, path => "/$'" }, $class;
}

sub open
{
	my ($self, $name) = @_;
	my $host = $self->{host};
	my $path = $self->{path};
	open(my $fh, '-|', "scp $host:$path$name /dev/stdout 2> /dev/null|gzip -d -c -q - 2> /dev/null") or return undef;
	return $fh;
}

sub list
{
	my ($self) = @_;
	my $host = $self->{host};
	my $path = $self->{path};
	return _list("ssh $host ls -l $path");
}

package OpenBSD::PackageLocation::Local;
our @ISA=qw(OpenBSD::PackageLocation);

sub open
{
	my ($self, $name) = @_;
	my $fullname = $self->{location}.$name;
	open(my $fh, '-|', "gzip -d -c -q 2>/dev/null $fullname") or return undef;
	return $fh;
}

sub list
{
	my $self = shift;
	my @l = ();
	opendir(my $dir, $self->{location}) or return undef;
	while (my $e = readdir $dir) {
		next unless -f "$dir/$e";
		next unless $e = ~ m/\.tgz$/;
		push(@l, $`);
	}
	close($dir);
	return @l;
}

package OpenBSD::PackageLocation::FTPorSCP;

sub _list
{
	my ($self, $cmd) = @_;
	my @l =();
	local $_;
	open(my $fh, '-|', "$cmd") or return undef;
	while(<$fh>) {
		chomp;
		next if m/^d.*\s+\S/;
		next unless m/([^\s]+)\.tgz\s*$/;
		push(@l, $1);
	}
	close($fh);
	return @l;
}

package OpenBSD::PackageLocation::HTTPorFTP;
sub open
{
	my ($self, $name) = @_;
	my $fullname = $self->{location}.$name;
	open(my $fh, '-|', "ftp -o - $fullname 2>/dev/null|gzip -d -c -q - 2>/dev/null") or return undef;
	return $fh;
}

package OpenBSD::PackageLocation::HTTP;
our @ISA=qw(OpenBSD::PackageLocation::HTTPorFTP OpenBSD::PackageLocation);
sub list
{
	my ($self) = @_;
	my $fullname = $self->{location};
	my @l =();
	local $_;
	open(my $fh, '-|', "echo ls|ftp -o - $fullname 2>/dev/null") or return undef;
	# XXX assumes a pkg HREF won't cross a line. Is this the case ?
	while(<$fh>) {
		chomp;
		for my $pkg (m/\<A\s+HREF=\"(.*?)\.tgz\"\>/gi) {
			next if $pkg =~ m|/|;
			push(@l, $pkg);
		}
	}
	close($fh);
	return @l;
}

package OpenBSD::PackageLocation::FTP;
our @ISA=qw(OpenBSD::PackageLocation::HTTPorFTP OpenBSD::PackageLocation OpenBSD::PackageLocation::FTPorSCP);

sub list
{
	my ($self) = @_;
	my $fullname = $self->{location};
	return _list("echo ls|ftp -o - $fullname 2>/dev/null");
}


package OpenBSD::PackageLocator;

# this returns an archive handle from an uninstalled package name, currently
# There is a cache available.

use OpenBSD::PackageInfo;
use OpenBSD::Temp;

my %packages;
my @pkgpath;

if (defined $ENV{PKG_PATH}) {
	my @tentative = split /\:/, $ENV{PKG_PATH};
	@pkgpath = ();
	while (my $i = shift @tentative) {
		if ($i =~ m/^(?:ftp|http|scp)$/i) {
			$i.= ":".(shift @tentative);
		}
		$i =~ m|/$| or $i.='/';
		push @pkgpath, OpenBSD::PackageLocation->new($i);
	}
} else {
	@pkgpath=(OpenBSD::PackageLocation->new("./"));
}

sub find
{
	my $class = shift;
	local $_ = shift;
	$_.=".tgz" unless m/\.tgz$/;
	if (exists $packages{$_}) {
		return $packages{$_};
	}
	my $package;
	if (m/\//) {
		use File::Basename;

		my ($pkgname, $path) = fileparse($_);
		my $location = OpenBSD::PackageLocation->new($path);
		$package = openAbsolute($location, $pkgname);
		if (defined $package) {
			push(@pkgpath, $location);
		}
	} else {
		for my $p (@pkgpath) {
			$package = openAbsolute($p, $_);
			last if defined $package;
		}
	}
	return $package unless defined $package;
	$packages{$_} = $package;
	bless $package, $class;
}

sub info
{
	my $self = shift;
	return $self->{dir};
}

sub close
{
	my $self = shift;
	close($self->{fh}) if defined $self->{fh};
	$self->{fh} = undef;
	$self->{archive} = undef;
}

sub openAbsolute
{
	my ($location, $name) = @_;
	my $fh = $location->open($name);
	if (!defined $fh) {
		return undef;
	}
	my $archive = new OpenBSD::Ustar $fh;
	my $dir = OpenBSD::Temp::dir();

	my $self = { name => $_, fh => $fh, archive => $archive, dir => $dir };
	# check that Open worked
	while (my $e = $archive->next()) {
		if ($e->isFile() && is_info_name($e->{name})) {
			$e->{name}=$dir.$e->{name};
			$e->create();
		} else {
			$archive->unput();
			last;
		}
	}
	if (-f $dir.CONTENTS) {
		return $self;
	} else {
		CORE::close($fh);
		return undef;
	}
}

# allows the autoloader to work correctly
sub DESTROY
{
}

1;
