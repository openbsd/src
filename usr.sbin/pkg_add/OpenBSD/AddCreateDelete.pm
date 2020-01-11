# ex:ts=8 sw=4:
# $OpenBSD: AddCreateDelete.pm,v 1.48 2020/01/11 13:46:39 espie Exp $
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

sub add_interactive_options
{
	my $self = shift;
	$self->{has_interactive_options} = 1;
	return $self;
}

sub handle_options
{
	my ($state, $opt_string, @usage) = @_;

	my $i;

	if ($state->{has_interactive_options}) {
		$opt_string .= 'iI';
		$state->{opt}{i} = sub {
			$i++;
		};
	};

	$state->SUPER::handle_options($opt_string.'L:mnx', @usage);

	$state->progress->setup($state->opt('x'), $state->opt('m'), $state);
	$state->{not} = $state->opt('n');
	if ($state->{has_interactive_options}) {
		if ($state->opt('I')) {
			$i = 0;
		} elsif (!defined $i) {
			$i = -t STDIN;
		}
	}
	if ($i) {
		require OpenBSD::Interactive;
		$state->{interactive} = OpenBSD::Interactive->new($state, $i);
	}
	$state->{interactive} //= OpenBSD::InteractiveStub->new($state);
}


sub is_interactive
{
	return shift->{interactive}->is_interactive;
}

sub find_window_size
{
	my ($state, $cont) = @_;
	$state->SUPER::find_window_size;
	$state->{progressmeter}->compute_playfield($cont);
}

sub confirm_defaults_to_no
{
	my $self = shift;
	return $self->{interactive}->confirm($self->f(@_), 0);
}

sub confirm_defaults_to_yes
{
	my $self = shift;
	return $self->{interactive}->confirm($self->f(@_), 1);
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

sub solve_dependency
{
	my ($self, $solver, $dep, $package) = @_;
	# full dependency solving with everything
	return $solver->really_solve_dependency($self, $dep, $package);
}

package OpenBSD::AddCreateDelete;
use OpenBSD::Error;

sub handle_options
{
	my ($self, $opt_string, $state, @usage) = @_;
	$state->handle_options($opt_string, $self, @usage);
}

sub try_and_run_command
{
	my ($self, $state) = @_;
	if ($state->defines('pkg-debug')) {
		$self->run_command($state);
	} else {
		try {
			$self->run_command($state);
		} catch {
			$state->errsay("#1: #2", $state->{cmd}, $_);
			OpenBSD::Handler->reset;
			if ($_ =~ m/^Caught SIG(\w+)/o) {
				kill $1, $$;
			}
			$state->{bad}++;
		};
	}
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
