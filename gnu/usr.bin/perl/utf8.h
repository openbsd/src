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
#define FOLD_FLAGS_LOCALE       0x1
#define FOLD_FLAGS_FULL         0x2
#define FOLD_FLAGS_NOMIX_ASCII  0x4

/* For _core_swash_init(), internal core use only */
#define _CORE_SWASH_INIT_USER_DEFINED_PROPERTY 0x1
#define _CORE_SWASH_INIT_RETURN_IF_UNDEF       0x2
#define _CORE_SWASH_INIT_ACCEPT_INVLIST        0x4

/*
=head1 Unicode Support
L<perlguts/Unicode Support> has an introduction to this API.

See also L</Character classification>,
and L</Character case changing>.
Various functions outside this section also work specially with Unicode.
Search for the string "utf8" in this document.

=for apidoc is_ascii_string

This is a misleadingly-named synonym for L</is_invariant_string>.
On ASCII-ish platforms, the name isn't misleading: the ASCII-range characters
are exactly the UTF-8 invariants.  But EBCDIC machines have more invariants
than just the ASCII characters, so C<is_invariant_string> is preferred.

=cut
*/
#define is_ascii_string(s, len)     is_invariant_string(s, len)

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
#define FOLDEQ_S1_FOLDS_SANE      (1 << 4)
#define FOLDEQ_S2_FOLDS_SANE      (1 << 5)

#define ibcmp_utf8(s1, pe1, l1, u1, s2, pe2, l2, u2) \
		    cBOOL(! foldEQ_utf8(s1, pe1, l1, u1, s2, pe2, l2, u2))

#ifdef EBCDIC
/* The equivalent of these macros but implementing UTF-EBCDIC
   are in the following header file:
 */

#include "utfebcdic.h"

#else	/* ! EBCDIC */
START_EXTERN_C

/* How wide can a single UTF-8 encoded character become in bytes. */
/* NOTE: Strictly speaking Perl's UTF-8 should not be called UTF-8 since UTF-8
 * is an encoding of Unicode, and Unicode's upper limit, 0x10FFFF, can be
 * expressed with 4 bytes.  However, Perl thinks of UTF-8 as a way to encode
 * non-negative integers in a binary format, even those above Unicode */
#define UTF8_MAXBYTES 13

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
/* 0xC2 */     2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* U+0080 to U+03FF */
/* 0xD0 */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* U+0400 to U+07FF */
/* 0xE0 */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, /* U+0800 to U+FFFF */
/* 0xF0 */ 4,4,4,4,4,4,4,4,5,5,5,5,6,6,	    /* above BMP to 2**31 - 1 */
           /* Perl extended (never was official UTF-8).  Up to 36 bit */
/* 0xFE */                             7,
           /* More extended, Up to 72 bits (64-bit + reserved) */
/* 0xFF */                               UTF8_MAXBYTES
};
#else
EXTCONST unsigned char PL_utf8skip[];
#endif

END_EXTERN_C

#if defined(_MSC_VER) && _MSC_VER < 1400
/* older MSVC versions have a smallish macro buffer */
#define PERL_SMALL_MACRO_BUFFER
#endif

/* Native character to/from iso-8859-1.  Are the identity functions on ASCII
 * platforms */
#ifdef PERL_SMALL_MACRO_BUFFER
#define NATIVE_TO_LATIN1(ch)     ((U8)(ch))
#define LATIN1_TO_NATIVE(ch)     ((U8)(ch))
#else
#define NATIVE_TO_LATIN1(ch)     (__ASSERT_(FITS_IN_8_BITS(ch)) ((U8) (ch)))
#define LATIN1_TO_NATIVE(ch)     (__ASSERT_(FITS_IN_8_BITS(ch)) ((U8) (ch)))
#endif

/* I8 is an intermediate version of UTF-8 used only in UTF-EBCDIC.  We thus
 * consider it to be identical to UTF-8 on ASCII platforms.  Strictly speaking
 * UTF-8 and UTF-EBCDIC are two different things, but we often conflate them
 * because they are 8-bit encodings that serve the same purpose in Perl, and
 * rarely do we need to distinguish them.  The term "NATIVE_UTF8" applies to
 * whichever one is applicable on the current platform */
#ifdef PERL_SMALL_MACRO_BUFFER
#define NATIVE_UTF8_TO_I8(ch) (ch)
#define I8_TO_NATIVE_UTF8(ch) (ch)
#else
#define NATIVE_UTF8_TO_I8(ch) (__ASSERT_(FITS_IN_8_BITS(ch)) ((U8) (ch)))
#define I8_TO_NATIVE_UTF8(ch) (__ASSERT_(FITS_IN_8_BITS(ch)) ((U8) (ch)))
#endif

