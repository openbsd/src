/*    numeric.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *    2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "That only makes eleven (plus one mislaid) and not fourteen,
 *  unless wizards count differently to other people."  --Beorn
 *
 *     [p.115 of _The Hobbit_: "Queer Lodgings"]
 */

/*
=head1 Numeric functions

=cut

This file contains all the stuff needed by perl for manipulating numeric
values, including such things as replacements for the OS's atof() function

*/

#include "EXTERN.h"
#define PERL_IN_NUMERIC_C
#include "perl.h"

U32
Perl_cast_ulong(NV f)
{
  if (f < 0.0)
    return f < I32_MIN ? (U32) I32_MIN : (U32)(I32) f;
  if (f < U32_MAX_P1) {
#if CASTFLAGS & 2
    if (f < U32_MAX_P1_HALF)
      return (U32) f;
    f -= U32_MAX_P1_HALF;
    return ((U32) f) | (1 + (U32_MAX >> 1));
#else
    return (U32) f;
#endif
  }
  return f > 0 ? U32_MAX : 0 /* NaN */;
}

I32
Perl_cast_i32(NV f)
{
  if (f < I32_MAX_P1)
    return f < I32_MIN ? I32_MIN : (I32) f;
  if (f < U32_MAX_P1) {
#if CASTFLAGS & 2
    if (f < U32_MAX_P1_HALF)
      return (I32)(U32) f;
    f -= U32_MAX_P1_HALF;
    return (I32)(((U32) f) | (1 + (U32_MAX >> 1)));
#else
    return (I32)(U32) f;
#endif
  }
  return f > 0 ? (I32)U32_MAX : 0 /* NaN */;
}

IV
Perl_cast_iv(NV f)
{
  if (f < IV_MAX_P1)
    return f < IV_MIN ? IV_MIN : (IV) f;
  if (f < UV_MAX_P1) {
#if CASTFLAGS & 2
    /* For future flexibility allowing for sizeof(UV) >= sizeof(IV)  */
    if (f < UV_MAX_P1_HALF)
      return (IV)(UV) f;
    f -= UV_MAX_P1_HALF;
    return (IV)(((UV) f) | (1 + (UV_MAX >> 1)));
#else
    return (IV)(UV) f;
#endif
  }
  return f > 0 ? (IV)UV_MAX : 0 /* NaN */;
}

UV
Perl_cast_uv(NV f)
{
  if (f < 0.0)
    return f < IV_MIN ? (UV) IV_MIN : (UV)(IV) f;
  if (f < UV_MAX_P1) {
#if CASTFLAGS & 2
    if (f < UV_MAX_P1_HALF)
      return (UV) f;
    f -= UV_MAX_P1_HALF;
    return ((UV) f) | (1 + (UV_MAX >> 1));
#else
    return (UV) f;
#endif
  }
  return f > 0 ? UV_MAX : 0 /* NaN */;
}

/*
=for apidoc grok_bin

converts a string representing a binary number to numeric form.

On entry C<start> and C<*len> give the string to scan, C<*flags> gives
conversion flags, and C<result> should be C<NULL> or a pointer to an NV.
The scan stops at the end of the string, or the first invalid character.
Unless C<PERL_SCAN_SILENT_ILLDIGIT> is set in C<*flags>, encountering an
invalid character will also trigger a warning.
On return C<*len> is set to the length of the scanned string,
and C<*flags> gives output flags.

If the value is <= C<UV_MAX> it is returned as a UV, the output flags are clear,
and nothing is written to C<*result>.  If the value is > C<UV_MAX>, C<grok_bin>
returns C<UV_MAX>, sets C<PERL_SCAN_GREATER_THAN_UV_MAX> in the output flags,
and writes the value to C<*result> (or the value is discarded if C<result>
is NULL).

The binary number may optionally be prefixed with C<"0b"> or C<"b"> unless
C<PERL_SCAN_DISALLOW_PREFIX> is set in C<*flags> on entry.  If
C<PERL_SCAN_ALLOW_UNDERSCORES> is set in C<*flags> then the binary
number may use C<"_"> characters to separate digits.

=cut

Not documented yet because experimental is C<PERL_SCAN_SILENT_NON_PORTABLE
which suppresses any message for non-portable numbers that are still valid
on this platform.
 */

UV
Perl_grok_bin(pTHX_ const char *start, STRLEN *len_p, I32 *flags, NV *result)
{
    const char *s = start;
    STRLEN len = *len_p;
    UV value = 0;
    NV value_nv = 0;

    const UV max_div_2 = UV_MAX / 2;
    const bool allow_underscores = cBOOL(*flags & PERL_SCAN_ALLOW_UNDERSCORES);
    bool overflowed = FALSE;
    char bit;

    PERL_ARGS_ASSERT_GROK_BIN;

    if (!(*flags & PERL_SCAN_DISALLOW_PREFIX)) {
        /* strip off leading b or 0b.
           for compatibility silently suffer "b" and "0b" as valid binary
           numbers. */
        if (len >= 1) {
            if (isALPHA_FOLD_EQ(s[0], 'b')) {
                s++;
                len--;
            }
            else if (len >= 2 && s[0] == '0' && (isALPHA_FOLD_EQ(s[1], 'b'))) {
                s+=2;
                len-=2;
            }
        }
    }

    for (; len-- && (bit = *s); s++) {
        if (bit == '0' || bit == '1') {
            /* Write it in this wonky order with a goto to attempt to get the
               compiler to make the common case integer-only loop pretty tight.
               With gcc seems to be much straighter code than old scan_bin.  */
          redo:
            if (!overflowed) {
                if (value <= max_div_2) {
                    value = (value << 1) | (bit - '0');
                    continue;
                }
                /* Bah. We're just overflowed.  */
		/* diag_listed_as: Integer overflow in %s number */
		Perl_ck_warner_d(aTHX_ packWARN(WARN_OVERFLOW),
				 "Integer overflow in binary number");
                overflowed = TRUE;
                value_nv = (NV) value;
            }
            value_nv *= 2.0;
	    /* If an NV has not enough bits in its mantissa to
	     * represent a UV this summing of small low-order numbers
	     * is a waste of time (because the NV cannot preserve
	     * the low-order bits anyway): we could just remember when
	     * did we overflow and in the end just multiply value_nv by the
	     * right amount. */
            value_nv += (NV)(bit - '0');
            continue;
        }
        if (bit == '_' && len && allow_underscores && (bit = s[1])
            && (bit == '0' || bit == '1'))
	    {
		--len;
		++s;
                goto redo;
	    }
        if (!(*flags & PERL_SCAN_SILENT_ILLDIGIT))
            Perl_ck_warner(aTHX_ packWARN(WARN_DIGIT),
			   "Illegal binary digit '%c' ignored", *s);
        break;
    }
    
    if (   ( overflowed && value_nv > 4294967295.0)
#if UVSIZE > 4
	|| (!overflowed && value > 0xffffffff
	    && ! (*flags & PERL_SCAN_SILENT_NON_PORTABLE))
#endif
	) {
	Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
		       "Binary number > 0b11111111111111111111111111111111 non-portable");
    }
    *len_p = s - start;
    if (!overflowed) {
        *flags = 0;
        return value;
    }
    *flags = PERL_SCAN_GREATER_THAN_UV_MAX;
    if (result)
        *result = value_nv;
    return UV_MAX;
}

