#! /usr/bin/perl

# ex:ts=8 sw=4:
# $OpenBSD: PkgAdd.pm,v 1.2 2010/06/09 07:26:01 espie Exp $
#
# Copyright (c) 2003-2010 Marc Espie <espie@openbsd.org>
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

use OpenBSD::AddDelete;

package OpenBSD::PackingList;

sub uses_old_libs
{
	my $plist = shift;
	require OpenBSD::RequiredBy;

	return  grep {/^\.libs\d*\-/o}
	    OpenBSD::Requiring->new($plist->pkgname)->list;
}

sub has_different_sig
{
	my ($plist, $state) = @_;
	if (!defined $plist->{different_sig}) {
		my $n = OpenBSD::PackingList->from_installation($plist->pkgname)->signature;
		my $o = $plist->signature;
		my $r = $n->compare($o);
		$state->print("Comparing full signature for #1 \"#2\" vs. \"#3\":",
		    $plist->pkgname, $o->string, $n->string)
			if $state->verbose >= 3;
		if (defined $r) {
			if ($r == 0) {
				$plist->{different_sig} = 0;
				$state->say("equal") if $state->verbose >= 3;
			} elsif ($r > 0) {
				$plist->{different_sig} = 1;
				$state->say("greater") if $state->verbose >= 3;
			} else {
				$plist->{different_sig} = 1;
				$state->say("less") if $state->verbose >= 3;
			}
		} else {
			$plist->{different_sig} = 1;
			$state->say("non comparable") if $state->verbose >= 3;
		}
	}
	return $plist->{different_sig};
}

package OpenBSD::PkgAdd::State;
our @ISA = qw(OpenBSD::AddDelete::State);

# one-level dependencies tree, for nicer printouts
sub build_deptree
{
	my ($state, $set, @deps) = @_;

	if (defined $state->{deptree}->{$set}) {
		$set = $state->{deptree}->{$set};
	}
	for my $dep (@deps) {
		$state->{deptree}->{$dep} = $set unless
		    defined $state->{deptree}->{$dep};
	}
}

sub todo
{
	my $state = shift;
	return $state->tracker->sets_todo;
}

sub deptree_header
{
	my ($state, $pkg) = @_;
	if (defined $state->{deptree}->{$pkg}) {
		my $s = $state->{deptree}->{$pkg}->real_set;
		if ($s eq $pkg) {
			delete $state->{deptree}->{$pkg};
		} else {
			return $s->short_print.':';
		}
	}
	return '';
}

sub set_name_from_handle
{
	my ($state, $h, $extra) = @_;
	$extra //= '';
	$state->log->set_context($extra.$h->pkgname);
}

OpenBSD::Auto::cache(updater,
    sub {
	require OpenBSD::Update;
	return OpenBSD::Update->new;
    });

OpenBSD::Auto::cache(tracker,
    sub {
	require OpenBSD::Tracker;
	return OpenBSD::Tracker->new;
    });

sub quirks
{
	my $state = shift;

	return $state->{quirks};
}

package OpenBSD::ConflictCache;
our @ISA = (qw(OpenBSD::Cloner));
sub new
{
	my $class = shift;
	bless {done => {}, c => {}}, $class;
}

sub add
{
	my ($self, $handle, $state) = @_;
	return if $self->{done}{$handle};
	$self->{done}{$handle} = 1;
	for my $conflict (OpenBSD::PkgCfl::find_all($handle->plist, $state)) {
		$self->{c}{$conflict} = 1;
	}
}

sub list
{
	my $self = shift;
	return keys %{$self->{c}};
}

sub merge
{
	my ($self, @extra) = @_;
	$self->clone('c', @extra);
	$self->clone('done', @extra);
}

package OpenBSD::UpdateSet;
use OpenBSD::PackageInfo;
use OpenBSD::Error;
use OpenBSD::Handle;

OpenBSD::Auto::cache(solver,
    sub {
	return OpenBSD::Dependencies::Solver->new(shift);
    });

OpenBSD::Auto::cache(conflict_cache,
    sub {
	return OpenBSD::ConflictCache->new;
    });

