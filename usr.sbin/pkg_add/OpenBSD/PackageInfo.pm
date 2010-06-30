# ex:ts=8 sw=4:
# $OpenBSD: PackageInfo.pm,v 1.48 2010/06/30 10:33:09 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageInfo;
require Exporter;
our @ISA=qw(Exporter);
our @EXPORT=qw(installed_packages installed_info installed_name info_names is_info_name installed_stems
    lock_db unlock_db
    add_installed delete_installed is_installed borked_package CONTENTS COMMENT DESC INSTALL DEINSTALL REQUIRE
    REQUIRED_BY REQUIRING DISPLAY UNDISPLAY MTREE_DIRS);

use OpenBSD::PackageName;
use OpenBSD::Paths;
use constant {
	CONTENTS => '+CONTENTS',
	COMMENT => '+COMMENT',
	DESC => '+DESC',
	INSTALL => '+INSTALL',
	DEINSTALL => '+DEINSTALL',
	REQUIRE => '+REQUIRE',
	REQUIRED_BY => '+REQUIRED_BY',
	REQUIRING => '+REQUIRING',
	DISPLAY => '+DISPLAY',
	UNDISPLAY => '+UNDISPLAY',
	MTREE_DIRS => '+MTREE_DIRS' };

use Fcntl qw/:flock/;
my $pkg_db = $ENV{"PKG_DBDIR"} || OpenBSD::Paths->pkgdb;

my ($list, $stemlist);

our @info = (CONTENTS, COMMENT, DESC, REQUIRE, INSTALL, DEINSTALL, REQUIRED_BY, REQUIRING, DISPLAY, UNDISPLAY, MTREE_DIRS);

our %info = ();
for my $i (@info) {
	my $j = $i;
	$j =~ s/\+/F/o;
	$info{$i} = $j;
}

sub _init_list
{
	$list = {};
	$stemlist = OpenBSD::PackageName::compile_stemlist();

	opendir(my $dir, $pkg_db) or die "Bad pkg_db: $!";
	while (my $e = readdir($dir)) {
		next if $e eq '.' or $e eq '..';
		add_installed($e);
	}
	close($dir);
}

sub add_installed
{
	if (!defined $list) {
		_init_list();
	}
	for my $p (@_) {
		$list->{$p} = 1;
		$stemlist->add($p);
	}
}

sub delete_installed
{
	if (!defined $list) {
		_init_list();
	}
	for my $p (@_) {
		delete $list->{$p};
		$stemlist->delete($p);

	}
}

sub installed_stems
{
	if (!defined $list) {
		_init_list();
	}
	return $stemlist;
}

sub installed_packages
{
	if (!defined $list) {
		_init_list();
	}
	if ($_[0]) {
		return grep { !/^\./o } keys %$list;
	} else {
		return keys %$list;
	}
}

sub installed_info
{
	my $name =  shift;

	# XXX remove the o if we allow pkg_db to change dynamically
	if ($name =~ m|^\Q$pkg_db\E/?|o) {
		return "$name/";
	} else {
		return "$pkg_db/$name/";
	}
}

sub installed_contents
{
	return installed_info(shift).CONTENTS;
}

sub borked_package
{
	my $pkgname = shift;
	$pkgname = "partial-$pkgname" unless $pkgname =~ m/^partial\-/;
	unless (-e "$pkg_db/$pkgname") {
		return $pkgname;
	}
	my $i = 1;

	while (-e "$pkg_db/$pkgname.$i") {
		$i++;
	}
	return "$pkgname.$i";
}

sub libs_package
{
	my $pkgname = shift;
	$pkgname =~ s/^\.libs\d*\-//;
	unless (-e "$pkg_db/.libs-$pkgname") {
		return ".libs-$pkgname";
	}
	my $i = 1;

	while (-e "$pkg_db/.libs$i-$pkgname") {
		$i++;
	}
	return ".libs$i-$pkgname";
}

sub is_installed
{
	my $name = installed_name(shift);
	if (!defined $list) {
		installed_packages();
	}
	return defined $list->{$name};
}

sub installed_name
{
	require File::Spec;
	my $name = File::Spec->canonpath(shift);
	$name =~ s|/$||o;
	# XXX remove the o if we allow pkg_db to change dynamically
	$name =~ s|^\Q$pkg_db\E/?||o;
	$name =~ s|/\+CONTENTS$||o;
	return $name;
}

sub info_names()
{
	return @info;
}

sub is_info_name
{
	my $name = shift;
	return $info{$name};
}

my $dlock;

sub lock_db($;$)
{
	my ($shared, $quiet) = @_;
	my $mode = $shared ? LOCK_SH : LOCK_EX;
	open($dlock, '<', $pkg_db) or return;
	if (flock($dlock, $mode | LOCK_NB)) {
		return;
	}
	print STDERR "Package database already locked... awaiting release... "
		unless $quiet;
	while (!flock($dlock, $mode)) {
	}
	print STDERR "done!\n" unless $quiet;
	return;
}

sub unlock_db()
{
	if (defined $dlock) {
		flock($dlock, LOCK_UN);
		close($dlock);
	}
}


sub solve_installed_names
{
	my ($old, $new, $msg, $state) = @_;

	my $bad = 0;
	my $seen = {};

	for my $pkgname (@$old) {
	    $pkgname =~ s/\.tgz$//o;
	    if (is_installed($pkgname)) {
	    	if (!$seen->{$pkgname}) {
		    $seen->{$pkgname} = 1;
		    push(@$new, installed_name($pkgname));
		}
	    } else {
		if (OpenBSD::PackageName::is_stem($pkgname)) {
		    require OpenBSD::Search;

		    my $r = $state->repo->installed->match_locations(OpenBSD::Search::Stem->new($pkgname));
		    if (@$r == 0) {
			print "Can't resolve $pkgname to an installed package name\n";
			$bad = 1;
		    } elsif (@$r == 1) {
			if (!$seen->{$r->[0]}) {
			    $seen->{$r->[0]} = 1;
			    push(@$new, $r->[0]->name);
			}
		    } else {
		    	# try to see if we already solved the ambiguity
			my $found = 0;
			for my $p (@$r) {
			    if ($seen->{$p}) {
				$found = 1;
				last;
			    }
			}
			next if $found;

			if ($state->defines('ambiguous')) {
			    my @l = map {$_->name} @$r;
			    $state->say("Ambiguous: #1 could be #2",
				$pkgname, join(' ', @l));
			    $state->say($msg);
			    push(@$new, @l);
			    for my $p (@$r) {
			    	$seen->{$p} = 1;
			    }
			} else {
			    my $result = $state->choose_location($pkgname, $r);
			    if (defined $result) {
			    	push(@$new, $result->name);
				$seen->{$result} = 1;
			    } else {
				$bad = 1;
			    }
			}
		    }
		}
	    }
    	}
	return $bad;
}

1;