/*
=for apidoc grok_hex

converts a string representing a hex number to numeric form.

On entry C<start> and C<*len_p> give the string to scan, C<*flags> gives
conversion flags, and C<result> should be C<NULL> or a pointer to an NV.
The scan stops at the end of the string, or the first invalid character.
Unless C<PERL_SCAN_SILENT_ILLDIGIT> is set in C<*flags>, encountering an
invalid character will also trigger a warning.
On return C<*len> is set to the length of the scanned string,
and C<*flags> gives output flags.

If the value is <= C<UV_MAX> it is returned as a UV, the output flags are clear,
and nothing is written to C<*result>.  If the value is > C<UV_MAX>, C<grok_hex>
returns C<UV_MAX>, sets C<PERL_SCAN_GREATER_THAN_UV_MAX> in the output flags,
and writes the value to C<*result> (or the value is discarded if C<result>
is C<NULL>).

The hex number may optionally be prefixed with C<"0x"> or C<"x"> unless
C<PERL_SCAN_DISALLOW_PREFIX> is set in C<*flags> on entry.  If
C<PERL_SCAN_ALLOW_UNDERSCORES> is set in C<*flags> then the hex
number may use C<"_"> characters to separate digits.

=cut

Not documented yet because experimental is C<PERL_SCAN_SILENT_NON_PORTABLE
which suppresses any message for non-portable numbers, but which are valid
on this platform.
 */

UV
Perl_grok_hex(pTHX_ const char *start, STRLEN *len_p, I32 *flags, NV *result)
{
    const char *s = start;
    STRLEN len = *len_p;
    UV value = 0;
    NV value_nv = 0;
    const UV max_div_16 = UV_MAX / 16;
    const bool allow_underscores = cBOOL(*flags & PERL_SCAN_ALLOW_UNDERSCORES);
    bool overflowed = FALSE;

    PERL_ARGS_ASSERT_GROK_HEX;

    if (!(*flags & PERL_SCAN_DISALLOW_PREFIX)) {
        /* strip off leading x or 0x.
           for compatibility silently suffer "x" and "0x" as valid hex numbers.
        */
        if (len >= 1) {
            if (isALPHA_FOLD_EQ(s[0], 'x')) {
                s++;
                len--;
            }
            else if (len >= 2 && s[0] == '0' && (isALPHA_FOLD_EQ(s[1], 'x'))) {
                s+=2;
                len-=2;
            }
        }
    }

    for (; len-- && *s; s++) {
        if (isXDIGIT(*s)) {
            /* Write it in this wonky order with a goto to attempt to get the
               compiler to make the common case integer-only loop pretty tight.
               With gcc seems to be much straighter code than old scan_hex.  */
          redo:
            if (!overflowed) {
                if (value <= max_div_16) {
                    value = (value << 4) | XDIGIT_VALUE(*s);
                    continue;
                }
                /* Bah. We're just overflowed.  */
		/* diag_listed_as: Integer overflow in %s number */
		Perl_ck_warner_d(aTHX_ packWARN(WARN_OVERFLOW),
				 "Integer overflow in hexadecimal number");
                overflowed = TRUE;
                value_nv = (NV) value;
            }
            value_nv *= 16.0;
	    /* If an NV has not enough bits in its mantissa to
	     * represent a UV this summing of small low-order numbers
	     * is a waste of time (because the NV cannot preserve
	     * the low-order bits anyway): we could just remember when
	     * did we overflow and in the end just multiply value_nv by the
	     * right amount of 16-tuples. */
            value_nv += (NV) XDIGIT_VALUE(*s);
            continue;
        }
        if (*s == '_' && len && allow_underscores && s[1]
		&& isXDIGIT(s[1]))
	    {
		--len;
		++s;
                goto redo;
	    }
        if (!(*flags & PERL_SCAN_SILENT_ILLDIGIT))
            Perl_ck_warner(aTHX_ packWARN(WARN_DIGIT),
                        "Illegal hexadecimal digit '%c' ignored", *s);
        break;
    }
    
    if (   ( overflowed && value_nv > 4294967295.0)
#if UVSIZE > 4
	|| (!overflowed && value > 0xffffffff
	    && ! (*flags & PERL_SCAN_SILENT_NON_PORTABLE))
#endif
	) {
	Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
		       "Hexadecimal number > 0xffffffff non-portable");
    }
    *len_p = s - start;
    if (!overflowed) {
        *flags = 0;
        return value;
    }
    *flags = PERL_SCAN_GREATER_THAN_UV_MAX;
    if (result)
        *result = value_nv;
    return UV_MAX;
}

/*
=for apidoc grok_oct

converts a string representing an octal number to numeric form.

On entry C<start> and C<*len> give the string to scan, C<*flags> gives
conversion flags, and C<result> should be C<NULL> or a pointer to an NV.
The scan stops at the end of the string, or the first invalid character.
Unless C<PERL_SCAN_SILENT_ILLDIGIT> is set in C<*flags>, encountering an
8 or 9 will also trigger a warning.
On return C<*len> is set to the length of the scanned string,
and C<*flags> gives output flags.

If the value is <= C<UV_MAX> it is returned as a UV, the output flags are clear,
and nothing is written to C<*result>.  If the value is > C<UV_MAX>, C<grok_oct>
returns C<UV_MAX>, sets C<PERL_SCAN_GREATER_THAN_UV_MAX> in the output flags,
and writes the value to C<*result> (or the value is discarded if C<result>
is C<NULL>).

If C<PERL_SCAN_ALLOW_UNDERSCORES> is set in C<*flags> then the octal
number may use C<"_"> characters to separate digits.

=cut

Not documented yet because experimental is C<PERL_SCAN_SILENT_NON_PORTABLE>
which suppresses any message for non-portable numbers, but which are valid
on this platform.
 */

