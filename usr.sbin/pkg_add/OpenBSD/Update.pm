# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.46 2004/12/20 22:43:25 espie Exp $
#
# Copyright (c) 2004 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackingElement;
sub can_update
{
	my ($self, $install, $state) = @_;

	if (!$self->updatable($install)) {
		$state->{okay} = 0;
	}
}

sub validate_depend
{
}

sub updatable($$) { 1 }

sub extract
{
}

sub mark_lib
{
}

sub unmark_lib
{
}

sub build_context
{
}

package OpenBSD::PackingElement::FileBase;
use File::Temp qw/tempfile/;

sub extract
{
	my ($self, $state) = @_;

	my $file = $self->prepare_to_extract($state);

	if (defined $self->{link} || defined $self->{symlink}) {
		return;
	}
	
	my $d = dirname($file->{destdir}.$file->{name});
	while (!-d $d && -e _) {
		$d = dirname($d);
	}
	if ($state->{not}) {
		print "extracting tempfile under $d\n" if $state->{very_verbose};
	} else {
		if (!-e _) {
			File::Path::mkpath($d);
		}
		my ($fh, $tempname) = tempfile('pkg.XXXXXXXXXX', 
		    DIR => $d);

		print "extracting $tempname\n" if $state->{very_verbose};
		$file->{name} = $tempname;
		$self->{tempname} = $tempname;
		$file->create();
	}
}

package OpenBSD::PackingElement::Dir;
sub extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname();
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
sub updatable($$) { 0 }

package OpenBSD::PackingElement::FINSTALL;
sub updatable($$) { !$_[1] }

package OpenBSD::PackingElement::FDEINSTALL;
sub updatable($$) { $_[1] }

package OpenBSD::PackingElement::Exec;
sub updatable($$) { !$_[1] }

package OpenBSD::PackingElement::Unexec;
sub updatable($$) 
{ 
	return 1 if $_[1];
	my $self = $_[0];

	# those are deemed innocuous
	if ($self->{expanded} =~ m|^/sbin/ldconfig\s+\-R\b| or
	    $self->{expanded} =~ m|^install-info\s+\-\-delete\b|) {
		return 1;
	} else {
		return 0;
	}
}

package OpenBSD::PackingElement::LibDepend;
use OpenBSD::Error;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$state->{okay} = 0;
		Warn "Can't update forward dependency of $wanting on $toreplace\n";
	}
}

package OpenBSD::PackingElement::Lib;
sub mark_lib
{
	my ($self, $libs, $libpatterns) = @_;
	my $libname = $self->fullname();
	if ($libname =~ m/^(.*\.so\.)(\d+)\.(\d+)$/) {
		$libpatterns->{"$1"} = [$2, $3, $libname];
	}
	$libs->{"$libname"} = 1;
}

sub unmark_lib
{
	my ($self, $libs, $libpatterns) = @_;
	my $libname = $self->fullname();
	if ($libname =~ m/^(.*\.so\.)(\d+)\.(\d+)$/) {
		my ($pat, $major, $minor) = ($1, $2, $3);
		my $p = $libpatterns->{"$pat"};
		if (defined $p && $p->[0] == $major && $p->[1] <= $minor) {
			my $n = $p->[2];
			delete $libs->{"$n"};
		}
	}
	delete $libs->{"$libname"};
}

package OpenBSD::PackingElement::Wantlib;
sub build_context
{
	my ($self, $hash) = @_;
	$hash->{$self->{name}} = 1;
}

package OpenBSD::PackingElement::Depend;
sub build_context
{
	my ($self, $hash) = @_;
	$hash->{$self->{def}} = 1;
}

package OpenBSD::PackingElement::PkgDep;
sub build_context
{
}

package OpenBSD::PackingElement::NewDepend;
use OpenBSD::Error;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$state->{okay} = 0;
		Warn "Can't update forward dependency of $wanting on $toreplace\n";
	}
}

package OpenBSD::PackingElement::Dependency;
use OpenBSD::Error;

sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$state->{okay} = 0;
		Warn "Can't update forward dependency of $wanting on $toreplace\n";
	}
}

package OpenBSD::Update;
use OpenBSD::RequiredBy;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;
use OpenBSD::Error;

