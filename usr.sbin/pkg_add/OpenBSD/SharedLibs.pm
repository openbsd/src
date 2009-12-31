# ex:ts=8 sw=4:
# $OpenBSD: SharedLibs.pm,v 1.44 2009/12/31 14:14:08 espie Exp $
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

use OpenBSD::Paths;
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

our $registered_libs = {};

sub register_lib
{
	my ($name, $pkgname) = @_;
	my ($stem, $major, $minor, $dir) = 
	    OpenBSD::PackingElement::Lib->parse($name);
	if (defined $stem) {
		push(@{$registered_libs->{$stem}->{$dir}->{$major}},
		    [$minor, $pkgname]);
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

sub normalize_dir_and_spec
{
	my ($base, $libspec) = @_;
	if ($libspec =~ m/^(.*)\/([^\/]+)$/o) {
		return ("$base/$1", $2);
	} else {
		return ("$base/lib", $libspec);
	}
}

sub parse_spec
{
	my $spec = shift;
	if ($spec =~ m/^(.*)\.(\d+)\.(\d+)$/o) {
		return ($1, $2, $3);
	} else {
		return undef;
	}
}

sub lookup_libspec
{
	my ($dir, $spec) = normalize_dir_and_spec(@_);
	my @r = ();
	my ($libname, $major, $minor) = parse_spec($spec);
	if (defined $libname) {
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

sub entry_string
{
	my ($stem, $M, $m) = @_;
	return "lib$stem.so.$M.$m";
}

sub why_is_this_bad
{
	my ($base, $name, $d1, $d2, $M1, $M2, $m1, $m2, $pkgname) = @_;
	if ($d1 ne $d2 && !($pkgname eq 'system' && $d1 eq "$base/lib")) {
		return "bad directory";
	}
	if ($M1 != $M2) {
		return "bad major";
	}
	if ($m1 > $m2) {
		return "minor not large enough";
	}
	return "$pkgname not reachable";
}

my $printed = {};

sub report_problem
{
	my ($state, $p) = @_;
	my $base = $state->{localbase};
	my ($dir, $name) = normalize_dir_and_spec($base, $p);
	my ($stem, $major, $minor) = parse_spec($name);

	my $r = "";
	if (!defined $stem) {
		$r = "| bad library specification\n";
	} elsif (!defined $registered_libs->{$stem}) {
		$r = "| not found anywhere\n";
	} else {
		while (my ($d, $v) = each %{$registered_libs->{$stem}}) {
			my @l = ();
			while (my ($M, $w) = each %$v) {
				for my $e (@$w) {
					push(@l, 
					    entry_string($stem, $M, $e->[0]).
					    " (".
					    why_is_this_bad($base, $name, $dir, 
						$d, $major, $M, $minor, 
						$e->[0], $e->[1]).
					    ")");
				}
			}
			if (@l > 0) {
				$r .= "| in $d: ". join(", ", sort @l). "\n";
			}
		}
	}
	if (!defined $printed->{$name} || $printed->{$name} ne $r) {
		$printed->{$name} = $r;
		$state->errsay("|library $name not found");
		$state->print($r);
	}
}

1;
