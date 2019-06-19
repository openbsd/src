# $OpenBSD: Library.pm,v 1.7 2018/12/11 05:45:14 semarie Exp $

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

use LT::LaFile;

package LT::LaFile;
sub link
{
	return LT::Linker::LaFile->new->link(@_);
}

package LT::Linker::LaFile;
our @ISA = qw(LT::Linker);

use LT::Util;
use LT::Trace;
use File::Basename;

sub link
{
	my ($linker, $self, $ltprog, $ltconfig, $la, $fname, $odir, $shared, 
	    $objs, $dirs, $libs, $deplibs, $libdirs, $parser, $gp) = @_;

	tsay {"creating link command for library (linked ",
		($shared) ? "dynamically" : "statically", ")"};

	my $RPdirs = $self->{RPdirs};

	my @libflags;
	my @cmd;
	my $dst = ($odir eq '.') ? "$ltdir/$fname" : "$odir/$ltdir/$fname";
	if ($la =~ m/\.a$/) {
		# probably just a convenience library
		$dst = ($odir eq '.') ? "$fname" : "$odir/$fname";
	}
	my $symlinkdir = $ltdir;
	if ($odir ne '.') {
		$symlinkdir = "$odir/$ltdir";
	}
	mkdir $symlinkdir if ! -d $symlinkdir;

	my ($staticlibs, $finalorderedlibs, $args) =
	    $linker->common1($parser, $gp, $deplibs, $libdirs, $dirs, $libs);

	# static linking
	if (!$shared) {
		@cmd = ('ar', 'cru', $dst);
		foreach my $a (@$staticlibs) {
			if ($a =~ m/\.a$/ && $a !~ m/_pic\.a/) {
				# extract objects from archive
				my $libfile = basename($a);
				my $xdir = "$odir/$ltdir/${la}x/$libfile";
				LT::Archive->extract($xdir, $a);
				my @kobjs = LT::Archive->get_objlist($a);
				map { $_ = "$xdir/$_"; } @kobjs;
				push @libflags, @kobjs;
			}
		}
		foreach my $k (@$finalorderedlibs) {
			my $l = $libs->{$k};
			# XXX improve test
			# this has to be done probably only with
			# convenience libraries
			next if !defined $l->{lafile};
			my $lainfo = LT::LaFile->parse($l->{lafile});
			next if ($lainfo->stringize('dlname') ne '');
			$l->resolve_library($dirs, 0, 0, ref($self));
			my $a = $l->{fullpath};
			if ($a =~ m/\.a$/ && $a !~ m/_pic\.a/) {
				# extract objects from archive
				my $libfile = basename $a;
				my $xdir = "$odir/$ltdir/${la}x/$libfile";
				LT::Archive->extract($xdir, $a);
				my @kobjs = LT::Archive->get_objlist($a);
				map { $_ = "$xdir/$_"; } @kobjs;
				push @libflags, @kobjs;
			}
		}
		push @cmd, @libflags if @libflags;
		push @cmd, @$objs if @$objs;
		my ($fh, $file);

		if (@cmd > 512) {
			use OpenBSD::MkTemp qw(mkstemp);
			my @extra = splice(@cmd, 512);
			($fh, $file) = mkstemp("/tmp/arargs.XXXXXXXXXXXX");
			print $fh map {"$_\n"} @extra;
			close $fh;
			push @cmd, "\@$file";
		}
		LT::Exec->link(@cmd);
		unlink($file) if defined $file;

		LT::Exec->link('ranlib', $dst);
		return;
	}

	my $tmp = [];
	while (my $k = shift @$finalorderedlibs) {
		my $l = $libs->{$k};
		$l->resolve_library($dirs, 1, $gp->static, ref($self));
		if ($l->{dropped}) {
			# remove library if dependency on it has been dropped
			delete $libs->{$k};
		} else {
			push(@$tmp, $k);
		}
	}
	$finalorderedlibs = $tmp;

	my @libobjects = values %$libs;
	tsay {"libs:\n", join("\n", (keys %$libs))};
	tsay {"libfiles:\n", 
	    join("\n", map { $_->{fullpath}//'UNDEF' } @libobjects) };

	$linker->create_symlinks($symlinkdir, $libs);
	my $prev_was_archive = 0;
	my $libcounter = 0;
	foreach my $k (@$finalorderedlibs) {
		my $a = $libs->{$k}->{fullpath} || die "Link error: $k not found in \$libs\n";
		if ($a =~ m/\.a$/) {
			# don't make a -lfoo out of a static library
			push @libflags, '-Wl,-whole-archive' unless $prev_was_archive;
			push @libflags, $a;
			if ($libcounter == @$finalorderedlibs - 1) {
				push @libflags, '-Wl,-no-whole-archive';
			}
			$prev_was_archive = 1;
		} else {
			push @libflags, '-Wl,-no-whole-archive' if $prev_was_archive;
			$prev_was_archive = 0;
			push @libflags, $linker->infer_libparameter($a, $k);
		}
		$libcounter++;
	}

	# add libdirs to rpath if they are not in standard lib path
	for my $l (@$libdirs) {
		if (!LT::OSConfig->is_search_dir($l)) {
			push @$RPdirs, $l;
		}
	}

	my @linkeropts = ();
	if (!$ltconfig->noshared) {
		push(@linkeropts, '-soname', $fname);
		for my $d (@$RPdirs) {
			push(@linkeropts, '-rpath', $d);
		}
	}

	@cmd = @$ltprog;
	push @cmd, $ltconfig->sharedflag, @{$ltconfig->picflags};
	push @cmd, '-o', $dst;
	push @cmd, '-pthread' if $parser->{pthread};
	push @cmd, @$args if $args;
	push @cmd, @$objs if @$objs;
	push @cmd, '-Wl,-whole-archive', @$staticlibs, '-Wl,-no-whole-archive'
	    if @$staticlibs;
	push @cmd, "-L$symlinkdir", @libflags if @libflags;

	my @e = $linker->export_symbols($ltconfig, 
	    "$odir/$ltdir/$la", $gp, @$objs, @$staticlibs);
	push(@cmd, join(',', "-Wl", @e)) if @e;
	push @cmd, join(',', "-Wl", @linkeropts) if @linkeropts;
	LT::Exec->link(@cmd);
}

1;