UV
Perl_grok_oct(pTHX_ const char *start, STRLEN *len_p, I32 *flags, NV *result)
{
    const char *s = start;
    STRLEN len = *len_p;
    UV value = 0;
    NV value_nv = 0;
    const UV max_div_8 = UV_MAX / 8;
    const bool allow_underscores = cBOOL(*flags & PERL_SCAN_ALLOW_UNDERSCORES);
    bool overflowed = FALSE;

    PERL_ARGS_ASSERT_GROK_OCT;

    for (; len-- && *s; s++) {
        if (isOCTAL(*s)) {
            /* Write it in this wonky order with a goto to attempt to get the
               compiler to make the common case integer-only loop pretty tight.
            */
          redo:
            if (!overflowed) {
                if (value <= max_div_8) {
                    value = (value << 3) | OCTAL_VALUE(*s);
                    continue;
                }
                /* Bah. We're just overflowed.  */
		/* diag_listed_as: Integer overflow in %s number */
		Perl_ck_warner_d(aTHX_ packWARN(WARN_OVERFLOW),
			       "Integer overflow in octal number");
                overflowed = TRUE;
                value_nv = (NV) value;
            }
            value_nv *= 8.0;
	    /* If an NV has not enough bits in its mantissa to
	     * represent a UV this summing of small low-order numbers
	     * is a waste of time (because the NV cannot preserve
	     * the low-order bits anyway): we could just remember when
	     * did we overflow and in the end just multiply value_nv by the
	     * right amount of 8-tuples. */
            value_nv += (NV) OCTAL_VALUE(*s);
            continue;
        }
        if (*s == '_' && len && allow_underscores && isOCTAL(s[1])) {
            --len;
            ++s;
            goto redo;
        }
        /* Allow \octal to work the DWIM way (that is, stop scanning
         * as soon as non-octal characters are seen, complain only if
         * someone seems to want to use the digits eight and nine.  Since we
         * know it is not octal, then if isDIGIT, must be an 8 or 9). */
        if (isDIGIT(*s)) {
            if (!(*flags & PERL_SCAN_SILENT_ILLDIGIT))
                Perl_ck_warner(aTHX_ packWARN(WARN_DIGIT),
			       "Illegal octal digit '%c' ignored", *s);
        }
        break;
    }
    
    if (   ( overflowed && value_nv > 4294967295.0)
#if UVSIZE > 4
	|| (!overflowed && value > 0xffffffff
	    && ! (*flags & PERL_SCAN_SILENT_NON_PORTABLE))
#endif
	) {
	Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
		       "Octal number > 037777777777 non-portable");
    }
    *len_p = s - start;
    if (!overflowed) {
        *flags = 0;
        return value;
    }
    *flags = PERL_SCAN_GREATER_THAN_UV_MAX;
    if (result)
        *result = value_nv;
    return UV_MAX;
}

/*
=for apidoc scan_bin

For backwards compatibility.  Use C<grok_bin> instead.

=for apidoc scan_hex

For backwards compatibility.  Use C<grok_hex> instead.

=for apidoc scan_oct

For backwards compatibility.  Use C<grok_oct> instead.

=cut
 */

NV
Perl_scan_bin(pTHX_ const char *start, STRLEN len, STRLEN *retlen)
{
    NV rnv;
    I32 flags = *retlen ? PERL_SCAN_ALLOW_UNDERSCORES : 0;
    const UV ruv = grok_bin (start, &len, &flags, &rnv);

    PERL_ARGS_ASSERT_SCAN_BIN;

    *retlen = len;
    return (flags & PERL_SCAN_GREATER_THAN_UV_MAX) ? rnv : (NV)ruv;
}

NV
Perl_scan_oct(pTHX_ const char *start, STRLEN len, STRLEN *retlen)
{
    NV rnv;
    I32 flags = *retlen ? PERL_SCAN_ALLOW_UNDERSCORES : 0;
    const UV ruv = grok_oct (start, &len, &flags, &rnv);

    PERL_ARGS_ASSERT_SCAN_OCT;

    *retlen = len;
    return (flags & PERL_SCAN_GREATER_THAN_UV_MAX) ? rnv : (NV)ruv;
}

NV
Perl_scan_hex(pTHX_ const char *start, STRLEN len, STRLEN *retlen)
{
    NV rnv;
    I32 flags = *retlen ? PERL_SCAN_ALLOW_UNDERSCORES : 0;
    const UV ruv = grok_hex (start, &len, &flags, &rnv);

    PERL_ARGS_ASSERT_SCAN_HEX;

    *retlen = len;
    return (flags & PERL_SCAN_GREATER_THAN_UV_MAX) ? rnv : (NV)ruv;
}

/*
=for apidoc grok_numeric_radix

Scan and skip for a numeric decimal separator (radix).

=cut
 */
bool
Perl_grok_numeric_radix(pTHX_ const char **sp, const char *send)
{
#ifdef USE_LOCALE_NUMERIC
    PERL_ARGS_ASSERT_GROK_NUMERIC_RADIX;

    if (IN_LC(LC_NUMERIC)) {
        DECLARATION_FOR_LC_NUMERIC_MANIPULATION;
        STORE_LC_NUMERIC_SET_TO_NEEDED();
        if (PL_numeric_radix_sv) {
            STRLEN len;
            const char * const radix = SvPV(PL_numeric_radix_sv, len);
            if (*sp + len <= send && memEQ(*sp, radix, len)) {
                *sp += len;
                RESTORE_LC_NUMERIC();
                return TRUE;
            }
        }
        RESTORE_LC_NUMERIC();
    }
    /* always try "." if numeric radix didn't match because
     * we may have data from different locales mixed */
#endif

    PERL_ARGS_ASSERT_GROK_NUMERIC_RADIX;

    if (*sp < send && **sp == '.') {
        ++*sp;
        return TRUE;
    }
    return FALSE;
}

/*
=for apidoc grok_infnan

Helper for C<grok_number()>, accepts various ways of spelling "infinity"
or "not a number", and returns one of the following flag combinations:

  IS_NUMBER_INFINITE
  IS_NUMBER_NAN
  IS_NUMBER_INFINITE | IS_NUMBER_NEG
  IS_NUMBER_NAN | IS_NUMBER_NEG
  0

possibly |-ed with C<IS_NUMBER_TRAILING>.

If an infinity or a not-a-number is recognized, C<*sp> will point to
one byte past the end of the recognized string.  If the recognition fails,
zero is returned, and C<*sp> will not move.

=cut
*/

