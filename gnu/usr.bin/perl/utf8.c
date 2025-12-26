/*    utf8.c
 *
 *    Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'What a fix!' said Sam.  'That's the one place in all the lands we've ever
 *  heard of that we don't want to see any closer; and that's the one place
 *  we're trying to get to!  And that's just where we can't get, nohow.'
 *
 *     [p.603 of _The Lord of the Rings_, IV/I: "The Taming of Sméagol"]
 *
 * 'Well do I understand your speech,' he answered in the same language;
 * 'yet few strangers do so.  Why then do you not speak in the Common Tongue,
 *  as is the custom in the West, if you wish to be answered?'
 *                           --Gandalf, addressing Théoden's door wardens
 *
 *     [p.508 of _The Lord of the Rings_, III/vi: "The King of the Golden Hall"]
 *
 * ...the travellers perceived that the floor was paved with stones of many
 * hues; branching runes and strange devices intertwined beneath their feet.
 *
 *     [p.512 of _The Lord of the Rings_, III/vi: "The King of the Golden Hall"]
 */

#include "EXTERN.h"
#define PERL_IN_UTF8_C
#include "perl.h"
#include "invlist_inline.h"

static const char malformed_text[] = "Malformed UTF-8 character";
static const char unees[] =
                        "Malformed UTF-8 character (unexpected end of string)";

/*
These are various utility functions for manipulating UTF8-encoded
strings.  For the uninitiated, this is a method of representing arbitrary
Unicode characters as a variable number of bytes, in such a way that
characters in the ASCII range are unmodified, and a zero byte never appears
within non-zero characters.
*/

void
Perl_force_out_malformed_utf8_message_(pTHX_
            const U8 *const p,      /* First byte in UTF-8 sequence */
            const U8 * const e,     /* Final byte in sequence (may include
                                       multiple chars */
                  U32 flags,        /* Flags to pass to utf8_to_uv(),
                                       usually 0, or some DISALLOW flags */
            const bool die_here)    /* If TRUE, this function does not return */
{
    PERL_ARGS_ASSERT_FORCE_OUT_MALFORMED_UTF8_MESSAGE_;

    /* This core-only function is to be called when a malformed UTF-8 character
     * is found, in order to output the detailed information about the
     * malformation before dieing.  The reason it exists is for the occasions
     * when such a malformation is fatal, but warnings might be turned off, so
     * that normally they would not be actually output.  This ensures that they
     * do get output.  Because a sequence may be malformed in more than one
     * way, multiple messages may be generated, so we can't make them fatal, as
     * that would cause the first one to die.
     *
     * Instead we pretend -W was passed to perl, then die afterwards.  The
     * flexibility is here to return to the caller so they can finish up and
     * die themselves */
    U32 errors;
    UV dummy;

    flags &= ~UTF8_CHECK_ONLY;
    flags |= (die_here) ? UTF8_DIE_IF_MALFORMED
                        : UTF8_FORCE_WARN_IF_MALFORMED;
    (void) utf8_to_uv_errors(p, e, &dummy, NULL, flags, &errors);

    if (! errors) {
        croak("panic: force_out_malformed_utf8_message_ should"
                  " be called only when there are errors found");
    }
}

STATIC HV *
S_new_msg_hv(pTHX_ const char * const message, /* The message text */
                   U32 categories,  /* Packed warning categories */
                   U32 flag)        /* Flag associated with this message */
{
    /* Creates, populates, and returns an HV* that describes an error message
     * for the translators between UTF8 and code point */

    SV* msg_sv = newSVpv(message, 0);
    SV* category_sv = newSVuv(categories);
    SV* flag_bit_sv = newSVuv(flag);

    HV* msg_hv = newHV();

    PERL_ARGS_ASSERT_NEW_MSG_HV;

    (void) hv_stores(msg_hv, "text", msg_sv);
    (void) hv_stores(msg_hv, "warn_categories",  category_sv);
    (void) hv_stores(msg_hv, "flag_bit", flag_bit_sv);

    return msg_hv;
}

/*
=for apidoc uvoffuni_to_utf8_flags

THIS FUNCTION SHOULD BE USED IN ONLY VERY SPECIALIZED CIRCUMSTANCES.
Instead, B<Almost all code should use L<perlapi/uv_to_utf8> or
L<perlapi/uv_to_utf8_flags>>.

This function is like them, but the input is a strict Unicode
(as opposed to native) code point.  Only in very rare circumstances should code
not be using the native code point.

For details, see the description for L<perlapi/uv_to_utf8_flags>.

=cut
*/

U8 *
Perl_uvoffuni_to_utf8_flags(pTHX_ U8 *d, UV uv, const UV flags)
{
    PERL_ARGS_ASSERT_UVOFFUNI_TO_UTF8_FLAGS;

    return uvoffuni_to_utf8_flags_msgs(d, uv, flags, NULL);
}

/* All these formats take a single UV code point argument */
const char surrogate_cp_format[] = "UTF-16 surrogate U+%04" UVXf;
const char nonchar_cp_format[]   = "Unicode non-character U+%04" UVXf
                                   " is not recommended for open interchange";
const char super_cp_format[]     = "Code point 0x%" UVXf " is not Unicode,"
                                   " may not be portable";

/*  Use shorter names internally in this file */
#define SHIFT   UTF_ACCUMULATION_SHIFT
#undef  MARK
#define MARK    UTF_CONTINUATION_MARK
#define MASK    UTF_CONTINUATION_MASK

/*
=for apidoc      uv_to_utf8_msgs
=for apidoc_item uvchr_to_utf8_flags_msgs

These functions are identical.  THEY SHOULD BE USED IN ONLY VERY SPECIALIZED
CIRCUMSTANCES.

Most code should use C<L</uv_to_utf8_flags>()> rather than call this directly.

This function is for code that wants any warning and/or error messages to be
returned to the caller rather than be displayed.  Any message that would have
been displayed if all lexical warnings are enabled will instead be returned.

It is just like C<L</uvchr_to_utf8_flags>> but it takes an extra parameter
placed after all the others, C<msgs>.  If this parameter is 0, this function
behaves identically to C<L</uvchr_to_utf8_flags>>.  Otherwise, C<msgs> should
be a pointer to an C<HV *> variable, in which this function creates a new HV to
contain any appropriate message.  The hash has three key-value pairs, as
follows:

=over 4

=item C<text>

The text of the message as a C<SVpv>.

=item C<warn_categories>

The warning category (or categories) packed into a C<SVuv>.

=item C<flag_bit>

A single flag bit associated with this message, in a C<SVuv>.
The bit corresponds to some bit in the C<*errors> return value.
The possibilities are:

=over

=item C<UNICODE_GOT_SURROGATE>

=item C<UNICODE_GOT_NONCHAR>

=item C<UNICODE_GOT_SUPER>

=item C<UNICODE_GOT_PERL_EXTENDED>

=back

=back

It's important to note that specifying this parameter as non-null will cause
any warning this function would otherwise generate to be suppressed, and
instead be placed in C<*msgs>.  The caller can check the lexical warnings state
(or not) when choosing what to do with the returned message.

Only a single message is returned; if a code point requires Perl extended UTF-8
to represent, it is also above-Unicode.  If either the
C<UNICODE_WARN_PERL_EXTENDED> or C<UNICODE_DISALLOW_PERL_EXTENDED> flags are
set, the return is controlled by them; if neither is set, the return is
controlled by the  C<UNICODE_WARN_SUPER> and C<UNICODE_DISALLOW_SUPER> flags.

The caller, of course, is responsible for freeing any returned HV.

=cut
*/

/* Undocumented; we don't want people using this.  Instead they should use
 * uv_to_utf8_msgs() */
U8 *
Perl_uvoffuni_to_utf8_flags_msgs(pTHX_ U8 *d, UV input_uv, UV flags, HV** msgs)
{
    U8 *p;
    UV shifted_uv = input_uv;
    STRLEN utf8_skip = OFFUNISKIP(input_uv);

    PERL_ARGS_ASSERT_UVOFFUNI_TO_UTF8_FLAGS_MSGS;

    if (msgs) {
        *msgs = NULL;
    }

    switch (utf8_skip) {
      case 1:
        *d++ = LATIN1_TO_NATIVE(input_uv);
        return d;

      default:
        if (   UNLIKELY(input_uv > MAX_LEGAL_CP
            && UNLIKELY(! (flags & UNICODE_ALLOW_ABOVE_IV_MAX))))
        {
            croak("%s", form_cp_too_large_msg(16, /* Hex output */
                                                         NULL, 0, input_uv));
        }

        if ((flags & (UNICODE_WARN_PERL_EXTENDED|UNICODE_WARN_SUPER))) {
            U32 category = packWARN2(WARN_NON_UNICODE, WARN_PORTABLE);
            const char * format = PL_extended_cp_format;
            if (msgs) {
                *msgs = new_msg_hv(form(format, input_uv),
                                   category,
                                   (flags & UNICODE_WARN_PERL_EXTENDED)
                                   ? UNICODE_GOT_PERL_EXTENDED
                                   : UNICODE_GOT_SUPER);
            }
            else {
                ck_warner_d(category, format, input_uv);
            }

            /* Don't output a 2nd msg */
            flags &= ~UNICODE_WARN_SUPER;
        }

        if (flags & UNICODE_DISALLOW_PERL_EXTENDED) {
            return NULL;
        }

        p = d + utf8_skip - 1;
        while (p >= d + 6 + ONE_IF_EBCDIC_ZERO_IF_NOT) {
            *p-- = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
            shifted_uv >>= SHIFT;
        }

        /* FALLTHROUGH */

      case 6 + ONE_IF_EBCDIC_ZERO_IF_NOT:
        d[5 + ONE_IF_EBCDIC_ZERO_IF_NOT]
                                = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
        shifted_uv >>= SHIFT;
        /* FALLTHROUGH */

      case 5 + ONE_IF_EBCDIC_ZERO_IF_NOT:
        d[4 + ONE_IF_EBCDIC_ZERO_IF_NOT]
                                = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
        shifted_uv >>= SHIFT;
        /* FALLTHROUGH */

      case 4 + ONE_IF_EBCDIC_ZERO_IF_NOT:
        if (UNLIKELY(UNICODE_IS_SUPER(input_uv))) {
            if (flags & UNICODE_WARN_SUPER) {
                U32 category = packWARN(WARN_NON_UNICODE);
                const char * format = super_cp_format;

                if (msgs) {
                    *msgs = new_msg_hv(form(format, input_uv),
                                       category,
                                       UNICODE_GOT_SUPER);
                }
                else {
                    ck_warner_d(category, format, input_uv);
                }

                if (flags & UNICODE_DISALLOW_SUPER) {
                    return NULL;
                }
            }
            if (       (flags & UNICODE_DISALLOW_SUPER)
                || (   (flags & UNICODE_DISALLOW_PERL_EXTENDED)
                    &&  UNICODE_IS_PERL_EXTENDED(input_uv)))
            {
                return NULL;
            }
        }

        d[3 + ONE_IF_EBCDIC_ZERO_IF_NOT]
                                = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
        shifted_uv >>= SHIFT;
        /* FALLTHROUGH */

      case 3 + ONE_IF_EBCDIC_ZERO_IF_NOT:
        if (isUNICODE_POSSIBLY_PROBLEMATIC(input_uv)) {
            if (UNLIKELY(UNICODE_IS_NONCHAR(input_uv))) {
                if (flags & UNICODE_WARN_NONCHAR) {
                    U32 category = packWARN(WARN_NONCHAR);
                    const char * format = nonchar_cp_format;
                    if (msgs) {
                        *msgs = new_msg_hv(form(format, input_uv),
                                           category,
                                           UNICODE_GOT_NONCHAR);
                    }
                    else {
                        ck_warner_d(category, format, input_uv);
                    }
                }
                if (flags & UNICODE_DISALLOW_NONCHAR) {
                    return NULL;
                }
            }
            else if (UNLIKELY(UNICODE_IS_SURROGATE(input_uv))) {
                if (flags & UNICODE_WARN_SURROGATE) {
                    U32 category = packWARN(WARN_SURROGATE);
                    const char * format = surrogate_cp_format;
                    if (msgs) {
                        *msgs = new_msg_hv(form(format, input_uv),
                                           category,
                                           UNICODE_GOT_SURROGATE);
                    }
                    else {
                        ck_warner_d(category, format, input_uv);
                    }
                }
                if (flags & UNICODE_DISALLOW_SURROGATE) {
                    return NULL;
                }
            }
        }

        d[2 + ONE_IF_EBCDIC_ZERO_IF_NOT]
                                = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
        shifted_uv >>= SHIFT;
        /* FALLTHROUGH */

#ifdef EBCDIC

      case 3:
        d[2] = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
        shifted_uv >>= SHIFT;
        /* FALLTHROUGH */

#endif

        /* FALLTHROUGH */
      case 2:
        d[1] = I8_TO_NATIVE_UTF8((shifted_uv & MASK) | MARK);
        shifted_uv >>= SHIFT;
        d[0] = I8_TO_NATIVE_UTF8((shifted_uv & UTF_START_MASK(utf8_skip))
                                             | UTF_START_MARK(utf8_skip));
        break;
    }

    return d + utf8_skip;
}

/*
=for apidoc      uv_to_utf8
=for apidoc_item uv_to_utf8_flags
=for apidoc_item uvchr_to_utf8
=for apidoc_item uvchr_to_utf8_flags

These each add the UTF-8 representation of the native code point C<uv> to the
end of the string C<d>; C<d> should have at least C<UVCHR_SKIP(uv)+1> (up to
C<UTF8_MAXBYTES+1>) free bytes available.  The return value is the pointer to
the byte after the end of the new character.  In other words,

    d = uv_to_utf8(d, uv);

This is the Unicode-aware way of saying

    *(d++) = uv;

(C<uvchr_to_utf8> is a synonym for C<uv_to_utf8>.)

C<uv_to_utf8_flags> is used to make some classes of code points problematic in
some way.  C<uv_to_utf8> is effectively the same as calling C<uv_to_utf8_flags>
with C<flags> set to 0, meaning no class of code point is considered
problematic.  That means any input code point from 0..C<IV_MAX> is considered
to be fine.  C<IV_MAX> is typically 0x7FFF_FFFF in a 32-bit word.

(C<uvchr_to_utf8_flags> is a synonym for C<uv_to_utf8_flags>).

A code point can be problematic in one of two ways.  Its use could just raise a
warning, and/or it could be forbidden with the function failing, and returning
NULL.

The potential classes of problematic code points and the flags that make them
so are:

If C<uv> is a Unicode surrogate code point and C<UNICODE_WARN_SURROGATE> is set,
the function will raise a warning, provided UTF8 warnings are enabled.  If
instead C<UNICODE_DISALLOW_SURROGATE> is set, the function will fail and return
NULL.  If both flags are set, the function will both warn and return NULL.

Similarly, the C<UNICODE_WARN_NONCHAR> and C<UNICODE_DISALLOW_NONCHAR> flags
affect how the function handles a Unicode non-character.

And likewise, the C<UNICODE_WARN_SUPER> and C<UNICODE_DISALLOW_SUPER> flags
affect the handling of code points that are above the Unicode maximum of
0x10FFFF.  Languages other than Perl may not be able to accept files that
contain these.

The flag C<UNICODE_WARN_ILLEGAL_INTERCHANGE> selects all three of
the above WARN flags; and C<UNICODE_DISALLOW_ILLEGAL_INTERCHANGE> selects all
three DISALLOW flags.  C<UNICODE_DISALLOW_ILLEGAL_INTERCHANGE> restricts the
allowed inputs to the strict UTF-8 traditionally defined by Unicode.
Similarly, C<UNICODE_WARN_ILLEGAL_C9_INTERCHANGE> and
C<UNICODE_DISALLOW_ILLEGAL_C9_INTERCHANGE> are shortcuts to select the
above-Unicode and surrogate flags, but not the non-character ones, as
defined in
L<Unicode Corrigendum #9|https://www.unicode.org/versions/corrigendum9.html>.
See L<perlunicode/Noncharacter code points>.

Extremely high code points were never specified in any standard, and require an
extension to UTF-8 to express, which Perl does.  It is likely that programs
written in something other than Perl would not be able to read files that
contain these; nor would Perl understand files written by something that uses a
different extension.  For these reasons, there is a separate set of flags that
can warn and/or disallow these extremely high code points, even if other
above-Unicode ones are accepted.  They are the C<UNICODE_WARN_PERL_EXTENDED>
and C<UNICODE_DISALLOW_PERL_EXTENDED> flags.  For more information see
C<L</UTF8_GOT_PERL_EXTENDED>>.  Of course C<UNICODE_DISALLOW_SUPER> will
treat all above-Unicode code points, including these, as malformations.  (Note
that the Unicode standard considers anything above 0x10FFFF to be illegal, but
there are standards predating it that allow up to 0x7FFF_FFFF (2**31 -1))

A somewhat misleadingly named synonym for C<UNICODE_WARN_PERL_EXTENDED> is
retained for backward compatibility: C<UNICODE_WARN_ABOVE_31_BIT>.  Similarly,
C<UNICODE_DISALLOW_ABOVE_31_BIT> is usable instead of the more accurately named
C<UNICODE_DISALLOW_PERL_EXTENDED>.  The names are misleading because on EBCDIC
platforms,these flags can apply to code points that actually do fit in 31 bits.
The new names accurately describe the situation in all cases.

=for apidoc Amnh||UNICODE_DISALLOW_ABOVE_31_BIT
=for apidoc Amnh||UNICODE_DISALLOW_ILLEGAL_C9_INTERCHANGE
=for apidoc Amnh||UNICODE_DISALLOW_ILLEGAL_INTERCHANGE
=for apidoc Amnh||UNICODE_DISALLOW_NONCHAR
=for apidoc Amnh||UNICODE_DISALLOW_PERL_EXTENDED
=for apidoc Amnh||UNICODE_DISALLOW_SUPER
=for apidoc Amnh||UNICODE_DISALLOW_SURROGATE
=for apidoc Amnh||UNICODE_WARN_ABOVE_31_BIT
=for apidoc Amnh||UNICODE_WARN_ILLEGAL_C9_INTERCHANGE
=for apidoc Amnh||UNICODE_WARN_ILLEGAL_INTERCHANGE
=for apidoc Amnh||UNICODE_WARN_NONCHAR
=for apidoc Amnh||UNICODE_WARN_PERL_EXTENDED
=for apidoc Amnh||UNICODE_WARN_SUPER
=for apidoc Amnh||UNICODE_WARN_SURROGATE

=cut
*/

PERL_STATIC_INLINE int
S_is_utf8_overlong(const U8 * const s, const STRLEN len)
{
    /* Returns an int indicating whether or not the UTF-8 sequence from 's' to
     * 's' + 'len' - 1 is an overlong.  It returns a positive number if it is
     * an overlong; 0 if it isn't, and -1 if there isn't enough information to
     * tell.  This last return value can happen if the sequence is incomplete,
     * missing some trailing bytes that would form a complete character.  If
     * there are enough bytes to make a definitive decision, this function does
     * so.
     *
     * The positive number returned when it is overlong is how many bytes
     * needed to be examined to make that determination.  Usually 1 or 2 bytes
     * are sufficient.
     *
     * Overlongs can occur for a few of the smallest start bytes or whenever
     * the number of continuation bytes changes.  The latter means whenever the
     * number of leading 1 bits in a start byte increases from the next lower
     * start byte.  That happens for start bytes C0, E0, F0, F8, FC, FE, and
     * FF. */

    PERL_ARGS_ASSERT_IS_UTF8_OVERLONG;

    /* Each platform has overlongs after the start bytes given above (expressed
     * in I8 for EBCDIC).  The values below were found by manually inspecting
     * the UTF-8 patterns.  See the tables in utf8.h and utfebcdic.h. */

    switch (NATIVE_UTF8_TO_I8(s[0])) {
      default:
        assert(UTF8_IS_START(s[0]));
        return 0;

      case 0xC0:
      case 0xC1:
        return 1;

#ifdef EBCDIC

      case 0xC2:
      case 0xC3:
      case 0xC4:
      case 0xE0:
        return 1;
#else
      case 0xE0:
        return (len < 2) ? -1 : (s[1] < 0xA0) ? 2 : 0;
#endif

      case 0xF0:
      case 0xF8:
      case 0xFC:
      case 0xFE:
        return (len < 2)
               ? -1     /* This pattern encapsulates
                         * F0 => 0x10; F8 => 0x08; FC => 0x04; FF => 0x02 */
               : (NATIVE_UTF8_TO_I8(s[1]) < UTF_MIN_CONTINUATION_BYTE
                                          + 0x100 - NATIVE_UTF8_TO_I8(s[0]))
                 ? 2
                 : 0;
      case 0xFF:
        return isFF_overlong(s, len);
    }
}

PERL_STATIC_INLINE int
S_isFF_overlong(const U8 * const s, const STRLEN len)
{
    /* Returns an int indicating whether or not the UTF-8 sequence from 's' to
     * 'e' - 1 is an overlong beginning with \xFF.  It returns a positive
     * number if it is; 0 if it isn't, and -1 if there isn't enough
     * information to tell.  This last return value can happen if the sequence
     * is incomplete, missing some trailing bytes that would form a complete
     * character.  If there are enough bytes to make a definitive decision,
     * this function does so.
     *
     * A positive return gives the number of bytes needed to be examined to
     * make the determination */

    PERL_ARGS_ASSERT_ISFF_OVERLONG;

#ifdef EBCDIC
    /* This works on all three EBCDIC code pages traditionally supported by
     * perl */
#  define FF_OVERLONG_PREFIX "\xfe\x41\x41\x41\x41\x41\x41\x41"
#else
#  define FF_OVERLONG_PREFIX "\xff\x80\x80\x80\x80\x80\x80"
#endif

    /* To be an FF overlong, all the available bytes must match */
    if (LIKELY(memNE(s, FF_OVERLONG_PREFIX,
                     MIN(len, STRLENs(FF_OVERLONG_PREFIX)))))
    {
        return 0;
    }

    /* To be an FF overlong sequence, all the bytes in FF_OVERLONG_PREFIX must
     * be there; what comes after them doesn't matter.  See tables in utf8.h,
     * utfebcdic.h. */
    if (len >= STRLENs(FF_OVERLONG_PREFIX)) {
        return STRLENs(FF_OVERLONG_PREFIX);
    }

    /* The missing bytes could cause the result to go one way or the other, so
     * the result is indeterminate */
    return -1;
}

/* At some point we may want to allow core to use up to UV_MAX */

#ifdef EBCDIC     /* Actually is I8 */
#  if defined(UV_IS_QUAD) /* These assume IV_MAX is 2**63-1, UV_MAX 2**64-1 */
#    define HIGHEST_REPRESENTABLE_UTF  "\xFF\xA7"
                              /* UV_MAX "\xFF\xAF" */
#  else      /* These assume IV_MAX is 2**31-1, UV_MAX 2**32-1 */
#    define HIGHEST_REPRESENTABLE_UTF  "\xFF\xA0\xA0\xA0\xA0\xA0\xA0\xA1"
                              /* UV_MAX "\xFF\xA0\xA0\xA0\xA0\xA0\xA0\xA3" */
#  endif
#else
#  if defined(UV_IS_QUAD)
#    define HIGHEST_REPRESENTABLE_UTF  "\xFF\x80\x87"
                              /* UV_MAX "\xFF\x80" */
#  else
#    define HIGHEST_REPRESENTABLE_UTF  "\xFD"
                              /* UV_MAX "\xFE\x83" */
#  endif
#endif

