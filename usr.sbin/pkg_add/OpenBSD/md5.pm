# ex:ts=8 sw=4:
# $OpenBSD: md5.pm,v 1.11 2010/06/30 10:51:04 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::digest;

sub new
{
	my ($class, $filename) = @_;
	$class = ref($class) || $class;
	my $digest = $class->digest_file($filename);
	bless \$digest, $class;
}

sub write
{
	my ($self, $fh) = @_;
	print $fh "\@", $self->keyword, " ", $self->stringize, "\n";
}

sub digest_file
{
	my ($self, $fname) = @_;
	open(my $file, '<', $fname) or die "can't open $fname: $!";
	my $digest = $self->digest_fh($file);
	close($file) or die "problem closing $fname: $!";
	return $digest;
}


sub digest_fh
{
	my ($self, $file) = @_;

	my $d = $self->algo;

	$d->addfile($file);
	return $d->digest;
}

sub fromstring
{
	my ($class, $arg) = @_;
	$class = ref($class) || $class;
	my $d = $class->unstringize($arg);
	bless \$d, $class;
}

sub equals
{
	my ($a, $b) = @_;
	return ref($a) eq ref($b) && $$a eq $$b;
}

package OpenBSD::md5;
our @ISA=(qw(OpenBSD::digest));

sub fromfile
{
	my $fname = shift;
	return OpenBSD::md5->digest_file($fname);
}

sub fromfh
{
	my $file = shift;
	return OpenBSD::md5->digest_fh($file);
}

sub algo
{
	my $self = shift;
	require Digest::MD5;

	return Digest::MD5->new;
}

sub stringize
{
	my $self = shift;
	return unpack('H*', $$self);
}

sub unstringize
{
	my ($class, $arg) = @_;
	return pack('H*', $arg);
}

sub keyword
{
	return "md5";
}

package OpenBSD::sha;
our @ISA=(qw(OpenBSD::digest));

use Digest::SHA;
use MIME::Base64;

sub algo
{
	my $self = shift;

	return Digest::SHA->new(256);
}

sub stringize
{
	my $self = shift;

	return encode_base64($$self, '');
}

sub unstringize
{
	my ($class, $arg) = @_;

	return decode_base64($arg);
}

sub keyword
{
	return "sha";
}

1;
