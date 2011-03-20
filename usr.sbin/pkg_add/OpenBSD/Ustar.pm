# ex:ts=8 sw=4:
# $OpenBSD: Ustar.pm,v 1.71 2011/03/20 08:17:24 espie Exp $
#
# Copyright (c) 2002-2007 Marc Espie <espie@openbsd.org>
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

# Handle utar archives

use strict;
use warnings;

package OpenBSD::Ustar;

use constant {
	FILE => "\0",
	FILE1 => '0',
	HARDLINK => '1',
	SOFTLINK => '2',
	CHARDEVICE => '3',
	BLOCKDEVICE => '4',
	DIR => '5',
	FIFO => '6',
	CONTFILE => '7',
	USTAR_HEADER => 'a100a8a8a8a12a12a8aa100a6a2a32a32a8a8a155a12',
	MAXFILENAME => 100,
	MAXLINKNAME => 100,
	MAXPREFIX => 155,
	MAXUSERNAME => 32,
	MAXGROUPNAME => 32
};

use File::Basename ();
use OpenBSD::IdCache;
use OpenBSD::Paths;

my $uidcache = new OpenBSD::UidCache;
my $gidcache = new OpenBSD::GidCache;
my $unamecache = new OpenBSD::UnameCache;
my $gnamecache = new OpenBSD::GnameCache;

# This is a multiple of st_blksize everywhere....
my $buffsize = 2 * 1024 * 1024;

sub new
{
	my ($class, $fh, $state, $destdir) = @_;

	$destdir = '' unless defined $destdir;

	return bless {
	    fh => $fh,
	    swallow => 0,
	    state => $state,
	    key => {},
	    destdir => $destdir} , $class;
}

sub set_description
{
	my ($self, $d) = @_;
	$self->{description} = $d;
}

sub fatal
{
	my ($self, $msg, @args) = @_;
	$self->{state}->fatal("Ustar [#1][#2]: #3", 
	    $self->{description} // '?', $self->{lastname} // '?', 
	    $self->{state}->f($msg, @args));
}

sub new_object
{
	my ($self, $h, $class) = @_;
	$h->{archive} = $self;
	$h->{destdir} = $self->{destdir};
	bless $h, $class;
	return $h;
}

sub skip
{
	my $self = shift;
	my $temp;

	while ($self->{swallow} > 0) {
		my $toread = $self->{swallow};
		if ($toread >$buffsize) {
			$toread = $buffsize;
		}
		my $actual = read($self->{fh}, $temp, $toread);
		if (!defined $actual) {
			$self->fatal("Error while skipping archive: #1", $!);
		}
		if ($actual == 0) {
			$self->fatal("Premature end of archive in header: #1", $!);
		}
		$self->{swallow} -= $actual;
	}
}

my $types = {
	DIR , 'OpenBSD::Ustar::Dir',
	HARDLINK , 'OpenBSD::Ustar::HardLink',
	SOFTLINK , 'OpenBSD::Ustar::SoftLink',
	FILE , 'OpenBSD::Ustar::File',
	FILE1 , 'OpenBSD::Ustar::File',
	FIFO , 'OpenBSD::Ustar::Fifo',
	CHARDEVICE , 'OpenBSD::Ustar::CharDevice',
	BLOCKDEVICE , 'OpenBSD::Ustar::BlockDevice',
};

