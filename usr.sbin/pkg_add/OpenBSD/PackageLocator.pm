# ex:ts=8 sw=4:
# $OpenBSD: PackageLocator.pm,v 1.45 2005/10/22 17:44:07 espie Exp $
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
	} elsif ($baseurl =~ m/src\:/i) {
		return OpenBSD::PackageRepository::Source->_new($baseurl);
	} else {
		return OpenBSD::PackageRepository::Local->_new($baseurl);
	}
}

sub available
{
	my $self = shift;

	return @{$self->list()};
}

sub wipe_info
{
	my ($self, $pkg) = @_;

	require File::Path;

	my $dir = $pkg->{dir};
	if (defined $dir) {

	    File::Path::rmtree($dir);
	    delete $pkg->{dir};
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

# hint: 0 premature close, 1 real error. undef, normal !

sub close
{
	my ($self, $object, $hint) = @_;
	close($object->{fh}) if defined $object->{fh};
	$self->parse_problems($object->{errors}, $hint) 
	    if defined $object->{errors};
	undef $object->{errors};
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
			$self->close($o, 0);
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
	my $fh = $self->open_pipe($object);
	if (!defined $fh) {
		return undef;
	}
	$object->{fh} = $fh;
	if (defined $already) {
		push @$already, $object;
	}
	return $fh;
}

sub find
{
	my ($repository, $name, $arch, $srcpath) = @_;
	$name.=".tgz" unless $name =~ m/\.tgz$/;
	my $self = OpenBSD::PackageLocation->new($repository, $name);

	return $self->openPackage($name, $arch);
}

sub grabPlist
{
	my ($repository, $name, $arch, $code) = @_;
	$name.=".tgz" unless $name =~ m/\.tgz$/;
	my $self = OpenBSD::PackageLocation->new($repository, $name);

	return $self->grabPlist($name, $arch, $code);
}

sub parse_problems
{
	my ($self, $filename, $hint) = @_;
	CORE::open(my $fh, '<', $filename) or return;

	my $baseurl = $self->{baseurl};
	local $_;
	my $notyet = 1;
	while(<$fh>) {
		next if m/^(?:200|220|221|226|230|227|250|331|500|150)[\s\-]/;
		next if m/^EPSV command not understood/;
		next if m/^Trying [\da-f\.\:]+\.\.\./;
		next if m/^Requesting \Q$baseurl\E/;
		next if m/^Remote system type is\s+/;
		next if m/^Connected to\s+/;
		next if m/^remote\:\s+/;
		next if m/^Using binary mode to transfer files/;
		next if m/^Retrieving\s+/;
		next if m/^\d+\s+bytes\s+received\s+in/;
		next if m/^ftp: connect to address.*: No route to host/;

		if (defined $hint && $hint == 0) {
			next if m/^ftp: -: short write/;
			next if m/^421\s+/;
		}
		if ($notyet) {
			print STDERR "Error from $baseurl:\n" if $notyet;
			$notyet = 0;
		}
		print STDERR  $_;
	}
	CORE::close($fh);
	unlink $filename;
}

package OpenBSD::PackageRepository::Installed;
our @ISA=qw(OpenBSD::PackageRepository);
use OpenBSD::PackageInfo;

sub new
{
	bless {}, shift;
}

sub find
{
	my ($repository, $name, $arch, $srcpath) = @_;
	my $self;

	if (is_installed($name)) {
		$self = OpenBSD::PackageLocation->new($repository, $name);
		$self->{dir} = installed_info($name);
	}
	return $self;
}

sub grabPlist
{
	my ($repository, $name, $arch, $code) = @_;
	require OpenBSD::PackingList;
	return  OpenBSD::PackingList->from_installation($name, $code);
}

sub available
{
	return installed_packages();
}

sub list
{
	my @list = installed_packages();
	return \@list;
}

sub wipe_info
{
}

sub may_exist
{
	my ($self, $name) = @_;
	return is_installed($name);
}

package PackageRepository::Source;

sub find
{
	my ($repository, $name, $arch, $srcpath) = @_;
	my $dir;
	my $make;
	if (defined $ENV{'MAKE'}) {
		$make = $ENV{'MAKE'};
	} else {
		$make = '/usr/bin/make';
	}
	if (defined $repository->{baseurl} && $repository->{baseurl} ne '') {
		$dir = $repository->{baseurl}
	} elsif (defined $ENV{PORTSDIR}) {
		$dir = $ENV{PORTSDIR};
	} else {
		$dir = '/usr/ports';
	}
	# figure out the repository name and the pkgname
	my $pkgfile = `cd $dir && SUBDIR=$srcpath ECHO_MSG=: $make show=PKGFILE`;
	chomp $pkgfile;
	if (! -f $pkgfile) {
		system "cd $dir && SUBDIR=$srcpath $make package BULK=Yes";
	}
	if (! -f $pkgfile) {
		return undef;
	}
	$pkgfile =~ m|(.*/)([^/]*)|;
	my ($base, $fname) = ($1, $2);

	my $repo = OpenBSD::PackageRepository::Local->_new($base);
	return $repo->find($fname);
}

package OpenBSD::PackageRepository::Local;
our @ISA=qw(OpenBSD::PackageRepository);

sub open_pipe
{
	my ($self, $object) = @_;
	my $pid = open(my $fh, "-|");
	if (!defined $pid) {
		die "Cannot fork: $!";
	}
	if ($pid) {
		return $fh;
	} else {
		open STDERR, ">/dev/null";
		exec {"/usr/bin/gzip"} 
		    "gzip", 
		    "-d", 
		    "-c", 
		    "-q", 
		    "-f", 
		    $self->{baseurl}.$object->{name}
		or die "Can't run gzip";
	}
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
		next unless $e =~ m/\.tgz$/;
		next unless -f "$dname/$e";
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

sub open_pipe
{
	my ($self, $object) = @_;
	my $fullname = $self->{baseurl}.$object->{name};
	my $pid = open(my $fh, "-|");
	if (!defined $pid) {
		die "Cannot fork: $!";
	}
	if ($pid) {
		return $fh;
	} else {
		open STDERR, ">/dev/null";
		exec {"/usr/bin/gzip"} 
		    "gzip", 
		    "-d", 
		    "-c", 
		    "-q", 
		    "-f", 
		    "-"
		or die "can't run gzip";
	}
}

package OpenBSD::PackageRepository::Distant;
our @ISA=qw(OpenBSD::PackageRepository);

my $buffsize = 2 * 1024 * 1024;

sub pkg_copy
{
	my ($in, $dir, $name) = @_;

	require File::Temp;
	my $template = $name;
	$template =~ s/\.tgz$/.XXXXXXXX/;

	my ($copy, $filename) = File::Temp::tempfile($template,
	    DIR => $dir) or die "Can't write copy to cache";
	chmod 0644, $filename;
	my $handler = sub {
		my ($sig) = @_;
		unlink $filename;
		$SIG{$sig} = 'DEFAULT';
		kill $sig, $$;
	};

	{

	local $SIG{'PIPE'} =  $handler;
	local $SIG{'INT'} =  $handler;
	local $SIG{'HUP'} =  $handler;
	local $SIG{'QUIT'} =  $handler;
	local $SIG{'KILL'} =  $handler;
	local $SIG{'TERM'} =  $handler;

	my ($buffer, $n);
	# copy stuff over
	do {
		$n = sysread($in, $buffer, $buffsize);
		if (!defined $n) {
			die "Error reading\n";
		}
		syswrite $copy, $buffer;
		syswrite STDOUT, $buffer;
	} while ($n != 0);
	close($copy);
	}

	rename $filename, "$dir/$name";
}

sub open_pipe
{
	require OpenBSD::Temp;

	my ($self, $object) = @_;
	$object->{errors} = OpenBSD::Temp::file();
	my $pid = open(my $fh, "-|");
	if (!defined $pid) {
		die "Cannot fork: $!";
	}
	if ($pid) {
		return $fh;
	} else {
		open STDERR, '>', $object->{errors};

		my $pid2 = open(STDIN, "-|");

		if (!defined $pid2) {
			die "Cannot fork: $!";
		}
		if ($pid2) {
			exec {"/usr/bin/gzip"} 
			    "gzip", 
			    "-d", 
			    "-c", 
			    "-q", 
			    "-" 
			or die "can't run gzip";
		} else {
			if (defined $ENV{'PKG_CACHE'}) {
				my $pid3 = open(my $in, "-|");
				if (!defined $pid3) {
					die "Cannot fork: $!";
				}
				if ($pid3) {
					pkg_copy($in, $ENV{'PKG_CACHE'}, 
					    $object->{name});
					exit(0);
				} else {
					$self->grab_object($object);
				}
			} else {
				$self->grab_object($object);
			}
		}
	}
}

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


package OpenBSD::PackageRepository::SCP;
our @ISA=qw(OpenBSD::PackageRepository::Distant);


sub grab_object
{
	my ($self, $object) = @_;

	exec {"/usr/bin/scp"} 
	    "scp", 
	    $self->{host}.":".$self->{path}.$object->{name}, 
	    "/dev/stdout"
	or die "can't run scp";
}

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

package OpenBSD::PackageRepository::HTTPorFTP;
our @ISA=qw(OpenBSD::PackageRepository::Distant);

our %distant = ();


sub grab_object
{
	my ($self, $object) = @_;
	my $ftp = defined $ENV{'FETCH_CMD'} ? $ENV{'FETCH_CMD'} : "/usr/bin/ftp";
	exec {$ftp} 
	    "ftp", 
	    "-o", 
	    "-", $self->{baseurl}.$object->{name}
	or die "can't run ftp";
}

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


package OpenBSD::PackageRepository::HTTP;
our @ISA=qw(OpenBSD::PackageRepository::HTTPorFTP);

sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		my $error = OpenBSD::Temp::file();
		$self->make_room();
		my $fullname = $self->{baseurl};
		my $l = $self->{list} = [];
		local $_;
		open(my $fh, '-|', "ftp -o - $fullname 2>$error") or return undef;
		# XXX assumes a pkg HREF won't cross a line. Is this the case ?
		while(<$fh>) {
			chomp;
			for my $pkg (m/\<A\s+HREF=\"(.*?)\.tgz\"\>/gi) {
				next if $pkg =~ m|/|;
				push(@$l, $pkg);
			}
		}
		close($fh);
		$self->parse_problems($error);
	}
	return $self->{list};
}

package OpenBSD::PackageRepository::FTP;
our @ISA=qw(OpenBSD::PackageRepository::HTTPorFTP);


sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		require OpenBSD::Temp;

		my $error = OpenBSD::Temp::file();
		$self->make_room();
		my $fullname = $self->{baseurl};
		$self->{list} = $self->_list("echo 'nlist *.tgz'|ftp -o - $fullname 2>$error");
		$self->parse_problems($error);
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
		$self->{repository}->parse_problems($self->{errors}) 
		    if defined $self->{errors};
		undef $self->{errors};
		return undef;
	}
	require OpenBSD::Ustar;

	my $archive = new OpenBSD::Ustar $fh;
	$self->{_archive} = $archive;
}

