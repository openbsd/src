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

my %internal = (
    asn1 => [qw(
	ASN1_ENCODING
	ASN1_STRING_FLAG_BITS_LEFT ASN1_STRING_FLAG_CONT
	ASN1_STRING_FLAG_MSTRING ASN1_STRING_FLAG_NDEF
	CHARTYPE_FIRST_ESC_2253 CHARTYPE_LAST_ESC_2253 CHARTYPE_PRINTABLESTRING
    )],
    bn => [qw(
	BN_BITS BN_BITS4 BN_BYTES
	BN_DEC_CONV BN_DEC_FMT1 BN_DEC_FMT2 BN_DEC_NUM BN_LLONG BN_LONG
	BN_MASK2 BN_MASK2h BN_MASK2h1 BN_MASK2l
	BN_TBIT BN_ULLONG
    )],
    objects => [qw(
	OBJ_bsearch OBJ_bsearch_ OBJ_bsearch_ex OBJ_bsearch_ex_
	USE_OBJ_MAC
    )],
    x509_vfy => [qw(
	X509_VERIFY_PARAM_ID
    )]
);

my %obsolete = (
    asn1 => [qw(
	ASN1_dup ASN1_d2i_bio ASN1_d2i_bio_of ASN1_d2i_fp ASN1_d2i_fp_of
	ASN1_i2d_bio ASN1_i2d_bio_of ASN1_i2d_bio_of_const
	ASN1_i2d_fp ASN1_i2d_fp_of ASN1_i2d_fp_of_const
	ASN1_LONG_UNDEF
	BIT_STRING_BITNAME
	ub_title
	V_ASN1_PRIMATIVE_TAG
	X509_algor_st
    )],
    bio => [qw(
	asn1_ps_func
	BIO_C_GET_PROXY_PARAM BIO_C_GET_SOCKS
	BIO_C_SET_PROXY_PARAM BIO_C_SET_SOCKS
	BIO_get_no_connect_return BIO_get_proxies
	BIO_get_proxy_header BIO_get_url
	BIO_set_filter_bio BIO_set_no_connect_return BIO_set_proxies
	BIO_set_proxy_cb BIO_set_proxy_header BIO_set_url
    )],
    bn => [qw(
	BN_HEX_FMT1 BN_HEX_FMT2 BN_MASK
    )],
    objects => [qw(
	_DECLARE_OBJ_BSEARCH_CMP_FN
	DECLARE_OBJ_BSEARCH_CMP_FN DECLARE_OBJ_BSEARCH_GLOBAL_CMP_FN
	IMPLEMENT_OBJ_BSEARCH_CMP_FN IMPLEMENT_OBJ_BSEARCH_GLOBAL_CMP_FN
    )],
);

