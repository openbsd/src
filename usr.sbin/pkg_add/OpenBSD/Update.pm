# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.14 2004/11/09 09:32:17 espie Exp $
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
	my ($self, $state) = @_;

	if (!$self->updatable()) {
		$state->{okay} = 0;
	}
}

sub validate_depend
{
}

sub updatable() { 1 }

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
		return;
	}
	my ($fh, $tempname) = tempfile(DIR => dirname($file->{destdir}.$file->{name}));

	print "extracting $tempname\n" if $state->{very_verbose};
	$file->{name} = $tempname;
	$file->create();
	$self->{tempname} = $tempname;
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
sub updatable() { 0 }

package OpenBSD::PackingElement::ExeclikeAction;
sub updatable() { 0 }

package OpenBSD::PackingElement::LibDepend;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$state->{okay} = 0;
		return;
	}
}

sub mark_lib
{
	my ($self, $libs) = @_;
	my $libname = $self->fullname();
	$libs->{"$libname"} = 1;
}

sub unmark_lib
{
	my ($self, $libs) = @_;
	my $libname = $self->fullname();
	delete $libs->{"$libname"};
}

package OpenBSD::PackingElement::NewDepend;
sub validate_depend
{
	my ($self, $state, $wanting, $toreplace, $replacement) = @_;

	if (defined $self->{name}) {
		return unless $self->{name} eq $wanting;
	}
	return unless OpenBSD::PkgSpec::match($self->{pattern}, $toreplace);
	if (!OpenBSD::PkgSpec::match($self->{pattern}, $replacement)) {
		$state->{okay} = 0;
	}
}

package OpenBSD::Update;
use OpenBSD::RequiredBy;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;

sub can_do
{
	my ($toreplace, $replacement, $state) = @_;

	my $wantlist = [];
	my $r = OpenBSD::RequiredBy->new($toreplace);
	$state->{okay} = 1;
	$state->{libs_to_check} = [];
	my $plist = OpenBSD::PackingList->fromfile(installed_info($toreplace).CONTENTS);
	$plist->visit('can_update', $state);
	if (-f $$r) {
		$wantlist = $r->list();
		my $done_wanted = {};
		for my $wanting (@$wantlist) {
			next if defined $done_wanted->{$wanting};
			$done_wanted->{$wanting} = 1;
			print "Verifying dependencies still match for $wanting\n";
			my $p2 = OpenBSD::PackingList->fromfile(installed_info($wanting).CONTENTS,
			    \&OpenBSD::PackingList::DependOnly);
			$p2->visit('validate_depend', $state, $wanting, $toreplace, $replacement);
		}
	}

	eval {
		OpenBSD::Delete::validate_plist($plist, $state->{destdir});
	};
	if ($@) {
		return 0;
	}

	$plist->{wantlist} = $wantlist;
	$plist->{libs_to_check} = $state->{libs_to_check};
	
	return $state->{okay} ? $plist : 0;
}

# create a packing-list with only the libraries we want to keep around.
sub split_libs
{
	my ($plist, $to_split) = @_;

	my $items = [];

	my $splitted = OpenBSD::PackingList->new();
	OpenBSD::PackingElement::Name->add($splitted, "_libs-".$plist->pkgname());
	# we conflict with the package we just removed...
	OpenBSD::PackingElement::Conflict->add($splitted, $plist->pkgname());

	for my $item (@{$plist->{items}}) {
		if ($item->isa("OpenBSD::PackingElement::Lib") &&
		    defined $to_split->{$item->fullname()}) {
		    	OpenBSD::PackingElement::Lib->add($splitted, $item->clone());
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
	my ($start, $name) = @_;
	my @todo = ($start);
	my $done = {};

	my $write = OpenBSD::RequiredBy->new($name);

	while (my $pkg = shift @todo) {
		$done->{$pkg} = 1;
		my $r = OpenBSD::RequiredBy->new($pkg);
		if (-f $$r) {
			my $list = $r->list();
			for my $pkg2 (@$list) {
				next if $done->{$pkg2};
				push(@todo, $pkg2);
				$write->add($pkg2);
				my $contents = installed_info($pkg2).CONTENTS;
				my $plist = OpenBSD::PackingList->fromfile($contents);
				OpenBSD::PackingElement::PkgDep->add($plist, $name);
				$plist->tofile($contents);
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

	$old_plist->visit('mark_lib', $libs);
	$new_plist->visit('unmark_lib', $libs);

	if (%$libs) {
		my $stub_list = split_libs($old_plist, $libs);
		my $stub_name = $stub_list->pkgname();
		my $dest = installed_info($stub_name);
		mkdir($dest);
		my $oldname = $old_plist->pkgname();
		open my $comment, '>', $dest.COMMENT;
		print $comment "Stub libraries for $oldname";
		close $comment;
		link($dest.COMMENT, $dest.DESC);
		$stub_list->tofile($dest.CONTENTS);
		$old_plist->tofile(installed_info($oldname).CONTENTS);

		walk_depends_closure($old_plist->pkgname(), $stub_name);
	}
}

			
sub adjust_dependency
{
	my ($dep, $from, $into) = @_;

	my $contents = installed_info($dep).CONTENTS;
	my $plist = OpenBSD::PackingList->fromfile($contents);
	my $items = [];
	for my $item (@{$plist->{pkgdep}}) {
		next if $item->{'name'} eq $from;
		push(@$items, $item);
	}
	$plist->{pkgdep} = $items;
	OpenBSD::PackingElement::PkgDep->add($plist, $into);
	$plist->tofile($contents);
}
1;
