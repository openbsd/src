/*    regcomp.c
 */

/*
 * 'A fair jaw-cracker dwarf-language must be.'            --Samwise Gamgee
 *
 *     [p.285 of _The Lord of the Rings_, II/iii: "The Ring Goes South"]
 */

/* This file contains functions for compiling a regular expression.  See
 * also regexec.c which funnily enough, contains functions for executing
 * a regular expression.
 *
 * This file is also copied at build time to ext/re/re_comp.c, where
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
#define PERL_IN_REGCOMP_C
#include "perl.h"

#ifndef PERL_IN_XSUB_RE
#  include "INTERN.h"
#endif

#define REG_COMP_C
#ifdef PERL_IN_XSUB_RE
#  include "re_comp.h"
#else
#  include "regcomp.h"
#endif

#ifdef op
#undef op
#endif /* op */

#ifdef MSDOS
#  if defined(BUGGY_MSC6)
 /* MSC 6.00A breaks on op/regexp.t test 85 unless we turn this off */
#    pragma optimize("a",off)
 /* But MSC 6.00A is happy with 'w', for aliases only across function calls*/
#    pragma optimize("w",on )
#  endif /* BUGGY_MSC6 */
#endif /* MSDOS */

#ifndef STATIC
#define	STATIC	static
#endif

typedef struct RExC_state_t {
    U32		flags;			/* are we folding, multilining? */
    char	*precomp;		/* uncompiled string. */
    REGEXP	*rx_sv;			/* The SV that is the regexp. */
    regexp	*rx;                    /* perl core regexp structure */
    regexp_internal	*rxi;           /* internal data for regexp object pprivate field */        
    char	*start;			/* Start of input for compile */
    char	*end;			/* End of input for compile */
    char	*parse;			/* Input-scan pointer. */
    I32		whilem_seen;		/* number of WHILEM in this expr */
    regnode	*emit_start;		/* Start of emitted-code area */
    regnode	*emit_bound;		/* First regnode outside of the allocated space */
    regnode	*emit;			/* Code-emit pointer; &regdummy = don't = compiling */
    I32		naughty;		/* How bad is this pattern? */
    I32		sawback;		/* Did we see \1, ...? */
    U32		seen;
    I32		size;			/* Code size. */
    I32		npar;			/* Capture buffer count, (OPEN). */
    I32		cpar;			/* Capture buffer count, (CLOSE). */
    I32		nestroot;		/* root parens we are in - used by accept */
    I32		extralen;
    I32		seen_zerolen;
    I32		seen_evals;
    regnode	**open_parens;		/* pointers to open parens */
    regnode	**close_parens;		/* pointers to close parens */
    regnode	*opend;			/* END node in program */
    I32		utf8;		/* whether the pattern is utf8 or not */
    I32		orig_utf8;	/* whether the pattern was originally in utf8 */
				/* XXX use this for future optimisation of case
				 * where pattern must be upgraded to utf8. */
    HV		*paren_names;		/* Paren names */
    
    regnode	**recurse;		/* Recurse regops */
    I32		recurse_count;		/* Number of recurse regops */
#if ADD_TO_REGEXEC
    char 	*starttry;		/* -Dr: where regtry was called. */
#define RExC_starttry	(pRExC_state->starttry)
#endif
#ifdef DEBUGGING
    const char  *lastparse;
    I32         lastnum;
    AV          *paren_name_list;       /* idx -> name */
#define RExC_lastparse	(pRExC_state->lastparse)
#define RExC_lastnum	(pRExC_state->lastnum)
#define RExC_paren_name_list    (pRExC_state->paren_name_list)
#endif
} RExC_state_t;

#define RExC_flags	(pRExC_state->flags)
#define RExC_precomp	(pRExC_state->precomp)
#define RExC_rx_sv	(pRExC_state->rx_sv)
#define RExC_rx		(pRExC_state->rx)
#define RExC_rxi	(pRExC_state->rxi)
#define RExC_start	(pRExC_state->start)
#define RExC_end	(pRExC_state->end)
#define RExC_parse	(pRExC_state->parse)
#define RExC_whilem_seen	(pRExC_state->whilem_seen)
#ifdef RE_TRACK_PATTERN_OFFSETS
#define RExC_offsets	(pRExC_state->rxi->u.offsets) /* I am not like the others */
#endif
#define RExC_emit	(pRExC_state->emit)
#define RExC_emit_start	(pRExC_state->emit_start)
#define RExC_emit_bound	(pRExC_state->emit_bound)
#define RExC_naughty	(pRExC_state->naughty)
#define RExC_sawback	(pRExC_state->sawback)
#define RExC_seen	(pRExC_state->seen)
#define RExC_size	(pRExC_state->size)
#define RExC_npar	(pRExC_state->npar)
#define RExC_nestroot   (pRExC_state->nestroot)
#define RExC_extralen	(pRExC_state->extralen)
#define RExC_seen_zerolen	(pRExC_state->seen_zerolen)
#define RExC_seen_evals	(pRExC_state->seen_evals)
#define RExC_utf8	(pRExC_state->utf8)
#define RExC_orig_utf8	(pRExC_state->orig_utf8)
#define RExC_open_parens	(pRExC_state->open_parens)
#define RExC_close_parens	(pRExC_state->close_parens)
#define RExC_opend	(pRExC_state->opend)
#define RExC_paren_names	(pRExC_state->paren_names)
#define RExC_recurse	(pRExC_state->recurse)
#define RExC_recurse_count	(pRExC_state->recurse_count)


#define	ISMULT1(c)	((c) == '*' || (c) == '+' || (c) == '?')
#define	ISMULT2(s)	((*s) == '*' || (*s) == '+' || (*s) == '?' || \
	((*s) == '{' && regcurly(s)))

#ifdef SPSTART
#undef SPSTART		/* dratted cpp namespace... */
#endif
/*
 * Flags to be passed up and down.
 */
#define	WORST		0	/* Worst case. */
#define	HASWIDTH	0x01	/* Known to match non-null strings. */
#define	SIMPLE		0x02	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		0x04	/* Starts with * or +. */
#define TRYAGAIN	0x08	/* Weeded out a declaration. */
#define POSTPONED	0x10    /* (?1),(?&name), (??{...}) or similar */

#define REG_NODE_NUM(x) ((x) ? (int)((x)-RExC_emit_start) : -1)

/* whether trie related optimizations are enabled */
#if PERL_ENABLE_EXTENDED_TRIE_OPTIMISATION
#define TRIE_STUDY_OPT
#define FULL_TRIE_STUDY
#define TRIE_STCLASS
#endif



#define PBYTE(u8str,paren) ((U8*)(u8str))[(paren) >> 3]
#define PBITVAL(paren) (1 << ((paren) & 7))
#define PAREN_TEST(u8str,paren) ( PBYTE(u8str,paren) & PBITVAL(paren))
#define PAREN_SET(u8str,paren) PBYTE(u8str,paren) |= PBITVAL(paren)
#define PAREN_UNSET(u8str,paren) PBYTE(u8str,paren) &= (~PBITVAL(paren))


/* About scan_data_t.

  During optimisation we recurse through the regexp program performing
  various inplace (keyhole style) optimisations. In addition study_chunk
  and scan_commit populate this data structure with information about
  what strings MUST appear in the pattern. We look for the longest 
  string that must appear for at a fixed location, and we look for the
  longest string that may appear at a floating location. So for instance
  in the pattern:
  
    /FOO[xX]A.*B[xX]BAR/
    
  Both 'FOO' and 'A' are fixed strings. Both 'B' and 'BAR' are floating
  strings (because they follow a .* construct). study_chunk will identify
  both FOO and BAR as being the longest fixed and floating strings respectively.
  
  The strings can be composites, for instance
  
     /(f)(o)(o)/
     
  will result in a composite fixed substring 'foo'.
  
  For each string some basic information is maintained:
  
  - offset or min_offset
    This is the position the string must appear at, or not before.
    It also implicitly (when combined with minlenp) tells us how many
    character must match before the string we are searching.
    Likewise when combined with minlenp and the length of the string
    tells us how many characters must appear after the string we have 
    found.
  
  - max_offset
    Only used for floating strings. This is the rightmost point that
    the string can appear at. Ifset to I32 max it indicates that the
    string can occur infinitely far to the right.
  
  - minlenp
    A pointer to the minimum length of the pattern that the string 
    was found inside. This is important as in the case of positive 
    lookahead or positive lookbehind we can have multiple patterns 
    involved. Consider
    
    /(?=FOO).*F/
    
    The minimum length of the pattern overall is 3, the minimum length
    of the lookahead part is 3, but the minimum length of the part that
    will actually match is 1. So 'FOO's minimum length is 3, but the 
    minimum length for the F is 1. This is important as the minimum length
    is used to determine offsets in front of and behind the string being 
    looked for.  Since strings can be composites this is the length of the
    pattern at the time it was commited with a scan_commit. Note that
    the length is calculated by study_chunk, so that the minimum lengths
    are not known until the full pattern has been compiled, thus the 
    pointer to the value.
  
  - lookbehind
  
    In the case of lookbehind the string being searched for can be
    offset past the start point of the final matching string. 
    If this value was just blithely removed from the min_offset it would
    invalidate some of the calculations for how many chars must match
    before or after (as they are derived from min_offset and minlen and
    the length of the string being searched for). 
    When the final pattern is compiled and the data is moved from the
    scan_data_t structure into the regexp structure the information
    about lookbehind is factored in, with the information that would 
    have been lost precalculated in the end_shift field for the 
    associated string.

  The fields pos_min and pos_delta are used to store the minimum offset
  and the delta to the maximum offset at the current point in the pattern.    

*/

typedef struct scan_data_t {
    /*I32 len_min;      unused */
    /*I32 len_delta;    unused */
    I32 pos_min;
    I32 pos_delta;
    SV *last_found;
    I32 last_end;	    /* min value, <0 unless valid. */
    I32 last_start_min;
    I32 last_start_max;
    SV **longest;	    /* Either &l_fixed, or &l_float. */
    SV *longest_fixed;      /* longest fixed string found in pattern */
    I32 offset_fixed;       /* offset where it starts */
    I32 *minlen_fixed;      /* pointer to the minlen relevent to the string */
    I32 lookbehind_fixed;   /* is the position of the string modfied by LB */
    SV *longest_float;      /* longest floating string found in pattern */
    I32 offset_float_min;   /* earliest point in string it can appear */
    I32 offset_float_max;   /* latest point in string it can appear */
    I32 *minlen_float;      /* pointer to the minlen relevent to the string */
    I32 lookbehind_float;   /* is the position of the string modified by LB */
    I32 flags;
    I32 whilem_c;
    I32 *last_closep;
    struct regnode_charclass_class *start_class;
} scan_data_t;

/*
 * Forward declarations for pregcomp()'s friends.
 */

static const scan_data_t zero_scan_data =
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0};

#define SF_BEFORE_EOL		(SF_BEFORE_SEOL|SF_BEFORE_MEOL)
#define SF_BEFORE_SEOL		0x0001
#define SF_BEFORE_MEOL		0x0002
#define SF_FIX_BEFORE_EOL	(SF_FIX_BEFORE_SEOL|SF_FIX_BEFORE_MEOL)
#define SF_FL_BEFORE_EOL	(SF_FL_BEFORE_SEOL|SF_FL_BEFORE_MEOL)

#ifdef NO_UNARY_PLUS
#  define SF_FIX_SHIFT_EOL	(0+2)
#  define SF_FL_SHIFT_EOL		(0+4)
#else
#  define SF_FIX_SHIFT_EOL	(+2)
#  define SF_FL_SHIFT_EOL		(+4)
#endif

#define SF_FIX_BEFORE_SEOL	(SF_BEFORE_SEOL << SF_FIX_SHIFT_EOL)
#define SF_FIX_BEFORE_MEOL	(SF_BEFORE_MEOL << SF_FIX_SHIFT_EOL)

#define SF_FL_BEFORE_SEOL	(SF_BEFORE_SEOL << SF_FL_SHIFT_EOL)
#define SF_FL_BEFORE_MEOL	(SF_BEFORE_MEOL << SF_FL_SHIFT_EOL) /* 0x20 */
#define SF_IS_INF		0x0040
#define SF_HAS_PAR		0x0080
#define SF_IN_PAR		0x0100
#define SF_HAS_EVAL		0x0200
#define SCF_DO_SUBSTR		0x0400
#define SCF_DO_STCLASS_AND	0x0800
#define SCF_DO_STCLASS_OR	0x1000
#define SCF_DO_STCLASS		(SCF_DO_STCLASS_AND|SCF_DO_STCLASS_OR)
#define SCF_WHILEM_VISITED_POS	0x2000

#define SCF_TRIE_RESTUDY        0x4000 /* Do restudy? */
#define SCF_SEEN_ACCEPT         0x8000 

#define UTF (RExC_utf8 != 0)
#define LOC ((RExC_flags & RXf_PMf_LOCALE) != 0)
#define FOLD ((RExC_flags & RXf_PMf_FOLD) != 0)

#define OOB_UNICODE		12345678
#define OOB_NAMEDCLASS		-1

#define CHR_SVLEN(sv) (UTF ? sv_len_utf8(sv) : SvCUR(sv))
#define CHR_DIST(a,b) (UTF ? utf8_distance(a,b) : a - b)


/* length of regex to show in messages that don't mark a position within */
#define RegexLengthToShowInErrorMessages 127

/*
 * If MARKER[12] are adjusted, be sure to adjust the constants at the top
 * of t/op/regmesg.t, the tests in t/op/re_tests, and those in
 * op/pragma/warn/regcomp.
 */
#define MARKER1 "<-- HERE"    /* marker as it appears in the description */
#define MARKER2 " <-- HERE "  /* marker as it appears within the regex */

#define REPORT_LOCATION " in regex; marked by " MARKER1 " in m/%.*s" MARKER2 "%s/"

/*
 * Calls SAVEDESTRUCTOR_X if needed, then calls Perl_croak with the given
 * arg. Show regex, up to a maximum length. If it's too long, chop and add
 * "...".
 */
#define _FAIL(code) STMT_START {					\
    const char *ellipses = "";						\
    IV len = RExC_end - RExC_precomp;					\
									\
    if (!SIZE_ONLY)							\
	SAVEDESTRUCTOR_X(clear_re,(void*)RExC_rx_sv);			\
    if (len > RegexLengthToShowInErrorMessages) {			\
	/* chop 10 shorter than the max, to ensure meaning of "..." */	\
	len = RegexLengthToShowInErrorMessages - 10;			\
	ellipses = "...";						\
    }									\
    code;                                                               \
} STMT_END

#define	FAIL(msg) _FAIL(			    \
    Perl_croak(aTHX_ "%s in regex m/%.*s%s/",	    \
	    msg, (int)len, RExC_precomp, ellipses))

#define	FAIL2(msg,arg) _FAIL(			    \
    Perl_croak(aTHX_ msg " in regex m/%.*s%s/",	    \
	    arg, (int)len, RExC_precomp, ellipses))

/*
 * Simple_vFAIL -- like FAIL, but marks the current location in the scan
 */
#define	Simple_vFAIL(m) STMT_START {					\
    const IV offset = RExC_parse - RExC_precomp;			\
    Perl_croak(aTHX_ "%s" REPORT_LOCATION,				\
	    m, (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL()
 */
#define	vFAIL(m) STMT_START {				\
    if (!SIZE_ONLY)					\
	SAVEDESTRUCTOR_X(clear_re,(void*)RExC_rx_sv);	\
    Simple_vFAIL(m);					\
} STMT_END

/*
 * Like Simple_vFAIL(), but accepts two arguments.
 */
#define	Simple_vFAIL2(m,a1) STMT_START {			\
    const IV offset = RExC_parse - RExC_precomp;			\
    S_re_croak2(aTHX_ m, REPORT_LOCATION, a1,			\
	    (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL2().
 */
#define	vFAIL2(m,a1) STMT_START {			\
    if (!SIZE_ONLY)					\
	SAVEDESTRUCTOR_X(clear_re,(void*)RExC_rx_sv);	\
    Simple_vFAIL2(m, a1);				\
} STMT_END


/*
 * Like Simple_vFAIL(), but accepts three arguments.
 */
#define	Simple_vFAIL3(m, a1, a2) STMT_START {			\
    const IV offset = RExC_parse - RExC_precomp;		\
    S_re_croak2(aTHX_ m, REPORT_LOCATION, a1, a2,		\
	    (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL3().
 */
#define	vFAIL3(m,a1,a2) STMT_START {			\
    if (!SIZE_ONLY)					\
	SAVEDESTRUCTOR_X(clear_re,(void*)RExC_rx_sv);	\
    Simple_vFAIL3(m, a1, a2);				\
} STMT_END

/*
 * Like Simple_vFAIL(), but accepts four arguments.
 */
#define	Simple_vFAIL4(m, a1, a2, a3) STMT_START {		\
    const IV offset = RExC_parse - RExC_precomp;		\
    S_re_croak2(aTHX_ m, REPORT_LOCATION, a1, a2, a3,		\
	    (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

#define	ckWARNreg(loc,m) STMT_START {					\
    const IV offset = loc - RExC_precomp;				\
    Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,	\
	    (int)offset, RExC_precomp, RExC_precomp + offset);		\
} STMT_END

#define	ckWARNregdep(loc,m) STMT_START {				\
    const IV offset = loc - RExC_precomp;				\
    Perl_ck_warner_d(aTHX_ packWARN2(WARN_DEPRECATED, WARN_REGEXP),	\
	    m REPORT_LOCATION,						\
	    (int)offset, RExC_precomp, RExC_precomp + offset);		\
} STMT_END

#define	ckWARN2reg(loc, m, a1) STMT_START {				\
    const IV offset = loc - RExC_precomp;				\
    Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,	\
	    a1, (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

#define	vWARN3(loc, m, a1, a2) STMT_START {				\
    const IV offset = loc - RExC_precomp;				\
    Perl_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,		\
	    a1, a2, (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

#define	ckWARN3reg(loc, m, a1, a2) STMT_START {				\
    const IV offset = loc - RExC_precomp;				\
    Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,	\
	    a1, a2, (int)offset, RExC_precomp, RExC_precomp + offset);	\
} STMT_END

#define	vWARN4(loc, m, a1, a2, a3) STMT_START {				\
    const IV offset = loc - RExC_precomp;				\
    Perl_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,		\
	    a1, a2, a3, (int)offset, RExC_precomp, RExC_precomp + offset); \
} STMT_END

#define	ckWARN4reg(loc, m, a1, a2, a3) STMT_START {			\
    const IV offset = loc - RExC_precomp;				\
    Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,	\
	    a1, a2, a3, (int)offset, RExC_precomp, RExC_precomp + offset); \
} STMT_END

#define	vWARN5(loc, m, a1, a2, a3, a4) STMT_START {			\
    const IV offset = loc - RExC_precomp;				\
    Perl_warner(aTHX_ packWARN(WARN_REGEXP), m REPORT_LOCATION,		\
	    a1, a2, a3, a4, (int)offset, RExC_precomp, RExC_precomp + offset); \
} STMT_END


/* Allow for side effects in s */
#define REGC(c,s) STMT_START {			\
    if (!SIZE_ONLY) *(s) = (c); else (void)(s);	\
} STMT_END

/* Macros for recording node offsets.   20001227 mjd@plover.com 
 * Nodes are numbered 1, 2, 3, 4.  Node #n's position is recorded in
 * element 2*n-1 of the array.  Element #2n holds the byte length node #n.
 * Element 0 holds the number n.
 * Position is 1 indexed.
 */
#ifndef RE_TRACK_PATTERN_OFFSETS
#define Set_Node_Offset_To_R(node,byte)
#define Set_Node_Offset(node,byte)
#define Set_Cur_Node_Offset
#define Set_Node_Length_To_R(node,len)
#define Set_Node_Length(node,len)
#define Set_Node_Cur_Length(node)
#define Node_Offset(n) 
#define Node_Length(n) 
#define Set_Node_Offset_Length(node,offset,len)
#define ProgLen(ri) ri->u.proglen
#define SetProgLen(ri,x) ri->u.proglen = x
#else
#define ProgLen(ri) ri->u.offsets[0]
#define SetProgLen(ri,x) ri->u.offsets[0] = x
#define Set_Node_Offset_To_R(node,byte) STMT_START {			\
    if (! SIZE_ONLY) {							\
	MJD_OFFSET_DEBUG(("** (%d) offset of node %d is %d.\n",		\
		    __LINE__, (int)(node), (int)(byte)));		\
	if((node) < 0) {						\
	    Perl_croak(aTHX_ "value of node is %d in Offset macro", (int)(node)); \
	} else {							\
	    RExC_offsets[2*(node)-1] = (byte);				\
	}								\
    }									\
} STMT_END

#define Set_Node_Offset(node,byte) \
    Set_Node_Offset_To_R((node)-RExC_emit_start, (byte)-RExC_start)
#define Set_Cur_Node_Offset Set_Node_Offset(RExC_emit, RExC_parse)

#define Set_Node_Length_To_R(node,len) STMT_START {			\
    if (! SIZE_ONLY) {							\
	MJD_OFFSET_DEBUG(("** (%d) size of node %d is %d.\n",		\
		__LINE__, (int)(node), (int)(len)));			\
	if((node) < 0) {						\
	    Perl_croak(aTHX_ "value of node is %d in Length macro", (int)(node)); \
	} else {							\
	    RExC_offsets[2*(node)] = (len);				\
	}								\
    }									\
} STMT_END

#define Set_Node_Length(node,len) \
    Set_Node_Length_To_R((node)-RExC_emit_start, len)
#define Set_Cur_Node_Length(len) Set_Node_Length(RExC_emit, len)
#define Set_Node_Cur_Length(node) \
    Set_Node_Length(node, RExC_parse - parse_start)

/* Get offsets and lengths */
#define Node_Offset(n) (RExC_offsets[2*((n)-RExC_emit_start)-1])
#define Node_Length(n) (RExC_offsets[2*((n)-RExC_emit_start)])

#define Set_Node_Offset_Length(node,offset,len) STMT_START {	\
    Set_Node_Offset_To_R((node)-RExC_emit_start, (offset));	\
    Set_Node_Length_To_R((node)-RExC_emit_start, (len));	\
} STMT_END
#endif

#if PERL_ENABLE_EXPERIMENTAL_REGEX_OPTIMISATIONS
#define EXPERIMENTAL_INPLACESCAN
#endif /*RE_TRACK_PATTERN_OFFSETS*/

#define DEBUG_STUDYDATA(str,data,depth)                              \
DEBUG_OPTIMISE_MORE_r(if(data){                                      \
    PerlIO_printf(Perl_debug_log,                                    \
        "%*s" str "Pos:%"IVdf"/%"IVdf                                \
        " Flags: 0x%"UVXf" Whilem_c: %"IVdf" Lcp: %"IVdf" %s",       \
        (int)(depth)*2, "",                                          \
        (IV)((data)->pos_min),                                       \
        (IV)((data)->pos_delta),                                     \
        (UV)((data)->flags),                                         \
        (IV)((data)->whilem_c),                                      \
        (IV)((data)->last_closep ? *((data)->last_closep) : -1),     \
        is_inf ? "INF " : ""                                         \
    );                                                               \
    if ((data)->last_found)                                          \
        PerlIO_printf(Perl_debug_log,                                \
            "Last:'%s' %"IVdf":%"IVdf"/%"IVdf" %sFixed:'%s' @ %"IVdf \
            " %sFloat: '%s' @ %"IVdf"/%"IVdf"",                      \
            SvPVX_const((data)->last_found),                         \
            (IV)((data)->last_end),                                  \
            (IV)((data)->last_start_min),                            \
            (IV)((data)->last_start_max),                            \
            ((data)->longest &&                                      \
             (data)->longest==&((data)->longest_fixed)) ? "*" : "",  \
            SvPVX_const((data)->longest_fixed),                      \
            (IV)((data)->offset_fixed),                              \
            ((data)->longest &&                                      \
             (data)->longest==&((data)->longest_float)) ? "*" : "",  \
            SvPVX_const((data)->longest_float),                      \
            (IV)((data)->offset_float_min),                          \
            (IV)((data)->offset_float_max)                           \
        );                                                           \
    PerlIO_printf(Perl_debug_log,"\n");                              \
});

static void clear_re(pTHX_ void *r);

/* Mark that we cannot extend a found fixed substring at this point.
   Update the longest found anchored substring and the longest found
   floating substrings if needed. */

STATIC void
S_scan_commit(pTHX_ const RExC_state_t *pRExC_state, scan_data_t *data, I32 *minlenp, int is_inf)
{
    const STRLEN l = CHR_SVLEN(data->last_found);
    const STRLEN old_l = CHR_SVLEN(*data->longest);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_SCAN_COMMIT;

    if ((l >= old_l) && ((l > old_l) || (data->flags & SF_BEFORE_EOL))) {
	SvSetMagicSV(*data->longest, data->last_found);
	if (*data->longest == data->longest_fixed) {
	    data->offset_fixed = l ? data->last_start_min : data->pos_min;
	    if (data->flags & SF_BEFORE_EOL)
		data->flags
		    |= ((data->flags & SF_BEFORE_EOL) << SF_FIX_SHIFT_EOL);
	    else
		data->flags &= ~SF_FIX_BEFORE_EOL;
	    data->minlen_fixed=minlenp;	
	    data->lookbehind_fixed=0;
	}
	else { /* *data->longest == data->longest_float */
	    data->offset_float_min = l ? data->last_start_min : data->pos_min;
	    data->offset_float_max = (l
				      ? data->last_start_max
				      : data->pos_min + data->pos_delta);
	    if (is_inf || (U32)data->offset_float_max > (U32)I32_MAX)
		data->offset_float_max = I32_MAX;
	    if (data->flags & SF_BEFORE_EOL)
		data->flags
		    |= ((data->flags & SF_BEFORE_EOL) << SF_FL_SHIFT_EOL);
	    else
		data->flags &= ~SF_FL_BEFORE_EOL;
            data->minlen_float=minlenp;
            data->lookbehind_float=0;
	}
    }
    SvCUR_set(data->last_found, 0);
    {
	SV * const sv = data->last_found;
	if (SvUTF8(sv) && SvMAGICAL(sv)) {
	    MAGIC * const mg = mg_find(sv, PERL_MAGIC_utf8);
	    if (mg)
		mg->mg_len = 0;
	}
    }
    data->last_end = -1;
    data->flags &= ~SF_BEFORE_EOL;
    DEBUG_STUDYDATA("commit: ",data,0);
}

/* Can match anything (initialization) */
STATIC void
S_cl_anything(const RExC_state_t *pRExC_state, struct regnode_charclass_class *cl)
{
    PERL_ARGS_ASSERT_CL_ANYTHING;

    ANYOF_CLASS_ZERO(cl);
    ANYOF_BITMAP_SETALL(cl);
    cl->flags = ANYOF_EOS|ANYOF_UNICODE_ALL;
    if (LOC)
	cl->flags |= ANYOF_LOCALE;
}

/* Can match anything (initialization) */
STATIC int
S_cl_is_anything(const struct regnode_charclass_class *cl)
{
    int value;

    PERL_ARGS_ASSERT_CL_IS_ANYTHING;

    for (value = 0; value <= ANYOF_MAX; value += 2)
	if (ANYOF_CLASS_TEST(cl, value) && ANYOF_CLASS_TEST(cl, value + 1))
	    return 1;
    if (!(cl->flags & ANYOF_UNICODE_ALL))
	return 0;
    if (!ANYOF_BITMAP_TESTALLSET((const void*)cl))
	return 0;
    return 1;
}

/* Can match anything (initialization) */
STATIC void
S_cl_init(const RExC_state_t *pRExC_state, struct regnode_charclass_class *cl)
{
    PERL_ARGS_ASSERT_CL_INIT;

    Zero(cl, 1, struct regnode_charclass_class);
    cl->type = ANYOF;
    cl_anything(pRExC_state, cl);
}

STATIC void
S_cl_init_zero(const RExC_state_t *pRExC_state, struct regnode_charclass_class *cl)
{
    PERL_ARGS_ASSERT_CL_INIT_ZERO;

    Zero(cl, 1, struct regnode_charclass_class);
    cl->type = ANYOF;
    cl_anything(pRExC_state, cl);
    if (LOC)
	cl->flags |= ANYOF_LOCALE;
}

/* 'And' a given class with another one.  Can create false positives */
/* We assume that cl is not inverted */
STATIC void
S_cl_and(struct regnode_charclass_class *cl,
	const struct regnode_charclass_class *and_with)
{
    PERL_ARGS_ASSERT_CL_AND;

    assert(and_with->type == ANYOF);
    if (!(and_with->flags & ANYOF_CLASS)
	&& !(cl->flags & ANYOF_CLASS)
	&& (and_with->flags & ANYOF_LOCALE) == (cl->flags & ANYOF_LOCALE)
	&& !(and_with->flags & ANYOF_FOLD)
	&& !(cl->flags & ANYOF_FOLD)) {
	int i;

	if (and_with->flags & ANYOF_INVERT)
	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] &= ~and_with->bitmap[i];
	else
	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] &= and_with->bitmap[i];
    } /* XXXX: logic is complicated otherwise, leave it along for a moment. */
    if (!(and_with->flags & ANYOF_EOS))
	cl->flags &= ~ANYOF_EOS;

    if (cl->flags & ANYOF_UNICODE_ALL && and_with->flags & ANYOF_UNICODE &&
	!(and_with->flags & ANYOF_INVERT)) {
	cl->flags &= ~ANYOF_UNICODE_ALL;
	cl->flags |= ANYOF_UNICODE;
	ARG_SET(cl, ARG(and_with));
    }
    if (!(and_with->flags & ANYOF_UNICODE_ALL) &&
	!(and_with->flags & ANYOF_INVERT))
	cl->flags &= ~ANYOF_UNICODE_ALL;
    if (!(and_with->flags & (ANYOF_UNICODE|ANYOF_UNICODE_ALL)) &&
	!(and_with->flags & ANYOF_INVERT))
	cl->flags &= ~ANYOF_UNICODE;
}

/* 'OR' a given class with another one.  Can create false positives */
/* We assume that cl is not inverted */
STATIC void
S_cl_or(const RExC_state_t *pRExC_state, struct regnode_charclass_class *cl, const struct regnode_charclass_class *or_with)
{
    PERL_ARGS_ASSERT_CL_OR;

    if (or_with->flags & ANYOF_INVERT) {
	/* We do not use
	 * (B1 | CL1) | (!B2 & !CL2) = (B1 | !B2 & !CL2) | (CL1 | (!B2 & !CL2))
	 *   <= (B1 | !B2) | (CL1 | !CL2)
	 * which is wasteful if CL2 is small, but we ignore CL2:
	 *   (B1 | CL1) | (!B2 & !CL2) <= (B1 | CL1) | !B2 = (B1 | !B2) | CL1
	 * XXXX Can we handle case-fold?  Unclear:
	 *   (OK1(i) | OK1(i')) | !(OK1(i) | OK1(i')) =
	 *   (OK1(i) | OK1(i')) | (!OK1(i) & !OK1(i'))
	 */
	if ( (or_with->flags & ANYOF_LOCALE) == (cl->flags & ANYOF_LOCALE)
	     && !(or_with->flags & ANYOF_FOLD)
	     && !(cl->flags & ANYOF_FOLD) ) {
	    int i;

	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] |= ~or_with->bitmap[i];
	} /* XXXX: logic is complicated otherwise */
	else {
	    cl_anything(pRExC_state, cl);
	}
    } else {
	/* (B1 | CL1) | (B2 | CL2) = (B1 | B2) | (CL1 | CL2)) */
	if ( (or_with->flags & ANYOF_LOCALE) == (cl->flags & ANYOF_LOCALE)
	     && (!(or_with->flags & ANYOF_FOLD)
		 || (cl->flags & ANYOF_FOLD)) ) {
	    int i;

	    /* OR char bitmap and class bitmap separately */
	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] |= or_with->bitmap[i];
	    if (or_with->flags & ANYOF_CLASS) {
		for (i = 0; i < ANYOF_CLASSBITMAP_SIZE; i++)
		    cl->classflags[i] |= or_with->classflags[i];
		cl->flags |= ANYOF_CLASS;
	    }
	}
	else { /* XXXX: logic is complicated, leave it along for a moment. */
	    cl_anything(pRExC_state, cl);
	}
    }
    if (or_with->flags & ANYOF_EOS)
	cl->flags |= ANYOF_EOS;

    if (cl->flags & ANYOF_UNICODE && or_with->flags & ANYOF_UNICODE &&
	ARG(cl) != ARG(or_with)) {
	cl->flags |= ANYOF_UNICODE_ALL;
	cl->flags &= ~ANYOF_UNICODE;
    }
    if (or_with->flags & ANYOF_UNICODE_ALL) {
	cl->flags |= ANYOF_UNICODE_ALL;
	cl->flags &= ~ANYOF_UNICODE;
    }
}

#define TRIE_LIST_ITEM(state,idx) (trie->states[state].trans.list)[ idx ]
#define TRIE_LIST_CUR(state)  ( TRIE_LIST_ITEM( state, 0 ).forid )
#define TRIE_LIST_LEN(state) ( TRIE_LIST_ITEM( state, 0 ).newstate )
#define TRIE_LIST_USED(idx)  ( trie->states[state].trans.list ? (TRIE_LIST_CUR( idx ) - 1) : 0 )


#ifdef DEBUGGING
/*
   dump_trie(trie,widecharmap,revcharmap)
   dump_trie_interim_list(trie,widecharmap,revcharmap,next_alloc)
   dump_trie_interim_table(trie,widecharmap,revcharmap,next_alloc)

   These routines dump out a trie in a somewhat readable format.
   The _interim_ variants are used for debugging the interim
   tables that are used to generate the final compressed
   representation which is what dump_trie expects.

   Part of the reason for their existance is to provide a form
   of documentation as to how the different representations function.

*/

/*
  Dumps the final compressed table form of the trie to Perl_debug_log.
  Used for debugging make_trie().
*/
 
STATIC void
S_dump_trie(pTHX_ const struct _reg_trie_data *trie, HV *widecharmap,
	    AV *revcharmap, U32 depth)
{
    U32 state;
    SV *sv=sv_newmortal();
    int colwidth= widecharmap ? 6 : 4;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_DUMP_TRIE;

    PerlIO_printf( Perl_debug_log, "%*sChar : %-6s%-6s%-4s ",
        (int)depth * 2 + 2,"",
        "Match","Base","Ofs" );

    for( state = 0 ; state < trie->uniquecharcount ; state++ ) {
	SV ** const tmp = av_fetch( revcharmap, state, 0);
        if ( tmp ) {
            PerlIO_printf( Perl_debug_log, "%*s", 
                colwidth,
                pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), colwidth, 
	                    PL_colors[0], PL_colors[1],
	                    (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0) |
	                    PERL_PV_ESCAPE_FIRSTCHAR 
                ) 
            );
        }
    }
    PerlIO_printf( Perl_debug_log, "\n%*sState|-----------------------",
        (int)depth * 2 + 2,"");

    for( state = 0 ; state < trie->uniquecharcount ; state++ )
        PerlIO_printf( Perl_debug_log, "%.*s", colwidth, "--------");
    PerlIO_printf( Perl_debug_log, "\n");

    for( state = 1 ; state < trie->statecount ; state++ ) {
	const U32 base = trie->states[ state ].trans.base;

        PerlIO_printf( Perl_debug_log, "%*s#%4"UVXf"|", (int)depth * 2 + 2,"", (UV)state);

        if ( trie->states[ state ].wordnum ) {
            PerlIO_printf( Perl_debug_log, " W%4X", trie->states[ state ].wordnum );
        } else {
            PerlIO_printf( Perl_debug_log, "%6s", "" );
        }

        PerlIO_printf( Perl_debug_log, " @%4"UVXf" ", (UV)base );

        if ( base ) {
            U32 ofs = 0;

            while( ( base + ofs  < trie->uniquecharcount ) ||
                   ( base + ofs - trie->uniquecharcount < trie->lasttrans
                     && trie->trans[ base + ofs - trie->uniquecharcount ].check != state))
                    ofs++;

            PerlIO_printf( Perl_debug_log, "+%2"UVXf"[ ", (UV)ofs);

            for ( ofs = 0 ; ofs < trie->uniquecharcount ; ofs++ ) {
                if ( ( base + ofs >= trie->uniquecharcount ) &&
                     ( base + ofs - trie->uniquecharcount < trie->lasttrans ) &&
                     trie->trans[ base + ofs - trie->uniquecharcount ].check == state )
                {
                   PerlIO_printf( Perl_debug_log, "%*"UVXf,
                    colwidth,
                    (UV)trie->trans[ base + ofs - trie->uniquecharcount ].next );
                } else {
                    PerlIO_printf( Perl_debug_log, "%*s",colwidth,"   ." );
                }
            }

            PerlIO_printf( Perl_debug_log, "]");

        }
        PerlIO_printf( Perl_debug_log, "\n" );
    }
}    
/*
  Dumps a fully constructed but uncompressed trie in list form.
  List tries normally only are used for construction when the number of 
  possible chars (trie->uniquecharcount) is very high.
  Used for debugging make_trie().
*/
STATIC void
S_dump_trie_interim_list(pTHX_ const struct _reg_trie_data *trie,
			 HV *widecharmap, AV *revcharmap, U32 next_alloc,
			 U32 depth)
{
    U32 state;
    SV *sv=sv_newmortal();
    int colwidth= widecharmap ? 6 : 4;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_DUMP_TRIE_INTERIM_LIST;

    /* print out the table precompression.  */
    PerlIO_printf( Perl_debug_log, "%*sState :Word | Transition Data\n%*s%s",
        (int)depth * 2 + 2,"", (int)depth * 2 + 2,"",
        "------:-----+-----------------\n" );
    
    for( state=1 ; state < next_alloc ; state ++ ) {
        U16 charid;
    
        PerlIO_printf( Perl_debug_log, "%*s %4"UVXf" :",
            (int)depth * 2 + 2,"", (UV)state  );
        if ( ! trie->states[ state ].wordnum ) {
            PerlIO_printf( Perl_debug_log, "%5s| ","");
        } else {
            PerlIO_printf( Perl_debug_log, "W%4x| ",
                trie->states[ state ].wordnum
            );
        }
        for( charid = 1 ; charid <= TRIE_LIST_USED( state ) ; charid++ ) {
	    SV ** const tmp = av_fetch( revcharmap, TRIE_LIST_ITEM(state,charid).forid, 0);
	    if ( tmp ) {
                PerlIO_printf( Perl_debug_log, "%*s:%3X=%4"UVXf" | ",
                    colwidth,
                    pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), colwidth, 
	                    PL_colors[0], PL_colors[1],
	                    (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0) |
	                    PERL_PV_ESCAPE_FIRSTCHAR 
                    ) ,
                    TRIE_LIST_ITEM(state,charid).forid,
                    (UV)TRIE_LIST_ITEM(state,charid).newstate
                );
                if (!(charid % 10)) 
                    PerlIO_printf(Perl_debug_log, "\n%*s| ",
                        (int)((depth * 2) + 14), "");
            }
        }
        PerlIO_printf( Perl_debug_log, "\n");
    }
}    

/*
  Dumps a fully constructed but uncompressed trie in table form.
  This is the normal DFA style state transition table, with a few 
  twists to facilitate compression later. 
  Used for debugging make_trie().
*/
STATIC void
S_dump_trie_interim_table(pTHX_ const struct _reg_trie_data *trie,
			  HV *widecharmap, AV *revcharmap, U32 next_alloc,
			  U32 depth)
{
    U32 state;
    U16 charid;
    SV *sv=sv_newmortal();
    int colwidth= widecharmap ? 6 : 4;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_DUMP_TRIE_INTERIM_TABLE;
    
    /*
       print out the table precompression so that we can do a visual check
       that they are identical.
     */
    
    PerlIO_printf( Perl_debug_log, "%*sChar : ",(int)depth * 2 + 2,"" );

    for( charid = 0 ; charid < trie->uniquecharcount ; charid++ ) {
	SV ** const tmp = av_fetch( revcharmap, charid, 0);
        if ( tmp ) {
            PerlIO_printf( Perl_debug_log, "%*s", 
                colwidth,
                pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), colwidth, 
	                    PL_colors[0], PL_colors[1],
	                    (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0) |
	                    PERL_PV_ESCAPE_FIRSTCHAR 
                ) 
            );
        }
    }

    PerlIO_printf( Perl_debug_log, "\n%*sState+-",(int)depth * 2 + 2,"" );

    for( charid=0 ; charid < trie->uniquecharcount ; charid++ ) {
        PerlIO_printf( Perl_debug_log, "%.*s", colwidth,"--------");
    }

    PerlIO_printf( Perl_debug_log, "\n" );

    for( state=1 ; state < next_alloc ; state += trie->uniquecharcount ) {

        PerlIO_printf( Perl_debug_log, "%*s%4"UVXf" : ", 
            (int)depth * 2 + 2,"",
            (UV)TRIE_NODENUM( state ) );

        for( charid = 0 ; charid < trie->uniquecharcount ; charid++ ) {
            UV v=(UV)SAFE_TRIE_NODENUM( trie->trans[ state + charid ].next );
            if (v)
                PerlIO_printf( Perl_debug_log, "%*"UVXf, colwidth, v );
            else
                PerlIO_printf( Perl_debug_log, "%*s", colwidth, "." );
        }
        if ( ! trie->states[ TRIE_NODENUM( state ) ].wordnum ) {
            PerlIO_printf( Perl_debug_log, " (%4"UVXf")\n", (UV)trie->trans[ state ].check );
        } else {
            PerlIO_printf( Perl_debug_log, " (%4"UVXf") W%4X\n", (UV)trie->trans[ state ].check,
            trie->states[ TRIE_NODENUM( state ) ].wordnum );
        }
    }
}

#endif

/* make_trie(startbranch,first,last,tail,word_count,flags,depth)
  startbranch: the first branch in the whole branch sequence
  first      : start branch of sequence of branch-exact nodes.
	       May be the same as startbranch
  last       : Thing following the last branch.
	       May be the same as tail.
  tail       : item following the branch sequence
  count      : words in the sequence
  flags      : currently the OP() type we will be building one of /EXACT(|F|Fl)/
  depth      : indent depth

Inplace optimizes a sequence of 2 or more Branch-Exact nodes into a TRIE node.

A trie is an N'ary tree where the branches are determined by digital
decomposition of the key. IE, at the root node you look up the 1st character and
follow that branch repeat until you find the end of the branches. Nodes can be
marked as "accepting" meaning they represent a complete word. Eg:

  /he|she|his|hers/

would convert into the following structure. Numbers represent states, letters
following numbers represent valid transitions on the letter from that state, if
the number is in square brackets it represents an accepting state, otherwise it
will be in parenthesis.

      +-h->+-e->[3]-+-r->(8)-+-s->[9]
      |    |
      |   (2)
      |    |
     (1)   +-i->(6)-+-s->[7]
      |
      +-s->(3)-+-h->(4)-+-e->[5]

      Accept Word Mapping: 3=>1 (he),5=>2 (she), 7=>3 (his), 9=>4 (hers)

This shows that when matching against the string 'hers' we will begin at state 1
read 'h' and move to state 2, read 'e' and move to state 3 which is accepting,
then read 'r' and go to state 8 followed by 's' which takes us to state 9 which
is also accepting. Thus we know that we can match both 'he' and 'hers' with a
single traverse. We store a mapping from accepting to state to which word was
matched, and then when we have multiple possibilities we try to complete the
rest of the regex in the order in which they occured in the alternation.

The only prior NFA like behaviour that would be changed by the TRIE support is
the silent ignoring of duplicate alternations which are of the form:

 / (DUPE|DUPE) X? (?{ ... }) Y /x

Thus EVAL blocks follwing a trie may be called a different number of times with
and without the optimisation. With the optimisations dupes will be silently
ignored. This inconsistant behaviour of EVAL type nodes is well established as
the following demonstrates:

 'words'=~/(word|word|word)(?{ print $1 })[xyz]/

which prints out 'word' three times, but

 'words'=~/(word|word|word)(?{ print $1 })S/

which doesnt print it out at all. This is due to other optimisations kicking in.

Example of what happens on a structural level:

The regexp /(ac|ad|ab)+/ will produce the folowing debug output:

   1: CURLYM[1] {1,32767}(18)
   5:   BRANCH(8)
   6:     EXACT <ac>(16)
   8:   BRANCH(11)
   9:     EXACT <ad>(16)
  11:   BRANCH(14)
  12:     EXACT <ab>(16)
  16:   SUCCEED(0)
  17:   NOTHING(18)
  18: END(0)

This would be optimizable with startbranch=5, first=5, last=16, tail=16
and should turn into:

   1: CURLYM[1] {1,32767}(18)
   5:   TRIE(16)
	[Words:3 Chars Stored:6 Unique Chars:4 States:5 NCP:1]
	  <ac>
	  <ad>
	  <ab>
  16:   SUCCEED(0)
  17:   NOTHING(18)
  18: END(0)

Cases where tail != last would be like /(?foo|bar)baz/:

   1: BRANCH(4)
   2:   EXACT <foo>(8)
   4: BRANCH(7)
   5:   EXACT <bar>(8)
   7: TAIL(8)
   8: EXACT <baz>(10)
  10: END(0)

which would be optimizable with startbranch=1, first=1, last=7, tail=8
and would end up looking like:

    1: TRIE(8)
      [Words:2 Chars Stored:6 Unique Chars:5 States:7 NCP:1]
	<foo>
	<bar>
   7: TAIL(8)
   8: EXACT <baz>(10)
  10: END(0)

    d = uvuni_to_utf8_flags(d, uv, 0);

is the recommended Unicode-aware way of saying

    *(d++) = uv;
*/

#define TRIE_STORE_REVCHAR                                                 \
    STMT_START {                                                           \
	if (UTF) {							   \
	    SV *zlopp = newSV(2);					   \
	    unsigned char *flrbbbbb = (unsigned char *) SvPVX(zlopp);	   \
	    unsigned const char *const kapow = uvuni_to_utf8(flrbbbbb, uvc & 0xFF); \
	    SvCUR_set(zlopp, kapow - flrbbbbb);				   \
	    SvPOK_on(zlopp);						   \
	    SvUTF8_on(zlopp);						   \
	    av_push(revcharmap, zlopp);					   \
	} else {							   \
	    char ooooff = (char)uvc;					   	   \
	    av_push(revcharmap, newSVpvn(&ooooff, 1));			   \
	}								   \
        } STMT_END

#define TRIE_READ_CHAR STMT_START {                                           \
    wordlen++;                                                                \
    if ( UTF ) {                                                              \
	if ( folder ) {                                                       \
	    if ( foldlen > 0 ) {                                              \
	       uvc = utf8n_to_uvuni( scan, UTF8_MAXLEN, &len, uniflags );     \
	       foldlen -= len;                                                \
	       scan += len;                                                   \
	       len = 0;                                                       \
	    } else {                                                          \
		uvc = utf8n_to_uvuni( (const U8*)uc, UTF8_MAXLEN, &len, uniflags);\
		uvc = to_uni_fold( uvc, foldbuf, &foldlen );                  \
		foldlen -= UNISKIP( uvc );                                    \
		scan = foldbuf + UNISKIP( uvc );                              \
	    }                                                                 \
	} else {                                                              \
	    uvc = utf8n_to_uvuni( (const U8*)uc, UTF8_MAXLEN, &len, uniflags);\
	}                                                                     \
    } else {                                                                  \
	uvc = (U32)*uc;                                                       \
	len = 1;                                                              \
    }                                                                         \
} STMT_END



#define TRIE_LIST_PUSH(state,fid,ns) STMT_START {               \
    if ( TRIE_LIST_CUR( state ) >=TRIE_LIST_LEN( state ) ) {    \
	U32 ging = TRIE_LIST_LEN( state ) *= 2;                 \
	Renew( trie->states[ state ].trans.list, ging, reg_trie_trans_le ); \
    }                                                           \
    TRIE_LIST_ITEM( state, TRIE_LIST_CUR( state ) ).forid = fid;     \
    TRIE_LIST_ITEM( state, TRIE_LIST_CUR( state ) ).newstate = ns;   \
    TRIE_LIST_CUR( state )++;                                   \
} STMT_END

#define TRIE_LIST_NEW(state) STMT_START {                       \
    Newxz( trie->states[ state ].trans.list,               \
	4, reg_trie_trans_le );                                 \
     TRIE_LIST_CUR( state ) = 1;                                \
     TRIE_LIST_LEN( state ) = 4;                                \
} STMT_END

#define TRIE_HANDLE_WORD(state) STMT_START {                    \
    U16 dupe= trie->states[ state ].wordnum;                    \
    regnode * const noper_next = regnext( noper );              \
                                                                \
    if (trie->wordlen)                                          \
        trie->wordlen[ curword ] = wordlen;                     \
    DEBUG_r({                                                   \
        /* store the word for dumping */                        \
        SV* tmp;                                                \
        if (OP(noper) != NOTHING)                               \
            tmp = newSVpvn_utf8(STRING(noper), STR_LEN(noper), UTF);	\
        else                                                    \
            tmp = newSVpvn_utf8( "", 0, UTF );			\
        av_push( trie_words, tmp );                             \
    });                                                         \
                                                                \
    curword++;                                                  \
                                                                \
    if ( noper_next < tail ) {                                  \
        if (!trie->jump)                                        \
            trie->jump = (U16 *) PerlMemShared_calloc( word_count + 1, sizeof(U16) ); \
        trie->jump[curword] = (U16)(noper_next - convert);      \
        if (!jumper)                                            \
            jumper = noper_next;                                \
        if (!nextbranch)                                        \
            nextbranch= regnext(cur);                           \
    }                                                           \
                                                                \
    if ( dupe ) {                                               \
        /* So it's a dupe. This means we need to maintain a   */\
        /* linked-list from the first to the next.            */\
        /* we only allocate the nextword buffer when there    */\
        /* a dupe, so first time we have to do the allocation */\
        if (!trie->nextword)                                    \
            trie->nextword = (U16 *)					\
		PerlMemShared_calloc( word_count + 1, sizeof(U16));	\
        while ( trie->nextword[dupe] )                          \
            dupe= trie->nextword[dupe];                         \
        trie->nextword[dupe]= curword;                          \
    } else {                                                    \
        /* we haven't inserted this word yet.                */ \
        trie->states[ state ].wordnum = curword;                \
    }                                                           \
} STMT_END


#define TRIE_TRANS_STATE(state,base,ucharcount,charid,special)		\
     ( ( base + charid >=  ucharcount					\
         && base + charid < ubound					\
         && state == trie->trans[ base - ucharcount + charid ].check	\
         && trie->trans[ base - ucharcount + charid ].next )		\
           ? trie->trans[ base - ucharcount + charid ].next		\
           : ( state==1 ? special : 0 )					\
      )

#define MADE_TRIE       1
#define MADE_JUMP_TRIE  2
#define MADE_EXACT_TRIE 4

STATIC I32
S_make_trie(pTHX_ RExC_state_t *pRExC_state, regnode *startbranch, regnode *first, regnode *last, regnode *tail, U32 word_count, U32 flags, U32 depth)
{
    dVAR;
    /* first pass, loop through and scan words */
    reg_trie_data *trie;
    HV *widecharmap = NULL;
    AV *revcharmap = newAV();
    regnode *cur;
    const U32 uniflags = UTF8_ALLOW_DEFAULT;
    STRLEN len = 0;
    UV uvc = 0;
    U16 curword = 0;
    U32 next_alloc = 0;
    regnode *jumper = NULL;
    regnode *nextbranch = NULL;
    regnode *convert = NULL;
    /* we just use folder as a flag in utf8 */
    const U8 * const folder = ( flags == EXACTF
                       ? PL_fold
                       : ( flags == EXACTFL
                           ? PL_fold_locale
                           : NULL
                         )
                     );

#ifdef DEBUGGING
    const U32 data_slot = add_data( pRExC_state, 4, "tuuu" );
    AV *trie_words = NULL;
    /* along with revcharmap, this only used during construction but both are
     * useful during debugging so we store them in the struct when debugging.
     */
#else
    const U32 data_slot = add_data( pRExC_state, 2, "tu" );
    STRLEN trie_charcount=0;
#endif
    SV *re_trie_maxbuff;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_MAKE_TRIE;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    trie = (reg_trie_data *) PerlMemShared_calloc( 1, sizeof(reg_trie_data) );
    trie->refcount = 1;
    trie->startstate = 1;
    trie->wordcount = word_count;
    RExC_rxi->data->data[ data_slot ] = (void*)trie;
    trie->charmap = (U16 *) PerlMemShared_calloc( 256, sizeof(U16) );
    if (!(UTF && folder))
	trie->bitmap = (char *) PerlMemShared_calloc( ANYOF_BITMAP_SIZE, 1 );
    DEBUG_r({
        trie_words = newAV();
    });

    re_trie_maxbuff = get_sv(RE_TRIE_MAXBUF_NAME, 1);
    if (!SvIOK(re_trie_maxbuff)) {
        sv_setiv(re_trie_maxbuff, RE_TRIE_MAXBUF_INIT);
    }
    DEBUG_OPTIMISE_r({
                PerlIO_printf( Perl_debug_log,
                  "%*smake_trie start==%d, first==%d, last==%d, tail==%d depth=%d\n",
                  (int)depth * 2 + 2, "", 
                  REG_NODE_NUM(startbranch),REG_NODE_NUM(first), 
                  REG_NODE_NUM(last), REG_NODE_NUM(tail),
                  (int)depth);
    });
   
   /* Find the node we are going to overwrite */
    if ( first == startbranch && OP( last ) != BRANCH ) {
        /* whole branch chain */
        convert = first;
    } else {
        /* branch sub-chain */
        convert = NEXTOPER( first );
    }
        
    /*  -- First loop and Setup --

       We first traverse the branches and scan each word to determine if it
       contains widechars, and how many unique chars there are, this is
       important as we have to build a table with at least as many columns as we
       have unique chars.

       We use an array of integers to represent the character codes 0..255
       (trie->charmap) and we use a an HV* to store Unicode characters. We use the
       native representation of the character value as the key and IV's for the
       coded index.

       *TODO* If we keep track of how many times each character is used we can
       remap the columns so that the table compression later on is more
       efficient in terms of memory by ensuring most common value is in the
       middle and the least common are on the outside.  IMO this would be better
       than a most to least common mapping as theres a decent chance the most
       common letter will share a node with the least common, meaning the node
       will not be compressable. With a middle is most common approach the worst
       case is when we have the least common nodes twice.

     */

    for ( cur = first ; cur < last ; cur = regnext( cur ) ) {
        regnode * const noper = NEXTOPER( cur );
        const U8 *uc = (U8*)STRING( noper );
        const U8 * const e  = uc + STR_LEN( noper );
        STRLEN foldlen = 0;
        U8 foldbuf[ UTF8_MAXBYTES_CASE + 1 ];
        const U8 *scan = (U8*)NULL;
        U32 wordlen      = 0;         /* required init */
        STRLEN chars = 0;
        bool set_bit = trie->bitmap ? 1 : 0; /*store the first char in the bitmap?*/

        if (OP(noper) == NOTHING) {
            trie->minlen= 0;
            continue;
        }
        if ( set_bit ) /* bitmap only alloced when !(UTF&&Folding) */
            TRIE_BITMAP_SET(trie,*uc); /* store the raw first byte
                                          regardless of encoding */

        for ( ; uc < e ; uc += len ) {
            TRIE_CHARCOUNT(trie)++;
            TRIE_READ_CHAR;
            chars++;
            if ( uvc < 256 ) {
                if ( !trie->charmap[ uvc ] ) {
                    trie->charmap[ uvc ]=( ++trie->uniquecharcount );
                    if ( folder )
                        trie->charmap[ folder[ uvc ] ] = trie->charmap[ uvc ];
                    TRIE_STORE_REVCHAR;
                }
                if ( set_bit ) {
                    /* store the codepoint in the bitmap, and if its ascii
                       also store its folded equivelent. */
                    TRIE_BITMAP_SET(trie,uvc);

		    /* store the folded codepoint */
		    if ( folder ) TRIE_BITMAP_SET(trie,folder[ uvc ]);

		    if ( !UTF ) {
			/* store first byte of utf8 representation of
			   codepoints in the 127 < uvc < 256 range */
			if (127 < uvc && uvc < 192) {
			    TRIE_BITMAP_SET(trie,194);
			} else if (191 < uvc ) {
			    TRIE_BITMAP_SET(trie,195);
			/* && uvc < 256 -- we know uvc is < 256 already */
			}
		    }
                    set_bit = 0; /* We've done our bit :-) */
                }
            } else {
                SV** svpp;
                if ( !widecharmap )
                    widecharmap = newHV();

                svpp = hv_fetch( widecharmap, (char*)&uvc, sizeof( UV ), 1 );

                if ( !svpp )
                    Perl_croak( aTHX_ "error creating/fetching widecharmap entry for 0x%"UVXf, uvc );

                if ( !SvTRUE( *svpp ) ) {
                    sv_setiv( *svpp, ++trie->uniquecharcount );
                    TRIE_STORE_REVCHAR;
                }
            }
        }
        if( cur == first ) {
            trie->minlen=chars;
            trie->maxlen=chars;
        } else if (chars < trie->minlen) {
            trie->minlen=chars;
        } else if (chars > trie->maxlen) {
            trie->maxlen=chars;
        }

    } /* end first pass */
    DEBUG_TRIE_COMPILE_r(
        PerlIO_printf( Perl_debug_log, "%*sTRIE(%s): W:%d C:%d Uq:%d Min:%d Max:%d\n",
                (int)depth * 2 + 2,"",
                ( widecharmap ? "UTF8" : "NATIVE" ), (int)word_count,
		(int)TRIE_CHARCOUNT(trie), trie->uniquecharcount,
		(int)trie->minlen, (int)trie->maxlen )
    );
    trie->wordlen = (U32 *) PerlMemShared_calloc( word_count, sizeof(U32) );

    /*
        We now know what we are dealing with in terms of unique chars and
        string sizes so we can calculate how much memory a naive
        representation using a flat table  will take. If it's over a reasonable
        limit (as specified by ${^RE_TRIE_MAXBUF}) we use a more memory
        conservative but potentially much slower representation using an array
        of lists.

        At the end we convert both representations into the same compressed
        form that will be used in regexec.c for matching with. The latter
        is a form that cannot be used to construct with but has memory
        properties similar to the list form and access properties similar
        to the table form making it both suitable for fast searches and
        small enough that its feasable to store for the duration of a program.

        See the comment in the code where the compressed table is produced
        inplace from the flat tabe representation for an explanation of how
        the compression works.

    */


    if ( (IV)( ( TRIE_CHARCOUNT(trie) + 1 ) * trie->uniquecharcount + 1) > SvIV(re_trie_maxbuff) ) {
        /*
            Second Pass -- Array Of Lists Representation

            Each state will be represented by a list of charid:state records
            (reg_trie_trans_le) the first such element holds the CUR and LEN
            points of the allocated array. (See defines above).

            We build the initial structure using the lists, and then convert
            it into the compressed table form which allows faster lookups
            (but cant be modified once converted).
        */

        STRLEN transcount = 1;

        DEBUG_TRIE_COMPILE_MORE_r( PerlIO_printf( Perl_debug_log, 
            "%*sCompiling trie using list compiler\n",
            (int)depth * 2 + 2, ""));
	
	trie->states = (reg_trie_state *)
	    PerlMemShared_calloc( TRIE_CHARCOUNT(trie) + 2,
				  sizeof(reg_trie_state) );
        TRIE_LIST_NEW(1);
        next_alloc = 2;

        for ( cur = first ; cur < last ; cur = regnext( cur ) ) {

	    regnode * const noper = NEXTOPER( cur );
	    U8 *uc           = (U8*)STRING( noper );
	    const U8 * const e = uc + STR_LEN( noper );
	    U32 state        = 1;         /* required init */
	    U16 charid       = 0;         /* sanity init */
	    U8 *scan         = (U8*)NULL; /* sanity init */
	    STRLEN foldlen   = 0;         /* required init */
            U32 wordlen      = 0;         /* required init */
	    U8 foldbuf[ UTF8_MAXBYTES_CASE + 1 ];

            if (OP(noper) != NOTHING) {
                for ( ; uc < e ; uc += len ) {

                    TRIE_READ_CHAR;

                    if ( uvc < 256 ) {
                        charid = trie->charmap[ uvc ];
		    } else {
                        SV** const svpp = hv_fetch( widecharmap, (char*)&uvc, sizeof( UV ), 0);
                        if ( !svpp ) {
                            charid = 0;
                        } else {
                            charid=(U16)SvIV( *svpp );
                        }
		    }
                    /* charid is now 0 if we dont know the char read, or nonzero if we do */
                    if ( charid ) {

                        U16 check;
                        U32 newstate = 0;

                        charid--;
                        if ( !trie->states[ state ].trans.list ) {
                            TRIE_LIST_NEW( state );
			}
                        for ( check = 1; check <= TRIE_LIST_USED( state ); check++ ) {
                            if ( TRIE_LIST_ITEM( state, check ).forid == charid ) {
                                newstate = TRIE_LIST_ITEM( state, check ).newstate;
                                break;
                            }
                        }
                        if ( ! newstate ) {
                            newstate = next_alloc++;
                            TRIE_LIST_PUSH( state, charid, newstate );
                            transcount++;
                        }
                        state = newstate;
                    } else {
                        Perl_croak( aTHX_ "panic! In trie construction, no char mapping for %"IVdf, uvc );
		    }
		}
	    }
            TRIE_HANDLE_WORD(state);

        } /* end second pass */

        /* next alloc is the NEXT state to be allocated */
        trie->statecount = next_alloc; 
        trie->states = (reg_trie_state *)
	    PerlMemShared_realloc( trie->states,
				   next_alloc
				   * sizeof(reg_trie_state) );

        /* and now dump it out before we compress it */
        DEBUG_TRIE_COMPILE_MORE_r(dump_trie_interim_list(trie, widecharmap,
							 revcharmap, next_alloc,
							 depth+1)
        );

        trie->trans = (reg_trie_trans *)
	    PerlMemShared_calloc( transcount, sizeof(reg_trie_trans) );
        {
            U32 state;
            U32 tp = 0;
            U32 zp = 0;


            for( state=1 ; state < next_alloc ; state ++ ) {
                U32 base=0;

                /*
                DEBUG_TRIE_COMPILE_MORE_r(
                    PerlIO_printf( Perl_debug_log, "tp: %d zp: %d ",tp,zp)
                );
                */

                if (trie->states[state].trans.list) {
                    U16 minid=TRIE_LIST_ITEM( state, 1).forid;
                    U16 maxid=minid;
		    U16 idx;

                    for( idx = 2 ; idx <= TRIE_LIST_USED( state ) ; idx++ ) {
			const U16 forid = TRIE_LIST_ITEM( state, idx).forid;
			if ( forid < minid ) {
			    minid=forid;
			} else if ( forid > maxid ) {
			    maxid=forid;
			}
                    }
                    if ( transcount < tp + maxid - minid + 1) {
                        transcount *= 2;
			trie->trans = (reg_trie_trans *)
			    PerlMemShared_realloc( trie->trans,
						     transcount
						     * sizeof(reg_trie_trans) );
                        Zero( trie->trans + (transcount / 2), transcount / 2 , reg_trie_trans );
                    }
                    base = trie->uniquecharcount + tp - minid;
                    if ( maxid == minid ) {
                        U32 set = 0;
                        for ( ; zp < tp ; zp++ ) {
                            if ( ! trie->trans[ zp ].next ) {
                                base = trie->uniquecharcount + zp - minid;
                                trie->trans[ zp ].next = TRIE_LIST_ITEM( state, 1).newstate;
                                trie->trans[ zp ].check = state;
                                set = 1;
                                break;
                            }
                        }
                        if ( !set ) {
                            trie->trans[ tp ].next = TRIE_LIST_ITEM( state, 1).newstate;
                            trie->trans[ tp ].check = state;
                            tp++;
                            zp = tp;
                        }
                    } else {
                        for ( idx=1; idx <= TRIE_LIST_USED( state ) ; idx++ ) {
                            const U32 tid = base -  trie->uniquecharcount + TRIE_LIST_ITEM( state, idx ).forid;
                            trie->trans[ tid ].next = TRIE_LIST_ITEM( state, idx ).newstate;
                            trie->trans[ tid ].check = state;
                        }
                        tp += ( maxid - minid + 1 );
                    }
                    Safefree(trie->states[ state ].trans.list);
                }
                /*
                DEBUG_TRIE_COMPILE_MORE_r(
                    PerlIO_printf( Perl_debug_log, " base: %d\n",base);
                );
                */
                trie->states[ state ].trans.base=base;
            }
            trie->lasttrans = tp + 1;
        }
    } else {
        /*
           Second Pass -- Flat Table Representation.

           we dont use the 0 slot of either trans[] or states[] so we add 1 to each.
           We know that we will need Charcount+1 trans at most to store the data
           (one row per char at worst case) So we preallocate both structures
           assuming worst case.

           We then construct the trie using only the .next slots of the entry
           structs.

           We use the .check field of the first entry of the node  temporarily to
           make compression both faster and easier by keeping track of how many non
           zero fields are in the node.

           Since trans are numbered from 1 any 0 pointer in the table is a FAIL
           transition.

           There are two terms at use here: state as a TRIE_NODEIDX() which is a
           number representing the first entry of the node, and state as a
           TRIE_NODENUM() which is the trans number. state 1 is TRIE_NODEIDX(1) and
           TRIE_NODENUM(1), state 2 is TRIE_NODEIDX(2) and TRIE_NODENUM(3) if there
           are 2 entrys per node. eg:

             A B       A B
          1. 2 4    1. 3 7
          2. 0 3    3. 0 5
          3. 0 0    5. 0 0
          4. 0 0    7. 0 0

           The table is internally in the right hand, idx form. However as we also
           have to deal with the states array which is indexed by nodenum we have to
           use TRIE_NODENUM() to convert.

        */
        DEBUG_TRIE_COMPILE_MORE_r( PerlIO_printf( Perl_debug_log, 
            "%*sCompiling trie using table compiler\n",
            (int)depth * 2 + 2, ""));

	trie->trans = (reg_trie_trans *)
	    PerlMemShared_calloc( ( TRIE_CHARCOUNT(trie) + 1 )
				  * trie->uniquecharcount + 1,
				  sizeof(reg_trie_trans) );
        trie->states = (reg_trie_state *)
	    PerlMemShared_calloc( TRIE_CHARCOUNT(trie) + 2,
				  sizeof(reg_trie_state) );
        next_alloc = trie->uniquecharcount + 1;


        for ( cur = first ; cur < last ; cur = regnext( cur ) ) {

	    regnode * const noper   = NEXTOPER( cur );
	    const U8 *uc     = (U8*)STRING( noper );
	    const U8 * const e = uc + STR_LEN( noper );

            U32 state        = 1;         /* required init */

            U16 charid       = 0;         /* sanity init */
            U32 accept_state = 0;         /* sanity init */
            U8 *scan         = (U8*)NULL; /* sanity init */

            STRLEN foldlen   = 0;         /* required init */
            U32 wordlen      = 0;         /* required init */
            U8 foldbuf[ UTF8_MAXBYTES_CASE + 1 ];

            if ( OP(noper) != NOTHING ) {
                for ( ; uc < e ; uc += len ) {

                    TRIE_READ_CHAR;

                    if ( uvc < 256 ) {
                        charid = trie->charmap[ uvc ];
                    } else {
                        SV* const * const svpp = hv_fetch( widecharmap, (char*)&uvc, sizeof( UV ), 0);
                        charid = svpp ? (U16)SvIV(*svpp) : 0;
                    }
                    if ( charid ) {
                        charid--;
                        if ( !trie->trans[ state + charid ].next ) {
                            trie->trans[ state + charid ].next = next_alloc;
                            trie->trans[ state ].check++;
                            next_alloc += trie->uniquecharcount;
                        }
                        state = trie->trans[ state + charid ].next;
                    } else {
                        Perl_croak( aTHX_ "panic! In trie construction, no char mapping for %"IVdf, uvc );
                    }
                    /* charid is now 0 if we dont know the char read, or nonzero if we do */
                }
            }
            accept_state = TRIE_NODENUM( state );
            TRIE_HANDLE_WORD(accept_state);

        } /* end second pass */

        /* and now dump it out before we compress it */
        DEBUG_TRIE_COMPILE_MORE_r(dump_trie_interim_table(trie, widecharmap,
							  revcharmap,
							  next_alloc, depth+1));

        {
        /*
           * Inplace compress the table.*

           For sparse data sets the table constructed by the trie algorithm will
           be mostly 0/FAIL transitions or to put it another way mostly empty.
           (Note that leaf nodes will not contain any transitions.)

           This algorithm compresses the tables by eliminating most such
           transitions, at the cost of a modest bit of extra work during lookup:

           - Each states[] entry contains a .base field which indicates the
           index in the state[] array wheres its transition data is stored.

           - If .base is 0 there are no  valid transitions from that node.

           - If .base is nonzero then charid is added to it to find an entry in
           the trans array.

           -If trans[states[state].base+charid].check!=state then the
           transition is taken to be a 0/Fail transition. Thus if there are fail
           transitions at the front of the node then the .base offset will point
           somewhere inside the previous nodes data (or maybe even into a node
           even earlier), but the .check field determines if the transition is
           valid.

           XXX - wrong maybe?
           The following process inplace converts the table to the compressed
           table: We first do not compress the root node 1,and mark its all its
           .check pointers as 1 and set its .base pointer as 1 as well. This
           allows to do a DFA construction from the compressed table later, and
           ensures that any .base pointers we calculate later are greater than
           0.

           - We set 'pos' to indicate the first entry of the second node.

           - We then iterate over the columns of the node, finding the first and
           last used entry at l and m. We then copy l..m into pos..(pos+m-l),
           and set the .check pointers accordingly, and advance pos
           appropriately and repreat for the next node. Note that when we copy
           the next pointers we have to convert them from the original
           NODEIDX form to NODENUM form as the former is not valid post
           compression.

           - If a node has no transitions used we mark its base as 0 and do not
           advance the pos pointer.

           - If a node only has one transition we use a second pointer into the
           structure to fill in allocated fail transitions from other states.
           This pointer is independent of the main pointer and scans forward
           looking for null transitions that are allocated to a state. When it
           finds one it writes the single transition into the "hole".  If the
           pointer doesnt find one the single transition is appended as normal.

           - Once compressed we can Renew/realloc the structures to release the
           excess space.

           See "Table-Compression Methods" in sec 3.9 of the Red Dragon,
           specifically Fig 3.47 and the associated pseudocode.

           demq
        */
        const U32 laststate = TRIE_NODENUM( next_alloc );
	U32 state, charid;
        U32 pos = 0, zp=0;
        trie->statecount = laststate;

        for ( state = 1 ; state < laststate ; state++ ) {
            U8 flag = 0;
	    const U32 stateidx = TRIE_NODEIDX( state );
	    const U32 o_used = trie->trans[ stateidx ].check;
	    U32 used = trie->trans[ stateidx ].check;
            trie->trans[ stateidx ].check = 0;

            for ( charid = 0 ; used && charid < trie->uniquecharcount ; charid++ ) {
                if ( flag || trie->trans[ stateidx + charid ].next ) {
                    if ( trie->trans[ stateidx + charid ].next ) {
                        if (o_used == 1) {
                            for ( ; zp < pos ; zp++ ) {
                                if ( ! trie->trans[ zp ].next ) {
                                    break;
                                }
                            }
                            trie->states[ state ].trans.base = zp + trie->uniquecharcount - charid ;
                            trie->trans[ zp ].next = SAFE_TRIE_NODENUM( trie->trans[ stateidx + charid ].next );
                            trie->trans[ zp ].check = state;
                            if ( ++zp > pos ) pos = zp;
                            break;
                        }
                        used--;
                    }
                    if ( !flag ) {
                        flag = 1;
                        trie->states[ state ].trans.base = pos + trie->uniquecharcount - charid ;
                    }
                    trie->trans[ pos ].next = SAFE_TRIE_NODENUM( trie->trans[ stateidx + charid ].next );
                    trie->trans[ pos ].check = state;
                    pos++;
                }
            }
        }
        trie->lasttrans = pos + 1;
        trie->states = (reg_trie_state *)
	    PerlMemShared_realloc( trie->states, laststate
				   * sizeof(reg_trie_state) );
        DEBUG_TRIE_COMPILE_MORE_r(
                PerlIO_printf( Perl_debug_log,
		    "%*sAlloc: %d Orig: %"IVdf" elements, Final:%"IVdf". Savings of %%%5.2f\n",
		    (int)depth * 2 + 2,"",
                    (int)( ( TRIE_CHARCOUNT(trie) + 1 ) * trie->uniquecharcount + 1 ),
		    (IV)next_alloc,
		    (IV)pos,
                    ( ( next_alloc - pos ) * 100 ) / (double)next_alloc );
            );

        } /* end table compress */
    }
    DEBUG_TRIE_COMPILE_MORE_r(
            PerlIO_printf(Perl_debug_log, "%*sStatecount:%"UVxf" Lasttrans:%"UVxf"\n",
                (int)depth * 2 + 2, "",
                (UV)trie->statecount,
                (UV)trie->lasttrans)
    );
    /* resize the trans array to remove unused space */
    trie->trans = (reg_trie_trans *)
	PerlMemShared_realloc( trie->trans, trie->lasttrans
			       * sizeof(reg_trie_trans) );

    /* and now dump out the compressed format */
    DEBUG_TRIE_COMPILE_r(dump_trie(trie, widecharmap, revcharmap, depth+1));

    {   /* Modify the program and insert the new TRIE node*/ 
        U8 nodetype =(U8)(flags & 0xFF);
        char *str=NULL;
        
#ifdef DEBUGGING
        regnode *optimize = NULL;
#ifdef RE_TRACK_PATTERN_OFFSETS

        U32 mjd_offset = 0;
        U32 mjd_nodelen = 0;
#endif /* RE_TRACK_PATTERN_OFFSETS */
#endif /* DEBUGGING */
        /*
           This means we convert either the first branch or the first Exact,
           depending on whether the thing following (in 'last') is a branch
           or not and whther first is the startbranch (ie is it a sub part of
           the alternation or is it the whole thing.)
           Assuming its a sub part we conver the EXACT otherwise we convert
           the whole branch sequence, including the first.
         */
        /* Find the node we are going to overwrite */
        if ( first != startbranch || OP( last ) == BRANCH ) {
            /* branch sub-chain */
            NEXT_OFF( first ) = (U16)(last - first);
#ifdef RE_TRACK_PATTERN_OFFSETS
            DEBUG_r({
                mjd_offset= Node_Offset((convert));
                mjd_nodelen= Node_Length((convert));
            });
#endif
            /* whole branch chain */
        }
#ifdef RE_TRACK_PATTERN_OFFSETS
        else {
            DEBUG_r({
                const  regnode *nop = NEXTOPER( convert );
                mjd_offset= Node_Offset((nop));
                mjd_nodelen= Node_Length((nop));
            });
        }
        DEBUG_OPTIMISE_r(
            PerlIO_printf(Perl_debug_log, "%*sMJD offset:%"UVuf" MJD length:%"UVuf"\n",
                (int)depth * 2 + 2, "",
                (UV)mjd_offset, (UV)mjd_nodelen)
        );
#endif
        /* But first we check to see if there is a common prefix we can 
           split out as an EXACT and put in front of the TRIE node.  */
        trie->startstate= 1;
        if ( trie->bitmap && !widecharmap && !trie->jump  ) {
            U32 state;
            for ( state = 1 ; state < trie->statecount-1 ; state++ ) {
                U32 ofs = 0;
                I32 idx = -1;
                U32 count = 0;
                const U32 base = trie->states[ state ].trans.base;

                if ( trie->states[state].wordnum )
                        count = 1;

                for ( ofs = 0 ; ofs < trie->uniquecharcount ; ofs++ ) {
                    if ( ( base + ofs >= trie->uniquecharcount ) &&
                         ( base + ofs - trie->uniquecharcount < trie->lasttrans ) &&
                         trie->trans[ base + ofs - trie->uniquecharcount ].check == state )
                    {
                        if ( ++count > 1 ) {
                            SV **tmp = av_fetch( revcharmap, ofs, 0);
			    const U8 *ch = (U8*)SvPV_nolen_const( *tmp );
                            if ( state == 1 ) break;
                            if ( count == 2 ) {
                                Zero(trie->bitmap, ANYOF_BITMAP_SIZE, char);
                                DEBUG_OPTIMISE_r(
                                    PerlIO_printf(Perl_debug_log,
					"%*sNew Start State=%"UVuf" Class: [",
                                        (int)depth * 2 + 2, "",
                                        (UV)state));
				if (idx >= 0) {
				    SV ** const tmp = av_fetch( revcharmap, idx, 0);
				    const U8 * const ch = (U8*)SvPV_nolen_const( *tmp );

                                    TRIE_BITMAP_SET(trie,*ch);
                                    if ( folder )
                                        TRIE_BITMAP_SET(trie, folder[ *ch ]);
                                    DEBUG_OPTIMISE_r(
                                        PerlIO_printf(Perl_debug_log, "%s", (char*)ch)
                                    );
				}
			    }
			    TRIE_BITMAP_SET(trie,*ch);
			    if ( folder )
				TRIE_BITMAP_SET(trie,folder[ *ch ]);
			    DEBUG_OPTIMISE_r(PerlIO_printf( Perl_debug_log,"%s", ch));
			}
                        idx = ofs;
		    }
                }
                if ( count == 1 ) {
                    SV **tmp = av_fetch( revcharmap, idx, 0);
                    STRLEN len;
                    char *ch = SvPV( *tmp, len );
                    DEBUG_OPTIMISE_r({
                        SV *sv=sv_newmortal();
                        PerlIO_printf( Perl_debug_log,
			    "%*sPrefix State: %"UVuf" Idx:%"UVuf" Char='%s'\n",
                            (int)depth * 2 + 2, "",
                            (UV)state, (UV)idx, 
                            pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), 6, 
	                        PL_colors[0], PL_colors[1],
	                        (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0) |
	                        PERL_PV_ESCAPE_FIRSTCHAR 
                            )
                        );
                    });
                    if ( state==1 ) {
                        OP( convert ) = nodetype;
                        str=STRING(convert);
                        STR_LEN(convert)=0;
                    }
                    STR_LEN(convert) += len;
                    while (len--)
                        *str++ = *ch++;
		} else {
#ifdef DEBUGGING	    
		    if (state>1)
			DEBUG_OPTIMISE_r(PerlIO_printf( Perl_debug_log,"]\n"));
#endif
		    break;
		}
	    }
            if (str) {
                regnode *n = convert+NODE_SZ_STR(convert);
                NEXT_OFF(convert) = NODE_SZ_STR(convert);
                trie->startstate = state;
                trie->minlen -= (state - 1);
                trie->maxlen -= (state - 1);
#ifdef DEBUGGING
               /* At least the UNICOS C compiler choked on this
                * being argument to DEBUG_r(), so let's just have
                * it right here. */
               if (
#ifdef PERL_EXT_RE_BUILD
                   1
#else
                   DEBUG_r_TEST
#endif
                   ) {
                   regnode *fix = convert;
                   U32 word = trie->wordcount;
                   mjd_nodelen++;
                   Set_Node_Offset_Length(convert, mjd_offset, state - 1);
                   while( ++fix < n ) {
                       Set_Node_Offset_Length(fix, 0, 0);
                   }
                   while (word--) {
                       SV ** const tmp = av_fetch( trie_words, word, 0 );
                       if (tmp) {
                           if ( STR_LEN(convert) <= SvCUR(*tmp) )
                               sv_chop(*tmp, SvPV_nolen(*tmp) + STR_LEN(convert));
                           else
                               sv_chop(*tmp, SvPV_nolen(*tmp) + SvCUR(*tmp));
                       }
                   }
               }
#endif
                if (trie->maxlen) {
                    convert = n;
		} else {
                    NEXT_OFF(convert) = (U16)(tail - convert);
                    DEBUG_r(optimize= n);
                }
            }
        }
        if (!jumper) 
            jumper = last; 
        if ( trie->maxlen ) {
	    NEXT_OFF( convert ) = (U16)(tail - convert);
	    ARG_SET( convert, data_slot );
	    /* Store the offset to the first unabsorbed branch in 
	       jump[0], which is otherwise unused by the jump logic. 
	       We use this when dumping a trie and during optimisation. */
	    if (trie->jump) 
	        trie->jump[0] = (U16)(nextbranch - convert);
            
            /* XXXX */
            if ( !trie->states[trie->startstate].wordnum && trie->bitmap && 
                 ( (char *)jumper - (char *)convert) >= (int)sizeof(struct regnode_charclass) )
            {
                OP( convert ) = TRIEC;
                Copy(trie->bitmap, ((struct regnode_charclass *)convert)->bitmap, ANYOF_BITMAP_SIZE, char);
                PerlMemShared_free(trie->bitmap);
                trie->bitmap= NULL;
            } else 
                OP( convert ) = TRIE;

            /* store the type in the flags */
            convert->flags = nodetype;
            DEBUG_r({
            optimize = convert 
                      + NODE_STEP_REGNODE 
                      + regarglen[ OP( convert ) ];
            });
            /* XXX We really should free up the resource in trie now, 
                   as we won't use them - (which resources?) dmq */
        }
        /* needed for dumping*/
        DEBUG_r(if (optimize) {
            regnode *opt = convert;

            while ( ++opt < optimize) {
                Set_Node_Offset_Length(opt,0,0);
            }
            /* 
                Try to clean up some of the debris left after the 
                optimisation.
             */
            while( optimize < jumper ) {
                mjd_nodelen += Node_Length((optimize));
                OP( optimize ) = OPTIMIZED;
                Set_Node_Offset_Length(optimize,0,0);
                optimize++;
            }
            Set_Node_Offset_Length(convert,mjd_offset,mjd_nodelen);
        });
    } /* end node insert */
    RExC_rxi->data->data[ data_slot + 1 ] = (void*)widecharmap;
#ifdef DEBUGGING
    RExC_rxi->data->data[ data_slot + TRIE_WORDS_OFFSET ] = (void*)trie_words;
    RExC_rxi->data->data[ data_slot + 3 ] = (void*)revcharmap;
#else
    SvREFCNT_dec(revcharmap);
#endif
    return trie->jump 
           ? MADE_JUMP_TRIE 
           : trie->startstate>1 
             ? MADE_EXACT_TRIE 
             : MADE_TRIE;
}

STATIC void
S_make_trie_failtable(pTHX_ RExC_state_t *pRExC_state, regnode *source,  regnode *stclass, U32 depth)
{
/* The Trie is constructed and compressed now so we can build a fail array now if its needed

   This is basically the Aho-Corasick algorithm. Its from exercise 3.31 and 3.32 in the
   "Red Dragon" -- Compilers, principles, techniques, and tools. Aho, Sethi, Ullman 1985/88
   ISBN 0-201-10088-6

   We find the fail state for each state in the trie, this state is the longest proper
   suffix of the current states 'word' that is also a proper prefix of another word in our
   trie. State 1 represents the word '' and is the thus the default fail state. This allows
   the DFA not to have to restart after its tried and failed a word at a given point, it
   simply continues as though it had been matching the other word in the first place.
   Consider
      'abcdgu'=~/abcdefg|cdgu/
   When we get to 'd' we are still matching the first word, we would encounter 'g' which would
   fail, which would bring use to the state representing 'd' in the second word where we would
   try 'g' and succeed, prodceding to match 'cdgu'.
 */
 /* add a fail transition */
    const U32 trie_offset = ARG(source);
    reg_trie_data *trie=(reg_trie_data *)RExC_rxi->data->data[trie_offset];
    U32 *q;
    const U32 ucharcount = trie->uniquecharcount;
    const U32 numstates = trie->statecount;
    const U32 ubound = trie->lasttrans + ucharcount;
    U32 q_read = 0;
    U32 q_write = 0;
    U32 charid;
    U32 base = trie->states[ 1 ].trans.base;
    U32 *fail;
    reg_ac_data *aho;
    const U32 data_slot = add_data( pRExC_state, 1, "T" );
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_MAKE_TRIE_FAILTABLE;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif


    ARG_SET( stclass, data_slot );
    aho = (reg_ac_data *) PerlMemShared_calloc( 1, sizeof(reg_ac_data) );
    RExC_rxi->data->data[ data_slot ] = (void*)aho;
    aho->trie=trie_offset;
    aho->states=(reg_trie_state *)PerlMemShared_malloc( numstates * sizeof(reg_trie_state) );
    Copy( trie->states, aho->states, numstates, reg_trie_state );
    Newxz( q, numstates, U32);
    aho->fail = (U32 *) PerlMemShared_calloc( numstates, sizeof(U32) );
    aho->refcount = 1;
    fail = aho->fail;
    /* initialize fail[0..1] to be 1 so that we always have
       a valid final fail state */
    fail[ 0 ] = fail[ 1 ] = 1;

    for ( charid = 0; charid < ucharcount ; charid++ ) {
	const U32 newstate = TRIE_TRANS_STATE( 1, base, ucharcount, charid, 0 );
	if ( newstate ) {
            q[ q_write ] = newstate;
            /* set to point at the root */
            fail[ q[ q_write++ ] ]=1;
        }
    }
    while ( q_read < q_write) {
	const U32 cur = q[ q_read++ % numstates ];
        base = trie->states[ cur ].trans.base;

        for ( charid = 0 ; charid < ucharcount ; charid++ ) {
	    const U32 ch_state = TRIE_TRANS_STATE( cur, base, ucharcount, charid, 1 );
	    if (ch_state) {
                U32 fail_state = cur;
                U32 fail_base;
                do {
                    fail_state = fail[ fail_state ];
                    fail_base = aho->states[ fail_state ].trans.base;
                } while ( !TRIE_TRANS_STATE( fail_state, fail_base, ucharcount, charid, 1 ) );

                fail_state = TRIE_TRANS_STATE( fail_state, fail_base, ucharcount, charid, 1 );
                fail[ ch_state ] = fail_state;
                if ( !aho->states[ ch_state ].wordnum && aho->states[ fail_state ].wordnum )
                {
                        aho->states[ ch_state ].wordnum =  aho->states[ fail_state ].wordnum;
                }
                q[ q_write++ % numstates] = ch_state;
            }
        }
    }
    /* restore fail[0..1] to 0 so that we "fall out" of the AC loop
       when we fail in state 1, this allows us to use the
       charclass scan to find a valid start char. This is based on the principle
       that theres a good chance the string being searched contains lots of stuff
       that cant be a start char.
     */
    fail[ 0 ] = fail[ 1 ] = 0;
    DEBUG_TRIE_COMPILE_r({
        PerlIO_printf(Perl_debug_log,
		      "%*sStclass Failtable (%"UVuf" states): 0", 
		      (int)(depth * 2), "", (UV)numstates
        );
        for( q_read=1; q_read<numstates; q_read++ ) {
            PerlIO_printf(Perl_debug_log, ", %"UVuf, (UV)fail[q_read]);
        }
        PerlIO_printf(Perl_debug_log, "\n");
    });
    Safefree(q);
    /*RExC_seen |= REG_SEEN_TRIEDFA;*/
}


/*
 * There are strange code-generation bugs caused on sparc64 by gcc-2.95.2.
 * These need to be revisited when a newer toolchain becomes available.
 */
#if defined(__sparc64__) && defined(__GNUC__)
#   if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#       undef  SPARC64_GCC_WORKAROUND
#       define SPARC64_GCC_WORKAROUND 1
#   endif
#endif

#define DEBUG_PEEP(str,scan,depth) \
    DEBUG_OPTIMISE_r({if (scan){ \
       SV * const mysv=sv_newmortal(); \
       regnode *Next = regnext(scan); \
       regprop(RExC_rx, mysv, scan); \
       PerlIO_printf(Perl_debug_log, "%*s" str ">%3d: %s (%d)\n", \
       (int)depth*2, "", REG_NODE_NUM(scan), SvPV_nolen_const(mysv),\
       Next ? (REG_NODE_NUM(Next)) : 0 ); \
   }});





#define JOIN_EXACT(scan,min,flags) \
    if (PL_regkind[OP(scan)] == EXACT) \
        join_exact(pRExC_state,(scan),(min),(flags),NULL,depth+1)

STATIC U32
S_join_exact(pTHX_ RExC_state_t *pRExC_state, regnode *scan, I32 *min, U32 flags,regnode *val, U32 depth) {
    /* Merge several consecutive EXACTish nodes into one. */
    regnode *n = regnext(scan);
    U32 stringok = 1;
    regnode *next = scan + NODE_SZ_STR(scan);
    U32 merged = 0;
    U32 stopnow = 0;
#ifdef DEBUGGING
    regnode *stop = scan;
    GET_RE_DEBUG_FLAGS_DECL;
#else
    PERL_UNUSED_ARG(depth);
#endif

    PERL_ARGS_ASSERT_JOIN_EXACT;
#ifndef EXPERIMENTAL_INPLACESCAN
    PERL_UNUSED_ARG(flags);
    PERL_UNUSED_ARG(val);
#endif
    DEBUG_PEEP("join",scan,depth);
    
    /* Skip NOTHING, merge EXACT*. */
    while (n &&
           ( PL_regkind[OP(n)] == NOTHING ||
             (stringok && (OP(n) == OP(scan))))
           && NEXT_OFF(n)
           && NEXT_OFF(scan) + NEXT_OFF(n) < I16_MAX) {
        
        if (OP(n) == TAIL || n > next)
            stringok = 0;
        if (PL_regkind[OP(n)] == NOTHING) {
            DEBUG_PEEP("skip:",n,depth);
            NEXT_OFF(scan) += NEXT_OFF(n);
            next = n + NODE_STEP_REGNODE;
#ifdef DEBUGGING
            if (stringok)
                stop = n;
#endif
            n = regnext(n);
        }
        else if (stringok) {
            const unsigned int oldl = STR_LEN(scan);
            regnode * const nnext = regnext(n);
            
            DEBUG_PEEP("merg",n,depth);
            
            merged++;
            if (oldl + STR_LEN(n) > U8_MAX)
                break;
            NEXT_OFF(scan) += NEXT_OFF(n);
            STR_LEN(scan) += STR_LEN(n);
            next = n + NODE_SZ_STR(n);
            /* Now we can overwrite *n : */
            Move(STRING(n), STRING(scan) + oldl, STR_LEN(n), char);
#ifdef DEBUGGING
            stop = next - 1;
#endif
            n = nnext;
            if (stopnow) break;
        }

#ifdef EXPERIMENTAL_INPLACESCAN
	if (flags && !NEXT_OFF(n)) {
	    DEBUG_PEEP("atch", val, depth);
	    if (reg_off_by_arg[OP(n)]) {
		ARG_SET(n, val - n);
	    }
	    else {
		NEXT_OFF(n) = val - n;
	    }
	    stopnow = 1;
	}
#endif
    }
    
    if (UTF && ( OP(scan) == EXACTF ) && ( STR_LEN(scan) >= 6 ) ) {
    /*
    Two problematic code points in Unicode casefolding of EXACT nodes:
    
    U+0390 - GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS
    U+03B0 - GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS
    
    which casefold to
    
    Unicode                      UTF-8
    
    U+03B9 U+0308 U+0301         0xCE 0xB9 0xCC 0x88 0xCC 0x81
    U+03C5 U+0308 U+0301         0xCF 0x85 0xCC 0x88 0xCC 0x81
    
    This means that in case-insensitive matching (or "loose matching",
    as Unicode calls it), an EXACTF of length six (the UTF-8 encoded byte
    length of the above casefolded versions) can match a target string
    of length two (the byte length of UTF-8 encoded U+0390 or U+03B0).
    This would rather mess up the minimum length computation.
    
    What we'll do is to look for the tail four bytes, and then peek
    at the preceding two bytes to see whether we need to decrease
    the minimum length by four (six minus two).
    
    Thanks to the design of UTF-8, there cannot be false matches:
    A sequence of valid UTF-8 bytes cannot be a subsequence of
    another valid sequence of UTF-8 bytes.
    
    */
         char * const s0 = STRING(scan), *s, *t;
         char * const s1 = s0 + STR_LEN(scan) - 1;
         char * const s2 = s1 - 4;
#ifdef EBCDIC /* RD tunifold greek 0390 and 03B0 */
	 const char t0[] = "\xaf\x49\xaf\x42";
#else
         const char t0[] = "\xcc\x88\xcc\x81";
#endif
         const char * const t1 = t0 + 3;
    
         for (s = s0 + 2;
              s < s2 && (t = ninstr(s, s1, t0, t1));
              s = t + 4) {
#ifdef EBCDIC
	      if (((U8)t[-1] == 0x68 && (U8)t[-2] == 0xB4) ||
		  ((U8)t[-1] == 0x46 && (U8)t[-2] == 0xB5))
#else
              if (((U8)t[-1] == 0xB9 && (U8)t[-2] == 0xCE) ||
                  ((U8)t[-1] == 0x85 && (U8)t[-2] == 0xCF))
#endif
                   *min -= 4;
         }
    }
    
#ifdef DEBUGGING
    /* Allow dumping */
    n = scan + NODE_SZ_STR(scan);
    while (n <= stop) {
        if (PL_regkind[OP(n)] != NOTHING || OP(n) == NOTHING) {
            OP(n) = OPTIMIZED;
            NEXT_OFF(n) = 0;
        }
        n++;
    }
#endif
    DEBUG_OPTIMISE_r(if (merged){DEBUG_PEEP("finl",scan,depth)});
    return stopnow;
}

/* REx optimizer.  Converts nodes into quickier variants "in place".
   Finds fixed substrings.  */

/* Stops at toplevel WHILEM as well as at "last". At end *scanp is set
   to the position after last scanned or to NULL. */

#define INIT_AND_WITHP \
    assert(!and_withp); \
    Newx(and_withp,1,struct regnode_charclass_class); \
    SAVEFREEPV(and_withp)

/* this is a chain of data about sub patterns we are processing that
   need to be handled seperately/specially in study_chunk. Its so
   we can simulate recursion without losing state.  */
struct scan_frame;
typedef struct scan_frame {
    regnode *last;  /* last node to process in this frame */
    regnode *next;  /* next node to process when last is reached */
    struct scan_frame *prev; /*previous frame*/
    I32 stop; /* what stopparen do we use */
} scan_frame;


#define SCAN_COMMIT(s, data, m) scan_commit(s, data, m, is_inf)

#define CASE_SYNST_FNC(nAmE)                                       \
case nAmE:                                                         \
    if (flags & SCF_DO_STCLASS_AND) {                              \
	    for (value = 0; value < 256; value++)                  \
		if (!is_ ## nAmE ## _cp(value))                       \
		    ANYOF_BITMAP_CLEAR(data->start_class, value);  \
    }                                                              \
    else {                                                         \
	    for (value = 0; value < 256; value++)                  \
		if (is_ ## nAmE ## _cp(value))                        \
		    ANYOF_BITMAP_SET(data->start_class, value);	   \
    }                                                              \
    break;                                                         \
case N ## nAmE:                                                    \
    if (flags & SCF_DO_STCLASS_AND) {                              \
	    for (value = 0; value < 256; value++)                   \
		if (is_ ## nAmE ## _cp(value))                         \
		    ANYOF_BITMAP_CLEAR(data->start_class, value);   \
    }                                                               \
    else {                                                          \
	    for (value = 0; value < 256; value++)                   \
		if (!is_ ## nAmE ## _cp(value))                        \
		    ANYOF_BITMAP_SET(data->start_class, value);	    \
    }                                                               \
    break



STATIC I32
S_study_chunk(pTHX_ RExC_state_t *pRExC_state, regnode **scanp,
                        I32 *minlenp, I32 *deltap,
			regnode *last,
			scan_data_t *data,
			I32 stopparen,
			U8* recursed,
			struct regnode_charclass_class *and_withp,
			U32 flags, U32 depth)
			/* scanp: Start here (read-write). */
			/* deltap: Write maxlen-minlen here. */
			/* last: Stop before this one. */
			/* data: string data about the pattern */
			/* stopparen: treat close N as END */
			/* recursed: which subroutines have we recursed into */
			/* and_withp: Valid if flags & SCF_DO_STCLASS_OR */
{
    dVAR;
    I32 min = 0, pars = 0, code;
    regnode *scan = *scanp, *next;
    I32 delta = 0;
    int is_inf = (flags & SCF_DO_SUBSTR) && (data->flags & SF_IS_INF);
    int is_inf_internal = 0;		/* The studied chunk is infinite */
    I32 is_par = OP(scan) == OPEN ? ARG(scan) : 0;
    scan_data_t data_fake;
    SV *re_trie_maxbuff = NULL;
    regnode *first_non_open = scan;
    I32 stopmin = I32_MAX;
    scan_frame *frame = NULL;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_STUDY_CHUNK;

#ifdef DEBUGGING
    StructCopy(&zero_scan_data, &data_fake, scan_data_t);
#endif

    if ( depth == 0 ) {
        while (first_non_open && OP(first_non_open) == OPEN)
            first_non_open=regnext(first_non_open);
    }


  fake_study_recurse:
    while ( scan && OP(scan) != END && scan < last ){
	/* Peephole optimizer: */
	DEBUG_STUDYDATA("Peep:", data,depth);
	DEBUG_PEEP("Peep",scan,depth);
        JOIN_EXACT(scan,&min,0);

	/* Follow the next-chain of the current node and optimize
	   away all the NOTHINGs from it.  */
	if (OP(scan) != CURLYX) {
	    const int max = (reg_off_by_arg[OP(scan)]
		       ? I32_MAX
		       /* I32 may be smaller than U16 on CRAYs! */
		       : (I32_MAX < U16_MAX ? I32_MAX : U16_MAX));
	    int off = (reg_off_by_arg[OP(scan)] ? ARG(scan) : NEXT_OFF(scan));
	    int noff;
	    regnode *n = scan;
	
	    /* Skip NOTHING and LONGJMP. */
	    while ((n = regnext(n))
		   && ((PL_regkind[OP(n)] == NOTHING && (noff = NEXT_OFF(n)))
		       || ((OP(n) == LONGJMP) && (noff = ARG(n))))
		   && off + noff < max)
		off += noff;
	    if (reg_off_by_arg[OP(scan)])
		ARG(scan) = off;
	    else
		NEXT_OFF(scan) = off;
	}



	/* The principal pseudo-switch.  Cannot be a switch, since we
	   look into several different things.  */
	if (OP(scan) == BRANCH || OP(scan) == BRANCHJ
		   || OP(scan) == IFTHEN) {
	    next = regnext(scan);
	    code = OP(scan);
	    /* demq: the op(next)==code check is to see if we have "branch-branch" AFAICT */
	
	    if (OP(next) == code || code == IFTHEN) {
	        /* NOTE - There is similar code to this block below for handling
	           TRIE nodes on a re-study.  If you change stuff here check there
	           too. */
		I32 max1 = 0, min1 = I32_MAX, num = 0;
		struct regnode_charclass_class accum;
		regnode * const startbranch=scan;
		
		if (flags & SCF_DO_SUBSTR)
		    SCAN_COMMIT(pRExC_state, data, minlenp); /* Cannot merge strings after this. */
		if (flags & SCF_DO_STCLASS)
		    cl_init_zero(pRExC_state, &accum);

		while (OP(scan) == code) {
		    I32 deltanext, minnext, f = 0, fake;
		    struct regnode_charclass_class this_class;

		    num++;
		    data_fake.flags = 0;
		    if (data) {
			data_fake.whilem_c = data->whilem_c;
			data_fake.last_closep = data->last_closep;
		    }
		    else
			data_fake.last_closep = &fake;

		    data_fake.pos_delta = delta;
		    next = regnext(scan);
		    scan = NEXTOPER(scan);
		    if (code != BRANCH)
			scan = NEXTOPER(scan);
		    if (flags & SCF_DO_STCLASS) {
			cl_init(pRExC_state, &this_class);
			data_fake.start_class = &this_class;
			f = SCF_DO_STCLASS_AND;
		    }
		    if (flags & SCF_WHILEM_VISITED_POS)
			f |= SCF_WHILEM_VISITED_POS;

		    /* we suppose the run is continuous, last=next...*/
		    minnext = study_chunk(pRExC_state, &scan, minlenp, &deltanext,
					  next, &data_fake,
					  stopparen, recursed, NULL, f,depth+1);
		    if (min1 > minnext)
			min1 = minnext;
		    if (max1 < minnext + deltanext)
			max1 = minnext + deltanext;
		    if (deltanext == I32_MAX)
			is_inf = is_inf_internal = 1;
		    scan = next;
		    if (data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
			pars++;
	            if (data_fake.flags & SCF_SEEN_ACCEPT) {
	                if ( stopmin > minnext) 
	                    stopmin = min + min1;
	                flags &= ~SCF_DO_SUBSTR;
	                if (data)
	                    data->flags |= SCF_SEEN_ACCEPT;
	            }
		    if (data) {
			if (data_fake.flags & SF_HAS_EVAL)
			    data->flags |= SF_HAS_EVAL;
			data->whilem_c = data_fake.whilem_c;
		    }
		    if (flags & SCF_DO_STCLASS)
			cl_or(pRExC_state, &accum, &this_class);
		}
		if (code == IFTHEN && num < 2) /* Empty ELSE branch */
		    min1 = 0;
		if (flags & SCF_DO_SUBSTR) {
		    data->pos_min += min1;
		    data->pos_delta += max1 - min1;
		    if (max1 != min1 || is_inf)
			data->longest = &(data->longest_float);
		}
		min += min1;
		delta += max1 - min1;
		if (flags & SCF_DO_STCLASS_OR) {
		    cl_or(pRExC_state, data->start_class, &accum);
		    if (min1) {
			cl_and(data->start_class, and_withp);
			flags &= ~SCF_DO_STCLASS;
		    }
		}
		else if (flags & SCF_DO_STCLASS_AND) {
		    if (min1) {
			cl_and(data->start_class, &accum);
			flags &= ~SCF_DO_STCLASS;
		    }
		    else {
			/* Switch to OR mode: cache the old value of
			 * data->start_class */
			INIT_AND_WITHP;
			StructCopy(data->start_class, and_withp,
				   struct regnode_charclass_class);
			flags &= ~SCF_DO_STCLASS_AND;
			StructCopy(&accum, data->start_class,
				   struct regnode_charclass_class);
			flags |= SCF_DO_STCLASS_OR;
			data->start_class->flags |= ANYOF_EOS;
		    }
		}

                if (PERL_ENABLE_TRIE_OPTIMISATION && OP( startbranch ) == BRANCH ) {
		/* demq.

		   Assuming this was/is a branch we are dealing with: 'scan' now
		   points at the item that follows the branch sequence, whatever
		   it is. We now start at the beginning of the sequence and look
		   for subsequences of

		   BRANCH->EXACT=>x1
		   BRANCH->EXACT=>x2
		   tail

		   which would be constructed from a pattern like /A|LIST|OF|WORDS/

		   If we can find such a subseqence we need to turn the first
		   element into a trie and then add the subsequent branch exact
		   strings to the trie.

		   We have two cases

		     1. patterns where the whole set of branch can be converted. 

		     2. patterns where only a subset can be converted.

		   In case 1 we can replace the whole set with a single regop
		   for the trie. In case 2 we need to keep the start and end
		   branchs so

		     'BRANCH EXACT; BRANCH EXACT; BRANCH X'
		     becomes BRANCH TRIE; BRANCH X;

		  There is an additional case, that being where there is a 
		  common prefix, which gets split out into an EXACT like node
		  preceding the TRIE node.

		  If x(1..n)==tail then we can do a simple trie, if not we make
		  a "jump" trie, such that when we match the appropriate word
		  we "jump" to the appopriate tail node. Essentailly we turn
		  a nested if into a case structure of sorts.

		*/
		
		    int made=0;
		    if (!re_trie_maxbuff) {
			re_trie_maxbuff = get_sv(RE_TRIE_MAXBUF_NAME, 1);
			if (!SvIOK(re_trie_maxbuff))
			    sv_setiv(re_trie_maxbuff, RE_TRIE_MAXBUF_INIT);
		    }
                    if ( SvIV(re_trie_maxbuff)>=0  ) {
                        regnode *cur;
                        regnode *first = (regnode *)NULL;
                        regnode *last = (regnode *)NULL;
                        regnode *tail = scan;
                        U8 optype = 0;
                        U32 count=0;

#ifdef DEBUGGING
                        SV * const mysv = sv_newmortal();       /* for dumping */
#endif
                        /* var tail is used because there may be a TAIL
                           regop in the way. Ie, the exacts will point to the
                           thing following the TAIL, but the last branch will
                           point at the TAIL. So we advance tail. If we
                           have nested (?:) we may have to move through several
                           tails.
                         */

                        while ( OP( tail ) == TAIL ) {
                            /* this is the TAIL generated by (?:) */
                            tail = regnext( tail );
                        }

                        
                        DEBUG_OPTIMISE_r({
                            regprop(RExC_rx, mysv, tail );
                            PerlIO_printf( Perl_debug_log, "%*s%s%s\n",
                                (int)depth * 2 + 2, "", 
                                "Looking for TRIE'able sequences. Tail node is: ", 
                                SvPV_nolen_const( mysv )
                            );
                        });
                        
                        /*

                           step through the branches, cur represents each
                           branch, noper is the first thing to be matched
                           as part of that branch and noper_next is the
                           regnext() of that node. if noper is an EXACT
                           and noper_next is the same as scan (our current
                           position in the regex) then the EXACT branch is
                           a possible optimization target. Once we have
                           two or more consequetive such branches we can
                           create a trie of the EXACT's contents and stich
                           it in place. If the sequence represents all of
                           the branches we eliminate the whole thing and
                           replace it with a single TRIE. If it is a
                           subsequence then we need to stitch it in. This
                           means the first branch has to remain, and needs
                           to be repointed at the item on the branch chain
                           following the last branch optimized. This could
                           be either a BRANCH, in which case the
                           subsequence is internal, or it could be the
                           item following the branch sequence in which
                           case the subsequence is at the end.

                        */

                        /* dont use tail as the end marker for this traverse */
                        for ( cur = startbranch ; cur != scan ; cur = regnext( cur ) ) {
                            regnode * const noper = NEXTOPER( cur );
#if defined(DEBUGGING) || defined(NOJUMPTRIE)
                            regnode * const noper_next = regnext( noper );
#endif

                            DEBUG_OPTIMISE_r({
                                regprop(RExC_rx, mysv, cur);
                                PerlIO_printf( Perl_debug_log, "%*s- %s (%d)",
                                   (int)depth * 2 + 2,"", SvPV_nolen_const( mysv ), REG_NODE_NUM(cur) );

                                regprop(RExC_rx, mysv, noper);
                                PerlIO_printf( Perl_debug_log, " -> %s",
                                    SvPV_nolen_const(mysv));

                                if ( noper_next ) {
                                  regprop(RExC_rx, mysv, noper_next );
                                  PerlIO_printf( Perl_debug_log,"\t=> %s\t",
                                    SvPV_nolen_const(mysv));
                                }
                                PerlIO_printf( Perl_debug_log, "(First==%d,Last==%d,Cur==%d)\n",
                                   REG_NODE_NUM(first), REG_NODE_NUM(last), REG_NODE_NUM(cur) );
                            });
                            if ( (((first && optype!=NOTHING) ? OP( noper ) == optype
                                         : PL_regkind[ OP( noper ) ] == EXACT )
                                  || OP(noper) == NOTHING )
#ifdef NOJUMPTRIE
                                  && noper_next == tail
#endif
                                  && count < U16_MAX)
                            {
                                count++;
                                if ( !first || optype == NOTHING ) {
                                    if (!first) first = cur;
                                    optype = OP( noper );
                                } else {
                                    last = cur;
                                }
                            } else {
/* 
    Currently we do not believe that the trie logic can
    handle case insensitive matching properly when the
    pattern is not unicode (thus forcing unicode semantics).

    If/when this is fixed the following define can be swapped
    in below to fully enable trie logic.

#define TRIE_TYPE_IS_SAFE 1

*/
#define TRIE_TYPE_IS_SAFE (UTF || optype==EXACT)

                                if ( last && TRIE_TYPE_IS_SAFE ) {
                                    make_trie( pRExC_state, 
                                            startbranch, first, cur, tail, count, 
                                            optype, depth+1 );
                                }
                                if ( PL_regkind[ OP( noper ) ] == EXACT
#ifdef NOJUMPTRIE
                                     && noper_next == tail
#endif
                                ){
                                    count = 1;
                                    first = cur;
                                    optype = OP( noper );
                                } else {
                                    count = 0;
                                    first = NULL;
                                    optype = 0;
                                }
                                last = NULL;
                            }
                        }
                        DEBUG_OPTIMISE_r({
                            regprop(RExC_rx, mysv, cur);
                            PerlIO_printf( Perl_debug_log,
                              "%*s- %s (%d) <SCAN FINISHED>\n", (int)depth * 2 + 2,
                              "", SvPV_nolen_const( mysv ),REG_NODE_NUM(cur));

                        });
                        
                        if ( last && TRIE_TYPE_IS_SAFE ) {
                            made= make_trie( pRExC_state, startbranch, first, scan, tail, count, optype, depth+1 );
#ifdef TRIE_STUDY_OPT	
                            if ( ((made == MADE_EXACT_TRIE && 
                                 startbranch == first) 
                                 || ( first_non_open == first )) && 
                                 depth==0 ) {
                                flags |= SCF_TRIE_RESTUDY;
                                if ( startbranch == first 
                                     && scan == tail ) 
                                {
                                    RExC_seen &=~REG_TOP_LEVEL_BRANCHES;
                                }
                            }
#endif
                        }
                    }
                    
                } /* do trie */
                
	    }
	    else if ( code == BRANCHJ ) {  /* single branch is optimized. */
		scan = NEXTOPER(NEXTOPER(scan));
	    } else			/* single branch is optimized. */
		scan = NEXTOPER(scan);
	    continue;
	} else if (OP(scan) == SUSPEND || OP(scan) == GOSUB || OP(scan) == GOSTART) {
	    scan_frame *newframe = NULL;
	    I32 paren;
	    regnode *start;
	    regnode *end;

	    if (OP(scan) != SUSPEND) {
	    /* set the pointer */
	        if (OP(scan) == GOSUB) {
	            paren = ARG(scan);
	            RExC_recurse[ARG2L(scan)] = scan;
                    start = RExC_open_parens[paren-1];
                    end   = RExC_close_parens[paren-1];
                } else {
                    paren = 0;
                    start = RExC_rxi->program + 1;
                    end   = RExC_opend;
                }
                if (!recursed) {
                    Newxz(recursed, (((RExC_npar)>>3) +1), U8);
                    SAVEFREEPV(recursed);
                }
                if (!PAREN_TEST(recursed,paren+1)) {
		    PAREN_SET(recursed,paren+1);
                    Newx(newframe,1,scan_frame);
                } else {
                    if (flags & SCF_DO_SUBSTR) {
                        SCAN_COMMIT(pRExC_state,data,minlenp);
                        data->longest = &(data->longest_float);
                    }
                    is_inf = is_inf_internal = 1;
                    if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
                        cl_anything(pRExC_state, data->start_class);
                    flags &= ~SCF_DO_STCLASS;
	        }
            } else {
	        Newx(newframe,1,scan_frame);
	        paren = stopparen;
	        start = scan+2;
	        end = regnext(scan);
	    }
	    if (newframe) {
                assert(start);
                assert(end);
	        SAVEFREEPV(newframe);
	        newframe->next = regnext(scan);
	        newframe->last = last;
	        newframe->stop = stopparen;
	        newframe->prev = frame;

	        frame = newframe;
	        scan =  start;
	        stopparen = paren;
	        last = end;

	        continue;
	    }
	}
	else if (OP(scan) == EXACT) {
	    I32 l = STR_LEN(scan);
	    UV uc;
	    if (UTF) {
		const U8 * const s = (U8*)STRING(scan);
		l = utf8_length(s, s + l);
		uc = utf8_to_uvchr(s, NULL);
	    } else {
		uc = *((U8*)STRING(scan));
	    }
	    min += l;
	    if (flags & SCF_DO_SUBSTR) { /* Update longest substr. */
		/* The code below prefers earlier match for fixed
		   offset, later match for variable offset.  */
		if (data->last_end == -1) { /* Update the start info. */
		    data->last_start_min = data->pos_min;
 		    data->last_start_max = is_inf
 			? I32_MAX : data->pos_min + data->pos_delta;
		}
		sv_catpvn(data->last_found, STRING(scan), STR_LEN(scan));
		if (UTF)
		    SvUTF8_on(data->last_found);
		{
		    SV * const sv = data->last_found;
		    MAGIC * const mg = SvUTF8(sv) && SvMAGICAL(sv) ?
			mg_find(sv, PERL_MAGIC_utf8) : NULL;
		    if (mg && mg->mg_len >= 0)
			mg->mg_len += utf8_length((U8*)STRING(scan),
						  (U8*)STRING(scan)+STR_LEN(scan));
		}
		data->last_end = data->pos_min + l;
		data->pos_min += l; /* As in the first entry. */
		data->flags &= ~SF_BEFORE_EOL;
	    }
	    if (flags & SCF_DO_STCLASS_AND) {
		/* Check whether it is compatible with what we know already! */
		int compat = 1;

		if (uc >= 0x100 ||
		    (!(data->start_class->flags & (ANYOF_CLASS | ANYOF_LOCALE))
		    && !ANYOF_BITMAP_TEST(data->start_class, uc)
		    && (!(data->start_class->flags & ANYOF_FOLD)
			|| !ANYOF_BITMAP_TEST(data->start_class, PL_fold[uc])))
                    )
		    compat = 0;
		ANYOF_CLASS_ZERO(data->start_class);
		ANYOF_BITMAP_ZERO(data->start_class);
		if (compat)
		    ANYOF_BITMAP_SET(data->start_class, uc);
		data->start_class->flags &= ~ANYOF_EOS;
		if (uc < 0x100)
		  data->start_class->flags &= ~ANYOF_UNICODE_ALL;
	    }
	    else if (flags & SCF_DO_STCLASS_OR) {
		/* false positive possible if the class is case-folded */
		if (uc < 0x100)
		    ANYOF_BITMAP_SET(data->start_class, uc);
		else
		    data->start_class->flags |= ANYOF_UNICODE_ALL;
		data->start_class->flags &= ~ANYOF_EOS;
		cl_and(data->start_class, and_withp);
	    }
	    flags &= ~SCF_DO_STCLASS;
	}
	else if (PL_regkind[OP(scan)] == EXACT) { /* But OP != EXACT! */
	    I32 l = STR_LEN(scan);
	    UV uc = *((U8*)STRING(scan));

	    /* Search for fixed substrings supports EXACT only. */
	    if (flags & SCF_DO_SUBSTR) {
		assert(data);
		SCAN_COMMIT(pRExC_state, data, minlenp);
	    }
	    if (UTF) {
		const U8 * const s = (U8 *)STRING(scan);
		l = utf8_length(s, s + l);
		uc = utf8_to_uvchr(s, NULL);
	    }
	    min += l;
	    if (flags & SCF_DO_SUBSTR)
		data->pos_min += l;
	    if (flags & SCF_DO_STCLASS_AND) {
		/* Check whether it is compatible with what we know already! */
		int compat = 1;

		if (uc >= 0x100 ||
		    (!(data->start_class->flags & (ANYOF_CLASS | ANYOF_LOCALE))
		    && !ANYOF_BITMAP_TEST(data->start_class, uc)
		     && !ANYOF_BITMAP_TEST(data->start_class, PL_fold[uc])))
		    compat = 0;
		ANYOF_CLASS_ZERO(data->start_class);
		ANYOF_BITMAP_ZERO(data->start_class);
		if (compat) {
		    ANYOF_BITMAP_SET(data->start_class, uc);
		    data->start_class->flags &= ~ANYOF_EOS;
		    data->start_class->flags |= ANYOF_FOLD;
		    if (OP(scan) == EXACTFL)
			data->start_class->flags |= ANYOF_LOCALE;
		}
	    }
	    else if (flags & SCF_DO_STCLASS_OR) {
		if (data->start_class->flags & ANYOF_FOLD) {
		    /* false positive possible if the class is case-folded.
		       Assume that the locale settings are the same... */
		    if (uc < 0x100)
			ANYOF_BITMAP_SET(data->start_class, uc);
		    data->start_class->flags &= ~ANYOF_EOS;
		}
		cl_and(data->start_class, and_withp);
	    }
	    flags &= ~SCF_DO_STCLASS;
	}
	else if (strchr((const char*)PL_varies,OP(scan))) {
	    I32 mincount, maxcount, minnext, deltanext, fl = 0;
	    I32 f = flags, pos_before = 0;
	    regnode * const oscan = scan;
	    struct regnode_charclass_class this_class;
	    struct regnode_charclass_class *oclass = NULL;
	    I32 next_is_eval = 0;

	    switch (PL_regkind[OP(scan)]) {
	    case WHILEM:		/* End of (?:...)* . */
		scan = NEXTOPER(scan);
		goto finish;
	    case PLUS:
		if (flags & (SCF_DO_SUBSTR | SCF_DO_STCLASS)) {
		    next = NEXTOPER(scan);
		    if (OP(next) == EXACT || (flags & SCF_DO_STCLASS)) {
			mincount = 1;
			maxcount = REG_INFTY;
			next = regnext(scan);
			scan = NEXTOPER(scan);
			goto do_curly;
		    }
		}
		if (flags & SCF_DO_SUBSTR)
		    data->pos_min++;
		min++;
		/* Fall through. */
	    case STAR:
		if (flags & SCF_DO_STCLASS) {
		    mincount = 0;
		    maxcount = REG_INFTY;
		    next = regnext(scan);
		    scan = NEXTOPER(scan);
		    goto do_curly;
		}
		is_inf = is_inf_internal = 1;
		scan = regnext(scan);
		if (flags & SCF_DO_SUBSTR) {
		    SCAN_COMMIT(pRExC_state, data, minlenp); /* Cannot extend fixed substrings */
		    data->longest = &(data->longest_float);
		}
		goto optimize_curly_tail;
	    case CURLY:
	        if (stopparen>0 && (OP(scan)==CURLYN || OP(scan)==CURLYM)
	            && (scan->flags == stopparen))
		{
		    mincount = 1;
		    maxcount = 1;
		} else {
		    mincount = ARG1(scan);
		    maxcount = ARG2(scan);
		}
		next = regnext(scan);
		if (OP(scan) == CURLYX) {
		    I32 lp = (data ? *(data->last_closep) : 0);
		    scan->flags = ((lp <= (I32)U8_MAX) ? (U8)lp : U8_MAX);
		}
		scan = NEXTOPER(scan) + EXTRA_STEP_2ARGS;
		next_is_eval = (OP(scan) == EVAL);
	      do_curly:
		if (flags & SCF_DO_SUBSTR) {
		    if (mincount == 0) SCAN_COMMIT(pRExC_state,data,minlenp); /* Cannot extend fixed substrings */
		    pos_before = data->pos_min;
		}
		if (data) {
		    fl = data->flags;
		    data->flags &= ~(SF_HAS_PAR|SF_IN_PAR|SF_HAS_EVAL);
		    if (is_inf)
			data->flags |= SF_IS_INF;
		}
		if (flags & SCF_DO_STCLASS) {
		    cl_init(pRExC_state, &this_class);
		    oclass = data->start_class;
		    data->start_class = &this_class;
		    f |= SCF_DO_STCLASS_AND;
		    f &= ~SCF_DO_STCLASS_OR;
		}
		/* These are the cases when once a subexpression
		   fails at a particular position, it cannot succeed
		   even after backtracking at the enclosing scope.
		
		   XXXX what if minimal match and we are at the
		        initial run of {n,m}? */
		if ((mincount != maxcount - 1) && (maxcount != REG_INFTY))
		    f &= ~SCF_WHILEM_VISITED_POS;

		/* This will finish on WHILEM, setting scan, or on NULL: */
		minnext = study_chunk(pRExC_state, &scan, minlenp, &deltanext, 
		                      last, data, stopparen, recursed, NULL,
				      (mincount == 0
					? (f & ~SCF_DO_SUBSTR) : f),depth+1);

		if (flags & SCF_DO_STCLASS)
		    data->start_class = oclass;
		if (mincount == 0 || minnext == 0) {
		    if (flags & SCF_DO_STCLASS_OR) {
			cl_or(pRExC_state, data->start_class, &this_class);
		    }
		    else if (flags & SCF_DO_STCLASS_AND) {
			/* Switch to OR mode: cache the old value of
			 * data->start_class */
			INIT_AND_WITHP;
			StructCopy(data->start_class, and_withp,
				   struct regnode_charclass_class);
			flags &= ~SCF_DO_STCLASS_AND;
			StructCopy(&this_class, data->start_class,
				   struct regnode_charclass_class);
			flags |= SCF_DO_STCLASS_OR;
			data->start_class->flags |= ANYOF_EOS;
		    }
		} else {		/* Non-zero len */
		    if (flags & SCF_DO_STCLASS_OR) {
			cl_or(pRExC_state, data->start_class, &this_class);
			cl_and(data->start_class, and_withp);
		    }
		    else if (flags & SCF_DO_STCLASS_AND)
			cl_and(data->start_class, &this_class);
		    flags &= ~SCF_DO_STCLASS;
		}
		if (!scan) 		/* It was not CURLYX, but CURLY. */
		    scan = next;
		if ( /* ? quantifier ok, except for (?{ ... }) */
		    (next_is_eval || !(mincount == 0 && maxcount == 1))
		    && (minnext == 0) && (deltanext == 0)
		    && data && !(data->flags & (SF_HAS_PAR|SF_IN_PAR))
		    && maxcount <= REG_INFTY/3) /* Complement check for big count */
		{
		    ckWARNreg(RExC_parse,
			      "Quantifier unexpected on zero-length expression");
		}

		min += minnext * mincount;
		is_inf_internal |= ((maxcount == REG_INFTY
				     && (minnext + deltanext) > 0)
				    || deltanext == I32_MAX);
		is_inf |= is_inf_internal;
		delta += (minnext + deltanext) * maxcount - minnext * mincount;

		/* Try powerful optimization CURLYX => CURLYN. */
		if (  OP(oscan) == CURLYX && data
		      && data->flags & SF_IN_PAR
		      && !(data->flags & SF_HAS_EVAL)
		      && !deltanext && minnext == 1 ) {
		    /* Try to optimize to CURLYN.  */
		    regnode *nxt = NEXTOPER(oscan) + EXTRA_STEP_2ARGS;
		    regnode * const nxt1 = nxt;
#ifdef DEBUGGING
		    regnode *nxt2;
#endif

		    /* Skip open. */
		    nxt = regnext(nxt);
		    if (!strchr((const char*)PL_simple,OP(nxt))
			&& !(PL_regkind[OP(nxt)] == EXACT
			     && STR_LEN(nxt) == 1))
			goto nogo;
#ifdef DEBUGGING
		    nxt2 = nxt;
#endif
		    nxt = regnext(nxt);
		    if (OP(nxt) != CLOSE)
			goto nogo;
		    if (RExC_open_parens) {
			RExC_open_parens[ARG(nxt1)-1]=oscan; /*open->CURLYM*/
			RExC_close_parens[ARG(nxt1)-1]=nxt+2; /*close->while*/
		    }
		    /* Now we know that nxt2 is the only contents: */
		    oscan->flags = (U8)ARG(nxt);
		    OP(oscan) = CURLYN;
		    OP(nxt1) = NOTHING;	/* was OPEN. */

#ifdef DEBUGGING
		    OP(nxt1 + 1) = OPTIMIZED; /* was count. */
		    NEXT_OFF(nxt1+ 1) = 0; /* just for consistancy. */
		    NEXT_OFF(nxt2) = 0;	/* just for consistancy with CURLY. */
		    OP(nxt) = OPTIMIZED;	/* was CLOSE. */
		    OP(nxt + 1) = OPTIMIZED; /* was count. */
		    NEXT_OFF(nxt+ 1) = 0; /* just for consistancy. */
#endif
		}
	      nogo:

		/* Try optimization CURLYX => CURLYM. */
		if (  OP(oscan) == CURLYX && data
		      && !(data->flags & SF_HAS_PAR)
		      && !(data->flags & SF_HAS_EVAL)
		      && !deltanext	/* atom is fixed width */
		      && minnext != 0	/* CURLYM can't handle zero width */
		) {
		    /* XXXX How to optimize if data == 0? */
		    /* Optimize to a simpler form.  */
		    regnode *nxt = NEXTOPER(oscan) + EXTRA_STEP_2ARGS; /* OPEN */
		    regnode *nxt2;

		    OP(oscan) = CURLYM;
		    while ( (nxt2 = regnext(nxt)) /* skip over embedded stuff*/
			    && (OP(nxt2) != WHILEM))
			nxt = nxt2;
		    OP(nxt2)  = SUCCEED; /* Whas WHILEM */
		    /* Need to optimize away parenths. */
		    if (data->flags & SF_IN_PAR) {
			/* Set the parenth number.  */
			regnode *nxt1 = NEXTOPER(oscan) + EXTRA_STEP_2ARGS; /* OPEN*/

			if (OP(nxt) != CLOSE)
			    FAIL("Panic opt close");
			oscan->flags = (U8)ARG(nxt);
			if (RExC_open_parens) {
			    RExC_open_parens[ARG(nxt1)-1]=oscan; /*open->CURLYM*/
			    RExC_close_parens[ARG(nxt1)-1]=nxt2+1; /*close->NOTHING*/
			}
			OP(nxt1) = OPTIMIZED;	/* was OPEN. */
			OP(nxt) = OPTIMIZED;	/* was CLOSE. */

#ifdef DEBUGGING
			OP(nxt1 + 1) = OPTIMIZED; /* was count. */
			OP(nxt + 1) = OPTIMIZED; /* was count. */
			NEXT_OFF(nxt1 + 1) = 0; /* just for consistancy. */
			NEXT_OFF(nxt + 1) = 0; /* just for consistancy. */
#endif
#if 0
			while ( nxt1 && (OP(nxt1) != WHILEM)) {
			    regnode *nnxt = regnext(nxt1);
			
			    if (nnxt == nxt) {
				if (reg_off_by_arg[OP(nxt1)])
				    ARG_SET(nxt1, nxt2 - nxt1);
				else if (nxt2 - nxt1 < U16_MAX)
				    NEXT_OFF(nxt1) = nxt2 - nxt1;
				else
				    OP(nxt) = NOTHING;	/* Cannot beautify */
			    }
			    nxt1 = nnxt;
			}
#endif
			/* Optimize again: */
			study_chunk(pRExC_state, &nxt1, minlenp, &deltanext, nxt,
				    NULL, stopparen, recursed, NULL, 0,depth+1);
		    }
		    else
			oscan->flags = 0;
		}
		else if ((OP(oscan) == CURLYX)
			 && (flags & SCF_WHILEM_VISITED_POS)
			 /* See the comment on a similar expression above.
			    However, this time it not a subexpression
			    we care about, but the expression itself. */
			 && (maxcount == REG_INFTY)
			 && data && ++data->whilem_c < 16) {
		    /* This stays as CURLYX, we can put the count/of pair. */
		    /* Find WHILEM (as in regexec.c) */
		    regnode *nxt = oscan + NEXT_OFF(oscan);

		    if (OP(PREVOPER(nxt)) == NOTHING) /* LONGJMP */
			nxt += ARG(nxt);
		    PREVOPER(nxt)->flags = (U8)(data->whilem_c
			| (RExC_whilem_seen << 4)); /* On WHILEM */
		}
		if (data && fl & (SF_HAS_PAR|SF_IN_PAR))
		    pars++;
		if (flags & SCF_DO_SUBSTR) {
		    SV *last_str = NULL;
		    int counted = mincount != 0;

		    if (data->last_end > 0 && mincount != 0) { /* Ends with a string. */
#if defined(SPARC64_GCC_WORKAROUND)
			I32 b = 0;
			STRLEN l = 0;
			const char *s = NULL;
			I32 old = 0;

			if (pos_before >= data->last_start_min)
			    b = pos_before;
			else
			    b = data->last_start_min;

			l = 0;
			s = SvPV_const(data->last_found, l);
			old = b - data->last_start_min;

#else
			I32 b = pos_before >= data->last_start_min
			    ? pos_before : data->last_start_min;
			STRLEN l;
			const char * const s = SvPV_const(data->last_found, l);
			I32 old = b - data->last_start_min;
#endif

			if (UTF)
			    old = utf8_hop((U8*)s, old) - (U8*)s;
			
			l -= old;
			/* Get the added string: */
			last_str = newSVpvn_utf8(s  + old, l, UTF);
			if (deltanext == 0 && pos_before == b) {
			    /* What was added is a constant string */
			    if (mincount > 1) {
				SvGROW(last_str, (mincount * l) + 1);
				repeatcpy(SvPVX(last_str) + l,
					  SvPVX_const(last_str), l, mincount - 1);
				SvCUR_set(last_str, SvCUR(last_str) * mincount);
				/* Add additional parts. */
				SvCUR_set(data->last_found,
					  SvCUR(data->last_found) - l);
				sv_catsv(data->last_found, last_str);
				{
				    SV * sv = data->last_found;
				    MAGIC *mg =
					SvUTF8(sv) && SvMAGICAL(sv) ?
					mg_find(sv, PERL_MAGIC_utf8) : NULL;
				    if (mg && mg->mg_len >= 0)
					mg->mg_len += CHR_SVLEN(last_str) - l;
				}
				data->last_end += l * (mincount - 1);
			    }
			} else {
			    /* start offset must point into the last copy */
			    data->last_start_min += minnext * (mincount - 1);
			    data->last_start_max += is_inf ? I32_MAX
				: (maxcount - 1) * (minnext + data->pos_delta);
			}
		    }
		    /* It is counted once already... */
		    data->pos_min += minnext * (mincount - counted);
		    data->pos_delta += - counted * deltanext +
			(minnext + deltanext) * maxcount - minnext * mincount;
		    if (mincount != maxcount) {
			 /* Cannot extend fixed substrings found inside
			    the group.  */
			SCAN_COMMIT(pRExC_state,data,minlenp);
			if (mincount && last_str) {
			    SV * const sv = data->last_found;
			    MAGIC * const mg = SvUTF8(sv) && SvMAGICAL(sv) ?
				mg_find(sv, PERL_MAGIC_utf8) : NULL;

			    if (mg)
				mg->mg_len = -1;
			    sv_setsv(sv, last_str);
			    data->last_end = data->pos_min;
			    data->last_start_min =
				data->pos_min - CHR_SVLEN(last_str);
			    data->last_start_max = is_inf
				? I32_MAX
				: data->pos_min + data->pos_delta
				- CHR_SVLEN(last_str);
			}
			data->longest = &(data->longest_float);
		    }
		    SvREFCNT_dec(last_str);
		}
		if (data && (fl & SF_HAS_EVAL))
		    data->flags |= SF_HAS_EVAL;
	      optimize_curly_tail:
		if (OP(oscan) != CURLYX) {
		    while (PL_regkind[OP(next = regnext(oscan))] == NOTHING
			   && NEXT_OFF(next))
			NEXT_OFF(oscan) += NEXT_OFF(next);
		}
		continue;
	    default:			/* REF and CLUMP only? */
		if (flags & SCF_DO_SUBSTR) {
		    SCAN_COMMIT(pRExC_state,data,minlenp);	/* Cannot expect anything... */
		    data->longest = &(data->longest_float);
		}
		is_inf = is_inf_internal = 1;
		if (flags & SCF_DO_STCLASS_OR)
		    cl_anything(pRExC_state, data->start_class);
		flags &= ~SCF_DO_STCLASS;
		break;
	    }
	}
	else if (OP(scan) == LNBREAK) {
	    if (flags & SCF_DO_STCLASS) {
		int value = 0;
		data->start_class->flags &= ~ANYOF_EOS;	/* No match on empty */
    	        if (flags & SCF_DO_STCLASS_AND) {
                    for (value = 0; value < 256; value++)
                        if (!is_VERTWS_cp(value))
                            ANYOF_BITMAP_CLEAR(data->start_class, value);  
                }                                                              
                else {                                                         
                    for (value = 0; value < 256; value++)
                        if (is_VERTWS_cp(value))
                            ANYOF_BITMAP_SET(data->start_class, value);	   
                }                                                              
                if (flags & SCF_DO_STCLASS_OR)
		    cl_and(data->start_class, and_withp);
		flags &= ~SCF_DO_STCLASS;
            }
	    min += 1;
	    delta += 1;
            if (flags & SCF_DO_SUBSTR) {
    	        SCAN_COMMIT(pRExC_state,data,minlenp);	/* Cannot expect anything... */
    	        data->pos_min += 1;
	        data->pos_delta += 1;
		data->longest = &(data->longest_float);
    	    }
    	    
	}
	else if (OP(scan) == FOLDCHAR) {
	    int d = ARG(scan)==0xDF ? 1 : 2;
	    flags &= ~SCF_DO_STCLASS;
            min += 1;
            delta += d;
            if (flags & SCF_DO_SUBSTR) {
	        SCAN_COMMIT(pRExC_state,data,minlenp);	/* Cannot expect anything... */
	        data->pos_min += 1;
	        data->pos_delta += d;
		data->longest = &(data->longest_float);
	    }
	}
	else if (strchr((const char*)PL_simple,OP(scan))) {
	    int value = 0;

	    if (flags & SCF_DO_SUBSTR) {
		SCAN_COMMIT(pRExC_state,data,minlenp);
		data->pos_min++;
	    }
	    min++;
	    if (flags & SCF_DO_STCLASS) {
		data->start_class->flags &= ~ANYOF_EOS;	/* No match on empty */

		/* Some of the logic below assumes that switching
		   locale on will only add false positives. */
		switch (PL_regkind[OP(scan)]) {
		case SANY:
		default:
		  do_default:
		    /* Perl_croak(aTHX_ "panic: unexpected simple REx opcode %d", OP(scan)); */
		    if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
			cl_anything(pRExC_state, data->start_class);
		    break;
		case REG_ANY:
		    if (OP(scan) == SANY)
			goto do_default;
		    if (flags & SCF_DO_STCLASS_OR) { /* Everything but \n */
			value = (ANYOF_BITMAP_TEST(data->start_class,'\n')
				 || (data->start_class->flags & ANYOF_CLASS));
			cl_anything(pRExC_state, data->start_class);
		    }
		    if (flags & SCF_DO_STCLASS_AND || !value)
			ANYOF_BITMAP_CLEAR(data->start_class,'\n');
		    break;
		case ANYOF:
		    if (flags & SCF_DO_STCLASS_AND)
			cl_and(data->start_class,
			       (struct regnode_charclass_class*)scan);
		    else
			cl_or(pRExC_state, data->start_class,
			      (struct regnode_charclass_class*)scan);
		    break;
		case ALNUM:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NALNUM);
			    for (value = 0; value < 256; value++)
				if (!isALNUM(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_ALNUM);
			else {
			    for (value = 0; value < 256; value++)
				if (isALNUM(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			
			}
		    }
		    break;
		case ALNUML:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NALNUM);
		    }
		    else {
			ANYOF_CLASS_SET(data->start_class,ANYOF_ALNUM);
			data->start_class->flags |= ANYOF_LOCALE;
		    }
		    break;
		case NALNUM:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_ALNUM);
			    for (value = 0; value < 256; value++)
				if (isALNUM(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_NALNUM);
			else {
			    for (value = 0; value < 256; value++)
				if (!isALNUM(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			
			}
		    }
		    break;
		case NALNUML:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_ALNUM);
		    }
		    else {
			data->start_class->flags |= ANYOF_LOCALE;
			ANYOF_CLASS_SET(data->start_class,ANYOF_NALNUM);
		    }
		    break;
		case SPACE:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NSPACE);
			    for (value = 0; value < 256; value++)
				if (!isSPACE(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_SPACE);
			else {
			    for (value = 0; value < 256; value++)
				if (isSPACE(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			
			}
		    }
		    break;
		case SPACEL:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NSPACE);
		    }
		    else {
			data->start_class->flags |= ANYOF_LOCALE;
			ANYOF_CLASS_SET(data->start_class,ANYOF_SPACE);
		    }
		    break;
		case NSPACE:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_SPACE);
			    for (value = 0; value < 256; value++)
				if (isSPACE(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_NSPACE);
			else {
			    for (value = 0; value < 256; value++)
				if (!isSPACE(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			
			}
		    }
		    break;
		case NSPACEL:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_SPACE);
			    for (value = 0; value < 256; value++)
				if (!isSPACE(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			data->start_class->flags |= ANYOF_LOCALE;
			ANYOF_CLASS_SET(data->start_class,ANYOF_NSPACE);
		    }
		    break;
		case DIGIT:
		    if (flags & SCF_DO_STCLASS_AND) {
			ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NDIGIT);
			for (value = 0; value < 256; value++)
			    if (!isDIGIT(value))
				ANYOF_BITMAP_CLEAR(data->start_class, value);
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_DIGIT);
			else {
			    for (value = 0; value < 256; value++)
				if (isDIGIT(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			
			}
		    }
		    break;
		case NDIGIT:
		    if (flags & SCF_DO_STCLASS_AND) {
			ANYOF_CLASS_CLEAR(data->start_class,ANYOF_DIGIT);
			for (value = 0; value < 256; value++)
			    if (isDIGIT(value))
				ANYOF_BITMAP_CLEAR(data->start_class, value);
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_NDIGIT);
			else {
			    for (value = 0; value < 256; value++)
				if (!isDIGIT(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			
			}
		    }
		    break;
		CASE_SYNST_FNC(VERTWS);
		CASE_SYNST_FNC(HORIZWS);
		
		}
		if (flags & SCF_DO_STCLASS_OR)
		    cl_and(data->start_class, and_withp);
		flags &= ~SCF_DO_STCLASS;
	    }
	}
	else if (PL_regkind[OP(scan)] == EOL && flags & SCF_DO_SUBSTR) {
	    data->flags |= (OP(scan) == MEOL
			    ? SF_BEFORE_MEOL
			    : SF_BEFORE_SEOL);
	}
	else if (  PL_regkind[OP(scan)] == BRANCHJ
		 /* Lookbehind, or need to calculate parens/evals/stclass: */
		   && (scan->flags || data || (flags & SCF_DO_STCLASS))
		   && (OP(scan) == IFMATCH || OP(scan) == UNLESSM)) {
            if ( !PERL_ENABLE_POSITIVE_ASSERTION_STUDY 
                || OP(scan) == UNLESSM )
            {
                /* Negative Lookahead/lookbehind
                   In this case we can't do fixed string optimisation.
                */

                I32 deltanext, minnext, fake = 0;
                regnode *nscan;
                struct regnode_charclass_class intrnl;
                int f = 0;

                data_fake.flags = 0;
                if (data) {
                    data_fake.whilem_c = data->whilem_c;
                    data_fake.last_closep = data->last_closep;
		}
                else
                    data_fake.last_closep = &fake;
		data_fake.pos_delta = delta;
                if ( flags & SCF_DO_STCLASS && !scan->flags
                     && OP(scan) == IFMATCH ) { /* Lookahead */
                    cl_init(pRExC_state, &intrnl);
                    data_fake.start_class = &intrnl;
                    f |= SCF_DO_STCLASS_AND;
		}
                if (flags & SCF_WHILEM_VISITED_POS)
                    f |= SCF_WHILEM_VISITED_POS;
                next = regnext(scan);
                nscan = NEXTOPER(NEXTOPER(scan));
                minnext = study_chunk(pRExC_state, &nscan, minlenp, &deltanext, 
                    last, &data_fake, stopparen, recursed, NULL, f, depth+1);
                if (scan->flags) {
                    if (deltanext) {
			FAIL("Variable length lookbehind not implemented");
                    }
                    else if (minnext > (I32)U8_MAX) {
			FAIL2("Lookbehind longer than %"UVuf" not implemented", (UV)U8_MAX);
                    }
                    scan->flags = (U8)minnext;
                }
                if (data) {
                    if (data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
                        pars++;
                    if (data_fake.flags & SF_HAS_EVAL)
                        data->flags |= SF_HAS_EVAL;
                    data->whilem_c = data_fake.whilem_c;
                }
                if (f & SCF_DO_STCLASS_AND) {
		    if (flags & SCF_DO_STCLASS_OR) {
			/* OR before, AND after: ideally we would recurse with
			 * data_fake to get the AND applied by study of the
			 * remainder of the pattern, and then derecurse;
			 * *** HACK *** for now just treat as "no information".
			 * See [perl #56690].
			 */
			cl_init(pRExC_state, data->start_class);
		    }  else {
			/* AND before and after: combine and continue */
			const int was = (data->start_class->flags & ANYOF_EOS);

			cl_and(data->start_class, &intrnl);
			if (was)
			    data->start_class->flags |= ANYOF_EOS;
		    }
                }
	    }
#if PERL_ENABLE_POSITIVE_ASSERTION_STUDY
            else {
                /* Positive Lookahead/lookbehind
                   In this case we can do fixed string optimisation,
                   but we must be careful about it. Note in the case of
                   lookbehind the positions will be offset by the minimum
                   length of the pattern, something we won't know about
                   until after the recurse.
                */
                I32 deltanext, fake = 0;
                regnode *nscan;
                struct regnode_charclass_class intrnl;
                int f = 0;
                /* We use SAVEFREEPV so that when the full compile 
                    is finished perl will clean up the allocated 
                    minlens when its all done. This was we don't
                    have to worry about freeing them when we know
                    they wont be used, which would be a pain.
                 */
                I32 *minnextp;
                Newx( minnextp, 1, I32 );
                SAVEFREEPV(minnextp);

                if (data) {
                    StructCopy(data, &data_fake, scan_data_t);
                    if ((flags & SCF_DO_SUBSTR) && data->last_found) {
                        f |= SCF_DO_SUBSTR;
                        if (scan->flags) 
                            SCAN_COMMIT(pRExC_state, &data_fake,minlenp);
                        data_fake.last_found=newSVsv(data->last_found);
                    }
                }
                else
                    data_fake.last_closep = &fake;
                data_fake.flags = 0;
		data_fake.pos_delta = delta;
                if (is_inf)
	            data_fake.flags |= SF_IS_INF;
                if ( flags & SCF_DO_STCLASS && !scan->flags
                     && OP(scan) == IFMATCH ) { /* Lookahead */
                    cl_init(pRExC_state, &intrnl);
                    data_fake.start_class = &intrnl;
                    f |= SCF_DO_STCLASS_AND;
                }
                if (flags & SCF_WHILEM_VISITED_POS)
                    f |= SCF_WHILEM_VISITED_POS;
                next = regnext(scan);
                nscan = NEXTOPER(NEXTOPER(scan));

                *minnextp = study_chunk(pRExC_state, &nscan, minnextp, &deltanext, 
                    last, &data_fake, stopparen, recursed, NULL, f,depth+1);
                if (scan->flags) {
                    if (deltanext) {
			FAIL("Variable length lookbehind not implemented");
                    }
                    else if (*minnextp > (I32)U8_MAX) {
			FAIL2("Lookbehind longer than %"UVuf" not implemented", (UV)U8_MAX);
                    }
                    scan->flags = (U8)*minnextp;
                }

                *minnextp += min;

                if (f & SCF_DO_STCLASS_AND) {
                    const int was = (data->start_class->flags & ANYOF_EOS);

                    cl_and(data->start_class, &intrnl);
                    if (was)
                        data->start_class->flags |= ANYOF_EOS;
                }
                if (data) {
                    if (data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
                        pars++;
                    if (data_fake.flags & SF_HAS_EVAL)
                        data->flags |= SF_HAS_EVAL;
                    data->whilem_c = data_fake.whilem_c;
                    if ((flags & SCF_DO_SUBSTR) && data_fake.last_found) {
                        if (RExC_rx->minlen<*minnextp)
                            RExC_rx->minlen=*minnextp;
                        SCAN_COMMIT(pRExC_state, &data_fake, minnextp);
                        SvREFCNT_dec(data_fake.last_found);
                        
                        if ( data_fake.minlen_fixed != minlenp ) 
                        {
                            data->offset_fixed= data_fake.offset_fixed;
                            data->minlen_fixed= data_fake.minlen_fixed;
                            data->lookbehind_fixed+= scan->flags;
                        }
                        if ( data_fake.minlen_float != minlenp )
                        {
                            data->minlen_float= data_fake.minlen_float;
                            data->offset_float_min=data_fake.offset_float_min;
                            data->offset_float_max=data_fake.offset_float_max;
                            data->lookbehind_float+= scan->flags;
                        }
                    }
                }


	    }
#endif
	}
	else if (OP(scan) == OPEN) {
	    if (stopparen != (I32)ARG(scan))
	        pars++;
	}
	else if (OP(scan) == CLOSE) {
	    if (stopparen == (I32)ARG(scan)) {
	        break;
	    }
	    if ((I32)ARG(scan) == is_par) {
		next = regnext(scan);

		if ( next && (OP(next) != WHILEM) && next < last)
		    is_par = 0;		/* Disable optimization */
	    }
	    if (data)
		*(data->last_closep) = ARG(scan);
	}
	else if (OP(scan) == EVAL) {
		if (data)
		    data->flags |= SF_HAS_EVAL;
	}
	else if ( PL_regkind[OP(scan)] == ENDLIKE ) {
	    if (flags & SCF_DO_SUBSTR) {
		SCAN_COMMIT(pRExC_state,data,minlenp);
		flags &= ~SCF_DO_SUBSTR;
	    }
	    if (data && OP(scan)==ACCEPT) {
	        data->flags |= SCF_SEEN_ACCEPT;
	        if (stopmin > min)
	            stopmin = min;
	    }
	}
	else if (OP(scan) == LOGICAL && scan->flags == 2) /* Embedded follows */
	{
		if (flags & SCF_DO_SUBSTR) {
		    SCAN_COMMIT(pRExC_state,data,minlenp);
		    data->longest = &(data->longest_float);
		}
		is_inf = is_inf_internal = 1;
		if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
		    cl_anything(pRExC_state, data->start_class);
		flags &= ~SCF_DO_STCLASS;
	}
	else if (OP(scan) == GPOS) {
	    if (!(RExC_rx->extflags & RXf_GPOS_FLOAT) &&
	        !(delta || is_inf || (data && data->pos_delta))) 
	    {
	        if (!(RExC_rx->extflags & RXf_ANCH) && (flags & SCF_DO_SUBSTR))
		    RExC_rx->extflags |= RXf_ANCH_GPOS;
	        if (RExC_rx->gofs < (U32)min)
		    RExC_rx->gofs = min;
            } else {
                RExC_rx->extflags |= RXf_GPOS_FLOAT;
                RExC_rx->gofs = 0;
            }	    
	}
#ifdef TRIE_STUDY_OPT
#ifdef FULL_TRIE_STUDY
        else if (PL_regkind[OP(scan)] == TRIE) {
            /* NOTE - There is similar code to this block above for handling
               BRANCH nodes on the initial study.  If you change stuff here
               check there too. */
            regnode *trie_node= scan;
            regnode *tail= regnext(scan);
            reg_trie_data *trie = (reg_trie_data*)RExC_rxi->data->data[ ARG(scan) ];
            I32 max1 = 0, min1 = I32_MAX;
            struct regnode_charclass_class accum;

            if (flags & SCF_DO_SUBSTR) /* XXXX Add !SUSPEND? */
                SCAN_COMMIT(pRExC_state, data,minlenp); /* Cannot merge strings after this. */
            if (flags & SCF_DO_STCLASS)
                cl_init_zero(pRExC_state, &accum);
                
            if (!trie->jump) {
                min1= trie->minlen;
                max1= trie->maxlen;
            } else {
                const regnode *nextbranch= NULL;
                U32 word;
                
                for ( word=1 ; word <= trie->wordcount ; word++) 
                {
                    I32 deltanext=0, minnext=0, f = 0, fake;
                    struct regnode_charclass_class this_class;
                    
                    data_fake.flags = 0;
                    if (data) {
                        data_fake.whilem_c = data->whilem_c;
                        data_fake.last_closep = data->last_closep;
                    }
                    else
                        data_fake.last_closep = &fake;
		    data_fake.pos_delta = delta;
                    if (flags & SCF_DO_STCLASS) {
                        cl_init(pRExC_state, &this_class);
                        data_fake.start_class = &this_class;
                        f = SCF_DO_STCLASS_AND;
                    }
                    if (flags & SCF_WHILEM_VISITED_POS)
                        f |= SCF_WHILEM_VISITED_POS;
    
                    if (trie->jump[word]) {
                        if (!nextbranch)
                            nextbranch = trie_node + trie->jump[0];
                        scan= trie_node + trie->jump[word];
                        /* We go from the jump point to the branch that follows
                           it. Note this means we need the vestigal unused branches
                           even though they arent otherwise used.
                         */
                        minnext = study_chunk(pRExC_state, &scan, minlenp, 
                            &deltanext, (regnode *)nextbranch, &data_fake, 
                            stopparen, recursed, NULL, f,depth+1);
                    }
                    if (nextbranch && PL_regkind[OP(nextbranch)]==BRANCH)
                        nextbranch= regnext((regnode*)nextbranch);
                    
                    if (min1 > (I32)(minnext + trie->minlen))
                        min1 = minnext + trie->minlen;
                    if (max1 < (I32)(minnext + deltanext + trie->maxlen))
                        max1 = minnext + deltanext + trie->maxlen;
                    if (deltanext == I32_MAX)
                        is_inf = is_inf_internal = 1;
                    
                    if (data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
                        pars++;
                    if (data_fake.flags & SCF_SEEN_ACCEPT) {
                        if ( stopmin > min + min1) 
	                    stopmin = min + min1;
	                flags &= ~SCF_DO_SUBSTR;
	                if (data)
	                    data->flags |= SCF_SEEN_ACCEPT;
	            }
                    if (data) {
                        if (data_fake.flags & SF_HAS_EVAL)
                            data->flags |= SF_HAS_EVAL;
                        data->whilem_c = data_fake.whilem_c;
                    }
                    if (flags & SCF_DO_STCLASS)
                        cl_or(pRExC_state, &accum, &this_class);
                }
            }
            if (flags & SCF_DO_SUBSTR) {
                data->pos_min += min1;
                data->pos_delta += max1 - min1;
                if (max1 != min1 || is_inf)
                    data->longest = &(data->longest_float);
            }
            min += min1;
            delta += max1 - min1;
            if (flags & SCF_DO_STCLASS_OR) {
                cl_or(pRExC_state, data->start_class, &accum);
                if (min1) {
                    cl_and(data->start_class, and_withp);
                    flags &= ~SCF_DO_STCLASS;
                }
            }
            else if (flags & SCF_DO_STCLASS_AND) {
                if (min1) {
                    cl_and(data->start_class, &accum);
                    flags &= ~SCF_DO_STCLASS;
                }
                else {
                    /* Switch to OR mode: cache the old value of
                     * data->start_class */
		    INIT_AND_WITHP;
                    StructCopy(data->start_class, and_withp,
                               struct regnode_charclass_class);
                    flags &= ~SCF_DO_STCLASS_AND;
                    StructCopy(&accum, data->start_class,
                               struct regnode_charclass_class);
                    flags |= SCF_DO_STCLASS_OR;
                    data->start_class->flags |= ANYOF_EOS;
                }
            }
            scan= tail;
            continue;
        }
#else
	else if (PL_regkind[OP(scan)] == TRIE) {
	    reg_trie_data *trie = (reg_trie_data*)RExC_rxi->data->data[ ARG(scan) ];
	    U8*bang=NULL;
	    
	    min += trie->minlen;
	    delta += (trie->maxlen - trie->minlen);
	    flags &= ~SCF_DO_STCLASS; /* xxx */
            if (flags & SCF_DO_SUBSTR) {
    	        SCAN_COMMIT(pRExC_state,data,minlenp);	/* Cannot expect anything... */
    	        data->pos_min += trie->minlen;
    	        data->pos_delta += (trie->maxlen - trie->minlen);
		if (trie->maxlen != trie->minlen)
		    data->longest = &(data->longest_float);
    	    }
    	    if (trie->jump) /* no more substrings -- for now /grr*/
    	        flags &= ~SCF_DO_SUBSTR; 
	}
#endif /* old or new */
#endif /* TRIE_STUDY_OPT */	

	/* Else: zero-length, ignore. */
	scan = regnext(scan);
    }
    if (frame) {
        last = frame->last;
        scan = frame->next;
        stopparen = frame->stop;
        frame = frame->prev;
        goto fake_study_recurse;
    }

  finish:
    assert(!frame);
    DEBUG_STUDYDATA("pre-fin:",data,depth);

    *scanp = scan;
    *deltap = is_inf_internal ? I32_MAX : delta;
    if (flags & SCF_DO_SUBSTR && is_inf)
	data->pos_delta = I32_MAX - data->pos_min;
    if (is_par > (I32)U8_MAX)
	is_par = 0;
    if (is_par && pars==1 && data) {
	data->flags |= SF_IN_PAR;
	data->flags &= ~SF_HAS_PAR;
    }
    else if (pars && data) {
	data->flags |= SF_HAS_PAR;
	data->flags &= ~SF_IN_PAR;
    }
    if (flags & SCF_DO_STCLASS_OR)
	cl_and(data->start_class, and_withp);
    if (flags & SCF_TRIE_RESTUDY)
        data->flags |= 	SCF_TRIE_RESTUDY;
    
    DEBUG_STUDYDATA("post-fin:",data,depth);
    
    return min < stopmin ? min : stopmin;
}

STATIC U32
S_add_data(RExC_state_t *pRExC_state, U32 n, const char *s)
{
    U32 count = RExC_rxi->data ? RExC_rxi->data->count : 0;

    PERL_ARGS_ASSERT_ADD_DATA;

    Renewc(RExC_rxi->data,
	   sizeof(*RExC_rxi->data) + sizeof(void*) * (count + n - 1),
	   char, struct reg_data);
    if(count)
	Renew(RExC_rxi->data->what, count + n, U8);
    else
	Newx(RExC_rxi->data->what, n, U8);
    RExC_rxi->data->count = count + n;
    Copy(s, RExC_rxi->data->what + count, n, U8);
    return count;
}

/*XXX: todo make this not included in a non debugging perl */
#ifndef PERL_IN_XSUB_RE
void
Perl_reginitcolors(pTHX)
{
    dVAR;
    const char * const s = PerlEnv_getenv("PERL_RE_COLORS");
    if (s) {
	char *t = savepv(s);
	int i = 0;
	PL_colors[0] = t;
	while (++i < 6) {
	    t = strchr(t, '\t');
	    if (t) {
		*t = '\0';
		PL_colors[i] = ++t;
	    }
	    else
		PL_colors[i] = t = (char *)"";
	}
    } else {
	int i = 0;
	while (i < 6)
	    PL_colors[i++] = (char *)"";
    }
    PL_colorset = 1;
}
#endif


#ifdef TRIE_STUDY_OPT
#define CHECK_RESTUDY_GOTO                                  \
        if (                                                \
              (data.flags & SCF_TRIE_RESTUDY)               \
              && ! restudied++                              \
        )     goto reStudy
#else
#define CHECK_RESTUDY_GOTO
#endif        

/*
 - pregcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.) [NB: not true in perl]
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.  [I'll say.]
 */



#ifndef PERL_IN_XSUB_RE
#define RE_ENGINE_PTR &PL_core_reg_engine
#else
extern const struct regexp_engine my_reg_engine;
#define RE_ENGINE_PTR &my_reg_engine
#endif

#ifndef PERL_IN_XSUB_RE 
REGEXP *
Perl_pregcomp(pTHX_ SV * const pattern, const U32 flags)
{
    dVAR;
    HV * const table = GvHV(PL_hintgv);

    PERL_ARGS_ASSERT_PREGCOMP;

    /* Dispatch a request to compile a regexp to correct 
       regexp engine. */
    if (table) {
        SV **ptr= hv_fetchs(table, "regcomp", FALSE);
        GET_RE_DEBUG_FLAGS_DECL;
        if (ptr && SvIOK(*ptr) && SvIV(*ptr)) {
            const regexp_engine *eng=INT2PTR(regexp_engine*,SvIV(*ptr));
            DEBUG_COMPILE_r({
                PerlIO_printf(Perl_debug_log, "Using engine %"UVxf"\n",
                    SvIV(*ptr));
            });            
            return CALLREGCOMP_ENG(eng, pattern, flags);
        } 
    }
    return Perl_re_compile(aTHX_ pattern, flags);
}
#endif

REGEXP *
Perl_re_compile(pTHX_ SV * const pattern, U32 pm_flags)
{
    dVAR;
    REGEXP *rx;
    struct regexp *r;
    register regexp_internal *ri;
    STRLEN plen;
    char  *exp = SvPV(pattern, plen);
    char* xend = exp + plen;
    regnode *scan;
    I32 flags;
    I32 minlen = 0;
    I32 sawplus = 0;
    I32 sawopen = 0;
    scan_data_t data;
    RExC_state_t RExC_state;
    RExC_state_t * const pRExC_state = &RExC_state;
#ifdef TRIE_STUDY_OPT    
    int restudied= 0;
    RExC_state_t copyRExC_state;
#endif    
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_COMPILE;

    DEBUG_r(if (!PL_colorset) reginitcolors());

    RExC_utf8 = RExC_orig_utf8 = SvUTF8(pattern);

    DEBUG_COMPILE_r({
        SV *dsv= sv_newmortal();
        RE_PV_QUOTED_DECL(s, RExC_utf8,
            dsv, exp, plen, 60);
        PerlIO_printf(Perl_debug_log, "%sCompiling REx%s %s\n",
		       PL_colors[4],PL_colors[5],s);
    });

redo_first_pass:
    RExC_precomp = exp;
    RExC_flags = pm_flags;
    RExC_sawback = 0;

    RExC_seen = 0;
    RExC_seen_zerolen = *exp == '^' ? -1 : 0;
    RExC_seen_evals = 0;
    RExC_extralen = 0;

    /* First pass: determine size, legality. */
    RExC_parse = exp;
    RExC_start = exp;
    RExC_end = xend;
    RExC_naughty = 0;
    RExC_npar = 1;
    RExC_nestroot = 0;
    RExC_size = 0L;
    RExC_emit = &PL_regdummy;
    RExC_whilem_seen = 0;
    RExC_open_parens = NULL;
    RExC_close_parens = NULL;
    RExC_opend = NULL;
    RExC_paren_names = NULL;
#ifdef DEBUGGING
    RExC_paren_name_list = NULL;
#endif
    RExC_recurse = NULL;
    RExC_recurse_count = 0;

#if 0 /* REGC() is (currently) a NOP at the first pass.
       * Clever compilers notice this and complain. --jhi */
    REGC((U8)REG_MAGIC, (char*)RExC_emit);
#endif
    DEBUG_PARSE_r(PerlIO_printf(Perl_debug_log, "Starting first pass (sizing)\n"));
    if (reg(pRExC_state, 0, &flags,1) == NULL) {
	RExC_precomp = NULL;
	return(NULL);
    }
    if (RExC_utf8 && !RExC_orig_utf8) {
        /* It's possible to write a regexp in ascii that represents Unicode
        codepoints outside of the byte range, such as via \x{100}. If we
        detect such a sequence we have to convert the entire pattern to utf8
        and then recompile, as our sizing calculation will have been based
        on 1 byte == 1 character, but we will need to use utf8 to encode
        at least some part of the pattern, and therefore must convert the whole
        thing.
        XXX: somehow figure out how to make this less expensive...
        -- dmq */
        STRLEN len = plen;
        DEBUG_PARSE_r(PerlIO_printf(Perl_debug_log,
	    "UTF8 mismatch! Converting to utf8 for resizing and compile\n"));
        exp = (char*)Perl_bytes_to_utf8(aTHX_ (U8*)exp, &len);
        xend = exp + len;
        RExC_orig_utf8 = RExC_utf8;
        SAVEFREEPV(exp);
        goto redo_first_pass;
    }
    DEBUG_PARSE_r({
        PerlIO_printf(Perl_debug_log, 
            "Required size %"IVdf" nodes\n"
            "Starting second pass (creation)\n", 
            (IV)RExC_size);
        RExC_lastnum=0; 
        RExC_lastparse=NULL; 
    });
    /* Small enough for pointer-storage convention?
       If extralen==0, this means that we will not need long jumps. */
    if (RExC_size >= 0x10000L && RExC_extralen)
        RExC_size += RExC_extralen;
    else
	RExC_extralen = 0;
    if (RExC_whilem_seen > 15)
	RExC_whilem_seen = 15;

    /* Allocate space and zero-initialize. Note, the two step process 
       of zeroing when in debug mode, thus anything assigned has to 
       happen after that */
    rx = (REGEXP*) newSV_type(SVt_REGEXP);
    r = (struct regexp*)SvANY(rx);
    Newxc(ri, sizeof(regexp_internal) + (unsigned)RExC_size * sizeof(regnode),
	 char, regexp_internal);
    if ( r == NULL || ri == NULL )
	FAIL("Regexp out of space");
#ifdef DEBUGGING
    /* avoid reading uninitialized memory in DEBUGGING code in study_chunk() */
    Zero(ri, sizeof(regexp_internal) + (unsigned)RExC_size * sizeof(regnode), char);
#else 
    /* bulk initialize base fields with 0. */
    Zero(ri, sizeof(regexp_internal), char);        
#endif

    /* non-zero initialization begins here */
    RXi_SET( r, ri );
    r->engine= RE_ENGINE_PTR;
    r->extflags = pm_flags;
    {
        bool has_p     = ((r->extflags & RXf_PMf_KEEPCOPY) == RXf_PMf_KEEPCOPY);
	bool has_minus = ((r->extflags & RXf_PMf_STD_PMMOD) != RXf_PMf_STD_PMMOD);
	bool has_runon = ((RExC_seen & REG_SEEN_RUN_ON_COMMENT)==REG_SEEN_RUN_ON_COMMENT);
	U16 reganch = (U16)((r->extflags & RXf_PMf_STD_PMMOD)
			    >> RXf_PMf_STD_PMMOD_SHIFT);
	const char *fptr = STD_PAT_MODS;        /*"msix"*/
	char *p;
	const STRLEN wraplen = plen + has_minus + has_p + has_runon
            + (sizeof(STD_PAT_MODS) - 1)
            + (sizeof("(?:)") - 1);

	p = sv_grow(MUTABLE_SV(rx), wraplen + 1);
	SvCUR_set(rx, wraplen);
	SvPOK_on(rx);
	SvFLAGS(rx) |= SvUTF8(pattern);
        *p++='('; *p++='?';
        if (has_p)
            *p++ = KEEPCOPY_PAT_MOD; /*'p'*/
        {
            char *r = p + (sizeof(STD_PAT_MODS) - 1) + has_minus - 1;
            char *colon = r + 1;
            char ch;

            while((ch = *fptr++)) {
                if(reganch & 1)
                    *p++ = ch;
                else
                    *r-- = ch;
                reganch >>= 1;
            }
            if(has_minus) {
                *r = '-';
                p = colon;
            }
        }

        *p++ = ':';
        Copy(RExC_precomp, p, plen, char);
	assert ((RX_WRAPPED(rx) - p) < 16);
	r->pre_prefix = p - RX_WRAPPED(rx);
        p += plen;
        if (has_runon)
            *p++ = '\n';
        *p++ = ')';
        *p = 0;
    }

    r->intflags = 0;
    r->nparens = RExC_npar - 1;	/* set early to validate backrefs */
    
    if (RExC_seen & REG_SEEN_RECURSE) {
        Newxz(RExC_open_parens, RExC_npar,regnode *);
        SAVEFREEPV(RExC_open_parens);
        Newxz(RExC_close_parens,RExC_npar,regnode *);
        SAVEFREEPV(RExC_close_parens);
    }

    /* Useful during FAIL. */
#ifdef RE_TRACK_PATTERN_OFFSETS
    Newxz(ri->u.offsets, 2*RExC_size+1, U32); /* MJD 20001228 */
    DEBUG_OFFSETS_r(PerlIO_printf(Perl_debug_log,
                          "%s %"UVuf" bytes for offset annotations.\n",
                          ri->u.offsets ? "Got" : "Couldn't get",
                          (UV)((2*RExC_size+1) * sizeof(U32))));
#endif
    SetProgLen(ri,RExC_size);
    RExC_rx_sv = rx;
    RExC_rx = r;
    RExC_rxi = ri;

    /* Second pass: emit code. */
    RExC_flags = pm_flags;	/* don't let top level (?i) bleed */
    RExC_parse = exp;
    RExC_end = xend;
    RExC_naughty = 0;
    RExC_npar = 1;
    RExC_emit_start = ri->program;
    RExC_emit = ri->program;
    RExC_emit_bound = ri->program + RExC_size + 1;

    /* Store the count of eval-groups for security checks: */
    RExC_rx->seen_evals = RExC_seen_evals;
    REGC((U8)REG_MAGIC, (char*) RExC_emit++);
    if (reg(pRExC_state, 0, &flags,1) == NULL) {
	ReREFCNT_dec(rx);   
	return(NULL);
    }
    /* XXXX To minimize changes to RE engine we always allocate
       3-units-long substrs field. */
    Newx(r->substrs, 1, struct reg_substr_data);
    if (RExC_recurse_count) {
        Newxz(RExC_recurse,RExC_recurse_count,regnode *);
        SAVEFREEPV(RExC_recurse);
    }

reStudy:
    r->minlen = minlen = sawplus = sawopen = 0;
    Zero(r->substrs, 1, struct reg_substr_data);

#ifdef TRIE_STUDY_OPT
    if (!restudied) {
        StructCopy(&zero_scan_data, &data, scan_data_t);
        copyRExC_state = RExC_state;
    } else {
        U32 seen=RExC_seen;
        DEBUG_OPTIMISE_r(PerlIO_printf(Perl_debug_log,"Restudying\n"));
        
        RExC_state = copyRExC_state;
        if (seen & REG_TOP_LEVEL_BRANCHES) 
            RExC_seen |= REG_TOP_LEVEL_BRANCHES;
        else
            RExC_seen &= ~REG_TOP_LEVEL_BRANCHES;
        if (data.last_found) {
            SvREFCNT_dec(data.longest_fixed);
	    SvREFCNT_dec(data.longest_float);
	    SvREFCNT_dec(data.last_found);
	}
	StructCopy(&zero_scan_data, &data, scan_data_t);
    }
#else
    StructCopy(&zero_scan_data, &data, scan_data_t);
#endif    

    /* Dig out information for optimizations. */
    r->extflags = RExC_flags; /* was pm_op */
    /*dmq: removed as part of de-PMOP: pm->op_pmflags = RExC_flags; */
 
    if (UTF)
	SvUTF8_on(rx);	/* Unicode in it? */
    ri->regstclass = NULL;
    if (RExC_naughty >= 10)	/* Probably an expensive pattern. */
	r->intflags |= PREGf_NAUGHTY;
    scan = ri->program + 1;		/* First BRANCH. */

    /* testing for BRANCH here tells us whether there is "must appear"
       data in the pattern. If there is then we can use it for optimisations */
    if (!(RExC_seen & REG_TOP_LEVEL_BRANCHES)) { /*  Only one top-level choice. */
	I32 fake;
	STRLEN longest_float_length, longest_fixed_length;
	struct regnode_charclass_class ch_class; /* pointed to by data */
	int stclass_flag;
	I32 last_close = 0; /* pointed to by data */
        regnode *first= scan;
        regnode *first_next= regnext(first);
	
	/*
	 * Skip introductions and multiplicators >= 1
	 * so that we can extract the 'meat' of the pattern that must 
	 * match in the large if() sequence following.
	 * NOTE that EXACT is NOT covered here, as it is normally
	 * picked up by the optimiser separately. 
	 *
	 * This is unfortunate as the optimiser isnt handling lookahead
	 * properly currently.
	 *
	 */
	while ((OP(first) == OPEN && (sawopen = 1)) ||
	       /* An OR of *one* alternative - should not happen now. */
	    (OP(first) == BRANCH && OP(first_next) != BRANCH) ||
	    /* for now we can't handle lookbehind IFMATCH*/
	    (OP(first) == IFMATCH && !first->flags) || 
	    (OP(first) == PLUS) ||
	    (OP(first) == MINMOD) ||
	       /* An {n,m} with n>0 */
	    (PL_regkind[OP(first)] == CURLY && ARG1(first) > 0) ||
	    (OP(first) == NOTHING && PL_regkind[OP(first_next)] != END ))
	{
		/* 
		 * the only op that could be a regnode is PLUS, all the rest
		 * will be regnode_1 or regnode_2.
		 *
		 */
		if (OP(first) == PLUS)
		    sawplus = 1;
		else
		    first += regarglen[OP(first)];
		
		first = NEXTOPER(first);
		first_next= regnext(first);
	}

	/* Starting-point info. */
      again:
        DEBUG_PEEP("first:",first,0);
        /* Ignore EXACT as we deal with it later. */
	if (PL_regkind[OP(first)] == EXACT) {
	    if (OP(first) == EXACT)
		NOOP;	/* Empty, get anchored substr later. */
	    else if ((OP(first) == EXACTF || OP(first) == EXACTFL))
		ri->regstclass = first;
	}
#ifdef TRIE_STCLASS	
	else if (PL_regkind[OP(first)] == TRIE &&
	        ((reg_trie_data *)ri->data->data[ ARG(first) ])->minlen>0) 
	{
	    regnode *trie_op;
	    /* this can happen only on restudy */
	    if ( OP(first) == TRIE ) {
                struct regnode_1 *trieop = (struct regnode_1 *)
		    PerlMemShared_calloc(1, sizeof(struct regnode_1));
                StructCopy(first,trieop,struct regnode_1);
                trie_op=(regnode *)trieop;
            } else {
                struct regnode_charclass *trieop = (struct regnode_charclass *)
		    PerlMemShared_calloc(1, sizeof(struct regnode_charclass));
                StructCopy(first,trieop,struct regnode_charclass);
                trie_op=(regnode *)trieop;
            }
            OP(trie_op)+=2;
            make_trie_failtable(pRExC_state, (regnode *)first, trie_op, 0);
	    ri->regstclass = trie_op;
	}
#endif	
	else if (strchr((const char*)PL_simple,OP(first)))
	    ri->regstclass = first;
	else if (PL_regkind[OP(first)] == BOUND ||
		 PL_regkind[OP(first)] == NBOUND)
	    ri->regstclass = first;
	else if (PL_regkind[OP(first)] == BOL) {
	    r->extflags |= (OP(first) == MBOL
			   ? RXf_ANCH_MBOL
			   : (OP(first) == SBOL
			      ? RXf_ANCH_SBOL
			      : RXf_ANCH_BOL));
	    first = NEXTOPER(first);
	    goto again;
	}
	else if (OP(first) == GPOS) {
	    r->extflags |= RXf_ANCH_GPOS;
	    first = NEXTOPER(first);
	    goto again;
	}
	else if ((!sawopen || !RExC_sawback) &&
	    (OP(first) == STAR &&
	    PL_regkind[OP(NEXTOPER(first))] == REG_ANY) &&
	    !(r->extflags & RXf_ANCH) && !(RExC_seen & REG_SEEN_EVAL))
	{
	    /* turn .* into ^.* with an implied $*=1 */
	    const int type =
		(OP(NEXTOPER(first)) == REG_ANY)
		    ? RXf_ANCH_MBOL
		    : RXf_ANCH_SBOL;
	    r->extflags |= type;
	    r->intflags |= PREGf_IMPLICIT;
	    first = NEXTOPER(first);
	    goto again;
	}
	if (sawplus && (!sawopen || !RExC_sawback)
	    && !(RExC_seen & REG_SEEN_EVAL)) /* May examine pos and $& */
	    /* x+ must match at the 1st pos of run of x's */
	    r->intflags |= PREGf_SKIP;

	/* Scan is after the zeroth branch, first is atomic matcher. */
#ifdef TRIE_STUDY_OPT
	DEBUG_PARSE_r(
	    if (!restudied)
	        PerlIO_printf(Perl_debug_log, "first at %"IVdf"\n",
			      (IV)(first - scan + 1))
        );
#else
	DEBUG_PARSE_r(
	    PerlIO_printf(Perl_debug_log, "first at %"IVdf"\n",
	        (IV)(first - scan + 1))
        );
#endif


	/*
	* If there's something expensive in the r.e., find the
	* longest literal string that must appear and make it the
	* regmust.  Resolve ties in favor of later strings, since
	* the regstart check works with the beginning of the r.e.
	* and avoiding duplication strengthens checking.  Not a
	* strong reason, but sufficient in the absence of others.
	* [Now we resolve ties in favor of the earlier string if
	* it happens that c_offset_min has been invalidated, since the
	* earlier string may buy us something the later one won't.]
	*/
	
	data.longest_fixed = newSVpvs("");
	data.longest_float = newSVpvs("");
	data.last_found = newSVpvs("");
	data.longest = &(data.longest_fixed);
	first = scan;
	if (!ri->regstclass) {
	    cl_init(pRExC_state, &ch_class);
	    data.start_class = &ch_class;
	    stclass_flag = SCF_DO_STCLASS_AND;
	} else				/* XXXX Check for BOUND? */
	    stclass_flag = 0;
	data.last_closep = &last_close;
        
	minlen = study_chunk(pRExC_state, &first, &minlen, &fake, scan + RExC_size, /* Up to end */
            &data, -1, NULL, NULL,
            SCF_DO_SUBSTR | SCF_WHILEM_VISITED_POS | stclass_flag,0);

	
        CHECK_RESTUDY_GOTO;


	if ( RExC_npar == 1 && data.longest == &(data.longest_fixed)
	     && data.last_start_min == 0 && data.last_end > 0
	     && !RExC_seen_zerolen
	     && !(RExC_seen & REG_SEEN_VERBARG)
	     && (!(RExC_seen & REG_SEEN_GPOS) || (r->extflags & RXf_ANCH_GPOS)))
	    r->extflags |= RXf_CHECK_ALL;
	scan_commit(pRExC_state, &data,&minlen,0);
	SvREFCNT_dec(data.last_found);

        /* Note that code very similar to this but for anchored string 
           follows immediately below, changes may need to be made to both. 
           Be careful. 
         */
	longest_float_length = CHR_SVLEN(data.longest_float);
	if (longest_float_length
	    || (data.flags & SF_FL_BEFORE_EOL
		&& (!(data.flags & SF_FL_BEFORE_MEOL)
		    || (RExC_flags & RXf_PMf_MULTILINE)))) 
        {
            I32 t,ml;

	    if (SvCUR(data.longest_fixed)  /* ok to leave SvCUR */
		&& data.offset_fixed == data.offset_float_min
		&& SvCUR(data.longest_fixed) == SvCUR(data.longest_float))
		    goto remove_float;		/* As in (a)+. */

            /* copy the information about the longest float from the reg_scan_data
               over to the program. */
	    if (SvUTF8(data.longest_float)) {
		r->float_utf8 = data.longest_float;
		r->float_substr = NULL;
	    } else {
		r->float_substr = data.longest_float;
		r->float_utf8 = NULL;
	    }
	    /* float_end_shift is how many chars that must be matched that 
	       follow this item. We calculate it ahead of time as once the
	       lookbehind offset is added in we lose the ability to correctly
	       calculate it.*/
	    ml = data.minlen_float ? *(data.minlen_float) 
	                           : (I32)longest_float_length;
	    r->float_end_shift = ml - data.offset_float_min
	        - longest_float_length + (SvTAIL(data.longest_float) != 0)
	        + data.lookbehind_float;
	    r->float_min_offset = data.offset_float_min - data.lookbehind_float;
	    r->float_max_offset = data.offset_float_max;
	    if (data.offset_float_max < I32_MAX) /* Don't offset infinity */
	        r->float_max_offset -= data.lookbehind_float;
	    
	    t = (data.flags & SF_FL_BEFORE_EOL /* Can't have SEOL and MULTI */
		       && (!(data.flags & SF_FL_BEFORE_MEOL)
			   || (RExC_flags & RXf_PMf_MULTILINE)));
	    fbm_compile(data.longest_float, t ? FBMcf_TAIL : 0);
	}
	else {
	  remove_float:
	    r->float_substr = r->float_utf8 = NULL;
	    SvREFCNT_dec(data.longest_float);
	    longest_float_length = 0;
	}

        /* Note that code very similar to this but for floating string 
           is immediately above, changes may need to be made to both. 
           Be careful. 
         */
	longest_fixed_length = CHR_SVLEN(data.longest_fixed);
	if (longest_fixed_length
	    || (data.flags & SF_FIX_BEFORE_EOL /* Cannot have SEOL and MULTI */
		&& (!(data.flags & SF_FIX_BEFORE_MEOL)
		    || (RExC_flags & RXf_PMf_MULTILINE)))) 
        {
            I32 t,ml;

            /* copy the information about the longest fixed 
               from the reg_scan_data over to the program. */
	    if (SvUTF8(data.longest_fixed)) {
		r->anchored_utf8 = data.longest_fixed;
		r->anchored_substr = NULL;
	    } else {
		r->anchored_substr = data.longest_fixed;
		r->anchored_utf8 = NULL;
	    }
	    /* fixed_end_shift is how many chars that must be matched that 
	       follow this item. We calculate it ahead of time as once the
	       lookbehind offset is added in we lose the ability to correctly
	       calculate it.*/
            ml = data.minlen_fixed ? *(data.minlen_fixed) 
                                   : (I32)longest_fixed_length;
            r->anchored_end_shift = ml - data.offset_fixed
	        - longest_fixed_length + (SvTAIL(data.longest_fixed) != 0)
	        + data.lookbehind_fixed;
	    r->anchored_offset = data.offset_fixed - data.lookbehind_fixed;

	    t = (data.flags & SF_FIX_BEFORE_EOL /* Can't have SEOL and MULTI */
		 && (!(data.flags & SF_FIX_BEFORE_MEOL)
		     || (RExC_flags & RXf_PMf_MULTILINE)));
	    fbm_compile(data.longest_fixed, t ? FBMcf_TAIL : 0);
	}
	else {
	    r->anchored_substr = r->anchored_utf8 = NULL;
	    SvREFCNT_dec(data.longest_fixed);
	    longest_fixed_length = 0;
	}
	if (ri->regstclass
	    && (OP(ri->regstclass) == REG_ANY || OP(ri->regstclass) == SANY))
	    ri->regstclass = NULL;
	if ((!(r->anchored_substr || r->anchored_utf8) || r->anchored_offset)
	    && stclass_flag
	    && !(data.start_class->flags & ANYOF_EOS)
	    && !cl_is_anything(data.start_class))
	{
	    const U32 n = add_data(pRExC_state, 1, "f");

	    Newx(RExC_rxi->data->data[n], 1,
		struct regnode_charclass_class);
	    StructCopy(data.start_class,
		       (struct regnode_charclass_class*)RExC_rxi->data->data[n],
		       struct regnode_charclass_class);
	    ri->regstclass = (regnode*)RExC_rxi->data->data[n];
	    r->intflags &= ~PREGf_SKIP;	/* Used in find_byclass(). */
	    DEBUG_COMPILE_r({ SV *sv = sv_newmortal();
	              regprop(r, sv, (regnode*)data.start_class);
		      PerlIO_printf(Perl_debug_log,
				    "synthetic stclass \"%s\".\n",
				    SvPVX_const(sv));});
	}

	/* A temporary algorithm prefers floated substr to fixed one to dig more info. */
	if (longest_fixed_length > longest_float_length) {
	    r->check_end_shift = r->anchored_end_shift;
	    r->check_substr = r->anchored_substr;
	    r->check_utf8 = r->anchored_utf8;
	    r->check_offset_min = r->check_offset_max = r->anchored_offset;
	    if (r->extflags & RXf_ANCH_SINGLE)
		r->extflags |= RXf_NOSCAN;
	}
	else {
	    r->check_end_shift = r->float_end_shift;
	    r->check_substr = r->float_substr;
	    r->check_utf8 = r->float_utf8;
	    r->check_offset_min = r->float_min_offset;
	    r->check_offset_max = r->float_max_offset;
	}
	/* XXXX Currently intuiting is not compatible with ANCH_GPOS.
	   This should be changed ASAP!  */
	if ((r->check_substr || r->check_utf8) && !(r->extflags & RXf_ANCH_GPOS)) {
	    r->extflags |= RXf_USE_INTUIT;
	    if (SvTAIL(r->check_substr ? r->check_substr : r->check_utf8))
		r->extflags |= RXf_INTUIT_TAIL;
	}
	/* XXX Unneeded? dmq (shouldn't as this is handled elsewhere)
	if ( (STRLEN)minlen < longest_float_length )
            minlen= longest_float_length;
        if ( (STRLEN)minlen < longest_fixed_length )
            minlen= longest_fixed_length;     
        */
    }
    else {
	/* Several toplevels. Best we can is to set minlen. */
	I32 fake;
	struct regnode_charclass_class ch_class;
	I32 last_close = 0;
	
	DEBUG_PARSE_r(PerlIO_printf(Perl_debug_log, "\nMulti Top Level\n"));

	scan = ri->program + 1;
	cl_init(pRExC_state, &ch_class);
	data.start_class = &ch_class;
	data.last_closep = &last_close;

        
	minlen = study_chunk(pRExC_state, &scan, &minlen, &fake, scan + RExC_size,
	    &data, -1, NULL, NULL, SCF_DO_STCLASS_AND|SCF_WHILEM_VISITED_POS,0);
        
        CHECK_RESTUDY_GOTO;

	r->check_substr = r->check_utf8 = r->anchored_substr = r->anchored_utf8
		= r->float_substr = r->float_utf8 = NULL;
	if (!(data.start_class->flags & ANYOF_EOS)
	    && !cl_is_anything(data.start_class))
	{
	    const U32 n = add_data(pRExC_state, 1, "f");

	    Newx(RExC_rxi->data->data[n], 1,
		struct regnode_charclass_class);
	    StructCopy(data.start_class,
		       (struct regnode_charclass_class*)RExC_rxi->data->data[n],
		       struct regnode_charclass_class);
	    ri->regstclass = (regnode*)RExC_rxi->data->data[n];
	    r->intflags &= ~PREGf_SKIP;	/* Used in find_byclass(). */
	    DEBUG_COMPILE_r({ SV* sv = sv_newmortal();
	              regprop(r, sv, (regnode*)data.start_class);
		      PerlIO_printf(Perl_debug_log,
				    "synthetic stclass \"%s\".\n",
				    SvPVX_const(sv));});
	}
    }

    /* Guard against an embedded (?=) or (?<=) with a longer minlen than
       the "real" pattern. */
    DEBUG_OPTIMISE_r({
	PerlIO_printf(Perl_debug_log,"minlen: %"IVdf" r->minlen:%"IVdf"\n",
		      (IV)minlen, (IV)r->minlen);
    });
    r->minlenret = minlen;
    if (r->minlen < minlen) 
        r->minlen = minlen;
    
    if (RExC_seen & REG_SEEN_GPOS)
	r->extflags |= RXf_GPOS_SEEN;
    if (RExC_seen & REG_SEEN_LOOKBEHIND)
	r->extflags |= RXf_LOOKBEHIND_SEEN;
    if (RExC_seen & REG_SEEN_EVAL)
	r->extflags |= RXf_EVAL_SEEN;
    if (RExC_seen & REG_SEEN_CANY)
	r->extflags |= RXf_CANY_SEEN;
    if (RExC_seen & REG_SEEN_VERBARG)
	r->intflags |= PREGf_VERBARG_SEEN;
    if (RExC_seen & REG_SEEN_CUTGROUP)
	r->intflags |= PREGf_CUTGROUP_SEEN;
    if (RExC_paren_names)
        RXp_PAREN_NAMES(r) = MUTABLE_HV(SvREFCNT_inc(RExC_paren_names));
    else
        RXp_PAREN_NAMES(r) = NULL;

#ifdef STUPID_PATTERN_CHECKS            
    if (RX_PRELEN(rx) == 0)
        r->extflags |= RXf_NULL;
    if (r->extflags & RXf_SPLIT && RX_PRELEN(rx) == 1 && RX_PRECOMP(rx)[0] == ' ')
        /* XXX: this should happen BEFORE we compile */
        r->extflags |= (RXf_SKIPWHITE|RXf_WHITE); 
    else if (RX_PRELEN(rx) == 3 && memEQ("\\s+", RX_PRECOMP(rx), 3))
        r->extflags |= RXf_WHITE;
    else if (RX_PRELEN(rx) == 1 && RXp_PRECOMP(rx)[0] == '^')
        r->extflags |= RXf_START_ONLY;
#else
    if (r->extflags & RXf_SPLIT && RX_PRELEN(rx) == 1 && RX_PRECOMP(rx)[0] == ' ')
            /* XXX: this should happen BEFORE we compile */
            r->extflags |= (RXf_SKIPWHITE|RXf_WHITE); 
    else {
        regnode *first = ri->program + 1;
        U8 fop = OP(first);
        U8 nop = OP(NEXTOPER(first));
        
        if (PL_regkind[fop] == NOTHING && nop == END)
            r->extflags |= RXf_NULL;
        else if (PL_regkind[fop] == BOL && nop == END)
            r->extflags |= RXf_START_ONLY;
        else if (fop == PLUS && nop ==SPACE && OP(regnext(first))==END)
            r->extflags |= RXf_WHITE;    
    }
#endif
#ifdef DEBUGGING
    if (RExC_paren_names) {
        ri->name_list_idx = add_data( pRExC_state, 1, "p" );
        ri->data->data[ri->name_list_idx] = (void*)SvREFCNT_inc(RExC_paren_name_list);
    } else
#endif
        ri->name_list_idx = 0;

    if (RExC_recurse_count) {
        for ( ; RExC_recurse_count ; RExC_recurse_count-- ) {
            const regnode *scan = RExC_recurse[RExC_recurse_count-1];
            ARG2L_SET( scan, RExC_open_parens[ARG(scan)-1] - scan );
        }
    }
    Newxz(r->offs, RExC_npar, regexp_paren_pair);
    /* assume we don't need to swap parens around before we match */

    DEBUG_DUMP_r({
        PerlIO_printf(Perl_debug_log,"Final program:\n");
        regdump(r);
    });
#ifdef RE_TRACK_PATTERN_OFFSETS
    DEBUG_OFFSETS_r(if (ri->u.offsets) {
        const U32 len = ri->u.offsets[0];
        U32 i;
        GET_RE_DEBUG_FLAGS_DECL;
        PerlIO_printf(Perl_debug_log, "Offsets: [%"UVuf"]\n\t", (UV)ri->u.offsets[0]);
        for (i = 1; i <= len; i++) {
            if (ri->u.offsets[i*2-1] || ri->u.offsets[i*2])
                PerlIO_printf(Perl_debug_log, "%"UVuf":%"UVuf"[%"UVuf"] ",
                (UV)i, (UV)ri->u.offsets[i*2-1], (UV)ri->u.offsets[i*2]);
            }
        PerlIO_printf(Perl_debug_log, "\n");
    });
#endif
    return rx;
}

#undef RE_ENGINE_PTR


SV*
Perl_reg_named_buff(pTHX_ REGEXP * const rx, SV * const key, SV * const value,
                    const U32 flags)
{
    PERL_ARGS_ASSERT_REG_NAMED_BUFF;

    PERL_UNUSED_ARG(value);

    if (flags & RXapif_FETCH) {
        return reg_named_buff_fetch(rx, key, flags);
    } else if (flags & (RXapif_STORE | RXapif_DELETE | RXapif_CLEAR)) {
        Perl_croak(aTHX_ "%s", PL_no_modify);
        return NULL;
    } else if (flags & RXapif_EXISTS) {
        return reg_named_buff_exists(rx, key, flags)
            ? &PL_sv_yes
            : &PL_sv_no;
    } else if (flags & RXapif_REGNAMES) {
        return reg_named_buff_all(rx, flags);
    } else if (flags & (RXapif_SCALAR | RXapif_REGNAMES_COUNT)) {
        return reg_named_buff_scalar(rx, flags);
    } else {
        Perl_croak(aTHX_ "panic: Unknown flags %d in named_buff", (int)flags);
        return NULL;
    }
}

SV*
Perl_reg_named_buff_iter(pTHX_ REGEXP * const rx, const SV * const lastkey,
                         const U32 flags)
{
    PERL_ARGS_ASSERT_REG_NAMED_BUFF_ITER;
    PERL_UNUSED_ARG(lastkey);

    if (flags & RXapif_FIRSTKEY)
        return reg_named_buff_firstkey(rx, flags);
    else if (flags & RXapif_NEXTKEY)
        return reg_named_buff_nextkey(rx, flags);
    else {
        Perl_croak(aTHX_ "panic: Unknown flags %d in named_buff_iter", (int)flags);
        return NULL;
    }
}

SV*
Perl_reg_named_buff_fetch(pTHX_ REGEXP * const r, SV * const namesv,
			  const U32 flags)
{
    AV *retarray = NULL;
    SV *ret;
    struct regexp *const rx = (struct regexp *)SvANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_FETCH;

    if (flags & RXapif_ALL)
        retarray=newAV();

    if (rx && RXp_PAREN_NAMES(rx)) {
        HE *he_str = hv_fetch_ent( RXp_PAREN_NAMES(rx), namesv, 0, 0 );
        if (he_str) {
            IV i;
            SV* sv_dat=HeVAL(he_str);
            I32 *nums=(I32*)SvPVX(sv_dat);
            for ( i=0; i<SvIVX(sv_dat); i++ ) {
                if ((I32)(rx->nparens) >= nums[i]
                    && rx->offs[nums[i]].start != -1
                    && rx->offs[nums[i]].end != -1)
                {
                    ret = newSVpvs("");
                    CALLREG_NUMBUF_FETCH(r,nums[i],ret);
                    if (!retarray)
                        return ret;
                } else {
                    ret = newSVsv(&PL_sv_undef);
                }
                if (retarray)
                    av_push(retarray, ret);
            }
            if (retarray)
                return newRV_noinc(MUTABLE_SV(retarray));
        }
    }
    return NULL;
}

bool
Perl_reg_named_buff_exists(pTHX_ REGEXP * const r, SV * const key,
                           const U32 flags)
{
    struct regexp *const rx = (struct regexp *)SvANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_EXISTS;

    if (rx && RXp_PAREN_NAMES(rx)) {
        if (flags & RXapif_ALL) {
            return hv_exists_ent(RXp_PAREN_NAMES(rx), key, 0);
        } else {
	    SV *sv = CALLREG_NAMED_BUFF_FETCH(r, key, flags);
            if (sv) {
		SvREFCNT_dec(sv);
                return TRUE;
            } else {
                return FALSE;
            }
        }
    } else {
        return FALSE;
    }
}

SV*
Perl_reg_named_buff_firstkey(pTHX_ REGEXP * const r, const U32 flags)
{
    struct regexp *const rx = (struct regexp *)SvANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_FIRSTKEY;

    if ( rx && RXp_PAREN_NAMES(rx) ) {
	(void)hv_iterinit(RXp_PAREN_NAMES(rx));

	return CALLREG_NAMED_BUFF_NEXTKEY(r, NULL, flags & ~RXapif_FIRSTKEY);
    } else {
	return FALSE;
    }
}

SV*
Perl_reg_named_buff_nextkey(pTHX_ REGEXP * const r, const U32 flags)
{
    struct regexp *const rx = (struct regexp *)SvANY(r);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_NEXTKEY;

    if (rx && RXp_PAREN_NAMES(rx)) {
        HV *hv = RXp_PAREN_NAMES(rx);
        HE *temphe;
        while ( (temphe = hv_iternext_flags(hv,0)) ) {
            IV i;
            IV parno = 0;
            SV* sv_dat = HeVAL(temphe);
            I32 *nums = (I32*)SvPVX(sv_dat);
            for ( i = 0; i < SvIVX(sv_dat); i++ ) {
                if ((I32)(rx->lastparen) >= nums[i] &&
                    rx->offs[nums[i]].start != -1 &&
                    rx->offs[nums[i]].end != -1)
                {
                    parno = nums[i];
                    break;
                }
            }
            if (parno || flags & RXapif_ALL) {
		return newSVhek(HeKEY_hek(temphe));
            }
        }
    }
    return NULL;
}

SV*
Perl_reg_named_buff_scalar(pTHX_ REGEXP * const r, const U32 flags)
{
    SV *ret;
    AV *av;
    I32 length;
    struct regexp *const rx = (struct regexp *)SvANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_SCALAR;

    if (rx && RXp_PAREN_NAMES(rx)) {
        if (flags & (RXapif_ALL | RXapif_REGNAMES_COUNT)) {
            return newSViv(HvTOTALKEYS(RXp_PAREN_NAMES(rx)));
        } else if (flags & RXapif_ONE) {
            ret = CALLREG_NAMED_BUFF_ALL(r, (flags | RXapif_REGNAMES));
            av = MUTABLE_AV(SvRV(ret));
            length = av_len(av);
	    SvREFCNT_dec(ret);
            return newSViv(length + 1);
        } else {
            Perl_croak(aTHX_ "panic: Unknown flags %d in named_buff_scalar", (int)flags);
            return NULL;
        }
    }
    return &PL_sv_undef;
}

SV*
Perl_reg_named_buff_all(pTHX_ REGEXP * const r, const U32 flags)
{
    struct regexp *const rx = (struct regexp *)SvANY(r);
    AV *av = newAV();

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_ALL;

    if (rx && RXp_PAREN_NAMES(rx)) {
        HV *hv= RXp_PAREN_NAMES(rx);
        HE *temphe;
        (void)hv_iterinit(hv);
        while ( (temphe = hv_iternext_flags(hv,0)) ) {
            IV i;
            IV parno = 0;
            SV* sv_dat = HeVAL(temphe);
            I32 *nums = (I32*)SvPVX(sv_dat);
            for ( i = 0; i < SvIVX(sv_dat); i++ ) {
                if ((I32)(rx->lastparen) >= nums[i] &&
                    rx->offs[nums[i]].start != -1 &&
                    rx->offs[nums[i]].end != -1)
                {
                    parno = nums[i];
                    break;
                }
            }
            if (parno || flags & RXapif_ALL) {
                av_push(av, newSVhek(HeKEY_hek(temphe)));
            }
        }
    }

    return newRV_noinc(MUTABLE_SV(av));
}

void
Perl_reg_numbered_buff_fetch(pTHX_ REGEXP * const r, const I32 paren,
			     SV * const sv)
{
    struct regexp *const rx = (struct regexp *)SvANY(r);
    char *s = NULL;
    I32 i = 0;
    I32 s1, t1;

    PERL_ARGS_ASSERT_REG_NUMBERED_BUFF_FETCH;
        
    if (!rx->subbeg) {
        sv_setsv(sv,&PL_sv_undef);
        return;
    } 
    else               
    if (paren == RX_BUFF_IDX_PREMATCH && rx->offs[0].start != -1) {
        /* $` */
	i = rx->offs[0].start;
	s = rx->subbeg;
    }
    else 
    if (paren == RX_BUFF_IDX_POSTMATCH && rx->offs[0].end != -1) {
        /* $' */
	s = rx->subbeg + rx->offs[0].end;
	i = rx->sublen - rx->offs[0].end;
    } 
    else
    if ( 0 <= paren && paren <= (I32)rx->nparens &&
        (s1 = rx->offs[paren].start) != -1 &&
        (t1 = rx->offs[paren].end) != -1)
    {
        /* $& $1 ... */
        i = t1 - s1;
        s = rx->subbeg + s1;
    } else {
        sv_setsv(sv,&PL_sv_undef);
        return;
    }          
    assert(rx->sublen >= (s - rx->subbeg) + i );
    if (i >= 0) {
        const int oldtainted = PL_tainted;
        TAINT_NOT;
        sv_setpvn(sv, s, i);
        PL_tainted = oldtainted;
        if ( (rx->extflags & RXf_CANY_SEEN)
            ? (RXp_MATCH_UTF8(rx)
                        && (!i || is_utf8_string((U8*)s, i)))
            : (RXp_MATCH_UTF8(rx)) )
        {
            SvUTF8_on(sv);
        }
        else
            SvUTF8_off(sv);
        if (PL_tainting) {
            if (RXp_MATCH_TAINTED(rx)) {
                if (SvTYPE(sv) >= SVt_PVMG) {
                    MAGIC* const mg = SvMAGIC(sv);
                    MAGIC* mgt;
                    PL_tainted = 1;
                    SvMAGIC_set(sv, mg->mg_moremagic);
                    SvTAINT(sv);
                    if ((mgt = SvMAGIC(sv))) {
                        mg->mg_moremagic = mgt;
                        SvMAGIC_set(sv, mg);
                    }
                } else {
                    PL_tainted = 1;
                    SvTAINT(sv);
                }
            } else 
                SvTAINTED_off(sv);
        }
    } else {
        sv_setsv(sv,&PL_sv_undef);
        return;
    }
}

void
Perl_reg_numbered_buff_store(pTHX_ REGEXP * const rx, const I32 paren,
							 SV const * const value)
{
    PERL_ARGS_ASSERT_REG_NUMBERED_BUFF_STORE;

    PERL_UNUSED_ARG(rx);
    PERL_UNUSED_ARG(paren);
    PERL_UNUSED_ARG(value);

    if (!PL_localizing)
        Perl_croak(aTHX_ "%s", PL_no_modify);
}

I32
Perl_reg_numbered_buff_length(pTHX_ REGEXP * const r, const SV * const sv,
                              const I32 paren)
{
    struct regexp *const rx = (struct regexp *)SvANY(r);
    I32 i;
    I32 s1, t1;

    PERL_ARGS_ASSERT_REG_NUMBERED_BUFF_LENGTH;

    /* Some of this code was originally in C<Perl_magic_len> in F<mg.c> */
	switch (paren) {
      /* $` / ${^PREMATCH} */
      case RX_BUFF_IDX_PREMATCH:
        if (rx->offs[0].start != -1) {
			i = rx->offs[0].start;
			if (i > 0) {
				s1 = 0;
				t1 = i;
				goto getlen;
			}
	    }
        return 0;
      /* $' / ${^POSTMATCH} */
      case RX_BUFF_IDX_POSTMATCH:
	    if (rx->offs[0].end != -1) {
			i = rx->sublen - rx->offs[0].end;
			if (i > 0) {
				s1 = rx->offs[0].end;
				t1 = rx->sublen;
				goto getlen;
			}
	    }
        return 0;
      /* $& / ${^MATCH}, $1, $2, ... */
      default:
	    if (paren <= (I32)rx->nparens &&
            (s1 = rx->offs[paren].start) != -1 &&
            (t1 = rx->offs[paren].end) != -1)
	    {
            i = t1 - s1;
            goto getlen;
        } else {
            if (ckWARN(WARN_UNINITIALIZED))
                report_uninit((const SV *)sv);
            return 0;
        }
    }
  getlen:
    if (i > 0 && RXp_MATCH_UTF8(rx)) {
        const char * const s = rx->subbeg + s1;
        const U8 *ep;
        STRLEN el;

        i = t1 - s1;
        if (is_utf8_string_loclen((U8*)s, i, &ep, &el))
			i = el;
    }
    return i;
}

SV*
Perl_reg_qr_package(pTHX_ REGEXP * const rx)
{
    PERL_ARGS_ASSERT_REG_QR_PACKAGE;
	PERL_UNUSED_ARG(rx);
	if (0)
	    return NULL;
	else
	    return newSVpvs("Regexp");
}

/* Scans the name of a named buffer from the pattern.
 * If flags is REG_RSN_RETURN_NULL returns null.
 * If flags is REG_RSN_RETURN_NAME returns an SV* containing the name
 * If flags is REG_RSN_RETURN_DATA returns the data SV* corresponding
 * to the parsed name as looked up in the RExC_paren_names hash.
 * If there is an error throws a vFAIL().. type exception.
 */

#define REG_RSN_RETURN_NULL    0
#define REG_RSN_RETURN_NAME    1
#define REG_RSN_RETURN_DATA    2

STATIC SV*
S_reg_scan_name(pTHX_ RExC_state_t *pRExC_state, U32 flags)
{
    char *name_start = RExC_parse;

    PERL_ARGS_ASSERT_REG_SCAN_NAME;

    if (isIDFIRST_lazy_if(RExC_parse, UTF)) {
	 /* skip IDFIRST by using do...while */
	if (UTF)
	    do {
		RExC_parse += UTF8SKIP(RExC_parse);
	    } while (isALNUM_utf8((U8*)RExC_parse));
	else
	    do {
		RExC_parse++;
	    } while (isALNUM(*RExC_parse));
    }

    if ( flags ) {
        SV* sv_name
	    = newSVpvn_flags(name_start, (int)(RExC_parse - name_start),
			     SVs_TEMP | (UTF ? SVf_UTF8 : 0));
        if ( flags == REG_RSN_RETURN_NAME)
            return sv_name;
        else if (flags==REG_RSN_RETURN_DATA) {
            HE *he_str = NULL;
            SV *sv_dat = NULL;
            if ( ! sv_name )      /* should not happen*/
                Perl_croak(aTHX_ "panic: no svname in reg_scan_name");
            if (RExC_paren_names)
                he_str = hv_fetch_ent( RExC_paren_names, sv_name, 0, 0 );
            if ( he_str )
                sv_dat = HeVAL(he_str);
            if ( ! sv_dat )
                vFAIL("Reference to nonexistent named group");
            return sv_dat;
        }
        else {
            Perl_croak(aTHX_ "panic: bad flag in reg_scan_name");
        }
        /* NOT REACHED */
    }
    return NULL;
}

#define DEBUG_PARSE_MSG(funcname)     DEBUG_PARSE_r({           \
    int rem=(int)(RExC_end - RExC_parse);                       \
    int cut;                                                    \
    int num;                                                    \
    int iscut=0;                                                \
    if (rem>10) {                                               \
        rem=10;                                                 \
        iscut=1;                                                \
    }                                                           \
    cut=10-rem;                                                 \
    if (RExC_lastparse!=RExC_parse)                             \
        PerlIO_printf(Perl_debug_log," >%.*s%-*s",              \
            rem, RExC_parse,                                    \
            cut + 4,                                            \
            iscut ? "..." : "<"                                 \
        );                                                      \
    else                                                        \
        PerlIO_printf(Perl_debug_log,"%16s","");                \
                                                                \
    if (SIZE_ONLY)                                              \
       num = RExC_size + 1;                                     \
    else                                                        \
       num=REG_NODE_NUM(RExC_emit);                             \
    if (RExC_lastnum!=num)                                      \
       PerlIO_printf(Perl_debug_log,"|%4d",num);                \
    else                                                        \
       PerlIO_printf(Perl_debug_log,"|%4s","");                 \
    PerlIO_printf(Perl_debug_log,"|%*s%-4s",                    \
        (int)((depth*2)), "",                                   \
        (funcname)                                              \
    );                                                          \
    RExC_lastnum=num;                                           \
    RExC_lastparse=RExC_parse;                                  \
})



#define DEBUG_PARSE(funcname)     DEBUG_PARSE_r({           \
    DEBUG_PARSE_MSG((funcname));                            \
    PerlIO_printf(Perl_debug_log,"%4s","\n");               \
})
#define DEBUG_PARSE_FMT(funcname,fmt,args)     DEBUG_PARSE_r({           \
    DEBUG_PARSE_MSG((funcname));                            \
    PerlIO_printf(Perl_debug_log,fmt "\n",args);               \
})
/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
#define REGTAIL(x,y,z) regtail((x),(y),(z),depth+1)
#ifdef DEBUGGING
#define REGTAIL_STUDY(x,y,z) regtail_study((x),(y),(z),depth+1)
#else
#define REGTAIL_STUDY(x,y,z) regtail((x),(y),(z),depth+1)
#endif

STATIC regnode *
S_reg(pTHX_ RExC_state_t *pRExC_state, I32 paren, I32 *flagp,U32 depth)
    /* paren: Parenthesized? 0=top, 1=(, inside: changed to letter. */
{
    dVAR;
    register regnode *ret;		/* Will be the head of the group. */
    register regnode *br;
    register regnode *lastbr;
    register regnode *ender = NULL;
    register I32 parno = 0;
    I32 flags;
    U32 oregflags = RExC_flags;
    bool have_branch = 0;
    bool is_open = 0;
    I32 freeze_paren = 0;
    I32 after_freeze = 0;

    /* for (?g), (?gc), and (?o) warnings; warning
       about (?c) will warn about (?g) -- japhy    */

#define WASTED_O  0x01
#define WASTED_G  0x02
#define WASTED_C  0x04
#define WASTED_GC (0x02|0x04)
    I32 wastedflags = 0x00;

    char * parse_start = RExC_parse; /* MJD */
    char * const oregcomp_parse = RExC_parse;

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REG;
    DEBUG_PARSE("reg ");

    *flagp = 0;				/* Tentatively. */


    /* Make an OPEN node, if parenthesized. */
    if (paren) {
        if ( *RExC_parse == '*') { /* (*VERB:ARG) */
	    char *start_verb = RExC_parse;
	    STRLEN verb_len = 0;
	    char *start_arg = NULL;
	    unsigned char op = 0;
	    int argok = 1;
	    int internal_argval = 0; /* internal_argval is only useful if !argok */
	    while ( *RExC_parse && *RExC_parse != ')' ) {
	        if ( *RExC_parse == ':' ) {
	            start_arg = RExC_parse + 1;
	            break;
	        }
	        RExC_parse++;
	    }
	    ++start_verb;
	    verb_len = RExC_parse - start_verb;
	    if ( start_arg ) {
	        RExC_parse++;
	        while ( *RExC_parse && *RExC_parse != ')' ) 
	            RExC_parse++;
	        if ( *RExC_parse != ')' ) 
	            vFAIL("Unterminated verb pattern argument");
	        if ( RExC_parse == start_arg )
	            start_arg = NULL;
	    } else {
	        if ( *RExC_parse != ')' )
	            vFAIL("Unterminated verb pattern");
	    }
	    
	    switch ( *start_verb ) {
            case 'A':  /* (*ACCEPT) */
                if ( memEQs(start_verb,verb_len,"ACCEPT") ) {
		    op = ACCEPT;
		    internal_argval = RExC_nestroot;
		}
		break;
            case 'C':  /* (*COMMIT) */
                if ( memEQs(start_verb,verb_len,"COMMIT") )
                    op = COMMIT;
                break;
            case 'F':  /* (*FAIL) */
                if ( verb_len==1 || memEQs(start_verb,verb_len,"FAIL") ) {
		    op = OPFAIL;
		    argok = 0;
		}
		break;
            case ':':  /* (*:NAME) */
	    case 'M':  /* (*MARK:NAME) */
	        if ( verb_len==0 || memEQs(start_verb,verb_len,"MARK") ) {
                    op = MARKPOINT;
                    argok = -1;
                }
                break;
            case 'P':  /* (*PRUNE) */
                if ( memEQs(start_verb,verb_len,"PRUNE") )
                    op = PRUNE;
                break;
            case 'S':   /* (*SKIP) */  
                if ( memEQs(start_verb,verb_len,"SKIP") ) 
                    op = SKIP;
                break;
            case 'T':  /* (*THEN) */
                /* [19:06] <TimToady> :: is then */
                if ( memEQs(start_verb,verb_len,"THEN") ) {
                    op = CUTGROUP;
                    RExC_seen |= REG_SEEN_CUTGROUP;
                }
                break;
	    }
	    if ( ! op ) {
	        RExC_parse++;
	        vFAIL3("Unknown verb pattern '%.*s'",
	            verb_len, start_verb);
	    }
	    if ( argok ) {
                if ( start_arg && internal_argval ) {
	            vFAIL3("Verb pattern '%.*s' may not have an argument",
	                verb_len, start_verb); 
	        } else if ( argok < 0 && !start_arg ) {
                    vFAIL3("Verb pattern '%.*s' has a mandatory argument",
	                verb_len, start_verb);    
	        } else {
	            ret = reganode(pRExC_state, op, internal_argval);
	            if ( ! internal_argval && ! SIZE_ONLY ) {
                        if (start_arg) {
                            SV *sv = newSVpvn( start_arg, RExC_parse - start_arg);
                            ARG(ret) = add_data( pRExC_state, 1, "S" );
                            RExC_rxi->data->data[ARG(ret)]=(void*)sv;
                            ret->flags = 0;
                        } else {
                            ret->flags = 1; 
                        }
                    }	            
	        }
	        if (!internal_argval)
	            RExC_seen |= REG_SEEN_VERBARG;
	    } else if ( start_arg ) {
	        vFAIL3("Verb pattern '%.*s' may not have an argument",
	                verb_len, start_verb);    
	    } else {
	        ret = reg_node(pRExC_state, op);
	    }
	    nextchar(pRExC_state);
	    return ret;
        } else 
	if (*RExC_parse == '?') { /* (?...) */
	    bool is_logical = 0;
	    const char * const seqstart = RExC_parse;

	    RExC_parse++;
	    paren = *RExC_parse++;
	    ret = NULL;			/* For look-ahead/behind. */
	    switch (paren) {

	    case 'P':	/* (?P...) variants for those used to PCRE/Python */
	        paren = *RExC_parse++;
		if ( paren == '<')         /* (?P<...>) named capture */
		    goto named_capture;
                else if (paren == '>') {   /* (?P>name) named recursion */
                    goto named_recursion;
                }
                else if (paren == '=') {   /* (?P=...)  named backref */
                    /* this pretty much dupes the code for \k<NAME> in regatom(), if
                       you change this make sure you change that */
                    char* name_start = RExC_parse;
		    U32 num = 0;
                    SV *sv_dat = reg_scan_name(pRExC_state,
                        SIZE_ONLY ? REG_RSN_RETURN_NULL : REG_RSN_RETURN_DATA);
                    if (RExC_parse == name_start || *RExC_parse != ')')
                        vFAIL2("Sequence %.3s... not terminated",parse_start);

                    if (!SIZE_ONLY) {
                        num = add_data( pRExC_state, 1, "S" );
                        RExC_rxi->data->data[num]=(void*)sv_dat;
                        SvREFCNT_inc_simple_void(sv_dat);
                    }
                    RExC_sawback = 1;
                    ret = reganode(pRExC_state,
                    	   (U8)(FOLD ? (LOC ? NREFFL : NREFF) : NREF),
                    	   num);
                    *flagp |= HASWIDTH;

                    Set_Node_Offset(ret, parse_start+1);
                    Set_Node_Cur_Length(ret); /* MJD */

                    nextchar(pRExC_state);
                    return ret;
                }
                RExC_parse++;
		vFAIL3("Sequence (%.*s...) not recognized", RExC_parse-seqstart, seqstart);
		/*NOTREACHED*/
            case '<':           /* (?<...) */
		if (*RExC_parse == '!')
		    paren = ',';
		else if (*RExC_parse != '=') 
              named_capture:
		{               /* (?<...>) */
		    char *name_start;
		    SV *svname;
		    paren= '>';
            case '\'':          /* (?'...') */
    		    name_start= RExC_parse;
    		    svname = reg_scan_name(pRExC_state,
    		        SIZE_ONLY ?  /* reverse test from the others */
    		        REG_RSN_RETURN_NAME : 
    		        REG_RSN_RETURN_NULL);
		    if (RExC_parse == name_start) {
		        RExC_parse++;
		        vFAIL3("Sequence (%.*s...) not recognized", RExC_parse-seqstart, seqstart);
		        /*NOTREACHED*/
                    }
		    if (*RExC_parse != paren)
		        vFAIL2("Sequence (?%c... not terminated",
		            paren=='>' ? '<' : paren);
		    if (SIZE_ONLY) {
			HE *he_str;
			SV *sv_dat = NULL;
                        if (!svname) /* shouldnt happen */
                            Perl_croak(aTHX_
                                "panic: reg_scan_name returned NULL");
                        if (!RExC_paren_names) {
                            RExC_paren_names= newHV();
                            sv_2mortal(MUTABLE_SV(RExC_paren_names));
#ifdef DEBUGGING
                            RExC_paren_name_list= newAV();
                            sv_2mortal(MUTABLE_SV(RExC_paren_name_list));
#endif
                        }
                        he_str = hv_fetch_ent( RExC_paren_names, svname, 1, 0 );
                        if ( he_str )
                            sv_dat = HeVAL(he_str);
                        if ( ! sv_dat ) {
                            /* croak baby croak */
                            Perl_croak(aTHX_
                                "panic: paren_name hash element allocation failed");
                        } else if ( SvPOK(sv_dat) ) {
                            /* (?|...) can mean we have dupes so scan to check
                               its already been stored. Maybe a flag indicating
                               we are inside such a construct would be useful,
                               but the arrays are likely to be quite small, so
                               for now we punt -- dmq */
                            IV count = SvIV(sv_dat);
                            I32 *pv = (I32*)SvPVX(sv_dat);
                            IV i;
                            for ( i = 0 ; i < count ; i++ ) {
                                if ( pv[i] == RExC_npar ) {
                                    count = 0;
                                    break;
                                }
                            }
                            if ( count ) {
                                pv = (I32*)SvGROW(sv_dat, SvCUR(sv_dat) + sizeof(I32)+1);
                                SvCUR_set(sv_dat, SvCUR(sv_dat) + sizeof(I32));
                                pv[count] = RExC_npar;
                                SvIV_set(sv_dat, SvIVX(sv_dat) + 1);
                            }
                        } else {
                            (void)SvUPGRADE(sv_dat,SVt_PVNV);
                            sv_setpvn(sv_dat, (char *)&(RExC_npar), sizeof(I32));
                            SvIOK_on(sv_dat);
                            SvIV_set(sv_dat, 1);
                        }
#ifdef DEBUGGING
                        if (!av_store(RExC_paren_name_list, RExC_npar, SvREFCNT_inc(svname)))
                            SvREFCNT_dec(svname);
#endif

                        /*sv_dump(sv_dat);*/
                    }
                    nextchar(pRExC_state);
		    paren = 1;
		    goto capturing_parens;
		}
                RExC_seen |= REG_SEEN_LOOKBEHIND;
		RExC_parse++;
	    case '=':           /* (?=...) */
		RExC_seen_zerolen++;
			break;
	    case '!':           /* (?!...) */
		RExC_seen_zerolen++;
	        if (*RExC_parse == ')') {
	            ret=reg_node(pRExC_state, OPFAIL);
	            nextchar(pRExC_state);
	            return ret;
	        }
	        break;
	    case '|':           /* (?|...) */
	        /* branch reset, behave like a (?:...) except that
	           buffers in alternations share the same numbers */
	        paren = ':'; 
	        after_freeze = freeze_paren = RExC_npar;
	        break;
	    case ':':           /* (?:...) */
	    case '>':           /* (?>...) */
		break;
	    case '$':           /* (?$...) */
	    case '@':           /* (?@...) */
		vFAIL2("Sequence (?%c...) not implemented", (int)paren);
		break;
	    case '#':           /* (?#...) */
		while (*RExC_parse && *RExC_parse != ')')
		    RExC_parse++;
		if (*RExC_parse != ')')
		    FAIL("Sequence (?#... not terminated");
		nextchar(pRExC_state);
		*flagp = TRYAGAIN;
		return NULL;
	    case '0' :           /* (?0) */
	    case 'R' :           /* (?R) */
		if (*RExC_parse != ')')
		    FAIL("Sequence (?R) not terminated");
		ret = reg_node(pRExC_state, GOSTART);
		*flagp |= POSTPONED;
		nextchar(pRExC_state);
		return ret;
		/*notreached*/
            { /* named and numeric backreferences */
                I32 num;
            case '&':            /* (?&NAME) */
                parse_start = RExC_parse - 1;
              named_recursion:
                {
    		    SV *sv_dat = reg_scan_name(pRExC_state,
    		        SIZE_ONLY ? REG_RSN_RETURN_NULL : REG_RSN_RETURN_DATA);
    		     num = sv_dat ? *((I32 *)SvPVX(sv_dat)) : 0;
                }
                goto gen_recurse_regop;
                /* NOT REACHED */
            case '+':
                if (!(RExC_parse[0] >= '1' && RExC_parse[0] <= '9')) {
                    RExC_parse++;
                    vFAIL("Illegal pattern");
                }
                goto parse_recursion;
                /* NOT REACHED*/
            case '-': /* (?-1) */
                if (!(RExC_parse[0] >= '1' && RExC_parse[0] <= '9')) {
                    RExC_parse--; /* rewind to let it be handled later */
                    goto parse_flags;
                } 
                /*FALLTHROUGH */
            case '1': case '2': case '3': case '4': /* (?1) */
	    case '5': case '6': case '7': case '8': case '9':
	        RExC_parse--;
              parse_recursion:
		num = atoi(RExC_parse);
  	        parse_start = RExC_parse - 1; /* MJD */
	        if (*RExC_parse == '-')
	            RExC_parse++;
		while (isDIGIT(*RExC_parse))
			RExC_parse++;
	        if (*RExC_parse!=')') 
	            vFAIL("Expecting close bracket");
			
              gen_recurse_regop:
                if ( paren == '-' ) {
                    /*
                    Diagram of capture buffer numbering.
                    Top line is the normal capture buffer numbers
                    Botton line is the negative indexing as from
                    the X (the (?-2))

                    +   1 2    3 4 5 X          6 7
                       /(a(x)y)(a(b(c(?-2)d)e)f)(g(h))/
                    -   5 4    3 2 1 X          x x

                    */
                    num = RExC_npar + num;
                    if (num < 1)  {
                        RExC_parse++;
                        vFAIL("Reference to nonexistent group");
                    }
                } else if ( paren == '+' ) {
                    num = RExC_npar + num - 1;
                }

                ret = reganode(pRExC_state, GOSUB, num);
                if (!SIZE_ONLY) {
		    if (num > (I32)RExC_rx->nparens) {
			RExC_parse++;
			vFAIL("Reference to nonexistent group");
	            }
	            ARG2L_SET( ret, RExC_recurse_count++);
                    RExC_emit++;
		    DEBUG_OPTIMISE_MORE_r(PerlIO_printf(Perl_debug_log,
			"Recurse #%"UVuf" to %"IVdf"\n", (UV)ARG(ret), (IV)ARG2L(ret)));
		} else {
		    RExC_size++;
    		}
    		RExC_seen |= REG_SEEN_RECURSE;
                Set_Node_Length(ret, 1 + regarglen[OP(ret)]); /* MJD */
		Set_Node_Offset(ret, parse_start); /* MJD */

                *flagp |= POSTPONED;
                nextchar(pRExC_state);
                return ret;
            } /* named and numeric backreferences */
            /* NOT REACHED */

	    case '?':           /* (??...) */
		is_logical = 1;
		if (*RExC_parse != '{') {
		    RExC_parse++;
		    vFAIL3("Sequence (%.*s...) not recognized", RExC_parse-seqstart, seqstart);
		    /*NOTREACHED*/
		}
		*flagp |= POSTPONED;
		paren = *RExC_parse++;
		/* FALL THROUGH */
	    case '{':           /* (?{...}) */
	    {
		I32 count = 1;
		U32 n = 0;
		char c;
		char *s = RExC_parse;

		RExC_seen_zerolen++;
		RExC_seen |= REG_SEEN_EVAL;
		while (count && (c = *RExC_parse)) {
		    if (c == '\\') {
			if (RExC_parse[1])
			    RExC_parse++;
		    }
		    else if (c == '{')
			count++;
		    else if (c == '}')
			count--;
		    RExC_parse++;
		}
		if (*RExC_parse != ')') {
		    RExC_parse = s;		
		    vFAIL("Sequence (?{...}) not terminated or not {}-balanced");
		}
		if (!SIZE_ONLY) {
		    PAD *pad;
		    OP_4tree *sop, *rop;
		    SV * const sv = newSVpvn(s, RExC_parse - 1 - s);

		    ENTER;
		    Perl_save_re_context(aTHX);
		    rop = sv_compile_2op(sv, &sop, "re", &pad);
		    sop->op_private |= OPpREFCOUNTED;
		    /* re_dup will OpREFCNT_inc */
		    OpREFCNT_set(sop, 1);
		    LEAVE;

		    n = add_data(pRExC_state, 3, "nop");
		    RExC_rxi->data->data[n] = (void*)rop;
		    RExC_rxi->data->data[n+1] = (void*)sop;
		    RExC_rxi->data->data[n+2] = (void*)pad;
		    SvREFCNT_dec(sv);
		}
		else {						/* First pass */
		    if (PL_reginterp_cnt < ++RExC_seen_evals
			&& IN_PERL_RUNTIME)
			/* No compiled RE interpolated, has runtime
			   components ===> unsafe.  */
			FAIL("Eval-group not allowed at runtime, use re 'eval'");
		    if (PL_tainting && PL_tainted)
			FAIL("Eval-group in insecure regular expression");
#if PERL_VERSION > 8
		    if (IN_PERL_COMPILETIME)
			PL_cv_has_eval = 1;
#endif
		}

		nextchar(pRExC_state);
		if (is_logical) {
		    ret = reg_node(pRExC_state, LOGICAL);
		    if (!SIZE_ONLY)
			ret->flags = 2;
                    REGTAIL(pRExC_state, ret, reganode(pRExC_state, EVAL, n));
                    /* deal with the length of this later - MJD */
		    return ret;
		}
		ret = reganode(pRExC_state, EVAL, n);
		Set_Node_Length(ret, RExC_parse - parse_start + 1);
		Set_Node_Offset(ret, parse_start);
		return ret;
	    }
	    case '(':           /* (?(?{...})...) and (?(?=...)...) */
	    {
	        int is_define= 0;
		if (RExC_parse[0] == '?') {        /* (?(?...)) */
		    if (RExC_parse[1] == '=' || RExC_parse[1] == '!'
			|| RExC_parse[1] == '<'
			|| RExC_parse[1] == '{') { /* Lookahead or eval. */
			I32 flag;
			
			ret = reg_node(pRExC_state, LOGICAL);
			if (!SIZE_ONLY)
			    ret->flags = 1;
                        REGTAIL(pRExC_state, ret, reg(pRExC_state, 1, &flag,depth+1));
			goto insert_if;
		    }
		}
		else if ( RExC_parse[0] == '<'     /* (?(<NAME>)...) */
		         || RExC_parse[0] == '\'' ) /* (?('NAME')...) */
	        {
	            char ch = RExC_parse[0] == '<' ? '>' : '\'';
	            char *name_start= RExC_parse++;
	            U32 num = 0;
	            SV *sv_dat=reg_scan_name(pRExC_state,
	                SIZE_ONLY ? REG_RSN_RETURN_NULL : REG_RSN_RETURN_DATA);
	            if (RExC_parse == name_start || *RExC_parse != ch)
                        vFAIL2("Sequence (?(%c... not terminated",
                            (ch == '>' ? '<' : ch));
                    RExC_parse++;
	            if (!SIZE_ONLY) {
                        num = add_data( pRExC_state, 1, "S" );
                        RExC_rxi->data->data[num]=(void*)sv_dat;
                        SvREFCNT_inc_simple_void(sv_dat);
                    }
                    ret = reganode(pRExC_state,NGROUPP,num);
                    goto insert_if_check_paren;
		}
		else if (RExC_parse[0] == 'D' &&
		         RExC_parse[1] == 'E' &&
		         RExC_parse[2] == 'F' &&
		         RExC_parse[3] == 'I' &&
		         RExC_parse[4] == 'N' &&
		         RExC_parse[5] == 'E')
		{
		    ret = reganode(pRExC_state,DEFINEP,0);
		    RExC_parse +=6 ;
		    is_define = 1;
		    goto insert_if_check_paren;
		}
		else if (RExC_parse[0] == 'R') {
		    RExC_parse++;
		    parno = 0;
		    if (RExC_parse[0] >= '1' && RExC_parse[0] <= '9' ) {
		        parno = atoi(RExC_parse++);
		        while (isDIGIT(*RExC_parse))
			    RExC_parse++;
		    } else if (RExC_parse[0] == '&') {
		        SV *sv_dat;
		        RExC_parse++;
		        sv_dat = reg_scan_name(pRExC_state,
    		            SIZE_ONLY ? REG_RSN_RETURN_NULL : REG_RSN_RETURN_DATA);
    		        parno = sv_dat ? *((I32 *)SvPVX(sv_dat)) : 0;
		    }
		    ret = reganode(pRExC_state,INSUBP,parno); 
		    goto insert_if_check_paren;
		}
		else if (RExC_parse[0] >= '1' && RExC_parse[0] <= '9' ) {
                    /* (?(1)...) */
		    char c;
		    parno = atoi(RExC_parse++);

		    while (isDIGIT(*RExC_parse))
			RExC_parse++;
                    ret = reganode(pRExC_state, GROUPP, parno);

                 insert_if_check_paren:
		    if ((c = *nextchar(pRExC_state)) != ')')
			vFAIL("Switch condition not recognized");
		  insert_if:
                    REGTAIL(pRExC_state, ret, reganode(pRExC_state, IFTHEN, 0));
                    br = regbranch(pRExC_state, &flags, 1,depth+1);
		    if (br == NULL)
			br = reganode(pRExC_state, LONGJMP, 0);
		    else
                        REGTAIL(pRExC_state, br, reganode(pRExC_state, LONGJMP, 0));
		    c = *nextchar(pRExC_state);
		    if (flags&HASWIDTH)
			*flagp |= HASWIDTH;
		    if (c == '|') {
		        if (is_define) 
		            vFAIL("(?(DEFINE)....) does not allow branches");
			lastbr = reganode(pRExC_state, IFTHEN, 0); /* Fake one for optimizer. */
                        regbranch(pRExC_state, &flags, 1,depth+1);
                        REGTAIL(pRExC_state, ret, lastbr);
		 	if (flags&HASWIDTH)
			    *flagp |= HASWIDTH;
			c = *nextchar(pRExC_state);
		    }
		    else
			lastbr = NULL;
		    if (c != ')')
			vFAIL("Switch (?(condition)... contains too many branches");
		    ender = reg_node(pRExC_state, TAIL);
                    REGTAIL(pRExC_state, br, ender);
		    if (lastbr) {
                        REGTAIL(pRExC_state, lastbr, ender);
                        REGTAIL(pRExC_state, NEXTOPER(NEXTOPER(lastbr)), ender);
		    }
		    else
                        REGTAIL(pRExC_state, ret, ender);
                    RExC_size++; /* XXX WHY do we need this?!!
                                    For large programs it seems to be required
                                    but I can't figure out why. -- dmq*/
		    return ret;
		}
		else {
		    vFAIL2("Unknown switch condition (?(%.2s", RExC_parse);
		}
	    }
            case 0:
		RExC_parse--; /* for vFAIL to print correctly */
                vFAIL("Sequence (? incomplete");
                break;
	    default:
	        --RExC_parse;
	        parse_flags:      /* (?i) */  
	    {
                U32 posflags = 0, negflags = 0;
	        U32 *flagsp = &posflags;

		while (*RExC_parse) {
		    /* && strchr("iogcmsx", *RExC_parse) */
		    /* (?g), (?gc) and (?o) are useless here
		       and must be globally applied -- japhy */
                    switch (*RExC_parse) {
	            CASE_STD_PMMOD_FLAGS_PARSE_SET(flagsp);
                    case ONCE_PAT_MOD: /* 'o' */
                    case GLOBAL_PAT_MOD: /* 'g' */
			if (SIZE_ONLY && ckWARN(WARN_REGEXP)) {
			    const I32 wflagbit = *RExC_parse == 'o' ? WASTED_O : WASTED_G;
			    if (! (wastedflags & wflagbit) ) {
				wastedflags |= wflagbit;
				vWARN5(
				    RExC_parse + 1,
				    "Useless (%s%c) - %suse /%c modifier",
				    flagsp == &negflags ? "?-" : "?",
				    *RExC_parse,
				    flagsp == &negflags ? "don't " : "",
				    *RExC_parse
				);
			    }
			}
			break;
		        
		    case CONTINUE_PAT_MOD: /* 'c' */
			if (SIZE_ONLY && ckWARN(WARN_REGEXP)) {
			    if (! (wastedflags & WASTED_C) ) {
				wastedflags |= WASTED_GC;
				vWARN3(
				    RExC_parse + 1,
				    "Useless (%sc) - %suse /gc modifier",
				    flagsp == &negflags ? "?-" : "?",
				    flagsp == &negflags ? "don't " : ""
				);
			    }
			}
			break;
	            case KEEPCOPY_PAT_MOD: /* 'p' */
                        if (flagsp == &negflags) {
                            if (SIZE_ONLY)
                                ckWARNreg(RExC_parse + 1,"Useless use of (?-p)");
                        } else {
                            *flagsp |= RXf_PMf_KEEPCOPY;
                        }
	                break;
                    case '-':
                        if (flagsp == &negflags) {
                            RExC_parse++;
		            vFAIL3("Sequence (%.*s...) not recognized", RExC_parse-seqstart, seqstart);
		            /*NOTREACHED*/
		        }
			flagsp = &negflags;
		        wastedflags = 0;  /* reset so (?g-c) warns twice */
		        break;
                    case ':':
		        paren = ':';
		        /*FALLTHROUGH*/
                    case ')':
                        RExC_flags |= posflags;
                        RExC_flags &= ~negflags;
                        if (paren != ':') {
                            oregflags |= posflags;
                            oregflags &= ~negflags;
                        }
                        nextchar(pRExC_state);
		        if (paren != ':') {
		            *flagp = TRYAGAIN;
		            return NULL;
		        } else {
                            ret = NULL;
		            goto parse_rest;
		        }
		        /*NOTREACHED*/
                    default:
		        RExC_parse++;
		        vFAIL3("Sequence (%.*s...) not recognized", RExC_parse-seqstart, seqstart);
		        /*NOTREACHED*/
                    }                           
		    ++RExC_parse;
		}
	    }} /* one for the default block, one for the switch */
	}
	else {                  /* (...) */
	  capturing_parens:
	    parno = RExC_npar;
	    RExC_npar++;
	    
	    ret = reganode(pRExC_state, OPEN, parno);
	    if (!SIZE_ONLY ){
	        if (!RExC_nestroot) 
	            RExC_nestroot = parno;
	        if (RExC_seen & REG_SEEN_RECURSE
	            && !RExC_open_parens[parno-1])
	        {
		    DEBUG_OPTIMISE_MORE_r(PerlIO_printf(Perl_debug_log,
			"Setting open paren #%"IVdf" to %d\n", 
			(IV)parno, REG_NODE_NUM(ret)));
	            RExC_open_parens[parno-1]= ret;
	        }
	    }
            Set_Node_Length(ret, 1); /* MJD */
            Set_Node_Offset(ret, RExC_parse); /* MJD */
	    is_open = 1;
	}
    }
    else                        /* ! paren */
	ret = NULL;
   
   parse_rest:
    /* Pick up the branches, linking them together. */
    parse_start = RExC_parse;   /* MJD */
    br = regbranch(pRExC_state, &flags, 1,depth+1);

    if (freeze_paren) {
        if (RExC_npar > after_freeze)
            after_freeze = RExC_npar;
        RExC_npar = freeze_paren;
    }

    /*     branch_len = (paren != 0); */

    if (br == NULL)
	return(NULL);
    if (*RExC_parse == '|') {
	if (!SIZE_ONLY && RExC_extralen) {
	    reginsert(pRExC_state, BRANCHJ, br, depth+1);
	}
	else {                  /* MJD */
	    reginsert(pRExC_state, BRANCH, br, depth+1);
            Set_Node_Length(br, paren != 0);
            Set_Node_Offset_To_R(br-RExC_emit_start, parse_start-RExC_start);
        }
	have_branch = 1;
	if (SIZE_ONLY)
	    RExC_extralen += 1;		/* For BRANCHJ-BRANCH. */
    }
    else if (paren == ':') {
	*flagp |= flags&SIMPLE;
    }
    if (is_open) {				/* Starts with OPEN. */
        REGTAIL(pRExC_state, ret, br);          /* OPEN -> first. */
    }
    else if (paren != '?')		/* Not Conditional */
	ret = br;
    *flagp |= flags & (SPSTART | HASWIDTH | POSTPONED);
    lastbr = br;
    while (*RExC_parse == '|') {
	if (!SIZE_ONLY && RExC_extralen) {
	    ender = reganode(pRExC_state, LONGJMP,0);
            REGTAIL(pRExC_state, NEXTOPER(NEXTOPER(lastbr)), ender); /* Append to the previous. */
	}
	if (SIZE_ONLY)
	    RExC_extralen += 2;		/* Account for LONGJMP. */
	nextchar(pRExC_state);
	if (freeze_paren) {
	    if (RExC_npar > after_freeze)
	        after_freeze = RExC_npar;
            RExC_npar = freeze_paren;	    
        }
        br = regbranch(pRExC_state, &flags, 0, depth+1);

	if (br == NULL)
	    return(NULL);
        REGTAIL(pRExC_state, lastbr, br);               /* BRANCH -> BRANCH. */
	lastbr = br;
	*flagp |= flags & (SPSTART | HASWIDTH | POSTPONED);
    }

    if (have_branch || paren != ':') {
	/* Make a closing node, and hook it on the end. */
	switch (paren) {
	case ':':
	    ender = reg_node(pRExC_state, TAIL);
	    break;
	case 1:
	    ender = reganode(pRExC_state, CLOSE, parno);
	    if (!SIZE_ONLY && RExC_seen & REG_SEEN_RECURSE) {
		DEBUG_OPTIMISE_MORE_r(PerlIO_printf(Perl_debug_log,
			"Setting close paren #%"IVdf" to %d\n", 
			(IV)parno, REG_NODE_NUM(ender)));
	        RExC_close_parens[parno-1]= ender;
	        if (RExC_nestroot == parno) 
	            RExC_nestroot = 0;
	    }	    
            Set_Node_Offset(ender,RExC_parse+1); /* MJD */
            Set_Node_Length(ender,1); /* MJD */
	    break;
	case '<':
	case ',':
	case '=':
	case '!':
	    *flagp &= ~HASWIDTH;
	    /* FALL THROUGH */
	case '>':
	    ender = reg_node(pRExC_state, SUCCEED);
	    break;
	case 0:
	    ender = reg_node(pRExC_state, END);
	    if (!SIZE_ONLY) {
                assert(!RExC_opend); /* there can only be one! */
                RExC_opend = ender;
            }
	    break;
	}
        REGTAIL(pRExC_state, lastbr, ender);

	if (have_branch && !SIZE_ONLY) {
	    if (depth==1)
	        RExC_seen |= REG_TOP_LEVEL_BRANCHES;

	    /* Hook the tails of the branches to the closing node. */
	    for (br = ret; br; br = regnext(br)) {
		const U8 op = PL_regkind[OP(br)];
		if (op == BRANCH) {
                    REGTAIL_STUDY(pRExC_state, NEXTOPER(br), ender);
		}
		else if (op == BRANCHJ) {
                    REGTAIL_STUDY(pRExC_state, NEXTOPER(NEXTOPER(br)), ender);
		}
	    }
	}
    }

    {
        const char *p;
        static const char parens[] = "=!<,>";

	if (paren && (p = strchr(parens, paren))) {
	    U8 node = ((p - parens) % 2) ? UNLESSM : IFMATCH;
	    int flag = (p - parens) > 1;

	    if (paren == '>')
		node = SUSPEND, flag = 0;
	    reginsert(pRExC_state, node,ret, depth+1);
	    Set_Node_Cur_Length(ret);
	    Set_Node_Offset(ret, parse_start + 1);
	    ret->flags = flag;
            REGTAIL_STUDY(pRExC_state, ret, reg_node(pRExC_state, TAIL));
	}
    }

    /* Check for proper termination. */
    if (paren) {
	RExC_flags = oregflags;
	if (RExC_parse >= RExC_end || *nextchar(pRExC_state) != ')') {
	    RExC_parse = oregcomp_parse;
	    vFAIL("Unmatched (");
	}
    }
    else if (!paren && RExC_parse < RExC_end) {
	if (*RExC_parse == ')') {
	    RExC_parse++;
	    vFAIL("Unmatched )");
	}
	else
	    FAIL("Junk on end of regexp");	/* "Can't happen". */
	/* NOTREACHED */
    }
    if (after_freeze)
        RExC_npar = after_freeze;
    return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
STATIC regnode *
S_regbranch(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, I32 first, U32 depth)
{
    dVAR;
    register regnode *ret;
    register regnode *chain = NULL;
    register regnode *latest;
    I32 flags = 0, c = 0;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGBRANCH;

    DEBUG_PARSE("brnc");

    if (first)
	ret = NULL;
    else {
	if (!SIZE_ONLY && RExC_extralen)
	    ret = reganode(pRExC_state, BRANCHJ,0);
	else {
	    ret = reg_node(pRExC_state, BRANCH);
            Set_Node_Length(ret, 1);
        }
    }
	
    if (!first && SIZE_ONLY)
	RExC_extralen += 1;			/* BRANCHJ */

    *flagp = WORST;			/* Tentatively. */

    RExC_parse--;
    nextchar(pRExC_state);
    while (RExC_parse < RExC_end && *RExC_parse != '|' && *RExC_parse != ')') {
	flags &= ~TRYAGAIN;
        latest = regpiece(pRExC_state, &flags,depth+1);
	if (latest == NULL) {
	    if (flags & TRYAGAIN)
		continue;
	    return(NULL);
	}
	else if (ret == NULL)
	    ret = latest;
	*flagp |= flags&(HASWIDTH|POSTPONED);
	if (chain == NULL) 	/* First piece. */
	    *flagp |= flags&SPSTART;
	else {
	    RExC_naughty++;
            REGTAIL(pRExC_state, chain, latest);
	}
	chain = latest;
	c++;
    }
    if (chain == NULL) {	/* Loop ran zero times. */
	chain = reg_node(pRExC_state, NOTHING);
	if (ret == NULL)
	    ret = chain;
    }
    if (c == 1) {
	*flagp |= flags&SIMPLE;
    }

    return ret;
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
STATIC regnode *
S_regpiece(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, U32 depth)
{
    dVAR;
    register regnode *ret;
    register char op;
    register char *next;
    I32 flags;
    const char * const origparse = RExC_parse;
    I32 min;
    I32 max = REG_INFTY;
    char *parse_start;
    const char *maxpos = NULL;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGPIECE;

    DEBUG_PARSE("piec");

    ret = regatom(pRExC_state, &flags,depth+1);
    if (ret == NULL) {
	if (flags & TRYAGAIN)
	    *flagp |= TRYAGAIN;
	return(NULL);
    }

    op = *RExC_parse;

    if (op == '{' && regcurly(RExC_parse)) {
	maxpos = NULL;
        parse_start = RExC_parse; /* MJD */
	next = RExC_parse + 1;
	while (isDIGIT(*next) || *next == ',') {
	    if (*next == ',') {
		if (maxpos)
		    break;
		else
		    maxpos = next;
	    }
	    next++;
	}
	if (*next == '}') {		/* got one */
	    if (!maxpos)
		maxpos = next;
	    RExC_parse++;
	    min = atoi(RExC_parse);
	    if (*maxpos == ',')
		maxpos++;
	    else
		maxpos = RExC_parse;
	    max = atoi(maxpos);
	    if (!max && *maxpos != '0')
		max = REG_INFTY;		/* meaning "infinity" */
	    else if (max >= REG_INFTY)
		vFAIL2("Quantifier in {,} bigger than %d", REG_INFTY - 1);
	    RExC_parse = next;
	    nextchar(pRExC_state);

	do_curly:
	    if ((flags&SIMPLE)) {
		RExC_naughty += 2 + RExC_naughty / 2;
		reginsert(pRExC_state, CURLY, ret, depth+1);
                Set_Node_Offset(ret, parse_start+1); /* MJD */
                Set_Node_Cur_Length(ret);
	    }
	    else {
		regnode * const w = reg_node(pRExC_state, WHILEM);

		w->flags = 0;
                REGTAIL(pRExC_state, ret, w);
		if (!SIZE_ONLY && RExC_extralen) {
		    reginsert(pRExC_state, LONGJMP,ret, depth+1);
		    reginsert(pRExC_state, NOTHING,ret, depth+1);
		    NEXT_OFF(ret) = 3;	/* Go over LONGJMP. */
		}
		reginsert(pRExC_state, CURLYX,ret, depth+1);
                                /* MJD hk */
                Set_Node_Offset(ret, parse_start+1);
                Set_Node_Length(ret,
                                op == '{' ? (RExC_parse - parse_start) : 1);

		if (!SIZE_ONLY && RExC_extralen)
		    NEXT_OFF(ret) = 3;	/* Go over NOTHING to LONGJMP. */
                REGTAIL(pRExC_state, ret, reg_node(pRExC_state, NOTHING));
		if (SIZE_ONLY)
		    RExC_whilem_seen++, RExC_extralen += 3;
		RExC_naughty += 4 + RExC_naughty;	/* compound interest */
	    }
	    ret->flags = 0;

	    if (min > 0)
		*flagp = WORST;
	    if (max > 0)
		*flagp |= HASWIDTH;
	    if (max < min)
		vFAIL("Can't do {n,m} with n > m");
	    if (!SIZE_ONLY) {
		ARG1_SET(ret, (U16)min);
		ARG2_SET(ret, (U16)max);
	    }

	    goto nest_check;
	}
    }

    if (!ISMULT1(op)) {
	*flagp = flags;
	return(ret);
    }

#if 0				/* Now runtime fix should be reliable. */

    /* if this is reinstated, don't forget to put this back into perldiag:

	    =item Regexp *+ operand could be empty at {#} in regex m/%s/

	   (F) The part of the regexp subject to either the * or + quantifier
           could match an empty string. The {#} shows in the regular
           expression about where the problem was discovered.

    */

    if (!(flags&HASWIDTH) && op != '?')
      vFAIL("Regexp *+ operand could be empty");
#endif

    parse_start = RExC_parse;
    nextchar(pRExC_state);

    *flagp = (op != '+') ? (WORST|SPSTART|HASWIDTH) : (WORST|HASWIDTH);

    if (op == '*' && (flags&SIMPLE)) {
	reginsert(pRExC_state, STAR, ret, depth+1);
	ret->flags = 0;
	RExC_naughty += 4;
    }
    else if (op == '*') {
	min = 0;
	goto do_curly;
    }
    else if (op == '+' && (flags&SIMPLE)) {
	reginsert(pRExC_state, PLUS, ret, depth+1);
	ret->flags = 0;
	RExC_naughty += 3;
    }
    else if (op == '+') {
	min = 1;
	goto do_curly;
    }
    else if (op == '?') {
	min = 0; max = 1;
	goto do_curly;
    }
  nest_check:
    if (!SIZE_ONLY && !(flags&(HASWIDTH|POSTPONED)) && max > REG_INFTY/3) {
	ckWARN3reg(RExC_parse,
		   "%.*s matches null string many times",
		   (int)(RExC_parse >= origparse ? RExC_parse - origparse : 0),
		   origparse);
    }

    if (RExC_parse < RExC_end && *RExC_parse == '?') {
	nextchar(pRExC_state);
	reginsert(pRExC_state, MINMOD, ret, depth+1);
        REGTAIL(pRExC_state, ret, ret + NODE_STEP_REGNODE);
    }
#ifndef REG_ALLOW_MINMOD_SUSPEND
    else
#endif
    if (RExC_parse < RExC_end && *RExC_parse == '+') {
        regnode *ender;
        nextchar(pRExC_state);
        ender = reg_node(pRExC_state, SUCCEED);
        REGTAIL(pRExC_state, ret, ender);
        reginsert(pRExC_state, SUSPEND, ret, depth+1);
        ret->flags = 0;
        ender = reg_node(pRExC_state, TAIL);
        REGTAIL(pRExC_state, ret, ender);
        /*ret= ender;*/
    }

    if (RExC_parse < RExC_end && ISMULT2(RExC_parse)) {
	RExC_parse++;
	vFAIL("Nested quantifiers");
    }

    return(ret);
}


/* reg_namedseq(pRExC_state,UVp)
   
   This is expected to be called by a parser routine that has 
   recognized '\N' and needs to handle the rest. RExC_parse is
   expected to point at the first char following the N at the time
   of the call.

   The \N may be inside (indicated by valuep not being NULL) or outside a
   character class.

   \N may begin either a named sequence, or if outside a character class, mean
   to match a non-newline.  For non single-quoted regexes, the tokenizer has
   attempted to decide which, and in the case of a named sequence converted it
   into one of the forms: \N{} (if the sequence is null), or \N{U+c1.c2...},
   where c1... are the characters in the sequence.  For single-quoted regexes,
   the tokenizer passes the \N sequence through unchanged; this code will not
   attempt to determine this nor expand those.  The net effect is that if the
   beginning of the passed-in pattern isn't '{U+' or there is no '}', it
   signals that this \N occurrence means to match a non-newline.
   
   Only the \N{U+...} form should occur in a character class, for the same
   reason that '.' inside a character class means to just match a period: it
   just doesn't make sense.
   
   If valuep is non-null then it is assumed that we are parsing inside 
   of a charclass definition and the first codepoint in the resolved
   string is returned via *valuep and the routine will return NULL. 
   In this mode if a multichar string is returned from the charnames 
   handler, a warning will be issued, and only the first char in the 
   sequence will be examined. If the string returned is zero length
   then the value of *valuep is undefined and NON-NULL will 
   be returned to indicate failure. (This will NOT be a valid pointer 
   to a regnode.)
   
   If valuep is null then it is assumed that we are parsing normal text and a
   new EXACT node is inserted into the program containing the resolved string,
   and a pointer to the new node is returned.  But if the string is zero length
   a NOTHING node is emitted instead.

   On success RExC_parse is set to the char following the endbrace.
   Parsing failures will generate a fatal error via vFAIL(...)
 */
STATIC regnode *
S_reg_namedseq(pTHX_ RExC_state_t *pRExC_state, UV *valuep, I32 *flagp)
{
    char * endbrace;    /* '}' following the name */
    regnode *ret = NULL;
#ifdef DEBUGGING
    char* parse_start = RExC_parse - 2;	    /* points to the '\N' */
#endif
    char* p;

    GET_RE_DEBUG_FLAGS_DECL;
 
    PERL_ARGS_ASSERT_REG_NAMEDSEQ;

    GET_RE_DEBUG_FLAGS;

    /* The [^\n] meaning of \N ignores spaces and comments under the /x
     * modifier.  The other meaning does not */
    p = (RExC_flags & RXf_PMf_EXTENDED)
	? regwhite( pRExC_state, RExC_parse )
	: RExC_parse;
   
    /* Disambiguate between \N meaning a named character versus \N meaning
     * [^\n].  The former is assumed when it can't be the latter. */
    if (*p != '{' || regcurly(p)) {
	RExC_parse = p;
	if (valuep) {
	    /* no bare \N in a charclass */
	    vFAIL("\\N in a character class must be a named character: \\N{...}");
	}
	nextchar(pRExC_state);
	ret = reg_node(pRExC_state, REG_ANY);
	*flagp |= HASWIDTH|SIMPLE;
	RExC_naughty++;
	RExC_parse--;
        Set_Node_Length(ret, 1); /* MJD */
	return ret;
    }

    /* Here, we have decided it should be a named sequence */

    /* The test above made sure that the next real character is a '{', but
     * under the /x modifier, it could be separated by space (or a comment and
     * \n) and this is not allowed (for consistency with \x{...} and the
     * tokenizer handling of \N{NAME}). */
    if (*RExC_parse != '{') {
	vFAIL("Missing braces on \\N{}");
    }

    RExC_parse++;	/* Skip past the '{' */

    if (! (endbrace = strchr(RExC_parse, '}')) /* no trailing brace */
	|| ! (endbrace == RExC_parse		/* nothing between the {} */
	      || (endbrace - RExC_parse >= 2	/* U+ (bad hex is checked below */
		  && strnEQ(RExC_parse, "U+", 2)))) /* for a better error msg) */
    {
	if (endbrace) RExC_parse = endbrace;	/* position msg's '<--HERE' */
	vFAIL("\\N{NAME} must be resolved by the lexer");
    }

    if (endbrace == RExC_parse) {   /* empty: \N{} */
	if (! valuep) {
	    RExC_parse = endbrace + 1;  
	    return reg_node(pRExC_state,NOTHING);
	}

	if (SIZE_ONLY) {
	    ckWARNreg(RExC_parse,
		    "Ignoring zero length \\N{} in character class"
	    );
	    RExC_parse = endbrace + 1;  
	}
	*valuep = 0;
	return (regnode *) &RExC_parse; /* Invalid regnode pointer */
    }

    RExC_utf8 = 1;	/* named sequences imply Unicode semantics */
    RExC_parse += 2;	/* Skip past the 'U+' */

    if (valuep) {   /* In a bracketed char class */
	/* We only pay attention to the first char of 
	multichar strings being returned. I kinda wonder
	if this makes sense as it does change the behaviour
	from earlier versions, OTOH that behaviour was broken
	as well. XXX Solution is to recharacterize as
	[rest-of-class]|multi1|multi2... */

	STRLEN length_of_hex;
	I32 flags = PERL_SCAN_ALLOW_UNDERSCORES
	    | PERL_SCAN_DISALLOW_PREFIX
	    | (SIZE_ONLY ? PERL_SCAN_SILENT_ILLDIGIT : 0);
    
	char * endchar = RExC_parse + strcspn(RExC_parse, ".}");
	if (endchar < endbrace) {
	    ckWARNreg(endchar, "Using just the first character returned by \\N{} in character class");
	}

	length_of_hex = (STRLEN)(endchar - RExC_parse);
	*valuep = grok_hex(RExC_parse, &length_of_hex, &flags, NULL);

	/* The tokenizer should have guaranteed validity, but it's possible to
	 * bypass it by using single quoting, so check */
	if (length_of_hex == 0
	    || length_of_hex != (STRLEN)(endchar - RExC_parse) )
	{
	    RExC_parse += length_of_hex;	/* Includes all the valid */
	    RExC_parse += (RExC_orig_utf8)	/* point to after 1st invalid */
			    ? UTF8SKIP(RExC_parse)
			    : 1;
	    /* Guard against malformed utf8 */
	    if (RExC_parse >= endchar) RExC_parse = endchar;
	    vFAIL("Invalid hexadecimal number in \\N{U+...}");
	}    

	RExC_parse = endbrace + 1;
	if (endchar == endbrace) return NULL;

        ret = (regnode *) &RExC_parse;	/* Invalid regnode pointer */
    }
    else {	/* Not a char class */
	char *s;	    /* String to put in generated EXACT node */
	STRLEN len = 0;	    /* Its current length */
	char *endchar;	    /* Points to '.' or '}' ending cur char in the input
			       stream */

	ret = reg_node(pRExC_state,
			(U8)(FOLD ? (LOC ? EXACTFL : EXACTF) : EXACT));
	s= STRING(ret);

	/* Exact nodes can hold only a U8 length's of text = 255.  Loop through
	 * the input which is of the form now 'c1.c2.c3...}' until find the
	 * ending brace or exeed length 255.  The characters that exceed this
	 * limit are dropped.  The limit could be relaxed should it become
	 * desirable by reparsing this as (?:\N{NAME}), so could generate
	 * multiple EXACT nodes, as is done for just regular input.  But this
	 * is primarily a named character, and not intended to be a huge long
	 * string, so 255 bytes should be good enough */
	while (1) {
	    STRLEN length_of_hex;
	    I32 grok_flags = PERL_SCAN_ALLOW_UNDERSCORES
			    | PERL_SCAN_DISALLOW_PREFIX
			    | (SIZE_ONLY ? PERL_SCAN_SILENT_ILLDIGIT : 0);
	    UV cp;  /* Ord of current character */

	    /* Code points are separated by dots.  If none, there is only one
	     * code point, and is terminated by the brace */
	    endchar = RExC_parse + strcspn(RExC_parse, ".}");

	    /* The values are Unicode even on EBCDIC machines */
	    length_of_hex = (STRLEN)(endchar - RExC_parse);
	    cp = grok_hex(RExC_parse, &length_of_hex, &grok_flags, NULL);
	    if ( length_of_hex == 0 
		|| length_of_hex != (STRLEN)(endchar - RExC_parse) )
	    {
		RExC_parse += length_of_hex;	    /* Includes all the valid */
		RExC_parse += (RExC_orig_utf8)	/* point to after 1st invalid */
				? UTF8SKIP(RExC_parse)
				: 1;
		/* Guard against malformed utf8 */
		if (RExC_parse >= endchar) RExC_parse = endchar;
		vFAIL("Invalid hexadecimal number in \\N{U+...}");
	    }    

	    if (! FOLD) {	/* Not folding, just append to the string */
		STRLEN unilen;

		/* Quit before adding this character if would exceed limit */
		if (len + UNISKIP(cp) > U8_MAX) break;

		unilen = reguni(pRExC_state, cp, s);
		if (unilen > 0) {
		    s   += unilen;
		    len += unilen;
		}
	    } else {	/* Folding, output the folded equivalent */
		STRLEN foldlen,numlen;
		U8 tmpbuf[UTF8_MAXBYTES_CASE+1], *foldbuf;
		cp = toFOLD_uni(cp, tmpbuf, &foldlen);

		/* Quit before exceeding size limit */
		if (len + foldlen > U8_MAX) break;
		
		for (foldbuf = tmpbuf;
		    foldlen;
		    foldlen -= numlen) 
		{
		    cp = utf8_to_uvchr(foldbuf, &numlen);
		    if (numlen > 0) {
			const STRLEN unilen = reguni(pRExC_state, cp, s);
			s       += unilen;
			len     += unilen;
			/* In EBCDIC the numlen and unilen can differ. */
			foldbuf += numlen;
			if (numlen >= foldlen)
			    break;
		    }
		    else
			break; /* "Can't happen." */
		}                          
	    }

	    /* Point to the beginning of the next character in the sequence. */
	    RExC_parse = endchar + 1;

	    /* Quit if no more characters */
	    if (RExC_parse >= endbrace) break;
	}


	if (SIZE_ONLY) {
	    if (RExC_parse < endbrace) {
		ckWARNreg(RExC_parse - 1,
			  "Using just the first characters returned by \\N{}");
	    }

	    RExC_size += STR_SZ(len);
	} else {
	    STR_LEN(ret) = len;
	    RExC_emit += STR_SZ(len);
	}

	RExC_parse = endbrace + 1;

	*flagp |= HASWIDTH; /* Not SIMPLE, as that causes the engine to fail
			       with malformed in t/re/pat_advanced.t */
	RExC_parse --;
	Set_Node_Cur_Length(ret); /* MJD */
	nextchar(pRExC_state);
    }

    return ret;
}


/*
 * reg_recode
 *
 * It returns the code point in utf8 for the value in *encp.
 *    value: a code value in the source encoding
 *    encp:  a pointer to an Encode object
 *
 * If the result from Encode is not a single character,
 * it returns U+FFFD (Replacement character) and sets *encp to NULL.
 */
STATIC UV
S_reg_recode(pTHX_ const char value, SV **encp)
{
    STRLEN numlen = 1;
    SV * const sv = newSVpvn_flags(&value, numlen, SVs_TEMP);
    const char * const s = *encp ? sv_recode_to_utf8(sv, *encp) : SvPVX(sv);
    const STRLEN newlen = SvCUR(sv);
    UV uv = UNICODE_REPLACEMENT;

    PERL_ARGS_ASSERT_REG_RECODE;

    if (newlen)
	uv = SvUTF8(sv)
	     ? utf8n_to_uvchr((U8*)s, newlen, &numlen, UTF8_ALLOW_DEFAULT)
	     : *(U8*)s;

    if (!newlen || numlen != newlen) {
	uv = UNICODE_REPLACEMENT;
	*encp = NULL;
    }
    return uv;
}


/*
 - regatom - the lowest level

   Try to identify anything special at the start of the pattern. If there
   is, then handle it as required. This may involve generating a single regop,
   such as for an assertion; or it may involve recursing, such as to
   handle a () structure.

   If the string doesn't start with something special then we gobble up
   as much literal text as we can.

   Once we have been able to handle whatever type of thing started the
   sequence, we return.

   Note: we have to be careful with escapes, as they can be both literal
   and special, and in the case of \10 and friends can either, depending
   on context. Specifically there are two seperate switches for handling
   escape sequences, with the one for handling literal escapes requiring
   a dummy entry for all of the special escapes that are actually handled
   by the other.
*/

STATIC regnode *
S_regatom(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, U32 depth)
{
    dVAR;
    register regnode *ret = NULL;
    I32 flags;
    char *parse_start = RExC_parse;
    GET_RE_DEBUG_FLAGS_DECL;
    DEBUG_PARSE("atom");
    *flagp = WORST;		/* Tentatively. */

    PERL_ARGS_ASSERT_REGATOM;

tryagain:
    switch ((U8)*RExC_parse) {
    case '^':
	RExC_seen_zerolen++;
	nextchar(pRExC_state);
	if (RExC_flags & RXf_PMf_MULTILINE)
	    ret = reg_node(pRExC_state, MBOL);
	else if (RExC_flags & RXf_PMf_SINGLELINE)
	    ret = reg_node(pRExC_state, SBOL);
	else
	    ret = reg_node(pRExC_state, BOL);
        Set_Node_Length(ret, 1); /* MJD */
	break;
    case '$':
	nextchar(pRExC_state);
	if (*RExC_parse)
	    RExC_seen_zerolen++;
	if (RExC_flags & RXf_PMf_MULTILINE)
	    ret = reg_node(pRExC_state, MEOL);
	else if (RExC_flags & RXf_PMf_SINGLELINE)
	    ret = reg_node(pRExC_state, SEOL);
	else
	    ret = reg_node(pRExC_state, EOL);
        Set_Node_Length(ret, 1); /* MJD */
	break;
    case '.':
	nextchar(pRExC_state);
	if (RExC_flags & RXf_PMf_SINGLELINE)
	    ret = reg_node(pRExC_state, SANY);
	else
	    ret = reg_node(pRExC_state, REG_ANY);
	*flagp |= HASWIDTH|SIMPLE;
	RExC_naughty++;
        Set_Node_Length(ret, 1); /* MJD */
	break;
    case '[':
    {
	char * const oregcomp_parse = ++RExC_parse;
        ret = regclass(pRExC_state,depth+1);
	if (*RExC_parse != ']') {
	    RExC_parse = oregcomp_parse;
	    vFAIL("Unmatched [");
	}
	nextchar(pRExC_state);
	*flagp |= HASWIDTH|SIMPLE;
        Set_Node_Length(ret, RExC_parse - oregcomp_parse + 1); /* MJD */
	break;
    }
    case '(':
	nextchar(pRExC_state);
        ret = reg(pRExC_state, 1, &flags,depth+1);
	if (ret == NULL) {
		if (flags & TRYAGAIN) {
		    if (RExC_parse == RExC_end) {
			 /* Make parent create an empty node if needed. */
			*flagp |= TRYAGAIN;
			return(NULL);
		    }
		    goto tryagain;
		}
		return(NULL);
	}
	*flagp |= flags&(HASWIDTH|SPSTART|SIMPLE|POSTPONED);
	break;
    case '|':
    case ')':
	if (flags & TRYAGAIN) {
	    *flagp |= TRYAGAIN;
	    return NULL;
	}
	vFAIL("Internal urp");
				/* Supposed to be caught earlier. */
	break;
    case '{':
	if (!regcurly(RExC_parse)) {
	    RExC_parse++;
	    goto defchar;
	}
	/* FALL THROUGH */
    case '?':
    case '+':
    case '*':
	RExC_parse++;
	vFAIL("Quantifier follows nothing");
	break;
    case 0xDF:
    case 0xC3:
    case 0xCE:
        do_foldchar:
        if (!LOC && FOLD) {
            U32 len,cp;
	    len=0; /* silence a spurious compiler warning */
            if ((cp = what_len_TRICKYFOLD_safe(RExC_parse,RExC_end,UTF,len))) {
                *flagp |= HASWIDTH; /* could be SIMPLE too, but needs a handler in regexec.regrepeat */
                RExC_parse+=len-1; /* we get one from nextchar() as well. :-( */
                ret = reganode(pRExC_state, FOLDCHAR, cp);
                Set_Node_Length(ret, 1); /* MJD */
                nextchar(pRExC_state); /* kill whitespace under /x */
                return ret;
            }
        }
        goto outer_default;
    case '\\':
	/* Special Escapes

	   This switch handles escape sequences that resolve to some kind
	   of special regop and not to literal text. Escape sequnces that
	   resolve to literal text are handled below in the switch marked
	   "Literal Escapes".

	   Every entry in this switch *must* have a corresponding entry
	   in the literal escape switch. However, the opposite is not
	   required, as the default for this switch is to jump to the
	   literal text handling code.
	*/
	switch ((U8)*++RExC_parse) {
	case 0xDF:
	case 0xC3:
	case 0xCE:
	           goto do_foldchar;	    
	/* Special Escapes */
	case 'A':
	    RExC_seen_zerolen++;
	    ret = reg_node(pRExC_state, SBOL);
	    *flagp |= SIMPLE;
	    goto finish_meta_pat;
	case 'G':
	    ret = reg_node(pRExC_state, GPOS);
	    RExC_seen |= REG_SEEN_GPOS;
	    *flagp |= SIMPLE;
	    goto finish_meta_pat;
	case 'K':
	    RExC_seen_zerolen++;
	    ret = reg_node(pRExC_state, KEEPS);
	    *flagp |= SIMPLE;
	    /* XXX:dmq : disabling in-place substitution seems to
	     * be necessary here to avoid cases of memory corruption, as
	     * with: C<$_="x" x 80; s/x\K/y/> -- rgs
	     */
	    RExC_seen |= REG_SEEN_LOOKBEHIND;
	    goto finish_meta_pat;
	case 'Z':
	    ret = reg_node(pRExC_state, SEOL);
	    *flagp |= SIMPLE;
	    RExC_seen_zerolen++;		/* Do not optimize RE away */
	    goto finish_meta_pat;
	case 'z':
	    ret = reg_node(pRExC_state, EOS);
	    *flagp |= SIMPLE;
	    RExC_seen_zerolen++;		/* Do not optimize RE away */
	    goto finish_meta_pat;
	case 'C':
	    ret = reg_node(pRExC_state, CANY);
	    RExC_seen |= REG_SEEN_CANY;
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'X':
	    ret = reg_node(pRExC_state, CLUMP);
	    *flagp |= HASWIDTH;
	    goto finish_meta_pat;
	case 'w':
	    ret = reg_node(pRExC_state, (U8)(LOC ? ALNUML     : ALNUM));
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'W':
	    ret = reg_node(pRExC_state, (U8)(LOC ? NALNUML    : NALNUM));
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'b':
	    RExC_seen_zerolen++;
	    RExC_seen |= REG_SEEN_LOOKBEHIND;
	    ret = reg_node(pRExC_state, (U8)(LOC ? BOUNDL     : BOUND));
	    *flagp |= SIMPLE;
	    goto finish_meta_pat;
	case 'B':
	    RExC_seen_zerolen++;
	    RExC_seen |= REG_SEEN_LOOKBEHIND;
	    ret = reg_node(pRExC_state, (U8)(LOC ? NBOUNDL    : NBOUND));
	    *flagp |= SIMPLE;
	    goto finish_meta_pat;
	case 's':
	    ret = reg_node(pRExC_state, (U8)(LOC ? SPACEL     : SPACE));
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'S':
	    ret = reg_node(pRExC_state, (U8)(LOC ? NSPACEL    : NSPACE));
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'd':
	    ret = reg_node(pRExC_state, DIGIT);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'D':
	    ret = reg_node(pRExC_state, NDIGIT);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'R':
	    ret = reg_node(pRExC_state, LNBREAK);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'h':
	    ret = reg_node(pRExC_state, HORIZWS);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'H':
	    ret = reg_node(pRExC_state, NHORIZWS);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'v':
	    ret = reg_node(pRExC_state, VERTWS);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;
	case 'V':
	    ret = reg_node(pRExC_state, NVERTWS);
	    *flagp |= HASWIDTH|SIMPLE;
         finish_meta_pat:	    
	    nextchar(pRExC_state);
            Set_Node_Length(ret, 2); /* MJD */
	    break;	    
	case 'p':
	case 'P':
	    {	
		char* const oldregxend = RExC_end;
#ifdef DEBUGGING
		char* parse_start = RExC_parse - 2;
#endif

		if (RExC_parse[1] == '{') {
		  /* a lovely hack--pretend we saw [\pX] instead */
		    RExC_end = strchr(RExC_parse, '}');
		    if (!RExC_end) {
		        const U8 c = (U8)*RExC_parse;
			RExC_parse += 2;
			RExC_end = oldregxend;
			vFAIL2("Missing right brace on \\%c{}", c);
		    }
		    RExC_end++;
		}
		else {
		    RExC_end = RExC_parse + 2;
		    if (RExC_end > oldregxend)
			RExC_end = oldregxend;
		}
		RExC_parse--;

                ret = regclass(pRExC_state,depth+1);

		RExC_end = oldregxend;
		RExC_parse--;

		Set_Node_Offset(ret, parse_start + 2);
		Set_Node_Cur_Length(ret);
		nextchar(pRExC_state);
		*flagp |= HASWIDTH|SIMPLE;
	    }
	    break;
        case 'N': 
            /* Handle \N and \N{NAME} here and not below because it can be
            multicharacter. join_exact() will join them up later on. 
            Also this makes sure that things like /\N{BLAH}+/ and 
            \N{BLAH} being multi char Just Happen. dmq*/
            ++RExC_parse;
            ret= reg_namedseq(pRExC_state, NULL, flagp); 
            break;
	case 'k':    /* Handle \k<NAME> and \k'NAME' */
	parse_named_seq:
        {   
            char ch= RExC_parse[1];	    
	    if (ch != '<' && ch != '\'' && ch != '{') {
	        RExC_parse++;
	        vFAIL2("Sequence %.2s... not terminated",parse_start);
	    } else {
	        /* this pretty much dupes the code for (?P=...) in reg(), if
                   you change this make sure you change that */
		char* name_start = (RExC_parse += 2);
		U32 num = 0;
                SV *sv_dat = reg_scan_name(pRExC_state,
                    SIZE_ONLY ? REG_RSN_RETURN_NULL : REG_RSN_RETURN_DATA);
                ch= (ch == '<') ? '>' : (ch == '{') ? '}' : '\'';
                if (RExC_parse == name_start || *RExC_parse != ch)
                    vFAIL2("Sequence %.3s... not terminated",parse_start);

                if (!SIZE_ONLY) {
                    num = add_data( pRExC_state, 1, "S" );
                    RExC_rxi->data->data[num]=(void*)sv_dat;
                    SvREFCNT_inc_simple_void(sv_dat);
                }

                RExC_sawback = 1;
                ret = reganode(pRExC_state,
                	   (U8)(FOLD ? (LOC ? NREFFL : NREFF) : NREF),
                	   num);
                *flagp |= HASWIDTH;

                /* override incorrect value set in reganode MJD */
                Set_Node_Offset(ret, parse_start+1);
                Set_Node_Cur_Length(ret); /* MJD */
                nextchar(pRExC_state);

            }
            break;
	}
	case 'g': 
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    {
		I32 num;
		bool isg = *RExC_parse == 'g';
		bool isrel = 0; 
		bool hasbrace = 0;
		if (isg) {
		    RExC_parse++;
		    if (*RExC_parse == '{') {
		        RExC_parse++;
		        hasbrace = 1;
		    }
		    if (*RExC_parse == '-') {
		        RExC_parse++;
		        isrel = 1;
		    }
		    if (hasbrace && !isDIGIT(*RExC_parse)) {
		        if (isrel) RExC_parse--;
                        RExC_parse -= 2;		            
		        goto parse_named_seq;
		}   }
		num = atoi(RExC_parse);
		if (isg && num == 0)
		    vFAIL("Reference to invalid group 0");
                if (isrel) {
                    num = RExC_npar - num;
                    if (num < 1)
                        vFAIL("Reference to nonexistent or unclosed group");
                }
		if (!isg && num > 9 && num >= RExC_npar)
		    goto defchar;
		else {
		    char * const parse_start = RExC_parse - 1; /* MJD */
		    while (isDIGIT(*RExC_parse))
			RExC_parse++;
	            if (parse_start == RExC_parse - 1) 
	                vFAIL("Unterminated \\g... pattern");
                    if (hasbrace) {
                        if (*RExC_parse != '}') 
                            vFAIL("Unterminated \\g{...} pattern");
                        RExC_parse++;
                    }    
		    if (!SIZE_ONLY) {
		        if (num > (I32)RExC_rx->nparens)
			    vFAIL("Reference to nonexistent group");
		    }
		    RExC_sawback = 1;
		    ret = reganode(pRExC_state,
				   (U8)(FOLD ? (LOC ? REFFL : REFF) : REF),
				   num);
		    *flagp |= HASWIDTH;

                    /* override incorrect value set in reganode MJD */
                    Set_Node_Offset(ret, parse_start+1);
                    Set_Node_Cur_Length(ret); /* MJD */
		    RExC_parse--;
		    nextchar(pRExC_state);
		}
	    }
	    break;
	case '\0':
	    if (RExC_parse >= RExC_end)
		FAIL("Trailing \\");
	    /* FALL THROUGH */
	default:
	    /* Do not generate "unrecognized" warnings here, we fall
	       back into the quick-grab loop below */
	    parse_start--;
	    goto defchar;
	}
	break;

    case '#':
	if (RExC_flags & RXf_PMf_EXTENDED) {
	    if ( reg_skipcomment( pRExC_state ) )
		goto tryagain;
	}
	/* FALL THROUGH */

    default:
        outer_default:{
	    register STRLEN len;
	    register UV ender;
	    register char *p;
	    char *s;
	    STRLEN foldlen;
	    U8 tmpbuf[UTF8_MAXBYTES_CASE+1], *foldbuf;

            parse_start = RExC_parse - 1;

	    RExC_parse++;

	defchar:
	    ender = 0;
	    ret = reg_node(pRExC_state,
			   (U8)(FOLD ? (LOC ? EXACTFL : EXACTF) : EXACT));
	    s = STRING(ret);
	    for (len = 0, p = RExC_parse - 1;
	      len < 127 && p < RExC_end;
	      len++)
	    {
		char * const oldp = p;

		if (RExC_flags & RXf_PMf_EXTENDED)
		    p = regwhite( pRExC_state, p );
		switch ((U8)*p) {
		case 0xDF:
		case 0xC3:
		case 0xCE:
		           if (LOC || !FOLD || !is_TRICKYFOLD_safe(p,RExC_end,UTF))
		                goto normal_default;
		case '^':
		case '$':
		case '.':
		case '[':
		case '(':
		case ')':
		case '|':
		    goto loopdone;
		case '\\':
		    /* Literal Escapes Switch

		       This switch is meant to handle escape sequences that
		       resolve to a literal character.

		       Every escape sequence that represents something
		       else, like an assertion or a char class, is handled
		       in the switch marked 'Special Escapes' above in this
		       routine, but also has an entry here as anything that
		       isn't explicitly mentioned here will be treated as
		       an unescaped equivalent literal.
		    */

		    switch ((U8)*++p) {
		    /* These are all the special escapes. */
    		    case 0xDF:
    		    case 0xC3:
    		    case 0xCE:
    		           if (LOC || !FOLD || !is_TRICKYFOLD_safe(p,RExC_end,UTF))
    		                goto normal_default;		    
		    case 'A':             /* Start assertion */
		    case 'b': case 'B':   /* Word-boundary assertion*/
		    case 'C':             /* Single char !DANGEROUS! */
		    case 'd': case 'D':   /* digit class */
		    case 'g': case 'G':   /* generic-backref, pos assertion */
		    case 'h': case 'H':   /* HORIZWS */
		    case 'k': case 'K':   /* named backref, keep marker */
		    case 'N':             /* named char sequence */
		    case 'p': case 'P':   /* Unicode property */
		              case 'R':   /* LNBREAK */
		    case 's': case 'S':   /* space class */
		    case 'v': case 'V':   /* VERTWS */
		    case 'w': case 'W':   /* word class */
		    case 'X':             /* eXtended Unicode "combining character sequence" */
		    case 'z': case 'Z':   /* End of line/string assertion */
			--p;
			goto loopdone;

	            /* Anything after here is an escape that resolves to a
	               literal. (Except digits, which may or may not)
	             */
		    case 'n':
			ender = '\n';
			p++;
			break;
		    case 'r':
			ender = '\r';
			p++;
			break;
		    case 't':
			ender = '\t';
			p++;
			break;
		    case 'f':
			ender = '\f';
			p++;
			break;
		    case 'e':
			  ender = ASCII_TO_NATIVE('\033');
			p++;
			break;
		    case 'a':
			  ender = ASCII_TO_NATIVE('\007');
			p++;
			break;
		    case 'x':
			if (*++p == '{') {
			    char* const e = strchr(p, '}');
	
			    if (!e) {
				RExC_parse = p + 1;
				vFAIL("Missing right brace on \\x{}");
			    }
			    else {
                                I32 flags = PERL_SCAN_ALLOW_UNDERSCORES
                                    | PERL_SCAN_DISALLOW_PREFIX;
                                STRLEN numlen = e - p - 1;
				ender = grok_hex(p + 1, &numlen, &flags, NULL);
				if (ender > 0xff)
				    RExC_utf8 = 1;
				p = e + 1;
			    }
			}
			else {
                            I32 flags = PERL_SCAN_DISALLOW_PREFIX;
			    STRLEN numlen = 2;
			    ender = grok_hex(p, &numlen, &flags, NULL);
			    p += numlen;
			}
			if (PL_encoding && ender < 0x100)
			    goto recode_encoding;
			break;
		    case 'c':
			p++;
			ender = UCHARAT(p++);
			ender = toCTRL(ender);
			break;
		    case '0': case '1': case '2': case '3':case '4':
		    case '5': case '6': case '7': case '8':case '9':
			if (*p == '0' ||
			  (isDIGIT(p[1]) && atoi(p) >= RExC_npar) ) {
                            I32 flags = 0;
			    STRLEN numlen = 3;
			    ender = grok_oct(p, &numlen, &flags, NULL);

			    /* An octal above 0xff is interpreted differently
			     * depending on if the re is in utf8 or not.  If it
			     * is in utf8, the value will be itself, otherwise
			     * it is interpreted as modulo 0x100.  It has been
			     * decided to discourage the use of octal above the
			     * single-byte range.  For now, warn only when
			     * it ends up modulo */
			    if (SIZE_ONLY && ender >= 0x100
				    && ! UTF && ! PL_encoding) {
				ckWARNregdep(p, "Use of octal value above 377 is deprecated");
			    }
			    p += numlen;
			}
			else {
			    --p;
			    goto loopdone;
			}
			if (PL_encoding && ender < 0x100)
			    goto recode_encoding;
			break;
		    recode_encoding:
			{
			    SV* enc = PL_encoding;
			    ender = reg_recode((const char)(U8)ender, &enc);
			    if (!enc && SIZE_ONLY)
				ckWARNreg(p, "Invalid escape in the specified encoding");
			    RExC_utf8 = 1;
			}
			break;
		    case '\0':
			if (p >= RExC_end)
			    FAIL("Trailing \\");
			/* FALL THROUGH */
		    default:
			if (!SIZE_ONLY&& isALPHA(*p))
			    ckWARN2reg(p + 1, "Unrecognized escape \\%c passed through", UCHARAT(p));
			goto normal_default;
		    }
		    break;
		default:
		  normal_default:
		    if (UTF8_IS_START(*p) && UTF) {
			STRLEN numlen;
			ender = utf8n_to_uvchr((U8*)p, RExC_end - p,
					       &numlen, UTF8_ALLOW_DEFAULT);
			p += numlen;
		    }
		    else
			ender = *p++;
		    break;
		}
		if ( RExC_flags & RXf_PMf_EXTENDED)
		    p = regwhite( pRExC_state, p );
		if (UTF && FOLD) {
		    /* Prime the casefolded buffer. */
		    ender = toFOLD_uni(ender, tmpbuf, &foldlen);
		}
		if (p < RExC_end && ISMULT2(p)) { /* Back off on ?+*. */
		    if (len)
			p = oldp;
		    else if (UTF) {
			 if (FOLD) {
			      /* Emit all the Unicode characters. */
			      STRLEN numlen;
			      for (foldbuf = tmpbuf;
				   foldlen;
				   foldlen -= numlen) {
				   ender = utf8_to_uvchr(foldbuf, &numlen);
				   if (numlen > 0) {
					const STRLEN unilen = reguni(pRExC_state, ender, s);
					s       += unilen;
					len     += unilen;
					/* In EBCDIC the numlen
					 * and unilen can differ. */
					foldbuf += numlen;
					if (numlen >= foldlen)
					     break;
				   }
				   else
					break; /* "Can't happen." */
			      }
			 }
			 else {
			      const STRLEN unilen = reguni(pRExC_state, ender, s);
			      if (unilen > 0) {
				   s   += unilen;
				   len += unilen;
			      }
			 }
		    }
		    else {
			len++;
			REGC((char)ender, s++);
		    }
		    break;
		}
		if (UTF) {
		     if (FOLD) {
		          /* Emit all the Unicode characters. */
			  STRLEN numlen;
			  for (foldbuf = tmpbuf;
			       foldlen;
			       foldlen -= numlen) {
			       ender = utf8_to_uvchr(foldbuf, &numlen);
			       if (numlen > 0) {
				    const STRLEN unilen = reguni(pRExC_state, ender, s);
				    len     += unilen;
				    s       += unilen;
				    /* In EBCDIC the numlen
				     * and unilen can differ. */
				    foldbuf += numlen;
				    if (numlen >= foldlen)
					 break;
			       }
			       else
				    break;
			  }
		     }
		     else {
			  const STRLEN unilen = reguni(pRExC_state, ender, s);
			  if (unilen > 0) {
			       s   += unilen;
			       len += unilen;
			  }
		     }
		     len--;
		}
		else
		    REGC((char)ender, s++);
	    }
	loopdone:
	    RExC_parse = p - 1;
            Set_Node_Cur_Length(ret); /* MJD */
	    nextchar(pRExC_state);
	    {
		/* len is STRLEN which is unsigned, need to copy to signed */
		IV iv = len;
		if (iv < 0)
		    vFAIL("Internal disaster");
	    }
	    if (len > 0)
		*flagp |= HASWIDTH;
	    if (len == 1 && UNI_IS_INVARIANT(ender))
		*flagp |= SIMPLE;
		
	    if (SIZE_ONLY)
		RExC_size += STR_SZ(len);
	    else {
		STR_LEN(ret) = len;
		RExC_emit += STR_SZ(len);
            }
	}
	break;
    }

    return(ret);
}

STATIC char *
S_regwhite( RExC_state_t *pRExC_state, char *p )
{
    const char *e = RExC_end;

    PERL_ARGS_ASSERT_REGWHITE;

    while (p < e) {
	if (isSPACE(*p))
	    ++p;
	else if (*p == '#') {
            bool ended = 0;
	    do {
		if (*p++ == '\n') {
		    ended = 1;
		    break;
		}
	    } while (p < e);
	    if (!ended)
	        RExC_seen |= REG_SEEN_RUN_ON_COMMENT;
	}
	else
	    break;
    }
    return p;
}

/* Parse POSIX character classes: [[:foo:]], [[=foo=]], [[.foo.]].
   Character classes ([:foo:]) can also be negated ([:^foo:]).
   Returns a named class id (ANYOF_XXX) if successful, -1 otherwise.
   Equivalence classes ([=foo=]) and composites ([.foo.]) are parsed,
   but trigger failures because they are currently unimplemented. */

#define POSIXCC_DONE(c)   ((c) == ':')
#define POSIXCC_NOTYET(c) ((c) == '=' || (c) == '.')
#define POSIXCC(c) (POSIXCC_DONE(c) || POSIXCC_NOTYET(c))

STATIC I32
S_regpposixcc(pTHX_ RExC_state_t *pRExC_state, I32 value)
{
    dVAR;
    I32 namedclass = OOB_NAMEDCLASS;

    PERL_ARGS_ASSERT_REGPPOSIXCC;

    if (value == '[' && RExC_parse + 1 < RExC_end &&
	/* I smell either [: or [= or [. -- POSIX has been here, right? */
	POSIXCC(UCHARAT(RExC_parse))) {
	const char c = UCHARAT(RExC_parse);
	char* const s = RExC_parse++;
	
	while (RExC_parse < RExC_end && UCHARAT(RExC_parse) != c)
	    RExC_parse++;
	if (RExC_parse == RExC_end)
	    /* Grandfather lone [:, [=, [. */
	    RExC_parse = s;
	else {
	    const char* const t = RExC_parse++; /* skip over the c */
	    assert(*t == c);

  	    if (UCHARAT(RExC_parse) == ']') {
		const char *posixcc = s + 1;
  		RExC_parse++; /* skip over the ending ] */

		if (*s == ':') {
		    const I32 complement = *posixcc == '^' ? *posixcc++ : 0;
		    const I32 skip = t - posixcc;

		    /* Initially switch on the length of the name.  */
		    switch (skip) {
		    case 4:
			if (memEQ(posixcc, "word", 4)) /* this is not POSIX, this is the Perl \w */
			    namedclass = complement ? ANYOF_NALNUM : ANYOF_ALNUM;
			break;
		    case 5:
			/* Names all of length 5.  */
			/* alnum alpha ascii blank cntrl digit graph lower
			   print punct space upper  */
			/* Offset 4 gives the best switch position.  */
			switch (posixcc[4]) {
			case 'a':
			    if (memEQ(posixcc, "alph", 4)) /* alpha */
				namedclass = complement ? ANYOF_NALPHA : ANYOF_ALPHA;
			    break;
			case 'e':
			    if (memEQ(posixcc, "spac", 4)) /* space */
				namedclass = complement ? ANYOF_NPSXSPC : ANYOF_PSXSPC;
			    break;
			case 'h':
			    if (memEQ(posixcc, "grap", 4)) /* graph */
				namedclass = complement ? ANYOF_NGRAPH : ANYOF_GRAPH;
			    break;
			case 'i':
			    if (memEQ(posixcc, "asci", 4)) /* ascii */
				namedclass = complement ? ANYOF_NASCII : ANYOF_ASCII;
			    break;
			case 'k':
			    if (memEQ(posixcc, "blan", 4)) /* blank */
				namedclass = complement ? ANYOF_NBLANK : ANYOF_BLANK;
			    break;
			case 'l':
			    if (memEQ(posixcc, "cntr", 4)) /* cntrl */
				namedclass = complement ? ANYOF_NCNTRL : ANYOF_CNTRL;
			    break;
			case 'm':
			    if (memEQ(posixcc, "alnu", 4)) /* alnum */
				namedclass = complement ? ANYOF_NALNUMC : ANYOF_ALNUMC;
			    break;
			case 'r':
			    if (memEQ(posixcc, "lowe", 4)) /* lower */
				namedclass = complement ? ANYOF_NLOWER : ANYOF_LOWER;
			    else if (memEQ(posixcc, "uppe", 4)) /* upper */
				namedclass = complement ? ANYOF_NUPPER : ANYOF_UPPER;
			    break;
			case 't':
			    if (memEQ(posixcc, "digi", 4)) /* digit */
				namedclass = complement ? ANYOF_NDIGIT : ANYOF_DIGIT;
			    else if (memEQ(posixcc, "prin", 4)) /* print */
				namedclass = complement ? ANYOF_NPRINT : ANYOF_PRINT;
			    else if (memEQ(posixcc, "punc", 4)) /* punct */
				namedclass = complement ? ANYOF_NPUNCT : ANYOF_PUNCT;
			    break;
			}
			break;
		    case 6:
			if (memEQ(posixcc, "xdigit", 6))
			    namedclass = complement ? ANYOF_NXDIGIT : ANYOF_XDIGIT;
			break;
		    }

		    if (namedclass == OOB_NAMEDCLASS)
			Simple_vFAIL3("POSIX class [:%.*s:] unknown",
				      t - s - 1, s + 1);
		    assert (posixcc[skip] == ':');
		    assert (posixcc[skip+1] == ']');
		} else if (!SIZE_ONLY) {
		    /* [[=foo=]] and [[.foo.]] are still future. */

		    /* adjust RExC_parse so the warning shows after
		       the class closes */
		    while (UCHARAT(RExC_parse) && UCHARAT(RExC_parse) != ']')
			RExC_parse++;
		    Simple_vFAIL3("POSIX syntax [%c %c] is reserved for future extensions", c, c);
		}
	    } else {
		/* Maternal grandfather:
		 * "[:" ending in ":" but not in ":]" */
		RExC_parse = s;
	    }
	}
    }

    return namedclass;
}

STATIC void
S_checkposixcc(pTHX_ RExC_state_t *pRExC_state)
{
    dVAR;

    PERL_ARGS_ASSERT_CHECKPOSIXCC;

    if (POSIXCC(UCHARAT(RExC_parse))) {
	const char *s = RExC_parse;
	const char  c = *s++;

	while (isALNUM(*s))
	    s++;
	if (*s && c == *s && s[1] == ']') {
	    ckWARN3reg(s+2,
		       "POSIX syntax [%c %c] belongs inside character classes",
		       c, c);

	    /* [[=foo=]] and [[.foo.]] are still future. */
	    if (POSIXCC_NOTYET(c)) {
		/* adjust RExC_parse so the error shows after
		   the class closes */
		while (UCHARAT(RExC_parse) && UCHARAT(RExC_parse++) != ']')
		    NOOP;
		Simple_vFAIL3("POSIX syntax [%c %c] is reserved for future extensions", c, c);
	    }
	}
    }
}


#define _C_C_T_(NAME,TEST,WORD)                         \
ANYOF_##NAME:                                           \
    if (LOC)                                            \
	ANYOF_CLASS_SET(ret, ANYOF_##NAME);             \
    else {                                              \
	for (value = 0; value < 256; value++)           \
	    if (TEST)                                   \
		ANYOF_BITMAP_SET(ret, value);           \
    }                                                   \
    yesno = '+';                                        \
    what = WORD;                                        \
    break;                                              \
case ANYOF_N##NAME:                                     \
    if (LOC)                                            \
	ANYOF_CLASS_SET(ret, ANYOF_N##NAME);            \
    else {                                              \
	for (value = 0; value < 256; value++)           \
	    if (!TEST)                                  \
		ANYOF_BITMAP_SET(ret, value);           \
    }                                                   \
    yesno = '!';                                        \
    what = WORD;                                        \
    break

#define _C_C_T_NOLOC_(NAME,TEST,WORD)                   \
ANYOF_##NAME:                                           \
	for (value = 0; value < 256; value++)           \
	    if (TEST)                                   \
		ANYOF_BITMAP_SET(ret, value);           \
    yesno = '+';                                        \
    what = WORD;                                        \
    break;                                              \
case ANYOF_N##NAME:                                     \
	for (value = 0; value < 256; value++)           \
	    if (!TEST)                                  \
		ANYOF_BITMAP_SET(ret, value);           \
    yesno = '!';                                        \
    what = WORD;                                        \
    break

/* 
   We dont use PERL_LEGACY_UNICODE_CHARCLASS_MAPPINGS as the direct test
   so that it is possible to override the option here without having to 
   rebuild the entire core. as we are required to do if we change regcomp.h
   which is where PERL_LEGACY_UNICODE_CHARCLASS_MAPPINGS is defined.
*/
#if PERL_LEGACY_UNICODE_CHARCLASS_MAPPINGS
#define BROKEN_UNICODE_CHARCLASS_MAPPINGS
#endif

#ifdef BROKEN_UNICODE_CHARCLASS_MAPPINGS
#define POSIX_CC_UNI_NAME(CCNAME) CCNAME
#else
#define POSIX_CC_UNI_NAME(CCNAME) "Posix" CCNAME
#endif

/*
   parse a class specification and produce either an ANYOF node that
   matches the pattern or if the pattern matches a single char only and
   that char is < 256 and we are case insensitive then we produce an 
   EXACT node instead.
*/

STATIC regnode *
S_regclass(pTHX_ RExC_state_t *pRExC_state, U32 depth)
{
    dVAR;
    register UV nextvalue;
    register IV prevvalue = OOB_UNICODE;
    register IV range = 0;
    UV value = 0; /* XXX:dmq: needs to be referenceable (unfortunately) */
    register regnode *ret;
    STRLEN numlen;
    IV namedclass;
    char *rangebegin = NULL;
    bool need_class = 0;
    SV *listsv = NULL;
    UV n;
    bool optimize_invert   = TRUE;
    AV* unicode_alternate  = NULL;
#ifdef EBCDIC
    UV literal_endpoint = 0;
#endif
    UV stored = 0;  /* number of chars stored in the class */

    regnode * const orig_emit = RExC_emit; /* Save the original RExC_emit in
        case we need to change the emitted regop to an EXACT. */
    const char * orig_parse = RExC_parse;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGCLASS;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    DEBUG_PARSE("clas");

    /* Assume we are going to generate an ANYOF node. */
    ret = reganode(pRExC_state, ANYOF, 0);

    if (!SIZE_ONLY)
	ANYOF_FLAGS(ret) = 0;

    if (UCHARAT(RExC_parse) == '^') {	/* Complement of range. */
	RExC_naughty++;
	RExC_parse++;
	if (!SIZE_ONLY)
	    ANYOF_FLAGS(ret) |= ANYOF_INVERT;
    }

    if (SIZE_ONLY) {
	RExC_size += ANYOF_SKIP;
	listsv = &PL_sv_undef; /* For code scanners: listsv always non-NULL. */
    }
    else {
 	RExC_emit += ANYOF_SKIP;
	if (FOLD)
	    ANYOF_FLAGS(ret) |= ANYOF_FOLD;
	if (LOC)
	    ANYOF_FLAGS(ret) |= ANYOF_LOCALE;
	ANYOF_BITMAP_ZERO(ret);
	listsv = newSVpvs("# comment\n");
    }

    nextvalue = RExC_parse < RExC_end ? UCHARAT(RExC_parse) : 0;

    if (!SIZE_ONLY && POSIXCC(nextvalue))
	checkposixcc(pRExC_state);

    /* allow 1st char to be ] (allowing it to be - is dealt with later) */
    if (UCHARAT(RExC_parse) == ']')
	goto charclassloop;

parseit:
    while (RExC_parse < RExC_end && UCHARAT(RExC_parse) != ']') {

    charclassloop:

	namedclass = OOB_NAMEDCLASS; /* initialize as illegal */

	if (!range)
	    rangebegin = RExC_parse;
	if (UTF) {
	    value = utf8n_to_uvchr((U8*)RExC_parse,
				   RExC_end - RExC_parse,
				   &numlen, UTF8_ALLOW_DEFAULT);
	    RExC_parse += numlen;
	}
	else
	    value = UCHARAT(RExC_parse++);

	nextvalue = RExC_parse < RExC_end ? UCHARAT(RExC_parse) : 0;
	if (value == '[' && POSIXCC(nextvalue))
	    namedclass = regpposixcc(pRExC_state, value);
	else if (value == '\\') {
	    if (UTF) {
		value = utf8n_to_uvchr((U8*)RExC_parse,
				   RExC_end - RExC_parse,
				   &numlen, UTF8_ALLOW_DEFAULT);
		RExC_parse += numlen;
	    }
	    else
		value = UCHARAT(RExC_parse++);
	    /* Some compilers cannot handle switching on 64-bit integer
	     * values, therefore value cannot be an UV.  Yes, this will
	     * be a problem later if we want switch on Unicode.
	     * A similar issue a little bit later when switching on
	     * namedclass. --jhi */
	    switch ((I32)value) {
	    case 'w':	namedclass = ANYOF_ALNUM;	break;
	    case 'W':	namedclass = ANYOF_NALNUM;	break;
	    case 's':	namedclass = ANYOF_SPACE;	break;
	    case 'S':	namedclass = ANYOF_NSPACE;	break;
	    case 'd':	namedclass = ANYOF_DIGIT;	break;
	    case 'D':	namedclass = ANYOF_NDIGIT;	break;
	    case 'v':	namedclass = ANYOF_VERTWS;	break;
	    case 'V':	namedclass = ANYOF_NVERTWS;	break;
	    case 'h':	namedclass = ANYOF_HORIZWS;	break;
	    case 'H':	namedclass = ANYOF_NHORIZWS;	break;
            case 'N':  /* Handle \N{NAME} in class */
                {
                    /* We only pay attention to the first char of 
                    multichar strings being returned. I kinda wonder
                    if this makes sense as it does change the behaviour
                    from earlier versions, OTOH that behaviour was broken
                    as well. */
                    UV v; /* value is register so we cant & it /grrr */
                    if (reg_namedseq(pRExC_state, &v, NULL)) {
                        goto parseit;
                    }
                    value= v; 
                }
                break;
	    case 'p':
	    case 'P':
		{
		char *e;
		if (RExC_parse >= RExC_end)
		    vFAIL2("Empty \\%c{}", (U8)value);
		if (*RExC_parse == '{') {
		    const U8 c = (U8)value;
		    e = strchr(RExC_parse++, '}');
                    if (!e)
                        vFAIL2("Missing right brace on \\%c{}", c);
		    while (isSPACE(UCHARAT(RExC_parse)))
		        RExC_parse++;
                    if (e == RExC_parse)
                        vFAIL2("Empty \\%c{}", c);
		    n = e - RExC_parse;
		    while (isSPACE(UCHARAT(RExC_parse + n - 1)))
		        n--;
		}
		else {
		    e = RExC_parse;
		    n = 1;
		}
		if (!SIZE_ONLY) {
		    if (UCHARAT(RExC_parse) == '^') {
			 RExC_parse++;
			 n--;
			 value = value == 'p' ? 'P' : 'p'; /* toggle */
			 while (isSPACE(UCHARAT(RExC_parse))) {
			      RExC_parse++;
			      n--;
			 }
		    }
		    Perl_sv_catpvf(aTHX_ listsv, "%cutf8::%.*s\n",
			(value=='p' ? '+' : '!'), (int)n, RExC_parse);
		}
		RExC_parse = e + 1;
		ANYOF_FLAGS(ret) |= ANYOF_UNICODE;
		namedclass = ANYOF_MAX;  /* no official name, but it's named */
		}
		break;
	    case 'n':	value = '\n';			break;
	    case 'r':	value = '\r';			break;
	    case 't':	value = '\t';			break;
	    case 'f':	value = '\f';			break;
	    case 'b':	value = '\b';			break;
	    case 'e':	value = ASCII_TO_NATIVE('\033');break;
	    case 'a':	value = ASCII_TO_NATIVE('\007');break;
	    case 'x':
		if (*RExC_parse == '{') {
                    I32 flags = PERL_SCAN_ALLOW_UNDERSCORES
                        | PERL_SCAN_DISALLOW_PREFIX;
		    char * const e = strchr(RExC_parse++, '}');
                    if (!e)
                        vFAIL("Missing right brace on \\x{}");

		    numlen = e - RExC_parse;
		    value = grok_hex(RExC_parse, &numlen, &flags, NULL);
		    RExC_parse = e + 1;
		}
		else {
                    I32 flags = PERL_SCAN_DISALLOW_PREFIX;
		    numlen = 2;
		    value = grok_hex(RExC_parse, &numlen, &flags, NULL);
		    RExC_parse += numlen;
		}
		if (PL_encoding && value < 0x100)
		    goto recode_encoding;
		break;
	    case 'c':
		value = UCHARAT(RExC_parse++);
		value = toCTRL(value);
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		{
		    I32 flags = 0;
		    numlen = 3;
		    value = grok_oct(--RExC_parse, &numlen, &flags, NULL);
		    RExC_parse += numlen;
		    if (PL_encoding && value < 0x100)
			goto recode_encoding;
		    break;
		}
	    recode_encoding:
		{
		    SV* enc = PL_encoding;
		    value = reg_recode((const char)(U8)value, &enc);
		    if (!enc && SIZE_ONLY)
			ckWARNreg(RExC_parse,
				  "Invalid escape in the specified encoding");
		    break;
		}
	    default:
		if (!SIZE_ONLY && isALPHA(value))
		    ckWARN2reg(RExC_parse,
			       "Unrecognized escape \\%c in character class passed through",
			       (int)value);
		break;
	    }
	} /* end of \blah */
#ifdef EBCDIC
	else
	    literal_endpoint++;
#endif

	if (namedclass > OOB_NAMEDCLASS) { /* this is a named class \blah */

	    if (!SIZE_ONLY && !need_class)
		ANYOF_CLASS_ZERO(ret);

	    need_class = 1;

	    /* a bad range like a-\d, a-[:digit:] ? */
	    if (range) {
		if (!SIZE_ONLY) {
		    const int w =
			RExC_parse >= rangebegin ?
			RExC_parse - rangebegin : 0;
		    ckWARN4reg(RExC_parse,
			       "False [] range \"%*.*s\"",
			       w, w, rangebegin);

		    if (prevvalue < 256) {
			ANYOF_BITMAP_SET(ret, prevvalue);
			ANYOF_BITMAP_SET(ret, '-');
		    }
		    else {
			ANYOF_FLAGS(ret) |= ANYOF_UNICODE;
			Perl_sv_catpvf(aTHX_ listsv,
				       "%04"UVxf"\n%04"UVxf"\n", (UV)prevvalue, (UV) '-');
		    }
		}

		range = 0; /* this was not a true range */
	    }


    
	    if (!SIZE_ONLY) {
		const char *what = NULL;
		char yesno = 0;

	        if (namedclass > OOB_NAMEDCLASS)
		    optimize_invert = FALSE;
		/* Possible truncation here but in some 64-bit environments
		 * the compiler gets heartburn about switch on 64-bit values.
		 * A similar issue a little earlier when switching on value.
		 * --jhi */
		switch ((I32)namedclass) {
		
		case _C_C_T_(ALNUMC, isALNUMC(value), POSIX_CC_UNI_NAME("Alnum"));
		case _C_C_T_(ALPHA, isALPHA(value), POSIX_CC_UNI_NAME("Alpha"));
		case _C_C_T_(BLANK, isBLANK(value), POSIX_CC_UNI_NAME("Blank"));
		case _C_C_T_(CNTRL, isCNTRL(value), POSIX_CC_UNI_NAME("Cntrl"));
		case _C_C_T_(GRAPH, isGRAPH(value), POSIX_CC_UNI_NAME("Graph"));
		case _C_C_T_(LOWER, isLOWER(value), POSIX_CC_UNI_NAME("Lower"));
		case _C_C_T_(PRINT, isPRINT(value), POSIX_CC_UNI_NAME("Print"));
		case _C_C_T_(PSXSPC, isPSXSPC(value), POSIX_CC_UNI_NAME("Space"));
		case _C_C_T_(PUNCT, isPUNCT(value), POSIX_CC_UNI_NAME("Punct"));
		case _C_C_T_(UPPER, isUPPER(value), POSIX_CC_UNI_NAME("Upper"));
#ifdef BROKEN_UNICODE_CHARCLASS_MAPPINGS
		case _C_C_T_(ALNUM, isALNUM(value), "Word");
		case _C_C_T_(SPACE, isSPACE(value), "SpacePerl");
#else
		case _C_C_T_(SPACE, isSPACE(value), "PerlSpace");
		case _C_C_T_(ALNUM, isALNUM(value), "PerlWord");
#endif		
		case _C_C_T_(XDIGIT, isXDIGIT(value), "XDigit");
		case _C_C_T_NOLOC_(VERTWS, is_VERTWS_latin1(&value), "VertSpace");
		case _C_C_T_NOLOC_(HORIZWS, is_HORIZWS_latin1(&value), "HorizSpace");
		case ANYOF_ASCII:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_ASCII);
		    else {
#ifndef EBCDIC
			for (value = 0; value < 128; value++)
			    ANYOF_BITMAP_SET(ret, value);
#else  /* EBCDIC */
			for (value = 0; value < 256; value++) {
			    if (isASCII(value))
			        ANYOF_BITMAP_SET(ret, value);
			}
#endif /* EBCDIC */
		    }
		    yesno = '+';
		    what = "ASCII";
		    break;
		case ANYOF_NASCII:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NASCII);
		    else {
#ifndef EBCDIC
			for (value = 128; value < 256; value++)
			    ANYOF_BITMAP_SET(ret, value);
#else  /* EBCDIC */
			for (value = 0; value < 256; value++) {
			    if (!isASCII(value))
			        ANYOF_BITMAP_SET(ret, value);
			}
#endif /* EBCDIC */
		    }
		    yesno = '!';
		    what = "ASCII";
		    break;		
		case ANYOF_DIGIT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_DIGIT);
		    else {
			/* consecutive digits assumed */
			for (value = '0'; value <= '9'; value++)
			    ANYOF_BITMAP_SET(ret, value);
		    }
		    yesno = '+';
		    what = POSIX_CC_UNI_NAME("Digit");
		    break;
		case ANYOF_NDIGIT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NDIGIT);
		    else {
			/* consecutive digits assumed */
			for (value = 0; value < '0'; value++)
			    ANYOF_BITMAP_SET(ret, value);
			for (value = '9' + 1; value < 256; value++)
			    ANYOF_BITMAP_SET(ret, value);
		    }
		    yesno = '!';
		    what = POSIX_CC_UNI_NAME("Digit");
		    break;		
		case ANYOF_MAX:
		    /* this is to handle \p and \P */
		    break;
		default:
		    vFAIL("Invalid [::] class");
		    break;
		}
		if (what) {
		    /* Strings such as "+utf8::isWord\n" */
		    Perl_sv_catpvf(aTHX_ listsv, "%cutf8::Is%s\n", yesno, what);
		}
		if (LOC)
		    ANYOF_FLAGS(ret) |= ANYOF_CLASS;
		continue;
	    }
	} /* end of namedclass \blah */

	if (range) {
	    if (prevvalue > (IV)value) /* b-a */ {
		const int w = RExC_parse - rangebegin;
		Simple_vFAIL4("Invalid [] range \"%*.*s\"", w, w, rangebegin);
		range = 0; /* not a valid range */
	    }
	}
	else {
	    prevvalue = value; /* save the beginning of the range */
	    if (*RExC_parse == '-' && RExC_parse+1 < RExC_end &&
		RExC_parse[1] != ']') {
		RExC_parse++;

		/* a bad range like \w-, [:word:]- ? */
		if (namedclass > OOB_NAMEDCLASS) {
		    if (ckWARN(WARN_REGEXP)) {
			const int w =
			    RExC_parse >= rangebegin ?
			    RExC_parse - rangebegin : 0;
			vWARN4(RExC_parse,
			       "False [] range \"%*.*s\"",
			       w, w, rangebegin);
		    }
		    if (!SIZE_ONLY)
			ANYOF_BITMAP_SET(ret, '-');
		} else
		    range = 1;	/* yeah, it's a range! */
		continue;	/* but do it the next time */
	    }
	}

	/* now is the next time */
        /*stored += (value - prevvalue + 1);*/
	if (!SIZE_ONLY) {
	    if (prevvalue < 256) {
	        const IV ceilvalue = value < 256 ? value : 255;
		IV i;
#ifdef EBCDIC
		/* In EBCDIC [\x89-\x91] should include
		 * the \x8e but [i-j] should not. */
		if (literal_endpoint == 2 &&
		    ((isLOWER(prevvalue) && isLOWER(ceilvalue)) ||
		     (isUPPER(prevvalue) && isUPPER(ceilvalue))))
		{
		    if (isLOWER(prevvalue)) {
			for (i = prevvalue; i <= ceilvalue; i++)
			    if (isLOWER(i) && !ANYOF_BITMAP_TEST(ret,i)) {
				stored++;
				ANYOF_BITMAP_SET(ret, i);
			    }
		    } else {
			for (i = prevvalue; i <= ceilvalue; i++)
			    if (isUPPER(i) && !ANYOF_BITMAP_TEST(ret,i)) {
				stored++;
				ANYOF_BITMAP_SET(ret, i);
			    }
		    }
		}
		else
#endif
		      for (i = prevvalue; i <= ceilvalue; i++) {
		        if (!ANYOF_BITMAP_TEST(ret,i)) {
		            stored++;  
			    ANYOF_BITMAP_SET(ret, i);
		        }
	              }
	  }
	  if (value > 255 || UTF) {
	        const UV prevnatvalue  = NATIVE_TO_UNI(prevvalue);
		const UV natvalue      = NATIVE_TO_UNI(value);
                stored+=2; /* can't optimize this class */
		ANYOF_FLAGS(ret) |= ANYOF_UNICODE;
		if (prevnatvalue < natvalue) { /* what about > ? */
		    Perl_sv_catpvf(aTHX_ listsv, "%04"UVxf"\t%04"UVxf"\n",
				   prevnatvalue, natvalue);
		}
		else if (prevnatvalue == natvalue) {
		    Perl_sv_catpvf(aTHX_ listsv, "%04"UVxf"\n", natvalue);
		    if (FOLD) {
			 U8 foldbuf[UTF8_MAXBYTES_CASE+1];
			 STRLEN foldlen;
			 const UV f = to_uni_fold(natvalue, foldbuf, &foldlen);

#ifdef EBCDIC /* RD t/uni/fold ff and 6b */
			 if (RExC_precomp[0] == ':' &&
			     RExC_precomp[1] == '[' &&
			     (f == 0xDF || f == 0x92)) {
			     f = NATIVE_TO_UNI(f);
                        }
#endif
			 /* If folding and foldable and a single
			  * character, insert also the folded version
			  * to the charclass. */
			 if (f != value) {
#ifdef EBCDIC /* RD tunifold ligatures s,t fb05, fb06 */
			     if ((RExC_precomp[0] == ':' &&
				  RExC_precomp[1] == '[' &&
				  (f == 0xA2 &&
				   (value == 0xFB05 || value == 0xFB06))) ?
				 foldlen == ((STRLEN)UNISKIP(f) - 1) :
				 foldlen == (STRLEN)UNISKIP(f) )
#else
			      if (foldlen == (STRLEN)UNISKIP(f))
#endif
				  Perl_sv_catpvf(aTHX_ listsv,
						 "%04"UVxf"\n", f);
			      else {
				  /* Any multicharacter foldings
				   * require the following transform:
				   * [ABCDEF] -> (?:[ABCabcDEFd]|pq|rst)
				   * where E folds into "pq" and F folds
				   * into "rst", all other characters
				   * fold to single characters.  We save
				   * away these multicharacter foldings,
				   * to be later saved as part of the
				   * additional "s" data. */
				  SV *sv;

				  if (!unicode_alternate)
				      unicode_alternate = newAV();
				  sv = newSVpvn_utf8((char*)foldbuf, foldlen,
						     TRUE);
				  av_push(unicode_alternate, sv);
			      }
			 }

			 /* If folding and the value is one of the Greek
			  * sigmas insert a few more sigmas to make the
			  * folding rules of the sigmas to work right.
			  * Note that not all the possible combinations
			  * are handled here: some of them are handled
			  * by the standard folding rules, and some of
			  * them (literal or EXACTF cases) are handled
			  * during runtime in regexec.c:S_find_byclass(). */
			 if (value == UNICODE_GREEK_SMALL_LETTER_FINAL_SIGMA) {
			      Perl_sv_catpvf(aTHX_ listsv, "%04"UVxf"\n",
					     (UV)UNICODE_GREEK_CAPITAL_LETTER_SIGMA);
			      Perl_sv_catpvf(aTHX_ listsv, "%04"UVxf"\n",
					     (UV)UNICODE_GREEK_SMALL_LETTER_SIGMA);
			 }
			 else if (value == UNICODE_GREEK_CAPITAL_LETTER_SIGMA)
			      Perl_sv_catpvf(aTHX_ listsv, "%04"UVxf"\n",
					     (UV)UNICODE_GREEK_SMALL_LETTER_SIGMA);
		    }
		}
	    }
#ifdef EBCDIC
	    literal_endpoint = 0;
#endif
        }

	range = 0; /* this range (if it was one) is done now */
    }

    if (need_class) {
	ANYOF_FLAGS(ret) |= ANYOF_LARGE;
	if (SIZE_ONLY)
	    RExC_size += ANYOF_CLASS_ADD_SKIP;
	else
	    RExC_emit += ANYOF_CLASS_ADD_SKIP;
    }


    if (SIZE_ONLY)
        return ret;
    /****** !SIZE_ONLY AFTER HERE *********/

    if( stored == 1 && (value < 128 || (value < 256 && !UTF))
        && !( ANYOF_FLAGS(ret) & ( ANYOF_FLAGS_ALL ^ ANYOF_FOLD ) )
    ) {
        /* optimize single char class to an EXACT node
           but *only* when its not a UTF/high char  */
        const char * cur_parse= RExC_parse;
        RExC_emit = (regnode *)orig_emit;
        RExC_parse = (char *)orig_parse;
        ret = reg_node(pRExC_state,
                       (U8)((ANYOF_FLAGS(ret) & ANYOF_FOLD) ? EXACTF : EXACT));
        RExC_parse = (char *)cur_parse;
        *STRING(ret)= (char)value;
        STR_LEN(ret)= 1;
        RExC_emit += STR_SZ(1);
	SvREFCNT_dec(listsv);
        return ret;
    }
    /* optimize case-insensitive simple patterns (e.g. /[a-z]/i) */
    if ( /* If the only flag is folding (plus possibly inversion). */
	((ANYOF_FLAGS(ret) & (ANYOF_FLAGS_ALL ^ ANYOF_INVERT)) == ANYOF_FOLD)
       ) {
	for (value = 0; value < 256; ++value) {
	    if (ANYOF_BITMAP_TEST(ret, value)) {
		UV fold = PL_fold[value];

		if (fold != value)
		    ANYOF_BITMAP_SET(ret, fold);
	    }
	}
	ANYOF_FLAGS(ret) &= ~ANYOF_FOLD;
    }

    /* optimize inverted simple patterns (e.g. [^a-z]) */
    if (optimize_invert &&
	/* If the only flag is inversion. */
	(ANYOF_FLAGS(ret) & ANYOF_FLAGS_ALL) ==	ANYOF_INVERT) {
	for (value = 0; value < ANYOF_BITMAP_SIZE; ++value)
	    ANYOF_BITMAP(ret)[value] ^= ANYOF_FLAGS_ALL;
	ANYOF_FLAGS(ret) = ANYOF_UNICODE_ALL;
    }
    {
	AV * const av = newAV();
	SV *rv;
	/* The 0th element stores the character class description
	 * in its textual form: used later (regexec.c:Perl_regclass_swash())
	 * to initialize the appropriate swash (which gets stored in
	 * the 1st element), and also useful for dumping the regnode.
	 * The 2nd element stores the multicharacter foldings,
	 * used later (regexec.c:S_reginclass()). */
	av_store(av, 0, listsv);
	av_store(av, 1, NULL);
	av_store(av, 2, MUTABLE_SV(unicode_alternate));
	rv = newRV_noinc(MUTABLE_SV(av));
	n = add_data(pRExC_state, 1, "s");
	RExC_rxi->data->data[n] = (void*)rv;
	ARG_SET(ret, n);
    }
    return ret;
}
#undef _C_C_T_


/* reg_skipcomment()

   Absorbs an /x style # comments from the input stream.
   Returns true if there is more text remaining in the stream.
   Will set the REG_SEEN_RUN_ON_COMMENT flag if the comment
   terminates the pattern without including a newline.

   Note its the callers responsibility to ensure that we are
   actually in /x mode

*/

STATIC bool
S_reg_skipcomment(pTHX_ RExC_state_t *pRExC_state)
{
    bool ended = 0;

    PERL_ARGS_ASSERT_REG_SKIPCOMMENT;

    while (RExC_parse < RExC_end)
        if (*RExC_parse++ == '\n') {
            ended = 1;
            break;
        }
    if (!ended) {
        /* we ran off the end of the pattern without ending
           the comment, so we have to add an \n when wrapping */
        RExC_seen |= REG_SEEN_RUN_ON_COMMENT;
        return 0;
    } else
        return 1;
}

/* nextchar()

   Advance that parse position, and optionall absorbs
   "whitespace" from the inputstream.

   Without /x "whitespace" means (?#...) style comments only,
   with /x this means (?#...) and # comments and whitespace proper.

   Returns the RExC_parse point from BEFORE the scan occurs.

   This is the /x friendly way of saying RExC_parse++.
*/

STATIC char*
S_nextchar(pTHX_ RExC_state_t *pRExC_state)
{
    char* const retval = RExC_parse++;

    PERL_ARGS_ASSERT_NEXTCHAR;

    for (;;) {
	if (*RExC_parse == '(' && RExC_parse[1] == '?' &&
		RExC_parse[2] == '#') {
	    while (*RExC_parse != ')') {
		if (RExC_parse == RExC_end)
		    FAIL("Sequence (?#... not terminated");
		RExC_parse++;
	    }
	    RExC_parse++;
	    continue;
	}
	if (RExC_flags & RXf_PMf_EXTENDED) {
	    if (isSPACE(*RExC_parse)) {
		RExC_parse++;
		continue;
	    }
	    else if (*RExC_parse == '#') {
	        if ( reg_skipcomment( pRExC_state ) )
	            continue;
	    }
	}
	return retval;
    }
}

/*
- reg_node - emit a node
*/
STATIC regnode *			/* Location. */
S_reg_node(pTHX_ RExC_state_t *pRExC_state, U8 op)
{
    dVAR;
    register regnode *ptr;
    regnode * const ret = RExC_emit;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REG_NODE;

    if (SIZE_ONLY) {
	SIZE_ALIGN(RExC_size);
	RExC_size += 1;
	return(ret);
    }
    if (RExC_emit >= RExC_emit_bound)
        Perl_croak(aTHX_ "panic: reg_node overrun trying to emit %d", op);

    NODE_ALIGN_FILL(ret);
    ptr = ret;
    FILL_ADVANCE_NODE(ptr, op);
#ifdef RE_TRACK_PATTERN_OFFSETS
    if (RExC_offsets) {         /* MJD */
	MJD_OFFSET_DEBUG(("%s:%d: (op %s) %s %"UVuf" (len %"UVuf") (max %"UVuf").\n", 
              "reg_node", __LINE__, 
              PL_reg_name[op],
              (UV)(RExC_emit - RExC_emit_start) > RExC_offsets[0] 
		? "Overwriting end of array!\n" : "OK",
              (UV)(RExC_emit - RExC_emit_start),
              (UV)(RExC_parse - RExC_start),
              (UV)RExC_offsets[0])); 
	Set_Node_Offset(RExC_emit, RExC_parse + (op == END));
    }
#endif
    RExC_emit = ptr;
    return(ret);
}

/*
- reganode - emit a node with an argument
*/
STATIC regnode *			/* Location. */
S_reganode(pTHX_ RExC_state_t *pRExC_state, U8 op, U32 arg)
{
    dVAR;
    register regnode *ptr;
    regnode * const ret = RExC_emit;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGANODE;

    if (SIZE_ONLY) {
	SIZE_ALIGN(RExC_size);
	RExC_size += 2;
	/* 
	   We can't do this:
	   
	   assert(2==regarglen[op]+1); 
	
	   Anything larger than this has to allocate the extra amount.
	   If we changed this to be:
	   
	   RExC_size += (1 + regarglen[op]);
	   
	   then it wouldn't matter. Its not clear what side effect
	   might come from that so its not done so far.
	   -- dmq
	*/
	return(ret);
    }
    if (RExC_emit >= RExC_emit_bound)
        Perl_croak(aTHX_ "panic: reg_node overrun trying to emit %d", op);

    NODE_ALIGN_FILL(ret);
    ptr = ret;
    FILL_ADVANCE_NODE_ARG(ptr, op, arg);
#ifdef RE_TRACK_PATTERN_OFFSETS
    if (RExC_offsets) {         /* MJD */
	MJD_OFFSET_DEBUG(("%s(%d): (op %s) %s %"UVuf" <- %"UVuf" (max %"UVuf").\n", 
              "reganode",
	      __LINE__,
	      PL_reg_name[op],
              (UV)(RExC_emit - RExC_emit_start) > RExC_offsets[0] ? 
              "Overwriting end of array!\n" : "OK",
              (UV)(RExC_emit - RExC_emit_start),
              (UV)(RExC_parse - RExC_start),
              (UV)RExC_offsets[0])); 
	Set_Cur_Node_Offset;
    }
#endif            
    RExC_emit = ptr;
    return(ret);
}

/*
- reguni - emit (if appropriate) a Unicode character
*/
STATIC STRLEN
S_reguni(pTHX_ const RExC_state_t *pRExC_state, UV uv, char* s)
{
    dVAR;

    PERL_ARGS_ASSERT_REGUNI;

    return SIZE_ONLY ? UNISKIP(uv) : (uvchr_to_utf8((U8*)s, uv) - (U8*)s);
}

/*
- reginsert - insert an operator in front of already-emitted operand
*
* Means relocating the operand.
*/
STATIC void
S_reginsert(pTHX_ RExC_state_t *pRExC_state, U8 op, regnode *opnd, U32 depth)
{
    dVAR;
    register regnode *src;
    register regnode *dst;
    register regnode *place;
    const int offset = regarglen[(U8)op];
    const int size = NODE_STEP_REGNODE + offset;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGINSERT;
    PERL_UNUSED_ARG(depth);
/* (PL_regkind[(U8)op] == CURLY ? EXTRA_STEP_2ARGS : 0); */
    DEBUG_PARSE_FMT("inst"," - %s",PL_reg_name[op]);
    if (SIZE_ONLY) {
	RExC_size += size;
	return;
    }

    src = RExC_emit;
    RExC_emit += size;
    dst = RExC_emit;
    if (RExC_open_parens) {
        int paren;
        /*DEBUG_PARSE_FMT("inst"," - %"IVdf, (IV)RExC_npar);*/
        for ( paren=0 ; paren < RExC_npar ; paren++ ) {
            if ( RExC_open_parens[paren] >= opnd ) {
                /*DEBUG_PARSE_FMT("open"," - %d",size);*/
                RExC_open_parens[paren] += size;
            } else {
                /*DEBUG_PARSE_FMT("open"," - %s","ok");*/
            }
            if ( RExC_close_parens[paren] >= opnd ) {
                /*DEBUG_PARSE_FMT("close"," - %d",size);*/
                RExC_close_parens[paren] += size;
            } else {
                /*DEBUG_PARSE_FMT("close"," - %s","ok");*/
            }
        }
    }

    while (src > opnd) {
	StructCopy(--src, --dst, regnode);
#ifdef RE_TRACK_PATTERN_OFFSETS
        if (RExC_offsets) {     /* MJD 20010112 */
	    MJD_OFFSET_DEBUG(("%s(%d): (op %s) %s copy %"UVuf" -> %"UVuf" (max %"UVuf").\n",
                  "reg_insert",
		  __LINE__,
		  PL_reg_name[op],
                  (UV)(dst - RExC_emit_start) > RExC_offsets[0] 
		    ? "Overwriting end of array!\n" : "OK",
                  (UV)(src - RExC_emit_start),
                  (UV)(dst - RExC_emit_start),
                  (UV)RExC_offsets[0])); 
	    Set_Node_Offset_To_R(dst-RExC_emit_start, Node_Offset(src));
	    Set_Node_Length_To_R(dst-RExC_emit_start, Node_Length(src));
        }
#endif
    }
    

    place = opnd;		/* Op node, where operand used to be. */
#ifdef RE_TRACK_PATTERN_OFFSETS
    if (RExC_offsets) {         /* MJD */
	MJD_OFFSET_DEBUG(("%s(%d): (op %s) %s %"UVuf" <- %"UVuf" (max %"UVuf").\n", 
              "reginsert",
	      __LINE__,
	      PL_reg_name[op],
              (UV)(place - RExC_emit_start) > RExC_offsets[0] 
              ? "Overwriting end of array!\n" : "OK",
              (UV)(place - RExC_emit_start),
              (UV)(RExC_parse - RExC_start),
              (UV)RExC_offsets[0]));
	Set_Node_Offset(place, RExC_parse);
	Set_Node_Length(place, 1);
    }
#endif    
    src = NEXTOPER(place);
    FILL_ADVANCE_NODE(place, op);
    Zero(src, offset, regnode);
}

/*
- regtail - set the next-pointer at the end of a node chain of p to val.
- SEE ALSO: regtail_study
*/
/* TODO: All three parms should be const */
STATIC void
S_regtail(pTHX_ RExC_state_t *pRExC_state, regnode *p, const regnode *val,U32 depth)
{
    dVAR;
    register regnode *scan;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTAIL;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    if (SIZE_ONLY)
	return;

    /* Find last node. */
    scan = p;
    for (;;) {
	regnode * const temp = regnext(scan);
        DEBUG_PARSE_r({
            SV * const mysv=sv_newmortal();
            DEBUG_PARSE_MSG((scan==p ? "tail" : ""));
            regprop(RExC_rx, mysv, scan);
            PerlIO_printf(Perl_debug_log, "~ %s (%d) %s %s\n",
                SvPV_nolen_const(mysv), REG_NODE_NUM(scan),
                    (temp == NULL ? "->" : ""),
                    (temp == NULL ? PL_reg_name[OP(val)] : "")
            );
        });
        if (temp == NULL)
            break;
        scan = temp;
    }

    if (reg_off_by_arg[OP(scan)]) {
        ARG_SET(scan, val - scan);
    }
    else {
        NEXT_OFF(scan) = val - scan;
    }
}

#ifdef DEBUGGING
/*
- regtail_study - set the next-pointer at the end of a node chain of p to val.
- Look for optimizable sequences at the same time.
- currently only looks for EXACT chains.

This is expermental code. The idea is to use this routine to perform 
in place optimizations on branches and groups as they are constructed,
with the long term intention of removing optimization from study_chunk so
that it is purely analytical.

Currently only used when in DEBUG mode. The macro REGTAIL_STUDY() is used
to control which is which.

*/
/* TODO: All four parms should be const */

STATIC U8
S_regtail_study(pTHX_ RExC_state_t *pRExC_state, regnode *p, const regnode *val,U32 depth)
{
    dVAR;
    register regnode *scan;
    U8 exact = PSEUDO;
#ifdef EXPERIMENTAL_INPLACESCAN
    I32 min = 0;
#endif
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTAIL_STUDY;


    if (SIZE_ONLY)
        return exact;

    /* Find last node. */

    scan = p;
    for (;;) {
        regnode * const temp = regnext(scan);
#ifdef EXPERIMENTAL_INPLACESCAN
        if (PL_regkind[OP(scan)] == EXACT)
            if (join_exact(pRExC_state,scan,&min,1,val,depth+1))
                return EXACT;
#endif
        if ( exact ) {
            switch (OP(scan)) {
                case EXACT:
                case EXACTF:
                case EXACTFL:
                        if( exact == PSEUDO )
                            exact= OP(scan);
                        else if ( exact != OP(scan) )
                            exact= 0;
                case NOTHING:
                    break;
                default:
                    exact= 0;
            }
        }
        DEBUG_PARSE_r({
            SV * const mysv=sv_newmortal();
            DEBUG_PARSE_MSG((scan==p ? "tsdy" : ""));
            regprop(RExC_rx, mysv, scan);
            PerlIO_printf(Perl_debug_log, "~ %s (%d) -> %s\n",
                SvPV_nolen_const(mysv),
                REG_NODE_NUM(scan),
                PL_reg_name[exact]);
        });
	if (temp == NULL)
	    break;
	scan = temp;
    }
    DEBUG_PARSE_r({
        SV * const mysv_val=sv_newmortal();
        DEBUG_PARSE_MSG("");
        regprop(RExC_rx, mysv_val, val);
        PerlIO_printf(Perl_debug_log, "~ attach to %s (%"IVdf") offset to %"IVdf"\n",
		      SvPV_nolen_const(mysv_val),
		      (IV)REG_NODE_NUM(val),
		      (IV)(val - scan)
        );
    });
    if (reg_off_by_arg[OP(scan)]) {
	ARG_SET(scan, val - scan);
    }
    else {
	NEXT_OFF(scan) = val - scan;
    }

    return exact;
}
#endif

/*
 - regcurly - a little FSA that accepts {\d+,?\d*}
 */
#ifndef PERL_IN_XSUB_RE
I32
Perl_regcurly(register const char *s)
{
    PERL_ARGS_ASSERT_REGCURLY;

    if (*s++ != '{')
	return FALSE;
    if (!isDIGIT(*s))
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (*s == ',')
	s++;
    while (isDIGIT(*s))
	s++;
    if (*s != '}')
	return FALSE;
    return TRUE;
}
#endif

/*
 - regdump - dump a regexp onto Perl_debug_log in vaguely comprehensible form
 */
#ifdef DEBUGGING
static void 
S_regdump_extflags(pTHX_ const char *lead, const U32 flags)
{
    int bit;
    int set=0;

    for (bit=0; bit<32; bit++) {
        if (flags & (1<<bit)) {
            if (!set++ && lead) 
                PerlIO_printf(Perl_debug_log, "%s",lead);
            PerlIO_printf(Perl_debug_log, "%s ",PL_reg_extflags_name[bit]);
        }	        
    }	   
    if (lead)  {
        if (set) 
            PerlIO_printf(Perl_debug_log, "\n");
        else 
            PerlIO_printf(Perl_debug_log, "%s[none-set]\n",lead);
    }            
}   
#endif

void
Perl_regdump(pTHX_ const regexp *r)
{
#ifdef DEBUGGING
    dVAR;
    SV * const sv = sv_newmortal();
    SV *dsv= sv_newmortal();
    RXi_GET_DECL(r,ri);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGDUMP;

    (void)dumpuntil(r, ri->program, ri->program + 1, NULL, NULL, sv, 0, 0);

    /* Header fields of interest. */
    if (r->anchored_substr) {
	RE_PV_QUOTED_DECL(s, 0, dsv, SvPVX_const(r->anchored_substr), 
	    RE_SV_DUMPLEN(r->anchored_substr), 30);
	PerlIO_printf(Perl_debug_log,
		      "anchored %s%s at %"IVdf" ",
		      s, RE_SV_TAIL(r->anchored_substr),
		      (IV)r->anchored_offset);
    } else if (r->anchored_utf8) {
	RE_PV_QUOTED_DECL(s, 1, dsv, SvPVX_const(r->anchored_utf8), 
	    RE_SV_DUMPLEN(r->anchored_utf8), 30);
	PerlIO_printf(Perl_debug_log,
		      "anchored utf8 %s%s at %"IVdf" ",
		      s, RE_SV_TAIL(r->anchored_utf8),
		      (IV)r->anchored_offset);
    }		      
    if (r->float_substr) {
	RE_PV_QUOTED_DECL(s, 0, dsv, SvPVX_const(r->float_substr), 
	    RE_SV_DUMPLEN(r->float_substr), 30);
	PerlIO_printf(Perl_debug_log,
		      "floating %s%s at %"IVdf"..%"UVuf" ",
		      s, RE_SV_TAIL(r->float_substr),
		      (IV)r->float_min_offset, (UV)r->float_max_offset);
    } else if (r->float_utf8) {
	RE_PV_QUOTED_DECL(s, 1, dsv, SvPVX_const(r->float_utf8), 
	    RE_SV_DUMPLEN(r->float_utf8), 30);
	PerlIO_printf(Perl_debug_log,
		      "floating utf8 %s%s at %"IVdf"..%"UVuf" ",
		      s, RE_SV_TAIL(r->float_utf8),
		      (IV)r->float_min_offset, (UV)r->float_max_offset);
    }
    if (r->check_substr || r->check_utf8)
	PerlIO_printf(Perl_debug_log,
		      (const char *)
		      (r->check_substr == r->float_substr
		       && r->check_utf8 == r->float_utf8
		       ? "(checking floating" : "(checking anchored"));
    if (r->extflags & RXf_NOSCAN)
	PerlIO_printf(Perl_debug_log, " noscan");
    if (r->extflags & RXf_CHECK_ALL)
	PerlIO_printf(Perl_debug_log, " isall");
    if (r->check_substr || r->check_utf8)
	PerlIO_printf(Perl_debug_log, ") ");

    if (ri->regstclass) {
	regprop(r, sv, ri->regstclass);
	PerlIO_printf(Perl_debug_log, "stclass %s ", SvPVX_const(sv));
    }
    if (r->extflags & RXf_ANCH) {
	PerlIO_printf(Perl_debug_log, "anchored");
	if (r->extflags & RXf_ANCH_BOL)
	    PerlIO_printf(Perl_debug_log, "(BOL)");
	if (r->extflags & RXf_ANCH_MBOL)
	    PerlIO_printf(Perl_debug_log, "(MBOL)");
	if (r->extflags & RXf_ANCH_SBOL)
	    PerlIO_printf(Perl_debug_log, "(SBOL)");
	if (r->extflags & RXf_ANCH_GPOS)
	    PerlIO_printf(Perl_debug_log, "(GPOS)");
	PerlIO_putc(Perl_debug_log, ' ');
    }
    if (r->extflags & RXf_GPOS_SEEN)
	PerlIO_printf(Perl_debug_log, "GPOS:%"UVuf" ", (UV)r->gofs);
    if (r->intflags & PREGf_SKIP)
	PerlIO_printf(Perl_debug_log, "plus ");
    if (r->intflags & PREGf_IMPLICIT)
	PerlIO_printf(Perl_debug_log, "implicit ");
    PerlIO_printf(Perl_debug_log, "minlen %"IVdf" ", (IV)r->minlen);
    if (r->extflags & RXf_EVAL_SEEN)
	PerlIO_printf(Perl_debug_log, "with eval ");
    PerlIO_printf(Perl_debug_log, "\n");
    DEBUG_FLAGS_r(regdump_extflags("r->extflags: ",r->extflags));            
#else
    PERL_ARGS_ASSERT_REGDUMP;
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(r);
#endif	/* DEBUGGING */
}

/*
- regprop - printable representation of opcode
*/
#define EMIT_ANYOF_TEST_SEPARATOR(do_sep,sv,flags) \
STMT_START { \
        if (do_sep) {                           \
            Perl_sv_catpvf(aTHX_ sv,"%s][%s",PL_colors[1],PL_colors[0]); \
            if (flags & ANYOF_INVERT)           \
                /*make sure the invert info is in each */ \
                sv_catpvs(sv, "^");             \
            do_sep = 0;                         \
        }                                       \
} STMT_END

void
Perl_regprop(pTHX_ const regexp *prog, SV *sv, const regnode *o)
{
#ifdef DEBUGGING
    dVAR;
    register int k;
    RXi_GET_DECL(prog,progi);
    GET_RE_DEBUG_FLAGS_DECL;
    
    PERL_ARGS_ASSERT_REGPROP;

    sv_setpvs(sv, "");

    if (OP(o) > REGNODE_MAX)		/* regnode.type is unsigned */
	/* It would be nice to FAIL() here, but this may be called from
	   regexec.c, and it would be hard to supply pRExC_state. */
	Perl_croak(aTHX_ "Corrupted regexp opcode %d > %d", (int)OP(o), (int)REGNODE_MAX);
    sv_catpv(sv, PL_reg_name[OP(o)]); /* Take off const! */

    k = PL_regkind[OP(o)];

    if (k == EXACT) {
	sv_catpvs(sv, " ");
	/* Using is_utf8_string() (via PERL_PV_UNI_DETECT) 
	 * is a crude hack but it may be the best for now since 
	 * we have no flag "this EXACTish node was UTF-8" 
	 * --jhi */
	pv_pretty(sv, STRING(o), STR_LEN(o), 60, PL_colors[0], PL_colors[1],
		  PERL_PV_ESCAPE_UNI_DETECT |
		  PERL_PV_PRETTY_ELLIPSES   |
		  PERL_PV_PRETTY_LTGT       |
		  PERL_PV_PRETTY_NOCLEAR
		  );
    } else if (k == TRIE) {
	/* print the details of the trie in dumpuntil instead, as
	 * progi->data isn't available here */
        const char op = OP(o);
        const U32 n = ARG(o);
        const reg_ac_data * const ac = IS_TRIE_AC(op) ?
               (reg_ac_data *)progi->data->data[n] :
               NULL;
        const reg_trie_data * const trie
	    = (reg_trie_data*)progi->data->data[!IS_TRIE_AC(op) ? n : ac->trie];
        
        Perl_sv_catpvf(aTHX_ sv, "-%s",PL_reg_name[o->flags]);
        DEBUG_TRIE_COMPILE_r(
            Perl_sv_catpvf(aTHX_ sv,
                "<S:%"UVuf"/%"IVdf" W:%"UVuf" L:%"UVuf"/%"UVuf" C:%"UVuf"/%"UVuf">",
                (UV)trie->startstate,
                (IV)trie->statecount-1, /* -1 because of the unused 0 element */
                (UV)trie->wordcount,
                (UV)trie->minlen,
                (UV)trie->maxlen,
                (UV)TRIE_CHARCOUNT(trie),
                (UV)trie->uniquecharcount
            )
        );
        if ( IS_ANYOF_TRIE(op) || trie->bitmap ) {
            int i;
            int rangestart = -1;
            U8* bitmap = IS_ANYOF_TRIE(op) ? (U8*)ANYOF_BITMAP(o) : (U8*)TRIE_BITMAP(trie);
            sv_catpvs(sv, "[");
            for (i = 0; i <= 256; i++) {
                if (i < 256 && BITMAP_TEST(bitmap,i)) {
                    if (rangestart == -1)
                        rangestart = i;
                } else if (rangestart != -1) {
                    if (i <= rangestart + 3)
                        for (; rangestart < i; rangestart++)
                            put_byte(sv, rangestart);
                    else {
                        put_byte(sv, rangestart);
                        sv_catpvs(sv, "-");
                        put_byte(sv, i - 1);
                    }
                    rangestart = -1;
                }
            }
            sv_catpvs(sv, "]");
        } 
	 
    } else if (k == CURLY) {
	if (OP(o) == CURLYM || OP(o) == CURLYN || OP(o) == CURLYX)
	    Perl_sv_catpvf(aTHX_ sv, "[%d]", o->flags); /* Parenth number */
	Perl_sv_catpvf(aTHX_ sv, " {%d,%d}", ARG1(o), ARG2(o));
    }
    else if (k == WHILEM && o->flags)			/* Ordinal/of */
	Perl_sv_catpvf(aTHX_ sv, "[%d/%d]", o->flags & 0xf, o->flags>>4);
    else if (k == REF || k == OPEN || k == CLOSE || k == GROUPP || OP(o)==ACCEPT) {
	Perl_sv_catpvf(aTHX_ sv, "%d", (int)ARG(o));	/* Parenth number */
	if ( RXp_PAREN_NAMES(prog) ) {
            if ( k != REF || OP(o) < NREF) {	    
	        AV *list= MUTABLE_AV(progi->data->data[progi->name_list_idx]);
	        SV **name= av_fetch(list, ARG(o), 0 );
	        if (name)
	            Perl_sv_catpvf(aTHX_ sv, " '%"SVf"'", SVfARG(*name));
            }	    
            else {
                AV *list= MUTABLE_AV(progi->data->data[ progi->name_list_idx ]);
                SV *sv_dat= MUTABLE_SV(progi->data->data[ ARG( o ) ]);
                I32 *nums=(I32*)SvPVX(sv_dat);
                SV **name= av_fetch(list, nums[0], 0 );
                I32 n;
                if (name) {
                    for ( n=0; n<SvIVX(sv_dat); n++ ) {
                        Perl_sv_catpvf(aTHX_ sv, "%s%"IVdf,
			   	    (n ? "," : ""), (IV)nums[n]);
                    }
                    Perl_sv_catpvf(aTHX_ sv, " '%"SVf"'", SVfARG(*name));
                }
            }
        }            
    } else if (k == GOSUB) 
	Perl_sv_catpvf(aTHX_ sv, "%d[%+d]", (int)ARG(o),(int)ARG2L(o));	/* Paren and offset */
    else if (k == VERB) {
        if (!o->flags) 
            Perl_sv_catpvf(aTHX_ sv, ":%"SVf, 
			   SVfARG((MUTABLE_SV(progi->data->data[ ARG( o ) ]))));
    } else if (k == LOGICAL)
	Perl_sv_catpvf(aTHX_ sv, "[%d]", o->flags);	/* 2: embedded, otherwise 1 */
    else if (k == FOLDCHAR)
	Perl_sv_catpvf(aTHX_ sv, "[0x%"UVXf"]", PTR2UV(ARG(o)) );
    else if (k == ANYOF) {
	int i, rangestart = -1;
	const U8 flags = ANYOF_FLAGS(o);
	int do_sep = 0;

	/* Should be synchronized with * ANYOF_ #xdefines in regcomp.h */
	static const char * const anyofs[] = {
	    "\\w",
	    "\\W",
	    "\\s",
	    "\\S",
	    "\\d",
	    "\\D",
	    "[:alnum:]",
	    "[:^alnum:]",
	    "[:alpha:]",
	    "[:^alpha:]",
	    "[:ascii:]",
	    "[:^ascii:]",
	    "[:cntrl:]",
	    "[:^cntrl:]",
	    "[:graph:]",
	    "[:^graph:]",
	    "[:lower:]",
	    "[:^lower:]",
	    "[:print:]",
	    "[:^print:]",
	    "[:punct:]",
	    "[:^punct:]",
	    "[:upper:]",
	    "[:^upper:]",
	    "[:xdigit:]",
	    "[:^xdigit:]",
	    "[:space:]",
	    "[:^space:]",
	    "[:blank:]",
	    "[:^blank:]"
	};

	if (flags & ANYOF_LOCALE)
	    sv_catpvs(sv, "{loc}");
	if (flags & ANYOF_FOLD)
	    sv_catpvs(sv, "{i}");
	Perl_sv_catpvf(aTHX_ sv, "[%s", PL_colors[0]);
	if (flags & ANYOF_INVERT)
	    sv_catpvs(sv, "^");
	
	/* output what the standard cp 0-255 bitmap matches */
	for (i = 0; i <= 256; i++) {
	    if (i < 256 && ANYOF_BITMAP_TEST(o,i)) {
		if (rangestart == -1)
		    rangestart = i;
	    } else if (rangestart != -1) {
		if (i <= rangestart + 3)
		    for (; rangestart < i; rangestart++)
			put_byte(sv, rangestart);
		else {
		    put_byte(sv, rangestart);
		    sv_catpvs(sv, "-");
		    put_byte(sv, i - 1);
		}
		do_sep = 1;
		rangestart = -1;
	    }
	}
        
        EMIT_ANYOF_TEST_SEPARATOR(do_sep,sv,flags);
        /* output any special charclass tests (used mostly under use locale) */
	if (o->flags & ANYOF_CLASS)
	    for (i = 0; i < (int)(sizeof(anyofs)/sizeof(char*)); i++)
		if (ANYOF_CLASS_TEST(o,i)) {
		    sv_catpv(sv, anyofs[i]);
		    do_sep = 1;
		}
        
        EMIT_ANYOF_TEST_SEPARATOR(do_sep,sv,flags);
        
        /* output information about the unicode matching */
	if (flags & ANYOF_UNICODE)
	    sv_catpvs(sv, "{unicode}");
	else if (flags & ANYOF_UNICODE_ALL)
	    sv_catpvs(sv, "{unicode_all}");

	{
	    SV *lv;
	    SV * const sw = regclass_swash(prog, o, FALSE, &lv, 0);
	
	    if (lv) {
		if (sw) {
		    U8 s[UTF8_MAXBYTES_CASE+1];

		    for (i = 0; i <= 256; i++) { /* just the first 256 */
			uvchr_to_utf8(s, i);
			
			if (i < 256 && swash_fetch(sw, s, TRUE)) {
			    if (rangestart == -1)
				rangestart = i;
			} else if (rangestart != -1) {
			    if (i <= rangestart + 3)
				for (; rangestart < i; rangestart++) {
				    const U8 * const e = uvchr_to_utf8(s,rangestart);
				    U8 *p;
				    for(p = s; p < e; p++)
					put_byte(sv, *p);
				}
			    else {
				const U8 *e = uvchr_to_utf8(s,rangestart);
				U8 *p;
				for (p = s; p < e; p++)
				    put_byte(sv, *p);
				sv_catpvs(sv, "-");
				e = uvchr_to_utf8(s, i-1);
				for (p = s; p < e; p++)
				    put_byte(sv, *p);
				}
				rangestart = -1;
			    }
			}
			
		    sv_catpvs(sv, "..."); /* et cetera */
		}

		{
		    char *s = savesvpv(lv);
		    char * const origs = s;
		
		    while (*s && *s != '\n')
			s++;
		
		    if (*s == '\n') {
			const char * const t = ++s;
			
			while (*s) {
			    if (*s == '\n')
				*s = ' ';
			    s++;
			}
			if (s[-1] == ' ')
			    s[-1] = 0;
			
			sv_catpv(sv, t);
		    }
		
		    Safefree(origs);
		}
	    }
	}

	Perl_sv_catpvf(aTHX_ sv, "%s]", PL_colors[1]);
    }
    else if (k == BRANCHJ && (OP(o) == UNLESSM || OP(o) == IFMATCH))
	Perl_sv_catpvf(aTHX_ sv, "[%d]", -(o->flags));
#else
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
    PERL_UNUSED_ARG(o);
    PERL_UNUSED_ARG(prog);
#endif	/* DEBUGGING */
}

SV *
Perl_re_intuit_string(pTHX_ REGEXP * const r)
{				/* Assume that RE_INTUIT is set */
    dVAR;
    struct regexp *const prog = (struct regexp *)SvANY(r);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_INTUIT_STRING;
    PERL_UNUSED_CONTEXT;

    DEBUG_COMPILE_r(
	{
	    const char * const s = SvPV_nolen_const(prog->check_substr
		      ? prog->check_substr : prog->check_utf8);

	    if (!PL_colorset) reginitcolors();
	    PerlIO_printf(Perl_debug_log,
		      "%sUsing REx %ssubstr:%s \"%s%.60s%s%s\"\n",
		      PL_colors[4],
		      prog->check_substr ? "" : "utf8 ",
		      PL_colors[5],PL_colors[0],
		      s,
		      PL_colors[1],
		      (strlen(s) > 60 ? "..." : ""));
	} );

    return prog->check_substr ? prog->check_substr : prog->check_utf8;
}

/* 
   pregfree() 
   
   handles refcounting and freeing the perl core regexp structure. When 
   it is necessary to actually free the structure the first thing it 
   does is call the 'free' method of the regexp_engine associated to to 
   the regexp, allowing the handling of the void *pprivate; member 
   first. (This routine is not overridable by extensions, which is why 
   the extensions free is called first.)
   
   See regdupe and regdupe_internal if you change anything here. 
*/
#ifndef PERL_IN_XSUB_RE
void
Perl_pregfree(pTHX_ REGEXP *r)
{
    SvREFCNT_dec(r);
}

void
Perl_pregfree2(pTHX_ REGEXP *rx)
{
    dVAR;
    struct regexp *const r = (struct regexp *)SvANY(rx);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_PREGFREE2;

    if (r->mother_re) {
        ReREFCNT_dec(r->mother_re);
    } else {
        CALLREGFREE_PVT(rx); /* free the private data */
        SvREFCNT_dec(RXp_PAREN_NAMES(r));
    }        
    if (r->substrs) {
        SvREFCNT_dec(r->anchored_substr);
        SvREFCNT_dec(r->anchored_utf8);
        SvREFCNT_dec(r->float_substr);
        SvREFCNT_dec(r->float_utf8);
	Safefree(r->substrs);
    }
    RX_MATCH_COPY_FREE(rx);
#ifdef PERL_OLD_COPY_ON_WRITE
    SvREFCNT_dec(r->saved_copy);
#endif
    Safefree(r->offs);
}

/*  reg_temp_copy()
    
    This is a hacky workaround to the structural issue of match results
    being stored in the regexp structure which is in turn stored in
    PL_curpm/PL_reg_curpm. The problem is that due to qr// the pattern
    could be PL_curpm in multiple contexts, and could require multiple
    result sets being associated with the pattern simultaneously, such
    as when doing a recursive match with (??{$qr})
    
    The solution is to make a lightweight copy of the regexp structure 
    when a qr// is returned from the code executed by (??{$qr}) this
    lightweight copy doesnt actually own any of its data except for
    the starp/end and the actual regexp structure itself. 
    
*/    
    
    
REGEXP *
Perl_reg_temp_copy (pTHX_ REGEXP *ret_x, REGEXP *rx)
{
    struct regexp *ret;
    struct regexp *const r = (struct regexp *)SvANY(rx);
    register const I32 npar = r->nparens+1;

    PERL_ARGS_ASSERT_REG_TEMP_COPY;

    if (!ret_x)
	ret_x = (REGEXP*) newSV_type(SVt_REGEXP);
    ret = (struct regexp *)SvANY(ret_x);
    
    (void)ReREFCNT_inc(rx);
    /* We can take advantage of the existing "copied buffer" mechanism in SVs
       by pointing directly at the buffer, but flagging that the allocated
       space in the copy is zero. As we've just done a struct copy, it's now
       a case of zero-ing that, rather than copying the current length.  */
    SvPV_set(ret_x, RX_WRAPPED(rx));
    SvFLAGS(ret_x) |= SvFLAGS(rx) & (SVf_POK|SVp_POK|SVf_UTF8);
    memcpy(&(ret->xpv_cur), &(r->xpv_cur),
	   sizeof(regexp) - STRUCT_OFFSET(regexp, xpv_cur));
    SvLEN_set(ret_x, 0);
    SvSTASH_set(ret_x, NULL);
    SvMAGIC_set(ret_x, NULL);
    Newx(ret->offs, npar, regexp_paren_pair);
    Copy(r->offs, ret->offs, npar, regexp_paren_pair);
    if (r->substrs) {
        Newx(ret->substrs, 1, struct reg_substr_data);
	StructCopy(r->substrs, ret->substrs, struct reg_substr_data);

	SvREFCNT_inc_void(ret->anchored_substr);
	SvREFCNT_inc_void(ret->anchored_utf8);
	SvREFCNT_inc_void(ret->float_substr);
	SvREFCNT_inc_void(ret->float_utf8);

	/* check_substr and check_utf8, if non-NULL, point to either their
	   anchored or float namesakes, and don't hold a second reference.  */
    }
    RX_MATCH_COPIED_off(ret_x);
#ifdef PERL_OLD_COPY_ON_WRITE
    ret->saved_copy = NULL;
#endif
    ret->mother_re = rx;
    
    return ret_x;
}
#endif

/* regfree_internal() 

   Free the private data in a regexp. This is overloadable by 
   extensions. Perl takes care of the regexp structure in pregfree(), 
   this covers the *pprivate pointer which technically perldoesnt 
   know about, however of course we have to handle the 
   regexp_internal structure when no extension is in use. 
   
   Note this is called before freeing anything in the regexp 
   structure. 
 */
 
void
Perl_regfree_internal(pTHX_ REGEXP * const rx)
{
    dVAR;
    struct regexp *const r = (struct regexp *)SvANY(rx);
    RXi_GET_DECL(r,ri);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGFREE_INTERNAL;

    DEBUG_COMPILE_r({
	if (!PL_colorset)
	    reginitcolors();
	{
	    SV *dsv= sv_newmortal();
            RE_PV_QUOTED_DECL(s, RX_UTF8(rx),
                dsv, RX_PRECOMP(rx), RX_PRELEN(rx), 60);
            PerlIO_printf(Perl_debug_log,"%sFreeing REx:%s %s\n", 
                PL_colors[4],PL_colors[5],s);
        }
    });
#ifdef RE_TRACK_PATTERN_OFFSETS
    if (ri->u.offsets)
        Safefree(ri->u.offsets);             /* 20010421 MJD */
#endif
    if (ri->data) {
	int n = ri->data->count;
	PAD* new_comppad = NULL;
	PAD* old_comppad;
	PADOFFSET refcnt;

	while (--n >= 0) {
          /* If you add a ->what type here, update the comment in regcomp.h */
	    switch (ri->data->what[n]) {
	    case 's':
	    case 'S':
	    case 'u':
		SvREFCNT_dec(MUTABLE_SV(ri->data->data[n]));
		break;
	    case 'f':
		Safefree(ri->data->data[n]);
		break;
	    case 'p':
		new_comppad = MUTABLE_AV(ri->data->data[n]);
		break;
	    case 'o':
		if (new_comppad == NULL)
		    Perl_croak(aTHX_ "panic: pregfree comppad");
		PAD_SAVE_LOCAL(old_comppad,
		    /* Watch out for global destruction's random ordering. */
		    (SvTYPE(new_comppad) == SVt_PVAV) ? new_comppad : NULL
		);
		OP_REFCNT_LOCK;
		refcnt = OpREFCNT_dec((OP_4tree*)ri->data->data[n]);
		OP_REFCNT_UNLOCK;
		if (!refcnt)
                    op_free((OP_4tree*)ri->data->data[n]);

		PAD_RESTORE_LOCAL(old_comppad);
		SvREFCNT_dec(MUTABLE_SV(new_comppad));
		new_comppad = NULL;
		break;
	    case 'n':
	        break;
            case 'T':	        
                { /* Aho Corasick add-on structure for a trie node.
                     Used in stclass optimization only */
                    U32 refcount;
                    reg_ac_data *aho=(reg_ac_data*)ri->data->data[n];
                    OP_REFCNT_LOCK;
                    refcount = --aho->refcount;
                    OP_REFCNT_UNLOCK;
                    if ( !refcount ) {
                        PerlMemShared_free(aho->states);
                        PerlMemShared_free(aho->fail);
			 /* do this last!!!! */
                        PerlMemShared_free(ri->data->data[n]);
                        PerlMemShared_free(ri->regstclass);
                    }
                }
                break;
	    case 't':
	        {
	            /* trie structure. */
	            U32 refcount;
	            reg_trie_data *trie=(reg_trie_data*)ri->data->data[n];
                    OP_REFCNT_LOCK;
                    refcount = --trie->refcount;
                    OP_REFCNT_UNLOCK;
                    if ( !refcount ) {
                        PerlMemShared_free(trie->charmap);
                        PerlMemShared_free(trie->states);
                        PerlMemShared_free(trie->trans);
                        if (trie->bitmap)
                            PerlMemShared_free(trie->bitmap);
                        if (trie->wordlen)
                            PerlMemShared_free(trie->wordlen);
                        if (trie->jump)
                            PerlMemShared_free(trie->jump);
                        if (trie->nextword)
                            PerlMemShared_free(trie->nextword);
                        /* do this last!!!! */
                        PerlMemShared_free(ri->data->data[n]);
		    }
		}
		break;
	    default:
		Perl_croak(aTHX_ "panic: regfree data code '%c'", ri->data->what[n]);
	    }
	}
	Safefree(ri->data->what);
	Safefree(ri->data);
    }

    Safefree(ri);
}

#define sv_dup_inc(s,t)	SvREFCNT_inc(sv_dup(s,t))
#define av_dup_inc(s,t)	MUTABLE_AV(SvREFCNT_inc(sv_dup((const SV *)s,t)))
#define hv_dup_inc(s,t)	MUTABLE_HV(SvREFCNT_inc(sv_dup((const SV *)s,t)))
#define SAVEPVN(p,n)	((p) ? savepvn(p,n) : NULL)

/* 
   re_dup - duplicate a regexp. 
   
   This routine is expected to clone a given regexp structure. It is only
   compiled under USE_ITHREADS.

   After all of the core data stored in struct regexp is duplicated
   the regexp_engine.dupe method is used to copy any private data
   stored in the *pprivate pointer. This allows extensions to handle
   any duplication it needs to do.

   See pregfree() and regfree_internal() if you change anything here. 
*/
#if defined(USE_ITHREADS)
#ifndef PERL_IN_XSUB_RE
void
Perl_re_dup_guts(pTHX_ const REGEXP *sstr, REGEXP *dstr, CLONE_PARAMS *param)
{
    dVAR;
    I32 npar;
    const struct regexp *r = (const struct regexp *)SvANY(sstr);
    struct regexp *ret = (struct regexp *)SvANY(dstr);
    
    PERL_ARGS_ASSERT_RE_DUP_GUTS;

    npar = r->nparens+1;
    Newx(ret->offs, npar, regexp_paren_pair);
    Copy(r->offs, ret->offs, npar, regexp_paren_pair);
    if(ret->swap) {
        /* no need to copy these */
        Newx(ret->swap, npar, regexp_paren_pair);
    }

    if (ret->substrs) {
	/* Do it this way to avoid reading from *r after the StructCopy().
	   That way, if any of the sv_dup_inc()s dislodge *r from the L1
	   cache, it doesn't matter.  */
	const bool anchored = r->check_substr
	    ? r->check_substr == r->anchored_substr
	    : r->check_utf8 == r->anchored_utf8;
        Newx(ret->substrs, 1, struct reg_substr_data);
	StructCopy(r->substrs, ret->substrs, struct reg_substr_data);

	ret->anchored_substr = sv_dup_inc(ret->anchored_substr, param);
	ret->anchored_utf8 = sv_dup_inc(ret->anchored_utf8, param);
	ret->float_substr = sv_dup_inc(ret->float_substr, param);
	ret->float_utf8 = sv_dup_inc(ret->float_utf8, param);

	/* check_substr and check_utf8, if non-NULL, point to either their
	   anchored or float namesakes, and don't hold a second reference.  */

	if (ret->check_substr) {
	    if (anchored) {
		assert(r->check_utf8 == r->anchored_utf8);
		ret->check_substr = ret->anchored_substr;
		ret->check_utf8 = ret->anchored_utf8;
	    } else {
		assert(r->check_substr == r->float_substr);
		assert(r->check_utf8 == r->float_utf8);
		ret->check_substr = ret->float_substr;
		ret->check_utf8 = ret->float_utf8;
	    }
	} else if (ret->check_utf8) {
	    if (anchored) {
		ret->check_utf8 = ret->anchored_utf8;
	    } else {
		ret->check_utf8 = ret->float_utf8;
	    }
	}
    }

    RXp_PAREN_NAMES(ret) = hv_dup_inc(RXp_PAREN_NAMES(ret), param);

    if (ret->pprivate)
	RXi_SET(ret,CALLREGDUPE_PVT(dstr,param));

    if (RX_MATCH_COPIED(dstr))
	ret->subbeg  = SAVEPVN(ret->subbeg, ret->sublen);
    else
	ret->subbeg = NULL;
#ifdef PERL_OLD_COPY_ON_WRITE
    ret->saved_copy = NULL;
#endif

    if (ret->mother_re) {
	if (SvPVX_const(dstr) == SvPVX_const(ret->mother_re)) {
	    /* Our storage points directly to our mother regexp, but that's
	       1: a buffer in a different thread
	       2: something we no longer hold a reference on
	       so we need to copy it locally.  */
	    /* Note we need to sue SvCUR() on our mother_re, because it, in
	       turn, may well be pointing to its own mother_re.  */
	    SvPV_set(dstr, SAVEPVN(SvPVX_const(ret->mother_re),
				   SvCUR(ret->mother_re)+1));
	    SvLEN_set(dstr, SvCUR(ret->mother_re)+1);
	}
	ret->mother_re      = NULL;
    }
    ret->gofs = 0;
}
#endif /* PERL_IN_XSUB_RE */

/*
   regdupe_internal()
   
   This is the internal complement to regdupe() which is used to copy
   the structure pointed to by the *pprivate pointer in the regexp.
   This is the core version of the extension overridable cloning hook.
   The regexp structure being duplicated will be copied by perl prior
   to this and will be provided as the regexp *r argument, however 
   with the /old/ structures pprivate pointer value. Thus this routine
   may override any copying normally done by perl.
   
   It returns a pointer to the new regexp_internal structure.
*/

void *
Perl_regdupe_internal(pTHX_ REGEXP * const rx, CLONE_PARAMS *param)
{
    dVAR;
    struct regexp *const r = (struct regexp *)SvANY(rx);
    regexp_internal *reti;
    int len, npar;
    RXi_GET_DECL(r,ri);

    PERL_ARGS_ASSERT_REGDUPE_INTERNAL;
    
    npar = r->nparens+1;
    len = ProgLen(ri);
    
    Newxc(reti, sizeof(regexp_internal) + len*sizeof(regnode), char, regexp_internal);
    Copy(ri->program, reti->program, len+1, regnode);
    

    reti->regstclass = NULL;

    if (ri->data) {
	struct reg_data *d;
        const int count = ri->data->count;
	int i;

	Newxc(d, sizeof(struct reg_data) + count*sizeof(void *),
		char, struct reg_data);
	Newx(d->what, count, U8);

	d->count = count;
	for (i = 0; i < count; i++) {
	    d->what[i] = ri->data->what[i];
	    switch (d->what[i]) {
	        /* legal options are one of: sSfpontTu
	           see also regcomp.h and pregfree() */
	    case 's':
	    case 'S':
	    case 'p': /* actually an AV, but the dup function is identical.  */
	    case 'u': /* actually an HV, but the dup function is identical.  */
		d->data[i] = sv_dup_inc((const SV *)ri->data->data[i], param);
		break;
	    case 'f':
		/* This is cheating. */
		Newx(d->data[i], 1, struct regnode_charclass_class);
		StructCopy(ri->data->data[i], d->data[i],
			    struct regnode_charclass_class);
		reti->regstclass = (regnode*)d->data[i];
		break;
	    case 'o':
		/* Compiled op trees are readonly and in shared memory,
		   and can thus be shared without duplication. */
		OP_REFCNT_LOCK;
		d->data[i] = (void*)OpREFCNT_inc((OP*)ri->data->data[i]);
		OP_REFCNT_UNLOCK;
		break;
	    case 'T':
		/* Trie stclasses are readonly and can thus be shared
		 * without duplication. We free the stclass in pregfree
		 * when the corresponding reg_ac_data struct is freed.
		 */
		reti->regstclass= ri->regstclass;
		/* Fall through */
	    case 't':
		OP_REFCNT_LOCK;
		((reg_trie_data*)ri->data->data[i])->refcount++;
		OP_REFCNT_UNLOCK;
		/* Fall through */
	    case 'n':
		d->data[i] = ri->data->data[i];
		break;
            default:
		Perl_croak(aTHX_ "panic: re_dup unknown data code '%c'", ri->data->what[i]);
	    }
	}

	reti->data = d;
    }
    else
	reti->data = NULL;

    reti->name_list_idx = ri->name_list_idx;

#ifdef RE_TRACK_PATTERN_OFFSETS
    if (ri->u.offsets) {
        Newx(reti->u.offsets, 2*len+1, U32);
        Copy(ri->u.offsets, reti->u.offsets, 2*len+1, U32);
    }
#else
    SetProgLen(reti,len);
#endif

    return (void*)reti;
}

#endif    /* USE_ITHREADS */

#ifndef PERL_IN_XSUB_RE

/*
 - regnext - dig the "next" pointer out of a node
 */
regnode *
Perl_regnext(pTHX_ register regnode *p)
{
    dVAR;
    register I32 offset;

    if (!p)
	return(NULL);

    offset = (reg_off_by_arg[OP(p)] ? ARG(p) : NEXT_OFF(p));
    if (offset == 0)
	return(NULL);

    return(p+offset);
}
#endif

STATIC void	
S_re_croak2(pTHX_ const char* pat1,const char* pat2,...)
{
    va_list args;
    STRLEN l1 = strlen(pat1);
    STRLEN l2 = strlen(pat2);
    char buf[512];
    SV *msv;
    const char *message;

    PERL_ARGS_ASSERT_RE_CROAK2;

    if (l1 > 510)
	l1 = 510;
    if (l1 + l2 > 510)
	l2 = 510 - l1;
    Copy(pat1, buf, l1 , char);
    Copy(pat2, buf + l1, l2 , char);
    buf[l1 + l2] = '\n';
    buf[l1 + l2 + 1] = '\0';
#ifdef I_STDARG
    /* ANSI variant takes additional second argument */
    va_start(args, pat2);
#else
    va_start(args);
#endif
    msv = vmess(buf, &args);
    va_end(args);
    message = SvPV_const(msv,l1);
    if (l1 > 512)
	l1 = 512;
    Copy(message, buf, l1 , char);
    buf[l1-1] = '\0';			/* Overwrite \n */
    Perl_croak(aTHX_ "%s", buf);
}

/* XXX Here's a total kludge.  But we need to re-enter for swash routines. */

#ifndef PERL_IN_XSUB_RE
void
Perl_save_re_context(pTHX)
{
    dVAR;

    struct re_save_state *state;

    SAVEVPTR(PL_curcop);
    SSGROW(SAVESTACK_ALLOC_FOR_RE_SAVE_STATE + 1);

    state = (struct re_save_state *)(PL_savestack + PL_savestack_ix);
    PL_savestack_ix += SAVESTACK_ALLOC_FOR_RE_SAVE_STATE;
    SSPUSHINT(SAVEt_RE_STATE);

    Copy(&PL_reg_state, state, 1, struct re_save_state);

    PL_reg_start_tmp = 0;
    PL_reg_start_tmpl = 0;
    PL_reg_oldsaved = NULL;
    PL_reg_oldsavedlen = 0;
    PL_reg_maxiter = 0;
    PL_reg_leftiter = 0;
    PL_reg_poscache = NULL;
    PL_reg_poscache_size = 0;
#ifdef PERL_OLD_COPY_ON_WRITE
    PL_nrs = NULL;
#endif

    /* Save $1..$n (#18107: UTF-8 s/(\w+)/uc($1)/e); AMS 20021106. */
    if (PL_curpm) {
	const REGEXP * const rx = PM_GETRE(PL_curpm);
	if (rx) {
	    U32 i;
	    for (i = 1; i <= RX_NPARENS(rx); i++) {
		char digits[TYPE_CHARS(long)];
		const STRLEN len = my_snprintf(digits, sizeof(digits), "%lu", (long)i);
		GV *const *const gvp
		    = (GV**)hv_fetch(PL_defstash, digits, len, 0);

		if (gvp) {
		    GV * const gv = *gvp;
		    if (SvTYPE(gv) == SVt_PVGV && GvSV(gv))
			save_scalar(gv);
		}
	    }
	}
    }
}
#endif

static void
clear_re(pTHX_ void *r)
{
    dVAR;
    ReREFCNT_dec((REGEXP *)r);
}

#ifdef DEBUGGING

STATIC void
S_put_byte(pTHX_ SV *sv, int c)
{
    PERL_ARGS_ASSERT_PUT_BYTE;

    /* Our definition of isPRINT() ignores locales, so only bytes that are
       not part of UTF-8 are considered printable. I assume that the same
       holds for UTF-EBCDIC.
       Also, code point 255 is not printable in either (it's E0 in EBCDIC,
       which Wikipedia says:

       EO, or Eight Ones, is an 8-bit EBCDIC character code represented as all
       ones (binary 1111 1111, hexadecimal FF). It is similar, but not
       identical, to the ASCII delete (DEL) or rubout control character.
       ) So the old condition can be simplified to !isPRINT(c)  */
    if (!isPRINT(c))
	Perl_sv_catpvf(aTHX_ sv, "\\%o", c);
    else {
	const char string = c;
	if (c == '-' || c == ']' || c == '\\' || c == '^')
	    sv_catpvs(sv, "\\");
	sv_catpvn(sv, &string, 1);
    }
}


#define CLEAR_OPTSTART \
    if (optstart) STMT_START { \
	    DEBUG_OPTIMISE_r(PerlIO_printf(Perl_debug_log, " (%"IVdf" nodes)\n", (IV)(node - optstart))); \
	    optstart=NULL; \
    } STMT_END

#define DUMPUNTIL(b,e) CLEAR_OPTSTART; node=dumpuntil(r,start,(b),(e),last,sv,indent+1,depth+1);

STATIC const regnode *
S_dumpuntil(pTHX_ const regexp *r, const regnode *start, const regnode *node,
	    const regnode *last, const regnode *plast, 
	    SV* sv, I32 indent, U32 depth)
{
    dVAR;
    register U8 op = PSEUDO;	/* Arbitrary non-END op. */
    register const regnode *next;
    const regnode *optstart= NULL;
    
    RXi_GET_DECL(r,ri);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_DUMPUNTIL;

#ifdef DEBUG_DUMPUNTIL
    PerlIO_printf(Perl_debug_log, "--- %d : %d - %d - %d\n",indent,node-start,
        last ? last-start : 0,plast ? plast-start : 0);
#endif
            
    if (plast && plast < last) 
        last= plast;

    while (PL_regkind[op] != END && (!last || node < last)) {
	/* While that wasn't END last time... */
	NODE_ALIGN(node);
	op = OP(node);
	if (op == CLOSE || op == WHILEM)
	    indent--;
	next = regnext((regnode *)node);

	/* Where, what. */
	if (OP(node) == OPTIMIZED) {
	    if (!optstart && RE_DEBUG_FLAG(RE_DEBUG_COMPILE_OPTIMISE))
	        optstart = node;
	    else
		goto after_print;
	} else
	    CLEAR_OPTSTART;
	
	regprop(r, sv, node);
	PerlIO_printf(Perl_debug_log, "%4"IVdf":%*s%s", (IV)(node - start),
		      (int)(2*indent + 1), "", SvPVX_const(sv));
        
        if (OP(node) != OPTIMIZED) {		      
            if (next == NULL)		/* Next ptr. */
                PerlIO_printf(Perl_debug_log, " (0)");
            else if (PL_regkind[(U8)op] == BRANCH && PL_regkind[OP(next)] != BRANCH )
                PerlIO_printf(Perl_debug_log, " (FAIL)");
            else 
                PerlIO_printf(Perl_debug_log, " (%"IVdf")", (IV)(next - start));
            (void)PerlIO_putc(Perl_debug_log, '\n'); 
        }
        
      after_print:
	if (PL_regkind[(U8)op] == BRANCHJ) {
	    assert(next);
	    {
                register const regnode *nnode = (OP(next) == LONGJMP
					     ? regnext((regnode *)next)
					     : next);
                if (last && nnode > last)
                    nnode = last;
                DUMPUNTIL(NEXTOPER(NEXTOPER(node)), nnode);
	    }
	}
	else if (PL_regkind[(U8)op] == BRANCH) {
	    assert(next);
	    DUMPUNTIL(NEXTOPER(node), next);
	}
	else if ( PL_regkind[(U8)op]  == TRIE ) {
	    const regnode *this_trie = node;
	    const char op = OP(node);
            const U32 n = ARG(node);
	    const reg_ac_data * const ac = op>=AHOCORASICK ?
               (reg_ac_data *)ri->data->data[n] :
               NULL;
	    const reg_trie_data * const trie =
	        (reg_trie_data*)ri->data->data[op<AHOCORASICK ? n : ac->trie];
#ifdef DEBUGGING
	    AV *const trie_words = MUTABLE_AV(ri->data->data[n + TRIE_WORDS_OFFSET]);
#endif
	    const regnode *nextbranch= NULL;
	    I32 word_idx;
            sv_setpvs(sv, "");
	    for (word_idx= 0; word_idx < (I32)trie->wordcount; word_idx++) {
		SV ** const elem_ptr = av_fetch(trie_words,word_idx,0);
		
                PerlIO_printf(Perl_debug_log, "%*s%s ",
                   (int)(2*(indent+3)), "",
                    elem_ptr ? pv_pretty(sv, SvPV_nolen_const(*elem_ptr), SvCUR(*elem_ptr), 60,
	                    PL_colors[0], PL_colors[1],
	                    (SvUTF8(*elem_ptr) ? PERL_PV_ESCAPE_UNI : 0) |
	                    PERL_PV_PRETTY_ELLIPSES    |
	                    PERL_PV_PRETTY_LTGT
                            )
                            : "???"
                );
                if (trie->jump) {
                    U16 dist= trie->jump[word_idx+1];
		    PerlIO_printf(Perl_debug_log, "(%"UVuf")\n",
				  (UV)((dist ? this_trie + dist : next) - start));
                    if (dist) {
                        if (!nextbranch)
                            nextbranch= this_trie + trie->jump[0];    
			DUMPUNTIL(this_trie + dist, nextbranch);
                    }
                    if (nextbranch && PL_regkind[OP(nextbranch)]==BRANCH)
                        nextbranch= regnext((regnode *)nextbranch);
                } else {
                    PerlIO_printf(Perl_debug_log, "\n");
		}
	    }
	    if (last && next > last)
	        node= last;
	    else
	        node= next;
	}
	else if ( op == CURLY ) {   /* "next" might be very big: optimizer */
	    DUMPUNTIL(NEXTOPER(node) + EXTRA_STEP_2ARGS,
                    NEXTOPER(node) + EXTRA_STEP_2ARGS + 1);
	}
	else if (PL_regkind[(U8)op] == CURLY && op != CURLYX) {
	    assert(next);
	    DUMPUNTIL(NEXTOPER(node) + EXTRA_STEP_2ARGS, next);
	}
	else if ( op == PLUS || op == STAR) {
	    DUMPUNTIL(NEXTOPER(node), NEXTOPER(node) + 1);
	}
	else if (op == ANYOF) {
	    /* arglen 1 + class block */
	    node += 1 + ((ANYOF_FLAGS(node) & ANYOF_LARGE)
		    ? ANYOF_CLASS_SKIP : ANYOF_SKIP);
	    node = NEXTOPER(node);
	}
	else if (PL_regkind[(U8)op] == EXACT) {
            /* Literal string, where present. */
	    node += NODE_SZ_STR(node) - 1;
	    node = NEXTOPER(node);
	}
	else {
	    node = NEXTOPER(node);
	    node += regarglen[(U8)op];
	}
	if (op == CURLYX || op == OPEN)
	    indent++;
    }
    CLEAR_OPTSTART;
#ifdef DEBUG_DUMPUNTIL    
    PerlIO_printf(Perl_debug_log, "--- %d\n", (int)indent);
#endif
    return node;
}

#endif	/* DEBUGGING */

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