/* Transforms in wide UV chars */
#define UNI_TO_NATIVE(ch)        ((UV) (ch))
#define NATIVE_TO_UNI(ch)        ((UV) (ch))

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

/* Is the representation of the Unicode code point 'cp' the same regardless of
 * being encoded in UTF-8 or not? */
#define OFFUNI_IS_INVARIANT(cp)     isASCII(cp)

/* Is the representation of the code point 'cp' the same regardless of
 * being encoded in UTF-8 or not?  'cp' is native if < 256; Unicode otherwise
 * */
#define UVCHR_IS_INVARIANT(cp)      OFFUNI_IS_INVARIANT(cp)

/* This defines the bits that are to be in the continuation bytes of a multi-byte
 * UTF-8 encoded character that mark it is a continuation byte. */
#define UTF_CONTINUATION_MARK		0x80

/* Misleadingly named: is the UTF8-encoded byte 'c' part of a variant sequence
 * in UTF-8?  This is the inverse of UTF8_IS_INVARIANT.  The |0 makes sure this
 * isn't mistakenly called with a ptr argument */
#define UTF8_IS_CONTINUED(c)        (((U8)((c) | 0)) &  UTF_CONTINUATION_MARK)

/* Is the byte 'c' the first byte of a multi-byte UTF8-8 encoded sequence?
 * This doesn't catch invariants (they are single-byte).  It also excludes the
 * illegal overlong sequences that begin with C0 and C1.  The |0 makes sure
 * this isn't mistakenly called with a ptr argument */
#define UTF8_IS_START(c)            (((U8)((c) | 0)) >= 0xc2)

/* For use in UTF8_IS_CONTINUATION() below */
#define UTF_IS_CONTINUATION_MASK    0xC0

/* Is the byte 'c' part of a multi-byte UTF8-8 encoded sequence, and not the
 * first byte thereof?  The |0 makes sure this isn't mistakenly called with a
 * ptr argument */
#define UTF8_IS_CONTINUATION(c)                                             \
    ((((U8)((c) | 0)) & UTF_IS_CONTINUATION_MASK) == UTF_CONTINUATION_MARK)

/* Is the UTF8-encoded byte 'c' the first byte of a two byte sequence?  Use
 * UTF8_IS_NEXT_CHAR_DOWNGRADEABLE() instead if the input isn't known to
 * be well-formed.  Masking with 0xfe allows the low bit to be 0 or 1; thus
 * this matches 0xc[23].  The |0 makes sure this isn't mistakenly called with a
 * ptr argument */
#define UTF8_IS_DOWNGRADEABLE_START(c)	((((U8)((c) | 0)) & 0xfe) == 0xc2)

/* Is the UTF8-encoded byte 'c' the first byte of a sequence of bytes that
 * represent a code point > 255?  The |0 makes sure this isn't mistakenly
 * called with a ptr argument */
#define UTF8_IS_ABOVE_LATIN1(c)     (((U8)((c) | 0)) >= 0xc4)

/* This is the number of low-order bits a continuation byte in a UTF-8 encoded
 * sequence contributes to the specification of the code point.  In the bit
 * maps above, you see that the first 2 bits are a constant '10', leaving 6 of
 * real information */
#define UTF_ACCUMULATION_SHIFT		6

/* ^? is defined to be DEL on ASCII systems.  See the definition of toCTRL()
 * for more */
#define QUESTION_MARK_CTRL  DEL_NATIVE

/* Surrogates, non-character code points and above-Unicode code points are
 * problematic in some contexts.  This allows code that needs to check for
 * those to to quickly exclude the vast majority of code points it will
 * encounter */
#define isUTF8_POSSIBLY_PROBLEMATIC(c) ((U8) c >= 0xED)

#endif /* EBCDIC vs ASCII */

/* 2**UTF_ACCUMULATION_SHIFT - 1 */
#define UTF_CONTINUATION_MASK  ((U8) ((1U << UTF_ACCUMULATION_SHIFT) - 1))