PERL_STATIC_INLINE int
S_does_utf8_overflow(const U8 * const s, const U8 * e)
{
    PERL_ARGS_ASSERT_DOES_UTF8_OVERFLOW;

    /* Returns an int indicating whether or not the UTF-8 sequence from 's' to
     * 'e' - 1 would overflow an IV on this platform; that is if it represents
     * a code point larger than the highest representable code point.  The
     * possible returns are: */
#define NO_OVERFLOW                 0   /* Definitely doesn't overflow */

/* There aren't enough examinable bytes available to be sure.  This can happen
 * if the sequence is incomplete, missing some trailing bytes that would form a
 * complete character. */
#define COULD_OVERFLOW              1

/* This overflows if not also overlong, and like COULD_OVERFLOW, there aren't
 * enough available bytes to be sure, but since overlongs are very rarely
 * encountered, for most purposes consider it to overflow */
#define ALMOST_CERTAINLY_OVERFLOWS  2

#define OVERFLOWS                   3   /* Definitely overflows */

    /* Note that the values are ordered so that you can use '>=' in checking
     * the return value. */

    const STRLEN len = e - s;
    const U8 *x;
    const U8 * y = (const U8 *) HIGHEST_REPRESENTABLE_UTF;

    for (x = s; x < e; x++, y++) {

        /* 'y' is set up to not include the trailing bytes that are all the
         * maximum possible continuation byte.  So when we reach the end of 'y'
         * (known to be NUL terminated), it is impossible for 'x' to contain
         * bytes larger than those omitted bytes, and therefore 'x' can't
         * overflow */
        if (*y == '\0') {
            return NO_OVERFLOW;
        }

        /* If this byte is less than the corresponding highest non-overflowing
         * UTF-8, the sequence doesn't overflow */
        if (NATIVE_UTF8_TO_I8(*x) < *y) {
            return NO_OVERFLOW;
        }

        if (UNLIKELY(NATIVE_UTF8_TO_I8(*x) > *y)) {
            goto overflows_if_not_overlong;
        }
    }

    /* Got to the end, and all bytes are the same.  If the input is a whole
     * character, it doesn't overflow.  And if it is a partial character,
     * there's not enough information to tell */
    return (len >= STRLENs(HIGHEST_REPRESENTABLE_UTF)) ? NO_OVERFLOW
                                                       : COULD_OVERFLOW;

  overflows_if_not_overlong: ;

    /* Here, the sequence overflows if not overlong.  Check for that */
    int is_overlong = is_utf8_overlong(s, len);
    if (LIKELY(is_overlong == 0)) {
        return OVERFLOWS;
    }

    /* Not long enough to determine */
    if (is_overlong < 0) {
        return ALMOST_CERTAINLY_OVERFLOWS;
    }

    /* Here, it appears to overflow, but it is also overlong.  That overlong
     * may evaluate to something that doesn't overflow; or it may evaluate to
     * something that does.  Figure it out */

#if 6 * UTF_CONTINUATION_BYTE_INFO_BITS <= IVSIZE * CHARBITS

    /* On many platforms, it is impossible for an overlong to overflow.  For
     * these, no further work is necessary: we can return immediately that this
     * overlong that is an apparent overflow actually isn't
     *
     * To see why, note that a length_N sequence can represent as overlongs all
     * the code points representable by shorter length sequences, but no
     * higher.  If it could represent a higher code point without being an
     * overlong, we wouldn't have had to increase the sequence length!
     *
     * The highest possible start byte is FF; the next highest is FE.  The
     * highest code point representable as an overlong on the platform is thus
     * the highest code point representable by a non-overlong sequence whose
     * start byte is FE.  If that value doesn't overflow the platform's word
     * size, overlongs can't overflow.
     *
     * FE consists of 7 bytes total; the FE start byte contributes 0 bits of
     * information (the high 7 bits, all ones, say that the sequence is 7 bytes
     * long, and the bottom, zero, bit is 0, so doesn't add anything. That
     * leaves the 6 continuation bytes to contribute
     * UTF_CONTINUATION_BYTE_INFO_BITS each.  If that number of bits doesn't
     * exceed the word size, it can't overflow. */

    return NO_OVERFLOW;

#else

    /* In practice, only a 32-bit ASCII box gets here.  The FE start byte can
     * represent, as an overlong, the highest code point representable by an FD
     * start byte, which is 5*6 continuation bytes of info plus one bit from
     * the start byte, or 31 bits.  That doesn't overflow.  More explicitly:
     * \xFD\xBF\xBF\xBF\xBF\xBF evaluates to 0x7FFFFFFF = 2*31 - 1.
     *
     * That means only the FF start byte can have an overflowing overlong. */
    if (*s < 0xFF) {
        return NO_OVERFLOW;
    }

    /* The sequence \xff\x80\x80\x80\x80\x80\x80\x82 is an overlong that
     * evaluates to 2**31, so overflows an IV.  For a UV it's
     *              \xff\x80\x80\x80\x80\x80\x80\x83 = 2**32 */
#  define OVERFLOWS_MIN_STRING  "\xff\x80\x80\x80\x80\x80\x80\x82"

    if (e - s < (ptrdiff_t) STRLENs(OVERFLOWS_MIN_STRING)) {
        return ALMOST_CERTAINLY_OVERFLOWS;  /* Not enough info to be sure */
    }

#  define strnGE(s1,s2,l) (strncmp(s1,s2,l) >= 0)

    return (strnGE((const char *) s, OVERFLOWS_MIN_STRING, STRLENs(OVERFLOWS_MIN_STRING)))
    ? OVERFLOWS
    : NO_OVERFLOW;

#endif

}

STRLEN
Perl_is_utf8_char_helper_(const U8 * const s, const U8 * e, const U32 flags)
{
    SSize_t len, full_len;

    /* An internal helper function.
     *
     * On input:
     *  's' is a string, which is known to be syntactically valid UTF-8 as far
     *      as (e - 1); e > s must hold.
     *  'e' This function is allowed to look at any byte from 's'...'e-1', but
     *      nowhere else.  The function has to cope as best it can if that
     *      sequence does not form a full character.
     * 'flags' can be 0, or any combination of the UTF8_DISALLOW_foo flags
     *      accepted by L</utf8_to_uv>.  If non-zero, this function returns
     *      0 if it determines the input will match something disallowed.
     * On output:
     *  The return is the number of bytes required to represent the code point
     *  if it isn't disallowed by 'flags'; 0 otherwise.  Be aware that if the
     *  input is for a partial character, a successful return will be larger
     *  than 'e - s'.
     *
     *  If *s..*(e-1) is only for a partial character, the function will return
     *  non-zero if there is any sequence of well-formed UTF-8 that, when
     *  appended to the input sequence, could result in an allowed code point;
     *  otherwise it returns 0.  Non characters cannot be determined based on
     *  partial character input.  But many  of the other excluded types can be
     *  determined with just the first one or two bytes.
     *
     */

    PERL_ARGS_ASSERT_IS_UTF8_CHAR_HELPER_;

    assert(e > s);
    assert(0 == (flags & ~UTF8_DISALLOW_ILLEGAL_INTERCHANGE));

    full_len = UTF8SKIP(s);

    len = e - s;
    if (len > full_len) {
        e = s + full_len;
        len = full_len;
    }

    switch (full_len) {
        bool is_super;

      default: /* Extended */
        if (flags & UTF8_DISALLOW_PERL_EXTENDED) {
            return 0;
        }

        /* FALLTHROUGH */

      case 6 + ONE_IF_EBCDIC_ZERO_IF_NOT:   /* above Unicode */
      case 5 + ONE_IF_EBCDIC_ZERO_IF_NOT:   /* above Unicode */

        if (flags & UTF8_DISALLOW_SUPER) {
            return 0;                       /* Above Unicode */
        }

        return full_len;

      case 4 + ONE_IF_EBCDIC_ZERO_IF_NOT:
        is_super = (   UNLIKELY(NATIVE_UTF8_TO_I8(s[0]) > UTF_START_BYTE_110000_)
                    || (   len > 1
                        && NATIVE_UTF8_TO_I8(s[0]) == UTF_START_BYTE_110000_
                        && NATIVE_UTF8_TO_I8(s[1])
                                                >= UTF_FIRST_CONT_BYTE_110000_));
        if (is_super) {
            if (flags & UTF8_DISALLOW_SUPER) {
                return 0;
            }
        }
        else if (   (flags & UTF8_DISALLOW_NONCHAR)
                 && len == full_len
                 && UNLIKELY(is_LARGER_NON_CHARS_utf8(s)))
        {
            return 0;
        }

        return full_len;

      case 3 + ONE_IF_EBCDIC_ZERO_IF_NOT:

        if (! isUTF8_POSSIBLY_PROBLEMATIC(s[0]) || len < 2) {
            return full_len;
        }

        if (   (flags & UTF8_DISALLOW_SURROGATE)
            &&  UNLIKELY(is_SURROGATE_utf8(s)))
        {
            return 0;       /* Surrogate */
        }

        if (  (flags & UTF8_DISALLOW_NONCHAR)
            && len == full_len
            && UNLIKELY(is_SHORTER_NON_CHARS_utf8(s)))
        {
            return 0;
        }

        return full_len;

      /* The lower code points don't have any disallowable characters */
#ifdef EBCDIC
      case 3:
        return full_len;
#endif

      case 2:
      case 1:
        return full_len;
    }
}

Size_t
Perl_is_utf8_FF_helper_(const U8 * const s0, const U8 * const e,
                        const bool require_partial)
{
    /* This is called to determine if the UTF-8 sequence starting at s0 and
     * continuing for up to one full character of bytes, but looking no further
     * than 'e - 1', is legal.  *s0 must be 0xFF (or whatever the native
     * equivalent of FF in I8 on EBCDIC platforms is).  This marks it as being
     * for the largest code points recognized by Perl, the ones that require
     * the most UTF-8 bytes per character to represent (somewhat less than
     * twice the size of the next longest kind).  This sequence will only ever
     * be Perl extended UTF-8.
     *
     * The routine returns 0 if the sequence is not fully valid, syntactically
     * or semantically.  That means it checks that everything following the
     * start byte is a continuation byte, and that it doesn't overflow, nor is
     * an overlong representation.
     *
     * If 'require_partial' is FALSE, the routine returns non-zero only if the
     * input (as far as 'e-1') is a full character.  The return is the count of
     * the bytes in the character.
     *
     * If 'require_partial' is TRUE, the routine returns non-zero only if the
     * input as far as 'e-1' is a partial, not full character, with no
     * malformations found before position 'e'.  The return is either just
     * FALSE, or TRUE.  */

    const U8 *s = s0 + 1;
    const U8 *send = e;

    PERL_ARGS_ASSERT_IS_UTF8_FF_HELPER_;

    assert(s0 < e);
    assert(*s0 == I8_TO_NATIVE_UTF8(0xFF));

    send = s + MIN(UTF8_MAXBYTES - 1, e - s);
    while (s < send) {
        if (! UTF8_IS_CONTINUATION(*s)) {
            return 0;
        }

        s++;
    }

    if (does_utf8_overflow(s0, e) == OVERFLOWS) {
        return 0;
    }

    if (0 < isFF_overlong(s0, e - s0)) {
        return 0;
    }

    /* Here, the character is valid as far as it got.  Check if got a partial
     * character */
    if (s - s0 < UTF8_MAXBYTES) {
        return (require_partial) ? 1 : 0;
    }

    /* Here, got a full character */
    return (require_partial) ? 0 : UTF8_MAXBYTES;
}

const char *
Perl__byte_dump_string(pTHX_ const U8 * const start, const STRLEN len, const bool format)
{
    /* Returns a mortalized C string that is a displayable copy of the 'len'
     * bytes starting at 'start'.  'format' gives how to display each byte.
     * Currently, there are only two formats, so it is currently a bool:
     *      0   \xab
     *      1    ab         (that is a space between two hex digit bytes)
     */

    if (start == NULL) {
        return "(nil)";
    }

    const STRLEN output_len = 4 * len + 1;  /* 4 bytes per each input, plus a
                                               trailing NUL */
    const U8 * s = start;
    const U8 * const e = start + len;
    char * output;
    char * d;

    PERL_ARGS_ASSERT__BYTE_DUMP_STRING;

    Newx(output, output_len, char);
    SAVEFREEPV(output);

    d = output;
    for (s = start; s < e; s++) {
        const unsigned high_nibble = (*s & 0xF0) >> 4;
        const unsigned low_nibble =  (*s & 0x0F);

        if (format) {
            if (s > start) {
                *d++ = ' ';
            }
        }
        else {
            *d++ = '\\';
            *d++ = 'x';
        }

        if (high_nibble < 10) {
            *d++ = high_nibble + '0';
        }
        else {
            *d++ = high_nibble - 10 + 'a';
        }

        if (low_nibble < 10) {
            *d++ = low_nibble + '0';
        }
        else {
            *d++ = low_nibble - 10 + 'a';
        }
    }

    *d = '\0';
    return output;
}

PERL_STATIC_INLINE char *
S_unexpected_non_continuation_text(pTHX_ const U8 * const s,

                                         /* Max number of bytes to print */
                                         STRLEN print_len,

                                         /* Which one is the non-continuation */
                                         const STRLEN non_cont_byte_pos,

                                         /* How many bytes should there be? */
                                         const STRLEN expect_len)
{
    /* Return the malformation warning text for an unexpected continuation
     * byte. */

    const char * const where = (non_cont_byte_pos == 1)
                               ? "immediately"
                               : form("%d bytes",
                                                 (int) non_cont_byte_pos);
    const U8 * x = s + non_cont_byte_pos;
    const U8 * e = s + print_len;

    PERL_ARGS_ASSERT_UNEXPECTED_NON_CONTINUATION_TEXT;

    /* We don't need to pass this parameter, but since it has already been
     * calculated, it's likely faster to pass it; verify under DEBUGGING */
    assert(expect_len == UTF8SKIP(s));

    /* As a defensive coding measure, don't output anything past a NUL.  Such
     * bytes shouldn't be in the middle of a malformation, and could mark the
     * end of the allocated string, and what comes after is undefined */
    for (; x < e; x++) {
        if (*x == '\0') {
            x++;            /* Output this particular NUL */
            break;
        }
    }

    return form("%s: %s (unexpected non-continuation byte 0x%02x,"
                           " %s after start byte 0x%02x; need %d bytes, got %d)",
                           malformed_text,
                           _byte_dump_string(s, x - s, 0),
                           *(s + non_cont_byte_pos),
                           where,
                           *s,
                           (int) expect_len,
                           (int) non_cont_byte_pos);
}

