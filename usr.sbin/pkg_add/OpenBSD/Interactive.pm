# ex:ts=8 sw=4:
# $OpenBSD: Interactive.pm,v 1.17 2010/12/24 09:04:14 espie Exp $
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

use strict;
use warnings;

package OpenBSD::Interactive;

my $always = 0;

sub ask_list
{
	my ($prompt, $interactive, @values) = @_;
	if (!$interactive || !-t STDIN || $always) {
		return $values[0];
	}
	print STDERR $prompt, "\n";
	my $i = 0;
	for my $v (@values) {
		printf STDERR "%s\t%2d: %s\n", $i == 0 ? " a" : "" , $i, $v;
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
	if ($result eq 'a') {
		$always = 1;
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

sub confirm
{
	my ($prompt, $default) = @_;
	if (!-t STDIN) {
		return 0;
	}
	if ($always) {
		return 1;
	}
LOOP2:
	print STDERR $prompt, $default ? "? [Y/n/a] " : "? [y/N/a] ";

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
	if ($result eq 'a') {
		$always = 1;
		return 1;
	}
	if ($result eq '') {
		return $default;
	}
	print STDERR "Ambiguous answer\n";
	goto LOOP2;
}

1;
