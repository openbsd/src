# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.154 2014/11/30 15:56:34 espie Exp $
#
# Copyright (c) 2005-2010 Marc Espie <espie@openbsd.org>
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

use OpenBSD::SharedLibs;

# generic dependencies lookup class: walk the dependency tree as far
# as necessary to resolve dependencies
package OpenBSD::lookup;

sub lookup
{
	my ($self, $solver, $dependencies, $state, $obj) = @_;

	my $known = $self->{known};
	if (my $r = $self->find_in_already_done($solver, $state, $obj)) {
		$dependencies->{$r} = 1;
		return 1;
	}
	if ($self->find_in_extra_sources($solver, $state, $obj)) {
		return 1;
	}
	# lookup through the rest of the tree...
	my $done = $self->{done};
	while (my $dep = pop @{$self->{todo}}) {
		require OpenBSD::RequiredBy;

		next if $done->{$dep};
		$done->{$dep} = 1;
		for my $dep2 (OpenBSD::Requiring->new($dep)->list) {
			push(@{$self->{todo}}, $dep2) unless $done->{$dep2};
		}
		$known->{$dep} = 1;
		if ($self->find_in_new_source($solver, $state, $obj, $dep)) {
			$dependencies->{$dep} = 1;
			return 1;
		}
	}
	if (my $r = $self->find_elsewhere($solver, $state, $obj)) {
		$dependencies->{$r} = 1;
		return 1;
	}

	return 0;
}

sub new
{
	my ($class, $solver) = @_;

	# prepare for closure
	my @todo = $solver->dependencies;
	bless { todo => \@todo, done => {}, known => {} }, $class;
}

sub dump
{
	my ($self, $state) = @_;

	return unless %{$self->{done}};
	$state->say("Full dependency tree is #1",
	    join(' ', keys %{$self->{done}}));
}

package OpenBSD::lookup::library;
our @ISA=qw(OpenBSD::lookup);

sub say_found
{
	my ($self, $state, $obj, $where) = @_;

	$state->say("found libspec #1 in #2", $obj->to_string, $where)
	    if $state->verbose >= 3;
}

sub find_in_already_done
{
	my ($self, $solver, $state, $obj) = @_;


	my $r = $solver->check_lib_spec($solver->{localbase}, $obj,
	    $self->{known});
	if ($r) {
		$self->say_found($state, $obj, $state->f("package #1", $r));
		return $r;
	} else {
		return undef;
	}
}

sub find_in_extra_sources
{
	my ($self, $solver, $state, $obj) = @_;
	return undef if !$obj->is_valid || defined $obj->{dir};

	OpenBSD::SharedLibs::add_libs_from_system($state->{destdir}, $state);
	for my $dir (OpenBSD::SharedLibs::system_dirs()) {
		if ($solver->check_lib_spec($dir, $obj, {system => 1})) {
			$self->say_found($state, $obj, $state->f("#1/lib", $dir));
			return 'system';
		}
	}
	return undef;
}

sub find_in_new_source
{
	my ($self, $solver, $state, $obj, $dep) = @_;

	if (defined $solver->{set}->{newer}->{$dep}) {
		OpenBSD::SharedLibs::add_libs_from_plist($solver->{set}->{newer}->{$dep}->plist, $state);
	} else {
		OpenBSD::SharedLibs::add_libs_from_installed_package($dep, $state);
	}
	if ($solver->check_lib_spec($solver->{localbase}, $obj, {$dep => 1})) {
		$self->say_found($state, $obj, $state->f("package #1", $dep));
		return $dep;
	}
	return undef;
}

sub find_elsewhere
{
	my ($self, $solver, $state, $obj) = @_;

	for my $n ($solver->{set}->newer) {
		for my $dep (@{$n->dependency_info->{depend}}) {
			my $r = $solver->find_old_lib($state,
			    $solver->{localbase}, $dep->{pattern}, $obj);
			if ($r) {
				$self->say_found($state, $obj,
				    $state->f("old package #1", $r));
				return $r;
			}
		}
	}
	return undef;
}

package OpenBSD::lookup::tag;
our @ISA=qw(OpenBSD::lookup);
sub find_in_extra_sources
{
}

