# ex:ts=8 sw=4:
# $OpenBSD: Subject.pm,v 1.2 2011/02/22 00:23:14 espie Exp $
# Copyright (c) 2010 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Makewhatis::SubjectHandler;

sub new
{
	my ($class, $p) = @_;
	return bless { p => $p}, $class;
}

sub add
{
}

sub p
{
	my $h = shift;
	return $h->{p};
}

sub set_filename
{
	my ($h, $name) = @_;
	$h->{current} = $name;
	$h->{has_subjects} = 0;
}

sub filename
{
	my $h = shift;
	return $h->{current};
}

sub errsay
{
	my $h = shift;
	if ($h->p->verbose) {
		push(@_, $h->filename);
		$h->p->errsay(@_);
	}
}

sub weird_subject
{
	my ($h, $line) = @_;
	$h->errsay("Weird subject line in #2:\n#1", $line) ;
}

sub cant_find_subject
{
	my $h = shift;
	$h->errsay("No subject found in #1");
}

package OpenBSD::MakeWhatis::Subject;

sub new
{
}

1;
