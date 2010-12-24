# ex:ts=8 sw=4:
# $OpenBSD: Log.pm,v 1.6 2010/12/24 09:04:14 espie Exp $
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

package OpenBSD::Log;

sub new
{
	my ($class, $printer) = @_;
	bless { p => $printer }, $class;
}

sub set_context
{
	my ($self, $context) = @_;
	$self->{context} = $context;
	$self->{output} = $self->{messages}->{$context} //= [];
	$self->{erroutput} = $self->{errmessages}->{$context} //= [];
}

sub f
{
	my $self = shift;
	$self->{p}->f(@_);
}

sub print
{
	my $self = shift;
	push(@{$self->{output}}, $self->f(@_));
}

sub say
{
	my $self = shift;
	push(@{$self->{output}}, $self->f(@_)."\n");
}

sub errprint
{
	my $self = shift;
	push(@{$self->{erroutput}}, $self->f(@_));
}

sub errsay
{
	my $self = shift;
	push(@{$self->{erroutput}}, $self->f(@_)."\n");
}

sub specialsort
{
	return ((sort grep { /^\-/ } @_), (sort grep { /^\+/} @_),
	    (sort grep { !/^[\-+]/ } @_));
}

sub dump
{
	my $self = shift;
	for my $ctxt (specialsort keys %{$self->{errmessages}}) {
		my $msgs = $self->{errmessages}->{$ctxt};
		if (@$msgs > 0) {
			$self->{p}->errsay("--- #1 -------------------", $ctxt);
			$self->{p}->_errprint(@$msgs);
		}
	}
	$self->{errmessages} = {};
	for my $ctxt (specialsort keys %{$self->{messages}}) {
		my $msgs = $self->{messages}->{$ctxt};
		if (@$msgs > 0) {
			$self->{p}->say("--- #1 -------------------", $ctxt);
			$self->{p}->_print(@$msgs);
		}
	}
	$self->{messages} = {};
}

sub fatal
{
	my $self = shift;
	if (defined $self->{context}) {
		$self->{p}->_fatal($self->{context}, ":", $self->f(@_));
	}

	$self->{p}->_fatal($self->f(@_));
}

sub system
{
	my $self = shift;
	if (open(my $grab, "-|", @_)) {
		my $_;
		while (<$grab>) {
			$self->{p}->_print($_);
		}
		if (!close $grab) {
			$self->{p}->say("system(#1) failed: #2 #3",
			    join(", ", @_), $!,
			    $self->{p}->child_error);
		}
		return $?;
	} else {
		$self->{p}->say("system(#1) was not run: #2 #3",
		    join(", ", @_), $!, $self->{p}->child_error);
	}
}

1;
