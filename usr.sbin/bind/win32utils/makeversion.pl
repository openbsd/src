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

# $ISC: makeversion.pl,v 1.4 2001/07/27 17:25:23 gson Exp $ 

# This script takes the version information from the version file located
# at the root of the source tree and the api files in each library directory
# and writes the resulting information into a version.h file that the build
# process uses to build the executable code.
# This program was written by PDM. danny.mayer@nominum.com 1-Jul-2001.

# List of directories with version files
@dirlist = ("isc","dns","isccc","isccfg","lwres");
$LibMacros{"isc"} = "LIBISC_EXPORTS";
$LibMacros{"dns"} = "LIBDNS_EXPORTS";
$LibMacros{"isccc"} = "LIBISCCC_EXPORTS";
$LibMacros{"isccfg"} = "LIBISCCFG_EXPORTS";
$LibMacros{"lwres"} = "LIBLWRES_EXPORTS";

@VersionNames = ("LIBINTERFACE", "LIBREVISION", "LIBAGE");
$versionfile = "versions.h";
$versionpath = "../$versionfile";

#
# First get the version information
#
open (VERSIONFILE, "../version");
while (<VERSIONFILE>) {
	chomp;
	($data) = split(/\#/);
	if($data) {
		($name, $value) = split(/=/,$data);
		($name) = split(/\s+/, $name);
		($value) = split(/\s+/, $value);
		$Versions{$name} = $value;
	}
}
close(VERSIONFILE);

# Now set up the output version file

$ThisDate = scalar localtime();
open (OUTVERSIONFILE, ">$versionpath") ||
      die "Can't open output file $versionpath: $!";

#Standard Header

print OUTVERSIONFILE '/*
 * Copyright (C) 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

';

print OUTVERSIONFILE "/*\n";
print OUTVERSIONFILE " * $versionfile.";
print OUTVERSIONFILE "  Generated automatically by makeversion.pl.\n";
print OUTVERSIONFILE " * Date generated: $ThisDate\n";
print OUTVERSIONFILE " */\n\n";

print OUTVERSIONFILE '
#ifndef  VERSIONS_H
#define VERSIONS_H 1

';

$Version = "$Versions{'MAJORVER'}.$Versions{'MINORVER'}.$Versions{'PATCHVER'}";
$Version = "$Version$Versions{'RELEASETYPE'}$Versions{'RELEASEVER'}";
print "BIND Version: $Version\n";

print OUTVERSIONFILE "#define VERSION \"$Version\"\n\n";

foreach $dir (@dirlist) {
	$apifile = "../lib/$dir/api";
	open (APIVERSION, $apifile);
	while (<APIVERSION>) {
		chomp;
		($data) = split(/\#/);
		if ($data) {
			($name, $value) = split(/=/, $data);
			$name =~ s/\s+//;
			$value =~ s/\s+//;
			 $ApiVersions{$name} = $value;
		}
	}

	print OUTVERSIONFILE "\n#ifdef $LibMacros{$dir}\n";
	foreach $name (@VersionNames) {
		print OUTVERSIONFILE "#define $name\t$ApiVersions{$name}\n";
	}
	print OUTVERSIONFILE "#endif\n\n";
}

print OUTVERSIONFILE "#endif /* VERSIONS_H */\n";
close OUTVERSIONFILE;


