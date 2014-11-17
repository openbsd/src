/*    regexp.h
 *
 *    Copyright (C) 1993, 1994, 1996, 1997, 1999, 2000, 2001, 2003,
 *    2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#ifndef PLUGGABLE_RE_EXTENSION
/* we don't want to include this stuff if we are inside of
   an external regex engine based on the core one - like re 'debug'*/

#include "utf8.h"

struct regnode {
    U8	flags;
    U8  type;
    U16 next_off;
};

typedef struct regnode regnode;

struct reg_substr_data;

struct reg_data;

struct regexp_engine;
struct regexp;

struct reg_substr_datum {
    SSize_t min_offset; /* min pos (in chars) that substr must appear */
    SSize_t max_offset  /* max pos (in chars) that substr must appear */;
    SV *substr;		/* non-utf8 variant */
    SV *utf8_substr;	/* utf8 variant */
    SSize_t end_shift;  /* how many fixed chars must end the string */
};
struct reg_substr_data {
    U8      check_ix;   /* index into data[] of check substr */
    struct reg_substr_datum data[3];	/* Actual array */
};

#ifdef PERL_ANY_COW
#define SV_SAVED_COPY   SV *saved_copy; /* If non-NULL, SV which is COW from original */
#else
#define SV_SAVED_COPY
#endif

/* offsets within a string of a particular /(.)/ capture */

typedef struct regexp_paren_pair {
    SSize_t start;
    SSize_t end;
    /* 'start_tmp' records a new opening position before the matching end
     * has been found, so that the old start and end values are still
     * valid, e.g.
     *	  "abc" =~ /(.(?{print "[$1]"}))+/
     *outputs [][a][b]
     * This field is not part of the API.  */
    SSize_t start_tmp;
} regexp_paren_pair;

#if defined(PERL_IN_REGCOMP_C) || defined(PERL_IN_UTF8_C)
#define _invlist_union(a, b, output) _invlist_union_maybe_complement_2nd(a, b, FALSE, output)
#define _invlist_intersection(a, b, output) _invlist_intersection_maybe_complement_2nd(a, b, FALSE, output)

/* Subtracting b from a leaves in a everything that was there that isn't in b,
 * that is the intersection of a with b's complement */
#define _invlist_subtract(a, b, output) _invlist_intersection_maybe_complement_2nd(a, b, TRUE, output)
#endif

/* record the position of a (?{...}) within a pattern */

struct reg_code_block {
    STRLEN start;
    STRLEN end;
    OP     *block;
    REGEXP *src_regex;
};


/*
  The regexp/REGEXP struct, see L<perlreapi> for further documentation
  on the individual fields. The struct is ordered so that the most
  commonly used fields are placed at the start.

  Any patch that adds items to this struct will need to include
  changes to F<sv.c> (C<Perl_re_dup()>) and F<regcomp.c>
  (C<pregfree()>). This involves freeing or cloning items in the
  regexp's data array based on the data item's type.
*/

#define _REGEXP_COMMON							\
        /* what engine created this regexp? */				\
	const struct regexp_engine* engine; 				\
	REGEXP *mother_re; /* what re is this a lightweight copy of? */	\
	HV *paren_names;   /* Optional hash of paren names */		\
	/* Information about the match that the perl core uses to */	\
	/* manage things */						\
	U32 extflags;	/* Flags used both externally and internally */	\
	SSize_t minlen;	/* mininum possible number of chars in string to match */\
	SSize_t minlenret; /* mininum possible number of chars in $& */		\
	STRLEN gofs;	/* chars left of pos that we search from */	\
	/* substring data about strings that must appear in the */	\
	/* final match, used for optimisations */			\
	struct reg_substr_data *substrs;				\
	U32 nparens;	/* number of capture buffers */			\
	/* private engine specific data */				\
	U32 intflags;	/* Engine Specific Internal flags */		\
	void *pprivate;	/* Data private to the regex engine which */	\
			/* created this object. */			\
	/* Data about the last/current match. These are modified */	\
	/* during matching */						\
	U32 lastparen;			/* last open paren matched */	\
	U32 lastcloseparen;		/* last close paren matched */	\
	/* Array of offsets for (@-) and (@+) */			\
	regexp_paren_pair *offs;					\
	/* saved or original string so \digit works forever. */		\
	char *subbeg;							\
	SV_SAVED_COPY	/* If non-NULL, SV which is COW from original */\
	SSize_t sublen;	/* Length of string pointed by subbeg */	\
	SSize_t suboffset; /* byte offset of subbeg from logical start of str */ \
	SSize_t subcoffset; /* suboffset equiv, but in chars (for @-/@+) */ \
	/* Information about the match that isn't often used */		\
        SSize_t maxlen;        /* mininum possible number of chars in string to match */\
	/* offset from wrapped to the start of precomp */		\
	PERL_BITFIELD32 pre_prefix:4;					\
        /* original flags used to compile the pattern, may differ */    \
        /* from extflags in various ways */                             \
        PERL_BITFIELD32 compflags:9;                                    \
	CV *qr_anoncv	/* the anon sub wrapped round qr/(?{..})/ */

