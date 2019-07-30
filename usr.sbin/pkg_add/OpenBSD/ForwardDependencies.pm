# ex:ts=8 sw=4:
# $OpenBSD: ForwardDependencies.pm,v 1.13 2012/04/28 15:24:52 espie Exp $
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
			my $h = $set->{older}->{$check};
			next unless defined $h;
			if (!defined $h->{update_found}) {
				$state->errsay("XXX #1", $check);
				$deps_f->delete($check);
			} else {
				# XXX proper OO wouldn't have ->is_real
				# but it would use double dispatch to record
				# every dependency.
				# ETOOMUCHSCAFFOLDING, quick&dirty hack
				# is much shorter and fairly localized.
				my $r = $h->{update_found};
				my $p = $r->pkgname;
				$state->say("Adjusting #1 to #2 in #3",
				    $check, $p, $f)
					if $state->verbose >= 3;
				if ($check ne $p) {
					if ($r->is_real) {
						$deps_f->delete($check)->add($p);
					} else {
						$deps_f->delete($check);
					}
				}
				if ($r->is_real) {
					OpenBSD::RequiredBy->new($p)->add($f);
				}
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
	$state->say("Verifying dependencies still match for #1",
	    join(', ', @r)) if $state->verbose >= 2;

	my @new = ($set->newer_names, $set->kept_names);
	my @old = $set->older_names;

	for my $f (@r) {
		my $p2 = OpenBSD::PackingList->from_installation(
		    $f, \&OpenBSD::PackingList::DependOnly);
		if (!defined $p2) {
			$state->errsay("Error: #1 missing from installation", $f);
		} else {
			$p2->check_forward_dependency($f, \@old, \@new, $result);
		}
	}
	if (%$result) {
		$state->say("#1 forward dependencies:", $set->print);
		while (my ($pkg, $l) = each %$result) {
			my $deps = join(',', map {$_->{pattern}} @$l);
			if (@$l == 1) {
				$state->say("| Dependency of #1 on #2 doesn't match",
				    $pkg, $deps);
			} else {
				$state->say("| Dependencies of #1 on #2 don't match",
				    $pkg, $deps);
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