/*

=for apidoc      utf8_to_uv
=for apidoc_item extended_utf8_to_uv
=for apidoc_item strict_utf8_to_uv
=for apidoc_item c9strict_utf8_to_uv
=for apidoc_item utf8_to_uv_or_die
=for apidoc_item utf8_to_uvchr_buf
=for apidoc_item utf8_to_uvchr

These functions each translate from UTF-8 to UTF-32 (or UTF-64 on 64 bit
platforms).  In other words, to a code point ordinal value.  (On EBCDIC
platforms, the initial encoding is UTF-EBCDIC, and the output is a native code
point).

For example, the string "A" would be converted to the number 65 on an ASCII
platform, and to 193 on an EBCDIC one.  Converting the string "ABC" would yield
the same results, as the functions stop after the first character converted.
Converting the string "\N{LATIN CAPITAL LETTER A WITH MACRON} plus anything
more in the string" would yield the number 0x100 on both types of platforms,
since the first character is U+0100.

The functions whose names contain C<to_uvchr> are older than the functions
whose names don't have C<chr> in them.  The API in the older functions is
harder to use correctly, and so they are kept only for backwards compatibility,
and may eventually become deprecated.  If you are writing a module and use
L<Devel::PPPort>, your code can use the new functions back to at least Perl
v5.7.1.

All the functions accept, without complaint, well-formed UTF-8 for any
non-problematic Unicode code point 0 .. 0x10FFFF.  There are two types of
Unicode problematic code points:  surrogate characters and non-character code
points.  (See L<perlunicode>.)  Some of the functions reject one or both of
these.  Private use characters and those code points yet to be assigned to a
particular character are never considered problematic.  Additionally, most of
the functions accept non-Unicode code points, those starting at 0x110000.

There are two sets of these functions:

=over 4

=item C<utf8_to_uv> forms

Almost all code should use only C<utf8_to_uv>, C<extended_utf8_to_uv>,
C<strict_utf8_to_uv>, C<c9strict_utf8_to_uv>, or C<utf8_to_uv_or_die>.  The
other functions are either the problematic old form, or are for specialized
uses.

C<utf8_to_uv_or_die> has a simpler interface than the other four, for use when
any errors encountered should be fatal.  It throws an exception with any errors
found, otherwise it returns the code point the input sequence represents.

The other four functions each return C<true> if the sequence of bytes starting
at C<s> form a complete, legal UTF-8 (or UTF-EBCDIC) sequence for a code point;
or false otherwise.  They take an extra parameter, the address of an IV,
C<&cp>.  C<*cp> will be set to the native code point value the sequence
represents, and C<*advance> will be set to its length, in bytes.

If the functions returns C<false>, C<*cp> is set to the Unicode REPLACEMENT
CHARACTER, and C<*advance> to the next position along C<s>, where the next
possible UTF-8 character could begin.  Failing to use this position as the next
starting point during parsing of strings has led to successful attacks by
crafted inputs.

The functions only examine as many bytes along C<s> as are needed to form a
complete UTF-8 representation of a single code point; they never examine the
byte at C<e>, or beyond.  They return false (or die in the case of
C<utf8_to_uv_or_die>) if the code point requires more than S<C<e - s>> bytes to
represent.

The functions differ only in what flavor of UTF-8 they accept.  All reject
syntactically invalid UTF-8.

=over 4

=item * C<strict_utf8_to_uv>

additionally rejects any UTF-8 that translates into a code point that isn't
specified by Unicode to be freely exchangeable, namely the surrogate characters
and non-character code points (besides non-Unicode code points, any above
0x10FFFF).  It does not raise a warning when rejecting these.

=item * C<c9strict_utf8_to_uv>

instead uses the exchangeable definition given by Unicode's Corregendum #9,
which accepts non-character code points while still rejecting surrogates.  It
does not raise a warning when rejecting these.

=item * C<utf8_to_uv>

=item * C<utf8_to_uv_or die>

accept all syntactically valid UTF-8, as extended by Perl to allow 64-bit code
points to be encoded.

C<extended_utf8_to_uv> is merely a synonym for C<utf8_to_uv>.  Use this form
to draw attention to the fact that it accepts any code point.  But since
Perl programs traditionally do this by default, plain C<utf8_to_uv> is the form
most often used.

=back

Whenever syntactically invalid input is rejected, an explanatory warning
message is raised, unless C<utf8> warnings (or the appropriate subcategory) are
turned off.  A given input sequence may contain multiple malformations, giving
rise to multiple warnings, as the functions attempt to find and report on all
malformations in a sequence.  All the possible malformations are listed in
C<L</utf8_to_uv_msgs>>, with some examples of multiple ones for the same
sequence.  You can use that function or C<L</utf8_to_uv_flags>> to exert more
control over the input that is considered acceptable, and the warnings that are
raised.

Often, C<s> is an arbitrarily long string containing the UTF-8 representations
of many code points in a row, and these functions are called in the course of
parsing C<s> to find all those code points.

If your code doesn't know how to deal with illegal input, as would be typical
of a low level routine, the loop could look like:

 while (s < e) {
     Size_t advance;
     UV cp;
     (void) utf8_to_uv(s, e, &cp, &advance);
     <handle 'cp'>
     s += advance;
 }

A REPLACEMENT CHARACTER will be inserted everywhere that malformed input
occurs.  Obviously, we aren't expecting such outcomes, but your code will be
protected from attacks and many harmful effects that could otherwise occur.

If the situation is such that it would be a bug for the input to be invalid, a
somewhat simpler loop suffices:

 while (s < e) {
     Size_t advance;
     UV cp = utf8_to_uv_or_die(s, e, &advance);
     <handle 'cp'>
     s += advance;
 }

This will throw an exception on invalid input, so your code doesn't have to
concern itself with that possibility.

If you do have a plan for handling malformed input, you could instead write:

 while (s < e) {
     Size_t advance;
     UV cp;

     if (UNLIKELY(! utf8_to_uv(s, e, &cp, &advance)) {
         <bail out or convert to handleable>
     }

     <handle 'cp'>

     s += advance;
 }

You may pass NULL to these functions instead of a pointer to your C<advance>
variable.  But the only legitimate case to do this is if you are only examining
the first character in C<s>, and have no plans to ever look further.  You could
also advance by using C<UTF8SKIP>, but this gives the correct result if and
only if the input is well-formed; and this practice has led to successful
attacks against such code; and it is extra work always, as the functions have
already done the equivalent work and return the correct value in C<advance>,
regardless of whether the input is well-formed or not.

Except with C<utf8_to_uv_or_die>, you must always pass a non-NULL pointer into
which to store the (first) code point C<s> represents.  If you don't care about
this value, you should be using one of the C<L</isUTF8_CHAR>> functions
instead.

=item C<utf8_to_uvchr> forms

These are the old form equivalents of C<utf8_to_uv> (and its synonym,
C<extended_utf8_to_uv>).  They are C<utf8_to_uvchr> and C<utf8_to_uvchr_buf>.
There is no old form equivalent of either C<strict_utf8_to_uv> nor
C<c9strict_utf8_to_uv>.

C<utf8_to_uvchr> is DEPRECATED.  Do NOT use it; it is a security hole ready to
bring destruction onto you and yours.

C<utf8_to_uvchr_buf> is discouraged and may eventually become deprecated.  It
checks if the sequence of bytes starting at C<s> form a complete, legal UTF-8
(or UTF-EBCDIC) sequence for a code point.  If so, it returns the code point
value the sequence represents, and C<*retlen> will be set to its length, in
bytes.  Thus, the next possible character in C<s> begins at S<C<s + *retlen>>.

The function only examines as many bytes along C<s> as are needed to form a
complete UTF-8 representation of a single code point, but it never examines
the byte at C<e>, or beyond.

If the sequence examined starting at C<s> is not legal Perl extended UTF-8, the
translation fails, and the resultant behavior unfortunately depends on if the
warnings category "utf8" is enabled or not.

=over 4

=item If C<'utf8'> warnings are disabled

The Unicode REPLACEMENT CHARACTER is silently returned, and C<*retlen> is set
(if C<retlen> isn't C<NULL>) so that (S<C<s> + C<*retlen>>) is the next
possible position in C<s> that could begin a non-malformed character.

But note that it is ambiguous whether a REPLACEMENT CHARACTER was actually in
the input, or if this function synthetically generated one.  In the unlikely
event that you care, you'd have to examine the input to disambiguate.

=item If C<'utf8'> warnings are enabled

A warning will be displayed, and 0 is returned and C<*retlen> is set (if
C<retlen> isn't C<NULL>) to -1.

But note that 0 may also be returned if S<*s> is a legal NUL character.  This
means that you have to disambiguate a 0 return.  You can do this by checking
that the first byte of C<s> is indeed a NUL; or by making sure to always pass a
non-NULL C<retlen> pointer, and by examining it.

Also note that should you wish to proceed with parsing C<s>, you have no easy
way of knowing where to start looking in it for the next possible character.
It is important to look in the right place to prevent attacks on your code.
It would be better to have instead called an equivalent function that provides
this information; any of the C<utf8_to_uv> series, or C<L</utf8n_to_uvchr>>.

=back

Because of these quirks, C<utf8_to_uvchr_buf> is very difficult to use
correctly and handle all cases.  Generally, you need to bail out at the first
failure it finds.

The deprecated C<utf8_to_uvchr> behaves the same way as C<utf8_to_uvchr_buf> for
well-formed input, and for the malformations it is capable of finding, but
doesn't find all of them, and it can read beyond the end of the input buffer,
which is why it is deprecated.

=back

The C<utf8_to_uv()> family of functions is preferred because they make it
easier to write code safe from attacks.  You should be converting to them; this
will result in simpler, more robust code.

=for apidoc      utf8_to_uv_flags
=for apidoc_item utf8n_to_uvchr

These functions are extensions of C<L</utf8_to_uv>>, where you need
more control over what UTF-8 sequences are acceptable.  These functions are
unlikely to be needed except for specialized purposes.

C<utf8n_to_uvchr> is more like an extension of C<utf8_to_uvchr_buf>, but
with fewer quirks, and a different method of specifying the bytes in C<s> it is
allowed to examine.  It has a C<curlen> parameter instead of an C<e> parameter,
so the furthest byte in C<s> it can look at is S<C<s + curlen - 1>>.  Its
return value is, like C<utf8_to_uvchr_buf>, ambiguous with respect to the NUL
and REPLACEMENT characters, but the value of C<*retlen> can be relied on
(except with the C<UTF8_CHECK_ONLY> flag described below) to know where the
next possible character along C<s> starts, removing that quirk.  Hence, you
always should use C<*retlen> to determine where the next character in C<s>
starts.

These functions have an additional parameter, C<flags>, besides the ones in
C<utf8_to_uv> and C<utf8_to_uvchr_buf>, which can be used to broaden or
restrict what is acceptable UTF-8.  C<flags> has the same meaning and behavior
in both functions.  When C<flags> is 0, these functions accept any
syntactically valid Perl-extended-UTF-8 sequence that doesn't overflow the
platform's word size.

There are flags that apply to accepting particular sequences, and flags that
apply to raising warnings about encountering sequences.  Each type is
independent of the other.  You can reject and not warn; warn and still accept;
or both reject and warn.  Rejecting means that the sequence gets translated
into the Unicode REPLACEMENT CHARACTER instead of what it was meant to
represent.

Unless otherwise stated below, warnings are subject to the C<utf8> warnings
category being on.

=over 4

=item C<UTF8_CHECK_ONLY>

This suppresses any warnings.  And it changes what is stored into
C<*retlen> with the C<uvchr> family of functions (for the worse).  It is not
likely to be of use to you.  You can use C<UTF8_ALLOW_ANY> (described below) to
also turn off warnings, and that flag doesn't adversely affect C<*retlen>.

This flag is ignored if C<UTF8_DIE_IF_MALFORMED> is also set.

=item C<UTF8_FORCE_WARN_IF_MALFORMED>

Normally, no warnings are generated if warnings are turned off lexically or
globally, regardless of any flags to the contrary.  But this flag effectively
turns on warnings temporarily for the duration of this function's execution.

Do not use it lightly.

This flag is ignored if C<UTF8_CHECK_ONLY> is also set.

=item C<UTF8_DISALLOW_SURROGATE>

=item C<UTF8_WARN_SURROGATE>

These reject and/or warn about UTF-8 sequences that represent surrogate
characters.  The warning categories C<utf8> and C<non_unicode> control if
warnings are actually raised.

=item C<UTF8_DISALLOW_NONCHAR>

=item C<UTF8_WARN_NONCHAR>

These reject and/or warn about UTF-8 sequences that represent non-character
code points.  The warning categories C<utf8> and C<nonchar> control if warnings
are actually raised.

=item C<UTF8_DISALLOW_SUPER>

=item C<UTF8_WARN_SUPER>

These reject and/or warn about UTF-8 sequences that represent code points
above 0x10FFFF.  The warning categories C<utf8> and C<non_unicode> control if
warnings are actually raised.

=item C<UTF8_DISALLOW_ILLEGAL_INTERCHANGE>

=item C<UTF8_WARN_ILLEGAL_INTERCHANGE>

These are the same as having selected all three of the corresponding SURROGATE,
NONCHAR and SUPER flags listed above.

All such code points are not considered to be safely freely exchangeable
between processes.

=item C<UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE>

=item C<UTF8_WARN_ILLEGAL_C9_INTERCHANGE>

These are the same as having selected both the corresponding SURROGATE and
SUPER flags listed above.

Unicode issued L<Unicode Corrigendum
#9|https://www.unicode.org/versions/corrigendum9.html> to allow non-character
code points to be exchanged by processes aware of the possibility.  (They are
still discouraged, however.)  For more discussion see
L<perlunicode/Noncharacter code points>.

=item C<UTF8_DISALLOW_PERL_EXTENDED>

=item C<UTF8_WARN_PERL_EXTENDED>

These reject and/or warn on encountering sequences that require Perl's
extension to UTF-8 to represent them.   These are all for code points above
0x10FFFF, so these sequences are a subset of the ones controlled by SUPER or
either of the illegal interchange sets of flags.  The warning categories
C<utf8>, C<non_unicode>, and C<portable> control if warnings are actually
raised.

Perl predates Unicode, and earlier standards allowed for code points up through
0x7FFF_FFFF (2**31 - 1).  Perl, of course, would like you to be able to
represent in UTF-8 any code point available on the platform.  To do so, some
extension must be used to express them.  Perl uses a natural extension to UTF-8
to represent the ones up to 2**36-1, and invented a further extension to
represent even higher ones, so that any code point that fits in a 64-bit word
can be represented.  We lump both of these extensions together and refer to
them as Perl extended UTF-8.  There exist other extensions that people have
invented, incompatible with Perl's.

On EBCDIC platforms starting in Perl v5.24, the Perl extension for representing
extremely high code points kicks in at 0x3FFF_FFFF (2**30 -1), which is lower
than on ASCII.  Prior to that, code points 2**31 and higher were simply
unrepresentable, and a different, incompatible method was used to represent
code points between 2**30 and 2**31 - 1.

It is likely that programs written in something other than Perl would not be
able to read files that contain these; nor would Perl understand files written
by something that uses a different extension.  Hence, you can specify that
above-Unicode code points are generally accepted and/or warned about, but still
exclude the ones that require this extension to represent.

=item C<UTF8_ALLOW_ANY> and kin

Other flags can be passed to allow, in a limited way, syntactic malformations
and/or overflowing the number of bits available in a UV on the platform.
The functions will not treat the relevant malformations as errors, hence will
not raise any warnings for them.  C<utf8_to_uv_msgs> will return C<true>.

B<However, all such malformations translate to the REPLACEMENT CHARACTER>,
regardless of any of the flags.

The only such flag that you would ever have any reason to use is
C<UTF8_ALLOW_ANY> which applies to any of the syntactic malformations and
overflow, except for empty input.  The other flags are analogous to ones in
the C<_GOT_> bits list in C<L</utf8_to_uv_msgs>>.

=item C<UTF8_DIE_IF_MALFORMED>

If the function would otherwise return C<false>, it instead croaks.  The
C<UTF8_FORCE_WARN_IF_MALFORMED> flag is effectively turned on so that the cause
of the croak is displayed.

=back

=for apidoc      utf8_to_uv_msgs
=for apidoc_item utf8n_to_uvchr_msgs
=for apidoc_item utf8_to_uv_errors
=for apidoc_item utf8n_to_uvchr_error

These functions are extensions of C<L</utf8_to_uv_flags>> and
C<L</utf8n_to_uvchr>>.  They are used for the highly specialized purpose of
when the caller needs to know the exact malformations that were encountered
and/or the diagnostics that would be raised.

They each take one or two extra parameters, pointers to where to store this
information.  The functions with C<_msgs> in their names return both types, so
take two extra parameters; those with C<_error> return just the malformations,
so take just one extra parameter.  When the extra parameters are both 0, the
functions behave identically to the function they extend.

When the C<errors> parameter is not NULL, it should be the address of a U32
variable, into which the functions store a bitmap, described just below, with a
bit set for each malformation the function found; 0 if none.  The C<ALLOW>-type
flags are ignored when determining the content of this variable.  That is, even
if you "allow" a particular malformation, if it is encountered, the
corresponding bit will be set to notify you that one was encountered.
However, the bits for conditions that are accepted by default aren't set
unless the flags passed to the function indicate that they should be
rejected or warned about when encountering them.  These are explicitly
noted in the list below along with the controlling flags.

The bits returned in C<errors> and their meanings are:

=over 4

=item C<UTF8_GOT_CONTINUATION>

The input sequence was malformed in that the first byte was a UTF-8
continuation byte.

=item C<UTF8_GOT_EMPTY>

The input parameters indicated the length of C<s> is 0.  Technically, this a
coding error, not a malformation; you should check before calling these
functions if there is actually anything to convert.  But perl needs to be able
to recover from bad input, and this is how it does it.

=item C<UTF8_GOT_LONG>

The input sequence was malformed in that there is some other sequence that
evaluates to the same code point, but that sequence is shorter than this one.

Until Unicode 3.1, it was legal for programs to accept this malformation, but
it was discovered that this created security issues.

=item C<UTF8_GOT_NONCHAR>

The code point represented by the input UTF-8 sequence is for a Unicode
non-character code point.
This bit is set only if the input C<flags> parameter contains either the
C<UTF8_DISALLOW_NONCHAR> or the C<UTF8_WARN_NONCHAR> flags.

=item C<UTF8_GOT_NON_CONTINUATION>

The input sequence was malformed in that a non-continuation-type byte was found
in a position where only a continuation-type one should be.  See also
C<L</UTF8_GOT_SHORT>>.

=item C<UTF8_GOT_OVERFLOW>

The input sequence was malformed in that it is for a code point that is not
representable in the number of bits available in an IV on the current platform.

=item C<UTF8_GOT_PERL_EXTENDED>

The input sequence is not standard UTF-8, but a Perl extension.  This bit is
set only if the input C<flags> parameter contains either the
C<UTF8_DISALLOW_PERL_EXTENDED> or the C<UTF8_WARN_PERL_EXTENDED> flags.

=item C<UTF8_GOT_SHORT>

The input sequence was malformed in that C<curlen> is smaller than required for
a complete sequence.  In other words, the input is for a partial character
sequence.

C<UTF8_GOT_SHORT> and C<UTF8_GOT_NON_CONTINUATION> both indicate a too short
sequence.  The difference is that C<UTF8_GOT_NON_CONTINUATION> indicates always
that there is an error, while C<UTF8_GOT_SHORT> means that an incomplete
sequence was looked at.   If no other flags are present, it means that the
sequence was valid as far as it went.  Depending on the application, this could
mean one of three things:

=over 4

=item *

The C<e> or C<curlen> parameters passed in were too small, and the function
was prevented from examining all the necessary bytes.

=item *

The buffer being looked at is based on reading data, and the data received so
far stopped in the middle of a character, so that the next read will
read the remainder of this character.  (It is up to the caller to deal with the
split bytes somehow.)

=item *

This is a real error, and the partial sequence is all we're going to get.

=back

=item C<UTF8_GOT_SUPER>

The input sequence was malformed in that it is for a non-Unicode code point;
that is, one above the legal Unicode maximum.
This bit is set only if the input C<flags> parameter contains either the
C<UTF8_DISALLOW_SUPER> or the C<UTF8_WARN_SUPER> flags.

=item C<UTF8_GOT_SURROGATE>

The input sequence was malformed in that it is for a Unicode UTF-16 surrogate
code point.
This bit is set only if the input C<flags> parameter contains either the
C<UTF8_DISALLOW_SURROGATE> or the C<UTF8_WARN_SURROGATE> flags.

=back

Note that more than one bit may have been set by these functions.  This is
because it is possible for multiple malformations to be present in the same
sequence.  An example would be an overlong sequence evaluating to a surrogate
when surrogates are forbidden.  Another example is overflow; standard UTF-8
never overflows, so something that does must have been expressed using Perl's
extended UTF-8.  It also is above all legal Unicode code points.  So there will
be a bit set for up to all three of these things.  1) Overflow always; 2)
perl-extended if the calling flags indicate those should be rejected or warned
about; and 3) above-Unicode, provided the calling flags indicate those should
be rejected or warned about.

If you don't care about the system's messages text nor warning categories, you
can customize error handling by calling one of the C<_error> functions, using
either of the flags C<UTF8_ALLOW_ANY> or C<UTF8_CHECK_ONLY> to suppress any
warnings, and then examine the C<*errors> return.  If you don't use those
flags, warnings will be raised as usual.

But if you do care, instead use one of the functions with C<_msgs> in their
names.  These allow you to completely customize error handling by suppressing
any warnings that would otherwise be raised; instead returning all relevant
information in a structure specified by an extra parameter, C<msgs>, a pointer
to a variable which has been declared to be an C<AV*>, and into which the
function creates a new AV to store information, described below, about all the
malformations that were encountered.

When this parameter is non-NULL, the C<UTF8_DIE_IF_MALFORMED> and
C<UTF8_FORCE_WARN_IF_MALFORMED> flags are asserted against in DEBUGGING builds,
and are ignored in non-DEBUGGING ones.  The C<UTF8_CHECK_ONLY> flag is always
ignored.

What is considered a malformation is affected by C<flags>, the same as
described in C<L</utf8_to_uv_flags>>.  No array element is generated for
malformations that are "allowed" by the input flags, in contrast to the
bitmap returned in a non-NULL C<*errors>.

Each element of the C<msgs> AV array is an anonymous hash with the following
three key-value pairs:

=over 4

=item C<text>

A C<SVpv> containing the text of the message about the problematic input.
This text is identical to any warning that otherwise would have been raised if
the appropriate warning categories were enabled.

=item C<warn_categories>

This is 0 if the C<flags> parameter to the function would ordinarily not have
caused the message to be output as a warning; otherwise it is the warning
category (or categories) that would have been used to generate a warning for
C<text>, packed into a C<SVuv>.  For example, if C<flags> contains
C<UTF8_DISALLOW_SURROGATE>, but not C<UTF8_WARN_SURROGATE>, this would be 0 if
the input was a surrogate.

=item C<flag>

A C<SVuv> containing a single flag bit associated with this message.  The bit
corresponds to some bit in the C<*errors> return value, such as
C<UTF8_GOT_LONG>.

=back

The array is sorted so that element C<[0]> contains the first message that
would have otherwise been raised; C<[1]>, the second; and so on.

You thus can completely override the normal error handling; you can check the
lexical warnings state (or not) when choosing what to do with the returned
messages.

The caller, of course, is responsible for freeing any returned AV.

=for apidoc Amnh||UTF8_ALLOW_CONTINUATION
=for apidoc Amnh||UTF8_ALLOW_EMPTY
=for apidoc Amnh||UTF8_ALLOW_LONG
=for apidoc Amnh||UTF8_ALLOW_NON_CONTINUATION
=for apidoc Amnh||UTF8_ALLOW_OVERFLOW
=for apidoc Amnh||UTF8_ALLOW_PERL_EXTENDED
=for apidoc Amnh||UTF8_ALLOW_SHORT
=for apidoc Amnh||UTF8_CHECK_ONLY
=for apidoc Amnh||UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE
=for apidoc Amnh||UTF8_DISALLOW_ILLEGAL_INTERCHANGE
=for apidoc Amnh||UTF8_DISALLOW_NONCHAR
=for apidoc Amnh||UTF8_DISALLOW_PERL_EXTENDED
=for apidoc Amnh||UTF8_DISALLOW_SUPER
=for apidoc Amnh||UTF8_DISALLOW_SURROGATE
=for apidoc Amnh||UTF8_GOT_CONTINUATION
=for apidoc Amnh||UTF8_GOT_EMPTY
=for apidoc Amnh||UTF8_GOT_LONG
=for apidoc Amnh||UTF8_GOT_NONCHAR
=for apidoc Amnh||UTF8_GOT_NON_CONTINUATION
=for apidoc Amnh||UTF8_GOT_OVERFLOW
=for apidoc Amnh||UTF8_GOT_PERL_EXTENDED
=for apidoc Amnh||UTF8_GOT_SHORT
=for apidoc Amnh||UTF8_GOT_SUPER
=for apidoc Amnh||UTF8_GOT_SURROGATE
=for apidoc Amnh||UTF8_WARN_ILLEGAL_C9_INTERCHANGE
=for apidoc Amnh||UTF8_WARN_ILLEGAL_INTERCHANGE
=for apidoc Amnh||UTF8_WARN_NONCHAR
=for apidoc Amnh||UTF8_WARN_PERL_EXTENDED
=for apidoc Amnh||UTF8_WARN_SUPER
=for apidoc Amnh||UTF8_WARN_SURROGATE

=cut
*/

