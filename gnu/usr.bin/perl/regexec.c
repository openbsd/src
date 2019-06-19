/*    regexec.c
 */

/*
 *	One Ring to rule them all, One Ring to find them
 *
 *     [p.v of _The Lord of the Rings_, opening poem]
 *     [p.50 of _The Lord of the Rings_, I/iii: "The Shadow of the Past"]
 *     [p.254 of _The Lord of the Rings_, II/ii: "The Council of Elrond"]
 */

/* This file contains functions for executing a regular expression.  See
 * also regcomp.c which funnily enough, contains functions for compiling
 * a regular expression.
 *
 * This file is also copied at build time to ext/re/re_exec.c, where
 * it's built with -DPERL_EXT_RE_BUILD -DPERL_EXT_RE_DEBUG -DPERL_EXT.
 * This causes the main functions to be compiled under new names and with
 * debugging support added, which makes "use re 'debug'" work.
 */

/* NOTE: this is derived from Henry Spencer's regexp code, and should not
 * confused with the original package (see point 3 below).  Thanks, Henry!
 */

/* Additional note: this code is very heavily munged from Henry's version
 * in places.  In some spots I've traded clarity for efficiency, so don't
 * blame Henry for some of the lack of readability.
 */

/* The names of the functions have been changed from regcomp and
 * regexec to  pregcomp and pregexec in order to avoid conflicts
 * with the POSIX routines of the same names.
*/

#ifdef PERL_EXT_RE_BUILD
#include "re_top.h"
#endif

/*
 * pregcomp and pregexec -- regsub and regerror are not used in perl
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 ****    Alterations to Henry's code are...
 ****
 ****    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 ****    2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
 ****    by Larry Wall and others
 ****
 ****    You may distribute under the terms of either the GNU General Public
 ****    License or the Artistic License, as specified in the README file.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include "EXTERN.h"
#define PERL_IN_REGEXEC_C
#include "perl.h"

#ifdef PERL_IN_XSUB_RE
#  include "re_comp.h"
#else
#  include "regcomp.h"
#endif

#include "invlist_inline.h"
#include "unicode_constants.h"

#define B_ON_NON_UTF8_LOCALE_IS_WRONG            \
 "Use of \\b{} or \\B{} for non-UTF-8 locale is wrong.  Assuming a UTF-8 locale"

static const char utf8_locale_required[] =
      "Use of (?[ ]) for non-UTF-8 locale is wrong.  Assuming a UTF-8 locale";

#ifdef DEBUGGING
/* At least one required character in the target string is expressible only in
 * UTF-8. */
static const char* const non_utf8_target_but_utf8_required
                = "Can't match, because target string needs to be in UTF-8\n";
#endif

/* Returns a boolean as to whether the input unsigned number is a power of 2
 * (2**0, 2**1, etc).  In other words if it has just a single bit set.
 * If not, subtracting 1 would leave the uppermost bit set, so the & would
 * yield non-zero */
#define isPOWER_OF_2(n) ((n & (n-1)) == 0)

#define NON_UTF8_TARGET_BUT_UTF8_REQUIRED(target) STMT_START {           \
    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "%s", non_utf8_target_but_utf8_required));\
    goto target;                                                         \
} STMT_END

#define HAS_NONLATIN1_FOLD_CLOSURE(i) _HAS_NONLATIN1_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(i)

#ifndef STATIC
#define	STATIC	static
#endif

/* Valid only if 'c', the character being looke-up, is an invariant under
 * UTF-8: it avoids the reginclass call if there are no complications: i.e., if
 * everything matchable is straight forward in the bitmap */
#define REGINCLASS(prog,p,c,u)  (ANYOF_FLAGS(p)                             \
                                ? reginclass(prog,p,c,c+1,u)                \
                                : ANYOF_BITMAP_TEST(p,*(c)))

/*
 * Forwards.
 */

#define CHR_SVLEN(sv) (utf8_target ? sv_len_utf8(sv) : SvCUR(sv))

#define HOPc(pos,off) \
	(char *)(reginfo->is_utf8_target \
	    ? reghop3((U8*)pos, off, \
                    (U8*)(off >= 0 ? reginfo->strend : reginfo->strbeg)) \
	    : (U8*)(pos + off))

/* like HOPMAYBE3 but backwards. lim must be +ve. Returns NULL on overshoot */
#define HOPBACK3(pos, off, lim) \
	(reginfo->is_utf8_target                          \
	    ? reghopmaybe3((U8*)pos, (SSize_t)0-off, (U8*)(lim)) \
	    : (pos - off >= lim)	                         \
		? (U8*)pos - off		                 \
		: NULL)

#define HOPBACKc(pos, off) ((char*)HOPBACK3(pos, off, reginfo->strbeg))

#define HOP3(pos,off,lim) (reginfo->is_utf8_target  ? reghop3((U8*)(pos), off, (U8*)(lim)) : (U8*)(pos + off))
#define HOP3c(pos,off,lim) ((char*)HOP3(pos,off,lim))

/* lim must be +ve. Returns NULL on overshoot */
#define HOPMAYBE3(pos,off,lim) \
	(reginfo->is_utf8_target                        \
	    ? reghopmaybe3((U8*)pos, off, (U8*)(lim))   \
	    : ((U8*)pos + off <= lim)                   \
		? (U8*)pos + off                        \
		: NULL)

/* like HOP3, but limits the result to <= lim even for the non-utf8 case.
 * off must be >=0; args should be vars rather than expressions */
#define HOP3lim(pos,off,lim) (reginfo->is_utf8_target \
    ? reghop3((U8*)(pos), off, (U8*)(lim)) \
    : (U8*)((pos + off) > lim ? lim : (pos + off)))
#define HOP3clim(pos,off,lim) ((char*)HOP3lim(pos,off,lim))

#define HOP4(pos,off,llim, rlim) (reginfo->is_utf8_target \
    ? reghop4((U8*)(pos), off, (U8*)(llim), (U8*)(rlim)) \
    : (U8*)(pos + off))
#define HOP4c(pos,off,llim, rlim) ((char*)HOP4(pos,off,llim, rlim))

#define NEXTCHR_EOS -10 /* nextchr has fallen off the end */
#define NEXTCHR_IS_EOS (nextchr < 0)

#define SET_nextchr \
    nextchr = ((locinput < reginfo->strend) ? UCHARAT(locinput) : NEXTCHR_EOS)

#define SET_locinput(p) \
    locinput = (p);  \
    SET_nextchr

#define PLACEHOLDER	/* Something for the preprocessor to grab onto */
/* TODO: Combine JUMPABLE and HAS_TEXT to cache OP(rn) */

/* for use after a quantifier and before an EXACT-like node -- japhy */
/* it would be nice to rework regcomp.sym to generate this stuff. sigh
 *
 * NOTE that *nothing* that affects backtracking should be in here, specifically
 * VERBS must NOT be included. JUMPABLE is used to determine  if we can ignore a
 * node that is in between two EXACT like nodes when ascertaining what the required
 * "follow" character is. This should probably be moved to regex compile time
 * although it may be done at run time beause of the REF possibility - more
 * investigation required. -- demerphq
*/
#define JUMPABLE(rn) (                                                             \
    OP(rn) == OPEN ||                                                              \
    (OP(rn) == CLOSE &&                                                            \
     !EVAL_CLOSE_PAREN_IS(cur_eval,ARG(rn)) ) ||                                   \
    OP(rn) == EVAL ||                                                              \
    OP(rn) == SUSPEND || OP(rn) == IFMATCH ||                                      \
    OP(rn) == PLUS || OP(rn) == MINMOD ||                                          \
    OP(rn) == KEEPS ||                                                             \
    (PL_regkind[OP(rn)] == CURLY && ARG1(rn) > 0)                                  \
)
#define IS_EXACT(rn) (PL_regkind[OP(rn)] == EXACT)

#define HAS_TEXT(rn) ( IS_EXACT(rn) || PL_regkind[OP(rn)] == REF )

#if 0 
/* Currently these are only used when PL_regkind[OP(rn)] == EXACT so
   we don't need this definition.  XXX These are now out-of-sync*/
#define IS_TEXT(rn)   ( OP(rn)==EXACT   || OP(rn)==REF   || OP(rn)==NREF   )
#define IS_TEXTF(rn)  ( OP(rn)==EXACTFU || OP(rn)==EXACTFU_SS || OP(rn)==EXACTFAA || OP(rn)==EXACTFAA_NO_TRIE || OP(rn)==EXACTF || OP(rn)==REFF  || OP(rn)==NREFF )
#define IS_TEXTFL(rn) ( OP(rn)==EXACTFL || OP(rn)==REFFL || OP(rn)==NREFFL )

#else
/* ... so we use this as its faster. */
#define IS_TEXT(rn)   ( OP(rn)==EXACT || OP(rn)==EXACTL )
#define IS_TEXTFU(rn)  ( OP(rn)==EXACTFU || OP(rn)==EXACTFLU8 || OP(rn)==EXACTFU_SS || OP(rn) == EXACTFAA || OP(rn) == EXACTFAA_NO_TRIE)
#define IS_TEXTF(rn)  ( OP(rn)==EXACTF  )
#define IS_TEXTFL(rn) ( OP(rn)==EXACTFL )

#endif

/*
  Search for mandatory following text node; for lookahead, the text must
  follow but for lookbehind (rn->flags != 0) we skip to the next step.
*/
#define FIND_NEXT_IMPT(rn) STMT_START {                                   \
    while (JUMPABLE(rn)) { \
	const OPCODE type = OP(rn); \
	if (type == SUSPEND || PL_regkind[type] == CURLY) \
	    rn = NEXTOPER(NEXTOPER(rn)); \
	else if (type == PLUS) \
	    rn = NEXTOPER(rn); \
	else if (type == IFMATCH) \
	    rn = (rn->flags == 0) ? NEXTOPER(NEXTOPER(rn)) : rn + ARG(rn); \
	else rn += NEXT_OFF(rn); \
    } \
} STMT_END 

#define SLAB_FIRST(s) (&(s)->states[0])
#define SLAB_LAST(s)  (&(s)->states[PERL_REGMATCH_SLAB_SLOTS-1])

static void S_setup_eval_state(pTHX_ regmatch_info *const reginfo);
static void S_cleanup_regmatch_info_aux(pTHX_ void *arg);
static regmatch_state * S_push_slab(pTHX);

#define REGCP_PAREN_ELEMS 3
#define REGCP_OTHER_ELEMS 3
#define REGCP_FRAME_ELEMS 1
/* REGCP_FRAME_ELEMS are not part of the REGCP_OTHER_ELEMS and
 * are needed for the regexp context stack bookkeeping. */

STATIC CHECKPOINT
S_regcppush(pTHX_ const regexp *rex, I32 parenfloor, U32 maxopenparen _pDEPTH)
{
    const int retval = PL_savestack_ix;
    const int paren_elems_to_push =
                (maxopenparen - parenfloor) * REGCP_PAREN_ELEMS;
    const UV total_elems = paren_elems_to_push + REGCP_OTHER_ELEMS;
    const UV elems_shifted = total_elems << SAVE_TIGHT_SHIFT;
    I32 p;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGCPPUSH;

    if (paren_elems_to_push < 0)
        Perl_croak(aTHX_ "panic: paren_elems_to_push, %i < 0, maxopenparen: %i parenfloor: %i REGCP_PAREN_ELEMS: %u",
                   (int)paren_elems_to_push, (int)maxopenparen,
                   (int)parenfloor, (unsigned)REGCP_PAREN_ELEMS);

    if ((elems_shifted >> SAVE_TIGHT_SHIFT) != total_elems)
	Perl_croak(aTHX_ "panic: paren_elems_to_push offset %" UVuf
		   " out of range (%lu-%ld)",
		   total_elems,
                   (unsigned long)maxopenparen,
                   (long)parenfloor);

    SSGROW(total_elems + REGCP_FRAME_ELEMS);
    
    DEBUG_BUFFERS_r(
	if ((int)maxopenparen > (int)parenfloor)
            Perl_re_exec_indentf( aTHX_
		"rex=0x%" UVxf " offs=0x%" UVxf ": saving capture indices:\n",
		depth,
                PTR2UV(rex),
		PTR2UV(rex->offs)
	    );
    );
    for (p = parenfloor+1; p <= (I32)maxopenparen;  p++) {
/* REGCP_PARENS_ELEMS are pushed per pairs of parentheses. */
	SSPUSHIV(rex->offs[p].end);
	SSPUSHIV(rex->offs[p].start);
	SSPUSHINT(rex->offs[p].start_tmp);
        DEBUG_BUFFERS_r(Perl_re_exec_indentf( aTHX_
	    "    \\%" UVuf ": %" IVdf "(%" IVdf ")..%" IVdf "\n",
	    depth,
            (UV)p,
	    (IV)rex->offs[p].start,
	    (IV)rex->offs[p].start_tmp,
	    (IV)rex->offs[p].end
	));
    }
/* REGCP_OTHER_ELEMS are pushed in any case, parentheses or no. */
    SSPUSHINT(maxopenparen);
    SSPUSHINT(rex->lastparen);
    SSPUSHINT(rex->lastcloseparen);
    SSPUSHUV(SAVEt_REGCONTEXT | elems_shifted); /* Magic cookie. */

    return retval;
}

/* These are needed since we do not localize EVAL nodes: */
#define REGCP_SET(cp)                                           \
    DEBUG_STATE_r(                                              \
        Perl_re_exec_indentf( aTHX_                             \
            "Setting an EVAL scope, savestack=%" IVdf ",\n",    \
            depth, (IV)PL_savestack_ix                          \
        )                                                       \
    );                                                          \
    cp = PL_savestack_ix

#define REGCP_UNWIND(cp)                                        \
    DEBUG_STATE_r(                                              \
        if (cp != PL_savestack_ix)                              \
            Perl_re_exec_indentf( aTHX_                         \
                "Clearing an EVAL scope, savestack=%"           \
                IVdf "..%" IVdf "\n",                           \
                depth, (IV)(cp), (IV)PL_savestack_ix            \
            )                                                   \
    );                                                          \
    regcpblow(cp)

#define UNWIND_PAREN(lp, lcp)               \
    for (n = rex->lastparen; n > lp; n--)   \
        rex->offs[n].end = -1;              \
    rex->lastparen = n;                     \
    rex->lastcloseparen = lcp;


STATIC void
S_regcppop(pTHX_ regexp *rex, U32 *maxopenparen_p _pDEPTH)
{
    UV i;
    U32 paren;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGCPPOP;

    /* Pop REGCP_OTHER_ELEMS before the parentheses loop starts. */
    i = SSPOPUV;
    assert((i & SAVE_MASK) == SAVEt_REGCONTEXT); /* Check that the magic cookie is there. */
    i >>= SAVE_TIGHT_SHIFT; /* Parentheses elements to pop. */
    rex->lastcloseparen = SSPOPINT;
    rex->lastparen = SSPOPINT;
    *maxopenparen_p = SSPOPINT;

    i -= REGCP_OTHER_ELEMS;
    /* Now restore the parentheses context. */
    DEBUG_BUFFERS_r(
	if (i || rex->lastparen + 1 <= rex->nparens)
            Perl_re_exec_indentf( aTHX_
		"rex=0x%" UVxf " offs=0x%" UVxf ": restoring capture indices to:\n",
		depth,
                PTR2UV(rex),
		PTR2UV(rex->offs)
	    );
    );
    paren = *maxopenparen_p;
    for ( ; i > 0; i -= REGCP_PAREN_ELEMS) {
	SSize_t tmps;
	rex->offs[paren].start_tmp = SSPOPINT;
	rex->offs[paren].start = SSPOPIV;
	tmps = SSPOPIV;
	if (paren <= rex->lastparen)
	    rex->offs[paren].end = tmps;
        DEBUG_BUFFERS_r( Perl_re_exec_indentf( aTHX_
	    "    \\%" UVuf ": %" IVdf "(%" IVdf ")..%" IVdf "%s\n",
	    depth,
            (UV)paren,
	    (IV)rex->offs[paren].start,
	    (IV)rex->offs[paren].start_tmp,
	    (IV)rex->offs[paren].end,
	    (paren > rex->lastparen ? "(skipped)" : ""));
	);
	paren--;
    }
#if 1
    /* It would seem that the similar code in regtry()
     * already takes care of this, and in fact it is in
     * a better location to since this code can #if 0-ed out
     * but the code in regtry() is needed or otherwise tests
     * requiring null fields (pat.t#187 and split.t#{13,14}
     * (as of patchlevel 7877)  will fail.  Then again,
     * this code seems to be necessary or otherwise
     * this erroneously leaves $1 defined: "1" =~ /^(?:(\d)x)?\d$/
     * --jhi updated by dapm */
    for (i = rex->lastparen + 1; i <= rex->nparens; i++) {
	if (i > *maxopenparen_p)
	    rex->offs[i].start = -1;
	rex->offs[i].end = -1;
        DEBUG_BUFFERS_r( Perl_re_exec_indentf( aTHX_
	    "    \\%" UVuf ": %s   ..-1 undeffing\n",
	    depth,
            (UV)i,
	    (i > *maxopenparen_p) ? "-1" : "  "
	));
    }
#endif
}

/* restore the parens and associated vars at savestack position ix,
 * but without popping the stack */

STATIC void
S_regcp_restore(pTHX_ regexp *rex, I32 ix, U32 *maxopenparen_p _pDEPTH)
{
    I32 tmpix = PL_savestack_ix;
    PERL_ARGS_ASSERT_REGCP_RESTORE;

    PL_savestack_ix = ix;
    regcppop(rex, maxopenparen_p);
    PL_savestack_ix = tmpix;
}

#define regcpblow(cp) LEAVE_SCOPE(cp)	/* Ignores regcppush()ed data. */

#ifndef PERL_IN_XSUB_RE

bool
Perl_isFOO_lc(pTHX_ const U8 classnum, const U8 character)
{
    /* Returns a boolean as to whether or not 'character' is a member of the
     * Posix character class given by 'classnum' that should be equivalent to a
     * value in the typedef '_char_class_number'.
     *
     * Ideally this could be replaced by a just an array of function pointers
     * to the C library functions that implement the macros this calls.
     * However, to compile, the precise function signatures are required, and
     * these may vary from platform to to platform.  To avoid having to figure
     * out what those all are on each platform, I (khw) am using this method,
     * which adds an extra layer of function call overhead (unless the C
     * optimizer strips it away).  But we don't particularly care about
     * performance with locales anyway. */

    switch ((_char_class_number) classnum) {
        case _CC_ENUM_ALPHANUMERIC: return isALPHANUMERIC_LC(character);
        case _CC_ENUM_ALPHA:     return isALPHA_LC(character);
        case _CC_ENUM_ASCII:     return isASCII_LC(character);
        case _CC_ENUM_BLANK:     return isBLANK_LC(character);
        case _CC_ENUM_CASED:     return    isLOWER_LC(character)
                                        || isUPPER_LC(character);
        case _CC_ENUM_CNTRL:     return isCNTRL_LC(character);
        case _CC_ENUM_DIGIT:     return isDIGIT_LC(character);
        case _CC_ENUM_GRAPH:     return isGRAPH_LC(character);
        case _CC_ENUM_LOWER:     return isLOWER_LC(character);
        case _CC_ENUM_PRINT:     return isPRINT_LC(character);
        case _CC_ENUM_PUNCT:     return isPUNCT_LC(character);
        case _CC_ENUM_SPACE:     return isSPACE_LC(character);
        case _CC_ENUM_UPPER:     return isUPPER_LC(character);
        case _CC_ENUM_WORDCHAR:  return isWORDCHAR_LC(character);
        case _CC_ENUM_XDIGIT:    return isXDIGIT_LC(character);
        default:    /* VERTSPACE should never occur in locales */
            Perl_croak(aTHX_ "panic: isFOO_lc() has an unexpected character class '%d'", classnum);
    }

    NOT_REACHED; /* NOTREACHED */
    return FALSE;
}

#endif

STATIC bool
S_isFOO_utf8_lc(pTHX_ const U8 classnum, const U8* character, const U8* e)
{
    /* Returns a boolean as to whether or not the (well-formed) UTF-8-encoded
     * 'character' is a member of the Posix character class given by 'classnum'
     * that should be equivalent to a value in the typedef
     * '_char_class_number'.
     *
     * This just calls isFOO_lc on the code point for the character if it is in
     * the range 0-255.  Outside that range, all characters use Unicode
     * rules, ignoring any locale.  So use the Unicode function if this class
     * requires a swash, and use the Unicode macro otherwise. */

    PERL_ARGS_ASSERT_ISFOO_UTF8_LC;

    if (UTF8_IS_INVARIANT(*character)) {
        return isFOO_lc(classnum, *character);
    }
    else if (UTF8_IS_DOWNGRADEABLE_START(*character)) {
        return isFOO_lc(classnum,
                        EIGHT_BIT_UTF8_TO_NATIVE(*character, *(character + 1)));
    }

    _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(character, e);

    switch ((_char_class_number) classnum) {
        case _CC_ENUM_SPACE:     return is_XPERLSPACE_high(character);
        case _CC_ENUM_BLANK:     return is_HORIZWS_high(character);
        case _CC_ENUM_XDIGIT:    return is_XDIGIT_high(character);
        case _CC_ENUM_VERTSPACE: return is_VERTWS_high(character);
        default:
            return _invlist_contains_cp(PL_XPosix_ptrs[classnum],
                                        utf8_to_uvchr_buf(character, e, NULL));
    }

    return FALSE; /* Things like CNTRL are always below 256 */
}

STATIC char *
S_find_next_ascii(char * s, const char * send, const bool utf8_target)
{
    /* Returns the position of the first ASCII byte in the sequence between 's'
     * and 'send-1' inclusive; returns 'send' if none found */

    PERL_ARGS_ASSERT_FIND_NEXT_ASCII;

#ifndef EBCDIC

    if ((STRLEN) (send - s) >= PERL_WORDSIZE

                            /* This term is wordsize if subword; 0 if not */
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(s)

                            /* 'offset' */
                          - (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK))
    {

        /* Process per-byte until reach word boundary.  XXX This loop could be
         * eliminated if we knew that this platform had fast unaligned reads */
        while (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK) {
            if (isASCII(*s)) {
                return s;
            }
            s++;    /* khw didn't bother creating a separate loop for
                       utf8_target */
        }

        /* Here, we know we have at least one full word to process.  Process
         * per-word as long as we have at least a full word left */
        do {
            PERL_UINTMAX_T complemented = ~ * (PERL_UINTMAX_T *) s;
            if (complemented & PERL_VARIANTS_WORD_MASK)  {

#  if   BYTEORDER == 0x1234 || BYTEORDER == 0x12345678    \
     || BYTEORDER == 0x4321 || BYTEORDER == 0x87654321

                s += _variant_byte_number(complemented);
                return s;

#  else   /* If weird byte order, drop into next loop to do byte-at-a-time
           checks. */

                break;
#  endif
            }

            s += PERL_WORDSIZE;

        } while (s + PERL_WORDSIZE <= send);
    }

#endif

    /* Process per-character */
    if (utf8_target) {
        while (s < send) {
            if (isASCII(*s)) {
                return s;
            }
            s += UTF8SKIP(s);
        }
    }
    else {
        while (s < send) {
            if (isASCII(*s)) {
                return s;
            }
            s++;
        }
    }

    return s;
}

STATIC char *
S_find_next_non_ascii(char * s, const char * send, const bool utf8_target)
{
    /* Returns the position of the first non-ASCII byte in the sequence between
     * 's' and 'send-1' inclusive; returns 'send' if none found */

#ifdef EBCDIC

    PERL_ARGS_ASSERT_FIND_NEXT_NON_ASCII;

    if (utf8_target) {
        while (s < send) {
            if ( ! isASCII(*s)) {
                return s;
            }
            s += UTF8SKIP(s);
        }
    }
    else {
        while (s < send) {
            if ( ! isASCII(*s)) {
                return s;
            }
            s++;
        }
    }

    return s;

#else

    const U8 * next_non_ascii = NULL;

    PERL_ARGS_ASSERT_FIND_NEXT_NON_ASCII;
    PERL_UNUSED_ARG(utf8_target);

    /* On ASCII platforms invariants and ASCII are identical, so if the string
     * is entirely invariants, there is no non-ASCII character */
    return (is_utf8_invariant_string_loc((U8 *) s,
                                         (STRLEN) (send - s),
                                         &next_non_ascii))
            ? (char *) send
            : (char *) next_non_ascii;

#endif

}

STATIC U8 *
S_find_span_end(U8 * s, const U8 * send, const U8 span_byte)
{
    /* Returns the position of the first byte in the sequence between 's' and
     * 'send-1' inclusive that isn't 'span_byte'; returns 'send' if none found.
     * */

    PERL_ARGS_ASSERT_FIND_SPAN_END;

    assert(send >= s);

    if ((STRLEN) (send - s) >= PERL_WORDSIZE
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(s)
                          - (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK))
    {
        PERL_UINTMAX_T span_word;

        /* Process per-byte until reach word boundary.  XXX This loop could be
         * eliminated if we knew that this platform had fast unaligned reads */
        while (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK) {
            if (*s != span_byte) {
                return s;
            }
            s++;
        }

        /* Create a word filled with the bytes we are spanning */
        span_word = PERL_COUNT_MULTIPLIER * span_byte;

        /* Process per-word as long as we have at least a full word left */
        do {

            /* Keep going if the whole word is composed of 'span_byte's */
            if ((* (PERL_UINTMAX_T *) s) == span_word)  {
                s += PERL_WORDSIZE;
                continue;
            }

            /* Here, at least one byte in the word isn't 'span_byte'. */

#ifdef EBCDIC

            break;

#else

            /* This xor leaves 1 bits only in those non-matching bytes */
            span_word ^= * (PERL_UINTMAX_T *) s;

            /* Make sure the upper bit of each non-matching byte is set.  This
             * makes each such byte look like an ASCII platform variant byte */
            span_word |= span_word << 1;
            span_word |= span_word << 2;
            span_word |= span_word << 4;

            /* That reduces the problem to what this function solves */
            return s + _variant_byte_number(span_word);

#endif

        } while (s + PERL_WORDSIZE <= send);
    }

    /* Process the straggler bytes beyond the final word boundary */
    while (s < send) {
        if (*s != span_byte) {
            return s;
        }
        s++;
    }

    return s;
}

STATIC U8 *
S_find_next_masked(U8 * s, const U8 * send, const U8 byte, const U8 mask)
{
    /* Returns the position of the first byte in the sequence between 's'
     * and 'send-1' inclusive that when ANDed with 'mask' yields 'byte';
     * returns 'send' if none found.  It uses word-level operations instead of
     * byte to speed up the process */

    PERL_ARGS_ASSERT_FIND_NEXT_MASKED;

    assert(send >= s);
    assert((byte & mask) == byte);

#ifndef EBCDIC

    if ((STRLEN) (send - s) >= PERL_WORDSIZE
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(s)
                          - (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK))
    {
        PERL_UINTMAX_T word_complemented, mask_word;

        while (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK) {
            if (((*s) & mask) == byte) {
                return s;
            }
            s++;
        }

        word_complemented = ~ (PERL_COUNT_MULTIPLIER * byte);
        mask_word =            PERL_COUNT_MULTIPLIER * mask;

        do {
            PERL_UINTMAX_T masked = (* (PERL_UINTMAX_T *) s) & mask_word;

            /* If 'masked' contains 'byte' within it, anding with the
             * complement will leave those 8 bits 0 */
            masked &= word_complemented;

            /* This causes the most significant bit to be set to 1 for any
             * bytes in the word that aren't completely 0 */
            masked |= masked << 1;
            masked |= masked << 2;
            masked |= masked << 4;

            /* The msbits are the same as what marks a byte as variant, so we
             * can use this mask.  If all msbits are 1, the word doesn't
             * contain 'byte' */
            if ((masked & PERL_VARIANTS_WORD_MASK) == PERL_VARIANTS_WORD_MASK) {
                s += PERL_WORDSIZE;
                continue;
            }

            /* Here, the msbit of bytes in the word that aren't 'byte' are 1,
             * and any that are, are 0.  Complement and re-AND to swap that */
            masked = ~ masked;
            masked &= PERL_VARIANTS_WORD_MASK;

            /* This reduces the problem to that solved by this function */
            s += _variant_byte_number(masked);
            return s;

        } while (s + PERL_WORDSIZE <= send);
    }

#endif

    while (s < send) {
        if (((*s) & mask) == byte) {
            return s;
        }
        s++;
    }

    return s;
}

STATIC U8 *
S_find_span_end_mask(U8 * s, const U8 * send, const U8 span_byte, const U8 mask)
{
    /* Returns the position of the first byte in the sequence between 's' and
     * 'send-1' inclusive that when ANDed with 'mask' isn't 'span_byte'.
     * 'span_byte' should have been ANDed with 'mask' in the call of this
     * function.  Returns 'send' if none found.  Works like find_span_end(),
     * except for the AND */

    PERL_ARGS_ASSERT_FIND_SPAN_END_MASK;

    assert(send >= s);
    assert((span_byte & mask) == span_byte);

    if ((STRLEN) (send - s) >= PERL_WORDSIZE
                          + PERL_WORDSIZE * PERL_IS_SUBWORD_ADDR(s)
                          - (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK))
    {
        PERL_UINTMAX_T span_word, mask_word;

        while (PTR2nat(s) & PERL_WORD_BOUNDARY_MASK) {
            if (((*s) & mask) != span_byte) {
                return s;
            }
            s++;
        }

        span_word = PERL_COUNT_MULTIPLIER * span_byte;
        mask_word = PERL_COUNT_MULTIPLIER * mask;

        do {
            PERL_UINTMAX_T masked = (* (PERL_UINTMAX_T *) s) & mask_word;

            if (masked == span_word) {
                s += PERL_WORDSIZE;
                continue;
            }

#ifdef EBCDIC

            break;

#else

            masked ^= span_word;
            masked |= masked << 1;
            masked |= masked << 2;
            masked |= masked << 4;
            return s + _variant_byte_number(masked);

#endif

        } while (s + PERL_WORDSIZE <= send);
    }

    while (s < send) {
        if (((*s) & mask) != span_byte) {
            return s;
        }
        s++;
    }

    return s;
}

/*
 * pregexec and friends
 */

#ifndef PERL_IN_XSUB_RE
/*
 - pregexec - match a regexp against a string
 */
I32
Perl_pregexec(pTHX_ REGEXP * const prog, char* stringarg, char *strend,
	 char *strbeg, SSize_t minend, SV *screamer, U32 nosave)
/* stringarg: the point in the string at which to begin matching */
/* strend:    pointer to null at end of string */
/* strbeg:    real beginning of string */
/* minend:    end of match must be >= minend bytes after stringarg. */
/* screamer:  SV being matched: only used for utf8 flag, pos() etc; string
 *            itself is accessed via the pointers above */
/* nosave:    For optimizations. */
{
    PERL_ARGS_ASSERT_PREGEXEC;

    return
	regexec_flags(prog, stringarg, strend, strbeg, minend, screamer, NULL,
		      nosave ? 0 : REXEC_COPY_STR);
}
#endif



/* re_intuit_start():
 *
 * Based on some optimiser hints, try to find the earliest position in the
 * string where the regex could match.
 *
 *   rx:     the regex to match against
 *   sv:     the SV being matched: only used for utf8 flag; the string
 *           itself is accessed via the pointers below. Note that on
 *           something like an overloaded SV, SvPOK(sv) may be false
 *           and the string pointers may point to something unrelated to
 *           the SV itself.
 *   strbeg: real beginning of string
 *   strpos: the point in the string at which to begin matching
 *   strend: pointer to the byte following the last char of the string
 *   flags   currently unused; set to 0
 *   data:   currently unused; set to NULL
 *
 * The basic idea of re_intuit_start() is to use some known information
 * about the pattern, namely:
 *
 *   a) the longest known anchored substring (i.e. one that's at a
 *      constant offset from the beginning of the pattern; but not
 *      necessarily at a fixed offset from the beginning of the
 *      string);
 *   b) the longest floating substring (i.e. one that's not at a constant
 *      offset from the beginning of the pattern);
 *   c) Whether the pattern is anchored to the string; either
 *      an absolute anchor: /^../, or anchored to \n: /^.../m,
 *      or anchored to pos(): /\G/;
 *   d) A start class: a real or synthetic character class which
 *      represents which characters are legal at the start of the pattern;
 *
 * to either quickly reject the match, or to find the earliest position
 * within the string at which the pattern might match, thus avoiding
 * running the full NFA engine at those earlier locations, only to
 * eventually fail and retry further along.
 *
 * Returns NULL if the pattern can't match, or returns the address within
 * the string which is the earliest place the match could occur.
 *
 * The longest of the anchored and floating substrings is called 'check'
 * and is checked first. The other is called 'other' and is checked
 * second. The 'other' substring may not be present.  For example,
 *
 *    /(abc|xyz)ABC\d{0,3}DEFG/
 *
 * will have
 *
 *   check substr (float)    = "DEFG", offset 6..9 chars
 *   other substr (anchored) = "ABC",  offset 3..3 chars
 *   stclass = [ax]
 *
 * Be aware that during the course of this function, sometimes 'anchored'
 * refers to a substring being anchored relative to the start of the
 * pattern, and sometimes to the pattern itself being anchored relative to
 * the string. For example:
 *
 *   /\dabc/:   "abc" is anchored to the pattern;
 *   /^\dabc/:  "abc" is anchored to the pattern and the string;
 *   /\d+abc/:  "abc" is anchored to neither the pattern nor the string;
 *   /^\d+abc/: "abc" is anchored to neither the pattern nor the string,
 *                    but the pattern is anchored to the string.
 */

char *
Perl_re_intuit_start(pTHX_
                    REGEXP * const rx,
                    SV *sv,
                    const char * const strbeg,
                    char *strpos,
                    char *strend,
                    const U32 flags,
                    re_scream_pos_data *data)
{
    struct regexp *const prog = ReANY(rx);
    SSize_t start_shift = prog->check_offset_min;
    /* Should be nonnegative! */
    SSize_t end_shift   = 0;
    /* current lowest pos in string where the regex can start matching */
    char *rx_origin = strpos;
    SV *check;
    const bool utf8_target = (sv && SvUTF8(sv)) ? 1 : 0; /* if no sv we have to assume bytes */
    U8   other_ix = 1 - prog->substrs->check_ix;
    bool ml_anch = 0;
    char *other_last = strpos;/* latest pos 'other' substr already checked to */
    char *check_at = NULL;		/* check substr found at this pos */
    const I32 multiline = prog->extflags & RXf_PMf_MULTILINE;
    RXi_GET_DECL(prog,progi);
    regmatch_info reginfo_buf;  /* create some info to pass to find_byclass */
    regmatch_info *const reginfo = &reginfo_buf;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_INTUIT_START;
    PERL_UNUSED_ARG(flags);
    PERL_UNUSED_ARG(data);

    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                "Intuit: trying to determine minimum start position...\n"));

    /* for now, assume that all substr offsets are positive. If at some point
     * in the future someone wants to do clever things with lookbehind and
     * -ve offsets, they'll need to fix up any code in this function
     * which uses these offsets. See the thread beginning
     * <20140113145929.GF27210@iabyn.com>
     */
    assert(prog->substrs->data[0].min_offset >= 0);
    assert(prog->substrs->data[0].max_offset >= 0);
    assert(prog->substrs->data[1].min_offset >= 0);
    assert(prog->substrs->data[1].max_offset >= 0);
    assert(prog->substrs->data[2].min_offset >= 0);
    assert(prog->substrs->data[2].max_offset >= 0);

    /* for now, assume that if both present, that the floating substring
     * doesn't start before the anchored substring.
     * If you break this assumption (e.g. doing better optimisations
     * with lookahead/behind), then you'll need to audit the code in this
     * function carefully first
     */
    assert(
            ! (  (prog->anchored_utf8 || prog->anchored_substr)
              && (prog->float_utf8    || prog->float_substr))
           || (prog->float_min_offset >= prog->anchored_offset));

    /* byte rather than char calculation for efficiency. It fails
     * to quickly reject some cases that can't match, but will reject
     * them later after doing full char arithmetic */
    if (prog->minlen > strend - strpos) {
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
			      "  String too short...\n"));
	goto fail;
    }

    RXp_MATCH_UTF8_set(prog, utf8_target);
    reginfo->is_utf8_target = cBOOL(utf8_target);
    reginfo->info_aux = NULL;
    reginfo->strbeg = strbeg;
    reginfo->strend = strend;
    reginfo->is_utf8_pat = cBOOL(RX_UTF8(rx));
    reginfo->intuit = 1;
    /* not actually used within intuit, but zero for safety anyway */
    reginfo->poscache_maxiter = 0;

    if (utf8_target) {
        if ((!prog->anchored_utf8 && prog->anchored_substr)
                || (!prog->float_utf8 && prog->float_substr))
	    to_utf8_substr(prog);
	check = prog->check_utf8;
    } else {
	if (!prog->check_substr && prog->check_utf8) {
	    if (! to_byte_substr(prog)) {
                NON_UTF8_TARGET_BUT_UTF8_REQUIRED(fail);
            }
        }
	check = prog->check_substr;
    }

    /* dump the various substring data */
    DEBUG_OPTIMISE_MORE_r({
        int i;
        for (i=0; i<=2; i++) {
            SV *sv = (utf8_target ? prog->substrs->data[i].utf8_substr
                                  : prog->substrs->data[i].substr);
            if (!sv)
                continue;

            Perl_re_printf( aTHX_
                "  substrs[%d]: min=%" IVdf " max=%" IVdf " end shift=%" IVdf
                " useful=%" IVdf " utf8=%d [%s]\n",
                i,
                (IV)prog->substrs->data[i].min_offset,
                (IV)prog->substrs->data[i].max_offset,
                (IV)prog->substrs->data[i].end_shift,
                BmUSEFUL(sv),
                utf8_target ? 1 : 0,
                SvPEEK(sv));
        }
    });

    if (prog->intflags & PREGf_ANCH) { /* Match at \G, beg-of-str or after \n */

        /* ml_anch: check after \n?
         *
         * A note about PREGf_IMPLICIT: on an un-anchored pattern beginning
         * with /.*.../, these flags will have been added by the
         * compiler:
         *   /.*abc/, /.*abc/m:  PREGf_IMPLICIT | PREGf_ANCH_MBOL
         *   /.*abc/s:           PREGf_IMPLICIT | PREGf_ANCH_SBOL
         */
	ml_anch =      (prog->intflags & PREGf_ANCH_MBOL)
                   && !(prog->intflags & PREGf_IMPLICIT);

	if (!ml_anch && !(prog->intflags & PREGf_IMPLICIT)) {
            /* we are only allowed to match at BOS or \G */

            /* trivially reject if there's a BOS anchor and we're not at BOS.
             *
             * Note that we don't try to do a similar quick reject for
             * \G, since generally the caller will have calculated strpos
             * based on pos() and gofs, so the string is already correctly
             * anchored by definition; and handling the exceptions would
             * be too fiddly (e.g. REXEC_IGNOREPOS).
             */
            if (   strpos != strbeg
                && (prog->intflags & PREGf_ANCH_SBOL))
            {
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                                "  Not at start...\n"));
	        goto fail;
	    }

            /* in the presence of an anchor, the anchored (relative to the
             * start of the regex) substr must also be anchored relative
             * to strpos. So quickly reject if substr isn't found there.
             * This works for \G too, because the caller will already have
             * subtracted gofs from pos, and gofs is the offset from the
             * \G to the start of the regex. For example, in /.abc\Gdef/,
             * where substr="abcdef", pos()=3, gofs=4, offset_min=1:
             * caller will have set strpos=pos()-4; we look for the substr
             * at position pos()-4+1, which lines up with the "a" */

	    if (prog->check_offset_min == prog->check_offset_max) {
	        /* Substring at constant offset from beg-of-str... */
	        SSize_t slen = SvCUR(check);
                char *s = HOP3c(strpos, prog->check_offset_min, strend);
	    
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                    "  Looking for check substr at fixed offset %" IVdf "...\n",
                    (IV)prog->check_offset_min));

	        if (SvTAIL(check)) {
                    /* In this case, the regex is anchored at the end too.
                     * Unless it's a multiline match, the lengths must match
                     * exactly, give or take a \n.  NB: slen >= 1 since
                     * the last char of check is \n */
		    if (!multiline
                        && (   strend - s > slen
                            || strend - s < slen - 1
                            || (strend - s == slen && strend[-1] != '\n')))
                    {
                        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                                            "  String too long...\n"));
                        goto fail_finish;
                    }
                    /* Now should match s[0..slen-2] */
                    slen--;
                }
                if (slen && (strend - s < slen
                    || *SvPVX_const(check) != *s
                    || (slen > 1 && (memNE(SvPVX_const(check), s, slen)))))
                {
                    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                                    "  String not equal...\n"));
                    goto fail_finish;
                }

                check_at = s;
                goto success_at_start;
	    }
	}
    }

    end_shift = prog->check_end_shift;

#ifdef DEBUGGING	/* 7/99: reports of failure (with the older version) */
    if (end_shift < 0)
	Perl_croak(aTHX_ "panic: end_shift: %" IVdf " pattern:\n%s\n ",
		   (IV)end_shift, RX_PRECOMP(rx));
#endif

  restart:
    
    /* This is the (re)entry point of the main loop in this function.
     * The goal of this loop is to:
     * 1) find the "check" substring in the region rx_origin..strend
     *    (adjusted by start_shift / end_shift). If not found, reject
     *    immediately.
     * 2) If it exists, look for the "other" substr too if defined; for
     *    example, if the check substr maps to the anchored substr, then
     *    check the floating substr, and vice-versa. If not found, go
     *    back to (1) with rx_origin suitably incremented.
     * 3) If we find an rx_origin position that doesn't contradict
     *    either of the substrings, then check the possible additional
     *    constraints on rx_origin of /^.../m or a known start class.
     *    If these fail, then depending on which constraints fail, jump
     *    back to here, or to various other re-entry points further along
     *    that skip some of the first steps.
     * 4) If we pass all those tests, update the BmUSEFUL() count on the
     *    substring. If the start position was determined to be at the
     *    beginning of the string  - so, not rejected, but not optimised,
     *    since we have to run regmatch from position 0 - decrement the
     *    BmUSEFUL() count. Otherwise increment it.
     */


    /* first, look for the 'check' substring */

    {
        U8* start_point;
        U8* end_point;

        DEBUG_OPTIMISE_MORE_r({
            Perl_re_printf( aTHX_
                "  At restart: rx_origin=%" IVdf " Check offset min: %" IVdf
                " Start shift: %" IVdf " End shift %" IVdf
                " Real end Shift: %" IVdf "\n",
                (IV)(rx_origin - strbeg),
                (IV)prog->check_offset_min,
                (IV)start_shift,
                (IV)end_shift,
                (IV)prog->check_end_shift);
        });
        
        end_point = HOPBACK3(strend, end_shift, rx_origin);
        if (!end_point)
            goto fail_finish;
        start_point = HOPMAYBE3(rx_origin, start_shift, end_point);
        if (!start_point)
            goto fail_finish;


        /* If the regex is absolutely anchored to either the start of the
         * string (SBOL) or to pos() (ANCH_GPOS), then
         * check_offset_max represents an upper bound on the string where
         * the substr could start. For the ANCH_GPOS case, we assume that
         * the caller of intuit will have already set strpos to
         * pos()-gofs, so in this case strpos + offset_max will still be
         * an upper bound on the substr.
         */
        if (!ml_anch
            && prog->intflags & PREGf_ANCH
            && prog->check_offset_max != SSize_t_MAX)
        {
            SSize_t check_len = SvCUR(check) - !!SvTAIL(check);
            const char * const anchor =
                        (prog->intflags & PREGf_ANCH_GPOS ? strpos : strbeg);
            SSize_t targ_len = (char*)end_point - anchor;

            if (check_len > targ_len) {
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
			      "Target string too short to match required substring...\n"));
                goto fail_finish;
            }

            /* do a bytes rather than chars comparison. It's conservative;
             * so it skips doing the HOP if the result can't possibly end
             * up earlier than the old value of end_point.
             */
            assert(anchor + check_len <= (char *)end_point);
            if (prog->check_offset_max + check_len < targ_len) {
                end_point = HOP3lim((U8*)anchor,
                                prog->check_offset_max,
                                end_point - check_len
                            )
                            + check_len;
                if (end_point < start_point)
                    goto fail_finish;
            }
        }

	check_at = fbm_instr( start_point, end_point,
		      check, multiline ? FBMrf_MULTILINE : 0);

        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "  doing 'check' fbm scan, [%" IVdf "..%" IVdf "] gave %" IVdf "\n",
            (IV)((char*)start_point - strbeg),
            (IV)((char*)end_point   - strbeg),
            (IV)(check_at ? check_at - strbeg : -1)
        ));

        /* Update the count-of-usability, remove useless subpatterns,
            unshift s.  */

        DEBUG_EXECUTE_r({
            RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
                SvPVX_const(check), RE_SV_DUMPLEN(check), 30);
            Perl_re_printf( aTHX_  "  %s %s substr %s%s%s",
                              (check_at ? "Found" : "Did not find"),
                (check == (utf8_target ? prog->anchored_utf8 : prog->anchored_substr)
                    ? "anchored" : "floating"),
                quoted,
                RE_SV_TAIL(check),
                (check_at ? " at offset " : "...\n") );
        });

        if (!check_at)
            goto fail_finish;
        /* set rx_origin to the minimum position where the regex could start
         * matching, given the constraint of the just-matched check substring.
         * But don't set it lower than previously.
         */

        if (check_at - rx_origin > prog->check_offset_max)
            rx_origin = HOP3c(check_at, -prog->check_offset_max, rx_origin);
        /* Finish the diagnostic message */
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "%ld (rx_origin now %" IVdf ")...\n",
            (long)(check_at - strbeg),
            (IV)(rx_origin - strbeg)
        ));
    }


    /* now look for the 'other' substring if defined */

    if (utf8_target ? prog->substrs->data[other_ix].utf8_substr
                    : prog->substrs->data[other_ix].substr)
    {
	/* Take into account the "other" substring. */
        char *last, *last1;
        char *s;
        SV* must;
        struct reg_substr_datum *other;

      do_other_substr:
        other = &prog->substrs->data[other_ix];

        /* if "other" is anchored:
         * we've previously found a floating substr starting at check_at.
         * This means that the regex origin must lie somewhere
         * between min (rx_origin): HOP3(check_at, -check_offset_max)
         * and max:                 HOP3(check_at, -check_offset_min)
         * (except that min will be >= strpos)
         * So the fixed  substr must lie somewhere between
         *  HOP3(min, anchored_offset)
         *  HOP3(max, anchored_offset) + SvCUR(substr)
         */

        /* if "other" is floating
         * Calculate last1, the absolute latest point where the
         * floating substr could start in the string, ignoring any
         * constraints from the earlier fixed match. It is calculated
         * as follows:
         *
         * strend - prog->minlen (in chars) is the absolute latest
         * position within the string where the origin of the regex
         * could appear. The latest start point for the floating
         * substr is float_min_offset(*) on from the start of the
         * regex.  last1 simply combines thee two offsets.
         *
         * (*) You might think the latest start point should be
         * float_max_offset from the regex origin, and technically
         * you'd be correct. However, consider
         *    /a\d{2,4}bcd\w/
         * Here, float min, max are 3,5 and minlen is 7.
         * This can match either
         *    /a\d\dbcd\w/
         *    /a\d\d\dbcd\w/
         *    /a\d\d\d\dbcd\w/
         * In the first case, the regex matches minlen chars; in the
         * second, minlen+1, in the third, minlen+2.
         * In the first case, the floating offset is 3 (which equals
         * float_min), in the second, 4, and in the third, 5 (which
         * equals float_max). In all cases, the floating string bcd
         * can never start more than 4 chars from the end of the
         * string, which equals minlen - float_min. As the substring
         * starts to match more than float_min from the start of the
         * regex, it makes the regex match more than minlen chars,
         * and the two cancel each other out. So we can always use
         * float_min - minlen, rather than float_max - minlen for the
         * latest position in the string.
         *
         * Note that -minlen + float_min_offset is equivalent (AFAIKT)
         * to CHR_SVLEN(must) - !!SvTAIL(must) + prog->float_end_shift
         */

        assert(prog->minlen >= other->min_offset);
        last1 = HOP3c(strend,
                        other->min_offset - prog->minlen, strbeg);

        if (other_ix) {/* i.e. if (other-is-float) */
            /* last is the latest point where the floating substr could
             * start, *given* any constraints from the earlier fixed
             * match. This constraint is that the floating string starts
             * <= float_max_offset chars from the regex origin (rx_origin).
             * If this value is less than last1, use it instead.
             */
            assert(rx_origin <= last1);
            last =
                /* this condition handles the offset==infinity case, and
                 * is a short-cut otherwise. Although it's comparing a
                 * byte offset to a char length, it does so in a safe way,
                 * since 1 char always occupies 1 or more bytes,
                 * so if a string range is  (last1 - rx_origin) bytes,
                 * it will be less than or equal to  (last1 - rx_origin)
                 * chars; meaning it errs towards doing the accurate HOP3
                 * rather than just using last1 as a short-cut */
                (last1 - rx_origin) < other->max_offset
                    ? last1
                    : (char*)HOP3lim(rx_origin, other->max_offset, last1);
        }
        else {
            assert(strpos + start_shift <= check_at);
            last = HOP4c(check_at, other->min_offset - start_shift,
                        strbeg, strend);
        }

        s = HOP3c(rx_origin, other->min_offset, strend);
        if (s < other_last)	/* These positions already checked */
            s = other_last;

        must = utf8_target ? other->utf8_substr : other->substr;
        assert(SvPOK(must));
        {
            char *from = s;
            char *to   = last + SvCUR(must) - (SvTAIL(must)!=0);

            if (to > strend)
                to = strend;
            if (from > to) {
                s = NULL;
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                    "  skipping 'other' fbm scan: %" IVdf " > %" IVdf "\n",
                    (IV)(from - strbeg),
                    (IV)(to   - strbeg)
                ));
            }
            else {
                s = fbm_instr(
                    (unsigned char*)from,
                    (unsigned char*)to,
                    must,
                    multiline ? FBMrf_MULTILINE : 0
                );
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                    "  doing 'other' fbm scan, [%" IVdf "..%" IVdf "] gave %" IVdf "\n",
                    (IV)(from - strbeg),
                    (IV)(to   - strbeg),
                    (IV)(s ? s - strbeg : -1)
                ));
            }
        }

        DEBUG_EXECUTE_r({
            RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
                SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
            Perl_re_printf( aTHX_  "  %s %s substr %s%s",
                s ? "Found" : "Contradicts",
                other_ix ? "floating" : "anchored",
                quoted, RE_SV_TAIL(must));
        });


        if (!s) {
            /* last1 is latest possible substr location. If we didn't
             * find it before there, we never will */
            if (last >= last1) {
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                                        "; giving up...\n"));
                goto fail_finish;
            }

            /* try to find the check substr again at a later
             * position. Maybe next time we'll find the "other" substr
             * in range too */
            other_last = HOP3c(last, 1, strend) /* highest failure */;
            rx_origin =
                other_ix /* i.e. if other-is-float */
                    ? HOP3c(rx_origin, 1, strend)
                    : HOP4c(last, 1 - other->min_offset, strbeg, strend);
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                "; about to retry %s at offset %ld (rx_origin now %" IVdf ")...\n",
                (other_ix ? "floating" : "anchored"),
                (long)(HOP3c(check_at, 1, strend) - strbeg),
                (IV)(rx_origin - strbeg)
            ));
            goto restart;
        }
        else {
            if (other_ix) { /* if (other-is-float) */
                /* other_last is set to s, not s+1, since its possible for
                 * a floating substr to fail first time, then succeed
                 * second time at the same floating position; e.g.:
                 *     "-AB--AABZ" =~ /\wAB\d*Z/
                 * The first time round, anchored and float match at
                 * "-(AB)--AAB(Z)" then fail on the initial \w character
                 * class. Second time round, they match at "-AB--A(AB)(Z)".
                 */
                other_last = s;
            }
            else {
                rx_origin = HOP3c(s, -other->min_offset, strbeg);
                other_last = HOP3c(s, 1, strend);
            }
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                " at offset %ld (rx_origin now %" IVdf ")...\n",
                  (long)(s - strbeg),
                (IV)(rx_origin - strbeg)
              ));

        }
    }
    else {
        DEBUG_OPTIMISE_MORE_r(
            Perl_re_printf( aTHX_
                "  Check-only match: offset min:%" IVdf " max:%" IVdf
                " check_at:%" IVdf " rx_origin:%" IVdf " rx_origin-check_at:%" IVdf
                " strend:%" IVdf "\n",
                (IV)prog->check_offset_min,
                (IV)prog->check_offset_max,
                (IV)(check_at-strbeg),
                (IV)(rx_origin-strbeg),
                (IV)(rx_origin-check_at),
                (IV)(strend-strbeg)
            )
        );
    }

  postprocess_substr_matches:

    /* handle the extra constraint of /^.../m if present */

    if (ml_anch && rx_origin != strbeg && rx_origin[-1] != '\n') {
        char *s;

        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                        "  looking for /^/m anchor"));

        /* we have failed the constraint of a \n before rx_origin.
         * Find the next \n, if any, even if it's beyond the current
         * anchored and/or floating substrings. Whether we should be
         * scanning ahead for the next \n or the next substr is debatable.
         * On the one hand you'd expect rare substrings to appear less
         * often than \n's. On the other hand, searching for \n means
         * we're effectively flipping between check_substr and "\n" on each
         * iteration as the current "rarest" string candidate, which
         * means for example that we'll quickly reject the whole string if
         * hasn't got a \n, rather than trying every substr position
         * first
         */

        s = HOP3c(strend, - prog->minlen, strpos);
        if (s <= rx_origin ||
            ! ( rx_origin = (char *)memchr(rx_origin, '\n', s - rx_origin)))
        {
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                            "  Did not find /%s^%s/m...\n",
                            PL_colors[0], PL_colors[1]));
            goto fail_finish;
        }

        /* earliest possible origin is 1 char after the \n.
         * (since *rx_origin == '\n', it's safe to ++ here rather than
         * HOP(rx_origin, 1)) */
        rx_origin++;

        if (prog->substrs->check_ix == 0  /* check is anchored */
            || rx_origin >= HOP3c(check_at,  - prog->check_offset_min, strpos))
        {
            /* Position contradicts check-string; either because
             * check was anchored (and thus has no wiggle room),
             * or check was float and rx_origin is above the float range */
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                "  Found /%s^%s/m, about to restart lookup for check-string with rx_origin %ld...\n",
                PL_colors[0], PL_colors[1], (long)(rx_origin - strbeg)));
            goto restart;
        }

        /* if we get here, the check substr must have been float,
         * is in range, and we may or may not have had an anchored
         * "other" substr which still contradicts */
        assert(prog->substrs->check_ix); /* check is float */

        if (utf8_target ? prog->anchored_utf8 : prog->anchored_substr) {
            /* whoops, the anchored "other" substr exists, so we still
             * contradict. On the other hand, the float "check" substr
             * didn't contradict, so just retry the anchored "other"
             * substr */
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                "  Found /%s^%s/m, rescanning for anchored from offset %" IVdf " (rx_origin now %" IVdf ")...\n",
                PL_colors[0], PL_colors[1],
                (IV)(rx_origin - strbeg + prog->anchored_offset),
                (IV)(rx_origin - strbeg)
            ));
            goto do_other_substr;
        }

        /* success: we don't contradict the found floating substring
         * (and there's no anchored substr). */
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "  Found /%s^%s/m with rx_origin %ld...\n",
            PL_colors[0], PL_colors[1], (long)(rx_origin - strbeg)));
    }
    else {
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "  (multiline anchor test skipped)\n"));
    }

  success_at_start:


    /* if we have a starting character class, then test that extra constraint.
     * (trie stclasses are too expensive to use here, we are better off to
     * leave it to regmatch itself) */

    if (progi->regstclass && PL_regkind[OP(progi->regstclass)]!=TRIE) {
        const U8* const str = (U8*)STRING(progi->regstclass);

        /* XXX this value could be pre-computed */
        const int cl_l = (PL_regkind[OP(progi->regstclass)] == EXACT
		    ?  (reginfo->is_utf8_pat
                        ? utf8_distance(str + STR_LEN(progi->regstclass), str)
                        : STR_LEN(progi->regstclass))
		    : 1);
	char * endpos;
        char *s;
        /* latest pos that a matching float substr constrains rx start to */
        char *rx_max_float = NULL;

        /* if the current rx_origin is anchored, either by satisfying an
         * anchored substring constraint, or a /^.../m constraint, then we
         * can reject the current origin if the start class isn't found
         * at the current position. If we have a float-only match, then
         * rx_origin is constrained to a range; so look for the start class
         * in that range. if neither, then look for the start class in the
         * whole rest of the string */

        /* XXX DAPM it's not clear what the minlen test is for, and why
         * it's not used in the floating case. Nothing in the test suite
         * causes minlen == 0 here. See <20140313134639.GS12844@iabyn.com>.
         * Here are some old comments, which may or may not be correct:
         *
	 *   minlen == 0 is possible if regstclass is \b or \B,
	 *   and the fixed substr is ''$.
         *   Since minlen is already taken into account, rx_origin+1 is
         *   before strend; accidentally, minlen >= 1 guaranties no false
         *   positives at rx_origin + 1 even for \b or \B.  But (minlen? 1 :
         *   0) below assumes that regstclass does not come from lookahead...
	 *   If regstclass takes bytelength more than 1: If charlength==1, OK.
         *   This leaves EXACTF-ish only, which are dealt with in
         *   find_byclass().
         */

	if (prog->anchored_substr || prog->anchored_utf8 || ml_anch)
            endpos = HOP3clim(rx_origin, (prog->minlen ? cl_l : 0), strend);
        else if (prog->float_substr || prog->float_utf8) {
	    rx_max_float = HOP3c(check_at, -start_shift, strbeg);
	    endpos = HOP3clim(rx_max_float, cl_l, strend);
        }
        else 
            endpos= strend;
		    
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "  looking for class: start_shift: %" IVdf " check_at: %" IVdf
            " rx_origin: %" IVdf " endpos: %" IVdf "\n",
              (IV)start_shift, (IV)(check_at - strbeg),
              (IV)(rx_origin - strbeg), (IV)(endpos - strbeg)));

        s = find_byclass(prog, progi->regstclass, rx_origin, endpos,
                            reginfo);
	if (!s) {
	    if (endpos == strend) {
                DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
				"  Could not match STCLASS...\n") );
		goto fail;
	    }
            DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
                               "  This position contradicts STCLASS...\n") );
            if ((prog->intflags & PREGf_ANCH) && !ml_anch
                        && !(prog->intflags & PREGf_IMPLICIT))
		goto fail;

	    /* Contradict one of substrings */
	    if (prog->anchored_substr || prog->anchored_utf8) {
                if (prog->substrs->check_ix == 1) { /* check is float */
                    /* Have both, check_string is floating */
                    assert(rx_origin + start_shift <= check_at);
                    if (rx_origin + start_shift != check_at) {
                        /* not at latest position float substr could match:
                         * Recheck anchored substring, but not floating.
                         * The condition above is in bytes rather than
                         * chars for efficiency. It's conservative, in
                         * that it errs on the side of doing 'goto
                         * do_other_substr'. In this case, at worst,
                         * an extra anchored search may get done, but in
                         * practice the extra fbm_instr() is likely to
                         * get skipped anyway. */
                        DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
                            "  about to retry anchored at offset %ld (rx_origin now %" IVdf ")...\n",
                            (long)(other_last - strbeg),
                            (IV)(rx_origin - strbeg)
                        ));
                        goto do_other_substr;
                    }
                }
            }
	    else {
                /* float-only */

                if (ml_anch) {
                    /* In the presence of ml_anch, we might be able to
                     * find another \n without breaking the current float
                     * constraint. */

                    /* strictly speaking this should be HOP3c(..., 1, ...),
                     * but since we goto a block of code that's going to
                     * search for the next \n if any, its safe here */
                    rx_origin++;
                    DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
                              "  about to look for /%s^%s/m starting at rx_origin %ld...\n",
                              PL_colors[0], PL_colors[1],
                              (long)(rx_origin - strbeg)) );
                    goto postprocess_substr_matches;
                }

                /* strictly speaking this can never be true; but might
                 * be if we ever allow intuit without substrings */
                if (!(utf8_target ? prog->float_utf8 : prog->float_substr))
                    goto fail;

                rx_origin = rx_max_float;
            }

            /* at this point, any matching substrings have been
             * contradicted. Start again... */

            rx_origin = HOP3c(rx_origin, 1, strend);

            /* uses bytes rather than char calculations for efficiency.
             * It's conservative: it errs on the side of doing 'goto restart',
             * where there is code that does a proper char-based test */
            if (rx_origin + start_shift + end_shift > strend) {
                DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
                                       "  Could not match STCLASS...\n") );
                goto fail;
            }
            DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
                "  about to look for %s substr starting at offset %ld (rx_origin now %" IVdf ")...\n",
                (prog->substrs->check_ix ? "floating" : "anchored"),
                (long)(rx_origin + start_shift - strbeg),
                (IV)(rx_origin - strbeg)
            ));
            goto restart;
	}

        /* Success !!! */

	if (rx_origin != s) {
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
			"  By STCLASS: moving %ld --> %ld\n",
                                  (long)(rx_origin - strbeg), (long)(s - strbeg))
                   );
        }
        else {
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                                  "  Does not contradict STCLASS...\n");
                   );
        }
    }

    /* Decide whether using the substrings helped */

    if (rx_origin != strpos) {
	/* Fixed substring is found far enough so that the match
	   cannot start at strpos. */

        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "  try at offset...\n"));
	++BmUSEFUL(utf8_target ? prog->check_utf8 : prog->check_substr);	/* hooray/5 */
    }
    else {
        /* The found rx_origin position does not prohibit matching at
         * strpos, so calling intuit didn't gain us anything. Decrement
         * the BmUSEFUL() count on the check substring, and if we reach
         * zero, free it.  */
	if (!(prog->intflags & PREGf_NAUGHTY)
	    && (utf8_target ? (
		prog->check_utf8		/* Could be deleted already */
		&& --BmUSEFUL(prog->check_utf8) < 0
		&& (prog->check_utf8 == prog->float_utf8)
	    ) : (
		prog->check_substr		/* Could be deleted already */
		&& --BmUSEFUL(prog->check_substr) < 0
		&& (prog->check_substr == prog->float_substr)
	    )))
	{
	    /* If flags & SOMETHING - do not do it many times on the same match */
            DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "  ... Disabling check substring...\n"));
	    /* XXX Does the destruction order has to change with utf8_target? */
	    SvREFCNT_dec(utf8_target ? prog->check_utf8 : prog->check_substr);
	    SvREFCNT_dec(utf8_target ? prog->check_substr : prog->check_utf8);
	    prog->check_substr = prog->check_utf8 = NULL;	/* disable */
	    prog->float_substr = prog->float_utf8 = NULL;	/* clear */
	    check = NULL;			/* abort */
	    /* XXXX This is a remnant of the old implementation.  It
	            looks wasteful, since now INTUIT can use many
	            other heuristics. */
	    prog->extflags &= ~RXf_USE_INTUIT;
	}
    }

    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "Intuit: %sSuccessfully guessed:%s match at offset %ld\n",
             PL_colors[4], PL_colors[5], (long)(rx_origin - strbeg)) );

    return rx_origin;

  fail_finish:				/* Substring not found */
    if (prog->check_substr || prog->check_utf8)		/* could be removed already */
	BmUSEFUL(utf8_target ? prog->check_utf8 : prog->check_substr) += 5; /* hooray */
  fail:
    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "%sMatch rejected by optimizer%s\n",
			  PL_colors[4], PL_colors[5]));
    return NULL;
}


#define DECL_TRIE_TYPE(scan) \
    const enum { trie_plain, trie_utf8, trie_utf8_fold, trie_latin_utf8_fold,       \
                 trie_utf8_exactfa_fold, trie_latin_utf8_exactfa_fold,              \
                 trie_utf8l, trie_flu8, trie_flu8_latin }                           \
                    trie_type = ((scan->flags == EXACT)                             \
                                 ? (utf8_target ? trie_utf8 : trie_plain)           \
                                 : (scan->flags == EXACTL)                          \
                                    ? (utf8_target ? trie_utf8l : trie_plain)       \
                                    : (scan->flags == EXACTFAA)                     \
                                      ? (utf8_target                                \
                                         ? trie_utf8_exactfa_fold                   \
                                         : trie_latin_utf8_exactfa_fold)            \
                                      : (scan->flags == EXACTFLU8                   \
                                         ? (utf8_target                             \
                                           ? trie_flu8                              \
                                           : trie_flu8_latin)                       \
                                         : (utf8_target                             \
                                           ? trie_utf8_fold                         \
                                           : trie_latin_utf8_fold)))

/* 'uscan' is set to foldbuf, and incremented, so below the end of uscan is
 * 'foldbuf+sizeof(foldbuf)' */
#define REXEC_TRIE_READ_CHAR(trie_type, trie, widecharmap, uc, uc_end, uscan, len, uvc, charid, foldlen, foldbuf, uniflags) \
STMT_START {                                                                        \
    STRLEN skiplen;                                                                 \
    U8 flags = FOLD_FLAGS_FULL;                                                     \
    switch (trie_type) {                                                            \
    case trie_flu8:                                                                 \
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;                                         \
        if (UTF8_IS_ABOVE_LATIN1(*uc)) {                                            \
            _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(uc, uc_end - uc);                \
        }                                                                           \
        goto do_trie_utf8_fold;                                                     \
    case trie_utf8_exactfa_fold:                                                    \
        flags |= FOLD_FLAGS_NOMIX_ASCII;                                            \
        /* FALLTHROUGH */                                                           \
    case trie_utf8_fold:                                                            \
      do_trie_utf8_fold:                                                            \
        if ( foldlen>0 ) {                                                          \
            uvc = utf8n_to_uvchr( (const U8*) uscan, foldlen, &len, uniflags );     \
            foldlen -= len;                                                         \
            uscan += len;                                                           \
            len=0;                                                                  \
        } else {                                                                    \
            uvc = _toFOLD_utf8_flags( (const U8*) uc, uc_end, foldbuf, &foldlen,    \
                                                                            flags); \
            len = UTF8SKIP(uc);                                                     \
            skiplen = UVCHR_SKIP( uvc );                                            \
            foldlen -= skiplen;                                                     \
            uscan = foldbuf + skiplen;                                              \
        }                                                                           \
        break;                                                                      \
    case trie_flu8_latin:                                                           \
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;                                         \
        goto do_trie_latin_utf8_fold;                                               \
    case trie_latin_utf8_exactfa_fold:                                              \
        flags |= FOLD_FLAGS_NOMIX_ASCII;                                            \
        /* FALLTHROUGH */                                                           \
    case trie_latin_utf8_fold:                                                      \
      do_trie_latin_utf8_fold:                                                      \
        if ( foldlen>0 ) {                                                          \
            uvc = utf8n_to_uvchr( (const U8*) uscan, foldlen, &len, uniflags );     \
            foldlen -= len;                                                         \
            uscan += len;                                                           \
            len=0;                                                                  \
        } else {                                                                    \
            len = 1;                                                                \
            uvc = _to_fold_latin1( (U8) *uc, foldbuf, &foldlen, flags);             \
            skiplen = UVCHR_SKIP( uvc );                                            \
            foldlen -= skiplen;                                                     \
            uscan = foldbuf + skiplen;                                              \
        }                                                                           \
        break;                                                                      \
    case trie_utf8l:                                                                \
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;                                         \
        if (utf8_target && UTF8_IS_ABOVE_LATIN1(*uc)) {                             \
            _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(uc, uc + UTF8SKIP(uc));          \
        }                                                                           \
        /* FALLTHROUGH */                                                           \
    case trie_utf8:                                                                 \
        uvc = utf8n_to_uvchr( (const U8*) uc, uc_end - uc, &len, uniflags );        \
        break;                                                                      \
    case trie_plain:                                                                \
        uvc = (UV)*uc;                                                              \
        len = 1;                                                                    \
    }                                                                               \
    if (uvc < 256) {                                                                \
        charid = trie->charmap[ uvc ];                                              \
    }                                                                               \
    else {                                                                          \
        charid = 0;                                                                 \
        if (widecharmap) {                                                          \
            SV** const svpp = hv_fetch(widecharmap,                                 \
                        (char*)&uvc, sizeof(UV), 0);                                \
            if (svpp)                                                               \
                charid = (U16)SvIV(*svpp);                                          \
        }                                                                           \
    }                                                                               \
} STMT_END

#define DUMP_EXEC_POS(li,s,doutf8,depth)                    \
    dump_exec_pos(li,s,(reginfo->strend),(reginfo->strbeg), \
                startpos, doutf8, depth)

#define REXEC_FBC_SCAN(UTF8, CODE)                          \
    STMT_START {                                            \
        while (s < strend) {                                \
            CODE                                            \
            s += ((UTF8) ? UTF8SKIP(s) : 1);                \
        }                                                   \
    } STMT_END

#define REXEC_FBC_CLASS_SCAN(UTF8, COND)                    \
    STMT_START {                                            \
        while (s < strend) {                                \
            REXEC_FBC_CLASS_SCAN_GUTS(UTF8, COND)           \
        }                                                   \
    } STMT_END

#define REXEC_FBC_CLASS_SCAN_GUTS(UTF8, COND)                  \
    if (COND) {                                                \
        FBC_CHECK_AND_TRY                                      \
        s += ((UTF8) ? UTF8SKIP(s) : 1);                       \
        previous_occurrence_end = s;                           \
    }                                                          \
    else {                                                     \
        s += ((UTF8) ? UTF8SKIP(s) : 1);                       \
    }

#define REXEC_FBC_CSCAN(CONDUTF8,COND)                         \
    if (utf8_target) {                                         \
	REXEC_FBC_CLASS_SCAN(1, CONDUTF8);                     \
    }                                                          \
    else {                                                     \
	REXEC_FBC_CLASS_SCAN(0, COND);                         \
    }

/* We keep track of where the next character should start after an occurrence
 * of the one we're looking for.  Knowing that, we can see right away if the
 * next occurrence is adjacent to the previous.  When 'doevery' is FALSE, we
 * don't accept the 2nd and succeeding adjacent occurrences */
#define FBC_CHECK_AND_TRY                                      \
        if (   (   doevery                                     \
                || s != previous_occurrence_end)               \
            && (reginfo->intuit || regtry(reginfo, &s)))       \
        {                                                      \
            goto got_it;                                       \
        }


/* This differs from the above macros in that it calls a function which returns
 * the next occurrence of the thing being looked for in 's'; and 'strend' if
 * there is no such occurrence. */
#define REXEC_FBC_FIND_NEXT_SCAN(UTF8, f)                   \
    while (s < strend) {                                    \
        s = (f);                                            \
        if (s >= strend) {                                  \
            break;                                          \
        }                                                   \
                                                            \
        FBC_CHECK_AND_TRY                                   \
        s += (UTF8) ? UTF8SKIP(s) : 1;                      \
        previous_occurrence_end = s;                        \
    }

/* The three macros below are slightly different versions of the same logic.
 *
 * The first is for /a and /aa when the target string is UTF-8.  This can only
 * match ascii, but it must advance based on UTF-8.   The other two handle the
 * non-UTF-8 and the more generic UTF-8 cases.   In all three, we are looking
 * for the boundary (or non-boundary) between a word and non-word character.
 * The utf8 and non-utf8 cases have the same logic, but the details must be
 * different.  Find the "wordness" of the character just prior to this one, and
 * compare it with the wordness of this one.  If they differ, we have a
 * boundary.  At the beginning of the string, pretend that the previous
 * character was a new-line.
 *
 * All these macros uncleanly have side-effects with each other and outside
 * variables.  So far it's been too much trouble to clean-up
 *
 * TEST_NON_UTF8 is the macro or function to call to test if its byte input is
 *               a word character or not.
 * IF_SUCCESS    is code to do if it finds that we are at a boundary between
 *               word/non-word
 * IF_FAIL       is code to do if we aren't at a boundary between word/non-word
 *
 * Exactly one of the two IF_FOO parameters is a no-op, depending on whether we
 * are looking for a boundary or for a non-boundary.  If we are looking for a
 * boundary, we want IF_FAIL to be the no-op, and for IF_SUCCESS to go out and
 * see if this tentative match actually works, and if so, to quit the loop
 * here.  And vice-versa if we are looking for a non-boundary.
 *
 * 'tmp' below in the next three macros in the REXEC_FBC_SCAN and
 * REXEC_FBC_SCAN loops is a loop invariant, a bool giving the return of
 * TEST_NON_UTF8(s-1).  To see this, note that that's what it is defined to be
 * at entry to the loop, and to get to the IF_FAIL branch, tmp must equal
 * TEST_NON_UTF8(s), and in the opposite branch, IF_SUCCESS, tmp is that
 * complement.  But in that branch we complement tmp, meaning that at the
 * bottom of the loop tmp is always going to be equal to TEST_NON_UTF8(s),
 * which means at the top of the loop in the next iteration, it is
 * TEST_NON_UTF8(s-1) */
#define FBC_UTF8_A(TEST_NON_UTF8, IF_SUCCESS, IF_FAIL)                         \
    tmp = (s != reginfo->strbeg) ? UCHARAT(s - 1) : '\n';                      \
    tmp = TEST_NON_UTF8(tmp);                                                  \
    REXEC_FBC_SCAN(1,  /* 1=>is-utf8; advances s while s < strend */           \
        if (tmp == ! TEST_NON_UTF8((U8) *s)) {                                 \
            tmp = !tmp;                                                        \
            IF_SUCCESS; /* Is a boundary if values for s-1 and s differ */     \
        }                                                                      \
        else {                                                                 \
            IF_FAIL;                                                           \
        }                                                                      \
    );                                                                         \

/* Like FBC_UTF8_A, but TEST_UV is a macro which takes a UV as its input, and
 * TEST_UTF8 is a macro that for the same input code points returns identically
 * to TEST_UV, but takes a pointer to a UTF-8 encoded string instead */
#define FBC_UTF8(TEST_UV, TEST_UTF8, IF_SUCCESS, IF_FAIL)                      \
    if (s == reginfo->strbeg) {                                                \
        tmp = '\n';                                                            \
    }                                                                          \
    else { /* Back-up to the start of the previous character */                \
        U8 * const r = reghop3((U8*)s, -1, (U8*)reginfo->strbeg);              \
        tmp = utf8n_to_uvchr(r, (U8*) reginfo->strend - r,                     \
                                                       0, UTF8_ALLOW_DEFAULT); \
    }                                                                          \
    tmp = TEST_UV(tmp);                                                        \
    REXEC_FBC_SCAN(1,  /* 1=>is-utf8; advances s while s < strend */           \
        if (tmp == ! (TEST_UTF8((U8 *) s, (U8 *) reginfo->strend))) {          \
            tmp = !tmp;                                                        \
            IF_SUCCESS;                                                        \
        }                                                                      \
        else {                                                                 \
            IF_FAIL;                                                           \
        }                                                                      \
    );

/* Like the above two macros.  UTF8_CODE is the complete code for handling
 * UTF-8.  Common to the BOUND and NBOUND cases, set-up by the FBC_BOUND, etc
 * macros below */
#define FBC_BOUND_COMMON(UTF8_CODE, TEST_NON_UTF8, IF_SUCCESS, IF_FAIL)        \
    if (utf8_target) {                                                         \
        UTF8_CODE                                                              \
    }                                                                          \
    else {  /* Not utf8 */                                                     \
	tmp = (s != reginfo->strbeg) ? UCHARAT(s - 1) : '\n';                  \
	tmp = TEST_NON_UTF8(tmp);                                              \
	REXEC_FBC_SCAN(0, /* 0=>not-utf8; advances s while s < strend */       \
	    if (tmp == ! TEST_NON_UTF8((U8) *s)) {                             \
		IF_SUCCESS;                                                    \
		tmp = !tmp;                                                    \
	    }                                                                  \
	    else {                                                             \
		IF_FAIL;                                                       \
	    }                                                                  \
	);                                                                     \
    }                                                                          \
    /* Here, things have been set up by the previous code so that tmp is the   \
     * return of TEST_NON_UTF(s-1) or TEST_UTF8(s-1) (depending on the         \
     * utf8ness of the target).  We also have to check if this matches against \
     * the EOS, which we treat as a \n (which is the same value in both UTF-8  \
     * or non-UTF8, so can use the non-utf8 test condition even for a UTF-8    \
     * string */                                                               \
    if (tmp == ! TEST_NON_UTF8('\n')) {                                        \
        IF_SUCCESS;                                                            \
    }                                                                          \
    else {                                                                     \
        IF_FAIL;                                                               \
    }

/* This is the macro to use when we want to see if something that looks like it
 * could match, actually does, and if so exits the loop */
#define REXEC_FBC_TRYIT                            \
    if ((reginfo->intuit || regtry(reginfo, &s)))  \
        goto got_it

/* The only difference between the BOUND and NBOUND cases is that
 * REXEC_FBC_TRYIT is called when matched in BOUND, and when non-matched in
 * NBOUND.  This is accomplished by passing it as either the if or else clause,
 * with the other one being empty (PLACEHOLDER is defined as empty).
 *
 * The TEST_FOO parameters are for operating on different forms of input, but
 * all should be ones that return identically for the same underlying code
 * points */
#define FBC_BOUND(TEST_NON_UTF8, TEST_UV, TEST_UTF8)                           \
    FBC_BOUND_COMMON(                                                          \
          FBC_UTF8(TEST_UV, TEST_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER),          \
          TEST_NON_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER)

#define FBC_BOUND_A(TEST_NON_UTF8)                                             \
    FBC_BOUND_COMMON(                                                          \
            FBC_UTF8_A(TEST_NON_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER),           \
            TEST_NON_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER)

#define FBC_NBOUND(TEST_NON_UTF8, TEST_UV, TEST_UTF8)                          \
    FBC_BOUND_COMMON(                                                          \
          FBC_UTF8(TEST_UV, TEST_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT),          \
          TEST_NON_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT)

#define FBC_NBOUND_A(TEST_NON_UTF8)                                            \
    FBC_BOUND_COMMON(                                                          \
            FBC_UTF8_A(TEST_NON_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT),           \
            TEST_NON_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT)

#ifdef DEBUGGING
static IV
S_get_break_val_cp_checked(SV* const invlist, const UV cp_in) {
  IV cp_out = Perl__invlist_search(invlist, cp_in);
  assert(cp_out >= 0);
  return cp_out;
}
#  define _generic_GET_BREAK_VAL_CP_CHECKED(invlist, invmap, cp) \
	invmap[S_get_break_val_cp_checked(invlist, cp)]
#else
#  define _generic_GET_BREAK_VAL_CP_CHECKED(invlist, invmap, cp) \
	invmap[_invlist_search(invlist, cp)]
#endif

/* Takes a pointer to an inversion list, a pointer to its corresponding
 * inversion map, and a code point, and returns the code point's value
 * according to the two arrays.  It assumes that all code points have a value.
 * This is used as the base macro for macros for particular properties */
#define _generic_GET_BREAK_VAL_CP(invlist, invmap, cp)              \
	_generic_GET_BREAK_VAL_CP_CHECKED(invlist, invmap, cp)

/* Same as above, but takes begin, end ptrs to a UTF-8 encoded string instead
 * of a code point, returning the value for the first code point in the string.
 * And it takes the particular macro name that finds the desired value given a
 * code point.  Merely convert the UTF-8 to code point and call the cp macro */
#define _generic_GET_BREAK_VAL_UTF8(cp_macro, pos, strend)                     \
             (__ASSERT_(pos < strend)                                          \
                 /* Note assumes is valid UTF-8 */                             \
             (cp_macro(utf8_to_uvchr_buf((pos), (strend), NULL))))

/* Returns the GCB value for the input code point */
#define getGCB_VAL_CP(cp)                                                      \
          _generic_GET_BREAK_VAL_CP(                                           \
                                    PL_GCB_invlist,                            \
                                    _Perl_GCB_invmap,                          \
                                    (cp))

/* Returns the GCB value for the first code point in the UTF-8 encoded string
 * bounded by pos and strend */
#define getGCB_VAL_UTF8(pos, strend)                                           \
    _generic_GET_BREAK_VAL_UTF8(getGCB_VAL_CP, pos, strend)

/* Returns the LB value for the input code point */
#define getLB_VAL_CP(cp)                                                       \
          _generic_GET_BREAK_VAL_CP(                                           \
                                    PL_LB_invlist,                             \
                                    _Perl_LB_invmap,                           \
                                    (cp))

/* Returns the LB value for the first code point in the UTF-8 encoded string
 * bounded by pos and strend */
#define getLB_VAL_UTF8(pos, strend)                                            \
    _generic_GET_BREAK_VAL_UTF8(getLB_VAL_CP, pos, strend)


/* Returns the SB value for the input code point */
#define getSB_VAL_CP(cp)                                                       \
          _generic_GET_BREAK_VAL_CP(                                           \
                                    PL_SB_invlist,                             \
                                    _Perl_SB_invmap,                     \
                                    (cp))

/* Returns the SB value for the first code point in the UTF-8 encoded string
 * bounded by pos and strend */
#define getSB_VAL_UTF8(pos, strend)                                            \
    _generic_GET_BREAK_VAL_UTF8(getSB_VAL_CP, pos, strend)

/* Returns the WB value for the input code point */
#define getWB_VAL_CP(cp)                                                       \
          _generic_GET_BREAK_VAL_CP(                                           \
                                    PL_WB_invlist,                             \
                                    _Perl_WB_invmap,                         \
                                    (cp))

/* Returns the WB value for the first code point in the UTF-8 encoded string
 * bounded by pos and strend */
#define getWB_VAL_UTF8(pos, strend)                                            \
    _generic_GET_BREAK_VAL_UTF8(getWB_VAL_CP, pos, strend)

/* We know what class REx starts with.  Try to find this position... */
/* if reginfo->intuit, its a dryrun */
/* annoyingly all the vars in this routine have different names from their counterparts
   in regmatch. /grrr */
STATIC char *
S_find_byclass(pTHX_ regexp * prog, const regnode *c, char *s, 
    const char *strend, regmatch_info *reginfo)
{
    dVAR;

    /* TRUE if x+ need not match at just the 1st pos of run of x's */
    const I32 doevery = (prog->intflags & PREGf_SKIP) == 0;

    char *pat_string;   /* The pattern's exactish string */
    char *pat_end;	    /* ptr to end char of pat_string */
    re_fold_t folder;	/* Function for computing non-utf8 folds */
    const U8 *fold_array;   /* array for folding ords < 256 */
    STRLEN ln;
    STRLEN lnc;
    U8 c1;
    U8 c2;
    char *e;

    /* In some cases we accept only the first occurence of 'x' in a sequence of
     * them.  This variable points to just beyond the end of the previous
     * occurrence of 'x', hence we can tell if we are in a sequence.  (Having
     * it point to beyond the 'x' allows us to work for UTF-8 without having to
     * hop back.) */
    char * previous_occurrence_end = 0;

    I32 tmp;            /* Scratch variable */
    const bool utf8_target = reginfo->is_utf8_target;
    UV utf8_fold_flags = 0;
    const bool is_utf8_pat = reginfo->is_utf8_pat;
    bool to_complement = FALSE; /* Invert the result?  Taking the xor of this
                                   with a result inverts that result, as 0^1 =
                                   1 and 1^1 = 0 */
    _char_class_number classnum;

    RXi_GET_DECL(prog,progi);

    PERL_ARGS_ASSERT_FIND_BYCLASS;

    /* We know what class it must start with. */
    switch (OP(c)) {
    case ANYOFL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;

        if (ANYOFL_UTF8_LOCALE_REQD(FLAGS(c)) && ! IN_UTF8_CTYPE_LOCALE) {
            Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE), utf8_locale_required);
        }

        /* FALLTHROUGH */
    case ANYOFD:
    case ANYOF:
        if (utf8_target) {
            REXEC_FBC_CLASS_SCAN(1, /* 1=>is-utf8 */
                      reginclass(prog, c, (U8*)s, (U8*) strend, utf8_target));
        }
        else if (ANYOF_FLAGS(c)) {
            REXEC_FBC_CLASS_SCAN(0, reginclass(prog,c, (U8*)s, (U8*)s+1, 0));
        }
        else {
            REXEC_FBC_CLASS_SCAN(0, ANYOF_BITMAP_TEST(c, *((U8*)s)));
        }
        break;

    case ANYOFM:    /* ARG() is the base byte; FLAGS() the mask byte */
        /* UTF-8ness doesn't matter, so use 0 */
        REXEC_FBC_FIND_NEXT_SCAN(0,
         (char *) find_next_masked((U8 *) s, (U8 *) strend,
                                   (U8) ARG(c), FLAGS(c)));
        break;

    case EXACTFAA_NO_TRIE: /* This node only generated for non-utf8 patterns */
        assert(! is_utf8_pat);
	/* FALLTHROUGH */
    case EXACTFAA:
        if (is_utf8_pat || utf8_target) {
            utf8_fold_flags = FOLDEQ_UTF8_NOMIX_ASCII;
            goto do_exactf_utf8;
        }
        fold_array = PL_fold_latin1;    /* Latin1 folds are not affected by */
        folder = foldEQ_latin1;	        /* /a, except the sharp s one which */
        goto do_exactf_non_utf8;	/* isn't dealt with by these */

    case EXACTF:   /* This node only generated for non-utf8 patterns */
        assert(! is_utf8_pat);
        if (utf8_target) {
            utf8_fold_flags = 0;
            goto do_exactf_utf8;
        }
        fold_array = PL_fold;
        folder = foldEQ;
        goto do_exactf_non_utf8;

    case EXACTFL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
        if (is_utf8_pat || utf8_target || IN_UTF8_CTYPE_LOCALE) {
            utf8_fold_flags = FOLDEQ_LOCALE;
            goto do_exactf_utf8;
        }
        fold_array = PL_fold_locale;
        folder = foldEQ_locale;
        goto do_exactf_non_utf8;

    case EXACTFU_SS:
        if (is_utf8_pat) {
            utf8_fold_flags = FOLDEQ_S2_ALREADY_FOLDED;
        }
        goto do_exactf_utf8;

    case EXACTFLU8:
            if (! utf8_target) {    /* All code points in this node require
                                       UTF-8 to express.  */
                break;
            }
            utf8_fold_flags =  FOLDEQ_LOCALE | FOLDEQ_S2_ALREADY_FOLDED
                                             | FOLDEQ_S2_FOLDS_SANE;
            goto do_exactf_utf8;

    case EXACTFU:
        if (is_utf8_pat || utf8_target) {
            utf8_fold_flags = is_utf8_pat ? FOLDEQ_S2_ALREADY_FOLDED : 0;
            goto do_exactf_utf8;
        }

        /* Any 'ss' in the pattern should have been replaced by regcomp,
         * so we don't have to worry here about this single special case
         * in the Latin1 range */
        fold_array = PL_fold_latin1;
        folder = foldEQ_latin1;

        /* FALLTHROUGH */

      do_exactf_non_utf8: /* Neither pattern nor string are UTF8, and there
                           are no glitches with fold-length differences
                           between the target string and pattern */

        /* The idea in the non-utf8 EXACTF* cases is to first find the
         * first character of the EXACTF* node and then, if necessary,
         * case-insensitively compare the full text of the node.  c1 is the
         * first character.  c2 is its fold.  This logic will not work for
         * Unicode semantics and the german sharp ss, which hence should
         * not be compiled into a node that gets here. */
        pat_string = STRING(c);
        ln  = STR_LEN(c);	/* length to match in octets/bytes */

        /* We know that we have to match at least 'ln' bytes (which is the
         * same as characters, since not utf8).  If we have to match 3
         * characters, and there are only 2 availabe, we know without
         * trying that it will fail; so don't start a match past the
         * required minimum number from the far end */
        e = HOP3c(strend, -((SSize_t)ln), s);
        if (e < s)
            break;

        c1 = *pat_string;
        c2 = fold_array[c1];
        if (c1 == c2) { /* If char and fold are the same */
            while (s <= e) {
                s = (char *) memchr(s, c1, e + 1 - s);
                if (s == NULL) {
                    break;
                }

                /* Check that the rest of the node matches */
                if (   (ln == 1 || folder(s + 1, pat_string + 1, ln - 1))
                    && (reginfo->intuit || regtry(reginfo, &s)) )
                {
                    goto got_it;
                }
                s++;
            }
        }
        else {
            U8 bits_differing = c1 ^ c2;

            /* If the folds differ in one bit position only, we can mask to
             * match either of them, and can use this faster find method.  Both
             * ASCII and EBCDIC tend to have their case folds differ in only
             * one position, so this is very likely */
            if (LIKELY(PL_bitcount[bits_differing] == 1)) {
                bits_differing = ~ bits_differing;
                while (s <= e) {
                    s = (char *) find_next_masked((U8 *) s, (U8 *) e + 1,
                                        (c1 & bits_differing), bits_differing);
                    if (s > e) {
                        break;
                    }

                    if (   (ln == 1 || folder(s + 1, pat_string + 1, ln - 1))
                        && (reginfo->intuit || regtry(reginfo, &s)) )
                    {
                        goto got_it;
                    }
                    s++;
                }
            }
            else {  /* Otherwise, stuck with looking byte-at-a-time.  This
                       should actually happen only in EXACTFL nodes */
                while (s <= e) {
                    if (    (*(U8*)s == c1 || *(U8*)s == c2)
                        && (ln == 1 || folder(s + 1, pat_string + 1, ln - 1))
                        && (reginfo->intuit || regtry(reginfo, &s)) )
                    {
                        goto got_it;
                    }
                    s++;
                }
            }
        }
        break;

      do_exactf_utf8:
      {
        unsigned expansion;

        /* If one of the operands is in utf8, we can't use the simpler folding
         * above, due to the fact that many different characters can have the
         * same fold, or portion of a fold, or different- length fold */
        pat_string = STRING(c);
        ln  = STR_LEN(c);	/* length to match in octets/bytes */
        pat_end = pat_string + ln;
        lnc = is_utf8_pat       /* length to match in characters */
                ? utf8_length((U8 *) pat_string, (U8 *) pat_end)
                : ln;

        /* We have 'lnc' characters to match in the pattern, but because of
         * multi-character folding, each character in the target can match
         * up to 3 characters (Unicode guarantees it will never exceed
         * this) if it is utf8-encoded; and up to 2 if not (based on the
         * fact that the Latin 1 folds are already determined, and the
         * only multi-char fold in that range is the sharp-s folding to
         * 'ss'.  Thus, a pattern character can match as little as 1/3 of a
         * string character.  Adjust lnc accordingly, rounding up, so that
         * if we need to match at least 4+1/3 chars, that really is 5. */
        expansion = (utf8_target) ? UTF8_MAX_FOLD_CHAR_EXPAND : 2;
        lnc = (lnc + expansion - 1) / expansion;

        /* As in the non-UTF8 case, if we have to match 3 characters, and
         * only 2 are left, it's guaranteed to fail, so don't start a
         * match that would require us to go beyond the end of the string
         */
        e = HOP3c(strend, -((SSize_t)lnc), s);

        /* XXX Note that we could recalculate e to stop the loop earlier,
         * as the worst case expansion above will rarely be met, and as we
         * go along we would usually find that e moves further to the left.
         * This would happen only after we reached the point in the loop
         * where if there were no expansion we should fail.  Unclear if
         * worth the expense */

        while (s <= e) {
            char *my_strend= (char *)strend;
            if (foldEQ_utf8_flags(s, &my_strend, 0,  utf8_target,
                  pat_string, NULL, ln, is_utf8_pat, utf8_fold_flags)
                && (reginfo->intuit || regtry(reginfo, &s)) )
            {
                goto got_it;
            }
            s += (utf8_target) ? UTF8SKIP(s) : 1;
        }
        break;
    }

    case BOUNDL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
        if (FLAGS(c) != TRADITIONAL_BOUND) {
            if (! IN_UTF8_CTYPE_LOCALE) {
                Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE),
                                                B_ON_NON_UTF8_LOCALE_IS_WRONG);
            }
            goto do_boundu;
        }

        FBC_BOUND(isWORDCHAR_LC, isWORDCHAR_LC_uvchr, isWORDCHAR_LC_utf8_safe);
        break;

    case NBOUNDL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
        if (FLAGS(c) != TRADITIONAL_BOUND) {
            if (! IN_UTF8_CTYPE_LOCALE) {
                Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE),
                                                B_ON_NON_UTF8_LOCALE_IS_WRONG);
            }
            goto do_nboundu;
        }

        FBC_NBOUND(isWORDCHAR_LC, isWORDCHAR_LC_uvchr, isWORDCHAR_LC_utf8_safe);
        break;

    case BOUND: /* regcomp.c makes sure that this only has the traditional \b
                   meaning */
        assert(FLAGS(c) == TRADITIONAL_BOUND);

        FBC_BOUND(isWORDCHAR, isWORDCHAR_uni, isWORDCHAR_utf8_safe);
        break;

    case BOUNDA: /* regcomp.c makes sure that this only has the traditional \b
                   meaning */
        assert(FLAGS(c) == TRADITIONAL_BOUND);

        FBC_BOUND_A(isWORDCHAR_A);
        break;

    case NBOUND: /* regcomp.c makes sure that this only has the traditional \b
                   meaning */
        assert(FLAGS(c) == TRADITIONAL_BOUND);

        FBC_NBOUND(isWORDCHAR, isWORDCHAR_uni, isWORDCHAR_utf8_safe);
        break;

    case NBOUNDA: /* regcomp.c makes sure that this only has the traditional \b
                   meaning */
        assert(FLAGS(c) == TRADITIONAL_BOUND);

        FBC_NBOUND_A(isWORDCHAR_A);
        break;

    case NBOUNDU:
        if ((bound_type) FLAGS(c) == TRADITIONAL_BOUND) {
            FBC_NBOUND(isWORDCHAR_L1, isWORDCHAR_uni, isWORDCHAR_utf8_safe);
            break;
        }

      do_nboundu:

        to_complement = 1;
        /* FALLTHROUGH */

    case BOUNDU:
      do_boundu:
        switch((bound_type) FLAGS(c)) {
            case TRADITIONAL_BOUND:
                FBC_BOUND(isWORDCHAR_L1, isWORDCHAR_uni, isWORDCHAR_utf8_safe);
                break;
            case GCB_BOUND:
                if (s == reginfo->strbeg) {
                    if (reginfo->intuit || regtry(reginfo, &s))
                    {
                        goto got_it;
                    }

                    /* Didn't match.  Try at the next position (if there is one) */
                    s += (utf8_target) ? UTF8SKIP(s) : 1;
                    if (UNLIKELY(s >= reginfo->strend)) {
                        break;
                    }
                }

                if (utf8_target) {
                    GCB_enum before = getGCB_VAL_UTF8(
                                               reghop3((U8*)s, -1,
                                                       (U8*)(reginfo->strbeg)),
                                               (U8*) reginfo->strend);
                    while (s < strend) {
                        GCB_enum after = getGCB_VAL_UTF8((U8*) s,
                                                        (U8*) reginfo->strend);
                        if (   (to_complement ^ isGCB(before,
                                                      after,
                                                      (U8*) reginfo->strbeg,
                                                      (U8*) s,
                                                      utf8_target))
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        before = after;
                        s += UTF8SKIP(s);
                    }
                }
                else {  /* Not utf8.  Everything is a GCB except between CR and
                           LF */
                    while (s < strend) {
                        if ((to_complement ^ (   UCHARAT(s - 1) != '\r'
                                              || UCHARAT(s) != '\n'))
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        s++;
                    }
                }

                /* And, since this is a bound, it can match after the final
                 * character in the string */
                if ((reginfo->intuit || regtry(reginfo, &s))) {
                    goto got_it;
                }
                break;

            case LB_BOUND:
                if (s == reginfo->strbeg) {
                    if (reginfo->intuit || regtry(reginfo, &s)) {
                        goto got_it;
                    }
                    s += (utf8_target) ? UTF8SKIP(s) : 1;
                    if (UNLIKELY(s >= reginfo->strend)) {
                        break;
                    }
                }

                if (utf8_target) {
                    LB_enum before = getLB_VAL_UTF8(reghop3((U8*)s,
                                                               -1,
                                                               (U8*)(reginfo->strbeg)),
                                                       (U8*) reginfo->strend);
                    while (s < strend) {
                        LB_enum after = getLB_VAL_UTF8((U8*) s, (U8*) reginfo->strend);
                        if (to_complement ^ isLB(before,
                                                 after,
                                                 (U8*) reginfo->strbeg,
                                                 (U8*) s,
                                                 (U8*) reginfo->strend,
                                                 utf8_target)
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        before = after;
                        s += UTF8SKIP(s);
                    }
                }
                else {  /* Not utf8. */
                    LB_enum before = getLB_VAL_CP((U8) *(s -1));
                    while (s < strend) {
                        LB_enum after = getLB_VAL_CP((U8) *s);
                        if (to_complement ^ isLB(before,
                                                 after,
                                                 (U8*) reginfo->strbeg,
                                                 (U8*) s,
                                                 (U8*) reginfo->strend,
                                                 utf8_target)
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        before = after;
                        s++;
                    }
                }

                if (reginfo->intuit || regtry(reginfo, &s)) {
                    goto got_it;
                }

                break;

            case SB_BOUND:
                if (s == reginfo->strbeg) {
                    if (reginfo->intuit || regtry(reginfo, &s)) {
                        goto got_it;
                    }
                    s += (utf8_target) ? UTF8SKIP(s) : 1;
                    if (UNLIKELY(s >= reginfo->strend)) {
                        break;
                    }
                }

                if (utf8_target) {
                    SB_enum before = getSB_VAL_UTF8(reghop3((U8*)s,
                                                        -1,
                                                        (U8*)(reginfo->strbeg)),
                                                      (U8*) reginfo->strend);
                    while (s < strend) {
                        SB_enum after = getSB_VAL_UTF8((U8*) s,
                                                         (U8*) reginfo->strend);
                        if ((to_complement ^ isSB(before,
                                                  after,
                                                  (U8*) reginfo->strbeg,
                                                  (U8*) s,
                                                  (U8*) reginfo->strend,
                                                  utf8_target))
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        before = after;
                        s += UTF8SKIP(s);
                    }
                }
                else {  /* Not utf8. */
                    SB_enum before = getSB_VAL_CP((U8) *(s -1));
                    while (s < strend) {
                        SB_enum after = getSB_VAL_CP((U8) *s);
                        if ((to_complement ^ isSB(before,
                                                  after,
                                                  (U8*) reginfo->strbeg,
                                                  (U8*) s,
                                                  (U8*) reginfo->strend,
                                                  utf8_target))
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        before = after;
                        s++;
                    }
                }

                /* Here are at the final position in the target string.  The SB
                 * value is always true here, so matches, depending on other
                 * constraints */
                if (reginfo->intuit || regtry(reginfo, &s)) {
                    goto got_it;
                }

                break;

            case WB_BOUND:
                if (s == reginfo->strbeg) {
                    if (reginfo->intuit || regtry(reginfo, &s)) {
                        goto got_it;
                    }
                    s += (utf8_target) ? UTF8SKIP(s) : 1;
                    if (UNLIKELY(s >= reginfo->strend)) {
                        break;
                    }
                }

                if (utf8_target) {
                    /* We are at a boundary between char_sub_0 and char_sub_1.
                     * We also keep track of the value for char_sub_-1 as we
                     * loop through the line.   Context may be needed to make a
                     * determination, and if so, this can save having to
                     * recalculate it */
                    WB_enum previous = WB_UNKNOWN;
                    WB_enum before = getWB_VAL_UTF8(
                                              reghop3((U8*)s,
                                                      -1,
                                                      (U8*)(reginfo->strbeg)),
                                              (U8*) reginfo->strend);
                    while (s < strend) {
                        WB_enum after = getWB_VAL_UTF8((U8*) s,
                                                        (U8*) reginfo->strend);
                        if ((to_complement ^ isWB(previous,
                                                  before,
                                                  after,
                                                  (U8*) reginfo->strbeg,
                                                  (U8*) s,
                                                  (U8*) reginfo->strend,
                                                  utf8_target))
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        previous = before;
                        before = after;
                        s += UTF8SKIP(s);
                    }
                }
                else {  /* Not utf8. */
                    WB_enum previous = WB_UNKNOWN;
                    WB_enum before = getWB_VAL_CP((U8) *(s -1));
                    while (s < strend) {
                        WB_enum after = getWB_VAL_CP((U8) *s);
                        if ((to_complement ^ isWB(previous,
                                                  before,
                                                  after,
                                                  (U8*) reginfo->strbeg,
                                                  (U8*) s,
                                                  (U8*) reginfo->strend,
                                                  utf8_target))
                            && (reginfo->intuit || regtry(reginfo, &s)))
                        {
                            goto got_it;
                        }
                        previous = before;
                        before = after;
                        s++;
                    }
                }

                if (reginfo->intuit || regtry(reginfo, &s)) {
                    goto got_it;
                }
        }
        break;

    case LNBREAK:
        REXEC_FBC_CSCAN(is_LNBREAK_utf8_safe(s, strend),
                        is_LNBREAK_latin1_safe(s, strend)
        );
        break;

    case ASCII:
        REXEC_FBC_FIND_NEXT_SCAN(0, find_next_ascii(s, strend, utf8_target));
        break;

    case NASCII:
        if (utf8_target) {
            REXEC_FBC_FIND_NEXT_SCAN(1, find_next_non_ascii(s, strend,
                                                            utf8_target));
        }
        else {
            REXEC_FBC_FIND_NEXT_SCAN(0, find_next_non_ascii(s, strend,
                                                            utf8_target));
        }

        break;

    /* The argument to all the POSIX node types is the class number to pass to
     * _generic_isCC() to build a mask for searching in PL_charclass[] */

    case NPOSIXL:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
        REXEC_FBC_CSCAN(to_complement ^ cBOOL(isFOO_utf8_lc(FLAGS(c), (U8 *) s, (U8 *) strend)),
                        to_complement ^ cBOOL(isFOO_lc(FLAGS(c), *s)));
        break;

    case NPOSIXD:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXD:
        if (utf8_target) {
            goto posix_utf8;
        }
        goto posixa;

    case NPOSIXA:
        if (utf8_target) {
            /* The complement of something that matches only ASCII matches all
             * non-ASCII, plus everything in ASCII that isn't in the class. */
            REXEC_FBC_CLASS_SCAN(1,   ! isASCII_utf8_safe(s, strend)
                                   || ! _generic_isCC_A(*s, FLAGS(c)));
            break;
        }

        to_complement = 1;
        goto posixa;

    case POSIXA:
        /* Don't need to worry about utf8, as it can match only a single
         * byte invariant character.  But we do anyway for performance reasons,
         * as otherwise we would have to examine all the continuation
         * characters */
        if (utf8_target) {
            REXEC_FBC_CLASS_SCAN(1, _generic_isCC_A(*s, FLAGS(c)));
            break;
        }

      posixa:
        REXEC_FBC_CLASS_SCAN(0, /* 0=>not-utf8 */
                        to_complement ^ cBOOL(_generic_isCC_A(*s, FLAGS(c))));
        break;

    case NPOSIXU:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXU:
        if (! utf8_target) {
            REXEC_FBC_CLASS_SCAN(0, /* 0=>not-utf8 */
                                 to_complement ^ cBOOL(_generic_isCC(*s,
                                                                    FLAGS(c))));
        }
        else {

          posix_utf8:
            classnum = (_char_class_number) FLAGS(c);
            switch (classnum) {
                default:
                    REXEC_FBC_CLASS_SCAN(1, /* 1=>is-utf8 */
                        to_complement ^ cBOOL(_invlist_contains_cp(
                                              PL_XPosix_ptrs[classnum],
                                              utf8_to_uvchr_buf((U8 *) s,
                                                                (U8 *) strend,
                                                                NULL))));
                    break;
                case _CC_ENUM_SPACE:
                    REXEC_FBC_CLASS_SCAN(1, /* 1=>is-utf8 */
                        to_complement ^ cBOOL(isSPACE_utf8_safe(s, strend)));
                    break;

                case _CC_ENUM_BLANK:
                    REXEC_FBC_CLASS_SCAN(1,
                        to_complement ^ cBOOL(isBLANK_utf8_safe(s, strend)));
                    break;

                case _CC_ENUM_XDIGIT:
                    REXEC_FBC_CLASS_SCAN(1,
                       to_complement ^ cBOOL(isXDIGIT_utf8_safe(s, strend)));
                    break;

                case _CC_ENUM_VERTSPACE:
                    REXEC_FBC_CLASS_SCAN(1,
                       to_complement ^ cBOOL(isVERTWS_utf8_safe(s, strend)));
                    break;

                case _CC_ENUM_CNTRL:
                    REXEC_FBC_CLASS_SCAN(1,
                        to_complement ^ cBOOL(isCNTRL_utf8_safe(s, strend)));
                    break;
            }
        }
        break;

    case AHOCORASICKC:
    case AHOCORASICK:
        {
            DECL_TRIE_TYPE(c);
            /* what trie are we using right now */
            reg_ac_data *aho = (reg_ac_data*)progi->data->data[ ARG( c ) ];
            reg_trie_data *trie = (reg_trie_data*)progi->data->data[ aho->trie ];
            HV *widecharmap = MUTABLE_HV(progi->data->data[ aho->trie + 1 ]);

            const char *last_start = strend - trie->minlen;
#ifdef DEBUGGING
            const char *real_start = s;
#endif
            STRLEN maxlen = trie->maxlen;
            SV *sv_points;
            U8 **points; /* map of where we were in the input string
                            when reading a given char. For ASCII this
                            is unnecessary overhead as the relationship
                            is always 1:1, but for Unicode, especially
                            case folded Unicode this is not true. */
            U8 foldbuf[ UTF8_MAXBYTES_CASE + 1 ];
            U8 *bitmap=NULL;


            GET_RE_DEBUG_FLAGS_DECL;

            /* We can't just allocate points here. We need to wrap it in
             * an SV so it gets freed properly if there is a croak while
             * running the match */
            ENTER;
            SAVETMPS;
            sv_points=newSV(maxlen * sizeof(U8 *));
            SvCUR_set(sv_points,
                maxlen * sizeof(U8 *));
            SvPOK_on(sv_points);
            sv_2mortal(sv_points);
            points=(U8**)SvPV_nolen(sv_points );
            if ( trie_type != trie_utf8_fold
                 && (trie->bitmap || OP(c)==AHOCORASICKC) )
            {
                if (trie->bitmap)
                    bitmap=(U8*)trie->bitmap;
                else
                    bitmap=(U8*)ANYOF_BITMAP(c);
            }
            /* this is the Aho-Corasick algorithm modified a touch
               to include special handling for long "unknown char" sequences.
               The basic idea being that we use AC as long as we are dealing
               with a possible matching char, when we encounter an unknown char
               (and we have not encountered an accepting state) we scan forward
               until we find a legal starting char.
               AC matching is basically that of trie matching, except that when
               we encounter a failing transition, we fall back to the current
               states "fail state", and try the current char again, a process
               we repeat until we reach the root state, state 1, or a legal
               transition. If we fail on the root state then we can either
               terminate if we have reached an accepting state previously, or
               restart the entire process from the beginning if we have not.

             */
            while (s <= last_start) {
                const U32 uniflags = UTF8_ALLOW_DEFAULT;
                U8 *uc = (U8*)s;
                U16 charid = 0;
                U32 base = 1;
                U32 state = 1;
                UV uvc = 0;
                STRLEN len = 0;
                STRLEN foldlen = 0;
                U8 *uscan = (U8*)NULL;
                U8 *leftmost = NULL;
#ifdef DEBUGGING
                U32 accepted_word= 0;
#endif
                U32 pointpos = 0;

                while ( state && uc <= (U8*)strend ) {
                    int failed=0;
                    U32 word = aho->states[ state ].wordnum;

                    if( state==1 ) {
                        if ( bitmap ) {
                            DEBUG_TRIE_EXECUTE_r(
                                if ( uc <= (U8*)last_start && !BITMAP_TEST(bitmap,*uc) ) {
                                    dump_exec_pos( (char *)uc, c, strend, real_start,
                                        (char *)uc, utf8_target, 0 );
                                    Perl_re_printf( aTHX_
                                        " Scanning for legal start char...\n");
                                }
                            );
                            if (utf8_target) {
                                while ( uc <= (U8*)last_start && !BITMAP_TEST(bitmap,*uc) ) {
                                    uc += UTF8SKIP(uc);
                                }
                            } else {
                                while ( uc <= (U8*)last_start  && !BITMAP_TEST(bitmap,*uc) ) {
                                    uc++;
                                }
                            }
                            s= (char *)uc;
                        }
                        if (uc >(U8*)last_start) break;
                    }

                    if ( word ) {
                        U8 *lpos= points[ (pointpos - trie->wordinfo[word].len) % maxlen ];
                        if (!leftmost || lpos < leftmost) {
                            DEBUG_r(accepted_word=word);
                            leftmost= lpos;
                        }
                        if (base==0) break;

                    }
                    points[pointpos++ % maxlen]= uc;
                    if (foldlen || uc < (U8*)strend) {
                        REXEC_TRIE_READ_CHAR(trie_type, trie, widecharmap, uc,
                                             (U8 *) strend, uscan, len, uvc,
                                             charid, foldlen, foldbuf,
                                             uniflags);
                        DEBUG_TRIE_EXECUTE_r({
                            dump_exec_pos( (char *)uc, c, strend,
                                        real_start, s, utf8_target, 0);
                            Perl_re_printf( aTHX_
                                " Charid:%3u CP:%4" UVxf " ",
                                 charid, uvc);
                        });
                    }
                    else {
                        len = 0;
                        charid = 0;
                    }


                    do {
#ifdef DEBUGGING
                        word = aho->states[ state ].wordnum;
#endif
                        base = aho->states[ state ].trans.base;

                        DEBUG_TRIE_EXECUTE_r({
                            if (failed)
                                dump_exec_pos( (char *)uc, c, strend, real_start,
                                    s,   utf8_target, 0 );
                            Perl_re_printf( aTHX_
                                "%sState: %4" UVxf ", word=%" UVxf,
                                failed ? " Fail transition to " : "",
                                (UV)state, (UV)word);
                        });
                        if ( base ) {
                            U32 tmp;
                            I32 offset;
                            if (charid &&
                                 ( ((offset = base + charid
                                    - 1 - trie->uniquecharcount)) >= 0)
                                 && ((U32)offset < trie->lasttrans)
                                 && trie->trans[offset].check == state
                                 && (tmp=trie->trans[offset].next))
                            {
                                DEBUG_TRIE_EXECUTE_r(
                                    Perl_re_printf( aTHX_ " - legal\n"));
                                state = tmp;
                                break;
                            }
                            else {
                                DEBUG_TRIE_EXECUTE_r(
                                    Perl_re_printf( aTHX_ " - fail\n"));
                                failed = 1;
                                state = aho->fail[state];
                            }
                        }
                        else {
                            /* we must be accepting here */
                            DEBUG_TRIE_EXECUTE_r(
                                    Perl_re_printf( aTHX_ " - accepting\n"));
                            failed = 1;
                            break;
                        }
                    } while(state);
                    uc += len;
                    if (failed) {
                        if (leftmost)
                            break;
                        if (!state) state = 1;
                    }
                }
                if ( aho->states[ state ].wordnum ) {
                    U8 *lpos = points[ (pointpos - trie->wordinfo[aho->states[ state ].wordnum].len) % maxlen ];
                    if (!leftmost || lpos < leftmost) {
                        DEBUG_r(accepted_word=aho->states[ state ].wordnum);
                        leftmost = lpos;
                    }
                }
                if (leftmost) {
                    s = (char*)leftmost;
                    DEBUG_TRIE_EXECUTE_r({
                        Perl_re_printf( aTHX_  "Matches word #%" UVxf " at position %" IVdf ". Trying full pattern...\n",
                            (UV)accepted_word, (IV)(s - real_start)
                        );
                    });
                    if (reginfo->intuit || regtry(reginfo, &s)) {
                        FREETMPS;
                        LEAVE;
                        goto got_it;
                    }
                    s = HOPc(s,1);
                    DEBUG_TRIE_EXECUTE_r({
                        Perl_re_printf( aTHX_ "Pattern failed. Looking for new start point...\n");
                    });
                } else {
                    DEBUG_TRIE_EXECUTE_r(
                        Perl_re_printf( aTHX_ "No match.\n"));
                    break;
                }
            }
            FREETMPS;
            LEAVE;
        }
        break;
    default:
        Perl_croak(aTHX_ "panic: unknown regstclass %d", (int)OP(c));
    }
    return 0;
  got_it:
    return s;
}

/* set RX_SAVED_COPY, RX_SUBBEG etc.
 * flags have same meanings as with regexec_flags() */

static void
S_reg_set_capture_string(pTHX_ REGEXP * const rx,
                            char *strbeg,
                            char *strend,
                            SV *sv,
                            U32 flags,
                            bool utf8_target)
{
    struct regexp *const prog = ReANY(rx);

    if (flags & REXEC_COPY_STR) {
#ifdef PERL_ANY_COW
        if (SvCANCOW(sv)) {
            DEBUG_C(Perl_re_printf( aTHX_
                              "Copy on write: regexp capture, type %d\n",
                                    (int) SvTYPE(sv)));
            /* Create a new COW SV to share the match string and store
             * in saved_copy, unless the current COW SV in saved_copy
             * is valid and suitable for our purpose */
            if ((   prog->saved_copy
                 && SvIsCOW(prog->saved_copy)
                 && SvPOKp(prog->saved_copy)
                 && SvIsCOW(sv)
                 && SvPOKp(sv)
                 && SvPVX(sv) == SvPVX(prog->saved_copy)))
            {
                /* just reuse saved_copy SV */
                if (RXp_MATCH_COPIED(prog)) {
                    Safefree(prog->subbeg);
                    RXp_MATCH_COPIED_off(prog);
                }
            }
            else {
                /* create new COW SV to share string */
                RXp_MATCH_COPY_FREE(prog);
                prog->saved_copy = sv_setsv_cow(prog->saved_copy, sv);
            }
            prog->subbeg = (char *)SvPVX_const(prog->saved_copy);
            assert (SvPOKp(prog->saved_copy));
            prog->sublen  = strend - strbeg;
            prog->suboffset = 0;
            prog->subcoffset = 0;
        } else
#endif
        {
            SSize_t min = 0;
            SSize_t max = strend - strbeg;
            SSize_t sublen;

            if (    (flags & REXEC_COPY_SKIP_POST)
                && !(prog->extflags & RXf_PMf_KEEPCOPY) /* //p */
                && !(PL_sawampersand & SAWAMPERSAND_RIGHT)
            ) { /* don't copy $' part of string */
                U32 n = 0;
                max = -1;
                /* calculate the right-most part of the string covered
                 * by a capture. Due to lookahead, this may be to
                 * the right of $&, so we have to scan all captures */
                while (n <= prog->lastparen) {
                    if (prog->offs[n].end > max)
                        max = prog->offs[n].end;
                    n++;
                }
                if (max == -1)
                    max = (PL_sawampersand & SAWAMPERSAND_LEFT)
                            ? prog->offs[0].start
                            : 0;
                assert(max >= 0 && max <= strend - strbeg);
            }

            if (    (flags & REXEC_COPY_SKIP_PRE)
                && !(prog->extflags & RXf_PMf_KEEPCOPY) /* //p */
                && !(PL_sawampersand & SAWAMPERSAND_LEFT)
            ) { /* don't copy $` part of string */
                U32 n = 0;
                min = max;
                /* calculate the left-most part of the string covered
                 * by a capture. Due to lookbehind, this may be to
                 * the left of $&, so we have to scan all captures */
                while (min && n <= prog->lastparen) {
                    if (   prog->offs[n].start != -1
                        && prog->offs[n].start < min)
                    {
                        min = prog->offs[n].start;
                    }
                    n++;
                }
                if ((PL_sawampersand & SAWAMPERSAND_RIGHT)
                    && min >  prog->offs[0].end
                )
                    min = prog->offs[0].end;

            }

            assert(min >= 0 && min <= max && min <= strend - strbeg);
            sublen = max - min;

            if (RXp_MATCH_COPIED(prog)) {
                if (sublen > prog->sublen)
                    prog->subbeg =
                            (char*)saferealloc(prog->subbeg, sublen+1);
            }
            else
                prog->subbeg = (char*)safemalloc(sublen+1);
            Copy(strbeg + min, prog->subbeg, sublen, char);
            prog->subbeg[sublen] = '\0';
            prog->suboffset = min;
            prog->sublen = sublen;
            RXp_MATCH_COPIED_on(prog);
        }
        prog->subcoffset = prog->suboffset;
        if (prog->suboffset && utf8_target) {
            /* Convert byte offset to chars.
             * XXX ideally should only compute this if @-/@+
             * has been seen, a la PL_sawampersand ??? */

            /* If there's a direct correspondence between the
             * string which we're matching and the original SV,
             * then we can use the utf8 len cache associated with
             * the SV. In particular, it means that under //g,
             * sv_pos_b2u() will use the previously cached
             * position to speed up working out the new length of
             * subcoffset, rather than counting from the start of
             * the string each time. This stops
             *   $x = "\x{100}" x 1E6; 1 while $x =~ /(.)/g;
             * from going quadratic */
            if (SvPOKp(sv) && SvPVX(sv) == strbeg)
                prog->subcoffset = sv_pos_b2u_flags(sv, prog->subcoffset,
                                                SV_GMAGIC|SV_CONST_RETURN);
            else
                prog->subcoffset = utf8_length((U8*)strbeg,
                                    (U8*)(strbeg+prog->suboffset));
        }
    }
    else {
        RXp_MATCH_COPY_FREE(prog);
        prog->subbeg = strbeg;
        prog->suboffset = 0;
        prog->subcoffset = 0;
        prog->sublen = strend - strbeg;
    }
}




/*
 - regexec_flags - match a regexp against a string
 */
I32
Perl_regexec_flags(pTHX_ REGEXP * const rx, char *stringarg, char *strend,
	      char *strbeg, SSize_t minend, SV *sv, void *data, U32 flags)
/* stringarg: the point in the string at which to begin matching */
/* strend:    pointer to null at end of string */
/* strbeg:    real beginning of string */
/* minend:    end of match must be >= minend bytes after stringarg. */
/* sv:        SV being matched: only used for utf8 flag, pos() etc; string
 *            itself is accessed via the pointers above */
/* data:      May be used for some additional optimizations.
              Currently unused. */
/* flags:     For optimizations. See REXEC_* in regexp.h */

{
    struct regexp *const prog = ReANY(rx);
    char *s;
    regnode *c;
    char *startpos;
    SSize_t minlen;		/* must match at least this many chars */
    SSize_t dontbother = 0;	/* how many characters not to try at end */
    const bool utf8_target = cBOOL(DO_UTF8(sv));
    I32 multiline;
    RXi_GET_DECL(prog,progi);
    regmatch_info reginfo_buf;  /* create some info to pass to regtry etc */
    regmatch_info *const reginfo = &reginfo_buf;
    regexp_paren_pair *swap = NULL;
    I32 oldsave;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGEXEC_FLAGS;
    PERL_UNUSED_ARG(data);

    /* Be paranoid... */
    if (prog == NULL) {
	Perl_croak(aTHX_ "NULL regexp parameter");
    }

    DEBUG_EXECUTE_r(
        debug_start_match(rx, utf8_target, stringarg, strend,
        "Matching");
    );

    startpos = stringarg;

    /* set these early as they may be used by the HOP macros below */
    reginfo->strbeg = strbeg;
    reginfo->strend = strend;
    reginfo->is_utf8_target = cBOOL(utf8_target);

    if (prog->intflags & PREGf_GPOS_SEEN) {
        MAGIC *mg;

        /* set reginfo->ganch, the position where \G can match */

        reginfo->ganch =
            (flags & REXEC_IGNOREPOS)
            ? stringarg /* use start pos rather than pos() */
            : ((mg = mg_find_mglob(sv)) && mg->mg_len >= 0)
              /* Defined pos(): */
            ? strbeg + MgBYTEPOS(mg, sv, strbeg, strend-strbeg)
            : strbeg; /* pos() not defined; use start of string */

        DEBUG_GPOS_r(Perl_re_printf( aTHX_
            "GPOS ganch set to strbeg[%" IVdf "]\n", (IV)(reginfo->ganch - strbeg)));

        /* in the presence of \G, we may need to start looking earlier in
         * the string than the suggested start point of stringarg:
         * if prog->gofs is set, then that's a known, fixed minimum
         * offset, such as
         * /..\G/:   gofs = 2
         * /ab|c\G/: gofs = 1
         * or if the minimum offset isn't known, then we have to go back
         * to the start of the string, e.g. /w+\G/
         */

        if (prog->intflags & PREGf_ANCH_GPOS) {
            if (prog->gofs) {
                startpos = HOPBACKc(reginfo->ganch, prog->gofs);
                if (!startpos ||
                    ((flags & REXEC_FAIL_ON_UNDERFLOW) && startpos < stringarg))
                {
                    DEBUG_r(Perl_re_printf( aTHX_
                            "fail: ganch-gofs before earliest possible start\n"));
                    return 0;
                }
            }
            else
                startpos = reginfo->ganch;
        }
        else if (prog->gofs) {
            startpos = HOPBACKc(startpos, prog->gofs);
            if (!startpos)
                startpos = strbeg;
        }
        else if (prog->intflags & PREGf_GPOS_FLOAT)
            startpos = strbeg;
    }

    minlen = prog->minlen;
    if ((startpos + minlen) > strend || startpos < strbeg) {
        DEBUG_r(Perl_re_printf( aTHX_
                    "Regex match can't succeed, so not even tried\n"));
        return 0;
    }

    /* at the end of this function, we'll do a LEAVE_SCOPE(oldsave),
     * which will call destuctors to reset PL_regmatch_state, free higher
     * PL_regmatch_slabs, and clean up regmatch_info_aux and
     * regmatch_info_aux_eval */

    oldsave = PL_savestack_ix;

    s = startpos;

    if ((prog->extflags & RXf_USE_INTUIT)
        && !(flags & REXEC_CHECKED))
    {
	s = re_intuit_start(rx, sv, strbeg, startpos, strend,
                                    flags, NULL);
	if (!s)
	    return 0;

	if (prog->extflags & RXf_CHECK_ALL) {
            /* we can match based purely on the result of INTUIT.
             * Set up captures etc just for $& and $-[0]
             * (an intuit-only match wont have $1,$2,..) */
            assert(!prog->nparens);

            /* s/// doesn't like it if $& is earlier than where we asked it to
             * start searching (which can happen on something like /.\G/) */
            if (       (flags & REXEC_FAIL_ON_UNDERFLOW)
                    && (s < stringarg))
            {
                /* this should only be possible under \G */
                assert(prog->intflags & PREGf_GPOS_SEEN);
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                    "matched, but failing for REXEC_FAIL_ON_UNDERFLOW\n"));
                goto phooey;
            }

            /* match via INTUIT shouldn't have any captures.
             * Let @-, @+, $^N know */
            prog->lastparen = prog->lastcloseparen = 0;
            RXp_MATCH_UTF8_set(prog, utf8_target);
            prog->offs[0].start = s - strbeg;
            prog->offs[0].end = utf8_target
                ? (char*)utf8_hop((U8*)s, prog->minlenret) - strbeg
                : s - strbeg + prog->minlenret;
            if ( !(flags & REXEC_NOT_FIRST) )
                S_reg_set_capture_string(aTHX_ rx,
                                        strbeg, strend,
                                        sv, flags, utf8_target);

	    return 1;
        }
    }

    multiline = prog->extflags & RXf_PMf_MULTILINE;
    
    if (strend - s < (minlen+(prog->check_offset_min<0?prog->check_offset_min:0))) {
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
			      "String too short [regexec_flags]...\n"));
	goto phooey;
    }
    
    /* Check validity of program. */
    if (UCHARAT(progi->program) != REG_MAGIC) {
	Perl_croak(aTHX_ "corrupted regexp program");
    }

    RXp_MATCH_TAINTED_off(prog);
    RXp_MATCH_UTF8_set(prog, utf8_target);

    reginfo->prog = rx;	 /* Yes, sorry that this is confusing.  */
    reginfo->intuit = 0;
    reginfo->is_utf8_pat = cBOOL(RX_UTF8(rx));
    reginfo->warned = FALSE;
    reginfo->sv = sv;
    reginfo->poscache_maxiter = 0; /* not yet started a countdown */
    /* see how far we have to get to not match where we matched before */
    reginfo->till = stringarg + minend;

    if (prog->extflags & RXf_EVAL_SEEN && SvPADTMP(sv)) {
        /* SAVEFREESV, not sv_mortalcopy, as this SV must last until after
           S_cleanup_regmatch_info_aux has executed (registered by
           SAVEDESTRUCTOR_X below).  S_cleanup_regmatch_info_aux modifies
           magic belonging to this SV.
           Not newSVsv, either, as it does not COW.
        */
        reginfo->sv = newSV(0);
        SvSetSV_nosteal(reginfo->sv, sv);
        SAVEFREESV(reginfo->sv);
    }

    /* reserve next 2 or 3 slots in PL_regmatch_state:
     * slot N+0: may currently be in use: skip it
     * slot N+1: use for regmatch_info_aux struct
     * slot N+2: use for regmatch_info_aux_eval struct if we have (?{})'s
     * slot N+3: ready for use by regmatch()
     */

    {
        regmatch_state *old_regmatch_state;
        regmatch_slab  *old_regmatch_slab;
        int i, max = (prog->extflags & RXf_EVAL_SEEN) ? 2 : 1;

        /* on first ever match, allocate first slab */
        if (!PL_regmatch_slab) {
            Newx(PL_regmatch_slab, 1, regmatch_slab);
            PL_regmatch_slab->prev = NULL;
            PL_regmatch_slab->next = NULL;
            PL_regmatch_state = SLAB_FIRST(PL_regmatch_slab);
        }

        old_regmatch_state = PL_regmatch_state;
        old_regmatch_slab  = PL_regmatch_slab;

        for (i=0; i <= max; i++) {
            if (i == 1)
                reginfo->info_aux = &(PL_regmatch_state->u.info_aux);
            else if (i ==2)
                reginfo->info_aux_eval =
                reginfo->info_aux->info_aux_eval =
                            &(PL_regmatch_state->u.info_aux_eval);

            if (++PL_regmatch_state >  SLAB_LAST(PL_regmatch_slab))
                PL_regmatch_state = S_push_slab(aTHX);
        }

        /* note initial PL_regmatch_state position; at end of match we'll
         * pop back to there and free any higher slabs */

        reginfo->info_aux->old_regmatch_state = old_regmatch_state;
        reginfo->info_aux->old_regmatch_slab  = old_regmatch_slab;
        reginfo->info_aux->poscache = NULL;

        SAVEDESTRUCTOR_X(S_cleanup_regmatch_info_aux, reginfo->info_aux);

        if ((prog->extflags & RXf_EVAL_SEEN))
            S_setup_eval_state(aTHX_ reginfo);
        else
            reginfo->info_aux_eval = reginfo->info_aux->info_aux_eval = NULL;
    }

    /* If there is a "must appear" string, look for it. */

    if (PL_curpm && (PM_GETRE(PL_curpm) == rx)) {
        /* We have to be careful. If the previous successful match
           was from this regex we don't want a subsequent partially
           successful match to clobber the old results.
           So when we detect this possibility we add a swap buffer
           to the re, and switch the buffer each match. If we fail,
           we switch it back; otherwise we leave it swapped.
        */
        swap = prog->offs;
        /* do we need a save destructor here for eval dies? */
        Newxz(prog->offs, (prog->nparens + 1), regexp_paren_pair);
        DEBUG_BUFFERS_r(Perl_re_exec_indentf( aTHX_
	    "rex=0x%" UVxf " saving  offs: orig=0x%" UVxf " new=0x%" UVxf "\n",
	    0,
            PTR2UV(prog),
	    PTR2UV(swap),
	    PTR2UV(prog->offs)
	));
    }

    if (prog->recurse_locinput)
        Zero(prog->recurse_locinput,prog->nparens + 1, char *);

    /* Simplest case: anchored match need be tried only once, or with
     * MBOL, only at the beginning of each line.
     *
     * Note that /.*.../ sets PREGf_IMPLICIT|MBOL, while /.*.../s sets
     * PREGf_IMPLICIT|SBOL. The idea is that with /.*.../s, if it doesn't
     * match at the start of the string then it won't match anywhere else
     * either; while with /.*.../, if it doesn't match at the beginning,
     * the earliest it could match is at the start of the next line */

    if (prog->intflags & (PREGf_ANCH & ~PREGf_ANCH_GPOS)) {
        char *end;

	if (regtry(reginfo, &s))
	    goto got_it;

        if (!(prog->intflags & PREGf_ANCH_MBOL))
            goto phooey;

        /* didn't match at start, try at other newline positions */

        if (minlen)
            dontbother = minlen - 1;
        end = HOP3c(strend, -dontbother, strbeg) - 1;

        /* skip to next newline */

        while (s <= end) { /* note it could be possible to match at the end of the string */
            /* NB: newlines are the same in unicode as they are in latin */
            if (*s++ != '\n')
                continue;
            if (prog->check_substr || prog->check_utf8) {
            /* note that with PREGf_IMPLICIT, intuit can only fail
             * or return the start position, so it's of limited utility.
             * Nevertheless, I made the decision that the potential for
             * quick fail was still worth it - DAPM */
                s = re_intuit_start(rx, sv, strbeg, s, strend, flags, NULL);
                if (!s)
                    goto phooey;
            }
            if (regtry(reginfo, &s))
                goto got_it;
        }
        goto phooey;
    } /* end anchored search */

    if (prog->intflags & PREGf_ANCH_GPOS)
    {
        /* PREGf_ANCH_GPOS should never be true if PREGf_GPOS_SEEN is not true */
        assert(prog->intflags & PREGf_GPOS_SEEN);
        /* For anchored \G, the only position it can match from is
         * (ganch-gofs); we already set startpos to this above; if intuit
         * moved us on from there, we can't possibly succeed */
        assert(startpos == HOPBACKc(reginfo->ganch, prog->gofs));
	if (s == startpos && regtry(reginfo, &s))
	    goto got_it;
	goto phooey;
    }

    /* Messy cases:  unanchored match. */
    if ((prog->anchored_substr || prog->anchored_utf8) && prog->intflags & PREGf_SKIP) {
	/* we have /x+whatever/ */
	/* it must be a one character string (XXXX Except is_utf8_pat?) */
	char ch;
#ifdef DEBUGGING
	int did_match = 0;
#endif
	if (utf8_target) {
            if (! prog->anchored_utf8) {
                to_utf8_substr(prog);
            }
            ch = SvPVX_const(prog->anchored_utf8)[0];
	    REXEC_FBC_SCAN(0,   /* 0=>not-utf8 */
		if (*s == ch) {
		    DEBUG_EXECUTE_r( did_match = 1 );
		    if (regtry(reginfo, &s)) goto got_it;
		    s += UTF8SKIP(s);
		    while (s < strend && *s == ch)
			s += UTF8SKIP(s);
		}
	    );

	}
	else {
            if (! prog->anchored_substr) {
                if (! to_byte_substr(prog)) {
                    NON_UTF8_TARGET_BUT_UTF8_REQUIRED(phooey);
                }
            }
            ch = SvPVX_const(prog->anchored_substr)[0];
	    REXEC_FBC_SCAN(0,   /* 0=>not-utf8 */
		if (*s == ch) {
		    DEBUG_EXECUTE_r( did_match = 1 );
		    if (regtry(reginfo, &s)) goto got_it;
		    s++;
		    while (s < strend && *s == ch)
			s++;
		}
	    );
	}
	DEBUG_EXECUTE_r(if (!did_match)
                Perl_re_printf( aTHX_
                                  "Did not find anchored character...\n")
               );
    }
    else if (prog->anchored_substr != NULL
	      || prog->anchored_utf8 != NULL
	      || ((prog->float_substr != NULL || prog->float_utf8 != NULL)
		  && prog->float_max_offset < strend - s)) {
	SV *must;
	SSize_t back_max;
	SSize_t back_min;
	char *last;
	char *last1;		/* Last position checked before */
#ifdef DEBUGGING
	int did_match = 0;
#endif
	if (prog->anchored_substr || prog->anchored_utf8) {
	    if (utf8_target) {
                if (! prog->anchored_utf8) {
                    to_utf8_substr(prog);
                }
                must = prog->anchored_utf8;
            }
            else {
                if (! prog->anchored_substr) {
                    if (! to_byte_substr(prog)) {
                        NON_UTF8_TARGET_BUT_UTF8_REQUIRED(phooey);
                    }
                }
                must = prog->anchored_substr;
            }
	    back_max = back_min = prog->anchored_offset;
	} else {
	    if (utf8_target) {
                if (! prog->float_utf8) {
                    to_utf8_substr(prog);
                }
                must = prog->float_utf8;
            }
            else {
                if (! prog->float_substr) {
                    if (! to_byte_substr(prog)) {
                        NON_UTF8_TARGET_BUT_UTF8_REQUIRED(phooey);
                    }
                }
                must = prog->float_substr;
            }
	    back_max = prog->float_max_offset;
	    back_min = prog->float_min_offset;
	}
	    
        if (back_min<0) {
	    last = strend;
	} else {
            last = HOP3c(strend,	/* Cannot start after this */
        	  -(SSize_t)(CHR_SVLEN(must)
        		 - (SvTAIL(must) != 0) + back_min), strbeg);
        }
	if (s > reginfo->strbeg)
	    last1 = HOPc(s, -1);
	else
	    last1 = s - 1;	/* bogus */

	/* XXXX check_substr already used to find "s", can optimize if
	   check_substr==must. */
	dontbother = 0;
	strend = HOPc(strend, -dontbother);
	while ( (s <= last) &&
		(s = fbm_instr((unsigned char*)HOP4c(s, back_min, strbeg,  strend),
				  (unsigned char*)strend, must,
				  multiline ? FBMrf_MULTILINE : 0)) ) {
	    DEBUG_EXECUTE_r( did_match = 1 );
	    if (HOPc(s, -back_max) > last1) {
		last1 = HOPc(s, -back_min);
		s = HOPc(s, -back_max);
	    }
	    else {
		char * const t = (last1 >= reginfo->strbeg)
                                    ? HOPc(last1, 1) : last1 + 1;

		last1 = HOPc(s, -back_min);
		s = t;
	    }
	    if (utf8_target) {
		while (s <= last1) {
		    if (regtry(reginfo, &s))
			goto got_it;
                    if (s >= last1) {
                        s++; /* to break out of outer loop */
                        break;
                    }
                    s += UTF8SKIP(s);
		}
	    }
	    else {
		while (s <= last1) {
		    if (regtry(reginfo, &s))
			goto got_it;
		    s++;
		}
	    }
	}
	DEBUG_EXECUTE_r(if (!did_match) {
            RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
                SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
            Perl_re_printf( aTHX_  "Did not find %s substr %s%s...\n",
			      ((must == prog->anchored_substr || must == prog->anchored_utf8)
			       ? "anchored" : "floating"),
                quoted, RE_SV_TAIL(must));
        });		    
	goto phooey;
    }
    else if ( (c = progi->regstclass) ) {
	if (minlen) {
	    const OPCODE op = OP(progi->regstclass);
	    /* don't bother with what can't match */
	    if (PL_regkind[op] != EXACT && PL_regkind[op] != TRIE)
	        strend = HOPc(strend, -(minlen - 1));
	}
	DEBUG_EXECUTE_r({
	    SV * const prop = sv_newmortal();
            regprop(prog, prop, c, reginfo, NULL);
	    {
		RE_PV_QUOTED_DECL(quoted,utf8_target,PERL_DEBUG_PAD_ZERO(1),
		    s,strend-s,PL_dump_re_max_len);
                Perl_re_printf( aTHX_
		    "Matching stclass %.*s against %s (%d bytes)\n",
		    (int)SvCUR(prop), SvPVX_const(prop),
		     quoted, (int)(strend - s));
	    }
	});
        if (find_byclass(prog, c, s, strend, reginfo))
	    goto got_it;
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "Contradicts stclass... [regexec_flags]\n"));
    }
    else {
	dontbother = 0;
	if (prog->float_substr != NULL || prog->float_utf8 != NULL) {
	    /* Trim the end. */
	    char *last= NULL;
	    SV* float_real;
	    STRLEN len;
	    const char *little;

	    if (utf8_target) {
                if (! prog->float_utf8) {
                    to_utf8_substr(prog);
                }
                float_real = prog->float_utf8;
            }
            else {
                if (! prog->float_substr) {
                    if (! to_byte_substr(prog)) {
                        NON_UTF8_TARGET_BUT_UTF8_REQUIRED(phooey);
                    }
                }
                float_real = prog->float_substr;
            }

            little = SvPV_const(float_real, len);
	    if (SvTAIL(float_real)) {
                    /* This means that float_real contains an artificial \n on
                     * the end due to the presence of something like this:
                     * /foo$/ where we can match both "foo" and "foo\n" at the
                     * end of the string.  So we have to compare the end of the
                     * string first against the float_real without the \n and
                     * then against the full float_real with the string.  We
                     * have to watch out for cases where the string might be
                     * smaller than the float_real or the float_real without
                     * the \n. */
		    char *checkpos= strend - len;
		    DEBUG_OPTIMISE_r(
                        Perl_re_printf( aTHX_
			    "%sChecking for float_real.%s\n",
			    PL_colors[4], PL_colors[5]));
		    if (checkpos + 1 < strbeg) {
                        /* can't match, even if we remove the trailing \n
                         * string is too short to match */
			DEBUG_EXECUTE_r(
                            Perl_re_printf( aTHX_
				"%sString shorter than required trailing substring, cannot match.%s\n",
				PL_colors[4], PL_colors[5]));
			goto phooey;
		    } else if (memEQ(checkpos + 1, little, len - 1)) {
                        /* can match, the end of the string matches without the
                         * "\n" */
			last = checkpos + 1;
		    } else if (checkpos < strbeg) {
                        /* cant match, string is too short when the "\n" is
                         * included */
			DEBUG_EXECUTE_r(
                            Perl_re_printf( aTHX_
				"%sString does not contain required trailing substring, cannot match.%s\n",
				PL_colors[4], PL_colors[5]));
			goto phooey;
		    } else if (!multiline) {
                        /* non multiline match, so compare with the "\n" at the
                         * end of the string */
			if (memEQ(checkpos, little, len)) {
			    last= checkpos;
			} else {
			    DEBUG_EXECUTE_r(
                                Perl_re_printf( aTHX_
				    "%sString does not contain required trailing substring, cannot match.%s\n",
				    PL_colors[4], PL_colors[5]));
			    goto phooey;
			}
		    } else {
                        /* multiline match, so we have to search for a place
                         * where the full string is located */
			goto find_last;
		    }
	    } else {
		  find_last:
		    if (len)
			last = rninstr(s, strend, little, little + len);
		    else
			last = strend;	/* matching "$" */
	    }
	    if (!last) {
                /* at one point this block contained a comment which was
                 * probably incorrect, which said that this was a "should not
                 * happen" case.  Even if it was true when it was written I am
                 * pretty sure it is not anymore, so I have removed the comment
                 * and replaced it with this one. Yves */
		DEBUG_EXECUTE_r(
                    Perl_re_printf( aTHX_
			"%sString does not contain required substring, cannot match.%s\n",
                        PL_colors[4], PL_colors[5]
	            ));
		goto phooey;
	    }
	    dontbother = strend - last + prog->float_min_offset;
	}
	if (minlen && (dontbother < minlen))
	    dontbother = minlen - 1;
	strend -= dontbother; 		   /* this one's always in bytes! */
	/* We don't know much -- general case. */
	if (utf8_target) {
	    for (;;) {
		if (regtry(reginfo, &s))
		    goto got_it;
		if (s >= strend)
		    break;
		s += UTF8SKIP(s);
	    };
	}
	else {
	    do {
		if (regtry(reginfo, &s))
		    goto got_it;
	    } while (s++ < strend);
	}
    }

    /* Failure. */
    goto phooey;

  got_it:
    /* s/// doesn't like it if $& is earlier than where we asked it to
     * start searching (which can happen on something like /.\G/) */
    if (       (flags & REXEC_FAIL_ON_UNDERFLOW)
            && (prog->offs[0].start < stringarg - strbeg))
    {
        /* this should only be possible under \G */
        assert(prog->intflags & PREGf_GPOS_SEEN);
        DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
            "matched, but failing for REXEC_FAIL_ON_UNDERFLOW\n"));
        goto phooey;
    }

    DEBUG_BUFFERS_r(
	if (swap)
            Perl_re_exec_indentf( aTHX_
		"rex=0x%" UVxf " freeing offs: 0x%" UVxf "\n",
		0,
                PTR2UV(prog),
		PTR2UV(swap)
	    );
    );
    Safefree(swap);

    /* clean up; this will trigger destructors that will free all slabs
     * above the current one, and cleanup the regmatch_info_aux
     * and regmatch_info_aux_eval sructs */

    LEAVE_SCOPE(oldsave);

    if (RXp_PAREN_NAMES(prog)) 
        (void)hv_iterinit(RXp_PAREN_NAMES(prog));

    /* make sure $`, $&, $', and $digit will work later */
    if ( !(flags & REXEC_NOT_FIRST) )
        S_reg_set_capture_string(aTHX_ rx,
                                    strbeg, reginfo->strend,
                                    sv, flags, utf8_target);

    return 1;

  phooey:
    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "%sMatch failed%s\n",
			  PL_colors[4], PL_colors[5]));

    /* clean up; this will trigger destructors that will free all slabs
     * above the current one, and cleanup the regmatch_info_aux
     * and regmatch_info_aux_eval sructs */

    LEAVE_SCOPE(oldsave);

    if (swap) {
        /* we failed :-( roll it back */
        DEBUG_BUFFERS_r(Perl_re_exec_indentf( aTHX_
	    "rex=0x%" UVxf " rolling back offs: freeing=0x%" UVxf " restoring=0x%" UVxf "\n",
	    0,
            PTR2UV(prog),
	    PTR2UV(prog->offs),
	    PTR2UV(swap)
	));
        Safefree(prog->offs);
        prog->offs = swap;
    }
    return 0;
}


/* Set which rex is pointed to by PL_reg_curpm, handling ref counting.
 * Do inc before dec, in case old and new rex are the same */
#define SET_reg_curpm(Re2)                          \
    if (reginfo->info_aux_eval) {                   \
	(void)ReREFCNT_inc(Re2);		    \
	ReREFCNT_dec(PM_GETRE(PL_reg_curpm));	    \
	PM_SETRE((PL_reg_curpm), (Re2));	    \
    }


/*
 - regtry - try match at specific point
 */
STATIC bool			/* 0 failure, 1 success */
S_regtry(pTHX_ regmatch_info *reginfo, char **startposp)
{
    CHECKPOINT lastcp;
    REGEXP *const rx = reginfo->prog;
    regexp *const prog = ReANY(rx);
    SSize_t result;
#ifdef DEBUGGING
    U32 depth = 0; /* used by REGCP_SET */
#endif
    RXi_GET_DECL(prog,progi);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTRY;

    reginfo->cutpoint=NULL;

    prog->offs[0].start = *startposp - reginfo->strbeg;
    prog->lastparen = 0;
    prog->lastcloseparen = 0;

    /* XXXX What this code is doing here?!!!  There should be no need
       to do this again and again, prog->lastparen should take care of
       this!  --ilya*/

    /* Tests pat.t#187 and split.t#{13,14} seem to depend on this code.
     * Actually, the code in regcppop() (which Ilya may be meaning by
     * prog->lastparen), is not needed at all by the test suite
     * (op/regexp, op/pat, op/split), but that code is needed otherwise
     * this erroneously leaves $1 defined: "1" =~ /^(?:(\d)x)?\d$/
     * Meanwhile, this code *is* needed for the
     * above-mentioned test suite tests to succeed.  The common theme
     * on those tests seems to be returning null fields from matches.
     * --jhi updated by dapm */

    /* After encountering a variant of the issue mentioned above I think
     * the point Ilya was making is that if we properly unwind whenever
     * we set lastparen to a smaller value then we should not need to do
     * this every time, only when needed. So if we have tests that fail if
     * we remove this, then it suggests somewhere else we are improperly
     * unwinding the lastparen/paren buffers. See UNWIND_PARENS() and
     * places it is called, and related regcp() routines. - Yves */
#if 1
    if (prog->nparens) {
	regexp_paren_pair *pp = prog->offs;
	I32 i;
	for (i = prog->nparens; i > (I32)prog->lastparen; i--) {
	    ++pp;
	    pp->start = -1;
	    pp->end = -1;
	}
    }
#endif
    REGCP_SET(lastcp);
    result = regmatch(reginfo, *startposp, progi->program + 1);
    if (result != -1) {
	prog->offs[0].end = result;
	return 1;
    }
    if (reginfo->cutpoint)
        *startposp= reginfo->cutpoint;
    REGCP_UNWIND(lastcp);
    return 0;
}


#define sayYES goto yes
#define sayNO goto no
#define sayNO_SILENT goto no_silent

/* we dont use STMT_START/END here because it leads to 
   "unreachable code" warnings, which are bogus, but distracting. */
#define CACHEsayNO \
    if (ST.cache_mask) \
       reginfo->info_aux->poscache[ST.cache_offset] |= ST.cache_mask; \
    sayNO

/* this is used to determine how far from the left messages like
   'failed...' are printed in regexec.c. It should be set such that
   messages are inline with the regop output that created them.
*/
#define REPORT_CODE_OFF 29
#define INDENT_CHARS(depth) ((int)(depth) % 20)
#ifdef DEBUGGING
int
Perl_re_exec_indentf(pTHX_ const char *fmt, U32 depth, ...)
{
    va_list ap;
    int result;
    PerlIO *f= Perl_debug_log;
    PERL_ARGS_ASSERT_RE_EXEC_INDENTF;
    va_start(ap, depth);
    PerlIO_printf(f, "%*s|%4" UVuf "| %*s", REPORT_CODE_OFF, "", (UV)depth, INDENT_CHARS(depth), "" );
    result = PerlIO_vprintf(f, fmt, ap);
    va_end(ap);
    return result;
}
#endif /* DEBUGGING */


#define CHRTEST_UNINIT -1001 /* c1/c2 haven't been calculated yet */
#define CHRTEST_VOID   -1000 /* the c1/c2 "next char" test should be skipped */
#define CHRTEST_NOT_A_CP_1 -999
#define CHRTEST_NOT_A_CP_2 -998

/* grab a new slab and return the first slot in it */

STATIC regmatch_state *
S_push_slab(pTHX)
{
    regmatch_slab *s = PL_regmatch_slab->next;
    if (!s) {
	Newx(s, 1, regmatch_slab);
	s->prev = PL_regmatch_slab;
	s->next = NULL;
	PL_regmatch_slab->next = s;
    }
    PL_regmatch_slab = s;
    return SLAB_FIRST(s);
}


/* push a new state then goto it */

#define PUSH_STATE_GOTO(state, node, input) \
    pushinput = input; \
    scan = node; \
    st->resume_state = state; \
    goto push_state;

/* push a new state with success backtracking, then goto it */

#define PUSH_YES_STATE_GOTO(state, node, input) \
    pushinput = input; \
    scan = node; \
    st->resume_state = state; \
    goto push_yes_state;




/*

regmatch() - main matching routine

This is basically one big switch statement in a loop. We execute an op,
set 'next' to point the next op, and continue. If we come to a point which
we may need to backtrack to on failure such as (A|B|C), we push a
backtrack state onto the backtrack stack. On failure, we pop the top
state, and re-enter the loop at the state indicated. If there are no more
states to pop, we return failure.

Sometimes we also need to backtrack on success; for example /A+/, where
after successfully matching one A, we need to go back and try to
match another one; similarly for lookahead assertions: if the assertion
completes successfully, we backtrack to the state just before the assertion
and then carry on.  In these cases, the pushed state is marked as
'backtrack on success too'. This marking is in fact done by a chain of
pointers, each pointing to the previous 'yes' state. On success, we pop to
the nearest yes state, discarding any intermediate failure-only states.
Sometimes a yes state is pushed just to force some cleanup code to be
called at the end of a successful match or submatch; e.g. (??{$re}) uses
it to free the inner regex.

Note that failure backtracking rewinds the cursor position, while
success backtracking leaves it alone.

A pattern is complete when the END op is executed, while a subpattern
such as (?=foo) is complete when the SUCCESS op is executed. Both of these
ops trigger the "pop to last yes state if any, otherwise return true"
behaviour.

A common convention in this function is to use A and B to refer to the two
subpatterns (or to the first nodes thereof) in patterns like /A*B/: so A is
the subpattern to be matched possibly multiple times, while B is the entire
rest of the pattern. Variable and state names reflect this convention.

The states in the main switch are the union of ops and failure/success of
substates associated with with that op.  For example, IFMATCH is the op
that does lookahead assertions /(?=A)B/ and so the IFMATCH state means
'execute IFMATCH'; while IFMATCH_A is a state saying that we have just
successfully matched A and IFMATCH_A_fail is a state saying that we have
just failed to match A. Resume states always come in pairs. The backtrack
state we push is marked as 'IFMATCH_A', but when that is popped, we resume
at IFMATCH_A or IFMATCH_A_fail, depending on whether we are backtracking
on success or failure.

The struct that holds a backtracking state is actually a big union, with
one variant for each major type of op. The variable st points to the
top-most backtrack struct. To make the code clearer, within each
block of code we #define ST to alias the relevant union.

Here's a concrete example of a (vastly oversimplified) IFMATCH
implementation:

    switch (state) {
    ....

#define ST st->u.ifmatch

    case IFMATCH: // we are executing the IFMATCH op, (?=A)B
	ST.foo = ...; // some state we wish to save
	...
	// push a yes backtrack state with a resume value of
	// IFMATCH_A/IFMATCH_A_fail, then continue execution at the
	// first node of A:
	PUSH_YES_STATE_GOTO(IFMATCH_A, A, newinput);
	// NOTREACHED

    case IFMATCH_A: // we have successfully executed A; now continue with B
	next = B;
	bar = ST.foo; // do something with the preserved value
	break;

    case IFMATCH_A_fail: // A failed, so the assertion failed
	...;   // do some housekeeping, then ...
	sayNO; // propagate the failure

#undef ST

    ...
    }

For any old-timers reading this who are familiar with the old recursive
approach, the code above is equivalent to:

    case IFMATCH: // we are executing the IFMATCH op, (?=A)B
    {
	int foo = ...
	...
	if (regmatch(A)) {
	    next = B;
	    bar = foo;
	    break;
	}
	...;   // do some housekeeping, then ...
	sayNO; // propagate the failure
    }

The topmost backtrack state, pointed to by st, is usually free. If you
want to claim it, populate any ST.foo fields in it with values you wish to
save, then do one of

	PUSH_STATE_GOTO(resume_state, node, newinput);
	PUSH_YES_STATE_GOTO(resume_state, node, newinput);

which sets that backtrack state's resume value to 'resume_state', pushes a
new free entry to the top of the backtrack stack, then goes to 'node'.
On backtracking, the free slot is popped, and the saved state becomes the
new free state. An ST.foo field in this new top state can be temporarily
accessed to retrieve values, but once the main loop is re-entered, it
becomes available for reuse.

Note that the depth of the backtrack stack constantly increases during the
left-to-right execution of the pattern, rather than going up and down with
the pattern nesting. For example the stack is at its maximum at Z at the
end of the pattern, rather than at X in the following:

    /(((X)+)+)+....(Y)+....Z/

The only exceptions to this are lookahead/behind assertions and the cut,
(?>A), which pop all the backtrack states associated with A before
continuing.
 
Backtrack state structs are allocated in slabs of about 4K in size.
PL_regmatch_state and st always point to the currently active state,
and PL_regmatch_slab points to the slab currently containing
PL_regmatch_state.  The first time regmatch() is called, the first slab is
allocated, and is never freed until interpreter destruction. When the slab
is full, a new one is allocated and chained to the end. At exit from
regmatch(), slabs allocated since entry are freed.

*/
 

#define DEBUG_STATE_pp(pp)                                  \
    DEBUG_STATE_r({                                         \
        DUMP_EXEC_POS(locinput, scan, utf8_target,depth);   \
        Perl_re_printf( aTHX_                                           \
            "%*s" pp " %s%s%s%s%s\n",                       \
            INDENT_CHARS(depth), "",                        \
            PL_reg_name[st->resume_state],                  \
            ((st==yes_state||st==mark_state) ? "[" : ""),   \
            ((st==yes_state) ? "Y" : ""),                   \
            ((st==mark_state) ? "M" : ""),                  \
            ((st==yes_state||st==mark_state) ? "]" : "")    \
        );                                                  \
    });


#define REG_NODE_NUM(x) ((x) ? (int)((x)-prog) : -1)

#ifdef DEBUGGING

STATIC void
S_debug_start_match(pTHX_ const REGEXP *prog, const bool utf8_target,
    const char *start, const char *end, const char *blurb)
{
    const bool utf8_pat = RX_UTF8(prog) ? 1 : 0;

    PERL_ARGS_ASSERT_DEBUG_START_MATCH;

    if (!PL_colorset)   
            reginitcolors();    
    {
        RE_PV_QUOTED_DECL(s0, utf8_pat, PERL_DEBUG_PAD_ZERO(0), 
            RX_PRECOMP_const(prog), RX_PRELEN(prog), PL_dump_re_max_len);
        
        RE_PV_QUOTED_DECL(s1, utf8_target, PERL_DEBUG_PAD_ZERO(1),
            start, end - start, PL_dump_re_max_len);
        
        Perl_re_printf( aTHX_
            "%s%s REx%s %s against %s\n", 
		       PL_colors[4], blurb, PL_colors[5], s0, s1); 
        
        if (utf8_target||utf8_pat)
            Perl_re_printf( aTHX_  "UTF-8 %s%s%s...\n",
                utf8_pat ? "pattern" : "",
                utf8_pat && utf8_target ? " and " : "",
                utf8_target ? "string" : ""
            ); 
    }
}

STATIC void
S_dump_exec_pos(pTHX_ const char *locinput, 
                      const regnode *scan, 
                      const char *loc_regeol, 
                      const char *loc_bostr, 
                      const char *loc_reg_starttry,
                      const bool utf8_target,
                      const U32 depth
                )
{
    const int docolor = *PL_colors[0] || *PL_colors[2] || *PL_colors[4];
    const int taill = (docolor ? 10 : 7); /* 3 chars for "> <" */
    int l = (loc_regeol - locinput) > taill ? taill : (loc_regeol - locinput);
    /* The part of the string before starttry has one color
       (pref0_len chars), between starttry and current
       position another one (pref_len - pref0_len chars),
       after the current position the third one.
       We assume that pref0_len <= pref_len, otherwise we
       decrease pref0_len.  */
    int pref_len = (locinput - loc_bostr) > (5 + taill) - l
	? (5 + taill) - l : locinput - loc_bostr;
    int pref0_len;

    PERL_ARGS_ASSERT_DUMP_EXEC_POS;

    while (utf8_target && UTF8_IS_CONTINUATION(*(U8*)(locinput - pref_len)))
	pref_len++;
    pref0_len = pref_len  - (locinput - loc_reg_starttry);
    if (l + pref_len < (5 + taill) && l < loc_regeol - locinput)
	l = ( loc_regeol - locinput > (5 + taill) - pref_len
	      ? (5 + taill) - pref_len : loc_regeol - locinput);
    while (utf8_target && UTF8_IS_CONTINUATION(*(U8*)(locinput + l)))
	l--;
    if (pref0_len < 0)
	pref0_len = 0;
    if (pref0_len > pref_len)
	pref0_len = pref_len;
    {
	const int is_uni = utf8_target ? 1 : 0;

	RE_PV_COLOR_DECL(s0,len0,is_uni,PERL_DEBUG_PAD(0),
	    (locinput - pref_len),pref0_len, PL_dump_re_max_len, 4, 5);
	
	RE_PV_COLOR_DECL(s1,len1,is_uni,PERL_DEBUG_PAD(1),
		    (locinput - pref_len + pref0_len),
		    pref_len - pref0_len, PL_dump_re_max_len, 2, 3);
	
	RE_PV_COLOR_DECL(s2,len2,is_uni,PERL_DEBUG_PAD(2),
		    locinput, loc_regeol - locinput, 10, 0, 1);

	const STRLEN tlen=len0+len1+len2;
        Perl_re_printf( aTHX_
                    "%4" IVdf " <%.*s%.*s%s%.*s>%*s|%4u| ",
		    (IV)(locinput - loc_bostr),
		    len0, s0,
		    len1, s1,
		    (docolor ? "" : "> <"),
		    len2, s2,
		    (int)(tlen > 19 ? 0 :  19 - tlen),
                    "",
                    depth);
    }
}

#endif

/* reg_check_named_buff_matched()
 * Checks to see if a named buffer has matched. The data array of 
 * buffer numbers corresponding to the buffer is expected to reside
 * in the regexp->data->data array in the slot stored in the ARG() of
 * node involved. Note that this routine doesn't actually care about the
 * name, that information is not preserved from compilation to execution.
 * Returns the index of the leftmost defined buffer with the given name
 * or 0 if non of the buffers matched.
 */
STATIC I32
S_reg_check_named_buff_matched(const regexp *rex, const regnode *scan)
{
    I32 n;
    RXi_GET_DECL(rex,rexi);
    SV *sv_dat= MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
    I32 *nums=(I32*)SvPVX(sv_dat);

    PERL_ARGS_ASSERT_REG_CHECK_NAMED_BUFF_MATCHED;

    for ( n=0; n<SvIVX(sv_dat); n++ ) {
        if ((I32)rex->lastparen >= nums[n] &&
            rex->offs[nums[n]].end != -1)
        {
            return nums[n];
        }
    }
    return 0;
}


static bool
S_setup_EXACTISH_ST_c1_c2(pTHX_ const regnode * const text_node, int *c1p,
        U8* c1_utf8, int *c2p, U8* c2_utf8, regmatch_info *reginfo)
{
    /* This function determines if there are one or two characters that match
     * the first character of the passed-in EXACTish node <text_node>, and if
     * so, returns them in the passed-in pointers.
     *
     * If it determines that no possible character in the target string can
     * match, it returns FALSE; otherwise TRUE.  (The FALSE situation occurs if
     * the first character in <text_node> requires UTF-8 to represent, and the
     * target string isn't in UTF-8.)
     *
     * If there are more than two characters that could match the beginning of
     * <text_node>, or if more context is required to determine a match or not,
     * it sets both *<c1p> and *<c2p> to CHRTEST_VOID.
     *
     * The motiviation behind this function is to allow the caller to set up
     * tight loops for matching.  If <text_node> is of type EXACT, there is
     * only one possible character that can match its first character, and so
     * the situation is quite simple.  But things get much more complicated if
     * folding is involved.  It may be that the first character of an EXACTFish
     * node doesn't participate in any possible fold, e.g., punctuation, so it
     * can be matched only by itself.  The vast majority of characters that are
     * in folds match just two things, their lower and upper-case equivalents.
     * But not all are like that; some have multiple possible matches, or match
     * sequences of more than one character.  This function sorts all that out.
     *
     * Consider the patterns A*B or A*?B where A and B are arbitrary.  In a
     * loop of trying to match A*, we know we can't exit where the thing
     * following it isn't a B.  And something can't be a B unless it is the
     * beginning of B.  By putting a quick test for that beginning in a tight
     * loop, we can rule out things that can't possibly be B without having to
     * break out of the loop, thus avoiding work.  Similarly, if A is a single
     * character, we can make a tight loop matching A*, using the outputs of
     * this function.
     *
     * If the target string to match isn't in UTF-8, and there aren't
     * complications which require CHRTEST_VOID, *<c1p> and *<c2p> are set to
     * the one or two possible octets (which are characters in this situation)
     * that can match.  In all cases, if there is only one character that can
     * match, *<c1p> and *<c2p> will be identical.
     *
     * If the target string is in UTF-8, the buffers pointed to by <c1_utf8>
     * and <c2_utf8> will contain the one or two UTF-8 sequences of bytes that
     * can match the beginning of <text_node>.  They should be declared with at
     * least length UTF8_MAXBYTES+1.  (If the target string isn't in UTF-8, it is
     * undefined what these contain.)  If one or both of the buffers are
     * invariant under UTF-8, *<c1p>, and *<c2p> will also be set to the
     * corresponding invariant.  If variant, the corresponding *<c1p> and/or
     * *<c2p> will be set to a negative number(s) that shouldn't match any code
     * point (unless inappropriately coerced to unsigned).   *<c1p> will equal
     * *<c2p> if and only if <c1_utf8> and <c2_utf8> are the same. */

    const bool utf8_target = reginfo->is_utf8_target;

    UV c1 = (UV)CHRTEST_NOT_A_CP_1;
    UV c2 = (UV)CHRTEST_NOT_A_CP_2;
    bool use_chrtest_void = FALSE;
    const bool is_utf8_pat = reginfo->is_utf8_pat;

    /* Used when we have both utf8 input and utf8 output, to avoid converting
     * to/from code points */
    bool utf8_has_been_setup = FALSE;

    dVAR;

    U8 *pat = (U8*)STRING(text_node);
    U8 folded[UTF8_MAX_FOLD_CHAR_EXPAND * UTF8_MAXBYTES_CASE + 1] = { '\0' };

    if (OP(text_node) == EXACT || OP(text_node) == EXACTL) {

        /* In an exact node, only one thing can be matched, that first
         * character.  If both the pat and the target are UTF-8, we can just
         * copy the input to the output, avoiding finding the code point of
         * that character */
        if (!is_utf8_pat) {
            c2 = c1 = *pat;
        }
        else if (utf8_target) {
            Copy(pat, c1_utf8, UTF8SKIP(pat), U8);
            Copy(pat, c2_utf8, UTF8SKIP(pat), U8);
            utf8_has_been_setup = TRUE;
        }
        else {
            c2 = c1 = valid_utf8_to_uvchr(pat, NULL);
        }
    }
    else { /* an EXACTFish node */
        U8 *pat_end = pat + STR_LEN(text_node);

        /* An EXACTFL node has at least some characters unfolded, because what
         * they match is not known until now.  So, now is the time to fold
         * the first few of them, as many as are needed to determine 'c1' and
         * 'c2' later in the routine.  If the pattern isn't UTF-8, we only need
         * to fold if in a UTF-8 locale, and then only the Sharp S; everything
         * else is 1-1 and isn't assumed to be folded.  In a UTF-8 pattern, we
         * need to fold as many characters as a single character can fold to,
         * so that later we can check if the first ones are such a multi-char
         * fold.  But, in such a pattern only locale-problematic characters
         * aren't folded, so we can skip this completely if the first character
         * in the node isn't one of the tricky ones */
        if (OP(text_node) == EXACTFL) {

            if (! is_utf8_pat) {
                if (IN_UTF8_CTYPE_LOCALE && *pat == LATIN_SMALL_LETTER_SHARP_S)
                {
                    folded[0] = folded[1] = 's';
                    pat = folded;
                    pat_end = folded + 2;
                }
            }
            else if (is_PROBLEMATIC_LOCALE_FOLDEDS_START_utf8(pat)) {
                U8 *s = pat;
                U8 *d = folded;
                int i;

                for (i = 0; i < UTF8_MAX_FOLD_CHAR_EXPAND && s < pat_end; i++) {
                    if (isASCII(*s)) {
                        *(d++) = (U8) toFOLD_LC(*s);
                        s++;
                    }
                    else {
                        STRLEN len;
                        _toFOLD_utf8_flags(s,
                                           pat_end,
                                           d,
                                           &len,
                                           FOLD_FLAGS_FULL | FOLD_FLAGS_LOCALE);
                        d += len;
                        s += UTF8SKIP(s);
                    }
                }

                pat = folded;
                pat_end = d;
            }
        }

        if ((is_utf8_pat && is_MULTI_CHAR_FOLD_utf8_safe(pat, pat_end))
             || (!is_utf8_pat && is_MULTI_CHAR_FOLD_latin1_safe(pat, pat_end)))
        {
            /* Multi-character folds require more context to sort out.  Also
             * PL_utf8_foldclosures used below doesn't handle them, so have to
             * be handled outside this routine */
            use_chrtest_void = TRUE;
        }
        else { /* an EXACTFish node which doesn't begin with a multi-char fold */
            c1 = is_utf8_pat ? valid_utf8_to_uvchr(pat, NULL) : *pat;
            if (c1 > 255) {
                const unsigned int * remaining_folds_to_list;
                unsigned int first_folds_to;

                /* Look up what code points (besides c1) fold to c1;  e.g.,
                 * [ 'K', KELVIN_SIGN ] both fold to 'k'. */
                Size_t folds_to_count = _inverse_folds(c1,
                                                     &first_folds_to,
                                                     &remaining_folds_to_list);
                if (folds_to_count == 0) {
                    c2 = c1;    /* there is only a single character that could
                                   match */
                }
                else if (folds_to_count != 1) {
                    /* If there aren't exactly two folds to this (itself and
                     * another), it is outside the scope of this function */
                    use_chrtest_void = TRUE;
                }
                else {  /* There are two.  We already have one, get the other */
                    c2 = first_folds_to;

                    /* Folds that cross the 255/256 boundary are forbidden if
                     * EXACTFL (and isnt a UTF8 locale), or EXACTFAA and one is
                     * ASCIII.  The only other match to c1 is c2, and since c1
                     * is above 255, c2 better be as well under these
                     * circumstances.  If it isn't, it means the only legal
                     * match of c1 is itself. */
                    if (    c2 < 256
                        && (   (   OP(text_node) == EXACTFL
                                && ! IN_UTF8_CTYPE_LOCALE)
                            || ((     OP(text_node) == EXACTFAA
                                   || OP(text_node) == EXACTFAA_NO_TRIE)
                                && (isASCII(c1) || isASCII(c2)))))
                    {
                        c2 = c1;
                    }
                }
            }
            else /* Here, c1 is <= 255 */
                if (utf8_target
                    && HAS_NONLATIN1_FOLD_CLOSURE(c1)
                    && ( ! (OP(text_node) == EXACTFL && ! IN_UTF8_CTYPE_LOCALE))
                    && ((OP(text_node) != EXACTFAA
                        && OP(text_node) != EXACTFAA_NO_TRIE)
                        || ! isASCII(c1)))
            {
                /* Here, there could be something above Latin1 in the target
                 * which folds to this character in the pattern.  All such
                 * cases except LATIN SMALL LETTER Y WITH DIAERESIS have more
                 * than two characters involved in their folds, so are outside
                 * the scope of this function */
                if (UNLIKELY(c1 == LATIN_SMALL_LETTER_Y_WITH_DIAERESIS)) {
                    c2 = LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS;
                }
                else {
                    use_chrtest_void = TRUE;
                }
            }
            else { /* Here nothing above Latin1 can fold to the pattern
                      character */
                switch (OP(text_node)) {

                    case EXACTFL:   /* /l rules */
                        c2 = PL_fold_locale[c1];
                        break;

                    case EXACTF:   /* This node only generated for non-utf8
                                    patterns */
                        assert(! is_utf8_pat);
                        if (! utf8_target) {    /* /d rules */
                            c2 = PL_fold[c1];
                            break;
                        }
                        /* FALLTHROUGH */
                        /* /u rules for all these.  This happens to work for
                        * EXACTFAA as nothing in Latin1 folds to ASCII */
                    case EXACTFAA_NO_TRIE:   /* This node only generated for
                                                non-utf8 patterns */
                        assert(! is_utf8_pat);
                        /* FALLTHROUGH */
                    case EXACTFAA:
                    case EXACTFU_SS:
                    case EXACTFU:
                        c2 = PL_fold_latin1[c1];
                        break;

                    default:
                        Perl_croak(aTHX_ "panic: Unexpected op %u", OP(text_node));
                        NOT_REACHED; /* NOTREACHED */
                }
            }
        }
    }

    /* Here have figured things out.  Set up the returns */
    if (use_chrtest_void) {
        *c2p = *c1p = CHRTEST_VOID;
    }
    else if (utf8_target) {
        if (! utf8_has_been_setup) {    /* Don't have the utf8; must get it */
            uvchr_to_utf8(c1_utf8, c1);
            uvchr_to_utf8(c2_utf8, c2);
        }

        /* Invariants are stored in both the utf8 and byte outputs; Use
         * negative numbers otherwise for the byte ones.  Make sure that the
         * byte ones are the same iff the utf8 ones are the same */
        *c1p = (UTF8_IS_INVARIANT(*c1_utf8)) ? *c1_utf8 : CHRTEST_NOT_A_CP_1;
        *c2p = (UTF8_IS_INVARIANT(*c2_utf8))
                ? *c2_utf8
                : (c1 == c2)
                  ? CHRTEST_NOT_A_CP_1
                  : CHRTEST_NOT_A_CP_2;
    }
    else if (c1 > 255) {
       if (c2 > 255) {  /* both possibilities are above what a non-utf8 string
                           can represent */
           return FALSE;
       }

       *c1p = *c2p = c2;    /* c2 is the only representable value */
    }
    else {  /* c1 is representable; see about c2 */
       *c1p = c1;
       *c2p = (c2 < 256) ? c2 : c1;
    }

    return TRUE;
}

STATIC bool
S_isGCB(pTHX_ const GCB_enum before, const GCB_enum after, const U8 * const strbeg, const U8 * const curpos, const bool utf8_target)
{
    /* returns a boolean indicating if there is a Grapheme Cluster Boundary
     * between the inputs.  See http://www.unicode.org/reports/tr29/. */

    PERL_ARGS_ASSERT_ISGCB;

    switch (GCB_table[before][after]) {
        case GCB_BREAKABLE:
            return TRUE;

        case GCB_NOBREAK:
            return FALSE;

        case GCB_RI_then_RI:
            {
                int RI_count = 1;
                U8 * temp_pos = (U8 *) curpos;

                /* Do not break within emoji flag sequences. That is, do not
                 * break between regional indicator (RI) symbols if there is an
                 * odd number of RI characters before the break point.
                 *  GB12   sot (RI RI)* RI × RI
                 *  GB13 [^RI] (RI RI)* RI × RI */

                while (backup_one_GCB(strbeg,
                                    &temp_pos,
                                    utf8_target) == GCB_Regional_Indicator)
                {
                    RI_count++;
                }

                return RI_count % 2 != 1;
            }

        case GCB_EX_then_EM:

            /* GB10  ( E_Base | E_Base_GAZ ) Extend* ×  E_Modifier */
            {
                U8 * temp_pos = (U8 *) curpos;
                GCB_enum prev;

                do {
                    prev = backup_one_GCB(strbeg, &temp_pos, utf8_target);
                }
                while (prev == GCB_Extend);

                return prev != GCB_E_Base && prev != GCB_E_Base_GAZ;
            }

        default:
            break;
    }

#ifdef DEBUGGING
    Perl_re_printf( aTHX_  "Unhandled GCB pair: GCB_table[%d, %d] = %d\n",
                                  before, after, GCB_table[before][after]);
    assert(0);
#endif
    return TRUE;
}

STATIC GCB_enum
S_backup_one_GCB(pTHX_ const U8 * const strbeg, U8 ** curpos, const bool utf8_target)
{
    GCB_enum gcb;

    PERL_ARGS_ASSERT_BACKUP_ONE_GCB;

    if (*curpos < strbeg) {
        return GCB_EDGE;
    }

    if (utf8_target) {
        U8 * prev_char_pos = reghopmaybe3(*curpos, -1, strbeg);
        U8 * prev_prev_char_pos;

        if (! prev_char_pos) {
            return GCB_EDGE;
        }

        if ((prev_prev_char_pos = reghopmaybe3((U8 *) prev_char_pos, -1, strbeg))) {
            gcb = getGCB_VAL_UTF8(prev_prev_char_pos, prev_char_pos);
            *curpos = prev_char_pos;
            prev_char_pos = prev_prev_char_pos;
        }
        else {
            *curpos = (U8 *) strbeg;
            return GCB_EDGE;
        }
    }
    else {
        if (*curpos - 2 < strbeg) {
            *curpos = (U8 *) strbeg;
            return GCB_EDGE;
        }
        (*curpos)--;
        gcb = getGCB_VAL_CP(*(*curpos - 1));
    }

    return gcb;
}

/* Combining marks attach to most classes that precede them, but this defines
 * the exceptions (from TR14) */
#define LB_CM_ATTACHES_TO(prev) ( ! (   prev == LB_EDGE                 \
                                     || prev == LB_Mandatory_Break      \
                                     || prev == LB_Carriage_Return      \
                                     || prev == LB_Line_Feed            \
                                     || prev == LB_Next_Line            \
                                     || prev == LB_Space                \
                                     || prev == LB_ZWSpace))

STATIC bool
S_isLB(pTHX_ LB_enum before,
             LB_enum after,
             const U8 * const strbeg,
             const U8 * const curpos,
             const U8 * const strend,
             const bool utf8_target)
{
    U8 * temp_pos = (U8 *) curpos;
    LB_enum prev = before;

    /* Is the boundary between 'before' and 'after' line-breakable?
     * Most of this is just a table lookup of a generated table from Unicode
     * rules.  But some rules require context to decide, and so have to be
     * implemented in code */

    PERL_ARGS_ASSERT_ISLB;

    /* Rule numbers in the comments below are as of Unicode 9.0 */

  redo:
    before = prev;
    switch (LB_table[before][after]) {
        case LB_BREAKABLE:
            return TRUE;

        case LB_NOBREAK:
        case LB_NOBREAK_EVEN_WITH_SP_BETWEEN:
            return FALSE;

        case LB_SP_foo + LB_BREAKABLE:
        case LB_SP_foo + LB_NOBREAK:
        case LB_SP_foo + LB_NOBREAK_EVEN_WITH_SP_BETWEEN:

            /* When we have something following a SP, we have to look at the
             * context in order to know what to do.
             *
             * SP SP should not reach here because LB7: Do not break before
             * spaces.  (For two spaces in a row there is nothing that
             * overrides that) */
            assert(after != LB_Space);

            /* Here we have a space followed by a non-space.  Mostly this is a
             * case of LB18: "Break after spaces".  But there are complications
             * as the handling of spaces is somewhat tricky.  They are in a
             * number of rules, which have to be applied in priority order, but
             * something earlier in the string can cause a rule to be skipped
             * and a lower priority rule invoked.  A prime example is LB7 which
             * says don't break before a space.  But rule LB8 (lower priority)
             * says that the first break opportunity after a ZW is after any
             * span of spaces immediately after it.  If a ZW comes before a SP
             * in the input, rule LB8 applies, and not LB7.  Other such rules
             * involve combining marks which are rules 9 and 10, but they may
             * override higher priority rules if they come earlier in the
             * string.  Since we're doing random access into the middle of the
             * string, we have to look for rules that should get applied based
             * on both string position and priority.  Combining marks do not
             * attach to either ZW nor SP, so we don't have to consider them
             * until later.
             *
             * To check for LB8, we have to find the first non-space character
             * before this span of spaces */
            do {
                prev = backup_one_LB(strbeg, &temp_pos, utf8_target);
            }
            while (prev == LB_Space);

            /* LB8 Break before any character following a zero-width space,
             * even if one or more spaces intervene.
             *      ZW SP* ÷
             * So if we have a ZW just before this span, and to get here this
             * is the final space in the span. */
            if (prev == LB_ZWSpace) {
                return TRUE;
            }

            /* Here, not ZW SP+.  There are several rules that have higher
             * priority than LB18 and can be resolved now, as they don't depend
             * on anything earlier in the string (except ZW, which we have
             * already handled).  One of these rules is LB11 Do not break
             * before Word joiner, but we have specially encoded that in the
             * lookup table so it is caught by the single test below which
             * catches the other ones. */
            if (LB_table[LB_Space][after] - LB_SP_foo
                                            == LB_NOBREAK_EVEN_WITH_SP_BETWEEN)
            {
                return FALSE;
            }

            /* If we get here, we have to XXX consider combining marks. */
            if (prev == LB_Combining_Mark) {

                /* What happens with these depends on the character they
                 * follow.  */
                do {
                    prev = backup_one_LB(strbeg, &temp_pos, utf8_target);
                }
                while (prev == LB_Combining_Mark);

                /* Most times these attach to and inherit the characteristics
                 * of that character, but not always, and when not, they are to
                 * be treated as AL by rule LB10. */
                if (! LB_CM_ATTACHES_TO(prev)) {
                    prev = LB_Alphabetic;
                }
            }

            /* Here, we have the character preceding the span of spaces all set
             * up.  We follow LB18: "Break after spaces" unless the table shows
             * that is overriden */
            return LB_table[prev][after] != LB_NOBREAK_EVEN_WITH_SP_BETWEEN;

        case LB_CM_ZWJ_foo:

            /* We don't know how to treat the CM except by looking at the first
             * non-CM character preceding it.  ZWJ is treated as CM */
            do {
                prev = backup_one_LB(strbeg, &temp_pos, utf8_target);
            }
            while (prev == LB_Combining_Mark || prev == LB_ZWJ);

            /* Here, 'prev' is that first earlier non-CM character.  If the CM
             * attatches to it, then it inherits the behavior of 'prev'.  If it
             * doesn't attach, it is to be treated as an AL */
            if (! LB_CM_ATTACHES_TO(prev)) {
                prev = LB_Alphabetic;
            }

            goto redo;

        case LB_HY_or_BA_then_foo + LB_BREAKABLE:
        case LB_HY_or_BA_then_foo + LB_NOBREAK:

            /* LB21a Don't break after Hebrew + Hyphen.
             * HL (HY | BA) × */

            if (backup_one_LB(strbeg, &temp_pos, utf8_target)
                                                          == LB_Hebrew_Letter)
            {
                return FALSE;
            }

            return LB_table[prev][after] - LB_HY_or_BA_then_foo == LB_BREAKABLE;

        case LB_PR_or_PO_then_OP_or_HY + LB_BREAKABLE:
        case LB_PR_or_PO_then_OP_or_HY + LB_NOBREAK:

            /* LB25a (PR | PO) × ( OP | HY )? NU */
            if (advance_one_LB(&temp_pos, strend, utf8_target) == LB_Numeric) {
                return FALSE;
            }

            return LB_table[prev][after] - LB_PR_or_PO_then_OP_or_HY
                                                                == LB_BREAKABLE;

        case LB_SY_or_IS_then_various + LB_BREAKABLE:
        case LB_SY_or_IS_then_various + LB_NOBREAK:
        {
            /* LB25d NU (SY | IS)* × (NU | SY | IS | CL | CP ) */

            LB_enum temp = prev;
            do {
                temp = backup_one_LB(strbeg, &temp_pos, utf8_target);
            }
            while (temp == LB_Break_Symbols || temp == LB_Infix_Numeric);
            if (temp == LB_Numeric) {
                return FALSE;
            }

            return LB_table[prev][after] - LB_SY_or_IS_then_various
                                                               == LB_BREAKABLE;
        }

        case LB_various_then_PO_or_PR + LB_BREAKABLE:
        case LB_various_then_PO_or_PR + LB_NOBREAK:
        {
            /* LB25e NU (SY | IS)* (CL | CP)? × (PO | PR) */

            LB_enum temp = prev;
            if (temp == LB_Close_Punctuation || temp == LB_Close_Parenthesis)
            {
                temp = backup_one_LB(strbeg, &temp_pos, utf8_target);
            }
            while (temp == LB_Break_Symbols || temp == LB_Infix_Numeric) {
                temp = backup_one_LB(strbeg, &temp_pos, utf8_target);
            }
            if (temp == LB_Numeric) {
                return FALSE;
            }
            return LB_various_then_PO_or_PR;
        }

        case LB_RI_then_RI + LB_NOBREAK:
        case LB_RI_then_RI + LB_BREAKABLE:
            {
                int RI_count = 1;

                /* LB30a Break between two regional indicator symbols if and
                 * only if there are an even number of regional indicators
                 * preceding the position of the break.
                 *
                 *    sot (RI RI)* RI × RI
                 *  [^RI] (RI RI)* RI × RI */

                while (backup_one_LB(strbeg,
                                     &temp_pos,
                                     utf8_target) == LB_Regional_Indicator)
                {
                    RI_count++;
                }

                return RI_count % 2 == 0;
            }

        default:
            break;
    }

#ifdef DEBUGGING
    Perl_re_printf( aTHX_  "Unhandled LB pair: LB_table[%d, %d] = %d\n",
                                  before, after, LB_table[before][after]);
    assert(0);
#endif
    return TRUE;
}

STATIC LB_enum
S_advance_one_LB(pTHX_ U8 ** curpos, const U8 * const strend, const bool utf8_target)
{
    LB_enum lb;

    PERL_ARGS_ASSERT_ADVANCE_ONE_LB;

    if (*curpos >= strend) {
        return LB_EDGE;
    }

    if (utf8_target) {
        *curpos += UTF8SKIP(*curpos);
        if (*curpos >= strend) {
            return LB_EDGE;
        }
        lb = getLB_VAL_UTF8(*curpos, strend);
    }
    else {
        (*curpos)++;
        if (*curpos >= strend) {
            return LB_EDGE;
        }
        lb = getLB_VAL_CP(**curpos);
    }

    return lb;
}

STATIC LB_enum
S_backup_one_LB(pTHX_ const U8 * const strbeg, U8 ** curpos, const bool utf8_target)
{
    LB_enum lb;

    PERL_ARGS_ASSERT_BACKUP_ONE_LB;

    if (*curpos < strbeg) {
        return LB_EDGE;
    }

    if (utf8_target) {
        U8 * prev_char_pos = reghopmaybe3(*curpos, -1, strbeg);
        U8 * prev_prev_char_pos;

        if (! prev_char_pos) {
            return LB_EDGE;
        }

        if ((prev_prev_char_pos = reghopmaybe3((U8 *) prev_char_pos, -1, strbeg))) {
            lb = getLB_VAL_UTF8(prev_prev_char_pos, prev_char_pos);
            *curpos = prev_char_pos;
            prev_char_pos = prev_prev_char_pos;
        }
        else {
            *curpos = (U8 *) strbeg;
            return LB_EDGE;
        }
    }
    else {
        if (*curpos - 2 < strbeg) {
            *curpos = (U8 *) strbeg;
            return LB_EDGE;
        }
        (*curpos)--;
        lb = getLB_VAL_CP(*(*curpos - 1));
    }

    return lb;
}

STATIC bool
S_isSB(pTHX_ SB_enum before,
             SB_enum after,
             const U8 * const strbeg,
             const U8 * const curpos,
             const U8 * const strend,
             const bool utf8_target)
{
    /* returns a boolean indicating if there is a Sentence Boundary Break
     * between the inputs.  See http://www.unicode.org/reports/tr29/ */

    U8 * lpos = (U8 *) curpos;
    bool has_para_sep = FALSE;
    bool has_sp = FALSE;

    PERL_ARGS_ASSERT_ISSB;

    /* Break at the start and end of text.
        SB1.  sot  ÷
        SB2.  ÷  eot
      But unstated in Unicode is don't break if the text is empty */
    if (before == SB_EDGE || after == SB_EDGE) {
        return before != after;
    }

    /* SB 3: Do not break within CRLF. */
    if (before == SB_CR && after == SB_LF) {
        return FALSE;
    }

    /* Break after paragraph separators.  CR and LF are considered
     * so because Unicode views text as like word processing text where there
     * are no newlines except between paragraphs, and the word processor takes
     * care of wrapping without there being hard line-breaks in the text *./
       SB4.  Sep | CR | LF  ÷ */
    if (before == SB_Sep || before == SB_CR || before == SB_LF) {
        return TRUE;
    }

    /* Ignore Format and Extend characters, except after sot, Sep, CR, or LF.
     * (See Section 6.2, Replacing Ignore Rules.)
        SB5.  X (Extend | Format)*  →  X */
    if (after == SB_Extend || after == SB_Format) {

        /* Implied is that the these characters attach to everything
         * immediately prior to them except for those separator-type
         * characters.  And the rules earlier have already handled the case
         * when one of those immediately precedes the extend char */
        return FALSE;
    }

    if (before == SB_Extend || before == SB_Format) {
        U8 * temp_pos = lpos;
        const SB_enum backup = backup_one_SB(strbeg, &temp_pos, utf8_target);
        if (   backup != SB_EDGE
            && backup != SB_Sep
            && backup != SB_CR
            && backup != SB_LF)
        {
            before = backup;
            lpos = temp_pos;
        }

        /* Here, both 'before' and 'backup' are these types; implied is that we
         * don't break between them */
        if (backup == SB_Extend || backup == SB_Format) {
            return FALSE;
        }
    }

    /* Do not break after ambiguous terminators like period, if they are
     * immediately followed by a number or lowercase letter, if they are
     * between uppercase letters, if the first following letter (optionally
     * after certain punctuation) is lowercase, or if they are followed by
     * "continuation" punctuation such as comma, colon, or semicolon. For
     * example, a period may be an abbreviation or numeric period, and thus may
     * not mark the end of a sentence.

     * SB6. ATerm  ×  Numeric */
    if (before == SB_ATerm && after == SB_Numeric) {
        return FALSE;
    }

    /* SB7.  (Upper | Lower) ATerm  ×  Upper */
    if (before == SB_ATerm && after == SB_Upper) {
        U8 * temp_pos = lpos;
        SB_enum backup = backup_one_SB(strbeg, &temp_pos, utf8_target);
        if (backup == SB_Upper || backup == SB_Lower) {
            return FALSE;
        }
    }

    /* The remaining rules that aren't the final one, all require an STerm or
     * an ATerm after having backed up over some Close* Sp*, and in one case an
     * optional Paragraph separator, although one rule doesn't have any Sp's in it.
     * So do that backup now, setting flags if either Sp or a paragraph
     * separator are found */

    if (before == SB_Sep || before == SB_CR || before == SB_LF) {
        has_para_sep = TRUE;
        before = backup_one_SB(strbeg, &lpos, utf8_target);
    }

    if (before == SB_Sp) {
        has_sp = TRUE;
        do {
            before = backup_one_SB(strbeg, &lpos, utf8_target);
        }
        while (before == SB_Sp);
    }

    while (before == SB_Close) {
        before = backup_one_SB(strbeg, &lpos, utf8_target);
    }

    /* The next few rules apply only when the backed-up-to is an ATerm, and in
     * most cases an STerm */
    if (before == SB_STerm || before == SB_ATerm) {

        /* So, here the lhs matches
         *      (STerm | ATerm) Close* Sp* (Sep | CR | LF)?
         * and we have set flags if we found an Sp, or the optional Sep,CR,LF.
         * The rules that apply here are:
         *
         * SB8    ATerm Close* Sp*  ×  ( ¬(OLetter | Upper | Lower | Sep | CR
                                           | LF | STerm | ATerm) )* Lower
           SB8a  (STerm | ATerm) Close* Sp*  ×  (SContinue | STerm | ATerm)
           SB9   (STerm | ATerm) Close*  ×  (Close | Sp | Sep | CR | LF)
           SB10  (STerm | ATerm) Close* Sp*  ×  (Sp | Sep | CR | LF)
           SB11  (STerm | ATerm) Close* Sp* (Sep | CR | LF)?  ÷
         */

        /* And all but SB11 forbid having seen a paragraph separator */
        if (! has_para_sep) {
            if (before == SB_ATerm) {          /* SB8 */
                U8 * rpos = (U8 *) curpos;
                SB_enum later = after;

                while (    later != SB_OLetter
                        && later != SB_Upper
                        && later != SB_Lower
                        && later != SB_Sep
                        && later != SB_CR
                        && later != SB_LF
                        && later != SB_STerm
                        && later != SB_ATerm
                        && later != SB_EDGE)
                {
                    later = advance_one_SB(&rpos, strend, utf8_target);
                }
                if (later == SB_Lower) {
                    return FALSE;
                }
            }

            if (   after == SB_SContinue    /* SB8a */
                || after == SB_STerm
                || after == SB_ATerm)
            {
                return FALSE;
            }

            if (! has_sp) {     /* SB9 applies only if there was no Sp* */
                if (   after == SB_Close
                    || after == SB_Sp
                    || after == SB_Sep
                    || after == SB_CR
                    || after == SB_LF)
                {
                    return FALSE;
                }
            }

            /* SB10.  This and SB9 could probably be combined some way, but khw
             * has decided to follow the Unicode rule book precisely for
             * simplified maintenance */
            if (   after == SB_Sp
                || after == SB_Sep
                || after == SB_CR
                || after == SB_LF)
            {
                return FALSE;
            }
        }

        /* SB11.  */
        return TRUE;
    }

    /* Otherwise, do not break.
    SB12.  Any  ×  Any */

    return FALSE;
}

STATIC SB_enum
S_advance_one_SB(pTHX_ U8 ** curpos, const U8 * const strend, const bool utf8_target)
{
    SB_enum sb;

    PERL_ARGS_ASSERT_ADVANCE_ONE_SB;

    if (*curpos >= strend) {
        return SB_EDGE;
    }

    if (utf8_target) {
        do {
            *curpos += UTF8SKIP(*curpos);
            if (*curpos >= strend) {
                return SB_EDGE;
            }
            sb = getSB_VAL_UTF8(*curpos, strend);
        } while (sb == SB_Extend || sb == SB_Format);
    }
    else {
        do {
            (*curpos)++;
            if (*curpos >= strend) {
                return SB_EDGE;
            }
            sb = getSB_VAL_CP(**curpos);
        } while (sb == SB_Extend || sb == SB_Format);
    }

    return sb;
}

STATIC SB_enum
S_backup_one_SB(pTHX_ const U8 * const strbeg, U8 ** curpos, const bool utf8_target)
{
    SB_enum sb;

    PERL_ARGS_ASSERT_BACKUP_ONE_SB;

    if (*curpos < strbeg) {
        return SB_EDGE;
    }

    if (utf8_target) {
        U8 * prev_char_pos = reghopmaybe3(*curpos, -1, strbeg);
        if (! prev_char_pos) {
            return SB_EDGE;
        }

        /* Back up over Extend and Format.  curpos is always just to the right
         * of the characater whose value we are getting */
        do {
            U8 * prev_prev_char_pos;
            if ((prev_prev_char_pos = reghopmaybe3((U8 *) prev_char_pos, -1,
                                                                      strbeg)))
            {
                sb = getSB_VAL_UTF8(prev_prev_char_pos, prev_char_pos);
                *curpos = prev_char_pos;
                prev_char_pos = prev_prev_char_pos;
            }
            else {
                *curpos = (U8 *) strbeg;
                return SB_EDGE;
            }
        } while (sb == SB_Extend || sb == SB_Format);
    }
    else {
        do {
            if (*curpos - 2 < strbeg) {
                *curpos = (U8 *) strbeg;
                return SB_EDGE;
            }
            (*curpos)--;
            sb = getSB_VAL_CP(*(*curpos - 1));
        } while (sb == SB_Extend || sb == SB_Format);
    }

    return sb;
}

STATIC bool
S_isWB(pTHX_ WB_enum previous,
             WB_enum before,
             WB_enum after,
             const U8 * const strbeg,
             const U8 * const curpos,
             const U8 * const strend,
             const bool utf8_target)
{
    /*  Return a boolean as to if the boundary between 'before' and 'after' is
     *  a Unicode word break, using their published algorithm, but tailored for
     *  Perl by treating spans of white space as one unit.  Context may be
     *  needed to make this determination.  If the value for the character
     *  before 'before' is known, it is passed as 'previous'; otherwise that
     *  should be set to WB_UNKNOWN.  The other input parameters give the
     *  boundaries and current position in the matching of the string.  That
     *  is, 'curpos' marks the position where the character whose wb value is
     *  'after' begins.  See http://www.unicode.org/reports/tr29/ */

    U8 * before_pos = (U8 *) curpos;
    U8 * after_pos = (U8 *) curpos;
    WB_enum prev = before;
    WB_enum next;

    PERL_ARGS_ASSERT_ISWB;

    /* Rule numbers in the comments below are as of Unicode 9.0 */

  redo:
    before = prev;
    switch (WB_table[before][after]) {
        case WB_BREAKABLE:
            return TRUE;

        case WB_NOBREAK:
            return FALSE;

        case WB_hs_then_hs:     /* 2 horizontal spaces in a row */
            next = advance_one_WB(&after_pos, strend, utf8_target,
                                 FALSE /* Don't skip Extend nor Format */ );
            /* A space immediately preceeding an Extend or Format is attached
             * to by them, and hence gets separated from previous spaces.
             * Otherwise don't break between horizontal white space */
            return next == WB_Extend || next == WB_Format;

        /* WB4 Ignore Format and Extend characters, except when they appear at
         * the beginning of a region of text.  This code currently isn't
         * general purpose, but it works as the rules are currently and likely
         * to be laid out.  The reason it works is that when 'they appear at
         * the beginning of a region of text', the rule is to break before
         * them, just like any other character.  Therefore, the default rule
         * applies and we don't have to look in more depth.  Should this ever
         * change, we would have to have 2 'case' statements, like in the rules
         * below, and backup a single character (not spacing over the extend
         * ones) and then see if that is one of the region-end characters and
         * go from there */
        case WB_Ex_or_FO_or_ZWJ_then_foo:
            prev = backup_one_WB(&previous, strbeg, &before_pos, utf8_target);
            goto redo;

        case WB_DQ_then_HL + WB_BREAKABLE:
        case WB_DQ_then_HL + WB_NOBREAK:

            /* WB7c  Hebrew_Letter Double_Quote  ×  Hebrew_Letter */

            if (backup_one_WB(&previous, strbeg, &before_pos, utf8_target)
                                                            == WB_Hebrew_Letter)
            {
                return FALSE;
            }

             return WB_table[before][after] - WB_DQ_then_HL == WB_BREAKABLE;

        case WB_HL_then_DQ + WB_BREAKABLE:
        case WB_HL_then_DQ + WB_NOBREAK:

            /* WB7b  Hebrew_Letter  ×  Double_Quote Hebrew_Letter */

            if (advance_one_WB(&after_pos, strend, utf8_target,
                                       TRUE /* Do skip Extend and Format */ )
                                                            == WB_Hebrew_Letter)
            {
                return FALSE;
            }

            return WB_table[before][after] - WB_HL_then_DQ == WB_BREAKABLE;

        case WB_LE_or_HL_then_MB_or_ML_or_SQ + WB_NOBREAK:
        case WB_LE_or_HL_then_MB_or_ML_or_SQ + WB_BREAKABLE:

            /* WB6  (ALetter | Hebrew_Letter)  ×  (MidLetter | MidNumLet
             *       | Single_Quote) (ALetter | Hebrew_Letter) */

            next = advance_one_WB(&after_pos, strend, utf8_target,
                                       TRUE /* Do skip Extend and Format */ );

            if (next == WB_ALetter || next == WB_Hebrew_Letter)
            {
                return FALSE;
            }

            return WB_table[before][after]
                            - WB_LE_or_HL_then_MB_or_ML_or_SQ == WB_BREAKABLE;

        case WB_MB_or_ML_or_SQ_then_LE_or_HL + WB_NOBREAK:
        case WB_MB_or_ML_or_SQ_then_LE_or_HL + WB_BREAKABLE:

            /* WB7  (ALetter | Hebrew_Letter) (MidLetter | MidNumLet
             *       | Single_Quote)  ×  (ALetter | Hebrew_Letter) */

            prev = backup_one_WB(&previous, strbeg, &before_pos, utf8_target);
            if (prev == WB_ALetter || prev == WB_Hebrew_Letter)
            {
                return FALSE;
            }

            return WB_table[before][after]
                            - WB_MB_or_ML_or_SQ_then_LE_or_HL == WB_BREAKABLE;

        case WB_MB_or_MN_or_SQ_then_NU + WB_NOBREAK:
        case WB_MB_or_MN_or_SQ_then_NU + WB_BREAKABLE:

            /* WB11  Numeric (MidNum | (MidNumLet | Single_Quote))  ×  Numeric
             * */

            if (backup_one_WB(&previous, strbeg, &before_pos, utf8_target)
                                                            == WB_Numeric)
            {
                return FALSE;
            }

            return WB_table[before][after]
                                - WB_MB_or_MN_or_SQ_then_NU == WB_BREAKABLE;

        case WB_NU_then_MB_or_MN_or_SQ + WB_NOBREAK:
        case WB_NU_then_MB_or_MN_or_SQ + WB_BREAKABLE:

            /* WB12  Numeric  ×  (MidNum | MidNumLet | Single_Quote) Numeric */

            if (advance_one_WB(&after_pos, strend, utf8_target,
                                       TRUE /* Do skip Extend and Format */ )
                                                            == WB_Numeric)
            {
                return FALSE;
            }

            return WB_table[before][after]
                                - WB_NU_then_MB_or_MN_or_SQ == WB_BREAKABLE;

        case WB_RI_then_RI + WB_NOBREAK:
        case WB_RI_then_RI + WB_BREAKABLE:
            {
                int RI_count = 1;

                /* Do not break within emoji flag sequences. That is, do not
                 * break between regional indicator (RI) symbols if there is an
                 * odd number of RI characters before the potential break
                 * point.
                 *
                 * WB15   sot (RI RI)* RI × RI
                 * WB16 [^RI] (RI RI)* RI × RI */

                while (backup_one_WB(&previous,
                                     strbeg,
                                     &before_pos,
                                     utf8_target) == WB_Regional_Indicator)
                {
                    RI_count++;
                }

                return RI_count % 2 != 1;
            }

        default:
            break;
    }

#ifdef DEBUGGING
    Perl_re_printf( aTHX_  "Unhandled WB pair: WB_table[%d, %d] = %d\n",
                                  before, after, WB_table[before][after]);
    assert(0);
#endif
    return TRUE;
}

STATIC WB_enum
S_advance_one_WB(pTHX_ U8 ** curpos,
                       const U8 * const strend,
                       const bool utf8_target,
                       const bool skip_Extend_Format)
{
    WB_enum wb;

    PERL_ARGS_ASSERT_ADVANCE_ONE_WB;

    if (*curpos >= strend) {
        return WB_EDGE;
    }

    if (utf8_target) {

        /* Advance over Extend and Format */
        do {
            *curpos += UTF8SKIP(*curpos);
            if (*curpos >= strend) {
                return WB_EDGE;
            }
            wb = getWB_VAL_UTF8(*curpos, strend);
        } while (    skip_Extend_Format
                 && (wb == WB_Extend || wb == WB_Format));
    }
    else {
        do {
            (*curpos)++;
            if (*curpos >= strend) {
                return WB_EDGE;
            }
            wb = getWB_VAL_CP(**curpos);
        } while (    skip_Extend_Format
                 && (wb == WB_Extend || wb == WB_Format));
    }

    return wb;
}

STATIC WB_enum
S_backup_one_WB(pTHX_ WB_enum * previous, const U8 * const strbeg, U8 ** curpos, const bool utf8_target)
{
    WB_enum wb;

    PERL_ARGS_ASSERT_BACKUP_ONE_WB;

    /* If we know what the previous character's break value is, don't have
        * to look it up */
    if (*previous != WB_UNKNOWN) {
        wb = *previous;

        /* But we need to move backwards by one */
        if (utf8_target) {
            *curpos = reghopmaybe3(*curpos, -1, strbeg);
            if (! *curpos) {
                *previous = WB_EDGE;
                *curpos = (U8 *) strbeg;
            }
            else {
                *previous = WB_UNKNOWN;
            }
        }
        else {
            (*curpos)--;
            *previous = (*curpos <= strbeg) ? WB_EDGE : WB_UNKNOWN;
        }

        /* And we always back up over these three types */
        if (wb != WB_Extend && wb != WB_Format && wb != WB_ZWJ) {
            return wb;
        }
    }

    if (*curpos < strbeg) {
        return WB_EDGE;
    }

    if (utf8_target) {
        U8 * prev_char_pos = reghopmaybe3(*curpos, -1, strbeg);
        if (! prev_char_pos) {
            return WB_EDGE;
        }

        /* Back up over Extend and Format.  curpos is always just to the right
         * of the characater whose value we are getting */
        do {
            U8 * prev_prev_char_pos;
            if ((prev_prev_char_pos = reghopmaybe3((U8 *) prev_char_pos,
                                                   -1,
                                                   strbeg)))
            {
                wb = getWB_VAL_UTF8(prev_prev_char_pos, prev_char_pos);
                *curpos = prev_char_pos;
                prev_char_pos = prev_prev_char_pos;
            }
            else {
                *curpos = (U8 *) strbeg;
                return WB_EDGE;
            }
        } while (wb == WB_Extend || wb == WB_Format || wb == WB_ZWJ);
    }
    else {
        do {
            if (*curpos - 2 < strbeg) {
                *curpos = (U8 *) strbeg;
                return WB_EDGE;
            }
            (*curpos)--;
            wb = getWB_VAL_CP(*(*curpos - 1));
        } while (wb == WB_Extend || wb == WB_Format);
    }

    return wb;
}

#define EVAL_CLOSE_PAREN_IS(st,expr)                        \
(                                                           \
    (   ( st )                                         ) && \
    (   ( st )->u.eval.close_paren                     ) && \
    ( ( ( st )->u.eval.close_paren ) == ( (expr) + 1 ) )    \
)

#define EVAL_CLOSE_PAREN_IS_TRUE(st,expr)                   \
(                                                           \
    (   ( st )                                         ) && \
    (   ( st )->u.eval.close_paren                     ) && \
    (   ( expr )                                       ) && \
    ( ( ( st )->u.eval.close_paren ) == ( (expr) + 1 ) )    \
)


#define EVAL_CLOSE_PAREN_SET(st,expr) \
    (st)->u.eval.close_paren = ( (expr) + 1 )

#define EVAL_CLOSE_PAREN_CLEAR(st) \
    (st)->u.eval.close_paren = 0

/* returns -1 on failure, $+[0] on success */
STATIC SSize_t
S_regmatch(pTHX_ regmatch_info *reginfo, char *startpos, regnode *prog)
{
    dVAR;
    const bool utf8_target = reginfo->is_utf8_target;
    const U32 uniflags = UTF8_ALLOW_DEFAULT;
    REGEXP *rex_sv = reginfo->prog;
    regexp *rex = ReANY(rex_sv);
    RXi_GET_DECL(rex,rexi);
    /* the current state. This is a cached copy of PL_regmatch_state */
    regmatch_state *st;
    /* cache heavy used fields of st in registers */
    regnode *scan;
    regnode *next;
    U32 n = 0;	/* general value; init to avoid compiler warning */
    SSize_t ln = 0; /* len or last;  init to avoid compiler warning */
    SSize_t endref = 0; /* offset of end of backref when ln is start */
    char *locinput = startpos;
    char *pushinput; /* where to continue after a PUSH */
    I32 nextchr;   /* is always set to UCHARAT(locinput), or -1 at EOS */

    bool result = 0;	    /* return value of S_regmatch */
    U32 depth = 0;            /* depth of backtrack stack */
    U32 nochange_depth = 0; /* depth of GOSUB recursion with nochange */
    const U32 max_nochange_depth =
        (3 * rex->nparens > MAX_RECURSE_EVAL_NOCHANGE_DEPTH) ?
        3 * rex->nparens : MAX_RECURSE_EVAL_NOCHANGE_DEPTH;
    regmatch_state *yes_state = NULL; /* state to pop to on success of
							    subpattern */
    /* mark_state piggy backs on the yes_state logic so that when we unwind 
       the stack on success we can update the mark_state as we go */
    regmatch_state *mark_state = NULL; /* last mark state we have seen */
    regmatch_state *cur_eval = NULL; /* most recent EVAL_AB state */
    struct regmatch_state  *cur_curlyx = NULL; /* most recent curlyx */
    U32 state_num;
    bool no_final = 0;      /* prevent failure from backtracking? */
    bool do_cutgroup = 0;   /* no_final only until next branch/trie entry */
    char *startpoint = locinput;
    SV *popmark = NULL;     /* are we looking for a mark? */
    SV *sv_commit = NULL;   /* last mark name seen in failure */
    SV *sv_yes_mark = NULL; /* last mark name we have seen 
                               during a successful match */
    U32 lastopen = 0;       /* last open we saw */
    bool has_cutgroup = RXp_HAS_CUTGROUP(rex) ? 1 : 0;
    SV* const oreplsv = GvSVn(PL_replgv);
    /* these three flags are set by various ops to signal information to
     * the very next op. They have a useful lifetime of exactly one loop
     * iteration, and are not preserved or restored by state pushes/pops
     */
    bool sw = 0;	    /* the condition value in (?(cond)a|b) */
    bool minmod = 0;	    /* the next "{n,m}" is a "{n,m}?" */
    int logical = 0;	    /* the following EVAL is:
				0: (?{...})
				1: (?(?{...})X|Y)
				2: (??{...})
			       or the following IFMATCH/UNLESSM is:
			        false: plain (?=foo)
				true:  used as a condition: (?(?=foo))
			    */
    PAD* last_pad = NULL;
    dMULTICALL;
    U8 gimme = G_SCALAR;
    CV *caller_cv = NULL;	/* who called us */
    CV *last_pushed_cv = NULL;	/* most recently called (?{}) CV */
    U32 maxopenparen = 0;       /* max '(' index seen so far */
    int to_complement;  /* Invert the result? */
    _char_class_number classnum;
    bool is_utf8_pat = reginfo->is_utf8_pat;
    bool match = FALSE;
    I32 orig_savestack_ix = PL_savestack_ix;
    U8 * script_run_begin = NULL;

/* Solaris Studio 12.3 messes up fetching PL_charclass['\n'] */
#if (defined(__SUNPRO_C) && (__SUNPRO_C == 0x5120) && defined(__x86_64) && defined(USE_64_BIT_ALL))
#  define SOLARIS_BAD_OPTIMIZER
    const U32 *pl_charclass_dup = PL_charclass;
#  define PL_charclass pl_charclass_dup
#endif

#ifdef DEBUGGING
    GET_RE_DEBUG_FLAGS_DECL;
#endif

    /* protect against undef(*^R) */
    SAVEFREESV(SvREFCNT_inc_simple_NN(oreplsv));

    /* shut up 'may be used uninitialized' compiler warnings for dMULTICALL */
    multicall_oldcatch = 0;
    PERL_UNUSED_VAR(multicall_cop);

    PERL_ARGS_ASSERT_REGMATCH;

    st = PL_regmatch_state;

    /* Note that nextchr is a byte even in UTF */
    SET_nextchr;
    scan = prog;

    DEBUG_OPTIMISE_r( DEBUG_EXECUTE_r({
            DUMP_EXEC_POS( locinput, scan, utf8_target, depth );
            Perl_re_printf( aTHX_ "regmatch start\n" );
    }));

    while (scan != NULL) {
	next = scan + NEXT_OFF(scan);
	if (next == scan)
	    next = NULL;
	state_num = OP(scan);

      reenter_switch:
        DEBUG_EXECUTE_r(
            if (state_num <= REGNODE_MAX) {
                SV * const prop = sv_newmortal();
                regnode *rnext = regnext(scan);

                DUMP_EXEC_POS( locinput, scan, utf8_target, depth );
                regprop(rex, prop, scan, reginfo, NULL);
                Perl_re_printf( aTHX_
                    "%*s%" IVdf ":%s(%" IVdf ")\n",
                    INDENT_CHARS(depth), "",
                    (IV)(scan - rexi->program),
                    SvPVX_const(prop),
                    (PL_regkind[OP(scan)] == END || !rnext) ?
                        0 : (IV)(rnext - rexi->program));
            }
        );

        to_complement = 0;

        SET_nextchr;
        assert(nextchr < 256 && (nextchr >= 0 || nextchr == NEXTCHR_EOS));

	switch (state_num) {
	case SBOL: /*  /^../ and /\A../  */
	    if (locinput == reginfo->strbeg)
		break;
	    sayNO;

	case MBOL: /*  /^../m  */
	    if (locinput == reginfo->strbeg ||
		(!NEXTCHR_IS_EOS && locinput[-1] == '\n'))
	    {
		break;
	    }
	    sayNO;

	case GPOS: /*  \G  */
	    if (locinput == reginfo->ganch)
		break;
	    sayNO;

	case KEEPS: /*   \K  */
	    /* update the startpoint */
	    st->u.keeper.val = rex->offs[0].start;
	    rex->offs[0].start = locinput - reginfo->strbeg;
	    PUSH_STATE_GOTO(KEEPS_next, next, locinput);
	    NOT_REACHED; /* NOTREACHED */

	case KEEPS_next_fail:
	    /* rollback the start point change */
	    rex->offs[0].start = st->u.keeper.val;
	    sayNO_SILENT;
	    NOT_REACHED; /* NOTREACHED */

	case MEOL: /* /..$/m  */
	    if (!NEXTCHR_IS_EOS && nextchr != '\n')
		sayNO;
	    break;

	case SEOL: /* /..$/  */
	    if (!NEXTCHR_IS_EOS && nextchr != '\n')
		sayNO;
	    if (reginfo->strend - locinput > 1)
		sayNO;
	    break;

	case EOS: /*  \z  */
	    if (!NEXTCHR_IS_EOS)
		sayNO;
	    break;

	case SANY: /*  /./s  */
	    if (NEXTCHR_IS_EOS)
		sayNO;
            goto increment_locinput;

	case REG_ANY: /*  /./  */
	    if ((NEXTCHR_IS_EOS) || nextchr == '\n')
		sayNO;
            goto increment_locinput;


#undef  ST
#define ST st->u.trie
        case TRIEC: /* (ab|cd) with known charclass */
            /* In this case the charclass data is available inline so
               we can fail fast without a lot of extra overhead. 
             */
            if(!NEXTCHR_IS_EOS && !ANYOF_BITMAP_TEST(scan, nextchr)) {
                DEBUG_EXECUTE_r(
                    Perl_re_exec_indentf( aTHX_  "%sTRIE: failed to match trie start class...%s\n",
                              depth, PL_colors[4], PL_colors[5])
                );
                sayNO_SILENT;
                NOT_REACHED; /* NOTREACHED */
            }
            /* FALLTHROUGH */
	case TRIE:  /* (ab|cd)  */
	    /* the basic plan of execution of the trie is:
	     * At the beginning, run though all the states, and
	     * find the longest-matching word. Also remember the position
	     * of the shortest matching word. For example, this pattern:
	     *    1  2 3 4    5
	     *    ab|a|x|abcd|abc
	     * when matched against the string "abcde", will generate
	     * accept states for all words except 3, with the longest
	     * matching word being 4, and the shortest being 2 (with
	     * the position being after char 1 of the string).
	     *
	     * Then for each matching word, in word order (i.e. 1,2,4,5),
	     * we run the remainder of the pattern; on each try setting
	     * the current position to the character following the word,
	     * returning to try the next word on failure.
	     *
	     * We avoid having to build a list of words at runtime by
	     * using a compile-time structure, wordinfo[].prev, which
	     * gives, for each word, the previous accepting word (if any).
	     * In the case above it would contain the mappings 1->2, 2->0,
	     * 3->0, 4->5, 5->1.  We can use this table to generate, from
	     * the longest word (4 above), a list of all words, by
	     * following the list of prev pointers; this gives us the
	     * unordered list 4,5,1,2. Then given the current word we have
	     * just tried, we can go through the list and find the
	     * next-biggest word to try (so if we just failed on word 2,
	     * the next in the list is 4).
	     *
	     * Since at runtime we don't record the matching position in
	     * the string for each word, we have to work that out for
	     * each word we're about to process. The wordinfo table holds
	     * the character length of each word; given that we recorded
	     * at the start: the position of the shortest word and its
	     * length in chars, we just need to move the pointer the
	     * difference between the two char lengths. Depending on
	     * Unicode status and folding, that's cheap or expensive.
	     *
	     * This algorithm is optimised for the case where are only a
	     * small number of accept states, i.e. 0,1, or maybe 2.
	     * With lots of accepts states, and having to try all of them,
	     * it becomes quadratic on number of accept states to find all
	     * the next words.
	     */

	    {
                /* what type of TRIE am I? (utf8 makes this contextual) */
                DECL_TRIE_TYPE(scan);

                /* what trie are we using right now */
		reg_trie_data * const trie
        	    = (reg_trie_data*)rexi->data->data[ ARG( scan ) ];
		HV * widecharmap = MUTABLE_HV(rexi->data->data[ ARG( scan ) + 1 ]);
                U32 state = trie->startstate;

                if (scan->flags == EXACTL || scan->flags == EXACTFLU8) {
                    _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
                    if (utf8_target
                        && ! NEXTCHR_IS_EOS
                        && UTF8_IS_ABOVE_LATIN1(nextchr)
                        && scan->flags == EXACTL)
                    {
                        /* We only output for EXACTL, as we let the folder
                         * output this message for EXACTFLU8 to avoid
                         * duplication */
                        _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(locinput,
                                                               reginfo->strend);
                    }
                }
                if (   trie->bitmap
                    && (NEXTCHR_IS_EOS || !TRIE_BITMAP_TEST(trie, nextchr)))
                {
        	    if (trie->states[ state ].wordnum) {
        	         DEBUG_EXECUTE_r(
                            Perl_re_exec_indentf( aTHX_  "%sTRIE: matched empty string...%s\n",
                                          depth, PL_colors[4], PL_colors[5])
                        );
			if (!trie->jump)
			    break;
        	    } else {
        	        DEBUG_EXECUTE_r(
                            Perl_re_exec_indentf( aTHX_  "%sTRIE: failed to match trie start class...%s\n",
                                          depth, PL_colors[4], PL_colors[5])
                        );
        	        sayNO_SILENT;
        	   }
                }

            { 
		U8 *uc = ( U8* )locinput;

		STRLEN len = 0;
		STRLEN foldlen = 0;
		U8 *uscan = (U8*)NULL;
		U8 foldbuf[ UTF8_MAXBYTES_CASE + 1 ];
		U32 charcount = 0; /* how many input chars we have matched */
		U32 accepted = 0; /* have we seen any accepting states? */

		ST.jump = trie->jump;
		ST.me = scan;
		ST.firstpos = NULL;
		ST.longfold = FALSE; /* char longer if folded => it's harder */
		ST.nextword = 0;

		/* fully traverse the TRIE; note the position of the
		   shortest accept state and the wordnum of the longest
		   accept state */

		while ( state && uc <= (U8*)(reginfo->strend) ) {
                    U32 base = trie->states[ state ].trans.base;
                    UV uvc = 0;
                    U16 charid = 0;
		    U16 wordnum;
                    wordnum = trie->states[ state ].wordnum;

		    if (wordnum) { /* it's an accept state */
			if (!accepted) {
			    accepted = 1;
			    /* record first match position */
			    if (ST.longfold) {
				ST.firstpos = (U8*)locinput;
				ST.firstchars = 0;
			    }
			    else {
				ST.firstpos = uc;
				ST.firstchars = charcount;
			    }
			}
			if (!ST.nextword || wordnum < ST.nextword)
			    ST.nextword = wordnum;
			ST.topword = wordnum;
		    }

		    DEBUG_TRIE_EXECUTE_r({
                                DUMP_EXEC_POS( (char *)uc, scan, utf8_target, depth );
                                /* HERE */
                                PerlIO_printf( Perl_debug_log,
                                    "%*s%sTRIE: State: %4" UVxf " Accepted: %c ",
                                    INDENT_CHARS(depth), "", PL_colors[4],
			            (UV)state, (accepted ? 'Y' : 'N'));
		    });

		    /* read a char and goto next state */
		    if ( base && (foldlen || uc < (U8*)(reginfo->strend))) {
			I32 offset;
			REXEC_TRIE_READ_CHAR(trie_type, trie, widecharmap, uc,
                                             (U8 *) reginfo->strend, uscan,
                                             len, uvc, charid, foldlen,
                                             foldbuf, uniflags);
			charcount++;
			if (foldlen>0)
			    ST.longfold = TRUE;
			if (charid &&
			     ( ((offset =
			      base + charid - 1 - trie->uniquecharcount)) >= 0)

			     && ((U32)offset < trie->lasttrans)
			     && trie->trans[offset].check == state)
			{
			    state = trie->trans[offset].next;
			}
			else {
			    state = 0;
			}
			uc += len;

		    }
		    else {
			state = 0;
		    }
		    DEBUG_TRIE_EXECUTE_r(
                        Perl_re_printf( aTHX_
		            "TRIE: Charid:%3x CP:%4" UVxf " After State: %4" UVxf "%s\n",
		            charid, uvc, (UV)state, PL_colors[5] );
		    );
		}
		if (!accepted)
		   sayNO;

		/* calculate total number of accept states */
		{
		    U16 w = ST.topword;
		    accepted = 0;
		    while (w) {
			w = trie->wordinfo[w].prev;
			accepted++;
		    }
		    ST.accepted = accepted;
		}

		DEBUG_EXECUTE_r(
                    Perl_re_exec_indentf( aTHX_  "%sTRIE: got %" IVdf " possible matches%s\n",
                        depth,
			PL_colors[4], (IV)ST.accepted, PL_colors[5] );
		);
		goto trie_first_try; /* jump into the fail handler */
	    }}
	    NOT_REACHED; /* NOTREACHED */

	case TRIE_next_fail: /* we failed - try next alternative */
        {
            U8 *uc;
            if ( ST.jump ) {
                /* undo any captures done in the tail part of a branch,
                 * e.g.
                 *    /(?:X(.)(.)|Y(.)).../
                 * where the trie just matches X then calls out to do the
                 * rest of the branch */
                REGCP_UNWIND(ST.cp);
                UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
	    }
	    if (!--ST.accepted) {
	        DEBUG_EXECUTE_r({
                    Perl_re_exec_indentf( aTHX_  "%sTRIE failed...%s\n",
                        depth,
			PL_colors[4],
			PL_colors[5] );
		});
		sayNO_SILENT;
	    }
	    {
		/* Find next-highest word to process.  Note that this code
		 * is O(N^2) per trie run (O(N) per branch), so keep tight */
		U16 min = 0;
		U16 word;
		U16 const nextword = ST.nextword;
		reg_trie_wordinfo * const wordinfo
		    = ((reg_trie_data*)rexi->data->data[ARG(ST.me)])->wordinfo;
		for (word=ST.topword; word; word=wordinfo[word].prev) {
		    if (word > nextword && (!min || word < min))
			min = word;
		}
		ST.nextword = min;
	    }

          trie_first_try:
            if (do_cutgroup) {
                do_cutgroup = 0;
                no_final = 0;
            }

            if ( ST.jump ) {
                ST.lastparen = rex->lastparen;
                ST.lastcloseparen = rex->lastcloseparen;
	        REGCP_SET(ST.cp);
            }

	    /* find start char of end of current word */
	    {
		U32 chars; /* how many chars to skip */
		reg_trie_data * const trie
		    = (reg_trie_data*)rexi->data->data[ARG(ST.me)];

		assert((trie->wordinfo[ST.nextword].len - trie->prefixlen)
			    >=  ST.firstchars);
		chars = (trie->wordinfo[ST.nextword].len - trie->prefixlen)
			    - ST.firstchars;
		uc = ST.firstpos;

		if (ST.longfold) {
		    /* the hard option - fold each char in turn and find
		     * its folded length (which may be different */
		    U8 foldbuf[UTF8_MAXBYTES_CASE + 1];
		    STRLEN foldlen;
		    STRLEN len;
		    UV uvc;
		    U8 *uscan;

		    while (chars) {
			if (utf8_target) {
			    uvc = utf8n_to_uvchr((U8*)uc, UTF8_MAXLEN, &len,
						    uniflags);
			    uc += len;
			}
			else {
			    uvc = *uc;
			    uc++;
			}
			uvc = to_uni_fold(uvc, foldbuf, &foldlen);
			uscan = foldbuf;
			while (foldlen) {
			    if (!--chars)
				break;
			    uvc = utf8n_to_uvchr(uscan, foldlen, &len,
                                                 uniflags);
			    uscan += len;
			    foldlen -= len;
			}
		    }
		}
		else {
		    if (utf8_target)
			while (chars--)
			    uc += UTF8SKIP(uc);
		    else
			uc += chars;
		}
	    }

	    scan = ST.me + ((ST.jump && ST.jump[ST.nextword])
			    ? ST.jump[ST.nextword]
			    : NEXT_OFF(ST.me));

	    DEBUG_EXECUTE_r({
                Perl_re_exec_indentf( aTHX_  "%sTRIE matched word #%d, continuing%s\n",
                    depth,
		    PL_colors[4],
		    ST.nextword,
		    PL_colors[5]
		    );
	    });

	    if ( ST.accepted > 1 || has_cutgroup || ST.jump ) {
		PUSH_STATE_GOTO(TRIE_next, scan, (char*)uc);
		NOT_REACHED; /* NOTREACHED */
	    }
	    /* only one choice left - just continue */
	    DEBUG_EXECUTE_r({
		AV *const trie_words
		    = MUTABLE_AV(rexi->data->data[ARG(ST.me)+TRIE_WORDS_OFFSET]);
		SV ** const tmp = trie_words
                        ? av_fetch(trie_words, ST.nextword - 1, 0) : NULL;
		SV *sv= tmp ? sv_newmortal() : NULL;

                Perl_re_exec_indentf( aTHX_  "%sTRIE: only one match left, short-circuiting: #%d <%s>%s\n",
                    depth, PL_colors[4],
		    ST.nextword,
		    tmp ? pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), 0,
			    PL_colors[0], PL_colors[1],
			    (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0)|PERL_PV_ESCAPE_NONASCII
			) 
		    : "not compiled under -Dr",
		    PL_colors[5] );
	    });

	    locinput = (char*)uc;
	    continue; /* execute rest of RE */
            /* NOTREACHED */
        }
#undef  ST

	case EXACTL:             /*  /abc/l       */
            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;

            /* Complete checking would involve going through every character
             * matched by the string to see if any is above latin1.  But the
             * comparision otherwise might very well be a fast assembly
             * language routine, and I (khw) don't think slowing things down
             * just to check for this warning is worth it.  So this just checks
             * the first character */
            if (utf8_target && UTF8_IS_ABOVE_LATIN1(*locinput)) {
                _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(locinput, reginfo->strend);
            }
            /* FALLTHROUGH */
	case EXACT: {            /*  /abc/        */
	    char *s = STRING(scan);
	    ln = STR_LEN(scan);
	    if (utf8_target != is_utf8_pat) {
		/* The target and the pattern have differing utf8ness. */
		char *l = locinput;
		const char * const e = s + ln;

		if (utf8_target) {
                    /* The target is utf8, the pattern is not utf8.
                     * Above-Latin1 code points can't match the pattern;
                     * invariants match exactly, and the other Latin1 ones need
                     * to be downgraded to a single byte in order to do the
                     * comparison.  (If we could be confident that the target
                     * is not malformed, this could be refactored to have fewer
                     * tests by just assuming that if the first bytes match, it
                     * is an invariant, but there are tests in the test suite
                     * dealing with (??{...}) which violate this) */
		    while (s < e) {
			if (l >= reginfo->strend
                            || UTF8_IS_ABOVE_LATIN1(* (U8*) l))
                        {
                            sayNO;
                        }
                        if (UTF8_IS_INVARIANT(*(U8*)l)) {
			    if (*l != *s) {
                                sayNO;
                            }
                            l++;
                        }
                        else {
                            if (EIGHT_BIT_UTF8_TO_NATIVE(*l, *(l+1)) != * (U8*) s)
                            {
                                sayNO;
                            }
                            l += 2;
                        }
			s++;
		    }
		}
		else {
		    /* The target is not utf8, the pattern is utf8. */
		    while (s < e) {
                        if (l >= reginfo->strend
                            || UTF8_IS_ABOVE_LATIN1(* (U8*) s))
                        {
                            sayNO;
                        }
                        if (UTF8_IS_INVARIANT(*(U8*)s)) {
			    if (*s != *l) {
                                sayNO;
                            }
                            s++;
                        }
                        else {
                            if (EIGHT_BIT_UTF8_TO_NATIVE(*s, *(s+1)) != * (U8*) l)
                            {
                                sayNO;
                            }
                            s += 2;
                        }
			l++;
		    }
		}
		locinput = l;
	    }
            else {
                /* The target and the pattern have the same utf8ness. */
                /* Inline the first character, for speed. */
                if (reginfo->strend - locinput < ln
                    || UCHARAT(s) != nextchr
                    || (ln > 1 && memNE(s, locinput, ln)))
                {
                    sayNO;
                }
                locinput += ln;
            }
	    break;
	    }

	case EXACTFL: {          /*  /abc/il      */
	    re_fold_t folder;
	    const U8 * fold_array;
	    const char * s;
	    U32 fold_utf8_flags;

            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
            folder = foldEQ_locale;
            fold_array = PL_fold_locale;
	    fold_utf8_flags = FOLDEQ_LOCALE;
	    goto do_exactf;

        case EXACTFLU8:           /*  /abc/il; but all 'abc' are above 255, so
                                      is effectively /u; hence to match, target
                                      must be UTF-8. */
            if (! utf8_target) {
                sayNO;
            }
            fold_utf8_flags =  FOLDEQ_LOCALE | FOLDEQ_S1_ALREADY_FOLDED
                                             | FOLDEQ_S1_FOLDS_SANE;
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    goto do_exactf;

	case EXACTFU_SS:         /*  /\x{df}/iu   */
	case EXACTFU:            /*  /abc/iu      */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    fold_utf8_flags = is_utf8_pat ? FOLDEQ_S1_ALREADY_FOLDED : 0;
	    goto do_exactf;

        case EXACTFAA_NO_TRIE:   /* This node only generated for non-utf8
                                   patterns */
            assert(! is_utf8_pat);
            /* FALLTHROUGH */
	case EXACTFAA:            /*  /abc/iaa     */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    fold_utf8_flags = FOLDEQ_UTF8_NOMIX_ASCII;
	    goto do_exactf;

        case EXACTF:             /*  /abc/i    This node only generated for
                                               non-utf8 patterns */
            assert(! is_utf8_pat);
	    folder = foldEQ;
	    fold_array = PL_fold;
	    fold_utf8_flags = 0;

	  do_exactf:
	    s = STRING(scan);
	    ln = STR_LEN(scan);

	    if (utf8_target
                || is_utf8_pat
                || state_num == EXACTFU_SS
                || (state_num == EXACTFL && IN_UTF8_CTYPE_LOCALE))
            {
	      /* Either target or the pattern are utf8, or has the issue where
	       * the fold lengths may differ. */
		const char * const l = locinput;
		char *e = reginfo->strend;

		if (! foldEQ_utf8_flags(s, 0,  ln, is_utf8_pat,
			                l, &e, 0,  utf8_target, fold_utf8_flags))
		{
		    sayNO;
		}
		locinput = e;
		break;
	    }

	    /* Neither the target nor the pattern are utf8 */
	    if (UCHARAT(s) != nextchr
                && !NEXTCHR_IS_EOS
		&& UCHARAT(s) != fold_array[nextchr])
	    {
		sayNO;
	    }
	    if (reginfo->strend - locinput < ln)
		sayNO;
	    if (ln > 1 && ! folder(s, locinput, ln))
		sayNO;
	    locinput += ln;
	    break;
	}

	case NBOUNDL: /*  /\B/l  */
            to_complement = 1;
            /* FALLTHROUGH */

	case BOUNDL:  /*  /\b/l  */
        {
            bool b1, b2;
            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;

            if (FLAGS(scan) != TRADITIONAL_BOUND) {
                if (! IN_UTF8_CTYPE_LOCALE) {
                    Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE),
                                                B_ON_NON_UTF8_LOCALE_IS_WRONG);
                }
                goto boundu;
            }

	    if (utf8_target) {
		if (locinput == reginfo->strbeg)
		    b1 = isWORDCHAR_LC('\n');
		else {
                    b1 = isWORDCHAR_LC_utf8_safe(reghop3((U8*)locinput, -1,
                                                        (U8*)(reginfo->strbeg)),
                                                 (U8*)(reginfo->strend));
		}
                b2 = (NEXTCHR_IS_EOS)
                    ? isWORDCHAR_LC('\n')
                    : isWORDCHAR_LC_utf8_safe((U8*) locinput,
                                              (U8*) reginfo->strend);
	    }
	    else { /* Here the string isn't utf8 */
		b1 = (locinput == reginfo->strbeg)
                     ? isWORDCHAR_LC('\n')
                     : isWORDCHAR_LC(UCHARAT(locinput - 1));
                b2 = (NEXTCHR_IS_EOS)
                    ? isWORDCHAR_LC('\n')
                    : isWORDCHAR_LC(nextchr);
	    }
            if (to_complement ^ (b1 == b2)) {
                sayNO;
            }
	    break;
        }

	case NBOUND:  /*  /\B/   */
            to_complement = 1;
            /* FALLTHROUGH */

	case BOUND:   /*  /\b/   */
	    if (utf8_target) {
                goto bound_utf8;
            }
            goto bound_ascii_match_only;

	case NBOUNDA: /*  /\B/a  */
            to_complement = 1;
            /* FALLTHROUGH */

	case BOUNDA:  /*  /\b/a  */
        {
            bool b1, b2;

          bound_ascii_match_only:
            /* Here the string isn't utf8, or is utf8 and only ascii characters
             * are to match \w.  In the latter case looking at the byte just
             * prior to the current one may be just the final byte of a
             * multi-byte character.  This is ok.  There are two cases:
             * 1) it is a single byte character, and then the test is doing
             *    just what it's supposed to.
             * 2) it is a multi-byte character, in which case the final byte is
             *    never mistakable for ASCII, and so the test will say it is
             *    not a word character, which is the correct answer. */
            b1 = (locinput == reginfo->strbeg)
                 ? isWORDCHAR_A('\n')
                 : isWORDCHAR_A(UCHARAT(locinput - 1));
            b2 = (NEXTCHR_IS_EOS)
                ? isWORDCHAR_A('\n')
                : isWORDCHAR_A(nextchr);
            if (to_complement ^ (b1 == b2)) {
                sayNO;
            }
	    break;
        }

	case NBOUNDU: /*  /\B/u  */
            to_complement = 1;
            /* FALLTHROUGH */

	case BOUNDU:  /*  /\b/u  */

          boundu:
            if (UNLIKELY(reginfo->strbeg >= reginfo->strend)) {
                match = FALSE;
            }
            else if (utf8_target) {
              bound_utf8:
                switch((bound_type) FLAGS(scan)) {
                    case TRADITIONAL_BOUND:
                    {
                        bool b1, b2;
                        b1 = (locinput == reginfo->strbeg)
                             ? 0 /* isWORDCHAR_L1('\n') */
                             : isWORDCHAR_utf8_safe(
                                               reghop3((U8*)locinput,
                                                       -1,
                                                       (U8*)(reginfo->strbeg)),
                                                    (U8*) reginfo->strend);
                        b2 = (NEXTCHR_IS_EOS)
                            ? 0 /* isWORDCHAR_L1('\n') */
                            : isWORDCHAR_utf8_safe((U8*)locinput,
                                                   (U8*) reginfo->strend);
                        match = cBOOL(b1 != b2);
                        break;
                    }
                    case GCB_BOUND:
                        if (locinput == reginfo->strbeg || NEXTCHR_IS_EOS) {
                            match = TRUE; /* GCB always matches at begin and
                                             end */
                        }
                        else {
                            /* Find the gcb values of previous and current
                             * chars, then see if is a break point */
                            match = isGCB(getGCB_VAL_UTF8(
                                                reghop3((U8*)locinput,
                                                        -1,
                                                        (U8*)(reginfo->strbeg)),
                                                (U8*) reginfo->strend),
                                          getGCB_VAL_UTF8((U8*) locinput,
                                                        (U8*) reginfo->strend),
                                          (U8*) reginfo->strbeg,
                                          (U8*) locinput,
                                          utf8_target);
                        }
                        break;

                    case LB_BOUND:
                        if (locinput == reginfo->strbeg) {
                            match = FALSE;
                        }
                        else if (NEXTCHR_IS_EOS) {
                            match = TRUE;
                        }
                        else {
                            match = isLB(getLB_VAL_UTF8(
                                                reghop3((U8*)locinput,
                                                        -1,
                                                        (U8*)(reginfo->strbeg)),
                                                (U8*) reginfo->strend),
                                          getLB_VAL_UTF8((U8*) locinput,
                                                        (U8*) reginfo->strend),
                                          (U8*) reginfo->strbeg,
                                          (U8*) locinput,
                                          (U8*) reginfo->strend,
                                          utf8_target);
                        }
                        break;

                    case SB_BOUND: /* Always matches at begin and end */
                        if (locinput == reginfo->strbeg || NEXTCHR_IS_EOS) {
                            match = TRUE;
                        }
                        else {
                            match = isSB(getSB_VAL_UTF8(
                                                reghop3((U8*)locinput,
                                                        -1,
                                                        (U8*)(reginfo->strbeg)),
                                                (U8*) reginfo->strend),
                                          getSB_VAL_UTF8((U8*) locinput,
                                                        (U8*) reginfo->strend),
                                          (U8*) reginfo->strbeg,
                                          (U8*) locinput,
                                          (U8*) reginfo->strend,
                                          utf8_target);
                        }
                        break;

                    case WB_BOUND:
                        if (locinput == reginfo->strbeg || NEXTCHR_IS_EOS) {
                            match = TRUE;
                        }
                        else {
                            match = isWB(WB_UNKNOWN,
                                         getWB_VAL_UTF8(
                                                reghop3((U8*)locinput,
                                                        -1,
                                                        (U8*)(reginfo->strbeg)),
                                                (U8*) reginfo->strend),
                                          getWB_VAL_UTF8((U8*) locinput,
                                                        (U8*) reginfo->strend),
                                          (U8*) reginfo->strbeg,
                                          (U8*) locinput,
                                          (U8*) reginfo->strend,
                                          utf8_target);
                        }
                        break;
                }
	    }
	    else {  /* Not utf8 target */
                switch((bound_type) FLAGS(scan)) {
                    case TRADITIONAL_BOUND:
                    {
                        bool b1, b2;
                        b1 = (locinput == reginfo->strbeg)
                            ? 0 /* isWORDCHAR_L1('\n') */
                            : isWORDCHAR_L1(UCHARAT(locinput - 1));
                        b2 = (NEXTCHR_IS_EOS)
                            ? 0 /* isWORDCHAR_L1('\n') */
                            : isWORDCHAR_L1(nextchr);
                        match = cBOOL(b1 != b2);
                        break;
                    }

                    case GCB_BOUND:
                        if (locinput == reginfo->strbeg || NEXTCHR_IS_EOS) {
                            match = TRUE; /* GCB always matches at begin and
                                             end */
                        }
                        else {  /* Only CR-LF combo isn't a GCB in 0-255
                                   range */
                            match =    UCHARAT(locinput - 1) != '\r'
                                    || UCHARAT(locinput) != '\n';
                        }
                        break;

                    case LB_BOUND:
                        if (locinput == reginfo->strbeg) {
                            match = FALSE;
                        }
                        else if (NEXTCHR_IS_EOS) {
                            match = TRUE;
                        }
                        else {
                            match = isLB(getLB_VAL_CP(UCHARAT(locinput -1)),
                                         getLB_VAL_CP(UCHARAT(locinput)),
                                         (U8*) reginfo->strbeg,
                                         (U8*) locinput,
                                         (U8*) reginfo->strend,
                                         utf8_target);
                        }
                        break;

                    case SB_BOUND: /* Always matches at begin and end */
                        if (locinput == reginfo->strbeg || NEXTCHR_IS_EOS) {
                            match = TRUE;
                        }
                        else {
                            match = isSB(getSB_VAL_CP(UCHARAT(locinput -1)),
                                         getSB_VAL_CP(UCHARAT(locinput)),
                                         (U8*) reginfo->strbeg,
                                         (U8*) locinput,
                                         (U8*) reginfo->strend,
                                         utf8_target);
                        }
                        break;

                    case WB_BOUND:
                        if (locinput == reginfo->strbeg || NEXTCHR_IS_EOS) {
                            match = TRUE;
                        }
                        else {
                            match = isWB(WB_UNKNOWN,
                                         getWB_VAL_CP(UCHARAT(locinput -1)),
                                         getWB_VAL_CP(UCHARAT(locinput)),
                                         (U8*) reginfo->strbeg,
                                         (U8*) locinput,
                                         (U8*) reginfo->strend,
                                         utf8_target);
                        }
                        break;
                }
	    }

            if (to_complement ^ ! match) {
                sayNO;
            }
	    break;

	case ANYOFL:  /*  /[abc]/l      */
            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;

            if (ANYOFL_UTF8_LOCALE_REQD(FLAGS(scan)) && ! IN_UTF8_CTYPE_LOCALE)
            {
              Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE), utf8_locale_required);
            }
            /* FALLTHROUGH */
	case ANYOFD:  /*   /[abc]/d       */
	case ANYOF:  /*   /[abc]/       */
            if (NEXTCHR_IS_EOS)
                sayNO;
	    if (utf8_target && ! UTF8_IS_INVARIANT(*locinput)) {
	        if (!reginclass(rex, scan, (U8*)locinput, (U8*)reginfo->strend,
                                                                   utf8_target))
		    sayNO;
		locinput += UTF8SKIP(locinput);
	    }
	    else {
		if (!REGINCLASS(rex, scan, (U8*)locinput, utf8_target))
		    sayNO;
		locinput++;
	    }
	    break;

        case ANYOFM:
            if (NEXTCHR_IS_EOS || (UCHARAT(locinput) & FLAGS(scan)) != ARG(scan)) {
                sayNO;
            }
            locinput++;
            break;

        case ASCII:
            if (NEXTCHR_IS_EOS || ! isASCII(UCHARAT(locinput))) {
                sayNO;
            }

            locinput++;     /* ASCII is always single byte */
            break;

        case NASCII:
            if (NEXTCHR_IS_EOS || isASCII(UCHARAT(locinput))) {
                sayNO;
            }

            goto increment_locinput;
            break;

        /* The argument (FLAGS) to all the POSIX node types is the class number
         * */

        case NPOSIXL:   /* \W or [:^punct:] etc. under /l */
            to_complement = 1;
            /* FALLTHROUGH */

        case POSIXL:    /* \w or [:punct:] etc. under /l */
            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
            if (NEXTCHR_IS_EOS)
                sayNO;

            /* Use isFOO_lc() for characters within Latin1.  (Note that
             * UTF8_IS_INVARIANT works even on non-UTF-8 strings, or else
             * wouldn't be invariant) */
            if (UTF8_IS_INVARIANT(nextchr) || ! utf8_target) {
                if (! (to_complement ^ cBOOL(isFOO_lc(FLAGS(scan), (U8) nextchr)))) {
                    sayNO;
                }

                locinput++;
                break;
            }

            if (! UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(locinput, reginfo->strend)) {
                /* An above Latin-1 code point, or malformed */
                _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(locinput,
                                                       reginfo->strend);
                goto utf8_posix_above_latin1;
            }

            /* Here is a UTF-8 variant code point below 256 and the target is
             * UTF-8 */
            if (! (to_complement ^ cBOOL(isFOO_lc(FLAGS(scan),
                                            EIGHT_BIT_UTF8_TO_NATIVE(nextchr,
                                            *(locinput + 1))))))
            {
                sayNO;
            }

            goto increment_locinput;

        case NPOSIXD:   /* \W or [:^punct:] etc. under /d */
            to_complement = 1;
            /* FALLTHROUGH */

        case POSIXD:    /* \w or [:punct:] etc. under /d */
            if (utf8_target) {
                goto utf8_posix;
            }
            goto posixa;

        case NPOSIXA:   /* \W or [:^punct:] etc. under /a */

            if (NEXTCHR_IS_EOS) {
                sayNO;
            }

            /* All UTF-8 variants match */
            if (! UTF8_IS_INVARIANT(nextchr)) {
                goto increment_locinput;
            }

            to_complement = 1;
            goto join_nposixa;

        case POSIXA:    /* \w or [:punct:] etc. under /a */

          posixa:
            /* We get here through POSIXD, NPOSIXD, and NPOSIXA when not in
             * UTF-8, and also from NPOSIXA even in UTF-8 when the current
             * character is a single byte */

            if (NEXTCHR_IS_EOS) {
                sayNO;
            }

          join_nposixa:

            if (! (to_complement ^ cBOOL(_generic_isCC_A(nextchr,
                                                                FLAGS(scan)))))
            {
                sayNO;
            }

            /* Here we are either not in utf8, or we matched a utf8-invariant,
             * so the next char is the next byte */
            locinput++;
            break;

        case NPOSIXU:   /* \W or [:^punct:] etc. under /u */
            to_complement = 1;
            /* FALLTHROUGH */

        case POSIXU:    /* \w or [:punct:] etc. under /u */
          utf8_posix:
            if (NEXTCHR_IS_EOS) {
                sayNO;
            }

            /* Use _generic_isCC() for characters within Latin1.  (Note that
             * UTF8_IS_INVARIANT works even on non-UTF-8 strings, or else
             * wouldn't be invariant) */
            if (UTF8_IS_INVARIANT(nextchr) || ! utf8_target) {
                if (! (to_complement ^ cBOOL(_generic_isCC(nextchr,
                                                           FLAGS(scan)))))
                {
                    sayNO;
                }
                locinput++;
            }
            else if (UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(locinput, reginfo->strend)) {
                if (! (to_complement
                       ^ cBOOL(_generic_isCC(EIGHT_BIT_UTF8_TO_NATIVE(nextchr,
                                                               *(locinput + 1)),
                                             FLAGS(scan)))))
                {
                    sayNO;
                }
                locinput += 2;
            }
            else {  /* Handle above Latin-1 code points */
              utf8_posix_above_latin1:
                classnum = (_char_class_number) FLAGS(scan);
                switch (classnum) {
                    default:
                        if (! (to_complement
                           ^ cBOOL(_invlist_contains_cp(
                                      PL_XPosix_ptrs[classnum],
                                      utf8_to_uvchr_buf((U8 *) locinput,
                                                        (U8 *) reginfo->strend,
                                                        NULL)))))
                        {
                            sayNO;
                        }
                        break;
                    case _CC_ENUM_SPACE:
                        if (! (to_complement
                                    ^ cBOOL(is_XPERLSPACE_high(locinput))))
                        {
                            sayNO;
                        }
                        break;
                    case _CC_ENUM_BLANK:
                        if (! (to_complement
                                        ^ cBOOL(is_HORIZWS_high(locinput))))
                        {
                            sayNO;
                        }
                        break;
                    case _CC_ENUM_XDIGIT:
                        if (! (to_complement
                                        ^ cBOOL(is_XDIGIT_high(locinput))))
                        {
                            sayNO;
                        }
                        break;
                    case _CC_ENUM_VERTSPACE:
                        if (! (to_complement
                                        ^ cBOOL(is_VERTWS_high(locinput))))
                        {
                            sayNO;
                        }
                        break;
                    case _CC_ENUM_CNTRL:    /* These can't match above Latin1 */
                    case _CC_ENUM_ASCII:
                        if (! to_complement) {
                            sayNO;
                        }
                        break;
                }
                locinput += UTF8SKIP(locinput);
            }
            break;

	case CLUMP: /* Match \X: logical Unicode character.  This is defined as
		       a Unicode extended Grapheme Cluster */
	    if (NEXTCHR_IS_EOS)
		sayNO;
	    if  (! utf8_target) {

		/* Match either CR LF  or '.', as all the other possibilities
		 * require utf8 */
		locinput++;	    /* Match the . or CR */
		if (nextchr == '\r' /* And if it was CR, and the next is LF,
				       match the LF */
		    && locinput < reginfo->strend
		    && UCHARAT(locinput) == '\n')
                {
                    locinput++;
                }
	    }
	    else {

                /* Get the gcb type for the current character */
                GCB_enum prev_gcb = getGCB_VAL_UTF8((U8*) locinput,
                                                       (U8*) reginfo->strend);

                /* Then scan through the input until we get to the first
                 * character whose type is supposed to be a gcb with the
                 * current character.  (There is always a break at the
                 * end-of-input) */
                locinput += UTF8SKIP(locinput);
                while (locinput < reginfo->strend) {
                    GCB_enum cur_gcb = getGCB_VAL_UTF8((U8*) locinput,
                                                         (U8*) reginfo->strend);
                    if (isGCB(prev_gcb, cur_gcb,
                              (U8*) reginfo->strbeg, (U8*) locinput,
                              utf8_target))
                    {
                        break;
                    }

                    prev_gcb = cur_gcb;
                    locinput += UTF8SKIP(locinput);
                }


	    }
	    break;
            
	case NREFFL:  /*  /\g{name}/il  */
	{   /* The capture buffer cases.  The ones beginning with N for the
	       named buffers just convert to the equivalent numbered and
	       pretend they were called as the corresponding numbered buffer
	       op.  */
	    /* don't initialize these in the declaration, it makes C++
	       unhappy */
	    const char *s;
	    char type;
	    re_fold_t folder;
	    const U8 *fold_array;
	    UV utf8_fold_flags;

            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
	    folder = foldEQ_locale;
	    fold_array = PL_fold_locale;
	    type = REFFL;
	    utf8_fold_flags = FOLDEQ_LOCALE;
	    goto do_nref;

	case NREFFA:  /*  /\g{name}/iaa  */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    type = REFFA;
	    utf8_fold_flags = FOLDEQ_UTF8_NOMIX_ASCII;
	    goto do_nref;

	case NREFFU:  /*  /\g{name}/iu  */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    type = REFFU;
	    utf8_fold_flags = 0;
	    goto do_nref;

	case NREFF:  /*  /\g{name}/i  */
	    folder = foldEQ;
	    fold_array = PL_fold;
	    type = REFF;
	    utf8_fold_flags = 0;
	    goto do_nref;

	case NREF:  /*  /\g{name}/   */
	    type = REF;
	    folder = NULL;
	    fold_array = NULL;
	    utf8_fold_flags = 0;
	  do_nref:

	    /* For the named back references, find the corresponding buffer
	     * number */
	    n = reg_check_named_buff_matched(rex,scan);

            if ( ! n ) {
                sayNO;
	    }
	    goto do_nref_ref_common;

	case REFFL:  /*  /\1/il  */
            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
	    folder = foldEQ_locale;
	    fold_array = PL_fold_locale;
	    utf8_fold_flags = FOLDEQ_LOCALE;
	    goto do_ref;

	case REFFA:  /*  /\1/iaa  */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    utf8_fold_flags = FOLDEQ_UTF8_NOMIX_ASCII;
	    goto do_ref;

	case REFFU:  /*  /\1/iu  */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    utf8_fold_flags = 0;
	    goto do_ref;

	case REFF:  /*  /\1/i  */
	    folder = foldEQ;
	    fold_array = PL_fold;
	    utf8_fold_flags = 0;
	    goto do_ref;

        case REF:  /*  /\1/    */
	    folder = NULL;
	    fold_array = NULL;
	    utf8_fold_flags = 0;

	  do_ref:
	    type = OP(scan);
	    n = ARG(scan);  /* which paren pair */

	  do_nref_ref_common:
	    ln = rex->offs[n].start;
	    endref = rex->offs[n].end;
	    reginfo->poscache_iter = reginfo->poscache_maxiter; /* Void cache */
	    if (rex->lastparen < n || ln == -1 || endref == -1)
		sayNO;			/* Do not match unless seen CLOSEn. */
	    if (ln == endref)
		break;

	    s = reginfo->strbeg + ln;
	    if (type != REF	/* REF can do byte comparison */
		&& (utf8_target || type == REFFU || type == REFFL))
	    {
		char * limit = reginfo->strend;

		/* This call case insensitively compares the entire buffer
		    * at s, with the current input starting at locinput, but
                    * not going off the end given by reginfo->strend, and
                    * returns in <limit> upon success, how much of the
                    * current input was matched */
		if (! foldEQ_utf8_flags(s, NULL, endref - ln, utf8_target,
				    locinput, &limit, 0, utf8_target, utf8_fold_flags))
		{
		    sayNO;
		}
		locinput = limit;
		break;
	    }

	    /* Not utf8:  Inline the first character, for speed. */
	    if (!NEXTCHR_IS_EOS &&
                UCHARAT(s) != nextchr &&
		(type == REF ||
		 UCHARAT(s) != fold_array[nextchr]))
		sayNO;
	    ln = endref - ln;
	    if (locinput + ln > reginfo->strend)
		sayNO;
	    if (ln > 1 && (type == REF
			   ? memNE(s, locinput, ln)
			   : ! folder(s, locinput, ln)))
		sayNO;
	    locinput += ln;
	    break;
	}

	case NOTHING: /* null op; e.g. the 'nothing' following
                       * the '*' in m{(a+|b)*}' */
	    break;
	case TAIL: /* placeholder while compiling (A|B|C) */
	    break;

#undef  ST
#define ST st->u.eval
#define CUR_EVAL cur_eval->u.eval

	{
	    SV *ret;
	    REGEXP *re_sv;
            regexp *re;
            regexp_internal *rei;
            regnode *startpoint;
            U32 arg;

	case GOSUB: /*    /(...(?1))/   /(...(?&foo))/   */
            arg= (U32)ARG(scan);
            if (cur_eval && cur_eval->locinput == locinput) {
                if ( ++nochange_depth > max_nochange_depth )
                    Perl_croak(aTHX_ 
                        "Pattern subroutine nesting without pos change"
                        " exceeded limit in regex");
            } else {
                nochange_depth = 0;
            }
	    re_sv = rex_sv;
            re = rex;
            rei = rexi;
            startpoint = scan + ARG2L(scan);
            EVAL_CLOSE_PAREN_SET( st, arg );
            /* Detect infinite recursion
             *
             * A pattern like /(?R)foo/ or /(?<x>(?&y)foo)(?<y>(?&x)bar)/
             * or "a"=~/(.(?2))((?<=(?=(?1)).))/ could recurse forever.
             * So we track the position in the string we are at each time
             * we recurse and if we try to enter the same routine twice from
             * the same position we throw an error.
             */
            if ( rex->recurse_locinput[arg] == locinput ) {
                /* FIXME: we should show the regop that is failing as part
                 * of the error message. */
                Perl_croak(aTHX_ "Infinite recursion in regex");
            } else {
                ST.prev_recurse_locinput= rex->recurse_locinput[arg];
                rex->recurse_locinput[arg]= locinput;

                DEBUG_r({
                    GET_RE_DEBUG_FLAGS_DECL;
                    DEBUG_STACK_r({
                        Perl_re_exec_indentf( aTHX_
                            "entering GOSUB, prev_recurse_locinput=%p recurse_locinput[%d]=%p\n",
                            depth, ST.prev_recurse_locinput, arg, rex->recurse_locinput[arg]
                        );
                    });
                });
            }

            /* Save all the positions seen so far. */
            ST.cp = regcppush(rex, 0, maxopenparen);
            REGCP_SET(ST.lastcp);

            /* and then jump to the code we share with EVAL */
            goto eval_recurse_doit;
            /* NOTREACHED */

        case EVAL:  /*   /(?{...})B/   /(??{A})B/  and  /(?(?{...})X|Y)B/   */
            if (cur_eval && cur_eval->locinput==locinput) {
		if ( ++nochange_depth > max_nochange_depth )
                    Perl_croak(aTHX_ "EVAL without pos change exceeded limit in regex");
            } else {
                nochange_depth = 0;
            }    
	    {
		/* execute the code in the {...} */

		dSP;
		IV before;
		OP * const oop = PL_op;
		COP * const ocurcop = PL_curcop;
		OP *nop;
		CV *newcv;

		/* save *all* paren positions */
                regcppush(rex, 0, maxopenparen);
                REGCP_SET(ST.lastcp);

		if (!caller_cv)
		    caller_cv = find_runcv(NULL);

		n = ARG(scan);

		if (rexi->data->what[n] == 'r') { /* code from an external qr */
                    newcv = (ReANY(
                                    (REGEXP*)(rexi->data->data[n])
                            ))->qr_anoncv;
		    nop = (OP*)rexi->data->data[n+1];
		}
		else if (rexi->data->what[n] == 'l') { /* literal code */
		    newcv = caller_cv;
		    nop = (OP*)rexi->data->data[n];
		    assert(CvDEPTH(newcv));
		}
		else {
		    /* literal with own CV */
		    assert(rexi->data->what[n] == 'L');
		    newcv = rex->qr_anoncv;
		    nop = (OP*)rexi->data->data[n];
		}

                /* Some notes about MULTICALL and the context and save stacks.
                 *
                 * In something like
                 *   /...(?{ my $x)}...(?{ my $y)}...(?{ my $z)}.../
                 * since codeblocks don't introduce a new scope (so that
                 * local() etc accumulate), at the end of a successful
                 * match there will be a SAVEt_CLEARSV on the savestack
                 * for each of $x, $y, $z. If the three code blocks above
                 * happen to have come from different CVs (e.g. via
                 * embedded qr//s), then we must ensure that during any
                 * savestack unwinding, PL_comppad always points to the
                 * right pad at each moment. We achieve this by
                 * interleaving SAVEt_COMPPAD's on the savestack whenever
                 * there is a change of pad.
                 * In theory whenever we call a code block, we should
                 * push a CXt_SUB context, then pop it on return from
                 * that code block. This causes a bit of an issue in that
                 * normally popping a context also clears the savestack
                 * back to cx->blk_oldsaveix, but here we specifically
                 * don't want to clear the save stack on exit from the
                 * code block.
                 * Also for efficiency we don't want to keep pushing and
                 * popping the single SUB context as we backtrack etc.
                 * So instead, we push a single context the first time
                 * we need, it, then hang onto it until the end of this
                 * function. Whenever we encounter a new code block, we
                 * update the CV etc if that's changed. During the times
                 * in this function where we're not executing a code
                 * block, having the SUB context still there is a bit
                 * naughty - but we hope that no-one notices.
                 * When the SUB context is initially pushed, we fake up
                 * cx->blk_oldsaveix to be as if we'd pushed this context
                 * on first entry to S_regmatch rather than at some random
                 * point during the regexe execution. That way if we
                 * croak, popping the context stack will ensure that
                 * *everything* SAVEd by this function is undone and then
                 * the context popped, rather than e.g., popping the
                 * context (and restoring the original PL_comppad) then
                 * popping more of the savestack and restoring a bad
                 * PL_comppad.
                 */

                /* If this is the first EVAL, push a MULTICALL. On
                 * subsequent calls, if we're executing a different CV, or
                 * if PL_comppad has got messed up from backtracking
                 * through SAVECOMPPADs, then refresh the context.
                 */
		if (newcv != last_pushed_cv || PL_comppad != last_pad)
		{
                    U8 flags = (CXp_SUB_RE |
                                ((newcv == caller_cv) ? CXp_SUB_RE_FAKE : 0));
                    SAVECOMPPAD();
		    if (last_pushed_cv) {
			CHANGE_MULTICALL_FLAGS(newcv, flags);
		    }
		    else {
			PUSH_MULTICALL_FLAGS(newcv, flags);
		    }
                    /* see notes above */
                    CX_CUR()->blk_oldsaveix = orig_savestack_ix;

		    last_pushed_cv = newcv;
		}
		else {
                    /* these assignments are just to silence compiler
                     * warnings */
		    multicall_cop = NULL;
		}
		last_pad = PL_comppad;

		/* the initial nextstate you would normally execute
		 * at the start of an eval (which would cause error
		 * messages to come from the eval), may be optimised
		 * away from the execution path in the regex code blocks;
		 * so manually set PL_curcop to it initially */
		{
		    OP *o = cUNOPx(nop)->op_first;
		    assert(o->op_type == OP_NULL);
		    if (o->op_targ == OP_SCOPE) {
			o = cUNOPo->op_first;
		    }
		    else {
			assert(o->op_targ == OP_LEAVE);
			o = cUNOPo->op_first;
			assert(o->op_type == OP_ENTER);
			o = OpSIBLING(o);
		    }

		    if (o->op_type != OP_STUB) {
			assert(    o->op_type == OP_NEXTSTATE
				|| o->op_type == OP_DBSTATE
				|| (o->op_type == OP_NULL
				    &&  (  o->op_targ == OP_NEXTSTATE
					|| o->op_targ == OP_DBSTATE
					)
				    )
			);
			PL_curcop = (COP*)o;
		    }
		}
		nop = nop->op_next;

                DEBUG_STATE_r( Perl_re_printf( aTHX_
		    "  re EVAL PL_op=0x%" UVxf "\n", PTR2UV(nop)) );

		rex->offs[0].end = locinput - reginfo->strbeg;
                if (reginfo->info_aux_eval->pos_magic)
                    MgBYTEPOS_set(reginfo->info_aux_eval->pos_magic,
                                  reginfo->sv, reginfo->strbeg,
                                  locinput - reginfo->strbeg);

                if (sv_yes_mark) {
                    SV *sv_mrk = get_sv("REGMARK", 1);
                    sv_setsv(sv_mrk, sv_yes_mark);
                }

		/* we don't use MULTICALL here as we want to call the
		 * first op of the block of interest, rather than the
		 * first op of the sub. Also, we don't want to free
                 * the savestack frame */
		before = (IV)(SP-PL_stack_base);
		PL_op = nop;
		CALLRUNOPS(aTHX);			/* Scalar context. */
		SPAGAIN;
		if ((IV)(SP-PL_stack_base) == before)
		    ret = &PL_sv_undef;   /* protect against empty (?{}) blocks. */
		else {
		    ret = POPs;
		    PUTBACK;
		}

		/* before restoring everything, evaluate the returned
		 * value, so that 'uninit' warnings don't use the wrong
		 * PL_op or pad. Also need to process any magic vars
		 * (e.g. $1) *before* parentheses are restored */

		PL_op = NULL;

                re_sv = NULL;
		if (logical == 0)        /*   (?{})/   */
		    sv_setsv(save_scalar(PL_replgv), ret); /* $^R */
		else if (logical == 1) { /*   /(?(?{...})X|Y)/    */
		    sw = cBOOL(SvTRUE_NN(ret));
		    logical = 0;
		}
		else {                   /*  /(??{})  */
		    /*  if its overloaded, let the regex compiler handle
		     *  it; otherwise extract regex, or stringify  */
		    if (SvGMAGICAL(ret))
			ret = sv_mortalcopy(ret);
		    if (!SvAMAGIC(ret)) {
			SV *sv = ret;
			if (SvROK(sv))
			    sv = SvRV(sv);
			if (SvTYPE(sv) == SVt_REGEXP)
			    re_sv = (REGEXP*) sv;
			else if (SvSMAGICAL(ret)) {
			    MAGIC *mg = mg_find(ret, PERL_MAGIC_qr);
			    if (mg)
				re_sv = (REGEXP *) mg->mg_obj;
			}

			/* force any undef warnings here */
			if (!re_sv && !SvPOK(ret) && !SvNIOK(ret)) {
			    ret = sv_mortalcopy(ret);
			    (void) SvPV_force_nolen(ret);
			}
		    }

		}

		/* *** Note that at this point we don't restore
		 * PL_comppad, (or pop the CxSUB) on the assumption it may
		 * be used again soon. This is safe as long as nothing
		 * in the regexp code uses the pad ! */
		PL_op = oop;
		PL_curcop = ocurcop;
                regcp_restore(rex, ST.lastcp, &maxopenparen);
                PL_curpm_under = PL_curpm;
                PL_curpm = PL_reg_curpm;

		if (logical != 2) {
                    PUSH_STATE_GOTO(EVAL_B, next, locinput);
		    /* NOTREACHED */
                }
	    }

		/* only /(??{})/  from now on */
		logical = 0;
		{
		    /* extract RE object from returned value; compiling if
		     * necessary */

		    if (re_sv) {
			re_sv = reg_temp_copy(NULL, re_sv);
		    }
		    else {
			U32 pm_flags = 0;

			if (SvUTF8(ret) && IN_BYTES) {
			    /* In use 'bytes': make a copy of the octet
			     * sequence, but without the flag on */
			    STRLEN len;
			    const char *const p = SvPV(ret, len);
			    ret = newSVpvn_flags(p, len, SVs_TEMP);
			}
			if (rex->intflags & PREGf_USE_RE_EVAL)
			    pm_flags |= PMf_USE_RE_EVAL;

			/* if we got here, it should be an engine which
			 * supports compiling code blocks and stuff */
			assert(rex->engine && rex->engine->op_comp);
                        assert(!(scan->flags & ~RXf_PMf_COMPILETIME));
			re_sv = rex->engine->op_comp(aTHX_ &ret, 1, NULL,
				    rex->engine, NULL, NULL,
                                    /* copy /msixn etc to inner pattern */
                                    ARG2L(scan),
                                    pm_flags);

			if (!(SvFLAGS(ret)
			      & (SVs_TEMP | SVs_GMG | SVf_ROK))
			 && (!SvPADTMP(ret) || SvREADONLY(ret))) {
			    /* This isn't a first class regexp. Instead, it's
			       caching a regexp onto an existing, Perl visible
			       scalar.  */
			    sv_magic(ret, MUTABLE_SV(re_sv), PERL_MAGIC_qr, 0, 0);
			}
		    }
		    SAVEFREESV(re_sv);
		    re = ReANY(re_sv);
		}
                RXp_MATCH_COPIED_off(re);
                re->subbeg = rex->subbeg;
                re->sublen = rex->sublen;
                re->suboffset = rex->suboffset;
                re->subcoffset = rex->subcoffset;
                re->lastparen = 0;
                re->lastcloseparen = 0;
		rei = RXi_GET(re);
                DEBUG_EXECUTE_r(
                    debug_start_match(re_sv, utf8_target, locinput,
                                    reginfo->strend, "EVAL/GOSUB: Matching embedded");
		);		
		startpoint = rei->program + 1;
                EVAL_CLOSE_PAREN_CLEAR(st); /* ST.close_paren = 0;
                                             * close_paren only for GOSUB */
                ST.prev_recurse_locinput= NULL; /* only used for GOSUB */
                /* Save all the seen positions so far. */
                ST.cp = regcppush(rex, 0, maxopenparen);
                REGCP_SET(ST.lastcp);
                /* and set maxopenparen to 0, since we are starting a "fresh" match */
                maxopenparen = 0;
                /* run the pattern returned from (??{...}) */

              eval_recurse_doit: /* Share code with GOSUB below this line
                            * At this point we expect the stack context to be
                            * set up correctly */

                /* invalidate the S-L poscache. We're now executing a
                 * different set of WHILEM ops (and their associated
                 * indexes) against the same string, so the bits in the
                 * cache are meaningless. Setting maxiter to zero forces
                 * the cache to be invalidated and zeroed before reuse.
		 * XXX This is too dramatic a measure. Ideally we should
                 * save the old cache and restore when running the outer
                 * pattern again */
		reginfo->poscache_maxiter = 0;

                /* the new regexp might have a different is_utf8_pat than we do */
                is_utf8_pat = reginfo->is_utf8_pat = cBOOL(RX_UTF8(re_sv));

		ST.prev_rex = rex_sv;
		ST.prev_curlyx = cur_curlyx;
		rex_sv = re_sv;
		SET_reg_curpm(rex_sv);
		rex = re;
		rexi = rei;
		cur_curlyx = NULL;
		ST.B = next;
		ST.prev_eval = cur_eval;
		cur_eval = st;
		/* now continue from first node in postoned RE */
		PUSH_YES_STATE_GOTO(EVAL_postponed_AB, startpoint, locinput);
		NOT_REACHED; /* NOTREACHED */
	}

	case EVAL_postponed_AB: /* cleanup after a successful (??{A})B */
            /* note: this is called twice; first after popping B, then A */
            DEBUG_STACK_r({
                Perl_re_exec_indentf( aTHX_  "EVAL_AB cur_eval=%p prev_eval=%p\n",
                    depth, cur_eval, ST.prev_eval);
            });

#define SET_RECURSE_LOCINPUT(STR,VAL)\
            if ( cur_eval && CUR_EVAL.close_paren ) {\
                DEBUG_STACK_r({ \
                    Perl_re_exec_indentf( aTHX_  STR " GOSUB%d ce=%p recurse_locinput=%p\n",\
                        depth,    \
                        CUR_EVAL.close_paren - 1,\
                        cur_eval, \
                        VAL);     \
                });               \
                rex->recurse_locinput[CUR_EVAL.close_paren - 1] = VAL;\
            }

            SET_RECURSE_LOCINPUT("EVAL_AB[before]", CUR_EVAL.prev_recurse_locinput);

	    rex_sv = ST.prev_rex;
            is_utf8_pat = reginfo->is_utf8_pat = cBOOL(RX_UTF8(rex_sv));
	    SET_reg_curpm(rex_sv);
	    rex = ReANY(rex_sv);
	    rexi = RXi_GET(rex);
            {
                /* preserve $^R across LEAVE's. See Bug 121070. */
                SV *save_sv= GvSV(PL_replgv);
                SvREFCNT_inc(save_sv);
                regcpblow(ST.cp); /* LEAVE in disguise */
                sv_setsv(GvSV(PL_replgv), save_sv);
                SvREFCNT_dec(save_sv);
            }
	    cur_eval = ST.prev_eval;
	    cur_curlyx = ST.prev_curlyx;

	    /* Invalidate cache. See "invalidate" comment above. */
	    reginfo->poscache_maxiter = 0;
            if ( nochange_depth )
	        nochange_depth--;

            SET_RECURSE_LOCINPUT("EVAL_AB[after]", cur_eval->locinput);
	    sayYES;


	case EVAL_B_fail: /* unsuccessful B in (?{...})B */
	    REGCP_UNWIND(ST.lastcp);
            sayNO;

	case EVAL_postponed_AB_fail: /* unsuccessfully ran A or B in (??{A})B */
	    /* note: this is called twice; first after popping B, then A */
            DEBUG_STACK_r({
                Perl_re_exec_indentf( aTHX_  "EVAL_AB_fail cur_eval=%p prev_eval=%p\n",
                    depth, cur_eval, ST.prev_eval);
            });

            SET_RECURSE_LOCINPUT("EVAL_AB_fail[before]", CUR_EVAL.prev_recurse_locinput);

	    rex_sv = ST.prev_rex;
            is_utf8_pat = reginfo->is_utf8_pat = cBOOL(RX_UTF8(rex_sv));
	    SET_reg_curpm(rex_sv);
	    rex = ReANY(rex_sv);
	    rexi = RXi_GET(rex); 

	    REGCP_UNWIND(ST.lastcp);
            regcppop(rex, &maxopenparen);
	    cur_eval = ST.prev_eval;
	    cur_curlyx = ST.prev_curlyx;

	    /* Invalidate cache. See "invalidate" comment above. */
	    reginfo->poscache_maxiter = 0;
	    if ( nochange_depth )
	        nochange_depth--;

            SET_RECURSE_LOCINPUT("EVAL_AB_fail[after]", cur_eval->locinput);
            sayNO_SILENT;
#undef ST

	case OPEN: /*  (  */
	    n = ARG(scan);  /* which paren pair */
	    rex->offs[n].start_tmp = locinput - reginfo->strbeg;
	    if (n > maxopenparen)
		maxopenparen = n;
            DEBUG_BUFFERS_r(Perl_re_exec_indentf( aTHX_
		"OPEN: rex=0x%" UVxf " offs=0x%" UVxf ": \\%" UVuf ": set %" IVdf " tmp; maxopenparen=%" UVuf "\n",
                depth,
		PTR2UV(rex),
		PTR2UV(rex->offs),
		(UV)n,
		(IV)rex->offs[n].start_tmp,
		(UV)maxopenparen
	    ));
            lastopen = n;
	    break;

        case SROPEN: /*  (*SCRIPT_RUN:  */
            script_run_begin = (U8 *) locinput;
            break;

/* XXX really need to log other places start/end are set too */
#define CLOSE_CAPTURE                                                      \
    rex->offs[n].start = rex->offs[n].start_tmp;                           \
    rex->offs[n].end = locinput - reginfo->strbeg;                         \
    DEBUG_BUFFERS_r(Perl_re_exec_indentf( aTHX_                            \
        "CLOSE: rex=0x%" UVxf " offs=0x%" UVxf ": \\%" UVuf ": set %" IVdf "..%" IVdf "\n", \
        depth,                                                             \
        PTR2UV(rex),                                                       \
        PTR2UV(rex->offs),                                                 \
        (UV)n,                                                             \
        (IV)rex->offs[n].start,                                            \
        (IV)rex->offs[n].end                                               \
    ))

	case CLOSE:  /*  )  */
	    n = ARG(scan);  /* which paren pair */
	    CLOSE_CAPTURE;
	    if (n > rex->lastparen)
		rex->lastparen = n;
	    rex->lastcloseparen = n;
            if ( EVAL_CLOSE_PAREN_IS( cur_eval, n ) )
	        goto fake_end;

	    break;

        case SRCLOSE:  /*  (*SCRIPT_RUN: ... )   */

            if (! isSCRIPT_RUN(script_run_begin, (U8 *) locinput, utf8_target))
            {
                sayNO;
            }

            break;


        case ACCEPT:  /*  (*ACCEPT)  */
            if (scan->flags)
                sv_yes_mark = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
            if (ARG2L(scan)){
                regnode *cursor;
                for (cursor=scan;
                     cursor && OP(cursor)!=END; 
                     cursor=regnext(cursor)) 
                {
                    if ( OP(cursor)==CLOSE ){
                        n = ARG(cursor);
                        if ( n <= lastopen ) {
			    CLOSE_CAPTURE;
                            if (n > rex->lastparen)
                                rex->lastparen = n;
                            rex->lastcloseparen = n;
                            if ( n == ARG(scan) || EVAL_CLOSE_PAREN_IS(cur_eval, n) )
                                break;
                        }
                    }
                }
            }
	    goto fake_end;
	    /* NOTREACHED */

	case GROUPP:  /*  (?(1))  */
	    n = ARG(scan);  /* which paren pair */
	    sw = cBOOL(rex->lastparen >= n && rex->offs[n].end != -1);
	    break;

	case NGROUPP:  /*  (?(<name>))  */
	    /* reg_check_named_buff_matched returns 0 for no match */
	    sw = cBOOL(0 < reg_check_named_buff_matched(rex,scan));
	    break;

        case INSUBP:   /*  (?(R))  */
            n = ARG(scan);
            /* this does not need to use EVAL_CLOSE_PAREN macros, as the arg
             * of SCAN is already set up as matches a eval.close_paren */
            sw = cur_eval && (n == 0 || CUR_EVAL.close_paren == n);
            break;

        case DEFINEP:  /*  (?(DEFINE))  */
            sw = 0;
            break;

	case IFTHEN:   /*  (?(cond)A|B)  */
	    reginfo->poscache_iter = reginfo->poscache_maxiter; /* Void cache */
	    if (sw)
		next = NEXTOPER(NEXTOPER(scan));
	    else {
		next = scan + ARG(scan);
		if (OP(next) == IFTHEN) /* Fake one. */
		    next = NEXTOPER(NEXTOPER(next));
	    }
	    break;

	case LOGICAL:  /* modifier for EVAL and IFMATCH */
	    logical = scan->flags;
	    break;

/*******************************************************************

The CURLYX/WHILEM pair of ops handle the most generic case of the /A*B/
pattern, where A and B are subpatterns. (For simple A, CURLYM or
STAR/PLUS/CURLY/CURLYN are used instead.)

A*B is compiled as <CURLYX><A><WHILEM><B>

On entry to the subpattern, CURLYX is called. This pushes a CURLYX
state, which contains the current count, initialised to -1. It also sets
cur_curlyx to point to this state, with any previous value saved in the
state block.

CURLYX then jumps straight to the WHILEM op, rather than executing A,
since the pattern may possibly match zero times (i.e. it's a while {} loop
rather than a do {} while loop).

Each entry to WHILEM represents a successful match of A. The count in the
CURLYX block is incremented, another WHILEM state is pushed, and execution
passes to A or B depending on greediness and the current count.

For example, if matching against the string a1a2a3b (where the aN are
substrings that match /A/), then the match progresses as follows: (the
pushed states are interspersed with the bits of strings matched so far):

    <CURLYX cnt=-1>
    <CURLYX cnt=0><WHILEM>
    <CURLYX cnt=1><WHILEM> a1 <WHILEM>
    <CURLYX cnt=2><WHILEM> a1 <WHILEM> a2 <WHILEM>
    <CURLYX cnt=3><WHILEM> a1 <WHILEM> a2 <WHILEM> a3 <WHILEM>
    <CURLYX cnt=3><WHILEM> a1 <WHILEM> a2 <WHILEM> a3 <WHILEM> b

(Contrast this with something like CURLYM, which maintains only a single
backtrack state:

    <CURLYM cnt=0> a1
    a1 <CURLYM cnt=1> a2
    a1 a2 <CURLYM cnt=2> a3
    a1 a2 a3 <CURLYM cnt=3> b
)

Each WHILEM state block marks a point to backtrack to upon partial failure
of A or B, and also contains some minor state data related to that
iteration.  The CURLYX block, pointed to by cur_curlyx, contains the
overall state, such as the count, and pointers to the A and B ops.

This is complicated slightly by nested CURLYX/WHILEM's. Since cur_curlyx
must always point to the *current* CURLYX block, the rules are:

When executing CURLYX, save the old cur_curlyx in the CURLYX state block,
and set cur_curlyx to point the new block.

When popping the CURLYX block after a successful or unsuccessful match,
restore the previous cur_curlyx.

When WHILEM is about to execute B, save the current cur_curlyx, and set it
to the outer one saved in the CURLYX block.

When popping the WHILEM block after a successful or unsuccessful B match,
restore the previous cur_curlyx.

Here's an example for the pattern (AI* BI)*BO
I and O refer to inner and outer, C and W refer to CURLYX and WHILEM:

cur_
curlyx backtrack stack
------ ---------------
NULL   
CO     <CO prev=NULL> <WO>
CI     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai 
CO     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai <WI prev=CI> bi 
NULL   <CO prev=NULL> <WO> <CI prev=CO> <WI> ai <WI prev=CI> bi <WO prev=CO> bo

At this point the pattern succeeds, and we work back down the stack to
clean up, restoring as we go:

CO     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai <WI prev=CI> bi 
CI     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai 
CO     <CO prev=NULL> <WO>
NULL   

*******************************************************************/

#define ST st->u.curlyx

	case CURLYX:    /* start of /A*B/  (for complex A) */
	{
	    /* No need to save/restore up to this paren */
	    I32 parenfloor = scan->flags;
	    
	    assert(next); /* keep Coverity happy */
	    if (OP(PREVOPER(next)) == NOTHING) /* LONGJMP */
		next += ARG(next);

	    /* XXXX Probably it is better to teach regpush to support
	       parenfloor > maxopenparen ... */
	    if (parenfloor > (I32)rex->lastparen)
		parenfloor = rex->lastparen; /* Pessimization... */

	    ST.prev_curlyx= cur_curlyx;
	    cur_curlyx = st;
	    ST.cp = PL_savestack_ix;

	    /* these fields contain the state of the current curly.
	     * they are accessed by subsequent WHILEMs */
	    ST.parenfloor = parenfloor;
	    ST.me = scan;
	    ST.B = next;
	    ST.minmod = minmod;
	    minmod = 0;
	    ST.count = -1;	/* this will be updated by WHILEM */
	    ST.lastloc = NULL;  /* this will be updated by WHILEM */

	    PUSH_YES_STATE_GOTO(CURLYX_end, PREVOPER(next), locinput);
	    NOT_REACHED; /* NOTREACHED */
	}

	case CURLYX_end: /* just finished matching all of A*B */
	    cur_curlyx = ST.prev_curlyx;
	    sayYES;
	    NOT_REACHED; /* NOTREACHED */

	case CURLYX_end_fail: /* just failed to match all of A*B */
	    regcpblow(ST.cp);
	    cur_curlyx = ST.prev_curlyx;
	    sayNO;
	    NOT_REACHED; /* NOTREACHED */


#undef ST
#define ST st->u.whilem

	case WHILEM:     /* just matched an A in /A*B/  (for complex A) */
	{
	    /* see the discussion above about CURLYX/WHILEM */
	    I32 n;
	    int min, max;
	    regnode *A;

	    assert(cur_curlyx); /* keep Coverity happy */

	    min = ARG1(cur_curlyx->u.curlyx.me);
	    max = ARG2(cur_curlyx->u.curlyx.me);
	    A = NEXTOPER(cur_curlyx->u.curlyx.me) + EXTRA_STEP_2ARGS;
	    n = ++cur_curlyx->u.curlyx.count; /* how many A's matched */
	    ST.save_lastloc = cur_curlyx->u.curlyx.lastloc;
	    ST.cache_offset = 0;
	    ST.cache_mask = 0;
	    

            DEBUG_EXECUTE_r( Perl_re_exec_indentf( aTHX_  "WHILEM: matched %ld out of %d..%d\n",
                  depth, (long)n, min, max)
	    );

	    /* First just match a string of min A's. */

	    if (n < min) {
                ST.cp = regcppush(rex, cur_curlyx->u.curlyx.parenfloor, maxopenparen);
		cur_curlyx->u.curlyx.lastloc = locinput;
		REGCP_SET(ST.lastcp);

		PUSH_STATE_GOTO(WHILEM_A_pre, A, locinput);
		NOT_REACHED; /* NOTREACHED */
	    }

	    /* If degenerate A matches "", assume A done. */

	    if (locinput == cur_curlyx->u.curlyx.lastloc) {
                DEBUG_EXECUTE_r( Perl_re_exec_indentf( aTHX_  "WHILEM: empty match detected, trying continuation...\n",
                   depth)
		);
		goto do_whilem_B_max;
	    }

	    /* super-linear cache processing.
             *
             * The idea here is that for certain types of CURLYX/WHILEM -
             * principally those whose upper bound is infinity (and
             * excluding regexes that have things like \1 and other very
             * non-regular expresssiony things), then if a pattern like
             * /....A*.../ fails and we backtrack to the WHILEM, then we
             * make a note that this particular WHILEM op was at string
             * position 47 (say) when the rest of pattern failed. Then, if
             * we ever find ourselves back at that WHILEM, and at string
             * position 47 again, we can just fail immediately rather than
             * running the rest of the pattern again.
             *
             * This is very handy when patterns start to go
             * 'super-linear', like in (a+)*(a+)*(a+)*, where you end up
             * with a combinatorial explosion of backtracking.
             *
             * The cache is implemented as a bit array, with one bit per
             * string byte position per WHILEM op (up to 16) - so its
             * between 0.25 and 2x the string size.
             *
             * To avoid allocating a poscache buffer every time, we do an
             * initially countdown; only after we have  executed a WHILEM
             * op (string-length x #WHILEMs) times do we allocate the
             * cache.
             *
             * The top 4 bits of scan->flags byte say how many different
             * relevant CURLLYX/WHILEM op pairs there are, while the
             * bottom 4-bits is the identifying index number of this
             * WHILEM.
             */

	    if (scan->flags) {

		if (!reginfo->poscache_maxiter) {
		    /* start the countdown: Postpone detection until we
		     * know the match is not *that* much linear. */
		    reginfo->poscache_maxiter
                        =    (reginfo->strend - reginfo->strbeg + 1)
                           * (scan->flags>>4);
		    /* possible overflow for long strings and many CURLYX's */
		    if (reginfo->poscache_maxiter < 0)
			reginfo->poscache_maxiter = I32_MAX;
		    reginfo->poscache_iter = reginfo->poscache_maxiter;
		}

		if (reginfo->poscache_iter-- == 0) {
		    /* initialise cache */
		    const SSize_t size = (reginfo->poscache_maxiter + 7)/8;
                    regmatch_info_aux *const aux = reginfo->info_aux;
		    if (aux->poscache) {
			if ((SSize_t)reginfo->poscache_size < size) {
			    Renew(aux->poscache, size, char);
			    reginfo->poscache_size = size;
			}
			Zero(aux->poscache, size, char);
		    }
		    else {
			reginfo->poscache_size = size;
			Newxz(aux->poscache, size, char);
		    }
                    DEBUG_EXECUTE_r( Perl_re_printf( aTHX_
      "%sWHILEM: Detected a super-linear match, switching on caching%s...\n",
			      PL_colors[4], PL_colors[5])
		    );
		}

		if (reginfo->poscache_iter < 0) {
		    /* have we already failed at this position? */
		    SSize_t offset, mask;

                    reginfo->poscache_iter = -1; /* stop eventual underflow */
		    offset  = (scan->flags & 0xf) - 1
                                +   (locinput - reginfo->strbeg)
                                  * (scan->flags>>4);
		    mask    = 1 << (offset % 8);
		    offset /= 8;
		    if (reginfo->info_aux->poscache[offset] & mask) {
                        DEBUG_EXECUTE_r( Perl_re_exec_indentf( aTHX_  "WHILEM: (cache) already tried at this position...\n",
                            depth)
			);
                        cur_curlyx->u.curlyx.count--;
			sayNO; /* cache records failure */
		    }
		    ST.cache_offset = offset;
		    ST.cache_mask   = mask;
		}
	    }

	    /* Prefer B over A for minimal matching. */

	    if (cur_curlyx->u.curlyx.minmod) {
		ST.save_curlyx = cur_curlyx;
		cur_curlyx = cur_curlyx->u.curlyx.prev_curlyx;
		PUSH_YES_STATE_GOTO(WHILEM_B_min, ST.save_curlyx->u.curlyx.B,
                                    locinput);
		NOT_REACHED; /* NOTREACHED */
	    }

	    /* Prefer A over B for maximal matching. */

	    if (n < max) { /* More greed allowed? */
                ST.cp = regcppush(rex, cur_curlyx->u.curlyx.parenfloor,
                            maxopenparen);
		cur_curlyx->u.curlyx.lastloc = locinput;
		REGCP_SET(ST.lastcp);
		PUSH_STATE_GOTO(WHILEM_A_max, A, locinput);
		NOT_REACHED; /* NOTREACHED */
	    }
	    goto do_whilem_B_max;
	}
	NOT_REACHED; /* NOTREACHED */

	case WHILEM_B_min: /* just matched B in a minimal match */
	case WHILEM_B_max: /* just matched B in a maximal match */
	    cur_curlyx = ST.save_curlyx;
	    sayYES;
	    NOT_REACHED; /* NOTREACHED */

	case WHILEM_B_max_fail: /* just failed to match B in a maximal match */
	    cur_curlyx = ST.save_curlyx;
	    cur_curlyx->u.curlyx.lastloc = ST.save_lastloc;
	    cur_curlyx->u.curlyx.count--;
	    CACHEsayNO;
	    NOT_REACHED; /* NOTREACHED */

	case WHILEM_A_min_fail: /* just failed to match A in a minimal match */
	    /* FALLTHROUGH */
	case WHILEM_A_pre_fail: /* just failed to match even minimal A */
	    REGCP_UNWIND(ST.lastcp);
            regcppop(rex, &maxopenparen);
	    cur_curlyx->u.curlyx.lastloc = ST.save_lastloc;
	    cur_curlyx->u.curlyx.count--;
	    CACHEsayNO;
	    NOT_REACHED; /* NOTREACHED */

	case WHILEM_A_max_fail: /* just failed to match A in a maximal match */
	    REGCP_UNWIND(ST.lastcp);
            regcppop(rex, &maxopenparen); /* Restore some previous $<digit>s? */
            DEBUG_EXECUTE_r(Perl_re_exec_indentf( aTHX_  "WHILEM: failed, trying continuation...\n",
                depth)
	    );
	  do_whilem_B_max:
	    if (cur_curlyx->u.curlyx.count >= REG_INFTY
		&& ckWARN(WARN_REGEXP)
		&& !reginfo->warned)
	    {
                reginfo->warned	= TRUE;
		Perl_warner(aTHX_ packWARN(WARN_REGEXP),
		     "Complex regular subexpression recursion limit (%d) "
		     "exceeded",
		     REG_INFTY - 1);
	    }

	    /* now try B */
	    ST.save_curlyx = cur_curlyx;
	    cur_curlyx = cur_curlyx->u.curlyx.prev_curlyx;
	    PUSH_YES_STATE_GOTO(WHILEM_B_max, ST.save_curlyx->u.curlyx.B,
                                locinput);
	    NOT_REACHED; /* NOTREACHED */

	case WHILEM_B_min_fail: /* just failed to match B in a minimal match */
	    cur_curlyx = ST.save_curlyx;

	    if (cur_curlyx->u.curlyx.count >= /*max*/ARG2(cur_curlyx->u.curlyx.me)) {
		/* Maximum greed exceeded */
		if (cur_curlyx->u.curlyx.count >= REG_INFTY
		    && ckWARN(WARN_REGEXP)
                    && !reginfo->warned)
		{
                    reginfo->warned	= TRUE;
		    Perl_warner(aTHX_ packWARN(WARN_REGEXP),
			"Complex regular subexpression recursion "
			"limit (%d) exceeded",
			REG_INFTY - 1);
		}
		cur_curlyx->u.curlyx.count--;
		CACHEsayNO;
	    }

            DEBUG_EXECUTE_r(Perl_re_exec_indentf( aTHX_  "WHILEM: B min fail: trying longer...\n", depth)
	    );
	    /* Try grabbing another A and see if it helps. */
	    cur_curlyx->u.curlyx.lastloc = locinput;
            ST.cp = regcppush(rex, cur_curlyx->u.curlyx.parenfloor,
                            maxopenparen);
	    REGCP_SET(ST.lastcp);
	    PUSH_STATE_GOTO(WHILEM_A_min,
		/*A*/ NEXTOPER(ST.save_curlyx->u.curlyx.me) + EXTRA_STEP_2ARGS,
                locinput);
	    NOT_REACHED; /* NOTREACHED */

#undef  ST
#define ST st->u.branch

	case BRANCHJ:	    /*  /(...|A|...)/ with long next pointer */
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    scan = NEXTOPER(scan);
	    /* FALLTHROUGH */

	case BRANCH:	    /*  /(...|A|...)/ */
	    scan = NEXTOPER(scan); /* scan now points to inner node */
	    ST.lastparen = rex->lastparen;
	    ST.lastcloseparen = rex->lastcloseparen;
	    ST.next_branch = next;
	    REGCP_SET(ST.cp);

	    /* Now go into the branch */
	    if (has_cutgroup) {
	        PUSH_YES_STATE_GOTO(BRANCH_next, scan, locinput);
	    } else {
	        PUSH_STATE_GOTO(BRANCH_next, scan, locinput);
	    }
	    NOT_REACHED; /* NOTREACHED */

        case CUTGROUP:  /*  /(*THEN)/  */
            sv_yes_mark = st->u.mark.mark_name = scan->flags
                ? MUTABLE_SV(rexi->data->data[ ARG( scan ) ])
                : NULL;
            PUSH_STATE_GOTO(CUTGROUP_next, next, locinput);
            NOT_REACHED; /* NOTREACHED */

        case CUTGROUP_next_fail:
            do_cutgroup = 1;
            no_final = 1;
            if (st->u.mark.mark_name)
                sv_commit = st->u.mark.mark_name;
            sayNO;	    
            NOT_REACHED; /* NOTREACHED */

        case BRANCH_next:
            sayYES;
            NOT_REACHED; /* NOTREACHED */

	case BRANCH_next_fail: /* that branch failed; try the next, if any */
	    if (do_cutgroup) {
	        do_cutgroup = 0;
	        no_final = 0;
	    }
	    REGCP_UNWIND(ST.cp);
            UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
	    scan = ST.next_branch;
	    /* no more branches? */
	    if (!scan || (OP(scan) != BRANCH && OP(scan) != BRANCHJ)) {
	        DEBUG_EXECUTE_r({
                    Perl_re_exec_indentf( aTHX_  "%sBRANCH failed...%s\n",
                        depth,
			PL_colors[4],
			PL_colors[5] );
		});
		sayNO_SILENT;
            }
	    continue; /* execute next BRANCH[J] op */
            /* NOTREACHED */
    
	case MINMOD: /* next op will be non-greedy, e.g. A*?  */
	    minmod = 1;
	    break;

#undef  ST
#define ST st->u.curlym

	case CURLYM:	/* /A{m,n}B/ where A is fixed-length */

	    /* This is an optimisation of CURLYX that enables us to push
	     * only a single backtracking state, no matter how many matches
	     * there are in {m,n}. It relies on the pattern being constant
	     * length, with no parens to influence future backrefs
	     */

	    ST.me = scan;
	    scan = NEXTOPER(scan) + NODE_STEP_REGNODE;

	    ST.lastparen      = rex->lastparen;
	    ST.lastcloseparen = rex->lastcloseparen;

	    /* if paren positive, emulate an OPEN/CLOSE around A */
	    if (ST.me->flags) {
		U32 paren = ST.me->flags;
		if (paren > maxopenparen)
		    maxopenparen = paren;
		scan += NEXT_OFF(scan); /* Skip former OPEN. */
	    }
	    ST.A = scan;
	    ST.B = next;
	    ST.alen = 0;
	    ST.count = 0;
	    ST.minmod = minmod;
	    minmod = 0;
	    ST.c1 = CHRTEST_UNINIT;
	    REGCP_SET(ST.cp);

	    if (!(ST.minmod ? ARG1(ST.me) : ARG2(ST.me))) /* min/max */
		goto curlym_do_B;

	  curlym_do_A: /* execute the A in /A{m,n}B/  */
	    PUSH_YES_STATE_GOTO(CURLYM_A, ST.A, locinput); /* match A */
	    NOT_REACHED; /* NOTREACHED */

	case CURLYM_A: /* we've just matched an A */
	    ST.count++;
	    /* after first match, determine A's length: u.curlym.alen */
	    if (ST.count == 1) {
		if (reginfo->is_utf8_target) {
		    char *s = st->locinput;
		    while (s < locinput) {
			ST.alen++;
			s += UTF8SKIP(s);
		    }
		}
		else {
		    ST.alen = locinput - st->locinput;
		}
		if (ST.alen == 0)
		    ST.count = ST.minmod ? ARG1(ST.me) : ARG2(ST.me);
	    }
	    DEBUG_EXECUTE_r(
                Perl_re_exec_indentf( aTHX_  "CURLYM now matched %" IVdf " times, len=%" IVdf "...\n",
                          depth, (IV) ST.count, (IV)ST.alen)
	    );

            if (EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.me->flags))
	        goto fake_end;
	        
	    {
		I32 max = (ST.minmod ? ARG1(ST.me) : ARG2(ST.me));
		if ( max == REG_INFTY || ST.count < max )
		    goto curlym_do_A; /* try to match another A */
	    }
	    goto curlym_do_B; /* try to match B */

	case CURLYM_A_fail: /* just failed to match an A */
	    REGCP_UNWIND(ST.cp);


	    if (ST.minmod || ST.count < ARG1(ST.me) /* min*/ 
                || EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.me->flags))
		sayNO;

	  curlym_do_B: /* execute the B in /A{m,n}B/  */
	    if (ST.c1 == CHRTEST_UNINIT) {
		/* calculate c1 and c2 for possible match of 1st char
		 * following curly */
		ST.c1 = ST.c2 = CHRTEST_VOID;
                assert(ST.B);
		if (HAS_TEXT(ST.B) || JUMPABLE(ST.B)) {
		    regnode *text_node = ST.B;
		    if (! HAS_TEXT(text_node))
			FIND_NEXT_IMPT(text_node);
	            /* this used to be 
	                
	                (HAS_TEXT(text_node) && PL_regkind[OP(text_node)] == EXACT)
	                
	            	But the former is redundant in light of the latter.
	            	
	            	if this changes back then the macro for 
	            	IS_TEXT and friends need to change.
	             */
		    if (PL_regkind[OP(text_node)] == EXACT) {
                        if (! S_setup_EXACTISH_ST_c1_c2(aTHX_
                           text_node, &ST.c1, ST.c1_utf8, &ST.c2, ST.c2_utf8,
                           reginfo))
                        {
                            sayNO;
                        }
		    }
		}
	    }

	    DEBUG_EXECUTE_r(
                Perl_re_exec_indentf( aTHX_  "CURLYM trying tail with matches=%" IVdf "...\n",
                    depth, (IV)ST.count)
		);
	    if (! NEXTCHR_IS_EOS && ST.c1 != CHRTEST_VOID) {
                if (! UTF8_IS_INVARIANT(nextchr) && utf8_target) {
                    if (memNE(locinput, ST.c1_utf8, UTF8SKIP(locinput))
                        && memNE(locinput, ST.c2_utf8, UTF8SKIP(locinput)))
                    {
                        /* simulate B failing */
                        DEBUG_OPTIMISE_r(
                            Perl_re_exec_indentf( aTHX_  "CURLYM Fast bail next target=0x%" UVXf " c1=0x%" UVXf " c2=0x%" UVXf "\n",
                                depth,
                                valid_utf8_to_uvchr((U8 *) locinput, NULL),
                                valid_utf8_to_uvchr(ST.c1_utf8, NULL),
                                valid_utf8_to_uvchr(ST.c2_utf8, NULL))
                        );
                        state_num = CURLYM_B_fail;
                        goto reenter_switch;
                    }
                }
                else if (nextchr != ST.c1 && nextchr != ST.c2) {
                    /* simulate B failing */
                    DEBUG_OPTIMISE_r(
                        Perl_re_exec_indentf( aTHX_  "CURLYM Fast bail next target=0x%X c1=0x%X c2=0x%X\n",
                            depth,
                            (int) nextchr, ST.c1, ST.c2)
                    );
                    state_num = CURLYM_B_fail;
                    goto reenter_switch;
                }
            }

	    if (ST.me->flags) {
		/* emulate CLOSE: mark current A as captured */
		I32 paren = ST.me->flags;
		if (ST.count) {
		    rex->offs[paren].start
			= HOPc(locinput, -ST.alen) - reginfo->strbeg;
		    rex->offs[paren].end = locinput - reginfo->strbeg;
		    if ((U32)paren > rex->lastparen)
			rex->lastparen = paren;
		    rex->lastcloseparen = paren;
		}
		else
		    rex->offs[paren].end = -1;

                if (EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.me->flags))
		{
		    if (ST.count) 
	                goto fake_end;
	            else
	                sayNO;
	        }
	    }
	    
	    PUSH_STATE_GOTO(CURLYM_B, ST.B, locinput); /* match B */
	    NOT_REACHED; /* NOTREACHED */

	case CURLYM_B_fail: /* just failed to match a B */
	    REGCP_UNWIND(ST.cp);
            UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
	    if (ST.minmod) {
		I32 max = ARG2(ST.me);
		if (max != REG_INFTY && ST.count == max)
		    sayNO;
		goto curlym_do_A; /* try to match a further A */
	    }
	    /* backtrack one A */
	    if (ST.count == ARG1(ST.me) /* min */)
		sayNO;
	    ST.count--;
	    SET_locinput(HOPc(locinput, -ST.alen));
	    goto curlym_do_B; /* try to match B */

#undef ST
#define ST st->u.curly

#define CURLY_SETPAREN(paren, success) \
    if (paren) { \
	if (success) { \
	    rex->offs[paren].start = HOPc(locinput, -1) - reginfo->strbeg; \
	    rex->offs[paren].end = locinput - reginfo->strbeg; \
	    if (paren > rex->lastparen) \
		rex->lastparen = paren; \
	    rex->lastcloseparen = paren; \
	} \
	else { \
	    rex->offs[paren].end = -1; \
	    rex->lastparen      = ST.lastparen; \
	    rex->lastcloseparen = ST.lastcloseparen; \
	} \
    }

        case STAR:		/*  /A*B/ where A is width 1 char */
	    ST.paren = 0;
	    ST.min = 0;
	    ST.max = REG_INFTY;
	    scan = NEXTOPER(scan);
	    goto repeat;

        case PLUS:		/*  /A+B/ where A is width 1 char */
	    ST.paren = 0;
	    ST.min = 1;
	    ST.max = REG_INFTY;
	    scan = NEXTOPER(scan);
	    goto repeat;

	case CURLYN:		/*  /(A){m,n}B/ where A is width 1 char */
            ST.paren = scan->flags;	/* Which paren to set */
            ST.lastparen      = rex->lastparen;
	    ST.lastcloseparen = rex->lastcloseparen;
	    if (ST.paren > maxopenparen)
		maxopenparen = ST.paren;
	    ST.min = ARG1(scan);  /* min to match */
	    ST.max = ARG2(scan);  /* max to match */
            if (EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.paren))
            {
	        ST.min=1;
	        ST.max=1;
	    }
            scan = regnext(NEXTOPER(scan) + NODE_STEP_REGNODE);
	    goto repeat;

	case CURLY:		/*  /A{m,n}B/ where A is width 1 char */
	    ST.paren = 0;
	    ST.min = ARG1(scan);  /* min to match */
	    ST.max = ARG2(scan);  /* max to match */
	    scan = NEXTOPER(scan) + NODE_STEP_REGNODE;
	  repeat:
	    /*
	    * Lookahead to avoid useless match attempts
	    * when we know what character comes next.
	    *
	    * Used to only do .*x and .*?x, but now it allows
	    * for )'s, ('s and (?{ ... })'s to be in the way
	    * of the quantifier and the EXACT-like node.  -- japhy
	    */

	    assert(ST.min <= ST.max);
            if (! HAS_TEXT(next) && ! JUMPABLE(next)) {
                ST.c1 = ST.c2 = CHRTEST_VOID;
            }
            else {
		regnode *text_node = next;

		if (! HAS_TEXT(text_node)) 
		    FIND_NEXT_IMPT(text_node);

		if (! HAS_TEXT(text_node))
		    ST.c1 = ST.c2 = CHRTEST_VOID;
		else {
		    if ( PL_regkind[OP(text_node)] != EXACT ) {
			ST.c1 = ST.c2 = CHRTEST_VOID;
		    }
		    else {
                    
                    /*  Currently we only get here when 
                        
                        PL_rekind[OP(text_node)] == EXACT
                    
                        if this changes back then the macro for IS_TEXT and 
                        friends need to change. */
                        if (! S_setup_EXACTISH_ST_c1_c2(aTHX_
                           text_node, &ST.c1, ST.c1_utf8, &ST.c2, ST.c2_utf8,
                           reginfo))
                        {
                            sayNO;
                        }
                    }
		}
	    }

	    ST.A = scan;
	    ST.B = next;
	    if (minmod) {
                char *li = locinput;
		minmod = 0;
		if (ST.min &&
                        regrepeat(rex, &li, ST.A, reginfo, ST.min)
                            < ST.min)
		    sayNO;
                SET_locinput(li);
		ST.count = ST.min;
		REGCP_SET(ST.cp);
		if (ST.c1 == CHRTEST_VOID)
		    goto curly_try_B_min;

		ST.oldloc = locinput;

		/* set ST.maxpos to the furthest point along the
		 * string that could possibly match */
		if  (ST.max == REG_INFTY) {
		    ST.maxpos = reginfo->strend - 1;
		    if (utf8_target)
			while (UTF8_IS_CONTINUATION(*(U8*)ST.maxpos))
			    ST.maxpos--;
		}
		else if (utf8_target) {
		    int m = ST.max - ST.min;
		    for (ST.maxpos = locinput;
			 m >0 && ST.maxpos < reginfo->strend; m--)
			ST.maxpos += UTF8SKIP(ST.maxpos);
		}
		else {
		    ST.maxpos = locinput + ST.max - ST.min;
		    if (ST.maxpos >= reginfo->strend)
			ST.maxpos = reginfo->strend - 1;
		}
		goto curly_try_B_min_known;

	    }
	    else {
                /* avoid taking address of locinput, so it can remain
                 * a register var */
                char *li = locinput;
                ST.count = regrepeat(rex, &li, ST.A, reginfo, ST.max);
		if (ST.count < ST.min)
		    sayNO;
                SET_locinput(li);
		if ((ST.count > ST.min)
		    && (PL_regkind[OP(ST.B)] == EOL) && (OP(ST.B) != MEOL))
		{
		    /* A{m,n} must come at the end of the string, there's
		     * no point in backing off ... */
		    ST.min = ST.count;
		    /* ...except that $ and \Z can match before *and* after
		       newline at the end.  Consider "\n\n" =~ /\n+\Z\n/.
		       We may back off by one in this case. */
		    if (UCHARAT(locinput - 1) == '\n' && OP(ST.B) != EOS)
			ST.min--;
		}
		REGCP_SET(ST.cp);
		goto curly_try_B_max;
	    }
	    NOT_REACHED; /* NOTREACHED */

	case CURLY_B_min_known_fail:
	    /* failed to find B in a non-greedy match where c1,c2 valid */

	    REGCP_UNWIND(ST.cp);
            if (ST.paren) {
                UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
            }
	    /* Couldn't or didn't -- move forward. */
	    ST.oldloc = locinput;
	    if (utf8_target)
		locinput += UTF8SKIP(locinput);
	    else
		locinput++;
	    ST.count++;
	  curly_try_B_min_known:
	     /* find the next place where 'B' could work, then call B */
	    {
		int n;
		if (utf8_target) {
		    n = (ST.oldloc == locinput) ? 0 : 1;
		    if (ST.c1 == ST.c2) {
			/* set n to utf8_distance(oldloc, locinput) */
			while (locinput <= ST.maxpos
                              && memNE(locinput, ST.c1_utf8, UTF8SKIP(locinput)))
                        {
			    locinput += UTF8SKIP(locinput);
			    n++;
			}
		    }
		    else {
			/* set n to utf8_distance(oldloc, locinput) */
			while (locinput <= ST.maxpos
                              && memNE(locinput, ST.c1_utf8, UTF8SKIP(locinput))
                              && memNE(locinput, ST.c2_utf8, UTF8SKIP(locinput)))
                        {
			    locinput += UTF8SKIP(locinput);
			    n++;
			}
		    }
		}
		else {  /* Not utf8_target */
		    if (ST.c1 == ST.c2) {
                        locinput = (char *) memchr(locinput,
                                                   ST.c1,
                                                   ST.maxpos + 1 - locinput);
                        if (! locinput) {
                            locinput = ST.maxpos + 1;
                        }
		    }
                    else {
                        U8 c1_c2_bits_differing = ST.c1 ^ ST.c2;

                        if (! isPOWER_OF_2(c1_c2_bits_differing)) {
                            while (   locinput <= ST.maxpos
                                   && UCHARAT(locinput) != ST.c1
                                   && UCHARAT(locinput) != ST.c2)
                            {
                                locinput++;
                            }
                        }
                        else {
                            /* If c1 and c2 only differ by a single bit, we can
                             * avoid a conditional each time through the loop,
                             * at the expense of a little preliminary setup and
                             * an extra mask each iteration.  By masking out
                             * that bit, we match exactly two characters, c1
                             * and c2, and so we don't have to test for both.
                             * On both ASCII and EBCDIC platforms, most of the
                             * ASCII-range and Latin1-range folded equivalents
                             * differ only in a single bit, so this is actually
                             * the most common case. (e.g. 'A' 0x41 vs 'a'
                             * 0x61). */
                            U8 c1_masked = ST.c1 &~ c1_c2_bits_differing;
                            U8 c1_c2_mask = ~ c1_c2_bits_differing;
                            while (   locinput <= ST.maxpos
                                   && (UCHARAT(locinput) & c1_c2_mask)
                                                                != c1_masked)
                            {
                                locinput++;
                            }
                        }
                    }
		    n = locinput - ST.oldloc;
		}
		if (locinput > ST.maxpos)
		    sayNO;
		if (n) {
                    /* In /a{m,n}b/, ST.oldloc is at "a" x m, locinput is
                     * at b; check that everything between oldloc and
                     * locinput matches */
                    char *li = ST.oldloc;
		    ST.count += n;
                    if (regrepeat(rex, &li, ST.A, reginfo, n) < n)
			sayNO;
                    assert(n == REG_INFTY || locinput == li);
		}
		CURLY_SETPAREN(ST.paren, ST.count);
                if (EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.paren))
		    goto fake_end;
		PUSH_STATE_GOTO(CURLY_B_min_known, ST.B, locinput);
	    }
	    NOT_REACHED; /* NOTREACHED */

	case CURLY_B_min_fail:
	    /* failed to find B in a non-greedy match where c1,c2 invalid */

	    REGCP_UNWIND(ST.cp);
            if (ST.paren) {
                UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
            }
	    /* failed -- move forward one */
            {
                char *li = locinput;
                if (!regrepeat(rex, &li, ST.A, reginfo, 1)) {
                    sayNO;
                }
                locinput = li;
            }
            {
		ST.count++;
		if (ST.count <= ST.max || (ST.max == REG_INFTY &&
			ST.count > 0)) /* count overflow ? */
		{
		  curly_try_B_min:
		    CURLY_SETPAREN(ST.paren, ST.count);
                    if (EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.paren))
                        goto fake_end;
		    PUSH_STATE_GOTO(CURLY_B_min, ST.B, locinput);
		}
	    }
            sayNO;
	    NOT_REACHED; /* NOTREACHED */

          curly_try_B_max:
	    /* a successful greedy match: now try to match B */
            if (EVAL_CLOSE_PAREN_IS_TRUE(cur_eval,(U32)ST.paren))
                goto fake_end;
	    {
		bool could_match = locinput < reginfo->strend;

		/* If it could work, try it. */
                if (ST.c1 != CHRTEST_VOID && could_match) {
                    if (! UTF8_IS_INVARIANT(UCHARAT(locinput)) && utf8_target)
                    {
                        could_match = memEQ(locinput,
                                            ST.c1_utf8,
                                            UTF8SKIP(locinput))
                                    || memEQ(locinput,
                                             ST.c2_utf8,
                                             UTF8SKIP(locinput));
                    }
                    else {
                        could_match = UCHARAT(locinput) == ST.c1
                                      || UCHARAT(locinput) == ST.c2;
                    }
                }
                if (ST.c1 == CHRTEST_VOID || could_match) {
		    CURLY_SETPAREN(ST.paren, ST.count);
		    PUSH_STATE_GOTO(CURLY_B_max, ST.B, locinput);
		    NOT_REACHED; /* NOTREACHED */
		}
	    }
	    /* FALLTHROUGH */

	case CURLY_B_max_fail:
	    /* failed to find B in a greedy match */

	    REGCP_UNWIND(ST.cp);
            if (ST.paren) {
                UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
            }
	    /*  back up. */
	    if (--ST.count < ST.min)
		sayNO;
	    locinput = HOPc(locinput, -1);
	    goto curly_try_B_max;

#undef ST

	case END: /*  last op of main pattern  */
          fake_end:
	    if (cur_eval) {
		/* we've just finished A in /(??{A})B/; now continue with B */
                SET_RECURSE_LOCINPUT("FAKE-END[before]", CUR_EVAL.prev_recurse_locinput);
		st->u.eval.prev_rex = rex_sv;		/* inner */

                /* Save *all* the positions. */
                st->u.eval.cp = regcppush(rex, 0, maxopenparen);
                rex_sv = CUR_EVAL.prev_rex;
		is_utf8_pat = reginfo->is_utf8_pat = cBOOL(RX_UTF8(rex_sv));
		SET_reg_curpm(rex_sv);
		rex = ReANY(rex_sv);
		rexi = RXi_GET(rex);

                st->u.eval.prev_curlyx = cur_curlyx;
                cur_curlyx = CUR_EVAL.prev_curlyx;

		REGCP_SET(st->u.eval.lastcp);

		/* Restore parens of the outer rex without popping the
		 * savestack */
                regcp_restore(rex, CUR_EVAL.lastcp, &maxopenparen);

		st->u.eval.prev_eval = cur_eval;
                cur_eval = CUR_EVAL.prev_eval;
		DEBUG_EXECUTE_r(
                    Perl_re_exec_indentf( aTHX_  "END: EVAL trying tail ... (cur_eval=%p)\n",
                                      depth, cur_eval););
                if ( nochange_depth )
	            nochange_depth--;

                SET_RECURSE_LOCINPUT("FAKE-END[after]", cur_eval->locinput);

                PUSH_YES_STATE_GOTO(EVAL_postponed_AB, st->u.eval.prev_eval->u.eval.B,
                                    locinput); /* match B */
	    }

	    if (locinput < reginfo->till) {
                DEBUG_EXECUTE_r(Perl_re_printf( aTHX_
                                      "%sEND: Match possible, but length=%ld is smaller than requested=%ld, failing!%s\n",
				      PL_colors[4],
				      (long)(locinput - startpos),
				      (long)(reginfo->till - startpos),
				      PL_colors[5]));
               				      
		sayNO_SILENT;		/* Cannot match: too short. */
	    }
	    sayYES;			/* Success! */

	case SUCCEED: /* successful SUSPEND/UNLESSM/IFMATCH/CURLYM */
	    DEBUG_EXECUTE_r(
            Perl_re_exec_indentf( aTHX_  "%sSUCCEED: subpattern success...%s\n",
                depth, PL_colors[4], PL_colors[5]));
	    sayYES;			/* Success! */

#undef  ST
#define ST st->u.ifmatch

        {
            char *newstart;

	case SUSPEND:	/* (?>A) */
	    ST.wanted = 1;
	    newstart = locinput;
	    goto do_ifmatch;	

	case UNLESSM:	/* -ve lookaround: (?!A), or with flags, (?<!A) */
	    ST.wanted = 0;
	    goto ifmatch_trivial_fail_test;

	case IFMATCH:	/* +ve lookaround: (?=A), or with flags, (?<=A) */
	    ST.wanted = 1;
	  ifmatch_trivial_fail_test:
	    if (scan->flags) {
		char * const s = HOPBACKc(locinput, scan->flags);
		if (!s) {
		    /* trivial fail */
		    if (logical) {
			logical = 0;
			sw = 1 - cBOOL(ST.wanted);
		    }
		    else if (ST.wanted)
			sayNO;
		    next = scan + ARG(scan);
		    if (next == scan)
			next = NULL;
		    break;
		}
		newstart = s;
	    }
	    else
		newstart = locinput;

	  do_ifmatch:
	    ST.me = scan;
	    ST.logical = logical;
	    logical = 0; /* XXX: reset state of logical once it has been saved into ST */
	    
	    /* execute body of (?...A) */
	    PUSH_YES_STATE_GOTO(IFMATCH_A, NEXTOPER(NEXTOPER(scan)), newstart);
	    NOT_REACHED; /* NOTREACHED */
        }

	case IFMATCH_A_fail: /* body of (?...A) failed */
	    ST.wanted = !ST.wanted;
	    /* FALLTHROUGH */

	case IFMATCH_A: /* body of (?...A) succeeded */
	    if (ST.logical) {
		sw = cBOOL(ST.wanted);
	    }
	    else if (!ST.wanted)
		sayNO;

	    if (OP(ST.me) != SUSPEND) {
                /* restore old position except for (?>...) */
		locinput = st->locinput;
	    }
	    scan = ST.me + ARG(ST.me);
	    if (scan == ST.me)
		scan = NULL;
	    continue; /* execute B */

#undef ST

	case LONGJMP: /*  alternative with many branches compiles to
                       * (BRANCHJ; EXACT ...; LONGJMP ) x N */
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    break;

	case COMMIT:  /*  (*COMMIT)  */
	    reginfo->cutpoint = reginfo->strend;
	    /* FALLTHROUGH */

	case PRUNE:   /*  (*PRUNE)   */
            if (scan->flags)
	        sv_yes_mark = sv_commit = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
	    PUSH_STATE_GOTO(COMMIT_next, next, locinput);
	    NOT_REACHED; /* NOTREACHED */

	case COMMIT_next_fail:
	    no_final = 1;    
	    /* FALLTHROUGH */	    
            sayNO;
            NOT_REACHED; /* NOTREACHED */

	case OPFAIL:   /* (*FAIL)  */
            if (scan->flags)
                sv_commit = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
            if (logical) {
                /* deal with (?(?!)X|Y) properly,
                 * make sure we trigger the no branch
                 * of the trailing IFTHEN structure*/
                sw= 0;
                break;
            } else {
                sayNO;
            }
	    NOT_REACHED; /* NOTREACHED */

#define ST st->u.mark
        case MARKPOINT: /*  (*MARK:foo)  */
            ST.prev_mark = mark_state;
            ST.mark_name = sv_commit = sv_yes_mark 
                = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
            mark_state = st;
            ST.mark_loc = locinput;
            PUSH_YES_STATE_GOTO(MARKPOINT_next, next, locinput);
            NOT_REACHED; /* NOTREACHED */

        case MARKPOINT_next:
            mark_state = ST.prev_mark;
            sayYES;
            NOT_REACHED; /* NOTREACHED */

        case MARKPOINT_next_fail:
            if (popmark && sv_eq(ST.mark_name,popmark)) 
            {
                if (ST.mark_loc > startpoint)
	            reginfo->cutpoint = HOPBACKc(ST.mark_loc, 1);
                popmark = NULL; /* we found our mark */
                sv_commit = ST.mark_name;

                DEBUG_EXECUTE_r({
                        Perl_re_exec_indentf( aTHX_  "%sMARKPOINT: next fail: setting cutpoint to mark:%" SVf "...%s\n",
                            depth,
		            PL_colors[4], SVfARG(sv_commit), PL_colors[5]);
		});
            }
            mark_state = ST.prev_mark;
            sv_yes_mark = mark_state ? 
                mark_state->u.mark.mark_name : NULL;
            sayNO;
            NOT_REACHED; /* NOTREACHED */

        case SKIP:  /*  (*SKIP)  */
            if (!scan->flags) {
                /* (*SKIP) : if we fail we cut here*/
                ST.mark_name = NULL;
                ST.mark_loc = locinput;
                PUSH_STATE_GOTO(SKIP_next,next, locinput);
            } else {
                /* (*SKIP:NAME) : if there is a (*MARK:NAME) fail where it was, 
                   otherwise do nothing.  Meaning we need to scan 
                 */
                regmatch_state *cur = mark_state;
                SV *find = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
                
                while (cur) {
                    if ( sv_eq( cur->u.mark.mark_name, 
                                find ) ) 
                    {
                        ST.mark_name = find;
                        PUSH_STATE_GOTO( SKIP_next, next, locinput);
                    }
                    cur = cur->u.mark.prev_mark;
                }
            }    
            /* Didn't find our (*MARK:NAME) so ignore this (*SKIP:NAME) */
            break;    

	case SKIP_next_fail:
	    if (ST.mark_name) {
	        /* (*CUT:NAME) - Set up to search for the name as we 
	           collapse the stack*/
	        popmark = ST.mark_name;	   
	    } else {
	        /* (*CUT) - No name, we cut here.*/
	        if (ST.mark_loc > startpoint)
	            reginfo->cutpoint = HOPBACKc(ST.mark_loc, 1);
	        /* but we set sv_commit to latest mark_name if there
	           is one so they can test to see how things lead to this
	           cut */    
                if (mark_state) 
                    sv_commit=mark_state->u.mark.mark_name;	            
            } 
            no_final = 1; 
            sayNO;
            NOT_REACHED; /* NOTREACHED */
#undef ST

        case LNBREAK: /* \R */
            if ((n=is_LNBREAK_safe(locinput, reginfo->strend, utf8_target))) {
                locinput += n;
            } else
                sayNO;
            break;

	default:
	    PerlIO_printf(Perl_error_log, "%" UVxf " %d\n",
			  PTR2UV(scan), OP(scan));
	    Perl_croak(aTHX_ "regexp memory corruption");

        /* this is a point to jump to in order to increment
         * locinput by one character */
          increment_locinput:
            assert(!NEXTCHR_IS_EOS);
            if (utf8_target) {
                locinput += PL_utf8skip[nextchr];
                /* locinput is allowed to go 1 char off the end (signifying
                 * EOS), but not 2+ */
                if (locinput > reginfo->strend)
                    sayNO;
            }
            else
                locinput++;
            break;
	    
	} /* end switch */ 

        /* switch break jumps here */
	scan = next; /* prepare to execute the next op and ... */
	continue;    /* ... jump back to the top, reusing st */
        /* NOTREACHED */

      push_yes_state:
	/* push a state that backtracks on success */
	st->u.yes.prev_yes_state = yes_state;
	yes_state = st;
	/* FALLTHROUGH */
      push_state:
	/* push a new regex state, then continue at scan  */
	{
	    regmatch_state *newst;

	    DEBUG_STACK_r({
	        regmatch_state *cur = st;
	        regmatch_state *curyes = yes_state;
	        U32 i;
	        regmatch_slab *slab = PL_regmatch_slab;
                for (i = 0; i < 3 && i <= depth; cur--,i++) {
                    if (cur < SLAB_FIRST(slab)) {
                	slab = slab->prev;
                	cur = SLAB_LAST(slab);
                    }
                    Perl_re_exec_indentf( aTHX_ "%4s #%-3d %-10s %s\n",
                        depth,
                        i ? "    " : "push",
                        depth - i, PL_reg_name[cur->resume_state],
                        (curyes == cur) ? "yes" : ""
                    );
                    if (curyes == cur)
	                curyes = cur->u.yes.prev_yes_state;
                }
            } else 
                DEBUG_STATE_pp("push")
            );
	    depth++;
	    st->locinput = locinput;
	    newst = st+1; 
	    if (newst >  SLAB_LAST(PL_regmatch_slab))
		newst = S_push_slab(aTHX);
	    PL_regmatch_state = newst;

	    locinput = pushinput;
	    st = newst;
	    continue;
            /* NOTREACHED */
	}
    }
#ifdef SOLARIS_BAD_OPTIMIZER
#  undef PL_charclass
#endif

    /*
    * We get here only if there's trouble -- normally "case END" is
    * the terminating point.
    */
    Perl_croak(aTHX_ "corrupted regexp pointers");
    NOT_REACHED; /* NOTREACHED */

  yes:
    if (yes_state) {
	/* we have successfully completed a subexpression, but we must now
	 * pop to the state marked by yes_state and continue from there */
	assert(st != yes_state);
#ifdef DEBUGGING
	while (st != yes_state) {
	    st--;
	    if (st < SLAB_FIRST(PL_regmatch_slab)) {
		PL_regmatch_slab = PL_regmatch_slab->prev;
		st = SLAB_LAST(PL_regmatch_slab);
	    }
	    DEBUG_STATE_r({
	        if (no_final) {
	            DEBUG_STATE_pp("pop (no final)");        
	        } else {
	            DEBUG_STATE_pp("pop (yes)");
	        }
	    });
	    depth--;
	}
#else
	while (yes_state < SLAB_FIRST(PL_regmatch_slab)
	    || yes_state > SLAB_LAST(PL_regmatch_slab))
	{
	    /* not in this slab, pop slab */
	    depth -= (st - SLAB_FIRST(PL_regmatch_slab) + 1);
	    PL_regmatch_slab = PL_regmatch_slab->prev;
	    st = SLAB_LAST(PL_regmatch_slab);
	}
	depth -= (st - yes_state);
#endif
	st = yes_state;
	yes_state = st->u.yes.prev_yes_state;
	PL_regmatch_state = st;
        
        if (no_final)
            locinput= st->locinput;
	state_num = st->resume_state + no_final;
	goto reenter_switch;
    }

    DEBUG_EXECUTE_r(Perl_re_printf( aTHX_  "%sMatch successful!%s\n",
			  PL_colors[4], PL_colors[5]));

    if (reginfo->info_aux_eval) {
	/* each successfully executed (?{...}) block does the equivalent of
	 *   local $^R = do {...}
	 * When popping the save stack, all these locals would be undone;
	 * bypass this by setting the outermost saved $^R to the latest
	 * value */
        /* I dont know if this is needed or works properly now.
         * see code related to PL_replgv elsewhere in this file.
         * Yves
         */
	if (oreplsv != GvSV(PL_replgv))
	    sv_setsv(oreplsv, GvSV(PL_replgv));
    }
    result = 1;
    goto final_exit;

  no:
    DEBUG_EXECUTE_r(
        Perl_re_exec_indentf( aTHX_  "%sfailed...%s\n",
            depth,
            PL_colors[4], PL_colors[5])
	);

  no_silent:
    if (no_final) {
        if (yes_state) {
            goto yes;
        } else {
            goto final_exit;
        }
    }    
    if (depth) {
	/* there's a previous state to backtrack to */
	st--;
	if (st < SLAB_FIRST(PL_regmatch_slab)) {
	    PL_regmatch_slab = PL_regmatch_slab->prev;
	    st = SLAB_LAST(PL_regmatch_slab);
	}
	PL_regmatch_state = st;
	locinput= st->locinput;

	DEBUG_STATE_pp("pop");
	depth--;
	if (yes_state == st)
	    yes_state = st->u.yes.prev_yes_state;

	state_num = st->resume_state + 1; /* failure = success + 1 */
        PERL_ASYNC_CHECK();
	goto reenter_switch;
    }
    result = 0;

  final_exit:
    if (rex->intflags & PREGf_VERBARG_SEEN) {
        SV *sv_err = get_sv("REGERROR", 1);
        SV *sv_mrk = get_sv("REGMARK", 1);
        if (result) {
            sv_commit = &PL_sv_no;
            if (!sv_yes_mark) 
                sv_yes_mark = &PL_sv_yes;
        } else {
            if (!sv_commit) 
                sv_commit = &PL_sv_yes;
            sv_yes_mark = &PL_sv_no;
        }
        assert(sv_err);
        assert(sv_mrk);
        sv_setsv(sv_err, sv_commit);
        sv_setsv(sv_mrk, sv_yes_mark);
    }


    if (last_pushed_cv) {
	dSP;
        /* see "Some notes about MULTICALL" above */
	POP_MULTICALL;
        PERL_UNUSED_VAR(SP);
    }
    else
        LEAVE_SCOPE(orig_savestack_ix);

    assert(!result ||  locinput - reginfo->strbeg >= 0);
    return result ?  locinput - reginfo->strbeg : -1;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 *
 * What 'simple' means is a node which can be the operand of a quantifier like
 * '+', or {1,3}
 *
 * startposp - pointer a pointer to the start position.  This is updated
 *             to point to the byte following the highest successful
 *             match.
 * p         - the regnode to be repeatedly matched against.
 * reginfo   - struct holding match state, such as strend
 * max       - maximum number of things to match.
 * depth     - (for debugging) backtracking depth.
 */
STATIC I32
S_regrepeat(pTHX_ regexp *prog, char **startposp, const regnode *p,
            regmatch_info *const reginfo, I32 max _pDEPTH)
{
    char *scan;     /* Pointer to current position in target string */
    I32 c;
    char *loceol = reginfo->strend;   /* local version */
    I32 hardcount = 0;  /* How many matches so far */
    bool utf8_target = reginfo->is_utf8_target;
    unsigned int to_complement = 0;  /* Invert the result? */
    UV utf8_flags;
    _char_class_number classnum;

    PERL_ARGS_ASSERT_REGREPEAT;

    scan = *startposp;
    if (max == REG_INFTY)
	max = I32_MAX;
    else if (! utf8_target && loceol - scan > max)
	loceol = scan + max;

    /* Here, for the case of a non-UTF-8 target we have adjusted <loceol> down
     * to the maximum of how far we should go in it (leaving it set to the real
     * end, if the maximum permissible would take us beyond that).  This allows
     * us to make the loop exit condition that we haven't gone past <loceol> to
     * also mean that we haven't exceeded the max permissible count, saving a
     * test each time through the loop.  But it assumes that the OP matches a
     * single byte, which is true for most of the OPs below when applied to a
     * non-UTF-8 target.  Those relatively few OPs that don't have this
     * characteristic will have to compensate.
     *
     * There is no adjustment for UTF-8 targets, as the number of bytes per
     * character varies.  OPs will have to test both that the count is less
     * than the max permissible (using <hardcount> to keep track), and that we
     * are still within the bounds of the string (using <loceol>.  A few OPs
     * match a single byte no matter what the encoding.  They can omit the max
     * test if, for the UTF-8 case, they do the adjustment that was skipped
     * above.
     *
     * Thus, the code above sets things up for the common case; and exceptional
     * cases need extra work; the common case is to make sure <scan> doesn't
     * go past <loceol>, and for UTF-8 to also use <hardcount> to make sure the
     * count doesn't exceed the maximum permissible */

    switch (OP(p)) {
    case REG_ANY:
	if (utf8_target) {
	    while (scan < loceol && hardcount < max && *scan != '\n') {
		scan += UTF8SKIP(scan);
		hardcount++;
	    }
	} else {
            scan = (char *) memchr(scan, '\n', loceol - scan);
            if (! scan) {
                scan = loceol;
            }
	}
	break;
    case SANY:
        if (utf8_target) {
	    while (scan < loceol && hardcount < max) {
	        scan += UTF8SKIP(scan);
		hardcount++;
	    }
	}
	else
	    scan = loceol;
	break;
    case EXACTL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
        if (utf8_target && UTF8_IS_ABOVE_LATIN1(*scan)) {
            _CHECK_AND_OUTPUT_WIDE_LOCALE_UTF8_MSG(scan, loceol);
        }
        /* FALLTHROUGH */
    case EXACT:
        assert(STR_LEN(p) == reginfo->is_utf8_pat ? UTF8SKIP(STRING(p)) : 1);

	c = (U8)*STRING(p);

        /* Can use a simple find if the pattern char to match on is invariant
         * under UTF-8, or both target and pattern aren't UTF-8.  Note that we
         * can use UTF8_IS_INVARIANT() even if the pattern isn't UTF-8, as it's
         * true iff it doesn't matter if the argument is in UTF-8 or not */
        if (UTF8_IS_INVARIANT(c) || (! utf8_target && ! reginfo->is_utf8_pat)) {
            if (utf8_target && loceol - scan > max) {
                /* We didn't adjust <loceol> because is UTF-8, but ok to do so,
                 * since here, to match at all, 1 char == 1 byte */
                loceol = scan + max;
            }
            scan = (char *) find_span_end((U8 *) scan, (U8 *) loceol, (U8) c);
	}
	else if (reginfo->is_utf8_pat) {
            if (utf8_target) {
                STRLEN scan_char_len;

                /* When both target and pattern are UTF-8, we have to do
                 * string EQ */
                while (hardcount < max
                       && scan < loceol
                       && (scan_char_len = UTF8SKIP(scan)) <= STR_LEN(p)
                       && memEQ(scan, STRING(p), scan_char_len))
                {
                    scan += scan_char_len;
                    hardcount++;
                }
            }
            else if (! UTF8_IS_ABOVE_LATIN1(c)) {

                /* Target isn't utf8; convert the character in the UTF-8
                 * pattern to non-UTF8, and do a simple find */
                c = EIGHT_BIT_UTF8_TO_NATIVE(c, *(STRING(p) + 1));
                scan = (char *) find_span_end((U8 *) scan, (U8 *) loceol, (U8) c);
            } /* else pattern char is above Latin1, can't possibly match the
                 non-UTF-8 target */
        }
        else {

            /* Here, the string must be utf8; pattern isn't, and <c> is
             * different in utf8 than not, so can't compare them directly.
             * Outside the loop, find the two utf8 bytes that represent c, and
             * then look for those in sequence in the utf8 string */
	    U8 high = UTF8_TWO_BYTE_HI(c);
	    U8 low = UTF8_TWO_BYTE_LO(c);

	    while (hardcount < max
		    && scan + 1 < loceol
		    && UCHARAT(scan) == high
		    && UCHARAT(scan + 1) == low)
	    {
		scan += 2;
		hardcount++;
	    }
	}
	break;

    case EXACTFAA_NO_TRIE: /* This node only generated for non-utf8 patterns */
        assert(! reginfo->is_utf8_pat);
        /* FALLTHROUGH */
    case EXACTFAA:
        utf8_flags = FOLDEQ_UTF8_NOMIX_ASCII;
	goto do_exactf;

    case EXACTFL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
	utf8_flags = FOLDEQ_LOCALE;
	goto do_exactf;

    case EXACTF:   /* This node only generated for non-utf8 patterns */
        assert(! reginfo->is_utf8_pat);
        utf8_flags = 0;
        goto do_exactf;

    case EXACTFLU8:
        if (! utf8_target) {
            break;
        }
        utf8_flags =  FOLDEQ_LOCALE | FOLDEQ_S2_ALREADY_FOLDED
                                    | FOLDEQ_S2_FOLDS_SANE;
        goto do_exactf;

    case EXACTFU_SS:
    case EXACTFU:
	utf8_flags = reginfo->is_utf8_pat ? FOLDEQ_S2_ALREADY_FOLDED : 0;

      do_exactf: {
        int c1, c2;
        U8 c1_utf8[UTF8_MAXBYTES+1], c2_utf8[UTF8_MAXBYTES+1];

        assert(STR_LEN(p) == reginfo->is_utf8_pat ? UTF8SKIP(STRING(p)) : 1);

        if (S_setup_EXACTISH_ST_c1_c2(aTHX_ p, &c1, c1_utf8, &c2, c2_utf8,
                                        reginfo))
        {
            if (c1 == CHRTEST_VOID) {
                /* Use full Unicode fold matching */
                char *tmpeol = reginfo->strend;
                STRLEN pat_len = reginfo->is_utf8_pat ? UTF8SKIP(STRING(p)) : 1;
                while (hardcount < max
                        && foldEQ_utf8_flags(scan, &tmpeol, 0, utf8_target,
                                             STRING(p), NULL, pat_len,
                                             reginfo->is_utf8_pat, utf8_flags))
                {
                    scan = tmpeol;
                    tmpeol = reginfo->strend;
                    hardcount++;
                }
            }
            else if (utf8_target) {
                if (c1 == c2) {
                    while (scan < loceol
                           && hardcount < max
                           && memEQ(scan, c1_utf8, UTF8SKIP(scan)))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                }
                else {
                    while (scan < loceol
                           && hardcount < max
                           && (memEQ(scan, c1_utf8, UTF8SKIP(scan))
                               || memEQ(scan, c2_utf8, UTF8SKIP(scan))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                }
            }
            else if (c1 == c2) {
                scan = (char *) find_span_end((U8 *) scan, (U8 *) loceol, (U8) c1);
            }
            else {
                /* See comments in regmatch() CURLY_B_min_known_fail.  We avoid
                 * a conditional each time through the loop if the characters
                 * differ only in a single bit, as is the usual situation */
                U8 c1_c2_bits_differing = c1 ^ c2;

                if (isPOWER_OF_2(c1_c2_bits_differing)) {
                    U8 c1_c2_mask = ~ c1_c2_bits_differing;

                    scan = (char *) find_span_end_mask((U8 *) scan,
                                                       (U8 *) loceol,
                                                       c1 & c1_c2_mask,
                                                       c1_c2_mask);
                }
                else {
                    while (    scan < loceol
                           && (UCHARAT(scan) == c1 || UCHARAT(scan) == c2))
                    {
                        scan++;
                    }
                }
            }
	}
	break;
    }
    case ANYOFL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;

        if (ANYOFL_UTF8_LOCALE_REQD(FLAGS(p)) && ! IN_UTF8_CTYPE_LOCALE) {
            Perl_ck_warner(aTHX_ packWARN(WARN_LOCALE), utf8_locale_required);
        }
        /* FALLTHROUGH */
    case ANYOFD:
    case ANYOF:
	if (utf8_target) {
	    while (hardcount < max
                   && scan < loceol
		   && reginclass(prog, p, (U8*)scan, (U8*) loceol, utf8_target))
	    {
		scan += UTF8SKIP(scan);
		hardcount++;
	    }
	}
        else if (ANYOF_FLAGS(p)) {
	    while (scan < loceol
                    && reginclass(prog, p, (U8*)scan, (U8*)scan+1, 0))
		scan++;
        }
        else {
	    while (scan < loceol && ANYOF_BITMAP_TEST(p, *((U8*)scan)))
		scan++;
	}
	break;

    case ANYOFM:
        if (utf8_target && loceol - scan > max) {

            /* We didn't adjust <loceol> at the beginning of this routine
             * because is UTF-8, but it is actually ok to do so, since here, to
             * match, 1 char == 1 byte. */
            loceol = scan + max;
        }

        scan = (char *) find_span_end_mask((U8 *) scan, (U8 *) loceol, (U8) ARG(p), FLAGS(p));
        break;

    case ASCII:
        if (utf8_target && loceol - scan > max) {
            loceol = scan + max;
        }

        scan = find_next_non_ascii(scan, loceol, utf8_target);
	break;

    case NASCII:
	if (utf8_target) {
	    while (     hardcount < max
                   &&   scan < loceol
		   && ! isASCII_utf8_safe(scan, loceol))
	    {
		scan += UTF8SKIP(scan);
		hardcount++;
	    }
	}
        else {
            scan = find_next_ascii(scan, loceol, utf8_target);
	}
	break;

    /* The argument (FLAGS) to all the POSIX node types is the class number */

    case NPOSIXL:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
	if (! utf8_target) {
	    while (scan < loceol && to_complement ^ cBOOL(isFOO_lc(FLAGS(p),
                                                                   *scan)))
            {
		scan++;
            }
	} else {
	    while (hardcount < max && scan < loceol
                   && to_complement ^ cBOOL(isFOO_utf8_lc(FLAGS(p),
                                                                  (U8 *) scan,
                                                                  (U8 *) loceol)))
            {
                scan += UTF8SKIP(scan);
		hardcount++;
	    }
	}
	break;

    case POSIXD:
        if (utf8_target) {
            goto utf8_posix;
        }
        /* FALLTHROUGH */

    case POSIXA:
        if (utf8_target && loceol - scan > max) {

            /* We didn't adjust <loceol> at the beginning of this routine
             * because is UTF-8, but it is actually ok to do so, since here, to
             * match, 1 char == 1 byte. */
            loceol = scan + max;
        }
        while (scan < loceol && _generic_isCC_A((U8) *scan, FLAGS(p))) {
	    scan++;
	}
	break;

    case NPOSIXD:
        if (utf8_target) {
            to_complement = 1;
            goto utf8_posix;
        }
        /* FALLTHROUGH */

    case NPOSIXA:
        if (! utf8_target) {
            while (scan < loceol && ! _generic_isCC_A((U8) *scan, FLAGS(p))) {
                scan++;
            }
        }
        else {

            /* The complement of something that matches only ASCII matches all
             * non-ASCII, plus everything in ASCII that isn't in the class. */
	    while (hardcount < max && scan < loceol
                   && (   ! isASCII_utf8_safe(scan, reginfo->strend)
                       || ! _generic_isCC_A((U8) *scan, FLAGS(p))))
            {
                scan += UTF8SKIP(scan);
		hardcount++;
	    }
        }
        break;

    case NPOSIXU:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXU:
	if (! utf8_target) {
            while (scan < loceol && to_complement
                                ^ cBOOL(_generic_isCC((U8) *scan, FLAGS(p))))
            {
                scan++;
            }
	}
	else {
          utf8_posix:
            classnum = (_char_class_number) FLAGS(p);
            switch (classnum) {
                default:
                    while (   hardcount < max && scan < loceol
                           && to_complement ^ cBOOL(_invlist_contains_cp(
                                              PL_XPosix_ptrs[classnum],
                                              utf8_to_uvchr_buf((U8 *) scan,
                                                                (U8 *) loceol,
                                                                NULL))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                    break;

                    /* For the classes below, the knowledge of how to handle
                     * every code point is compiled in to Perl via a macro.
                     * This code is written for making the loops as tight as
                     * possible.  It could be refactored to save space instead.
                     * */

                case _CC_ENUM_SPACE:
                    while (hardcount < max
                           && scan < loceol
                           && (to_complement
                               ^ cBOOL(isSPACE_utf8_safe(scan, loceol))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                    break;
                case _CC_ENUM_BLANK:
                    while (hardcount < max
                           && scan < loceol
                           && (to_complement
                                ^ cBOOL(isBLANK_utf8_safe(scan, loceol))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                    break;
                case _CC_ENUM_XDIGIT:
                    while (hardcount < max
                           && scan < loceol
                           && (to_complement
                               ^ cBOOL(isXDIGIT_utf8_safe(scan, loceol))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                    break;
                case _CC_ENUM_VERTSPACE:
                    while (hardcount < max
                           && scan < loceol
                           && (to_complement
                               ^ cBOOL(isVERTWS_utf8_safe(scan, loceol))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                    break;
                case _CC_ENUM_CNTRL:
                    while (hardcount < max
                           && scan < loceol
                           && (to_complement
                               ^ cBOOL(isCNTRL_utf8_safe(scan, loceol))))
                    {
                        scan += UTF8SKIP(scan);
                        hardcount++;
                    }
                    break;
            }
	}
        break;

    case LNBREAK:
        if (utf8_target) {
	    while (hardcount < max && scan < loceol &&
                    (c=is_LNBREAK_utf8_safe(scan, loceol))) {
		scan += c;
		hardcount++;
	    }
	} else {
            /* LNBREAK can match one or two latin chars, which is ok, but we
             * have to use hardcount in this situation, and throw away the
             * adjustment to <loceol> done before the switch statement */
            loceol = reginfo->strend;
	    while (scan < loceol && (c=is_LNBREAK_latin1_safe(scan, loceol))) {
		scan+=c;
		hardcount++;
	    }
	}
	break;

    case BOUNDL:
    case NBOUNDL:
        _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
        /* FALLTHROUGH */
    case BOUND:
    case BOUNDA:
    case BOUNDU:
    case EOS:
    case GPOS:
    case KEEPS:
    case NBOUND:
    case NBOUNDA:
    case NBOUNDU:
    case OPFAIL:
    case SBOL:
    case SEOL:
        /* These are all 0 width, so match right here or not at all. */
        break;

    default:
        Perl_croak(aTHX_ "panic: regrepeat() called with unrecognized node type %d='%s'", OP(p), PL_reg_name[OP(p)]);
        NOT_REACHED; /* NOTREACHED */

    }

    if (hardcount)
	c = hardcount;
    else
	c = scan - *startposp;
    *startposp = scan;

    DEBUG_r({
	GET_RE_DEBUG_FLAGS_DECL;
	DEBUG_EXECUTE_r({
	    SV * const prop = sv_newmortal();
            regprop(prog, prop, p, reginfo, NULL);
            Perl_re_exec_indentf( aTHX_  "%s can match %" IVdf " times out of %" IVdf "...\n",
                        depth, SvPVX_const(prop),(IV)c,(IV)max);
	});
    });

    return(c);
}


#if !defined(PERL_IN_XSUB_RE) || defined(PLUGGABLE_RE_EXTENSION)
/*
- regclass_swash - prepare the utf8 swash.  Wraps the shared core version to
create a copy so that changes the caller makes won't change the shared one.
If <altsvp> is non-null, will return NULL in it, for back-compat.
 */
SV *
Perl_regclass_swash(pTHX_ const regexp *prog, const regnode* node, bool doinit, SV** listsvp, SV **altsvp)
{
    PERL_ARGS_ASSERT_REGCLASS_SWASH;

    if (altsvp) {
        *altsvp = NULL;
    }

    return newSVsv(_get_regclass_nonbitmap_data(prog, node, doinit, listsvp, NULL, NULL));
}

#endif /* !defined(PERL_IN_XSUB_RE) || defined(PLUGGABLE_RE_EXTENSION) */

/*
 - reginclass - determine if a character falls into a character class
 
  n is the ANYOF-type regnode
  p is the target string
  p_end points to one byte beyond the end of the target string
  utf8_target tells whether p is in UTF-8.

  Returns true if matched; false otherwise.

  Note that this can be a synthetic start class, a combination of various
  nodes, so things you think might be mutually exclusive, such as locale,
  aren't.  It can match both locale and non-locale

 */

STATIC bool
S_reginclass(pTHX_ regexp * const prog, const regnode * const n, const U8* const p, const U8* const p_end, const bool utf8_target)
{
    dVAR;
    const char flags = ANYOF_FLAGS(n);
    bool match = FALSE;
    UV c = *p;

    PERL_ARGS_ASSERT_REGINCLASS;

    /* If c is not already the code point, get it.  Note that
     * UTF8_IS_INVARIANT() works even if not in UTF-8 */
    if (! UTF8_IS_INVARIANT(c) && utf8_target) {
        STRLEN c_len = 0;
        const U32 utf8n_flags = UTF8_ALLOW_DEFAULT;
	c = utf8n_to_uvchr(p, p_end - p, &c_len, utf8n_flags | UTF8_CHECK_ONLY);
	if (c_len == (STRLEN)-1) {
            _force_out_malformed_utf8_message(p, p_end,
                                              utf8n_flags,
                                              1 /* 1 means die */ );
            NOT_REACHED; /* NOTREACHED */
        }
        if (c > 255 && OP(n) == ANYOFL && ! ANYOFL_UTF8_LOCALE_REQD(flags)) {
            _CHECK_AND_OUTPUT_WIDE_LOCALE_CP_MSG(c);
        }
    }

    /* If this character is potentially in the bitmap, check it */
    if (c < NUM_ANYOF_CODE_POINTS) {
	if (ANYOF_BITMAP_TEST(n, c))
	    match = TRUE;
	else if ((flags
                & ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER)
                  && OP(n) == ANYOFD
		  && ! utf8_target
		  && ! isASCII(c))
	{
	    match = TRUE;
	}
	else if (flags & ANYOF_LOCALE_FLAGS) {
	    if ((flags & ANYOFL_FOLD)
                && c < 256
		&& ANYOF_BITMAP_TEST(n, PL_fold_locale[c]))
            {
                match = TRUE;
            }
            else if (ANYOF_POSIXL_TEST_ANY_SET(n)
                     && c < 256
            ) {

                /* The data structure is arranged so bits 0, 2, 4, ... are set
                 * if the class includes the Posix character class given by
                 * bit/2; and 1, 3, 5, ... are set if the class includes the
                 * complemented Posix class given by int(bit/2).  So we loop
                 * through the bits, each time changing whether we complement
                 * the result or not.  Suppose for the sake of illustration
                 * that bits 0-3 mean respectively, \w, \W, \s, \S.  If bit 0
                 * is set, it means there is a match for this ANYOF node if the
                 * character is in the class given by the expression (0 / 2 = 0
                 * = \w).  If it is in that class, isFOO_lc() will return 1,
                 * and since 'to_complement' is 0, the result will stay TRUE,
                 * and we exit the loop.  Suppose instead that bit 0 is 0, but
                 * bit 1 is 1.  That means there is a match if the character
                 * matches \W.  We won't bother to call isFOO_lc() on bit 0,
                 * but will on bit 1.  On the second iteration 'to_complement'
                 * will be 1, so the exclusive or will reverse things, so we
                 * are testing for \W.  On the third iteration, 'to_complement'
                 * will be 0, and we would be testing for \s; the fourth
                 * iteration would test for \S, etc.
                 *
                 * Note that this code assumes that all the classes are closed
                 * under folding.  For example, if a character matches \w, then
                 * its fold does too; and vice versa.  This should be true for
                 * any well-behaved locale for all the currently defined Posix
                 * classes, except for :lower: and :upper:, which are handled
                 * by the pseudo-class :cased: which matches if either of the
                 * other two does.  To get rid of this assumption, an outer
                 * loop could be used below to iterate over both the source
                 * character, and its fold (if different) */

                int count = 0;
                int to_complement = 0;

                while (count < ANYOF_MAX) {
                    if (ANYOF_POSIXL_TEST(n, count)
                        && to_complement ^ cBOOL(isFOO_lc(count/2, (U8) c)))
                    {
                        match = TRUE;
                        break;
                    }
                    count++;
                    to_complement ^= 1;
                }
	    }
	}
    }


    /* If the bitmap didn't (or couldn't) match, and something outside the
     * bitmap could match, try that. */
    if (!match) {
	if (c >= NUM_ANYOF_CODE_POINTS
            && (flags & ANYOF_MATCHES_ALL_ABOVE_BITMAP))
        {
	    match = TRUE;	/* Everything above the bitmap matches */
	}
            /* Here doesn't match everything above the bitmap.  If there is
             * some information available beyond the bitmap, we may find a
             * match in it.  If so, this is most likely because the code point
             * is outside the bitmap range.  But rarely, it could be because of
             * some other reason.  If so, various flags are set to indicate
             * this possibility.  On ANYOFD nodes, there may be matches that
             * happen only when the target string is UTF-8; or for other node
             * types, because runtime lookup is needed, regardless of the
             * UTF-8ness of the target string.  Finally, under /il, there may
             * be some matches only possible if the locale is a UTF-8 one. */
	else if (    ARG(n) != ANYOF_ONLY_HAS_BITMAP
                 && (   c >= NUM_ANYOF_CODE_POINTS
                     || (   (flags & ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP)
                         && (   UNLIKELY(OP(n) != ANYOFD)
                             || (utf8_target && ! isASCII_uni(c)
#                               if NUM_ANYOF_CODE_POINTS > 256
                                                                 && c < 256
#                               endif
                                )))
                     || (   ANYOFL_SOME_FOLDS_ONLY_IN_UTF8_LOCALE(flags)
                         && IN_UTF8_CTYPE_LOCALE)))
        {
            SV* only_utf8_locale = NULL;
	    SV * const sw = _get_regclass_nonbitmap_data(prog, n, TRUE, 0,
                                                       &only_utf8_locale, NULL);
	    if (sw) {
                U8 utf8_buffer[2];
		U8 * utf8_p;
		if (utf8_target) {
		    utf8_p = (U8 *) p;
		} else { /* Convert to utf8 */
		    utf8_p = utf8_buffer;
                    append_utf8_from_native_byte(*p, &utf8_p);
		    utf8_p = utf8_buffer;
		}

		if (swash_fetch(sw, utf8_p, TRUE)) {
		    match = TRUE;
                }
	    }
            if (! match && only_utf8_locale && IN_UTF8_CTYPE_LOCALE) {
                match = _invlist_contains_cp(only_utf8_locale, c);
            }
	}

        if (UNICODE_IS_SUPER(c)
            && (flags
               & ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER)
            && OP(n) != ANYOFD
            && ckWARN_d(WARN_NON_UNICODE))
        {
            Perl_warner(aTHX_ packWARN(WARN_NON_UNICODE),
                "Matched non-Unicode code point 0x%04" UVXf " against Unicode property; may not be portable", c);
        }
    }

#if ANYOF_INVERT != 1
    /* Depending on compiler optimization cBOOL takes time, so if don't have to
     * use it, don't */
#   error ANYOF_INVERT needs to be set to 1, or guarded with cBOOL below,
#endif

    /* The xor complements the return if to invert: 1^1 = 0, 1^0 = 1 */
    return (flags & ANYOF_INVERT) ^ match;
}

STATIC U8 *
S_reghop3(U8 *s, SSize_t off, const U8* lim)
{
    /* return the position 'off' UTF-8 characters away from 's', forward if
     * 'off' >= 0, backwards if negative.  But don't go outside of position
     * 'lim', which better be < s  if off < 0 */

    PERL_ARGS_ASSERT_REGHOP3;

    if (off >= 0) {
	while (off-- && s < lim) {
	    /* XXX could check well-formedness here */
	    U8 *new_s = s + UTF8SKIP(s);
            if (new_s > lim) /* lim may be in the middle of a long character */
                return s;
            s = new_s;
	}
    }
    else {
        while (off++ && s > lim) {
            s--;
            if (UTF8_IS_CONTINUED(*s)) {
                while (s > lim && UTF8_IS_CONTINUATION(*s))
                    s--;
                if (! UTF8_IS_START(*s)) {
                    Perl_croak_nocontext("Malformed UTF-8 character (fatal)");
                }
	    }
            /* XXX could check well-formedness here */
	}
    }
    return s;
}

STATIC U8 *
S_reghop4(U8 *s, SSize_t off, const U8* llim, const U8* rlim)
{
    PERL_ARGS_ASSERT_REGHOP4;

    if (off >= 0) {
        while (off-- && s < rlim) {
            /* XXX could check well-formedness here */
            s += UTF8SKIP(s);
        }
    }
    else {
        while (off++ && s > llim) {
            s--;
            if (UTF8_IS_CONTINUED(*s)) {
                while (s > llim && UTF8_IS_CONTINUATION(*s))
                    s--;
                if (! UTF8_IS_START(*s)) {
                    Perl_croak_nocontext("Malformed UTF-8 character (fatal)");
                }
            }
            /* XXX could check well-formedness here */
        }
    }
    return s;
}

/* like reghop3, but returns NULL on overrun, rather than returning last
 * char pos */

STATIC U8 *
S_reghopmaybe3(U8* s, SSize_t off, const U8* const lim)
{
    PERL_ARGS_ASSERT_REGHOPMAYBE3;

    if (off >= 0) {
	while (off-- && s < lim) {
	    /* XXX could check well-formedness here */
	    s += UTF8SKIP(s);
	}
	if (off >= 0)
	    return NULL;
    }
    else {
        while (off++ && s > lim) {
            s--;
            if (UTF8_IS_CONTINUED(*s)) {
                while (s > lim && UTF8_IS_CONTINUATION(*s))
                    s--;
                if (! UTF8_IS_START(*s)) {
                    Perl_croak_nocontext("Malformed UTF-8 character (fatal)");
                }
	    }
            /* XXX could check well-formedness here */
	}
	if (off <= 0)
	    return NULL;
    }
    return s;
}


/* when executing a regex that may have (?{}), extra stuff needs setting
   up that will be visible to the called code, even before the current
   match has finished. In particular:

   * $_ is localised to the SV currently being matched;
   * pos($_) is created if necessary, ready to be updated on each call-out
     to code;
   * a fake PMOP is created that can be set to PL_curpm (normally PL_curpm
     isn't set until the current pattern is successfully finished), so that
     $1 etc of the match-so-far can be seen;
   * save the old values of subbeg etc of the current regex, and  set then
     to the current string (again, this is normally only done at the end
     of execution)
*/

static void
S_setup_eval_state(pTHX_ regmatch_info *const reginfo)
{
    MAGIC *mg;
    regexp *const rex = ReANY(reginfo->prog);
    regmatch_info_aux_eval *eval_state = reginfo->info_aux_eval;

    eval_state->rex = rex;

    if (reginfo->sv) {
        /* Make $_ available to executed code. */
        if (reginfo->sv != DEFSV) {
            SAVE_DEFSV;
            DEFSV_set(reginfo->sv);
        }

        if (!(mg = mg_find_mglob(reginfo->sv))) {
            /* prepare for quick setting of pos */
            mg = sv_magicext_mglob(reginfo->sv);
            mg->mg_len = -1;
        }
        eval_state->pos_magic = mg;
        eval_state->pos       = mg->mg_len;
        eval_state->pos_flags = mg->mg_flags;
    }
    else
        eval_state->pos_magic = NULL;

    if (!PL_reg_curpm) {
        /* PL_reg_curpm is a fake PMOP that we can attach the current
         * regex to and point PL_curpm at, so that $1 et al are visible
         * within a /(?{})/. It's just allocated once per interpreter the
         * first time its needed */
        Newxz(PL_reg_curpm, 1, PMOP);
#ifdef USE_ITHREADS
        {
            SV* const repointer = &PL_sv_undef;
            /* this regexp is also owned by the new PL_reg_curpm, which
               will try to free it.  */
            av_push(PL_regex_padav, repointer);
            PL_reg_curpm->op_pmoffset = av_tindex(PL_regex_padav);
            PL_regex_pad = AvARRAY(PL_regex_padav);
        }
#endif
    }
    SET_reg_curpm(reginfo->prog);
    eval_state->curpm = PL_curpm;
    PL_curpm_under = PL_curpm;
    PL_curpm = PL_reg_curpm;
    if (RXp_MATCH_COPIED(rex)) {
        /*  Here is a serious problem: we cannot rewrite subbeg,
            since it may be needed if this match fails.  Thus
            $` inside (?{}) could fail... */
        eval_state->subbeg     = rex->subbeg;
        eval_state->sublen     = rex->sublen;
        eval_state->suboffset  = rex->suboffset;
        eval_state->subcoffset = rex->subcoffset;
#ifdef PERL_ANY_COW
        eval_state->saved_copy = rex->saved_copy;
#endif
        RXp_MATCH_COPIED_off(rex);
    }
    else
        eval_state->subbeg = NULL;
    rex->subbeg = (char *)reginfo->strbeg;
    rex->suboffset = 0;
    rex->subcoffset = 0;
    rex->sublen = reginfo->strend - reginfo->strbeg;
}


/* destructor to clear up regmatch_info_aux and regmatch_info_aux_eval */

static void
S_cleanup_regmatch_info_aux(pTHX_ void *arg)
{
    regmatch_info_aux *aux = (regmatch_info_aux *) arg;
    regmatch_info_aux_eval *eval_state =  aux->info_aux_eval;
    regmatch_slab *s;

    Safefree(aux->poscache);

    if (eval_state) {

        /* undo the effects of S_setup_eval_state() */

        if (eval_state->subbeg) {
            regexp * const rex = eval_state->rex;
            rex->subbeg     = eval_state->subbeg;
            rex->sublen     = eval_state->sublen;
            rex->suboffset  = eval_state->suboffset;
            rex->subcoffset = eval_state->subcoffset;
#ifdef PERL_ANY_COW
            rex->saved_copy = eval_state->saved_copy;
#endif
            RXp_MATCH_COPIED_on(rex);
        }
        if (eval_state->pos_magic)
        {
            eval_state->pos_magic->mg_len = eval_state->pos;
            eval_state->pos_magic->mg_flags =
                 (eval_state->pos_magic->mg_flags & ~MGf_BYTES)
               | (eval_state->pos_flags & MGf_BYTES);
        }

        PL_curpm = eval_state->curpm;
    }

    PL_regmatch_state = aux->old_regmatch_state;
    PL_regmatch_slab  = aux->old_regmatch_slab;

    /* free all slabs above current one - this must be the last action
     * of this function, as aux and eval_state are allocated within
     * slabs and may be freed here */

    s = PL_regmatch_slab->next;
    if (s) {
        PL_regmatch_slab->next = NULL;
        while (s) {
            regmatch_slab * const osl = s;
            s = s->next;
            Safefree(osl);
        }
    }
}


STATIC void
S_to_utf8_substr(pTHX_ regexp *prog)
{
    /* Converts substr fields in prog from bytes to UTF-8, calling fbm_compile
     * on the converted value */

    int i = 1;

    PERL_ARGS_ASSERT_TO_UTF8_SUBSTR;

    do {
	if (prog->substrs->data[i].substr
	    && !prog->substrs->data[i].utf8_substr) {
	    SV* const sv = newSVsv(prog->substrs->data[i].substr);
	    prog->substrs->data[i].utf8_substr = sv;
	    sv_utf8_upgrade(sv);
	    if (SvVALID(prog->substrs->data[i].substr)) {
		if (SvTAIL(prog->substrs->data[i].substr)) {
		    /* Trim the trailing \n that fbm_compile added last
		       time.  */
		    SvCUR_set(sv, SvCUR(sv) - 1);
		    /* Whilst this makes the SV technically "invalid" (as its
		       buffer is no longer followed by "\0") when fbm_compile()
		       adds the "\n" back, a "\0" is restored.  */
		    fbm_compile(sv, FBMcf_TAIL);
		} else
		    fbm_compile(sv, 0);
	    }
	    if (prog->substrs->data[i].substr == prog->check_substr)
		prog->check_utf8 = sv;
	}
    } while (i--);
}

STATIC bool
S_to_byte_substr(pTHX_ regexp *prog)
{
    /* Converts substr fields in prog from UTF-8 to bytes, calling fbm_compile
     * on the converted value; returns FALSE if can't be converted. */

    int i = 1;

    PERL_ARGS_ASSERT_TO_BYTE_SUBSTR;

    do {
	if (prog->substrs->data[i].utf8_substr
	    && !prog->substrs->data[i].substr) {
	    SV* sv = newSVsv(prog->substrs->data[i].utf8_substr);
	    if (! sv_utf8_downgrade(sv, TRUE)) {
                return FALSE;
            }
            if (SvVALID(prog->substrs->data[i].utf8_substr)) {
                if (SvTAIL(prog->substrs->data[i].utf8_substr)) {
                    /* Trim the trailing \n that fbm_compile added last
                        time.  */
                    SvCUR_set(sv, SvCUR(sv) - 1);
                    fbm_compile(sv, FBMcf_TAIL);
                } else
                    fbm_compile(sv, 0);
            }
	    prog->substrs->data[i].substr = sv;
	    if (prog->substrs->data[i].utf8_substr == prog->check_utf8)
		prog->check_substr = sv;
	}
    } while (i--);

    return TRUE;
}

#ifndef PERL_IN_XSUB_RE

bool
Perl__is_grapheme(pTHX_ const U8 * strbeg, const U8 * s, const U8 * strend, const UV cp)
{
    /* Temporary helper function for toke.c.  Verify that the code point 'cp'
     * is a stand-alone grapheme.  The UTF-8 for 'cp' begins at position 's' in
     * the larger string bounded by 'strbeg' and 'strend'.
     *
     * 'cp' needs to be assigned (if not a future version of the Unicode
     * Standard could make it something that combines with adjacent characters,
     * so code using it would then break), and there has to be a GCB break
     * before and after the character. */

    GCB_enum cp_gcb_val, prev_cp_gcb_val, next_cp_gcb_val;
    const U8 * prev_cp_start;

    PERL_ARGS_ASSERT__IS_GRAPHEME;

    /* Unassigned code points are forbidden */
    if (UNLIKELY(! ELEMENT_RANGE_MATCHES_INVLIST(
                                    _invlist_search(PL_Assigned_invlist, cp))))
    {
        return FALSE;
    }

    cp_gcb_val = getGCB_VAL_CP(cp);

    /* Find the GCB value of the previous code point in the input */
    prev_cp_start = utf8_hop_back(s, -1, strbeg);
    if (UNLIKELY(prev_cp_start == s)) {
        prev_cp_gcb_val = GCB_EDGE;
    }
    else {
        prev_cp_gcb_val = getGCB_VAL_UTF8(prev_cp_start, strend);
    }

    /* And check that is a grapheme boundary */
    if (! isGCB(prev_cp_gcb_val, cp_gcb_val, strbeg, s,
                TRUE /* is UTF-8 encoded */ ))
    {
        return FALSE;
    }

    /* Similarly verify there is a break between the current character and the
     * following one */
    s += UTF8SKIP(s);
    if (s >= strend) {
        next_cp_gcb_val = GCB_EDGE;
    }
    else {
        next_cp_gcb_val = getGCB_VAL_UTF8(s, strend);
    }

    return isGCB(cp_gcb_val, next_cp_gcb_val, strbeg, s, TRUE);
}

/*
=head1 Unicode Support

=for apidoc isSCRIPT_RUN

Returns a bool as to whether or not the sequence of bytes from C<s> up to but
not including C<send> form a "script run".  C<utf8_target> is TRUE iff the
sequence starting at C<s> is to be treated as UTF-8.  To be precise, except for
two degenerate cases given below, this function returns TRUE iff all code
points in it come from any combination of three "scripts" given by the Unicode
"Script Extensions" property: Common, Inherited, and possibly one other.
Additionally all decimal digits must come from the same consecutive sequence of
10.

For example, if all the characters in the sequence are Greek, or Common, or
Inherited, this function will return TRUE, provided any decimal digits in it
are from the same block of digits in Common.  (These are the ASCII digits
"0".."9" and additionally a block for full width forms of these, and several
others used in mathematical notation.)   For scripts (unlike Greek) that have
their own digits defined this will accept either digits from that set or from
one of the Common digit sets, but not a combination of the two.  Some scripts,
such as Arabic, have more than one set of digits.  All digits must come from
the same set for this function to return TRUE.

C<*ret_script>, if C<ret_script> is not NULL, will on return of TRUE
contain the script found, using the C<SCX_enum> typedef.  Its value will be
C<SCX_INVALID> if the function returns FALSE.

If the sequence is empty, TRUE is returned, but C<*ret_script> (if asked for)
will be C<SCX_INVALID>.

If the sequence contains a single code point which is unassigned to a character
in the version of Unicode being used, the function will return TRUE, and the
script will be C<SCX_Unknown>.  Any other combination of unassigned code points
in the input sequence will result in the function treating the input as not
being a script run.

The returned script will be C<SCX_Inherited> iff all the code points in it are
from the Inherited script.

Otherwise, the returned script will be C<SCX_Common> iff all the code points in
it are from the Inherited or Common scripts.

=cut

*/

bool
Perl_isSCRIPT_RUN(pTHX_ const U8 * s, const U8 * send, const bool utf8_target)
{
    /* Basically, it looks at each character in the sequence to see if the
     * above conditions are met; if not it fails.  It uses an inversion map to
     * find the enum corresponding to the script of each character.  But this
     * is complicated by the fact that a few code points can be in any of
     * several scripts.  The data has been constructed so that there are
     * additional enum values (all negative) for these situations.  The
     * absolute value of those is an index into another table which contains
     * pointers to auxiliary tables for each such situation.  Each aux array
     * lists all the scripts for the given situation.  There is another,
     * parallel, table that gives the number of entries in each aux table.
     * These are all defined in charclass_invlists.h */

    /* XXX Here are the additional things UTS 39 says could be done:
     *
     * Forbid sequences of the same nonspacing mark
     *
     * Check to see that all the characters are in the sets of exemplar
     * characters for at least one language in the Unicode Common Locale Data
     * Repository [CLDR]. */

    dVAR;

    /* Things that match /\d/u */
    SV * decimals_invlist = PL_XPosix_ptrs[_CC_DIGIT];
    UV * decimals_array = invlist_array(decimals_invlist);

    /* What code point is the digit '0' of the script run? (0 meaning FALSE if
     * not currently known) */
    UV zero_of_run = 0;

    SCX_enum script_of_run  = SCX_INVALID;   /* Illegal value */
    SCX_enum script_of_char = SCX_INVALID;

    /* If the script remains not fully determined from iteration to iteration,
     * this is the current intersection of the possiblities.  */
    SCX_enum * intersection = NULL;
    PERL_UINT_FAST8_T intersection_len = 0;

    bool retval = TRUE;
    SCX_enum * ret_script = NULL;

    assert(send >= s);

    PERL_ARGS_ASSERT_ISSCRIPT_RUN;

    /* All code points in 0..255 are either Common or Latin, so must be a
     * script run.  We can return immediately unless we need to know which
     * script it is. */
    if (! utf8_target && LIKELY(send > s)) {
        if (ret_script == NULL) {
            return TRUE;
        }

        /* If any character is Latin, the run is Latin */
        while (s < send) {
            if (isALPHA_L1(*s) && LIKELY(*s != MICRO_SIGN_NATIVE)) {
                *ret_script = SCX_Latin;
                return TRUE;
            }
        }

        /* Here, all are Common */
        *ret_script = SCX_Common;
        return TRUE;
    }

    /* Look at each character in the sequence */
    while (s < send) {
        /* If the current character being examined is a digit, this is the code
         * point of the zero for its sequence of 10 */
        UV zero_of_char;

        UV cp;

        /* The code allows all scripts to use the ASCII digits.  This is
         * because they are in the Common script.  Hence any ASCII ones found
         * are ok, unless and until a digit from another set has already been
         * encountered.  digit ranges in Common are not similarly blessed) */
        if (UNLIKELY(isDIGIT(*s))) {
            if (UNLIKELY(script_of_run == SCX_Unknown)) {
                retval = FALSE;
                break;
            }
            if (zero_of_run) {
                if (zero_of_run != '0') {
                    retval = FALSE;
                    break;
                }
            }
            else {
                zero_of_run = '0';
            }
            s++;
            continue;
        }

        /* Here, isn't an ASCII digit.  Find the code point of the character */
        if (! UTF8_IS_INVARIANT(*s)) {
            Size_t len;
            cp = valid_utf8_to_uvchr((U8 *) s, &len);
            s += len;
        }
        else {
            cp = *(s++);
        }

        /* If is within the range [+0 .. +9] of the script's zero, it also is a
         * digit in that script.  We can skip the rest of this code for this
         * character. */
        if (UNLIKELY(   zero_of_run
                     && cp >= zero_of_run
                     && cp - zero_of_run <= 9))
        {
            continue;
        }

        /* Find the character's script.  The correct values are hard-coded here
         * for small-enough code points. */
        if (cp < 0x2B9) {   /* From inspection of Unicode db; extremely
                               unlikely to change */
            if (       cp > 255
                || (   isALPHA_L1(cp)
                    && LIKELY(cp != MICRO_SIGN_NATIVE)))
            {
                script_of_char = SCX_Latin;
            }
            else {
                script_of_char = SCX_Common;
            }
        }
        else {
            script_of_char = _Perl_SCX_invmap[
                                       _invlist_search(PL_SCX_invlist, cp)];
        }

        /* We arbitrarily accept a single unassigned character, but not in
         * combination with anything else, and not a run of them. */
        if (   UNLIKELY(script_of_run == SCX_Unknown)
            || UNLIKELY(   script_of_run != SCX_INVALID
                        && script_of_char == SCX_Unknown))
        {
            retval = FALSE;
            break;
        }

        /* For the first character, or the run is inherited, the run's script
         * is set to the char's */
        if (   UNLIKELY(script_of_run == SCX_INVALID)
            || UNLIKELY(script_of_run == SCX_Inherited))
        {
            script_of_run = script_of_char;
        }

        /* For the character's script to be Unknown, it must be the first
         * character in the sequence (for otherwise a test above would have
         * prevented us from reaching here), and we have set the run's script
         * to it.  Nothing further to be done for this character */
        if (UNLIKELY(script_of_char == SCX_Unknown)) {
            continue;
        }

        /* We accept 'inherited' script characters currently even at the
         * beginning.  (We know that no characters in Inherited are digits, or
         * we'd have to check for that) */
        if (UNLIKELY(script_of_char == SCX_Inherited)) {
            continue;
        }

        /* If the run so far is Common, and the new character isn't, change the
         * run's script to that of this character */
        if (script_of_run == SCX_Common && script_of_char != SCX_Common) {
            script_of_run = script_of_char;
        }

        /* Now we can see if the script of the new character is the same as
         * that of the run */
        if (LIKELY(script_of_char == script_of_run)) {
            /* By far the most common case */
            goto scripts_match;
        }

        /* Here, the script of the run isn't Common.  But characters in Common
         * match any script */
        if (script_of_char == SCX_Common) {
            goto scripts_match;
        }

#ifndef HAS_SCX_AUX_TABLES

        /* Too early a Unicode version to have a code point belonging to more
         * than one script, so, if the scripts don't exactly match, fail */
        PERL_UNUSED_VAR(intersection_len);
        retval = FALSE;
        break;

#else

        /* Here there is no exact match between the character's script and the
         * run's.  And we've handled the special cases of scripts Unknown,
         * Inherited, and Common.
         *
         * Negative script numbers signify that the value may be any of several
         * scripts, and we need to look at auxiliary information to make our
         * deterimination.  But if both are non-negative, we can fail now */
        if (LIKELY(script_of_char >= 0)) {
            const SCX_enum * search_in;
            PERL_UINT_FAST8_T search_in_len;
            PERL_UINT_FAST8_T i;

            if (LIKELY(script_of_run >= 0)) {
                retval = FALSE;
                break;
            }

            /* Use the previously constructed set of possible scripts, if any.
             * */
            if (intersection) {
                search_in = intersection;
                search_in_len = intersection_len;
            }
            else {
                search_in = SCX_AUX_TABLE_ptrs[-script_of_run];
                search_in_len = SCX_AUX_TABLE_lengths[-script_of_run];
            }

            for (i = 0; i < search_in_len; i++) {
                if (search_in[i] == script_of_char) {
                    script_of_run = script_of_char;
                    goto scripts_match;
                }
            }

            retval = FALSE;
            break;
        }
        else if (LIKELY(script_of_run >= 0)) {
            /* script of character could be one of several, but run is a single
             * script */
            const SCX_enum * search_in = SCX_AUX_TABLE_ptrs[-script_of_char];
            const PERL_UINT_FAST8_T search_in_len
                                     = SCX_AUX_TABLE_lengths[-script_of_char];
            PERL_UINT_FAST8_T i;

            for (i = 0; i < search_in_len; i++) {
                if (search_in[i] == script_of_run) {
                    script_of_char = script_of_run;
                    goto scripts_match;
                }
            }

            retval = FALSE;
            break;
        }
        else {
            /* Both run and char could be in one of several scripts.  If the
             * intersection is empty, then this character isn't in this script
             * run.  Otherwise, we need to calculate the intersection to use
             * for future iterations of the loop, unless we are already at the
             * final character */
            const SCX_enum * search_char = SCX_AUX_TABLE_ptrs[-script_of_char];
            const PERL_UINT_FAST8_T char_len
                                      = SCX_AUX_TABLE_lengths[-script_of_char];
            const SCX_enum * search_run;
            PERL_UINT_FAST8_T run_len;

            SCX_enum * new_overlap = NULL;
            PERL_UINT_FAST8_T i, j;

            if (intersection) {
                search_run = intersection;
                run_len = intersection_len;
            }
            else {
                search_run = SCX_AUX_TABLE_ptrs[-script_of_run];
                run_len = SCX_AUX_TABLE_lengths[-script_of_run];
            }

            intersection_len = 0;

            for (i = 0; i < run_len; i++) {
                for (j = 0; j < char_len; j++) {
                    if (search_run[i] == search_char[j]) {

                        /* Here, the script at i,j matches.  That means this
                         * character is in the run.  But continue on to find
                         * the complete intersection, for the next loop
                         * iteration, and for the digit check after it.
                         *
                         * On the first found common script, we malloc space
                         * for the intersection list for the worst case of the
                         * intersection, which is the minimum of the number of
                         * scripts remaining in each set. */
                        if (intersection_len == 0) {
                            Newx(new_overlap,
                                 MIN(run_len - i, char_len - j),
                                 SCX_enum);
                        }
                        new_overlap[intersection_len++] = search_run[i];
                    }
                }
            }

            /* Here we've looked through everything.  If they have no scripts
             * in common, not a run */
            if (intersection_len == 0) {
                retval = FALSE;
                break;
            }

            /* If there is only a single script in common, set to that.
             * Otherwise, use the intersection going forward */
            Safefree(intersection);
            intersection = NULL;
            if (intersection_len == 1) {
                script_of_run = script_of_char = new_overlap[0];
                Safefree(new_overlap);
                new_overlap = NULL;
            }
            else {
                intersection = new_overlap;
            }
        }

#endif

  scripts_match:

        /* Here, the script of the character is compatible with that of the
         * run.  That means that in most cases, it continues the script run.
         * Either it and the run match exactly, or one or both can be in any of
         * several scripts, and the intersection is not empty.  However, if the
         * character is a decimal digit, it could still mean failure if it is
         * from the wrong sequence of 10.  So, we need to look at if it's a
         * digit.  We've already handled the 10 decimal digits, and the next
         * lowest one is this one: */
        if (cp < FIRST_NON_ASCII_DECIMAL_DIGIT) {
            continue;   /* Not a digit; this character is part of the run */
        }

        /* If we have a definitive '0' for the script of this character, we
         * know that for this to be a digit, it must be in the range of +0..+9
         * of that zero. */
        if (   script_of_char >= 0
            && (zero_of_char = script_zeros[script_of_char]))
        {
            if (   cp < zero_of_char
                || cp > zero_of_char + 9)
            {
                continue;   /* Not a digit; this character is part of the run
                             */
            }

        }
        else {  /* Need to look up if this character is a digit or not */
            SSize_t index_of_zero_of_char;
            index_of_zero_of_char = _invlist_search(decimals_invlist, cp);
            if (     UNLIKELY(index_of_zero_of_char < 0)
                || ! ELEMENT_RANGE_MATCHES_INVLIST(index_of_zero_of_char))
            {
                continue;   /* Not a digit; this character is part of the run.
                             */
            }

            zero_of_char = decimals_array[index_of_zero_of_char];
        }

        /* Here, the character is a decimal digit, and the zero of its sequence
         * of 10 is in 'zero_of_char'.  If we already have a zero for this run,
         * they better be the same. */
        if (zero_of_run) {
            if (zero_of_run != zero_of_char) {
                retval = FALSE;
                break;
            }
        }
        else {  /* Otherwise we now have a zero for this run */
            zero_of_run = zero_of_char;
        }
    } /* end of looping through CLOSESR text */

    Safefree(intersection);

    if (ret_script != NULL) {
        if (retval) {
            *ret_script = script_of_run;
        }
        else {
            *ret_script = SCX_INVALID;
        }
    }

    return retval;
}

#endif /* ifndef PERL_IN_XSUB_RE */

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
