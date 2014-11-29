# ex:ts=8 sw=4:
# $OpenBSD: Replace.pm,v 1.89 2014/11/29 10:42:51 espie Exp $
#
# Copyright (c) 2004-2014 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

use OpenBSD::Delete;

package OpenBSD::PackingElement;
sub can_update
{
	my ($self, $installing, $state) = @_;

	my $issue = $self->update_issue($installing);

	if (defined $issue) {
	    	push(@{$state->{journal}}, $issue);
	}
}

sub update_issue { undef }

package OpenBSD::PackingElement::Exec;
sub update_issue
{
	my ($self, $installing) = @_;
	return if !$installing;
	return '@'.$self->keyword.' '.$self->{expanded};
}

package OpenBSD::PackingElement::ExecAdd;
sub update_issue { undef }

package OpenBSD::PackingElement::Unexec;
sub update_issue
{
	my ($self, $installing) = @_;

	return if $installing;

	return '@'.$self->keyword.' '.$self->{expanded};
}

package OpenBSD::PackingElement::UnexecDelete;
sub update_issue { undef }

package OpenBSD::Replace;

sub check_plist_exec
{
	my ($plist, $state, $new) = @_;

	$state->{journal} = [];
	$plist->can_update($new, $state);
	return 1 if @{$state->{journal}} == 0;

	$state->errsay(($new ? "New": "Old").
	    " package #1 will run the following commands", $plist->pkgname);
	for my $i (@{$state->{journal}}) {
		if ($new) {
			$state->errsay("+ #1", $i);
		} else {
			$state->errsay("- #1", $i);
		}
	}
	return 0;
}

sub can_old_package_be_replaced
{
	my ($plist, $state) = @_;
	return check_plist_exec($plist, $state, 0);
}

sub is_new_package_safe
{
	my ($plist, $state) = @_;
	return check_plist_exec($plist, $state, 1);
}

sub is_set_safe
{
	my ($set, $state) = @_;

	if (!$state->defines('paranoid') && !$state->verbose) {
		return 1;
	}

	my $ok = 1;

	for my $pkg ($set->older) {
		$ok = 0 unless can_old_package_be_replaced($pkg->plist, $state);
	}
	for my $pkg ($set->newer) {
		$ok = 0 unless is_new_package_safe($pkg->plist, $state);
	}
	return 1 if $ok;

	if (!$state->defines('paranoid')) {
		$state->errsay("Running update");
		return 1;
	} elsif ($state->is_interactive) {
		if ($state->confirm("proceed with update anyway", 0)) {
			return 1;
		} else {
			return 0;
		}
	} else {
		$state->errsay("Cannot install #1",
		    $set->print);
		return 0;
    	}
}


1;
