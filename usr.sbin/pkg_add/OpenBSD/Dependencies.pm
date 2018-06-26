# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.171 2018/06/26 09:40:33 espie Exp $
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
use OpenBSD::Dependencies::SolverBase;

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
					delete $k->solver->{to_register};
					push(@real, $k);
				}
				$merged->{$k} = 1;
				$k = $done->{$k};
			}
		}
		delete $initial->solver->{deplist};
		delete $initial->solver->{to_register};
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
		# unless -ii, we return the def if available
		if ($state->is_interactive < 2) {
			if (defined(my $d = $c{$dep->{def}})) {
				return $d;
			}
		}
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
		$self->add_dep($state->tracker->{to_install}{$v});
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
		    	defined $self->{set}{newer}{$dep};
	}
	return $self->{bad};
}

sub register_dependencies
{
	my ($self, $state) = @_;

	require OpenBSD::RequiredBy;
	for my $pkg ($self->{set}->newer) {
		my $pkgname = $pkg->pkgname;
		my @l = keys %{$self->{to_register}{$pkg}};

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

sub solve_old_depends
{
	my ($self, $state) = @_;

	$self->{old_dependencies} = {};
	for my $package ($self->{set}->older) {
		for my $dep (@{$package->dependency_info->{depend}}) {
			my $v = $self->solve_dependency($state, $dep, $package);
			# XXX
			next if !defined $v;
			$self->{old_dependencies}{$v} = $dep;
		}
	}
}

sub solve_handle_tags
{
	my ($solver, $h, $state) = @_;
	my $plist = $h->plist;
	return 1 if !defined $plist->{tags};
	my $okay = 1;
	$solver->{tag_finder} //= OpenBSD::lookup::tag->new($solver, $state);
	for my $tag (@{$plist->{tags}}) {
		$solver->{tag_finder}->lookup($solver,
		    $solver->{to_register}{$h}, $state, $tag)
		 || $solver->find_in_self($plist, $state, $tag);
		if (!$solver->verify_tag($tag, $state, $plist, $h->{is_old})) {
			$okay = 0;
		}
	}
	return $okay;
}

sub solve_tags
{
	my ($solver, $state) = @_;

	my $okay = 1;
	for my $h ($solver->{set}->changed_handles) {
		if (!$solver->solve_handle_tags($h, $state)) {
			$okay = 0;
		}
	}
	if (!$okay) {
		$solver->dump($state);
		$solver->{tag_finder}->dump($state);
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
