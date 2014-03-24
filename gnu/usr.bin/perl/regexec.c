/*    regexec.c
 */

/*
 * 	One Ring to rule them all, One Ring to find them
 &
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

/* At least one required character in the target string is expressible only in
 * UTF-8. */
static const char* const non_utf8_target_but_utf8_required
                = "Can't match, because target string needs to be in UTF-8\n";

#define NON_UTF8_TARGET_BUT_UTF8_REQUIRED(target) STMT_START { \
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%s", non_utf8_target_but_utf8_required));\
    goto target; \
} STMT_END

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

#include "inline_invlist.c"
#include "unicode_constants.h"

#define HAS_NONLATIN1_FOLD_CLOSURE(i) _HAS_NONLATIN1_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(i)

#ifndef STATIC
#define	STATIC	static
#endif

/* Valid for non-utf8 strings: avoids the reginclass
 * call if there are no complications: i.e., if everything matchable is
 * straight forward in the bitmap */
#define REGINCLASS(prog,p,c)  (ANYOF_FLAGS(p) ? reginclass(prog,p,c,0)   \
					      : ANYOF_BITMAP_TEST(p,*(c)))

/*
 * Forwards.
 */

#define CHR_SVLEN(sv) (utf8_target ? sv_len_utf8(sv) : SvCUR(sv))
#define CHR_DIST(a,b) (PL_reg_match_utf8 ? utf8_distance(a,b) : a - b)

#define HOPc(pos,off) \
	(char *)(PL_reg_match_utf8 \
	    ? reghop3((U8*)pos, off, (U8*)(off >= 0 ? PL_regeol : PL_bostr)) \
	    : (U8*)(pos + off))
#define HOPBACKc(pos, off) \
	(char*)(PL_reg_match_utf8\
	    ? reghopmaybe3((U8*)pos, -off, (U8*)PL_bostr) \
	    : (pos - off >= PL_bostr)		\
		? (U8*)pos - off		\
		: NULL)

#define HOP3(pos,off,lim) (PL_reg_match_utf8 ? reghop3((U8*)(pos), off, (U8*)(lim)) : (U8*)(pos + off))
#define HOP3c(pos,off,lim) ((char*)HOP3(pos,off,lim))


#define NEXTCHR_EOS -10 /* nextchr has fallen off the end */
#define NEXTCHR_IS_EOS (nextchr < 0)

#define SET_nextchr \
    nextchr = ((locinput < PL_regeol) ? UCHARAT(locinput) : NEXTCHR_EOS)

#define SET_locinput(p) \
    locinput = (p);  \
    SET_nextchr


#define LOAD_UTF8_CHARCLASS(swash_ptr, property_name) STMT_START {            \
        if (!swash_ptr) {                                                     \
            U8 flags = _CORE_SWASH_INIT_ACCEPT_INVLIST;                       \
            swash_ptr = _core_swash_init("utf8", property_name, &PL_sv_undef, \
                                         1, 0, NULL, &flags);                 \
            assert(swash_ptr);                                                \
        }                                                                     \
    } STMT_END

/* If in debug mode, we test that a known character properly matches */
#ifdef DEBUGGING
#   define LOAD_UTF8_CHARCLASS_DEBUG_TEST(swash_ptr,                          \
                                          property_name,                      \
                                          utf8_char_in_property)              \
        LOAD_UTF8_CHARCLASS(swash_ptr, property_name);                        \
        assert(swash_fetch(swash_ptr, (U8 *) utf8_char_in_property, TRUE));
#else
#   define LOAD_UTF8_CHARCLASS_DEBUG_TEST(swash_ptr,                          \
                                          property_name,                      \
                                          utf8_char_in_property)              \
        LOAD_UTF8_CHARCLASS(swash_ptr, property_name)
#endif

#define LOAD_UTF8_CHARCLASS_ALNUM() LOAD_UTF8_CHARCLASS_DEBUG_TEST(           \
                                        PL_utf8_swash_ptrs[_CC_WORDCHAR],     \
                                        swash_property_names[_CC_WORDCHAR],   \
                                        GREEK_SMALL_LETTER_IOTA_UTF8)

#define LOAD_UTF8_CHARCLASS_GCB()  /* Grapheme cluster boundaries */          \
    STMT_START {                                                              \
	LOAD_UTF8_CHARCLASS_DEBUG_TEST(PL_utf8_X_regular_begin,               \
                                       "_X_regular_begin",                    \
                                       GREEK_SMALL_LETTER_IOTA_UTF8);         \
	LOAD_UTF8_CHARCLASS_DEBUG_TEST(PL_utf8_X_extend,                      \
                                       "_X_extend",                           \
                                       COMBINING_GRAVE_ACCENT_UTF8);          \
    } STMT_END

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
#define JUMPABLE(rn) (      \
    OP(rn) == OPEN ||       \
    (OP(rn) == CLOSE && (!cur_eval || cur_eval->u.eval.close_paren != ARG(rn))) || \
    OP(rn) == EVAL ||   \
    OP(rn) == SUSPEND || OP(rn) == IFMATCH || \
    OP(rn) == PLUS || OP(rn) == MINMOD || \
    OP(rn) == KEEPS || \
    (PL_regkind[OP(rn)] == CURLY && ARG1(rn) > 0) \
)
#define IS_EXACT(rn) (PL_regkind[OP(rn)] == EXACT)

#define HAS_TEXT(rn) ( IS_EXACT(rn) || PL_regkind[OP(rn)] == REF )

#if 0 
/* Currently these are only used when PL_regkind[OP(rn)] == EXACT so
   we don't need this definition. */
#define IS_TEXT(rn)   ( OP(rn)==EXACT   || OP(rn)==REF   || OP(rn)==NREF   )
#define IS_TEXTF(rn)  ( OP(rn)==EXACTFU || OP(rn)==EXACTFU_SS || OP(rn)==EXACTFU_TRICKYFOLD || OP(rn)==EXACTFA || OP(rn)==EXACTF || OP(rn)==REFF  || OP(rn)==NREFF )
#define IS_TEXTFL(rn) ( OP(rn)==EXACTFL || OP(rn)==REFFL || OP(rn)==NREFFL )

#else
/* ... so we use this as its faster. */
#define IS_TEXT(rn)   ( OP(rn)==EXACT   )
#define IS_TEXTFU(rn)  ( OP(rn)==EXACTFU || OP(rn)==EXACTFU_SS || OP(rn)==EXACTFU_TRICKYFOLD || OP(rn) == EXACTFA)
#define IS_TEXTF(rn)  ( OP(rn)==EXACTF  )
#define IS_TEXTFL(rn) ( OP(rn)==EXACTFL )

#endif

/*
  Search for mandatory following text node; for lookahead, the text must
  follow but for lookbehind (rn->flags != 0) we skip to the next step.
*/
#define FIND_NEXT_IMPT(rn) STMT_START { \
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

/* These constants are for finding GCB=LV and GCB=LVT in the CLUMP regnode.
 * These are for the pre-composed Hangul syllables, which are all in a
 * contiguous block and arranged there in such a way so as to facilitate
 * alorithmic determination of their characteristics.  As such, they don't need
 * a swash, but can be determined by simple arithmetic.  Almost all are
 * GCB=LVT, but every 28th one is a GCB=LV */
#define SBASE 0xAC00    /* Start of block */
#define SCount 11172    /* Length of block */
#define TCount 28

static void restore_pos(pTHX_ void *arg);

#define REGCP_PAREN_ELEMS 3
#define REGCP_OTHER_ELEMS 3
#define REGCP_FRAME_ELEMS 1
/* REGCP_FRAME_ELEMS are not part of the REGCP_OTHER_ELEMS and
 * are needed for the regexp context stack bookkeeping. */

STATIC CHECKPOINT
S_regcppush(pTHX_ const regexp *rex, I32 parenfloor, U32 maxopenparen)
{
    dVAR;
    const int retval = PL_savestack_ix;
    const int paren_elems_to_push =
                (maxopenparen - parenfloor) * REGCP_PAREN_ELEMS;
    const UV total_elems = paren_elems_to_push + REGCP_OTHER_ELEMS;
    const UV elems_shifted = total_elems << SAVE_TIGHT_SHIFT;
    I32 p;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGCPPUSH;

    if (paren_elems_to_push < 0)
	Perl_croak(aTHX_ "panic: paren_elems_to_push, %i < 0",
		   paren_elems_to_push);

    if ((elems_shifted >> SAVE_TIGHT_SHIFT) != total_elems)
	Perl_croak(aTHX_ "panic: paren_elems_to_push offset %"UVuf
		   " out of range (%lu-%ld)",
		   total_elems,
                   (unsigned long)maxopenparen,
                   (long)parenfloor);

    SSGROW(total_elems + REGCP_FRAME_ELEMS);
    
    DEBUG_BUFFERS_r(
	if ((int)maxopenparen > (int)parenfloor)
	    PerlIO_printf(Perl_debug_log,
		"rex=0x%"UVxf" offs=0x%"UVxf": saving capture indices:\n",
		PTR2UV(rex),
		PTR2UV(rex->offs)
	    );
    );
    for (p = parenfloor+1; p <= (I32)maxopenparen;  p++) {
/* REGCP_PARENS_ELEMS are pushed per pairs of parentheses. */
	SSPUSHINT(rex->offs[p].end);
	SSPUSHINT(rex->offs[p].start);
	SSPUSHINT(rex->offs[p].start_tmp);
	DEBUG_BUFFERS_r(PerlIO_printf(Perl_debug_log,
	    "    \\%"UVuf": %"IVdf"(%"IVdf")..%"IVdf"\n",
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
            PerlIO_printf(Perl_debug_log,		        \
	        "  Setting an EVAL scope, savestack=%"IVdf"\n",	\
	        (IV)PL_savestack_ix));                          \
    cp = PL_savestack_ix

#define REGCP_UNWIND(cp)                                        \
    DEBUG_STATE_r(                                              \
        if (cp != PL_savestack_ix) 		                \
    	    PerlIO_printf(Perl_debug_log,		        \
		"  Clearing an EVAL scope, savestack=%"IVdf"..%"IVdf"\n", \
	        (IV)(cp), (IV)PL_savestack_ix));                \
    regcpblow(cp)

#define UNWIND_PAREN(lp, lcp)               \
    for (n = rex->lastparen; n > lp; n--)   \
        rex->offs[n].end = -1;              \
    rex->lastparen = n;                     \
    rex->lastcloseparen = lcp;


STATIC void
S_regcppop(pTHX_ regexp *rex, U32 *maxopenparen_p)
{
    dVAR;
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
	    PerlIO_printf(Perl_debug_log,
		"rex=0x%"UVxf" offs=0x%"UVxf": restoring capture indices to:\n",
		PTR2UV(rex),
		PTR2UV(rex->offs)
	    );
    );
    paren = *maxopenparen_p;
    for ( ; i > 0; i -= REGCP_PAREN_ELEMS) {
	I32 tmps;
	rex->offs[paren].start_tmp = SSPOPINT;
	rex->offs[paren].start = SSPOPINT;
	tmps = SSPOPINT;
	if (paren <= rex->lastparen)
	    rex->offs[paren].end = tmps;
	DEBUG_BUFFERS_r( PerlIO_printf(Perl_debug_log,
	    "    \\%"UVuf": %"IVdf"(%"IVdf")..%"IVdf"%s\n",
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
	DEBUG_BUFFERS_r( PerlIO_printf(Perl_debug_log,
	    "    \\%"UVuf": %s   ..-1 undeffing\n",
	    (UV)i,
	    (i > *maxopenparen_p) ? "-1" : "  "
	));
    }
#endif
}

/* restore the parens and associated vars at savestack position ix,
 * but without popping the stack */

STATIC void
S_regcp_restore(pTHX_ regexp *rex, I32 ix, U32 *maxopenparen_p)
{
    I32 tmpix = PL_savestack_ix;
    PL_savestack_ix = ix;
    regcppop(rex, maxopenparen_p);
    PL_savestack_ix = tmpix;
}

#define regcpblow(cp) LEAVE_SCOPE(cp)	/* Ignores regcppush()ed data. */

STATIC bool
S_isFOO_lc(pTHX_ const U8 classnum, const U8 character)
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
        case _CC_ENUM_CASED:     return isLOWER_LC(character)
                                        || isUPPER_LC(character);
        case _CC_ENUM_CNTRL:     return isCNTRL_LC(character);
        case _CC_ENUM_DIGIT:     return isDIGIT_LC(character);
        case _CC_ENUM_GRAPH:     return isGRAPH_LC(character);
        case _CC_ENUM_LOWER:     return isLOWER_LC(character);
        case _CC_ENUM_PRINT:     return isPRINT_LC(character);
        case _CC_ENUM_PSXSPC:    return isPSXSPC_LC(character);
        case _CC_ENUM_PUNCT:     return isPUNCT_LC(character);
        case _CC_ENUM_SPACE:     return isSPACE_LC(character);
        case _CC_ENUM_UPPER:     return isUPPER_LC(character);
        case _CC_ENUM_WORDCHAR:  return isWORDCHAR_LC(character);
        case _CC_ENUM_XDIGIT:    return isXDIGIT_LC(character);
        default:    /* VERTSPACE should never occur in locales */
            Perl_croak(aTHX_ "panic: isFOO_lc() has an unexpected character class '%d'", classnum);
    }

    assert(0); /* NOTREACHED */
    return FALSE;
}

STATIC bool
S_isFOO_utf8_lc(pTHX_ const U8 classnum, const U8* character)
{
    /* Returns a boolean as to whether or not the (well-formed) UTF-8-encoded
     * 'character' is a member of the Posix character class given by 'classnum'
     * that should be equivalent to a value in the typedef
     * '_char_class_number'.
     *
     * This just calls isFOO_lc on the code point for the character if it is in
     * the range 0-255.  Outside that range, all characters avoid Unicode
     * rules, ignoring any locale.  So use the Unicode function if this class
     * requires a swash, and use the Unicode macro otherwise. */

    PERL_ARGS_ASSERT_ISFOO_UTF8_LC;

    if (UTF8_IS_INVARIANT(*character)) {
        return isFOO_lc(classnum, *character);
    }
    else if (UTF8_IS_DOWNGRADEABLE_START(*character)) {
        return isFOO_lc(classnum,
                        TWO_BYTE_UTF8_TO_UNI(*character, *(character + 1)));
    }

    if (classnum < _FIRST_NON_SWASH_CC) {

        /* Initialize the swash unless done already */
        if (! PL_utf8_swash_ptrs[classnum]) {
            U8 flags = _CORE_SWASH_INIT_ACCEPT_INVLIST;
            PL_utf8_swash_ptrs[classnum] = _core_swash_init("utf8",
                swash_property_names[classnum], &PL_sv_undef, 1, 0, NULL, &flags);
        }

        return cBOOL(swash_fetch(PL_utf8_swash_ptrs[classnum], (U8 *)
                                 character,
                                 TRUE /* is UTF */ ));
    }

    switch ((_char_class_number) classnum) {
        case _CC_ENUM_SPACE:
        case _CC_ENUM_PSXSPC:    return is_XPERLSPACE_high(character);

        case _CC_ENUM_BLANK:     return is_HORIZWS_high(character);
        case _CC_ENUM_XDIGIT:    return is_XDIGIT_high(character);
        case _CC_ENUM_VERTSPACE: return is_VERTWS_high(character);
        default:                 return 0;  /* Things like CNTRL are always
                                               below 256 */
    }

    assert(0); /* NOTREACHED */
    return FALSE;
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
	 char *strbeg, I32 minend, SV *screamer, U32 nosave)
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

/*
 * Need to implement the following flags for reg_anch:
 *
 * USE_INTUIT_NOML		- Useful to call re_intuit_start() first
 * USE_INTUIT_ML
 * INTUIT_AUTORITATIVE_NOML	- Can trust a positive answer
 * INTUIT_AUTORITATIVE_ML
 * INTUIT_ONCE_NOML		- Intuit can match in one location only.
 * INTUIT_ONCE_ML
 *
 * Another flag for this function: SECOND_TIME (so that float substrs
 * with giant delta may be not rechecked).
 */

/* Assumptions: if ANCH_GPOS, then strpos is anchored. XXXX Check GPOS logic */

/* If SCREAM, then SvPVX_const(sv) should be compatible with strpos and strend.
   Otherwise, only SvCUR(sv) is used to get strbeg. */

/* XXXX We assume that strpos is strbeg unless sv. */

/* XXXX Some places assume that there is a fixed substring.
	An update may be needed if optimizer marks as "INTUITable"
	RExen without fixed substrings.  Similarly, it is assumed that
	lengths of all the strings are no more than minlen, thus they
	cannot come from lookahead.
	(Or minlen should take into account lookahead.) 
  NOTE: Some of this comment is not correct. minlen does now take account
  of lookahead/behind. Further research is required. -- demerphq

*/

/* A failure to find a constant substring means that there is no need to make
   an expensive call to REx engine, thus we celebrate a failure.  Similarly,
   finding a substring too deep into the string means that fewer calls to
   regtry() should be needed.

   REx compiler's optimizer found 4 possible hints:
	a) Anchored substring;
	b) Fixed substring;
	c) Whether we are anchored (beginning-of-line or \G);
	d) First node (of those at offset 0) which may distinguish positions;
   We use a)b)d) and multiline-part of c), and try to find a position in the
   string which does not contradict any of them.
 */

/* Most of decisions we do here should have been done at compile time.
   The nodes of the REx which we used for the search should have been
   deleted from the finite automaton. */

char *
Perl_re_intuit_start(pTHX_ REGEXP * const rx, SV *sv, char *strpos,
		     char *strend, const U32 flags, re_scream_pos_data *data)
{
    dVAR;
    struct regexp *const prog = ReANY(rx);
    I32 start_shift = 0;
    /* Should be nonnegative! */
    I32 end_shift   = 0;
    char *s;
    SV *check;
    char *strbeg;
    char *t;
    const bool utf8_target = (sv && SvUTF8(sv)) ? 1 : 0; /* if no sv we have to assume bytes */
    I32 ml_anch;
    char *other_last = NULL;	/* other substr checked before this */
    char *check_at = NULL;		/* check substr found at this pos */
    char *checked_upto = NULL;          /* how far into the string we have already checked using find_byclass*/
    const I32 multiline = prog->extflags & RXf_PMf_MULTILINE;
    RXi_GET_DECL(prog,progi);
    bool is_utf8_pat;
#ifdef DEBUGGING
    const char * const i_strpos = strpos;
#endif
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_INTUIT_START;
    PERL_UNUSED_ARG(flags);
    PERL_UNUSED_ARG(data);

    RX_MATCH_UTF8_set(rx,utf8_target);

    is_utf8_pat = cBOOL(RX_UTF8(rx));

    DEBUG_EXECUTE_r( 
        debug_start_match(rx, utf8_target, strpos, strend,
            sv ? "Guessing start of match in sv for"
               : "Guessing start of match in string for");
	      );

    /* CHR_DIST() would be more correct here but it makes things slow. */
    if (prog->minlen > strend - strpos) {
	DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			      "String too short... [re_intuit_start]\n"));
	goto fail;
    }

    /* XXX we need to pass strbeg as a separate arg: the following is
     * guesswork and can be wrong... */
    if (sv && SvPOK(sv)) {
        char * p   = SvPVX(sv);
        STRLEN cur = SvCUR(sv); 
        if (p <= strpos && strpos < p + cur) {
            strbeg = p;
            assert(p <= strend && strend <= p + cur);
        }
        else
            strbeg = strend - cur;
    }
    else 
        strbeg = strpos;

    PL_regeol = strend;
    if (utf8_target) {
	if (!prog->check_utf8 && prog->check_substr)
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
    if (prog->extflags & RXf_ANCH) {	/* Match at beg-of-str or after \n */
	ml_anch = !( (prog->extflags & RXf_ANCH_SINGLE)
		     || ( (prog->extflags & RXf_ANCH_BOL)
			  && !multiline ) );	/* Check after \n? */

	if (!ml_anch) {
	  if ( !(prog->extflags & RXf_ANCH_GPOS) /* Checked by the caller */
		&& !(prog->intflags & PREGf_IMPLICIT) /* not a real BOL */
	       /* SvCUR is not set on references: SvRV and SvPVX_const overlap */
	       && sv && !SvROK(sv)
	       && (strpos != strbeg)) {
	      DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Not at start...\n"));
	      goto fail;
	  }
	  if (prog->check_offset_min == prog->check_offset_max
              && !(prog->extflags & RXf_CANY_SEEN)
              && ! multiline)   /* /m can cause \n's to match that aren't
                                   accounted for in the string max length.
                                   See [perl #115242] */
          {
	    /* Substring at constant offset from beg-of-str... */
	    I32 slen;

	    s = HOP3c(strpos, prog->check_offset_min, strend);
	    
	    if (SvTAIL(check)) {
		slen = SvCUR(check);	/* >= 1 */

		if ( strend - s > slen || strend - s < slen - 1
		     || (strend - s == slen && strend[-1] != '\n')) {
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "String too long...\n"));
		    goto fail_finish;
		}
		/* Now should match s[0..slen-2] */
		slen--;
		if (slen && (*SvPVX_const(check) != *s
			     || (slen > 1
				 && memNE(SvPVX_const(check), s, slen)))) {
		  report_neq:
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "String not equal...\n"));
		    goto fail_finish;
		}
	    }
	    else if (*SvPVX_const(check) != *s
		     || ((slen = SvCUR(check)) > 1
			 && memNE(SvPVX_const(check), s, slen)))
		goto report_neq;
	    check_at = s;
	    goto success_at_start;
	  }
	}
	/* Match is anchored, but substr is not anchored wrt beg-of-str. */
	s = strpos;
	start_shift = prog->check_offset_min; /* okay to underestimate on CC */
	end_shift = prog->check_end_shift;
	
	if (!ml_anch) {
	    const I32 end = prog->check_offset_max + CHR_SVLEN(check)
					 - (SvTAIL(check) != 0);
	    const I32 eshift = CHR_DIST((U8*)strend, (U8*)s) - end;

	    if (end_shift < eshift)
		end_shift = eshift;
	}
    }
    else {				/* Can match at random position */
	ml_anch = 0;
	s = strpos;
	start_shift = prog->check_offset_min;  /* okay to underestimate on CC */
	end_shift = prog->check_end_shift;
	
	/* end shift should be non negative here */
    }

