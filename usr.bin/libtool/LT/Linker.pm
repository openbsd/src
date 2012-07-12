# $OpenBSD: Linker.pm,v 1.4 2012/07/12 09:43:34 espie Exp $

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
use feature qw(say);

package LT::Linker;
use LT::Trace;
use LT::Util;
use File::Basename;
use Cwd qw(abs_path);

sub new
{
	my $class = shift;
	bless {}, $class;
}

sub create_symlinks
{
	my ($self, $dir, $libs) = @_;
	if (! -d $dir) {
		mkdir($dir) or die "Cannot mkdir($dir) : $!\n";
	}

	foreach my $l (values %$libs) {
		my $f = $l->{fullpath};
		next if !defined $f;
		next if $f =~ m/\.a$/;
		my $libnames = [];
		if (defined $l->{lafile}) {
			require LT::LaFile;
			my $lainfo = LT::LaFile->parse($l->{lafile});
			my $librarynames = $lainfo->stringize('library_names');
			@$libnames = split /\s/, $librarynames;
			$libnames = reverse_zap_duplicates_ref($libnames);
		} else {
			push @$libnames, basename($f);
		}	
		foreach my $libfile (@$libnames) {
			my $link = "$dir/$libfile";
			tsay {"ln -s $f $link"};
			next if -f $link;
			my $p = abs_path($f);
			if (!symlink($p, $link)) {
				die "Cannot create symlink($p, $link): $!\n"
				    unless  $!{EEXIST};
			}
		}
	}
	return $dir;
}

sub common1
{
	my ($self, $parser, $gp, $deplibs, $libdirs, $dirs, $libs) = @_;

	$parser->resolve_la($deplibs, $libdirs);
	my $orderedlibs = [];
	my $staticlibs = [];
	my $args = $parser->parse_linkargs2($gp, $orderedlibs, $staticlibs, $dirs, 
	    $libs);
	tsay {"staticlibs = \n", join("\n", @$staticlibs)};
	tsay {"orderedlibs = @$orderedlibs"};
	$orderedlibs = reverse_zap_duplicates_ref($orderedlibs);
	tsay {"final orderedlibs = @$orderedlibs"};
	return ($staticlibs, $orderedlibs, $args);
}
1;


