# $OpenBSD: PackageInfo.pm,v 1.4 2003/12/09 19:22:36 espie Exp $
#
# Copyright (c) 2003 Marc Espie.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
# PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;
package OpenBSD::PackageInfo;
our @ISA=qw(Exporter);
our @EXPORT=qw(installed_packages installed_info installed_name info_names is_info_name 
    add_installed delete_installed is_installed borked_package CONTENTS COMMENT DESC INSTALL DEINSTALL REQUIRE 
    REQUIRED_BY DISPLAY MTREE_DIRS);

use OpenBSD::PackageName;
use constant {
	CONTENTS => '+CONTENTS',
	COMMENT => '+COMMENT',
	DESC => '+DESC',
	INSTALL => '+INSTALL',
	DEINSTALL => '+DEINSTALL',
	REQUIRE => '+REQUIRE',
	REQUIRED_BY => '+REQUIRED_BY',
	DISPLAY => '+DISPLAY',
	MTREE_DIRS => '+MTREE_DIRS' };

my $pkg_db = $ENV{"PKG_DBDIR"} || '/var/db/pkg';

our $list;

our @info = (CONTENTS, COMMENT, DESC, INSTALL, DEINSTALL, REQUIRE, REQUIRED_BY, DISPLAY, MTREE_DIRS);

our %info = ();
for my $i (@info) {
	my $j = $i;
	$j =~ s/\+/F/;
	$info{$i} = $j;
}

sub add_installed
{
	if (!defined $list) {
		installed_packages();
	}
	for my $p (@_) {
		$list->{$p} = 1;
	}
}

sub delete_installed
{
	if (!defined $list) {
		installed_packages();
	}
	for my $p (@_) {
		undef $list->{$p};

	}
}

sub installed_packages()
{
	if (!defined $list) {
		$list = {};

		opendir(my $dir, $pkg_db) or die "Bad pkg_db";
		while (my $e = readdir($dir)) {
			next if $e eq '.' or $e eq '..';
			next unless -d "$pkg_db/$e";
			if (-f "$pkg_db/$e/+CONTENTS") {
				$list->{$e} = 1;
			} else {
				print "Warning: $e is not really a package";
			}
		}
		close($dir);
	}
	return keys %$list;
}

sub installed_info($)
{
	my $name =  shift;

	if ($name =~ m|^\Q$pkg_db\E/?|) {
		return "$name/";
	} else {
		return "$pkg_db/$name/";
	}
}

sub borked_package()
{
	my $i = 1;

	while (-e "$pkg_db/borked.$i") {
		$i++;
	}
	return "borked.$i";
}

sub is_installed($)
{
	my $name = installed_name(shift);
	if (!defined $list) {
		installed_packages();
	}
	return defined $list->{$name};
}

sub installed_name($)
{
	my $name = shift;
	$name =~ s|/$||;
	$name =~ s|^\Q$pkg_db\E/?||;
	return $name;
}

sub info_names()
{
	return @info;
}

sub is_info_name($)
{
	my $name = shift;
	return $info{$name};
}

1;