sub setup_header
{
	my ($set, $state, $handle, $info) = @_;

	my $header = $state->deptree_header($set);
	if (defined $handle) {
		$header .= $handle->pkgname;
	} else {
		$header .= $set->print;
	}
	if (defined $info) {
		$header.=" ($info)";
	}

	if (!$state->progress->set_header($header)) {
		return unless $state->verbose;
		if (!defined $info) {
			$header = "Adding $header";
		}
		if (defined $state->{lastheader} &&
		    $header eq $state->{lastheader}) {
			return;
		}
		$state->{lastheader} = $header;
		$state->print("#1", $header);
		$state->print("(pretending) ") if $state->{not};
		if ($state->{do_faked}) {
			$state->print(" under #1", $state->{destdir});
		}
		$state->print("\n");
	}
}

sub complete
{
	my ($set, $state) = @_;

	for my $n ($set->newer) {
		$n->complete($state);
		my $pkgname = $n->pkgname;
		my $plist = $n->plist;
		return 1 if !defined $plist;
		if (is_installed($pkgname) &&
		    (!$state->{allow_replacing} ||
		      !$state->defines('installed') &&
		      !$plist->has_different_sig($state) &&
		      !$plist->uses_old_libs)) {
		      	my $o = $set->{older}->{$pkgname};
			if (!defined $o) {
				$o = OpenBSD::Handle->create_old($pkgname, $state);
				$set->add_older($o);
			}
			$o->{update_found} = $o;
			$set->move_kept($o);
			$o->{tweaked} =
			    OpenBSD::Add::tweak_package_status($pkgname, $state);
			$state->updater->progress_message($state, "No change in $pkgname");
			delete $set->{newer}->{$pkgname};
			$n->cleanup;
		}
		return 1 if $n->has_error;
	}
	for my $o ($set->older) {
		$o->complete_old($state);
	}

	my $check = $set->install_issues($state);
	return 0 if !defined $check;

	if ($check) {
		$state->{bad}++;
		$set->cleanup(OpenBSD::Handle::CANT_INSTALL, $check);
		$state->tracker->cant($set);
	}
	return 1;
}

sub find_conflicts
{
	my ($set, $state) = @_;

	my $c = $set->conflict_cache;

	for my $handle ($set->newer) {
		$c->add($handle, $state);
	}
	return $c->list;
}

sub mark_as_manual_install
{
	my $set = shift;

	for my $handle ($set->newer) {
		my $plist = $handle->plist;
		$plist->has('manual-installation') or
		    OpenBSD::PackingElement::ManualInstallation->add($plist);
	}
}

sub updates
{
	my ($n, $plist) = @_;
	if (!$n->location->update_info->match_pkgpath($plist)) {
		return 0;
	}
	if (!$n->plist->conflict_list->conflicts_with($plist->pkgname)) {
		return 0;
	}
	my $r = OpenBSD::PackageName->from_string($n->pkgname)->compare(
	    OpenBSD::PackageName->from_string($plist->pkgname));
	if (defined $r && $r < 0) {
		return 0;
	}
	return 1;
}

sub is_an_update_from
{
	my ($set, @conflicts) = @_;
LOOP:	for my $c (@conflicts) {
		next if $c =~ m/^\.libs\d*\-/;
		next if $c =~ m/^partial\-/;
		my $plist = OpenBSD::PackingList->from_installation($c, \&OpenBSD::PackingList::UpdateInfoOnly);
		return 0 unless defined $plist;
		for my $n ($set->newer) {
			if (updates($n, $plist)) {
				next LOOP;
			}
		}
	    	return 0;
	}
	return 1;
}