typedef struct regexp {
	_XPV_HEAD;
	_REGEXP_COMMON;
} regexp;

#define RXp_PAREN_NAMES(rx)	((rx)->paren_names)

/* used for high speed searches */
typedef struct re_scream_pos_data_s
{
    char **scream_olds;		/* match pos */
    SSize_t *scream_pos;	/* Internal iterator of scream. */
} re_scream_pos_data;

/* regexp_engine structure. This is the dispatch table for regexes.
 * Any regex engine implementation must be able to build one of these.
 */
typedef struct regexp_engine {
    REGEXP* (*comp) (pTHX_ SV * const pattern, U32 flags);
    I32     (*exec) (pTHX_ REGEXP * const rx, char* stringarg, char* strend,
                     char* strbeg, SSize_t minend, SV* sv,
                     void* data, U32 flags);
    char*   (*intuit) (pTHX_
                        REGEXP * const rx,
                        SV *sv,
                        const char * const strbeg,
                        char *strpos,
                        char *strend,
                        const U32 flags,
                       re_scream_pos_data *data);
    SV*     (*checkstr) (pTHX_ REGEXP * const rx);
    void    (*free) (pTHX_ REGEXP * const rx);
    void    (*numbered_buff_FETCH) (pTHX_ REGEXP * const rx, const I32 paren,
                                    SV * const sv);
    void    (*numbered_buff_STORE) (pTHX_ REGEXP * const rx, const I32 paren,
                                   SV const * const value);
    I32     (*numbered_buff_LENGTH) (pTHX_ REGEXP * const rx, const SV * const sv,
                                    const I32 paren);
    SV*     (*named_buff) (pTHX_ REGEXP * const rx, SV * const key,
                           SV * const value, const U32 flags);
    SV*     (*named_buff_iter) (pTHX_ REGEXP * const rx, const SV * const lastkey,
                                const U32 flags);
    SV*     (*qr_package)(pTHX_ REGEXP * const rx);
#ifdef USE_ITHREADS
    void*   (*dupe) (pTHX_ REGEXP * const rx, CLONE_PARAMS *param);
#endif
    REGEXP* (*op_comp) (pTHX_ SV ** const patternp, int pat_count,
		    OP *expr, const struct regexp_engine* eng,
		    REGEXP *VOL old_re,
		    bool *is_bare_re, U32 orig_rx_flags, U32 pm_flags);
} regexp_engine;

/*
  These are passed to the numbered capture variable callbacks as the
  paren name. >= 1 is reserved for actual numbered captures, i.e. $1,
  $2 etc.
*/
#define RX_BUFF_IDX_CARET_PREMATCH  -5 /* ${^PREMATCH}  */
#define RX_BUFF_IDX_CARET_POSTMATCH -4 /* ${^POSTMATCH} */
#define RX_BUFF_IDX_CARET_FULLMATCH -3 /* ${^MATCH}     */
#define RX_BUFF_IDX_PREMATCH        -2 /* $` */
#define RX_BUFF_IDX_POSTMATCH       -1 /* $' */
#define RX_BUFF_IDX_FULLMATCH        0 /* $& */

/*
  Flags that are passed to the named_buff and named_buff_iter
  callbacks above. Those routines are called from universal.c via the
  Tie::Hash::NamedCapture interface for %+ and %- and the re::
  functions in the same file.
*/

/* The Tie::Hash::NamedCapture operation this is part of, if any */
#define RXapif_FETCH     0x0001
#define RXapif_STORE     0x0002
#define RXapif_DELETE    0x0004
#define RXapif_CLEAR     0x0008
#define RXapif_EXISTS    0x0010
#define RXapif_SCALAR    0x0020
#define RXapif_FIRSTKEY  0x0040
#define RXapif_NEXTKEY   0x0080

/* Whether %+ or %- is being operated on */
#define RXapif_ONE       0x0100 /* %+ */
#define RXapif_ALL       0x0200 /* %- */

/* Whether this is being called from a re:: function */
#define RXapif_REGNAME         0x0400
#define RXapif_REGNAMES        0x0800
#define RXapif_REGNAMES_COUNT  0x1000