/* Internal macro to be used only in this file to aid in constructing other
 * publicly accessible macros.
 * The number of bytes required to express this uv in UTF-8, for just those
 * uv's requiring 2 through 6 bytes, as these are common to all platforms and
 * word sizes.  The number of bytes needed is given by the number of leading 1
 * bits in the start byte.  There are 32 start bytes that have 2 initial 1 bits
 * (C0-DF); there are 16 that have 3 initial 1 bits (E0-EF); 8 that have 4
 * initial 1 bits (F0-F8); 4 that have 5 initial 1 bits (F9-FB), and 2 that
 * have 6 initial 1 bits (FC-FD).  The largest number a string of n bytes can
 * represent is       (the number of possible start bytes for 'n')
 *                  * (the number of possiblities for each start byte
 * The latter in turn is
 *                  2  ** (  (how many continuation bytes there are)
 *                         * (the number of bits of information each
 *                            continuation byte holds))
 *
 * If we were on a platform where we could use a fast find first set bit
 * instruction (or count leading zeros instruction) this could be replaced by
 * using that to find the log2 of the uv, and divide that by the number of bits
 * of information in each continuation byte, adjusting for large cases and how
 * much information is in a start byte for that length */
#define __COMMON_UNI_SKIP(uv)                                               \
          (UV) (uv) < (32 * (1U << (    UTF_ACCUMULATION_SHIFT))) ? 2 :     \
          (UV) (uv) < (16 * (1U << (2 * UTF_ACCUMULATION_SHIFT))) ? 3 :     \
          (UV) (uv) < ( 8 * (1U << (3 * UTF_ACCUMULATION_SHIFT))) ? 4 :     \
          (UV) (uv) < ( 4 * (1U << (4 * UTF_ACCUMULATION_SHIFT))) ? 5 :     \
          (UV) (uv) < ( 2 * (1U << (5 * UTF_ACCUMULATION_SHIFT))) ? 6 :

/* Internal macro to be used only in this file.
 * This adds to __COMMON_UNI_SKIP the details at this platform's upper range.
 * For any-sized EBCDIC platforms, or 64-bit ASCII ones, we need one more test
 * to see if just 7 bytes is needed, or if the maximum is needed.  For 32-bit
 * ASCII platforms, everything is representable by 7 bytes */
#if defined(UV_IS_QUAD) || defined(EBCDIC)
#   define __BASE_UNI_SKIP(uv) (__COMMON_UNI_SKIP(uv)                       \
     (UV) (uv) < ((UV) 1U << (6 * UTF_ACCUMULATION_SHIFT)) ? 7 : UTF8_MAXBYTES)
#else
#   define __BASE_UNI_SKIP(uv) (__COMMON_UNI_SKIP(uv) 7)
#endif

/* The next two macros use the base macro defined above, and add in the tests
 * at the low-end of the range, for just 1 byte, yielding complete macros,
 * publicly accessible. */

/* Input is a true Unicode (not-native) code point */
#define OFFUNISKIP(uv) (OFFUNI_IS_INVARIANT(uv) ? 1 : __BASE_UNI_SKIP(uv))

/*

=for apidoc Am|STRLEN|UVCHR_SKIP|UV cp
returns the number of bytes required to represent the code point C<cp> when
encoded as UTF-8.  C<cp> is a native (ASCII or EBCDIC) code point if less than
255; a Unicode code point otherwise.

=cut
 */
#define UVCHR_SKIP(uv) ( UVCHR_IS_INVARIANT(uv) ? 1 : __BASE_UNI_SKIP(uv))

/* As explained in the comments for __COMMON_UNI_SKIP, 32 start bytes with
 * UTF_ACCUMULATION_SHIFT bits of information each */
#define MAX_UTF8_TWO_BYTE (32 * (1U << UTF_ACCUMULATION_SHIFT) - 1)

/* constrained by EBCDIC which has 5 bits per continuation byte */
#define MAX_PORTABLE_UTF8_TWO_BYTE (32 * (1U << 5) - 1)

/* The maximum number of UTF-8 bytes a single Unicode character can
 * uppercase/lowercase/fold into.  Unicode guarantees that the maximum
 * expansion is UTF8_MAX_FOLD_CHAR_EXPAND characters, but any above-Unicode
 * code point will fold to itself, so we only have to look at the expansion of
 * the maximum Unicode code point.  But this number may be less than the space
 * occupied by a very large code point under Perl's extended UTF-8.  We have to
 * make it large enough to fit any single character.  (It turns out that ASCII
 * and EBCDIC differ in which is larger) */
#define UTF8_MAXBYTES_CASE	                                                \
        (UTF8_MAXBYTES >= (UTF8_MAX_FOLD_CHAR_EXPAND * OFFUNISKIP(0x10FFFF))    \
                           ? UTF8_MAXBYTES                                      \
                           : (UTF8_MAX_FOLD_CHAR_EXPAND * OFFUNISKIP(0x10FFFF)))

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

/* This defines the 1-bits that are to be in the first byte of a multi-byte
 * UTF-8 encoded character that mark it as a start byte and give the number of
 * bytes that comprise the character. 'len' is the number of bytes in the
 * multi-byte sequence. */
