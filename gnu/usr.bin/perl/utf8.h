/*    utf8.h
 *
 * This file contains definitions for use with the UTF-8 encoding.  It
 * actually also works with the variant UTF-8 encoding called UTF-EBCDIC, and
 * hides almost all of the differences between these from the caller.  In other
 * words, someone should #include this file, and if the code is being compiled
 * on an EBCDIC platform, things should mostly just work.
 *
 *    Copyright (C) 2000, 2001, 2002, 2005, 2006, 2007, 2009,
 *    2010, 2011 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifndef H_UTF8      /* Guard against recursive inclusion */
#define H_UTF8 1

/* Use UTF-8 as the default script encoding?
 * Turning this on will break scripts having non-UTF-8 binary
 * data (such as Latin-1) in string literals. */
#ifdef USE_UTF8_SCRIPTS
#    define USE_UTF8_IN_NAMES (!IN_BYTES)
#else
#    define USE_UTF8_IN_NAMES (PL_hints & HINT_UTF8)
#endif

#include "regcharclass.h"
#include "unicode_constants.h"

/* For to_utf8_fold_flags, q.v. */
#define FOLD_FLAGS_LOCALE 0x1
#define FOLD_FLAGS_FULL   0x2
#define FOLD_FLAGS_NOMIX_ASCII 0x4

/* For _core_swash_init(), internal core use only */
#define _CORE_SWASH_INIT_USER_DEFINED_PROPERTY 0x1
#define _CORE_SWASH_INIT_RETURN_IF_UNDEF       0x2
#define _CORE_SWASH_INIT_ACCEPT_INVLIST        0x4

#define uvchr_to_utf8(a,b)          uvchr_to_utf8_flags(a,b,0)
#define uvchr_to_utf8_flags(d,uv,flags)                                        \
                            uvoffuni_to_utf8_flags(d,NATIVE_TO_UNI(uv),flags)
#define utf8_to_uvchr_buf(s, e, lenp)                                          \
                     utf8n_to_uvchr(s, (U8*)(e) - (U8*)(s), lenp,              \
                                    ckWARN_d(WARN_UTF8) ? 0 : UTF8_ALLOW_ANY)

#define to_uni_fold(c, p, lenp) _to_uni_fold_flags(c, p, lenp, FOLD_FLAGS_FULL)
#define to_utf8_fold(c, p, lenp) _to_utf8_fold_flags(c, p, lenp, FOLD_FLAGS_FULL)
#define to_utf8_lower(a,b,c) _to_utf8_lower_flags(a,b,c,0)
#define to_utf8_upper(a,b,c) _to_utf8_upper_flags(a,b,c,0)
#define to_utf8_title(a,b,c) _to_utf8_title_flags(a,b,c,0)

/* Source backward compatibility. */
#define is_utf8_string_loc(s, len, ep)	is_utf8_string_loclen(s, len, ep, 0)

#define foldEQ_utf8(s1, pe1, l1, u1, s2, pe2, l2, u2) \
		    foldEQ_utf8_flags(s1, pe1, l1, u1, s2, pe2, l2, u2, 0)
#define FOLDEQ_UTF8_NOMIX_ASCII   (1 << 0)
#define FOLDEQ_LOCALE             (1 << 1)
#define FOLDEQ_S1_ALREADY_FOLDED  (1 << 2)
#define FOLDEQ_S2_ALREADY_FOLDED  (1 << 3)

/*
=for apidoc ibcmp_utf8

This is a synonym for (! foldEQ_utf8())

=cut
*/
#define ibcmp_utf8(s1, pe1, l1, u1, s2, pe2, l2, u2) \
		    cBOOL(! foldEQ_utf8(s1, pe1, l1, u1, s2, pe2, l2, u2))

#ifdef EBCDIC
/* The equivalent of these macros but implementing UTF-EBCDIC
   are in the following header file:
 */

#include "utfebcdic.h"

#else	/* ! EBCDIC */
START_EXTERN_C

#ifdef DOINIT
EXTCONST unsigned char PL_utf8skip[] = {
/* 0x00 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x10 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x20 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x30 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x40 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x50 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x60 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x70 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
/* 0x80 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus: continuation byte */
/* 0x90 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus: continuation byte */
/* 0xA0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus: continuation byte */
/* 0xB0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus: continuation byte */
/* 0xC0 */ 2,2,				    /* overlong */
/* 0xC2 */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,     /* U+0080 to U+03FF */
/* 0xD0 */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* U+0400 to U+07FF */
/* 0xE0 */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, /* U+0800 to U+FFFF */
/* 0xF0 */ 4,4,4,4,4,4,4,4,5,5,5,5,6,6,	    /* above BMP to 2**31 - 1 */
/* 0xFE */ 7,13, /* Perl extended (never was official UTF-8).  Up to 72bit
		    allowed (64-bit + reserved). */
};
#else
EXTCONST unsigned char PL_utf8skip[];
#endif

