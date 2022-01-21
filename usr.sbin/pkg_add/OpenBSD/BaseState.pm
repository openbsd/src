# ex:ts=8 sw=4:
# $OpenBSD: BaseState.pm,v 1.1 2022/01/21 17:41:41 espie Exp $
#
# Copyright (c) 2007-2022 Marc Espie <espie@openbsd.org>
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

package OpenBSD::BaseState;
use Carp;

sub can_output
{
	1;
}
sub sync_display
{
}

my $forbidden = qr{[^[:print:]\s]};

sub safe
{
	my ($self, $string) = @_;
	$string =~ s/$forbidden/?/g;
	return $string;
}

sub f
{
	my $self = shift;
	if (@_ == 0) {
		return undef;
	}
	my ($fmt, @l) = @_;

	# is there anything to format, actually ?
	if ($fmt =~ m/\#\d/) {
		# encode any unknown chars as ?
		for (@l) {
			s/$forbidden/?/g if defined;
		}
		# make it so that #0 is #
		unshift(@l, '#');
		$fmt =~ s,\#(\d+),($l[$1] // "<Undefined #$1>"),ge;
	}
	return $fmt;
}

sub _fatal
{
	my $self = shift;
	# implementation note: to print "fatal errors" elsewhere,
	# the way is to eval { croak @_}; and decide what to do with $@.
	delete $SIG{__DIE__};
	$self->sync_display;
	croak @_, "\n";
}

sub fatal
{
	my $self = shift;
	$self->_fatal($self->f(@_));
}

sub _fhprint
{
	my $self = shift;
	my $fh = shift;
	$self->sync_display;
	print $fh @_;
}
sub _print
{
	my $self = shift;
	$self->_fhprint(\*STDOUT, @_) if $self->can_output;
}

sub _errprint
{
	my $self = shift;
	$self->_fhprint(\*STDERR, @_);
}

sub fhprint
{
	my $self = shift;
	my $fh = shift;
	$self->_fhprint($fh, $self->f(@_));
}

sub fhsay
{
	my $self = shift;
	my $fh = shift;
	if (@_ == 0) {
		$self->_fhprint($fh, "\n");
	} else {
		$self->_fhprint($fh, $self->f(@_), "\n");
	}
}

sub print
{
	my $self = shift;
	$self->fhprint(\*STDOUT, @_) if $self->can_output;
}

sub say
{
	my $self = shift;
	$self->fhsay(\*STDOUT, @_) if $self->can_output;
}

sub errprint
{
	my $self = shift;
	$self->fhprint(\*STDERR, @_);
}

sub errsay
{
	my $self = shift;
	$self->fhsay(\*STDERR, @_);
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
		my $value = eval "&POSIX::$sym()";
		# skip over POSIX stuff we don't have like SIGRT or SIGPOLL
		next unless defined $value;
		$signal_name[$value] = $1;
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
	my ($self, $number) = @_;

	if (@signal_name == 0) {
		$self->fillup_names;
	}

	return $signal_name[$number] || $number;
}

sub child_error
{
	my ($self, $error) = @_;
	$error //= $?;

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

sub _system
{
	my $self = shift;
	$self->sync_display;
	my ($todo, $todo2);
	if (ref $_[0] eq 'CODE') {
		$todo = shift;
	} else {
		$todo = sub {};
	}
	if (ref $_[0] eq 'CODE') {
		$todo2 = shift;
	} else {
		$todo2 = sub {};
	}
	my $r = fork;
	if (!defined $r) {
		return 1;
	} elsif ($r == 0) {
		$DB::inhibit_exit = 0;
		&$todo;
		exec {$_[0]} @_ or
		    exit 1;
	} else {
		&$todo2;
		waitpid($r, 0);
		return $?;
	}
}

sub system
{
	my $self = shift;
	my $r = $self->_system(@_);
	if ($r != 0) {
		if (ref $_[0] eq 'CODE') {
			shift;
		}
		if (ref $_[0] eq 'CODE') {
			shift;
		}
		$self->errsay("system(#1) failed: #2",
		    join(", ", @_), $self->child_error);
	}
	return $r;
}

sub verbose_system
{
	my $self = shift;
	my @p = @_;
	if (ref $p[0]) {
		shift @p;
	}
	if (ref $p[0]) {
		shift @p;
	}

	$self->print("Running #1", join(' ', @p));
	my $r = $self->_system(@_);
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
