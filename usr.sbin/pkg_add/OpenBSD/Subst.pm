# ex:ts=8 sw=4:
# $OpenBSD: Subst.pm,v 1.14 2010/12/24 09:04:14 espie Exp $
#
# Copyright (c) 2008 Marc Espie <espie@openbsd.org>
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

# very simple package, just holds everything needed for substitution
# according to package rules.

package OpenBSD::Subst;

sub new
{
	bless {}, shift;
}

sub hash
{
	shift;
}

sub add
{
	my ($self, $k, $v) = @_;
	$k =~ s/^\^//;
	$self->{$k} = $v;
}

sub value
{
	my ($self, $k) = @_;
	return $self->{$k};
}

sub parse_option
{
	my ($self, $opt) = @_;
	if ($opt =~ m/^([^=]+)\=(.*)$/o) {
		my ($k, $v) = ($1, $2);
		$v =~ s/^\'(.*)\'$/$1/;
		$v =~ s/^\"(.*)\"$/$1/;
		$self->add($k, $v);
	} else {
		$self->add($opt, 1);
	}
}

sub do
{
	my $self = shift;
	my $_ = shift;
	return $_ unless m/\$/o;	# optimization
	while (my ($k, $v) = each %{$self->hash}) {
		s/\$\{\Q$k\E\}/$v/g;
	}
	s/\$\\/\$/go;
	return $_;
}

sub copy_fh2
{
	my ($self, $src, $dest) = @_;
	my $_;
	while (<$src>) {
		print $dest $self->do($_);
	}
}

sub copy_fh
{
	my ($self, $srcname, $dest) = @_;
	open my $src, '<', $srcname or die "can't open $srcname";
	$self->copy_fh2($src, $dest);
}

sub copy
{
	my ($self, $srcname, $destname) = @_;
	open my $dest, '>', $destname or die "can't open $destname";
	$self->copy_fh($srcname, $dest);
	return $dest;
}

sub has_fragment
{
	my ($self, $def, $frag) = @_;

	my $v = $self->value($def);

	if (!defined $v) {
		die "Error: unknown fragment $frag";
	} elsif ($v == 1) {
		return 1;
	} elsif ($v == 0) {
		return 0;
	} else {
		die "Incorrect define for $frag";
	}
}

sub empty
{
	my ($self, $k) = @_;

	my $v = $self->value($k);
	if (defined $v && $v) {
		return 0;
	} else {
		return 1;
	}
}

1;