END_EXTERN_C

/* Native character to/from iso-8859-1.  Are the identity functions on ASCII
 * platforms */
#define NATIVE_TO_LATIN1(ch)     (ch)
#define LATIN1_TO_NATIVE(ch)     (ch)

/* I8 is an intermediate version of UTF-8 used only in UTF-EBCDIC.  We thus
 * consider it to be identical to UTF-8 on ASCII platforms.  Strictly speaking
 * UTF-8 and UTF-EBCDIC are two different things, but we often conflate them
 * because they are 8-bit encodings that serve the same purpose in Perl, and
 * rarely do we need to distinguish them.  The term "NATIVE_UTF8" applies to
 * whichever one is applicable on the current platform */
#define NATIVE_UTF8_TO_I8(ch) (ch)
#define I8_TO_NATIVE_UTF8(ch) (ch)

/* Transforms in wide UV chars */
#define UNI_TO_NATIVE(ch)        (ch)
#define NATIVE_TO_UNI(ch)        (ch)

/*

 The following table is from Unicode 3.2.

 Code Points		1st Byte  2nd Byte  3rd Byte  4th Byte

   U+0000..U+007F	00..7F
   U+0080..U+07FF     * C2..DF    80..BF
   U+0800..U+0FFF	E0      * A0..BF    80..BF
   U+1000..U+CFFF       E1..EC    80..BF    80..BF
   U+D000..U+D7FF       ED        80..9F    80..BF
   U+D800..U+DFFF       ED        A0..BF    80..BF  (surrogates)
   U+E000..U+FFFF       EE..EF    80..BF    80..BF
  U+10000..U+3FFFF	F0      * 90..BF    80..BF    80..BF
  U+40000..U+FFFFF	F1..F3    80..BF    80..BF    80..BF
 U+100000..U+10FFFF	F4        80..8F    80..BF    80..BF
    Below are non-Unicode code points
 U+110000..U+13FFFF	F4        90..BF    80..BF    80..BF
 U+110000..U+1FFFFF	F5..F7    80..BF    80..BF    80..BF
 U+200000..:            F8..    * 88..BF    80..BF    80..BF    80..BF

Note the gaps before several of the byte entries above marked by '*'.  These are
caused by legal UTF-8 avoiding non-shortest encodings: it is technically
possible to UTF-8-encode a single code point in different ways, but that is
explicitly forbidden, and the shortest possible encoding should always be used
(and that is what Perl does).  The non-shortest ones are called 'overlongs'.

 */

/*
 Another way to look at it, as bits:

                  Code Points      1st Byte   2nd Byte   3rd Byte   4th Byte

                        0aaa aaaa  0aaa aaaa
              0000 0bbb bbaa aaaa  110b bbbb  10aa aaaa
              cccc bbbb bbaa aaaa  1110 cccc  10bb bbbb  10aa aaaa
 00 000d ddcc cccc bbbb bbaa aaaa  1111 0ddd  10cc cccc  10bb bbbb  10aa aaaa

As you can see, the continuation bytes all begin with C<10>, and the
leading bits of the start byte tell how many bytes there are in the
encoded character.

Perl's extended UTF-8 means we can have start bytes up to FF.

*/

/* Is the representation of the Unicode code point 'c' the same regardless of
 * being encoded in UTF-8 or not? */
#define UNI_IS_INVARIANT(c)		(((UV)c) <  0x80)

/* Is the UTF8-encoded byte 'c' part of a variant sequence in UTF-8?  This is
 * the inverse of UTF8_IS_INVARIANT */
#define UTF8_IS_CONTINUED(c) 		(((U8)c) &  0x80)

/* Is the byte 'c' the first byte of a multi-byte UTF8-8 encoded sequence?
 * This doesn't catch invariants (they are single-byte).  It also excludes the
 * illegal overlong sequences that begin with C0 and C1. */
#define UTF8_IS_START(c)		(((U8)c) >= 0xc2)

/* Is the byte 'c' part of a multi-byte UTF8-8 encoded sequence, and not the
 * first byte thereof?  */
#define UTF8_IS_CONTINUATION(c)		((((U8)c) & 0xC0) == 0x80)

