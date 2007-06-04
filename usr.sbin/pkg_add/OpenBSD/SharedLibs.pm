# ex:ts=8 sw=4:
# $OpenBSD: SharedLibs.pm,v 1.21 2007/06/04 17:00:45 espie Exp $
#
# Copyright (c) 2003-2005 Marc Espie <espie@openbsd.org>
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
my @ldconfig = ('/sbin/ldconfig');


sub init_path($)
{
	my $destdir = shift;
	$path={};
	if ($destdir ne '') {
		unshift @ldconfig, 'chroot', $destdir;
	}
	open my $fh, "-|", @ldconfig, "-r";
	if (defined $fh) {
		local $_;
		while (<$fh>) {
			if (m/^\s*search directories:\s*(.*?)\s*$/o) {
				for my $d (split(/\:/o, $1)) {
					$path->{$d} = 1;
				}
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
	VSystem($state->{very_verbose}, 
	    @ldconfig, "-R") unless $state->{not};
	$OpenBSD::PackingElement::Lib::todo = 0;
}

our $registered_libs = {};

sub register_lib
{
	my ($name, $pkgname) = @_;
	my ($stem, $major, $minor, $dir) = 
	    OpenBSD::PackingElement::Lib->parse($name);
	if (defined $stem) {
		push(@{$registered_libs->{$stem}->{$dir}->{$major}},
		    [$minor, $pkgname]);
	}
}

my $done_plist = {};

sub system_dirs
{
	return ("/usr", "/usr/X11R6");
}

sub add_libs_from_system
{
	my ($destdir) = @_;
	return if $done_plist->{'system'};
	$done_plist->{'system'} = 1;
	for my $dirname (system_dirs()) {
		opendir(my $dir, $destdir.$dirname."/lib") or next;
		while (my $d = readdir($dir)) {
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
	if (!defined $plist) {
		Warn "Can't read plist for $pkgname\n";
		return;
	}

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

sub _lookup_libspec
{
	my ($dir, $spec) = @_;
	my @r = ();

	if ($spec =~ m/^(.*)\.(\d+)\.(\d+)$/o) {
		my ($libname, $major, $minor) = ($1, $2, $3);
		my $exists = $registered_libs->{$libname}->{$dir}->{$major};
		if (defined $exists) {
			for my $e (@$exists) {
				if ($e->[0] >= $minor) {
					push(@r, $e->[1]);
				}
			}
		}
	}
	return @r;
}

sub lookup_libspec
{
	my ($base, $libspec) = @_;
		
	if ($libspec =~ m|(.*)/|o) {
		return _lookup_libspec("$base/$1", $');
	} else {
		return _lookup_libspec("$base/lib", $libspec);
	}
}

sub write_entry
{
	my ($name, $d, $M, $m) = @_;
	print "$name: found partial match in $d: major=$M, minor=$m ";
}

sub why_is_this_bad
{
	my ($name, $d1, $d2, $M1, $M2, $m1, $m2, $pkgname) = @_;
	if ($d1 ne $d2) {
		print "(bad directory)\n";
		return;
	}
	if ($M1 != $M2) {
		print "(bad major)\n";
		return;
	}
	if ($m1 < $m2) {
		print "(too small minor)\n";
		return;
	}
	print "($pkgname not reachable)\n";
}

sub _report_problem
{
	my ($dir, $name) = @_;
	if ($name =~ m/^(.*)\.(\d+)\.(\d+)$/o) {
		my ($stem, $major, $minor) = ($1, $2, $3, $4);
		return unless defined $registered_libs->{$stem};
		while (my ($d, $v) = each %{$registered_libs->{$stem}}) {
			while (my ($M, $w) = each %$v) {
				for my $e (@$w) {
					write_entry($name, $d, $M, $e->[0]);
					why_is_this_bad($name, $dir, $d, $major, $M, $minor, $e->[0], $e->[1]);
				}
			}
		}
	}
}

sub report_problem
{
	my ($base, $libspec) = @_;
		
	if ($libspec =~ m|(.*)/|o) {
		return _report_problem("$base/$1", $');
	} else {
		return _report_problem("$base/lib", $libspec);
	}
}
1;