bool
Perl_utf8_to_uv_msgs_helper_(const U8 * const s0,
                             const U8 * const e,
                             UV *cp_p,
                             Size_t *advance_p,
                             U32 flags,
                             U32 * errors,
                             AV ** msgs)
{
    PERL_ARGS_ASSERT_UTF8_TO_UV_MSGS_HELPER_;

    /* Here, is one of:
     *  a)  malformed;
     *  b)  a problematic code point (surrogate, non-unicode, or nonchar); or
     *  c)  on ASCII platforms, one of the Hangul syllables that the dfa
     *      doesn't properly handle.  Quickly dispose of the final case.
     */

    /* Assume will be successful; override later if necessary */
    if (UNLIKELY(errors)) {
        *errors = 0;
    }
    if (UNLIKELY(msgs)) {
        *msgs = NULL;

        /* This form of the function has higher priority than this flag */
        flags &= ~UTF8_CHECK_ONLY;
    }

    /* Each of the affected Hanguls starts with \xED */
    if (is_HANGUL_ED_utf8_safe(s0, e)) { /* Always false on EBCDIC */
        if (advance_p) {
            *advance_p = 3;
        }

        *cp_p = ((0xED & UTF_START_MASK(3)) << (2 * UTF_ACCUMULATION_SHIFT))
            | ((s0[1] & UTF_CONTINUATION_MASK) << UTF_ACCUMULATION_SHIFT)
            |  (s0[2] & UTF_CONTINUATION_MASK);
        return true;
    }

    /* In conjunction with the exhaustive tests that can be enabled in
     * APItest/t/utf8_warn_base.pl, this can make sure the dfa does precisely
     * what it is intended to do, and that no flaws in it are masked by
     * dropping down and executing the code below
    assert(! isUTF8_CHAR(s0, e)
          || UTF8_IS_SURROGATE(s0, e)
          || UTF8_IS_SUPER(s0, e)
          || UTF8_IS_NONCHAR(s0, e));
    */

    /* Accumulate the code point translation of the input byte sequence
     * s0 .. e-1, looking for malformations.
     *
     * The order of malformation tests here is important.  We should consume as
     * few bytes as possible in order to not skip any valid character.  This is
     * required by the Unicode Standard (section 3.9 of Unicode 6.0); see also
     * https://unicode.org/reports/tr36 for more discussion as to why.  For
     * example, once we've done a UTF8SKIP, we can tell the expected number of
     * bytes, and could fail right off the bat if the input parameters indicate
     * that there are too few available.  But it could be that just that first
     * byte is garbled, and the intended character occupies fewer bytes.  If we
     * blindly assumed that the first byte is correct, and skipped based on
     * that number, we could skip over a valid input character.  So instead, we
     * always examine the sequence byte-by-byte.
     *
     * We also should not consume too few bytes, otherwise someone could inject
     * things.  For example, an input could be deliberately designed to
     * overflow, and if this code bailed out immediately upon discovering that,
     * returning to the caller C<*advance_p> pointing to the very next byte (one
     * which is actually part of the overflowing sequence), that could look
     * legitimate to the caller, which could discard the initial partial
     * sequence and process the rest, inappropriately.
     *
     * Some possible input sequences are malformed in more than one way.  This
     * function goes to lengths to try to find all of them.  This is necessary
     * for correctness, as the inputs may allow one malformation but not
     * another, and if we abandon searching for others after finding the
     * allowed one, we could allow in something that shouldn't have been.
     */

    Size_t expectlen = 0;   /* How long should this sequence be? */
    Size_t curlen = 0;      /* How many bytes have we processed so far */
    UV uv = 0;              /* The accumulated code point, so far */
    const U8 * s = s0;      /* Our current position examining the sequence */
    int overlong_detect_length = 0;

    /* Gives how many bytes are available, which may turn out to be less than
     * (but never more than) the expected length,  */
    Size_t avail_len;

    /* The ending position, plus 1, of the first character in the sequence
     * beginning at s0.  In other words, 'e', adjusted down to to be no more
     * than a single character */
    const U8 * send = e;

    /* A bit is set here for each potential problem found as we go along */
    U32 possible_problems = 0;

    /* The above variables have to be initialized before the 'goto' */

    if (UNLIKELY(s0 >= send)) {
        possible_problems |= UTF8_GOT_EMPTY;
        avail_len = 0;
        goto ready_to_handle_errors;
    }
    avail_len = send - s0;

    /* We now know we can examine the first byte of the input.  A continuation
     * byte can't start a valid sequence */
    if (UNLIKELY(UTF8_IS_CONTINUATION(*s0))) {
        possible_problems |= UTF8_GOT_CONTINUATION;
        curlen = 1;
        goto ready_to_handle_errors;
    }

    /* This is a helper function; invariants should have been handled before
     * calling it */
    assert(! NATIVE_BYTE_IS_INVARIANT(*s0));

    /* Here is not a continuation byte, nor an invariant.  The only thing left
     * is a start byte (possibly for an overlong). */
    expectlen = UTF8SKIP(s0); /* How long should this sequence be? */

    /* Convert to I8 on EBCDIC (no-op on ASCII), then remove the leading bits
     * that indicate the number of bytes in the character's whole UTF-8
     * sequence, leaving just the bits that are part of the value.  */
    uv = NATIVE_UTF8_TO_I8(*s0) & UTF_START_MASK(expectlen);

    /* Setup the loop end point, making sure to not look past the end of the
     * input string, and flag it as too short if the size isn't big enough. */
    if (UNLIKELY(avail_len < expectlen)) {
        possible_problems |= UTF8_GOT_SHORT;
    }
    else {
        send = (U8*) s0 + expectlen;
        avail_len = expectlen;
    }

    /* Now, loop through the remaining bytes in the character's sequence,
     * accumulating each into the working value as we go. */
    for (s = s0 + 1; s < send; s++) {
        if (LIKELY(UTF8_IS_CONTINUATION(*s))) {
            uv = UTF8_ACCUMULATE(uv, *s);
            continue;
        }

        /* Here, found a non-continuation before processing all expected bytes.
         * This byte indicates the beginning of a new character, so quit, even
         * if allowing this malformation. */
        possible_problems |= UTF8_GOT_NON_CONTINUATION;
        break;
    } /* End of loop through the character's bytes */

    /* Save how many bytes were actually in the character */
    curlen = s - s0;

    /* Note that there are two types of too-short malformation.  One is when
     * there is actual wrong data before the normal termination of the
     * sequence.  The other is that the sequence wasn't complete before the end
     * of the data we are allowed to look at, based on the input 'curlen'.
     * This means that we were passed data for a partial character, but it is
     * valid as far as we saw.  The other is definitely invalid.  This
     * distinction could be important to a caller, so the two types are kept
     * separate.
     *
     * A convenience macro that matches either of the too-short conditions.  */
#define UTF8_GOT_TOO_SHORT (UTF8_GOT_SHORT|UTF8_GOT_NON_CONTINUATION)

    /* Check for overflow.  The algorithm requires us to not look past the end
     * of the current character, even if partial, so the upper limit is 's' */
    if (UNLIKELY(does_utf8_overflow(s0, s) >= ALMOST_CERTAINLY_OVERFLOWS)) {
        possible_problems |= UTF8_GOT_OVERFLOW;
        uv = UV_MAX;
    }

/* Is the first byte of 's' a start byte in the UTF-8 encoding system, not
 * excluding starting an overlong sequence? */
#define UTF8_IS_SYNTACTIC_START_BYTE(s)  (NATIVE_TO_I8(*s) >= 0xC0)

    /* Check for overlong. */
    if (UTF8_IS_SYNTACTIC_START_BYTE(s0)) {
        overlong_detect_length = is_utf8_overlong(s0, s - s0);
        if (UNLIKELY(overlong_detect_length > 0)) {

            /* Two flags control the same malformation.  The more restrictive
             * and less likely one causes the other one to be set as well, so
             * as to simplify the code below. */
            if (UNLIKELY(flags & UTF8_ALLOW_LONG_AND_ITS_VALUE)) {
                possible_problems |= UTF8_GOT_LONG_WITH_VALUE;
                flags |= UTF8_ALLOW_LONG;
            }
            else {
                possible_problems |= UTF8_GOT_LONG;
            }
        }
    }

    /* Here, we have found all the possible problems, except for when the input
     * is for a problematic code point either rejected or warned about by the
     * input parameters.  Do a quick check, and if the input could be one of
     * those code points and any of those pararameter flags are set, we have to
     * investigate further. */
    if (   UNLIKELY(isUTF8_POSSIBLY_PROBLEMATIC(*s0))
        && (flags & ( UTF8_DISALLOW_ILLEGAL_INTERCHANGE
                     |UTF8_WARN_ILLEGAL_INTERCHANGE)))
    {
        /* Here, we care about problematic code points, and the input could be
         * one of them.  By examining just the first byte, we can see if this
         * is using non-standard UTF-8.  Even if it is an overlong that reduces
         * to a small code point, it is still using this Perl invention, so
         * mark it as such */
        bool must_be_super = false;
        if (UNLIKELY(UTF8_IS_PERL_EXTENDED(s0))) {
            if (flags & (UTF8_DISALLOW_PERL_EXTENDED|UTF8_WARN_PERL_EXTENDED))
            {
                possible_problems |= UTF8_GOT_PERL_EXTENDED;
            }

            /* If the sequence overflows or isn't overlong, it must represent
             * an above-Unicode code point.  Set it as well.  (In the case of
             * not having enough information to determine if it is overlong, we
             * must assume that it isn't.) */
            if (   (possible_problems & UTF8_GOT_OVERFLOW)
                || overlong_detect_length <= 0)
            {
                must_be_super = true;
                if (flags & (UTF8_DISALLOW_SUPER|UTF8_WARN_SUPER)) {
                    possible_problems |= UTF8_GOT_SUPER;
                }
            }
        }

        /* Perl extended UTF-8 can be used to represent any smaller code point
         * if overlongs are allowed.  'must_be_super' is 'true' here if we
         * found extended UTF-8 without overlongs.  If so, we know this can't
         * be any other type of problematic code point. so no further
         * processing is necessary. */
        if (! must_be_super) {

            /* Otherwise, we need to check if it actually is problematic.
             * Either we know the code point exactly, or above we found this
             * sequence includes a too-short malformation.  In the latter case,
             * we may be able to determine if the input had to be the initial
             * portion of one of the problematic code points.  This doesn't
             * work for noncharacter code points (which can't be detected from
             * a partial sequence), but if we're looking for something instead
             * of or in addition to non-characters, try determining if the
             * filled out sequence would have to be for one of them. */
            if (   UNLIKELY(possible_problems & UTF8_GOT_TOO_SHORT)
                && LIKELY(flags & ~(UTF8_DISALLOW_NONCHAR|UTF8_WARN_NONCHAR)))
            {
                /* Here, the input sequence was incomplete.  The range of
                 * possible code points this beginning portion could represent
                 * is limited; the more bytes we have available, the tighter
                 * the possible range.  That range can be determined by
                 * hypothetically filling out the sequence with the lowest
                 * legal continuation bytes to get the lowest possible code
                 * point, and by using the highest continuation bytes to get
                 * the highest code point.  That's effectively what we do here.
                 * It turns out that there is no need to find the high end of
                 * the range, as using the highest possible continuation bytes
                 * in all cases yields the upper limit of each type of
                 * problematic condition that has an upper limit.   See the
                 * b1a21fc8531cf47ab0 commit message for a detailed analysis.
                 *
                 * The smallest legal continuation byte is generally
                 * UTF8_MIN_CONTINUATION_BYTE.  But for a few start bytes it is
                 * larger.  In all cases that matter only the byte immediately
                 * following the start byte need be larger.  This is handled by
                 * pretending we saw that larger minimum (if necessary) and
                 * accumulating its value.  Then a loop is used filling in the
                 * rest with the normal minimum.  (The formula was based on
                 * manual inspection of UTF-8 conversion tables, just as was
                 * done in S_is_utf8_overlong) */
                Size_t modlen = curlen;
                if (modlen == 1) {
                    switch (NATIVE_UTF8_TO_I8(*s0)) {
                      case 0xf0:
                      case 0xf8:
                      case 0xfc:
                      case 0xfe:
                   /* case 0xff:    See message for b1a21fc8531cf47ab0 */
                        uv = UTF8_ACCUMULATE(uv,
                                             0x100 + 0x10
                                           + UTF_MIN_CONTINUATION_BYTE
                                           - NATIVE_UTF8_TO_I8(*s0));
                        modlen++;
                        break;
                    }
                }

                for (Size_t i = modlen; i < expectlen; i++) {
                    uv = UTF8_ACCUMULATE(uv, UTF8_MIN_CONTINUATION_BYTE);
                }
            }

            /* Here 'uv' is as valid as it can get.  Perhaps it was valid all
             * along because there were no malformations, or the only
             * malformation is an overlong (which allows it to be fully
             * computed).  Or it may have been "cured" as best it can by the
             * loop just above. */
            if (UNLIKELY(UNICODE_IS_SURROGATE(uv))) {
                if (flags & (UTF8_DISALLOW_SURROGATE|UTF8_WARN_SURROGATE)) {
                    possible_problems |= UTF8_GOT_SURROGATE;
                }
            }
            else if (UNLIKELY(UNICODE_IS_SUPER(uv))) {
                if (flags & (UTF8_DISALLOW_SUPER|UTF8_WARN_SUPER)) {
                    possible_problems |= UTF8_GOT_SUPER;
                }
            }
            else if (UNLIKELY(UNICODE_IS_NONCHAR(uv))) {
                if (flags & (UTF8_DISALLOW_NONCHAR|UTF8_WARN_NONCHAR)) {
                    possible_problems |= UTF8_GOT_NONCHAR;
                }
            }
        }  /* End of ! must_be_super */
    }      /* End of checking if is a special code point */

  ready_to_handle_errors: ;

    /* At this point:
     * s0                   points to the first byte of the character
     * expectlen            gives the number of bytes that the character is
     *                      expected to occupy, based on the value of the
     *                      presumed start byte in s0.  This will be 0 if the
     *                      sequence is empty, or 1 if s0 isn't actually a
     *                      start byte.  CAUTION: this could be beyond the end
     *                      of the buffer.
     * avail_len            gives the number of bytes in the sequence this
     *                      call can look at, one character's worth at most.
     * curlen               gives the number of bytes in the sequence that
     *                      this call actually looked at.  This is returned to
     *                      the caller as the value they should advance the
     *                      input by for the next call to this function.
     * possible_problems    is 0 if there weren't any problems; otherwise a bit
     *                      is set in it for each potential problem found.
     * uv                   contains the value of the code point the input
     *                      sequence represents, as far as we were able to
     *                      determine.  This is the correct translation of the
     *                      input bytes if and only if no malformations were
     *                      encountered.  If a too-short malformation was
     *                      encountered, the code above, if it thinks it might
     *                      make a difference, will have stored into this
     *                      variable the minimum code point the sequence could
     *                      possibly represent
     * s                    points to just after where we left off processing
     *                      the character
     * send                 points to just after where that character should
     *                      end, based on how many bytes the start byte tells
     *                      us should be in it, but no further than s0 +
     *                      avail_len
     */
    bool success = true;

    if (UNLIKELY(possible_problems)) {
        dTHX;

        /* Here, the input sequence is potentially problematic.  The code here
         * determines if that is indeed the case and how to handle it.  The
         * possible outcomes are:
         *  1)  substituting the Unicode REPLACEMENT CHARACTER as the
         *      translation for this input sequence; and/or
         *  2)  returning information about the problem to the caller in
         *      *errors and/or *msgs; and/or
         *  3)  raising appropriate warnings.
         *  4)  potentially croaking if the input is a forbidden sequence, and
         *      the flag has been set that indicates to croak on those.
         *
         * There are two main categories of potential problems.
         *
         *  a)  One type is considered by default to be problematic.  There are
         *      three subclasses:
         *      1)  Some syntactic malformation meant that no code point could
         *          be calculated for the input.  An example is that the
         *          sequence was incomplete, more bytes were called for than
         *          the input contained.  The function returns the Unicode
         *          REPLACEMENT CHARACTER as the translation of these.
         *      2)  The sequence is legal Perl extended UTF-8, but is for a
         *          code point too large to be represented on this platform.
         *          The function returns the Unicode REPLACEMENT CHARACTER as
         *          the translation of these.
         *      3)  The sequence represents a code point which can also be
         *          represented by a shorter sequence.  These have been
         *          declared illegal by Unicode fiat because they were being
         *          used as Trojan horses to successfully attack applications.
         *          One undocumented flag causes these to be accepted, but
         *          otherwise the function returns the Unicode REPLACEMENT
         *          CHARACTER as the translation of these.
         *
         *      These all have the same results unless flags are passed to
         *      change the behavior.  Without flags the behavior is:
         *
         *      1)  The function returns failure.
         *      2)  *cp_p is set to the REPLACEMENT_CHARACTER
         *      3)  For each problem, a bit is set in *errors denoting the
         *          error, if errors is not NULL.
         *      4)  For each problem, an entry is generated in *msgs, if msgs
         *          is not NULL.
         *      5)  a warning is raised if msgs is NULL and the appropriate
         *          warning category(ies) are enabled.
         *
         *      Various flags change the behavior:
         *
         *          UTF8_FORCE_WARN_IF_MALFORMED is forbidden if msgs is not
         *              NULL, and is ignored if UTF8_CHECK_ONLY is also
         *              specified; otherwise it turns on all warnings
         *              categories for the duration of the function.
         *
         *          UTF8_DIE_IF_MALFORMED is forbidden if msgs is not NULL;
         *              otherwise it acts as if UTF8_FORCE_WARN_IF_MALFORMED
         *              has also been specified, and also croaks rather than
         *              returning.
         *
         *          UTF8_CHECK_ONLY is ignored if msgs is not NULL or if
         *              UTF8_DIE_IF_MALFORMED is also set; otherwise it
         *              suppresses any warnings; behaviors 1) through 4) above
         *              are unchanged
         *
         *      Also there is a flag associated with each possible condition,
         *      for example, UTF8_ALLOW_LONG.  If set, the behavior is modified
         *      so that the corresponding condition:
         *          1)  doesn't cause the function to return failure
         *          2)  the REPLACEMENT_CHARACTER is still stored in *cp_p,
         *              except for the flag UTF8_ALLOW_LONG_AND_ITS_VALUE,
         *              which returns the calculated code point, even if plain
         *              UTF8_ALLOW_LONG is also set.
         *          3)  *errors still has a bit set.
         *          4)  no entry is generated in *msgs.
         *          5)  no warning is raised
         *
         *      Note that this means the UTF8_CHECK_ONLY flag has the same
         *      effect as passing an ALLOW flag for every condition.
         *
         *      Note also that an entry is placed in *errors for each condition
         *      found, regardless of the other flags.  The caller can rely on
         *      this being an accurate accounting of all conditions found, even
         *      if they aren't otherwise reported.
         *
         *  b)  The other type is by default not considered to be a problem.
         *      These are for when the input was syntactically valid UTF-8 (as
         *      extended by Perl) for a code point that is representable on
         *      this platform, but that code point isn't considered by Unicode
         *      to be freely exchangeable between applications.
         *
         *      The 'flags' parameter to this function must contain an
         *      appropriate set bit in order for this function to consider them
         *      to be problems.  And to get here, code earlier in this function
         *      has determined one of those flags applies to this sequence.
         *      This means that we know already that this input is problematic,
         *      unlike the type a) items.
         *
         *      Each of these problematic sequences has two independent flags
         *      associated with it.  The DISALLOW flag causes this code point
         *      to be rejected; the WARN flag causes it to attempt to raise a
         *      warning about it.  To do both, specify both flags.  This is
         *      different from the type a) items, where the ALLOW flag affects
         *      both the rejection and warning.  The same 5 actions as type a)
         *      have to be done, but the conditions differ.  The actions when
         *      the UTF8_CHECK_ONLY flag is not included are:
         *
         *      1)  If the DISALLOW flag is set, the function returns failure,
         *          or croaks if the UTF8_DIE_IF_MALFORMED flag is included.
         *      2)  If the DISALLOW flag is set, the REPLACEMENT_CHARACTER is
         *          substituted for the returned code point
         *      3)  A bit is set in *errors if errors is not NULL
         *      4)  An entry in *msgs is generated if msgs is not NULL.  Since
         *          to get here, we know the input is problematic, an entry is
         *          unconditionally made.  The warnings category for it will be
         *          zero if neither the corresponding WARN flag nor the
         *          UTF8_FORCE_WARN_IF_MALFORMED flag are included.
         *      5)  A warning is raised if msgs is NULL and either:
         *            i)  the flag UTF8_FORCE_WARN_IF_MALFORMED is included; or
         *           ii)  the corresponding WARN flag is included, and the
         *                appropriate warning category(ies) are enabled.
         *
         *      Including the UTF8_CHECK_ONLY flag has no effect if the
         *      UTF8_DIE_IF_MALFORMED is also included; otherwise it changes
         *      the above actions only to not do 5); so no warnings get
         *      generated.
         */

        bool disallowed = FALSE;
        const U32 orig_problems = possible_problems;
        const UV input_uv = uv;
        U32 error_flags_return = 0;
        AV * msgs_return = NULL;
        Size_t super_msgs_count = 0;

        /* The conditions that are rejected by default are the ones for which
         * you need a flag to accept.  There is a good reason for them being
         * generally rejected.  All but LONG can't be evaluated to a specific
         * code point.  And LONG is forbidden to do so because of the potential
         * for hacking attacks. */
#define DEFAULT_REJECTS                                                     \
            (UTF8_ALLOW_ANY|UTF8_ALLOW_EMPTY|UTF8_ALLOW_LONG_AND_ITS_VALUE)

        /* Determine which conditions the caller wants to reject.  Most are
         * indicated by the corresponding flag being 0.  Complement these via
         * xor, while leaving alone the conditions that require a 1 to reject.
         * This normalizes 'rejects' so that a 1 bit means to reject the
         * corresponding condition; 0 to accept. */
        U32 rejects = flags ^ DEFAULT_REJECTS;

        /* The conditions that lead to the REPLACEMENT CHARACTER being returned
         * are the ones which always lead to this, plus the ones specified by
         * the input flags.  The former are the ones that are by default
         * rejected, except UTF8_ALLOW_LONG_AND_ITS_VALUE, which explicitly
         * requests the calculated value to be returned. */
        U32 replaces = ( UTF8_ALLOW_ANY|UTF8_ALLOW_EMPTY)
                        |(flags & UTF8_DISALLOW_ILLEGAL_INTERCHANGE);

        /* The following macro returns:
         *    0   when there is no reason to generate a message for this
         *        condition, because the appropriate warnings categories are
         *        off and not overridden
         *  < 0   when the only reason would be to return a message in an AV
         *        structure.  This happens when the macro would otherwise
         *        return 0, but detects there is an AV structure to fill in.
         *  > 0   when there are warning categories effectively enabled.  If
         *        so, the value is the result of calling the appropriate
         *        packWARN macro on those categories.
         *
         * The first parameter 'warning' is a warnings category that applies to
         * the condition.  The following tests are checked in this priority
         * order; the first that matches is taken:
         *
         * 1)   'warning' is considered enabled if the UTF8_DIE_IF_MALFORMED
         *      flag is set.
         * 2)   'warning' is considered disabled if the UTF8_CHECK_ONLY flag is
         *      set.
         * 3)   'warning' is considered enabled if the
         *      UTF8_FORCE_WARN_IF_MALFORMED flag is set
         * 4)   'warning is considered enabled if ckWARN_d(warning) is true
         * 5)   A secondary warning category is optionally passed, along with
         *      either to use ckWARN or ckWARN_d on it.  This is considered
         *      enabled if that returns true.
         * 6)   -1 is returned if 'msgs' isn't NULL, which means the caller
         *      wants any message stored into it
         * 7)   0 is returned.
         *
         * When called without a second category, the macro outputs a bunch of
         * zeroes that the compiler should fold to nothing */
#define PACK_WARN(warning, extra_ckWARN, extra_category)                    \
           (UNLIKELY(flags & UTF8_DIE_IF_MALFORMED)    ? packWARN(warning)  \
          : (flags & UTF8_CHECK_ONLY)                  ? 0                  \
          : UNLIKELY(flags & UTF8_FORCE_WARN_IF_MALFORMED) ? packWARN(warning)\
          :  ckWARN_d(warning)                         ? packWARN(warning)  \
          :  extra_ckWARN(extra_category +0)           ? packWARN2(warning, \
                                                         extra_category +0) \
          :  (msgs)                                    ? -1                 \
          :  0)

        while (possible_problems) { /* Handle each possible problem */
            IV pack_warn = 0;
            char * message = NULL;

            /* The lowest bit positions, as #defined in utf8.h, are handled
             * first.  Some of the ordering is important so that higher
             * priority items are done before lower ones; some of which may
             * depend on earlier actions.  Also the ordering tries to cause any
             * messages to be displayed in kind of decreasing severity order.
             * */
            U32 this_problem = 1U << lsbit_pos32(possible_problems);

            U32 this_flag_bit = this_problem;

            /* All cases set this */
            error_flags_return |= this_problem;

            /* Turn off so next iteration doesn't retry this */
            possible_problems &= ~this_problem;

            if (this_problem & replaces) {
                uv = UNICODE_REPLACEMENT;
            }
            if (this_problem & rejects) {
                disallowed = true;
            }

            /* The code is structured so that there is a case: in a switch()
             * for each condition type, so as to handle the different details of
             * each.  The only common part after setting things up is the
             * handling of any generated warning message.  That means that if a
             * case: finds there is no message, it can 'continue' to the next
             * loop iteration instead of doing a 'break', whose only purpose
             * would be to handle the message.
             */

            switch (this_problem) {
              default:
                croak("panic: Unexpected case value in "
                                 " utf8n_to_uvchr_msgs() %" U32uf,
                           this_problem);
                /* NOTREACHED */
                break;

/* If this condition is allowed, no message is to be generated.  Similarly, if
 * warnings for it aren't enabled.  All of these are controlled only by 'utf8'
 * warnings.  This macro relies on the GOT and ACCEPT flags being identical. */
#define COMMON_DEFAULT_REJECTS(p1, p2)                                      \
                if (   (! (this_problem & rejects))                         \
                    || ((pack_warn = PACK_WARN(WARN_UTF8,p1,p2)) == 0))     \
                {                                                           \
                    continue;                                               \
                }                                                           \

              case UTF8_GOT_EMPTY:
                COMMON_DEFAULT_REJECTS(,);

                /* This so-called malformation is now treated as a bug in the
                 * caller.  If you have nothing to decode, skip calling this
                 * function */

                assert(0);
                message = Perl_form(aTHX_ "%s (empty string)", malformed_text);
                break;

              case UTF8_GOT_CONTINUATION:
                COMMON_DEFAULT_REJECTS(,);
                message = form(
                                "%s: %s (unexpected continuation byte 0x%02x,"
                                " with no preceding start byte)",
                                malformed_text,
                                _byte_dump_string(s0, 1, 0),
                                *s0);
                break;

              case UTF8_GOT_SHORT:
                COMMON_DEFAULT_REJECTS(,);
                message = form(
                             "%s: %s (too short; %d byte%s available, need %d)",
                             malformed_text,
                             _byte_dump_string(s0, avail_len, 0),
                             (int)avail_len,
                             avail_len == 1 ? "" : "s", /* Pluralize */
                             (int)expectlen);
                break;

              case UTF8_GOT_NON_CONTINUATION:
               {
                COMMON_DEFAULT_REJECTS(,);

                /* If we don't know for sure that the input length is valid,
                 * avoid as much as possible reading past the end of the buffer
                 * */
                int printlen = (flags & UTF8_NO_CONFIDENCE_IN_CURLEN_)
                                ? (int) (s - s0)
                                : (int) (avail_len);
                message = form("%s",
                                    unexpected_non_continuation_text(s0,
                                                            printlen,
                                                            s - s0,
                                                            (int) expectlen));
                break;
               }

              case UTF8_GOT_LONG:
              case UTF8_GOT_LONG_WITH_VALUE:
                COMMON_DEFAULT_REJECTS(,);

                /* These error types cause 'input_uv' to be something that
                 * isn't what was intended, so can't use it in the message.
                 * The other error types either can't generate an overlong, or
                 * else the 'input_uv' is valid */
                if (orig_problems & (UTF8_GOT_TOO_SHORT|UTF8_GOT_OVERFLOW)) {
                    message = Perl_form(aTHX_
                            "%s: %s (any UTF-8 sequence that starts with"
                            " \"%s\" is overlong which can and should be"
                            " represented with a different, shorter sequence)",
                            malformed_text,
                            _byte_dump_string(s0, send - s0, 0),
                            _byte_dump_string(s0, curlen, 0));
                }
                else {
                    U8 tmpbuf[UTF8_MAXBYTES+1];
                    const U8 * const e = uvoffuni_to_utf8_flags(tmpbuf,
                                                                input_uv, 0);

                    /* Don't use U+ for non-Unicode code points, which includes
                     * those in the Latin1 range */
                    const char * preface = (  UNICODE_IS_SUPER(input_uv)
#ifdef EBCDIC
                                            || input_uv <= 0xFF
#endif
                                            )
                                            ? "0x"
                                            : "U+";
                    message = Perl_form(aTHX_
                                "%s: %s (overlong; instead use %s to represent"
                                " %s%0*" UVXf ")",
                                malformed_text,
                                _byte_dump_string(s0, avail_len, 0),
                                _byte_dump_string(tmpbuf, e - tmpbuf, 0),
                                preface,
                                ((input_uv < 256) ? 2 : 4), /* Field width of 2
                                                               for small code
                                                               points */
                                UNI_TO_NATIVE(input_uv));
                }
                break;

/* PACK_WARN returns:
 *    0   when there is no reason to generate a message for this condition
 *        because the appropriate warnings categories are off and not
 *        overridden
 *  < 0   if the only reason would be to return a message in an AV structure;
 *        but this is only done if this condition is to be rejected
 *  > 0   if the categories are effectively on; but this is only done for these
 *        default-accepted conditions if at least one of the following is true:
 *          1) the caller has expicitly set the individual flag to demand
 *             warnings for this condition; or
 *          2) the caller has passed flags that demand all conditions generate
 *             warnings; or
 *          3) the condition is to be rejected and is to be passed back to the
 *             caller in an AV structure
 * This macro relies on each GOT and ACCEPT flags being identical.
 */
#define COMMON_DEFAULT_ACCEPTEDS(warn_flag, p1, p2, p3)                     \
                pack_warn = PACK_WARN(p1, p2, p3);                          \
                if (    pack_warn == 0                                      \
                    || (pack_warn < 0 && ! (this_problem & rejects))        \
                    || (   pack_warn > 0                                    \
                        && (0 == (flags & ( warn_flag                       \
                                           |UTF8_DIE_IF_MALFORMED           \
                                           |UTF8_FORCE_WARN_IF_MALFORMED))) \
                        && (! msgs || ! (this_problem & rejects))))         \
                {                                                           \
                    continue;                                               \
                }

              case UTF8_GOT_SURROGATE:
                COMMON_DEFAULT_ACCEPTEDS(UTF8_WARN_SURROGATE,
                                         WARN_SURROGATE,,);

                /* This is the only error that can occur with a surrogate when
                 * the 'input_uv' isn't valid */
                if (orig_problems & UTF8_GOT_TOO_SHORT) {
                    message = Perl_form(aTHX_
                                   "UTF-16 surrogate (any UTF-8 sequence that"
                                   " starts with \"%s\" is for a surrogate)",
                                   _byte_dump_string(s0, curlen, 0));
                }
                else {
                    message = Perl_form(aTHX_ surrogate_cp_format, input_uv);
                }

                break;

              case UTF8_GOT_NONCHAR:
                COMMON_DEFAULT_ACCEPTEDS(UTF8_WARN_NONCHAR, WARN_NONCHAR,,);

                /* The code above should have guaranteed that we don't get here
                 * with conditions other than these */
                assert (! (orig_problems & ~( UTF8_GOT_LONG
                                             |UTF8_GOT_LONG_WITH_VALUE
                                             |UTF8_GOT_PERL_EXTENDED
                                             |UTF8_GOT_NONCHAR)));
                message = form(nonchar_cp_format, input_uv);
                break;

                /* The final three cases are all closely related.  They are
                 * ordered in execution by severity of the corresponding
                 * condition */
                STATIC_ASSERT_STMT(  UTF8_GOT_OVERFLOW
                                   < UTF8_GOT_PERL_EXTENDED);
                STATIC_ASSERT_STMT(UTF8_GOT_PERL_EXTENDED < UTF8_GOT_SUPER);

                /* And each is a subset of the next.  The code does a bit of
                 * setup for each and then jumps to common handling.  This
                 * structure comes from the desire to use the most dire warning
                 * suitable for the condition even if the only warning class
                 * that is enabled is a less severe one.  It just makes sense
                 * that if someone wants to be warned about all above-Unicode
                 * code points, and this one is so far above that it won't fit
                 * in the platform's word size, that the overflow warning would
                 * be output instead of the more mild one. */

              bool overflows;
              bool is_extended;

              case UTF8_GOT_OVERFLOW:
                COMMON_DEFAULT_REJECTS(ckWARN_d, WARN_NON_UNICODE);
                overflows = true;
                is_extended = true;
                goto super_common;

              case UTF8_GOT_PERL_EXTENDED:
                COMMON_DEFAULT_ACCEPTEDS(UTF8_WARN_PERL_EXTENDED,
                                         WARN_NON_UNICODE, ckWARN_d,
                                         WARN_PORTABLE);
                overflows = orig_problems & UTF8_GOT_OVERFLOW;
                is_extended = true;
                goto super_common;

              case UTF8_GOT_SUPER:
                COMMON_DEFAULT_ACCEPTEDS(UTF8_WARN_SUPER, WARN_NON_UNICODE,,);
                overflows = orig_problems & UTF8_GOT_OVERFLOW;
                is_extended = UTF8_IS_PERL_EXTENDED(s0);

              super_common:
               {
                /* To get here the COMMON macros above determined that a
                 * warning message needs to be generated for this case.
                 * (Otherwise they would have executed a 'continue' statement
                 * to try the next case.).  But they don't always catch if a
                 * message has already been generated for the underlying
                 * condition.  Skip if so. */
                if (super_msgs_count++) {
                    continue;
                }

                /* Now generate the message text.  We can't include the code
                 * point in it if there isn't a specific one, either because
                 * this overflowed, or there weren't enough bytes to form a
                 * complete character.
                 *
                 * We also can't include it if the resultant message would be
                 * misleading.  This can happen when a sequence is an overlong,
                 * using Perl extended UTF-8.  That could evaluate to a
                 * character in the Unicode range, say the letter "A"; we don't
                 * want a message saying that "A" isn't Unicode, because this
                 * would be a lie.  "A" definitely is Unicode.  It was just
                 * expressed in a non-standard form of UTF-8 that we warn
                 * about.  If the sequence uses extended UTF-8 but the
                 * resulting code point isn't for above Unicode, we know we
                 * have this situation. */

                if (overflows) {
                    message = Perl_form(aTHX_ "%s: %s (overflows)",
                                              malformed_text,
                                              _byte_dump_string(s0, curlen, 0));
                }
                else if (   (orig_problems & UTF8_GOT_TOO_SHORT)
                         || (     UTF8_IS_PERL_EXTENDED(s0)
                             && ! UNICODE_IS_SUPER(input_uv)))
                {
                    if (is_extended) {
                        message = Perl_form(aTHX_
                                        "Any UTF-8 sequence that starts with"
                                        " \"%s\" is a Perl extension, and so"
                                        " is not portable",
                                        _byte_dump_string(s0, curlen, 0));
                    }
                    else {
                        message = Perl_form(aTHX_
                                        "Any UTF-8 sequence that starts with"
                                        " \"%s\" is for a non-Unicode code"
                                        " point, may not be portable",
                                        _byte_dump_string(s0, curlen, 0));
                    }
                }
                else if (is_extended) {
                    message = Perl_form(aTHX_ PL_extended_cp_format, input_uv);
                }
                else {
                    message = Perl_form(aTHX_ super_cp_format, input_uv);
                }

                /* This message only needs to output once.  Ww can potentially
                 * save some loop iterations by turning off looking for
                 * warnings for it. */
                flags &= ~(UTF8_WARN_PERL_EXTENDED|UTF8_WARN_SUPER);

                break;
               }
            } /* End of switch() on the possible problems */

            /* We only get here if there is a message to be displayed or
             * returned; each case statement in the switch above does a
             * continue if no message for it need be generated. */
            if (msgs) {

                /* It's illegal to call this with these flags, but we only fail
                 * in the unlikely event that it matters.  Outside of DEBUGGING
                 * builds, those flags contradictory to this operation get
                 * ignored */
                assert(! (flags & ( UTF8_DIE_IF_MALFORMED
                                   |UTF8_FORCE_WARN_IF_MALFORMED)));

                if (msgs_return == NULL) {
                    msgs_return = newAV();
                }

                av_push(msgs_return,
                        /* Negative 'pack_warn' really means 0 here.  But this
                         * converts that to UTF-8 to preserve broken behavior
                         * depended upon by Encode. */
                        newRV_noinc((SV*) new_msg_hv(message,
                                                     ((pack_warn <= 0)
                                                      ? packWARN(WARN_UTF8)
                                                      : pack_warn),
                                                     this_flag_bit)));
            }
            else {
                if (UNLIKELY(flags & ( UTF8_DIE_IF_MALFORMED
                                      |UTF8_FORCE_WARN_IF_MALFORMED)))
                {
                    ENTER;
                    SAVEI8(PL_dowarn);
                    SAVESPTR(PL_curcop);

                    PL_dowarn = G_WARN_ALL_ON|G_WARN_ON;
                    if (PL_curcop) {
                        SAVECURCOPWARNINGS();
                        PL_curcop->cop_warnings = pWARN_ALL;
                    }
                }

                if (PL_op) {
                    warner(pack_warn, "%s in %s", message, OP_DESC(PL_op));
                }
                else {
                    warner(pack_warn, "%s", message);
                }

                if (UNLIKELY(flags & ( UTF8_DIE_IF_MALFORMED
                                      |UTF8_FORCE_WARN_IF_MALFORMED)))
                {
                    LEAVE;
                }
            }
        }   /* End of 'while (possible_problems)' */

        if (msgs_return) {
            *msgs = msgs_return;
        }

        if (errors) {
            *errors = error_flags_return;
        }

        if (disallowed) {
            if ((flags & ~UTF8_CHECK_ONLY) & UTF8_DIE_IF_MALFORMED) {
                croak("Malformed UTF-8 character (fatal)");
            }

            success = false;
        }
    } /* End of there was a possible problem */

    if (advance_p) {
        *advance_p = curlen;
    }

    *cp_p = UNI_TO_NATIVE(uv);
    return success;
}

