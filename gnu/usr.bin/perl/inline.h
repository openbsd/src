/*    inline.h
 *
 *    Copyright (C) 2012 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * This file is a home for static inline functions that cannot go in other
 * headers files, because they depend on proto.h (included after most other
 * headers) or struct definitions.
 *
 * Each section names the header file that the functions "belong" to.
 */

/* ------------------------------- av.h ------------------------------- */

PERL_STATIC_INLINE SSize_t
S_av_top_index(pTHX_ AV *av)
{
    PERL_ARGS_ASSERT_AV_TOP_INDEX;
    assert(SvTYPE(av) == SVt_PVAV);

    return AvFILL(av);
}

/* ------------------------------- cv.h ------------------------------- */

PERL_STATIC_INLINE GV *
S_CvGV(pTHX_ CV *sv)
{
    return CvNAMED(sv)
	? Perl_cvgv_from_hek(aTHX_ sv)
	: ((XPVCV*)MUTABLE_PTR(SvANY(sv)))->xcv_gv_u.xcv_gv;
}

PERL_STATIC_INLINE I32 *
S_CvDEPTHp(const CV * const sv)
{
    assert(SvTYPE(sv) == SVt_PVCV || SvTYPE(sv) == SVt_PVFM);
    return &((XPVCV*)SvANY(sv))->xcv_depth;
}

/*
 CvPROTO returns the prototype as stored, which is not necessarily what
 the interpreter should be using. Specifically, the interpreter assumes
 that spaces have been stripped, which has been the case if the prototype
 was added by toke.c, but is generally not the case if it was added elsewhere.
 Since we can't enforce the spacelessness at assignment time, this routine
 provides a temporary copy at parse time with spaces removed.
 I<orig> is the start of the original buffer, I<len> is the length of the
 prototype and will be updated when this returns.
 */

#ifdef PERL_CORE
PERL_STATIC_INLINE char *
S_strip_spaces(pTHX_ const char * orig, STRLEN * const len)
{
    SV * tmpsv;
    char * tmps;
    tmpsv = newSVpvn_flags(orig, *len, SVs_TEMP);
    tmps = SvPVX(tmpsv);
    while ((*len)--) {
	if (!isSPACE(*orig))
	    *tmps++ = *orig;
	orig++;
    }
    *tmps = '\0';
    *len = tmps - SvPVX(tmpsv);
		return SvPVX(tmpsv);
}
#endif

/* ------------------------------- mg.h ------------------------------- */

#if defined(PERL_CORE) || defined(PERL_EXT)
/* assumes get-magic and stringification have already occurred */
PERL_STATIC_INLINE STRLEN
S_MgBYTEPOS(pTHX_ MAGIC *mg, SV *sv, const char *s, STRLEN len)
{
    assert(mg->mg_type == PERL_MAGIC_regex_global);
    assert(mg->mg_len != -1);
    if (mg->mg_flags & MGf_BYTES || !DO_UTF8(sv))
	return (STRLEN)mg->mg_len;
    else {
	const STRLEN pos = (STRLEN)mg->mg_len;
	/* Without this check, we may read past the end of the buffer: */
	if (pos > sv_or_pv_len_utf8(sv, s, len)) return len+1;
	return sv_or_pv_pos_u2b(sv, s, pos, NULL);
    }
}
#endif

/* ------------------------------- pad.h ------------------------------ */

#if defined(PERL_IN_PAD_C) || defined(PERL_IN_OP_C)
PERL_STATIC_INLINE bool
PadnameIN_SCOPE(const PADNAME * const pn, const U32 seq)
{
    /* is seq within the range _LOW to _HIGH ?
     * This is complicated by the fact that PL_cop_seqmax
     * may have wrapped around at some point */
    if (COP_SEQ_RANGE_LOW(pn) == PERL_PADSEQ_INTRO)
	return FALSE; /* not yet introduced */

    if (COP_SEQ_RANGE_HIGH(pn) == PERL_PADSEQ_INTRO) {
    /* in compiling scope */
	if (
	    (seq >  COP_SEQ_RANGE_LOW(pn))
	    ? (seq - COP_SEQ_RANGE_LOW(pn) < (U32_MAX >> 1))
	    : (COP_SEQ_RANGE_LOW(pn) - seq > (U32_MAX >> 1))
	)
	    return TRUE;
    }
    else if (
	(COP_SEQ_RANGE_LOW(pn) > COP_SEQ_RANGE_HIGH(pn))
	?
	    (  seq >  COP_SEQ_RANGE_LOW(pn)
	    || seq <= COP_SEQ_RANGE_HIGH(pn))

	:    (  seq >  COP_SEQ_RANGE_LOW(pn)
	     && seq <= COP_SEQ_RANGE_HIGH(pn))
    )
	return TRUE;
    return FALSE;
}
#endif

/* ------------------------------- pp.h ------------------------------- */

PERL_STATIC_INLINE I32
S_TOPMARK(pTHX)
{
    DEBUG_s(DEBUG_v(PerlIO_printf(Perl_debug_log,
				 "MARK top  %p %" IVdf "\n",
				  PL_markstack_ptr,
				  (IV)*PL_markstack_ptr)));
    return *PL_markstack_ptr;
}

PERL_STATIC_INLINE I32
S_POPMARK(pTHX)
{
    DEBUG_s(DEBUG_v(PerlIO_printf(Perl_debug_log,
				 "MARK pop  %p %" IVdf "\n",
				  (PL_markstack_ptr-1),
				  (IV)*(PL_markstack_ptr-1))));
    assert((PL_markstack_ptr > PL_markstack) || !"MARK underflow");
    return *PL_markstack_ptr--;
}

/* ----------------------------- regexp.h ----------------------------- */

PERL_STATIC_INLINE struct regexp *
S_ReANY(const REGEXP * const re)
{
    XPV* const p = (XPV*)SvANY(re);
    assert(isREGEXP(re));
    return SvTYPE(re) == SVt_PVLV ? p->xpv_len_u.xpvlenu_rx
                                   : (struct regexp *)p;
}

/* ------------------------------- sv.h ------------------------------- */

PERL_STATIC_INLINE SV *
S_SvREFCNT_inc(SV *sv)
{
    if (LIKELY(sv != NULL))
	SvREFCNT(sv)++;
    return sv;
}
PERL_STATIC_INLINE SV *
S_SvREFCNT_inc_NN(SV *sv)
{
    SvREFCNT(sv)++;
    return sv;
}
PERL_STATIC_INLINE void
S_SvREFCNT_inc_void(SV *sv)
{
    if (LIKELY(sv != NULL))
	SvREFCNT(sv)++;
}
PERL_STATIC_INLINE void
S_SvREFCNT_dec(pTHX_ SV *sv)
{
    if (LIKELY(sv != NULL)) {
	U32 rc = SvREFCNT(sv);
	if (LIKELY(rc > 1))
	    SvREFCNT(sv) = rc - 1;
	else
	    Perl_sv_free2(aTHX_ sv, rc);
    }
}

PERL_STATIC_INLINE void
S_SvREFCNT_dec_NN(pTHX_ SV *sv)
{
    U32 rc = SvREFCNT(sv);
    if (LIKELY(rc > 1))
	SvREFCNT(sv) = rc - 1;
    else
	Perl_sv_free2(aTHX_ sv, rc);
}

PERL_STATIC_INLINE void
SvAMAGIC_on(SV *sv)
{
    assert(SvROK(sv));
    if (SvOBJECT(SvRV(sv))) HvAMAGIC_on(SvSTASH(SvRV(sv)));
}
PERL_STATIC_INLINE void
SvAMAGIC_off(SV *sv)
{
    if (SvROK(sv) && SvOBJECT(SvRV(sv)))
	HvAMAGIC_off(SvSTASH(SvRV(sv)));
}