sub install_issues
{
	my ($set, $state) = @_;

	my @conflicts = $set->find_conflicts($state);

	if (@conflicts == 0) {
		if ($state->defines('update_only')) {
			return "only update, no install";
		} else {
			return 0;
		}
	}

	if (!$state->{allow_replacing}) {
		if (grep { !/^.libs\d*\-/ && !/^partial\-/ } @conflicts) {
			if (!$set->is_an_update_from(@conflicts)) {
				$state->errsay("Can't install #1 because of conflicts (#2)",
				    $set->print, join(',', @conflicts));
				return "conflicts";
			}
		}
	}

	my $later = 0;
	for my $toreplace (@conflicts) {
		if ($state->tracker->is_installed($toreplace)) {
			$state->errsay("Cannot replace #1 in #2: just got installed",
			    $toreplace, $set->print);
			return "replacing just installed";
		}

		next if defined $set->{older}->{$toreplace};
		next if defined $set->{kept}->{$toreplace};

		$later = 1;
		my $s = $state->tracker->is_to_update($toreplace);
		if (defined $s && $s ne $set) {
			$set->merge($state->tracker, $s);
		} else {
			$set->add_older(OpenBSD::Handle->create_old($toreplace,
			    $state));
		}
	}

	return if $later;


	my $manual_install = 0;

	for my $old ($set->older) {
		my $name = $old->pkgname;

		if ($old->has_error(OpenBSD::Handle::NOT_FOUND)) {
			$state->fatal("can't find #1 in installation", $name);
		}
		if ($old->has_error(OpenBSD::Handle::BAD_PACKAGE)) {
			$state->fatal("couldn't find packing-list for #1", $name);
		}

		if ($old->plist->has('manual-installation')) {
			$manual_install = 1;
		}
	}

	$set->mark_as_manual_install if $manual_install;

	return 0;
}

sub try_merging
{
	my ($set, $m, $state) = @_;

	my $s = $state->tracker->is_to_update($m);
	if (!defined $s) {
		$s = OpenBSD::UpdateSet->new->add_older(
		    OpenBSD::Handle->create_old($m, $state));
	}
	if ($state->updater->process_set($s, $state)) {
		$state->say("Merging #1#2", $s->print, $state->ntogo);
		$set->merge($state->tracker, $s);
		return 1;
	} else {
		$state->errsay("NOT MERGING: can't find update for #1#2", 
		    $s->print, $state->ntogo);
		return 0;
	}
}

sub check_forward_dependencies
{
	my ($set, $state) = @_;

	require OpenBSD::ForwardDependencies;
	$set->{forward} = OpenBSD::ForwardDependencies->find($set);
	my $bad = $set->{forward}->check($state);

	if (%$bad) {
		my $no_merge = 1;
		if (!$state->defines('dontmerge')) {
			my $okay = 1;
			for my $m (keys %$bad) {
				if ($set->try_merging($m, $state)) {
					$no_merge = 0;
				} else {
					$okay = 0;
				}
			}
			return 0 if $okay == 1;
		}
		if ($state->defines('updatedepends')) {
			$state->errsay("Forcing update");
			return $no_merge;
		} elsif ($state->{interactive}) {
			if ($state->confirm("Proceed with update anyways", 0)) {
				return $no_merge;
			} else {
				return undef;
			}
		} else {
			return undef;
		}
	}
	return 1;
}

sub recheck_conflicts
{
	my ($set, $state) = @_;

	# no conflicts between newer sets nor kept sets
	for my $h ($set->newer, $set->kept) {
		for my $h2 ($set->newer, $set->kept) {
			next if $h2 == $h;
			if ($h->plist->conflict_list->conflicts_with($h2->pkgname)) {
				$state->errsay("#1: internal conflict between #2 and #3", 
				    $set->print, $h->pkgname, $h2->pkgname);
				return 0;
			}
		}
	}

	return 1;
}

package OpenBSD::PkgAdd;
our @ISA = qw(OpenBSD::AddDelete);

use OpenBSD::Dependencies;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;
use OpenBSD::PackageLocator;
use OpenBSD::PackageName;
use OpenBSD::PkgCfl;
use OpenBSD::Add;
use OpenBSD::SharedLibs;
use OpenBSD::UpdateSet;
use OpenBSD::Error;

sub failed_message
{
	my ($base_msg, $interrupted, @l) = @_;
	my $msg = $base_msg;
	if ($interrupted) {
		$msg = "Caught SIG$interrupted. $msg";
	}
	if (@l > 0) {
		$msg.= ", partial installation recorded as ".join(',', @l);
	}
	return $msg;
}

sub save_partial_set
{
	my ($set, $state) = @_;

	return () if $state->{not};
	my @l = ();
	for my $h ($set->newer) {
		next unless defined $h->{partial};
		push(@l, OpenBSD::Add::record_partial_installation($h->plist, $state, $h->{partial}));
	}
	return @l;
}