/*
=for apidoc utf8_length

Returns the number of characters in the sequence of UTF-8-encoded bytes starting
at C<s> and ending at the byte just before C<e>.  If <s> and <e> point to the
same place, it returns 0 with no warning raised.

If C<e E<lt> s> or if the scan would end up past C<e>, it raises a UTF8 warning
and returns the number of valid characters.

=cut

    For long strings we process the input word-at-a-time, and count
    continuations, instead of otherwise counting characters and using UTF8SKIP
    to find the next one.  If our input were 13-byte characters, the per-word
    would be a loser, as we would be doing things in 8 byte chunks (or 4 on a
    32-bit platform).  But the maximum legal Unicode code point is 4 bytes, and
    most text will have a significant number of 1 and 2 byte characters, so the
    per-word is generally a winner.

    There are start-up and finish costs with the per-word method, so we use the
    standard method unless the input has a relatively large length.
*/

STRLEN
Perl_utf8_length(pTHX_ const U8 * const s0, const U8 * const e)
{
    STRLEN continuations = 0;
    STRLEN len = 0;
    const U8 * s = s0;

    PERL_ARGS_ASSERT_UTF8_LENGTH;

    /* For EBCDIC and short strings, we count the characters.  The boundary
     * was determined by eyeballing the output of Porting/bench.pl and
     * choosing a number where the continuations method gave better results (on
     * a 64 bit system, khw not having access to a 32 bit system with
     * cachegrind).  The number isn't critical, as at these sizes, the total
     * time spent isn't large either way */

#ifndef EBCDIC

    if (e - s0 < 96)

#endif

    {
        while (s < e) { /* Count characters directly */

            /* Take extra care to not exceed 'e' (which would be undefined
             * behavior) should the input be malformed, with a partial
             * character at the end */
            ptrdiff_t expected_byte_count = UTF8SKIP(s);
            if (UNLIKELY(e - s  < expected_byte_count)) {
                goto warn_and_return;
            }

            len++;
            s += expected_byte_count;
        }

        if (LIKELY(e == s)) {
            return len;
        }

      warn_and_return:
        if (PL_op)
            ck_warner_d(packWARN(WARN_UTF8),
                        "%s in %s", unees, OP_DESC(PL_op));
        else
            ck_warner_d(packWARN(WARN_UTF8), "%s", unees);

        return s - s0;
    }

#ifndef EBCDIC

    /* Count continuations, word-at-a-time.
     *
     * We need to stop before the final start character in order to
     * preserve the limited error checking that's always been done */
    const U8 * e_limit = e - UTF8_MAXBYTES;

    /* Points to the first byte >=s which is positioned at a word boundary.  If
     * s is on a word boundary, it is s, otherwise it is to the next word. */
    const U8 * partial_word_end = s + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(s)
                                    - (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK);

    /* Process up to a full word boundary. */
    while (s < partial_word_end) {
        const Size_t skip = UTF8SKIP(s);

        continuations += skip - 1;
        s += skip;
    }

    /* Adjust back down any overshoot */
    continuations -= s - partial_word_end;
    s = partial_word_end;

    do { /* Process per-word */

        /* The idea for counting continuation bytes came from
         * https://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
         * One thing it does that this doesn't is to prefetch the buffer
         *      __builtin_prefetch(&s[256], 0, 0);
         *
         * A continuation byte has the upper 2 bits be '10', and the rest
         * dont-cares.  The VARIANTS mask zeroes out all but the upper bit of
         * each byte in the word.  That gets shifted to the byte's lowest bit,
         * and 'anded' with the complement of the 2nd highest bit of the byte,
         * which has also been shifted to that position.  Hence the bit in that
         * position will be 1 iff the upper bit is 1 and the next one is 0.  We
         * then use the same integer multiplcation and shifting that are used
         * in variant_under_utf8_count() to count how many of those are set in
         * the word. */

        continuations += (((((* (const PERL_UINTMAX_T *) s)
                                            & PERL_VARIANTS_WORD_MASK) >> 7)
                      & (((~ (* (const PERL_UINTMAX_T *) s))) >> 6))
                  * PERL_COUNT_MULTIPLIER)
                >> ((PERL_WORDSIZE - 1) * CHARBITS);
        s += PERL_WORDSIZE;
    } while (s + PERL_WORDSIZE <= e_limit);

    /* Process remainder per-byte */
    while (s < e) {
	if (UTF8_IS_CONTINUATION(*s)) {
            continuations++;
            s++;
            continue;
        }

        /* Here is a starter byte.  Use UTF8SKIP from now on */
        do {
            ptrdiff_t expected_byte_count = UTF8SKIP(s);
            if (UNLIKELY(e - s  < expected_byte_count)) {
                break;
            }

            continuations += expected_byte_count- 1;
            s += expected_byte_count;
        } while (s < e);

        break;
    }

#  endif

    if (LIKELY(e == s)) {
        return s - s0 - continuations;
    }

    /* Convert to characters */
    s -= continuations;

    goto warn_and_return;
}

/*
=for apidoc bytes_cmp_utf8

Compares the sequence of characters (stored as octets) in C<b>, C<blen> with the
sequence of characters (stored as UTF-8)
in C<u>, C<ulen>.  Returns 0 if they are
equal, -1 or -2 if the first string is less than the second string, +1 or +2
if the first string is greater than the second string.

-1 or +1 is returned if the shorter string was identical to the start of the
longer string.  -2 or +2 is returned if
there was a difference between characters
within the strings.

=cut
*/

int
Perl_bytes_cmp_utf8(pTHX_ const U8 *b, STRLEN blen, const U8 *u, STRLEN ulen)
{
    const U8 *const bend = b + blen;
    const U8 *const uend = u + ulen;

    PERL_ARGS_ASSERT_BYTES_CMP_UTF8;

    while (b < bend && u < uend) {
        U8 c = *u++;
        if (!UTF8_IS_INVARIANT(c)) {
            if (UTF8_IS_DOWNGRADEABLE_START(c)) {
                if (u < uend) {
                    U8 c1 = *u++;
                    if (UTF8_IS_CONTINUATION(c1)) {
                        c = EIGHT_BIT_UTF8_TO_NATIVE(c, c1);
                    } else {
                        /* diag_listed_as: Malformed UTF-8 character%s */
                        ck_warner_d(packWARN(WARN_UTF8),
                                    "%s %s%s",
                                    unexpected_non_continuation_text(u - 2, 2, 1, 2),
                                    PL_op ? " in " : "",
                                    PL_op ? OP_DESC(PL_op) : "");
                        return -2;
                    }
                } else {
                    if (PL_op)
                        ck_warner_d(packWARN(WARN_UTF8),
                                    "%s in %s", unees, OP_DESC(PL_op));
                    else
                        ck_warner_d(packWARN(WARN_UTF8), "%s", unees);
                    return -2; /* Really want to return undef :-)  */
                }
            } else {
                return -2;
            }
        }
        if (*b != c) {
            return *b < c ? -2 : +2;
        }
        ++b;
    }

    if (b == bend && u == uend)
        return 0;

    return b < bend ? +1 : -1;
}

/*
=for apidoc      utf8_to_bytes_overwrite
=for apidoc_item utf8_to_bytes_new_pv
=for apidoc_item utf8_to_bytes_temp_pv
=for apidoc_item utf8_to_bytes
=for apidoc_item bytes_from_utf8

These each convert a string encoded as UTF-8 into the equivalent native byte
representation, if possible.  The first three forms are preferred; their API is
more convenient to use, and each return C<true> if the result is in bytes;
C<false> if the conversion failed.

=over 4

=item * C<utf8_to_bytes_overwrite>

=item * C<utf8_to_bytes_new_pv>

=item * C<utf8_to_bytes_temp_pv>

These differ primarily in the form of the returned string and the allowed
constness of the input string.  In each, if the input string was already in
native bytes or was not convertible, the input isn't changed.

In each of these three functions, the input C<s_ptr> is a pointer to the string
to be converted and C<*lenp> is its length (so that the first byte will be at
C<*sptr[0]>).

C<utf8_to_bytes_overwrite> overwrites the input string with the bytes
conversion.  Hence, the input string should not be C<const>.  (Converting the
multi-byte UTF-8 encoding to single bytes never expands the result, so
overwriting is always feasible.)

Both C<utf8_to_bytes_new_pv> and C<utf8_to_bytes_temp_pv> allocate new memory
to hold the converted string, never changing the input.  Hence the input string
may be C<const>.  They differ in that C<utf8_to_bytes_temp_pv> arranges for the
new memory to automatically be freed.  With C<utf8_to_bytes_new_pv>, the caller
is responsible for freeing the memory.  As explained below, not all successful
calls result in new memory being allocated.  Hence this function also returns
to the caller (via an extra parameter, C<*free_me>) a pointer to any new
memory, or C<NULL> if none was allocated.

The functions return C<false> when the input is not well-formed UTF-8 or contains
at least one UTF-8 sequence that represents a code point that can't be
expressed as a byte.  The contents of C<*s_ptr> and C<*lenp> are not changed.
C<utf8_to_bytes_new_pv> sets C<*free_me> to C<NULL>.

They all return C<true> when either:

=over 4

=item The input turned out to already be in bytes form

The contents of C<*s_ptr> and C<*lenp> are not changed.
C<utf8_to_bytes_new_pv> sets C<*free_me> to C<NULL>.

=item The input was successfully converted

=over 4

=item For C<utf8_to_bytes_overwrite>

The input string C<*s_ptr> was overwritten with the native bytes, including a
NUL terminator.  C<*lenp> has been updated with the new length.

=item For C<utf8_to_bytes_new_pv> and C<utf8_to_bytes_temp_pv>

The input string was not changed.  Instead, new memory has been allocated
containing the translation of the input into native bytes, with a NUL
terminator byte.  C<*s_ptr> now points to that new memory, and  C<*lenp>
contains its length.

For C<utf8_to_bytes_temp_pv>, the new memory has been arranged to be
automatically freed, via a call to C<L</SAVEFREEPV>>.

For C<utf8_to_bytes_new_pv>, C<*free_me> has been set to C<*s_ptr>, and it is
the caller's responsibility to free the new memory when done using it.
The following paradigm is convenient to use for this:

 void * free_me;
 if (utf8_to_bytes_new_pv(&s, &len, &free_me) {
    ...
 }
 else {
    ...
 }

 ...

 Safefree(free_me);

C<free_me> can be used as a boolean (non-NULL meaning C<true>) to indicate that
the input was indeed changed if you need to revisit that later in the code.
Your design is likely flawed if you find yourself using C<free_me> for any
other purpose.

=back

=back

Note that in all cases, C<*s_ptr> and C<*lenp> will have correct and consistent
values, updated as was necessary.

Also note that upon successful conversion, the number of variants in the string
can be computed by having saved the value of C<*lenp> before the call, and
subtracting the after-call value of C<*lenp> from it.  This is also true for
the other two functions described below.

=item * C<utf8_to_bytes>

Plain C<utf8_to_bytes> (which has never lost its experimental status) also
converts a UTF-8 encoded string to bytes, but there are more glitches that the
caller has to be prepared to handle.

The input string is passed with one less indirection level, C<s>.

=over

=item If the conversion was a noop

The contents of C<s> and C<*lenp> are not changed, and the function returns
C<s>.

=item If the conversion was successful

The contents of C<s> were changed, and C<*lenp> updated to be the correct length.
The function returns C<s> (unchanged).

=item If the conversion failed

The contents of C<s> were not changed.

The function returns NULL and sets C<*lenp> to -1, cast to C<STRLEN>.
This means that you will have to use a temporary containing the string length
to pass to the function if you will need the value afterwards.

=back

=item * C<bytes_from_utf8>

C<bytes_from_utf8> also converts a potentially UTF-8 encoded string C<s> to
bytes.  It preserves C<s>, allocating new memory for the converted string.

In contrast to the other functions, the input string to this one need not
be UTF-8.  If not, the caller has set C<*is_utf8p> to be C<false>, and the
function does nothing, returning the original C<s>.

Also do nothing if there are code points in the string not expressible in
native byte encoding, returning the original C<s>.

Otherwise, C<*is_utf8p> is set to 0, and the return value is a pointer to a
newly created string containing the native byte equivalent of C<s>, and whose
length is returned in C<*lenp>, updated.  The new string is C<NUL>-terminated.
The caller is responsible for arranging for the memory used by this string to
get freed.

The major problem with this function is that memory is allocated and filled
even when the input string was already in bytes form.

=back

New code should use the first three functions listed above.

=cut
*/

bool
Perl_utf8_to_bytes_(pTHX_ U8 **s_ptr, STRLEN *lenp, void ** free_me,
                          Perl_utf8_to_bytes_arg result_as)
{
    PERL_ARGS_ASSERT_UTF8_TO_BYTES_;

    if (result_as == PL_utf8_to_bytes_new_memory) {
        *free_me = NULL;
    }

    U8 * first_variant;

    /* This is a no-op if no variants at all in the input */
    if (is_utf8_invariant_string_loc(*s_ptr, *lenp,
                                    (const U8 **) &first_variant))
    {
        return true;
    }

    /* Nothing before 'first_variant' needs to be changed, so start the real
     * work there */

    U8 * const s0 = *s_ptr;
    const U8 * const send = s0 + *lenp;
    U8 * s = first_variant;
    Size_t invariant_length = first_variant - s0;
    Size_t variant_count = 0;

#ifndef EBCDIC      /* The below relies on the bit patterns of UTF-8 */

    /* Do a first pass through the string to see if it actually is translatable
     * into bytes, and if so, how big the result is.  On long strings this is
     * done a word at a time, so is relatively quick. (There is some
     * start-up/tear-down overhead with the per-word algorithm, so no real gain
     * unless the remaining portion of the string is long enough.  The current
     * value is just a guess.)  On EBCDIC, it's always per-byte. */
    if ((send - s) > (ptrdiff_t) (5 * PERL_WORDSIZE)) {

        /* If the string contains any start byte besides C2 and C3, then it
         * isn't translatable into bytes */

        const PERL_UINTMAX_T C0_mask = PERL_COUNT_MULTIPLIER * 0xC0;
        const PERL_UINTMAX_T C2_mask = PERL_COUNT_MULTIPLIER * 0xC2;
        const PERL_UINTMAX_T FE_mask = PERL_COUNT_MULTIPLIER * 0xFE;

        /* Points to the first byte >=s which is positioned at a word boundary.
         * If s is on a word boundary, it is s, otherwise it is the first byte
         * of the next word. */
        U8 * partial_word_end = s + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(s)
                                - (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK);

        /* Here there is at least a full word beyond the first word boundary.
         * Process up to that boundary. */
        while (s < partial_word_end) {
            if (! UTF8_IS_INVARIANT(*s)) {
                if (! UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(s, send)) {
                    return false;
                }

                s++;
                variant_count++;
            }

            s++;
        }

        /* Adjust back down any overshoot */
        s = partial_word_end;

        /* Process per-word */
        do {

            PERL_UINTMAX_T C2_C3_start_bytes;

            /* First find the bytes that are start bytes.  ANDing with
             * C0C0...C0 causes any start byte to become C0; any other byte
             * becomes something else.  Then XORing with C0 causes any start
             * byte to become 0; all other bytes non-zero. */
            PERL_UINTMAX_T start_bytes
                          = ((* (PERL_UINTMAX_T *) s) & C0_mask) ^ C0_mask;

            /* These shifts causes the most significant bit to be set to 1 for
             * any bytes in the word that aren't completely 0.  Hence after
             * these, only the start bytes have 0 in their msb */
            start_bytes |= start_bytes << 1;
            start_bytes |= start_bytes << 2;
            start_bytes |= start_bytes << 4;

            /* When we complement, then AND with 8080...80, the start bytes
             * will have 1 in their msb, and all other bits are 0 */
            start_bytes = ~ start_bytes & PERL_VARIANTS_WORD_MASK;

            /* Now repeat the procedure, but look for bytes that match only
             * C2-C3. */
            C2_C3_start_bytes = ((* (PERL_UINTMAX_T *) s) & FE_mask)
                                                                ^ C2_mask;
            C2_C3_start_bytes |= C2_C3_start_bytes << 1;
            C2_C3_start_bytes |= C2_C3_start_bytes << 2;
            C2_C3_start_bytes |= C2_C3_start_bytes << 4;
            C2_C3_start_bytes = ~ C2_C3_start_bytes
                                & PERL_VARIANTS_WORD_MASK;

            /* Here, start_bytes has a 1 in the msb of each byte that has a
             *                                              start_byte; And
             * C2_C3_start_bytes has a 1 in the msb of each byte that has a
             *                                       start_byte of C2 or C3
             * If they're not equal, there are start bytes that aren't C2
             * nor C3, hence this is not downgradable */
            if (start_bytes != C2_C3_start_bytes) {
                return false;
            }

            /* Commit 03c1e4ab1d6ee9062fb3f94b0ba31db6698724b1 contains an
               explanation of how this works */
            variant_count +=
                (Size_t) (((((start_bytes)) >> 7) * PERL_COUNT_MULTIPLIER)
                                      >> ((PERL_WORDSIZE - 1) * CHARBITS));

            s += PERL_WORDSIZE;
        } while (s + PERL_WORDSIZE <= send);

        /* If the final byte was a start byte, it means that the character
         * straddles two words, so back off one to start looking below at the
         * first byte of the character  */
        if (s > first_variant && UTF8_IS_START(*(s-1))) {
            s--;
            variant_count--;
        }
    }

#endif
    /* Do the straggler bytes beyond what the loop above did */
    while (s < send) {
        if (! UTF8_IS_INVARIANT(*s)) {
            if (! UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(s, send)) {
                return false;
            }
            s++;
            variant_count++;
        }
        s++;
    }

    /* Here, we passed the tests above and know how many UTF-8 variant
     * characters there are, which allows us to calculate the size to malloc
     * for the non-destructive case */
    U8 *d0;
    if (result_as == PL_utf8_to_bytes_overwrite) {
        d0 = s0;
    }
    else {
        Newx(d0, (*lenp) + 1 - variant_count, U8);
        Copy(s0, d0, invariant_length, U8);
    }

    U8 * d = d0 + invariant_length;

    /* For the cases where the per-word algorithm wasn't used, everything is
     * well-formed and can definitely be translated.  When the per word
     * algorithm was used, it found that all start bytes in the string were C2
     * or C3, hence any well-formed sequences are convertible to bytes.  But we
     * didn't test, for example, that there weren't two C2's in a row.  That
     * means that in the loop below, we have to be sure things are well-formed.
     * Because it is very very unlikely that we got this far for something
     * malformed, and because we prioritize speed in the normal case over the
     * malformed one, we go ahead and do the translation, and undo it if found
     * to be necessary. */
    s = first_variant;
    while (s < send) {
        U8 c = *s++;
        if (! UVCHR_IS_INVARIANT(c)) {

            /* Then it is a multi-byte character.  The first pass above
             * determined that the string contains only invariants, the C2 and
             * C3 start bytes, and continuation bytes.  The condition above
             * excluded this from being an invariant.  To be well formed, it
             * needs to be a start byte followed by a continuation byte. */
            if (   UNLIKELY(  UTF8_IS_CONTINUATION(c))
                || UNLIKELY(  s >= send)
                || UNLIKELY(! UTF8_IS_CONTINUATION(*s)))
            {
                goto cant_convert;
            }

            c = EIGHT_BIT_UTF8_TO_NATIVE(c, *s);
            s++;
        }

        *d++ = c;
    }

    /* Success! */
    *d = '\0';
    *lenp = d - d0;

    if (result_as != PL_utf8_to_bytes_overwrite) {
        *s_ptr = d0;
        if (result_as == PL_utf8_to_bytes_use_temporary) {
            SAVEFREEPV(*s_ptr);
        }
        else {
            *free_me = *s_ptr;
        }
    }

    return true;

  cant_convert: ;

    /* Here, we found a malformation in the input.  This won't happen except
     * when the per-word algorithm was used in the first pass, because that may
     * miss some malformations.  It determined that the only start bytes in the
     * text are C2 and C3, but didn't examine it to make sure each of those was
     * followed by precisely one continuation, for example.
     *
     * If the result is in newly allocated memory, just free it */
    if (result_as != PL_utf8_to_bytes_overwrite) {
        Safefree(d0);
        return false;
    }

    /* Otherwise, we have to undo all we've done before, back down to the first
     * UTF-8 variant.  Note that each 2-byte variant we've done so far
     * (converted to single byte) slides things to the left one byte, and so we
     * have bytes that haven't been written over.
     *
     * Here, 'd' points to the next position to overwrite, and 's' points to
     * the first invalid byte.  That means 'd's contents haven't been changed
     * yet, nor has anything else beyond it in the string.  In restoring to the
     * original contents, we don't need to do anything past (d-1).
     *
     * In particular, the bytes from 'd' to 's' have not been changed.  This
     * loop uses a new variable 's1' (to avoid confusing 'source' and
     * 'destination') set to 'd',  and moves 's' and 's1' in lock step back so
     * that afterwards, 's1' points to the first changed byte that will be the
     * source for the first byte (or bytes) at 's' that need to be changed
     * back.  Note that s1 can expand to two bytes */
    U8 * s1 = d;
    while (s >= d) {
        s--;
        if (! UVCHR_IS_INVARIANT(*s1)) {
            s--;
        }
        s1--;
    }

    /* Do the changing back */
    while (s1 >= first_variant) {
        if (UVCHR_IS_INVARIANT(*s1)) {
            *s-- = *s1--;
        }
        else {
            *s-- = UTF8_EIGHT_BIT_LO(*s1);
            *s-- = UTF8_EIGHT_BIT_HI(*s1);
            s1--;
        }
    }

    return false;
}