int
Perl_grok_infnan(pTHX_ const char** sp, const char* send)
{
    const char* s = *sp;
    int flags = 0;
    bool odh = FALSE; /* one-dot-hash: 1.#INF */

    PERL_ARGS_ASSERT_GROK_INFNAN;

    if (*s == '+') {
        s++; if (s == send) return 0;
    }
    else if (*s == '-') {
        flags |= IS_NUMBER_NEG; /* Yes, -NaN happens. Incorrect but happens. */
        s++; if (s == send) return 0;
    }

    if (*s == '1') {
        /* Visual C: 1.#SNAN, -1.#QNAN, 1#INF, 1.#IND (maybe also 1.#NAN)
         * Let's keep the dot optional. */
        s++; if (s == send) return 0;
        if (*s == '.') {
            s++; if (s == send) return 0;
        }
        if (*s == '#') {
            s++; if (s == send) return 0;
        } else
            return 0;
        odh = TRUE;
    }

    if (isALPHA_FOLD_EQ(*s, 'I')) {
        /* INF or IND (1.#IND is "indeterminate", a certain type of NAN) */

        s++; if (s == send || isALPHA_FOLD_NE(*s, 'N')) return 0;
        s++; if (s == send) return 0;
        if (isALPHA_FOLD_EQ(*s, 'F')) {
            s++;
            if (s < send && (isALPHA_FOLD_EQ(*s, 'I'))) {
                int fail =
                    flags | IS_NUMBER_INFINITY | IS_NUMBER_NOT_INT | IS_NUMBER_TRAILING;
                s++; if (s == send || isALPHA_FOLD_NE(*s, 'N')) return fail;
                s++; if (s == send || isALPHA_FOLD_NE(*s, 'I')) return fail;
                s++; if (s == send || isALPHA_FOLD_NE(*s, 'T')) return fail;
                s++; if (s == send || isALPHA_FOLD_NE(*s, 'Y')) return fail;
                s++;
            } else if (odh) {
                while (*s == '0') { /* 1.#INF00 */
                    s++;
                }
            }
            while (s < send && isSPACE(*s))
                s++;
            if (s < send && *s) {
                flags |= IS_NUMBER_TRAILING;
            }
            flags |= IS_NUMBER_INFINITY | IS_NUMBER_NOT_INT;
        }
        else if (isALPHA_FOLD_EQ(*s, 'D') && odh) { /* 1.#IND */
            s++;
            flags |= IS_NUMBER_NAN | IS_NUMBER_NOT_INT;
            while (*s == '0') { /* 1.#IND00 */
                s++;
            }
            if (*s) {
                flags |= IS_NUMBER_TRAILING;
            }
        } else
            return 0;
    }
    else {
        /* Maybe NAN of some sort */

        if (isALPHA_FOLD_EQ(*s, 'S') || isALPHA_FOLD_EQ(*s, 'Q')) {
            /* snan, qNaN */
            /* XXX do something with the snan/qnan difference */
            s++; if (s == send) return 0;
        }

        if (isALPHA_FOLD_EQ(*s, 'N')) {
            s++; if (s == send || isALPHA_FOLD_NE(*s, 'A')) return 0;
            s++; if (s == send || isALPHA_FOLD_NE(*s, 'N')) return 0;
            s++;

            flags |= IS_NUMBER_NAN | IS_NUMBER_NOT_INT;

            /* NaN can be followed by various stuff (NaNQ, NaNS), but
             * there are also multiple different NaN values, and some
             * implementations output the "payload" values,
             * e.g. NaN123, NAN(abc), while some legacy implementations
             * have weird stuff like NaN%. */
            if (isALPHA_FOLD_EQ(*s, 'q') ||
                isALPHA_FOLD_EQ(*s, 's')) {
                /* "nanq" or "nans" are ok, though generating
                 * these portably is tricky. */
                s++;
            }
            if (*s == '(') {
                /* C99 style "nan(123)" or Perlish equivalent "nan($uv)". */
                const char *t;
                s++;
                if (s == send) {
                    return flags | IS_NUMBER_TRAILING;
                }
                t = s + 1;
                while (t < send && *t && *t != ')') {
                    t++;
                }
                if (t == send) {
                    return flags | IS_NUMBER_TRAILING;
                }
                if (*t == ')') {
                    int nantype;
                    UV nanval;
                    if (s[0] == '0' && s + 2 < t &&
                        isALPHA_FOLD_EQ(s[1], 'x') &&
                        isXDIGIT(s[2])) {
                        STRLEN len = t - s;
                        I32 flags = PERL_SCAN_ALLOW_UNDERSCORES;
                        nanval = grok_hex(s, &len, &flags, NULL);
                        if ((flags & PERL_SCAN_GREATER_THAN_UV_MAX)) {
                            nantype = 0;
                        } else {
                            nantype = IS_NUMBER_IN_UV;
                        }
                        s += len;
                    } else if (s[0] == '0' && s + 2 < t &&
                               isALPHA_FOLD_EQ(s[1], 'b') &&
                               (s[2] == '0' || s[2] == '1')) {
                        STRLEN len = t - s;
                        I32 flags = PERL_SCAN_ALLOW_UNDERSCORES;
                        nanval = grok_bin(s, &len, &flags, NULL);
                        if ((flags & PERL_SCAN_GREATER_THAN_UV_MAX)) {
                            nantype = 0;
                        } else {
                            nantype = IS_NUMBER_IN_UV;
                        }
                        s += len;
                    } else {
                        const char *u;
                        nantype =
                            grok_number_flags(s, t - s, &nanval,
                                              PERL_SCAN_TRAILING |
                                              PERL_SCAN_ALLOW_UNDERSCORES);
                        /* Unfortunately grok_number_flags() doesn't
                         * tell how far we got and the ')' will always
                         * be "trailing", so we need to double-check
                         * whether we had something dubious. */
                        for (u = s; u < t; u++) {
                            if (!isDIGIT(*u)) {
                                flags |= IS_NUMBER_TRAILING;
                                break;
                            }
                        }
                        s = u;
                    }

                    /* XXX Doesn't do octal: nan("0123").
                     * Probably not a big loss. */

                    if ((nantype & IS_NUMBER_NOT_INT) ||
                        !(nantype && IS_NUMBER_IN_UV)) {
                        /* XXX the nanval is currently unused, that is,
                         * not inserted as the NaN payload of the NV.
                         * But the above code already parses the C99
                         * nan(...)  format.  See below, and see also
                         * the nan() in POSIX.xs.
                         *
                         * Certain configuration combinations where
                         * NVSIZE is greater than UVSIZE mean that
                         * a single UV cannot contain all the possible
                         * NaN payload bits.  There would need to be
                         * some more generic syntax than "nan($uv)".
                         *
                         * Issues to keep in mind:
                         *
                         * (1) In most common cases there would
                         * not be an integral number of bytes that
                         * could be set, only a certain number of bits.
                         * For example for the common case of
                         * NVSIZE == UVSIZE == 8 there is room for 52
                         * bits in the payload, but the most significant
                         * bit is commonly reserved for the
                         * signaling/quiet bit, leaving 51 bits.
                         * Furthermore, the C99 nan() is supposed
                         * to generate quiet NaNs, so it is doubtful
                         * whether it should be able to generate
                         * signaling NaNs.  For the x86 80-bit doubles
                         * (if building a long double Perl) there would
                         * be 62 bits (s/q bit being the 63rd).
                         *
                         * (2) Endianness of the payload bits. If the
                         * payload is specified as an UV, the low-order
                         * bits of the UV are naturally little-endianed
                         * (rightmost) bits of the payload.  The endianness
                         * of UVs and NVs can be different. */
                        return 0;
                    }
                    if (s < t) {
                        flags |= IS_NUMBER_TRAILING;
                    }
                } else {
                    /* Looked like nan(...), but no close paren. */
                    flags |= IS_NUMBER_TRAILING;
                }
            } else {
                while (s < send && isSPACE(*s))
                    s++;
                if (s < send && *s) {
                    /* Note that we here implicitly accept (parse as
                     * "nan", but with warnings) also any other weird
                     * trailing stuff for "nan".  In the above we just
                     * check that if we got the C99-style "nan(...)",
                     * the "..."  looks sane.
                     * If in future we accept more ways of specifying
                     * the nan payload, the accepting would happen around
                     * here. */
                    flags |= IS_NUMBER_TRAILING;
                }
            }
            s = send;
        }
        else
            return 0;
    }

    while (s < send && isSPACE(*s))
        s++;

    *sp = s;
    return flags;
}

