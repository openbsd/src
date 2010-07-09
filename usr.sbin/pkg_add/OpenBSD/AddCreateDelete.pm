# ex:ts=8 sw=4:
# $OpenBSD: AddCreateDelete.pm,v 1.11 2010/07/09 12:42:43 espie Exp $
#
# Copyright (c) 2007-2010 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

# common framework, let's place most everything in there

package OpenBSD::AddCreateDelete::State;
our @ISA = qw(OpenBSD::State);

use OpenBSD::State;
use OpenBSD::ProgressMeter;

sub init
{
	my $self = shift;

	$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	$self->{bad} = 0;
	$self->SUPER::init;
	$self->{export_level}++;
}

sub progress
{
	my $self = shift;
	return $self->{progressmeter};
}

sub not
{
	my $self = shift;
	return $self->{not};
}

sub _print
{
	my $self = shift;
	$self->progress->print(@_);
}

sub _errprint
{
	my $self = shift;
	$self->progress->errprint(@_);
}

sub handle_options
{
	my ($state, $opt_string, @usage) = @_;

	$state->SUPER::handle_options($opt_string.'mnx', @usage);

	$state->progress->setup($state->opt('x'), $state->opt('m'));
	$state->{not} = $state->opt('n');
}

package OpenBSD::AddCreateDelete;
use OpenBSD::Error;

sub handle_options
{
	my ($self, $opt_string, $state, @usage) = @_;
	$state->handle_options($opt_string, $self, @usage);
}

1;