/*
=head1 REGEXP Functions

=for apidoc Am|REGEXP *|SvRX|SV *sv

Convenience macro to get the REGEXP from a SV.  This is approximately
equivalent to the following snippet:

    if (SvMAGICAL(sv))
        mg_get(sv);
    if (SvROK(sv))
        sv = MUTABLE_SV(SvRV(sv));
    if (SvTYPE(sv) == SVt_REGEXP)
        return (REGEXP*) sv;

NULL will be returned if a REGEXP* is not found.

=for apidoc Am|bool|SvRXOK|SV* sv

Returns a boolean indicating whether the SV (or the one it references)
is a REGEXP.

If you want to do something with the REGEXP* later use SvRX instead
and check for NULL.

=cut
*/

#define SvRX(sv)   (Perl_get_re_arg(aTHX_ sv))
#define SvRXOK(sv) (Perl_get_re_arg(aTHX_ sv) ? TRUE : FALSE)


/* Flags stored in regexp->extflags
 * These are used by code external to the regexp engine
 *
 * Note that the flags whose names start with RXf_PMf_ are defined in
 * op_reg_common.h, being copied from the parallel flags of op_pmflags
 *
 * NOTE: if you modify any RXf flags you should run regen.pl or
 * regen/regcomp.pl so that regnodes.h is updated with the changes.
 *
 */

#include "op_reg_common.h"

#define RXf_PMf_STD_PMMOD	(RXf_PMf_MULTILINE|RXf_PMf_SINGLELINE|RXf_PMf_FOLD|RXf_PMf_EXTENDED)

#define CASE_STD_PMMOD_FLAGS_PARSE_SET(pmfl)                        \
    case IGNORE_PAT_MOD:    *(pmfl) |= RXf_PMf_FOLD;       break;   \
    case MULTILINE_PAT_MOD: *(pmfl) |= RXf_PMf_MULTILINE;  break;   \
    case SINGLE_PAT_MOD:    *(pmfl) |= RXf_PMf_SINGLELINE; break;   \
    case XTENDED_PAT_MOD:   *(pmfl) |= RXf_PMf_EXTENDED;   break

/* Note, includes charset ones, assumes 0 is the default for them */
#define STD_PMMOD_FLAGS_CLEAR(pmfl)                        \
    *(pmfl) &= ~(RXf_PMf_FOLD|RXf_PMf_MULTILINE|RXf_PMf_SINGLELINE|RXf_PMf_EXTENDED|RXf_PMf_CHARSET)

/* chars and strings used as regex pattern modifiers
 * Singular is a 'c'har, plural is a "string"
 *
 * NOTE, KEEPCOPY was originally 'k', but was changed to 'p' for preserve
 * for compatibility reasons with Regexp::Common which highjacked (?k:...)
 * for its own uses. So 'k' is out as well.
 */
#define DEFAULT_PAT_MOD      '^'    /* Short for all the default modifiers */
#define EXEC_PAT_MOD         'e'
#define KEEPCOPY_PAT_MOD     'p'
#define ONCE_PAT_MOD         'o'
#define GLOBAL_PAT_MOD       'g'
#define CONTINUE_PAT_MOD     'c'
#define MULTILINE_PAT_MOD    'm'
#define SINGLE_PAT_MOD       's'
#define IGNORE_PAT_MOD       'i'
#define XTENDED_PAT_MOD      'x'
#define NONDESTRUCT_PAT_MOD  'r'
#define LOCALE_PAT_MOD       'l'
#define UNICODE_PAT_MOD      'u'
#define DEPENDS_PAT_MOD      'd'
#define ASCII_RESTRICT_PAT_MOD 'a'

#define ONCE_PAT_MODS        "o"
#define KEEPCOPY_PAT_MODS    "p"
#define EXEC_PAT_MODS        "e"
#define LOOP_PAT_MODS        "gc"
#define NONDESTRUCT_PAT_MODS "r"
#define LOCALE_PAT_MODS      "l"
#define UNICODE_PAT_MODS     "u"
#define DEPENDS_PAT_MODS     "d"
#define ASCII_RESTRICT_PAT_MODS "a"
#define ASCII_MORE_RESTRICT_PAT_MODS "aa"

/* This string is expected by regcomp.c to be ordered so that the first
 * character is the flag in bit RXf_PMf_STD_PMMOD_SHIFT of extflags; the next
 * character is bit +1, etc. */
#define STD_PAT_MODS        "msix"

#define CHARSET_PAT_MODS    ASCII_RESTRICT_PAT_MODS DEPENDS_PAT_MODS LOCALE_PAT_MODS UNICODE_PAT_MODS

/* This string is expected by XS_re_regexp_pattern() in universal.c to be ordered
 * so that the first character is the flag in bit RXf_PMf_STD_PMMOD_SHIFT of
 * extflags; the next character is in bit +1, etc. */
#define INT_PAT_MODS    STD_PAT_MODS    KEEPCOPY_PAT_MODS

