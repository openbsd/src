# ex:ts=8 sw=4:
# $OpenBSD: Compile.pm,v 1.1 2012/06/24 13:44:53 espie Exp $
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

use File::Basename;
use LT::LoFile;
use LT::Util;

my @valid_src = qw(asm c cc cpp cxx f s);
my %opts;
sub run
{
	my ($class, $ltprog, $gp, $tags, $noshared) = @_;
	my $lofile = LT::LoFile->new;

	$gp->getoptions('o=s'		=> \$opts{'o'},
			'prefer-pic'	=> \$opts{'prefer-pic'},
			'prefer-non-pic'=> \$opts{'prefer-non-pic'},
			'static'	=> \$opts{'static'},
			);
	# XXX options ignored: -prefer-pic and -prefer-non-pic
	my $pic = 0;
	my $nonpic = 1;
	# assume we need to build pic objects
	$pic = 1 if (!$noshared);
	$nonpic = 0 if ($pic && grep { $_ eq 'disable-static' } @$tags);
	$pic = 0 if ($nonpic && grep { $_ eq 'disable-shared' } @$tags);
	$nonpic = 1 if ($opts{'static'});

	my ($outfile, $odir, $ofile, $srcfile, $srcext);
	# XXX check whether -c flag is present and if not, die?
	if ($opts{'o'}) {
		# fix extension if needed
		($outfile = $opts{'o'}) =~ s/\.o$/.lo/;
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
		($ofile = basename $srcfile) =~ s/\.($srcext)$/.lo/i;
		$outfile = "$odir/$ofile";
	}
	LT::Trace::debug {"srcfile = $srcfile\n"} if $srcfile;
	LT::Trace::debug {"outfile = $outfile\n"};
	(my $nonpicobj = $ofile) =~ s/\.lo$/.o/;
	my $picobj = "$ltdir/$nonpicobj";

	$lofile->{picobj} = $picobj if $pic;
	$lofile->{nonpicobj} = $nonpicobj if $nonpic;
	$lofile->compile($ltprog, $odir, \@ARGV);
	$lofile->write($outfile);
}

1;
