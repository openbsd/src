# ex:ts=8 sw=4:
# $OpenBSD: Ustar.pm,v 1.15 2004/12/26 15:18:51 espie Exp $
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
	USTAR_HEADER => 'a100a8a8a8a12a12a8aa100a6a2a32a32a8a8a155',
};

use File::Path ();
use File::Basename ();
use OpenBSD::IdCache;

my $uidcache = new OpenBSD::UidCache;
my $gidcache = new OpenBSD::GidCache;

# This is a multiple of st_blksize everywhere....
my $buffsize = 2 * 1024 * 1024;

sub new
{
    my ($class, $fh, $destdir) = @_;

    $destdir = '' unless defined $destdir;

    return bless { fh => $fh, swallow => 0, destdir => $destdir} , $class;
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
    $prefix) = unpack(USTAR_HEADER, $header);
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
	archive => $self,
	destdir => $self->{destdir}
	};
    # adjust swallow
    $self->{swallow} = $size;
    if ($size % 512) {
	$self->{swallow} += 512 - $size % 512;
    }
    if ($type eq DIR) {
    	bless $result, 'OpenBSD::Ustar::Dir';
    } elsif ($type eq HARDLINK) {
	bless $result, 'OpenBSD::Ustar::HardLink';
    } elsif ($type eq SOFTLINK) {
    	bless $result, 'OpenBSD::Ustar::SoftLink';
    } elsif ($type eq FILE || $type eq FILE1) {
    	bless $result, 'OpenBSD::Ustar::File';
    } else {
    	die "Unsupported type";
    }
    return $result;
}

sub mkheader
{
	my ($entry, $type) = @_;
	my ($name, $prefix);
	if (length($name) < 100) {
		$prefix = '';
	} elsif (length($name) > 255) {
		die "Can't fit such a name $name\n";
	} elsif ($name =~ m|^(.*)/(.{,100})$|) {
		$prefix = $1;
		$name = $2;
	} else {
		die "Can't fit such a name $name\n";
	}
	my $header;
	my $cksum = ' 'x8;
	for (1 .. 2) {
		$header = pack(USTAR_HEADER, 
		    $name,
		    sprintf("%o", $entry->{mode}),
		    sprintf("%o", $entry->{uid}),
		    sprintf("%o", $entry->{gid}),
		    sprintf("%o", $entry->{size}),
		    sprintf("%o", $entry->{mtime}),
		    $cksum,
		    $type,
		    $entry->{linkname},
		    'ustar', '00',
		    $entry->{uname},
		    $entry->{gname},
		    '0', '0',
		    $prefix);
		$cksum = unpack("%C*", $header);
	}
	return $header;
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

sub isDir() { 0 }
sub isFile() { 0 }
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

sub isLink() { 1 }
sub isHardLink() { 1 }

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

sub isFile() { 1 }

1;
