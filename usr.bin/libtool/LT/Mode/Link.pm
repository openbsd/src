# ex:ts=8 sw=4:
# $OpenBSD: Link.pm,v 1.7 2012/07/10 13:32:10 espie Exp $
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
use feature qw(say);

package LT::Mode::Link;
our @ISA = qw(LT::Mode);

use LT::Util;
use LT::Parser;
use LT::Trace;
use File::Basename;

use constant {
	OBJECT	=> 0, # unused ?
	LIBRARY	=> 1,
	PROGRAM	=> 2,
};

sub help
{
	print <<"EOH";

Usage: $0 --mode=link LINK-COMMAND ...
Link object files and libraries into a library or a program
EOH
}

my $shared = 0;
my $static = 1;
my @libsearchdirs;

sub run
{
	my ($class, $ltprog, $gp, $ltconfig) = @_;

	my $noshared  = $ltconfig->noshared;
	my $cmd;
	my @Rresolved;		# -R options originating from .la resolution
	my $libdirs = [];	# list of libdirs
	my $libs = {};		# libraries
	my $dirs = {};		# paths to find libraries
	# put a priority in the dir hash
	# always look here
	$dirs->{'/usr/lib'} = 3;

	$gp->handle_permuted_options(
	    'all-static',
	    'allow-undefined', # we don't care about THAT one
	    'avoid-version',
	    'dlopen:',
	    'dlpreopen:',
	    'export-dynamic',
	    'export-symbols:',
	    'export-symbols-regex:',
	    'module',
	    'no-fast-install',
	    'no-install',
	    'no-undefined',
	    'o:@',
	    'objectlist:',
	    'precious-files-regex:',
	    'prefer-pic',
	    'prefer-non-pic',
	    'release:',
	    'rpath:@',
	    'R:@',
	    'shrext:',
	    'static',
	    'thread-safe', # XXX and --thread-safe ?
	    'version-info:',
	    'version-number:');

	# XXX options ignored: dlopen, dlpreopen, no-fast-install,
	# 	no-install, no-undefined, precious-files-regex,
	# 	shrext, thread-safe, prefer-pic, prefer-non-pic

	my @RPopts = $gp->rpath;	 # -rpath options
	my @Ropts = $gp->R;		 # -R options on the command line

	@libsearchdirs = get_search_dirs();
	# add the .libs dir as well in case people try to link directly
	# with the real library instead of the .la library
	push @libsearchdirs, './.libs';

	if (!$gp->o) {
		shortdie "No output file given.\n";
	}
	if ($gp->o > 1) {
		shortdie "Multiple output files given.\n";
	}

	my $outfile = ($gp->o)[0];
	tsay {"outfile = $outfile"};
	my $odir = dirname($outfile);
	my $ofile = basename($outfile);

	# what are we linking?
	my $linkmode = PROGRAM;
	if ($ofile =~ m/\.l?a$/) {
		$linkmode = LIBRARY;
	}
	tsay {"linkmode: $linkmode"};

	# eat multiple version-info arguments, we only accept the first.
	map { $_ = '' if ($_ =~ m/\d+:\d+:\d+/); } @ARGV;

	my @objs;
	my @sobjs;
	if ($gp->objectlist) {
		my $objectlist = $gp->objectlist;
		open(my $ol, '<', $objectlist) or die "Cannot open $objectlist: $!\n";
		my @objlist = <$ol>;
		for (@objlist) { chomp; }
		generate_objlist(\@objs, \@sobjs, \@objlist);
	} else {
		generate_objlist(\@objs, \@sobjs, \@ARGV);
	}
	tsay {"objs = @objs"};
	tsay {"sobjs = @sobjs"};

	my $deplibs = [];	# list of dependent libraries (both -L and -l flags)
	my $parser = LT::Parser->new(\@ARGV);
	$parser->{result} = [];


	if ($linkmode == PROGRAM) {
		require LT::Program;
		my $program = LT::Program->new;
		$program->{outfilepath} = $outfile;
		# XXX give higher priority to dirs of not installed libs
		if ($gp->export_dynamic) {
			push(@{$parser->{args}}, "-Wl,-E");
		}

		$parser->parse_linkargs1($deplibs, \@Rresolved, \@libsearchdirs,
				$dirs, $libs, $parser->{args}, 0);
		$parser->{args} = $parser->{result};
		tsay {"end parse_linkargs1"};
		tsay {"deplibs = @$deplibs"};

		$program->{objlist} = \@objs;
		if (@objs == 0) {
			if (@sobjs > 0) {
				tsay {"no non-pic libtool objects found, trying pic objects..."};
				$program->{objlist} = \@sobjs;
			} elsif (@sobjs == 0) {
				tsay {"no libtool objects of any kind found"};
				tsay {"hoping for real objects in ARGV..."};
			}
		}
		my $RPdirs = [];
		@$RPdirs = (@Ropts, @RPopts, @Rresolved);
		$program->{RPdirs} = $RPdirs;

		$program->link($ltprog, $ltconfig, $dirs, $libs, $deplibs, $libdirs, $parser, $gp);
	} elsif ($linkmode == LIBRARY) {
		my $convenience = 0;
		require LT::LaFile;
		my $lainfo = LT::LaFile->new;

		$shared = 1 if ($gp->version_info ||
				$gp->avoid_version ||
				$gp->module);
		if (!@RPopts) {
			$convenience = 1;
			$noshared = 1;
			$static = 1;
			$shared = 0;
		} else {
			$shared = 1;
		}
		if ($ofile =~ m/\.a$/ && !$convenience) {
			$ofile =~ s/\.a$/.la/;
			$outfile =~ s/\.a$/.la/;
		}
		(my $libname = $ofile) =~ s/\.l?a$//;	# remove extension
		my $staticlib = $libname.'.a';
		my $sharedlib = $libname.'.so';
		my $sharedlib_symlink;

		if ($gp->static || $gp->all_static) {
			$shared = 0;
			$static = 1;
		}
		$shared = 0 if $noshared;

		$parser->parse_linkargs1($deplibs, \@Rresolved, \@libsearchdirs,
				$dirs, $libs, $parser->{args}, 0);
		$parser->{args} = $parser->{result};
		tsay {"end parse_linkargs1"};
		tsay {"deplibs = @$deplibs"};

		my $sover = '0.0';
		my $origver = 'unknown';
		# environment overrides -version-info
		(my $envlibname = $libname) =~ s/[.+-]/_/g;
		my ($current, $revision, $age) = (0, 0, 0);
		if ($gp->version_info) {
			($current, $revision, $age) = parse_version_info($gp->version_info);
			$origver = "$current.$revision";
			$sover = $origver;
		}
		if ($ENV{"${envlibname}_ltversion"}) {
			# this takes priority over the previous
			$sover = $ENV{"${envlibname}_ltversion"};
			($current, $revision) = split /\./, $sover;
			$age = 0;
		}
		if ($gp->release) {
			$sharedlib_symlink = $sharedlib;
			$sharedlib = $libname.'-'.$gp->release.'.so';
		}
		if ($gp->avoid_version ||
			($gp->release && !$gp->version_info)) {
			# don't add a version in these cases
		} else {
			$sharedlib .= ".$sover";
			if ($gp->release) {
				$sharedlib_symlink .= ".$sover";
			}
		}

		# XXX add error condition somewhere...
		$static = 0 if $shared && $gp->has_tag('disable-static');
		$shared = 0 if $static && $gp->has_tag('disable-shared');

		tsay {"SHARED: $shared\nSTATIC: $static"};

		$lainfo->{'libname'} = $libname;
		if ($shared) {
			$lainfo->{'dlname'} = $sharedlib;
			$lainfo->{'library_names'} = $sharedlib;
			$lainfo->{'library_names'} .= " $sharedlib_symlink"
				if $gp->release;
			$lainfo->link($ltprog, $ltconfig, $ofile, $sharedlib, $odir, 1, \@sobjs, $dirs, $libs, $deplibs, $libdirs, $parser, $gp);
			tsay {"sharedlib: $sharedlib"};
			$lainfo->{'current'} = $current;
			$lainfo->{'revision'} = $revision;
			$lainfo->{'age'} = $age;
		}
		if ($static) {
			$lainfo->{'old_library'} = $staticlib;
			$lainfo->link($ltprog, $ltconfig, $ofile, $staticlib, $odir, 0, ($convenience && @sobjs > 0) ? \@sobjs : \@objs, $dirs, $libs, $deplibs, $libdirs, $parser, $gp);
			tsay {($convenience ? "convenience" : "static"),
			    " lib: $staticlib"};
		}
		$lainfo->{installed} = 'no';
		$lainfo->{shouldnotlink} = $gp->module ? 'yes' : 'no';
		map { $_ = "-R$_" } @Ropts;
		unshift @$deplibs, @Ropts if @Ropts;
		tsay {"deplibs = @$deplibs"};
		my $finaldeplibs = reverse_zap_duplicates_ref($deplibs);
		tsay {"finaldeplibs = @$finaldeplibs"};
		$lainfo->set('dependency_libs', "@$finaldeplibs");
		if (@RPopts) {
			if (@RPopts > 1) {
				tsay {"more than 1 -rpath option given, ",
				    "taking the first: ", $RPopts[0]};
			}
			$lainfo->{'libdir'} = $RPopts[0];
		}
		if (!($convenience && $ofile =~ m/\.a$/)) {
			$lainfo->write($outfile, $ofile);
			unlink("$odir/$ltdir/$ofile");
			symlink("../$ofile", "$odir/$ltdir/$ofile");
		}
		my $lai = "$odir/$ltdir/$ofile".'i';
		if ($shared) {
			my $pdeplibs = process_deplibs($finaldeplibs);
			if (defined $pdeplibs) {
				$lainfo->set('dependency_libs', "@$pdeplibs");
			}
			if (! $gp->module) {
				$lainfo->write_shared_libs_log($origver);
			}
		}
		$lainfo->{'installed'} = 'yes';
		# write .lai file (.la file that will be installed)
		$lainfo->write($lai, $ofile);
	}
}