PERL_STATIC_INLINE U32
S_SvPADSTALE_on(SV *sv)
{
    assert(!(SvFLAGS(sv) & SVs_PADTMP));
    return SvFLAGS(sv) |= SVs_PADSTALE;
}
PERL_STATIC_INLINE U32
S_SvPADSTALE_off(SV *sv)
{
    assert(!(SvFLAGS(sv) & SVs_PADTMP));
    return SvFLAGS(sv) &= ~SVs_PADSTALE;
}
#if defined(PERL_CORE) || defined (PERL_EXT)
PERL_STATIC_INLINE STRLEN
S_sv_or_pv_pos_u2b(pTHX_ SV *sv, const char *pv, STRLEN pos, STRLEN *lenp)
{
    PERL_ARGS_ASSERT_SV_OR_PV_POS_U2B;
    if (SvGAMAGIC(sv)) {
	U8 *hopped = utf8_hop((U8 *)pv, pos);
	if (lenp) *lenp = (STRLEN)(utf8_hop(hopped, *lenp) - hopped);
	return (STRLEN)(hopped - (U8 *)pv);
    }
    return sv_pos_u2b_flags(sv,pos,lenp,SV_CONST_RETURN);
}
#endif

/* ------------------------------- handy.h ------------------------------- */

/* saves machine code for a common noreturn idiom typically used in Newx*() */
GCC_DIAG_IGNORE_DECL(-Wunused-function);
static void
S_croak_memory_wrap(void)
{
    Perl_croak_nocontext("%s",PL_memory_wrap);
}
GCC_DIAG_RESTORE_DECL;

/* ------------------------------- utf8.h ------------------------------- */

/*
=head1 Unicode Support
*/

PERL_STATIC_INLINE void
S_append_utf8_from_native_byte(const U8 byte, U8** dest)
{
    /* Takes an input 'byte' (Latin1 or EBCDIC) and appends it to the UTF-8
     * encoded string at '*dest', updating '*dest' to include it */

    PERL_ARGS_ASSERT_APPEND_UTF8_FROM_NATIVE_BYTE;

    if (NATIVE_BYTE_IS_INVARIANT(byte))
        *((*dest)++) = byte;
    else {
        *((*dest)++) = UTF8_EIGHT_BIT_HI(byte);
        *((*dest)++) = UTF8_EIGHT_BIT_LO(byte);
    }
}

/*
=for apidoc valid_utf8_to_uvchr
Like C<L</utf8_to_uvchr_buf>>, but should only be called when it is known that
the next character in the input UTF-8 string C<s> is well-formed (I<e.g.>,
it passes C<L</isUTF8_CHAR>>.  Surrogates, non-character code points, and
non-Unicode code points are allowed.

=cut

 */

PERL_STATIC_INLINE UV
Perl_valid_utf8_to_uvchr(const U8 *s, STRLEN *retlen)
{
    const UV expectlen = UTF8SKIP(s);
    const U8* send = s + expectlen;
    UV uv = *s;

    PERL_ARGS_ASSERT_VALID_UTF8_TO_UVCHR;

    if (retlen) {
        *retlen = expectlen;
    }

    /* An invariant is trivially returned */
    if (expectlen == 1) {
	return uv;
    }

    /* Remove the leading bits that indicate the number of bytes, leaving just
     * the bits that are part of the value */
    uv = NATIVE_UTF8_TO_I8(uv) & UTF_START_MASK(expectlen);

    /* Now, loop through the remaining bytes, accumulating each into the
     * working total as we go.  (I khw tried unrolling the loop for up to 4
     * bytes, but there was no performance improvement) */
    for (++s; s < send; s++) {
        uv = UTF8_ACCUMULATE(uv, *s);
    }

    return UNI_TO_NATIVE(uv);

}

/*
=for apidoc is_utf8_invariant_string

Returns TRUE if the first C<len> bytes of the string C<s> are the same
regardless of the UTF-8 encoding of the string (or UTF-EBCDIC encoding on
EBCDIC machines); otherwise it returns FALSE.  That is, it returns TRUE if they
are UTF-8 invariant.  On ASCII-ish machines, all the ASCII characters and only
the ASCII characters fit this definition.  On EBCDIC machines, the ASCII-range
characters are invariant, but so also are the C1 controls.

If C<len> is 0, it will be calculated using C<strlen(s)>, (which means if you
use this option, that C<s> can't have embedded C<NUL> characters and has to
have a terminating C<NUL> byte).

See also
C<L</is_utf8_string>>,
C<L</is_utf8_string_flags>>,
C<L</is_utf8_string_loc>>,
C<L</is_utf8_string_loc_flags>>,
C<L</is_utf8_string_loclen>>,
C<L</is_utf8_string_loclen_flags>>,
C<L</is_utf8_fixed_width_buf_flags>>,
C<L</is_utf8_fixed_width_buf_loc_flags>>,
C<L</is_utf8_fixed_width_buf_loclen_flags>>,
C<L</is_strict_utf8_string>>,
C<L</is_strict_utf8_string_loc>>,
C<L</is_strict_utf8_string_loclen>>,
C<L</is_c9strict_utf8_string>>,
C<L</is_c9strict_utf8_string_loc>>,
and
C<L</is_c9strict_utf8_string_loclen>>.

=cut

*/

#define is_utf8_invariant_string(s, len)                                    \
                                is_utf8_invariant_string_loc(s, len, NULL)

/*
=for apidoc is_utf8_invariant_string_loc

Like C<L</is_utf8_invariant_string>> but upon failure, stores the location of
the first UTF-8 variant character in the C<ep> pointer; if all characters are
UTF-8 invariant, this function does not change the contents of C<*ep>.

=cut

*/

PERL_STATIC_INLINE bool
S_is_utf8_invariant_string_loc(const U8* const s, STRLEN len, const U8 ** ep)
{
    const U8* send;
    const U8* x = s;

    PERL_ARGS_ASSERT_IS_UTF8_INVARIANT_STRING_LOC;

    if (len == 0) {
        len = strlen((const char *)s);
    }

    send = s + len;

/* This looks like 0x010101... */
#  define PERL_COUNT_MULTIPLIER   (~ (UINTMAX_C(0)) / 0xFF)

/* This looks like 0x808080... */
#  define PERL_VARIANTS_WORD_MASK (PERL_COUNT_MULTIPLIER * 0x80)
#  define PERL_WORDSIZE            sizeof(PERL_UINTMAX_T)
#  define PERL_WORD_BOUNDARY_MASK (PERL_WORDSIZE - 1)

/* Evaluates to 0 if 'x' is at a word boundary; otherwise evaluates to 1, by
 * or'ing together the lowest bits of 'x'.  Hopefully the final term gets
 * optimized out completely on a 32-bit system, and its mask gets optimized out
 * on a 64-bit system */
#  define PERL_IS_SUBWORD_ADDR(x) (1 & (       PTR2nat(x)                     \
                                      |   (  PTR2nat(x) >> 1)                 \
                                      | ( ( (PTR2nat(x)                       \
                                           & PERL_WORD_BOUNDARY_MASK) >> 2))))

#ifndef EBCDIC

    /* Do the word-at-a-time iff there is at least one usable full word.  That
     * means that after advancing to a word boundary, there still is at least a
     * full word left.  The number of bytes needed to advance is 'wordsize -
     * offset' unless offset is 0. */
    if ((STRLEN) (send - x) >= PERL_WORDSIZE

                            /* This term is wordsize if subword; 0 if not */
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(x)

                            /* 'offset' */
                          - (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK))
    {

        /* Process per-byte until reach word boundary.  XXX This loop could be
         * eliminated if we knew that this platform had fast unaligned reads */
        while (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK) {
            if (! UTF8_IS_INVARIANT(*x)) {
                if (ep) {
                    *ep = x;
                }

                return FALSE;
            }
            x++;
        }

        /* Here, we know we have at least one full word to process.  Process
         * per-word as long as we have at least a full word left */
        do {
            if ((* (PERL_UINTMAX_T *) x) & PERL_VARIANTS_WORD_MASK)  {

                /* Found a variant.  Just return if caller doesn't want its
                 * exact position */
                if (! ep) {
                    return FALSE;
                }

#  if   BYTEORDER == 0x1234 || BYTEORDER == 0x12345678    \
     || BYTEORDER == 0x4321 || BYTEORDER == 0x87654321

                *ep = x + _variant_byte_number(* (PERL_UINTMAX_T *) x);
                assert(*ep >= s && *ep < send);

                return FALSE;

#  else   /* If weird byte order, drop into next loop to do byte-at-a-time
           checks. */

                break;
#  endif
            }

            x += PERL_WORDSIZE;

        } while (x + PERL_WORDSIZE <= send);
    }

