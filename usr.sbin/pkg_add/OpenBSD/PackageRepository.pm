# ex:ts=8 sw=4:
# $OpenBSD: PackageRepository.pm,v 1.93 2010/12/24 09:04:14 espie Exp $
#
# Copyright (c) 2003-2010 Marc Espie <espie@openbsd.org>
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

# XXX load extra class, grab match from Base class, and tweak inheritance
# to get all methods.

use OpenBSD::PackageRepository::Installed;
$OpenBSD::PackageRepository::Installed::ISA = qw(OpenBSD::PackageRepository);

package OpenBSD::PackageRepository;
our @ISA=(qw(OpenBSD::PackageRepositoryBase));

use OpenBSD::PackageLocation;
use OpenBSD::Paths;

sub baseurl
{
	my $self = shift;

	return $self->{path};
}

sub new
{
	my ($class, $baseurl, $state) = @_;
	my $o = $class->parse(\$baseurl, $state);
	if ($baseurl ne '') {
		return undef;
	}
	return $o;
}

sub can_be_empty
{
	my $self = shift;
	$self->{empty_okay} = 1;
	return $self;
}

my $cache = {};

sub unique
{
	my ($class, $o) = @_;
	return $o unless defined $o;
	if (defined $cache->{$o->url}) {
		return $cache->{$o->url};
	}
	$cache->{$o->url} = $o;
	return $o;
}

my $cleanup = sub {
	for my $repo (values %$cache) {
		$repo->cleanup;
	}
};
END {
	&$cleanup;
}

OpenBSD::Handler->register($cleanup);

sub parse_fullurl
{
	my ($class, $r, $state) = @_;

	$class->strip_urlscheme($r) or return undef;
	return $class->unique($class->parse_url($r, $state));
}

sub ftp() { 'OpenBSD::PackageRepository::FTP' }
sub http() { 'OpenBSD::PackageRepository::HTTP' }
sub https() { 'OpenBSD::PackageRepository::HTTPS' }
sub scp() { 'OpenBSD::PackageRepository::SCP' }
sub source() { 'OpenBSD::PackageRepository::Source' }
sub file() { 'OpenBSD::PackageRepository::Local' }
sub installed() { 'OpenBSD::PackageRepository::Installed' }
sub pipe() { 'OpenBSD::PackageRepository::Local::Pipe' }

sub parse
{
	my ($class, $r, $state) = @_;
	my $_ = $$r;
	return undef if $_ eq '';

	if (m/^ftp\:/io) {
		return $class->ftp->parse_fullurl($r, $state);
	} elsif (m/^http\:/io) {
		return $class->http->parse_fullurl($r, $state);
	} elsif (m/^https\:/io) {
		return $class->https->parse_fullurl($r, $state);
	} elsif (m/^scp\:/io) {
		require OpenBSD::PackageRepository::SCP;

		return $class->scp->parse_fullurl($r, $state);
	} elsif (m/^src\:/io) {
		require OpenBSD::PackageRepository::Source;

		return $class->source->parse_fullurl($r, $state);
	} elsif (m/^file\:/io) {
		return $class->file->parse_fullurl($r, $state);
	} elsif (m/^inst\:$/io) {
		return $class->installed->parse_fullurl($r, $state);
	} elsif (m/^pipe\:$/io) {
		return $class->pipe->parse_fullurl($r, $state);
	} else {
		return $class->file->parse_fullurl($r, $state);
	}
}

sub available
{
	my $self = shift;

	return @{$self->list};
}

sub stemlist
{
	my $self = shift;
	if (!defined $self->{stemlist}) {
		require OpenBSD::PackageName;
		my @l = $self->available;
		if (@l == 0 && !$self->{empty_okay}) {
			$self->{state}->errsay("#1 is empty", $self->url);
		}
		$self->{stemlist} = OpenBSD::PackageName::avail2stems(@l);
	}
	return $self->{stemlist};
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
	if (defined $object->{pid2}) {
		local $SIG{ALRM} = sub {
			kill HUP => $object->{pid2};
		};
		alarm(30);
		waitpid($object->{pid2}, 0);
		alarm(0);
	}
	$self->parse_problems($object->{errors}, $hint, $object)
	    if defined $object->{errors};
	undef $object->{errors};
	$object->deref;
}

