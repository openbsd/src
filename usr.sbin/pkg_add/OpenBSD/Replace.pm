# ex:ts=8 sw=4:
# $OpenBSD: Replace.pm,v 1.7 2007/05/02 15:09:04 espie Exp $
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
#

use strict;
use warnings;

use OpenBSD::Delete;
use OpenBSD::Interactive;

package OpenBSD::PackingElement;
sub can_update
{
	my ($self, $install, $state) = @_;

	my $issue = $self->update_issue($install);
	
	if (defined $issue) {
		$state->{okay} = 0;
	    	push(@{$state->{journal}}, $issue);
	}
}

sub validate_depend
{
}

sub update_issue($$) { undef }

sub extract
{
}

sub mark_lib
{
}

sub unmark_lib
{
}

package OpenBSD::PackingElement::FileBase;
use File::Temp qw/tempfile/;

sub extract
{
	my ($self, $state) = @_;

	my $file = $self->prepare_to_extract($state);

	if (defined $self->{link} || defined $self->{symlink}) {
		$self->{zap} = 1;
		return;
	}
	
	my $d = dirname($file->{destdir}.$file->{name});
	while (!-d $d && -e _) {
		$d = dirname($d);
	}
	if ($state->{not}) {
		print "extracting tempfile under $d\n" if $state->{very_verbose};
		$state->{archive}->skip;
	} else {
		if (!-e _) {
			File::Path::mkpath($d);
		}
		my ($fh, $tempname) = tempfile('pkg.XXXXXXXXXX', 
		    DIR => $d);

		print "extracting $tempname\n" if $state->{very_verbose};
		$file->{name} = $tempname;
		$self->{tempname} = $tempname;
		$file->create;
	}
}

package OpenBSD::PackingElement::Dir;
sub extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	return if -e $destdir.$fullname;
	print "new directory ", $destdir, $fullname, "\n" if $state->{very_verbose};
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
sub update_issue($$) 
{ 
	return $_[0]->{name}." script";
}

package OpenBSD::PackingElement::FINSTALL;
sub update_issue($$) 
{ 
	return if !$_[1];
	return $_[0]->SUPER::update_issue($_[1]);
}

package OpenBSD::PackingElement::FDEINSTALL;
sub update_issue($$) 
{ 
	return if $_[1];
	return $_[0]->SUPER::update_issue($_[1]);
}

package OpenBSD::PackingElement::Exec;
sub update_issue($$) 
{ 
	return if !$_[1];
	return '@exec '.$_[0]->{expanded};
}

package OpenBSD::PackingElement::Unexec;
sub update_issue($$) 
{ 
	return if $_[1];
	my $self = $_[0];

	# those are deemed innocuous
	if ($self->{expanded} =~ m|^/sbin/ldconfig\s+\-R\b| or
	    $self->{expanded} =~ m|^install-info\s+\-\-delete\b|) {
		return;
	} else {
		return '@unexec '.$self->{expanded};
	}
}

package OpenBSD::PackingElement::Depend;
use OpenBSD::Error;

sub check_replacement_spec
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	# nothing to validate if old dependency doesn't concern us.
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	# nothing to do if new dependency just matches
	return if OpenBSD::PkgSpec::match($self->{pattern}, $replacement);

	if ($state->{forced}->{updatedepends}) {
	    Warn "Forward dependency of $wanting on $toreplace doesn't match $replacement, forcing it\n";
	    $state->{forcedupdates} = {} unless defined $state->{forcedupdates};
	    $state->{forcedupdates}->{$wanting} = 1;
	} elsif ($state->{interactive}) {

	    if (OpenBSD::Interactive::confirm("Forward dependency of $wanting on $toreplace doesn't match $replacement, proceed with update anyways", 1, 0, 'updatedepends')) {
		$state->{forcedupdates} = {} unless defined $state->{forcedupdates};
		$state->{forcedupdates}->{$wanting} = 1;
	    } else {
		$state->{okay} = 0;
	    }
	} else {
	    $state->{okay} = 0;
	    Warn "Can't update forward dependency of $wanting on $toreplace: $replacement doesn't match (use -F updatedepends to force it)\n";
	}
}

package OpenBSD::PackingElement::LibDepend;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	$self->check_replacement_spec($state, $wanting, $toreplace, $replacement);
}

package OpenBSD::PackingElement::Lib;
sub mark_lib
{
	my ($self, $libs, $libpatterns) = @_;
	my $libname = $self->fullname;
	if ($libname =~ m/^(.*\.so\.)(\d+)\.(\d+)$/) {
		$libpatterns->{$1} = [$2, $3, $libname];
	}
	$libs->{$libname} = 1;
}

sub unmark_lib
{
	my ($self, $libs, $libpatterns) = @_;
	my $libname = $self->fullname;
	if ($libname =~ m/^(.*\.so\.)(\d+)\.(\d+)$/) {
		my ($pat, $major, $minor) = ($1, $2, $3);
		my $p = $libpatterns->{$pat};
		if (defined $p && $p->[0] == $major && $p->[1] <= $minor) {
			my $n = $p->[2];
			delete $libs->{$n};
		}
	}
	delete $libs->{$libname};
}

