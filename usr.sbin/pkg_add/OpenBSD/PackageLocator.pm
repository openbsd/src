# $OpenBSD: PackageLocator.pm,v 1.9 2004/03/11 18:54:06 espie Exp $
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


# by default, all objects may exist
sub may_exist
{
	return 1;
}

# by default, we don't track opened files for this key

sub opened
{
	undef;
}

sub close
{
	my ($self, $object) = @_;
	close($object->{fh}) if defined $object->{fh};
	$object->_close();
}


# open method that tracks opened files per-host.
sub open
{
	my ($self, $object) = @_;

	return undef unless $self->may_exist($object->{name});

	# kill old files if too many
	my $already = $self->opened();
	if (defined $already) {
		# gc old objects
		if (@$already >= $self->maxcount()) {
			@$already = grep { defined $_->{fh} } @$already;
		}
		while (@$already >= $self->maxcount()) {
			my $o = shift @$already;
			$self->close($o);
		}
	}

	my $p = $self->pipename($object->{name});
	
	open(my $fh, '-|', $p) or return undef;
	$object->{fh} = $fh;
	if (defined $already) {
		push @$already, $object;
	}
	return $fh;
}

# by default, we don't know how to list packages there.
sub simplelist
{
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

sub pipename
{
	my ($self, $name) = @_;
	my $host = $self->{host};
	my $path = $self->{path};
	return "scp $host:$path$name /dev/stdout 2> /dev/null|gzip -d -c -q - 2> /dev/null";
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

sub pipename
{
	my ($self, $name) = @_;
	my $fullname = $self->{location}.$name;
	return "gzip -d -c -q -f 2>/dev/null $fullname";
}

sub may_exist
{
	my ($self, $name) = @_;
	return -r $self->{location}.$name;
}

sub list
{
	my $self = shift;
	my @l = ();
	my $dname = $self->{location};
	opendir(my $dir, $dname) or return undef;
	while (my $e = readdir $dir) {
		next unless -f "$dname/$e";
		next unless $e =~ m/\.tgz$/;
		push(@l, $`);
	}
	close($dir);
	return @l;
}

sub simplelist
{
	return $_[0]->list();
}

package OpenBSD::PackageLocation::Local::Pipe;
our @ISA=qw(OpenBSD::PackageLocation::Local);

sub may_exist
{
	return 1;
}

sub pipename
{
	return "gzip -d -c -q -f 2>/dev/null -";
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

my %distant = ();

sub maxcount
{
	return 1;
}

sub opened
{
	my $self = $_[0];
	my $k = $self->{key};
	if (!defined $distant{$k}) {
		$distant{$k} = [];
	}
	return $distant{$k};
}

sub _new
{
	my ($class, $location) = @_;
	my $distant_host;
	if ($location =~ m/^(http|ftp)\:\/\/(.*?)\//i) {
	    $distant_host = $&;
	}
	bless { location => $location, key => $distant_host }, $class;
}

sub pipename
{
	my ($self, $name) = @_;
	my $fullname = $self->{location}.$name;
	return "ftp -o - $fullname 2>/dev/null|gzip -d -c -q - 2>/dev/null";
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

	if ($_ eq '-') {
		my $location = OpenBSD::PackageLocation::Local::Pipe->_new('./');
		my $package = $class->openAbsolute($location, '');
		return $package;
	}
	$_.=".tgz" unless m/\.tgz$/;
	if (exists $packages{$_}) {
		return $packages{$_};
	}
	my $package;
	if (m/\//) {
		use File::Basename;

		my ($pkgname, $path) = fileparse($_);
		my $location = OpenBSD::PackageLocation->new($path);
		$package = $class->openAbsolute($location, $pkgname);
		if (defined $package) {
			push(@pkgpath, $location);
		}
	} else {
		for my $p (@pkgpath) {
			$package = $class->openAbsolute($p, $_);
			last if defined $package;
		}
	}
	$packages{$_} = $package if defined($package);
	return $package;
}

sub available
{
	my @l = ();
	foreach my $loc (@pkgpath) {
		push(@l, $loc->simplelist());
	}
	return @l;
}

sub info
{
	my $self = shift;
	return $self->{dir};
}

sub close
{
	my $self = shift;
	$self->{location}->close($self);
}

sub _close
{
	my $self = shift;
	$self->{fh} = undef;
	$self->{_archive} = undef;
}

sub _open
{
	my $self = shift;

	my $fh = $self->{location}->open($self);
	if (!defined $fh) {
		return undef;
	}
	my $archive = new OpenBSD::Ustar $fh;
	$self->{_archive} = $archive;
}

sub openAbsolute
{
	my ($class, $location, $name) = @_;
	my $self = { location => $location, name => $name};
	bless $self, $class;

	if (!$self->_open()) {
		return undef;
	}
	my $dir = OpenBSD::Temp::dir();
	$self->{dir} = $dir;

	# check that Open worked
	while (my $e = $self->next()) {
		if ($e->isFile() && is_info_name($e->{name})) {
			$e->{name}=$dir.$e->{name};
			eval { $e->create(); }
		} else {
			$self->unput();
			last;
		}
	}
	if (-f $dir.CONTENTS) {
#		$self->close();
		return $self;
	} else {
		$self->close();
		return undef;
	}
}

sub reopen
{
	my $self = shift;
	if (!$self->_open()) {
		return undef;
	}
	while (my $e = $self->{_archive}->next()) {
		if ($e->{name} eq $self->{_current}->{name}) {
			$self->{_current} = $e;
			return $self;
		}
	}
	return undef;
}

# proxy for archive operations
sub next
{
	my $self = shift;

	if (!defined $self->{fh}) {
		if (!$self->reopen()) {
			return undef;
		}
	}
	if (!$self->{_unput}) {
		$self->{_current} = $self->{_archive}->next();
	}
	$self->{_unput} = 0;
	return $self->{_current};
}

sub unput
{
	my $self = shift;
	$self->{_unput} = 1;
}

# allows the autoloader to work correctly
sub DESTROY
{
}

1;