sub make_room
{
	my $self = shift;

	# kill old files if too many
	my $already = $self->opened;
	if (defined $already) {
		# gc old objects
		if (@$already >= $self->maxcount) {
			@$already = grep { defined $_->{fh} } @$already;
		}
		while (@$already >= $self->maxcount) {
			my $o = shift @$already;
			$self->close_now($o);
		}
	}
	return $already;
}

# open method that tracks opened files per-host.
sub open
{
	my ($self, $object) = @_;

	return unless $self->may_exist($object->{name});

	# kill old files if too many
	my $already = $self->make_room;
	my $fh = $self->open_pipe($object);
	if (!defined $fh) {
		return;
	}
	$object->{fh} = $fh;
	if (defined $already) {
		push @$already, $object;
	}
	return $fh;
}

sub find
{
	my ($repository, $name, $arch) = @_;
	my $self = $repository->new_location($name, $arch);

	if ($self->contents) {
		return $self;
	}
}

sub grabPlist
{
	my ($repository, $name, $arch, $code) = @_;
	my $self = $repository->new_location($name, $arch);

	return $self->grabPlist($code);
}

sub parse_problems
{
	my ($self, $filename, $hint, $object) = @_;
	unlink $filename;
}

sub cleanup
{
	# nothing to do
}

sub relative_url
{
	my ($self, $name) = @_;
	if (defined $name) {
		return $self->baseurl.$name.".tgz";
	} else {
		return $self->baseurl;
	}
}

sub add_to_list
{
	my ($self, $list, $filename) = @_;
	if ($filename =~ m/^(.*\-\d.*)\.tgz$/o) {
		push(@$list, $1);
	}
}

sub did_it_fork
{
	my ($self, $pid) = @_;
	if (!defined $pid) {
		$self->{state}->fatal("Cannot fork: #1", $!);
	}
	if ($pid == 0) {
		undef $SIG{'WINCH'};
		undef $SIG{'CONT'};
		undef $SIG{'INFO'};
	}
}

sub exec_gunzip
{
	my $self = shift;
	exec {OpenBSD::Paths->gzip}
	    ("gzip",
	    "-d",
	    "-c",
	    "-q",
	    @_)
	or $self->{state}->fatal("Can't run gzip: #1", $!);
}

package OpenBSD::PackageRepository::Local;
our @ISA=qw(OpenBSD::PackageRepository);
use OpenBSD::Error;

sub urlscheme
{
	return 'file';
}

my $pkg_db;

sub pkg_db
{
	if (!defined $pkg_db) {
		use OpenBSD::Paths;
		$pkg_db = $ENV{"PKG_DBDIR"} || OpenBSD::Paths->pkgdb;
	}
	return $pkg_db;
}

sub parse_fullurl
{
	my ($class, $r, $state) = @_;

	my $ok = $class->strip_urlscheme($r);
	my $o = $class->parse_url($r, $state);
	if (!$ok && $o->{path} eq $class->pkg_db."/") {
		return $class->installed->new(0, $state);
	} else {
		return $class->unique($o);
	}
}

# wrapper around copy, that sometimes does not copy
sub may_copy
{
	my ($self, $object, $destdir) = @_;
	my $src = $self->relative_url($object->{name});
	require File::Spec;
	my (undef, undef, $base) = File::Spec->splitpath($src);
	my $dest = File::Spec->catfile($destdir, $base);
	if (File::Spec->canonpath($dest) eq File::Spec->canonpath($src)) {
	    	return;
	}
	if (-f $dest) {
		my ($ddev, $dino) = (stat $dest)[0,1];
		my ($sdev, $sino) = (stat $src)[0, 1];
		if ($ddev == $sdev and $sino == $dino) {
			return;
		}
	}
	$self->{state}->copy_file($src, $destdir);
}

