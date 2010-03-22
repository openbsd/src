# ex:ts=8 sw=4:
# $OpenBSD: AddDelete.pm,v 1.19 2010/03/22 20:38:44 espie Exp $
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
use OpenBSD::Getopt;
use OpenBSD::Error;
use OpenBSD::Paths;
use OpenBSD::ProgressMeter;

our $bad = 0;
our %defines = ();
our $state;

our ($opt_n, $opt_x, $opt_v, $opt_B, $opt_L, $opt_i, $opt_q, $opt_c, $opt_I, $opt_s);
$opt_v = 0;

sub handle_options
{
	my ($opt_string, $hash, @usage) = @_;

	set_usage(@usage);
	$state = OpenBSD::State->new;
	$hash->{h} = sub { Usage(); };
	$hash->{F} = sub { 
		for my $o (split /\,/o, shift) { 
			$defines{$o} = 1;
		}
	};
	$hash->{D} = sub {
		my $_ = shift;
		if (m/^(.*?)=(.*)/) {
			$defines{$1} = $2;
		} else {
			$defines{$_} = 1;
		}
	};
	try {
		getopts('hciInqvsxB:D:F:L:'.$opt_string, $hash);
	} catchall {
		Usage($_);
	};

	$opt_L = OpenBSD::Paths->localbase unless defined $opt_L;

	if ($opt_s) {
		$opt_n = 1;
	}
	$state->{not} = $opt_n;
	# XXX RequiredBy
	$main::not = $opt_n;
	$state->{defines} = \%defines;
	$state->{interactive} = $opt_i;
	$state->{v} = $opt_v;
	$state->{localbase} = $opt_L;
	$state->{size_only} = $opt_s;
	$state->{quick} = $opt_q;
	$state->{extra} = $opt_c;
	$state->{dont_run_scripts} = $opt_I;
}

sub do_the_main_work
{
	my $code = shift;

	if ($bad) {
		exit(1);
	}

	my $handler = sub { my $sig = shift; Fatal "Caught SIG$sig"; };
	local $SIG{'INT'} = $handler;
	local $SIG{'QUIT'} = $handler;
	local $SIG{'HUP'} = $handler;
	local $SIG{'KILL'} = $handler;
	local $SIG{'TERM'} = $handler;

	if ($state->{defines}->{debug}) {
		&$code;
	} else { 
		eval { &$code; };
	}
	my $dielater = $@;
	return $dielater;
}

sub framework
{
	my $code = shift;
	try {
		lock_db($opt_n) unless $state->{defines}->{nolock};
		$state->progress->setup($opt_x);
		$state->check_root;
		process_parameters();
		my $dielater = do_the_main_work($code);
		# cleanup various things
		$state->{recorder}->cleanup($state);
		OpenBSD::PackingElement::Lib::ensure_ldconfig($state);
		OpenBSD::PackingElement::Fontdir::finish_fontdirs($state);
		$state->progress->clear;
		$state->log->dump;
		finish_display();
		if ($state->verbose >= 2 || $opt_s || 
		    $state->{defines}->{tally}) {
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

	if ($bad) {
		exit(1);
	}
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

package OpenBSD::Log;
use OpenBSD::Error;
our @ISA = qw(OpenBSD::Error);

sub set_context
{
	&OpenBSD::Error::set_pkgname;
}

sub dump
{
	&OpenBSD::Error::delayed_output;
}


package OpenBSD::UI;
use OpenBSD::Error;
use OpenBSD::Vstat;

sub new
{
	my $class = shift;
	my $o = bless {}, $class;
	$o->init(@_);
	return $o;
}

sub init
{
	my $self = shift;
	$self->{l} = OpenBSD::Log->new;
	$self->{vstat} = OpenBSD::Vstat->new($self);
	$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	$self->{status} = OpenBSD::Status->new;
	$self->{recorder} = OpenBSD::SharedItemsRecorder->new;
	$self->{v} = 0;
}

sub ntogo
{
	my ($self, $offset) = @_;

	return $self->progress->ntogo($self->todo, $offset);
}

sub ntogo_string
{
	my ($self, $todo, $offset) = @_;

	$todo //= 0;
	$offset //= 0;
	$todo += $offset;

	if ($todo > 0) {
		return " ($todo to go)";
	} else {
		return "";
	}
}

sub verbose
{
	return shift->{v};
}

sub vstat
{
	return shift->{vstat};
}

sub log
{
	my $self = shift;
	if (@_ == 0) {
		return $self->{l};
	} else {
		$self->{l}->print(@_);
	}
}

sub print
{
	my $self = shift;
	$self->progress->print(@_);
}

sub say
{
	my $self = shift;
	$self->progress->print(@_, "\n");
}

sub errprint
{
	my $self = shift;
	$self->progress->errprint(@_);
}

sub errsay
{
	my $self = shift;
	$self->progress->errprint(@_, "\n");
}

sub progress
{
	my $self = shift;
	return $self->{progressmeter};
}

sub vsystem
{
	my $self = shift;
	$self->progress->clear;
	OpenBSD::Error::VSystem($self->verbose >= 2, @_);
}

sub system
{
	my $self = shift;
	$self->progress->clear;
	OpenBSD::Error::System(@_);
}

sub unlink
{
	my $self = shift;
	$self->progress->clear;
	OpenBSD::Error::Unlink(@_);
}

sub check_root
{
	my $state = shift;
	if ($< && !$state->{defines}->{nonroot}) {
		if ($state->{not}) {
			$state->errsay("$0 should be run as root") if $state->verbose;
		} else {
			Fatal "$0 must be run as root";
		}
	}
}

sub choose_location
{
	my ($state, $name, $list, $is_quirks) = @_;
	if (@$list == 0) {
		$state->errsay("Can't find $name") unless $is_quirks;
		return undef;
	} elsif (@$list == 1) {
		return $list->[0];
	}

	my %h = map {($_->name, $_)} @$list;
	if ($state->{interactive}) {
		require OpenBSD::Interactive;

		$h{'<None>'} = undef;
		$state->progress->clear;
		my $result = OpenBSD::Interactive::ask_list("Ambiguous: choose package for $name", 1, sort keys %h);
		return $h{$result};
	} else {
		$state->errsay("Ambiguous: $name could be ", join(' ', keys %h));
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

sub status
{
	my $self = shift;

	return $self->{status};
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

	$state->say($what." ".$object.$state->ntogo_string($state->todo));
	if ($state->{defines}->{carp}) {
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
