# ex:ts=8 sw=4:
# $OpenBSD: Interactive.pm,v 1.9 2007/06/04 14:40:39 espie Exp $
#
# Copyright (c) 2005-2007 Marc Espie <espie@openbsd.org>
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
#

use strict;
use warnings;

package OpenBSD::Interactive;

sub ask_list
{
	my ($prompt, $interactive, @values) = @_;
	if (!$interactive || !-t STDIN) {
		return $values[0];
	}
	print STDERR $prompt, "\n";
	my $i = 0;
	for my $v (@values) {
		printf STDERR "\t%2d: %s\n", $i, $v;
		$i++;
	}
LOOP:
	print STDERR "Your choice: ";
	my $result = <STDIN>;
	unless (defined $result) {
		print STDERR "\n";
		return $values[0];
	}
	chomp $result;
	if ($result eq '') {
		return $values[0];
	}
	if ($result =~ m/^\d+$/o) {
		if ($result >= 0 && $result < @values) {
			return $values[$result];
		}
		print STDERR "invalid numeric value !\n";
		goto LOOP;
	}
	if (grep { $result eq $_ } @values) {
		return $result;
	} else {
		print STDERR "Ambiguous value !\n";
		goto LOOP;
	}
}

my $always = {};

sub confirm
{
	my ($prompt, $interactive, $default, $key) = @_;
	if (!$interactive || !-t STDIN) {
		return 0;
	}
	if (defined $key && $always->{$key}) {
		return 1;
	}
LOOP2:
	my $a = defined $key ? '/a' : '';
	print STDERR $prompt, $default ? "? [Y/n$a] " : "? [y/N$a] ";

	my $result = <STDIN>;
	unless(defined $result) {
		print STDERR "\n";
		return $default;
	}
	chomp $result;
	$result =~ s/\s+//go;
	$result =~ tr/A-Z/a-z/;
	if ($result eq 'yes' or $result eq 'y') {
		return 1;
	}
	if ($result eq 'no' or $result eq 'n') {
		return 0;
	}
	if (defined $key && $result eq 'a') {
		$always->{$key} = 1;
		return 1;
	}
	if ($result eq '') {
		return $default;
	}
	print STDERR "Ambiguous answer\n";
	goto LOOP2;
}

sub choose1
{
	my ($pkgname, $interactive, @l) = @_;
	if (@l == 0) {
	    print "Can't resolve $pkgname\n";
	} elsif (@l == 1) {
		return $l[0];
	} elsif (@l != 0) {
		if ($interactive) {
		    my $result = ask_list("Ambiguous: choose package for $pkgname", 1, ("<None>", @l));
		    if ($result ne '<None>') {
			return $result;
		    }
		} else {
		    print "Ambiguous: $pkgname could be ", join(' ', @l),"\n";
		}
	}
	return;
}

1;