sub open_pipe
{
	my ($self, $object) = @_;
	if (defined $ENV{'PKG_CACHE'}) {
		$self->may_copy($object, $ENV{'PKG_CACHE'});
	}
	my $pid = open(my $fh, "-|");
	$self->did_it_fork($pid);
	if ($pid) {
		return $fh;
	} else {
		open STDERR, ">/dev/null";
		$self->exec_gunzip("-f", $self->relative_url($object->{name}));
	}
}

sub may_exist
{
	my ($self, $name) = @_;
	return -r $self->relative_url($name);
}

sub list
{
	my $self = shift;
	my $l = [];
	my $dname = $self->baseurl;
	opendir(my $dir, $dname) or return $l;
	while (my $e = readdir $dir) {
		next unless -f "$dname/$e";
		$self->add_to_list($l, $e);
	}
	close($dir);
	return $l;
}

package OpenBSD::PackageRepository::Local::Pipe;
our @ISA=qw(OpenBSD::PackageRepository::Local);

sub urlscheme
{
	return 'pipe';
}

sub relative_url
{
	return '';
}

sub may_exist
{
	return 1;
}

sub new
{
	my ($class, $state) = @_;
	return bless { state => $state}, $class;
}

sub open_pipe
{
	my ($self, $object) = @_;
	my $pid = open(my $fh, "-|");
	$self->did_it_fork($pid);
	if ($pid) {
		return $fh;
	} else {
		open STDERR, ">/dev/null";
		$self->exec_gunzip("-f", "-");
	}
}

package OpenBSD::PackageRepository::Distant;
our @ISA=qw(OpenBSD::PackageRepository);

sub baseurl
{
	my $self = shift;

	return "//$self->{host}$self->{path}";
}

sub parse_url
{
	my ($class, $r, $state) = @_;
	# same heuristics as ftp(1):
	# find host part, rest is parsed as a local url
	if (my ($host, $path) = $$r =~ m/^\/\/(.*?)(\/.*)$/) {

		$$r = $path;
		my $o = $class->SUPER::parse_url($r, $state);
		$o->{host} = $host;
		return $o;
	} else {
		return undef;
	}
}

my $buffsize = 2 * 1024 * 1024;

sub pkg_copy
{
	my ($self, $in, $object) = @_;

	require OpenBSD::Temp;
	my $name = $object->{name};
	my $dir = $object->{cache_dir};

	my ($copy, $filename) = OpenBSD::Temp::permanent_file($dir, $name) or die "Can't write copy to cache";
	chmod 0644, $filename;
	$object->{tempname} = $filename;
	my $handler = sub {
		my ($sig) = @_;
		unlink $filename;
		close($in);
		$SIG{$sig} = 'DEFAULT';
		kill $sig, $$;
	};

	my $nonempty = 0;
	my $error = 0;
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
			$self->{state}->fatal("Error reading: #1", $!);
		}
		if ($n > 0) {
			$nonempty = 1;
		}
		if (!$error) {
			my $r = syswrite $copy, $buffer;
			if (!defined $r || $r < $n) {
				$error = 1;
			}
		}
		syswrite STDOUT, $buffer;
	} while ($n != 0);
	close($copy);
	}

	if ($nonempty && !$error) {
		rename $filename, "$dir/$name.tgz";
	} else {
		unlink $filename;
	}
	close($in);
}

