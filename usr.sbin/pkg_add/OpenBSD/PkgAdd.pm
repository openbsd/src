#! /usr/bin/perl

# ex:ts=8 sw=4:
# $OpenBSD: PkgAdd.pm,v 1.98 2018/02/27 22:46:53 espie Exp $
#
# Copyright (c) 2003-2014 Marc Espie <espie@openbsd.org>
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
		my $r = $n->compare($o, $state->defines("SHORTENED"));
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

package OpenBSD::PackingElement;
sub hash_files
{
}
sub tie_files
{
}

package OpenBSD::PackingElement::FileBase;
sub hash_files
{
	my ($self, $sha, $state) = @_;
	return if $self->{link} or $self->{symlink} or $self->{nochecksum};
	if (defined $self->{d}) {
		$sha->{$self->{d}->key} = $self;
	}
}

sub tie_files
{
	my ($self, $sha, $state) = @_;
	return if $self->{link} or $self->{symlink} or $self->{nochecksum};
	# XXX python doesn't like this, overreliance on timestamps
	return if $self->{name} =~ m/\.py$/ && !defined $self->{ts};
	if (defined $sha->{$self->{d}->key}) {
		my $tied = $sha->{$self->{d}->key};
		# don't tie if there's a problem with the file
		my $realname = $tied->realname($state);
		return unless -f $realname;
		# and do a sanity check that this file wasn't altered
		return unless (stat _)[7] == $self->{size};
		if ($state->defines('checksum')) {
			my $d = $self->compute_digest($realname, $self->{d});
			# XXX we don't have to display anything here
			# because delete will take care of that
			return unless $d->equals($self->{d});
		}
		$self->{tieto} = $tied;
		$tied->{tied} = 1;
		$state->say("Tieing #1 to #2", $self->stringize,
		    $tied->stringize) if $state->verbose >= 3;
	}
}

package OpenBSD::PkgAdd::State;
our @ISA = qw(OpenBSD::AddDelete::State);

sub handle_options
{
	my $state = shift;
	$state->SUPER::handle_options('ruUzl:A:P:',
	    '[-acinqrsUuVvxz] [-A arch] [-B pkg-destdir] [-D name[=value]]',
	    '[-L localbase] [-l file] [-P type] pkg-name ...');

	$state->{arch} = $state->opt('A');

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
	if (defined $state->opt('B')) {
		$state->{destdir} = $state->opt('B');
	}
	if (defined $state->{destdir}) {
		$state->{destdir}.='/';
	} else {
		$state->{destdir} = '';
	}


	$state->{hard_replace} = $state->opt('r');
	$state->{newupdates} = $state->opt('u') || $state->opt('U');
	$state->{allow_replacing} = $state->{hard_replace} ||
	    $state->{newupdates};
	$state->{pkglist} = $state->opt('l');
	$state->{update} = $state->opt('u');
	$state->{fuzzy} = $state->opt('z');

	if (@ARGV == 0 && !$state->{update} && !$state->{pkglist}) {
		$state->usage("Missing pkgname");
	}
}

OpenBSD::Auto::cache(cache_directory,
	sub {
		my $self = shift;
		if (defined $ENV{PKG_CACHE}) {
			return $ENV{PKG_CACHE};
		} else {
			return undef;
		}
	});

sub set_name_from_handle
{
	my ($state, $h, $extra) = @_;
	$extra //= '';
	$state->log->set_context($extra.$h->pkgname);
}

sub updateset
{
	my $self = shift;
	require OpenBSD::UpdateSet;

	return OpenBSD::UpdateSet->new($self);
}

sub updateset_with_new
{
	my ($self, $pkgname) = @_;

	return $self->updateset->add_newer(
	    OpenBSD::Handle->create_new($pkgname));
}

sub updateset_from_location
{
	my ($self, $location) = @_;

	return $self->updateset->add_newer(
	    OpenBSD::Handle->from_location($location));
}

sub display_timestamp
{
	my ($state, $pkgname, $timestamp) = @_;
	$state->say("#1 signed on #2", $pkgname, $timestamp);
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
	for my $conflict (OpenBSD::PkgCfl::find_all($handle, $state)) {
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
		$state->print("\n");
	}
}

my $checked = {};

