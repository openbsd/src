/*    utfebcdic.h
 *
 *    Copyright (C) 2001, 2002, 2003, 2005, 2006, 2007, 2009,
 *    2010, 2011 by Larry Wall, Nick Ing-Simmons, and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * Macros to implement UTF-EBCDIC as perl's internal encoding
 * Adapted from version 7.1 of Unicode Technical Report #16:
 *  http://www.unicode.org/unicode/reports/tr16
 *
 * To summarize, the way it works is:
 * To convert an EBCDIC code point to UTF-EBCDIC:
 *  1)	convert to Unicode.  No conversion is necesary for code points above
 *      255, as Unicode and EBCDIC are identical in this range.  For smaller
 *      code points, the conversion is done by lookup in the PL_e2a table (with
 *      inverse PL_a2e) in the generated file 'ebcdic_tables.h'.  The 'a'
 *      stands for ASCII platform, meaning 0-255 Unicode.
 *  2)	convert that to a utf8-like string called I8 ('I' stands for
 *	intermediate) with variant characters occupying multiple bytes.  This
 *	step is similar to the utf8-creating step from Unicode, but the details
 *	are different.  This transformation is called UTF8-Mod.  There is a
 *	chart about the bit patterns in a comment later in this file.  But
 *	essentially here are the differences:
 *			    UTF8		I8
 *	invariant byte	    starts with 0	starts with 0 or 100
 *	continuation byte   starts with 10	starts with 101
 *	start byte	    same in both: if the code point requires N bytes,
 *			    then the leading N bits are 1, followed by a 0.  If
 *			    all 8 bits in the first byte are 1, the code point
 *			    will occupy 14 bytes (compared to 13 in Perl's
 *			    extended UTF-8).  This is incompatible with what
 *			    tr16 implies should be the representation of code
 *			    points 2**30 and above, but allows Perl to be able
 *			    to represent all code points that fit in a 64-bit
 *			    word in either our extended UTF-EBCDIC or UTF-8.
 *  3)	Use the algorithm in tr16 to convert each byte from step 2 into
 *	final UTF-EBCDIC.  This is done by table lookup from a table
 *	constructed from the algorithm, reproduced in ebcdic_tables.h as
 *	PL_utf2e, with its inverse being PL_e2utf.  They are constructed so that
 *	all EBCDIC invariants remain invariant, but no others do, and the first
 *	byte of a variant will always have its upper bit set.  But note that
 *	the upper bit of some invariants is also 1.  The table also is designed
 *	so that lexically comparing two UTF-EBCDIC-variant characters yields
 *	the Unicode code point order.  (To get native code point order, one has
 *	to convert the latin1-range characters to their native code point
 *	value.)
 *
 *  For example, the ordinal value of 'A' is 193 in EBCDIC, and also is 193 in
 *  UTF-EBCDIC.  Step 1) converts it to 65, Step 2 leaves it at 65, and Step 3
 *  converts it back to 193.  As an example of how a variant character works,
 *  take LATIN SMALL LETTER Y WITH DIAERESIS, which is typically 0xDF in
 *  EBCDIC.  Step 1 converts it to the Unicode value, 0xFF.  Step 2 converts
 *  that to two bytes = 11000111 10111111 = C7 BF, and Step 3 converts those to
 *  0x8B 0x73.
 *
 * If you're starting from Unicode, skip step 1.  For UTF-EBCDIC to straight
 * EBCDIC, reverse the steps.
 *
 * The EBCDIC invariants have been chosen to be those characters whose Unicode
 * equivalents have ordinal numbers less than 160, that is the same characters
 * that are expressible in ASCII, plus the C1 controls.  So there are 160
 * invariants instead of the 128 in UTF-8.
 *
 * The purpose of Step 3 is to make the encoding be invariant for the chosen
 * characters.  This messes up the convenient patterns found in step 2, so
 * generally, one has to undo step 3 into a temporary to use them.  However,
 * one "shadow", or parallel table, PL_utf8skip, has been constructed that
 * doesn't require undoing things.  It is such that for each byte, it says
 * how long the sequence is if that (UTF-EBCDIC) byte were to begin it
 *
 * There are actually 3 slightly different UTF-EBCDIC encodings in
 * ebcdic_tables.h, one for each of the code pages recognized by Perl.  That
 * means that there are actually three different sets of tables, one for each
 * code page.  (If Perl is compiled on platforms using another EBCDIC code
 * page, it may not compile, or Perl may silently mistake it for one of the
 * three.)
 *
 * Note that tr16 actually only specifies one version of UTF-EBCDIC, based on
 * the 1047 encoding, and which is supposed to be used for all code pages.
 * But this doesn't work.  To illustrate the problem, consider the '^' character.
 * On a 037 code page it is the single byte 176, whereas under 1047 UTF-EBCDIC
 * it is the single byte 95.  If Perl implemented tr16 exactly, it would mean
 * that changing a string containing '^' to UTF-EBCDIC would change that '^'
 * from 176 to 95 (and vice-versa), violating the rule that ASCII-range
 * characters are the same in UTF-8 or not.  Much code in Perl assumes this
 * rule.  See for example
 * http://grokbase.com/t/perl/mvs/025xf0yhmn/utf-ebcdic-for-posix-bc-malformed-utf-8-character
 * What Perl does is create a version of UTF-EBCDIC suited to each code page;
 * the one for the 1047 code page is identical to what's specified in tr16.
 * This complicates interchanging files between computers using different code
 * pages.  Best is to convert to I8 before sending them, as the I8
 * representation is the same no matter what the underlying code page is.
 *
 * Because of the way UTF-EBCDIC is constructed, the lowest 32 code points that
 * aren't equivalent to ASCII characters nor C1 controls form the set of
 * continuation bytes; the remaining 64 non-ASCII, non-control code points form
 * the potential start bytes, in order.  (However, the first 5 of these lead to
 * malformed overlongs, so there really are only 59 start bytes, and the first
 * three of the 59 are the start bytes for the Latin1 range.)  Hence the
 * UTF-EBCDIC for the smallest variant code point, 0x160, will have likely 0x41
 * as its continuation byte, provided 0x41 isn't an ASCII or C1 equivalent.
 * And its start byte will be the code point that is 37 (32+5) non-ASCII,
 * non-control code points past it.  (0 - 3F are controls, and 40 is SPACE,
 * leaving 41 as the first potentially available one.)  In contrast, on ASCII
 * platforms, the first 64 (not 32) non-ASCII code points are the continuation
 * bytes.  And the first 2 (not 5) potential start bytes form overlong
 * malformed sequences.
 *
 * EBCDIC characters above 0xFF are the same as Unicode in Perl's
 * implementation of all 3 encodings, so for those Step 1 is trivial.
 *
 * (Note that the entries for invariant characters are necessarily the same in
 * PL_e2a and PL_e2utf; likewise for their inverses.)
 *
 * UTF-EBCDIC strings are the same length or longer than UTF-8 representations
 * of the same string.  The maximum code point representable as 2 bytes in
 * UTF-EBCDIC is 0x3FFF, instead of 0x7FFF in UTF-8.
 */