sub partial_install
{
	my ($base_msg, $set, $state) = @_;
	return failed_message($base_msg, $state->{interrupted}, save_partial_set($set, $state));
}

sub build_before
{
	my %known = map {($_->pkgname, 1)} @_;
	require OpenBSD::RequiredBy;
	for my $c (@_) {
		for my $d (OpenBSD::RequiredBy->new($c->pkgname)->list) {
			push(@{$c->{before}}, $d) if $known{$d};
		}
	}
}

sub okay
{
	my ($h, $c) = @_;

	for my $d (@{$c->{before}}) {
		return 0 if !$h->{$d};
	}
	return 1;
}

sub iterate
{
	my $sub = pop @_;
	my $done = {};
	my $something_done;

	do {
		$something_done = 0;

		for my $c (@_) {
			next if $done->{$c->pkgname};
			if (okay($done, $c)) {
				&$sub($c);
				$done->{$c->pkgname} = 1;
				$something_done = 1;
			}
		}
	} while ($something_done);
	# if we can't do stuff in order, do it anyways
	for my $c (@_) {
		next if $done->{$c->pkgname};
		&$sub($c);
	}
}

sub check_x509_signature
{
	my ($set, $state) = @_;
	for my $handle ($set->newer) {
		$state->set_name_from_handle($handle, '+');
		my $plist = $handle->plist;
		if ($plist->is_signed) {
			if ($state->defines('nosig')) {
				$state->errsay("NOT CHECKING DIGITAL SIGNATURE FOR #1",
				    $plist->pkgname);
				$state->{check_digest} = 0;
			} else {
				require OpenBSD::x509;

				if (!OpenBSD::x509::check_signature($plist,
				    $state)) {
					$state->fatal("#1 is corrupted", 
					    $set->print);
				}
				$state->{check_digest} = 1;
				$state->{packages_with_sig}++;
			}
		} else {
			$state->{packages_without_sig}{$plist->pkgname} = 1;
			$state->{check_digest} = 0;
		}
	}
}

sub delete_old_packages
{
	my ($set, $state) = @_;

	build_before($set->older_to_do);
	iterate($set->older_to_do, sub {
		return if $state->{size_only};
		my $o = shift;
		$set->setup_header($state, $o, "deleting");
		my $oldname = $o->pkgname;
		$state->set_name_from_handle($o, '-');
		require OpenBSD::Delete;
		try {
			OpenBSD::Delete::delete_plist($o->plist, $state);
		} catchall {
			$state->errprint($_);
			$state->fatal(partial_install(
			    "Deinstallation of $oldname failed",
			    $set, $state));
		};

		if (defined $state->{updatedepends}) {
			delete $state->{updatedepends}->{$oldname};
		}
		OpenBSD::PkgCfl::unregister($o->plist, $state);
	});
	# Here there should be code to handle old libs
}

