#!/usr/bin/perl
#
# Copyright (C) 2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: makedefs.pl,v 1.5 2001/07/31 00:03:19 gson Exp $

# makedefs.pl
# This script goes through all of the lib header files and creates a .def file
# for each DLL for Win32. It recurses as necessary through the subdirectories
#
# This program should only be run if it is necessary to regenerate
# the .def files.  Normally these files should be updated by  hand, adding
# new functions to the end and removing obsolete ones.
# If you do renerate them you will also need to modify them by hand to
# to pick up those routines not detected by this program (like openlog).
#
# Search String: ^(([_a-z0-9])*( ))*prefix_[_a-z0-9]+_[a-z0-9]+( )*\(
# List of directories

@prefixlist = ("isc", "isccfg","dns", "isccc", "libres");
@prefixlist = ("isccc");
@iscdirlist = ("isc/include/isc","isc/win32/include/isc");
@iscprefixlist = ("isc", "isc", "cfg");

@isccfgdirlist = ("isccfg/include/isccfg");
@isccfgprefixlist = ("cfg");

@iscccdirlist = ("isccc/include/isccc");
@iscccprefixlist = ("isccc");

@dnsdirlist = ("dns/include/dns","dns/sec/dst/include/dst");
@dnsprefixlist = ("dns", "dst");

@lwresdirlist = ("lwres/include/lwres");
@lwresprefixlist = ("lwres");

# Run the changes for each directory in the directory list 

$ind = 0;
createoutfile($iscprefixlist[0]);
foreach $dir (@iscdirlist) {
	createdeffile($dir, $iscprefixlist[$ind]);
	$ind++;
}
close OUTDEFFILE;

$ind = 0;
createoutfile($isccfgprefixlist[0]);
foreach $dir (@isccfgdirlist) {
	createdeffile($dir, $isccfgprefixlist[$ind]);
	$ind++;
}
close OUTDEFFILE;

$ind = 0;
createoutfile($dnsprefixlist[0]);
foreach $dir (@dnsdirlist) {
	createdeffile($dir, $dnsprefixlist[$ind]);
	$ind++;
}
close OUTDEFFILE;

$ind = 0;
createoutfile($iscccprefixlist[0]);
foreach $dir (@iscccdirlist) {
	createdeffile($dir, $iscccprefixlist[$ind]);
	$ind++;
}
close OUTDEFFILE;

$ind = 0;
createoutfile($lwresprefixlist[0]);
foreach $dir (@lwresdirlist) {
	createdeffile($dir, $lwresprefixlist[$ind]);
	$ind++;
}
close OUTDEFFILE;

exit;

#
# Subroutines
#
sub createdeffile {
	$xdir = $_[0];

	#
	# Get the List of files in the directory to be processed.
	#
	#^(([_a-z0-9])*( ))*prefix_[_a-z]+_[a-z]+( )*\(
	$prefix = $_[1];
	$pattern = "\^\(\(\[\_a\-z0\-9\]\)\*\( \)\)\*\(\\*\( \)\+\)\*$prefix";
	$pattern = "$pattern\_\[\_a\-z0\-9\]\+_\[a\-z0\-9\]\+\( \)\*\\\(";

	opendir(DIR,$xdir) || die "No Directory: $!";
	@files = grep(/\.h$/i, readdir(DIR));
	closedir(DIR);

	foreach $filename (sort @files) {
		#
		# Open the file and locate the pattern.
		#
		open (HFILE, "$xdir/$filename") ||
		      die "Can't open file $filename : $!";

		while (<HFILE>) {
			if(/$pattern/) {
				$func = $&;
				chop($func);
				$space = rindex($func, " ") + 1;
				if($space >= 0) {
					# strip out return values
					$func = substr($func, $space, 100);
				}
				print OUTDEFFILE "$func\n";
			}
		}
		# Set up the Patterns
		close(HFILE);
	}
}

# This is the routine that applies the changes

# output the result to the platform specific directory.
sub createoutfile {
	$outfile = "lib$_[0].def";

	open (OUTDEFFILE, ">$outfile")
	    || die "Can't open output file $outfile: $!";
	print OUTDEFFILE "LIBRARY lib$_[0]\n";
	print OUTDEFFILE "\n";
	print OUTDEFFILE "; Exported Functions\n";
	print OUTDEFFILE "EXPORTS\n";
	print OUTDEFFILE "\n";
}