sub find_elsewhere
{
}

sub find_in_already_done
{
	my ($self, $solver, $state, $obj) = @_;
	my $r = $self->{known_tags}->{$obj};
	if (defined $r) {
		$state->say("Found tag #1 in #2", $obj, $r)
		    if $state->verbose >= 3;
	}
	return $r;
}

sub find_in_plist
{
	my ($self, $plist, $dep) = @_;
	if ($plist->has('define-tag')) {
		for my $t (@{$plist->{'define-tag'}}) {
			$self->{known_tags}->{$t->name} = $dep;
		}
	}
}

sub find_in_new_source
{
	my ($self, $solver, $state, $obj, $dep) = @_;
	my $plist = OpenBSD::PackingList->from_installation($dep,
	    \&OpenBSD::PackingList::DependOnly);
	if (!defined $plist) {
		$state->errsay("Can't read plist for #1", $dep);
	}
	$self->find_in_plist($plist, $dep);
	return $self->find_in_already_done($solver, $state, $obj);
}

package _cache;

sub new
{
	my ($class, $v) = @_;
	bless \$v, $class;
}

sub pretty
{
	my $self = shift;
	return ref($self)."(".$$self.")";
}

package _cache::self;
our @ISA=(qw(_cache));
sub do
{
	my ($v, $solver, $state, $dep, $package) = @_;
	push(@{$package->{before}}, $$v);
	return $$v;
}

package _cache::installed;
our @ISA=(qw(_cache));
sub do
{
	my ($v, $solver, $state, $dep, $package) = @_;
	return $$v;
}

package _cache::bad;
our @ISA=(qw(_cache));
sub do
{
	my ($v, $solver, $state, $dep, $package) = @_;
	return $$v;
}

package _cache::to_install;
our @ISA=(qw(_cache));
sub do
{
	my ($v, $solver, $state, $dep, $package) = @_;
	if ($state->tracker->{uptodate}{$$v}) {
		bless $v, "_cache::installed";
		$solver->set_global($dep, $v);
		return $$v;
	}
	if ($state->tracker->{cant_install}{$$v}) {
		bless $v, "_cache::bad";
		$solver->set_global($dep, $v);
		return $$v;
	}
	if ($state->tracker->{to_install}{$$v}) {
		my $set = $state->tracker->{to_install}{$$v};
		if ($set->real_set eq $solver->{set}) {
			bless $v, "_cache::self";
			return $v->do($solver, $state, $dep, $package);
		} else {
			$solver->add_dep($set);
			return $$v;
		}
	}
	return;
}

package _cache::to_update;
our @ISA=(qw(_cache));
sub do
{
	my ($v, $solver, $state, $dep, $package) = @_;
	my $alt = $solver->find_dep_in_self($state, $dep);
	if ($alt) {
		$solver->set_cache($dep, _cache::self->new($alt));
		push(@{$package->{before}}, $alt);
		return $alt;
	}

	if ($state->tracker->{to_update}{$$v}) {
		$solver->add_dep($state->tracker->{to_update}{$$v});
		return $$v;
	}
	if ($state->tracker->{uptodate}{$$v}) {
		bless $v, "_cache::installed";
		$solver->set_global($dep, $v);
		return $$v;
	}
	if ($state->tracker->{cant_update}{$$v}) {
		bless $v, "_cache::bad";
		$solver->set_global($dep, $v);
		return $$v;
	}
	my @candidates = $dep->spec->filter(keys %{$state->tracker->{installed}});
	if (@candidates > 0) {
		$solver->set_global($dep, _cache::installed->new($candidates[0]));
		return $candidates[0];
	}
	return;
}

package OpenBSD::Cloner;
sub clone
{
	my ($self, $h, @extra) = @_;
	for my $extra (@extra) {
		next unless defined $extra;
		while (my ($k, $e) = each %{$extra->{$h}}) {
			$self->{$h}{$k} //= $e;
		}
	}
}

package OpenBSD::Dependencies::SolverBase;
our @ISA = qw(OpenBSD::Cloner);

my $global_cache = {};