#define UTF_START_MARK(len) (((len) >  7) ? 0xFF : (0xFF & (0xFE << (7-(len)))))

/* Masks out the initial one bits in a start byte, leaving the real data ones.
 * Doesn't work on an invariant byte.  'len' is the number of bytes in the
 * multi-byte sequence that comprises the character. */
#define UTF_START_MASK(len) (((len) >= 7) ? 0x00 : (0x1F >> ((len)-2)))

/* Adds a UTF8 continuation byte 'new' of information to a running total code
 * point 'old' of all the continuation bytes so far.  This is designed to be
 * used in a loop to convert from UTF-8 to the code point represented.  Note
 * that this is asymmetric on EBCDIC platforms, in that the 'new' parameter is
 * the UTF-EBCDIC byte, whereas the 'old' parameter is a Unicode (not EBCDIC)
 * code point in process of being generated */
#define UTF8_ACCUMULATE(old, new) (((old) << UTF_ACCUMULATION_SHIFT)           \
                                   | ((NATIVE_UTF8_TO_I8((U8)new))             \
                                       & UTF_CONTINUATION_MASK))

/* If a value is anded with this, and the result is non-zero, then using the
 * original value in UTF8_ACCUMULATE will overflow, shifting bits off the left
 * */
#define UTF_ACCUMULATION_OVERFLOW_MASK					\
    (((UV) UTF_CONTINUATION_MASK) << ((sizeof(UV) * CHARBITS)           \
           - UTF_ACCUMULATION_SHIFT))

/* This works in the face of malformed UTF-8. */
#define UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(s, e) (UTF8_IS_DOWNGRADEABLE_START(*s) \
                                               && ( (e) - (s) > 1)             \
                                               && UTF8_IS_CONTINUATION(*((s)+1)))

/* Number of bytes a code point occupies in UTF-8. */
#define NATIVE_SKIP(uv) UVCHR_SKIP(uv)

/* Most code which says UNISKIP is really thinking in terms of native code
 * points (0-255) plus all those beyond.  This is an imprecise term, but having
 * it means existing code continues to work.  For precision, use UVCHR_SKIP,
 * NATIVE_SKIP, or OFFUNISKIP */
#define UNISKIP(uv)   UVCHR_SKIP(uv)

/* Longer, but more accurate name */
#define UTF8_IS_ABOVE_LATIN1_START(c)     UTF8_IS_ABOVE_LATIN1(c)

/* Convert a UTF-8 variant Latin1 character to a native code point value.
 * Needs just one iteration of accumulate.  Should be used only if it is known
 * that the code point is < 256, and is not UTF-8 invariant.  Use the slower
 * but more general TWO_BYTE_UTF8_TO_NATIVE() which handles any code point
 * representable by two bytes (which turns out to be up through
 * MAX_PORTABLE_UTF8_TWO_BYTE).  The two parameters are:
 *  HI: a downgradable start byte;
 *  LO: continuation.
 * */
#define EIGHT_BIT_UTF8_TO_NATIVE(HI, LO)                                        \
    ( __ASSERT_(UTF8_IS_DOWNGRADEABLE_START(HI))                                \
      __ASSERT_(UTF8_IS_CONTINUATION(LO))                                       \
     LATIN1_TO_NATIVE(UTF8_ACCUMULATE((                                         \
                            NATIVE_UTF8_TO_I8(HI) & UTF_START_MASK(2)), (LO))))

/* Convert a two (not one) byte utf8 character to a native code point value.
 * Needs just one iteration of accumulate.  Should not be used unless it is
 * known that the two bytes are legal: 1) two-byte start, and 2) continuation.
 * Note that the result can be larger than 255 if the input character is not
 * downgradable */
#define TWO_BYTE_UTF8_TO_NATIVE(HI, LO) \
    ( __ASSERT_(PL_utf8skip[HI] == 2)                                           \
      __ASSERT_(UTF8_IS_CONTINUATION(LO))                                       \
     UNI_TO_NATIVE(UTF8_ACCUMULATE((NATIVE_UTF8_TO_I8(HI) & UTF_START_MASK(2)), \
                                   (LO))))

/* Should never be used, and be deprecated */
#define TWO_BYTE_UTF8_TO_UNI(HI, LO) NATIVE_TO_UNI(TWO_BYTE_UTF8_TO_NATIVE(HI, LO))

/*

=for apidoc Am|STRLEN|UTF8SKIP|char* s
returns the number of bytes in the UTF-8 encoded character whose first (perhaps
only) byte is pointed to by C<s>.

=cut
 */
#define UTF8SKIP(s)  PL_utf8skip[*(const U8*)(s)]
#define UTF8_SKIP(s) UTF8SKIP(s)

