# ex:ts=8 sw=4:
# $OpenBSD: Replace.pm,v 1.57 2009/11/29 11:22:25 espie Exp $
#
# Copyright (c) 2004-2006 Marc Espie <espie@openbsd.org>
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
		$state->{okay} = 0;
	    	push(@{$state->{journal}}, $issue);
	}
}

sub validate_depend
{
}

sub update_issue { undef }

sub extract
{
	my ($self, $state) = @_;
	$state->{partial}->{$self} = 1;
}

sub mark_lib
{
}

sub unmark_lib
{
}

sub separate_element
{
	my ($self, $libs, $c1, $c2) = @_;
	$c2->{$self} = 1;
}

sub extract_and_progress
{
	my ($self, $state, $donesize, $totsize) = @_;
	$self->extract($state);
	if ($state->{interrupted}) {
		die "Interrupted";
	}
	$self->mark_progress($state->progress, $donesize, $totsize);
}

package OpenBSD::PackingElement::Meta;

sub separate_element
{
	my ($self, $libs, $c1, $c2) = @_;
	$c1->{$self} = 1;
	$c2->{$self} = 1;
}

package OpenBSD::PackingElement::DigitalSignature;
sub separate_element
{
	my ($self, $libs, $c1, $c2) = @_;
	$c2->{$self} = 1;
}

package OpenBSD::PackingElement::State;

sub separate_element
{
	&OpenBSD::PackingElement::Meta::separate_element;
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
		$state->say("extracting tempfile under $d") 
		    if $state->{very_verbose};
		$state->{archive}->skip;
	} else {
		if (!-e _) {
			File::Path::mkpath($d);
		}
		my ($fh, $tempname) = OpenBSD::Temp::permanent_file($d, "pkg");

		$state->say("extracting $tempname") if $state->{very_verbose};
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
	$state->say("new directory ", $destdir, $fullname) 
	    if $state->{very_verbose};
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

package OpenBSD::PackingElement::Depend;
sub separate_element
{
	&OpenBSD::PackingElement::separate_element;
}

package OpenBSD::PackingElement::SpecialFile;
sub separate_element
{
	&OpenBSD::PackingElement::separate_element;
}

package OpenBSD::PackingElement::Dependency;
use OpenBSD::Error;

sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, @replacement) = @_;

	# nothing to validate if old dependency doesn't concern us.
	return unless $self->spec->filter($toreplace);
	# nothing to do if new dependency just matches
	return if $self->spec->filter(@replacement);

	if ($state->{defines}->{updatedepends}) {
	    $state->errsay("Forward dependency of $wanting on $toreplace doesn't match @replacement, forcing it");
	    $state->{forcedupdates} = {} unless defined $state->{forcedupdates};
	    $state->{forcedupdates}->{$wanting} = 1;
	} elsif ($state->{interactive}) {

	    if ($state->confirm("Forward dependency of $wanting on $toreplace doesn't match @replacement, proceed with update anyways", 0)) {
		$state->{forcedupdates} = {} unless defined $state->{forcedupdates};
		$state->{forcedupdates}->{$wanting} = 1;
	    } else {
		$state->{okay} = 0;
	    }
	} else {
	    $state->{okay} = 0;
	    $state->errsay("Can't update forward dependency of $wanting on $toreplace: @replacement doesn't match (use -F updatedepends to force it)");
	}
}

package OpenBSD::PackingElement::Lib;
sub mark_lib
{
	my ($self, $libs, $libpatterns) = @_;
	my $libname = $self->fullname;
	my ($stem, $major, $minor, $dir) = $self->parse($libname);
	if (defined $stem) {
		$libpatterns->{$stem}->{$dir} = [$major, $minor, $libname];
	}
	$libs->{$libname} = 1;
}

sub separate_element
{
	my ($self, $libs, $c1, $c2) = @_;
	if ($libs->{$self->fullname}) {
		$c1->{$self} = 1;
	} else {
		$c2->{$self} = 1;
	}
}

sub unmark_lib
{
	my ($self, $libs, $libpatterns) = @_;
	my $libname = $self->fullname;
	my ($stem, $major, $minor, $dir) = $self->parse($libname);
	if (defined $stem) {
		my $p = $libpatterns->{$stem}->{$dir};
		if (defined $p && $p->[0] == $major && $p->[1] <= $minor) {
			my $n = $p->[2];
			delete $libs->{$n};
		}
	}
	delete $libs->{$libname};
}

package OpenBSD::Replace;
use OpenBSD::RequiredBy;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;
use OpenBSD::Error;

sub perform_extraction
{
	my ($handle, $state) = @_;

	$handle->{partial} = {};
	$state->{partial} = $handle->{partial};
	my $totsize = $handle->{totsize};
	$state->{archive} = $handle->{location};
	my $donesize = 0;
	$state->{donesize} = 0;
	$handle->{plist}->extract_and_progress($state, \$donesize, $totsize);
}

