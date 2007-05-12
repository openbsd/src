# ex:ts=8 sw=4:
# $OpenBSD: PkgSpec.pm,v 1.8 2007/05/12 22:55:29 espie Exp $
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
package OpenBSD::PkgSpec;

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


	# then, guess at where the version number is if any,
	
	# this finds patterns like -<=2.3,>=3.4.p1-
	# the only constraint is that the actual number 
	# - must start with a digit, 
	# - not contain - or ,
	if ($p =~ m/\-((?:\>|\>\=|\<|\<\=)?\d[^-]*)/) {
		($stemspec, $vspec, $flavorspec) = ($`, $1, $');
	# `any version' matcher
	} elsif ($p =~ m/\-\*/) {
		($stemspec, $vspec, $flavorspec) = ($`, '*', $');
	# okay, so no version marker. Assume no flavor spec.
	} else {
		($stemspec, $vspec, $flavorspec) = ($p, '', '');
	}

	$stemspec =~ s/\./\\\./g;
	$stemspec =~ s/\+/\\\+/g;
	$stemspec =~ s/\*/\.\*/g;
	$stemspec =~ s/\?/\./g;
	$vspec =~ s/\./\\\./g;
	$vspec =~ s/\+/\\\+/g;
	$vspec =~ s/\*/\.\*/g;
	$vspec =~ s/\?/\./g;

	$p = $stemspec;
	$p.="-.*" if $vspec ne '';

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
	    	} else {
			if ($vspec eq '') {
				push(@result, $_);
			}
		}
	}
		
	return @result;
}

sub match_ref
{
	my ($self, $r) = @_;
	my @l = ();

	for my $subpattern (@{$self->{patterns}}) {
		push(@l, subpattern_match($subpattern, $r));
	}
	return @l;
}

sub match
{
	my ($self, @list) = @_;
	return $self->match_ref(\@list);
}

sub new
{
	my ($class, $pattern) = @_;
	my @l = split /\|/, $pattern;
	bless { patterns => \@l }, $class;
}

1;