sub next
{
	my $self = shift;
	# get rid of the current object
	$self->skip;
	my $header;
	my $n = read $self->{fh}, $header, 512;
	return if (defined $n) and $n == 0;
	$self->fatal("Error while reading header")
	    unless defined $n and $n == 512;
	if ($header eq "\0"x512) {
		return $self->next;
	}
	# decode header
	my ($name, $mode, $uid, $gid, $size, $mtime, $chksum, $type,
	    $linkname, $magic, $version, $uname, $gname, $major, $minor,
	    $prefix, $pad) = unpack(USTAR_HEADER, $header);
	if ($magic ne "ustar\0" || $version ne '00') {
		$self->fatal("Not an ustar archive header");
	}
	# verify checksum
	my $value = $header;
	substr($value, 148, 8) = " "x8;
	my $ck2 = unpack("%C*", $value);
	if ($ck2 != oct($chksum)) {
		$self->fatal("Bad archive checksum");
	}
	$name =~ s/\0*$//o;
	$mode = oct($mode) & 0xfff;
	$uname =~ s/\0*$//o;
	$gname =~ s/\0*$//o;
	$linkname =~ s/\0*$//o;
	$major = oct($major);
	$minor = oct($minor);
	$uid = oct($uid);
	$gid = oct($gid);
	$uid = $uidcache->lookup($uname, $uid);
	$gid = $gidcache->lookup($gname, $gid);
	$mtime = oct($mtime);
	unless ($prefix =~ m/^\0/o) {
		$prefix =~ s/\0*$//o;
		$name = "$prefix/$name";
	}

	$self->{lastname} = $name;
	$size = oct($size);
	my $result= {
	    name => $name,
	    mode => $mode,
	    atime => $mtime,
	    mtime => $mtime,
	    linkname=> $linkname,
	    uname => $uname,
	    uid => $uid,
	    gname => $gname,
	    gid => $gid,
	    size => $size,
	    major => $major,
	    minor => $minor,
	};
	if (defined $types->{$type}) {
		$self->new_object($result, $types->{$type});
	} else {
		$self->fatal("Unsupported type #1", $type);
	}
	if (!$result->isFile && $result->{size} != 0) {
		$self->fatal("Bad archive: non null size for #1 (#2)", 
		    $types->{$type}, $result->{name});
	}

	# adjust swallow
	$self->{swallow} = $size;
	if ($size % 512) {
		$self->{swallow} += 512 - $size % 512;
	}
	$self->{cachename} = $name;
	return $result;
}

sub split_name
{
	my $name = shift;
	my $prefix = '';

	my $l = length $name;
	if ($l > MAXFILENAME && $l <= MAXFILENAME+MAXPREFIX+1) {
		while (length($name) > MAXFILENAME &&
		    $name =~ m/^(.*?\/)(.*)$/o) {
			$prefix .= $1;
			$name = $2;
		}
		$prefix =~ s|/$||;
	}
	return ($prefix, $name);
}

sub mkheader
{
	my ($archive, $entry, $type) = @_;
	my ($prefix, $name) = split_name($entry->name);
	my $linkname = $entry->{linkname};
	my $size = $entry->{size};
	my ($major, $minor);
	if ($entry->isDevice) {
		$major = $entry->{major};
		$minor = $entry->{minor};
	} else {
		$major = 0;
		$minor = 0;
	}
	my ($uname, $gname);
	if (defined $entry->{uname}) {
		$uname = $entry->{uname};
	} else {
		$uname = $entry->{uid};
	}
	if (defined $entry->{gname}) {
		$gname = $entry->{gname};
	} else {
		$gname = $entry->{gid};
	}

	if (defined $entry->{cwd}) {
		my $cwd = $entry->{cwd};
		$cwd.='/' unless $cwd =~ m/\/$/o;
		$linkname =~ s/^\Q$cwd\E//;
	}
	if (!defined $linkname) {
		$linkname = '';
	}
	if (length $prefix > MAXPREFIX) {
		$archive->fatal("Prefix too long #1", $prefix);
	}
	if (length $name > MAXFILENAME) {
		$archive->fatal("Name too long #1", $name);
	}
	if (length $linkname > MAXLINKNAME) {
		$archive->fatal("Linkname too long #1", $linkname);
	}
	if (length $uname > MAXUSERNAME) {
		$archive->fatal("Username too long #1", $uname);
	}
	if (length $gname > MAXGROUPNAME) {
		$archive->fatal("Groupname too long #1", $gname);
	}
	my $header;
	my $cksum = ' 'x8;
	for (1 .. 2) {
		$header = pack(USTAR_HEADER,
		    $name,
		    sprintf("%07o", $entry->{mode}),
		    sprintf("%07o", $entry->{uid}),
		    sprintf("%07o", $entry->{gid}),
		    sprintf("%011o", $size),
		    sprintf("%011o", $entry->{mtime}),
		    $cksum,
		    $type,
		    $linkname,
		    'ustar', '00',
		    $uname,
		    $gname,
		    sprintf("%07o", $major),
		    sprintf("%07o", $minor),
		    $prefix, "\0");
		$cksum = sprintf("%07o", unpack("%C*", $header));
	}
	return $header;
}