U8 *
Perl_utf8_to_bytes(pTHX_ U8 *s, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_UTF8_TO_BYTES;

    if (utf8_to_bytes_overwrite(&s, lenp)) {
        return s;
    }

    *lenp = (STRLEN) -1;
    return NULL;
}

U8 *
Perl_bytes_from_utf8(pTHX_ const U8 *s, STRLEN *lenp, bool *is_utf8p)
{
    PERL_ARGS_ASSERT_BYTES_FROM_UTF8;

    if (*is_utf8p) {
        void * new_memory = NULL;
        if (utf8_to_bytes_new_pv(&s, lenp, &new_memory)) {
            *is_utf8p = false;

            /* Our callers are always expecting new memory upon success.  Give
             * it to them, adding a trailing NUL if not already there */
            if (new_memory == NULL) {
                U8 * new_s;
                Newx(new_s, *lenp + 1, U8);
                Copy(s, new_s, *lenp, U8);
                new_s[*lenp] = '\0';
                s = new_s;
            }
        }
    }

    return (U8 *) s;
}

/*
=for apidoc      bytes_to_utf8
=for apidoc_item bytes_to_utf8_free_me
=for apidoc_item bytes_to_utf8_temp_pv

These each convert a string C<s> of length C<*lenp> bytes from the native
encoding into UTF-8 (UTF-EBCDIC on EBCDIC platforms), returning a pointer to
the UTF-8 string, and setting C<*lenp> to its length in bytes.

C<bytes_to_utf8> always allocates new memory for the result, making sure it is
NUL-terminated.

C<bytes_to_utf8_free_me> simply returns a pointer to the input string if the
string's UTF-8 representation is the same as its native representation.
Otherwise, it behaves like C<bytes_to_utf8>, returning a pointer to new memory
containing the conversion of the input.  In other words, it returns the input
string if converting the string would be a no-op.  Note that when no new string
is allocated, the function can't add a NUL to the original string if one wasn't
already there.

In both cases, the caller is responsible for arranging for any new memory to
get freed.

C<bytes_to_utf8_temp_pv> simply returns a pointer to the input string if the
string's UTF-8 representation is the same as its native representation, thus
behaving like C<bytes_to_utf8_free_me> in this situation.  Otherwise, it
behaves like C<bytes_to_utf8>, returning a pointer to new memory containing the
conversion of the input.  The difference is that it also arranges for the new
memory to automatically be freed by calling C<L</SAVEFREEPV>> on it.

C<bytes_to_utf8_free_me> takes an extra parameter, C<free_me> to communicate.
to the caller that memory was allocated or not.  If that parameter is NULL,
C<bytes_to_utf8_free_me> acts identically to C<bytes_to_utf8>, always
allocating new memory.

But when it is a non-NULL pointer, C<bytes_to_utf8_free_me> stores into it
either NULL if no memory was allocated; or a pointer to that new memory.  This
allows the following convenient paradigm:

 void * free_me;
 U8 converted = bytes_to_utf8_free_me(string, &len, &free_me);

 ...

 Safefree(free_me);

You don't have to know if memory was allocated or not.  Just call C<Safefree>
unconditionally.  C<free_me> will contain a suitable value to pass to
C<Safefree> for it to do the right thing, regardless.
Your design is likely flawed if you find yourself using C<free_me> for anything
other than passing to C<Safefree>.

Upon return, the number of variants in the string can be computed by having
saved the value of C<*lenp> before the call, and subtracting the after-call
value of C<*lenp> from it.

If you want to convert to UTF-8 from encodings other than the native (Latin1 or
EBCDIC), see L</sv_recode_to_utf8>().

=cut
*/

U8*
Perl_bytes_to_utf8_free_me(pTHX_ const U8 *s, Size_t *lenp,
                                 void ** free_me_ptr)
{
    PERL_ARGS_ASSERT_BYTES_TO_UTF8_FREE_ME;
    PERL_UNUSED_CONTEXT;

    const U8 * const send = s + (*lenp);
    const Size_t variant_count = variant_under_utf8_count(s, send);

    /* Return the input unchanged if the flag indicates to do so, and there
     * are no characters that differ when represented in UTF-8, and the
     * original is NUL-terminated */
    if (free_me_ptr != NULL && variant_count == 0) {
        *free_me_ptr = NULL;
        return (U8 *) s;
    }

    U8 *d;
    U8 *dst;

    /* 1 for each byte + 1 for each byte that expands to two, + trailing NUL */
    Newx(d, (*lenp) + variant_count + 1, U8);
    dst = d;

    while (s < send) {
        append_utf8_from_native_byte(*s, &d);
        s++;
    }

    *d = '\0';
    *lenp = d - dst;

    if (free_me_ptr != NULL) {
        *free_me_ptr = dst;
    }

    return dst;
}

/*
 * Convert native UTF-16 to UTF-8. Called via the more public functions
 * utf16_to_utf8() for big-endian and utf16_to_utf8_reversed() for
 * little-endian,
 *
 * 'p' is the UTF-16 input string, passed as a pointer to U8.
 * 'bytelen' is its length (must be even)
 * 'd' is the pointer to the destination buffer.  The caller must ensure that
 *     the space is large enough.  The maximum expansion factor is 2 times
 *     'bytelen'.  1.5 if never going to run on an EBCDIC box.
 * '*newlen' will contain the number of bytes this function filled of 'd'.
 * 'high_byte' is 0 if UTF-16BE; 1 if UTF-16LE
 * 'low_byte' is 1  if UTF-16BE; 0 if UTF-16LE
 *
 * The expansion factor is because UTF-16 requires 2 bytes for every code point
 * below 0x10000; otherwise 4 bytes.  UTF-8 requires 1-3 bytes for every code
 * point below 0x1000; otherwise 4 bytes.  UTF-EBCDIC requires 1-4 bytes for
 * every code point below 0x1000; otherwise 4-5 bytes.
 *
 * The worst case is where every code point is below U+10000, hence requiring 2
 * UTF-16 bytes, but is U+0800 or higher on ASCII platforms, requiring 3 UTF-8
 * bytes; or >= U+4000 on EBCDIC requiring 4 UTF-8 bytes.
 *
 * Do not use in-place. */

U8*
Perl_utf16_to_utf8_base(pTHX_ U8* p, U8* d, Size_t bytelen, Size_t *newlen,
                              const bool high_byte, /* Which of next two bytes is
                                                  high order */
                              const bool low_byte)
{
    U8* pend;
    U8* dstart = d;

    PERL_ARGS_ASSERT_UTF16_TO_UTF8_BASE;

    if (bytelen & 1)
        croak("panic: utf16_to_utf8%s: odd bytelen %" UVuf,
                ((high_byte == 0) ? "" : "_reversed"), (UV)bytelen);
    pend = p + bytelen;

    while (p < pend) {

        /* Next 16 bits is what we want.  (The bool is cast to U8 because on
         * platforms where a bool is implemented as a signed char, a compiler
         * warning may be generated) */
        U32 uv = (p[(U8) high_byte] << 8) + p[(U8) low_byte];
        p += 2;

        /* If it's a surrogate, we find the uv that the surrogate pair encodes.
         * */
        if (UNLIKELY(UNICODE_IS_SURROGATE(uv))) {

#define FIRST_HIGH_SURROGATE UNICODE_SURROGATE_FIRST
#define LAST_HIGH_SURROGATE  0xDBFF
#define FIRST_LOW_SURROGATE  0xDC00
#define LAST_LOW_SURROGATE   UNICODE_SURROGATE_LAST
#define FIRST_IN_PLANE1      0x10000

            if (UNLIKELY(p >= pend) || UNLIKELY(uv > LAST_HIGH_SURROGATE)) {
                croak("Malformed UTF-16 surrogate");
            }
            else {
                U32 low_surrogate = (p[(U8) high_byte] << 8) + p[(U8) low_byte];
                if (UNLIKELY(! inRANGE(low_surrogate, FIRST_LOW_SURROGATE,
                                                       LAST_LOW_SURROGATE)))
                {
                    croak("Malformed UTF-16 surrogate");
                }

                p += 2;

                /* Here uv is the high surrogate.  Combine with low surrogate
                 * just computed to form the actual U32 code point.
                 *
                 * From https://unicode.org/faq/utf_bom.html#utf16-4 */
                uv = FIRST_IN_PLANE1 + (uv << 10) - (FIRST_HIGH_SURROGATE << 10)
                                     + low_surrogate - FIRST_LOW_SURROGATE;
            }
        }

        /* Here, 'uv' is the real U32 we want to find the UTF-8 of */
        d = uv_to_utf8(d, uv);
    }

    *newlen = d - dstart;
    return d;
}

U8*
Perl_utf16_to_utf8(pTHX_ U8* p, U8* d, Size_t bytelen, Size_t *newlen)
{
    PERL_ARGS_ASSERT_UTF16_TO_UTF8;

    return utf16_to_utf8(p, d, bytelen, newlen);
}

U8*
Perl_utf16_to_utf8_reversed(pTHX_ U8* p, U8* d, Size_t bytelen, Size_t *newlen)
{
    PERL_ARGS_ASSERT_UTF16_TO_UTF8_REVERSED;

    return utf16_to_utf8_reversed(p, d, bytelen, newlen);
}

/*
 * Convert UTF-8 to native UTF-16. Called via the macros utf8_to_utf16() for
 * big-endian and utf8_to_utf16_reversed() for little-endian,
 *
 * 's' is the UTF-8 input string, passed as a pointer to U8.
 * 'bytelen' is its length
 * 'd' is the pointer to the destination buffer, currently passed as U8 *.  The
 *     caller must ensure that the space is large enough.  The maximum
 *     expansion factor is 2 times 'bytelen'.  This happens when the input is
 *     entirely single-byte ASCII, expanding to two-byte UTF-16.
 * '*newlen' will contain the number of bytes this function filled of 'd'.
 * 'high_byte' is 0 if UTF-16BE; 1 if UTF-16LE
 * 'low_byte'  is 1 if UTF-16BE; 0 if UTF-16LE
 *
 * Do not use in-place. */
U8*
Perl_utf8_to_utf16_base(pTHX_ U8* s, U8* d, Size_t bytelen, Size_t *newlen,
                              const bool high_byte, /* Which of next two bytes
                                                       is high order */
                              const bool low_byte)
{
    U8* send;
    U8* dstart = d;

    PERL_ARGS_ASSERT_UTF8_TO_UTF16_BASE;

    send = s + bytelen;

    while (s < send) {
        STRLEN retlen;
        UV uv;
        (void) c9strict_utf8_to_uv(s, send, &uv, &retlen);

        if (uv >= FIRST_IN_PLANE1) {    /* Requires a surrogate pair */

            /* From https://unicode.org/faq/utf_bom.html#utf16-4 */
            U32 high_surrogate = (uv >> 10) - (FIRST_IN_PLANE1 >> 10)
                               + FIRST_HIGH_SURROGATE;

            /* (The bool is cast to U8 because on platforms where a bool is
             * implemented as a signed char, a compiler warning may be
             * generated) */
            d[(U8) high_byte] = high_surrogate >> 8;
            d[(U8) low_byte]  = high_surrogate & nBIT_MASK(8);
            d += 2;

            /* The low surrogate is the lower 10 bits plus the offset */
            uv &= nBIT_MASK(10);
            uv += FIRST_LOW_SURROGATE;

            /* Drop down to output the low surrogate like it were a
             * non-surrogate */
        }

        d[(U8) high_byte] = uv >> 8;
        d[(U8) low_byte] = uv & nBIT_MASK(8);
        d += 2;

        s += retlen;
    }

    *newlen = d - dstart;
    return d;
}

bool
Perl__is_uni_FOO(pTHX_ const U8 classnum, const UV c)
{
    return _invlist_contains_cp(PL_XPosix_ptrs[classnum], c);
}

bool
Perl__is_uni_perl_idcont(pTHX_ UV c)
{
    return _invlist_contains_cp(PL_utf8_perl_idcont, c);
}

bool
Perl__is_uni_perl_idstart(pTHX_ UV c)
{
    return _invlist_contains_cp(PL_utf8_perl_idstart, c);
}

UV
Perl__to_upper_title_latin1(pTHX_ const U8 c, U8* p, STRLEN *lenp,
                                  const char S_or_s)
{
    /* We have the latin1-range values compiled into the core, so just use
     * those, converting the result to UTF-8.  The only difference between upper
     * and title case in this range is that LATIN_SMALL_LETTER_SHARP_S is
     * either "SS" or "Ss".  Which one to use is passed into the routine in
     * 'S_or_s' to avoid a test */

    UV converted = toUPPER_LATIN1_MOD(c);

    PERL_ARGS_ASSERT__TO_UPPER_TITLE_LATIN1;

    assert(S_or_s == 'S' || S_or_s == 's');

    if (UVCHR_IS_INVARIANT(converted)) { /* No difference between the two for
                                             characters in this range */
        *p = (U8) converted;
        *lenp = 1;
        return converted;
    }

    /* toUPPER_LATIN1_MOD gives the correct results except for three outliers,
     * which it maps to one of them, so as to only have to have one check for
     * it in the main case */
    if (UNLIKELY(converted == LATIN_SMALL_LETTER_Y_WITH_DIAERESIS)) {
        switch (c) {
            case LATIN_SMALL_LETTER_Y_WITH_DIAERESIS:
                converted = LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS;
                break;
            case MICRO_SIGN:
                converted = GREEK_CAPITAL_LETTER_MU;
                break;
#if    UNICODE_MAJOR_VERSION > 2                                        \
   || (UNICODE_MAJOR_VERSION == 2 && UNICODE_DOT_VERSION >= 1           \
                                  && UNICODE_DOT_DOT_VERSION >= 8)
            case LATIN_SMALL_LETTER_SHARP_S:
                *(p)++ = 'S';
                *p = S_or_s;
                *lenp = 2;
                return 'S';
#endif
            default:
                croak("panic: to_upper_title_latin1 did not expect"
                                 " '%c' to map to '%c'",
                                 c, LATIN_SMALL_LETTER_Y_WITH_DIAERESIS);
                NOT_REACHED; /* NOTREACHED */
        }
    }

    *(p)++ = UTF8_TWO_BYTE_HI(converted);
    *p = UTF8_TWO_BYTE_LO(converted);
    *lenp = 2;

    return converted;
}

/* If compiled on an early Unicode version, there may not be auxiliary tables
 * */
#ifndef HAS_UC_AUX_TABLES
#  define UC_AUX_TABLE_ptrs     NULL
#  define UC_AUX_TABLE_lengths  NULL
#endif
#ifndef HAS_TC_AUX_TABLES
#  define TC_AUX_TABLE_ptrs     NULL
#  define TC_AUX_TABLE_lengths  NULL
#endif
#ifndef HAS_LC_AUX_TABLES
#  define LC_AUX_TABLE_ptrs     NULL
#  define LC_AUX_TABLE_lengths  NULL
#endif
#ifndef HAS_CF_AUX_TABLES
#  define CF_AUX_TABLE_ptrs     NULL
#  define CF_AUX_TABLE_lengths  NULL
#endif

/* Call the function to convert a UTF-8 encoded character to the specified case.
 * Note that there may be more than one character in the result.
 * 's' is a pointer to the first byte of the input character
 * 'd' will be set to the first byte of the string of changed characters.  It
 *	needs to have space for UTF8_MAXBYTES_CASE+1 bytes
 * 'lenp' will be set to the length in bytes of the string of changed characters
 *
 * The functions return the ordinal of the first character in the string of
 * 'd' */
#define CALL_UPPER_CASE(uv, s, d, lenp)                                     \
                _to_utf8_case(uv, s, d, lenp, PL_utf8_toupper,              \
                                              Uppercase_Mapping_invmap,     \
                                              UC_AUX_TABLE_ptrs,            \
                                              UC_AUX_TABLE_lengths,         \
                                              "uppercase")
#define CALL_TITLE_CASE(uv, s, d, lenp)                                     \
                _to_utf8_case(uv, s, d, lenp, PL_utf8_totitle,              \
                                              Titlecase_Mapping_invmap,     \
                                              TC_AUX_TABLE_ptrs,            \
                                              TC_AUX_TABLE_lengths,         \
                                              "titlecase")
#define CALL_LOWER_CASE(uv, s, d, lenp)                                     \
                _to_utf8_case(uv, s, d, lenp, PL_utf8_tolower,              \
                                              Lowercase_Mapping_invmap,     \
                                              LC_AUX_TABLE_ptrs,            \
                                              LC_AUX_TABLE_lengths,         \
                                              "lowercase")


/* This additionally has the input parameter 'specials', which if non-zero will
 * cause this to use the specials hash for folding (meaning get full case
 * folding); otherwise, when zero, this implies a simple case fold */
#define CALL_FOLD_CASE(uv, s, d, lenp, specials)                            \
        (specials)                                                          \
        ?  _to_utf8_case(uv, s, d, lenp, PL_utf8_tofold,                    \
                                          Case_Folding_invmap,              \
                                          CF_AUX_TABLE_ptrs,                \
                                          CF_AUX_TABLE_lengths,             \
                                          "foldcase")                       \
        : _to_utf8_case(uv, s, d, lenp, PL_utf8_tosimplefold,               \
                                         Simple_Case_Folding_invmap,        \
                                         NULL, NULL,                        \
                                         "foldcase")

UV
Perl_to_uni_upper(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    /* Convert the Unicode character whose ordinal is <c> to its uppercase
     * version and store that in UTF-8 in <p> and its length in bytes in <lenp>.
     * Note that the <p> needs to be at least UTF8_MAXBYTES_CASE+1 bytes since
     * the changed version may be longer than the original character.
     *
     * The ordinal of the first character of the changed version is returned
     * (but note, as explained above, that there may be more.) */

    PERL_ARGS_ASSERT_TO_UNI_UPPER;

    if (c < 256) {
        return _to_upper_title_latin1((U8) c, p, lenp, 'S');
    }

    return CALL_UPPER_CASE(c, NULL, p, lenp);
}

UV
Perl_to_uni_title(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UNI_TITLE;

    if (c < 256) {
        return _to_upper_title_latin1((U8) c, p, lenp, 's');
    }

    return CALL_TITLE_CASE(c, NULL, p, lenp);
}

STATIC U8
S_to_lower_latin1(const U8 c, U8* p, STRLEN *lenp, const char dummy)
{
    /* We have the latin1-range values compiled into the core, so just use
     * those, converting the result to UTF-8.  Since the result is always just
     * one character, we allow <p> to be NULL */

    U8 converted = toLOWER_LATIN1(c);

    PERL_UNUSED_ARG(dummy);

    if (p != NULL) {
        if (NATIVE_BYTE_IS_INVARIANT(converted)) {
            *p = converted;
            *lenp = 1;
        }
        else {
            /* Result is known to always be < 256, so can use the EIGHT_BIT
             * macros */
            *p = UTF8_EIGHT_BIT_HI(converted);
            *(p+1) = UTF8_EIGHT_BIT_LO(converted);
            *lenp = 2;
        }
    }
    return converted;
}

UV
Perl_to_uni_lower(pTHX_ UV c, U8* p, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_TO_UNI_LOWER;

    if (c < 256) {
        return to_lower_latin1((U8) c, p, lenp, 0 /* 0 is a dummy arg */ );
    }

    return CALL_LOWER_CASE(c, NULL, p, lenp);
}

UV
Perl__to_fold_latin1(const U8 c, U8* p, STRLEN *lenp, const unsigned int flags)
{
    /* Corresponds to to_lower_latin1(); <flags> bits meanings:
     *	    FOLD_FLAGS_NOMIX_ASCII iff non-ASCII to ASCII folds are prohibited
     *	    FOLD_FLAGS_FULL  iff full folding is to be used;
     *
     *	Not to be used for locale folds
     */

    UV converted;

    PERL_ARGS_ASSERT__TO_FOLD_LATIN1;

    assert (! (flags & FOLD_FLAGS_LOCALE));

    if (UNLIKELY(c == MICRO_SIGN)) {
        converted = GREEK_SMALL_LETTER_MU;
    }
#if    UNICODE_MAJOR_VERSION > 3 /* no multifolds in early Unicode */   \
   || (UNICODE_MAJOR_VERSION == 3 && (   UNICODE_DOT_VERSION > 0)       \
                                      || UNICODE_DOT_DOT_VERSION > 0)
    else if (   (flags & FOLD_FLAGS_FULL)
             && UNLIKELY(c == LATIN_SMALL_LETTER_SHARP_S))
    {
        /* If can't cross 127/128 boundary, can't return "ss"; instead return
         * two U+017F characters, as fc("\df") should eq fc("\x{17f}\x{17f}")
         * under those circumstances. */
        if (flags & FOLD_FLAGS_NOMIX_ASCII) {
            *lenp = 2 * STRLENs(LATIN_SMALL_LETTER_LONG_S_UTF8);
            Copy(LATIN_SMALL_LETTER_LONG_S_UTF8 LATIN_SMALL_LETTER_LONG_S_UTF8,
                 p, *lenp, U8);
            return LATIN_SMALL_LETTER_LONG_S;
        }
        else {
            *(p)++ = 's';
            *p = 's';
            *lenp = 2;
            return 's';
        }
    }
#endif
    else { /* In this range the fold of all other characters is their lower
              case */
        converted = toLOWER_LATIN1(c);
    }

    if (UVCHR_IS_INVARIANT(converted)) {
        *p = (U8) converted;
        *lenp = 1;
    }
    else {
        *(p)++ = UTF8_TWO_BYTE_HI(converted);
        *p = UTF8_TWO_BYTE_LO(converted);
        *lenp = 2;
    }

    return converted;
}

UV
Perl__to_uni_fold_flags(pTHX_ UV c, U8* p, STRLEN *lenp, U8 flags)
{

    /* Not currently externally documented, and subject to change
     *  <flags> bits meanings:
     *	    FOLD_FLAGS_FULL  iff full folding is to be used;
     *	    FOLD_FLAGS_LOCALE is set iff the rules from the current underlying
     *	                      locale are to be used.
     *	    FOLD_FLAGS_NOMIX_ASCII iff non-ASCII to ASCII folds are prohibited
     */

    PERL_ARGS_ASSERT__TO_UNI_FOLD_FLAGS;

    if (flags & FOLD_FLAGS_LOCALE) {
        /* Treat a non-Turkic UTF-8 locale as not being in locale at all,
         * except for potentially warning */
        CHECK_AND_WARN_PROBLEMATIC_LOCALE_;
        if (IN_UTF8_CTYPE_LOCALE && ! IN_UTF8_TURKIC_LOCALE) {
            flags &= ~FOLD_FLAGS_LOCALE;
        }
        else {
            goto needs_full_generality;
        }
    }

    if (c < 256) {
        return _to_fold_latin1((U8) c, p, lenp,
                            flags & (FOLD_FLAGS_FULL | FOLD_FLAGS_NOMIX_ASCII));
    }

    /* Here, above 255.  If no special needs, just use the macro */
    if ( ! (flags & (FOLD_FLAGS_LOCALE|FOLD_FLAGS_NOMIX_ASCII))) {
        return CALL_FOLD_CASE(c, NULL, p, lenp, flags & FOLD_FLAGS_FULL);
    }
    else {  /* Otherwise, _toFOLD_utf8_flags has the intelligence to deal with
               the special flags. */
        U8 utf8_c[UTF8_MAXBYTES + 1];

      needs_full_generality:
        uv_to_utf8(utf8_c, c);
        return _toFOLD_utf8_flags(utf8_c, utf8_c + C_ARRAY_LENGTH(utf8_c),
                                  p, lenp, flags);
    }
}