/* Is the UTF8-encoded byte 'c' the first byte of a two byte sequence?  Use
 * UTF8_IS_NEXT_CHAR_DOWNGRADEABLE() instead if the input isn't known to
 * be well-formed.  Masking with 0xfe allows the low bit to be 0 or 1; thus
 * this matches 0xc[23]. */
#define UTF8_IS_DOWNGRADEABLE_START(c)	(((U8)(c) & 0xfe) == 0xc2)

/* Is the UTF8-encoded byte 'c' the first byte of a sequence of bytes that
 * represent a code point > 255? */
#define UTF8_IS_ABOVE_LATIN1(c)	((U8)(c) >= 0xc4)

/* This defines the 1-bits that are to be in the first byte of a multi-byte
 * UTF-8 encoded character that give the number of bytes that comprise the
 * character. 'len' is the number of bytes in the multi-byte sequence. */
#define UTF_START_MARK(len) (((len) >  7) ? 0xFF : (0xFF & (0xFE << (7-(len)))))

/* Masks out the initial one bits in a start byte, leaving the real data ones.
 * Doesn't work on an invariant byte.  'len' is the number of bytes in the
 * multi-byte sequence that comprises the character. */
#define UTF_START_MASK(len) (((len) >= 7) ? 0x00 : (0x1F >> ((len)-2)))

/* This defines the bits that are to be in the continuation bytes of a multi-byte
 * UTF-8 encoded character that indicate it is a continuation byte. */
#define UTF_CONTINUATION_MARK		0x80

/* This is the number of low-order bits a continuation byte in a UTF-8 encoded
 * sequence contributes to the specification of the code point.  In the bit
 * maps above, you see that the first 2 bits are a constant '10', leaving 6 of
 * real information */
#define UTF_ACCUMULATION_SHIFT		6

/* 2**UTF_ACCUMULATION_SHIFT - 1 */
#define UTF_CONTINUATION_MASK		((U8)0x3f)

/* If a value is anded with this, and the result is non-zero, then using the
 * original value in UTF8_ACCUMULATE will overflow, shifting bits off the left
 * */
#define UTF_ACCUMULATION_OVERFLOW_MASK					\
    (((UV) UTF_CONTINUATION_MASK) << ((sizeof(UV) * CHARBITS)           \
           - UTF_ACCUMULATION_SHIFT))

#if UVSIZE >= 8
#  define UTF8_QUAD_MAX UINT64_C(0x1000000000)

/* Input is a true Unicode (not-native) code point */
#define OFFUNISKIP(uv) ( (uv) < 0x80        ? 1 : \
		      (uv) < 0x800          ? 2 : \
		      (uv) < 0x10000        ? 3 : \
		      (uv) < 0x200000       ? 4 : \
		      (uv) < 0x4000000      ? 5 : \
		      (uv) < 0x80000000     ? 6 : \
                      (uv) < UTF8_QUAD_MAX ? 7 : 13 )
#else
/* No, I'm not even going to *TRY* putting #ifdef inside a #define */
#define OFFUNISKIP(uv) ( (uv) < 0x80        ? 1 : \
		      (uv) < 0x800          ? 2 : \
		      (uv) < 0x10000        ? 3 : \
		      (uv) < 0x200000       ? 4 : \
		      (uv) < 0x4000000      ? 5 : \
		      (uv) < 0x80000000     ? 6 : 7 )
#endif

/* How wide can a single UTF-8 encoded character become in bytes. */
/* NOTE: Strictly speaking Perl's UTF-8 should not be called UTF-8 since UTF-8
 * is an encoding of Unicode, and Unicode's upper limit, 0x10FFFF, can be
 * expressed with 4 bytes.  However, Perl thinks of UTF-8 as a way to encode
 * non-negative integers in a binary format, even those above Unicode */
#define UTF8_MAXBYTES 13

/* The maximum number of UTF-8 bytes a single Unicode character can
 * uppercase/lowercase/fold into.  Unicode guarantees that the maximum
 * expansion is 3 characters.  On ASCIIish platforms, the highest Unicode
 * character occupies 4 bytes, therefore this number would be 12, but this is
 * smaller than the maximum width a single above-Unicode character can occupy,
 * so use that instead */
#if UTF8_MAXBYTES < 12
#error UTF8_MAXBYTES must be at least 12
#endif

#define MAX_UTF8_TWO_BYTE 0x7FF

#define UTF8_MAXBYTES_CASE	UTF8_MAXBYTES

#endif /* EBCDIC vs ASCII */

/* Rest of these are attributes of Unicode and perl's internals rather than the
 * encoding, or happen to be the same in both ASCII and EBCDIC (at least at
 * this level; the macros that some of these call may have different
 * definitions in the two encodings */

