# ex:ts=8 sw=4:
# $OpenBSD: Signature.pm,v 1.22 2018/12/16 10:45:38 espie Exp $
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

package OpenBSD::PackingElement::VersionElement;
sub signature
{
	my ($self, $hash) = @_;
	$hash->{$self->signature_key} = $self;
}

sub always
{
	return 1;
}
package OpenBSD::PackingElement::Dependency;
sub signature_key
{
	my $self = shift;
	return $self->{pkgpath};
}

sub sigspec
{
	my $self = shift;
	return OpenBSD::PackageName->from_string($self->{def});
}

sub long_string
{
	my $self = shift;
	return '@'.$self->sigspec->to_string;
}

sub compare
{
	my ($a, $b) = @_;
	return $a->sigspec->compare($b->sigspec);
}

sub always
{
	return 0;
}

package OpenBSD::PackingElement::Wantlib;
sub signature_key
{
	my $self = shift;
	my $spec = $self->spec;
	if ($spec->is_valid) {
		return $spec->key;
	} else {
		return "???";
	}
}

sub compare
{
	my ($a, $b) = @_;
	return $a->spec->compare($b->spec);
}

sub long_string
{
	my $self = shift;
	return $self->spec->to_string;
}

sub always
{
	return 1;
}

package OpenBSD::PackingElement::Version;
sub signature_key
{
	return 'VERSION';
}

sub long_string
{
	my $self = shift;
	return $self->{name};
}

sub compare
{
	my ($a, $b) = @_;
	return $a->{name} <=> $b->{name};
}

package OpenBSD::Signature;
sub from_plist
{
	my ($class, $plist) = @_;

	my $k = {};
	$plist->visit('signature', $k);

	$k->{VERSION} //= OpenBSD::PackingElement::Version->new(0);

	if ($plist->has('always-update')) {
		return $class->full->new($plist->pkgname, $k, $plist);
	} else {
		return $class->new($plist->pkgname, $k);
	}
}

sub full
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
	return join(',', $self->{name}, sort map {$_->long_string} values %{$self->{extra}});
}

sub compare
{
	my ($a, $b, $shortened) = @_;
	return $b->revert_compare($a, $shortened);
}

sub revert_compare
{
	my ($b, $a, $shortened) = @_;

	if ($a->{name} eq $b->{name}) {
		my $awins = 0;
		my $bwins = 0;
		my $done = {};
		my $errors = 0;
		while (my ($k, $v) = each %{$a->{extra}}) {
			if (!defined $b->{extra}{$k}) {
				print STDERR "Couldn't find $k in second signature\n";
				$errors++;
				next;
			}
			$done->{$k} = 1;
			next if $shortened && !$v->always;
			my $r = $v->compare($b->{extra}{$k});
			if ($r > 0) {
				$awins++;
			} elsif ($r < 0) {
				$bwins++;
			}
		}
		for my $k (keys %{$b->{extra}}) {
			if (!$done->{$k}) {
				print STDERR "Couldn't find $k in first signature\n";
				$errors++;
			}
		}
		if ($errors) {
			$a->print_error($b);
			return undef;
		}
		if ($awins == 0) {
			return -$bwins;
		} elsif ($bwins == 0) {
			return $awins;
		} else {
			return undef;
		}
	} else {
		return OpenBSD::PackageName->from_string($a->{name})->compare(OpenBSD::PackageName->from_string($b->{name}));
	}
}

sub print_error
{
	my ($a, $b) = @_;

	print STDERR "Error: $a->{name} exists in two non-comparable versions\n";
	print STDERR "Someone forgot to bump a REVISION\n";
	print STDERR $a->string, " vs. ", $b->string, "\n";
}

package OpenBSD::Signature::Full;
our @ISA=qw(OpenBSD::Signature);

sub new
{
	my ($class, $pkgname, $extra, $plist) = @_;
	my $o = $class->SUPER::new($pkgname, $extra);
	my $hash;
	open my $fh, '>', \$hash;
	$plist->write_without_variation($fh);
	close $fh;
	$o->{hash} = $hash;
	return $o;
}

sub string
{
	my $self = shift;
	return join(',', $self->SUPER::string, $self->{hash});
}

sub revert_compare
{
	my ($b, $a) = @_;
	my $r = $b->SUPER::revert_compare($a);
	if (defined $r && $r == 0) {
		if ($a->string ne $b->string) {
			return undef;
		}
	}
	return $r;
}

sub compare
{
	my ($a, $b) = @_;
	my $r = $a->SUPER::compare($b);
	if (defined $r && $r == 0) {
		if ($a->string ne $b->string) {
			return undef;
		}
	}
	return $r;
}

1;
