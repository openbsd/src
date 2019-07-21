# ex:ts=8 sw=4:
# $OpenBSD: Error.pm,v 1.37 2019/07/21 15:31:39 espie Exp $
#
# Copyright (c) 2004-2010 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

# this is a set of common classes related to error handling in pkg land

package OpenBSD::Auto;
sub cache(*&)
{
	my ($sym, $code) = @_;
	my $callpkg = caller;
	my $actual = sub {
		my $self = shift;
		return $self->{$sym} //= &$code($self);
	};
	no strict 'refs';
	*{$callpkg."::$sym"} = $actual;
}

# a bunch of other modules create persistent state that must be cleaned up
# on exit (temporary files, network connections to abort properly...)
# END blocks would do that (but see below...) but sig handling bypasses that,
# so we MUST install SIG handlers.

# note that END will be run for *each* process, so beware!
# (temp files are registered per pid, for instance, so they only
# get cleaned when the proper pid is used)
package OpenBSD::Handler;

# hash of code to run on ANY exit
my $cleanup = {};

sub cleanup
{
	my ($class, $sig) = @_;
	# XXX note that order of cleanup is "unpredictable"
	for my $v (values %$cleanup) {
		&$v($sig);
	}
}

#END {
#	# XXX localize $? so that cleanup doesn't fuck up our exit code
#	local $?;
#	cleanup();
#}

# register each code block "by name" so that we can re-register each
# block several times
sub register
{
	my ($class, $code) = @_;
	$cleanup->{$code} = $code;
}

my $handler = sub {
	my $sig = shift;
	__PACKAGE__->cleanup($sig);
	# after cleanup, just propagate the signal
	$SIG{$sig} = 'DEFAULT';
	kill $sig, $$;
};

sub reset
{
	for my $sig (qw(INT QUIT HUP KILL TERM)) {
		$SIG{$sig} = $handler;
	}
}

__PACKAGE__->reset;

package OpenBSD::Error;
require Exporter;
our @ISA=qw(Exporter);
our @EXPORT=qw(try throw catch catchall rethrow);

our ($FileName, $Line, $FullMessage);

use Carp;
sub dienow
{
	my ($error, $handler) = @_;
	if ($error) {
		if ($error =~ m/^(.*?)(?:\s+at\s+(.*)\s+line\s+(\d+)\.?)?$/o) {
			local $_ = $1;
			$FileName = $2;
			$Line = $3;
			$FullMessage = $error;

			$handler->exec($error, $1, $2, $3);
		} else {
			die "Fatal error: can't parse $error";
		}
	}
}

sub try(&@)
{
	my ($try, $catch) = @_;
	eval { &$try };
	dienow($@, $catch);
}

sub throw
{
	croak @_;

}

sub rethrow
{
	my $e = shift;
	die $e if $e;
}

sub catch(&)
{
		bless $_[0], "OpenBSD::Error::catch";
}

sub catchall(&)
{
	bless $_[0], "OpenBSD::Error::catch";
}

sub rmtree
{
	my $class = shift;
	require File::Path;
	require Cwd;

	# XXX make sure we live somewhere
	Cwd::getcwd() || chdir('/');

	File::Path::rmtree(@_);
}

package OpenBSD::Error::catch;
sub exec
{
	my ($self, $full, $e) = @_;
	&$self;
}

1;