/* In domain restricted to ASCII, these may make more sense to the reader than
 * the ones with Latin1 in the name */
#define NATIVE_TO_ASCII(ch)      NATIVE_TO_LATIN1(ch)
#define ASCII_TO_NATIVE(ch)      LATIN1_TO_NATIVE(ch)

/* More or less misleadingly-named defines, retained for back compat */
#define NATIVE_TO_UTF(ch)        NATIVE_UTF8_TO_I8(ch)
#define NATIVE_TO_I8(ch)         NATIVE_UTF8_TO_I8(ch)
#define UTF_TO_NATIVE(ch)        I8_TO_NATIVE_UTF8(ch)
#define I8_TO_NATIVE(ch)         I8_TO_NATIVE_UTF8(ch)
#define NATIVE8_TO_UNI(ch)       NATIVE_TO_LATIN1(ch)

/* Adds a UTF8 continuation byte 'new' of information to a running total code
 * point 'old' of all the continuation bytes so far.  This is designed to be
 * used in a loop to convert from UTF-8 to the code point represented.  Note
 * that this is asymmetric on EBCDIC platforms, in that the 'new' parameter is
 * the UTF-EBCDIC byte, whereas the 'old' parameter is a Unicode (not EBCDIC)
 * code point in process of being generated */
#define UTF8_ACCUMULATE(old, new) (((old) << UTF_ACCUMULATION_SHIFT)           \
                                   | ((NATIVE_UTF8_TO_I8((U8)new))             \
                                       & UTF_CONTINUATION_MASK))

/* This works in the face of malformed UTF-8. */
#define UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(s, e) (UTF8_IS_DOWNGRADEABLE_START(*s) \
                                               && ( (e) - (s) > 1)             \
                                               && UTF8_IS_CONTINUATION(*((s)+1)))

/* Number of bytes a code point occupies in UTF-8. */
#define NATIVE_SKIP(uv) OFFUNISKIP(NATIVE_TO_UNI(uv))

/* Most code which says UNISKIP is really thinking in terms of native code
 * points (0-255) plus all those beyond.  This is an imprecise term, but having
 * it means existing code continues to work.  For precision, use NATIVE_SKIP
 * and OFFUNISKIP */
#define UNISKIP(uv)   NATIVE_SKIP(uv)

/* Convert a two (not one) byte utf8 character to a native code point value.
 * Needs just one iteration of accumulate.  Should not be used unless it is
 * known that the two bytes are legal: 1) two-byte start, and 2) continuation.
 * Note that the result can be larger than 255 if the input character is not
 * downgradable */
#define TWO_BYTE_UTF8_TO_NATIVE(HI, LO) \
     UNI_TO_NATIVE(UTF8_ACCUMULATE((NATIVE_UTF8_TO_I8(HI) & UTF_START_MASK(2)), \
                                   (LO)))

/* Should never be used, and be deprecated */
#define TWO_BYTE_UTF8_TO_UNI(HI, LO) NATIVE_TO_UNI(TWO_BYTE_UTF8_TO_NATIVE(HI, LO))

/* How many bytes in the UTF-8 encoded character whose first (perhaps only)
 * byte is pointed to by 's' */
#define UTF8SKIP(s) PL_utf8skip[*(const U8*)(s)]

/* Is the byte 'c' the same character when encoded in UTF-8 as when not.  This
 * works on both UTF-8 encoded strings and non-encoded, as it returns TRUE in
 * each for the exact same set of bit patterns.  (And it works on any byte in a
 * UTF-8 encoded string) */
#define UTF8_IS_INVARIANT(c)		UNI_IS_INVARIANT(NATIVE_UTF8_TO_I8(c))

/* Like the above, but its name implies a non-UTF8 input, and is implemented
 * differently (for no particular reason) */
#define NATIVE_BYTE_IS_INVARIANT(c)	UNI_IS_INVARIANT(NATIVE_TO_LATIN1(c))

/* Like the above, but accepts any UV as input */
#define UVCHR_IS_INVARIANT(uv)          UNI_IS_INVARIANT(NATIVE_TO_UNI(uv))

#define MAX_PORTABLE_UTF8_TWO_BYTE 0x3FF    /* constrained by EBCDIC */

/* The macros in the next 4 sets are used to generate the two utf8 or utfebcdic
 * bytes from an ordinal that is known to fit into exactly two (not one) bytes;
 * it must be less than 0x3FF to work across both encodings. */

/* These two are helper macros for the other three sets, and should not be used
 * directly anywhere else.  'translate_function' is either NATIVE_TO_LATIN1
 * (which works for code points up to 0xFF) or NATIVE_TO_UNI which works for any
 * code point */
