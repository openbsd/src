# ex:ts=8 sw=4:
# $OpenBSD: Ustar.pm,v 1.37 2005/09/20 20:06:48 espie Exp $
#
# Copyright (c) 2002-2004 Marc Espie <espie@openbsd.org>
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

use File::Path ();
use File::Basename ();
use OpenBSD::IdCache;

my $uidcache = new OpenBSD::UidCache;
my $gidcache = new OpenBSD::GidCache;
my $unamecache = new OpenBSD::UnameCache;
my $gnamecache = new OpenBSD::GnameCache;

# This is a multiple of st_blksize everywhere....
my $buffsize = 2 * 1024 * 1024;

sub new
{
    my ($class, $fh, $destdir) = @_;

    $destdir = '' unless defined $destdir;

    return bless { fh => $fh, swallow => 0, key => {}, destdir => $destdir} , $class;
}


sub skip
{
    my $self = shift;
    return if $self->{swallow} == 0;

    my $temp;
    while ($self->{swallow} > $buffsize) {
    	read($self->{fh}, $temp, $buffsize);
	$self->{swallow} -= $buffsize;
    }
    read($self->{fh},  $temp, $self->{swallow});
    $self->{swallow} = 0;
}

sub next
{
    my $self = shift;
    # get rid of the current object
    $self->skip();
    my $header;
    my $n = read $self->{fh}, $header, 512;
    return undef if $n == 0;
    die "Error while reading header"
	unless defined $n and $n == 512;
    if ($header eq "\0"x512) {
	return $self->next();
    }
    # decode header
    my ($name, $mode, $uid, $gid, $size, $mtime, $chksum, $type,
    $linkname, $magic, $version, $uname, $gname, $major, $minor,
    $prefix, $pad) = unpack(USTAR_HEADER, $header);
    if ($magic ne "ustar\0" || $version ne '00') {
	die "Not an ustar archive header";
    }
    # verify checksum
    my $value = $header;
    substr($value, 148, 8) = " "x8;
    my $ck2 = unpack("%C*", $value);
    if ($ck2 != oct($chksum)) {
	die "Bad archive checksum";
    }
    $name =~ s/\0*$//;
    $mode = oct($mode) & 0xfff;
    $uname =~ s/\0*$//;
    $gname =~ s/\0*$//;
    $linkname =~ s/\0*$//;
    $major = oct($major);
    $minor = oct($minor);
    $uid = oct($uid);
    $gid = oct($gid);
    $uid = $uidcache->lookup($uname, $uid);
    $gid = $gidcache->lookup($gname, $gid);
    $mtime = oct($mtime);
    unless ($prefix =~ m/^\0/) {
	$prefix =~ s/\0*$//;
	$name = "$prefix/$name";
    }
    
    $size = oct($size);
    my $result= {
	name => $name,
	mode => $mode,
	mtime=> $mtime,
	linkname=> $linkname,
	uname => $uname,
	uid => $uid,
	gname => $gname,
	gid => $gid,
	size => $size,
	major => $major,
	minor => $minor,
	archive => $self,
	destdir => $self->{destdir}
	};
    if ($type eq DIR) {
    	bless $result, 'OpenBSD::Ustar::Dir';
    } elsif ($type eq HARDLINK) {
	bless $result, 'OpenBSD::Ustar::HardLink';
    } elsif ($type eq SOFTLINK) {
    	bless $result, 'OpenBSD::Ustar::SoftLink';
    } elsif ($type eq FILE || $type eq FILE1) {
    	bless $result, 'OpenBSD::Ustar::File';
    } elsif ($type eq FIFO) {
    	bless $result, 'OpenBSD::Ustar::Fifo';
    } elsif ($type eq CHARDEVICE) {
    	bless $result, 'OpenBSD::Ustar::CharDevice';
    } elsif ($type eq BLOCKDEVICE) {
    	bless $result, 'OpenBSD::Ustar::BlockDevice';
    } else {
    	die "Unsupported type";
    }
    if (!$result->isFile()) {
    	if ($size != 0) {
		die "Bad archive: non null size for non file entry\n";
	}
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
		    $name =~ m/^(.*?\/)(.*)$/) {
			$prefix .= $1;
			$name = $2;
		}
		$prefix =~ s|/$||;
	}
	return ($prefix, $name);
}

