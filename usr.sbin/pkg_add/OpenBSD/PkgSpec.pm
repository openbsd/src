# ex:ts=8 sw=4:
# $OpenBSD: PkgSpec.pm,v 1.19 2009/03/07 12:07:37 espie Exp $
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
package OpenBSD::PkgSpec;
use OpenBSD::PackageName;

sub check_version
{
	my ($v, $constraint_list) = @_;

	for my $c (@$constraint_list) {
		return 0 if $c->match($v) == 0;
	}
	return 1;
}

sub check_1flavor
{
	my ($f, $spec) = @_;

	for my $_ (split /\-/o, $spec) {
		# must not be here
		if (m/^\!(.*)$/o) {
			return 0 if $f->{$1};
		# must be here
		} else {
			return 0 unless $f->{$_};
		}
	}
	return 1;
}

sub check_flavor
{
	my ($h, $spec) = @_;
	# no flavor constraints
	return 1 if $spec eq '';

	$spec =~ s/^-//o;

	# check each flavor constraint
	for my $_ (split /\,/o, $spec) {
		if (check_1flavor($h, $_)) {
			return 1;
		}
	}
	return 0;
}

sub subpattern_match
{
	my ($p, $list) = @_;

	# let's try really hard to find the stem and the flavors
	unless ($p =~ m/^(.*?)\-((?:(?:\>|\>\=|\<\=|\<|\=)?\d|\*)[^-]*)(.*)$/) {
		die "Invalid spec $p";
	}

	my ($stemspec, $vspec, $flavorspec) = ($1, $2, $3);

	$stemspec =~ s/\./\\\./go;
	$stemspec =~ s/\+/\\\+/go;
	$stemspec =~ s/\*/\.\*/go;
	$stemspec =~ s/\?/\./go;
	$stemspec =~ s/^(\\\.libs)\-/$1\\d*\-/go;

	# First trim down the list
	my @l = grep {/^$stemspec-.*$/} @$list;

	# turn the vspec into a list of constraints.
	my @constraints = ();
	if ($vspec eq '*') {
		# non constraint
	} else {
		for my $c (split /\,/, $vspec) {
			push(@constraints, 
			    OpenBSD::PackageName::versionspec->from_string($c));
		}
	}

	my @result = ();
	# Now, have to extract the version number, and the flavor...
	for my $s (@l) {
		my $name = OpenBSD::PackageName->from_string($s);
		if ($name->{stem} =~ m/^$stemspec$/ &&
			check_flavor($name->{flavors}, $flavorspec) &&
			check_version($name->{version}, \@constraints)) {
			    	push(@result, $s);
			}
	}
		
	return @result;
}

1;