START_EXTERN_C

#ifdef DOINIT

#include "ebcdic_tables.h"

#else
EXTCONST U8 PL_utf8skip[];
EXTCONST U8 PL_e2utf[];
EXTCONST U8 PL_utf2e[];
EXTCONST U8 PL_e2a[];
EXTCONST U8 PL_a2e[];
EXTCONST U8 PL_fold[];
EXTCONST U8 PL_fold_latin1[];
EXTCONST U8 PL_latin1_lc[];
EXTCONST U8 PL_mod_latin1_uc[];
#endif

END_EXTERN_C

/* EBCDIC-happy ways of converting native code to UTF-8 */

/* Use these when ch is known to be < 256 */
#define NATIVE_TO_LATIN1(ch)            (__ASSERT_(FITS_IN_8_BITS(ch)) PL_e2a[(U8)(ch)])
#define LATIN1_TO_NATIVE(ch)            (__ASSERT_(FITS_IN_8_BITS(ch)) PL_a2e[(U8)(ch)])

/* Use these on bytes */
#define NATIVE_UTF8_TO_I8(b)           (__ASSERT_(FITS_IN_8_BITS(b)) PL_e2utf[(U8)(b)])
#define I8_TO_NATIVE_UTF8(b)           (__ASSERT_(FITS_IN_8_BITS(b)) PL_utf2e[(U8)(b)])

/* Transforms in wide UV chars */
#define NATIVE_TO_UNI(ch)    (FITS_IN_8_BITS(ch) ? NATIVE_TO_LATIN1(ch) : (UV) (ch))
#define UNI_TO_NATIVE(ch)    (FITS_IN_8_BITS(ch) ? LATIN1_TO_NATIVE(ch) : (UV) (ch))

/* How wide can a single UTF-8 encoded character become in bytes. */
/* NOTE: Strictly speaking Perl's UTF-8 should not be called UTF-8 since UTF-8
 * is an encoding of Unicode, and Unicode's upper limit, 0x10FFFF, can be
 * expressed with 5 bytes.  However, Perl thinks of UTF-8 as a way to encode
 * non-negative integers in a binary format, even those above Unicode.  14 is
 * the smallest number that covers 2**64
 *
 * WARNING: This number must be in sync with the value in
 * regen/charset_translations.pl. */
#define UTF8_MAXBYTES 14

