# ex:ts=8 sw=4:
# $OpenBSD: Error.pm,v 1.36 2019/07/21 14:18:55 espie Exp $
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

package OpenBSD::Handler;

my $list = [];

sub register
{
	my ($class, $code) = @_;
	push(@$list, $code);
}

my $handler = sub {
	my $sig = shift;
	for my $c (@$list) {
		&$c($sig);
	}
	$SIG{$sig} = 'DEFAULT';
	kill $sig, $$;
};

sub reset
{
	$SIG{'INT'} = $handler;
	$SIG{'QUIT'} = $handler;
	$SIG{'HUP'} = $handler;
	$SIG{'KILL'} = $handler;
	$SIG{'TERM'} = $handler;
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