sub can_do
{
	my ($toreplace, $replacement, $state) = @_;

	$state->{okay} = 1;
	$state->{libs_to_check} = [];
	my $plist = OpenBSD::PackingList->from_installation($toreplace);
	if (!defined $plist) {
		Fatal "Couldn't find packing-list for $toreplace\n";
	}
	$plist->visit('can_update', 0, $state);
	if ($state->{okay} == 0) {
		Warn "Old package ", $plist->pkgname(), " contains unsafe operations\n";
	}
	if ($state->{forced}->{update}) {
		$state->{okay} = 1;
	}
	my @wantlist = OpenBSD::RequiredBy->new($toreplace)->list();
	for my $wanting (@wantlist) {
		print "Verifying dependencies still match for $wanting\n" if $state->{verbose};
		my $p2 = OpenBSD::PackingList->from_installation($wanting,
		    \&OpenBSD::PackingList::DependOnly);
		$p2->visit('validate_depend', $state, $wanting, $toreplace, $replacement);
	}

	if ($state->{forced}->{updatedepends}) {
		$state->{okay} = 1;
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
	$plist->visit('can_update', 1, $state);
	if ($state->{okay} == 0) {
		Warn "New package ", $plist->pkgname(), 
		    " contains unsafe operations\n";
	}
	return $state->{okay} || $state->{forced}->{update};
}

# create a packing-list with only the libraries we want to keep around.
sub split_libs
{
	my ($plist, $to_split) = @_;

	my $items = [];

	my $splitted = OpenBSD::PackingList->new();

	OpenBSD::PackingElement::Name->add($splitted, ".libs-".$plist->pkgname());
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
		OpenBSD::PackingElement::Conflict->add($splitted, $plist->pkgname());
	} else {
		require OpenBSD::PackageName;

		my $stem = OpenBSD::PackageName::splitstem($plist->pkgname());
		OpenBSD::PackingElement::Conflict->add($splitted, $stem."-*");
	}

	for my $item (@{$plist->{items}}) {
		if ($item->isa("OpenBSD::PackingElement::Lib") &&
		    defined $to_split->{$item->fullname()}) {
		    	$item->clone()->add_object($splitted);
			next;
		}
		if ($item->isa("OpenBSD::PackingElement::Cwd")) {
			$item->clone()->add_object($splitted);
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
	my $r = OpenBSD::Requiring->new($pkg);
	for my $item (@{$plist->{pkgdep}}) {
		$r->add($item->{name});
	}
	delete $plist->{pkgdep};
	$plist->to_installation();
}

sub walk_depends_closure
{
	my ($start, $name, $state) = @_;
	my @todo = ($start);
	my $done = {};

	print "Packages that depend on those shared libraries:\n" 
	    if $state->{beverbose};

	my $write = OpenBSD::RequiredBy->new($name);

	while (my $pkg = shift @todo) {
		$done->{$pkg} = 1;
		for my $pkg2 (OpenBSD::RequiredBy->new($pkg)->list()) {
			next if $done->{$pkg2};
			push(@todo, $pkg2);
			print "\t$pkg2\n" if $state->{beverbose};
			$done->{$pkg2} = 1;
			$write->add($pkg2);
			my $l = OpenBSD::Requiring->new($pkg2);
			if (!$l->list()) {
				convert_to_requiring($pkg2);
			}
			$l->add($name);
		}
	}
}



sub save_old_libraries
{
	my ($new_plist, $state) = @_;

	for my $old_plist (@{$new_plist->{replacing}}) {

		my $libs = {};
		my $p = {};

		print "Looking for changes in shared libraries\n" 
		    if $state->{beverbose};
		$old_plist->visit('mark_lib', $libs, $p);
		$new_plist->visit('unmark_lib', $libs, $p);

		if (%$libs) {
			print "Libraries to keep: ", join(",", sort(keys %$libs)), "\n" 
			    if $state->{beverbose};
			my $stub_list = split_libs($old_plist, $libs);
			my $stub_name = $stub_list->pkgname();
			my $dest = installed_info($stub_name);
			print "Keeping them in $stub_name\n" if $state->{beverbose};
			if ($state->{not}) {
				$stub_list->to_cache();
				$old_plist->to_cache();
			} else {
				mkdir($dest);
				my $oldname = $old_plist->pkgname();
				open my $comment, '>', $dest.COMMENT;
				print $comment "Stub libraries for $oldname";
				close $comment;
				link($dest.COMMENT, $dest.DESC);
				$stub_list->to_installation();
				$old_plist->to_installation();
			}
			add_installed($stub_name);

			walk_depends_closure($old_plist->pkgname(), $stub_name, $state);
		} else {
			print "No libraries to keep\n" if $state->{beverbose};
		}
	}
}

			
sub adjust_dependency
{
	my ($dep, $from, $into) = @_;

	my $l = OpenBSD::Requiring->new($dep);
	if (!$l->list()) {
		convert_to_requiring($dep);
	}
	$l->delete($from);
	$l->add($into);
}

sub is_needed
{
	my ($plist, $state) = @_;
	my $new_context = {};
	$plist->visit('build_context', $new_context);
	my $oplist = OpenBSD::PackingList->from_installation($plist->pkgname());
	my $old_context = {};
	$oplist->visit('build_context', $old_context);
	my $n = join(',', sort keys %$new_context);
	my $o = join(',', sort keys %$old_context);
	print "Comparing full signature \"$o\" vs. \"$n\"\n" 
	    if $state->{very_verbose};
	return join(',', sort keys %$new_context) ne 
	    join(',', sort keys %$old_context);
}

sub figure_out_libs
{
	my ($plist, $state, @libs) = @_;

	my $has = {};
	my $result = [];

	for my $item (@{$plist->{items}}) {
		next unless $item->IsFile();
		$has->{$item->fullname()} = 1;
	}

	for my $oldlib (@libs) {
		print "Checking for collisions with $oldlib... " 
		    if $state->{verbose};

#		require OpenBSD::RequiredBy;
#
#		if (OpenBSD::RequiredBy->new($oldlib)->list() == 0) {
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
			if ($file->IsFile()) {
				if ($has->{$file->fullname()}) {
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
			OpenBSD::Delete::validate_plist($dummy, $state);
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