sub check_security
{
	my ($set, $state, $plist, $h) = @_;
	return if $checked->{$plist->fullpkgpath};
	$checked->{$plist->fullpkgpath} = 1;
	return if $set->{quirks};
	my ($error, $bad);
	$state->run_quirks(
		sub {
			my $quirks = shift;
			return unless $quirks->can("check_security");
			$bad = $quirks->check_security($plist->fullpkgpath);
			if (defined $bad) {
				require OpenBSD::PkgSpec;
				my $spec = OpenBSD::PkgSpec->new($bad);
				my $r = $spec->match_locations([$h->{location}]);
				if (@$r != 0) {
					$error++;
				}
			}
		});
	if ($error) {
		$state->errsay("Package #1 found, matching insecure #2", 
		    $h->pkgname, $bad);
	}
}

sub display_timestamp
{
	my ($pkgname, $plist, $state) = @_;

	return unless $plist->is_signed;
	$state->display_timestamp($pkgname,
	    $plist->get('digital-signature')->iso8601);
}

sub find_kept_handle
{
	my ($set, $n,  $state) = @_;
	unless (defined $n->{location} && defined $n->{location}{update_info}) {
		$n->complete($state);
	}
	my $plist = $n->dependency_info;
	return if !defined $plist;
	my $pkgname = $plist->pkgname;
	if ($set->{quirks}) {
		display_timestamp($pkgname, $plist, $state);
	}
	# condition for no update
	unless (is_installed($pkgname) &&
	    (!$state->{allow_replacing} ||
	      !$state->defines('installed') &&
	      !$plist->has_different_sig($state) &&
	      !$plist->uses_old_libs)) {
	      	$set->check_security($state, $plist, $n);
	      	return;
	}
	my $o = $set->{older}{$pkgname};
	if (!defined $o) {
		$o = OpenBSD::Handle->create_old($pkgname, $state);
		if (!defined $o->pkgname) {
			$state->{bad}++;
			$set->cleanup(OpenBSD::Handle::CANT_INSTALL, 
			    "Bogus package already installed");
		    	return;
		}
	}
	$set->check_security($state, $plist, $o);
	$set->move_kept($o);
	$o->{tweaked} =
	    OpenBSD::Add::tweak_package_status($pkgname, $state);
	$state->updater->progress_message($state, "No change in $pkgname");
	delete $set->{newer}{$pkgname};
	$n->cleanup;
}

sub figure_out_kept
{
	my ($set, $state) = @_;

	for my $n ($set->newer) {
		$set->find_kept_handle($n, $state);
	}
}