sub cached
{
	my ($self, $dep) = @_;
	return $global_cache->{$dep->{pattern}} ||
	    $self->{cache}{$dep->{pattern}};
}

sub set_cache
{
	my ($self, $dep, $value) = @_;
	$self->{cache}{$dep->{pattern}} = $value;
}

sub set_global
{
	my ($self, $dep, $value) = @_;
	$global_cache->{$dep->{pattern}} = $value;
}

sub global_cache
{
	my ($self, $pattern) = @_;
	return $global_cache->{$pattern};
}

sub find_candidate
{
	my ($self, $dep, @list) = @_;
	my @candidates = $dep->spec->filter(@list);
	if (@candidates >= 1) {
		return $candidates[0];
	} else {
		return undef;
	}
}

sub solve_dependency
{
	my ($self, $state, $dep, $package) = @_;

	my $v;

	if (defined $self->cached($dep)) {
		if ($state->defines('stat_cache')) {
			if (defined $self->global_cache($dep->{pattern})) {
				$state->print("Global ");
			}
			$state->say("Cache hit on #1: #2", $dep->{pattern},
			    $self->cached($dep)->pretty);
		}
		$v = $self->cached($dep)->do($self, $state, $dep, $package);
		return $v if $v;
	}
	if ($state->defines('stat_cache')) {
		$state->say("No cache hit on #1", $dep->{pattern});
	}

	$self->really_solve_dependency($state, $dep, $package);
}

sub solve_depends
{
	my ($self, $state) = @_;

	$self->{all_dependencies} = {};
	$self->{to_register} = {};
	$self->{deplist} = {};
	delete $self->{installed};

	for my $package ($self->{set}->newer, $self->{set}->kept) {
		$package->{before} = [];
		for my $dep (@{$package->dependency_info->{depend}}) {
			my $v = $self->solve_dependency($state, $dep, $package);
			# XXX
			next if !defined $v;
			$self->{all_dependencies}->{$v} = $dep;
			$self->{to_register}->{$package}->{$v} = $dep;
		}
	}

	return values %{$self->{deplist}};
}

sub solve_wantlibs
{
	my ($solver, $state) = @_;
	my $okay = 1;

	my $lib_finder = OpenBSD::lookup::library->new($solver);
	for my $h ($solver->{set}->newer) {
		for my $lib (@{$h->{plist}->{wantlib}}) {
			$solver->{localbase} = $h->{plist}->localbase;
			next if $lib_finder->lookup($solver,
			    $solver->{to_register}->{$h}, $state,
			    $lib->spec);
			if ($okay) {
				$solver->errsay_library($state, $h);
			}
			$okay = 0;
			OpenBSD::SharedLibs::report_problem($state,
			    $lib->spec);
		}
	}
	if (!$okay) {
		$solver->dump($state);
		$lib_finder->dump($state);
	}
	return $okay;
}

sub dump
{
	my ($self, $state) = @_;
	if ($self->dependencies) {
	    $state->print("Direct dependencies for #1 resolve to #2",
	    	$self->{set}->print, join(' ',  $self->dependencies));
	    $state->print(" (todo: #1)",
	    	join(' ', (map {$_->print} values %{$self->{deplist}})))
	    	if %{$self->{deplist}};
	    $state->print("\n");
	}
}

sub dependencies
{
	my $self = shift;
	if (wantarray) {
		return keys %{$self->{all_dependencies}};
	} else {
		return scalar(%{$self->{all_dependencies}});
	}
}

sub check_lib_spec
{
	my ($self, $base, $spec, $dependencies) = @_;
	my $r = OpenBSD::SharedLibs::lookup_libspec($base, $spec);
	for my $candidate (@$r) {
		if ($dependencies->{$candidate->origin}) {
			return $candidate->origin;
		}
	}
	return;
}

sub find_dep_in_installed
{
	my ($self, $state, $dep) = @_;

	return $self->find_candidate($dep, @{$self->installed_list});
}

sub find_dep_in_self
{
	my ($self, $state, $dep) = @_;

	return $self->find_candidate($dep, $self->{set}->newer_names);
}

