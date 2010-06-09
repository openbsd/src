# ex:ts=8 sw=4:
# $OpenBSD: AddCreateDelete.pm,v 1.3 2010/06/09 07:26:01 espie Exp $
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

use OpenBSD::Subst;
use OpenBSD::State;
use OpenBSD::ProgressMeter;

sub init
{
	my $self = shift;

	$self->{subst} = OpenBSD::Subst->new;
	$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	$self->{bad} = 0;
	$self->SUPER::init;
}

sub progress
{
	my $self = shift;
	return $self->{progressmeter};
}

sub verbose
{
	my $self = shift;
	return $self->{v};
}

sub not
{
	my $self = shift;
	return $self->{not};
}

sub opt
{
	my ($self, $k) = @_;
	return $self->{opt}{$k};
}

sub print
{
	my $self = shift;
	$self->progress->print($self->f(@_));
}

sub say
{
	my $self = shift;
	$self->progress->print($self->f(@_), "\n");
}

sub errprint
{
	my $self = shift;
	$self->progress->errprint($self->f(@_));
}

sub errsay
{
	my $self = shift;
	$self->progress->errprint($self->f(@_), "\n");
}

package OpenBSD::AddCreateDelete;
use OpenBSD::Getopt;
use OpenBSD::Error;

sub handle_options
{
	my ($self, $opt_string, $state, @usage) = @_;


	$state->{opt}{v} = 0;
	$state->{opt}{h} = sub { $state->usage; };
	$state->{opt}{D} = sub {
		$state->{subst}->parse_option(shift);
	};
	$state->usage_is(@usage);
	try {
		getopts('hmnvxD:'.$opt_string, $state->{opt});
	} catchall {
		$state->usage("#1", $_);
	};
	$state->progress->setup($state->opt('x'), $state->opt('m'));
	$state->{v} = $state->opt('v');
	$state->{not} = $state->opt('n');
}

1;