sub complete
{
	my ($set, $state) = @_;

	for my $n ($set->newer) {
		$n->complete($state);
		my $plist = $n->plist;
		return 1 if !defined $plist;
		return 1 if $n->has_error;
	}
	# XXX kept must have complete plists to be able to track 
	# libs for OldLibs
	for my $o ($set->older, $set->kept) {
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
	if (!$n->conflict_list->conflicts_with($plist->pkgname)) {
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
		if (grep { !/^\.libs\d*\-/ && !/^partial\-/ } @conflicts) {
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

		next if defined $set->{older}{$toreplace};
		next if defined $set->{kept}{$toreplace};

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
			$state->fatal("couldn't find packing-list for #1", 
			    $name);
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
		$s = $state->updateset->add_older(
		    OpenBSD::Handle->create_old($m, $state));
	}
	if ($state->updater->process_set($s, $state)) {
		$state->say("Merging #1 (#2)", $s->print, $state->ntogo);
		$set->merge($state->tracker, $s);
		return 1;
	} else {
		$state->errsay("NOT MERGING: can't find update for #1 (#2)",
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
		} elsif ($state->confirm_defaults_to_no(
		    "Proceed with update anyway")) {
				return $no_merge;
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
			if ($h->conflict_list->conflicts_with($h2->pkgname)) {
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
use OpenBSD::PackageName;
use OpenBSD::PkgCfl;
use OpenBSD::Add;
use OpenBSD::SharedLibs;
use OpenBSD::UpdateSet;
use OpenBSD::Error;

sub failed_message
{
	my ($base_msg, $received, @l) = @_;
	my $msg = $base_msg;
	if ($received) {
		$msg = "Caught SIG$received. $msg";
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
	return failed_message($base_msg, $state->{received}, save_partial_set($set, $state));
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
	# if we can't do stuff in order, do it anyway
	for my $c (@_) {
		next if $done->{$c->pkgname};
		&$sub($c);
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
			$state->errsay($_);
			$state->fatal(partial_install(
			    "Deinstallation of $oldname failed",
			    $set, $state));
		};

		if (defined $state->{updatedepends}) {
			delete $state->{updatedepends}->{$oldname};
		}
		OpenBSD::PkgCfl::unregister($o, $state);
	});
	$set->cleanup_old_shared($state);
	# Here there should be code to handle old libs
}

sub delayed_delete
{
	my $state = shift;
	for my $realname (@{$state->{delayed}}) {
		if (!unlink $realname) {
			$state->errsay("Problem deleting #1: #2", $realname, 
			    $!);
			$state->log("deleting #1 failed: #2", $realname, $!);
		}
	}
	delete $state->{delayed};
}

sub really_add
{
	my ($set, $state) = @_;

	my $errors = 0;

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
	$state->{replacing} = $replacing;

	my $handler = sub {
		$state->{received} = shift;
		$state->errsay("Interrupted");
		if ($state->{hardkill}) {
			delete $state->{hardkill};
			return;
		}
		$state->{interrupted}++;
	};
	local $SIG{'INT'} = $handler;
	local $SIG{'QUIT'} = $handler;
	local $SIG{'HUP'} = $handler;
	local $SIG{'KILL'} = $handler;
	local $SIG{'TERM'} = $handler;

	$state->{hardkill} = $state->{delete_first};

	if ($replacing) {
		require OpenBSD::OldLibs;
		OpenBSD::OldLibs->save($set, $state);
	}

	if ($state->{delete_first}) {
		delete_old_packages($set, $state);
	}

	for my $handle ($set->newer) {
		next if $state->{size_only};
		$set->setup_header($state, $handle, "extracting");

		try {
			OpenBSD::Add::perform_extraction($handle,
			    $state);
		} catchall {
			unless ($state->{interrupted}) {
				$state->errsay($_);
				$errors++;
			}
		};
		if ($state->{interrupted} || $errors) {
			$state->fatal(partial_install("Installation of ".
			    $handle->pkgname." failed", $set, $state));
		}
	}
	if ($state->{delete_first}) {
		delayed_delete($state);
	} else {
		$state->{hardkill} = 1;
		delete_old_packages($set, $state);
	}

	iterate($set->newer, sub {
		return if $state->{size_only};
		my $handle = shift;
		my $pkgname = $handle->pkgname;
		my $plist = $handle->plist;

		$set->setup_header($state, $handle, "installing");
		$state->set_name_from_handle($handle, '+');

		try {
			OpenBSD::Add::perform_installation($handle, $state);
		} catchall {
			unless ($state->{interrupted}) {
				$state->errsay($_);
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
		OpenBSD::SharedLibs::add_libs_from_plist($plist, $state);
		OpenBSD::Add::tweak_plist_status($plist, $state);
		OpenBSD::Add::register_installation($plist, $state);
		add_installed($pkgname);
		delete $handle->{partial};
		OpenBSD::PkgCfl::register($handle, $state);
	}
	delete $state->{partial};
	$set->{solver}->register_dependencies($state);
	if ($replacing) {
		$set->{forward}->adjust($state);
	}
	if ($state->{repairdependencies}) {
		$set->{solver}->repair_dependencies($state);
	}
	delete $state->{delete_first};
	$state->syslog("Added #1", $set->print);
	if ($state->{received}) {
		die "interrupted";
	}
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
			    $handle->pkgname, $handle->error_message)
			    unless $handle->has_reported_error;
			$state->{bad}++;
			$set->cleanup($handle->has_error);
			$state->tracker->cant($set);
			return 1;
		}
	}
	return 0;
}

sub newer_is_bad_arch
{
	my ($set, $state) = @_;

	for my $handle ($set->newer) {
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

sub may_tie_files
{
	my ($set, $state) = @_;
	if ($set->newer > 0 && $set->older_to_do > 0 && !$state->defines('donttie')) {
		my $sha = {};

		for my $o ($set->older_to_do) {
			$o->{plist}->hash_files($sha, $state);
		}
		for my $n ($set->newer) {
			$n->{plist}->tie_files($sha, $state);
		}
	}
}

sub process_set
{
	my ($self, $set, $state) = @_;

	$state->{current_set} = $set;

	if (!$state->updater->process_set($set, $state)) {
		return ();
	}

	for my $handle ($set->newer) {
		if ($state->tracker->is_installed($handle->pkgname)) {
			$set->move_kept($handle);
			$handle->{tweaked} = OpenBSD::Add::tweak_package_status($handle->pkgname, $state);
		}
	}

	$set->figure_out_kept($state);

	if (newer_has_errors($set, $state)) {
		return ();
	}
	if ($set->newer == 0 && $set->older_to_do == 0) {
		$state->tracker->uptodate($set);
		return ();
	}

	my @deps = $set->solver->solve_depends($state);
	if ($state->verbose >= 2) {
		$set->solver->dump($state);
	}
	if (@deps > 0) {
		$state->build_deptree($set, @deps);
		$set->solver->check_for_loops($state);
		return (@deps, $set);
	}

	if (!$set->complete($state)) {
		return $set;
	}

	if (newer_has_errors($set, $state)) {
		return ();
	}

	for my $h ($set->newer) {
		$set->check_security($state, $h->plist, $h);
	}

	if (newer_is_bad_arch($set, $state)) {
		return ();
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
	my $baddeps = $set->solver->check_depends;

	if (@$baddeps) {
		$state->errsay("Can't install #1: can't resolve #2",
		    $set->print, join(',', @$baddeps));
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
	may_tie_files($set, $state);
	if ($set->newer > 0 || $set->older_to_do > 0) {
		for my $h ($set->newer) {
			$h->plist->set_infodir($h->location->info);
			delete $h->location->{contents};
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
		$state->run_quirks(
		    sub {
		    	my $quirks = shift;
			$quirks->filter_obsolete(\@cantupdate, $state);
		    });

		$state->say("Couldn't find updates for #1", 
		    join(' ', sort @cantupdate)) if @cantupdate > 0;
	}
	if (defined $state->{issues}) {
		$state->say("There were some ambiguities. ".
		    "Please run in interactive mode again.");
	}
}

# if we already have quirks, we update it. If not, we try to install it.
sub quirk_set
{
	my $state = shift;
	require OpenBSD::Search;

	my $set = $state->updateset;
	$set->{quirks} = 1;
	my $l = $state->repo->installed->match_locations(OpenBSD::Search::Stem->new('quirks'));
	if (@$l > 0) {
		$set->add_older(map {OpenBSD::Handle->from_location($_)} @$l);
	} else {
		$set->add_hints2('quirks');
	}
	return $set;
}

sub do_quirks
{
	my ($self, $state) = @_;
	my $set = quirk_set($state);
	$self->process_set($set, $state);
}


sub process_parameters
{
	my ($self, $state) = @_;
	my $add_hints = $state->{fuzzy} ? "add_hints" : "add_hints2";

	# match against a list
	if ($state->{pkglist}) {
		open my $f, '<', $state->{pkglist} or
		    $state->fatal("bad list #1: #2", $state->{pkglist}, $!);
		while (<$f>) {
			chomp;
			s/\s.*//;
			s/\.tgz$//;
			push(@{$state->{setlist}},
			    $state->updateset->$add_hints($_));
		}
	}

	# update existing stuff
	if ($state->{update}) {

		if (@ARGV == 0) {
			@ARGV = sort(installed_packages());
			$state->{allupdates} = 1;
		}
		my $inst = $state->repo->installed;
		for my $pkgname (@ARGV) {
			my $l;

			next if $pkgname =~ m/^quirks\-\d/;
			if (OpenBSD::PackageName::is_stem($pkgname)) {
				$l = $state->updater->stem2location($inst, $pkgname, $state);
			} else {
				$l = $inst->find($pkgname);
			}
			if (!defined $l) {
				$state->say("Problem finding #1", $pkgname);
			} else {
				push(@{$state->{setlist}},
				    $state->updateset->add_older(OpenBSD::Handle->from_location($l)));
			}
		}
	} else {

	# actual names
		for my $pkgname (@ARGV) {
			next if $pkgname =~ m/^quirks\-\d/;
			push(@{$state->{setlist}},
			    $state->updateset->$add_hints($pkgname));
		}
	}
}

sub finish_display
{
	my ($self, $state) = @_;
	OpenBSD::Add::manpages_index($state);

	# and display delayed thingies.
	if (defined $state->{updatedepends} && %{$state->{updatedepends}}) {
		$state->say("Forced updates, bogus dependencies for ",
		    join(' ', sort(keys %{$state->{updatedepends}})),
		    " may remain");
	}
	inform_user_of_problems($state);
}

sub tweak_list
{
	my ($self, $state) = @_;

	$state->run_quirks(
	    sub {
	    	my $quirks = shift;
		$quirks->tweak_list($state->{setlist}, $state);
	    });
}

sub main
{
	my ($self, $state) = @_;

	$state->progress->set_header('');
	$self->do_quirks($state);

	$self->process_setlist($state);
}


sub new_state
{
	my ($self, $cmd) = @_;
	return OpenBSD::PkgAdd::State->new($cmd);
}

1;
