# ex:ts=8 sw=4:
# $OpenBSD: State.pm,v 1.71 2022/02/12 09:46:19 espie Exp $
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
	return bless {state => $state}, $class;
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
use OpenBSD::Subst;
use OpenBSD::Error;
use parent qw(OpenBSD::BaseState Exporter);
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
	$SIG{'CONT'} = sub {
		$self->handle_continue;
	}
}

sub repo
{
	my $self = shift;
	return $self->{repo};
}

sub handle_continue
{
	my $self = shift;
	$self->find_window_size;
	# invalidate cache so this runs again after continue
	delete $self->{can_output};
}

OpenBSD::Auto::cache(can_output,
	sub {
		require POSIX;

		return 1 if !-t STDOUT;
		# XXX uses POSIX semantics so fd, we can hardcode stdout ;)
		my $s = POSIX::tcgetpgrp(1);
		# note that STDOUT may be redirected 
		# (tcgetpgrp() returns 0 for pipes and -1 for files)
		# (we shouldn't be there because of the tty test)
		return $s <= 0 || getpgrp() == $s;
	});

OpenBSD::Auto::cache(installpath,
	sub {
		my $self = shift;
		return undef if $self->defines('NOINSTALLPATH');
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

sub do_options
{
	my ($state, $sub) = @_;
	# this could be nicer...

	try {
		&$sub;
	} catch {
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

1;