#if 0	/* Not currently used, but may be needed in the future */
PERLVAR(I, seen_deprecated_macro, HV *)

STATIC void
S_warn_on_first_deprecated_use(pTHX_ U32 category,
                                     const char * const name,
                                     const char * const alternative,
                                     const bool use_locale,
                                     const char * const file,
                                     const unsigned line)
{
    const char * key;

    PERL_ARGS_ASSERT_WARN_ON_FIRST_DEPRECATED_USE;

    if (ckWARN_d(category)) {

        key = form("%s;%d;%s;%d", name, use_locale, file, line);
        if (! hv_fetch(PL_seen_deprecated_macro, key, strlen(key), 0)) {
            if (! PL_seen_deprecated_macro) {
                PL_seen_deprecated_macro = newHV();
            }
            if (! hv_store(PL_seen_deprecated_macro, key,
                           strlen(key), &PL_sv_undef, 0))
            {
                croak("panic: hv_store() unexpectedly failed");
            }

            if (instr(file, "mathoms.c")) {
                warner(category,
                       "In %s, line %d, starting in Perl v5.32, %s()"
                       " will be removed.  Avoid this message by"
                       " converting to use %s().\n",
                       file, line, name, alternative);
            }
            else {
                warner(category,
                       "In %s, line %d, starting in Perl v5.32, %s() will"
                       " require an additional parameter.  Avoid this"
                       " message by converting to use %s().\n",
                       file, line, name, alternative);
            }
        }
    }
}
#endif

/* returns a boolean giving whether or not the UTF8-encoded character that
 * starts at <p>, and extending no further than <e - 1> is in the inversion
 * list <invlist>. */
#define IS_UTF8_IN_INVLIST(p, e, invlist)                                   \
            _invlist_contains_cp(invlist, utf8_to_uv_or_die(p, e, NULL))

bool
Perl__is_utf8_FOO(pTHX_ const U8 classnum, const U8 *p, const U8 * const e)
{
    PERL_ARGS_ASSERT__IS_UTF8_FOO;

    return IS_UTF8_IN_INVLIST(p, e, PL_XPosix_ptrs[classnum]);
}

bool
Perl__is_utf8_perl_idstart(pTHX_ const U8 *p, const U8 * const e)
{
    PERL_ARGS_ASSERT__IS_UTF8_PERL_IDSTART;

    return IS_UTF8_IN_INVLIST(p, e, PL_utf8_perl_idstart);
}

bool
Perl__is_utf8_perl_idcont(pTHX_ const U8 *p, const U8 * const e)
{
    PERL_ARGS_ASSERT__IS_UTF8_PERL_IDCONT;

    return IS_UTF8_IN_INVLIST(p, e, PL_utf8_perl_idcont);
}

STATIC UV
S_to_case_cp_list(pTHX_
                  const UV original,
                  const U32 ** const remaining_list,
                  Size_t * remaining_count,
                  SV *invlist, const I32 * const invmap,
                  const U32 * const * const aux_tables,
                  const U8 * const aux_table_lengths,
                  const char * const normal)
{
    SSize_t index;
    I32 base;

    /* Calculate the changed case of code point 'original'.  The first code
     * point of the changed case is returned.
     *
     * If 'remaining_count' is not NULL, *remaining_count will be set to how
     * many *other* code points are in the changed case.  If non-zero and
     * 'remaining_list' is also not NULL, *remaining_list will be set to point
     * to a non-modifiable array containing the second and potentially third
     * code points in the changed case.  (Unicode guarantees a maximum of 3.)
     * Note that this means that *remaining_list is undefined unless there are
     * multiple code points, and the caller has chosen to find out how many by
     * making 'remaining_count' not NULL.
     *
     * 'normal' is a string to use to name the new case in any generated
     * messages, as a fallback if the operation being used is not available.
     *
     * The casing to use is given by the data structures in the remaining
     * arguments.
     */

    PERL_ARGS_ASSERT_TO_CASE_CP_LIST;

    /* 'index' is guaranteed to be non-negative, as this is an inversion map
     * that covers all possible inputs.  See [perl #133365] */
    index = _invlist_search(invlist, original);
    base = invmap[index];

    /* Most likely, the case change will contain just a single code point */
    if (remaining_count) {
        *remaining_count = 0;
    }

    if (LIKELY(base == 0)) {    /* 0 => original was unchanged by casing */

        /* At this bottom level routine is where we warn about illegal code
         * points */
        if (isUNICODE_POSSIBLY_PROBLEMATIC(original)) {
            if (UNLIKELY(UNICODE_IS_SURROGATE(original))) {
                ck_warner_d(packWARN(WARN_SURROGATE),
                            "Operation \"%s\" returns its argument for"
                            " UTF-16 surrogate U+%04" UVXf,
                            (PL_op) ? OP_DESC(PL_op) : normal,
                            original);
            }
            else if (UNLIKELY(UNICODE_IS_SUPER(original))) {
                if (UNLIKELY(original > MAX_LEGAL_CP))
                    croak("%s", form_cp_too_large_msg(16, NULL, 0, original));

                ck_warner_d(packWARN(WARN_NON_UNICODE),
                            "Operation \"%s\" returns its argument for"
                            " non-Unicode code point 0x%04" UVXf,
                            (PL_op) ? OP_DESC(PL_op) : normal,
                            original);
            }

            /* Note that non-characters are perfectly legal, so no warning
             * should be given. */
        }

        return original;
    }

    if (LIKELY(base > 0)) {  /* means original mapped to a single code point,
                                different from itself */
        return base + original - invlist_array(invlist)[index];
    }

    /* Here 'base' is negative.  That means the mapping is 1-to-many, and
     * requires an auxiliary table look up.  abs(base) gives the index into a
     * list of such tables which points to the proper aux table.  And a
     * parallel list gives the length of each corresponding aux table.  Skip
     * the first entry in the *remaining returns, as it is returned by the
     * function. */
    base = -base;
    if (remaining_count) {
        *remaining_count = (Size_t) (aux_table_lengths[base] - 1);

        if (remaining_list) {
            *remaining_list  = aux_tables[base] + 1;
        }
    }

    return (UV) aux_tables[base][0];
}

STATIC UV
S__to_utf8_case(pTHX_ const UV original, const U8 *p,
                      U8* ustrp, STRLEN *lenp,
                      SV *invlist, const I32 * const invmap,
                      const U32 * const * const aux_tables,
                      const U8 * const aux_table_lengths,
                      const char * const normal)
{
    /* Change the case of code point 'original'.  If 'p' is non-NULL, it points to
     * the beginning of the (assumed to be valid) UTF-8 representation of
     * 'original'.  'normal' is a string to use to name the new case in any
     * generated messages, as a fallback if the operation being used is not
     * available.  The new case is given by the data structures in the
     * remaining arguments.
     *
     * On return 'ustrp' points to '*lenp' UTF-8 encoded bytes representing the
     * entire changed case string, and the return value is the first code point
     * in that string
     *
     * Note that the <ustrp> needs to be at least UTF8_MAXBYTES_CASE+1 bytes
     * since the changed version may be longer than the original character. */

    const U32 * remaining_list;
    Size_t remaining_count;
    UV first = to_case_cp_list(original,
                               &remaining_list, &remaining_count,
                               invlist, invmap,
                               aux_tables, aux_table_lengths,
                               normal);

    PERL_ARGS_ASSERT__TO_UTF8_CASE;

    /* If the code point maps to itself and we already have its representation,
     * copy it instead of recalculating */
    if (original == first && p) {
        *lenp = UTF8SKIP(p);

        if (p != ustrp) {   /* Don't copy onto itself */
            Copy(p, ustrp, *lenp, U8);
        }
    }
    else {
        U8 * d = ustrp;
        Size_t i;

        d = uv_to_utf8(d, first);

        for (i = 0; i < remaining_count; i++) {
            d = uv_to_utf8(d, remaining_list[i]);
        }

        *d = '\0';
        *lenp = d - ustrp;
    }

    return first;
}

Size_t
Perl__inverse_folds(pTHX_ const UV cp, U32 * first_folds_to,
                          const U32 ** remaining_folds_to)
{
    /* Returns the count of the number of code points that fold to the input
     * 'cp' (besides itself).
     *
     * If the return is 0, there is nothing else that folds to it, and
     * '*first_folds_to' is set to 0, and '*remaining_folds_to' is set to NULL.
     *
     * If the return is 1, '*first_folds_to' is set to the single code point,
     * and '*remaining_folds_to' is set to NULL.
     *
     * Otherwise, '*first_folds_to' is set to a code point, and
     * '*remaining_fold_to' is set to an array that contains the others.  The
     * length of this array is the returned count minus 1.
     *
     * The reason for this convolution is to avoid having to deal with
     * allocating and freeing memory.  The lists are already constructed, so
     * the return can point to them, but single code points aren't, so would
     * need to be constructed if we didn't employ something like this API
     *
     * The code points returned by this function are all legal Unicode, which
     * occupy at most 21 bits, and so a U32 is sufficient, and the lists are
     * constructed with this size (to save space and memory), and we return
     * pointers, so they must be this size */

    /* 'index' is guaranteed to be non-negative, as this is an inversion map
     * that covers all possible inputs.  See [perl #133365] */
    SSize_t index = _invlist_search(PL_utf8_foldclosures, cp);
    I32 base = _Perl_IVCF_invmap[index];

    PERL_ARGS_ASSERT__INVERSE_FOLDS;

    if (base == 0) {            /* No fold */
        *first_folds_to = 0;
        *remaining_folds_to = NULL;
        return 0;
    }

#ifndef HAS_IVCF_AUX_TABLES     /* This Unicode version only has 1-1 folds */

    assert(base > 0);

#else

    if (UNLIKELY(base < 0)) {   /* Folds to more than one character */

        /* The data structure is set up so that the absolute value of 'base' is
         * an index into a table of pointers to arrays, with the array
         * corresponding to the index being the list of code points that fold
         * to 'cp', and the parallel array containing the length of the list
         * array */
        *first_folds_to = IVCF_AUX_TABLE_ptrs[-base][0];
        *remaining_folds_to = IVCF_AUX_TABLE_ptrs[-base] + 1;
                                                /* +1 excludes first_folds_to */
        return IVCF_AUX_TABLE_lengths[-base];
    }

#endif

    /* Only the single code point.  This works like 'fc(G) = G - A + a' */
    *first_folds_to = (U32) (base + cp
                                  - invlist_array(PL_utf8_foldclosures)[index]);
    *remaining_folds_to = NULL;
    return 1;
}

STATIC UV
S_check_locale_boundary_crossing(pTHX_ const U8* const p, const UV result,
                                       U8* const ustrp, STRLEN *lenp)
{
    /* This is called when changing the case of a UTF-8-encoded character above
     * the Latin1 range, and the operation is in a non-UTF-8 locale.  If the
     * result contains a character that crosses the 255/256 boundary, disallow
     * the change, and return the original code point.  See L<perlfunc/lc> for
     * why;
     *
     * p	points to the original string whose case was changed; assumed
     *          by this routine to be well-formed
     * result	the code point of the first character in the changed-case string
     * ustrp	points to the changed-case string (<result> represents its
     *          first char)
     * lenp	points to the length of <ustrp> */

    UV original;    /* To store the first code point of <p> */

    PERL_ARGS_ASSERT_CHECK_LOCALE_BOUNDARY_CROSSING;

    assert(UTF8_IS_ABOVE_LATIN1(*p));

    /* We know immediately if the first character in the string crosses the
     * boundary, so can skip testing */
    if (result > 255) {

        /* Look at every character in the result; if any cross the
        * boundary, the whole thing is disallowed */
        U8* s = ustrp + UTF8SKIP(ustrp);
        U8* e = ustrp + *lenp;
        while (s < e) {
            if (! UTF8_IS_ABOVE_LATIN1(*s)) {
                goto bad_crossing;
            }
            s += UTF8SKIP(s);
        }

        /* Here, no characters crossed, result is ok as-is, but we warn. */
        CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG_(p, p + UTF8SKIP(p));
        return result;
    }

  bad_crossing:

    /* Failed, have to return the original */
    original = valid_utf8_to_uvchr(p, lenp);

    /* diag_listed_as: Can't do %s("%s") on non-UTF-8 locale; resolved to "%s". */
    ck_warner(packWARN(WARN_LOCALE),
              "Can't do %s(\"\\x{%" UVXf "}\") on non-UTF-8"
              " locale; resolved to \"\\x{%" UVXf "}\".",
              OP_DESC(PL_op), original, original);
    Copy(p, ustrp, *lenp, char);
    return original;
}

STATIC UV
S_turkic_fc(pTHX_ const U8 * const p, const U8 * const e,
                        U8 * ustrp, STRLEN *lenp)
{
    /* Returns 0 if the foldcase of the input UTF-8 encoded sequence from
     * p0..e-1 according to Turkic rules is the same as for non-Turkic.
     * Otherwise, it returns the first code point of the Turkic foldcased
     * sequence, and the entire sequence will be stored in *ustrp.  ustrp will
     * contain *lenp bytes
     *
     * Turkic differs only from non-Turkic in that 'i' and LATIN CAPITAL LETTER
     * I WITH DOT ABOVE form a case pair, as do 'I' and LATIN SMALL LETTER
     * DOTLESS I */

    PERL_ARGS_ASSERT_TURKIC_FC;
    assert(e > p);

    if (UNLIKELY(*p == 'I')) {
        *lenp = 2;
        ustrp[0] = UTF8_TWO_BYTE_HI(LATIN_SMALL_LETTER_DOTLESS_I);
        ustrp[1] = UTF8_TWO_BYTE_LO(LATIN_SMALL_LETTER_DOTLESS_I);
        return LATIN_SMALL_LETTER_DOTLESS_I;
    }

    if (UNLIKELY(memBEGINs(p, e - p,
                           LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE_UTF8)))
    {
        *lenp = 1;
        *ustrp = 'i';
        return 'i';
    }

    return 0;
}

STATIC UV
S_turkic_lc(pTHX_ const U8 * const p0, const U8 * const e,
                        U8 * ustrp, STRLEN *lenp)
{
    /* Returns 0 if the lowercase of the input UTF-8 encoded sequence from
     * p0..e-1 according to Turkic rules is the same as for non-Turkic.
     * Otherwise, it returns the first code point of the Turkic lowercased
     * sequence, and the entire sequence will be stored in *ustrp.  ustrp will
     * contain *lenp bytes */

    PERL_ARGS_ASSERT_TURKIC_LC;
    assert(e > p0);

    /* A 'I' requires context as to what to do */
    if (UNLIKELY(*p0 == 'I')) {
        const U8 * p = p0 + 1;

        /* According to the Unicode SpecialCasing.txt file, a capital 'I'
         * modified by a dot above lowercases to 'i' even in turkic locales. */
        while (p < e) {
            UV cp;

            if (memBEGINs(p, e - p, COMBINING_DOT_ABOVE_UTF8)) {
                ustrp[0] = 'i';
                *lenp = 1;
                return 'i';
            }

            /* For the dot above to modify the 'I', it must be part of a
             * combining sequence immediately following the 'I', and no other
             * modifier with a ccc of 230 may intervene */
            cp = utf8_to_uv_or_die(p, e, NULL);
            if (! _invlist_contains_cp(PL_CCC_non0_non230, cp)) {
                break;
            }

            /* Here the combining sequence continues */
            p += UTF8SKIP(p);
        }
    }

    /* In all other cases the lc is the same as the fold */
    return turkic_fc(p0, e, ustrp, lenp);
}

STATIC UV
S_turkic_uc(pTHX_ const U8 * const p, const U8 * const e,
                        U8 * ustrp, STRLEN *lenp)
{
    /* Returns 0 if the upper or title-case of the input UTF-8 encoded sequence
     * from p0..e-1 according to Turkic rules is the same as for non-Turkic.
     * Otherwise, it returns the first code point of the Turkic upper or
     * title-cased sequence, and the entire sequence will be stored in *ustrp.
     * ustrp will contain *lenp bytes
     *
     * Turkic differs only from non-Turkic in that 'i' and LATIN CAPITAL LETTER
     * I WITH DOT ABOVE form a case pair, as do 'I' and LATIN SMALL LETTER
     * DOTLESS I */

    PERL_ARGS_ASSERT_TURKIC_UC;
    assert(e > p);

    if (*p == 'i') {
        *lenp = 2;
        ustrp[0] = UTF8_TWO_BYTE_HI(LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE);
        ustrp[1] = UTF8_TWO_BYTE_LO(LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE);
        return LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE;
    }

    if (memBEGINs(p, e - p, LATIN_SMALL_LETTER_DOTLESS_I_UTF8)) {
        *lenp = 1;
        *ustrp = 'I';
        return 'I';
    }

    return 0;
}

/* The process for changing the case is essentially the same for the four case
 * change types, except there are complications for folding.  Otherwise the
 * difference is only which case to change to.  To make sure that they all do
 * the same thing, the bodies of the functions are extracted out into the
 * following two macros.  The functions are written with the same variable
 * names, and these are known and used inside these macros.  It would be
 * better, of course, to have inline functions to do it, but since different
 * macros are called, depending on which case is being changed to, this is not
 * feasible in C (to khw's knowledge).  Two macros are created so that the fold
 * function can start with the common start macro, then finish with its special
 * handling; while the other three cases can just use the common end macro.
 *
 * The algorithm is to use the proper (passed in) macro or function to change
 * the case for code points that are below 256.  The macro is used if using
 * locale rules for the case change; the function if not.  If the code point is
 * above 255, it is computed from the input UTF-8, and another macro is called
 * to do the conversion.  If necessary, the output is converted to UTF-8.  If
 * using a locale, we have to check that the change did not cross the 255/256
 * boundary, see check_locale_boundary_crossing() for further details.
 *
 * The macros are split with the correct case change for the below-256 case
 * stored into 'result', and in the middle of an else clause for the above-255
 * case.  At that point in the 'else', 'result' is not the final result, but is
 * the input code point calculated from the UTF-8.  The fold code needs to
 * realize all this and take it from there.
 *
 * To deal with Turkic locales, the function specified by the parameter
 * 'turkic' is called when appropriate.
 *
 * If you read the two macros as sequential, it's easier to understand what's
 * going on. */
#define CASE_CHANGE_BODY_START(locale_flags, libc_change_function, L1_func,  \
                               L1_func_extra_param, turkic)                  \
                                                                             \
    if (flags & (locale_flags)) {                                            \
        CHECK_AND_WARN_PROBLEMATIC_LOCALE_;                                  \
        if (IN_UTF8_CTYPE_LOCALE) {                                          \
            if (UNLIKELY(IN_UTF8_TURKIC_LOCALE)) {                           \
                UV ret = turkic(p, e, ustrp, lenp);                          \
                if (ret) return ret;                                         \
            }                                                                \
                                                                             \
            /* Otherwise, treat a UTF-8 locale as not being in locale at     \
             * all */                                                        \
            flags &= ~(locale_flags);                                        \
        }                                                                    \
    }                                                                        \
                                                                             \
    if (UTF8_IS_INVARIANT(*p)) {                                             \
        if (flags & (locale_flags)) {                                        \
            result = libc_change_function(*p);                               \
        }                                                                    \
        else {                                                               \
            return L1_func(*p, ustrp, lenp, L1_func_extra_param);            \
        }                                                                    \
    }                                                                        \
    else if UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(p, e) {                          \
        U8 c = EIGHT_BIT_UTF8_TO_NATIVE(*p, *(p+1));                         \
        if (flags & (locale_flags)) {                                        \
            result = libc_change_function(c);                                \
        }                                                                    \
        else {                                                               \
            return L1_func(c, ustrp, lenp,  L1_func_extra_param);            \
        }                                                                    \
    }                                                                        \
    else {  /* malformed UTF-8 or ord above 255 */                           \
        result = utf8_to_uv_or_die(p, e, NULL);                              \

#define CASE_CHANGE_BODY_END(locale_flags, change_macro)                     \
        result = change_macro(result, p, ustrp, lenp);                       \
                                                                             \
        if (flags & (locale_flags)) {                                        \
            result = check_locale_boundary_crossing(p, result, ustrp, lenp); \
        }                                                                    \
        return result;                                                       \
    }                                                                        \
                                                                             \
    /* Here, used locale rules.  Convert back to UTF-8 */                    \
    if (UTF8_IS_INVARIANT(result)) {                                         \
        *ustrp = (U8) result;                                                \
        *lenp = 1;                                                           \
    }                                                                        \
    else {                                                                   \
        *ustrp = UTF8_EIGHT_BIT_HI((U8) result);                             \
        *(ustrp + 1) = UTF8_EIGHT_BIT_LO((U8) result);                       \
        *lenp = 2;                                                           \
    }                                                                        \
                                                                             \
    return result;

/* Not currently externally documented, and subject to change:
 * <flags> is set iff the rules from the current underlying locale are to
 *         be used. */

UV
Perl__to_utf8_upper_flags(pTHX_ const U8 *p,
                                const U8 *e,
                                U8* ustrp,
                                STRLEN *lenp,
                                bool flags)
{
    UV result;

    PERL_ARGS_ASSERT__TO_UTF8_UPPER_FLAGS;

    /* ~0 makes anything non-zero in 'flags' mean we are using locale rules */
    /* 2nd char of uc(U+DF) is 'S' */
    CASE_CHANGE_BODY_START(~0, toupper, _to_upper_title_latin1, 'S',
                                                                    turkic_uc);
    CASE_CHANGE_BODY_END  (~0, CALL_UPPER_CASE);
}

/* Not currently externally documented, and subject to change:
 * <flags> is set iff the rules from the current underlying locale are to be
 *         used.  Since titlecase is not defined in POSIX, for other than a
 *         UTF-8 locale, uppercase is used instead for code points < 256.
 */

UV
Perl__to_utf8_title_flags(pTHX_ const U8 *p,
                                const U8 *e,
                                U8* ustrp,
                                STRLEN *lenp,
                                bool flags)
{
    UV result;

    PERL_ARGS_ASSERT__TO_UTF8_TITLE_FLAGS;

    /* 2nd char of ucfirst(U+DF) is 's' */
    CASE_CHANGE_BODY_START(~0, toupper, _to_upper_title_latin1, 's',
                                                                    turkic_uc);
    CASE_CHANGE_BODY_END  (~0, CALL_TITLE_CASE);
}

/* Not currently externally documented, and subject to change:
 * <flags> is set iff the rules from the current underlying locale are to
 *         be used.
 */

UV
Perl__to_utf8_lower_flags(pTHX_ const U8 *p,
                                const U8 *e,
                                U8* ustrp,
                                STRLEN *lenp,
                                bool flags)
{
    UV result;

    PERL_ARGS_ASSERT__TO_UTF8_LOWER_FLAGS;

    CASE_CHANGE_BODY_START(~0, tolower, to_lower_latin1, 0 /* 0 is dummy */,
                                                                    turkic_lc);
    CASE_CHANGE_BODY_END  (~0, CALL_LOWER_CASE)
}

/* Not currently externally documented, and subject to change,
 * in <flags>
 *	bit FOLD_FLAGS_LOCALE is set iff the rules from the current underlying
 *	                      locale are to be used.
 *      bit FOLD_FLAGS_FULL   is set iff full case folds are to be used;
 *			      otherwise simple folds
 *      bit FOLD_FLAGS_NOMIX_ASCII is set iff folds of non-ASCII to ASCII are
 *			      prohibited
 */