/* Most code that says 'UNI_' really means the native value for code points up
 * through 255 */
#define UNI_IS_INVARIANT(cp)   UVCHR_IS_INVARIANT(cp)

/* Is the byte 'c' the same character when encoded in UTF-8 as when not.  This
 * works on both UTF-8 encoded strings and non-encoded, as it returns TRUE in
 * each for the exact same set of bit patterns.  It is valid on a subset of
 * what UVCHR_IS_INVARIANT is valid on, so can just use that; and the compiler
 * should optimize out anything extraneous given the implementation of the
 * latter.  The |0 makes sure this isn't mistakenly called with a ptr argument.
 * */
#define UTF8_IS_INVARIANT(c)	UVCHR_IS_INVARIANT((c) | 0)

/* Like the above, but its name implies a non-UTF8 input, which as the comments
 * above show, doesn't matter as to its implementation */
#define NATIVE_BYTE_IS_INVARIANT(c)	UVCHR_IS_INVARIANT(c)

/* The macros in the next 4 sets are used to generate the two utf8 or utfebcdic
 * bytes from an ordinal that is known to fit into exactly two (not one) bytes;
 * it must be less than 0x3FF to work across both encodings. */

/* These two are helper macros for the other three sets, and should not be used
 * directly anywhere else.  'translate_function' is either NATIVE_TO_LATIN1
 * (which works for code points up through 0xFF) or NATIVE_TO_UNI which works
 * for any code point */
#define __BASE_TWO_BYTE_HI(c, translate_function)                               \
           (__ASSERT_(! UVCHR_IS_INVARIANT(c))                                  \
            I8_TO_NATIVE_UTF8((translate_function(c) >> UTF_ACCUMULATION_SHIFT) \
                              | UTF_START_MARK(2)))
#define __BASE_TWO_BYTE_LO(c, translate_function)                               \
             (__ASSERT_(! UVCHR_IS_INVARIANT(c))                                \
              I8_TO_NATIVE_UTF8((translate_function(c) & UTF_CONTINUATION_MASK) \
                                 | UTF_CONTINUATION_MARK))

/* The next two macros should not be used.  They were designed to be usable as
 * the case label of a switch statement, but this doesn't work for EBCDIC.  Use
 * regen/unicode_constants.pl instead */
#define UTF8_TWO_BYTE_HI_nocast(c)  __BASE_TWO_BYTE_HI(c, NATIVE_TO_UNI)
#define UTF8_TWO_BYTE_LO_nocast(c)  __BASE_TWO_BYTE_LO(c, NATIVE_TO_UNI)

/* The next two macros are used when the source should be a single byte
 * character; checked for under DEBUGGING */
#define UTF8_EIGHT_BIT_HI(c) (__ASSERT_(FITS_IN_8_BITS(c))                    \
                             ( __BASE_TWO_BYTE_HI(c, NATIVE_TO_LATIN1)))
#define UTF8_EIGHT_BIT_LO(c) (__ASSERT_(FITS_IN_8_BITS(c))                    \
                             (__BASE_TWO_BYTE_LO(c, NATIVE_TO_LATIN1)))

/* These final two macros in the series are used when the source can be any
 * code point whose UTF-8 is known to occupy 2 bytes; they are less efficient
 * than the EIGHT_BIT versions on EBCDIC platforms.  We use the logical '~'
 * operator instead of "<=" to avoid getting compiler warnings.
 * MAX_UTF8_TWO_BYTE should be exactly all one bits in the lower few
 * places, so the ~ works */
#define UTF8_TWO_BYTE_HI(c)                                                    \
       (__ASSERT_((sizeof(c) ==  1)                                            \
                  || !(((WIDEST_UTYPE)(c)) & ~MAX_UTF8_TWO_BYTE))              \
        (__BASE_TWO_BYTE_HI(c, NATIVE_TO_UNI)))
#define UTF8_TWO_BYTE_LO(c)                                                    \
       (__ASSERT_((sizeof(c) ==  1)                                            \
                  || !(((WIDEST_UTYPE)(c)) & ~MAX_UTF8_TWO_BYTE))              \
        (__BASE_TWO_BYTE_LO(c, NATIVE_TO_UNI)))

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
#define isIDFIRST_lazy_if(p,UTF) ((IN_BYTES || !UTF)                \
				 ? isIDFIRST(*(p))                  \
				 : isIDFIRST_utf8((const U8*)p))
#define isWORDCHAR_lazy_if(p,UTF)   ((IN_BYTES || (!UTF))           \
				 ? isWORDCHAR(*(p))                 \
				 : isWORDCHAR_utf8((const U8*)p))
