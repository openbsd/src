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

/* ----------------------------- regexp.h ----------------------------- */

PERL_STATIC_INLINE struct regexp *
S_ReANY(const REGEXP * const re)
{
    assert(isREGEXP(re));
    return re->sv_u.svu_rx;
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
S_SvPADTMP_on(SV *sv)
{
    assert(!(SvFLAGS(sv) & SVs_PADMY));
    return SvFLAGS(sv) |= SVs_PADTMP;
}
PERL_STATIC_INLINE U32
S_SvPADTMP_off(SV *sv)
{
    assert(!(SvFLAGS(sv) & SVs_PADMY));
    return SvFLAGS(sv) &= ~SVs_PADTMP;
}
PERL_STATIC_INLINE U32
S_SvPADSTALE_on(SV *sv)
{
    assert(SvFLAGS(sv) & SVs_PADMY);
    return SvFLAGS(sv) |= SVs_PADSTALE;
}
PERL_STATIC_INLINE U32
S_SvPADSTALE_off(SV *sv)
{
    assert(SvFLAGS(sv) & SVs_PADMY);
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
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
static void
S_croak_memory_wrap(void)
{
    Perl_croak_nocontext("%s",PL_memory_wrap);
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef BOOTSTRAP_CHARSET
static bool
S_bootstrap_ctype(U8 character, UV classnum, bool full_Latin1)
{
    /* See comments in handy.h.  This is placed in this file primarily to avoid
     * having to have an entry for it in embed.fnc */

    dTHX;

    if (! full_Latin1 && ! isASCII(character)) {
        return FALSE;
    }

    switch (classnum) {
        case _CC_ALPHANUMERIC: return isALPHANUMERIC_L1(character);
        case _CC_ALPHA:     return isALPHA_L1(character);
        case _CC_ASCII:     return isASCII_L1(character);
        case _CC_BLANK:     return isBLANK_L1(character);
        case _CC_CASED:     return isLOWER_L1(character)
                                   || isUPPER_L1(character);
        case _CC_CNTRL:     return isCNTRL_L1(character);
        case _CC_DIGIT:     return isDIGIT_L1(character);
        case _CC_GRAPH:     return isGRAPH_L1(character);
        case _CC_LOWER:     return isLOWER_L1(character);
        case _CC_PRINT:     return isPRINT_L1(character);
        case _CC_PSXSPC:    return isPSXSPC_L1(character);
        case _CC_PUNCT:     return isPUNCT_L1(character);
        case _CC_SPACE:     return isSPACE_L1(character);
        case _CC_UPPER:     return isUPPER_L1(character);
        case _CC_WORDCHAR:  return isWORDCHAR_L1(character);
        case _CC_XDIGIT:    return isXDIGIT_L1(character);
        case _CC_VERTSPACE: return isSPACE_L1(character) && ! isBLANK_L1(character);
        case _CC_IDFIRST:   return isIDFIRST_L1(character);
        case _CC_QUOTEMETA: return _isQUOTEMETA(character);
        case _CC_CHARNAME_CONT: return isCHARNAME_CONT(character);
        case _CC_NONLATIN1_FOLD: return _HAS_NONLATIN1_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(character);
        case _CC_NON_FINAL_FOLD: return _IS_NON_FINAL_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(character);
        case _CC_IS_IN_SOME_FOLD: return _IS_IN_SOME_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(character);
        case _CC_BACKSLASH_FOO_LBRACE_IS_META: return 0;


        default: break;
    }
    Perl_croak(aTHX_ "panic: bootstrap_ctype() has an unexpected character class '%" UVxf "'", classnum);
}
#endif

/* ------------------------------- utf8.h ------------------------------- */

PERL_STATIC_INLINE void
S_append_utf8_from_native_byte(const U8 byte, U8** dest)
{
    /* Takes an input 'byte' (Latin1 or EBCDIC) and appends it to the UTF-8
     * encoded string at '*dest', updating '*dest' to include it */

    PERL_ARGS_ASSERT_APPEND_UTF8_FROM_NATIVE_BYTE;

    if (NATIVE_BYTE_IS_INVARIANT(byte))
        *(*dest)++ = byte;
    else {
        *(*dest)++ = UTF8_EIGHT_BIT_HI(byte);
        *(*dest)++ = UTF8_EIGHT_BIT_LO(byte);
    }
}

/* These two exist only to replace the macros they formerly were so that their
 * use can be deprecated */

PERL_STATIC_INLINE bool
S_isIDFIRST_lazy(pTHX_ const char* p)
{
    PERL_ARGS_ASSERT_ISIDFIRST_LAZY;

    return isIDFIRST_lazy_if(p,1);
}

PERL_STATIC_INLINE bool
S_isALNUM_lazy(pTHX_ const char* p)
{
    PERL_ARGS_ASSERT_ISALNUM_LAZY;

    return isALNUM_lazy_if(p,1);
}

/* ------------------------------- perl.h ----------------------------- */

/*
=for apidoc AiR|bool|is_safe_syscall|const char *pv|STRLEN len|const char *what|const char *op_name

Test that the given C<pv> doesn't contain any internal C<NUL> characters.
If it does, set C<errno> to ENOENT, optionally warn, and return FALSE.

Return TRUE if the name is safe.

Used by the IS_SAFE_SYSCALL() macro.

=cut
*/

PERL_STATIC_INLINE bool
S_is_safe_syscall(pTHX_ const char *pv, STRLEN len, const char *what, const char *op_name) {
    /* While the Windows CE API provides only UCS-16 (or UTF-16) APIs
     * perl itself uses xce*() functions which accept 8-bit strings.
     */

    PERL_ARGS_ASSERT_IS_SAFE_SYSCALL;

    if (pv && len > 1) {
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

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
