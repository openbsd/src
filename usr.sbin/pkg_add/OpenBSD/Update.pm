# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.25 2004/11/11 20:59:05 espie Exp $
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

sub extract_with_pm
{
	require OpenBSD::ProgressMeter;

	my ($self, $state) = @_;

	$self->extract($state);
	if (defined $self->{size}) {
		$state->{donesize} += $self->{size};
		OpenBSD::ProgressMeter::show($state->{donesize}, $state->{totsize});
	}
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
	
	if ($state->{not}) {
		print "extracting tempfile under ", dirname($file->{destdir}.$file->{name}), "\n";
	} else {
		my ($fh, $tempname) = tempfile(DIR => dirname($file->{destdir}.$file->{name}));

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

package OpenBSD::Update;
use OpenBSD::RequiredBy;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;
use OpenBSD::Error;

sub can_do
{
	my ($toreplace, $replacement, $state) = @_;

	my $wantlist = [];
	my $r = OpenBSD::RequiredBy->new($toreplace);
	$state->{okay} = 1;
	$state->{libs_to_check} = [];
	my $plist = OpenBSD::PackingList->from_installation($toreplace);
	$plist->visit('can_update', 0, $state);
	if ($state->{okay} == 0) {
		Warn "Old package contains impossible to update elements\n";
	}
	if ($state->{forced}->{update}) {
		$state->{okay} = 1;
	}
	if (-f $$r) {
		$wantlist = $r->list();
		my $done_wanted = {};
		for my $wanting (@$wantlist) {
			next if defined $done_wanted->{$wanting};
			$done_wanted->{$wanting} = 1;
			print "Verifying dependencies still match for $wanting\n" if $state->{verbose};
			my $p2 = OpenBSD::PackingList->from_installation($wanting,
			    \&OpenBSD::PackingList::DependOnly);
			$p2->visit('validate_depend', $state, $wanting, $toreplace, $replacement);
		}
	}

	if ($state->{forced}->{updatedepends}) {
		$state->{okay} = 1;
	}
	eval {
		OpenBSD::Delete::validate_plist($plist, $state);
	};
	if ($@) {
		Warn "$@";
		return 0;
	}

	$plist->{wantlist} = $wantlist;
	$plist->{libs_to_check} = $state->{libs_to_check};
	
	return $state->{okay} ? $plist : 0;
}

sub is_safe
{
	my ($plist, $state) = @_;
	$state->{okay} = 1;
	$plist->visit('can_update', 1, $state);
	if ($state->{okay} == 0) {
		Warn "New package contains unsafe operations\n";
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
	# we conflict with the package we just removed...
	OpenBSD::PackingElement::Conflict->add($splitted, $plist->pkgname());

	for my $item (@{$plist->{items}}) {
		if ($item->isa("OpenBSD::PackingElement::Lib") &&
		    defined $to_split->{$item->fullname()}) {
		    	$item->clone()->add_object($splitted);
		} elsif ($item->isa("OpenBSD::PackingElement::Cwd")) {
			OpenBSD::PackingElement::Cwd->add($splitted, $item->{name});
		} else {
			push(@$items, $item);
		}
	}
	$plist->{items} = $items;
	return $splitted;
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
		my $r = OpenBSD::RequiredBy->new($pkg);
		if (-f $$r) {
			my $list = $r->list();
			for my $pkg2 (@$list) {
				next if $done->{$pkg2};
				push(@todo, $pkg2);
				print "\t$pkg2\n" if $state->{beverbose};
				$write->add($pkg2) unless $state->{not};
				my $plist = OpenBSD::PackingList->from_installation($pkg2);
				OpenBSD::PackingElement::PkgDep->add($plist, $name);
				$plist->to_installation() unless $state->{not};
				$done->{$pkg2} = 1;
			}
		}
	}
}



sub save_old_libraries
{
	my ($new_plist, $state) = @_;

	my $old_plist = $new_plist->{replacing};
	my $libs = {};
	my $p = {};

	print "Looking for changes in shared libraries\n" 
	    if $state->{beverbose};
	$old_plist->visit('mark_lib', $libs, $p);
	$new_plist->visit('unmark_lib', $libs, $p);

	print "Libraries to keep: ", join(",", sort(keys %$libs)), "\n" 
	    if $state->{beverbose};
	if (%$libs) {
		my $stub_list = split_libs($old_plist, $libs);
		my $stub_name = $stub_list->pkgname();
		my $dest = installed_info($stub_name);
		print "Keeping them in $stub_name\n" if $state->{beverbose};
		unless ($state->{not}) {
			mkdir($dest);
			my $oldname = $old_plist->pkgname();
			open my $comment, '>', $dest.COMMENT;
			print $comment "Stub libraries for $oldname";
			close $comment;
			link($dest.COMMENT, $dest.DESC);
			$stub_list->to_installation();
			$old_plist->to_installation();
		}

		walk_depends_closure($old_plist->pkgname(), $stub_name, $state);
	}
}

			
sub adjust_dependency
{
	my ($dep, $from, $into) = @_;

	my $plist = OpenBSD::PackingList->from_installation($dep);
	my $items = [];
	for my $item (@{$plist->{pkgdep}}) {
		next if $item->{'name'} eq $from;
		push(@$items, $item);
	}
	$plist->{pkgdep} = $items;
	OpenBSD::PackingElement::PkgDep->add($plist, $into);
	$plist->to_installation();
}
1;
