# ex:ts=8 sw=4:
# $OpenBSD: AddCreateDelete.pm,v 1.33 2015/02/09 11:01:08 espie Exp $
#
# Copyright (c) 2007-2014 Marc Espie <espie@openbsd.org>
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
	$self->SUPER::init(@_);
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

sub sync_display
{
	my $self = shift;
	$self->progress->clear;
}

sub handle_options
{
	my ($state, $opt_string, @usage) = @_;

	my $i;
	$state->{opt}{i} //= sub {
		$i++;
	};
	$state->SUPER::handle_options($opt_string.'IiL:mnx', @usage);

	$state->progress->setup($state->opt('x'), $state->opt('m'), $state);
	$state->{not} = $state->opt('n');
	if ($state->opt('I')) {
		$i = 0;
	} elsif (!defined $i) {
		$i = -t STDIN;
	}
	if ($i) {
		require OpenBSD::Interactive;
		$state->{interactive} = OpenBSD::Interactive->new($state, $i);
	} else {
		$state->{interactive} = OpenBSD::InteractiveStub->new($state);
	}
}


sub is_interactive
{
	return shift->{interactive}->is_interactive;
}

sub confirm
{
	my $self = shift;
	return $self->{interactive}->confirm(@_);
}

sub ask_list
{
	my $self = shift;
	return $self->{interactive}->ask_list(@_);
}

sub vsystem
{
	my $self = shift;
	if ($self->verbose < 2) {
		$self->system(@_);
	} else {
		$self->verbose_system(@_);
	}
}

sub system
{
	my $self = shift;
	$self->SUPER::system(@_);
}

sub run_makewhatis
{
	my ($state, $opts, $l) = @_;
	my $braindead = sub { chdir('/'); };
	while (@$l > 1000) {
		my @b = splice(@$l, 0, 1000);
		$state->vsystem($braindead,
		    OpenBSD::Paths->makewhatis, @$opts, '--', @b);
	}
	$state->vsystem($braindead,
	    OpenBSD::Paths->makewhatis, @$opts, '--', @$l);
}

sub ntogo
{
	my ($self, $offset) = @_;

	return $self->{wantntogo} ?
	    $self->progress->ntogo($self, $offset) :
	    $self->f("ok");
}

sub ntogo_string
{
	my ($self, $offset) = @_;

	return $self->{wantntogo} ?
	    $self->f(" (#1)", $self->ntodo($offset // 0)) :
	    $self->f("");
}

OpenBSD::Auto::cache(signer_list,
	sub {
		my $self = shift;
		if ($self->defines('SIGNER')) {
			return [split /,/, $self->{subst}->value('SIGNER')];
		} else {
			if ($self->defines('FW_UPDATE')) {
				return [qr{^.*fw$}];
			} else {
				return [qr{^.*pkg$}];
			}
		}
	});

package OpenBSD::AddCreateDelete;
use OpenBSD::Error;

sub handle_options
{
	my ($self, $opt_string, $state, @usage) = @_;
	$state->handle_options($opt_string, $self, @usage);
}

package OpenBSD::InteractiveStub;
sub new
{
	my $class = shift;
	bless {}, $class;
}

sub ask_list
{
	my ($self, $prompt, @values) = @_;
	return $values[0];
}

sub confirm
{
	my ($self, $prompt, $yesno) = @_;
	return $yesno;
}

sub is_interactive
{
	return 0;
}
1;
