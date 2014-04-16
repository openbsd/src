# ex:ts=8 sw=4:
# $OpenBSD: Compile.pm,v 1.13 2014/04/16 14:39:06 zhuk Exp $
#
# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
# Copyright (c) 2012 Marc Espie <espie@openbsd.org>
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

package LT::Mode::Compile;
our @ISA = qw(LT::Mode);

use File::Basename;
use LT::LoFile;
use LT::Util;
use LT::Trace;

sub help
{
	print <<"EOH";

Usage: $0 --mode=compile COMPILE-COMMAND [opts] SOURCE
Compile source file into library object

  -o OUTPUT-FILE
EOH
}

my @valid_src = qw(asm c cc cpp cxx f s);
sub run
{
	my ($class, $ltprog, $gp, $ltconfig) = @_;
	my $lofile = LT::LoFile->new;

	my $pic = !$ltconfig->noshared;
	my $nonpic = 1;
	if ($gp->has_tag('disable-shared')) {
		$pic = 0;
	}
	if ($gp->has_tag('disable-static') && $pic) {
		$nonpic = 0;
	}

	my $pic_mode = 0;

	my @pie_flags = ();

	$gp->handle_permuted_options('o:!@',
		qr{\-Wc\,(.*)}, 
		    sub { 
			$gp->keep_for_later(split(/\,/, shift));
		    },
		'Xcompiler:', 
		    sub {
			$gp->keep_for_later($_[2]);
		    },
		'pie|fpie|fPIE',
		    sub {
			push(@pie_flags, $_[3]);
		    },
		'no-suppress', # we just ignore that one
		'prefer-pic', sub { $pic_mode = 1; },
		'prefer-non-pic', sub { $pic_mode =  0; },
		'static', 
		    sub { 
			$pic = 0; 
			$nonpic = 1;
		    },
		'shared', 
		    sub {
			if (!$pic) {
				shortdie "bad configuration: can't build shared library";
			}
			$nonpic = 0;
		    });

	my ($outfile, $odir, $ofile, $srcfile, $srcext);
	# XXX check whether -c flag is present and if not, die?
	if ($gp->o) {
		if ($gp->o > 1) {
			shortdie "Can't specify -o more than once\n";
		}
		# fix extension if needed
		($outfile = ($gp->o)[0]) =~ s/\.o$/.lo/;
		$odir = dirname($outfile);
		$ofile = basename($outfile);
	} else {
		# XXX sometimes no -o flag is present and we need another way
		my $srcre = join '|', @valid_src;
		my $found = 0;
		foreach my $a (@main::ARGV) {
			if ($a =~ m/\.($srcre)$/i) {
				$srcfile = $a;
				$srcext = $1;
				$found = 1;
				last;
			}
		}
		$found or die "Cannot find source file in command\n";
		# the output file ends up in the current directory
		$odir = '.';
		($ofile = basename($srcfile)) =~ s/\.($srcext)$/.lo/i;
		$outfile = "$odir/$ofile";
	}
	tsay {"srcfile = $srcfile"} if $srcfile;
	tsay {"outfile = $outfile"};
	(my $nonpicobj = $ofile) =~ s/\.lo$/.o/;
	my $picobj = "$ltdir/$nonpicobj";

	$lofile->{picobj} = $picobj if $pic;
	$lofile->{nonpicobj} = $nonpicobj if $nonpic;
	$lofile->{picflags} = $ltconfig->picflags;
	if ($pic_mode) {
		$lofile->{nonpicflags} = $ltconfig->picflags;
	} else {
		$lofile->{nonpicflags} = \@pie_flags;
	}
	$lofile->compile($ltprog, $odir, \@ARGV);
	$lofile->write($outfile);
}

1;