# XXX reuse code from SharedLibs.pm instead
sub get_search_dirs
{
	my @libsearchdirs;
	open(my $fh, '-|', '/sbin/ldconfig -r');
	if (defined $fh) {
		while (<$fh>) {
			if (m/^\s*search directories:\s*(.*?)\s*$/o) {
				foreach my $d (split(/\:/o, $1)) {
					push @libsearchdirs, $d;
				}
				last;
			}
		}
		close($fh);
	} else {
		die "Can't run ldconfig\n";
        }
	return @libsearchdirs;
}

# populate arrays of non-pic and pic objects and remove these from @ARGV
sub generate_objlist
{
	my $objs = shift;
	my $sobjs = shift;
	my $objsource = shift;

	my $result = [];
	foreach my $a (@$objsource) {
		if ($a =~ m/\S+\.lo$/) {
			require LT::LoFile;
			my $ofile = basename($a);
			my $odir = dirname($a);
			my $loinfo = LT::LoFile->parse($a);
			if ($loinfo->{'non_pic_object'}) {
				my $o;
				$o .= "$odir/" if ($odir ne '.');
				$o .= $loinfo->{'non_pic_object'};
				push @$objs, $o;
			}
			if ($loinfo->{'pic_object'}) {
				my $o;
				$o .= "$odir/" if ($odir ne '.');
				$o .= $loinfo->{'pic_object'};
				push @$sobjs, $o;
			}
		} elsif ($a =~ m/\S+\.o$/) {
			push @$objs, $a;
		} else {
			push @$result, $a;
		}
	}
	@$objsource = @$result;
}