#define isALNUM_lazy_if(p,UTF)   isWORDCHAR_lazy_if(p,UTF)

#define UTF8_MAXLEN UTF8_MAXBYTES

/* A Unicode character can fold to up to 3 characters */
#define UTF8_MAX_FOLD_CHAR_EXPAND 3

#define IN_BYTES (CopHINTS_get(PL_curcop) & HINT_BYTES)

/*

=for apidoc Am|bool|DO_UTF8|SV* sv
Returns a bool giving whether or not the PV in C<sv> is to be treated as being
encoded in UTF-8.

You should use this I<after> a call to C<SvPV()> or one of its variants, in
case any call to string overloading updates the internal UTF-8 encoding flag.

=cut
*/
#define DO_UTF8(sv) (SvUTF8(sv) && !IN_BYTES)

/* Should all strings be treated as Unicode, and not just UTF-8 encoded ones?
 * Is so within 'feature unicode_strings' or 'locale :not_characters', and not
 * within 'use bytes'.  UTF-8 locales are not tested for here, but perhaps
 * could be */
#define IN_UNI_8_BIT                                                             \
	    (((CopHINTS_get(PL_curcop) & (HINT_UNI_8_BIT))                       \
               || (CopHINTS_get(PL_curcop) & HINT_LOCALE_PARTIAL                 \
                   /* -1 below is for :not_characters */                         \
                   && _is_in_locale_category(FALSE, -1)))                        \
              && ! IN_BYTES)


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

/* Code points which never were part of the original UTF-8 standard, which only
 * went up to 2 ** 31 - 1.  Note that these all overflow a signed 32-bit word,
 * The first byte of these code points is FE or FF on ASCII platforms.  If the
 * first byte is FF, it will overflow a 32-bit word. */
#define UTF8_DISALLOW_ABOVE_31_BIT      0x0800
#define UTF8_WARN_ABOVE_31_BIT          0x1000

/* For back compat, these old names are misleading for UTF_EBCDIC */
#define UTF8_DISALLOW_FE_FF             UTF8_DISALLOW_ABOVE_31_BIT
#define UTF8_WARN_FE_FF                 UTF8_WARN_ABOVE_31_BIT

#define UTF8_CHECK_ONLY			0x2000

/* For backwards source compatibility.  They do nothing, as the default now
 * includes what they used to mean.  The first one's meaning was to allow the
 * just the single non-character 0xFFFF */
#define UTF8_ALLOW_FFFF 0
#define UTF8_ALLOW_SURROGATE 0

#define UTF8_DISALLOW_ILLEGAL_INTERCHANGE                                       \
                       ( UTF8_DISALLOW_SUPER|UTF8_DISALLOW_NONCHAR              \
                        |UTF8_DISALLOW_SURROGATE)
#define UTF8_WARN_ILLEGAL_INTERCHANGE \
                         (UTF8_WARN_SUPER|UTF8_WARN_NONCHAR|UTF8_WARN_SURROGATE)
#define UTF8_ALLOW_ANY                                                          \
	    (~( UTF8_DISALLOW_ILLEGAL_INTERCHANGE|UTF8_DISALLOW_ABOVE_31_BIT    \
               |UTF8_WARN_ILLEGAL_INTERCHANGE|UTF8_WARN_ABOVE_31_BIT))
#define UTF8_ALLOW_ANYUV                                                        \
         (UTF8_ALLOW_EMPTY                                                      \
	  & ~(UTF8_DISALLOW_ILLEGAL_INTERCHANGE|UTF8_WARN_ILLEGAL_INTERCHANGE))
#define UTF8_ALLOW_DEFAULT		(ckWARN(WARN_UTF8) ? 0 : \
					 UTF8_ALLOW_ANYUV)

/* Several of the macros below have a second parameter that is currently
 * unused; but could be used in the future to make sure that the input is
 * well-formed. */

#define UTF8_IS_SURROGATE(s, e) cBOOL(is_SURROGATE_utf8(s))
#define UTF8_IS_REPLACEMENT(s, send) cBOOL(is_REPLACEMENT_utf8_safe(s,send))

/*		  ASCII		     EBCDIC I8
 * U+10FFFF: \xF4\x8F\xBF\xBF	\xF9\xA1\xBF\xBF\xBF	max legal Unicode
 * U+110000: \xF4\x90\x80\x80	\xF9\xA2\xA0\xA0\xA0
 * U+110001: \xF4\x90\x80\x81	\xF9\xA2\xA0\xA0\xA1
 *
 * BE AWARE that this test doesn't rule out malformed code points, in
 * particular overlongs */
