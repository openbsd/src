# ex:ts=8 sw=4:
# $OpenBSD: State.pm,v 1.16 2010/07/26 23:27:28 espie Exp $
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

package OpenBSD::Configuration;
sub new
{
	my ($class, $state) = @_;
	my $self = bless {}, $class;
	require OpenBSD::Paths;
	$self->read_file(OpenBSD::Paths->pkgconf, $state);
	return $self;
}

sub read_file
{
	my ($self, $filename, $state) = @_;
	open(my $fh, '<', $filename) or return;
	my $_;
	while (<$fh>) {
		chomp;
		next if m/^\s*\#/;
		next if m/^\s*$/;
		my ($cmd, $k, $v, $add);
		my $h = $self;
		if (($cmd, $k, $add, $v) = m/^\s*(.*?)\.(.*?)\s*(\+?)\=\s*(.*)\s*$/) {
			next unless $cmd eq $state->{cmd};
			my $h = $self->{cmd} = {};
		} elsif (($k, $add, $v) = m/^\s*(.*?)\s*(\+?)\=\s*(.*)\s*$/) {
		} else {
			# bad line: should we say so ?
			$state->errsay("Bad line in #1: #2 (#3)", 
			    $filename, $_, $.);
		}
		# remove caps
		$k =~ tr/A-Z/a-z/;
		if ($add eq '') {
			$h->{$k} = [$v];
		} else {
			push(@{$h->{$k}}, $v);
		}
	}
}

sub ref
{
	my ($self, $k) = @_;
	if (defined $self->{cmd}{$k}) {
		return $self->{cmd}{$k};
	} else {
		return $self->{$k};
	}
}

sub value
{
	my ($self, $k) = @_;
	my $r = $self->ref($k);
	if (!defined $r) {
		return $r;
	}
	if (wantarray) {
		return @$r;
	} else {
		return $r->[0];
	}
}

sub istrue
{
	my ($self, $k) = @_;
	my $v = $self->value($k);
	if (defined $v && $v =~ /^yes$/i) {
		return 1;
	} else {
		return 0;
	}
}

package OpenBSD::PackageRepositoryFactory;
sub new
{
	my ($class, $state) = @_;
	bless {state => $state}, $class;
}

sub installed
{
	my ($self, $all) = @_;
	require OpenBSD::PackageRepository::Installed;

	return OpenBSD::PackageRepository::Installed->new($all, $self->{state});
}

sub path_parse
{
	my ($self, $pkgname) = @_;
	require OpenBSD::PackageLocator;

	return OpenBSD::PackageLocator->path_parse($pkgname, $self->{state});
}

sub find
{
	my ($self, $pkg, $arch) = @_;
	require OpenBSD::PackageLocator;

	return OpenBSD::PackageLocator->find($pkg, $arch, $self->{state});
}

sub match_locations
{
	my $self = shift;
	require OpenBSD::PackageLocator;

	return OpenBSD::PackageLocator->match_locations(@_, $self->{state});
}

sub grabPlist
{
	my ($self, $url, $arch, $code) = @_;
	require OpenBSD::PackageLocator;

	return OpenBSD::PackageLocator->grabPlist($url, $arch, $code, $self->{state});
}

sub path
{
	my $self = shift;
	require OpenBSD::PackageRepositoryList;

	return OpenBSD::PackageRepositoryList->new($self->{state});
}

# common routines to everything state.
# in particular, provides "singleton-like" access to UI.
package OpenBSD::State;
use Carp;
use OpenBSD::Subst;
use OpenBSD::Error;
require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = ();

sub new
{
	my ($class, $cmd) = @_;
	my $o = bless {cmd => $cmd}, $class;
	$o->init(@_);
	return $o;
}

sub init
{
	my $self = shift;
	$self->{subst} = OpenBSD::Subst->new;
	$self->{repo} = OpenBSD::PackageRepositoryFactory->new($self);
	$self->{export_level} = 1;
}

sub repo
{
	my $self = shift;
	return $self->{repo};
}

OpenBSD::Auto::cache(config,
	sub {
		return OpenBSD::Configuration->new(shift);
	});

sub usage_is
{
	my ($self, @usage) = @_;
	$self->{usage} = \@usage;
}

sub verbose
{
	my $self = shift;
	return $self->{v};
}

sub opt
{
	my ($self, $k) = @_;
	return $self->{opt}{$k};
}

sub usage
{
	my $self = shift;
	my $code = 0;
	if (@_) {
		print STDERR "$self->{cmd}: ", $self->f(@_), "\n";
		$code = 1;
	}
	print STDERR "Usage: $self->{cmd} ", shift(@{$self->{usage}}), "\n";
	for my $l (@{$self->{usage}}) {
		print STDERR "       $l\n";
	}
	exit($code);
}

