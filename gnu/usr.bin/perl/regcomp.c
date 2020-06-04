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
 * regexec to pregcomp and pregexec in order to avoid conflicts
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

#define REG_COMP_C
#ifdef PERL_IN_XSUB_RE
#  include "re_comp.h"
EXTERN_C const struct regexp_engine my_reg_engine;
#else
#  include "regcomp.h"
#endif

#include "dquote_inline.h"
#include "invlist_inline.h"
#include "unicode_constants.h"

#define HAS_NONLATIN1_FOLD_CLOSURE(i) \
 _HAS_NONLATIN1_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(i)
#define HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(i) \
 _HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE_ONLY_FOR_USE_BY_REGCOMP_DOT_C_AND_REGEXEC_DOT_C(i)
#define IS_NON_FINAL_FOLD(c) _IS_NON_FINAL_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c)
#define IS_IN_SOME_FOLD_L1(c) _IS_IN_SOME_FOLD_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c)

#ifndef STATIC
#define	STATIC	static
#endif

/* this is a chain of data about sub patterns we are processing that
   need to be handled separately/specially in study_chunk. Its so
   we can simulate recursion without losing state.  */
struct scan_frame;
typedef struct scan_frame {
    regnode *last_regnode;      /* last node to process in this frame */
    regnode *next_regnode;      /* next node to process when last is reached */
    U32 prev_recursed_depth;
    I32 stopparen;              /* what stopparen do we use */
    bool in_gosub;              /* this or an outer frame is for GOSUB */

    struct scan_frame *this_prev_frame; /* this previous frame */
    struct scan_frame *prev_frame;      /* previous frame */
    struct scan_frame *next_frame;      /* next frame */
} scan_frame;

/* Certain characters are output as a sequence with the first being a
 * backslash. */
#define isBACKSLASHED_PUNCT(c)  strchr("-[]\\^", c)


struct RExC_state_t {
    U32		flags;			/* RXf_* are we folding, multilining? */
    U32		pm_flags;		/* PMf_* stuff from the calling PMOP */
    char	*precomp;		/* uncompiled string. */
    char	*precomp_end;		/* pointer to end of uncompiled string. */
    REGEXP	*rx_sv;			/* The SV that is the regexp. */
    regexp	*rx;                    /* perl core regexp structure */
    regexp_internal	*rxi;           /* internal data for regexp object
                                           pprivate field */
    char	*start;			/* Start of input for compile */
    char	*end;			/* End of input for compile */
    char	*parse;			/* Input-scan pointer. */
    char        *copy_start;            /* start of copy of input within
                                           constructed parse string */
    char        *save_copy_start;       /* Provides one level of saving
                                           and restoring 'copy_start' */
    char        *copy_start_in_input;   /* Position in input string
                                           corresponding to copy_start */
    SSize_t	whilem_seen;		/* number of WHILEM in this expr */
    regnode	*emit_start;		/* Start of emitted-code area */
    regnode_offset emit;		/* Code-emit pointer */
    I32		naughty;		/* How bad is this pattern? */
    I32		sawback;		/* Did we see \1, ...? */
    U32		seen;
    SSize_t	size;			/* Number of regnode equivalents in
                                           pattern */

    /* position beyond 'precomp' of the warning message furthest away from
     * 'precomp'.  During the parse, no warnings are raised for any problems
     * earlier in the parse than this position.  This works if warnings are
     * raised the first time a given spot is parsed, and if only one
     * independent warning is raised for any given spot */
    Size_t	latest_warn_offset;

    I32         npar;                   /* Capture buffer count so far in the
                                           parse, (OPEN) plus one. ("par" 0 is
                                           the whole pattern)*/
    I32         total_par;              /* During initial parse, is either 0,
                                           or -1; the latter indicating a
                                           reparse is needed.  After that pass,
                                           it is what 'npar' became after the
                                           pass.  Hence, it being > 0 indicates
                                           we are in a reparse situation */
    I32		nestroot;		/* root parens we are in - used by
                                           accept */
    I32		seen_zerolen;
    regnode_offset *open_parens;	/* offsets to open parens */
    regnode_offset *close_parens;	/* offsets to close parens */
    I32      parens_buf_size;           /* #slots malloced open/close_parens */
    regnode     *end_op;                /* END node in program */
    I32		utf8;		/* whether the pattern is utf8 or not */
    I32		orig_utf8;	/* whether the pattern was originally in utf8 */
				/* XXX use this for future optimisation of case
				 * where pattern must be upgraded to utf8. */
    I32		uni_semantics;	/* If a d charset modifier should use unicode
				   rules, even if the pattern is not in
				   utf8 */
    HV		*paren_names;		/* Paren names */

    regnode	**recurse;		/* Recurse regops */
    I32         recurse_count;          /* Number of recurse regops we have generated */
    U8          *study_chunk_recursed;  /* bitmap of which subs we have moved
                                           through */
    U32         study_chunk_recursed_bytes;  /* bytes in bitmap */
    I32		in_lookbehind;
    I32		contains_locale;
    I32		override_recoding;
#ifdef EBCDIC
    I32		recode_x_to_native;
#endif
    I32		in_multi_char_class;
    struct reg_code_blocks *code_blocks;/* positions of literal (?{})
					    within pattern */
    int		code_index;		/* next code_blocks[] slot */
    SSize_t     maxlen;                        /* mininum possible number of chars in string to match */
    scan_frame *frame_head;
    scan_frame *frame_last;
    U32         frame_count;
    AV         *warn_text;
    HV         *unlexed_names;
#ifdef ADD_TO_REGEXEC
    char 	*starttry;		/* -Dr: where regtry was called. */
#define RExC_starttry	(pRExC_state->starttry)
#endif
    SV		*runtime_code_qr;	/* qr with the runtime code blocks */
#ifdef DEBUGGING
    const char  *lastparse;
    I32         lastnum;
    AV          *paren_name_list;       /* idx -> name */
    U32         study_chunk_recursed_count;
    SV          *mysv1;
    SV          *mysv2;

#define RExC_lastparse	(pRExC_state->lastparse)
#define RExC_lastnum	(pRExC_state->lastnum)
#define RExC_paren_name_list    (pRExC_state->paren_name_list)
#define RExC_study_chunk_recursed_count    (pRExC_state->study_chunk_recursed_count)
#define RExC_mysv	(pRExC_state->mysv1)
#define RExC_mysv1	(pRExC_state->mysv1)
#define RExC_mysv2	(pRExC_state->mysv2)

#endif
    bool        seen_d_op;
    bool        strict;
    bool        study_started;
    bool        in_script_run;
    bool        use_BRANCHJ;
};

#define RExC_flags	(pRExC_state->flags)
#define RExC_pm_flags	(pRExC_state->pm_flags)
#define RExC_precomp	(pRExC_state->precomp)
#define RExC_copy_start_in_input (pRExC_state->copy_start_in_input)
#define RExC_copy_start_in_constructed  (pRExC_state->copy_start)
#define RExC_save_copy_start_in_constructed  (pRExC_state->save_copy_start)
#define RExC_precomp_end (pRExC_state->precomp_end)
#define RExC_rx_sv	(pRExC_state->rx_sv)
#define RExC_rx		(pRExC_state->rx)
#define RExC_rxi	(pRExC_state->rxi)
#define RExC_start	(pRExC_state->start)
#define RExC_end	(pRExC_state->end)
#define RExC_parse	(pRExC_state->parse)
#define RExC_latest_warn_offset (pRExC_state->latest_warn_offset )
#define RExC_whilem_seen	(pRExC_state->whilem_seen)
#define RExC_seen_d_op (pRExC_state->seen_d_op) /* Seen something that differs
                                                   under /d from /u ? */


#ifdef RE_TRACK_PATTERN_OFFSETS
#  define RExC_offsets	(RExC_rxi->u.offsets) /* I am not like the
                                                         others */
#endif
#define RExC_emit	(pRExC_state->emit)
#define RExC_emit_start	(pRExC_state->emit_start)
#define RExC_sawback	(pRExC_state->sawback)
#define RExC_seen	(pRExC_state->seen)
#define RExC_size	(pRExC_state->size)
#define RExC_maxlen        (pRExC_state->maxlen)
#define RExC_npar	(pRExC_state->npar)
#define RExC_total_parens	(pRExC_state->total_par)
#define RExC_parens_buf_size	(pRExC_state->parens_buf_size)
#define RExC_nestroot   (pRExC_state->nestroot)
#define RExC_seen_zerolen	(pRExC_state->seen_zerolen)
#define RExC_utf8	(pRExC_state->utf8)
#define RExC_uni_semantics	(pRExC_state->uni_semantics)
#define RExC_orig_utf8	(pRExC_state->orig_utf8)
#define RExC_open_parens	(pRExC_state->open_parens)
#define RExC_close_parens	(pRExC_state->close_parens)
#define RExC_end_op	(pRExC_state->end_op)
#define RExC_paren_names	(pRExC_state->paren_names)
#define RExC_recurse	(pRExC_state->recurse)
#define RExC_recurse_count	(pRExC_state->recurse_count)
#define RExC_study_chunk_recursed        (pRExC_state->study_chunk_recursed)
#define RExC_study_chunk_recursed_bytes  \
                                   (pRExC_state->study_chunk_recursed_bytes)
#define RExC_in_lookbehind	(pRExC_state->in_lookbehind)
#define RExC_contains_locale	(pRExC_state->contains_locale)
#ifdef EBCDIC
#   define RExC_recode_x_to_native (pRExC_state->recode_x_to_native)
#endif
#define RExC_in_multi_char_class (pRExC_state->in_multi_char_class)
#define RExC_frame_head (pRExC_state->frame_head)
#define RExC_frame_last (pRExC_state->frame_last)
#define RExC_frame_count (pRExC_state->frame_count)
#define RExC_strict (pRExC_state->strict)
#define RExC_study_started      (pRExC_state->study_started)
#define RExC_warn_text (pRExC_state->warn_text)
#define RExC_in_script_run      (pRExC_state->in_script_run)
#define RExC_use_BRANCHJ        (pRExC_state->use_BRANCHJ)
#define RExC_unlexed_names (pRExC_state->unlexed_names)

/* Heuristic check on the complexity of the pattern: if TOO_NAUGHTY, we set
 * a flag to disable back-off on the fixed/floating substrings - if it's
 * a high complexity pattern we assume the benefit of avoiding a full match
 * is worth the cost of checking for the substrings even if they rarely help.
 */
#define RExC_naughty	(pRExC_state->naughty)
#define TOO_NAUGHTY (10)
#define MARK_NAUGHTY(add) \
    if (RExC_naughty < TOO_NAUGHTY) \
        RExC_naughty += (add)
#define MARK_NAUGHTY_EXP(exp, add) \
    if (RExC_naughty < TOO_NAUGHTY) \
        RExC_naughty += RExC_naughty / (exp) + (add)

#define	ISMULT1(c)	((c) == '*' || (c) == '+' || (c) == '?')
#define	ISMULT2(s)	((*s) == '*' || (*s) == '+' || (*s) == '?' || \
	((*s) == '{' && regcurly(s)))

/*
 * Flags to be passed up and down.
 */
#define	WORST		0	/* Worst case. */
#define	HASWIDTH	0x01	/* Known to not match null strings, could match
                                   non-null ones. */

/* Simple enough to be STAR/PLUS operand; in an EXACTish node must be a single
 * character.  (There needs to be a case: in the switch statement in regexec.c
 * for any node marked SIMPLE.)  Note that this is not the same thing as
 * REGNODE_SIMPLE */
#define	SIMPLE		0x02
#define	SPSTART		0x04	/* Starts with * or + */
#define POSTPONED	0x08    /* (?1),(?&name), (??{...}) or similar */
#define TRYAGAIN	0x10	/* Weeded out a declaration. */
#define RESTART_PARSE   0x20    /* Need to redo the parse */
#define NEED_UTF8       0x40    /* In conjunction with RESTART_PARSE, need to
                                   calcuate sizes as UTF-8 */

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

#define REQUIRE_UTF8(flagp) STMT_START {                                   \
                                     if (!UTF) {                           \
                                         *flagp = RESTART_PARSE|NEED_UTF8; \
                                         return 0;                         \
                                     }                                     \
                             } STMT_END

/* Change from /d into /u rules, and restart the parse.  RExC_uni_semantics is
 * a flag that indicates we need to override /d with /u as a result of
 * something in the pattern.  It should only be used in regards to calling
 * set_regex_charset() or get_regex_charse() */
#define REQUIRE_UNI_RULES(flagp, restart_retval)                            \
    STMT_START {                                                            \
            if (DEPENDS_SEMANTICS) {                                        \
                set_regex_charset(&RExC_flags, REGEX_UNICODE_CHARSET);      \
                RExC_uni_semantics = 1;                                     \
                if (RExC_seen_d_op && LIKELY(! IN_PARENS_PASS)) {           \
                    /* No need to restart the parse if we haven't seen      \
                     * anything that differs between /u and /d, and no need \
                     * to restart immediately if we're going to reparse     \
                     * anyway to count parens */                            \
                    *flagp |= RESTART_PARSE;                                \
                    return restart_retval;                                  \
                }                                                           \
            }                                                               \
    } STMT_END

#define REQUIRE_BRANCHJ(flagp, restart_retval)                              \
    STMT_START {                                                            \
                RExC_use_BRANCHJ = 1;                                       \
                *flagp |= RESTART_PARSE;                                    \
                return restart_retval;                                      \
    } STMT_END

/* Until we have completed the parse, we leave RExC_total_parens at 0 or
 * less.  After that, it must always be positive, because the whole re is
 * considered to be surrounded by virtual parens.  Setting it to negative
 * indicates there is some construct that needs to know the actual number of
 * parens to be properly handled.  And that means an extra pass will be
 * required after we've counted them all */
#define ALL_PARENS_COUNTED (RExC_total_parens > 0)
#define REQUIRE_PARENS_PASS                                                 \
    STMT_START {  /* No-op if have completed a pass */                      \
                    if (! ALL_PARENS_COUNTED) RExC_total_parens = -1;       \
    } STMT_END
#define IN_PARENS_PASS (RExC_total_parens < 0)


/* This is used to return failure (zero) early from the calling function if
 * various flags in 'flags' are set.  Two flags always cause a return:
 * 'RESTART_PARSE' and 'NEED_UTF8'.   'extra' can be used to specify any
 * additional flags that should cause a return; 0 if none.  If the return will
 * be done, '*flagp' is first set to be all of the flags that caused the
 * return. */
#define RETURN_FAIL_ON_RESTART_OR_FLAGS(flags,flagp,extra)                  \
    STMT_START {                                                            \
            if ((flags) & (RESTART_PARSE|NEED_UTF8|(extra))) {              \
                *(flagp) = (flags) & (RESTART_PARSE|NEED_UTF8|(extra));     \
                return 0;                                                   \
            }                                                               \
    } STMT_END

#define MUST_RESTART(flags) ((flags) & (RESTART_PARSE))

#define RETURN_FAIL_ON_RESTART(flags,flagp)                                 \
                        RETURN_FAIL_ON_RESTART_OR_FLAGS( flags, flagp, 0)
#define RETURN_FAIL_ON_RESTART_FLAGP(flagp)                                 \
                                    if (MUST_RESTART(*(flagp))) return 0

/* This converts the named class defined in regcomp.h to its equivalent class
 * number defined in handy.h. */
#define namedclass_to_classnum(class)  ((int) ((class) / 2))
#define classnum_to_namedclass(classnum)  ((classnum) * 2)

#define _invlist_union_complement_2nd(a, b, output) \
                        _invlist_union_maybe_complement_2nd(a, b, TRUE, output)
#define _invlist_intersection_complement_2nd(a, b, output) \
                 _invlist_intersection_maybe_complement_2nd(a, b, TRUE, output)

/* About scan_data_t.

  During optimisation we recurse through the regexp program performing
  various inplace (keyhole style) optimisations. In addition study_chunk
  and scan_commit populate this data structure with information about
  what strings MUST appear in the pattern. We look for the longest
  string that must appear at a fixed location, and we look for the
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

  - min_offset
    This is the position the string must appear at, or not before.
    It also implicitly (when combined with minlenp) tells us how many
    characters must match before the string we are searching for.
    Likewise when combined with minlenp and the length of the string it
    tells us how many characters must appear after the string we have
    found.

  - max_offset
    Only used for floating strings. This is the rightmost point that
    the string can appear at. If set to SSize_t_MAX it indicates that the
    string can occur infinitely far to the right.
    For fixed strings, it is equal to min_offset.

  - minlenp
    A pointer to the minimum number of characters of the pattern that the
    string was found inside. This is important as in the case of positive
    lookahead or positive lookbehind we can have multiple patterns
    involved. Consider

    /(?=FOO).*F/

    The minimum length of the pattern overall is 3, the minimum length
    of the lookahead part is 3, but the minimum length of the part that
    will actually match is 1. So 'FOO's minimum length is 3, but the
    minimum length for the F is 1. This is important as the minimum length
    is used to determine offsets in front of and behind the string being
    looked for.  Since strings can be composites this is the length of the
    pattern at the time it was committed with a scan_commit. Note that
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

struct scan_data_substrs {
    SV      *str;       /* longest substring found in pattern */
    SSize_t min_offset; /* earliest point in string it can appear */
    SSize_t max_offset; /* latest point in string it can appear */
    SSize_t *minlenp;   /* pointer to the minlen relevant to the string */
    SSize_t lookbehind; /* is the pos of the string modified by LB */
    I32 flags;          /* per substring SF_* and SCF_* flags */
};

typedef struct scan_data_t {
    /*I32 len_min;      unused */
    /*I32 len_delta;    unused */
    SSize_t pos_min;
    SSize_t pos_delta;
    SV *last_found;
    SSize_t last_end;	    /* min value, <0 unless valid. */
    SSize_t last_start_min;
    SSize_t last_start_max;
    U8      cur_is_floating; /* whether the last_* values should be set as
                              * the next fixed (0) or floating (1)
                              * substring */

    /* [0] is longest fixed substring so far, [1] is longest float so far */
    struct scan_data_substrs  substrs[2];

    I32 flags;             /* common SF_* and SCF_* flags */
    I32 whilem_c;
    SSize_t *last_closep;
    regnode_ssc *start_class;
} scan_data_t;

/*
 * Forward declarations for pregcomp()'s friends.
 */

static const scan_data_t zero_scan_data = {
    0, 0, NULL, 0, 0, 0, 0,
    {
        { NULL, 0, 0, 0, 0, 0 },
        { NULL, 0, 0, 0, 0, 0 },
    },
    0, 0, NULL, NULL
};

/* study flags */

#define SF_BEFORE_SEOL		0x0001
#define SF_BEFORE_MEOL		0x0002
#define SF_BEFORE_EOL		(SF_BEFORE_SEOL|SF_BEFORE_MEOL)

#define SF_IS_INF		0x0040
#define SF_HAS_PAR		0x0080
#define SF_IN_PAR		0x0100
#define SF_HAS_EVAL		0x0200


/* SCF_DO_SUBSTR is the flag that tells the regexp analyzer to track the
 * longest substring in the pattern. When it is not set the optimiser keeps
 * track of position, but does not keep track of the actual strings seen,
 *
 * So for instance /foo/ will be parsed with SCF_DO_SUBSTR being true, but
 * /foo/i will not.
 *
 * Similarly, /foo.*(blah|erm|huh).*fnorble/ will have "foo" and "fnorble"
 * parsed with SCF_DO_SUBSTR on, but while processing the (...) it will be
 * turned off because of the alternation (BRANCH). */
#define SCF_DO_SUBSTR		0x0400

#define SCF_DO_STCLASS_AND	0x0800
#define SCF_DO_STCLASS_OR	0x1000
#define SCF_DO_STCLASS		(SCF_DO_STCLASS_AND|SCF_DO_STCLASS_OR)
#define SCF_WHILEM_VISITED_POS	0x2000

#define SCF_TRIE_RESTUDY        0x4000 /* Do restudy? */
#define SCF_SEEN_ACCEPT         0x8000
#define SCF_TRIE_DOING_RESTUDY 0x10000
#define SCF_IN_DEFINE          0x20000




#define UTF cBOOL(RExC_utf8)

/* The enums for all these are ordered so things work out correctly */
#define LOC (get_regex_charset(RExC_flags) == REGEX_LOCALE_CHARSET)
#define DEPENDS_SEMANTICS (get_regex_charset(RExC_flags)                    \
                                                     == REGEX_DEPENDS_CHARSET)
#define UNI_SEMANTICS (get_regex_charset(RExC_flags) == REGEX_UNICODE_CHARSET)
#define AT_LEAST_UNI_SEMANTICS (get_regex_charset(RExC_flags)                \
                                                     >= REGEX_UNICODE_CHARSET)
#define ASCII_RESTRICTED (get_regex_charset(RExC_flags)                      \
                                            == REGEX_ASCII_RESTRICTED_CHARSET)
#define AT_LEAST_ASCII_RESTRICTED (get_regex_charset(RExC_flags)             \
                                            >= REGEX_ASCII_RESTRICTED_CHARSET)
#define ASCII_FOLD_RESTRICTED (get_regex_charset(RExC_flags)                 \
                                        == REGEX_ASCII_MORE_RESTRICTED_CHARSET)

#define FOLD cBOOL(RExC_flags & RXf_PMf_FOLD)

/* For programs that want to be strictly Unicode compatible by dying if any
 * attempt is made to match a non-Unicode code point against a Unicode
 * property.  */
#define ALWAYS_WARN_SUPER  ckDEAD(packWARN(WARN_NON_UNICODE))

#define OOB_NAMEDCLASS		-1

/* There is no code point that is out-of-bounds, so this is problematic.  But
 * its only current use is to initialize a variable that is always set before
 * looked at. */
#define OOB_UNICODE		0xDEADBEEF

#define CHR_SVLEN(sv) (UTF ? sv_len_utf8(sv) : SvCUR(sv))


/* length of regex to show in messages that don't mark a position within */
#define RegexLengthToShowInErrorMessages 127

/*
 * If MARKER[12] are adjusted, be sure to adjust the constants at the top
 * of t/op/regmesg.t, the tests in t/op/re_tests, and those in
 * op/pragma/warn/regcomp.
 */
#define MARKER1 "<-- HERE"    /* marker as it appears in the description */
#define MARKER2 " <-- HERE "  /* marker as it appears within the regex */

#define REPORT_LOCATION " in regex; marked by " MARKER1    \
                        " in m/%" UTF8f MARKER2 "%" UTF8f "/"

/* The code in this file in places uses one level of recursion with parsing
 * rebased to an alternate string constructed by us in memory.  This can take
 * the form of something that is completely different from the input, or
 * something that uses the input as part of the alternate.  In the first case,
 * there should be no possibility of an error, as we are in complete control of
 * the alternate string.  But in the second case we don't completely control
 * the input portion, so there may be errors in that.  Here's an example:
 *      /[abc\x{DF}def]/ui
 * is handled specially because \x{df} folds to a sequence of more than one
 * character: 'ss'.  What is done is to create and parse an alternate string,
 * which looks like this:
 *      /(?:\x{DF}|[abc\x{DF}def])/ui
 * where it uses the input unchanged in the middle of something it constructs,
 * which is a branch for the DF outside the character class, and clustering
 * parens around the whole thing. (It knows enough to skip the DF inside the
 * class while in this substitute parse.) 'abc' and 'def' may have errors that
 * need to be reported.  The general situation looks like this:
 *
 *                                       |<------- identical ------>|
 *              sI                       tI               xI       eI
 * Input:       ---------------------------------------------------------------
 * Constructed:         ---------------------------------------------------
 *                      sC               tC               xC       eC     EC
 *                                       |<------- identical ------>|
 *
 * sI..eI   is the portion of the input pattern we are concerned with here.
 * sC..EC   is the constructed substitute parse string.
 *  sC..tC  is constructed by us
 *  tC..eC  is an exact duplicate of the portion of the input pattern tI..eI.
 *          In the diagram, these are vertically aligned.
 *  eC..EC  is also constructed by us.
 * xC       is the position in the substitute parse string where we found a
 *          problem.
 * xI       is the position in the original pattern corresponding to xC.
 *
 * We want to display a message showing the real input string.  Thus we need to
 * translate from xC to xI.  We know that xC >= tC, since the portion of the
 * string sC..tC has been constructed by us, and so shouldn't have errors.  We
 * get:
 *      xI = tI + (xC - tC)
 *
 * When the substitute parse is constructed, the code needs to set:
 *      RExC_start (sC)
 *      RExC_end (eC)
 *      RExC_copy_start_in_input  (tI)
 *      RExC_copy_start_in_constructed (tC)
 * and restore them when done.
 *
 * During normal processing of the input pattern, both
 * 'RExC_copy_start_in_input' and 'RExC_copy_start_in_constructed' are set to
 * sI, so that xC equals xI.
 */

#define sI              RExC_precomp
#define eI              RExC_precomp_end
#define sC              RExC_start
#define eC              RExC_end
#define tI              RExC_copy_start_in_input
#define tC              RExC_copy_start_in_constructed
#define xI(xC)          (tI + (xC - tC))
#define xI_offset(xC)   (xI(xC) - sI)

#define REPORT_LOCATION_ARGS(xC)                                            \
    UTF8fARG(UTF,                                                           \
             (xI(xC) > eI) /* Don't run off end */                          \
              ? eI - sI   /* Length before the <--HERE */                   \
              : ((xI_offset(xC) >= 0)                                       \
                 ? xI_offset(xC)                                            \
                 : (Perl_croak(aTHX_ "panic: %s: %d: negative offset: %"    \
                                    IVdf " trying to output message for "   \
                                    " pattern %.*s",                        \
                                    __FILE__, __LINE__, (IV) xI_offset(xC), \
                                    ((int) (eC - sC)), sC), 0)),            \
             sI),         /* The input pattern printed up to the <--HERE */ \
    UTF8fARG(UTF,                                                           \
             (xI(xC) > eI) ? 0 : eI - xI(xC), /* Length after <--HERE */    \
             (xI(xC) > eI) ? eI : xI(xC))     /* pattern after <--HERE */

/* Used to point after bad bytes for an error message, but avoid skipping
 * past a nul byte. */
#define SKIP_IF_CHAR(s, e) (!*(s) ? 0 : UTF ? UTF8_SAFE_SKIP(s, e) : 1)

/* Set up to clean up after our imminent demise */
#define PREPARE_TO_DIE                                                      \
    STMT_START {					                    \
        if (RExC_rx_sv)                                                     \
            SAVEFREESV(RExC_rx_sv);                                         \
        if (RExC_open_parens)                                               \
            SAVEFREEPV(RExC_open_parens);                                   \
        if (RExC_close_parens)                                              \
            SAVEFREEPV(RExC_close_parens);                                  \
    } STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then calls Perl_croak with the given
 * arg. Show regex, up to a maximum length. If it's too long, chop and add
 * "...".
 */
#define _FAIL(code) STMT_START {					\
    const char *ellipses = "";						\
    IV len = RExC_precomp_end - RExC_precomp;				\
									\
    PREPARE_TO_DIE;						        \
    if (len > RegexLengthToShowInErrorMessages) {			\
	/* chop 10 shorter than the max, to ensure meaning of "..." */	\
	len = RegexLengthToShowInErrorMessages - 10;			\
	ellipses = "...";						\
    }									\
    code;                                                               \
} STMT_END

#define	FAIL(msg) _FAIL(			    \
    Perl_croak(aTHX_ "%s in regex m/%" UTF8f "%s/",	    \
	    msg, UTF8fARG(UTF, len, RExC_precomp), ellipses))

#define	FAIL2(msg,arg) _FAIL(			    \
    Perl_croak(aTHX_ msg " in regex m/%" UTF8f "%s/",	    \
	    arg, UTF8fARG(UTF, len, RExC_precomp), ellipses))

/*
 * Simple_vFAIL -- like FAIL, but marks the current location in the scan
 */
#define	Simple_vFAIL(m) STMT_START {					\
    Perl_croak(aTHX_ "%s" REPORT_LOCATION,				\
	    m, REPORT_LOCATION_ARGS(RExC_parse));	                \
} STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL()
 */
#define	vFAIL(m) STMT_START {				\
    PREPARE_TO_DIE;                                     \
    Simple_vFAIL(m);					\
} STMT_END

/*
 * Like Simple_vFAIL(), but accepts two arguments.
 */
#define	Simple_vFAIL2(m,a1) STMT_START {			\
    S_re_croak2(aTHX_ UTF, m, REPORT_LOCATION, a1,		\
                      REPORT_LOCATION_ARGS(RExC_parse));	\
} STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL2().
 */
#define	vFAIL2(m,a1) STMT_START {			\
    PREPARE_TO_DIE;                                     \
    Simple_vFAIL2(m, a1);				\
} STMT_END


/*
 * Like Simple_vFAIL(), but accepts three arguments.
 */
#define	Simple_vFAIL3(m, a1, a2) STMT_START {			\
    S_re_croak2(aTHX_ UTF, m, REPORT_LOCATION, a1, a2,		\
	    REPORT_LOCATION_ARGS(RExC_parse));	                \
} STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL3().
 */
#define	vFAIL3(m,a1,a2) STMT_START {			\
    PREPARE_TO_DIE;                                     \
    Simple_vFAIL3(m, a1, a2);				\
} STMT_END

/*
 * Like Simple_vFAIL(), but accepts four arguments.
 */
#define	Simple_vFAIL4(m, a1, a2, a3) STMT_START {		\
    S_re_croak2(aTHX_ UTF, m, REPORT_LOCATION, a1, a2, a3,	\
	    REPORT_LOCATION_ARGS(RExC_parse));	                \
} STMT_END

#define	vFAIL4(m,a1,a2,a3) STMT_START {			\
    PREPARE_TO_DIE;                                     \
    Simple_vFAIL4(m, a1, a2, a3);			\
} STMT_END

/* A specialized version of vFAIL2 that works with UTF8f */
#define vFAIL2utf8f(m, a1) STMT_START {             \
    PREPARE_TO_DIE;                                 \
    S_re_croak2(aTHX_ UTF, m, REPORT_LOCATION, a1,  \
            REPORT_LOCATION_ARGS(RExC_parse));      \
} STMT_END

#define vFAIL3utf8f(m, a1, a2) STMT_START {             \
    PREPARE_TO_DIE;                                     \
    S_re_croak2(aTHX_ UTF, m, REPORT_LOCATION, a1, a2,  \
            REPORT_LOCATION_ARGS(RExC_parse));          \
} STMT_END

/* Setting this to NULL is a signal to not output warnings */
#define TURN_OFF_WARNINGS_IN_SUBSTITUTE_PARSE                               \
    STMT_START {                                                            \
      RExC_save_copy_start_in_constructed  = RExC_copy_start_in_constructed;\
      RExC_copy_start_in_constructed = NULL;                                \
    } STMT_END
#define RESTORE_WARNINGS                                                    \
    RExC_copy_start_in_constructed = RExC_save_copy_start_in_constructed

/* Since a warning can be generated multiple times as the input is reparsed, we
 * output it the first time we come to that point in the parse, but suppress it
 * otherwise.  'RExC_copy_start_in_constructed' being NULL is a flag to not
 * generate any warnings */
#define TO_OUTPUT_WARNINGS(loc)                                         \
  (   RExC_copy_start_in_constructed                                    \
   && ((xI(loc)) - RExC_precomp) > (Ptrdiff_t) RExC_latest_warn_offset)

/* After we've emitted a warning, we save the position in the input so we don't
 * output it again */
#define UPDATE_WARNINGS_LOC(loc)                                        \
    STMT_START {                                                        \
        if (TO_OUTPUT_WARNINGS(loc)) {                                  \
            RExC_latest_warn_offset = MAX(sI, MIN(eI, xI(loc)))         \
                                                       - RExC_precomp;  \
        }                                                               \
    } STMT_END

/* 'warns' is the output of the packWARNx macro used in 'code' */
#define _WARN_HELPER(loc, warns, code)                                  \
    STMT_START {                                                        \
        if (! RExC_copy_start_in_constructed) {                         \
            Perl_croak( aTHX_ "panic! %s: %d: Tried to warn when none"  \
                              " expected at '%s'",                      \
                              __FILE__, __LINE__, loc);                 \
        }                                                               \
        if (TO_OUTPUT_WARNINGS(loc)) {                                  \
            if (ckDEAD(warns))                                          \
                PREPARE_TO_DIE;                                         \
            code;                                                       \
            UPDATE_WARNINGS_LOC(loc);                                   \
        }                                                               \
    } STMT_END

/* m is not necessarily a "literal string", in this macro */
#define reg_warn_non_literal_string(loc, m)                             \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                            \
                      Perl_warner(aTHX_ packWARN(WARN_REGEXP),          \
                                       "%s" REPORT_LOCATION,            \
                                  m, REPORT_LOCATION_ARGS(loc)))

#define	ckWARNreg(loc,m) 					        \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                            \
                      Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),       \
                                          m REPORT_LOCATION,	        \
	                                  REPORT_LOCATION_ARGS(loc)))

#define	vWARN(loc, m)           				        \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                            \
                      Perl_warner(aTHX_ packWARN(WARN_REGEXP),          \
                                       m REPORT_LOCATION,               \
                                       REPORT_LOCATION_ARGS(loc)))      \

#define	vWARN_dep(loc, m)           				        \
    _WARN_HELPER(loc, packWARN(WARN_DEPRECATED),                        \
                      Perl_warner(aTHX_ packWARN(WARN_DEPRECATED),      \
                                       m REPORT_LOCATION,               \
	                               REPORT_LOCATION_ARGS(loc)))

#define	ckWARNdep(loc,m)            				        \
    _WARN_HELPER(loc, packWARN(WARN_DEPRECATED),                        \
                      Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED), \
	                                    m REPORT_LOCATION,          \
	                                    REPORT_LOCATION_ARGS(loc)))

#define	ckWARNregdep(loc,m)             				    \
    _WARN_HELPER(loc, packWARN2(WARN_DEPRECATED, WARN_REGEXP),              \
                      Perl_ck_warner_d(aTHX_ packWARN2(WARN_DEPRECATED,     \
                                                      WARN_REGEXP),         \
	                                     m REPORT_LOCATION,             \
	                                     REPORT_LOCATION_ARGS(loc)))

#define	ckWARN2reg_d(loc,m, a1)             				    \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                                \
                      Perl_ck_warner_d(aTHX_ packWARN(WARN_REGEXP),         \
	                                    m REPORT_LOCATION,              \
	                                    a1, REPORT_LOCATION_ARGS(loc)))

#define	ckWARN2reg(loc, m, a1)                                              \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                                \
                      Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),           \
                                          m REPORT_LOCATION,	            \
                                          a1, REPORT_LOCATION_ARGS(loc)))

#define	vWARN3(loc, m, a1, a2)          				    \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                                \
                      Perl_warner(aTHX_ packWARN(WARN_REGEXP),              \
                                       m REPORT_LOCATION,                   \
	                               a1, a2, REPORT_LOCATION_ARGS(loc)))

#define	ckWARN3reg(loc, m, a1, a2)          				    \
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                                \
                      Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),           \
                                          m REPORT_LOCATION,                \
	                                  a1, a2,                           \
                                          REPORT_LOCATION_ARGS(loc)))

#define	vWARN4(loc, m, a1, a2, a3)          				\
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                            \
                      Perl_warner(aTHX_ packWARN(WARN_REGEXP),          \
                                       m REPORT_LOCATION,               \
	                               a1, a2, a3,                      \
                                       REPORT_LOCATION_ARGS(loc)))

#define	ckWARN4reg(loc, m, a1, a2, a3)          			\
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                            \
                      Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),       \
                                          m REPORT_LOCATION,            \
	                                  a1, a2, a3,                   \
                                          REPORT_LOCATION_ARGS(loc)))

#define	vWARN5(loc, m, a1, a2, a3, a4)          			\
    _WARN_HELPER(loc, packWARN(WARN_REGEXP),                            \
                      Perl_warner(aTHX_ packWARN(WARN_REGEXP),          \
                                       m REPORT_LOCATION,		\
	                               a1, a2, a3, a4,                  \
                                       REPORT_LOCATION_ARGS(loc)))

#define	ckWARNexperimental(loc, class, m)                               \
    _WARN_HELPER(loc, packWARN(class),                                  \
                      Perl_ck_warner_d(aTHX_ packWARN(class),           \
                                            m REPORT_LOCATION,          \
                                            REPORT_LOCATION_ARGS(loc)))

/* Convert between a pointer to a node and its offset from the beginning of the
 * program */
#define REGNODE_p(offset)    (RExC_emit_start + (offset))
#define REGNODE_OFFSET(node) ((node) - RExC_emit_start)

/* Macros for recording node offsets.   20001227 mjd@plover.com
 * Nodes are numbered 1, 2, 3, 4.  Node #n's position is recorded in
 * element 2*n-1 of the array.  Element #2n holds the byte length node #n.
 * Element 0 holds the number n.
 * Position is 1 indexed.
 */
#ifndef RE_TRACK_PATTERN_OFFSETS
#define Set_Node_Offset_To_R(offset,byte)
#define Set_Node_Offset(node,byte)
#define Set_Cur_Node_Offset
#define Set_Node_Length_To_R(node,len)
#define Set_Node_Length(node,len)
#define Set_Node_Cur_Length(node,start)
#define Node_Offset(n)
#define Node_Length(n)
#define Set_Node_Offset_Length(node,offset,len)
#define ProgLen(ri) ri->u.proglen
#define SetProgLen(ri,x) ri->u.proglen = x
#define Track_Code(code)
#else
#define ProgLen(ri) ri->u.offsets[0]
#define SetProgLen(ri,x) ri->u.offsets[0] = x
#define Set_Node_Offset_To_R(offset,byte) STMT_START {			\
	MJD_OFFSET_DEBUG(("** (%d) offset of node %d is %d.\n",		\
		    __LINE__, (int)(offset), (int)(byte)));		\
	if((offset) < 0) {						\
	    Perl_croak(aTHX_ "value of node is %d in Offset macro",     \
                                         (int)(offset));                \
	} else {							\
            RExC_offsets[2*(offset)-1] = (byte);	                \
	}								\
} STMT_END

#define Set_Node_Offset(node,byte)                                      \
    Set_Node_Offset_To_R(REGNODE_OFFSET(node), (byte)-RExC_start)
#define Set_Cur_Node_Offset Set_Node_Offset(RExC_emit, RExC_parse)

#define Set_Node_Length_To_R(node,len) STMT_START {			\
	MJD_OFFSET_DEBUG(("** (%d) size of node %d is %d.\n",		\
		__LINE__, (int)(node), (int)(len)));			\
	if((node) < 0) {						\
	    Perl_croak(aTHX_ "value of node is %d in Length macro",     \
                                         (int)(node));                  \
	} else {							\
	    RExC_offsets[2*(node)] = (len);				\
	}								\
} STMT_END

#define Set_Node_Length(node,len) \
    Set_Node_Length_To_R(REGNODE_OFFSET(node), len)
#define Set_Node_Cur_Length(node, start)                \
    Set_Node_Length(node, RExC_parse - start)

/* Get offsets and lengths */
#define Node_Offset(n) (RExC_offsets[2*(REGNODE_OFFSET(n))-1])
#define Node_Length(n) (RExC_offsets[2*(REGNODE_OFFSET(n))])

#define Set_Node_Offset_Length(node,offset,len) STMT_START {	\
    Set_Node_Offset_To_R(REGNODE_OFFSET(node), (offset));	\
    Set_Node_Length_To_R(REGNODE_OFFSET(node), (len));	\
} STMT_END

#define Track_Code(code) STMT_START { code } STMT_END
#endif

#if PERL_ENABLE_EXPERIMENTAL_REGEX_OPTIMISATIONS
#define EXPERIMENTAL_INPLACESCAN
#endif /*PERL_ENABLE_EXPERIMENTAL_REGEX_OPTIMISATIONS*/

#ifdef DEBUGGING
int
Perl_re_printf(pTHX_ const char *fmt, ...)
{
    va_list ap;
    int result;
    PerlIO *f= Perl_debug_log;
    PERL_ARGS_ASSERT_RE_PRINTF;
    va_start(ap, fmt);
    result = PerlIO_vprintf(f, fmt, ap);
    va_end(ap);
    return result;
}

int
Perl_re_indentf(pTHX_ const char *fmt, U32 depth, ...)
{
    va_list ap;
    int result;
    PerlIO *f= Perl_debug_log;
    PERL_ARGS_ASSERT_RE_INDENTF;
    va_start(ap, depth);
    PerlIO_printf(f, "%*s", ( (int)depth % 20 ) * 2, "");
    result = PerlIO_vprintf(f, fmt, ap);
    va_end(ap);
    return result;
}
#endif /* DEBUGGING */

#define DEBUG_RExC_seen()                                                   \
        DEBUG_OPTIMISE_MORE_r({                                             \
            Perl_re_printf( aTHX_ "RExC_seen: ");                           \
                                                                            \
            if (RExC_seen & REG_ZERO_LEN_SEEN)                              \
                Perl_re_printf( aTHX_ "REG_ZERO_LEN_SEEN ");                \
                                                                            \
            if (RExC_seen & REG_LOOKBEHIND_SEEN)                            \
                Perl_re_printf( aTHX_ "REG_LOOKBEHIND_SEEN ");              \
                                                                            \
            if (RExC_seen & REG_GPOS_SEEN)                                  \
                Perl_re_printf( aTHX_ "REG_GPOS_SEEN ");                    \
                                                                            \
            if (RExC_seen & REG_RECURSE_SEEN)                               \
                Perl_re_printf( aTHX_ "REG_RECURSE_SEEN ");                 \
                                                                            \
            if (RExC_seen & REG_TOP_LEVEL_BRANCHES_SEEN)                    \
                Perl_re_printf( aTHX_ "REG_TOP_LEVEL_BRANCHES_SEEN ");      \
                                                                            \
            if (RExC_seen & REG_VERBARG_SEEN)                               \
                Perl_re_printf( aTHX_ "REG_VERBARG_SEEN ");                 \
                                                                            \
            if (RExC_seen & REG_CUTGROUP_SEEN)                              \
                Perl_re_printf( aTHX_ "REG_CUTGROUP_SEEN ");                \
                                                                            \
            if (RExC_seen & REG_RUN_ON_COMMENT_SEEN)                        \
                Perl_re_printf( aTHX_ "REG_RUN_ON_COMMENT_SEEN ");          \
                                                                            \
            if (RExC_seen & REG_UNFOLDED_MULTI_SEEN)                        \
                Perl_re_printf( aTHX_ "REG_UNFOLDED_MULTI_SEEN ");          \
                                                                            \
            if (RExC_seen & REG_UNBOUNDED_QUANTIFIER_SEEN)                  \
                Perl_re_printf( aTHX_ "REG_UNBOUNDED_QUANTIFIER_SEEN ");    \
                                                                            \
            Perl_re_printf( aTHX_ "\n");                                    \
        });

#define DEBUG_SHOW_STUDY_FLAG(flags,flag) \
  if ((flags) & flag) Perl_re_printf( aTHX_  "%s ", #flag)


#ifdef DEBUGGING
static void
S_debug_show_study_flags(pTHX_ U32 flags, const char *open_str,
                                    const char *close_str)
{
    if (!flags)
        return;

    Perl_re_printf( aTHX_  "%s", open_str);
    DEBUG_SHOW_STUDY_FLAG(flags, SF_BEFORE_SEOL);
    DEBUG_SHOW_STUDY_FLAG(flags, SF_BEFORE_MEOL);
    DEBUG_SHOW_STUDY_FLAG(flags, SF_IS_INF);
    DEBUG_SHOW_STUDY_FLAG(flags, SF_HAS_PAR);
    DEBUG_SHOW_STUDY_FLAG(flags, SF_IN_PAR);
    DEBUG_SHOW_STUDY_FLAG(flags, SF_HAS_EVAL);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_DO_SUBSTR);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_DO_STCLASS_AND);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_DO_STCLASS_OR);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_DO_STCLASS);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_WHILEM_VISITED_POS);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_TRIE_RESTUDY);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_SEEN_ACCEPT);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_TRIE_DOING_RESTUDY);
    DEBUG_SHOW_STUDY_FLAG(flags, SCF_IN_DEFINE);
    Perl_re_printf( aTHX_  "%s", close_str);
}


static void
S_debug_studydata(pTHX_ const char *where, scan_data_t *data,
                    U32 depth, int is_inf)
{
    GET_RE_DEBUG_FLAGS_DECL;

    DEBUG_OPTIMISE_MORE_r({
        if (!data)
            return;
        Perl_re_indentf(aTHX_  "%s: Pos:%" IVdf "/%" IVdf " Flags: 0x%" UVXf,
            depth,
            where,
            (IV)data->pos_min,
            (IV)data->pos_delta,
            (UV)data->flags
        );

        S_debug_show_study_flags(aTHX_ data->flags," [","]");

        Perl_re_printf( aTHX_
            " Whilem_c: %" IVdf " Lcp: %" IVdf " %s",
            (IV)data->whilem_c,
            (IV)(data->last_closep ? *((data)->last_closep) : -1),
            is_inf ? "INF " : ""
        );

        if (data->last_found) {
            int i;
            Perl_re_printf(aTHX_
                "Last:'%s' %" IVdf ":%" IVdf "/%" IVdf,
                    SvPVX_const(data->last_found),
                    (IV)data->last_end,
                    (IV)data->last_start_min,
                    (IV)data->last_start_max
            );

            for (i = 0; i < 2; i++) {
                Perl_re_printf(aTHX_
                    " %s%s: '%s' @ %" IVdf "/%" IVdf,
                    data->cur_is_floating == i ? "*" : "",
                    i ? "Float" : "Fixed",
                    SvPVX_const(data->substrs[i].str),
                    (IV)data->substrs[i].min_offset,
                    (IV)data->substrs[i].max_offset
                );
                S_debug_show_study_flags(aTHX_ data->substrs[i].flags," [","]");
            }
        }

        Perl_re_printf( aTHX_ "\n");
    });
}


static void
S_debug_peep(pTHX_ const char *str, const RExC_state_t *pRExC_state,
                regnode *scan, U32 depth, U32 flags)
{
    GET_RE_DEBUG_FLAGS_DECL;

    DEBUG_OPTIMISE_r({
        regnode *Next;

        if (!scan)
            return;
        Next = regnext(scan);
        regprop(RExC_rx, RExC_mysv, scan, NULL, pRExC_state);
        Perl_re_indentf( aTHX_   "%s>%3d: %s (%d)",
            depth,
            str,
            REG_NODE_NUM(scan), SvPV_nolen_const(RExC_mysv),
            Next ? (REG_NODE_NUM(Next)) : 0 );
        S_debug_show_study_flags(aTHX_ flags," [ ","]");
        Perl_re_printf( aTHX_  "\n");
   });
}


#  define DEBUG_STUDYDATA(where, data, depth, is_inf) \
                    S_debug_studydata(aTHX_ where, data, depth, is_inf)

#  define DEBUG_PEEP(str, scan, depth, flags)   \
                    S_debug_peep(aTHX_ str, pRExC_state, scan, depth, flags)

#else
#  define DEBUG_STUDYDATA(where, data, depth, is_inf) NOOP
#  define DEBUG_PEEP(str, scan, depth, flags)         NOOP
#endif


/* =========================================================
 * BEGIN edit_distance stuff.
 *
 * This calculates how many single character changes of any type are needed to
 * transform a string into another one.  It is taken from version 3.1 of
 *
 * https://metacpan.org/pod/Text::Levenshtein::Damerau::XS
 */

/* Our unsorted dictionary linked list.   */
/* Note we use UVs, not chars. */

struct dictionary{
  UV key;
  UV value;
  struct dictionary* next;
};
typedef struct dictionary item;


PERL_STATIC_INLINE item*
push(UV key, item* curr)
{
    item* head;
    Newx(head, 1, item);
    head->key = key;
    head->value = 0;
    head->next = curr;
    return head;
}


PERL_STATIC_INLINE item*
find(item* head, UV key)
{
    item* iterator = head;
    while (iterator){
        if (iterator->key == key){
            return iterator;
        }
        iterator = iterator->next;
    }

    return NULL;
}

PERL_STATIC_INLINE item*
uniquePush(item* head, UV key)
{
    item* iterator = head;

    while (iterator){
        if (iterator->key == key) {
            return head;
        }
        iterator = iterator->next;
    }

    return push(key, head);
}

PERL_STATIC_INLINE void
dict_free(item* head)
{
    item* iterator = head;

    while (iterator) {
        item* temp = iterator;
        iterator = iterator->next;
        Safefree(temp);
    }

    head = NULL;
}

/* End of Dictionary Stuff */

/* All calculations/work are done here */
STATIC int
S_edit_distance(const UV* src,
                const UV* tgt,
                const STRLEN x,             /* length of src[] */
                const STRLEN y,             /* length of tgt[] */
                const SSize_t maxDistance
)
{
    item *head = NULL;
    UV swapCount, swapScore, targetCharCount, i, j;
    UV *scores;
    UV score_ceil = x + y;

    PERL_ARGS_ASSERT_EDIT_DISTANCE;

    /* intialize matrix start values */
    Newx(scores, ( (x + 2) * (y + 2)), UV);
    scores[0] = score_ceil;
    scores[1 * (y + 2) + 0] = score_ceil;
    scores[0 * (y + 2) + 1] = score_ceil;
    scores[1 * (y + 2) + 1] = 0;
    head = uniquePush(uniquePush(head, src[0]), tgt[0]);

    /* work loops    */
    /* i = src index */
    /* j = tgt index */
    for (i=1;i<=x;i++) {
        if (i < x)
            head = uniquePush(head, src[i]);
        scores[(i+1) * (y + 2) + 1] = i;
        scores[(i+1) * (y + 2) + 0] = score_ceil;
        swapCount = 0;

        for (j=1;j<=y;j++) {
            if (i == 1) {
                if(j < y)
                head = uniquePush(head, tgt[j]);
                scores[1 * (y + 2) + (j + 1)] = j;
                scores[0 * (y + 2) + (j + 1)] = score_ceil;
            }

            targetCharCount = find(head, tgt[j-1])->value;
            swapScore = scores[targetCharCount * (y + 2) + swapCount] + i - targetCharCount - 1 + j - swapCount;

            if (src[i-1] != tgt[j-1]){
                scores[(i+1) * (y + 2) + (j + 1)] = MIN(swapScore,(MIN(scores[i * (y + 2) + j], MIN(scores[(i+1) * (y + 2) + j], scores[i * (y + 2) + (j + 1)])) + 1));
            }
            else {
                swapCount = j;
                scores[(i+1) * (y + 2) + (j + 1)] = MIN(scores[i * (y + 2) + j], swapScore);
            }
        }

        find(head, src[i-1])->value = i;
    }

    {
        IV score = scores[(x+1) * (y + 2) + (y + 1)];
        dict_free(head);
        Safefree(scores);
        return (maxDistance != 0 && maxDistance < score)?(-1):score;
    }
}

/* END of edit_distance() stuff
 * ========================================================= */

/* is c a control character for which we have a mnemonic? */
#define isMNEMONIC_CNTRL(c) _IS_MNEMONIC_CNTRL_ONLY_FOR_USE_BY_REGCOMP_DOT_C(c)

STATIC const char *
S_cntrl_to_mnemonic(const U8 c)
{
    /* Returns the mnemonic string that represents character 'c', if one
     * exists; NULL otherwise.  The only ones that exist for the purposes of
     * this routine are a few control characters */

    switch (c) {
        case '\a':       return "\\a";
        case '\b':       return "\\b";
        case ESC_NATIVE: return "\\e";
        case '\f':       return "\\f";
        case '\n':       return "\\n";
        case '\r':       return "\\r";
        case '\t':       return "\\t";
    }

    return NULL;
}

/* Mark that we cannot extend a found fixed substring at this point.
   Update the longest found anchored substring or the longest found
   floating substrings if needed. */

STATIC void
S_scan_commit(pTHX_ const RExC_state_t *pRExC_state, scan_data_t *data,
                    SSize_t *minlenp, int is_inf)
{
    const STRLEN l = CHR_SVLEN(data->last_found);
    SV * const longest_sv = data->substrs[data->cur_is_floating].str;
    const STRLEN old_l = CHR_SVLEN(longest_sv);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_SCAN_COMMIT;

    if ((l >= old_l) && ((l > old_l) || (data->flags & SF_BEFORE_EOL))) {
        const U8 i = data->cur_is_floating;
	SvSetMagicSV(longest_sv, data->last_found);
        data->substrs[i].min_offset = l ? data->last_start_min : data->pos_min;

	if (!i) /* fixed */
	    data->substrs[0].max_offset = data->substrs[0].min_offset;
	else { /* float */
	    data->substrs[1].max_offset = (l
                          ? data->last_start_max
                          : (data->pos_delta > SSize_t_MAX - data->pos_min
					 ? SSize_t_MAX
					 : data->pos_min + data->pos_delta));
	    if (is_inf
		 || (STRLEN)data->substrs[1].max_offset > (STRLEN)SSize_t_MAX)
		data->substrs[1].max_offset = SSize_t_MAX;
        }

        if (data->flags & SF_BEFORE_EOL)
            data->substrs[i].flags |= (data->flags & SF_BEFORE_EOL);
        else
            data->substrs[i].flags &= ~SF_BEFORE_EOL;
        data->substrs[i].minlenp = minlenp;
        data->substrs[i].lookbehind = 0;
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
    DEBUG_STUDYDATA("commit", data, 0, is_inf);
}

/* An SSC is just a regnode_charclass_posix with an extra field: the inversion
 * list that describes which code points it matches */

STATIC void
S_ssc_anything(pTHX_ regnode_ssc *ssc)
{
    /* Set the SSC 'ssc' to match an empty string or any code point */

    PERL_ARGS_ASSERT_SSC_ANYTHING;

    assert(is_ANYOF_SYNTHETIC(ssc));

    /* mortalize so won't leak */
    ssc->invlist = sv_2mortal(_add_range_to_invlist(NULL, 0, UV_MAX));
    ANYOF_FLAGS(ssc) |= SSC_MATCHES_EMPTY_STRING;  /* Plus matches empty */
}

STATIC int
S_ssc_is_anything(const regnode_ssc *ssc)
{
    /* Returns TRUE if the SSC 'ssc' can match the empty string and any code
     * point; FALSE otherwise.  Thus, this is used to see if using 'ssc' buys
     * us anything: if the function returns TRUE, 'ssc' hasn't been restricted
     * in any way, so there's no point in using it */

    UV start, end;
    bool ret;

    PERL_ARGS_ASSERT_SSC_IS_ANYTHING;

    assert(is_ANYOF_SYNTHETIC(ssc));

    if (! (ANYOF_FLAGS(ssc) & SSC_MATCHES_EMPTY_STRING)) {
        return FALSE;
    }

    /* See if the list consists solely of the range 0 - Infinity */
    invlist_iterinit(ssc->invlist);
    ret = invlist_iternext(ssc->invlist, &start, &end)
          && start == 0
          && end == UV_MAX;

    invlist_iterfinish(ssc->invlist);

    if (ret) {
        return TRUE;
    }

    /* If e.g., both \w and \W are set, matches everything */
    if (ANYOF_POSIXL_SSC_TEST_ANY_SET(ssc)) {
        int i;
        for (i = 0; i < ANYOF_POSIXL_MAX; i += 2) {
            if (ANYOF_POSIXL_TEST(ssc, i) && ANYOF_POSIXL_TEST(ssc, i+1)) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

STATIC void
S_ssc_init(pTHX_ const RExC_state_t *pRExC_state, regnode_ssc *ssc)
{
    /* Initializes the SSC 'ssc'.  This includes setting it to match an empty
     * string, any code point, or any posix class under locale */

    PERL_ARGS_ASSERT_SSC_INIT;

    Zero(ssc, 1, regnode_ssc);
    set_ANYOF_SYNTHETIC(ssc);
    ARG_SET(ssc, ANYOF_ONLY_HAS_BITMAP);
    ssc_anything(ssc);

    /* If any portion of the regex is to operate under locale rules that aren't
     * fully known at compile time, initialization includes it.  The reason
     * this isn't done for all regexes is that the optimizer was written under
     * the assumption that locale was all-or-nothing.  Given the complexity and
     * lack of documentation in the optimizer, and that there are inadequate
     * test cases for locale, many parts of it may not work properly, it is
     * safest to avoid locale unless necessary. */
    if (RExC_contains_locale) {
	ANYOF_POSIXL_SETALL(ssc);
    }
    else {
	ANYOF_POSIXL_ZERO(ssc);
    }
}

STATIC int
S_ssc_is_cp_posixl_init(const RExC_state_t *pRExC_state,
                        const regnode_ssc *ssc)
{
    /* Returns TRUE if the SSC 'ssc' is in its initial state with regard only
     * to the list of code points matched, and locale posix classes; hence does
     * not check its flags) */

    UV start, end;
    bool ret;

    PERL_ARGS_ASSERT_SSC_IS_CP_POSIXL_INIT;

    assert(is_ANYOF_SYNTHETIC(ssc));

    invlist_iterinit(ssc->invlist);
    ret = invlist_iternext(ssc->invlist, &start, &end)
          && start == 0
          && end == UV_MAX;

    invlist_iterfinish(ssc->invlist);

    if (! ret) {
        return FALSE;
    }

    if (RExC_contains_locale && ! ANYOF_POSIXL_SSC_TEST_ALL_SET(ssc)) {
        return FALSE;
    }

    return TRUE;
}

#define INVLIST_INDEX 0
#define ONLY_LOCALE_MATCHES_INDEX 1
#define DEFERRED_USER_DEFINED_INDEX 2

STATIC SV*
S_get_ANYOF_cp_list_for_ssc(pTHX_ const RExC_state_t *pRExC_state,
                               const regnode_charclass* const node)
{
    /* Returns a mortal inversion list defining which code points are matched
     * by 'node', which is of type ANYOF.  Handles complementing the result if
     * appropriate.  If some code points aren't knowable at this time, the
     * returned list must, and will, contain every code point that is a
     * possibility. */

    dVAR;
    SV* invlist = NULL;
    SV* only_utf8_locale_invlist = NULL;
    unsigned int i;
    const U32 n = ARG(node);
    bool new_node_has_latin1 = FALSE;
    const U8 flags = OP(node) == ANYOFH ? 0 : ANYOF_FLAGS(node);

    PERL_ARGS_ASSERT_GET_ANYOF_CP_LIST_FOR_SSC;

    /* Look at the data structure created by S_set_ANYOF_arg() */
    if (n != ANYOF_ONLY_HAS_BITMAP) {
        SV * const rv = MUTABLE_SV(RExC_rxi->data->data[n]);
        AV * const av = MUTABLE_AV(SvRV(rv));
        SV **const ary = AvARRAY(av);
        assert(RExC_rxi->data->what[n] == 's');

        if (av_tindex_skip_len_mg(av) >= DEFERRED_USER_DEFINED_INDEX) {

            /* Here there are things that won't be known until runtime -- we
             * have to assume it could be anything */
            invlist = sv_2mortal(_new_invlist(1));
            return _add_range_to_invlist(invlist, 0, UV_MAX);
        }
        else if (ary[INVLIST_INDEX]) {

            /* Use the node's inversion list */
            invlist = sv_2mortal(invlist_clone(ary[INVLIST_INDEX], NULL));
        }

        /* Get the code points valid only under UTF-8 locales */
        if (   (flags & ANYOFL_FOLD)
            &&  av_tindex_skip_len_mg(av) >= ONLY_LOCALE_MATCHES_INDEX)
        {
            only_utf8_locale_invlist = ary[ONLY_LOCALE_MATCHES_INDEX];
        }
    }

    if (! invlist) {
        invlist = sv_2mortal(_new_invlist(0));
    }

    /* An ANYOF node contains a bitmap for the first NUM_ANYOF_CODE_POINTS
     * code points, and an inversion list for the others, but if there are code
     * points that should match only conditionally on the target string being
     * UTF-8, those are placed in the inversion list, and not the bitmap.
     * Since there are circumstances under which they could match, they are
     * included in the SSC.  But if the ANYOF node is to be inverted, we have
     * to exclude them here, so that when we invert below, the end result
     * actually does include them.  (Think about "\xe0" =~ /[^\xc0]/di;).  We
     * have to do this here before we add the unconditionally matched code
     * points */
    if (flags & ANYOF_INVERT) {
        _invlist_intersection_complement_2nd(invlist,
                                             PL_UpperLatin1,
                                             &invlist);
    }

    /* Add in the points from the bit map */
    if (OP(node) != ANYOFH) {
        for (i = 0; i < NUM_ANYOF_CODE_POINTS; i++) {
            if (ANYOF_BITMAP_TEST(node, i)) {
                unsigned int start = i++;

                for (;    i < NUM_ANYOF_CODE_POINTS
                       && ANYOF_BITMAP_TEST(node, i); ++i)
                {
                    /* empty */
                }
                invlist = _add_range_to_invlist(invlist, start, i-1);
                new_node_has_latin1 = TRUE;
            }
        }
    }

    /* If this can match all upper Latin1 code points, have to add them
     * as well.  But don't add them if inverting, as when that gets done below,
     * it would exclude all these characters, including the ones it shouldn't
     * that were added just above */
    if (! (flags & ANYOF_INVERT) && OP(node) == ANYOFD
        && (flags & ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER))
    {
        _invlist_union(invlist, PL_UpperLatin1, &invlist);
    }

    /* Similarly for these */
    if (flags & ANYOF_MATCHES_ALL_ABOVE_BITMAP) {
        _invlist_union_complement_2nd(invlist, PL_InBitmap, &invlist);
    }

    if (flags & ANYOF_INVERT) {
        _invlist_invert(invlist);
    }
    else if (flags & ANYOFL_FOLD) {
        if (new_node_has_latin1) {

            /* Under /li, any 0-255 could fold to any other 0-255, depending on
             * the locale.  We can skip this if there are no 0-255 at all. */
            _invlist_union(invlist, PL_Latin1, &invlist);

            invlist = add_cp_to_invlist(invlist, LATIN_SMALL_LETTER_DOTLESS_I);
            invlist = add_cp_to_invlist(invlist, LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE);
        }
        else {
            if (_invlist_contains_cp(invlist, LATIN_SMALL_LETTER_DOTLESS_I)) {
                invlist = add_cp_to_invlist(invlist, 'I');
            }
            if (_invlist_contains_cp(invlist,
                                        LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE))
            {
                invlist = add_cp_to_invlist(invlist, 'i');
            }
        }
    }

    /* Similarly add the UTF-8 locale possible matches.  These have to be
     * deferred until after the non-UTF-8 locale ones are taken care of just
     * above, or it leads to wrong results under ANYOF_INVERT */
    if (only_utf8_locale_invlist) {
        _invlist_union_maybe_complement_2nd(invlist,
                                            only_utf8_locale_invlist,
                                            flags & ANYOF_INVERT,
                                            &invlist);
    }

    return invlist;
}

/* These two functions currently do the exact same thing */
#define ssc_init_zero		ssc_init

#define ssc_add_cp(ssc, cp)   ssc_add_range((ssc), (cp), (cp))
#define ssc_match_all_cp(ssc) ssc_add_range(ssc, 0, UV_MAX)

/* 'AND' a given class with another one.  Can create false positives.  'ssc'
 * should not be inverted.  'and_with->flags & ANYOF_MATCHES_POSIXL' should be
 * 0 if 'and_with' is a regnode_charclass instead of a regnode_ssc. */

STATIC void
S_ssc_and(pTHX_ const RExC_state_t *pRExC_state, regnode_ssc *ssc,
                const regnode_charclass *and_with)
{
    /* Accumulate into SSC 'ssc' its 'AND' with 'and_with', which is either
     * another SSC or a regular ANYOF class.  Can create false positives. */

    SV* anded_cp_list;
    U8  and_with_flags = (OP(and_with) == ANYOFH) ? 0 : ANYOF_FLAGS(and_with);
    U8  anded_flags;

    PERL_ARGS_ASSERT_SSC_AND;

    assert(is_ANYOF_SYNTHETIC(ssc));

    /* 'and_with' is used as-is if it too is an SSC; otherwise have to extract
     * the code point inversion list and just the relevant flags */
    if (is_ANYOF_SYNTHETIC(and_with)) {
        anded_cp_list = ((regnode_ssc *)and_with)->invlist;
        anded_flags = and_with_flags;

        /* XXX This is a kludge around what appears to be deficiencies in the
         * optimizer.  If we make S_ssc_anything() add in the WARN_SUPER flag,
         * there are paths through the optimizer where it doesn't get weeded
         * out when it should.  And if we don't make some extra provision for
         * it like the code just below, it doesn't get added when it should.
         * This solution is to add it only when AND'ing, which is here, and
         * only when what is being AND'ed is the pristine, original node
         * matching anything.  Thus it is like adding it to ssc_anything() but
         * only when the result is to be AND'ed.  Probably the same solution
         * could be adopted for the same problem we have with /l matching,
         * which is solved differently in S_ssc_init(), and that would lead to
         * fewer false positives than that solution has.  But if this solution
         * creates bugs, the consequences are only that a warning isn't raised
         * that should be; while the consequences for having /l bugs is
         * incorrect matches */
        if (ssc_is_anything((regnode_ssc *)and_with)) {
            anded_flags |= ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER;
        }
    }
    else {
        anded_cp_list = get_ANYOF_cp_list_for_ssc(pRExC_state, and_with);
        if (OP(and_with) == ANYOFD) {
            anded_flags = and_with_flags & ANYOF_COMMON_FLAGS;
        }
        else {
            anded_flags = and_with_flags
            &( ANYOF_COMMON_FLAGS
              |ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER
              |ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP);
            if (ANYOFL_UTF8_LOCALE_REQD(and_with_flags)) {
                anded_flags &=
                    ANYOFL_SHARED_UTF8_LOCALE_fold_HAS_MATCHES_nonfold_REQD;
            }
        }
    }

    ANYOF_FLAGS(ssc) &= anded_flags;

    /* Below, C1 is the list of code points in 'ssc'; P1, its posix classes.
     * C2 is the list of code points in 'and-with'; P2, its posix classes.
     * 'and_with' may be inverted.  When not inverted, we have the situation of
     * computing:
     *  (C1 | P1) & (C2 | P2)
     *                     =  (C1 & (C2 | P2)) | (P1 & (C2 | P2))
     *                     =  ((C1 & C2) | (C1 & P2)) | ((P1 & C2) | (P1 & P2))
     *                    <=  ((C1 & C2) |       P2)) | ( P1       | (P1 & P2))
     *                    <=  ((C1 & C2) | P1 | P2)
     * Alternatively, the last few steps could be:
     *                     =  ((C1 & C2) | (C1 & P2)) | ((P1 & C2) | (P1 & P2))
     *                    <=  ((C1 & C2) |  C1      ) | (      C2  | (P1 & P2))
     *                    <=  (C1 | C2 | (P1 & P2))
     * We favor the second approach if either P1 or P2 is non-empty.  This is
     * because these components are a barrier to doing optimizations, as what
     * they match cannot be known until the moment of matching as they are
     * dependent on the current locale, 'AND"ing them likely will reduce or
     * eliminate them.
     * But we can do better if we know that C1,P1 are in their initial state (a
     * frequent occurrence), each matching everything:
     *  (<everything>) & (C2 | P2) =  C2 | P2
     * Similarly, if C2,P2 are in their initial state (again a frequent
     * occurrence), the result is a no-op
     *  (C1 | P1) & (<everything>) =  C1 | P1
     *
     * Inverted, we have
     *  (C1 | P1) & ~(C2 | P2)  =  (C1 | P1) & (~C2 & ~P2)
     *                          =  (C1 & (~C2 & ~P2)) | (P1 & (~C2 & ~P2))
     *                         <=  (C1 & ~C2) | (P1 & ~P2)
     * */

    if ((and_with_flags & ANYOF_INVERT)
        && ! is_ANYOF_SYNTHETIC(and_with))
    {
        unsigned int i;

        ssc_intersection(ssc,
                         anded_cp_list,
                         FALSE /* Has already been inverted */
                         );

        /* If either P1 or P2 is empty, the intersection will be also; can skip
         * the loop */
        if (! (and_with_flags & ANYOF_MATCHES_POSIXL)) {
            ANYOF_POSIXL_ZERO(ssc);
        }
        else if (ANYOF_POSIXL_SSC_TEST_ANY_SET(ssc)) {

            /* Note that the Posix class component P from 'and_with' actually
             * looks like:
             *      P = Pa | Pb | ... | Pn
             * where each component is one posix class, such as in [\w\s].
             * Thus
             *      ~P = ~(Pa | Pb | ... | Pn)
             *         = ~Pa & ~Pb & ... & ~Pn
             *        <= ~Pa | ~Pb | ... | ~Pn
             * The last is something we can easily calculate, but unfortunately
             * is likely to have many false positives.  We could do better
             * in some (but certainly not all) instances if two classes in
             * P have known relationships.  For example
             *      :lower: <= :alpha: <= :alnum: <= \w <= :graph: <= :print:
             * So
             *      :lower: & :print: = :lower:
             * And similarly for classes that must be disjoint.  For example,
             * since \s and \w can have no elements in common based on rules in
             * the POSIX standard,
             *      \w & ^\S = nothing
             * Unfortunately, some vendor locales do not meet the Posix
             * standard, in particular almost everything by Microsoft.
             * The loop below just changes e.g., \w into \W and vice versa */

            regnode_charclass_posixl temp;
            int add = 1;    /* To calculate the index of the complement */

            Zero(&temp, 1, regnode_charclass_posixl);
            ANYOF_POSIXL_ZERO(&temp);
            for (i = 0; i < ANYOF_MAX; i++) {
                assert(i % 2 != 0
                       || ! ANYOF_POSIXL_TEST((regnode_charclass_posixl*) and_with, i)
                       || ! ANYOF_POSIXL_TEST((regnode_charclass_posixl*) and_with, i + 1));

                if (ANYOF_POSIXL_TEST((regnode_charclass_posixl*) and_with, i)) {
                    ANYOF_POSIXL_SET(&temp, i + add);
                }
                add = 0 - add; /* 1 goes to -1; -1 goes to 1 */
            }
            ANYOF_POSIXL_AND(&temp, ssc);

        } /* else ssc already has no posixes */
    } /* else: Not inverted.  This routine is a no-op if 'and_with' is an SSC
         in its initial state */
    else if (! is_ANYOF_SYNTHETIC(and_with)
             || ! ssc_is_cp_posixl_init(pRExC_state, (regnode_ssc *)and_with))
    {
        /* But if 'ssc' is in its initial state, the result is just 'and_with';
         * copy it over 'ssc' */
        if (ssc_is_cp_posixl_init(pRExC_state, ssc)) {
            if (is_ANYOF_SYNTHETIC(and_with)) {
                StructCopy(and_with, ssc, regnode_ssc);
            }
            else {
                ssc->invlist = anded_cp_list;
                ANYOF_POSIXL_ZERO(ssc);
                if (and_with_flags & ANYOF_MATCHES_POSIXL) {
                    ANYOF_POSIXL_OR((regnode_charclass_posixl*) and_with, ssc);
                }
            }
        }
        else if (ANYOF_POSIXL_SSC_TEST_ANY_SET(ssc)
                 || (and_with_flags & ANYOF_MATCHES_POSIXL))
        {
            /* One or the other of P1, P2 is non-empty. */
            if (and_with_flags & ANYOF_MATCHES_POSIXL) {
                ANYOF_POSIXL_AND((regnode_charclass_posixl*) and_with, ssc);
            }
            ssc_union(ssc, anded_cp_list, FALSE);
        }
        else { /* P1 = P2 = empty */
            ssc_intersection(ssc, anded_cp_list, FALSE);
        }
    }
}

STATIC void
S_ssc_or(pTHX_ const RExC_state_t *pRExC_state, regnode_ssc *ssc,
               const regnode_charclass *or_with)
{
    /* Accumulate into SSC 'ssc' its 'OR' with 'or_with', which is either
     * another SSC or a regular ANYOF class.  Can create false positives if
     * 'or_with' is to be inverted. */

    SV* ored_cp_list;
    U8 ored_flags;
    U8  or_with_flags = (OP(or_with) == ANYOFH) ? 0 : ANYOF_FLAGS(or_with);

    PERL_ARGS_ASSERT_SSC_OR;

    assert(is_ANYOF_SYNTHETIC(ssc));

    /* 'or_with' is used as-is if it too is an SSC; otherwise have to extract
     * the code point inversion list and just the relevant flags */
    if (is_ANYOF_SYNTHETIC(or_with)) {
        ored_cp_list = ((regnode_ssc*) or_with)->invlist;
        ored_flags = or_with_flags;
    }
    else {
        ored_cp_list = get_ANYOF_cp_list_for_ssc(pRExC_state, or_with);
        ored_flags = or_with_flags & ANYOF_COMMON_FLAGS;
        if (OP(or_with) != ANYOFD) {
            ored_flags
            |= or_with_flags
             & ( ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER
                |ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP);
            if (ANYOFL_UTF8_LOCALE_REQD(or_with_flags)) {
                ored_flags |=
                    ANYOFL_SHARED_UTF8_LOCALE_fold_HAS_MATCHES_nonfold_REQD;
            }
        }
    }

    ANYOF_FLAGS(ssc) |= ored_flags;

    /* Below, C1 is the list of code points in 'ssc'; P1, its posix classes.
     * C2 is the list of code points in 'or-with'; P2, its posix classes.
     * 'or_with' may be inverted.  When not inverted, we have the simple
     * situation of computing:
     *  (C1 | P1) | (C2 | P2)  =  (C1 | C2) | (P1 | P2)
     * If P1|P2 yields a situation with both a class and its complement are
     * set, like having both \w and \W, this matches all code points, and we
     * can delete these from the P component of the ssc going forward.  XXX We
     * might be able to delete all the P components, but I (khw) am not certain
     * about this, and it is better to be safe.
     *
     * Inverted, we have
     *  (C1 | P1) | ~(C2 | P2)  =  (C1 | P1) | (~C2 & ~P2)
     *                         <=  (C1 | P1) | ~C2
     *                         <=  (C1 | ~C2) | P1
     * (which results in actually simpler code than the non-inverted case)
     * */

    if ((or_with_flags & ANYOF_INVERT)
        && ! is_ANYOF_SYNTHETIC(or_with))
    {
        /* We ignore P2, leaving P1 going forward */
    }   /* else  Not inverted */
    else if (or_with_flags & ANYOF_MATCHES_POSIXL) {
        ANYOF_POSIXL_OR((regnode_charclass_posixl*)or_with, ssc);
        if (ANYOF_POSIXL_SSC_TEST_ANY_SET(ssc)) {
            unsigned int i;
            for (i = 0; i < ANYOF_MAX; i += 2) {
                if (ANYOF_POSIXL_TEST(ssc, i) && ANYOF_POSIXL_TEST(ssc, i + 1))
                {
                    ssc_match_all_cp(ssc);
                    ANYOF_POSIXL_CLEAR(ssc, i);
                    ANYOF_POSIXL_CLEAR(ssc, i+1);
                }
            }
        }
    }

    ssc_union(ssc,
              ored_cp_list,
              FALSE /* Already has been inverted */
              );
}

PERL_STATIC_INLINE void
S_ssc_union(pTHX_ regnode_ssc *ssc, SV* const invlist, const bool invert2nd)
{
    PERL_ARGS_ASSERT_SSC_UNION;

    assert(is_ANYOF_SYNTHETIC(ssc));

    _invlist_union_maybe_complement_2nd(ssc->invlist,
                                        invlist,
                                        invert2nd,
                                        &ssc->invlist);
}

PERL_STATIC_INLINE void
S_ssc_intersection(pTHX_ regnode_ssc *ssc,
                         SV* const invlist,
                         const bool invert2nd)
{
    PERL_ARGS_ASSERT_SSC_INTERSECTION;

    assert(is_ANYOF_SYNTHETIC(ssc));

    _invlist_intersection_maybe_complement_2nd(ssc->invlist,
                                               invlist,
                                               invert2nd,
                                               &ssc->invlist);
}

PERL_STATIC_INLINE void
S_ssc_add_range(pTHX_ regnode_ssc *ssc, const UV start, const UV end)
{
    PERL_ARGS_ASSERT_SSC_ADD_RANGE;

    assert(is_ANYOF_SYNTHETIC(ssc));

    ssc->invlist = _add_range_to_invlist(ssc->invlist, start, end);
}

PERL_STATIC_INLINE void
S_ssc_cp_and(pTHX_ regnode_ssc *ssc, const UV cp)
{
    /* AND just the single code point 'cp' into the SSC 'ssc' */

    SV* cp_list = _new_invlist(2);

    PERL_ARGS_ASSERT_SSC_CP_AND;

    assert(is_ANYOF_SYNTHETIC(ssc));

    cp_list = add_cp_to_invlist(cp_list, cp);
    ssc_intersection(ssc, cp_list,
                     FALSE /* Not inverted */
                     );
    SvREFCNT_dec_NN(cp_list);
}

PERL_STATIC_INLINE void
S_ssc_clear_locale(regnode_ssc *ssc)
{
    /* Set the SSC 'ssc' to not match any locale things */
    PERL_ARGS_ASSERT_SSC_CLEAR_LOCALE;

    assert(is_ANYOF_SYNTHETIC(ssc));

    ANYOF_POSIXL_ZERO(ssc);
    ANYOF_FLAGS(ssc) &= ~ANYOF_LOCALE_FLAGS;
}

#define NON_OTHER_COUNT   NON_OTHER_COUNT_FOR_USE_ONLY_BY_REGCOMP_DOT_C

STATIC bool
S_is_ssc_worth_it(const RExC_state_t * pRExC_state, const regnode_ssc * ssc)
{
    /* The synthetic start class is used to hopefully quickly winnow down
     * places where a pattern could start a match in the target string.  If it
     * doesn't really narrow things down that much, there isn't much point to
     * having the overhead of using it.  This function uses some very crude
     * heuristics to decide if to use the ssc or not.
     *
     * It returns TRUE if 'ssc' rules out more than half what it considers to
     * be the "likely" possible matches, but of course it doesn't know what the
     * actual things being matched are going to be; these are only guesses
     *
     * For /l matches, it assumes that the only likely matches are going to be
     *      in the 0-255 range, uniformly distributed, so half of that is 127
     * For /a and /d matches, it assumes that the likely matches will be just
     *      the ASCII range, so half of that is 63
     * For /u and there isn't anything matching above the Latin1 range, it
     *      assumes that that is the only range likely to be matched, and uses
     *      half that as the cut-off: 127.  If anything matches above Latin1,
     *      it assumes that all of Unicode could match (uniformly), except for
     *      non-Unicode code points and things in the General Category "Other"
     *      (unassigned, private use, surrogates, controls and formats).  This
     *      is a much large number. */

    U32 count = 0;      /* Running total of number of code points matched by
                           'ssc' */
    UV start, end;      /* Start and end points of current range in inversion
                           XXX outdated.  UTF-8 locales are common, what about invert? list */
    const U32 max_code_points = (LOC)
                                ?  256
                                : ((  ! UNI_SEMANTICS
                                    ||  invlist_highest(ssc->invlist) < 256)
                                  ? 128
                                  : NON_OTHER_COUNT);
    const U32 max_match = max_code_points / 2;

    PERL_ARGS_ASSERT_IS_SSC_WORTH_IT;

    invlist_iterinit(ssc->invlist);
    while (invlist_iternext(ssc->invlist, &start, &end)) {
        if (start >= max_code_points) {
            break;
        }
        end = MIN(end, max_code_points - 1);
        count += end - start + 1;
        if (count >= max_match) {
            invlist_iterfinish(ssc->invlist);
            return FALSE;
        }
    }

    return TRUE;
}


STATIC void
S_ssc_finalize(pTHX_ RExC_state_t *pRExC_state, regnode_ssc *ssc)
{
    /* The inversion list in the SSC is marked mortal; now we need a more
     * permanent copy, which is stored the same way that is done in a regular
     * ANYOF node, with the first NUM_ANYOF_CODE_POINTS code points in a bit
     * map */

    SV* invlist = invlist_clone(ssc->invlist, NULL);

    PERL_ARGS_ASSERT_SSC_FINALIZE;

    assert(is_ANYOF_SYNTHETIC(ssc));

    /* The code in this file assumes that all but these flags aren't relevant
     * to the SSC, except SSC_MATCHES_EMPTY_STRING, which should be cleared
     * by the time we reach here */
    assert(! (ANYOF_FLAGS(ssc)
        & ~( ANYOF_COMMON_FLAGS
            |ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER
            |ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP)));

    populate_ANYOF_from_invlist( (regnode *) ssc, &invlist);

    set_ANYOF_arg(pRExC_state, (regnode *) ssc, invlist, NULL, NULL);

    /* Make sure is clone-safe */
    ssc->invlist = NULL;

    if (ANYOF_POSIXL_SSC_TEST_ANY_SET(ssc)) {
        ANYOF_FLAGS(ssc) |= ANYOF_MATCHES_POSIXL;
        OP(ssc) = ANYOFPOSIXL;
    }
    else if (RExC_contains_locale) {
        OP(ssc) = ANYOFL;
    }

    assert(! (ANYOF_FLAGS(ssc) & ANYOF_LOCALE_FLAGS) || RExC_contains_locale);
}

#define TRIE_LIST_ITEM(state,idx) (trie->states[state].trans.list)[ idx ]
#define TRIE_LIST_CUR(state)  ( TRIE_LIST_ITEM( state, 0 ).forid )
#define TRIE_LIST_LEN(state) ( TRIE_LIST_ITEM( state, 0 ).newstate )
#define TRIE_LIST_USED(idx)  ( trie->states[state].trans.list         \
                               ? (TRIE_LIST_CUR( idx ) - 1)           \
                               : 0 )


#ifdef DEBUGGING
/*
   dump_trie(trie,widecharmap,revcharmap)
   dump_trie_interim_list(trie,widecharmap,revcharmap,next_alloc)
   dump_trie_interim_table(trie,widecharmap,revcharmap,next_alloc)

   These routines dump out a trie in a somewhat readable format.
   The _interim_ variants are used for debugging the interim
   tables that are used to generate the final compressed
   representation which is what dump_trie expects.

   Part of the reason for their existence is to provide a form
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
    U16 word;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_DUMP_TRIE;

    Perl_re_indentf( aTHX_  "Char : %-6s%-6s%-4s ",
        depth+1, "Match","Base","Ofs" );

    for( state = 0 ; state < trie->uniquecharcount ; state++ ) {
	SV ** const tmp = av_fetch( revcharmap, state, 0);
        if ( tmp ) {
            Perl_re_printf( aTHX_  "%*s",
                colwidth,
                pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), colwidth,
	                    PL_colors[0], PL_colors[1],
	                    (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0) |
	                    PERL_PV_ESCAPE_FIRSTCHAR
                )
            );
        }
    }
    Perl_re_printf( aTHX_  "\n");
    Perl_re_indentf( aTHX_ "State|-----------------------", depth+1);

    for( state = 0 ; state < trie->uniquecharcount ; state++ )
        Perl_re_printf( aTHX_  "%.*s", colwidth, "--------");
    Perl_re_printf( aTHX_  "\n");

    for( state = 1 ; state < trie->statecount ; state++ ) {
	const U32 base = trie->states[ state ].trans.base;

        Perl_re_indentf( aTHX_  "#%4" UVXf "|", depth+1, (UV)state);

        if ( trie->states[ state ].wordnum ) {
            Perl_re_printf( aTHX_  " W%4X", trie->states[ state ].wordnum );
        } else {
            Perl_re_printf( aTHX_  "%6s", "" );
        }

        Perl_re_printf( aTHX_  " @%4" UVXf " ", (UV)base );

        if ( base ) {
            U32 ofs = 0;

            while( ( base + ofs  < trie->uniquecharcount ) ||
                   ( base + ofs - trie->uniquecharcount < trie->lasttrans
                     && trie->trans[ base + ofs - trie->uniquecharcount ].check
                                                                    != state))
                    ofs++;

            Perl_re_printf( aTHX_  "+%2" UVXf "[ ", (UV)ofs);

            for ( ofs = 0 ; ofs < trie->uniquecharcount ; ofs++ ) {
                if ( ( base + ofs >= trie->uniquecharcount )
                        && ( base + ofs - trie->uniquecharcount
                                                        < trie->lasttrans )
                        && trie->trans[ base + ofs
                                    - trie->uniquecharcount ].check == state )
                {
                   Perl_re_printf( aTHX_  "%*" UVXf, colwidth,
                    (UV)trie->trans[ base + ofs - trie->uniquecharcount ].next
                   );
                } else {
                    Perl_re_printf( aTHX_  "%*s", colwidth,"   ." );
                }
            }

            Perl_re_printf( aTHX_  "]");

        }
        Perl_re_printf( aTHX_  "\n" );
    }
    Perl_re_indentf( aTHX_  "word_info N:(prev,len)=",
                                depth);
    for (word=1; word <= trie->wordcount; word++) {
        Perl_re_printf( aTHX_  " %d:(%d,%d)",
	    (int)word, (int)(trie->wordinfo[word].prev),
	    (int)(trie->wordinfo[word].len));
    }
    Perl_re_printf( aTHX_  "\n" );
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
    Perl_re_indentf( aTHX_  "State :Word | Transition Data\n",
            depth+1 );
    Perl_re_indentf( aTHX_  "%s",
            depth+1, "------:-----+-----------------\n" );

    for( state=1 ; state < next_alloc ; state ++ ) {
        U16 charid;

        Perl_re_indentf( aTHX_  " %4" UVXf " :",
            depth+1, (UV)state  );
        if ( ! trie->states[ state ].wordnum ) {
            Perl_re_printf( aTHX_  "%5s| ","");
        } else {
            Perl_re_printf( aTHX_  "W%4x| ",
                trie->states[ state ].wordnum
            );
        }
        for( charid = 1 ; charid <= TRIE_LIST_USED( state ) ; charid++ ) {
	    SV ** const tmp = av_fetch( revcharmap,
                                        TRIE_LIST_ITEM(state, charid).forid, 0);
	    if ( tmp ) {
                Perl_re_printf( aTHX_  "%*s:%3X=%4" UVXf " | ",
                    colwidth,
                    pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp),
                              colwidth,
                              PL_colors[0], PL_colors[1],
                              (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0)
                              | PERL_PV_ESCAPE_FIRSTCHAR
                    ) ,
                    TRIE_LIST_ITEM(state, charid).forid,
                    (UV)TRIE_LIST_ITEM(state, charid).newstate
                );
                if (!(charid % 10))
                    Perl_re_printf( aTHX_  "\n%*s| ",
                        (int)((depth * 2) + 14), "");
            }
        }
        Perl_re_printf( aTHX_  "\n");
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

    Perl_re_indentf( aTHX_  "Char : ", depth+1 );

    for( charid = 0 ; charid < trie->uniquecharcount ; charid++ ) {
	SV ** const tmp = av_fetch( revcharmap, charid, 0);
        if ( tmp ) {
            Perl_re_printf( aTHX_  "%*s",
                colwidth,
                pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), colwidth,
	                    PL_colors[0], PL_colors[1],
	                    (SvUTF8(*tmp) ? PERL_PV_ESCAPE_UNI : 0) |
	                    PERL_PV_ESCAPE_FIRSTCHAR
                )
            );
        }
    }

    Perl_re_printf( aTHX_ "\n");
    Perl_re_indentf( aTHX_  "State+-", depth+1 );

    for( charid=0 ; charid < trie->uniquecharcount ; charid++ ) {
        Perl_re_printf( aTHX_  "%.*s", colwidth,"--------");
    }

    Perl_re_printf( aTHX_  "\n" );

    for( state=1 ; state < next_alloc ; state += trie->uniquecharcount ) {

        Perl_re_indentf( aTHX_  "%4" UVXf " : ",
            depth+1,
            (UV)TRIE_NODENUM( state ) );

        for( charid = 0 ; charid < trie->uniquecharcount ; charid++ ) {
            UV v=(UV)SAFE_TRIE_NODENUM( trie->trans[ state + charid ].next );
            if (v)
                Perl_re_printf( aTHX_  "%*" UVXf, colwidth, v );
            else
                Perl_re_printf( aTHX_  "%*s", colwidth, "." );
        }
        if ( ! trie->states[ TRIE_NODENUM( state ) ].wordnum ) {
            Perl_re_printf( aTHX_  " (%4" UVXf ")\n",
                                            (UV)trie->trans[ state ].check );
        } else {
            Perl_re_printf( aTHX_  " (%4" UVXf ") W%4X\n",
                                            (UV)trie->trans[ state ].check,
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
  flags      : currently the OP() type we will be building one of /EXACT(|F|FA|FU|FU_SS|L|FLU8)/
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
rest of the regex in the order in which they occurred in the alternation.

The only prior NFA like behaviour that would be changed by the TRIE support is
the silent ignoring of duplicate alternations which are of the form:

 / (DUPE|DUPE) X? (?{ ... }) Y /x

Thus EVAL blocks following a trie may be called a different number of times with
and without the optimisation. With the optimisations dupes will be silently
ignored. This inconsistent behaviour of EVAL type nodes is well established as
the following demonstrates:

 'words'=~/(word|word|word)(?{ print $1 })[xyz]/

which prints out 'word' three times, but

 'words'=~/(word|word|word)(?{ print $1 })S/

which doesnt print it out at all. This is due to other optimisations kicking in.

Example of what happens on a structural level:

The regexp /(ac|ad|ab)+/ will produce the following debug output:

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

    d = uvchr_to_utf8_flags(d, uv, 0);

is the recommended Unicode-aware way of saying

    *(d++) = uv;
*/

#define TRIE_STORE_REVCHAR(val)                                            \
    STMT_START {                                                           \
	if (UTF) {							   \
            SV *zlopp = newSV(UTF8_MAXBYTES);				   \
	    unsigned char *flrbbbbb = (unsigned char *) SvPVX(zlopp);	   \
            unsigned const char *const kapow = uvchr_to_utf8(flrbbbbb, val); \
	    SvCUR_set(zlopp, kapow - flrbbbbb);				   \
	    SvPOK_on(zlopp);						   \
	    SvUTF8_on(zlopp);						   \
	    av_push(revcharmap, zlopp);					   \
	} else {							   \
            char ooooff = (char)val;                                           \
	    av_push(revcharmap, newSVpvn(&ooooff, 1));			   \
	}								   \
        } STMT_END

/* This gets the next character from the input, folding it if not already
 * folded. */
#define TRIE_READ_CHAR STMT_START {                                           \
    wordlen++;                                                                \
    if ( UTF ) {                                                              \
        /* if it is UTF then it is either already folded, or does not need    \
         * folding */                                                         \
        uvc = valid_utf8_to_uvchr( (const U8*) uc, &len);                     \
    }                                                                         \
    else if (folder == PL_fold_latin1) {                                      \
        /* This folder implies Unicode rules, which in the range expressible  \
         *  by not UTF is the lower case, with the two exceptions, one of     \
         *  which should have been taken care of before calling this */       \
        assert(*uc != LATIN_SMALL_LETTER_SHARP_S);                            \
        uvc = toLOWER_L1(*uc);                                                \
        if (UNLIKELY(uvc == MICRO_SIGN)) uvc = GREEK_SMALL_LETTER_MU;         \
        len = 1;                                                              \
    } else {                                                                  \
        /* raw data, will be folded later if needed */                        \
        uvc = (U32)*uc;                                                       \
        len = 1;                                                              \
    }                                                                         \
} STMT_END



#define TRIE_LIST_PUSH(state,fid,ns) STMT_START {               \
    if ( TRIE_LIST_CUR( state ) >=TRIE_LIST_LEN( state ) ) {    \
	U32 ging = TRIE_LIST_LEN( state ) * 2;                  \
	Renew( trie->states[ state ].trans.list, ging, reg_trie_trans_le ); \
        TRIE_LIST_LEN( state ) = ging;                          \
    }                                                           \
    TRIE_LIST_ITEM( state, TRIE_LIST_CUR( state ) ).forid = fid;     \
    TRIE_LIST_ITEM( state, TRIE_LIST_CUR( state ) ).newstate = ns;   \
    TRIE_LIST_CUR( state )++;                                   \
} STMT_END

#define TRIE_LIST_NEW(state) STMT_START {                       \
    Newx( trie->states[ state ].trans.list,                     \
	4, reg_trie_trans_le );                                 \
     TRIE_LIST_CUR( state ) = 1;                                \
     TRIE_LIST_LEN( state ) = 4;                                \
} STMT_END

#define TRIE_HANDLE_WORD(state) STMT_START {                    \
    U16 dupe= trie->states[ state ].wordnum;                    \
    regnode * const noper_next = regnext( noper );              \
                                                                \
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
    trie->wordinfo[curword].prev   = 0;                         \
    trie->wordinfo[curword].len    = wordlen;                   \
    trie->wordinfo[curword].accept = state;                     \
                                                                \
    if ( noper_next < tail ) {                                  \
        if (!trie->jump)                                        \
            trie->jump = (U16 *) PerlMemShared_calloc( word_count + 1, \
                                                 sizeof(U16) ); \
        trie->jump[curword] = (U16)(noper_next - convert);      \
        if (!jumper)                                            \
            jumper = noper_next;                                \
        if (!nextbranch)                                        \
            nextbranch= regnext(cur);                           \
    }                                                           \
                                                                \
    if ( dupe ) {                                               \
        /* It's a dupe. Pre-insert into the wordinfo[].prev   */\
        /* chain, so that when the bits of chain are later    */\
        /* linked together, the dups appear in the chain      */\
	trie->wordinfo[curword].prev = trie->wordinfo[dupe].prev; \
	trie->wordinfo[dupe].prev = curword;                    \
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

#define TRIE_BITMAP_SET_FOLDED(trie, uvc, folder)           \
STMT_START {                                                \
    TRIE_BITMAP_SET(trie, uvc);                             \
    /* store the folded codepoint */                        \
    if ( folder )                                           \
        TRIE_BITMAP_SET(trie, folder[(U8) uvc ]);           \
                                                            \
    if ( !UTF ) {                                           \
        /* store first byte of utf8 representation of */    \
        /* variant codepoints */                            \
        if (! UVCHR_IS_INVARIANT(uvc)) {                    \
            TRIE_BITMAP_SET(trie, UTF8_TWO_BYTE_HI(uvc));   \
        }                                                   \
    }                                                       \
} STMT_END
#define MADE_TRIE       1
#define MADE_JUMP_TRIE  2
#define MADE_EXACT_TRIE 4

STATIC I32
S_make_trie(pTHX_ RExC_state_t *pRExC_state, regnode *startbranch,
                  regnode *first, regnode *last, regnode *tail,
                  U32 word_count, U32 flags, U32 depth)
{
    /* first pass, loop through and scan words */
    reg_trie_data *trie;
    HV *widecharmap = NULL;
    AV *revcharmap = newAV();
    regnode *cur;
    STRLEN len = 0;
    UV uvc = 0;
    U16 curword = 0;
    U32 next_alloc = 0;
    regnode *jumper = NULL;
    regnode *nextbranch = NULL;
    regnode *convert = NULL;
    U32 *prev_states; /* temp array mapping each state to previous one */
    /* we just use folder as a flag in utf8 */
    const U8 * folder = NULL;

    /* in the below add_data call we are storing either 'tu' or 'tuaa'
     * which stands for one trie structure, one hash, optionally followed
     * by two arrays */
#ifdef DEBUGGING
    const U32 data_slot = add_data( pRExC_state, STR_WITH_LEN("tuaa"));
    AV *trie_words = NULL;
    /* along with revcharmap, this only used during construction but both are
     * useful during debugging so we store them in the struct when debugging.
     */
#else
    const U32 data_slot = add_data( pRExC_state, STR_WITH_LEN("tu"));
    STRLEN trie_charcount=0;
#endif
    SV *re_trie_maxbuff;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_MAKE_TRIE;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    switch (flags) {
        case EXACT: case EXACT_ONLY8: case EXACTL: break;
	case EXACTFAA:
        case EXACTFUP:
	case EXACTFU:
	case EXACTFLU8: folder = PL_fold_latin1; break;
	case EXACTF:  folder = PL_fold; break;
        default: Perl_croak( aTHX_ "panic! In trie construction, unknown node type %u %s", (unsigned) flags, PL_reg_name[flags] );
    }

    trie = (reg_trie_data *) PerlMemShared_calloc( 1, sizeof(reg_trie_data) );
    trie->refcount = 1;
    trie->startstate = 1;
    trie->wordcount = word_count;
    RExC_rxi->data->data[ data_slot ] = (void*)trie;
    trie->charmap = (U16 *) PerlMemShared_calloc( 256, sizeof(U16) );
    if (flags == EXACT || flags == EXACT_ONLY8 || flags == EXACTL)
	trie->bitmap = (char *) PerlMemShared_calloc( ANYOF_BITMAP_SIZE, 1 );
    trie->wordinfo = (reg_trie_wordinfo *) PerlMemShared_calloc(
                       trie->wordcount+1, sizeof(reg_trie_wordinfo));

    DEBUG_r({
        trie_words = newAV();
    });

    re_trie_maxbuff = get_sv(RE_TRIE_MAXBUF_NAME, GV_ADD);
    assert(re_trie_maxbuff);
    if (!SvIOK(re_trie_maxbuff)) {
        sv_setiv(re_trie_maxbuff, RE_TRIE_MAXBUF_INIT);
    }
    DEBUG_TRIE_COMPILE_r({
        Perl_re_indentf( aTHX_
          "make_trie start==%d, first==%d, last==%d, tail==%d depth=%d\n",
          depth+1,
          REG_NODE_NUM(startbranch), REG_NODE_NUM(first),
          REG_NODE_NUM(last), REG_NODE_NUM(tail), (int)depth);
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
       (trie->charmap) and we use a an HV* to store Unicode characters. We use
       the native representation of the character value as the key and IV's for
       the coded index.

       *TODO* If we keep track of how many times each character is used we can
       remap the columns so that the table compression later on is more
       efficient in terms of memory by ensuring the most common value is in the
       middle and the least common are on the outside.  IMO this would be better
       than a most to least common mapping as theres a decent chance the most
       common letter will share a node with the least common, meaning the node
       will not be compressible. With a middle is most common approach the worst
       case is when we have the least common nodes twice.

     */

    for ( cur = first ; cur < last ; cur = regnext( cur ) ) {
        regnode *noper = NEXTOPER( cur );
        const U8 *uc;
        const U8 *e;
        int foldlen = 0;
        U32 wordlen      = 0;         /* required init */
        STRLEN minchars = 0;
        STRLEN maxchars = 0;
        bool set_bit = trie->bitmap ? 1 : 0; /*store the first char in the
                                               bitmap?*/

        if (OP(noper) == NOTHING) {
            /* skip past a NOTHING at the start of an alternation
             * eg, /(?:)a|(?:b)/ should be the same as /a|b/
             *
             * If the next node is not something we are supposed to process
             * we will just ignore it due to the condition guarding the
             * next block.
             */

            regnode *noper_next= regnext(noper);
            if (noper_next < tail)
                noper= noper_next;
        }

        if (    noper < tail
            && (    OP(noper) == flags
                || (flags == EXACT && OP(noper) == EXACT_ONLY8)
                || (flags == EXACTFU && (   OP(noper) == EXACTFU_ONLY8
                                         || OP(noper) == EXACTFUP))))
        {
            uc= (U8*)STRING(noper);
            e= uc + STR_LEN(noper);
        } else {
            trie->minlen= 0;
            continue;
        }


        if ( set_bit ) { /* bitmap only alloced when !(UTF&&Folding) */
            TRIE_BITMAP_SET(trie,*uc); /* store the raw first byte
                                          regardless of encoding */
            if (OP( noper ) == EXACTFUP) {
                /* false positives are ok, so just set this */
                TRIE_BITMAP_SET(trie, LATIN_SMALL_LETTER_SHARP_S);
            }
        }

        for ( ; uc < e ; uc += len ) {  /* Look at each char in the current
                                           branch */
            TRIE_CHARCOUNT(trie)++;
            TRIE_READ_CHAR;

            /* TRIE_READ_CHAR returns the current character, or its fold if /i
             * is in effect.  Under /i, this character can match itself, or
             * anything that folds to it.  If not under /i, it can match just
             * itself.  Most folds are 1-1, for example k, K, and KELVIN SIGN
             * all fold to k, and all are single characters.   But some folds
             * expand to more than one character, so for example LATIN SMALL
             * LIGATURE FFI folds to the three character sequence 'ffi'.  If
             * the string beginning at 'uc' is 'ffi', it could be matched by
             * three characters, or just by the one ligature character. (It
             * could also be matched by two characters: LATIN SMALL LIGATURE FF
             * followed by 'i', or by 'f' followed by LATIN SMALL LIGATURE FI).
             * (Of course 'I' and/or 'F' instead of 'i' and 'f' can also
             * match.)  The trie needs to know the minimum and maximum number
             * of characters that could match so that it can use size alone to
             * quickly reject many match attempts.  The max is simple: it is
             * the number of folded characters in this branch (since a fold is
             * never shorter than what folds to it. */

            maxchars++;

            /* And the min is equal to the max if not under /i (indicated by
             * 'folder' being NULL), or there are no multi-character folds.  If
             * there is a multi-character fold, the min is incremented just
             * once, for the character that folds to the sequence.  Each
             * character in the sequence needs to be added to the list below of
             * characters in the trie, but we count only the first towards the
             * min number of characters needed.  This is done through the
             * variable 'foldlen', which is returned by the macros that look
             * for these sequences as the number of bytes the sequence
             * occupies.  Each time through the loop, we decrement 'foldlen' by
             * how many bytes the current char occupies.  Only when it reaches
             * 0 do we increment 'minchars' or look for another multi-character
             * sequence. */
            if (folder == NULL) {
                minchars++;
            }
            else if (foldlen > 0) {
                foldlen -= (UTF) ? UTF8SKIP(uc) : 1;
            }
            else {
                minchars++;

                /* See if *uc is the beginning of a multi-character fold.  If
                 * so, we decrement the length remaining to look at, to account
                 * for the current character this iteration.  (We can use 'uc'
                 * instead of the fold returned by TRIE_READ_CHAR because for
                 * non-UTF, the latin1_safe macro is smart enough to account
                 * for all the unfolded characters, and because for UTF, the
                 * string will already have been folded earlier in the
                 * compilation process */
                if (UTF) {
                    if ((foldlen = is_MULTI_CHAR_FOLD_utf8_safe(uc, e))) {
                        foldlen -= UTF8SKIP(uc);
                    }
                }
                else if ((foldlen = is_MULTI_CHAR_FOLD_latin1_safe(uc, e))) {
                    foldlen--;
                }
            }

            /* The current character (and any potential folds) should be added
             * to the possible matching characters for this position in this
             * branch */
            if ( uvc < 256 ) {
                if ( folder ) {
                    U8 folded= folder[ (U8) uvc ];
                    if ( !trie->charmap[ folded ] ) {
                        trie->charmap[ folded ]=( ++trie->uniquecharcount );
                        TRIE_STORE_REVCHAR( folded );
                    }
                }
                if ( !trie->charmap[ uvc ] ) {
                    trie->charmap[ uvc ]=( ++trie->uniquecharcount );
                    TRIE_STORE_REVCHAR( uvc );
                }
                if ( set_bit ) {
		    /* store the codepoint in the bitmap, and its folded
		     * equivalent. */
                    TRIE_BITMAP_SET_FOLDED(trie, uvc, folder);
                    set_bit = 0; /* We've done our bit :-) */
                }
            } else {

                /* XXX We could come up with the list of code points that fold
                 * to this using PL_utf8_foldclosures, except not for
                 * multi-char folds, as there may be multiple combinations
                 * there that could work, which needs to wait until runtime to
                 * resolve (The comment about LIGATURE FFI above is such an
                 * example */

                SV** svpp;
                if ( !widecharmap )
                    widecharmap = newHV();

                svpp = hv_fetch( widecharmap, (char*)&uvc, sizeof( UV ), 1 );

                if ( !svpp )
                    Perl_croak( aTHX_ "error creating/fetching widecharmap entry for 0x%" UVXf, uvc );

                if ( !SvTRUE( *svpp ) ) {
                    sv_setiv( *svpp, ++trie->uniquecharcount );
                    TRIE_STORE_REVCHAR(uvc);
                }
            }
        } /* end loop through characters in this branch of the trie */

        /* We take the min and max for this branch and combine to find the min
         * and max for all branches processed so far */
        if( cur == first ) {
            trie->minlen = minchars;
            trie->maxlen = maxchars;
        } else if (minchars < trie->minlen) {
            trie->minlen = minchars;
        } else if (maxchars > trie->maxlen) {
            trie->maxlen = maxchars;
        }
    } /* end first pass */
    DEBUG_TRIE_COMPILE_r(
        Perl_re_indentf( aTHX_
                "TRIE(%s): W:%d C:%d Uq:%d Min:%d Max:%d\n",
                depth+1,
                ( widecharmap ? "UTF8" : "NATIVE" ), (int)word_count,
		(int)TRIE_CHARCOUNT(trie), trie->uniquecharcount,
		(int)trie->minlen, (int)trie->maxlen )
    );

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


    Newx(prev_states, TRIE_CHARCOUNT(trie) + 2, U32);
    prev_states[1] = 0;

    if ( (IV)( ( TRIE_CHARCOUNT(trie) + 1 ) * trie->uniquecharcount + 1)
                                                    > SvIV(re_trie_maxbuff) )
    {
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

        DEBUG_TRIE_COMPILE_MORE_r( Perl_re_indentf( aTHX_  "Compiling trie using list compiler\n",
            depth+1));

	trie->states = (reg_trie_state *)
	    PerlMemShared_calloc( TRIE_CHARCOUNT(trie) + 2,
				  sizeof(reg_trie_state) );
        TRIE_LIST_NEW(1);
        next_alloc = 2;

        for ( cur = first ; cur < last ; cur = regnext( cur ) ) {

            regnode *noper   = NEXTOPER( cur );
	    U32 state        = 1;         /* required init */
	    U16 charid       = 0;         /* sanity init */
            U32 wordlen      = 0;         /* required init */

            if (OP(noper) == NOTHING) {
                regnode *noper_next= regnext(noper);
                if (noper_next < tail)
                    noper= noper_next;
                /* we will undo this assignment if noper does not
                 * point at a trieable type in the else clause of
                 * the following statement. */
            }

            if (    noper < tail
                && (    OP(noper) == flags
                    || (flags == EXACT && OP(noper) == EXACT_ONLY8)
                    || (flags == EXACTFU && (   OP(noper) == EXACTFU_ONLY8
                                             || OP(noper) == EXACTFUP))))
            {
                const U8 *uc= (U8*)STRING(noper);
                const U8 *e= uc + STR_LEN(noper);

                for ( ; uc < e ; uc += len ) {

                    TRIE_READ_CHAR;

                    if ( uvc < 256 ) {
                        charid = trie->charmap[ uvc ];
		    } else {
                        SV** const svpp = hv_fetch( widecharmap,
                                                    (char*)&uvc,
                                                    sizeof( UV ),
                                                    0);
                        if ( !svpp ) {
                            charid = 0;
                        } else {
                            charid=(U16)SvIV( *svpp );
                        }
		    }
                    /* charid is now 0 if we dont know the char read, or
                     * nonzero if we do */
                    if ( charid ) {

                        U16 check;
                        U32 newstate = 0;

                        charid--;
                        if ( !trie->states[ state ].trans.list ) {
                            TRIE_LIST_NEW( state );
			}
                        for ( check = 1;
                              check <= TRIE_LIST_USED( state );
                              check++ )
                        {
                            if ( TRIE_LIST_ITEM( state, check ).forid
                                                                    == charid )
                            {
                                newstate = TRIE_LIST_ITEM( state, check ).newstate;
                                break;
                            }
                        }
                        if ( ! newstate ) {
                            newstate = next_alloc++;
			    prev_states[newstate] = state;
                            TRIE_LIST_PUSH( state, charid, newstate );
                            transcount++;
                        }
                        state = newstate;
                    } else {
                        Perl_croak( aTHX_ "panic! In trie construction, no char mapping for %" IVdf, uvc );
		    }
		}
            } else {
                /* If we end up here it is because we skipped past a NOTHING, but did not end up
                 * on a trieable type. So we need to reset noper back to point at the first regop
                 * in the branch before we call TRIE_HANDLE_WORD()
                */
                noper= NEXTOPER(cur);
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
                    Perl_re_printf( aTHX_  "tp: %d zp: %d ",tp,zp)
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
                        Zero( trie->trans + (transcount / 2),
                              transcount / 2,
                              reg_trie_trans );
                    }
                    base = trie->uniquecharcount + tp - minid;
                    if ( maxid == minid ) {
                        U32 set = 0;
                        for ( ; zp < tp ; zp++ ) {
                            if ( ! trie->trans[ zp ].next ) {
                                base = trie->uniquecharcount + zp - minid;
                                trie->trans[ zp ].next = TRIE_LIST_ITEM( state,
                                                                   1).newstate;
                                trie->trans[ zp ].check = state;
                                set = 1;
                                break;
                            }
                        }
                        if ( !set ) {
                            trie->trans[ tp ].next = TRIE_LIST_ITEM( state,
                                                                   1).newstate;
                            trie->trans[ tp ].check = state;
                            tp++;
                            zp = tp;
                        }
                    } else {
                        for ( idx=1; idx <= TRIE_LIST_USED( state ) ; idx++ ) {
                            const U32 tid = base
                                           - trie->uniquecharcount
                                           + TRIE_LIST_ITEM( state, idx ).forid;
                            trie->trans[ tid ].next = TRIE_LIST_ITEM( state,
                                                                idx ).newstate;
                            trie->trans[ tid ].check = state;
                        }
                        tp += ( maxid - minid + 1 );
                    }
                    Safefree(trie->states[ state ].trans.list);
                }
                /*
                DEBUG_TRIE_COMPILE_MORE_r(
                    Perl_re_printf( aTHX_  " base: %d\n",base);
                );
                */
                trie->states[ state ].trans.base=base;
            }
            trie->lasttrans = tp + 1;
        }
    } else {
        /*
           Second Pass -- Flat Table Representation.

           we dont use the 0 slot of either trans[] or states[] so we add 1 to
           each.  We know that we will need Charcount+1 trans at most to store
           the data (one row per char at worst case) So we preallocate both
           structures assuming worst case.

           We then construct the trie using only the .next slots of the entry
           structs.

           We use the .check field of the first entry of the node temporarily
           to make compression both faster and easier by keeping track of how
           many non zero fields are in the node.

           Since trans are numbered from 1 any 0 pointer in the table is a FAIL
           transition.

           There are two terms at use here: state as a TRIE_NODEIDX() which is
           a number representing the first entry of the node, and state as a
           TRIE_NODENUM() which is the trans number. state 1 is TRIE_NODEIDX(1)
           and TRIE_NODENUM(1), state 2 is TRIE_NODEIDX(2) and TRIE_NODENUM(3)
           if there are 2 entrys per node. eg:

             A B       A B
          1. 2 4    1. 3 7
          2. 0 3    3. 0 5
          3. 0 0    5. 0 0
          4. 0 0    7. 0 0

           The table is internally in the right hand, idx form. However as we
           also have to deal with the states array which is indexed by nodenum
           we have to use TRIE_NODENUM() to convert.

        */
        DEBUG_TRIE_COMPILE_MORE_r( Perl_re_indentf( aTHX_  "Compiling trie using table compiler\n",
            depth+1));

	trie->trans = (reg_trie_trans *)
	    PerlMemShared_calloc( ( TRIE_CHARCOUNT(trie) + 1 )
				  * trie->uniquecharcount + 1,
				  sizeof(reg_trie_trans) );
        trie->states = (reg_trie_state *)
	    PerlMemShared_calloc( TRIE_CHARCOUNT(trie) + 2,
				  sizeof(reg_trie_state) );
        next_alloc = trie->uniquecharcount + 1;


        for ( cur = first ; cur < last ; cur = regnext( cur ) ) {

            regnode *noper   = NEXTOPER( cur );

            U32 state        = 1;         /* required init */

            U16 charid       = 0;         /* sanity init */
            U32 accept_state = 0;         /* sanity init */

            U32 wordlen      = 0;         /* required init */

            if (OP(noper) == NOTHING) {
                regnode *noper_next= regnext(noper);
                if (noper_next < tail)
                    noper= noper_next;
                /* we will undo this assignment if noper does not
                 * point at a trieable type in the else clause of
                 * the following statement. */
            }

            if (    noper < tail
                && (    OP(noper) == flags
                    || (flags == EXACT && OP(noper) == EXACT_ONLY8)
                    || (flags == EXACTFU && (   OP(noper) == EXACTFU_ONLY8
                                             || OP(noper) == EXACTFUP))))
            {
                const U8 *uc= (U8*)STRING(noper);
                const U8 *e= uc + STR_LEN(noper);

                for ( ; uc < e ; uc += len ) {

                    TRIE_READ_CHAR;

                    if ( uvc < 256 ) {
                        charid = trie->charmap[ uvc ];
                    } else {
                        SV* const * const svpp = hv_fetch( widecharmap,
                                                           (char*)&uvc,
                                                           sizeof( UV ),
                                                           0);
                        charid = svpp ? (U16)SvIV(*svpp) : 0;
                    }
                    if ( charid ) {
                        charid--;
                        if ( !trie->trans[ state + charid ].next ) {
                            trie->trans[ state + charid ].next = next_alloc;
                            trie->trans[ state ].check++;
			    prev_states[TRIE_NODENUM(next_alloc)]
				    = TRIE_NODENUM(state);
                            next_alloc += trie->uniquecharcount;
                        }
                        state = trie->trans[ state + charid ].next;
                    } else {
                        Perl_croak( aTHX_ "panic! In trie construction, no char mapping for %" IVdf, uvc );
                    }
                    /* charid is now 0 if we dont know the char read, or
                     * nonzero if we do */
                }
            } else {
                /* If we end up here it is because we skipped past a NOTHING, but did not end up
                 * on a trieable type. So we need to reset noper back to point at the first regop
                 * in the branch before we call TRIE_HANDLE_WORD().
                */
                noper= NEXTOPER(cur);
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

           - If .base is 0 there are no valid transitions from that node.

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
           table: We first do not compress the root node 1,and mark all its
           .check pointers as 1 and set its .base pointer as 1 as well. This
           allows us to do a DFA construction from the compressed table later,
           and ensures that any .base pointers we calculate later are greater
           than 0.

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

            for ( charid = 0;
                  used && charid < trie->uniquecharcount;
                  charid++ )
            {
                if ( flag || trie->trans[ stateidx + charid ].next ) {
                    if ( trie->trans[ stateidx + charid ].next ) {
                        if (o_used == 1) {
                            for ( ; zp < pos ; zp++ ) {
                                if ( ! trie->trans[ zp ].next ) {
                                    break;
                                }
                            }
                            trie->states[ state ].trans.base
                                                    = zp
                                                      + trie->uniquecharcount
                                                      - charid ;
                            trie->trans[ zp ].next
                                = SAFE_TRIE_NODENUM( trie->trans[ stateidx
                                                             + charid ].next );
                            trie->trans[ zp ].check = state;
                            if ( ++zp > pos ) pos = zp;
                            break;
                        }
                        used--;
                    }
                    if ( !flag ) {
                        flag = 1;
                        trie->states[ state ].trans.base
                                       = pos + trie->uniquecharcount - charid ;
                    }
                    trie->trans[ pos ].next
                        = SAFE_TRIE_NODENUM(
                                       trie->trans[ stateidx + charid ].next );
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
            Perl_re_indentf( aTHX_  "Alloc: %d Orig: %" IVdf " elements, Final:%" IVdf ". Savings of %%%5.2f\n",
                depth+1,
                (int)( ( TRIE_CHARCOUNT(trie) + 1 ) * trie->uniquecharcount
                       + 1 ),
                (IV)next_alloc,
                (IV)pos,
                ( ( next_alloc - pos ) * 100 ) / (double)next_alloc );
            );

        } /* end table compress */
    }
    DEBUG_TRIE_COMPILE_MORE_r(
            Perl_re_indentf( aTHX_  "Statecount:%" UVxf " Lasttrans:%" UVxf "\n",
                depth+1,
                (UV)trie->statecount,
                (UV)trie->lasttrans)
    );
    /* resize the trans array to remove unused space */
    trie->trans = (reg_trie_trans *)
	PerlMemShared_realloc( trie->trans, trie->lasttrans
			       * sizeof(reg_trie_trans) );

    {   /* Modify the program and insert the new TRIE node */
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
           Assuming its a sub part we convert the EXACT otherwise we convert
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
            Perl_re_indentf( aTHX_  "MJD offset:%" UVuf " MJD length:%" UVuf "\n",
                depth+1,
                (UV)mjd_offset, (UV)mjd_nodelen)
        );
#endif
        /* But first we check to see if there is a common prefix we can
           split out as an EXACT and put in front of the TRIE node.  */
        trie->startstate= 1;
        if ( trie->bitmap && !widecharmap && !trie->jump  ) {
            /* we want to find the first state that has more than
             * one transition, if that state is not the first state
             * then we have a common prefix which we can remove.
             */
            U32 state;
            for ( state = 1 ; state < trie->statecount-1 ; state++ ) {
                U32 ofs = 0;
                I32 first_ofs = -1; /* keeps track of the ofs of the first
                                       transition, -1 means none */
                U32 count = 0;
                const U32 base = trie->states[ state ].trans.base;

                /* does this state terminate an alternation? */
                if ( trie->states[state].wordnum )
                        count = 1;

                for ( ofs = 0 ; ofs < trie->uniquecharcount ; ofs++ ) {
                    if ( ( base + ofs >= trie->uniquecharcount ) &&
                         ( base + ofs - trie->uniquecharcount < trie->lasttrans ) &&
                         trie->trans[ base + ofs - trie->uniquecharcount ].check == state )
                    {
                        if ( ++count > 1 ) {
                            /* we have more than one transition */
                            SV **tmp;
                            U8 *ch;
                            /* if this is the first state there is no common prefix
                             * to extract, so we can exit */
                            if ( state == 1 ) break;
                            tmp = av_fetch( revcharmap, ofs, 0);
                            ch = (U8*)SvPV_nolen_const( *tmp );

                            /* if we are on count 2 then we need to initialize the
                             * bitmap, and store the previous char if there was one
                             * in it*/
                            if ( count == 2 ) {
                                /* clear the bitmap */
                                Zero(trie->bitmap, ANYOF_BITMAP_SIZE, char);
                                DEBUG_OPTIMISE_r(
                                    Perl_re_indentf( aTHX_  "New Start State=%" UVuf " Class: [",
                                        depth+1,
                                        (UV)state));
                                if (first_ofs >= 0) {
                                    SV ** const tmp = av_fetch( revcharmap, first_ofs, 0);
				    const U8 * const ch = (U8*)SvPV_nolen_const( *tmp );

                                    TRIE_BITMAP_SET_FOLDED(trie,*ch, folder);
                                    DEBUG_OPTIMISE_r(
                                        Perl_re_printf( aTHX_  "%s", (char*)ch)
                                    );
				}
			    }
                            /* store the current firstchar in the bitmap */
                            TRIE_BITMAP_SET_FOLDED(trie,*ch, folder);
                            DEBUG_OPTIMISE_r(Perl_re_printf( aTHX_ "%s", ch));
			}
                        first_ofs = ofs;
		    }
                }
                if ( count == 1 ) {
                    /* This state has only one transition, its transition is part
                     * of a common prefix - we need to concatenate the char it
                     * represents to what we have so far. */
                    SV **tmp = av_fetch( revcharmap, first_ofs, 0);
                    STRLEN len;
                    char *ch = SvPV( *tmp, len );
                    DEBUG_OPTIMISE_r({
                        SV *sv=sv_newmortal();
                        Perl_re_indentf( aTHX_  "Prefix State: %" UVuf " Ofs:%" UVuf " Char='%s'\n",
                            depth+1,
                            (UV)state, (UV)first_ofs,
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
                        DEBUG_OPTIMISE_r(Perl_re_printf( aTHX_ "]\n"));
#endif
		    break;
		}
	    }
	    trie->prefixlen = (state-1);
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
#ifdef RE_TRACK_PATTERN_OFFSETS
                   mjd_nodelen++;
#endif
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

            /* If the start state is not accepting (meaning there is no empty string/NOTHING)
	     *   and there is a bitmap
	     *   and the first "jump target" node we found leaves enough room
	     * then convert the TRIE node into a TRIEC node, with the bitmap
	     * embedded inline in the opcode - this is hypothetically faster.
	     */
            if ( !trie->states[trie->startstate].wordnum
		 && trie->bitmap
		 && ( (char *)jumper - (char *)convert) >= (int)sizeof(struct regnode_charclass) )
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
                Set_Node_Offset_Length(opt, 0, 0);
            }
            /*
                Try to clean up some of the debris left after the
                optimisation.
             */
            while( optimize < jumper ) {
                Track_Code( mjd_nodelen += Node_Length((optimize)); );
                OP( optimize ) = OPTIMIZED;
                Set_Node_Offset_Length(optimize, 0, 0);
                optimize++;
            }
            Set_Node_Offset_Length(convert, mjd_offset, mjd_nodelen);
        });
    } /* end node insert */

    /*  Finish populating the prev field of the wordinfo array.  Walk back
     *  from each accept state until we find another accept state, and if
     *  so, point the first word's .prev field at the second word. If the
     *  second already has a .prev field set, stop now. This will be the
     *  case either if we've already processed that word's accept state,
     *  or that state had multiple words, and the overspill words were
     *  already linked up earlier.
     */
    {
	U16 word;
	U32 state;
	U16 prev;

	for (word=1; word <= trie->wordcount; word++) {
	    prev = 0;
	    if (trie->wordinfo[word].prev)
		continue;
	    state = trie->wordinfo[word].accept;
	    while (state) {
		state = prev_states[state];
		if (!state)
		    break;
		prev = trie->states[state].wordnum;
		if (prev)
		    break;
	    }
	    trie->wordinfo[word].prev = prev;
	}
	Safefree(prev_states);
    }


    /* and now dump out the compressed format */
    DEBUG_TRIE_COMPILE_r(dump_trie(trie, widecharmap, revcharmap, depth+1));

    RExC_rxi->data->data[ data_slot + 1 ] = (void*)widecharmap;
#ifdef DEBUGGING
    RExC_rxi->data->data[ data_slot + TRIE_WORDS_OFFSET ] = (void*)trie_words;
    RExC_rxi->data->data[ data_slot + 3 ] = (void*)revcharmap;
#else
    SvREFCNT_dec_NN(revcharmap);
#endif
    return trie->jump
           ? MADE_JUMP_TRIE
           : trie->startstate>1
             ? MADE_EXACT_TRIE
             : MADE_TRIE;
}

STATIC regnode *
S_construct_ahocorasick_from_trie(pTHX_ RExC_state_t *pRExC_state, regnode *source, U32 depth)
{
/* The Trie is constructed and compressed now so we can build a fail array if
 * it's needed

   This is basically the Aho-Corasick algorithm. Its from exercise 3.31 and
   3.32 in the
   "Red Dragon" -- Compilers, principles, techniques, and tools. Aho, Sethi,
   Ullman 1985/88
   ISBN 0-201-10088-6

   We find the fail state for each state in the trie, this state is the longest
   proper suffix of the current state's 'word' that is also a proper prefix of
   another word in our trie. State 1 represents the word '' and is thus the
   default fail state. This allows the DFA not to have to restart after its
   tried and failed a word at a given point, it simply continues as though it
   had been matching the other word in the first place.
   Consider
      'abcdgu'=~/abcdefg|cdgu/
   When we get to 'd' we are still matching the first word, we would encounter
   'g' which would fail, which would bring us to the state representing 'd' in
   the second word where we would try 'g' and succeed, proceeding to match
   'cdgu'.
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
    const U32 data_slot = add_data( pRExC_state, STR_WITH_LEN("T"));
    regnode *stclass;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_CONSTRUCT_AHOCORASICK_FROM_TRIE;
    PERL_UNUSED_CONTEXT;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    if ( OP(source) == TRIE ) {
        struct regnode_1 *op = (struct regnode_1 *)
            PerlMemShared_calloc(1, sizeof(struct regnode_1));
        StructCopy(source, op, struct regnode_1);
        stclass = (regnode *)op;
    } else {
        struct regnode_charclass *op = (struct regnode_charclass *)
            PerlMemShared_calloc(1, sizeof(struct regnode_charclass));
        StructCopy(source, op, struct regnode_charclass);
        stclass = (regnode *)op;
    }
    OP(stclass)+=2; /* convert the TRIE type to its AHO-CORASICK equivalent */

    ARG_SET( stclass, data_slot );
    aho = (reg_ac_data *) PerlMemShared_calloc( 1, sizeof(reg_ac_data) );
    RExC_rxi->data->data[ data_slot ] = (void*)aho;
    aho->trie=trie_offset;
    aho->states=(reg_trie_state *)PerlMemShared_malloc( numstates * sizeof(reg_trie_state) );
    Copy( trie->states, aho->states, numstates, reg_trie_state );
    Newx( q, numstates, U32);
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
        Perl_re_indentf( aTHX_  "Stclass Failtable (%" UVuf " states): 0",
                      depth, (UV)numstates
        );
        for( q_read=1; q_read<numstates; q_read++ ) {
            Perl_re_printf( aTHX_  ", %" UVuf, (UV)fail[q_read]);
        }
        Perl_re_printf( aTHX_  "\n");
    });
    Safefree(q);
    /*RExC_seen |= REG_TRIEDFA_SEEN;*/
    return stclass;
}


/* The below joins as many adjacent EXACTish nodes as possible into a single
 * one.  The regop may be changed if the node(s) contain certain sequences that
 * require special handling.  The joining is only done if:
 * 1) there is room in the current conglomerated node to entirely contain the
 *    next one.
 * 2) they are compatible node types
 *
 * The adjacent nodes actually may be separated by NOTHING-kind nodes, and
 * these get optimized out
 *
 * XXX khw thinks this should be enhanced to fill EXACT (at least) nodes as full
 * as possible, even if that means splitting an existing node so that its first
 * part is moved to the preceeding node.  This would maximise the efficiency of
 * memEQ during matching.
 *
 * If a node is to match under /i (folded), the number of characters it matches
 * can be different than its character length if it contains a multi-character
 * fold.  *min_subtract is set to the total delta number of characters of the
 * input nodes.
 *
 * And *unfolded_multi_char is set to indicate whether or not the node contains
 * an unfolded multi-char fold.  This happens when it won't be known until
 * runtime whether the fold is valid or not; namely
 *  1) for EXACTF nodes that contain LATIN SMALL LETTER SHARP S, as only if the
 *      target string being matched against turns out to be UTF-8 is that fold
 *      valid; or
 *  2) for EXACTFL nodes whose folding rules depend on the locale in force at
 *      runtime.
 * (Multi-char folds whose components are all above the Latin1 range are not
 * run-time locale dependent, and have already been folded by the time this
 * function is called.)
 *
 * This is as good a place as any to discuss the design of handling these
 * multi-character fold sequences.  It's been wrong in Perl for a very long
 * time.  There are three code points in Unicode whose multi-character folds
 * were long ago discovered to mess things up.  The previous designs for
 * dealing with these involved assigning a special node for them.  This
 * approach doesn't always work, as evidenced by this example:
 *      "\xDFs" =~ /s\xDF/ui    # Used to fail before these patches
 * Both sides fold to "sss", but if the pattern is parsed to create a node that
 * would match just the \xDF, it won't be able to handle the case where a
 * successful match would have to cross the node's boundary.  The new approach
 * that hopefully generally solves the problem generates an EXACTFUP node
 * that is "sss" in this case.
 *
 * It turns out that there are problems with all multi-character folds, and not
 * just these three.  Now the code is general, for all such cases.  The
 * approach taken is:
 * 1)   This routine examines each EXACTFish node that could contain multi-
 *      character folded sequences.  Since a single character can fold into
 *      such a sequence, the minimum match length for this node is less than
 *      the number of characters in the node.  This routine returns in
 *      *min_subtract how many characters to subtract from the the actual
 *      length of the string to get a real minimum match length; it is 0 if
 *      there are no multi-char foldeds.  This delta is used by the caller to
 *      adjust the min length of the match, and the delta between min and max,
 *      so that the optimizer doesn't reject these possibilities based on size
 *      constraints.
 *
 * 2)   For the sequence involving the LATIN SMALL LETTER SHARP S (U+00DF)
 *      under /u, we fold it to 'ss' in regatom(), and in this routine, after
 *      joining, we scan for occurrences of the sequence 'ss' in non-UTF-8
 *      EXACTFU nodes.  The node type of such nodes is then changed to
 *      EXACTFUP, indicating it is problematic, and needs careful handling.
 *      (The procedures in step 1) above are sufficient to handle this case in
 *      UTF-8 encoded nodes.)  The reason this is problematic is that this is
 *      the only case where there is a possible fold length change in non-UTF-8
 *      patterns.  By reserving a special node type for problematic cases, the
 *      far more common regular EXACTFU nodes can be processed faster.
 *      regexec.c takes advantage of this.
 *
 *      EXACTFUP has been created as a grab-bag for (hopefully uncommon)
 *      problematic cases.   These all only occur when the pattern is not
 *      UTF-8.  In addition to the 'ss' sequence where there is a possible fold
 *      length change, it handles the situation where the string cannot be
 *      entirely folded.  The strings in an EXACTFish node are folded as much
 *      as possible during compilation in regcomp.c.  This saves effort in
 *      regex matching.  By using an EXACTFUP node when it is not possible to
 *      fully fold at compile time, regexec.c can know that everything in an
 *      EXACTFU node is folded, so folding can be skipped at runtime.  The only
 *      case where folding in EXACTFU nodes can't be done at compile time is
 *      the presumably uncommon MICRO SIGN, when the pattern isn't UTF-8.  This
 *      is because its fold requires UTF-8 to represent.  Thus EXACTFUP nodes
 *      handle two very different cases.  Alternatively, there could have been
 *      a node type where there are length changes, one for unfolded, and one
 *      for both.  If yet another special case needed to be created, the number
 *      of required node types would have to go to 7.  khw figures that even
 *      though there are plenty of node types to spare, that the maintenance
 *      cost wasn't worth the small speedup of doing it that way, especially
 *      since he thinks the MICRO SIGN is rarely encountered in practice.
 *
 *      There are other cases where folding isn't done at compile time, but
 *      none of them are under /u, and hence not for EXACTFU nodes.  The folds
 *      in EXACTFL nodes aren't known until runtime, and vary as the locale
 *      changes.  Some folds in EXACTF depend on if the runtime target string
 *      is UTF-8 or not.  (regatom() will create an EXACTFU node even under /di
 *      when no fold in it depends on the UTF-8ness of the target string.)
 *
 * 3)   A problem remains for unfolded multi-char folds. (These occur when the
 *      validity of the fold won't be known until runtime, and so must remain
 *      unfolded for now.  This happens for the sharp s in EXACTF and EXACTFAA
 *      nodes when the pattern isn't in UTF-8.  (Note, BTW, that there cannot
 *      be an EXACTF node with a UTF-8 pattern.)  They also occur for various
 *      folds in EXACTFL nodes, regardless of the UTF-ness of the pattern.)
 *      The reason this is a problem is that the optimizer part of regexec.c
 *      (probably unwittingly, in Perl_regexec_flags()) makes an assumption
 *      that a character in the pattern corresponds to at most a single
 *      character in the target string.  (And I do mean character, and not byte
 *      here, unlike other parts of the documentation that have never been
 *      updated to account for multibyte Unicode.)  Sharp s in EXACTF and
 *      EXACTFL nodes can match the two character string 'ss'; in EXACTFAA
 *      nodes it can match "\x{17F}\x{17F}".  These, along with other ones in
 *      EXACTFL nodes, violate the assumption, and they are the only instances
 *      where it is violated.  I'm reluctant to try to change the assumption,
 *      as the code involved is impenetrable to me (khw), so instead the code
 *      here punts.  This routine examines EXACTFL nodes, and (when the pattern
 *      isn't UTF-8) EXACTF and EXACTFAA for such unfolded folds, and returns a
 *      boolean indicating whether or not the node contains such a fold.  When
 *      it is true, the caller sets a flag that later causes the optimizer in
 *      this file to not set values for the floating and fixed string lengths,
 *      and thus avoids the optimizer code in regexec.c that makes the invalid
 *      assumption.  Thus, there is no optimization based on string lengths for
 *      EXACTFL nodes that contain these few folds, nor for non-UTF8-pattern
 *      EXACTF and EXACTFAA nodes that contain the sharp s.  (The reason the
 *      assumption is wrong only in these cases is that all other non-UTF-8
 *      folds are 1-1; and, for UTF-8 patterns, we pre-fold all other folds to
 *      their expanded versions.  (Again, we can't prefold sharp s to 'ss' in
 *      EXACTF nodes because we don't know at compile time if it actually
 *      matches 'ss' or not.  For EXACTF nodes it will match iff the target
 *      string is in UTF-8.  This is in contrast to EXACTFU nodes, where it
 *      always matches; and EXACTFAA where it never does.  In an EXACTFAA node
 *      in a UTF-8 pattern, sharp s is folded to "\x{17F}\x{17F}, avoiding the
 *      problem; but in a non-UTF8 pattern, folding it to that above-Latin1
 *      string would require the pattern to be forced into UTF-8, the overhead
 *      of which we want to avoid.  Similarly the unfolded multi-char folds in
 *      EXACTFL nodes will match iff the locale at the time of match is a UTF-8
 *      locale.)
 *
 *      Similarly, the code that generates tries doesn't currently handle
 *      not-already-folded multi-char folds, and it looks like a pain to change
 *      that.  Therefore, trie generation of EXACTFAA nodes with the sharp s
 *      doesn't work.  Instead, such an EXACTFAA is turned into a new regnode,
 *      EXACTFAA_NO_TRIE, which the trie code knows not to handle.  Most people
 *      using /iaa matching will be doing so almost entirely with ASCII
 *      strings, so this should rarely be encountered in practice */

#define JOIN_EXACT(scan,min_subtract,unfolded_multi_char, flags) \
    if (PL_regkind[OP(scan)] == EXACT) \
        join_exact(pRExC_state,(scan),(min_subtract),unfolded_multi_char, (flags), NULL, depth+1)

STATIC U32
S_join_exact(pTHX_ RExC_state_t *pRExC_state, regnode *scan,
                   UV *min_subtract, bool *unfolded_multi_char,
                   U32 flags, regnode *val, U32 depth)
{
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
    DEBUG_PEEP("join", scan, depth, 0);

    assert(PL_regkind[OP(scan)] == EXACT);

    /* Look through the subsequent nodes in the chain.  Skip NOTHING, merge
     * EXACT ones that are mergeable to the current one. */
    while (    n
           && (    PL_regkind[OP(n)] == NOTHING
               || (stringok && PL_regkind[OP(n)] == EXACT))
           && NEXT_OFF(n)
           && NEXT_OFF(scan) + NEXT_OFF(n) < I16_MAX)
    {

        if (OP(n) == TAIL || n > next)
            stringok = 0;
        if (PL_regkind[OP(n)] == NOTHING) {
            DEBUG_PEEP("skip:", n, depth, 0);
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

            /* XXX I (khw) kind of doubt that this works on platforms (should
             * Perl ever run on one) where U8_MAX is above 255 because of lots
             * of other assumptions */
            /* Don't join if the sum can't fit into a single node */
            if (oldl + STR_LEN(n) > U8_MAX)
                break;

            /* Joining something that requires UTF-8 with something that
             * doesn't, means the result requires UTF-8. */
            if (OP(scan) == EXACT && (OP(n) == EXACT_ONLY8)) {
                OP(scan) = EXACT_ONLY8;
            }
            else if (OP(scan) == EXACT_ONLY8 && (OP(n) == EXACT)) {
                ;   /* join is compatible, no need to change OP */
            }
            else if ((OP(scan) == EXACTFU) && (OP(n) == EXACTFU_ONLY8)) {
                OP(scan) = EXACTFU_ONLY8;
            }
            else if ((OP(scan) == EXACTFU_ONLY8) && (OP(n) == EXACTFU)) {
                ;   /* join is compatible, no need to change OP */
            }
            else if (OP(scan) == EXACTFU && OP(n) == EXACTFU) {
                ;   /* join is compatible, no need to change OP */
            }
            else if (OP(scan) == EXACTFU && OP(n) == EXACTFU_S_EDGE) {

                 /* Under /di, temporary EXACTFU_S_EDGE nodes are generated,
                  * which can join with EXACTFU ones.  We check for this case
                  * here.  These need to be resolved to either EXACTFU or
                  * EXACTF at joining time.  They have nothing in them that
                  * would forbid them from being the more desirable EXACTFU
                  * nodes except that they begin and/or end with a single [Ss].
                  * The reason this is problematic is because they could be
                  * joined in this loop with an adjacent node that ends and/or
                  * begins with [Ss] which would then form the sequence 'ss',
                  * which matches differently under /di than /ui, in which case
                  * EXACTFU can't be used.  If the 'ss' sequence doesn't get
                  * formed, the nodes get absorbed into any adjacent EXACTFU
                  * node.  And if the only adjacent node is EXACTF, they get
                  * absorbed into that, under the theory that a longer node is
                  * better than two shorter ones, even if one is EXACTFU.  Note
                  * that EXACTFU_ONLY8 is generated only for UTF-8 patterns,
                  * and the EXACTFU_S_EDGE ones only for non-UTF-8.  */

                if (STRING(n)[STR_LEN(n)-1] == 's') {

                    /* Here the joined node would end with 's'.  If the node
                     * following the combination is an EXACTF one, it's better to
                     * join this trailing edge 's' node with that one, leaving the
                     * current one in 'scan' be the more desirable EXACTFU */
                    if (OP(nnext) == EXACTF) {
                        break;
                    }

                    OP(scan) = EXACTFU_S_EDGE;

                }   /* Otherwise, the beginning 's' of the 2nd node just
                       becomes an interior 's' in 'scan' */
            }
            else if (OP(scan) == EXACTF && OP(n) == EXACTF) {
                ;   /* join is compatible, no need to change OP */
            }
            else if (OP(scan) == EXACTF && OP(n) == EXACTFU_S_EDGE) {

                /* EXACTF nodes are compatible for joining with EXACTFU_S_EDGE
                 * nodes.  But the latter nodes can be also joined with EXACTFU
                 * ones, and that is a better outcome, so if the node following
                 * 'n' is EXACTFU, quit now so that those two can be joined
                 * later */
                if (OP(nnext) == EXACTFU) {
                    break;
                }

                /* The join is compatible, and the combined node will be
                 * EXACTF.  (These don't care if they begin or end with 's' */
            }
            else if (OP(scan) == EXACTFU_S_EDGE && OP(n) == EXACTFU_S_EDGE) {
                if (   STRING(scan)[STR_LEN(scan)-1] == 's'
                    && STRING(n)[0] == 's')
                {
                    /* When combined, we have the sequence 'ss', which means we
                     * have to remain /di */
                    OP(scan) = EXACTF;
                }
            }
            else if (OP(scan) == EXACTFU_S_EDGE && OP(n) == EXACTFU) {
                if (STRING(n)[0] == 's') {
                    ;   /* Here the join is compatible and the combined node
                           starts with 's', no need to change OP */
                }
                else {  /* Now the trailing 's' is in the interior */
                    OP(scan) = EXACTFU;
                }
            }
            else if (OP(scan) == EXACTFU_S_EDGE && OP(n) == EXACTF) {

                /* The join is compatible, and the combined node will be
                 * EXACTF.  (These don't care if they begin or end with 's' */
                OP(scan) = EXACTF;
            }
            else if (OP(scan) != OP(n)) {

                /* The only other compatible joinings are the same node type */
                break;
            }

            DEBUG_PEEP("merg", n, depth, 0);
            merged++;

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
	    DEBUG_PEEP("atch", val, depth, 0);
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

    /* This temporary node can now be turned into EXACTFU, and must, as
     * regexec.c doesn't handle it */
    if (OP(scan) == EXACTFU_S_EDGE) {
        OP(scan) = EXACTFU;
    }

    *min_subtract = 0;
    *unfolded_multi_char = FALSE;

    /* Here, all the adjacent mergeable EXACTish nodes have been merged.  We
     * can now analyze for sequences of problematic code points.  (Prior to
     * this final joining, sequences could have been split over boundaries, and
     * hence missed).  The sequences only happen in folding, hence for any
     * non-EXACT EXACTish node */
    if (OP(scan) != EXACT && OP(scan) != EXACT_ONLY8 && OP(scan) != EXACTL) {
        U8* s0 = (U8*) STRING(scan);
        U8* s = s0;
        U8* s_end = s0 + STR_LEN(scan);

        int total_count_delta = 0;  /* Total delta number of characters that
                                       multi-char folds expand to */

	/* One pass is made over the node's string looking for all the
	 * possibilities.  To avoid some tests in the loop, there are two main
	 * cases, for UTF-8 patterns (which can't have EXACTF nodes) and
	 * non-UTF-8 */
	if (UTF) {
            U8* folded = NULL;

            if (OP(scan) == EXACTFL) {
                U8 *d;

                /* An EXACTFL node would already have been changed to another
                 * node type unless there is at least one character in it that
                 * is problematic; likely a character whose fold definition
                 * won't be known until runtime, and so has yet to be folded.
                 * For all but the UTF-8 locale, folds are 1-1 in length, but
                 * to handle the UTF-8 case, we need to create a temporary
                 * folded copy using UTF-8 locale rules in order to analyze it.
                 * This is because our macros that look to see if a sequence is
                 * a multi-char fold assume everything is folded (otherwise the
                 * tests in those macros would be too complicated and slow).
                 * Note that here, the non-problematic folds will have already
                 * been done, so we can just copy such characters.  We actually
                 * don't completely fold the EXACTFL string.  We skip the
                 * unfolded multi-char folds, as that would just create work
                 * below to figure out the size they already are */

                Newx(folded, UTF8_MAX_FOLD_CHAR_EXPAND * STR_LEN(scan) + 1, U8);
                d = folded;
                while (s < s_end) {
                    STRLEN s_len = UTF8SKIP(s);
                    if (! is_PROBLEMATIC_LOCALE_FOLD_utf8(s)) {
                        Copy(s, d, s_len, U8);
                        d += s_len;
                    }
                    else if (is_FOLDS_TO_MULTI_utf8(s)) {
                        *unfolded_multi_char = TRUE;
                        Copy(s, d, s_len, U8);
                        d += s_len;
                    }
                    else if (isASCII(*s)) {
                        *(d++) = toFOLD(*s);
                    }
                    else {
                        STRLEN len;
                        _toFOLD_utf8_flags(s, s_end, d, &len, FOLD_FLAGS_FULL);
                        d += len;
                    }
                    s += s_len;
                }

                /* Point the remainder of the routine to look at our temporary
                 * folded copy */
                s = folded;
                s_end = d;
            } /* End of creating folded copy of EXACTFL string */

            /* Examine the string for a multi-character fold sequence.  UTF-8
             * patterns have all characters pre-folded by the time this code is
             * executed */
            while (s < s_end - 1) /* Can stop 1 before the end, as minimum
                                     length sequence we are looking for is 2 */
	    {
                int count = 0;  /* How many characters in a multi-char fold */
                int len = is_MULTI_CHAR_FOLD_utf8_safe(s, s_end);
                if (! len) {    /* Not a multi-char fold: get next char */
                    s += UTF8SKIP(s);
                    continue;
                }

                { /* Here is a generic multi-char fold. */
                    U8* multi_end  = s + len;

                    /* Count how many characters are in it.  In the case of
                     * /aa, no folds which contain ASCII code points are
                     * allowed, so check for those, and skip if found. */
                    if (OP(scan) != EXACTFAA && OP(scan) != EXACTFAA_NO_TRIE) {
                        count = utf8_length(s, multi_end);
                        s = multi_end;
                    }
                    else {
                        while (s < multi_end) {
                            if (isASCII(*s)) {
                                s++;
                                goto next_iteration;
                            }
                            else {
                                s += UTF8SKIP(s);
                            }
                            count++;
                        }
                    }
                }

                /* The delta is how long the sequence is minus 1 (1 is how long
                 * the character that folds to the sequence is) */
                total_count_delta += count - 1;
              next_iteration: ;
	    }

            /* We created a temporary folded copy of the string in EXACTFL
             * nodes.  Therefore we need to be sure it doesn't go below zero,
             * as the real string could be shorter */
            if (OP(scan) == EXACTFL) {
                int total_chars = utf8_length((U8*) STRING(scan),
                                           (U8*) STRING(scan) + STR_LEN(scan));
                if (total_count_delta > total_chars) {
                    total_count_delta = total_chars;
                }
            }

            *min_subtract += total_count_delta;
            Safefree(folded);
	}
	else if (OP(scan) == EXACTFAA) {

            /* Non-UTF-8 pattern, EXACTFAA node.  There can't be a multi-char
             * fold to the ASCII range (and there are no existing ones in the
             * upper latin1 range).  But, as outlined in the comments preceding
             * this function, we need to flag any occurrences of the sharp s.
             * This character forbids trie formation (because of added
             * complexity) */
#if    UNICODE_MAJOR_VERSION > 3 /* no multifolds in early Unicode */   \
   || (UNICODE_MAJOR_VERSION == 3 && (   UNICODE_DOT_VERSION > 0)       \
                                      || UNICODE_DOT_DOT_VERSION > 0)
	    while (s < s_end) {
                if (*s == LATIN_SMALL_LETTER_SHARP_S) {
                    OP(scan) = EXACTFAA_NO_TRIE;
                    *unfolded_multi_char = TRUE;
                    break;
                }
                s++;
            }
        }
	else {

            /* Non-UTF-8 pattern, not EXACTFAA node.  Look for the multi-char
             * folds that are all Latin1.  As explained in the comments
             * preceding this function, we look also for the sharp s in EXACTF
             * and EXACTFL nodes; it can be in the final position.  Otherwise
             * we can stop looking 1 byte earlier because have to find at least
             * two characters for a multi-fold */
	    const U8* upper = (OP(scan) == EXACTF || OP(scan) == EXACTFL)
                              ? s_end
                              : s_end -1;

	    while (s < upper) {
                int len = is_MULTI_CHAR_FOLD_latin1_safe(s, s_end);
                if (! len) {    /* Not a multi-char fold. */
                    if (*s == LATIN_SMALL_LETTER_SHARP_S
                        && (OP(scan) == EXACTF || OP(scan) == EXACTFL))
                    {
                        *unfolded_multi_char = TRUE;
                    }
                    s++;
                    continue;
                }

                if (len == 2
                    && isALPHA_FOLD_EQ(*s, 's')
                    && isALPHA_FOLD_EQ(*(s+1), 's'))
                {

                    /* EXACTF nodes need to know that the minimum length
                     * changed so that a sharp s in the string can match this
                     * ss in the pattern, but they remain EXACTF nodes, as they
                     * won't match this unless the target string is is UTF-8,
                     * which we don't know until runtime.  EXACTFL nodes can't
                     * transform into EXACTFU nodes */
                    if (OP(scan) != EXACTF && OP(scan) != EXACTFL) {
                        OP(scan) = EXACTFUP;
                    }
		}

                *min_subtract += len - 1;
                s += len;
	    }
#endif
	}

        if (     STR_LEN(scan) == 1
            &&   isALPHA_A(* STRING(scan))
            &&  (         OP(scan) == EXACTFAA
                 || (     OP(scan) == EXACTFU
                     && ! HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(* STRING(scan)))))
        {
            U8 mask = ~ ('A' ^ 'a'); /* These differ in just one bit */

            /* Replace a length 1 ASCII fold pair node with an ANYOFM node,
             * with the mask set to the complement of the bit that differs
             * between upper and lower case, and the lowest code point of the
             * pair (which the '&' forces) */
            OP(scan) = ANYOFM;
            ARG_SET(scan, *STRING(scan) & mask);
            FLAGS(scan) = mask;
        }
    }

#ifdef DEBUGGING
    /* Allow dumping but overwriting the collection of skipped
     * ops and/or strings with fake optimized ops */
    n = scan + NODE_SZ_STR(scan);
    while (n <= stop) {
	OP(n) = OPTIMIZED;
	FLAGS(n) = 0;
	NEXT_OFF(n) = 0;
        n++;
    }
#endif
    DEBUG_OPTIMISE_r(if (merged){DEBUG_PEEP("finl", scan, depth, 0);});
    return stopnow;
}

/* REx optimizer.  Converts nodes into quicker variants "in place".
   Finds fixed substrings.  */

/* Stops at toplevel WHILEM as well as at "last". At end *scanp is set
   to the position after last scanned or to NULL. */

#define INIT_AND_WITHP \
    assert(!and_withp); \
    Newx(and_withp, 1, regnode_ssc); \
    SAVEFREEPV(and_withp)


static void
S_unwind_scan_frames(pTHX_ const void *p)
{
    scan_frame *f= (scan_frame *)p;
    do {
        scan_frame *n= f->next_frame;
        Safefree(f);
        f= n;
    } while (f);
}

/* Follow the next-chain of the current node and optimize away
   all the NOTHINGs from it.
 */
STATIC void
S_rck_elide_nothing(pTHX_ regnode *node)
{
    dVAR;

    PERL_ARGS_ASSERT_RCK_ELIDE_NOTHING;

    if (OP(node) != CURLYX) {
        const int max = (reg_off_by_arg[OP(node)]
                        ? I32_MAX
                          /* I32 may be smaller than U16 on CRAYs! */
                        : (I32_MAX < U16_MAX ? I32_MAX : U16_MAX));
        int off = (reg_off_by_arg[OP(node)] ? ARG(node) : NEXT_OFF(node));
        int noff;
        regnode *n = node;

        /* Skip NOTHING and LONGJMP. */
        while (
            (n = regnext(n))
            && (
                (PL_regkind[OP(n)] == NOTHING && (noff = NEXT_OFF(n)))
                || ((OP(n) == LONGJMP) && (noff = ARG(n)))
            )
            && off + noff < max
        ) {
            off += noff;
        }
        if (reg_off_by_arg[OP(node)])
            ARG(node) = off;
        else
            NEXT_OFF(node) = off;
    }
    return;
}

/* the return from this sub is the minimum length that could possibly match */
STATIC SSize_t
S_study_chunk(pTHX_ RExC_state_t *pRExC_state, regnode **scanp,
                        SSize_t *minlenp, SSize_t *deltap,
			regnode *last,
			scan_data_t *data,
			I32 stopparen,
                        U32 recursed_depth,
			regnode_ssc *and_withp,
			U32 flags, U32 depth, bool was_mutate_ok)
			/* scanp: Start here (read-write). */
			/* deltap: Write maxlen-minlen here. */
			/* last: Stop before this one. */
			/* data: string data about the pattern */
			/* stopparen: treat close N as END */
			/* recursed: which subroutines have we recursed into */
			/* and_withp: Valid if flags & SCF_DO_STCLASS_OR */
{
    dVAR;
    /* There must be at least this number of characters to match */
    SSize_t min = 0;
    I32 pars = 0, code;
    regnode *scan = *scanp, *next;
    SSize_t delta = 0;
    int is_inf = (flags & SCF_DO_SUBSTR) && (data->flags & SF_IS_INF);
    int is_inf_internal = 0;		/* The studied chunk is infinite */
    I32 is_par = OP(scan) == OPEN ? ARG(scan) : 0;
    scan_data_t data_fake;
    SV *re_trie_maxbuff = NULL;
    regnode *first_non_open = scan;
    SSize_t stopmin = SSize_t_MAX;
    scan_frame *frame = NULL;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_STUDY_CHUNK;
    RExC_study_started= 1;

    Zero(&data_fake, 1, scan_data_t);

    if ( depth == 0 ) {
        while (first_non_open && OP(first_non_open) == OPEN)
            first_non_open=regnext(first_non_open);
    }


  fake_study_recurse:
    DEBUG_r(
        RExC_study_chunk_recursed_count++;
    );
    DEBUG_OPTIMISE_MORE_r(
    {
        Perl_re_indentf( aTHX_  "study_chunk stopparen=%ld recursed_count=%lu depth=%lu recursed_depth=%lu scan=%p last=%p",
            depth, (long)stopparen,
            (unsigned long)RExC_study_chunk_recursed_count,
            (unsigned long)depth, (unsigned long)recursed_depth,
            scan,
            last);
        if (recursed_depth) {
            U32 i;
            U32 j;
            for ( j = 0 ; j < recursed_depth ; j++ ) {
                for ( i = 0 ; i < (U32)RExC_total_parens ; i++ ) {
                    if (
                        PAREN_TEST(RExC_study_chunk_recursed +
                                   ( j * RExC_study_chunk_recursed_bytes), i )
                        && (
                            !j ||
                            !PAREN_TEST(RExC_study_chunk_recursed +
                                   (( j - 1 ) * RExC_study_chunk_recursed_bytes), i)
                        )
                    ) {
                        Perl_re_printf( aTHX_ " %d",(int)i);
                        break;
                    }
                }
                if ( j + 1 < recursed_depth ) {
                    Perl_re_printf( aTHX_  ",");
                }
            }
        }
        Perl_re_printf( aTHX_ "\n");
    }
    );
    while ( scan && OP(scan) != END && scan < last ){
        UV min_subtract = 0;    /* How mmany chars to subtract from the minimum
                                   node length to get a real minimum (because
                                   the folded version may be shorter) */
	bool unfolded_multi_char = FALSE;
        /* avoid mutating ops if we are anywhere within the recursed or
         * enframed handling for a GOSUB: the outermost level will handle it.
         */
        bool mutate_ok = was_mutate_ok && !(frame && frame->in_gosub);
	/* Peephole optimizer: */
        DEBUG_STUDYDATA("Peep", data, depth, is_inf);
        DEBUG_PEEP("Peep", scan, depth, flags);


        /* The reason we do this here is that we need to deal with things like
         * /(?:f)(?:o)(?:o)/ which cant be dealt with by the normal EXACT
         * parsing code, as each (?:..) is handled by a different invocation of
         * reg() -- Yves
         */
        if (mutate_ok)
            JOIN_EXACT(scan,&min_subtract, &unfolded_multi_char, 0);

        /* Follow the next-chain of the current node and optimize
           away all the NOTHINGs from it.
         */
        rck_elide_nothing(scan);

	/* The principal pseudo-switch.  Cannot be a switch, since we
	   look into several different things.  */
        if ( OP(scan) == DEFINEP ) {
            SSize_t minlen = 0;
            SSize_t deltanext = 0;
            SSize_t fake_last_close = 0;
            I32 f = SCF_IN_DEFINE;

            StructCopy(&zero_scan_data, &data_fake, scan_data_t);
            scan = regnext(scan);
            assert( OP(scan) == IFTHEN );
            DEBUG_PEEP("expect IFTHEN", scan, depth, flags);

            data_fake.last_closep= &fake_last_close;
            minlen = *minlenp;
            next = regnext(scan);
            scan = NEXTOPER(NEXTOPER(scan));
            DEBUG_PEEP("scan", scan, depth, flags);
            DEBUG_PEEP("next", next, depth, flags);

            /* we suppose the run is continuous, last=next...
             * NOTE we dont use the return here! */
            /* DEFINEP study_chunk() recursion */
            (void)study_chunk(pRExC_state, &scan, &minlen,
                              &deltanext, next, &data_fake, stopparen,
                              recursed_depth, NULL, f, depth+1, mutate_ok);

            scan = next;
        } else
        if (
            OP(scan) == BRANCH  ||
            OP(scan) == BRANCHJ ||
            OP(scan) == IFTHEN
        ) {
	    next = regnext(scan);
	    code = OP(scan);

            /* The op(next)==code check below is to see if we
             * have "BRANCH-BRANCH", "BRANCHJ-BRANCHJ", "IFTHEN-IFTHEN"
             * IFTHEN is special as it might not appear in pairs.
             * Not sure whether BRANCH-BRANCHJ is possible, regardless
             * we dont handle it cleanly. */
	    if (OP(next) == code || code == IFTHEN) {
                /* NOTE - There is similar code to this block below for
                 * handling TRIE nodes on a re-study.  If you change stuff here
                 * check there too. */
		SSize_t max1 = 0, min1 = SSize_t_MAX, num = 0;
		regnode_ssc accum;
		regnode * const startbranch=scan;

                if (flags & SCF_DO_SUBSTR) {
                    /* Cannot merge strings after this. */
                    scan_commit(pRExC_state, data, minlenp, is_inf);
                }

                if (flags & SCF_DO_STCLASS)
		    ssc_init_zero(pRExC_state, &accum);

		while (OP(scan) == code) {
		    SSize_t deltanext, minnext, fake;
		    I32 f = 0;
		    regnode_ssc this_class;

                    DEBUG_PEEP("Branch", scan, depth, flags);

		    num++;
                    StructCopy(&zero_scan_data, &data_fake, scan_data_t);
		    if (data) {
			data_fake.whilem_c = data->whilem_c;
			data_fake.last_closep = data->last_closep;
		    }
		    else
			data_fake.last_closep = &fake;

		    data_fake.pos_delta = delta;
		    next = regnext(scan);

                    scan = NEXTOPER(scan); /* everything */
                    if (code != BRANCH)    /* everything but BRANCH */
			scan = NEXTOPER(scan);

		    if (flags & SCF_DO_STCLASS) {
			ssc_init(pRExC_state, &this_class);
			data_fake.start_class = &this_class;
			f = SCF_DO_STCLASS_AND;
		    }
		    if (flags & SCF_WHILEM_VISITED_POS)
			f |= SCF_WHILEM_VISITED_POS;

		    /* we suppose the run is continuous, last=next...*/
                    /* recurse study_chunk() for each BRANCH in an alternation */
		    minnext = study_chunk(pRExC_state, &scan, minlenp,
                                      &deltanext, next, &data_fake, stopparen,
                                      recursed_depth, NULL, f, depth+1,
                                      mutate_ok);

		    if (min1 > minnext)
			min1 = minnext;
		    if (deltanext == SSize_t_MAX) {
			is_inf = is_inf_internal = 1;
			max1 = SSize_t_MAX;
		    } else if (max1 < minnext + deltanext)
			max1 = minnext + deltanext;
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
			ssc_or(pRExC_state, &accum, (regnode_charclass*)&this_class);
		}
		if (code == IFTHEN && num < 2) /* Empty ELSE branch */
		    min1 = 0;
		if (flags & SCF_DO_SUBSTR) {
		    data->pos_min += min1;
		    if (data->pos_delta >= SSize_t_MAX - (max1 - min1))
		        data->pos_delta = SSize_t_MAX;
		    else
		        data->pos_delta += max1 - min1;
		    if (max1 != min1 || is_inf)
			data->cur_is_floating = 1;
		}
		min += min1;
		if (delta == SSize_t_MAX
		 || SSize_t_MAX - delta - (max1 - min1) < 0)
		    delta = SSize_t_MAX;
		else
		    delta += max1 - min1;
		if (flags & SCF_DO_STCLASS_OR) {
		    ssc_or(pRExC_state, data->start_class, (regnode_charclass*) &accum);
		    if (min1) {
			ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);
			flags &= ~SCF_DO_STCLASS;
		    }
		}
		else if (flags & SCF_DO_STCLASS_AND) {
		    if (min1) {
			ssc_and(pRExC_state, data->start_class, (regnode_charclass *) &accum);
			flags &= ~SCF_DO_STCLASS;
		    }
		    else {
			/* Switch to OR mode: cache the old value of
			 * data->start_class */
			INIT_AND_WITHP;
			StructCopy(data->start_class, and_withp, regnode_ssc);
			flags &= ~SCF_DO_STCLASS_AND;
			StructCopy(&accum, data->start_class, regnode_ssc);
			flags |= SCF_DO_STCLASS_OR;
		    }
		}

                if (PERL_ENABLE_TRIE_OPTIMISATION
                    && OP(startbranch) == BRANCH
                    && mutate_ok
                ) {
		/* demq.

                   Assuming this was/is a branch we are dealing with: 'scan'
                   now points at the item that follows the branch sequence,
                   whatever it is. We now start at the beginning of the
                   sequence and look for subsequences of

		   BRANCH->EXACT=>x1
		   BRANCH->EXACT=>x2
		   tail

                   which would be constructed from a pattern like
                   /A|LIST|OF|WORDS/

		   If we can find such a subsequence we need to turn the first
		   element into a trie and then add the subsequent branch exact
		   strings to the trie.

		   We have two cases

                     1. patterns where the whole set of branches can be
                        converted.

		     2. patterns where only a subset can be converted.

		   In case 1 we can replace the whole set with a single regop
		   for the trie. In case 2 we need to keep the start and end
		   branches so

		     'BRANCH EXACT; BRANCH EXACT; BRANCH X'
		     becomes BRANCH TRIE; BRANCH X;

		  There is an additional case, that being where there is a
		  common prefix, which gets split out into an EXACT like node
		  preceding the TRIE node.

		  If x(1..n)==tail then we can do a simple trie, if not we make
		  a "jump" trie, such that when we match the appropriate word
		  we "jump" to the appropriate tail node. Essentially we turn
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
                        U8 trietype = 0;
                        U32 count=0;

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


                        DEBUG_TRIE_COMPILE_r({
                            regprop(RExC_rx, RExC_mysv, tail, NULL, pRExC_state);
                            Perl_re_indentf( aTHX_  "%s %" UVuf ":%s\n",
                              depth+1,
                              "Looking for TRIE'able sequences. Tail node is ",
                              (UV) REGNODE_OFFSET(tail),
                              SvPV_nolen_const( RExC_mysv )
                            );
                        });

                        /*

                            Step through the branches
                                cur represents each branch,
                                noper is the first thing to be matched as part
                                      of that branch
                                noper_next is the regnext() of that node.

                            We normally handle a case like this
                            /FOO[xyz]|BAR[pqr]/ via a "jump trie" but we also
                            support building with NOJUMPTRIE, which restricts
                            the trie logic to structures like /FOO|BAR/.

                            If noper is a trieable nodetype then the branch is
                            a possible optimization target. If we are building
                            under NOJUMPTRIE then we require that noper_next is
                            the same as scan (our current position in the regex
                            program).

                            Once we have two or more consecutive such branches
                            we can create a trie of the EXACT's contents and
                            stitch it in place into the program.

                            If the sequence represents all of the branches in
                            the alternation we replace the entire thing with a
                            single TRIE node.

                            Otherwise when it is a subsequence we need to
                            stitch it in place and replace only the relevant
                            branches. This means the first branch has to remain
                            as it is used by the alternation logic, and its
                            next pointer, and needs to be repointed at the item
                            on the branch chain following the last branch we
                            have optimized away.

                            This could be either a BRANCH, in which case the
                            subsequence is internal, or it could be the item
                            following the branch sequence in which case the
                            subsequence is at the end (which does not
                            necessarily mean the first node is the start of the
                            alternation).

                            TRIE_TYPE(X) is a define which maps the optype to a
                            trietype.

                                optype          |  trietype
                                ----------------+-----------
                                NOTHING         | NOTHING
                                EXACT           | EXACT
                                EXACT_ONLY8     | EXACT
                                EXACTFU         | EXACTFU
                                EXACTFU_ONLY8   | EXACTFU
                                EXACTFUP        | EXACTFU
                                EXACTFAA        | EXACTFAA
                                EXACTL          | EXACTL
                                EXACTFLU8       | EXACTFLU8


                        */
#define TRIE_TYPE(X) ( ( NOTHING == (X) )                                   \
                       ? NOTHING                                            \
                       : ( EXACT == (X) || EXACT_ONLY8 == (X) )             \
                         ? EXACT                                            \
                         : (     EXACTFU == (X)                             \
                              || EXACTFU_ONLY8 == (X)                       \
                              || EXACTFUP == (X) )                          \
                           ? EXACTFU                                        \
                           : ( EXACTFAA == (X) )                            \
                             ? EXACTFAA                                     \
                             : ( EXACTL == (X) )                            \
                               ? EXACTL                                     \
                               : ( EXACTFLU8 == (X) )                       \
                                 ? EXACTFLU8                                \
                                 : 0 )

                        /* dont use tail as the end marker for this traverse */
                        for ( cur = startbranch ; cur != scan ; cur = regnext( cur ) ) {
                            regnode * const noper = NEXTOPER( cur );
                            U8 noper_type = OP( noper );
                            U8 noper_trietype = TRIE_TYPE( noper_type );
#if defined(DEBUGGING) || defined(NOJUMPTRIE)
                            regnode * const noper_next = regnext( noper );
                            U8 noper_next_type = (noper_next && noper_next < tail) ? OP(noper_next) : 0;
                            U8 noper_next_trietype = (noper_next && noper_next < tail) ? TRIE_TYPE( noper_next_type ) :0;
#endif

                            DEBUG_TRIE_COMPILE_r({
                                regprop(RExC_rx, RExC_mysv, cur, NULL, pRExC_state);
                                Perl_re_indentf( aTHX_  "- %d:%s (%d)",
                                   depth+1,
                                   REG_NODE_NUM(cur), SvPV_nolen_const( RExC_mysv ), REG_NODE_NUM(cur) );

                                regprop(RExC_rx, RExC_mysv, noper, NULL, pRExC_state);
                                Perl_re_printf( aTHX_  " -> %d:%s",
                                    REG_NODE_NUM(noper), SvPV_nolen_const(RExC_mysv));

                                if ( noper_next ) {
                                  regprop(RExC_rx, RExC_mysv, noper_next, NULL, pRExC_state);
                                  Perl_re_printf( aTHX_ "\t=> %d:%s\t",
                                    REG_NODE_NUM(noper_next), SvPV_nolen_const(RExC_mysv));
                                }
                                Perl_re_printf( aTHX_  "(First==%d,Last==%d,Cur==%d,tt==%s,ntt==%s,nntt==%s)\n",
                                   REG_NODE_NUM(first), REG_NODE_NUM(last), REG_NODE_NUM(cur),
				   PL_reg_name[trietype], PL_reg_name[noper_trietype], PL_reg_name[noper_next_trietype]
				);
                            });

                            /* Is noper a trieable nodetype that can be merged
                             * with the current trie (if there is one)? */
                            if ( noper_trietype
                                  &&
                                  (
                                        ( noper_trietype == NOTHING )
                                        || ( trietype == NOTHING )
                                        || ( trietype == noper_trietype )
                                  )
#ifdef NOJUMPTRIE
                                  && noper_next >= tail
#endif
                                  && count < U16_MAX)
                            {
                                /* Handle mergable triable node Either we are
                                 * the first node in a new trieable sequence,
                                 * in which case we do some bookkeeping,
                                 * otherwise we update the end pointer. */
                                if ( !first ) {
                                    first = cur;
				    if ( noper_trietype == NOTHING ) {
#if !defined(DEBUGGING) && !defined(NOJUMPTRIE)
					regnode * const noper_next = regnext( noper );
                                        U8 noper_next_type = (noper_next && noper_next < tail) ? OP(noper_next) : 0;
					U8 noper_next_trietype = noper_next_type ? TRIE_TYPE( noper_next_type ) :0;
#endif

                                        if ( noper_next_trietype ) {
					    trietype = noper_next_trietype;
                                        } else if (noper_next_type)  {
                                            /* a NOTHING regop is 1 regop wide.
                                             * We need at least two for a trie
                                             * so we can't merge this in */
                                            first = NULL;
                                        }
                                    } else {
                                        trietype = noper_trietype;
                                    }
                                } else {
                                    if ( trietype == NOTHING )
                                        trietype = noper_trietype;
                                    last = cur;
                                }
				if (first)
				    count++;
                            } /* end handle mergable triable node */
                            else {
                                /* handle unmergable node -
                                 * noper may either be a triable node which can
                                 * not be tried together with the current trie,
                                 * or a non triable node */
                                if ( last ) {
                                    /* If last is set and trietype is not
                                     * NOTHING then we have found at least two
                                     * triable branch sequences in a row of a
                                     * similar trietype so we can turn them
                                     * into a trie. If/when we allow NOTHING to
                                     * start a trie sequence this condition
                                     * will be required, and it isn't expensive
                                     * so we leave it in for now. */
                                    if ( trietype && trietype != NOTHING )
                                        make_trie( pRExC_state,
                                                startbranch, first, cur, tail,
                                                count, trietype, depth+1 );
                                    last = NULL; /* note: we clear/update
                                                    first, trietype etc below,
                                                    so we dont do it here */
                                }
                                if ( noper_trietype
#ifdef NOJUMPTRIE
                                     && noper_next >= tail
#endif
                                ){
                                    /* noper is triable, so we can start a new
                                     * trie sequence */
                                    count = 1;
                                    first = cur;
                                    trietype = noper_trietype;
                                } else if (first) {
                                    /* if we already saw a first but the
                                     * current node is not triable then we have
                                     * to reset the first information. */
                                    count = 0;
                                    first = NULL;
                                    trietype = 0;
                                }
                            } /* end handle unmergable node */
                        } /* loop over branches */
                        DEBUG_TRIE_COMPILE_r({
                            regprop(RExC_rx, RExC_mysv, cur, NULL, pRExC_state);
                            Perl_re_indentf( aTHX_  "- %s (%d) <SCAN FINISHED> ",
                              depth+1, SvPV_nolen_const( RExC_mysv ), REG_NODE_NUM(cur));
                            Perl_re_printf( aTHX_  "(First==%d, Last==%d, Cur==%d, tt==%s)\n",
                               REG_NODE_NUM(first), REG_NODE_NUM(last), REG_NODE_NUM(cur),
                               PL_reg_name[trietype]
                            );

                        });
                        if ( last && trietype ) {
                            if ( trietype != NOTHING ) {
                                /* the last branch of the sequence was part of
                                 * a trie, so we have to construct it here
                                 * outside of the loop */
                                made= make_trie( pRExC_state, startbranch,
                                                 first, scan, tail, count,
                                                 trietype, depth+1 );
#ifdef TRIE_STUDY_OPT
                                if ( ((made == MADE_EXACT_TRIE &&
                                     startbranch == first)
                                     || ( first_non_open == first )) &&
                                     depth==0 ) {
                                    flags |= SCF_TRIE_RESTUDY;
                                    if ( startbranch == first
                                         && scan >= tail )
                                    {
                                        RExC_seen &=~REG_TOP_LEVEL_BRANCHES_SEEN;
                                    }
                                }
#endif
                            } else {
                                /* at this point we know whatever we have is a
                                 * NOTHING sequence/branch AND if 'startbranch'
                                 * is 'first' then we can turn the whole thing
                                 * into a NOTHING
                                 */
                                if ( startbranch == first ) {
                                    regnode *opt;
                                    /* the entire thing is a NOTHING sequence,
                                     * something like this: (?:|) So we can
                                     * turn it into a plain NOTHING op. */
                                    DEBUG_TRIE_COMPILE_r({
                                        regprop(RExC_rx, RExC_mysv, cur, NULL, pRExC_state);
                                        Perl_re_indentf( aTHX_  "- %s (%d) <NOTHING BRANCH SEQUENCE>\n",
                                          depth+1,
                                          SvPV_nolen_const( RExC_mysv ), REG_NODE_NUM(cur));

                                    });
                                    OP(startbranch)= NOTHING;
                                    NEXT_OFF(startbranch)= tail - startbranch;
                                    for ( opt= startbranch + 1; opt < tail ; opt++ )
                                        OP(opt)= OPTIMIZED;
                                }
                            }
                        } /* end if ( last) */
                    } /* TRIE_MAXBUF is non zero */

                } /* do trie */

	    }
	    else if ( code == BRANCHJ ) {  /* single branch is optimized. */
		scan = NEXTOPER(NEXTOPER(scan));
	    } else			/* single branch is optimized. */
		scan = NEXTOPER(scan);
	    continue;
        } else if (OP(scan) == SUSPEND || OP(scan) == GOSUB) {
            I32 paren = 0;
            regnode *start = NULL;
            regnode *end = NULL;
            U32 my_recursed_depth= recursed_depth;

            if (OP(scan) != SUSPEND) { /* GOSUB */
                /* Do setup, note this code has side effects beyond
                 * the rest of this block. Specifically setting
                 * RExC_recurse[] must happen at least once during
                 * study_chunk(). */
                paren = ARG(scan);
                RExC_recurse[ARG2L(scan)] = scan;
                start = REGNODE_p(RExC_open_parens[paren]);
                end   = REGNODE_p(RExC_close_parens[paren]);

                /* NOTE we MUST always execute the above code, even
                 * if we do nothing with a GOSUB */
                if (
                    ( flags & SCF_IN_DEFINE )
                    ||
                    (
                        (is_inf_internal || is_inf || (data && data->flags & SF_IS_INF))
                        &&
                        ( (flags & (SCF_DO_STCLASS | SCF_DO_SUBSTR)) == 0 )
                    )
                ) {
                    /* no need to do anything here if we are in a define. */
                    /* or we are after some kind of infinite construct
                     * so we can skip recursing into this item.
                     * Since it is infinite we will not change the maxlen
                     * or delta, and if we miss something that might raise
                     * the minlen it will merely pessimise a little.
                     *
                     * Iow /(?(DEFINE)(?<foo>foo|food))a+(?&foo)/
                     * might result in a minlen of 1 and not of 4,
                     * but this doesn't make us mismatch, just try a bit
                     * harder than we should.
                     * */
                    scan= regnext(scan);
                    continue;
                }

                if (
                    !recursed_depth
                    ||
                    !PAREN_TEST(RExC_study_chunk_recursed + ((recursed_depth-1) * RExC_study_chunk_recursed_bytes), paren)
                ) {
                    /* it is quite possible that there are more efficient ways
                     * to do this. We maintain a bitmap per level of recursion
                     * of which patterns we have entered so we can detect if a
                     * pattern creates a possible infinite loop. When we
                     * recurse down a level we copy the previous levels bitmap
                     * down. When we are at recursion level 0 we zero the top
                     * level bitmap. It would be nice to implement a different
                     * more efficient way of doing this. In particular the top
                     * level bitmap may be unnecessary.
                     */
                    if (!recursed_depth) {
                        Zero(RExC_study_chunk_recursed, RExC_study_chunk_recursed_bytes, U8);
                    } else {
                        Copy(RExC_study_chunk_recursed + ((recursed_depth-1) * RExC_study_chunk_recursed_bytes),
                             RExC_study_chunk_recursed + (recursed_depth * RExC_study_chunk_recursed_bytes),
                             RExC_study_chunk_recursed_bytes, U8);
                    }
                    /* we havent recursed into this paren yet, so recurse into it */
                    DEBUG_STUDYDATA("gosub-set", data, depth, is_inf);
                    PAREN_SET(RExC_study_chunk_recursed + (recursed_depth * RExC_study_chunk_recursed_bytes), paren);
                    my_recursed_depth= recursed_depth + 1;
                } else {
                    DEBUG_STUDYDATA("gosub-inf", data, depth, is_inf);
                    /* some form of infinite recursion, assume infinite length
                     * */
                    if (flags & SCF_DO_SUBSTR) {
                        scan_commit(pRExC_state, data, minlenp, is_inf);
                        data->cur_is_floating = 1;
                    }
                    is_inf = is_inf_internal = 1;
                    if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
                        ssc_anything(data->start_class);
                    flags &= ~SCF_DO_STCLASS;

                    start= NULL; /* reset start so we dont recurse later on. */
	        }
            } else {
	        paren = stopparen;
                start = scan + 2;
	        end = regnext(scan);
	    }
            if (start) {
                scan_frame *newframe;
                assert(end);
                if (!RExC_frame_last) {
                    Newxz(newframe, 1, scan_frame);
                    SAVEDESTRUCTOR_X(S_unwind_scan_frames, newframe);
                    RExC_frame_head= newframe;
                    RExC_frame_count++;
                } else if (!RExC_frame_last->next_frame) {
                    Newxz(newframe, 1, scan_frame);
                    RExC_frame_last->next_frame= newframe;
                    newframe->prev_frame= RExC_frame_last;
                    RExC_frame_count++;
                } else {
                    newframe= RExC_frame_last->next_frame;
                }
                RExC_frame_last= newframe;

                newframe->next_regnode = regnext(scan);
                newframe->last_regnode = last;
                newframe->stopparen = stopparen;
                newframe->prev_recursed_depth = recursed_depth;
                newframe->this_prev_frame= frame;
                newframe->in_gosub = (
                    (frame && frame->in_gosub) || OP(scan) == GOSUB
                );

                DEBUG_STUDYDATA("frame-new", data, depth, is_inf);
                DEBUG_PEEP("fnew", scan, depth, flags);

	        frame = newframe;
	        scan =  start;
	        stopparen = paren;
	        last = end;
                depth = depth + 1;
                recursed_depth= my_recursed_depth;

	        continue;
	    }
	}
	else if (   OP(scan) == EXACT
                 || OP(scan) == EXACT_ONLY8
                 || OP(scan) == EXACTL)
        {
	    SSize_t l = STR_LEN(scan);
	    UV uc;
            assert(l);
	    if (UTF) {
		const U8 * const s = (U8*)STRING(scan);
		uc = utf8_to_uvchr_buf(s, s + l, NULL);
		l = utf8_length(s, s + l);
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
 			? SSize_t_MAX : data->pos_min + data->pos_delta;
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

            /* ANDing the code point leaves at most it, and not in locale, and
             * can't match null string */
	    if (flags & SCF_DO_STCLASS_AND) {
                ssc_cp_and(data->start_class, uc);
                ANYOF_FLAGS(data->start_class) &= ~SSC_MATCHES_EMPTY_STRING;
                ssc_clear_locale(data->start_class);
	    }
	    else if (flags & SCF_DO_STCLASS_OR) {
                ssc_add_cp(data->start_class, uc);
		ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);

                /* See commit msg 749e076fceedeb708a624933726e7989f2302f6a */
                ANYOF_FLAGS(data->start_class) &= ~SSC_MATCHES_EMPTY_STRING;
	    }
	    flags &= ~SCF_DO_STCLASS;
	}
        else if (PL_regkind[OP(scan)] == EXACT) {
            /* But OP != EXACT!, so is EXACTFish */
	    SSize_t l = STR_LEN(scan);
            const U8 * s = (U8*)STRING(scan);

	    /* Search for fixed substrings supports EXACT only. */
	    if (flags & SCF_DO_SUBSTR) {
		assert(data);
                scan_commit(pRExC_state, data, minlenp, is_inf);
	    }
	    if (UTF) {
		l = utf8_length(s, s + l);
	    }
	    if (unfolded_multi_char) {
                RExC_seen |= REG_UNFOLDED_MULTI_SEEN;
	    }
	    min += l - min_subtract;
            assert (min >= 0);
            delta += min_subtract;
	    if (flags & SCF_DO_SUBSTR) {
		data->pos_min += l - min_subtract;
		if (data->pos_min < 0) {
                    data->pos_min = 0;
                }
                data->pos_delta += min_subtract;
		if (min_subtract) {
		    data->cur_is_floating = 1; /* float */
		}
	    }

            if (flags & SCF_DO_STCLASS) {
                SV* EXACTF_invlist = _make_exactf_invlist(pRExC_state, scan);

                assert(EXACTF_invlist);
                if (flags & SCF_DO_STCLASS_AND) {
                    if (OP(scan) != EXACTFL)
                        ssc_clear_locale(data->start_class);
                    ANYOF_FLAGS(data->start_class) &= ~SSC_MATCHES_EMPTY_STRING;
                    ANYOF_POSIXL_ZERO(data->start_class);
                    ssc_intersection(data->start_class, EXACTF_invlist, FALSE);
                }
                else {  /* SCF_DO_STCLASS_OR */
                    ssc_union(data->start_class, EXACTF_invlist, FALSE);
                    ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);

                    /* See commit msg 749e076fceedeb708a624933726e7989f2302f6a */
                    ANYOF_FLAGS(data->start_class) &= ~SSC_MATCHES_EMPTY_STRING;
                }
                flags &= ~SCF_DO_STCLASS;
                SvREFCNT_dec(EXACTF_invlist);
            }
	}
	else if (REGNODE_VARIES(OP(scan))) {
	    SSize_t mincount, maxcount, minnext, deltanext, pos_before = 0;
	    I32 fl = 0, f = flags;
	    regnode * const oscan = scan;
	    regnode_ssc this_class;
	    regnode_ssc *oclass = NULL;
	    I32 next_is_eval = 0;

	    switch (PL_regkind[OP(scan)]) {
	    case WHILEM:		/* End of (?:...)* . */
		scan = NEXTOPER(scan);
		goto finish;
	    case PLUS:
		if (flags & (SCF_DO_SUBSTR | SCF_DO_STCLASS)) {
		    next = NEXTOPER(scan);
		    if (   OP(next) == EXACT
                        || OP(next) == EXACT_ONLY8
                        || OP(next) == EXACTL
                        || (flags & SCF_DO_STCLASS))
                    {
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
		/* FALLTHROUGH */
	    case STAR:
                next = NEXTOPER(scan);

                /* This temporary node can now be turned into EXACTFU, and
                 * must, as regexec.c doesn't handle it */
                if (OP(next) == EXACTFU_S_EDGE && mutate_ok) {
                    OP(next) = EXACTFU;
                }

                if (     STR_LEN(next) == 1
                    &&   isALPHA_A(* STRING(next))
                    && (         OP(next) == EXACTFAA
                        || (     OP(next) == EXACTFU
                            && ! HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(* STRING(next))))
                    &&   mutate_ok
                ) {
                    /* These differ in just one bit */
                    U8 mask = ~ ('A' ^ 'a');

                    assert(isALPHA_A(* STRING(next)));

                    /* Then replace it by an ANYOFM node, with
                    * the mask set to the complement of the
                    * bit that differs between upper and lower
                    * case, and the lowest code point of the
                    * pair (which the '&' forces) */
                    OP(next) = ANYOFM;
                    ARG_SET(next, *STRING(next) & mask);
                    FLAGS(next) = mask;
                }

		if (flags & SCF_DO_STCLASS) {
		    mincount = 0;
		    maxcount = REG_INFTY;
		    next = regnext(scan);
		    scan = NEXTOPER(scan);
		    goto do_curly;
		}
		if (flags & SCF_DO_SUBSTR) {
                    scan_commit(pRExC_state, data, minlenp, is_inf);
                    /* Cannot extend fixed substrings */
		    data->cur_is_floating = 1; /* float */
		}
                is_inf = is_inf_internal = 1;
                scan = regnext(scan);
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
                    if (mincount == 0)
                        scan_commit(pRExC_state, data, minlenp, is_inf);
                    /* Cannot extend fixed substrings */
		    pos_before = data->pos_min;
		}
		if (data) {
		    fl = data->flags;
		    data->flags &= ~(SF_HAS_PAR|SF_IN_PAR|SF_HAS_EVAL);
		    if (is_inf)
			data->flags |= SF_IS_INF;
		}
		if (flags & SCF_DO_STCLASS) {
		    ssc_init(pRExC_state, &this_class);
		    oclass = data->start_class;
		    data->start_class = &this_class;
		    f |= SCF_DO_STCLASS_AND;
		    f &= ~SCF_DO_STCLASS_OR;
		}
	        /* Exclude from super-linear cache processing any {n,m}
		   regops for which the combination of input pos and regex
		   pos is not enough information to determine if a match
		   will be possible.

		   For example, in the regex /foo(bar\s*){4,8}baz/ with the
		   regex pos at the \s*, the prospects for a match depend not
		   only on the input position but also on how many (bar\s*)
		   repeats into the {4,8} we are. */
               if ((mincount > 1) || (maxcount > 1 && maxcount != REG_INFTY))
		    f &= ~SCF_WHILEM_VISITED_POS;

		/* This will finish on WHILEM, setting scan, or on NULL: */
                /* recurse study_chunk() on loop bodies */
		minnext = study_chunk(pRExC_state, &scan, minlenp, &deltanext,
                                  last, data, stopparen, recursed_depth, NULL,
                                  (mincount == 0
                                   ? (f & ~SCF_DO_SUBSTR)
                                   : f)
                                  , depth+1, mutate_ok);

		if (flags & SCF_DO_STCLASS)
		    data->start_class = oclass;
		if (mincount == 0 || minnext == 0) {
		    if (flags & SCF_DO_STCLASS_OR) {
			ssc_or(pRExC_state, data->start_class, (regnode_charclass *) &this_class);
		    }
		    else if (flags & SCF_DO_STCLASS_AND) {
			/* Switch to OR mode: cache the old value of
			 * data->start_class */
			INIT_AND_WITHP;
			StructCopy(data->start_class, and_withp, regnode_ssc);
			flags &= ~SCF_DO_STCLASS_AND;
			StructCopy(&this_class, data->start_class, regnode_ssc);
			flags |= SCF_DO_STCLASS_OR;
                        ANYOF_FLAGS(data->start_class)
                                                |= SSC_MATCHES_EMPTY_STRING;
		    }
		} else {		/* Non-zero len */
		    if (flags & SCF_DO_STCLASS_OR) {
			ssc_or(pRExC_state, data->start_class, (regnode_charclass *) &this_class);
			ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);
		    }
		    else if (flags & SCF_DO_STCLASS_AND)
			ssc_and(pRExC_state, data->start_class, (regnode_charclass *) &this_class);
		    flags &= ~SCF_DO_STCLASS;
		}
		if (!scan) 		/* It was not CURLYX, but CURLY. */
		    scan = next;
		if (((flags & (SCF_TRIE_DOING_RESTUDY|SCF_DO_SUBSTR))==SCF_DO_SUBSTR)
		    /* ? quantifier ok, except for (?{ ... }) */
		    && (next_is_eval || !(mincount == 0 && maxcount == 1))
		    && (minnext == 0) && (deltanext == 0)
		    && data && !(data->flags & (SF_HAS_PAR|SF_IN_PAR))
                    && maxcount <= REG_INFTY/3) /* Complement check for big
                                                   count */
		{
		    _WARN_HELPER(RExC_precomp_end, packWARN(WARN_REGEXP),
                        Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),
                            "Quantifier unexpected on zero-length expression "
                            "in regex m/%" UTF8f "/",
			     UTF8fARG(UTF, RExC_precomp_end - RExC_precomp,
				  RExC_precomp)));
                }

                if ( ( minnext > 0 && mincount >= SSize_t_MAX / minnext )
                    || min >= SSize_t_MAX - minnext * mincount )
                {
                    FAIL("Regexp out of space");
                }

		min += minnext * mincount;
		is_inf_internal |= deltanext == SSize_t_MAX
                         || (maxcount == REG_INFTY && minnext + deltanext > 0);
		is_inf |= is_inf_internal;
                if (is_inf) {
		    delta = SSize_t_MAX;
                } else {
		    delta += (minnext + deltanext) * maxcount
                             - minnext * mincount;
                }
		/* Try powerful optimization CURLYX => CURLYN. */
		if (  OP(oscan) == CURLYX && data
		      && data->flags & SF_IN_PAR
		      && !(data->flags & SF_HAS_EVAL)
		      && !deltanext && minnext == 1
                      && mutate_ok
                ) {
		    /* Try to optimize to CURLYN.  */
		    regnode *nxt = NEXTOPER(oscan) + EXTRA_STEP_2ARGS;
		    regnode * const nxt1 = nxt;
#ifdef DEBUGGING
		    regnode *nxt2;
#endif

		    /* Skip open. */
		    nxt = regnext(nxt);
		    if (!REGNODE_SIMPLE(OP(nxt))
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

                        /*open->CURLYM*/
                        RExC_open_parens[ARG(nxt1)] = REGNODE_OFFSET(oscan);

                        /*close->while*/
                        RExC_close_parens[ARG(nxt1)] = REGNODE_OFFSET(nxt) + 2;
		    }
		    /* Now we know that nxt2 is the only contents: */
		    oscan->flags = (U8)ARG(nxt);
		    OP(oscan) = CURLYN;
		    OP(nxt1) = NOTHING;	/* was OPEN. */

#ifdef DEBUGGING
		    OP(nxt1 + 1) = OPTIMIZED; /* was count. */
		    NEXT_OFF(nxt1+ 1) = 0; /* just for consistency. */
		    NEXT_OFF(nxt2) = 0;	/* just for consistency with CURLY. */
		    OP(nxt) = OPTIMIZED;	/* was CLOSE. */
		    OP(nxt + 1) = OPTIMIZED; /* was count. */
		    NEXT_OFF(nxt+ 1) = 0; /* just for consistency. */
#endif
		}
	      nogo:

		/* Try optimization CURLYX => CURLYM. */
		if (  OP(oscan) == CURLYX && data
		      && !(data->flags & SF_HAS_PAR)
		      && !(data->flags & SF_HAS_EVAL)
		      && !deltanext	/* atom is fixed width */
		      && minnext != 0	/* CURLYM can't handle zero width */
                         /* Nor characters whose fold at run-time may be
                          * multi-character */
                      && ! (RExC_seen & REG_UNFOLDED_MULTI_SEEN)
                      && mutate_ok
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
		    if ((data->flags & SF_IN_PAR) && OP(nxt) == CLOSE) {
			/* Set the parenth number.  */
			regnode *nxt1 = NEXTOPER(oscan) + EXTRA_STEP_2ARGS; /* OPEN*/

			oscan->flags = (U8)ARG(nxt);
			if (RExC_open_parens) {
                             /*open->CURLYM*/
                            RExC_open_parens[ARG(nxt1)] = REGNODE_OFFSET(oscan);

                            /*close->NOTHING*/
                            RExC_close_parens[ARG(nxt1)] = REGNODE_OFFSET(nxt2)
                                                         + 1;
			}
			OP(nxt1) = OPTIMIZED;	/* was OPEN. */
			OP(nxt) = OPTIMIZED;	/* was CLOSE. */

#ifdef DEBUGGING
			OP(nxt1 + 1) = OPTIMIZED; /* was count. */
			OP(nxt + 1) = OPTIMIZED; /* was count. */
			NEXT_OFF(nxt1 + 1) = 0; /* just for consistency. */
			NEXT_OFF(nxt + 1) = 0; /* just for consistency. */
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
                        /* recurse study_chunk() on optimised CURLYX => CURLYM */
			study_chunk(pRExC_state, &nxt1, minlenp, &deltanext, nxt,
                                    NULL, stopparen, recursed_depth, NULL, 0,
                                    depth+1, mutate_ok);
		    }
		    else
			oscan->flags = 0;
		}
		else if ((OP(oscan) == CURLYX)
			 && (flags & SCF_WHILEM_VISITED_POS)
			 /* See the comment on a similar expression above.
			    However, this time it's not a subexpression
			    we care about, but the expression itself. */
			 && (maxcount == REG_INFTY)
			 && data) {
		    /* This stays as CURLYX, we can put the count/of pair. */
		    /* Find WHILEM (as in regexec.c) */
		    regnode *nxt = oscan + NEXT_OFF(oscan);

		    if (OP(PREVOPER(nxt)) == NOTHING) /* LONGJMP */
			nxt += ARG(nxt);
                    nxt = PREVOPER(nxt);
                    if (nxt->flags & 0xf) {
                        /* we've already set whilem count on this node */
                    } else if (++data->whilem_c < 16) {
                        assert(data->whilem_c <= RExC_whilem_seen);
                        nxt->flags = (U8)(data->whilem_c
                            | (RExC_whilem_seen << 4)); /* On WHILEM */
                    }
		}
		if (data && fl & (SF_HAS_PAR|SF_IN_PAR))
		    pars++;
		if (flags & SCF_DO_SUBSTR) {
		    SV *last_str = NULL;
                    STRLEN last_chrs = 0;
		    int counted = mincount != 0;

                    if (data->last_end > 0 && mincount != 0) { /* Ends with a
                                                                  string. */
			SSize_t b = pos_before >= data->last_start_min
			    ? pos_before : data->last_start_min;
			STRLEN l;
			const char * const s = SvPV_const(data->last_found, l);
			SSize_t old = b - data->last_start_min;
                        assert(old >= 0);

			if (UTF)
			    old = utf8_hop_forward((U8*)s, old,
                                               (U8 *) SvEND(data->last_found))
                                - (U8*)s;
			l -= old;
			/* Get the added string: */
			last_str = newSVpvn_utf8(s  + old, l, UTF);
                        last_chrs = UTF ? utf8_length((U8*)(s + old),
                                            (U8*)(s + old + l)) : l;
			if (deltanext == 0 && pos_before == b) {
			    /* What was added is a constant string */
			    if (mincount > 1) {

				SvGROW(last_str, (mincount * l) + 1);
				repeatcpy(SvPVX(last_str) + l,
					  SvPVX_const(last_str), l,
                                          mincount - 1);
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
					mg->mg_len += last_chrs * (mincount-1);
				}
                                last_chrs *= mincount;
				data->last_end += l * (mincount - 1);
			    }
			} else {
			    /* start offset must point into the last copy */
			    data->last_start_min += minnext * (mincount - 1);
			    data->last_start_max =
                              is_inf
                               ? SSize_t_MAX
			       : data->last_start_max +
                                 (maxcount - 1) * (minnext + data->pos_delta);
			}
		    }
		    /* It is counted once already... */
		    data->pos_min += minnext * (mincount - counted);
#if 0
Perl_re_printf( aTHX_  "counted=%" UVuf " deltanext=%" UVuf
                              " SSize_t_MAX=%" UVuf " minnext=%" UVuf
                              " maxcount=%" UVuf " mincount=%" UVuf "\n",
    (UV)counted, (UV)deltanext, (UV)SSize_t_MAX, (UV)minnext, (UV)maxcount,
    (UV)mincount);
if (deltanext != SSize_t_MAX)
Perl_re_printf( aTHX_  "LHS=%" UVuf " RHS=%" UVuf "\n",
    (UV)(-counted * deltanext + (minnext + deltanext) * maxcount
          - minnext * mincount), (UV)(SSize_t_MAX - data->pos_delta));
#endif
		    if (deltanext == SSize_t_MAX
                        || -counted * deltanext + (minnext + deltanext) * maxcount - minnext * mincount >= SSize_t_MAX - data->pos_delta)
		        data->pos_delta = SSize_t_MAX;
		    else
		        data->pos_delta += - counted * deltanext +
			(minnext + deltanext) * maxcount - minnext * mincount;
		    if (mincount != maxcount) {
			 /* Cannot extend fixed substrings found inside
			    the group.  */
                        scan_commit(pRExC_state, data, minlenp, is_inf);
			if (mincount && last_str) {
			    SV * const sv = data->last_found;
			    MAGIC * const mg = SvUTF8(sv) && SvMAGICAL(sv) ?
				mg_find(sv, PERL_MAGIC_utf8) : NULL;

			    if (mg)
				mg->mg_len = -1;
			    sv_setsv(sv, last_str);
			    data->last_end = data->pos_min;
			    data->last_start_min = data->pos_min - last_chrs;
			    data->last_start_max = is_inf
				? SSize_t_MAX
				: data->pos_min + data->pos_delta - last_chrs;
			}
			data->cur_is_floating = 1; /* float */
		    }
		    SvREFCNT_dec(last_str);
		}
		if (data && (fl & SF_HAS_EVAL))
		    data->flags |= SF_HAS_EVAL;
	      optimize_curly_tail:
		rck_elide_nothing(oscan);
		continue;

	    default:
#ifdef DEBUGGING
                Perl_croak(aTHX_ "panic: unexpected varying REx opcode %d",
                                                                    OP(scan));
#endif
            case REF:
            case CLUMP:
		if (flags & SCF_DO_SUBSTR) {
                    /* Cannot expect anything... */
                    scan_commit(pRExC_state, data, minlenp, is_inf);
		    data->cur_is_floating = 1; /* float */
		}
		is_inf = is_inf_internal = 1;
		if (flags & SCF_DO_STCLASS_OR) {
                    if (OP(scan) == CLUMP) {
                        /* Actually is any start char, but very few code points
                         * aren't start characters */
                        ssc_match_all_cp(data->start_class);
                    }
                    else {
                        ssc_anything(data->start_class);
                    }
                }
		flags &= ~SCF_DO_STCLASS;
		break;
	    }
	}
	else if (OP(scan) == LNBREAK) {
	    if (flags & SCF_DO_STCLASS) {
    	        if (flags & SCF_DO_STCLASS_AND) {
                    ssc_intersection(data->start_class,
                                    PL_XPosix_ptrs[_CC_VERTSPACE], FALSE);
                    ssc_clear_locale(data->start_class);
                    ANYOF_FLAGS(data->start_class)
                                                &= ~SSC_MATCHES_EMPTY_STRING;
                }
                else if (flags & SCF_DO_STCLASS_OR) {
                    ssc_union(data->start_class,
                              PL_XPosix_ptrs[_CC_VERTSPACE],
                              FALSE);
		    ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);

                    /* See commit msg for
                     * 749e076fceedeb708a624933726e7989f2302f6a */
                    ANYOF_FLAGS(data->start_class)
                                                &= ~SSC_MATCHES_EMPTY_STRING;
                }
		flags &= ~SCF_DO_STCLASS;
            }
	    min++;
            if (delta != SSize_t_MAX)
                delta++;    /* Because of the 2 char string cr-lf */
            if (flags & SCF_DO_SUBSTR) {
                /* Cannot expect anything... */
                scan_commit(pRExC_state, data, minlenp, is_inf);
    	        data->pos_min += 1;
                if (data->pos_delta != SSize_t_MAX) {
                    data->pos_delta += 1;
                }
		data->cur_is_floating = 1; /* float */
    	    }
	}
	else if (REGNODE_SIMPLE(OP(scan))) {

	    if (flags & SCF_DO_SUBSTR) {
                scan_commit(pRExC_state, data, minlenp, is_inf);
		data->pos_min++;
	    }
	    min++;
	    if (flags & SCF_DO_STCLASS) {
                bool invert = 0;
                SV* my_invlist = NULL;
                U8 namedclass;

                /* See commit msg 749e076fceedeb708a624933726e7989f2302f6a */
                ANYOF_FLAGS(data->start_class) &= ~SSC_MATCHES_EMPTY_STRING;

		/* Some of the logic below assumes that switching
		   locale on will only add false positives. */
		switch (OP(scan)) {

		default:
#ifdef DEBUGGING
                   Perl_croak(aTHX_ "panic: unexpected simple REx opcode %d",
                                                                     OP(scan));
#endif
		case SANY:
		    if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
			ssc_match_all_cp(data->start_class);
		    break;

		case REG_ANY:
                    {
                        SV* REG_ANY_invlist = _new_invlist(2);
                        REG_ANY_invlist = add_cp_to_invlist(REG_ANY_invlist,
                                                            '\n');
                        if (flags & SCF_DO_STCLASS_OR) {
                            ssc_union(data->start_class,
                                      REG_ANY_invlist,
                                      TRUE /* TRUE => invert, hence all but \n
                                            */
                                      );
                        }
                        else if (flags & SCF_DO_STCLASS_AND) {
                            ssc_intersection(data->start_class,
                                             REG_ANY_invlist,
                                             TRUE  /* TRUE => invert */
                                             );
                            ssc_clear_locale(data->start_class);
                        }
                        SvREFCNT_dec_NN(REG_ANY_invlist);
		    }
		    break;

                case ANYOFD:
                case ANYOFL:
                case ANYOFPOSIXL:
                case ANYOFH:
                case ANYOF:
		    if (flags & SCF_DO_STCLASS_AND)
			ssc_and(pRExC_state, data->start_class,
                                (regnode_charclass *) scan);
		    else
			ssc_or(pRExC_state, data->start_class,
                                                          (regnode_charclass *) scan);
		    break;

                case NANYOFM:
                case ANYOFM:
                  {
                    SV* cp_list = get_ANYOFM_contents(scan);

                    if (flags & SCF_DO_STCLASS_OR) {
                        ssc_union(data->start_class, cp_list, invert);
                    }
                    else if (flags & SCF_DO_STCLASS_AND) {
                        ssc_intersection(data->start_class, cp_list, invert);
                    }

                    SvREFCNT_dec_NN(cp_list);
                    break;
                  }

		case NPOSIXL:
                    invert = 1;
                    /* FALLTHROUGH */

		case POSIXL:
                    namedclass = classnum_to_namedclass(FLAGS(scan)) + invert;
                    if (flags & SCF_DO_STCLASS_AND) {
                        bool was_there = cBOOL(
                                          ANYOF_POSIXL_TEST(data->start_class,
                                                                 namedclass));
                        ANYOF_POSIXL_ZERO(data->start_class);
                        if (was_there) {    /* Do an AND */
                            ANYOF_POSIXL_SET(data->start_class, namedclass);
                        }
                        /* No individual code points can now match */
                        data->start_class->invlist
                                                = sv_2mortal(_new_invlist(0));
                    }
                    else {
                        int complement = namedclass + ((invert) ? -1 : 1);

                        assert(flags & SCF_DO_STCLASS_OR);

                        /* If the complement of this class was already there,
                         * the result is that they match all code points,
                         * (\d + \D == everything).  Remove the classes from
                         * future consideration.  Locale is not relevant in
                         * this case */
                        if (ANYOF_POSIXL_TEST(data->start_class, complement)) {
                            ssc_match_all_cp(data->start_class);
                            ANYOF_POSIXL_CLEAR(data->start_class, namedclass);
                            ANYOF_POSIXL_CLEAR(data->start_class, complement);
                        }
                        else {  /* The usual case; just add this class to the
                                   existing set */
                            ANYOF_POSIXL_SET(data->start_class, namedclass);
                        }
                    }
                    break;

                case NPOSIXA:   /* For these, we always know the exact set of
                                   what's matched */
                    invert = 1;
                    /* FALLTHROUGH */
		case POSIXA:
                    my_invlist = invlist_clone(PL_Posix_ptrs[FLAGS(scan)], NULL);
                    goto join_posix_and_ascii;

		case NPOSIXD:
		case NPOSIXU:
                    invert = 1;
                    /* FALLTHROUGH */
		case POSIXD:
		case POSIXU:
                    my_invlist = invlist_clone(PL_XPosix_ptrs[FLAGS(scan)], NULL);

                    /* NPOSIXD matches all upper Latin1 code points unless the
                     * target string being matched is UTF-8, which is
                     * unknowable until match time.  Since we are going to
                     * invert, we want to get rid of all of them so that the
                     * inversion will match all */
                    if (OP(scan) == NPOSIXD) {
                        _invlist_subtract(my_invlist, PL_UpperLatin1,
                                          &my_invlist);
                    }

                  join_posix_and_ascii:

                    if (flags & SCF_DO_STCLASS_AND) {
                        ssc_intersection(data->start_class, my_invlist, invert);
                        ssc_clear_locale(data->start_class);
                    }
                    else {
                        assert(flags & SCF_DO_STCLASS_OR);
                        ssc_union(data->start_class, my_invlist, invert);
                    }
                    SvREFCNT_dec(my_invlist);
		}
		if (flags & SCF_DO_STCLASS_OR)
		    ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);
		flags &= ~SCF_DO_STCLASS;
	    }
	}
	else if (PL_regkind[OP(scan)] == EOL && flags & SCF_DO_SUBSTR) {
	    data->flags |= (OP(scan) == MEOL
			    ? SF_BEFORE_MEOL
			    : SF_BEFORE_SEOL);
            scan_commit(pRExC_state, data, minlenp, is_inf);

	}
	else if (  PL_regkind[OP(scan)] == BRANCHJ
		 /* Lookbehind, or need to calculate parens/evals/stclass: */
		   && (scan->flags || data || (flags & SCF_DO_STCLASS))
		   && (OP(scan) == IFMATCH || OP(scan) == UNLESSM))
        {
            if ( !PERL_ENABLE_POSITIVE_ASSERTION_STUDY
                || OP(scan) == UNLESSM )
            {
                /* Negative Lookahead/lookbehind
                   In this case we can't do fixed string optimisation.
                */

                SSize_t deltanext, minnext, fake = 0;
                regnode *nscan;
                regnode_ssc intrnl;
                int f = 0;

                StructCopy(&zero_scan_data, &data_fake, scan_data_t);
                if (data) {
                    data_fake.whilem_c = data->whilem_c;
                    data_fake.last_closep = data->last_closep;
		}
                else
                    data_fake.last_closep = &fake;
		data_fake.pos_delta = delta;
                if ( flags & SCF_DO_STCLASS && !scan->flags
                     && OP(scan) == IFMATCH ) { /* Lookahead */
                    ssc_init(pRExC_state, &intrnl);
                    data_fake.start_class = &intrnl;
                    f |= SCF_DO_STCLASS_AND;
		}
                if (flags & SCF_WHILEM_VISITED_POS)
                    f |= SCF_WHILEM_VISITED_POS;
                next = regnext(scan);
                nscan = NEXTOPER(NEXTOPER(scan));

                /* recurse study_chunk() for lookahead body */
                minnext = study_chunk(pRExC_state, &nscan, minlenp, &deltanext,
                                      last, &data_fake, stopparen,
                                      recursed_depth, NULL, f, depth+1,
                                      mutate_ok);
                if (scan->flags) {
                    if (   deltanext < 0
                        || deltanext > (I32) U8_MAX
                        || minnext > (I32)U8_MAX
                        || minnext + deltanext > (I32)U8_MAX)
                    {
			FAIL2("Lookbehind longer than %" UVuf " not implemented",
                              (UV)U8_MAX);
                    }

                    /* The 'next_off' field has been repurposed to count the
                     * additional starting positions to try beyond the initial
                     * one.  (This leaves it at 0 for non-variable length
                     * matches to avoid breakage for those not using this
                     * extension) */
                    if (deltanext) {
                        scan->next_off = deltanext;
                        ckWARNexperimental(RExC_parse,
                            WARN_EXPERIMENTAL__VLB,
                            "Variable length lookbehind is experimental");
                    }
                    scan->flags = (U8)minnext + deltanext;
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
			ssc_init(pRExC_state, data->start_class);
		    }  else {
                        /* AND before and after: combine and continue.  These
                         * assertions are zero-length, so can match an EMPTY
                         * string */
			ssc_and(pRExC_state, data->start_class, (regnode_charclass *) &intrnl);
                        ANYOF_FLAGS(data->start_class)
                                                   |= SSC_MATCHES_EMPTY_STRING;
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
                SSize_t deltanext, fake = 0;
                regnode *nscan;
                regnode_ssc intrnl;
                int f = 0;
                /* We use SAVEFREEPV so that when the full compile
                    is finished perl will clean up the allocated
                    minlens when it's all done. This way we don't
                    have to worry about freeing them when we know
                    they wont be used, which would be a pain.
                 */
                SSize_t *minnextp;
                Newx( minnextp, 1, SSize_t );
                SAVEFREEPV(minnextp);

                if (data) {
                    StructCopy(data, &data_fake, scan_data_t);
                    if ((flags & SCF_DO_SUBSTR) && data->last_found) {
                        f |= SCF_DO_SUBSTR;
                        if (scan->flags)
                            scan_commit(pRExC_state, &data_fake, minlenp, is_inf);
                        data_fake.last_found=newSVsv(data->last_found);
                    }
                }
                else
                    data_fake.last_closep = &fake;
                data_fake.flags = 0;
                data_fake.substrs[0].flags = 0;
                data_fake.substrs[1].flags = 0;
		data_fake.pos_delta = delta;
                if (is_inf)
	            data_fake.flags |= SF_IS_INF;
                if ( flags & SCF_DO_STCLASS && !scan->flags
                     && OP(scan) == IFMATCH ) { /* Lookahead */
                    ssc_init(pRExC_state, &intrnl);
                    data_fake.start_class = &intrnl;
                    f |= SCF_DO_STCLASS_AND;
                }
                if (flags & SCF_WHILEM_VISITED_POS)
                    f |= SCF_WHILEM_VISITED_POS;
                next = regnext(scan);
                nscan = NEXTOPER(NEXTOPER(scan));

                /* positive lookahead study_chunk() recursion */
                *minnextp = study_chunk(pRExC_state, &nscan, minnextp,
                                        &deltanext, last, &data_fake,
                                        stopparen, recursed_depth, NULL,
                                        f, depth+1, mutate_ok);
                if (scan->flags) {
                    assert(0);  /* This code has never been tested since this
                                   is normally not compiled */
                    if (   deltanext < 0
                        || deltanext > (I32) U8_MAX
                        || *minnextp > (I32)U8_MAX
                        || *minnextp + deltanext > (I32)U8_MAX)
                    {
			FAIL2("Lookbehind longer than %" UVuf " not implemented",
                              (UV)U8_MAX);
                    }

                    if (deltanext) {
                        scan->next_off = deltanext;
                    }
                    scan->flags = (U8)*minnextp + deltanext;
                }

                *minnextp += min;

                if (f & SCF_DO_STCLASS_AND) {
                    ssc_and(pRExC_state, data->start_class, (regnode_charclass *) &intrnl);
                    ANYOF_FLAGS(data->start_class) |= SSC_MATCHES_EMPTY_STRING;
                }
                if (data) {
                    if (data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
                        pars++;
                    if (data_fake.flags & SF_HAS_EVAL)
                        data->flags |= SF_HAS_EVAL;
                    data->whilem_c = data_fake.whilem_c;
                    if ((flags & SCF_DO_SUBSTR) && data_fake.last_found) {
                        int i;
                        if (RExC_rx->minlen<*minnextp)
                            RExC_rx->minlen=*minnextp;
                        scan_commit(pRExC_state, &data_fake, minnextp, is_inf);
                        SvREFCNT_dec_NN(data_fake.last_found);

                        for (i = 0; i < 2; i++) {
                            if (data_fake.substrs[i].minlenp != minlenp) {
                                data->substrs[i].min_offset =
                                            data_fake.substrs[i].min_offset;
                                data->substrs[i].max_offset =
                                            data_fake.substrs[i].max_offset;
                                data->substrs[i].minlenp =
                                            data_fake.substrs[i].minlenp;
                                data->substrs[i].lookbehind += scan->flags;
                            }
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
                scan_commit(pRExC_state, data, minlenp, is_inf);
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
                    scan_commit(pRExC_state, data, minlenp, is_inf);
		    data->cur_is_floating = 1; /* float */
		}
		is_inf = is_inf_internal = 1;
		if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
		    ssc_anything(data->start_class);
		flags &= ~SCF_DO_STCLASS;
	}
	else if (OP(scan) == GPOS) {
            if (!(RExC_rx->intflags & PREGf_GPOS_FLOAT) &&
	        !(delta || is_inf || (data && data->pos_delta)))
	    {
                if (!(RExC_rx->intflags & PREGf_ANCH) && (flags & SCF_DO_SUBSTR))
                    RExC_rx->intflags |= PREGf_ANCH_GPOS;
	        if (RExC_rx->gofs < (STRLEN)min)
		    RExC_rx->gofs = min;
            } else {
                RExC_rx->intflags |= PREGf_GPOS_FLOAT;
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
            SSize_t max1 = 0, min1 = SSize_t_MAX;
            regnode_ssc accum;

            if (flags & SCF_DO_SUBSTR) { /* XXXX Add !SUSPEND? */
                /* Cannot merge strings after this. */
                scan_commit(pRExC_state, data, minlenp, is_inf);
            }
            if (flags & SCF_DO_STCLASS)
                ssc_init_zero(pRExC_state, &accum);

            if (!trie->jump) {
                min1= trie->minlen;
                max1= trie->maxlen;
            } else {
                const regnode *nextbranch= NULL;
                U32 word;

                for ( word=1 ; word <= trie->wordcount ; word++)
                {
                    SSize_t deltanext=0, minnext=0, f = 0, fake;
                    regnode_ssc this_class;

                    StructCopy(&zero_scan_data, &data_fake, scan_data_t);
                    if (data) {
                        data_fake.whilem_c = data->whilem_c;
                        data_fake.last_closep = data->last_closep;
                    }
                    else
                        data_fake.last_closep = &fake;
		    data_fake.pos_delta = delta;
                    if (flags & SCF_DO_STCLASS) {
                        ssc_init(pRExC_state, &this_class);
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
                           it. Note this means we need the vestigal unused
                           branches even though they arent otherwise used. */
                        /* optimise study_chunk() for TRIE */
                        minnext = study_chunk(pRExC_state, &scan, minlenp,
                            &deltanext, (regnode *)nextbranch, &data_fake,
                            stopparen, recursed_depth, NULL, f, depth+1,
                            mutate_ok);
                    }
                    if (nextbranch && PL_regkind[OP(nextbranch)]==BRANCH)
                        nextbranch= regnext((regnode*)nextbranch);

                    if (min1 > (SSize_t)(minnext + trie->minlen))
                        min1 = minnext + trie->minlen;
                    if (deltanext == SSize_t_MAX) {
                        is_inf = is_inf_internal = 1;
                        max1 = SSize_t_MAX;
                    } else if (max1 < (SSize_t)(minnext + deltanext + trie->maxlen))
                        max1 = minnext + deltanext + trie->maxlen;

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
                        ssc_or(pRExC_state, &accum, (regnode_charclass *) &this_class);
                }
            }
            if (flags & SCF_DO_SUBSTR) {
                data->pos_min += min1;
                data->pos_delta += max1 - min1;
                if (max1 != min1 || is_inf)
                    data->cur_is_floating = 1; /* float */
            }
            min += min1;
            if (delta != SSize_t_MAX) {
                if (SSize_t_MAX - (max1 - min1) >= delta)
                    delta += max1 - min1;
                else
                    delta = SSize_t_MAX;
            }
            if (flags & SCF_DO_STCLASS_OR) {
                ssc_or(pRExC_state, data->start_class, (regnode_charclass *) &accum);
                if (min1) {
                    ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);
                    flags &= ~SCF_DO_STCLASS;
                }
            }
            else if (flags & SCF_DO_STCLASS_AND) {
                if (min1) {
                    ssc_and(pRExC_state, data->start_class, (regnode_charclass *) &accum);
                    flags &= ~SCF_DO_STCLASS;
                }
                else {
                    /* Switch to OR mode: cache the old value of
                     * data->start_class */
		    INIT_AND_WITHP;
                    StructCopy(data->start_class, and_withp, regnode_ssc);
                    flags &= ~SCF_DO_STCLASS_AND;
                    StructCopy(&accum, data->start_class, regnode_ssc);
                    flags |= SCF_DO_STCLASS_OR;
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
                /* Cannot expect anything... */
                scan_commit(pRExC_state, data, minlenp, is_inf);
    	        data->pos_min += trie->minlen;
    	        data->pos_delta += (trie->maxlen - trie->minlen);
		if (trie->maxlen != trie->minlen)
		    data->cur_is_floating = 1; /* float */
    	    }
    	    if (trie->jump) /* no more substrings -- for now /grr*/
               flags &= ~SCF_DO_SUBSTR;
	}
#endif /* old or new */
#endif /* TRIE_STUDY_OPT */

	/* Else: zero-length, ignore. */
	scan = regnext(scan);
    }

  finish:
    if (frame) {
        /* we need to unwind recursion. */
        depth = depth - 1;

        DEBUG_STUDYDATA("frame-end", data, depth, is_inf);
        DEBUG_PEEP("fend", scan, depth, flags);

        /* restore previous context */
        last = frame->last_regnode;
        scan = frame->next_regnode;
        stopparen = frame->stopparen;
        recursed_depth = frame->prev_recursed_depth;

        RExC_frame_last = frame->prev_frame;
        frame = frame->this_prev_frame;
        goto fake_study_recurse;
    }

    assert(!frame);
    DEBUG_STUDYDATA("pre-fin", data, depth, is_inf);

    *scanp = scan;
    *deltap = is_inf_internal ? SSize_t_MAX : delta;

    if (flags & SCF_DO_SUBSTR && is_inf)
	data->pos_delta = SSize_t_MAX - data->pos_min;
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
	ssc_and(pRExC_state, data->start_class, (regnode_charclass *) and_withp);
    if (flags & SCF_TRIE_RESTUDY)
        data->flags |= 	SCF_TRIE_RESTUDY;

    DEBUG_STUDYDATA("post-fin", data, depth, is_inf);

    {
        SSize_t final_minlen= min < stopmin ? min : stopmin;

        if (!(RExC_seen & REG_UNBOUNDED_QUANTIFIER_SEEN)) {
            if (final_minlen > SSize_t_MAX - delta)
                RExC_maxlen = SSize_t_MAX;
            else if (RExC_maxlen < final_minlen + delta)
                RExC_maxlen = final_minlen + delta;
        }
        return final_minlen;
    }
    NOT_REACHED; /* NOTREACHED */
}

STATIC U32
S_add_data(RExC_state_t* const pRExC_state, const char* const s, const U32 n)
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

/*XXX: todo make this not included in a non debugging perl, but appears to be
 * used anyway there, in 'use re' */
#ifndef PERL_IN_XSUB_RE
void
Perl_reginitcolors(pTHX)
{
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
#define CHECK_RESTUDY_GOTO_butfirst(dOsomething)            \
    STMT_START {                                            \
        if (                                                \
              (data.flags & SCF_TRIE_RESTUDY)               \
              && ! restudied++                              \
        ) {                                                 \
            dOsomething;                                    \
            goto reStudy;                                   \
        }                                                   \
    } STMT_END
#else
#define CHECK_RESTUDY_GOTO_butfirst
#endif

/*
 * pregcomp - compile a regular expression into internal code
 *
 * Decides which engine's compiler to call based on the hint currently in
 * scope
 */

#ifndef PERL_IN_XSUB_RE

/* return the currently in-scope regex engine (or the default if none)  */

regexp_engine const *
Perl_current_re_engine(pTHX)
{
    if (IN_PERL_COMPILETIME) {
	HV * const table = GvHV(PL_hintgv);
	SV **ptr;

	if (!table || !(PL_hints & HINT_LOCALIZE_HH))
	    return &PL_core_reg_engine;
	ptr = hv_fetchs(table, "regcomp", FALSE);
	if ( !(ptr && SvIOK(*ptr) && SvIV(*ptr)))
	    return &PL_core_reg_engine;
	return INT2PTR(regexp_engine*, SvIV(*ptr));
    }
    else {
	SV *ptr;
	if (!PL_curcop->cop_hints_hash)
	    return &PL_core_reg_engine;
	ptr = cop_hints_fetch_pvs(PL_curcop, "regcomp", 0);
	if ( !(ptr && SvIOK(ptr) && SvIV(ptr)))
	    return &PL_core_reg_engine;
	return INT2PTR(regexp_engine*, SvIV(ptr));
    }
}


REGEXP *
Perl_pregcomp(pTHX_ SV * const pattern, const U32 flags)
{
    regexp_engine const *eng = current_re_engine();
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_PREGCOMP;

    /* Dispatch a request to compile a regexp to correct regexp engine. */
    DEBUG_COMPILE_r({
        Perl_re_printf( aTHX_  "Using engine %" UVxf "\n",
			PTR2UV(eng));
    });
    return CALLREGCOMP_ENG(eng, pattern, flags);
}
#endif

/* public(ish) entry point for the perl core's own regex compiling code.
 * It's actually a wrapper for Perl_re_op_compile that only takes an SV
 * pattern rather than a list of OPs, and uses the internal engine rather
 * than the current one */

REGEXP *
Perl_re_compile(pTHX_ SV * const pattern, U32 rx_flags)
{
    SV *pat = pattern; /* defeat constness! */
    PERL_ARGS_ASSERT_RE_COMPILE;
    return Perl_re_op_compile(aTHX_ &pat, 1, NULL,
#ifdef PERL_IN_XSUB_RE
                                &my_reg_engine,
#else
                                &PL_core_reg_engine,
#endif
                                NULL, NULL, rx_flags, 0);
}


static void
S_free_codeblocks(pTHX_ struct reg_code_blocks *cbs)
{
    int n;

    if (--cbs->refcnt > 0)
        return;
    for (n = 0; n < cbs->count; n++) {
        REGEXP *rx = cbs->cb[n].src_regex;
        if (rx) {
            cbs->cb[n].src_regex = NULL;
            SvREFCNT_dec_NN(rx);
        }
    }
    Safefree(cbs->cb);
    Safefree(cbs);
}


static struct reg_code_blocks *
S_alloc_code_blocks(pTHX_  int ncode)
{
     struct reg_code_blocks *cbs;
    Newx(cbs, 1, struct reg_code_blocks);
    cbs->count = ncode;
    cbs->refcnt = 1;
    SAVEDESTRUCTOR_X(S_free_codeblocks, cbs);
    if (ncode)
        Newx(cbs->cb, ncode, struct reg_code_block);
    else
        cbs->cb = NULL;
    return cbs;
}


/* upgrade pattern pat_p of length plen_p to UTF8, and if there are code
 * blocks, recalculate the indices. Update pat_p and plen_p in-place to
 * point to the realloced string and length.
 *
 * This is essentially a copy of Perl_bytes_to_utf8() with the code index
 * stuff added */

static void
S_pat_upgrade_to_utf8(pTHX_ RExC_state_t * const pRExC_state,
		    char **pat_p, STRLEN *plen_p, int num_code_blocks)
{
    U8 *const src = (U8*)*pat_p;
    U8 *dst, *d;
    int n=0;
    STRLEN s = 0;
    bool do_end = 0;
    GET_RE_DEBUG_FLAGS_DECL;

    DEBUG_PARSE_r(Perl_re_printf( aTHX_
        "UTF8 mismatch! Converting to utf8 for resizing and compile\n"));

    /* 1 for each byte + 1 for each byte that expands to two, + trailing NUL */
    Newx(dst, *plen_p + variant_under_utf8_count(src, src + *plen_p) + 1, U8);
    d = dst;

    while (s < *plen_p) {
        append_utf8_from_native_byte(src[s], &d);

        if (n < num_code_blocks) {
            assert(pRExC_state->code_blocks);
            if (!do_end && pRExC_state->code_blocks->cb[n].start == s) {
                pRExC_state->code_blocks->cb[n].start = d - dst - 1;
                assert(*(d - 1) == '(');
                do_end = 1;
            }
            else if (do_end && pRExC_state->code_blocks->cb[n].end == s) {
                pRExC_state->code_blocks->cb[n].end = d - dst - 1;
                assert(*(d - 1) == ')');
                do_end = 0;
                n++;
            }
        }
        s++;
    }
    *d = '\0';
    *plen_p = d - dst;
    *pat_p = (char*) dst;
    SAVEFREEPV(*pat_p);
    RExC_orig_utf8 = RExC_utf8 = 1;
}



/* S_concat_pat(): concatenate a list of args to the pattern string pat,
 * while recording any code block indices, and handling overloading,
 * nested qr// objects etc.  If pat is null, it will allocate a new
 * string, or just return the first arg, if there's only one.
 *
 * Returns the malloced/updated pat.
 * patternp and pat_count is the array of SVs to be concatted;
 * oplist is the optional list of ops that generated the SVs;
 * recompile_p is a pointer to a boolean that will be set if
 *   the regex will need to be recompiled.
 * delim, if non-null is an SV that will be inserted between each element
 */

static SV*
S_concat_pat(pTHX_ RExC_state_t * const pRExC_state,
                SV *pat, SV ** const patternp, int pat_count,
                OP *oplist, bool *recompile_p, SV *delim)
{
    SV **svp;
    int n = 0;
    bool use_delim = FALSE;
    bool alloced = FALSE;

    /* if we know we have at least two args, create an empty string,
     * then concatenate args to that. For no args, return an empty string */
    if (!pat && pat_count != 1) {
        pat = newSVpvs("");
        SAVEFREESV(pat);
        alloced = TRUE;
    }

    for (svp = patternp; svp < patternp + pat_count; svp++) {
        SV *sv;
        SV *rx  = NULL;
        STRLEN orig_patlen = 0;
        bool code = 0;
        SV *msv = use_delim ? delim : *svp;
        if (!msv) msv = &PL_sv_undef;

        /* if we've got a delimiter, we go round the loop twice for each
         * svp slot (except the last), using the delimiter the second
         * time round */
        if (use_delim) {
            svp--;
            use_delim = FALSE;
        }
        else if (delim)
            use_delim = TRUE;

        if (SvTYPE(msv) == SVt_PVAV) {
            /* we've encountered an interpolated array within
             * the pattern, e.g. /...@a..../. Expand the list of elements,
             * then recursively append elements.
             * The code in this block is based on S_pushav() */

            AV *const av = (AV*)msv;
            const SSize_t maxarg = AvFILL(av) + 1;
            SV **array;

            if (oplist) {
                assert(oplist->op_type == OP_PADAV
                    || oplist->op_type == OP_RV2AV);
                oplist = OpSIBLING(oplist);
            }

            if (SvRMAGICAL(av)) {
                SSize_t i;

                Newx(array, maxarg, SV*);
                SAVEFREEPV(array);
                for (i=0; i < maxarg; i++) {
                    SV ** const svp = av_fetch(av, i, FALSE);
                    array[i] = svp ? *svp : &PL_sv_undef;
                }
            }
            else
                array = AvARRAY(av);

            pat = S_concat_pat(aTHX_ pRExC_state, pat,
                                array, maxarg, NULL, recompile_p,
                                /* $" */
                                GvSV((gv_fetchpvs("\"", GV_ADDMULTI, SVt_PV))));

            continue;
        }


        /* we make the assumption here that each op in the list of
         * op_siblings maps to one SV pushed onto the stack,
         * except for code blocks, with have both an OP_NULL and
         * and OP_CONST.
         * This allows us to match up the list of SVs against the
         * list of OPs to find the next code block.
         *
         * Note that       PUSHMARK PADSV PADSV ..
         * is optimised to
         *                 PADRANGE PADSV  PADSV  ..
         * so the alignment still works. */

        if (oplist) {
            if (oplist->op_type == OP_NULL
                && (oplist->op_flags & OPf_SPECIAL))
            {
                assert(n < pRExC_state->code_blocks->count);
                pRExC_state->code_blocks->cb[n].start = pat ? SvCUR(pat) : 0;
                pRExC_state->code_blocks->cb[n].block = oplist;
                pRExC_state->code_blocks->cb[n].src_regex = NULL;
                n++;
                code = 1;
                oplist = OpSIBLING(oplist); /* skip CONST */
                assert(oplist);
            }
            oplist = OpSIBLING(oplist);;
        }

	/* apply magic and QR overloading to arg */

        SvGETMAGIC(msv);
        if (SvROK(msv) && SvAMAGIC(msv)) {
            SV *sv = AMG_CALLunary(msv, regexp_amg);
            if (sv) {
                if (SvROK(sv))
                    sv = SvRV(sv);
                if (SvTYPE(sv) != SVt_REGEXP)
                    Perl_croak(aTHX_ "Overloaded qr did not return a REGEXP");
                msv = sv;
            }
        }

        /* try concatenation overload ... */
        if (pat && (SvAMAGIC(pat) || SvAMAGIC(msv)) &&
                (sv = amagic_call(pat, msv, concat_amg, AMGf_assign)))
        {
            sv_setsv(pat, sv);
            /* overloading involved: all bets are off over literal
             * code. Pretend we haven't seen it */
            if (n)
                pRExC_state->code_blocks->count -= n;
            n = 0;
        }
        else  {
            /* ... or failing that, try "" overload */
            while (SvAMAGIC(msv)
                    && (sv = AMG_CALLunary(msv, string_amg))
                    && sv != msv
                    &&  !(   SvROK(msv)
                          && SvROK(sv)
                          && SvRV(msv) == SvRV(sv))
            ) {
                msv = sv;
                SvGETMAGIC(msv);
            }
            if (SvROK(msv) && SvTYPE(SvRV(msv)) == SVt_REGEXP)
                msv = SvRV(msv);

            if (pat) {
                /* this is a partially unrolled
                 *     sv_catsv_nomg(pat, msv);
                 * that allows us to adjust code block indices if
                 * needed */
                STRLEN dlen;
                char *dst = SvPV_force_nomg(pat, dlen);
                orig_patlen = dlen;
                if (SvUTF8(msv) && !SvUTF8(pat)) {
                    S_pat_upgrade_to_utf8(aTHX_ pRExC_state, &dst, &dlen, n);
                    sv_setpvn(pat, dst, dlen);
                    SvUTF8_on(pat);
                }
                sv_catsv_nomg(pat, msv);
                rx = msv;
            }
            else {
                /* We have only one SV to process, but we need to verify
                 * it is properly null terminated or we will fail asserts
                 * later. In theory we probably shouldn't get such SV's,
                 * but if we do we should handle it gracefully. */
                if ( SvTYPE(msv) != SVt_PV || (SvLEN(msv) > SvCUR(msv) && *(SvEND(msv)) == 0) || SvIsCOW_shared_hash(msv) ) {
                    /* not a string, or a string with a trailing null */
                    pat = msv;
                } else {
                    /* a string with no trailing null, we need to copy it
                     * so it has a trailing null */
                    pat = sv_2mortal(newSVsv(msv));
                }
            }

            if (code)
                pRExC_state->code_blocks->cb[n-1].end = SvCUR(pat)-1;
        }

        /* extract any code blocks within any embedded qr//'s */
        if (rx && SvTYPE(rx) == SVt_REGEXP
            && RX_ENGINE((REGEXP*)rx)->op_comp)
        {

            RXi_GET_DECL(ReANY((REGEXP *)rx), ri);
            if (ri->code_blocks && ri->code_blocks->count) {
                int i;
                /* the presence of an embedded qr// with code means
                 * we should always recompile: the text of the
                 * qr// may not have changed, but it may be a
                 * different closure than last time */
                *recompile_p = 1;
                if (pRExC_state->code_blocks) {
                    int new_count = pRExC_state->code_blocks->count
                            + ri->code_blocks->count;
                    Renew(pRExC_state->code_blocks->cb,
                            new_count, struct reg_code_block);
                    pRExC_state->code_blocks->count = new_count;
                }
                else
                    pRExC_state->code_blocks = S_alloc_code_blocks(aTHX_
                                                    ri->code_blocks->count);

                for (i=0; i < ri->code_blocks->count; i++) {
                    struct reg_code_block *src, *dst;
                    STRLEN offset =  orig_patlen
                        + ReANY((REGEXP *)rx)->pre_prefix;
                    assert(n < pRExC_state->code_blocks->count);
                    src = &ri->code_blocks->cb[i];
                    dst = &pRExC_state->code_blocks->cb[n];
                    dst->start	    = src->start + offset;
                    dst->end	    = src->end   + offset;
                    dst->block	    = src->block;
                    dst->src_regex  = (REGEXP*) SvREFCNT_inc( (SV*)
                                            src->src_regex
                                                ? src->src_regex
                                                : (REGEXP*)rx);
                    n++;
                }
            }
        }
    }
    /* avoid calling magic multiple times on a single element e.g. =~ $qr */
    if (alloced)
        SvSETMAGIC(pat);

    return pat;
}



/* see if there are any run-time code blocks in the pattern.
 * False positives are allowed */

static bool
S_has_runtime_code(pTHX_ RExC_state_t * const pRExC_state,
		    char *pat, STRLEN plen)
{
    int n = 0;
    STRLEN s;

    PERL_UNUSED_CONTEXT;

    for (s = 0; s < plen; s++) {
	if (   pRExC_state->code_blocks
            && n < pRExC_state->code_blocks->count
	    && s == pRExC_state->code_blocks->cb[n].start)
	{
	    s = pRExC_state->code_blocks->cb[n].end;
	    n++;
	    continue;
	}
	/* TODO ideally should handle [..], (#..), /#.../x to reduce false
	 * positives here */
	if (pat[s] == '(' && s+2 <= plen && pat[s+1] == '?' &&
	    (pat[s+2] == '{'
                || (s + 2 <= plen && pat[s+2] == '?' && pat[s+3] == '{'))
	)
	    return 1;
    }
    return 0;
}

/* Handle run-time code blocks. We will already have compiled any direct
 * or indirect literal code blocks. Now, take the pattern 'pat' and make a
 * copy of it, but with any literal code blocks blanked out and
 * appropriate chars escaped; then feed it into
 *
 *    eval "qr'modified_pattern'"
 *
 * For example,
 *
 *       a\bc(?{"this was literal"})def'ghi\\jkl(?{"this is runtime"})mno
 *
 * becomes
 *
 *    qr'a\\bc_______________________def\'ghi\\\\jkl(?{"this is runtime"})mno'
 *
 * After eval_sv()-ing that, grab any new code blocks from the returned qr
 * and merge them with any code blocks of the original regexp.
 *
 * If the pat is non-UTF8, while the evalled qr is UTF8, don't merge;
 * instead, just save the qr and return FALSE; this tells our caller that
 * the original pattern needs upgrading to utf8.
 */

static bool
S_compile_runtime_code(pTHX_ RExC_state_t * const pRExC_state,
    char *pat, STRLEN plen)
{
    SV *qr;

    GET_RE_DEBUG_FLAGS_DECL;

    if (pRExC_state->runtime_code_qr) {
	/* this is the second time we've been called; this should
	 * only happen if the main pattern got upgraded to utf8
	 * during compilation; re-use the qr we compiled first time
	 * round (which should be utf8 too)
	 */
	qr = pRExC_state->runtime_code_qr;
	pRExC_state->runtime_code_qr = NULL;
	assert(RExC_utf8 && SvUTF8(qr));
    }
    else {
	int n = 0;
	STRLEN s;
	char *p, *newpat;
	int newlen = plen + 7; /* allow for "qr''xx\0" extra chars */
	SV *sv, *qr_ref;
	dSP;

	/* determine how many extra chars we need for ' and \ escaping */
	for (s = 0; s < plen; s++) {
	    if (pat[s] == '\'' || pat[s] == '\\')
		newlen++;
	}

	Newx(newpat, newlen, char);
	p = newpat;
	*p++ = 'q'; *p++ = 'r'; *p++ = '\'';

	for (s = 0; s < plen; s++) {
	    if (   pRExC_state->code_blocks
	        && n < pRExC_state->code_blocks->count
		&& s == pRExC_state->code_blocks->cb[n].start)
	    {
		/* blank out literal code block so that they aren't
                 * recompiled: eg change from/to:
                 *     /(?{xyz})/
                 *     /(?=====)/
                 * and
                 *     /(??{xyz})/
                 *     /(?======)/
                 * and
                 *     /(?(?{xyz}))/
                 *     /(?(?=====))/
                */
		assert(pat[s]   == '(');
		assert(pat[s+1] == '?');
                *p++ = '(';
                *p++ = '?';
                s += 2;
		while (s < pRExC_state->code_blocks->cb[n].end) {
		    *p++ = '=';
		    s++;
		}
                *p++ = ')';
		n++;
		continue;
	    }
	    if (pat[s] == '\'' || pat[s] == '\\')
		*p++ = '\\';
	    *p++ = pat[s];
	}
	*p++ = '\'';
	if (pRExC_state->pm_flags & RXf_PMf_EXTENDED) {
	    *p++ = 'x';
            if (pRExC_state->pm_flags & RXf_PMf_EXTENDED_MORE) {
                *p++ = 'x';
            }
        }
	*p++ = '\0';
	DEBUG_COMPILE_r({
            Perl_re_printf( aTHX_
		"%sre-parsing pattern for runtime code:%s %s\n",
		PL_colors[4], PL_colors[5], newpat);
	});

	sv = newSVpvn_flags(newpat, p-newpat-1, RExC_utf8 ? SVf_UTF8 : 0);
	Safefree(newpat);

	ENTER;
	SAVETMPS;
	save_re_context();
	PUSHSTACKi(PERLSI_REQUIRE);
        /* G_RE_REPARSING causes the toker to collapse \\ into \ when
         * parsing qr''; normally only q'' does this. It also alters
         * hints handling */
	eval_sv(sv, G_SCALAR|G_RE_REPARSING);
	SvREFCNT_dec_NN(sv);
	SPAGAIN;
	qr_ref = POPs;
	PUTBACK;
	{
	    SV * const errsv = ERRSV;
	    if (SvTRUE_NN(errsv))
                /* use croak_sv ? */
		Perl_croak_nocontext("%" SVf, SVfARG(errsv));
	}
	assert(SvROK(qr_ref));
	qr = SvRV(qr_ref);
	assert(SvTYPE(qr) == SVt_REGEXP && RX_ENGINE((REGEXP*)qr)->op_comp);
	/* the leaving below frees the tmp qr_ref.
	 * Give qr a life of its own */
	SvREFCNT_inc(qr);
	POPSTACK;
	FREETMPS;
	LEAVE;

    }

    if (!RExC_utf8 && SvUTF8(qr)) {
	/* first time through; the pattern got upgraded; save the
	 * qr for the next time through */
	assert(!pRExC_state->runtime_code_qr);
	pRExC_state->runtime_code_qr = qr;
	return 0;
    }


    /* extract any code blocks within the returned qr//  */


    /* merge the main (r1) and run-time (r2) code blocks into one */
    {
	RXi_GET_DECL(ReANY((REGEXP *)qr), r2);
	struct reg_code_block *new_block, *dst;
	RExC_state_t * const r1 = pRExC_state; /* convenient alias */
	int i1 = 0, i2 = 0;
        int r1c, r2c;

	if (!r2->code_blocks || !r2->code_blocks->count) /* we guessed wrong */
	{
	    SvREFCNT_dec_NN(qr);
	    return 1;
	}

        if (!r1->code_blocks)
            r1->code_blocks = S_alloc_code_blocks(aTHX_ 0);

        r1c = r1->code_blocks->count;
        r2c = r2->code_blocks->count;

	Newx(new_block, r1c + r2c, struct reg_code_block);

	dst = new_block;

	while (i1 < r1c || i2 < r2c) {
	    struct reg_code_block *src;
	    bool is_qr = 0;

	    if (i1 == r1c) {
		src = &r2->code_blocks->cb[i2++];
		is_qr = 1;
	    }
	    else if (i2 == r2c)
		src = &r1->code_blocks->cb[i1++];
	    else if (  r1->code_blocks->cb[i1].start
	             < r2->code_blocks->cb[i2].start)
	    {
		src = &r1->code_blocks->cb[i1++];
		assert(src->end < r2->code_blocks->cb[i2].start);
	    }
	    else {
		assert(  r1->code_blocks->cb[i1].start
		       > r2->code_blocks->cb[i2].start);
		src = &r2->code_blocks->cb[i2++];
		is_qr = 1;
		assert(src->end < r1->code_blocks->cb[i1].start);
	    }

	    assert(pat[src->start] == '(');
	    assert(pat[src->end]   == ')');
	    dst->start	    = src->start;
	    dst->end	    = src->end;
	    dst->block	    = src->block;
	    dst->src_regex  = is_qr ? (REGEXP*) SvREFCNT_inc( (SV*) qr)
				    : src->src_regex;
	    dst++;
	}
	r1->code_blocks->count += r2c;
	Safefree(r1->code_blocks->cb);
	r1->code_blocks->cb = new_block;
    }

    SvREFCNT_dec_NN(qr);
    return 1;
}


STATIC bool
S_setup_longest(pTHX_ RExC_state_t *pRExC_state,
                      struct reg_substr_datum  *rsd,
                      struct scan_data_substrs *sub,
                      STRLEN longest_length)
{
    /* This is the common code for setting up the floating and fixed length
     * string data extracted from Perl_re_op_compile() below.  Returns a boolean
     * as to whether succeeded or not */

    I32 t;
    SSize_t ml;
    bool eol  = cBOOL(sub->flags & SF_BEFORE_EOL);
    bool meol = cBOOL(sub->flags & SF_BEFORE_MEOL);

    if (! (longest_length
           || (eol /* Can't have SEOL and MULTI */
               && (! meol || (RExC_flags & RXf_PMf_MULTILINE)))
          )
            /* See comments for join_exact for why REG_UNFOLDED_MULTI_SEEN */
        || (RExC_seen & REG_UNFOLDED_MULTI_SEEN))
    {
        return FALSE;
    }

    /* copy the information about the longest from the reg_scan_data
        over to the program. */
    if (SvUTF8(sub->str)) {
        rsd->substr      = NULL;
        rsd->utf8_substr = sub->str;
    } else {
        rsd->substr      = sub->str;
        rsd->utf8_substr = NULL;
    }
    /* end_shift is how many chars that must be matched that
        follow this item. We calculate it ahead of time as once the
        lookbehind offset is added in we lose the ability to correctly
        calculate it.*/
    ml = sub->minlenp ? *(sub->minlenp) : (SSize_t)longest_length;
    rsd->end_shift = ml - sub->min_offset
        - longest_length
            /* XXX SvTAIL is always false here - did you mean FBMcf_TAIL
             * intead? - DAPM
            + (SvTAIL(sub->str) != 0)
            */
        + sub->lookbehind;

    t = (eol/* Can't have SEOL and MULTI */
         && (! meol || (RExC_flags & RXf_PMf_MULTILINE)));
    fbm_compile(sub->str, t ? FBMcf_TAIL : 0);

    return TRUE;
}

STATIC void
S_set_regex_pv(pTHX_ RExC_state_t *pRExC_state, REGEXP *Rx)
{
    /* Calculates and sets in the compiled pattern 'Rx' the string to compile,
     * properly wrapped with the right modifiers */

    bool has_p     = ((RExC_rx->extflags & RXf_PMf_KEEPCOPY) == RXf_PMf_KEEPCOPY);
    bool has_charset = RExC_utf8 || (get_regex_charset(RExC_rx->extflags)
                                                != REGEX_DEPENDS_CHARSET);

    /* The caret is output if there are any defaults: if not all the STD
        * flags are set, or if no character set specifier is needed */
    bool has_default =
                (((RExC_rx->extflags & RXf_PMf_STD_PMMOD) != RXf_PMf_STD_PMMOD)
                || ! has_charset);
    bool has_runon = ((RExC_seen & REG_RUN_ON_COMMENT_SEEN)
                                                == REG_RUN_ON_COMMENT_SEEN);
    U8 reganch = (U8)((RExC_rx->extflags & RXf_PMf_STD_PMMOD)
                        >> RXf_PMf_STD_PMMOD_SHIFT);
    const char *fptr = STD_PAT_MODS;        /*"msixxn"*/
    char *p;
    STRLEN pat_len = RExC_precomp_end - RExC_precomp;

    /* We output all the necessary flags; we never output a minus, as all
        * those are defaults, so are
        * covered by the caret */
    const STRLEN wraplen = pat_len + has_p + has_runon
        + has_default       /* If needs a caret */
        + PL_bitcount[reganch] /* 1 char for each set standard flag */

            /* If needs a character set specifier */
        + ((has_charset) ? MAX_CHARSET_NAME_LENGTH : 0)
        + (sizeof("(?:)") - 1);

    PERL_ARGS_ASSERT_SET_REGEX_PV;

    /* make sure PL_bitcount bounds not exceeded */
    assert(sizeof(STD_PAT_MODS) <= 8);

    p = sv_grow(MUTABLE_SV(Rx), wraplen + 1); /* +1 for the ending NUL */
    SvPOK_on(Rx);
    if (RExC_utf8)
        SvFLAGS(Rx) |= SVf_UTF8;
    *p++='('; *p++='?';

    /* If a default, cover it using the caret */
    if (has_default) {
        *p++= DEFAULT_PAT_MOD;
    }
    if (has_charset) {
        STRLEN len;
        const char* name;

        name = get_regex_charset_name(RExC_rx->extflags, &len);
        if strEQ(name, DEPENDS_PAT_MODS) {  /* /d under UTF-8 => /u */
            assert(RExC_utf8);
            name = UNICODE_PAT_MODS;
            len = sizeof(UNICODE_PAT_MODS) - 1;
        }
        Copy(name, p, len, char);
        p += len;
    }
    if (has_p)
        *p++ = KEEPCOPY_PAT_MOD; /*'p'*/
    {
        char ch;
        while((ch = *fptr++)) {
            if(reganch & 1)
                *p++ = ch;
            reganch >>= 1;
        }
    }

    *p++ = ':';
    Copy(RExC_precomp, p, pat_len, char);
    assert ((RX_WRAPPED(Rx) - p) < 16);
    RExC_rx->pre_prefix = p - RX_WRAPPED(Rx);
    p += pat_len;

    /* Adding a trailing \n causes this to compile properly:
            my $R = qr / A B C # D E/x; /($R)/
        Otherwise the parens are considered part of the comment */
    if (has_runon)
        *p++ = '\n';
    *p++ = ')';
    *p = 0;
    SvCUR_set(Rx, p - RX_WRAPPED(Rx));
}

/*
 * Perl_re_op_compile - the perl internal RE engine's function to compile a
 * regular expression into internal code.
 * The pattern may be passed either as:
 *    a list of SVs (patternp plus pat_count)
 *    a list of OPs (expr)
 * If both are passed, the SV list is used, but the OP list indicates
 * which SVs are actually pre-compiled code blocks
 *
 * The SVs in the list have magic and qr overloading applied to them (and
 * the list may be modified in-place with replacement SVs in the latter
 * case).
 *
 * If the pattern hasn't changed from old_re, then old_re will be
 * returned.
 *
 * eng is the current engine. If that engine has an op_comp method, then
 * handle directly (i.e. we assume that op_comp was us); otherwise, just
 * do the initial concatenation of arguments and pass on to the external
 * engine.
 *
 * If is_bare_re is not null, set it to a boolean indicating whether the
 * arg list reduced (after overloading) to a single bare regex which has
 * been returned (i.e. /$qr/).
 *
 * orig_rx_flags contains RXf_* flags. See perlreapi.pod for more details.
 *
 * pm_flags contains the PMf_* flags, typically based on those from the
 * pm_flags field of the related PMOP. Currently we're only interested in
 * PMf_HAS_CV, PMf_IS_QR, PMf_USE_RE_EVAL.
 *
 * For many years this code had an initial sizing pass that calculated
 * (sometimes incorrectly, leading to security holes) the size needed for the
 * compiled pattern.  That was changed by commit
 * 7c932d07cab18751bfc7515b4320436273a459e2 in 5.29, which reallocs the size, a
 * node at a time, as parsing goes along.  Patches welcome to fix any obsolete
 * references to this sizing pass.
 *
 * Now, an initial crude guess as to the size needed is made, based on the
 * length of the pattern.  Patches welcome to improve that guess.  That amount
 * of space is malloc'd and then immediately freed, and then clawed back node
 * by node.  This design is to minimze, to the extent possible, memory churn
 * when doing the the reallocs.
 *
 * A separate parentheses counting pass may be needed in some cases.
 * (Previously the sizing pass did this.)  Patches welcome to reduce the number
 * of these cases.
 *
 * The existence of a sizing pass necessitated design decisions that are no
 * longer needed.  There are potential areas of simplification.
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.  [I'll say.]
 */

REGEXP *
Perl_re_op_compile(pTHX_ SV ** const patternp, int pat_count,
		    OP *expr, const regexp_engine* eng, REGEXP *old_re,
		     bool *is_bare_re, const U32 orig_rx_flags, const U32 pm_flags)
{
    dVAR;
    REGEXP *Rx;         /* Capital 'R' means points to a REGEXP */
    STRLEN plen;
    char *exp;
    regnode *scan;
    I32 flags;
    SSize_t minlen = 0;
    U32 rx_flags;
    SV *pat;
    SV** new_patternp = patternp;

    /* these are all flags - maybe they should be turned
     * into a single int with different bit masks */
    I32 sawlookahead = 0;
    I32 sawplus = 0;
    I32 sawopen = 0;
    I32 sawminmod = 0;

    regex_charset initial_charset = get_regex_charset(orig_rx_flags);
    bool recompile = 0;
    bool runtime_code = 0;
    scan_data_t data;
    RExC_state_t RExC_state;
    RExC_state_t * const pRExC_state = &RExC_state;
#ifdef TRIE_STUDY_OPT
    int restudied = 0;
    RExC_state_t copyRExC_state;
#endif
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_OP_COMPILE;

    DEBUG_r(if (!PL_colorset) reginitcolors());

    /* Initialize these here instead of as-needed, as is quick and avoids
     * having to test them each time otherwise */
    if (! PL_InBitmap) {
#ifdef DEBUGGING
        char * dump_len_string;
#endif

        /* This is calculated here, because the Perl program that generates the
         * static global ones doesn't currently have access to
         * NUM_ANYOF_CODE_POINTS */
	PL_InBitmap = _new_invlist(2);
	PL_InBitmap = _add_range_to_invlist(PL_InBitmap, 0,
                                                    NUM_ANYOF_CODE_POINTS - 1);
#ifdef DEBUGGING
        dump_len_string = PerlEnv_getenv("PERL_DUMP_RE_MAX_LEN");
        if (   ! dump_len_string
            || ! grok_atoUV(dump_len_string, (UV *)&PL_dump_re_max_len, NULL))
        {
            PL_dump_re_max_len = 60;    /* A reasonable default */
        }
#endif
    }

    pRExC_state->warn_text = NULL;
    pRExC_state->unlexed_names = NULL;
    pRExC_state->code_blocks = NULL;

    if (is_bare_re)
	*is_bare_re = FALSE;

    if (expr && (expr->op_type == OP_LIST ||
		(expr->op_type == OP_NULL && expr->op_targ == OP_LIST))) {
	/* allocate code_blocks if needed */
	OP *o;
	int ncode = 0;

	for (o = cLISTOPx(expr)->op_first; o; o = OpSIBLING(o))
	    if (o->op_type == OP_NULL && (o->op_flags & OPf_SPECIAL))
		ncode++; /* count of DO blocks */

	if (ncode)
            pRExC_state->code_blocks = S_alloc_code_blocks(aTHX_ ncode);
    }

    if (!pat_count) {
        /* compile-time pattern with just OP_CONSTs and DO blocks */

        int n;
        OP *o;

        /* find how many CONSTs there are */
        assert(expr);
        n = 0;
        if (expr->op_type == OP_CONST)
            n = 1;
        else
            for (o = cLISTOPx(expr)->op_first; o; o = OpSIBLING(o)) {
                if (o->op_type == OP_CONST)
                    n++;
            }

        /* fake up an SV array */

        assert(!new_patternp);
        Newx(new_patternp, n, SV*);
        SAVEFREEPV(new_patternp);
        pat_count = n;

        n = 0;
        if (expr->op_type == OP_CONST)
            new_patternp[n] = cSVOPx_sv(expr);
        else
            for (o = cLISTOPx(expr)->op_first; o; o = OpSIBLING(o)) {
                if (o->op_type == OP_CONST)
                    new_patternp[n++] = cSVOPo_sv;
            }

    }

    DEBUG_PARSE_r(Perl_re_printf( aTHX_
        "Assembling pattern from %d elements%s\n", pat_count,
            orig_rx_flags & RXf_SPLIT ? " for split" : ""));

    /* set expr to the first arg op */

    if (pRExC_state->code_blocks && pRExC_state->code_blocks->count
         && expr->op_type != OP_CONST)
    {
            expr = cLISTOPx(expr)->op_first;
            assert(   expr->op_type == OP_PUSHMARK
                   || (expr->op_type == OP_NULL && expr->op_targ == OP_PUSHMARK)
                   || expr->op_type == OP_PADRANGE);
            expr = OpSIBLING(expr);
    }

    pat = S_concat_pat(aTHX_ pRExC_state, NULL, new_patternp, pat_count,
                        expr, &recompile, NULL);

    /* handle bare (possibly after overloading) regex: foo =~ $re */
    {
        SV *re = pat;
        if (SvROK(re))
            re = SvRV(re);
        if (SvTYPE(re) == SVt_REGEXP) {
            if (is_bare_re)
                *is_bare_re = TRUE;
            SvREFCNT_inc(re);
            DEBUG_PARSE_r(Perl_re_printf( aTHX_
                "Precompiled pattern%s\n",
                    orig_rx_flags & RXf_SPLIT ? " for split" : ""));

            return (REGEXP*)re;
        }
    }

    exp = SvPV_nomg(pat, plen);

    if (!eng->op_comp) {
	if ((SvUTF8(pat) && IN_BYTES)
		|| SvGMAGICAL(pat) || SvAMAGIC(pat))
	{
	    /* make a temporary copy; either to convert to bytes,
	     * or to avoid repeating get-magic / overloaded stringify */
	    pat = newSVpvn_flags(exp, plen, SVs_TEMP |
					(IN_BYTES ? 0 : SvUTF8(pat)));
	}
	return CALLREGCOMP_ENG(eng, pat, orig_rx_flags);
    }

    /* ignore the utf8ness if the pattern is 0 length */
    RExC_utf8 = RExC_orig_utf8 = (plen == 0 || IN_BYTES) ? 0 : SvUTF8(pat);
    RExC_uni_semantics = 0;
    RExC_contains_locale = 0;
    RExC_strict = cBOOL(pm_flags & RXf_PMf_STRICT);
    RExC_in_script_run = 0;
    RExC_study_started = 0;
    pRExC_state->runtime_code_qr = NULL;
    RExC_frame_head= NULL;
    RExC_frame_last= NULL;
    RExC_frame_count= 0;
    RExC_latest_warn_offset = 0;
    RExC_use_BRANCHJ = 0;
    RExC_total_parens = 0;
    RExC_open_parens = NULL;
    RExC_close_parens = NULL;
    RExC_paren_names = NULL;
    RExC_size = 0;
    RExC_seen_d_op = FALSE;
#ifdef DEBUGGING
    RExC_paren_name_list = NULL;
#endif

    DEBUG_r({
        RExC_mysv1= sv_newmortal();
        RExC_mysv2= sv_newmortal();
    });

    DEBUG_COMPILE_r({
            SV *dsv= sv_newmortal();
            RE_PV_QUOTED_DECL(s, RExC_utf8, dsv, exp, plen, PL_dump_re_max_len);
            Perl_re_printf( aTHX_  "%sCompiling REx%s %s\n",
                          PL_colors[4], PL_colors[5], s);
        });

    /* we jump here if we have to recompile, e.g., from upgrading the pattern
     * to utf8 */

    if ((pm_flags & PMf_USE_RE_EVAL)
		/* this second condition covers the non-regex literal case,
		 * i.e.  $foo =~ '(?{})'. */
		|| (IN_PERL_COMPILETIME && (PL_hints & HINT_RE_EVAL))
    )
	runtime_code = S_has_runtime_code(aTHX_ pRExC_state, exp, plen);

  redo_parse:
    /* return old regex if pattern hasn't changed */
    /* XXX: note in the below we have to check the flags as well as the
     * pattern.
     *
     * Things get a touch tricky as we have to compare the utf8 flag
     * independently from the compile flags.  */

    if (   old_re
        && !recompile
        && !!RX_UTF8(old_re) == !!RExC_utf8
        && ( RX_COMPFLAGS(old_re) == ( orig_rx_flags & RXf_PMf_FLAGCOPYMASK ) )
	&& RX_PRECOMP(old_re)
	&& RX_PRELEN(old_re) == plen
        && memEQ(RX_PRECOMP(old_re), exp, plen)
	&& !runtime_code /* with runtime code, always recompile */ )
    {
        return old_re;
    }

    /* Allocate the pattern's SV */
    RExC_rx_sv = Rx = (REGEXP*) newSV_type(SVt_REGEXP);
    RExC_rx = ReANY(Rx);
    if ( RExC_rx == NULL )
        FAIL("Regexp out of space");

    rx_flags = orig_rx_flags;

    if (   (UTF || RExC_uni_semantics)
        && initial_charset == REGEX_DEPENDS_CHARSET)
    {

	/* Set to use unicode semantics if the pattern is in utf8 and has the
	 * 'depends' charset specified, as it means unicode when utf8  */
	set_regex_charset(&rx_flags, REGEX_UNICODE_CHARSET);
        RExC_uni_semantics = 1;
    }

    RExC_pm_flags = pm_flags;

    if (runtime_code) {
        assert(TAINTING_get || !TAINT_get);
	if (TAINT_get)
	    Perl_croak(aTHX_ "Eval-group in insecure regular expression");

	if (!S_compile_runtime_code(aTHX_ pRExC_state, exp, plen)) {
	    /* whoops, we have a non-utf8 pattern, whilst run-time code
	     * got compiled as utf8. Try again with a utf8 pattern */
            S_pat_upgrade_to_utf8(aTHX_ pRExC_state, &exp, &plen,
                pRExC_state->code_blocks ? pRExC_state->code_blocks->count : 0);
            goto redo_parse;
	}
    }
    assert(!pRExC_state->runtime_code_qr);

    RExC_sawback = 0;

    RExC_seen = 0;
    RExC_maxlen = 0;
    RExC_in_lookbehind = 0;
    RExC_seen_zerolen = *exp == '^' ? -1 : 0;
#ifdef EBCDIC
    RExC_recode_x_to_native = 0;
#endif
    RExC_in_multi_char_class = 0;

    RExC_start = RExC_copy_start_in_constructed = RExC_copy_start_in_input = RExC_precomp = exp;
    RExC_precomp_end = RExC_end = exp + plen;
    RExC_nestroot = 0;
    RExC_whilem_seen = 0;
    RExC_end_op = NULL;
    RExC_recurse = NULL;
    RExC_study_chunk_recursed = NULL;
    RExC_study_chunk_recursed_bytes= 0;
    RExC_recurse_count = 0;
    pRExC_state->code_index = 0;

    /* Initialize the string in the compiled pattern.  This is so that there is
     * something to output if necessary */
    set_regex_pv(pRExC_state, Rx);

    DEBUG_PARSE_r({
        Perl_re_printf( aTHX_
            "Starting parse and generation\n");
        RExC_lastnum=0;
        RExC_lastparse=NULL;
    });

    /* Allocate space and zero-initialize. Note, the two step process
       of zeroing when in debug mode, thus anything assigned has to
       happen after that */
    if (!  RExC_size) {

        /* On the first pass of the parse, we guess how big this will be.  Then
         * we grow in one operation to that amount and then give it back.  As
         * we go along, we re-allocate what we need.
         *
         * XXX Currently the guess is essentially that the pattern will be an
         * EXACT node with one byte input, one byte output.  This is crude, and
         * better heuristics are welcome.
         *
         * On any subsequent passes, we guess what we actually computed in the
         * latest earlier pass.  Such a pass probably didn't complete so is
         * missing stuff.  We could improve those guesses by knowing where the
         * parse stopped, and use the length so far plus apply the above
         * assumption to what's left. */
        RExC_size = STR_SZ(RExC_end - RExC_start);
    }

    Newxc(RExC_rxi, sizeof(regexp_internal) + RExC_size, char, regexp_internal);
    if ( RExC_rxi == NULL )
        FAIL("Regexp out of space");

    Zero(RExC_rxi, sizeof(regexp_internal) + RExC_size, char);
    RXi_SET( RExC_rx, RExC_rxi );

    /* We start from 0 (over from 0 in the case this is a reparse.  The first
     * node parsed will give back any excess memory we have allocated so far).
     * */
    RExC_size = 0;

    /* non-zero initialization begins here */
    RExC_rx->engine= eng;
    RExC_rx->extflags = rx_flags;
    RXp_COMPFLAGS(RExC_rx) = orig_rx_flags & RXf_PMf_FLAGCOPYMASK;

    if (pm_flags & PMf_IS_QR) {
	RExC_rxi->code_blocks = pRExC_state->code_blocks;
        if (RExC_rxi->code_blocks) {
            RExC_rxi->code_blocks->refcnt++;
        }
    }

    RExC_rx->intflags = 0;

    RExC_flags = rx_flags;	/* don't let top level (?i) bleed */
    RExC_parse = exp;

    /* This NUL is guaranteed because the pattern comes from an SV*, and the sv
     * code makes sure the final byte is an uncounted NUL.  But should this
     * ever not be the case, lots of things could read beyond the end of the
     * buffer: loops like
     *      while(isFOO(*RExC_parse)) RExC_parse++;
     *      strchr(RExC_parse, "foo");
     * etc.  So it is worth noting. */
    assert(*RExC_end == '\0');

    RExC_naughty = 0;
    RExC_npar = 1;
    RExC_parens_buf_size = 0;
    RExC_emit_start = RExC_rxi->program;
    pRExC_state->code_index = 0;

    *((char*) RExC_emit_start) = (char) REG_MAGIC;
    RExC_emit = 1;

    /* Do the parse */
    if (reg(pRExC_state, 0, &flags, 1)) {

        /* Success!, But we may need to redo the parse knowing how many parens
         * there actually are */
        if (IN_PARENS_PASS) {
            flags |= RESTART_PARSE;
        }

        /* We have that number in RExC_npar */
        RExC_total_parens = RExC_npar;

        /* XXX For backporting, use long jumps if there is any possibility of
         * overflow */
        if (RExC_size > U16_MAX && ! RExC_use_BRANCHJ) {
            RExC_use_BRANCHJ = TRUE;
            flags |= RESTART_PARSE;
        }
    }
    else if (! MUST_RESTART(flags)) {
	ReREFCNT_dec(Rx);
        Perl_croak(aTHX_ "panic: reg returned failure to re_op_compile, flags=%#" UVxf, (UV) flags);
    }

    /* Here, we either have success, or we have to redo the parse for some reason */
    if (MUST_RESTART(flags)) {

        /* It's possible to write a regexp in ascii that represents Unicode
        codepoints outside of the byte range, such as via \x{100}. If we
        detect such a sequence we have to convert the entire pattern to utf8
        and then recompile, as our sizing calculation will have been based
        on 1 byte == 1 character, but we will need to use utf8 to encode
        at least some part of the pattern, and therefore must convert the whole
        thing.
        -- dmq */
        if (flags & NEED_UTF8) {

            /* We have stored the offset of the final warning output so far.
             * That must be adjusted.  Any variant characters between the start
             * of the pattern and this warning count for 2 bytes in the final,
             * so just add them again */
            if (UNLIKELY(RExC_latest_warn_offset > 0)) {
                RExC_latest_warn_offset +=
                            variant_under_utf8_count((U8 *) exp, (U8 *) exp
                                                + RExC_latest_warn_offset);
            }
            S_pat_upgrade_to_utf8(aTHX_ pRExC_state, &exp, &plen,
            pRExC_state->code_blocks ? pRExC_state->code_blocks->count : 0);
            DEBUG_PARSE_r(Perl_re_printf( aTHX_ "Need to redo parse after upgrade\n"));
        }
        else {
            DEBUG_PARSE_r(Perl_re_printf( aTHX_ "Need to redo parse\n"));
        }

        if (ALL_PARENS_COUNTED) {
            /* Make enough room for all the known parens, and zero it */
            Renew(RExC_open_parens, RExC_total_parens, regnode_offset);
            Zero(RExC_open_parens, RExC_total_parens, regnode_offset);
            RExC_open_parens[0] = 1;    /* +1 for REG_MAGIC */

            Renew(RExC_close_parens, RExC_total_parens, regnode_offset);
            Zero(RExC_close_parens, RExC_total_parens, regnode_offset);
        }
        else { /* Parse did not complete.  Reinitialize the parentheses
                  structures */
            RExC_total_parens = 0;
            if (RExC_open_parens) {
                Safefree(RExC_open_parens);
                RExC_open_parens = NULL;
            }
            if (RExC_close_parens) {
                Safefree(RExC_close_parens);
                RExC_close_parens = NULL;
            }
        }

        /* Clean up what we did in this parse */
        SvREFCNT_dec_NN(RExC_rx_sv);

        goto redo_parse;
    }

    /* Here, we have successfully parsed and generated the pattern's program
     * for the regex engine.  We are ready to finish things up and look for
     * optimizations. */

    /* Update the string to compile, with correct modifiers, etc */
    set_regex_pv(pRExC_state, Rx);

    RExC_rx->nparens = RExC_total_parens - 1;

    /* Uses the upper 4 bits of the FLAGS field, so keep within that size */
    if (RExC_whilem_seen > 15)
        RExC_whilem_seen = 15;

    DEBUG_PARSE_r({
        Perl_re_printf( aTHX_
            "Required size %" IVdf " nodes\n", (IV)RExC_size);
        RExC_lastnum=0;
        RExC_lastparse=NULL;
    });

#ifdef RE_TRACK_PATTERN_OFFSETS
    DEBUG_OFFSETS_r(Perl_re_printf( aTHX_
                          "%s %" UVuf " bytes for offset annotations.\n",
                          RExC_offsets ? "Got" : "Couldn't get",
                          (UV)((RExC_offsets[0] * 2 + 1))));
    DEBUG_OFFSETS_r(if (RExC_offsets) {
        const STRLEN len = RExC_offsets[0];
        STRLEN i;
        GET_RE_DEBUG_FLAGS_DECL;
        Perl_re_printf( aTHX_
                      "Offsets: [%" UVuf "]\n\t", (UV)RExC_offsets[0]);
        for (i = 1; i <= len; i++) {
            if (RExC_offsets[i*2-1] || RExC_offsets[i*2])
                Perl_re_printf( aTHX_  "%" UVuf ":%" UVuf "[%" UVuf "] ",
                (UV)i, (UV)RExC_offsets[i*2-1], (UV)RExC_offsets[i*2]);
        }
        Perl_re_printf( aTHX_  "\n");
    });

#else
    SetProgLen(RExC_rxi,RExC_size);
#endif

    DEBUG_OPTIMISE_r(
        Perl_re_printf( aTHX_  "Starting post parse optimization\n");
    );

    /* XXXX To minimize changes to RE engine we always allocate
       3-units-long substrs field. */
    Newx(RExC_rx->substrs, 1, struct reg_substr_data);
    if (RExC_recurse_count) {
        Newx(RExC_recurse, RExC_recurse_count, regnode *);
        SAVEFREEPV(RExC_recurse);
    }

    if (RExC_seen & REG_RECURSE_SEEN) {
        /* Note, RExC_total_parens is 1 + the number of parens in a pattern.
         * So its 1 if there are no parens. */
        RExC_study_chunk_recursed_bytes= (RExC_total_parens >> 3) +
                                         ((RExC_total_parens & 0x07) != 0);
        Newx(RExC_study_chunk_recursed,
             RExC_study_chunk_recursed_bytes * RExC_total_parens, U8);
        SAVEFREEPV(RExC_study_chunk_recursed);
    }

  reStudy:
    RExC_rx->minlen = minlen = sawlookahead = sawplus = sawopen = sawminmod = 0;
    DEBUG_r(
        RExC_study_chunk_recursed_count= 0;
    );
    Zero(RExC_rx->substrs, 1, struct reg_substr_data);
    if (RExC_study_chunk_recursed) {
        Zero(RExC_study_chunk_recursed,
             RExC_study_chunk_recursed_bytes * RExC_total_parens, U8);
    }


#ifdef TRIE_STUDY_OPT
    if (!restudied) {
        StructCopy(&zero_scan_data, &data, scan_data_t);
        copyRExC_state = RExC_state;
    } else {
        U32 seen=RExC_seen;
        DEBUG_OPTIMISE_r(Perl_re_printf( aTHX_ "Restudying\n"));

        RExC_state = copyRExC_state;
        if (seen & REG_TOP_LEVEL_BRANCHES_SEEN)
            RExC_seen |= REG_TOP_LEVEL_BRANCHES_SEEN;
        else
            RExC_seen &= ~REG_TOP_LEVEL_BRANCHES_SEEN;
	StructCopy(&zero_scan_data, &data, scan_data_t);
    }
#else
    StructCopy(&zero_scan_data, &data, scan_data_t);
#endif

    /* Dig out information for optimizations. */
    RExC_rx->extflags = RExC_flags; /* was pm_op */
    /*dmq: removed as part of de-PMOP: pm->op_pmflags = RExC_flags; */

    if (UTF)
	SvUTF8_on(Rx);	/* Unicode in it? */
    RExC_rxi->regstclass = NULL;
    if (RExC_naughty >= TOO_NAUGHTY)	/* Probably an expensive pattern. */
	RExC_rx->intflags |= PREGf_NAUGHTY;
    scan = RExC_rxi->program + 1;		/* First BRANCH. */

    /* testing for BRANCH here tells us whether there is "must appear"
       data in the pattern. If there is then we can use it for optimisations */
    if (!(RExC_seen & REG_TOP_LEVEL_BRANCHES_SEEN)) { /*  Only one top-level choice.
                                                  */
	SSize_t fake;
	STRLEN longest_length[2];
	regnode_ssc ch_class; /* pointed to by data */
	int stclass_flag;
	SSize_t last_close = 0; /* pointed to by data */
        regnode *first= scan;
        regnode *first_next= regnext(first);
        int i;

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
	    (OP(first) == IFMATCH && !first->flags && (sawlookahead = 1)) ||
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
                 * (yves doesn't think this is true)
		 */
		if (OP(first) == PLUS)
		    sawplus = 1;
                else {
                    if (OP(first) == MINMOD)
                        sawminmod = 1;
		    first += regarglen[OP(first)];
                }
		first = NEXTOPER(first);
		first_next= regnext(first);
	}

	/* Starting-point info. */
      again:
        DEBUG_PEEP("first:", first, 0, 0);
        /* Ignore EXACT as we deal with it later. */
	if (PL_regkind[OP(first)] == EXACT) {
	    if (   OP(first) == EXACT
                || OP(first) == EXACT_ONLY8
                || OP(first) == EXACTL)
            {
		NOOP;	/* Empty, get anchored substr later. */
            }
	    else
		RExC_rxi->regstclass = first;
	}
#ifdef TRIE_STCLASS
	else if (PL_regkind[OP(first)] == TRIE &&
	        ((reg_trie_data *)RExC_rxi->data->data[ ARG(first) ])->minlen>0)
	{
            /* this can happen only on restudy */
            RExC_rxi->regstclass = construct_ahocorasick_from_trie(pRExC_state, (regnode *)first, 0);
	}
#endif
	else if (REGNODE_SIMPLE(OP(first)))
	    RExC_rxi->regstclass = first;
	else if (PL_regkind[OP(first)] == BOUND ||
		 PL_regkind[OP(first)] == NBOUND)
	    RExC_rxi->regstclass = first;
	else if (PL_regkind[OP(first)] == BOL) {
            RExC_rx->intflags |= (OP(first) == MBOL
                           ? PREGf_ANCH_MBOL
                           : PREGf_ANCH_SBOL);
	    first = NEXTOPER(first);
	    goto again;
	}
	else if (OP(first) == GPOS) {
            RExC_rx->intflags |= PREGf_ANCH_GPOS;
	    first = NEXTOPER(first);
	    goto again;
	}
	else if ((!sawopen || !RExC_sawback) &&
            !sawlookahead &&
	    (OP(first) == STAR &&
	    PL_regkind[OP(NEXTOPER(first))] == REG_ANY) &&
            !(RExC_rx->intflags & PREGf_ANCH) && !pRExC_state->code_blocks)
	{
	    /* turn .* into ^.* with an implied $*=1 */
	    const int type =
		(OP(NEXTOPER(first)) == REG_ANY)
                    ? PREGf_ANCH_MBOL
                    : PREGf_ANCH_SBOL;
            RExC_rx->intflags |= (type | PREGf_IMPLICIT);
	    first = NEXTOPER(first);
	    goto again;
	}
        if (sawplus && !sawminmod && !sawlookahead
            && (!sawopen || !RExC_sawback)
	    && !pRExC_state->code_blocks) /* May examine pos and $& */
	    /* x+ must match at the 1st pos of run of x's */
	    RExC_rx->intflags |= PREGf_SKIP;

	/* Scan is after the zeroth branch, first is atomic matcher. */
#ifdef TRIE_STUDY_OPT
	DEBUG_PARSE_r(
	    if (!restudied)
                Perl_re_printf( aTHX_  "first at %" IVdf "\n",
			      (IV)(first - scan + 1))
        );
#else
	DEBUG_PARSE_r(
            Perl_re_printf( aTHX_  "first at %" IVdf "\n",
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

	data.substrs[0].str = newSVpvs("");
	data.substrs[1].str = newSVpvs("");
	data.last_found = newSVpvs("");
	data.cur_is_floating = 0; /* initially any found substring is fixed */
	ENTER_with_name("study_chunk");
	SAVEFREESV(data.substrs[0].str);
	SAVEFREESV(data.substrs[1].str);
	SAVEFREESV(data.last_found);
	first = scan;
	if (!RExC_rxi->regstclass) {
	    ssc_init(pRExC_state, &ch_class);
	    data.start_class = &ch_class;
	    stclass_flag = SCF_DO_STCLASS_AND;
	} else				/* XXXX Check for BOUND? */
	    stclass_flag = 0;
	data.last_closep = &last_close;

        DEBUG_RExC_seen();
        /*
         * MAIN ENTRY FOR study_chunk() FOR m/PATTERN/
         * (NO top level branches)
         */
	minlen = study_chunk(pRExC_state, &first, &minlen, &fake,
                             scan + RExC_size, /* Up to end */
            &data, -1, 0, NULL,
            SCF_DO_SUBSTR | SCF_WHILEM_VISITED_POS | stclass_flag
                          | (restudied ? SCF_TRIE_DOING_RESTUDY : 0),
            0, TRUE);


        CHECK_RESTUDY_GOTO_butfirst(LEAVE_with_name("study_chunk"));


	if ( RExC_total_parens == 1 && !data.cur_is_floating
	     && data.last_start_min == 0 && data.last_end > 0
	     && !RExC_seen_zerolen
             && !(RExC_seen & REG_VERBARG_SEEN)
             && !(RExC_seen & REG_GPOS_SEEN)
        ){
	    RExC_rx->extflags |= RXf_CHECK_ALL;
        }
	scan_commit(pRExC_state, &data,&minlen, 0);


        /* XXX this is done in reverse order because that's the way the
         * code was before it was parameterised. Don't know whether it
         * actually needs doing in reverse order. DAPM */
        for (i = 1; i >= 0; i--) {
            longest_length[i] = CHR_SVLEN(data.substrs[i].str);

            if (   !(   i
                     && SvCUR(data.substrs[0].str)  /* ok to leave SvCUR */
                     &&    data.substrs[0].min_offset
                        == data.substrs[1].min_offset
                     &&    SvCUR(data.substrs[0].str)
                        == SvCUR(data.substrs[1].str)
                    )
                && S_setup_longest (aTHX_ pRExC_state,
                                        &(RExC_rx->substrs->data[i]),
                                        &(data.substrs[i]),
                                        longest_length[i]))
            {
                RExC_rx->substrs->data[i].min_offset =
                        data.substrs[i].min_offset - data.substrs[i].lookbehind;

                RExC_rx->substrs->data[i].max_offset = data.substrs[i].max_offset;
                /* Don't offset infinity */
                if (data.substrs[i].max_offset < SSize_t_MAX)
                    RExC_rx->substrs->data[i].max_offset -= data.substrs[i].lookbehind;
                SvREFCNT_inc_simple_void_NN(data.substrs[i].str);
            }
            else {
                RExC_rx->substrs->data[i].substr      = NULL;
                RExC_rx->substrs->data[i].utf8_substr = NULL;
                longest_length[i] = 0;
            }
        }

	LEAVE_with_name("study_chunk");

	if (RExC_rxi->regstclass
	    && (OP(RExC_rxi->regstclass) == REG_ANY || OP(RExC_rxi->regstclass) == SANY))
	    RExC_rxi->regstclass = NULL;

	if ((!(RExC_rx->substrs->data[0].substr || RExC_rx->substrs->data[0].utf8_substr)
              || RExC_rx->substrs->data[0].min_offset)
	    && stclass_flag
            && ! (ANYOF_FLAGS(data.start_class) & SSC_MATCHES_EMPTY_STRING)
	    && is_ssc_worth_it(pRExC_state, data.start_class))
	{
	    const U32 n = add_data(pRExC_state, STR_WITH_LEN("f"));

            ssc_finalize(pRExC_state, data.start_class);

	    Newx(RExC_rxi->data->data[n], 1, regnode_ssc);
	    StructCopy(data.start_class,
		       (regnode_ssc*)RExC_rxi->data->data[n],
		       regnode_ssc);
	    RExC_rxi->regstclass = (regnode*)RExC_rxi->data->data[n];
	    RExC_rx->intflags &= ~PREGf_SKIP;	/* Used in find_byclass(). */
	    DEBUG_COMPILE_r({ SV *sv = sv_newmortal();
                      regprop(RExC_rx, sv, (regnode*)data.start_class, NULL, pRExC_state);
                      Perl_re_printf( aTHX_
				    "synthetic stclass \"%s\".\n",
				    SvPVX_const(sv));});
            data.start_class = NULL;
	}

        /* A temporary algorithm prefers floated substr to fixed one of
         * same length to dig more info. */
	i = (longest_length[0] <= longest_length[1]);
        RExC_rx->substrs->check_ix = i;
        RExC_rx->check_end_shift  = RExC_rx->substrs->data[i].end_shift;
        RExC_rx->check_substr     = RExC_rx->substrs->data[i].substr;
        RExC_rx->check_utf8       = RExC_rx->substrs->data[i].utf8_substr;
        RExC_rx->check_offset_min = RExC_rx->substrs->data[i].min_offset;
        RExC_rx->check_offset_max = RExC_rx->substrs->data[i].max_offset;
        if (!i && (RExC_rx->intflags & (PREGf_ANCH_SBOL|PREGf_ANCH_GPOS)))
            RExC_rx->intflags |= PREGf_NOSCAN;

	if ((RExC_rx->check_substr || RExC_rx->check_utf8) ) {
	    RExC_rx->extflags |= RXf_USE_INTUIT;
	    if (SvTAIL(RExC_rx->check_substr ? RExC_rx->check_substr : RExC_rx->check_utf8))
		RExC_rx->extflags |= RXf_INTUIT_TAIL;
	}

	/* XXX Unneeded? dmq (shouldn't as this is handled elsewhere)
	if ( (STRLEN)minlen < longest_length[1] )
            minlen= longest_length[1];
        if ( (STRLEN)minlen < longest_length[0] )
            minlen= longest_length[0];
        */
    }
    else {
	/* Several toplevels. Best we can is to set minlen. */
	SSize_t fake;
	regnode_ssc ch_class;
	SSize_t last_close = 0;

        DEBUG_PARSE_r(Perl_re_printf( aTHX_  "\nMulti Top Level\n"));

	scan = RExC_rxi->program + 1;
	ssc_init(pRExC_state, &ch_class);
	data.start_class = &ch_class;
	data.last_closep = &last_close;

        DEBUG_RExC_seen();
        /*
         * MAIN ENTRY FOR study_chunk() FOR m/P1|P2|.../
         * (patterns WITH top level branches)
         */
	minlen = study_chunk(pRExC_state,
            &scan, &minlen, &fake, scan + RExC_size, &data, -1, 0, NULL,
            SCF_DO_STCLASS_AND|SCF_WHILEM_VISITED_POS|(restudied
                                                      ? SCF_TRIE_DOING_RESTUDY
                                                      : 0),
            0, TRUE);

        CHECK_RESTUDY_GOTO_butfirst(NOOP);

	RExC_rx->check_substr = NULL;
        RExC_rx->check_utf8 = NULL;
        RExC_rx->substrs->data[0].substr      = NULL;
        RExC_rx->substrs->data[0].utf8_substr = NULL;
        RExC_rx->substrs->data[1].substr      = NULL;
        RExC_rx->substrs->data[1].utf8_substr = NULL;

        if (! (ANYOF_FLAGS(data.start_class) & SSC_MATCHES_EMPTY_STRING)
	    && is_ssc_worth_it(pRExC_state, data.start_class))
        {
	    const U32 n = add_data(pRExC_state, STR_WITH_LEN("f"));

            ssc_finalize(pRExC_state, data.start_class);

	    Newx(RExC_rxi->data->data[n], 1, regnode_ssc);
	    StructCopy(data.start_class,
		       (regnode_ssc*)RExC_rxi->data->data[n],
		       regnode_ssc);
	    RExC_rxi->regstclass = (regnode*)RExC_rxi->data->data[n];
	    RExC_rx->intflags &= ~PREGf_SKIP;	/* Used in find_byclass(). */
	    DEBUG_COMPILE_r({ SV* sv = sv_newmortal();
                      regprop(RExC_rx, sv, (regnode*)data.start_class, NULL, pRExC_state);
                      Perl_re_printf( aTHX_
				    "synthetic stclass \"%s\".\n",
				    SvPVX_const(sv));});
            data.start_class = NULL;
	}
    }

    if (RExC_seen & REG_UNBOUNDED_QUANTIFIER_SEEN) {
        RExC_rx->extflags |= RXf_UNBOUNDED_QUANTIFIER_SEEN;
        RExC_rx->maxlen = REG_INFTY;
    }
    else {
        RExC_rx->maxlen = RExC_maxlen;
    }

    /* Guard against an embedded (?=) or (?<=) with a longer minlen than
       the "real" pattern. */
    DEBUG_OPTIMISE_r({
        Perl_re_printf( aTHX_ "minlen: %" IVdf " RExC_rx->minlen:%" IVdf " maxlen:%" IVdf "\n",
                      (IV)minlen, (IV)RExC_rx->minlen, (IV)RExC_maxlen);
    });
    RExC_rx->minlenret = minlen;
    if (RExC_rx->minlen < minlen)
        RExC_rx->minlen = minlen;

    if (RExC_seen & REG_RECURSE_SEEN ) {
        RExC_rx->intflags |= PREGf_RECURSE_SEEN;
        Newx(RExC_rx->recurse_locinput, RExC_rx->nparens + 1, char *);
    }
    if (RExC_seen & REG_GPOS_SEEN)
        RExC_rx->intflags |= PREGf_GPOS_SEEN;
    if (RExC_seen & REG_LOOKBEHIND_SEEN)
        RExC_rx->extflags |= RXf_NO_INPLACE_SUBST; /* inplace might break the
                                                lookbehind */
    if (pRExC_state->code_blocks)
	RExC_rx->extflags |= RXf_EVAL_SEEN;
    if (RExC_seen & REG_VERBARG_SEEN)
    {
	RExC_rx->intflags |= PREGf_VERBARG_SEEN;
        RExC_rx->extflags |= RXf_NO_INPLACE_SUBST; /* don't understand this! Yves */
    }
    if (RExC_seen & REG_CUTGROUP_SEEN)
	RExC_rx->intflags |= PREGf_CUTGROUP_SEEN;
    if (pm_flags & PMf_USE_RE_EVAL)
	RExC_rx->intflags |= PREGf_USE_RE_EVAL;
    if (RExC_paren_names)
        RXp_PAREN_NAMES(RExC_rx) = MUTABLE_HV(SvREFCNT_inc(RExC_paren_names));
    else
        RXp_PAREN_NAMES(RExC_rx) = NULL;

    /* If we have seen an anchor in our pattern then we set the extflag RXf_IS_ANCHORED
     * so it can be used in pp.c */
    if (RExC_rx->intflags & PREGf_ANCH)
        RExC_rx->extflags |= RXf_IS_ANCHORED;


    {
        /* this is used to identify "special" patterns that might result
         * in Perl NOT calling the regex engine and instead doing the match "itself",
         * particularly special cases in split//. By having the regex compiler
         * do this pattern matching at a regop level (instead of by inspecting the pattern)
         * we avoid weird issues with equivalent patterns resulting in different behavior,
         * AND we allow non Perl engines to get the same optimizations by the setting the
         * flags appropriately - Yves */
        regnode *first = RExC_rxi->program + 1;
        U8 fop = OP(first);
        regnode *next = regnext(first);
        U8 nop = OP(next);

        if (PL_regkind[fop] == NOTHING && nop == END)
            RExC_rx->extflags |= RXf_NULL;
        else if ((fop == MBOL || (fop == SBOL && !first->flags)) && nop == END)
            /* when fop is SBOL first->flags will be true only when it was
             * produced by parsing /\A/, and not when parsing /^/. This is
             * very important for the split code as there we want to
             * treat /^/ as /^/m, but we do not want to treat /\A/ as /^/m.
             * See rt #122761 for more details. -- Yves */
            RExC_rx->extflags |= RXf_START_ONLY;
        else if (fop == PLUS
                 && PL_regkind[nop] == POSIXD && FLAGS(next) == _CC_SPACE
                 && nop == END)
            RExC_rx->extflags |= RXf_WHITE;
        else if ( RExC_rx->extflags & RXf_SPLIT
                  && (fop == EXACT || fop == EXACT_ONLY8 || fop == EXACTL)
                  && STR_LEN(first) == 1
                  && *(STRING(first)) == ' '
                  && nop == END )
            RExC_rx->extflags |= (RXf_SKIPWHITE|RXf_WHITE);

    }

    if (RExC_contains_locale) {
        RXp_EXTFLAGS(RExC_rx) |= RXf_TAINTED;
    }

#ifdef DEBUGGING
    if (RExC_paren_names) {
        RExC_rxi->name_list_idx = add_data( pRExC_state, STR_WITH_LEN("a"));
        RExC_rxi->data->data[RExC_rxi->name_list_idx]
                                   = (void*)SvREFCNT_inc(RExC_paren_name_list);
    } else
#endif
    RExC_rxi->name_list_idx = 0;

    while ( RExC_recurse_count > 0 ) {
        const regnode *scan = RExC_recurse[ --RExC_recurse_count ];
        /*
         * This data structure is set up in study_chunk() and is used
         * to calculate the distance between a GOSUB regopcode and
         * the OPEN/CURLYM (CURLYM's are special and can act like OPEN's)
         * it refers to.
         *
         * If for some reason someone writes code that optimises
         * away a GOSUB opcode then the assert should be changed to
         * an if(scan) to guard the ARG2L_SET() - Yves
         *
         */
        assert(scan && OP(scan) == GOSUB);
        ARG2L_SET( scan, RExC_open_parens[ARG(scan)] - REGNODE_OFFSET(scan));
    }

    Newxz(RExC_rx->offs, RExC_total_parens, regexp_paren_pair);
    /* assume we don't need to swap parens around before we match */
    DEBUG_TEST_r({
        Perl_re_printf( aTHX_ "study_chunk_recursed_count: %lu\n",
            (unsigned long)RExC_study_chunk_recursed_count);
    });
    DEBUG_DUMP_r({
        DEBUG_RExC_seen();
        Perl_re_printf( aTHX_ "Final program:\n");
        regdump(RExC_rx);
    });

    if (RExC_open_parens) {
        Safefree(RExC_open_parens);
        RExC_open_parens = NULL;
    }
    if (RExC_close_parens) {
        Safefree(RExC_close_parens);
        RExC_close_parens = NULL;
    }

#ifdef USE_ITHREADS
    /* under ithreads the ?pat? PMf_USED flag on the pmop is simulated
     * by setting the regexp SV to readonly-only instead. If the
     * pattern's been recompiled, the USEDness should remain. */
    if (old_re && SvREADONLY(old_re))
        SvREADONLY_on(Rx);
#endif
    return Rx;
}


SV*
Perl_reg_named_buff(pTHX_ REGEXP * const rx, SV * const key, SV * const value,
                    const U32 flags)
{
    PERL_ARGS_ASSERT_REG_NAMED_BUFF;

    PERL_UNUSED_ARG(value);

    if (flags & RXapif_FETCH) {
        return reg_named_buff_fetch(rx, key, flags);
    } else if (flags & (RXapif_STORE | RXapif_DELETE | RXapif_CLEAR)) {
        Perl_croak_no_modify();
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
        Perl_croak(aTHX_ "panic: Unknown flags %d in named_buff_iter",
                                            (int)flags);
        return NULL;
    }
}

SV*
Perl_reg_named_buff_fetch(pTHX_ REGEXP * const r, SV * const namesv,
			  const U32 flags)
{
    SV *ret;
    struct regexp *const rx = ReANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_FETCH;

    if (rx && RXp_PAREN_NAMES(rx)) {
        HE *he_str = hv_fetch_ent( RXp_PAREN_NAMES(rx), namesv, 0, 0 );
        if (he_str) {
            IV i;
            SV* sv_dat=HeVAL(he_str);
            I32 *nums=(I32*)SvPVX(sv_dat);
            AV * const retarray = (flags & RXapif_ALL) ? newAV() : NULL;
            for ( i=0; i<SvIVX(sv_dat); i++ ) {
                if ((I32)(rx->nparens) >= nums[i]
                    && rx->offs[nums[i]].start != -1
                    && rx->offs[nums[i]].end != -1)
                {
                    ret = newSVpvs("");
                    CALLREG_NUMBUF_FETCH(r, nums[i], ret);
                    if (!retarray)
                        return ret;
                } else {
                    if (retarray)
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
    struct regexp *const rx = ReANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_EXISTS;

    if (rx && RXp_PAREN_NAMES(rx)) {
        if (flags & RXapif_ALL) {
            return hv_exists_ent(RXp_PAREN_NAMES(rx), key, 0);
        } else {
	    SV *sv = CALLREG_NAMED_BUFF_FETCH(r, key, flags);
            if (sv) {
		SvREFCNT_dec_NN(sv);
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
    struct regexp *const rx = ReANY(r);

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
    struct regexp *const rx = ReANY(r);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_NEXTKEY;

    if (rx && RXp_PAREN_NAMES(rx)) {
        HV *hv = RXp_PAREN_NAMES(rx);
        HE *temphe;
        while ( (temphe = hv_iternext_flags(hv, 0)) ) {
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
    SSize_t length;
    struct regexp *const rx = ReANY(r);

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_SCALAR;

    if (rx && RXp_PAREN_NAMES(rx)) {
        if (flags & (RXapif_ALL | RXapif_REGNAMES_COUNT)) {
            return newSViv(HvTOTALKEYS(RXp_PAREN_NAMES(rx)));
        } else if (flags & RXapif_ONE) {
            ret = CALLREG_NAMED_BUFF_ALL(r, (flags | RXapif_REGNAMES));
            av = MUTABLE_AV(SvRV(ret));
            length = av_tindex(av);
	    SvREFCNT_dec_NN(ret);
            return newSViv(length + 1);
        } else {
            Perl_croak(aTHX_ "panic: Unknown flags %d in named_buff_scalar",
                                                (int)flags);
            return NULL;
        }
    }
    return &PL_sv_undef;
}

SV*
Perl_reg_named_buff_all(pTHX_ REGEXP * const r, const U32 flags)
{
    struct regexp *const rx = ReANY(r);
    AV *av = newAV();

    PERL_ARGS_ASSERT_REG_NAMED_BUFF_ALL;

    if (rx && RXp_PAREN_NAMES(rx)) {
        HV *hv= RXp_PAREN_NAMES(rx);
        HE *temphe;
        (void)hv_iterinit(hv);
        while ( (temphe = hv_iternext_flags(hv, 0)) ) {
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
    struct regexp *const rx = ReANY(r);
    char *s = NULL;
    SSize_t i = 0;
    SSize_t s1, t1;
    I32 n = paren;

    PERL_ARGS_ASSERT_REG_NUMBERED_BUFF_FETCH;

    if (      n == RX_BUFF_IDX_CARET_PREMATCH
           || n == RX_BUFF_IDX_CARET_FULLMATCH
           || n == RX_BUFF_IDX_CARET_POSTMATCH
       )
    {
        bool keepcopy = cBOOL(rx->extflags & RXf_PMf_KEEPCOPY);
        if (!keepcopy) {
            /* on something like
             *    $r = qr/.../;
             *    /$qr/p;
             * the KEEPCOPY is set on the PMOP rather than the regex */
            if (PL_curpm && r == PM_GETRE(PL_curpm))
                 keepcopy = cBOOL(PL_curpm->op_pmflags & PMf_KEEPCOPY);
        }
        if (!keepcopy)
            goto ret_undef;
    }

    if (!rx->subbeg)
        goto ret_undef;

    if (n == RX_BUFF_IDX_CARET_FULLMATCH)
        /* no need to distinguish between them any more */
        n = RX_BUFF_IDX_FULLMATCH;

    if ((n == RX_BUFF_IDX_PREMATCH || n == RX_BUFF_IDX_CARET_PREMATCH)
        && rx->offs[0].start != -1)
    {
        /* $`, ${^PREMATCH} */
	i = rx->offs[0].start;
	s = rx->subbeg;
    }
    else
    if ((n == RX_BUFF_IDX_POSTMATCH || n == RX_BUFF_IDX_CARET_POSTMATCH)
        && rx->offs[0].end != -1)
    {
        /* $', ${^POSTMATCH} */
	s = rx->subbeg - rx->suboffset + rx->offs[0].end;
	i = rx->sublen + rx->suboffset - rx->offs[0].end;
    }
    else
    if ( 0 <= n && n <= (I32)rx->nparens &&
        (s1 = rx->offs[n].start) != -1 &&
        (t1 = rx->offs[n].end) != -1)
    {
        /* $&, ${^MATCH},  $1 ... */
        i = t1 - s1;
        s = rx->subbeg + s1 - rx->suboffset;
    } else {
        goto ret_undef;
    }

    assert(s >= rx->subbeg);
    assert((STRLEN)rx->sublen >= (STRLEN)((s - rx->subbeg) + i) );
    if (i >= 0) {
#ifdef NO_TAINT_SUPPORT
        sv_setpvn(sv, s, i);
#else
        const int oldtainted = TAINT_get;
        TAINT_NOT;
        sv_setpvn(sv, s, i);
        TAINT_set(oldtainted);
#endif
        if (RXp_MATCH_UTF8(rx))
            SvUTF8_on(sv);
        else
            SvUTF8_off(sv);
        if (TAINTING_get) {
            if (RXp_MATCH_TAINTED(rx)) {
                if (SvTYPE(sv) >= SVt_PVMG) {
                    MAGIC* const mg = SvMAGIC(sv);
                    MAGIC* mgt;
                    TAINT;
                    SvMAGIC_set(sv, mg->mg_moremagic);
                    SvTAINT(sv);
                    if ((mgt = SvMAGIC(sv))) {
                        mg->mg_moremagic = mgt;
                        SvMAGIC_set(sv, mg);
                    }
                } else {
                    TAINT;
                    SvTAINT(sv);
                }
            } else
                SvTAINTED_off(sv);
        }
    } else {
      ret_undef:
        sv_set_undef(sv);
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
        Perl_croak_no_modify();
}

I32
Perl_reg_numbered_buff_length(pTHX_ REGEXP * const r, const SV * const sv,
                              const I32 paren)
{
    struct regexp *const rx = ReANY(r);
    I32 i;
    I32 s1, t1;

    PERL_ARGS_ASSERT_REG_NUMBERED_BUFF_LENGTH;

    if (   paren == RX_BUFF_IDX_CARET_PREMATCH
        || paren == RX_BUFF_IDX_CARET_FULLMATCH
        || paren == RX_BUFF_IDX_CARET_POSTMATCH
    )
    {
        bool keepcopy = cBOOL(rx->extflags & RXf_PMf_KEEPCOPY);
        if (!keepcopy) {
            /* on something like
             *    $r = qr/.../;
             *    /$qr/p;
             * the KEEPCOPY is set on the PMOP rather than the regex */
            if (PL_curpm && r == PM_GETRE(PL_curpm))
                 keepcopy = cBOOL(PL_curpm->op_pmflags & PMf_KEEPCOPY);
        }
        if (!keepcopy)
            goto warn_undef;
    }

    /* Some of this code was originally in C<Perl_magic_len> in F<mg.c> */
    switch (paren) {
      case RX_BUFF_IDX_CARET_PREMATCH: /* ${^PREMATCH} */
      case RX_BUFF_IDX_PREMATCH:       /* $` */
        if (rx->offs[0].start != -1) {
			i = rx->offs[0].start;
			if (i > 0) {
				s1 = 0;
				t1 = i;
				goto getlen;
			}
	    }
        return 0;

      case RX_BUFF_IDX_CARET_POSTMATCH: /* ${^POSTMATCH} */
      case RX_BUFF_IDX_POSTMATCH:       /* $' */
	    if (rx->offs[0].end != -1) {
			i = rx->sublen - rx->offs[0].end;
			if (i > 0) {
				s1 = rx->offs[0].end;
				t1 = rx->sublen;
				goto getlen;
			}
	    }
        return 0;

      default: /* $& / ${^MATCH}, $1, $2, ... */
	    if (paren <= (I32)rx->nparens &&
            (s1 = rx->offs[paren].start) != -1 &&
            (t1 = rx->offs[paren].end) != -1)
	    {
            i = t1 - s1;
            goto getlen;
        } else {
          warn_undef:
            if (ckWARN(WARN_UNINITIALIZED))
                report_uninit((const SV *)sv);
            return 0;
        }
    }
  getlen:
    if (i > 0 && RXp_MATCH_UTF8(rx)) {
        const char * const s = rx->subbeg - rx->suboffset + s1;
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
    SV* sv_name;

    PERL_ARGS_ASSERT_REG_SCAN_NAME;

    assert (RExC_parse <= RExC_end);
    if (RExC_parse == RExC_end) NOOP;
    else if (isIDFIRST_lazy_if_safe(RExC_parse, RExC_end, UTF)) {
         /* Note that the code here assumes well-formed UTF-8.  Skip IDFIRST by
          * using do...while */
	if (UTF)
	    do {
		RExC_parse += UTF8SKIP(RExC_parse);
	    } while (   RExC_parse < RExC_end
                     && isWORDCHAR_utf8_safe((U8*)RExC_parse, (U8*) RExC_end));
	else
	    do {
		RExC_parse++;
	    } while (RExC_parse < RExC_end && isWORDCHAR(*RExC_parse));
    } else {
        RExC_parse++; /* so the <- from the vFAIL is after the offending
                         character */
        vFAIL("Group name must start with a non-digit word character");
    }
    sv_name = newSVpvn_flags(name_start, (int)(RExC_parse - name_start),
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
        if ( ! sv_dat ) {   /* Didn't find group */

            /* It might be a forward reference; we can't fail until we
                * know, by completing the parse to get all the groups, and
                * then reparsing */
            if (ALL_PARENS_COUNTED)  {
                vFAIL("Reference to nonexistent named group");
            }
            else {
                REQUIRE_PARENS_PASS;
            }
        }
        return sv_dat;
    }

    Perl_croak(aTHX_ "panic: bad flag %lx in reg_scan_name",
                     (unsigned long) flags);
}

#define DEBUG_PARSE_MSG(funcname)     DEBUG_PARSE_r({           \
    if (RExC_lastparse!=RExC_parse) {                           \
        Perl_re_printf( aTHX_  "%s",                            \
            Perl_pv_pretty(aTHX_ RExC_mysv1, RExC_parse,        \
                RExC_end - RExC_parse, 16,                      \
                "", "",                                         \
                PERL_PV_ESCAPE_UNI_DETECT |                     \
                PERL_PV_PRETTY_ELLIPSES   |                     \
                PERL_PV_PRETTY_LTGT       |                     \
                PERL_PV_ESCAPE_RE         |                     \
                PERL_PV_PRETTY_EXACTSIZE                        \
            )                                                   \
        );                                                      \
    } else                                                      \
        Perl_re_printf( aTHX_ "%16s","");                       \
                                                                \
    if (RExC_lastnum!=RExC_emit)                                \
       Perl_re_printf( aTHX_ "|%4d", RExC_emit);                \
    else                                                        \
       Perl_re_printf( aTHX_ "|%4s","");                        \
    Perl_re_printf( aTHX_ "|%*s%-4s",                           \
        (int)((depth*2)), "",                                   \
        (funcname)                                              \
    );                                                          \
    RExC_lastnum=RExC_emit;                                     \
    RExC_lastparse=RExC_parse;                                  \
})



#define DEBUG_PARSE(funcname)     DEBUG_PARSE_r({           \
    DEBUG_PARSE_MSG((funcname));                            \
    Perl_re_printf( aTHX_ "%4s","\n");                                  \
})
#define DEBUG_PARSE_FMT(funcname,fmt,args)     DEBUG_PARSE_r({\
    DEBUG_PARSE_MSG((funcname));                            \
    Perl_re_printf( aTHX_ fmt "\n",args);                               \
})

/* This section of code defines the inversion list object and its methods.  The
 * interfaces are highly subject to change, so as much as possible is static to
 * this file.  An inversion list is here implemented as a malloc'd C UV array
 * as an SVt_INVLIST scalar.
 *
 * An inversion list for Unicode is an array of code points, sorted by ordinal
 * number.  Each element gives the code point that begins a range that extends
 * up-to but not including the code point given by the next element.  The final
 * element gives the first code point of a range that extends to the platform's
 * infinity.  The even-numbered elements (invlist[0], invlist[2], invlist[4],
 * ...) give ranges whose code points are all in the inversion list.  We say
 * that those ranges are in the set.  The odd-numbered elements give ranges
 * whose code points are not in the inversion list, and hence not in the set.
 * Thus, element [0] is the first code point in the list.  Element [1]
 * is the first code point beyond that not in the list; and element [2] is the
 * first code point beyond that that is in the list.  In other words, the first
 * range is invlist[0]..(invlist[1]-1), and all code points in that range are
 * in the inversion list.  The second range is invlist[1]..(invlist[2]-1), and
 * all code points in that range are not in the inversion list.  The third
 * range invlist[2]..(invlist[3]-1) gives code points that are in the inversion
 * list, and so forth.  Thus every element whose index is divisible by two
 * gives the beginning of a range that is in the list, and every element whose
 * index is not divisible by two gives the beginning of a range not in the
 * list.  If the final element's index is divisible by two, the inversion list
 * extends to the platform's infinity; otherwise the highest code point in the
 * inversion list is the contents of that element minus 1.
 *
 * A range that contains just a single code point N will look like
 *  invlist[i]   == N
 *  invlist[i+1] == N+1
 *
 * If N is UV_MAX (the highest representable code point on the machine), N+1 is
 * impossible to represent, so element [i+1] is omitted.  The single element
 * inversion list
 *  invlist[0] == UV_MAX
 * contains just UV_MAX, but is interpreted as matching to infinity.
 *
 * Taking the complement (inverting) an inversion list is quite simple, if the
 * first element is 0, remove it; otherwise add a 0 element at the beginning.
 * This implementation reserves an element at the beginning of each inversion
 * list to always contain 0; there is an additional flag in the header which
 * indicates if the list begins at the 0, or is offset to begin at the next
 * element.  This means that the inversion list can be inverted without any
 * copying; just flip the flag.
 *
 * More about inversion lists can be found in "Unicode Demystified"
 * Chapter 13 by Richard Gillam, published by Addison-Wesley.
 *
 * The inversion list data structure is currently implemented as an SV pointing
 * to an array of UVs that the SV thinks are bytes.  This allows us to have an
 * array of UV whose memory management is automatically handled by the existing
 * facilities for SV's.
 *
 * Some of the methods should always be private to the implementation, and some
 * should eventually be made public */

/* The header definitions are in F<invlist_inline.h> */

#ifndef PERL_IN_XSUB_RE

PERL_STATIC_INLINE UV*
S__invlist_array_init(SV* const invlist, const bool will_have_0)
{
    /* Returns a pointer to the first element in the inversion list's array.
     * This is called upon initialization of an inversion list.  Where the
     * array begins depends on whether the list has the code point U+0000 in it
     * or not.  The other parameter tells it whether the code that follows this
     * call is about to put a 0 in the inversion list or not.  The first
     * element is either the element reserved for 0, if TRUE, or the element
     * after it, if FALSE */

    bool* offset = get_invlist_offset_addr(invlist);
    UV* zero_addr = (UV *) SvPVX(invlist);

    PERL_ARGS_ASSERT__INVLIST_ARRAY_INIT;

    /* Must be empty */
    assert(! _invlist_len(invlist));

    *zero_addr = 0;

    /* 1^1 = 0; 1^0 = 1 */
    *offset = 1 ^ will_have_0;
    return zero_addr + *offset;
}

PERL_STATIC_INLINE void
S_invlist_set_len(pTHX_ SV* const invlist, const UV len, const bool offset)
{
    /* Sets the current number of elements stored in the inversion list.
     * Updates SvCUR correspondingly */
    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_INVLIST_SET_LEN;

    assert(is_invlist(invlist));

    SvCUR_set(invlist,
              (len == 0)
               ? 0
               : TO_INTERNAL_SIZE(len + offset));
    assert(SvLEN(invlist) == 0 || SvCUR(invlist) <= SvLEN(invlist));
}

STATIC void
S_invlist_replace_list_destroys_src(pTHX_ SV * dest, SV * src)
{
    /* Replaces the inversion list in 'dest' with the one from 'src'.  It
     * steals the list from 'src', so 'src' is made to have a NULL list.  This
     * is similar to what SvSetMagicSV() would do, if it were implemented on
     * inversion lists, though this routine avoids a copy */

    const UV src_len          = _invlist_len(src);
    const bool src_offset     = *get_invlist_offset_addr(src);
    const STRLEN src_byte_len = SvLEN(src);
    char * array              = SvPVX(src);

    const int oldtainted = TAINT_get;

    PERL_ARGS_ASSERT_INVLIST_REPLACE_LIST_DESTROYS_SRC;

    assert(is_invlist(src));
    assert(is_invlist(dest));
    assert(! invlist_is_iterating(src));
    assert(SvCUR(src) == 0 || SvCUR(src) < SvLEN(src));

    /* Make sure it ends in the right place with a NUL, as our inversion list
     * manipulations aren't careful to keep this true, but sv_usepvn_flags()
     * asserts it */
    array[src_byte_len - 1] = '\0';

    TAINT_NOT;      /* Otherwise it breaks */
    sv_usepvn_flags(dest,
                    (char *) array,
                    src_byte_len - 1,

                    /* This flag is documented to cause a copy to be avoided */
                    SV_HAS_TRAILING_NUL);
    TAINT_set(oldtainted);
    SvPV_set(src, 0);
    SvLEN_set(src, 0);
    SvCUR_set(src, 0);

    /* Finish up copying over the other fields in an inversion list */
    *get_invlist_offset_addr(dest) = src_offset;
    invlist_set_len(dest, src_len, src_offset);
    *get_invlist_previous_index_addr(dest) = 0;
    invlist_iterfinish(dest);
}

PERL_STATIC_INLINE IV*
S_get_invlist_previous_index_addr(SV* invlist)
{
    /* Return the address of the IV that is reserved to hold the cached index
     * */
    PERL_ARGS_ASSERT_GET_INVLIST_PREVIOUS_INDEX_ADDR;

    assert(is_invlist(invlist));

    return &(((XINVLIST*) SvANY(invlist))->prev_index);
}

PERL_STATIC_INLINE IV
S_invlist_previous_index(SV* const invlist)
{
    /* Returns cached index of previous search */

    PERL_ARGS_ASSERT_INVLIST_PREVIOUS_INDEX;

    return *get_invlist_previous_index_addr(invlist);
}

PERL_STATIC_INLINE void
S_invlist_set_previous_index(SV* const invlist, const IV index)
{
    /* Caches <index> for later retrieval */

    PERL_ARGS_ASSERT_INVLIST_SET_PREVIOUS_INDEX;

    assert(index == 0 || index < (int) _invlist_len(invlist));

    *get_invlist_previous_index_addr(invlist) = index;
}

PERL_STATIC_INLINE void
S_invlist_trim(SV* invlist)
{
    /* Free the not currently-being-used space in an inversion list */

    /* But don't free up the space needed for the 0 UV that is always at the
     * beginning of the list, nor the trailing NUL */
    const UV min_size = TO_INTERNAL_SIZE(1) + 1;

    PERL_ARGS_ASSERT_INVLIST_TRIM;

    assert(is_invlist(invlist));

    SvPV_renew(invlist, MAX(min_size, SvCUR(invlist) + 1));
}

PERL_STATIC_INLINE void
S_invlist_clear(pTHX_ SV* invlist)    /* Empty the inversion list */
{
    PERL_ARGS_ASSERT_INVLIST_CLEAR;

    assert(is_invlist(invlist));

    invlist_set_len(invlist, 0, 0);
    invlist_trim(invlist);
}

#endif /* ifndef PERL_IN_XSUB_RE */

PERL_STATIC_INLINE bool
S_invlist_is_iterating(SV* const invlist)
{
    PERL_ARGS_ASSERT_INVLIST_IS_ITERATING;

    return *(get_invlist_iter_addr(invlist)) < (STRLEN) UV_MAX;
}

#ifndef PERL_IN_XSUB_RE

PERL_STATIC_INLINE UV
S_invlist_max(SV* const invlist)
{
    /* Returns the maximum number of elements storable in the inversion list's
     * array, without having to realloc() */

    PERL_ARGS_ASSERT_INVLIST_MAX;

    assert(is_invlist(invlist));

    /* Assumes worst case, in which the 0 element is not counted in the
     * inversion list, so subtracts 1 for that */
    return SvLEN(invlist) == 0  /* This happens under _new_invlist_C_array */
           ? FROM_INTERNAL_SIZE(SvCUR(invlist)) - 1
           : FROM_INTERNAL_SIZE(SvLEN(invlist)) - 1;
}

STATIC void
S_initialize_invlist_guts(pTHX_ SV* invlist, const Size_t initial_size)
{
    PERL_ARGS_ASSERT_INITIALIZE_INVLIST_GUTS;

    /* First 1 is in case the zero element isn't in the list; second 1 is for
     * trailing NUL */
    SvGROW(invlist, TO_INTERNAL_SIZE(initial_size + 1) + 1);
    invlist_set_len(invlist, 0, 0);

    /* Force iterinit() to be used to get iteration to work */
    invlist_iterfinish(invlist);

    *get_invlist_previous_index_addr(invlist) = 0;
}

SV*
Perl__new_invlist(pTHX_ IV initial_size)
{

    /* Return a pointer to a newly constructed inversion list, with enough
     * space to store 'initial_size' elements.  If that number is negative, a
     * system default is used instead */

    SV* new_list;

    if (initial_size < 0) {
	initial_size = 10;
    }

    new_list = newSV_type(SVt_INVLIST);
    initialize_invlist_guts(new_list, initial_size);

    return new_list;
}

SV*
Perl__new_invlist_C_array(pTHX_ const UV* const list)
{
    /* Return a pointer to a newly constructed inversion list, initialized to
     * point to <list>, which has to be in the exact correct inversion list
     * form, including internal fields.  Thus this is a dangerous routine that
     * should not be used in the wrong hands.  The passed in 'list' contains
     * several header fields at the beginning that are not part of the
     * inversion list body proper */

    const STRLEN length = (STRLEN) list[0];
    const UV version_id =          list[1];
    const bool offset   =    cBOOL(list[2]);
#define HEADER_LENGTH 3
    /* If any of the above changes in any way, you must change HEADER_LENGTH
     * (if appropriate) and regenerate INVLIST_VERSION_ID by running
     *      perl -E 'say int(rand 2**31-1)'
     */
#define INVLIST_VERSION_ID 148565664 /* This is a combination of a version and
                                        data structure type, so that one being
                                        passed in can be validated to be an
                                        inversion list of the correct vintage.
                                       */

    SV* invlist = newSV_type(SVt_INVLIST);

    PERL_ARGS_ASSERT__NEW_INVLIST_C_ARRAY;

    if (version_id != INVLIST_VERSION_ID) {
        Perl_croak(aTHX_ "panic: Incorrect version for previously generated inversion list");
    }

    /* The generated array passed in includes header elements that aren't part
     * of the list proper, so start it just after them */
    SvPV_set(invlist, (char *) (list + HEADER_LENGTH));

    SvLEN_set(invlist, 0);  /* Means we own the contents, and the system
			       shouldn't touch it */

    *(get_invlist_offset_addr(invlist)) = offset;

    /* The 'length' passed to us is the physical number of elements in the
     * inversion list.  But if there is an offset the logical number is one
     * less than that */
    invlist_set_len(invlist, length  - offset, offset);

    invlist_set_previous_index(invlist, 0);

    /* Initialize the iteration pointer. */
    invlist_iterfinish(invlist);

    SvREADONLY_on(invlist);

    return invlist;
}

STATIC void
S_invlist_extend(pTHX_ SV* const invlist, const UV new_max)
{
    /* Grow the maximum size of an inversion list */

    PERL_ARGS_ASSERT_INVLIST_EXTEND;

    assert(is_invlist(invlist));

    /* Add one to account for the zero element at the beginning which may not
     * be counted by the calling parameters */
    SvGROW((SV *)invlist, TO_INTERNAL_SIZE(new_max + 1));
}

STATIC void
S__append_range_to_invlist(pTHX_ SV* const invlist,
                                 const UV start, const UV end)
{
   /* Subject to change or removal.  Append the range from 'start' to 'end' at
    * the end of the inversion list.  The range must be above any existing
    * ones. */

    UV* array;
    UV max = invlist_max(invlist);
    UV len = _invlist_len(invlist);
    bool offset;

    PERL_ARGS_ASSERT__APPEND_RANGE_TO_INVLIST;

    if (len == 0) { /* Empty lists must be initialized */
        offset = start != 0;
        array = _invlist_array_init(invlist, ! offset);
    }
    else {
	/* Here, the existing list is non-empty. The current max entry in the
	 * list is generally the first value not in the set, except when the
	 * set extends to the end of permissible values, in which case it is
	 * the first entry in that final set, and so this call is an attempt to
	 * append out-of-order */

	UV final_element = len - 1;
	array = invlist_array(invlist);
	if (   array[final_element] > start
	    || ELEMENT_RANGE_MATCHES_INVLIST(final_element))
	{
	    Perl_croak(aTHX_ "panic: attempting to append to an inversion list, but wasn't at the end of the list, final=%" UVuf ", start=%" UVuf ", match=%c",
		     array[final_element], start,
		     ELEMENT_RANGE_MATCHES_INVLIST(final_element) ? 't' : 'f');
	}

        /* Here, it is a legal append.  If the new range begins 1 above the end
         * of the range below it, it is extending the range below it, so the
         * new first value not in the set is one greater than the newly
         * extended range.  */
        offset = *get_invlist_offset_addr(invlist);
	if (array[final_element] == start) {
	    if (end != UV_MAX) {
		array[final_element] = end + 1;
	    }
	    else {
		/* But if the end is the maximum representable on the machine,
                 * assume that infinity was actually what was meant.  Just let
                 * the range that this would extend to have no end */
		invlist_set_len(invlist, len - 1, offset);
	    }
	    return;
	}
    }

    /* Here the new range doesn't extend any existing set.  Add it */

    len += 2;	/* Includes an element each for the start and end of range */

    /* If wll overflow the existing space, extend, which may cause the array to
     * be moved */
    if (max < len) {
	invlist_extend(invlist, len);

        /* Have to set len here to avoid assert failure in invlist_array() */
        invlist_set_len(invlist, len, offset);

	array = invlist_array(invlist);
    }
    else {
	invlist_set_len(invlist, len, offset);
    }

    /* The next item on the list starts the range, the one after that is
     * one past the new range.  */
    array[len - 2] = start;
    if (end != UV_MAX) {
	array[len - 1] = end + 1;
    }
    else {
	/* But if the end is the maximum representable on the machine, just let
	 * the range have no end */
	invlist_set_len(invlist, len - 1, offset);
    }
}

SSize_t
Perl__invlist_search(SV* const invlist, const UV cp)
{
    /* Searches the inversion list for the entry that contains the input code
     * point <cp>.  If <cp> is not in the list, -1 is returned.  Otherwise, the
     * return value is the index into the list's array of the range that
     * contains <cp>, that is, 'i' such that
     *	array[i] <= cp < array[i+1]
     */

    IV low = 0;
    IV mid;
    IV high = _invlist_len(invlist);
    const IV highest_element = high - 1;
    const UV* array;

    PERL_ARGS_ASSERT__INVLIST_SEARCH;

    /* If list is empty, return failure. */
    if (high == 0) {
	return -1;
    }

    /* (We can't get the array unless we know the list is non-empty) */
    array = invlist_array(invlist);

    mid = invlist_previous_index(invlist);
    assert(mid >=0);
    if (mid > highest_element) {
        mid = highest_element;
    }

    /* <mid> contains the cache of the result of the previous call to this
     * function (0 the first time).  See if this call is for the same result,
     * or if it is for mid-1.  This is under the theory that calls to this
     * function will often be for related code points that are near each other.
     * And benchmarks show that caching gives better results.  We also test
     * here if the code point is within the bounds of the list.  These tests
     * replace others that would have had to be made anyway to make sure that
     * the array bounds were not exceeded, and these give us extra information
     * at the same time */
    if (cp >= array[mid]) {
        if (cp >= array[highest_element]) {
            return highest_element;
        }

        /* Here, array[mid] <= cp < array[highest_element].  This means that
         * the final element is not the answer, so can exclude it; it also
         * means that <mid> is not the final element, so can refer to 'mid + 1'
         * safely */
        if (cp < array[mid + 1]) {
            return mid;
        }
        high--;
        low = mid + 1;
    }
    else { /* cp < aray[mid] */
        if (cp < array[0]) { /* Fail if outside the array */
            return -1;
        }
        high = mid;
        if (cp >= array[mid - 1]) {
            goto found_entry;
        }
    }

    /* Binary search.  What we are looking for is <i> such that
     *	array[i] <= cp < array[i+1]
     * The loop below converges on the i+1.  Note that there may not be an
     * (i+1)th element in the array, and things work nonetheless */
    while (low < high) {
	mid = (low + high) / 2;
        assert(mid <= highest_element);
	if (array[mid] <= cp) { /* cp >= array[mid] */
	    low = mid + 1;

	    /* We could do this extra test to exit the loop early.
	    if (cp < array[low]) {
		return mid;
	    }
	    */
	}
	else { /* cp < array[mid] */
	    high = mid;
	}
    }

  found_entry:
    high--;
    invlist_set_previous_index(invlist, high);
    return high;
}

void
Perl__invlist_union_maybe_complement_2nd(pTHX_ SV* const a, SV* const b,
                                         const bool complement_b, SV** output)
{
    /* Take the union of two inversion lists and point '*output' to it.  On
     * input, '*output' MUST POINT TO NULL OR TO AN SV* INVERSION LIST (possibly
     * even 'a' or 'b').  If to an inversion list, the contents of the original
     * list will be replaced by the union.  The first list, 'a', may be
     * NULL, in which case a copy of the second list is placed in '*output'.
     * If 'complement_b' is TRUE, the union is taken of the complement
     * (inversion) of 'b' instead of b itself.
     *
     * The basis for this comes from "Unicode Demystified" Chapter 13 by
     * Richard Gillam, published by Addison-Wesley, and explained at some
     * length there.  The preface says to incorporate its examples into your
     * code at your own risk.
     *
     * The algorithm is like a merge sort. */

    const UV* array_a;    /* a's array */
    const UV* array_b;
    UV len_a;	    /* length of a's array */
    UV len_b;

    SV* u;			/* the resulting union */
    UV* array_u;
    UV len_u = 0;

    UV i_a = 0;		    /* current index into a's array */
    UV i_b = 0;
    UV i_u = 0;

    /* running count, as explained in the algorithm source book; items are
     * stopped accumulating and are output when the count changes to/from 0.
     * The count is incremented when we start a range that's in an input's set,
     * and decremented when we start a range that's not in a set.  So this
     * variable can be 0, 1, or 2.  When it is 0 neither input is in their set,
     * and hence nothing goes into the union; 1, just one of the inputs is in
     * its set (and its current range gets added to the union); and 2 when both
     * inputs are in their sets.  */
    UV count = 0;

    PERL_ARGS_ASSERT__INVLIST_UNION_MAYBE_COMPLEMENT_2ND;
    assert(a != b);
    assert(*output == NULL || is_invlist(*output));

    len_b = _invlist_len(b);
    if (len_b == 0) {

        /* Here, 'b' is empty, hence it's complement is all possible code
         * points.  So if the union includes the complement of 'b', it includes
         * everything, and we need not even look at 'a'.  It's easiest to
         * create a new inversion list that matches everything.  */
        if (complement_b) {
            SV* everything = _add_range_to_invlist(NULL, 0, UV_MAX);

            if (*output == NULL) { /* If the output didn't exist, just point it
                                      at the new list */
                *output = everything;
            }
            else { /* Otherwise, replace its contents with the new list */
                invlist_replace_list_destroys_src(*output, everything);
                SvREFCNT_dec_NN(everything);
            }

            return;
        }

        /* Here, we don't want the complement of 'b', and since 'b' is empty,
         * the union will come entirely from 'a'.  If 'a' is NULL or empty, the
         * output will be empty */

        if (a == NULL || _invlist_len(a) == 0) {
            if (*output == NULL) {
                *output = _new_invlist(0);
            }
            else {
                invlist_clear(*output);
            }
            return;
        }

        /* Here, 'a' is not empty, but 'b' is, so 'a' entirely determines the
         * union.  We can just return a copy of 'a' if '*output' doesn't point
         * to an existing list */
        if (*output == NULL) {
            *output = invlist_clone(a, NULL);
            return;
        }

        /* If the output is to overwrite 'a', we have a no-op, as it's
         * already in 'a' */
        if (*output == a) {
            return;
        }

        /* Here, '*output' is to be overwritten by 'a' */
        u = invlist_clone(a, NULL);
        invlist_replace_list_destroys_src(*output, u);
        SvREFCNT_dec_NN(u);

        return;
    }

    /* Here 'b' is not empty.  See about 'a' */

    if (a == NULL || ((len_a = _invlist_len(a)) == 0)) {

        /* Here, 'a' is empty (and b is not).  That means the union will come
         * entirely from 'b'.  If '*output' is NULL, we can directly return a
         * clone of 'b'.  Otherwise, we replace the contents of '*output' with
         * the clone */

        SV ** dest = (*output == NULL) ? output : &u;
        *dest = invlist_clone(b, NULL);
        if (complement_b) {
            _invlist_invert(*dest);
        }

        if (dest == &u) {
            invlist_replace_list_destroys_src(*output, u);
            SvREFCNT_dec_NN(u);
        }

	return;
    }

    /* Here both lists exist and are non-empty */
    array_a = invlist_array(a);
    array_b = invlist_array(b);

    /* If are to take the union of 'a' with the complement of b, set it
     * up so are looking at b's complement. */
    if (complement_b) {

	/* To complement, we invert: if the first element is 0, remove it.  To
	 * do this, we just pretend the array starts one later */
        if (array_b[0] == 0) {
            array_b++;
            len_b--;
        }
        else {

            /* But if the first element is not zero, we pretend the list starts
             * at the 0 that is always stored immediately before the array. */
            array_b--;
            len_b++;
        }
    }

    /* Size the union for the worst case: that the sets are completely
     * disjoint */
    u = _new_invlist(len_a + len_b);

    /* Will contain U+0000 if either component does */
    array_u = _invlist_array_init(u, (    len_a > 0 && array_a[0] == 0)
                                      || (len_b > 0 && array_b[0] == 0));

    /* Go through each input list item by item, stopping when have exhausted
     * one of them */
    while (i_a < len_a && i_b < len_b) {
	UV cp;	    /* The element to potentially add to the union's array */
	bool cp_in_set;   /* is it in the the input list's set or not */

	/* We need to take one or the other of the two inputs for the union.
	 * Since we are merging two sorted lists, we take the smaller of the
         * next items.  In case of a tie, we take first the one that is in its
         * set.  If we first took the one not in its set, it would decrement
         * the count, possibly to 0 which would cause it to be output as ending
         * the range, and the next time through we would take the same number,
         * and output it again as beginning the next range.  By doing it the
         * opposite way, there is no possibility that the count will be
         * momentarily decremented to 0, and thus the two adjoining ranges will
         * be seamlessly merged.  (In a tie and both are in the set or both not
         * in the set, it doesn't matter which we take first.) */
	if (       array_a[i_a] < array_b[i_b]
	    || (   array_a[i_a] == array_b[i_b]
		&& ELEMENT_RANGE_MATCHES_INVLIST(i_a)))
	{
	    cp_in_set = ELEMENT_RANGE_MATCHES_INVLIST(i_a);
	    cp = array_a[i_a++];
	}
	else {
	    cp_in_set = ELEMENT_RANGE_MATCHES_INVLIST(i_b);
	    cp = array_b[i_b++];
	}

	/* Here, have chosen which of the two inputs to look at.  Only output
	 * if the running count changes to/from 0, which marks the
	 * beginning/end of a range that's in the set */
	if (cp_in_set) {
	    if (count == 0) {
		array_u[i_u++] = cp;
	    }
	    count++;
	}
	else {
	    count--;
	    if (count == 0) {
		array_u[i_u++] = cp;
	    }
	}
    }


    /* The loop above increments the index into exactly one of the input lists
     * each iteration, and ends when either index gets to its list end.  That
     * means the other index is lower than its end, and so something is
     * remaining in that one.  We decrement 'count', as explained below, if
     * that list is in its set.  (i_a and i_b each currently index the element
     * beyond the one we care about.) */
    if (   (i_a != len_a && PREV_RANGE_MATCHES_INVLIST(i_a))
	|| (i_b != len_b && PREV_RANGE_MATCHES_INVLIST(i_b)))
    {
	count--;
    }

    /* Above we decremented 'count' if the list that had unexamined elements in
     * it was in its set.  This has made it so that 'count' being non-zero
     * means there isn't anything left to output; and 'count' equal to 0 means
     * that what is left to output is precisely that which is left in the
     * non-exhausted input list.
     *
     * To see why, note first that the exhausted input obviously has nothing
     * left to add to the union.  If it was in its set at its end, that means
     * the set extends from here to the platform's infinity, and hence so does
     * the union and the non-exhausted set is irrelevant.  The exhausted set
     * also contributed 1 to 'count'.  If 'count' was 2, it got decremented to
     * 1, but if it was 1, the non-exhausted set wasn't in its set, and so
     * 'count' remains at 1.  This is consistent with the decremented 'count'
     * != 0 meaning there's nothing left to add to the union.
     *
     * But if the exhausted input wasn't in its set, it contributed 0 to
     * 'count', and the rest of the union will be whatever the other input is.
     * If 'count' was 0, neither list was in its set, and 'count' remains 0;
     * otherwise it gets decremented to 0.  This is consistent with 'count'
     * == 0 meaning the remainder of the union is whatever is left in the
     * non-exhausted list. */
    if (count != 0) {
        len_u = i_u;
    }
    else {
        IV copy_count = len_a - i_a;
        if (copy_count > 0) {   /* The non-exhausted input is 'a' */
	    Copy(array_a + i_a, array_u + i_u, copy_count, UV);
        }
        else { /* The non-exhausted input is b */
            copy_count = len_b - i_b;
	    Copy(array_b + i_b, array_u + i_u, copy_count, UV);
        }
        len_u = i_u + copy_count;
    }

    /* Set the result to the final length, which can change the pointer to
     * array_u, so re-find it.  (Note that it is unlikely that this will
     * change, as we are shrinking the space, not enlarging it) */
    if (len_u != _invlist_len(u)) {
	invlist_set_len(u, len_u, *get_invlist_offset_addr(u));
	invlist_trim(u);
	array_u = invlist_array(u);
    }

    if (*output == NULL) {  /* Simply return the new inversion list */
        *output = u;
    }
    else {
        /* Otherwise, overwrite the inversion list that was in '*output'.  We
         * could instead free '*output', and then set it to 'u', but experience
         * has shown [perl #127392] that if the input is a mortal, we can get a
         * huge build-up of these during regex compilation before they get
         * freed. */
        invlist_replace_list_destroys_src(*output, u);
        SvREFCNT_dec_NN(u);
    }

    return;
}

void
Perl__invlist_intersection_maybe_complement_2nd(pTHX_ SV* const a, SV* const b,
                                               const bool complement_b, SV** i)
{
    /* Take the intersection of two inversion lists and point '*i' to it.  On
     * input, '*i' MUST POINT TO NULL OR TO AN SV* INVERSION LIST (possibly
     * even 'a' or 'b').  If to an inversion list, the contents of the original
     * list will be replaced by the intersection.  The first list, 'a', may be
     * NULL, in which case '*i' will be an empty list.  If 'complement_b' is
     * TRUE, the result will be the intersection of 'a' and the complement (or
     * inversion) of 'b' instead of 'b' directly.
     *
     * The basis for this comes from "Unicode Demystified" Chapter 13 by
     * Richard Gillam, published by Addison-Wesley, and explained at some
     * length there.  The preface says to incorporate its examples into your
     * code at your own risk.  In fact, it had bugs
     *
     * The algorithm is like a merge sort, and is essentially the same as the
     * union above
     */

    const UV* array_a;		/* a's array */
    const UV* array_b;
    UV len_a;	/* length of a's array */
    UV len_b;

    SV* r;		     /* the resulting intersection */
    UV* array_r;
    UV len_r = 0;

    UV i_a = 0;		    /* current index into a's array */
    UV i_b = 0;
    UV i_r = 0;

    /* running count of how many of the two inputs are postitioned at ranges
     * that are in their sets.  As explained in the algorithm source book,
     * items are stopped accumulating and are output when the count changes
     * to/from 2.  The count is incremented when we start a range that's in an
     * input's set, and decremented when we start a range that's not in a set.
     * Only when it is 2 are we in the intersection. */
    UV count = 0;

    PERL_ARGS_ASSERT__INVLIST_INTERSECTION_MAYBE_COMPLEMENT_2ND;
    assert(a != b);
    assert(*i == NULL || is_invlist(*i));

    /* Special case if either one is empty */
    len_a = (a == NULL) ? 0 : _invlist_len(a);
    if ((len_a == 0) || ((len_b = _invlist_len(b)) == 0)) {
        if (len_a != 0 && complement_b) {

            /* Here, 'a' is not empty, therefore from the enclosing 'if', 'b'
             * must be empty.  Here, also we are using 'b's complement, which
             * hence must be every possible code point.  Thus the intersection
             * is simply 'a'. */

            if (*i == a) {  /* No-op */
                return;
            }

            if (*i == NULL) {
                *i = invlist_clone(a, NULL);
                return;
            }

            r = invlist_clone(a, NULL);
            invlist_replace_list_destroys_src(*i, r);
            SvREFCNT_dec_NN(r);
            return;
        }

        /* Here, 'a' or 'b' is empty and not using the complement of 'b'.  The
         * intersection must be empty */
        if (*i == NULL) {
            *i = _new_invlist(0);
            return;
        }

        invlist_clear(*i);
	return;
    }

    /* Here both lists exist and are non-empty */
    array_a = invlist_array(a);
    array_b = invlist_array(b);

    /* If are to take the intersection of 'a' with the complement of b, set it
     * up so are looking at b's complement. */
    if (complement_b) {

	/* To complement, we invert: if the first element is 0, remove it.  To
	 * do this, we just pretend the array starts one later */
        if (array_b[0] == 0) {
            array_b++;
            len_b--;
        }
        else {

            /* But if the first element is not zero, we pretend the list starts
             * at the 0 that is always stored immediately before the array. */
            array_b--;
            len_b++;
        }
    }

    /* Size the intersection for the worst case: that the intersection ends up
     * fragmenting everything to be completely disjoint */
    r= _new_invlist(len_a + len_b);

    /* Will contain U+0000 iff both components do */
    array_r = _invlist_array_init(r,    len_a > 0 && array_a[0] == 0
                                     && len_b > 0 && array_b[0] == 0);

    /* Go through each list item by item, stopping when have exhausted one of
     * them */
    while (i_a < len_a && i_b < len_b) {
	UV cp;	    /* The element to potentially add to the intersection's
		       array */
	bool cp_in_set;	/* Is it in the input list's set or not */

	/* We need to take one or the other of the two inputs for the
	 * intersection.  Since we are merging two sorted lists, we take the
         * smaller of the next items.  In case of a tie, we take first the one
         * that is not in its set (a difference from the union algorithm).  If
         * we first took the one in its set, it would increment the count,
         * possibly to 2 which would cause it to be output as starting a range
         * in the intersection, and the next time through we would take that
         * same number, and output it again as ending the set.  By doing the
         * opposite of this, there is no possibility that the count will be
         * momentarily incremented to 2.  (In a tie and both are in the set or
         * both not in the set, it doesn't matter which we take first.) */
	if (       array_a[i_a] < array_b[i_b]
	    || (   array_a[i_a] == array_b[i_b]
		&& ! ELEMENT_RANGE_MATCHES_INVLIST(i_a)))
	{
	    cp_in_set = ELEMENT_RANGE_MATCHES_INVLIST(i_a);
	    cp = array_a[i_a++];
	}
	else {
	    cp_in_set = ELEMENT_RANGE_MATCHES_INVLIST(i_b);
	    cp= array_b[i_b++];
	}

	/* Here, have chosen which of the two inputs to look at.  Only output
	 * if the running count changes to/from 2, which marks the
	 * beginning/end of a range that's in the intersection */
	if (cp_in_set) {
	    count++;
	    if (count == 2) {
		array_r[i_r++] = cp;
	    }
	}
	else {
	    if (count == 2) {
		array_r[i_r++] = cp;
	    }
	    count--;
	}

    }

    /* The loop above increments the index into exactly one of the input lists
     * each iteration, and ends when either index gets to its list end.  That
     * means the other index is lower than its end, and so something is
     * remaining in that one.  We increment 'count', as explained below, if the
     * exhausted list was in its set.  (i_a and i_b each currently index the
     * element beyond the one we care about.) */
    if (   (i_a == len_a && PREV_RANGE_MATCHES_INVLIST(i_a))
        || (i_b == len_b && PREV_RANGE_MATCHES_INVLIST(i_b)))
    {
	count++;
    }

    /* Above we incremented 'count' if the exhausted list was in its set.  This
     * has made it so that 'count' being below 2 means there is nothing left to
     * output; otheriwse what's left to add to the intersection is precisely
     * that which is left in the non-exhausted input list.
     *
     * To see why, note first that the exhausted input obviously has nothing
     * left to affect the intersection.  If it was in its set at its end, that
     * means the set extends from here to the platform's infinity, and hence
     * anything in the non-exhausted's list will be in the intersection, and
     * anything not in it won't be.  Hence, the rest of the intersection is
     * precisely what's in the non-exhausted list  The exhausted set also
     * contributed 1 to 'count', meaning 'count' was at least 1.  Incrementing
     * it means 'count' is now at least 2.  This is consistent with the
     * incremented 'count' being >= 2 means to add the non-exhausted list to
     * the intersection.
     *
     * But if the exhausted input wasn't in its set, it contributed 0 to
     * 'count', and the intersection can't include anything further; the
     * non-exhausted set is irrelevant.  'count' was at most 1, and doesn't get
     * incremented.  This is consistent with 'count' being < 2 meaning nothing
     * further to add to the intersection. */
    if (count < 2) { /* Nothing left to put in the intersection. */
        len_r = i_r;
    }
    else { /* copy the non-exhausted list, unchanged. */
        IV copy_count = len_a - i_a;
        if (copy_count > 0) {   /* a is the one with stuff left */
	    Copy(array_a + i_a, array_r + i_r, copy_count, UV);
        }
        else {  /* b is the one with stuff left */
            copy_count = len_b - i_b;
	    Copy(array_b + i_b, array_r + i_r, copy_count, UV);
        }
        len_r = i_r + copy_count;
    }

    /* Set the result to the final length, which can change the pointer to
     * array_r, so re-find it.  (Note that it is unlikely that this will
     * change, as we are shrinking the space, not enlarging it) */
    if (len_r != _invlist_len(r)) {
	invlist_set_len(r, len_r, *get_invlist_offset_addr(r));
	invlist_trim(r);
	array_r = invlist_array(r);
    }

    if (*i == NULL) { /* Simply return the calculated intersection */
        *i = r;
    }
    else { /* Otherwise, replace the existing inversion list in '*i'.  We could
              instead free '*i', and then set it to 'r', but experience has
              shown [perl #127392] that if the input is a mortal, we can get a
              huge build-up of these during regex compilation before they get
              freed. */
        if (len_r) {
            invlist_replace_list_destroys_src(*i, r);
        }
        else {
            invlist_clear(*i);
        }
        SvREFCNT_dec_NN(r);
    }

    return;
}

SV*
Perl__add_range_to_invlist(pTHX_ SV* invlist, UV start, UV end)
{
    /* Add the range from 'start' to 'end' inclusive to the inversion list's
     * set.  A pointer to the inversion list is returned.  This may actually be
     * a new list, in which case the passed in one has been destroyed.  The
     * passed-in inversion list can be NULL, in which case a new one is created
     * with just the one range in it.  The new list is not necessarily
     * NUL-terminated.  Space is not freed if the inversion list shrinks as a
     * result of this function.  The gain would not be large, and in many
     * cases, this is called multiple times on a single inversion list, so
     * anything freed may almost immediately be needed again.
     *
     * This used to mostly call the 'union' routine, but that is much more
     * heavyweight than really needed for a single range addition */

    UV* array;              /* The array implementing the inversion list */
    UV len;                 /* How many elements in 'array' */
    SSize_t i_s;            /* index into the invlist array where 'start'
                               should go */
    SSize_t i_e = 0;        /* And the index where 'end' should go */
    UV cur_highest;         /* The highest code point in the inversion list
                               upon entry to this function */

    /* This range becomes the whole inversion list if none already existed */
    if (invlist == NULL) {
	invlist = _new_invlist(2);
        _append_range_to_invlist(invlist, start, end);
        return invlist;
    }

    /* Likewise, if the inversion list is currently empty */
    len = _invlist_len(invlist);
    if (len == 0) {
        _append_range_to_invlist(invlist, start, end);
        return invlist;
    }

    /* Starting here, we have to know the internals of the list */
    array = invlist_array(invlist);

    /* If the new range ends higher than the current highest ... */
    cur_highest = invlist_highest(invlist);
    if (end > cur_highest) {

        /* If the whole range is higher, we can just append it */
        if (start > cur_highest) {
            _append_range_to_invlist(invlist, start, end);
            return invlist;
        }

        /* Otherwise, add the portion that is higher ... */
        _append_range_to_invlist(invlist, cur_highest + 1, end);

        /* ... and continue on below to handle the rest.  As a result of the
         * above append, we know that the index of the end of the range is the
         * final even numbered one of the array.  Recall that the final element
         * always starts a range that extends to infinity.  If that range is in
         * the set (meaning the set goes from here to infinity), it will be an
         * even index, but if it isn't in the set, it's odd, and the final
         * range in the set is one less, which is even. */
        if (end == UV_MAX) {
            i_e = len;
        }
        else {
            i_e = len - 2;
        }
    }

    /* We have dealt with appending, now see about prepending.  If the new
     * range starts lower than the current lowest ... */
    if (start < array[0]) {

        /* Adding something which has 0 in it is somewhat tricky, and uncommon.
         * Let the union code handle it, rather than having to know the
         * trickiness in two code places.  */
        if (UNLIKELY(start == 0)) {
            SV* range_invlist;

            range_invlist = _new_invlist(2);
            _append_range_to_invlist(range_invlist, start, end);

            _invlist_union(invlist, range_invlist, &invlist);

            SvREFCNT_dec_NN(range_invlist);

            return invlist;
        }

        /* If the whole new range comes before the first entry, and doesn't
         * extend it, we have to insert it as an additional range */
        if (end < array[0] - 1) {
            i_s = i_e = -1;
            goto splice_in_new_range;
        }

        /* Here the new range adjoins the existing first range, extending it
         * downwards. */
        array[0] = start;

        /* And continue on below to handle the rest.  We know that the index of
         * the beginning of the range is the first one of the array */
        i_s = 0;
    }
    else { /* Not prepending any part of the new range to the existing list.
            * Find where in the list it should go.  This finds i_s, such that:
            *     invlist[i_s] <= start < array[i_s+1]
            */
        i_s = _invlist_search(invlist, start);
    }

    /* At this point, any extending before the beginning of the inversion list
     * and/or after the end has been done.  This has made it so that, in the
     * code below, each endpoint of the new range is either in a range that is
     * in the set, or is in a gap between two ranges that are.  This means we
     * don't have to worry about exceeding the array bounds.
     *
     * Find where in the list the new range ends (but we can skip this if we
     * have already determined what it is, or if it will be the same as i_s,
     * which we already have computed) */
    if (i_e == 0) {
        i_e = (start == end)
              ? i_s
              : _invlist_search(invlist, end);
    }

    /* Here generally invlist[i_e] <= end < array[i_e+1].  But if invlist[i_e]
     * is a range that goes to infinity there is no element at invlist[i_e+1],
     * so only the first relation holds. */

    if ( ! ELEMENT_RANGE_MATCHES_INVLIST(i_s)) {

        /* Here, the ranges on either side of the beginning of the new range
         * are in the set, and this range starts in the gap between them.
         *
         * The new range extends the range above it downwards if the new range
         * ends at or above that range's start */
        const bool extends_the_range_above = (   end == UV_MAX
                                              || end + 1 >= array[i_s+1]);

        /* The new range extends the range below it upwards if it begins just
         * after where that range ends */
        if (start == array[i_s]) {

            /* If the new range fills the entire gap between the other ranges,
             * they will get merged together.  Other ranges may also get
             * merged, depending on how many of them the new range spans.  In
             * the general case, we do the merge later, just once, after we
             * figure out how many to merge.  But in the case where the new
             * range exactly spans just this one gap (possibly extending into
             * the one above), we do the merge here, and an early exit.  This
             * is done here to avoid having to special case later. */
            if (i_e - i_s <= 1) {

                /* If i_e - i_s == 1, it means that the new range terminates
                 * within the range above, and hence 'extends_the_range_above'
                 * must be true.  (If the range above it extends to infinity,
                 * 'i_s+2' will be above the array's limit, but 'len-i_s-2'
                 * will be 0, so no harm done.) */
                if (extends_the_range_above) {
                    Move(array + i_s + 2, array + i_s, len - i_s - 2, UV);
                    invlist_set_len(invlist,
                                    len - 2,
                                    *(get_invlist_offset_addr(invlist)));
                    return invlist;
                }

                /* Here, i_e must == i_s.  We keep them in sync, as they apply
                 * to the same range, and below we are about to decrement i_s
                 * */
                i_e--;
            }

            /* Here, the new range is adjacent to the one below.  (It may also
             * span beyond the range above, but that will get resolved later.)
             * Extend the range below to include this one. */
            array[i_s] = (end == UV_MAX) ? UV_MAX : end + 1;
            i_s--;
            start = array[i_s];
        }
        else if (extends_the_range_above) {

            /* Here the new range only extends the range above it, but not the
             * one below.  It merges with the one above.  Again, we keep i_e
             * and i_s in sync if they point to the same range */
            if (i_e == i_s) {
                i_e++;
            }
            i_s++;
            array[i_s] = start;
        }
    }

    /* Here, we've dealt with the new range start extending any adjoining
     * existing ranges.
     *
     * If the new range extends to infinity, it is now the final one,
     * regardless of what was there before */
    if (UNLIKELY(end == UV_MAX)) {
        invlist_set_len(invlist, i_s + 1, *(get_invlist_offset_addr(invlist)));
        return invlist;
    }

    /* If i_e started as == i_s, it has also been dealt with,
     * and been updated to the new i_s, which will fail the following if */
    if (! ELEMENT_RANGE_MATCHES_INVLIST(i_e)) {

        /* Here, the ranges on either side of the end of the new range are in
         * the set, and this range ends in the gap between them.
         *
         * If this range is adjacent to (hence extends) the range above it, it
         * becomes part of that range; likewise if it extends the range below,
         * it becomes part of that range */
        if (end + 1 == array[i_e+1]) {
            i_e++;
            array[i_e] = start;
        }
        else if (start <= array[i_e]) {
            array[i_e] = end + 1;
            i_e--;
        }
    }

    if (i_s == i_e) {

        /* If the range fits entirely in an existing range (as possibly already
         * extended above), it doesn't add anything new */
        if (ELEMENT_RANGE_MATCHES_INVLIST(i_s)) {
            return invlist;
        }

        /* Here, no part of the range is in the list.  Must add it.  It will
         * occupy 2 more slots */
      splice_in_new_range:

        invlist_extend(invlist, len + 2);
        array = invlist_array(invlist);
        /* Move the rest of the array down two slots. Don't include any
         * trailing NUL */
        Move(array + i_e + 1, array + i_e + 3, len - i_e - 1, UV);

        /* Do the actual splice */
        array[i_e+1] = start;
        array[i_e+2] = end + 1;
        invlist_set_len(invlist, len + 2, *(get_invlist_offset_addr(invlist)));
        return invlist;
    }

    /* Here the new range crossed the boundaries of a pre-existing range.  The
     * code above has adjusted things so that both ends are in ranges that are
     * in the set.  This means everything in between must also be in the set.
     * Just squash things together */
    Move(array + i_e + 1, array + i_s + 1, len - i_e - 1, UV);
    invlist_set_len(invlist,
                    len - i_e + i_s,
                    *(get_invlist_offset_addr(invlist)));

    return invlist;
}

SV*
Perl__setup_canned_invlist(pTHX_ const STRLEN size, const UV element0,
                                 UV** other_elements_ptr)
{
    /* Create and return an inversion list whose contents are to be populated
     * by the caller.  The caller gives the number of elements (in 'size') and
     * the very first element ('element0').  This function will set
     * '*other_elements_ptr' to an array of UVs, where the remaining elements
     * are to be placed.
     *
     * Obviously there is some trust involved that the caller will properly
     * fill in the other elements of the array.
     *
     * (The first element needs to be passed in, as the underlying code does
     * things differently depending on whether it is zero or non-zero) */

    SV* invlist = _new_invlist(size);
    bool offset;

    PERL_ARGS_ASSERT__SETUP_CANNED_INVLIST;

    invlist = add_cp_to_invlist(invlist, element0);
    offset = *get_invlist_offset_addr(invlist);

    invlist_set_len(invlist, size, offset);
    *other_elements_ptr = invlist_array(invlist) + 1;
    return invlist;
}

#endif

PERL_STATIC_INLINE SV*
S_add_cp_to_invlist(pTHX_ SV* invlist, const UV cp) {
    return _add_range_to_invlist(invlist, cp, cp);
}

#ifndef PERL_IN_XSUB_RE
void
Perl__invlist_invert(pTHX_ SV* const invlist)
{
    /* Complement the input inversion list.  This adds a 0 if the list didn't
     * have a zero; removes it otherwise.  As described above, the data
     * structure is set up so that this is very efficient */

    PERL_ARGS_ASSERT__INVLIST_INVERT;

    assert(! invlist_is_iterating(invlist));

    /* The inverse of matching nothing is matching everything */
    if (_invlist_len(invlist) == 0) {
	_append_range_to_invlist(invlist, 0, UV_MAX);
	return;
    }

    *get_invlist_offset_addr(invlist) = ! *get_invlist_offset_addr(invlist);
}

SV*
Perl_invlist_clone(pTHX_ SV* const invlist, SV* new_invlist)
{
    /* Return a new inversion list that is a copy of the input one, which is
     * unchanged.  The new list will not be mortal even if the old one was. */

    const STRLEN nominal_length = _invlist_len(invlist);
    const STRLEN physical_length = SvCUR(invlist);
    const bool offset = *(get_invlist_offset_addr(invlist));

    PERL_ARGS_ASSERT_INVLIST_CLONE;

    if (new_invlist == NULL) {
        new_invlist = _new_invlist(nominal_length);
    }
    else {
        sv_upgrade(new_invlist, SVt_INVLIST);
        initialize_invlist_guts(new_invlist, nominal_length);
    }

    *(get_invlist_offset_addr(new_invlist)) = offset;
    invlist_set_len(new_invlist, nominal_length, offset);
    Copy(SvPVX(invlist), SvPVX(new_invlist), physical_length, char);

    return new_invlist;
}

#endif

PERL_STATIC_INLINE STRLEN*
S_get_invlist_iter_addr(SV* invlist)
{
    /* Return the address of the UV that contains the current iteration
     * position */

    PERL_ARGS_ASSERT_GET_INVLIST_ITER_ADDR;

    assert(is_invlist(invlist));

    return &(((XINVLIST*) SvANY(invlist))->iterator);
}

PERL_STATIC_INLINE void
S_invlist_iterinit(SV* invlist)	/* Initialize iterator for invlist */
{
    PERL_ARGS_ASSERT_INVLIST_ITERINIT;

    *get_invlist_iter_addr(invlist) = 0;
}

PERL_STATIC_INLINE void
S_invlist_iterfinish(SV* invlist)
{
    /* Terminate iterator for invlist.  This is to catch development errors.
     * Any iteration that is interrupted before completed should call this
     * function.  Functions that add code points anywhere else but to the end
     * of an inversion list assert that they are not in the middle of an
     * iteration.  If they were, the addition would make the iteration
     * problematical: if the iteration hadn't reached the place where things
     * were being added, it would be ok */

    PERL_ARGS_ASSERT_INVLIST_ITERFINISH;

    *get_invlist_iter_addr(invlist) = (STRLEN) UV_MAX;
}

STATIC bool
S_invlist_iternext(SV* invlist, UV* start, UV* end)
{
    /* An C<invlist_iterinit> call on <invlist> must be used to set this up.
     * This call sets in <*start> and <*end>, the next range in <invlist>.
     * Returns <TRUE> if successful and the next call will return the next
     * range; <FALSE> if was already at the end of the list.  If the latter,
     * <*start> and <*end> are unchanged, and the next call to this function
     * will start over at the beginning of the list */

    STRLEN* pos = get_invlist_iter_addr(invlist);
    UV len = _invlist_len(invlist);
    UV *array;

    PERL_ARGS_ASSERT_INVLIST_ITERNEXT;

    if (*pos >= len) {
	*pos = (STRLEN) UV_MAX;	/* Force iterinit() to be required next time */
	return FALSE;
    }

    array = invlist_array(invlist);

    *start = array[(*pos)++];

    if (*pos >= len) {
	*end = UV_MAX;
    }
    else {
	*end = array[(*pos)++] - 1;
    }

    return TRUE;
}

PERL_STATIC_INLINE UV
S_invlist_highest(SV* const invlist)
{
    /* Returns the highest code point that matches an inversion list.  This API
     * has an ambiguity, as it returns 0 under either the highest is actually
     * 0, or if the list is empty.  If this distinction matters to you, check
     * for emptiness before calling this function */

    UV len = _invlist_len(invlist);
    UV *array;

    PERL_ARGS_ASSERT_INVLIST_HIGHEST;

    if (len == 0) {
	return 0;
    }

    array = invlist_array(invlist);

    /* The last element in the array in the inversion list always starts a
     * range that goes to infinity.  That range may be for code points that are
     * matched in the inversion list, or it may be for ones that aren't
     * matched.  In the latter case, the highest code point in the set is one
     * less than the beginning of this range; otherwise it is the final element
     * of this range: infinity */
    return (ELEMENT_RANGE_MATCHES_INVLIST(len - 1))
           ? UV_MAX
           : array[len - 1] - 1;
}

STATIC SV *
S_invlist_contents(pTHX_ SV* const invlist, const bool traditional_style)
{
    /* Get the contents of an inversion list into a string SV so that they can
     * be printed out.  If 'traditional_style' is TRUE, it uses the format
     * traditionally done for debug tracing; otherwise it uses a format
     * suitable for just copying to the output, with blanks between ranges and
     * a dash between range components */

    UV start, end;
    SV* output;
    const char intra_range_delimiter = (traditional_style ? '\t' : '-');
    const char inter_range_delimiter = (traditional_style ? '\n' : ' ');

    if (traditional_style) {
        output = newSVpvs("\n");
    }
    else {
        output = newSVpvs("");
    }

    PERL_ARGS_ASSERT_INVLIST_CONTENTS;

    assert(! invlist_is_iterating(invlist));

    invlist_iterinit(invlist);
    while (invlist_iternext(invlist, &start, &end)) {
	if (end == UV_MAX) {
	    Perl_sv_catpvf(aTHX_ output, "%04" UVXf "%cINFTY%c",
                                          start, intra_range_delimiter,
                                                 inter_range_delimiter);
	}
	else if (end != start) {
	    Perl_sv_catpvf(aTHX_ output, "%04" UVXf "%c%04" UVXf "%c",
		                          start,
                                                   intra_range_delimiter,
                                                  end, inter_range_delimiter);
	}
	else {
	    Perl_sv_catpvf(aTHX_ output, "%04" UVXf "%c",
                                          start, inter_range_delimiter);
	}
    }

    if (SvCUR(output) && ! traditional_style) {/* Get rid of trailing blank */
        SvCUR_set(output, SvCUR(output) - 1);
    }

    return output;
}

#ifndef PERL_IN_XSUB_RE
void
Perl__invlist_dump(pTHX_ PerlIO *file, I32 level,
                         const char * const indent, SV* const invlist)
{
    /* Designed to be called only by do_sv_dump().  Dumps out the ranges of the
     * inversion list 'invlist' to 'file' at 'level'  Each line is prefixed by
     * the string 'indent'.  The output looks like this:
         [0] 0x000A .. 0x000D
         [2] 0x0085
         [4] 0x2028 .. 0x2029
         [6] 0x3104 .. INFTY
     * This means that the first range of code points matched by the list are
     * 0xA through 0xD; the second range contains only the single code point
     * 0x85, etc.  An inversion list is an array of UVs.  Two array elements
     * are used to define each range (except if the final range extends to
     * infinity, only a single element is needed).  The array index of the
     * first element for the corresponding range is given in brackets. */

    UV start, end;
    STRLEN count = 0;

    PERL_ARGS_ASSERT__INVLIST_DUMP;

    if (invlist_is_iterating(invlist)) {
        Perl_dump_indent(aTHX_ level, file,
             "%sCan't dump inversion list because is in middle of iterating\n",
             indent);
        return;
    }

    invlist_iterinit(invlist);
    while (invlist_iternext(invlist, &start, &end)) {
	if (end == UV_MAX) {
	    Perl_dump_indent(aTHX_ level, file,
                                       "%s[%" UVuf "] 0x%04" UVXf " .. INFTY\n",
                                   indent, (UV)count, start);
	}
	else if (end != start) {
	    Perl_dump_indent(aTHX_ level, file,
                                    "%s[%" UVuf "] 0x%04" UVXf " .. 0x%04" UVXf "\n",
		                indent, (UV)count, start,         end);
	}
	else {
	    Perl_dump_indent(aTHX_ level, file, "%s[%" UVuf "] 0x%04" UVXf "\n",
                                            indent, (UV)count, start);
	}
        count += 2;
    }
}

#endif

#if defined(PERL_ARGS_ASSERT__INVLISTEQ) && !defined(PERL_IN_XSUB_RE)
bool
Perl__invlistEQ(pTHX_ SV* const a, SV* const b, const bool complement_b)
{
    /* Return a boolean as to if the two passed in inversion lists are
     * identical.  The final argument, if TRUE, says to take the complement of
     * the second inversion list before doing the comparison */

    const UV len_a = _invlist_len(a);
    UV len_b = _invlist_len(b);

    const UV* array_a = NULL;
    const UV* array_b = NULL;

    PERL_ARGS_ASSERT__INVLISTEQ;

    /* This code avoids accessing the arrays unless it knows the length is
     * non-zero */

    if (len_a == 0) {
        if (len_b == 0) {
            return ! complement_b;
        }
    }
    else {
        array_a = invlist_array(a);
    }

    if (len_b != 0) {
        array_b = invlist_array(b);
    }

    /* If are to compare 'a' with the complement of b, set it
     * up so are looking at b's complement. */
    if (complement_b) {

        /* The complement of nothing is everything, so <a> would have to have
         * just one element, starting at zero (ending at infinity) */
        if (len_b == 0) {
            return (len_a == 1 && array_a[0] == 0);
        }
        if (array_b[0] == 0) {

            /* Otherwise, to complement, we invert.  Here, the first element is
             * 0, just remove it.  To do this, we just pretend the array starts
             * one later */

            array_b++;
            len_b--;
        }
        else {

            /* But if the first element is not zero, we pretend the list starts
             * at the 0 that is always stored immediately before the array. */
            array_b--;
            len_b++;
        }
    }

    return    len_a == len_b
           && memEQ(array_a, array_b, len_a * sizeof(array_a[0]));

}
#endif

/*
 * As best we can, determine the characters that can match the start of
 * the given EXACTF-ish node.  This is for use in creating ssc nodes, so there
 * can be false positive matches
 *
 * Returns the invlist as a new SV*; it is the caller's responsibility to
 * call SvREFCNT_dec() when done with it.
 */
STATIC SV*
S__make_exactf_invlist(pTHX_ RExC_state_t *pRExC_state, regnode *node)
{
    dVAR;
    const U8 * s = (U8*)STRING(node);
    SSize_t bytelen = STR_LEN(node);
    UV uc;
    /* Start out big enough for 2 separate code points */
    SV* invlist = _new_invlist(4);

    PERL_ARGS_ASSERT__MAKE_EXACTF_INVLIST;

    if (! UTF) {
        uc = *s;

        /* We punt and assume can match anything if the node begins
         * with a multi-character fold.  Things are complicated.  For
         * example, /ffi/i could match any of:
         *  "\N{LATIN SMALL LIGATURE FFI}"
         *  "\N{LATIN SMALL LIGATURE FF}I"
         *  "F\N{LATIN SMALL LIGATURE FI}"
         *  plus several other things; and making sure we have all the
         *  possibilities is hard. */
        if (is_MULTI_CHAR_FOLD_latin1_safe(s, s + bytelen)) {
            invlist = _add_range_to_invlist(invlist, 0, UV_MAX);
        }
        else {
            /* Any Latin1 range character can potentially match any
             * other depending on the locale, and in Turkic locales, U+130 and
             * U+131 */
            if (OP(node) == EXACTFL) {
                _invlist_union(invlist, PL_Latin1, &invlist);
                invlist = add_cp_to_invlist(invlist,
                                                LATIN_SMALL_LETTER_DOTLESS_I);
                invlist = add_cp_to_invlist(invlist,
                                        LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE);
            }
            else {
                /* But otherwise, it matches at least itself.  We can
                 * quickly tell if it has a distinct fold, and if so,
                 * it matches that as well */
                invlist = add_cp_to_invlist(invlist, uc);
                if (IS_IN_SOME_FOLD_L1(uc))
                    invlist = add_cp_to_invlist(invlist, PL_fold_latin1[uc]);
            }

            /* Some characters match above-Latin1 ones under /i.  This
             * is true of EXACTFL ones when the locale is UTF-8 */
            if (HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(uc)
                && (! isASCII(uc) || (OP(node) != EXACTFAA
                                    && OP(node) != EXACTFAA_NO_TRIE)))
            {
                add_above_Latin1_folds(pRExC_state, (U8) uc, &invlist);
            }
        }
    }
    else {  /* Pattern is UTF-8 */
        U8 folded[UTF8_MAX_FOLD_CHAR_EXPAND * UTF8_MAXBYTES_CASE + 1] = { '\0' };
        const U8* e = s + bytelen;
        IV fc;

        fc = uc = utf8_to_uvchr_buf(s, s + bytelen, NULL);

        /* The only code points that aren't folded in a UTF EXACTFish
         * node are are the problematic ones in EXACTFL nodes */
        if (OP(node) == EXACTFL && is_PROBLEMATIC_LOCALE_FOLDEDS_START_cp(uc)) {
            /* We need to check for the possibility that this EXACTFL
             * node begins with a multi-char fold.  Therefore we fold
             * the first few characters of it so that we can make that
             * check */
            U8 *d = folded;
            int i;

            fc = -1;
            for (i = 0; i < UTF8_MAX_FOLD_CHAR_EXPAND && s < e; i++) {
                if (isASCII(*s)) {
                    *(d++) = (U8) toFOLD(*s);
                    if (fc < 0) {       /* Save the first fold */
                        fc = *(d-1);
                    }
                    s++;
                }
                else {
                    STRLEN len;
                    UV fold = toFOLD_utf8_safe(s, e, d, &len);
                    if (fc < 0) {       /* Save the first fold */
                        fc = fold;
                    }
                    d += len;
                    s += UTF8SKIP(s);
                }
            }

            /* And set up so the code below that looks in this folded
             * buffer instead of the node's string */
            e = d;
            s = folded;
        }

        /* When we reach here 's' points to the fold of the first
         * character(s) of the node; and 'e' points to far enough along
         * the folded string to be just past any possible multi-char
         * fold.
         *
         * Unlike the non-UTF-8 case, the macro for determining if a
         * string is a multi-char fold requires all the characters to
         * already be folded.  This is because of all the complications
         * if not.  Note that they are folded anyway, except in EXACTFL
         * nodes.  Like the non-UTF case above, we punt if the node
         * begins with a multi-char fold  */

        if (is_MULTI_CHAR_FOLD_utf8_safe(s, e)) {
            invlist = _add_range_to_invlist(invlist, 0, UV_MAX);
        }
        else {  /* Single char fold */
            unsigned int k;
            unsigned int first_fold;
            const unsigned int * remaining_folds;
            Size_t folds_count;

            /* It matches itself */
            invlist = add_cp_to_invlist(invlist, fc);

            /* ... plus all the things that fold to it, which are found in
             * PL_utf8_foldclosures */
            folds_count = _inverse_folds(fc, &first_fold,
                                                &remaining_folds);
            for (k = 0; k < folds_count; k++) {
                UV c = (k == 0) ? first_fold : remaining_folds[k-1];

                /* /aa doesn't allow folds between ASCII and non- */
                if (   (OP(node) == EXACTFAA || OP(node) == EXACTFAA_NO_TRIE)
                    && isASCII(c) != isASCII(fc))
                {
                    continue;
                }

                invlist = add_cp_to_invlist(invlist, c);
            }

            if (OP(node) == EXACTFL) {

                /* If either [iI] are present in an EXACTFL node the above code
                 * should have added its normal case pair, but under a Turkish
                 * locale they could match instead the case pairs from it.  Add
                 * those as potential matches as well */
                if (isALPHA_FOLD_EQ(fc, 'I')) {
                    invlist = add_cp_to_invlist(invlist,
                                                LATIN_SMALL_LETTER_DOTLESS_I);
                    invlist = add_cp_to_invlist(invlist,
                                        LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE);
                }
                else if (fc == LATIN_SMALL_LETTER_DOTLESS_I) {
                    invlist = add_cp_to_invlist(invlist, 'I');
                }
                else if (fc == LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE) {
                    invlist = add_cp_to_invlist(invlist, 'i');
                }
            }
        }
    }

    return invlist;
}

#undef HEADER_LENGTH
#undef TO_INTERNAL_SIZE
#undef FROM_INTERNAL_SIZE
#undef INVLIST_VERSION_ID

/* End of inversion list object */

STATIC void
S_parse_lparen_question_flags(pTHX_ RExC_state_t *pRExC_state)
{
    /* This parses the flags that are in either the '(?foo)' or '(?foo:bar)'
     * constructs, and updates RExC_flags with them.  On input, RExC_parse
     * should point to the first flag; it is updated on output to point to the
     * final ')' or ':'.  There needs to be at least one flag, or this will
     * abort */

    /* for (?g), (?gc), and (?o) warnings; warning
       about (?c) will warn about (?g) -- japhy    */

#define WASTED_O  0x01
#define WASTED_G  0x02
#define WASTED_C  0x04
#define WASTED_GC (WASTED_G|WASTED_C)
    I32 wastedflags = 0x00;
    U32 posflags = 0, negflags = 0;
    U32 *flagsp = &posflags;
    char has_charset_modifier = '\0';
    regex_charset cs;
    bool has_use_defaults = FALSE;
    const char* const seqstart = RExC_parse - 1; /* Point to the '?' */
    int x_mod_count = 0;

    PERL_ARGS_ASSERT_PARSE_LPAREN_QUESTION_FLAGS;

    /* '^' as an initial flag sets certain defaults */
    if (UCHARAT(RExC_parse) == '^') {
        RExC_parse++;
        has_use_defaults = TRUE;
        STD_PMMOD_FLAGS_CLEAR(&RExC_flags);
        cs = (RExC_uni_semantics)
             ? REGEX_UNICODE_CHARSET
             : REGEX_DEPENDS_CHARSET;
        set_regex_charset(&RExC_flags, cs);
    }
    else {
        cs = get_regex_charset(RExC_flags);
        if (   cs == REGEX_DEPENDS_CHARSET
            && RExC_uni_semantics)
        {
            cs = REGEX_UNICODE_CHARSET;
        }
    }

    while (RExC_parse < RExC_end) {
        /* && strchr("iogcmsx", *RExC_parse) */
        /* (?g), (?gc) and (?o) are useless here
           and must be globally applied -- japhy */
        switch (*RExC_parse) {

            /* Code for the imsxn flags */
            CASE_STD_PMMOD_FLAGS_PARSE_SET(flagsp, x_mod_count);

            case LOCALE_PAT_MOD:
                if (has_charset_modifier) {
                    goto excess_modifier;
                }
                else if (flagsp == &negflags) {
                    goto neg_modifier;
                }
                cs = REGEX_LOCALE_CHARSET;
                has_charset_modifier = LOCALE_PAT_MOD;
                break;
            case UNICODE_PAT_MOD:
                if (has_charset_modifier) {
                    goto excess_modifier;
                }
                else if (flagsp == &negflags) {
                    goto neg_modifier;
                }
                cs = REGEX_UNICODE_CHARSET;
                has_charset_modifier = UNICODE_PAT_MOD;
                break;
            case ASCII_RESTRICT_PAT_MOD:
                if (flagsp == &negflags) {
                    goto neg_modifier;
                }
                if (has_charset_modifier) {
                    if (cs != REGEX_ASCII_RESTRICTED_CHARSET) {
                        goto excess_modifier;
                    }
                    /* Doubled modifier implies more restricted */
                    cs = REGEX_ASCII_MORE_RESTRICTED_CHARSET;
                }
                else {
                    cs = REGEX_ASCII_RESTRICTED_CHARSET;
                }
                has_charset_modifier = ASCII_RESTRICT_PAT_MOD;
                break;
            case DEPENDS_PAT_MOD:
                if (has_use_defaults) {
                    goto fail_modifiers;
                }
                else if (flagsp == &negflags) {
                    goto neg_modifier;
                }
                else if (has_charset_modifier) {
                    goto excess_modifier;
                }

                /* The dual charset means unicode semantics if the
                 * pattern (or target, not known until runtime) are
                 * utf8, or something in the pattern indicates unicode
                 * semantics */
                cs = (RExC_uni_semantics)
                     ? REGEX_UNICODE_CHARSET
                     : REGEX_DEPENDS_CHARSET;
                has_charset_modifier = DEPENDS_PAT_MOD;
                break;
              excess_modifier:
                RExC_parse++;
                if (has_charset_modifier == ASCII_RESTRICT_PAT_MOD) {
                    vFAIL2("Regexp modifier \"%c\" may appear a maximum of twice", ASCII_RESTRICT_PAT_MOD);
                }
                else if (has_charset_modifier == *(RExC_parse - 1)) {
                    vFAIL2("Regexp modifier \"%c\" may not appear twice",
                                        *(RExC_parse - 1));
                }
                else {
                    vFAIL3("Regexp modifiers \"%c\" and \"%c\" are mutually exclusive", has_charset_modifier, *(RExC_parse - 1));
                }
                NOT_REACHED; /*NOTREACHED*/
              neg_modifier:
                RExC_parse++;
                vFAIL2("Regexp modifier \"%c\" may not appear after the \"-\"",
                                    *(RExC_parse - 1));
                NOT_REACHED; /*NOTREACHED*/
            case ONCE_PAT_MOD: /* 'o' */
            case GLOBAL_PAT_MOD: /* 'g' */
                if (ckWARN(WARN_REGEXP)) {
                    const I32 wflagbit = *RExC_parse == 'o'
                                         ? WASTED_O
                                         : WASTED_G;
                    if (! (wastedflags & wflagbit) ) {
                        wastedflags |= wflagbit;
			/* diag_listed_as: Useless (?-%s) - don't use /%s modifier in regex; marked by <-- HERE in m/%s/ */
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
                if (ckWARN(WARN_REGEXP)) {
                    if (! (wastedflags & WASTED_C) ) {
                        wastedflags |= WASTED_GC;
			/* diag_listed_as: Useless (?-%s) - don't use /%s modifier in regex; marked by <-- HERE in m/%s/ */
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
                    ckWARNreg(RExC_parse + 1,"Useless use of (?-p)");
                } else {
                    *flagsp |= RXf_PMf_KEEPCOPY;
                }
                break;
            case '-':
                /* A flag is a default iff it is following a minus, so
                 * if there is a minus, it means will be trying to
                 * re-specify a default which is an error */
                if (has_use_defaults || flagsp == &negflags) {
                    goto fail_modifiers;
                }
                flagsp = &negflags;
                wastedflags = 0;  /* reset so (?g-c) warns twice */
                x_mod_count = 0;
                break;
            case ':':
            case ')':

                if ((posflags & (RXf_PMf_EXTENDED|RXf_PMf_EXTENDED_MORE)) == RXf_PMf_EXTENDED) {
                    negflags |= RXf_PMf_EXTENDED_MORE;
                }
                RExC_flags |= posflags;

                if (negflags & RXf_PMf_EXTENDED) {
                    negflags |= RXf_PMf_EXTENDED_MORE;
                }
                RExC_flags &= ~negflags;
                set_regex_charset(&RExC_flags, cs);

                return;
            default:
              fail_modifiers:
                RExC_parse += SKIP_IF_CHAR(RExC_parse, RExC_end);
		/* diag_listed_as: Sequence (?%s...) not recognized in regex; marked by <-- HERE in m/%s/ */
                vFAIL2utf8f("Sequence (%" UTF8f "...) not recognized",
                      UTF8fARG(UTF, RExC_parse-seqstart, seqstart));
                NOT_REACHED; /*NOTREACHED*/
        }

        RExC_parse += UTF ? UTF8SKIP(RExC_parse) : 1;
    }

    vFAIL("Sequence (?... not terminated");
}

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

PERL_STATIC_INLINE regnode_offset
S_handle_named_backref(pTHX_ RExC_state_t *pRExC_state,
                             I32 *flagp,
                             char * parse_start,
                             char ch
                      )
{
    regnode_offset ret;
    char* name_start = RExC_parse;
    U32 num = 0;
    SV *sv_dat = reg_scan_name(pRExC_state, REG_RSN_RETURN_DATA);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_HANDLE_NAMED_BACKREF;

    if (RExC_parse == name_start || *RExC_parse != ch) {
        /* diag_listed_as: Sequence \%s... not terminated in regex; marked by <-- HERE in m/%s/ */
        vFAIL2("Sequence %.3s... not terminated", parse_start);
    }

    if (sv_dat) {
        num = add_data( pRExC_state, STR_WITH_LEN("S"));
        RExC_rxi->data->data[num]=(void*)sv_dat;
        SvREFCNT_inc_simple_void_NN(sv_dat);
    }
    RExC_sawback = 1;
    ret = reganode(pRExC_state,
                   ((! FOLD)
                     ? NREF
                     : (ASCII_FOLD_RESTRICTED)
                       ? NREFFA
                       : (AT_LEAST_UNI_SEMANTICS)
                         ? NREFFU
                         : (LOC)
                           ? NREFFL
                           : NREFF),
                    num);
    *flagp |= HASWIDTH;

    Set_Node_Offset(REGNODE_p(ret), parse_start+1);
    Set_Node_Cur_Length(REGNODE_p(ret), parse_start);

    nextchar(pRExC_state);
    return ret;
}

/* On success, returns the offset at which any next node should be placed into
 * the regex engine program being compiled.
 *
 * Returns 0 otherwise, with *flagp set to indicate why:
 *  TRYAGAIN        at the end of (?) that only sets flags.
 *  RESTART_PARSE   if the parse needs to be restarted, or'd with
 *                  NEED_UTF8 if the pattern needs to be upgraded to UTF-8.
 *  Otherwise would only return 0 if regbranch() returns 0, which cannot
 *  happen.  */
STATIC regnode_offset
S_reg(pTHX_ RExC_state_t *pRExC_state, I32 paren, I32 *flagp, U32 depth)
    /* paren: Parenthesized? 0=top; 1,2=inside '(': changed to letter.
     * 2 is like 1, but indicates that nextchar() has been called to advance
     * RExC_parse beyond the '('.  Things like '(?' are indivisible tokens, and
     * this flag alerts us to the need to check for that */
{
    regnode_offset ret = 0;    /* Will be the head of the group. */
    regnode_offset br;
    regnode_offset lastbr;
    regnode_offset ender = 0;
    I32 parno = 0;
    I32 flags;
    U32 oregflags = RExC_flags;
    bool have_branch = 0;
    bool is_open = 0;
    I32 freeze_paren = 0;
    I32 after_freeze = 0;
    I32 num; /* numeric backreferences */
    SV * max_open;  /* Max number of unclosed parens */

    char * parse_start = RExC_parse; /* MJD */
    char * const oregcomp_parse = RExC_parse;

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REG;
    DEBUG_PARSE("reg ");


    max_open = get_sv(RE_COMPILE_RECURSION_LIMIT, GV_ADD);
    assert(max_open);
    if (!SvIOK(max_open)) {
        sv_setiv(max_open, RE_COMPILE_RECURSION_INIT);
    }
    if (depth > 4 * (UV) SvIV(max_open)) { /* We increase depth by 4 for each
                                              open paren */
        vFAIL("Too many nested open parens");
    }

    *flagp = 0;				/* Tentatively. */

    /* Having this true makes it feasible to have a lot fewer tests for the
     * parse pointer being in scope.  For example, we can write
     *      while(isFOO(*RExC_parse)) RExC_parse++;
     * instead of
     *      while(RExC_parse < RExC_end && isFOO(*RExC_parse)) RExC_parse++;
     */
    assert(*RExC_end == '\0');

    /* Make an OPEN node, if parenthesized. */
    if (paren) {

        /* Under /x, space and comments can be gobbled up between the '(' and
         * here (if paren ==2).  The forms '(*VERB' and '(?...' disallow such
         * intervening space, as the sequence is a token, and a token should be
         * indivisible */
        bool has_intervening_patws = (paren == 2)
                                  && *(RExC_parse - 1) != '(';

        if (RExC_parse >= RExC_end) {
	    vFAIL("Unmatched (");
        }

        if (paren == 'r') {     /* Atomic script run */
            paren = '>';
            goto parse_rest;
        }
        else if ( *RExC_parse == '*') { /* (*VERB:ARG), (*construct:...) */
	    char *start_verb = RExC_parse + 1;
	    STRLEN verb_len;
	    char *start_arg = NULL;
	    unsigned char op = 0;
            int arg_required = 0;
            int internal_argval = -1; /* if >-1 we are not allowed an argument*/
            bool has_upper = FALSE;

            if (has_intervening_patws) {
                RExC_parse++;   /* past the '*' */

                /* For strict backwards compatibility, don't change the message
                 * now that we also have lowercase operands */
                if (isUPPER(*RExC_parse)) {
                    vFAIL("In '(*VERB...)', the '(' and '*' must be adjacent");
                }
                else {
                    vFAIL("In '(*...)', the '(' and '*' must be adjacent");
                }
            }
	    while (RExC_parse < RExC_end && *RExC_parse != ')' ) {
	        if ( *RExC_parse == ':' ) {
	            start_arg = RExC_parse + 1;
	            break;
	        }
                else if (! UTF) {
                    if (isUPPER(*RExC_parse)) {
                        has_upper = TRUE;
                    }
                    RExC_parse++;
                }
                else {
                    RExC_parse += UTF8SKIP(RExC_parse);
                }
	    }
	    verb_len = RExC_parse - start_verb;
	    if ( start_arg ) {
                if (RExC_parse >= RExC_end) {
                    goto unterminated_verb_pattern;
                }

	        RExC_parse += UTF ? UTF8SKIP(RExC_parse) : 1;
	        while ( RExC_parse < RExC_end && *RExC_parse != ')' ) {
                    RExC_parse += UTF ? UTF8SKIP(RExC_parse) : 1;
                }
	        if ( RExC_parse >= RExC_end || *RExC_parse != ')' ) {
                  unterminated_verb_pattern:
                    if (has_upper) {
                        vFAIL("Unterminated verb pattern argument");
                    }
                    else {
                        vFAIL("Unterminated '(*...' argument");
                    }
                }
	    } else {
	        if ( RExC_parse >= RExC_end || *RExC_parse != ')' ) {
                    if (has_upper) {
                        vFAIL("Unterminated verb pattern");
                    }
                    else {
                        vFAIL("Unterminated '(*...' construct");
                    }
                }
	    }

            /* Here, we know that RExC_parse < RExC_end */

	    switch ( *start_verb ) {
            case 'A':  /* (*ACCEPT) */
                if ( memEQs(start_verb, verb_len,"ACCEPT") ) {
		    op = ACCEPT;
		    internal_argval = RExC_nestroot;
		}
		break;
            case 'C':  /* (*COMMIT) */
                if ( memEQs(start_verb, verb_len,"COMMIT") )
                    op = COMMIT;
                break;
            case 'F':  /* (*FAIL) */
                if ( verb_len==1 || memEQs(start_verb, verb_len,"FAIL") ) {
		    op = OPFAIL;
		}
		break;
            case ':':  /* (*:NAME) */
	    case 'M':  /* (*MARK:NAME) */
	        if ( verb_len==0 || memEQs(start_verb, verb_len,"MARK") ) {
                    op = MARKPOINT;
                    arg_required = 1;
                }
                break;
            case 'P':  /* (*PRUNE) */
                if ( memEQs(start_verb, verb_len,"PRUNE") )
                    op = PRUNE;
                break;
            case 'S':   /* (*SKIP) */
                if ( memEQs(start_verb, verb_len,"SKIP") )
                    op = SKIP;
                break;
            case 'T':  /* (*THEN) */
                /* [19:06] <TimToady> :: is then */
                if ( memEQs(start_verb, verb_len,"THEN") ) {
                    op = CUTGROUP;
                    RExC_seen |= REG_CUTGROUP_SEEN;
                }
                break;
            case 'a':
                if (   memEQs(start_verb, verb_len, "asr")
                    || memEQs(start_verb, verb_len, "atomic_script_run"))
                {
                    paren = 'r';        /* Mnemonic: recursed run */
                    goto script_run;
                }
                else if (memEQs(start_verb, verb_len, "atomic")) {
                    paren = 't';    /* AtOMIC */
                    goto alpha_assertions;
                }
                break;
            case 'p':
                if (   memEQs(start_verb, verb_len, "plb")
                    || memEQs(start_verb, verb_len, "positive_lookbehind"))
                {
                    paren = 'b';
                    goto lookbehind_alpha_assertions;
                }
                else if (   memEQs(start_verb, verb_len, "pla")
                         || memEQs(start_verb, verb_len, "positive_lookahead"))
                {
                    paren = 'a';
                    goto alpha_assertions;
                }
                break;
            case 'n':
                if (   memEQs(start_verb, verb_len, "nlb")
                    || memEQs(start_verb, verb_len, "negative_lookbehind"))
                {
                    paren = 'B';
                    goto lookbehind_alpha_assertions;
                }
                else if (   memEQs(start_verb, verb_len, "nla")
                         || memEQs(start_verb, verb_len, "negative_lookahead"))
                {
                    paren = 'A';
                    goto alpha_assertions;
                }
                break;
            case 's':
                if (   memEQs(start_verb, verb_len, "sr")
                    || memEQs(start_verb, verb_len, "script_run"))
                {
                    regnode_offset atomic;

                    paren = 's';

                   script_run:

                    /* This indicates Unicode rules. */
                    REQUIRE_UNI_RULES(flagp, 0);

                    if (! start_arg) {
                        goto no_colon;
                    }

                    RExC_parse = start_arg;

                    if (RExC_in_script_run) {

                        /*  Nested script runs are treated as no-ops, because
                         *  if the nested one fails, the outer one must as
                         *  well.  It could fail sooner, and avoid (??{} with
                         *  side effects, but that is explicitly documented as
                         *  undefined behavior. */

                        ret = 0;

                        if (paren == 's') {
                            paren = ':';
                            goto parse_rest;
                        }

                        /* But, the atomic part of a nested atomic script run
                         * isn't a no-op, but can be treated just like a '(?>'
                         * */
                        paren = '>';
                        goto parse_rest;
                    }

                    /* By doing this here, we avoid extra warnings for nested
                     * script runs */
                    ckWARNexperimental(RExC_parse,
                        WARN_EXPERIMENTAL__SCRIPT_RUN,
                        "The script_run feature is experimental");

                    if (paren == 's') {
                        /* Here, we're starting a new regular script run */
                        ret = reg_node(pRExC_state, SROPEN);
                        RExC_in_script_run = 1;
                        is_open = 1;
                        goto parse_rest;
                    }

                    /* Here, we are starting an atomic script run.  This is
                     * handled by recursing to deal with the atomic portion
                     * separately, enclosed in SROPEN ... SRCLOSE nodes */

                    ret = reg_node(pRExC_state, SROPEN);

                    RExC_in_script_run = 1;

                    atomic = reg(pRExC_state, 'r', &flags, depth);
                    if (flags & (RESTART_PARSE|NEED_UTF8)) {
                        *flagp = flags & (RESTART_PARSE|NEED_UTF8);
                        return 0;
                    }

                    if (! REGTAIL(pRExC_state, ret, atomic)) {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }

                    if (! REGTAIL(pRExC_state, atomic, reg_node(pRExC_state,
                                                                SRCLOSE)))
                    {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }

                    RExC_in_script_run = 0;
                    return ret;
                }

                break;

            lookbehind_alpha_assertions:
                RExC_seen |= REG_LOOKBEHIND_SEEN;
                RExC_in_lookbehind++;
                /*FALLTHROUGH*/

            alpha_assertions:
                ckWARNexperimental(RExC_parse,
                        WARN_EXPERIMENTAL__ALPHA_ASSERTIONS,
                        "The alpha_assertions feature is experimental");

                RExC_seen_zerolen++;

                if (! start_arg) {
                    goto no_colon;
                }

                /* An empty negative lookahead assertion simply is failure */
                if (paren == 'A' && RExC_parse == start_arg) {
                    ret=reganode(pRExC_state, OPFAIL, 0);
                    nextchar(pRExC_state);
                    return ret;
	        }

                RExC_parse = start_arg;
                goto parse_rest;

              no_colon:
                vFAIL2utf8f(
                "'(*%" UTF8f "' requires a terminating ':'",
                UTF8fARG(UTF, verb_len, start_verb));
		NOT_REACHED; /*NOTREACHED*/

	    } /* End of switch */
	    if ( ! op ) {
	        RExC_parse += UTF
                              ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                              : 1;
                if (has_upper || verb_len == 0) {
                    vFAIL2utf8f(
                    "Unknown verb pattern '%" UTF8f "'",
                    UTF8fARG(UTF, verb_len, start_verb));
                }
                else {
                    vFAIL2utf8f(
                    "Unknown '(*...)' construct '%" UTF8f "'",
                    UTF8fARG(UTF, verb_len, start_verb));
                }
	    }
            if ( RExC_parse == start_arg ) {
                start_arg = NULL;
            }
            if ( arg_required && !start_arg ) {
                vFAIL3("Verb pattern '%.*s' has a mandatory argument",
                    verb_len, start_verb);
            }
            if (internal_argval == -1) {
                ret = reganode(pRExC_state, op, 0);
            } else {
                ret = reg2Lanode(pRExC_state, op, 0, internal_argval);
            }
            RExC_seen |= REG_VERBARG_SEEN;
            if (start_arg) {
                SV *sv = newSVpvn( start_arg,
                                    RExC_parse - start_arg);
                ARG(REGNODE_p(ret)) = add_data( pRExC_state,
                                        STR_WITH_LEN("S"));
                RExC_rxi->data->data[ARG(REGNODE_p(ret))]=(void*)sv;
                FLAGS(REGNODE_p(ret)) = 1;
            } else {
                FLAGS(REGNODE_p(ret)) = 0;
            }
            if ( internal_argval != -1 )
                ARG2L_SET(REGNODE_p(ret), internal_argval);
	    nextchar(pRExC_state);
	    return ret;
        }
        else if (*RExC_parse == '?') { /* (?...) */
	    bool is_logical = 0;
	    const char * const seqstart = RExC_parse;
            const char * endptr;
            if (has_intervening_patws) {
                RExC_parse++;
                vFAIL("In '(?...)', the '(' and '?' must be adjacent");
            }

	    RExC_parse++;           /* past the '?' */
            paren = *RExC_parse;    /* might be a trailing NUL, if not
                                       well-formed */
            RExC_parse += UTF ? UTF8SKIP(RExC_parse) : 1;
            if (RExC_parse > RExC_end) {
                paren = '\0';
            }
	    ret = 0;			/* For look-ahead/behind. */
	    switch (paren) {

	    case 'P':	/* (?P...) variants for those used to PCRE/Python */
	        paren = *RExC_parse;
		if ( paren == '<') {    /* (?P<...>) named capture */
                    RExC_parse++;
                    if (RExC_parse >= RExC_end) {
                        vFAIL("Sequence (?P<... not terminated");
                    }
		    goto named_capture;
                }
                else if (paren == '>') {   /* (?P>name) named recursion */
                    RExC_parse++;
                    if (RExC_parse >= RExC_end) {
                        vFAIL("Sequence (?P>... not terminated");
                    }
                    goto named_recursion;
                }
                else if (paren == '=') {   /* (?P=...)  named backref */
                    RExC_parse++;
                    return handle_named_backref(pRExC_state, flagp,
                                                parse_start, ')');
                }
                RExC_parse += SKIP_IF_CHAR(RExC_parse, RExC_end);
                /* diag_listed_as: Sequence (?%s...) not recognized in regex; marked by <-- HERE in m/%s/ */
		vFAIL3("Sequence (%.*s...) not recognized",
                                RExC_parse-seqstart, seqstart);
		NOT_REACHED; /*NOTREACHED*/
            case '<':           /* (?<...) */
		if (*RExC_parse == '!')
		    paren = ',';
		else if (*RExC_parse != '=')
              named_capture:
		{               /* (?<...>) */
		    char *name_start;
		    SV *svname;
		    paren= '>';
                /* FALLTHROUGH */
            case '\'':          /* (?'...') */
                    name_start = RExC_parse;
                    svname = reg_scan_name(pRExC_state, REG_RSN_RETURN_NAME);
		    if (   RExC_parse == name_start
                        || RExC_parse >= RExC_end
                        || *RExC_parse != paren)
                    {
		        vFAIL2("Sequence (?%c... not terminated",
		            paren=='>' ? '<' : paren);
                    }
		    {
			HE *he_str;
			SV *sv_dat = NULL;
                        if (!svname) /* shouldn't happen */
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
                                pv = (I32*)SvGROW(sv_dat,
                                                SvCUR(sv_dat) + sizeof(I32)+1);
                                SvCUR_set(sv_dat, SvCUR(sv_dat) + sizeof(I32));
                                pv[count] = RExC_npar;
                                SvIV_set(sv_dat, SvIVX(sv_dat) + 1);
                            }
                        } else {
                            (void)SvUPGRADE(sv_dat, SVt_PVNV);
                            sv_setpvn(sv_dat, (char *)&(RExC_npar),
                                                                sizeof(I32));
                            SvIOK_on(sv_dat);
                            SvIV_set(sv_dat, 1);
                        }
#ifdef DEBUGGING
                        /* Yes this does cause a memory leak in debugging Perls
                         * */
                        if (!av_store(RExC_paren_name_list,
                                      RExC_npar, SvREFCNT_inc_NN(svname)))
                            SvREFCNT_dec_NN(svname);
#endif

                        /*sv_dump(sv_dat);*/
                    }
                    nextchar(pRExC_state);
		    paren = 1;
		    goto capturing_parens;
		}

                RExC_seen |= REG_LOOKBEHIND_SEEN;
		RExC_in_lookbehind++;
		RExC_parse++;
                if (RExC_parse >= RExC_end) {
                    vFAIL("Sequence (?... not terminated");
                }

                /* FALLTHROUGH */
	    case '=':           /* (?=...) */
		RExC_seen_zerolen++;
                break;
	    case '!':           /* (?!...) */
		RExC_seen_zerolen++;
		/* check if we're really just a "FAIL" assertion */
                skip_to_be_ignored_text(pRExC_state, &RExC_parse,
                                        FALSE /* Don't force to /x */ );
	        if (*RExC_parse == ')') {
                    ret=reganode(pRExC_state, OPFAIL, 0);
	            nextchar(pRExC_state);
	            return ret;
	        }
	        break;
	    case '|':           /* (?|...) */
	        /* branch reset, behave like a (?:...) except that
	           buffers in alternations share the same numbers */
	        paren = ':';
	        after_freeze = freeze_paren = RExC_npar;

                /* XXX This construct currently requires an extra pass.
                 * Investigation would be required to see if that could be
                 * changed */
                REQUIRE_PARENS_PASS;
	        break;
	    case ':':           /* (?:...) */
	    case '>':           /* (?>...) */
		break;
	    case '$':           /* (?$...) */
	    case '@':           /* (?@...) */
		vFAIL2("Sequence (?%c...) not implemented", (int)paren);
		break;
	    case '0' :           /* (?0) */
	    case 'R' :           /* (?R) */
                if (RExC_parse == RExC_end || *RExC_parse != ')')
		    FAIL("Sequence (?R) not terminated");
                num = 0;
                RExC_seen |= REG_RECURSE_SEEN;

                /* XXX These constructs currently require an extra pass.
                 * It probably could be changed */
                REQUIRE_PARENS_PASS;

		*flagp |= POSTPONED;
                goto gen_recurse_regop;
		/*notreached*/
            /* named and numeric backreferences */
            case '&':            /* (?&NAME) */
                parse_start = RExC_parse - 1;
              named_recursion:
                {
                    SV *sv_dat = reg_scan_name(pRExC_state,
                                               REG_RSN_RETURN_DATA);
                   num = sv_dat ? *((I32 *)SvPVX(sv_dat)) : 0;
                }
                if (RExC_parse >= RExC_end || *RExC_parse != ')')
                    vFAIL("Sequence (?&... not terminated");
                goto gen_recurse_regop;
                /* NOTREACHED */
            case '+':
                if (! inRANGE(RExC_parse[0], '1', '9')) {
                    RExC_parse++;
                    vFAIL("Illegal pattern");
                }
                goto parse_recursion;
                /* NOTREACHED*/
            case '-': /* (?-1) */
                if (! inRANGE(RExC_parse[0], '1', '9')) {
                    RExC_parse--; /* rewind to let it be handled later */
                    goto parse_flags;
                }
                /* FALLTHROUGH */
            case '1': case '2': case '3': case '4': /* (?1) */
	    case '5': case '6': case '7': case '8': case '9':
	        RExC_parse = (char *) seqstart + 1;  /* Point to the digit */
              parse_recursion:
                {
                    bool is_neg = FALSE;
                    UV unum;
                    parse_start = RExC_parse - 1; /* MJD */
                    if (*RExC_parse == '-') {
                        RExC_parse++;
                        is_neg = TRUE;
                    }
                    endptr = RExC_end;
                    if (grok_atoUV(RExC_parse, &unum, &endptr)
                        && unum <= I32_MAX
                    ) {
                        num = (I32)unum;
                        RExC_parse = (char*)endptr;
                    } else
                        num = I32_MAX;
                    if (is_neg) {
                        /* Some limit for num? */
                        num = -num;
                    }
                }
	        if (*RExC_parse!=')')
	            vFAIL("Expecting close bracket");

              gen_recurse_regop:
                if ( paren == '-' ) {
                    /*
                    Diagram of capture buffer numbering.
                    Top line is the normal capture buffer numbers
                    Bottom line is the negative indexing as from
                    the X (the (?-2))

                    +   1 2    3 4 5 X          6 7
                       /(a(x)y)(a(b(c(?-2)d)e)f)(g(h))/
                    -   5 4    3 2 1 X          x x

                    */
                    num = RExC_npar + num;
                    if (num < 1)  {

                        /* It might be a forward reference; we can't fail until
                         * we know, by completing the parse to get all the
                         * groups, and then reparsing */
                        if (ALL_PARENS_COUNTED)  {
                            RExC_parse++;
                            vFAIL("Reference to nonexistent group");
                        }
                        else {
                            REQUIRE_PARENS_PASS;
                        }
                    }
                } else if ( paren == '+' ) {
                    num = RExC_npar + num - 1;
                }
                /* We keep track how many GOSUB items we have produced.
                   To start off the ARG2L() of the GOSUB holds its "id",
                   which is used later in conjunction with RExC_recurse
                   to calculate the offset we need to jump for the GOSUB,
                   which it will store in the final representation.
                   We have to defer the actual calculation until much later
                   as the regop may move.
                 */

                ret = reg2Lanode(pRExC_state, GOSUB, num, RExC_recurse_count);
                if (num >= RExC_npar) {

                    /* It might be a forward reference; we can't fail until we
                     * know, by completing the parse to get all the groups, and
                     * then reparsing */
                    if (ALL_PARENS_COUNTED)  {
                        if (num >= RExC_total_parens) {
                            RExC_parse++;
                            vFAIL("Reference to nonexistent group");
                        }
                    }
                    else {
                        REQUIRE_PARENS_PASS;
                    }
                }
                RExC_recurse_count++;
                DEBUG_OPTIMISE_MORE_r(Perl_re_printf( aTHX_
                    "%*s%*s Recurse #%" UVuf " to %" IVdf "\n",
                            22, "|    |", (int)(depth * 2 + 1), "",
                            (UV)ARG(REGNODE_p(ret)),
                            (IV)ARG2L(REGNODE_p(ret))));
                RExC_seen |= REG_RECURSE_SEEN;

                Set_Node_Length(REGNODE_p(ret),
                                1 + regarglen[OP(REGNODE_p(ret))]); /* MJD */
		Set_Node_Offset(REGNODE_p(ret), parse_start); /* MJD */

                *flagp |= POSTPONED;
                assert(*RExC_parse == ')');
                nextchar(pRExC_state);
                return ret;

            /* NOTREACHED */

	    case '?':           /* (??...) */
		is_logical = 1;
		if (*RExC_parse != '{') {
                    RExC_parse += SKIP_IF_CHAR(RExC_parse, RExC_end);
                    /* diag_listed_as: Sequence (?%s...) not recognized in regex; marked by <-- HERE in m/%s/ */
                    vFAIL2utf8f(
                        "Sequence (%" UTF8f "...) not recognized",
                        UTF8fARG(UTF, RExC_parse-seqstart, seqstart));
		    NOT_REACHED; /*NOTREACHED*/
		}
		*flagp |= POSTPONED;
		paren = '{';
                RExC_parse++;
		/* FALLTHROUGH */
	    case '{':           /* (?{...}) */
	    {
		U32 n = 0;
		struct reg_code_block *cb;
                OP * o;

		RExC_seen_zerolen++;

		if (   !pRExC_state->code_blocks
		    || pRExC_state->code_index
                                        >= pRExC_state->code_blocks->count
		    || pRExC_state->code_blocks->cb[pRExC_state->code_index].start
			!= (STRLEN)((RExC_parse -3 - (is_logical ? 1 : 0))
			    - RExC_start)
		) {
		    if (RExC_pm_flags & PMf_USE_RE_EVAL)
			FAIL("panic: Sequence (?{...}): no code block found\n");
		    FAIL("Eval-group not allowed at runtime, use re 'eval'");
		}
		/* this is a pre-compiled code block (?{...}) */
		cb = &pRExC_state->code_blocks->cb[pRExC_state->code_index];
		RExC_parse = RExC_start + cb->end;
		o = cb->block;
                if (cb->src_regex) {
                    n = add_data(pRExC_state, STR_WITH_LEN("rl"));
                    RExC_rxi->data->data[n] =
                        (void*)SvREFCNT_inc((SV*)cb->src_regex);
                    RExC_rxi->data->data[n+1] = (void*)o;
                }
                else {
                    n = add_data(pRExC_state,
                            (RExC_pm_flags & PMf_HAS_CV) ? "L" : "l", 1);
                    RExC_rxi->data->data[n] = (void*)o;
                }
		pRExC_state->code_index++;
		nextchar(pRExC_state);

		if (is_logical) {
                    regnode_offset eval;
		    ret = reg_node(pRExC_state, LOGICAL);

                    eval = reg2Lanode(pRExC_state, EVAL,
                                       n,

                                       /* for later propagation into (??{})
                                        * return value */
                                       RExC_flags & RXf_PMf_COMPILETIME
                                      );
                    FLAGS(REGNODE_p(ret)) = 2;
                    if (! REGTAIL(pRExC_state, ret, eval)) {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }
                    /* deal with the length of this later - MJD */
		    return ret;
		}
		ret = reg2Lanode(pRExC_state, EVAL, n, 0);
		Set_Node_Length(REGNODE_p(ret), RExC_parse - parse_start + 1);
		Set_Node_Offset(REGNODE_p(ret), parse_start);
		return ret;
	    }
	    case '(':           /* (?(?{...})...) and (?(?=...)...) */
	    {
	        int is_define= 0;
                const int DEFINE_len = sizeof("DEFINE") - 1;
		if (    RExC_parse < RExC_end - 1
                    && (   (       RExC_parse[0] == '?'        /* (?(?...)) */
                            && (   RExC_parse[1] == '='
                                || RExC_parse[1] == '!'
                                || RExC_parse[1] == '<'
                                || RExC_parse[1] == '{'))
		        || (       RExC_parse[0] == '*'        /* (?(*...)) */
                            && (   memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "pla:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "plb:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "nla:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "nlb:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "positive_lookahead:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "positive_lookbehind:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "negative_lookahead:")
                                || memBEGINs(RExC_parse + 1,
                                         (Size_t) (RExC_end - (RExC_parse + 1)),
                                         "negative_lookbehind:"))))
                ) { /* Lookahead or eval. */
                    I32 flag;
                    regnode_offset tail;

                    ret = reg_node(pRExC_state, LOGICAL);
                    FLAGS(REGNODE_p(ret)) = 1;

                    tail = reg(pRExC_state, 1, &flag, depth+1);
                    RETURN_FAIL_ON_RESTART(flag, flagp);
                    if (! REGTAIL(pRExC_state, ret, tail)) {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }
                    goto insert_if;
                }
		else if (   RExC_parse[0] == '<'     /* (?(<NAME>)...) */
		         || RExC_parse[0] == '\'' ) /* (?('NAME')...) */
	        {
	            char ch = RExC_parse[0] == '<' ? '>' : '\'';
	            char *name_start= RExC_parse++;
	            U32 num = 0;
	            SV *sv_dat=reg_scan_name(pRExC_state, REG_RSN_RETURN_DATA);
	            if (   RExC_parse == name_start
                        || RExC_parse >= RExC_end
                        || *RExC_parse != ch)
                    {
                        vFAIL2("Sequence (?(%c... not terminated",
                            (ch == '>' ? '<' : ch));
                    }
                    RExC_parse++;
                    if (sv_dat) {
                        num = add_data( pRExC_state, STR_WITH_LEN("S"));
                        RExC_rxi->data->data[num]=(void*)sv_dat;
                        SvREFCNT_inc_simple_void_NN(sv_dat);
                    }
                    ret = reganode(pRExC_state, NGROUPP, num);
                    goto insert_if_check_paren;
		}
		else if (memBEGINs(RExC_parse,
                                   (STRLEN) (RExC_end - RExC_parse),
                                   "DEFINE"))
                {
		    ret = reganode(pRExC_state, DEFINEP, 0);
		    RExC_parse += DEFINE_len;
		    is_define = 1;
		    goto insert_if_check_paren;
		}
		else if (RExC_parse[0] == 'R') {
		    RExC_parse++;
                    /* parno == 0 => /(?(R)YES|NO)/  "in any form of recursion OR eval"
                     * parno == 1 => /(?(R0)YES|NO)/ "in GOSUB (?0) / (?R)"
                     * parno == 2 => /(?(R1)YES|NO)/ "in GOSUB (?1) (parno-1)"
                     */
		    parno = 0;
                    if (RExC_parse[0] == '0') {
                        parno = 1;
                        RExC_parse++;
                    }
                    else if (inRANGE(RExC_parse[0], '1', '9')) {
                        UV uv;
                        endptr = RExC_end;
                        if (grok_atoUV(RExC_parse, &uv, &endptr)
                            && uv <= I32_MAX
                        ) {
                            parno = (I32)uv + 1;
                            RExC_parse = (char*)endptr;
                        }
                        /* else "Switch condition not recognized" below */
		    } else if (RExC_parse[0] == '&') {
		        SV *sv_dat;
		        RExC_parse++;
		        sv_dat = reg_scan_name(pRExC_state,
                                               REG_RSN_RETURN_DATA);
                        if (sv_dat)
                            parno = 1 + *((I32 *)SvPVX(sv_dat));
		    }
		    ret = reganode(pRExC_state, INSUBP, parno);
		    goto insert_if_check_paren;
		}
                else if (inRANGE(RExC_parse[0], '1', '9')) {
                    /* (?(1)...) */
		    char c;
                    UV uv;
                    endptr = RExC_end;
                    if (grok_atoUV(RExC_parse, &uv, &endptr)
                        && uv <= I32_MAX
                    ) {
                        parno = (I32)uv;
                        RExC_parse = (char*)endptr;
                    }
                    else {
                        vFAIL("panic: grok_atoUV returned FALSE");
                    }
                    ret = reganode(pRExC_state, GROUPP, parno);

                 insert_if_check_paren:
		    if (UCHARAT(RExC_parse) != ')') {
                        RExC_parse += UTF
                                      ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                                      : 1;
			vFAIL("Switch condition not recognized");
		    }
		    nextchar(pRExC_state);
		  insert_if:
                    if (! REGTAIL(pRExC_state, ret, reganode(pRExC_state,
                                                             IFTHEN, 0)))
                    {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }
                    br = regbranch(pRExC_state, &flags, 1, depth+1);
		    if (br == 0) {
                        RETURN_FAIL_ON_RESTART(flags,flagp);
                        FAIL2("panic: regbranch returned failure, flags=%#" UVxf,
                              (UV) flags);
                    } else
                    if (! REGTAIL(pRExC_state, br, reganode(pRExC_state,
                                                             LONGJMP, 0)))
                    {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }
		    c = UCHARAT(RExC_parse);
                    nextchar(pRExC_state);
		    if (flags&HASWIDTH)
			*flagp |= HASWIDTH;
		    if (c == '|') {
		        if (is_define)
		            vFAIL("(?(DEFINE)....) does not allow branches");

                        /* Fake one for optimizer.  */
                        lastbr = reganode(pRExC_state, IFTHEN, 0);

                        if (!regbranch(pRExC_state, &flags, 1, depth+1)) {
                            RETURN_FAIL_ON_RESTART(flags, flagp);
                            FAIL2("panic: regbranch returned failure, flags=%#" UVxf,
                                  (UV) flags);
                        }
                        if (! REGTAIL(pRExC_state, ret, lastbr)) {
                            REQUIRE_BRANCHJ(flagp, 0);
                        }
		 	if (flags&HASWIDTH)
			    *flagp |= HASWIDTH;
                        c = UCHARAT(RExC_parse);
                        nextchar(pRExC_state);
		    }
		    else
			lastbr = 0;
                    if (c != ')') {
                        if (RExC_parse >= RExC_end)
                            vFAIL("Switch (?(condition)... not terminated");
                        else
                            vFAIL("Switch (?(condition)... contains too many branches");
                    }
		    ender = reg_node(pRExC_state, TAIL);
                    if (! REGTAIL(pRExC_state, br, ender)) {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }
		    if (lastbr) {
                        if (! REGTAIL(pRExC_state, lastbr, ender)) {
                            REQUIRE_BRANCHJ(flagp, 0);
                        }
                        if (! REGTAIL(pRExC_state,
                                      REGNODE_OFFSET(
                                                 NEXTOPER(
                                                 NEXTOPER(REGNODE_p(lastbr)))),
                                      ender))
                        {
                            REQUIRE_BRANCHJ(flagp, 0);
                        }
		    }
		    else
                        if (! REGTAIL(pRExC_state, ret, ender)) {
                            REQUIRE_BRANCHJ(flagp, 0);
                        }
#if 0  /* Removing this doesn't cause failures in the test suite -- khw */
                    RExC_size++; /* XXX WHY do we need this?!!
                                    For large programs it seems to be required
                                    but I can't figure out why. -- dmq*/
#endif
		    return ret;
		}
                RExC_parse += UTF
                              ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                              : 1;
                vFAIL("Unknown switch condition (?(...))");
	    }
	    case '[':           /* (?[ ... ]) */
                return handle_regex_sets(pRExC_state, NULL, flagp, depth+1,
                                         oregcomp_parse);
            case 0: /* A NUL */
		RExC_parse--; /* for vFAIL to print correctly */
                vFAIL("Sequence (? incomplete");
                break;

            case ')':
                if (RExC_strict) {  /* [perl #132851] */
                    ckWARNreg(RExC_parse, "Empty (?) without any modifiers");
                }
                /* FALLTHROUGH */
	    default: /* e.g., (?i) */
	        RExC_parse = (char *) seqstart + 1;
              parse_flags:
		parse_lparen_question_flags(pRExC_state);
                if (UCHARAT(RExC_parse) != ':') {
                    if (RExC_parse < RExC_end)
                        nextchar(pRExC_state);
                    *flagp = TRYAGAIN;
                    return 0;
                }
                paren = ':';
                nextchar(pRExC_state);
                ret = 0;
                goto parse_rest;
            } /* end switch */
	}
	else {
            if (*RExC_parse == '{') {
                ckWARNregdep(RExC_parse + 1,
                            "Unescaped left brace in regex is "
                            "deprecated here (and will be fatal "
                            "in Perl 5.32), passed through");
            }
            /* Not bothering to indent here, as the above 'else' is temporary
             * */
        if (!(RExC_flags & RXf_PMf_NOCAPTURE)) {   /* (...) */
	  capturing_parens:
	    parno = RExC_npar;
	    RExC_npar++;
            if (! ALL_PARENS_COUNTED) {
                /* If we are in our first pass through (and maybe only pass),
                 * we  need to allocate memory for the capturing parentheses
                 * data structures.
                 */

                if (!RExC_parens_buf_size) {
                    /* first guess at number of parens we might encounter */
                    RExC_parens_buf_size = 10;

                    /* setup RExC_open_parens, which holds the address of each
                     * OPEN tag, and to make things simpler for the 0 index the
                     * start of the program - this is used later for offsets */
                    Newxz(RExC_open_parens, RExC_parens_buf_size,
                            regnode_offset);
                    RExC_open_parens[0] = 1;    /* +1 for REG_MAGIC */

                    /* setup RExC_close_parens, which holds the address of each
                     * CLOSE tag, and to make things simpler for the 0 index
                     * the end of the program - this is used later for offsets
                     * */
                    Newxz(RExC_close_parens, RExC_parens_buf_size,
                            regnode_offset);
                    /* we dont know where end op starts yet, so we dont need to
                     * set RExC_close_parens[0] like we do RExC_open_parens[0]
                     * above */
                }
                else if (RExC_npar > RExC_parens_buf_size) {
                    I32 old_size = RExC_parens_buf_size;

                    RExC_parens_buf_size *= 2;

                    Renew(RExC_open_parens, RExC_parens_buf_size,
                            regnode_offset);
                    Zero(RExC_open_parens + old_size,
                            RExC_parens_buf_size - old_size, regnode_offset);

                    Renew(RExC_close_parens, RExC_parens_buf_size,
                            regnode_offset);
                    Zero(RExC_close_parens + old_size,
                            RExC_parens_buf_size - old_size, regnode_offset);
                }
            }

	    ret = reganode(pRExC_state, OPEN, parno);
            if (!RExC_nestroot)
                RExC_nestroot = parno;
            if (RExC_open_parens && !RExC_open_parens[parno])
            {
                DEBUG_OPTIMISE_MORE_r(Perl_re_printf( aTHX_
                    "%*s%*s Setting open paren #%" IVdf " to %d\n",
                    22, "|    |", (int)(depth * 2 + 1), "",
                    (IV)parno, ret));
                RExC_open_parens[parno]= ret;
            }

            Set_Node_Length(REGNODE_p(ret), 1); /* MJD */
            Set_Node_Offset(REGNODE_p(ret), RExC_parse); /* MJD */
	    is_open = 1;
	} else {
            /* with RXf_PMf_NOCAPTURE treat (...) as (?:...) */
            paren = ':';
	    ret = 0;
	}
        }
    }
    else                        /* ! paren */
	ret = 0;

   parse_rest:
    /* Pick up the branches, linking them together. */
    parse_start = RExC_parse;   /* MJD */
    br = regbranch(pRExC_state, &flags, 1, depth+1);

    /*     branch_len = (paren != 0); */

    if (br == 0) {
        RETURN_FAIL_ON_RESTART(flags, flagp);
        FAIL2("panic: regbranch returned failure, flags=%#" UVxf, (UV) flags);
    }
    if (*RExC_parse == '|') {
	if (RExC_use_BRANCHJ) {
	    reginsert(pRExC_state, BRANCHJ, br, depth+1);
	}
	else {                  /* MJD */
	    reginsert(pRExC_state, BRANCH, br, depth+1);
            Set_Node_Length(REGNODE_p(br), paren != 0);
            Set_Node_Offset_To_R(br, parse_start-RExC_start);
        }
	have_branch = 1;
    }
    else if (paren == ':') {
	*flagp |= flags&SIMPLE;
    }
    if (is_open) {				/* Starts with OPEN. */
        if (! REGTAIL(pRExC_state, ret, br)) {  /* OPEN -> first. */
            REQUIRE_BRANCHJ(flagp, 0);
        }
    }
    else if (paren != '?')		/* Not Conditional */
	ret = br;
    *flagp |= flags & (SPSTART | HASWIDTH | POSTPONED);
    lastbr = br;
    while (*RExC_parse == '|') {
	if (RExC_use_BRANCHJ) {
            bool shut_gcc_up;

	    ender = reganode(pRExC_state, LONGJMP, 0);

            /* Append to the previous. */
            shut_gcc_up = REGTAIL(pRExC_state,
                         REGNODE_OFFSET(NEXTOPER(NEXTOPER(REGNODE_p(lastbr)))),
                         ender);
            PERL_UNUSED_VAR(shut_gcc_up);
	}
	nextchar(pRExC_state);
	if (freeze_paren) {
	    if (RExC_npar > after_freeze)
	        after_freeze = RExC_npar;
            RExC_npar = freeze_paren;
        }
        br = regbranch(pRExC_state, &flags, 0, depth+1);

	if (br == 0) {
            RETURN_FAIL_ON_RESTART(flags, flagp);
            FAIL2("panic: regbranch returned failure, flags=%#" UVxf, (UV) flags);
        }
        if (!  REGTAIL(pRExC_state, lastbr, br)) {  /* BRANCH -> BRANCH. */
            REQUIRE_BRANCHJ(flagp, 0);
        }
	lastbr = br;
	*flagp |= flags & (SPSTART | HASWIDTH | POSTPONED);
    }

    if (have_branch || paren != ':') {
        regnode * br;

	/* Make a closing node, and hook it on the end. */
	switch (paren) {
	case ':':
	    ender = reg_node(pRExC_state, TAIL);
	    break;
	case 1: case 2:
	    ender = reganode(pRExC_state, CLOSE, parno);
            if ( RExC_close_parens ) {
                DEBUG_OPTIMISE_MORE_r(Perl_re_printf( aTHX_
                        "%*s%*s Setting close paren #%" IVdf " to %d\n",
                        22, "|    |", (int)(depth * 2 + 1), "",
                        (IV)parno, ender));
                RExC_close_parens[parno]= ender;
	        if (RExC_nestroot == parno)
	            RExC_nestroot = 0;
	    }
            Set_Node_Offset(REGNODE_p(ender), RExC_parse+1); /* MJD */
            Set_Node_Length(REGNODE_p(ender), 1); /* MJD */
	    break;
	case 's':
	    ender = reg_node(pRExC_state, SRCLOSE);
            RExC_in_script_run = 0;
	    break;
	case '<':
        case 'a':
        case 'A':
        case 'b':
        case 'B':
	case ',':
	case '=':
	case '!':
	    *flagp &= ~HASWIDTH;
	    /* FALLTHROUGH */
        case 't':   /* aTomic */
	case '>':
	    ender = reg_node(pRExC_state, SUCCEED);
	    break;
	case 0:
	    ender = reg_node(pRExC_state, END);
            assert(!RExC_end_op); /* there can only be one! */
            RExC_end_op = REGNODE_p(ender);
            if (RExC_close_parens) {
                DEBUG_OPTIMISE_MORE_r(Perl_re_printf( aTHX_
                    "%*s%*s Setting close paren #0 (END) to %d\n",
                    22, "|    |", (int)(depth * 2 + 1), "",
                    ender));

                RExC_close_parens[0]= ender;
            }
	    break;
	}
        DEBUG_PARSE_r({
            DEBUG_PARSE_MSG("lsbr");
            regprop(RExC_rx, RExC_mysv1, REGNODE_p(lastbr), NULL, pRExC_state);
            regprop(RExC_rx, RExC_mysv2, REGNODE_p(ender), NULL, pRExC_state);
            Perl_re_printf( aTHX_  "~ tying lastbr %s (%" IVdf ") to ender %s (%" IVdf ") offset %" IVdf "\n",
                          SvPV_nolen_const(RExC_mysv1),
                          (IV)lastbr,
                          SvPV_nolen_const(RExC_mysv2),
                          (IV)ender,
                          (IV)(ender - lastbr)
            );
        });
        if (! REGTAIL(pRExC_state, lastbr, ender)) {
            REQUIRE_BRANCHJ(flagp, 0);
        }

	if (have_branch) {
            char is_nothing= 1;
	    if (depth==1)
                RExC_seen |= REG_TOP_LEVEL_BRANCHES_SEEN;

	    /* Hook the tails of the branches to the closing node. */
	    for (br = REGNODE_p(ret); br; br = regnext(br)) {
		const U8 op = PL_regkind[OP(br)];
		if (op == BRANCH) {
                    if (! REGTAIL_STUDY(pRExC_state,
                                        REGNODE_OFFSET(NEXTOPER(br)),
                                        ender))
                    {
                        REQUIRE_BRANCHJ(flagp, 0);
                    }
                    if ( OP(NEXTOPER(br)) != NOTHING
                         || regnext(NEXTOPER(br)) != REGNODE_p(ender))
                        is_nothing= 0;
		}
		else if (op == BRANCHJ) {
                    bool shut_gcc_up = REGTAIL_STUDY(pRExC_state,
                                        REGNODE_OFFSET(NEXTOPER(NEXTOPER(br))),
                                        ender);
                    PERL_UNUSED_VAR(shut_gcc_up);
                    /* for now we always disable this optimisation * /
                    if ( OP(NEXTOPER(NEXTOPER(br))) != NOTHING
                         || regnext(NEXTOPER(NEXTOPER(br))) != REGNODE_p(ender))
                    */
                        is_nothing= 0;
		}
	    }
            if (is_nothing) {
                regnode * ret_as_regnode = REGNODE_p(ret);
                br= PL_regkind[OP(ret_as_regnode)] != BRANCH
                               ? regnext(ret_as_regnode)
                               : ret_as_regnode;
                DEBUG_PARSE_r({
                    DEBUG_PARSE_MSG("NADA");
                    regprop(RExC_rx, RExC_mysv1, ret_as_regnode,
                                     NULL, pRExC_state);
                    regprop(RExC_rx, RExC_mysv2, REGNODE_p(ender),
                                     NULL, pRExC_state);
                    Perl_re_printf( aTHX_  "~ converting ret %s (%" IVdf ") to ender %s (%" IVdf ") offset %" IVdf "\n",
                                  SvPV_nolen_const(RExC_mysv1),
                                  (IV)REG_NODE_NUM(ret_as_regnode),
                                  SvPV_nolen_const(RExC_mysv2),
                                  (IV)ender,
                                  (IV)(ender - ret)
                    );
                });
                OP(br)= NOTHING;
                if (OP(REGNODE_p(ender)) == TAIL) {
                    NEXT_OFF(br)= 0;
                    RExC_emit= REGNODE_OFFSET(br) + 1;
                } else {
                    regnode *opt;
                    for ( opt= br + 1; opt < REGNODE_p(ender) ; opt++ )
                        OP(opt)= OPTIMIZED;
                    NEXT_OFF(br)= REGNODE_p(ender) - br;
                }
            }
	}
    }

    {
        const char *p;
         /* Even/odd or x=don't care: 010101x10x */
        static const char parens[] = "=!aA<,>Bbt";
         /* flag below is set to 0 up through 'A'; 1 for larger */

	if (paren && (p = strchr(parens, paren))) {
	    U8 node = ((p - parens) % 2) ? UNLESSM : IFMATCH;
	    int flag = (p - parens) > 3;

	    if (paren == '>' || paren == 't') {
		node = SUSPEND, flag = 0;
            }

	    reginsert(pRExC_state, node, ret, depth+1);
            Set_Node_Cur_Length(REGNODE_p(ret), parse_start);
	    Set_Node_Offset(REGNODE_p(ret), parse_start + 1);
	    FLAGS(REGNODE_p(ret)) = flag;
            if (! REGTAIL_STUDY(pRExC_state, ret, reg_node(pRExC_state, TAIL)))
            {
                REQUIRE_BRANCHJ(flagp, 0);
            }
	}
    }

    /* Check for proper termination. */
    if (paren) {
        /* restore original flags, but keep (?p) and, if we've encountered
         * something in the parse that changes /d rules into /u, keep the /u */
	RExC_flags = oregflags | (RExC_flags & RXf_PMf_KEEPCOPY);
        if (DEPENDS_SEMANTICS && RExC_uni_semantics) {
            set_regex_charset(&RExC_flags, REGEX_UNICODE_CHARSET);
        }
	if (RExC_parse >= RExC_end || UCHARAT(RExC_parse) != ')') {
	    RExC_parse = oregcomp_parse;
	    vFAIL("Unmatched (");
	}
	nextchar(pRExC_state);
    }
    else if (!paren && RExC_parse < RExC_end) {
	if (*RExC_parse == ')') {
	    RExC_parse++;
	    vFAIL("Unmatched )");
	}
	else
	    FAIL("Junk on end of regexp");	/* "Can't happen". */
	NOT_REACHED; /* NOTREACHED */
    }

    if (RExC_in_lookbehind) {
	RExC_in_lookbehind--;
    }
    if (after_freeze > RExC_npar)
        RExC_npar = after_freeze;
    return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 *
 * On success, returns the offset at which any next node should be placed into
 * the regex engine program being compiled.
 *
 * Returns 0 otherwise, setting flagp to RESTART_PARSE if the parse needs
 * to be restarted, or'd with NEED_UTF8 if the pattern needs to be upgraded to
 * UTF-8
 */
STATIC regnode_offset
S_regbranch(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, I32 first, U32 depth)
{
    regnode_offset ret;
    regnode_offset chain = 0;
    regnode_offset latest;
    I32 flags = 0, c = 0;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGBRANCH;

    DEBUG_PARSE("brnc");

    if (first)
	ret = 0;
    else {
	if (RExC_use_BRANCHJ)
	    ret = reganode(pRExC_state, BRANCHJ, 0);
	else {
	    ret = reg_node(pRExC_state, BRANCH);
            Set_Node_Length(REGNODE_p(ret), 1);
        }
    }

    *flagp = WORST;			/* Tentatively. */

    skip_to_be_ignored_text(pRExC_state, &RExC_parse,
                            FALSE /* Don't force to /x */ );
    while (RExC_parse < RExC_end && *RExC_parse != '|' && *RExC_parse != ')') {
	flags &= ~TRYAGAIN;
        latest = regpiece(pRExC_state, &flags, depth+1);
	if (latest == 0) {
	    if (flags & TRYAGAIN)
		continue;
            RETURN_FAIL_ON_RESTART(flags, flagp);
            FAIL2("panic: regpiece returned failure, flags=%#" UVxf, (UV) flags);
	}
	else if (ret == 0)
            ret = latest;
	*flagp |= flags&(HASWIDTH|POSTPONED);
	if (chain == 0) 	/* First piece. */
	    *flagp |= flags&SPSTART;
	else {
	    /* FIXME adding one for every branch after the first is probably
	     * excessive now we have TRIE support. (hv) */
	    MARK_NAUGHTY(1);
            if (! REGTAIL(pRExC_state, chain, latest)) {
                /* XXX We could just redo this branch, but figuring out what
                 * bookkeeping needs to be reset is a pain, and it's likely
                 * that other branches that goto END will also be too large */
                REQUIRE_BRANCHJ(flagp, 0);
            }
	}
	chain = latest;
	c++;
    }
    if (chain == 0) {	/* Loop ran zero times. */
	chain = reg_node(pRExC_state, NOTHING);
	if (ret == 0)
	    ret = chain;
    }
    if (c == 1) {
	*flagp |= flags&SIMPLE;
    }

    return ret;
}

/*
 - regpiece - something followed by possible quantifier * + ? {n,m}
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 *
 * On success, returns the offset at which any next node should be placed into
 * the regex engine program being compiled.
 *
 * Returns 0 otherwise, with *flagp set to indicate why:
 *  TRYAGAIN        if regatom() returns 0 with TRYAGAIN.
 *  RESTART_PARSE   if the parse needs to be restarted, or'd with
 *                  NEED_UTF8 if the pattern needs to be upgraded to UTF-8.
 */
STATIC regnode_offset
S_regpiece(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, U32 depth)
{
    regnode_offset ret;
    char op;
    char *next;
    I32 flags;
    const char * const origparse = RExC_parse;
    I32 min;
    I32 max = REG_INFTY;
#ifdef RE_TRACK_PATTERN_OFFSETS
    char *parse_start;
#endif
    const char *maxpos = NULL;
    UV uv;

    /* Save the original in case we change the emitted regop to a FAIL. */
    const regnode_offset orig_emit = RExC_emit;

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGPIECE;

    DEBUG_PARSE("piec");

    ret = regatom(pRExC_state, &flags, depth+1);
    if (ret == 0) {
        RETURN_FAIL_ON_RESTART_OR_FLAGS(flags, flagp, TRYAGAIN);
        FAIL2("panic: regatom returned failure, flags=%#" UVxf, (UV) flags);
    }

    op = *RExC_parse;

    if (op == '{' && regcurly(RExC_parse)) {
	maxpos = NULL;
#ifdef RE_TRACK_PATTERN_OFFSETS
        parse_start = RExC_parse; /* MJD */
#endif
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
            const char* endptr;
	    if (!maxpos)
		maxpos = next;
	    RExC_parse++;
            if (isDIGIT(*RExC_parse)) {
                endptr = RExC_end;
                if (!grok_atoUV(RExC_parse, &uv, &endptr))
                    vFAIL("Invalid quantifier in {,}");
                if (uv >= REG_INFTY)
                    vFAIL2("Quantifier in {,} bigger than %d", REG_INFTY - 1);
                min = (I32)uv;
            } else {
                min = 0;
            }
	    if (*maxpos == ',')
		maxpos++;
	    else
		maxpos = RExC_parse;
            if (isDIGIT(*maxpos)) {
                endptr = RExC_end;
                if (!grok_atoUV(maxpos, &uv, &endptr))
                    vFAIL("Invalid quantifier in {,}");
                if (uv >= REG_INFTY)
                    vFAIL2("Quantifier in {,} bigger than %d", REG_INFTY - 1);
                max = (I32)uv;
            } else {
		max = REG_INFTY;		/* meaning "infinity" */
            }
	    RExC_parse = next;
	    nextchar(pRExC_state);
            if (max < min) {    /* If can't match, warn and optimize to fail
                                   unconditionally */
                reginsert(pRExC_state, OPFAIL, orig_emit, depth+1);
                ckWARNreg(RExC_parse, "Quantifier {n,m} with n > m can't match");
                NEXT_OFF(REGNODE_p(orig_emit)) =
                                    regarglen[OPFAIL] + NODE_STEP_REGNODE;
                return ret;
            }
            else if (min == max && *RExC_parse == '?')
            {
                ckWARN2reg(RExC_parse + 1,
                           "Useless use of greediness modifier '%c'",
                           *RExC_parse);
            }

	  do_curly:
	    if ((flags&SIMPLE)) {
                if (min == 0 && max == REG_INFTY) {
                    reginsert(pRExC_state, STAR, ret, depth+1);
                    MARK_NAUGHTY(4);
                    RExC_seen |= REG_UNBOUNDED_QUANTIFIER_SEEN;
                    goto nest_check;
                }
                if (min == 1 && max == REG_INFTY) {
                    reginsert(pRExC_state, PLUS, ret, depth+1);
                    MARK_NAUGHTY(3);
                    RExC_seen |= REG_UNBOUNDED_QUANTIFIER_SEEN;
                    goto nest_check;
                }
                MARK_NAUGHTY_EXP(2, 2);
		reginsert(pRExC_state, CURLY, ret, depth+1);
                Set_Node_Offset(REGNODE_p(ret), parse_start+1); /* MJD */
                Set_Node_Cur_Length(REGNODE_p(ret), parse_start);
	    }
	    else {
		const regnode_offset w = reg_node(pRExC_state, WHILEM);

		FLAGS(REGNODE_p(w)) = 0;
                if (!  REGTAIL(pRExC_state, ret, w)) {
                    REQUIRE_BRANCHJ(flagp, 0);
                }
		if (RExC_use_BRANCHJ) {
		    reginsert(pRExC_state, LONGJMP, ret, depth+1);
		    reginsert(pRExC_state, NOTHING, ret, depth+1);
		    NEXT_OFF(REGNODE_p(ret)) = 3;	/* Go over LONGJMP. */
		}
		reginsert(pRExC_state, CURLYX, ret, depth+1);
                                /* MJD hk */
                Set_Node_Offset(REGNODE_p(ret), parse_start+1);
                Set_Node_Length(REGNODE_p(ret),
                                op == '{' ? (RExC_parse - parse_start) : 1);

		if (RExC_use_BRANCHJ)
                    NEXT_OFF(REGNODE_p(ret)) = 3;   /* Go over NOTHING to
                                                       LONGJMP. */
                if (! REGTAIL(pRExC_state, ret, reg_node(pRExC_state,
                                                          NOTHING)))
                {
                    REQUIRE_BRANCHJ(flagp, 0);
                }
                RExC_whilem_seen++;
                MARK_NAUGHTY_EXP(1, 4);     /* compound interest */
	    }
	    FLAGS(REGNODE_p(ret)) = 0;

	    if (min > 0)
		*flagp = WORST;
	    if (max > 0)
		*flagp |= HASWIDTH;
            ARG1_SET(REGNODE_p(ret), (U16)min);
            ARG2_SET(REGNODE_p(ret), (U16)max);
            if (max == REG_INFTY)
                RExC_seen |= REG_UNBOUNDED_QUANTIFIER_SEEN;

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

#ifdef RE_TRACK_PATTERN_OFFSETS
    parse_start = RExC_parse;
#endif
    nextchar(pRExC_state);

    *flagp = (op != '+') ? (WORST|SPSTART|HASWIDTH) : (WORST|HASWIDTH);

    if (op == '*') {
	min = 0;
	goto do_curly;
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
    if (!(flags&(HASWIDTH|POSTPONED)) && max > REG_INFTY/3) {
	ckWARN2reg(RExC_parse,
		   "%" UTF8f " matches null string many times",
		   UTF8fARG(UTF, (RExC_parse >= origparse
                                 ? RExC_parse - origparse
                                 : 0),
		   origparse));
    }

    if (*RExC_parse == '?') {
	nextchar(pRExC_state);
	reginsert(pRExC_state, MINMOD, ret, depth+1);
        if (! REGTAIL(pRExC_state, ret, ret + NODE_STEP_REGNODE)) {
            REQUIRE_BRANCHJ(flagp, 0);
        }
    }
    else if (*RExC_parse == '+') {
        regnode_offset ender;
        nextchar(pRExC_state);
        ender = reg_node(pRExC_state, SUCCEED);
        if (! REGTAIL(pRExC_state, ret, ender)) {
            REQUIRE_BRANCHJ(flagp, 0);
        }
        reginsert(pRExC_state, SUSPEND, ret, depth+1);
        ender = reg_node(pRExC_state, TAIL);
        if (! REGTAIL(pRExC_state, ret, ender)) {
            REQUIRE_BRANCHJ(flagp, 0);
        }
    }

    if (ISMULT2(RExC_parse)) {
	RExC_parse++;
	vFAIL("Nested quantifiers");
    }

    return(ret);
}

STATIC bool
S_grok_bslash_N(pTHX_ RExC_state_t *pRExC_state,
                regnode_offset * node_p,
                UV * code_point_p,
                int * cp_count,
                I32 * flagp,
                const bool strict,
                const U32 depth
    )
{
 /* This routine teases apart the various meanings of \N and returns
  * accordingly.  The input parameters constrain which meaning(s) is/are valid
  * in the current context.
  *
  * Exactly one of <node_p> and <code_point_p> must be non-NULL.
  *
  * If <code_point_p> is not NULL, the context is expecting the result to be a
  * single code point.  If this \N instance turns out to a single code point,
  * the function returns TRUE and sets *code_point_p to that code point.
  *
  * If <node_p> is not NULL, the context is expecting the result to be one of
  * the things representable by a regnode.  If this \N instance turns out to be
  * one such, the function generates the regnode, returns TRUE and sets *node_p
  * to point to the offset of that regnode into the regex engine program being
  * compiled.
  *
  * If this instance of \N isn't legal in any context, this function will
  * generate a fatal error and not return.
  *
  * On input, RExC_parse should point to the first char following the \N at the
  * time of the call.  On successful return, RExC_parse will have been updated
  * to point to just after the sequence identified by this routine.  Also
  * *flagp has been updated as needed.
  *
  * When there is some problem with the current context and this \N instance,
  * the function returns FALSE, without advancing RExC_parse, nor setting
  * *node_p, nor *code_point_p, nor *flagp.
  *
  * If <cp_count> is not NULL, the caller wants to know the length (in code
  * points) that this \N sequence matches.  This is set, and the input is
  * parsed for errors, even if the function returns FALSE, as detailed below.
  *
  * There are 6 possibilities here, as detailed in the next 6 paragraphs.
  *
  * Probably the most common case is for the \N to specify a single code point.
  * *cp_count will be set to 1, and *code_point_p will be set to that code
  * point.
  *
  * Another possibility is for the input to be an empty \N{}.  This is no
  * longer accepted, and will generate a fatal error.
  *
  * Another possibility is for a custom charnames handler to be in effect which
  * translates the input name to an empty string.  *cp_count will be set to 0.
  * *node_p will be set to a generated NOTHING node.
  *
  * Still another possibility is for the \N to mean [^\n]. *cp_count will be
  * set to 0. *node_p will be set to a generated REG_ANY node.
  *
  * The fifth possibility is that \N resolves to a sequence of more than one
  * code points.  *cp_count will be set to the number of code points in the
  * sequence. *node_p will be set to a generated node returned by this
  * function calling S_reg().
  *
  * The final possibility is that it is premature to be calling this function;
  * the parse needs to be restarted.  This can happen when this changes from
  * /d to /u rules, or when the pattern needs to be upgraded to UTF-8.  The
  * latter occurs only when the fifth possibility would otherwise be in
  * effect, and is because one of those code points requires the pattern to be
  * recompiled as UTF-8.  The function returns FALSE, and sets the
  * RESTART_PARSE and NEED_UTF8 flags in *flagp, as appropriate.  When this
  * happens, the caller needs to desist from continuing parsing, and return
  * this information to its caller.  This is not set for when there is only one
  * code point, as this can be called as part of an ANYOF node, and they can
  * store above-Latin1 code points without the pattern having to be in UTF-8.
  *
  * For non-single-quoted regexes, the tokenizer has resolved character and
  * sequence names inside \N{...} into their Unicode values, normalizing the
  * result into what we should see here: '\N{U+c1.c2...}', where c1... are the
  * hex-represented code points in the sequence.  This is done there because
  * the names can vary based on what charnames pragma is in scope at the time,
  * so we need a way to take a snapshot of what they resolve to at the time of
  * the original parse. [perl #56444].
  *
  * That parsing is skipped for single-quoted regexes, so here we may get
  * '\N{NAME}', which is parsed now.  If the single-quoted regex is something
  * like '\N{U+41}', that code point is Unicode, and has to be translated into
  * the native character set for non-ASCII platforms.  The other possibilities
  * are already native, so no translation is done. */

    char * endbrace;    /* points to '}' following the name */
    char* p = RExC_parse; /* Temporary */

    SV * substitute_parse = NULL;
    char *orig_end;
    char *save_start;
    I32 flags;

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_GROK_BSLASH_N;

    GET_RE_DEBUG_FLAGS;

    assert(cBOOL(node_p) ^ cBOOL(code_point_p));  /* Exactly one should be set */
    assert(! (node_p && cp_count));               /* At most 1 should be set */

    if (cp_count) {     /* Initialize return for the most common case */
        *cp_count = 1;
    }

    /* The [^\n] meaning of \N ignores spaces and comments under the /x
     * modifier.  The other meanings do not, so use a temporary until we find
     * out which we are being called with */
    skip_to_be_ignored_text(pRExC_state, &p,
                            FALSE /* Don't force to /x */ );

    /* Disambiguate between \N meaning a named character versus \N meaning
     * [^\n].  The latter is assumed when the {...} following the \N is a legal
     * quantifier, or if there is no '{' at all */
    if (*p != '{' || regcurly(p)) {
        RExC_parse = p;
        if (cp_count) {
            *cp_count = -1;
        }

        if (! node_p) {
            return FALSE;
        }

        *node_p = reg_node(pRExC_state, REG_ANY);
        *flagp |= HASWIDTH|SIMPLE;
        MARK_NAUGHTY(1);
        Set_Node_Length(REGNODE_p(*(node_p)), 1); /* MJD */
        return TRUE;
    }

    /* The test above made sure that the next real character is a '{', but
     * under the /x modifier, it could be separated by space (or a comment and
     * \n) and this is not allowed (for consistency with \x{...} and the
     * tokenizer handling of \N{NAME}). */
    if (*RExC_parse != '{') {
        vFAIL("Missing braces on \\N{}");
    }

    RExC_parse++;       /* Skip past the '{' */

    endbrace = (char *) memchr(RExC_parse, '}', RExC_end - RExC_parse);
    if (! endbrace) { /* no trailing brace */
        vFAIL2("Missing right brace on \\%c{}", 'N');
    }

    /* Here, we have decided it should be a named character or sequence.  These
     * imply Unicode semantics */
    REQUIRE_UNI_RULES(flagp, FALSE);

    /* \N{_} is what toke.c returns to us to indicate a name that evaluates to
     * nothing at all (not allowed under strict) */
    if (endbrace - RExC_parse == 1 && *RExC_parse == '_') {
        RExC_parse = endbrace;
        if (strict) {
            RExC_parse++;   /* Position after the "}" */
            vFAIL("Zero length \\N{}");
        }

        if (cp_count) {
            *cp_count = 0;
        }
        nextchar(pRExC_state);
        if (! node_p) {
            return FALSE;
        }

        *node_p = reg_node(pRExC_state, NOTHING);
        return TRUE;
    }

    if (endbrace - RExC_parse < 2 || ! strBEGINs(RExC_parse, "U+")) {

        /* Here, the name isn't of the form  U+....  This can happen if the
         * pattern is single-quoted, so didn't get evaluated in toke.c.  Now
         * is the time to find out what the name means */

        const STRLEN name_len = endbrace - RExC_parse;
        SV *  value_sv;     /* What does this name evaluate to */
        SV ** value_svp;
        const U8 * value;   /* string of name's value */
        STRLEN value_len;   /* and its length */

        /*  RExC_unlexed_names is a hash of names that weren't evaluated by
         *  toke.c, and their values. Make sure is initialized */
        if (! RExC_unlexed_names) {
            RExC_unlexed_names = newHV();
        }

        /* If we have already seen this name in this pattern, use that.  This
         * allows us to only call the charnames handler once per name per
         * pattern.  A broken or malicious handler could return something
         * different each time, which could cause the results to vary depending
         * on if something gets added or subtracted from the pattern that
         * causes the number of passes to change, for example */
        if ((value_svp = hv_fetch(RExC_unlexed_names, RExC_parse,
                                                      name_len, 0)))
        {
            value_sv = *value_svp;
        }
        else { /* Otherwise we have to go out and get the name */
            const char * error_msg = NULL;
            value_sv = get_and_check_backslash_N_name(RExC_parse, endbrace,
                                                      UTF,
                                                      &error_msg);
            if (error_msg) {
                RExC_parse = endbrace;
                vFAIL(error_msg);
            }

            /* If no error message, should have gotten a valid return */
            assert (value_sv);

            /* Save the name's meaning for later use */
            if (! hv_store(RExC_unlexed_names, RExC_parse, name_len,
                           value_sv, 0))
            {
                Perl_croak(aTHX_ "panic: hv_store() unexpectedly failed");
            }
        }

        /* Here, we have the value the name evaluates to in 'value_sv' */
        value = (U8 *) SvPV(value_sv, value_len);

        /* See if the result is one code point vs 0 or multiple */
        if (value_len > 0 && value_len <= (UV) ((SvUTF8(value_sv))
                                               ? UTF8SKIP(value)
                                               : 1))
        {
            /* Here, exactly one code point.  If that isn't what is wanted,
             * fail */
            if (! code_point_p) {
                RExC_parse = p;
                return FALSE;
            }

            /* Convert from string to numeric code point */
            *code_point_p = (SvUTF8(value_sv))
                            ? valid_utf8_to_uvchr(value, NULL)
                            : *value;

            /* Have parsed this entire single code point \N{...}.  *cp_count
             * has already been set to 1, so don't do it again. */
            RExC_parse = endbrace;
            nextchar(pRExC_state);
            return TRUE;
        } /* End of is a single code point */

        /* Count the code points, if caller desires.  The API says to do this
         * even if we will later return FALSE */
        if (cp_count) {
            *cp_count = 0;

            *cp_count = (SvUTF8(value_sv))
                        ? utf8_length(value, value + value_len)
                        : value_len;
        }

        /* Fail if caller doesn't want to handle a multi-code-point sequence.
         * But don't back the pointer up if the caller wants to know how many
         * code points there are (they need to handle it themselves in this
         * case).  */
        if (! node_p) {
            if (! cp_count) {
                RExC_parse = p;
            }
            return FALSE;
        }

        /* Convert this to a sub-pattern of the form "(?: ... )", and then call
         * reg recursively to parse it.  That way, it retains its atomicness,
         * while not having to worry about any special handling that some code
         * points may have. */

        substitute_parse = newSVpvs("?:");
        sv_catsv(substitute_parse, value_sv);
        sv_catpv(substitute_parse, ")");

#ifdef EBCDIC
        /* The value should already be native, so no need to convert on EBCDIC
         * platforms.*/
        assert(! RExC_recode_x_to_native);
#endif

    }
    else {   /* \N{U+...} */
        Size_t count = 0;   /* code point count kept internally */

        /* We can get to here when the input is \N{U+...} or when toke.c has
         * converted a name to the \N{U+...} form.  This include changing a
         * name that evaluates to multiple code points to \N{U+c1.c2.c3 ...} */

        RExC_parse += 2;    /* Skip past the 'U+' */

        /* Code points are separated by dots.  The '}' terminates the whole
         * thing. */

        do {    /* Loop until the ending brace */
            UV cp = 0;
            char * start_digit;     /* The first of the current code point */
            if (! isXDIGIT(*RExC_parse)) {
                RExC_parse++;
                vFAIL("Invalid hexadecimal number in \\N{U+...}");
            }

            start_digit = RExC_parse;
            count++;

            /* Loop through the hex digits of the current code point */
            do {
                /* Adding this digit will shift the result 4 bits.  If that
                 * result would be above the legal max, it's overflow */
                if (cp > MAX_LEGAL_CP >> 4) {

                    /* Find the end of the code point */
                    do {
                        RExC_parse ++;
                    } while (isXDIGIT(*RExC_parse) || *RExC_parse == '_');

                    /* Be sure to synchronize this message with the similar one
                     * in utf8.c */
                    vFAIL4("Use of code point 0x%.*s is not allowed; the"
                        " permissible max is 0x%" UVxf,
                        (int) (RExC_parse - start_digit), start_digit,
                        MAX_LEGAL_CP);
                }

                /* Accumulate this (valid) digit into the running total */
                cp  = (cp << 4) + READ_XDIGIT(RExC_parse);

                /* READ_XDIGIT advanced the input pointer.  Ignore a single
                 * underscore separator */
                if (*RExC_parse == '_' && isXDIGIT(RExC_parse[1])) {
                    RExC_parse++;
                }
            } while (isXDIGIT(*RExC_parse));

            /* Here, have accumulated the next code point */
            if (RExC_parse >= endbrace) {   /* If done ... */
                if (count != 1) {
                    goto do_concat;
                }

                /* Here, is a single code point; fail if doesn't want that */
                if (! code_point_p) {
                    RExC_parse = p;
                    return FALSE;
                }

                /* A single code point is easy to handle; just return it */
                *code_point_p = UNI_TO_NATIVE(cp);
                RExC_parse = endbrace;
                nextchar(pRExC_state);
                return TRUE;
            }

            /* Here, the only legal thing would be a multiple character
             * sequence (of the form "\N{U+c1.c2. ... }".   So the next
             * character must be a dot (and the one after that can't be the
             * endbrace, or we'd have something like \N{U+100.} ) */
            if (*RExC_parse != '.' || RExC_parse + 1 >= endbrace) {
                RExC_parse += (RExC_orig_utf8)  /* point to after 1st invalid */
                                ? UTF8SKIP(RExC_parse)
                                : 1;
                if (RExC_parse >= endbrace) { /* Guard against malformed utf8 */
                    RExC_parse = endbrace;
                }
                vFAIL("Invalid hexadecimal number in \\N{U+...}");
            }

            /* Here, looks like its really a multiple character sequence.  Fail
             * if that's not what the caller wants.  But continue with counting
             * and error checking if they still want a count */
            if (! node_p && ! cp_count) {
                return FALSE;
            }

            /* What is done here is to convert this to a sub-pattern of the
             * form \x{char1}\x{char2}...  and then call reg recursively to
             * parse it (enclosing in "(?: ... )" ).  That way, it retains its
             * atomicness, while not having to worry about special handling
             * that some code points may have.  We don't create a subpattern,
             * but go through the motions of code point counting and error
             * checking, if the caller doesn't want a node returned. */

            if (node_p && count == 1) {
                substitute_parse = newSVpvs("?:");
            }

          do_concat:

            if (node_p) {
                /* Convert to notation the rest of the code understands */
                sv_catpvs(substitute_parse, "\\x{");
                sv_catpvn(substitute_parse, start_digit,
                                            RExC_parse - start_digit);
                sv_catpvs(substitute_parse, "}");
            }

            /* Move to after the dot (or ending brace the final time through.)
             * */
            RExC_parse++;
            count++;

        } while (RExC_parse < endbrace);

        if (! node_p) { /* Doesn't want the node */
            assert (cp_count);

            *cp_count = count;
            return FALSE;
        }

        sv_catpvs(substitute_parse, ")");

#ifdef EBCDIC
        /* The values are Unicode, and therefore have to be converted to native
         * on a non-Unicode (meaning non-ASCII) platform. */
        RExC_recode_x_to_native = 1;
#endif

    }

    /* Here, we have the string the name evaluates to, ready to be parsed,
     * stored in 'substitute_parse' as a series of valid "\x{...}\x{...}"
     * constructs.  This can be called from within a substitute parse already.
     * The error reporting mechanism doesn't work for 2 levels of this, but the
     * code above has validated this new construct, so there should be no
     * errors generated by the below.  And this isn' an exact copy, so the
     * mechanism to seamlessly deal with this won't work, so turn off warnings
     * during it */
    save_start = RExC_start;
    orig_end = RExC_end;

    RExC_parse = RExC_start = SvPVX(substitute_parse);
    RExC_end = RExC_parse + SvCUR(substitute_parse);
    TURN_OFF_WARNINGS_IN_SUBSTITUTE_PARSE;

    *node_p = reg(pRExC_state, 1, &flags, depth+1);

    /* Restore the saved values */
    RESTORE_WARNINGS;
    RExC_start = save_start;
    RExC_parse = endbrace;
    RExC_end = orig_end;
#ifdef EBCDIC
    RExC_recode_x_to_native = 0;
#endif

    SvREFCNT_dec_NN(substitute_parse);

    if (! *node_p) {
        RETURN_FAIL_ON_RESTART(flags, flagp);
        FAIL2("panic: reg returned failure to grok_bslash_N, flags=%#" UVxf,
            (UV) flags);
    }
    *flagp |= flags&(HASWIDTH|SPSTART|SIMPLE|POSTPONED);

    nextchar(pRExC_state);

    return TRUE;
}


PERL_STATIC_INLINE U8
S_compute_EXACTish(RExC_state_t *pRExC_state)
{
    U8 op;

    PERL_ARGS_ASSERT_COMPUTE_EXACTISH;

    if (! FOLD) {
        return (LOC)
                ? EXACTL
                : EXACT;
    }

    op = get_regex_charset(RExC_flags);
    if (op >= REGEX_ASCII_RESTRICTED_CHARSET) {
        op--; /* /a is same as /u, and map /aa's offset to what /a's would have
                 been, so there is no hole */
    }

    return op + EXACTF;
}

STATIC bool
S_new_regcurly(const char *s, const char *e)
{
    /* This is a temporary function designed to match the most lenient form of
     * a {m,n} quantifier we ever envision, with either number omitted, and
     * spaces anywhere between/before/after them.
     *
     * If this function fails, then the string it matches is very unlikely to
     * ever be considered a valid quantifier, so we can allow the '{' that
     * begins it to be considered as a literal */

    bool has_min = FALSE;
    bool has_max = FALSE;

    PERL_ARGS_ASSERT_NEW_REGCURLY;

    if (s >= e || *s++ != '{')
	return FALSE;

    while (s < e && isSPACE(*s)) {
        s++;
    }
    while (s < e && isDIGIT(*s)) {
        has_min = TRUE;
        s++;
    }
    while (s < e && isSPACE(*s)) {
        s++;
    }

    if (*s == ',') {
	s++;
        while (s < e && isSPACE(*s)) {
            s++;
        }
        while (s < e && isDIGIT(*s)) {
            has_max = TRUE;
            s++;
        }
        while (s < e && isSPACE(*s)) {
            s++;
        }
    }

    return s < e && *s == '}' && (has_min || has_max);
}

/* Parse backref decimal value, unless it's too big to sensibly be a backref,
 * in which case return I32_MAX (rather than possibly 32-bit wrapping) */

static I32
S_backref_value(char *p, char *e)
{
    const char* endptr = e;
    UV val;
    if (grok_atoUV(p, &val, &endptr) && val <= I32_MAX)
        return (I32)val;
    return I32_MAX;
}


/*
 - regatom - the lowest level

   Try to identify anything special at the start of the current parse position.
   If there is, then handle it as required. This may involve generating a
   single regop, such as for an assertion; or it may involve recursing, such as
   to handle a () structure.

   If the string doesn't start with something special then we gobble up
   as much literal text as we can.  If we encounter a quantifier, we have to
   back off the final literal character, as that quantifier applies to just it
   and not to the whole string of literals.

   Once we have been able to handle whatever type of thing started the
   sequence, we return the offset into the regex engine program being compiled
   at which any  next regnode should be placed.

   Returns 0, setting *flagp to TRYAGAIN if reg() returns 0 with TRYAGAIN.
   Returns 0, setting *flagp to RESTART_PARSE if the parse needs to be
   restarted, or'd with NEED_UTF8 if the pattern needs to be upgraded to UTF-8
   Otherwise does not return 0.

   Note: we have to be careful with escapes, as they can be both literal
   and special, and in the case of \10 and friends, context determines which.

   A summary of the code structure is:

   switch (first_byte) {
	cases for each special:
	    handle this special;
	    break;
	case '\\':
	    switch (2nd byte) {
		cases for each unambiguous special:
		    handle this special;
		    break;
		cases for each ambigous special/literal:
		    disambiguate;
		    if (special)  handle here
		    else goto defchar;
		default: // unambiguously literal:
		    goto defchar;
	    }
	default:  // is a literal char
	    // FALL THROUGH
	defchar:
	    create EXACTish node for literal;
	    while (more input and node isn't full) {
		switch (input_byte) {
		   cases for each special;
                       make sure parse pointer is set so that the next call to
                           regatom will see this special first
                       goto loopdone; // EXACTish node terminated by prev. char
		   default:
		       append char to EXACTISH node;
		}
	        get next input byte;
	    }
        loopdone:
   }
   return the generated node;

   Specifically there are two separate switches for handling
   escape sequences, with the one for handling literal escapes requiring
   a dummy entry for all of the special escapes that are actually handled
   by the other.

*/

STATIC regnode_offset
S_regatom(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, U32 depth)
{
    dVAR;
    regnode_offset ret = 0;
    I32 flags = 0;
    char *parse_start;
    U8 op;
    int invert = 0;
    U8 arg;

    GET_RE_DEBUG_FLAGS_DECL;

    *flagp = WORST;		/* Tentatively. */

    DEBUG_PARSE("atom");

    PERL_ARGS_ASSERT_REGATOM;

  tryagain:
    parse_start = RExC_parse;
    assert(RExC_parse < RExC_end);
    switch ((U8)*RExC_parse) {
    case '^':
	RExC_seen_zerolen++;
	nextchar(pRExC_state);
	if (RExC_flags & RXf_PMf_MULTILINE)
	    ret = reg_node(pRExC_state, MBOL);
	else
	    ret = reg_node(pRExC_state, SBOL);
        Set_Node_Length(REGNODE_p(ret), 1); /* MJD */
	break;
    case '$':
	nextchar(pRExC_state);
	if (*RExC_parse)
	    RExC_seen_zerolen++;
	if (RExC_flags & RXf_PMf_MULTILINE)
	    ret = reg_node(pRExC_state, MEOL);
	else
	    ret = reg_node(pRExC_state, SEOL);
        Set_Node_Length(REGNODE_p(ret), 1); /* MJD */
	break;
    case '.':
	nextchar(pRExC_state);
	if (RExC_flags & RXf_PMf_SINGLELINE)
	    ret = reg_node(pRExC_state, SANY);
	else
	    ret = reg_node(pRExC_state, REG_ANY);
	*flagp |= HASWIDTH|SIMPLE;
	MARK_NAUGHTY(1);
        Set_Node_Length(REGNODE_p(ret), 1); /* MJD */
	break;
    case '[':
    {
	char * const oregcomp_parse = ++RExC_parse;
        ret = regclass(pRExC_state, flagp, depth+1,
                       FALSE, /* means parse the whole char class */
                       TRUE, /* allow multi-char folds */
                       FALSE, /* don't silence non-portable warnings. */
                       (bool) RExC_strict,
                       TRUE, /* Allow an optimized regnode result */
                       NULL);
        if (ret == 0) {
            RETURN_FAIL_ON_RESTART_FLAGP(flagp);
            FAIL2("panic: regclass returned failure to regatom, flags=%#" UVxf,
                  (UV) *flagp);
        }
	if (*RExC_parse != ']') {
	    RExC_parse = oregcomp_parse;
	    vFAIL("Unmatched [");
	}
	nextchar(pRExC_state);
        Set_Node_Length(REGNODE_p(ret), RExC_parse - oregcomp_parse + 1); /* MJD */
	break;
    }
    case '(':
	nextchar(pRExC_state);
        ret = reg(pRExC_state, 2, &flags, depth+1);
	if (ret == 0) {
		if (flags & TRYAGAIN) {
		    if (RExC_parse >= RExC_end) {
			 /* Make parent create an empty node if needed. */
			*flagp |= TRYAGAIN;
			return(0);
		    }
		    goto tryagain;
		}
                RETURN_FAIL_ON_RESTART(flags, flagp);
                FAIL2("panic: reg returned failure to regatom, flags=%#" UVxf,
                                                                 (UV) flags);
	}
	*flagp |= flags&(HASWIDTH|SPSTART|SIMPLE|POSTPONED);
	break;
    case '|':
    case ')':
	if (flags & TRYAGAIN) {
	    *flagp |= TRYAGAIN;
	    return 0;
	}
	vFAIL("Internal urp");
				/* Supposed to be caught earlier. */
	break;
    case '?':
    case '+':
    case '*':
	RExC_parse++;
	vFAIL("Quantifier follows nothing");
	break;
    case '\\':
	/* Special Escapes

	   This switch handles escape sequences that resolve to some kind
	   of special regop and not to literal text. Escape sequences that
	   resolve to literal text are handled below in the switch marked
	   "Literal Escapes".

	   Every entry in this switch *must* have a corresponding entry
	   in the literal escape switch. However, the opposite is not
	   required, as the default for this switch is to jump to the
	   literal text handling code.
	*/
	RExC_parse++;
	switch ((U8)*RExC_parse) {
	/* Special Escapes */
	case 'A':
	    RExC_seen_zerolen++;
	    ret = reg_node(pRExC_state, SBOL);
            /* SBOL is shared with /^/ so we set the flags so we can tell
             * /\A/ from /^/ in split. */
            FLAGS(REGNODE_p(ret)) = 1;
	    *flagp |= SIMPLE;
	    goto finish_meta_pat;
	case 'G':
	    ret = reg_node(pRExC_state, GPOS);
            RExC_seen |= REG_GPOS_SEEN;
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
            RExC_seen |= REG_LOOKBEHIND_SEEN;
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
	    vFAIL("\\C no longer supported");
	case 'X':
	    ret = reg_node(pRExC_state, CLUMP);
	    *flagp |= HASWIDTH;
	    goto finish_meta_pat;

	case 'W':
            invert = 1;
            /* FALLTHROUGH */
	case 'w':
            arg = ANYOF_WORDCHAR;
            goto join_posix;

	case 'B':
            invert = 1;
            /* FALLTHROUGH */
	case 'b':
          {
            U8 flags = 0;
	    regex_charset charset = get_regex_charset(RExC_flags);

	    RExC_seen_zerolen++;
            RExC_seen |= REG_LOOKBEHIND_SEEN;
	    op = BOUND + charset;

	    if (RExC_parse >= RExC_end || *(RExC_parse + 1) != '{') {
                flags = TRADITIONAL_BOUND;
                if (op > BOUNDA) {  /* /aa is same as /a */
                    op = BOUNDA;
                }
            }
            else {
                STRLEN length;
                char name = *RExC_parse;
                char * endbrace = NULL;
                RExC_parse += 2;
                endbrace = (char *) memchr(RExC_parse, '}', RExC_end - RExC_parse);

                if (! endbrace) {
                    vFAIL2("Missing right brace on \\%c{}", name);
                }
                /* XXX Need to decide whether to take spaces or not.  Should be
                 * consistent with \p{}, but that currently is SPACE, which
                 * means vertical too, which seems wrong
                 * while (isBLANK(*RExC_parse)) {
                    RExC_parse++;
                }*/
                if (endbrace == RExC_parse) {
                    RExC_parse++;  /* After the '}' */
                    vFAIL2("Empty \\%c{}", name);
                }
                length = endbrace - RExC_parse;
                /*while (isBLANK(*(RExC_parse + length - 1))) {
                    length--;
                }*/
                switch (*RExC_parse) {
                    case 'g':
                        if (    length != 1
                            && (memNEs(RExC_parse + 1, length - 1, "cb")))
                        {
                            goto bad_bound_type;
                        }
                        flags = GCB_BOUND;
                        break;
                    case 'l':
                        if (length != 2 || *(RExC_parse + 1) != 'b') {
                            goto bad_bound_type;
                        }
                        flags = LB_BOUND;
                        break;
                    case 's':
                        if (length != 2 || *(RExC_parse + 1) != 'b') {
                            goto bad_bound_type;
                        }
                        flags = SB_BOUND;
                        break;
                    case 'w':
                        if (length != 2 || *(RExC_parse + 1) != 'b') {
                            goto bad_bound_type;
                        }
                        flags = WB_BOUND;
                        break;
                    default:
                      bad_bound_type:
                        RExC_parse = endbrace;
			vFAIL2utf8f(
                            "'%" UTF8f "' is an unknown bound type",
			    UTF8fARG(UTF, length, endbrace - length));
                        NOT_REACHED; /*NOTREACHED*/
                }
                RExC_parse = endbrace;
                REQUIRE_UNI_RULES(flagp, 0);

                if (op == BOUND) {
                    op = BOUNDU;
                }
                else if (op >= BOUNDA) {  /* /aa is same as /a */
                    op = BOUNDU;
                    length += 4;

                    /* Don't have to worry about UTF-8, in this message because
                     * to get here the contents of the \b must be ASCII */
                    ckWARN4reg(RExC_parse + 1,  /* Include the '}' in msg */
                              "Using /u for '%.*s' instead of /%s",
                              (unsigned) length,
                              endbrace - length + 1,
                              (charset == REGEX_ASCII_RESTRICTED_CHARSET)
                              ? ASCII_RESTRICT_PAT_MODS
                              : ASCII_MORE_RESTRICT_PAT_MODS);
                }
	    }

            if (op == BOUND) {
                RExC_seen_d_op = TRUE;
            }
            else if (op == BOUNDL) {
                RExC_contains_locale = 1;
            }

            if (invert) {
                op += NBOUND - BOUND;
            }

	    ret = reg_node(pRExC_state, op);
            FLAGS(REGNODE_p(ret)) = flags;

	    *flagp |= SIMPLE;

	    goto finish_meta_pat;
          }

	case 'D':
            invert = 1;
            /* FALLTHROUGH */
	case 'd':
            arg = ANYOF_DIGIT;
            if (! DEPENDS_SEMANTICS) {
                goto join_posix;
            }

            /* \d doesn't have any matches in the upper Latin1 range, hence /d
             * is equivalent to /u.  Changing to /u saves some branches at
             * runtime */
            op = POSIXU;
            goto join_posix_op_known;

	case 'R':
	    ret = reg_node(pRExC_state, LNBREAK);
	    *flagp |= HASWIDTH|SIMPLE;
	    goto finish_meta_pat;

	case 'H':
            invert = 1;
            /* FALLTHROUGH */
	case 'h':
	    arg = ANYOF_BLANK;
            op = POSIXU;
            goto join_posix_op_known;

	case 'V':
            invert = 1;
            /* FALLTHROUGH */
	case 'v':
	    arg = ANYOF_VERTWS;
            op = POSIXU;
            goto join_posix_op_known;

	case 'S':
            invert = 1;
            /* FALLTHROUGH */
	case 's':
            arg = ANYOF_SPACE;

          join_posix:

	    op = POSIXD + get_regex_charset(RExC_flags);
            if (op > POSIXA) {  /* /aa is same as /a */
                op = POSIXA;
            }
            else if (op == POSIXL) {
                RExC_contains_locale = 1;
            }
            else if (op == POSIXD) {
                RExC_seen_d_op = TRUE;
            }

          join_posix_op_known:

            if (invert) {
                op += NPOSIXD - POSIXD;
            }

	    ret = reg_node(pRExC_state, op);
            FLAGS(REGNODE_p(ret)) = namedclass_to_classnum(arg);

	    *flagp |= HASWIDTH|SIMPLE;
            /* FALLTHROUGH */

          finish_meta_pat:
            if (   UCHARAT(RExC_parse + 1) == '{'
                && UNLIKELY(! new_regcurly(RExC_parse + 1, RExC_end)))
            {
                RExC_parse += 2;
                vFAIL("Unescaped left brace in regex is illegal here");
            }
	    nextchar(pRExC_state);
            Set_Node_Length(REGNODE_p(ret), 2); /* MJD */
	    break;
	case 'p':
	case 'P':
            RExC_parse--;

            ret = regclass(pRExC_state, flagp, depth+1,
                           TRUE, /* means just parse this element */
                           FALSE, /* don't allow multi-char folds */
                           FALSE, /* don't silence non-portable warnings.  It
                                     would be a bug if these returned
                                     non-portables */
                           (bool) RExC_strict,
                           TRUE, /* Allow an optimized regnode result */
                           NULL);
            RETURN_FAIL_ON_RESTART_FLAGP(flagp);
            /* regclass() can only return RESTART_PARSE and NEED_UTF8 if
             * multi-char folds are allowed.  */
            if (!ret)
                FAIL2("panic: regclass returned failure to regatom, flags=%#" UVxf,
                      (UV) *flagp);

            RExC_parse--;

            Set_Node_Offset(REGNODE_p(ret), parse_start);
            Set_Node_Cur_Length(REGNODE_p(ret), parse_start - 2);
            nextchar(pRExC_state);
	    break;
        case 'N':
            /* Handle \N, \N{} and \N{NAMED SEQUENCE} (the latter meaning the
             * \N{...} evaluates to a sequence of more than one code points).
             * The function call below returns a regnode, which is our result.
             * The parameters cause it to fail if the \N{} evaluates to a
             * single code point; we handle those like any other literal.  The
             * reason that the multicharacter case is handled here and not as
             * part of the EXACtish code is because of quantifiers.  In
             * /\N{BLAH}+/, the '+' applies to the whole thing, and doing it
             * this way makes that Just Happen. dmq.
             * join_exact() will join this up with adjacent EXACTish nodes
             * later on, if appropriate. */
            ++RExC_parse;
            if (grok_bslash_N(pRExC_state,
                              &ret,     /* Want a regnode returned */
                              NULL,     /* Fail if evaluates to a single code
                                           point */
                              NULL,     /* Don't need a count of how many code
                                           points */
                              flagp,
                              RExC_strict,
                              depth)
            ) {
                break;
            }

            RETURN_FAIL_ON_RESTART_FLAGP(flagp);

            /* Here, evaluates to a single code point.  Go get that */
            RExC_parse = parse_start;
            goto defchar;

	case 'k':    /* Handle \k<NAME> and \k'NAME' */
      parse_named_seq:
        {
            char ch;
            if (   RExC_parse >= RExC_end - 1
                || ((   ch = RExC_parse[1]) != '<'
                                      && ch != '\''
                                      && ch != '{'))
            {
	        RExC_parse++;
		/* diag_listed_as: Sequence \%s... not terminated in regex; marked by <-- HERE in m/%s/ */
	        vFAIL2("Sequence %.2s... not terminated", parse_start);
	    } else {
		RExC_parse += 2;
                ret = handle_named_backref(pRExC_state,
                                           flagp,
                                           parse_start,
                                           (ch == '<')
                                           ? '>'
                                           : (ch == '{')
                                             ? '}'
                                             : '\'');
            }
            break;
	}
	case 'g':
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    {
		I32 num;
		bool hasbrace = 0;

		if (*RExC_parse == 'g') {
                    bool isrel = 0;

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
                    }

                    if (RExC_parse >= RExC_end) {
                        goto unterminated_g;
                    }
                    num = S_backref_value(RExC_parse, RExC_end);
                    if (num == 0)
                        vFAIL("Reference to invalid group 0");
                    else if (num == I32_MAX) {
                         if (isDIGIT(*RExC_parse))
			    vFAIL("Reference to nonexistent group");
                        else
                          unterminated_g:
                            vFAIL("Unterminated \\g... pattern");
                    }

                    if (isrel) {
                        num = RExC_npar - num;
                        if (num < 1)
                            vFAIL("Reference to nonexistent or unclosed group");
                    }
                }
                else {
                    num = S_backref_value(RExC_parse, RExC_end);
                    /* bare \NNN might be backref or octal - if it is larger
                     * than or equal RExC_npar then it is assumed to be an
                     * octal escape. Note RExC_npar is +1 from the actual
                     * number of parens. */
                    /* Note we do NOT check if num == I32_MAX here, as that is
                     * handled by the RExC_npar check */

                    if (
                        /* any numeric escape < 10 is always a backref */
                        num > 9
                        /* any numeric escape < RExC_npar is a backref */
                        && num >= RExC_npar
                        /* cannot be an octal escape if it starts with 8 */
                        && *RExC_parse != '8'
                        /* cannot be an octal escape it it starts with 9 */
                        && *RExC_parse != '9'
                    ) {
                        /* Probably not meant to be a backref, instead likely
                         * to be an octal character escape, e.g. \35 or \777.
                         * The above logic should make it obvious why using
                         * octal escapes in patterns is problematic. - Yves */
                        RExC_parse = parse_start;
                        goto defchar;
                    }
                }

                /* At this point RExC_parse points at a numeric escape like
                 * \12 or \88 or something similar, which we should NOT treat
                 * as an octal escape. It may or may not be a valid backref
                 * escape. For instance \88888888 is unlikely to be a valid
                 * backref. */
                while (isDIGIT(*RExC_parse))
                    RExC_parse++;
                if (hasbrace) {
                    if (*RExC_parse != '}')
                        vFAIL("Unterminated \\g{...} pattern");
                    RExC_parse++;
                }
                if (num >= (I32)RExC_npar) {

                    /* It might be a forward reference; we can't fail until we
                     * know, by completing the parse to get all the groups, and
                     * then reparsing */
                    if (ALL_PARENS_COUNTED)  {
                        if (num >= RExC_total_parens)  {
                            vFAIL("Reference to nonexistent group");
                        }
                    }
                    else {
                        REQUIRE_PARENS_PASS;
                    }
                }
                RExC_sawback = 1;
                ret = reganode(pRExC_state,
                               ((! FOLD)
                                 ? REF
                                 : (ASCII_FOLD_RESTRICTED)
                                   ? REFFA
                                   : (AT_LEAST_UNI_SEMANTICS)
                                     ? REFFU
                                     : (LOC)
                                       ? REFFL
                                       : REFF),
                                num);
                if (OP(REGNODE_p(ret)) == REFF) {
                    RExC_seen_d_op = TRUE;
                }
                *flagp |= HASWIDTH;

                /* override incorrect value set in reganode MJD */
                Set_Node_Offset(REGNODE_p(ret), parse_start);
                Set_Node_Cur_Length(REGNODE_p(ret), parse_start-1);
                skip_to_be_ignored_text(pRExC_state, &RExC_parse,
                                        FALSE /* Don't force to /x */ );
	    }
	    break;
	case '\0':
	    if (RExC_parse >= RExC_end)
		FAIL("Trailing \\");
	    /* FALLTHROUGH */
	default:
	    /* Do not generate "unrecognized" warnings here, we fall
	       back into the quick-grab loop below */
            RExC_parse = parse_start;
	    goto defchar;
	} /* end of switch on a \foo sequence */
	break;

    case '#':

        /* '#' comments should have been spaced over before this function was
         * called */
        assert((RExC_flags & RXf_PMf_EXTENDED) == 0);
	/*
        if (RExC_flags & RXf_PMf_EXTENDED) {
	    RExC_parse = reg_skipcomment( pRExC_state, RExC_parse );
	    if (RExC_parse < RExC_end)
		goto tryagain;
	}
        */

	/* FALLTHROUGH */

    default:
	  defchar: {

            /* Here, we have determined that the next thing is probably a
             * literal character.  RExC_parse points to the first byte of its
             * definition.  (It still may be an escape sequence that evaluates
             * to a single character) */

	    STRLEN len = 0;
	    UV ender = 0;
	    char *p;
	    char *s;

/* This allows us to fill a node with just enough spare so that if the final
 * character folds, its expansion is guaranteed to fit */
#define MAX_NODE_STRING_SIZE (255-UTF8_MAXBYTES_CASE)

	    char *s0;
	    U8 upper_parse = MAX_NODE_STRING_SIZE;

            /* We start out as an EXACT node, even if under /i, until we find a
             * character which is in a fold.  The algorithm now segregates into
             * separate nodes, characters that fold from those that don't under
             * /i.  (This hopefully will create nodes that are fixed strings
             * even under /i, giving the optimizer something to grab on to.)
             * So, if a node has something in it and the next character is in
             * the opposite category, that node is closed up, and the function
             * returns.  Then regatom is called again, and a new node is
             * created for the new category. */
            U8 node_type = EXACT;

            /* Assume the node will be fully used; the excess is given back at
             * the end.  We can't make any other length assumptions, as a byte
             * input sequence could shrink down. */
            Ptrdiff_t initial_size = STR_SZ(256);

            bool next_is_quantifier;
            char * oldp = NULL;

            /* We can convert EXACTF nodes to EXACTFU if they contain only
             * characters that match identically regardless of the target
             * string's UTF8ness.  The reason to do this is that EXACTF is not
             * trie-able, EXACTFU is, and EXACTFU requires fewer operations at
             * runtime.
             *
             * Similarly, we can convert EXACTFL nodes to EXACTFLU8 if they
             * contain only above-Latin1 characters (hence must be in UTF8),
             * which don't participate in folds with Latin1-range characters,
             * as the latter's folds aren't known until runtime. */
            bool maybe_exactfu = FOLD && (DEPENDS_SEMANTICS || LOC);

            /* Single-character EXACTish nodes are almost always SIMPLE.  This
             * allows us to override this as encountered */
            U8 maybe_SIMPLE = SIMPLE;

            /* Does this node contain something that can't match unless the
             * target string is (also) in UTF-8 */
            bool requires_utf8_target = FALSE;

            /* The sequence 'ss' is problematic in non-UTF-8 patterns. */
            bool has_ss = FALSE;

            /* So is the MICRO SIGN */
            bool has_micro_sign = FALSE;

            /* Allocate an EXACT node.  The node_type may change below to
             * another EXACTish node, but since the size of the node doesn't
             * change, it works */
            ret = regnode_guts(pRExC_state, node_type, initial_size, "exact");
            FILL_NODE(ret, node_type);
            RExC_emit++;

	    s = STRING(REGNODE_p(ret));

            s0 = s;

	  reparse:

            /* This breaks under rare circumstances.  If folding, we do not
             * want to split a node at a character that is a non-final in a
             * multi-char fold, as an input string could just happen to want to
             * match across the node boundary.  The code at the end of the loop
             * looks for this, and backs off until it finds not such a
             * character, but it is possible (though extremely, extremely
             * unlikely) for all characters in the node to be non-final fold
             * ones, in which case we just leave the node fully filled, and
             * hope that it doesn't match the string in just the wrong place */

            assert( ! UTF     /* Is at the beginning of a character */
                   || UTF8_IS_INVARIANT(UCHARAT(RExC_parse))
                   || UTF8_IS_START(UCHARAT(RExC_parse)));

            /* Here, we have a literal character.  Find the maximal string of
             * them in the input that we can fit into a single EXACTish node.
             * We quit at the first non-literal or when the node gets full, or
             * under /i the categorization of folding/non-folding character
             * changes */
	    for (p = RExC_parse; len < upper_parse && p < RExC_end; ) {

                /* In most cases each iteration adds one byte to the output.
                 * The exceptions override this */
                Size_t added_len = 1;

		oldp = p;

                /* White space has already been ignored */
                assert(   (RExC_flags & RXf_PMf_EXTENDED) == 0
                       || ! is_PATWS_safe((p), RExC_end, UTF));

		switch ((U8)*p) {
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
		    case 'A':             /* Start assertion */
		    case 'b': case 'B':   /* Word-boundary assertion*/
		    case 'C':             /* Single char !DANGEROUS! */
		    case 'd': case 'D':   /* digit class */
		    case 'g': case 'G':   /* generic-backref, pos assertion */
		    case 'h': case 'H':   /* HORIZWS */
		    case 'k': case 'K':   /* named backref, keep marker */
		    case 'p': case 'P':   /* Unicode property */
		              case 'R':   /* LNBREAK */
		    case 's': case 'S':   /* space class */
		    case 'v': case 'V':   /* VERTWS */
		    case 'w': case 'W':   /* word class */
                    case 'X':             /* eXtended Unicode "combining
                                             character sequence" */
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
		    case 'N': /* Handle a single-code point named character. */
                        RExC_parse = p + 1;
                        if (! grok_bslash_N(pRExC_state,
                                            NULL,   /* Fail if evaluates to
                                                       anything other than a
                                                       single code point */
                                            &ender, /* The returned single code
                                                       point */
                                            NULL,   /* Don't need a count of
                                                       how many code points */
                                            flagp,
                                            RExC_strict,
                                            depth)
                        ) {
                            if (*flagp & NEED_UTF8)
                                FAIL("panic: grok_bslash_N set NEED_UTF8");
                            RETURN_FAIL_ON_RESTART_FLAGP(flagp);

                            /* Here, it wasn't a single code point.  Go close
                             * up this EXACTish node.  The switch() prior to
                             * this switch handles the other cases */
                            RExC_parse = p = oldp;
                            goto loopdone;
                        }
                        p = RExC_parse;
                        RExC_parse = parse_start;

                        /* The \N{} means the pattern, if previously /d,
                         * becomes /u.  That means it can't be an EXACTF node,
                         * but an EXACTFU */
                        if (node_type == EXACTF) {
                            node_type = EXACTFU;

                            /* If the node already contains something that
                             * differs between EXACTF and EXACTFU, reparse it
                             * as EXACTFU */
                            if (! maybe_exactfu) {
                                len = 0;
                                s = s0;
                                goto reparse;
                            }
                        }

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
			ender = ESC_NATIVE;
			p++;
			break;
		    case 'a':
			ender = '\a';
			p++;
			break;
		    case 'o':
			{
			    UV result;
			    const char* error_msg;

			    bool valid = grok_bslash_o(&p,
                                                       RExC_end,
						       &result,
						       &error_msg,
						       TO_OUTPUT_WARNINGS(p),
                                                       (bool) RExC_strict,
                                                       TRUE, /* Output warnings
                                                                for non-
                                                                portables */
                                                       UTF);
			    if (! valid) {
				RExC_parse = p;	/* going to die anyway; point
						   to exact spot of failure */
				vFAIL(error_msg);
			    }
                            UPDATE_WARNINGS_LOC(p - 1);
                            ender = result;
			    break;
			}
		    case 'x':
			{
                            UV result = UV_MAX; /* initialize to erroneous
                                                   value */
			    const char* error_msg;

			    bool valid = grok_bslash_x(&p,
                                                       RExC_end,
						       &result,
						       &error_msg,
                                                       TO_OUTPUT_WARNINGS(p),
                                                       (bool) RExC_strict,
                                                       TRUE, /* Silence warnings
                                                                for non-
                                                                portables */
                                                       UTF);
			    if (! valid) {
				RExC_parse = p;	/* going to die anyway; point
						   to exact spot of failure */
				vFAIL(error_msg);
			    }
                            UPDATE_WARNINGS_LOC(p - 1);
                            ender = result;

                            if (ender < 0x100) {
#ifdef EBCDIC
                                if (RExC_recode_x_to_native) {
                                    ender = LATIN1_TO_NATIVE(ender);
                                }
#endif
			    }
			    break;
			}
		    case 'c':
			p++;
			ender = grok_bslash_c(*p, TO_OUTPUT_WARNINGS(p));
                        UPDATE_WARNINGS_LOC(p);
                        p++;
			break;
                    case '8': case '9': /* must be a backreference */
                        --p;
                        /* we have an escape like \8 which cannot be an octal escape
                         * so we exit the loop, and let the outer loop handle this
                         * escape which may or may not be a legitimate backref. */
                        goto loopdone;
                    case '1': case '2': case '3':case '4':
		    case '5': case '6': case '7':
                        /* When we parse backslash escapes there is ambiguity
                         * between backreferences and octal escapes. Any escape
                         * from \1 - \9 is a backreference, any multi-digit
                         * escape which does not start with 0 and which when
                         * evaluated as decimal could refer to an already
                         * parsed capture buffer is a back reference. Anything
                         * else is octal.
                         *
                         * Note this implies that \118 could be interpreted as
                         * 118 OR as "\11" . "8" depending on whether there
                         * were 118 capture buffers defined already in the
                         * pattern.  */

                        /* NOTE, RExC_npar is 1 more than the actual number of
                         * parens we have seen so far, hence the "<" as opposed
                         * to "<=" */
                        if ( !isDIGIT(p[1]) || S_backref_value(p, RExC_end) < RExC_npar)
                        {  /* Not to be treated as an octal constant, go
                                   find backref */
                            --p;
                            goto loopdone;
                        }
                        /* FALLTHROUGH */
                    case '0':
			{
			    I32 flags = PERL_SCAN_SILENT_ILLDIGIT;
			    STRLEN numlen = 3;
			    ender = grok_oct(p, &numlen, &flags, NULL);
			    p += numlen;
                            if (   isDIGIT(*p)  /* like \08, \178 */
                                && ckWARN(WARN_REGEXP)
                                && numlen < 3)
                            {
				reg_warn_non_literal_string(
                                         p + 1,
                                         form_short_octal_warning(p, numlen));
                            }
			}
			break;
		    case '\0':
			if (p >= RExC_end)
			    FAIL("Trailing \\");
			/* FALLTHROUGH */
		    default:
			if (isALPHANUMERIC(*p)) {
                            /* An alpha followed by '{' is going to fail next
                             * iteration, so don't output this warning in that
                             * case */
                            if (! isALPHA(*p) || *(p + 1) != '{') {
                                ckWARN2reg(p + 1, "Unrecognized escape \\%.1s"
                                                  " passed through", p);
                            }
			}
			goto normal_default;
		    } /* End of switch on '\' */
		    break;
		case '{':
                    /* Trying to gain new uses for '{' without breaking too
                     * much existing code is hard.  The solution currently
                     * adopted is:
                     *  1)  If there is no ambiguity that a '{' should always
                     *      be taken literally, at the start of a construct, we
                     *      just do so.
                     *  2)  If the literal '{' conflicts with our desired use
                     *      of it as a metacharacter, we die.  The deprecation
                     *      cycles for this have come and gone.
                     *  3)  If there is ambiguity, we raise a simple warning.
                     *      This could happen, for example, if the user
                     *      intended it to introduce a quantifier, but slightly
                     *      misspelled the quantifier.  Without this warning,
                     *      the quantifier would silently be taken as a literal
                     *      string of characters instead of a meta construct */
		    if (len || (p > RExC_start && isALPHA_A(*(p - 1)))) {
                        if (      RExC_strict
                            || (  p > parse_start + 1
                                && isALPHA_A(*(p - 1))
                                && *(p - 2) == '\\')
                            || new_regcurly(p, RExC_end))
                        {
                            RExC_parse = p + 1;
                            vFAIL("Unescaped left brace in regex is "
                                  "illegal here");
                        }
                        ckWARNreg(p + 1, "Unescaped left brace in regex is"
                                         " passed through");
		    }
		    goto normal_default;
                case '}':
                case ']':
                    if (p > RExC_parse && RExC_strict) {
                        ckWARN2reg(p + 1, "Unescaped literal '%c'", *p);
                    }
		    /*FALLTHROUGH*/
		default:    /* A literal character */
		  normal_default:
		    if (! UTF8_IS_INVARIANT(*p) && UTF) {
			STRLEN numlen;
			ender = utf8n_to_uvchr((U8*)p, RExC_end - p,
					       &numlen, UTF8_ALLOW_DEFAULT);
			p += numlen;
		    }
		    else
			ender = (U8) *p++;
		    break;
		} /* End of switch on the literal */

		/* Here, have looked at the literal character, and <ender>
                 * contains its ordinal; <p> points to the character after it.
                 * */

                if (ender > 255) {
                    REQUIRE_UTF8(flagp);
                }

                /* We need to check if the next non-ignored thing is a
                 * quantifier.  Move <p> to after anything that should be
                 * ignored, which, as a side effect, positions <p> for the next
                 * loop iteration */
                skip_to_be_ignored_text(pRExC_state, &p,
                                        FALSE /* Don't force to /x */ );

                /* If the next thing is a quantifier, it applies to this
                 * character only, which means that this character has to be in
                 * its own node and can't just be appended to the string in an
                 * existing node, so if there are already other characters in
                 * the node, close the node with just them, and set up to do
                 * this character again next time through, when it will be the
                 * only thing in its new node */

                next_is_quantifier =    LIKELY(p < RExC_end)
                                     && UNLIKELY(ISMULT2(p));

                if (next_is_quantifier && LIKELY(len)) {
                    p = oldp;
                    goto loopdone;
                }

                /* Ready to add 'ender' to the node */

                if (! FOLD) {  /* The simple case, just append the literal */

                      not_fold_common:
                        if (UVCHR_IS_INVARIANT(ender) || ! UTF) {
                            *(s++) = (char) ender;
                        }
                        else {
                            U8 * new_s = uvchr_to_utf8((U8*)s, ender);
                            added_len = (char *) new_s - s;
                            s = (char *) new_s;

                            if (ender > 255)  {
                                requires_utf8_target = TRUE;
                            }
                        }
                }
                else if (LOC && is_PROBLEMATIC_LOCALE_FOLD_cp(ender)) {

                    /* Here are folding under /l, and the code point is
                     * problematic.  If this is the first character in the
                     * node, change the node type to folding.   Otherwise, if
                     * this is the first problematic character, close up the
                     * existing node, so can start a new node with this one */
                    if (! len) {
                        node_type = EXACTFL;
                        RExC_contains_locale = 1;
                    }
                    else if (node_type == EXACT) {
                        p = oldp;
                        goto loopdone;
                    }

                    /* This problematic code point means we can't simplify
                     * things */
                    maybe_exactfu = FALSE;

                    /* Here, we are adding a problematic fold character.
                     * "Problematic" in this context means that its fold isn't
                     * known until runtime.  (The non-problematic code points
                     * are the above-Latin1 ones that fold to also all
                     * above-Latin1.  Their folds don't vary no matter what the
                     * locale is.) But here we have characters whose fold
                     * depends on the locale.  We just add in the unfolded
                     * character, and wait until runtime to fold it */
                    goto not_fold_common;
                }
                else /* regular fold; see if actually is in a fold */
                     if (   (ender < 256 && ! IS_IN_SOME_FOLD_L1(ender))
                         || (ender > 255
                            && ! _invlist_contains_cp(PL_in_some_fold, ender)))
                {
                    /* Here, folding, but the character isn't in a fold.
                     *
                     * Start a new node if previous characters in the node were
                     * folded */
                    if (len && node_type != EXACT) {
                        p = oldp;
                        goto loopdone;
                    }

                    /* Here, continuing a node with non-folded characters.  Add
                     * this one */
                    goto not_fold_common;
                }
                else {  /* Here, does participate in some fold */

                    /* If this is the first character in the node, change its
                     * type to folding.  Otherwise, if this is the first
                     * folding character in the node, close up the existing
                     * node, so can start a new node with this one.  */
                    if (! len) {
                        node_type = compute_EXACTish(pRExC_state);
                    }
                    else if (node_type == EXACT) {
                        p = oldp;
                        goto loopdone;
                    }

                    if (UTF) {  /* Use the folded value */
                        if (UVCHR_IS_INVARIANT(ender)) {
                            *(s)++ = (U8) toFOLD(ender);
                        }
                        else {
                            ender = _to_uni_fold_flags(
                                    ender,
                                    (U8 *) s,
                                    &added_len,
                                    FOLD_FLAGS_FULL | ((ASCII_FOLD_RESTRICTED)
                                                    ? FOLD_FLAGS_NOMIX_ASCII
                                                    : 0));
                            s += added_len;

                            if (   ender > 255
                                && LIKELY(ender != GREEK_SMALL_LETTER_MU))
                            {
                                /* U+B5 folds to the MU, so its possible for a
                                 * non-UTF-8 target to match it */
                                requires_utf8_target = TRUE;
                            }
                        }
                    }
                    else {

                        /* Here is non-UTF8.  First, see if the character's
                         * fold differs between /d and /u. */
                        if (PL_fold[ender] != PL_fold_latin1[ender]) {
                            maybe_exactfu = FALSE;
                        }

#if    UNICODE_MAJOR_VERSION > 3 /* no multifolds in early Unicode */   \
   || (UNICODE_MAJOR_VERSION == 3 && (   UNICODE_DOT_VERSION > 0)       \
                                      || UNICODE_DOT_DOT_VERSION > 0)

                        /* On non-ancient Unicode versions, this includes the
                         * multi-char fold SHARP S to 'ss' */

                        if (   UNLIKELY(ender == LATIN_SMALL_LETTER_SHARP_S)
                                 || (   isALPHA_FOLD_EQ(ender, 's')
                                     && len > 0
                                     && isALPHA_FOLD_EQ(*(s-1), 's')))
                        {
                            /* Here, we have one of the following:
                             *  a)  a SHARP S.  This folds to 'ss' only under
                             *      /u rules.  If we are in that situation,
                             *      fold the SHARP S to 'ss'.  See the comments
                             *      for join_exact() as to why we fold this
                             *      non-UTF at compile time, and no others.
                             *  b)  'ss'.  When under /u, there's nothing
                             *      special needed to be done here.  The
                             *      previous iteration handled the first 's',
                             *      and this iteration will handle the second.
                             *      If, on the otherhand it's not /u, we have
                             *      to exclude the possibility of moving to /u,
                             *      so that we won't generate an unwanted
                             *      match, unless, at runtime, the target
                             *      string is in UTF-8.
                             * */

                            has_ss = TRUE;
                            maybe_exactfu = FALSE;  /* Can't generate an
                                                       EXACTFU node (unless we
                                                       already are in one) */
                            if (UNLIKELY(ender == LATIN_SMALL_LETTER_SHARP_S)) {
                                maybe_SIMPLE = 0;
                                if (node_type == EXACTFU) {
                                    *(s++) = 's';

                                    /* Let the code below add in the extra 's' */
                                    ender = 's';
                                    added_len = 2;
                                }
                            }
                        }
#endif

                        else if (UNLIKELY(ender == MICRO_SIGN)) {
                            has_micro_sign = TRUE;
                        }

                        *(s++) = (DEPENDS_SEMANTICS)
                                 ? (char) toFOLD(ender)

                                   /* Under /u, the fold of any character in
                                    * the 0-255 range happens to be its
                                    * lowercase equivalent, except for LATIN
                                    * SMALL LETTER SHARP S, which was handled
                                    * above, and the MICRO SIGN, whose fold
                                    * requires UTF-8 to represent.  */
                                 : (char) toLOWER_L1(ender);
                    }
		} /* End of adding current character to the node */

                len += added_len;

		if (next_is_quantifier) {

                    /* Here, the next input is a quantifier, and to get here,
                     * the current character is the only one in the node. */
                    goto loopdone;
		}

	    } /* End of loop through literal characters */

            /* Here we have either exhausted the input or ran out of room in
             * the node.  (If we encountered a character that can't be in the
             * node, transfer is made directly to <loopdone>, and so we
             * wouldn't have fallen off the end of the loop.)  In the latter
             * case, we artificially have to split the node into two, because
             * we just don't have enough space to hold everything.  This
             * creates a problem if the final character participates in a
             * multi-character fold in the non-final position, as a match that
             * should have occurred won't, due to the way nodes are matched,
             * and our artificial boundary.  So back off until we find a non-
             * problematic character -- one that isn't at the beginning or
             * middle of such a fold.  (Either it doesn't participate in any
             * folds, or appears only in the final position of all the folds it
             * does participate in.)  A better solution with far fewer false
             * positives, and that would fill the nodes more completely, would
             * be to actually have available all the multi-character folds to
             * test against, and to back-off only far enough to be sure that
             * this node isn't ending with a partial one.  <upper_parse> is set
             * further below (if we need to reparse the node) to include just
             * up through that final non-problematic character that this code
             * identifies, so when it is set to less than the full node, we can
             * skip the rest of this */
            if (FOLD && p < RExC_end && upper_parse == MAX_NODE_STRING_SIZE) {
                PERL_UINT_FAST8_T backup_count = 0;

                const STRLEN full_len = len;

		assert(len >= MAX_NODE_STRING_SIZE);

                /* Here, <s> points to just beyond where we have output the
                 * final character of the node.  Look backwards through the
                 * string until find a non- problematic character */

		if (! UTF) {

                    /* This has no multi-char folds to non-UTF characters */
                    if (ASCII_FOLD_RESTRICTED) {
                        goto loopdone;
                    }

                    while (--s >= s0 && IS_NON_FINAL_FOLD(*s)) {
                        backup_count++;
                    }
                    len = s - s0 + 1;
		}
                else {

                    /* Point to the first byte of the final character */
                    s = (char *) utf8_hop_back((U8 *) s, -1, (U8 *) s0);

                    while (s >= s0) {   /* Search backwards until find
                                           a non-problematic char */
                        if (UTF8_IS_INVARIANT(*s)) {

                            /* There are no ascii characters that participate
                             * in multi-char folds under /aa.  In EBCDIC, the
                             * non-ascii invariants are all control characters,
                             * so don't ever participate in any folds. */
                            if (ASCII_FOLD_RESTRICTED
                                || ! IS_NON_FINAL_FOLD(*s))
                            {
                                break;
                            }
                        }
                        else if (UTF8_IS_DOWNGRADEABLE_START(*s)) {
                            if (! IS_NON_FINAL_FOLD(EIGHT_BIT_UTF8_TO_NATIVE(
                                                                  *s, *(s+1))))
                            {
                                break;
                            }
                        }
                        else if (! _invlist_contains_cp(
                                        PL_NonFinalFold,
                                        valid_utf8_to_uvchr((U8 *) s, NULL)))
                        {
                            break;
                        }

                        /* Here, the current character is problematic in that
                         * it does occur in the non-final position of some
                         * fold, so try the character before it, but have to
                         * special case the very first byte in the string, so
                         * we don't read outside the string */
                        s = (s == s0) ? s -1 : (char *) utf8_hop((U8 *) s, -1);
                        backup_count++;
                    } /* End of loop backwards through the string */

                    /* If there were only problematic characters in the string,
                     * <s> will point to before s0, in which case the length
                     * should be 0, otherwise include the length of the
                     * non-problematic character just found */
                    len = (s < s0) ? 0 : s - s0 + UTF8SKIP(s);
		}

                /* Here, have found the final character, if any, that is
                 * non-problematic as far as ending the node without splitting
                 * it across a potential multi-char fold.  <len> contains the
                 * number of bytes in the node up-to and including that
                 * character, or is 0 if there is no such character, meaning
                 * the whole node contains only problematic characters.  In
                 * this case, give up and just take the node as-is.  We can't
                 * do any better */
                if (len == 0) {
                    len = full_len;

                } else {

                    /* Here, the node does contain some characters that aren't
                     * problematic.  If we didn't have to backup any, then the
                     * final character in the node is non-problematic, and we
                     * can take the node as-is */
                    if (backup_count == 0) {
                        goto loopdone;
                    }
                    else if (backup_count == 1) {

                        /* If the final character is problematic, but the
                         * penultimate is not, back-off that last character to
                         * later start a new node with it */
                        p = oldp;
                        goto loopdone;
                    }

                    /* Here, the final non-problematic character is earlier
                     * in the input than the penultimate character.  What we do
                     * is reparse from the beginning, going up only as far as
                     * this final ok one, thus guaranteeing that the node ends
                     * in an acceptable character.  The reason we reparse is
                     * that we know how far in the character is, but we don't
                     * know how to correlate its position with the input parse.
                     * An alternate implementation would be to build that
                     * correlation as we go along during the original parse,
                     * but that would entail extra work for every node, whereas
                     * this code gets executed only when the string is too
                     * large for the node, and the final two characters are
                     * problematic, an infrequent occurrence.  Yet another
                     * possible strategy would be to save the tail of the
                     * string, and the next time regatom is called, initialize
                     * with that.  The problem with this is that unless you
                     * back off one more character, you won't be guaranteed
                     * regatom will get called again, unless regbranch,
                     * regpiece ... are also changed.  If you do back off that
                     * extra character, so that there is input guaranteed to
                     * force calling regatom, you can't handle the case where
                     * just the first character in the node is acceptable.  I
                     * (khw) decided to try this method which doesn't have that
                     * pitfall; if performance issues are found, we can do a
                     * combination of the current approach plus that one */
                    upper_parse = len;
                    len = 0;
                    s = s0;
                    goto reparse;
                }
	    }   /* End of verifying node ends with an appropriate char */

          loopdone:   /* Jumped to when encounters something that shouldn't be
                         in the node */

            /* Free up any over-allocated space; cast is to silence bogus
             * warning in MS VC */
            change_engine_size(pRExC_state,
                                - (Ptrdiff_t) (initial_size - STR_SZ(len)));

            /* I (khw) don't know if you can get here with zero length, but the
             * old code handled this situation by creating a zero-length EXACT
             * node.  Might as well be NOTHING instead */
            if (len == 0) {
                OP(REGNODE_p(ret)) = NOTHING;
            }
            else {

                /* If the node type is EXACT here, check to see if it
                 * should be EXACTL, or EXACT_ONLY8. */
                if (node_type == EXACT) {
                    if (LOC) {
                        node_type = EXACTL;
                    }
                    else if (requires_utf8_target) {
                        node_type = EXACT_ONLY8;
                    }
                } else if (FOLD) {
                    if (    UNLIKELY(has_micro_sign || has_ss)
                        && (node_type == EXACTFU || (   node_type == EXACTF
                                                     && maybe_exactfu)))
                    {   /* These two conditions are problematic in non-UTF-8
                           EXACTFU nodes. */
                        assert(! UTF);
                        node_type = EXACTFUP;
                    }
                    else if (node_type == EXACTFL) {

                        /* 'maybe_exactfu' is deliberately set above to
                         * indicate this node type, where all code points in it
                         * are above 255 */
                        if (maybe_exactfu) {
                            node_type = EXACTFLU8;
                        }
                        else if (UNLIKELY(
                             _invlist_contains_cp(PL_HasMultiCharFold, ender)))
                        {
                            /* A character that folds to more than one will
                             * match multiple characters, so can't be SIMPLE.
                             * We don't have to worry about this with EXACTFLU8
                             * nodes just above, as they have already been
                             * folded (since the fold doesn't vary at run
                             * time).  Here, if the final character in the node
                             * folds to multiple, it can't be simple.  (This
                             * only has an effect if the node has only a single
                             * character, hence the final one, as elsewhere we
                             * turn off simple for nodes whose length > 1 */
                            maybe_SIMPLE = 0;
                        }
                    }
                    else if (node_type == EXACTF) {  /* Means is /di */

                        /* If 'maybe_exactfu' is clear, then we need to stay
                         * /di.  If it is set, it means there are no code
                         * points that match differently depending on UTF8ness
                         * of the target string, so it can become an EXACTFU
                         * node */
                        if (! maybe_exactfu) {
                            RExC_seen_d_op = TRUE;
                        }
                        else if (   isALPHA_FOLD_EQ(* STRING(REGNODE_p(ret)), 's')
                                 || isALPHA_FOLD_EQ(ender, 's'))
                        {
                            /* But, if the node begins or ends in an 's' we
                             * have to defer changing it into an EXACTFU, as
                             * the node could later get joined with another one
                             * that ends or begins with 's' creating an 'ss'
                             * sequence which would then wrongly match the
                             * sharp s without the target being UTF-8.  We
                             * create a special node that we resolve later when
                             * we join nodes together */

                            node_type = EXACTFU_S_EDGE;
                        }
                        else {
                            node_type = EXACTFU;
                        }
                    }

                    if (requires_utf8_target && node_type == EXACTFU) {
                        node_type = EXACTFU_ONLY8;
                    }
                }

                OP(REGNODE_p(ret)) = node_type;
                STR_LEN(REGNODE_p(ret)) = len;
                RExC_emit += STR_SZ(len);

                /* If the node isn't a single character, it can't be SIMPLE */
                if (len > (Size_t) ((UTF) ? UVCHR_SKIP(ender) : 1)) {
                    maybe_SIMPLE = 0;
                }

                *flagp |= HASWIDTH | maybe_SIMPLE;
            }

            Set_Node_Length(REGNODE_p(ret), p - parse_start - 1);
            RExC_parse = p;

	    {
		/* len is STRLEN which is unsigned, need to copy to signed */
		IV iv = len;
		if (iv < 0)
		    vFAIL("Internal disaster");
	    }

	} /* End of label 'defchar:' */
	break;
    } /* End of giant switch on input character */

    /* Position parse to next real character */
    skip_to_be_ignored_text(pRExC_state, &RExC_parse,
                                            FALSE /* Don't force to /x */ );
    if (   *RExC_parse == '{'
        && OP(REGNODE_p(ret)) != SBOL && ! regcurly(RExC_parse))
    {
        if (RExC_strict || new_regcurly(RExC_parse, RExC_end)) {
            RExC_parse++;
            vFAIL("Unescaped left brace in regex is illegal here");
        }
        ckWARNreg(RExC_parse + 1, "Unescaped left brace in regex is"
                                  " passed through");
    }

    return(ret);
}


STATIC void
S_populate_ANYOF_from_invlist(pTHX_ regnode *node, SV** invlist_ptr)
{
    /* Uses the inversion list '*invlist_ptr' to populate the ANYOF 'node'.  It
     * sets up the bitmap and any flags, removing those code points from the
     * inversion list, setting it to NULL should it become completely empty */

    dVAR;

    PERL_ARGS_ASSERT_POPULATE_ANYOF_FROM_INVLIST;
    assert(PL_regkind[OP(node)] == ANYOF);

    /* There is no bitmap for this node type */
    if (OP(node) == ANYOFH) {
        return;
    }

    ANYOF_BITMAP_ZERO(node);
    if (*invlist_ptr) {

	/* This gets set if we actually need to modify things */
	bool change_invlist = FALSE;

	UV start, end;

	/* Start looking through *invlist_ptr */
	invlist_iterinit(*invlist_ptr);
	while (invlist_iternext(*invlist_ptr, &start, &end)) {
	    UV high;
	    int i;

            if (end == UV_MAX && start <= NUM_ANYOF_CODE_POINTS) {
                ANYOF_FLAGS(node) |= ANYOF_MATCHES_ALL_ABOVE_BITMAP;
            }

	    /* Quit if are above what we should change */
	    if (start >= NUM_ANYOF_CODE_POINTS) {
		break;
	    }

	    change_invlist = TRUE;

	    /* Set all the bits in the range, up to the max that we are doing */
	    high = (end < NUM_ANYOF_CODE_POINTS - 1)
                   ? end
                   : NUM_ANYOF_CODE_POINTS - 1;
	    for (i = start; i <= (int) high; i++) {
		if (! ANYOF_BITMAP_TEST(node, i)) {
		    ANYOF_BITMAP_SET(node, i);
		}
	    }
	}
	invlist_iterfinish(*invlist_ptr);

        /* Done with loop; remove any code points that are in the bitmap from
         * *invlist_ptr; similarly for code points above the bitmap if we have
         * a flag to match all of them anyways */
	if (change_invlist) {
	    _invlist_subtract(*invlist_ptr, PL_InBitmap, invlist_ptr);
	}
        if (ANYOF_FLAGS(node) & ANYOF_MATCHES_ALL_ABOVE_BITMAP) {
	    _invlist_intersection(*invlist_ptr, PL_InBitmap, invlist_ptr);
	}

	/* If have completely emptied it, remove it completely */
	if (_invlist_len(*invlist_ptr) == 0) {
	    SvREFCNT_dec_NN(*invlist_ptr);
	    *invlist_ptr = NULL;
	}
    }
}

/* Parse POSIX character classes: [[:foo:]], [[=foo=]], [[.foo.]].
   Character classes ([:foo:]) can also be negated ([:^foo:]).
   Returns a named class id (ANYOF_XXX) if successful, -1 otherwise.
   Equivalence classes ([=foo=]) and composites ([.foo.]) are parsed,
   but trigger failures because they are currently unimplemented. */

#define POSIXCC_DONE(c)   ((c) == ':')
#define POSIXCC_NOTYET(c) ((c) == '=' || (c) == '.')
#define POSIXCC(c) (POSIXCC_DONE(c) || POSIXCC_NOTYET(c))
#define MAYBE_POSIXCC(c) (POSIXCC(c) || (c) == '^' || (c) == ';')

#define WARNING_PREFIX              "Assuming NOT a POSIX class since "
#define NO_BLANKS_POSIX_WARNING     "no blanks are allowed in one"
#define SEMI_COLON_POSIX_WARNING    "a semi-colon was found instead of a colon"

#define NOT_MEANT_TO_BE_A_POSIX_CLASS (OOB_NAMEDCLASS - 1)

/* 'posix_warnings' and 'warn_text' are names of variables in the following
 * routine. q.v. */
#define ADD_POSIX_WARNING(p, text)  STMT_START {                            \
        if (posix_warnings) {                                               \
            if (! RExC_warn_text ) RExC_warn_text =                         \
                                         (AV *) sv_2mortal((SV *) newAV()); \
            av_push(RExC_warn_text, Perl_newSVpvf(aTHX_                     \
                                             WARNING_PREFIX                 \
                                             text                           \
                                             REPORT_LOCATION,               \
                                             REPORT_LOCATION_ARGS(p)));     \
        }                                                                   \
    } STMT_END
#define CLEAR_POSIX_WARNINGS()                                              \
    STMT_START {                                                            \
        if (posix_warnings && RExC_warn_text)                               \
            av_clear(RExC_warn_text);                                       \
    } STMT_END

#define CLEAR_POSIX_WARNINGS_AND_RETURN(ret)                                \
    STMT_START {                                                            \
        CLEAR_POSIX_WARNINGS();                                             \
        return ret;                                                         \
    } STMT_END

STATIC int
S_handle_possible_posix(pTHX_ RExC_state_t *pRExC_state,

    const char * const s,      /* Where the putative posix class begins.
                                  Normally, this is one past the '['.  This
                                  parameter exists so it can be somewhere
                                  besides RExC_parse. */
    char ** updated_parse_ptr, /* Where to set the updated parse pointer, or
                                  NULL */
    AV ** posix_warnings,      /* Where to place any generated warnings, or
                                  NULL */
    const bool check_only      /* Don't die if error */
)
{
    /* This parses what the caller thinks may be one of the three POSIX
     * constructs:
     *  1) a character class, like [:blank:]
     *  2) a collating symbol, like [. .]
     *  3) an equivalence class, like [= =]
     * In the latter two cases, it croaks if it finds a syntactically legal
     * one, as these are not handled by Perl.
     *
     * The main purpose is to look for a POSIX character class.  It returns:
     *  a) the class number
     *      if it is a completely syntactically and semantically legal class.
     *      'updated_parse_ptr', if not NULL, is set to point to just after the
     *      closing ']' of the class
     *  b) OOB_NAMEDCLASS
     *      if it appears that one of the three POSIX constructs was meant, but
     *      its specification was somehow defective.  'updated_parse_ptr', if
     *      not NULL, is set to point to the character just after the end
     *      character of the class.  See below for handling of warnings.
     *  c) NOT_MEANT_TO_BE_A_POSIX_CLASS
     *      if it  doesn't appear that a POSIX construct was intended.
     *      'updated_parse_ptr' is not changed.  No warnings nor errors are
     *      raised.
     *
     * In b) there may be errors or warnings generated.  If 'check_only' is
     * TRUE, then any errors are discarded.  Warnings are returned to the
     * caller via an AV* created into '*posix_warnings' if it is not NULL.  If
     * instead it is NULL, warnings are suppressed.
     *
     * The reason for this function, and its complexity is that a bracketed
     * character class can contain just about anything.  But it's easy to
     * mistype the very specific posix class syntax but yielding a valid
     * regular bracketed class, so it silently gets compiled into something
     * quite unintended.
     *
     * The solution adopted here maintains backward compatibility except that
     * it adds a warning if it looks like a posix class was intended but
     * improperly specified.  The warning is not raised unless what is input
     * very closely resembles one of the 14 legal posix classes.  To do this,
     * it uses fuzzy parsing.  It calculates how many single-character edits it
     * would take to transform what was input into a legal posix class.  Only
     * if that number is quite small does it think that the intention was a
     * posix class.  Obviously these are heuristics, and there will be cases
     * where it errs on one side or another, and they can be tweaked as
     * experience informs.
     *
     * The syntax for a legal posix class is:
     *
     * qr/(?xa: \[ : \^? [[:lower:]]{4,6} : \] )/
     *
     * What this routine considers syntactically to be an intended posix class
     * is this (the comments indicate some restrictions that the pattern
     * doesn't show):
     *
     *  qr/(?x: \[?                         # The left bracket, possibly
     *                                      # omitted
     *          \h*                         # possibly followed by blanks
     *          (?: \^ \h* )?               # possibly a misplaced caret
     *          [:;]?                       # The opening class character,
     *                                      # possibly omitted.  A typo
     *                                      # semi-colon can also be used.
     *          \h*
     *          \^?                         # possibly a correctly placed
     *                                      # caret, but not if there was also
     *                                      # a misplaced one
     *          \h*
     *          .{3,15}                     # The class name.  If there are
     *                                      # deviations from the legal syntax,
     *                                      # its edit distance must be close
     *                                      # to a real class name in order
     *                                      # for it to be considered to be
     *                                      # an intended posix class.
     *          \h*
     *          [[:punct:]]?                # The closing class character,
     *                                      # possibly omitted.  If not a colon
     *                                      # nor semi colon, the class name
     *                                      # must be even closer to a valid
     *                                      # one
     *          \h*
     *          \]?                         # The right bracket, possibly
     *                                      # omitted.
     *     )/
     *
     * In the above, \h must be ASCII-only.
     *
     * These are heuristics, and can be tweaked as field experience dictates.
     * There will be cases when someone didn't intend to specify a posix class
     * that this warns as being so.  The goal is to minimize these, while
     * maximizing the catching of things intended to be a posix class that
     * aren't parsed as such.
     */

    const char* p             = s;
    const char * const e      = RExC_end;
    unsigned complement       = 0;      /* If to complement the class */
    bool found_problem        = FALSE;  /* Assume OK until proven otherwise */
    bool has_opening_bracket  = FALSE;
    bool has_opening_colon    = FALSE;
    int class_number          = OOB_NAMEDCLASS; /* Out-of-bounds until find
                                                   valid class */
    const char * possible_end = NULL;   /* used for a 2nd parse pass */
    const char* name_start;             /* ptr to class name first char */

    /* If the number of single-character typos the input name is away from a
     * legal name is no more than this number, it is considered to have meant
     * the legal name */
    int max_distance          = 2;

    /* to store the name.  The size determines the maximum length before we
     * decide that no posix class was intended.  Should be at least
     * sizeof("alphanumeric") */
    UV input_text[15];
    STATIC_ASSERT_DECL(C_ARRAY_LENGTH(input_text) >= sizeof "alphanumeric");

    PERL_ARGS_ASSERT_HANDLE_POSSIBLE_POSIX;

    CLEAR_POSIX_WARNINGS();

    if (p >= e) {
        return NOT_MEANT_TO_BE_A_POSIX_CLASS;
    }

    if (*(p - 1) != '[') {
        ADD_POSIX_WARNING(p, "it doesn't start with a '['");
        found_problem = TRUE;
    }
    else {
        has_opening_bracket = TRUE;
    }

    /* They could be confused and think you can put spaces between the
     * components */
    if (isBLANK(*p)) {
        found_problem = TRUE;

        do {
            p++;
        } while (p < e && isBLANK(*p));

        ADD_POSIX_WARNING(p, NO_BLANKS_POSIX_WARNING);
    }

    /* For [. .] and [= =].  These are quite different internally from [: :],
     * so they are handled separately.  */
    if (POSIXCC_NOTYET(*p) && p < e - 3) /* 1 for the close, and 1 for the ']'
                                            and 1 for at least one char in it
                                          */
    {
        const char open_char  = *p;
        const char * temp_ptr = p + 1;

        /* These two constructs are not handled by perl, and if we find a
         * syntactically valid one, we croak.  khw, who wrote this code, finds
         * this explanation of them very unclear:
         * http://pubs.opengroup.org/onlinepubs/009696899/basedefs/xbd_chap09.html
         * And searching the rest of the internet wasn't very helpful either.
         * It looks like just about any byte can be in these constructs,
         * depending on the locale.  But unless the pattern is being compiled
         * under /l, which is very rare, Perl runs under the C or POSIX locale.
         * In that case, it looks like [= =] isn't allowed at all, and that
         * [. .] could be any single code point, but for longer strings the
         * constituent characters would have to be the ASCII alphabetics plus
         * the minus-hyphen.  Any sensible locale definition would limit itself
         * to these.  And any portable one definitely should.  Trying to parse
         * the general case is a nightmare (see [perl #127604]).  So, this code
         * looks only for interiors of these constructs that match:
         *      qr/.|[-\w]{2,}/
         * Using \w relaxes the apparent rules a little, without adding much
         * danger of mistaking something else for one of these constructs.
         *
         * [. .] in some implementations described on the internet is usable to
         * escape a character that otherwise is special in bracketed character
         * classes.  For example [.].] means a literal right bracket instead of
         * the ending of the class
         *
         * [= =] can legitimately contain a [. .] construct, but we don't
         * handle this case, as that [. .] construct will later get parsed
         * itself and croak then.  And [= =] is checked for even when not under
         * /l, as Perl has long done so.
         *
         * The code below relies on there being a trailing NUL, so it doesn't
         * have to keep checking if the parse ptr < e.
         */
        if (temp_ptr[1] == open_char) {
            temp_ptr++;
        }
        else while (    temp_ptr < e
                    && (isWORDCHAR(*temp_ptr) || *temp_ptr == '-'))
        {
            temp_ptr++;
        }

        if (*temp_ptr == open_char) {
            temp_ptr++;
            if (*temp_ptr == ']') {
                temp_ptr++;
                if (! found_problem && ! check_only) {
                    RExC_parse = (char *) temp_ptr;
                    vFAIL3("POSIX syntax [%c %c] is reserved for future "
                            "extensions", open_char, open_char);
                }

                /* Here, the syntax wasn't completely valid, or else the call
                 * is to check-only */
                if (updated_parse_ptr) {
                    *updated_parse_ptr = (char *) temp_ptr;
                }

                CLEAR_POSIX_WARNINGS_AND_RETURN(OOB_NAMEDCLASS);
            }
        }

        /* If we find something that started out to look like one of these
         * constructs, but isn't, we continue below so that it can be checked
         * for being a class name with a typo of '.' or '=' instead of a colon.
         * */
    }

    /* Here, we think there is a possibility that a [: :] class was meant, and
     * we have the first real character.  It could be they think the '^' comes
     * first */
    if (*p == '^') {
        found_problem = TRUE;
        ADD_POSIX_WARNING(p + 1, "the '^' must come after the colon");
        complement = 1;
        p++;

        if (isBLANK(*p)) {
            found_problem = TRUE;

            do {
                p++;
            } while (p < e && isBLANK(*p));

            ADD_POSIX_WARNING(p, NO_BLANKS_POSIX_WARNING);
        }
    }

    /* But the first character should be a colon, which they could have easily
     * mistyped on a qwerty keyboard as a semi-colon (and which may be hard to
     * distinguish from a colon, so treat that as a colon).  */
    if (*p == ':') {
        p++;
        has_opening_colon = TRUE;
    }
    else if (*p == ';') {
        found_problem = TRUE;
        p++;
        ADD_POSIX_WARNING(p, SEMI_COLON_POSIX_WARNING);
        has_opening_colon = TRUE;
    }
    else {
        found_problem = TRUE;
        ADD_POSIX_WARNING(p, "there must be a starting ':'");

        /* Consider an initial punctuation (not one of the recognized ones) to
         * be a left terminator */
        if (*p != '^' && *p != ']' && isPUNCT(*p)) {
            p++;
        }
    }

    /* They may think that you can put spaces between the components */
    if (isBLANK(*p)) {
        found_problem = TRUE;

        do {
            p++;
        } while (p < e && isBLANK(*p));

        ADD_POSIX_WARNING(p, NO_BLANKS_POSIX_WARNING);
    }

    if (*p == '^') {

        /* We consider something like [^:^alnum:]] to not have been intended to
         * be a posix class, but XXX maybe we should */
        if (complement) {
            CLEAR_POSIX_WARNINGS_AND_RETURN(NOT_MEANT_TO_BE_A_POSIX_CLASS);
        }

        complement = 1;
        p++;
    }

    /* Again, they may think that you can put spaces between the components */
    if (isBLANK(*p)) {
        found_problem = TRUE;

        do {
            p++;
        } while (p < e && isBLANK(*p));

        ADD_POSIX_WARNING(p, NO_BLANKS_POSIX_WARNING);
    }

    if (*p == ']') {

        /* XXX This ']' may be a typo, and something else was meant.  But
         * treating it as such creates enough complications, that that
         * possibility isn't currently considered here.  So we assume that the
         * ']' is what is intended, and if we've already found an initial '[',
         * this leaves this construct looking like [:] or [:^], which almost
         * certainly weren't intended to be posix classes */
        if (has_opening_bracket) {
            CLEAR_POSIX_WARNINGS_AND_RETURN(NOT_MEANT_TO_BE_A_POSIX_CLASS);
        }

        /* But this function can be called when we parse the colon for
         * something like qr/[alpha:]]/, so we back up to look for the
         * beginning */
        p--;

        if (*p == ';') {
            found_problem = TRUE;
            ADD_POSIX_WARNING(p, SEMI_COLON_POSIX_WARNING);
        }
        else if (*p != ':') {

            /* XXX We are currently very restrictive here, so this code doesn't
             * consider the possibility that, say, /[alpha.]]/ was intended to
             * be a posix class. */
            CLEAR_POSIX_WARNINGS_AND_RETURN(NOT_MEANT_TO_BE_A_POSIX_CLASS);
        }

        /* Here we have something like 'foo:]'.  There was no initial colon,
         * and we back up over 'foo.  XXX Unlike the going forward case, we
         * don't handle typos of non-word chars in the middle */
        has_opening_colon = FALSE;
        p--;

        while (p > RExC_start && isWORDCHAR(*p)) {
            p--;
        }
        p++;

        /* Here, we have positioned ourselves to where we think the first
         * character in the potential class is */
    }

    /* Now the interior really starts.  There are certain key characters that
     * can end the interior, or these could just be typos.  To catch both
     * cases, we may have to do two passes.  In the first pass, we keep on
     * going unless we come to a sequence that matches
     *      qr/ [[:punct:]] [[:blank:]]* \] /xa
     * This means it takes a sequence to end the pass, so two typos in a row if
     * that wasn't what was intended.  If the class is perfectly formed, just
     * this one pass is needed.  We also stop if there are too many characters
     * being accumulated, but this number is deliberately set higher than any
     * real class.  It is set high enough so that someone who thinks that
     * 'alphanumeric' is a correct name would get warned that it wasn't.
     * While doing the pass, we keep track of where the key characters were in
     * it.  If we don't find an end to the class, and one of the key characters
     * was found, we redo the pass, but stop when we get to that character.
     * Thus the key character was considered a typo in the first pass, but a
     * terminator in the second.  If two key characters are found, we stop at
     * the second one in the first pass.  Again this can miss two typos, but
     * catches a single one
     *
     * In the first pass, 'possible_end' starts as NULL, and then gets set to
     * point to the first key character.  For the second pass, it starts as -1.
     * */

    name_start = p;
  parse_name:
    {
        bool has_blank               = FALSE;
        bool has_upper               = FALSE;
        bool has_terminating_colon   = FALSE;
        bool has_terminating_bracket = FALSE;
        bool has_semi_colon          = FALSE;
        unsigned int name_len        = 0;
        int punct_count              = 0;

        while (p < e) {

            /* Squeeze out blanks when looking up the class name below */
            if (isBLANK(*p) ) {
                has_blank = TRUE;
                found_problem = TRUE;
                p++;
                continue;
            }

            /* The name will end with a punctuation */
            if (isPUNCT(*p)) {
                const char * peek = p + 1;

                /* Treat any non-']' punctuation followed by a ']' (possibly
                 * with intervening blanks) as trying to terminate the class.
                 * ']]' is very likely to mean a class was intended (but
                 * missing the colon), but the warning message that gets
                 * generated shows the error position better if we exit the
                 * loop at the bottom (eventually), so skip it here. */
                if (*p != ']') {
                    if (peek < e && isBLANK(*peek)) {
                        has_blank = TRUE;
                        found_problem = TRUE;
                        do {
                            peek++;
                        } while (peek < e && isBLANK(*peek));
                    }

                    if (peek < e && *peek == ']') {
                        has_terminating_bracket = TRUE;
                        if (*p == ':') {
                            has_terminating_colon = TRUE;
                        }
                        else if (*p == ';') {
                            has_semi_colon = TRUE;
                            has_terminating_colon = TRUE;
                        }
                        else {
                            found_problem = TRUE;
                        }
                        p = peek + 1;
                        goto try_posix;
                    }
                }

                /* Here we have punctuation we thought didn't end the class.
                 * Keep track of the position of the key characters that are
                 * more likely to have been class-enders */
                if (*p == ']' || *p == '[' || *p == ':' || *p == ';') {

                    /* Allow just one such possible class-ender not actually
                     * ending the class. */
                    if (possible_end) {
                        break;
                    }
                    possible_end = p;
                }

                /* If we have too many punctuation characters, no use in
                 * keeping going */
                if (++punct_count > max_distance) {
                    break;
                }

                /* Treat the punctuation as a typo. */
                input_text[name_len++] = *p;
                p++;
            }
            else if (isUPPER(*p)) { /* Use lowercase for lookup */
                input_text[name_len++] = toLOWER(*p);
                has_upper = TRUE;
                found_problem = TRUE;
                p++;
            } else if (! UTF || UTF8_IS_INVARIANT(*p)) {
                input_text[name_len++] = *p;
                p++;
            }
            else {
                input_text[name_len++] = utf8_to_uvchr_buf((U8 *) p, e, NULL);
                p+= UTF8SKIP(p);
            }

            /* The declaration of 'input_text' is how long we allow a potential
             * class name to be, before saying they didn't mean a class name at
             * all */
            if (name_len >= C_ARRAY_LENGTH(input_text)) {
                break;
            }
        }

        /* We get to here when the possible class name hasn't been properly
         * terminated before:
         *   1) we ran off the end of the pattern; or
         *   2) found two characters, each of which might have been intended to
         *      be the name's terminator
         *   3) found so many punctuation characters in the purported name,
         *      that the edit distance to a valid one is exceeded
         *   4) we decided it was more characters than anyone could have
         *      intended to be one. */

        found_problem = TRUE;

        /* In the final two cases, we know that looking up what we've
         * accumulated won't lead to a match, even a fuzzy one. */
        if (   name_len >= C_ARRAY_LENGTH(input_text)
            || punct_count > max_distance)
        {
            /* If there was an intermediate key character that could have been
             * an intended end, redo the parse, but stop there */
            if (possible_end && possible_end != (char *) -1) {
                possible_end = (char *) -1; /* Special signal value to say
                                               we've done a first pass */
                p = name_start;
                goto parse_name;
            }

            /* Otherwise, it can't have meant to have been a class */
            CLEAR_POSIX_WARNINGS_AND_RETURN(NOT_MEANT_TO_BE_A_POSIX_CLASS);
        }

        /* If we ran off the end, and the final character was a punctuation
         * one, back up one, to look at that final one just below.  Later, we
         * will restore the parse pointer if appropriate */
        if (name_len && p == e && isPUNCT(*(p-1))) {
            p--;
            name_len--;
        }

        if (p < e && isPUNCT(*p)) {
            if (*p == ']') {
                has_terminating_bracket = TRUE;

                /* If this is a 2nd ']', and the first one is just below this
                 * one, consider that to be the real terminator.  This gives a
                 * uniform and better positioning for the warning message  */
                if (   possible_end
                    && possible_end != (char *) -1
                    && *possible_end == ']'
                    && name_len && input_text[name_len - 1] == ']')
                {
                    name_len--;
                    p = possible_end;

                    /* And this is actually equivalent to having done the 2nd
                     * pass now, so set it to not try again */
                    possible_end = (char *) -1;
                }
            }
            else {
                if (*p == ':') {
                    has_terminating_colon = TRUE;
                }
                else if (*p == ';') {
                    has_semi_colon = TRUE;
                    has_terminating_colon = TRUE;
                }
                p++;
            }
        }

    try_posix:

        /* Here, we have a class name to look up.  We can short circuit the
         * stuff below for short names that can't possibly be meant to be a
         * class name.  (We can do this on the first pass, as any second pass
         * will yield an even shorter name) */
        if (name_len < 3) {
            CLEAR_POSIX_WARNINGS_AND_RETURN(NOT_MEANT_TO_BE_A_POSIX_CLASS);
        }

        /* Find which class it is.  Initially switch on the length of the name.
         * */
        switch (name_len) {
            case 4:
                if (memEQs(name_start, 4, "word")) {
                    /* this is not POSIX, this is the Perl \w */
                    class_number = ANYOF_WORDCHAR;
                }
                break;
            case 5:
                /* Names all of length 5: alnum alpha ascii blank cntrl digit
                 *                        graph lower print punct space upper
                 * Offset 4 gives the best switch position.  */
                switch (name_start[4]) {
                    case 'a':
                        if (memBEGINs(name_start, 5, "alph")) /* alpha */
                            class_number = ANYOF_ALPHA;
                        break;
                    case 'e':
                        if (memBEGINs(name_start, 5, "spac")) /* space */
                            class_number = ANYOF_SPACE;
                        break;
                    case 'h':
                        if (memBEGINs(name_start, 5, "grap")) /* graph */
                            class_number = ANYOF_GRAPH;
                        break;
                    case 'i':
                        if (memBEGINs(name_start, 5, "asci")) /* ascii */
                            class_number = ANYOF_ASCII;
                        break;
                    case 'k':
                        if (memBEGINs(name_start, 5, "blan")) /* blank */
                            class_number = ANYOF_BLANK;
                        break;
                    case 'l':
                        if (memBEGINs(name_start, 5, "cntr")) /* cntrl */
                            class_number = ANYOF_CNTRL;
                        break;
                    case 'm':
                        if (memBEGINs(name_start, 5, "alnu")) /* alnum */
                            class_number = ANYOF_ALPHANUMERIC;
                        break;
                    case 'r':
                        if (memBEGINs(name_start, 5, "lowe")) /* lower */
                            class_number = (FOLD) ? ANYOF_CASED : ANYOF_LOWER;
                        else if (memBEGINs(name_start, 5, "uppe")) /* upper */
                            class_number = (FOLD) ? ANYOF_CASED : ANYOF_UPPER;
                        break;
                    case 't':
                        if (memBEGINs(name_start, 5, "digi")) /* digit */
                            class_number = ANYOF_DIGIT;
                        else if (memBEGINs(name_start, 5, "prin")) /* print */
                            class_number = ANYOF_PRINT;
                        else if (memBEGINs(name_start, 5, "punc")) /* punct */
                            class_number = ANYOF_PUNCT;
                        break;
                }
                break;
            case 6:
                if (memEQs(name_start, 6, "xdigit"))
                    class_number = ANYOF_XDIGIT;
                break;
        }

        /* If the name exactly matches a posix class name the class number will
         * here be set to it, and the input almost certainly was meant to be a
         * posix class, so we can skip further checking.  If instead the syntax
         * is exactly correct, but the name isn't one of the legal ones, we
         * will return that as an error below.  But if neither of these apply,
         * it could be that no posix class was intended at all, or that one
         * was, but there was a typo.  We tease these apart by doing fuzzy
         * matching on the name */
        if (class_number == OOB_NAMEDCLASS && found_problem) {
            const UV posix_names[][6] = {
                                                { 'a', 'l', 'n', 'u', 'm' },
                                                { 'a', 'l', 'p', 'h', 'a' },
                                                { 'a', 's', 'c', 'i', 'i' },
                                                { 'b', 'l', 'a', 'n', 'k' },
                                                { 'c', 'n', 't', 'r', 'l' },
                                                { 'd', 'i', 'g', 'i', 't' },
                                                { 'g', 'r', 'a', 'p', 'h' },
                                                { 'l', 'o', 'w', 'e', 'r' },
                                                { 'p', 'r', 'i', 'n', 't' },
                                                { 'p', 'u', 'n', 'c', 't' },
                                                { 's', 'p', 'a', 'c', 'e' },
                                                { 'u', 'p', 'p', 'e', 'r' },
                                                { 'w', 'o', 'r', 'd' },
                                                { 'x', 'd', 'i', 'g', 'i', 't' }
                                            };
            /* The names of the above all have added NULs to make them the same
             * size, so we need to also have the real lengths */
            const UV posix_name_lengths[] = {
                                                sizeof("alnum") - 1,
                                                sizeof("alpha") - 1,
                                                sizeof("ascii") - 1,
                                                sizeof("blank") - 1,
                                                sizeof("cntrl") - 1,
                                                sizeof("digit") - 1,
                                                sizeof("graph") - 1,
                                                sizeof("lower") - 1,
                                                sizeof("print") - 1,
                                                sizeof("punct") - 1,
                                                sizeof("space") - 1,
                                                sizeof("upper") - 1,
                                                sizeof("word")  - 1,
                                                sizeof("xdigit")- 1
                                            };
            unsigned int i;
            int temp_max = max_distance;    /* Use a temporary, so if we
                                               reparse, we haven't changed the
                                               outer one */

            /* Use a smaller max edit distance if we are missing one of the
             * delimiters */
            if (   has_opening_bracket + has_opening_colon < 2
                || has_terminating_bracket + has_terminating_colon < 2)
            {
                temp_max--;
            }

            /* See if the input name is close to a legal one */
            for (i = 0; i < C_ARRAY_LENGTH(posix_names); i++) {

                /* Short circuit call if the lengths are too far apart to be
                 * able to match */
                if (abs( (int) (name_len - posix_name_lengths[i]))
                    > temp_max)
                {
                    continue;
                }

                if (edit_distance(input_text,
                                  posix_names[i],
                                  name_len,
                                  posix_name_lengths[i],
                                  temp_max
                                 )
                    > -1)
                { /* If it is close, it probably was intended to be a class */
                    goto probably_meant_to_be;
                }
            }

            /* Here the input name is not close enough to a valid class name
             * for us to consider it to be intended to be a posix class.  If
             * we haven't already done so, and the parse found a character that
             * could have been terminators for the name, but which we absorbed
             * as typos during the first pass, repeat the parse, signalling it
             * to stop at that character */
            if (possible_end && possible_end != (char *) -1) {
                possible_end = (char *) -1;
                p = name_start;
                goto parse_name;
            }

            /* Here neither pass found a close-enough class name */
            CLEAR_POSIX_WARNINGS_AND_RETURN(NOT_MEANT_TO_BE_A_POSIX_CLASS);
        }

    probably_meant_to_be:

        /* Here we think that a posix specification was intended.  Update any
         * parse pointer */
        if (updated_parse_ptr) {
            *updated_parse_ptr = (char *) p;
        }

        /* If a posix class name was intended but incorrectly specified, we
         * output or return the warnings */
        if (found_problem) {

            /* We set flags for these issues in the parse loop above instead of
             * adding them to the list of warnings, because we can parse it
             * twice, and we only want one warning instance */
            if (has_upper) {
                ADD_POSIX_WARNING(p, "the name must be all lowercase letters");
            }
            if (has_blank) {
                ADD_POSIX_WARNING(p, NO_BLANKS_POSIX_WARNING);
            }
            if (has_semi_colon) {
                ADD_POSIX_WARNING(p, SEMI_COLON_POSIX_WARNING);
            }
            else if (! has_terminating_colon) {
                ADD_POSIX_WARNING(p, "there is no terminating ':'");
            }
            if (! has_terminating_bracket) {
                ADD_POSIX_WARNING(p, "there is no terminating ']'");
            }

            if (   posix_warnings
                && RExC_warn_text
                && av_top_index(RExC_warn_text) > -1)
            {
                *posix_warnings = RExC_warn_text;
            }
        }
        else if (class_number != OOB_NAMEDCLASS) {
            /* If it is a known class, return the class.  The class number
             * #defines are structured so each complement is +1 to the normal
             * one */
            CLEAR_POSIX_WARNINGS_AND_RETURN(class_number + complement);
        }
        else if (! check_only) {

            /* Here, it is an unrecognized class.  This is an error (unless the
            * call is to check only, which we've already handled above) */
            const char * const complement_string = (complement)
                                                   ? "^"
                                                   : "";
            RExC_parse = (char *) p;
            vFAIL3utf8f("POSIX class [:%s%" UTF8f ":] unknown",
                        complement_string,
                        UTF8fARG(UTF, RExC_parse - name_start - 2, name_start));
        }
    }

    return OOB_NAMEDCLASS;
}
#undef ADD_POSIX_WARNING

STATIC unsigned  int
S_regex_set_precedence(const U8 my_operator) {

    /* Returns the precedence in the (?[...]) construct of the input operator,
     * specified by its character representation.  The precedence follows
     * general Perl rules, but it extends this so that ')' and ']' have (low)
     * precedence even though they aren't really operators */

    switch (my_operator) {
        case '!':
            return 5;
        case '&':
            return 4;
        case '^':
        case '|':
        case '+':
        case '-':
            return 3;
        case ')':
            return 2;
        case ']':
            return 1;
    }

    NOT_REACHED; /* NOTREACHED */
    return 0;   /* Silence compiler warning */
}

STATIC regnode_offset
S_handle_regex_sets(pTHX_ RExC_state_t *pRExC_state, SV** return_invlist,
                    I32 *flagp, U32 depth,
                    char * const oregcomp_parse)
{
    /* Handle the (?[...]) construct to do set operations */

    U8 curchar;                     /* Current character being parsed */
    UV start, end;	            /* End points of code point ranges */
    SV* final = NULL;               /* The end result inversion list */
    SV* result_string;              /* 'final' stringified */
    AV* stack;                      /* stack of operators and operands not yet
                                       resolved */
    AV* fence_stack = NULL;         /* A stack containing the positions in
                                       'stack' of where the undealt-with left
                                       parens would be if they were actually
                                       put there */
    /* The 'volatile' is a workaround for an optimiser bug
     * in Solaris Studio 12.3. See RT #127455 */
    volatile IV fence = 0;          /* Position of where most recent undealt-
                                       with left paren in stack is; -1 if none.
                                     */
    STRLEN len;                     /* Temporary */
    regnode_offset node;                  /* Temporary, and final regnode returned by
                                       this function */
    const bool save_fold = FOLD;    /* Temporary */
    char *save_end, *save_parse;    /* Temporaries */
    const bool in_locale = LOC;     /* we turn off /l during processing */

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_HANDLE_REGEX_SETS;

    DEBUG_PARSE("xcls");

    if (in_locale) {
        set_regex_charset(&RExC_flags, REGEX_UNICODE_CHARSET);
    }

    /* The use of this operator implies /u.  This is required so that the
     * compile time values are valid in all runtime cases */
    REQUIRE_UNI_RULES(flagp, 0);

    ckWARNexperimental(RExC_parse,
                       WARN_EXPERIMENTAL__REGEX_SETS,
                       "The regex_sets feature is experimental");

    /* Everything in this construct is a metacharacter.  Operands begin with
     * either a '\' (for an escape sequence), or a '[' for a bracketed
     * character class.  Any other character should be an operator, or
     * parenthesis for grouping.  Both types of operands are handled by calling
     * regclass() to parse them.  It is called with a parameter to indicate to
     * return the computed inversion list.  The parsing here is implemented via
     * a stack.  Each entry on the stack is a single character representing one
     * of the operators; or else a pointer to an operand inversion list. */

#define IS_OPERATOR(a) SvIOK(a)
#define IS_OPERAND(a)  (! IS_OPERATOR(a))

    /* The stack is kept in Łukasiewicz order.  (That's pronounced similar
     * to luke-a-shave-itch (or -itz), but people who didn't want to bother
     * with pronouncing it called it Reverse Polish instead, but now that YOU
     * know how to pronounce it you can use the correct term, thus giving due
     * credit to the person who invented it, and impressing your geek friends.
     * Wikipedia says that the pronounciation of "Ł" has been changing so that
     * it is now more like an English initial W (as in wonk) than an L.)
     *
     * This means that, for example, 'a | b & c' is stored on the stack as
     *
     * c  [4]
     * b  [3]
     * &  [2]
     * a  [1]
     * |  [0]
     *
     * where the numbers in brackets give the stack [array] element number.
     * In this implementation, parentheses are not stored on the stack.
     * Instead a '(' creates a "fence" so that the part of the stack below the
     * fence is invisible except to the corresponding ')' (this allows us to
     * replace testing for parens, by using instead subtraction of the fence
     * position).  As new operands are processed they are pushed onto the stack
     * (except as noted in the next paragraph).  New operators of higher
     * precedence than the current final one are inserted on the stack before
     * the lhs operand (so that when the rhs is pushed next, everything will be
     * in the correct positions shown above.  When an operator of equal or
     * lower precedence is encountered in parsing, all the stacked operations
     * of equal or higher precedence are evaluated, leaving the result as the
     * top entry on the stack.  This makes higher precedence operations
     * evaluate before lower precedence ones, and causes operations of equal
     * precedence to left associate.
     *
     * The only unary operator '!' is immediately pushed onto the stack when
     * encountered.  When an operand is encountered, if the top of the stack is
     * a '!", the complement is immediately performed, and the '!' popped.  The
     * resulting value is treated as a new operand, and the logic in the
     * previous paragraph is executed.  Thus in the expression
     *      [a] + ! [b]
     * the stack looks like
     *
     * !
     * a
     * +
     *
     * as 'b' gets parsed, the latter gets evaluated to '!b', and the stack
     * becomes
     *
     * !b
     * a
     * +
     *
     * A ')' is treated as an operator with lower precedence than all the
     * aforementioned ones, which causes all operations on the stack above the
     * corresponding '(' to be evaluated down to a single resultant operand.
     * Then the fence for the '(' is removed, and the operand goes through the
     * algorithm above, without the fence.
     *
     * A separate stack is kept of the fence positions, so that the position of
     * the latest so-far unbalanced '(' is at the top of it.
     *
     * The ']' ending the construct is treated as the lowest operator of all,
     * so that everything gets evaluated down to a single operand, which is the
     * result */

    sv_2mortal((SV *)(stack = newAV()));
    sv_2mortal((SV *)(fence_stack = newAV()));

    while (RExC_parse < RExC_end) {
        I32 top_index;              /* Index of top-most element in 'stack' */
        SV** top_ptr;               /* Pointer to top 'stack' element */
        SV* current = NULL;         /* To contain the current inversion list
                                       operand */
        SV* only_to_avoid_leaks;

        skip_to_be_ignored_text(pRExC_state, &RExC_parse,
                                TRUE /* Force /x */ );
        if (RExC_parse >= RExC_end) {   /* Fail */
            break;
        }

        curchar = UCHARAT(RExC_parse);

redo_curchar:

#ifdef ENABLE_REGEX_SETS_DEBUGGING
                    /* Enable with -Accflags=-DENABLE_REGEX_SETS_DEBUGGING */
        DEBUG_U(dump_regex_sets_structures(pRExC_state,
                                           stack, fence, fence_stack));
#endif

        top_index = av_tindex_skip_len_mg(stack);

        switch (curchar) {
            SV** stacked_ptr;       /* Ptr to something already on 'stack' */
            char stacked_operator;  /* The topmost operator on the 'stack'. */
            SV* lhs;                /* Operand to the left of the operator */
            SV* rhs;                /* Operand to the right of the operator */
            SV* fence_ptr;          /* Pointer to top element of the fence
                                       stack */

            case '(':

                if (   RExC_parse < RExC_end - 2
                    && UCHARAT(RExC_parse + 1) == '?'
                    && UCHARAT(RExC_parse + 2) == '^')
                {
                    /* If is a '(?', could be an embedded '(?^flags:(?[...])'.
                     * This happens when we have some thing like
                     *
                     *   my $thai_or_lao = qr/(?[ \p{Thai} + \p{Lao} ])/;
                     *   ...
                     *   qr/(?[ \p{Digit} & $thai_or_lao ])/;
                     *
                     * Here we would be handling the interpolated
                     * '$thai_or_lao'.  We handle this by a recursive call to
                     * ourselves which returns the inversion list the
                     * interpolated expression evaluates to.  We use the flags
                     * from the interpolated pattern. */
                    U32 save_flags = RExC_flags;
                    const char * save_parse;

                    RExC_parse += 2;        /* Skip past the '(?' */
                    save_parse = RExC_parse;

                    /* Parse the flags for the '(?'.  We already know the first
                     * flag to parse is a '^' */
                    parse_lparen_question_flags(pRExC_state);

                    if (   RExC_parse >= RExC_end - 4
                        || UCHARAT(RExC_parse) != ':'
                        || UCHARAT(++RExC_parse) != '('
                        || UCHARAT(++RExC_parse) != '?'
                        || UCHARAT(++RExC_parse) != '[')
                    {

                        /* In combination with the above, this moves the
                         * pointer to the point just after the first erroneous
                         * character. */
                        if (RExC_parse >= RExC_end - 4) {
                            RExC_parse = RExC_end;
                        }
                        else if (RExC_parse != save_parse) {
                            RExC_parse += (UTF)
                                          ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                                          : 1;
                        }
                        vFAIL("Expecting '(?flags:(?[...'");
                    }

                    /* Recurse, with the meat of the embedded expression */
                    RExC_parse++;
                    if (! handle_regex_sets(pRExC_state, &current, flagp,
                                                    depth+1, oregcomp_parse))
                    {
                        RETURN_FAIL_ON_RESTART(*flagp, flagp);
                    }

                    /* Here, 'current' contains the embedded expression's
                     * inversion list, and RExC_parse points to the trailing
                     * ']'; the next character should be the ')' */
                    RExC_parse++;
                    if (UCHARAT(RExC_parse) != ')')
                        vFAIL("Expecting close paren for nested extended charclass");

                    /* Then the ')' matching the original '(' handled by this
                     * case: statement */
                    RExC_parse++;
                    if (UCHARAT(RExC_parse) != ')')
                        vFAIL("Expecting close paren for wrapper for nested extended charclass");

                    RExC_flags = save_flags;
                    goto handle_operand;
                }

                /* A regular '('.  Look behind for illegal syntax */
                if (top_index - fence >= 0) {
                    /* If the top entry on the stack is an operator, it had
                     * better be a '!', otherwise the entry below the top
                     * operand should be an operator */
                    if (   ! (top_ptr = av_fetch(stack, top_index, FALSE))
                        || (IS_OPERATOR(*top_ptr) && SvUV(*top_ptr) != '!')
                        || (   IS_OPERAND(*top_ptr)
                            && (   top_index - fence < 1
                                || ! (stacked_ptr = av_fetch(stack,
                                                             top_index - 1,
                                                             FALSE))
                                || ! IS_OPERATOR(*stacked_ptr))))
                    {
                        RExC_parse++;
                        vFAIL("Unexpected '(' with no preceding operator");
                    }
                }

                /* Stack the position of this undealt-with left paren */
                av_push(fence_stack, newSViv(fence));
                fence = top_index + 1;
                break;

            case '\\':
                /* regclass() can only return RESTART_PARSE and NEED_UTF8 if
                 * multi-char folds are allowed.  */
                if (!regclass(pRExC_state, flagp, depth+1,
                              TRUE, /* means parse just the next thing */
                              FALSE, /* don't allow multi-char folds */
                              FALSE, /* don't silence non-portable warnings.  */
                              TRUE,  /* strict */
                              FALSE, /* Require return to be an ANYOF */
                              &current))
                {
                    RETURN_FAIL_ON_RESTART(*flagp, flagp);
                    goto regclass_failed;
                }

                /* regclass() will return with parsing just the \ sequence,
                 * leaving the parse pointer at the next thing to parse */
                RExC_parse--;
                goto handle_operand;

            case '[':   /* Is a bracketed character class */
            {
                /* See if this is a [:posix:] class. */
                bool is_posix_class = (OOB_NAMEDCLASS
                            < handle_possible_posix(pRExC_state,
                                                RExC_parse + 1,
                                                NULL,
                                                NULL,
                                                TRUE /* checking only */));
                /* If it is a posix class, leave the parse pointer at the '['
                 * to fool regclass() into thinking it is part of a
                 * '[[:posix:]]'. */
                if (! is_posix_class) {
                    RExC_parse++;
                }

                /* regclass() can only return RESTART_PARSE and NEED_UTF8 if
                 * multi-char folds are allowed.  */
                if (!regclass(pRExC_state, flagp, depth+1,
                                is_posix_class, /* parse the whole char
                                                    class only if not a
                                                    posix class */
                                FALSE, /* don't allow multi-char folds */
                                TRUE, /* silence non-portable warnings. */
                                TRUE, /* strict */
                                FALSE, /* Require return to be an ANYOF */
                                &current))
                {
                    RETURN_FAIL_ON_RESTART(*flagp, flagp);
                    goto regclass_failed;
                }

                if (! current) {
                    break;
                }

                /* function call leaves parse pointing to the ']', except if we
                 * faked it */
                if (is_posix_class) {
                    RExC_parse--;
                }

                goto handle_operand;
            }

            case ']':
                if (top_index >= 1) {
                    goto join_operators;
                }

                /* Only a single operand on the stack: are done */
                goto done;

            case ')':
                if (av_tindex_skip_len_mg(fence_stack) < 0) {
                    if (UCHARAT(RExC_parse - 1) == ']')  {
                        break;
                    }
                    RExC_parse++;
                    vFAIL("Unexpected ')'");
                }

                /* If nothing after the fence, is missing an operand */
                if (top_index - fence < 0) {
                    RExC_parse++;
                    goto bad_syntax;
                }
                /* If at least two things on the stack, treat this as an
                  * operator */
                if (top_index - fence >= 1) {
                    goto join_operators;
                }

                /* Here only a single thing on the fenced stack, and there is a
                 * fence.  Get rid of it */
                fence_ptr = av_pop(fence_stack);
                assert(fence_ptr);
                fence = SvIV(fence_ptr);
                SvREFCNT_dec_NN(fence_ptr);
                fence_ptr = NULL;

                if (fence < 0) {
                    fence = 0;
                }

                /* Having gotten rid of the fence, we pop the operand at the
                 * stack top and process it as a newly encountered operand */
                current = av_pop(stack);
                if (IS_OPERAND(current)) {
                    goto handle_operand;
                }

                RExC_parse++;
                goto bad_syntax;

            case '&':
            case '|':
            case '+':
            case '-':
            case '^':

                /* These binary operators should have a left operand already
                 * parsed */
                if (   top_index - fence < 0
                    || top_index - fence == 1
                    || ( ! (top_ptr = av_fetch(stack, top_index, FALSE)))
                    || ! IS_OPERAND(*top_ptr))
                {
                    goto unexpected_binary;
                }

                /* If only the one operand is on the part of the stack visible
                 * to us, we just place this operator in the proper position */
                if (top_index - fence < 2) {

                    /* Place the operator before the operand */

                    SV* lhs = av_pop(stack);
                    av_push(stack, newSVuv(curchar));
                    av_push(stack, lhs);
                    break;
                }

                /* But if there is something else on the stack, we need to
                 * process it before this new operator if and only if the
                 * stacked operation has equal or higher precedence than the
                 * new one */

             join_operators:

                /* The operator on the stack is supposed to be below both its
                 * operands */
                if (   ! (stacked_ptr = av_fetch(stack, top_index - 2, FALSE))
                    || IS_OPERAND(*stacked_ptr))
                {
                    /* But if not, it's legal and indicates we are completely
                     * done if and only if we're currently processing a ']',
                     * which should be the final thing in the expression */
                    if (curchar == ']') {
                        goto done;
                    }

                  unexpected_binary:
                    RExC_parse++;
                    vFAIL2("Unexpected binary operator '%c' with no "
                           "preceding operand", curchar);
                }
                stacked_operator = (char) SvUV(*stacked_ptr);

                if (regex_set_precedence(curchar)
                    > regex_set_precedence(stacked_operator))
                {
                    /* Here, the new operator has higher precedence than the
                     * stacked one.  This means we need to add the new one to
                     * the stack to await its rhs operand (and maybe more
                     * stuff).  We put it before the lhs operand, leaving
                     * untouched the stacked operator and everything below it
                     * */
                    lhs = av_pop(stack);
                    assert(IS_OPERAND(lhs));

                    av_push(stack, newSVuv(curchar));
                    av_push(stack, lhs);
                    break;
                }

                /* Here, the new operator has equal or lower precedence than
                 * what's already there.  This means the operation already
                 * there should be performed now, before the new one. */

                rhs = av_pop(stack);
                if (! IS_OPERAND(rhs)) {

                    /* This can happen when a ! is not followed by an operand,
                     * like in /(?[\t &!])/ */
                    goto bad_syntax;
                }

                lhs = av_pop(stack);

                if (! IS_OPERAND(lhs)) {

                    /* This can happen when there is an empty (), like in
                     * /(?[[0]+()+])/ */
                    goto bad_syntax;
                }

                switch (stacked_operator) {
                    case '&':
                        _invlist_intersection(lhs, rhs, &rhs);
                        break;

                    case '|':
                    case '+':
                        _invlist_union(lhs, rhs, &rhs);
                        break;

                    case '-':
                        _invlist_subtract(lhs, rhs, &rhs);
                        break;

                    case '^':   /* The union minus the intersection */
                    {
                        SV* i = NULL;
                        SV* u = NULL;

                        _invlist_union(lhs, rhs, &u);
                        _invlist_intersection(lhs, rhs, &i);
                        _invlist_subtract(u, i, &rhs);
                        SvREFCNT_dec_NN(i);
                        SvREFCNT_dec_NN(u);
                        break;
                    }
                }
                SvREFCNT_dec(lhs);

                /* Here, the higher precedence operation has been done, and the
                 * result is in 'rhs'.  We overwrite the stacked operator with
                 * the result.  Then we redo this code to either push the new
                 * operator onto the stack or perform any higher precedence
                 * stacked operation */
                only_to_avoid_leaks = av_pop(stack);
                SvREFCNT_dec(only_to_avoid_leaks);
                av_push(stack, rhs);
                goto redo_curchar;

            case '!':   /* Highest priority, right associative */

                /* If what's already at the top of the stack is another '!",
                 * they just cancel each other out */
                if (   (top_ptr = av_fetch(stack, top_index, FALSE))
                    && (IS_OPERATOR(*top_ptr) && SvUV(*top_ptr) == '!'))
                {
                    only_to_avoid_leaks = av_pop(stack);
                    SvREFCNT_dec(only_to_avoid_leaks);
                }
                else { /* Otherwise, since it's right associative, just push
                          onto the stack */
                    av_push(stack, newSVuv(curchar));
                }
                break;

            default:
                RExC_parse += (UTF) ? UTF8SKIP(RExC_parse) : 1;
                if (RExC_parse >= RExC_end) {
                    break;
                }
                vFAIL("Unexpected character");

          handle_operand:

            /* Here 'current' is the operand.  If something is already on the
             * stack, we have to check if it is a !.  But first, the code above
             * may have altered the stack in the time since we earlier set
             * 'top_index'.  */

            top_index = av_tindex_skip_len_mg(stack);
            if (top_index - fence >= 0) {
                /* If the top entry on the stack is an operator, it had better
                 * be a '!', otherwise the entry below the top operand should
                 * be an operator */
                top_ptr = av_fetch(stack, top_index, FALSE);
                assert(top_ptr);
                if (IS_OPERATOR(*top_ptr)) {

                    /* The only permissible operator at the top of the stack is
                     * '!', which is applied immediately to this operand. */
                    curchar = (char) SvUV(*top_ptr);
                    if (curchar != '!') {
                        SvREFCNT_dec(current);
                        vFAIL2("Unexpected binary operator '%c' with no "
                                "preceding operand", curchar);
                    }

                    _invlist_invert(current);

                    only_to_avoid_leaks = av_pop(stack);
                    SvREFCNT_dec(only_to_avoid_leaks);

                    /* And we redo with the inverted operand.  This allows
                     * handling multiple ! in a row */
                    goto handle_operand;
                }
                          /* Single operand is ok only for the non-binary ')'
                           * operator */
                else if ((top_index - fence == 0 && curchar != ')')
                         || (top_index - fence > 0
                             && (! (stacked_ptr = av_fetch(stack,
                                                           top_index - 1,
                                                           FALSE))
                                 || IS_OPERAND(*stacked_ptr))))
                {
                    SvREFCNT_dec(current);
                    vFAIL("Operand with no preceding operator");
                }
            }

            /* Here there was nothing on the stack or the top element was
             * another operand.  Just add this new one */
            av_push(stack, current);

        } /* End of switch on next parse token */

        RExC_parse += (UTF) ? UTF8SKIP(RExC_parse) : 1;
    } /* End of loop parsing through the construct */

    vFAIL("Syntax error in (?[...])");

  done:

    if (RExC_parse >= RExC_end || RExC_parse[1] != ')') {
        if (RExC_parse < RExC_end) {
            RExC_parse++;
        }

        vFAIL("Unexpected ']' with no following ')' in (?[...");
    }

    if (av_tindex_skip_len_mg(fence_stack) >= 0) {
        vFAIL("Unmatched (");
    }

    if (av_tindex_skip_len_mg(stack) < 0   /* Was empty */
        || ((final = av_pop(stack)) == NULL)
        || ! IS_OPERAND(final)
        || ! is_invlist(final)
        || av_tindex_skip_len_mg(stack) >= 0)  /* More left on stack */
    {
      bad_syntax:
        SvREFCNT_dec(final);
        vFAIL("Incomplete expression within '(?[ ])'");
    }

    /* Here, 'final' is the resultant inversion list from evaluating the
     * expression.  Return it if so requested */
    if (return_invlist) {
        *return_invlist = final;
        return END;
    }

    /* Otherwise generate a resultant node, based on 'final'.  regclass() is
     * expecting a string of ranges and individual code points */
    invlist_iterinit(final);
    result_string = newSVpvs("");
    while (invlist_iternext(final, &start, &end)) {
        if (start == end) {
            Perl_sv_catpvf(aTHX_ result_string, "\\x{%" UVXf "}", start);
        }
        else {
            Perl_sv_catpvf(aTHX_ result_string, "\\x{%" UVXf "}-\\x{%" UVXf "}",
                                                     start,          end);
        }
    }

    /* About to generate an ANYOF (or similar) node from the inversion list we
     * have calculated */
    save_parse = RExC_parse;
    RExC_parse = SvPV(result_string, len);
    save_end = RExC_end;
    RExC_end = RExC_parse + len;
    TURN_OFF_WARNINGS_IN_SUBSTITUTE_PARSE;

    /* We turn off folding around the call, as the class we have constructed
     * already has all folding taken into consideration, and we don't want
     * regclass() to add to that */
    RExC_flags &= ~RXf_PMf_FOLD;
    /* regclass() can only return RESTART_PARSE and NEED_UTF8 if multi-char
     * folds are allowed.  */
    node = regclass(pRExC_state, flagp, depth+1,
                    FALSE, /* means parse the whole char class */
                    FALSE, /* don't allow multi-char folds */
                    TRUE, /* silence non-portable warnings.  The above may very
                             well have generated non-portable code points, but
                             they're valid on this machine */
                    FALSE, /* similarly, no need for strict */
                    FALSE, /* Require return to be an ANYOF */
                    NULL
                );

    RESTORE_WARNINGS;
    RExC_parse = save_parse + 1;
    RExC_end = save_end;
    SvREFCNT_dec_NN(final);
    SvREFCNT_dec_NN(result_string);

    if (save_fold) {
        RExC_flags |= RXf_PMf_FOLD;
    }

    if (!node) {
        RETURN_FAIL_ON_RESTART(*flagp, flagp);
        goto regclass_failed;
    }

    /* Fix up the node type if we are in locale.  (We have pretended we are
     * under /u for the purposes of regclass(), as this construct will only
     * work under UTF-8 locales.  But now we change the opcode to be ANYOFL (so
     * as to cause any warnings about bad locales to be output in regexec.c),
     * and add the flag that indicates to check if not in a UTF-8 locale.  The
     * reason we above forbid optimization into something other than an ANYOF
     * node is simply to minimize the number of code changes in regexec.c.
     * Otherwise we would have to create new EXACTish node types and deal with
     * them.  This decision could be revisited should this construct become
     * popular.
     *
     * (One might think we could look at the resulting ANYOF node and suppress
     * the flag if everything is above 255, as those would be UTF-8 only,
     * but this isn't true, as the components that led to that result could
     * have been locale-affected, and just happen to cancel each other out
     * under UTF-8 locales.) */
    if (in_locale) {
        set_regex_charset(&RExC_flags, REGEX_LOCALE_CHARSET);

        assert(OP(REGNODE_p(node)) == ANYOF);

        OP(REGNODE_p(node)) = ANYOFL;
        ANYOF_FLAGS(REGNODE_p(node))
                |= ANYOFL_SHARED_UTF8_LOCALE_fold_HAS_MATCHES_nonfold_REQD;
    }

    nextchar(pRExC_state);
    Set_Node_Length(REGNODE_p(node), RExC_parse - oregcomp_parse + 1); /* MJD */
    return node;

  regclass_failed:
    FAIL2("panic: regclass returned failure to handle_sets, " "flags=%#" UVxf,
                                                                (UV) *flagp);
}

#ifdef ENABLE_REGEX_SETS_DEBUGGING

STATIC void
S_dump_regex_sets_structures(pTHX_ RExC_state_t *pRExC_state,
                             AV * stack, const IV fence, AV * fence_stack)
{   /* Dumps the stacks in handle_regex_sets() */

    const SSize_t stack_top = av_tindex_skip_len_mg(stack);
    const SSize_t fence_stack_top = av_tindex_skip_len_mg(fence_stack);
    SSize_t i;

    PERL_ARGS_ASSERT_DUMP_REGEX_SETS_STRUCTURES;

    PerlIO_printf(Perl_debug_log, "\nParse position is:%s\n", RExC_parse);

    if (stack_top < 0) {
        PerlIO_printf(Perl_debug_log, "Nothing on stack\n");
    }
    else {
        PerlIO_printf(Perl_debug_log, "Stack: (fence=%d)\n", (int) fence);
        for (i = stack_top; i >= 0; i--) {
            SV ** element_ptr = av_fetch(stack, i, FALSE);
            if (! element_ptr) {
            }

            if (IS_OPERATOR(*element_ptr)) {
                PerlIO_printf(Perl_debug_log, "[%d]: %c\n",
                                            (int) i, (int) SvIV(*element_ptr));
            }
            else {
                PerlIO_printf(Perl_debug_log, "[%d] ", (int) i);
                sv_dump(*element_ptr);
            }
        }
    }

    if (fence_stack_top < 0) {
        PerlIO_printf(Perl_debug_log, "Nothing on fence_stack\n");
    }
    else {
        PerlIO_printf(Perl_debug_log, "Fence_stack: \n");
        for (i = fence_stack_top; i >= 0; i--) {
            SV ** element_ptr = av_fetch(fence_stack, i, FALSE);
            if (! element_ptr) {
            }

            PerlIO_printf(Perl_debug_log, "[%d]: %d\n",
                                            (int) i, (int) SvIV(*element_ptr));
        }
    }
}

#endif

#undef IS_OPERATOR
#undef IS_OPERAND

STATIC void
S_add_above_Latin1_folds(pTHX_ RExC_state_t *pRExC_state, const U8 cp, SV** invlist)
{
    /* This adds the Latin1/above-Latin1 folding rules.
     *
     * This should be called only for a Latin1-range code points, cp, which is
     * known to be involved in a simple fold with other code points above
     * Latin1.  It would give false results if /aa has been specified.
     * Multi-char folds are outside the scope of this, and must be handled
     * specially. */

    PERL_ARGS_ASSERT_ADD_ABOVE_LATIN1_FOLDS;

    assert(HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(cp));

    /* The rules that are valid for all Unicode versions are hard-coded in */
    switch (cp) {
        case 'k':
        case 'K':
          *invlist =
             add_cp_to_invlist(*invlist, KELVIN_SIGN);
            break;
        case 's':
        case 'S':
          *invlist = add_cp_to_invlist(*invlist, LATIN_SMALL_LETTER_LONG_S);
            break;
        case MICRO_SIGN:
          *invlist = add_cp_to_invlist(*invlist, GREEK_CAPITAL_LETTER_MU);
          *invlist = add_cp_to_invlist(*invlist, GREEK_SMALL_LETTER_MU);
            break;
        case LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE:
        case LATIN_SMALL_LETTER_A_WITH_RING_ABOVE:
          *invlist = add_cp_to_invlist(*invlist, ANGSTROM_SIGN);
            break;
        case LATIN_SMALL_LETTER_Y_WITH_DIAERESIS:
          *invlist = add_cp_to_invlist(*invlist,
                                        LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS);
            break;

        default:    /* Other code points are checked against the data for the
                       current Unicode version */
          {
            Size_t folds_count;
            unsigned int first_fold;
            const unsigned int * remaining_folds;
            UV folded_cp;

            if (isASCII(cp)) {
                folded_cp = toFOLD(cp);
            }
            else {
                U8 dummy_fold[UTF8_MAXBYTES_CASE+1];
                Size_t dummy_len;
                folded_cp = _to_fold_latin1(cp, dummy_fold, &dummy_len, 0);
            }

            if (folded_cp > 255) {
                *invlist = add_cp_to_invlist(*invlist, folded_cp);
            }

            folds_count = _inverse_folds(folded_cp, &first_fold,
                                                    &remaining_folds);
            if (folds_count == 0) {

                /* Use deprecated warning to increase the chances of this being
                 * output */
                ckWARN2reg_d(RExC_parse,
                        "Perl folding rules are not up-to-date for 0x%02X;"
                        " please use the perlbug utility to report;", cp);
            }
            else {
                unsigned int i;

                if (first_fold > 255) {
                    *invlist = add_cp_to_invlist(*invlist, first_fold);
                }
                for (i = 0; i < folds_count - 1; i++) {
                    if (remaining_folds[i] > 255) {
                        *invlist = add_cp_to_invlist(*invlist,
                                                    remaining_folds[i]);
                    }
                }
            }
            break;
         }
    }
}

STATIC void
S_output_posix_warnings(pTHX_ RExC_state_t *pRExC_state, AV* posix_warnings)
{
    /* Output the elements of the array given by '*posix_warnings' as REGEXP
     * warnings. */

    SV * msg;
    const bool first_is_fatal = ckDEAD(packWARN(WARN_REGEXP));

    PERL_ARGS_ASSERT_OUTPUT_POSIX_WARNINGS;

    if (! TO_OUTPUT_WARNINGS(RExC_parse)) {
        return;
    }

    while ((msg = av_shift(posix_warnings)) != &PL_sv_undef) {
        if (first_is_fatal) {           /* Avoid leaking this */
            av_undef(posix_warnings);   /* This isn't necessary if the
                                            array is mortal, but is a
                                            fail-safe */
            (void) sv_2mortal(msg);
            PREPARE_TO_DIE;
        }
        Perl_warner(aTHX_ packWARN(WARN_REGEXP), "%s", SvPVX(msg));
        SvREFCNT_dec_NN(msg);
    }

    UPDATE_WARNINGS_LOC(RExC_parse);
}

STATIC AV *
S_add_multi_match(pTHX_ AV* multi_char_matches, SV* multi_string, const STRLEN cp_count)
{
    /* This adds the string scalar <multi_string> to the array
     * <multi_char_matches>.  <multi_string> is known to have exactly
     * <cp_count> code points in it.  This is used when constructing a
     * bracketed character class and we find something that needs to match more
     * than a single character.
     *
     * <multi_char_matches> is actually an array of arrays.  Each top-level
     * element is an array that contains all the strings known so far that are
     * the same length.  And that length (in number of code points) is the same
     * as the index of the top-level array.  Hence, the [2] element is an
     * array, each element thereof is a string containing TWO code points;
     * while element [3] is for strings of THREE characters, and so on.  Since
     * this is for multi-char strings there can never be a [0] nor [1] element.
     *
     * When we rewrite the character class below, we will do so such that the
     * longest strings are written first, so that it prefers the longest
     * matching strings first.  This is done even if it turns out that any
     * quantifier is non-greedy, out of this programmer's (khw) laziness.  Tom
     * Christiansen has agreed that this is ok.  This makes the test for the
     * ligature 'ffi' come before the test for 'ff', for example */

    AV* this_array;
    AV** this_array_ptr;

    PERL_ARGS_ASSERT_ADD_MULTI_MATCH;

    if (! multi_char_matches) {
        multi_char_matches = newAV();
    }

    if (av_exists(multi_char_matches, cp_count)) {
        this_array_ptr = (AV**) av_fetch(multi_char_matches, cp_count, FALSE);
        this_array = *this_array_ptr;
    }
    else {
        this_array = newAV();
        av_store(multi_char_matches, cp_count,
                 (SV*) this_array);
    }
    av_push(this_array, multi_string);

    return multi_char_matches;
}

/* The names of properties whose definitions are not known at compile time are
 * stored in this SV, after a constant heading.  So if the length has been
 * changed since initialization, then there is a run-time definition. */
#define HAS_NONLOCALE_RUNTIME_PROPERTY_DEFINITION                            \
                                        (SvCUR(listsv) != initial_listsv_len)

/* There is a restricted set of white space characters that are legal when
 * ignoring white space in a bracketed character class.  This generates the
 * code to skip them.
 *
 * There is a line below that uses the same white space criteria but is outside
 * this macro.  Both here and there must use the same definition */
#define SKIP_BRACKETED_WHITE_SPACE(do_skip, p)                          \
    STMT_START {                                                        \
        if (do_skip) {                                                  \
            while (isBLANK_A(UCHARAT(p)))                               \
            {                                                           \
                p++;                                                    \
            }                                                           \
        }                                                               \
    } STMT_END

STATIC regnode_offset
S_regclass(pTHX_ RExC_state_t *pRExC_state, I32 *flagp, U32 depth,
                 const bool stop_at_1,  /* Just parse the next thing, don't
                                           look for a full character class */
                 bool allow_mutiple_chars,
                 const bool silence_non_portable,   /* Don't output warnings
                                                       about too large
                                                       characters */
                 const bool strict,
                 bool optimizable,                  /* ? Allow a non-ANYOF return
                                                       node */
                 SV** ret_invlist  /* Return an inversion list, not a node */
          )
{
    /* parse a bracketed class specification.  Most of these will produce an
     * ANYOF node; but something like [a] will produce an EXACT node; [aA], an
     * EXACTFish node; [[:ascii:]], a POSIXA node; etc.  It is more complex
     * under /i with multi-character folds: it will be rewritten following the
     * paradigm of this example, where the <multi-fold>s are characters which
     * fold to multiple character sequences:
     *      /[abc\x{multi-fold1}def\x{multi-fold2}ghi]/i
     * gets effectively rewritten as:
     *      /(?:\x{multi-fold1}|\x{multi-fold2}|[abcdefghi]/i
     * reg() gets called (recursively) on the rewritten version, and this
     * function will return what it constructs.  (Actually the <multi-fold>s
     * aren't physically removed from the [abcdefghi], it's just that they are
     * ignored in the recursion by means of a flag:
     * <RExC_in_multi_char_class>.)
     *
     * ANYOF nodes contain a bit map for the first NUM_ANYOF_CODE_POINTS
     * characters, with the corresponding bit set if that character is in the
     * list.  For characters above this, an inversion list is used.  There
     * are extra bits for \w, etc. in locale ANYOFs, as what these match is not
     * determinable at compile time
     *
     * On success, returns the offset at which any next node should be placed
     * into the regex engine program being compiled.
     *
     * Returns 0 otherwise, setting flagp to RESTART_PARSE if the parse needs
     * to be restarted, or'd with NEED_UTF8 if the pattern needs to be upgraded to
     * UTF-8
     */

    dVAR;
    UV prevvalue = OOB_UNICODE, save_prevvalue = OOB_UNICODE;
    IV range = 0;
    UV value = OOB_UNICODE, save_value = OOB_UNICODE;
    regnode_offset ret = -1;    /* Initialized to an illegal value */
    STRLEN numlen;
    int namedclass = OOB_NAMEDCLASS;
    char *rangebegin = NULL;
    SV *listsv = NULL;      /* List of \p{user-defined} whose definitions
                               aren't available at the time this was called */
    STRLEN initial_listsv_len = 0; /* Kind of a kludge to see if it is more
				      than just initialized.  */
    SV* properties = NULL;    /* Code points that match \p{} \P{} */
    SV* posixes = NULL;     /* Code points that match classes like [:word:],
                               extended beyond the Latin1 range.  These have to
                               be kept separate from other code points for much
                               of this function because their handling  is
                               different under /i, and for most classes under
                               /d as well */
    SV* nposixes = NULL;    /* Similarly for [:^word:].  These are kept
                               separate for a while from the non-complemented
                               versions because of complications with /d
                               matching */
    SV* simple_posixes = NULL; /* But under some conditions, the classes can be
                                  treated more simply than the general case,
                                  leading to less compilation and execution
                                  work */
    UV element_count = 0;   /* Number of distinct elements in the class.
			       Optimizations may be possible if this is tiny */
    AV * multi_char_matches = NULL; /* Code points that fold to more than one
                                       character; used under /i */
    UV n;
    char * stop_ptr = RExC_end;    /* where to stop parsing */

    /* ignore unescaped whitespace? */
    const bool skip_white = cBOOL(   ret_invlist
                                  || (RExC_flags & RXf_PMf_EXTENDED_MORE));

    /* inversion list of code points this node matches only when the target
     * string is in UTF-8.  These are all non-ASCII, < 256.  (Because is under
     * /d) */
    SV* upper_latin1_only_utf8_matches = NULL;

    /* Inversion list of code points this node matches regardless of things
     * like locale, folding, utf8ness of the target string */
    SV* cp_list = NULL;

    /* Like cp_list, but code points on this list need to be checked for things
     * that fold to/from them under /i */
    SV* cp_foldable_list = NULL;

    /* Like cp_list, but code points on this list are valid only when the
     * runtime locale is UTF-8 */
    SV* only_utf8_locale_list = NULL;

    /* In a range, if one of the endpoints is non-character-set portable,
     * meaning that it hard-codes a code point that may mean a different
     * charactger in ASCII vs. EBCDIC, as opposed to, say, a literal 'A' or a
     * mnemonic '\t' which each mean the same character no matter which
     * character set the platform is on. */
    unsigned int non_portable_endpoint = 0;

    /* Is the range unicode? which means on a platform that isn't 1-1 native
     * to Unicode (i.e. non-ASCII), each code point in it should be considered
     * to be a Unicode value.  */
    bool unicode_range = FALSE;
    bool invert = FALSE;    /* Is this class to be complemented */

    bool warn_super = ALWAYS_WARN_SUPER;

    const char * orig_parse = RExC_parse;

    /* This variable is used to mark where the end in the input is of something
     * that looks like a POSIX construct but isn't.  During the parse, when
     * something looks like it could be such a construct is encountered, it is
     * checked for being one, but not if we've already checked this area of the
     * input.  Only after this position is reached do we check again */
    char *not_posix_region_end = RExC_parse - 1;

    AV* posix_warnings = NULL;
    const bool do_posix_warnings = ckWARN(WARN_REGEXP);
    U8 op = END;    /* The returned node-type, initialized to an impossible
                       one.  */
    U8 anyof_flags = 0;   /* flag bits if the node is an ANYOF-type */
    U32 posixl = 0;       /* bit field of posix classes matched under /l */


/* Flags as to what things aren't knowable until runtime.  (Note that these are
 * mutually exclusive.) */
#define HAS_USER_DEFINED_PROPERTY 0x01   /* /u any user-defined properties that
                                            haven't been defined as of yet */
#define HAS_D_RUNTIME_DEPENDENCY  0x02   /* /d if the target being matched is
                                            UTF-8 or not */
#define HAS_L_RUNTIME_DEPENDENCY   0x04 /* /l what the posix classes match and
                                            what gets folded */
    U32 has_runtime_dependency = 0;     /* OR of the above flags */

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGCLASS;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif


    /* If wants an inversion list returned, we can't optimize to something
     * else. */
    if (ret_invlist) {
        optimizable = FALSE;
    }

    DEBUG_PARSE("clas");

#if UNICODE_MAJOR_VERSION < 3 /* no multifolds in early Unicode */      \
    || (UNICODE_MAJOR_VERSION == 3 && UNICODE_DOT_VERSION == 0          \
                                   && UNICODE_DOT_DOT_VERSION == 0)
    allow_mutiple_chars = FALSE;
#endif

    /* We include the /i status at the beginning of this so that we can
     * know it at runtime */
    listsv = sv_2mortal(Perl_newSVpvf(aTHX_ "#%d\n", cBOOL(FOLD)));
    initial_listsv_len = SvCUR(listsv);
    SvTEMP_off(listsv); /* Grr, TEMPs and mortals are conflated.  */

    SKIP_BRACKETED_WHITE_SPACE(skip_white, RExC_parse);

    assert(RExC_parse <= RExC_end);

    if (UCHARAT(RExC_parse) == '^') {	/* Complement the class */
	RExC_parse++;
        invert = TRUE;
        allow_mutiple_chars = FALSE;
        MARK_NAUGHTY(1);
        SKIP_BRACKETED_WHITE_SPACE(skip_white, RExC_parse);
    }

    /* Check that they didn't say [:posix:] instead of [[:posix:]] */
    if (! ret_invlist && MAYBE_POSIXCC(UCHARAT(RExC_parse))) {
        int maybe_class = handle_possible_posix(pRExC_state,
                                                RExC_parse,
                                                &not_posix_region_end,
                                                NULL,
                                                TRUE /* checking only */);
        if (maybe_class >= OOB_NAMEDCLASS && do_posix_warnings) {
            ckWARN4reg(not_posix_region_end,
                    "POSIX syntax [%c %c] belongs inside character classes%s",
                    *RExC_parse, *RExC_parse,
                    (maybe_class == OOB_NAMEDCLASS)
                    ? ((POSIXCC_NOTYET(*RExC_parse))
                        ? " (but this one isn't implemented)"
                        : " (but this one isn't fully valid)")
                    : ""
                    );
        }
    }

    /* If the caller wants us to just parse a single element, accomplish this
     * by faking the loop ending condition */
    if (stop_at_1 && RExC_end > RExC_parse) {
        stop_ptr = RExC_parse + 1;
    }

    /* allow 1st char to be ']' (allowing it to be '-' is dealt with later) */
    if (UCHARAT(RExC_parse) == ']')
	goto charclassloop;

    while (1) {

        if (   posix_warnings
            && av_tindex_skip_len_mg(posix_warnings) >= 0
            && RExC_parse > not_posix_region_end)
        {
            /* Warnings about posix class issues are considered tentative until
             * we are far enough along in the parse that we can no longer
             * change our mind, at which point we output them.  This is done
             * each time through the loop so that a later class won't zap them
             * before they have been dealt with. */
            output_posix_warnings(pRExC_state, posix_warnings);
        }

        if  (RExC_parse >= stop_ptr) {
            break;
        }

        SKIP_BRACKETED_WHITE_SPACE(skip_white, RExC_parse);

        if  (UCHARAT(RExC_parse) == ']') {
            break;
        }

      charclassloop:

	namedclass = OOB_NAMEDCLASS; /* initialize as illegal */
        save_value = value;
        save_prevvalue = prevvalue;

	if (!range) {
	    rangebegin = RExC_parse;
	    element_count++;
            non_portable_endpoint = 0;
	}
	if (UTF && ! UTF8_IS_INVARIANT(* RExC_parse)) {
	    value = utf8n_to_uvchr((U8*)RExC_parse,
				   RExC_end - RExC_parse,
				   &numlen, UTF8_ALLOW_DEFAULT);
	    RExC_parse += numlen;
	}
	else
	    value = UCHARAT(RExC_parse++);

        if (value == '[') {
            char * posix_class_end;
            namedclass = handle_possible_posix(pRExC_state,
                                               RExC_parse,
                                               &posix_class_end,
                                               do_posix_warnings ? &posix_warnings : NULL,
                                               FALSE    /* die if error */);
            if (namedclass > OOB_NAMEDCLASS) {

                /* If there was an earlier attempt to parse this particular
                 * posix class, and it failed, it was a false alarm, as this
                 * successful one proves */
                if (   posix_warnings
                    && av_tindex_skip_len_mg(posix_warnings) >= 0
                    && not_posix_region_end >= RExC_parse
                    && not_posix_region_end <= posix_class_end)
                {
                    av_undef(posix_warnings);
                }

                RExC_parse = posix_class_end;
            }
            else if (namedclass == OOB_NAMEDCLASS) {
                not_posix_region_end = posix_class_end;
            }
            else {
                namedclass = OOB_NAMEDCLASS;
            }
        }
        else if (   RExC_parse - 1 > not_posix_region_end
                 && MAYBE_POSIXCC(value))
        {
            (void) handle_possible_posix(
                        pRExC_state,
                        RExC_parse - 1,  /* -1 because parse has already been
                                            advanced */
                        &not_posix_region_end,
                        do_posix_warnings ? &posix_warnings : NULL,
                        TRUE /* checking only */);
        }
        else if (  strict && ! skip_white
                 && (   _generic_isCC(value, _CC_VERTSPACE)
                     || is_VERTWS_cp_high(value)))
        {
            vFAIL("Literal vertical space in [] is illegal except under /x");
        }
        else if (value == '\\') {
            /* Is a backslash; get the code point of the char after it */

            if (RExC_parse >= RExC_end) {
                vFAIL("Unmatched [");
            }

	    if (UTF && ! UTF8_IS_INVARIANT(UCHARAT(RExC_parse))) {
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

            /* If the \ is escaping white space when white space is being
             * skipped, it means that that white space is wanted literally, and
             * is already in 'value'.  Otherwise, need to translate the escape
             * into what it signifies. */
            if (! skip_white || ! isBLANK_A(value)) switch ((I32)value) {

	    case 'w':	namedclass = ANYOF_WORDCHAR;	break;
	    case 'W':	namedclass = ANYOF_NWORDCHAR;	break;
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
                    const char * const backslash_N_beg = RExC_parse - 2;
                    int cp_count;

                    if (! grok_bslash_N(pRExC_state,
                                        NULL,      /* No regnode */
                                        &value,    /* Yes single value */
                                        &cp_count, /* Multiple code pt count */
                                        flagp,
                                        strict,
                                        depth)
                    ) {

                        if (*flagp & NEED_UTF8)
                            FAIL("panic: grok_bslash_N set NEED_UTF8");

                        RETURN_FAIL_ON_RESTART_FLAGP(flagp);

                        if (cp_count < 0) {
                            vFAIL("\\N in a character class must be a named character: \\N{...}");
                        }
                        else if (cp_count == 0) {
                            ckWARNreg(RExC_parse,
                              "Ignoring zero length \\N{} in character class");
                        }
                        else { /* cp_count > 1 */
                            assert(cp_count > 1);
                            if (! RExC_in_multi_char_class) {
                                if ( ! allow_mutiple_chars
                                    || invert
                                    || range
                                    || *RExC_parse == '-')
                                {
                                    if (strict) {
                                        RExC_parse--;
                                        vFAIL("\\N{} in inverted character class or as a range end-point is restricted to one character");
                                    }
                                    ckWARNreg(RExC_parse, "Using just the first character returned by \\N{} in character class");
                                    break; /* <value> contains the first code
                                              point. Drop out of the switch to
                                              process it */
                                }
                                else {
                                    SV * multi_char_N = newSVpvn(backslash_N_beg,
                                                 RExC_parse - backslash_N_beg);
                                    multi_char_matches
                                        = add_multi_match(multi_char_matches,
                                                          multi_char_N,
                                                          cp_count);
                                }
                            }
                        } /* End of cp_count != 1 */

                        /* This element should not be processed further in this
                         * class */
                        element_count--;
                        value = save_value;
                        prevvalue = save_prevvalue;
                        continue;   /* Back to top of loop to get next char */
                    }

                    /* Here, is a single code point, and <value> contains it */
                    unicode_range = TRUE;   /* \N{} are Unicode */
                }
                break;
	    case 'p':
	    case 'P':
		{
		char *e;

		/* \p means they want Unicode semantics */
		REQUIRE_UNI_RULES(flagp, 0);

		if (RExC_parse >= RExC_end)
		    vFAIL2("Empty \\%c", (U8)value);
		if (*RExC_parse == '{') {
		    const U8 c = (U8)value;
		    e = (char *) memchr(RExC_parse, '}', RExC_end - RExC_parse);
                    if (!e) {
                        RExC_parse++;
                        vFAIL2("Missing right brace on \\%c{}", c);
                    }

                    RExC_parse++;

                    /* White space is allowed adjacent to the braces and after
                     * any '^', even when not under /x */
                    while (isSPACE(*RExC_parse)) {
                         RExC_parse++;
		    }

		    if (UCHARAT(RExC_parse) == '^') {

                        /* toggle.  (The rhs xor gets the single bit that
                         * differs between P and p; the other xor inverts just
                         * that bit) */
                        value ^= 'P' ^ 'p';

                        RExC_parse++;
                        while (isSPACE(*RExC_parse)) {
                            RExC_parse++;
                        }
                    }

                    if (e == RExC_parse)
                        vFAIL2("Empty \\%c{}", c);

		    n = e - RExC_parse;
		    while (isSPACE(*(RExC_parse + n - 1)))
		        n--;

		}   /* The \p isn't immediately followed by a '{' */
		else if (! isALPHA(*RExC_parse)) {
                    RExC_parse += (UTF)
                                  ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                                  : 1;
                    vFAIL2("Character following \\%c must be '{' or a "
                           "single-character Unicode property name",
                           (U8) value);
                }
                else {
		    e = RExC_parse;
		    n = 1;
		}
		{
                    char* name = RExC_parse;

                    /* Any message returned about expanding the definition */
                    SV* msg = newSVpvs_flags("", SVs_TEMP);

                    /* If set TRUE, the property is user-defined as opposed to
                     * official Unicode */
                    bool user_defined = FALSE;

                    SV * prop_definition = parse_uniprop_string(
                                            name, n, UTF, FOLD,
                                            FALSE, /* This is compile-time */

                                            /* We can't defer this defn when
                                             * the full result is required in
                                             * this call */
                                            ! cBOOL(ret_invlist),

                                            &user_defined,
                                            msg,
                                            0 /* Base level */
                                           );
                    if (SvCUR(msg)) {   /* Assumes any error causes a msg */
                        assert(prop_definition == NULL);
                        RExC_parse = e + 1;
                        if (SvUTF8(msg)) {  /* msg being UTF-8 makes the whole
                                               thing so, or else the display is
                                               mojibake */
                            RExC_utf8 = TRUE;
                        }
			/* diag_listed_as: Can't find Unicode property definition "%s" in regex; marked by <-- HERE in m/%s/ */
                        vFAIL2utf8f("%" UTF8f, UTF8fARG(SvUTF8(msg),
                                    SvCUR(msg), SvPVX(msg)));
                    }

                    if (! is_invlist(prop_definition)) {

                        /* Here, the definition isn't known, so we have gotten
                         * returned a string that will be evaluated if and when
                         * encountered at runtime.  We add it to the list of
                         * such properties, along with whether it should be
                         * complemented or not */
                        if (value == 'P') {
                            sv_catpvs(listsv, "!");
                        }
                        else {
                            sv_catpvs(listsv, "+");
                        }
                        sv_catsv(listsv, prop_definition);

                        has_runtime_dependency |= HAS_USER_DEFINED_PROPERTY;

                        /* We don't know yet what this matches, so have to flag
                         * it */
                        anyof_flags |= ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP;
                    }
                    else {
                        assert (prop_definition && is_invlist(prop_definition));

                        /* Here we do have the complete property definition
                         *
                         * Temporary workaround for [perl #133136].  For this
                         * precise input that is in the .t that is failing,
                         * load utf8.pm, which is what the test wants, so that
                         * that .t passes */
                        if (     memEQs(RExC_start, e + 1 - RExC_start,
                                        "foo\\p{Alnum}")
                            && ! hv_common(GvHVn(PL_incgv),
                                           NULL,
                                           "utf8.pm", sizeof("utf8.pm") - 1,
                                           0, HV_FETCH_ISEXISTS, NULL, 0))
                        {
                            require_pv("utf8.pm");
                        }

                        if (! user_defined &&
                            /* We warn on matching an above-Unicode code point
                             * if the match would return true, except don't
                             * warn for \p{All}, which has exactly one element
                             * = 0 */
                            (_invlist_contains_cp(prop_definition, 0x110000)
                                && (! (_invlist_len(prop_definition) == 1
                                       && *invlist_array(prop_definition) == 0))))
                        {
                            warn_super = TRUE;
                        }

                        /* Invert if asking for the complement */
                        if (value == 'P') {
			    _invlist_union_complement_2nd(properties,
                                                          prop_definition,
                                                          &properties);
                        }
                        else {
                            _invlist_union(properties, prop_definition, &properties);
			}
                    }
                }

		RExC_parse = e + 1;
                namedclass = ANYOF_UNIPROP;  /* no official name, but it's
                                                named */
		}
		break;
	    case 'n':	value = '\n';			break;
	    case 'r':	value = '\r';			break;
	    case 't':	value = '\t';			break;
	    case 'f':	value = '\f';			break;
	    case 'b':	value = '\b';			break;
	    case 'e':	value = ESC_NATIVE;             break;
	    case 'a':	value = '\a';                   break;
	    case 'o':
		RExC_parse--;	/* function expects to be pointed at the 'o' */
		{
		    const char* error_msg;
		    bool valid = grok_bslash_o(&RExC_parse,
                                               RExC_end,
					       &value,
					       &error_msg,
                                               TO_OUTPUT_WARNINGS(RExC_parse),
                                               strict,
                                               silence_non_portable,
                                               UTF);
		    if (! valid) {
			vFAIL(error_msg);
		    }
                    UPDATE_WARNINGS_LOC(RExC_parse - 1);
		}
                non_portable_endpoint++;
		break;
	    case 'x':
		RExC_parse--;	/* function expects to be pointed at the 'x' */
		{
		    const char* error_msg;
		    bool valid = grok_bslash_x(&RExC_parse,
                                               RExC_end,
					       &value,
					       &error_msg,
					       TO_OUTPUT_WARNINGS(RExC_parse),
                                               strict,
                                               silence_non_portable,
                                               UTF);
                    if (! valid) {
			vFAIL(error_msg);
		    }
                    UPDATE_WARNINGS_LOC(RExC_parse - 1);
		}
                non_portable_endpoint++;
		break;
	    case 'c':
		value = grok_bslash_c(*RExC_parse, TO_OUTPUT_WARNINGS(RExC_parse));
                UPDATE_WARNINGS_LOC(RExC_parse);
		RExC_parse++;
                non_portable_endpoint++;
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7':
		{
		    /* Take 1-3 octal digits */
		    I32 flags = PERL_SCAN_SILENT_ILLDIGIT;
                    numlen = (strict) ? 4 : 3;
                    value = grok_oct(--RExC_parse, &numlen, &flags, NULL);
		    RExC_parse += numlen;
                    if (numlen != 3) {
                        if (strict) {
                            RExC_parse += (UTF)
                                          ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                                          : 1;
                            vFAIL("Need exactly 3 octal digits");
                        }
                        else if (   numlen < 3 /* like \08, \178 */
                                 && RExC_parse < RExC_end
                                 && isDIGIT(*RExC_parse)
                                 && ckWARN(WARN_REGEXP))
                        {
                            reg_warn_non_literal_string(
                                 RExC_parse + 1,
                                 form_short_octal_warning(RExC_parse, numlen));
                        }
                    }
                    non_portable_endpoint++;
		    break;
		}
	    default:
		/* Allow \_ to not give an error */
		if (isWORDCHAR(value) && value != '_') {
                    if (strict) {
                        vFAIL2("Unrecognized escape \\%c in character class",
                               (int)value);
                    }
                    else {
                        ckWARN2reg(RExC_parse,
                            "Unrecognized escape \\%c in character class passed through",
                            (int)value);
                    }
		}
		break;
	    }   /* End of switch on char following backslash */
	} /* end of handling backslash escape sequences */

        /* Here, we have the current token in 'value' */

	if (namedclass > OOB_NAMEDCLASS) { /* this is a named class \blah */
            U8 classnum;

	    /* a bad range like a-\d, a-[:digit:].  The '-' is taken as a
	     * literal, as is the character that began the false range, i.e.
	     * the 'a' in the examples */
	    if (range) {
                const int w = (RExC_parse >= rangebegin)
                                ? RExC_parse - rangebegin
                                : 0;
                if (strict) {
                    vFAIL2utf8f(
                        "False [] range \"%" UTF8f "\"",
                        UTF8fARG(UTF, w, rangebegin));
                }
                else {
                    ckWARN2reg(RExC_parse,
                        "False [] range \"%" UTF8f "\"",
                        UTF8fARG(UTF, w, rangebegin));
                    cp_list = add_cp_to_invlist(cp_list, '-');
                    cp_foldable_list = add_cp_to_invlist(cp_foldable_list,
                                                            prevvalue);
                }

		range = 0; /* this was not a true range */
                element_count += 2; /* So counts for three values */
	    }

            classnum = namedclass_to_classnum(namedclass);

	    if (LOC && namedclass < ANYOF_POSIXL_MAX
#ifndef HAS_ISASCII
                && classnum != _CC_ASCII
#endif
            ) {
                SV* scratch_list = NULL;

                /* What the Posix classes (like \w, [:space:]) match isn't
                 * generally knowable under locale until actual match time.  A
                 * special node is used for these which has extra space for a
                 * bitmap, with a bit reserved for each named class that is to
                 * be matched against.  (This isn't needed for \p{} and
                 * pseudo-classes, as they are not affected by locale, and
                 * hence are dealt with separately.)  However, if a named class
                 * and its complement are both present, then it matches
                 * everything, and there is no runtime dependency.  Odd numbers
                 * are the complements of the next lower number, so xor works.
                 * (Note that something like [\w\D] should match everything,
                 * because \d should be a proper subset of \w.  But rather than
                 * trust that the locale is well behaved, we leave this to
                 * runtime to sort out) */
                if (POSIXL_TEST(posixl, namedclass ^ 1)) {
                    cp_list = _add_range_to_invlist(cp_list, 0, UV_MAX);
                    POSIXL_ZERO(posixl);
                    has_runtime_dependency &= ~HAS_L_RUNTIME_DEPENDENCY;
                    anyof_flags &= ~ANYOF_MATCHES_POSIXL;
                    continue;   /* We could ignore the rest of the class, but
                                   best to parse it for any errors */
                }
                else { /* Here, isn't the complement of any already parsed
                          class */
                    POSIXL_SET(posixl, namedclass);
                    has_runtime_dependency |= HAS_L_RUNTIME_DEPENDENCY;
                    anyof_flags |= ANYOF_MATCHES_POSIXL;

                    /* The above-Latin1 characters are not subject to locale
                     * rules.  Just add them to the unconditionally-matched
                     * list */

                    /* Get the list of the above-Latin1 code points this
                     * matches */
                    _invlist_intersection_maybe_complement_2nd(PL_AboveLatin1,
                                            PL_XPosix_ptrs[classnum],

                                            /* Odd numbers are complements,
                                             * like NDIGIT, NASCII, ... */
                                            namedclass % 2 != 0,
                                            &scratch_list);
                    /* Checking if 'cp_list' is NULL first saves an extra
                     * clone.  Its reference count will be decremented at the
                     * next union, etc, or if this is the only instance, at the
                     * end of the routine */
                    if (! cp_list) {
                        cp_list = scratch_list;
                    }
                    else {
                        _invlist_union(cp_list, scratch_list, &cp_list);
                        SvREFCNT_dec_NN(scratch_list);
                    }
                    continue;   /* Go get next character */
                }
            }
            else {

                /* Here, is not /l, or is a POSIX class for which /l doesn't
                 * matter (or is a Unicode property, which is skipped here). */
                if (namedclass >= ANYOF_POSIXL_MAX) {  /* If a special class */
                    if (namedclass != ANYOF_UNIPROP) { /* UNIPROP = \p and \P */

                        /* Here, should be \h, \H, \v, or \V.  None of /d, /i
                         * nor /l make a difference in what these match,
                         * therefore we just add what they match to cp_list. */
                        if (classnum != _CC_VERTSPACE) {
                            assert(   namedclass == ANYOF_HORIZWS
                                   || namedclass == ANYOF_NHORIZWS);

                            /* It turns out that \h is just a synonym for
                             * XPosixBlank */
                            classnum = _CC_BLANK;
                        }

                        _invlist_union_maybe_complement_2nd(
                                cp_list,
                                PL_XPosix_ptrs[classnum],
                                namedclass % 2 != 0,    /* Complement if odd
                                                          (NHORIZWS, NVERTWS)
                                                        */
                                &cp_list);
                    }
                }
                else if (   AT_LEAST_UNI_SEMANTICS
                         || classnum == _CC_ASCII
                         || (DEPENDS_SEMANTICS && (   classnum == _CC_DIGIT
                                                   || classnum == _CC_XDIGIT)))
                {
                    /* We usually have to worry about /d affecting what POSIX
                     * classes match, with special code needed because we won't
                     * know until runtime what all matches.  But there is no
                     * extra work needed under /u and /a; and [:ascii:] is
                     * unaffected by /d; and :digit: and :xdigit: don't have
                     * runtime differences under /d.  So we can special case
                     * these, and avoid some extra work below, and at runtime.
                     * */
                    _invlist_union_maybe_complement_2nd(
                                                     simple_posixes,
                                                      ((AT_LEAST_ASCII_RESTRICTED)
                                                       ? PL_Posix_ptrs[classnum]
                                                       : PL_XPosix_ptrs[classnum]),
                                                     namedclass % 2 != 0,
                                                     &simple_posixes);
                }
                else {  /* Garden variety class.  If is NUPPER, NALPHA, ...
                           complement and use nposixes */
                    SV** posixes_ptr = namedclass % 2 == 0
                                       ? &posixes
                                       : &nposixes;
                    _invlist_union_maybe_complement_2nd(
                                                     *posixes_ptr,
                                                     PL_XPosix_ptrs[classnum],
                                                     namedclass % 2 != 0,
                                                     posixes_ptr);
                }
	    }
	} /* end of namedclass \blah */

        SKIP_BRACKETED_WHITE_SPACE(skip_white, RExC_parse);

        /* If 'range' is set, 'value' is the ending of a range--check its
         * validity.  (If value isn't a single code point in the case of a
         * range, we should have figured that out above in the code that
         * catches false ranges).  Later, we will handle each individual code
         * point in the range.  If 'range' isn't set, this could be the
         * beginning of a range, so check for that by looking ahead to see if
         * the next real character to be processed is the range indicator--the
         * minus sign */

	if (range) {
#ifdef EBCDIC
            /* For unicode ranges, we have to test that the Unicode as opposed
             * to the native values are not decreasing.  (Above 255, there is
             * no difference between native and Unicode) */
	    if (unicode_range && prevvalue < 255 && value < 255) {
                if (NATIVE_TO_LATIN1(prevvalue) > NATIVE_TO_LATIN1(value)) {
                    goto backwards_range;
                }
            }
            else
#endif
	    if (prevvalue > value) /* b-a */ {
		int w;
#ifdef EBCDIC
              backwards_range:
#endif
                w = RExC_parse - rangebegin;
                vFAIL2utf8f(
                    "Invalid [] range \"%" UTF8f "\"",
                    UTF8fARG(UTF, w, rangebegin));
                NOT_REACHED; /* NOTREACHED */
	    }
	}
	else {
            prevvalue = value; /* save the beginning of the potential range */
            if (! stop_at_1     /* Can't be a range if parsing just one thing */
                && *RExC_parse == '-')
            {
                char* next_char_ptr = RExC_parse + 1;

                /* Get the next real char after the '-' */
                SKIP_BRACKETED_WHITE_SPACE(skip_white, next_char_ptr);

                /* If the '-' is at the end of the class (just before the ']',
                 * it is a literal minus; otherwise it is a range */
                if (next_char_ptr < RExC_end && *next_char_ptr != ']') {
                    RExC_parse = next_char_ptr;

                    /* a bad range like \w-, [:word:]- ? */
                    if (namedclass > OOB_NAMEDCLASS) {
                        if (strict || ckWARN(WARN_REGEXP)) {
                            const int w = RExC_parse >= rangebegin
                                          ?  RExC_parse - rangebegin
                                          : 0;
                            if (strict) {
                                vFAIL4("False [] range \"%*.*s\"",
                                    w, w, rangebegin);
                            }
                            else {
                                vWARN4(RExC_parse,
                                    "False [] range \"%*.*s\"",
                                    w, w, rangebegin);
                            }
                        }
                        cp_list = add_cp_to_invlist(cp_list, '-');
                        element_count++;
                    } else
                        range = 1;	/* yeah, it's a range! */
                    continue;	/* but do it the next time */
                }
	    }
	}

        if (namedclass > OOB_NAMEDCLASS) {
            continue;
        }

        /* Here, we have a single value this time through the loop, and
         * <prevvalue> is the beginning of the range, if any; or <value> if
         * not. */

	/* non-Latin1 code point implies unicode semantics. */
	if (value > 255) {
            REQUIRE_UNI_RULES(flagp, 0);
	}

        /* Ready to process either the single value, or the completed range.
         * For single-valued non-inverted ranges, we consider the possibility
         * of multi-char folds.  (We made a conscious decision to not do this
         * for the other cases because it can often lead to non-intuitive
         * results.  For example, you have the peculiar case that:
         *  "s s" =~ /^[^\xDF]+$/i => Y
         *  "ss"  =~ /^[^\xDF]+$/i => N
         *
         * See [perl #89750] */
        if (FOLD && allow_mutiple_chars && value == prevvalue) {
            if (    value == LATIN_SMALL_LETTER_SHARP_S
                || (value > 255 && _invlist_contains_cp(PL_HasMultiCharFold,
                                                        value)))
            {
                /* Here <value> is indeed a multi-char fold.  Get what it is */

                U8 foldbuf[UTF8_MAXBYTES_CASE+1];
                STRLEN foldlen;

                UV folded = _to_uni_fold_flags(
                                value,
                                foldbuf,
                                &foldlen,
                                FOLD_FLAGS_FULL | (ASCII_FOLD_RESTRICTED
                                                   ? FOLD_FLAGS_NOMIX_ASCII
                                                   : 0)
                                );

                /* Here, <folded> should be the first character of the
                 * multi-char fold of <value>, with <foldbuf> containing the
                 * whole thing.  But, if this fold is not allowed (because of
                 * the flags), <fold> will be the same as <value>, and should
                 * be processed like any other character, so skip the special
                 * handling */
                if (folded != value) {

                    /* Skip if we are recursed, currently parsing the class
                     * again.  Otherwise add this character to the list of
                     * multi-char folds. */
                    if (! RExC_in_multi_char_class) {
                        STRLEN cp_count = utf8_length(foldbuf,
                                                      foldbuf + foldlen);
                        SV* multi_fold = sv_2mortal(newSVpvs(""));

                        Perl_sv_catpvf(aTHX_ multi_fold, "\\x{%" UVXf "}", value);

                        multi_char_matches
                                        = add_multi_match(multi_char_matches,
                                                          multi_fold,
                                                          cp_count);

                    }

                    /* This element should not be processed further in this
                     * class */
                    element_count--;
                    value = save_value;
                    prevvalue = save_prevvalue;
                    continue;
                }
            }
        }

        if (strict && ckWARN(WARN_REGEXP)) {
            if (range) {

                /* If the range starts above 255, everything is portable and
                 * likely to be so for any forseeable character set, so don't
                 * warn. */
                if (unicode_range && non_portable_endpoint && prevvalue < 256) {
                    vWARN(RExC_parse, "Both or neither range ends should be Unicode");
                }
                else if (prevvalue != value) {

                    /* Under strict, ranges that stop and/or end in an ASCII
                     * printable should have each end point be a portable value
                     * for it (preferably like 'A', but we don't warn if it is
                     * a (portable) Unicode name or code point), and the range
                     * must be be all digits or all letters of the same case.
                     * Otherwise, the range is non-portable and unclear as to
                     * what it contains */
                    if (             (isPRINT_A(prevvalue) || isPRINT_A(value))
                        && (          non_portable_endpoint
                            || ! (   (isDIGIT_A(prevvalue) && isDIGIT_A(value))
                                  || (isLOWER_A(prevvalue) && isLOWER_A(value))
                                  || (isUPPER_A(prevvalue) && isUPPER_A(value))
                    ))) {
                        vWARN(RExC_parse, "Ranges of ASCII printables should"
                                          " be some subset of \"0-9\","
                                          " \"A-Z\", or \"a-z\"");
                    }
                    else if (prevvalue >= FIRST_NON_ASCII_DECIMAL_DIGIT) {
                        SSize_t index_start;
                        SSize_t index_final;

                        /* But the nature of Unicode and languages mean we
                         * can't do the same checks for above-ASCII ranges,
                         * except in the case of digit ones.  These should
                         * contain only digits from the same group of 10.  The
                         * ASCII case is handled just above.  Hence here, the
                         * range could be a range of digits.  First some
                         * unlikely special cases.  Grandfather in that a range
                         * ending in 19DA (NEW TAI LUE THAM DIGIT ONE) is bad
                         * if its starting value is one of the 10 digits prior
                         * to it.  This is because it is an alternate way of
                         * writing 19D1, and some people may expect it to be in
                         * that group.  But it is bad, because it won't give
                         * the expected results.  In Unicode 5.2 it was
                         * considered to be in that group (of 11, hence), but
                         * this was fixed in the next version */

                        if (UNLIKELY(value == 0x19DA && prevvalue >= 0x19D0)) {
                            goto warn_bad_digit_range;
                        }
                        else if (UNLIKELY(   prevvalue >= 0x1D7CE
                                          &&     value <= 0x1D7FF))
                        {
                            /* This is the only other case currently in Unicode
                             * where the algorithm below fails.  The code
                             * points just above are the end points of a single
                             * range containing only decimal digits.  It is 5
                             * different series of 0-9.  All other ranges of
                             * digits currently in Unicode are just a single
                             * series.  (And mktables will notify us if a later
                             * Unicode version breaks this.)
                             *
                             * If the range being checked is at most 9 long,
                             * and the digit values represented are in
                             * numerical order, they are from the same series.
                             * */
                            if (         value - prevvalue > 9
                                ||    (((    value - 0x1D7CE) % 10)
                                     <= (prevvalue - 0x1D7CE) % 10))
                            {
                                goto warn_bad_digit_range;
                            }
                        }
                        else {

                            /* For all other ranges of digits in Unicode, the
                             * algorithm is just to check if both end points
                             * are in the same series, which is the same range.
                             * */
                            index_start = _invlist_search(
                                                    PL_XPosix_ptrs[_CC_DIGIT],
                                                    prevvalue);

                            /* Warn if the range starts and ends with a digit,
                             * and they are not in the same group of 10. */
                            if (   index_start >= 0
                                && ELEMENT_RANGE_MATCHES_INVLIST(index_start)
                                && (index_final =
                                    _invlist_search(PL_XPosix_ptrs[_CC_DIGIT],
                                                    value)) != index_start
                                && index_final >= 0
                                && ELEMENT_RANGE_MATCHES_INVLIST(index_final))
                            {
                              warn_bad_digit_range:
                                vWARN(RExC_parse, "Ranges of digits should be"
                                                  " from the same group of"
                                                  " 10");
                            }
                        }
                    }
                }
            }
            if ((! range || prevvalue == value) && non_portable_endpoint) {
                if (isPRINT_A(value)) {
                    char literal[3];
                    unsigned d = 0;
                    if (isBACKSLASHED_PUNCT(value)) {
                        literal[d++] = '\\';
                    }
                    literal[d++] = (char) value;
                    literal[d++] = '\0';

                    vWARN4(RExC_parse,
                           "\"%.*s\" is more clearly written simply as \"%s\"",
                           (int) (RExC_parse - rangebegin),
                           rangebegin,
                           literal
                        );
                }
                else if isMNEMONIC_CNTRL(value) {
                    vWARN4(RExC_parse,
                           "\"%.*s\" is more clearly written simply as \"%s\"",
                           (int) (RExC_parse - rangebegin),
                           rangebegin,
                           cntrl_to_mnemonic((U8) value)
                        );
                }
            }
        }

        /* Deal with this element of the class */

#ifndef EBCDIC
        cp_foldable_list = _add_range_to_invlist(cp_foldable_list,
                                                    prevvalue, value);
#else
        /* On non-ASCII platforms, for ranges that span all of 0..255, and ones
         * that don't require special handling, we can just add the range like
         * we do for ASCII platforms */
        if ((UNLIKELY(prevvalue == 0) && value >= 255)
            || ! (prevvalue < 256
                    && (unicode_range
                        || (! non_portable_endpoint
                            && ((isLOWER_A(prevvalue) && isLOWER_A(value))
                                || (isUPPER_A(prevvalue)
                                    && isUPPER_A(value)))))))
        {
            cp_foldable_list = _add_range_to_invlist(cp_foldable_list,
                                                        prevvalue, value);
        }
        else {
            /* Here, requires special handling.  This can be because it is a
             * range whose code points are considered to be Unicode, and so
             * must be individually translated into native, or because its a
             * subrange of 'A-Z' or 'a-z' which each aren't contiguous in
             * EBCDIC, but we have defined them to include only the "expected"
             * upper or lower case ASCII alphabetics.  Subranges above 255 are
             * the same in native and Unicode, so can be added as a range */
            U8 start = NATIVE_TO_LATIN1(prevvalue);
            unsigned j;
            U8 end = (value < 256) ? NATIVE_TO_LATIN1(value) : 255;
            for (j = start; j <= end; j++) {
                cp_foldable_list = add_cp_to_invlist(cp_foldable_list, LATIN1_TO_NATIVE(j));
            }
            if (value > 255) {
                cp_foldable_list = _add_range_to_invlist(cp_foldable_list,
                                                            256, value);
            }
        }
#endif

	range = 0; /* this range (if it was one) is done now */
    } /* End of loop through all the text within the brackets */

    if (   posix_warnings && av_tindex_skip_len_mg(posix_warnings) >= 0) {
        output_posix_warnings(pRExC_state, posix_warnings);
    }

    /* If anything in the class expands to more than one character, we have to
     * deal with them by building up a substitute parse string, and recursively
     * calling reg() on it, instead of proceeding */
    if (multi_char_matches) {
	SV * substitute_parse = newSVpvn_flags("?:", 2, SVs_TEMP);
        I32 cp_count;
	STRLEN len;
	char *save_end = RExC_end;
	char *save_parse = RExC_parse;
	char *save_start = RExC_start;
        Size_t constructed_prefix_len = 0; /* This gives the length of the
                                              constructed portion of the
                                              substitute parse. */
        bool first_time = TRUE;     /* First multi-char occurrence doesn't get
                                       a "|" */
        I32 reg_flags;

        assert(! invert);
        /* Only one level of recursion allowed */
        assert(RExC_copy_start_in_constructed == RExC_precomp);

#if 0   /* Have decided not to deal with multi-char folds in inverted classes,
           because too confusing */
        if (invert) {
            sv_catpvs(substitute_parse, "(?:");
        }
#endif

        /* Look at the longest folds first */
        for (cp_count = av_tindex_skip_len_mg(multi_char_matches);
                        cp_count > 0;
                        cp_count--)
        {

            if (av_exists(multi_char_matches, cp_count)) {
                AV** this_array_ptr;
                SV* this_sequence;

                this_array_ptr = (AV**) av_fetch(multi_char_matches,
                                                 cp_count, FALSE);
                while ((this_sequence = av_pop(*this_array_ptr)) !=
                                                                &PL_sv_undef)
                {
                    if (! first_time) {
                        sv_catpvs(substitute_parse, "|");
                    }
                    first_time = FALSE;

                    sv_catpv(substitute_parse, SvPVX(this_sequence));
                }
            }
        }

        /* If the character class contains anything else besides these
         * multi-character folds, have to include it in recursive parsing */
        if (element_count) {
            sv_catpvs(substitute_parse, "|[");
            constructed_prefix_len = SvCUR(substitute_parse);
            sv_catpvn(substitute_parse, orig_parse, RExC_parse - orig_parse);

            /* Put in a closing ']' only if not going off the end, as otherwise
             * we are adding something that really isn't there */
            if (RExC_parse < RExC_end) {
                sv_catpvs(substitute_parse, "]");
            }
        }

        sv_catpvs(substitute_parse, ")");
#if 0
        if (invert) {
            /* This is a way to get the parse to skip forward a whole named
             * sequence instead of matching the 2nd character when it fails the
             * first */
            sv_catpvs(substitute_parse, "(*THEN)(*SKIP)(*FAIL)|.)");
        }
#endif

        /* Set up the data structure so that any errors will be properly
         * reported.  See the comments at the definition of
         * REPORT_LOCATION_ARGS for details */
        RExC_copy_start_in_input = (char *) orig_parse;
	RExC_start = RExC_parse = SvPV(substitute_parse, len);
        RExC_copy_start_in_constructed = RExC_start + constructed_prefix_len;
	RExC_end = RExC_parse + len;
        RExC_in_multi_char_class = 1;

	ret = reg(pRExC_state, 1, &reg_flags, depth+1);

        *flagp |= reg_flags & (HASWIDTH|SIMPLE|SPSTART|POSTPONED|RESTART_PARSE|NEED_UTF8);

        /* And restore so can parse the rest of the pattern */
        RExC_parse = save_parse;
	RExC_start = RExC_copy_start_in_constructed = RExC_copy_start_in_input = save_start;
	RExC_end = save_end;
	RExC_in_multi_char_class = 0;
        SvREFCNT_dec_NN(multi_char_matches);
        return ret;
    }

    /* If folding, we calculate all characters that could fold to or from the
     * ones already on the list */
    if (cp_foldable_list) {
        if (FOLD) {
            UV start, end;	/* End points of code point ranges */

            SV* fold_intersection = NULL;
            SV** use_list;

            /* Our calculated list will be for Unicode rules.  For locale
             * matching, we have to keep a separate list that is consulted at
             * runtime only when the locale indicates Unicode rules (and we
             * don't include potential matches in the ASCII/Latin1 range, as
             * any code point could fold to any other, based on the run-time
             * locale).   For non-locale, we just use the general list */
            if (LOC) {
                use_list = &only_utf8_locale_list;
            }
            else {
                use_list = &cp_list;
            }

            /* Only the characters in this class that participate in folds need
             * be checked.  Get the intersection of this class and all the
             * possible characters that are foldable.  This can quickly narrow
             * down a large class */
            _invlist_intersection(PL_in_some_fold, cp_foldable_list,
                                  &fold_intersection);

            /* Now look at the foldable characters in this class individually */
            invlist_iterinit(fold_intersection);
            while (invlist_iternext(fold_intersection, &start, &end)) {
                UV j;
                UV folded;

                /* Look at every character in the range */
                for (j = start; j <= end; j++) {
                    U8 foldbuf[UTF8_MAXBYTES_CASE+1];
                    STRLEN foldlen;
                    unsigned int k;
                    Size_t folds_count;
                    unsigned int first_fold;
                    const unsigned int * remaining_folds;

                    if (j < 256) {

                        /* Under /l, we don't know what code points below 256
                         * fold to, except we do know the MICRO SIGN folds to
                         * an above-255 character if the locale is UTF-8, so we
                         * add it to the special list (in *use_list)  Otherwise
                         * we know now what things can match, though some folds
                         * are valid under /d only if the target is UTF-8.
                         * Those go in a separate list */
                        if (      IS_IN_SOME_FOLD_L1(j)
                            && ! (LOC && j != MICRO_SIGN))
                        {

                            /* ASCII is always matched; non-ASCII is matched
                             * only under Unicode rules (which could happen
                             * under /l if the locale is a UTF-8 one */
                            if (isASCII(j) || ! DEPENDS_SEMANTICS) {
                                *use_list = add_cp_to_invlist(*use_list,
                                                            PL_fold_latin1[j]);
                            }
                            else if (j != PL_fold_latin1[j]) {
                                upper_latin1_only_utf8_matches
                                        = add_cp_to_invlist(
                                                upper_latin1_only_utf8_matches,
                                                PL_fold_latin1[j]);
                            }
                        }

                        if (HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(j)
                            && (! isASCII(j) || ! ASCII_FOLD_RESTRICTED))
                        {
                            add_above_Latin1_folds(pRExC_state,
                                                   (U8) j,
                                                   use_list);
                        }
                        continue;
                    }

                    /* Here is an above Latin1 character.  We don't have the
                     * rules hard-coded for it.  First, get its fold.  This is
                     * the simple fold, as the multi-character folds have been
                     * handled earlier and separated out */
                    folded = _to_uni_fold_flags(j, foldbuf, &foldlen,
                                                        (ASCII_FOLD_RESTRICTED)
                                                        ? FOLD_FLAGS_NOMIX_ASCII
                                                        : 0);

                    /* Single character fold of above Latin1.  Add everything
                     * in its fold closure to the list that this node should
                     * match. */
                    folds_count = _inverse_folds(folded, &first_fold,
                                                    &remaining_folds);
                    for (k = 0; k <= folds_count; k++) {
                        UV c = (k == 0)     /* First time through use itself */
                                ? folded
                                : (k == 1)  /* 2nd time use, the first fold */
                                   ? first_fold

                                     /* Then the remaining ones */
                                   : remaining_folds[k-2];

                        /* /aa doesn't allow folds between ASCII and non- */
                        if ((   ASCII_FOLD_RESTRICTED
                            && (isASCII(c) != isASCII(j))))
                        {
                            continue;
                        }

                        /* Folds under /l which cross the 255/256 boundary are
                         * added to a separate list.  (These are valid only
                         * when the locale is UTF-8.) */
                        if (c < 256 && LOC) {
                            *use_list = add_cp_to_invlist(*use_list, c);
                            continue;
                        }

                        if (isASCII(c) || c > 255 || AT_LEAST_UNI_SEMANTICS)
                        {
                            cp_list = add_cp_to_invlist(cp_list, c);
                        }
                        else {
                            /* Similarly folds involving non-ascii Latin1
                             * characters under /d are added to their list */
                            upper_latin1_only_utf8_matches
                                    = add_cp_to_invlist(
                                                upper_latin1_only_utf8_matches,
                                                c);
                        }
                    }
                }
            }
            SvREFCNT_dec_NN(fold_intersection);
        }

        /* Now that we have finished adding all the folds, there is no reason
         * to keep the foldable list separate */
        _invlist_union(cp_list, cp_foldable_list, &cp_list);
	SvREFCNT_dec_NN(cp_foldable_list);
    }

    /* And combine the result (if any) with any inversion lists from posix
     * classes.  The lists are kept separate up to now because we don't want to
     * fold the classes */
    if (simple_posixes) {   /* These are the classes known to be unaffected by
                               /a, /aa, and /d */
        if (cp_list) {
            _invlist_union(cp_list, simple_posixes, &cp_list);
            SvREFCNT_dec_NN(simple_posixes);
        }
        else {
            cp_list = simple_posixes;
        }
    }
    if (posixes || nposixes) {
        if (! DEPENDS_SEMANTICS) {

            /* For everything but /d, we can just add the current 'posixes' and
             * 'nposixes' to the main list */
            if (posixes) {
                if (cp_list) {
                    _invlist_union(cp_list, posixes, &cp_list);
                    SvREFCNT_dec_NN(posixes);
                }
                else {
                    cp_list = posixes;
                }
            }
            if (nposixes) {
                if (cp_list) {
                    _invlist_union(cp_list, nposixes, &cp_list);
                    SvREFCNT_dec_NN(nposixes);
                }
                else {
                    cp_list = nposixes;
                }
            }
        }
        else {
            /* Under /d, things like \w match upper Latin1 characters only if
             * the target string is in UTF-8.  But things like \W match all the
             * upper Latin1 characters if the target string is not in UTF-8.
             *
             * Handle the case with something like \W separately */
            if (nposixes) {
                SV* only_non_utf8_list = invlist_clone(PL_UpperLatin1, NULL);

                /* A complemented posix class matches all upper Latin1
                 * characters if not in UTF-8.  And it matches just certain
                 * ones when in UTF-8.  That means those certain ones are
                 * matched regardless, so can just be added to the
                 * unconditional list */
                if (cp_list) {
                    _invlist_union(cp_list, nposixes, &cp_list);
                    SvREFCNT_dec_NN(nposixes);
                    nposixes = NULL;
                }
                else {
                    cp_list = nposixes;
                }

                /* Likewise for 'posixes' */
                _invlist_union(posixes, cp_list, &cp_list);
                SvREFCNT_dec(posixes);

                /* Likewise for anything else in the range that matched only
                 * under UTF-8 */
                if (upper_latin1_only_utf8_matches) {
                    _invlist_union(cp_list,
                                   upper_latin1_only_utf8_matches,
                                   &cp_list);
                    SvREFCNT_dec_NN(upper_latin1_only_utf8_matches);
                    upper_latin1_only_utf8_matches = NULL;
                }

                /* If we don't match all the upper Latin1 characters regardless
                 * of UTF-8ness, we have to set a flag to match the rest when
                 * not in UTF-8 */
                _invlist_subtract(only_non_utf8_list, cp_list,
                                  &only_non_utf8_list);
                if (_invlist_len(only_non_utf8_list) != 0) {
                    anyof_flags |= ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER;
                }
                SvREFCNT_dec_NN(only_non_utf8_list);
            }
            else {
                /* Here there were no complemented posix classes.  That means
                 * the upper Latin1 characters in 'posixes' match only when the
                 * target string is in UTF-8.  So we have to add them to the
                 * list of those types of code points, while adding the
                 * remainder to the unconditional list.
                 *
                 * First calculate what they are */
                SV* nonascii_but_latin1_properties = NULL;
                _invlist_intersection(posixes, PL_UpperLatin1,
                                      &nonascii_but_latin1_properties);

                /* And add them to the final list of such characters. */
                _invlist_union(upper_latin1_only_utf8_matches,
                               nonascii_but_latin1_properties,
                               &upper_latin1_only_utf8_matches);

                /* Remove them from what now becomes the unconditional list */
                _invlist_subtract(posixes, nonascii_but_latin1_properties,
                                  &posixes);

                /* And add those unconditional ones to the final list */
                if (cp_list) {
                    _invlist_union(cp_list, posixes, &cp_list);
                    SvREFCNT_dec_NN(posixes);
                    posixes = NULL;
                }
                else {
                    cp_list = posixes;
                }

                SvREFCNT_dec(nonascii_but_latin1_properties);

                /* Get rid of any characters from the conditional list that we
                 * now know are matched unconditionally, which may make that
                 * list empty */
                _invlist_subtract(upper_latin1_only_utf8_matches,
                                  cp_list,
                                  &upper_latin1_only_utf8_matches);
                if (_invlist_len(upper_latin1_only_utf8_matches) == 0) {
                    SvREFCNT_dec_NN(upper_latin1_only_utf8_matches);
                    upper_latin1_only_utf8_matches = NULL;
                }
            }
        }
    }

    /* And combine the result (if any) with any inversion list from properties.
     * The lists are kept separate up to now so that we can distinguish the two
     * in regards to matching above-Unicode.  A run-time warning is generated
     * if a Unicode property is matched against a non-Unicode code point. But,
     * we allow user-defined properties to match anything, without any warning,
     * and we also suppress the warning if there is a portion of the character
     * class that isn't a Unicode property, and which matches above Unicode, \W
     * or [\x{110000}] for example.
     * (Note that in this case, unlike the Posix one above, there is no
     * <upper_latin1_only_utf8_matches>, because having a Unicode property
     * forces Unicode semantics */
    if (properties) {
        if (cp_list) {

            /* If it matters to the final outcome, see if a non-property
             * component of the class matches above Unicode.  If so, the
             * warning gets suppressed.  This is true even if just a single
             * such code point is specified, as, though not strictly correct if
             * another such code point is matched against, the fact that they
             * are using above-Unicode code points indicates they should know
             * the issues involved */
            if (warn_super) {
                warn_super = ! (invert
                               ^ (invlist_highest(cp_list) > PERL_UNICODE_MAX));
            }

            _invlist_union(properties, cp_list, &cp_list);
            SvREFCNT_dec_NN(properties);
        }
        else {
            cp_list = properties;
        }

        if (warn_super) {
            anyof_flags
             |= ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER;

            /* Because an ANYOF node is the only one that warns, this node
             * can't be optimized into something else */
            optimizable = FALSE;
        }
    }

    /* Here, we have calculated what code points should be in the character
     * class.
     *
     * Now we can see about various optimizations.  Fold calculation (which we
     * did above) needs to take place before inversion.  Otherwise /[^k]/i
     * would invert to include K, which under /i would match k, which it
     * shouldn't.  Therefore we can't invert folded locale now, as it won't be
     * folded until runtime */

    /* If we didn't do folding, it's because some information isn't available
     * until runtime; set the run-time fold flag for these  We know to set the
     * flag if we have a non-NULL list for UTF-8 locales, or the class matches
     * at least one 0-255 range code point */
    if (LOC && FOLD) {

        /* Some things on the list might be unconditionally included because of
         * other components.  Remove them, and clean up the list if it goes to
         * 0 elements */
        if (only_utf8_locale_list && cp_list) {
            _invlist_subtract(only_utf8_locale_list, cp_list,
                              &only_utf8_locale_list);

            if (_invlist_len(only_utf8_locale_list) == 0) {
                SvREFCNT_dec_NN(only_utf8_locale_list);
                only_utf8_locale_list = NULL;
            }
        }
        if (    only_utf8_locale_list
            || (cp_list && (   _invlist_contains_cp(cp_list, LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE)
                            || _invlist_contains_cp(cp_list, LATIN_SMALL_LETTER_DOTLESS_I))))
        {
            has_runtime_dependency |= HAS_L_RUNTIME_DEPENDENCY;
            anyof_flags
                 |= ANYOFL_FOLD
                 |  ANYOFL_SHARED_UTF8_LOCALE_fold_HAS_MATCHES_nonfold_REQD;
        }
        else if (cp_list) { /* Look to see if a 0-255 code point is in list */
            UV start, end;
            invlist_iterinit(cp_list);
            if (invlist_iternext(cp_list, &start, &end) && start < 256) {
                anyof_flags |= ANYOFL_FOLD;
                has_runtime_dependency |= HAS_L_RUNTIME_DEPENDENCY;
            }
            invlist_iterfinish(cp_list);
        }
    }
    else if (   DEPENDS_SEMANTICS
             && (    upper_latin1_only_utf8_matches
                 || (anyof_flags & ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER)))
    {
        RExC_seen_d_op = TRUE;
        has_runtime_dependency |= HAS_D_RUNTIME_DEPENDENCY;
    }

    /* Optimize inverted patterns (e.g. [^a-z]) when everything is known at
     * compile time. */
    if (     cp_list
        &&   invert
        && ! has_runtime_dependency)
    {
        _invlist_invert(cp_list);

	/* Clear the invert flag since have just done it here */
	invert = FALSE;
    }

    if (ret_invlist) {
        *ret_invlist = cp_list;

        return RExC_emit;
    }

    /* All possible optimizations below still have these characteristics.
     * (Multi-char folds aren't SIMPLE, but they don't get this far in this
     * routine) */
    *flagp |= HASWIDTH|SIMPLE;

    if (anyof_flags & ANYOF_LOCALE_FLAGS) {
        RExC_contains_locale = 1;
    }

    /* Some character classes are equivalent to other nodes.  Such nodes take
     * up less room, and some nodes require fewer operations to execute, than
     * ANYOF nodes.  EXACTish nodes may be joinable with adjacent nodes to
     * improve efficiency. */

    if (optimizable) {
        PERL_UINT_FAST8_T i;
        Size_t partial_cp_count = 0;
        UV start[MAX_FOLD_FROMS+1] = { 0 }; /* +1 for the folded-to char */
        UV   end[MAX_FOLD_FROMS+1] = { 0 };

        if (cp_list) { /* Count the code points in enough ranges that we would
                          see all the ones possible in any fold in this version
                          of Unicode */

            invlist_iterinit(cp_list);
            for (i = 0; i <= MAX_FOLD_FROMS; i++) {
                if (! invlist_iternext(cp_list, &start[i], &end[i])) {
                    break;
                }
                partial_cp_count += end[i] - start[i] + 1;
            }

            invlist_iterfinish(cp_list);
        }

        /* If we know at compile time that this matches every possible code
         * point, any run-time dependencies don't matter */
        if (start[0] == 0 && end[0] == UV_MAX) {
            if (invert) {
                ret = reganode(pRExC_state, OPFAIL, 0);
            }
            else {
                ret = reg_node(pRExC_state, SANY);
                MARK_NAUGHTY(1);
            }
            goto not_anyof;
        }

        /* Similarly, for /l posix classes, if both a class and its
         * complement match, any run-time dependencies don't matter */
        if (posixl) {
            for (namedclass = 0; namedclass < ANYOF_POSIXL_MAX;
                                                        namedclass += 2)
            {
                if (   POSIXL_TEST(posixl, namedclass)      /* class */
                    && POSIXL_TEST(posixl, namedclass + 1)) /* its complement */
                {
                    if (invert) {
                        ret = reganode(pRExC_state, OPFAIL, 0);
                    }
                    else {
                        ret = reg_node(pRExC_state, SANY);
                        MARK_NAUGHTY(1);
                    }
                    goto not_anyof;
                }
            }
            /* For well-behaved locales, some classes are subsets of others,
             * so complementing the subset and including the non-complemented
             * superset should match everything, like [\D[:alnum:]], and
             * [[:^alpha:][:alnum:]], but some implementations of locales are
             * buggy, and khw thinks its a bad idea to have optimization change
             * behavior, even if it avoids an OS bug in a given case */

#define isSINGLE_BIT_SET(n) isPOWER_OF_2(n)

            /* If is a single posix /l class, can optimize to just that op.
             * Such a node will not match anything in the Latin1 range, as that
             * is not determinable until runtime, but will match whatever the
             * class does outside that range.  (Note that some classes won't
             * match anything outside the range, like [:ascii:]) */
            if (    isSINGLE_BIT_SET(posixl)
                && (partial_cp_count == 0 || start[0] > 255))
            {
                U8 classnum;
                SV * class_above_latin1 = NULL;
                bool already_inverted;
                bool are_equivalent;

                /* Compute which bit is set, which is the same thing as, e.g.,
                 * ANYOF_CNTRL.  From
                 * https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
                 * */
                static const int MultiplyDeBruijnBitPosition2[32] =
                    {
                    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
                    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
                    };

                namedclass = MultiplyDeBruijnBitPosition2[(posixl
                                                          * 0x077CB531U) >> 27];
                classnum = namedclass_to_classnum(namedclass);

                /* The named classes are such that the inverted number is one
                 * larger than the non-inverted one */
                already_inverted = namedclass
                                 - classnum_to_namedclass(classnum);

                /* Create an inversion list of the official property, inverted
                 * if the constructed node list is inverted, and restricted to
                 * only the above latin1 code points, which are the only ones
                 * known at compile time */
                _invlist_intersection_maybe_complement_2nd(
                                                    PL_AboveLatin1,
                                                    PL_XPosix_ptrs[classnum],
                                                    already_inverted,
                                                    &class_above_latin1);
                are_equivalent = _invlistEQ(class_above_latin1, cp_list,
                                                                        FALSE);
                SvREFCNT_dec_NN(class_above_latin1);

                if (are_equivalent) {

                    /* Resolve the run-time inversion flag with this possibly
                     * inverted class */
                    invert = invert ^ already_inverted;

                    ret = reg_node(pRExC_state,
                                   POSIXL + invert * (NPOSIXL - POSIXL));
                    FLAGS(REGNODE_p(ret)) = classnum;
                    goto not_anyof;
                }
            }
        }

        /* khw can't think of any other possible transformation involving
         * these. */
        if (has_runtime_dependency & HAS_USER_DEFINED_PROPERTY) {
            goto is_anyof;
        }

        if (! has_runtime_dependency) {

            /* If the list is empty, nothing matches.  This happens, for
             * example, when a Unicode property that doesn't match anything is
             * the only element in the character class (perluniprops.pod notes
             * such properties). */
            if (partial_cp_count == 0) {
                if (invert) {
                    ret = reg_node(pRExC_state, SANY);
                }
                else {
                    ret = reganode(pRExC_state, OPFAIL, 0);
                }

                goto not_anyof;
            }

            /* If matches everything but \n */
            if (   start[0] == 0 && end[0] == '\n' - 1
                && start[1] == '\n' + 1 && end[1] == UV_MAX)
            {
                assert (! invert);
                ret = reg_node(pRExC_state, REG_ANY);
                MARK_NAUGHTY(1);
                goto not_anyof;
            }
        }

        /* Next see if can optimize classes that contain just a few code points
         * into an EXACTish node.  The reason to do this is to let the
         * optimizer join this node with adjacent EXACTish ones.
         *
         * An EXACTFish node can be generated even if not under /i, and vice
         * versa.  But care must be taken.  An EXACTFish node has to be such
         * that it only matches precisely the code points in the class, but we
         * want to generate the least restrictive one that does that, to
         * increase the odds of being able to join with an adjacent node.  For
         * example, if the class contains [kK], we have to make it an EXACTFAA
         * node to prevent the KELVIN SIGN from matching.  Whether we are under
         * /i or not is irrelevant in this case.  Less obvious is the pattern
         * qr/[\x{02BC}]n/i.  U+02BC is MODIFIER LETTER APOSTROPHE. That is
         * supposed to match the single character U+0149 LATIN SMALL LETTER N
         * PRECEDED BY APOSTROPHE.  And so even though there is no simple fold
         * that includes \X{02BC}, there is a multi-char fold that does, and so
         * the node generated for it must be an EXACTFish one.  On the other
         * hand qr/:/i should generate a plain EXACT node since the colon
         * participates in no fold whatsoever, and having it EXACT tells the
         * optimizer the target string cannot match unless it has a colon in
         * it.
         *
         * We don't typically generate an EXACTish node if doing so would
         * require changing the pattern to UTF-8, as that affects /d and
         * otherwise is slower.  However, under /i, not changing to UTF-8 can
         * miss some potential multi-character folds.  We calculate the
         * EXACTish node, and then decide if something would be missed if we
         * don't upgrade */
        if (   ! posixl
            && ! invert

                /* Only try if there are no more code points in the class than
                 * in the max possible fold */
            &&   partial_cp_count > 0 && partial_cp_count <= MAX_FOLD_FROMS + 1

            && (start[0] < 256 || UTF || FOLD))
        {
            if (partial_cp_count == 1 && ! upper_latin1_only_utf8_matches)
            {
                /* We can always make a single code point class into an
                 * EXACTish node. */

                if (LOC) {

                    /* Here is /l:  Use EXACTL, except /li indicates EXACTFL,
                     * as that means there is a fold not known until runtime so
                     * shows as only a single code point here. */
                    op = (FOLD) ? EXACTFL : EXACTL;
                }
                else if (! FOLD) { /* Not /l and not /i */
                    op = (start[0] < 256) ? EXACT : EXACT_ONLY8;
                }
                else if (start[0] < 256) { /* /i, not /l, and the code point is
                                              small */

                    /* Under /i, it gets a little tricky.  A code point that
                     * doesn't participate in a fold should be an EXACT node.
                     * We know this one isn't the result of a simple fold, or
                     * there'd be more than one code point in the list, but it
                     * could be part of a multi- character fold.  In that case
                     * we better not create an EXACT node, as we would wrongly
                     * be telling the optimizer that this code point must be in
                     * the target string, and that is wrong.  This is because
                     * if the sequence around this code point forms a
                     * multi-char fold, what needs to be in the string could be
                     * the code point that folds to the sequence.
                     *
                     * This handles the case of below-255 code points, as we
                     * have an easy look up for those.  The next clause handles
                     * the above-256 one */
                    op = IS_IN_SOME_FOLD_L1(start[0])
                         ? EXACTFU
                         : EXACT;
                }
                else {  /* /i, larger code point.  Since we are under /i, and
                           have just this code point, we know that it can't
                           fold to something else, so PL_InMultiCharFold
                           applies to it */
                    op = _invlist_contains_cp(PL_InMultiCharFold,
                                              start[0])
                         ? EXACTFU_ONLY8
                         : EXACT_ONLY8;
                }

                value = start[0];
            }
            else if (  ! (has_runtime_dependency & ~HAS_D_RUNTIME_DEPENDENCY)
                     && _invlist_contains_cp(PL_in_some_fold, start[0]))
            {
                /* Here, the only runtime dependency, if any, is from /d, and
                 * the class matches more than one code point, and the lowest
                 * code point participates in some fold.  It might be that the
                 * other code points are /i equivalent to this one, and hence
                 * they would representable by an EXACTFish node.  Above, we
                 * eliminated classes that contain too many code points to be
                 * EXACTFish, with the test for MAX_FOLD_FROMS
                 *
                 * First, special case the ASCII fold pairs, like 'B' and 'b'.
                 * We do this because we have EXACTFAA at our disposal for the
                 * ASCII range */
                if (partial_cp_count == 2 && isASCII(start[0])) {

                    /* The only ASCII characters that participate in folds are
                     * alphabetics */
                    assert(isALPHA(start[0]));
                    if (   end[0] == start[0]   /* First range is a single
                                                   character, so 2nd exists */
                        && isALPHA_FOLD_EQ(start[0], start[1]))
                    {

                        /* Here, is part of an ASCII fold pair */

                        if (   ASCII_FOLD_RESTRICTED
                            || HAS_NONLATIN1_SIMPLE_FOLD_CLOSURE(start[0]))
                        {
                            /* If the second clause just above was true, it
                             * means we can't be under /i, or else the list
                             * would have included more than this fold pair.
                             * Therefore we have to exclude the possibility of
                             * whatever else it is that folds to these, by
                             * using EXACTFAA */
                            op = EXACTFAA;
                        }
                        else if (HAS_NONLATIN1_FOLD_CLOSURE(start[0])) {

                            /* Here, there's no simple fold that start[0] is part
                             * of, but there is a multi-character one.  If we
                             * are not under /i, we want to exclude that
                             * possibility; if under /i, we want to include it
                             * */
                            op = (FOLD) ? EXACTFU : EXACTFAA;
                        }
                        else {

                            /* Here, the only possible fold start[0] particpates in
                             * is with start[1].  /i or not isn't relevant */
                            op = EXACTFU;
                        }

                        value = toFOLD(start[0]);
                    }
                }
                else if (  ! upper_latin1_only_utf8_matches
                         || (   _invlist_len(upper_latin1_only_utf8_matches)
                                                                          == 2
                             && PL_fold_latin1[
                               invlist_highest(upper_latin1_only_utf8_matches)]
                             == start[0]))
                {
                    /* Here, the smallest character is non-ascii or there are
                     * more than 2 code points matched by this node.  Also, we
                     * either don't have /d UTF-8 dependent matches, or if we
                     * do, they look like they could be a single character that
                     * is the fold of the lowest one in the always-match list.
                     * This test quickly excludes most of the false positives
                     * when there are /d UTF-8 depdendent matches.  These are
                     * like LATIN CAPITAL LETTER A WITH GRAVE matching LATIN
                     * SMALL LETTER A WITH GRAVE iff the target string is
                     * UTF-8.  (We don't have to worry above about exceeding
                     * the array bounds of PL_fold_latin1[] because any code
                     * point in 'upper_latin1_only_utf8_matches' is below 256.)
                     *
                     * EXACTFAA would apply only to pairs (hence exactly 2 code
                     * points) in the ASCII range, so we can't use it here to
                     * artificially restrict the fold domain, so we check if
                     * the class does or does not match some EXACTFish node.
                     * Further, if we aren't under /i, and and the folded-to
                     * character is part of a multi-character fold, we can't do
                     * this optimization, as the sequence around it could be
                     * that multi-character fold, and we don't here know the
                     * context, so we have to assume it is that multi-char
                     * fold, to prevent potential bugs.
                     *
                     * To do the general case, we first find the fold of the
                     * lowest code point (which may be higher than the lowest
                     * one), then find everything that folds to it.  (The data
                     * structure we have only maps from the folded code points,
                     * so we have to do the earlier step.) */

                    Size_t foldlen;
                    U8 foldbuf[UTF8_MAXBYTES_CASE];
                    UV folded = _to_uni_fold_flags(start[0],
                                                        foldbuf, &foldlen, 0);
                    unsigned int first_fold;
                    const unsigned int * remaining_folds;
                    Size_t folds_to_this_cp_count = _inverse_folds(
                                                            folded,
                                                            &first_fold,
                                                            &remaining_folds);
                    Size_t folds_count = folds_to_this_cp_count + 1;
                    SV * fold_list = _new_invlist(folds_count);
                    unsigned int i;

                    /* If there are UTF-8 dependent matches, create a temporary
                     * list of what this node matches, including them. */
                    SV * all_cp_list = NULL;
                    SV ** use_this_list = &cp_list;

                    if (upper_latin1_only_utf8_matches) {
                        all_cp_list = _new_invlist(0);
                        use_this_list = &all_cp_list;
                        _invlist_union(cp_list,
                                       upper_latin1_only_utf8_matches,
                                       use_this_list);
                    }

                    /* Having gotten everything that participates in the fold
                     * containing the lowest code point, we turn that into an
                     * inversion list, making sure everything is included. */
                    fold_list = add_cp_to_invlist(fold_list, start[0]);
                    fold_list = add_cp_to_invlist(fold_list, folded);
                    if (folds_to_this_cp_count > 0) {
                        fold_list = add_cp_to_invlist(fold_list, first_fold);
                        for (i = 0; i + 1 < folds_to_this_cp_count; i++) {
                            fold_list = add_cp_to_invlist(fold_list,
                                                        remaining_folds[i]);
                        }
                    }

                    /* If the fold list is identical to what's in this ANYOF
                     * node, the node can be represented by an EXACTFish one
                     * instead */
                    if (_invlistEQ(*use_this_list, fold_list,
                                   0 /* Don't complement */ )
                    ) {

                        /* But, we have to be careful, as mentioned above.
                         * Just the right sequence of characters could match
                         * this if it is part of a multi-character fold.  That
                         * IS what we want if we are under /i.  But it ISN'T
                         * what we want if not under /i, as it could match when
                         * it shouldn't.  So, when we aren't under /i and this
                         * character participates in a multi-char fold, we
                         * don't optimize into an EXACTFish node.  So, for each
                         * case below we have to check if we are folding
                         * and if not, if it is not part of a multi-char fold.
                         * */
                        if (start[0] > 255) {    /* Highish code point */
                            if (FOLD || ! _invlist_contains_cp(
                                            PL_InMultiCharFold, folded))
                            {
                                op = (LOC)
                                     ? EXACTFLU8
                                     : (ASCII_FOLD_RESTRICTED)
                                       ? EXACTFAA
                                       : EXACTFU_ONLY8;
                                value = folded;
                            }
                        }   /* Below, the lowest code point < 256 */
                        else if (    FOLD
                                 &&  folded == 's'
                                 &&  DEPENDS_SEMANTICS)
                        {   /* An EXACTF node containing a single character
                                's', can be an EXACTFU if it doesn't get
                                joined with an adjacent 's' */
                            op = EXACTFU_S_EDGE;
                            value = folded;
                        }
                        else if (    FOLD
                                || ! HAS_NONLATIN1_FOLD_CLOSURE(start[0]))
                        {
                            if (upper_latin1_only_utf8_matches) {
                                op = EXACTF;

                                /* We can't use the fold, as that only matches
                                 * under UTF-8 */
                                value = start[0];
                            }
                            else if (     UNLIKELY(start[0] == MICRO_SIGN)
                                     && ! UTF)
                            {   /* EXACTFUP is a special node for this
                                   character */
                                op = (ASCII_FOLD_RESTRICTED)
                                     ? EXACTFAA
                                     : EXACTFUP;
                                value = MICRO_SIGN;
                            }
                            else if (     ASCII_FOLD_RESTRICTED
                                     && ! isASCII(start[0]))
                            {   /* For ASCII under /iaa, we can use EXACTFU
                                   below */
                                op = EXACTFAA;
                                value = folded;
                            }
                            else {
                                op = EXACTFU;
                                value = folded;
                            }
                        }
                    }

                    SvREFCNT_dec_NN(fold_list);
                    SvREFCNT_dec(all_cp_list);
                }
            }

            if (op != END) {

                /* Here, we have calculated what EXACTish node we would use.
                 * But we don't use it if it would require converting the
                 * pattern to UTF-8, unless not using it could cause us to miss
                 * some folds (hence be buggy) */

                if (! UTF && value > 255) {
                    SV * in_multis = NULL;

                    assert(FOLD);

                    /* If there is no code point that is part of a multi-char
                     * fold, then there aren't any matches, so we don't do this
                     * optimization.  Otherwise, it could match depending on
                     * the context around us, so we do upgrade */
                    _invlist_intersection(PL_InMultiCharFold, cp_list, &in_multis);
                    if (UNLIKELY(_invlist_len(in_multis) != 0)) {
                        REQUIRE_UTF8(flagp);
                    }
                    else {
                        op = END;
                    }
                }

                if (op != END) {
                    U8 len = (UTF) ? UVCHR_SKIP(value) : 1;

                    ret = regnode_guts(pRExC_state, op, len, "exact");
                    FILL_NODE(ret, op);
                    RExC_emit += 1 + STR_SZ(len);
                    STR_LEN(REGNODE_p(ret)) = len;
                    if (len == 1) {
                        *STRING(REGNODE_p(ret)) = (U8) value;
                    }
                    else {
                        uvchr_to_utf8((U8 *) STRING(REGNODE_p(ret)), value);
                    }
                    goto not_anyof;
                }
            }
        }

        if (! has_runtime_dependency) {

            /* See if this can be turned into an ANYOFM node.  Think about the
             * bit patterns in two different bytes.  In some positions, the
             * bits in each will be 1; and in other positions both will be 0;
             * and in some positions the bit will be 1 in one byte, and 0 in
             * the other.  Let 'n' be the number of positions where the bits
             * differ.  We create a mask which has exactly 'n' 0 bits, each in
             * a position where the two bytes differ.  Now take the set of all
             * bytes that when ANDed with the mask yield the same result.  That
             * set has 2**n elements, and is representable by just two 8 bit
             * numbers: the result and the mask.  Importantly, matching the set
             * can be vectorized by creating a word full of the result bytes,
             * and a word full of the mask bytes, yielding a significant speed
             * up.  Here, see if this node matches such a set.  As a concrete
             * example consider [01], and the byte representing '0' which is
             * 0x30 on ASCII machines.  It has the bits 0011 0000.  Take the
             * mask 1111 1110.  If we AND 0x31 and 0x30 with that mask we get
             * 0x30.  Any other bytes ANDed yield something else.  So [01],
             * which is a common usage, is optimizable into ANYOFM, and can
             * benefit from the speed up.  We can only do this on UTF-8
             * invariant bytes, because they have the same bit patterns under
             * UTF-8 as not. */
            PERL_UINT_FAST8_T inverted = 0;
#ifdef EBCDIC
            const PERL_UINT_FAST8_T max_permissible = 0xFF;
#else
            const PERL_UINT_FAST8_T max_permissible = 0x7F;
#endif
            /* If doesn't fit the criteria for ANYOFM, invert and try again.
             * If that works we will instead later generate an NANYOFM, and
             * invert back when through */
            if (invlist_highest(cp_list) > max_permissible) {
                _invlist_invert(cp_list);
                inverted = 1;
            }

            if (invlist_highest(cp_list) <= max_permissible) {
                UV this_start, this_end;
                UV lowest_cp = UV_MAX;  /* inited to suppress compiler warn */
                U8 bits_differing = 0;
                Size_t full_cp_count = 0;
                bool first_time = TRUE;

                /* Go through the bytes and find the bit positions that differ
                 * */
                invlist_iterinit(cp_list);
                while (invlist_iternext(cp_list, &this_start, &this_end)) {
                    unsigned int i = this_start;

                    if (first_time) {
                        if (! UVCHR_IS_INVARIANT(i)) {
                            goto done_anyofm;
                        }

                        first_time = FALSE;
                        lowest_cp = this_start;

                        /* We have set up the code point to compare with.
                         * Don't compare it with itself */
                        i++;
                    }

                    /* Find the bit positions that differ from the lowest code
                     * point in the node.  Keep track of all such positions by
                     * OR'ing */
                    for (; i <= this_end; i++) {
                        if (! UVCHR_IS_INVARIANT(i)) {
                            goto done_anyofm;
                        }

                        bits_differing  |= i ^ lowest_cp;
                    }

                    full_cp_count += this_end - this_start + 1;
                }
                invlist_iterfinish(cp_list);

                /* At the end of the loop, we count how many bits differ from
                 * the bits in lowest code point, call the count 'd'.  If the
                 * set we found contains 2**d elements, it is the closure of
                 * all code points that differ only in those bit positions.  To
                 * convince yourself of that, first note that the number in the
                 * closure must be a power of 2, which we test for.  The only
                 * way we could have that count and it be some differing set,
                 * is if we got some code points that don't differ from the
                 * lowest code point in any position, but do differ from each
                 * other in some other position.  That means one code point has
                 * a 1 in that position, and another has a 0.  But that would
                 * mean that one of them differs from the lowest code point in
                 * that position, which possibility we've already excluded.  */
                if (  (inverted || full_cp_count > 1)
                    && full_cp_count == 1U << PL_bitcount[bits_differing])
                {
                    U8 ANYOFM_mask;

                    op = ANYOFM + inverted;;

                    /* We need to make the bits that differ be 0's */
                    ANYOFM_mask = ~ bits_differing; /* This goes into FLAGS */

                    /* The argument is the lowest code point */
                    ret = reganode(pRExC_state, op, lowest_cp);
                    FLAGS(REGNODE_p(ret)) = ANYOFM_mask;
                }
            }
          done_anyofm:

            if (inverted) {
                _invlist_invert(cp_list);
            }

            if (op != END) {
                goto not_anyof;
            }
        }

        if (! (anyof_flags & ANYOF_LOCALE_FLAGS)) {
            PERL_UINT_FAST8_T type;
            SV * intersection = NULL;
            SV* d_invlist = NULL;

            /* See if this matches any of the POSIX classes.  The POSIXA and
             * POSIXD ones are about the same speed as ANYOF ops, but take less
             * room; the ones that have above-Latin1 code point matches are
             * somewhat faster than ANYOF.  */

            for (type = POSIXA; type >= POSIXD; type--) {
                int posix_class;

                if (type == POSIXL) {   /* But not /l posix classes */
                    continue;
                }

                for (posix_class = 0;
                     posix_class <= _HIGHEST_REGCOMP_DOT_H_SYNC;
                     posix_class++)
                {
                    SV** our_code_points = &cp_list;
                    SV** official_code_points;
                    int try_inverted;

                    if (type == POSIXA) {
                        official_code_points = &PL_Posix_ptrs[posix_class];
                    }
                    else {
                        official_code_points = &PL_XPosix_ptrs[posix_class];
                    }

                    /* Skip non-existent classes of this type.  e.g. \v only
                     * has an entry in PL_XPosix_ptrs */
                    if (! *official_code_points) {
                        continue;
                    }

                    /* Try both the regular class, and its inversion */
                    for (try_inverted = 0; try_inverted < 2; try_inverted++) {
                        bool this_inverted = invert ^ try_inverted;

                        if (type != POSIXD) {

                            /* This class that isn't /d can't match if we have
                             * /d dependencies */
                            if (has_runtime_dependency
                                                    & HAS_D_RUNTIME_DEPENDENCY)
                            {
                                continue;
                            }
                        }
                        else /* is /d */ if (! this_inverted) {

                            /* /d classes don't match anything non-ASCII below
                             * 256 unconditionally (which cp_list contains) */
                            _invlist_intersection(cp_list, PL_UpperLatin1,
                                                           &intersection);
                            if (_invlist_len(intersection) != 0) {
                                continue;
                            }

                            SvREFCNT_dec(d_invlist);
                            d_invlist = invlist_clone(cp_list, NULL);

                            /* But under UTF-8 it turns into using /u rules.
                             * Add the things it matches under these conditions
                             * so that we check below that these are identical
                             * to what the tested class should match */
                            if (upper_latin1_only_utf8_matches) {
                                _invlist_union(
                                            d_invlist,
                                            upper_latin1_only_utf8_matches,
                                            &d_invlist);
                            }
                            our_code_points = &d_invlist;
                        }
                        else {  /* POSIXD, inverted.  If this doesn't have this
                                   flag set, it isn't /d. */
                            if (! (anyof_flags & ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER))
                            {
                                continue;
                            }
                            our_code_points = &cp_list;
                        }

                        /* Here, have weeded out some things.  We want to see
                         * if the list of characters this node contains
                         * ('*our_code_points') precisely matches those of the
                         * class we are currently checking against
                         * ('*official_code_points'). */
                        if (_invlistEQ(*our_code_points,
                                       *official_code_points,
                                       try_inverted))
                        {
                            /* Here, they precisely match.  Optimize this ANYOF
                             * node into its equivalent POSIX one of the
                             * correct type, possibly inverted */
                            ret = reg_node(pRExC_state, (try_inverted)
                                                        ? type + NPOSIXA
                                                                - POSIXA
                                                        : type);
                            FLAGS(REGNODE_p(ret)) = posix_class;
                            SvREFCNT_dec(d_invlist);
                            SvREFCNT_dec(intersection);
                            goto not_anyof;
                        }
                    }
                }
            }
            SvREFCNT_dec(d_invlist);
            SvREFCNT_dec(intersection);
        }

        /* If didn't find an optimization and there is no need for a
        * bitmap, optimize to indicate that */
        if (     start[0] >= NUM_ANYOF_CODE_POINTS
            && ! LOC
            && ! upper_latin1_only_utf8_matches
            &&   anyof_flags == 0)
        {
            UV highest_cp = invlist_highest(cp_list);

            /* If the lowest and highest code point in the class have the same
             * UTF-8 first byte, then all do, and we can store that byte for
             * regexec.c to use so that it can more quickly scan the target
             * string for potential matches for this class.  We co-opt the the
             * flags field for this.  Zero means, they don't have the same
             * first byte.  We do accept here very large code points (for
             * future use), but don't bother with this optimization for them,
             * as it would cause other complications */
            if (highest_cp > IV_MAX) {
                anyof_flags = 0;
            }
            else {
                U8 low_utf8[UTF8_MAXBYTES+1];
                U8 high_utf8[UTF8_MAXBYTES+1];

                (void) uvchr_to_utf8(low_utf8, start[0]);
                (void) uvchr_to_utf8(high_utf8, invlist_highest(cp_list));

                anyof_flags = (low_utf8[0] == high_utf8[0])
                            ? low_utf8[0]
                            : 0;
            }

            op = ANYOFH;
        }
    }   /* End of seeing if can optimize it into a different node */

  is_anyof: /* It's going to be an ANYOF node. */
    if (op != ANYOFH) {
        op = (has_runtime_dependency & HAS_D_RUNTIME_DEPENDENCY)
             ? ANYOFD
             : ((posixl)
                ? ANYOFPOSIXL
                : ((LOC)
                   ? ANYOFL
                   : ANYOF));
    }

    ret = regnode_guts(pRExC_state, op, regarglen[op], "anyof");
    FILL_NODE(ret, op);        /* We set the argument later */
    RExC_emit += 1 + regarglen[op];
    ANYOF_FLAGS(REGNODE_p(ret)) = anyof_flags;

    /* Here, <cp_list> contains all the code points we can determine at
     * compile time that match under all conditions.  Go through it, and
     * for things that belong in the bitmap, put them there, and delete from
     * <cp_list>.  While we are at it, see if everything above 255 is in the
     * list, and if so, set a flag to speed up execution */

    populate_ANYOF_from_invlist(REGNODE_p(ret), &cp_list);

    if (posixl) {
        ANYOF_POSIXL_SET_TO_BITMAP(REGNODE_p(ret), posixl);
    }

    if (invert) {
        ANYOF_FLAGS(REGNODE_p(ret)) |= ANYOF_INVERT;
    }

    /* Here, the bitmap has been populated with all the Latin1 code points that
     * always match.  Can now add to the overall list those that match only
     * when the target string is UTF-8 (<upper_latin1_only_utf8_matches>).
     * */
    if (upper_latin1_only_utf8_matches) {
	if (cp_list) {
	    _invlist_union(cp_list,
                           upper_latin1_only_utf8_matches,
                           &cp_list);
	    SvREFCNT_dec_NN(upper_latin1_only_utf8_matches);
	}
	else {
	    cp_list = upper_latin1_only_utf8_matches;
	}
        ANYOF_FLAGS(REGNODE_p(ret)) |= ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP;
    }

    set_ANYOF_arg(pRExC_state, REGNODE_p(ret), cp_list,
                  (HAS_NONLOCALE_RUNTIME_PROPERTY_DEFINITION)
                   ? listsv : NULL,
                  only_utf8_locale_list);
    return ret;

  not_anyof:

    /* Here, the node is getting optimized into something that's not an ANYOF
     * one.  Finish up. */

    Set_Node_Offset_Length(REGNODE_p(ret), orig_parse - RExC_start,
                                           RExC_parse - orig_parse);;
    SvREFCNT_dec(cp_list);;
    return ret;
}

#undef HAS_NONLOCALE_RUNTIME_PROPERTY_DEFINITION

STATIC void
S_set_ANYOF_arg(pTHX_ RExC_state_t* const pRExC_state,
                regnode* const node,
                SV* const cp_list,
                SV* const runtime_defns,
                SV* const only_utf8_locale_list)
{
    /* Sets the arg field of an ANYOF-type node 'node', using information about
     * the node passed-in.  If there is nothing outside the node's bitmap, the
     * arg is set to ANYOF_ONLY_HAS_BITMAP.  Otherwise, it sets the argument to
     * the count returned by add_data(), having allocated and stored an array,
     * av, as follows:
     *
     *  av[0] stores the inversion list defining this class as far as known at
     *        this time, or PL_sv_undef if nothing definite is now known.
     *  av[1] stores the inversion list of code points that match only if the
     *        current locale is UTF-8, or if none, PL_sv_undef if there is an
     *        av[2], or no entry otherwise.
     *  av[2] stores the list of user-defined properties whose subroutine
     *        definitions aren't known at this time, or no entry if none. */

    UV n;

    PERL_ARGS_ASSERT_SET_ANYOF_ARG;

    if (! cp_list && ! runtime_defns && ! only_utf8_locale_list) {
        assert(! (ANYOF_FLAGS(node)
                & ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP));
	ARG_SET(node, ANYOF_ONLY_HAS_BITMAP);
    }
    else {
	AV * const av = newAV();
	SV *rv;

        if (cp_list) {
            av_store(av, INVLIST_INDEX, cp_list);
        }

        if (only_utf8_locale_list) {
            av_store(av, ONLY_LOCALE_MATCHES_INDEX, only_utf8_locale_list);
        }

        if (runtime_defns) {
            av_store(av, DEFERRED_USER_DEFINED_INDEX, SvREFCNT_inc(runtime_defns));
        }

	rv = newRV_noinc(MUTABLE_SV(av));
	n = add_data(pRExC_state, STR_WITH_LEN("s"));
	RExC_rxi->data->data[n] = (void*)rv;
	ARG_SET(node, n);
    }
}

#if !defined(PERL_IN_XSUB_RE) || defined(PLUGGABLE_RE_EXTENSION)
SV *
Perl__get_regclass_nonbitmap_data(pTHX_ const regexp *prog,
                                        const regnode* node,
                                        bool doinit,
                                        SV** listsvp,
                                        SV** only_utf8_locale_ptr,
                                        SV** output_invlist)

{
    /* For internal core use only.
     * Returns the inversion list for the input 'node' in the regex 'prog'.
     * If <doinit> is 'true', will attempt to create the inversion list if not
     *    already done.
     * If <listsvp> is non-null, will return the printable contents of the
     *    property definition.  This can be used to get debugging information
     *    even before the inversion list exists, by calling this function with
     *    'doinit' set to false, in which case the components that will be used
     *    to eventually create the inversion list are returned  (in a printable
     *    form).
     * If <only_utf8_locale_ptr> is not NULL, it is where this routine is to
     *    store an inversion list of code points that should match only if the
     *    execution-time locale is a UTF-8 one.
     * If <output_invlist> is not NULL, it is where this routine is to store an
     *    inversion list of the code points that would be instead returned in
     *    <listsvp> if this were NULL.  Thus, what gets output in <listsvp>
     *    when this parameter is used, is just the non-code point data that
     *    will go into creating the inversion list.  This currently should be just
     *    user-defined properties whose definitions were not known at compile
     *    time.  Using this parameter allows for easier manipulation of the
     *    inversion list's data by the caller.  It is illegal to call this
     *    function with this parameter set, but not <listsvp>
     *
     * Tied intimately to how S_set_ANYOF_arg sets up the data structure.  Note
     * that, in spite of this function's name, the inversion list it returns
     * may include the bitmap data as well */

    SV *si  = NULL;         /* Input initialization string */
    SV* invlist = NULL;

    RXi_GET_DECL(prog, progi);
    const struct reg_data * const data = prog ? progi->data : NULL;

    PERL_ARGS_ASSERT__GET_REGCLASS_NONBITMAP_DATA;
    assert(! output_invlist || listsvp);

    if (data && data->count) {
	const U32 n = ARG(node);

	if (data->what[n] == 's') {
	    SV * const rv = MUTABLE_SV(data->data[n]);
	    AV * const av = MUTABLE_AV(SvRV(rv));
	    SV **const ary = AvARRAY(av);

            invlist = ary[INVLIST_INDEX];

            if (av_tindex_skip_len_mg(av) >= ONLY_LOCALE_MATCHES_INDEX) {
                *only_utf8_locale_ptr = ary[ONLY_LOCALE_MATCHES_INDEX];
            }

            if (av_tindex_skip_len_mg(av) >= DEFERRED_USER_DEFINED_INDEX) {
                si = ary[DEFERRED_USER_DEFINED_INDEX];
            }

	    if (doinit && (si || invlist)) {
                if (si) {
                    bool user_defined;
                    SV * msg = newSVpvs_flags("", SVs_TEMP);

                    SV * prop_definition = handle_user_defined_property(
                            "", 0, FALSE,   /* There is no \p{}, \P{} */
                            SvPVX_const(si)[1] - '0',   /* /i or not has been
                                                           stored here for just
                                                           this occasion */
                            TRUE,           /* run time */
                            FALSE,          /* This call must find the defn */
                            si,             /* The property definition  */
                            &user_defined,
                            msg,
                            0               /* base level call */
                           );

                    if (SvCUR(msg)) {
                        assert(prop_definition == NULL);

                        Perl_croak(aTHX_ "%" UTF8f,
                                UTF8fARG(SvUTF8(msg), SvCUR(msg), SvPVX(msg)));
                    }

                    if (invlist) {
                        _invlist_union(invlist, prop_definition, &invlist);
                        SvREFCNT_dec_NN(prop_definition);
                    }
                    else {
                        invlist = prop_definition;
                    }

                    STATIC_ASSERT_STMT(ONLY_LOCALE_MATCHES_INDEX == 1 + INVLIST_INDEX);
                    STATIC_ASSERT_STMT(DEFERRED_USER_DEFINED_INDEX == 1 + ONLY_LOCALE_MATCHES_INDEX);

                    av_store(av, INVLIST_INDEX, invlist);
                    av_fill(av, (ary[ONLY_LOCALE_MATCHES_INDEX])
                                 ? ONLY_LOCALE_MATCHES_INDEX:
                                 INVLIST_INDEX);
                    si = NULL;
                }
	    }
	}
    }

    /* If requested, return a printable version of what this ANYOF node matches
     * */
    if (listsvp) {
	SV* matches_string = NULL;

        /* This function can be called at compile-time, before everything gets
         * resolved, in which case we return the currently best available
         * information, which is the string that will eventually be used to do
         * that resolving, 'si' */
	if (si) {
            /* Here, we only have 'si' (and possibly some passed-in data in
             * 'invlist', which is handled below)  If the caller only wants
             * 'si', use that.  */
            if (! output_invlist) {
                matches_string = newSVsv(si);
            }
            else {
                /* But if the caller wants an inversion list of the node, we
                 * need to parse 'si' and place as much as possible in the
                 * desired output inversion list, making 'matches_string' only
                 * contain the currently unresolvable things */
                const char *si_string = SvPVX(si);
                STRLEN remaining = SvCUR(si);
                UV prev_cp = 0;
                U8 count = 0;

                /* Ignore everything before the first new-line */
                while (*si_string != '\n' && remaining > 0) {
                    si_string++;
                    remaining--;
                }
                assert(remaining > 0);

                si_string++;
                remaining--;

                while (remaining > 0) {

                    /* The data consists of just strings defining user-defined
                     * property names, but in prior incarnations, and perhaps
                     * somehow from pluggable regex engines, it could still
                     * hold hex code point definitions.  Each component of a
                     * range would be separated by a tab, and each range by a
                     * new-line.  If these are found, instead add them to the
                     * inversion list */
                    I32 grok_flags =  PERL_SCAN_SILENT_ILLDIGIT
                                     |PERL_SCAN_SILENT_NON_PORTABLE;
                    STRLEN len = remaining;
                    UV cp = grok_hex(si_string, &len, &grok_flags, NULL);

                    /* If the hex decode routine found something, it should go
                     * up to the next \n */
                    if (   *(si_string + len) == '\n') {
                        if (count) {    /* 2nd code point on line */
                            *output_invlist = _add_range_to_invlist(*output_invlist, prev_cp, cp);
                        }
                        else {
                            *output_invlist = add_cp_to_invlist(*output_invlist, cp);
                        }
                        count = 0;
                        goto prepare_for_next_iteration;
                    }

                    /* If the hex decode was instead for the lower range limit,
                     * save it, and go parse the upper range limit */
                    if (*(si_string + len) == '\t') {
                        assert(count == 0);

                        prev_cp = cp;
                        count = 1;
                      prepare_for_next_iteration:
                        si_string += len + 1;
                        remaining -= len + 1;
                        continue;
                    }

                    /* Here, didn't find a legal hex number.  Just add it from
                     * here to the next \n */

                    remaining -= len;
                    while (*(si_string + len) != '\n' && remaining > 0) {
                        remaining--;
                        len++;
                    }
                    if (*(si_string + len) == '\n') {
                        len++;
                        remaining--;
                    }
                    if (matches_string) {
                        sv_catpvn(matches_string, si_string, len - 1);
                    }
                    else {
                        matches_string = newSVpvn(si_string, len - 1);
                    }
                    si_string += len;
                    sv_catpvs(matches_string, " ");
                } /* end of loop through the text */

                assert(matches_string);
                if (SvCUR(matches_string)) {  /* Get rid of trailing blank */
                    SvCUR_set(matches_string, SvCUR(matches_string) - 1);
                }
            } /* end of has an 'si' */
	}

        /* Add the stuff that's already known */
        if (invlist) {

            /* Again, if the caller doesn't want the output inversion list, put
             * everything in 'matches-string' */
            if (! output_invlist) {
                if ( ! matches_string) {
                    matches_string = newSVpvs("\n");
                }
                sv_catsv(matches_string, invlist_contents(invlist,
                                                  TRUE /* traditional style */
                                                  ));
            }
            else if (! *output_invlist) {
                *output_invlist = invlist_clone(invlist, NULL);
            }
            else {
                _invlist_union(*output_invlist, invlist, output_invlist);
            }
        }

	*listsvp = matches_string;
    }

    return invlist;
}
#endif /* !defined(PERL_IN_XSUB_RE) || defined(PLUGGABLE_RE_EXTENSION) */

/* reg_skipcomment()

   Absorbs an /x style # comment from the input stream,
   returning a pointer to the first character beyond the comment, or if the
   comment terminates the pattern without anything following it, this returns
   one past the final character of the pattern (in other words, RExC_end) and
   sets the REG_RUN_ON_COMMENT_SEEN flag.

   Note it's the callers responsibility to ensure that we are
   actually in /x mode

*/

PERL_STATIC_INLINE char*
S_reg_skipcomment(RExC_state_t *pRExC_state, char* p)
{
    PERL_ARGS_ASSERT_REG_SKIPCOMMENT;

    assert(*p == '#');

    while (p < RExC_end) {
        if (*(++p) == '\n') {
            return p+1;
        }
    }

    /* we ran off the end of the pattern without ending the comment, so we have
     * to add an \n when wrapping */
    RExC_seen |= REG_RUN_ON_COMMENT_SEEN;
    return p;
}

STATIC void
S_skip_to_be_ignored_text(pTHX_ RExC_state_t *pRExC_state,
                                char ** p,
                                const bool force_to_xmod
                         )
{
    /* If the text at the current parse position '*p' is a '(?#...)' comment,
     * or if we are under /x or 'force_to_xmod' is TRUE, and the text at '*p'
     * is /x whitespace, advance '*p' so that on exit it points to the first
     * byte past all such white space and comments */

    const bool use_xmod = force_to_xmod || (RExC_flags & RXf_PMf_EXTENDED);

    PERL_ARGS_ASSERT_SKIP_TO_BE_IGNORED_TEXT;

    assert( ! UTF || UTF8_IS_INVARIANT(**p) || UTF8_IS_START(**p));

    for (;;) {
	if (RExC_end - (*p) >= 3
	    && *(*p)     == '('
	    && *(*p + 1) == '?'
	    && *(*p + 2) == '#')
	{
	    while (*(*p) != ')') {
		if ((*p) == RExC_end)
		    FAIL("Sequence (?#... not terminated");
		(*p)++;
	    }
	    (*p)++;
	    continue;
	}

	if (use_xmod) {
            const char * save_p = *p;
            while ((*p) < RExC_end) {
                STRLEN len;
                if ((len = is_PATWS_safe((*p), RExC_end, UTF))) {
                    (*p) += len;
                }
                else if (*(*p) == '#') {
                    (*p) = reg_skipcomment(pRExC_state, (*p));
                }
                else {
                    break;
                }
            }
            if (*p != save_p) {
                continue;
            }
	}

        break;
    }

    return;
}

/* nextchar()

   Advances the parse position by one byte, unless that byte is the beginning
   of a '(?#...)' style comment, or is /x whitespace and /x is in effect.  In
   those two cases, the parse position is advanced beyond all such comments and
   white space.

   This is the UTF, (?#...), and /x friendly way of saying RExC_parse++.
*/

STATIC void
S_nextchar(pTHX_ RExC_state_t *pRExC_state)
{
    PERL_ARGS_ASSERT_NEXTCHAR;

    if (RExC_parse < RExC_end) {
        assert(   ! UTF
               || UTF8_IS_INVARIANT(*RExC_parse)
               || UTF8_IS_START(*RExC_parse));

        RExC_parse += (UTF)
                      ? UTF8_SAFE_SKIP(RExC_parse, RExC_end)
                      : 1;

        skip_to_be_ignored_text(pRExC_state, &RExC_parse,
                                FALSE /* Don't force /x */ );
    }
}

STATIC void
S_change_engine_size(pTHX_ RExC_state_t *pRExC_state, const Ptrdiff_t size)
{
    /* 'size' is the delta to add or subtract from the current memory allocated
     * to the regex engine being constructed */

    PERL_ARGS_ASSERT_CHANGE_ENGINE_SIZE;

    RExC_size += size;

    Renewc(RExC_rxi,
           sizeof(regexp_internal) + (RExC_size + 1) * sizeof(regnode),
                                                /* +1 for REG_MAGIC */
           char,
           regexp_internal);
    if ( RExC_rxi == NULL )
	FAIL("Regexp out of space");
    RXi_SET(RExC_rx, RExC_rxi);

    RExC_emit_start = RExC_rxi->program;
    if (size > 0) {
        Zero(REGNODE_p(RExC_emit), size, regnode);
    }

#ifdef RE_TRACK_PATTERN_OFFSETS
    Renew(RExC_offsets, 2*RExC_size+1, U32);
    if (size > 0) {
        Zero(RExC_offsets + 2*(RExC_size - size) + 1, 2 * size, U32);
    }
    RExC_offsets[0] = RExC_size;
#endif
}

STATIC regnode_offset
S_regnode_guts(pTHX_ RExC_state_t *pRExC_state, const U8 op, const STRLEN extra_size, const char* const name)
{
    /* Allocate a regnode for 'op', with 'extra_size' extra space.  It aligns
     * and increments RExC_size and RExC_emit
     *
     * It returns the regnode's offset into the regex engine program */

    const regnode_offset ret = RExC_emit;

    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGNODE_GUTS;

    SIZE_ALIGN(RExC_size);
    change_engine_size(pRExC_state, (Ptrdiff_t) 1 + extra_size);
    NODE_ALIGN_FILL(REGNODE_p(ret));
#ifndef RE_TRACK_PATTERN_OFFSETS
    PERL_UNUSED_ARG(name);
    PERL_UNUSED_ARG(op);
#else
    assert(extra_size >= regarglen[op] || PL_regkind[op] == ANYOF);

    if (RExC_offsets) {         /* MJD */
	MJD_OFFSET_DEBUG(
              ("%s:%d: (op %s) %s %" UVuf " (len %" UVuf ") (max %" UVuf ").\n",
              name, __LINE__,
              PL_reg_name[op],
              (UV)(RExC_emit) > RExC_offsets[0]
		? "Overwriting end of array!\n" : "OK",
              (UV)(RExC_emit),
              (UV)(RExC_parse - RExC_start),
              (UV)RExC_offsets[0]));
	Set_Node_Offset(REGNODE_p(RExC_emit), RExC_parse + (op == END));
    }
#endif
    return(ret);
}

/*
- reg_node - emit a node
*/
STATIC regnode_offset /* Location. */
S_reg_node(pTHX_ RExC_state_t *pRExC_state, U8 op)
{
    const regnode_offset ret = regnode_guts(pRExC_state, op, regarglen[op], "reg_node");
    regnode_offset ptr = ret;

    PERL_ARGS_ASSERT_REG_NODE;

    assert(regarglen[op] == 0);

    FILL_ADVANCE_NODE(ptr, op);
    RExC_emit = ptr;
    return(ret);
}

/*
- reganode - emit a node with an argument
*/
STATIC regnode_offset /* Location. */
S_reganode(pTHX_ RExC_state_t *pRExC_state, U8 op, U32 arg)
{
    const regnode_offset ret = regnode_guts(pRExC_state, op, regarglen[op], "reganode");
    regnode_offset ptr = ret;

    PERL_ARGS_ASSERT_REGANODE;

    /* ANYOF are special cased to allow non-length 1 args */
    assert(regarglen[op] == 1);

    FILL_ADVANCE_NODE_ARG(ptr, op, arg);
    RExC_emit = ptr;
    return(ret);
}

STATIC regnode_offset
S_reg2Lanode(pTHX_ RExC_state_t *pRExC_state, const U8 op, const U32 arg1, const I32 arg2)
{
    /* emit a node with U32 and I32 arguments */

    const regnode_offset ret = regnode_guts(pRExC_state, op, regarglen[op], "reg2Lanode");
    regnode_offset ptr = ret;

    PERL_ARGS_ASSERT_REG2LANODE;

    assert(regarglen[op] == 2);

    FILL_ADVANCE_NODE_2L_ARG(ptr, op, arg1, arg2);
    RExC_emit = ptr;
    return(ret);
}

/*
- reginsert - insert an operator in front of already-emitted operand
*
* That means that on exit 'operand' is the offset of the newly inserted
* operator, and the original operand has been relocated.
*
* IMPORTANT NOTE - it is the *callers* responsibility to correctly
* set up NEXT_OFF() of the inserted node if needed. Something like this:
*
*   reginsert(pRExC, OPFAIL, orig_emit, depth+1);
*   NEXT_OFF(orig_emit) = regarglen[OPFAIL] + NODE_STEP_REGNODE;
*
* ALSO NOTE - FLAGS(newly-inserted-operator) will be set to 0 as well.
*/
STATIC void
S_reginsert(pTHX_ RExC_state_t *pRExC_state, const U8 op,
                  const regnode_offset operand, const U32 depth)
{
    regnode *src;
    regnode *dst;
    regnode *place;
    const int offset = regarglen[(U8)op];
    const int size = NODE_STEP_REGNODE + offset;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGINSERT;
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(depth);
/* (PL_regkind[(U8)op] == CURLY ? EXTRA_STEP_2ARGS : 0); */
    DEBUG_PARSE_FMT("inst"," - %s", PL_reg_name[op]);
    assert(!RExC_study_started); /* I believe we should never use reginsert once we have started
                                    studying. If this is wrong then we need to adjust RExC_recurse
                                    below like we do with RExC_open_parens/RExC_close_parens. */
    change_engine_size(pRExC_state, (Ptrdiff_t) size);
    src = REGNODE_p(RExC_emit);
    RExC_emit += size;
    dst = REGNODE_p(RExC_emit);

    /* If we are in a "count the parentheses" pass, the numbers are unreliable,
     * and [perl #133871] shows this can lead to problems, so skip this
     * realignment of parens until a later pass when they are reliable */
    if (! IN_PARENS_PASS && RExC_open_parens) {
        int paren;
        /*DEBUG_PARSE_FMT("inst"," - %" IVdf, (IV)RExC_npar);*/
        /* remember that RExC_npar is rex->nparens + 1,
         * iow it is 1 more than the number of parens seen in
         * the pattern so far. */
        for ( paren=0 ; paren < RExC_npar ; paren++ ) {
            /* note, RExC_open_parens[0] is the start of the
             * regex, it can't move. RExC_close_parens[0] is the end
             * of the regex, it *can* move. */
            if ( paren && RExC_open_parens[paren] >= operand ) {
                /*DEBUG_PARSE_FMT("open"," - %d", size);*/
                RExC_open_parens[paren] += size;
            } else {
                /*DEBUG_PARSE_FMT("open"," - %s","ok");*/
            }
            if ( RExC_close_parens[paren] >= operand ) {
                /*DEBUG_PARSE_FMT("close"," - %d", size);*/
                RExC_close_parens[paren] += size;
            } else {
                /*DEBUG_PARSE_FMT("close"," - %s","ok");*/
            }
        }
    }
    if (RExC_end_op)
        RExC_end_op += size;

    while (src > REGNODE_p(operand)) {
	StructCopy(--src, --dst, regnode);
#ifdef RE_TRACK_PATTERN_OFFSETS
        if (RExC_offsets) {     /* MJD 20010112 */
	    MJD_OFFSET_DEBUG(
                 ("%s(%d): (op %s) %s copy %" UVuf " -> %" UVuf " (max %" UVuf ").\n",
                  "reginsert",
		  __LINE__,
		  PL_reg_name[op],
                  (UV)(REGNODE_OFFSET(dst)) > RExC_offsets[0]
		    ? "Overwriting end of array!\n" : "OK",
                  (UV)REGNODE_OFFSET(src),
                  (UV)REGNODE_OFFSET(dst),
                  (UV)RExC_offsets[0]));
	    Set_Node_Offset_To_R(REGNODE_OFFSET(dst), Node_Offset(src));
	    Set_Node_Length_To_R(REGNODE_OFFSET(dst), Node_Length(src));
        }
#endif
    }

    place = REGNODE_p(operand);	/* Op node, where operand used to be. */
#ifdef RE_TRACK_PATTERN_OFFSETS
    if (RExC_offsets) {         /* MJD */
	MJD_OFFSET_DEBUG(
              ("%s(%d): (op %s) %s %" UVuf " <- %" UVuf " (max %" UVuf ").\n",
              "reginsert",
	      __LINE__,
	      PL_reg_name[op],
              (UV)REGNODE_OFFSET(place) > RExC_offsets[0]
              ? "Overwriting end of array!\n" : "OK",
              (UV)REGNODE_OFFSET(place),
              (UV)(RExC_parse - RExC_start),
              (UV)RExC_offsets[0]));
	Set_Node_Offset(place, RExC_parse);
	Set_Node_Length(place, 1);
    }
#endif
    src = NEXTOPER(place);
    FLAGS(place) = 0;
    FILL_NODE(operand, op);

    /* Zero out any arguments in the new node */
    Zero(src, offset, regnode);
}

/*
- regtail - set the next-pointer at the end of a node chain of p to val.  If
            that value won't fit in the space available, instead returns FALSE.
            (Except asserts if we can't fit in the largest space the regex
            engine is designed for.)
- SEE ALSO: regtail_study
*/
STATIC bool
S_regtail(pTHX_ RExC_state_t * pRExC_state,
                const regnode_offset p,
                const regnode_offset val,
                const U32 depth)
{
    regnode_offset scan;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTAIL;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    /* Find last node. */
    scan = (regnode_offset) p;
    for (;;) {
	regnode * const temp = regnext(REGNODE_p(scan));
        DEBUG_PARSE_r({
            DEBUG_PARSE_MSG((scan==p ? "tail" : ""));
            regprop(RExC_rx, RExC_mysv, REGNODE_p(scan), NULL, pRExC_state);
            Perl_re_printf( aTHX_  "~ %s (%d) %s %s\n",
                SvPV_nolen_const(RExC_mysv), scan,
                    (temp == NULL ? "->" : ""),
                    (temp == NULL ? PL_reg_name[OP(REGNODE_p(val))] : "")
            );
        });
        if (temp == NULL)
            break;
        scan = REGNODE_OFFSET(temp);
    }

    if (reg_off_by_arg[OP(REGNODE_p(scan))]) {
        assert((UV) (val - scan) <= U32_MAX);
        ARG_SET(REGNODE_p(scan), val - scan);
    }
    else {
        if (val - scan > U16_MAX) {
            /* Populate this with something that won't loop and will likely
             * lead to a crash if the caller ignores the failure return, and
             * execution continues */
            NEXT_OFF(REGNODE_p(scan)) = U16_MAX;
            return FALSE;
        }
        NEXT_OFF(REGNODE_p(scan)) = val - scan;
    }

    return TRUE;
}

#ifdef DEBUGGING
/*
- regtail_study - set the next-pointer at the end of a node chain of p to val.
- Look for optimizable sequences at the same time.
- currently only looks for EXACT chains.

This is experimental code. The idea is to use this routine to perform
in place optimizations on branches and groups as they are constructed,
with the long term intention of removing optimization from study_chunk so
that it is purely analytical.

Currently only used when in DEBUG mode. The macro REGTAIL_STUDY() is used
to control which is which.

This used to return a value that was ignored.  It was a problem that it is
#ifdef'd to be another function that didn't return a value.  khw has changed it
so both currently return a pass/fail return.

*/
/* TODO: All four parms should be const */

STATIC bool
S_regtail_study(pTHX_ RExC_state_t *pRExC_state, regnode_offset p,
                      const regnode_offset val, U32 depth)
{
    regnode_offset scan;
    U8 exact = PSEUDO;
#ifdef EXPERIMENTAL_INPLACESCAN
    I32 min = 0;
#endif
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTAIL_STUDY;


    /* Find last node. */

    scan = p;
    for (;;) {
        regnode * const temp = regnext(REGNODE_p(scan));
#ifdef EXPERIMENTAL_INPLACESCAN
        if (PL_regkind[OP(REGNODE_p(scan))] == EXACT) {
	    bool unfolded_multi_char;	/* Unexamined in this routine */
            if (join_exact(pRExC_state, scan, &min,
                           &unfolded_multi_char, 1, REGNODE_p(val), depth+1))
                return TRUE; /* Was return EXACT */
	}
#endif
        if ( exact ) {
            switch (OP(REGNODE_p(scan))) {
                case EXACT:
                case EXACT_ONLY8:
                case EXACTL:
                case EXACTF:
                case EXACTFU_S_EDGE:
                case EXACTFAA_NO_TRIE:
                case EXACTFAA:
                case EXACTFU:
                case EXACTFU_ONLY8:
                case EXACTFLU8:
                case EXACTFUP:
                case EXACTFL:
                        if( exact == PSEUDO )
                            exact= OP(REGNODE_p(scan));
                        else if ( exact != OP(REGNODE_p(scan)) )
                            exact= 0;
                case NOTHING:
                    break;
                default:
                    exact= 0;
            }
        }
        DEBUG_PARSE_r({
            DEBUG_PARSE_MSG((scan==p ? "tsdy" : ""));
            regprop(RExC_rx, RExC_mysv, REGNODE_p(scan), NULL, pRExC_state);
            Perl_re_printf( aTHX_  "~ %s (%d) -> %s\n",
                SvPV_nolen_const(RExC_mysv),
                scan,
                PL_reg_name[exact]);
        });
	if (temp == NULL)
	    break;
	scan = REGNODE_OFFSET(temp);
    }
    DEBUG_PARSE_r({
        DEBUG_PARSE_MSG("");
        regprop(RExC_rx, RExC_mysv, REGNODE_p(val), NULL, pRExC_state);
        Perl_re_printf( aTHX_
                      "~ attach to %s (%" IVdf ") offset to %" IVdf "\n",
		      SvPV_nolen_const(RExC_mysv),
		      (IV)val,
		      (IV)(val - scan)
        );
    });
    if (reg_off_by_arg[OP(REGNODE_p(scan))]) {
        assert((UV) (val - scan) <= U32_MAX);
	ARG_SET(REGNODE_p(scan), val - scan);
    }
    else {
        if (val - scan > U16_MAX) {
            /* Populate this with something that won't loop and will likely
             * lead to a crash if the caller ignores the failure return, and
             * execution continues */
            NEXT_OFF(REGNODE_p(scan)) = U16_MAX;
            return FALSE;
        }
	NEXT_OFF(REGNODE_p(scan)) = val - scan;
    }

    return TRUE; /* Was 'return exact' */
}
#endif

STATIC SV*
S_get_ANYOFM_contents(pTHX_ const regnode * n) {

    /* Returns an inversion list of all the code points matched by the
     * ANYOFM/NANYOFM node 'n' */

    SV * cp_list = _new_invlist(-1);
    const U8 lowest = (U8) ARG(n);
    unsigned int i;
    U8 count = 0;
    U8 needed = 1U << PL_bitcount[ (U8) ~ FLAGS(n)];

    PERL_ARGS_ASSERT_GET_ANYOFM_CONTENTS;

    /* Starting with the lowest code point, any code point that ANDed with the
     * mask yields the lowest code point is in the set */
    for (i = lowest; i <= 0xFF; i++) {
        if ((i & FLAGS(n)) == ARG(n)) {
            cp_list = add_cp_to_invlist(cp_list, i);
            count++;

            /* We know how many code points (a power of two) that are in the
             * set.  No use looking once we've got that number */
            if (count >= needed) break;
        }
    }

    if (OP(n) == NANYOFM) {
        _invlist_invert(cp_list);
    }
    return cp_list;
}

/*
 - regdump - dump a regexp onto Perl_debug_log in vaguely comprehensible form
 */
#ifdef DEBUGGING

static void
S_regdump_intflags(pTHX_ const char *lead, const U32 flags)
{
    int bit;
    int set=0;

    ASSUME(REG_INTFLAGS_NAME_SIZE <= sizeof(flags)*8);

    for (bit=0; bit<REG_INTFLAGS_NAME_SIZE; bit++) {
        if (flags & (1<<bit)) {
            if (!set++ && lead)
                Perl_re_printf( aTHX_  "%s", lead);
            Perl_re_printf( aTHX_  "%s ", PL_reg_intflags_name[bit]);
        }
    }
    if (lead)  {
        if (set)
            Perl_re_printf( aTHX_  "\n");
        else
            Perl_re_printf( aTHX_  "%s[none-set]\n", lead);
    }
}

static void
S_regdump_extflags(pTHX_ const char *lead, const U32 flags)
{
    int bit;
    int set=0;
    regex_charset cs;

    ASSUME(REG_EXTFLAGS_NAME_SIZE <= sizeof(flags)*8);

    for (bit=0; bit<REG_EXTFLAGS_NAME_SIZE; bit++) {
        if (flags & (1<<bit)) {
	    if ((1<<bit) & RXf_PMf_CHARSET) {	/* Output separately, below */
		continue;
	    }
            if (!set++ && lead)
                Perl_re_printf( aTHX_  "%s", lead);
            Perl_re_printf( aTHX_  "%s ", PL_reg_extflags_name[bit]);
        }
    }
    if ((cs = get_regex_charset(flags)) != REGEX_DEPENDS_CHARSET) {
            if (!set++ && lead) {
                Perl_re_printf( aTHX_  "%s", lead);
            }
            switch (cs) {
                case REGEX_UNICODE_CHARSET:
                    Perl_re_printf( aTHX_  "UNICODE");
                    break;
                case REGEX_LOCALE_CHARSET:
                    Perl_re_printf( aTHX_  "LOCALE");
                    break;
                case REGEX_ASCII_RESTRICTED_CHARSET:
                    Perl_re_printf( aTHX_  "ASCII-RESTRICTED");
                    break;
                case REGEX_ASCII_MORE_RESTRICTED_CHARSET:
                    Perl_re_printf( aTHX_  "ASCII-MORE_RESTRICTED");
                    break;
                default:
                    Perl_re_printf( aTHX_  "UNKNOWN CHARACTER SET");
                    break;
            }
    }
    if (lead)  {
        if (set)
            Perl_re_printf( aTHX_  "\n");
        else
            Perl_re_printf( aTHX_  "%s[none-set]\n", lead);
    }
}
#endif

void
Perl_regdump(pTHX_ const regexp *r)
{
#ifdef DEBUGGING
    int i;
    SV * const sv = sv_newmortal();
    SV *dsv= sv_newmortal();
    RXi_GET_DECL(r, ri);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGDUMP;

    (void)dumpuntil(r, ri->program, ri->program + 1, NULL, NULL, sv, 0, 0);

    /* Header fields of interest. */
    for (i = 0; i < 2; i++) {
        if (r->substrs->data[i].substr) {
            RE_PV_QUOTED_DECL(s, 0, dsv,
                            SvPVX_const(r->substrs->data[i].substr),
                            RE_SV_DUMPLEN(r->substrs->data[i].substr),
                            PL_dump_re_max_len);
            Perl_re_printf( aTHX_
                          "%s %s%s at %" IVdf "..%" UVuf " ",
                          i ? "floating" : "anchored",
                          s,
                          RE_SV_TAIL(r->substrs->data[i].substr),
                          (IV)r->substrs->data[i].min_offset,
                          (UV)r->substrs->data[i].max_offset);
        }
        else if (r->substrs->data[i].utf8_substr) {
            RE_PV_QUOTED_DECL(s, 1, dsv,
                            SvPVX_const(r->substrs->data[i].utf8_substr),
                            RE_SV_DUMPLEN(r->substrs->data[i].utf8_substr),
                            30);
            Perl_re_printf( aTHX_
                          "%s utf8 %s%s at %" IVdf "..%" UVuf " ",
                          i ? "floating" : "anchored",
                          s,
                          RE_SV_TAIL(r->substrs->data[i].utf8_substr),
                          (IV)r->substrs->data[i].min_offset,
                          (UV)r->substrs->data[i].max_offset);
        }
    }

    if (r->check_substr || r->check_utf8)
        Perl_re_printf( aTHX_
		      (const char *)
		      (   r->check_substr == r->substrs->data[1].substr
		       && r->check_utf8   == r->substrs->data[1].utf8_substr
		       ? "(checking floating" : "(checking anchored"));
    if (r->intflags & PREGf_NOSCAN)
        Perl_re_printf( aTHX_  " noscan");
    if (r->extflags & RXf_CHECK_ALL)
        Perl_re_printf( aTHX_  " isall");
    if (r->check_substr || r->check_utf8)
        Perl_re_printf( aTHX_  ") ");

    if (ri->regstclass) {
        regprop(r, sv, ri->regstclass, NULL, NULL);
        Perl_re_printf( aTHX_  "stclass %s ", SvPVX_const(sv));
    }
    if (r->intflags & PREGf_ANCH) {
        Perl_re_printf( aTHX_  "anchored");
        if (r->intflags & PREGf_ANCH_MBOL)
            Perl_re_printf( aTHX_  "(MBOL)");
        if (r->intflags & PREGf_ANCH_SBOL)
            Perl_re_printf( aTHX_  "(SBOL)");
        if (r->intflags & PREGf_ANCH_GPOS)
            Perl_re_printf( aTHX_  "(GPOS)");
        Perl_re_printf( aTHX_ " ");
    }
    if (r->intflags & PREGf_GPOS_SEEN)
        Perl_re_printf( aTHX_  "GPOS:%" UVuf " ", (UV)r->gofs);
    if (r->intflags & PREGf_SKIP)
        Perl_re_printf( aTHX_  "plus ");
    if (r->intflags & PREGf_IMPLICIT)
        Perl_re_printf( aTHX_  "implicit ");
    Perl_re_printf( aTHX_  "minlen %" IVdf " ", (IV)r->minlen);
    if (r->extflags & RXf_EVAL_SEEN)
        Perl_re_printf( aTHX_  "with eval ");
    Perl_re_printf( aTHX_  "\n");
    DEBUG_FLAGS_r({
        regdump_extflags("r->extflags: ", r->extflags);
        regdump_intflags("r->intflags: ", r->intflags);
    });
#else
    PERL_ARGS_ASSERT_REGDUMP;
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(r);
#endif	/* DEBUGGING */
}

/* Should be synchronized with ANYOF_ #defines in regcomp.h */
#ifdef DEBUGGING

#  if   _CC_WORDCHAR != 0 || _CC_DIGIT != 1        || _CC_ALPHA != 2    \
     || _CC_LOWER != 3    || _CC_UPPER != 4        || _CC_PUNCT != 5    \
     || _CC_PRINT != 6    || _CC_ALPHANUMERIC != 7 || _CC_GRAPH != 8    \
     || _CC_CASED != 9    || _CC_SPACE != 10       || _CC_BLANK != 11   \
     || _CC_XDIGIT != 12  || _CC_CNTRL != 13       || _CC_ASCII != 14   \
     || _CC_VERTSPACE != 15
#   error Need to adjust order of anyofs[]
#  endif
static const char * const anyofs[] = {
    "\\w",
    "\\W",
    "\\d",
    "\\D",
    "[:alpha:]",
    "[:^alpha:]",
    "[:lower:]",
    "[:^lower:]",
    "[:upper:]",
    "[:^upper:]",
    "[:punct:]",
    "[:^punct:]",
    "[:print:]",
    "[:^print:]",
    "[:alnum:]",
    "[:^alnum:]",
    "[:graph:]",
    "[:^graph:]",
    "[:cased:]",
    "[:^cased:]",
    "\\s",
    "\\S",
    "[:blank:]",
    "[:^blank:]",
    "[:xdigit:]",
    "[:^xdigit:]",
    "[:cntrl:]",
    "[:^cntrl:]",
    "[:ascii:]",
    "[:^ascii:]",
    "\\v",
    "\\V"
};
#endif

/*
- regprop - printable representation of opcode, with run time support
*/

void
Perl_regprop(pTHX_ const regexp *prog, SV *sv, const regnode *o, const regmatch_info *reginfo, const RExC_state_t *pRExC_state)
{
#ifdef DEBUGGING
    dVAR;
    int k;
    RXi_GET_DECL(prog, progi);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGPROP;

    SvPVCLEAR(sv);

    if (OP(o) > REGNODE_MAX)		/* regnode.type is unsigned */
	/* It would be nice to FAIL() here, but this may be called from
	   regexec.c, and it would be hard to supply pRExC_state. */
	Perl_croak(aTHX_ "Corrupted regexp opcode %d > %d",
                                              (int)OP(o), (int)REGNODE_MAX);
    sv_catpv(sv, PL_reg_name[OP(o)]); /* Take off const! */

    k = PL_regkind[OP(o)];

    if (k == EXACT) {
	sv_catpvs(sv, " ");
	/* Using is_utf8_string() (via PERL_PV_UNI_DETECT)
	 * is a crude hack but it may be the best for now since
	 * we have no flag "this EXACTish node was UTF-8"
	 * --jhi */
	pv_pretty(sv, STRING(o), STR_LEN(o), PL_dump_re_max_len,
                  PL_colors[0], PL_colors[1],
		  PERL_PV_ESCAPE_UNI_DETECT |
		  PERL_PV_ESCAPE_NONASCII   |
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

        Perl_sv_catpvf(aTHX_ sv, "-%s", PL_reg_name[o->flags]);
        DEBUG_TRIE_COMPILE_r({
          if (trie->jump)
            sv_catpvs(sv, "(JUMP)");
          Perl_sv_catpvf(aTHX_ sv,
            "<S:%" UVuf "/%" IVdf " W:%" UVuf " L:%" UVuf "/%" UVuf " C:%" UVuf "/%" UVuf ">",
            (UV)trie->startstate,
            (IV)trie->statecount-1, /* -1 because of the unused 0 element */
            (UV)trie->wordcount,
            (UV)trie->minlen,
            (UV)trie->maxlen,
            (UV)TRIE_CHARCOUNT(trie),
            (UV)trie->uniquecharcount
          );
        });
        if ( IS_ANYOF_TRIE(op) || trie->bitmap ) {
            sv_catpvs(sv, "[");
            (void) put_charclass_bitmap_innards(sv,
                                                ((IS_ANYOF_TRIE(op))
                                                 ? ANYOF_BITMAP(o)
                                                 : TRIE_BITMAP(trie)),
                                                NULL,
                                                NULL,
                                                NULL,
                                                FALSE
                                               );
            sv_catpvs(sv, "]");
        }
    } else if (k == CURLY) {
        U32 lo = ARG1(o), hi = ARG2(o);
	if (OP(o) == CURLYM || OP(o) == CURLYN || OP(o) == CURLYX)
	    Perl_sv_catpvf(aTHX_ sv, "[%d]", o->flags); /* Parenth number */
        Perl_sv_catpvf(aTHX_ sv, "{%u,", (unsigned) lo);
        if (hi == REG_INFTY)
            sv_catpvs(sv, "INFTY");
        else
            Perl_sv_catpvf(aTHX_ sv, "%u", (unsigned) hi);
        sv_catpvs(sv, "}");
    }
    else if (k == WHILEM && o->flags)			/* Ordinal/of */
	Perl_sv_catpvf(aTHX_ sv, "[%d/%d]", o->flags & 0xf, o->flags>>4);
    else if (k == REF || k == OPEN || k == CLOSE
             || k == GROUPP || OP(o)==ACCEPT)
    {
        AV *name_list= NULL;
        U32 parno= OP(o) == ACCEPT ? (U32)ARG2L(o) : ARG(o);
        Perl_sv_catpvf(aTHX_ sv, "%" UVuf, (UV)parno);        /* Parenth number */
	if ( RXp_PAREN_NAMES(prog) ) {
            name_list= MUTABLE_AV(progi->data->data[progi->name_list_idx]);
        } else if ( pRExC_state ) {
            name_list= RExC_paren_name_list;
        }
        if (name_list) {
            if ( k != REF || (OP(o) < NREF)) {
                SV **name= av_fetch(name_list, parno, 0 );
	        if (name)
	            Perl_sv_catpvf(aTHX_ sv, " '%" SVf "'", SVfARG(*name));
            }
            else {
                SV *sv_dat= MUTABLE_SV(progi->data->data[ parno ]);
                I32 *nums=(I32*)SvPVX(sv_dat);
                SV **name= av_fetch(name_list, nums[0], 0 );
                I32 n;
                if (name) {
                    for ( n=0; n<SvIVX(sv_dat); n++ ) {
                        Perl_sv_catpvf(aTHX_ sv, "%s%" IVdf,
			   	    (n ? "," : ""), (IV)nums[n]);
                    }
                    Perl_sv_catpvf(aTHX_ sv, " '%" SVf "'", SVfARG(*name));
                }
            }
        }
        if ( k == REF && reginfo) {
            U32 n = ARG(o);  /* which paren pair */
            I32 ln = prog->offs[n].start;
            if (prog->lastparen < n || ln == -1 || prog->offs[n].end == -1)
                Perl_sv_catpvf(aTHX_ sv, ": FAIL");
            else if (ln == prog->offs[n].end)
                Perl_sv_catpvf(aTHX_ sv, ": ACCEPT - EMPTY STRING");
            else {
                const char *s = reginfo->strbeg + ln;
                Perl_sv_catpvf(aTHX_ sv, ": ");
                Perl_pv_pretty( aTHX_ sv, s, prog->offs[n].end - prog->offs[n].start, 32, 0, 0,
                    PERL_PV_ESCAPE_UNI_DETECT|PERL_PV_PRETTY_NOCLEAR|PERL_PV_PRETTY_ELLIPSES|PERL_PV_PRETTY_QUOTE );
            }
        }
    } else if (k == GOSUB) {
        AV *name_list= NULL;
        if ( RXp_PAREN_NAMES(prog) ) {
            name_list= MUTABLE_AV(progi->data->data[progi->name_list_idx]);
        } else if ( pRExC_state ) {
            name_list= RExC_paren_name_list;
        }

        /* Paren and offset */
        Perl_sv_catpvf(aTHX_ sv, "%d[%+d:%d]", (int)ARG(o),(int)ARG2L(o),
                (int)((o + (int)ARG2L(o)) - progi->program) );
        if (name_list) {
            SV **name= av_fetch(name_list, ARG(o), 0 );
            if (name)
                Perl_sv_catpvf(aTHX_ sv, " '%" SVf "'", SVfARG(*name));
        }
    }
    else if (k == LOGICAL)
        /* 2: embedded, otherwise 1 */
	Perl_sv_catpvf(aTHX_ sv, "[%d]", o->flags);
    else if (k == ANYOF) {
	const U8 flags = (OP(o) == ANYOFH) ? 0 : ANYOF_FLAGS(o);
        bool do_sep = FALSE;    /* Do we need to separate various components of
                                   the output? */
        /* Set if there is still an unresolved user-defined property */
        SV *unresolved                = NULL;

        /* Things that are ignored except when the runtime locale is UTF-8 */
        SV *only_utf8_locale_invlist = NULL;

        /* Code points that don't fit in the bitmap */
        SV *nonbitmap_invlist = NULL;

        /* And things that aren't in the bitmap, but are small enough to be */
        SV* bitmap_range_not_in_bitmap = NULL;

        const bool inverted = flags & ANYOF_INVERT;

	if (OP(o) == ANYOFL || OP(o) == ANYOFPOSIXL) {
            if (ANYOFL_UTF8_LOCALE_REQD(flags)) {
                sv_catpvs(sv, "{utf8-locale-reqd}");
            }
            if (flags & ANYOFL_FOLD) {
                sv_catpvs(sv, "{i}");
            }
        }

        /* If there is stuff outside the bitmap, get it */
        if (ARG(o) != ANYOF_ONLY_HAS_BITMAP) {
            (void) _get_regclass_nonbitmap_data(prog, o, FALSE,
                                                &unresolved,
                                                &only_utf8_locale_invlist,
                                                &nonbitmap_invlist);
            /* The non-bitmap data may contain stuff that could fit in the
             * bitmap.  This could come from a user-defined property being
             * finally resolved when this call was done; or much more likely
             * because there are matches that require UTF-8 to be valid, and so
             * aren't in the bitmap.  This is teased apart later */
            _invlist_intersection(nonbitmap_invlist,
                                  PL_InBitmap,
                                  &bitmap_range_not_in_bitmap);
            /* Leave just the things that don't fit into the bitmap */
            _invlist_subtract(nonbitmap_invlist,
                              PL_InBitmap,
                              &nonbitmap_invlist);
        }

        /* Obey this flag to add all above-the-bitmap code points */
        if (flags & ANYOF_MATCHES_ALL_ABOVE_BITMAP) {
            nonbitmap_invlist = _add_range_to_invlist(nonbitmap_invlist,
                                                      NUM_ANYOF_CODE_POINTS,
                                                      UV_MAX);
        }

        /* Ready to start outputting.  First, the initial left bracket */
	Perl_sv_catpvf(aTHX_ sv, "[%s", PL_colors[0]);

        if (OP(o) != ANYOFH) {
            /* Then all the things that could fit in the bitmap */
            do_sep = put_charclass_bitmap_innards(sv,
                                                  ANYOF_BITMAP(o),
                                                  bitmap_range_not_in_bitmap,
                                                  only_utf8_locale_invlist,
                                                  o,

                                                  /* Can't try inverting for a
                                                   * better display if there
                                                   * are things that haven't
                                                   * been resolved */
                                                  unresolved != NULL);
            SvREFCNT_dec(bitmap_range_not_in_bitmap);

            /* If there are user-defined properties which haven't been defined
             * yet, output them.  If the result is not to be inverted, it is
             * clearest to output them in a separate [] from the bitmap range
             * stuff.  If the result is to be complemented, we have to show
             * everything in one [], as the inversion applies to the whole
             * thing.  Use {braces} to separate them from anything in the
             * bitmap and anything above the bitmap. */
            if (unresolved) {
                if (inverted) {
                    if (! do_sep) { /* If didn't output anything in the bitmap
                                     */
                        sv_catpvs(sv, "^");
                    }
                    sv_catpvs(sv, "{");
                }
                else if (do_sep) {
                    Perl_sv_catpvf(aTHX_ sv,"%s][%s", PL_colors[1],
                                                      PL_colors[0]);
                }
                sv_catsv(sv, unresolved);
                if (inverted) {
                    sv_catpvs(sv, "}");
                }
                do_sep = ! inverted;
            }
        }

        /* And, finally, add the above-the-bitmap stuff */
        if (nonbitmap_invlist && _invlist_len(nonbitmap_invlist)) {
            SV* contents;

            /* See if truncation size is overridden */
            const STRLEN dump_len = (PL_dump_re_max_len > 256)
                                    ? PL_dump_re_max_len
                                    : 256;

            /* This is output in a separate [] */
            if (do_sep) {
                Perl_sv_catpvf(aTHX_ sv,"%s][%s", PL_colors[1], PL_colors[0]);
            }

            /* And, for easy of understanding, it is shown in the
             * uncomplemented form if possible.  The one exception being if
             * there are unresolved items, where the inversion has to be
             * delayed until runtime */
            if (inverted && ! unresolved) {
                _invlist_invert(nonbitmap_invlist);
                _invlist_subtract(nonbitmap_invlist, PL_InBitmap, &nonbitmap_invlist);
            }

            contents = invlist_contents(nonbitmap_invlist,
                                        FALSE /* output suitable for catsv */
                                       );

            /* If the output is shorter than the permissible maximum, just do it. */
            if (SvCUR(contents) <= dump_len) {
                sv_catsv(sv, contents);
            }
            else {
                const char * contents_string = SvPVX(contents);
                STRLEN i = dump_len;

                /* Otherwise, start at the permissible max and work back to the
                 * first break possibility */
                while (i > 0 && contents_string[i] != ' ') {
                    i--;
                }
                if (i == 0) {       /* Fail-safe.  Use the max if we couldn't
                                       find a legal break */
                    i = dump_len;
                }

                sv_catpvn(sv, contents_string, i);
                sv_catpvs(sv, "...");
            }

            SvREFCNT_dec_NN(contents);
            SvREFCNT_dec_NN(nonbitmap_invlist);
        }

        /* And finally the matching, closing ']' */
	Perl_sv_catpvf(aTHX_ sv, "%s]", PL_colors[1]);

        if (OP(o) == ANYOFH && FLAGS(o) != 0) {
            Perl_sv_catpvf(aTHX_ sv, " (First UTF-8 byte=\\x%02x)", FLAGS(o));
        }


        SvREFCNT_dec(unresolved);
    }
    else if (k == ANYOFM) {
        SV * cp_list = get_ANYOFM_contents(o);

	Perl_sv_catpvf(aTHX_ sv, "[%s", PL_colors[0]);
        if (OP(o) == NANYOFM) {
            _invlist_invert(cp_list);
        }

        put_charclass_bitmap_innards(sv, NULL, cp_list, NULL, NULL, TRUE);
	Perl_sv_catpvf(aTHX_ sv, "%s]", PL_colors[1]);

        SvREFCNT_dec(cp_list);
    }
    else if (k == POSIXD || k == NPOSIXD) {
        U8 index = FLAGS(o) * 2;
        if (index < C_ARRAY_LENGTH(anyofs)) {
            if (*anyofs[index] != '[')  {
                sv_catpvs(sv, "[");
            }
            sv_catpv(sv, anyofs[index]);
            if (*anyofs[index] != '[')  {
                sv_catpvs(sv, "]");
            }
        }
        else {
            Perl_sv_catpvf(aTHX_ sv, "[illegal type=%d])", index);
        }
    }
    else if (k == BOUND || k == NBOUND) {
        /* Must be synced with order of 'bound_type' in regcomp.h */
        const char * const bounds[] = {
            "",      /* Traditional */
            "{gcb}",
            "{lb}",
            "{sb}",
            "{wb}"
        };
        assert(FLAGS(o) < C_ARRAY_LENGTH(bounds));
        sv_catpv(sv, bounds[FLAGS(o)]);
    }
    else if (k == BRANCHJ && (OP(o) == UNLESSM || OP(o) == IFMATCH)) {
	Perl_sv_catpvf(aTHX_ sv, "[%d", -(o->flags));
        if (o->next_off) {
            Perl_sv_catpvf(aTHX_ sv, "..-%d", o->flags - o->next_off);
        }
	Perl_sv_catpvf(aTHX_ sv, "]");
    }
    else if (OP(o) == SBOL)
        Perl_sv_catpvf(aTHX_ sv, " /%s/", o->flags ? "\\A" : "^");

    /* add on the verb argument if there is one */
    if ( ( k == VERB || OP(o) == ACCEPT || OP(o) == OPFAIL ) && o->flags) {
        if ( ARG(o) )
            Perl_sv_catpvf(aTHX_ sv, ":%" SVf,
                       SVfARG((MUTABLE_SV(progi->data->data[ ARG( o ) ]))));
        else
            sv_catpvs(sv, ":NULL");
    }
#else
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
    PERL_UNUSED_ARG(o);
    PERL_UNUSED_ARG(prog);
    PERL_UNUSED_ARG(reginfo);
    PERL_UNUSED_ARG(pRExC_state);
#endif	/* DEBUGGING */
}



SV *
Perl_re_intuit_string(pTHX_ REGEXP * const r)
{				/* Assume that RE_INTUIT is set */
    struct regexp *const prog = ReANY(r);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_INTUIT_STRING;
    PERL_UNUSED_CONTEXT;

    DEBUG_COMPILE_r(
	{
	    const char * const s = SvPV_nolen_const(RX_UTF8(r)
		      ? prog->check_utf8 : prog->check_substr);

	    if (!PL_colorset) reginitcolors();
            Perl_re_printf( aTHX_
		      "%sUsing REx %ssubstr:%s \"%s%.60s%s%s\"\n",
		      PL_colors[4],
		      RX_UTF8(r) ? "utf8 " : "",
		      PL_colors[5], PL_colors[0],
		      s,
		      PL_colors[1],
		      (strlen(s) > PL_dump_re_max_len ? "..." : ""));
	} );

    /* use UTF8 check substring if regexp pattern itself is in UTF8 */
    return RX_UTF8(r) ? prog->check_utf8 : prog->check_substr;
}

/*
   pregfree()

   handles refcounting and freeing the perl core regexp structure. When
   it is necessary to actually free the structure the first thing it
   does is call the 'free' method of the regexp_engine associated to
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
    struct regexp *const r = ReANY(rx);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_PREGFREE2;

    if (! r)
        return;

    if (r->mother_re) {
        ReREFCNT_dec(r->mother_re);
    } else {
        CALLREGFREE_PVT(rx); /* free the private data */
        SvREFCNT_dec(RXp_PAREN_NAMES(r));
    }
    if (r->substrs) {
        int i;
        for (i = 0; i < 2; i++) {
            SvREFCNT_dec(r->substrs->data[i].substr);
            SvREFCNT_dec(r->substrs->data[i].utf8_substr);
        }
	Safefree(r->substrs);
    }
    RX_MATCH_COPY_FREE(rx);
#ifdef PERL_ANY_COW
    SvREFCNT_dec(r->saved_copy);
#endif
    Safefree(r->offs);
    SvREFCNT_dec(r->qr_anoncv);
    if (r->recurse_locinput)
        Safefree(r->recurse_locinput);
}


/*  reg_temp_copy()

    Copy ssv to dsv, both of which should of type SVt_REGEXP or SVt_PVLV,
    except that dsv will be created if NULL.

    This function is used in two main ways. First to implement
        $r = qr/....; $s = $$r;

    Secondly, it is used as a hacky workaround to the structural issue of
    match results
    being stored in the regexp structure which is in turn stored in
    PL_curpm/PL_reg_curpm. The problem is that due to qr// the pattern
    could be PL_curpm in multiple contexts, and could require multiple
    result sets being associated with the pattern simultaneously, such
    as when doing a recursive match with (??{$qr})

    The solution is to make a lightweight copy of the regexp structure
    when a qr// is returned from the code executed by (??{$qr}) this
    lightweight copy doesn't actually own any of its data except for
    the starp/end and the actual regexp structure itself.

*/


REGEXP *
Perl_reg_temp_copy(pTHX_ REGEXP *dsv, REGEXP *ssv)
{
    struct regexp *drx;
    struct regexp *const srx = ReANY(ssv);
    const bool islv = dsv && SvTYPE(dsv) == SVt_PVLV;

    PERL_ARGS_ASSERT_REG_TEMP_COPY;

    if (!dsv)
	dsv = (REGEXP*) newSV_type(SVt_REGEXP);
    else {
        assert(SvTYPE(dsv) == SVt_REGEXP || (SvTYPE(dsv) == SVt_PVLV));

        /* our only valid caller, sv_setsv_flags(), should have done
         * a SV_CHECK_THINKFIRST_COW_DROP() by now */
        assert(!SvOOK(dsv));
        assert(!SvIsCOW(dsv));
        assert(!SvROK(dsv));

        if (SvPVX_const(dsv)) {
            if (SvLEN(dsv))
                Safefree(SvPVX(dsv));
            SvPVX(dsv) = NULL;
        }
        SvLEN_set(dsv, 0);
        SvCUR_set(dsv, 0);
	SvOK_off((SV *)dsv);

	if (islv) {
	    /* For PVLVs, the head (sv_any) points to an XPVLV, while
             * the LV's xpvlenu_rx will point to a regexp body, which
             * we allocate here */
	    REGEXP *temp = (REGEXP *)newSV_type(SVt_REGEXP);
	    assert(!SvPVX(dsv));
            ((XPV*)SvANY(dsv))->xpv_len_u.xpvlenu_rx = temp->sv_any;
	    temp->sv_any = NULL;
	    SvFLAGS(temp) = (SvFLAGS(temp) & ~SVTYPEMASK) | SVt_NULL;
	    SvREFCNT_dec_NN(temp);
	    /* SvCUR still resides in the xpvlv struct, so the regexp copy-
	       ing below will not set it. */
	    SvCUR_set(dsv, SvCUR(ssv));
	}
    }
    /* This ensures that SvTHINKFIRST(sv) is true, and hence that
       sv_force_normal(sv) is called.  */
    SvFAKE_on(dsv);
    drx = ReANY(dsv);

    SvFLAGS(dsv) |= SvFLAGS(ssv) & (SVf_POK|SVp_POK|SVf_UTF8);
    SvPV_set(dsv, RX_WRAPPED(ssv));
    /* We share the same string buffer as the original regexp, on which we
       hold a reference count, incremented when mother_re is set below.
       The string pointer is copied here, being part of the regexp struct.
     */
    memcpy(&(drx->xpv_cur), &(srx->xpv_cur),
	   sizeof(regexp) - STRUCT_OFFSET(regexp, xpv_cur));
    if (!islv)
        SvLEN_set(dsv, 0);
    if (srx->offs) {
        const I32 npar = srx->nparens+1;
        Newx(drx->offs, npar, regexp_paren_pair);
        Copy(srx->offs, drx->offs, npar, regexp_paren_pair);
    }
    if (srx->substrs) {
        int i;
        Newx(drx->substrs, 1, struct reg_substr_data);
	StructCopy(srx->substrs, drx->substrs, struct reg_substr_data);

        for (i = 0; i < 2; i++) {
            SvREFCNT_inc_void(drx->substrs->data[i].substr);
            SvREFCNT_inc_void(drx->substrs->data[i].utf8_substr);
        }

	/* check_substr and check_utf8, if non-NULL, point to either their
	   anchored or float namesakes, and don't hold a second reference.  */
    }
    RX_MATCH_COPIED_off(dsv);
#ifdef PERL_ANY_COW
    drx->saved_copy = NULL;
#endif
    drx->mother_re = ReREFCNT_inc(srx->mother_re ? srx->mother_re : ssv);
    SvREFCNT_inc_void(drx->qr_anoncv);
    if (srx->recurse_locinput)
        Newx(drx->recurse_locinput, srx->nparens + 1, char *);

    return dsv;
}
#endif


/* regfree_internal()

   Free the private data in a regexp. This is overloadable by
   extensions. Perl takes care of the regexp structure in pregfree(),
   this covers the *pprivate pointer which technically perl doesn't
   know about, however of course we have to handle the
   regexp_internal structure when no extension is in use.

   Note this is called before freeing anything in the regexp
   structure.
 */

void
Perl_regfree_internal(pTHX_ REGEXP * const rx)
{
    struct regexp *const r = ReANY(rx);
    RXi_GET_DECL(r, ri);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGFREE_INTERNAL;

    if (! ri) {
        return;
    }

    DEBUG_COMPILE_r({
	if (!PL_colorset)
	    reginitcolors();
	{
	    SV *dsv= sv_newmortal();
            RE_PV_QUOTED_DECL(s, RX_UTF8(rx),
                dsv, RX_PRECOMP(rx), RX_PRELEN(rx), PL_dump_re_max_len);
            Perl_re_printf( aTHX_ "%sFreeing REx:%s %s\n",
                PL_colors[4], PL_colors[5], s);
        }
    });

#ifdef RE_TRACK_PATTERN_OFFSETS
    if (ri->u.offsets)
        Safefree(ri->u.offsets);             /* 20010421 MJD */
#endif
    if (ri->code_blocks)
        S_free_codeblocks(aTHX_ ri->code_blocks);

    if (ri->data) {
	int n = ri->data->count;

	while (--n >= 0) {
          /* If you add a ->what type here, update the comment in regcomp.h */
	    switch (ri->data->what[n]) {
	    case 'a':
	    case 'r':
	    case 's':
	    case 'S':
	    case 'u':
		SvREFCNT_dec(MUTABLE_SV(ri->data->data[n]));
		break;
	    case 'f':
		Safefree(ri->data->data[n]);
		break;
	    case 'l':
	    case 'L':
	        break;
            case 'T':
                { /* Aho Corasick add-on structure for a trie node.
                     Used in stclass optimization only */
                    U32 refcount;
                    reg_ac_data *aho=(reg_ac_data*)ri->data->data[n];
#ifdef USE_ITHREADS
                    dVAR;
#endif
                    OP_REFCNT_LOCK;
                    refcount = --aho->refcount;
                    OP_REFCNT_UNLOCK;
                    if ( !refcount ) {
                        PerlMemShared_free(aho->states);
                        PerlMemShared_free(aho->fail);
			 /* do this last!!!! */
                        PerlMemShared_free(ri->data->data[n]);
                        /* we should only ever get called once, so
                         * assert as much, and also guard the free
                         * which /might/ happen twice. At the least
                         * it will make code anlyzers happy and it
                         * doesn't cost much. - Yves */
                        assert(ri->regstclass);
                        if (ri->regstclass) {
                            PerlMemShared_free(ri->regstclass);
                            ri->regstclass = 0;
                        }
                    }
                }
                break;
	    case 't':
	        {
	            /* trie structure. */
	            U32 refcount;
	            reg_trie_data *trie=(reg_trie_data*)ri->data->data[n];
#ifdef USE_ITHREADS
                    dVAR;
#endif
                    OP_REFCNT_LOCK;
                    refcount = --trie->refcount;
                    OP_REFCNT_UNLOCK;
                    if ( !refcount ) {
                        PerlMemShared_free(trie->charmap);
                        PerlMemShared_free(trie->states);
                        PerlMemShared_free(trie->trans);
                        if (trie->bitmap)
                            PerlMemShared_free(trie->bitmap);
                        if (trie->jump)
                            PerlMemShared_free(trie->jump);
			PerlMemShared_free(trie->wordinfo);
                        /* do this last!!!! */
                        PerlMemShared_free(ri->data->data[n]);
		    }
		}
		break;
	    default:
		Perl_croak(aTHX_ "panic: regfree data code '%c'",
                                                    ri->data->what[n]);
	    }
	}
	Safefree(ri->data->what);
	Safefree(ri->data);
    }

    Safefree(ri);
}

#define av_dup_inc(s, t)	MUTABLE_AV(sv_dup_inc((const SV *)s, t))
#define hv_dup_inc(s, t)	MUTABLE_HV(sv_dup_inc((const SV *)s, t))
#define SAVEPVN(p, n)	((p) ? savepvn(p, n) : NULL)

/*
   re_dup_guts - duplicate a regexp.

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
    const struct regexp *r = ReANY(sstr);
    struct regexp *ret = ReANY(dstr);

    PERL_ARGS_ASSERT_RE_DUP_GUTS;

    npar = r->nparens+1;
    Newx(ret->offs, npar, regexp_paren_pair);
    Copy(r->offs, ret->offs, npar, regexp_paren_pair);

    if (ret->substrs) {
	/* Do it this way to avoid reading from *r after the StructCopy().
	   That way, if any of the sv_dup_inc()s dislodge *r from the L1
	   cache, it doesn't matter.  */
        int i;
	const bool anchored = r->check_substr
	    ? r->check_substr == r->substrs->data[0].substr
	    : r->check_utf8   == r->substrs->data[0].utf8_substr;
        Newx(ret->substrs, 1, struct reg_substr_data);
	StructCopy(r->substrs, ret->substrs, struct reg_substr_data);

        for (i = 0; i < 2; i++) {
            ret->substrs->data[i].substr =
                        sv_dup_inc(ret->substrs->data[i].substr, param);
            ret->substrs->data[i].utf8_substr =
                        sv_dup_inc(ret->substrs->data[i].utf8_substr, param);
        }

	/* check_substr and check_utf8, if non-NULL, point to either their
	   anchored or float namesakes, and don't hold a second reference.  */

	if (ret->check_substr) {
	    if (anchored) {
		assert(r->check_utf8 == r->substrs->data[0].utf8_substr);

		ret->check_substr = ret->substrs->data[0].substr;
		ret->check_utf8   = ret->substrs->data[0].utf8_substr;
	    } else {
		assert(r->check_substr == r->substrs->data[1].substr);
		assert(r->check_utf8   == r->substrs->data[1].utf8_substr);

		ret->check_substr = ret->substrs->data[1].substr;
		ret->check_utf8   = ret->substrs->data[1].utf8_substr;
	    }
	} else if (ret->check_utf8) {
	    if (anchored) {
		ret->check_utf8 = ret->substrs->data[0].utf8_substr;
	    } else {
		ret->check_utf8 = ret->substrs->data[1].utf8_substr;
	    }
	}
    }

    RXp_PAREN_NAMES(ret) = hv_dup_inc(RXp_PAREN_NAMES(ret), param);
    ret->qr_anoncv = MUTABLE_CV(sv_dup_inc((const SV *)ret->qr_anoncv, param));
    if (r->recurse_locinput)
        Newx(ret->recurse_locinput, r->nparens + 1, char *);

    if (ret->pprivate)
	RXi_SET(ret, CALLREGDUPE_PVT(dstr, param));

    if (RX_MATCH_COPIED(dstr))
	ret->subbeg  = SAVEPVN(ret->subbeg, ret->sublen);
    else
	ret->subbeg = NULL;
#ifdef PERL_ANY_COW
    ret->saved_copy = NULL;
#endif

    /* Whether mother_re be set or no, we need to copy the string.  We
       cannot refrain from copying it when the storage points directly to
       our mother regexp, because that's
	       1: a buffer in a different thread
	       2: something we no longer hold a reference on
	       so we need to copy it locally.  */
    RX_WRAPPED(dstr) = SAVEPVN(RX_WRAPPED_const(sstr), SvCUR(sstr)+1);
    /* set malloced length to a non-zero value so it will be freed
     * (otherwise in combination with SVf_FAKE it looks like an alien
     * buffer). It doesn't have to be the actual malloced size, since it
     * should never be grown */
    SvLEN_set(dstr, SvCUR(sstr)+1);
    ret->mother_re   = NULL;
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
    struct regexp *const r = ReANY(rx);
    regexp_internal *reti;
    int len;
    RXi_GET_DECL(r, ri);

    PERL_ARGS_ASSERT_REGDUPE_INTERNAL;

    len = ProgLen(ri);

    Newxc(reti, sizeof(regexp_internal) + len*sizeof(regnode),
          char, regexp_internal);
    Copy(ri->program, reti->program, len+1, regnode);


    if (ri->code_blocks) {
	int n;
	Newx(reti->code_blocks, 1, struct reg_code_blocks);
	Newx(reti->code_blocks->cb, ri->code_blocks->count,
                    struct reg_code_block);
	Copy(ri->code_blocks->cb, reti->code_blocks->cb,
             ri->code_blocks->count, struct reg_code_block);
	for (n = 0; n < ri->code_blocks->count; n++)
	     reti->code_blocks->cb[n].src_regex = (REGEXP*)
		    sv_dup_inc((SV*)(ri->code_blocks->cb[n].src_regex), param);
        reti->code_blocks->count = ri->code_blocks->count;
        reti->code_blocks->refcnt = 1;
    }
    else
	reti->code_blocks = NULL;

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
	        /* see also regcomp.h and regfree_internal() */
            case 'a': /* actually an AV, but the dup function is identical.
                         values seem to be "plain sv's" generally. */
            case 'r': /* a compiled regex (but still just another SV) */
            case 's': /* an RV (currently only used for an RV to an AV by the ANYOF code)
                         this use case should go away, the code could have used
                         'a' instead - see S_set_ANYOF_arg() for array contents. */
            case 'S': /* actually an SV, but the dup function is identical.  */
            case 'u': /* actually an HV, but the dup function is identical.
                         values are "plain sv's" */
		d->data[i] = sv_dup_inc((const SV *)ri->data->data[i], param);
		break;
	    case 'f':
                /* Synthetic Start Class - "Fake" charclass we generate to optimize
                 * patterns which could start with several different things. Pre-TRIE
                 * this was more important than it is now, however this still helps
                 * in some places, for instance /x?a+/ might produce a SSC equivalent
                 * to [xa]. This is used by Perl_re_intuit_start() and S_find_byclass()
                 * in regexec.c
                 */
		/* This is cheating. */
		Newx(d->data[i], 1, regnode_ssc);
		StructCopy(ri->data->data[i], d->data[i], regnode_ssc);
		reti->regstclass = (regnode*)d->data[i];
		break;
	    case 'T':
                /* AHO-CORASICK fail table */
                /* Trie stclasses are readonly and can thus be shared
		 * without duplication. We free the stclass in pregfree
		 * when the corresponding reg_ac_data struct is freed.
		 */
		reti->regstclass= ri->regstclass;
		/* FALLTHROUGH */
	    case 't':
                /* TRIE transition table */
		OP_REFCNT_LOCK;
		((reg_trie_data*)ri->data->data[i])->refcount++;
		OP_REFCNT_UNLOCK;
		/* FALLTHROUGH */
            case 'l': /* (?{...}) or (??{ ... }) code (cb->block) */
            case 'L': /* same when RExC_pm_flags & PMf_HAS_CV and code
                         is not from another regexp */
		d->data[i] = ri->data->data[i];
		break;
            default:
                Perl_croak(aTHX_ "panic: re_dup_guts unknown data code '%c'",
                                                           ri->data->what[i]);
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
    SetProgLen(reti, len);
#endif

    return (void*)reti;
}

#endif    /* USE_ITHREADS */

#ifndef PERL_IN_XSUB_RE

/*
 - regnext - dig the "next" pointer out of a node
 */
regnode *
Perl_regnext(pTHX_ regnode *p)
{
    I32 offset;

    if (!p)
	return(NULL);

    if (OP(p) > REGNODE_MAX) {		/* regnode.type is unsigned */
	Perl_croak(aTHX_ "Corrupted regexp opcode %d > %d",
                                                (int)OP(p), (int)REGNODE_MAX);
    }

    offset = (reg_off_by_arg[OP(p)] ? ARG(p) : NEXT_OFF(p));
    if (offset == 0)
	return(NULL);

    return(p+offset);
}

#endif

STATIC void
S_re_croak2(pTHX_ bool utf8, const char* pat1, const char* pat2,...)
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
    va_start(args, pat2);
    msv = vmess(buf, &args);
    va_end(args);
    message = SvPV_const(msv, l1);
    if (l1 > 512)
	l1 = 512;
    Copy(message, buf, l1 , char);
    /* l1-1 to avoid \n */
    Perl_croak(aTHX_ "%" UTF8f, UTF8fARG(utf8, l1-1, buf));
}

/* XXX Here's a total kludge.  But we need to re-enter for swash routines. */

#ifndef PERL_IN_XSUB_RE
void
Perl_save_re_context(pTHX)
{
    I32 nparens = -1;
    I32 i;

    /* Save $1..$n (#18107: UTF-8 s/(\w+)/uc($1)/e); AMS 20021106. */

    if (PL_curpm) {
	const REGEXP * const rx = PM_GETRE(PL_curpm);
	if (rx)
            nparens = RX_NPARENS(rx);
    }

    /* RT #124109. This is a complete hack; in the SWASHNEW case we know
     * that PL_curpm will be null, but that utf8.pm and the modules it
     * loads will only use $1..$3.
     * The t/porting/re_context.t test file checks this assumption.
     */
    if (nparens == -1)
        nparens = 3;

    for (i = 1; i <= nparens; i++) {
        char digits[TYPE_CHARS(long)];
        const STRLEN len = my_snprintf(digits, sizeof(digits),
                                       "%lu", (long)i);
        GV *const *const gvp
            = (GV**)hv_fetch(PL_defstash, digits, len, 0);

        if (gvp) {
            GV * const gv = *gvp;
            if (SvTYPE(gv) == SVt_PVGV && GvSV(gv))
                save_scalar(gv);
        }
    }
}
#endif

#ifdef DEBUGGING

STATIC void
S_put_code_point(pTHX_ SV *sv, UV c)
{
    PERL_ARGS_ASSERT_PUT_CODE_POINT;

    if (c > 255) {
        Perl_sv_catpvf(aTHX_ sv, "\\x{%04" UVXf "}", c);
    }
    else if (isPRINT(c)) {
	const char string = (char) c;

        /* We use {phrase} as metanotation in the class, so also escape literal
         * braces */
	if (isBACKSLASHED_PUNCT(c) || c == '{' || c == '}')
	    sv_catpvs(sv, "\\");
	sv_catpvn(sv, &string, 1);
    }
    else if (isMNEMONIC_CNTRL(c)) {
        Perl_sv_catpvf(aTHX_ sv, "%s", cntrl_to_mnemonic((U8) c));
    }
    else {
        Perl_sv_catpvf(aTHX_ sv, "\\x%02X", (U8) c);
    }
}

#define MAX_PRINT_A MAX_PRINT_A_FOR_USE_ONLY_BY_REGCOMP_DOT_C

STATIC void
S_put_range(pTHX_ SV *sv, UV start, const UV end, const bool allow_literals)
{
    /* Appends to 'sv' a displayable version of the range of code points from
     * 'start' to 'end'.  Mnemonics (like '\r') are used for the few controls
     * that have them, when they occur at the beginning or end of the range.
     * It uses hex to output the remaining code points, unless 'allow_literals'
     * is true, in which case the printable ASCII ones are output as-is (though
     * some of these will be escaped by put_code_point()).
     *
     * NOTE:  This is designed only for printing ranges of code points that fit
     *        inside an ANYOF bitmap.  Higher code points are simply suppressed
     */

    const unsigned int min_range_count = 3;

    assert(start <= end);

    PERL_ARGS_ASSERT_PUT_RANGE;

    while (start <= end) {
        UV this_end;
        const char * format;

        if (end - start < min_range_count) {

            /* Output chars individually when they occur in short ranges */
            for (; start <= end; start++) {
                put_code_point(sv, start);
            }
            break;
        }

        /* If permitted by the input options, and there is a possibility that
         * this range contains a printable literal, look to see if there is
         * one. */
        if (allow_literals && start <= MAX_PRINT_A) {

            /* If the character at the beginning of the range isn't an ASCII
             * printable, effectively split the range into two parts:
             *  1) the portion before the first such printable,
             *  2) the rest
             * and output them separately. */
            if (! isPRINT_A(start)) {
                UV temp_end = start + 1;

                /* There is no point looking beyond the final possible
                 * printable, in MAX_PRINT_A */
                UV max = MIN(end, MAX_PRINT_A);

                while (temp_end <= max && ! isPRINT_A(temp_end)) {
                    temp_end++;
                }

                /* Here, temp_end points to one beyond the first printable if
                 * found, or to one beyond 'max' if not.  If none found, make
                 * sure that we use the entire range */
                if (temp_end > MAX_PRINT_A) {
                    temp_end = end + 1;
                }

                /* Output the first part of the split range: the part that
                 * doesn't have printables, with the parameter set to not look
                 * for literals (otherwise we would infinitely recurse) */
                put_range(sv, start, temp_end - 1, FALSE);

                /* The 2nd part of the range (if any) starts here. */
                start = temp_end;

                /* We do a continue, instead of dropping down, because even if
                 * the 2nd part is non-empty, it could be so short that we want
                 * to output it as individual characters, as tested for at the
                 * top of this loop.  */
                continue;
            }

            /* Here, 'start' is a printable ASCII.  If it is an alphanumeric,
             * output a sub-range of just the digits or letters, then process
             * the remaining portion as usual. */
            if (isALPHANUMERIC_A(start)) {
                UV mask = (isDIGIT_A(start))
                           ? _CC_DIGIT
                             : isUPPER_A(start)
                               ? _CC_UPPER
                               : _CC_LOWER;
                UV temp_end = start + 1;

                /* Find the end of the sub-range that includes just the
                 * characters in the same class as the first character in it */
                while (temp_end <= end && _generic_isCC_A(temp_end, mask)) {
                    temp_end++;
                }
                temp_end--;

                /* For short ranges, don't duplicate the code above to output
                 * them; just call recursively */
                if (temp_end - start < min_range_count) {
                    put_range(sv, start, temp_end, FALSE);
                }
                else {  /* Output as a range */
                    put_code_point(sv, start);
                    sv_catpvs(sv, "-");
                    put_code_point(sv, temp_end);
                }
                start = temp_end + 1;
                continue;
            }

            /* We output any other printables as individual characters */
            if (isPUNCT_A(start) || isSPACE_A(start)) {
                while (start <= end && (isPUNCT_A(start)
                                        || isSPACE_A(start)))
                {
                    put_code_point(sv, start);
                    start++;
                }
                continue;
            }
        } /* End of looking for literals */

        /* Here is not to output as a literal.  Some control characters have
         * mnemonic names.  Split off any of those at the beginning and end of
         * the range to print mnemonically.  It isn't possible for many of
         * these to be in a row, so this won't overwhelm with output */
        if (   start <= end
            && (isMNEMONIC_CNTRL(start) || isMNEMONIC_CNTRL(end)))
        {
            while (isMNEMONIC_CNTRL(start) && start <= end) {
                put_code_point(sv, start);
                start++;
            }

            /* If this didn't take care of the whole range ... */
            if (start <= end) {

                /* Look backwards from the end to find the final non-mnemonic
                 * */
                UV temp_end = end;
                while (isMNEMONIC_CNTRL(temp_end)) {
                    temp_end--;
                }

                /* And separately output the interior range that doesn't start
                 * or end with mnemonics */
                put_range(sv, start, temp_end, FALSE);

                /* Then output the mnemonic trailing controls */
                start = temp_end + 1;
                while (start <= end) {
                    put_code_point(sv, start);
                    start++;
                }
                break;
            }
        }

        /* As a final resort, output the range or subrange as hex. */

        this_end = (end < NUM_ANYOF_CODE_POINTS)
                    ? end
                    : NUM_ANYOF_CODE_POINTS - 1;
#if NUM_ANYOF_CODE_POINTS > 256
        format = (this_end < 256)
                 ? "\\x%02" UVXf "-\\x%02" UVXf
                 : "\\x{%04" UVXf "}-\\x{%04" UVXf "}";
#else
        format = "\\x%02" UVXf "-\\x%02" UVXf;
#endif
        GCC_DIAG_IGNORE_STMT(-Wformat-nonliteral);
        Perl_sv_catpvf(aTHX_ sv, format, start, this_end);
        GCC_DIAG_RESTORE_STMT;
        break;
    }
}

STATIC void
S_put_charclass_bitmap_innards_invlist(pTHX_ SV *sv, SV* invlist)
{
    /* Concatenate onto the PV in 'sv' a displayable form of the inversion list
     * 'invlist' */

    UV start, end;
    bool allow_literals = TRUE;

    PERL_ARGS_ASSERT_PUT_CHARCLASS_BITMAP_INNARDS_INVLIST;

    /* Generally, it is more readable if printable characters are output as
     * literals, but if a range (nearly) spans all of them, it's best to output
     * it as a single range.  This code will use a single range if all but 2
     * ASCII printables are in it */
    invlist_iterinit(invlist);
    while (invlist_iternext(invlist, &start, &end)) {

        /* If the range starts beyond the final printable, it doesn't have any
         * in it */
        if (start > MAX_PRINT_A) {
            break;
        }

        /* In both ASCII and EBCDIC, a SPACE is the lowest printable.  To span
         * all but two, the range must start and end no later than 2 from
         * either end */
        if (start < ' ' + 2 && end > MAX_PRINT_A - 2) {
            if (end > MAX_PRINT_A) {
                end = MAX_PRINT_A;
            }
            if (start < ' ') {
                start = ' ';
            }
            if (end - start >= MAX_PRINT_A - ' ' - 2) {
                allow_literals = FALSE;
            }
            break;
        }
    }
    invlist_iterfinish(invlist);

    /* Here we have figured things out.  Output each range */
    invlist_iterinit(invlist);
    while (invlist_iternext(invlist, &start, &end)) {
        if (start >= NUM_ANYOF_CODE_POINTS) {
            break;
        }
        put_range(sv, start, end, allow_literals);
    }
    invlist_iterfinish(invlist);

    return;
}

STATIC SV*
S_put_charclass_bitmap_innards_common(pTHX_
        SV* invlist,            /* The bitmap */
        SV* posixes,            /* Under /l, things like [:word:], \S */
        SV* only_utf8,          /* Under /d, matches iff the target is UTF-8 */
        SV* not_utf8,           /* /d, matches iff the target isn't UTF-8 */
        SV* only_utf8_locale,   /* Under /l, matches if the locale is UTF-8 */
        const bool invert       /* Is the result to be inverted? */
)
{
    /* Create and return an SV containing a displayable version of the bitmap
     * and associated information determined by the input parameters.  If the
     * output would have been only the inversion indicator '^', NULL is instead
     * returned. */

    dVAR;
    SV * output;

    PERL_ARGS_ASSERT_PUT_CHARCLASS_BITMAP_INNARDS_COMMON;

    if (invert) {
        output = newSVpvs("^");
    }
    else {
        output = newSVpvs("");
    }

    /* First, the code points in the bitmap that are unconditionally there */
    put_charclass_bitmap_innards_invlist(output, invlist);

    /* Traditionally, these have been placed after the main code points */
    if (posixes) {
        sv_catsv(output, posixes);
    }

    if (only_utf8 && _invlist_len(only_utf8)) {
        Perl_sv_catpvf(aTHX_ output, "%s{utf8}%s", PL_colors[1], PL_colors[0]);
        put_charclass_bitmap_innards_invlist(output, only_utf8);
    }

    if (not_utf8 && _invlist_len(not_utf8)) {
        Perl_sv_catpvf(aTHX_ output, "%s{not utf8}%s", PL_colors[1], PL_colors[0]);
        put_charclass_bitmap_innards_invlist(output, not_utf8);
    }

    if (only_utf8_locale && _invlist_len(only_utf8_locale)) {
        Perl_sv_catpvf(aTHX_ output, "%s{utf8 locale}%s", PL_colors[1], PL_colors[0]);
        put_charclass_bitmap_innards_invlist(output, only_utf8_locale);

        /* This is the only list in this routine that can legally contain code
         * points outside the bitmap range.  The call just above to
         * 'put_charclass_bitmap_innards_invlist' will simply suppress them, so
         * output them here.  There's about a half-dozen possible, and none in
         * contiguous ranges longer than 2 */
        if (invlist_highest(only_utf8_locale) >= NUM_ANYOF_CODE_POINTS) {
            UV start, end;
            SV* above_bitmap = NULL;

            _invlist_subtract(only_utf8_locale, PL_InBitmap, &above_bitmap);

            invlist_iterinit(above_bitmap);
            while (invlist_iternext(above_bitmap, &start, &end)) {
                UV i;

                for (i = start; i <= end; i++) {
                    put_code_point(output, i);
                }
            }
            invlist_iterfinish(above_bitmap);
            SvREFCNT_dec_NN(above_bitmap);
        }
    }

    if (invert && SvCUR(output) == 1) {
        return NULL;
    }

    return output;
}

STATIC bool
S_put_charclass_bitmap_innards(pTHX_ SV *sv,
                                     char *bitmap,
                                     SV *nonbitmap_invlist,
                                     SV *only_utf8_locale_invlist,
                                     const regnode * const node,
                                     const bool force_as_is_display)
{
    /* Appends to 'sv' a displayable version of the innards of the bracketed
     * character class defined by the other arguments:
     *  'bitmap' points to the bitmap, or NULL if to ignore that.
     *  'nonbitmap_invlist' is an inversion list of the code points that are in
     *      the bitmap range, but for some reason aren't in the bitmap; NULL if
     *      none.  The reasons for this could be that they require some
     *      condition such as the target string being or not being in UTF-8
     *      (under /d), or because they came from a user-defined property that
     *      was not resolved at the time of the regex compilation (under /u)
     *  'only_utf8_locale_invlist' is an inversion list of the code points that
     *      are valid only if the runtime locale is a UTF-8 one; NULL if none
     *  'node' is the regex pattern ANYOF node.  It is needed only when the
     *      above two parameters are not null, and is passed so that this
     *      routine can tease apart the various reasons for them.
     *  'force_as_is_display' is TRUE if this routine should definitely NOT try
     *      to invert things to see if that leads to a cleaner display.  If
     *      FALSE, this routine is free to use its judgment about doing this.
     *
     * It returns TRUE if there was actually something output.  (It may be that
     * the bitmap, etc is empty.)
     *
     * When called for outputting the bitmap of a non-ANYOF node, just pass the
     * bitmap, with the succeeding parameters set to NULL, and the final one to
     * FALSE.
     */

    /* In general, it tries to display the 'cleanest' representation of the
     * innards, choosing whether to display them inverted or not, regardless of
     * whether the class itself is to be inverted.  However,  there are some
     * cases where it can't try inverting, as what actually matches isn't known
     * until runtime, and hence the inversion isn't either. */

    dVAR;
    bool inverting_allowed = ! force_as_is_display;

    int i;
    STRLEN orig_sv_cur = SvCUR(sv);

    SV* invlist;            /* Inversion list we accumulate of code points that
                               are unconditionally matched */
    SV* only_utf8 = NULL;   /* Under /d, list of matches iff the target is
                               UTF-8 */
    SV* not_utf8 =  NULL;   /* /d, list of matches iff the target isn't UTF-8
                             */
    SV* posixes = NULL;     /* Under /l, string of things like [:word:], \D */
    SV* only_utf8_locale = NULL;    /* Under /l, list of matches if the locale
                                       is UTF-8 */

    SV* as_is_display;      /* The output string when we take the inputs
                               literally */
    SV* inverted_display;   /* The output string when we invert the inputs */

    U8 flags = (node) ? ANYOF_FLAGS(node) : 0;

    bool invert = cBOOL(flags & ANYOF_INVERT);  /* Is the input to be inverted
                                                   to match? */
    /* We are biased in favor of displaying things without them being inverted,
     * as that is generally easier to understand */
    const int bias = 5;

    PERL_ARGS_ASSERT_PUT_CHARCLASS_BITMAP_INNARDS;

    /* Start off with whatever code points are passed in.  (We clone, so we
     * don't change the caller's list) */
    if (nonbitmap_invlist) {
        assert(invlist_highest(nonbitmap_invlist) < NUM_ANYOF_CODE_POINTS);
        invlist = invlist_clone(nonbitmap_invlist, NULL);
    }
    else {  /* Worst case size is every other code point is matched */
        invlist = _new_invlist(NUM_ANYOF_CODE_POINTS / 2);
    }

    if (flags) {
        if (OP(node) == ANYOFD) {

            /* This flag indicates that the code points below 0x100 in the
             * nonbitmap list are precisely the ones that match only when the
             * target is UTF-8 (they should all be non-ASCII). */
            if (flags & ANYOF_SHARED_d_UPPER_LATIN1_UTF8_STRING_MATCHES_non_d_RUNTIME_USER_PROP)
            {
                _invlist_intersection(invlist, PL_UpperLatin1, &only_utf8);
                _invlist_subtract(invlist, only_utf8, &invlist);
            }

            /* And this flag for matching all non-ASCII 0xFF and below */
            if (flags & ANYOF_SHARED_d_MATCHES_ALL_NON_UTF8_NON_ASCII_non_d_WARN_SUPER)
            {
                not_utf8 = invlist_clone(PL_UpperLatin1, NULL);
            }
        }
        else if (OP(node) == ANYOFL || OP(node) == ANYOFPOSIXL) {

            /* If either of these flags are set, what matches isn't
             * determinable except during execution, so don't know enough here
             * to invert */
            if (flags & (ANYOFL_FOLD|ANYOF_MATCHES_POSIXL)) {
                inverting_allowed = FALSE;
            }

            /* What the posix classes match also varies at runtime, so these
             * will be output symbolically. */
            if (ANYOF_POSIXL_TEST_ANY_SET(node)) {
                int i;

                posixes = newSVpvs("");
                for (i = 0; i < ANYOF_POSIXL_MAX; i++) {
                    if (ANYOF_POSIXL_TEST(node, i)) {
                        sv_catpv(posixes, anyofs[i]);
                    }
                }
            }
        }
    }

    /* Accumulate the bit map into the unconditional match list */
    if (bitmap) {
        for (i = 0; i < NUM_ANYOF_CODE_POINTS; i++) {
            if (BITMAP_TEST(bitmap, i)) {
                int start = i++;
                for (;
                     i < NUM_ANYOF_CODE_POINTS && BITMAP_TEST(bitmap, i);
                     i++)
                { /* empty */ }
                invlist = _add_range_to_invlist(invlist, start, i-1);
            }
        }
    }

    /* Make sure that the conditional match lists don't have anything in them
     * that match unconditionally; otherwise the output is quite confusing.
     * This could happen if the code that populates these misses some
     * duplication. */
    if (only_utf8) {
        _invlist_subtract(only_utf8, invlist, &only_utf8);
    }
    if (not_utf8) {
        _invlist_subtract(not_utf8, invlist, &not_utf8);
    }

    if (only_utf8_locale_invlist) {

        /* Since this list is passed in, we have to make a copy before
         * modifying it */
        only_utf8_locale = invlist_clone(only_utf8_locale_invlist, NULL);

        _invlist_subtract(only_utf8_locale, invlist, &only_utf8_locale);

        /* And, it can get really weird for us to try outputting an inverted
         * form of this list when it has things above the bitmap, so don't even
         * try */
        if (invlist_highest(only_utf8_locale) >= NUM_ANYOF_CODE_POINTS) {
            inverting_allowed = FALSE;
        }
    }

    /* Calculate what the output would be if we take the input as-is */
    as_is_display = put_charclass_bitmap_innards_common(invlist,
                                                    posixes,
                                                    only_utf8,
                                                    not_utf8,
                                                    only_utf8_locale,
                                                    invert);

    /* If have to take the output as-is, just do that */
    if (! inverting_allowed) {
        if (as_is_display) {
            sv_catsv(sv, as_is_display);
            SvREFCNT_dec_NN(as_is_display);
        }
    }
    else { /* But otherwise, create the output again on the inverted input, and
              use whichever version is shorter */

        int inverted_bias, as_is_bias;

        /* We will apply our bias to whichever of the the results doesn't have
         * the '^' */
        if (invert) {
            invert = FALSE;
            as_is_bias = bias;
            inverted_bias = 0;
        }
        else {
            invert = TRUE;
            as_is_bias = 0;
            inverted_bias = bias;
        }

        /* Now invert each of the lists that contribute to the output,
         * excluding from the result things outside the possible range */

        /* For the unconditional inversion list, we have to add in all the
         * conditional code points, so that when inverted, they will be gone
         * from it */
        _invlist_union(only_utf8, invlist, &invlist);
        _invlist_union(not_utf8, invlist, &invlist);
        _invlist_union(only_utf8_locale, invlist, &invlist);
        _invlist_invert(invlist);
        _invlist_intersection(invlist, PL_InBitmap, &invlist);

        if (only_utf8) {
            _invlist_invert(only_utf8);
            _invlist_intersection(only_utf8, PL_UpperLatin1, &only_utf8);
        }
        else if (not_utf8) {

            /* If a code point matches iff the target string is not in UTF-8,
             * then complementing the result has it not match iff not in UTF-8,
             * which is the same thing as matching iff it is UTF-8. */
            only_utf8 = not_utf8;
            not_utf8 = NULL;
        }

        if (only_utf8_locale) {
            _invlist_invert(only_utf8_locale);
            _invlist_intersection(only_utf8_locale,
                                  PL_InBitmap,
                                  &only_utf8_locale);
        }

        inverted_display = put_charclass_bitmap_innards_common(
                                            invlist,
                                            posixes,
                                            only_utf8,
                                            not_utf8,
                                            only_utf8_locale, invert);

        /* Use the shortest representation, taking into account our bias
         * against showing it inverted */
        if (   inverted_display
            && (   ! as_is_display
                || (  SvCUR(inverted_display) + inverted_bias
                    < SvCUR(as_is_display)    + as_is_bias)))
        {
	    sv_catsv(sv, inverted_display);
        }
        else if (as_is_display) {
	    sv_catsv(sv, as_is_display);
        }

        SvREFCNT_dec(as_is_display);
        SvREFCNT_dec(inverted_display);
    }

    SvREFCNT_dec_NN(invlist);
    SvREFCNT_dec(only_utf8);
    SvREFCNT_dec(not_utf8);
    SvREFCNT_dec(posixes);
    SvREFCNT_dec(only_utf8_locale);

    return SvCUR(sv) > orig_sv_cur;
}

#define CLEAR_OPTSTART                                                       \
    if (optstart) STMT_START {                                               \
        DEBUG_OPTIMISE_r(Perl_re_printf( aTHX_                                           \
                              " (%" IVdf " nodes)\n", (IV)(node - optstart))); \
        optstart=NULL;                                                       \
    } STMT_END

#define DUMPUNTIL(b,e)                                                       \
                    CLEAR_OPTSTART;                                          \
                    node=dumpuntil(r,start,(b),(e),last,sv,indent+1,depth+1);

STATIC const regnode *
S_dumpuntil(pTHX_ const regexp *r, const regnode *start, const regnode *node,
	    const regnode *last, const regnode *plast,
	    SV* sv, I32 indent, U32 depth)
{
    U8 op = PSEUDO;	/* Arbitrary non-END op. */
    const regnode *next;
    const regnode *optstart= NULL;

    RXi_GET_DECL(r, ri);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_DUMPUNTIL;

#ifdef DEBUG_DUMPUNTIL
    Perl_re_printf( aTHX_  "--- %d : %d - %d - %d\n", indent, node-start,
        last ? last-start : 0, plast ? plast-start : 0);
#endif

    if (plast && plast < last)
        last= plast;

    while (PL_regkind[op] != END && (!last || node < last)) {
        assert(node);
	/* While that wasn't END last time... */
	NODE_ALIGN(node);
	op = OP(node);
	if (op == CLOSE || op == SRCLOSE || op == WHILEM)
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

        regprop(r, sv, node, NULL, NULL);
        Perl_re_printf( aTHX_  "%4" IVdf ":%*s%s", (IV)(node - start),
		      (int)(2*indent + 1), "", SvPVX_const(sv));

        if (OP(node) != OPTIMIZED) {
            if (next == NULL)		/* Next ptr. */
                Perl_re_printf( aTHX_  " (0)");
            else if (PL_regkind[(U8)op] == BRANCH
                     && PL_regkind[OP(next)] != BRANCH )
                Perl_re_printf( aTHX_  " (FAIL)");
            else
                Perl_re_printf( aTHX_  " (%" IVdf ")", (IV)(next - start));
            Perl_re_printf( aTHX_ "\n");
        }

      after_print:
	if (PL_regkind[(U8)op] == BRANCHJ) {
	    assert(next);
	    {
                const regnode *nnode = (OP(next) == LONGJMP
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
	    AV *const trie_words
                           = MUTABLE_AV(ri->data->data[n + TRIE_WORDS_OFFSET]);
#endif
	    const regnode *nextbranch= NULL;
	    I32 word_idx;
            SvPVCLEAR(sv);
	    for (word_idx= 0; word_idx < (I32)trie->wordcount; word_idx++) {
		SV ** const elem_ptr = av_fetch(trie_words, word_idx, 0);

                Perl_re_indentf( aTHX_  "%s ",
                    indent+3,
                    elem_ptr
                    ? pv_pretty(sv, SvPV_nolen_const(*elem_ptr),
                                SvCUR(*elem_ptr), PL_dump_re_max_len,
                                PL_colors[0], PL_colors[1],
                                (SvUTF8(*elem_ptr)
                                 ? PERL_PV_ESCAPE_UNI
                                 : 0)
                                | PERL_PV_PRETTY_ELLIPSES
                                | PERL_PV_PRETTY_LTGT
                            )
                    : "???"
                );
                if (trie->jump) {
                    U16 dist= trie->jump[word_idx+1];
                    Perl_re_printf( aTHX_  "(%" UVuf ")\n",
                               (UV)((dist ? this_trie + dist : next) - start));
                    if (dist) {
                        if (!nextbranch)
                            nextbranch= this_trie + trie->jump[0];
			DUMPUNTIL(this_trie + dist, nextbranch);
                    }
                    if (nextbranch && PL_regkind[OP(nextbranch)]==BRANCH)
                        nextbranch= regnext((regnode *)nextbranch);
                } else {
                    Perl_re_printf( aTHX_  "\n");
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
	else if (PL_regkind[(U8)op] == EXACT) {
            /* Literal string, where present. */
	    node += NODE_SZ_STR(node) - 1;
	    node = NEXTOPER(node);
	}
	else {
	    node = NEXTOPER(node);
	    node += regarglen[(U8)op];
	}
	if (op == CURLYX || op == OPEN || op == SROPEN)
	    indent++;
    }
    CLEAR_OPTSTART;
#ifdef DEBUG_DUMPUNTIL
    Perl_re_printf( aTHX_  "--- %d\n", (int)indent);
#endif
    return node;
}

#endif	/* DEBUGGING */

#ifndef PERL_IN_XSUB_RE

#include "uni_keywords.h"

void
Perl_init_uniprops(pTHX)
{
    dVAR;

    PL_user_def_props = newHV();

#ifdef USE_ITHREADS

    HvSHAREKEYS_off(PL_user_def_props);
    PL_user_def_props_aTHX = aTHX;

#endif

    /* Set up the inversion list global variables */

    PL_XPosix_ptrs[_CC_ASCII] = _new_invlist_C_array(uni_prop_ptrs[UNI_ASCII]);
    PL_XPosix_ptrs[_CC_ALPHANUMERIC] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXALNUM]);
    PL_XPosix_ptrs[_CC_ALPHA] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXALPHA]);
    PL_XPosix_ptrs[_CC_BLANK] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXBLANK]);
    PL_XPosix_ptrs[_CC_CASED] =  _new_invlist_C_array(uni_prop_ptrs[UNI_CASED]);
    PL_XPosix_ptrs[_CC_CNTRL] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXCNTRL]);
    PL_XPosix_ptrs[_CC_DIGIT] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXDIGIT]);
    PL_XPosix_ptrs[_CC_GRAPH] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXGRAPH]);
    PL_XPosix_ptrs[_CC_LOWER] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXLOWER]);
    PL_XPosix_ptrs[_CC_PRINT] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXPRINT]);
    PL_XPosix_ptrs[_CC_PUNCT] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXPUNCT]);
    PL_XPosix_ptrs[_CC_SPACE] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXSPACE]);
    PL_XPosix_ptrs[_CC_UPPER] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXUPPER]);
    PL_XPosix_ptrs[_CC_VERTSPACE] = _new_invlist_C_array(uni_prop_ptrs[UNI_VERTSPACE]);
    PL_XPosix_ptrs[_CC_WORDCHAR] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXWORD]);
    PL_XPosix_ptrs[_CC_XDIGIT] = _new_invlist_C_array(uni_prop_ptrs[UNI_XPOSIXXDIGIT]);

    PL_Posix_ptrs[_CC_ASCII] = _new_invlist_C_array(uni_prop_ptrs[UNI_ASCII]);
    PL_Posix_ptrs[_CC_ALPHANUMERIC] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXALNUM]);
    PL_Posix_ptrs[_CC_ALPHA] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXALPHA]);
    PL_Posix_ptrs[_CC_BLANK] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXBLANK]);
    PL_Posix_ptrs[_CC_CASED] = PL_Posix_ptrs[_CC_ALPHA];
    PL_Posix_ptrs[_CC_CNTRL] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXCNTRL]);
    PL_Posix_ptrs[_CC_DIGIT] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXDIGIT]);
    PL_Posix_ptrs[_CC_GRAPH] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXGRAPH]);
    PL_Posix_ptrs[_CC_LOWER] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXLOWER]);
    PL_Posix_ptrs[_CC_PRINT] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXPRINT]);
    PL_Posix_ptrs[_CC_PUNCT] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXPUNCT]);
    PL_Posix_ptrs[_CC_SPACE] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXSPACE]);
    PL_Posix_ptrs[_CC_UPPER] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXUPPER]);
    PL_Posix_ptrs[_CC_VERTSPACE] = NULL;
    PL_Posix_ptrs[_CC_WORDCHAR] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXWORD]);
    PL_Posix_ptrs[_CC_XDIGIT] = _new_invlist_C_array(uni_prop_ptrs[UNI_POSIXXDIGIT]);

    PL_GCB_invlist = _new_invlist_C_array(_Perl_GCB_invlist);
    PL_SB_invlist = _new_invlist_C_array(_Perl_SB_invlist);
    PL_WB_invlist = _new_invlist_C_array(_Perl_WB_invlist);
    PL_LB_invlist = _new_invlist_C_array(_Perl_LB_invlist);
    PL_SCX_invlist = _new_invlist_C_array(_Perl_SCX_invlist);

    PL_AboveLatin1 = _new_invlist_C_array(AboveLatin1_invlist);
    PL_Latin1 = _new_invlist_C_array(Latin1_invlist);
    PL_UpperLatin1 = _new_invlist_C_array(UpperLatin1_invlist);

    PL_Assigned_invlist = _new_invlist_C_array(uni_prop_ptrs[UNI_ASSIGNED]);

    PL_utf8_perl_idstart = _new_invlist_C_array(uni_prop_ptrs[UNI__PERL_IDSTART]);
    PL_utf8_perl_idcont = _new_invlist_C_array(uni_prop_ptrs[UNI__PERL_IDCONT]);

    PL_utf8_charname_begin = _new_invlist_C_array(uni_prop_ptrs[UNI__PERL_CHARNAME_BEGIN]);
    PL_utf8_charname_continue = _new_invlist_C_array(uni_prop_ptrs[UNI__PERL_CHARNAME_CONTINUE]);

    PL_in_some_fold = _new_invlist_C_array(uni_prop_ptrs[UNI__PERL_ANY_FOLDS]);
    PL_HasMultiCharFold = _new_invlist_C_array(uni_prop_ptrs[
                                            UNI__PERL_FOLDS_TO_MULTI_CHAR]);
    PL_InMultiCharFold = _new_invlist_C_array(uni_prop_ptrs[
                                            UNI__PERL_IS_IN_MULTI_CHAR_FOLD]);
    PL_NonFinalFold = _new_invlist_C_array(uni_prop_ptrs[
                                            UNI__PERL_NON_FINAL_FOLDS]);

    PL_utf8_toupper = _new_invlist_C_array(Uppercase_Mapping_invlist);
    PL_utf8_tolower = _new_invlist_C_array(Lowercase_Mapping_invlist);
    PL_utf8_totitle = _new_invlist_C_array(Titlecase_Mapping_invlist);
    PL_utf8_tofold = _new_invlist_C_array(Case_Folding_invlist);
    PL_utf8_tosimplefold = _new_invlist_C_array(Simple_Case_Folding_invlist);
    PL_utf8_foldclosures = _new_invlist_C_array(_Perl_IVCF_invlist);
    PL_utf8_mark = _new_invlist_C_array(uni_prop_ptrs[UNI_M]);
    PL_CCC_non0_non230 = _new_invlist_C_array(_Perl_CCC_non0_non230_invlist);
    PL_Private_Use = _new_invlist_C_array(uni_prop_ptrs[UNI_CO]);

#ifdef UNI_XIDC
    /* The below are used only by deprecated functions.  They could be removed */
    PL_utf8_xidcont  = _new_invlist_C_array(uni_prop_ptrs[UNI_XIDC]);
    PL_utf8_idcont   = _new_invlist_C_array(uni_prop_ptrs[UNI_IDC]);
    PL_utf8_xidstart = _new_invlist_C_array(uni_prop_ptrs[UNI_XIDS]);
#endif
}

#if 0

This code was mainly added for backcompat to give a warning for non-portable
code points in user-defined properties.  But experiments showed that the
warning in earlier perls were only omitted on overflow, which should be an
error, so there really isnt a backcompat issue, and actually adding the
warning when none was present before might cause breakage, for little gain.  So
khw left this code in, but not enabled.  Tests were never added.

embed.fnc entry:
Ei	|const char *|get_extended_utf8_msg|const UV cp

PERL_STATIC_INLINE const char *
S_get_extended_utf8_msg(pTHX_ const UV cp)
{
    U8 dummy[UTF8_MAXBYTES + 1];
    HV *msgs;
    SV **msg;

    uvchr_to_utf8_flags_msgs(dummy, cp, UNICODE_WARN_PERL_EXTENDED,
                             &msgs);

    msg = hv_fetchs(msgs, "text", 0);
    assert(msg);

    (void) sv_2mortal((SV *) msgs);

    return SvPVX(*msg);
}

#endif

SV *
Perl_handle_user_defined_property(pTHX_

    /* Parses the contents of a user-defined property definition; returning the
     * expanded definition if possible.  If so, the return is an inversion
     * list.
     *
     * If there are subroutines that are part of the expansion and which aren't
     * known at the time of the call to this function, this returns what
     * parse_uniprop_string() returned for the first one encountered.
     *
     * If an error was found, NULL is returned, and 'msg' gets a suitable
     * message appended to it.  (Appending allows the back trace of how we got
     * to the faulty definition to be displayed through nested calls of
     * user-defined subs.)
     *
     * The caller IS responsible for freeing any returned SV.
     *
     * The syntax of the contents is pretty much described in perlunicode.pod,
     * but we also allow comments on each line */

    const char * name,          /* Name of property */
    const STRLEN name_len,      /* The name's length in bytes */
    const bool is_utf8,         /* ? Is 'name' encoded in UTF-8 */
    const bool to_fold,         /* ? Is this under /i */
    const bool runtime,         /* ? Are we in compile- or run-time */
    const bool deferrable,      /* Is it ok for this property's full definition
                                   to be deferred until later? */
    SV* contents,               /* The property's definition */
    bool *user_defined_ptr,     /* This will be set TRUE as we wouldn't be
                                   getting called unless this is thought to be
                                   a user-defined property */
    SV * msg,                   /* Any error or warning msg(s) are appended to
                                   this */
    const STRLEN level)         /* Recursion level of this call */
{
    STRLEN len;
    const char * string         = SvPV_const(contents, len);
    const char * const e        = string + len;
    const bool is_contents_utf8 = cBOOL(SvUTF8(contents));
    const STRLEN msgs_length_on_entry = SvCUR(msg);

    const char * s0 = string;   /* Points to first byte in the current line
                                   being parsed in 'string' */
    const char overflow_msg[] = "Code point too large in \"";
    SV* running_definition = NULL;

    PERL_ARGS_ASSERT_HANDLE_USER_DEFINED_PROPERTY;

    *user_defined_ptr = TRUE;

    /* Look at each line */
    while (s0 < e) {
        const char * s;     /* Current byte */
        char op = '+';      /* Default operation is 'union' */
        IV   min = 0;       /* range begin code point */
        IV   max = -1;      /* and range end */
        SV* this_definition;

        /* Skip comment lines */
        if (*s0 == '#') {
            s0 = strchr(s0, '\n');
            if (s0 == NULL) {
                break;
            }
            s0++;
            continue;
        }

        /* For backcompat, allow an empty first line */
        if (*s0 == '\n') {
            s0++;
            continue;
        }

        /* First character in the line may optionally be the operation */
        if (   *s0 == '+'
            || *s0 == '!'
            || *s0 == '-'
            || *s0 == '&')
        {
            op = *s0++;
        }

        /* If the line is one or two hex digits separated by blank space, its
         * a range; otherwise it is either another user-defined property or an
         * error */

        s = s0;

        if (! isXDIGIT(*s)) {
            goto check_if_property;
        }

        do { /* Each new hex digit will add 4 bits. */
            if (min > ( (IV) MAX_LEGAL_CP >> 4)) {
                s = strchr(s, '\n');
                if (s == NULL) {
                    s = e;
                }
                if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                sv_catpv(msg, overflow_msg);
                Perl_sv_catpvf(aTHX_ msg, "%" UTF8f,
                                     UTF8fARG(is_contents_utf8, s - s0, s0));
                sv_catpvs(msg, "\"");
                goto return_failure;
            }

            /* Accumulate this digit into the value */
            min = (min << 4) + READ_XDIGIT(s);
        } while (isXDIGIT(*s));

        while (isBLANK(*s)) { s++; }

        /* We allow comments at the end of the line */
        if (*s == '#') {
            s = strchr(s, '\n');
            if (s == NULL) {
                s = e;
            }
            s++;
        }
        else if (s < e && *s != '\n') {
            if (! isXDIGIT(*s)) {
                goto check_if_property;
            }

            /* Look for the high point of the range */
            max = 0;
            do {
                if (max > ( (IV) MAX_LEGAL_CP >> 4)) {
                    s = strchr(s, '\n');
                    if (s == NULL) {
                        s = e;
                    }
                    if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                    sv_catpv(msg, overflow_msg);
                    Perl_sv_catpvf(aTHX_ msg, "%" UTF8f,
                                      UTF8fARG(is_contents_utf8, s - s0, s0));
                    sv_catpvs(msg, "\"");
                    goto return_failure;
                }

                max = (max << 4) + READ_XDIGIT(s);
            } while (isXDIGIT(*s));

            while (isBLANK(*s)) { s++; }

            if (*s == '#') {
                s = strchr(s, '\n');
                if (s == NULL) {
                    s = e;
                }
            }
            else if (s < e && *s != '\n') {
                goto check_if_property;
            }
        }

        if (max == -1) {    /* The line only had one entry */
            max = min;
        }
        else if (max < min) {
            if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
            sv_catpvs(msg, "Illegal range in \"");
            Perl_sv_catpvf(aTHX_ msg, "%" UTF8f,
                                UTF8fARG(is_contents_utf8, s - s0, s0));
            sv_catpvs(msg, "\"");
            goto return_failure;
        }

#if 0   /* See explanation at definition above of get_extended_utf8_msg() */

        if (   UNICODE_IS_PERL_EXTENDED(min)
            || UNICODE_IS_PERL_EXTENDED(max))
        {
            if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");

            /* If both code points are non-portable, warn only on the lower
             * one. */
            sv_catpv(msg, get_extended_utf8_msg(
                                            (UNICODE_IS_PERL_EXTENDED(min))
                                            ? min : max));
            sv_catpvs(msg, " in \"");
            Perl_sv_catpvf(aTHX_ msg, "%" UTF8f,
                                 UTF8fARG(is_contents_utf8, s - s0, s0));
            sv_catpvs(msg, "\"");
        }

#endif

        /* Here, this line contains a legal range */
        this_definition = sv_2mortal(_new_invlist(2));
        this_definition = _add_range_to_invlist(this_definition, min, max);
        goto calculate;

      check_if_property:

        /* Here it isn't a legal range line.  See if it is a legal property
         * line.  First find the end of the meat of the line */
        s = strpbrk(s, "#\n");
        if (s == NULL) {
            s = e;
        }

        /* Ignore trailing blanks in keeping with the requirements of
         * parse_uniprop_string() */
        s--;
        while (s > s0 && isBLANK_A(*s)) {
            s--;
        }
        s++;

        this_definition = parse_uniprop_string(s0, s - s0,
                                               is_utf8, to_fold, runtime,
                                               deferrable,
                                               user_defined_ptr, msg,
                                               (name_len == 0)
                                                ? level /* Don't increase level
                                                           if input is empty */
                                                : level + 1
                                              );
        if (this_definition == NULL) {
            goto return_failure;    /* 'msg' should have had the reason
                                       appended to it by the above call */
        }

        if (! is_invlist(this_definition)) {    /* Unknown at this time */
            return newSVsv(this_definition);
        }

        if (*s != '\n') {
            s = strchr(s, '\n');
            if (s == NULL) {
                s = e;
            }
        }

      calculate:

        switch (op) {
            case '+':
                _invlist_union(running_definition, this_definition,
                                                        &running_definition);
                break;
            case '-':
                _invlist_subtract(running_definition, this_definition,
                                                        &running_definition);
                break;
            case '&':
                _invlist_intersection(running_definition, this_definition,
                                                        &running_definition);
                break;
            case '!':
                _invlist_union_complement_2nd(running_definition,
                                        this_definition, &running_definition);
                break;
            default:
                Perl_croak(aTHX_ "panic: %s: %d: Unexpected operation %d",
                                 __FILE__, __LINE__, op);
                break;
        }

        /* Position past the '\n' */
        s0 = s + 1;
    }   /* End of loop through the lines of 'contents' */

    /* Here, we processed all the lines in 'contents' without error.  If we
     * didn't add any warnings, simply return success */
    if (msgs_length_on_entry == SvCUR(msg)) {

        /* If the expansion was empty, the answer isn't nothing: its an empty
         * inversion list */
        if (running_definition == NULL) {
            running_definition = _new_invlist(1);
        }

        return running_definition;
    }

    /* Otherwise, add some explanatory text, but we will return success */
    goto return_msg;

  return_failure:
    running_definition = NULL;

  return_msg:

    if (name_len > 0) {
        sv_catpvs(msg, " in expansion of ");
        Perl_sv_catpvf(aTHX_ msg, "%" UTF8f, UTF8fARG(is_utf8, name_len, name));
    }

    return running_definition;
}

/* As explained below, certain operations need to take place in the first
 * thread created.  These macros switch contexts */
#ifdef USE_ITHREADS
#  define DECLARATION_FOR_GLOBAL_CONTEXT                                    \
                                        PerlInterpreter * save_aTHX = aTHX;
#  define SWITCH_TO_GLOBAL_CONTEXT                                          \
                           PERL_SET_CONTEXT((aTHX = PL_user_def_props_aTHX))
#  define RESTORE_CONTEXT  PERL_SET_CONTEXT((aTHX = save_aTHX));
#  define CUR_CONTEXT      aTHX
#  define ORIGINAL_CONTEXT save_aTHX
#else
#  define DECLARATION_FOR_GLOBAL_CONTEXT
#  define SWITCH_TO_GLOBAL_CONTEXT          NOOP
#  define RESTORE_CONTEXT                   NOOP
#  define CUR_CONTEXT                       NULL
#  define ORIGINAL_CONTEXT                  NULL
#endif

STATIC void
S_delete_recursion_entry(pTHX_ void *key)
{
    /* Deletes the entry used to detect recursion when expanding user-defined
     * properties.  This is a function so it can be set up to be called even if
     * the program unexpectedly quits */

    dVAR;
    SV ** current_entry;
    const STRLEN key_len = strlen((const char *) key);
    DECLARATION_FOR_GLOBAL_CONTEXT;

    SWITCH_TO_GLOBAL_CONTEXT;

    /* If the entry is one of these types, it is a permanent entry, and not the
     * one used to detect recursions.  This function should delete only the
     * recursion entry */
    current_entry = hv_fetch(PL_user_def_props, (const char *) key, key_len, 0);
    if (     current_entry
        && ! is_invlist(*current_entry)
        && ! SvPOK(*current_entry))
    {
        (void) hv_delete(PL_user_def_props, (const char *) key, key_len,
                                                                    G_DISCARD);
    }

    RESTORE_CONTEXT;
}

STATIC SV *
S_get_fq_name(pTHX_
              const char * const name,    /* The first non-blank in the \p{}, \P{} */
              const Size_t name_len,      /* Its length in bytes, not including any trailing space */
              const bool is_utf8,         /* ? Is 'name' encoded in UTF-8 */
              const bool has_colon_colon
             )
{
    /* Returns a mortal SV containing the fully qualified version of the input
     * name */

    SV * fq_name;

    fq_name = newSVpvs_flags("", SVs_TEMP);

    /* Use the current package if it wasn't included in our input */
    if (! has_colon_colon) {
        const HV * pkg = (IN_PERL_COMPILETIME)
                         ? PL_curstash
                         : CopSTASH(PL_curcop);
        const char* pkgname = HvNAME(pkg);

        Perl_sv_catpvf(aTHX_ fq_name, "%" UTF8f,
                      UTF8fARG(is_utf8, strlen(pkgname), pkgname));
        sv_catpvs(fq_name, "::");
    }

    Perl_sv_catpvf(aTHX_ fq_name, "%" UTF8f,
                         UTF8fARG(is_utf8, name_len, name));
    return fq_name;
}

SV *
Perl_parse_uniprop_string(pTHX_

    /* Parse the interior of a \p{}, \P{}.  Returns its definition if knowable
     * now.  If so, the return is an inversion list.
     *
     * If the property is user-defined, it is a subroutine, which in turn
     * may call other subroutines.  This function will call the whole nest of
     * them to get the definition they return; if some aren't known at the time
     * of the call to this function, the fully qualified name of the highest
     * level sub is returned.  It is an error to call this function at runtime
     * without every sub defined.
     *
     * If an error was found, NULL is returned, and 'msg' gets a suitable
     * message appended to it.  (Appending allows the back trace of how we got
     * to the faulty definition to be displayed through nested calls of
     * user-defined subs.)
     *
     * The caller should NOT try to free any returned inversion list.
     *
     * Other parameters will be set on return as described below */

    const char * const name,    /* The first non-blank in the \p{}, \P{} */
    const Size_t name_len,      /* Its length in bytes, not including any
                                   trailing space */
    const bool is_utf8,         /* ? Is 'name' encoded in UTF-8 */
    const bool to_fold,         /* ? Is this under /i */
    const bool runtime,         /* TRUE if this is being called at run time */
    const bool deferrable,      /* TRUE if it's ok for the definition to not be
                                   known at this call */
    bool *user_defined_ptr,     /* Upon return from this function it will be
                                   set to TRUE if any component is a
                                   user-defined property */
    SV * msg,                   /* Any error or warning msg(s) are appended to
                                   this */
   const STRLEN level)          /* Recursion level of this call */
{
    dVAR;
    char* lookup_name;          /* normalized name for lookup in our tables */
    unsigned lookup_len;        /* Its length */
    bool stricter = FALSE;      /* Some properties have stricter name
                                   normalization rules, which we decide upon
                                   based on parsing */

    /* nv= or numeric_value=, or possibly one of the cjk numeric properties
     * (though it requires extra effort to download them from Unicode and
     * compile perl to know about them) */
    bool is_nv_type = FALSE;

    unsigned int i, j = 0;
    int equals_pos = -1;    /* Where the '=' is found, or negative if none */
    int slash_pos  = -1;    /* Where the '/' is found, or negative if none */
    int table_index = 0;    /* The entry number for this property in the table
                               of all Unicode property names */
    bool starts_with_In_or_Is = FALSE;  /* ? Does the name start with 'In' or
                                             'Is' */
    Size_t lookup_offset = 0;   /* Used to ignore the first few characters of
                                   the normalized name in certain situations */
    Size_t non_pkg_begin = 0;   /* Offset of first byte in 'name' that isn't
                                   part of a package name */
    bool could_be_user_defined = TRUE;  /* ? Could this be a user-defined
                                             property rather than a Unicode
                                             one. */
    SV * prop_definition = NULL;  /* The returned definition of 'name' or NULL
                                     if an error.  If it is an inversion list,
                                     it is the definition.  Otherwise it is a
                                     string containing the fully qualified sub
                                     name of 'name' */
    SV * fq_name = NULL;        /* For user-defined properties, the fully
                                   qualified name */
    bool invert_return = FALSE; /* ? Do we need to complement the result before
                                     returning it */

    PERL_ARGS_ASSERT_PARSE_UNIPROP_STRING;

    /* The input will be normalized into 'lookup_name' */
    Newx(lookup_name, name_len, char);
    SAVEFREEPV(lookup_name);

    /* Parse the input. */
    for (i = 0; i < name_len; i++) {
        char cur = name[i];

        /* Most of the characters in the input will be of this ilk, being parts
         * of a name */
        if (isIDCONT_A(cur)) {

            /* Case differences are ignored.  Our lookup routine assumes
             * everything is lowercase, so normalize to that */
            if (isUPPER_A(cur)) {
                lookup_name[j++] = toLOWER_A(cur);
                continue;
            }

            if (cur == '_') { /* Don't include these in the normalized name */
                continue;
            }

            lookup_name[j++] = cur;

            /* The first character in a user-defined name must be of this type.
             * */
            if (i - non_pkg_begin == 0 && ! isIDFIRST_A(cur)) {
                could_be_user_defined = FALSE;
            }

            continue;
        }

        /* Here, the character is not something typically in a name,  But these
         * two types of characters (and the '_' above) can be freely ignored in
         * most situations.  Later it may turn out we shouldn't have ignored
         * them, and we have to reparse, but we don't have enough information
         * yet to make that decision */
        if (cur == '-' || isSPACE_A(cur)) {
            could_be_user_defined = FALSE;
            continue;
        }

        /* An equals sign or single colon mark the end of the first part of
         * the property name */
        if (    cur == '='
            || (cur == ':' && (i >= name_len - 1 || name[i+1] != ':')))
        {
            lookup_name[j++] = '='; /* Treat the colon as an '=' */
            equals_pos = j; /* Note where it occurred in the input */
            could_be_user_defined = FALSE;
            break;
        }

        /* Otherwise, this character is part of the name. */
        lookup_name[j++] = cur;

        /* Here it isn't a single colon, so if it is a colon, it must be a
         * double colon */
        if (cur == ':') {

            /* A double colon should be a package qualifier.  We note its
             * position and continue.  Note that one could have
             *      pkg1::pkg2::...::foo
             * so that the position at the end of the loop will be just after
             * the final qualifier */

            i++;
            non_pkg_begin = i + 1;
            lookup_name[j++] = ':';
        }
        else { /* Only word chars (and '::') can be in a user-defined name */
            could_be_user_defined = FALSE;
        }
    } /* End of parsing through the lhs of the property name (or all of it if
         no rhs) */

#define STRLENs(s)  (sizeof("" s "") - 1)

    /* If there is a single package name 'utf8::', it is ambiguous.  It could
     * be for a user-defined property, or it could be a Unicode property, as
     * all of them are considered to be for that package.  For the purposes of
     * parsing the rest of the property, strip it off */
    if (non_pkg_begin == STRLENs("utf8::") && memBEGINPs(name, name_len, "utf8::")) {
        lookup_name +=  STRLENs("utf8::");
        j -=  STRLENs("utf8::");
        equals_pos -=  STRLENs("utf8::");
    }

    /* Here, we are either done with the whole property name, if it was simple;
     * or are positioned just after the '=' if it is compound. */

    if (equals_pos >= 0) {
        assert(! stricter); /* We shouldn't have set this yet */

        /* Space immediately after the '=' is ignored */
        i++;
        for (; i < name_len; i++) {
            if (! isSPACE_A(name[i])) {
                break;
            }
        }

        /* Most punctuation after the equals indicates a subpattern, like
         * \p{foo=/bar/} */
        if (   isPUNCT_A(name[i])
            && name[i] != '-'
            && name[i] != '+'
            && name[i] != '_'
            && name[i] != '{')
        {
            /* Find the property.  The table includes the equals sign, so we
             * use 'j' as-is */
            table_index = match_uniprop((U8 *) lookup_name, j);
            if (table_index) {
                const char * const * prop_values
                                            = UNI_prop_value_ptrs[table_index];
                SV * subpattern;
                Size_t subpattern_len;
                REGEXP * subpattern_re;
                char open = name[i++];
                char close;
                const char * pos_in_brackets;
                bool escaped = 0;

                /* A backslash means the real delimitter is the next character.
                 * */
                if (open == '\\') {
                    open = name[i++];
                    escaped = 1;
                }

                /* This data structure is constructed so that the matching
                 * closing bracket is 3 past its matching opening.  The second
                 * set of closing is so that if the opening is something like
                 * ']', the closing will be that as well.  Something similar is
                 * done in toke.c */
                pos_in_brackets = strchr("([<)]>)]>", open);
                close = (pos_in_brackets) ? pos_in_brackets[3] : open;

                if (    i >= name_len
                    ||  name[name_len-1] != close
                    || (escaped && name[name_len-2] != '\\'))
                {
                    sv_catpvs(msg, "Unicode property wildcard not terminated");
                    goto append_name_to_msg;
                }

                Perl_ck_warner_d(aTHX_
                    packWARN(WARN_EXPERIMENTAL__UNIPROP_WILDCARDS),
                    "The Unicode property wildcards feature is experimental");

                /* Now create and compile the wildcard subpattern.  Use /iaa
                 * because nothing outside of ASCII will match, and it the
                 * property values should all match /i.  Note that when the
                 * pattern fails to compile, our added text to the user's
                 * pattern will be displayed to the user, which is not so
                 * desirable. */
                subpattern_len = name_len - i - 1 - escaped;
                subpattern = Perl_newSVpvf(aTHX_ "(?iaa:%.*s)",
                                              (unsigned) subpattern_len,
                                              name + i);
                subpattern = sv_2mortal(subpattern);
                subpattern_re = re_compile(subpattern, 0);
                assert(subpattern_re);  /* Should have died if didn't compile
                                         successfully */

                /* For each legal property value, see if the supplied pattern
                 * matches it. */
                while (*prop_values) {
                    const char * const entry = *prop_values;
                    const Size_t len = strlen(entry);
                    SV* entry_sv = newSVpvn_flags(entry, len, SVs_TEMP);

                    if (pregexec(subpattern_re,
                                 (char *) entry,
                                 (char *) entry + len,
                                 (char *) entry, 0,
                                 entry_sv,
                                 0))
                    { /* Here, matched.  Add to the returned list */
                        Size_t total_len = j + len;
                        SV * sub_invlist = NULL;
                        char * this_string;

                        /* We know this is a legal \p{property=value}.  Call
                         * the function to return the list of code points that
                         * match it */
                        Newxz(this_string, total_len + 1, char);
                        Copy(lookup_name, this_string, j, char);
                        my_strlcat(this_string, entry, total_len + 1);
                        SAVEFREEPV(this_string);
                        sub_invlist = parse_uniprop_string(this_string,
                                                           total_len,
                                                           is_utf8,
                                                           to_fold,
                                                           runtime,
                                                           deferrable,
                                                           user_defined_ptr,
                                                           msg,
                                                           level + 1);
                        _invlist_union(prop_definition, sub_invlist,
                                       &prop_definition);
                    }

                    prop_values++;  /* Next iteration, look at next propvalue */
                } /* End of looking through property values; (the data
                     structure is terminated by a NULL ptr) */

                SvREFCNT_dec_NN(subpattern_re);

                if (prop_definition) {
                    return prop_definition;
                }

                sv_catpvs(msg, "No Unicode property value wildcard matches:");
                goto append_name_to_msg;
            }

            /* Here's how khw thinks we should proceed to handle the properties
             * not yet done:    Bidi Mirroring Glyph
                                Bidi Paired Bracket
                                Case Folding  (both full and simple)
                                Decomposition Mapping
                                Equivalent Unified Ideograph
                                Name
                                Name Alias
                                Lowercase Mapping  (both full and simple)
                                NFKC Case Fold
                                Titlecase Mapping  (both full and simple)
                                Uppercase Mapping  (both full and simple)
             * Move the part that looks at the property values into a perl
             * script, like utf8_heavy.pl is done.  This makes things somewhat
             * easier, but most importantly, it avoids always adding all these
             * strings to the memory usage when the feature is little-used.
             *
             * The property values would all be concatenated into a single
             * string per property with each value on a separate line, and the
             * code point it's for on alternating lines.  Then we match the
             * user's input pattern m//mg, without having to worry about their
             * uses of '^' and '$'.  Only the values that aren't the default
             * would be in the strings.  Code points would be in UTF-8.  The
             * search pattern that we would construct would look like
             * (?: \n (code-point_re) \n (?aam: user-re ) \n )
             * And so $1 would contain the code point that matched the user-re.
             * For properties where the default is the code point itself, such
             * as any of the case changing mappings, the string would otherwise
             * consist of all Unicode code points in UTF-8 strung together.
             * This would be impractical.  So instead, examine their compiled
             * pattern, looking at the ssc.  If none, reject the pattern as an
             * error.  Otherwise run the pattern against every code point in
             * the ssc.  The ssc is kind of like tr18's 3.9 Possible Match Sets
             * And it might be good to create an API to return the ssc.
             *
             * For the name properties, a new function could be created in
             * charnames which essentially does the same thing as above,
             * sharing Name.pl with the other charname functions.  Don't know
             * about loose name matching, or algorithmically determined names.
             * Decomposition.pl similarly.
             *
             * It might be that a new pattern modifier would have to be
             * created, like /t for resTricTed, which changed the behavior of
             * some constructs in their subpattern, like \A. */
        } /* End of is a wildcard subppattern */


        /* Certain properties whose values are numeric need special handling.
         * They may optionally be prefixed by 'is'.  Ignore that prefix for the
         * purposes of checking if this is one of those properties */
        if (memBEGINPs(lookup_name, j, "is")) {
            lookup_offset = 2;
        }

        /* Then check if it is one of these specially-handled properties.  The
         * possibilities are hard-coded because easier this way, and the list
         * is unlikely to change.
         *
         * All numeric value type properties are of this ilk, and are also
         * special in a different way later on.  So find those first.  There
         * are several numeric value type properties in the Unihan DB (which is
         * unlikely to be compiled with perl, but we handle it here in case it
         * does get compiled).  They all end with 'numeric'.  The interiors
         * aren't checked for the precise property.  This would stop working if
         * a cjk property were to be created that ended with 'numeric' and
         * wasn't a numeric type */
        is_nv_type = memEQs(lookup_name + lookup_offset,
                       j - 1 - lookup_offset, "numericvalue")
                  || memEQs(lookup_name + lookup_offset,
                      j - 1 - lookup_offset, "nv")
                  || (   memENDPs(lookup_name + lookup_offset,
                            j - 1 - lookup_offset, "numeric")
                      && (   memBEGINPs(lookup_name + lookup_offset,
                                      j - 1 - lookup_offset, "cjk")
                          || memBEGINPs(lookup_name + lookup_offset,
                                      j - 1 - lookup_offset, "k")));
        if (   is_nv_type
            || memEQs(lookup_name + lookup_offset,
                      j - 1 - lookup_offset, "canonicalcombiningclass")
            || memEQs(lookup_name + lookup_offset,
                      j - 1 - lookup_offset, "ccc")
            || memEQs(lookup_name + lookup_offset,
                      j - 1 - lookup_offset, "age")
            || memEQs(lookup_name + lookup_offset,
                      j - 1 - lookup_offset, "in")
            || memEQs(lookup_name + lookup_offset,
                      j - 1 - lookup_offset, "presentin"))
        {
            unsigned int k;

            /* Since the stuff after the '=' is a number, we can't throw away
             * '-' willy-nilly, as those could be a minus sign.  Other stricter
             * rules also apply.  However, these properties all can have the
             * rhs not be a number, in which case they contain at least one
             * alphabetic.  In those cases, the stricter rules don't apply.
             * But the numeric type properties can have the alphas [Ee] to
             * signify an exponent, and it is still a number with stricter
             * rules.  So look for an alpha that signifies not-strict */
            stricter = TRUE;
            for (k = i; k < name_len; k++) {
                if (   isALPHA_A(name[k])
                    && (! is_nv_type || ! isALPHA_FOLD_EQ(name[k], 'E')))
                {
                    stricter = FALSE;
                    break;
                }
            }
        }

        if (stricter) {

            /* A number may have a leading '+' or '-'.  The latter is retained
             * */
            if (name[i] == '+') {
                i++;
            }
            else if (name[i] == '-') {
                lookup_name[j++] = '-';
                i++;
            }

            /* Skip leading zeros including single underscores separating the
             * zeros, or between the final leading zero and the first other
             * digit */
            for (; i < name_len - 1; i++) {
                if (    name[i] != '0'
                    && (name[i] != '_' || ! isDIGIT_A(name[i+1])))
                {
                    break;
                }
            }
        }
    }
    else {  /* No '=' */

       /* Only a few properties without an '=' should be parsed with stricter
        * rules.  The list is unlikely to change. */
        if (   memBEGINPs(lookup_name, j, "perl")
            && memNEs(lookup_name + 4, j - 4, "space")
            && memNEs(lookup_name + 4, j - 4, "word"))
        {
            stricter = TRUE;

            /* We set the inputs back to 0 and the code below will reparse,
             * using strict */
            i = j = 0;
        }
    }

    /* Here, we have either finished the property, or are positioned to parse
     * the remainder, and we know if stricter rules apply.  Finish out, if not
     * already done */
    for (; i < name_len; i++) {
        char cur = name[i];

        /* In all instances, case differences are ignored, and we normalize to
         * lowercase */
        if (isUPPER_A(cur)) {
            lookup_name[j++] = toLOWER(cur);
            continue;
        }

        /* An underscore is skipped, but not under strict rules unless it
         * separates two digits */
        if (cur == '_') {
            if (    stricter
                && (     i == 0 || (int) i == equals_pos || i == name_len- 1
                    || ! isDIGIT_A(name[i-1]) || ! isDIGIT_A(name[i+1])))
            {
                lookup_name[j++] = '_';
            }
            continue;
        }

        /* Hyphens are skipped except under strict */
        if (cur == '-' && ! stricter) {
            continue;
        }

        /* XXX Bug in documentation.  It says white space skipped adjacent to
         * non-word char.  Maybe we should, but shouldn't skip it next to a dot
         * in a number */
        if (isSPACE_A(cur) && ! stricter) {
            continue;
        }

        lookup_name[j++] = cur;

        /* Unless this is a non-trailing slash, we are done with it */
        if (i >= name_len - 1 || cur != '/') {
            continue;
        }

        slash_pos = j;

        /* A slash in the 'numeric value' property indicates that what follows
         * is a denominator.  It can have a leading '+' and '0's that should be
         * skipped.  But we have never allowed a negative denominator, so treat
         * a minus like every other character.  (No need to rule out a second
         * '/', as that won't match anything anyway */
        if (is_nv_type) {
            i++;
            if (i < name_len && name[i] == '+') {
                i++;
            }

            /* Skip leading zeros including underscores separating digits */
            for (; i < name_len - 1; i++) {
                if (   name[i] != '0'
                    && (name[i] != '_' || ! isDIGIT_A(name[i+1])))
                {
                    break;
                }
            }

            /* Store the first real character in the denominator */
            lookup_name[j++] = name[i];
        }
    }

    /* Here are completely done parsing the input 'name', and 'lookup_name'
     * contains a copy, normalized.
     *
     * This special case is grandfathered in: 'L_' and 'GC=L_' are accepted and
     * different from without the underscores.  */
    if (  (   UNLIKELY(memEQs(lookup_name, j, "l"))
           || UNLIKELY(memEQs(lookup_name, j, "gc=l")))
        && UNLIKELY(name[name_len-1] == '_'))
    {
        lookup_name[j++] = '&';
    }

    /* If the original input began with 'In' or 'Is', it could be a subroutine
     * call to a user-defined property instead of a Unicode property name. */
    if (    non_pkg_begin + name_len > 2
        &&  name[non_pkg_begin+0] == 'I'
        && (name[non_pkg_begin+1] == 'n' || name[non_pkg_begin+1] == 's'))
    {
        starts_with_In_or_Is = TRUE;
    }
    else {
        could_be_user_defined = FALSE;
    }

    if (could_be_user_defined) {
        CV* user_sub;

        /* If the user defined property returns the empty string, it could
         * easily be because the pattern is being compiled before the data it
         * actually needs to compile is available.  This could be argued to be
         * a bug in the perl code, but this is a change of behavior for Perl,
         * so we handle it.  This means that intentionally returning nothing
         * will not be resolved until runtime */
        bool empty_return = FALSE;

        /* Here, the name could be for a user defined property, which are
         * implemented as subs. */
        user_sub = get_cvn_flags(name, name_len, 0);
        if (user_sub) {
            const char insecure[] = "Insecure user-defined property";

            /* Here, there is a sub by the correct name.  Normally we call it
             * to get the property definition */
            dSP;
            SV * user_sub_sv = MUTABLE_SV(user_sub);
            SV * error;     /* Any error returned by calling 'user_sub' */
            SV * key;       /* The key into the hash of user defined sub names
                             */
            SV * placeholder;
            SV ** saved_user_prop_ptr;      /* Hash entry for this property */

            /* How many times to retry when another thread is in the middle of
             * expanding the same definition we want */
            PERL_INT_FAST8_T retry_countdown = 10;

            DECLARATION_FOR_GLOBAL_CONTEXT;

            /* If we get here, we know this property is user-defined */
            *user_defined_ptr = TRUE;

            /* We refuse to call a potentially tainted subroutine; returning an
             * error instead */
            if (TAINT_get) {
                if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                sv_catpvn(msg, insecure, sizeof(insecure) - 1);
                goto append_name_to_msg;
            }

            /* In principal, we only call each subroutine property definition
             * once during the life of the program.  This guarantees that the
             * property definition never changes.  The results of the single
             * sub call are stored in a hash, which is used instead for future
             * references to this property.  The property definition is thus
             * immutable.  But, to allow the user to have a /i-dependent
             * definition, we call the sub once for non-/i, and once for /i,
             * should the need arise, passing the /i status as a parameter.
             *
             * We start by constructing the hash key name, consisting of the
             * fully qualified subroutine name, preceded by the /i status, so
             * that there is a key for /i and a different key for non-/i */
            key = newSVpvn(((to_fold) ? "1" : "0"), 1);
            fq_name = S_get_fq_name(aTHX_ name, name_len, is_utf8,
                                          non_pkg_begin != 0);
            sv_catsv(key, fq_name);
            sv_2mortal(key);

            /* We only call the sub once throughout the life of the program
             * (with the /i, non-/i exception noted above).  That means the
             * hash must be global and accessible to all threads.  It is
             * created at program start-up, before any threads are created, so
             * is accessible to all children.  But this creates some
             * complications.
             *
             * 1) The keys can't be shared, or else problems arise; sharing is
             *    turned off at hash creation time
             * 2) All SVs in it are there for the remainder of the life of the
             *    program, and must be created in the same interpreter context
             *    as the hash, or else they will be freed from the wrong pool
             *    at global destruction time.  This is handled by switching to
             *    the hash's context to create each SV going into it, and then
             *    immediately switching back
             * 3) All accesses to the hash must be controlled by a mutex, to
             *    prevent two threads from getting an unstable state should
             *    they simultaneously be accessing it.  The code below is
             *    crafted so that the mutex is locked whenever there is an
             *    access and unlocked only when the next stable state is
             *    achieved.
             *
             * The hash stores either the definition of the property if it was
             * valid, or, if invalid, the error message that was raised.  We
             * use the type of SV to distinguish.
             *
             * There's also the need to guard against the definition expansion
             * from infinitely recursing.  This is handled by storing the aTHX
             * of the expanding thread during the expansion.  Again the SV type
             * is used to distinguish this from the other two cases.  If we
             * come to here and the hash entry for this property is our aTHX,
             * it means we have recursed, and the code assumes that we would
             * infinitely recurse, so instead stops and raises an error.
             * (Any recursion has always been treated as infinite recursion in
             * this feature.)
             *
             * If instead, the entry is for a different aTHX, it means that
             * that thread has gotten here first, and hasn't finished expanding
             * the definition yet.  We just have to wait until it is done.  We
             * sleep and retry a few times, returning an error if the other
             * thread doesn't complete. */

          re_fetch:
            USER_PROP_MUTEX_LOCK;

            /* If we have an entry for this key, the subroutine has already
             * been called once with this /i status. */
            saved_user_prop_ptr = hv_fetch(PL_user_def_props,
                                                   SvPVX(key), SvCUR(key), 0);
            if (saved_user_prop_ptr) {

                /* If the saved result is an inversion list, it is the valid
                 * definition of this property */
                if (is_invlist(*saved_user_prop_ptr)) {
                    prop_definition = *saved_user_prop_ptr;

                    /* The SV in the hash won't be removed until global
                     * destruction, so it is stable and we can unlock */
                    USER_PROP_MUTEX_UNLOCK;

                    /* The caller shouldn't try to free this SV */
                    return prop_definition;
                }

                /* Otherwise, if it is a string, it is the error message
                 * that was returned when we first tried to evaluate this
                 * property.  Fail, and append the message */
                if (SvPOK(*saved_user_prop_ptr)) {
                    if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                    sv_catsv(msg, *saved_user_prop_ptr);

                    /* The SV in the hash won't be removed until global
                     * destruction, so it is stable and we can unlock */
                    USER_PROP_MUTEX_UNLOCK;

                    return NULL;
                }

                assert(SvIOK(*saved_user_prop_ptr));

                /* Here, we have an unstable entry in the hash.  Either another
                 * thread is in the middle of expanding the property's
                 * definition, or we are ourselves recursing.  We use the aTHX
                 * in it to distinguish */
                if (SvIV(*saved_user_prop_ptr) != PTR2IV(CUR_CONTEXT)) {

                    /* Here, it's another thread doing the expanding.  We've
                     * looked as much as we are going to at the contents of the
                     * hash entry.  It's safe to unlock. */
                    USER_PROP_MUTEX_UNLOCK;

                    /* Retry a few times */
                    if (retry_countdown-- > 0) {
                        PerlProc_sleep(1);
                        goto re_fetch;
                    }

                    if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                    sv_catpvs(msg, "Timeout waiting for another thread to "
                                   "define");
                    goto append_name_to_msg;
                }

                /* Here, we are recursing; don't dig any deeper */
                USER_PROP_MUTEX_UNLOCK;

                if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                sv_catpvs(msg,
                          "Infinite recursion in user-defined property");
                goto append_name_to_msg;
            }

            /* Here, this thread has exclusive control, and there is no entry
             * for this property in the hash.  So we have the go ahead to
             * expand the definition ourselves. */

            PUSHSTACKi(PERLSI_MAGIC);
            ENTER;

            /* Create a temporary placeholder in the hash to detect recursion
             * */
            SWITCH_TO_GLOBAL_CONTEXT;
            placeholder= newSVuv(PTR2IV(ORIGINAL_CONTEXT));
            (void) hv_store_ent(PL_user_def_props, key, placeholder, 0);
            RESTORE_CONTEXT;

            /* Now that we have a placeholder, we can let other threads
             * continue */
            USER_PROP_MUTEX_UNLOCK;

            /* Make sure the placeholder always gets destroyed */
            SAVEDESTRUCTOR_X(S_delete_recursion_entry, SvPVX(key));

            PUSHMARK(SP);
            SAVETMPS;

            /* Call the user's function, with the /i status as a parameter.
             * Note that we have gone to a lot of trouble to keep this call
             * from being within the locked mutex region. */
            XPUSHs(boolSV(to_fold));
            PUTBACK;

            /* The following block was taken from swash_init().  Presumably
             * they apply to here as well, though we no longer use a swash --
             * khw */
            SAVEHINTS();
            save_re_context();
            /* We might get here via a subroutine signature which uses a utf8
             * parameter name, at which point PL_subname will have been set
             * but not yet used. */
            save_item(PL_subname);

            (void) call_sv(user_sub_sv, G_EVAL|G_SCALAR);

            SPAGAIN;

            error = ERRSV;
            if (TAINT_get || SvTRUE(error)) {
                if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                if (SvTRUE(error)) {
                    sv_catpvs(msg, "Error \"");
                    sv_catsv(msg, error);
                    sv_catpvs(msg, "\"");
                }
                if (TAINT_get) {
                    if (SvTRUE(error)) sv_catpvs(msg, "; ");
                    sv_catpvn(msg, insecure, sizeof(insecure) - 1);
                }

                if (name_len > 0) {
                    sv_catpvs(msg, " in expansion of ");
                    Perl_sv_catpvf(aTHX_ msg, "%" UTF8f, UTF8fARG(is_utf8,
                                                                  name_len,
                                                                  name));
                }

                (void) POPs;
                prop_definition = NULL;
            }
            else {  /* G_SCALAR guarantees a single return value */
                SV * contents = POPs;

                /* The contents is supposed to be the expansion of the property
                 * definition.  If the definition is deferrable, and we got an
                 * empty string back, set a flag to later defer it (after clean
                 * up below). */
                if (      deferrable
                    && (! SvPOK(contents) || SvCUR(contents) == 0))
                {
                        empty_return = TRUE;
                }
                else { /* Otherwise, call a function to check for valid syntax,
                          and handle it */

                    prop_definition = handle_user_defined_property(
                                                    name, name_len,
                                                    is_utf8, to_fold, runtime,
                                                    deferrable,
                                                    contents, user_defined_ptr,
                                                    msg,
                                                    level);
                }
            }

            /* Here, we have the results of the expansion.  Delete the
             * placeholder, and if the definition is now known, replace it with
             * that definition.  We need exclusive access to the hash, and we
             * can't let anyone else in, between when we delete the placeholder
             * and add the permanent entry */
            USER_PROP_MUTEX_LOCK;

            S_delete_recursion_entry(aTHX_ SvPVX(key));

            if (    ! empty_return
                && (! prop_definition || is_invlist(prop_definition)))
            {
                /* If we got success we use the inversion list defining the
                 * property; otherwise use the error message */
                SWITCH_TO_GLOBAL_CONTEXT;
                (void) hv_store_ent(PL_user_def_props,
                                    key,
                                    ((prop_definition)
                                     ? newSVsv(prop_definition)
                                     : newSVsv(msg)),
                                    0);
                RESTORE_CONTEXT;
            }

            /* All done, and the hash now has a permanent entry for this
             * property.  Give up exclusive control */
            USER_PROP_MUTEX_UNLOCK;

            FREETMPS;
            LEAVE;
            POPSTACK;

            if (empty_return) {
                goto definition_deferred;
            }

            if (prop_definition) {

                /* If the definition is for something not known at this time,
                 * we toss it, and go return the main property name, as that's
                 * the one the user will be aware of */
                if (! is_invlist(prop_definition)) {
                    SvREFCNT_dec_NN(prop_definition);
                    goto definition_deferred;
                }

                sv_2mortal(prop_definition);
            }

            /* And return */
            return prop_definition;

        }   /* End of calling the subroutine for the user-defined property */
    }       /* End of it could be a user-defined property */

    /* Here it wasn't a user-defined property that is known at this time.  See
     * if it is a Unicode property */

    lookup_len = j;     /* This is a more mnemonic name than 'j' */

    /* Get the index into our pointer table of the inversion list corresponding
     * to the property */
    table_index = match_uniprop((U8 *) lookup_name, lookup_len);

    /* If it didn't find the property ... */
    if (table_index == 0) {

        /* Try again stripping off any initial 'In' or 'Is' */
        if (starts_with_In_or_Is) {
            lookup_name += 2;
            lookup_len -= 2;
            equals_pos -= 2;
            slash_pos -= 2;

            table_index = match_uniprop((U8 *) lookup_name, lookup_len);
        }

        if (table_index == 0) {
            char * canonical;

            /* Here, we didn't find it.  If not a numeric type property, and
             * can't be a user-defined one, it isn't a legal property */
            if (! is_nv_type) {
                if (! could_be_user_defined) {
                    goto failed;
                }

                /* Here, the property name is legal as a user-defined one.   At
                 * compile time, it might just be that the subroutine for that
                 * property hasn't been encountered yet, but at runtime, it's
                 * an error to try to use an undefined one */
                if (! deferrable) {
                    if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                    sv_catpvs(msg, "Unknown user-defined property name");
                    goto append_name_to_msg;
                }

                goto definition_deferred;
            } /* End of isn't a numeric type property */

            /* The numeric type properties need more work to decide.  What we
             * do is make sure we have the number in canonical form and look
             * that up. */

            if (slash_pos < 0) {    /* No slash */

                /* When it isn't a rational, take the input, convert it to a
                 * NV, then create a canonical string representation of that
                 * NV. */

                NV value;
                SSize_t value_len = lookup_len - equals_pos;

                /* Get the value */
                if (   value_len <= 0
                    || my_atof3(lookup_name + equals_pos, &value,
                                value_len)
                          != lookup_name + lookup_len)
                {
                    goto failed;
                }

                /* If the value is an integer, the canonical value is integral
                 * */
                if (Perl_ceil(value) == value) {
                    canonical = Perl_form(aTHX_ "%.*s%.0" NVff,
                                            equals_pos, lookup_name, value);
                }
                else {  /* Otherwise, it is %e with a known precision */
                    char * exp_ptr;

                    canonical = Perl_form(aTHX_ "%.*s%.*" NVef,
                                                equals_pos, lookup_name,
                                                PL_E_FORMAT_PRECISION, value);

                    /* The exponent generated is expecting two digits, whereas
                     * %e on some systems will generate three.  Remove leading
                     * zeros in excess of 2 from the exponent.  We start
                     * looking for them after the '=' */
                    exp_ptr = strchr(canonical + equals_pos, 'e');
                    if (exp_ptr) {
                        char * cur_ptr = exp_ptr + 2; /* past the 'e[+-]' */
                        SSize_t excess_exponent_len = strlen(cur_ptr) - 2;

                        assert(*(cur_ptr - 1) == '-' || *(cur_ptr - 1) == '+');

                        if (excess_exponent_len > 0) {
                            SSize_t leading_zeros = strspn(cur_ptr, "0");
                            SSize_t excess_leading_zeros
                                    = MIN(leading_zeros, excess_exponent_len);
                            if (excess_leading_zeros > 0) {
                                Move(cur_ptr + excess_leading_zeros,
                                     cur_ptr,
                                     strlen(cur_ptr) - excess_leading_zeros
                                       + 1,  /* Copy the NUL as well */
                                     char);
                            }
                        }
                    }
                }
            }
            else {  /* Has a slash.  Create a rational in canonical form  */
                UV numerator, denominator, gcd, trial;
                const char * end_ptr;
                const char * sign = "";

                /* We can't just find the numerator, denominator, and do the
                 * division, then use the method above, because that is
                 * inexact.  And the input could be a rational that is within
                 * epsilon (given our precision) of a valid rational, and would
                 * then incorrectly compare valid.
                 *
                 * We're only interested in the part after the '=' */
                const char * this_lookup_name = lookup_name + equals_pos;
                lookup_len -= equals_pos;
                slash_pos -= equals_pos;

                /* Handle any leading minus */
                if (this_lookup_name[0] == '-') {
                    sign = "-";
                    this_lookup_name++;
                    lookup_len--;
                    slash_pos--;
                }

                /* Convert the numerator to numeric */
                end_ptr = this_lookup_name + slash_pos;
                if (! grok_atoUV(this_lookup_name, &numerator, &end_ptr)) {
                    goto failed;
                }

                /* It better have included all characters before the slash */
                if (*end_ptr != '/') {
                    goto failed;
                }

                /* Set to look at just the denominator */
                this_lookup_name += slash_pos;
                lookup_len -= slash_pos;
                end_ptr = this_lookup_name + lookup_len;

                /* Convert the denominator to numeric */
                if (! grok_atoUV(this_lookup_name, &denominator, &end_ptr)) {
                    goto failed;
                }

                /* It better be the rest of the characters, and don't divide by
                 * 0 */
                if (   end_ptr != this_lookup_name + lookup_len
                    || denominator == 0)
                {
                    goto failed;
                }

                /* Get the greatest common denominator using
                   http://en.wikipedia.org/wiki/Euclidean_algorithm */
                gcd = numerator;
                trial = denominator;
                while (trial != 0) {
                    UV temp = trial;
                    trial = gcd % trial;
                    gcd = temp;
                }

                /* If already in lowest possible terms, we have already tried
                 * looking this up */
                if (gcd == 1) {
                    goto failed;
                }

                /* Reduce the rational, which should put it in canonical form
                 * */
                numerator /= gcd;
                denominator /= gcd;

                canonical = Perl_form(aTHX_ "%.*s%s%" UVuf "/%" UVuf,
                        equals_pos, lookup_name, sign, numerator, denominator);
            }

            /* Here, we have the number in canonical form.  Try that */
            table_index = match_uniprop((U8 *) canonical, strlen(canonical));
            if (table_index == 0) {
                goto failed;
            }
        }   /* End of still didn't find the property in our table */
    }       /* End of       didn't find the property in our table */

    /* Here, we have a non-zero return, which is an index into a table of ptrs.
     * A negative return signifies that the real index is the absolute value,
     * but the result needs to be inverted */
    if (table_index < 0) {
        invert_return = TRUE;
        table_index = -table_index;
    }

    /* Out-of band indices indicate a deprecated property.  The proper index is
     * modulo it with the table size.  And dividing by the table size yields
     * an offset into a table constructed by regen/mk_invlists.pl to contain
     * the corresponding warning message */
    if (table_index > MAX_UNI_KEYWORD_INDEX) {
        Size_t warning_offset = table_index / MAX_UNI_KEYWORD_INDEX;
        table_index %= MAX_UNI_KEYWORD_INDEX;
        Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
                "Use of '%.*s' in \\p{} or \\P{} is deprecated because: %s",
                (int) name_len, name, deprecated_property_msgs[warning_offset]);
    }

    /* In a few properties, a different property is used under /i.  These are
     * unlikely to change, so are hard-coded here. */
    if (to_fold) {
        if (   table_index == UNI_XPOSIXUPPER
            || table_index == UNI_XPOSIXLOWER
            || table_index == UNI_TITLE)
        {
            table_index = UNI_CASED;
        }
        else if (   table_index == UNI_UPPERCASELETTER
                 || table_index == UNI_LOWERCASELETTER
#  ifdef UNI_TITLECASELETTER   /* Missing from early Unicodes */
                 || table_index == UNI_TITLECASELETTER
#  endif
        ) {
            table_index = UNI_CASEDLETTER;
        }
        else if (  table_index == UNI_POSIXUPPER
                || table_index == UNI_POSIXLOWER)
        {
            table_index = UNI_POSIXALPHA;
        }
    }

    /* Create and return the inversion list */
    prop_definition =_new_invlist_C_array(uni_prop_ptrs[table_index]);
    sv_2mortal(prop_definition);


    /* See if there is a private use override to add to this definition */
    {
        COPHH * hinthash = (IN_PERL_COMPILETIME)
                           ? CopHINTHASH_get(&PL_compiling)
                           : CopHINTHASH_get(PL_curcop);
	SV * pu_overrides = cophh_fetch_pv(hinthash, "private_use", 0, 0);

        if (UNLIKELY(pu_overrides && SvPOK(pu_overrides))) {

            /* See if there is an element in the hints hash for this table */
            SV * pu_lookup = Perl_newSVpvf(aTHX_ "%d=", table_index);
            const char * pos = strstr(SvPVX(pu_overrides), SvPVX(pu_lookup));

            if (pos) {
                bool dummy;
                SV * pu_definition;
                SV * pu_invlist;
                SV * expanded_prop_definition =
                            sv_2mortal(invlist_clone(prop_definition, NULL));

                /* If so, it's definition is the string from here to the next
                 * \a character.  And its format is the same as a user-defined
                 * property */
                pos += SvCUR(pu_lookup);
                pu_definition = newSVpvn(pos, strchr(pos, '\a') - pos);
                pu_invlist = handle_user_defined_property(lookup_name,
                                                          lookup_len,
                                                          0, /* Not UTF-8 */
                                                          0, /* Not folded */
                                                          runtime,
                                                          deferrable,
                                                          pu_definition,
                                                          &dummy,
                                                          msg,
                                                          level);
                if (TAINT_get) {
                    if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
                    sv_catpvs(msg, "Insecure private-use override");
                    goto append_name_to_msg;
                }

                /* For now, as a safety measure, make sure that it doesn't
                 * override non-private use code points */
                _invlist_intersection(pu_invlist, PL_Private_Use, &pu_invlist);

                /* Add it to the list to be returned */
                _invlist_union(prop_definition, pu_invlist,
                               &expanded_prop_definition);
                prop_definition = expanded_prop_definition;
                Perl_ck_warner_d(aTHX_ packWARN(WARN_EXPERIMENTAL__PRIVATE_USE), "The private_use feature is experimental");
            }
        }
    }

    if (invert_return) {
        _invlist_invert(prop_definition);
    }
    return prop_definition;


  failed:
    if (non_pkg_begin != 0) {
        if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
        sv_catpvs(msg, "Illegal user-defined property name");
    }
    else {
        if (SvCUR(msg) > 0) sv_catpvs(msg, "; ");
        sv_catpvs(msg, "Can't find Unicode property definition");
    }
    /* FALLTHROUGH */

  append_name_to_msg:
    {
        const char * prefix = (runtime && level == 0) ?  " \\p{" : " \"";
        const char * suffix = (runtime && level == 0) ?  "}" : "\"";

        sv_catpv(msg, prefix);
        Perl_sv_catpvf(aTHX_ msg, "%" UTF8f, UTF8fARG(is_utf8, name_len, name));
        sv_catpv(msg, suffix);
    }

    return NULL;

  definition_deferred:

    /* Here it could yet to be defined, so defer evaluation of this
     * until its needed at runtime.  We need the fully qualified property name
     * to avoid ambiguity, and a trailing newline */
    if (! fq_name) {
        fq_name = S_get_fq_name(aTHX_ name, name_len, is_utf8,
                                      non_pkg_begin != 0 /* If has "::" */
                               );
    }
    sv_catpvs(fq_name, "\n");

    *user_defined_ptr = TRUE;
    return fq_name;
}

#endif

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