package OpenBSD::PackingElement::NewDepend;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	$self->check_replacement_spec($state, $wanting, $toreplace, $replacement);
}

package OpenBSD::PackingElement::Dependency;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	$self->check_replacement_spec($state, $wanting, $toreplace, $replacement);
}

package OpenBSD::Replace;
use OpenBSD::RequiredBy;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;
use OpenBSD::Error;
use OpenBSD::Interactive;

sub can_do
{
	my ($toreplace, $replacement, $state, $ignore) = @_;

	$state->{okay} = 1;
	$state->{libs_to_check} = [];
	my $plist = OpenBSD::PackingList->from_installation($toreplace);
	if (!defined $plist) {
		Fatal "Couldn't find packing-list for $toreplace\n";
	}
	$state->{journal} = [];
	$plist->can_update(0, $state);
	if ($state->{okay} == 0) {
		Warn "Old package ", $plist->pkgname, " contains potentially unsafe operations\n";
		for my $i (@{$state->{journal}}) {
			Warn "\t$i\n";
		}
		if ($state->{forced}->{update}) {
			Warn "(forcing update)\n";
			$state->{okay} = 1;
		} elsif ($state->{interactive}) {

			if (OpenBSD::Interactive::confirm("proceed with update anyways", 1, 0, 'update')) {
			    $state->{okay} = 1;
			}
		}
	}
	my @wantlist = OpenBSD::RequiredBy->new($toreplace)->list;
	my @r = ();
	for my $wanting (@wantlist) {
		push(@r, $wanting) if !defined $ignore->{$wanting};
	}
	if (@r) {
		print "Verifying dependencies still match for ", 
		    join(', ', @r), "\n" if $state->{verbose};
		for my $wanting (@wantlist) {
			my $p2 = OpenBSD::PackingList->from_installation(
			    $wanting, \&OpenBSD::PackingList::DependOnly);
			if (!defined $p2) {
				Warn "Error: $wanting missing from installation\n"
			} else {
				$p2->validate_depend($state, $wanting, 
				    $toreplace, $replacement);
			}
		}
	}

	if ($state->{okay}) {
		try {
			OpenBSD::Delete::validate_plist($plist, $state);
		} catchall {
			Warn "$_";
			return 0;
		};
	}

	$plist->{wantlist} = \@wantlist;
	$plist->{libs_to_check} = $state->{libs_to_check};
	
	return $state->{okay} ? $plist : 0;
}

sub is_safe
{
	my ($plist, $state) = @_;
	$state->{okay} = 1;
	$state->{journal} = [];
	$plist->can_update(1, $state);
	if ($state->{okay} == 0) {
		Warn "New package ", $plist->pkgname, 
		    " contains potentially unsafe operations\n";
		for my $i (@{$state->{journal}}) {
			Warn "\t$i\n";
		}
		if ($state->{forced}->{update}) {
			Warn "(forcing update)\n";
			$state->{okay} = 1;
		} elsif ($state->{interactive}) {
			if (OpenBSD::Interactive::confirm("proceed with update anyways", 1, 0, 'update')) {
			    $state->{okay} = 1;
			}
		}
	}
	return $state->{okay};
}

# create a packing-list with only the libraries we want to keep around.
sub split_libs
{
	my ($plist, $to_split) = @_;

	my $items = [];

	my $splitted = OpenBSD::PackingList->new;

	OpenBSD::PackingElement::Name->add($splitted, ".libs-".$plist->pkgname);
	if (defined $plist->{conflict}) {
		for my $item (@{$plist->{conflict}}) {
			$item->clone()->add_object($splitted);
		}
	}
	if (defined $plist->{pkgcfl}) {
		for my $item (@{$plist->{pkgcfl}}) {
			$item->clone()->add_object($splitted);
		}
	}
	if (defined $plist->{'no-default-conflict'}) {
		# we conflict with the package we just removed...
		OpenBSD::PackingElement::Conflict->add($splitted, $plist->pkgname);
	} else {
		require OpenBSD::PackageName;

		my $stem = OpenBSD::PackageName::splitstem($plist->pkgname);
		OpenBSD::PackingElement::Conflict->add($splitted, $stem."-*");
	}

	for my $item (@{$plist->{items}}) {
		if ($item->isa("OpenBSD::PackingElement::Lib") &&
		    defined $to_split->{$item->fullname}) {
		    	$item->clone->add_object($splitted);
			next;
		}
		if ($item->isa("OpenBSD::PackingElement::Cwd")) {
			$item->clone->add_object($splitted);
		}
		push(@$items, $item);
	}
	$plist->{items} = $items;
	return $splitted;
}