#ifdef EBCDIC /* Both versions assume well-formed UTF8 */
#   define UTF8_IS_SUPER(s, e) (NATIVE_UTF8_TO_I8(* (U8*) (s)) >= 0xF9          \
                         && (NATIVE_UTF8_TO_I8(* (U8*) (s)) > 0xF9              \
                             || (NATIVE_UTF8_TO_I8(* ((U8*) (s) + 1)) >= 0xA2)))
#else
#   define UTF8_IS_SUPER(s, e) (*(U8*) (s) >= 0xF4                              \
                           && (*(U8*) (s) > 0xF4 || (*((U8*) (s) + 1) >= 0x90)))
#endif

/* These are now machine generated, and the 'given' clause is no longer
 * applicable */
#define UTF8_IS_NONCHAR_GIVEN_THAT_NON_SUPER_AND_GE_PROBLEMATIC(s, e)          \
                                                    cBOOL(is_NONCHAR_utf8(s))
#define UTF8_IS_NONCHAR(s, e)                                                  \
                UTF8_IS_NONCHAR_GIVEN_THAT_NON_SUPER_AND_GE_PROBLEMATIC(s, e)

#define UNICODE_SURROGATE_FIRST		0xD800
#define UNICODE_SURROGATE_LAST		0xDFFF
#define UNICODE_REPLACEMENT		0xFFFD
#define UNICODE_BYTE_ORDER_MARK		0xFEFF

/* Though our UTF-8 encoding can go beyond this,
 * let's be conservative and do as Unicode says. */
#define PERL_UNICODE_MAX	0x10FFFF

#define UNICODE_WARN_SURROGATE        0x0001	/* UTF-16 surrogates */
#define UNICODE_WARN_NONCHAR          0x0002	/* Non-char code points */
#define UNICODE_WARN_SUPER            0x0004	/* Above 0x10FFFF */
#define UNICODE_WARN_ABOVE_31_BIT     0x0008	/* Above 0x7FFF_FFFF */
#define UNICODE_DISALLOW_SURROGATE    0x0010
#define UNICODE_DISALLOW_NONCHAR      0x0020
#define UNICODE_DISALLOW_SUPER        0x0040
#define UNICODE_DISALLOW_ABOVE_31_BIT 0x0080
#define UNICODE_WARN_ILLEGAL_INTERCHANGE                                      \
            (UNICODE_WARN_SURROGATE|UNICODE_WARN_NONCHAR|UNICODE_WARN_SUPER)
#define UNICODE_DISALLOW_ILLEGAL_INTERCHANGE                                  \
 (UNICODE_DISALLOW_SURROGATE|UNICODE_DISALLOW_NONCHAR|UNICODE_DISALLOW_SUPER)

/* For backward source compatibility, as are now the default */
#define UNICODE_ALLOW_SURROGATE 0
#define UNICODE_ALLOW_SUPER	0
#define UNICODE_ALLOW_ANY	0

/* This matches the 2048 code points between UNICODE_SURROGATE_FIRST (0xD800) and
 * UNICODE_SURROGATE_LAST (0xDFFF) */
#define UNICODE_IS_SURROGATE(uv)        (((UV) (uv) & (~0xFFFF | 0xF800))       \
                                                                    == 0xD800)

#define UNICODE_IS_REPLACEMENT(uv)	((UV) (uv) == UNICODE_REPLACEMENT)
#define UNICODE_IS_BYTE_ORDER_MARK(uv)	((UV) (uv) == UNICODE_BYTE_ORDER_MARK)

/* Is 'uv' one of the 32 contiguous-range noncharacters? */
#define UNICODE_IS_32_CONTIGUOUS_NONCHARS(uv)      ((UV) (uv) >= 0xFDD0         \
                                                 && (UV) (uv) <= 0xFDEF)

/* Is 'uv' one of the 34 plane-ending noncharacters 0xFFFE, 0xFFFF, 0x1FFFE,
 * 0x1FFFF, ... 0x10FFFE, 0x10FFFF, given that we know that 'uv' is not above
 * the Unicode legal max */
#define UNICODE_IS_END_PLANE_NONCHAR_GIVEN_NOT_SUPER(uv)                        \
                                              (((UV) (uv) & 0xFFFE) == 0xFFFE)

#define UNICODE_IS_NONCHAR(uv)                                                  \
    (   UNICODE_IS_32_CONTIGUOUS_NONCHARS(uv)                                   \
     || (   LIKELY( ! UNICODE_IS_SUPER(uv))                                     \
         && UNICODE_IS_END_PLANE_NONCHAR_GIVEN_NOT_SUPER(uv)))

