# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.1 2005/08/07 16:25:35 espie Exp $
#
# Copyright (c) 2005 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Dependencies;

use OpenBSD::PackageName;
use OpenBSD::PkgSpec;
use OpenBSD::PackageInfo;
use OpenBSD::SharedLibs;
use OpenBSD::Error;

sub solve
{
	my ($state, $handle, @extra) = @_;
	my $plist = $handle->{plist};
	my $verbose = $state->{verbose};
	my $to_register = $handle->{solved_dependencies} = {};
	my $to_install = {};
	for my $fullname (@extra) {
		$to_install->{OpenBSD::PackageName::url2pkgname($fullname)} = 
		    $fullname;
	}

	# do simple old style pkgdep first
	my @deps = ();
	for my $dep (@{$plist->{pkgdep}}) {
		if (!is_installed($dep->{name})) {
			push(@deps, $dep->{name});
		}
		$to_register->{$dep->{name}} = 1;
	}
	for my $dep (@{$plist->{depend}}, @{$plist->{newdepend}}, @{$plist->{libdepend}}) {
	    next if defined $dep->{name} and $dep->{name} ne $plist->pkgname();

	    my @candidates;
	    if ($state->{replace}) {
		# try against list of packages to install
		@candidates = OpenBSD::PkgSpec::match($dep->{pattern}, keys %{$to_install});
		if (@candidates >= 1) {
		    push(@deps, $to_install->{$candidates[0]});
		    $to_register->{$candidates[0]} = 1;
		    next;
		}
	    }
	    @candidates = OpenBSD::PkgSpec::match($dep->{pattern}, installed_packages());
	    if (@candidates >= 1) {
		    $to_register->{$candidates[0]} = 1;
		    next;
	    }
	    if (!$state->{replace}) {
		# try against list of packages to install
		@candidates = OpenBSD::PkgSpec::match($dep->{pattern}, keys %{$to_install});
		if (@candidates >= 1) {
		    push(@deps, $to_install->{$candidates[0]});
		    $to_register->{$candidates[0]} = 1;
		    next;
		}
	    }
	    # try with list of available packages
	    @candidates = OpenBSD::PkgSpec::match($dep->{pattern}, OpenBSD::PackageLocator::available());
	    # one single choice
	    if (@candidates == 1) {
		push(@deps, $candidates[0]);
		$to_register->{$candidates[0]} = 1;
		next;
	    }
	    if (@candidates > 1) {
		# grab default if available
		if (grep {$_ eq $dep->{def}} @candidates) {
		    push(@deps, $dep->{def});
		    $to_register->{$dep->{def}} = 1;
		    next;
		}
		push(@deps, $candidates[0]);
		$to_register->{$candidates[0]} = 1;
	    }
	    # can't get a list of packages, assume default
	    # will be there.
	    push(@deps, $dep->{def});
	    $to_register->{$dep->{def}} = 1;
	}

	if ($verbose && %$to_register) {
	    print "Dependencies for ", $plist->pkgname(), " resolve to: ", 
	    	join(', ', keys %$to_register);
	    print " (todo: ", join(',', @deps), ")" if @deps > 0;
	    print "\n";
	}
	return @deps;
}

sub check_lib_spec
{
	my ($verbose, $base, $spec, $dependencies) = @_;
	my @r = OpenBSD::SharedLibs::lookup_libspec($base, $spec);
	for my $candidate (@r) {
		if ($dependencies->{$candidate}) {
			print " found in $candidate\n" if $verbose;
			return 1;
		}
	}
	print " not found." if $verbose;
	return undef;
}

sub find_old_lib
{
	my ($state, $base, $pattern, $lib, $dependencies) = @_;

	$pattern = ".libs-".$pattern;
	for my $try (OpenBSD::PkgSpec::match($pattern, installed_packages())) {
		OpenBSD::SharedLibs::add_package_libs($try);
		if (check_lib_spec($state->{very_verbose},
		    $base, $lib, {$try => 1})) {
			Warn "Found library ", $lib, " in old package $try\n"
			    if $state->{verbose};
			$dependencies->{$try} = 1;
			return 1;
		}
	}
	return 0;
}

sub lookup_library
{
	my ($state, $lib, $plist, $dependencies, $harder, $done) = @_;

	print "checking libspec $lib..." if $state->{very_verbose};
	if (check_lib_spec($state->{very_verbose},
	    $plist->pkgbase(), $lib, $dependencies)) {
	    return 1;
	}
	if ($harder && $lib !~ m|/|) {

		OpenBSD::SharedLibs::add_system_libs($state->{destdir});
		if (check_lib_spec($state->{very_verbose},
		    "/usr", $lib, {system => 1})) {
			return 1;
		}
		if (check_lib_spec($state->{very_verbose},
		    "/usr/X11R6", $lib, {system => 1})) {
			return 1;
		}
	}
	for my $dep (@{$plist->{depends}}) {
		if (find_old_lib($state, $plist->pkgbase(), $dep->{pattern}, $lib, $dependencies)) {
			return 1;
		}
    	}
	if ($harder) {
		# lookup through the full tree...
		my @todo = keys %$dependencies;
		while (my $dep = pop @todo) {
			require OpenBSD::RequiredBy;

			next if $done->{$dep};
			$done->{$dep} = 1;
			for my $dep2 (OpenBSD::Requiring->new($dep)->list()) {
				push(@todo, $dep2) unless $done->{$dep2};
			}
			next if $dependencies->{$dep};
			OpenBSD::SharedLibs::add_package_libs($dep);
			if (check_lib_spec($state->{very_verbose},
			    $plist->pkgbase(), $lib, {$dep => 1})) {
				Warn "Found library ", $lib, " in dependent package $dep\n" if $state->{verbose};
				$dependencies->{$dep} = 1;
				return 1;
			}
		}
	}
	if ($state->{forced}->{boguslibs}) {
		my $explored = {};
		# lookup through the full tree...
		my @todo = keys %$dependencies;
		while (my $dep = pop @todo) {
			require OpenBSD::RequiredBy;

			next if $explored->{$dep};
			$explored->{$dep} = 1;
			for my $dep2 (OpenBSD::Requiring->new($dep)->list()) {
				push(@todo, $dep2) unless $done->{$dep2};
			}
			OpenBSD::SharedLibs::add_bogus_package_libs($dep);
			if (check_lib_spec($state->{very_verbose},
			    $plist->pkgbase(), $lib, {$dep => 1})) {
				Warn "Found unmarked library ", $lib, " in dependent package $dep\n" if $state->{verbose};
				$dependencies->{$dep} = 1;
				return 1;
			}
		}
	}
	print "\n" if $state->{very_verbose};
	return;
}


1;
