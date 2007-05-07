# ex:ts=8 sw=4:
# $OpenBSD: SharedLibs.pm,v 1.12 2007/05/07 08:14:51 espie Exp $
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
			if (m/^\s*search directories:\s*(.*?)\s*$/) {
				for my $d (split(':', $1)) {
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
	if ($name =~ m/^(.*\/lib.*?\.so\.\d+)\.(\d+)$/) {
		my ($stem, $minor) = ($1, $2);
		$registered_libs->{"$stem"} = [] 
		    unless defined $registered_libs->{"$stem"};
		push(@{$registered_libs->{"$stem"}}, [$minor, $pkgname]);
	}
}

my $done_plist = {};

sub add_system_libs
{
	my ($destdir) = @_;
	return if $done_plist->{'system'};
	$done_plist->{'system'} = 1;
	for my $dirname ("/usr/lib", "/usr/X11R6/lib") {
		opendir(my $dir, $destdir.$dirname) or next;
		while (my $d = readdir($dir)) {
			register_lib("$dirname/$d", 'system');
		}
		closedir($dir);
	}
}

sub add_package_libs
{
	my ($pkgname, $wantpath) = @_;
	return if $done_plist->{$pkgname};
	$done_plist->{$pkgname} = 1;
	my $plist = OpenBSD::PackingList->from_installation($pkgname, 
	    \&OpenBSD::PackingList::LibraryOnly);
	if (!defined $plist) {
		Warn "Can't read plist for $pkgname\n";
		return;
	}
	if (defined $wantpath) {
		if (defined $plist->{extrainfo}) {
			$pkgname = $plist->{extrainfo}->{subdir};
		}
	}

	$plist->mark_available_lib($pkgname);
}

sub add_plist_libs
{
	my ($plist) = @_;
	my $pkgname = $plist->pkgname;
	return if $done_plist->{$pkgname};
	$done_plist->{$pkgname} = 1;
	$plist->mark_available_lib($pkgname);
}

sub _lookup_libspec
{
	my ($dir, $spec) = @_;
	my @r = ();

	if ($spec =~ m/^(.*)\.(\d+)\.(\d+)$/) {
		my ($libname, $major, $minor) = ($1, $2, $3);
		my $exists = $registered_libs->{"$dir/lib$libname.so.$major"};
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
	my ($base, $libspec, $wantpath) = @_;
		
	if ($libspec =~ m|(.*)/|) {
		return _lookup_libspec("$base/$1", $');
	} else {
		return _lookup_libspec("$base/lib", $libspec);
	}
}

1;