sub mkheader
{
	my ($entry, $type) = @_;
	my ($prefix, $name) = split_name($entry->{name});
	my $linkname = $entry->{linkname};
	my $size = $entry->{size};
	if (!$entry->isFile()) {
		$size = 0;
	}
	my ($major, $minor);
	if ($entry->isDevice()) {
		$major = $entry->{major};
		$minor = $entry->{minor};
	} else {
		$major = 0;
		$minor = 0;
	}
	if (defined $entry->{cwd}) {
		my $cwd = $entry->{cwd};
		$cwd.='/' unless $cwd =~ m/\/$/;
		$linkname =~ s/^\Q$cwd\E//;
	}
	if (!defined $linkname) {
		$linkname = '';
	}
	if (length $prefix > MAXPREFIX) {
		die "Prefix too long $prefix\n";
	}
	if (length $name > MAXFILENAME) {
		die "Name too long $name\n";
	}
	if (length $linkname > MAXLINKNAME) {
		die "Linkname too long $linkname\n";
	}
	if (!defined $entry->{uname}) {
		die "No user name for ", $entry->{name}, " (uid ", $entry->{uid}, ")\n";
	}
	if (length $entry->{uname} > MAXUSERNAME) {
		die "Username too long ", $entry->{uname}, "\n";
	}
	if (!defined $entry->{gname}) {
		die "No group name for ", $entry->{name}, " (gid ", $entry->{gid}. "\n";
	}
	if (length $entry->{gname} > MAXGROUPNAME) {
		die "Groupname too long ", $entry->{gname}, "\n";
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
		    $entry->{uname},
		    $entry->{gname},
		    sprintf("%07o", $major),
		    sprintf("%07o", $minor),
		    $prefix, "\0");
		$cksum = sprintf("%07o", unpack("%C*", $header));
	}
	return $header;
}

sub prepare
{
	my ($self, $filename) = @_;

	my $destdir = $self->{destdir};
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
		archive => $self,
		destdir => $self->{destdir}
	};
	my $k = $entry->{key};
	if (defined $self->{key}->{$k}) {
		$entry->{linkname} = $self->{key}->{$k};
		bless $entry, "OpenBSD::Ustar::HardLink";
	} elsif (-l $realname) {
		$entry->{linkname} = readlink($realname);
		bless $entry, "OpenBSD::Ustar::SoftLink";
	} elsif (-p _) {
		bless $entry, "OpenBSD::Ustar::Fifo";
	} elsif (-c _) {
		bless $entry, "OpenBSD::Ustar::CharDevice";
	} elsif (-b _) {
		bless $entry, "OpenBSD::Ustar::BlockDevice";
	} elsif (-d _) {
		bless $entry, "OpenBSD::Ustar::Dir";
	} else {
		bless $entry, "OpenBSD::Ustar::File";
	}
	return $entry;
}

sub pad
{
	my $fh = $_[0]->{fh};
	print $fh "\0"x1024;
}