sub prepare
{
	my ($self, $filename, $destdir) = @_;

	$destdir //= $self->{destdir};
	my $realname = "$destdir/$filename";

	my ($dev, $ino, $mode, $uid, $gid, $rdev, $size, $mtime) =
	    (lstat $realname)[0,1,2,4,5,6, 7,9];

	my $entry = {
		key => "$dev/$ino",
		name => $filename,
		realname => $realname,
		mode => $mode,
		uid => $uid,
		gid => $gid,
		size => $size,
		mtime => $mtime,
		uname => $unamecache->lookup($uid),
		gname => $gnamecache->lookup($gid),
		major => $rdev/256,
		minor => $rdev%256,
	};
	my $k = $entry->{key};
	my $class = "OpenBSD::Ustar::File"; # default
	if (defined $self->{key}->{$k}) {
		$entry->{linkname} = $self->{key}->{$k};
		$class = "OpenBSD::Ustar::HardLink";
	} elsif (-l $realname) {
		$entry->{linkname} = readlink($realname);
		$class = "OpenBSD::Ustar::SoftLink";
	} elsif (-p _) {
		$class = "OpenBSD::Ustar::Fifo";
	} elsif (-c _) {
		$class = "OpenBSD::Ustar::CharDevice";
	} elsif (-b _) {
		$class ="OpenBSD::Ustar::BlockDevice";
	} elsif (-d _) {
		$class = "OpenBSD::Ustar::Dir";
	}
	$self->new_object($entry, $class);
	if (!$entry->isFile) {
		$entry->{size} = 0;
	}
	return $entry;
}

sub pad
{
	my $self = shift;
	my $fh = $self->{fh};
	print $fh "\0"x1024 or $self->fatal("Error writing to archive: #1", $!);
}

sub close
{
	my $self = shift;
	if (defined $self->{padout}) {
	    $self->pad;
	}
	close($self->{fh});
}

sub destdir
{
	my $self = shift;
	if (@_ > 0) {
		$self->{destdir} = shift;
	} else {
		return $self->{destdir};
	}
}

sub fh
{
	return $_[0]->{fh};
}

package OpenBSD::Ustar::Object;

sub fatal
{
	my ($self, @args) = @_;
	$self->{archive}->fatal(@args);
}

sub system
{
	my ($self, @args) = @_;
	$self->{archive}{state}->system(@args);
}

sub errsay
{
	my ($self, @args) = @_;
	$self->{archive}->{state}->errsay(@args);
}
sub todo
{
	my ($self, $toread) = @_;
	return if $toread == 0;
	return unless defined $self->{archive}{callback};
	&{$self->{archive}{callback}}($self->{size} - $toread);
}

sub name
{
	my $self = shift;
	return $self->{name};
}

sub set_name
{
	my ($self, $v) = @_;
	$self->{name} = $v;
}

sub set_modes
{
	my $self = shift;
	chown $self->{uid}, $self->{gid}, $self->{destdir}.$self->name;
	chmod $self->{mode}, $self->{destdir}.$self->name;
	if (defined $self->{mtime} || defined $self->{atime}) {
		utime $self->{atime} // time, $self->{mtime} // time,
		    $self->{destdir}.$self->name;
	}
}

sub ensure_dir
{
	my ($self, $dir) = @_;
	return if -d $dir;
	$self->ensure_dir(File::Basename::dirname($dir));
	if (mkdir($dir)) {
		return;
	}
	$self->fatal("Error making directory #1: #2", $dir, $!);
}

sub make_basedir
{
	my $self = shift;
	my $dir = $self->{destdir}.File::Basename::dirname($self->name);
	$self->ensure_dir($dir);
}

sub write
{
	my $self = shift;
	my $arc = $self->{archive};
	my $out = $arc->{fh};

	$arc->{padout} = 1;
	my $header = $arc->mkheader($self, $self->type);
	print $out $header or $self->fatal("Error writing to archive: #1", $!);
	$self->write_contents($arc);
	my $k = $self->{key};
	if (!defined $arc->{key}->{$k}) {
		$arc->{key}->{$k} = $self->name;
	}
}

sub alias
{
	my ($self, $arc, $alias) = @_;

	my $k = $self->{archive}.":".$self->{archive}->{cachename};
	if (!defined $arc->{key}->{$k}) {
		$arc->{key}->{$k} = $alias;
	}
}

sub write_contents
{
	# only files have anything to write
}

sub resolve_links
{
	# only hard links must cheat
}

sub copy_contents
{
	# only files need copying
}

sub copy
{
	my ($self, $wrarc) = @_;
	my $out = $wrarc->{fh};
	$self->resolve_links($wrarc);
	$wrarc->{padout} = 1;
	my $header = $wrarc->mkheader($self, $self->type);
	print $out $header or $self->fatal("Error writing to archive: #1", $!);

	$self->copy_contents($wrarc);
}