UV
Perl__to_utf8_fold_flags(pTHX_ const U8 *p,
                               const U8 *e,
                               U8* ustrp,
                               STRLEN *lenp,
                               U8 flags)
{
    UV result;

    PERL_ARGS_ASSERT__TO_UTF8_FOLD_FLAGS;

    /* These are mutually exclusive */
    assert (! ((flags & FOLD_FLAGS_LOCALE) && (flags & FOLD_FLAGS_NOMIX_ASCII)));

    assert(p != ustrp); /* Otherwise overwrites */

    CASE_CHANGE_BODY_START(FOLD_FLAGS_LOCALE, tolower, _to_fold_latin1,
                 ((flags) & (FOLD_FLAGS_FULL | FOLD_FLAGS_NOMIX_ASCII)),
                                                                    turkic_fc);

        result = CALL_FOLD_CASE(result, p, ustrp, lenp, flags & FOLD_FLAGS_FULL);

        if (flags & FOLD_FLAGS_LOCALE) {

#           define LONG_S_T      LATIN_SMALL_LIGATURE_LONG_S_T_UTF8
#         ifdef LATIN_CAPITAL_LETTER_SHARP_S_UTF8
#           define CAP_SHARP_S   LATIN_CAPITAL_LETTER_SHARP_S_UTF8

            /* Special case these two characters, as what normally gets
             * returned under locale doesn't work */
            if (memBEGINs((char *) p, e - p, CAP_SHARP_S))
            {
                /* diag_listed_as: Can't do %s("%s") on non-UTF-8 locale; resolved to "%s". */
                ck_warner(packWARN(WARN_LOCALE),
                          "Can't do fc(\"\\x{1E9E}\") on non-UTF-8 locale; "
                          "resolved to \"\\x{17F}\\x{17F}\".");
                goto return_long_s;
            }
            else
#endif
                 if (memBEGINs((char *) p, e - p, LONG_S_T))
            {
                /* diag_listed_as: Can't do %s("%s") on non-UTF-8 locale; resolved to "%s". */
                ck_warner(packWARN(WARN_LOCALE),
                          "Can't do fc(\"\\x{FB05}\") on non-UTF-8 locale; "
                          "resolved to \"\\x{FB06}\".");
                goto return_ligature_st;
            }

#if    UNICODE_MAJOR_VERSION   == 3         \
    && UNICODE_DOT_VERSION     == 0         \
    && UNICODE_DOT_DOT_VERSION == 1
#           define DOTTED_I   LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE_UTF8

            /* And special case this on this Unicode version only, for the same
             * reaons the other two are special cased.  They would cross the
             * 255/256 boundary which is forbidden under /l, and so the code
             * wouldn't catch that they are equivalent (which they are only in
             * this release) */
            else if (memBEGINs((char *) p, e - p, DOTTED_I)) {
                /* diag_listed_as: Can't do %s("%s") on non-UTF-8 locale; resolved to "%s". */
                ck_warner(packWARN(WARN_LOCALE),
                          "Can't do fc(\"\\x{0130}\") on non-UTF-8 locale; "
                          "resolved to \"\\x{0131}\".");
                goto return_dotless_i;
            }
#endif

            return check_locale_boundary_crossing(p, result, ustrp, lenp);
        }
        else if (! (flags & FOLD_FLAGS_NOMIX_ASCII)) {
            return result;
        }
        else {
            /* This is called when changing the case of a UTF-8-encoded
             * character above the ASCII range, and the result should not
             * contain an ASCII character. */

            UV original;    /* To store the first code point of <p> */

            /* Look at every character in the result; if any cross the
            * boundary, the whole thing is disallowed */
            U8* s = ustrp;
            U8* send = ustrp + *lenp;
            while (s < send) {
                if (isASCII(*s)) {
                    /* Crossed, have to return the original */
                    original = valid_utf8_to_uvchr(p, lenp);

                    /* But in these instances, there is an alternative we can
                     * return that is valid */
                    if (original == LATIN_SMALL_LETTER_SHARP_S
#ifdef LATIN_CAPITAL_LETTER_SHARP_S /* not defined in early Unicode releases */
                        || original == LATIN_CAPITAL_LETTER_SHARP_S
#endif
                    ) {
                        goto return_long_s;
                    }
                    else if (original == LATIN_SMALL_LIGATURE_LONG_S_T) {
                        goto return_ligature_st;
                    }
#if    UNICODE_MAJOR_VERSION   == 3         \
    && UNICODE_DOT_VERSION     == 0         \
    && UNICODE_DOT_DOT_VERSION == 1

                    else if (original == LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE) {
                        goto return_dotless_i;
                    }
#endif
                    Copy(p, ustrp, *lenp, char);
                    return original;
                }
                s += UTF8SKIP(s);
            }

            /* Here, no characters crossed, result is ok as-is */
            return result;
        }
    }

    /* Here, used locale rules.  Convert back to UTF-8 */
    if (UTF8_IS_INVARIANT(result)) {
        *ustrp = (U8) result;
        *lenp = 1;
    }
    else {
        *ustrp = UTF8_EIGHT_BIT_HI((U8) result);
        *(ustrp + 1) = UTF8_EIGHT_BIT_LO((U8) result);
        *lenp = 2;
    }

    return result;

  return_long_s:
    /* Certain folds to 'ss' are prohibited by the options, but they do allow
     * folds to a string of two of these characters.  By returning this
     * instead, then, e.g.,
     *      fc("\x{1E9E}") eq fc("\x{17F}\x{17F}")
     * works. */

    *lenp = 2 * STRLENs(LATIN_SMALL_LETTER_LONG_S_UTF8);
    Copy(LATIN_SMALL_LETTER_LONG_S_UTF8   LATIN_SMALL_LETTER_LONG_S_UTF8,
        ustrp, *lenp, U8);
    return LATIN_SMALL_LETTER_LONG_S;

  return_ligature_st:
    /* Two folds to 'st' are prohibited by the options; instead we pick one and
     * have the other one fold to it */

    *lenp = STRLENs(LATIN_SMALL_LIGATURE_ST_UTF8);
    Copy(LATIN_SMALL_LIGATURE_ST_UTF8, ustrp, *lenp, U8);
    return LATIN_SMALL_LIGATURE_ST;

#if    UNICODE_MAJOR_VERSION   == 3         \
    && UNICODE_DOT_VERSION     == 0         \
    && UNICODE_DOT_DOT_VERSION == 1

  return_dotless_i:
    *lenp = STRLENs(LATIN_SMALL_LETTER_DOTLESS_I_UTF8);
    Copy(LATIN_SMALL_LETTER_DOTLESS_I_UTF8, ustrp, *lenp, U8);
    return LATIN_SMALL_LETTER_DOTLESS_I;

#endif

}

bool
Perl_check_utf8_print(pTHX_ const U8* s, const STRLEN len)
{
    /* May change: warns if surrogates, non-character code points, or
     * non-Unicode code points are in 's' which has length 'len' bytes.
     * Returns TRUE if none found; FALSE otherwise.  The only other validity
     * check is to make sure that this won't exceed the string's length nor
     * overflow */

    const U8* const e = s + len;
    bool ok = TRUE;

    PERL_ARGS_ASSERT_CHECK_UTF8_PRINT;

    while (s < e) {
        if (UTF8SKIP(s) > len) {
            ck_warner_d(packWARN(WARN_UTF8),
                        "%s in %s", unees, PL_op ? OP_DESC(PL_op) : "print");
            return FALSE;
        }
        if (UNLIKELY(isUTF8_POSSIBLY_PROBLEMATIC(*s))) {
            if (UNLIKELY(UTF8_IS_SUPER(s, e))) {
                if (   ckWARN_d(WARN_NON_UNICODE)
                    || UNLIKELY(does_utf8_overflow(s, s + len) >= ALMOST_CERTAINLY_OVERFLOWS))
                {
                    UV dummy;

                    /* A side effect of this function will be to warn */
                    (void) utf8_to_uv_flags(s, e, &dummy, NULL, UTF8_WARN_SUPER);
                    ok = FALSE;
                }
            }
            else if (UNLIKELY(UTF8_IS_SURROGATE(s, e))) {
                if (ckWARN_d(WARN_SURROGATE)) {
                    /* This has a different warning than the one the called
                     * function would output, so can't just call it, unlike we
                     * do for the non-chars and above-unicodes */
                    UV uv = utf8_to_uv_or_die(s, e, NULL);
                    warner(packWARN(WARN_SURROGATE),
                           "Unicode surrogate U+%04" UVXf " is illegal in UTF-8",
                           uv);
                    ok = FALSE;
                }
            }
            else if (   UNLIKELY(UTF8_IS_NONCHAR(s, e))
                     && (ckWARN_d(WARN_NONCHAR)))
            {
                UV dummy;

                /* A side effect of this function will be to warn */
                (void) utf8_to_uv_flags(s, e, &dummy, NULL, UTF8_WARN_NONCHAR);
                ok = FALSE;
            }
        }
        s += UTF8SKIP(s);
    }

    return ok;
}

/*
=for apidoc pv_uni_display

Build to the scalar C<dsv> a displayable version of the UTF-8 encoded string
C<spv>, length C<len>, the displayable version being at most C<pvlim> bytes
long (if longer, the rest is truncated and C<"..."> will be appended).

The C<flags> argument can have any combination of these flag bits

=over

=item C<UNI_DISPLAY_ISPRINT>

to display C<isPRINT()>able characters as themselves

=item C<UNI_DISPLAY_BACKSLASH>

to display the C<\\[nrfta\\]> as the backslashed versions (like C<"\n">)

(C<UNI_DISPLAY_BACKSLASH> is preferred over C<UNI_DISPLAY_ISPRINT> for C<"\\">).

=item C<UNI_DISPLAY_BACKSPACE>

to display C<\b> for a backspace, but only when C<UNI_DISPLAY_BACKSLASH> also
is set.

=item C<UNI_DISPLAY_REGEX>

This a shorthand for C<UNI_DISPLAY_ISPRINT> along with
C<UNI_DISPLAY_BACKSLASH>.

=item C<UNI_DISPLAY_QQ>

This a shorthand for all three C<UNI_DISPLAY_ISPRINT>,
C<UNI_DISPLAY_BACKSLASH>, and C<UNI_DISPLAY_BACKSLASH>.

=back

The pointer to the PV of the C<dsv> is returned.

See also L</sv_uni_display>.

=for apidoc Amnh||UNI_DISPLAY_BACKSLASH
=for apidoc Amnh||UNI_DISPLAY_BACKSPACE
=for apidoc Amnh||UNI_DISPLAY_ISPRINT
=for apidoc Amnh||UNI_DISPLAY_QQ
=for apidoc Amnh||UNI_DISPLAY_REGEX
=cut
*/
char *
Perl_pv_uni_display(pTHX_ SV *dsv, const U8 *spv, STRLEN len, STRLEN pvlim,
                          UV flags)
{
    PERL_ARGS_ASSERT_PV_UNI_DISPLAY;

    int truncated = 0;
    const U8 *s, *e;
    STRLEN next_len = 0;

    SvPVCLEAR(dsv);
    SvUTF8_off(dsv);
    for (s = spv, e = s + len; s < e; s += next_len) {
        UV u;
        bool ok = 0;

        if (pvlim && SvCUR(dsv) >= pvlim) {
             truncated++;
             break;
        }

        (void) utf8_to_uv(s, e, &u, &next_len);
        assert(next_len > 0);

        if (u < 256) {
            const U8 c = (U8) u;
            if (flags & UNI_DISPLAY_BACKSLASH) {
                if (    isMNEMONIC_CNTRL(c)
                    && (   c != '\b'
                        || (flags & UNI_DISPLAY_BACKSPACE)))
                {
                   const char * mnemonic = cntrl_to_mnemonic(c);
                   sv_catpvn(dsv, mnemonic, strlen(mnemonic));
                   ok = 1;
                }
                else if (c == '\\') {
                   sv_catpvs(dsv, "\\\\");
                   ok = 1;
                }
            }
            /* isPRINT() is the locale-blind version. */
            if (!ok && (flags & UNI_DISPLAY_ISPRINT) && isPRINT(c)) {
                const char string = c;
                sv_catpvn(dsv, &string, 1);
                ok = 1;
            }
        }
        if (!ok)
            sv_catpvf(dsv, "\\x{%" UVxf "}", u);
    }
    if (truncated)
         sv_catpvs(dsv, "...");

    return SvPVX(dsv);
}

/*
=for apidoc sv_uni_display

Build to the scalar C<dsv> a displayable version of the scalar C<sv>,
the displayable version being at most C<pvlim> bytes long
(if longer, the rest is truncated and "..." will be appended).

The C<flags> argument is as in L</pv_uni_display>().

The pointer to the PV of the C<dsv> is returned.

=cut
*/
char *
Perl_sv_uni_display(pTHX_ SV *dsv, SV *ssv, STRLEN pvlim, UV flags)
{
    const char * const ptr =
        isREGEXP(ssv) ? RX_WRAPPED((REGEXP*)ssv) : SvPVX_const(ssv);

    PERL_ARGS_ASSERT_SV_UNI_DISPLAY;

    return Perl_pv_uni_display(aTHX_ dsv, (const U8*)ptr,
                                SvCUR(ssv), pvlim, flags);
}

/*
=for apidoc foldEQ_utf8

Returns true if the leading portions of the strings C<s1> and C<s2> (either or
both of which may be in UTF-8) are the same case-insensitively; false
otherwise.  How far into the strings to compare is determined by other input
parameters.

If C<u1> is true, the string C<s1> is assumed to be in UTF-8-encoded Unicode;
otherwise it is assumed to be in native 8-bit encoding.  Correspondingly for
C<u2> with respect to C<s2>.

If the byte length C<l1> is non-zero, it says how far into C<s1> to check for
fold equality.  In other words, C<s1>+C<l1> will be used as a goal to reach.
The scan will not be considered to be a match unless the goal is reached, and
scanning won't continue past that goal.  Correspondingly for C<l2> with respect
to C<s2>.

If C<pe1> is non-C<NULL> and the pointer it points to is not C<NULL>, that
pointer is considered an end pointer to the position 1 byte past the maximum
point in C<s1> beyond which scanning will not continue under any circumstances.
(This routine assumes that UTF-8 encoded input strings are not malformed;
malformed input can cause it to read past C<pe1>).  This means that if both
C<l1> and C<pe1> are specified, and C<pe1> is less than C<s1>+C<l1>, the match
will never be successful because it can never
get as far as its goal (and in fact is asserted against).  Correspondingly for
C<pe2> with respect to C<s2>.

At least one of C<s1> and C<s2> must have a goal (at least one of C<l1> and
C<l2> must be non-zero), and if both do, both have to be
reached for a successful match.   Also, if the fold of a character is multiple
characters, all of them must be matched (see tr21 reference below for
'folding').

Upon a successful match, if C<pe1> is non-C<NULL>,
it will be set to point to the beginning of the I<next> character of C<s1>
beyond what was matched.  Correspondingly for C<pe2> and C<s2>.

For case-insensitiveness, the "casefolding" of Unicode is used
instead of upper/lowercasing both the characters, see
L<https://www.unicode.org/reports/tr21/> (Case Mappings).

=for apidoc Cmnh||FOLDEQ_UTF8_NOMIX_ASCII
=for apidoc Cmnh||FOLDEQ_LOCALE
=for apidoc Cmnh||FOLDEQ_S1_ALREADY_FOLDED
=for apidoc Cmnh||FOLDEQ_S1_FOLDS_SANE
=for apidoc Cmnh||FOLDEQ_S2_ALREADY_FOLDED
=for apidoc Cmnh||FOLDEQ_S2_FOLDS_SANE

=cut */

/* A flags parameter has been added which may change, and hence isn't
 * externally documented.  Currently it is:
 *  0 for as-documented above
 *  FOLDEQ_UTF8_NOMIX_ASCII meaning that if a non-ASCII character folds to an
                            ASCII one, to not match
 *  FOLDEQ_LOCALE	    is set iff the rules from the current underlying
 *	                    locale are to be used.
 *  FOLDEQ_S1_ALREADY_FOLDED  s1 has already been folded before calling this
 *                          routine.  This allows that step to be skipped.
 *                          Currently, this requires s1 to be encoded as UTF-8
 *                          (u1 must be true), which is asserted for.
 *  FOLDEQ_S1_FOLDS_SANE    With either NOMIX_ASCII or LOCALE, no folds may
 *                          cross certain boundaries.  Hence, the caller should
 *                          let this function do the folding instead of
 *                          pre-folding.  This code contains an assertion to
 *                          that effect.  However, if the caller knows what
 *                          it's doing, it can pass this flag to indicate that,
 *                          and the assertion is skipped.
 *  FOLDEQ_S2_ALREADY_FOLDED  Similar to FOLDEQ_S1_ALREADY_FOLDED, but applies
 *                          to s2, and s2 doesn't have to be UTF-8 encoded.
 *                          This introduces an asymmetry to save a few branches
 *                          in a loop.  Currently, this is not a problem, as
 *                          never are both inputs pre-folded.  Simply call this
 *                          function with the pre-folded one as the second
 *                          string.
 *  FOLDEQ_S2_FOLDS_SANE
 */

I32
Perl_foldEQ_utf8_flags(pTHX_ const char *s1, char **pe1, UV l1, bool u1,
                             const char *s2, char **pe2, UV l2, bool u2,
                             U32 flags)
{
    const U8 *p1  = (const U8*)s1; /* Point to current char */
    const U8 *p2  = (const U8*)s2;
    const U8 *g1 = NULL;       /* goal for s1 */
    const U8 *g2 = NULL;
    const U8 *e1 = NULL;       /* Don't scan s1 past this */
    U8 *f1 = NULL;             /* Point to current folded */
    const U8 *e2 = NULL;
    U8 *f2 = NULL;
    STRLEN n1 = 0, n2 = 0;              /* Number of bytes in current char */
    U8 foldbuf1[UTF8_MAXBYTES_CASE+1];
    U8 foldbuf2[UTF8_MAXBYTES_CASE+1];
    U8 flags_for_folder = FOLD_FLAGS_FULL;

    PERL_ARGS_ASSERT_FOLDEQ_UTF8_FLAGS;

    assert( ! (             (flags & (FOLDEQ_UTF8_NOMIX_ASCII | FOLDEQ_LOCALE))
               && ((        (flags &  FOLDEQ_S1_ALREADY_FOLDED)
                        && !(flags &  FOLDEQ_S1_FOLDS_SANE))
                    || (    (flags &  FOLDEQ_S2_ALREADY_FOLDED)
                        && !(flags &  FOLDEQ_S2_FOLDS_SANE)))));
    /* The algorithm is to trial the folds without regard to the flags on
     * the first line of the above assert(), and then see if the result
     * violates them.  This means that the inputs can't be pre-folded to a
     * violating result, hence the assert.  This could be changed, with the
     * addition of extra tests here for the already-folded case, which would
     * slow it down.  That cost is more than any possible gain for when these
     * flags are specified, as the flags indicate /il or /iaa matching which
     * is less common than /iu, and I (khw) also believe that real-world /il
     * and /iaa matches are most likely to involve code points 0-255, and this
     * function only under rare conditions gets called for 0-255. */

    if (flags & FOLDEQ_LOCALE) {
        if (IN_UTF8_CTYPE_LOCALE) {
            if (UNLIKELY(IN_UTF8_TURKIC_LOCALE)) {
                flags_for_folder |= FOLD_FLAGS_LOCALE;
            }
            else {
                flags &= ~FOLDEQ_LOCALE;
            }
        }
        else {
            flags_for_folder |= FOLD_FLAGS_LOCALE;
        }
    }
    if (flags & FOLDEQ_UTF8_NOMIX_ASCII) {
        flags_for_folder |= FOLD_FLAGS_NOMIX_ASCII;
    }

    if (pe1) {
        e1 = *(U8**)pe1;
    }

    if (l1) {
        g1 = (const U8*)s1 + l1;
    }

    if (pe2) {
        e2 = *(U8**)pe2;
    }

    if (l2) {
        g2 = (const U8*)s2 + l2;
    }

    /* Must have at least one goal */
    assert(g1 || g2);

    if (g1) {

        /* Will never match if goal is out-of-bounds */
        assert(! e1  || e1 >= g1);

        /* Here, there isn't an end pointer, or it is beyond the goal.  We
        * only go as far as the goal */
        e1 = g1;
    }
    else {
        assert(e1);    /* Must have an end for looking at s1 */
    }

    /* Same for goal for s2 */
    if (g2) {
        assert(! e2  || e2 >= g2);
        e2 = g2;
    }
    else {
        assert(e2);
    }

    /* If both operands are already folded, we could just do a memEQ on the
     * whole strings at once, but it would be better if the caller realized
     * this and didn't even call us */

    /* Look through both strings, a character at a time */
    while (p1 < e1 && p2 < e2) {

        /* If at the beginning of a new character in s1, get its fold to use
         * and the length of the fold. */
        if (n1 == 0) {
            if (flags & FOLDEQ_S1_ALREADY_FOLDED) {
                f1 = (U8 *) p1;
                assert(u1);
                n1 = UTF8SKIP(f1);
            }
            else {
                if (isASCII(*p1) && ! (flags & FOLDEQ_LOCALE)) {

                    /* We have to forbid mixing ASCII with non-ASCII if the
                     * flags so indicate.  And, we can short circuit having to
                     * call the general functions for this common ASCII case,
                     * all of whose non-locale folds are also ASCII, and hence
                     * UTF-8 invariants, so the UTF8ness of the strings is not
                     * relevant. */
                    if ((flags & FOLDEQ_UTF8_NOMIX_ASCII) && ! isASCII(*p2)) {
                        return 0;
                    }
                    n1 = 1;
                    *foldbuf1 = toFOLD(*p1);
                }
                else if (u1) {
                    _toFOLD_utf8_flags(p1, e1, foldbuf1, &n1, flags_for_folder);
                }
                else {  /* Not UTF-8, get UTF-8 fold */
                    _to_uni_fold_flags(*p1, foldbuf1, &n1, flags_for_folder);
                }
                f1 = foldbuf1;
            }
        }

        if (n2 == 0) {    /* Same for s2 */
            if (flags & FOLDEQ_S2_ALREADY_FOLDED) {

                /* Point to the already-folded character.  But for non-UTF-8
                 * variants, convert to UTF-8 for the algorithm below */
                if (UTF8_IS_INVARIANT(*p2)) {
                    f2 = (U8 *) p2;
                    n2 = 1;
                }
                else if (u2) {
                    f2 = (U8 *) p2;
                    n2 = UTF8SKIP(f2);
                }
                else {
                    foldbuf2[0] = UTF8_EIGHT_BIT_HI(*p2);
                    foldbuf2[1] = UTF8_EIGHT_BIT_LO(*p2);
                    f2 = foldbuf2;
                    n2 = 2;
                }
            }
            else {
                if (isASCII(*p2) && ! (flags & FOLDEQ_LOCALE)) {
                    if ((flags & FOLDEQ_UTF8_NOMIX_ASCII) && ! isASCII(*p1)) {
                        return 0;
                    }
                    n2 = 1;
                    *foldbuf2 = toFOLD(*p2);
                }
                else if (u2) {
                    _toFOLD_utf8_flags(p2, e2, foldbuf2, &n2, flags_for_folder);
                }
                else {
                    _to_uni_fold_flags(*p2, foldbuf2, &n2, flags_for_folder);
                }
                f2 = foldbuf2;
            }
        }

        /* Here f1 and f2 point to the beginning of the strings to compare.
         * These strings are the folds of the next character from each input
         * string, stored in UTF-8. */

        /* While there is more to look for in both folds, see if they
        * continue to match */
        while (n1 && n2) {
            U8 fold_length = UTF8SKIP(f1);
            if (fold_length != UTF8SKIP(f2)
                || (fold_length == 1 && *f1 != *f2) /* Short circuit memNE
                                                       function call for single
                                                       byte */
                || memNE((char*)f1, (char*)f2, fold_length))
            {
                return 0; /* mismatch */
            }

            /* Here, they matched, advance past them */
            n1 -= fold_length;
            f1 += fold_length;
            n2 -= fold_length;
            f2 += fold_length;
        }

        /* When reach the end of any fold, advance the input past it */
        if (n1 == 0) {
            p1 += u1 ? UTF8SKIP(p1) : 1;
        }
        if (n2 == 0) {
            p2 += u2 ? UTF8SKIP(p2) : 1;
        }
    } /* End of loop through both strings */

    /* A match is defined by each scan that specified an explicit length
    * reaching its final goal, and the other not having matched a partial
    * character (which can happen when the fold of a character is more than one
    * character). */
    if (! ((g1 == 0 || p1 == g1) && (g2 == 0 || p2 == g2)) || n1 || n2) {
        return 0;
    }

    /* Successful match.  Set output pointers */
    if (pe1) {
        *pe1 = (char*)p1;
    }
    if (pe2) {
        *pe2 = (char*)p2;
    }
    return 1;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