/*
=for apidoc grok_number_flags

Recognise (or not) a number.  The type of the number is returned
(0 if unrecognised), otherwise it is a bit-ORed combination of
C<IS_NUMBER_IN_UV>, C<IS_NUMBER_GREATER_THAN_UV_MAX>, C<IS_NUMBER_NOT_INT>,
C<IS_NUMBER_NEG>, C<IS_NUMBER_INFINITY>, C<IS_NUMBER_NAN> (defined in perl.h).

If the value of the number can fit in a UV, it is returned in C<*valuep>.
C<IS_NUMBER_IN_UV> will be set to indicate that C<*valuep> is valid, C<IS_NUMBER_IN_UV>
will never be set unless C<*valuep> is valid, but C<*valuep> may have been assigned
to during processing even though C<IS_NUMBER_IN_UV> is not set on return.
If C<valuep> is C<NULL>, C<IS_NUMBER_IN_UV> will be set for the same cases as when
C<valuep> is non-C<NULL>, but no actual assignment (or SEGV) will occur.

C<IS_NUMBER_NOT_INT> will be set with C<IS_NUMBER_IN_UV> if trailing decimals were
seen (in which case C<*valuep> gives the true value truncated to an integer), and
C<IS_NUMBER_NEG> if the number is negative (in which case C<*valuep> holds the
absolute value).  C<IS_NUMBER_IN_UV> is not set if e notation was used or the
number is larger than a UV.

C<flags> allows only C<PERL_SCAN_TRAILING>, which allows for trailing
non-numeric text on an otherwise successful I<grok>, setting
C<IS_NUMBER_TRAILING> on the result.

=for apidoc grok_number

Identical to C<grok_number_flags()> with C<flags> set to zero.

=cut
 */
int
Perl_grok_number(pTHX_ const char *pv, STRLEN len, UV *valuep)
{
    PERL_ARGS_ASSERT_GROK_NUMBER;

    return grok_number_flags(pv, len, valuep, 0);
}

static const UV uv_max_div_10 = UV_MAX / 10;
static const U8 uv_max_mod_10 = UV_MAX % 10;