sub can_old_package_be_replaced
{
	my ($old_plist, $set, $state) = @_;

	$state->{okay} = 1;
	$state->{journal} = [];
	$old_plist->can_update(0, $state);
	if ($state->{okay} == 0) {
		$state->errsay("Old package ", $old_plist->pkgname, 
		    " contains potentially unsafe operations");
		for my $i (@{$state->{journal}}) {
			$state->errsay("\t", $i);
		}
		if ($state->{defines}->{update}) {
			$state->errsay("(forcing update)");
			$state->{okay} = 1;
		} elsif ($state->{interactive}) {

			if ($state->confirm("proceed with update anyways", 0)) {
			    $state->{okay} = 1;
			}
		}
	}
	my @wantlist = OpenBSD::RequiredBy->new($old_plist->pkgname)->list;
	my @r = ();
	for my $wanting (@wantlist) {
		push(@r, $wanting) if !defined $set->{older}->{$wanting};
	}
	if (@r) {
		$state->say("Verifying dependencies still match for ", 
		    join(', ', @r)) if $state->{verbose};
		for my $wanting (@wantlist) {
			my $p2 = OpenBSD::PackingList->from_installation(
			    $wanting, \&OpenBSD::PackingList::DependOnly);
			if (!defined $p2) {
				$state->errsay("Error: $wanting missing from installation");
			} else {
				$p2->validate_depend($state, $wanting, 
				    $old_plist->pkgname, $set->newer_names);
			}
		}
	}
	return $state->{okay};
}

sub is_new_package_safe
{
	my ($plist, $state) = @_;
	$state->{okay} = 1;
	$state->{journal} = [];
	$plist->can_update(1, $state);
	if ($state->{okay} == 0) {
		$state->errsay("New package ", $plist->pkgname, 
		    " contains potentially unsafe operations");
		for my $i (@{$state->{journal}}) {
			$state->errsay("\t", $i);
		}
		if ($state->{defines}->{update}) {
			$state->errsay("(forcing update)");
			$state->{okay} = 1;
		} elsif ($state->{interactive}) {
			if ($state->confirm("proceed with update anyways", 0)) {
			    $state->{okay} = 1;
			}
		}
	}
	return $state->{okay};
}

sub split_some_libs
{
	my ($plist, $libs) = @_;
	my $c1 = {};
	my $c2 = {};
	$plist->separate_element($libs, $c1, $c2);
	my $p1 = $plist->make_deep_copy($c1);
	my $p2 = $plist->make_shallow_copy($c2);
	return ($p1, $p2);
}

# create a packing-list with only the libraries we want to keep around.
sub split_libs
{
	my ($plist, $to_split) = @_;

	(my $splitted, $plist) = split_some_libs($plist, $to_split);

	require OpenBSD::PackageInfo;

	$splitted->set_pkgname(OpenBSD::PackageInfo::libs_package($plist->pkgname));

	if (defined $plist->{'no-default-conflict'}) {
		# we conflict with the package we just removed...
		OpenBSD::PackingElement::Conflict->add($splitted, $plist->pkgname);
	} else {
		require OpenBSD::PackageName;

		my $stem = OpenBSD::PackageName::splitstem($plist->pkgname);
		OpenBSD::PackingElement::Conflict->add($splitted, $stem."-*");
	}
	return ($plist, $splitted);
}

sub adjust_depends_closure
{
	my ($oldname, $plist, $state) = @_;

	$state->say("Packages that depend on those shared libraries:") 
	    if $state->{beverbose};

	my $write = OpenBSD::RequiredBy->new($plist->pkgname);
	for my $pkg (OpenBSD::RequiredBy->compute_closure($oldname)) {
		$state->say("\t", $pkg) if $state->{beverbose};
		$write->add($pkg);
		OpenBSD::Requiring->new($pkg)->add($plist->pkgname);
	}
}


sub save_old_libraries
{
	my ($set, $state) = @_;

	for my $o ($set->older) {

		my $oldname = $o->pkgname;
		my $libs = {};
		my $p = {};

		$state->say("Looking for changes in shared libraries") 
		    if $state->{beverbose};
		$o->{plist}->mark_lib($libs, $p);
		for my $n ($set->newer) {
			$n->{plist}->unmark_lib($libs, $p);
		}

		if (%$libs) {
			$state->say("Libraries to keep: ", 
			    join(",", sort(keys %$libs))) 
			    if $state->{verbose};
			($o->{plist}, my $stub_list) = split_libs($o->{plist}, $libs);
			my $stub_name = $stub_list->pkgname;
			my $dest = installed_info($stub_name);
			$state->say("Keeping them in $stub_name") 
			    if $state->{verbose};
			if ($state->{not}) {
				$stub_list->to_cache;
				$o->{plist}->to_cache;
			} else {
				mkdir($dest);
				open my $descr, '>', $dest.DESC;
				print $descr "Stub libraries for $oldname\n";
				close $descr;
				my $f = OpenBSD::PackingElement::FDESC->add($stub_list, DESC);
				$f->{ignore} = 1;
				$f->add_digest($f->compute_digest($dest.DESC));
				$stub_list->to_installation;
				$o->{plist}->to_installation;
			}
			add_installed($stub_name);

			require OpenBSD::PkgCfl;
			OpenBSD::PkgCfl::register($stub_list, $state);

			adjust_depends_closure($oldname, $stub_list, $state);
		} else {
			$state->say("No libraries to keep") 
			    if $state->{verbose};
		}
	}
}

			
sub adjust_dependency
{
	my ($dep, $from, $into) = @_;

	my $l = OpenBSD::Requiring->new($dep);
	$l->delete($from);
	$l->add($into);
}

1;