sub open_pipe
{
	require OpenBSD::Temp;

	my ($self, $object) = @_;
	$object->{errors} = OpenBSD::Temp->file;
	$object->{cache_dir} = $ENV{'PKG_CACHE'};
	$object->{parent} = $$;

	my ($rdfh, $wrfh);
	pipe($rdfh, $wrfh);

	my $pid = open(my $fh, "-|");
	$self->did_it_fork($pid);
	if ($pid) {
		$object->{pid} = $pid;
	} else {
		open(STDIN, '<&', $rdfh) or
		    $self->{state}->fatal("Bad dup: #1", $!);
		close($rdfh);
		close($wrfh);
		$self->exec_gunzip("-f", "-");
	}
	my $pid2 = fork();

	$self->did_it_fork($pid2);
	if ($pid2) {
		$object->{pid2} = $pid2;
	} else {
		open STDERR, '>', $object->{errors};
		open(STDOUT, '>&', $wrfh) or
		    $self->{state}->fatal("Bad dup: #1", $!);
		close($rdfh);
		close($wrfh);
		close($fh);
		if (defined $object->{cache_dir}) {
			my $pid3 = open(my $in, "-|");
			$self->did_it_fork($pid3);
			if ($pid3) {
				$self->pkg_copy($in, $object);
			} else {
				$self->grab_object($object);
			}
		} else {
			$self->grab_object($object);
		}
		exit(0);
	}
	close($rdfh);
	close($wrfh);
	return $fh;
}

sub finish_and_close
{
	my ($self, $object) = @_;
	if (defined $object->{cache_dir}) {
		while (defined $object->_next) {
		}
	}
	$self->SUPER::finish_and_close($object);
}

package OpenBSD::PackageRepository::HTTPorFTP;
our @ISA=qw(OpenBSD::PackageRepository::Distant);

our %distant = ();


sub grab_object
{
	my ($self, $object) = @_;
	my ($ftp, @extra) = split(/\s+/, OpenBSD::Paths->ftp);
	exec {$ftp}
	    $ftp,
	    @extra,
	    "-o",
	    "-", $self->url($object->{name})
	or $self->{state}->fatal("Can't run ".OpenBSD::Paths->ftp.": #1", $!);
}

sub maxcount
{
	return 1;
}

sub opened
{
	my $self = $_[0];
	my $k = $self->{host};
	if (!defined $distant{$k}) {
		$distant{$k} = [];
	}
	return $distant{$k};
}

sub should_have
{
	my ($self, $pkgname) = @_;
	if (defined $self->{lasterror} && $self->{lasterror} == 421) {
		return (defined $self->{list}) &&
			grep { $_ eq $pkgname } @{$self->{list}};
	} else {
		return 0;
	}
}

sub try_until_success
{
	my ($self, $pkgname, $code) = @_;

	for (my $retry = 5; $retry <= 160; $retry *= 2) {
		undef $self->{lasterror};
		my $o = &$code;
		if (defined $o) {
			return $o;
		}
		if (defined $self->{lasterror} && $self->{lasterror} == 550) {
			last;
		}
		if ($self->should_have($pkgname)) {
			$self->errsay("Temporary error, sleeping #1 seconds",
				$retry);
			sleep($retry);
		}
	}
	return undef;
}

sub find
{
	my ($self, $pkgname, @extra) = @_;

	return $self->try_until_success($pkgname,
	    sub {
	    	return $self->SUPER::find($pkgname, @extra); });

}

sub grabPlist
{
	my ($self, $pkgname, @extra) = @_;

	return $self->try_until_success($pkgname,
	    sub {
	    	return $self->SUPER::grabPlist($pkgname, @extra); });
}

