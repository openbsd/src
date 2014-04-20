# $OpenBSD: Program.pm,v 1.6 2014/04/20 17:34:26 zhuk Exp $

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

use LT::Program;

package LT::Program;

sub link
{
	return LT::Linker::Program->new->link(@_);
}

package LT::Linker::Program;
our @ISA = qw(LT::Linker);

use LT::Trace;
use LT::Util;
use File::Basename;

sub link
{
	my ($linker, $self, $ltprog, $ltconfig, $dirs, $libs, $deplibs, 
	    $libdirs, $parser, $gp) = @_;

	tsay {"linking program (", ($gp->static ? "not " : ""),
	    "dynamically linking not-installed libtool libraries)"};

	my $fpath  = $self->{outfilepath};
	my $RPdirs = $self->{RPdirs};

	my $odir  = dirname($fpath);
	my $fname = basename($fpath);

	my @libflags;
	my @cmd;
	my $dst;

	my ($staticlibs, $finalorderedlibs, $args) =
	    $linker->common1($parser, $gp, $deplibs, $libdirs, $dirs, $libs);

	my $symlinkdir = $ltdir;
	if ($odir ne '.') {
		$symlinkdir = "$odir/$ltdir";
	}
	mkdir $symlinkdir if ! -d $symlinkdir;
	if ($parser->{seen_la_shared}) {
		$dst = ($odir eq '.') ? "$ltdir/$fname" : "$odir/$ltdir/$fname";
		$self->write_wrapper;
	} else {
		$dst = ($odir eq '.') ? $fname : "$odir/$fname";
	}

	my $rpath_link = LT::UList->new;
	# add libdirs to rpath if they are not in standard lib path
	for my $l (@$libdirs) {
		if (LT::OSConfig->is_search_dir($l)) {
			push @$rpath_link, $l;
		} else {
			push @$RPdirs, $l;
		}
	}
	foreach my $k (keys %$libs) {
		tprint {"key = $k - "};
		my $r = ref($libs->{$k});
		tsay {"ref = $r"};
		$libs->create($k)->resolve_library($dirs, 1, $gp->static, 
		    ref($self));
	}

	my @libobjects = values %$libs;
	tsay {"libs:\n", join("\n", keys %$libs)};
	tsay {"libfiles:\n", join("\n", map { $_->{fullpath} } @libobjects)};

	$linker->create_symlinks($symlinkdir, $libs);
	foreach my $k (@$finalorderedlibs) {
		my $a = $libs->{$k}->{fullpath} || die "Link error: $k not found in \$libs\n";
		if ($a =~ m/\.a$/) {
			# don't make a -lfoo out of a static library
			push @libflags, $a;
		} else {
			push @libflags, $linker->infer_libparameter($a, $k);
		}
	}

	my @linkeropts = ();
	if (!$ltconfig->noshared) {
		for my $d (@$RPdirs) {
			push(@linkeropts, '-rpath', $d);
		}
		for my $d (@$rpath_link) {
			push(@linkeropts, '-rpath-link', $d);
		}
	}

	push(@linkeropts, $linker->export_symbols($ltconfig, 
	    "$odir/$ltdir/$fname", $gp, @{$self->{objlist}}, @$staticlibs));

	@cmd = @$ltprog;
	push @cmd, '-o', $dst;
	push @cmd, '-pthread' if $parser->{pthread};
	push @cmd, @$args if $args;
	push @cmd, @{$self->{objlist}} if @{$self->{objlist}};
	push @cmd, @$staticlibs if @$staticlibs;
	push @cmd, "-L$symlinkdir", @libflags if @libflags;
	push @cmd, join(',', '-Wl', @linkeropts) if @linkeropts;
	LT::Exec->link(@cmd);
}
1;
