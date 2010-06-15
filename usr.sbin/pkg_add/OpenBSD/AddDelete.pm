# ex:ts=8 sw=4:
# $OpenBSD: AddDelete.pm,v 1.26 2010/06/15 08:26:39 espie Exp $
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
package main;
our $not;

package OpenBSD::AddDelete;
use OpenBSD::Error;
use OpenBSD::Paths;
use OpenBSD::PackageInfo;
use OpenBSD::AddCreateDelete;

our @ISA = qw(OpenBSD::AddCreateDelete);

sub handle_options
{
	my ($self, $opt_string, $hash, $cmd, @usage) = @_;

	my $state = $self->new_state($cmd);
	$state->{opt} = $hash;
	$hash->{F} = sub {
		for my $o (split /\,/o, shift) {
			$state->{subst}->add($o, 1);
		}
	};
	$self->SUPER::handle_options($opt_string.'ciInqsB:F:',
	    $state, @usage);

	if ($state->opt('s')) {
		$state->{not} = 1;
	}
	# XXX RequiredBy
	$main::not = $state->{not};
	$state->{interactive} = $state->opt('i');
	$state->{localbase} = $state->opt('L') // OpenBSD::Paths->localbase;
	$state->{size_only} = $state->opt('s');
	$state->{quick} = $state->opt('q');
	$state->{extra} = $state->opt('c');
	$state->{dont_run_scripts} = $state->opt('I');
	$ENV{'PKG_DELETE_EXTRA'} = $state->{extra} ? "Yes" : "No";
	return $state;
}

sub do_the_main_work
{
	my ($self, $state) = @_;

	if ($state->{bad}) {
		exit(1);
	}

	my $handler = sub { $state->fatal("Caught SIG#1", shift); };
	local $SIG{'INT'} = $handler;
	local $SIG{'QUIT'} = $handler;
	local $SIG{'HUP'} = $handler;
	local $SIG{'KILL'} = $handler;
	local $SIG{'TERM'} = $handler;

	if ($state->defines('debug')) {
		$self->main($state);
	} else {
		eval { $self->main($state); };
	}
	my $dielater = $@;
	return $dielater;
}

sub framework
{
	my ($self, $state) = @_;
	try {
		lock_db($state->{not}) unless $state->defines('nolock');
		$state->check_root;
		$self->process_parameters($state);
		my $dielater = $self->do_the_main_work($state);
		# cleanup various things
		$state->{recorder}->cleanup($state);
		OpenBSD::PackingElement::Lib::ensure_ldconfig($state);
		OpenBSD::PackingElement::Fontdir::finish_fontdirs($state);
		$state->progress->clear;
		$state->log->dump;
		$self->finish_display($state);
		if ($state->verbose >= 2 || $state->{size_only} ||
		    $state->defines('tally')) {
			$state->vstat->tally;
		}
		# show any error, and show why we died...
		rethrow $dielater;
	} catch {
		print STDERR "$0: $_\n";
		OpenBSD::Handler->reset;
		if ($_ =~ m/^Caught SIG(\w+)/o) {
			kill $1, $$;
		}
		exit(1);
	};

	if ($state->{bad}) {
		exit(1);
	}
}

sub parse_and_run
{
	my ($self, $cmd) = @_;

	my $state = $self->handle_options($cmd);
	local $SIG{'INFO'} = sub { $state->status->print($state); };

	$self->framework($state);
}

package OpenBSD::SharedItemsRecorder;
sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub is_empty
{
	my $self = shift;
	return !(defined $self->{dirs} or defined $self->{users} or
		defined $self->{groups});
}

sub cleanup
{
	my ($self, $state) = @_;
	return if $self->is_empty or $state->{not};

	require OpenBSD::SharedItems;
	OpenBSD::SharedItems::cleanup($self, $state);
}

package OpenBSD::AddDelete::State;
use OpenBSD::Vstat;
use OpenBSD::Log;
our @ISA = qw(OpenBSD::AddCreateDelete::State);

