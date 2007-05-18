# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.36 2007/05/18 09:45:18 espie Exp $
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
	bless {plist => $plist, to_install => {}, deplist => [], to_register => {} }, $class;
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
	push(@{$self->{deplist}}, $pkgname);
	$self->add_installed_dep($pkgname, $satisfy);
}

sub add_installed_dep
{
	my ($self, $pkgname, $satisfy) = @_;
	$self->{to_register}->{$pkgname} = $satisfy;
}

sub solve_dependency
{
	my ($self, $state, $dep) = @_;

	my $spec = $dep->spec;
	if ($state->{replace}) {
	    my $v = find_candidate($spec, keys %{$self->{to_install}});
	    if ($v) {
		$self->add_new_dep($v, $dep);
		return;
	    }
	}

	my $v = find_candidate($spec, installed_packages());
	if ($v) {
		$self->add_installed_dep($v, $dep);
		return;
	}
	if (!$state->{replace}) {
	    my $v = find_candidate($spec, keys %{$self->{to_install}});
	    if ($v) {
		$self->add_new_dep($v, $dep);
	    	return;
	    }
	}
	require OpenBSD::PackageLocator;

	# try with list of available packages
	my @candidates = OpenBSD::PackageLocator->match($spec);
	if (!$state->{forced}->{allversions}) {
	    @candidates = OpenBSD::PackageName::keep_most_recent(@candidates);
	}
	# one single choice
	if (@candidates == 1) {
	    $self->add_new_dep($candidates[0], $dep);
	    return;
	}
	if (@candidates > 1) {
	    require OpenBSD::Interactive;

	    # put default first if available
	    @candidates = ((grep {$_ eq $dep->{def}} @candidates),
			    (sort (grep {$_ ne $dep->{def}} @candidates)));
	    my $choice = 
		OpenBSD::Interactive::ask_list('Ambiguous: choose dependency for '.$self->pkgname.': ',
		    $state->{interactive}, @candidates);
	    $self->add_new_dep($choice, $dep);
	    return;
	}
	# can't get a list of packages, assume default
	# will be there.
	$self->add_new_dep($dep->{def}, $dep);
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