use OpenBSD::PackageInfo;
OpenBSD::Auto::cache(installed_list,
	sub {
		my $self = shift;
		my @l = installed_packages();

		for my $o ($self->{set}->older_names) {
			@l = grep {$_ ne $o} @l;
		}
		return \@l;
	}
);

sub add_dep
{
	my ($self, $d) = @_;
	$self->{deplist}{$d} = $d;
}

package OpenBSD::Dependencies::Solver;
our @ISA = qw(OpenBSD::Dependencies::SolverBase);

use OpenBSD::PackageInfo;

sub merge
{
	my ($solver, @extra) = @_;

	$solver->clone('cache', @extra);
}

sub new
{
	my ($class, $set) = @_;
	bless { set => $set, bad => [] }, $class;
}

sub check_for_loops
{
	my ($self, $state) = @_;

	my $initial = $self->{set};

	my @todo = ();
	my @to_merge = ();
	push(@todo, $initial);
	my $done = {};

	while (my $set = shift @todo) {
		next unless defined $set->{solver};
		for my $l (values %{$set->solver->{deplist}}) {
			if ($l eq $initial) {
				push(@to_merge, $set);
			}
			next if $done->{$l};
			next if $done->{$l->real_set};
			push(@todo, $l);
			$done->{$l} = $set;
		}
	}
	if (@to_merge > 0) {
		my $merged = {};
		my @real = ();
		$state->say("Detected loop, merging sets #1", $state->ntogo);
		$state->say("| #1", $initial->print);
		for my $set (@to_merge) {
			my $k = $set;
			while ($k ne $initial && !$merged->{$k}) {
				unless ($k->{finished}) {
					$state->say("| #1", $k->print);
					delete $k->solver->{deplist};
					push(@real, $k);
				}
				$merged->{$k} = 1;
				$k = $done->{$k};
			}
		}
		delete $initial->solver->{deplist};
		$initial->merge($state->tracker, @real);
	}
}

sub find_dep_in_repositories
{
	my ($self, $state, $dep) = @_;

	return unless $dep->spec->is_valid;

	my $candidates = $self->{set}->match_locations($dep->spec);
	if (!$state->defines('allversions')) {
		require OpenBSD::Search;
		$candidates = OpenBSD::Search::FilterLocation->
		    keep_most_recent->filter_locations($candidates);
	}
	# XXX not really efficient, but hey
	my %c = map {($_->name, $_)} @$candidates;
	my @pkgs = keys %c;
	if (@pkgs == 1) {
		return $candidates->[0];
	} elsif (@pkgs > 1) {
		# put default first if available
		@pkgs = ((grep {$_ eq $dep->{def}} @pkgs),
		    (sort (grep {$_ ne $dep->{def}} @pkgs)));
		my $good = $state->ask_list(
		    'Ambiguous: choose dependency for '.$self->{set}->print.': ',
		    @pkgs);
		return $c{$good};
	} else {
		return;
	}
}

sub find_dep_in_stuff_to_install
{
	my ($self, $state, $dep) = @_;

	my $v = $self->find_candidate($dep,
	    keys %{$state->tracker->{uptodate}});
	if ($v) {
		$self->set_global($dep, _cache::installed->new($v));
		return $v;
	}
	# this is tricky, we don't always know what we're going to actually
	# install yet.
	my @candidates = $dep->spec->filter(keys %{$state->tracker->{to_update}});
	if (@candidates > 0) {
		for my $k (@candidates) {
			my $set = $state->tracker->{to_update}{$k};
			$self->add_dep($set);
		}
		if (@candidates == 1) {
			$self->set_cache($dep,
			    _cache::to_update->new($candidates[0]));
		}
		return $candidates[0];
	}

	$v = $self->find_candidate($dep, keys %{$state->tracker->{to_install}});
	if ($v) {
		$self->set_cache($dep, _cache::to_install->new($v));
		$self->add_dep($state->tracker->{to_install}->{$v});
	}
	return $v;
}