sub f
{
	my $self = shift;
	if (@_ == 0) {
		return undef;
	}
	my ($_, @l) = @_;
	# make it so that #0 is #
	unshift(@l, '#');
	s/\#(\d+)/$l[$1]/ge;
	return $_;
}

sub _fatal
{
	my $self = shift;
	# implementation note: to print "fatal errors" elsewhere,
	# the way is to eval { croak @_}; and decide what to do with $@.
	croak "Fatal error: ", @_, "\n";
}

sub fatal
{
	my $self = shift;
	$self->_fatal($self->f(@_));
}

sub _print
{
	my $self = shift;
	print @_;
}

sub _errprint
{
	my $self = shift;
	print STDERR @_;
}

sub print
{
	my $self = shift;
	$self->_print($self->f(@_));
}

sub say
{
	my $self = shift;
	if (@_ == 0) {
		$self->_print("\n");
	} else {
		$self->_print($self->f(@_), "\n");
	}
}

sub errprint
{
	my $self = shift;
	$self->_errprint($self->f(@_));
}

sub errsay
{
	my $self = shift;
	if (@_ == 0) {
		$self->_errprint("\n");
	} else {
		$self->_errprint($self->f(@_), "\n");
	}
}

sub do_options
{
	my ($state, $sub) = @_;
	# this could be nicer...

	try {
		&$sub;
	} catchall {
		$state->usage("#1", $_);
	};
}

sub handle_options
{
	my ($state, $opt_string, @usage) = @_;
	require OpenBSD::Getopt;

	$state->{opt}{v} = 0 unless $opt_string =~ m/v/;
	$state->{opt}{h} = sub { $state->usage; } unless $opt_string =~ m/h/;
	$state->{opt}{D} = sub {
		$state->{subst}->parse_option(shift);
	} unless $opt_string =~ m/D/;
	$state->usage_is(@usage);
	$state->do_options(sub {
		OpenBSD::Getopt::getopts($opt_string.'hvD:', $state->{opt});
	});
	$state->{v} = $state->opt('v');
	return if $state->{no_exports};
	# XXX
	no strict "refs";
	no strict "vars";
	for my $k (keys %{$state->{opt}}) {
		${"opt_$k"} = $state->opt($k);
		push(@EXPORT, "\$opt_$k");
	}
	local $Exporter::ExportLevel = $state->{export_level};
	import OpenBSD::State;
}

my @signal_name = ();
sub fillup_names
{
	{
	# XXX force autoload
	package verylocal;

	require POSIX;
	POSIX->import(qw(signal_h));
	}

	for my $sym (keys %POSIX::) {
		next unless $sym =~ /^SIG([A-Z].*)/;
		$signal_name[eval "&POSIX::$sym()"] = $1;
	}
	# extra BSD signals
	$signal_name[5] = 'TRAP';
	$signal_name[7] = 'IOT';
	$signal_name[10] = 'BUS';
	$signal_name[12] = 'SYS';
	$signal_name[16] = 'URG';
	$signal_name[23] = 'IO';
	$signal_name[24] = 'XCPU';
	$signal_name[25] = 'XFSZ';
	$signal_name[26] = 'VTALRM';
	$signal_name[27] = 'PROF';
	$signal_name[28] = 'WINCH';
	$signal_name[29] = 'INFO';
}

sub find_signal
{
	my $number =  shift;

	if (@signal_name == 0) {
		fillup_names();
	}

	return $signal_name[$number] || $number;
}

sub child_error
{
	my $self = shift;
	my $error = $?;

	my $extra = "";

	if ($error & 128) {
		$extra = $self->f(" (core dumped)");
	}
	if ($error & 127) {
		return $self->f("killed by signal #1#2",
		    find_signal($error & 127), $extra);
	} else {
		return $self->f("exit(#1)#2", ($error >> 8), $extra);
	}
}

sub system
{
	my $self = shift;
	my $r = CORE::system(@_);
	if ($r != 0) {
		$self->say("system(#1) failed: #2",
		    join(", ", @_), $self->child_error);
	}
	return $r;
}

sub verbose_system
{
	my $self = shift;

	$self->print("Running #1", join(' ', @_));
	my $r = CORE::system(@_);
	if ($r != 0) {
		$self->say("... failed: #1", $self->child_error);
	} else {
		$self->say;
	}
}

sub copy_file
{
	my $self = shift;
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		$self->say("copy(#1) failed: #2", join(',', @_), $!);
	}
	return $r;
}

sub unlink
{
	my $self = shift;
	my $verbose = shift;
	my $r = unlink @_;
	if ($r != @_) {
		$self->say("rm #1 failed: removed only #2 targets, #3",
		    join(' ', @_), $r, $!);
	} elsif ($verbose) {
		$self->say("rm #1", join(' ', @_));
	}
	return $r;
}

sub copy
{
	my $self = shift;
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		$self->say("copy(#1) failed: #2", join(',', @_), $!);
	}
	return $r;
}

1;
