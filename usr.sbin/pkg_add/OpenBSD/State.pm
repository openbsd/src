# ex:ts=8 sw=4:
# $OpenBSD: State.pm,v 1.54 2018/08/03 06:37:08 espie Exp $
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

package OpenBSD::PackageRepositoryFactory;
sub new
{
	my ($class, $state) = @_;
	bless {state => $state}, $class;
}

sub locator
{
	my $self = shift;
	return $self->{state}->locator;
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

	return $self->locator->path_parse($pkgname, $self->{state});
}

sub find
{
	my ($self, $pkg) = @_;

	return $self->locator->find($pkg, $self->{state});
}

sub reinitialize
{
}

sub match_locations
{
	my $self = shift;

	return $self->locator->match_locations(@_, $self->{state});
}

sub grabPlist
{
	my ($self, $url, $code) = @_;

	return $self->locator->grabPlist($url, $code, $self->{state});
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

sub locator
{
	require OpenBSD::PackageLocator;
	return "OpenBSD::PackageLocator";
}

sub cache_directory
{
	return undef;
}

sub new
{
	my $class = shift;
	my $cmd = shift;
	if (!defined $cmd) {
		$cmd = $0;
		$cmd =~ s,.*/,,;
	}
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

sub sync_display
{
}

OpenBSD::Auto::cache(installpath,
	sub {
		my $self = shift;
		require OpenBSD::Paths;
		open(my $fh, '<', OpenBSD::Paths->installurl) or return undef;
		while (<$fh>) {
			chomp;
			next if m/^\s*\#/;
			next if m/^\s*$/;
			return "$_/%c/packages/%a/";
		}
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
		s/$forbidden/?/g for @l;
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
	croak "Fatal error: ", @_, "\n";
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
	$self->_fhprint(\*STDOUT, @_);
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
	$self->fhprint(\*STDOUT, @_);
}

sub say
{
	my $self = shift;
	$self->fhsay(\*STDOUT, @_);
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

	if ($state->defines('unsigned')) {
		$state->{signature_style} //= 'unsigned';
	} elsif ($state->defines('oldsign')) {
		$state->fatal('old style signature no longer supported');
	} else {
		$state->{signature_style} //= 'new';
	}

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

sub defines
{
	my ($self, $k) = @_;
	return $self->{subst}->value($k);
}

sub width
{
	my $self = shift;
	if (!defined $self->{width}) {
		$self->find_window_size;
	}
	return $self->{width};
}

sub height
{
	my $self = shift;
	if (!defined $self->{height}) {
		$self->find_window_size;
	}
	return $self->{height};
}
		
sub find_window_size
{
	my $self = shift;
	require Term::ReadKey;
	my @l = Term::ReadKey::GetTermSizeGWINSZ(\*STDOUT);
	# default to sane values
	$self->{width} = 80;
	$self->{height} = 24;
	if (@l == 4) {
		# only use what we got if sane
		$self->{width} = $l[0] if $l[0] > 0;
		$self->{height} = $l[1] if $l[1] > 0;
		$SIG{'WINCH'} = sub {
			$self->find_window_size;
		};
	}
	$SIG{'CONT'} = sub {
		$self->find_window_size(1);
	}
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
		&$todo;
		exec {$_[0]} @_;
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
