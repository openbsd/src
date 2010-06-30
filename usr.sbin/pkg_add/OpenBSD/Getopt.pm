# ex:ts=8 sw=4:
# $OpenBSD: Getopt.pm,v 1.8 2010/06/30 10:41:42 espie Exp $
#
# Copyright (c) 2006 Marc Espie <espie@openbsd.org>
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
#
# This is inspired by Getopt::Std, except for the ability to invoke subs
# on options.

use strict;
use warnings;

package OpenBSD::Getopt;
require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(getopts);

sub handle_option
{
	my ($opt, $hash, $params) = @_;

	if (defined $hash->{$opt} and ref($hash->{$opt}) eq 'CODE') {
		&{$hash->{$opt}}($params);
	} else {
		no strict "refs";
		no strict "vars";

		if (defined $params) {
			${"opt_$opt"} = $params;
			$hash->{$opt} = $params;
		} else {
			${"opt_$opt"}++;
			$hash->{$opt}++;
		}
		push(@EXPORT, "\$opt_$opt");
	}
}

sub getopts($;$)
{
    my ($args, $hash) = @_;

    $hash = {} unless defined $hash;
    local @EXPORT;

    while ($_ = shift @ARGV) {
    	last if /^--$/o;
    	unless (m/^-(.)(.*)/so) {
		unshift @ARGV, $_;
		last;
	}
	my ($opt, $other) = ($1, $2);
	if ($args =~ m/\Q$opt\E(\:?)/) {
		if ($1 eq ':') {
			if ($other eq '') {
				die "no argument for option -$opt" unless @ARGV;
				$other = shift @ARGV;
			}
			handle_option($opt, $hash, $other);
		} else {
			handle_option($opt, $hash);
			if ($other ne '') {
				$_ = "-$other";
				redo;
			}
		}
	} else {
		die "Unknown option -$opt";
	}
    }
    local $Exporter::ExportLevel = 1;
    import OpenBSD::Getopt;
    return $hash;
}

1;
