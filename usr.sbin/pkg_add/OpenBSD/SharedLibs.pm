# ex:ts=8 sw=4:
# $OpenBSD: SharedLibs.pm,v 1.57 2010/12/24 10:31:59 espie Exp $
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

use OpenBSD::Paths;
use OpenBSD::LibSpec;

package OpenBSD::PackingElement;

sub mark_available_lib
{
}

package OpenBSD::PackingElement::Lib;

sub mark_available_lib
{
	my ($self, $pkgname, $state) = @_;
	OpenBSD::SharedLibs::register_libname($self->fullname,
	    $pkgname, $state);
}

package OpenBSD::SharedLibs;
use File::Basename;
use OpenBSD::Error;

our $repo = OpenBSD::LibRepo->new;

sub register_library
{
	my ($lib, $pkgname) = @_;
	$repo->register($lib, $pkgname);
}

sub register_libname
{
	my ($name, $pkgname, $state) = @_;
	my $lib = OpenBSD::Library->from_string($name);
	if ($lib->is_valid) {
		register_library($lib, $pkgname);
	} else {
		$state->errsay("Bogus library in #1: #2", $pkgname, $name)
		    unless $pkgname eq 'system';
	}

}

my $done_plist = {};

sub system_dirs
{
	return OpenBSD::Paths->library_dirs;
}

sub add_libs_from_system
{
	my ($destdir, $state) = @_;
	return if $done_plist->{'system'};
	$done_plist->{'system'} = 1;
	for my $dirname (system_dirs()) {
		opendir(my $dir, $destdir.$dirname."/lib") or next;
		while (my $d = readdir($dir)) {
			next unless $d =~ m/\.so/;
			register_libname("$dirname/lib/$d", 'system', $state);
		}
		closedir($dir);
	}
}

sub add_libs_from_installed_package
{
	my ($pkgname, $state) = @_;
	return if $done_plist->{$pkgname};
	$done_plist->{$pkgname} = 1;
	my $plist = OpenBSD::PackingList->from_installation($pkgname,
	    \&OpenBSD::PackingList::LibraryOnly);
	return if !defined $plist;

	$plist->mark_available_lib($pkgname, $state);
}

sub add_libs_from_plist
{
	my ($plist, $state) = @_;
	my $pkgname = $plist->pkgname;
	return if $done_plist->{$pkgname};
	$done_plist->{$pkgname} = 1;
	$plist->mark_available_lib($pkgname, $state);
}

sub lookup_libspec
{
	my ($base, $spec) = @_;
	return $spec->lookup($repo, $base);
}

my $printed = {};

sub report_problem
{
	my ($state, $spec) = @_;
	my $name = $spec->to_string;
	my $base = $state->{localbase};
	my $approx = $spec->lookup_stem($repo);

	my $r = "";
	if (!$spec->is_valid) {
		$r = "| bad library specification\n";
	} elsif (!defined $approx) {
 		$r = "| not found anywhere\n";
	} else {
		for my $bad (@$approx) {
			my $ouch = $spec->no_match($bad, $base);
			$ouch //= "not reachable";
			$r .= "| ".$bad->to_string." (".$bad->origin."): ".
			    $ouch."\n";
		}
	}
	if (!defined $printed->{$name} || $printed->{$name} ne $r) {
		$printed->{$name} = $r;
		$state->errsay("|library #1 not found", $name);
		$state->print("#1", $r);
	}
}

1;
