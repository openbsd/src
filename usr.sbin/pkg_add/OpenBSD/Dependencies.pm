# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.47 2007/06/02 11:30:06 espie Exp $
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
	my ($class, $set) = @_;
	bless {set => $set, plist => $set->handle->{plist}, 
	    to_install => {}, deplist => [], to_register => {} }, $class;
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

	if ($state->{allow_replacing}) {
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
		if ($v) {
			push(@{$self->{deplist}}, $v);
			$self->{to_register}->{$v} = $dep;
			return;
		}
	}

	if (!$v) {
		$v = find_candidate($dep->spec, installed_packages());
	}
	if (!$v && !$state->{allow_replacing}) {
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

sub register_dependencies
{
	my ($self, $state) = @_;

	require OpenBSD::RequiredBy;
	my $pkgname = $self->pkgname;
	my @l = $self->dependencies;

	OpenBSD::Requiring->new($pkgname)->add(@l);
	for my $dep (@l) {
		OpenBSD::RequiredBy->new($dep)->add($pkgname);
	}
}

sub record_old_dependencies
{
	my ($self, $state) = @_;
	for my $o ($self->{set}->older) {
		require OpenBSD::RequiredBy;
		my @wantlist = OpenBSD::RequiredBy->new($o->{pkgname})->list;
		$o->{wantlist} = \@wantlist;
	}
}

sub adjust_old_dependencies
{
	my ($self, $state) = @_;
	my $pkgname = $self->{set}->handle->{pkgname};
	for my $o ($self->{set}->older) {
		require OpenBSD::Replace;
		require OpenBSD::RequiredBy;

		my $oldname = $o->{pkgname};

		print "Adjusting dependencies for $pkgname/$oldname\n" 
		    if $state->{beverbose};
		my $d = OpenBSD::RequiredBy->new($pkgname);
		for my $dep (@{$o->{wantlist}}) {
			if (defined $self->{set}->{skipupdatedeps}->{$dep}) {
				print "\tskipping $dep\n" if $state->{beverbose};
				next;
			}
			print "\t$dep\n" if $state->{beverbose};
			$d->add($dep);
			OpenBSD::Replace::adjust_dependency($dep, $oldname, $pkgname);
		}
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
		OpenBSD::SharedLibs::add_libs_from_installed_package($try);
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
	my $known = $self->{known};
	my $r = check_lib_spec($plist->localbase, $lib, $known);
	if ($r) {
		print "found libspec $lib in package $r\n" if $state->{verbose};
		$dependencies->{$r} = 1;
		return 1;
	}
	if ($lib !~ m|/|) {

		OpenBSD::SharedLibs::add_libs_from_system($state->{destdir});
		for my $dir (OpenBSD::SharedLibs::system_dirs()) {
			if (check_lib_spec($dir, $lib, {system => 1})) {
				print "found libspec $lib in $dir/lib\n" if $state->{very_verbose};
				return 1;
			}
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
		OpenBSD::SharedLibs::add_libs_from_installed_package($dep);
		$known->{$dep} = 1;
		if (check_lib_spec($plist->localbase, $lib, {$dep => 1})) {
			print "found libspec $lib in package $dep\n" if $state->{verbose};
			$dependencies->{$dep} = 1;
			return 1;
		} 
	}
	for my $dep (@{$plist->{depends}}) {
		$r = find_old_lib($state, $plist->localbase, $dep->{pattern}, $lib, $dependencies);
		if ($r) {
			print "found libspec $lib in old package $r\n" if $state->{verbose};
			return 1;
		}
    	}
	
	print "libspec $lib not found\n" if $state->{very_verbose};
	return;
}

1;