int
Perl_grok_number_flags(pTHX_ const char *pv, STRLEN len, UV *valuep, U32 flags)
{
  const char *s = pv;
  const char * const send = pv + len;
  const char *d;
  int numtype = 0;

  PERL_ARGS_ASSERT_GROK_NUMBER_FLAGS;

  while (s < send && isSPACE(*s))
    s++;
  if (s == send) {
    return 0;
  } else if (*s == '-') {
    s++;
    numtype = IS_NUMBER_NEG;
  }
  else if (*s == '+')
    s++;

  if (s == send)
    return 0;

  /* The first digit (after optional sign): note that might
   * also point to "infinity" or "nan", or "1.#INF". */
  d = s;

  /* next must be digit or the radix separator or beginning of infinity/nan */
  if (isDIGIT(*s)) {
    /* UVs are at least 32 bits, so the first 9 decimal digits cannot
       overflow.  */
    UV value = *s - '0';
    /* This construction seems to be more optimiser friendly.
       (without it gcc does the isDIGIT test and the *s - '0' separately)
       With it gcc on arm is managing 6 instructions (6 cycles) per digit.
       In theory the optimiser could deduce how far to unroll the loop
       before checking for overflow.  */
    if (++s < send) {
      int digit = *s - '0';
      if (digit >= 0 && digit <= 9) {
        value = value * 10 + digit;
        if (++s < send) {
          digit = *s - '0';
          if (digit >= 0 && digit <= 9) {
            value = value * 10 + digit;
            if (++s < send) {
              digit = *s - '0';
              if (digit >= 0 && digit <= 9) {
                value = value * 10 + digit;
		if (++s < send) {
                  digit = *s - '0';
                  if (digit >= 0 && digit <= 9) {
                    value = value * 10 + digit;
                    if (++s < send) {
                      digit = *s - '0';
                      if (digit >= 0 && digit <= 9) {
                        value = value * 10 + digit;
                        if (++s < send) {
                          digit = *s - '0';
                          if (digit >= 0 && digit <= 9) {
                            value = value * 10 + digit;
                            if (++s < send) {
                              digit = *s - '0';
                              if (digit >= 0 && digit <= 9) {
                                value = value * 10 + digit;
                                if (++s < send) {
                                  digit = *s - '0';
                                  if (digit >= 0 && digit <= 9) {
                                    value = value * 10 + digit;
                                    if (++s < send) {
                                      /* Now got 9 digits, so need to check
                                         each time for overflow.  */
                                      digit = *s - '0';
                                      while (digit >= 0 && digit <= 9
                                             && (value < uv_max_div_10
                                                 || (value == uv_max_div_10
                                                     && digit <= uv_max_mod_10))) {
                                        value = value * 10 + digit;
                                        if (++s < send)
                                          digit = *s - '0';
                                        else
                                          break;
                                      }
                                      if (digit >= 0 && digit <= 9
                                          && (s < send)) {
                                        /* value overflowed.
                                           skip the remaining digits, don't
                                           worry about setting *valuep.  */
                                        do {
                                          s++;
                                        } while (s < send && isDIGIT(*s));
                                        numtype |=
                                          IS_NUMBER_GREATER_THAN_UV_MAX;
                                        goto skip_value;
                                      }
                                    }
                                  }
				}
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
	}
      }
    }
    numtype |= IS_NUMBER_IN_UV;
    if (valuep)
      *valuep = value;

  skip_value:
    if (GROK_NUMERIC_RADIX(&s, send)) {
      numtype |= IS_NUMBER_NOT_INT;
      while (s < send && isDIGIT(*s))  /* optional digits after the radix */
        s++;
    }
  }
  else if (GROK_NUMERIC_RADIX(&s, send)) {
    numtype |= IS_NUMBER_NOT_INT | IS_NUMBER_IN_UV; /* valuep assigned below */
    /* no digits before the radix means we need digits after it */
    if (s < send && isDIGIT(*s)) {
      do {
        s++;
      } while (s < send && isDIGIT(*s));
      if (valuep) {
        /* integer approximation is valid - it's 0.  */
        *valuep = 0;
      }
    }
    else
        return 0;
  }

  if (s > d && s < send) {
    /* we can have an optional exponent part */
    if (isALPHA_FOLD_EQ(*s, 'e')) {
      s++;
      if (s < send && (*s == '-' || *s == '+'))
        s++;
      if (s < send && isDIGIT(*s)) {
        do {
          s++;
        } while (s < send && isDIGIT(*s));
      }
      else if (flags & PERL_SCAN_TRAILING)
        return numtype | IS_NUMBER_TRAILING;
      else
        return 0;

      /* The only flag we keep is sign.  Blow away any "it's UV"  */
      numtype &= IS_NUMBER_NEG;
      numtype |= IS_NUMBER_NOT_INT;
    }
  }
  while (s < send && isSPACE(*s))
    s++;
  if (s >= send)
    return numtype;
  if (len == 10 && memEQ(pv, "0 but true", 10)) {
    if (valuep)
      *valuep = 0;
    return IS_NUMBER_IN_UV;
  }
  /* We could be e.g. at "Inf" or "NaN", or at the "#" of "1.#INF". */
  if ((s + 2 < send) && strchr("inqs#", toFOLD(*s))) {
      /* Really detect inf/nan. Start at d, not s, since the above
       * code might have already consumed the "1." or "1". */
      int infnan = Perl_grok_infnan(aTHX_ &d, send);
      if ((infnan & IS_NUMBER_INFINITY)) {
          return (numtype | infnan); /* Keep sign for infinity. */
      }
      else if ((infnan & IS_NUMBER_NAN)) {
          return (numtype | infnan) & ~IS_NUMBER_NEG; /* Clear sign for nan. */
      }
  }
  else if (flags & PERL_SCAN_TRAILING) {
    return numtype | IS_NUMBER_TRAILING;
  }

  return 0;
}

/*
grok_atoUV

grok_atoUV parses a C-style zero-byte terminated string, looking for
a decimal unsigned integer.

Returns the unsigned integer, if a valid value can be parsed
from the beginning of the string.

Accepts only the decimal digits '0'..'9'.

As opposed to atoi or strtol, grok_atoUV does NOT allow optional
leading whitespace, or negative inputs.  If such features are
required, the calling code needs to explicitly implement those.

Returns true if a valid value could be parsed. In that case, valptr
is set to the parsed value, and endptr (if provided) is set to point
to the character after the last digit.

Returns false otherwise. This can happen if a) there is a leading zero
followed by another digit; b) the digits would overflow a UV; or c)
there are trailing non-digits AND endptr is not provided.

Background: atoi has severe problems with illegal inputs, it cannot be
used for incremental parsing, and therefore should be avoided
atoi and strtol are also affected by locale settings, which can also be
seen as a bug (global state controlled by user environment).

*/

bool
Perl_grok_atoUV(const char *pv, UV *valptr, const char** endptr)
{
    const char* s = pv;
    const char** eptr;
    const char* end2; /* Used in case endptr is NULL. */
    UV val = 0; /* The parsed value. */

    PERL_ARGS_ASSERT_GROK_ATOUV;

    eptr = endptr ? endptr : &end2;
    if (isDIGIT(*s)) {
        /* Single-digit inputs are quite common. */
        val = *s++ - '0';
        if (isDIGIT(*s)) {
            /* Fail on extra leading zeros. */
            if (val == 0)
                return FALSE;
            while (isDIGIT(*s)) {
                /* This could be unrolled like in grok_number(), but
                 * the expected uses of this are not speed-needy, and
                 * unlikely to need full 64-bitness. */
                U8 digit = *s++ - '0';
                if (val < uv_max_div_10 ||
                    (val == uv_max_div_10 && digit <= uv_max_mod_10)) {
                    val = val * 10 + digit;
                } else {
                    return FALSE;
                }
            }
        }
    }
    if (s == pv)
        return FALSE;
    if (endptr == NULL && *s)
        return FALSE; /* If endptr is NULL, no trailing non-digits allowed. */
    *eptr = s;
    *valptr = val;
    return TRUE;
}

#ifndef USE_QUADMATH
STATIC NV
S_mulexp10(NV value, I32 exponent)
{
    NV result = 1.0;
    NV power = 10.0;
    bool negative = 0;
    I32 bit;

    if (exponent == 0)
	return value;
    if (value == 0)
	return (NV)0;

    /* On OpenVMS VAX we by default use the D_FLOAT double format,
     * and that format does not have *easy* capabilities [1] for
     * overflowing doubles 'silently' as IEEE fp does.  We also need 
     * to support G_FLOAT on both VAX and Alpha, and though the exponent 
     * range is much larger than D_FLOAT it still doesn't do silent 
     * overflow.  Therefore we need to detect early whether we would 
     * overflow (this is the behaviour of the native string-to-float 
     * conversion routines, and therefore of native applications, too).
     *
     * [1] Trying to establish a condition handler to trap floating point
     *     exceptions is not a good idea. */

    /* In UNICOS and in certain Cray models (such as T90) there is no
     * IEEE fp, and no way at all from C to catch fp overflows gracefully.
     * There is something you can do if you are willing to use some
     * inline assembler: the instruction is called DFI-- but that will
     * disable *all* floating point interrupts, a little bit too large
     * a hammer.  Therefore we need to catch potential overflows before
     * it's too late. */

#if ((defined(VMS) && !defined(_IEEE_FP)) || defined(_UNICOS)) && defined(NV_MAX_10_EXP)
    STMT_START {
	const NV exp_v = log10(value);
	if (exponent >= NV_MAX_10_EXP || exponent + exp_v >= NV_MAX_10_EXP)
	    return NV_MAX;
	if (exponent < 0) {
	    if (-(exponent + exp_v) >= NV_MAX_10_EXP)
		return 0.0;
	    while (-exponent >= NV_MAX_10_EXP) {
		/* combination does not overflow, but 10^(-exponent) does */
		value /= 10;
		++exponent;
	    }
	}
    } STMT_END;
#endif

    if (exponent < 0) {
	negative = 1;
	exponent = -exponent;
#ifdef NV_MAX_10_EXP
        /* for something like 1234 x 10^-309, the action of calculating
         * the intermediate value 10^309 then returning 1234 / (10^309)
         * will fail, since 10^309 becomes infinity. In this case try to
         * refactor it as 123 / (10^308) etc.
         */
        while (value && exponent > NV_MAX_10_EXP) {
            exponent--;
            value /= 10;
        }
        if (value == 0.0)
            return value;
#endif
    }
#if defined(__osf__)
    /* Even with cc -ieee + ieee_set_fp_control(IEEE_TRAP_ENABLE_INV)
     * Tru64 fp behavior on inf/nan is somewhat broken. Another way
     * to do this would be ieee_set_fp_control(IEEE_TRAP_ENABLE_OVF)
     * but that breaks another set of infnan.t tests. */
#  define FP_OVERFLOWS_TO_ZERO
#endif
    for (bit = 1; exponent; bit <<= 1) {
	if (exponent & bit) {
	    exponent ^= bit;
	    result *= power;
#ifdef FP_OVERFLOWS_TO_ZERO
            if (result == 0)
                return value < 0 ? -NV_INF : NV_INF;
#endif
	    /* Floating point exceptions are supposed to be turned off,
	     *  but if we're obviously done, don't risk another iteration.  
	     */
	     if (exponent == 0) break;
	}
	power *= power;
    }
    return negative ? value / result : value * result;
}
#endif /* #ifndef USE_QUADMATH */

NV
Perl_my_atof(pTHX_ const char* s)
{
    NV x = 0.0;
#ifdef USE_QUADMATH
    Perl_my_atof2(aTHX_ s, &x);
    return x;
#else
#  ifdef USE_LOCALE_NUMERIC
    PERL_ARGS_ASSERT_MY_ATOF;

    {
        DECLARATION_FOR_LC_NUMERIC_MANIPULATION;
        STORE_LC_NUMERIC_SET_TO_NEEDED();
        if (PL_numeric_radix_sv && IN_LC(LC_NUMERIC)) {
            const char *standard = NULL, *local = NULL;
            bool use_standard_radix;

            /* Look through the string for the first thing that looks like a
             * decimal point: either the value in the current locale or the
             * standard fallback of '.'. The one which appears earliest in the
             * input string is the one that we should have atof look for. Note
             * that we have to determine this beforehand because on some
             * systems, Perl_atof2 is just a wrapper around the system's atof.
             * */
            standard = strchr(s, '.');
            local = strstr(s, SvPV_nolen(PL_numeric_radix_sv));

            use_standard_radix = standard && (!local || standard < local);

            if (use_standard_radix)
                SET_NUMERIC_STANDARD();

            Perl_atof2(s, x);

            if (use_standard_radix)
                SET_NUMERIC_UNDERLYING();
        }
        else
            Perl_atof2(s, x);
        RESTORE_LC_NUMERIC();
    }
#  else
    Perl_atof2(s, x);
#  endif
#endif
    return x;
}


#ifdef USING_MSVC6
#  pragma warning(push)
#  pragma warning(disable:4756;disable:4056)
#endif
static char*
S_my_atof_infnan(pTHX_ const char* s, bool negative, const char* send, NV* value)
{
    const char *p0 = negative ? s - 1 : s;
    const char *p = p0;
    int infnan = grok_infnan(&p, send);
    if (infnan && p != p0) {
        /* If we can generate inf/nan directly, let's do so. */
#ifdef NV_INF
        if ((infnan & IS_NUMBER_INFINITY)) {
            *value = (infnan & IS_NUMBER_NEG) ? -NV_INF: NV_INF;
            return (char*)p;
        }
#endif
#ifdef NV_NAN
        if ((infnan & IS_NUMBER_NAN)) {
            *value = NV_NAN;
            return (char*)p;
        }
#endif
#ifdef Perl_strtod
        /* If still here, we didn't have either NV_INF or NV_NAN,
         * and can try falling back to native strtod/strtold.
         *
         * (Though, are our NV_INF or NV_NAN ever not defined?)
         *
         * The native interface might not recognize all the possible
         * inf/nan strings Perl recognizes.  What we can try
         * is to try faking the input.  We will try inf/-inf/nan
         * as the most promising/portable input. */
        {
            const char* fake = NULL;
            char* endp;
            NV nv;
            if ((infnan & IS_NUMBER_INFINITY)) {
                fake = ((infnan & IS_NUMBER_NEG)) ? "-inf" : "inf";
            }
            else if ((infnan & IS_NUMBER_NAN)) {
                fake = "nan";
            }
            assert(fake);
            nv = Perl_strtod(fake, &endp);
            if (fake != endp) {
                if ((infnan & IS_NUMBER_INFINITY)) {
#ifdef Perl_isinf
                    if (Perl_isinf(nv))
                        *value = nv;
#else
                    /* last resort, may generate SIGFPE */
                    *value = Perl_exp((NV)1e9);
                    if ((infnan & IS_NUMBER_NEG))
                        *value = -*value;
#endif
                    return (char*)p; /* p, not endp */
                }
                else if ((infnan & IS_NUMBER_NAN)) {
#ifdef Perl_isnan
                    if (Perl_isnan(nv))
                        *value = nv;
#else
                    /* last resort, may generate SIGFPE */
                    *value = Perl_log((NV)-1.0);
#endif
                    return (char*)p; /* p, not endp */
                }
            }
        }
#endif /* #ifdef Perl_strtod */
    }
    return NULL;
}
#ifdef USING_MSVC6
#  pragma warning(pop)
#endif

char*
Perl_my_atof2(pTHX_ const char* orig, NV* value)
{
    const char* s = orig;
    NV result[3] = {0.0, 0.0, 0.0};
#if defined(USE_PERL_ATOF) || defined(USE_QUADMATH)
    const char* send = s + strlen(orig); /* one past the last */
    bool negative = 0;
#endif
#if defined(USE_PERL_ATOF) && !defined(USE_QUADMATH)
    UV accumulator[2] = {0,0};	/* before/after dp */
    bool seen_digit = 0;
    I32 exp_adjust[2] = {0,0};
    I32 exp_acc[2] = {-1, -1};
    /* the current exponent adjust for the accumulators */
    I32 exponent = 0;
    I32	seen_dp  = 0;
    I32 digit = 0;
    I32 old_digit = 0;
    I32 sig_digits = 0; /* noof significant digits seen so far */
#endif

#if defined(USE_PERL_ATOF) || defined(USE_QUADMATH)
    PERL_ARGS_ASSERT_MY_ATOF2;

    /* leading whitespace */
    while (isSPACE(*s))
	++s;

    /* sign */
    switch (*s) {
	case '-':
	    negative = 1;
	    /* FALLTHROUGH */
	case '+':
	    ++s;
    }
#endif

#ifdef USE_QUADMATH
    {
        char* endp;
        if ((endp = S_my_atof_infnan(aTHX_ s, negative, send, value)))
            return endp;
        result[2] = strtoflt128(s, &endp);
        if (s != endp) {
            *value = negative ? -result[2] : result[2];
            return endp;
        }
        return NULL;
    }
#elif defined(USE_PERL_ATOF)

/* There is no point in processing more significant digits
 * than the NV can hold. Note that NV_DIG is a lower-bound value,
 * while we need an upper-bound value. We add 2 to account for this;
 * since it will have been conservative on both the first and last digit.
 * For example a 32-bit mantissa with an exponent of 4 would have
 * exact values in the set
 *               4
 *               8
 *              ..
 *     17179869172
 *     17179869176
 *     17179869180
 *
 * where for the purposes of calculating NV_DIG we would have to discount
 * both the first and last digit, since neither can hold all values from
 * 0..9; but for calculating the value we must examine those two digits.
 */
#ifdef MAX_SIG_DIG_PLUS
    /* It is not necessarily the case that adding 2 to NV_DIG gets all the
       possible digits in a NV, especially if NVs are not IEEE compliant
       (e.g., long doubles on IRIX) - Allen <allens@cpan.org> */
# define MAX_SIG_DIGITS (NV_DIG+MAX_SIG_DIG_PLUS)
#else
# define MAX_SIG_DIGITS (NV_DIG+2)
#endif

/* the max number we can accumulate in a UV, and still safely do 10*N+9 */
#define MAX_ACCUMULATE ( (UV) ((UV_MAX - 9)/10))

    {
        const char* endp;
        if ((endp = S_my_atof_infnan(aTHX_ s, negative, send, value)))
            return (char*)endp;
    }

    /* we accumulate digits into an integer; when this becomes too
     * large, we add the total to NV and start again */

    while (1) {
	if (isDIGIT(*s)) {
	    seen_digit = 1;
	    old_digit = digit;
	    digit = *s++ - '0';
	    if (seen_dp)
		exp_adjust[1]++;

	    /* don't start counting until we see the first significant
	     * digit, eg the 5 in 0.00005... */
	    if (!sig_digits && digit == 0)
		continue;

	    if (++sig_digits > MAX_SIG_DIGITS) {
		/* limits of precision reached */
	        if (digit > 5) {
		    ++accumulator[seen_dp];
		} else if (digit == 5) {
		    if (old_digit % 2) { /* round to even - Allen */
			++accumulator[seen_dp];
		    }
		}
		if (seen_dp) {
		    exp_adjust[1]--;
		} else {
		    exp_adjust[0]++;
		}
		/* skip remaining digits */
		while (isDIGIT(*s)) {
		    ++s;
		    if (! seen_dp) {
			exp_adjust[0]++;
		    }
		}
		/* warn of loss of precision? */
	    }
	    else {
		if (accumulator[seen_dp] > MAX_ACCUMULATE) {
		    /* add accumulator to result and start again */
		    result[seen_dp] = S_mulexp10(result[seen_dp],
						 exp_acc[seen_dp])
			+ (NV)accumulator[seen_dp];
		    accumulator[seen_dp] = 0;
		    exp_acc[seen_dp] = 0;
		}
		accumulator[seen_dp] = accumulator[seen_dp] * 10 + digit;
		++exp_acc[seen_dp];
	    }
	}
	else if (!seen_dp && GROK_NUMERIC_RADIX(&s, send)) {
	    seen_dp = 1;
	    if (sig_digits > MAX_SIG_DIGITS) {
		do {
		    ++s;
		} while (isDIGIT(*s));
		break;
	    }
	}
	else {
	    break;
	}
    }

    result[0] = S_mulexp10(result[0], exp_acc[0]) + (NV)accumulator[0];
    if (seen_dp) {
	result[1] = S_mulexp10(result[1], exp_acc[1]) + (NV)accumulator[1];
    }

    if (seen_digit && (isALPHA_FOLD_EQ(*s, 'e'))) {
	bool expnegative = 0;

	++s;
	switch (*s) {
	    case '-':
		expnegative = 1;
		/* FALLTHROUGH */
	    case '+':
		++s;
	}
	while (isDIGIT(*s))
	    exponent = exponent * 10 + (*s++ - '0');
	if (expnegative)
	    exponent = -exponent;
    }



    /* now apply the exponent */

    if (seen_dp) {
	result[2] = S_mulexp10(result[0],exponent+exp_adjust[0])
		+ S_mulexp10(result[1],exponent-exp_adjust[1]);
    } else {
	result[2] = S_mulexp10(result[0],exponent+exp_adjust[0]);
    }

    /* now apply the sign */
    if (negative)
	result[2] = -result[2];
#endif /* USE_PERL_ATOF */
    *value = result[2];
    return (char *)s;
}

/*
=for apidoc isinfnan

C<Perl_isinfnan()> is utility function that returns true if the NV
argument is either an infinity or a C<NaN>, false otherwise.  To test
in more detail, use C<Perl_isinf()> and C<Perl_isnan()>.

This is also the logical inverse of Perl_isfinite().

=cut
*/
bool
Perl_isinfnan(NV nv)
{
#ifdef Perl_isinf
    if (Perl_isinf(nv))
        return TRUE;
#endif
#ifdef Perl_isnan
    if (Perl_isnan(nv))
        return TRUE;
#endif
    return FALSE;
}

/*
=for apidoc

Checks whether the argument would be either an infinity or C<NaN> when used
as a number, but is careful not to trigger non-numeric or uninitialized
warnings.  it assumes the caller has done C<SvGETMAGIC(sv)> already.

=cut
*/

bool
Perl_isinfnansv(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_ISINFNANSV;
    if (!SvOK(sv))
        return FALSE;
    if (SvNOKp(sv))
        return Perl_isinfnan(SvNVX(sv));
    if (SvIOKp(sv))
        return FALSE;
    {
        STRLEN len;
        const char *s = SvPV_nomg_const(sv, len);
        return cBOOL(grok_infnan(&s, s+len));
    }
}

#ifndef HAS_MODFL
/* C99 has truncl, pre-C99 Solaris had aintl.  We can use either with
 * copysignl to emulate modfl, which is in some platforms missing or
 * broken. */
#  if defined(HAS_TRUNCL) && defined(HAS_COPYSIGNL)
long double
Perl_my_modfl(long double x, long double *ip)
{
    *ip = truncl(x);
    return (x == *ip ? copysignl(0.0L, x) : x - *ip);
}
#  elif defined(HAS_AINTL) && defined(HAS_COPYSIGNL)
long double
Perl_my_modfl(long double x, long double *ip)
{
    *ip = aintl(x);
    return (x == *ip ? copysignl(0.0L, x) : x - *ip);
}
#  endif
#endif

/* Similarly, with ilogbl and scalbnl we can emulate frexpl. */
#if ! defined(HAS_FREXPL) && defined(HAS_ILOGBL) && defined(HAS_SCALBNL)
long double
Perl_my_frexpl(long double x, int *e) {
    *e = x == 0.0L ? 0 : ilogbl(x) + 1;
    return (scalbnl(x, -*e));
}
#endif

/*
=for apidoc Perl_signbit

Return a non-zero integer if the sign bit on an NV is set, and 0 if
it is not.  

If F<Configure> detects this system has a C<signbit()> that will work with
our NVs, then we just use it via the C<#define> in F<perl.h>.  Otherwise,
fall back on this implementation.  The main use of this function
is catching C<-0.0>.

C<Configure> notes:  This function is called C<'Perl_signbit'> instead of a
plain C<'signbit'> because it is easy to imagine a system having a C<signbit()>
function or macro that doesn't happen to work with our particular choice
of NVs.  We shouldn't just re-C<#define> C<signbit> as C<Perl_signbit> and expect
the standard system headers to be happy.  Also, this is a no-context
function (no C<pTHX_>) because C<Perl_signbit()> is usually re-C<#defined> in
F<perl.h> as a simple macro call to the system's C<signbit()>.
Users should just always call C<Perl_signbit()>.

=cut
*/
#if !defined(HAS_SIGNBIT)
int
Perl_signbit(NV x) {
#  ifdef Perl_fp_class_nzero
    return Perl_fp_class_nzero(x);
    /* Try finding the high byte, and assume it's highest bit
     * is the sign.  This assumption is probably wrong somewhere. */
#  elif defined(USE_LONG_DOUBLE) && LONG_DOUBLEKIND == LONG_DOUBLE_IS_X86_80_BIT_LITTLE_ENDIAN
    return (((unsigned char *)&x)[9] & 0x80);
#  elif defined(NV_LITTLE_ENDIAN)
    /* Note that NVSIZE is sizeof(NV), which would make the below be
     * wrong if the end bytes are unused, which happens with the x86
     * 80-bit long doubles, which is why take care of that above. */
    return (((unsigned char *)&x)[NVSIZE - 1] & 0x80);
#  elif defined(NV_BIG_ENDIAN)
    return (((unsigned char *)&x)[0] & 0x80);
#  else
    /* This last resort fallback is wrong for the negative zero. */
    return (x < 0.0) ? 1 : 0;
#  endif
}
#endif

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
