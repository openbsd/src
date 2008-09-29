#!/usr/bin/perl -ws

# curliff.pl - convert certain files in the Perl distribution that
# need to be in CR-LF format to CR-LF, or back to LF format (with the
# -r option).  The CR-LF format is NOT to be used for checking in
# files to the Perforce repository, but it IS to be used when making
# Perl snapshots or releases.

use strict;

use vars qw($r);

# This list is also in makerel.
my @FILES = qw(
	       djgpp/configure.bat
	       README.ce
	       README.dos
	       README.symbian
	       README.win32
	       symbian/config.pl
	       symbian/makesis.pl
	       symbian/README
	       symbian/xsbuild.pl
	       win32/Makefile
	       win32/makefile.mk
	       win32/Makefile.ce
	       win32/ce-helpers/compile-all.bat
	       win32/ce-helpers/compile.bat
	       win32/ce-helpers/registry.bat
	       );

{
    local($^I, @ARGV) = ('.orig', @FILES);
    while (<>) {
	if ($r) {
	    s/\015\012/\012/;		# Curliffs to liffs.
	} else {
	    s/\015?\012/\015\012/;	# Curliffs and liffs to curliffs.
	}
        print;
        close ARGV if eof;              # Reset $.
    }
}