#define __BASE_TWO_BYTE_HI(c, translate_function)                               \
            I8_TO_NATIVE_UTF8((translate_function(c) >> UTF_ACCUMULATION_SHIFT) \
                              | UTF_START_MARK(2))
#define __BASE_TWO_BYTE_LO(c, translate_function)                               \
              I8_TO_NATIVE_UTF8((translate_function(c) & UTF_CONTINUATION_MASK) \
                                 | UTF_CONTINUATION_MARK)

/* The next two macros should not be used.  They were designed to be usable as
 * the case label of a switch statement, but this doesn't work for EBCDIC.  Use
 * regen/unicode_constants.pl instead */
#define UTF8_TWO_BYTE_HI_nocast(c)  __BASE_TWO_BYTE_HI(c, NATIVE_TO_UNI)
#define UTF8_TWO_BYTE_LO_nocast(c)  __BASE_TWO_BYTE_LO(c, NATIVE_TO_UNI)

/* The next two macros are used when the source should be a single byte
 * character; checked for under DEBUGGING */
#define UTF8_EIGHT_BIT_HI(c) (__ASSERT_(FITS_IN_8_BITS(c))                    \
                             ((U8) __BASE_TWO_BYTE_HI(c, NATIVE_TO_LATIN1)))
#define UTF8_EIGHT_BIT_LO(c) (__ASSERT_(FITS_IN_8_BITS(c))                    \
                             ((U8) __BASE_TWO_BYTE_LO(c, NATIVE_TO_LATIN1)))

/* These final two macros in the series are used when the source can be any
 * code point whose UTF-8 is known to occupy 2 bytes; they are less efficient
 * than the EIGHT_BIT versions on EBCDIC platforms.  We use the logical '~'
 * operator instead of "<=" to avoid getting compiler warnings.
 * MAX_PORTABLE_UTF8_TWO_BYTE should be exactly all one bits in the lower few
 * places, so the ~ works */
#define UTF8_TWO_BYTE_HI(c)                                                    \
       (__ASSERT_((sizeof(c) ==  1)                                            \
                  || !(((WIDEST_UTYPE)(c)) & ~MAX_PORTABLE_UTF8_TWO_BYTE))     \
        ((U8) __BASE_TWO_BYTE_HI(c, NATIVE_TO_LATIN1)))
#define UTF8_TWO_BYTE_LO(c)                                                    \
       (__ASSERT_((sizeof(c) ==  1)                                            \
                  || !(((WIDEST_UTYPE)(c)) & ~MAX_PORTABLE_UTF8_TWO_BYTE))     \
        ((U8) __BASE_TWO_BYTE_LO(c, NATIVE_TO_LATIN1)))

/* This is illegal in any well-formed UTF-8 in both EBCDIC and ASCII
 * as it is only in overlongs. */
#define ILLEGAL_UTF8_BYTE   I8_TO_NATIVE_UTF8(0xC1)

/*
 * 'UTF' is whether or not p is encoded in UTF8.  The names 'foo_lazy_if' stem
 * from an earlier version of these macros in which they didn't call the
 * foo_utf8() macros (i.e. were 'lazy') unless they decided that *p is the
 * beginning of a utf8 character.  Now that foo_utf8() determines that itself,
 * no need to do it again here
 */
#define isIDFIRST_lazy_if(p,UTF) ((IN_BYTES || !UTF ) \
				 ? isIDFIRST(*(p)) \
				 : isIDFIRST_utf8((const U8*)p))
#define isWORDCHAR_lazy_if(p,UTF)   ((IN_BYTES || (!UTF )) \
				 ? isWORDCHAR(*(p)) \
				 : isWORDCHAR_utf8((const U8*)p))
#define isALNUM_lazy_if(p,UTF)   isWORDCHAR_lazy_if(p,UTF)

#define UTF8_MAXLEN UTF8_MAXBYTES

/* A Unicode character can fold to up to 3 characters */
#define UTF8_MAX_FOLD_CHAR_EXPAND 3

#define IN_BYTES (CopHINTS_get(PL_curcop) & HINT_BYTES)
#define DO_UTF8(sv) (SvUTF8(sv) && !IN_BYTES)
#define IN_UNI_8_BIT \
	    (CopHINTS_get(PL_curcop) & (HINT_UNI_8_BIT|HINT_LOCALE_NOT_CHARS) \
	     && ! IN_LOCALE_RUNTIME && ! IN_BYTES)


#define UTF8_ALLOW_EMPTY		0x0001	/* Allow a zero length string */