#ifdef QDEBUGGING	/* 7/99: reports of failure (with the older version) */
    if (end_shift < 0)
	Perl_croak(aTHX_ "panic: end_shift: %"IVdf" pattern:\n%s\n ",
		   (IV)end_shift, RX_PRECOMP(prog));
#endif

  restart:
    /* Find a possible match in the region s..strend by looking for
       the "check" substring in the region corrected by start/end_shift. */
    
    {
        I32 srch_start_shift = start_shift;
        I32 srch_end_shift = end_shift;
        U8* start_point;
        U8* end_point;
        if (srch_start_shift < 0 && strbeg - s > srch_start_shift) {
	    srch_end_shift -= ((strbeg - s) - srch_start_shift); 
	    srch_start_shift = strbeg - s;
	}
    DEBUG_OPTIMISE_MORE_r({
        PerlIO_printf(Perl_debug_log, "Check offset min: %"IVdf" Start shift: %"IVdf" End shift %"IVdf" Real End Shift: %"IVdf"\n",
            (IV)prog->check_offset_min,
            (IV)srch_start_shift,
            (IV)srch_end_shift, 
            (IV)prog->check_end_shift);
    });       
        
        if (prog->extflags & RXf_CANY_SEEN) {
            start_point= (U8*)(s + srch_start_shift);
            end_point= (U8*)(strend - srch_end_shift);
        } else {
	    start_point= HOP3(s, srch_start_shift, srch_start_shift < 0 ? strbeg : strend);
            end_point= HOP3(strend, -srch_end_shift, strbeg);
	}
	DEBUG_OPTIMISE_MORE_r({
            PerlIO_printf(Perl_debug_log, "fbm_instr len=%d str=<%.*s>\n", 
                (int)(end_point - start_point),
                (int)(end_point - start_point) > 20 ? 20 : (int)(end_point - start_point), 
                start_point);
        });

	s = fbm_instr( start_point, end_point,
		      check, multiline ? FBMrf_MULTILINE : 0);
    }
    /* Update the count-of-usability, remove useless subpatterns,
	unshift s.  */

    DEBUG_EXECUTE_r({
        RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
            SvPVX_const(check), RE_SV_DUMPLEN(check), 30);
        PerlIO_printf(Perl_debug_log, "%s %s substr %s%s%s",
			  (s ? "Found" : "Did not find"),
	    (check == (utf8_target ? prog->anchored_utf8 : prog->anchored_substr)
	        ? "anchored" : "floating"),
	    quoted,
	    RE_SV_TAIL(check),
	    (s ? " at offset " : "...\n") ); 
    });

    if (!s)
	goto fail_finish;
    /* Finish the diagnostic message */
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%ld...\n", (long)(s - i_strpos)) );

    /* XXX dmq: first branch is for positive lookbehind...
       Our check string is offset from the beginning of the pattern.
       So we need to do any stclass tests offset forward from that 
       point. I think. :-(
     */
    
        
    
    check_at=s;
     

    /* Got a candidate.  Check MBOL anchoring, and the *other* substr.
       Start with the other substr.
       XXXX no SCREAM optimization yet - and a very coarse implementation
       XXXX /ttx+/ results in anchored="ttx", floating="x".  floating will
		*always* match.  Probably should be marked during compile...
       Probably it is right to do no SCREAM here...
     */

    if (utf8_target ? (prog->float_utf8 && prog->anchored_utf8)
                : (prog->float_substr && prog->anchored_substr)) 
    {
	/* Take into account the "other" substring. */
	/* XXXX May be hopelessly wrong for UTF... */
	if (!other_last)
	    other_last = strpos;
	if (check == (utf8_target ? prog->float_utf8 : prog->float_substr)) {
	  do_other_anchored:
	    {
		char * const last = HOP3c(s, -start_shift, strbeg);
		char *last1, *last2;
		char * const saved_s = s;
		SV* must;

		t = s - prog->check_offset_max;
		if (s - strpos > prog->check_offset_max  /* signed-corrected t > strpos */
		    && (!utf8_target
			|| ((t = (char*)reghopmaybe3((U8*)s, -(prog->check_offset_max), (U8*)strpos))
			    && t > strpos)))
		    NOOP;
		else
		    t = strpos;
		t = HOP3c(t, prog->anchored_offset, strend);
		if (t < other_last)	/* These positions already checked */
		    t = other_last;
		last2 = last1 = HOP3c(strend, -prog->minlen, strbeg);
		if (last < last1)
		    last1 = last;
                /* XXXX It is not documented what units *_offsets are in.  
                   We assume bytes, but this is clearly wrong. 
                   Meaning this code needs to be carefully reviewed for errors.
                   dmq.
                  */
 
		/* On end-of-str: see comment below. */
		must = utf8_target ? prog->anchored_utf8 : prog->anchored_substr;
		if (must == &PL_sv_undef) {
		    s = (char*)NULL;
		    DEBUG_r(must = prog->anchored_utf8);	/* for debug */
		}
		else
		    s = fbm_instr(
			(unsigned char*)t,
			HOP3(HOP3(last1, prog->anchored_offset, strend)
				+ SvCUR(must), -(SvTAIL(must)!=0), strbeg),
			must,
			multiline ? FBMrf_MULTILINE : 0
		    );
                DEBUG_EXECUTE_r({
                    RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
                        SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
                    PerlIO_printf(Perl_debug_log, "%s anchored substr %s%s",
			(s ? "Found" : "Contradicts"),
                        quoted, RE_SV_TAIL(must));
                });		    
		
			    
		if (!s) {
		    if (last1 >= last2) {
			DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
						", giving up...\n"));
			goto fail_finish;
		    }
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			", trying floating at offset %ld...\n",
			(long)(HOP3c(saved_s, 1, strend) - i_strpos)));
		    other_last = HOP3c(last1, prog->anchored_offset+1, strend);
		    s = HOP3c(last, 1, strend);
		    goto restart;
		}
		else {
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, " at offset %ld...\n",
			  (long)(s - i_strpos)));
		    t = HOP3c(s, -prog->anchored_offset, strbeg);
		    other_last = HOP3c(s, 1, strend);
		    s = saved_s;
		    if (t == strpos)
			goto try_at_start;
		    goto try_at_offset;
		}
	    }
	}
	else {		/* Take into account the floating substring. */
	    char *last, *last1;
	    char * const saved_s = s;
	    SV* must;

	    t = HOP3c(s, -start_shift, strbeg);
	    last1 = last =
		HOP3c(strend, -prog->minlen + prog->float_min_offset, strbeg);
	    if (CHR_DIST((U8*)last, (U8*)t) > prog->float_max_offset)
		last = HOP3c(t, prog->float_max_offset, strend);
	    s = HOP3c(t, prog->float_min_offset, strend);
	    if (s < other_last)
		s = other_last;
 /* XXXX It is not documented what units *_offsets are in.  Assume bytes.  */
	    must = utf8_target ? prog->float_utf8 : prog->float_substr;
	    /* fbm_instr() takes into account exact value of end-of-str
	       if the check is SvTAIL(ed).  Since false positives are OK,
	       and end-of-str is not later than strend we are OK. */
	    if (must == &PL_sv_undef) {
		s = (char*)NULL;
		DEBUG_r(must = prog->float_utf8);	/* for debug message */
	    }
	    else
		s = fbm_instr((unsigned char*)s,
			      (unsigned char*)last + SvCUR(must)
				  - (SvTAIL(must)!=0),
			      must, multiline ? FBMrf_MULTILINE : 0);
	    DEBUG_EXECUTE_r({
	        RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
	            SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
	        PerlIO_printf(Perl_debug_log, "%s floating substr %s%s",
		    (s ? "Found" : "Contradicts"),
		    quoted, RE_SV_TAIL(must));
            });
	    if (!s) {
		if (last1 == last) {
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
					    ", giving up...\n"));
		    goto fail_finish;
		}
		DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		    ", trying anchored starting at offset %ld...\n",
		    (long)(saved_s + 1 - i_strpos)));
		other_last = last;
		s = HOP3c(t, 1, strend);
		goto restart;
	    }
	    else {
		DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, " at offset %ld...\n",
		      (long)(s - i_strpos)));
		other_last = s; /* Fix this later. --Hugo */
		s = saved_s;
		if (t == strpos)
		    goto try_at_start;
		goto try_at_offset;
	    }
	}
    }

    
    t= (char*)HOP3( s, -prog->check_offset_max, (prog->check_offset_max<0) ? strend : strpos);
        
    DEBUG_OPTIMISE_MORE_r(
        PerlIO_printf(Perl_debug_log, 
            "Check offset min:%"IVdf" max:%"IVdf" S:%"IVdf" t:%"IVdf" D:%"IVdf" end:%"IVdf"\n",
            (IV)prog->check_offset_min,
            (IV)prog->check_offset_max,
            (IV)(s-strpos),
            (IV)(t-strpos),
            (IV)(t-s),
            (IV)(strend-strpos)
        )
    );

    if (s - strpos > prog->check_offset_max  /* signed-corrected t > strpos */
        && (!utf8_target
	    || ((t = (char*)reghopmaybe3((U8*)s, -prog->check_offset_max, (U8*) ((prog->check_offset_max<0) ? strend : strpos)))
		 && t > strpos))) 
    {
	/* Fixed substring is found far enough so that the match
	   cannot start at strpos. */
      try_at_offset:
	if (ml_anch && t[-1] != '\n') {
	    /* Eventually fbm_*() should handle this, but often
	       anchored_offset is not 0, so this check will not be wasted. */
	    /* XXXX In the code below we prefer to look for "^" even in
	       presence of anchored substrings.  And we search even
	       beyond the found float position.  These pessimizations
	       are historical artefacts only.  */
	  find_anchor:
	    while (t < strend - prog->minlen) {
		if (*t == '\n') {
		    if (t < check_at - prog->check_offset_min) {
			if (utf8_target ? prog->anchored_utf8 : prog->anchored_substr) {
			    /* Since we moved from the found position,
			       we definitely contradict the found anchored
			       substr.  Due to the above check we do not
			       contradict "check" substr.
			       Thus we can arrive here only if check substr
			       is float.  Redo checking for "other"=="fixed".
			     */
			    strpos = t + 1;			
			    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m at offset %ld, rescanning for anchored from offset %ld...\n",
				PL_colors[0], PL_colors[1], (long)(strpos - i_strpos), (long)(strpos - i_strpos + prog->anchored_offset)));
			    goto do_other_anchored;
			}
			/* We don't contradict the found floating substring. */
			/* XXXX Why not check for STCLASS? */
			s = t + 1;
			DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m at offset %ld...\n",
			    PL_colors[0], PL_colors[1], (long)(s - i_strpos)));
			goto set_useful;
		    }
		    /* Position contradicts check-string */
		    /* XXXX probably better to look for check-string
		       than for "\n", so one should lower the limit for t? */
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m, restarting lookup for check-string at offset %ld...\n",
			PL_colors[0], PL_colors[1], (long)(t + 1 - i_strpos)));
		    other_last = strpos = s = t + 1;
		    goto restart;
		}
		t++;
	    }
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Did not find /%s^%s/m...\n",
			PL_colors[0], PL_colors[1]));
	    goto fail_finish;
	}
	else {
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Starting position does not contradict /%s^%s/m...\n",
			PL_colors[0], PL_colors[1]));
	}
	s = t;
      set_useful:
	++BmUSEFUL(utf8_target ? prog->check_utf8 : prog->check_substr);	/* hooray/5 */
    }
    else {
	/* The found string does not prohibit matching at strpos,
	   - no optimization of calling REx engine can be performed,
	   unless it was an MBOL and we are not after MBOL,
	   or a future STCLASS check will fail this. */
      try_at_start:
	/* Even in this situation we may use MBOL flag if strpos is offset
	   wrt the start of the string. */
	if (ml_anch && sv && !SvROK(sv)	/* See prev comment on SvROK */
	    && (strpos != strbeg) && strpos[-1] != '\n'
	    /* May be due to an implicit anchor of m{.*foo}  */
	    && !(prog->intflags & PREGf_IMPLICIT))
	{
	    t = strpos;
	    goto find_anchor;
	}
	DEBUG_EXECUTE_r( if (ml_anch)
	    PerlIO_printf(Perl_debug_log, "Position at offset %ld does not contradict /%s^%s/m...\n",
			  (long)(strpos - i_strpos), PL_colors[0], PL_colors[1]);
	);
      success_at_start:
	if (!(prog->intflags & PREGf_NAUGHTY)	/* XXXX If strpos moved? */
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
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "... Disabling check substring...\n"));
	    /* XXX Does the destruction order has to change with utf8_target? */
	    SvREFCNT_dec(utf8_target ? prog->check_utf8 : prog->check_substr);
	    SvREFCNT_dec(utf8_target ? prog->check_substr : prog->check_utf8);
	    prog->check_substr = prog->check_utf8 = NULL;	/* disable */
	    prog->float_substr = prog->float_utf8 = NULL;	/* clear */
	    check = NULL;			/* abort */
	    s = strpos;
	    /* XXXX If the check string was an implicit check MBOL, then we need to unset the relevant flag
		    see http://bugs.activestate.com/show_bug.cgi?id=87173 */
	    if (prog->intflags & PREGf_IMPLICIT)
		prog->extflags &= ~RXf_ANCH_MBOL;
	    /* XXXX This is a remnant of the old implementation.  It
	            looks wasteful, since now INTUIT can use many
	            other heuristics. */
	    prog->extflags &= ~RXf_USE_INTUIT;
	    /* XXXX What other flags might need to be cleared in this branch? */
	}
	else
	    s = strpos;
    }

    /* Last resort... */
    /* XXXX BmUSEFUL already changed, maybe multiple change is meaningful... */
    /* trie stclasses are too expensive to use here, we are better off to
       leave it to regmatch itself */
    if (progi->regstclass && PL_regkind[OP(progi->regstclass)]!=TRIE) {
	/* minlen == 0 is possible if regstclass is \b or \B,
	   and the fixed substr is ''$.
	   Since minlen is already taken into account, s+1 is before strend;
	   accidentally, minlen >= 1 guaranties no false positives at s + 1
	   even for \b or \B.  But (minlen? 1 : 0) below assumes that
	   regstclass does not come from lookahead...  */
	/* If regstclass takes bytelength more than 1: If charlength==1, OK.
	   This leaves EXACTF-ish only, which are dealt with in find_byclass().  */
        const U8* const str = (U8*)STRING(progi->regstclass);
        const int cl_l = (PL_regkind[OP(progi->regstclass)] == EXACT
		    ? CHR_DIST(str+STR_LEN(progi->regstclass), str)
		    : 1);
	char * endpos;
	if (prog->anchored_substr || prog->anchored_utf8 || ml_anch)
            endpos= HOP3c(s, (prog->minlen ? cl_l : 0), strend);
        else if (prog->float_substr || prog->float_utf8)
	    endpos= HOP3c(HOP3c(check_at, -start_shift, strbeg), cl_l, strend);
        else 
            endpos= strend;
		    
        if (checked_upto < s)
           checked_upto = s;
        DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "start_shift: %"IVdf" check_at: %"IVdf" s: %"IVdf" endpos: %"IVdf" checked_upto: %"IVdf"\n",
                                      (IV)start_shift, (IV)(check_at - strbeg), (IV)(s - strbeg), (IV)(endpos - strbeg), (IV)(checked_upto- strbeg)));

	t = s;
        s = find_byclass(prog, progi->regstclass, checked_upto, endpos,
                            NULL, is_utf8_pat);
	if (s) {
	    checked_upto = s;
	} else {
#ifdef DEBUGGING
	    const char *what = NULL;
#endif
	    if (endpos == strend) {
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
				"Could not match STCLASS...\n") );
		goto fail;
	    }
	    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
				   "This position contradicts STCLASS...\n") );
	    if ((prog->extflags & RXf_ANCH) && !ml_anch)
		goto fail;
	    checked_upto = HOPBACKc(endpos, start_shift);
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "start_shift: %"IVdf" check_at: %"IVdf" endpos: %"IVdf" checked_upto: %"IVdf"\n",
                                      (IV)start_shift, (IV)(check_at - strbeg), (IV)(endpos - strbeg), (IV)(checked_upto- strbeg)));
	    /* Contradict one of substrings */
	    if (prog->anchored_substr || prog->anchored_utf8) {
		if ((utf8_target ? prog->anchored_utf8 : prog->anchored_substr) == check) {
		    DEBUG_EXECUTE_r( what = "anchored" );
		  hop_and_restart:
		    s = HOP3c(t, 1, strend);
		    if (s + start_shift + end_shift > strend) {
			/* XXXX Should be taken into account earlier? */
			DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
					       "Could not match STCLASS...\n") );
			goto fail;
		    }
		    if (!check)
			goto giveup;
		    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
				"Looking for %s substr starting at offset %ld...\n",
				 what, (long)(s + start_shift - i_strpos)) );
		    goto restart;
		}
		/* Have both, check_string is floating */
		if (t + start_shift >= check_at) /* Contradicts floating=check */
		    goto retry_floating_check;
		/* Recheck anchored substring, but not floating... */
		s = check_at;
		if (!check)
		    goto giveup;
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
			  "Looking for anchored substr starting at offset %ld...\n",
			  (long)(other_last - i_strpos)) );
		goto do_other_anchored;
	    }
	    /* Another way we could have checked stclass at the
               current position only: */
	    if (ml_anch) {
		s = t = t + 1;
		if (!check)
		    goto giveup;
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
			  "Looking for /%s^%s/m starting at offset %ld...\n",
			  PL_colors[0], PL_colors[1], (long)(t - i_strpos)) );
		goto try_at_offset;
	    }
	    if (!(utf8_target ? prog->float_utf8 : prog->float_substr))	/* Could have been deleted */
		goto fail;
	    /* Check is floating substring. */
	  retry_floating_check:
	    t = check_at - start_shift;
	    DEBUG_EXECUTE_r( what = "floating" );
	    goto hop_and_restart;
	}
	if (t != s) {
            DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			"By STCLASS: moving %ld --> %ld\n",
                                  (long)(t - i_strpos), (long)(s - i_strpos))
                   );
        }
        else {
            DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
                                  "Does not contradict STCLASS...\n"); 
                   );
        }
    }
  giveup:
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%s%s:%s match at offset %ld\n",
			  PL_colors[4], (check ? "Guessed" : "Giving up"),
			  PL_colors[5], (long)(s - i_strpos)) );
    return s;

  fail_finish:				/* Substring not found */
    if (prog->check_substr || prog->check_utf8)		/* could be removed already */
	BmUSEFUL(utf8_target ? prog->check_utf8 : prog->check_substr) += 5; /* hooray */
  fail:
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%sMatch rejected by optimizer%s\n",
			  PL_colors[4], PL_colors[5]));
    return NULL;
}

#define DECL_TRIE_TYPE(scan) \
    const enum { trie_plain, trie_utf8, trie_utf8_fold, trie_latin_utf8_fold } \
                    trie_type = ((scan->flags == EXACT) \
                              ? (utf8_target ? trie_utf8 : trie_plain) \
                              : (utf8_target ? trie_utf8_fold : trie_latin_utf8_fold))

