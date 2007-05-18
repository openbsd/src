# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.37 2007/05/18 12:18:33 espie Exp $
#
# Copyright (c) 2005-2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Dependencies::Solver;

use OpenBSD::PackageInfo;

sub find_candidate
{
	    my $spec = shift;
	    my @candidates = $spec->filter(@_);
	    if (@candidates >= 1) {
		    return $candidates[0];
	    } else {
		    return undef;
	    }
}

sub new
{
	my ($class, $plist) = @_;
	bless {plist => $plist, to_install => {}, deplist => [], 
	    to_register => {} }, $class;
}

sub dependencies
{
	my $self = shift;
	if (wantarray) {
		return keys %{$self->{to_register}};
	} else {
		return scalar(%{$self->{to_register}});
	}
}

sub has_dep
{
	my ($self, $dep) = @_;
	return $self->{to_register}->{$dep};
}

sub pkgname
{
	my $self = shift;
	return $self->{plist}->pkgname;
}

sub add_todo
{
	my ($self, @extra) = @_;

	require OpenBSD::PackageName;

	for my $fullname (@extra) {
		$self->{to_install}->
		    {OpenBSD::PackageName::url2pkgname($fullname)} = $fullname;
	}
}

sub add_new_dep
{
	my ($self, $pkgname, $satisfy) = @_;
	push(@{$self->{deplist}}, $pkgname) if !is_installed($pkgname);
	$self->{to_register}->{$pkgname} = $satisfy;
}

sub find_dep_in_repositories
{
	my ($self, $state, $dep) = @_;
	require OpenBSD::PackageLocator;

	my @candidates = OpenBSD::PackageLocator->match($dep->spec);
	if (!$state->{forced}->{allversions}) {
		@candidates = OpenBSD::PackageName::keep_most_recent(@candidates);
	}
	if (@candidates == 1) {
		return $candidates[0];
	} elsif (@candidates > 1) {
		require OpenBSD::Interactive;

		# put default first if available
		@candidates = ((grep {$_ eq $dep->{def}} @candidates),
		    (sort (grep {$_ ne $dep->{def}} @candidates)));
		return OpenBSD::Interactive::ask_list(
		    'Ambiguous: choose dependency for '.$self->pkgname.': ',
		    $state->{interactive}, @candidates);
	} else {
		return;
	}
}

sub find_dep_in_stuff_to_install
{
	my ($self, $state, $dep) = @_;

	return find_candidate($dep->spec, keys %{$self->{to_install}});
}

sub solve_dependency
{
	my ($self, $state, $dep) = @_;

	my $v;

	if ($state->{replace}) {
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
	}

	if (!$v) {
		$v = find_candidate($dep->spec, installed_packages());
	}
	if (!$v && !$state->{replace}) {
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
	}

	if (!$v) {
		$v = $self->find_dep_in_repositories($state, $dep);
	}
	# resort to default if nothing else
	if (!$v) {
		$v = $dep->{def};
	}

	$self->add_new_dep($v, $dep);
}

sub solve
{
	my ($self, $state, @extra) = @_;

	$self->add_todo(@extra);

	for my $dep (@{$self->{plist}->{depend}}) {
	    $self->solve_dependency($state, $dep);
	}

	# prepare for closure
	my @todo = $self->dependencies;
	$self->{todo} = \@todo;
	$self->{done} = {};
	$self->{known} = {};

	return @{$self->{deplist}};
}

sub dump
{
	my $self = shift;
	if ($self->dependencies) {
	    print "Dependencies for ", $self->pkgname, " resolve to: ", 
	    	join(', ',  $self->dependencies);
	    print " (todo: ", join(',', @{$self->{deplist}}), ")" 
	    	if @{$self->{deplist}} > 0;
	    print "\n";
	    print "Full dependency tree is ", join(',', keys %{$self->{done}}), "\n"	if %{$self->{done}};
	}
}

use OpenBSD::SharedLibs;

sub check_lib_spec
{
	my ($base, $spec, $dependencies) = @_;
	my @r = OpenBSD::SharedLibs::lookup_libspec($base, $spec);
	for my $candidate (@r) {
		if ($dependencies->{$candidate}) {
			return $candidate;
		}
	}
	return;
}

sub find_old_lib
{
	my ($state, $base, $pattern, $lib, $dependencies) = @_;

	require OpenBSD::Search;
	require OpenBSD::PackageRepository::Installed;

	for my $try (OpenBSD::PackageRepository::Installed->new->match(OpenBSD::Search::PkgSpec->new(".libs-".$pattern))) {
		OpenBSD::SharedLibs::add_package_libs($try);
		if (check_lib_spec($base, $lib, {$try => 1})) {
			$dependencies->{$try} = 1;
			return "$try($lib)";
		}
	}
	return;
}

sub lookup_library
{
	my ($self, $state, $lib) = @_;

	my $plist = $self->{plist};
	my $dependencies = $self->{to_register};

	my $r = check_lib_spec($plist->localbase, $lib, $dependencies);
	if ($r) {
	    print "found libspec $lib in $r\n" if $state->{very_verbose};
	    return 1;
	}
	my $known = $self->{known};
	$r = check_lib_spec($plist->localbase, $lib, $known);
	if ($r) {
		print "found libspec $lib in dependent package $r\n" if $state->{verbose};
		delete $known->{$r};
		$dependencies->{$r} = 1;
		return 1;
	}
	if ($lib !~ m|/|) {

		OpenBSD::SharedLibs::add_system_libs($state->{destdir});
		for my $dir (OpenBSD::SharedLibs::system_dirs()) {
			if (check_lib_spec($dir, $lib, {system => 1})) {
				print "found libspec $lib in $dir/lib\n" if $state->{very_verbose};
				return 1;
			}
		}
	}
	for my $dep (@{$plist->{depends}}) {
		$r = find_old_lib($state, $plist->localbase, $dep->{pattern}, $lib, $dependencies);
		if ($r) {
			print "found libspec $lib in old package $r\n" if $state->{verbose};
			return 1;
		}
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
		next if $dependencies->{$dep};
		OpenBSD::SharedLibs::add_package_libs($dep);
		if (check_lib_spec($plist->localbase, $lib, {$dep => 1})) {
			print "found libspec $lib in dependent package $dep\n" if $state->{verbose};
			$dependencies->{$dep} = 1;
			return 1;
		} else {
			$known->{$dep} = 1;
		}
	}
	
	print "libspec $lib not found\n" if $state->{very_verbose};
	return;
}


1;
