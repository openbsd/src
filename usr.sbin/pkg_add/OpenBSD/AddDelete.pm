# ex:ts=8 sw=4:
# $OpenBSD: AddDelete.pm,v 1.84 2019/04/07 12:30:39 espie Exp $
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

sub do_the_main_work
{
	my ($self, $state) = @_;

	if ($state->{bad}) {
		return;
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

sub handle_end_tags
{
	my ($self, $state) = @_;
	return if !defined $state->{tags}{atend};
	$state->progress->for_list("Running tags", 
	    [keys %{$state->{tags}{atend}}],
	    sub {
	    	my $k = shift;
		return if $state->{tags}{deleted}{$k};
		return if $state->{tags}{superseded}{$k};
		$state->{tags}{atend}{$k}->run_tag($state);
	    });
}

sub framework
{
	my ($self, $state) = @_;

	my $do = sub {
		lock_db($state->{not}, $state) unless $state->defines('nolock');
		$state->check_root;
		$self->process_parameters($state);
		my $dielater = $self->do_the_main_work($state);
		# cleanup various things
		$self->handle_end_tags($state);
		$state->{recorder}->cleanup($state);
		$state->ldconfig->ensure;
		OpenBSD::PackingElement->finish($state);
		$state->progress->clear;
		$state->log->dump;
		$self->finish_display($state);
		if ($state->verbose >= 2 || $state->{size_only} ||
		    $state->defines('tally')) {
			$state->vstat->tally;
		}
		$state->say("Extracted #1 from #2", 
		    $state->{stats}{donesize},
		    $state->{stats}{totsize}) 
			if defined $state->{stats} and $state->verbose;
		# show any error, and show why we died...
		rethrow $dielater;
	};
	if ($state->defines('debug')) {
		&$do;
	} else {
		try {
			&$do;
		} catch {
			$state->errsay("#1: #2", $0, $_);
			OpenBSD::Handler->reset;
			if ($_ =~ m/^Caught SIG(\w+)/o) {
				kill $1, $$;
			}
			$state->{bad}++;
		};
	}

}

sub parse_and_run
{
	my ($self, $cmd) = @_;

	my $state = $self->new_state($cmd);
	$state->handle_options;

	local $SIG{'INFO'} = sub { $state->status->print($state); };

	my ($lflag, $termios);
	if ($self->silence_children($state)) {
		require POSIX;

		$termios = POSIX::Termios->new;

		if (defined $termios->getattr) {
			$lflag = $termios->getlflag;
		}

		if (defined $lflag) {
			my $NOKERNINFO = 0x02000000; # not defined in POSIX
			$termios->setlflag($lflag | $NOKERNINFO);
			$termios->setattr;
			
		}
	}

	$self->framework($state);

	if (defined $lflag) {
		$termios->setlflag($lflag);
		$termios->setattr;
	}

	return $state->{bad} != 0;
}

sub silence_children
{
	1
}

# nothing to do
sub tweak_list
{
}

sub process_setlist
{
	my ($self, $state) = @_;
	$state->tracker->todo(@{$state->{setlist}});
	# this is the actual very small loop that processes all sets
	while (my $set = shift @{$state->{setlist}}) {
		$state->status->what->set($set);
		$set = $set->real_set;
		next if $set->{finished};
		$state->progress->set_header('Checking packages');
		unshift(@{$state->{setlist}}, $self->process_set($set, $state));
		$self->tweak_list($state);
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

package OpenBSD::AddDelete::State;
use OpenBSD::Vstat;
use OpenBSD::Log;
our @ISA = qw(OpenBSD::AddCreateDelete::State);

sub handle_options
{
	my ($state, $opt_string, @usage) = @_;

	$state->{extra_stats} = 0;
	$state->{opt}{V} = sub {
		$state->{extra_stats}++;
	};
	$state->{no_exports} = 1;
	$state->add_interactive_options;
	$state->SUPER::handle_options($opt_string.'aciInqsVB:', @usage);

	if ($state->opt('s')) {
		$state->{not} = 1;
	}
	# XXX RequiredBy
	$main::not = $state->{not};
	$state->{localbase} = $state->opt('L') // OpenBSD::Paths->localbase;
	$ENV{PATH} = join(':',
	    '/bin',
	    '/sbin',
	    '/usr/bin',
	    '/usr/sbin',
	    '/usr/X11R6/bin',
	    "$state->{localbase}/bin",
	    "$state->{localbase}/sbin");

	$state->{size_only} = $state->opt('s');
	$state->{quick} = $state->opt('q');
	$state->{extra} = $state->opt('c');
	$state->{automatic} = $state->opt('a') // 0;
	$ENV{'PKG_DELETE_EXTRA'} = $state->{extra} ? "Yes" : "No";
	if ($state->{not}) {
		$state->{loglevel} = 0;
	}
	$state->{loglevel} //= 1;
	if ($state->{loglevel}) {
		require Sys::Syslog;
		Sys::Syslog::openlog($state->{cmd}, "nofatal");
	}
	$state->{wantntogo} = $state->{extra_stats};
	if (defined $ENV{PKG_CHECKSUM}) {
		$state->{subst}->add('checksum', 1);
	}
	my $base = $state->opt('B') // '';
	if ($base ne '') {
		$base.='/' unless $base =~ m/\/$/o;
	}
	$state->{destdir} = $base;
}

sub init
{
	my $self = shift;
	$self->{l} = OpenBSD::Log->new($self);
	$self->{vstat} = OpenBSD::Vstat->new($self);
	$self->{status} = OpenBSD::Status->new;
	$self->{recorder} = OpenBSD::SharedItemsRecorder->new;
	$self->{v} = 0;
	$self->SUPER::init(@_);
	$self->{export_level}++;
}

sub syslog
{
	my $self = shift;
	return unless $self->{loglevel};
	Sys::Syslog::syslog('info', $self->f(@_));
}

sub ntodo
{
	my ($state, $offset) = @_;
	return $state->tracker->sets_todo($offset);
}

# one-level dependencies tree, for nicer printouts
sub build_deptree
{
	my ($state, $set, @deps) = @_;

	if (defined $state->{deptree}->{$set}) {
		$set = $state->{deptree}->{$set};
	}
	for my $dep (@deps) {
		$state->{deptree}->{$dep} = $set unless
		    defined $state->{deptree}->{$dep};
	}
}

sub deptree_header
{
	my ($state, $pkg) = @_;
	if (defined $state->{deptree}->{$pkg}) {
		my $s = $state->{deptree}->{$pkg}->real_set;
		if ($s eq $pkg) {
			delete $state->{deptree}->{$pkg};
		} else {
			return $s->short_print.':';
		}
	}
	return '';
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

sub run_quirks
{
	my ($state, $sub) = @_;

	if (!exists $state->{quirks}) {
		eval {
			require OpenBSD::Quirks;
			# interface version number.
			$state->{quirks} = OpenBSD::Quirks->new(1);
		};
		if ($@) {
			my $show = $state->verbose >= 2;
			if (!$show) {
				my $l = $state->repo->installed->match_locations(OpenBSD::Search::Stem->new('quirks'));
				$show = @$l > 0;
			}
			$state->errsay("Can't load quirk: #1", $@) if $show;
			# XXX cache that this didn't work
			$state->{quirks} = undef;
		}
	}

	if (defined $state->{quirks}) {
		eval {
			&$sub($state->{quirks});
		};
		if ($@) {
			$state->errsay("Bad quirk: #1", $@);
		}
	}
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
		if (!$is_quirks) {
			$state->errsay("Can't find #1", $name);
			$state->run_quirks(
			    sub {
				my $quirks = shift;
				$quirks->filter_obsolete([$name], $state);
			    });
		}
		return undef;
	} elsif (@$list == 1) {
		return $list->[0];
	}

	my %h = map {($_->name, $_)} @$list;
	if ($state->is_interactive) {
		$h{'<None>'} = undef;
		$state->progress->clear;
		my $result = $state->ask_list("Ambiguous: choose package for $name", sort keys %h);
		return $h{$result};
	} else {
		$state->errsay("Ambiguous: #1 could be #2",
		    $name, join(' ', keys %h));
		return undef;
	}
}

sub status
{
	my $self = shift;

	return $self->{status};
}

sub replacing
{
	my $self = shift;
	return $self->{replacing};
}

OpenBSD::Auto::cache(ldconfig,
    sub {
    	my $self = shift;
	return OpenBSD::LdConfig->new($self);
    });

# if we're not running as root, allow some stuff when not under /usr/local
sub allow_nonroot
{
	my ($state, $path) = @_;
	return $state->defines('nonroot') &&
	    $path !~ m,^\Q$state->{localbase}/\E,;
}

sub make_path
{
	my ($state, $path, $fullname) = @_;
	require File::Path;
	if ($state->allow_nonroot($fullname)) {
		eval {
			File::Path::mkpath($path);
		};
	} else {
		File::Path::mkpath($path);
	}
}

# this is responsible for running ldconfig when needed
package OpenBSD::LdConfig;

sub new
{
	my ($class, $state) = @_;
	bless { state => $state, todo => 0 }, $class;
}

# called once to figure out which directories are actually used
sub init
{
	my $self = shift;
	my $state = $self->{state};
	my $destdir = $state->{destdir};

	$self->{ldconfig} = [OpenBSD::Paths->ldconfig];

	$self->{path} = {};
	if ($destdir ne '') {
		unshift @{$self->{ldconfig}}, OpenBSD::Paths->chroot, '--',
		    $destdir;
	}
	open my $fh, "-|", @{$self->{ldconfig}}, "-r";
	if (defined $fh) {
		while (<$fh>) {
			if (m/^\s*search directories:\s*(.*?)\s*$/o) {
				for my $d (split(/\:/o, $1)) {
					$self->{path}{$d} = 1;
				}
				last;
			}
		}
		close($fh);
	} else {
		$state->errsay("Can't find ldconfig");
	}
}

# called from libs to figure out whether ldconfig should be rerun
sub mark_directory
{
	my ($self, $name) = @_;
	if (!defined $self->{path}) {
		$self->init;
	}
	require File::Basename;
	my $d = File::Basename::dirname($name);
	if ($self->{path}{$d}) {
		$self->{todo} = 1;
	}
}

# call before running any command (or at end) to run ldconfig just in time
sub ensure
{
	my $self = shift;
	if ($self->{todo}) {
		my $state = $self->{state};
		$state->vsystem(@{$self->{ldconfig}}, "-R")
		    unless $state->{not};
		$self->{todo} = 0;
	}
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

	$state->say($what." #1#2", $object, $state->ntogo_string);
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