sub parse_problems
{
	my ($self, $filename, $hint, $object) = @_;
	CORE::open(my $fh, '<', $filename) or return;

	my $baseurl = $self->url;
	my $url = $baseurl;
	if (defined $object) {
		$url = $object->url;
	}
	my $_;
	my $notyet = 1;
	while(<$fh>) {
		next if m/^(?:200|220|221|226|229|230|227|250|331|500|150)[\s\-]/o;
		next if m/^EPSV command not understood/o;
		next if m/^Trying [\da-f\.\:]+\.\.\./o;
		next if m/^Requesting \Q$baseurl\E/;
		next if m/^Remote system type is\s+/o;
		next if m/^Connected to\s+/o;
		next if m/^remote\:\s+/o;
		next if m/^Using binary mode to transfer files/o;
		next if m/^Retrieving\s+/o;
		next if m/^Success?fully retrieved file/o;
		next if m/^\d+\s+bytes\s+received\s+in/o;
		next if m/^ftp: connect to address.*: No route to host/o;

		if (defined $hint && $hint == 0) {
			next if m/^ftp: -: short write/o;
			next if m/^ftp: Writing -: Broken pipe/o;
			next if m/^421\s+/o;
		}
		if ($notyet) {
			$self->{state}->errsay("Error from #1", $url);
			$notyet = 0;
		}
		if (m/^421\s+/o ||
		    m/^ftp: connect: Connection timed out/o ||
		    m/^ftp: Can't connect or login to host/o) {
			$self->{lasterror} = 421;
		}
		if (m/^550\s+/o) {
			$self->{lasterror} = 550;
		}
		$self->{state}->errprint("#1", $_);
	}
	CORE::close($fh);
	$self->SUPER::parse_problems($filename, $hint, $object);
}

sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		$self->make_room;
		my $error = OpenBSD::Temp->file;
		$self->{list} = $self->obtain_list($error);
		$self->parse_problems($error);
		if ($self->{no_such_dir}) {
			$self->{state}->errsay(
			    "#1: Directory does not exist on #2",
			    $self->{path}, $self->{host});
		    	$self->{lasterror} = 404;
		}
	}
	return $self->{list};
}

sub get_http_list
{
	my ($self, $error) = @_;

	my $fullname = $self->url;
	my $l = [];
	my $_;
	open(my $fh, '-|', OpenBSD::Paths->ftp." -o - $fullname 2>$error")
	    or return;
	while(<$fh>) {
		chomp;
		for my $pkg (m/\<A\s+HREF=\"(.*?\.tgz)\"\>/gio) {
			$pkg = $1 if $pkg =~ m|^.*/(.*)$|;
			# decode uri-encoding; from URI::Escape
			$pkg =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
			$self->add_to_list($l, $pkg);
		}
	}
	close($fh);
	return $l;
}

package OpenBSD::PackageRepository::HTTP;
our @ISA=qw(OpenBSD::PackageRepository::HTTPorFTP);

sub urlscheme
{
	return 'http';
}

sub obtain_list
{
	my ($self, $error) = @_;
	return $self->get_http_list($error);
}

package OpenBSD::PackageRepository::HTTPS;
our @ISA=qw(OpenBSD::PackageRepository::HTTP);

sub urlscheme
{
	return 'https';
}

package OpenBSD::PackageRepository::FTP;
our @ISA=qw(OpenBSD::PackageRepository::HTTPorFTP);

sub urlscheme
{
	return 'ftp';
}

sub _list
{
	my ($self, $cmd) = @_;
	my $l =[];
	my $_;
	open(my $fh, '-|', "$cmd") or return;
	while(<$fh>) {
		chomp;
		next if m/^\d\d\d\s+\S/;
		if (m/No such file or directory|Failed to change directory/i) {
			$self->{no_such_dir} = 1;
		}
		next unless m/^(?:\.\/)?(\S+\.tgz)\s*$/;
		$self->add_to_list($l, $1);
	}
	close($fh);
	return $l;
}

sub get_ftp_list
{
	my ($self, $error) = @_;

	my $fullname = $self->url;
	return $self->_list("echo 'nlist'| ".OpenBSD::Paths->ftp
	    ." $fullname 2>$error");
}

sub obtain_list
{
	my ($self, $error) = @_;
	if (defined $ENV{'ftp_proxy'} && $ENV{'ftp_proxy'} ne '') {
		return $self->get_http_list($error);
	} else {
		return $self->get_ftp_list($error);
	}
}

1;