#define REXEC_TRIE_READ_CHAR(trie_type, trie, widecharmap, uc, uscan, len, uvc, charid, foldlen, foldbuf, uniflags) \
STMT_START {                               \
    STRLEN skiplen;                                                                 \
    switch (trie_type) {                                                            \
    case trie_utf8_fold:                                                            \
        if ( foldlen>0 ) {                                                          \
            uvc = utf8n_to_uvuni( (const U8*) uscan, UTF8_MAXLEN, &len, uniflags ); \
            foldlen -= len;                                                         \
            uscan += len;                                                           \
            len=0;                                                                  \
        } else {                                                                    \
            uvc = to_utf8_fold( (const U8*) uc, foldbuf, &foldlen );                \
            len = UTF8SKIP(uc);                                                     \
            skiplen = UNISKIP( uvc );                                               \
            foldlen -= skiplen;                                                     \
            uscan = foldbuf + skiplen;                                              \
        }                                                                           \
        break;                                                                      \
    case trie_latin_utf8_fold:                                                      \
        if ( foldlen>0 ) {                                                          \
            uvc = utf8n_to_uvuni( (const U8*) uscan, UTF8_MAXLEN, &len, uniflags ); \
            foldlen -= len;                                                         \
            uscan += len;                                                           \
            len=0;                                                                  \
        } else {                                                                    \
            len = 1;                                                                \
            uvc = _to_fold_latin1( (U8) *uc, foldbuf, &foldlen, 1);                 \
            skiplen = UNISKIP( uvc );                                               \
            foldlen -= skiplen;                                                     \
            uscan = foldbuf + skiplen;                                              \
        }                                                                           \
        break;                                                                      \
    case trie_utf8:                                                                 \
        uvc = utf8n_to_uvuni( (const U8*) uc, UTF8_MAXLEN, &len, uniflags );        \
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

#define REXEC_FBC_EXACTISH_SCAN(CoNd)                     \
STMT_START {                                              \
    while (s <= e) {                                      \
	if ( (CoNd)                                       \
	     && (ln == 1 || folder(s, pat_string, ln))    \
	     && (!reginfo || regtry(reginfo, &s)) )       \
	    goto got_it;                                  \
	s++;                                              \
    }                                                     \
} STMT_END

#define REXEC_FBC_UTF8_SCAN(CoDe)                     \
STMT_START {                                          \
    while (s < strend) {                              \
	CoDe                                          \
	s += UTF8SKIP(s);                             \
    }                                                 \
} STMT_END

#define REXEC_FBC_SCAN(CoDe)                          \
STMT_START {                                          \
    while (s < strend) {                              \
	CoDe                                          \
	s++;                                          \
    }                                                 \
} STMT_END

#define REXEC_FBC_UTF8_CLASS_SCAN(CoNd)               \
REXEC_FBC_UTF8_SCAN(                                  \
    if (CoNd) {                                       \
	if (tmp && (!reginfo || regtry(reginfo, &s))) \
	    goto got_it;                              \
	else                                          \
	    tmp = doevery;                            \
    }                                                 \
    else                                              \
	tmp = 1;                                      \
)

#define REXEC_FBC_CLASS_SCAN(CoNd)                    \
REXEC_FBC_SCAN(                                       \
    if (CoNd) {                                       \
	if (tmp && (!reginfo || regtry(reginfo, &s)))  \
	    goto got_it;                              \
	else                                          \
	    tmp = doevery;                            \
    }                                                 \
    else                                              \
	tmp = 1;                                      \
)

#define REXEC_FBC_TRYIT               \
if ((!reginfo || regtry(reginfo, &s))) \
    goto got_it

#define REXEC_FBC_CSCAN(CoNdUtF8,CoNd)                         \
    if (utf8_target) {                                             \
	REXEC_FBC_UTF8_CLASS_SCAN(CoNdUtF8);                   \
    }                                                          \
    else {                                                     \
	REXEC_FBC_CLASS_SCAN(CoNd);                            \
    }
    
#define DUMP_EXEC_POS(li,s,doutf8) \
    dump_exec_pos(li,s,(PL_regeol),(PL_bostr),(PL_reg_starttry),doutf8)


#define UTF8_NOLOAD(TEST_NON_UTF8, IF_SUCCESS, IF_FAIL) \
	tmp = (s != PL_bostr) ? UCHARAT(s - 1) : '\n';                         \
	tmp = TEST_NON_UTF8(tmp);                                              \
	REXEC_FBC_UTF8_SCAN(                                                   \
	    if (tmp == ! TEST_NON_UTF8((U8) *s)) { \
		tmp = !tmp;                                                    \
		IF_SUCCESS;                                                    \
	    }                                                                  \
	    else {                                                             \
		IF_FAIL;                                                       \
	    }                                                                  \
	);                                                                     \

#define UTF8_LOAD(TeSt1_UtF8, TeSt2_UtF8, IF_SUCCESS, IF_FAIL) \
	if (s == PL_bostr) {                                                   \
	    tmp = '\n';                                                        \
	}                                                                      \
	else {                                                                 \
	    U8 * const r = reghop3((U8*)s, -1, (U8*)PL_bostr);                 \
	    tmp = utf8n_to_uvchr(r, UTF8SKIP(r), 0, UTF8_ALLOW_DEFAULT);       \
	}                                                                      \
	tmp = TeSt1_UtF8;                                                      \
	LOAD_UTF8_CHARCLASS_ALNUM();                                                                \
	REXEC_FBC_UTF8_SCAN(                                                   \
	    if (tmp == ! (TeSt2_UtF8)) { \
		tmp = !tmp;                                                    \
		IF_SUCCESS;                                                    \
	    }                                                                  \
	    else {                                                             \
		IF_FAIL;                                                       \
	    }                                                                  \
	);                                                                     \

/* The only difference between the BOUND and NBOUND cases is that
 * REXEC_FBC_TRYIT is called when matched in BOUND, and when non-matched in
 * NBOUND.  This is accomplished by passing it in either the if or else clause,
 * with the other one being empty */
#define FBC_BOUND(TEST_NON_UTF8, TEST1_UTF8, TEST2_UTF8) \
    FBC_BOUND_COMMON(UTF8_LOAD(TEST1_UTF8, TEST2_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER), TEST_NON_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER)

#define FBC_BOUND_NOLOAD(TEST_NON_UTF8, TEST1_UTF8, TEST2_UTF8) \
    FBC_BOUND_COMMON(UTF8_NOLOAD(TEST_NON_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER), TEST_NON_UTF8, REXEC_FBC_TRYIT, PLACEHOLDER)

#define FBC_NBOUND(TEST_NON_UTF8, TEST1_UTF8, TEST2_UTF8) \
    FBC_BOUND_COMMON(UTF8_LOAD(TEST1_UTF8, TEST2_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT), TEST_NON_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT)

#define FBC_NBOUND_NOLOAD(TEST_NON_UTF8, TEST1_UTF8, TEST2_UTF8) \
    FBC_BOUND_COMMON(UTF8_NOLOAD(TEST_NON_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT), TEST_NON_UTF8, PLACEHOLDER, REXEC_FBC_TRYIT)


/* Common to the BOUND and NBOUND cases.  Unfortunately the UTF8 tests need to
 * be passed in completely with the variable name being tested, which isn't
 * such a clean interface, but this is easier to read than it was before.  We
 * are looking for the boundary (or non-boundary between a word and non-word
 * character.  The utf8 and non-utf8 cases have the same logic, but the details
 * must be different.  Find the "wordness" of the character just prior to this
 * one, and compare it with the wordness of this one.  If they differ, we have
 * a boundary.  At the beginning of the string, pretend that the previous
 * character was a new-line */
#define FBC_BOUND_COMMON(UTF8_CODE, TEST_NON_UTF8, IF_SUCCESS, IF_FAIL) \
    if (utf8_target) {                                                         \
		UTF8_CODE \
    }                                                                          \
    else {  /* Not utf8 */                                                     \
	tmp = (s != PL_bostr) ? UCHARAT(s - 1) : '\n';                         \
	tmp = TEST_NON_UTF8(tmp);                                              \
	REXEC_FBC_SCAN(                                                        \
	    if (tmp == ! TEST_NON_UTF8((U8) *s)) {                             \
		tmp = !tmp;                                                    \
		IF_SUCCESS;                                                    \
	    }                                                                  \
	    else {                                                             \
		IF_FAIL;                                                       \
	    }                                                                  \
	);                                                                     \
    }                                                                          \
    if ((!prog->minlen && tmp) && (!reginfo || regtry(reginfo, &s)))           \
	goto got_it;

/* We know what class REx starts with.  Try to find this position... */
/* if reginfo is NULL, its a dryrun */
/* annoyingly all the vars in this routine have different names from their counterparts
   in regmatch. /grrr */

STATIC char *
S_find_byclass(pTHX_ regexp * prog, const regnode *c, char *s, 
    const char *strend, regmatch_info *reginfo, bool is_utf8_pat)
{
    dVAR;
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
    I32 tmp = 1;	/* Scratch variable? */
    const bool utf8_target = PL_reg_match_utf8;
    UV utf8_fold_flags = 0;
    bool to_complement = FALSE; /* Invert the result?  Taking the xor of this
                                   with a result inverts that result, as 0^1 =
                                   1 and 1^1 = 0 */
    _char_class_number classnum;

    RXi_GET_DECL(prog,progi);

    PERL_ARGS_ASSERT_FIND_BYCLASS;

    /* We know what class it must start with. */
    switch (OP(c)) {
    case ANYOF:
    case ANYOF_SYNTHETIC:
    case ANYOF_WARN_SUPER:
        if (utf8_target) {
            REXEC_FBC_UTF8_CLASS_SCAN(
                      reginclass(prog, c, (U8*)s, utf8_target));
        }
        else {
            REXEC_FBC_CLASS_SCAN(REGINCLASS(prog, c, (U8*)s));
        }
        break;
    case CANY:
        REXEC_FBC_SCAN(
            if (tmp && (!reginfo || regtry(reginfo, &s)))
                goto got_it;
            else
                tmp = doevery;
        );
        break;

    case EXACTFA:
        if (is_utf8_pat || utf8_target) {
            utf8_fold_flags = FOLDEQ_UTF8_NOMIX_ASCII;
            goto do_exactf_utf8;
        }
        fold_array = PL_fold_latin1;    /* Latin1 folds are not affected by */
        folder = foldEQ_latin1;	        /* /a, except the sharp s one which */
        goto do_exactf_non_utf8;	/* isn't dealt with by these */

    case EXACTF:
        if (utf8_target) {

            /* regcomp.c already folded this if pattern is in UTF-8 */
            utf8_fold_flags = 0;
            goto do_exactf_utf8;
        }
        fold_array = PL_fold;
        folder = foldEQ;
        goto do_exactf_non_utf8;

    case EXACTFL:
        if (is_utf8_pat || utf8_target) {
            utf8_fold_flags = FOLDEQ_UTF8_LOCALE;
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

    case EXACTFU_TRICKYFOLD:
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

        /* FALL THROUGH */

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
        e = HOP3c(strend, -((I32)ln), s);

        if (!reginfo && e < s) {
            e = s;			/* Due to minlen logic of intuit() */
        }

        c1 = *pat_string;
        c2 = fold_array[c1];
        if (c1 == c2) { /* If char and fold are the same */
            REXEC_FBC_EXACTISH_SCAN(*(U8*)s == c1);
        }
        else {
            REXEC_FBC_EXACTISH_SCAN(*(U8*)s == c1 || *(U8*)s == c2);
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
        e = HOP3c(strend, -((I32)lnc), s);

        if (!reginfo && e < s) {
            e = s;			/* Due to minlen logic of intuit() */
        }

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
                && (!reginfo || regtry(reginfo, &s)) )
            {
                goto got_it;
            }
            s += (utf8_target) ? UTF8SKIP(s) : 1;
        }
        break;
    }
    case BOUNDL:
        RXp_MATCH_TAINTED_on(prog);
        FBC_BOUND(isWORDCHAR_LC,
                  isWORDCHAR_LC_uvchr(UNI_TO_NATIVE(tmp)),
                  isWORDCHAR_LC_utf8((U8*)s));
        break;
    case NBOUNDL:
        RXp_MATCH_TAINTED_on(prog);
        FBC_NBOUND(isWORDCHAR_LC,
                   isWORDCHAR_LC_uvchr(UNI_TO_NATIVE(tmp)),
                   isWORDCHAR_LC_utf8((U8*)s));
        break;
    case BOUND:
        FBC_BOUND(isWORDCHAR,
                  isWORDCHAR_uni(tmp),
                  cBOOL(swash_fetch(PL_utf8_swash_ptrs[_CC_WORDCHAR], (U8*)s, utf8_target)));
        break;
    case BOUNDA:
        FBC_BOUND_NOLOAD(isWORDCHAR_A,
                         isWORDCHAR_A(tmp),
                         isWORDCHAR_A((U8*)s));
        break;
    case NBOUND:
        FBC_NBOUND(isWORDCHAR,
                   isWORDCHAR_uni(tmp),
                   cBOOL(swash_fetch(PL_utf8_swash_ptrs[_CC_WORDCHAR], (U8*)s, utf8_target)));
        break;
    case NBOUNDA:
        FBC_NBOUND_NOLOAD(isWORDCHAR_A,
                          isWORDCHAR_A(tmp),
                          isWORDCHAR_A((U8*)s));
        break;
    case BOUNDU:
        FBC_BOUND(isWORDCHAR_L1,
                  isWORDCHAR_uni(tmp),
                  cBOOL(swash_fetch(PL_utf8_swash_ptrs[_CC_WORDCHAR], (U8*)s, utf8_target)));
        break;
    case NBOUNDU:
        FBC_NBOUND(isWORDCHAR_L1,
                   isWORDCHAR_uni(tmp),
                   cBOOL(swash_fetch(PL_utf8_swash_ptrs[_CC_WORDCHAR], (U8*)s, utf8_target)));
        break;
    case LNBREAK:
        REXEC_FBC_CSCAN(is_LNBREAK_utf8_safe(s, strend),
                        is_LNBREAK_latin1_safe(s, strend)
        );
        break;

    /* The argument to all the POSIX node types is the class number to pass to
     * _generic_isCC() to build a mask for searching in PL_charclass[] */

    case NPOSIXL:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXL:
        RXp_MATCH_TAINTED_on(prog);
        REXEC_FBC_CSCAN(to_complement ^ cBOOL(isFOO_utf8_lc(FLAGS(c), (U8 *) s)),
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
             * UTF-8 variant code points, plus everything in ASCII that isn't
             * in the class */
            REXEC_FBC_UTF8_CLASS_SCAN(! UTF8_IS_INVARIANT(*s)
                                      || ! _generic_isCC_A(*s, FLAGS(c)));
            break;
        }

        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXA:
      posixa:
        /* Don't need to worry about utf8, as it can match only a single
         * byte invariant character. */
        REXEC_FBC_CLASS_SCAN(
                        to_complement ^ cBOOL(_generic_isCC_A(*s, FLAGS(c))));
        break;

    case NPOSIXU:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXU:
        if (! utf8_target) {
            REXEC_FBC_CLASS_SCAN(to_complement ^ cBOOL(_generic_isCC(*s,
                                                                    FLAGS(c))));
        }
        else {

      posix_utf8:
            classnum = (_char_class_number) FLAGS(c);
            if (classnum < _FIRST_NON_SWASH_CC) {
                while (s < strend) {

                    /* We avoid loading in the swash as long as possible, but
                     * should we have to, we jump to a separate loop.  This
                     * extra 'if' statement is what keeps this code from being
                     * just a call to REXEC_FBC_UTF8_CLASS_SCAN() */
                    if (UTF8_IS_ABOVE_LATIN1(*s)) {
                        goto found_above_latin1;
                    }
                    if ((UTF8_IS_INVARIANT(*s)
                         && to_complement ^ cBOOL(_generic_isCC((U8) *s,
                                                                classnum)))
                        || (UTF8_IS_DOWNGRADEABLE_START(*s)
                            && to_complement ^ cBOOL(
                                _generic_isCC(TWO_BYTE_UTF8_TO_UNI(*s, *(s + 1)),
                                              classnum))))
                    {
                        if (tmp && (!reginfo || regtry(reginfo, &s)))
                            goto got_it;
                        else {
                            tmp = doevery;
                        }
                    }
                    else {
                        tmp = 1;
                    }
                    s += UTF8SKIP(s);
                }
            }
            else switch (classnum) {    /* These classes are implemented as
                                           macros */
                case _CC_ENUM_SPACE: /* XXX would require separate code if we
                                        revert the change of \v matching this */
                    /* FALL THROUGH */

                case _CC_ENUM_PSXSPC:
                    REXEC_FBC_UTF8_CLASS_SCAN(
                                        to_complement ^ cBOOL(isSPACE_utf8(s)));
                    break;

                case _CC_ENUM_BLANK:
                    REXEC_FBC_UTF8_CLASS_SCAN(
                                        to_complement ^ cBOOL(isBLANK_utf8(s)));
                    break;

                case _CC_ENUM_XDIGIT:
                    REXEC_FBC_UTF8_CLASS_SCAN(
                                       to_complement ^ cBOOL(isXDIGIT_utf8(s)));
                    break;

                case _CC_ENUM_VERTSPACE:
                    REXEC_FBC_UTF8_CLASS_SCAN(
                                       to_complement ^ cBOOL(isVERTWS_utf8(s)));
                    break;

                case _CC_ENUM_CNTRL:
                    REXEC_FBC_UTF8_CLASS_SCAN(
                                        to_complement ^ cBOOL(isCNTRL_utf8(s)));
                    break;

                default:
                    Perl_croak(aTHX_ "panic: find_byclass() node %d='%s' has an unexpected character class '%d'", OP(c), PL_reg_name[OP(c)], classnum);
                    assert(0); /* NOTREACHED */
            }
        }
        break;

      found_above_latin1:   /* Here we have to load a swash to get the result
                               for the current code point */
        if (! PL_utf8_swash_ptrs[classnum]) {
            U8 flags = _CORE_SWASH_INIT_ACCEPT_INVLIST;
            PL_utf8_swash_ptrs[classnum] =
                    _core_swash_init("utf8", swash_property_names[classnum],
                                     &PL_sv_undef, 1, 0, NULL, &flags);
        }

        /* This is a copy of the loop above for swash classes, though using the
         * FBC macro instead of being expanded out.  Since we've loaded the
         * swash, we don't have to check for that each time through the loop */
        REXEC_FBC_UTF8_CLASS_SCAN(
                to_complement ^ cBOOL(_generic_utf8(
                                      classnum,
                                      s,
                                      swash_fetch(PL_utf8_swash_ptrs[classnum],
                                                  (U8 *) s, TRUE))));
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
                                        (char *)uc, utf8_target );
                                    PerlIO_printf( Perl_debug_log,
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
                        REXEC_TRIE_READ_CHAR(trie_type, trie,
                                         widecharmap, uc,
                                         uscan, len, uvc, charid, foldlen,
                                         foldbuf, uniflags);
                        DEBUG_TRIE_EXECUTE_r({
                            dump_exec_pos( (char *)uc, c, strend,
                                        real_start, s, utf8_target);
                            PerlIO_printf(Perl_debug_log,
                                " Charid:%3u CP:%4"UVxf" ",
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
                                    s,   utf8_target );
                            PerlIO_printf( Perl_debug_log,
                                "%sState: %4"UVxf", word=%"UVxf,
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
                                    PerlIO_printf( Perl_debug_log," - legal\n"));
                                state = tmp;
                                break;
                            }
                            else {
                                DEBUG_TRIE_EXECUTE_r(
                                    PerlIO_printf( Perl_debug_log," - fail\n"));
                                failed = 1;
                                state = aho->fail[state];
                            }
                        }
                        else {
                            /* we must be accepting here */
                            DEBUG_TRIE_EXECUTE_r(
                                    PerlIO_printf( Perl_debug_log," - accepting\n"));
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
                        PerlIO_printf(
                            Perl_debug_log,"Matches word #%"UVxf" at position %"IVdf". Trying full pattern...\n",
                            (UV)accepted_word, (IV)(s - real_start)
                        );
                    });
                    if (!reginfo || regtry(reginfo, &s)) {
                        FREETMPS;
                        LEAVE;
                        goto got_it;
                    }
                    s = HOPc(s,1);
                    DEBUG_TRIE_EXECUTE_r({
                        PerlIO_printf( Perl_debug_log,"Pattern failed. Looking for new start point...\n");
                    });
                } else {
                    DEBUG_TRIE_EXECUTE_r(
                        PerlIO_printf( Perl_debug_log,"No match.\n"));
                    break;
                }
            }
            FREETMPS;
            LEAVE;
        }
        break;
    default:
        Perl_croak(aTHX_ "panic: unknown regstclass %d", (int)OP(c));
        break;
    }
    return 0;
  got_it:
    return s;
}


/*
 - regexec_flags - match a regexp against a string
 */
I32
Perl_regexec_flags(pTHX_ REGEXP * const rx, char *stringarg, char *strend,
	      char *strbeg, I32 minend, SV *sv, void *data, U32 flags)
/* stringarg: the point in the string at which to begin matching */
/* strend:    pointer to null at end of string */
/* strbeg:    real beginning of string */
/* minend:    end of match must be >= minend bytes after stringarg. */
/* sv:        SV being matched: only used for utf8 flag, pos() etc; string
 *            itself is accessed via the pointers above */
/* data:      May be used for some additional optimizations.
              Currently its only used, with a U32 cast, for transmitting
              the ganch offset when doing a /g match. This will change */
/* nosave:    For optimizations. */