#define EXT_PAT_MODS    ONCE_PAT_MODS   KEEPCOPY_PAT_MODS
#define QR_PAT_MODS     STD_PAT_MODS    EXT_PAT_MODS	   CHARSET_PAT_MODS
#define M_PAT_MODS      QR_PAT_MODS     LOOP_PAT_MODS
#define S_PAT_MODS      M_PAT_MODS      EXEC_PAT_MODS      NONDESTRUCT_PAT_MODS

/*
 * NOTE: if you modify any RXf flags you should run regen.pl or
 * regen/regcomp.pl so that regnodes.h is updated with the changes.
 *
 */

/* Leave some space, so future bit allocations can go either in the shared or
 * unshared area without affecting binary compatibility */
#define RXf_BASE_SHIFT (_RXf_PMf_SHIFT_NEXT)

/*
  Set in Perl_pmruntime if op_flags & OPf_SPECIAL, i.e. split. Will
  be used by regex engines to check whether they should set
  RXf_SKIPWHITE
*/
#define RXf_SPLIT                (1<<(RXf_BASE_SHIFT-1))
#if RXf_SPLIT != RXf_PMf_SPLIT
#   error "RXf_SPLIT does not match RXf_PMf_SPLIT"
#endif

/* Manually decorate this function with gcc-style attributes just to
 * avoid having to restructure the header files and their called order,
 * as proto.h would have to be included before this file, and isn't */

PERL_STATIC_INLINE const char *
get_regex_charset_name(const U32 flags, STRLEN* const lenp)
    __attribute__warn_unused_result__;

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
        default:
	    return "?";	    /* Unknown */
    }
}

/* Do we have some sort of anchor? */
#define RXf_IS_ANCHORED         (1<<(RXf_BASE_SHIFT+0))
#define RXf_UNUSED1             (1<<(RXf_BASE_SHIFT+1))
#define RXf_UNUSED2             (1<<(RXf_BASE_SHIFT+2))
#define RXf_UNUSED3             (1<<(RXf_BASE_SHIFT+3))
#define RXf_UNUSED4             (1<<(RXf_BASE_SHIFT+4))
#define RXf_UNUSED5             (1<<(RXf_BASE_SHIFT+5))

/* What we have seen */
#define RXf_NO_INPLACE_SUBST    (1<<(RXf_BASE_SHIFT+6))
#define RXf_EVAL_SEEN   	(1<<(RXf_BASE_SHIFT+7))
#define RXf_UNUSED8             (1<<(RXf_BASE_SHIFT+8))

/* Special */
#define RXf_UNBOUNDED_QUANTIFIER_SEEN   (1<<(RXf_BASE_SHIFT+9))
#define RXf_CHECK_ALL   	(1<<(RXf_BASE_SHIFT+10))

/* UTF8 related */
#define RXf_MATCH_UTF8  	(1<<(RXf_BASE_SHIFT+11)) /* $1 etc are utf8 */

/* Intuit related */
#define RXf_USE_INTUIT_NOML	(1<<(RXf_BASE_SHIFT+12))
#define RXf_USE_INTUIT_ML	(1<<(RXf_BASE_SHIFT+13))
#define RXf_INTUIT_TAIL 	(1<<(RXf_BASE_SHIFT+14))
#define RXf_USE_INTUIT		(RXf_USE_INTUIT_NOML|RXf_USE_INTUIT_ML)

/* Copy and tainted info */
#define RXf_COPY_DONE   	(1<<(RXf_BASE_SHIFT+16))

/* post-execution: $1 et al are tainted */
#define RXf_TAINTED_SEEN	(1<<(RXf_BASE_SHIFT+17))
/* this pattern was tainted during compilation */
#define RXf_TAINTED		(1<<(RXf_BASE_SHIFT+18))

/* Flags indicating special patterns */
#define RXf_START_ONLY		(1<<(RXf_BASE_SHIFT+19)) /* Pattern is /^/ */
#define RXf_SKIPWHITE                (1<<(RXf_BASE_SHIFT+20)) /* Pattern is for a split " " */
#define RXf_WHITE		(1<<(RXf_BASE_SHIFT+21)) /* Pattern is /\s+/ */
#define RXf_NULL		(1U<<(RXf_BASE_SHIFT+22)) /* Pattern is // */
#if RXf_BASE_SHIFT+22 > 31
#   error Too many RXf_PMf bits used.  See regnodes.h for any spare in middle
#endif

/*
 * NOTE: if you modify any RXf flags you should run regen.pl or
 * regen/regcomp.pl so that regnodes.h is updated with the changes.
 *
 */