sub really_add
{
	my ($set, $state) = @_;

	my $errors = 0;

	check_x509_signature($set, $state);

	if ($state->{not}) {
		$state->status->what("Pretending to add");
	} else {
		$state->status->what("Adding");
	}
	$set->setup_header($state);

	# XXX in `combined' updates, some dependencies may remove extra
	# packages, so we do a double-take on the list of packages we
	# are actually replacing.
	my $replacing = 0;
	if ($set->older_to_do) {
		$replacing = 1;
	}
#	if (defined $plist->{old_libs}) {
#		$replacing = 1;
#	}
	$state->{replacing} = $replacing;

	$ENV{'PKG_PREFIX'} = $state->{localbase};

	my $handler = sub {
		$state->{interrupted} = shift;
	};
	local $SIG{'INT'} = $handler;
	local $SIG{'QUIT'} = $handler;
	local $SIG{'HUP'} = $handler;
	local $SIG{'KILL'} = $handler;
	local $SIG{'TERM'} = $handler;

	if ($replacing) {
		require OpenBSD::OldLibs;
		OpenBSD::OldLibs->save($set, $state);
	}

	if ($replacing && !$state->{delete_first}) {
		$state->{extracted_first} = 1;
		for my $handle ($set->newer) {
			next if $state->{size_only};
			$set->setup_header($state, $handle, "extracting");

			try {
				OpenBSD::Replace::perform_extraction($handle,
				    $state);
			} catchall {
				unless ($state->{interrupted}) {
					$state->errprint($_);
					$errors++;
				}
			};
			if ($state->{interrupted} || $errors) {
				$state->fatal(partial_install("Installation of ".
				    $handle->pkgname." failed", $set, $state));
			}
		}
	} else {
		$state->{extracted_first} = 0;
	}

	if ($replacing) {
		delete_old_packages($set, $state);
	}

	iterate($set->newer, sub {
		return if $state->{size_only};
		my $handle = shift;

		my $pkgname = $handle->pkgname;
		my $plist = $handle->plist;
		$set->setup_header($state, $handle,
		    $replacing ? "installing" : undef);
		$state->set_name_from_handle($handle, '+');

		try {
			OpenBSD::Add::perform_installation($handle, $state);
			if (!$state->{interrupted} && $plist->has(INSTALL)) {
				$plist->get(INSTALL)->run($state, 'POST-INSTALL');
			}
		} catchall {
			unless ($state->{interrupted}) {
				$state->errprint($_);
				$errors++;
			}
		};

		unlink($plist->infodir.CONTENTS);
		if ($state->{interrupted} || $errors) {
			$state->fatal(partial_install("Installation of $pkgname failed",
			    $set, $state));
		}
	});
	$set->setup_header($state);
	$state->progress->next($state->ntogo(-1));
	for my $handle ($set->newer) {
		my $pkgname = $handle->pkgname;
		my $plist = $handle->plist;
		OpenBSD::SharedLibs::add_libs_from_plist($plist);
		OpenBSD::Add::tweak_plist_status($plist, $state);
		$plist->to_cache;
		OpenBSD::Add::register_installation($plist);
		add_installed($pkgname);
		delete $handle->{partial};
		OpenBSD::PkgCfl::register($plist, $state);
		if ($plist->has(DISPLAY)) {
			$plist->get(DISPLAY)->prepare($state);
		}
	}
	for my $handle ($set->newer) {
		$set->{solver}->register_dependencies($state);
	}
	if ($replacing) {
		$set->{forward}->adjust($state);
	}
	if ($state->{repairdependencies}) {
		$set->{solver}->repair_dependencies($state);
	}
	delete $state->{delete_first};
}

sub newer_has_errors
{
	my ($set, $state) = @_;

	for my $handle ($set->newer) {
		if ($handle->has_error(OpenBSD::Handle::ALREADY_INSTALLED)) {
			$set->cleanup(OpenBSD::Handle::ALREADY_INSTALLED);
			return 1;
		}
		if ($handle->has_error) {
			$state->set_name_from_handle($handle);
			$state->log("Can't install #1: #2", 
			    $handle->pkgname, $handle->error_message);
			$state->{bad}++;
			$set->cleanup($handle->has_error);
			$state->tracker->cant($set);
			return 1;
		}

		if ($handle->plist->has('arch')) {
			unless ($handle->plist->{arch}->check($state->{arch})) {
				$state->set_name_from_handle($handle);
				$state->log("#1 is not for the right architecture",
				    $handle->pkgname);
				if (!$state->defines('arch')) {
					$state->{bad}++;
					$set->cleanup(OpenBSD::Handle::CANT_INSTALL);
					$state->tracker->cant($set);
					return 1;
				}
			}
		}
	}
	return 0;
}