/*
  The following table is adapted from tr16, it shows the I8 encoding of Unicode code points.

        Unicode                         U32 Bit pattern 1st Byte 2nd Byte 3rd Byte 4th Byte 5th Byte 6th Byte 7th Byte
    U+0000..U+007F                     000000000xxxxxxx 0xxxxxxx
    U+0080..U+009F                     00000000100xxxxx 100xxxxx
    U+00A0..U+03FF                     000000yyyyyxxxxx 110yyyyy 101xxxxx
    U+0400..U+3FFF                     00zzzzyyyyyxxxxx 1110zzzz 101yyyyy 101xxxxx
    U+4000..U+3FFFF                 0wwwzzzzzyyyyyxxxxx 11110www 101zzzzz 101yyyyy 101xxxxx
   U+40000..U+3FFFFF            0vvwwwwwzzzzzyyyyyxxxxx 111110vv 101wwwww 101zzzzz 101yyyyy 101xxxxx
  U+400000..U+3FFFFFF       0uvvvvvwwwwwzzzzzyyyyyxxxxx 1111110u 101vvvvv 101wwwww 101zzzzz 101yyyyy 101xxxxx
 U+4000000..U+3FFFFFFF 00uuuuuvvvvvwwwwwzzzzzyyyyyxxxxx 11111110 101uuuuu 101vvvvv 101wwwww 101zzzzz 101yyyyy 101xxxxx

Beyond this, Perl uses an incompatible extension, similar to the one used in
regular UTF-8.  There are now 14 bytes.  A full 32 bits of information thus looks like this:
                                                        1st Byte  2nd-7th 8th Byte 9th Byte 10th B   11th B   12th B   13th B   14th B
U+40000000..U+FFFFFFFF ttuuuuuvvvvvwwwwwzzzzzyyyyyxxxxx 11111111 10100000 101000tt 101uuuuu 101vvvvv 101wwwww 101zzzzz 101yyyyy 101xxxxx

For 32-bit words, the 2nd through 7th bytes effectively function as leading
zeros.  Above 32 bits, these fill up, with each byte yielding 5 bits of
information, so that with 13 continuation bytes, we can handle 65 bits, just
above what a 64 bit word can hold */


/* This is a fundamental property of UTF-EBCDIC */
#define OFFUNI_IS_INVARIANT(c) (((UV)(c)) <  0xA0)

/* It turns out that on EBCDIC platforms, the invariants are the characters
 * that have ASCII equivalents, plus the C1 controls.  Since the C0 controls
 * and DELETE are ASCII, this is the same as: (isASCII(uv) || isCNTRL_L1(uv))
 * */
#define UVCHR_IS_INVARIANT(uv) cBOOL(FITS_IN_8_BITS(uv)                        \
   && (PL_charclass[(U8) (uv)] & (_CC_mask(_CC_ASCII) | _CC_mask(_CC_CNTRL))))

/* UTF-EBCDIC semantic macros - We used to transform back into I8 and then
 * compare, but now only have to do a single lookup by using a bit in
 * l1_char_class_tab.h.
 * Comments as to the meaning of each are given at their corresponding utf8.h
 * definitions. */

#define UTF8_IS_START(c)		_generic_isCC(c, _CC_UTF8_IS_START)

#define UTF_IS_CONTINUATION_MASK    0xE0

#define UTF8_IS_CONTINUATION(c)		_generic_isCC(c, _CC_UTF8_IS_CONTINUATION)

/* The above instead could be written as this:
#define UTF8_IS_CONTINUATION(c)                                                 \
            (((NATIVE_UTF8_TO_I8(c) & UTF_IS_CONTINUATION_MASK)                 \
                                                == UTF_CONTINUATION_MARK)
 */

/* Equivalent to ! UVCHR_IS_INVARIANT(c) */
#define UTF8_IS_CONTINUED(c) 		cBOOL(FITS_IN_8_BITS(c)                 \
   && ! (PL_charclass[(U8) (c)] & (_CC_mask(_CC_ASCII) | _CC_mask(_CC_CNTRL))))

#define UTF8_IS_DOWNGRADEABLE_START(c)   _generic_isCC(c,                       \
                                              _CC_UTF8_IS_DOWNGRADEABLE_START)

/* Equivalent to (UTF8_IS_START(c) && ! UTF8_IS_DOWNGRADEABLE_START(c))
 * Makes sure that the START bit is set and the DOWNGRADEABLE bit isn't */
#define UTF8_IS_ABOVE_LATIN1(c) cBOOL(FITS_IN_8_BITS(c)                         \
  && ((PL_charclass[(U8) (c)] & ( _CC_mask(_CC_UTF8_IS_START)                   \
                                 |_CC_mask(_CC_UTF8_IS_DOWNGRADEABLE_START)))   \
                        == _CC_mask(_CC_UTF8_IS_START)))

#define isUTF8_POSSIBLY_PROBLEMATIC(c)                                          \
                _generic_isCC(c, _CC_UTF8_START_BYTE_IS_FOR_AT_LEAST_SURROGATE)

#define UTF_CONTINUATION_MARK		0xA0
#define UTF_ACCUMULATION_SHIFT		5

/* ^? is defined to be APC on EBCDIC systems.  See the definition of toCTRL()
 * for more */
#define QUESTION_MARK_CTRL   LATIN1_TO_NATIVE(0x9F)

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