#endif      /* End of ! EBCDIC */

    /* Process per-byte */
    while (x < send) {
	if (! UTF8_IS_INVARIANT(*x)) {
            if (ep) {
                *ep = x;
            }

            return FALSE;
        }

        x++;
    }

    return TRUE;
}

#ifndef EBCDIC

PERL_STATIC_INLINE unsigned int
S__variant_byte_number(PERL_UINTMAX_T word)
{

    /* This returns the position in a word (0..7) of the first variant byte in
     * it.  This is a helper function.  Note that there are no branches */

    assert(word);

    /* Get just the msb bits of each byte */
    word &= PERL_VARIANTS_WORD_MASK;

#  ifdef USING_MSVC6    /* VC6 has some issues with the normal code, and the
                           easiest thing is to hide that from the callers */
    {
        unsigned int i;
        const U8 * s = (U8 *) &word;
        dTHX;

        for (i = 0; i < sizeof(word); i++ ) {
            if (s[i]) {
                return i;
            }
        }

        Perl_croak(aTHX_ "panic: %s: %d: unexpected zero word\n",
                                 __FILE__, __LINE__);
    }

#  elif BYTEORDER == 0x1234 || BYTEORDER == 0x12345678

    /* Bytes are stored like
     *  Byte8 ... Byte2 Byte1
     *  63..56...15...8 7...0
     *
     *  Isolate the lsb;
     * https://stackoverflow.com/questions/757059/position-of-least-significant-bit-that-is-set
     *
     * The word will look this this, with a rightmost set bit in position 's':
     * ('x's are don't cares)
     *      s
     *  x..x100..0
     *  x..xx10..0      Right shift (rightmost 0 is shifted off)
     *  x..xx01..1      Subtract 1, turns all the trailing zeros into 1's and
     *                  the 1 just to their left into a 0; the remainder is
     *                  untouched
     *  0..0011..1      The xor with x..xx10..0 clears that remainder, sets
     *                  bottom to all 1
     *  0..0100..0      Add 1 to clear the word except for the bit in 's'
     *
     * Another method is to do 'word &= -word'; but it generates a compiler
     * message on some platforms about taking the negative of an unsigned */

    word >>= 1;
    word = 1 + (word ^ (word - 1));

#  elif BYTEORDER == 0x4321 || BYTEORDER == 0x87654321

    /* Bytes are stored like
     *  Byte1 Byte2  ... Byte8
     * 63..56 55..47 ... 7...0
     *
     * Isolate the msb; http://codeforces.com/blog/entry/10330
     *
     * Only the most significant set bit matters.  Or'ing word with its right
     * shift of 1 makes that bit and the next one to its right both 1.  Then
     * right shifting by 2 makes for 4 1-bits in a row. ...  We end with the
     * msb and all to the right being 1. */
    word |= word >>  1;
    word |= word >>  2;
    word |= word >>  4;
    word |= word >>  8;
    word |= word >> 16;
    word |= word >> 32;  /* This should get optimized out on 32-bit systems. */

    /* Then subtracting the right shift by 1 clears all but the left-most of
     * the 1 bits, which is our desired result */
    word -= (word >> 1);

#  else
#    error Unexpected byte order
#  endif

    /* Here 'word' has a single bit set: the  msb of the first byte in which it
     * is set.  Calculate that position in the word.  We can use this
     * specialized solution: https://stackoverflow.com/a/32339674/1626653,
     * assumes an 8-bit byte.  (On a 32-bit machine, the larger numbers should
     * just get shifted off at compile time) */
    word = (word >> 7) * ((UINTMAX_C( 7) << 56) | (UINTMAX_C(15) << 48)
                        | (UINTMAX_C(23) << 40) | (UINTMAX_C(31) << 32)
                        |           (39 <<  24) |           (47 <<  16)
                        |           (55 <<   8) |           (63 <<   0));
    word >>= PERL_WORDSIZE * 7; /* >> by either 56 or 24 */

    /* Here, word contains the position 7..63 of that bit.  Convert to 0..7 */
    word = ((word + 1) >> 3) - 1;

#  if BYTEORDER == 0x4321 || BYTEORDER == 0x87654321

    /* And invert the result */
    word = CHARBITS - word - 1;

#  endif

    return (unsigned int) word;
}

#endif
#if defined(PERL_CORE) || defined(PERL_EXT)

/*
=for apidoc variant_under_utf8_count

This function looks at the sequence of bytes between C<s> and C<e>, which are
assumed to be encoded in ASCII/Latin1, and returns how many of them would
change should the string be translated into UTF-8.  Due to the nature of UTF-8,
each of these would occupy two bytes instead of the single one in the input
string.  Thus, this function returns the precise number of bytes the string
would expand by when translated to UTF-8.

Unlike most of the other functions that have C<utf8> in their name, the input
to this function is NOT a UTF-8-encoded string.  The function name is slightly
I<odd> to emphasize this.

This function is internal to Perl because khw thinks that any XS code that
would want this is probably operating too close to the internals.  Presenting a
valid use case could change that.

See also
C<L<perlapi/is_utf8_invariant_string>>
and
C<L<perlapi/is_utf8_invariant_string_loc>>,

=cut

*/

PERL_STATIC_INLINE Size_t
S_variant_under_utf8_count(const U8* const s, const U8* const e)
{
    const U8* x = s;
    Size_t count = 0;

    PERL_ARGS_ASSERT_VARIANT_UNDER_UTF8_COUNT;

#  ifndef EBCDIC

    /* Test if the string is long enough to use word-at-a-time.  (Logic is the
     * same as for is_utf8_invariant_string()) */
    if ((STRLEN) (e - x) >= PERL_WORDSIZE
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(x)
                          - (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK))
    {

        /* Process per-byte until reach word boundary.  XXX This loop could be
         * eliminated if we knew that this platform had fast unaligned reads */
        while (PTR2nat(x) & PERL_WORD_BOUNDARY_MASK) {
            count += ! UTF8_IS_INVARIANT(*x++);
        }

        /* Process per-word as long as we have at least a full word left */
        do {    /* Commit 03c1e4ab1d6ee9062fb3f94b0ba31db6698724b1 contains an
                   explanation of how this works */
            PERL_UINTMAX_T increment
                = ((((* (PERL_UINTMAX_T *) x) & PERL_VARIANTS_WORD_MASK) >> 7)
                      * PERL_COUNT_MULTIPLIER)
                    >> ((PERL_WORDSIZE - 1) * CHARBITS);
            count += (Size_t) increment;
            x += PERL_WORDSIZE;
        } while (x + PERL_WORDSIZE <= e);
    }

#  endif

    /* Process per-byte */
    while (x < e) {
	if (! UTF8_IS_INVARIANT(*x)) {
            count++;
        }

        x++;
    }

    return count;
}

#endif

#ifndef PERL_IN_REGEXEC_C   /* Keep  these around for that file */
#  undef PERL_WORDSIZE
#  undef PERL_COUNT_MULTIPLIER
#  undef PERL_WORD_BOUNDARY_MASK
#  undef PERL_VARIANTS_WORD_MASK
#endif

/*
=for apidoc is_utf8_string

Returns TRUE if the first C<len> bytes of string C<s> form a valid
Perl-extended-UTF-8 string; returns FALSE otherwise.  If C<len> is 0, it will
be calculated using C<strlen(s)> (which means if you use this option, that C<s>
can't have embedded C<NUL> characters and has to have a terminating C<NUL>
byte).  Note that all characters being ASCII constitute 'a valid UTF-8 string'.

This function considers Perl's extended UTF-8 to be valid.  That means that
code points above Unicode, surrogates, and non-character code points are
considered valid by this function.  Use C<L</is_strict_utf8_string>>,
C<L</is_c9strict_utf8_string>>, or C<L</is_utf8_string_flags>> to restrict what
code points are considered valid.

See also
C<L</is_utf8_invariant_string>>,
C<L</is_utf8_invariant_string_loc>>,
C<L</is_utf8_string_loc>>,
C<L</is_utf8_string_loclen>>,
C<L</is_utf8_fixed_width_buf_flags>>,
C<L</is_utf8_fixed_width_buf_loc_flags>>,
C<L</is_utf8_fixed_width_buf_loclen_flags>>,

=cut
*/

