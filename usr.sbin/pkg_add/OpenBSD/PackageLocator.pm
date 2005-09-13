# ex:ts=8 sw=4:
# $OpenBSD: PackageLocator.pm,v 1.23 2005/09/13 09:30:55 espie Exp $
#
# Copyright (c) 2003-2004 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageRepository;

sub _new
{
	my ($class, $address) = @_;
	bless { baseurl => $address }, $class;
}

sub new
{
	my ($class, $baseurl) = @_;
	if ($baseurl =~ m/^ftp\:/i) {
		return OpenBSD::PackageRepository::FTP->_new($baseurl);
	} elsif ($baseurl =~ m/^http\:/i) {
		return OpenBSD::PackageRepository::HTTP->_new($baseurl);
	} elsif ($baseurl =~ m/^scp\:/i) {
		return OpenBSD::PackageRepository::SCP->_new($baseurl);
	} else {
		return OpenBSD::PackageRepository::Local->_new($baseurl);
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
	$object->deref();
}

sub make_room
{
	my $self = shift;

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
	return $already;
}

# open method that tracks opened files per-host.
sub open
{
	my ($self, $object) = @_;

	return undef unless $self->may_exist($object->{name});

	# kill old files if too many
	my $already = $self->make_room();
	my $p = $self->pipename($object->{name});
	
	open(my $fh, '-|', $p) or return undef;
	$object->{fh} = $fh;
	if (defined $already) {
		push @$already, $object;
	}
	return $fh;
}

sub openPackage
{
	my ($repository, $name, $arch) = @_;
	my $self = OpenBSD::PackageLocation->new($repository, $name);

	return $self->openPackage($name, $arch);
}

sub grabPlist
{
	my ($repository, $name, $arch, $code) = @_;
	my $self = OpenBSD::PackageLocation->new($repository, $name);

	return $self->grabPlist($name, $arch, $code);
}

package OpenBSD::PackageRepository::SCP;
our @ISA=qw(OpenBSD::PackageRepository OpenBSD::PackageRepository::FTPorSCP);

our %distant = ();

sub maxcount
{
	return 2;
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
	my ($class, $baseurl) = @_;
	$baseurl =~ s/scp\:\/\///i;
	$baseurl =~ m/\//;
	bless {	host => $`, key => $`, path => "/$'" }, $class;
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
	if (!defined $self->{list}) {
		my $host = $self->{host};
		my $path = $self->{path};
		$self->{list} = $self->_list("ssh $host ls -l $path");
	}
	return $self->{list};
}

package OpenBSD::PackageRepository::Local;
our @ISA=qw(OpenBSD::PackageRepository);

sub pipename
{
	my ($self, $name) = @_;
	my $fullname = $self->{baseurl}.$name;
	return "gzip -d -c -q -f 2>/dev/null $fullname";
}

sub may_exist
{
	my ($self, $name) = @_;
	return -r $self->{baseurl}.$name;
}

sub list
{
	my $self = shift;
	my $l = [];
	my $dname = $self->{baseurl};
	opendir(my $dir, $dname) or return $l;
	while (my $e = readdir $dir) {
		next unless -f "$dname/$e";
		next unless $e =~ m/\.tgz$/;
		push(@$l, $`);
	}
	close($dir);
	return $l;
}

package OpenBSD::PackageRepository::Local::Pipe;
our @ISA=qw(OpenBSD::PackageRepository::Local);

sub may_exist
{
	return 1;
}

sub pipename
{
	return "gzip -d -c -q -f 2>/dev/null -";
}

package OpenBSD::PackageRepository::FTPorSCP;

sub _list
{
	my ($self, $cmd) = @_;
	my $l =[];
	local $_;
	open(my $fh, '-|', "$cmd") or return undef;
	while(<$fh>) {
		chomp;
		next if m/^d.*\s+\S/;
		next unless m/([^\s]+)\.tgz\s*$/;
		push(@$l, $1);
	}
	close($fh);
	return $l;
}

package OpenBSD::PackageRepository::HTTPorFTP;

our %distant = ();

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
	my ($class, $baseurl) = @_;
	my $distant_host;
	if ($baseurl =~ m/^(http|ftp)\:\/\/(.*?)\//i) {
	    $distant_host = $&;
	}
	bless { baseurl => $baseurl, key => $distant_host }, $class;
}

sub pipename
{
	my ($self, $name) = @_;
	my $fullname = $self->{baseurl}.$name;
	return "ftp -o - $fullname 2>/dev/null|gzip -d -c -q - 2>/dev/null";
}

package OpenBSD::PackageRepository::HTTP;
our @ISA=qw(OpenBSD::PackageRepository::HTTPorFTP OpenBSD::PackageRepository);
sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		$self->make_room();
		my $fullname = $self->{baseurl};
		my $l = $self->{list} = [];
		local $_;
		open(my $fh, '-|', "ftp -o - $fullname 2>/dev/null") or return undef;
		# XXX assumes a pkg HREF won't cross a line. Is this the case ?
		while(<$fh>) {
			chomp;
			for my $pkg (m/\<A\s+HREF=\"(.*?)\.tgz\"\>/gi) {
				next if $pkg =~ m|/|;
				push(@$l, $pkg);
			}
		}
		close($fh);
	}
	return $self->{list};
}

package OpenBSD::PackageRepository::FTP;
our @ISA=qw(OpenBSD::PackageRepository::HTTPorFTP OpenBSD::PackageRepository OpenBSD::PackageRepository::FTPorSCP);

sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		$self->make_room();
		my $fullname = $self->{baseurl};
		$self->{list} = $self->_list("echo nlist *.tgz|ftp -o - $fullname 2>/dev/null");
	}
	return $self->{list};
}

package OpenBSD::PackageLocation;

use OpenBSD::PackageInfo;
use OpenBSD::Temp;

sub new
{
	my ($class, $repository, $name) = @_;
	my $self = { repository => $repository, name => $name};
	bless $self, $class;
}

sub openArchive
{
	my $self = shift;

	my $fh = $self->{repository}->open($self);
	if (!defined $fh) {
		return undef;
	}
	require OpenBSD::Ustar;

	my $archive = new OpenBSD::Ustar $fh;
	$self->{_archive} = $archive;
}

sub grabInfoFiles
{
	my $self = shift;
	my $dir = $self->{dir};

	while (my $e = $self->next()) {
		if ($e->isFile() && is_info_name($e->{name})) {
			$e->{name}=$dir.$e->{name};
			eval { $e->create(); };
			if ($@) {
				unlink($e->{name});
				$@ =~ s/\s+at.*//;
				print STDERR $@;
				return 0;
			}
		} else {
			$self->unput();
			last;
		}
	}
	return 1;
}

sub grabPlist
{
	my ($self, $pkgname, $arch, $code) = @_;
	if (!$self->openArchive()) {
		return undef;
	}

	# maybe it's a fat package.
	while (my $e = $self->next()) {
		unless ($e->{name} eq CONTENTS or $e->{name} =~ m/\/\+CONTENTS$/) {
			last;
		}
		my $prefix = $`;
		my $value = $e->contents();
		open my $fh,  '<', \$value or next;
		require OpenBSD::PackingList;
		$pkgname =~ s/\.tgz$//;
		my $plist = OpenBSD::PackingList->read($fh, $code);
		close $fh;
		next if defined $pkgname and $plist->pkgname() ne $pkgname;
		if ($plist->has('arch')) {
			if ($plist->{arch}->check($arch)) {
				return $plist;
			}
		}
	}
	# hopeless
	$self->close();

	return undef;
}

sub openPackage
{
	my ($self, $pkgname, $arch) = @_;
	if (!$self->openArchive()) {
		return undef;
	}
	my $dir = OpenBSD::Temp::dir();
	$self->{dir} = $dir;

	$self->grabInfoFiles();

	if (-f $dir.CONTENTS) {
		return $self;
	} 

	# maybe it's a fat package.
	while (my $e = $self->next()) {
		unless ($e->{name} =~ m/\/\+CONTENTS$/) {
			last;
		}
		my $prefix = $`;
		$e->{name}=$dir.CONTENTS;
		eval { $e->create(); };
		require OpenBSD::PackingList;
		$pkgname =~ s/\.tgz$//;
		my $plist = OpenBSD::PackingList->fromfile($dir.CONTENTS, \&OpenBSD::PackingList::FatOnly);
		next if defined $pkgname and $plist->pkgname() ne $pkgname;
		if ($plist->has('arch')) {
			if ($plist->{arch}->check($arch)) {
				$self->{filter} = $prefix;
				bless $self, "OpenBSD::FatPackageLocation";
				$self->grabInfoFile();
				return $self;
			}
		}
	}
	# hopeless
	$self->close();

	require File::Path;
	File::Path::rmtree($dir);
	delete $self->{dir};
	return undef;
}

sub info
{
	my $self = shift;
	return $self->{dir};
}

sub close
{
	my $self = shift;
	$self->{repository}->close($self);
}

sub deref
{
	my $self = shift;
	$self->{fh} = undef;
	$self->{_archive} = undef;
}

sub reopen
{
	my $self = shift;
	if (!$self->openArchive()) {
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
		my $e = $self->{_archive}->next();
		$self->{_current} = $e;
	}
	$self->{_unput} = 0;
	return $self->{_current};
}

sub unput
{
	my $self = shift;
	$self->{_unput} = 1;
}

package OpenBSD::FatPackageLocation;
our @ISA=qw(OpenBSD::PackageLocation);

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
		my $e = $self->{_archive}->next();
		if ($e->{name} =~ m/^(.*?)\/(.*)$/) {
			my ($beg, $name) = ($1, $2);
			if (index($beg, $self->{filter}) == -1) {
				return $self->next();
			}
			$e->{name} = $name;
			if ($e->isHardLink()) {
				$e->{linkname} =~ s/^(.*?)\///;
			}
		}
		$self->{_current} = $e;
	}
	$self->{_unput} = 0;
	return $self->{_current};
}


package OpenBSD::PackageLocator;

# this returns an archive handle from an uninstalled package name, currently
# There is a cache available.

my %packages;
my @pkgpath;
my $need_new_cache = 1;
my $available_packages = {};
my @pkglist = ();


if (defined $ENV{PKG_PATH}) {
	my $pkgpath = $ENV{PKG_PATH};
	$pkgpath =~ s/^\:+//;
	$pkgpath =~ s/\:+$//;
	my @tentative = split /\/\:/, $pkgpath;
	@pkgpath = ();
	while (my $i = shift @tentative) {
		$i =~ m|/$| or $i.='/';
		push @pkgpath, OpenBSD::PackageRepository->new($i);
	}
} else {
	@pkgpath=(OpenBSD::PackageRepository->new("./"));
}

sub find
{
	my $class = shift;
	local $_ = shift;
	my $arch = shift;

	if ($_ eq '-') {
		my $repository = OpenBSD::PackageRepository::Local::Pipe->_new('./');
		my $package = $repository->openPackage(undef, $arch);
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
		my $repository = OpenBSD::PackageRepository->new($path);
		$package = $repository->openPackage($pkgname, $arch);
		if (defined $package) {
			push(@pkgpath, $repository);
			$need_new_cache = 1;
		}
	} else {
		for my $p (@pkgpath) {
			$package = $p->openPackage($_, $arch);
			last if defined $package;
		}
	}
	$packages{$_} = $package if defined($package);
	return $package;
}

sub available
{
	if ($need_new_cache) {
		$available_packages = {};
		foreach my $loc (reverse @pkgpath) {
		    foreach my $pkg (@{$loc->list()}) {
		    	$available_packages->{$pkg} = $loc;
		    }
		}
		@pkglist = keys %$available_packages;
		$need_new_cache = 0;
	}
	return @pkglist;
}

1;