#ifdef NO_TAINT_SUPPORT
#   define RX_ISTAINTED(prog)    0
#   define RX_TAINT_on(prog)     NOOP
#   define RXp_MATCH_TAINTED(prog) 0
#   define RX_MATCH_TAINTED(prog)  0
#   define RXp_MATCH_TAINTED_on(prog) NOOP
#   define RX_MATCH_TAINTED_on(prog)  NOOP
#   define RX_MATCH_TAINTED_off(prog) NOOP
#else
#   define RX_ISTAINTED(prog)    (RX_EXTFLAGS(prog) & RXf_TAINTED)
#   define RX_TAINT_on(prog)     (RX_EXTFLAGS(prog) |= RXf_TAINTED)
#   define RXp_MATCH_TAINTED(prog)    (RXp_EXTFLAGS(prog) & RXf_TAINTED_SEEN)
#   define RX_MATCH_TAINTED(prog)     (RX_EXTFLAGS(prog)  & RXf_TAINTED_SEEN)
#   define RXp_MATCH_TAINTED_on(prog) (RXp_EXTFLAGS(prog) |= RXf_TAINTED_SEEN)
#   define RX_MATCH_TAINTED_on(prog)  (RX_EXTFLAGS(prog)  |= RXf_TAINTED_SEEN)
#   define RX_MATCH_TAINTED_off(prog) (RX_EXTFLAGS(prog)  &= ~RXf_TAINTED_SEEN)
#endif

#define RX_HAS_CUTGROUP(prog) ((prog)->intflags & PREGf_CUTGROUP_SEEN)
#define RX_MATCH_TAINTED_set(prog, t) ((t) \
				       ? RX_MATCH_TAINTED_on(prog) \
				       : RX_MATCH_TAINTED_off(prog))

#define RXp_MATCH_COPIED(prog)		(RXp_EXTFLAGS(prog) & RXf_COPY_DONE)
#define RX_MATCH_COPIED(prog)		(RX_EXTFLAGS(prog) & RXf_COPY_DONE)
#define RXp_MATCH_COPIED_on(prog)	(RXp_EXTFLAGS(prog) |= RXf_COPY_DONE)
#define RX_MATCH_COPIED_on(prog)	(RX_EXTFLAGS(prog) |= RXf_COPY_DONE)
#define RXp_MATCH_COPIED_off(prog)	(RXp_EXTFLAGS(prog) &= ~RXf_COPY_DONE)
#define RX_MATCH_COPIED_off(prog)	(RX_EXTFLAGS(prog) &= ~RXf_COPY_DONE)
#define RX_MATCH_COPIED_set(prog,t)	((t) \
					 ? RX_MATCH_COPIED_on(prog) \
					 : RX_MATCH_COPIED_off(prog))

#define RXp_EXTFLAGS(rx)	((rx)->extflags)
#define RXp_COMPFLAGS(rx)        ((rx)->compflags)

/* For source compatibility. We used to store these explicitly.  */
#define RX_PRECOMP(prog)	(RX_WRAPPED(prog) + ReANY(prog)->pre_prefix)
#define RX_PRECOMP_const(prog)	(RX_WRAPPED_const(prog) + ReANY(prog)->pre_prefix)
/* FIXME? Are we hardcoding too much here and constraining plugin extension
   writers? Specifically, the value 1 assumes that the wrapped version always
   has exactly one character at the end, a ')'. Will that always be true?  */
#define RX_PRELEN(prog)		(RX_WRAPLEN(prog) - ReANY(prog)->pre_prefix - 1)
#define RX_WRAPPED(prog)	ReANY(prog)->xpv_len_u.xpvlenu_pv
#define RX_WRAPPED_const(prog)	((const char *)RX_WRAPPED(prog))
#define RX_WRAPLEN(prog)	SvCUR(prog)
#define RX_CHECK_SUBSTR(prog)	(ReANY(prog)->check_substr)
#define RX_REFCNT(prog)		SvREFCNT(prog)
#define RX_EXTFLAGS(prog)	RXp_EXTFLAGS(ReANY(prog))
#define RX_COMPFLAGS(prog)        RXp_COMPFLAGS(ReANY(prog))
#define RX_ENGINE(prog)		(ReANY(prog)->engine)
#define RX_SUBBEG(prog)		(ReANY(prog)->subbeg)
#define RX_SUBOFFSET(prog)	(ReANY(prog)->suboffset)
#define RX_SUBCOFFSET(prog)	(ReANY(prog)->subcoffset)
#define RX_OFFS(prog)		(ReANY(prog)->offs)
#define RX_NPARENS(prog)	(ReANY(prog)->nparens)
#define RX_SUBLEN(prog)		(ReANY(prog)->sublen)
#define RX_MINLEN(prog)		(ReANY(prog)->minlen)
#define RX_MINLENRET(prog)	(ReANY(prog)->minlenret)
#define RX_GOFS(prog)		(ReANY(prog)->gofs)
#define RX_LASTPAREN(prog)	(ReANY(prog)->lastparen)
#define RX_LASTCLOSEPAREN(prog)	(ReANY(prog)->lastcloseparen)
#define RX_SAVED_COPY(prog)	(ReANY(prog)->saved_copy)
/* last match was zero-length */
#define RX_ZERO_LEN(prog) \
        (RX_OFFS(prog)[0].start + (SSize_t)RX_GOFS(prog) \
          == RX_OFFS(prog)[0].end)