#define is_utf8_string(s, len)  is_utf8_string_loclen(s, len, NULL, NULL)

#if defined(PERL_CORE) || defined (PERL_EXT)

/*
=for apidoc is_utf8_non_invariant_string

Returns TRUE if L<perlapi/is_utf8_invariant_string> returns FALSE for the first
C<len> bytes of the string C<s>, but they are, nonetheless, legal Perl-extended
UTF-8; otherwise returns FALSE.

A TRUE return means that at least one code point represented by the sequence
either is a wide character not representable as a single byte, or the
representation differs depending on whether the sequence is encoded in UTF-8 or
not.

See also
C<L<perlapi/is_utf8_invariant_string>>,
C<L<perlapi/is_utf8_string>>

=cut

This is commonly used to determine if a SV's UTF-8 flag should be turned on.
It generally needn't be if its string is entirely UTF-8 invariant, and it
shouldn't be if it otherwise contains invalid UTF-8.

It is an internal function because khw thinks that XS code shouldn't be working
at this low a level.  A valid use case could change that.

*/

PERL_STATIC_INLINE bool
S_is_utf8_non_invariant_string(const U8* const s, STRLEN len)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_NON_INVARIANT_STRING;

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        return FALSE;
    }

    return is_utf8_string(first_variant, len - (first_variant - s));
}

#endif

/*
=for apidoc is_strict_utf8_string

Returns TRUE if the first C<len> bytes of string C<s> form a valid
UTF-8-encoded string that is fully interchangeable by any application using
Unicode rules; otherwise it returns FALSE.  If C<len> is 0, it will be
calculated using C<strlen(s)> (which means if you use this option, that C<s>
can't have embedded C<NUL> characters and has to have a terminating C<NUL>
byte).  Note that all characters being ASCII constitute 'a valid UTF-8 string'.

This function returns FALSE for strings containing any
code points above the Unicode max of 0x10FFFF, surrogate code points, or
non-character code points.

See also
C<L</is_utf8_invariant_string>>,
C<L</is_utf8_invariant_string_loc>>,
C<L</is_utf8_string>>,
C<L</is_utf8_string_flags>>,
C<L</is_utf8_string_loc>>,
C<L</is_utf8_string_loc_flags>>,
C<L</is_utf8_string_loclen>>,
C<L</is_utf8_string_loclen_flags>>,
C<L</is_utf8_fixed_width_buf_flags>>,
C<L</is_utf8_fixed_width_buf_loc_flags>>,
C<L</is_utf8_fixed_width_buf_loclen_flags>>,
C<L</is_strict_utf8_string_loc>>,
C<L</is_strict_utf8_string_loclen>>,
C<L</is_c9strict_utf8_string>>,
C<L</is_c9strict_utf8_string_loc>>,
and
C<L</is_c9strict_utf8_string_loclen>>.

=cut
*/

#define is_strict_utf8_string(s, len)  is_strict_utf8_string_loclen(s, len, NULL, NULL)

/*
=for apidoc is_c9strict_utf8_string

Returns TRUE if the first C<len> bytes of string C<s> form a valid
UTF-8-encoded string that conforms to
L<Unicode Corrigendum #9|http://www.unicode.org/versions/corrigendum9.html>;
otherwise it returns FALSE.  If C<len> is 0, it will be calculated using
C<strlen(s)> (which means if you use this option, that C<s> can't have embedded
C<NUL> characters and has to have a terminating C<NUL> byte).  Note that all
characters being ASCII constitute 'a valid UTF-8 string'.

This function returns FALSE for strings containing any code points above the
Unicode max of 0x10FFFF or surrogate code points, but accepts non-character
code points per
L<Corrigendum #9|http://www.unicode.org/versions/corrigendum9.html>.

See also
C<L</is_utf8_invariant_string>>,
C<L</is_utf8_invariant_string_loc>>,
C<L</is_utf8_string>>,
C<L</is_utf8_string_flags>>,
C<L</is_utf8_string_loc>>,
C<L</is_utf8_string_loc_flags>>,
C<L</is_utf8_string_loclen>>,
C<L</is_utf8_string_loclen_flags>>,
C<L</is_utf8_fixed_width_buf_flags>>,
C<L</is_utf8_fixed_width_buf_loc_flags>>,
C<L</is_utf8_fixed_width_buf_loclen_flags>>,
C<L</is_strict_utf8_string>>,
C<L</is_strict_utf8_string_loc>>,
C<L</is_strict_utf8_string_loclen>>,
C<L</is_c9strict_utf8_string_loc>>,
and
C<L</is_c9strict_utf8_string_loclen>>.

=cut
*/

#define is_c9strict_utf8_string(s, len)  is_c9strict_utf8_string_loclen(s, len, NULL, 0)

/*
=for apidoc is_utf8_string_flags

Returns TRUE if the first C<len> bytes of string C<s> form a valid
UTF-8 string, subject to the restrictions imposed by C<flags>;
returns FALSE otherwise.  If C<len> is 0, it will be calculated
using C<strlen(s)> (which means if you use this option, that C<s> can't have
embedded C<NUL> characters and has to have a terminating C<NUL> byte).  Note
that all characters being ASCII constitute 'a valid UTF-8 string'.

If C<flags> is 0, this gives the same results as C<L</is_utf8_string>>; if
C<flags> is C<UTF8_DISALLOW_ILLEGAL_INTERCHANGE>, this gives the same results
as C<L</is_strict_utf8_string>>; and if C<flags> is
C<UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE>, this gives the same results as
C<L</is_c9strict_utf8_string>>.  Otherwise C<flags> may be any
combination of the C<UTF8_DISALLOW_I<foo>> flags understood by
C<L</utf8n_to_uvchr>>, with the same meanings.

See also
C<L</is_utf8_invariant_string>>,
C<L</is_utf8_invariant_string_loc>>,
C<L</is_utf8_string>>,
C<L</is_utf8_string_loc>>,
C<L</is_utf8_string_loc_flags>>,
C<L</is_utf8_string_loclen>>,
C<L</is_utf8_string_loclen_flags>>,
C<L</is_utf8_fixed_width_buf_flags>>,
C<L</is_utf8_fixed_width_buf_loc_flags>>,
C<L</is_utf8_fixed_width_buf_loclen_flags>>,
C<L</is_strict_utf8_string>>,
C<L</is_strict_utf8_string_loc>>,
C<L</is_strict_utf8_string_loclen>>,
C<L</is_c9strict_utf8_string>>,
C<L</is_c9strict_utf8_string_loc>>,
and
C<L</is_c9strict_utf8_string_loclen>>.

=cut
*/

PERL_STATIC_INLINE bool
S_is_utf8_string_flags(const U8 *s, STRLEN len, const U32 flags)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_FLAGS;
    assert(0 == (flags & ~(UTF8_DISALLOW_ILLEGAL_INTERCHANGE
                          |UTF8_DISALLOW_PERL_EXTENDED)));

    if (len == 0) {
        len = strlen((const char *)s);
    }

    if (flags == 0) {
        return is_utf8_string(s, len);
    }

    if ((flags & ~UTF8_DISALLOW_PERL_EXTENDED)
                                        == UTF8_DISALLOW_ILLEGAL_INTERCHANGE)
    {
        return is_strict_utf8_string(s, len);
    }

    if ((flags & ~UTF8_DISALLOW_PERL_EXTENDED)
                                       == UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE)
    {
        return is_c9strict_utf8_string(s, len);
    }

    if (! is_utf8_invariant_string_loc(s, len, &first_variant)) {
        const U8* const send = s + len;
        const U8* x = first_variant;

        while (x < send) {
            STRLEN cur_len = isUTF8_CHAR_flags(x, send, flags);
            if (UNLIKELY(! cur_len)) {
                return FALSE;
            }
            x += cur_len;
        }
    }

    return TRUE;
}

/*

=for apidoc is_utf8_string_loc

Like C<L</is_utf8_string>> but stores the location of the failure (in the
case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer.

See also C<L</is_utf8_string_loclen>>.

=cut
*/

#define is_utf8_string_loc(s, len, ep)  is_utf8_string_loclen(s, len, ep, 0)