my %postponed = (
    asn1 => [qw(
	ASN1_ITEM_EXP ASN1_ITEM_ptr ASN1_ITEM_ref ASN1_ITEM_rptr
	ASN1_TEMPLATE ASN1_TLC
	CHECKED_D2I_OF CHECKED_I2D_OF CHECKED_NEW_OF
	CHECKED_PPTR_OF CHECKED_PTR_OF
	DECLARE_ASN1_ALLOC_FUNCTIONS DECLARE_ASN1_ALLOC_FUNCTIONS_name
	DECLARE_ASN1_ENCODE_FUNCTIONS DECLARE_ASN1_ENCODE_FUNCTIONS_const
	DECLARE_ASN1_FUNCTIONS DECLARE_ASN1_FUNCTIONS_const
	DECLARE_ASN1_FUNCTIONS_fname DECLARE_ASN1_FUNCTIONS_name
	DECLARE_ASN1_ITEM
	DECLARE_ASN1_NDEF_FUNCTION
	DECLARE_ASN1_PRINT_FUNCTION DECLARE_ASN1_PRINT_FUNCTION_fname
	DECLARE_ASN1_SET_OF
	D2I_OF
	IMPLEMENT_ASN1_SET_OF
	I2D_OF I2D_OF_const
	TYPEDEF_D2I_OF TYPEDEF_D2I2D_OF TYPEDEF_I2D_OF
    )],
    x509 => [qw(
	d2i_PBEPARAM d2i_PBE2PARAM d2i_PBKDF2PARAM
	i2d_PBEPARAM i2d_PBE2PARAM i2d_PBKDF2PARAM
	NETSCAPE_SPKAC NETSCAPE_SPKI
	PBEPARAM PBEPARAM_free PBEPARAM_new
	PBE2PARAM PBE2PARAM_free PBE2PARAM_new
	PBKDF2PARAM PBKDF2PARAM_free PBKDF2PARAM_new
	PKCS5_pbe_set PKCS5_pbe_set0_algor
	PKCS5_pbe2_set PKCS5_pbe2_set_iv
	PKCS5_pbkdf2_set
    )]
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
my %expect_undoc = ();
my %found_undoc = ();
my $verbose = 0;

if (defined $ARGV[0] && $ARGV[0] eq '-v') {
	$verbose = 1;
	shift @ARGV;
}
$#ARGV == 0 or die "usage: $0 [-v] headername";
$hfile .= "/$ARGV[0].h";
open my $in_fh, '<', $hfile or die "$hfile: $!";

$expect_undoc{$_} = 1 foreach @{$internal{$ARGV[0]}};
$expect_undoc{$_} = 1 foreach @{$obsolete{$ARGV[0]}};
$expect_undoc{$_} = 1 foreach @{$postponed{$ARGV[0]}};

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
		s/\s*\/\*.*?\*\/// and next;
		s/\s*\/\*.*// and $in_comment = 1;
	}

	# End C++ stuff.

	if ($in_cplusplus) {
		/^#endif$/ and $in_cplusplus = 0;
		print "-- $line\n" if $verbose;
		next;
	}

	# End declarations of structs.

	if ($in_struct) {
		if (/^\s*union\s+{$/) {
			print "-s $line\n" if $verbose;
			$in_struct++;
			next;
		}
		unless (s/^\s*\}//) {
			print "-s $line\n" if $verbose;
			next;
		}
		if (--$in_struct && /^\s+\w+;$/) {
			print "-s $line\n" if $verbose;
			next;
		}
		unless ($in_typedef_struct) {
			/^\s*;$/ or die "at end of struct: $_";
			print "-s $line\n" if $verbose;
			next;
		}
		$in_typedef_struct = 0;
		my ($id) = /^\s*(\w+);$/
		    or die "at end of typedef struct: $_";
		unless (system "$MANW -k 'Vt~^$id\$' > /dev/null 2>&1") {
			print "Vt $line\n" if $verbose;
			next;
		}
		if ($expect_undoc{$id}) {
			print "V- $line\n" if $verbose;
			$found_undoc{$id} = 1;
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
	    /^TYPEDEF_D2I2D_OF\(\w+\);$/ ||
	    /^#define __bounded__\(\w+, \w+, \w+\)$/ ||
	    /^#define HEADER_\w+_H$/ ||
	    /^#endif$/ ||
	    /^#else$/ ||
	    /^extern\s+const\s+ASN1_ITEM\s+\w+_it;$/ ||
	    /^#\s*include\s/ ||
	    /^#ifn?def\s/ ||
	    /^#if !?defined/ ||
	    /^#undef\s+BN_LLONG$/) {
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

	# Handle macros.

	if (my ($id) = /^#\s*define\s+(\w+)\s+\S/) {
		/\\$/ and $in_define = 1;
		if ($id eq 'BN_ULONG' &&
		    not system "$MANW -k 'Vt~^$id\$' > /dev/null 2>&1") {
			print "Vt $line\n" if $verbose;
			next;
		}
		unless (system "$MANW -k 'Dv~^$id\$' > /dev/null 2>&1") {
			print "Dv $line\n" if $verbose;
			next;
		}
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		unless (system qw/grep -qR/, '^\.\\\\" .*\<' . $id . '\>',
		    "$srcdir/") {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /^ASN1_PCTX_FLAGS_\w+$/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /^(?:ASN1|BIO|BN|X509(?:V3)?)_[FR]_\w+$/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /^X509_V_ERR_\w+$/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($id =~ /^(?:SN|LN|NID|OBJ)_\w+$/) {
			print "D- $line\n" if $verbose;
			next;
		}
		if ($expect_undoc{$id}) {
			print "D- $line\n" if $verbose;
			$found_undoc{$id} = 1;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: #define $id";
		}
		next;
	}
	if (my ($id) = /^#\s*define\s+(\w+)\(/) {
		/\\$/ and $in_define = 1;
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		unless (system qw/grep -qR/, '^\.\\\\" .*\<' . $id . '\>',
		    "$srcdir/") {
			print "F- $line\n" if $verbose;
			next;
		}
		if ($expect_undoc{$id}) {
			print "F- $line\n" if $verbose;
			$found_undoc{$id} = 1;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: #define $id()";
		}
		next;
	}
	if (my ($id) = /^#\s*define\s+(\w+)$/) {
		if ($expect_undoc{$id}) {
			print "-- $line\n" if $verbose;
			$found_undoc{$id} = 1;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: #define $id";
		}
		next;
	}

	# Handle global variables.

	if (my ($id) = /^extern\s+int\s+(\w+);$/) {
		unless (system "$MANW -k 'Va~^$id\$' > /dev/null 2>&1") {
			print "Va $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: extern int $id";
		}
		next;
	}

	# Handle variable type declarations.

	if (my ($id) = /^struct\s+(\w+);$/) {
		unless (system "$MANW -k 'Vt~^$id\$' > /dev/null 2>&1") {
			print "Vt $line\n" if $verbose;
			next;
		}
		if ($expect_undoc{$id}) {
			print "V- $line\n" if $verbose;
			$found_undoc{$id} = 1;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: struct $id";
		}
		next;
	}

	if (my ($id) = /^typedef\s+(?:const\s+)?(?:struct\s+)?\S+\s+(\w+);$/) {
		unless (system "$MANW -k 'Vt~^$id\$' > /dev/null 2>&1") {
			print "Vt $line\n" if $verbose;
			next;
		}
		if ($expect_undoc{$id}) {
			print "V- $line\n" if $verbose;
			$found_undoc{$id} = 1;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: typedef $id";
		}
		next;
	}

	if (my ($id) =/^typedef\s+\w+(?:\s+\*)?\s+\(\*(\w+)\)\(/) {
		/\);$/ or $in_function = 1;
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		if ($verbose) {
			print "XX $line\n";
		} else {
			warn "not found: function type (*$id)()";
		}
		next;
	}

	# Handle function declarations.

	if (/^\w+(?:\(\w+\))?(?:\s+\w+)*\s+(?:\(?\*\s*)?(\w+)\(/) {
		my $id = $1;
		/\);$/ or $in_function = 1;
		unless (system "$MANW $id > /dev/null 2>&1") {
			print "Fn $line\n" if $verbose;
			next;
		}
		# These functions are still provided by OpenSSL
		# and still used by the Python test suite,
		# but intentionally undocumented because nothing
		# else uses them according to tb@, Dec 3, 2021.
		if ($id =~ /NETSCAPE_(?:CERT_SEQUENCE|SPKAC|SPKI)/) {
			print "F- $line\n" if $verbose;
			next;
		}
		unless (system qw/grep -qR/, '^\.\\\\" .*\<' . $id . '\>',
		    "$srcdir/") {
			print "F- $line\n" if $verbose;
			next;
		}
		if ($expect_undoc{$id}) {
			print "F- $line\n" if $verbose;
			$found_undoc{$id} = 1;
			next;
		}
		if ($id =~ /^ASN1_PCTX_\w+$/) {
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
	if (/^int$/) {
		$_ .= ' ' . <$in_fh>;
		goto try_again;
	}
	if (/ \*$/) {
		$_ .= <$in_fh>;
		goto try_again;
	}
	die "parse error: $_";
}
close $in_fh;
foreach (keys %expect_undoc) {
	warn "expected as undocumented but not found: $_"
	    unless $found_undoc{$_};
}
exit 0;