{
    dVAR;
    struct regexp *const prog = ReANY(rx);
    char *s;
    regnode *c;
    char *startpos = stringarg;
    I32 minlen;		/* must match at least this many chars */
    I32 dontbother = 0;	/* how many characters not to try at end */
    I32 end_shift = 0;			/* Same for the end. */		/* CC */
    I32 scream_pos = -1;		/* Internal iterator of scream. */
    char *scream_olds = NULL;
    const bool utf8_target = cBOOL(DO_UTF8(sv));
    I32 multiline;
    RXi_GET_DECL(prog,progi);
    regmatch_info reginfo;  /* create some info to pass to regtry etc */
    regexp_paren_pair *swap = NULL;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGEXEC_FLAGS;
    PERL_UNUSED_ARG(data);

    /* Be paranoid... */
    if (prog == NULL || startpos == NULL) {
	Perl_croak(aTHX_ "NULL regexp parameter");
	return 0;
    }

    multiline = prog->extflags & RXf_PMf_MULTILINE;
    reginfo.prog = rx;	 /* Yes, sorry that this is confusing.  */

    RX_MATCH_UTF8_set(rx, utf8_target);
    DEBUG_EXECUTE_r( 
        debug_start_match(rx, utf8_target, startpos, strend,
        "Matching");
    );

    minlen = prog->minlen;
    
    if (strend - startpos < (minlen+(prog->check_offset_min<0?prog->check_offset_min:0))) {
        DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			      "String too short [regexec_flags]...\n"));
	goto phooey;
    }

    
    /* Check validity of program. */
    if (UCHARAT(progi->program) != REG_MAGIC) {
	Perl_croak(aTHX_ "corrupted regexp program");
    }

    RX_MATCH_TAINTED_off(rx);
    PL_reg_state.re_state_eval_setup_done = FALSE;
    PL_reg_maxiter = 0;

    reginfo.is_utf8_pat = cBOOL(RX_UTF8(rx));
    reginfo.warned = FALSE;
    /* Mark beginning of line for ^ and lookbehind. */
    reginfo.bol = startpos; /* XXX not used ??? */
    PL_bostr  = strbeg;
    reginfo.sv = sv;

    /* Mark end of line for $ (and such) */
    PL_regeol = strend;

    /* see how far we have to get to not match where we matched before */
    reginfo.till = startpos+minend;

    /* If there is a "must appear" string, look for it. */
    s = startpos;

    if (prog->extflags & RXf_GPOS_SEEN) { /* Need to set reginfo->ganch */
	MAGIC *mg;
	if (flags & REXEC_IGNOREPOS){	/* Means: check only at start */
	    reginfo.ganch = startpos + prog->gofs;
	    DEBUG_GPOS_r(PerlIO_printf(Perl_debug_log,
	      "GPOS IGNOREPOS: reginfo.ganch = startpos + %"UVxf"\n",(UV)prog->gofs));
	} else if (sv && SvTYPE(sv) >= SVt_PVMG
		  && SvMAGIC(sv)
		  && (mg = mg_find(sv, PERL_MAGIC_regex_global))
		  && mg->mg_len >= 0) {
	    reginfo.ganch = strbeg + mg->mg_len;	/* Defined pos() */
	    DEBUG_GPOS_r(PerlIO_printf(Perl_debug_log,
		"GPOS MAGIC: reginfo.ganch = strbeg + %"IVdf"\n",(IV)mg->mg_len));

	    if (prog->extflags & RXf_ANCH_GPOS) {
	        if (s > reginfo.ganch)
		    goto phooey;
		s = reginfo.ganch - prog->gofs;
	        DEBUG_GPOS_r(PerlIO_printf(Perl_debug_log,
		     "GPOS ANCH_GPOS: s = ganch - %"UVxf"\n",(UV)prog->gofs));
		if (s < strbeg)
		    goto phooey;
	    }
	}
	else if (data) {
	    reginfo.ganch = strbeg + PTR2UV(data);
            DEBUG_GPOS_r(PerlIO_printf(Perl_debug_log,
		 "GPOS DATA: reginfo.ganch= strbeg + %"UVxf"\n",PTR2UV(data)));

	} else {				/* pos() not defined */
	    reginfo.ganch = strbeg;
            DEBUG_GPOS_r(PerlIO_printf(Perl_debug_log,
		 "GPOS: reginfo.ganch = strbeg\n"));
	}
    }
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
	DEBUG_BUFFERS_r(PerlIO_printf(Perl_debug_log,
	    "rex=0x%"UVxf" saving  offs: orig=0x%"UVxf" new=0x%"UVxf"\n",
	    PTR2UV(prog),
	    PTR2UV(swap),
	    PTR2UV(prog->offs)
	));
    }
    if (!(flags & REXEC_CHECKED) && (prog->check_substr != NULL || prog->check_utf8 != NULL)) {
	re_scream_pos_data d;

	d.scream_olds = &scream_olds;
	d.scream_pos = &scream_pos;
	s = re_intuit_start(rx, sv, s, strend, flags, &d);
	if (!s) {
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Not present...\n"));
	    goto phooey;	/* not present */
	}
    }



    /* Simplest case:  anchored match need be tried only once. */
    /*  [unless only anchor is BOL and multiline is set] */
    if (prog->extflags & (RXf_ANCH & ~RXf_ANCH_GPOS)) {
	if (s == startpos && regtry(&reginfo, &startpos))
	    goto got_it;
	else if (multiline || (prog->intflags & PREGf_IMPLICIT)
		 || (prog->extflags & RXf_ANCH_MBOL)) /* XXXX SBOL? */
	{
	    char *end;

	    if (minlen)
		dontbother = minlen - 1;
	    end = HOP3c(strend, -dontbother, strbeg) - 1;
	    /* for multiline we only have to try after newlines */
	    if (prog->check_substr || prog->check_utf8) {
                /* because of the goto we can not easily reuse the macros for bifurcating the
                   unicode/non-unicode match modes here like we do elsewhere - demerphq */
                if (utf8_target) {
                    if (s == startpos)
                        goto after_try_utf8;
                    while (1) {
                        if (regtry(&reginfo, &s)) {
                            goto got_it;
                        }
                      after_try_utf8:
                        if (s > end) {
                            goto phooey;
                        }
                        if (prog->extflags & RXf_USE_INTUIT) {
                            s = re_intuit_start(rx, sv, s + UTF8SKIP(s), strend, flags, NULL);
                            if (!s) {
                                goto phooey;
                            }
                        }
                        else {
                            s += UTF8SKIP(s);
                        }
                    }
                } /* end search for check string in unicode */
                else {
                    if (s == startpos) {
                        goto after_try_latin;
                    }
                    while (1) {
                        if (regtry(&reginfo, &s)) {
                            goto got_it;
                        }
                      after_try_latin:
                        if (s > end) {
                            goto phooey;
                        }
                        if (prog->extflags & RXf_USE_INTUIT) {
                            s = re_intuit_start(rx, sv, s + 1, strend, flags, NULL);
                            if (!s) {
                                goto phooey;
                            }
                        }
                        else {
                            s++;
                        }
                    }
                } /* end search for check string in latin*/
	    } /* end search for check string */
	    else { /* search for newline */
		if (s > startpos) {
                    /*XXX: The s-- is almost definitely wrong here under unicode - demeprhq*/
		    s--;
		}
		/* We can use a more efficient search as newlines are the same in unicode as they are in latin */
		while (s <= end) { /* note it could be possible to match at the end of the string */
		    if (*s++ == '\n') {	/* don't need PL_utf8skip here */
			if (regtry(&reginfo, &s))
			    goto got_it;
		    }
		}
	    } /* end search for newline */
	} /* end anchored/multiline check string search */
	goto phooey;
    } else if (RXf_GPOS_CHECK == (prog->extflags & RXf_GPOS_CHECK)) 
    {
        /* the warning about reginfo.ganch being used without initialization
           is bogus -- we set it above, when prog->extflags & RXf_GPOS_SEEN 
           and we only enter this block when the same bit is set. */
        char *tmp_s = reginfo.ganch - prog->gofs;

	if (tmp_s >= strbeg && regtry(&reginfo, &tmp_s))
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
	    REXEC_FBC_SCAN(
		if (*s == ch) {
		    DEBUG_EXECUTE_r( did_match = 1 );
		    if (regtry(&reginfo, &s)) goto got_it;
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
	    REXEC_FBC_SCAN(
		if (*s == ch) {
		    DEBUG_EXECUTE_r( did_match = 1 );
		    if (regtry(&reginfo, &s)) goto got_it;
		    s++;
		    while (s < strend && *s == ch)
			s++;
		}
	    );
	}
	DEBUG_EXECUTE_r(if (!did_match)
		PerlIO_printf(Perl_debug_log,
                                  "Did not find anchored character...\n")
               );
    }
    else if (prog->anchored_substr != NULL
	      || prog->anchored_utf8 != NULL
	      || ((prog->float_substr != NULL || prog->float_utf8 != NULL)
		  && prog->float_max_offset < strend - s)) {
	SV *must;
	I32 back_max;
	I32 back_min;
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
        	  -(I32)(CHR_SVLEN(must)
        		 - (SvTAIL(must) != 0) + back_min), strbeg);
        }
	if (s > PL_bostr)
	    last1 = HOPc(s, -1);
	else
	    last1 = s - 1;	/* bogus */

	/* XXXX check_substr already used to find "s", can optimize if
	   check_substr==must. */
	scream_pos = -1;
	dontbother = end_shift;
	strend = HOPc(strend, -dontbother);
	while ( (s <= last) &&
		(s = fbm_instr((unsigned char*)HOP3(s, back_min, (back_min<0 ? strbeg : strend)),
				  (unsigned char*)strend, must,
				  multiline ? FBMrf_MULTILINE : 0)) ) {
	    DEBUG_EXECUTE_r( did_match = 1 );
	    if (HOPc(s, -back_max) > last1) {
		last1 = HOPc(s, -back_min);
		s = HOPc(s, -back_max);
	    }
	    else {
		char * const t = (last1 >= PL_bostr) ? HOPc(last1, 1) : last1 + 1;

		last1 = HOPc(s, -back_min);
		s = t;
	    }
	    if (utf8_target) {
		while (s <= last1) {
		    if (regtry(&reginfo, &s))
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
		    if (regtry(&reginfo, &s))
			goto got_it;
		    s++;
		}
	    }
	}
	DEBUG_EXECUTE_r(if (!did_match) {
            RE_PV_QUOTED_DECL(quoted, utf8_target, PERL_DEBUG_PAD_ZERO(0),
                SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
            PerlIO_printf(Perl_debug_log, "Did not find %s substr %s%s...\n",
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
	    if (PL_regkind[op] != EXACT && op != CANY && PL_regkind[op] != TRIE)
	        strend = HOPc(strend, -(minlen - 1));
	}
	DEBUG_EXECUTE_r({
	    SV * const prop = sv_newmortal();
	    regprop(prog, prop, c);
	    {
		RE_PV_QUOTED_DECL(quoted,utf8_target,PERL_DEBUG_PAD_ZERO(1),
		    s,strend-s,60);
		PerlIO_printf(Perl_debug_log,
		    "Matching stclass %.*s against %s (%d bytes)\n",
		    (int)SvCUR(prop), SvPVX_const(prop),
		     quoted, (int)(strend - s));
	    }
	});
        if (find_byclass(prog, c, s, strend, &reginfo, reginfo.is_utf8_pat))
	    goto got_it;
	DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Contradicts stclass... [regexec_flags]\n"));
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
			PerlIO_printf(Perl_debug_log,
			    "%sChecking for float_real.%s\n",
			    PL_colors[4], PL_colors[5]));
		    if (checkpos + 1 < strbeg) {
                        /* can't match, even if we remove the trailing \n
                         * string is too short to match */
			DEBUG_EXECUTE_r(
			    PerlIO_printf(Perl_debug_log,
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
			    PerlIO_printf(Perl_debug_log,
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
				PerlIO_printf(Perl_debug_log,
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
		    PerlIO_printf(Perl_debug_log,
			"String does not contain required substring, cannot match.\n"
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
		if (regtry(&reginfo, &s))
		    goto got_it;
		if (s >= strend)
		    break;
		s += UTF8SKIP(s);
	    };
	}
	else {
	    do {
		if (regtry(&reginfo, &s))
		    goto got_it;
	    } while (s++ < strend);
	}
    }

    /* Failure. */
    goto phooey;

got_it:
    DEBUG_BUFFERS_r(
	if (swap)
	    PerlIO_printf(Perl_debug_log,
		"rex=0x%"UVxf" freeing offs: 0x%"UVxf"\n",
		PTR2UV(prog),
		PTR2UV(swap)
	    );
    );
    Safefree(swap);

    if (PL_reg_state.re_state_eval_setup_done)
	restore_pos(aTHX_ prog);
    if (RXp_PAREN_NAMES(prog)) 
        (void)hv_iterinit(RXp_PAREN_NAMES(prog));

    /* make sure $`, $&, $', and $digit will work later */
    if ( !(flags & REXEC_NOT_FIRST) ) {
	if (flags & REXEC_COPY_STR) {
#ifdef PERL_ANY_COW
	    if (SvCANCOW(sv)) {
		if (DEBUG_C_TEST) {
		    PerlIO_printf(Perl_debug_log,
				  "Copy on write: regexp capture, type %d\n",
				  (int) SvTYPE(sv));
		}
                RX_MATCH_COPY_FREE(rx);
		prog->saved_copy = sv_setsv_cow(prog->saved_copy, sv);
		prog->subbeg = (char *)SvPVX_const(prog->saved_copy);
		assert (SvPOKp(prog->saved_copy));
                prog->sublen  = PL_regeol - strbeg;
                prog->suboffset = 0;
                prog->subcoffset = 0;
	    } else
#endif
	    {
                I32 min = 0;
                I32 max = PL_regeol - strbeg;
                I32 sublen;

                if (    (flags & REXEC_COPY_SKIP_POST)
                    && !(RX_EXTFLAGS(rx) & RXf_PMf_KEEPCOPY) /* //p */
                    && !(PL_sawampersand & SAWAMPERSAND_RIGHT)
                ) { /* don't copy $' part of string */
                    U32 n = 0;
                    max = -1;
                    /* calculate the right-most part of the string covered
                     * by a capture. Due to look-ahead, this may be to
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
                    assert(max >= 0 && max <= PL_regeol - strbeg);
                }

                if (    (flags & REXEC_COPY_SKIP_PRE)
                    && !(RX_EXTFLAGS(rx) & RXf_PMf_KEEPCOPY) /* //p */
                    && !(PL_sawampersand & SAWAMPERSAND_LEFT)
                ) { /* don't copy $` part of string */
                    U32 n = 0;
                    min = max;
                    /* calculate the left-most part of the string covered
                     * by a capture. Due to look-behind, this may be to
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

                assert(min >= 0 && min <= max && min <= PL_regeol - strbeg);
                sublen = max - min;

                if (RX_MATCH_COPIED(rx)) {
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
                RX_MATCH_COPIED_on(rx);
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
                    sv_pos_b2u(sv, &(prog->subcoffset));
                else
                    prog->subcoffset = utf8_length((U8*)strbeg,
                                        (U8*)(strbeg+prog->suboffset));
            }
	}
	else {
            RX_MATCH_COPY_FREE(rx);
	    prog->subbeg = strbeg;
	    prog->suboffset = 0;
	    prog->subcoffset = 0;
	    prog->sublen = PL_regeol - strbeg;	/* strend may have been modified */
	}
    }

    return 1;

phooey:
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%sMatch failed%s\n",
			  PL_colors[4], PL_colors[5]));
    if (PL_reg_state.re_state_eval_setup_done)
	restore_pos(aTHX_ prog);
    if (swap) {
        /* we failed :-( roll it back */
	DEBUG_BUFFERS_r(PerlIO_printf(Perl_debug_log,
	    "rex=0x%"UVxf" rolling back offs: freeing=0x%"UVxf" restoring=0x%"UVxf"\n",
	    PTR2UV(prog),
	    PTR2UV(prog->offs),
	    PTR2UV(swap)
	));
        Safefree(prog->offs);
        prog->offs = swap;
    }
    return 0;
}


/* Set which rex is pointed to by PL_reg_state, handling ref counting.
 * Do inc before dec, in case old and new rex are the same */
#define SET_reg_curpm(Re2) \
    if (PL_reg_state.re_state_eval_setup_done) {    \
	(void)ReREFCNT_inc(Re2);		    \
	ReREFCNT_dec(PM_GETRE(PL_reg_curpm));	    \
	PM_SETRE((PL_reg_curpm), (Re2));	    \
    }


/*
 - regtry - try match at specific point
 */
STATIC I32			/* 0 failure, 1 success */
S_regtry(pTHX_ regmatch_info *reginfo, char **startposp)
{
    dVAR;
    CHECKPOINT lastcp;
    REGEXP *const rx = reginfo->prog;
    regexp *const prog = ReANY(rx);
    I32 result;
    RXi_GET_DECL(prog,progi);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTRY;

    reginfo->cutpoint=NULL;

    if ((prog->extflags & RXf_EVAL_SEEN)
	&& !PL_reg_state.re_state_eval_setup_done)
    {
	MAGIC *mg;

	PL_reg_state.re_state_eval_setup_done = TRUE;
	if (reginfo->sv) {
	    /* Make $_ available to executed code. */
	    if (reginfo->sv != DEFSV) {
		SAVE_DEFSV;
		DEFSV_set(reginfo->sv);
	    }
	
	    if (!(SvTYPE(reginfo->sv) >= SVt_PVMG && SvMAGIC(reginfo->sv)
		  && (mg = mg_find(reginfo->sv, PERL_MAGIC_regex_global)))) {
		/* prepare for quick setting of pos */
#ifdef PERL_OLD_COPY_ON_WRITE
		if (SvIsCOW(reginfo->sv))
		    sv_force_normal_flags(reginfo->sv, 0);
#endif
		mg = sv_magicext(reginfo->sv, NULL, PERL_MAGIC_regex_global,
				 &PL_vtbl_mglob, NULL, 0);
		mg->mg_len = -1;
	    }
	    PL_reg_magic    = mg;
	    PL_reg_oldpos   = mg->mg_len;
	    SAVEDESTRUCTOR_X(restore_pos, prog);
        }
        if (!PL_reg_curpm) {
	    Newxz(PL_reg_curpm, 1, PMOP);
#ifdef USE_ITHREADS
            {
		SV* const repointer = &PL_sv_undef;
                /* this regexp is also owned by the new PL_reg_curpm, which
		   will try to free it.  */
                av_push(PL_regex_padav, repointer);
                PL_reg_curpm->op_pmoffset = av_len(PL_regex_padav);
                PL_regex_pad = AvARRAY(PL_regex_padav);
            }
#endif      
        }
	SET_reg_curpm(rx);
	PL_reg_oldcurpm = PL_curpm;
	PL_curpm = PL_reg_curpm;
	if (RXp_MATCH_COPIED(prog)) {
	    /*  Here is a serious problem: we cannot rewrite subbeg,
		since it may be needed if this match fails.  Thus
		$` inside (?{}) could fail... */
	    PL_reg_oldsaved = prog->subbeg;
	    PL_reg_oldsavedlen = prog->sublen;
	    PL_reg_oldsavedoffset = prog->suboffset;
	    PL_reg_oldsavedcoffset = prog->suboffset;
#ifdef PERL_ANY_COW
	    PL_nrs = prog->saved_copy;
#endif
	    RXp_MATCH_COPIED_off(prog);
	}
	else
	    PL_reg_oldsaved = NULL;
	prog->subbeg = PL_bostr;
	prog->suboffset = 0;
	prog->subcoffset = 0;
	prog->sublen = PL_regeol - PL_bostr; /* strend may have been modified */
    }
#ifdef DEBUGGING
    PL_reg_starttry = *startposp;
#endif
    prog->offs[0].start = *startposp - PL_bostr;
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
       PL_reg_poscache[ST.cache_offset] |= ST.cache_mask; \
    sayNO

/* this is used to determine how far from the left messages like
   'failed...' are printed. It should be set such that messages 
   are inline with the regop output that created them.
*/
#define REPORT_CODE_OFF 32


#define CHRTEST_UNINIT -1001 /* c1/c2 haven't been calculated yet */
#define CHRTEST_VOID   -1000 /* the c1/c2 "next char" test should be skipped */
#define CHRTEST_NOT_A_CP_1 -999
#define CHRTEST_NOT_A_CP_2 -998

#define SLAB_FIRST(s) (&(s)->states[0])
#define SLAB_LAST(s)  (&(s)->states[PERL_REGMATCH_SLAB_SLOTS-1])

/* grab a new slab and return the first slot in it */

STATIC regmatch_state *
S_push_slab(pTHX)
{
#if PERL_VERSION < 9 && !defined(PERL_CORE)
    dMY_CXT;
#endif
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
 

#define DEBUG_STATE_pp(pp)				    \
    DEBUG_STATE_r({					    \
	DUMP_EXEC_POS(locinput, scan, utf8_target);		    \
	PerlIO_printf(Perl_debug_log,			    \
	    "    %*s"pp" %s%s%s%s%s\n",			    \
	    depth*2, "",				    \
	    PL_reg_name[st->resume_state],                     \
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
            RX_PRECOMP_const(prog), RX_PRELEN(prog), 60);   
        
        RE_PV_QUOTED_DECL(s1, utf8_target, PERL_DEBUG_PAD_ZERO(1),
            start, end - start, 60); 
        
        PerlIO_printf(Perl_debug_log, 
            "%s%s REx%s %s against %s\n", 
		       PL_colors[4], blurb, PL_colors[5], s0, s1); 
        
        if (utf8_target||utf8_pat)
            PerlIO_printf(Perl_debug_log, "UTF-8 %s%s%s...\n",
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
                      const bool utf8_target)
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
	const int is_uni = (utf8_target && OP(scan) != CANY) ? 1 : 0;

	RE_PV_COLOR_DECL(s0,len0,is_uni,PERL_DEBUG_PAD(0),
	    (locinput - pref_len),pref0_len, 60, 4, 5);
	
	RE_PV_COLOR_DECL(s1,len1,is_uni,PERL_DEBUG_PAD(1),
		    (locinput - pref_len + pref0_len),
		    pref_len - pref0_len, 60, 2, 3);
	
	RE_PV_COLOR_DECL(s2,len2,is_uni,PERL_DEBUG_PAD(2),
		    locinput, loc_regeol - locinput, 10, 0, 1);

	const STRLEN tlen=len0+len1+len2;
	PerlIO_printf(Perl_debug_log,
		    "%4"IVdf" <%.*s%.*s%s%.*s>%*s|",
		    (IV)(locinput - loc_bostr),
		    len0, s0,
		    len1, s1,
		    (docolor ? "" : "> <"),
		    len2, s2,
		    (int)(tlen > 19 ? 0 :  19 - tlen),
		    "");
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
S_reg_check_named_buff_matched(pTHX_ const regexp *rex, const regnode *scan)
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


/* free all slabs above current one  - called during LEAVE_SCOPE */

STATIC void
S_clear_backtrack_stack(pTHX_ void *p)
{
    regmatch_slab *s = PL_regmatch_slab->next;
    PERL_UNUSED_ARG(p);

    if (!s)
	return;
    PL_regmatch_slab->next = NULL;
    while (s) {
	regmatch_slab * const osl = s;
	s = s->next;
	Safefree(osl);
    }
}
static bool
S_setup_EXACTISH_ST_c1_c2(pTHX_ const regnode * const text_node, int *c1p,
        U8* c1_utf8, int *c2p, U8* c2_utf8, bool is_utf8_pat)
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

    const bool utf8_target = PL_reg_match_utf8;

    UV c1 = CHRTEST_NOT_A_CP_1;
    UV c2 = CHRTEST_NOT_A_CP_2;
    bool use_chrtest_void = FALSE;

    /* Used when we have both utf8 input and utf8 output, to avoid converting
     * to/from code points */
    bool utf8_has_been_setup = FALSE;

    dVAR;

    U8 *pat = (U8*)STRING(text_node);

    if (OP(text_node) == EXACT) {

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
    else /* an EXACTFish node */
         if ((is_utf8_pat
                    && is_MULTI_CHAR_FOLD_utf8_safe(pat,
                                                    pat + STR_LEN(text_node)))
             || (!is_utf8_pat
                    && is_MULTI_CHAR_FOLD_latin1_safe(pat,
                                                    pat + STR_LEN(text_node))))
    {
        /* Multi-character folds require more context to sort out.  Also
         * PL_utf8_foldclosures used below doesn't handle them, so have to be
         * handled outside this routine */
        use_chrtest_void = TRUE;
    }
    else { /* an EXACTFish node which doesn't begin with a multi-char fold */
        c1 = is_utf8_pat ? valid_utf8_to_uvchr(pat, NULL) : *pat;
        if (c1 > 256) {
            /* Load the folds hash, if not already done */
            SV** listp;
            if (! PL_utf8_foldclosures) {
                if (! PL_utf8_tofold) {
                    U8 dummy[UTF8_MAXBYTES+1];

                    /* Force loading this by folding an above-Latin1 char */
                    to_utf8_fold((U8*) HYPHEN_UTF8, dummy, NULL);
                    assert(PL_utf8_tofold); /* Verify that worked */
                }
                PL_utf8_foldclosures = _swash_inversion_hash(PL_utf8_tofold);
            }

            /* The fold closures data structure is a hash with the keys being
             * the UTF-8 of every character that is folded to, like 'k', and
             * the values each an array of all code points that fold to its
             * key.  e.g. [ 'k', 'K', KELVIN_SIGN ].  Multi-character folds are
             * not included */
            if ((! (listp = hv_fetch(PL_utf8_foldclosures,
                                     (char *) pat,
                                     UTF8SKIP(pat),
                                     FALSE))))
            {
                /* Not found in the hash, therefore there are no folds
                 * containing it, so there is only a single character that
                 * could match */
                c2 = c1;
            }
            else {  /* Does participate in folds */
                AV* list = (AV*) *listp;
                if (av_len(list) != 1) {

                    /* If there aren't exactly two folds to this, it is outside
                     * the scope of this function */
                    use_chrtest_void = TRUE;
                }
                else {  /* There are two.  Get them */
                    SV** c_p = av_fetch(list, 0, FALSE);
                    if (c_p == NULL) {
                        Perl_croak(aTHX_ "panic: invalid PL_utf8_foldclosures structure");
                    }
                    c1 = SvUV(*c_p);

                    c_p = av_fetch(list, 1, FALSE);
                    if (c_p == NULL) {
                        Perl_croak(aTHX_ "panic: invalid PL_utf8_foldclosures structure");
                    }
                    c2 = SvUV(*c_p);

                    /* Folds that cross the 255/256 boundary are forbidden if
                     * EXACTFL, or EXACTFA and one is ASCIII.  Since the
                     * pattern character is above 256, and its only other match
                     * is below 256, the only legal match will be to itself.
                     * We have thrown away the original, so have to compute
                     * which is the one above 255 */
                    if ((c1 < 256) != (c2 < 256)) {
                        if (OP(text_node) == EXACTFL
                            || (OP(text_node) == EXACTFA
                                && (isASCII(c1) || isASCII(c2))))
                        {
                            if (c1 < 256) {
                                c1 = c2;
                            }
                            else {
                                c2 = c1;
                            }
                        }
                    }
                }
            }
        }
        else /* Here, c1 is < 255 */
             if (utf8_target
                 && HAS_NONLATIN1_FOLD_CLOSURE(c1)
                 && OP(text_node) != EXACTFL
                 && (OP(text_node) != EXACTFA || ! isASCII(c1)))
        {
            /* Here, there could be something above Latin1 in the target which
             * folds to this character in the pattern.  All such cases except
             * LATIN SMALL LETTER Y WITH DIAERESIS have more than two characters
             * involved in their folds, so are outside the scope of this
             * function */
            if (UNLIKELY(c1 == LATIN_SMALL_LETTER_Y_WITH_DIAERESIS)) {
                c2 = LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS;
            }
            else {
                use_chrtest_void = TRUE;
            }
        }
        else { /* Here nothing above Latin1 can fold to the pattern character */
            switch (OP(text_node)) {

                case EXACTFL:   /* /l rules */
                    c2 = PL_fold_locale[c1];
                    break;

                case EXACTF:
                    if (! utf8_target) {    /* /d rules */
                        c2 = PL_fold[c1];
                        break;
                    }
                    /* FALLTHROUGH */
                    /* /u rules for all these.  This happens to work for
                     * EXACTFA as nothing in Latin1 folds to ASCII */
                case EXACTFA:
                case EXACTFU_TRICKYFOLD:
                case EXACTFU_SS:
                case EXACTFU:
                    c2 = PL_fold_latin1[c1];
                    break;

		default:
                    Perl_croak(aTHX_ "panic: Unexpected op %u", OP(text_node));
                    assert(0); /* NOTREACHED */
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

/* returns -1 on failure, $+[0] on success */
STATIC I32
S_regmatch(pTHX_ regmatch_info *reginfo, char *startpos, regnode *prog)
{
#if PERL_VERSION < 9 && !defined(PERL_CORE)
    dMY_CXT;
#endif
    dVAR;
    const bool utf8_target = PL_reg_match_utf8;
    const U32 uniflags = UTF8_ALLOW_DEFAULT;
    REGEXP *rex_sv = reginfo->prog;
    regexp *rex = ReANY(rex_sv);
    RXi_GET_DECL(rex,rexi);
    I32	oldsave;
    /* the current state. This is a cached copy of PL_regmatch_state */
    regmatch_state *st;
    /* cache heavy used fields of st in registers */
    regnode *scan;
    regnode *next;
    U32 n = 0;	/* general value; init to avoid compiler warning */
    I32 ln = 0; /* len or last;  init to avoid compiler warning */
    char *locinput = startpos;
    char *pushinput; /* where to continue after a PUSH */
    I32 nextchr;   /* is always set to UCHARAT(locinput) */

    bool result = 0;	    /* return value of S_regmatch */
    int depth = 0;	    /* depth of backtrack stack */
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
    bool has_cutgroup = RX_HAS_CUTGROUP(rex) ? 1 : 0;   
    SV* const oreplsv = GvSV(PL_replgv);
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
    I32 gimme = G_SCALAR;
    CV *caller_cv = NULL;	/* who called us */
    CV *last_pushed_cv = NULL;	/* most recently called (?{}) CV */
    CHECKPOINT runops_cp;	/* savestack position before executing EVAL */
    U32 maxopenparen = 0;       /* max '(' index seen so far */
    int to_complement;  /* Invert the result? */
    _char_class_number classnum;
    bool is_utf8_pat = reginfo->is_utf8_pat;

#ifdef DEBUGGING
    GET_RE_DEBUG_FLAGS_DECL;
#endif

    /* shut up 'may be used uninitialized' compiler warnings for dMULTICALL */
    multicall_oldcatch = 0;
    multicall_cv = NULL;
    cx = NULL;
    PERL_UNUSED_VAR(multicall_cop);
    PERL_UNUSED_VAR(newsp);


    PERL_ARGS_ASSERT_REGMATCH;

    DEBUG_OPTIMISE_r( DEBUG_EXECUTE_r({
	    PerlIO_printf(Perl_debug_log,"regmatch start\n");
    }));
    /* on first ever call to regmatch, allocate first slab */
    if (!PL_regmatch_slab) {
	Newx(PL_regmatch_slab, 1, regmatch_slab);
	PL_regmatch_slab->prev = NULL;
	PL_regmatch_slab->next = NULL;
	PL_regmatch_state = SLAB_FIRST(PL_regmatch_slab);
    }

    oldsave = PL_savestack_ix;
    SAVEDESTRUCTOR_X(S_clear_backtrack_stack, NULL);
    SAVEVPTR(PL_regmatch_slab);
    SAVEVPTR(PL_regmatch_state);

    /* grab next free state slot */
    st = ++PL_regmatch_state;
    if (st >  SLAB_LAST(PL_regmatch_slab))
	st = PL_regmatch_state = S_push_slab(aTHX);

    /* Note that nextchr is a byte even in UTF */
    SET_nextchr;
    scan = prog;
    while (scan != NULL) {

        DEBUG_EXECUTE_r( {
	    SV * const prop = sv_newmortal();
	    regnode *rnext=regnext(scan);
	    DUMP_EXEC_POS( locinput, scan, utf8_target );
	    regprop(rex, prop, scan);
            
	    PerlIO_printf(Perl_debug_log,
		    "%3"IVdf":%*s%s(%"IVdf")\n",
		    (IV)(scan - rexi->program), depth*2, "",
		    SvPVX_const(prop),
		    (PL_regkind[OP(scan)] == END || !rnext) ? 
		        0 : (IV)(rnext - rexi->program));
	});

	next = scan + NEXT_OFF(scan);
	if (next == scan)
	    next = NULL;
	state_num = OP(scan);

      reenter_switch:
        to_complement = 0;

        SET_nextchr;
        assert(nextchr < 256 && (nextchr >= 0 || nextchr == NEXTCHR_EOS));

	switch (state_num) {
	case BOL: /*  /^../  */
	    if (locinput == PL_bostr)
	    {
		/* reginfo->till = reginfo->bol; */
		break;
	    }
	    sayNO;

	case MBOL: /*  /^../m  */
	    if (locinput == PL_bostr ||
		(!NEXTCHR_IS_EOS && locinput[-1] == '\n'))
	    {
		break;
	    }
	    sayNO;

	case SBOL: /*  /^../s  */
	    if (locinput == PL_bostr)
		break;
	    sayNO;

	case GPOS: /*  \G  */
	    if (locinput == reginfo->ganch)
		break;
	    sayNO;

	case KEEPS: /*   \K  */
	    /* update the startpoint */
	    st->u.keeper.val = rex->offs[0].start;
	    rex->offs[0].start = locinput - PL_bostr;
	    PUSH_STATE_GOTO(KEEPS_next, next, locinput);
	    assert(0); /*NOTREACHED*/
	case KEEPS_next_fail:
	    /* rollback the start point change */
	    rex->offs[0].start = st->u.keeper.val;
	    sayNO_SILENT;
	    assert(0); /*NOTREACHED*/

	case EOL: /* /..$/  */
		goto seol;

	case MEOL: /* /..$/m  */
	    if (!NEXTCHR_IS_EOS && nextchr != '\n')
		sayNO;
	    break;

	case SEOL: /* /..$/s  */
	  seol:
	    if (!NEXTCHR_IS_EOS && nextchr != '\n')
		sayNO;
	    if (PL_regeol - locinput > 1)
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

	case CANY: /*  \C  */
	    if (NEXTCHR_IS_EOS)
		sayNO;
	    locinput++;
	    break;

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
                    PerlIO_printf(Perl_debug_log,
                              "%*s  %sfailed to match trie start class...%s\n",
                              REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5])
                );
                sayNO_SILENT;
                assert(0); /* NOTREACHED */
            }
            /* FALL THROUGH */
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

                if (   trie->bitmap
                    && (NEXTCHR_IS_EOS || !TRIE_BITMAP_TEST(trie, nextchr)))
                {
        	    if (trie->states[ state ].wordnum) {
        	         DEBUG_EXECUTE_r(
                            PerlIO_printf(Perl_debug_log,
                        	          "%*s  %smatched empty string...%s\n",
                        	          REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5])
                        );
			if (!trie->jump)
			    break;
        	    } else {
        	        DEBUG_EXECUTE_r(
                            PerlIO_printf(Perl_debug_log,
                        	          "%*s  %sfailed to match trie start class...%s\n",
                        	          REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5])
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

		while ( state && uc <= (U8*)PL_regeol ) {
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
		                DUMP_EXEC_POS( (char *)uc, scan, utf8_target );
			        PerlIO_printf( Perl_debug_log,
			            "%*s  %sState: %4"UVxf" Accepted: %c ",
			            2+depth * 2, "", PL_colors[4],
			            (UV)state, (accepted ? 'Y' : 'N'));
		    });

		    /* read a char and goto next state */
		    if ( base && (foldlen || uc < (U8*)PL_regeol)) {
			I32 offset;
			REXEC_TRIE_READ_CHAR(trie_type, trie, widecharmap, uc,
					     uscan, len, uvc, charid, foldlen,
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
		        PerlIO_printf( Perl_debug_log,
		            "Charid:%3x CP:%4"UVxf" After State: %4"UVxf"%s\n",
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
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sgot %"IVdf" possible matches%s\n",
			REPORT_CODE_OFF + depth * 2, "",
			PL_colors[4], (IV)ST.accepted, PL_colors[5] );
		);
		goto trie_first_try; /* jump into the fail handler */
	    }}
	    assert(0); /* NOTREACHED */

	case TRIE_next_fail: /* we failed - try next alternative */
        {
            U8 *uc;
            if ( ST.jump) {
                REGCP_UNWIND(ST.cp);
                UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
	    }
	    if (!--ST.accepted) {
	        DEBUG_EXECUTE_r({
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sTRIE failed...%s\n",
			REPORT_CODE_OFF+depth*2, "", 
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

            if ( ST.jump) {
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
			    uvc = utf8n_to_uvuni((U8*)uc, UTF8_MAXLEN, &len,
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
			    uvc = utf8n_to_uvuni(uscan, UTF8_MAXLEN, &len,
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
		PerlIO_printf( Perl_debug_log,
		    "%*s  %sTRIE matched word #%d, continuing%s\n",
		    REPORT_CODE_OFF+depth*2, "", 
		    PL_colors[4],
		    ST.nextword,
		    PL_colors[5]
		    );
	    });

	    if (ST.accepted > 1 || has_cutgroup) {
		PUSH_STATE_GOTO(TRIE_next, scan, (char*)uc);
		assert(0); /* NOTREACHED */
	    }
	    /* only one choice left - just continue */
	    DEBUG_EXECUTE_r({
		AV *const trie_words
		    = MUTABLE_AV(rexi->data->data[ARG(ST.me)+TRIE_WORDS_OFFSET]);
		SV ** const tmp = av_fetch( trie_words,
		    ST.nextword-1, 0 );
		SV *sv= tmp ? sv_newmortal() : NULL;

		PerlIO_printf( Perl_debug_log,
		    "%*s  %sonly one match left, short-circuiting: #%d <%s>%s\n",
		    REPORT_CODE_OFF+depth*2, "", PL_colors[4],
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
	    assert(0); /* NOTREACHED */
        }
#undef  ST

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
			if (l >= PL_regeol || UTF8_IS_ABOVE_LATIN1(* (U8*) l)) {
                            sayNO;
                        }
                        if (UTF8_IS_INVARIANT(*(U8*)l)) {
			    if (*l != *s) {
                                sayNO;
                            }
                            l++;
                        }
                        else {
                            if (TWO_BYTE_UTF8_TO_UNI(*l, *(l+1)) != * (U8*) s) {
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
                        if (l >= PL_regeol || UTF8_IS_ABOVE_LATIN1(* (U8*) s))
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
                            if (TWO_BYTE_UTF8_TO_UNI(*s, *(s+1)) != * (U8*) l) {
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
                if (PL_regeol - locinput < ln
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

            RX_MATCH_TAINTED_on(reginfo->prog);
            folder = foldEQ_locale;
            fold_array = PL_fold_locale;
	    fold_utf8_flags = FOLDEQ_UTF8_LOCALE;
	    goto do_exactf;

	case EXACTFU_SS:         /*  /\x{df}/iu   */
	case EXACTFU_TRICKYFOLD: /*  /\x{390}/iu  */
	case EXACTFU:            /*  /abc/iu      */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    fold_utf8_flags = is_utf8_pat ? FOLDEQ_S1_ALREADY_FOLDED : 0;
	    goto do_exactf;

	case EXACTFA:            /*  /abc/iaa     */
	    folder = foldEQ_latin1;
	    fold_array = PL_fold_latin1;
	    fold_utf8_flags = FOLDEQ_UTF8_NOMIX_ASCII;
	    goto do_exactf;

	case EXACTF:             /*  /abc/i       */
	    folder = foldEQ;
	    fold_array = PL_fold;
	    fold_utf8_flags = 0;

	  do_exactf:
	    s = STRING(scan);
	    ln = STR_LEN(scan);

	    if (utf8_target || is_utf8_pat || state_num == EXACTFU_SS) {
	      /* Either target or the pattern are utf8, or has the issue where
	       * the fold lengths may differ. */
		const char * const l = locinput;
		char *e = PL_regeol;

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
	    if (PL_regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && ! folder(s, locinput, ln))
		sayNO;
	    locinput += ln;
	    break;
	}

	/* XXX Could improve efficiency by separating these all out using a
	 * macro or in-line function.  At that point regcomp.c would no longer
	 * have to set the FLAGS fields of these */
	case BOUNDL:  /*  /\b/l  */
	case NBOUNDL: /*  /\B/l  */
            RX_MATCH_TAINTED_on(reginfo->prog);
	    /* FALL THROUGH */
	case BOUND:   /*  /\b/   */
	case BOUNDU:  /*  /\b/u  */
	case BOUNDA:  /*  /\b/a  */
	case NBOUND:  /*  /\B/   */
	case NBOUNDU: /*  /\B/u  */
	case NBOUNDA: /*  /\B/a  */
	    /* was last char in word? */
	    if (utf8_target
		&& FLAGS(scan) != REGEX_ASCII_RESTRICTED_CHARSET
		&& FLAGS(scan) != REGEX_ASCII_MORE_RESTRICTED_CHARSET)
	    {
		if (locinput == PL_bostr)
		    ln = '\n';
		else {
		    const U8 * const r = reghop3((U8*)locinput, -1, (U8*)PL_bostr);

		    ln = utf8n_to_uvchr(r, UTF8SKIP(r), 0, uniflags);
		}
		if (FLAGS(scan) != REGEX_LOCALE_CHARSET) {
		    ln = isWORDCHAR_uni(ln);
                    if (NEXTCHR_IS_EOS)
                        n = 0;
                    else {
                        LOAD_UTF8_CHARCLASS_ALNUM();
                        n = swash_fetch(PL_utf8_swash_ptrs[_CC_WORDCHAR], (U8*)locinput,
                                                                utf8_target);
                    }
		}
		else {
		    ln = isWORDCHAR_LC_uvchr(UNI_TO_NATIVE(ln));
		    n = NEXTCHR_IS_EOS ? 0 : isWORDCHAR_LC_utf8((U8*)locinput);
		}
	    }
	    else {

		/* Here the string isn't utf8, or is utf8 and only ascii
		 * characters are to match \w.  In the latter case looking at
		 * the byte just prior to the current one may be just the final
		 * byte of a multi-byte character.  This is ok.  There are two
		 * cases:
		 * 1) it is a single byte character, and then the test is doing
		 *	just what it's supposed to.
		 * 2) it is a multi-byte character, in which case the final
		 *	byte is never mistakable for ASCII, and so the test
		 *	will say it is not a word character, which is the
		 *	correct answer. */
		ln = (locinput != PL_bostr) ?
		    UCHARAT(locinput - 1) : '\n';
		switch (FLAGS(scan)) {
		    case REGEX_UNICODE_CHARSET:
			ln = isWORDCHAR_L1(ln);
			n = NEXTCHR_IS_EOS ? 0 : isWORDCHAR_L1(nextchr);
			break;
		    case REGEX_LOCALE_CHARSET:
			ln = isWORDCHAR_LC(ln);
			n = NEXTCHR_IS_EOS ? 0 : isWORDCHAR_LC(nextchr);
			break;
		    case REGEX_DEPENDS_CHARSET:
			ln = isWORDCHAR(ln);
			n = NEXTCHR_IS_EOS ? 0 : isWORDCHAR(nextchr);
			break;
		    case REGEX_ASCII_RESTRICTED_CHARSET:
		    case REGEX_ASCII_MORE_RESTRICTED_CHARSET:
			ln = isWORDCHAR_A(ln);
			n = NEXTCHR_IS_EOS ? 0 : isWORDCHAR_A(nextchr);
			break;
		    default:
			Perl_croak(aTHX_ "panic: Unexpected FLAGS %u in op %u", FLAGS(scan), OP(scan));
			break;
		}
	    }
	    /* Note requires that all BOUNDs be lower than all NBOUNDs in
	     * regcomp.sym */
	    if (((!ln) == (!n)) == (OP(scan) < NBOUND))
		    sayNO;
	    break;

	case ANYOF:  /*  /[abc]/       */
	case ANYOF_WARN_SUPER:
            if (NEXTCHR_IS_EOS)
                sayNO;
	    if (utf8_target) {
	        if (!reginclass(rex, scan, (U8*)locinput, utf8_target))
		    sayNO;
		locinput += UTF8SKIP(locinput);
	    }
	    else {
		if (!REGINCLASS(rex, scan, (U8*)locinput))
		    sayNO;
		locinput++;
	    }
	    break;

        /* The argument (FLAGS) to all the POSIX node types is the class number
         * */

        case NPOSIXL:   /* \W or [:^punct:] etc. under /l */
            to_complement = 1;
            /* FALLTHROUGH */

        case POSIXL:    /* \w or [:punct:] etc. under /l */
            if (NEXTCHR_IS_EOS)
                sayNO;

            /* The locale hasn't influenced the outcome before this, so defer
             * tainting until now */
            RX_MATCH_TAINTED_on(reginfo->prog);

            /* Use isFOO_lc() for characters within Latin1.  (Note that
             * UTF8_IS_INVARIANT works even on non-UTF-8 strings, or else
             * wouldn't be invariant) */
            if (UTF8_IS_INVARIANT(nextchr) || ! utf8_target) {
                if (! (to_complement ^ cBOOL(isFOO_lc(FLAGS(scan), (U8) nextchr)))) {
                    sayNO;
                }
            }
            else if (UTF8_IS_DOWNGRADEABLE_START(nextchr)) {
                if (! (to_complement ^ cBOOL(isFOO_lc(FLAGS(scan),
                                        (U8) TWO_BYTE_UTF8_TO_UNI(nextchr,
                                                            *(locinput + 1))))))
                {
                    sayNO;
                }
            }
            else { /* Here, must be an above Latin-1 code point */
                goto utf8_posix_not_eos;
            }

            /* Here, must be utf8 */
            locinput += UTF8SKIP(locinput);
            break;

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
            /* FALLTHROUGH */

        case POSIXA:    /* \w or [:punct:] etc. under /a */

          posixa:
            /* We get here through POSIXD, NPOSIXD, and NPOSIXA when not in
             * UTF-8, and also from NPOSIXA even in UTF-8 when the current
             * character is a single byte */

            if (NEXTCHR_IS_EOS
                || ! (to_complement ^ cBOOL(_generic_isCC_A(nextchr,
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
          utf8_posix_not_eos:

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
            else if (UTF8_IS_DOWNGRADEABLE_START(nextchr)) {
                if (! (to_complement
                       ^ cBOOL(_generic_isCC(TWO_BYTE_UTF8_TO_UNI(nextchr,
                                                               *(locinput + 1)),
                                              FLAGS(scan)))))
                {
                    sayNO;
                }
                locinput += 2;
            }
            else {  /* Handle above Latin-1 code points */
                classnum = (_char_class_number) FLAGS(scan);
                if (classnum < _FIRST_NON_SWASH_CC) {

                    /* Here, uses a swash to find such code points.  Load if if
                     * not done already */
                    if (! PL_utf8_swash_ptrs[classnum]) {
                        U8 flags = _CORE_SWASH_INIT_ACCEPT_INVLIST;
                        PL_utf8_swash_ptrs[classnum]
                                = _core_swash_init("utf8",
                                        swash_property_names[classnum],
                                        &PL_sv_undef, 1, 0, NULL, &flags);
                    }
                    if (! (to_complement
                           ^ cBOOL(swash_fetch(PL_utf8_swash_ptrs[classnum],
                                               (U8 *) locinput, TRUE))))
                    {
                        sayNO;
                    }
                }
                else {  /* Here, uses macros to find above Latin-1 code points */
                    switch (classnum) {
                        case _CC_ENUM_SPACE:    /* XXX would require separate
                                                   code if we revert the change
                                                   of \v matching this */
                        case _CC_ENUM_PSXSPC:
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
                        default:    /* The rest, e.g. [:cntrl:], can't match
                                       above Latin1 */
                            if (! to_complement) {
                                sayNO;
                            }
                            break;
                    }
                }
                locinput += UTF8SKIP(locinput);
            }
            break;

	case CLUMP: /* Match \X: logical Unicode character.  This is defined as
		       a Unicode extended Grapheme Cluster */
	    /* From http://www.unicode.org/reports/tr29 (5.2 version).  An
	      extended Grapheme Cluster is:

            CR LF
            | Prepend* Begin Extend*
            | .

            Begin is:           ( Special_Begin | ! Control )
            Special_Begin is:   ( Regional-Indicator+ | Hangul-syllable )
            Extend is:          ( Grapheme_Extend | Spacing_Mark )
            Control is:         [ GCB_Control | CR | LF ]
            Hangul-syllable is: ( T+ | ( L* ( L | ( LVT | ( V | LV ) V* ) T* ) ))

               If we create a 'Regular_Begin' = Begin - Special_Begin, then
               we can rewrite

                   Begin is ( Regular_Begin + Special Begin )

               It turns out that 98.4% of all Unicode code points match
               Regular_Begin.  Doing it this way eliminates a table match in
               the previous implementation for almost all Unicode code points.

	       There is a subtlety with Prepend* which showed up in testing.
	       Note that the Begin, and only the Begin is required in:
	        | Prepend* Begin Extend*
	       Also, Begin contains '! Control'.  A Prepend must be a
	       '!  Control', which means it must also be a Begin.  What it
	       comes down to is that if we match Prepend* and then find no
	       suitable Begin afterwards, that if we backtrack the last
	       Prepend, that one will be a suitable Begin.
	    */

	    if (NEXTCHR_IS_EOS)
		sayNO;
	    if  (! utf8_target) {

		/* Match either CR LF  or '.', as all the other possibilities
		 * require utf8 */
		locinput++;	    /* Match the . or CR */
		if (nextchr == '\r' /* And if it was CR, and the next is LF,
				       match the LF */
		    && locinput < PL_regeol
		    && UCHARAT(locinput) == '\n')
                {
                    locinput++;
                }
	    }
	    else {

		/* Utf8: See if is ( CR LF ); already know that locinput <
		 * PL_regeol, so locinput+1 is in bounds */
		if ( nextchr == '\r' && locinput+1 < PL_regeol
                     && UCHARAT(locinput + 1) == '\n')
                {
		    locinput += 2;
		}
		else {
                    STRLEN len;

		    /* In case have to backtrack to beginning, then match '.' */
		    char *starting = locinput;

		    /* In case have to backtrack the last prepend */
		    char *previous_prepend = NULL;

		    LOAD_UTF8_CHARCLASS_GCB();

                    /* Match (prepend)*   */
                    while (locinput < PL_regeol
                           && (len = is_GCB_Prepend_utf8(locinput)))
                    {
                        previous_prepend = locinput;
                        locinput += len;
                    }

		    /* As noted above, if we matched a prepend character, but
		     * the next thing won't match, back off the last prepend we
		     * matched, as it is guaranteed to match the begin */
		    if (previous_prepend
			&& (locinput >=  PL_regeol
			    || (! swash_fetch(PL_utf8_X_regular_begin,
					     (U8*)locinput, utf8_target)
			         && ! is_GCB_SPECIAL_BEGIN_START_utf8(locinput)))
                        )
		    {
			locinput = previous_prepend;
		    }

		    /* Note that here we know PL_regeol > locinput, as we
		     * tested that upon input to this switch case, and if we
		     * moved locinput forward, we tested the result just above
		     * and it either passed, or we backed off so that it will
		     * now pass */
		    if (swash_fetch(PL_utf8_X_regular_begin,
                                    (U8*)locinput, utf8_target)) {
                        locinput += UTF8SKIP(locinput);
                    }
                    else if (! is_GCB_SPECIAL_BEGIN_START_utf8(locinput)) {

			/* Here did not match the required 'Begin' in the
			 * second term.  So just match the very first
			 * character, the '.' of the final term of the regex */
			locinput = starting + UTF8SKIP(starting);
                        goto exit_utf8;
		    } else {

                        /* Here is a special begin.  It can be composed of
                         * several individual characters.  One possibility is
                         * RI+ */
                        if ((len = is_GCB_RI_utf8(locinput))) {
                            locinput += len;
                            while (locinput < PL_regeol
                                   && (len = is_GCB_RI_utf8(locinput)))
                            {
                                locinput += len;
                            }
                        } else if ((len = is_GCB_T_utf8(locinput))) {
                            /* Another possibility is T+ */
                            locinput += len;
                            while (locinput < PL_regeol
                                && (len = is_GCB_T_utf8(locinput)))
                            {
                                locinput += len;
                            }
                        } else {

                            /* Here, neither RI+ nor T+; must be some other
                             * Hangul.  That means it is one of the others: L,
                             * LV, LVT or V, and matches:
                             * L* (L | LVT T* | V * V* T* | LV  V* T*) */

                            /* Match L*           */
                            while (locinput < PL_regeol
                                   && (len = is_GCB_L_utf8(locinput)))
                            {
                                locinput += len;
                            }

                            /* Here, have exhausted L*.  If the next character
                             * is not an LV, LVT nor V, it means we had to have
                             * at least one L, so matches L+ in the original
                             * equation, we have a complete hangul syllable.
                             * Are done. */

                            if (locinput < PL_regeol
                                && is_GCB_LV_LVT_V_utf8(locinput))
                            {
                                /* Otherwise keep going.  Must be LV, LVT or V.
                                 * See if LVT, by first ruling out V, then LV */
                                if (! is_GCB_V_utf8(locinput)
                                        /* All but every TCount one is LV */
                                    && (valid_utf8_to_uvchr((U8 *) locinput,
                                                                         NULL)
                                                                        - SBASE)
                                        % TCount != 0)
                                {
                                    locinput += UTF8SKIP(locinput);
                                } else {

                                    /* Must be  V or LV.  Take it, then match
                                     * V*     */
                                    locinput += UTF8SKIP(locinput);
                                    while (locinput < PL_regeol
                                           && (len = is_GCB_V_utf8(locinput)))
                                    {
                                        locinput += len;
                                    }
                                }

                                /* And any of LV, LVT, or V can be followed
                                 * by T*            */
                                while (locinput < PL_regeol
                                       && (len = is_GCB_T_utf8(locinput)))
                                {
                                    locinput += len;
                                }
                            }
                        }
                    }

                    /* Match any extender */
                    while (locinput < PL_regeol
                            && swash_fetch(PL_utf8_X_extend,
                                            (U8*)locinput, utf8_target))
                    {
                        locinput += UTF8SKIP(locinput);
                    }
		}
            exit_utf8:
		if (locinput > PL_regeol) sayNO;
	    }
	    break;
            
	case NREFFL:  /*  /\g{name}/il  */
	{   /* The capture buffer cases.  The ones beginning with N for the
	       named buffers just convert to the equivalent numbered and
	       pretend they were called as the corresponding numbered buffer
	       op.  */
	    /* don't initialize these in the declaration, it makes C++
	       unhappy */
	    char *s;
	    char type;
	    re_fold_t folder;
	    const U8 *fold_array;
	    UV utf8_fold_flags;

            RX_MATCH_TAINTED_on(reginfo->prog);
	    folder = foldEQ_locale;
	    fold_array = PL_fold_locale;
	    type = REFFL;
	    utf8_fold_flags = FOLDEQ_UTF8_LOCALE;
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
            RX_MATCH_TAINTED_on(reginfo->prog);
	    folder = foldEQ_locale;
	    fold_array = PL_fold_locale;
	    utf8_fold_flags = FOLDEQ_UTF8_LOCALE;
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
	    PL_reg_leftiter = PL_reg_maxiter;		/* Void cache */
	    if (rex->lastparen < n || ln == -1)
		sayNO;			/* Do not match unless seen CLOSEn. */
	    if (ln == rex->offs[n].end)
		break;

	    s = PL_bostr + ln;
	    if (type != REF	/* REF can do byte comparison */
		&& (utf8_target || type == REFFU))
	    { /* XXX handle REFFL better */
		char * limit = PL_regeol;

		/* This call case insensitively compares the entire buffer
		    * at s, with the current input starting at locinput, but
		    * not going off the end given by PL_regeol, and returns in
		    * <limit> upon success, how much of the current input was
		    * matched */
		if (! foldEQ_utf8_flags(s, NULL, rex->offs[n].end - ln, utf8_target,
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
	    ln = rex->offs[n].end - ln;
	    if (locinput + ln > PL_regeol)
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

	case BACK: /* ??? doesn't appear to be used ??? */
	    break;

#undef  ST
#define ST st->u.eval
	{
	    SV *ret;
	    REGEXP *re_sv;
            regexp *re;
            regexp_internal *rei;
            regnode *startpoint;

	case GOSTART: /*  (?R)  */
	case GOSUB: /*    /(...(?1))/   /(...(?&foo))/   */
	    if (cur_eval && cur_eval->locinput==locinput) {
                if (cur_eval->u.eval.close_paren == (U32)ARG(scan)) 
                    Perl_croak(aTHX_ "Infinite recursion in regex");
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
            if (OP(scan)==GOSUB) {
                startpoint = scan + ARG2L(scan);
                ST.close_paren = ARG(scan);
            } else {
                startpoint = rei->program+1;
                ST.close_paren = 0;
            }
            goto eval_recurse_doit;
            assert(0); /* NOTREACHED */

        case EVAL:  /*   /(?{A})B/   /(??{A})B/  and /(?(?{A})X|Y)B/   */        
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
		char *saved_regeol = PL_regeol;
		struct re_save_state saved_state;
		CV *newcv;

		/* save *all* paren positions */
		regcppush(rex, 0, maxopenparen);
		REGCP_SET(runops_cp);

		/* To not corrupt the existing regex state while executing the
		 * eval we would normally put it on the save stack, like with
		 * save_re_context. However, re-evals have a weird scoping so we
		 * can't just add ENTER/LEAVE here. With that, things like
		 *
		 *    (?{$a=2})(a(?{local$a=$a+1}))*aak*c(?{$b=$a})
		 *
		 * would break, as they expect the localisation to be unwound
		 * only when the re-engine backtracks through the bit that
		 * localised it.
		 *
		 * What we do instead is just saving the state in a local c
		 * variable.
		 */
		Copy(&PL_reg_state, &saved_state, 1, struct re_save_state);

		if (!caller_cv)
		    caller_cv = find_runcv(NULL);

		n = ARG(scan);

		if (rexi->data->what[n] == 'r') { /* code from an external qr */
		    newcv = (ReANY(
						(REGEXP*)(rexi->data->data[n])
					    ))->qr_anoncv
					;
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

		/* normally if we're about to execute code from the same
		 * CV that we used previously, we just use the existing
		 * CX stack entry. However, its possible that in the
		 * meantime we may have backtracked, popped from the save
		 * stack, and undone the SAVECOMPPAD(s) associated with
		 * PUSH_MULTICALL; in which case PL_comppad no longer
		 * points to newcv's pad. */
		if (newcv != last_pushed_cv || PL_comppad != last_pad)
		{
                    U8 flags = (CXp_SUB_RE |
                                ((newcv == caller_cv) ? CXp_SUB_RE_FAKE : 0));
		    if (last_pushed_cv) {
			CHANGE_MULTICALL_FLAGS(newcv, flags);
		    }
		    else {
			PUSH_MULTICALL_FLAGS(newcv, flags);
		    }
		    last_pushed_cv = newcv;
		}
		else {
                    /* these assignments are just to silence compiler
                     * warnings */
		    multicall_cop = NULL;
		    newsp = NULL;
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
			o = o->op_sibling;
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

		DEBUG_STATE_r( PerlIO_printf(Perl_debug_log, 
		    "  re EVAL PL_op=0x%"UVxf"\n", PTR2UV(nop)) );

		rex->offs[0].end = PL_reg_magic->mg_len = locinput - PL_bostr;

                if (sv_yes_mark) {
                    SV *sv_mrk = get_sv("REGMARK", 1);
                    sv_setsv(sv_mrk, sv_yes_mark);
                }

		/* we don't use MULTICALL here as we want to call the
		 * first op of the block of interest, rather than the
		 * first op of the sub */
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
		    sw = cBOOL(SvTRUE(ret));
		    logical = 0;
		}
		else {                   /*  /(??{})  */
		    /*  if its overloaded, let the regex compiler handle
		     *  it; otherwise extract regex, or stringify  */
		    if (!SvAMAGIC(ret)) {
			SV *sv = ret;
			if (SvROK(sv))
			    sv = SvRV(sv);
			if (SvTYPE(sv) == SVt_REGEXP)
			    re_sv = (REGEXP*) sv;
			else if (SvSMAGICAL(sv)) {
			    MAGIC *mg = mg_find(sv, PERL_MAGIC_qr);
			    if (mg)
				re_sv = (REGEXP *) mg->mg_obj;
			}

			/* force any magic, undef warnings here */
			if (!re_sv) {
			    ret = sv_mortalcopy(ret);
			    (void) SvPV_force_nolen(ret);
			}
		    }

		}

		Copy(&saved_state, &PL_reg_state, 1, struct re_save_state);

		/* *** Note that at this point we don't restore
		 * PL_comppad, (or pop the CxSUB) on the assumption it may
		 * be used again soon. This is safe as long as nothing
		 * in the regexp code uses the pad ! */
		PL_op = oop;
		PL_curcop = ocurcop;
		PL_regeol = saved_regeol;
		S_regcp_restore(aTHX_ rex, runops_cp, &maxopenparen);

		if (logical != 2)
		    break;
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
                                    /* copy /msix etc to inner pattern */
                                    scan->flags,
                                    pm_flags);

			if (!(SvFLAGS(ret)
			      & (SVs_TEMP | SVs_PADTMP | SVf_READONLY
				 | SVs_GMG))) {
			    /* This isn't a first class regexp. Instead, it's
			       caching a regexp onto an existing, Perl visible
			       scalar.  */
			    sv_magic(ret, MUTABLE_SV(re_sv), PERL_MAGIC_qr, 0, 0);
			}
			/* safe to do now that any $1 etc has been
			 * interpolated into the new pattern string and
			 * compiled */
			S_regcp_restore(aTHX_ rex, runops_cp, &maxopenparen);
		    }
		    SAVEFREESV(re_sv);
		    re = ReANY(re_sv);
		}
                RXp_MATCH_COPIED_off(re);
                re->subbeg = rex->subbeg;
                re->sublen = rex->sublen;
                re->suboffset = rex->suboffset;
                re->subcoffset = rex->subcoffset;
		rei = RXi_GET(re);
                DEBUG_EXECUTE_r(
                    debug_start_match(re_sv, utf8_target, locinput, PL_regeol,
                        "Matching embedded");
		);		
		startpoint = rei->program + 1;
               	ST.close_paren = 0; /* only used for GOSUB */

        eval_recurse_doit: /* Share code with GOSUB below this line */                		
		/* run the pattern returned from (??{...}) */

                /* Save *all* the positions. */
		ST.cp = regcppush(rex, 0, maxopenparen);
		REGCP_SET(ST.lastcp);
		
		re->lastparen = 0;
		re->lastcloseparen = 0;

		maxopenparen = 0;

		/* XXXX This is too dramatic a measure... */
		PL_reg_maxiter = 0;

		ST.saved_utf8_pat = is_utf8_pat;
		is_utf8_pat = cBOOL(RX_UTF8(re_sv));

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
		PUSH_YES_STATE_GOTO(EVAL_AB, startpoint, locinput);
		assert(0); /* NOTREACHED */
	}

	case EVAL_AB: /* cleanup after a successful (??{A})B */
	    /* note: this is called twice; first after popping B, then A */
            is_utf8_pat = ST.saved_utf8_pat;
	    rex_sv = ST.prev_rex;
	    SET_reg_curpm(rex_sv);
	    rex = ReANY(rex_sv);
	    rexi = RXi_GET(rex);
	    regcpblow(ST.cp);
	    cur_eval = ST.prev_eval;
	    cur_curlyx = ST.prev_curlyx;

	    /* XXXX This is too dramatic a measure... */
	    PL_reg_maxiter = 0;
            if ( nochange_depth )
	        nochange_depth--;
	    sayYES;


	case EVAL_AB_fail: /* unsuccessfully ran A or B in (??{A})B */
	    /* note: this is called twice; first after popping B, then A */
            is_utf8_pat = ST.saved_utf8_pat;
	    rex_sv = ST.prev_rex;
	    SET_reg_curpm(rex_sv);
	    rex = ReANY(rex_sv);
	    rexi = RXi_GET(rex); 

	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex, &maxopenparen);
	    cur_eval = ST.prev_eval;
	    cur_curlyx = ST.prev_curlyx;
	    /* XXXX This is too dramatic a measure... */
	    PL_reg_maxiter = 0;
	    if ( nochange_depth )
	        nochange_depth--;
	    sayNO_SILENT;
#undef ST

	case OPEN: /*  (  */
	    n = ARG(scan);  /* which paren pair */
	    rex->offs[n].start_tmp = locinput - PL_bostr;
	    if (n > maxopenparen)
		maxopenparen = n;
	    DEBUG_BUFFERS_r(PerlIO_printf(Perl_debug_log,
		"rex=0x%"UVxf" offs=0x%"UVxf": \\%"UVuf": set %"IVdf" tmp; maxopenparen=%"UVuf"\n",
		PTR2UV(rex),
		PTR2UV(rex->offs),
		(UV)n,
		(IV)rex->offs[n].start_tmp,
		(UV)maxopenparen
	    ));
            lastopen = n;
	    break;

/* XXX really need to log other places start/end are set too */
#define CLOSE_CAPTURE \
    rex->offs[n].start = rex->offs[n].start_tmp; \
    rex->offs[n].end = locinput - PL_bostr; \
    DEBUG_BUFFERS_r(PerlIO_printf(Perl_debug_log, \
	"rex=0x%"UVxf" offs=0x%"UVxf": \\%"UVuf": set %"IVdf"..%"IVdf"\n", \
	PTR2UV(rex), \
	PTR2UV(rex->offs), \
	(UV)n, \
	(IV)rex->offs[n].start, \
	(IV)rex->offs[n].end \
    ))

	case CLOSE:  /*  )  */
	    n = ARG(scan);  /* which paren pair */
	    CLOSE_CAPTURE;
	    if (n > rex->lastparen)
		rex->lastparen = n;
	    rex->lastcloseparen = n;
            if (cur_eval && cur_eval->u.eval.close_paren == n) {
	        goto fake_end;
	    }    
	    break;

        case ACCEPT:  /*  (*ACCEPT)  */
            if (ARG(scan)){
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
                            if ( n == ARG(scan) || (cur_eval &&
                                cur_eval->u.eval.close_paren == n))
                                break;
                        }
                    }
                }
            }
	    goto fake_end;
	    /*NOTREACHED*/	    

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
            sw = (cur_eval && (!n || cur_eval->u.eval.close_paren == n));
            break;

        case DEFINEP:  /*  (?(DEFINE))  */
            sw = 0;
            break;

	case IFTHEN:   /*  (?(cond)A|B)  */
	    PL_reg_leftiter = PL_reg_maxiter;		/* Void cache */
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
	    assert(0); /* NOTREACHED */
	}

	case CURLYX_end: /* just finished matching all of A*B */
	    cur_curlyx = ST.prev_curlyx;
	    sayYES;
	    assert(0); /* NOTREACHED */

	case CURLYX_end_fail: /* just failed to match all of A*B */
	    regcpblow(ST.cp);
	    cur_curlyx = ST.prev_curlyx;
	    sayNO;
	    assert(0); /* NOTREACHED */


#undef ST
#define ST st->u.whilem

	case WHILEM:     /* just matched an A in /A*B/  (for complex A) */
	{
	    /* see the discussion above about CURLYX/WHILEM */
	    I32 n;
	    int min = ARG1(cur_curlyx->u.curlyx.me);
	    int max = ARG2(cur_curlyx->u.curlyx.me);
	    regnode *A = NEXTOPER(cur_curlyx->u.curlyx.me) + EXTRA_STEP_2ARGS;

	    assert(cur_curlyx); /* keep Coverity happy */
	    n = ++cur_curlyx->u.curlyx.count; /* how many A's matched */
	    ST.save_lastloc = cur_curlyx->u.curlyx.lastloc;
	    ST.cache_offset = 0;
	    ST.cache_mask = 0;
	    

	    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
		  "%*s  whilem: matched %ld out of %d..%d\n",
		  REPORT_CODE_OFF+depth*2, "", (long)n, min, max)
	    );

	    /* First just match a string of min A's. */

	    if (n < min) {
		ST.cp = regcppush(rex, cur_curlyx->u.curlyx.parenfloor,
                                    maxopenparen);
		cur_curlyx->u.curlyx.lastloc = locinput;
		REGCP_SET(ST.lastcp);

		PUSH_STATE_GOTO(WHILEM_A_pre, A, locinput);
		assert(0); /* NOTREACHED */
	    }

	    /* If degenerate A matches "", assume A done. */

	    if (locinput == cur_curlyx->u.curlyx.lastloc) {
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
		   "%*s  whilem: empty match detected, trying continuation...\n",
		   REPORT_CODE_OFF+depth*2, "")
		);
		goto do_whilem_B_max;
	    }

	    /* super-linear cache processing */

	    if (scan->flags) {

		if (!PL_reg_maxiter) {
		    /* start the countdown: Postpone detection until we
		     * know the match is not *that* much linear. */
		    PL_reg_maxiter = (PL_regeol - PL_bostr + 1) * (scan->flags>>4);
		    /* possible overflow for long strings and many CURLYX's */
		    if (PL_reg_maxiter < 0)
			PL_reg_maxiter = I32_MAX;
		    PL_reg_leftiter = PL_reg_maxiter;
		}

		if (PL_reg_leftiter-- == 0) {
		    /* initialise cache */
		    const I32 size = (PL_reg_maxiter + 7)/8;
		    if (PL_reg_poscache) {
			if ((I32)PL_reg_poscache_size < size) {
			    Renew(PL_reg_poscache, size, char);
			    PL_reg_poscache_size = size;
			}
			Zero(PL_reg_poscache, size, char);
		    }
		    else {
			PL_reg_poscache_size = size;
			Newxz(PL_reg_poscache, size, char);
		    }
		    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
      "%swhilem: Detected a super-linear match, switching on caching%s...\n",
			      PL_colors[4], PL_colors[5])
		    );
		}

		if (PL_reg_leftiter < 0) {
		    /* have we already failed at this position? */
		    I32 offset, mask;
		    offset  = (scan->flags & 0xf) - 1
		  		+ (locinput - PL_bostr)  * (scan->flags>>4);
		    mask    = 1 << (offset % 8);
		    offset /= 8;
		    if (PL_reg_poscache[offset] & mask) {
			DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
			    "%*s  whilem: (cache) already tried at this position...\n",
			    REPORT_CODE_OFF+depth*2, "")
			);
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
		ST.cp = regcppush(rex, ST.save_curlyx->u.curlyx.parenfloor,
                            maxopenparen);
		REGCP_SET(ST.lastcp);
		PUSH_YES_STATE_GOTO(WHILEM_B_min, ST.save_curlyx->u.curlyx.B,
                                    locinput);
		assert(0); /* NOTREACHED */
	    }

	    /* Prefer A over B for maximal matching. */

	    if (n < max) { /* More greed allowed? */
		ST.cp = regcppush(rex, cur_curlyx->u.curlyx.parenfloor,
                            maxopenparen);
		cur_curlyx->u.curlyx.lastloc = locinput;
		REGCP_SET(ST.lastcp);
		PUSH_STATE_GOTO(WHILEM_A_max, A, locinput);
		assert(0); /* NOTREACHED */
	    }
	    goto do_whilem_B_max;
	}
	assert(0); /* NOTREACHED */

	case WHILEM_B_min: /* just matched B in a minimal match */
	case WHILEM_B_max: /* just matched B in a maximal match */
	    cur_curlyx = ST.save_curlyx;
	    sayYES;
	    assert(0); /* NOTREACHED */

	case WHILEM_B_max_fail: /* just failed to match B in a maximal match */
	    cur_curlyx = ST.save_curlyx;
	    cur_curlyx->u.curlyx.lastloc = ST.save_lastloc;
	    cur_curlyx->u.curlyx.count--;
	    CACHEsayNO;
	    assert(0); /* NOTREACHED */

	case WHILEM_A_min_fail: /* just failed to match A in a minimal match */
	    /* FALL THROUGH */
	case WHILEM_A_pre_fail: /* just failed to match even minimal A */
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex, &maxopenparen);
	    cur_curlyx->u.curlyx.lastloc = ST.save_lastloc;
	    cur_curlyx->u.curlyx.count--;
	    CACHEsayNO;
	    assert(0); /* NOTREACHED */

	case WHILEM_A_max_fail: /* just failed to match A in a maximal match */
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex, &maxopenparen); /* Restore some previous $<digit>s? */
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		"%*s  whilem: failed, trying continuation...\n",
		REPORT_CODE_OFF+depth*2, "")
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
	    assert(0); /* NOTREACHED */

	case WHILEM_B_min_fail: /* just failed to match B in a minimal match */
	    cur_curlyx = ST.save_curlyx;
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex, &maxopenparen);

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

	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		"%*s  trying longer...\n", REPORT_CODE_OFF+depth*2, "")
	    );
	    /* Try grabbing another A and see if it helps. */
	    cur_curlyx->u.curlyx.lastloc = locinput;
	    ST.cp = regcppush(rex, cur_curlyx->u.curlyx.parenfloor,
                            maxopenparen);
	    REGCP_SET(ST.lastcp);
	    PUSH_STATE_GOTO(WHILEM_A_min,
		/*A*/ NEXTOPER(ST.save_curlyx->u.curlyx.me) + EXTRA_STEP_2ARGS,
                locinput);
	    assert(0); /* NOTREACHED */

#undef  ST
#define ST st->u.branch

	case BRANCHJ:	    /*  /(...|A|...)/ with long next pointer */
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    scan = NEXTOPER(scan);
	    /* FALL THROUGH */

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
	    assert(0); /* NOTREACHED */

        case CUTGROUP:  /*  /(*THEN)/  */
            sv_yes_mark = st->u.mark.mark_name = scan->flags ? NULL :
                MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
            PUSH_STATE_GOTO(CUTGROUP_next, next, locinput);
            assert(0); /* NOTREACHED */

        case CUTGROUP_next_fail:
            do_cutgroup = 1;
            no_final = 1;
            if (st->u.mark.mark_name)
                sv_commit = st->u.mark.mark_name;
            sayNO;	    
            assert(0); /* NOTREACHED */

        case BRANCH_next:
            sayYES;
            assert(0); /* NOTREACHED */

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
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sBRANCH failed...%s\n",
			REPORT_CODE_OFF+depth*2, "", 
			PL_colors[4],
			PL_colors[5] );
		});
		sayNO_SILENT;
            }
	    continue; /* execute next BRANCH[J] op */
	    assert(0); /* NOTREACHED */
    
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
	    assert(0); /* NOTREACHED */

	case CURLYM_A: /* we've just matched an A */
	    ST.count++;
	    /* after first match, determine A's length: u.curlym.alen */
	    if (ST.count == 1) {
		if (PL_reg_match_utf8) {
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
		PerlIO_printf(Perl_debug_log,
			  "%*s  CURLYM now matched %"IVdf" times, len=%"IVdf"...\n",
			  (int)(REPORT_CODE_OFF+(depth*2)), "",
			  (IV) ST.count, (IV)ST.alen)
	    );

	    if (cur_eval && cur_eval->u.eval.close_paren && 
	        cur_eval->u.eval.close_paren == (U32)ST.me->flags) 
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
	        || (cur_eval && cur_eval->u.eval.close_paren &&
	            cur_eval->u.eval.close_paren == (U32)ST.me->flags))
		sayNO;

	  curlym_do_B: /* execute the B in /A{m,n}B/  */
	    if (ST.c1 == CHRTEST_UNINIT) {
		/* calculate c1 and c2 for possible match of 1st char
		 * following curly */
		ST.c1 = ST.c2 = CHRTEST_VOID;
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
                           is_utf8_pat))
                        {
                            sayNO;
                        }
		    }
		}
	    }

	    DEBUG_EXECUTE_r(
		PerlIO_printf(Perl_debug_log,
		    "%*s  CURLYM trying tail with matches=%"IVdf"...\n",
		    (int)(REPORT_CODE_OFF+(depth*2)),
		    "", (IV)ST.count)
		);
	    if (! NEXTCHR_IS_EOS && ST.c1 != CHRTEST_VOID) {
                if (! UTF8_IS_INVARIANT(nextchr) && utf8_target) {
                    if (memNE(locinput, ST.c1_utf8, UTF8SKIP(locinput))
                        && memNE(locinput, ST.c2_utf8, UTF8SKIP(locinput)))
                    {
                        /* simulate B failing */
                        DEBUG_OPTIMISE_r(
                            PerlIO_printf(Perl_debug_log,
                                "%*s  CURLYM Fast bail next target=U+%"UVXf" c1=U+%"UVXf" c2=U+%"UVXf"\n",
                                (int)(REPORT_CODE_OFF+(depth*2)),"",
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
                        PerlIO_printf(Perl_debug_log,
                            "%*s  CURLYM Fast bail next target=U+%X c1=U+%X c2=U+%X\n",
                            (int)(REPORT_CODE_OFF+(depth*2)),"",
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
			= HOPc(locinput, -ST.alen) - PL_bostr;
		    rex->offs[paren].end = locinput - PL_bostr;
		    if ((U32)paren > rex->lastparen)
			rex->lastparen = paren;
		    rex->lastcloseparen = paren;
		}
		else
		    rex->offs[paren].end = -1;
		if (cur_eval && cur_eval->u.eval.close_paren &&
		    cur_eval->u.eval.close_paren == (U32)ST.me->flags) 
		{
		    if (ST.count) 
	                goto fake_end;
	            else
	                sayNO;
	        }
	    }
	    
	    PUSH_STATE_GOTO(CURLYM_B, ST.B, locinput); /* match B */
	    assert(0); /* NOTREACHED */

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
	    rex->offs[paren].start = HOPc(locinput, -1) - PL_bostr; \
	    rex->offs[paren].end = locinput - PL_bostr; \
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
	    if (cur_eval && cur_eval->u.eval.close_paren &&
	        cur_eval->u.eval.close_paren == (U32)ST.paren) {
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
                           is_utf8_pat))
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
                        regrepeat(rex, &li, ST.A, ST.min, depth, is_utf8_pat)
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
		    ST.maxpos = PL_regeol - 1;
		    if (utf8_target)
			while (UTF8_IS_CONTINUATION(*(U8*)ST.maxpos))
			    ST.maxpos--;
		}
		else if (utf8_target) {
		    int m = ST.max - ST.min;
		    for (ST.maxpos = locinput;
			 m >0 && ST.maxpos < PL_regeol; m--)
			ST.maxpos += UTF8SKIP(ST.maxpos);
		}
		else {
		    ST.maxpos = locinput + ST.max - ST.min;
		    if (ST.maxpos >= PL_regeol)
			ST.maxpos = PL_regeol - 1;
		}
		goto curly_try_B_min_known;

	    }
	    else {
                /* avoid taking address of locinput, so it can remain
                 * a register var */
                char *li = locinput;
		ST.count = regrepeat(rex, &li, ST.A, ST.max, depth,
                                        is_utf8_pat);
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
	    assert(0); /* NOTREACHED */


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
			while (locinput <= ST.maxpos &&
			       UCHARAT(locinput) != ST.c1)
			    locinput++;
		    }
		    else {
			while (locinput <= ST.maxpos
			       && UCHARAT(locinput) != ST.c1
			       && UCHARAT(locinput) != ST.c2)
			    locinput++;
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
		    if (regrepeat(rex, &li, ST.A, n, depth, is_utf8_pat) < n)
			sayNO;
                    assert(n == REG_INFTY || locinput == li);
		}
		CURLY_SETPAREN(ST.paren, ST.count);
		if (cur_eval && cur_eval->u.eval.close_paren && 
		    cur_eval->u.eval.close_paren == (U32)ST.paren) {
		    goto fake_end;
	        }
		PUSH_STATE_GOTO(CURLY_B_min_known, ST.B, locinput);
	    }
	    assert(0); /* NOTREACHED */


	case CURLY_B_min_fail:
	    /* failed to find B in a non-greedy match where c1,c2 invalid */

	    REGCP_UNWIND(ST.cp);
            if (ST.paren) {
                UNWIND_PAREN(ST.lastparen, ST.lastcloseparen);
            }
	    /* failed -- move forward one */
            {
                char *li = locinput;
                if (!regrepeat(rex, &li, ST.A, 1, depth, is_utf8_pat)) {
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
		    if (cur_eval && cur_eval->u.eval.close_paren &&
		        cur_eval->u.eval.close_paren == (U32)ST.paren) {
                        goto fake_end;
                    }
		    PUSH_STATE_GOTO(CURLY_B_min, ST.B, locinput);
		}
	    }
            sayNO;
	    assert(0); /* NOTREACHED */


	curly_try_B_max:
	    /* a successful greedy match: now try to match B */
            if (cur_eval && cur_eval->u.eval.close_paren &&
                cur_eval->u.eval.close_paren == (U32)ST.paren) {
                goto fake_end;
            }
	    {
		bool could_match = locinput < PL_regeol;

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
		    assert(0); /* NOTREACHED */
		}
	    }
	    /* FALL THROUGH */

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
                st->u.eval.saved_utf8_pat = is_utf8_pat;
		is_utf8_pat = cur_eval->u.eval.saved_utf8_pat;

		st->u.eval.prev_rex = rex_sv;		/* inner */

                /* Save *all* the positions. */
		st->u.eval.cp = regcppush(rex, 0, maxopenparen);
		rex_sv = cur_eval->u.eval.prev_rex;
		SET_reg_curpm(rex_sv);
		rex = ReANY(rex_sv);
		rexi = RXi_GET(rex);
		cur_curlyx = cur_eval->u.eval.prev_curlyx;

		REGCP_SET(st->u.eval.lastcp);

		/* Restore parens of the outer rex without popping the
		 * savestack */
		S_regcp_restore(aTHX_ rex, cur_eval->u.eval.lastcp,
                                        &maxopenparen);

		st->u.eval.prev_eval = cur_eval;
		cur_eval = cur_eval->u.eval.prev_eval;
		DEBUG_EXECUTE_r(
		    PerlIO_printf(Perl_debug_log, "%*s  EVAL trying tail ... %"UVxf"\n",
				      REPORT_CODE_OFF+depth*2, "",PTR2UV(cur_eval)););
                if ( nochange_depth )
	            nochange_depth--;

                PUSH_YES_STATE_GOTO(EVAL_AB, st->u.eval.prev_eval->u.eval.B,
                                    locinput); /* match B */
	    }

	    if (locinput < reginfo->till) {
		DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
				      "%sMatch possible, but length=%ld is smaller than requested=%ld, failing!%s\n",
				      PL_colors[4],
				      (long)(locinput - PL_reg_starttry),
				      (long)(reginfo->till - PL_reg_starttry),
				      PL_colors[5]));
               				      
		sayNO_SILENT;		/* Cannot match: too short. */
	    }
	    sayYES;			/* Success! */

	case SUCCEED: /* successful SUSPEND/UNLESSM/IFMATCH/CURLYM */
	    DEBUG_EXECUTE_r(
	    PerlIO_printf(Perl_debug_log,
		"%*s  %ssubpattern success...%s\n",
		REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5]));
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
	    assert(0); /* NOTREACHED */
        }

	case IFMATCH_A_fail: /* body of (?...A) failed */
	    ST.wanted = !ST.wanted;
	    /* FALL THROUGH */

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
	    reginfo->cutpoint = PL_regeol;
	    /* FALLTHROUGH */

	case PRUNE:   /*  (*PRUNE)   */
	    if (!scan->flags)
	        sv_yes_mark = sv_commit = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
	    PUSH_STATE_GOTO(COMMIT_next, next, locinput);
	    assert(0); /* NOTREACHED */

	case COMMIT_next_fail:
	    no_final = 1;    
	    /* FALLTHROUGH */	    

	case OPFAIL:   /* (*FAIL)  */
	    sayNO;
	    assert(0); /* NOTREACHED */

