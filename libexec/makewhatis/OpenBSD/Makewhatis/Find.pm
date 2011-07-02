# ex:ts=8 sw=4:
# $OpenBSD: Find.pm,v 1.3 2011/07/02 12:47:49 espie Exp $
# Copyright (c) 2000-2004 Marc Espie <espie@openbsd.org>
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
package OpenBSD::Makewhatis::Find;

use File::Find;

# Find out all file names that correspond to existing stuff

sub equivalents
{
	my $_ = shift;
	my @l = ();
	s/(?:\.Z|\.gz)$//;
	push(@l, $_, "$_.Z", "$_.gz");
	if (s,/cat([\dln]\w*?)/(.*)\.0$,/man$1/$2.$1,) {
		push(@l, $_, "$_.Z", "$_.gz");
	} elsif (s,/man([\dln]\w*?)/(.*)\.\1$,/cat$1/$2.0,) {
		push(@l, $_, "$_.Z", "$_.gz");
	}
	return @l;
}

# $list = find_manpages($dir)
#
#   find all manpages under $dir, trim some duplicates.
#
sub find_manpages($)
{
	my $dir = shift;
	my $h = {};
	my $list=[];
	my $nodes = {};
	my $done = {};
	find(
	    sub {
		return unless m/\.[\dln]\w*(?:\.Z|\.gz)?$/;
		return unless -f $_;
		my $unique = join("/", (stat _)[0,1]);
		return if defined $nodes->{$unique};
		$nodes->{$unique} = 1;
		push @$list, $File::Find::name;
		$h->{$File::Find::name} = (stat _)[9];
	    }, $dir);
	for my $i (keys %$h) {
		next if $done->{$i};
		# only keep stuff that actually exists
		my @l = grep {defined $h->{$_}} equivalents($i);
		# don't do it twice
		$done->{$_} = 1 for @l;
		# find the most recent one
		@l = sort {$h->{$a} <=> $h->{$b}} @l;
		push @$list, pop @l;
	}
	return $list;
}

1;