# convert 4:5:8 into a list of numbers
sub parse_version_info
{
	my $vinfo = shift;

	if ($vinfo =~ m/^(\d+):(\d+):(\d+)$/) {
		return ($1, $2, $3);
	} elsif ($vinfo =~ m/^(\d+):(\d+)$/) {
		return ($1, $2, 0);
	} elsif ($vinfo =~ m/^(\d+)$/) {
		return ($1, 0, 0);
	} else {
		die "Error parsing -version-info $vinfo\n";
	}
}

# prepare dependency_libs information for the .la file which is installed
# i.e. remove any .libs directories and use the final libdir for all the
# .la files
sub process_deplibs
{
	my $linkflags = shift;

	my $result;

	foreach my $lf (@$linkflags) {
		if ($lf =~ m/-L\S+\Q$ltdir\E$/) {
		} elsif ($lf =~ m/-L\./) {
		} elsif ($lf =~ m/\/\S+\/(\S+\.la)/) {
			my $lafile = $1;
			require LT::LaFile;
			my $libdir = LT::LaFile->parse($lf)->{'libdir'};
			if ($libdir eq '') {
				# this drops libraries which will not be
				# installed
				# XXX improve checks when adding to deplibs
				say "warning: $lf dropped from deplibs";
			} else {
				$lf = $libdir.'/'.$lafile;
				push @$result, $lf;
			}
		} else {
			push @$result, $lf;
		}
	}
	return $result;
}


1;