/* Allow first byte to be a continuation byte */
#define UTF8_ALLOW_CONTINUATION		0x0002

/* Allow second... bytes to be non-continuation bytes */
#define UTF8_ALLOW_NON_CONTINUATION	0x0004

/* expecting more bytes than were available in the string */
#define UTF8_ALLOW_SHORT		0x0008

/* Overlong sequence; i.e., the code point can be specified in fewer bytes. */
#define UTF8_ALLOW_LONG                 0x0010

#define UTF8_DISALLOW_SURROGATE		0x0020	/* Unicode surrogates */
#define UTF8_WARN_SURROGATE		0x0040

#define UTF8_DISALLOW_NONCHAR           0x0080	/* Unicode non-character */
#define UTF8_WARN_NONCHAR               0x0100	/*  code points */

#define UTF8_DISALLOW_SUPER		0x0200	/* Super-set of Unicode: code */
#define UTF8_WARN_SUPER		        0x0400	/* points above the legal max */

/* Code points which never were part of the original UTF-8 standard, the first
 * byte of which is a FE or FF on ASCII platforms. If the first byte is FF, it
 * will overflow a 32-bit word.  If the first byte is FE, it will overflow a
 * signed 32-bit word. */
#define UTF8_DISALLOW_FE_FF		0x0800
#define UTF8_WARN_FE_FF		        0x1000

#define UTF8_CHECK_ONLY			0x2000

/* For backwards source compatibility.  They do nothing, as the default now
 * includes what they used to mean.  The first one's meaning was to allow the
 * just the single non-character 0xFFFF */
#define UTF8_ALLOW_FFFF 0
#define UTF8_ALLOW_SURROGATE 0

#define UTF8_DISALLOW_ILLEGAL_INTERCHANGE (UTF8_DISALLOW_SUPER|UTF8_DISALLOW_NONCHAR|UTF8_DISALLOW_SURROGATE|UTF8_DISALLOW_FE_FF)
#define UTF8_WARN_ILLEGAL_INTERCHANGE \
	(UTF8_WARN_SUPER|UTF8_WARN_NONCHAR|UTF8_WARN_SURROGATE|UTF8_WARN_FE_FF)
#define UTF8_ALLOW_ANY \
	    (~(UTF8_DISALLOW_ILLEGAL_INTERCHANGE|UTF8_WARN_ILLEGAL_INTERCHANGE))
#define UTF8_ALLOW_ANYUV                                                        \
         (UTF8_ALLOW_EMPTY                                                      \
	  & ~(UTF8_DISALLOW_ILLEGAL_INTERCHANGE|UTF8_WARN_ILLEGAL_INTERCHANGE))
#define UTF8_ALLOW_DEFAULT		(ckWARN(WARN_UTF8) ? 0 : \
					 UTF8_ALLOW_ANYUV)

/* Surrogates, non-character code points and above-Unicode code points are
 * problematic in some contexts.  This allows code that needs to check for
 * those to to quickly exclude the vast majority of code points it will
 * encounter */
#define UTF8_FIRST_PROBLEMATIC_CODE_POINT_FIRST_BYTE \
                                    FIRST_SURROGATE_UTF8_FIRST_BYTE

#define UTF8_IS_SURROGATE(s) cBOOL(is_SURROGATE_utf8(s))
#define UTF8_IS_REPLACEMENT(s, send) cBOOL(is_REPLACEMENT_utf8_safe(s,send))

/*		  ASCII		     EBCDIC I8
 * U+10FFFF: \xF4\x8F\xBF\xBF	\xF9\xA1\xBF\xBF\xBF	max legal Unicode
 * U+110000: \xF4\x90\x80\x80	\xF9\xA2\xA0\xA0\xA0
 * U+110001: \xF4\x90\x80\x81	\xF9\xA2\xA0\xA0\xA1
 */
#ifdef EBCDIC /* Both versions assume well-formed UTF8 */
#   define UTF8_IS_SUPER(s) (NATIVE_UTF8_TO_I8(* (U8*) (s)) >= 0xF9             \
                         && (NATIVE_UTF8_TO_I8(* (U8*) (s)) > 0xF9              \
                             || (NATIVE_UTF8_TO_I8(* ((U8*) (s) + 1)) >= 0xA2)))
#else
#   define UTF8_IS_SUPER(s) (*(U8*) (s) >= 0xF4                                 \
                            && (*(U8*) (s) > 0xF4 || (*((U8*) (s) + 1) >= 0x90)))
#endif

/* These are now machine generated, and the 'given' clause is no longer
 * applicable */
