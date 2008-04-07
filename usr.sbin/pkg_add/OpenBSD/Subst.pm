# ex:ts=8 sw=4:
# $OpenBSD: Subst.pm,v 1.1 2008/04/07 11:02:24 espie Exp $
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

sub add
{
	my ($self, $k, $v) = @_;
	$self->{$k} = $v;
}

sub parse_option
{
	my ($self, $opt) = @_;
	if ($opt =~ m/^([^=]+)\=(.*)$/o) {
		my ($k, $v) = ($1, $2);
		$v =~ s/^\'(.*)\'$/$1/;
		$v =~ s/^\"(.*)\"$/$1/;
		$self->{$k} = $v;
	} else {
		$self->{$opt} = 1;
	}
}

sub do
{
	my $self = shift;
	local $_ = shift;
	return $_ unless m/\$/o;	# optimization
	while (my ($k, $v) = each %$self) {
		s/\$\{\Q$k\E\}/$v/g;
	}
	s/\$\\/\$/go;
	return $_;
}

sub copy_fh
{
	my ($self, $srcname, $dest) = @_;
	open my $src, '<', $srcname or die "can't open $srcname";
	local $_;
	while (<$src>) {
		print $dest $self->do($_);
	}
}

sub copy
{
	my ($self, $srcname, $destname) = @_;
	open my $dest, '>', $destname or die "can't open $destname";
	$self->copy_fh($srcname, $dest);
}

sub has_fragment
{
	my ($self, $def, $frag) = @_;

	if (!defined $self->{$def}) {
		die "Error: unknown fragment $frag";
	} elsif ($self->{$def} == 1) {
		return 1;
	} elsif ($self->{$def} == 0) {
		return 0;
	} else {
		die "Incorrect define for $frag";
	}
}

sub value
{
	my ($self, $k) = @_;
	return $self->{$k};
}

sub empty
{
	my ($self, $k) = @_;
	if (defined $self->{$k} && $self->{$k}) {
		return 0;
	} else {
		return 1;
	}
}

1;
