# ex:ts=8 sw=4:
# $OpenBSD: IdCache.pm,v 1.7 2010/06/30 10:41:42 espie Exp $
#
# Copyright (c) 2002-2005 Marc Espie <espie@openbsd.org>
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

package OpenBSD::SimpleIdCache;
sub new
{
	my $class = shift;
	bless {}, $class;
}

sub lookup
{
	my ($self, $name, $default) = @_;
	my $r;

	if (defined $self->{$name}) {
		$r = $self->{$name};
	} else {
		$r = $self->convert($name);
		if (!defined $r) {
			$r = $default;
		}
		$self->{$name} = $r;
	}
	return $r;
}


package OpenBSD::IdCache;
our @ISA=qw(OpenBSD::SimpleIdCache);

sub lookup
{
	my ($self, $name, $default) = @_;

	if ($name =~ m/^\d+$/o) {
		return $name;
	} else {
		return $self->SUPER::lookup($name, $default);
	}
}

package OpenBSD::UidCache;
our @ISA=qw(OpenBSD::IdCache);

sub convert
{
	my @entry = getpwnam($_[1]);
	return @entry == 0 ? undef : $entry[2];
}

package OpenBSD::GidCache;
our @ISA=qw(OpenBSD::IdCache);

sub convert
{
	my @entry = getgrnam($_[1]);
	return @entry == 0 ? undef : $entry[2];
}

package OpenBSD::UnameCache;
our @ISA=qw(OpenBSD::SimpleIdCache);

sub convert
{
	return getpwuid($_[1]);
}

package OpenBSD::GnameCache;
our @ISA=qw(OpenBSD::SimpleIdCache);

sub convert
{
	return getgrgid($_[1]);
}

1;