/*

=for apidoc is_utf8_string_loclen

Like C<L</is_utf8_string>> but stores the location of the failure (in the
case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer, and the number of UTF-8
encoded characters in the C<el> pointer.

See also C<L</is_utf8_string_loc>>.

=cut
*/

PERL_STATIC_INLINE bool
Perl_is_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOCLEN;

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* const send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isUTF8_CHAR(x, send);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

/*

=for apidoc is_strict_utf8_string_loc

Like C<L</is_strict_utf8_string>> but stores the location of the failure (in the
case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer.

See also C<L</is_strict_utf8_string_loclen>>.

=cut
*/

#define is_strict_utf8_string_loc(s, len, ep)                               \
                                is_strict_utf8_string_loclen(s, len, ep, 0)

/*

=for apidoc is_strict_utf8_string_loclen

Like C<L</is_strict_utf8_string>> but stores the location of the failure (in the
case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer, and the number of UTF-8
encoded characters in the C<el> pointer.

See also C<L</is_strict_utf8_string_loc>>.

=cut
*/

PERL_STATIC_INLINE bool
S_is_strict_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_STRICT_UTF8_STRING_LOCLEN;

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* const send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isSTRICT_UTF8_CHAR(x, send);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

/*

=for apidoc is_c9strict_utf8_string_loc

Like C<L</is_c9strict_utf8_string>> but stores the location of the failure (in
the case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer.

See also C<L</is_c9strict_utf8_string_loclen>>.

=cut
*/

#define is_c9strict_utf8_string_loc(s, len, ep)	                            \
                            is_c9strict_utf8_string_loclen(s, len, ep, 0)

/*

=for apidoc is_c9strict_utf8_string_loclen

Like C<L</is_c9strict_utf8_string>> but stores the location of the failure (in
the case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer, and the number of UTF-8 encoded
characters in the C<el> pointer.

See also C<L</is_c9strict_utf8_string_loc>>.

=cut
*/