#define UTF8_IS_NONCHAR_GIVEN_THAT_NON_SUPER_AND_GE_PROBLEMATIC(s)             \
                                                    cBOOL(is_NONCHAR_utf8(s))
#define UTF8_IS_NONCHAR_(s)                                                    \
                    UTF8_IS_NONCHAR_GIVEN_THAT_NON_SUPER_AND_GE_PROBLEMATIC(s)

#define UNICODE_SURROGATE_FIRST		0xD800
#define UNICODE_SURROGATE_LAST		0xDFFF
#define UNICODE_REPLACEMENT		0xFFFD
#define UNICODE_BYTE_ORDER_MARK		0xFEFF

/* Though our UTF-8 encoding can go beyond this,
 * let's be conservative and do as Unicode says. */
#define PERL_UNICODE_MAX	0x10FFFF

#define UNICODE_WARN_SURROGATE     0x0001	/* UTF-16 surrogates */
#define UNICODE_WARN_NONCHAR       0x0002	/* Non-char code points */
#define UNICODE_WARN_SUPER         0x0004	/* Above 0x10FFFF */
#define UNICODE_WARN_FE_FF         0x0008	/* Above 0x10FFFF */
#define UNICODE_DISALLOW_SURROGATE 0x0010
#define UNICODE_DISALLOW_NONCHAR   0x0020
#define UNICODE_DISALLOW_SUPER     0x0040
#define UNICODE_DISALLOW_FE_FF     0x0080
#define UNICODE_WARN_ILLEGAL_INTERCHANGE                                      \
            (UNICODE_WARN_SURROGATE|UNICODE_WARN_NONCHAR|UNICODE_WARN_SUPER)
#define UNICODE_DISALLOW_ILLEGAL_INTERCHANGE                                  \
 (UNICODE_DISALLOW_SURROGATE|UNICODE_DISALLOW_NONCHAR|UNICODE_DISALLOW_SUPER)

/* For backward source compatibility, as are now the default */
#define UNICODE_ALLOW_SURROGATE 0
#define UNICODE_ALLOW_SUPER	0
#define UNICODE_ALLOW_ANY	0

#define UNICODE_IS_SURROGATE(c)		((c) >= UNICODE_SURROGATE_FIRST && \
					 (c) <= UNICODE_SURROGATE_LAST)
#define UNICODE_IS_REPLACEMENT(c)	((c) == UNICODE_REPLACEMENT)
#define UNICODE_IS_BYTE_ORDER_MARK(c)	((c) == UNICODE_BYTE_ORDER_MARK)
#define UNICODE_IS_NONCHAR(c)		((c >= 0xFDD0 && c <= 0xFDEF) \
			/* The other noncharacters end in FFFE or FFFF, which  \
			 * the mask below catches both of, but beyond the last \
			 * official unicode code point, they aren't            \
			 * noncharacters, since those aren't Unicode           \
			 * characters at all */                                \
			|| ((((c & 0xFFFE) == 0xFFFE)) && ! UNICODE_IS_SUPER(c)))
#define UNICODE_IS_SUPER(c)		((c) > PERL_UNICODE_MAX)
#define UNICODE_IS_FE_FF(c)		((c) > 0x7FFFFFFF)

#define LATIN_SMALL_LETTER_SHARP_S      LATIN_SMALL_LETTER_SHARP_S_NATIVE
#define LATIN_SMALL_LETTER_Y_WITH_DIAERESIS                                  \
                                LATIN_SMALL_LETTER_Y_WITH_DIAERESIS_NATIVE
#define MICRO_SIGN      MICRO_SIGN_NATIVE
#define LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE                               \
                            LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE_NATIVE
#define LATIN_SMALL_LETTER_A_WITH_RING_ABOVE                                 \
                                LATIN_SMALL_LETTER_A_WITH_RING_ABOVE_NATIVE
#define UNICODE_GREEK_CAPITAL_LETTER_SIGMA	0x03A3
#define UNICODE_GREEK_SMALL_LETTER_FINAL_SIGMA	0x03C2
#define UNICODE_GREEK_SMALL_LETTER_SIGMA	0x03C3
#define GREEK_SMALL_LETTER_MU                   0x03BC
#define GREEK_CAPITAL_LETTER_MU                 0x039C	/* Upper and title case
                                                           of MICRON */
#define LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS   0x0178	/* Also is title case */
#define LATIN_CAPITAL_LETTER_SHARP_S	        0x1E9E
#define LATIN_SMALL_LETTER_LONG_S               0x017F
#define LATIN_SMALL_LIGATURE_LONG_S_T           0xFB05
#define LATIN_SMALL_LIGATURE_ST                 0xFB06
#define KELVIN_SIGN                             0x212A
#define ANGSTROM_SIGN                           0x212B