sub install_set
{
	my ($set, $state) = @_;

	$set = $set->real_set;

	if ($set->{finished}) {
		return ();
	}

	if (!$state->updater->process_set($set, $state)) {
		return ();
	}

	for my $handle ($set->newer) {
		if ($state->tracker->is_installed($handle->pkgname)) {
			$set->move_kept($handle);
		}
	}

	if (!$set->complete($state)) {
		return $set;
	}

	if (newer_has_errors($set, $state)) {
		return ();
	}

	my @deps = $set->solver->solve_depends($state);
	if ($state->verbose >= 2) {
		$set->solver->dump;
	}
	if (@deps > 0) {
		$state->build_deptree($set, @deps);
		$set->solver->check_for_loops($state);
		return (@deps, $set);
	}

	if ($set->older_to_do) {
		my $r = $set->check_forward_dependencies($state);
		if (!defined $r) {
			$state->{bad}++;
			$set->cleanup(OpenBSD::Handle::CANT_INSTALL);
			$state->tracker->cant($set);
			return ();
		}
		if ($r == 0) {
			return $set;
		}
	}

	# verify dependencies have been installed
	my @baddeps = $set->solver->check_depends;

	if (@baddeps) {
		$state->errsay("Can't install #1: can't resolve #2", 
		    $set->print, join(',', @baddeps));
		$state->{bad}++;
		$set->cleanup(OpenBSD::Handle::CANT_INSTALL,"bad dependencies");
		$state->tracker->cant($set);
		return ();
	}

	if (!$set->solver->solve_wantlibs($state)) {
		$state->{bad}++;
		$set->cleanup(OpenBSD::Handle::CANT_INSTALL, "libs not found");
		$state->tracker->cant($set);
		return ();
	}
#	if (!$set->solver->solve_tags($state)) {
#		if (!$state->defines('libdepends')) {
#			$state->{bad}++;
#			return ();
#		}
#	}
	if (!$set->recheck_conflicts($state)) {
		$state->{bad}++;
		$set->cleanup(OpenBSD::Handle::CANT_INSTALL, "fatal conflicts");
		$state->tracker->cant($set);
		return ();
	}
	if ($set->older_to_do) {
		require OpenBSD::Replace;
		if (!OpenBSD::Replace::is_set_safe($set, $state)) {
			$state->{bad}++;
			$set->cleanup(OpenBSD::Handle::CANT_INSTALL, "exec detected");
			$state->tracker->cant($set);
			return ();
		}
	}
	if ($set->newer > 0 || $set->older_to_do > 0) {
		for my $h ($set->newer) {
			$h->plist->set_infodir($h->location->info);
		}

		if (!$set->validate_plists($state)) {
			$state->{bad}++;
			$set->cleanup(OpenBSD::Handle::CANT_INSTALL,
			    "file issues");
			$state->tracker->cant($set);
			return ();
		}

		really_add($set, $state);
	}
	$set->cleanup;
	$state->tracker->done($set);
	return ();
}

sub inform_user_of_problems
{
	my $state = shift;
	my @cantupdate = $state->tracker->cant_list;
	if (@cantupdate > 0) {
		$state->say("Couldn't find updates for #1", join(', ', @cantupdate));
	}
	if (defined $state->{issues}) {
		$state->say("There were some ambiguities. ".
		    "Please run in interactive mode again.");
	}
}

# if we already have quirks, we update it. If not, we try to install it.
sub quirk_set
{
	require OpenBSD::PackageRepository::Installed;
	require OpenBSD::Search;

	my $set = OpenBSD::UpdateSet->new;
	$set->{quirks} = 1;
	my $l = OpenBSD::PackageRepository::Installed->new->match_locations(OpenBSD::Search::Stem->new('quirks'));
	if (@$l > 0) {
		$set->add_older(map {OpenBSD::Handle->from_location($_)} @$l);
	} else {
		$set->add_hints2('quirks');
	}
	return $set;
}

sub do_quirks
{
	my $state = shift;

	install_set(quirk_set(), $state);
	eval {
		require OpenBSD::Quirks;
		# interface version number.
		$state->{quirks} = OpenBSD::Quirks->new(1);
	};
}

# Here we create the list of packages to install
# actually, an updateset list (@todo2), and we hope to do this lazily
# later for the most part...
my @todo2 = ();


sub process_parameters
{
	my ($self, $state) = @_;
	my $add_hints = $state->opt('z') ? "add_hints" : "add_hints2";

	# match fuzzily against a list
	if ($state->opt('l')) {
		open my $f, '<', $state->opt('l') or 
		    die "$!: bad list ".$state->opt('l');
		my $_;
		while (<$f>) {
			chomp;
			s/\s.*//;
			push(@todo2, OpenBSD::UpdateSet->new->$add_hints($_));
		}
	}

	# update existing stuff
	if ($state->opt('u')) {
		require OpenBSD::PackageRepository::Installed;

		if (@ARGV == 0) {
			@ARGV = sort(installed_packages());
			$state->{allupdates} = 1;
		}
		my $inst = OpenBSD::PackageRepository::Installed->new;
		for my $pkgname (@ARGV) {
			my $l;

			next if $pkgname =~ m/^quirks\-\d/;
			if (OpenBSD::PackageName::is_stem($pkgname)) {
				$l = $state->updater->stem2location($inst, $pkgname, $state);
			} else {
				$l = $inst->find($pkgname, $state->{arch});
			}
			if (!defined $l) {
				$state->say("Problem finding #1", $pkgname);
			} else {
				push(@todo2, OpenBSD::UpdateSet->new->add_older(OpenBSD::Handle->from_location($l)));
			}
		}
	} else {

	# actual names
		for my $pkgname (@ARGV) {
			next if $pkgname =~ m/^quirks\-\d/;
			push(@todo2,
			    OpenBSD::UpdateSet->new->$add_hints($pkgname));
		}
	}
}