sub grabInfoFiles
{
	my $self = shift;
	my $dir = $self->{dir} = OpenBSD::Temp::dir();

	if (defined $self->{contents} && ! -f $dir.CONTENTS) {
		open my $fh, '>', $dir.CONTENTS or die "Permission denied";
		print $fh $self->{contents};
		close $fh;
	}

	while (my $e = $self->intNext()) {
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

sub scanPackage
{
	my $self = shift;
	while (my $e = $self->intNext()) {
		if ($e->isFile() && is_info_name($e->{name})) {
			if ($e->{name} eq CONTENTS && !defined $self->{dir}) {
				$self->{contents} = $e->contents();
				last;
			}
			if (!defined $self->{dir}) {
				$self->{dir} = OpenBSD::Temp::dir();
			}
			$e->{name}=$self->{dir}.$e->{name};
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

	my $pkg = $self->openPackage($pkgname, $arch);
	if (defined $pkg) {
		my $plist = $self->plist($code);
		$pkg->wipe_info();
		$pkg->close(0);
		return $plist;
	} else {
		return undef;
	}
}

sub openPackage
{
	my ($self, $pkgname, $arch) = @_;
	if (!$self->openArchive()) {
		return undef;
	}
	$self->scanPackage();

	if (defined $self->{contents}) {
		return $self;
	} 

	# maybe it's a fat package.
	while (my $e = $self->intNext()) {
		unless ($e->{name} =~ m/\/\+CONTENTS$/) {
			last;
		}
		my $prefix = $`;
		my $contents = $e->contents();
		require OpenBSD::PackingList;

		$pkgname =~ s/\.tgz$//;

		my $plist = OpenBSD::PackingList->fromfile(\$contents, 
		    \&OpenBSD::PackingList::FatOnly);
		next if defined $pkgname and $plist->pkgname() ne $pkgname;
		if ($plist->has('arch')) {
			if ($plist->{arch}->check($arch)) {
				$self->{filter} = $prefix;
				bless $self, "OpenBSD::FatPackageLocation";
				$self->{contents} = $contents;
				return $self;
			}
		}
	}
	# hopeless
	$self->close(1);
	$self->wipe_info();
	return undef;
}

sub wipe_info
{
	my $self = shift;
	$self->{repository}->wipe_info($self);
}

sub info
{
	my $self = shift;
	if (!defined $self->{dir}) {
		$self->grabInfoFiles();
	}
	return $self->{dir};
}

sub plist
{
	my ($self, $code) = @_;

	require OpenBSD::PackingList;

	if (defined $self->{contents}) {
		my $value = $self->{contents};
		return OpenBSD::PackingList->fromfile(\$value, $code);
	} elsif (defined $self->{dir} && -f $self->{dir}.CONTENTS) {
		return OpenBSD::PackingList->fromfile($self->{dir}.CONTENTS, 
		    $code);
	}
	# hopeless
	$self->close(1);

	return undef;
}

sub close
{
	my ($self, $hint) = @_;
	$self->{repository}->close($self, $hint);
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

	if (!defined $self->{dir}) {
		$self->grabInfoFiles();
	}
	return $self->intNext();
}

sub intNext
{
	my $self = shift;

	if (!defined $self->{fh}) {
		if (!$self->reopen()) {
			return undef;
		}
	}
	if (!$self->{_unput}) {
		$self->{_current} = $self->getNext();
	}
	$self->{_unput} = 0;
	return $self->{_current};
}

sub unput
{ 	
	my $self = shift;
	$self->{_unput} = 1;
}

sub getNext
{
	my $self = shift;

	return $self->{_archive}->next();
}

package OpenBSD::FatPackageLocation;
our @ISA=qw(OpenBSD::PackageLocation);

sub getNext
{
	my $self = shift;

	my $e = $self->SUPER::getNext();
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
	return $e;
}

package OpenBSD::PackageRepositoryList;

sub new
{
	my $class = shift;
	return bless {list => [], avail => undef }, $class;
}

sub add
{
	my $self = shift;
	push @{$self->{list}}, @_;
	if (@_ > 0) {
		$self->{avail} = undef;
	}
}

sub find
{
	my ($self, $pkgname, $arch, $srcpath) = @_;

	for my $repo (@{$self->{list}}) {
		my $pkg = $repo->find($pkgname, $arch, $srcpath);
		return $pkg if defined $pkg;
	}
	return undef;
}

sub grabPlist
{
	my ($self, $pkgname, $arch, $code) = @_;

	for my $repo (@{$self->{list}}) {
		my $plist = $repo->grabPlist($pkgname, $arch, $code);
		return $plist if defined $plist;
	}
	return undef;
}

sub available
{
	my $self = shift;

	if (!defined $self->{avail}) {
		my $available_packages = {};
		foreach my $loc (reverse @{$self->{list}}) {
		    foreach my $pkg (@{$loc->list()}) {
		    	$available_packages->{$pkg} = $loc;
		    }
		}
		$self->{avail} = $available_packages;
	}
	return keys %{$self->{avail}};
}

package OpenBSD::PackageLocator;

# this returns an archive handle from an uninstalled package name, currently
# There is a cache available.

my %packages;
my $pkgpath = OpenBSD::PackageRepositoryList->new();

if (defined $ENV{PKG_PATH}) {
	my $v = $ENV{PKG_PATH};
	$v =~ s/^\:+//;
	$v =~ s/\:+$//;
	my @tentative = split /\/\:/, $v;
	while (my $i = shift @tentative) {
		$i =~ m|/$| or $i.='/';
		$pkgpath->add(OpenBSD::PackageRepository->new($i));
	}
} else {
	$pkgpath->add(OpenBSD::PackageRepository->new("./"));
}

sub find
{
	my $class = shift;
	local $_ = shift;
	my $arch = shift;
	my $srcpath = shift;

	if ($_ eq '-') {
		my $repository = OpenBSD::PackageRepository::Local::Pipe->_new('./');
		my $package = $repository->find(undef, $arch, $srcpath);
		return $package;
	}
	if (exists $packages{$_}) {
		return $packages{$_};
	}
	my $package;
	if (m/\//) {
		use File::Basename;

		my ($pkgname, $path) = fileparse($_);
		my $repository = OpenBSD::PackageRepository->new($path);
		$package = $repository->find($pkgname, $arch, $srcpath);
		if (defined $package) {
			$pkgpath->add($repository);
		}
	} else {
		$package = $pkgpath->find($_, $arch, $srcpath);
	}
	$packages{$_} = $package if defined($package);
	return $package;
}

sub available
{
	return $pkgpath->available();
}

sub grabPlist
{
	my $class = shift;
	local $_ = shift;
	my $arch = shift;
	my $code = shift;

	if ($_ eq '-') {
		my $repository = OpenBSD::PackageRepository::Local::Pipe->_new('./');
		my $plist = $repository->grabPlist(undef, $arch, $code);
		return $plist;
	}
	my $plist;
	if (m/\//) {
		use File::Basename;

		my ($pkgname, $path) = fileparse($_);
		my $repository = OpenBSD::PackageRepository->new($path);
		$plist = $repository->grabPlist($pkgname, $arch, $code);
		if (defined $plist) {
			$pkgpath->add($repository);
		}
	} else {
		$plist = $pkgpath->grabPlist($_, $arch, $code);
	}
	return $plist;
}

1;
