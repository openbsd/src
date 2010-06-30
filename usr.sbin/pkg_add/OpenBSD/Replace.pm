# ex:ts=8 sw=4:
# $OpenBSD: Replace.pm,v 1.74 2010/06/30 10:09:09 espie Exp $
#
# Copyright (c) 2004-2010 Marc Espie <espie@openbsd.org>
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

sub extract
{
	my ($self, $state) = @_;
	$state->{partial}->{$self} = 1;
	if ($state->{interrupted}) {
		die "Interrupted";
	}
}

package OpenBSD::PackingElement::FileBase;
use OpenBSD::Temp;

sub extract
{
	my ($self, $state) = @_;

	my $file = $self->prepare_to_extract($state);

	if (defined $self->{link} || defined $self->{symlink}) {
		$state->{archive}->skip;
		return;
	}

	$self->SUPER::extract($state);

	# figure out a safe directory where to put the temp file
	my $d = dirname($file->{destdir}.$file->name);
	# we go back up until we find an existing directory.
	# hopefully this will be on the same file system.
	while (!-d $d && -e _ || defined $state->{noshadow}->{$d}) {
		$d = dirname($d);
	}
	if ($state->{not}) {
		$state->say("extracting tempfile under #1", $d)
		    if $state->verbose >= 3;
		$state->{archive}->skip;
	} else {
		if (!-e _) {
			File::Path::mkpath($d);
		}
		my ($fh, $tempname) = OpenBSD::Temp::permanent_file($d, "pkg");

		$state->say("extracting #1", $tempname) if $state->verbose >= 3;
		$self->{tempname} = $tempname;

		# XXX don't apply destdir twice
		$file->{destdir} = '';
		$file->set_name($tempname);
		$file->create;
		$self->may_check_digest($file, $state);
	}
}

package OpenBSD::PackingElement::Dir;
sub extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	return if -e $destdir.$fullname;
	$self->SUPER::extract($state);
	$state->say("new directory #1", $destdir.$fullname)
	    if $state->verbose >= 3;
	return if $state->{not};
	File::Path::mkpath($destdir.$fullname);
}


package OpenBSD::PackingElement::Sample;
sub extract
{
}

package OpenBSD::PackingElement::Sampledir;
sub extract
{
}

package OpenBSD::PackingElement::ScriptFile;
sub update_issue
{
	my ($self, $installing) = @_;
	return $self->name." script";
}

package OpenBSD::PackingElement::FINSTALL;
sub update_issue
{
	my ($self, $installing) = @_;
	return if !$installing;
	return $self->SUPER::update_issue($installing);
}

package OpenBSD::PackingElement::FDEINSTALL;
sub update_issue
{
	my ($self, $installing) = @_;
	return if $installing;
	return $self->SUPER::update_issue($installing);
}

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

sub perform_extraction
{
	my ($handle, $state) = @_;

	$handle->{partial} = {};
	$state->{partial} = $handle->{partial};
	$state->{archive} = $handle->{location};
	$state->progress->visit_with_size($handle->{plist}, 'extract', $state);
}

sub check_plist_exec
{
	my ($plist, $state, $new) = @_;

	$state->{journal} = [];
	$plist->can_update($new, $state);
	return 1 if @{$state->{journal}} == 0;

	$state->errsay(($new ? "New": "Old"). 
	    " package #1 contains potentially unsafe operations", $plist->pkgname);
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

	if ($state->defines('update') && !$state->verbose) {
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

	if ($state->defines('update')) {
		$state->errsay("Forcing update");
		return 1;
	} elsif ($state->{interactive}) {

		if ($state->confirm("proceed with update anyways", 0)) {
			return 1;
		} else {
			return 0;
		}
	} else {
		$state->errsay("Cannot install #1 (use -D update)",
		    $set->print);
		return 0;
    	}
}


1;