sub isDir() { 0 }
sub isFile() { 0 }
sub isDevice() { 0 }
sub isFifo() { 0 }
sub isLink() { 0 }
sub isSymLink() { 0 }
sub isHardLink() { 0 }

package OpenBSD::Ustar::Dir;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->ensure_dir($self->{destdir}.$self->name);
	$self->set_modes;
}

sub isDir() { 1 }

sub type() { OpenBSD::Ustar::DIR }

package OpenBSD::Ustar::HardLink;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir;
	my $linkname = $self->{linkname};
	if (defined $self->{cwd}) {
		$linkname=$self->{cwd}.'/'.$linkname;
	}
	link $self->{destdir}.$linkname, $self->{destdir}.$self->name or
	    $self->fatal("Can't link #1#2 to #1#3: #4",
	    	$self->{destdir}, $linkname, $self->name, $!);
}

sub resolve_links
{
	my ($self, $arc) = @_;

	my $k = $self->{archive}.":".$self->{linkname};
	if (defined $arc->{key}->{$k}) {
		$self->{linkname} = $arc->{key}->{$k};
	} else {
		print join("\n", keys(%{$arc->{key}})), "\n";
		$self->fatal("Can't copy link over: original for #1 NOT available", $k);
	}
}

sub isLink() { 1 }
sub isHardLink() { 1 }

sub type() { OpenBSD::Ustar::HARDLINK }

package OpenBSD::Ustar::SoftLink;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir;
	symlink $self->{linkname}, $self->{destdir}.$self->name or
	    $self->fatal("Can't symlink #1 to #2#3: #4",
	    	$self->{linkname}, $self->{destdir}, $self->name, $!);
}

sub isLink() { 1 }
sub isSymLink() { 1 }

sub type() { OpenBSD::Ustar::SOFTLINK }

package OpenBSD::Ustar::Fifo;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir;
	require POSIX;
	POSIX::mkfifo($self->{destdir}.$self->name, $self->{mode}) or
	    $self->fatal("Can't create fifo #1#2: #3", $self->{destdir},
	    	$self->name, $!);
	$self->set_modes;
}

sub isFifo() { 1 }
sub type() { OpenBSD::Ustar::FIFO }

package OpenBSD::UStar::Device;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir;
	$self->system(OpenBSD::Paths->mknod,
	    '-m', $self->{mode}, '--', $self->{destdir}.$self->name,
	    $self->devicetype, $self->{major}, $self->{minor});
	$self->set_modes;
}

sub isDevice() { 1 }

package OpenBSD::Ustar::BlockDevice;
our @ISA=qw(OpenBSD::Ustar::Device);

sub type() { OpenBSD::Ustar::BLOCKDEVICE }
sub devicetype() { 'b' }

package OpenBSD::Ustar::CharDevice;
our @ISA=qw(OpenBSD::Ustar::Device);

sub type() { OpenBSD::Ustar::BLOCKDEVICE }
sub devicetype() { 'c' }

package OpenBSD::CompactWriter;

use constant {
	FH => 0,
	BS => 1,
	ZEROES => 2,
	UNFINISHED => 3,
};

sub new
{
	my ($class, $fname) = @_;
	open (my $out, '>', $fname) or return;
	my $bs = (stat $out)[11];
	my $zeroes;
	if (defined $bs) {
		$zeroes = "\x00"x$bs;
	}
	bless [ $out, $bs, $zeroes, 0 ], $class;
}

sub write
{
	my ($self, $buffer) = @_;
	my ($fh, $bs, $zeroes, $e) = @$self;
START:
	if (defined $bs) {
		for (my $i = 0; $i + $bs <= length($buffer); $i+= $bs) {
			if (substr($buffer, $i, $bs) eq $zeroes) {
				my $r = syswrite($fh, $buffer, $i);
				unless (defined $r && $r == $i) {
					return 0;
				}
				$i+=$bs;
				my $seek_forward = $bs;
				while (substr($buffer, $i, $bs) eq $zeroes) {
					$i += $bs;
					$seek_forward += $bs;
				}
				defined(sysseek($fh, $seek_forward, 1))
				    or return 0;
				$buffer = substr($buffer, $i);
				if (length $buffer == 0) {
					$self->[UNFINISHED] = 1;
					return 1;
				}
				goto START;
			}
		}
	}
	$self->[UNFINISHED] = 0;
	my $r = syswrite($fh, $buffer);
	if (defined $r && $r == length $buffer) {
		return 1;
	} else {
		return 0;
	}
}