#define ST st->u.mark
        case MARKPOINT: /*  (*MARK:foo)  */
            ST.prev_mark = mark_state;
            ST.mark_name = sv_commit = sv_yes_mark 
                = MUTABLE_SV(rexi->data->data[ ARG( scan ) ]);
            mark_state = st;
            ST.mark_loc = locinput;
            PUSH_YES_STATE_GOTO(MARKPOINT_next, next, locinput);
            assert(0); /* NOTREACHED */

        case MARKPOINT_next:
            mark_state = ST.prev_mark;
            sayYES;
            assert(0); /* NOTREACHED */

        case MARKPOINT_next_fail:
            if (popmark && sv_eq(ST.mark_name,popmark)) 
            {
                if (ST.mark_loc > startpoint)
	            reginfo->cutpoint = HOPBACKc(ST.mark_loc, 1);
                popmark = NULL; /* we found our mark */
                sv_commit = ST.mark_name;

                DEBUG_EXECUTE_r({
                        PerlIO_printf(Perl_debug_log,
		            "%*s  %ssetting cutpoint to mark:%"SVf"...%s\n",
		            REPORT_CODE_OFF+depth*2, "", 
		            PL_colors[4], SVfARG(sv_commit), PL_colors[5]);
		});
            }
            mark_state = ST.prev_mark;
            sv_yes_mark = mark_state ? 
                mark_state->u.mark.mark_name : NULL;
            sayNO;
            assert(0); /* NOTREACHED */

        case SKIP:  /*  (*SKIP)  */
            if (scan->flags) {
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
            assert(0); /* NOTREACHED */
#undef ST

        case LNBREAK: /* \R */
            if ((n=is_LNBREAK_safe(locinput, PL_regeol, utf8_target))) {
                locinput += n;
            } else
                sayNO;
            break;

	default:
	    PerlIO_printf(Perl_error_log, "%"UVxf" %d\n",
			  PTR2UV(scan), OP(scan));
	    Perl_croak(aTHX_ "regexp memory corruption");

        /* this is a point to jump to in order to increment
         * locinput by one character */
        increment_locinput:
            assert(!NEXTCHR_IS_EOS);
            if (utf8_target) {
                locinput += PL_utf8skip[nextchr];
                /* locinput is allowed to go 1 char off the end, but not 2+ */
                if (locinput > PL_regeol)
                    sayNO;
            }
            else
                locinput++;
            break;
	    
	} /* end switch */ 

        /* switch break jumps here */
	scan = next; /* prepare to execute the next op and ... */
	continue;    /* ... jump back to the top, reusing st */
	assert(0); /* NOTREACHED */

      push_yes_state:
	/* push a state that backtracks on success */
	st->u.yes.prev_yes_state = yes_state;
	yes_state = st;
	/* FALL THROUGH */
      push_state:
	/* push a new regex state, then continue at scan  */
	{
	    regmatch_state *newst;

	    DEBUG_STACK_r({
	        regmatch_state *cur = st;
	        regmatch_state *curyes = yes_state;
	        int curd = depth;
	        regmatch_slab *slab = PL_regmatch_slab;
                for (;curd > -1;cur--,curd--) {
                    if (cur < SLAB_FIRST(slab)) {
                	slab = slab->prev;
                	cur = SLAB_LAST(slab);
                    }
                    PerlIO_printf(Perl_error_log, "%*s#%-3d %-10s %s\n",
                        REPORT_CODE_OFF + 2 + depth * 2,"",
                        curd, PL_reg_name[cur->resume_state],
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
	    assert(0); /* NOTREACHED */
	}
    }

    /*
    * We get here only if there's trouble -- normally "case END" is
    * the terminating point.
    */
    Perl_croak(aTHX_ "corrupted regexp pointers");
    /*NOTREACHED*/
    sayNO;

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

    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%sMatch successful!%s\n",
			  PL_colors[4], PL_colors[5]));

    if (PL_reg_state.re_state_eval_setup_done) {
	/* each successfully executed (?{...}) block does the equivalent of
	 *   local $^R = do {...}
	 * When popping the save stack, all these locals would be undone;
	 * bypass this by setting the outermost saved $^R to the latest
	 * value */
	if (oreplsv != GvSV(PL_replgv))
	    sv_setsv(oreplsv, GvSV(PL_replgv));
    }
    result = 1;
    goto final_exit;

no:
    DEBUG_EXECUTE_r(
	PerlIO_printf(Perl_debug_log,
            "%*s  %sfailed...%s\n",
            REPORT_CODE_OFF+depth*2, "", 
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
        sv_setsv(sv_err, sv_commit);
        sv_setsv(sv_mrk, sv_yes_mark);
    }


    if (last_pushed_cv) {
	dSP;
	POP_MULTICALL;
        PERL_UNUSED_VAR(SP);
    }

    /* clean up; in particular, free all slabs above current one */
    LEAVE_SCOPE(oldsave);

    assert(!result ||  locinput - PL_bostr >= 0);
    return result ?  locinput - PL_bostr : -1;
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
 * max       - maximum number of things to match.
 * depth     - (for debugging) backtracking depth.
 */
STATIC I32
S_regrepeat(pTHX_ regexp *prog, char **startposp, const regnode *p,
                I32 max, int depth, bool is_utf8_pat)
{
    dVAR;
    char *scan;     /* Pointer to current position in target string */
    I32 c;
    char *loceol = PL_regeol;   /* local version */
    I32 hardcount = 0;  /* How many matches so far */
    bool utf8_target = PL_reg_match_utf8;
    int to_complement = 0;  /* Invert the result? */
    UV utf8_flags;
    _char_class_number classnum;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

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
	    while (scan < loceol && *scan != '\n')
		scan++;
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
    case CANY:  /* Move <scan> forward <max> bytes, unless goes off end */
        if (utf8_target && loceol - scan > max) {

            /* <loceol> hadn't been adjusted in the UTF-8 case */
            scan +=  max;
        }
        else {
            scan = loceol;
        }
	break;
    case EXACT:
        assert(STR_LEN(p) == is_utf8_pat ? UTF8SKIP(STRING(p)) : 1);

	c = (U8)*STRING(p);

        /* Can use a simple loop if the pattern char to match on is invariant
         * under UTF-8, or both target and pattern aren't UTF-8.  Note that we
         * can use UTF8_IS_INVARIANT() even if the pattern isn't UTF-8, as it's
         * true iff it doesn't matter if the argument is in UTF-8 or not */
        if (UTF8_IS_INVARIANT(c) || (! utf8_target && ! is_utf8_pat)) {
            if (utf8_target && loceol - scan > max) {
                /* We didn't adjust <loceol> because is UTF-8, but ok to do so,
                 * since here, to match at all, 1 char == 1 byte */
                loceol = scan + max;
            }
	    while (scan < loceol && UCHARAT(scan) == c) {
		scan++;
	    }
	}
	else if (is_utf8_pat) {
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
                 * pattern to non-UTF8, and do a simple loop */
                c = TWO_BYTE_UTF8_TO_UNI(c, *(STRING(p) + 1));
                while (scan < loceol && UCHARAT(scan) == c) {
                    scan++;
                }
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

    case EXACTFA:
	utf8_flags = FOLDEQ_UTF8_NOMIX_ASCII;
	goto do_exactf;

    case EXACTFL:
        RXp_MATCH_TAINTED_on(prog);
	utf8_flags = FOLDEQ_UTF8_LOCALE;
	goto do_exactf;

    case EXACTF:
	    utf8_flags = 0;
	    goto do_exactf;

    case EXACTFU_SS:
    case EXACTFU_TRICKYFOLD:
    case EXACTFU:
	utf8_flags = is_utf8_pat ? FOLDEQ_S2_ALREADY_FOLDED : 0;

    do_exactf: {
        int c1, c2;
        U8 c1_utf8[UTF8_MAXBYTES+1], c2_utf8[UTF8_MAXBYTES+1];

        assert(STR_LEN(p) == is_utf8_pat ? UTF8SKIP(STRING(p)) : 1);

        if (S_setup_EXACTISH_ST_c1_c2(aTHX_ p, &c1, c1_utf8, &c2, c2_utf8,
                                        is_utf8_pat))
        {
            if (c1 == CHRTEST_VOID) {
                /* Use full Unicode fold matching */
                char *tmpeol = PL_regeol;
                STRLEN pat_len = is_utf8_pat ? UTF8SKIP(STRING(p)) : 1;
                while (hardcount < max
                        && foldEQ_utf8_flags(scan, &tmpeol, 0, utf8_target,
                                             STRING(p), NULL, pat_len,
                                             is_utf8_pat, utf8_flags))
                {
                    scan = tmpeol;
                    tmpeol = PL_regeol;
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
                while (scan < loceol && UCHARAT(scan) == c1) {
                    scan++;
                }
            }
            else {
                while (scan < loceol &&
                    (UCHARAT(scan) == c1 || UCHARAT(scan) == c2))
                {
                    scan++;
                }
            }
	}
	break;
    }
    case ANYOF:
    case ANYOF_WARN_SUPER:
	if (utf8_target) {
	    while (hardcount < max
                   && scan < loceol
		   && reginclass(prog, p, (U8*)scan, utf8_target))
	    {
		scan += UTF8SKIP(scan);
		hardcount++;
	    }
	} else {
	    while (scan < loceol && REGINCLASS(prog, p, (U8*)scan))
		scan++;
	}
	break;

    /* The argument (FLAGS) to all the POSIX node types is the class number */

    case NPOSIXL:
        to_complement = 1;
        /* FALLTHROUGH */

    case POSIXL:
        RXp_MATCH_TAINTED_on(prog);
	if (! utf8_target) {
	    while (scan < loceol && to_complement ^ cBOOL(isFOO_lc(FLAGS(p),
                                                                   *scan)))
            {
		scan++;
            }
	} else {
	    while (hardcount < max && scan < loceol
                   && to_complement ^ cBOOL(isFOO_utf8_lc(FLAGS(p),
                                                                  (U8 *) scan)))
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
        /* FALL THROUGH */

    case NPOSIXA:
        if (! utf8_target) {
            while (scan < loceol && ! _generic_isCC_A((U8) *scan, FLAGS(p))) {
                scan++;
            }
        }
        else {

            /* The complement of something that matches only ASCII matches all
             * UTF-8 variant code points, plus everything in ASCII that isn't
             * in the class. */
	    while (hardcount < max && scan < loceol
                   && (! UTF8_IS_INVARIANT(*scan)
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
            if (classnum < _FIRST_NON_SWASH_CC) {

                /* Here, a swash is needed for above-Latin1 code points.
                 * Process as many Latin1 code points using the built-in rules.
                 * Go to another loop to finish processing upon encountering
                 * the first Latin1 code point.  We could do that in this loop
                 * as well, but the other way saves having to test if the swash
                 * has been loaded every time through the loop: extra space to
                 * save a test. */
                while (hardcount < max && scan < loceol) {
                    if (UTF8_IS_INVARIANT(*scan)) {
                        if (! (to_complement ^ cBOOL(_generic_isCC((U8) *scan,
                                                                   classnum))))
                        {
                            break;
                        }
                        scan++;
                    }
                    else if (UTF8_IS_DOWNGRADEABLE_START(*scan)) {
                        if (! (to_complement
                              ^ cBOOL(_generic_isCC(TWO_BYTE_UTF8_TO_UNI(*scan,
                                                                   *(scan + 1)),
                                                    classnum))))
                        {
                            break;
                        }
                        scan += 2;
                    }
                    else {
                        goto found_above_latin1;
                    }

                    hardcount++;
                }
            }
            else {
                /* For these character classes, the knowledge of how to handle
                 * every code point is compiled in to Perl via a macro.  This
                 * code is written for making the loops as tight as possible.
                 * It could be refactored to save space instead */
                switch (classnum) {
                    case _CC_ENUM_SPACE:    /* XXX would require separate code
                                               if we revert the change of \v
                                               matching this */
                        /* FALL THROUGH */
                    case _CC_ENUM_PSXSPC:
                        while (hardcount < max
                               && scan < loceol
                               && (to_complement ^ cBOOL(isSPACE_utf8(scan))))
                        {
                            scan += UTF8SKIP(scan);
                            hardcount++;
                        }
                        break;
                    case _CC_ENUM_BLANK:
                        while (hardcount < max
                               && scan < loceol
                               && (to_complement ^ cBOOL(isBLANK_utf8(scan))))
                        {
                            scan += UTF8SKIP(scan);
                            hardcount++;
                        }
                        break;
                    case _CC_ENUM_XDIGIT:
                        while (hardcount < max
                               && scan < loceol
                               && (to_complement ^ cBOOL(isXDIGIT_utf8(scan))))
                        {
                            scan += UTF8SKIP(scan);
                            hardcount++;
                        }
                        break;
                    case _CC_ENUM_VERTSPACE:
                        while (hardcount < max
                               && scan < loceol
                               && (to_complement ^ cBOOL(isVERTWS_utf8(scan))))
                        {
                            scan += UTF8SKIP(scan);
                            hardcount++;
                        }
                        break;
                    case _CC_ENUM_CNTRL:
                        while (hardcount < max
                               && scan < loceol
                               && (to_complement ^ cBOOL(isCNTRL_utf8(scan))))
                        {
                            scan += UTF8SKIP(scan);
                            hardcount++;
                        }
                        break;
                    default:
                        Perl_croak(aTHX_ "panic: regrepeat() node %d='%s' has an unexpected character class '%d'", OP(p), PL_reg_name[OP(p)], classnum);
                }
            }
	}
        break;

      found_above_latin1:   /* Continuation of POSIXU and NPOSIXU */

        /* Load the swash if not already present */
        if (! PL_utf8_swash_ptrs[classnum]) {
            U8 flags = _CORE_SWASH_INIT_ACCEPT_INVLIST;
            PL_utf8_swash_ptrs[classnum] = _core_swash_init(
                                        "utf8", swash_property_names[classnum],
                                        &PL_sv_undef, 1, 0, NULL, &flags);
        }

        while (hardcount < max && scan < loceol
               && to_complement ^ cBOOL(_generic_utf8(
                                       classnum,
                                       scan,
                                       swash_fetch(PL_utf8_swash_ptrs[classnum],
                                                   (U8 *) scan,
                                                   TRUE))))
        {
            scan += UTF8SKIP(scan);
            hardcount++;
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
            loceol = PL_regeol;
	    while (scan < loceol && (c=is_LNBREAK_latin1_safe(scan, loceol))) {
		scan+=c;
		hardcount++;
	    }
	}
	break;

    case BOUND:
    case BOUNDA:
    case BOUNDL:
    case BOUNDU:
    case EOS:
    case GPOS:
    case KEEPS:
    case NBOUND:
    case NBOUNDA:
    case NBOUNDL:
    case NBOUNDU:
    case OPFAIL:
    case SBOL:
    case SEOL:
        /* These are all 0 width, so match right here or not at all. */
        break;

    default:
        Perl_croak(aTHX_ "panic: regrepeat() called with unrecognized node type %d='%s'", OP(p), PL_reg_name[OP(p)]);
        assert(0); /* NOTREACHED */

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
	    regprop(prog, prop, p);
	    PerlIO_printf(Perl_debug_log,
			"%*s  %s can match %"IVdf" times out of %"IVdf"...\n",
			REPORT_CODE_OFF + depth*2, "", SvPVX_const(prop),(IV)c,(IV)max);
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

    return newSVsv(core_regclass_swash(prog, node, doinit, listsvp));
}
#endif

STATIC SV *
S_core_regclass_swash(pTHX_ const regexp *prog, const regnode* node, bool doinit, SV** listsvp)
{
    /* Returns the swash for the input 'node' in the regex 'prog'.
     * If <doinit> is true, will attempt to create the swash if not already
     *	  done.
     * If <listsvp> is non-null, will return the swash initialization string in
     *	  it.
     * Tied intimately to how regcomp.c sets up the data structure */

    dVAR;
    SV *sw  = NULL;
    SV *si  = NULL;
    SV*  invlist = NULL;

    RXi_GET_DECL(prog,progi);
    const struct reg_data * const data = prog ? progi->data : NULL;

    PERL_ARGS_ASSERT_CORE_REGCLASS_SWASH;

    assert(ANYOF_NONBITMAP(node));

    if (data && data->count) {
	const U32 n = ARG(node);

	if (data->what[n] == 's') {
	    SV * const rv = MUTABLE_SV(data->data[n]);
	    AV * const av = MUTABLE_AV(SvRV(rv));
	    SV **const ary = AvARRAY(av);
	    U8 swash_init_flags = _CORE_SWASH_INIT_ACCEPT_INVLIST;
	
	    si = *ary;	/* ary[0] = the string to initialize the swash with */

	    /* Elements 2 and 3 are either both present or both absent. [2] is
	     * any inversion list generated at compile time; [3] indicates if
	     * that inversion list has any user-defined properties in it. */
	    if (av_len(av) >= 2) {
		invlist = ary[2];
		if (SvUV(ary[3])) {
                    swash_init_flags |= _CORE_SWASH_INIT_USER_DEFINED_PROPERTY;
                }
	    }
	    else {
		invlist = NULL;
	    }

	    /* Element [1] is reserved for the set-up swash.  If already there,
	     * return it; if not, create it and store it there */
	    if (SvROK(ary[1])) {
		sw = ary[1];
	    }
	    else if (si && doinit) {

		sw = _core_swash_init("utf8", /* the utf8 package */
				      "", /* nameless */
				      si,
				      1, /* binary */
				      0, /* not from tr/// */
				      invlist,
				      &swash_init_flags);
		(void)av_store(av, 1, sw);
	    }
	}
    }
	
    if (listsvp) {
	SV* matches_string = newSVpvn("", 0);

	/* Use the swash, if any, which has to have incorporated into it all
	 * possibilities */
	if ((! sw || (invlist = _get_swash_invlist(sw)) == NULL)
            && (si && si != &PL_sv_undef))
        {

	    /* If no swash, use the input initialization string, if available */
	    sv_catsv(matches_string, si);
	}

	/* Add the inversion list to whatever we have.  This may have come from
	 * the swash, or from an input parameter */
	if (invlist) {
	    sv_catsv(matches_string, _invlist_contents(invlist));
	}
	*listsvp = matches_string;
    }

    return sw;
}

/*
 - reginclass - determine if a character falls into a character class
 
  n is the ANYOF regnode
  p is the target string
  utf8_target tells whether p is in UTF-8.

  Returns true if matched; false otherwise.

  Note that this can be a synthetic start class, a combination of various
  nodes, so things you think might be mutually exclusive, such as locale,
  aren't.  It can match both locale and non-locale

 */

STATIC bool
S_reginclass(pTHX_ regexp * const prog, const regnode * const n, const U8* const p, const bool utf8_target)
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
	c = utf8n_to_uvchr(p, UTF8_MAXBYTES, &c_len,
		(UTF8_ALLOW_DEFAULT & UTF8_ALLOW_ANYUV)
		| UTF8_ALLOW_FFFF | UTF8_CHECK_ONLY);
		/* see [perl #37836] for UTF8_ALLOW_ANYUV; [perl #38293] for
		 * UTF8_ALLOW_FFFF */
	if (c_len == (STRLEN)-1)
	    Perl_croak(aTHX_ "Malformed UTF-8 character (fatal)");
    }

    /* If this character is potentially in the bitmap, check it */
    if (c < 256) {
	if (ANYOF_BITMAP_TEST(n, c))
	    match = TRUE;
	else if (flags & ANYOF_NON_UTF8_LATIN1_ALL
		&& ! utf8_target
		&& ! isASCII(c))
	{
	    match = TRUE;
	}
	else if (flags & ANYOF_LOCALE) {
	    RXp_MATCH_TAINTED_on(prog);

	    if ((flags & ANYOF_LOC_FOLD)
		 && ANYOF_BITMAP_TEST(n, PL_fold_locale[c]))
	    {
		match = TRUE;
	    }
	    else if (ANYOF_CLASS_TEST_ANY_SET(n)) {

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
                    if (ANYOF_CLASS_TEST(n, count)
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
     * bitmap could match, try that.  Locale nodes specify completely the
     * behavior of code points in the bit map (otherwise, a utf8 target would
     * cause them to be treated as Unicode and not locale), except in
     * the very unlikely event when this node is a synthetic start class, which
     * could be a combination of locale and non-locale nodes.  So allow locale
     * to match for the synthetic start class, which will give a false
     * positive that will be resolved when the match is done again as not part
     * of the synthetic start class */
    if (!match) {
	if (utf8_target && (flags & ANYOF_UNICODE_ALL) && c >= 256) {
	    match = TRUE;	/* Everything above 255 matches */
	}
	else if (ANYOF_NONBITMAP(n)
		 && ((flags & ANYOF_NONBITMAP_NON_UTF8)
		     || (utf8_target
		         && (c >=256
			     || (! (flags & ANYOF_LOCALE))
			     || OP(n) == ANYOF_SYNTHETIC))))
	{
	    SV * const sw = core_regclass_swash(prog, n, TRUE, 0);
	    if (sw) {
		U8 * utf8_p;
		if (utf8_target) {
		    utf8_p = (U8 *) p;
		} else { /* Convert to utf8 */
		    STRLEN len = 1;
		    utf8_p = bytes_to_utf8(p, &len);
		}

		if (swash_fetch(sw, utf8_p, TRUE)) {
		    match = TRUE;
                }

		/* If we allocated a string above, free it */
		if (! utf8_target) Safefree(utf8_p);
	    }
	}

        if (UNICODE_IS_SUPER(c)
            && OP(n) == ANYOF_WARN_SUPER
            && ckWARN_d(WARN_NON_UNICODE))
        {
            Perl_warner(aTHX_ packWARN(WARN_NON_UNICODE),
                "Code point 0x%04"UVXf" is not Unicode, all \\p{} matches fail; all \\P{} matches succeed", c);
        }
    }

    /* The xor complements the return if to invert: 1^1 = 0, 1^0 = 1 */
    return cBOOL(flags & ANYOF_INVERT) ^ match;
}

STATIC U8 *
S_reghop3(U8 *s, I32 off, const U8* lim)
{
    /* return the position 'off' UTF-8 characters away from 's', forward if
     * 'off' >= 0, backwards if negative.  But don't go outside of position
     * 'lim', which better be < s  if off < 0 */

    dVAR;

    PERL_ARGS_ASSERT_REGHOP3;

    if (off >= 0) {
	while (off-- && s < lim) {
	    /* XXX could check well-formedness here */
	    s += UTF8SKIP(s);
	}
    }
    else {
        while (off++ && s > lim) {
            s--;
            if (UTF8_IS_CONTINUED(*s)) {
                while (s > lim && UTF8_IS_CONTINUATION(*s))
                    s--;
	    }
            /* XXX could check well-formedness here */
	}
    }
    return s;
}

#ifdef XXX_dmq
/* there are a bunch of places where we use two reghop3's that should
   be replaced with this routine. but since thats not done yet 
   we ifdef it out - dmq
*/
STATIC U8 *
S_reghop4(U8 *s, I32 off, const U8* llim, const U8* rlim)
{
    dVAR;

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
            }
            /* XXX could check well-formedness here */
        }
    }
    return s;
}
#endif

STATIC U8 *
S_reghopmaybe3(U8* s, I32 off, const U8* lim)
{
    dVAR;

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
	    }
            /* XXX could check well-formedness here */
	}
	if (off <= 0)
	    return NULL;
    }
    return s;
}

static void
restore_pos(pTHX_ void *arg)
{
    dVAR;
    regexp * const rex = (regexp *)arg;
    if (PL_reg_state.re_state_eval_setup_done) {
	if (PL_reg_oldsaved) {
	    rex->subbeg = PL_reg_oldsaved;
	    rex->sublen = PL_reg_oldsavedlen;
	    rex->suboffset = PL_reg_oldsavedoffset;
	    rex->subcoffset = PL_reg_oldsavedcoffset;
#ifdef PERL_ANY_COW
	    rex->saved_copy = PL_nrs;
#endif
	    RXp_MATCH_COPIED_on(rex);
	}
	PL_reg_magic->mg_len = PL_reg_oldpos;
	PL_reg_state.re_state_eval_setup_done = FALSE;
	PL_curpm = PL_reg_oldcurpm;
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

    dVAR;
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

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
