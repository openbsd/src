# $OpenBSD: Ustar.pm,v 1.4 2003/12/19 00:29:20 espie Exp $
#
# Copyright (c) 2002 Marc Espie.
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

# Handle utar archives

# Prototype of new pkg* implementation, tar module.
# Interface is likely to change A LOT in the near future...

use strict;
use warnings;
package OpenBSD::Ustar;
# helps autoloader
sub DESTROY
{
}

use constant FILE => "\0";
use constant FILE1 => '0';
use constant HARDLINK => '1';
use constant SOFTLINK => '2';
use constant CHARDEVICE => '3';
use constant BLOCKDEVICE => '4';
use constant DIR => '5';
use constant FIFO => '6';
use constant CONTFILE => '7';
use File::Path ();
use File::Basename ();

my $uidcache = {};
my $gidcache = {};
my $buffsize = 2 * 1024 * 1024;

sub new
{
    my ($class, $fh) = @_;

    return bless { fh => $fh, swallow => 0} , $class;
}


sub name2uid
{
	my $name = shift;
	return $uidcache->{$name} if defined $uidcache->{$name};
	my @entry = getpwnam($name);
	if (@entry == 0) {
		return $uidcache->{$name} = shift;
	} else {
		return $uidcache->{$name} = $entry[2];
	}
}

sub name2gid
{
	my $name = shift;
	return $gidcache->{$name} if defined $gidcache->{$name};
	my @entry = getgrnam($name);
	if (@entry == 0) {
		return $gidcache->{$name} = shift;
	} else {
		return $gidcache->{$name} = $entry[2];
	}
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
    $prefix) = unpack('a100a8a8a8a12a12a8aa100a6a2a32a32a8a8a155', $header);
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
    $uid = oct($uid);
    $gid = oct($gid);
    $uid = name2uid($uname, $uid);
    $gid = name2gid($gname, $gid);
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
	archive => $self
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

package OpenBSD::Ustar::Object;
sub set_modes
{
	my $self = shift;
	chown $self->{uid}, $self->{gid}, $self->{name};
	chmod $self->{mode}, $self->{name};
	utime $self->{mtime}, $self->{mtime}, $self->{name};
}

sub make_basedir
{
	my $self = shift;
	my $dir = File::Basename::dirname($self->{name});
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
	File::Path::mkpath($self->{name});
	$self->SUPER::set_modes();
}

sub isDir() { 1 }

package OpenBSD::Ustar::HardLink;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir($self->{name});
	if (defined $self->{cwd}) {
		link $self->{cwd}."/".$self->{linkname}, $self->{name};
	} else {
		link $self->{linkname}, $self->{name};
	}
}

sub isLink() { 1 }
sub isHardLink() { 1 }

package OpenBSD::Ustar::SoftLink;
our @ISA=qw(OpenBSD::Ustar::Object);

sub create
{
	my $self = shift;
	$self->make_basedir($self->{name});
	symlink $self->{linkname}, $self->{name};
}

sub isLink() { 1 }
sub isHardLink() { 1 }

package OpenBSD::Ustar::File;
our @ISA=qw(OpenBSD::Ustar::Object);

use IO::File;

sub create
{
	my $self = shift;
	$self->make_basedir($self->{name});
	my $out = new IO::File $self->{name}, "w";
	if (!defined $out) {
		print "Can't write to ", $self->{name}, "\n";
		return;
	}
	my $buffer;
	my $toread = $self->{size};
	while ($toread > 0) {
		my $maxread = $buffsize;
		$maxread = $toread if $maxread > $toread;
		read($self->{archive}->{fh}, $buffer, $maxread);
		$self->{archive}->{swallow} -= $maxread;
		print $out $buffer;
		$toread -= $maxread;
	}
	$out->close();
	$self->SUPER::set_modes();
}

sub isFile() { 1 }

1;