sub really_solve_dependency
{
	my ($self, $state, $dep, $package) = @_;

	my $v;

	if ($state->{allow_replacing}) {

		$v = $self->find_dep_in_self($state, $dep);
		if ($v) {
			$self->set_cache($dep, _cache::self->new($v));
			push(@{$package->{before}}, $v);
			return $v;
		}
		$v = $self->find_candidate($dep, $self->{set}->older_names);
		if ($v) {
			push(@{$self->{bad}}, $dep->{pattern});
			return $v;
		}
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
		return $v if $v;
	}

	$v = $self->find_dep_in_installed($state, $dep);
	if ($v) {
		if ($state->{newupdates}) {
			if ($state->tracker->is_known($v)) {
				return $v;
			}
			my $set = $state->updateset->add_older(OpenBSD::Handle->create_old($v, $state));
			$set->merge_paths($self->{set});
			$self->add_dep($set);
			$self->set_cache($dep, _cache::to_update->new($v));
			$state->tracker->todo($set);
		}
		return $v;
	}
	if (!$state->{allow_replacing}) {
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
		return $v if $v;
	}

	$v = $self->find_dep_in_repositories($state, $dep);

	my $s;
	if ($v) {
		$s = $state->updateset_from_location($v);
		$v = $v->name;
	} else {
		# resort to default if nothing else
		$v = $dep->{def};
		$s = $state->updateset_with_new($v);
	}

	$s->merge_paths($self->{set});
	$state->tracker->todo($s);
	$self->add_dep($s);
	$self->set_cache($dep, _cache::to_install->new($v));
	return $v;
}

sub check_depends
{
	my $self = shift;

	for my $dep ($self->dependencies) {
		push(@{$self->{bad}}, $dep)
		    unless is_installed($dep) or
		    	defined $self->{set}->{newer}->{$dep};
	}
	return $self->{bad};
}

sub register_dependencies
{
	my ($self, $state) = @_;

	require OpenBSD::RequiredBy;
	for my $pkg ($self->{set}->newer) {
		my $pkgname = $pkg->pkgname;
		my @l = keys %{$self->{to_register}->{$pkg}};

		OpenBSD::Requiring->new($pkgname)->add(@l);
		for my $dep (@l) {
			OpenBSD::RequiredBy->new($dep)->add($pkgname);
		}
	}
}

sub repair_dependencies
{
	my ($self, $state) = @_;
	for my $p ($self->{set}->newer) {
		my $pkgname = $p->pkgname;
		for my $pkg (installed_packages(1)) {
			my $plist = OpenBSD::PackingList->from_installation(
			    $pkg, \&OpenBSD::PackingList::DependOnly);
			$plist->repair_dependency($pkg, $pkgname);
		}
	}
}

sub find_old_lib
{
	my ($self, $state, $base, $pattern, $lib) = @_;

	require OpenBSD::Search;

	my $r = $state->repo->installed->match_locations(OpenBSD::Search::PkgSpec->new(".libs-".$pattern));
	for my $try (map {$_->name} @$r) {
		OpenBSD::SharedLibs::add_libs_from_installed_package($try, $state);
		if ($self->check_lib_spec($base, $lib, {$try => 1})) {
			return $try;
		}
	}
	return undef;
}

sub errsay_library
{
	my ($solver, $state, $h) = @_;

	$state->errsay("Can't install #1 because of libraries", $h->pkgname);
}

sub solve_tags
{
	my ($solver, $state) = @_;
	my $okay = 1;

	my $tag_finder = OpenBSD::lookup::tag->new($solver);
	for my $h ($solver->{set}->newer) {
		for my $tag (keys %{$h->{plist}->{tags}}) {
			next if $tag_finder->lookup($solver,
			    $solver->{to_register}->{$h}, $state, $tag);
			$state->errsay("Can't install #1: tag definition not found #2",
			    $h->pkgname, $tag);
			if ($okay) {
				$solver->dump($state);
				$tag_finder->dump($state);
				$okay = 0;
			}
	    	}
	}
	return $okay;
}

package OpenBSD::PackingElement;
sub repair_dependency
{
}

package OpenBSD::PackingElement::Dependency;
sub repair_dependency
{
	my ($self, $requiring, $required) = @_;
	if ($self->spec->filter($required) == 1) {
		require OpenBSD::RequiredBy;
		OpenBSD::RequiredBy->new($required)->add($requiring);
		OpenBSD::Requiring->new($requiring)->add($required);
	}
}

1;
