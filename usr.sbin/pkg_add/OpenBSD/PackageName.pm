# $OpenBSD: PackageName.pm,v 1.3 2003/11/06 17:47:25 espie Exp $
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
package OpenBSD::PackageName;

sub url2pkgname($)
{
	my $name = $_[0];
	$name =~ s|.*/||;
	$name =~ s|\.tgz$||;

	return $name;
}

sub new
{
	my ($class, $name) = @_;
	my $self = { name => $name };
# remove irrelevant filesystem info
	my $pkgname = url2pkgname($name);
	$self->{pkgname} = $pkgname;
# cut pkgname into pieces
	my @list = splitname($pkgname);
	$self->{stem} = $list[0];
	$self->{version} = $list[1];
	$self->{flavors} = [];
	push @{$self->{flavors}}, @list[2,];
	bless $self, $class;
}

# see package-specs(7)
sub splitname
{
	local $_ = shift;
	if (/\-(?=\d)/) {
		my $stem = $`;
		my $rest = $';
		my @all = split /\-/, $rest;
		return ($stem, @all);
	} else {
		return ($_);
	}
}

sub is_stem
{
	local $_ = shift;
	if (m/\-\d/) {
		return 0;
	} else {
		return 1;
	}
}

sub findstem
{
	my ($k, @list) = @_;
	my @r = ();
	for my $n (@list) {
		my $stem = (splitname $n)[0];
		if ($k eq $stem) {
			push(@r, $n);
		}
	}
	return @r;
}

# all the shit that does handle package specifications
sub compare_pseudo_numbers
{
	my ($n, $m) = @_;

	my ($n1, $m1);

	if ($n =~ m/^\d+/) {
		$n1 = $&;
		$n = $';
	}
	if ($m =~ m/^\d+/) {
		$m1 = $&;
		$m = $';
	}

	if ($n1 == $m1) {
		return $n cmp $m;
	} else {
		return $n1 <=> $m1;
	}
}


sub dewey_compare
{
	my ($a, $b) = @_;
	my ($pa, $pb);

	unless ($b =~ m/p\d+$/) { 		# does the Dewey hold a p<number> ?
		$a =~ s/p\d+$//; 	# No -> strip it from version.
	}

	return 0 if $a =~ /^$b$/; 	# bare equality

	if ($a =~ s/p(\d+)$//) {	# extract patchlevels
		$pa = $1;
	}
	if ($b =~ s/p(\d+)$//) {
		$pb = $1;
	}

	my @a = split(/\./, $a);
	push @a, $pa if defined $pa;	# ... and restore them
	my @b = split(/\\\./, $b);
	push @b, $pb if defined $pb;
	while (@a > 0 && @b > 0) {
		my $va = shift @a;
		my $vb = shift @b;
		next if $va eq $vb;
		return compare_pseudo_numbers($va, $vb);
	}
	if (@a > 0) {
		return 1;
	} else {
		return -1;
	}
}

sub check_version
{
	my ($v, $spec) = @_;
	local $_;

	# any version spec
	return 1 if $spec eq '.*';

	my @specs = split(/,/, $spec);
	for (grep /^\d/, @specs) { 		# exact number: check match
		return 1 if $v =~ /^$_$/;
		return 1 if $v =~ /^${_}p\d+$/; # allows for recent patches
	}

	# Last chance: dewey specs ?
	my @deweys = grep !/^\d/, @specs;		
	for (@deweys) {
		if (m/^\<\=|\>\=|\<|\>/) {
			my ($op, $dewey) = ($&, $');
			my $compare = dewey_compare($v, $dewey);
			return 0 if $op eq '<' && $compare >= 0;
			return 0 if $op eq '<=' && $compare > 0;
			return 0 if $op eq '>' && $compare <= 0;
			return 0 if $op eq '>=' && $compare < 0;
		} else {
			return 0;	# unknown spec type
		}
	}
	return @deweys == 0 ? 0 : 1;
}

sub check_1flavor
{
	my ($f, $spec) = @_;
	local $_;

	for (split /-/, $spec) {
		# must not be here
		if (m/^\!/) {
			return 0 if $f->{$'};
		# must be here
		} else {
			return 0 unless $f->{$_};
		}
	}
	return 1;
}

sub check_flavor
{
	my ($f, $spec) = @_;
	local $_;
	# no flavor constraints
	return 1 if $spec eq '';

	$spec =~ s/^-//;
	# retrieve all flavors
	my %f = map +($_, 1), split /\-/, $f;

	# check each flavor constraint
	for (split /,/, $spec) {
		if (check_1flavor(\%f, $_)) {
			return 1;
		}
	}
	return 0;
}

sub subpattern_match
{
	my ($p, $list) = @_;
	local $_;

	my ($stemspec, $vspec, $flavorspec);

	# first, handle special characters (shell -> perl)
	$p =~ s/\./\\\./g;
	$p =~ s/\+/\\\+/g;
	$p =~ s/\*/\.\*/g;
	$p =~ s/\?/\./g;

	# then, guess at where the version number is if any,
	
	# this finds patterns like -<=2.3,>=3.4.p1-
	# the only constraint is that the actual number 
	# - must start with a digit, 
	# - not contain - or ,
	if ($p =~ m/\-((?:\>|\>\=|\<|\<\=)?\d[^-]*)/) {
		($stemspec, $vspec, $flavorspec) = ($`, $1, $');
	# `any version' matcher
	} elsif ($p =~ m/\-(\.\*)/) {
		($stemspec, $vspec, $flavorspec) = ($`, $1, $');
	# okay, so no version marker. Assume no flavor spec.
	} else {
		($stemspec, $vspec, $flavorspec) = ($p, '', '');
	}

	$p = "$stemspec-\.\*" if $vspec ne '';

	# First trim down the list
	my @l = grep {/^$p$/} @$list;

	my @result = ();
	# Now, have to extract the version number, and the flavor...
	for (@l) {
		my ($stem, $v, $flavor);
		if (m/\-(\d[^-]*)/) {
			($stem, $v, $flavor) = ($`, $1, $');
			if ($stem =~ m/^$stemspec$/ &&
			    check_version($v, $vspec) &&
			    check_flavor($flavor, $flavorspec)) {
			    	push(@result, $_);
			}
	    	}
	}
		
	return @result;
}

sub pkgspec_match
{
	my ($pattern, @list) = @_;
	my @l = ();

	for my $subpattern (split /\|/, $pattern) {
		push(@l, subpattern_match($subpattern, \@list));
	}
	return @l;
}

1;