#define UNI_DISPLAY_ISPRINT	0x0001
#define UNI_DISPLAY_BACKSLASH	0x0002
#define UNI_DISPLAY_QQ		(UNI_DISPLAY_ISPRINT|UNI_DISPLAY_BACKSLASH)
#define UNI_DISPLAY_REGEX	(UNI_DISPLAY_ISPRINT|UNI_DISPLAY_BACKSLASH)

#define ANYOF_FOLD_SHARP_S(node, input, end)	\
	(ANYOF_BITMAP_TEST(node, LATIN_SMALL_LETTER_SHARP_S) && \
	 (ANYOF_NONBITMAP(node)) && \
	 (ANYOF_FLAGS(node) & ANYOF_LOC_NONBITMAP_FOLD) && \
	 ((end) > (input) + 1) && \
	 toFOLD((input)[0]) == 's' && \
	 toFOLD((input)[1]) == 's')
#define SHARP_S_SKIP 2

/* If you want to exclude surrogates, and beyond legal Unicode, see the blame
 * log for earlier versions which gave details for these */

#ifndef EBCDIC
/* This was generated by regen/regcharclass.pl, and then moved here.  The lines
 * that generated it were then commented out.  This was done solely because it
 * takes on the order of 10 minutes to generate, and is never going to change.
 * The EBCDIC equivalent hasn't been commented out in regcharclass.pl, so it
 * should generate and run the correct stuff */
/*
	UTF8_CHAR: Matches utf8 from 1 to 4 bytes

	0x0 - 0x1FFFFF
*/
/*** GENERATED CODE ***/
#define is_UTF8_CHAR_utf8_safe(s,e)                                         \
( ((e)-(s) > 3) ?                                                           \
    ( ( ( ((U8*)s)[0] & 0x80 ) == 0x00 ) ? 1                                \
    : ( 0xC2 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xDF ) ?                      \
	( ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ? 2 : 0 )                      \
    : ( 0xE0 == ((U8*)s)[0] ) ?                                             \
	( ( ( ( ((U8*)s)[1] & 0xE0 ) == 0xA0 ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) ? 3 : 0 )\
    : ( 0xE1 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xEF ) ?                      \
	( ( ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) ? 3 : 0 )\
    : ( 0xF0 == ((U8*)s)[0] ) ?                                             \
	( ( ( ( 0x90 <= ((U8*)s)[1] && ((U8*)s)[1] <= 0xBF ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[3] & 0xC0 ) == 0x80 ) ) ? 4 : 0 )\
    : ( ( ( ( 0xF1 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xF7 ) && ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[3] & 0xC0 ) == 0x80 ) ) ? 4 : 0 )\
: ((e)-(s) > 2) ?                                                           \
    ( ( ( ((U8*)s)[0] & 0x80 ) == 0x00 ) ? 1                                \
    : ( 0xC2 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xDF ) ?                      \
	( ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ? 2 : 0 )                      \
    : ( 0xE0 == ((U8*)s)[0] ) ?                                             \
	( ( ( ( ((U8*)s)[1] & 0xE0 ) == 0xA0 ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) ? 3 : 0 )\
    : ( ( ( 0xE1 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xEF ) && ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) ? 3 : 0 )\
: ((e)-(s) > 1) ?                                                           \
    ( ( ( ((U8*)s)[0] & 0x80 ) == 0x00 ) ? 1                                \
    : ( ( 0xC2 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xDF ) && ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ) ? 2 : 0 )\
: ((e)-(s) > 0) ?                                                           \
    ( ( ((U8*)s)[0] & 0x80 ) == 0x00 )                                      \
: 0 )
#endif

/* IS_UTF8_CHAR(p) is strictly speaking wrong (not UTF-8) because it
 * (1) allows UTF-8 encoded UTF-16 surrogates
 * (2) it allows code points past U+10FFFF.
 * The Perl_is_utf8_char() full "slow" code will handle the Perl
 * "extended UTF-8". */
#define IS_UTF8_CHAR(p, n)      (is_UTF8_CHAR_utf8_safe(p, (p) + (n)) == n)

/* regen/regcharclass.pl generates is_UTF8_CHAR_utf8_safe() macros for up to
 * these number of bytes.  So this has to be coordinated with it */
#ifdef EBCDIC
#   define IS_UTF8_CHAR_FAST(n) ((n) <= 5)
#else
#   define IS_UTF8_CHAR_FAST(n) ((n) <= 4)
#endif

#endif /* H_UTF8 */

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
