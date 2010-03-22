# ex:ts=8 sw=4:
# $OpenBSD: Term.pm,v 1.1 2010/03/22 20:38:44 espie Exp $
#
# Copyright (c) 2004-2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackingElement;
sub size_and
{
	my ($self, $progress, $donesize, $totsize, $method, @r) = @_;
	if (defined $self->{size}) {
		$$donesize += $self->{size};
		$progress->show($$donesize, $totsize);
	}
	$self->$method(@r);
}

sub compute_size
{
	my ($self, $totsize) = @_;

	$$totsize += $self->{size} if defined $self->{size};
}

sub compute_count
{
	my ($self, $count) = @_;

	$$count ++;
}

sub count_and
{
	my ($self, $progress, $done, $total, $method, @r) = @_;
	$$done ++;
	$progress->show($$done, $total);
	$self->$method(@r);
}

package OpenBSD::ProgressMeter::Term;
our @ISA = qw(OpenBSD::ProgressMeter);
use POSIX;
use Term::Cap;

sub init
{
	my $self = shift;
	my $oldfh = select(STDOUT);
	$| = 1;
	select($oldfh);
	$self->find_window_size;
	$self->{lastdisplay} = '';
	$self->{continued} = 0;
	return unless defined $ENV{TERM} || defined $ENV{TERMCAP};
	my $termios = POSIX::Termios->new;
	$termios->getattr(0);
	$self->{terminal} = Term::Cap->Tgetent({ OSPEED => 
	    $termios->getospeed});
	$self->{glitch} = $self->{terminal}->Tputs("xn", 1);
	$self->{cleareol} = $self->{terminal}->Tputs("ce", 1);
	$self->{hpa} = $self->{terminal}->Tputs("ch", 1);
}

my $wsz_format = 'SSSS';
our %sizeof;

sub find_window_size
{
	my $self = shift;
	# try to get exact window width
	my $r;
	$r = pack($wsz_format, 0, 0, 0, 0);
	$sizeof{'struct winsize'} = 8;
	require 'sys/ttycom.ph';
	if (ioctl(STDOUT, &TIOCGWINSZ, $r)) {
		my ($rows, $cols, $xpix, $ypix) = 
		    unpack($wsz_format, $r);
		$self->{width} = $cols;
	} else {
		$self->{width} = 80;
	}
}

sub compute_playfield
{
	my $self = shift;
	$self->{playfield} = $self->{width} - length($self->{header}) - 7;
	# we can print to 80 columns
	if ($self->{glitch}) {
		$self->{playfield} += 1;
	}
	if ($self->{playfield} < 5) {
		$self->{playfield} = 0;
	}
}

sub set_header
{
	my ($self, $header) = @_;
	$self->{header} = $header;
	$self->compute_playfield;
	$SIG{'WINCH'} = sub {
		$self->find_window_size;
		$self->compute_playfield;
	};
	$SIG{'CONT'} = sub {
		$self->{continued} = 1;
	};
	return 1;
}

sub hmove
{
	my ($self, $v) = @_;
	my $_ = $self->{hpa};
	s/\%i// and $v++;
	s/\%n// and $v ^= 0140;
	s/\%B// and $v = 16 * ($v/10) + $v%10;
	s/\%D// and $v = $v - 2*($v%16);
	s/\%\./sprintf('%c', $v)/e;
	s/\%d/sprintf($&, $v)/e;
	s/\%2/sprintf('%2d', $v)/e;
	s/\%3/sprintf('%3d', $v)/e;
	s/\%\+(.)/sprintf('%c', $v+ord($1))/e;
	s/\%\%/\%/g;
	return $_;
}

sub _show
{
	my ($self, $extra, $stars) = @_;
	my $d = $self->{header};
	if (defined $extra) {
		$d.="|$extra";
	}
	return if $d eq $self->{lastdisplay} && !$self->{continued};
	if ($self->{width} > length($d)) {
		if ($self->{cleareol}) {
			$d .= $self->{cleareol};
		} else {
			$d .= ' 'x($self->{width} - length($d) - 1);
		}
	}
	my $prefix;
	if (!$self->{continued} && defined $self->{hpa}) {
		if (defined $stars && defined $self->{stars}) { 
			$prefix = length($self->{header})+1+$self->{stars};
		} else {
			$prefix = length($self->{header});
		}
	}
	if (defined $prefix && substr($self->{lastdisplay}, 0, $prefix) eq
	    substr($d, 0, $prefix)) {
		print $self->hmove($prefix), substr($d, $prefix);
	} else {
		print "\r$d";
	}
	$self->{lastdisplay} = $d;
	$self->{continued} = 0;
}

sub message
{
	my ($self, $message) = @_;
	if ($self->{cleareol}) {
		$message .= $self->{cleareol};
	} elsif ($self->{playfield} > length($message)) {
		$message .= ' 'x($self->{playfield} - length($message));
	}
	if ($self->{playfield}) {
		$self->_show(substr($message, 0, $self->{playfield}));
	} else {
		$self->_show;
	}
}

sub show
{
	my ($self, $current, $total) = @_;

	if ($self->{playfield}) {
		my $stars = int (($current * $self->{playfield}) / $total + 0.5);
		my $percent = int (($current * 100)/$total + 0.5);
		if ($percent < 100) {
			$self->_show('*'x$stars.' 'x($self->{playfield}-$stars)."| ".$percent."\%", $stars);
		} else {
			$self->_show('*'x$self->{playfield}."|100\%", $stars);
		}
		$self->{stars} = $stars;
	} else {
	    	$self->_show;
	}
}

sub clear
{
	my $self = shift;
	return unless length($self->{lastdisplay}) > 0;
	if ($self->{cleareol}) {
		print "\r", $self->{cleareol};
	} else {
		print "\r", ' 'x length($self->{lastdisplay}), "\r";
	}
	$self->{lastdisplay} = '';
	undef $self->{stars};
}

sub next
{
	my ($self, $todo) = @_;
	$self->clear;

	$todo //= '';
	print "\r$self->{header}: ok$todo\n";
}

sub ntogo
{
	&OpenBSD::UI::ntogo_string;
}

sub compute_size
{
	my $plist = shift;
	my $totsize = 0;
	$plist->compute_size(\$totsize);
	$totsize = 1 if $totsize == 0;
	return $totsize;
}

sub compute_count
{
	my $plist = shift;
	my $total = 0;
	$plist->compute_count(\$total);
	$total = 1 if $total == 0;
	return $total;
}

sub visit_with_size
{
	my ($progress, $plist, $method, $state, @r) = @_;
	$plist->{totsize} //= compute_size($plist);
	my $donesize = 0;
	$progress->show($donesize, $plist->{totsize});
	if (defined $state->{archive}) {
		$state->{archive}{callback} = sub {
		    my $done = shift;
		    $progress->show($$donesize + $done, $plist->{totsize});
		};
	}
	$plist->size_and($progress, \$donesize, $plist->{totsize}, 
	    $method, $state, @r);
}

sub visit_with_count
{
	my ($progress, $plist, $method, $state, @r) = @_;
	$plist->{total} //= compute_count($plist);
	my $count = 0;
	$progress->show($count, $plist->{total});
	$plist->count_and($progress, \$count, $plist->{total}, 
	    $method, $state, @r);
}
1;
