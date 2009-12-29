# ex:ts=8 sw=4:
# $OpenBSD: ForwardDependencies.pm,v 1.4 2009/12/29 18:16:14 espie Exp $
#
# Copyright (c) 2009 Marc Espie <espie@openbsd.org>
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

# handling of forward dependency adjustments

use strict;
use warnings;

package OpenBSD::ForwardDependencies;

require OpenBSD::RequiredBy;

sub find
{
	my ($class, $set) = @_;
	my $forward = {};
	for my $old ($set->older) {
		for my $f (OpenBSD::RequiredBy->new($old->pkgname)->list) {
			next if defined $set->{older}->{$f};
			next if defined $set->{kept}->{$f};
			$forward->{$f} = 1;
		}
	}
	bless { forward => $forward, set => $set}, $class;
}

sub adjust
{
	my ($self, $state) = @_;
	my $set = $self->{set};

	for my $f (keys %{$self->{forward}}) {
		my $deps_f = OpenBSD::Requiring->new($f);
		for my $check ($deps_f->list) {
			if (defined $set->{older}->{$check}) {
				my $r = $set->{older}->{$check}->{update_found}->pkgname;
				$state->say("Adjusting $check to $r in $f")
				    if $state->verbose >= 3;
				if ($check ne $r) {
					$deps_f->delete($check)->add($r);
				}
				OpenBSD::RequiredBy->new($r)->add($f);
			}
		}
	}
}

sub check
{
	my ($self, $state) = @_;
	my $forward = $self->{forward};
	my $set = $self->{set};

	my @r = keys %$forward;

	my $result = {};

	return $result if @r == 0;
	$state->say("Verifying dependencies still match for ", 
	    join(', ', @r)) if $state->verbose >= 2;

	my @new = ($set->newer_names, $set->kept_names);
	my @old = $set->older_names;

	for my $f (@r) {
		my $p2 = OpenBSD::PackingList->from_installation(
		    $f, \&OpenBSD::PackingList::DependOnly);
		if (!defined $p2) {
			$state->errsay("Error: $f missing from installation");
		} else {
			$p2->check_forward_dependency($f, \@old, \@new, $result);
		}
	}
	if (%$result) {
		$state->say($set->print, " forward dependencies:");
		while (my ($pkg, $l) = each %$result) {
			my $deps = join(',', map {$_->{pattern}} @$l);
			if (@$l == 1) {
				$state->say("| Dependency of $pkg on $deps doesn't match");
			} else {
				$state->say("| Dependencies of $pkg on $deps don't match");
			}
		}
	}
	return $result;
}

package OpenBSD::PackingElement;
sub check_forward_dependency
{
}

package OpenBSD::PackingElement::Dependency;
sub check_forward_dependency
{
	my ($self, $f, $old, $new, $r) = @_;

	# nothing to validate if old dependency doesn't concern us.
	return unless $self->spec->filter(@$old);
	# nothing to do if new dependency just matches
	return if $self->spec->filter(@$new);

	push(@{$r->{$f}}, $self);
}

1;
