# ex:ts=8 sw=4:
# $OpenBSD: Error.pm,v 1.6 2004/11/15 02:44:50 espie Exp $
#
# Copyright (c) 2004 Marc Espie <espie@openbsd.org>
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
#

use strict;
use warnings;

package OpenBSD::Error;
our @ISA=qw(Exporter);
our @EXPORT=qw(System VSystem Copy Fatal Warn);

sub System
{
	my $r = system(@_);
	if ($r != 0) {
		print "system(", join(", ", @_), ") failed: $?\n";
	}
	return $r;
}

sub VSystem
{
	my $verbose = shift;
	if (!$verbose) {
		&System;
	} else {
		print "Running ", join(' ', @_);
		my $r = system(@_);
		if ($r != 0) {
			print "... failed: $?\n";
		} else {
			print "\n";
		}
	}
}

sub Copy
{
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		print "copy(", join(',', @_),") failed: $!\n";
	}
	return $r;
}

sub Fatal
{
	require Carp;
	Carp::croak @_;
}

sub Warn
{
	print STDERR @_;
}

sub new
{
	my $class = shift;
	bless {messages=>{}}, $class;
}

sub set_pkgname
{
	my ($self, $pkgname) = @_;
	$self->{pkgname} = $pkgname;
	if (!defined $self->{messages}->{$pkgname}) {
		$self->{messages}->{$pkgname} = [];
	}
	$self->{output} = $self->{messages}->{$pkgname};
}

sub warn
{
	&OpenBSD::Error::print;
}

sub fatal
{
	my $self = shift;
	require Carp;
	Carp::croak ($self->{pkgname}, ':', @_);
}

sub print
{
	my $self = shift;
	push(@{$self->{output}}, join('', @_));
}

sub delayed_output
{
	my $self = shift;
	for my $pkg (sort keys %{$self->{messages}}) {
		my $msgs = $self->{messages}->{$pkg};
		if (@$msgs > 0) {
			print "--- $pkg -------------------\n";
			print @$msgs;
		}
	}
}

sub system
{
	my $state = shift;
	if (open(my $grab, "-|", @_)) {
		local $_;
		while (<$grab>) {
			$state->print($_);
		}
		if (!close $grab) {
		    $state->print("system(", join(", ", @_), ") failed: $! $?\n");
		}
		return $?;
	} else {
		    $state->print("system(", join(", ", @_), ") was not run: $! $?\n");
	}
}
1;