#endif /* PLUGGABLE_RE_EXTENSION */

/* Stuff that needs to be included in the pluggable extension goes below here */

#ifdef PERL_ANY_COW
#define RX_MATCH_COPY_FREE(rx) \
	STMT_START {if (RX_SAVED_COPY(rx)) { \
	    SV_CHECK_THINKFIRST_COW_DROP(RX_SAVED_COPY(rx)); \
	} \
	if (RX_MATCH_COPIED(rx)) { \
	    Safefree(RX_SUBBEG(rx)); \
	    RX_MATCH_COPIED_off(rx); \
	}} STMT_END
#else
#define RX_MATCH_COPY_FREE(rx) \
	STMT_START {if (RX_MATCH_COPIED(rx)) { \
	    Safefree(RX_SUBBEG(rx)); \
	    RX_MATCH_COPIED_off(rx); \
	}} STMT_END
#endif

#define RXp_MATCH_UTF8(prog)		(RXp_EXTFLAGS(prog) & RXf_MATCH_UTF8)
#define RX_MATCH_UTF8(prog)		(RX_EXTFLAGS(prog) & RXf_MATCH_UTF8)
#define RX_MATCH_UTF8_on(prog)		(RX_EXTFLAGS(prog) |= RXf_MATCH_UTF8)
#define RX_MATCH_UTF8_off(prog)		(RX_EXTFLAGS(prog) &= ~RXf_MATCH_UTF8)
#define RX_MATCH_UTF8_set(prog, t)	((t) \
			? RX_MATCH_UTF8_on(prog) \
			: RX_MATCH_UTF8_off(prog))

/* Whether the pattern stored at RX_WRAPPED is in UTF-8  */
#define RX_UTF8(prog)			SvUTF8(prog)


/* bits in flags arg of Perl_regexec_flags() */

#define REXEC_COPY_STR  0x01    /* Need to copy the string for captures. */
#define REXEC_CHECKED   0x02    /* re_intuit_start() already called. */
#define REXEC_SCREAM    0x04    /* currently unused. */
#define REXEC_IGNOREPOS 0x08    /* use stringarg, not pos(), for \G match */
#define REXEC_NOT_FIRST 0x10    /* This is another iteration of //g:
                                   no need to copy string again */

                                     /* under REXEC_COPY_STR, it's ok for the
                                        engine (modulo PL_sawamperand etc)
                                        to skip copying: ... */