sub finish_display
{
	my ($self, $state) = @_;
	OpenBSD::Add::manpages_index($state);


	# and display delayed thingies.
	if ($state->{packages_with_sig}) {
		$state->print("Packages with signatures: #1",
		    $state->{packages_with_sig});
		if ($state->{packages_without_sig}) {
			print ". UNSIGNED PACKAGES: ",
			    join(', ', keys %{$state->{packages_without_sig}});
		}
		print "\n";
	}
	if (defined $state->{updatedepends} && %{$state->{updatedepends}}) {
		print "Forced updates, bogus dependencies for ",
		    join(' ', sort(keys %{$state->{updatedepends}})),
		    " may remain\n";
	}
	inform_user_of_problems($state);
}

sub handle_options
{
	my ($self, $cmd) = @_;
	my $state = $self->SUPER::handle_options('aruUzl:A:P:Q:', {}, $cmd,
	    '[-acIinqrsUuvxz] [-A arch] [-B pkg-destdir] [-D name[=value]]',
	    '[-L localbase] [-l file] [-P type] [-Q quick-destdir] pkg-name [...]');

	$state->{do_faked} = 0;
	$state->{arch} = $state->opt('A');

	if (defined $state->opt('Q') and defined $state->opt('B')) {
		$state->usage("-Q and -B are incompatible options");
	}
	if (defined $state->opt('Q') and defined $state->opt('r')) {
		$state->usage("-r and -Q are incompatible options");
	}
	if ($state->opt('P')) {
		if ($state->opt('P') eq 'cdrom') {
			$state->{cdrom_only} = 1;
		}
		elsif ($state->opt('P') eq 'ftp') {
			$state->{ftp_only} = 1;
		}
		else {
		    $state->usage("bad option: -P #1", $state->opt('P'));
		}
	}
	if (defined $state->opt('Q')) {
		$state->{destdir} = $state->opt('Q');
		$state->{do_faked} = 1;
	} elsif (defined $state->opt('B')) {
		$state->{destdir} = $state->opt('B');
	} elsif (defined $ENV{'PKG_PREFIX'}) {
		$state->{destdir} = $ENV{'PKG_PREFIX'};
	}
	if (defined $state->{destdir}) {
		$state->{destdir}.='/';
		$ENV{'PKG_DESTDIR'} = $state->{destdir};
	} else {
		$state->{destdir} = '';
		delete $ENV{'PKG_DESTDIR'};
	}


	$state->{automatic} = $state->opt('a');
	$state->{hard_replace} = $state->opt('r');
	$state->{newupdates} = $state->opt('u') || $state->opt('U');
	$state->{allow_replacing} = $state->{hard_replace} || 
	    $state->{newupdates};

	if (@ARGV == 0 && !$state->opt('u') && !$state->opt('l')) {
		$state->usage("Missing pkgname");
	}
	return $state;
}

sub main
{
	my ($self, $state) = @_;
	if ($state->{allow_replacing}) {
		$state->progress->set_header("Checking packages");
		do_quirks($state);
	}

	$state->tracker->todo(@todo2);
	# This is the actual very small loop that adds all packages
	while (my $set = shift @todo2) {
		$state->progress->set_header("Checking packages");

		$state->status->what->set($set);
		unshift(@todo2, install_set($set, $state));
		eval {
			$state->quirks->tweak_list(\@todo2, $state);
		};
	}
}


sub new_state
{
	my ($self, $cmd) = @_;
	return OpenBSD::PkgAdd::State->new($cmd);
}

1;