#define UNICODE_IS_SUPER(uv)    ((UV) (uv) > PERL_UNICODE_MAX)
#define UNICODE_IS_ABOVE_31_BIT(uv)    ((UV) (uv) > 0x7FFFFFFF)

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
#ifdef LATIN_CAPITAL_LETTER_SHARP_S_UTF8
#   define LATIN_CAPITAL_LETTER_SHARP_S	        0x1E9E
#endif
#define LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE   0x130
#define LATIN_SMALL_LETTER_DOTLESS_I            0x131
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
	 isALPHA_FOLD_EQ((input)[0], 's'))

#define SHARP_S_SKIP 2

/* If you want to exclude surrogates, and beyond legal Unicode, see the blame
 * log for earlier versions which gave details for these */

/* A helper macro for isUTF8_CHAR, so use that one, and not this one.  This is
 * retained solely for backwards compatibility and may be deprecated and
 * removed in a future Perl version.
 *
 * regen/regcharclass.pl generates is_UTF8_CHAR_utf8() macros for up to these
 * number of bytes.  So this has to be coordinated with that file */
#ifdef EBCDIC
#   define IS_UTF8_CHAR_FAST(n) ((n) <= 3)
#else
#   define IS_UTF8_CHAR_FAST(n) ((n) <= 4)
#endif

#ifndef EBCDIC
/* A helper macro for isUTF8_CHAR, so use that one instead of this.  This was
 * generated by regen/regcharclass.pl, and then moved here.  The lines that
 * generated it were then commented out.  This was done solely because it takes
 * on the order of 10 minutes to generate, and is never going to change, unless
 * the generated code is improved.
 *
 * The EBCDIC versions have been cut to not cover all of legal Unicode,
 * otherwise they take too long to generate; besides there is a separate one
 * for each code page, so they are in regcharclass.h instead of here */
/*
	UTF8_CHAR: Matches legal UTF-8 encoded characters from 2 through 4 bytes

	0x80 - 0x1FFFFF
*/
/*** GENERATED CODE ***/
#define is_UTF8_CHAR_utf8_no_length_checks(s)                               \
( ( 0xC2 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xDF ) ?                          \
    ( ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ? 2 : 0 )                          \
: ( 0xE0 == ((U8*)s)[0] ) ?                                                 \
    ( ( ( ( ((U8*)s)[1] & 0xE0 ) == 0xA0 ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) ? 3 : 0 )\
: ( 0xE1 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xEF ) ?                          \
    ( ( ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) ? 3 : 0 )\
: ( 0xF0 == ((U8*)s)[0] ) ?                                                 \
    ( ( ( ( 0x90 <= ((U8*)s)[1] && ((U8*)s)[1] <= 0xBF ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[3] & 0xC0 ) == 0x80 ) ) ? 4 : 0 )\
: ( ( ( ( 0xF1 <= ((U8*)s)[0] && ((U8*)s)[0] <= 0xF7 ) && ( ( ((U8*)s)[1] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[2] & 0xC0 ) == 0x80 ) ) && ( ( ((U8*)s)[3] & 0xC0 ) == 0x80 ) ) ? 4 : 0 )
#endif

/*

=for apidoc Am|STRLEN|isUTF8_CHAR|const U8 *s|const U8 *e

Returns the number of bytes beginning at C<s> which form a legal UTF-8 (or
UTF-EBCDIC) encoded character, looking no further than S<C<e - s>> bytes into
C<s>.  Returns 0 if the sequence starting at C<s> through S<C<e - 1>> is not
well-formed UTF-8.

Note that an INVARIANT character (i.e. ASCII on non-EBCDIC
machines) is a valid UTF-8 character.

=cut
*/

#define isUTF8_CHAR(s, e)   (UNLIKELY((e) <= (s))                           \
                             ? 0                                            \
                             : (UTF8_IS_INVARIANT(*s))                      \
                               ? 1                                          \
                               : UNLIKELY(((e) - (s)) < UTF8SKIP(s))        \
                                 ? 0                                        \
                                 : LIKELY(IS_UTF8_CHAR_FAST(UTF8SKIP(s)))   \
                                   ? is_UTF8_CHAR_utf8_no_length_checks(s)  \
                                   : _is_utf8_char_slow(s, e))

#define is_utf8_char_buf(buf, buf_end) isUTF8_CHAR(buf, buf_end)

/* Do not use; should be deprecated.  Use isUTF8_CHAR() instead; this is
 * retained solely for backwards compatibility */
#define IS_UTF8_CHAR(p, n)      (isUTF8_CHAR(p, (p) + (n)) == n)

#endif /* H_UTF8 */

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
