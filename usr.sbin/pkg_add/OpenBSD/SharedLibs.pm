# ex:ts=8 sw=4:
# $OpenBSD: SharedLibs.pm,v 1.51 2010/06/30 10:51:04 espie Exp $
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
	my ($self, $pkgname) = @_;
	OpenBSD::SharedLibs::register_lib($self->fullname, $pkgname);
}

package OpenBSD::SharedLibs;
use File::Basename;
use OpenBSD::Error;

my $path;
my @ldconfig = (OpenBSD::Paths->ldconfig);


sub init_path($)
{
	my $destdir = shift;
	$path={};
	if ($destdir ne '') {
		unshift @ldconfig, OpenBSD::Paths->chroot, '--', $destdir;
	}
	open my $fh, "-|", @ldconfig, "-r";
	if (defined $fh) {
		my $_;
		while (<$fh>) {
			if (m/^\s*search directories:\s*(.*?)\s*$/o) {
				for my $d (split(/\:/o, $1)) {
					$path->{$d} = 1;
				}
				last;
			}
		}
		close($fh);
	} else {
		print STDERR "Can't find ldconfig\n";
	}
}

sub mark_ldconfig_directory
{
	my ($name, $destdir) = @_;
	if (!defined $path) {
		init_path($destdir);
	}
	my $d = dirname($name);
	if ($path->{$d}) {
		$OpenBSD::PackingElement::Lib::todo = 1;
	}
}

sub ensure_ldconfig
{
	my $state = shift;
	$state->vsystem(@ldconfig, "-R") unless $state->{not};
	$OpenBSD::PackingElement::Lib::todo = 0;
}

our $repo = OpenBSD::LibRepo->new;

sub register_lib
{
	my ($name, $pkgname) = @_;
	my $lib = OpenBSD::Library->from_string($name);
	if ($lib->is_valid) {
		$repo->register($lib, $pkgname);
	} else {
		print STDERR "Bogus library in $pkgname: $name\n"
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
	my ($destdir) = @_;
	return if $done_plist->{'system'};
	$done_plist->{'system'} = 1;
	for my $dirname (system_dirs()) {
		opendir(my $dir, $destdir.$dirname."/lib") or next;
		while (my $d = readdir($dir)) {
			next unless $d =~ m/\.so/;
			register_lib("$dirname/lib/$d", 'system');
		}
		closedir($dir);
	}
}

sub add_libs_from_installed_package
{
	my $pkgname = shift;
	return if $done_plist->{$pkgname};
	$done_plist->{$pkgname} = 1;
	my $plist = OpenBSD::PackingList->from_installation($pkgname,
	    \&OpenBSD::PackingList::LibraryOnly);
	return if !defined $plist;

	$plist->mark_available_lib($pkgname);
}

sub add_libs_from_plist
{
	my $plist = shift;
	my $pkgname = $plist->pkgname;
	return if $done_plist->{$pkgname};
	$done_plist->{$pkgname} = 1;
	$plist->mark_available_lib($pkgname);
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