sub close
{
	my $self = shift;
	if (defined $self->{padout}) {
	    $self->pad();
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
sub set_modes
{
	my $self = shift;
	chown $self->{uid}, $self->{gid}, $self->{destdir}.$self->{name};
	chmod $self->{mode}, $self->{destdir}.$self->{name};
	utime $self->{mtime}, $self->{mtime}, $self->{destdir}.$self->{name};
}

sub make_basedir
{
	my $self = shift;
	my $dir = $self->{destdir}.File::Basename::dirname($self->{name});
	File::Path::mkpath($dir) unless -d $dir;
}

sub write
{
	my $self = shift;
	my $arc = $self->{archive};
	my $out = $arc->{fh};

	$arc->{padout} = 1;
	my $header = OpenBSD::Ustar::mkheader($self, $self->type());
	print $out $header;
	$self->write_contents($arc);
	my $k = $self->{key};
	if (!defined $arc->{key}->{$k}) {
		$arc->{key}->{$k} = $self->{name};
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
	my $header = OpenBSD::Ustar::mkheader($self, $self->type());
	print $out $header;

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
	File::Path::mkpath($self->{destdir}.$self->{name});
	$self->SUPER::set_modes();
}

sub isDir() { 1 }

sub type() { OpenBSD::Ustar::DIR }

package OpenBSD::Ustar::HardLink;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir($self->{name});
	my $linkname = $self->{linkname};
	if (defined $self->{cwd}) {
		$linkname=$self->{cwd}.'/'.$linkname;
	}
	link $self->{destdir}.$linkname, $self->{destdir}.$self->{name} or
	    die "Can't link $self->{destdir}$linkname to $self->{destdir}$self->{name}: $!";
}

sub resolve_links
{
	my ($self, $arc) = @_;

	my $k = $self->{archive}.":".$self->{linkname};
	if (defined $arc->{key}->{$k}) {
		$self->{linkname} = $arc->{key}->{$k};
	} else {
		print join("\n", keys(%{$arc->{key}})), "\n";
		die "Can't copy link over: original for $k NOT available\n";
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
	$self->make_basedir($self->{name});
	symlink $self->{linkname}, $self->{destdir}.$self->{name} or 
	    die "Can't symlink $self->{linkname} to $self->{destdir}$self->{name}: $!";
}

sub isLink() { 1 }
sub isSymLink() { 1 }

sub type() { OpenBSD::Ustar::SOFTLINK }

package OpenBSD::Ustar::Fifo;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir($self->{name});
	require POSIX;
	POSIX::mkfifo($self->{destdir}.$self->{name}, $self->{mode}) or
	    die "Can't create fifo $self->{name}: $!";
	$self->SUPER::set_modes();
}

sub isFifo() { 1 }
sub type() { OpenBSD::Ustar::FIFO }

package OpenBSD::UStar::Device;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir($self->{name});
	system('/sbin/mknod', 'mknod', '-m', $self->{mode}, $self->{destdir}.$self->{name}, $self->devicetype(), $self->{major}, $self->{minor});
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
	open (my $out, '>', $fname);
	if (!defined $out) {
		return undef;
	}
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
				defined(syswrite($fh, $buffer, $i)) or return 0;
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
	defined(syswrite($fh, $buffer)) or return 0;
	return 1;
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
	$self->make_basedir($self->{name});
	my $buffer;
	my $out = OpenBSD::CompactWriter->new($self->{destdir}.$self->{name});
	if (!defined $out) {
		die "Can't write to $self->{destdir}$self->{name}: $!";
	}
	my $toread = $self->{size};
	while ($toread > 0) {
		my $maxread = $buffsize;
		$maxread = $toread if $maxread > $toread;
		if (!defined read($self->{archive}->{fh}, $buffer, $maxread)) {
			die "Error reading from archive: $!";
		}
		$self->{archive}->{swallow} -= $maxread;
		unless ($out->write($buffer)) {
			die "Error writing to $self->{destdir}$self->{name}: $!";
		}
			
		$toread -= $maxread;
	}
	$out->close() or die "Error closing $self->{destdir}$self->{name}: $!";
	$self->SUPER::set_modes();
}

sub contents
{
	my $self = shift;
	my $toread = $self->{size};
	my $buffer;

	if (!defined read($self->{archive}->{fh}, $buffer, $toread)) {
		die "Error reading from archive: $!";
	}
	$self->{archive}->{swallow} -= $toread;
	return $buffer;
}

sub write_contents
{
	my ($self, $arc) = @_;
	my $filename = $self->{realname};
	my $size = $self->{size};
	my $out = $arc->{fh};
	open my $fh, "<", $filename or die "Can't read file $filename: $!";

	my $buffer;
	my $toread = $size;
	while ($toread > 0) {
		my $maxread = $buffsize;
		$maxread = $toread if $maxread > $toread;
		if (!defined read($fh, $buffer, $maxread)) {
			die "Error reading from file: $!";
		}
		unless (print $out $buffer) {
			die "Error writing to archive: $!";
		}
			
		$toread -= $maxread;
	}
	if ($size % 512) {
		print $out "\0" x (512 - $size % 512);
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
		if (!defined read($self->{archive}->{fh}, $buffer, $maxread)) {
			die "Error reading from archive: $!";
		}
		$self->{archive}->{swallow} -= $maxread;
		unless (print $out $buffer) {
			die "Error writing to archive $!";
		}
			
		$toread -= $maxread;
	}
	if ($size % 512) {
		print $out "\0" x (512 - $size % 512);
	}
	$self->alias($arc, $self->{name});
}

sub isFile() { 1 }

sub type() { OpenBSD::Ustar::FILE1 }

1;
