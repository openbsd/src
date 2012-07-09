# ex:ts=8 sw=4:
# $OpenBSD: Compile.pm,v 1.5 2012/07/09 10:52:26 espie Exp $
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
	my ($class, $ltprog, $gp, $noshared) = @_;
	my $lofile = LT::LoFile->new;

	$gp->handle_permuted_options('o:',
		'prefer-pic', 'prefer-non-pic', 'static');
	# XXX options ignored: -prefer-pic and -prefer-non-pic
	my $pic = 0;
	my $nonpic = 1;
	# assume we need to build pic objects
	$pic = 1 if (!$noshared);
	$nonpic = 0 if ($pic && $gp->has_tag('disable-static'));
	$pic = 0 if ($nonpic && $gp->has_tag('disable-shared'));
	$nonpic = 1 if $gp->static;

	my ($outfile, $odir, $ofile, $srcfile, $srcext);
	# XXX check whether -c flag is present and if not, die?
	if ($gp->{opt}{o}) {
		# fix extension if needed
		($outfile = $gp->{opt}{o}) =~ s/\.o$/.lo/;
		$odir = dirname($outfile);
		$ofile = basename($outfile);
	} else {
		# XXX sometimes no -o flag is present and we need another way
		my $srcre = join '|', @valid_src;
		my $found = 0;
		foreach my $a (@ARGV) {
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
	$lofile->compile($ltprog, $odir, \@ARGV);
	$lofile->write($outfile);
}

1;
