# ex:ts=8 sw=4:
# $OpenBSD: State.pm,v 1.2 2010/06/09 11:57:21 espie Exp $
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

# common routines to everything state.
# in particular, provides "singleton-like" access to UI.
package OpenBSD::State;
use Carp;

sub new
{
	my ($class, $cmd) = @_;
	my $o = bless {cmd => $cmd}, $class;
	$o->init(@_);
	return $o;
}

sub init
{
}

sub usage_is
{
	my ($self, @usage) = @_;
	$self->{usage} = \@usage;
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
	my $_ = shift;
	# make it so that #0 is #
	unshift(@_, '#');
	s/\#(\d+)/$_[$1]/ge;
	return $_;
}

sub fatal
{
	my $self = shift;
	# implementation note: to print "fatal errors" elsewhere,
	# the way is to eval { croak @_}; and decide what to do with $@.
	croak "Fatal error: ", $self->f(@_), "\n";
}

sub print
{
	my $self = shift;
	print $self->f(@_);
}

sub say
{
	my $self = shift;
	print $self->f(@_), "\n";
}

sub errprint
{
	my $self = shift;
	print STDERR $self->f(@_);
}

sub errsay
{
	my $self = shift;
	print STDERR $self->f(@_), "\n";
}

sub do_options
{
	my ($state, $sub) = @_;
	require OpenBSD::Error;
	# this could be nicer...
	eval { &$sub; };
	OpenBSD::Error::dienow($@, 
	    bless sub { $state->usage("#1", $_)}, "OpenBSD::Error::catchall");
}

1;