PERL_STATIC_INLINE bool
S_is_c9strict_utf8_string_loclen(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_C9STRICT_UTF8_STRING_LOCLEN;

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* const send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isC9_STRICT_UTF8_CHAR(x, send);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

/*

=for apidoc is_utf8_string_loc_flags

Like C<L</is_utf8_string_flags>> but stores the location of the failure (in the
case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer.

See also C<L</is_utf8_string_loclen_flags>>.

=cut
*/

#define is_utf8_string_loc_flags(s, len, ep, flags)                         \
                        is_utf8_string_loclen_flags(s, len, ep, 0, flags)


/* The above 3 actual functions could have been moved into the more general one
 * just below, and made #defines that call it with the right 'flags'.  They are
 * currently kept separate to increase their chances of getting inlined */

/*

=for apidoc is_utf8_string_loclen_flags

Like C<L</is_utf8_string_flags>> but stores the location of the failure (in the
case of "utf8ness failure") or the location C<s>+C<len> (in the case of
"utf8ness success") in the C<ep> pointer, and the number of UTF-8
encoded characters in the C<el> pointer.

See also C<L</is_utf8_string_loc_flags>>.

=cut
*/

PERL_STATIC_INLINE bool
S_is_utf8_string_loclen_flags(const U8 *s, STRLEN len, const U8 **ep, STRLEN *el, const U32 flags)
{
    const U8 * first_variant;

    PERL_ARGS_ASSERT_IS_UTF8_STRING_LOCLEN_FLAGS;
    assert(0 == (flags & ~(UTF8_DISALLOW_ILLEGAL_INTERCHANGE
                          |UTF8_DISALLOW_PERL_EXTENDED)));

    if (len == 0) {
        len = strlen((const char *) s);
    }

    if (flags == 0) {
        return is_utf8_string_loclen(s, len, ep, el);
    }

    if ((flags & ~UTF8_DISALLOW_PERL_EXTENDED)
                                        == UTF8_DISALLOW_ILLEGAL_INTERCHANGE)
    {
        return is_strict_utf8_string_loclen(s, len, ep, el);
    }

    if ((flags & ~UTF8_DISALLOW_PERL_EXTENDED)
                                    == UTF8_DISALLOW_ILLEGAL_C9_INTERCHANGE)
    {
        return is_c9strict_utf8_string_loclen(s, len, ep, el);
    }

    if (is_utf8_invariant_string_loc(s, len, &first_variant)) {
        if (el)
            *el = len;

        if (ep) {
            *ep = s + len;
        }

        return TRUE;
    }

    {
        const U8* send = s + len;
        const U8* x = first_variant;
        STRLEN outlen = first_variant - s;

        while (x < send) {
            const STRLEN cur_len = isUTF8_CHAR_flags(x, send, flags);
            if (UNLIKELY(! cur_len)) {
                break;
            }
            x += cur_len;
            outlen++;
        }

        if (el)
            *el = outlen;

        if (ep) {
            *ep = x;
        }

        return (x == send);
    }
}

/*
=for apidoc utf8_distance

Returns the number of UTF-8 characters between the UTF-8 pointers C<a>
and C<b>.

WARNING: use only if you *know* that the pointers point inside the
same UTF-8 buffer.

=cut
*/

PERL_STATIC_INLINE IV
Perl_utf8_distance(pTHX_ const U8 *a, const U8 *b)
{
    PERL_ARGS_ASSERT_UTF8_DISTANCE;

    return (a < b) ? -1 * (IV) utf8_length(a, b) : (IV) utf8_length(b, a);
}

/*
=for apidoc utf8_hop

Return the UTF-8 pointer C<s> displaced by C<off> characters, either
forward or backward.

WARNING: do not use the following unless you *know* C<off> is within
the UTF-8 data pointed to by C<s> *and* that on entry C<s> is aligned
on the first byte of character or just after the last byte of a character.

=cut
*/

PERL_STATIC_INLINE U8 *
Perl_utf8_hop(const U8 *s, SSize_t off)
{
    PERL_ARGS_ASSERT_UTF8_HOP;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (off >= 0) {
	while (off--)
	    s += UTF8SKIP(s);
    }
    else {
	while (off++) {
	    s--;
	    while (UTF8_IS_CONTINUATION(*s))
		s--;
	}
    }
    GCC_DIAG_IGNORE(-Wcast-qual)
    return (U8 *)s;
    GCC_DIAG_RESTORE
}

/*
=for apidoc utf8_hop_forward

Return the UTF-8 pointer C<s> displaced by up to C<off> characters,
forward.

C<off> must be non-negative.

C<s> must be before or equal to C<end>.

When moving forward it will not move beyond C<end>.

Will not exceed this limit even if the string is not valid "UTF-8".

=cut
*/

PERL_STATIC_INLINE U8 *
Perl_utf8_hop_forward(const U8 *s, SSize_t off, const U8 *end)
{
    PERL_ARGS_ASSERT_UTF8_HOP_FORWARD;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    assert(s <= end);
    assert(off >= 0);

    while (off--) {
        STRLEN skip = UTF8SKIP(s);
        if ((STRLEN)(end - s) <= skip) {
            GCC_DIAG_IGNORE(-Wcast-qual)
            return (U8 *)end;
            GCC_DIAG_RESTORE
        }
        s += skip;
    }

    GCC_DIAG_IGNORE(-Wcast-qual)
    return (U8 *)s;
    GCC_DIAG_RESTORE
}

/*
=for apidoc utf8_hop_back

Return the UTF-8 pointer C<s> displaced by up to C<off> characters,
backward.

C<off> must be non-positive.

C<s> must be after or equal to C<start>.

When moving backward it will not move before C<start>.

Will not exceed this limit even if the string is not valid "UTF-8".

=cut
*/

PERL_STATIC_INLINE U8 *
Perl_utf8_hop_back(const U8 *s, SSize_t off, const U8 *start)
{
    PERL_ARGS_ASSERT_UTF8_HOP_BACK;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    assert(start <= s);
    assert(off <= 0);

    while (off++ && s > start) {
        s--;
        while (UTF8_IS_CONTINUATION(*s) && s > start)
            s--;
    }
    
    GCC_DIAG_IGNORE(-Wcast-qual)
    return (U8 *)s;
    GCC_DIAG_RESTORE
}

/*
=for apidoc utf8_hop_safe

Return the UTF-8 pointer C<s> displaced by up to C<off> characters,
either forward or backward.

When moving backward it will not move before C<start>.

When moving forward it will not move beyond C<end>.

Will not exceed those limits even if the string is not valid "UTF-8".

=cut
*/

PERL_STATIC_INLINE U8 *
Perl_utf8_hop_safe(const U8 *s, SSize_t off, const U8 *start, const U8 *end)
{
    PERL_ARGS_ASSERT_UTF8_HOP_SAFE;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    assert(start <= s && s <= end);

    if (off >= 0) {
        return utf8_hop_forward(s, off, end);
    }
    else {
        return utf8_hop_back(s, off, start);
    }
}

/*

=for apidoc is_utf8_valid_partial_char

Returns 0 if the sequence of bytes starting at C<s> and looking no further than
S<C<e - 1>> is the UTF-8 encoding, as extended by Perl, for one or more code
points.  Otherwise, it returns 1 if there exists at least one non-empty
sequence of bytes that when appended to sequence C<s>, starting at position
C<e> causes the entire sequence to be the well-formed UTF-8 of some code point;
otherwise returns 0.

In other words this returns TRUE if C<s> points to a partial UTF-8-encoded code
point.

This is useful when a fixed-length buffer is being tested for being well-formed
UTF-8, but the final few bytes in it don't comprise a full character; that is,
it is split somewhere in the middle of the final code point's UTF-8
representation.  (Presumably when the buffer is refreshed with the next chunk
of data, the new first bytes will complete the partial code point.)   This
function is used to verify that the final bytes in the current buffer are in
fact the legal beginning of some code point, so that if they aren't, the
failure can be signalled without having to wait for the next read.

=cut
*/
#define is_utf8_valid_partial_char(s, e)                                    \
                                is_utf8_valid_partial_char_flags(s, e, 0)

/*

=for apidoc is_utf8_valid_partial_char_flags

Like C<L</is_utf8_valid_partial_char>>, it returns a boolean giving whether
or not the input is a valid UTF-8 encoded partial character, but it takes an
extra parameter, C<flags>, which can further restrict which code points are
considered valid.

If C<flags> is 0, this behaves identically to
C<L</is_utf8_valid_partial_char>>.  Otherwise C<flags> can be any combination
of the C<UTF8_DISALLOW_I<foo>> flags accepted by C<L</utf8n_to_uvchr>>.  If
there is any sequence of bytes that can complete the input partial character in
such a way that a non-prohibited character is formed, the function returns
TRUE; otherwise FALSE.  Non character code points cannot be determined based on
partial character input.  But many  of the other possible excluded types can be
determined from just the first one or two bytes.

=cut
 */

PERL_STATIC_INLINE bool
S_is_utf8_valid_partial_char_flags(const U8 * const s, const U8 * const e, const U32 flags)
{
    PERL_ARGS_ASSERT_IS_UTF8_VALID_PARTIAL_CHAR_FLAGS;

    assert(0 == (flags & ~(UTF8_DISALLOW_ILLEGAL_INTERCHANGE
                          |UTF8_DISALLOW_PERL_EXTENDED)));

    if (s >= e || s + UTF8SKIP(s) <= e) {
        return FALSE;
    }

    return cBOOL(_is_utf8_char_helper(s, e, flags));
}

/*

=for apidoc is_utf8_fixed_width_buf_flags

Returns TRUE if the fixed-width buffer starting at C<s> with length C<len>
is entirely valid UTF-8, subject to the restrictions given by C<flags>;
otherwise it returns FALSE.

If C<flags> is 0, any well-formed UTF-8, as extended by Perl, is accepted
without restriction.  If the final few bytes of the buffer do not form a
complete code point, this will return TRUE anyway, provided that
C<L</is_utf8_valid_partial_char_flags>> returns TRUE for them.

If C<flags> in non-zero, it can be any combination of the
C<UTF8_DISALLOW_I<foo>> flags accepted by C<L</utf8n_to_uvchr>>, and with the
same meanings.

This function differs from C<L</is_utf8_string_flags>> only in that the latter
returns FALSE if the final few bytes of the string don't form a complete code
point.

=cut
 */
#define is_utf8_fixed_width_buf_flags(s, len, flags)                        \
                is_utf8_fixed_width_buf_loclen_flags(s, len, 0, 0, flags)

/*

=for apidoc is_utf8_fixed_width_buf_loc_flags

Like C<L</is_utf8_fixed_width_buf_flags>> but stores the location of the
failure in the C<ep> pointer.  If the function returns TRUE, C<*ep> will point
to the beginning of any partial character at the end of the buffer; if there is
no partial character C<*ep> will contain C<s>+C<len>.

See also C<L</is_utf8_fixed_width_buf_loclen_flags>>.

=cut
*/

#define is_utf8_fixed_width_buf_loc_flags(s, len, loc, flags)               \
                is_utf8_fixed_width_buf_loclen_flags(s, len, loc, 0, flags)

/*

=for apidoc is_utf8_fixed_width_buf_loclen_flags

Like C<L</is_utf8_fixed_width_buf_loc_flags>> but stores the number of
complete, valid characters found in the C<el> pointer.

=cut
*/

PERL_STATIC_INLINE bool
S_is_utf8_fixed_width_buf_loclen_flags(const U8 * const s,
                                       STRLEN len,
                                       const U8 **ep,
                                       STRLEN *el,
                                       const U32 flags)
{
    const U8 * maybe_partial;

    PERL_ARGS_ASSERT_IS_UTF8_FIXED_WIDTH_BUF_LOCLEN_FLAGS;

    if (! ep) {
        ep  = &maybe_partial;
    }

    /* If it's entirely valid, return that; otherwise see if the only error is
     * that the final few bytes are for a partial character */
    return    is_utf8_string_loclen_flags(s, len, ep, el, flags)
           || is_utf8_valid_partial_char_flags(*ep, s + len, flags);
}

/* ------------------------------- perl.h ----------------------------- */

/*
=head1 Miscellaneous Functions

=for apidoc AiR|bool|is_safe_syscall|const char *pv|STRLEN len|const char *what|const char *op_name

Test that the given C<pv> doesn't contain any internal C<NUL> characters.
If it does, set C<errno> to C<ENOENT>, optionally warn, and return FALSE.

Return TRUE if the name is safe.

Used by the C<IS_SAFE_SYSCALL()> macro.

=cut
*/

PERL_STATIC_INLINE bool
S_is_safe_syscall(pTHX_ const char *pv, STRLEN len, const char *what, const char *op_name) {
    /* While the Windows CE API provides only UCS-16 (or UTF-16) APIs
     * perl itself uses xce*() functions which accept 8-bit strings.
     */

    PERL_ARGS_ASSERT_IS_SAFE_SYSCALL;

    if (len > 1) {
        char *null_at;
        if (UNLIKELY((null_at = (char *)memchr(pv, 0, len-1)) != NULL)) {
                SETERRNO(ENOENT, LIB_INVARG);
                Perl_ck_warner(aTHX_ packWARN(WARN_SYSCALLS),
                                   "Invalid \\0 character in %s for %s: %s\\0%s",
                                   what, op_name, pv, null_at+1);
                return FALSE;
        }
    }

    return TRUE;
}

/*

Return true if the supplied filename has a newline character
immediately before the first (hopefully only) NUL.

My original look at this incorrectly used the len from SvPV(), but
that's incorrect, since we allow for a NUL in pv[len-1].

So instead, strlen() and work from there.

This allow for the user reading a filename, forgetting to chomp it,
then calling:

  open my $foo, "$file\0";

*/

#ifdef PERL_CORE

PERL_STATIC_INLINE bool
S_should_warn_nl(const char *pv) {
    STRLEN len;

    PERL_ARGS_ASSERT_SHOULD_WARN_NL;

    len = strlen(pv);

    return len > 0 && pv[len-1] == '\n';
}

#endif

/* ------------------ pp.c, regcomp.c, toke.c, universal.c ------------ */

#define MAX_CHARSET_NAME_LENGTH 2

PERL_STATIC_INLINE const char *
get_regex_charset_name(const U32 flags, STRLEN* const lenp)
{
    /* Returns a string that corresponds to the name of the regex character set
     * given by 'flags', and *lenp is set the length of that string, which
     * cannot exceed MAX_CHARSET_NAME_LENGTH characters */

    *lenp = 1;
    switch (get_regex_charset(flags)) {
        case REGEX_DEPENDS_CHARSET: return DEPENDS_PAT_MODS;
        case REGEX_LOCALE_CHARSET:  return LOCALE_PAT_MODS;
        case REGEX_UNICODE_CHARSET: return UNICODE_PAT_MODS;
	case REGEX_ASCII_RESTRICTED_CHARSET: return ASCII_RESTRICT_PAT_MODS;
	case REGEX_ASCII_MORE_RESTRICTED_CHARSET:
	    *lenp = 2;
	    return ASCII_MORE_RESTRICT_PAT_MODS;
    }
    /* The NOT_REACHED; hides an assert() which has a rather complex
     * definition in perl.h. */
    NOT_REACHED; /* NOTREACHED */
    return "?";	    /* Unknown */
}

/*

Return false if any get magic is on the SV other than taint magic.

*/

PERL_STATIC_INLINE bool
S_sv_only_taint_gmagic(SV *sv) {
    MAGIC *mg = SvMAGIC(sv);

    PERL_ARGS_ASSERT_SV_ONLY_TAINT_GMAGIC;

    while (mg) {
        if (mg->mg_type != PERL_MAGIC_taint
            && !(mg->mg_flags & MGf_GSKIP)
            && mg->mg_virtual->svt_get) {
            return FALSE;
        }
        mg = mg->mg_moremagic;
    }

    return TRUE;
}

/* ------------------ cop.h ------------------------------------------- */


/* Enter a block. Push a new base context and return its address. */

PERL_STATIC_INLINE PERL_CONTEXT *
S_cx_pushblock(pTHX_ U8 type, U8 gimme, SV** sp, I32 saveix)
{
    PERL_CONTEXT * cx;

    PERL_ARGS_ASSERT_CX_PUSHBLOCK;

    CXINC;
    cx = CX_CUR();
    cx->cx_type        = type;
    cx->blk_gimme      = gimme;
    cx->blk_oldsaveix  = saveix;
    cx->blk_oldsp      = (I32)(sp - PL_stack_base);
    cx->blk_oldcop     = PL_curcop;
    cx->blk_oldmarksp  = (I32)(PL_markstack_ptr - PL_markstack);
    cx->blk_oldscopesp = PL_scopestack_ix;
    cx->blk_oldpm      = PL_curpm;
    cx->blk_old_tmpsfloor = PL_tmps_floor;

    PL_tmps_floor        = PL_tmps_ix;
    CX_DEBUG(cx, "PUSH");
    return cx;
}


/* Exit a block (RETURN and LAST). */

PERL_STATIC_INLINE void
S_cx_popblock(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPBLOCK;

    CX_DEBUG(cx, "POP");
    /* these 3 are common to cx_popblock and cx_topblock */
    PL_markstack_ptr = PL_markstack + cx->blk_oldmarksp;
    PL_scopestack_ix = cx->blk_oldscopesp;
    PL_curpm         = cx->blk_oldpm;

    /* LEAVE_SCOPE() should have made this true. /(?{})/ cheats
     * and leaves a CX entry lying around for repeated use, so
     * skip for multicall */                  \
    assert(   (CxTYPE(cx) == CXt_SUB && CxMULTICALL(cx))
            || PL_savestack_ix == cx->blk_oldsaveix);
    PL_curcop     = cx->blk_oldcop;
    PL_tmps_floor = cx->blk_old_tmpsfloor;
}

/* Continue a block elsewhere (e.g. NEXT, REDO, GOTO).
 * Whereas cx_popblock() restores the state to the point just before
 * cx_pushblock() was called,  cx_topblock() restores it to the point just
 * *after* cx_pushblock() was called. */

PERL_STATIC_INLINE void
S_cx_topblock(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_TOPBLOCK;

    CX_DEBUG(cx, "TOP");
    /* these 3 are common to cx_popblock and cx_topblock */
    PL_markstack_ptr = PL_markstack + cx->blk_oldmarksp;
    PL_scopestack_ix = cx->blk_oldscopesp;
    PL_curpm         = cx->blk_oldpm;

    PL_stack_sp      = PL_stack_base + cx->blk_oldsp;
}


PERL_STATIC_INLINE void
S_cx_pushsub(pTHX_ PERL_CONTEXT *cx, CV *cv, OP *retop, bool hasargs)
{
    U8 phlags = CX_PUSHSUB_GET_LVALUE_MASK(Perl_was_lvalue_sub);

    PERL_ARGS_ASSERT_CX_PUSHSUB;

    PERL_DTRACE_PROBE_ENTRY(cv);
    cx->blk_sub.cv = cv;
    cx->blk_sub.olddepth = CvDEPTH(cv);
    cx->blk_sub.prevcomppad = PL_comppad;
    cx->cx_type |= (hasargs) ? CXp_HASARGS : 0;
    cx->blk_sub.retop = retop;
    SvREFCNT_inc_simple_void_NN(cv);
    cx->blk_u16 = PL_op->op_private & (phlags|OPpDEREF);
}


/* subsets of cx_popsub() */

PERL_STATIC_INLINE void
S_cx_popsub_common(pTHX_ PERL_CONTEXT *cx)
{
    CV *cv;

    PERL_ARGS_ASSERT_CX_POPSUB_COMMON;
    assert(CxTYPE(cx) == CXt_SUB);

    PL_comppad = cx->blk_sub.prevcomppad;
    PL_curpad = LIKELY(PL_comppad) ? AvARRAY(PL_comppad) : NULL;
    cv = cx->blk_sub.cv;
    CvDEPTH(cv) = cx->blk_sub.olddepth;
    cx->blk_sub.cv = NULL;
    SvREFCNT_dec(cv);
}


/* handle the @_ part of leaving a sub */

PERL_STATIC_INLINE void
S_cx_popsub_args(pTHX_ PERL_CONTEXT *cx)
{
    AV *av;

    PERL_ARGS_ASSERT_CX_POPSUB_ARGS;
    assert(CxTYPE(cx) == CXt_SUB);
    assert(AvARRAY(MUTABLE_AV(
        PadlistARRAY(CvPADLIST(cx->blk_sub.cv))[
                CvDEPTH(cx->blk_sub.cv)])) == PL_curpad);

    CX_POP_SAVEARRAY(cx);
    av = MUTABLE_AV(PAD_SVl(0));
    if (UNLIKELY(AvREAL(av)))
        /* abandon @_ if it got reified */
        clear_defarray(av, 0);
    else {
        CLEAR_ARGARRAY(av);
    }
}


PERL_STATIC_INLINE void
S_cx_popsub(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPSUB;
    assert(CxTYPE(cx) == CXt_SUB);

    PERL_DTRACE_PROBE_RETURN(cx->blk_sub.cv);

    if (CxHASARGS(cx))
        cx_popsub_args(cx);
    cx_popsub_common(cx);
}


PERL_STATIC_INLINE void
S_cx_pushformat(pTHX_ PERL_CONTEXT *cx, CV *cv, OP *retop, GV *gv)
{
    PERL_ARGS_ASSERT_CX_PUSHFORMAT;

    cx->blk_format.cv          = cv;
    cx->blk_format.retop       = retop;
    cx->blk_format.gv          = gv;
    cx->blk_format.dfoutgv     = PL_defoutgv;
    cx->blk_format.prevcomppad = PL_comppad;
    cx->blk_u16                = 0;

    SvREFCNT_inc_simple_void_NN(cv);
    CvDEPTH(cv)++;
    SvREFCNT_inc_void(cx->blk_format.dfoutgv);
}


PERL_STATIC_INLINE void
S_cx_popformat(pTHX_ PERL_CONTEXT *cx)
{
    CV *cv;
    GV *dfout;

    PERL_ARGS_ASSERT_CX_POPFORMAT;
    assert(CxTYPE(cx) == CXt_FORMAT);

    dfout = cx->blk_format.dfoutgv;
    setdefout(dfout);
    cx->blk_format.dfoutgv = NULL;
    SvREFCNT_dec_NN(dfout);

    PL_comppad = cx->blk_format.prevcomppad;
    PL_curpad = LIKELY(PL_comppad) ? AvARRAY(PL_comppad) : NULL;
    cv = cx->blk_format.cv;
    cx->blk_format.cv = NULL;
    --CvDEPTH(cv);
    SvREFCNT_dec_NN(cv);
}


PERL_STATIC_INLINE void
S_cx_pusheval(pTHX_ PERL_CONTEXT *cx, OP *retop, SV *namesv)
{
    PERL_ARGS_ASSERT_CX_PUSHEVAL;

    cx->blk_eval.retop         = retop;
    cx->blk_eval.old_namesv    = namesv;
    cx->blk_eval.old_eval_root = PL_eval_root;
    cx->blk_eval.cur_text      = PL_parser ? PL_parser->linestr : NULL;
    cx->blk_eval.cv            = NULL; /* later set by doeval_compile() */
    cx->blk_eval.cur_top_env   = PL_top_env;

    assert(!(PL_in_eval     & ~ 0x3F));
    assert(!(PL_op->op_type & ~0x1FF));
    cx->blk_u16 = (PL_in_eval & 0x3F) | ((U16)PL_op->op_type << 7);
}


PERL_STATIC_INLINE void
S_cx_popeval(pTHX_ PERL_CONTEXT *cx)
{
    SV *sv;

    PERL_ARGS_ASSERT_CX_POPEVAL;
    assert(CxTYPE(cx) == CXt_EVAL);

    PL_in_eval = CxOLD_IN_EVAL(cx);
    assert(!(PL_in_eval & 0xc0));
    PL_eval_root = cx->blk_eval.old_eval_root;
    sv = cx->blk_eval.cur_text;
    if (sv && CxEVAL_TXT_REFCNTED(cx)) {
        cx->blk_eval.cur_text = NULL;
        SvREFCNT_dec_NN(sv);
    }

    sv = cx->blk_eval.old_namesv;
    if (sv) {
        cx->blk_eval.old_namesv = NULL;
        SvREFCNT_dec_NN(sv);
    }
}


/* push a plain loop, i.e.
 *     { block }
 *     while (cond) { block }
 *     for (init;cond;continue) { block }
 * This loop can be last/redo'ed etc.
 */

PERL_STATIC_INLINE void
S_cx_pushloop_plain(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_PUSHLOOP_PLAIN;
    cx->blk_loop.my_op = cLOOP;
}


/* push a true for loop, i.e.
 *     for var (list) { block }
 */

PERL_STATIC_INLINE void
S_cx_pushloop_for(pTHX_ PERL_CONTEXT *cx, void *itervarp, SV* itersave)
{
    PERL_ARGS_ASSERT_CX_PUSHLOOP_FOR;

    /* this one line is common with cx_pushloop_plain */
    cx->blk_loop.my_op = cLOOP;

    cx->blk_loop.itervar_u.svp = (SV**)itervarp;
    cx->blk_loop.itersave      = itersave;
#ifdef USE_ITHREADS
    cx->blk_loop.oldcomppad = PL_comppad;
#endif
}


/* pop all loop types, including plain */

PERL_STATIC_INLINE void
S_cx_poploop(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPLOOP;

    assert(CxTYPE_is_LOOP(cx));
    if (  CxTYPE(cx) == CXt_LOOP_ARY
       || CxTYPE(cx) == CXt_LOOP_LAZYSV)
    {
        /* Free ary or cur. This assumes that state_u.ary.ary
         * aligns with state_u.lazysv.cur. See cx_dup() */
        SV *sv = cx->blk_loop.state_u.lazysv.cur;
        cx->blk_loop.state_u.lazysv.cur = NULL;
        SvREFCNT_dec_NN(sv);
        if (CxTYPE(cx) == CXt_LOOP_LAZYSV) {
            sv = cx->blk_loop.state_u.lazysv.end;
            cx->blk_loop.state_u.lazysv.end = NULL;
            SvREFCNT_dec_NN(sv);
        }
    }
    if (cx->cx_type & (CXp_FOR_PAD|CXp_FOR_GV)) {
        SV *cursv;
        SV **svp = (cx)->blk_loop.itervar_u.svp;
        if ((cx->cx_type & CXp_FOR_GV))
            svp = &GvSV((GV*)svp);
        cursv = *svp;
        *svp = cx->blk_loop.itersave;
        cx->blk_loop.itersave = NULL;
        SvREFCNT_dec(cursv);
    }
}


PERL_STATIC_INLINE void
S_cx_pushwhen(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_PUSHWHEN;

    cx->blk_givwhen.leave_op = cLOGOP->op_other;
}


PERL_STATIC_INLINE void
S_cx_popwhen(pTHX_ PERL_CONTEXT *cx)
{
    PERL_ARGS_ASSERT_CX_POPWHEN;
    assert(CxTYPE(cx) == CXt_WHEN);

    PERL_UNUSED_ARG(cx);
    PERL_UNUSED_CONTEXT;
    /* currently NOOP */
}


PERL_STATIC_INLINE void
S_cx_pushgiven(pTHX_ PERL_CONTEXT *cx, SV *orig_defsv)
{
    PERL_ARGS_ASSERT_CX_PUSHGIVEN;

    cx->blk_givwhen.leave_op = cLOGOP->op_other;
    cx->blk_givwhen.defsv_save = orig_defsv;
}


PERL_STATIC_INLINE void
S_cx_popgiven(pTHX_ PERL_CONTEXT *cx)
{
    SV *sv;

    PERL_ARGS_ASSERT_CX_POPGIVEN;
    assert(CxTYPE(cx) == CXt_GIVEN);

    sv = GvSV(PL_defgv);
    GvSV(PL_defgv) = cx->blk_givwhen.defsv_save;
    cx->blk_givwhen.defsv_save = NULL;
    SvREFCNT_dec(sv);
}

/* ------------------ util.h ------------------------------------------- */

/*
=head1 Miscellaneous Functions

=for apidoc foldEQ

Returns true if the leading C<len> bytes of the strings C<s1> and C<s2> are the
same
case-insensitively; false otherwise.  Uppercase and lowercase ASCII range bytes
match themselves and their opposite case counterparts.  Non-cased and non-ASCII
range bytes match only themselves.

=cut
*/

PERL_STATIC_INLINE I32
Perl_foldEQ(const char *s1, const char *s2, I32 len)
{
    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ;

    assert(len >= 0);

    while (len--) {
	if (*a != *b && *a != PL_fold[*b])
	    return 0;
	a++,b++;
    }
    return 1;
}

PERL_STATIC_INLINE I32
Perl_foldEQ_latin1(const char *s1, const char *s2, I32 len)
{
    /* Compare non-utf8 using Unicode (Latin1) semantics.  Does not work on
     * MICRO_SIGN, LATIN_SMALL_LETTER_SHARP_S, nor
     * LATIN_SMALL_LETTER_Y_WITH_DIAERESIS, and does not check for these.  Nor
     * does it check that the strings each have at least 'len' characters */

    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ_LATIN1;

    assert(len >= 0);

    while (len--) {
	if (*a != *b && *a != PL_fold_latin1[*b]) {
	    return 0;
	}
	a++, b++;
    }
    return 1;
}

/*
=for apidoc foldEQ_locale

Returns true if the leading C<len> bytes of the strings C<s1> and C<s2> are the
same case-insensitively in the current locale; false otherwise.

=cut
*/

PERL_STATIC_INLINE I32
Perl_foldEQ_locale(const char *s1, const char *s2, I32 len)
{
    dVAR;
    const U8 *a = (const U8 *)s1;
    const U8 *b = (const U8 *)s2;

    PERL_ARGS_ASSERT_FOLDEQ_LOCALE;

    assert(len >= 0);

    while (len--) {
	if (*a != *b && *a != PL_fold_locale[*b])
	    return 0;
	a++,b++;
    }
    return 1;
}

#if ! defined (HAS_MEMRCHR) && (defined(PERL_CORE) || defined(PERL_EXT))

PERL_STATIC_INLINE void *
S_my_memrchr(const char * s, const char c, const STRLEN len)
{
    /* memrchr(), since many platforms lack it */

    const char * t = s + len - 1;

    PERL_ARGS_ASSERT_MY_MEMRCHR;

    while (t >= s) {
        if (*t == c) {
            return (void *) t;
        }
        t--;
    }

    return NULL;
}

#endif

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
