# ex:ts=8 sw=4:
# $OpenBSD: Signature.pm,v 1.2 2010/01/10 12:38:27 espie Exp $
#
# Copyright (c) 2010 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackingElement;
sub signature {}

package OpenBSD::PackingElement::Dependency;
sub signature
{
	my ($self, $hash) = @_;
	$hash->{$self->{pkgpath}} = OpenBSD::PackageName->from_string($self->{def});
}

package OpenBSD::PackingElement::Wantlib;
sub signature
{
	my ($self, $hash) = @_;
	require OpenBSD::SharedLibs;

	my ($stem, $major, $minor) = OpenBSD::SharedLibs::parse_spec($self->name);
	if (defined $stem) {
		$hash->{$stem} = OpenBSD::LibrarySpec->new($stem, 
		    $major, $minor);
	}
}

package OpenBSD::Signature;
sub from_plist
{
	my ($class, $plist) = @_;

	if ($plist->has('always-update')) {
		my $s;
		open my $fh, '>', \$s;
		$plist->write_no_sig($fh);
		close $fh;
		return $class->always->new($plist->pkgname, $s);
	} else {
		my $k = {};
		$plist->visit('signature', $k);
		return $class->new($plist->pkgname, $k);
	}
}

sub always
{
	return "OpenBSD::Signature::Full";
}

sub new
{
	my ($class, $pkgname, $extra) = @_;
	bless { name => $pkgname, extra => $extra }, $class;
}

sub string
{
	my $self = shift;
	return join(',', $self->{name}, sort map {$_->to_string} values %{$self->{extra}});
}

sub compare
{
	my ($a, $b) = @_;

	if ($a->{name} eq $b->{name}) {
		return $b->revert_compare($a);
	} else {
		return OpenBSD::PackageName->from_string($a->{name})->compare(OpenBSD::PackageName->from_string($b->{name}));
	}
}

sub print_error
{
	my ($a, $b) = @_;

	print STDERR "Error: $a->{name} exists in two non-comparable versions\n";
	print STDERR "Someone forgot to bump a PKGNAME\n";
	print STDERR $a->string, " vs. ", $b->string, "\n";
}

sub revert_compare
{
	my ($b, $a) = @_;

	my $awins = 0;
	my $bwins = 0;
	my $done = {};
	while (my ($k, $v) = each %{$a->{extra}}) {
		if (!defined $b->{extra}{$k}) {
			$a->print_error($b);
			return undef;
		}
		$done->{$k} = 1;
		my $r = $v->compare($b->{extra}{$k});
		if ($r > 0) {
			$awins++;
		} elsif ($r < 0) {
			$bwins++;
		}
	}
	for my $k (keys %{$b->{extra}}) {
		if (!$done->{$k}) {
			$a->print_error($b);
			return undef;
		}
	}
	if ($awins == 0) {
		return -$bwins;
	} elsif ($bwins == 0) {
		return $awins;
	} else {
		return undef;
	}
}

package OpenBSD::Signature::Full;
our @ISA=qw(OpenBSD::Signature);

sub string
{
	my $self = shift;
	return $self->{extra};
}

sub revert_compare
{
	my ($b, $a) = @_;
	return $a->string cmp $b->string;
}

sub compare
{
	my ($a, $b) = @_;
	return $a->string cmp $b->string;
}

package OpenBSD::LibrarySpec;
sub new
{
	my ($class, $stem, $major, $minor) = @_;
	bless {stem => $stem, major => $major, minor => $minor}, $class;
}

sub to_string
{
	my $self = shift;
	return join('.', $self->{stem}, $self->{major}, $self->{minor});
}

sub compare
{
	my ($a, $b) = @_;
	
	if ($a->{stem} ne $b->{stem}) {
		return undef;
	}
	if ($a->{major} != $b->{major}) {
		return $a->{major} <=> $b->{major};
	}
	return $a->{minor} <=> $b->{minor};
}

1;
