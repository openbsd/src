#!/usr/bin/perl
#
# Copyright (c) 2021 Ingo Schwarze <schwarze@openbsd.org>
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

my @obsolete = qw(
    d2i_PBEPARAM d2i_PBE2PARAM d2i_PBKDF2PARAM
    i2d_PBEPARAM i2d_PBE2PARAM i2d_PBKDF2PARAM
    PBEPARAM PBEPARAM_free PBEPARAM_new
    PBE2PARAM PBE2PARAM_free PBE2PARAM_new
    PBKDF2PARAM PBKDF2PARAM_free PBKDF2PARAM_new
    PKCS5_pbe_set PKCS5_pbe_set0_algor
    PKCS5_pbe2_set PKCS5_pbe2_set_iv
    PKCS5_pbkdf2_set
    X509_EX_V_INIT
    X509_EXT_PACK_STRING X509_EXT_PACK_UNKNOWN
);

my $MANW = 'man -M /usr/share/man -w';
my $srcdir = '/usr/src/lib/libcrypto/man';
my $hfile = '/usr/include/openssl';

my $in_cplusplus = 0;
my $in_comment = 0;
my $in_define = 0;
my $in_function = 0;
my $in_struct = 0;
my $in_typedef_struct = 0;
my $verbose = 0;

if (defined $ARGV[0] && $ARGV[0] eq '-v') {
	$verbose = 1;
	shift @ARGV;
}
$#ARGV == 0 or die "usage: $0 [-v] headername";
$hfile .= "/$ARGV[0].h";
open my $in_fh, '<', $hfile or die "$hfile: $!";

while (<$in_fh>) {
try_again:
	chomp;
	my $line = $_;

	# C language comments.

	if ($in_comment) {
		unless (s/.*?\*\///) {
			print "-- $line\n" if $verbose;
			next;
		}
		$in_comment = 0;
	}
	while (/\/\*/) {
		s/\/\*.*?\*\/// and next;
		s/\/\*.*// and $in_comment = 1;
	}

	# End C++ stuff.

	if ($in_cplusplus) {
		/^#endif$/ and $in_cplusplus = 0;
		print "-- $line\n" if $verbose;
		next;
	}

	# End declarations of structs.

	if ($in_struct) {
		unless (s/^\s*\}//) {
			print "-s $line\n" if $verbose;
			next;
		}
		$in_struct = 0;
		unless ($in_typedef_struct) {
			/^\s*;$/ or die "at end of struct: $_";
			print "-s $line\n" if $verbose;
			next;
		}
		$in_typedef_struct = 0;
		my ($id) = /^\s*(\w+);$/
		    or die "at end of typedef struct: $_";
		unless (system "$MANW -k Vt=$id > /dev/null 2>&1") {
			print "Vt $line\n" if $verbose;
			next;
		}
		if ($id =~ /NETSCAPE/) {
			print "V- $line\n" if $verbose;
			next;
		}
		if (grep { $_ eq $id } @obsolete) {
			print "V- $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: typedef struct $id";
		}
		next;
	}

	# End macro definitions.

	if ($in_define) {
		/\\$/ or $in_define = 0;
		print "-d $line\n" if $verbose;
		next;
	}

	# End function declarations.

	if ($in_function) {
		/^\s/ or die "function arguments not indented: $_";
		/\);$/ and $in_function = 0;
		print "-f $line\n" if $verbose;
		next;
	}

	# Begin C++ stuff.

	if (/^#ifdef\s+__cplusplus$/) {
		$in_cplusplus = 1;
		print "-- $line\n" if $verbose;
		next;
	}

	# Uninteresting lines.

	if (/^\s*$/ ||
	    /^DECLARE_STACK_OF\(\w+\)$/ ||
	    /^#define HEADER_\w+_H$/ ||
	    /^#endif$/ ||
	    /^extern\s+const\s+ASN1_ITEM\s+\w+_it;$/ ||
	    /^#include\s/ ||
	    /^#ifn?def\s/) {
		print "-- $line\n" if $verbose;
		next;
	}

	# Begin declarations of structs.

	if (/^(typedef )?(?:struct|enum)(?: \w+)? \{$/) {
		$in_struct = 1;
		$1 and $in_typedef_struct = 1;
		print "-s $line\n" if $verbose;
		next;
	}

	if (my ($id) = /^#define\s+(\w+)\s+\S/) {
		/\\$/ and $in_define = 1;
		unless (system "$MANW -k Dv=$id > /dev/null 2>&1") {
			print "Dv $line\n" if $verbose;
			next;
		}
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		unless (system qw/grep -qR/, '^\.\\\\" ' . $id . '\>',
		    "$srcdir/") {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /NETSCAPE/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /^X509_[FR]_\w+$/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /^X509_V_ERR_\w+$/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if (grep { $_ eq $id } @obsolete) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: #define $id";
		}
		next;
	}
	if (my ($id) = /^#define\s+(\w+)\(/) {
		/\\$/ and $in_define = 1;
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		unless (system qw/grep -qR/, '^\.\\\\" .*' . $id . '(3)',
		    "$srcdir/") {
			print "F- $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: #define $id()";
		}
		next;
	}
	if (/^typedef\s+(?:struct\s+)?\S+\s+(\w+);$/) {
		unless (system "$MANW -k Vt=$1 > /dev/null 2>&1") {
			print "Vt $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: typedef $1";
		}
		next;
	}
	if (/^\w+(?:\(\w+\))?(?:\s+\w+)?(?:\s+|\s*\(?\*\s*)(\w+)\s*\(/) {
		my $id = $1;
		/\);$/ or $in_function = 1;
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		if ($id =~ /NETSCAPE/) {
			print "F- $line\n" if $verbose;
			next;
		}
		unless (system qw/grep -qR/, '^\.\\\\" .*' . $id . '\>',
		    "$srcdir/") {
			print "F- $line\n" if $verbose;
			next;
		}
		if (grep { $_ eq $id } @obsolete) {
			print "F- $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: function $id()";
		}
		next;
	}
	if (/ \*$/) {
		$_ .= <$in_fh>;
		goto try_again;
	}
	die "parse error: $_";
}

close $in_fh;
exit 0;