#define REXEC_COPY_SKIP_PRE  0x20    /* ...the $` part of the string, or */
#define REXEC_COPY_SKIP_POST 0x40    /* ...the $' part of the string */
#define REXEC_FAIL_ON_UNDERFLOW 0x80 /* fail the match if $& would start before
                                        the start pos (so s/.\G// would fail
                                        on second iteration */

#if defined(__GNUC__) && !defined(PERL_GCC_BRACE_GROUPS_FORBIDDEN)
#  define ReREFCNT_inc(re)						\
    ({									\
	/* This is here to generate a casting warning if incorrect.  */	\
	REGEXP *const _rerefcnt_inc = (re);				\
	assert(SvTYPE(_rerefcnt_inc) == SVt_REGEXP);			\
	SvREFCNT_inc(_rerefcnt_inc);					\
	_rerefcnt_inc;							\
    })
#  define ReREFCNT_dec(re)						\
    ({									\
	/* This is here to generate a casting warning if incorrect.  */	\
	REGEXP *const _rerefcnt_dec = (re);				\
	SvREFCNT_dec(_rerefcnt_dec);					\
    })
#else
#  define ReREFCNT_dec(re)	SvREFCNT_dec(re)
#  define ReREFCNT_inc(re)	((REGEXP *) SvREFCNT_inc(re))
#endif
#define ReANY(re)		S_ReANY((const REGEXP *)(re))

/* FIXME for plugins. */

#define FBMcf_TAIL_DOLLAR	1
#define FBMcf_TAIL_DOLLARM	2
#define FBMcf_TAIL_Z		4
#define FBMcf_TAIL_z		8
#define FBMcf_TAIL		(FBMcf_TAIL_DOLLAR|FBMcf_TAIL_DOLLARM|FBMcf_TAIL_Z|FBMcf_TAIL_z)

#define FBMrf_MULTILINE	1

struct regmatch_state;
struct regmatch_slab;

/* like regmatch_info_aux, but contains extra fields only needed if the
 * pattern contains (?{}). If used, is snuck into the second slot in the
 * regmatch_state stack at the start of execution */

typedef struct {
    regexp *rex;
    PMOP    *curpm;     /* saved PL_curpm */
#ifdef PERL_ANY_COW
    SV      *saved_copy; /* saved saved_copy field from rex */
#endif
    char    *subbeg;    /* saved subbeg     field from rex */
    STRLEN  sublen;     /* saved sublen     field from rex */
    STRLEN  suboffset;  /* saved suboffset  field from rex */
    STRLEN  subcoffset; /* saved subcoffset field from rex */
    MAGIC   *pos_magic; /* pos() magic attached to $_ */
    SSize_t pos;        /* the original value of pos() in pos_magic */
    U8      pos_flags;  /* flags to be restored; currently only MGf_BYTES*/
} regmatch_info_aux_eval;


/* fields that logically  live in regmatch_info, but which need cleaning
 * up on croak(), and so are instead are snuck into the first slot in
 * the regmatch_state stack at the start of execution */

typedef struct {
    regmatch_info_aux_eval *info_aux_eval;
    struct regmatch_state *old_regmatch_state; /* saved PL_regmatch_state */
    struct regmatch_slab  *old_regmatch_slab;  /* saved PL_regmatch_slab */
    char *poscache;	/* S-L cache of fail positions of WHILEMs */
} regmatch_info_aux;


/* some basic information about the current match that is created by
 * Perl_regexec_flags and then passed to regtry(), regmatch() etc.
 * It is allocated as a local var on the stack, so nothing should be
 * stored in it that needs preserving or clearing up on croak().
 * For that, see the aux_info and aux_info_eval members of the
 * regmatch_state union. */

typedef struct {
    REGEXP *prog;        /* the regex being executed */
    const char * strbeg; /* real start of string */
    char *strend;        /* one byte beyond last char of match string */
    char *till;          /* matches shorter than this fail (see minlen arg) */
    SV *sv;              /* the SV string currently being matched */
    char *ganch;         /* position of \G anchor */
    char *cutpoint;      /* (*COMMIT) position (if any) */
    regmatch_info_aux      *info_aux; /* extra fields that need cleanup */
    regmatch_info_aux_eval *info_aux_eval; /* extra saved state for (?{}) */
    I32  poscache_maxiter; /* how many whilems todo before S-L cache kicks in */
    I32  poscache_iter;    /* current countdown from _maxiter to zero */
    STRLEN poscache_size;  /* size of regmatch_info_aux.poscache */
    bool intuit;    /* re_intuit_start() is the top-level caller */
    bool is_utf8_pat;    /* regex is utf8 */
    bool is_utf8_target; /* string being matched is utf8 */
    bool warned; /* we have issued a recursion warning; no need for more */
} regmatch_info;
 

/* structures for holding and saving the state maintained by regmatch() */

#ifndef MAX_RECURSE_EVAL_NOCHANGE_DEPTH
#define MAX_RECURSE_EVAL_NOCHANGE_DEPTH 1000
#endif

typedef I32 CHECKPOINT;

typedef struct regmatch_state {
    int resume_state;		/* where to jump to on return */
    char *locinput;		/* where to backtrack in string on failure */

    union {

        /* the 'info_aux' and 'info_aux_eval' union members are cuckoos in
         * the nest. They aren't saved backtrack state; rather they
         * represent one or two extra chunks of data that need allocating
         * at the start of a match. These fields would logically live in
         * the regmatch_info struct, except that is allocated on the
         * C stack, and these fields are all things that require cleanup
         * after a croak(), when the stack is lost.
         * As a convenience, we just use the first 1 or 2 regmatch_state
         * slots to store this info, as we will be allocating a slab of
         * these anyway. Otherwise we'd have to malloc and then free them,
         * or allocate them on the save stack (where they will get
         * realloced if the save stack grows).
         * info_aux contains the extra fields that are always needed;
         * info_aux_eval contains extra fields that only needed if
         * the pattern contains code blocks
         * We split them into two separate structs to avoid increasing
         * the size of the union.
         */

        regmatch_info_aux info_aux;

        regmatch_info_aux_eval info_aux_eval;

	/* this is a fake union member that matches the first element
	 * of each member that needs to store positive backtrack
	 * information */
	struct {
	    struct regmatch_state *prev_yes_state;
	} yes;

        /* branchlike members */
        /* this is a fake union member that matches the first elements
         * of each member that needs to behave like a branch */
        struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    U32 lastparen;
	    U32 lastcloseparen;
	    CHECKPOINT cp;
	    
        } branchlike;
        	    
	struct {
	    /* the first elements must match u.branchlike */
	    struct regmatch_state *prev_yes_state;
	    U32 lastparen;
	    U32 lastcloseparen;
	    CHECKPOINT cp;
	    
	    regnode *next_branch; /* next branch node */
	} branch;

	struct {
	    /* the first elements must match u.branchlike */
	    struct regmatch_state *prev_yes_state;
	    U32 lastparen;
	    U32 lastcloseparen;
	    CHECKPOINT cp;

	    U32		accepted; /* how many accepting states left */
	    bool	longfold;/* saw a fold with a 1->n char mapping */
	    U16         *jump;  /* positive offsets from me */
	    regnode	*me;	/* Which node am I - needed for jump tries*/
	    U8		*firstpos;/* pos in string of first trie match */
	    U32		firstchars;/* len in chars of firstpos from start */
	    U16		nextword;/* next word to try */
	    U16		topword; /* longest accepted word */
	} trie;

        /* special types - these members are used to store state for special
           regops like eval, if/then, lookaround and the markpoint state */
	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *prev_eval;
	    struct regmatch_state *prev_curlyx;
	    REGEXP	*prev_rex;
	    CHECKPOINT	cp;	/* remember current savestack indexes */
	    CHECKPOINT	lastcp;
	    U32        close_paren; /* which close bracket is our end */
	    regnode	*B;	/* the node following us  */
	} eval;

	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    I32 wanted;
	    I32 logical;	/* saved copy of 'logical' var */
	    regnode  *me; /* the IFMATCH/SUSPEND/UNLESSM node  */
	} ifmatch; /* and SUSPEND/UNLESSM */
	
	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *prev_mark;
	    SV* mark_name;
	    char *mark_loc;
	} mark;
	
	struct {
	    int val;
	} keeper;

        /* quantifiers - these members are used for storing state for
           for the regops used to implement quantifiers */
	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *prev_curlyx; /* previous cur_curlyx */
	    regnode	*me;	/* the CURLYX node  */
	    regnode	*B;	/* the B node in /A*B/  */
	    CHECKPOINT	cp;	/* remember current savestack index */
	    bool	minmod;
	    int		parenfloor;/* how far back to strip paren data */

	    /* these two are modified by WHILEM */
	    int		count;	/* how many instances of A we've matched */
	    char	*lastloc;/* where previous A matched (0-len detect) */
	} curlyx;

	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    struct regmatch_state *save_curlyx;
	    CHECKPOINT	cp;	/* remember current savestack indexes */
	    CHECKPOINT	lastcp;
	    char	*save_lastloc;	/* previous curlyx.lastloc */
	    I32		cache_offset;
	    I32		cache_mask;
	} whilem;

	struct {
	    /* this first element must match u.yes */
	    struct regmatch_state *prev_yes_state;
	    int c1, c2;		/* case fold search */
	    CHECKPOINT cp;
	    U32 lastparen;
	    U32 lastcloseparen;
	    I32 alen;		/* length of first-matched A string */
	    I32 count;
	    bool minmod;
	    regnode *A, *B;	/* the nodes corresponding to /A*B/  */
	    regnode *me;	/* the curlym node */
            U8 c1_utf8[UTF8_MAXBYTES+1];  /* */
            U8 c2_utf8[UTF8_MAXBYTES+1];
	} curlym;

	struct {
	    U32 paren;
	    CHECKPOINT cp;
	    U32 lastparen;
	    U32 lastcloseparen;
	    int c1, c2;		/* case fold search */
	    char *maxpos;	/* highest possible point in string to match */
	    char *oldloc;	/* the previous locinput */
	    int count;
	    int min, max;	/* {m,n} */
	    regnode *A, *B;	/* the nodes corresponding to /A*B/  */
            U8 c1_utf8[UTF8_MAXBYTES+1];  /* */
            U8 c2_utf8[UTF8_MAXBYTES+1];
	} curly; /* and CURLYN/PLUS/STAR */

    } u;
} regmatch_state;

/* how many regmatch_state structs to allocate as a single slab.
 * We do it in 4K blocks for efficiency. The "3" is 2 for the next/prev
 * pointers, plus 1 for any mythical malloc overhead. */
 
#define PERL_REGMATCH_SLAB_SLOTS \
    ((4096 - 3 * sizeof (void*)) / sizeof(regmatch_state))

typedef struct regmatch_slab {
    regmatch_state states[PERL_REGMATCH_SLAB_SLOTS];
    struct regmatch_slab *prev, *next;
} regmatch_slab;



/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 et:
 */
