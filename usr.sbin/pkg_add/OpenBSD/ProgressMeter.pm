# ex:ts=8 sw=4:
# $OpenBSD: ProgressMeter.pm,v 1.38 2010/12/24 09:04:14 espie Exp $
#
# Copyright (c) 2010 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

package OpenBSD::ProgressMeter;
sub new
{
	bless {}, "OpenBSD::ProgressMeter::Stub";
}

sub setup
{
	my ($self, $opt_x, $opt_m, $state) = @_;
	if ($opt_m || (!$opt_x && -t STDOUT)) {
		require OpenBSD::ProgressMeter::Term;
		bless $self, "OpenBSD::ProgressMeter::Term";
		$self->{state} = $state;
		$self->init;
	}
}

sub print
{
	shift->clear;
	print @_;
}

sub errprint
{
	shift->clear;
	print STDERR @_;
}

sub for_list
{
	my ($self, $msg, $l, $code) = @_;
	if (defined $msg) {
		$self->set_header($msg);
	}
	my $total = scalar @$l;
	my $i = 0;
	for my $e (@$l) {
		$self->show(++$i, $total);
		&$code($e);
	}
	$self->next;
}

# stub class when no actual progressmeter that still prints out.
package OpenBSD::ProgressMeter::Stub;
our @ISA = qw(OpenBSD::ProgressMeter);

sub clear {}

sub show {}

sub working {}
sub message {}

sub next {}

sub set_header {}

sub ntogo
{
	return "";
}

sub visit_with_size
{
	my ($progress, $plist, $method, @r) = @_;
	$plist->$method(@r);
}

sub visit_with_count
{
	&OpenBSD::ProgressMeter::Stub::visit_with_size;
}

1;
