# ex:ts=8 sw=4:
# $OpenBSD: SetList.pm,v 1.2 2014/04/14 12:37:00 espie Exp $
#
# Copyright (c) 2003-2014 Marc Espie <espie@openbsd.org>
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

# Provides an interface to the setlists of src/xenocara.
# requires a state object derived from OpenBSD::State, for printing out error
# this object should provide $state->build_tag('src', $set, $rev);
# and $state->process_entry for the actual walking.

package OpenBSD::SetList;
use strict;
use warnings;

my ($rev, $arch);
sub findos
{
	my $cmd = OpenBSD::Paths->uname." -mr";
	($rev, $arch) = split(/\s+/o, `$cmd`);
	chomp $arch;
	$rev =~ s/\.//;
}

sub walk
{
	my ($class, $state, $src) = @_;
	findos() if !defined $arch;
	my $dir = "$src/distrib/sets/lists";
	for my $set ($class->sets) {
		$state->build_tag($class->base_tag, $set, $rev);
		my $output = 0;
		for my $f ($class->files_for_set($dir, $set)) {
			open my $l, '<', $f or next;
			while (my $e = <$l>) {
				chomp $e;
				$e =~ s/^\.//;
				$state->process_entry($e);
				$output = 1;
			}
		}
		if (!$output) {
			$state->fatal("Couldn't find set #1", $set);
		}
	}
}

sub files_for_set
{
	my ($self, $dir, $set) = @_;
	return ("$dir/$set/mi", "$dir/$set/md.$arch");
}

package OpenBSD::SetList::Source;
our @ISA = qw(OpenBSD::SetList);
sub sets
{
	return (qw(base comp man etc game));
}

sub base_tag
{
	return 'src';
}

package OpenBSD::SetList::Xenocara;
our @ISA = qw(OpenBSD::SetList);
sub sets
{
	return (qw(xbase xetc xfont xserv xshare));
}

sub base_tag
{
	return 'xenocara';
}

sub files_for_set
{
	my ($self, $dir, $set) = @_;
	if ($set eq 'xfont') {
		return ("$dir/$set/mi", "$dir/$set/md.x11r7");
	} else {
		return $self->SUPER::files_for_set($dir, $set);
	}
}

1;