sub convert_to_requiring
{
	my $pkg = shift;

	my $plist = OpenBSD::PackingList->from_installation($pkg);
	if (!defined $plist) {
		Warn "Couldn't read plist for $pkg\n";
		return;
	}
	my $r = OpenBSD::Requiring->new($pkg);
	for my $item (@{$plist->{pkgdep}}) {
		$r->add($item->{name});
	}
	delete $plist->{pkgdep};
	$plist->to_installation;
}

sub walk_depends_closure
{
	my ($start, $plist, $state) = @_;
	my @todo = ($start);
	my $done = {};
	my $depend = 0;
	my $name = $plist->pkgname;

	print "Packages that depend on those shared libraries:\n" 
	    if $state->{beverbose};

	my $write = OpenBSD::RequiredBy->new($name);

	while (my $pkg = shift @todo) {
		$done->{$pkg} = 1;
		for my $pkg2 (OpenBSD::RequiredBy->new($pkg)->list) {
			next if $done->{$pkg2};
			push(@todo, $pkg2);
			print "\t$pkg2\n" if $state->{beverbose};
			$done->{$pkg2} = 1;
			$write->add($pkg2);
			my $l = OpenBSD::Requiring->new($pkg2);
			if (!$l->list) {
				convert_to_requiring($pkg2);
			}
			$l->add($name);
			$depend = 1;
		}
	}
#	if (!$depend && $state->{interactive}) {
#		if ($state->{forced}->{zapoldlibs} ||
#		    OpenBSD::Interactive::confirm("Nothing depends on $name.  Delete it", 1, 0)) {
#		    	OpenBSD::Delete::delete_plist($plist, $state);
#		}
#	}
}



sub save_old_libraries
{
	my ($new_plist, $state) = @_;

	for my $old_plist (@{$new_plist->{replacing}}) {

		my $libs = {};
		my $p = {};

		print "Looking for changes in shared libraries\n" 
		    if $state->{beverbose};
		$old_plist->mark_lib($libs, $p);
		$new_plist->unmark_lib($libs, $p);

		if (%$libs) {
			print "Libraries to keep: ", join(",", sort(keys %$libs)), "\n" 
			    if $state->{beverbose};
			my $stub_list = split_libs($old_plist, $libs);
			my $stub_name = $stub_list->pkgname;
			my $dest = installed_info($stub_name);
			print "Keeping them in $stub_name\n" if $state->{beverbose};
			if ($state->{not}) {
				$stub_list->to_cache;
				$old_plist->to_cache;
			} else {
				require OpenBSD::md5;

				mkdir($dest);
				my $oldname = $old_plist->pkgname;
				open my $descr, '>', $dest.DESC;
				print $descr "Stub libraries for $oldname\n";
				close $descr;
				my $f = OpenBSD::PackingElement::FDESC->add($stub_list, DESC);
				$f->{ignore} = 1;
				$f->{md5} = OpenBSD::md5::fromfile($dest.DESC);
				$stub_list->to_installation;
				$old_plist->to_installation;
			}
			add_installed($stub_name);

			require OpenBSD::PkgCfl;
			OpenBSD::PkgCfl::register($stub_list, $state);

			walk_depends_closure($old_plist->pkgname, $stub_list, $state);
		} else {
			print "No libraries to keep\n" if $state->{beverbose};
		}
	}
}

			
sub adjust_dependency
{
	my ($dep, $from, $into) = @_;

	my $l = OpenBSD::Requiring->new($dep);
	if (!$l->list) {
		convert_to_requiring($dep);
	}
	$l->delete($from);
	$l->add($into);
}

sub figure_out_libs
{
	my ($plist, $state, @libs) = @_;

	my $has = {};
	my $result = [];

	for my $item (@{$plist->{items}}) {
		next unless $item->IsFile;
		$has->{$item->fullname} = 1;
	}

	for my $oldlib (@libs) {
		print "Checking for collisions with $oldlib... " 
		    if $state->{verbose};

#		require OpenBSD::RequiredBy;
#
#		if (OpenBSD::RequiredBy->new($oldlib)->list == 0) {
#			require OpenBSD::Delete;
#
#			OpenBSD::Delete::delete_package($oldlib, $state);
#			delete_installed($oldlib);
#			next;
#		}

		my $p = OpenBSD::PackingList->from_installation($oldlib);
		my $n = [];
		my $delete = [];
		my $empty = 1;
		my $doit = 0;
		for my $file (@{$p->{items}}) {
			if ($file->IsFile) {
				if ($has->{$file->fullname}) {
					$doit = 1;
					push(@$delete, $file);
					next;
				} else {
					$empty = 0;
				}
			}
			push(@$n, $file);
		}
		$p->{items} = $n;
		if ($doit) {
			print "some found\n" if $state->{verbose};
			my $dummy = {items => $delete};
			push(@$result, 
			    { plist => $p, 
			      todelete => $dummy,
			      empty => $empty});
			require OpenBSD::Delete;
			OpenBSD::Delete::validate_plist($p, $state);
		} else {
			print "none found\n" if $state->{verbose};
		}
	}
	if (@$result) {
		$plist->{old_libs} = $result;
		return 0;
	}
	return 1;
}

1;