sub init
{
	my $self = shift;
	$self->{l} = OpenBSD::Log->new($self);
	$self->{vstat} = OpenBSD::Vstat->new($self);
	$self->{status} = OpenBSD::Status->new;
	$self->{recorder} = OpenBSD::SharedItemsRecorder->new;
	$self->{v} = 0;
	$self->SUPER::init(@_);
}

sub ntogo
{
	my ($self, $offset) = @_;

	return $self->progress->ntogo($self, $offset);
}

sub ntogo_string
{
	my ($self, $offset) = @_;

	return $self->todo($offset // 0);
}

sub vstat
{
	my $self = shift;
	return $self->{vstat};
}

sub log
{
	my $self = shift;
	if (@_ == 0) {
		return $self->{l};
	} else {
		$self->{l}->say(@_);
	}
}

sub vsystem
{
	my $self = shift;
	my $verbose = $self;
	if ($self->verbose < 2) {
		$self->system(@_);
	} else {
		$self->print("Running #1", join(' ', @_));
		my $r = CORE::system(@_);
		if ($r != 0) {
			$self->say("... failed: #1", $self->child_error);
		} else {
			$self->say;
		}
	}
}

sub system
{
	my $self = shift;
	$self->progress->clear;
	$self->SUPER::system(@_);
}

sub check_root
{
	my $state = shift;
	if ($< && !$state->defines('nonroot')) {
		if ($state->{not}) {
			$state->errsay("#1 should be run as root", 
			    $state->{cmd}) if $state->verbose;
		} else {
			$state->fatal("#1 must be run as root", $state->{cmd});
		}
	}
}

sub choose_location
{
	my ($state, $name, $list, $is_quirks) = @_;
	if (@$list == 0) {
		$state->errsay("Can't find #1", $name) unless $is_quirks;
		return undef;
	} elsif (@$list == 1) {
		return $list->[0];
	}

	my %h = map {($_->name, $_)} @$list;
	if ($state->{interactive}) {
		require OpenBSD::Interactive;

		$h{'<None>'} = undef;
		$state->progress->clear;
		my $result = $state->ask_list("Ambiguous: choose package for $name", 1, sort keys %h);
		return $h{$result};
	} else {
		$state->errsay("Ambiguous: #1 could be #2", 
		    $name, join(' ', keys %h));
		return undef;
	}
}

sub confirm
{
	my ($state, $prompt, $default) = @_;

	return 0 if !$state->{interactive};
	require OpenBSD::Interactive;
	return OpenBSD::Interactive::confirm($prompt, $default);
}

sub ask_list
{
	my ($state, $prompt, $interactive, @values) = @_;

	require OpenBSD::Interactive;
	return OpenBSD::Interactive::ask_list($prompt, $interactive, @values);
}

sub status
{
	my $self = shift;

	return $self->{status};
}

sub defines
{
	my ($self, $k) = @_;
	return $self->{subst}->value($k);
}

# the object that gets displayed during status updates
package OpenBSD::Status;

sub print
{
	my ($self, $state) = @_;

	my $what = $self->{what};
	$what //= "Processing";
	my $object;
	if (defined $self->{object}) {
		$object = $self->{object};
	} elsif (defined $self->{set}) {
		$object = $self->{set}->print;
	} else {
		$object = "Parameters";
	}

	$state->say($what." #1 (#2)", $object, $state->ntogo_string);
	if ($state->defines('carp')) {
		require Carp;
		Carp::cluck("currently here");
	}
}

sub set
{
	my ($self, $set) = @_;
	delete $self->{object};
	$self->{set} = $set;
	return $self;
}

sub object
{
	my ($self, $object) = @_;
	delete $self->{set};
	$self->{object} = $object;
	return $self;
}

sub what
{
	my ($self, $what) = @_;
	$self->{what} = $what;
	return $self;
}

sub new
{
	my $class = shift;

	bless {}, $class;
}

1;