sub close
{
	my ($self) = @_;
	if ($self->[UNFINISHED]) {
		defined(sysseek($self->[FH], -1, 1)) or return 0;
		defined(syswrite($self->[FH], "\0")) or return 0;
	}
	return 1;
}

package OpenBSD::Ustar::File;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir;
	my $buffer;
	my $out = OpenBSD::CompactWriter->new($self->{destdir}.$self->name);
	if (!defined $out) {
		$self->fatal("Can't write to #1#2: #3", $self->{destdir},
		    $self->name, $!);
	}
	my $toread = $self->{size};
	if ($self->{partial}) {
		$toread -= length($self->{partial});
		unless ($out->write($self->{partial})) {
			$self->fatal("Error writing to #1#2: #3",
			    $self->{destdir}, $self->name, $!);
		}
	}
	while ($toread > 0) {
		my $maxread = $buffsize;
		$maxread = $toread if $maxread > $toread;
		my $actual = read($self->{archive}->{fh}, $buffer, $maxread);
		if (!defined $actual) {
			$self->fatal("Error reading from archive: #1", $!);
		}
		if ($actual == 0) {
			$self->fatal("Premature end of archive");
		}
		$self->{archive}->{swallow} -= $actual;
		unless ($out->write($buffer)) {
			$self->fatal("Error writing to #1#2: #3",
			    $self->{destdir}, $self->name, $!);
		}

		$toread -= $actual;
		$self->todo($toread);
	}
	$out->close or $self->fatal("Error closing #1#2: #3",
	    $self->{destdir}, $self->name, $!);
	$self->set_modes;
}

sub contents
{
	my ($self, $lookfor) = @_;
	my $toread = $self->{size};
	my $buffer;
	my $offset = 0;
	if ($self->{partial}) {
		$buffer = $self->{partial};
		$offset = length($self->{partial});
		$toread -= $offset;
	}

	while ($toread != 0) {
		my $sz = $toread;
		if (defined $lookfor) {
			last if (defined $buffer) and &$lookfor($buffer);
			$sz = 1024 if $sz > 1024;
		}
		my $actual = read($self->{archive}->{fh}, $buffer, $sz, $offset);
		if (!defined $actual) {
			$self->fatal("Error reading from archive: #1", $!);
		}
		if ($actual != $sz) {
			$self->fatal("Error: short read from archive");
		}
		$self->{archive}->{swallow} -= $actual;
		$toread -= $actual;
		$offset += $actual;
	}

	$self->{partial} = $buffer;
	return $buffer;
}

sub write_contents
{
	my ($self, $arc) = @_;
	my $filename = $self->{realname};
	my $size = $self->{size};
	my $out = $arc->{fh};
	open my $fh, "<", $filename or $self->fatal("Can't read file #1: #2",
	    $filename, $!);

	my $buffer;
	my $toread = $size;
	while ($toread > 0) {
		my $maxread = $buffsize;
		$maxread = $toread if $maxread > $toread;
		my $actual = read($fh, $buffer, $maxread);
		if (!defined $actual) {
			$self->fatal("Error reading from file: #1", $!);
		}
		if ($actual == 0) {
			$self->fatal("Premature end of file");
		}
		unless (print $out $buffer) {
			$self->fatal("Error writing to archive: #1", $!);
		}

		$toread -= $actual;
		$self->todo($toread);
	}
	if ($size % 512) {
		print $out "\0" x (512 - $size % 512) or
		    $self->fatal("Error writing to archive: #1", $!);
	}
}

sub copy_contents
{
	my ($self, $arc) = @_;
	my $out = $arc->{fh};
	my $buffer;
	my $size = $self->{size};
	my $toread = $size;
	while ($toread > 0) {
		my $maxread = $buffsize;
		$maxread = $toread if $maxread > $toread;
		my $actual = read($self->{archive}->{fh}, $buffer, $maxread);
		if (!defined $actual) {
			$self->fatal("Error reading from archive: #1", $!);
		}
		if ($actual == 0) {
			$self->fatal("Premature end of archive");
		}
		$self->{archive}->{swallow} -= $actual;
		unless (print $out $buffer) {
			$self->fatal("Error writing to archive #1", $!);
		}

		$toread -= $actual;
	}
	if ($size % 512) {
		print $out "\0" x (512 - $size % 512) or
		    $self->fatal("Error writing to archive: #1", $!);
	}
	$self->alias($arc, $self->name);
}

sub isFile() { 1 }

sub type() { OpenBSD::Ustar::FILE1 }

1;
