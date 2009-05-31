/*		Character grid hypertext object
 *		===============================
 */

#include <HTUtils.h>
#include <HTString.h>
#include <HTAccess.h>
#include <HTAnchor.h>
#include <HTParse.h>
#include <HTTP.h>
#include <HTAlert.h>
#include <HTCJK.h>
#include <HTFile.h>
#include <UCDefs.h>
#include <UCAux.h>
#include <HText.h>

#include <assert.h>

#include <GridText.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <LYStructs.h>
#include <LYGlobalDefs.h>
#include <LYGetFile.h>
#include <LYClean.h>
#include <LYMail.h>
#include <LYList.h>
#include <LYCharSets.h>
#include <LYCharUtils.h>	/* LYUCTranslateBack... */
#include <UCMap.h>
#include <LYEdit.h>
#include <LYPrint.h>
#include <LYPrettySrc.h>
#include <TRSTable.h>
#include <LYHistory.h>
#ifdef EXP_CHARTRANS_AUTOSWITCH
#include <UCAuto.h>
#endif /* EXP_CHARTRANS_AUTOSWITCH */

#include <LYexit.h>
#include <LYLeaks.h>

/*#define DEBUG_APPCH 1*/

#ifdef USE_COLOR_STYLE
#include <AttrList.h>
#include <LYHash.h>

unsigned int cached_styles[CACHEH][CACHEW];

#endif

#include <LYJustify.h>

#ifdef CONV_JISX0201KANA_JISX0208KANA
#define is_CJK2(b) (HTCJK != NOCJK && is8bits(UCH(b)))
#else
#define is_CJK2(b) (HTCJK != NOCJK && is8bits(UCH(b)) && kanji_code != SJIS)
#endif

#ifdef USE_CURSES_PADS
#  define DISPLAY_COLS    (LYwideLines ? MAX_COLS : LYcols)
#  define WRAP_COLS(text) ((text)->stbl ?				\
			   (LYtableCols <= 0				\
			    ? DISPLAY_COLS				\
			    : (LYtableCols * LYcols)/12) - LYbarWidth	\
			   : LYcolLimit)
#else
#  define DISPLAY_COLS    LYcols
#  define WRAP_COLS(text) LYcolLimit
#endif

#define FirstHTLine(text) ((text)->last_line->next)
#define LastHTLine(text)  ((text)->last_line)

static void HText_trimHightext(HText *text, BOOLEAN final,
			       int stop_before);

#ifdef USE_COLOR_STYLE
static void LynxResetScreenCache(void)
{
    int i, j;

    for (i = 1; (i < CACHEH && i <= display_lines); i++) {
	for (j = 0; j < CACHEW; j++)
	    cached_styles[i][j] = 0;
    }
}
#endif /* USE_COLOR_STYLE */

struct _HTStream {		/* only know it as object */
    const HTStreamClass *isa;
    /* ... */
};

#define IS_UTF_EXTRA(ch) (text->T.output_utf8 && \
			  (UCH((ch))&0xc0) == 0x80)

#define IS_UTF8_EXTRA(ch) (!(text && text->T.output_utf8) || \
			  !is8bits(ch) || \
			  (UCH(line->data[i] & 0xc0) == 0xc0))

/* a test in compact form: how many extra UTF-8 chars after initial? - kw */
#define UTF8_XNEGLEN(c) (c&0xC0? 0 :c&32? 1 :c&16? 2 :c&8? 3 :c&4? 4 :c&2? 5:0)
#define UTF_XLEN(c) UTF8_XNEGLEN(((char)~(c)))

#ifdef KANJI_CODE_OVERRIDE
HTkcode last_kcode = NOKANJI;	/* 1997/11/14 (Fri) 09:09:26 */
#endif

#ifdef CJK_EX
#define CHAR_WIDTH 6
#else
#define CHAR_WIDTH 1
#endif

/*	Exports
*/
HText *HTMainText = NULL;	/* Equivalent of main window */
HTParentAnchor *HTMainAnchor = NULL;	/* Anchor for HTMainText */

const char *HTAppName = LYNX_NAME;	/* Application name */
const char *HTAppVersion = LYNX_VERSION;	/* Application version */

static int HTFormNumber = 0;
static int HTFormFields = 0;
static char *HTCurSelectGroup = NULL;	/* Form select group name */
static int HTCurSelectGroupCharset = -1;	/* ... and name's charset */
int HTCurSelectGroupType = F_RADIO_TYPE;	/* Group type */
char *HTCurSelectGroupSize = NULL;	/* Length of select */
static char *HTCurSelectedOptionValue = NULL;	/* Select choice */

const char *checked_box = "[X]";
const char *unchecked_box = "[ ]";
const char *checked_radio = "(*)";
const char *unchecked_radio = "( )";

static BOOLEAN underline_on = OFF;
static BOOLEAN bold_on = OFF;

#ifdef USE_SOURCE_CACHE
int LYCacheSource = SOURCE_CACHE_NONE;
int LYCacheSourceForAborted = SOURCE_CACHE_FOR_ABORTED_DROP;
#endif

#ifdef USE_SCROLLBAR
BOOLEAN LYShowScrollbar = FALSE;
BOOLEAN LYsb_arrow = TRUE;
int LYsb_begin = -1;
int LYsb_end = -1;
#endif

#ifndef VMS			/* VMS has a better way - right? - kw */
#define CHECK_FREE_MEM
#endif

#ifdef CHECK_FREE_MEM
static void *LY_check_calloc(size_t nmemb, size_t size);

#define LY_CALLOC LY_check_calloc
#else
  /* using the regular calloc */
#define LY_CALLOC calloc
#endif

/*
 * The HTPool.data[] array has to align the same as malloc() would, to make the
 * ALLOC_POOL scheme portable.  For many platforms, that is the same as the
 * number of bytes in a pointer.  It may be larger, e.g., on machines which
 * have more stringent requirements for floating point.  32-bits are plenty for
 * representing styles, but we may need 64-bit or 128-bit alignment.
 *
 * The real issue is that performance is degraded if the alignment is not met,
 * and some platforms such as Tru64 generate lots of warning messages.
 */
#ifndef ALIGN_SIZE
#define ALIGN_SIZE      sizeof(double)
#endif

typedef struct {
    unsigned int direction:2;	/* on or off */
    unsigned int horizpos:14;	/* horizontal position of this change */
    unsigned int style:16;	/* which style to change to */
} HTStyleChange;

#if defined(USE_COLOR_STYLE)
#define MAX_STYLES_ON_LINE   64
  /* buffers used when current line is being aggregated, in split_line() */
static HTStyleChange stylechanges_buffers[2][MAX_STYLES_ON_LINE];
#endif

typedef HTStyleChange pool_data;

enum {
    POOL_SIZE = ((8192
		  - 4 * sizeof(void *)
		  - sizeof(struct _HTPool *)
		  - sizeof(int))
		 / sizeof(pool_data))
};

typedef struct _HTPool {
    pool_data data[POOL_SIZE];
    struct _HTPool *prev;
    int used;
} HTPool;

/************************************************************************
These are generic macros for any pools (provided those structures have the
same members as HTPool).  Pools are used for allocation of groups of
objects of the same type T.  Pools are represented as a list of structures of
type P (called pool chunks here).  Structure P has an array of N objects of
type T named 'data' (the number N in the array can be chosen arbitrary),
pointer to the previous pool chunk named 'prev', and the number of used items
in that pool chunk named 'used'.  Here is a definition of the structure P:
	struct P
	{
	    T data[N];
	    struct P* prev;
	    int used;
	};
 It's recommended that sizeof(P) be memory page size minus 32 in order malloc'd
chunks to fit in machine page size.
 Allocation of 'n' items in the pool is implemented by incrementing member
'used' by 'n' if (used+n <= N), or malloc a new pool chunk and
allocating 'n' items in that new chunk.  It's the task of the programmer to
assert that 'n' is <= N.  Only entire pool may be freed - this limitation makes
allocation algorithms trivial and fast - so the use of pools is limited to
objects that are freed in batch, that are not deallocated not in the batch, and
not reallocated.
 Pools greatly reduce memory fragmentation and memory allocation/deallocation
speed due to the simple algorithms used.  Due to the fact that memory is
'allocated' in array, alignment overhead is minimal.  Allocating strings in a
pool provided their length will never exceed N and is much smaller than N seems
to be very efficient.
 [Several types of memory-hungry objects are stored in the pool now:  styles,
lines, anchors, and FormInfo. Arrays of HTStyleChange are stored as is,
other objects are stored using a cast.]

 Pool is referenced by the pointer to the last chunk that contains free slots.
Functions that allocate memory in the pool update that pointer if needed.
There are 3 functions - POOL_NEW, POOL_FREE, and ALLOC_IN_POOL.

      - VH

*************************************************************************/

#define POOLallocstyles(ptr, n)     ptr = ALLOC_IN_POOL(&HTMainText->pool, n * sizeof(pool_data))
#define POOLallocHTLine(ptr, size)  ptr = (HTLine*) ALLOC_IN_POOL(&HTMainText->pool, LINE_SIZE(size))
#define POOLallocstring(ptr, len)   ptr = (char*) ALLOC_IN_POOL(&HTMainText->pool, len + 1)
#define POOLtypecalloc(T, ptr)      ptr = (T*) ALLOC_IN_POOL(&HTMainText->pool, sizeof(T))

/**************************************************************************/
/*
 * Allocates 'n' items in the pool of type 'HTPool' pointed by 'poolptr'.
 * Returns a pointer to the "allocated" memory or NULL if fails.
 * Updates 'poolptr' if necessary.
 */
static pool_data *ALLOC_IN_POOL(HTPool ** ppoolptr, unsigned request)
{
    HTPool *pool = *ppoolptr;
    pool_data *ptr;
    unsigned n;
    unsigned j;

    if (!pool) {
	ptr = NULL;
    } else {
	n = request;
	if (n == 0)
	    n = 1;
	j = (n % ALIGN_SIZE);
	if (j != 0)
	    n += (ALIGN_SIZE - j);
	n /= sizeof(pool_data);

	if (POOL_SIZE >= (pool->used + n)) {
	    ptr = pool->data + pool->used;
	    pool->used += n;
	} else {
	    HTPool *newpool = (HTPool *) LY_CALLOC(1, sizeof(HTPool));

	    if (!newpool) {
		ptr = NULL;
	    } else {
		newpool->prev = pool;
		newpool->used = n;
		ptr = newpool->data;
		*ppoolptr = newpool;
	    }
	}
    }
    return ptr;
}

/*
 * Returns a pointer to initialized pool of type 'HTPool', or NULL if fails.
 */
static HTPool *POOL_NEW(void)
{
    HTPool *poolptr = (HTPool *) LY_CALLOC(1, sizeof(HTPool));

    if (poolptr) {
	poolptr->prev = NULL;
	poolptr->used = 0;
    }
    return poolptr;
}

/*
 * Frees a pool of type 'HTPool' pointed by poolptr.
 */
static void POOL_FREE(HTPool * poolptr)
{
    HTPool *cur = poolptr;
    HTPool *prev;

    while (cur) {
	prev = cur->prev;
	free(cur);
	cur = prev;
    }
}

/**************************************************************************/
/**************************************************************************/

typedef struct _line {
    struct _line *next;
    struct _line *prev;
    unsigned short offset;	/* Implicit initial spaces */
    unsigned short size;	/* Number of characters */
#if defined(USE_COLOR_STYLE)
    HTStyleChange *styles;
    unsigned short numstyles;
#endif
    char data[1];		/* Space for terminator at least! */
} HTLine;

#define LINE_SIZE(size) (sizeof(HTLine)+(size))		/* Allow for terminator */

#ifndef HTLINE_NOT_IN_POOL
#define HTLINE_NOT_IN_POOL 0	/* debug with this set to 1 */
#endif

#if HTLINE_NOT_IN_POOL
#define allocHTLine(ptr, size)  { ptr = (HTLine *)calloc(1, LINE_SIZE(size)); }
#define freeHTLine(self, ptr)   { \
	if (ptr && ptr != TEMP_LINE(self, 0) && ptr != TEMP_LINE(self, 1)) \
	    FREE(ptr); \
    }
#else
#define allocHTLine(ptr, size)  POOLallocHTLine(ptr, size)
#define freeHTLine(self, ptr)   {}
#endif

/*
 * Last line buffer; the second is used in split_line(). Not in pool!
 * We cannot wrap in middle of multibyte sequences, so allocate 2 extra
 * for a workspace.  This is stored in the HText, to prevent confusion
 * between different documents.  Note also that it is declared with an
 * HTLine at the beginning so pointers will be properly aligned.
 */
typedef struct {
    HTLine base;
    char data[MAX_LINE + 2];
} HTLineTemp;

#define TEMP_LINE(p,n) ((HTLine *)&(p->temp_line[n]))

typedef struct _TextAnchor {
    struct _TextAnchor *next;
    struct _TextAnchor *prev;	/* www_user_search only! */
    int sgml_offset;		/* used for updating position after reparsing */
    int number;			/* For user interface */
    int line_num;		/* Place in document */
    short line_pos;		/* Bytes/chars - extent too */
    short extent;		/* (see HText_trimHightext) */
    BOOL show_anchor;		/* Show the anchor? */
    BOOL inUnderline;		/* context is underlined */
    BOOL expansion_anch;	/* TEXTAREA edit new anchor */
    char link_type;		/* Normal, internal, or form? */
    FormInfo *input_field;	/* Info for form links */
    HiliteList lites;

    HTChildAnchor *anchor;
} TextAnchor;

typedef struct {
    char *name;			/* ID value of TAB */
    int column;			/* Zero-based column value */
} HTTabID;

typedef enum {
    S_text,
    S_esc,
    S_dollar,
    S_paren,
    S_nonascii_text,
    S_dollar_paren,
    S_jisx0201_text
} eGridState;			/* Escape sequence? */

#ifdef USE_TH_JP_AUTO_DETECT
typedef enum {			/* Detected Kanji code */
    DET_SJIS,
    DET_EUC,
    DET_NOTYET,
    DET_MIXED
} eDetectedKCode;

typedef enum {
    SJIS_state_neutral,
    SJIS_state_in_kanji,
    SJIS_state_has_bad_code
} eSJIS_status;

typedef enum {
    EUC_state_neutral,
    EUC_state_in_kanji,
    EUC_state_in_kana,
    EUC_state_has_bad_code
} eEUC_status;
#endif

/*	Notes on struct _HText:
 *	next_line is valid if stale is false.
 *	top_of_screen line means the line at the top of the screen
 *			or just under the title if there is one.
 */
struct _HText {
    HTParentAnchor *node_anchor;

    HTLine *last_line;
    HTLineTemp temp_line[2];
    int Lines;			/* Number of them */
    TextAnchor *first_anchor;	/* double-linked on demand */
    TextAnchor *last_anchor;
    TextAnchor *last_anchor_before_stbl;
    TextAnchor *last_anchor_before_split;
    HTList *forms;		/* also linked internally */
    int last_anchor_number;	/* user number */
    BOOL source;		/* Is the text source? */
    BOOL toolbar;		/* Toolbar set? */
    HTList *tabs;		/* TAB IDs */
    HTList *hidden_links;	/* Content-less links ... */
    int hiddenlinkflag;		/*  ... and how to treat them */
    BOOL no_cache;		/* Always refresh? */
    char LastChar;		/* For absorbing white space */
    BOOL IgnoreExcess;		/* Ignore chars at wrap point */

/* For Internal use: */
    HTStyle *style;		/* Current style */
    int display_on_the_fly;	/* Lines left */
    int top_of_screen;		/* Line number */
    HTLine *top_of_screen_line;	/* Top */
    HTLine *next_line;		/* Bottom + 1 */
    unsigned permissible_split;	/* in last line */
    BOOL in_line_1;		/* of paragraph */
    BOOL stale;			/* Must refresh */
    BOOL page_has_target;	/* has target on screen */
    BOOL has_utf8;		/* has utf-8 on screen or line */
    BOOL had_utf8;		/* had utf-8 when last displayed */
#ifdef DISP_PARTIAL
    int first_lineno_last_disp_partial;
    int last_lineno_last_disp_partial;
#endif
    STable_info *stbl;
    HTList *enclosed_stbl;

    HTkcode kcode;		/* Kanji code? */
    HTkcode specified_kcode;	/* Specified Kanji code */
#ifdef USE_TH_JP_AUTO_DETECT
    eDetectedKCode detected_kcode;
    eSJIS_status SJIS_status;
    eEUC_status EUC_status;
#endif
    eGridState state;		/* Escape sequence? */
    int kanji_buf;		/* Lead multibyte */
    int in_sjis;		/* SJIS flag */
    int halted;			/* emergency halt */

    BOOL have_8bit_chars;	/* Any non-ASCII chars? */
    LYUCcharset *UCI;		/* node_anchor UCInfo */
    int UCLYhndl;		/* charset we are fed */
    UCTransParams T;

    HTStream *target;		/* Output stream */
    HTStreamClass targetClass;	/* Output routines */

    HTPool *pool;		/* this HText memory pool */

#ifdef USE_SOURCE_CACHE
    /*
     * Parse settings when this HText was generated.
     */
    BOOL clickable_images;
    BOOL pseudo_inline_alts;
    BOOL verbose_img;
    BOOL raw_mode;
    BOOL historical_comments;
    BOOL minimal_comments;
    BOOL soft_dquotes;
    short old_dtd;
    short keypad_mode;
    short disp_lines;		/* Screen size */
    short disp_cols;		/* Used for reports only */
#endif
};

/* exported */
void *HText_pool_calloc(HText *text, unsigned size)
{
    return (void *) ALLOC_IN_POOL(&text->pool, size);
}

static void HText_AddHiddenLink(HText *text, TextAnchor *textanchor);

#ifdef EXP_JUSTIFY_ELTS
BOOL can_justify_here;
BOOL can_justify_here_saved;

BOOL can_justify_this_line;	/* =FALSE if line contains form objects */
int wait_for_this_stacked_elt;	/* -1 if can justify contents of the

				   element on the op of stack. If positive - specifies minimal stack depth
				   plus 1 at which we can justify element (can be MAX_LINE+2 if
				   ok_justify ==FALSE or in psrcview. */
BOOL form_in_htext;		/*to indicate that we are in form (since HTML_FORM is

				   not stacked in the HTML.c */
BOOL in_DT = FALSE;

#ifdef DEBUG_JUSTIFY
BOOL can_justify_stack_depth;	/* can be 0 or 1 if all code is correct */
#endif

typedef struct {
    int byte_len;		/*length in bytes */
    int cell_len;		/*length in cells */
} ht_run_info;

static int justify_start_position;	/* this is an index of char from which

					   justification can start (eg after "* " preceeding <li> text) */

static int ht_num_runs;		/*the number of runs filled */
static ht_run_info ht_runs[MAX_LINE];
static BOOL this_line_was_split;
static TextAnchor *last_anchor_of_previous_line;
static BOOL have_raw_nbsps = FALSE;

void ht_justify_cleanup(void)
{
    wait_for_this_stacked_elt = !ok_justify
#  ifdef USE_PRETTYSRC
	|| psrc_view
#  endif
	? 30000 /*MAX_NESTING */  + 2	/*some unreachable value */
	: -1;
    can_justify_here = TRUE;
    can_justify_this_line = TRUE;
    form_in_htext = FALSE;

    last_anchor_of_previous_line = NULL;
    this_line_was_split = FALSE;
    in_DT = FALSE;
    have_raw_nbsps = FALSE;
}

void mark_justify_start_position(void *text)
{
    if (text && ((HText *) text)->last_line)
	justify_start_position = ((HText *) text)->last_line->size;
}

#define REALLY_CAN_JUSTIFY(text) ( (wait_for_this_stacked_elt<0) && \
	( text->style->alignment == HT_LEFT     || \
	  text->style->alignment == HT_JUSTIFY) && \
	HTCJK == NOCJK && !in_DT && \
	can_justify_here && can_justify_this_line && !form_in_htext )

#endif /* EXP_JUSTIFY_ELTS */

/*
 * Boring static variable used for moving cursor across
 */
#define UNDERSCORES(n) \
 ((n) >= MAX_LINE ? underscore_string : &underscore_string[(MAX_LINE-1)] - (n))

/*
 *	Memory leak fixed.
 *	05-29-94 Lynx 2-3-1 Garrett Arch Blythe
 *	Changed to arrays.
 */
static char underscore_string[MAX_LINE + 1];
char star_string[MAX_LINE + 1];

static int ctrl_chars_on_this_line = 0;		/* num of ctrl chars in current line */
static int utfxtra_on_this_line = 0;	/* num of UTF-8 extra bytes in line,

					   they *also* count as ctrl chars. */
#ifdef WIDEC_CURSES
#define UTFXTRA_ON_THIS_LINE 0
#else
#define UTFXTRA_ON_THIS_LINE utfxtra_on_this_line
#endif

static HTStyle default_style =
{0, "(Unstyled)", 0, "",
 (HTFont) 0, 1, HT_BLACK, 0, 0,
 0, 0, 0, HT_LEFT, 1, 0, 0,
 NO, NO, 0, 0, 0};

static HTList *loaded_texts = NULL;	/* A list of all those in memory */
HTList *search_queries = NULL;	/* isindex and whereis queries   */

#ifdef LY_FIND_LEAKS
static void free_all_texts(void);
#endif

static BOOL HText_TrueEmptyLine(HTLine *line, HText *text, BOOL IgnoreSpaces);

static int HText_TrueLineSize(HTLine *line, HText *text, BOOL IgnoreSpaces);

#ifdef CHECK_FREE_MEM

/*
 * text->halted = 1: have set fake 'Z' and output a message
 *		  2: next time when HText_appendCharacter is called
 *		     it will append *** MEMORY EXHAUSTED ***, then set
 *		     to 3.
 *		  3: normal text output will be suppressed (but not anchors,
 *		     form fields etc.)
 */
static void HText_halt(void)
{
    if (HTFormNumber > 0)
	HText_DisableCurrentForm();
    if (!HTMainText)
	return;
    if (HTMainText->halted < 2)
	HTMainText->halted = 2;
}

#define MIN_NEEDED_MEM 5000

/*
 * Check whether factor*min(bytes,MIN_NEEDED_MEM) is available,
 * or bytes if factor is 0.
 * MIN_NEEDED_MEM and factor together represent a security margin,
 * to take account of all the memory allocations where we don't check
 * and of buffers which may be emptied before HTCheckForInterupt()
 * is (maybe) called and other things happening, with some chance of
 * success.
 * This just tries to malloc() the to-be-checked-for amount of memory,
 * which might make the situation worse depending how allocation works.
 * There should be a better way...  - kw
 */
static BOOL mem_is_avail(size_t factor, size_t bytes)
{
    void *p;

    if (bytes < MIN_NEEDED_MEM && factor > 0)
	bytes = MIN_NEEDED_MEM;
    if (factor == 0)
	factor = 1;
    p = malloc(factor * bytes);
    if (p) {
	FREE(p);
	return YES;
    } else {
	return NO;
    }
}

/*
 * Replacement for calloc which checks for "enough" free memory
 * (with some security margins) and tries various recovery actions
 * if deemed necessary.  - kw
 */
static void *LY_check_calloc(size_t nmemb, size_t size)
{
    int i, n;

    if (mem_is_avail(4, nmemb * size)) {
	return (calloc(nmemb, size));
    }
    n = HTList_count(loaded_texts);
    for (i = n - 1; i > 0; i--) {
	HText *t = (HText *) HTList_objectAt(loaded_texts, i);

	CTRACE((tfp,
		"\nBUG *** Emergency freeing document %d/%d for '%s'%s!\n",
		i + 1, n,
		((t && t->node_anchor &&
		  t->node_anchor->address) ?
		 t->node_anchor->address : "unknown anchor"),
		((t && t->node_anchor &&
		  t->node_anchor->post_data) ?
		 " with POST data" : "")));
	HTList_removeObjectAt(loaded_texts, i);
	HText_free(t);
	if (mem_is_avail(4, nmemb * size)) {
	    return (calloc(nmemb, size));
	}
    }
    LYFakeZap(YES);
    if (!HTMainText || HTMainText->halted <= 1) {
	if (!mem_is_avail(2, nmemb * size)) {
	    HText_halt();
	    if (mem_is_avail(0, 700)) {
		HTAlert(gettext("Memory exhausted, display interrupted!"));
	    }
	} else {
	    if ((!HTMainText || HTMainText->halted == 0) &&
		mem_is_avail(0, 700)) {
		HTAlert(gettext("Memory exhausted, will interrupt transfer!"));
		if (HTMainText)
		    HTMainText->halted = 1;
	    }
	}
    }
    return (calloc(nmemb, size));
}

#endif /* CHECK_FREE_MEM */

#ifdef USE_COLOR_STYLE
/*
 * Color style information is stored with the multibyte-character offset into
 * the string at which the style would apply.  Compute the corresponding column
 * so we can compare it with the updated column value after writing strings
 * with curses.
 *
 * The offsets count multibyte characters.  Other parts of the code assume each
 * character uses one cell, but some CJK (or UTF-8) codes use two cells.  We
 * need to know the number of cells.
 */
static int StyleToCols(HText *text, HTLine *line, int nstyle)
{
    int result = line->offset;	/* this much is spaces one byte/cell */
    int nchars = line->styles[nstyle].horizpos;
    char *data = line->data;
    char *last = line->size + data;
    int utf_extra;

    while (nchars > 0 && data < last) {
	if (IsSpecialAttrChar(*data) && *data != LY_SOFT_NEWLINE) {
	    ++data;
	} else {
	    utf_extra = utf8_length(text->T.output_utf8, data);
	    if (utf_extra++) {
		result += LYstrExtent(data, utf_extra, 2);
		data += utf_extra;
	    } else if (is_CJK2(*data)) {
		data += 2;
		result += 2;
	    } else {
		++data;
		++result;
	    }
	    --nchars;
	}
    }

    return result;
}
#endif

/*
 * Clear highlight information for a given anchor
 * (text was allocated in the pool).
 */
static void LYClearHiText(TextAnchor *a)
{
    FREE(a->lites.hl_info);

    a->lites.hl_base.hl_text = NULL;
    a->lites.hl_len = 0;
}

#define LYFreeHiText(a)     FREE((a)->lites.hl_info)

/*
 * Set the initial highlight information for a given anchor.
 */
static void LYSetHiText(TextAnchor *a,
			const char *text,
			int len)
{
    if (text != NULL) {
	POOLallocstring(a->lites.hl_base.hl_text, len + 1);
	memcpy(a->lites.hl_base.hl_text, text, len);
	*(a->lites.hl_base.hl_text + len) = '\0';

	a->lites.hl_len = 1;
    }
}

/*
 * Add highlight information for the next line of a anchor.
 */
static void LYAddHiText(TextAnchor *a,
			const char *text,
			int x)
{
    HiliteInfo *have = a->lites.hl_info;
    unsigned need = (a->lites.hl_len - 1);
    unsigned want = (a->lites.hl_len += 1) * sizeof(HiliteInfo);

    if (have != NULL) {
	have = (HiliteInfo *) realloc(have, want);
    } else {
	have = (HiliteInfo *) malloc(want);
    }
    a->lites.hl_info = have;

    POOLallocstring(have[need].hl_text, strlen(text) + 1);
    strcpy(have[need].hl_text, text);
    have[need].hl_x = x;
}

/*
 * Return an offset to skip leading blanks in the highlighted link.  That is
 * needed to avoid having the color-style paint the leading blanks.
 */
#ifdef USE_COLOR_STYLE
static int LYAdjHiTextPos(TextAnchor *a, int count)
{
    char *result;

    if (count >= a->lites.hl_len)
	result = NULL;
    else if (count > 0)
	result = a->lites.hl_info[count - 1].hl_text;
    else
	result = a->lites.hl_base.hl_text;

    return (result != 0) ? (LYSkipBlanks(result) - result) : 0;
}
#else
#define LYAdjHiTextPos(a,count) 0
#endif

/*
 * Get the highlight text, counting from zero.
 */
static char *LYGetHiTextStr(TextAnchor *a, int count)
{
    char *result;

    if (count >= a->lites.hl_len)
	result = NULL;
    else if (count > 0)
	result = a->lites.hl_info[count - 1].hl_text;
    else
	result = a->lites.hl_base.hl_text;
    result += LYAdjHiTextPos(a, count);
    return result;
}

/*
 * Get the X-ordinate at which to draw the corresponding highlight-text
 */
static int LYGetHiTextPos(TextAnchor *a, int count)
{
    int result;

    if (count >= a->lites.hl_len)
	result = -1;
    else if (count > 0)
	result = a->lites.hl_info[count - 1].hl_x;
    else
	result = a->line_pos;
    result += LYAdjHiTextPos(a, count);
    return result;
}

/*
 * Copy highlighting information from anchor 'b' to 'a'.
 */
static void LYCopyHiText(TextAnchor *a, TextAnchor *b)
{
    int count;
    char *s;

    LYClearHiText(a);
    for (count = 0;; ++count) {
	if ((s = LYGetHiTextStr(b, count)) == NULL)
	    break;
	if (count == 0) {
	    LYSetHiText(a, s, strlen(s));
	} else {
	    LYAddHiText(a, s, LYGetHiTextPos(b, count));
	}
    }
}

static void HText_getChartransInfo(HText *me)
{
    me->UCLYhndl = HTAnchor_getUCLYhndl(me->node_anchor, UCT_STAGE_HTEXT);
    if (me->UCLYhndl < 0) {
	int chndl = current_char_set;

	HTAnchor_setUCInfoStage(me->node_anchor, chndl,
				UCT_STAGE_HTEXT, UCT_SETBY_STRUCTURED);
	me->UCLYhndl = HTAnchor_getUCLYhndl(me->node_anchor,
					    UCT_STAGE_HTEXT);
    }
    me->UCI = HTAnchor_getUCInfoStage(me->node_anchor, UCT_STAGE_HTEXT);
}

static void PerFormInfo_free(PerFormInfo * form)
{
    if (form) {
	FREE(form->accept_cs);
	FREE(form->thisacceptcs);
	FREE(form);
    }
}

static void free_form_fields(FormInfo * input_field)
{
    /*
     * Free form fields.
     */
    if (input_field->type == F_OPTION_LIST_TYPE &&
	input_field->select_list != NULL) {
	/*
	 * Free off option lists if present.
	 * It should always be present for F_OPTION_LIST_TYPE
	 * unless we had invalid markup which prevented
	 * HText_setLastOptionValue from finishing its job
	 * and left the input field in an insane state.  - kw
	 */
	OptionType *optptr = input_field->select_list;
	OptionType *tmp;

	while (optptr) {
	    tmp = optptr;
	    optptr = tmp->next;
	    FREE(tmp->name);
	    FREE(tmp->cp_submit_value);
	    FREE(tmp);
	}
	input_field->select_list = NULL;
	/*
	 * Don't free the value field on option
	 * lists since it points to a option value
	 * same for orig value.
	 */
	input_field->value = NULL;
	input_field->orig_value = NULL;
	input_field->cp_submit_value = NULL;
	input_field->orig_submit_value = NULL;
    } else {
	FREE(input_field->value);
	FREE(input_field->orig_value);
	FREE(input_field->cp_submit_value);
	FREE(input_field->orig_submit_value);
    }
    FREE(input_field->name);
    FREE(input_field->submit_action);
    FREE(input_field->submit_enctype);
    FREE(input_field->submit_title);

    FREE(input_field->accept_cs);
}

static void FormList_delete(HTList *forms)
{
    HTList *cur = forms;
    PerFormInfo *form;

    while ((form = (PerFormInfo *) HTList_nextObject(cur)) != NULL)
	PerFormInfo_free(form);
    HTList_delete(forms);
}

#ifdef DISP_PARTIAL
static void ResetPartialLinenos(HText *text)
{
    if (text != 0) {
	text->first_lineno_last_disp_partial = -1;
	text->last_lineno_last_disp_partial = -1;
    }
}
#endif

/*			Creation Method
 *			---------------
 */
HText *HText_new(HTParentAnchor *anchor)
{
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
#include <lib$routines.h>
    int status, VMType = 3, VMTotal;
#endif /* VMS && VAXC && !__DECC */
    HTLine *line = NULL;
    HText *self = typecalloc(HText);

    if (!self)
	return self;

    CTRACE((tfp, "GridText: start HText_new\n"));

#if defined(VMS) && defined (VAXC) && !defined(__DECC)
    status = lib$stat_vm(&VMType, &VMTotal);
    CTRACE((tfp, "GridText: VMTotal = %d\n", VMTotal));
#endif /* VMS && VAXC && !__DECC */

    /*
     * If the previously shown text had UTF-8 characters on screen,
     * remember this in the newly created object.  Do this now, before
     * the previous object may become invalid.  - kw
     */
    if (HTMainText) {
	if (HText_hasUTF8OutputSet(HTMainText) &&
	    HTLoadedDocumentEightbit() &&
	    LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8) {
	    self->had_utf8 = HTMainText->has_utf8;
	} else {
	    self->had_utf8 = HTMainText->has_utf8;
	}
	HTMainText->has_utf8 = NO;
    }

    if (!loaded_texts) {
	loaded_texts = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(free_all_texts);
#endif
    }

    /*
     * Links between anchors & documents are a 1-1 relationship.  If
     * an anchor is already linked to a document we didn't call
     * HTuncache_current_document(), so we'll check now
     * and free it before reloading.  - Dick Wesseling (ftu@fi.ruu.nl)
     */
    if (anchor->document) {
	HTList_removeObject(loaded_texts, anchor->document);
	CTRACE((tfp, "GridText: Auto-uncaching\n"));

	HTAnchor_delete_links(anchor);
	((HText *) anchor->document)->node_anchor = NULL;
	HText_free((HText *) anchor->document);
	anchor->document = NULL;
    }

    HTList_addObject(loaded_texts, self);
#if defined(VMS) && defined(VAXC) && !defined(__DECC)
    while (HTList_count(loaded_texts) > HTCacheSize &&
	   VMTotal > HTVirtualMemorySize)
#else
    if (HTList_count(loaded_texts) > HTCacheSize)
#endif /* VMS && VAXC && !__DECC */
    {
	CTRACE((tfp, "GridText: Freeing off cached doc.\n"));
	HText_free((HText *) HTList_removeFirstObject(loaded_texts));
#if defined(VMS) && defined (VAXC) && !defined(__DECC)
	status = lib$stat_vm(&VMType, &VMTotal);
	CTRACE((tfp, "GridText: VMTotal reduced to %d\n", VMTotal));
#endif /* VMS && VAXC && !__DECC */
    }

    self->pool = POOL_NEW();
    if (!self->pool)
	outofmem(__FILE__, "HText_New");

    line = self->last_line = TEMP_LINE(self, 0);
    line->next = line->prev = line;
    line->offset = line->size = 0;
    line->data[line->size] = '\0';
#ifdef USE_COLOR_STYLE
    line->numstyles = 0;
    line->styles = stylechanges_buffers[0];
#endif
    self->Lines = 0;
    self->first_anchor = self->last_anchor = NULL;
    self->last_anchor_before_split = NULL;
    self->style = &default_style;
    self->top_of_screen = 0;
    self->node_anchor = anchor;
    self->last_anchor_number = 0;	/* Numbering of them for references */
    self->stale = YES;
    self->toolbar = NO;
    self->tabs = NULL;
#ifdef USE_SOURCE_CACHE
    /*
     * Remember the parse settings.
     */
    self->clickable_images = clickable_images;
    self->pseudo_inline_alts = pseudo_inline_alts;
    self->verbose_img = verbose_img;
    self->raw_mode = LYUseDefaultRawMode;
    self->historical_comments = historical_comments;
    self->minimal_comments = minimal_comments;
    self->soft_dquotes = soft_dquotes;
    self->old_dtd = Old_DTD;
    self->keypad_mode = keypad_mode;
    self->disp_lines = LYlines;
    self->disp_cols = DISPLAY_COLS;
#endif
    /*
     * If we are going to render the List Page, always merge in hidden
     * links to get the numbering consistent if form fields are numbered
     * and show up as hidden links in the list of links.
     * If we are going to render a bookmark file, also always merge in
     * hidden links, to get the link numbers consistent with the counting
     * in remove_bookmark_link().  Normally a bookmark file shouldn't
     * contain any entries with empty titles, but it might happen.  - kw
     */
    if (anchor->bookmark ||
	LYIsUIPage3(anchor->address, UIP_LIST_PAGE, 0) ||
	LYIsUIPage3(anchor->address, UIP_ADDRLIST_PAGE, 0))
	self->hiddenlinkflag = HIDDENLINKS_MERGE;
    else
	self->hiddenlinkflag = LYHiddenLinks;
    self->hidden_links = NULL;
    self->no_cache = ((anchor->no_cache ||
		       anchor->post_data)
		      ? YES
		      : NO);
    self->LastChar = '\0';
    self->IgnoreExcess = FALSE;

#ifndef USE_PRETTYSRC
    if (HTOutputFormat == WWW_SOURCE)
	self->source = YES;
    else
	self->source = NO;
#else
    /* mark_htext_as_source == TRUE if we are parsing html file (and psrc_view
     * is set temporary to false at creation time)
     *
     * psrc_view == TRUE if source of the text produced by some lynx module
     * (like ftp browsers) is requested).  - VH
     */
    self->source = (BOOL) (LYpsrc
			   ? mark_htext_as_source || psrc_view
			   : HTOutputFormat == WWW_SOURCE);
    mark_htext_as_source = FALSE;
#endif
    HTAnchor_setDocument(anchor, (HyperDoc *) self);
    HTFormNumber = 0;		/* no forms started yet */
    HTMainText = self;
    HTMainAnchor = anchor;
    self->display_on_the_fly = 0;
    self->kcode = NOKANJI;
    self->specified_kcode = NOKANJI;
#ifdef USE_TH_JP_AUTO_DETECT
    self->detected_kcode = DET_NOTYET;
    self->SJIS_status = SJIS_state_neutral;
    self->EUC_status = EUC_state_neutral;
#endif
    self->state = S_text;
    self->kanji_buf = '\0';
    self->in_sjis = 0;
    self->have_8bit_chars = NO;
    HText_getChartransInfo(self);
    UCSetTransParams(&self->T,
		     self->UCLYhndl, self->UCI,
		     current_char_set,
		     &LYCharSet_UC[current_char_set]);

    /*
     * Check the kcode setting if the anchor has a charset element.  -FM
     */
    HText_setKcode(self, anchor->charset,
		   HTAnchor_getUCInfoStage(anchor, UCT_STAGE_HTEXT));

    /*
     * Memory leak fixed.
     * 05-29-94 Lynx 2-3-1 Garrett Arch Blythe
     * Check to see if our underline and star_string need initialization
     * if the underline is not filled with dots.
     */
    if (underscore_string[0] != '.') {
	/*
	 * Create an array of dots for the UNDERSCORES macro.  -FM
	 */
	memset(underscore_string, '.', (MAX_LINE - 1));
	underscore_string[(MAX_LINE - 1)] = '\0';
	underscore_string[MAX_LINE] = '\0';
	/*
	 * Create an array of underscores for the STARS macro.  -FM
	 */
	memset(star_string, '_', (MAX_LINE - 1));
	star_string[(MAX_LINE - 1)] = '\0';
	star_string[MAX_LINE] = '\0';
    }

    underline_on = FALSE;	/* reset */
    bold_on = FALSE;

#ifdef DISP_PARTIAL
    /*
     * By this function we create HText object
     * so we may start displaying the document while downloading. - LP
     */
    if (display_partial_flag) {
	display_partial = TRUE;	/* enable HTDisplayPartial() */
	NumOfLines_partial = 0;	/* initialize */
    }

    /*
     * These two fields should only be set to valid line numbers
     * by calls of display_page during partial displaying.  This
     * is just so that the FIRST display_page AFTER that can avoid
     * repainting the same lines on the screen.  - kw
     */
    ResetPartialLinenos(self);
#endif

#ifdef EXP_JUSTIFY_ELTS
    ht_justify_cleanup();
#endif
    return self;
}

/*			Creation Method 2
 *			---------------
 *
 *      Stream is assumed open and left open.
 */
HText *HText_new2(HTParentAnchor *anchor,
		  HTStream *stream)
{
    HText *result = HText_new(anchor);

    if (stream) {
	result->target = stream;
	result->targetClass = *stream->isa;	/* copy action procedures */
    }
    return result;
}

/*	Free Entire Text
 *	----------------
 */
void HText_free(HText *self)
{
    if (!self)
	return;

#if HTLINE_NOT_IN_POOL
    {
	HTLine *f = FirstHTLine(self);
	HTLine *l = self->last_line;

	while (l != f) {	/* Free off line array */
	    self->last_line = l->prev;
	    freeHTLine(self, l);
	    l = self->last_line;
	}
	freeHTLine(self, f);
    }
#endif

    while (self->first_anchor) {	/* Free off anchor array */
	TextAnchor *l = self->first_anchor;

	self->first_anchor = l->next;

	if (l->link_type == INPUT_ANCHOR && l->input_field) {
	    free_form_fields(l->input_field);
	}

	LYFreeHiText(l);
    }
    FormList_delete(self->forms);

    /*
     * Free the tabs list.  -FM
     */
    if (self->tabs) {
	HTTabID *Tab = NULL;
	HTList *cur = self->tabs;

	while (NULL != (Tab = (HTTabID *) HTList_nextObject(cur))) {
	    FREE(Tab->name);
	    FREE(Tab);
	}
	HTList_delete(self->tabs);
	self->tabs = NULL;
    }

    /*
     * Free the hidden links list.  -FM
     */
    if (self->hidden_links) {
	LYFreeStringList(self->hidden_links);
	self->hidden_links = NULL;
    }

    /*
     * Invoke HTAnchor_delete() to free the node_anchor
     * if it is not a destination of other links.  -FM
     */
    if (self->node_anchor) {
	HTAnchor_resetUCInfoStage(self->node_anchor, -1, UCT_STAGE_STRUCTURED,
				  UCT_SETBY_NONE);
	HTAnchor_resetUCInfoStage(self->node_anchor, -1, UCT_STAGE_HTEXT,
				  UCT_SETBY_NONE);
#ifdef USE_SOURCE_CACHE
	/* Remove source cache files and chunks always, even if the
	 * HTAnchor_delete call does not actually remove the anchor.
	 * Keeping them would just be a waste of space - they won't
	 * be used any more after the anchor has been disassociated
	 * from a HText structure. - kw
	 */
	HTAnchor_clearSourceCache(self->node_anchor);
#endif

	HTAnchor_delete_links(self->node_anchor);

	HTAnchor_setDocument(self->node_anchor, (HyperDoc *) 0);

	if (HTAnchor_delete(self->node_anchor->parent))
	    /*
	     * Make sure HTMainAnchor won't point
	     * to an invalid structure.  - KW
	     */
	    HTMainAnchor = NULL;
    }

    POOL_FREE(self->pool);
    FREE(self);
}

/*		Display Methods
 *		---------------
 */

/*	Output a line
 *	-------------
 */
static int display_line(HTLine *line,
			HText *text,
			int scrline GCC_UNUSED,
			const char *target GCC_UNUSED)
{
    register int i, j;
    char buffer[7];
    char *data;
    size_t utf_extra = 0;
    char LastDisplayChar = ' ';

#ifdef USE_COLOR_STYLE
    int current_style = 0;

#define inunderline NO
#define inbold NO
#else
    BOOL inbold = NO, inunderline = NO;
#endif
#if defined(SHOW_WHEREIS_TARGETS) && !defined(USE_COLOR_STYLE)
    const char *cp_tgt;
    int i_start_tgt = 0, i_after_tgt;
    int HitOffset, LenNeeded;
    BOOL intarget = NO;

#else
#define intarget NO
#endif /* SHOW_WHEREIS_TARGETS && !USE_COLOR_STYLE */

#if !(defined(NCURSES_VERSION) || defined(WIDEC_CURSES))
    text->has_utf8 = NO;	/* use as per-line flag, except with ncurses */
#endif

#if defined(WIDEC_CURSES)
    /*
     * FIXME: this should not be necessary, but in some wide-character pages
     * the output line wraps, foiling our attempt to just use newlines to
     * advance to the next page.
     */
    LYmove(scrline + TITLE_LINES - 1, 0);
#endif

    /*
     * Set up the multibyte character buffer,
     * and clear the line to which we will be
     * writing.
     */
    buffer[0] = buffer[1] = buffer[2] = '\0';
    LYclrtoeol();

    /*
     * Add offset, making sure that we do not
     * go over the COLS limit on the display.
     */
    j = (int) line->offset;
    if (j >= DISPLAY_COLS)
	j = DISPLAY_COLS - 1;
#ifdef USE_SLANG
    SLsmg_forward(j);
    i = j;
#else
#ifdef USE_COLOR_STYLE
    if (line->size == 0)
	i = j;
    else
#endif
	for (i = 0; i < j; i++)
	    LYaddch(' ');
#endif /* USE_SLANG */

    /*
     * Add the data, making sure that we do not
     * go over the COLS limit on the display.
     */
    data = line->data;
    i++;

#ifndef USE_COLOR_STYLE
#if defined(SHOW_WHEREIS_TARGETS)
    /*
     * If the target is on this line, it will be emphasized.
     */
    i_after_tgt = i;
    if (target) {
	cp_tgt = LYno_attr_mb_strstr(data,
				     target,
				     text->T.output_utf8, YES,
				     &HitOffset,
				     &LenNeeded);
	if (cp_tgt) {
	    if (((int) line->offset + LenNeeded) >= DISPLAY_COLS) {
		cp_tgt = NULL;
	    } else {
		text->page_has_target = YES;
		i_start_tgt = i + HitOffset;
		i_after_tgt = i + LenNeeded;
	    }
	}
    } else {
	cp_tgt = NULL;
    }
#endif /* SHOW_WHEREIS_TARGETS */
#endif /* USE_COLOR_STYLE */

    while ((i <= DISPLAY_COLS) && ((buffer[0] = *data) != '\0')) {

#ifndef USE_COLOR_STYLE
#if defined(SHOW_WHEREIS_TARGETS)
	if (cp_tgt && i >= i_after_tgt) {
	    if (intarget) {
		cp_tgt = LYno_attr_mb_strstr(data,
					     target,
					     text->T.output_utf8, YES,
					     &HitOffset,
					     &LenNeeded);
		if (cp_tgt) {
		    i_start_tgt = i + HitOffset;
		    i_after_tgt = i + LenNeeded;
		}
		if (!cp_tgt || i_start_tgt != i) {
		    LYstopTargetEmphasis();
		    intarget = NO;
		    if (inbold)
			lynx_start_bold();
		    if (inunderline)
			lynx_start_underline();
		}
	    }
	}
#endif /* SHOW_WHEREIS_TARGETS */
#endif /* USE_COLOR_STYLE */

	data++;

#if defined(USE_COLOR_STYLE)
#define CStyle line->styles[current_style]

	while (current_style < line->numstyles &&
	       i >= (int) (CStyle.horizpos + line->offset + 1)) {
	    LynxChangeStyle(CStyle.style, CStyle.direction);
	    current_style++;
	}
#endif
	switch (buffer[0]) {

#ifndef USE_COLOR_STYLE
	case LY_UNDERLINE_START_CHAR:
	    if (dump_output_immediately && use_underscore) {
		LYaddch('_');
		i++;
	    } else {
		inunderline = YES;
		if (!intarget) {
#if defined(PDCURSES)
		    if (LYShowColor == SHOW_COLOR_NEVER)
			lynx_start_bold();
		    else
			lynx_start_underline();
#else
		    lynx_start_underline();
#endif /* PDCURSES */
		}
	    }
	    break;

	case LY_UNDERLINE_END_CHAR:
	    if (dump_output_immediately && use_underscore) {
		LYaddch('_');
		i++;
	    } else {
		inunderline = NO;
		if (!intarget) {
#if defined(PDCURSES)
		    if (LYShowColor == SHOW_COLOR_NEVER)
			lynx_stop_bold();
		    else
			lynx_stop_underline();
#else
		    lynx_stop_underline();
#endif /* PDCURSES */
		}
	    }
	    break;

	case LY_BOLD_START_CHAR:
	    inbold = YES;
	    if (!intarget)
		lynx_start_bold();
	    break;

	case LY_BOLD_END_CHAR:
	    inbold = NO;
	    if (!intarget)
		lynx_stop_bold();
	    break;

#endif /* !USE_COLOR_STYLE */
	case LY_SOFT_NEWLINE:
	    if (!dump_output_immediately) {
		LYaddch('+');
		i++;
#if defined(SHOW_WHEREIS_TARGETS) && !defined(USE_COLOR_STYLE)
		i_after_tgt++;
#endif
	    }
	    break;

	case LY_SOFT_HYPHEN:
	    if (*data != '\0' ||
		isspace(UCH(LastDisplayChar)) ||
		LastDisplayChar == '-') {
		/*
		 * Ignore the soft hyphen if it is not the last character in
		 * the line.  Also ignore it if it first character following
		 * the margin, or if it is preceded by a white character (we
		 * loaded 'M' into LastDisplayChar if it was a multibyte
		 * character) or hyphen, though it should have been excluded by
		 * HText_appendCharacter() or by split_line() in those cases. 
		 * -FM
		 */
		break;
	    } else {
		/*
		 * Make it a hard hyphen and fall through.  -FM
		 */
		buffer[0] = '-';
	    }
	    /* FALLTHRU */

	default:
#ifndef USE_COLOR_STYLE
#if defined(SHOW_WHEREIS_TARGETS)
	    if (!intarget && cp_tgt && i >= i_start_tgt) {
		/*
		 * Start the emphasis.
		 */
		if (data > cp_tgt) {
		    LYstartTargetEmphasis();
		    intarget = YES;
		}
	    }
#endif /* SHOW_WHEREIS_TARGETS */
#endif /* USE_COLOR_STYLE */
	    if (text->T.output_utf8 && is8bits(buffer[0])) {
		text->has_utf8 = YES;
		utf_extra = utf8_length(text->T.output_utf8, data - 1);
		LastDisplayChar = 'M';
	    }
	    if (utf_extra) {
		strncpy(&buffer[1], data, utf_extra);
		buffer[utf_extra + 1] = '\0';
		LYaddstr(buffer);
		buffer[1] = '\0';
		data += utf_extra;
		utf_extra = 0;
	    } else if (is_CJK2(buffer[0])) {
		/*
		 * For CJK strings, by Masanobu Kimura.
		 */
		if (i <= DISPLAY_COLS) {
		    buffer[1] = *data;
		    buffer[2] = '\0';
		    data++;
		    i++;
		    LYaddstr(buffer);
		    buffer[1] = '\0';
		    /*
		     * For now, load 'M' into LastDisplayChar, but we should
		     * check whether it's white and if so, use ' '.  I don't
		     * know if there actually are white CJK characters, and
		     * we're loading ' ' for multibyte spacing characters in
		     * this code set, but this will become an issue when the
		     * development code set's multibyte character handling is
		     * used.  -FM
		     */
		    LastDisplayChar = 'M';
#ifndef USE_SLANG
		    {
			int y, x;

			getyx(LYwin, y, x);
			if (x >= DISPLAY_COLS || x == 0)
			    break;
		    }
#endif
		}
	    } else {
		LYaddstr(buffer);
		LastDisplayChar = buffer[0];
	    }
	    i++;
	}			/* end of switch */
    }				/* end of while */

#if !(defined(NCURSES_VERSION) || defined(WIDEC_CURSES))
    if (text->has_utf8) {
	LYtouchline(scrline);
	text->has_utf8 = NO;	/* we had some, but have dealt with it. */
    }
#endif
    /*
     * Add the return.
     */
    LYaddch('\n');

#if defined(SHOW_WHEREIS_TARGETS) && !defined(USE_COLOR_STYLE)
    if (intarget)
	LYstopTargetEmphasis();
#else
#undef intarget
#endif /* SHOW_WHEREIS_TARGETS && !USE_COLOR_STYLE */
#ifndef USE_COLOR_STYLE
    lynx_stop_underline();
    lynx_stop_bold();
#else
    while (current_style < line->numstyles) {
	LynxChangeStyle(CStyle.style, CStyle.direction);
	current_style++;
    }
#undef CStyle
#endif
    return (0);
}

/*	Output the title line
 *	---------------------
 */
static void display_title(HText *text)
{
    char *title = NULL;
    char percent[20];
    unsigned char *tmp = NULL;
    int i = 0, j = 0, toolbar = 0;
    int limit;

    /*
     * Make sure we have a text structure.  -FM
     */
    if (!text)
	return;

    lynx_start_title_color();
#ifdef USE_COLOR_STYLE
/* turn the TITLE style on */
    if (last_colorattr_ptr > 0) {
	LynxChangeStyle(s_title, STACK_ON);
    } else {
	LynxChangeStyle(s_title, ABS_ON);
    }
#endif /* USE_COLOR_STYLE */

    /*
     * Load the title field.  -FM
     */
    StrAllocCopy(title,
		 (HTAnchor_title(text->node_anchor) ?
		  HTAnchor_title(text->node_anchor) : " "));	/* "" -> " " */
    LYReduceBlanks(title);

    /*
     * Generate the page indicator (percent) string.
     */
    limit = LYscreenWidth();
    if (limit < 10) {
	percent[0] = '\0';
    } else if ((display_lines) <= 0 && LYlines > 0 &&
	       text->top_of_screen <= 99999 && text->Lines <= 999999) {
	sprintf(percent, " (l%d of %d)",
		text->top_of_screen, text->Lines);
    } else if ((text->Lines >= display_lines) && (display_lines > 0)) {
	int total_pages = ((text->Lines + display_lines - 1)
			   / display_lines);
	int start_of_last_page = ((text->Lines <= display_lines)
				  ? 0
				  : (text->Lines - display_lines));

	sprintf(percent, " (p%d of %d)",
		((text->top_of_screen >= start_of_last_page)
		 ? total_pages
		 : ((text->top_of_screen + display_lines) / (display_lines))),
		total_pages);
    } else {
	percent[0] = '\0';
    }

    /*
     * Generate and display the title string, with page indicator
     * if appropriate, preceded by the toolbar token if appropriate,
     * and truncated if necessary.  -FM & KW
     */
    if (HTCJK != NOCJK) {
	if (*title &&
	    (tmp = typecallocn(unsigned char, (strlen(title) * 2 + 256)))) {
	    if (kanji_code == EUC) {
		TO_EUC((unsigned char *) title, tmp);
	    } else if (kanji_code == SJIS) {
		TO_SJIS((unsigned char *) title, tmp);
	    } else {
		for (i = 0, j = 0; title[i]; i++) {
		    if (title[i] != CH_ESC) {	/* S/390 -- gil -- 1487 */
			tmp[j++] = title[i];
		    }
		}
		tmp[j] = '\0';
	    }
	    StrAllocCopy(title, (const char *) tmp);
	    FREE(tmp);
	}
    }
    LYmove(0, 0);
    LYclrtoeol();
#if defined(SH_EX) && defined(KANJI_CODE_OVERRIDE)
    LYaddstr(str_kcode(last_kcode));
#endif
    if (HText_hasToolbar(text)) {
	LYaddch('#');
	toolbar = 1;
    }
#ifdef USE_COLOR_STYLE
    if (s_forw_backw != NOSTYLE && (nhist || nhist_extra > 1)) {
	int c = nhist ? ACS_LARROW : ' ';

	/* turn the FORWBACKW.ARROW style on */
	LynxChangeStyle(s_forw_backw, STACK_ON);
	if (nhist) {
	    LYaddch(c);
	    LYaddch(c);
	    LYaddch(c);
	} else
	    LYmove(0, 3 + toolbar);
	if (nhist_extra > 1) {
	    LYaddch(ACS_RARROW);
	    LYaddch(ACS_RARROW);
	    LYaddch(ACS_RARROW);
	}
	LynxChangeStyle(s_forw_backw, STACK_OFF);
    }
#endif /* USE_COLOR_STYLE */
#ifdef WIDEC_CURSES
    i = limit - LYbarWidth - strlen(percent) - LYstrCells(title);
    if (i <= 0) {		/* title is truncated */
	i = limit - LYbarWidth - strlen(percent) - 3;
	if (i <= 0) {		/* no room at all */
	    title[0] = '\0';
	} else {
	    strcpy(title + LYstrExtent2(title, i), "...");
	}
	i = 0;
    }
    LYmove(0, i);
#else
    i = (limit - 1) - strlen(percent) - strlen(title);
    if (i >= CHAR_WIDTH) {
	LYmove(0, i);
    } else {
	/*
	 * Truncation takes into account the possibility that
	 * multibyte characters might be present.  -HS (H.  Senshu)
	 */
	int last;

	last = (int) strlen(percent) + CHAR_WIDTH;
	if (limit - 3 >= last) {
	    title[(limit - 3) - last] = '.';
	    title[(limit - 2) - last] = '.';
	    title[(limit - 1) - last] = '\0';
	} else {
	    title[(limit - 1) - last] = '\0';
	}
	LYmove(0, CHAR_WIDTH);
    }
#endif
    LYaddstr(title);
    if (percent[0] != '\0')
	LYaddstr(percent);
    LYaddch('\n');
    FREE(title);

#if defined(USE_COLOR_STYLE) && defined(CAN_CUT_AND_PASTE)
    if (s_hot_paste != NOSTYLE) {	/* Only if the user set the style */
	LYmove(0, LYcolLimit);
	LynxChangeStyle(s_hot_paste, STACK_ON);
	LYaddch(ACS_RARROW);
	LynxChangeStyle(s_hot_paste, STACK_OFF);
	LYmove(1, 0);		/* As after \n */
    }
#endif /* USE_COLOR_STYLE */

#ifdef USE_COLOR_STYLE
/* turn the TITLE style off */
    LynxChangeStyle(s_title, STACK_OFF);
#endif /* USE_COLOR_STYLE */
    lynx_stop_title_color();

    return;
}

/*	Output the scrollbar
 *	---------------------
 */
#ifdef USE_SCROLLBAR
static void display_scrollbar(HText *text)
{
    int i;
    int h = display_lines - 2 * (LYsb_arrow != 0);	/* Height of the scrollbar */
    int off = (LYsb_arrow != 0);	/* Start of the scrollbar */
    int top_skip, bot_skip, sh, shown;

    LYsb_begin = LYsb_end = -1;
    if (!LYShowScrollbar || !text || h <= 2
	|| text->Lines <= display_lines)
	return;

    if (text->top_of_screen >= text->Lines - display_lines) {
	/* Only part of the screen shows actual text */
	shown = text->Lines - text->top_of_screen;

	if (shown <= 0)
	    shown = 1;
    } else
	shown = display_lines;
    /* Each cell of scrollbar represents text->Lines/h lines of text. */
    /* Always smaller than h */
    sh = (shown * h + text->Lines / 2) / text->Lines;
    if (sh <= 0)
	sh = 1;
    if (sh >= h - 1)
	sh = h - 2;		/* Position at ends indicates BEG and END */

    if (text->top_of_screen == 0)
	top_skip = 0;
    else if (text->Lines - (text->top_of_screen + display_lines - 1) <= 0)
	top_skip = h - sh;
    else {
	/* text->top_of_screen between 1 and text->Lines - display_lines
	   corresponds to top_skip between 1 and h - sh - 1 */
	/* Use rounding to get as many positions into top_skip==h - sh - 1
	   as into top_skip == 1:
	   1--->1, text->Lines - display_lines + 1--->h - sh. */
	top_skip = (int) (1 +
			  1. * (h - sh - 1) * text->top_of_screen
			  / (text->Lines - display_lines + 1));
    }
    bot_skip = h - sh - top_skip;

    LYsb_begin = top_skip;
    LYsb_end = h - bot_skip;

    if (LYsb_arrow) {
#ifdef USE_COLOR_STYLE
	int s = top_skip ? s_sb_aa : s_sb_naa;

	if (last_colorattr_ptr > 0) {
	    LynxChangeStyle(s, STACK_ON);
	} else {
	    LynxChangeStyle(s, ABS_ON);
	}
#endif /* USE_COLOR_STYLE */
	LYmove(1, LYcolLimit + LYshiftWin);
	addch_raw(ACS_UARROW);
#ifdef USE_COLOR_STYLE
	LynxChangeStyle(s, STACK_OFF);
#endif /* USE_COLOR_STYLE */
    }
#ifdef USE_COLOR_STYLE
    if (last_colorattr_ptr > 0) {
	LynxChangeStyle(s_sb_bg, STACK_ON);
    } else {
	LynxChangeStyle(s_sb_bg, ABS_ON);
    }
#endif /* USE_COLOR_STYLE */

    for (i = 1; i <= h; i++) {
#ifdef USE_COLOR_STYLE
	if (i - 1 <= top_skip && i > top_skip)
	    LynxChangeStyle(s_sb_bar, STACK_ON);
	if (i - 1 <= h - bot_skip && i > h - bot_skip)
	    LynxChangeStyle(s_sb_bar, STACK_OFF);
#endif /* USE_COLOR_STYLE */
	LYmove(i + off, LYcolLimit + LYshiftWin);
	if (i > top_skip && i <= h - bot_skip) {
	    LYaddch(ACS_BLOCK);
	} else {
	    LYaddch(ACS_CKBOARD);
	}
    }
#ifdef USE_COLOR_STYLE
    LynxChangeStyle(s_sb_bg, STACK_OFF);
#endif /* USE_COLOR_STYLE */

    if (LYsb_arrow) {
#ifdef USE_COLOR_STYLE
	int s = bot_skip ? s_sb_aa : s_sb_naa;

	if (last_colorattr_ptr > 0) {
	    LynxChangeStyle(s, STACK_ON);
	} else {
	    LynxChangeStyle(s, ABS_ON);
	}
#endif /* USE_COLOR_STYLE */
	LYmove(h + 2, LYcolLimit + LYshiftWin);
	addch_raw(ACS_DARROW);
#ifdef USE_COLOR_STYLE
	LynxChangeStyle(s, STACK_OFF);
#endif /* USE_COLOR_STYLE */
    }
    return;
}
#else
#define display_scrollbar(text)	/*nothing */
#endif /* USE_SCROLLBAR */

/*	Output a page
 *	-------------
 */
static void display_page(HText *text,
			 int line_number,
			 const char *target)
{
    HTLine *line = NULL;
    int i;
    int title_lines = TITLE_LINES;

#if defined(USE_COLOR_STYLE) && defined(SHOW_WHEREIS_TARGETS)
    const char *cp;
#endif
    char tmp[7];
    TextAnchor *Anchor_ptr = NULL;
    int stop_before_for_anchors;
    FormInfo *FormInfo_ptr;
    BOOL display_flag = FALSE;
    HTAnchor *link_dest;
    HTAnchor *link_dest_intl = NULL;
    static int last_nlinks = 0;
    static int charset_last_displayed = -1;

#ifdef DISP_PARTIAL
    int last_disp_partial = -1;
#endif

    lynx_mode = NORMAL_LYNX_MODE;

    if (text == NULL) {
	/*
	 * Check whether to force a screen clear to enable scrollback,
	 * or as a hack to fix a reverse clear screen problem for some
	 * curses packages.  - shf@access.digex.net & seldon@eskimo.com
	 */
	if (enable_scrollback) {
	    LYaddch('*');
	    LYrefresh();
	    LYclear();
	}
	LYaddstr("\n\nError accessing document!\nNo data available!\n");
	LYrefresh();
	nlinks = 0;		/* set number of links to 0 */
	return;
    }
#ifdef DISP_PARTIAL
    if (display_partial || recent_sizechange || text->stale) {
	/*  Reset them, will be set near end if all is okay. - kw */
	ResetPartialLinenos(text);
    }
#endif /* DISP_PARTIAL */

    tmp[0] = tmp[1] = tmp[2] = '\0';
    if (target && *target == '\0')
	target = NULL;
    text->page_has_target = NO;
    if (display_lines <= 0) {
	/* No screen space to display anything!
	 * returning here makes it more likely we will survive if
	 * an xterm is temporarily made very small.  - kw */
	return;
    }

    line = text->last_line->prev;
    line_number = HText_getPreferredTopLine(text, line_number);

    for (i = 0, line = FirstHTLine(text);	/* Find line */
	 i < line_number && (line != text->last_line);
	 i++, line = line->next) {	/* Loop */
#ifndef VMS
	if (!LYNoCore) {
	    assert(line->next != NULL);
	} else if (line->next == NULL) {
	    if (enable_scrollback) {
		LYaddch('*');
		LYrefresh();
		LYclear();
	    }
	    LYaddstr("\n\nError drawing page!\nBad HText structure!\n");
	    LYrefresh();
	    nlinks = 0;		/* set number of links to 0 */
	    return;
	}
#else
	assert(line->next != NULL);
#endif /* !VMS */
    }				/* Loop */

    if (LYlowest_eightbit[current_char_set] <= 255 &&
	(current_char_set != charset_last_displayed) &&
    /*
     * current_char_set has changed since last invocation,
     * and it's not just 7-bit.
     * Also we don't want to do this for -dump and -source etc.
     */
	LYCursesON) {
#ifdef EXP_CHARTRANS_AUTOSWITCH
	UCChangeTerminalCodepage(current_char_set,
				 &LYCharSet_UC[current_char_set]);
#endif /* EXP_CHARTRANS_AUTOSWITCH */
	charset_last_displayed = current_char_set;
    }

    /*
     * Check whether to force a screen clear to enable scrollback,
     * or as a hack to fix a reverse clear screen problem for some
     * curses packages.  - shf@access.digex.net & seldon@eskimo.com
     */
    if (enable_scrollback) {
	LYaddch('*');
	LYrefresh();
	LYclear();
    }
#ifdef USE_COLOR_STYLE
    /*
     * Reset stack of color attribute changes to avoid color leaking,
     * except if what we last displayed from this text was the previous
     * screenful, in which case carrying over the state might be beneficial
     * (although it shouldn't generally be needed any more).  - kw
     */
    if (text->stale ||
	line_number != text->top_of_screen + (display_lines)) {
	last_colorattr_ptr = 0;
    }
#endif

    text->top_of_screen = line_number;
    text->top_of_screen_line = line;
    if (no_title) {
	LYmove(0, 0);
	title_lines = 0;
    } else {
	display_title(text);	/* will move cursor to top of screen */
    }
    display_flag = TRUE;

#ifdef USE_COLOR_STYLE
#ifdef DISP_PARTIAL
    if (display_partial ||
	line_number != text->first_lineno_last_disp_partial ||
	line_number > text->last_lineno_last_disp_partial)
#endif /* DISP_PARTIAL */
	LynxResetScreenCache();
#endif /* USE_COLOR_STYLE */

#ifdef DISP_PARTIAL
    if (display_partial && text->stbl) {
	stop_before_for_anchors = Stbl_getStartLineDeep(text->stbl);
	if (stop_before_for_anchors > line_number + (display_lines))
	    stop_before_for_anchors = line_number + (display_lines);
    } else
#endif
	stop_before_for_anchors = line_number + (display_lines);

    /*
     * Output the page.
     */
    if (line) {
#if defined(USE_COLOR_STYLE) && defined(SHOW_WHEREIS_TARGETS)
	char *data;
	int offset, LenNeeded;
#endif
#ifdef DISP_PARTIAL
	if (display_partial ||
	    line_number != text->first_lineno_last_disp_partial)
	    text->has_utf8 = NO;
#else
	text->has_utf8 = NO;
#endif
	for (i = 0; i < (display_lines); i++) {
	    /*
	     * Verify and display each line.
	     */
#ifndef VMS
	    if (!LYNoCore) {
		assert(line != NULL);
	    } else if (line == NULL) {
		if (enable_scrollback) {
		    LYaddch('*');
		    LYrefresh();
		    LYclear();
		}
		LYaddstr("\n\nError drawing page!\nBad HText structure!\n");
		LYrefresh();
		nlinks = 0;	/* set number of links to 0 */
		return;
	    }
#else
	    assert(line != NULL);
#endif /* !VMS */

#ifdef DISP_PARTIAL
	    if (!display_partial &&
		line_number == text->first_lineno_last_disp_partial &&
		i + line_number <= text->last_lineno_last_disp_partial)
		LYmove((i + title_lines + 1), 0);
	    else
#endif
		display_line(line, text, i + 1, target);

#if defined(SHOW_WHEREIS_TARGETS)
#ifdef USE_COLOR_STYLE		/* otherwise done in display_line - kw */
	    /*
	     * If the target is on this line, recursively
	     * seek and emphasize it.  -FM
	     */
	    data = (char *) line->data;
	    offset = (int) line->offset;
	    while (non_empty(target) &&
		   (cp = LYno_attr_mb_strstr(data,
					     target,
					     text->T.output_utf8, YES,
					     NULL,
					     &LenNeeded)) != NULL &&
		   ((int) line->offset + LenNeeded) <= DISPLAY_COLS) {
		int itmp = 0;
		int written = 0;
		int x_pos = offset + (int) (cp - data);
		int len = strlen(target);
		size_t utf_extra = 0;
		int y;

		text->page_has_target = YES;

		/*
		 * Start the emphasis.
		 */
		LYstartTargetEmphasis();

		/*
		 * Output the target characters.
		 */
		for (;
		     written < len && (tmp[0] = data[itmp]) != '\0';
		     itmp++) {
		    if (IsSpecialAttrChar(tmp[0]) && tmp[0] != LY_SOFT_NEWLINE) {
			/*
			 * Ignore special characters.
			 */
			x_pos--;

		    } else if (&data[itmp] >= cp) {
			if (cp == &data[itmp]) {
			    /*
			     * First printable character of target.
			     */
			    LYmove((i + title_lines), x_pos);
			}
			/*
			 * Output all the printable target chars.
			 */
			utf_extra = utf8_length(text->T.output_utf8, data + itmp);
			if (utf_extra) {
			    strncpy(&tmp[1], &line->data[itmp + 1], utf_extra);
			    tmp[utf_extra + 1] = '\0';
			    itmp += utf_extra;
			    LYaddstr(tmp);
			    tmp[1] = '\0';
			    written += (utf_extra + 1);
			    utf_extra = 0;
			} else if (HTCJK != NOCJK && is8bits(tmp[0])) {
			    /*
			     * For CJK strings, by Masanobu Kimura.
			     */
			    tmp[1] = data[++itmp];
			    LYaddstr(tmp);
			    tmp[1] = '\0';
			    written += 2;
			} else {
			    LYaddstr(tmp);
			    written++;
			}
		    }
		}

		/*
		 * Stop the emphasis, and reset the offset and
		 * data pointer for our current position in the
		 * line.  -FM
		 */
		LYstopTargetEmphasis();
		LYGetYX(y, offset);
		data = (char *) &data[itmp];

		/*
		 * Adjust the cursor position, should we be at
		 * the end of the line, or not have another hit
		 * in it.  -FM
		 */
		LYmove((i + title_lines + 1), 0);
	    }			/* end while */
#endif /* USE_COLOR_STYLE */
#endif /* SHOW_WHEREIS_TARGETS */

	    /*
	     * Stop if this is the last line.  Otherwise, make sure
	     * display_flag is set and process the next line.  -FM
	     */
	    if (line == text->last_line) {
		/*
		 * Clear remaining lines of display.
		 */
		for (i++; i < (display_lines); i++) {
		    LYmove((i + title_lines), 0);
		    LYclrtoeol();
		}
		break;
	    }
#ifdef DISP_PARTIAL
	    if (display_partial) {
		/*
		 * Remember as fully shown during last partial display,
		 * if it was not the last text line.  - kw
		 */
		last_disp_partial = i + line_number;
	    }
#endif /* DISP_PARTIAL */
	    display_flag = TRUE;
	    line = line->next;
	}			/* end of "Verify and display each line." loop */
    }
    /* end "Output the page." */
    text->next_line = line;	/* Line after screen */
    text->stale = NO;		/* Display is up-to-date */

    /*
     * Add the anchors to Lynx structures.
     */
    nlinks = 0;
    for (Anchor_ptr = text->first_anchor;
	 Anchor_ptr != NULL && Anchor_ptr->line_num <= stop_before_for_anchors;
	 Anchor_ptr = Anchor_ptr->next) {

	if (Anchor_ptr->line_num >= line_number
	    && Anchor_ptr->line_num < stop_before_for_anchors) {
	    char *hi_string = LYGetHiTextStr(Anchor_ptr, 0);

	    /*
	     * Load normal hypertext anchors.
	     */
	    if (Anchor_ptr->show_anchor
		&& non_empty(hi_string)
		&& (Anchor_ptr->link_type & HYPERTEXT_ANCHOR)) {
		int count;
		char *s;

		for (count = 0;; ++count) {
		    s = LYGetHiTextStr(Anchor_ptr, count);
		    if (count == 0)
			LYSetHilite(nlinks, s);
		    if (s == NULL)
			break;
		    if (count != 0) {
			LYAddHilite(nlinks, s, LYGetHiTextPos(Anchor_ptr, count));
		    }
		}

		links[nlinks].inUnderline = Anchor_ptr->inUnderline;

		links[nlinks].sgml_offset = Anchor_ptr->sgml_offset;
		links[nlinks].anchor_number = Anchor_ptr->number;
		links[nlinks].anchor_line_num = Anchor_ptr->line_num;

		link_dest = HTAnchor_followLink(Anchor_ptr->anchor);
		{
		    /*
		     * Memory leak fixed 05-27-94
		     * Garrett Arch Blythe
		     */
		    auto char *cp_AnchorAddress = NULL;

		    if (traversal)
			cp_AnchorAddress = stub_HTAnchor_address(link_dest);
		    else {
#ifndef DONT_TRACK_INTERNAL_LINKS
			if (Anchor_ptr->link_type == INTERNAL_LINK_ANCHOR) {
			    link_dest_intl = HTAnchor_followTypedLink(
									 Anchor_ptr->anchor, HTInternalLink);
			    if (link_dest_intl && link_dest_intl != link_dest) {

				CTRACE((tfp,
					"GridText: display_page: unexpected typed link to %s!\n",
					link_dest_intl->parent->address));
				link_dest_intl = NULL;
			    }
			} else
			    link_dest_intl = NULL;
			if (link_dest_intl) {
			    char *cp2 = HTAnchor_address(link_dest_intl);

			    cp_AnchorAddress = cp2;
			} else
#endif
			    cp_AnchorAddress = HTAnchor_address(link_dest);
		    }
		    FREE(links[nlinks].lname);

		    if (cp_AnchorAddress != NULL)
			links[nlinks].lname = cp_AnchorAddress;
		    else
			StrAllocCopy(links[nlinks].lname, empty_string);
		}

		links[nlinks].lx = Anchor_ptr->line_pos;
		links[nlinks].ly = ((Anchor_ptr->line_num + 1) - line_number);
		if (link_dest_intl)
		    links[nlinks].type = WWW_INTERN_LINK_TYPE;
		else
		    links[nlinks].type = WWW_LINK_TYPE;
		links[nlinks].target = empty_string;
		links[nlinks].l_form = NULL;

		nlinks++;
		display_flag = TRUE;

	    } else if (Anchor_ptr->link_type == INPUT_ANCHOR
		       && Anchor_ptr->input_field->type != F_HIDDEN_TYPE) {
		/*
		 * Handle form fields.
		 */
		lynx_mode = FORMS_LYNX_MODE;

		FormInfo_ptr = Anchor_ptr->input_field;

		links[nlinks].sgml_offset = Anchor_ptr->sgml_offset;
		links[nlinks].anchor_number = Anchor_ptr->number;
		links[nlinks].anchor_line_num = Anchor_ptr->line_num;

		links[nlinks].l_form = FormInfo_ptr;
		links[nlinks].lx = Anchor_ptr->line_pos;
		links[nlinks].ly = ((Anchor_ptr->line_num + 1) - line_number);
		links[nlinks].type = WWW_FORM_LINK_TYPE;
		links[nlinks].inUnderline = Anchor_ptr->inUnderline;
		links[nlinks].target = empty_string;
		StrAllocCopy(links[nlinks].lname, empty_string);

		if (FormInfo_ptr->type == F_RADIO_TYPE) {
		    LYSetHilite(nlinks,
				FormInfo_ptr->num_value
				? checked_radio
				: unchecked_radio);
		} else if (FormInfo_ptr->type == F_CHECKBOX_TYPE) {
		    LYSetHilite(nlinks,
				FormInfo_ptr->num_value
				? checked_box
				: unchecked_box);
		} else if (FormInfo_ptr->type == F_PASSWORD_TYPE) {
		    /* FIXME: use LYstrExtent, not strlen */
		    LYSetHilite(nlinks,
				STARS(strlen(FormInfo_ptr->value)));
		} else {	/* TEXT type */
		    LYSetHilite(nlinks,
				FormInfo_ptr->value);
		}

		nlinks++;
		/*
		 * Bold the link after incrementing nlinks.
		 */
		LYhighlight(OFF, (nlinks - 1), target);

		display_flag = TRUE;

	    } else {
		/*
		 * Not showing anchor.
		 */
		if (non_empty(hi_string))
		    CTRACE((tfp,
			    "\nGridText: Not showing link, hightext=%s\n",
			    hi_string));
	    }
	}

	if (nlinks == MAXLINKS) {
	    /*
	     * Links array is full.  If interactive, tell user
	     * to use half-page or two-line scrolling.  -FM
	     */
	    if (LYCursesON) {
		HTAlert(MAXLINKS_REACHED);
	    }
	    CTRACE((tfp, "\ndisplay_page: MAXLINKS reached.\n"));
	    break;
	}
    }				/* end of loop "Add the anchors to Lynx structures." */

    /*
     * Free any un-reallocated links[] entries
     * from the previous page draw.  -FM
     */
    LYFreeHilites(nlinks, last_nlinks);
    last_nlinks = nlinks;

    /*
     * If Anchor_ptr is not NULL and is not pointing to the last
     * anchor, then there are anchors farther down in the document,
     * and we need to flag this for traversals.
     */
    more_links = FALSE;
    if (traversal && Anchor_ptr) {
	if (Anchor_ptr->next)
	    more_links = TRUE;
    }

    if (!display_flag) {
	/*
	 * Nothing on the page.
	 */
	LYaddstr("\n     Document is empty");
    }
    display_scrollbar(text);

#ifdef DISP_PARTIAL
    if (display_partial && display_flag &&
	last_disp_partial >= text->top_of_screen &&
	!enable_scrollback &&
	!recent_sizechange) {	/*  really remember them if ok - kw  */
	text->first_lineno_last_disp_partial = text->top_of_screen;
	text->last_lineno_last_disp_partial = last_disp_partial;
    } else {
	ResetPartialLinenos(text);
    }
#endif /* DISP_PARTIAL */

#if !defined(WIDEC_CURSES)
    if (text->has_utf8 || text->had_utf8) {
	/*
	 * For other than ncurses, repainting is taken care of
	 * by touching lines in display_line and highlight.  - kw 1999-10-07
	 */
	text->had_utf8 = text->has_utf8;
	clearok(curscr, TRUE);
    } else if (HTCJK != NOCJK) {
	/*
	 * For non-multibyte curses.
	 *
	 * Full repainting is necessary, otherwise only part of a multibyte
	 * character sequence might be written because of curses output
	 * optimizations.
	 */
	clearok(curscr, TRUE);
    }
#endif /* WIDEC_CURSES */

    LYrefresh();
    return;
}

/*			Object Building methods
 *			-----------------------
 *
 *	These are used by a parser to build the text in an object
 */
void HText_beginAppend(HText *text)
{
    text->permissible_split = 0;
    text->in_line_1 = YES;

}

/* LYcols_cu is the notion that the display library has of the screen
   width.  Normally it is the same as LYcols, but there may be a
   difference via SLANG_MBCS_HACK.  Checks of the line length (as the
   non-UTF-8-aware display library would see it) against LYcols_cu are
   is used to try to prevent that lines with UTF-8 chars get wrapped
   by the library when they shouldn't.
   If there is no display library involved, i.e. dump_output_immediately,
   no such limit should be imposed.  MAX_COLS should be just as good
   as any other large value.  (But don't use INT_MAX or something close
   to it to, avoid over/underflow.) - kw */
#ifdef USE_SLANG
#define LYcols_cu(text) (dump_output_immediately ? MAX_COLS : SLtt_Screen_Cols)
#else
#ifdef WIDEC_CURSES
#define LYcols_cu(text) WRAP_COLS(text)
#else
#define LYcols_cu(text) (dump_output_immediately ? MAX_COLS : DISPLAY_COLS)
#endif
#endif

/*	Add a new line of text
 *	----------------------
 *
 * On entry,
 *
 *	split	is zero for newline function, else number of characters
 *		before split.
 *	text->display_on_the_fly
 *		may be set to indicate direct output of the finished line.
 * On exit,
 *		A new line has been made, justified according to the
 *		current style.  Text after the split (if split nonzero)
 *		is taken over onto the next line.
 *
 *		If display_on_the_fly is set, then it is decremented and
 *		the finished line is displayed.
 */
#define new_line(text) split_line(text, 0)

#define DEBUG_SPLITLINE

#ifdef DEBUG_SPLITLINE
#define CTRACE_SPLITLINE(p)	CTRACE(p)
#else
#define CTRACE_SPLITLINE(p)	/*nothing */
#endif

static int set_style_by_embedded_chars(char *s,
				       char *e,
				       unsigned char start_c,
				       unsigned char end_c)
{
    int ret = NO;

    while (--e >= s) {
	if (*e == end_c)
	    break;
	if (*e == start_c) {
	    ret = YES;
	    break;
	}
    }
    return ret;
}

static void move_anchors_in_region(HTLine *line, int line_number,
				   TextAnchor **prev_anchor,	/*updates++ */
				   int *prev_head_processed,
				   int sbyte,
				   int ebyte,
				   int shift)	/* Likewise */
{
    /*
     * Update anchor positions for anchors that start on this line.  Note:  we
     * rely on a->line_pos counting bytes, not characters.  That's one reason
     * why HText_trimHightext has to be prevented from acting on these anchors
     * in partial display mode before we get a chance to deal with them here.
     */
    TextAnchor *a;
    int head_processed = *prev_head_processed;

    /*
     * We need to know whether (*prev_anchor)->line_pos is "in new coordinates"
     * or in old ones.  If prev_anchor' head was touched on the previous
     * iteration, we set head_processed.  The tail may need to be treated now.
     */
    for (a = *prev_anchor;
	 a && a->line_num <= line_number;
	 a = a->next, head_processed = 0) {
	/* extent==0 needs to be special-cased; happens if no text for
	   the anchor was processed yet.  */
	/* Subtract one so that the space is not inserted at the end
	   of the anchor... */
	int last = a->line_pos + (a->extent ? a->extent - 1 : 0);

	/* Include the anchors started on the previous line */
	if (a->line_num < line_number - 1)
	    continue;
	if (a->line_num == line_number - 1)
	    last -= line->prev->size + 1;	/* Fake "\n" "between" lines counted too */
	if (last < sbyte)	/* Completely before the start */
	    continue;

	if (!head_processed	/* a->line_pos is not edited yet */
	    && a->line_num == line_number
	    && a->line_pos >= ebyte)	/* Completely after the end */
	    break;
	/* Now we know that the anchor context intersects the chunk */

	/* Fix the start */
	if (!head_processed && a->line_num == line_number
	    && a->line_pos >= sbyte) {
	    a->line_pos += shift;
	    a->extent -= shift;
	    head_processed = 1;
	}
	/* Fix the end */
	if (last < ebyte) {
	    a->extent += shift;
	} else {
	    break;		/* Keep this `a' for the next step */
	}
    }
    *prev_anchor = a;
    *prev_head_processed = head_processed;
}

/*
 * Given a line and two int arrays of old/now position, this function
 * creates a new line where spaces have been inserted/removed
 * in appropriate places - so that characters at/after the old
 * position end up at/after the new position, for each pair, if possible.
 * Some necessary changes for anchors starting on this line are also done
 * here if needed.  Updates 'prev_anchor' internally.
 * Returns a newly allocated HTLine* if changes were made
 * (caller has to free the old one).
 * Returns NULL if no changes needed.  (Remove-spaces code may be buggy...)
 * - kw
 */
static HTLine *insert_blanks_in_line(HTLine *line, int line_number,
				     HText *text,
				     TextAnchor **prev_anchor,	/*updates++ */
				     int ninserts,
				     int *oldpos,	/* Measured in cells */
				     int *newpos)	/* Likewise */
{
    int ioldc = 0;		/* count visible characters */
    int ip;			/* count insertion pairs */

#if defined(USE_COLOR_STYLE)
    int istyle = 0;
#endif
    int added_chars = 0;
    int shift = 0;
    int head_processed;
    HTLine *mod_line;
    char *newdata;
    char *s = line->data;
    char *pre = s;
    char *copied = line->data, *t;

    if (!(line && line->size && ninserts))
	return NULL;
    for (ip = 0; ip < ninserts; ip++)
	if (newpos[ip] > oldpos[ip] &&
	    (newpos[ip] - oldpos[ip]) > added_chars)
	    added_chars = newpos[ip] - oldpos[ip];
    if (line->size + added_chars > MAX_LINE - 2)
	return NULL;
    if (line == text->last_line) {
	if (line == TEMP_LINE(text, 0))
	    mod_line = TEMP_LINE(text, 1);
	else
	    mod_line = TEMP_LINE(text, 0);
    } else {
	allocHTLine(mod_line, line->size + added_chars);
    }
    if (!mod_line)
	return NULL;
    if (!*prev_anchor)
	*prev_anchor = text->first_anchor;
    head_processed = (*prev_anchor && (*prev_anchor)->line_num < line_number);
    memcpy(mod_line, line, LINE_SIZE(0));
    t = newdata = mod_line->data;
    ip = 0;
    while (ip <= ninserts) {
	/* line->size is in bytes, so it may be larger than needed... */
	int curlim = (ip < ninserts
		      ? oldpos[ip]
	/* Include'em all! */
		      : ((int) line->size <= MAX_LINE
			 ? MAX_LINE + 1
			 : (int) line->size + 1));

	pre = s;

	/* Fast forward to char==curlim or EOL.  Stop *before* the
	   style-change chars. */
	while (*s) {
	    if (text && text->T.output_utf8
		&& UCH(*s) >= 0x80 && UCH(*s) < 0xC0) {
		pre = s + 1;
	    } else if (!IsSpecialAttrChar(*s)) {	/* At a "displayed" char */
		if (ioldc >= curlim)
		    break;
		ioldc++;
		pre = s + 1;
	    }
	    s++;
	}

	/* Now s is at the "displayed" char, pre is before the style change */
	if (ip)			/* Fix anchor positions */
	    move_anchors_in_region(line, line_number, prev_anchor /*updates++ */ ,
				   &head_processed,
				   copied - line->data, pre - line->data,
				   shift);
#if defined(USE_COLOR_STYLE)	/* Move styles too */
#define NStyle mod_line->styles[istyle]
	for (;
	     istyle < line->numstyles && (int) NStyle.horizpos < curlim;
	     istyle++)
	    /* Should not we include OFF-styles at curlim? */
	    NStyle.horizpos += shift;
#endif
	while (copied < pre)	/* Copy verbatim to byte == pre */
	    *t++ = *copied++;
	if (ip < ninserts) {	/* Insert spaces */
	    int delta = newpos[ip] - oldpos[ip] - shift;

	    if (delta < 0) {	/* Not used yet? */
		while (delta++ < 0 && t > newdata && t[-1] == ' ')
		    t--, shift--;
	    } else
		shift = newpos[ip] - oldpos[ip];
	    while (delta-- > 0)
		*t++ = ' ';
	}
	ip++;
    }
    while (pre < s)		/* Copy remaining style-codes */
	*t++ = *pre++;
    /* Check whether the last anchor continues on the next line */
    if (head_processed && *prev_anchor && (*prev_anchor)->line_num == line_number)
	(*prev_anchor)->extent += shift;
    *t = '\0';
    mod_line->size = t - newdata;
    return mod_line;
}

#if defined(USE_COLOR_STYLE)
static HTStyleChange *skip_matched_and_correct_offsets(HTStyleChange *end,
						       HTStyleChange *start,
						       unsigned split_pos)
{				/* Found an OFF change not part of an adjacent matched pair.
				 * Walk backward looking for the corresponding ON change.
				 * Move everything after split_pos to be at split_pos.
				 * This can only work correctly if all changes are correctly
				 * nested!  If this fails, assume it is safer to leave whatever
				 * comes before the OFF on the previous line alone. */
    int level = 0;
    HTStyleChange *tmp = end;

    for (; tmp >= start; tmp--) {
	if (tmp->style == end->style) {
	    if (tmp->direction == STACK_OFF)
		level--;
	    else if (tmp->direction == STACK_ON) {
		if (++level == 0)
		    return tmp;
	    } else
		return 0;
	}
	if (tmp->horizpos > split_pos) {
	    tmp->horizpos = split_pos;
	}
    }
    return 0;
}
#endif /* USE_COLOR_STYLE */

static void split_line(HText *text, unsigned split)
{
    HTStyle *style = text->style;
    int spare;
    int indent = (text->in_line_1
		  ? text->style->indent1st
		  : text->style->leftIndent);
    int new_offset;
    short alignment;
    TextAnchor *a;
    int CurLine = text->Lines;
    int HeadTrim = 0;
    int SpecialAttrChars = 0;
    int TailTrim = 0;
    int s, s_post, s_pre, t_underline = underline_on, t_bold = bold_on;
    char *p;
    char *cp;
    int ctrl_chars_on_previous_line = 0;

#ifndef WIDEC_CURSES
    int utfxtra_on_previous_line = UTFXTRA_ON_THIS_LINE;
#endif

    HTLine *previous = text->last_line;
    HTLine *line;

    /*
     * Set new line.
     */
    if (previous == TEMP_LINE(text, 0))
	line = TEMP_LINE(text, 1);
    else
	line = TEMP_LINE(text, 0);
    if (line == NULL)
	return;
    memset(line, 0, LINE_SIZE(0));

    ctrl_chars_on_this_line = 0;	/*reset since we are going to a new line */
    utfxtra_on_this_line = 0;	/*reset too, we'll count them */
    text->LastChar = ' ';

#ifdef DEBUG_APPCH
    CTRACE((tfp, "GridText: split_line(%p,%d) called\n", text, split));
    CTRACE((tfp, "   bold_on=%d, underline_on=%d\n", bold_on, underline_on));
#endif

    cp = previous->data;

    /* Float LY_SOFT_NEWLINE to the start */
    if (cp[0] == LY_BOLD_START_CHAR
	|| cp[0] == LY_UNDERLINE_START_CHAR) {
	switch (cp[1]) {
	case LY_SOFT_NEWLINE:
	    cp[1] = cp[0];
	    cp[0] = LY_SOFT_NEWLINE;
	    break;
	case LY_BOLD_START_CHAR:
	case LY_UNDERLINE_START_CHAR:
	    if (cp[2] == LY_SOFT_NEWLINE) {
		cp[2] = cp[1];
		cp[1] = cp[0];
		cp[0] = LY_SOFT_NEWLINE;
	    }
	    break;
	}
    }
    if (split > previous->size) {
	CTRACE((tfp,
		"*** split_line: split==%u greater than last_line->size==%d !\n",
		split, previous->size));
	if (split > MAX_LINE) {
	    split = previous->size;
	    if ((cp = strrchr(previous->data, ' ')) &&
		cp - previous->data > 1)
		split = cp - previous->data;
	    CTRACE((tfp, "                split adjusted to %u.\n", split));
	}
    }

    text->Lines++;

    previous->next->prev = line;
    line->prev = previous;
    line->next = previous->next;
    previous->next = line;
    text->last_line = line;
    line->size = 0;
    line->offset = 0;
    text->permissible_split = 0;	/* 12/13/93 */
    line->data[0] = '\0';

    alignment = style->alignment;

    if (split > 0) {		/* Restore flags to the value at the splitting point */
	if (!(dump_output_immediately && use_underscore))
	    t_underline = set_style_by_embedded_chars(previous->data,
						      previous->data + split,
						      LY_UNDERLINE_START_CHAR, LY_UNDERLINE_END_CHAR);

	t_bold = set_style_by_embedded_chars(previous->data,
					     previous->data + split,
					     LY_BOLD_START_CHAR, LY_BOLD_END_CHAR);

    }

    if (!(dump_output_immediately && use_underscore) && t_underline) {
	line->data[line->size++] = LY_UNDERLINE_START_CHAR;
	line->data[line->size] = '\0';
	ctrl_chars_on_this_line++;
	SpecialAttrChars++;
    }
    if (t_bold) {
	line->data[line->size++] = LY_BOLD_START_CHAR;
	line->data[line->size] = '\0';
	ctrl_chars_on_this_line++;
	SpecialAttrChars++;
    }

    /*
     * Split at required point
     */
    if (split > 0) {		/* Delete space at "split" splitting line */
	char *prevdata = previous->data, *linedata = line->data;
	unsigned plen;
	int i;

	/* Split the line. -FM */
	prevdata[previous->size] = '\0';
	previous->size = split;

	/*
	 * Trim any spaces or soft hyphens from the beginning
	 * of our new line.  -FM
	 */
	p = prevdata + split;
	while (((*p == ' '
#ifdef EXP_JUSTIFY_ELTS
	/* if justification is allowed for prev line, then raw
	 * HT_NON_BREAK_SPACE are still present in data[] (they'll be
	 * substituted at the end of this function with ' ') - VH
	 */
		 || *p == HT_NON_BREAK_SPACE
#endif
		)
		&& (HeadTrim || text->first_anchor ||
		    underline_on || bold_on ||
		    alignment != HT_LEFT ||
		    style->wordWrap || style->freeFormat ||
		    style->spaceBefore || style->spaceAfter)) ||
	       *p == LY_SOFT_HYPHEN) {
	    p++;
	    HeadTrim++;
	}

	plen = strlen(p);
	if (plen) {		/* Count funny characters */
	    for (i = (plen - 1); i >= 0; i--) {
		if (p[i] == LY_UNDERLINE_START_CHAR ||
		    p[i] == LY_UNDERLINE_END_CHAR ||
		    p[i] == LY_BOLD_START_CHAR ||
		    p[i] == LY_BOLD_END_CHAR ||
		    p[i] == LY_SOFT_HYPHEN) {
		    ctrl_chars_on_this_line++;
		} else if (IS_UTF_EXTRA(p[i])) {
		    utfxtra_on_this_line++;
		}
		if (p[i] == LY_SOFT_HYPHEN && (int) text->permissible_split < i)
		    text->permissible_split = i + 1;
	    }
	    ctrl_chars_on_this_line += utfxtra_on_this_line;

	    /* Add the data to the new line. -FM */
	    strcat(linedata, p);
	    line->size += plen;
	}
    }

    /*
     * Economize on space.
     */
    p = previous->data + previous->size - 1;
    while (p >= previous->data
	   && (*p == ' '
#ifdef EXP_JUSTIFY_ELTS
    /* if justification is allowed for prev line, then raw
     * HT_NON_BREAK_SPACE are still present in data[] (they'll be
     * substituted at the end of this function with ' ') - VH
     */
	       || *p == HT_NON_BREAK_SPACE
#endif
	   )
#ifdef USE_PRETTYSRC
	   && !psrc_view	/*don't strip trailing whites - since next line can
				   start with LY_SOFT_NEWLINE - so we don't lose spaces when
				   'p'rinting this text to file -VH */
#endif
	   && (ctrl_chars_on_this_line || HeadTrim || text->first_anchor ||
	       underline_on || bold_on ||
	       alignment != HT_LEFT ||
	       style->wordWrap || style->freeFormat ||
	       style->spaceBefore || style->spaceAfter)) {
	p--;			/*  Strip trailers. */
    }
    TailTrim = previous->data + previous->size - 1 - p;		/*  Strip trailers. */
    previous->size -= TailTrim;
    p[1] = '\0';

    /*
     * s is the effective split position, given by either a non-zero
     * value of split or by the size of the previous line before
     * trimming.  - kw
     */
    if (split == 0) {
	s = previous->size + TailTrim;	/* the original size */
    } else {
	s = split;
    }
    s_post = s + HeadTrim;
    s_pre = s - TailTrim;

#ifdef DEBUG_SPLITLINE
#ifdef DEBUG_APPCH
    if (s != (int) split)
#endif
	CTRACE((tfp, "GridText: split_line(%u [now:%d]) called\n", split, s));
#endif

#if defined(USE_COLOR_STYLE)
    if (previous->styles == stylechanges_buffers[0])
	line->styles = stylechanges_buffers[1];
    else
	line->styles = stylechanges_buffers[0];
    line->numstyles = 0;
    {
	HTStyleChange *from = previous->styles + previous->numstyles - 1;
	HTStyleChange *to = line->styles + MAX_STYLES_ON_LINE - 1;
	HTStyleChange *scan, *at_end;

	/* Color style changes after the split position
	 * are transferred to the new line.  Ditto for changes
	 * in the trimming region, but we stop when we reach an OFF change.
	 * The second loop below may then handle remaining changes.  - kw */
	while (from >= previous->styles && to >= line->styles) {
	    *to = *from;
	    if ((int) to->horizpos > s_post) {
		to->horizpos += -s_post + SpecialAttrChars;
	    } else if ((int) to->horizpos > s_pre &&
		       (to->direction == STACK_ON ||
			to->direction == ABS_ON)) {
		to->horizpos = ((int) to->horizpos < s) ? 0 : SpecialAttrChars;
	    } else {
		break;
	    }
	    to--;
	    from--;
	}
	/* FROM may be invalid, otherwise it is either an ON change at or
	   before s_pre, or is an OFF change at or before s_post.  */

	scan = from;
	at_end = from;
	/* Now on the previous line we have a correctly nested but
	   possibly non-terminated sequence of style changes.
	   Terminate it, and duplicate unterminated changes at the
	   beginning of the new line. */
	while (scan >= previous->styles && at_end >= previous->styles) {
	    /* The algorithm: scan back though the styles on the previous line.
	       a) If OFF, skip the matched group.
	       Report a bug on failure.
	       b) If ON, (try to) cancel the corresponding ON at at_end,
	       and the corresponding OFF at to;
	       If not, put the corresponding OFF at at_end, and copy to to;
	     */
	    if (scan->direction == STACK_OFF) {
		scan = skip_matched_and_correct_offsets(scan, previous->styles,
							s_pre);
		if (!scan) {
		    CTRACE((tfp, "BUG: styles improperly nested.\n"));
		    break;
		}
	    } else if (scan->direction == STACK_ON) {
		if (at_end->direction == STACK_ON
		    && at_end->style == scan->style
		    && (int) at_end->horizpos >= s_pre)
		    at_end--;
		else if (at_end >= previous->styles + MAX_STYLES_ON_LINE - 1) {
		    CTRACE((tfp, "BUG: style overflow before split_line.\n"));
		    break;
		} else {
		    at_end++;
		    at_end->direction = STACK_OFF;
		    at_end->style = scan->style;
		    at_end->horizpos = s_pre;
		}
		if (to < line->styles + MAX_STYLES_ON_LINE - 1
		    && to[1].direction == STACK_OFF
		    && to[1].horizpos <= (unsigned) SpecialAttrChars
		    && to[1].style == scan->style)
		    to++;
		else if (to >= line->styles) {
		    *to = *scan;
		    to->horizpos = SpecialAttrChars;
		    to--;
		} else {
		    CTRACE((tfp, "BUG: style overflow after split_line.\n"));
		    break;
		}
	    }
	    if ((int) scan->horizpos > s_pre) {
		scan->horizpos = s_pre;
	    }
	    scan--;
	}
	line->numstyles = line->styles + MAX_STYLES_ON_LINE - 1 - to;
	if (line->numstyles > 0 && line->numstyles < MAX_STYLES_ON_LINE) {
	    int n;

	    for (n = 0; n < line->numstyles; n++)
		line->styles[n] = to[n + 1];
	} else if (line->numstyles == 0) {
	    line->styles[0].horizpos = ~0;	/* ?!!! */
	}
	previous->numstyles = at_end - previous->styles + 1;
	if (previous->numstyles == 0) {
	    previous->styles[0].horizpos = ~0;	/* ?!!! */
	}
    }
#endif /*USE_COLOR_STYLE */

    {
	HTLine *temp;

	allocHTLine(temp, previous->size);
	if (!temp)
	    outofmem(__FILE__, "split_line_2");
	memcpy(temp, previous, LINE_SIZE(previous->size));
#if defined(USE_COLOR_STYLE)
	POOLallocstyles(temp->styles, previous->numstyles);
	if (!temp->styles)
	    outofmem(__FILE__, "split_line_2");
	memcpy(temp->styles, previous->styles, sizeof(HTStyleChange) * previous->numstyles);
#endif
	previous = temp;
    }

    previous->prev->next = previous;	/* Link in new line */
    previous->next->prev = previous;	/* Could be same node of course */

    /*
     * Terminate finished line for printing.
     */
    previous->data[previous->size] = '\0';

    /*
     * Align left, right or center.
     */
    spare = 0;
    if (
#ifdef EXP_JUSTIFY_ELTS
	   this_line_was_split ||
#endif
	   (alignment == HT_CENTER ||
	    alignment == HT_RIGHT) || text->stbl) {
	/* Calculate spare character positions if needed */
	for (cp = previous->data; *cp; cp++) {
	    if (*cp == LY_UNDERLINE_START_CHAR ||
		*cp == LY_UNDERLINE_END_CHAR ||
		*cp == LY_BOLD_START_CHAR ||
		*cp == LY_BOLD_END_CHAR ||
#ifndef WIDEC_CURSES
		IS_UTF_EXTRA(*cp) ||
#endif
		*cp == LY_SOFT_HYPHEN) {
		ctrl_chars_on_previous_line++;
	    }
	}
	if ((previous->size > 0) &&
	    (int) (previous->data[previous->size - 1] == LY_SOFT_HYPHEN))
	    ctrl_chars_on_previous_line--;

	/* @@ first line indent */
#ifdef WIDEC_CURSES
	spare = WRAP_COLS(text)
	    - (int) style->rightIndent
	    - indent
	    - LYstrExtent2(previous->data, previous->size);
	if (spare < 0 && LYwideLines)	/* Can be wider than screen */
	    spare = 0;
#else
	spare = WRAP_COLS(text) -
	    (int) style->rightIndent - indent +
	    ctrl_chars_on_previous_line - previous->size;
	if (spare < 0 && LYwideLines)	/* Can be wider than screen */
	    spare = 0;

	if (spare > 0 && !dump_output_immediately &&
	    text->T.output_utf8 && ctrl_chars_on_previous_line) {
	    utfxtra_on_previous_line -= UTFXTRA_ON_THIS_LINE;
	    if (utfxtra_on_previous_line) {
		int spare_cu = (LYcols_cu(text) -
				utfxtra_on_previous_line - indent +
				ctrl_chars_on_previous_line - previous->size);

		/*
		 * Shift non-leftaligned UTF-8 lines that would be
		 * mishandled by the display library towards the left
		 * if this would make them fit.  The resulting display
		 * will not be as intended, but this is better than
		 * having them split by curses.  (Curses cursor movement
		 * optimization may still cause wrong positioning within
		 * the line, in particular after a sequence of spaces).
		 * - kw
		 */
		if (spare_cu < spare) {
		    if (spare_cu >= 0) {
			if (alignment == HT_CENTER &&
			    (int) (previous->offset + indent + spare / 2 +
				   previous->size)
			    - ctrl_chars_on_previous_line
			    + utfxtra_on_previous_line <= LYcols_cu(text))
			    /* do nothing - it still fits - kw */ ;
			else {
			    spare = spare_cu;
			    if (alignment == HT_CENTER) {
				/*
				 * Can't move toward center all the way,
				 * but at least make line contents appear
				 * as far right as possible.  - kw
				 */
				alignment = HT_RIGHT;
			    }
			}
		    } else if (indent + (int) previous->offset + spare_cu >= 0) {	/* subtract overdraft from effective indentation */
			indent += (int) previous->offset + spare_cu;
			previous->offset = 0;
			spare = 0;
		    }
		}
	    }
	}
#endif
    }

    new_offset = previous->offset;
    switch (style->alignment) {
    case HT_CENTER:
	new_offset += indent + spare / 2;
	break;
    case HT_RIGHT:
	new_offset += indent + spare;
	break;
    case HT_LEFT:
    case HT_JUSTIFY:		/* Not implemented */
    default:
	new_offset += indent;
	break;
    }				/* switch */
    previous->offset = ((new_offset < 0) ? 0 : new_offset);

    if (text->stbl)
	/*
	 * Notify simple table stuff of line split, so that it can
	 * set the last cell's length.  The last cell should and
	 * its row should really end here, or on one of the following
	 * lines with no more characters added after the break.
	 * We don't know whether a cell has been started, so ignore
	 * errors here.
	 * This call is down here because we need the
	 * ctrl_chars_on_previous_line, which have just been re-
	 * counted above.  - kw
	 */
	Stbl_lineBreak(text->stbl,
		       text->Lines - 1,
		       previous->offset,
		       previous->size - ctrl_chars_on_previous_line);

    text->in_line_1 = NO;	/* unless caller sets it otherwise */

    /*
     * If we split the line, adjust the anchor
     * structure values for the new line.  -FM
     */

    if (s > 0) {		/* if not completely empty */
	int moved = 0;

	/* In the algorithm below we move or not move anchors between
	   lines using some heuristic criteria.  However, it is
	   desirable not to have two consequent anchors on different
	   lines *in a wrong order*!  (How can this happen?)
	   So when the "reasonable choice" is not unique, we use the
	   MOVED flag to choose one.
	 */
	/* Our operations can make a non-empty all-whitespace link
	   empty.  So what? */
	if ((a = text->last_anchor_before_split) == 0)
	    a = text->first_anchor;

	for (; a; a = a->next) {
	    if (a->line_num == CurLine) {
		int len = a->extent, n = a->number, start = a->line_pos;
		int end = start + len;

		text->last_anchor_before_split = a;

		/* Which anchors do we leave on the previous line?
		   a) empty finished (We need a cut-off value.
		   "Just because": those before s;
		   this is the only case when we use s, not s_pre/s_post);
		   b) Those which start before s_pre;
		 */
		if (start < s_pre) {
		    if (end <= s_pre)
			continue;	/* No problem */

		    CTRACE_SPLITLINE((tfp, "anchor %d: no relocation", n));
		    if (end > s_post) {
			CTRACE_SPLITLINE((tfp, " of the start.\n"));
			a->extent += -(TailTrim + HeadTrim) + SpecialAttrChars;
		    } else {
			CTRACE_SPLITLINE((tfp, ", cut the end.\n"));
			a->extent = s_pre - start;
		    }
		    continue;
		} else if (start < s && !len
			   && (!n || (a->show_anchor && !moved))) {
		    CTRACE_SPLITLINE((tfp,
				      "anchor %d: no relocation, empty-finished",
				      n));
		    a->line_pos = s_pre;	/* Leave at the end of line */
		    continue;
		}

		/* The rest we relocate */
		moved = 1;
		a->line_num++;
		CTRACE_SPLITLINE((tfp,
				  "anchor %d: (T,H,S)=(%d,%d,%d); (line,pos,ext):(%d,%d,%d), ",
				  n, TailTrim, HeadTrim, SpecialAttrChars,
				  a->line_num, a->line_pos, a->extent));
		if (end < s_post) {	/* Move the end to s_post */
		    CTRACE_SPLITLINE((tfp, "Move end +%d, ", s_post - end));
		    len += s_post - end;
		}
		if (start < s_post) {	/* Move the start to s_post */
		    CTRACE_SPLITLINE((tfp, "Move start +%d, ", s_post - start));
		    len -= s_post - start;
		    start = s_post;
		}
		a->line_pos = start - s_post + SpecialAttrChars;
		a->extent = len;

		CTRACE_SPLITLINE((tfp, "->(%d,%d,%d)\n",
				  a->line_num, a->line_pos, a->extent));
	    } else if (a->line_num > CurLine)
		break;
	}
    }
#ifdef EXP_JUSTIFY_ELTS
    /* now perform justification - by VH */

    if (this_line_was_split
	&& spare > 0
	&& !text->stbl		/* We don't inform TRST on the cell width change yet */
	&& justify_max_void_percent > 0
	&& justify_max_void_percent <= 100
	&& justify_max_void_percent >= ((100 * spare)
					/ (WRAP_COLS(text)
					   - (int) style->rightIndent
					   - indent
					   + ctrl_chars_on_previous_line))) {
	/* this is the only case when we need justification */
	char *jp = previous->data + justify_start_position;
	ht_run_info *r = ht_runs;
	char c;
	int total_byte_len = 0, total_cell_len = 0;
	int d_, r_;
	HTLine *jline;

	ht_num_runs = 0;
	r->byte_len = r->cell_len = 0;

	for (; (c = *jp) != 0; ++jp) {
	    if (c == ' ') {
		total_byte_len += r->byte_len;
		total_cell_len += r->cell_len;
		++r;
		++ht_num_runs;
		r->byte_len = r->cell_len = 0;
		continue;
	    }
	    ++r->byte_len;
	    if (IsSpecialAttrChar(c))
		continue;

	    ++r->cell_len;
	    if (c == HT_NON_BREAK_SPACE) {
		*jp = ' ';	/* substitute it */
		continue;
	    }
	    if (text->T.output_utf8 && is8bits(c)) {
		int utf_extra = utf8_length(text->T.output_utf8, jp);

		r->byte_len += utf_extra;
		jp += utf_extra;
	    }
	}
	total_byte_len += r->byte_len;
	total_cell_len += r->cell_len;
	++ht_num_runs;

	if (ht_num_runs != 1) {
	    int *oldpos = (int *) malloc(sizeof(int) * 2 * (ht_num_runs - 1));
	    int *newpos = oldpos + ht_num_runs - 1;
	    int i = 1;

	    if (oldpos == NULL)
		outofmem(__FILE__, "split_line_3");

	    d_ = spare / (ht_num_runs - 1);
	    r_ = spare % (ht_num_runs - 1);

	    /* The first run is not moved, proceed to the second one */
	    oldpos[0] = justify_start_position + ht_runs[0].cell_len + 1;
	    newpos[0] = oldpos[0] + (d_ + (r_-- > 0));
	    while (i < ht_num_runs - 1) {
		int delta = ht_runs[i].cell_len + 1;

		oldpos[i] = oldpos[i - 1] + delta;
		newpos[i] = newpos[i - 1] + delta + (d_ + (r_-- > 0));
		i++;
	    }
	    jline = insert_blanks_in_line(previous, CurLine, text,
					  &last_anchor_of_previous_line /*updates++ */ ,
					  ht_num_runs - 1, oldpos, newpos);
	    free(oldpos);
	    if (jline == NULL)
		outofmem(__FILE__, "split_line_4");
	    previous->next->prev = jline;
	    previous->prev->next = jline;

	    freeHTLine(text, previous);

	    previous = jline;
	}
	if (justify_start_position) {
	    char *p2 = previous->data;

	    for (; p2 < previous->data + justify_start_position; ++p2)
		*p2 = (*p2 == HT_NON_BREAK_SPACE ? ' ' : *p2);
	}
    } else {
	if (REALLY_CAN_JUSTIFY(text)) {
	    char *p2;

	    /* it was permitted to justify line, but this function was called
	     * to end paragraph - we must substitute HT_NON_BREAK_SPACEs with
	     * spaces in previous line
	     */
	    if (line->size && !text->stbl) {
		CTRACE((tfp,
			"BUG: justification: shouldn't happen - new line is not empty!\n\t'%.*s'\n",
			line->size, line->data));
	    }

	    for (p2 = previous->data; *p2; ++p2)
		if (*p2 == HT_NON_BREAK_SPACE)
		    *p2 = ' ';
	} else if (have_raw_nbsps) {
	    /* this is very rare case, that can happen in forms placed in
	       table cells */
	    unsigned i;

	    for (i = 0; i < previous->size; ++i)
		if (previous->data[i] == HT_NON_BREAK_SPACE)
		    previous->data[i] = ' ';

	    /*next line won't be justified, so substitute nbsps in it too */
	    for (i = 0; i < line->size; ++i)
		if (line->data[i] == HT_NON_BREAK_SPACE)
		    line->data[i] = ' ';
	}

	/* else HT_NON_BREAK_SPACEs were substituted with spaces in
	   HText_appendCharacter */
    }
    /* cleanup */
    can_justify_this_line = TRUE;
    justify_start_position = 0;
    this_line_was_split = FALSE;
    have_raw_nbsps = FALSE;
#endif /* EXP_JUSTIFY_ELTS */
    return;
}				/* split_line */

/*	Allow vertical blank space
 *	--------------------------
 */
static void blank_lines(HText *text, int newlines)
{
    if (HText_LastLineEmpty(text, FALSE)) {	/* No text on current line */
	HTLine *line = text->last_line->prev;
	BOOL first = (line == text->last_line);

	if (no_title && first)
	    return;

#ifdef USE_COLOR_STYLE
	/* Style-change petty requests at the start of the document: */
	if (first && newlines == 1)
	    return;		/* Do not add a blank line at start */
#endif

	while (line != NULL &&
	       line != text->last_line &&
	       HText_TrueEmptyLine(line, text, FALSE)) {
	    if (newlines == 0)
		break;
	    newlines--;		/* Don't bother: already blank */
	    line = line->prev;
	}
    } else {
	newlines++;		/* Need also to finish this line */
    }

    for (; newlines; newlines--) {
	new_line(text);
    }
    text->in_line_1 = YES;
}

/*	New paragraph in current style
 *	------------------------------
 * See also: setStyle.
 */
void HText_appendParagraph(HText *text)
{
    int after = text->style->spaceAfter;
    int before = text->style->spaceBefore;

    blank_lines(text, ((after > before) ? after : before));
}

/*	Set Style
 *	---------
 *
 *	Does not filter unnecessary style changes.
 */
void HText_setStyle(HText *text, HTStyle *style)
{
    int after, before;

    if (!style)
	return;			/* Safety */
    after = text->style->spaceAfter;
    before = style->spaceBefore;

    CTRACE((tfp, "GridText: Change to style %s\n", style->name));

    blank_lines(text, ((after > before) ? after : before));

    text->style = style;
}

/*	Append a character to the text object
 *	-------------------------------------
 */
void HText_appendCharacter(HText *text, int ch)
{
    HTLine *line;
    HTStyle *style;
    int indent;
    int limit = 0;
    int actual;

#ifdef DEBUG_APPCH
#ifdef CJK_EX
    static unsigned char save_ch = 0;
#endif

    if (TRACE) {
	char *special = NULL;	/* make trace a little more readable */

	switch (ch) {
	case HT_NON_BREAK_SPACE:
	    special = "HT_NON_BREAK_SPACE";
	    break;
	case HT_EN_SPACE:
	    special = "HT_EN_SPACE";
	    break;
	case LY_UNDERLINE_START_CHAR:
	    special = "LY_UNDERLINE_START_CHAR";
	    break;
	case LY_UNDERLINE_END_CHAR:
	    special = "LY_UNDERLINE_END_CHAR";
	    break;
	case LY_BOLD_START_CHAR:
	    special = "LY_BOLD_START_CHAR";
	    break;
	case LY_BOLD_END_CHAR:
	    special = "LY_BOLD_END_CHAR";
	    break;
	case LY_SOFT_HYPHEN:
	    special = "LY_SOFT_HYPHEN";
	    break;
	case LY_SOFT_NEWLINE:
	    special = "LY_SOFT_NEWLINE";
	    break;
	default:
	    special = NULL;
	    break;
	}

	if (special != NULL) {
	    CTRACE((tfp, "add(%s %d special char) %d/%d\n", special, ch,
		    HTisDocumentSource(), HTOutputFormat != WWW_SOURCE));
	} else {
#ifdef CJK_EX			/* 1998/08/30 (Sun) 13:26:23 */
	    if (save_ch == 0) {
		if (IS_SJIS_HI1(ch) || IS_SJIS_HI2(ch)) {
		    save_ch = ch;
		} else {
		    CTRACE((tfp, "add(%c) %d/%d\n", ch,
			    HTisDocumentSource(), HTOutputFormat != WWW_SOURCE));
		}
	    } else {
		CTRACE((tfp, "add(%c%c) %d/%d\n", save_ch, ch,
			HTisDocumentSource(), HTOutputFormat != WWW_SOURCE));
		save_ch = 0;
	    }
#else
	    if (UCH(ch) < 0x80) {
		CTRACE((tfp, "add(%c) %d/%d\n", UCH(ch),
			HTisDocumentSource(), HTOutputFormat != WWW_SOURCE));
	    } else {
		CTRACE((tfp, "add(%02x) %d/%d\n", UCH(ch),
			HTisDocumentSource(), HTOutputFormat != WWW_SOURCE));
	    }
#endif /* CJK_EX */
	}
    }				/* trace only */
#endif /* DEBUG_APPCH */

    /*
     * Make sure we don't crash on NULLs.
     */
    if (!text)
	return;

    if (text->halted > 1) {
	/*
	 * We should stop outputting more text, because low memory was
	 * detected.  - kw
	 */
	if (text->halted == 2) {
	    /*
	     * But if we haven't done so yet, first append a warning.
	     * We should still have a few bytes left for that :).
	     * We temporarily reset test->halted to 0 for this, since
	     * this function will get called recursively.  - kw
	     */
	    text->halted = 0;
	    text->kanji_buf = '\0';
	    HText_appendText(text, gettext(" *** MEMORY EXHAUSTED ***"));
	}
	text->halted = 3;
	return;
    }
#ifdef USE_TH_JP_AUTO_DETECT
    if ((HTCJK == JAPANESE) && (text->detected_kcode != DET_MIXED) &&
	(text->specified_kcode != SJIS) && (text->specified_kcode != EUC)) {
	unsigned char c;
	eDetectedKCode save_d_kcode;

	c = UCH(ch);
	save_d_kcode = text->detected_kcode;
	switch (text->SJIS_status) {
	case SJIS_state_has_bad_code:
	    break;
	case SJIS_state_neutral:
	    if (IS_SJIS_HI1(c) || IS_SJIS_HI2(c)) {
		text->SJIS_status = SJIS_state_in_kanji;
	    } else if ((c & 0x80) && !IS_SJIS_X0201KANA(c)) {
		text->SJIS_status = SJIS_state_has_bad_code;
		if (text->EUC_status == EUC_state_has_bad_code)
		    text->detected_kcode = DET_MIXED;
		else
		    text->detected_kcode = DET_EUC;
	    }
	    break;
	case SJIS_state_in_kanji:
	    if (IS_SJIS_LO(c)) {
		text->SJIS_status = SJIS_state_neutral;
	    } else {
		text->SJIS_status = SJIS_state_has_bad_code;
		if (text->EUC_status == EUC_state_has_bad_code)
		    text->detected_kcode = DET_MIXED;
		else
		    text->detected_kcode = DET_EUC;
	    }
	    break;
	}
	switch (text->EUC_status) {
	case EUC_state_has_bad_code:
	    break;
	case EUC_state_neutral:
	    if (IS_EUC_HI(c)) {
		text->EUC_status = EUC_state_in_kanji;
	    } else if (c == 0x8e) {
		text->EUC_status = EUC_state_in_kana;
	    } else if (c & 0x80) {
		text->EUC_status = EUC_state_has_bad_code;
		if (text->SJIS_status == SJIS_state_has_bad_code)
		    text->detected_kcode = DET_MIXED;
		else
		    text->detected_kcode = DET_SJIS;
	    }
	    break;
	case EUC_state_in_kanji:
	    if (IS_EUC_LOX(c)) {
		text->EUC_status = EUC_state_neutral;
	    } else {
		text->EUC_status = EUC_state_has_bad_code;
		if (text->SJIS_status == SJIS_state_has_bad_code)
		    text->detected_kcode = DET_MIXED;
		else
		    text->detected_kcode = DET_SJIS;
	    }
	    break;
	case EUC_state_in_kana:
	    if ((0xA1 <= c) && (c <= 0xDF)) {
		text->EUC_status = EUC_state_neutral;
	    } else {
		text->EUC_status = EUC_state_has_bad_code;
		if (text->SJIS_status == SJIS_state_has_bad_code)
		    text->detected_kcode = DET_MIXED;
		else
		    text->detected_kcode = DET_SJIS;
	    }
	    break;
	}
	if (save_d_kcode != text->detected_kcode) {
	    switch (text->detected_kcode) {
	    case DET_SJIS:
		CTRACE((tfp,
			"TH_JP_AUTO_DETECT: This document's kcode seems SJIS.\n"));
		break;
	    case DET_EUC:
		CTRACE((tfp,
			"TH_JP_AUTO_DETECT: This document's kcode seems EUC.\n"));
		break;
	    case DET_MIXED:
		CTRACE((tfp,
			"TH_JP_AUTO_DETECT: This document's kcode seems mixed!\n"));
		break;
	    default:
		CTRACE((tfp,
			"TH_JP_AUTO_DETECT: This document's kcode is unexpected!\n"));
		break;
	    }
	}
    }
#endif /* USE_TH_JP_AUTO_DETECT */
    /*
     * Make sure we don't hang on escape sequences.
     */
    if (ch == CH_ESC && HTCJK == NOCJK) {	/* decimal 27  S/390 -- gil -- 1504 */
	return;
    }
#ifndef USE_SLANG
    /*
     * Block 8-bit chars not allowed by the current display character
     * set if they are below what LYlowest_eightbit indicates.
     * Slang used its own replacements, so for USE_SLANG blocking here
     * is not necessary to protect terminals from those characters.
     * They should have been filtered out or translated by an earlier
     * processing stage anyway.  - kw
     */
#ifndef   EBCDIC		/* S/390 -- gil -- 1514 */
    if (is8bits(ch) && HTCJK == NOCJK &&
	!text->T.transp && !text->T.output_utf8 &&
	UCH(ch) < LYlowest_eightbit[current_char_set]) {
	return;
    }
#endif /* EBCDIC */
#endif /* !USE_SLANG */
    if (UCH(ch) == 155 && HTCJK == NOCJK) {	/* octal 233 */
	if (!HTPassHighCtrlRaw &&
	    !text->T.transp && !text->T.output_utf8 &&
	    (155 < LYlowest_eightbit[current_char_set])) {
	    return;
	}
    }

    line = text->last_line;
    style = text->style;

    indent = text->in_line_1 ? (int) style->indent1st : (int) style->leftIndent;

    if (HTCJK != NOCJK) {
	switch (text->state) {
	case S_text:
	    if (ch == CH_ESC) {	/* S/390 -- gil -- 1536 */
		/*
		 * Setting up for CJK escape sequence handling (based on
		 * Takuya ASADA's (asada@three-a.co.jp) CJK Lynx).  -FM
		 */
		text->state = S_esc;
		text->kanji_buf = '\0';
		return;
	    }
	    break;

	case S_esc:
	    /*
	     * Expecting '$'or '(' following CJK ESC.
	     */
	    if (ch == '$') {
		text->state = S_dollar;
		return;
	    } else if (ch == '(') {
		text->state = S_paren;
		return;
	    } else {
		text->state = S_text;
	    }
	    /* FALLTHRU */

	case S_dollar:
	    /*
	     * Expecting '@', 'B', 'A' or '(' after CJK "ESC$".
	     */
	    if (ch == '@' || ch == 'B' || ch == 'A') {
		text->state = S_nonascii_text;
		if (ch == '@' || ch == 'B')
		    text->kcode = JIS;
		return;
	    } else if (ch == '(') {
		text->state = S_dollar_paren;
		return;
	    } else {
		text->state = S_text;
	    }
	    break;

	case S_dollar_paren:
	    /*
	     * Expecting 'C' after CJK "ESC$(".
	     */
	    if (ch == 'C') {
		text->state = S_nonascii_text;
		return;
	    } else {
		text->state = S_text;
	    }
	    break;

	case S_paren:
	    /*
	     * Expecting 'B', 'J', 'T' or 'I' after CJK "ESC(".
	     */
	    if (ch == 'B' || ch == 'J' || ch == 'T') {
		/*
		 * Can split here.  -FM
		 */
		text->permissible_split = text->last_line->size;
		text->state = S_text;
		return;
	    } else if (ch == 'I') {
		text->state = S_jisx0201_text;
		/*
		 * Can split here.  -FM
		 */
		text->permissible_split = text->last_line->size;
		text->kcode = JIS;
		return;
	    } else {
		text->state = S_text;
	    }
	    break;

	case S_nonascii_text:
	    /*
	     * Expecting CJK ESC after non-ASCII text.
	     */
	    if (ch == CH_ESC) {	/* S/390 -- gil -- 1553 */
		text->state = S_esc;
		text->kanji_buf = '\0';
		if (HTCJK == JAPANESE) {
		    text->kcode = NOKANJI;
		}
		return;
	    } else if (UCH(ch) < 32) {
		text->state = S_text;
		text->kanji_buf = '\0';
		if (HTCJK == JAPANESE) {
		    text->kcode = NOKANJI;
		}
	    } else {
		ch |= 0200;
	    }
	    break;

	    /*
	     * JIS X0201 Kana in JIS support.  - by ASATAKU
	     */
	case S_jisx0201_text:
	    if (ch == CH_ESC) {	/* S/390 -- gil -- 1570 */
		text->state = S_esc;
		text->kanji_buf = '\0';
		text->kcode = NOKANJI;
		return;
	    } else {
		text->kanji_buf = '\216';
		ch |= 0200;
	    }
	    break;
	}			/* end switch */

	if (!text->kanji_buf) {
	    if ((ch & 0200) != 0) {
		/*
		 * JIS X0201 Kana in SJIS support.  - by ASATAKU
		 */
		if ((text->kcode != JIS)
		    && (
#ifdef KANJI_CODE_OVERRIDE
			   (last_kcode == SJIS) ||
			   ((last_kcode == NOKANJI) &&
#endif
			    ((text->kcode == SJIS) ||
#ifdef USE_TH_JP_AUTO_DETECT
			     ((text->detected_kcode == DET_SJIS) &&
			      (text->specified_kcode == NOKANJI)) ||
#endif
			     ((text->kcode == NOKANJI) &&
			      (text->specified_kcode == SJIS)))
#ifdef KANJI_CODE_OVERRIDE
			   )
#endif
		    ) &&
		    (UCH(ch) >= 0xA1) &&
		    (UCH(ch) <= 0xDF)) {
#ifdef CONV_JISX0201KANA_JISX0208KANA
		    unsigned char c = UCH(ch);
		    unsigned char kb = UCH(text->kanji_buf);

		    JISx0201TO0208_SJIS(c,
					(unsigned char *) &kb,
					(unsigned char *) &c);
		    ch = (char) c;
		    text->kanji_buf = kb;
#endif
		    /* 1998/01/19 (Mon) 09:06:15 */
		    text->permissible_split = (int) text->last_line->size;
		} else {
		    text->kanji_buf = ch;
		    /*
		     * Can split here.  -FM
		     */
		    text->permissible_split = text->last_line->size;
		    return;
		}
	    }
	} else {
	    goto check_WrapSource;
	}
    } else if (ch == CH_ESC) {	/* S/390 -- gil -- 1587 */
	return;
    }
#ifdef CJK_EX			/* MOJI-BAKE Fix! 1997/10/12 -- 10/31 (Fri) 00:22:57 - JH7AYN */
    if (HTCJK != NOCJK &&	/* added condition - kw */
	(ch == LY_BOLD_START_CHAR || ch == LY_BOLD_END_CHAR)) {
	text->permissible_split = (int) line->size;	/* Can split here */
	if (HTCJK == JAPANESE)
	    text->kcode = NOKANJI;
    }
#endif

    if (IsSpecialAttrChar(ch) && ch != LY_SOFT_NEWLINE) {
#if !defined(USE_COLOR_STYLE) || !defined(NO_DUMP_WITH_BACKSPACES)
	if (line->size >= (MAX_LINE - 1)) {
	    return;
	}
#if defined(USE_COLOR_STYLE) && !defined(NO_DUMP_WITH_BACKSPACES)
	if (with_backspaces && HTCJK == NOCJK && !text->T.output_utf8) {
#endif
	    if (ch == LY_UNDERLINE_START_CHAR) {
		line->data[line->size++] = LY_UNDERLINE_START_CHAR;
		line->data[line->size] = '\0';
		underline_on = ON;
		if (!(dump_output_immediately && use_underscore))
		    ctrl_chars_on_this_line++;
		return;
	    } else if (ch == LY_UNDERLINE_END_CHAR) {
		line->data[line->size++] = LY_UNDERLINE_END_CHAR;
		line->data[line->size] = '\0';
		underline_on = OFF;
		if (!(dump_output_immediately && use_underscore))
		    ctrl_chars_on_this_line++;
		return;
	    } else if (ch == LY_BOLD_START_CHAR) {
		line->data[line->size++] = LY_BOLD_START_CHAR;
		line->data[line->size] = '\0';
		bold_on = ON;
		ctrl_chars_on_this_line++;
		return;
	    } else if (ch == LY_BOLD_END_CHAR) {
		line->data[line->size++] = LY_BOLD_END_CHAR;
		line->data[line->size] = '\0';
		bold_on = OFF;
		ctrl_chars_on_this_line++;
		return;
	    } else if (ch == LY_SOFT_HYPHEN) {
		int i;

		/*
		 * Ignore the soft hyphen if it is the first character
		 * on the line, or if it is preceded by a space or
		 * hyphen.  -FM
		 */
		if (line->size < 1 || text->permissible_split >= line->size) {
		    return;
		}

		for (i = (text->permissible_split + 1); line->data[i]; i++) {
		    if (!IsSpecialAttrChar(UCH(line->data[i])) &&
			!isspace(UCH(line->data[i])) &&
			UCH(line->data[i]) != '-' &&
			UCH(line->data[i]) != HT_NON_BREAK_SPACE &&
			UCH(line->data[i]) != HT_EN_SPACE) {
			break;
		    }
		}
		if (line->data[i] == '\0') {
		    return;
		}
	    }
#if defined(USE_COLOR_STYLE) && !defined(NO_DUMP_WITH_BACKSPACES)
	} else {
	    /* if (with_backspaces && HTCJK==HTNOCJK && !text->T.output_utf8) */
	    return;
	}
#endif

#else
	return;
#endif
    } else if (ch == LY_SOFT_NEWLINE) {
	line->data[line->size++] = LY_SOFT_NEWLINE;
	line->data[line->size] = '\0';
	return;
    }

    if (text->T.output_utf8) {
	/*
	 * Some extra checks for UTF-8 output here to make sure
	 * memory is not overrun.  For a non-first char, append
	 * to the line here and return.  - kw
	 */
	if (IS_UTF_EXTRA(ch)) {
	    if ((line->size > (MAX_LINE - 1))
		|| (indent + (int) (line->offset + line->size)
		    + UTFXTRA_ON_THIS_LINE
		    - ctrl_chars_on_this_line
		    + ((line->size > 0) &&
		       (int) (line->data[line->size - 1] ==
			      LY_SOFT_HYPHEN ?
			      1 : 0)) >= LYcols_cu(text))
		) {
		if (!text->permissible_split || text->source) {
		    text->permissible_split = line->size;
		    while (text->permissible_split > 0 &&
			   IS_UTF_EXTRA(line->data[text->permissible_split - 1]))
			text->permissible_split--;
		    if (text->permissible_split &&
			(line->data[text->permissible_split - 1] & 0x80))
			text->permissible_split--;
		    if (text->permissible_split == line->size)
			text->permissible_split = 0;
		}
		split_line(text, text->permissible_split);
		line = text->last_line;
		if (text->source && line->size - ctrl_chars_on_this_line
		    + UTFXTRA_ON_THIS_LINE == 0)
		    HText_appendCharacter(text, LY_SOFT_NEWLINE);
	    }
	    line->data[line->size++] = (char) ch;
	    line->data[line->size] = '\0';
	    utfxtra_on_this_line++;
	    ctrl_chars_on_this_line++;
	    return;
	} else if (ch & 0x80) {	/* a first char of UTF-8 sequence - kw */
	    if ((line->size > (MAX_LINE - 7))) {
		if (!text->permissible_split || text->source) {
		    text->permissible_split = line->size;
		    while (text->permissible_split > 0 &&
			   (line->data[text->permissible_split - 1] & 0x80)
			   == 0xC0) {
			text->permissible_split--;
		    }
		    if (text->permissible_split == line->size)
			text->permissible_split = 0;
		}
		split_line(text, text->permissible_split);
		line = text->last_line;
		if (text->source && line->size - ctrl_chars_on_this_line
		    + UTFXTRA_ON_THIS_LINE == 0)
		    HText_appendCharacter(text, LY_SOFT_NEWLINE);
	    }
	}
    }

    /*
     * New Line.
     */
    if (ch == '\n') {
	new_line(text);
	text->in_line_1 = YES;	/* First line of new paragraph */
	/*
	 * There are some pages written in
	 * different kanji codes.  - TA & kw
	 */
	if (HTCJK == JAPANESE)
	    text->kcode = NOKANJI;
	return;
    }

    /*
     * Convert EN_SPACE to a space here so that it doesn't get collapsed.
     */
    if (ch == HT_EN_SPACE)
	ch = ' ';

#ifdef SH_EX			/* 1997/11/01 (Sat) 12:08:54 */
    if (ch == 0x0b) {		/* ^K ??? */
	ch = '\r';
    }
    if (ch == 0x1a) {		/* ^Z ??? */
	ch = '\r';
    }
#endif

    /*
     * I'm going to cheat here in a BIG way.  Since I know that all
     * \r's will be trapped by HTML_put_character I'm going to use
     * \r to mean go down a line but don't start a new paragraph.
     * i.e., use the second line indenting.
     */
    if (ch == '\r') {
	new_line(text);
	text->in_line_1 = NO;
	/*
	 * There are some pages written in
	 * different kanji codes.  - TA & kw
	 */
	if (HTCJK == JAPANESE)
	    text->kcode = NOKANJI;
	return;
    }

    /*
     * Tabs.
     */
    if (ch == '\t') {
	const HTTabStop *Tab;
	int target, target_cu;	/* Where to tab to */
	int here, here_cu;	/* in _cu we try to guess what curses thinks */

	if (line->size > 0 && line->data[line->size - 1] == LY_SOFT_HYPHEN) {
	    /*
	     * A tab shouldn't follow a soft hyphen, so
	     * if one does, we'll dump the soft hyphen.  -FM
	     */
	    line->data[--line->size] = '\0';
	    ctrl_chars_on_this_line--;
	}
	here = ((int) (line->size + line->offset) + indent)
	    - ctrl_chars_on_this_line;	/* Consider special chars GAB */
	here_cu = here + UTFXTRA_ON_THIS_LINE;
	if (style->tabs) {	/* Use tab table */
	    for (Tab = style->tabs;
		 Tab->position <= here;
		 Tab++) {
		if (!Tab->position) {
		    new_line(text);
		    return;
		}
	    }
	    target = Tab->position;
	} else if (text->in_line_1) {	/* Use 2nd indent */
	    if (here >= (int) style->leftIndent) {
		new_line(text);	/* wrap */
		return;
	    } else {
		target = (int) style->leftIndent;
	    }
	} else {		/* Default tabs align with left indent mod 8 */
#ifdef DEFAULT_TABS_8
	    target = (((int) line->offset + (int) line->size + 8) & (-8))
		+ (int) style->leftIndent;
#else
	    new_line(text);
	    return;
#endif
	}

	if (target >= here)
	    target_cu = target;
	else
	    target_cu = target + (here_cu - here);

	if (target > WRAP_COLS(text) - (int) style->rightIndent &&
	    HTOutputFormat != WWW_SOURCE) {
	    new_line(text);
	} else {
	    /*
	     * Can split here.  -FM
	     */
	    text->permissible_split = line->size;
	    if (target_cu > WRAP_COLS(text))
		target -= target_cu - WRAP_COLS(text);
	    if (line->size == 0) {
		line->offset += (target - here);
	    } else {
		for (; here < target; here++) {
		    /* Put character into line */
		    line->data[line->size++] = ' ';
		    line->data[line->size] = '\0';
		}
	    }
	}
	return;
    }
    /* if tab */
  check_WrapSource:
    if ((text->source || dont_wrap_pre) && text == HTMainText) {
	/*
	 * If we're displaying document source, wrap long lines to keep all of
	 * the source visible.
	 */
	int target = (int) (line->offset + line->size) - ctrl_chars_on_this_line;
	int target_cu = target + UTFXTRA_ON_THIS_LINE;

	if (target >= WRAP_COLS(text) - style->rightIndent -
	    (((HTCJK != NOCJK) && text->kanji_buf) ? 1 : 0) ||
	    (text->T.output_utf8 &&
	     target_cu + UTF_XLEN(ch) >= LYcols_cu(text))) {
	    int saved_kanji_buf;
	    eGridState saved_state;

	    new_line(text);
	    line = text->last_line;

	    saved_kanji_buf = text->kanji_buf;
	    saved_state = text->state;
	    text->kanji_buf = '\0';
	    text->state = S_text;
	    HText_appendCharacter(text, LY_SOFT_NEWLINE);
	    text->kanji_buf = saved_kanji_buf;
	    text->state = saved_state;
	}
    }

    if (ch == ' ') {
	/*
	 * Can split here.  -FM
	 */
	text->permissible_split = text->last_line->size;
	/*
	 * There are some pages written in
	 * different kanji codes.  - TA
	 */
	if (HTCJK == JAPANESE)
	    text->kcode = NOKANJI;
    }

    /*
     * Check if we should ignore characters at the wrap point.
     */
    if (text->IgnoreExcess) {
	int nominal = (indent + (int) (line->offset + line->size) - ctrl_chars_on_this_line);
	int number;

	limit = WRAP_COLS(text);
	if (fields_are_numbered()
	    && !number_fields_on_left
	    && text->last_anchor != 0
	    && (number = text->last_anchor->number) > 0) {
	    limit -= (number > 99999
		      ? 6
		      : (number > 9999
			 ? 5
			 : (number > 999
			    ? 4
			    : (number > 99
			       ? 3
			       : (number > 9
				  ? 2
				  : 1))))) + 2;
	}
	if ((nominal + (int) style->rightIndent) >= limit
	    || (nominal + UTFXTRA_ON_THIS_LINE) >= LYcols_cu(text)) {
	    return;
	}
    }

    /*
     * Check for end of line.
     */
    actual = ((indent + (int) line->offset + (int) line->size) +
	      ((line->size > 0) &&
	       (int) (line->data[line->size - 1] == LY_SOFT_HYPHEN ? 1 : 0)));

    if ((actual
	 + (int) style->rightIndent
	 - ctrl_chars_on_this_line
	 + (((HTCJK != NOCJK) && text->kanji_buf) ? 1 : 0)
	) >= WRAP_COLS(text)
	|| (text->T.output_utf8
	    && ((actual
		 + UTFXTRA_ON_THIS_LINE
		 - ctrl_chars_on_this_line
		 + UTF_XLEN(ch)
		) >= (LYcols_cu(text) - 1)))) {

	if (style->wordWrap && HTOutputFormat != WWW_SOURCE) {
#ifdef EXP_JUSTIFY_ELTS
	    if (REALLY_CAN_JUSTIFY(text))
		this_line_was_split = TRUE;
#endif
	    split_line(text, text->permissible_split);
	    if (ch == ' ') {
		return;		/* Ignore space causing split */
	    }

	} else if (HTOutputFormat == WWW_SOURCE) {
	    /*
	     * For source output we don't want to wrap this stuff
	     * unless absolutely necessary.  - LJM
	     * !
	     * If we don't wrap here we might get a segmentation fault.
	     * but let's see what happens
	     */
	    if ((int) line->size >= (int) (MAX_LINE - 1)) {
		new_line(text);	/* try not to linewrap */
	    }
	} else {
	    /*
	     * For normal stuff like pre let's go ahead and
	     * wrap so the user can see all of the text.
	     */
	    if ((dump_output_immediately || (crawl && traversal))
		&& dont_wrap_pre) {
		if ((int) line->size >= (int) (MAX_LINE - 1)) {
		    new_line(text);
		}
	    } else {
		new_line(text);
	    }
	}
    } else if ((int) line->size >= (int) (MAX_LINE - 1)) {
	/*
	 * Never overrun memory if DISPLAY_COLS is set to a large value - KW
	 */
	new_line(text);
    }

    /*
     * Insert normal characters.
     */
    if (ch == HT_NON_BREAK_SPACE
#ifdef EXP_JUSTIFY_ELTS
	&& !REALLY_CAN_JUSTIFY(text)
#endif
	)
	ch = ' ';
#ifdef EXP_JUSTIFY_ELTS
    else
	have_raw_nbsps = TRUE;
#endif

    /* we leave raw HT_NON_BREAK_SPACE otherwise (we'll substitute it later) */

    if (ch & 0x80)
	text->have_8bit_chars = YES;

    /*
     * Kanji charactor handling.
     */
    {
	HTFont font = style->font;
	unsigned char hi, lo, tmp[2];

	line = text->last_line;	/* May have changed */

	if (HTCJK != NOCJK && text->kanji_buf) {
	    hi = UCH(text->kanji_buf);
	    lo = UCH(ch);

	    if (HTCJK == JAPANESE) {
		if (text->kcode != JIS) {
		    if (IS_SJIS_2BYTE(hi, lo)) {
			if (IS_EUC(hi, lo)) {
#ifdef KANJI_CODE_OVERRIDE
			    if (last_kcode != NOKANJI)
				text->kcode = last_kcode;
			    else
#endif
			    if (text->specified_kcode != NOKANJI)
				text->kcode = text->specified_kcode;
#ifdef USE_TH_JP_AUTO_DETECT
			    else if (text->detected_kcode == DET_EUC)
				text->kcode = EUC;
			    else if (text->detected_kcode == DET_SJIS)
				text->kcode = SJIS;
#endif
			    else if (IS_EUC_X0201KANA(hi, lo) &&
				     (text->kcode != EUC))
				text->kcode = SJIS;
			} else
			    text->kcode = SJIS;
		    } else if (IS_EUC(hi, lo))
			text->kcode = EUC;
		    else
			text->kcode = NOKANJI;
		}

		switch (kanji_code) {
		case EUC:
		    if (text->kcode == SJIS) {
			SJIS_TO_EUC1(hi, lo, tmp);
			line->data[line->size++] = tmp[0];
			line->data[line->size++] = tmp[1];
		    } else if (IS_EUC(hi, lo)) {
#ifdef CONV_JISX0201KANA_JISX0208KANA
			JISx0201TO0208_EUC(hi, lo, &hi, &lo);
#endif
			line->data[line->size++] = hi;
			line->data[line->size++] = lo;
		    } else {
			CTRACE((tfp,
				"This character (%X:%X) doesn't seem Japanese\n",
				hi, lo));
			line->data[line->size++] = '=';
			line->data[line->size++] = '=';
		    }
		    break;

		case SJIS:
		    if ((text->kcode == EUC) || (text->kcode == JIS)) {
#ifndef CONV_JISX0201KANA_JISX0208KANA
			if (IS_EUC_X0201KANA(hi, lo))
			    line->data[line->size++] = lo;
			else
#endif
			{
			    EUC_TO_SJIS1(hi, lo, tmp);
			    line->data[line->size++] = tmp[0];
			    line->data[line->size++] = tmp[1];
			}
		    } else if (IS_SJIS_2BYTE(hi, lo)) {
			line->data[line->size++] = hi;
			line->data[line->size++] = lo;
		    } else {
			line->data[line->size++] = '=';
			line->data[line->size++] = '=';
			CTRACE((tfp,
				"This character (%X:%X) doesn't seem Japanese\n",
				hi, lo));
		    }
		    break;

		default:
		    break;
		}
	    } else {
		line->data[line->size++] = hi;
		line->data[line->size++] = lo;
	    }
	    text->kanji_buf = 0;
	}
#ifndef CONV_JISX0201KANA_JISX0208KANA
	else if ((HTCJK == JAPANESE) && IS_SJIS_X0201KANA(UCH((ch))) &&
		 (kanji_code == EUC)) {
	    line->data[line->size++] = UCH(0x8e);
	    line->data[line->size++] = ch;
	}
#endif
	else if (HTCJK != NOCJK) {
	    line->data[line->size++] = (char) ((kanji_code != NOKANJI) ?
					       ch :
					       (font & HT_CAPITALS) ?
					       TOUPPER(ch) : ch);
	} else {
	    line->data[line->size++] =	/* Put character into line */
		(char) (font & HT_CAPITALS ? TOUPPER(ch) : ch);
	}
	line->data[line->size] = '\0';
	if (font & HT_DOUBLE)	/* Do again if doubled */
	    HText_appendCharacter(text, HT_NON_BREAK_SPACE);
	/* NOT a permissible split */

	if (ch == LY_SOFT_HYPHEN) {
	    ctrl_chars_on_this_line++;
	    /*
	     * Can split here.  -FM
	     */
	    text->permissible_split = text->last_line->size;
	}
	if (ch == LY_SOFT_NEWLINE) {
	    ctrl_chars_on_this_line++;
	}
    }
    return;
}

#ifdef USE_COLOR_STYLE
/*  Insert a style change into the current line
 *  -------------------------------------------
 */
void _internal_HTC(HText *text, int style, int dir)
{
    HTLine *line;

    /* can't change style if we have no text to change style with */
    if (text != 0) {

	line = text->last_line;

	if (line->numstyles > 0 && dir == 0 &&
	    line->styles[line->numstyles - 1].direction &&
	    line->styles[line->numstyles - 1].style == (unsigned) style &&
	    (int) line->styles[line->numstyles - 1].horizpos
	    == (int) line->size - ctrl_chars_on_this_line) {
	    /*
	     * If this is an OFF change directly preceded by an
	     * ON for the same style, just remove the previous one.  - kw
	     */
	    line->numstyles--;
	} else if (line->numstyles < MAX_STYLES_ON_LINE) {
	    line->styles[line->numstyles].horizpos = line->size;
	    /*
	     * Special chars for bold and underlining usually don't
	     * occur with color style, but soft hyphen can.
	     * And in UTF-8 display mode all non-initial bytes are
	     * counted as ctrl_chars.  - kw
	     */
	    if ((int) line->styles[line->numstyles].horizpos >= ctrl_chars_on_this_line) {
		line->styles[line->numstyles].horizpos -= ctrl_chars_on_this_line;
	    }
	    line->styles[line->numstyles].style = style;
	    line->styles[line->numstyles].direction = dir;
	    line->numstyles++;
	}
    }
}
#endif

/*	Set LastChar element in the text object.
 *	----------------------------------------
 */
void HText_setLastChar(HText *text, char ch)
{
    if (!text)
	return;

    text->LastChar = ch;
}

/*	Get LastChar element in the text object.
 *	----------------------------------------
 */
char HText_getLastChar(HText *text)
{
    if (!text)
	return ('\0');

    return ((char) text->LastChar);
}

/*	Set IgnoreExcess element in the text object.
 *	--------------------------------------------
 */
void HText_setIgnoreExcess(HText *text, BOOL ignore)
{
    if (!text)
	return;

    text->IgnoreExcess = ignore;
}

/*		Simple table handling - private
 *		-------------------------------
 */

/*
 * HText_insertBlanksInStblLines fixes up table lines when simple table
 * processing is closed, by calling insert_blanks_in_line for lines
 * that need fixup.  Also recalculates alignment for those lines,
 * does additional updating of anchor positions, and makes sure the
 * display of the lines on screen will be updated after partial display
 * upon return to mainloop.  - kw
 */
static int HText_insertBlanksInStblLines(HText *me, int ncols)
{
    HTLine *line;
    HTLine *mod_line, *first_line = NULL;
    int *oldpos;
    int *newpos;
    int ninserts, lineno;
    int first_lineno, last_lineno, first_lineno_pass2;

#ifdef EXP_NESTED_TABLES
    int last_nonempty = -1;
#endif
    int added_chars_before = 0;
    int lines_changed = 0;
    int max_width = 0, indent, spare, table_offset;
    HTStyle *style;
    short alignment;
    int i = 0;

    lineno = first_lineno = Stbl_getStartLine(me->stbl);
    if (lineno < 0 || lineno > me->Lines)
	return -1;
    /*
     * oldpos, newpos:  allocate space for two int arrays.
     */
    oldpos = typecallocn(int, 2 * ncols);
    if (!oldpos)
	return -1;
    else
	newpos = oldpos + ncols;
    for (line = FirstHTLine(me); i < lineno; line = line->next, i++) {
	if (!line) {
	    free(oldpos);
	    return -1;
	}
    }
    first_lineno_pass2 = last_lineno = me->Lines;
    for (; line && lineno <= last_lineno; line = line->next, lineno++) {
	ninserts = Stbl_getFixupPositions(me->stbl, lineno, oldpos, newpos);
	if (ninserts < 0)
	    continue;
	if (!first_line) {
	    first_line = line;
	    first_lineno_pass2 = lineno;
	    if (TRACE) {
		int ip;

		CTRACE((tfp, "line %d first to adjust  --  newpos:", lineno));
		for (ip = 0; ip < ncols; ip++)
		    CTRACE((tfp, " %d", newpos[ip]));
		CTRACE((tfp, "\n"));
	    }
	}
	if (line == me->last_line) {
	    if (line->size == 0 || HText_TrueEmptyLine(line, me, FALSE))
		continue;
	    /*
	     * Last ditch effort to end the table with a line break,
	     * if HTML_end_element didn't do it.  - kw
	     */
	    if (first_line == line)	/* obscure: all table on last line... */
		first_line = NULL;
	    new_line(me);
	    line = me->last_line->prev;
	    if (first_line == NULL)
		first_line = line;
	}
	if (ninserts == 0) {
	    /*  Do it also for no positions (but not error) */
	    int width = HText_TrueLineSize(line, me, FALSE);

	    if (width > max_width)
		max_width = width;
#ifdef EXP_NESTED_TABLES
	    if (nested_tables) {
		if (width && last_nonempty < lineno)
		    last_nonempty = lineno;
	    }
#endif
	    CTRACE((tfp, "line %d true/max width:%d/%d oldpos: NONE\n",
		    lineno, width, max_width));
	    continue;
	}
	mod_line = insert_blanks_in_line(line, lineno, me,
					 &me->last_anchor_before_stbl /*updates++ */ ,
					 ninserts, oldpos, newpos);
	if (mod_line) {
	    if (line == me->last_line) {
		me->last_line = mod_line;
	    } else {
		added_chars_before += (mod_line->size - line->size);
	    }
	    line->prev->next = mod_line;
	    line->next->prev = mod_line;
	    lines_changed++;
	    if (line == first_line)
		first_line = mod_line;
	    freeHTLine(me, line);
	    line = mod_line;
#ifdef DISP_PARTIAL
	    /*
	     * Make sure modified lines get fully re-displayed after
	     * loading with partial display is done.
	     */
	    if (me->first_lineno_last_disp_partial >= 0) {
		if (me->first_lineno_last_disp_partial >= lineno) {
		    ResetPartialLinenos(me);
		} else if (me->last_lineno_last_disp_partial >= lineno) {
		    me->last_lineno_last_disp_partial = lineno - 1;
		}
	    }
#endif
	} {
	    int width = HText_TrueLineSize(line, me, FALSE);

	    if (width > max_width)
		max_width = width;
#ifdef EXP_NESTED_TABLES
	    if (nested_tables) {
		if (width && last_nonempty < lineno)
		    last_nonempty = lineno;
	    }
#endif
	    if (TRACE) {
		int ip;

		CTRACE((tfp, "line %d true/max width:%d/%d oldpos:",
			lineno, width, max_width));
		for (ip = 0; ip < ninserts; ip++)
		    CTRACE((tfp, " %d", oldpos[ip]));
		CTRACE((tfp, "\n"));
	    }
	}
    }
    /*
     * Line offsets have been set based on the paragraph style, and
     * have already been updated for centering or right-alignment
     * for each line in split_line.  Here we want to undo all that, and
     * align the table as a whole (i.e.  all lines for which
     * Stbl_getFixupPositions returned >= 0).  All those lines have to
     * get the same offset, for the simple table formatting mechanism
     * to make sense, and that may not actually be the case at this point.
     *
     * What indentation and alignment do we want for the table as
     * a whole?  Let's take most style properties from me->style.
     * With some luck, it is the appropriate style for the element
     * enclosing the TABLE.  But let's take alignment from the attribute
     * of the TABLE itself instead, if it was specified.
     *
     * Note that this logic assumes that all lines have been finished
     * by split_line.  The order of calls made by HTML_end_element for
     * HTML_TABLE should take care of this.
     */
    style = me->style;
    alignment = Stbl_getAlignment(me->stbl);
    if (alignment == HT_ALIGN_NONE)
	alignment = style->alignment;
    indent = style->leftIndent;
    /* Calculate spare character positions */
    spare = WRAP_COLS(me) -
	(int) style->rightIndent - indent - max_width;
    if (spare < 0 && (int) style->rightIndent + spare >= 0) {
	/*
	 * Not enough room!  But we can fit if we ignore right indentation,
	 * so let's do that.
	 */
	spare = 0;
    } else if (spare < 0) {
	spare += style->rightIndent;	/* ignore right indent, but need more */
    }
    if (spare < 0 && indent + spare >= 0) {
	/*
	 * Still not enough room.  But we can move to the left.
	 */
	indent += spare;
	spare = 0;
    } else if (spare < 0) {
	/*
	 * Still not enough.  Something went wrong.  Try the best we
	 * can do.
	 */
	CTRACE((tfp,
		"BUG: insertBlanks: resulting table too wide by %d positions!\n",
		-spare));
	indent = spare = 0;
    }
    /*
     * Align left, right or center.
     */
    switch (alignment) {
    case HT_CENTER:
	table_offset = indent + spare / 2;
	break;
    case HT_RIGHT:
	table_offset = indent + spare;
	break;
    case HT_LEFT:
    case HT_JUSTIFY:
    default:
	table_offset = indent;
	break;
    }				/* switch */

    CTRACE((tfp, "changing offsets"));
    for (line = first_line, lineno = first_lineno_pass2;
	 line && lineno <= last_lineno && line != me->last_line;
	 line = line->next, lineno++) {
	ninserts = Stbl_getFixupPositions(me->stbl, lineno, oldpos, newpos);
	if (ninserts >= 0 && (int) line->offset != table_offset) {
#ifdef DISP_PARTIAL
	    /*  As above make sure modified lines get fully re-displayed */
	    if (me->first_lineno_last_disp_partial >= 0) {
		if (me->first_lineno_last_disp_partial >= lineno) {
		    ResetPartialLinenos(me);
		} else if (me->last_lineno_last_disp_partial >= lineno) {
		    me->last_lineno_last_disp_partial = lineno - 1;
		}
	    }
#endif
	    CTRACE((tfp, " %d:%d", lineno, table_offset - line->offset));
	    line->offset = (table_offset > 0
			    ? table_offset
			    : 0);
	}
    }
#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	if (max_width)
	    Stbl_update_enclosing(me->stbl, max_width, last_nonempty);
    }
#endif
    CTRACE((tfp, " %d:done\n", lineno));
    free(oldpos);
    return lines_changed;
}

/*		Simple table handling - public functions
 *		----------------------------------------
 */

/*	Cancel simple table handling
*/
void HText_cancelStbl(HText *me)
{
    if (!me || !me->stbl) {
	CTRACE((tfp, "cancelStbl: ignored.\n"));
	return;
    }
    CTRACE((tfp, "cancelStbl: ok, will do.\n"));
#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	STable_info *stbl = me->stbl;

	while (stbl) {
	    STable_info *enclosing = Stbl_get_enclosing(stbl);

	    Stbl_free(stbl);
	    stbl = enclosing;
	}
    } else
#endif
	Stbl_free(me->stbl);
    me->stbl = NULL;
}

/*	Start simple table handling
*/
void HText_startStblTABLE(HText *me, short alignment)
{
#ifdef EXP_NESTED_TABLES
    STable_info *current = me->stbl;
#endif

    if (!me)
	return;

#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	if (current)
	    new_line(me);
    } else
#endif
    {
	if (me->stbl)
	    HText_cancelStbl(me);	/* auto cancel previously open table */
    }

    me->stbl = Stbl_startTABLE(alignment);
    if (me->stbl) {
	CTRACE((tfp, "startStblTABLE: started.\n"));
#ifdef EXP_NESTED_TABLES
	if (nested_tables) {
	    Stbl_set_enclosing(me->stbl, current, me->last_anchor_before_stbl);
	}
#endif
	me->last_anchor_before_stbl = me->last_anchor;
    } else {
	CTRACE((tfp, "startStblTABLE: failed.\n"));
    }
}

#ifdef EXP_NESTED_TABLES
static void free_enclosed_stbl(HText *me)
{
    if (me->enclosed_stbl != NULL) {
	HTList *list = me->enclosed_stbl;
	STable_info *stbl;

	while (NULL != (stbl = (STable_info *) HTList_nextObject(list))) {
	    CTRACE((tfp, "endStblTABLE: finally free %p\n", me->stbl));
	    Stbl_free(stbl);
	}
	HTList_delete(me->enclosed_stbl);
	me->enclosed_stbl = NULL;
    }
}

#else
#define free_enclosed_stbl(me)	/* nothing */
#endif

/*	Finish simple table handling
 *	Return TRUE if the table is nested inside another table.
 */
int HText_endStblTABLE(HText *me)
{
    int ncols, lines_changed = 0;
    STable_info *enclosing = NULL;

    if (!me || !me->stbl) {
	CTRACE((tfp, "endStblTABLE: ignored.\n"));
	free_enclosed_stbl(me);
	return FALSE;
    }
    CTRACE((tfp, "endStblTABLE: ok, will try.\n"));

    ncols = Stbl_finishTABLE(me->stbl);
    CTRACE((tfp, "endStblTABLE: ncols = %d.\n", ncols));

    if (ncols > 0) {
	lines_changed = HText_insertBlanksInStblLines(me, ncols);
	CTRACE((tfp, "endStblTABLE: changed %d lines, done.\n", lines_changed));
#ifdef DISP_PARTIAL
	/* allow HTDisplayPartial() to redisplay the changed lines.
	 * There is no harm if we got several stbl in the document, hope so.
	 */
	NumOfLines_partial -= lines_changed;	/* fake */
#endif /* DISP_PARTIAL */
    }
#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	enclosing = Stbl_get_enclosing(me->stbl);
	me->last_anchor_before_stbl = Stbl_get_last_anchor_before(me->stbl);
	if (enclosing == NULL) {
	    Stbl_free(me->stbl);
	    free_enclosed_stbl(me);
	} else {
	    if (me->enclosed_stbl == NULL)
		me->enclosed_stbl = HTList_new();
	    HTList_addObject(me->enclosed_stbl, me->stbl);
	    CTRACE((tfp, "endStblTABLE: postpone free %p\n", me->stbl));
	}
	me->stbl = enclosing;
    } else {
	Stbl_free(me->stbl);
	me->stbl = NULL;
    }
#else
    Stbl_free(me->stbl);
    me->stbl = NULL;
#endif

    CTRACE((tfp, "endStblTABLE: have%s enclosing table (%p)\n",
	    enclosing == 0 ? " NO" : "", enclosing));

    return enclosing != 0;
}

/*	Start simple table row
*/
void HText_startStblTR(HText *me, short alignment)
{
    if (!me || !me->stbl)
	return;
    if (Stbl_addRowToTable(me->stbl, alignment, me->Lines) < 0)
	HText_cancelStbl(me);	/* give up */
}

/*	Finish simple table row
*/
void HText_endStblTR(HText *me)
{
    if (!me || !me->stbl)
	return;
    /* should this do something?? */
}

/*	Start simple table cell
*/
void HText_startStblTD(HText *me, int colspan,
		       int rowspan,
		       short alignment,
		       BOOL isheader)
{
    if (!me || !me->stbl)
	return;
    if (colspan < 0)
	colspan = 1;
    if (colspan > TRST_MAXCOLSPAN) {
	CTRACE((tfp, "*** COLSPAN=%d is too large, ignored!\n", colspan));
	colspan = 1;
    }
    if (rowspan > TRST_MAXROWSPAN) {
	CTRACE((tfp, "*** ROWSPAN=%d is too large, ignored!\n", rowspan));
	rowspan = 1;
    }
    if (Stbl_addCellToTable(me->stbl, colspan, rowspan, alignment, isheader,
			    me->Lines,
			    HText_LastLineOffset(me),
			    HText_LastLineSize(me, FALSE)) < 0)
	HText_cancelStbl(me);	/* give up */
}

/*	Finish simple table cell
*/
void HText_endStblTD(HText *me)
{
    if (!me || !me->stbl)
	return;
    if (Stbl_finishCellInTable(me->stbl, TRST_ENDCELL_ENDTD,
			       me->Lines,
			       HText_LastLineOffset(me),
			       HText_LastLineSize(me, FALSE)) < 0)
	HText_cancelStbl(me);	/* give up */
}

/*	Remember COL info / Start a COLGROUP and remember info
*/
void HText_startStblCOL(HText *me, int span,
			short alignment,
			BOOL isgroup)
{
    if (!me || !me->stbl)
	return;
    if (span <= 0)
	span = 1;
    if (span > TRST_MAXCOLSPAN) {
	CTRACE((tfp, "*** SPAN=%d is too large, ignored!\n", span));
	span = 1;
    }
    if (Stbl_addColInfo(me->stbl, span, alignment, isgroup) < 0)
	HText_cancelStbl(me);	/* give up */
}

/*	Finish a COLGROUP
*/
void HText_endStblCOLGROUP(HText *me)
{
    if (!me || !me->stbl)
	return;
    if (Stbl_finishColGroup(me->stbl) < 0)
	HText_cancelStbl(me);	/* give up */
}

/*	Start a THEAD / TFOOT / TBODY - remember its alignment info
*/
void HText_startStblRowGroup(HText *me, short alignment)
{
    if (!me || !me->stbl)
	return;
    if (Stbl_addRowGroup(me->stbl, alignment) < 0)
	HText_cancelStbl(me);	/* give up */
}

/*		Anchor handling
 *		---------------
 */
static void add_link_number(HText *text, TextAnchor *a, BOOL save_position)
{
    char marker[32];

    /*
     * If we are doing link_numbering add the link number.
     */
    if ((a->number > 0)
#ifdef USE_PRETTYSRC
	&& (text->source ? !psrcview_no_anchor_numbering : 1)
#endif
	&& links_are_numbered()) {
	char saved_lastchar = text->LastChar;
	int saved_linenum = text->Lines;

	sprintf(marker, "[%d]", a->number);
	HText_appendText(text, marker);
	if (saved_linenum && text->Lines && saved_lastchar != ' ')
	    text->LastChar = ']';	/* if marker not after space caused split */
	if (save_position) {
	    a->line_num = text->Lines;
	    a->line_pos = text->last_line->size;
	}
    }
}

/*	Start an anchor field
*/
int HText_beginAnchor(HText *text, BOOL underline,
		      HTChildAnchor *anc)
{
    TextAnchor *a;

    POOLtypecalloc(TextAnchor, a);

    if (a == NULL)
	outofmem(__FILE__, "HText_beginAnchor");
    a->inUnderline = underline;

    a->sgml_offset = SGML_offset();
    a->line_num = text->Lines;
    a->line_pos = text->last_line->size;
    if (text->last_anchor) {
	text->last_anchor->next = a;
    } else {
	text->first_anchor = a;
    }
    a->next = 0;
    a->anchor = anc;
    a->extent = 0;
    a->link_type = HYPERTEXT_ANCHOR;
    text->last_anchor = a;

#ifndef DONT_TRACK_INTERNAL_LINKS
    if (HTAnchor_followTypedLink(anc, HTInternalLink)) {
	a->number = ++(text->last_anchor_number);
	a->link_type = INTERNAL_LINK_ANCHOR;
    } else
#endif
    if (HTAnchor_followLink(anc)) {
	a->number = ++(text->last_anchor_number);
    } else {
	a->number = 0;
    }

    if (number_links_on_left)
	add_link_number(text, a, TRUE);
    return (a->number);
}

/* If !really, report whether the anchor is empty. */
static BOOL HText_endAnchor0(HText *text, int number,
			     int really)
{
    TextAnchor *a;

    /*
     * The number argument is set to 0 in HTML.c and
     * LYCharUtils.c when we want to end the anchor
     * for the immediately preceding HText_beginAnchor()
     * call.  If it's greater than 0, we want to handle
     * a particular anchor.  This allows us to set links
     * for positions indicated by NAME or ID attributes,
     * without needing to close any anchor with an HREF
     * within which that link might be embedded.  -FM
     */
    if (number <= 0 || number == text->last_anchor->number) {
	a = text->last_anchor;
    } else {
	for (a = text->first_anchor; a; a = a->next) {
	    if (a->number == number) {
		break;
	    }
	}
	if (a == NULL) {
	    /*
	     * There's no anchor with that number,
	     * so we'll default to the last anchor,
	     * and cross our fingers.  -FM
	     */
	    a = text->last_anchor;
	}
    }

    CTRACE((tfp, "GridText:HText_endAnchor0: number:%d link_type:%d\n",
	    a->number, a->link_type));
    if (a->link_type == INPUT_ANCHOR) {
	/*
	 * Shouldn't happen, but put test here anyway to be safe.  - LE
	 */

	CTRACE((tfp,
		"BUG: HText_endAnchor0: internal error: last anchor was input field!\n"));
	return FALSE;
    }

    if (a->number) {
	/*
	 * If it goes somewhere...
	 */
	int i, j, k, l;
	BOOL remove_numbers_on_empty = (BOOL) ((links_are_numbered() &&
						((text->hiddenlinkflag != HIDDENLINKS_MERGE)
						 || (LYNoISMAPifUSEMAP &&
						     !(text->node_anchor && text->node_anchor->bookmark)
						     && HTAnchor_isISMAPScript
						     (HTAnchor_followLink(a->anchor))))));
	HTLine *last = text->last_line;
	HTLine *prev = text->last_line->prev;
	HTLine *start = last;
	int CurBlankExtent = 0;
	int BlankExtent = 0;
	int extent_adjust = 0;

	/* Find the length taken by the anchor */
	l = text->Lines;	/* lineno of last */

	/* the last line of an anchor may contain a trailing blank,
	 * which will be trimmed later.  Discount it from the extent.
	 */
	if (l > a->line_num) {
	    for (i = start->size; i > 0; --i) {
		if (isspace(UCH(start->data[i - 1]))) {
		    --extent_adjust;
		} else {
		    break;
		}
	    }
	}

	while (l > a->line_num) {
	    extent_adjust += start->size;
	    start = start->prev;
	    l--;
	}
	/* Now start is the start line of the anchor */
	extent_adjust += start->size - a->line_pos;
	start = last;		/* Used later */

	/*
	 * Check if the anchor content has only
	 * white and special characters, starting
	 * with the content on the last line.  -FM
	 */
	a->extent += extent_adjust;
	if (a->extent > (int) last->size) {
	    /*
	     * The anchor extends over more than one line,
	     * so set up to check the entire last line.  -FM
	     */
	    i = last->size;
	} else {
	    /*
	     * The anchor is restricted to the last line,
	     * so check from the start of the anchor.  -FM
	     */
	    i = a->extent;
	}
	k = j = (last->size - i);
	while (j < (int) last->size) {
	    if (!IsSpecialAttrChar(last->data[j]) &&
		!isspace(UCH(last->data[j])) &&
		last->data[j] != HT_NON_BREAK_SPACE &&
		last->data[j] != HT_EN_SPACE)
		break;
	    i--;
	    j++;
	}
	if (i == 0) {
	    if (a->extent > (int) last->size) {
		/*
		 * The anchor starts on a preceding line, and
		 * the last line has only white and special
		 * characters, so declare the entire extent
		 * of the last line as blank.  -FM
		 */
		CurBlankExtent = BlankExtent = last->size;
	    } else {
		/*
		 * The anchor starts on the last line, and
		 * has only white or special characters, so
		 * declare the anchor's extent as blank.  -FM
		 */
		CurBlankExtent = BlankExtent = a->extent;
	    }
	}
	/*
	 * While the anchor starts on a line preceding
	 * the one we just checked, and the one we just
	 * checked has only white and special characters,
	 * check whether the anchor's content on the
	 * immediately preceding line also has only
	 * white and special characters.  -FM
	 */
	while (i == 0 &&
	       (a->extent > CurBlankExtent ||
		(a->extent == CurBlankExtent &&
		 k == 0 &&
		 prev != text->last_line &&
		 (prev->size == 0 ||
		  prev->data[prev->size - 1] == ']')))) {
	    start = prev;
	    k = j = prev->size - a->extent + CurBlankExtent;
	    if (j < 0) {
		/*
		 * The anchor starts on a preceding line,
		 * so check all of this line.  -FM
		 */
		j = 0;
		i = prev->size;
	    } else {
		/*
		 * The anchor starts on this line.  -FM
		 */
		i = a->extent - CurBlankExtent;
	    }
	    while (j < (int) prev->size) {
		if (!IsSpecialAttrChar(prev->data[j]) &&
		    !isspace(UCH(prev->data[j])) &&
		    prev->data[j] != HT_NON_BREAK_SPACE &&
		    prev->data[j] != HT_EN_SPACE)
		    break;
		i--;
		j++;
	    }
	    if (i == 0) {
		if (a->extent > (CurBlankExtent + (int) prev->size) ||
		    (a->extent == CurBlankExtent + (int) prev->size &&
		     k == 0 &&
		     prev->prev != text->last_line &&
		     (prev->prev->size == 0 ||
		      prev->prev->data[prev->prev->size - 1] == ']'))) {
		    /*
		     * This line has only white and special
		     * characters, so treat its entire extent
		     * as blank, and decrement the pointer for
		     * the line to be analyzed.  -FM
		     */
		    CurBlankExtent += prev->size;
		    BlankExtent = CurBlankExtent;
		    prev = prev->prev;
		} else {
		    /*
		     * The anchor starts on this line, and it
		     * has only white or special characters, so
		     * declare the anchor's extent as blank.  -FM
		     */
		    BlankExtent = a->extent;
		    break;
		}
	    }
	}
	if (!really) {		/* Just report whether it is empty */
	    a->extent -= extent_adjust;
	    return (BOOL) (i == 0);
	}
	if (i == 0) {
	    /*
	     * It's an invisible anchor probably from an ALT=""
	     * or an ignored ISMAP attribute due to a companion
	     * USEMAP.  -FM
	     */
	    a->show_anchor = NO;

	    CTRACE((tfp,
		    "HText_endAnchor0: hidden (line,pos,ext,BlankExtent):(%d,%d,%d,%d)",
		    a->line_num, a->line_pos, a->extent,
		    BlankExtent));

	    /*
	     * If links are numbered, then try to get rid of the
	     * numbered bracket and adjust the anchor count.  -FM
	     *
	     * Well, let's do this only if -hiddenlinks=merged is not in
	     * effect, or if we can be reasonably sure that
	     * this is the result of an intentional non-generation of
	     * anchor text via NO_ISMAP_IF_USEMAP.  In other cases it can
	     * actually be a feature that numbered links alert the viewer
	     * to the presence of a link which is otherwise not selectable -
	     * possibly caused by HTML errors. - kw
	     */
	    if (remove_numbers_on_empty) {
		int NumSize = 0;
		TextAnchor *anc;

		/*
		 * Set start->data[j] to the close-square-bracket,
		 * or to the beginning of the line on which the
		 * anchor start.  -FM
		 */
		if (start == last) {
		    /*
		     * The anchor starts on the last line.  -FM
		     */
		    j = (last->size - a->extent - 1);
		} else {
		    /*
		     * The anchor starts on a previous line.  -FM
		     */
		    prev = start->prev;
		    j = (start->size - a->extent + CurBlankExtent - 1);
		}
		if (j < 0)
		    j = 0;
		i = j;

		/*
		 * If start->data[j] is a close-square-bracket, verify
		 * that it's the end of the numbered bracket, and if so,
		 * strip the numbered bracket.  If start->data[j] is not
		 * a close-square-bracket, check whether we had a wrap
		 * and the close-square-bracket is at the end of the
		 * previous line.  If so, strip the numbered bracket
		 * from that line.  -FM
		 */
		if (start->data[j] == ']') {
		    j--;
		    NumSize++;
		    while (j >= 0 && isdigit(UCH(start->data[j]))) {
			j--;
			NumSize++;
		    }
		    while (j < 0) {
			j++;
			NumSize--;
		    }
		    if (start->data[j] == '[') {
			/*
			 * The numbered bracket is entirely
			 * on this line.  -FM
			 */
			NumSize++;
			if (start == last && (int) text->permissible_split > j) {
			    if ((int) text->permissible_split - NumSize < j)
				text->permissible_split = j;
			    else
				text->permissible_split -= NumSize;
			}
			k = j + NumSize;
			while (k < (int) start->size)
			    start->data[j++] = start->data[k++];
			for (anc = a; anc; anc = anc->next) {
			    if (anc->line_num == a->line_num &&
				anc->line_pos >= NumSize) {
				anc->line_pos -= NumSize;
			    }
			}
			start->size = j;
			start->data[j++] = '\0';
			while (j < k)
			    start->data[j++] = '\0';
		    } else if (prev && prev->size > 1) {
			k = (i + 1);
			j = (prev->size - 1);
			while ((j >= 0) && IsSpecialAttrChar(prev->data[j]))
			    j--;
			i = (j + 1);
			while (j >= 0 &&
			       isdigit(UCH(prev->data[j]))) {
			    j--;
			    NumSize++;
			}
			while (j < 0) {
			    j++;
			    NumSize--;
			}
			if (prev->data[j] == '[') {
			    /*
			     * The numbered bracket started on the
			     * previous line, and part of it was
			     * wrapped to this line.  -FM
			     */
			    NumSize++;
			    l = (i - j);
			    while (i < (int) prev->size)
				prev->data[j++] = prev->data[i++];
			    prev->size = j;
			    prev->data[j] = '\0';
			    while (j < i)
				prev->data[j++] = '\0';
			    if (start == last && text->permissible_split > 0) {
				if ((int) text->permissible_split < k)
				    text->permissible_split = 0;
				else
				    text->permissible_split -= k;
			    }
			    j = 0;
			    i = k;
			    while (k < (int) start->size)
				start->data[j++] = start->data[k++];
			    for (anc = a; anc; anc = anc->next) {
				if (anc->line_num == a->line_num &&
				    anc->line_pos >= i) {
				    anc->line_pos -= i;
				}
			    }
			    start->size = j;
			    start->data[j++] = '\0';
			    while (j < k)
				start->data[j++] = '\0';
			} else {
			    /*
			     * Shucks!  We didn't find the
			     * numbered bracket.  -FM
			     */
			    a->show_anchor = YES;
			}
		    } else {
			/*
			 * Shucks!  We didn't find the
			 * numbered bracket.  -FM
			 */
			a->show_anchor = YES;
		    }
		} else if (prev && prev->size > 2) {
		    j = (prev->size - 1);
		    while ((j >= 0) && IsSpecialAttrChar(prev->data[j]))
			j--;
		    if (j < 0)
			j = 0;
		    i = (j + 1);
		    if ((j >= 2) &&
			(prev->data[j] == ']' &&
			 isdigit(UCH(prev->data[j - 1])))) {
			j--;
			NumSize++;
			k = (j + 1);
			while (j >= 0 &&
			       isdigit(UCH(prev->data[j]))) {
			    j--;
			    NumSize++;
			}
			while (j < 0) {
			    j++;
			    NumSize--;
			}
			if (prev->data[j] == '[') {
			    /*
			     * The numbered bracket is all on the
			     * previous line, and the anchor content
			     * was wrapped to the last line.  -FM
			     */
			    NumSize++;
			    k = j + NumSize;
			    while (k < (int) prev->size)
				prev->data[j++] = prev->data[k++];
			    prev->size = j;
			    prev->data[j++] = '\0';
			    while (j < k)
				prev->data[j++] = '\0';
			} else {
			    /*
			     * Shucks!  We didn't find the
			     * numbered bracket.  -FM
			     */
			    a->show_anchor = YES;
			}
		    } else {
			/*
			 * Shucks!  We didn't find the
			 * numbered bracket.  -FM
			 */
			a->show_anchor = YES;
		    }
		} else {
		    /*
		     * Shucks!  We didn't find the
		     * numbered bracket.  -FM
		     */
		    a->show_anchor = YES;
		}
	    }
	} else {
	    if (!number_links_on_left)
		add_link_number(text, a, FALSE);
	    /*
	     * The anchor's content is not restricted to only
	     * white and special characters, so we'll show it
	     * as a link.  -FM
	     */
	    a->show_anchor = YES;
	    if (BlankExtent) {
		CTRACE((tfp,
			"HText_endAnchor0: blanks (line,pos,ext,BlankExtent):(%d,%d,%d,%d)",
			a->line_num, a->line_pos, a->extent,
			BlankExtent));
	    }
	}
	if (a->show_anchor == NO) {
	    /*
	     * The anchor's content is restricted to white
	     * and special characters, so set its number
	     * and extent to zero, decrement the visible
	     * anchor number counter, and add this anchor
	     * to the hidden links list.  -FM
	     */
	    a->extent = 0;
	    if (text->hiddenlinkflag != HIDDENLINKS_MERGE) {
		a->number = 0;
		text->last_anchor_number--;
		HText_AddHiddenLink(text, a);
	    }
	} else {
	    /*
	     * The anchor's content is not restricted to white
	     * and special characters, so we'll display the
	     * content, but shorten its extent by any trailing
	     * blank lines we've detected.  -FM
	     */
	    a->extent -= ((BlankExtent < a->extent) ?
			  BlankExtent : 0);
	}
	if (BlankExtent || a->extent <= 0 || a->number <= 0) {
	    CTRACE((tfp,
		    "->[%d](%d,%d,%d,%d)\n",
		    a->number,
		    a->line_num, a->line_pos, a->extent,
		    BlankExtent));
	}
    } else {
	if (!really)		/* Just report whether it is empty */
	    return FALSE;
	/*
	 * It's a named anchor without an HREF, so it
	 * should be registered but not shown as a
	 * link.  -FM
	 */
	a->show_anchor = NO;
	a->extent = 0;
    }
    return FALSE;
}

void HText_endAnchor(HText *text, int number)
{
    HText_endAnchor0(text, number, 1);
}

/*
    This returns whether the given anchor has blank content. Shamelessly copied
    from HText_endAnchor. The values returned are meaningful only for "normal"
    links - like ones produced by <a href=".">foo</a>, no inputs, etc. - VH
*/
#ifdef MARK_HIDDEN_LINKS
BOOL HText_isAnchorBlank(HText *text, int number)
{
    return HText_endAnchor0(text, number, 0);
}
#endif /* MARK_HIDDEN_LINKS */

void HText_appendText(HText *text, const char *str)
{
    const char *p;

    if (str == NULL)
	return;

    if (text->halted == 3)
	return;

    for (p = str; *p; p++) {
	HText_appendCharacter(text, *p);
    }
}

static int remove_special_attr_chars(char *buf)
{
    register char *cp;
    register int soft_newline_count = 0;

    for (cp = buf; *cp != '\0'; cp++) {
	/*
	 * Don't print underline chars.
	 */
	soft_newline_count += (*cp == LY_SOFT_NEWLINE);
	if (!IsSpecialAttrChar(*cp)) {
	    *buf++ = *cp;
	}
    }
    *buf = '\0';
    return soft_newline_count;
}

/*
 *  This function trims blank lines from the end of the document, and
 *  then gets the hightext from the text by finding the char position,
 *  and brings the anchors in line with the text by adding the text
 *  offset to each of the anchors.
 */
void HText_endAppend(HText *text)
{
    HTLine *line_ptr;

    if (!text)
	return;

    CTRACE((tfp, "GridText: Entering HText_endAppend\n"));

    /*
     * Create a blank line at the bottom.
     */
    new_line(text);

    if (text->halted) {
	if (text->stbl)
	    HText_cancelStbl(text);
	/*
	 * If output was stopped because memory was low, and we made
	 * it to the end of the document, reset those flags and hope
	 * things are better now.  - kw
	 */
	LYFakeZap(NO);
	text->halted = 0;
    } else if (text->stbl) {
	/*
	 * Could happen if TABLE end tag was missing.
	 * Alternatively we could cancel in this case.  - kw
	 */
	HText_endStblTABLE(text);
    }

    /*
     * Get the first line.
     */
    line_ptr = FirstHTLine(text);

    /*
     * Remove the blank lines at the end of document.
     */
    while (text->last_line->data[0] == '\0' && text->Lines > 2) {
	HTLine *next_to_the_last_line = text->last_line->prev;

	CTRACE((tfp, "GridText: Removing bottom blank line: `%s'\n",
		text->last_line->data));
	/*
	 * line_ptr points to the first line.
	 */
	next_to_the_last_line->next = line_ptr;
	line_ptr->prev = next_to_the_last_line;
	freeHTLine(text, text->last_line);
	text->last_line = next_to_the_last_line;
	text->Lines--;
	CTRACE((tfp, "GridText: New bottom line: `%s'\n",
		text->last_line->data));
    }

    /*
     * Fix up the anchor structure values and
     * create the hightext strings.  -FM
     */
    HText_trimHightext(text, TRUE, -1);
}

/*
 *  This function gets the hightext from the text by finding the char
 *  position, and brings the anchors in line with the text by adding the text
 *  offset to each of the anchors.
 *
 *  `Forms input' fields cannot be displayed properly without this function
 *  to be invoked (detected in display_partial mode).
 *
 *  If final is set, this is the final fixup; if not set, we don't have
 *  to do everything because there should be another call later.
 *
 *  BEFORE this function has treated a TextAnchor, its line_pos and
 *  extent fields are counting bytes in the HTLine data, including
 *  invisible special attribute chars and counting UTF-8 multibyte
 *  characters as multiple bytes.
 *
 *  AFTER the adjustment, the anchor line_pos (and hightext offset if
 *  applicable) fields indicate x positions in terms of displayed character
 *  cells, and the extent field apparently is unimportant; the anchor text has
 *  been copied to the hightext fields (which should have been NULL up to that
 *  point), with special attribute chars removed.
 *
 *  This needs to be done so that display_page finds the anchors in the
 *  form it expects when it sets the links[] elements.
 */
static void HText_trimHightext(HText *text,
			       BOOLEAN final,
			       int stop_before)
{
    int cur_line, cur_shift;
    TextAnchor *anchor_ptr;
    TextAnchor *prev_a = NULL;
    HTLine *line_ptr;
    HTLine *line_ptr2;
    unsigned char ch;
    char *hilite_str;
    int hilite_len;
    int actual_len;
    int count_line;

    if (!text)
	return;

    if (final) {
	CTRACE((tfp, "GridText: Entering HText_trimHightext (final)\n"));
    } else {
	if (stop_before < 0 || stop_before > text->Lines)
	    stop_before = text->Lines;
	CTRACE((tfp,
		"GridText: Entering HText_trimHightext (partial: 0..%d/%d)\n",
		stop_before, text->Lines));
    }

    /*
     * Get the first line.
     */
    line_ptr = FirstHTLine(text);
    cur_line = 0;

    /*
     * Fix up the anchor structure values and
     * create the hightext strings.  -FM
     */
    for (anchor_ptr = text->first_anchor;
	 anchor_ptr != NULL;
	 prev_a = anchor_ptr, anchor_ptr = anchor_ptr->next) {
	int anchor_col;

      re_parse:
	/*
	 * Find the right line.
	 */
	for (; anchor_ptr->line_num > cur_line;
	     line_ptr = line_ptr->next, cur_line++) {
	    ;			/* null body */
	}

	if (!final) {
	    /*
	     * If this is not the final call, stop when we have reached
	     * the last line, or the very end of preceding line.
	     * The last line is probably still not finished.  - kw
	     */
	    if (cur_line >= stop_before)
		break;
	    if (anchor_ptr->line_num >= text->Lines - 1
		&& anchor_ptr->line_pos >= (int) text->last_line->prev->size)
		break;
	    /*
	     * Also skip this anchor if it looks like HText_endAnchor
	     * is not yet done with it.  - kw
	     */
	    if (!anchor_ptr->extent && anchor_ptr->number &&
		(anchor_ptr->link_type & HYPERTEXT_ANCHOR) &&
		!anchor_ptr->show_anchor &&
		anchor_ptr->number == text->last_anchor_number)
		continue;
	}

	/*
	 * If hightext has already been set, then we must have already
	 * done the trimming & adjusting for this anchor, so avoid
	 * doing it a second time.  - kw
	 */
	if (LYGetHiTextStr(anchor_ptr, 0) != NULL)
	    continue;

	if (anchor_ptr->line_pos > (int) line_ptr->size) {
	    anchor_ptr->line_pos = line_ptr->size;
	}
	if (anchor_ptr->line_pos < 0) {
	    anchor_ptr->line_pos = 0;
	    anchor_ptr->line_num = cur_line;
	}
	CTRACE((tfp,
		"GridText: Anchor found on line:%d col:%d [%05d:%d] ext:%d\n",
		cur_line,
		anchor_ptr->line_pos,
		anchor_ptr->sgml_offset,
		anchor_ptr->number,
		anchor_ptr->extent));

	cur_shift = 0;
	/*
	 * Strip off any spaces or SpecialAttrChars at the beginning,
	 * if they exist, but only on HYPERTEXT_ANCHORS.
	 */
	if (anchor_ptr->link_type & HYPERTEXT_ANCHOR) {
	    ch = UCH(line_ptr->data[anchor_ptr->line_pos]);
	    while (isspace(ch) ||
		   IsSpecialAttrChar(ch)) {
		anchor_ptr->line_pos++;
		anchor_ptr->extent--;
		cur_shift++;
		ch = UCH(line_ptr->data[anchor_ptr->line_pos]);
	    }
	}
	if (anchor_ptr->extent < 0) {
	    anchor_ptr->extent = 0;
	}

	CTRACE((tfp, "anchor text: '%s'\n", line_ptr->data));
	/*
	 * If the link begins with an end of line and we have more lines, then
	 * start the highlighting on the next line.  -FM.
	 *
	 * But if an empty anchor is at the end of line and empty, keep it
	 * where it is, unless the previous anchor in the list (if any) already
	 * starts later.  - kw
	 */
	if ((unsigned) anchor_ptr->line_pos >= strlen(line_ptr->data)) {
	    if (cur_line < text->Lines &&
		(anchor_ptr->extent ||
		 anchor_ptr->line_pos != (int) line_ptr->size ||
		 (prev_a &&	/* How could this happen? */
		  (prev_a->line_num > anchor_ptr->line_num)))) {
		anchor_ptr->line_num++;
		anchor_ptr->line_pos = 0;
		CTRACE((tfp, "found anchor at end of line\n"));
		goto re_parse;
	    } else {
		CTRACE((tfp, "found anchor at end of line, leaving it there\n"));
	    }
	}

	/*
	 * Copy the link name into the data structure.
	 */
	if (line_ptr->data
	    && anchor_ptr->extent > 0
	    && anchor_ptr->line_pos >= 0) {
	    int size = (int) line_ptr->size - anchor_ptr->line_pos;

	    if (size > anchor_ptr->extent)
		size = anchor_ptr->extent;
	    LYClearHiText(anchor_ptr);
	    LYSetHiText(anchor_ptr,
			&line_ptr->data[anchor_ptr->line_pos],
			size);
	} else {
	    LYClearHiText(anchor_ptr);
	    LYSetHiText(anchor_ptr, "", 0);
	}

	/*
	 * If the anchor extends over more than one line, copy that into the
	 * data structure.
	 */
	hilite_str = LYGetHiTextStr(anchor_ptr, 0);
	hilite_len = strlen(hilite_str);
	actual_len = anchor_ptr->extent;

	line_ptr2 = line_ptr;
	count_line = cur_line;
	while (actual_len > hilite_len) {
	    HTLine *old_line_ptr2 = line_ptr2;

	    count_line++;
	    line_ptr2 = line_ptr2->next;

	    if (!final
		&& count_line >= stop_before) {
		LYClearHiText(anchor_ptr);
		break;
	    } else if (old_line_ptr2 == text->last_line) {
		break;
	    }

	    /*
	     * Double check that we have a line pointer, and if so, copy into
	     * highlight text.
	     */
	    if (line_ptr2) {
		char *hi_string = NULL;
		int hi_offset = line_ptr2->offset;

		StrnAllocCopy(hi_string,
			      line_ptr2->data,
			      (actual_len - hilite_len));
		actual_len -= strlen(hi_string);
		/*handle LY_SOFT_NEWLINEs -VH */
		hi_offset += remove_special_attr_chars(hi_string);

		if (anchor_ptr->link_type & HYPERTEXT_ANCHOR) {
		    LYTrimTrailing(hi_string);
		}
		if (non_empty(hi_string)) {
		    LYAddHiText(anchor_ptr, hi_string, hi_offset);
		} else if (actual_len > hilite_len) {
		    LYAddHiText(anchor_ptr, "", hi_offset);
		}
		FREE(hi_string);
	    }
	}

	if (!final
	    && count_line >= stop_before) {
	    break;
	}

	hilite_str = LYGetHiTextStr(anchor_ptr, 0);
	remove_special_attr_chars(hilite_str);
	if (anchor_ptr->link_type & HYPERTEXT_ANCHOR) {
	    LYTrimTrailing(hilite_str);
	}

	/*
	 * Save the offset (bytes) of the anchor in the line's data.
	 */
	anchor_col = anchor_ptr->line_pos;

	/*
	 * Subtract any formatting characters from the x position of the link.
	 */
#ifdef WIDEC_CURSES
	if (anchor_ptr->line_pos > 0) {
	    /*
	     * LYstrExtent filters out the formatting characters, so we do not
	     * have to count them here, except for soft newlines.
	     */
	    anchor_ptr->line_pos = LYstrExtent2(line_ptr->data, anchor_col);
	    if (line_ptr->data[0] == LY_SOFT_NEWLINE)
		anchor_ptr->line_pos += 1;
	}
#else /* 8-bit curses, etc.  */
	if (anchor_ptr->line_pos > 0) {
	    register int offset = 0, i = 0;
	    int have_soft_newline_in_1st_line = 0;

	    for (; i < anchor_col; i++) {
		if (IS_UTF_EXTRA(line_ptr->data[i]) ||
		    IsSpecialAttrChar(line_ptr->data[i])) {
		    offset++;
		    have_soft_newline_in_1st_line += (line_ptr->data[i] == LY_SOFT_NEWLINE);
		}
	    }
	    anchor_ptr->line_pos -= offset;
	    /*handle LY_SOFT_NEWLINEs -VH */
	    anchor_ptr->line_pos += have_soft_newline_in_1st_line;
	}
#endif /* WIDEC_CURSES */

	/*
	 * Set the line number.
	 */
	anchor_ptr->line_pos += line_ptr->offset;
	anchor_ptr->line_num = cur_line;

	CTRACE((tfp, "GridText:     add link on line %d col %d [%d] %s\n",
		cur_line, anchor_ptr->line_pos,
		anchor_ptr->number, "in HText_trimHightext"));
    }
}

/*	Return the anchor associated with this node
*/
HTParentAnchor *HText_nodeAnchor(HText *text)
{
    return text->node_anchor;
}

/*				GridText specials
 *				=================
 */

/*
 * HText_childNextNumber() returns the anchor with index [number],
 * using a pointer from the previous number (=optimization) or NULL.
 */
HTChildAnchor *HText_childNextNumber(int number, void **prev)
{
    /* Sorry, TextAnchor is not declared outside this file, use a cast. */
    TextAnchor *a = (TextAnchor *) *prev;

    if (!HTMainText || number <= 0)
	return (HTChildAnchor *) 0;	/* Fail */
    if (number == 1 || !a)
	a = HTMainText->first_anchor;

    /* a strange thing:  positive a->number's are sorted,
     * and between them several a->number's may be 0 -- skip them
     */
    for (; a && a->number != number; a = a->next) ;

    if (!a)
	return (HTChildAnchor *) 0;	/* Fail */
    *prev = (void *) a;
    return a->anchor;
}

/*
 * HText_FormDescNumber() returns a description of the form field
 * with index N.  The index corresponds to the [number] we print
 * for the field.  -FM & LE
 */
void HText_FormDescNumber(int number,
			  const char **desc)
{
    TextAnchor *a;

    if (!desc)
	return;

    if (!(HTMainText && HTMainText->first_anchor) || number <= 0) {
	*desc = gettext("unknown field or link");
	return;
    }

    for (a = HTMainText->first_anchor; a; a = a->next) {
	if (a->number == number) {
	    if (!(a->input_field && a->input_field->type)) {
		*desc = gettext("unknown field or link");
		return;
	    }
	    break;
	}
    }

    switch (a->input_field->type) {
    case F_TEXT_TYPE:
	*desc = gettext("text entry field");
	return;
    case F_PASSWORD_TYPE:
	*desc = gettext("password entry field");
	return;
    case F_CHECKBOX_TYPE:
	*desc = gettext("checkbox");
	return;
    case F_RADIO_TYPE:
	*desc = gettext("radio button");
	return;
    case F_SUBMIT_TYPE:
	*desc = gettext("submit button");
	return;
    case F_RESET_TYPE:
	*desc = gettext("reset button");
	return;
    case F_OPTION_LIST_TYPE:
	*desc = gettext("popup menu");
	return;
    case F_HIDDEN_TYPE:
	*desc = gettext("hidden form field");
	return;
    case F_TEXTAREA_TYPE:
	*desc = gettext("text entry area");
	return;
    case F_RANGE_TYPE:
	*desc = gettext("range entry field");
	return;
    case F_FILE_TYPE:
	*desc = gettext("file entry field");
	return;
    case F_TEXT_SUBMIT_TYPE:
	*desc = gettext("text-submit field");
	return;
    case F_IMAGE_SUBMIT_TYPE:
	*desc = gettext("image-submit button");
	return;
    case F_KEYGEN_TYPE:
	*desc = gettext("keygen field");
	return;
    default:
	*desc = gettext("unknown form field");
	return;
    }
}

/* HTGetRelLinkNum returns the anchor number to which follow_link_number()
 * is to jump (input was 123+ or 123- or 123+g or 123-g or 123 or 123g)
 * num is the number specified
 * rel is 0 or '+' or '-'
 * cur is the current link
 */
int HTGetRelLinkNum(int num,
		    int rel,
		    int cur)
{
    TextAnchor *a, *l = 0;
    int scrtop = HText_getTopOfScreen();	/*XXX +1? */
    int curline = links[cur].anchor_line_num;
    int curpos = links[cur].lx;
    int on_screen = (curline >= scrtop && curline < (scrtop + display_lines));

    /* curanchor may or may not be the "current link", depending whether it's
     * on the current screen
     */
    int curanchor = links[cur].anchor_number;

    CTRACE((tfp, "HTGetRelLinkNum(%d,%d,%d) -- HTMainText=%p\n",
	    num, rel, cur, HTMainText));
    CTRACE((tfp,
	    "  scrtop=%d, curline=%d, curanchor=%d, display_lines=%d, %s\n",
	    scrtop, curline, curanchor, display_lines,
	    on_screen ? "on_screen" : "0"));
    if (!HTMainText)
	return 0;
    if (rel == 0)
	return num;

    /* if cur numbered link is on current page, use it */
    if (on_screen && curanchor) {
	CTRACE((tfp, "curanchor=%d at line %d on screen\n", curanchor, curline));
	if (rel == '+')
	    return curanchor + num;
	else if (rel == '-')
	    return curanchor - num;
	else
	    return num;		/* shouldn't happen */
    }

    /* no current link on screen, or current link is not numbered
     * -- find previous closest numbered link
     */
    for (a = HTMainText->first_anchor; a; a = a->next) {
	CTRACE((tfp, "  a->line_num=%d, a->number=%d\n", a->line_num, a->number));
	if (a->line_num >= scrtop)
	    break;
	if (a->number == 0)
	    continue;
	l = a;
	curanchor = l->number;
    }
    CTRACE((tfp, "  a=%p, l=%p, curanchor=%d\n", a, l, curanchor));
    if (on_screen) {		/* on screen but not a numbered link */
	for (; a; a = a->next) {
	    if (a->number) {
		l = a;
		curanchor = l->number;
	    }
	    if (curline == a->line_num && curpos == a->line_pos)
		break;
	}
    }
    if (rel == '+') {
	return curanchor + num;
    } else if (rel == '-') {
	if (l)
	    return curanchor + 1 - num;
	else {
	    for (; a && a->number == 0; a = a->next) ;
	    return a ? a->number - num : 0;
	}
    } else
	return num;		/* shouldn't happen */
}

/*
 * HTGetLinkInfo returns some link info based on the number.
 *
 * If want_go is not 0, caller requests to know a line number for
 * the link indicated by number.  It will be returned in *go_line, and
 * *linknum will be set to an index into the links[] array, to use after
 * the line in *go_line has been made the new top screen line.
 * *hightext and *lname are unchanged.  - KW
 *
 * If want_go is 0 and the number doesn't represent an input field, info
 * on the link indicated by number is deposited in *hightext and *lname.
 */
int HTGetLinkInfo(int number,
		  int want_go,
		  int *go_line,
		  int *linknum,
		  char **hightext,
		  char **lname)
{
    TextAnchor *a;
    HTAnchor *link_dest;

#ifndef DONT_TRACK_INTERNAL_LINKS
    HTAnchor *link_dest_intl = NULL;
#endif
    int anchors_this_line = 0, anchors_this_screen = 0;
    int prev_anchor_line = -1, prev_prev_anchor_line = -1;

    if (!HTMainText)
	return (NO);

    for (a = HTMainText->first_anchor; a; a = a->next) {
	/*
	 * Count anchors, first on current line if there is more
	 * than one.  We have to count all links, including form
	 * field anchors and others with a->number == 0, because
	 * they are or will be included in the links[] array.
	 * The exceptions are hidden form fields and anchors with
	 * show_anchor not set, because they won't appear in links[]
	 * and don't count towards nlinks.  - KW
	 */
	if ((a->show_anchor) &&
	    !(a->link_type == INPUT_ANCHOR
	      && a->input_field->type == F_HIDDEN_TYPE)) {
	    if (a->line_num == prev_anchor_line) {
		anchors_this_line++;
	    } else {
		/*
		 * This anchor is on a different line than the previous one.
		 * Remember which was the line number of the previous anchor,
		 * for use in screen positioning later.  - KW
		 */
		anchors_this_line = 1;
		prev_prev_anchor_line = prev_anchor_line;
		prev_anchor_line = a->line_num;
	    }
	    if (a->line_num >= HTMainText->top_of_screen) {
		/*
		 * Count all anchors starting with the top line of the
		 * currently displayed screen.  Just keep on counting
		 * beyond this screen's bottom line - we'll know whether
		 * a found anchor is below the current screen by a check
		 * against nlinks later.  - KW
		 */
		anchors_this_screen++;
	    }
	}

	if (a->number == number) {
	    /*
	     * We found it.  Now process it, depending
	     * on what kind of info is requested.  - KW
	     */
	    if (want_go || a->link_type == INPUT_ANCHOR) {
		if (a->show_anchor == NO) {
		    /*
		     * The number requested has been assigned to an anchor
		     * without any selectable text, so we cannot position
		     * on it.  The code for suppressing such anchors in
		     * HText_endAnchor() may not have applied, or it may
		     * have failed.  Return a failure indication so that
		     * the user will notice that something is wrong,
		     * instead of positioning on some other anchor which
		     * might result in inadvertent activation.  - KW
		     */
		    return (NO);
		}
		if (anchors_this_screen > 0 &&
		    anchors_this_screen <= nlinks &&
		    a->line_num >= HTMainText->top_of_screen &&
		    a->line_num < HTMainText->top_of_screen + (display_lines)) {
		    /*
		     * If the requested anchor is within the current screen,
		     * just set *go_line so that the screen window won't move
		     * (keep it as it is), and set *linknum to the index of
		     * this link in the current links[] array.  - KW
		     */
		    *go_line = HTMainText->top_of_screen;
		    if (linknum)
			*linknum = anchors_this_screen - 1;
		} else {
		    /*
		     * if the requested anchor is not within the currently
		     * displayed screen, set *go_line such that the top line
		     * will be either
		     *  (1) the line immediately below the previous
		     *      anchor, or
		     *  (2) about one third of a screenful above the line
		     *      with the target, or
		     *  (3) the first line of the document -
		     * whichever comes last.  In all cases the line with our
		     * target will end up being the first line with any links
		     * on the new screen, so that we can use the
		     * anchors_this_line counter to point to the anchor in
		     * the new links[] array.  - kw
		     */
		    int max_offset = SEARCH_GOAL_LINE - 1;

		    if (max_offset < 0)
			max_offset = 0;
		    else if (max_offset >= display_lines)
			max_offset = display_lines - 1;
		    *go_line = prev_anchor_line - max_offset;
		    if (*go_line <= prev_prev_anchor_line)
			*go_line = prev_prev_anchor_line + 1;
		    if (*go_line < 0)
			*go_line = 0;
		    if (linknum)
			*linknum = anchors_this_line - 1;
		}
		return (LINK_LINE_FOUND);
	    } else {
		*hightext = LYGetHiTextStr(a, 0);
		link_dest = HTAnchor_followLink(a->anchor);
		{
		    char *cp_freeme = NULL;

		    if (traversal) {
			cp_freeme = stub_HTAnchor_address(link_dest);
		    } else {
#ifndef DONT_TRACK_INTERNAL_LINKS
			if (a->link_type == INTERNAL_LINK_ANCHOR) {
			    link_dest_intl =
				HTAnchor_followTypedLink(a->anchor, HTInternalLink);
			    if (link_dest_intl && link_dest_intl != link_dest) {

				CTRACE((tfp,
					"HTGetLinkInfo: unexpected typed link to %s!\n",
					link_dest_intl->parent->address));
				link_dest_intl = NULL;
			    }
			}
			if (link_dest_intl) {
			    char *cp2 = HTAnchor_address(link_dest_intl);

			    FREE(*lname);
			    *lname = cp2;
			    return (WWW_INTERN_LINK_TYPE);
			} else
#endif
			    cp_freeme = HTAnchor_address(link_dest);
		    }
		    StrAllocCopy(*lname, cp_freeme);
		    FREE(cp_freeme);
		}
		return (WWW_LINK_TYPE);
	    }
	}
    }
    return (NO);
}

static BOOLEAN same_anchor_or_field(int numberA,
				    FormInfo * formA,
				    int numberB,
				    FormInfo * formB,
				    BOOLEAN ta_same)
{
    if (numberA > 0 || numberB > 0) {
	if (numberA == numberB)
	    return (YES);
	else if (!ta_same)
	    return (NO);
    }
    if (formA || formB) {
	if (formA == formB) {
	    return (YES);
	} else if (!ta_same) {
	    return (NO);
	} else if (!(formA && formB)) {
	    return (NO);
	}
    } else {
	return (NO);
    }
    if (formA->type != formB->type ||
	formA->type != F_TEXTAREA_TYPE ||
	formB->type != F_TEXTAREA_TYPE) {
	return (NO);
    }
    if (formA->number != formB->number)
	return (NO);
    if (!formA->name || !formB->name)
	return (YES);
    return (BOOL) (strcmp(formA->name, formB->name) == 0);
}

#define same_anchor_as_link(i,a,ta_same) (i >= 0 && a &&\
		same_anchor_or_field(links[i].anchor_number,\
		(links[i].type == WWW_FORM_LINK_TYPE) ? links[i].l_form : NULL,\
		a->number,\
		(a->link_type == INPUT_ANCHOR) ? a->input_field : NULL,\
		ta_same))
#define same_anchors(a1,a2,ta_same) (a1 && a2 &&\
		same_anchor_or_field(a1->number,\
		(a1->link_type == INPUT_ANCHOR) ? a1->input_field : NULL,\
		a2->number,\
		(a2->link_type == INPUT_ANCHOR) ? a2->input_field : NULL,\
		ta_same))

/*
 * Are there more textarea lines belonging to the same textarea before
 * (direction < 0) or after (direction > 0) the current one?
 * On entry, curlink must be the index in links[] of a textarea field.  - kw
 */
BOOL HText_TAHasMoreLines(int curlink,
			  int direction)
{
    TextAnchor *a;
    TextAnchor *prev_a = NULL;

    if (!HTMainText)
	return (NO);
    if (direction < 0) {
	for (a = HTMainText->first_anchor; a; prev_a = a, a = a->next) {
	    if (a->link_type == INPUT_ANCHOR &&
		links[curlink].l_form == a->input_field) {
		return same_anchors(a, prev_a, TRUE);
	    }
	    if (links[curlink].anchor_number &&
		a->number >= links[curlink].anchor_number)
		break;
	}
	return NO;
    } else {
	for (a = HTMainText->first_anchor; a; a = a->next) {
	    if (a->link_type == INPUT_ANCHOR &&
		links[curlink].l_form == a->input_field) {
		return same_anchors(a, a->next, TRUE);
	    }
	    if (links[curlink].anchor_number &&
		a->number >= links[curlink].anchor_number)
		break;
	}
	return NO;
    }
}

/*
 * HTGetLinkOrFieldStart - moving to previous or next link or form field.
 *
 * On input,
 *	curlink: current link, as index in links[] array (-1 if none)
 *	direction: whether to move up or down (or stay where we are)
 *	ta_skip: if FALSE, input fields belonging to the same textarea are
 *		 are treated as different fields, as usual;
 *		 if TRUE, fields of the same textarea are treated as a
 *		 group for skipping.
 * The caller wants information for positioning on the new link to be
 * deposited in *go_line and (if linknum is not NULL) *linknum.
 *
 * On failure (no more links in the requested direction) returns NO
 * and doesn't change *go_line or *linknum.  Otherwise, LINK_DO_ARROWUP
 * may be returned, and *go_line and *linknum not changed, to indicate that
 * the caller should use a normal PREV_LINK or PREV_PAGE mechanism.
 * Otherwise:
 * The number (0-based counting) for the new top screen line will be returned
 * in *go_line, and *linknum will be set to an index into the links[] array,
 * to use after the line in *go_line has been made the new top screen
 * line.  - kw
 */
int HTGetLinkOrFieldStart(int curlink,
			  int *go_line,
			  int *linknum,
			  int direction,
			  BOOLEAN ta_skip)
{
    TextAnchor *a;
    int anchors_this_line = 0;
    int prev_anchor_line = -1, prev_prev_anchor_line = -1;

    struct agroup {
	TextAnchor *anc;
	int prev_anchor_line;
	int anchors_this_line;
	int anchors_this_group;
    } previous, current;
    struct agroup *group_to_go = NULL;

    if (!HTMainText)
	return (NO);

    previous.anc = current.anc = NULL;
    previous.prev_anchor_line = current.prev_anchor_line = -1;
    previous.anchors_this_line = current.anchors_this_line = 0;
    previous.anchors_this_group = current.anchors_this_group = 0;

    for (a = HTMainText->first_anchor; a; a = a->next) {
	/*
	 * Count anchors, first on current line if there is more
	 * than one.  We have to count all links, including form
	 * field anchors and others with a->number == 0, because
	 * they are or will be included in the links[] array.
	 * The exceptions are hidden form fields and anchors with
	 * show_anchor not set, because they won't appear in links[]
	 * and don't count towards nlinks.  - KW
	 */
	if ((a->show_anchor) &&
	    !(a->link_type == INPUT_ANCHOR
	      && a->input_field->type == F_HIDDEN_TYPE)) {
	    if (a->line_num == prev_anchor_line) {
		anchors_this_line++;
	    } else {
		/*
		 * This anchor is on a different line than the previous one.
		 * Remember which was the line number of the previous anchor,
		 * for use in screen positioning later.  - KW
		 */
		anchors_this_line = 1;
		prev_prev_anchor_line = prev_anchor_line;
		prev_anchor_line = a->line_num;
	    }

	    if (!same_anchors(current.anc, a, ta_skip)) {
		previous.anc = current.anc;
		previous.prev_anchor_line = current.prev_anchor_line;
		previous.anchors_this_line = current.anchors_this_line;
		previous.anchors_this_group = current.anchors_this_group;
		current.anc = a;
		current.prev_anchor_line = prev_prev_anchor_line;
		current.anchors_this_line = anchors_this_line;
		current.anchors_this_group = 1;
	    } else {
		current.anchors_this_group++;
	    }
	    if (curlink >= 0) {
		if (same_anchor_as_link(curlink, a, ta_skip)) {
		    if (direction == -1) {
			group_to_go = &previous;
			break;
		    } else if (direction == 0) {
			group_to_go = &current;
			break;
		    }
		} else if (direction > 0 &&
			   same_anchor_as_link(curlink, previous.anc, ta_skip)) {
		    group_to_go = &current;
		    break;
		}
	    } else {
		if (a->line_num >= HTMainText->top_of_screen) {
		    if (direction < 0) {
			group_to_go = &previous;
			break;
		    } else if (direction == 0) {
			if (previous.anc) {
			    group_to_go = &previous;
			    break;
			} else {
			    group_to_go = &current;
			    break;
			}
		    } else {
			group_to_go = &current;
			break;
		    }
		}
	    }
	}
    }
    if (!group_to_go && curlink < 0 && direction <= 0) {
	group_to_go = &current;
    }
    if (group_to_go) {
	a = group_to_go->anc;
	if (a) {
	    int max_offset;

	    /*
	     * We know where to go; most of the stuff below is just
	     * tweaks to try to position the new screen in a specific
	     * way.
	     *
	     * In some cases going to a previous link can be done
	     * via the normal LYK_PREV_LINK action, which may give
	     * better positioning of the new screen.  - kw
	     */
	    if (a->line_num < HTMainText->top_of_screen &&
		a->line_num >= HTMainText->top_of_screen - (display_lines)) {
		if ((curlink < 0 &&
		     group_to_go->anchors_this_group == 1) ||
		    (direction < 0 &&
		     group_to_go != &current &&
		     current.anc &&
		     current.anc->line_num >= HTMainText->top_of_screen &&
		     group_to_go->anchors_this_group == 1) ||
		    (a->next &&
		     a->next->line_num >= HTMainText->top_of_screen)) {
		    return (LINK_DO_ARROWUP);
		}
	    }
	    /*
	     * The fundamental limitation of the current anchors_this_line
	     * counter method is that we only can set *linknum to the right
	     * index into the future links[] array if the line with our link
	     * ends up being the first line with any links (that count) on
	     * the new screen.  Subject to that restriction we still have
	     * some vertical liberty (sometimes), and try to make the best
	     * of it.  It may be a question of taste though.  - kw
	     */
	    if (a->line_num <= (display_lines)) {
		max_offset = 0;
	    } else if (a->line_num < HTMainText->top_of_screen) {
		int screensback =
		(HTMainText->top_of_screen - a->line_num + (display_lines) - 1)
		/ (display_lines);

		max_offset = a->line_num - (HTMainText->top_of_screen -
					    screensback * (display_lines));
	    } else if (HTMainText->Lines - a->line_num <= (display_lines)) {
		max_offset = a->line_num - (HTMainText->Lines + 1
					    - (display_lines));
	    } else if (a->line_num >=
		       HTMainText->top_of_screen + (display_lines)) {
		int screensahead =
		(a->line_num - HTMainText->top_of_screen) / (display_lines);

		max_offset = a->line_num - HTMainText->top_of_screen -
		    screensahead * (display_lines);
	    } else {
		max_offset = SEARCH_GOAL_LINE - 1;
	    }

	    /* Stuff below should remain unchanged if line positioning
	       is tweaked. - kw */
	    if (max_offset < 0)
		max_offset = 0;
	    else if (max_offset >= display_lines)
		max_offset = display_lines - 1;
	    *go_line = a->line_num - max_offset;
	    if (*go_line <= group_to_go->prev_anchor_line)
		*go_line = group_to_go->prev_anchor_line + 1;

	    if (*go_line < 0)
		*go_line = 0;
	    if (linknum)
		*linknum = group_to_go->anchors_this_line - 1;
	    return (LINK_LINE_FOUND);
	}
    }
    return (NO);
}

/*
 * This function finds the line indicated by line_num in the
 * HText structure indicated by text, and searches that line
 * for the first hit with the string indicated by target.  If
 * there is no hit, FALSE is returned.  If there is a hit, then
 * a copy of the line starting at that first hit is loaded into
 * *data with all IsSpecial characters stripped, its offset and
 * the printable target length (without IsSpecial, or extra CJK
 * or utf8 characters) are loaded into *offset and *tLen, and
 * TRUE is returned.  -FM
 */
BOOL HText_getFirstTargetInLine(HText *text, int line_num,
				BOOL utf_flag,
				int *offset,
				int *tLen,
				char **data,
				const char *target)
{
    HTLine *line;
    char *LineData;
    int LineOffset, HitOffset, LenNeeded, i;
    const char *cp;

    /*
     * Make sure we have an HText structure, that line_num is
     * in its range, and that we have a target string.  -FM
     */
    if (!(text &&
	  line_num >= 0 &&
	  line_num <= text->Lines &&
	  non_empty(target))) {
	return (FALSE);
    }

    /*
     * Find the line and set up its data and offset -FM
     */
    for (i = 0, line = FirstHTLine(text);
	 i < line_num && (line != text->last_line);
	 i++, line = line->next) {
	if (line->next == NULL) {
	    return (FALSE);
	}
    }
    if (!line && line->data[0])
	return (FALSE);
    LineData = (char *) line->data;
    LineOffset = (int) line->offset;

    /*
     * If the target is on the line, load the offset of
     * its first character and the subsequent line data,
     * strip any special characters from the loaded line
     * data, and return TRUE.  -FM
     */
    if (((cp = LYno_attr_mb_strstr(LineData,
				   target,
				   utf_flag, YES,
				   &HitOffset,
				   &LenNeeded)) != NULL) &&
	(LineOffset + LenNeeded) <= DISPLAY_COLS) {
	/*
	 * We had a hit so load the results,
	 * remove IsSpecial characters from
	 * the allocated data string, and
	 * return TRUE.  -FM
	 */
	*offset = (LineOffset + HitOffset);
	*tLen = (LenNeeded - HitOffset);
	StrAllocCopy(*data, cp);
	remove_special_attr_chars(*data);
	return (TRUE);
    }

    /*
     * The line does not contain the target.  -FM
     */
    return (FALSE);
}

/*
 * HText_getNumOfLines returns the number of lines in the
 * current document.
 */
int HText_getNumOfLines(void)
{
    return (HTMainText ? HTMainText->Lines : 0);
}

/*
 * HText_getNumOfBytes returns the size of the document, as rendered.  This
 * may be different from the original filesize.
 */
int HText_getNumOfBytes(void)
{
    int result = -1;
    HTLine *line = NULL;

    if (HTMainText != 0) {
	for (line = FirstHTLine(HTMainText);
	     line != HTMainText->last_line;
	     line = line->next) {
	    result += 1 + strlen(line->data);
	}
    }
    return result;
}

/*
 * HText_getTitle returns the title of the
 * current document.
 */
const char *HText_getTitle(void)
{
    return (HTMainText ?
	    HTAnchor_title(HTMainText->node_anchor) : 0);
}

#ifdef USE_COLOR_STYLE
const char *HText_getStyle(void)
{
    return (HTMainText ?
	    HTAnchor_style(HTMainText->node_anchor) : 0);
}
#endif

/*
 * HText_getSugFname returns the suggested filename of the current
 * document (normally derived from a Content-Disposition header with
 * attachment; filename=name.suffix).  -FM
 */
const char *HText_getSugFname(void)
{
    return (HTMainText ?
	    HTAnchor_SugFname(HTMainText->node_anchor) : 0);
}

/*
 * HTCheckFnameForCompression receives the address of an allocated
 * string containing a filename, and an anchor pointer, and expands
 * or truncates the string's suffix if appropriate, based on whether
 * the anchor indicates that the file is compressed.  We assume
 * that the file was not uncompressed (as when downloading), and
 * believe the headers about whether it's compressed or not.  -FM
 *
 * Added third arg - if strip_ok is FALSE, we don't trust the anchor
 * info enough to remove a compression suffix if the anchor object
 * does not indicate compression.  - kw
 */
void HTCheckFnameForCompression(char **fname,
				HTParentAnchor *anchor,
				BOOLEAN strip_ok)
{
    char *fn = *fname;
    char *dot = NULL;
    char *cp = NULL;
    const char *suffix = "";
    const char *ct = NULL;
    const char *ce = NULL;
    CompressFileType method = cftNone;
    CompressFileType second;

    /*
     * Make sure we have a string and anchor.  -FM
     */
    if (!(fn && anchor))
	return;

    /*
     * Make sure we have a file, not directory, name.  -FM
     */
    if (*(fn = LYPathLeaf(fn)) == '\0')
	return;

    /*
     * Check the anchor's content_type and content_encoding
     * elements for a gzip or Unix compressed file.  -FM
     */
    ct = HTAnchor_content_type(anchor);
    ce = HTAnchor_content_encoding(anchor);
    if (ce == NULL && ct != 0) {
	/*
	 * No Content-Encoding, so check
	 * the Content-Type.  -FM
	 */
	if (!strncasecomp(ct, "application/gzip", 16) ||
	    !strncasecomp(ct, "application/x-gzip", 18)) {
	    method = cftGzip;
	} else if (!strncasecomp(ct, "application/compress", 20) ||
		   !strncasecomp(ct, "application/x-compress", 22)) {
	    method = cftCompress;
	} else if (!strncasecomp(ct, "application/bzip2", 17) ||
		   !strncasecomp(ct, "application/x-bzip2", 19)) {
	    method = cftBzip2;
	}
    } else if (ce != 0) {
	method = HTEncodingToCompressType(ce);
    }

    /*
     * If no Content-Encoding has been detected via the anchor
     * pointer, but strip_ok is not set, there is nothing left
     * to do.  - kw
     */
    if ((method == cftNone) && !strip_ok)
	return;

    /*
     * Treat .tgz specially
     */
    if ((dot = strrchr(fn, '.')) != NULL
	&& !strcasecomp(dot, ".tgz")) {
	if (method == cftNone) {
	    strcpy(dot, ".tar");
	}
	return;
    }

    /*
     * Seek the last dot, and check whether
     * we have a gzip or compress suffix.  -FM
     */
    if ((dot = strrchr(fn, '.')) != NULL) {
	int rootlen = 0;

	if (HTCompressFileType(fn, ".", &rootlen) != cftNone) {
	    if (method == cftNone) {
		/*
		 * It has a suffix which signifies a gzipped
		 * or compressed file for us, but the anchor
		 * claims otherwise, so tweak the suffix.  -FM
		 */
		*dot = '\0';
	    }
	    return;
	}
	if ((second = HTCompressFileType(fn, "-_", &rootlen)) != cftNone) {
	    cp = fn + rootlen;
	    if (method == cftNone) {
		/*
		 * It has a tail which signifies a gzipped
		 * file for us, but the anchor claims otherwise,
		 * so tweak the suffix.  -FM
		 */
		if (cp == dot + 1)
		    cp--;
		*cp = '\0';
	    } else {
		/*
		 * The anchor claims it's gzipped, and we
		 * believe it, so force this tail to the
		 * conventional suffix.  -FM
		 */
#ifdef VMS
		*cp = '-';
#else
		*cp = '.';
#endif /* VMS */
		if (second == cftCompress)
		    LYUpperCase(cp);
		else
		    LYLowerCase(cp);
	    }
	    return;
	}
    }

    switch (method) {
    case cftNone:
	suffix = "";
	break;
    case cftCompress:
	suffix = ".Z";
	break;
    case cftDeflate:
	suffix = ".zz";
	break;
    case cftGzip:
	suffix = ".gz";
	break;
    case cftBzip2:
	suffix = ".bz2";
	break;
    }

    /*
     * Add the appropriate suffix.  -FM
     */
    if (*suffix) {
	if (!dot) {
	    StrAllocCat(*fname, suffix);
	} else if (*++dot == '\0') {
	    StrAllocCat(*fname, suffix + 1);
	} else {
	    StrAllocCat(*fname, suffix);
#ifdef VMS
	    (*fname)[strlen(*fname) - strlen(suffix)] = '-';
#endif /* !VMS */
	}
    }
}

/*
 * HText_getLastModified returns the Last-Modified header
 * if available, for the current document.  -FM
 */
const char *HText_getLastModified(void)
{
    return (HTMainText ?
	    HTAnchor_last_modified(HTMainText->node_anchor) : 0);
}

/*
 * HText_getDate returns the Date header
 * if available, for the current document.  -FM
 */
const char *HText_getDate(void)
{
    return (HTMainText ?
	    HTAnchor_date(HTMainText->node_anchor) : 0);
}

/*
 * HText_getServer returns the Server header
 * if available, for the current document.  -FM
 */
const char *HText_getServer(void)
{
    return (HTMainText ?
	    HTAnchor_server(HTMainText->node_anchor) : 0);
}

#ifdef EXP_HTTP_HEADERS
/*
 * Returns the full text of HTTP headers, if available, for the current
 * document.
 */
const char *HText_getHttpHeaders(void)
{
    return (HTMainText ?
	    HTAnchor_http_headers(HTMainText->node_anchor) : 0);
}
#endif

/*
 * HText_pageDisplay displays a screen of text
 * starting from the line 'line_num'-1.
 * This is the primary call for lynx.
 */
void HText_pageDisplay(int line_num,
		       char *target)
{
#ifdef DISP_PARTIAL
    if (debug_display_partial || (LYTraceLogFP != NULL)) {
	CTRACE((tfp, "GridText: HText_pageDisplay at line %d started\n", line_num));
    }

    if (display_partial) {
	int stop_before = -1;

	/*
	 * Garbage is reported from forms input fields in incremental mode.
	 * So we start HText_trimHightext() to forget this side effect.
	 * This function was split-out from HText_endAppend().
	 * It may not be the best solution but it works.  - LP
	 *
	 * (FALSE = indicate that we are in partial mode)
	 * Multiple calls of HText_trimHightext works without problem now.
	 */
	if (HTMainText && HTMainText->stbl)
	    stop_before = Stbl_getStartLineDeep(HTMainText->stbl);
	HText_trimHightext(HTMainText, FALSE, stop_before);
    }
#endif

    display_page(HTMainText, line_num - 1, target);

#ifdef DISP_PARTIAL
    if (display_partial && debug_display_partial)
	LYSleepMsg();
#endif

    is_www_index = HTAnchor_isIndex(HTMainAnchor);

#ifdef DISP_PARTIAL
    if (debug_display_partial || (LYTraceLogFP != NULL)) {
	CTRACE((tfp, "GridText: HText_pageDisplay finished\n"));
    }
#endif
}

/*
 * Return YES if we have a whereis search target on the displayed
 * page.  - kw
 */
BOOL HText_pageHasPrevTarget(void)
{
    if (!HTMainText)
	return NO;
    else
	return HTMainText->page_has_target;
}

/*
 * Find the number of the closest anchor to the given document offset.  Used
 * in reparsing, this will usually find an exact match, as a link shifts around
 * on the display.  It will not find a match when (for example) the source view
 * shows images that are not links in the html.
 */
int HText_closestAnchor(HText *text, int offset)
{
    int result = -1;
    int absdiff = 0;
    int newdiff;
    TextAnchor *Anchor_ptr = NULL;
    TextAnchor *closest = NULL;

    for (Anchor_ptr = text->first_anchor;
	 Anchor_ptr != NULL;
	 Anchor_ptr = Anchor_ptr->next) {
	if (Anchor_ptr->sgml_offset == offset) {
	    result = Anchor_ptr->number;
	    break;
	} else {
	    newdiff = abs(Anchor_ptr->sgml_offset - offset);
	    if (absdiff == 0 || absdiff > newdiff) {
		absdiff = newdiff;
		closest = Anchor_ptr;
	    }
	}
    }
    if (result < 0 && closest != 0) {
	result = closest->number;
    }

    return result;
}

/*
 * Find the offset for the given anchor, e.g., the inverse of
 * HText_closestAnchor().
 */
int HText_locateAnchor(HText *text, int anchor_number)
{
    int result = -1;
    TextAnchor *Anchor_ptr = NULL;

    for (Anchor_ptr = text->first_anchor;
	 Anchor_ptr != NULL;
	 Anchor_ptr = Anchor_ptr->next) {
	if (Anchor_ptr->number == anchor_number) {
	    result = Anchor_ptr->sgml_offset;
	    break;
	}
    }

    return result;
}

/*
 * This is supposed to give the same result as the inline checks in
 * display_page(), so we can decide which anchors will be visible.
 */
static BOOL anchor_is_numbered(TextAnchor *Anchor_ptr)
{
    BOOL result = FALSE;

    if (Anchor_ptr->show_anchor
    /* FIXME: && non_empty(hi_string) */
	&& (Anchor_ptr->link_type & HYPERTEXT_ANCHOR)) {
	result = TRUE;
    } else if (Anchor_ptr->link_type == INPUT_ANCHOR
	       && Anchor_ptr->input_field->type != F_HIDDEN_TYPE) {
	result = TRUE;
    }
    return result;
}

/*
 * Return the absolute line number (counting from the beginning of the
 * document) for the given absolute anchor number.  Normally line numbers are
 * computed within the screen, and for that we use the links[] array.  A few
 * uses require the absolute anchor number.  For example, reparsing a document,
 * e.g., switching between normal and source views will alter the line numbers
 * of each link, and may require adjusting the top line number used for the
 * display, before we recompute the links[] array.
 */
int HText_getAbsLineNumber(HText *text,
			   int anchor_number)
{
    int result = -1;

    if (anchor_number >= 0 && text != 0) {
	TextAnchor *Anchor_ptr = NULL;

	for (Anchor_ptr = text->first_anchor;
	     Anchor_ptr != NULL;
	     Anchor_ptr = Anchor_ptr->next) {
	    if (anchor_is_numbered(Anchor_ptr)
		&& Anchor_ptr->number == anchor_number) {
		result = Anchor_ptr->line_num;
		break;
	    }
	}
    }
    return result;
}

/*
 * Compute the link-number in a page, given the top line number of the page and
 * the absolute anchor number.
 */
int HText_anchorRelativeTo(HText *text, int top_lineno, int anchor_number)
{
    int result = 0;
    int from_top = 0;
    TextAnchor *Anchor_ptr = NULL;

    for (Anchor_ptr = text->first_anchor;
	 Anchor_ptr != NULL;
	 Anchor_ptr = Anchor_ptr->next) {
	if (Anchor_ptr->number == anchor_number) {
	    result = from_top;
	    break;
	}
	if (!anchor_is_numbered(Anchor_ptr))
	    continue;
	if (Anchor_ptr->line_num >= top_lineno) {
	    ++from_top;
	}
    }
    return result;
}

/*
 * HText_LinksInLines returns the number of links in the
 * 'Lines' number of lines beginning with 'line_num'-1.  -FM
 */
int HText_LinksInLines(HText *text,
		       int line_num,
		       int Lines)
{
    int total = 0;
    int start = (line_num - 1);
    int end = (start + Lines);
    TextAnchor *Anchor_ptr = NULL;

    if (!text)
	return total;

    for (Anchor_ptr = text->first_anchor;
	 Anchor_ptr != NULL && Anchor_ptr->line_num <= end;
	 Anchor_ptr = Anchor_ptr->next) {
	if (Anchor_ptr->line_num >= start &&
	    Anchor_ptr->line_num < end &&
	    Anchor_ptr->show_anchor &&
	    !(Anchor_ptr->link_type == INPUT_ANCHOR
	      && Anchor_ptr->input_field->type == F_HIDDEN_TYPE))
	    ++total;
    }

    return total;
}

void HText_setStale(HText *text)
{
    text->stale = YES;
}

void HText_refresh(HText *text)
{
    if (text->stale)
	display_page(text, text->top_of_screen, "");
}

int HText_sourceAnchors(HText *text)
{
    return (text ? text->last_anchor_number : -1);
}

BOOL HText_canScrollUp(HText *text)
{
    return (BOOL) (text->top_of_screen != 0);
}

/*
 * Check if there is more info below this page.
 */
BOOL HText_canScrollDown(void)
{
    HText *text = HTMainText;

    return (BOOL) ((text != 0)
		   && ((text->top_of_screen + display_lines) < text->Lines));
}

/*		Scroll actions
*/
void HText_scrollTop(HText *text)
{
    display_page(text, 0, "");
}

void HText_scrollDown(HText *text)
{
    display_page(text, text->top_of_screen + display_lines, "");
}

void HText_scrollUp(HText *text)
{
    display_page(text, text->top_of_screen - display_lines, "");
}

void HText_scrollBottom(HText *text)
{
    display_page(text, text->Lines - display_lines, "");
}

/*		Browsing functions
 *		==================
 */

/* Bring to front and highlight it
*/
BOOL HText_select(HText *text)
{
    if (text != HTMainText) {
	/*
	 * Reset flag for whereis search string - cannot be true here
	 * since text is not our HTMainText.  - kw
	 */
	if (text)
	    text->page_has_target = NO;

#ifdef DISP_PARTIAL
	/* Reset these for the previous and current text. - kw */
	ResetPartialLinenos(text);
	ResetPartialLinenos(HTMainText);
#endif /* DISP_PARTIAL */

#ifdef CAN_SWITCH_DISPLAY_CHARSET
	/* text->UCLYhndl is not reset by META, so use a more circumvent way */
	if (text->node_anchor->UCStages->s[UCT_STAGE_HTEXT].LYhndl
	    != current_char_set)
	    Switch_Display_Charset(text->node_anchor->UCStages->s[UCT_STAGE_HTEXT].LYhndl, SWITCH_DISPLAY_CHARSET_MAYBE);
#endif
	if (HTMainText) {
	    if (HText_hasUTF8OutputSet(HTMainText) &&
		HTLoadedDocumentEightbit() &&
		LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8) {
		text->had_utf8 = HTMainText->has_utf8;
	    } else {
		text->had_utf8 = NO;
	    }
	    HTMainText->has_utf8 = NO;
	    text->has_utf8 = NO;
	}

	HTMainText = text;
	HTMainAnchor = text->node_anchor;

	/*
	 * Make this text the most current in the loaded texts list.  -FM
	 */
	if (loaded_texts && HTList_removeObject(loaded_texts, text))
	    HTList_addObject(loaded_texts, text);
    }
    return YES;
}

/*
 * This function returns TRUE if doc's post_data, address
 * and isHEAD elements are identical to those of a loaded
 * (memory cached) text.  -FM
 */
BOOL HText_POSTReplyLoaded(DocInfo *doc)
{
    HText *text = NULL;
    HTList *cur = loaded_texts;
    bstring *post_data;
    char *address;
    BOOL is_head;

    /*
     * Make sure we have the structures.  -FM
     */
    if (!cur || !doc)
	return (FALSE);

    /*
     * Make sure doc is for a POST reply.  -FM
     */
    if ((post_data = doc->post_data) == NULL ||
	(address = doc->address) == NULL)
	return (FALSE);
    is_head = doc->isHEAD;

    /*
     * Loop through the loaded texts looking for a
     * POST reply match.  -FM
     */
    while (NULL != (text = (HText *) HTList_nextObject(cur))) {
	if (text->node_anchor &&
	    text->node_anchor->post_data &&
	    BINEQ(post_data, text->node_anchor->post_data) &&
	    text->node_anchor->address &&
	    !strcmp(address, text->node_anchor->address) &&
	    is_head == text->node_anchor->isHEAD) {
	    return (TRUE);
	}
    }

    return (FALSE);
}

BOOL HTFindPoundSelector(const char *selector)
{
    TextAnchor *a;

    CTRACE((tfp, "FindPound: searching for \"%s\"\n", selector));
    for (a = HTMainText->first_anchor; a != 0; a = a->next) {

	if (a->anchor && a->anchor->tag) {
	    if (!strcmp(a->anchor->tag, selector)) {

		www_search_result = a->line_num + 1;

		CTRACE((tfp, "FindPound: Selecting anchor [%d] at line %d\n",
			a->number, www_search_result));
		if (!strcmp(selector, LYToolbarName)) {
		    --www_search_result;
		}
		return (YES);
	    }
	}
    }
    return (NO);
}

BOOL HText_selectAnchor(HText *text, HTChildAnchor *anchor)
{
    TextAnchor *a;

/* This is done later, hence HText_select is unused in GridText.c
   Should it be the contrary ? @@@
    if (text != HTMainText) {
	HText_select(text);
    }
*/

    for (a = text->first_anchor; a; a = a->next) {
	if (a->anchor == anchor)
	    break;
    }
    if (!a) {
	CTRACE((tfp, "HText: No such anchor in this text!\n"));
	return NO;
    }

    if (text != HTMainText) {	/* Comment out by ??? */
	HTMainText = text;	/* Put back in by tbl 921208 */
	HTMainAnchor = text->node_anchor;
    } {
	int l = a->line_num;

	CTRACE((tfp, "HText: Selecting anchor [%d] at line %d\n",
		a->number, l));

	if (!text->stale &&
	    (l >= text->top_of_screen) &&
	    (l < text->top_of_screen + display_lines + 1))
	    return YES;

	www_search_result = l - (display_lines / 3);	/* put in global variable */
    }

    return YES;
}

/*		Editing functions		- NOT IMPLEMENTED
 *		=================
 *
 *	These are called from the application.  There are many more functions
 *	not included here from the original text object.
 */

/*	Style handling:
*/
/*	Apply this style to the selection
*/
void HText_applyStyle(HText *me GCC_UNUSED, HTStyle *style GCC_UNUSED)
{

}

/*	Update all text with changed style.
*/
void HText_updateStyle(HText *me GCC_UNUSED, HTStyle *style GCC_UNUSED)
{

}

/*	Return style of  selection
*/
HTStyle *HText_selectionStyle(HText *me GCC_UNUSED, HTStyleSheet *sheet GCC_UNUSED)
{
    return 0;
}

/*	Paste in styled text
*/
void HText_replaceSel(HText *me GCC_UNUSED, const char *aString GCC_UNUSED,
		      HTStyle *aStyle GCC_UNUSED)
{
}

/*	Apply this style to the selection and all similarly formatted text
 *	(style recovery only)
 */
void HTextApplyToSimilar(HText *me GCC_UNUSED, HTStyle *style GCC_UNUSED)
{

}

/*	Select the first unstyled run.
 *	(style recovery only)
 */
void HTextSelectUnstyled(HText *me GCC_UNUSED, HTStyleSheet *sheet GCC_UNUSED)
{

}

/*	Anchor handling:
*/
void HText_unlinkSelection(HText *me GCC_UNUSED)
{

}

HTAnchor *HText_referenceSelected(HText *me GCC_UNUSED)
{
    return 0;
}

int HText_getTopOfScreen(void)
{
    HText *text = HTMainText;

    return text != 0 ? text->top_of_screen : 0;
}

int HText_getLines(HText *text)
{
    return text->Lines;
}

/*
 * Constrain the line number to be within the document.  The line number is
 * zero-based.
 */
int HText_getPreferredTopLine(HText *text, int line_number)
{
    int last_screen = text->Lines - (display_lines - 2);

    if (text->Lines < display_lines) {
	line_number = 0;
    } else if (line_number > text->Lines) {
	line_number = last_screen;
    } else if (line_number < 0) {
	line_number = 0;
    }
    return line_number;
}

HTAnchor *HText_linkSelTo(HText *me GCC_UNUSED,
			  HTAnchor * anchor GCC_UNUSED)
{
    return 0;
}

/*
 * Utility for freeing the list of previous isindex and whereis queries.  -FM
 */
void HTSearchQueries_free(void)
{
    LYFreeStringList(search_queries);
    search_queries = NULL;
}

/*
 * Utility for listing isindex and whereis queries, making
 * any repeated queries the most current in the list.  -FM
 */
void HTAddSearchQuery(char *query)
{
    char *new_query = NULL;
    char *old;
    HTList *cur;

    if (!non_empty(query))
	return;

    StrAllocCopy(new_query, query);

    if (!search_queries) {
	search_queries = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTSearchQueries_free);
#endif
	HTList_addObject(search_queries, new_query);
	return;
    }

    cur = search_queries;
    while (NULL != (old = (char *) HTList_nextObject(cur))) {
	if (!strcmp(old, new_query)) {
	    HTList_removeObject(search_queries, old);
	    FREE(old);
	    break;
	}
    }
    HTList_addObject(search_queries, new_query);

    return;
}

int do_www_search(DocInfo *doc)
{
    char searchstring[256], temp[256], *cp, *tmpaddress = NULL;
    int ch;
    RecallType recall;
    int QueryTotal;
    int QueryNum;
    BOOLEAN PreviousSearch = FALSE;

    /*
     * Load the default query buffer
     */
    if ((cp = strchr(doc->address, '?')) != NULL) {
	/*
	 * This is an index from a previous search.
	 * Use its query as the default.
	 */
	PreviousSearch = TRUE;
	LYstrncpy(searchstring, ++cp, sizeof(searchstring) - 1);
	for (cp = searchstring; *cp; cp++)
	    if (*cp == '+')
		*cp = ' ';
	HTUnEscape(searchstring);
	strcpy(temp, searchstring);
	/*
	 * Make sure it's treated as the most recent query.  -FM
	 */
	HTAddSearchQuery(searchstring);
    } else {
	/*
	 * New search; no default.
	 */
	searchstring[0] = '\0';
	temp[0] = '\0';
    }

    /*
     * Prompt for a query string.
     */
    if (searchstring[0] == '\0') {
	if (HTMainAnchor->isIndexPrompt)
	    _statusline(HTMainAnchor->isIndexPrompt);
	else
	    _statusline(ENTER_DATABASE_QUERY);
    } else
	_statusline(EDIT_CURRENT_QUERY);
    QueryTotal = (search_queries ? HTList_count(search_queries) : 0);
    recall = (((PreviousSearch && QueryTotal >= 2) ||
	       (!PreviousSearch && QueryTotal >= 1)) ? RECALL_URL : NORECALL);
    QueryNum = QueryTotal;
  get_query:
    if ((ch = LYgetstr(searchstring, VISIBLE,
		       sizeof(searchstring), recall)) < 0 ||
	*searchstring == '\0' || ch == UPARROW || ch == DNARROW) {
	if (recall && ch == UPARROW) {
	    if (PreviousSearch) {
		/*
		 * Use the second to last query in the list.  -FM
		 */
		QueryNum = 1;
		PreviousSearch = FALSE;
	    } else {
		/*
		 * Go back to the previous query in the list.  -FM
		 */
		QueryNum++;
	    }
	    if (QueryNum >= QueryTotal)
		/*
		 * Roll around to the last query in the list.  -FM
		 */
		QueryNum = 0;
	    if ((cp = (char *) HTList_objectAt(search_queries,
					       QueryNum)) != NULL) {
		LYstrncpy(searchstring, cp, sizeof(searchstring) - 1);
		if (*temp && !strcmp(temp, searchstring)) {
		    _statusline(EDIT_CURRENT_QUERY);
		} else if ((*temp && QueryTotal == 2) ||
			   (!(*temp) && QueryTotal == 1)) {
		    _statusline(EDIT_THE_PREV_QUERY);
		} else {
		    _statusline(EDIT_A_PREV_QUERY);
		}
		goto get_query;
	    }
	} else if (recall && ch == DNARROW) {
	    if (PreviousSearch) {
		/*
		 * Use the first query in the list.  -FM
		 */
		QueryNum = QueryTotal - 1;
		PreviousSearch = FALSE;
	    } else {
		/*
		 * Advance to the next query in the list.  -FM
		 */
		QueryNum--;
	    }
	    if (QueryNum < 0)
		/*
		 * Roll around to the first query in the list.  -FM
		 */
		QueryNum = QueryTotal - 1;
	    if ((cp = (char *) HTList_objectAt(search_queries,
					       QueryNum)) != NULL) {
		LYstrncpy(searchstring, cp, sizeof(searchstring) - 1);
		if (*temp && !strcmp(temp, searchstring)) {
		    _statusline(EDIT_CURRENT_QUERY);
		} else if ((*temp && QueryTotal == 2) ||
			   (!(*temp) && QueryTotal == 1)) {
		    _statusline(EDIT_THE_PREV_QUERY);
		} else {
		    _statusline(EDIT_A_PREV_QUERY);
		}
		goto get_query;
	    }
	}

	/*
	 * Search cancelled.
	 */
	HTInfoMsg(CANCELLED);
	return (NULLFILE);
    }

    /*
     * Strip leaders and trailers.  -FM
     */
    LYTrimLeading(searchstring);
    if (!(*searchstring)) {
	HTInfoMsg(CANCELLED);
	return (NULLFILE);
    }
    LYTrimTrailing(searchstring);

    /*
     * Don't resubmit the same query unintentionally.
     */
    if (!LYforce_no_cache && 0 == strcmp(temp, searchstring)) {
	HTUserMsg(USE_C_R_TO_RESUB_CUR_QUERY);
	return (NULLFILE);
    }

    /*
     * Add searchstring to the query list,
     * or make it the most current.  -FM
     */
    HTAddSearchQuery(searchstring);

    /*
     * Show the URL with the new query.
     */
    if ((cp = strchr(doc->address, '?')) != NULL)
	*cp = '\0';
    StrAllocCopy(tmpaddress, doc->address);
    StrAllocCat(tmpaddress, "?");
    StrAllocCat(tmpaddress, searchstring);
    user_message(WWW_WAIT_MESSAGE, tmpaddress);
#ifdef SYSLOG_REQUESTED_URLS
    LYSyslog(tmpaddress);
#endif
    FREE(tmpaddress);
    if (cp)
	*cp = '?';

    /*
     * OK, now we do the search.
     */
    if (HTSearch(searchstring, HTMainAnchor)) {
	/*
	 * Memory leak fixed.
	 * 05-28-94 Lynx 2-3-1 Garrett Arch Blythe
	 */
	auto char *cp_freeme = NULL;

	if (traversal)
	    cp_freeme = stub_HTAnchor_address((HTAnchor *) HTMainAnchor);
	else
	    cp_freeme = HTAnchor_address((HTAnchor *) HTMainAnchor);
	StrAllocCopy(doc->address, cp_freeme);
	FREE(cp_freeme);

	CTRACE((tfp, "\ndo_www_search: newfile: %s\n", doc->address));

	/*
	 * Yah, the search succeeded.
	 */
	return (NORMAL);
    }

    /*
     * Either the search failed (Yuk), or we got redirection.
     * If it's redirection, use_this_url_instead is set, and
     * mainloop() will deal with it such that security features
     * and restrictions are checked before acting on the URL, or
     * rejecting it.  -FM
     */
    return (NOT_FOUND);
}

static void write_offset(FILE *fp, HTLine *line)
{
    int i;

    if (line->data[0]) {
	for (i = 0; i < (int) line->offset; i++) {
	    fputc(' ', fp);
	}
    }
}

static void write_hyphen(FILE *fp)
{
    if (dump_output_immediately &&
	LYRawMode &&
	LYlowest_eightbit[current_char_set] <= 173 &&
	(LYCharSet_UC[current_char_set].enc == UCT_ENC_8859 ||
	 (LYCharSet_UC[current_char_set].like8859 & UCT_R_8859SPECL)) != 0) {
	fputc(0xad, fp);	/* the iso8859 byte for SHY */
    } else {
	fputc('-', fp);
    }
}

/*
 * Returns the length after trimming trailing blanks.  Modify the string as
 * needed so that any special character which follows a trailing blank is moved
 * before the (trimmed) blank, so the result which will be dumped has no
 * trailing blanks.
 */
static int TrimmedLength(char *string)
{
    int result = strlen(string);
    int adjust = result;
    int ch;

    while (adjust > 0) {
	ch = UCH(string[adjust - 1]);
	if (isspace(ch) || IsSpecialAttrChar(ch)) {
	    --adjust;
	} else {
	    break;
	}
    }
    if (result != adjust) {
	char *dst = string + adjust;
	char *src = dst;

	for (;;) {
	    src = LYSkipBlanks(src);
	    if ((*dst++ = *src++) == '\0')
		break;
	}
	result = (dst - string - 1);
    }
    return result;
}

/*
 * Print the contents of the file in HTMainText to
 * the file descriptor fp.
 * If is_email is TRUE add ">" before each "From " line.
 * If is_reply is TRUE add ">" to the beginning of each
 * line to specify the file is a reply to message.
 */
void print_wwwfile_to_fd(FILE *fp,
			 BOOLEAN is_email,
			 BOOLEAN is_reply)
{
    register int i;
    int first = TRUE;
    int limit;
    HTLine *line;

#ifndef NO_DUMP_WITH_BACKSPACES
    HText *text = HTMainText;
    BOOL in_b = FALSE;
    BOOL in_u = FALSE;
    BOOL bs = (BOOL) (!is_email && !is_reply
		      && text != 0
		      && with_backspaces
		      && HTCJK == NOCJK
		      && !text->T.output_utf8);
#endif

    if (!HTMainText)
	return;

    line = FirstHTLine(HTMainText);
    for (;; line = line->next) {
	if (first) {
	    first = FALSE;
	    if (is_reply) {
		fputc('>', fp);
	    } else if (is_email && !strncmp(line->data, "From ", 5)) {
		fputc('>', fp);
	    }
	} else if (line->data[0] != LY_SOFT_NEWLINE) {
	    fputc('\n', fp);
	    /*
	     * Add news-style quotation if requested.  -FM
	     */
	    if (is_reply) {
		fputc('>', fp);
	    } else if (is_email && !strncmp(line->data, "From ", 5)) {
		fputc('>', fp);
	    }
	}

	write_offset(fp, line);

	/*
	 * Add data.
	 */
	limit = TrimmedLength(line->data);
	for (i = 0; i < limit; i++) {
	    int ch = UCH(line->data[i]);

	    if (!IsSpecialAttrChar(ch)) {
#ifndef NO_DUMP_WITH_BACKSPACES
		if (in_b) {
		    fputc(ch, fp);
		    fputc('\b', fp);
		    fputc(ch, fp);
		} else if (in_u) {
		    fputc('_', fp);
		    fputc('\b', fp);
		    fputc(ch, fp);
		} else
#endif
		    fputc(ch, fp);
	    } else if (ch == LY_SOFT_HYPHEN &&
		       (i + 1) >= limit) {	/* last char on line */
		write_hyphen(fp);
	    } else if (dump_output_immediately && use_underscore) {
		switch (ch) {
		case LY_UNDERLINE_START_CHAR:
		case LY_UNDERLINE_END_CHAR:
		    fputc('_', fp);
		    break;
		case LY_BOLD_START_CHAR:
		case LY_BOLD_END_CHAR:
		    break;
		}
	    }
#ifndef NO_DUMP_WITH_BACKSPACES
	    else if (bs) {
		switch (ch) {
		case LY_UNDERLINE_START_CHAR:
		    if (!in_b)
			in_u = TRUE;	/*favor bold over underline */
		    break;
		case LY_UNDERLINE_END_CHAR:
		    in_u = FALSE;
		    break;
		case LY_BOLD_START_CHAR:
		    if (in_u)
			in_u = FALSE;	/* turn it off */
		    in_b = TRUE;
		    break;
		case LY_BOLD_END_CHAR:
		    in_b = FALSE;
		    break;
		}
	    }
#endif
	}

	if (line == HTMainText->last_line)
	    break;

#ifdef VMS
	if (HadVMSInterrupt)
	    break;
#endif /* VMS */
    }
    fputc('\n', fp);

}

/*
 * Print the contents of the file in HTMainText to
 * the file descriptor fp.
 * First output line is "thelink", ie, the URL for this file.
 */
void print_crawl_to_fd(FILE *fp, char *thelink,
		       char *thetitle)
{
    register int i;
    int first = TRUE;
    int limit;
    HTLine *line;

    if (!HTMainText)
	return;

    line = FirstHTLine(HTMainText);
    fprintf(fp, "THE_URL:%s\n", thelink);
    if (thetitle != NULL) {
	fprintf(fp, "THE_TITLE:%s\n", thetitle);
    }

    for (;; line = line->next) {
	if (!first && line->data[0] != LY_SOFT_NEWLINE)
	    fputc('\n', fp);
	first = FALSE;
	write_offset(fp, line);

	/*
	 * Add data.
	 */
	limit = TrimmedLength(line->data);
	for (i = 0; i < limit; i++) {
	    int ch = UCH(line->data[i]);

	    if (!IsSpecialAttrChar(ch)) {
		fputc(ch, fp);
	    } else if (ch == LY_SOFT_HYPHEN &&
		       (i + 1) >= limit) {	/* last char on line */
		write_hyphen(fp);
	    }
	}

	if (line == HTMainText->last_line) {
	    break;
	}
    }
    fputc('\n', fp);

    /*
     * Add the References list if appropriate
     */
    if ((no_list == FALSE) &&
	links_are_numbered()) {
	printlist(fp, FALSE);
    }
#ifdef VMS
    HadVMSInterrupt = FALSE;
#endif /* VMS */
}

static void adjust_search_result(DocInfo *doc, int tentative_result,
				 int start_line)
{
    if (tentative_result > 0) {
	int anch_line = -1;
	TextAnchor *a;
	int nl_closest = -1;
	int goal = SEARCH_GOAL_LINE;
	int max_offset;
	BOOL on_screen = (BOOL) (tentative_result > HTMainText->top_of_screen &&
				 tentative_result <= HTMainText->top_of_screen +
				 display_lines);

	if (goal < 1)
	    goal = 1;
	else if (goal > display_lines)
	    goal = display_lines;
	max_offset = goal - 1;

	if (on_screen && nlinks > 0) {
	    int i;

	    for (i = 0; i < nlinks; i++) {
		if (doc->line + links[i].ly - 1 <= tentative_result)
		    nl_closest = i;
		if (doc->line + links[i].ly - 1 >= tentative_result)
		    break;
	    }
	    if (nl_closest >= 0 &&
		doc->line + links[nl_closest].ly - 1 == tentative_result) {
		www_search_result = doc->line;
		doc->link = nl_closest;
		return;
	    }
	}

	/* find last anchor before or on target line */
	for (a = HTMainText->first_anchor;
	     a && a->line_num <= tentative_result - 1; a = a->next) {
	    anch_line = a->line_num + 1;
	}
	/* position such that the anchor found is on first screen line,
	   if it is not too far above the target line; but also try to
	   make sure we move forward. */
	if (anch_line >= 0 &&
	    anch_line >= tentative_result - max_offset &&
	    (anch_line > start_line ||
	     tentative_result <= HTMainText->top_of_screen)) {
	    www_search_result = anch_line;
	} else if (tentative_result - start_line > 0 &&
		   tentative_result - (start_line + 1) <= max_offset) {
	    www_search_result = start_line + 1;
	} else if (tentative_result > HTMainText->top_of_screen &&
		   tentative_result <= start_line &&	/* have wrapped */
		   tentative_result <= HTMainText->top_of_screen + goal) {
	    www_search_result = HTMainText->top_of_screen + 1;
	} else if (tentative_result <= goal)
	    www_search_result = 1;
	else
	    www_search_result = tentative_result - max_offset;
	if (www_search_result == doc->line) {
	    if (nl_closest >= 0) {
		doc->link = nl_closest;
		return;
	    }
	}
    }
}

static BOOL anchor_has_target(TextAnchor *a, char *target)
{
    OptionType *option;
    char *stars = NULL, *sp;
    const char *cp;
    int count;

    /*
     * Search the hightext strings, taking the case_sensitive setting into
     * account.  -FM
     */
    for (count = 0;; ++count) {
	if ((cp = LYGetHiTextStr(a, count)) == NULL)
	    break;
	if (LYno_attr_strstr(cp, target))
	    return TRUE;
    }

    /*
     * Search the relevant form fields, taking the
     * case_sensitive setting into account.  -FM
     */
    if ((a->input_field != NULL && a->input_field->value != NULL) &&
	a->input_field->type != F_HIDDEN_TYPE) {
	if (a->input_field->type == F_PASSWORD_TYPE) {
	    /*
	     * Check the actual, hidden password, and then
	     * the displayed string.  -FM
	     */
	    if (LYno_attr_strstr(a->input_field->value, target)) {
		return TRUE;
	    }
	    StrAllocCopy(stars, a->input_field->value);
	    for (sp = stars; *sp != '\0'; sp++)
		*sp = '*';
	    if (LYno_attr_strstr(stars, target)) {
		FREE(stars);
		return TRUE;
	    }
	    FREE(stars);
	} else if (a->input_field->type == F_OPTION_LIST_TYPE) {
	    /*
	     * Search the option strings that are displayed
	     * when the popup is invoked.  -FM
	     */
	    option = a->input_field->select_list;
	    while (option != NULL) {
		if (LYno_attr_strstr(option->name, target)) {
		    return TRUE;
		}
		option = option->next;
	    }
	} else if (a->input_field->type == F_RADIO_TYPE) {
	    /*
	     * Search for checked or unchecked parens.  -FM
	     */
	    if (a->input_field->num_value) {
		cp = checked_radio;
	    } else {
		cp = unchecked_radio;
	    }
	    if (LYno_attr_strstr(cp, target)) {
		return TRUE;
	    }
	} else if (a->input_field->type == F_CHECKBOX_TYPE) {
	    /*
	     * Search for checked or unchecked square brackets.  -FM
	     */
	    if (a->input_field->num_value) {
		cp = checked_box;
	    } else {
		cp = unchecked_box;
	    }
	    if (LYno_attr_strstr(cp, target)) {
		return TRUE;
	    }
	} else {
	    /*
	     * Check the values intended for display.  May have been found
	     * already via the hightext search, but make sure here that the
	     * entire value is searched.  -FM
	     */
	    if (LYno_attr_strstr(a->input_field->value, target)) {
		return TRUE;
	    }
	}
    }
    return FALSE;
}

static TextAnchor *line_num_to_anchor(int line_num)
{
    TextAnchor *a;

    if (HTMainText != 0) {
	a = HTMainText->first_anchor;
	while (a != 0 && a->line_num < line_num) {
	    a = a->next;
	}
    } else {
	a = 0;
    }
    return a;
}

static int line_num_in_text(HText *text, HTLine *line)
{
    int result = 1;
    HTLine *temp = FirstHTLine(text);

    while (temp != line) {
	temp = temp->next;
	++result;
    }
    return result;
}

/* Computes the 'prev' pointers on demand, and returns the one for the given
 * anchor.
 */
static TextAnchor *get_prev_anchor(TextAnchor *a)
{
    TextAnchor *p, *q;

    if (a->prev == 0) {
	if ((p = HTMainText->first_anchor) != 0) {
	    while ((q = p->next) != 0) {
		q->prev = p;
		p = q;
	    }
	}
    }
    return a->prev;
}

static int www_search_forward(int start_line,
			      DocInfo *doc,
			      char *target,
			      HTLine *line,
			      int count)
{
    int wrapped = 0;
    TextAnchor *a = line_num_to_anchor(count - 1);
    int tentative_result = -1;

    for (;;) {
	while ((a != NULL) && a->line_num == (count - 1)) {
	    if (a->show_anchor &&
		!(a->link_type == INPUT_ANCHOR
		  && a->input_field->type == F_HIDDEN_TYPE)) {
		if (anchor_has_target(a, target)) {
		    adjust_search_result(doc, count, start_line);
		    return 1;
		}
	    }
	    a = a->next;
	}

	if (LYno_attr_strstr(line->data, target)) {
	    tentative_result = count;
	    break;
	} else if ((count == start_line && wrapped) || wrapped > 1) {
	    HTUserMsg2(STRING_NOT_FOUND, target);
	    return -1;
	} else if (line == HTMainText->last_line) {
	    count = 0;
	    wrapped++;
	}
	line = line->next;
	count++;
    }
    if (tentative_result > 0) {
	adjust_search_result(doc, tentative_result, start_line);
    }
    return 0;
}

static int www_search_backward(int start_line,
			       DocInfo *doc,
			       char *target,
			       HTLine *line,
			       int count)
{
    int wrapped = 0;
    TextAnchor *a = line_num_to_anchor(count - 1);
    int tentative_result = -1;

    for (;;) {
	while ((a != NULL) && a->line_num == (count - 1)) {
	    if (a->show_anchor &&
		!(a->link_type == INPUT_ANCHOR
		  && a->input_field->type == F_HIDDEN_TYPE)) {
		if (anchor_has_target(a, target)) {
		    adjust_search_result(doc, count, start_line);
		    return 1;
		}
	    }
	    a = get_prev_anchor(a);
	}

	if (LYno_attr_strstr(line->data, target)) {
	    tentative_result = count;
	    break;
	} else if ((count == start_line && wrapped) || wrapped > 1) {
	    HTUserMsg2(STRING_NOT_FOUND, target);
	    return -1;
	} else if (line == FirstHTLine(HTMainText)) {
	    count = line_num_in_text(HTMainText, LastHTLine(HTMainText)) + 1;
	    wrapped++;
	}
	line = line->prev;
	count--;
    }
    if (tentative_result > 0) {
	adjust_search_result(doc, tentative_result, start_line);
    }
    return 0;
}

void www_user_search(int start_line,
		     DocInfo *doc,
		     char *target,
		     int direction)
{
    HTLine *line;
    int count;

    if (!HTMainText) {
	return;
    }

    /*
     * Advance to the start line.
     */
    line = FirstHTLine(HTMainText);
    if (start_line + direction > 0) {
	for (count = 1;
	     count < start_line + direction;
	     line = line->next, count++) {
	    if (line == HTMainText->last_line) {
		line = FirstHTLine(HTMainText);
		count = 1;
		break;
	    }
	}
    } else {
	line = HTMainText->last_line;
	count = line_num_in_text(HTMainText, line);
    }

    if (direction >= 0)
	www_search_forward(start_line, doc, target, line, count);
    else
	www_search_backward(start_line, doc, target, line, count);
}

void user_message(const char *message,
		  const char *argument)
{
    char *temp = NULL;

    if (message == NULL) {
	mustshow = FALSE;
	return;
    }

    HTSprintf0(&temp, message, NonNull(argument));

    statusline(temp);

    FREE(temp);
    return;
}

/*
 * HText_getOwner returns the owner of the
 * current document.
 */
const char *HText_getOwner(void)
{
    return (HTMainText ?
	    HTAnchor_owner(HTMainText->node_anchor) : 0);
}

/*
 * HText_setMainTextOwner sets the owner for the
 * current document.
 */
void HText_setMainTextOwner(const char *owner)
{
    if (!HTMainText)
	return;

    HTAnchor_setOwner(HTMainText->node_anchor, owner);
}

/*
 * HText_getRevTitle returns the RevTitle element of the
 * current document, used as the subject for mailto comments
 * to the owner.
 */
const char *HText_getRevTitle(void)
{
    return (HTMainText ?
	    HTAnchor_RevTitle(HTMainText->node_anchor) : 0);
}

/*
 * HText_getContentBase returns the Content-Base header
 * of the current document.
 */
const char *HText_getContentBase(void)
{
    return (HTMainText ?
	    HTAnchor_content_base(HTMainText->node_anchor) : 0);
}

/*
 * HText_getContentLocation returns the Content-Location header
 * of the current document.
 */
const char *HText_getContentLocation(void)
{
    return (HTMainText ?
	    HTAnchor_content_location(HTMainText->node_anchor) : 0);
}

/*
 * HText_getMessageID returns the Message-ID of the
 * current document.
 */
const char *HText_getMessageID(void)
{
    return (HTMainText ?
	    HTAnchor_messageID(HTMainText->node_anchor) : NULL);
}

void HTuncache_current_document(void)
{
    /*
     * Should remove current document from memory.
     */
    if (HTMainText) {
	HTParentAnchor *htmain_anchor = HTMainText->node_anchor;

	if (htmain_anchor) {
	    if (!(HTOutputFormat && HTOutputFormat == WWW_SOURCE)) {
		FREE(htmain_anchor->UCStages);
	    }
	}
	CTRACE((tfp, "\nHTuncache.. freeing document for '%s'%s\n",
		((htmain_anchor &&
		  htmain_anchor->address) ?
		 htmain_anchor->address : "unknown anchor"),
		((htmain_anchor &&
		  htmain_anchor->post_data)
		 ? " with POST data"
		 : "")));
	HTList_removeObject(loaded_texts, HTMainText);
	HText_free(HTMainText);
	HTMainText = NULL;
    } else {
	CTRACE((tfp, "HTuncache.. HTMainText already is NULL!\n"));
    }
}

#ifdef USE_SOURCE_CACHE

static HTProtocol scm =
{"source-cache-mem", 0, 0};	/* dummy - kw */

static BOOLEAN useSourceCache(void)
{
    BOOLEAN result = FALSE;

    if (LYCacheSource == SOURCE_CACHE_FILE) {
	result = (HTMainAnchor->source_cache_file != 0);
	CTRACE((tfp, "SourceCache: file-cache%s found\n",
		result ? "" : " not"));
    }
    return result;
}

static BOOLEAN useMemoryCache(void)
{
    BOOLEAN result = FALSE;

    if (LYCacheSource == SOURCE_CACHE_MEMORY) {
	result = (HTMainAnchor->source_cache_chunk != 0);
	CTRACE((tfp, "SourceCache: memory-cache%s found\n",
		result ? "" : " not"));
    }
    return result;
}

BOOLEAN HTreparse_document(void)
{
    BOOLEAN ok = FALSE;

    if (!HTMainAnchor || LYCacheSource == SOURCE_CACHE_NONE) {
	CTRACE((tfp, "HTreparse_document returns FALSE\n"));
	return FALSE;
    }

    if (useSourceCache()) {
	FILE *fp;
	HTFormat format;
	int ret;

	CTRACE((tfp, "SourceCache: Reparsing file %s\n",
		HTMainAnchor->source_cache_file));

	/*
	 * This magic FREE(anchor->UCStages) call
	 * stolen from HTuncache_current_document() above.
	 */
	if (!(HTOutputFormat && HTOutputFormat == WWW_SOURCE)) {
	    FREE(HTMainAnchor->UCStages);
	}

	/*
	 * This is more or less copied out of HTLoadFile(), except we don't
	 * get a content encoding.  This may be overkill.  -dsb
	 */
	if (HTMainAnchor->content_type) {
	    format = HTAtom_for(HTMainAnchor->content_type);
	} else {
	    format = HTFileFormat(HTMainAnchor->source_cache_file, NULL, NULL);
	    format = HTCharsetFormat(format, HTMainAnchor,
				     UCLYhndl_for_unspec);
	    /* not UCLYhndl_HTFile_for_unspec - we are talking about remote
	     * documents...
	     */
	}
	CTRACE((tfp, "  Content type is \"%s\"\n", format->name));

	fp = fopen(HTMainAnchor->source_cache_file, "r");
	if (!fp) {
	    CTRACE((tfp, "  Cannot read file %s\n", HTMainAnchor->source_cache_file));
	    LYRemoveTemp(HTMainAnchor->source_cache_file);
	    FREE(HTMainAnchor->source_cache_file);
	    return FALSE;
	}

	if (HText_HaveUserChangedForms(HTMainText)) {
	    /*
	     * Issue a warning.  Will not restore changed forms, currently.
	     */
	    HTAlert(RELOADING_FORM);
	}
	/* Set HTMainAnchor->protocol or HTMainAnchor->physical to convince
	 * the SourceCacheWriter to not regenerate the cache file (which
	 * would be an unnecessary "loop"). - kw
	 */
	HTAnchor_setProtocol(HTMainAnchor, &HTFile);
	ret = HTParseFile(format, HTOutputFormat, HTMainAnchor, fp, NULL);
	LYCloseInput(fp);
	if (ret == HT_PARTIAL_CONTENT) {
	    HTInfoMsg(gettext("Loading incomplete."));
	    CTRACE((tfp,
		    "SourceCache: `%s' has been accessed, partial content.\n",
		    HTLoadedDocumentURL()));
	}
	ok = (BOOL) (ret == HT_LOADED || ret == HT_PARTIAL_CONTENT);

	CTRACE((tfp, "Reparse file %s\n", (ok ? "succeeded" : "failed")));

    } else if (useMemoryCache()) {
	HTFormat format = WWW_HTML;
	int ret;

	CTRACE((tfp, "SourceCache: Reparsing from memory chunk %p\n",
		(void *) HTMainAnchor->source_cache_chunk));

	/*
	 * This magic FREE(anchor->UCStages) call
	 * stolen from HTuncache_current_document() above.
	 */
	if (!(HTOutputFormat && HTOutputFormat == WWW_SOURCE)) {
	    FREE(HTMainAnchor->UCStages);
	}

	if (HTMainAnchor->content_type) {
	    format = HTAtom_for(HTMainAnchor->content_type);
	} else {
	    /*
	     * This is only done to make things aligned with SOURCE_CACHE_NONE
	     * and SOURCE_CACHE_FILE when switching to source mode since the
	     * original document's charset will be LYPushAssumed() and then
	     * LYPopAssumed().  See LYK_SOURCE in mainloop if you change
	     * something here.  No user-visible benefits, seems just '=' Info
	     * Page will show source's effective charset as "(assumed)".
	     */
	    format = HTCharsetFormat(format, HTMainAnchor,
				     UCLYhndl_for_unspec);
	}
	/* not UCLYhndl_HTFile_for_unspec - we are talking about remote documents... */

	if (HText_HaveUserChangedForms(HTMainText)) {
	    /*
	     * Issue a warning.  Will not restore changed forms, currently.
	     */
	    HTAlert(RELOADING_FORM);
	}
	/* Set HTMainAnchor->protocol or HTMainAnchor->physical to convince
	 * the SourceCacheWriter to not regenerate the cache chunk (which
	 * would be an unnecessary "loop"). - kw
	 */
	HTAnchor_setProtocol(HTMainAnchor, &scm);	/* cheating -
							   anything != &HTTP or &HTTPS would do - kw */
	ret = HTParseMem(format, HTOutputFormat, HTMainAnchor,
			 HTMainAnchor->source_cache_chunk, NULL);
	ok = (BOOL) (ret == HT_LOADED);

	CTRACE((tfp, "Reparse memory %s\n", (ok ? "succeeded" : "failed")));
    }

    return ok;
}

BOOLEAN HTcan_reparse_document(void)
{
    BOOLEAN result = FALSE;

    if (!HTMainAnchor || LYCacheSource == SOURCE_CACHE_NONE) {
	result = FALSE;
    } else if (useSourceCache()) {
	result = LYCanReadFile(HTMainAnchor->source_cache_file);
    } else if (useMemoryCache()) {
	result = TRUE;
    }

    CTRACE((tfp, "HTcan_reparse_document -> %d\n", result));
    return result;
}

static void trace_setting_change(const char *name,
				 int prev_setting,
				 int new_setting)
{
    if (prev_setting != new_setting)
	CTRACE((tfp,
		"HTdocument_settings_changed: %s setting has changed (was %d, now %d)\n",
		name, prev_setting, new_setting));
}

BOOLEAN HTdocument_settings_changed(void)
{
    /*
     * Annoying Hack(TM):  If we don't have a source cache, we can't
     * reparse anyway, so pretend the settings haven't changed.
     */
    if (!HTMainText || !HTcan_reparse_document())
	return FALSE;

    if (TRACE) {
	/*
	 * If we're tracing, note everying that has changed.
	 */
	trace_setting_change("CLICKABLE_IMAGES",
			     HTMainText->clickable_images, clickable_images);
	trace_setting_change("PSEUDO_INLINE_ALTS",
			     HTMainText->pseudo_inline_alts,
			     pseudo_inline_alts);
	trace_setting_change("VERBOSE_IMG",
			     HTMainText->verbose_img,
			     verbose_img);
	trace_setting_change("RAW_MODE", HTMainText->raw_mode,
			     LYUseDefaultRawMode);
	trace_setting_change("HISTORICAL_COMMENTS",
			     HTMainText->historical_comments,
			     historical_comments);
	trace_setting_change("MINIMAL_COMMENTS",
			     HTMainText->minimal_comments, minimal_comments);
	trace_setting_change("SOFT_DQUOTES",
			     HTMainText->soft_dquotes, soft_dquotes);
	trace_setting_change("OLD_DTD", HTMainText->old_dtd, Old_DTD);
	trace_setting_change("KEYPAD_MODE",
			     HTMainText->keypad_mode, keypad_mode);
	if (HTMainText->disp_lines != LYlines || HTMainText->disp_cols != DISPLAY_COLS)
	    CTRACE((tfp,
		    "HTdocument_settings_changed: Screen size has changed (was %dx%d, now %dx%d)\n",
		    HTMainText->disp_cols,
		    HTMainText->disp_lines,
		    DISPLAY_COLS,
		    LYlines));
    }

    return (HTMainText->clickable_images != clickable_images ||
	    HTMainText->pseudo_inline_alts != pseudo_inline_alts ||
	    HTMainText->verbose_img != verbose_img ||
	    HTMainText->raw_mode != LYUseDefaultRawMode ||
	    HTMainText->historical_comments != historical_comments ||
	    (HTMainText->minimal_comments != minimal_comments &&
	     !historical_comments) ||
	    HTMainText->soft_dquotes != soft_dquotes ||
	    HTMainText->old_dtd != Old_DTD ||
	    HTMainText->keypad_mode != keypad_mode ||
	    HTMainText->disp_cols != DISPLAY_COLS);
}
#endif

int HTisDocumentSource(void)
{
    return (HTMainText != 0) ? HTMainText->source : FALSE;
}

const char *HTLoadedDocumentURL(void)
{
    if (!HTMainText)
	return ("");

    if (HTMainText->node_anchor && HTMainText->node_anchor->address)
	return (HTMainText->node_anchor->address);
    else
	return ("");
}

bstring *HTLoadedDocumentPost_data(void)
{
    if (HTMainText
	&& HTMainText->node_anchor
	&& HTMainText->node_anchor->post_data)
	return (HTMainText->node_anchor->post_data);
    else
	return (0);
}

const char *HTLoadedDocumentTitle(void)
{
    if (!HTMainText)
	return ("");

    if (HTMainText->node_anchor && HTMainText->node_anchor->title)
	return (HTMainText->node_anchor->title);
    else
	return ("");
}

BOOLEAN HTLoadedDocumentIsHEAD(void)
{
    if (!HTMainText)
	return (FALSE);

    if (HTMainText->node_anchor && HTMainText->node_anchor->isHEAD)
	return (HTMainText->node_anchor->isHEAD);
    else
	return (FALSE);
}

BOOLEAN HTLoadedDocumentIsSafe(void)
{
    if (!HTMainText)
	return (FALSE);

    if (HTMainText->node_anchor && HTMainText->node_anchor->safe)
	return (HTMainText->node_anchor->safe);
    else
	return (FALSE);
}

const char *HTLoadedDocumentCharset(void)
{
    if (!HTMainText)
	return (NULL);

    if (HTMainText->node_anchor && HTMainText->node_anchor->charset)
	return (HTMainText->node_anchor->charset);
    else
	return (NULL);
}

BOOL HTLoadedDocumentEightbit(void)
{
    if (!HTMainText)
	return (NO);
    else
	return (HTMainText->have_8bit_chars);
}

void HText_setNodeAnchorBookmark(const char *bookmark)
{
    if (!HTMainText)
	return;

    if (HTMainText->node_anchor)
	HTAnchor_setBookmark(HTMainText->node_anchor, bookmark);
}

const char *HTLoadedDocumentBookmark(void)
{
    if (!HTMainText)
	return (NULL);

    if (HTMainText->node_anchor && HTMainText->node_anchor->bookmark)
	return (HTMainText->node_anchor->bookmark);
    else
	return (NULL);
}

int HText_LastLineSize(HText *text, BOOL IgnoreSpaces)
{
    if (!text || !text->last_line || !text->last_line->size)
	return 0;
    return HText_TrueLineSize(text->last_line, text, IgnoreSpaces);
}

BOOL HText_LastLineEmpty(HText *text, BOOL IgnoreSpaces)
{
    if (!text || !text->last_line || !text->last_line->size)
	return TRUE;
    return HText_TrueEmptyLine(text->last_line, text, IgnoreSpaces);
}

int HText_LastLineOffset(HText *text)
{
    if (!text || !text->last_line)
	return 0;
    return text->last_line->offset;
}

int HText_PreviousLineSize(HText *text, BOOL IgnoreSpaces)
{
    HTLine *line;

    if (!text || !text->last_line)
	return 0;
    if (!(line = text->last_line->prev))
	return 0;
    return HText_TrueLineSize(line, text, IgnoreSpaces);
}

BOOL HText_PreviousLineEmpty(HText *text, BOOL IgnoreSpaces)
{
    HTLine *line;

    if (!text || !text->last_line)
	return TRUE;
    if (!(line = text->last_line->prev))
	return TRUE;
    return HText_TrueEmptyLine(line, text, IgnoreSpaces);
}

/*
 * Compute the "true" line size.
 */
static int HText_TrueLineSize(HTLine *line, HText *text, BOOL IgnoreSpaces)
{
    size_t i;
    int true_size = 0;

    if (!(line && line->size))
	return 0;

    if (IgnoreSpaces) {
	for (i = 0; i < line->size; i++) {
	    if (!IsSpecialAttrChar(UCH(line->data[i])) &&
		IS_UTF8_EXTRA(line->data[i]) &&
		!isspace(UCH(line->data[i])) &&
		UCH(line->data[i]) != HT_NON_BREAK_SPACE &&
		UCH(line->data[i]) != HT_EN_SPACE) {
		true_size++;
	    }
	}
    } else {
	for (i = 0; i < line->size; i++) {
	    if (!IsSpecialAttrChar(line->data[i]) &&
		IS_UTF8_EXTRA(line->data[i])) {
		true_size++;
	    }
	}
    }
    return true_size;
}

/*
 * Tell if the line is really empty.  This is invoked much more often than
 * HText_TrueLineSize(), and most lines are not empty.  So it is faster to
 * do this check than to check if the line size happens to be zero.
 */
static BOOL HText_TrueEmptyLine(HTLine *line, HText *text, BOOL IgnoreSpaces)
{
    size_t i;

    if (!(line && line->size))
	return TRUE;

    if (IgnoreSpaces) {
	for (i = 0; i < line->size; i++) {
	    if (!IsSpecialAttrChar(UCH(line->data[i])) &&
		IS_UTF8_EXTRA(line->data[i]) &&
		!isspace(UCH(line->data[i])) &&
		UCH(line->data[i]) != HT_NON_BREAK_SPACE &&
		UCH(line->data[i]) != HT_EN_SPACE) {
		return FALSE;
	    }
	}
    } else {
	for (i = 0; i < line->size; i++) {
	    if (!IsSpecialAttrChar(line->data[i]) &&
		IS_UTF8_EXTRA(line->data[i])) {
		return FALSE;
	    }
	}
    }
    return TRUE;
}

void HText_NegateLineOne(HText *text)
{
    if (text) {
	text->in_line_1 = NO;
    }
    return;
}

BOOL HText_inLineOne(HText *text)
{
    if (text) {
	return text->in_line_1;
    }
    return YES;
}

/*
 * This function is for removing the first of two
 * successive blank lines.  It should be called after
 * checking the situation with HText_LastLineSize()
 * and HText_PreviousLineSize().  Any characters in
 * the removed line (i.e., control characters, or it
 * wouldn't have tested blank) should have been
 * reiterated by split_line() in the retained blank
 * line.  -FM
 */
void HText_RemovePreviousLine(HText *text)
{
    HTLine *line, *previous;

    if (!(text && text->Lines > 1))
	return;

    line = text->last_line->prev;
    previous = line->prev;
    previous->next = text->last_line;
    text->last_line->prev = previous;
    text->Lines--;
    freeHTLine(text, line);
}

/*
 * NOTE:  This function presently is correct only if the
 *	  alignment is HT_LEFT.  The offset is still zero,
 *	  because that's not determined for HT_CENTER or
 *	  HT_RIGHT until subsequent characters are received
 *	  and split_line() is called. -FM
 */
int HText_getCurrentColumn(HText *text)
{
    int column = 0;
    BOOL IgnoreSpaces = FALSE;

    if (text) {
	column = (text->in_line_1 ?
		  (int) text->style->indent1st : (int) text->style->leftIndent)
	    + HText_LastLineSize(text, IgnoreSpaces)
	    + (int) text->last_line->offset;
    }
    return column;
}

int HText_getMaximumColumn(HText *text)
{
    int column = DISPLAY_COLS;

    if (text) {
	column -= (int) text->style->rightIndent;
    }
    return column;
}

/*
 * NOTE:  This function uses HText_getCurrentColumn() which
 *	  presently is correct only if the alignment is
 *	  HT_LEFT. -FM
 */
void HText_setTabID(HText *text, const char *name)
{
    HTTabID *Tab = NULL;
    HTList *cur = text->tabs;
    HTList *last = NULL;

    if (!text || isEmpty(name))
	return;

    if (!cur) {
	cur = text->tabs = HTList_new();
    } else {
	while (NULL != (Tab = (HTTabID *) HTList_nextObject(cur))) {
	    if (Tab->name && !strcmp(Tab->name, name))
		return;		/* Already set.  Keep the first value. */
	    last = cur;
	}
	if (last)
	    cur = last;
    }
    if (!Tab) {			/* New name.  Create a new node */
	Tab = typecalloc(HTTabID);
	if (Tab == NULL)
	    outofmem(__FILE__, "HText_setTabID");
	HTList_addObject(cur, Tab);
	StrAllocCopy(Tab->name, name);
    }
    Tab->column = HText_getCurrentColumn(text);
    return;
}

int HText_getTabIDColumn(HText *text, const char *name)
{
    int column = 0;
    HTTabID *Tab;
    HTList *cur = text->tabs;

    if (text && non_empty(name) && cur) {
	while (NULL != (Tab = (HTTabID *) HTList_nextObject(cur))) {
	    if (Tab->name && !strcmp(Tab->name, name))
		break;
	}
	if (Tab)
	    column = Tab->column;
    }
    return column;
}

/*
 * This function is for saving the address of a link
 * which had an attribute in the markup that resolved
 * to a URL (i.e., not just a NAME or ID attribute),
 * but was found in HText_endAnchor() to have no visible
 * content for use as a link name.  It loads the address
 * into text->hidden_links, whose count can be determined
 * via HText_HiddenLinks(), below.  The addresses can be
 * retrieved via HText_HiddenLinkAt(), below, based on
 * count.  -FM
 */
static void HText_AddHiddenLink(HText *text, TextAnchor *textanchor)
{
    HTAnchor *dest;

    /*
     * Make sure we have an HText structure and anchor.  -FM
     */
    if (!(text && textanchor && textanchor->anchor))
	return;

    /*
     * Create the hidden links list
     * if it hasn't been already.  -FM
     */
    if (text->hidden_links == NULL)
	text->hidden_links = HTList_new();

    /*
     * Store the address, in reverse list order
     * so that first in will be first out on
     * retrievals.  -FM
     */
    if ((dest = HTAnchor_followLink(textanchor->anchor)) &&
	(text->hiddenlinkflag != HIDDENLINKS_IGNORE ||
	 HTList_isEmpty(text->hidden_links))) {
	HTList_appendObject(text->hidden_links, HTAnchor_address(dest));
    }

    return;
}

/*
 * This function returns the number of addresses
 * that are loaded in text->hidden_links.  -FM
 */
int HText_HiddenLinkCount(HText *text)
{
    int count = 0;

    if (text && text->hidden_links)
	count = HTList_count((HTList *) text->hidden_links);

    return (count);
}

/*
 * This function returns the address, corresponding to
 * a hidden link, at the position (zero-based) in the
 * text->hidden_links list of the number argument.  -FM
 */
const char *HText_HiddenLinkAt(HText *text, int number)
{
    char *href = NULL;

    if (text && text->hidden_links && number >= 0)
	href = (char *) HTList_objectAt((HTList *) text->hidden_links, number);

    return (href);
}

/*
 * Form methods
 * These routines are used to build forms consisting
 * of input fields
 */
static int HTFormMethod;
static char *HTFormAction = NULL;
static char *HTFormEnctype = NULL;
static char *HTFormTitle = NULL;
static char *HTFormAcceptCharset = NULL;
static BOOLEAN HTFormDisabled = FALSE;
static PerFormInfo *HTCurrentForm;

void HText_beginForm(char *action,
		     char *method,
		     char *enctype,
		     char *title,
		     const char *accept_cs)
{
    PerFormInfo *newform;

    HTFormMethod = URL_GET_METHOD;
    HTFormNumber++;
    HTFormFields = 0;
    HTFormDisabled = FALSE;

    /*
     * Check the ACTION.  -FM
     */
    if (action != NULL) {
	if (isMAILTO_URL(action)) {
	    HTFormMethod = URL_MAIL_METHOD;
	}
	StrAllocCopy(HTFormAction, action);
    } else
	StrAllocCopy(HTFormAction, HTLoadedDocumentURL());

    /*
     * Check the METHOD.  -FM
     */
    if (method != NULL && HTFormMethod != URL_MAIL_METHOD)
	if (!strcasecomp(method, "post") || !strcasecomp(method, "pget"))
	    HTFormMethod = URL_POST_METHOD;

    /*
     * Check the ENCTYPE.  -FM
     */
    if (non_empty(enctype)) {
	StrAllocCopy(HTFormEnctype, enctype);
	if (HTFormMethod != URL_MAIL_METHOD &&
	    !strncasecomp(enctype, "multipart/form-data", 19))
	    HTFormMethod = URL_POST_METHOD;
    } else {
	FREE(HTFormEnctype);
    }

    /*
     * Check the TITLE.  -FM
     */
    if (non_empty(title))
	StrAllocCopy(HTFormTitle, title);
    else
	FREE(HTFormTitle);

    /*
     * Check for an ACCEPT_CHARSET.  If present, store it and
     * convert to lowercase and collapse spaces.  - kw
     */
    if (accept_cs != NULL) {
	StrAllocCopy(HTFormAcceptCharset, accept_cs);
	LYRemoveBlanks(HTFormAcceptCharset);
	LYLowerCase(HTFormAcceptCharset);
    }

    /*
     * Create a new "PerFormInfo" structure to hold info on the current
     * form.  The HTForm* variables could all migrate there, currently
     * this isn't done (yet?) but it might be less confusing.
     * Currently the only data saved in this structure that will actually
     * be used is the accept_cs string.
     * This will be appended to the forms list kept by the HText object
     * if and when we reach a HText_endForm.  - kw
     */
    newform = typecalloc(PerFormInfo);
    if (newform == NULL)
	outofmem(__FILE__, "HText_beginForm");
    newform->number = HTFormNumber;

    PerFormInfo_free(HTCurrentForm);	/* shouldn't happen here - kw */
    HTCurrentForm = newform;

    CTRACE((tfp, "BeginForm: action:%s Method:%d%s%s%s%s%s%s\n",
	    HTFormAction, HTFormMethod,
	    (HTFormTitle ? " Title:" : ""),
	    NonNull(HTFormTitle),
	    (HTFormEnctype ? " Enctype:" : ""),
	    NonNull(HTFormEnctype),
	    (HTFormAcceptCharset ? " Accept-charset:" : ""),
	    NonNull(HTFormAcceptCharset)));
}

void HText_endForm(HText *text)
{
    if (HTFormFields == 1 && text && text->first_anchor) {
	/*
	 * Support submission of a single text input field in
	 * the form via <return> instead of a submit button.  -FM
	 */
	TextAnchor *a;

	/*
	 * Go through list of anchors and get our input field.  -FM
	 */
	for (a = text->first_anchor; a != NULL; a = a->next) {
	    if (a->link_type == INPUT_ANCHOR &&
		a->input_field->number == HTFormNumber &&
		a->input_field->type == F_TEXT_TYPE) {
		/*
		 * Got it.  Make it submitting.  -FM
		 */
		a->input_field->submit_action = NULL;
		StrAllocCopy(a->input_field->submit_action, HTFormAction);
		if (HTFormEnctype != NULL)
		    StrAllocCopy(a->input_field->submit_enctype,
				 HTFormEnctype);
		if (HTFormTitle != NULL)
		    StrAllocCopy(a->input_field->submit_title, HTFormTitle);
		a->input_field->submit_method = HTFormMethod;
		a->input_field->type = F_TEXT_SUBMIT_TYPE;
		if (HTFormDisabled)
		    a->input_field->disabled = TRUE;
		break;
	    }
	}
    }
    /*
     * Append info on the current form to the HText object's list of
     * forms.
     * HText_beginInput call will have set some of the data in the
     * PerFormInfo structure (if there were any form fields at all),
     * we also fill in the ACCEPT-CHARSET data now (this could have
     * been done earlier).  - kw
     */
    if (HTCurrentForm) {
	if (HTFormDisabled)
	    HTCurrentForm->disabled = TRUE;
	HTCurrentForm->accept_cs = HTFormAcceptCharset;
	HTFormAcceptCharset = NULL;
	if (!text->forms)
	    text->forms = HTList_new();
	HTList_appendObject(text->forms, HTCurrentForm);
	HTCurrentForm = NULL;
    } else {
	CTRACE((tfp, "endForm:    HTCurrentForm is missing!\n"));
    }

    FREE(HTCurSelectGroup);
    FREE(HTCurSelectGroupSize);
    FREE(HTCurSelectedOptionValue);
    FREE(HTFormAction);
    FREE(HTFormEnctype);
    FREE(HTFormTitle);
    FREE(HTFormAcceptCharset);
    HTFormFields = 0;
    HTFormDisabled = FALSE;
}

void HText_beginSelect(char *name,
		       int name_cs,
		       BOOLEAN multiple,
		       char *size)
{
    /*
     * Save the group name.
     */
    StrAllocCopy(HTCurSelectGroup, name);
    HTCurSelectGroupCharset = name_cs;

    /*
     * If multiple then all options are actually checkboxes.
     */
    if (multiple)
	HTCurSelectGroupType = F_CHECKBOX_TYPE;
    /*
     * If not multiple then all options are radio buttons.
     */
    else
	HTCurSelectGroupType = F_RADIO_TYPE;

    /*
     * Length of an option list.
     */
    StrAllocCopy(HTCurSelectGroupSize, size);

    CTRACE((tfp, "HText_beginSelect: name=%s type=%d size=%s\n",
	    ((HTCurSelectGroup == NULL) ?
	     "<NULL>" : HTCurSelectGroup),
	    HTCurSelectGroupType,
	    ((HTCurSelectGroupSize == NULL) ?
	     "<NULL>" : HTCurSelectGroupSize)));
    CTRACE((tfp, "HText_beginSelect: name_cs=%d \"%s\"\n",
	    HTCurSelectGroupCharset,
	    (HTCurSelectGroupCharset >= 0 ?
	     LYCharSet_UC[HTCurSelectGroupCharset].MIMEname : "<UNKNOWN>")));
}

/*
 *  This function returns the number of the option whose
 *  value currently is being accumulated for a select
 *  block. - LE && FM
 */
int HText_getOptionNum(HText *text)
{
    TextAnchor *a;
    OptionType *op;
    int n = 1;			/* start count at 1 */

    if (!(text && text->last_anchor))
	return (0);

    a = text->last_anchor;
    if (!(a->link_type == INPUT_ANCHOR && a->input_field &&
	  a->input_field->type == F_OPTION_LIST_TYPE))
	return (0);

    for (op = a->input_field->select_list; op; op = op->next)
	n++;
    CTRACE((tfp, "HText_getOptionNum: Got number '%d'.\n", n));
    return (n);
}

/*
 *  This function checks for a numbered option pattern
 *  as the prefix for an option value.  If present, and
 *  we are in the correct keypad mode, it returns a
 *  pointer to the actual value, following that prefix.
 *  Otherwise, it returns the original pointer.
 */
static char *HText_skipOptionNumPrefix(char *opname)
{
    /*
     * Check if we are in the correct keypad mode.
     */
    if (fields_are_numbered()) {
	/*
	 * Skip the option number embedded in the option name so the
	 * extra chars won't mess up cgi scripts processing the value.
	 * The format is (nnn)__ where nnn is a number and there is a
	 * minimum of 5 chars (no underscores if (nnn) exceeds 5 chars).
	 * See HTML.c.  If the chars don't exactly match this format,
	 * just use all of opname.  - LE
	 */
	char *cp = opname;

	if ((non_empty(cp) && *cp++ == '(') &&
	    *cp && isdigit(UCH(*cp++))) {
	    while (*cp && isdigit(UCH(*cp)))
		++cp;
	    if (*cp && *cp++ == ')') {
		int i = (cp - opname);

		while (i < 5) {
		    if (*cp != '_')
			break;
		    i++;
		    cp++;
		}
		if (i < 5) {
		    cp = opname;
		}
	    } else {
		cp = opname;
	    }
	} else {
	    cp = opname;
	}
	return (cp);
    }

    return (opname);
}

/*
 *  We couldn't set the value field for the previous option
 *  tag so we have to do it now.  Assume that the last anchor
 *  was the previous options tag.
 */
char *HText_setLastOptionValue(HText *text, char *value,
			       char *submit_value,
			       int order,
			       BOOLEAN checked,
			       int val_cs,
			       int submit_val_cs)
{
    char *cp, *cp1;
    char *ret_Value = NULL;
    unsigned char *tmp = NULL;
    int number = 0, i, j;

    if (!(value
	  && text
	  && text->last_anchor
	  && text->last_anchor->input_field
	  && text->last_anchor->link_type == INPUT_ANCHOR)) {
	CTRACE((tfp, "HText_setLastOptionValue: invalid call!  value:%s!\n",
		(value ? value : "<NULL>")));
	return NULL;
    }

    CTRACE((tfp,
	    "Entering HText_setLastOptionValue: value:\"%s\", checked:%s\n",
	    value, (checked ? "on" : "off")));

    /*
     * Strip end spaces, newline is also whitespace.
     */
    if (*value) {
	cp = &value[strlen(value) - 1];
	while ((cp >= value) && (isspace(UCH(*cp)) ||
				 IsSpecialAttrChar(UCH(*cp))))
	    cp--;
	*(cp + 1) = '\0';
    }

    /*
     * Find first non space
     */
    cp = value;
    while (isspace(UCH(*cp)) ||
	   IsSpecialAttrChar(UCH(*cp)))
	cp++;
    if (HTCurSelectGroupType == F_RADIO_TYPE &&
	LYSelectPopups &&
	fields_are_numbered()) {
	/*
	 * Collapse any space between the popup option
	 * prefix and actual value.  -FM
	 */
	if ((cp1 = HText_skipOptionNumPrefix(cp)) > cp) {
	    i = 0, j = (cp1 - cp);
	    while (isspace(UCH(cp1[i])) ||
		   IsSpecialAttrChar(UCH(cp1[i]))) {
		i++;
	    }
	    if (i > 0) {
		while (cp1[i] != '\0')
		    cp[j++] = cp1[i++];
		cp[j] = '\0';
	    }
	}
    }

    if (HTCurSelectGroupType == F_CHECKBOX_TYPE) {
	StrAllocCopy(text->last_anchor->input_field->value, cp);
	text->last_anchor->input_field->value_cs = val_cs;
	/*
	 * Put the text on the screen as well.
	 */
	HText_appendText(text, cp);

    } else if (LYSelectPopups == FALSE) {
	StrAllocCopy(text->last_anchor->input_field->value,
		     (submit_value ? submit_value : cp));
	text->last_anchor->input_field->value_cs = (submit_value ?
						    submit_val_cs : val_cs);
	/*
	 * Put the text on the screen as well.
	 */
	HText_appendText(text, cp);

    } else {
	/*
	 * Create a linked list of option values.
	 */
	OptionType *op_ptr = text->last_anchor->input_field->select_list;
	OptionType *new_ptr = NULL;
	BOOLEAN first_option = FALSE;

	/*
	 * Deal with newlines or tabs.
	 */
	LYReduceBlanks(value);

	if (!op_ptr) {
	    /*
	     * No option items yet.
	     */
	    if (text->last_anchor->input_field->type != F_OPTION_LIST_TYPE) {
		CTRACE((tfp,
			"HText_setLastOptionValue: last input_field not F_OPTION_LIST_TYPE (%d)\n",
			F_OPTION_LIST_TYPE));
		CTRACE((tfp, "                          but %d, ignoring!\n",
			text->last_anchor->input_field->type));
		return NULL;
	    }

	    new_ptr = text->last_anchor->input_field->select_list =
		typecalloc(OptionType);
	    if (new_ptr == NULL)
		outofmem(__FILE__, "HText_setLastOptionValue");

	    first_option = TRUE;
	} else {
	    while (op_ptr->next) {
		number++;
		op_ptr = op_ptr->next;
	    }
	    number++;		/* add one more */

	    op_ptr->next = new_ptr = typecalloc(OptionType);
	    if (new_ptr == NULL)
		outofmem(__FILE__, "HText_setLastOptionValue");
	}

	new_ptr->name = NULL;
	new_ptr->cp_submit_value = NULL;
	new_ptr->next = NULL;
	/*
	 * Find first non-space again, convert_to_spaces above may have
	 * changed the string.  - kw
	 */
	cp = value;
	while (isspace(UCH(*cp)) ||
	       IsSpecialAttrChar(UCH(*cp)))
	    cp++;
	for (i = 0, j = 0; cp[i]; i++) {
	    if (cp[i] == HT_NON_BREAK_SPACE ||
		cp[i] == HT_EN_SPACE) {
		cp[j++] = ' ';
	    } else if (cp[i] != LY_SOFT_HYPHEN &&
		       !IsSpecialAttrChar(UCH(cp[i]))) {
		cp[j++] = cp[i];
	    }
	}
	cp[j] = '\0';
	if (HTCJK != NOCJK) {
	    if (cp &&
		(tmp = typecallocn(unsigned char, strlen(cp) * 2 + 1)) != 0) {
		if (tmp == NULL)
		    outofmem(__FILE__, "HText_setLastOptionValue");
		if (kanji_code == EUC) {
		    TO_EUC((unsigned char *) cp, tmp);
		    val_cs = current_char_set;
		} else if (kanji_code == SJIS) {
		    TO_SJIS((unsigned char *) cp, tmp);
		    val_cs = current_char_set;
		} else {
		    for (i = 0, j = 0; cp[i]; i++) {
			if (cp[i] != CH_ESC) {	/* S/390 -- gil -- 1604 */
			    tmp[j++] = cp[i];
			}
		    }
		}
		StrAllocCopy(new_ptr->name, (const char *) tmp);
		FREE(tmp);
	    }
	} else {
	    StrAllocCopy(new_ptr->name, cp);
	}
	StrAllocCopy(new_ptr->cp_submit_value,
		     (submit_value ? submit_value :
		      HText_skipOptionNumPrefix(new_ptr->name)));
	new_ptr->value_cs = (submit_value ? submit_val_cs : val_cs);

	if (first_option) {
	    FormInfo *last_input = text->last_anchor->input_field;

	    StrAllocCopy(HTCurSelectedOptionValue, new_ptr->name);
	    last_input->num_value = 0;
	    /*
	     * If this is the first option in a popup select list,
	     * HText_beginInput may have allocated the value and
	     * cp_submit_value fields, so free them now to avoid
	     * a memory leak.  - kw
	     */
	    FREE(last_input->value);
	    FREE(last_input->cp_submit_value);

	    last_input->value = last_input->select_list->name;
	    last_input->orig_value = last_input->select_list->name;
	    last_input->cp_submit_value = last_input->select_list->cp_submit_value;
	    last_input->orig_submit_value = last_input->select_list->cp_submit_value;
	    last_input->value_cs = new_ptr->value_cs;
	} else {
	    int newlen = strlen(new_ptr->name);
	    int curlen = (HTCurSelectedOptionValue
			  ? strlen(HTCurSelectedOptionValue)
			  : 0);

	    /*
	     * Make the selected Option Value as long as
	     * the longest option.
	     */
	    if (newlen > curlen)
		StrAllocCat(HTCurSelectedOptionValue,
			    UNDERSCORES(newlen - curlen));
	}

	if (checked) {
	    int curlen = strlen(new_ptr->name);
	    int newlen = (HTCurSelectedOptionValue
			  ? strlen(HTCurSelectedOptionValue)
			  : 0);
	    FormInfo *last_input = text->last_anchor->input_field;

	    /*
	     * Set the default option as this one.
	     */
	    last_input->num_value = number;
	    last_input->value = new_ptr->name;
	    last_input->orig_value = new_ptr->name;
	    last_input->cp_submit_value = new_ptr->cp_submit_value;
	    last_input->orig_submit_value = new_ptr->cp_submit_value;
	    last_input->value_cs = new_ptr->value_cs;
	    StrAllocCopy(HTCurSelectedOptionValue, new_ptr->name);
	    if (newlen > curlen)
		StrAllocCat(HTCurSelectedOptionValue,
			    UNDERSCORES(newlen - curlen));
	}

	/*
	 * Return the selected Option value to be sent to the screen.
	 */
	if (order == LAST_ORDER) {
	    /*
	     * Change the value.
	     */
	    if (HTCurSelectedOptionValue == 0)
		StrAllocCopy(HTCurSelectedOptionValue, "");
	    text->last_anchor->input_field->size =
		strlen(HTCurSelectedOptionValue);
	    ret_Value = HTCurSelectedOptionValue;
	}
    }

    if (TRACE) {
	CTRACE((tfp, "HText_setLastOptionValue:%s value=\"%s\"\n",
		(order == LAST_ORDER) ? " LAST_ORDER" : "",
		value));
	CTRACE((tfp, "            val_cs=%d \"%s\"",
		val_cs,
		(val_cs >= 0 ?
		 LYCharSet_UC[val_cs].MIMEname : "<UNKNOWN>")));
	if (submit_value) {
	    CTRACE((tfp, " (submit_val_cs %d \"%s\") submit_value%s=\"%s\"\n",
		    submit_val_cs,
		    (submit_val_cs >= 0 ?
		     LYCharSet_UC[submit_val_cs].MIMEname : "<UNKNOWN>"),
		    (HTCurSelectGroupType == F_CHECKBOX_TYPE) ?
		    "(ignored)" : "",
		    submit_value));
	} else {
	    CTRACE((tfp, "\n"));
	}
    }
    return (ret_Value);
}

/*
 * Assign a form input anchor.
 * Returns the number of characters to leave
 * blank so that the input field can fit.
 */
int HText_beginInput(HText *text, BOOL underline,
		     InputFieldData * I)
{
    TextAnchor *a;
    FormInfo *f;
    const char *cp_option = NULL;
    char *IValue = NULL;
    unsigned char *tmp = NULL;
    int i, j;
    int adjust_marker = 0;
    int MaximumSize;
    char marker[16];

    CTRACE((tfp, "GridText: Entering HText_beginInput\n"));

    POOLtypecalloc(TextAnchor, a);

    POOLtypecalloc(FormInfo, f);
    if (a == NULL || f == NULL)
	outofmem(__FILE__, "HText_beginInput");

    a->sgml_offset = SGML_offset();
    a->inUnderline = underline;
    a->line_num = text->Lines;
    a->line_pos = text->last_line->size;

    /*
     * If this is a radio button, or an OPTION we're converting
     * to a radio button, and it's the first with this name, make
     * sure it's checked by default.  Otherwise, if it's checked,
     * uncheck the default or any preceding radio button with this
     * name that was checked.  -FM
     */
    if (I->type != NULL && !strcmp(I->type, "OPTION") &&
	HTCurSelectGroupType == F_RADIO_TYPE && LYSelectPopups == FALSE) {
	I->type = "RADIO";
	I->name = HTCurSelectGroup;
	I->name_cs = HTCurSelectGroupCharset;
    }
    if (I->name && I->type && !strcasecomp(I->type, "radio")) {
	if (!text->last_anchor) {
	    I->checked = TRUE;
	} else {
	    TextAnchor *b;
	    int i2 = 0;

	    for (b = text->first_anchor; b != NULL; b = b->next) {
		if (b->link_type == INPUT_ANCHOR &&
		    b->input_field->type == F_RADIO_TYPE &&
		    b->input_field->number == HTFormNumber) {
		    if (!strcmp(b->input_field->name, I->name)) {
			if (I->checked && b->input_field->num_value) {
			    b->input_field->num_value = 0;
			    StrAllocCopy(b->input_field->orig_value, "0");
			    break;
			}
			i2++;
		    }
		}
	    }
	    if (i2 == 0)
		I->checked = TRUE;
	}
    }

    a->next = 0;
    a->anchor = NULL;
    a->link_type = INPUT_ANCHOR;
    a->show_anchor = YES;

    LYClearHiText(a);
    a->extent = 2;

    a->input_field = f;

    f->select_list = 0;
    f->number = HTFormNumber;
    f->disabled = HTFormDisabled;
    f->no_cache = NO;

    HTFormFields++;

    /*
     * Set the no_cache flag if the METHOD is POST.  -FM
     */
    if (HTFormMethod == URL_POST_METHOD)
	f->no_cache = TRUE;

    /*
     * Set up VALUE.
     */
    if (I->value)
	StrAllocCopy(IValue, I->value);
    if (IValue &&
	HTCJK != NOCJK &&
	((I->type == NULL) || strcasecomp(I->type, "hidden"))) {
	if ((tmp = typecallocn(unsigned char, strlen(IValue) * 2 + 1)) != 0) {
	    if (kanji_code == EUC) {
		TO_EUC((unsigned char *) IValue, tmp);
		I->value_cs = current_char_set;
	    } else if (kanji_code == SJIS) {
		TO_SJIS((unsigned char *) IValue, tmp);
		I->value_cs = current_char_set;
	    } else {
		for (i = 0, j = 0; IValue[i]; i++) {
		    if (IValue[i] != CH_ESC) {	/* S/390 -- gil -- 1621 */
			tmp[j++] = IValue[i];
		    }
		}
	    }
	    StrAllocCopy(IValue, (const char *) tmp);
	    FREE(tmp);
	}
    }

    /*
     * Special case of OPTION.
     * Is handled above if radio type and LYSelectPopups is FALSE.
     */
    /* set the values and let the parsing below do the work */
    if (I->type != NULL && !strcmp(I->type, "OPTION")) {
	cp_option = I->type;
	if (HTCurSelectGroupType == F_RADIO_TYPE)
	    I->type = "OPTION_LIST";
	else
	    I->type = "CHECKBOX";
	I->name = HTCurSelectGroup;
	I->name_cs = HTCurSelectGroupCharset;

	/*
	 * The option's size parameter actually gives the length and not
	 * the width of the list.  Perform the conversion here
	 * and get rid of the allocated HTCurSelect....
	 * 0 is ok as it means any length (arbitrary decision).
	 */
	if (HTCurSelectGroupSize != NULL) {
	    f->size_l = atoi(HTCurSelectGroupSize);
	    FREE(HTCurSelectGroupSize);
	}
    }

    /*
     * Set SIZE.
     */
    if (I->size != 0) {
	f->size = I->size;
	/*
	 * Leave at zero for option lists.
	 */
	if (f->size == 0 && cp_option == NULL) {
	    f->size = 20;	/* default */
	}
    } else {
	f->size = 20;		/* default */
    }

    /*
     * Set MAXLENGTH.
     */
    if (I->maxlength != NULL) {
	f->maxlength = atoi(I->maxlength);
    } else {
	f->maxlength = 0;	/* 0 means infinite */
    }

    /*
     * Set CHECKED
     * (num_value is only relevant to check and radio types).
     */
    if (I->checked == TRUE)
	f->num_value = 1;
    else
	f->num_value = 0;

    /*
     * Set TYPE.
     */
    if (I->type != NULL) {
	if (!strcasecomp(I->type, "password")) {
	    f->type = F_PASSWORD_TYPE;
	} else if (!strcasecomp(I->type, "checkbox")) {
	    f->type = F_CHECKBOX_TYPE;
	} else if (!strcasecomp(I->type, "radio")) {
	    f->type = F_RADIO_TYPE;
	} else if (!strcasecomp(I->type, "submit")) {
	    f->type = F_SUBMIT_TYPE;
	} else if (!strcasecomp(I->type, "image")) {
	    f->type = F_IMAGE_SUBMIT_TYPE;
	} else if (!strcasecomp(I->type, "reset")) {
	    f->type = F_RESET_TYPE;
	} else if (!strcasecomp(I->type, "OPTION_LIST")) {
	    f->type = F_OPTION_LIST_TYPE;
	} else if (!strcasecomp(I->type, "hidden")) {
	    f->type = F_HIDDEN_TYPE;
	    HTFormFields--;
	    f->size = 0;
	} else if (!strcasecomp(I->type, "textarea")) {
	    f->type = F_TEXTAREA_TYPE;
	} else if (!strcasecomp(I->type, "range")) {
	    f->type = F_RANGE_TYPE;
	} else if (!strcasecomp(I->type, "file")) {
	    f->type = F_FILE_TYPE;
	    CTRACE((tfp, "ok, got a file uploader\n"));
	} else if (!strcasecomp(I->type, "keygen")) {
	    f->type = F_KEYGEN_TYPE;
	} else {
	    /*
	     * Note that TYPE="scribble" defaults to TYPE="text".  -FM
	     */
	    f->type = F_TEXT_TYPE;	/* default */
	}
    } else {
	f->type = F_TEXT_TYPE;
    }

    /*
     * Set NAME.
     */
    if (I->name != NULL) {
	StrAllocCopy(f->name, I->name);
	f->name_cs = I->name_cs;
    } else {
	if (f->type == F_RESET_TYPE ||
	    f->type == F_SUBMIT_TYPE ||
	    f->type == F_IMAGE_SUBMIT_TYPE) {
	    /*
	     * Set name to empty string.
	     */
	    StrAllocCopy(f->name, "");
	} else {
	    /*
	     * Error!  NAME must be present.
	     */
	    CTRACE((tfp,
		    "GridText: No name present in input field; not displaying\n"));
	    FREE(IValue);
	    return (0);
	}
    }

    /*
     * Add this anchor to the anchor list
     */
    if (text->last_anchor) {
	text->last_anchor->next = a;
    } else {
	text->first_anchor = a;
    }

    /*
     * Set VALUE, if it exists.  Otherwise, if it's not
     * an option list make it a zero-length string.  -FM
     */
    if (IValue != NULL) {
	/*
	 * OPTION VALUE is not actually the value to be seen but is to
	 * be sent....
	 */
	if (f->type == F_OPTION_LIST_TYPE ||
	    f->type == F_CHECKBOX_TYPE) {
	    /*
	     * Fill both with the value.  The f->value may be
	     * overwritten in HText_setLastOptionValue....
	     */
	    StrAllocCopy(f->value, IValue);
	    StrAllocCopy(f->cp_submit_value, IValue);
	} else {
	    StrAllocCopy(f->value, IValue);
	}
	f->value_cs = I->value_cs;
    } else if (f->type != F_OPTION_LIST_TYPE) {
	StrAllocCopy(f->value, "");
	/*
	 * May be an empty INPUT field.  The text entered will then
	 * probably be in the current display character set.  - kw
	 */
	f->value_cs = current_char_set;
    }

    /*
     * Run checks and fill in necessary values.
     */
    if (f->type == F_RESET_TYPE) {
	if (non_empty(f->value)) {
	    f->size = strlen(f->value);
	} else {
	    StrAllocCopy(f->value, "Reset");
	    f->size = 5;
	}
    } else if (f->type == F_IMAGE_SUBMIT_TYPE ||
	       f->type == F_SUBMIT_TYPE) {
	if (non_empty(f->value)) {
	    f->size = strlen(f->value);
	} else if (f->type == F_IMAGE_SUBMIT_TYPE) {
	    StrAllocCopy(f->value, "[IMAGE]-Submit");
	    f->size = 14;
	} else {
	    StrAllocCopy(f->value, "Submit");
	    f->size = 6;
	}
	f->submit_action = NULL;
	StrAllocCopy(f->submit_action, HTFormAction);
	if (HTFormEnctype != NULL)
	    StrAllocCopy(f->submit_enctype, HTFormEnctype);
	if (HTFormTitle != NULL)
	    StrAllocCopy(f->submit_title, HTFormTitle);
	f->submit_method = HTFormMethod;

    } else if (f->type == F_RADIO_TYPE || f->type == F_CHECKBOX_TYPE) {
	f->size = 3;
	if (IValue == NULL)
	    StrAllocCopy(f->value, (f->type == F_CHECKBOX_TYPE ? "on" : ""));

    }
    FREE(IValue);

    /*
     * Set original values.
     */
    if (f->type == F_RADIO_TYPE || f->type == F_CHECKBOX_TYPE) {
	if (f->num_value)
	    StrAllocCopy(f->orig_value, "1");
	else
	    StrAllocCopy(f->orig_value, "0");
    } else if (f->type == F_OPTION_LIST_TYPE) {
	f->orig_value = NULL;
    } else {
	StrAllocCopy(f->orig_value, f->value);
    }

    /*
     * Store accept-charset if present, converting to lowercase
     * and collapsing spaces.  - kw
     */
    if (I->accept_cs) {
	StrAllocCopy(f->accept_cs, I->accept_cs);
	LYRemoveBlanks(f->accept_cs);
	LYLowerCase(f->accept_cs);
    }

    /*
     * Add numbers to form fields if needed.  - LE & FM
     */
    switch (f->type) {
	/*
	 * Do not supply number for hidden fields, nor
	 * for types that are not yet implemented.
	 */
    case F_HIDDEN_TYPE:
#ifndef USE_FILE_UPLOAD
    case F_FILE_TYPE:
#endif
    case F_RANGE_TYPE:
    case F_KEYGEN_TYPE:
	a->number = 0;
	break;

    default:
	if (fields_are_numbered())
	    a->number = ++(text->last_anchor_number);
	else
	    a->number = 0;
	break;
    }
    if (fields_are_numbered() && (a->number > 0)) {
	sprintf(marker, "[%d]", a->number);
	adjust_marker = strlen(marker);
	if (number_fields_on_left) {
	    BOOL had_bracket = (f->type == F_OPTION_LIST_TYPE);

	    HText_appendText(text, had_bracket ? (marker + 1) : marker);
	    if (had_bracket)
		HText_appendCharacter(text, '[');
	}
	a->line_num = text->Lines;
	a->line_pos = text->last_line->size;
    } else {
	*marker = '\0';
    }

    /*
     * Restrict SIZE to maximum allowable size.
     */
    MaximumSize = WRAP_COLS(text) + 1 - adjust_marker;
    switch (f->type) {

    case F_SUBMIT_TYPE:
    case F_IMAGE_SUBMIT_TYPE:
    case F_RESET_TYPE:
    case F_TEXT_TYPE:
    case F_TEXTAREA_TYPE:
	/*
	 * For submit and reset buttons, and for text entry
	 * fields and areas, we limit the size element to that
	 * of one line for the current style because that's
	 * the most we could highlight on overwrites, and/or
	 * handle in the line editor.  The actual values for
	 * text entry lines can be long, and will be scrolled
	 * horizontally within the editing window.  -FM
	 */
	MaximumSize -= (1 +
			(int) text->style->leftIndent +
			(int) text->style->rightIndent);

	/*  If we are numbering form links, place is taken by [nn]  */
	if (fields_are_numbered()) {
	    if (!number_fields_on_left
		&& f->type == F_TEXT_TYPE
		&& MaximumSize > a->line_pos + 10)
		MaximumSize -= a->line_pos;
	    else
		MaximumSize -= strlen(marker);
	}

	/*
	 * Save value for submit/reset buttons so they
	 * will be visible when printing the page.  - LE
	 */
	I->value = f->value;
	break;

    default:
	/*
	 * For all other fields we limit the size element to
	 * 10 less than the screen width, because either they
	 * are types with small placeholders, and/or are a
	 * type which is handled via a popup window.  -FM
	 */
	MaximumSize -= 10;
	break;
    }

    if (MaximumSize < 1)
	MaximumSize = 1;

    if (f->size > MaximumSize)
	f->size = MaximumSize;

    /*
     * Add this anchor to the anchor list
     */
    text->last_anchor = a;

    if (HTCurrentForm) {	/* should always apply! - kw */
	if (!HTCurrentForm->first_field) {
	    HTCurrentForm->first_field = f;
	}
	HTCurrentForm->last_field = f;
	HTCurrentForm->nfields++;	/* will count hidden fields - kw */
	/*
	 * Propagate form field's accept-charset attribute to enclosing
	 * form if the form itself didn't have an accept-charset - kw
	 */
	if (f->accept_cs && !HTFormAcceptCharset) {
	    StrAllocCopy(HTFormAcceptCharset, f->accept_cs);
	}
	if (!text->forms) {
	    text->forms = HTList_new();
	}
    } else {
	CTRACE((tfp, "beginInput: HTCurrentForm is missing!\n"));
    }

    CTRACE((tfp, "Input link: name=%s\nvalue=%s\nsize=%d\n",
	    f->name,
	    NonNull(f->value),
	    f->size));
    CTRACE((tfp, "Input link: name_cs=%d \"%s\" (from %d \"%s\")\n",
	    f->name_cs,
	    (f->name_cs >= 0 ?
	     LYCharSet_UC[f->name_cs].MIMEname : "<UNKNOWN>"),
	    I->name_cs,
	    (I->name_cs >= 0 ?
	     LYCharSet_UC[I->name_cs].MIMEname : "<UNKNOWN>")));
    CTRACE((tfp, "            value_cs=%d \"%s\" (from %d \"%s\")\n",
	    f->value_cs,
	    (f->value_cs >= 0 ?
	     LYCharSet_UC[f->value_cs].MIMEname : "<UNKNOWN>"),
	    I->value_cs,
	    (I->value_cs >= 0 ?
	     LYCharSet_UC[I->value_cs].MIMEname : "<UNKNOWN>")));

    /*
     * Return the SIZE of the input field.
     */
    if (I->size && f->size > adjust_marker) {
	f->size -= adjust_marker;
    }
    return (f->size);
}

/*
 * If we're numbering fields on the right, do it.  Note that some fields may
 * be too long for the line - we'll lose the marker in that case rather than
 * truncate the field.
 */
void HText_endInput(HText *text)
{
    if (fields_are_numbered()
	&& !number_fields_on_left
	&& text != NULL
	&& text->last_anchor != NULL
	&& text->last_anchor->number > 0) {
	char marker[20];

	HText_setIgnoreExcess(text, FALSE);
	sprintf(marker, "[%d]", text->last_anchor->number);
	HText_appendText(text, marker);
    }
}

/*
 * Get a translation (properly:  transcoding) quality, factoring in
 * our ability to translate (an UCTQ_t) and a possible q parameter
 * on the given charset string, for cs_from -> givenmime.
 * The parsed input string will be mutilated on exit(!).
 * Note that results are not normalised to 1.0, but results from
 * different calls of this function can be compared.  - kw
 *
 * Obsolete, it was planned to use here a quality parametr UCTQ_t,
 * which is boolean now.
 */
static double get_trans_q(int cs_from,
			  char *givenmime)
{
    double df = 1.0;
    BOOL tq;
    char *p;

    if (!givenmime || !(*givenmime))
	return 0.0;
    if ((p = strchr(givenmime, ';')) != NULL) {
	*p++ = '\0';
    }
    if (!strcmp(givenmime, "*"))
	tq = UCCanTranslateFromTo(cs_from,
				  UCGetLYhndl_byMIME("utf-8"));
    else
	tq = UCCanTranslateFromTo(cs_from,
				  UCGetLYhndl_byMIME(givenmime));
    if (!tq)
	return 0.0;
    if (non_empty(p)) {
	char *pair, *field = p, *pval, *ptok;

	/* Get all the parameters to the Charset */
	while ((pair = HTNextTok(&field, ";", "\"", NULL)) != NULL) {
	    if ((ptok = HTNextTok(&pair, "= ", NULL, NULL)) != NULL &&
		(pval = HTNextField(&pair)) != NULL) {
		if (0 == strcasecomp(ptok, "q")) {
		    df = strtod(pval, NULL);
		    break;
		}
	    }
	}
	return (df * tq);
    } else {
	return tq;
    }
}

/*
 * Find the best charset for submission, if we have an ACCEPT_CHARSET
 * list.  It factors in how well we can translate (just as guess, and
 * not a very good one..) and possible ";q=" factors.  Yes this is
 * more general than it needs to be here.
 *
 * Input is cs_in and acceptstring.
 *
 * Will return charset handle as int.
 * best_csname will point to a newly allocated MIME string for the
 * charset corresponding to the return value if return value >= 0.
 * - kw
 */
static int find_best_target_cs(char **best_csname,
			       int cs_from,
			       const char *acceptstring)
{
    char *paccept = NULL;
    double bestq = -1.0;
    char *bestmime = NULL;
    char *field, *nextfield;

    StrAllocCopy(paccept, acceptstring);
    nextfield = paccept;
    while ((field = HTNextTok(&nextfield, ",", "\"", NULL)) != NULL) {
	double q;

	if (*field != '\0') {
	    /* Get the Charset */
	    q = get_trans_q(cs_from, field);
	    if (q > bestq) {
		bestq = q;
		bestmime = field;
	    }
	}
    }
    if (bestmime) {
	if (!strcmp(bestmime, "*"))	/* non-standard for HTML attribute.. */
	    StrAllocCopy(*best_csname, "utf-8");
	else
	    StrAllocCopy(*best_csname, bestmime);
	FREE(paccept);
	if (bestq > 0)
	    return (UCGetLYhndl_byMIME(*best_csname));
	else
	    return (-1);
    }
    FREE(paccept);
    return (-1);
}

#ifdef USE_FILE_UPLOAD
static void load_a_file(const char *val_used,
			bstring **result)
{
    FILE *fd;
    size_t bytes;
    char buffer[257];

    CTRACE((tfp, "Ok, about to convert %s to mime/thingy\n", val_used));

    if (*val_used) {		/* ignore empty form field */
	if ((fd = fopen(val_used, BIN_R)) == 0) {
	    HTAlert(gettext("Can't open file for uploading"));
	} else {
	    while ((bytes = fread(buffer, sizeof(char), 256, fd)) != 0) {
		HTSABCat(result, buffer, bytes);
	    }
	    LYCloseInput(fd);
	}
    }
}

static const char *guess_content_type(const char *filename)
{
    HTAtom *encoding;
    const char *desc;
    HTFormat format = HTFileFormat(filename, &encoding, &desc);

    return (format != 0 && non_empty(format->name))
	? format->name
	: "text/plain";
}
#endif /* USE_FILE_UPLOAD */

static void cannot_transcode(BOOL *had_warning,
			     const char *target_csname)
{
    if (*had_warning == NO) {
	*had_warning = YES;
	_user_message(CANNOT_TRANSCODE_FORM,
		      target_csname ? target_csname : "UNKNOWN");
	LYSleepAlert();
    }
}

#define SPECIAL_8BIT 1
#define SPECIAL_FORM 2

static unsigned check_form_specialchars(const char *value)
{
    unsigned result = 0;
    const char *p;

    for (p = value;
	 non_empty(p) && (result != (SPECIAL_8BIT | SPECIAL_FORM));
	 p++) {
	if ((*p == HT_NON_BREAK_SPACE) ||
	    (*p == HT_EN_SPACE) ||
	    (*p == LY_SOFT_HYPHEN)) {
	    result |= SPECIAL_FORM;
	} else if ((*p & 0x80) != 0) {
	    result |= SPECIAL_8BIT;
	}
    }
    return result;
}

/*
 * Scan the given data, adding characters to the MIME-boundary to keep it from
 * matching any part of the data.
 */
static void UpdateBoundary(char **Boundary,
			   bstring *data)
{
    int j;
    int have = strlen(*Boundary);
    int last = BStrLen(data);
    char *text = BStrData(data);
    char *want = *Boundary;

    for (j = 0; j <= (last - have); ++j) {
	if (want[0] == text[j]
	    && !memcmp(want, text + j, have)) {
	    char temp[2];

	    temp[0] = isdigit(text[have + j]) ? 'a' : '0';
	    temp[1] = '\0';
	    StrAllocCat(want, temp);
	    ++have;
	}
    }
    *Boundary = want;
}

/*
 * Convert a string to base64
 */
static char *convert_to_base64(const char *src,
			       int len)
{
#define B64_LINE       76

    static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *dest;
    int rlen;			/* length of result string */
    unsigned char c1, c2, c3;
    const char *eol;
    char *r;
    const char *str;
    int eollen;
    int chunk;

    str = src;
    eol = "\n";
    eollen = 1;

    /* calculate the length of the result */
    rlen = (len + 2) / 3 * 4;	/* encoded bytes */
    if (rlen) {
	/* add space for EOL */
	rlen += ((rlen - 1) / B64_LINE + 1) * eollen;
    }

    /* allocate a result buffer */
    if ((dest = (char *) malloc(rlen + 1)) == NULL) {
	outofmem(__FILE__, "convert_to_base64");
    }
    r = dest;

    /* encode */
    for (chunk = 0; len > 0; len -= 3, chunk++) {
	if (chunk == (B64_LINE / 4)) {
	    const char *c = eol;
	    const char *e = eol + eollen;

	    while (c < e)
		*r++ = *c++;
	    chunk = 0;
	}
	c1 = *str++;
	c2 = *str++;
	*r++ = basis_64[c1 >> 2];
	*r++ = basis_64[((c1 & 0x3) << 4) | ((c2 & 0xF0) >> 4)];
	if (len > 2) {
	    c3 = *str++;
	    *r++ = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >> 6)];
	    *r++ = basis_64[c3 & 0x3F];
	} else if (len == 2) {
	    *r++ = basis_64[(c2 & 0xF) << 2];
	    *r++ = '=';
	} else {		/* len == 1 */
	    *r++ = '=';
	    *r++ = '=';
	}
    }
    if (rlen) {
	/* append eol to the result string */
	const char *c = eol;
	const char *e = eol + eollen;

	while (c < e)
	    *r++ = *c++;
    }
    *r = '\0';

    return dest;
}

typedef enum {
    NO_QUOTE			/* no quoting needed */
    ,QUOTE_MULTI		/* multipart */
    ,QUOTE_BASE64		/* encode as base64 */
    ,QUOTE_SPECIAL		/* escape special characters only */
} QuoteData;

typedef struct {
    int type;			/* the type of this field */
    BOOL first;			/* true if this begins a submission part */
    char *name;			/* the name of this field */
    char *value;		/* the nominal value of this field */
    bstring *data;		/* its data, which is usually the same as the value */
    QuoteData quote;		/* how to quote/translate the data */
} PostData;

static char *escape_or_quote_name(const char *name,
				  QuoteData quoting,
				  const char *MultipartContentType)
{
    char *escaped1 = NULL;

    switch (quoting) {
    case NO_QUOTE:
	StrAllocCopy(escaped1, name);
	break;
    case QUOTE_MULTI:
    case QUOTE_BASE64:
	StrAllocCopy(escaped1, "Content-Disposition: form-data");
	HTSprintf(&escaped1, "; name=\"%s\"", name);
	if (MultipartContentType)
	    HTSprintf(&escaped1, MultipartContentType, "text/plain");
	if (quoting == QUOTE_BASE64)
	    StrAllocCat(escaped1, "\r\nContent-Transfer-Encoding: base64");
	StrAllocCat(escaped1, "\r\n\r\n");
	break;
    case QUOTE_SPECIAL:
	escaped1 = HTEscapeSP(name, URL_XALPHAS);
	break;
    }
    return escaped1;
}

static char *escape_or_quote_value(const char *value,
				   QuoteData quoting)
{
    char *escaped2 = NULL;

    switch (quoting) {
    case NO_QUOTE:
    case QUOTE_MULTI:
	StrAllocCopy(escaped2, NonNull(value));
	break;
    case QUOTE_BASE64:
	/* FIXME: this is redundant */
	escaped2 = convert_to_base64(value, strlen(value));
	break;
    case QUOTE_SPECIAL:
	escaped2 = HTEscapeSP(value, URL_XALPHAS);
	break;
    }
    return escaped2;
}

/*
 * Check if we should encode the data in base64.  We can, only if we're using
 * a multipart content type.  We should, if we're sending mail and the data
 * contains long lines or nonprinting characters.
 */
static int check_if_base64_needed(int submit_method,
				  bstring *data)
{
    int width = 0;
    BOOL printable = TRUE;
    char *text = BStrData(data);

    if (text != 0) {
	int col = 0;
	int n;
	int length = BStrLen(data);

	for (n = 0; n < length; ++n) {
	    int ch = UCH(text[n]);

	    if (is8bits(ch) || ((ch < 32 && ch != '\n'))) {
		CTRACE((tfp, "nonprintable %d:%#x\n", n, ch));
		printable = FALSE;
	    }
	    if (ch == '\n' || ch == '\r') {
		if (width < col)
		    width = col;
		col = 0;
	    } else {
		++col;
	    }
	}
	if (width < col)
	    width = col;
    }
    return !printable && ((submit_method == URL_MAIL_METHOD) && (width > 72));
}

/*
 * HText_SubmitForm - generate submit data from form fields.
 * For mailto forms, send the data.
 * For other methods, set fields in structure pointed to by doc
 * appropriately for next request.
 * Returns 1 if *doc set appropriately for next request,
 * 0 otherwise.  - kw
 */
int HText_SubmitForm(FormInfo * submit_item, DocInfo *doc, char *link_name,
		     char *link_value)
{
    BOOL had_chartrans_warning = NO;
    BOOL have_accept_cs = NO;
    BOOL success;
    BOOLEAN PlainText = FALSE;
    BOOLEAN SemiColon = FALSE;
    BOOL skip_field = FALSE;
    const char *out_csname;
    const char *target_csname = NULL;
    PerFormInfo *thisform;
    PostData *my_data = NULL;
    TextAnchor *anchor_ptr;
    bstring *my_query = NULL;
    char *Boundary = NULL;
    char *MultipartContentType = NULL;
    char *content_type_out = NULL;
    char *copied_name_used = NULL;
    char *copied_val_used = NULL;
    char *escaped1 = NULL;
    char *escaped2 = NULL;
    char *last_textarea_name = NULL;
    const char *name_used = "";
    char *previous_blanks = NULL;
    const char *val_used = "";
    int anchor_count = 0;
    int anchor_limit = 0;
    int form_number = submit_item->number;
    int result = 0;
    int target_cs = -1;
    int textarea_lineno = 0;
    unsigned form_is_special = 0;

    CTRACE((tfp, "SubmitForm\n  link_name=%s\n  link_value=%s\n", link_name, link_value));
    if (!HTMainText)
	return 0;

    thisform = (PerFormInfo *) HTList_objectAt(HTMainText->forms, form_number
					       - 1);
    /*  Sanity check */
    if (!thisform) {
	CTRACE((tfp, "SubmitForm: form %d not in HTMainText's list!\n",
		form_number));
    } else if (thisform->number != form_number) {
	CTRACE((tfp, "SubmitForm: failed sanity check, %d!=%d !\n",
		thisform->number, form_number));
	thisform = NULL;
    }

    if (isEmpty(submit_item->submit_action)) {
	CTRACE((tfp, "SubmitForm: no action given\n"));
	return 0;
    }

    /*
     * If we're mailing, make sure it's a mailto ACTION.  -FM
     */
    if ((submit_item->submit_method == URL_MAIL_METHOD) &&
	!isMAILTO_URL(submit_item->submit_action)) {
	HTAlert(BAD_FORM_MAILTO);
	return 0;
    }

    /*
     * Check the ENCTYPE and set up the appropriate variables.  -FM
     */
    if (submit_item->submit_enctype &&
	!strncasecomp(submit_item->submit_enctype, "text/plain", 10)) {
	/*
	 * Do not hex escape, and use physical newlines
	 * to separate name=value pairs.  -FM
	 */
	PlainText = TRUE;
    } else if (submit_item->submit_enctype &&
	       !strncasecomp(submit_item->submit_enctype,
			     "application/sgml-form-urlencoded", 32)) {
	/*
	 * Use semicolons instead of ampersands as the
	 * separators for name=value pairs.  -FM
	 */
	SemiColon = TRUE;
    } else if (submit_item->submit_enctype &&
	       !strncasecomp(submit_item->submit_enctype,
			     "multipart/form-data", 19)) {
	/*
	 * Use the multipart MIME format.  Later we will ensure it does not
	 * occur within the content.
	 */
	Boundary = "xnyLAaB03X";
    }

    /*
     * Determine in what character encoding (aka.  charset) we should
     * submit.  We call this target_cs and the MIME name for it
     * target_csname.
     * TODO:   - actually use ACCEPT-CHARSET stuff from FORM
     * TODO:   - deal with list in ACCEPT-CHARSET, find a "best"
     *           charset to submit
     */

    /* Look at ACCEPT-CHARSET on the submitting field if we have one. */
    if (thisform && submit_item->accept_cs &&
	strcasecomp(submit_item->accept_cs, "UNKNOWN")) {
	have_accept_cs = YES;
	target_cs = find_best_target_cs(&thisform->thisacceptcs,
					current_char_set,
					submit_item->accept_cs);
    }
    /* Look at ACCEPT-CHARSET on form as a whole if submitting field
     * didn't have one. */
    if (thisform && !have_accept_cs && thisform->accept_cs &&
	strcasecomp(thisform->accept_cs, "UNKNOWN")) {
	have_accept_cs = YES;
	target_cs = find_best_target_cs(&thisform->thisacceptcs,
					current_char_set,
					thisform->accept_cs);
    }
    if (have_accept_cs && (target_cs >= 0) && thisform->thisacceptcs) {
	target_csname = thisform->thisacceptcs;
    }

    if (target_cs < 0 &&
	HTMainText->node_anchor->charset &&
	*HTMainText->node_anchor->charset) {
	target_cs = UCGetLYhndl_byMIME(HTMainText->node_anchor->charset);
	if (target_cs >= 0) {
	    target_csname = HTMainText->node_anchor->charset;
	} else {
	    target_cs = UCLYhndl_for_unspec;	/* always >= 0 */
	    target_csname = LYCharSet_UC[target_cs].MIMEname;
	}
    }
    if (target_cs < 0) {
	target_cs = UCLYhndl_for_unspec;	/* always >= 0 */
    }

    /*
     * Go through list of anchors and get a "max." charset parameter - kw
     */
    for (anchor_ptr = HTMainText->first_anchor;
	 anchor_ptr != NULL;
	 anchor_ptr = anchor_ptr->next) {

	if (anchor_ptr->link_type != INPUT_ANCHOR)
	    continue;

	if (anchor_ptr->input_field->number == form_number &&
	    !anchor_ptr->input_field->disabled) {

	    FormInfo *form_ptr = anchor_ptr->input_field;
	    char *val = (form_ptr->cp_submit_value != NULL
			 ? form_ptr->cp_submit_value
			 : form_ptr->value);

	    unsigned field_is_special = check_form_specialchars(val);
	    unsigned name_is_special = check_form_specialchars(form_ptr->name);

	    form_is_special = (field_is_special | name_is_special);

	    if (field_is_special == 0) {
		/* already ok */
	    } else if (target_cs < 0) {
		/* already confused */
	    } else if ((field_is_special & SPECIAL_8BIT) == 0
		       && (LYCharSet_UC[target_cs].enc == UCT_ENC_8859
			   || (LYCharSet_UC[target_cs].like8859 & UCT_R_8859SPECL))) {
		/* those specials will be trivial */
	    } else if (UCNeedNotTranslate(form_ptr->value_cs, target_cs)) {
		/* already ok */
	    } else if (UCCanTranslateFromTo(form_ptr->value_cs, target_cs)) {
		/* also ok */
	    } else if (UCCanTranslateFromTo(target_cs, form_ptr->value_cs)) {
		target_cs = form_ptr->value_cs;		/* try this */
		target_csname = NULL;	/* will be set after loop */
	    } else {
		target_cs = -1;	/* don't know what to do */
	    }

	    /*  Same for name */
	    if (name_is_special == 0) {
		/* already ok */
	    } else if (target_cs < 0) {
		/* already confused */
	    } else if ((name_is_special & SPECIAL_8BIT) == 0
		       && (LYCharSet_UC[target_cs].enc == UCT_ENC_8859
			   || (LYCharSet_UC[target_cs].like8859 & UCT_R_8859SPECL))) {
		/* those specials will be trivial */
	    } else if (UCNeedNotTranslate(form_ptr->name_cs, target_cs)) {
		/* already ok */
	    } else if (UCCanTranslateFromTo(form_ptr->name_cs, target_cs)) {
		/* also ok */
	    } else if (UCCanTranslateFromTo(target_cs, form_ptr->name_cs)) {
		target_cs = form_ptr->value_cs;		/* try this */
		target_csname = NULL;	/* will be set after loop */
	    } else {
		target_cs = -1;	/* don't know what to do */
	    }

	    ++anchor_limit;
	} else if (anchor_ptr->input_field->number > form_number) {
	    break;
	}
    }

    /*
     * If we have input fields (we expect this), make an array of them so we
     * can organize the data.
     */
    if (anchor_limit != 0) {
	my_data = typecallocn(PostData, anchor_limit);
	if (my_data == 0)
	    outofmem(__FILE__, "HText_SubmitForm");
    }

    if (target_csname == NULL && target_cs >= 0) {
	if ((form_is_special & SPECIAL_8BIT) != 0) {
	    target_csname = LYCharSet_UC[target_cs].MIMEname;
	} else if ((form_is_special & SPECIAL_FORM) != 0) {
	    target_csname = LYCharSet_UC[target_cs].MIMEname;
	} else {
	    target_csname = "us-ascii";
	}
    }

    if (submit_item->submit_method == URL_GET_METHOD && Boundary == NULL) {
	char *temp = NULL;

	StrAllocCopy(temp, submit_item->submit_action);
	/*
	 * Method is GET.  Clip out any anchor in the current URL.
	 */
	strtok(temp, "#");
	/*
	 * Clip out any old query in the current URL.
	 */
	strtok(temp, "?");
	/*
	 * Add the lead question mark for the new URL.
	 */
	StrAllocCat(temp, "?");
	BStrCat0(my_query, temp);
    } else {
	/*
	 * We are submitting POST content to a server,
	 * so load content_type_out.  This will be put in
	 * the post_content_type element if all goes well.  -FM, kw
	 */
	if (SemiColon == TRUE) {
	    StrAllocCopy(content_type_out,
			 "application/sgml-form-urlencoded");
	} else if (PlainText == TRUE) {
	    StrAllocCopy(content_type_out,
			 "text/plain");
	} else if (Boundary != NULL) {
	    StrAllocCopy(content_type_out,
			 "multipart/form-data");
	} else {
	    StrAllocCopy(content_type_out,
			 "application/x-www-form-urlencoded");
	}

	/*
	 * If the ENCTYPE is not multipart/form-data, append the
	 * charset we'll be sending to the post_content_type, IF
	 *  (1) there was an explicit accept-charset attribute, OR
	 *  (2) we have 8-bit or special chars, AND the document had
	 *      an explicit (recognized and accepted) charset parameter,
	 *      AND it or target_csname is different from iso-8859-1,
	 *      OR
	 *  (3) we have 8-bit or special chars, AND the document had
	 *      no explicit (recognized and accepted) charset parameter,
	 *      AND target_cs is different from the currently effective
	 *      assumed charset (which should have been set by the user
	 *      so that it reflects what the server is sending, if the
	 *      document is rendered correctly).
	 * For multipart/form-data the equivalent will be done later,
	 * separately for each form field.  - kw
	 */
	if (have_accept_cs
	    || ((form_is_special & SPECIAL_8BIT) != 0
		|| (form_is_special & SPECIAL_FORM) != 0)) {
	    if (target_cs >= 0 && target_csname) {
		if (Boundary == NULL) {
		    if ((HTMainText->node_anchor->charset &&
			 (strcmp(HTMainText->node_anchor->charset,
				 "iso-8859-1") ||
			  strcmp(target_csname, "iso-8859-1"))) ||
			(!HTMainText->node_anchor->charset &&
			 target_cs != UCLYhndl_for_unspec)) {
			HTSprintf(&content_type_out, "; charset=%s", target_csname);
		    }
		}
	    } else {
		cannot_transcode(&had_chartrans_warning, target_csname);
	    }
	}
    }

    out_csname = target_csname;

    /*
     * Build up a list of the input fields and their associated values.
     */
    for (anchor_ptr = HTMainText->first_anchor;
	 anchor_ptr != NULL;
	 anchor_ptr = anchor_ptr->next) {

	if (anchor_ptr->link_type != INPUT_ANCHOR)
	    continue;

	if (anchor_ptr->input_field->number == form_number &&
	    !anchor_ptr->input_field->disabled) {

	    FormInfo *form_ptr = anchor_ptr->input_field;
	    int out_cs;
	    QuoteData quoting = (PlainText
				 ? NO_QUOTE
				 : (Boundary
				    ? QUOTE_MULTI
				    : QUOTE_SPECIAL));

	    if (form_ptr->type != F_TEXTAREA_TYPE)
		textarea_lineno = 0;

	    CTRACE((tfp, "SubmitForm[%d/%d]: ",
		    anchor_count + 1, anchor_limit));

	    name_used = NonNull(form_ptr->name);

	    switch (form_ptr->type) {
	    case F_RESET_TYPE:
		CTRACE((tfp, "reset\n"));
		break;
#ifdef USE_FILE_UPLOAD
	    case F_FILE_TYPE:
		val_used = NonNull(form_ptr->value);
		CTRACE((tfp, "I will submit %s (from %s)\n",
			val_used, name_used));
		break;
#endif
	    case F_SUBMIT_TYPE:
	    case F_TEXT_SUBMIT_TYPE:
	    case F_IMAGE_SUBMIT_TYPE:
		if (!(non_empty(form_ptr->name) &&
		      !strcmp(form_ptr->name, link_name))) {
		    CTRACE((tfp, "skipping submit field with "));
		    CTRACE((tfp, "name \"%s\" for link_name \"%s\", %s.\n",
			    form_ptr->name ? form_ptr->name : "???",
			    link_name ? link_name : "???",
			    non_empty(form_ptr->name) ?
			    "not current link" : "no field name"));
		    break;
		}
		if (!(form_ptr->type == F_TEXT_SUBMIT_TYPE ||
		      (non_empty(form_ptr->value) &&
		       !strcmp(form_ptr->value, link_value)))) {
		    CTRACE((tfp, "skipping submit field with "));
		    CTRACE((tfp, "name \"%s\" for link_name \"%s\", %s!\n",
			    form_ptr->name ? form_ptr->name : "???",
			    link_name ? link_name : "???",
			    "values are different"));
		    break;
		}
		/* FALLTHRU */
	    case F_RADIO_TYPE:
	    case F_CHECKBOX_TYPE:
	    case F_TEXTAREA_TYPE:
	    case F_PASSWORD_TYPE:
	    case F_TEXT_TYPE:
	    case F_OPTION_LIST_TYPE:
	    case F_HIDDEN_TYPE:
		/*
		 * Be sure to actually look at the option submit value.
		 */
		if (form_ptr->cp_submit_value != NULL) {
		    val_used = form_ptr->cp_submit_value;
		} else {
		    val_used = form_ptr->value;
		}

		/*
		 * Charset-translate value now, because we need to know the
		 * charset parameter for multipart bodyparts.  - kw
		 */
		if (check_form_specialchars(val_used) != 0) {
		    /*  We should translate back. */
		    StrAllocCopy(copied_val_used, val_used);
		    success = LYUCTranslateBackFormData(&copied_val_used,
							form_ptr->value_cs,
							target_cs, PlainText);
		    CTRACE((tfp, "field \"%s\" %d %s -> %d %s %s\n",
			    NonNull(form_ptr->name),
			    form_ptr->value_cs,
			    ((form_ptr->value_cs >= 0)
			     ? LYCharSet_UC[form_ptr->value_cs].MIMEname
			     : "???"),
			    target_cs,
			    target_csname ? target_csname : "???",
			    success ? "OK" : "FAILED"));
		    if (success) {
			val_used = copied_val_used;
		    }
		} else {	/* We can use the value directly. */
		    CTRACE((tfp, "field \"%s\" %d %s OK\n",
			    NonNull(form_ptr->name),
			    target_cs,
			    target_csname ? target_csname : "???"));
		    success = YES;
		}
		if (!success) {
		    cannot_transcode(&had_chartrans_warning, target_csname);
		    out_cs = form_ptr->value_cs;
		} else {
		    out_cs = target_cs;
		}
		if (out_cs >= 0)
		    out_csname = LYCharSet_UC[out_cs].MIMEname;
		if (Boundary) {
		    StrAllocCopy(MultipartContentType,
				 "\r\nContent-Type: %s");
		    if (!success && form_ptr->value_cs < 0) {
			/*  This is weird. */
			out_csname = "UNKNOWN-8BIT";
		    } else if (!success) {
			target_csname = NULL;
		    } else {
			if (!target_csname) {
			    target_csname = LYCharSet_UC[target_cs].MIMEname;
			}
		    }
		    if (strcmp(out_csname, "iso-8859-1"))
			HTSprintf(&MultipartContentType, "; charset=%s", out_csname);
		}

		/*
		 * Charset-translate name now, because we need to know the
		 * charset parameter for multipart bodyparts.  - kw
		 */
		if (form_ptr->type == F_TEXTAREA_TYPE) {
		    textarea_lineno++;
		    if (textarea_lineno > 1 &&
			last_textarea_name && form_ptr->name &&
			!strcmp(last_textarea_name, form_ptr->name)) {
			break;
		    }
		}

		if (check_form_specialchars(name_used) != 0) {
		    /*  We should translate back. */
		    StrAllocCopy(copied_name_used, name_used);
		    success = LYUCTranslateBackFormData(&copied_name_used,
							form_ptr->name_cs,
							target_cs, PlainText);
		    CTRACE((tfp, "name \"%s\" %d %s -> %d %s %s\n",
			    NonNull(form_ptr->name),
			    form_ptr->name_cs,
			    ((form_ptr->name_cs >= 0)
			     ? LYCharSet_UC[form_ptr->name_cs].MIMEname
			     : "???"),
			    target_cs,
			    target_csname ? target_csname : "???",
			    success ? "OK" : "FAILED"));
		    if (success) {
			name_used = copied_name_used;
		    }
		    if (Boundary) {
			if (!success) {
			    StrAllocCopy(MultipartContentType, "");
			    target_csname = NULL;
			} else {
			    if (!target_csname)
				target_csname = LYCharSet_UC[target_cs].MIMEname;
			}
		    }
		} else {	/* We can use the name directly. */
		    CTRACE((tfp, "name \"%s\" %d %s OK\n",
			    NonNull(form_ptr->name),
			    target_cs,
			    target_csname ? target_csname : "???"));
		    success = YES;
		    if (Boundary) {
			StrAllocCopy(copied_name_used, name_used);
		    }
		}
		if (!success) {
		    cannot_transcode(&had_chartrans_warning, target_csname);
		}
		if (Boundary) {
		    /*
		     * According to RFC 1867, Non-ASCII field names
		     * "should be encoded according to the prescriptions
		     * of RFC 1522 [...].  I don't think RFC 1522 actually
		     * is meant to apply to parameters like this, and it
		     * is unknown whether any server would make sense of
		     * it, so for now just use some quoting/escaping and
		     * otherwise leave 8-bit values as they are.
		     * Non-ASCII characters in form field names submitted
		     * as multipart/form-data can only occur if the form
		     * provider specifically asked for it anyway.  - kw
		     */
		    HTMake822Word(&copied_name_used, FALSE);
		    name_used = copied_name_used;
		}

		break;
	    default:
		CTRACE((tfp, "What type is %d?\n", form_ptr->type));
		break;
	    }

	    skip_field = FALSE;
	    my_data[anchor_count].first = TRUE;
	    my_data[anchor_count].type = form_ptr->type;

	    /*
	     * Using the values of 'name_used' and 'val_used' computed in the
	     * previous case-statement, compute the 'first' and 'data' values
	     * for the current input field.
	     */
	    switch (form_ptr->type) {

	    default:
		skip_field = TRUE;
		break;

#ifdef USE_FILE_UPLOAD
	    case F_FILE_TYPE:
		load_a_file(val_used, &(my_data[anchor_count].data));
		break;
#endif /* USE_FILE_UPLOAD */

	    case F_SUBMIT_TYPE:
	    case F_TEXT_SUBMIT_TYPE:
	    case F_IMAGE_SUBMIT_TYPE:
		if ((non_empty(form_ptr->name) &&
		     !strcmp(form_ptr->name, link_name)) &&
		    (form_ptr->type == F_TEXT_SUBMIT_TYPE ||
		     (non_empty(form_ptr->value) &&
		      !strcmp(form_ptr->value, link_value)))) {
		    ;
		} else {
		    skip_field = TRUE;
		}
		break;

	    case F_RADIO_TYPE:
	    case F_CHECKBOX_TYPE:
		/*
		 * Only add if selected.
		 */
		if (form_ptr->num_value) {
		    ;
		} else {
		    skip_field = TRUE;
		}
		break;

	    case F_TEXTAREA_TYPE:
		if (!last_textarea_name ||
		    strcmp(last_textarea_name, form_ptr->name)) {
		    textarea_lineno = 1;
		    last_textarea_name = form_ptr->name;
		} else {
		    my_data[anchor_count].first = FALSE;
		}
		break;

	    case F_PASSWORD_TYPE:
	    case F_TEXT_TYPE:
	    case F_OPTION_LIST_TYPE:
	    case F_HIDDEN_TYPE:
		break;
	    }

	    /*
	     * If we did not decide to skip the current field, populate the
	     * values in the array for it.
	     */
	    if (!skip_field) {
		StrAllocCopy(my_data[anchor_count].name, name_used);
		StrAllocCopy(my_data[anchor_count].value, val_used);
		if (my_data[anchor_count].data == 0)
		    BStrCat0(my_data[anchor_count].data, val_used);
		my_data[anchor_count].quote = quoting;
		if (quoting == QUOTE_MULTI
		    && check_if_base64_needed(submit_item->submit_method,
					      my_data[anchor_count].data)) {
		    CTRACE((tfp, "will encode as base64\n"));
		    my_data[anchor_count].quote = QUOTE_BASE64;
		    escaped2 =
			convert_to_base64(BStrData(my_data[anchor_count].data),
					  BStrLen(my_data[anchor_count].data));
		    BStrCopy0(my_data[anchor_count].data, escaped2);
		    FREE(escaped2);
		}
	    }
	    ++anchor_count;

	    FREE(copied_name_used);
	    FREE(copied_val_used);

	} else if (anchor_ptr->input_field->number > form_number) {
	    break;
	}
    }

    FREE(copied_name_used);

    if (my_data != 0) {
	BOOL first_one = TRUE;

	/*
	 * If we're using a MIME-boundary, make it unique.
	 */
	if (content_type_out != 0 && Boundary != 0) {
	    Boundary = 0;
	    StrAllocCopy(Boundary, "LYNX");
	    for (anchor_count = 0; anchor_count < anchor_limit; ++anchor_count) {
		if (my_data[anchor_count].data != 0) {
		    UpdateBoundary(&Boundary, my_data[anchor_count].data);
		}
	    }
	    HTSprintf(&content_type_out, "; boundary=%s", Boundary);
	}

	for (anchor_count = 0; anchor_count < anchor_limit; ++anchor_count) {

	    if (my_data[anchor_count].name != 0
		&& my_data[anchor_count].value != 0) {

		CTRACE((tfp,
			"processing [%d:%d] name=%s(first:%d, value=%s, data=%p)\n",
			anchor_count + 1,
			anchor_limit,
			NonNull(my_data[anchor_count].name),
			my_data[anchor_count].first,
			NonNull(my_data[anchor_count].value),
			my_data[anchor_count].data));

		if (my_data[anchor_count].first) {
		    if (first_one) {
			if (Boundary) {
			    HTBprintf(&my_query, "--%s\r\n", Boundary);
			}
			first_one = FALSE;
		    } else {
			if (PlainText) {
			    BStrCat0(my_query, "\n");
			} else if (SemiColon) {
			    BStrCat0(my_query, ";");
			} else if (Boundary) {
			    HTBprintf(&my_query, "\r\n--%s\r\n", Boundary);
			} else {
			    BStrCat0(my_query, "&");
			}
		    }
		}

		/* append a null to the string */
		HTSABCat(&(my_data[anchor_count].data), "", 1);
		name_used = my_data[anchor_count].name;
		val_used = my_data[anchor_count].value;

	    } else {
		/* there is no data to send */
		continue;
	    }

	    switch (my_data[anchor_count].type) {
	    case F_TEXT_TYPE:
	    case F_PASSWORD_TYPE:
	    case F_OPTION_LIST_TYPE:
	    case F_HIDDEN_TYPE:
		escaped1 = escape_or_quote_name(my_data[anchor_count].name,
						my_data[anchor_count].quote,
						MultipartContentType);

		escaped2 = escape_or_quote_value(val_used,
						 my_data[anchor_count].quote);

		HTBprintf(&my_query,
			  "%s%s%s%s%s",
			  escaped1,
			  (Boundary ? "" : "="),
			  (PlainText ? "\n" : ""),
			  escaped2,
			  ((PlainText && *escaped2) ? "\n" : ""));
		break;
	    case F_CHECKBOX_TYPE:
	    case F_RADIO_TYPE:
		escaped1 = escape_or_quote_name(my_data[anchor_count].name,
						my_data[anchor_count].quote,
						MultipartContentType);

		escaped2 = escape_or_quote_value(val_used,
						 my_data[anchor_count].quote);

		HTBprintf(&my_query,
			  "%s%s%s%s%s",
			  escaped1,
			  (Boundary ? "" : "="),
			  (PlainText ? "\n" : ""),
			  escaped2,
			  ((PlainText && *escaped2) ? "\n" : ""));
		break;
	    case F_SUBMIT_TYPE:
	    case F_TEXT_SUBMIT_TYPE:
	    case F_IMAGE_SUBMIT_TYPE:
		/*
		 * If it has a non-zero length name (e.g., because
		 * its IMAGE_SUBMIT_TYPE is to be handled homologously
		 * to an image map, or a SUBMIT_TYPE in a set of
		 * multiple submit buttons, or a single type="text"
		 * that's been converted to a TEXT_SUBMIT_TYPE),
		 * include the name=value pair, or fake name.x=0 and
		 * name.y=0 pairs for IMAGE_SUBMIT_TYPE.  -FM
		 */
		escaped1 = escape_or_quote_name(my_data[anchor_count].name,
						my_data[anchor_count].quote,
						MultipartContentType);

		escaped2 = escape_or_quote_value(val_used,
						 my_data[anchor_count].quote);

		if (my_data[anchor_count].type == F_IMAGE_SUBMIT_TYPE) {
		    /*
		     * It's a clickable image submit button.  Fake a 0,0
		     * coordinate pair, which typically returns the image's
		     * default.  -FM
		     */
		    if (Boundary) {
			*(strchr(escaped1, '=') + 1) = '\0';
			HTBprintf(&my_query,
				  "%s\"%s.x\"\r\n\r\n0\r\n--%s\r\n%s\"%s.y\"\r\n\r\n0",
				  escaped1,
				  my_data[anchor_count].name,
				  Boundary,
				  escaped1,
				  my_data[anchor_count].name);
		    } else {
			HTBprintf(&my_query,
				  "%s.x=0%s%s.y=0%s",
				  escaped1,
				  (PlainText ?
				   "\n" : (SemiColon ?
					   ";" : "&")),
				  escaped1,
				  ((PlainText && *escaped1) ?
				   "\n" : ""));
		    }
		} else {
		    /*
		     * It's a standard submit button.  Use the name=value
		     * pair.  = FM
		     */
		    HTBprintf(&my_query,
			      "%s%s%s%s%s",
			      escaped1,
			      (Boundary ? "" : "="),
			      (PlainText ? "\n" : ""),
			      escaped2,
			      ((PlainText && *escaped2) ? "\n" : ""));
		}
		break;
	    case F_RESET_TYPE:
		/* ignore */
		break;
	    case F_TEXTAREA_TYPE:
		escaped2 = escape_or_quote_value(val_used,
						 my_data[anchor_count].quote);

		if (my_data[anchor_count].first) {
		    textarea_lineno = 1;
		    /*
		     * Names are different so this is the first textarea or a
		     * different one from any before it.
		     */
		    if (PlainText) {
			FREE(previous_blanks);
		    } else if (Boundary) {
			StrAllocCopy(previous_blanks, "\r\n");
		    } else {
			StrAllocCopy(previous_blanks, "%0d%0a");
		    }
		    escaped1 = escape_or_quote_name(name_used,
						    my_data[anchor_count].quote,
						    MultipartContentType);

		    HTBprintf(&my_query,
			      "%s%s%s%s%s",
			      escaped1,
			      (Boundary ? "" : "="),
			      (PlainText ? "\n" : ""),
			      escaped2,
			      ((PlainText && *escaped2) ? "\n" : ""));
		} else {
		    const char *marker = (PlainText
					  ? "\n"
					  : (Boundary
					     ? "\r\n"
					     : "%0d%0a"));

		    /*
		     * This is a continuation of a previous textarea.
		     */
		    if (escaped2[0] != '\0') {
			if (previous_blanks) {
			    BStrCat0(my_query, previous_blanks);
			    FREE(previous_blanks);
			}
			BStrCat0(my_query, escaped2);
			if (PlainText || Boundary)
			    BStrCat0(my_query, marker);
			else
			    StrAllocCopy(previous_blanks, marker);
		    } else {
			StrAllocCat(previous_blanks, marker);
		    }
		}
		break;
	    case F_RANGE_TYPE:
		/* not implemented */
		break;
#ifdef USE_FILE_UPLOAD
	    case F_FILE_TYPE:
		if (PlainText) {
		    StrAllocCopy(escaped1, my_data[anchor_count].name);
		} else if (Boundary) {
		    const char *t = guess_content_type(val_used);

		    StrAllocCopy(escaped1, "Content-Disposition: form-data");
		    HTSprintf(&escaped1, "; name=\"%s\"",
			      my_data[anchor_count].name);
		    HTSprintf(&escaped1, "; filename=\"%s\"", val_used);
		    /* Should we take into account the encoding? */
		    HTSprintf(&escaped1, "\r\nContent-Type: %s", t);
		    if (my_data[anchor_count].quote == QUOTE_BASE64)
			StrAllocCat(escaped1,
				    "\r\nContent-Transfer-Encoding: base64");
		    StrAllocCat(escaped1, "\r\n\r\n");
		} else {
		    escaped1 = HTEscapeSP(my_data[anchor_count].name, URL_XALPHAS);
		}

		HTBprintf(&my_query,
			  "%s%s%s",
			  escaped1,
			  (Boundary ? "" : "="),
			  (PlainText ? "\n" : ""));
		/*
		 * If we have anything more than the trailing null we added,
		 * append the file-data to the query.
		 */
		if (BStrLen(my_data[anchor_count].data) > 1) {
		    HTSABCat(&my_query,
			     BStrData(my_data[anchor_count].data),
			     BStrLen(my_data[anchor_count].data) - 1);
		    if (PlainText)
			HTBprintf(&my_query, "\n");
		}
		break;
#endif /* USE_FILE_UPLOAD */
	    case F_KEYGEN_TYPE:
		/* not implemented */
		break;
	    }

	    FREE(escaped1);
	    FREE(escaped2);
	}
	if (Boundary) {
	    HTBprintf(&my_query, "\r\n--%s--\r\n", Boundary);
	}
	/*
	 * The data may contain a null - so we use fwrite().
	 */
	if (TRACE) {
	    CTRACE((tfp, "Query %d{", BStrLen(my_query)));
	    trace_bstring(my_query);
	    CTRACE((tfp, "}\n"));
	}
    }

    if (submit_item->submit_method == URL_MAIL_METHOD) {
	HTUserMsg2(gettext("Submitting %s"), submit_item->submit_action);
	HTSABCat(&my_query, "", 1);	/* append null */
	mailform((submit_item->submit_action + 7),
		 (isEmpty(submit_item->submit_title)
		  ? NonNull(HText_getTitle())
		  : submit_item->submit_title),
		 BStrData(my_query),
		 content_type_out);
	result = 0;
	BStrFree(my_query);
	FREE(content_type_out);
    } else {
	_statusline(SUBMITTING_FORM);

	if (submit_item->submit_method == URL_POST_METHOD || Boundary) {
	    LYFreePostData(doc);
	    doc->post_data = my_query;
	    doc->post_content_type = content_type_out;	/* don't free c_t_out */
	    CTRACE((tfp, "GridText - post_data set:\n%s\n", content_type_out));
	    StrAllocCopy(doc->address, submit_item->submit_action);
	} else {		/* GET_METHOD */
	    HTSABCat(&my_query, "", 1);		/* append null */
	    StrAllocCopy(doc->address, BStrData(my_query));	/* FIXME? */
	    LYFreePostData(doc);
	    FREE(content_type_out);
	}
	result = 1;
    }

    FREE(MultipartContentType);
    FREE(previous_blanks);
    FREE(Boundary);
    if (my_data != 0) {
	for (anchor_count = 0; anchor_count < anchor_limit; ++anchor_count) {
	    FREE(my_data[anchor_count].name);
	    FREE(my_data[anchor_count].value);
	    BStrFree(my_data[anchor_count].data);
	}
	FREE(my_data);
    }

    return (result);
}

void HText_DisableCurrentForm(void)
{
    TextAnchor *anchor_ptr;

    HTFormDisabled = TRUE;
    if (!HTMainText)
	return;

    /*
     * Go through list of anchors and set the disabled flag.
     */
    for (anchor_ptr = HTMainText->first_anchor;
	 anchor_ptr != NULL;
	 anchor_ptr = anchor_ptr->next) {
	if (anchor_ptr->link_type == INPUT_ANCHOR &&
	    anchor_ptr->input_field->number == HTFormNumber) {

	    anchor_ptr->input_field->disabled = TRUE;
	}
    }

    return;
}

void HText_ResetForm(FormInfo * form)
{
    TextAnchor *anchor_ptr;

    _statusline(RESETTING_FORM);
    if (HTMainText == 0)
	return;

    /*
     * Go through list of anchors and reset values.
     */
    for (anchor_ptr = HTMainText->first_anchor;
	 anchor_ptr != NULL;
	 anchor_ptr = anchor_ptr->next) {
	if (anchor_ptr->link_type == INPUT_ANCHOR) {
	    if (anchor_ptr->input_field->number == form->number) {

		if (anchor_ptr->input_field->type == F_RADIO_TYPE ||
		    anchor_ptr->input_field->type == F_CHECKBOX_TYPE) {

		    if (anchor_ptr->input_field->orig_value[0] == '0')
			anchor_ptr->input_field->num_value = 0;
		    else
			anchor_ptr->input_field->num_value = 1;

		} else if (anchor_ptr->input_field->type ==
			   F_OPTION_LIST_TYPE) {
		    anchor_ptr->input_field->value =
			anchor_ptr->input_field->orig_value;

		    anchor_ptr->input_field->cp_submit_value =
			anchor_ptr->input_field->orig_submit_value;

		} else {
		    StrAllocCopy(anchor_ptr->input_field->value,
				 anchor_ptr->input_field->orig_value);
		}
	    } else if (anchor_ptr->input_field->number > form->number) {
		break;
	    }
	}
    }
}

/*
 * This function is called before reloading/reparsing current document to find
 * whether any forms content was changed by user so any information will be
 * lost.
 */
BOOLEAN HText_HaveUserChangedForms(HText *text)
{
    TextAnchor *anchor_ptr;

    if (text == 0)
	return FALSE;

    /*
     * Go through list of anchors to check if any value was changed.
     * This code based on HText_ResetForm()
     */
    for (anchor_ptr = text->first_anchor;
	 anchor_ptr != NULL;
	 anchor_ptr = anchor_ptr->next) {
	if (anchor_ptr->link_type == INPUT_ANCHOR) {

	    if (anchor_ptr->input_field->type == F_RADIO_TYPE ||
		anchor_ptr->input_field->type == F_CHECKBOX_TYPE) {

		if ((anchor_ptr->input_field->orig_value[0] == '0' &&
		     anchor_ptr->input_field->num_value == 1) ||
		    (anchor_ptr->input_field->orig_value[0] != '0' &&
		     anchor_ptr->input_field->num_value == 0))
		    return TRUE;

	    } else if (anchor_ptr->input_field->type == F_OPTION_LIST_TYPE) {
		if (strcmp(anchor_ptr->input_field->value,
			   anchor_ptr->input_field->orig_value))
		    return TRUE;

		if (strcmp(anchor_ptr->input_field->cp_submit_value,
			   anchor_ptr->input_field->orig_submit_value))
		    return TRUE;

	    } else {
		if (strcmp(anchor_ptr->input_field->value,
			   anchor_ptr->input_field->orig_value))
		    return TRUE;
	    }
	}
    }
    return FALSE;
}

void HText_activateRadioButton(FormInfo * form)
{
    TextAnchor *anchor_ptr;
    int form_number = form->number;

    if (!HTMainText)
	return;
    for (anchor_ptr = HTMainText->first_anchor;
	 anchor_ptr != NULL;
	 anchor_ptr = anchor_ptr->next) {
	if (anchor_ptr->link_type == INPUT_ANCHOR &&
	    anchor_ptr->input_field->type == F_RADIO_TYPE) {

	    if (anchor_ptr->input_field->number == form_number) {

		/* if it has the same name and its on */
		if (!strcmp(anchor_ptr->input_field->name, form->name) &&
		    anchor_ptr->input_field->num_value) {
		    anchor_ptr->input_field->num_value = 0;
		    break;
		}
	    } else if (anchor_ptr->input_field->number > form_number) {
		break;
	    }

	}
    }

    form->num_value = 1;
}

#ifdef LY_FIND_LEAKS
/*
 *	Purpose:	Free all currently loaded HText objects in memory.
 *	Arguments:	void
 *	Return Value:	void
 *	Remarks/Portability/Dependencies/Restrictions:
 *		Usage of this function should really be limited to program
 *			termination.
 *	Revision History:
 *		05-27-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static void free_all_texts(void)
{
    HText *cur = NULL;

    if (!loaded_texts)
	return;

    /*
     * Simply loop through the loaded texts list killing them off.
     */
    while (loaded_texts && !HTList_isEmpty(loaded_texts)) {
	if ((cur = (HText *) HTList_removeLastObject(loaded_texts)) != NULL) {
	    HText_free(cur);
	}
    }

    /*
     * Get rid of the text list.
     */
    if (loaded_texts) {
	HTList_delete(loaded_texts);
    }

    /*
     * Insurance for bad HTML.
     */
    FREE(HTCurSelectGroup);
    FREE(HTCurSelectGroupSize);
    FREE(HTCurSelectedOptionValue);
    FREE(HTFormAction);
    FREE(HTFormEnctype);
    FREE(HTFormTitle);
    FREE(HTFormAcceptCharset);
    PerFormInfo_free(HTCurrentForm);

    return;
}
#endif /* LY_FIND_LEAKS */

/*
 *  stub_HTAnchor_address is like HTAnchor_address, but it returns the
 *  parent address for child links.  This is only useful for traversal's
 *  where one does not want to index a text file N times, once for each
 *  of N internal links.  Since the parent link has already been taken,
 *  it won't go again, hence the (incorrect) links won't cause problems.
 */
char *stub_HTAnchor_address(HTAnchor * me)
{
    char *addr = NULL;

    if (me)
	StrAllocCopy(addr, me->parent->address);
    return addr;
}

void HText_setToolbar(HText *text)
{
    if (text)
	text->toolbar = TRUE;
    return;
}

BOOL HText_hasToolbar(HText *text)
{
    return (BOOL) ((text && text->toolbar) ? TRUE : FALSE);
}

void HText_setNoCache(HText *text)
{
    if (text)
	text->no_cache = TRUE;
    return;
}

BOOL HText_hasNoCacheSet(HText *text)
{
    return (BOOL) ((text && text->no_cache) ? TRUE : FALSE);
}

BOOL HText_hasUTF8OutputSet(HText *text)
{
    return (BOOL) ((text && text->T.output_utf8) ? TRUE : FALSE);
}

/*
 *  Check charset and set the kcode element. -FM
 *  Info on the input charset may be passed in in two forms,
 *  as a string (if given explicitly) and as a pointer to
 *  a LYUCcharset (from chartrans mechanism); either can be NULL.
 *  For Japanese the kcode will be reset at a space or explicit
 *  line or paragraph break, so what we set here may not last for
 *  long.  It's potentially more important not to set HTCJK to
 *  NOCJK unless we are sure. - kw
 */
void HText_setKcode(HText *text, const char *charset,
		    LYUCcharset *p_in)
{
    BOOL charset_explicit;

    if (!text)
	return;

    /*
     * Check whether we have some kind of info.  - kw
     */
    if (!charset && !p_in) {
	return;
    }
    charset_explicit = charset ? TRUE : FALSE;
    /*
     * If no explicit charset string, use the implied one.  - kw
     */
    if (!charset || *charset == '\0') {
	charset = p_in->MIMEname;
    }
    /*
     * Check whether we have a specified charset.  -FM
     */
    if (!charset || *charset == '\0') {
	return;
    }

    /*
     * We've included the charset, and not forced a download offer,
     * only if the currently selected character set can handle it,
     * so check the charset value and set the text->kcode element
     * appropriately.  -FM
     */
    /*  If charset isn't specified explicitely nor assumed,
     * p_in->MIMEname would be set as display charset.
     * So text->kcode sholud be set as SJIS or EUC here only if charset
     * is specified explicitely, otherwise text->kcode would cause
     * mishandling Japanese strings.  -- TH
     */
    if (charset_explicit && (!strcmp(charset, "shift_jis") ||
			     !strcmp(charset, "x-sjis") ||	/* 1997/11/28 (Fri) 18:11:33 */
			     !strcmp(charset, "x-shift-jis"))) {
	text->kcode = SJIS;
    } else if (charset_explicit
#ifdef EXP_JAPANESEUTF8_SUPPORT
	       && strcmp(charset, "utf-8")
#endif
	       && ((p_in && (p_in->enc == UCT_ENC_CJK)) ||
		   !strcmp(charset, "x-euc") ||		/* 1997/11/28 (Fri) 18:11:24 */
		   !strcmp(charset, "euc-jp") ||
		   !strncmp(charset, "x-euc-", 6) ||
		   !strcmp(charset, "euc-kr") ||
		   !strcmp(charset, "iso-2022-kr") ||
		   !strcmp(charset, "big5") ||
		   !strcmp(charset, "cn-big5") ||
		   !strcmp(charset, "euc-cn") ||
		   !strcmp(charset, "gb2312") ||
		   !strncmp(charset, "cn-gb", 5) ||
		   !strcmp(charset, "iso-2022-cn"))) {
	text->kcode = EUC;
    } else {
	/*
	 * If we get to here, it's not CJK, so disable that if
	 * it is enabled.  But only if we are quite sure.  -FM & kw
	 */
	text->kcode = NOKANJI;
	if (HTCJK != NOCJK) {
	    if (!p_in || ((p_in->enc != UCT_ENC_CJK)
#ifdef EXP_JAPANESEUTF8_SUPPORT
			  && (p_in->enc != UCT_ENC_UTF8)
#endif
		)) {
		HTCJK = NOCJK;
	    }
	}
    }

    if (charset_explicit
#ifdef EXP_JAPANESEUTF8_SUPPORT
	&& strcmp(charset, "utf-8")
#endif
	) {
	text->specified_kcode = text->kcode;
    } else {
	if (UCAssume_MIMEcharset) {
	    if (!strcmp(UCAssume_MIMEcharset, "euc-jp"))
		text->kcode = text->specified_kcode = EUC;
	    else if (!strcmp(UCAssume_MIMEcharset, "shift_jis"))
		text->kcode = text->specified_kcode = SJIS;
	}
    }

    return;
}

/*
 *  Set a permissible split at the current end of the last line. -FM
 */
void HText_setBreakPoint(HText *text)
{
    if (!text)
	return;

    /*
     * Can split here.  -FM
     */
    text->permissible_split = text->last_line->size;

    return;
}

/*
 *  This function determines whether a document which
 *  would be sought via the a URL that has a fragment
 *  directive appended is otherwise identical to the
 *  currently loaded document, and if so, returns
 *  FALSE, so that any no_cache directives can be
 *  overridden "safely", on the grounds that we are
 *  simply acting on the equivalent of a paging
 *  command.  Otherwise, it returns TRUE, i.e, that
 *  the target document might differ from the current,
 *  based on any caching directives or analyses which
 *  claimed or suggested this. -FM
 */
BOOL HText_AreDifferent(HTParentAnchor *anchor,
			const char *full_address)
{
    HTParentAnchor *MTanc;
    char *MTaddress;
    char *MTpound;

    /*
     * Do we have a loaded document and both
     * arguments for this function?
     */
    if (!(HTMainText && anchor && full_address))
	return TRUE;

    /*
     * Do we have both URLs?
     */
    MTanc = HTMainText->node_anchor;
    if (!(MTanc->address && anchor->address))
	return (TRUE);

    /*
     * Do we have a fragment associated with the target?
     */
    if (findPoundSelector(full_address) == NULL)
	return (TRUE);

    /*
     * Always treat client-side image map menus
     * as potentially stale, so we'll create a
     * fresh menu from the LynxMaps HTList.
     */
    if (isLYNXIMGMAP(anchor->address))
	return (TRUE);

    /*
     * Do the docs differ in the type of request?
     */
    if (MTanc->isHEAD != anchor->isHEAD)
	return (TRUE);

    /*
     * Are the actual URLs different, after factoring
     * out a "LYNXIMGMAP:" leader in the MainText URL
     * and its fragment, if present?
     */
    MTaddress = (isLYNXIMGMAP(MTanc->address)
		 ? MTanc->address + LEN_LYNXIMGMAP
		 : MTanc->address);
    MTpound = trimPoundSelector(MTaddress);
    if (strcmp(MTaddress, anchor->address)) {
	restorePoundSelector(MTpound);
	return (TRUE);
    }
    restorePoundSelector(MTpound);

    /*
     * If the MainText is not an image map menu,
     * do the docs have different POST contents?
     */
    if (MTaddress == MTanc->address) {
	if (MTanc->post_data) {
	    if (anchor->post_data) {
		if (!BINEQ(MTanc->post_data, anchor->post_data)) {
		    /*
		     * Both have contents, and they differ.
		     */
		    return (TRUE);
		}
	    } else {
		/*
		 * The loaded document has content, but the
		 * target doesn't, so they're different.
		 */
		return (TRUE);
	    }
	} else if (anchor->post_data) {
	    /*
	     * The loaded document does not have content, but
	     * the target does, so they're different.
	     */
	    return (TRUE);
	}
    }

    /*
     * We'll assume the target is a position in the currently
     * displayed document, and thus can ignore any header, META,
     * or other directives not to use a cached rendition.  -FM
     */
    return (FALSE);
}

#define CanTrimTextArea(c) \
    (LYtrimInputFields ? isspace(c) : ((c) == '\r' || (c) == '\n'))

/*
 * Cleanup new lines coming into a TEXTAREA from an external editor, or a
 * file, such that they are in a suitable format for TEXTAREA rendering,
 * display, and manipulation.  That means trimming off trailing whitespace
 * from the line, expanding TABS into SPACES, and substituting a printable
 * character for control chars, and the like.
 *
 * --KED 02/24/99
 */
static void cleanup_line_for_textarea(char *line,
				      int len)
{
    char tbuf[MAX_LINE];

    char *cp;
    char *p;
    char *s;
    int i;
    int n;

    /*
     * Whack off trailing whitespace from the line.
     */
    for (i = len, p = line + (len - 1); i != 0; p--, i--) {
	if (CanTrimTextArea(UCH(*p)))
	    *p = '\0';
	else
	    break;
    }

    if (strlen(line) != 0) {
	/*
	 * Expand any tab's, since they won't render properly in a TEXTAREA.
	 *
	 * [Is that "by spec", or just a "lynxism"?  As may be, it seems that
	 * such chars may cause other problems, too ...  with cursor movement,
	 * submit'ing, etc.  Probably needs looking into more deeply.]
	 */
	p = line;
	s = tbuf;

	while (*p) {
	    if ((cp = strchr(p, '\t')) != 0) {
		i = cp - p;
		s = (strncpy(s, p, i)) + i;
		n = TABSTOP - (i % TABSTOP);
		s = (strncpy(s, SPACES, n)) + n;
		p += (i + 1);

	    } else {

		strcpy(s, p);
		break;
	    }
	}

	/*
	 * Replace control chars with something printable.  Note that char
	 * substitution above 0x7f is dependent on the charset being used,
	 * and then only applies to the contiguous run of char values that
	 * are between 0x80, and the 1st real high-order-bit-set character,
	 * as specified by the charset.  In general (ie, for many character
	 * sets), that usually means the so-called "C1 control chars" that
	 * range from 0x80 thru 0x9f.  For EBCDIC machines, we only trim the
	 * (control) chars below a space (x'40').
	 *
	 * The assumption in all this is that the charset used in the editor,
	 * is compatible with the charset specified in lynx.
	 *
	 * [At some point in time, when/if lynx ever supports multibyte chars
	 * internally (eg, UCS-2, UCS-4, UTF-16, etc), this kind of thing may
	 * well cause problems.  But then, supporting such char sets will
	 * require massive changes in (most) all parts of the lynx code, so
	 * until then, we do the rational thing with char values that would
	 * otherwise foul the display, if left alone.  If you're implementing
	 * multibyte character set support, consider yourself to have been
	 * warned.]
	 */
	for (p = line, s = tbuf; *s != '\0'; p++, s++) {
#ifndef EBCDIC
	    *p = ((UCH(*s) < UCH(' ')) ||
		  (UCH(*s) == UCH('\177')) ||
		  ((UCH(*s) > UCH('\177')) &&
		   (UCH(*s) <
		    UCH(LYlowest_eightbit[current_char_set]))))
		? (char) SPLAT : *s;
#else
	    *p = (UCH(*s) < UCH(' ')) ? SPLAT : *s;
#endif
	}
	*p = '\0';
    }

    return;
}

/*
 * Re-render the text of a tagged ("[123]") HTLine (arg1), with the tag
 * number incremented by some value (arg5).  The re-rendered string may
 * be allowed to expand in the event of a tag width change (eg, 99 -> 100)
 * as controlled by arg6 (CHOP or NOCHOP).  Arg4 is either (the address
 * of) a value which must match, in order for the tag to be incremented,
 * or (the address of) a 0-value, which will match any value, and cause
 * any valid tag to be incremented.  Arg2 is a pointer to the first/only
 * anchor that exists on the line; we may need to adjust their position(s)
 * on the line.  Arg3 when non-0 indicates the number of new digits that
 * were added to the 2nd line in a line crossing pair.
 *
 * All tags fields in a line which individually match an expected new value,
 * are incremented.  Line crossing [tags] are handled (PITA).
 *
 * Untagged or improperly tagged lines are not altered.
 *
 * Returns the number of chars added to the original string's length, if
 * any.
 *
 * --KED 02/03/99
 */
static int increment_tagged_htline(HTLine *ht, TextAnchor *a, int *lx_val,
				   int *old_val,
				   int incr,
				   int mode)
{
    char buf[MAX_LINE];
    char lxbuf[MAX_LINE * 2];

    TextAnchor *st_anchor = a;
    TextAnchor *nxt_anchor;

    char *p = ht->data;
    char *s = buf;
    char *lx = lxbuf;
    char *t;

    BOOLEAN plx = FALSE;
    BOOLEAN valid;

    int val;
    int n;
    int new_n;
    int pre_n;
    int post_n;
    int fixup = 0;

    /*
     * Cleanup for the 2nd half of a line crosser, whose number of tag
     * digits grew by some number of places (usually 1 when it does
     * happen, though it *could* be more).  The tag chars were already
     * rendered into the 2nd line of the pair, but the positioning and
     * other effects haven't been rippled through any other anchors on
     * the (2nd) line.  So we do that here, as a special case, since
     * the part of the tag that's in the 2nd line of the pair, will not
     * be found by the tag string parsing code.  Double PITA.
     *
     * [see comments below on line crosser caused problems]
     */
    if (*lx_val != 0) {
	nxt_anchor = st_anchor;
	while ((nxt_anchor) && (nxt_anchor->line_num == a->line_num)) {
	    nxt_anchor->line_pos += *lx_val;
	    nxt_anchor = nxt_anchor->next;
	}
	fixup = *lx_val;
	*lx_val = 0;
	if (st_anchor)
	    st_anchor = st_anchor->next;
    }

    /*
     * Walk thru the line looking for tags (ie, "[nnn]" strings).
     */
    while (*p != '\0') {
	if (*p != '[') {
	    *s++ = *p++;
	    continue;

	} else {
	    *s++ = *p++;
	    t = p;
	    n = 0;
	    valid = TRUE;	/* p = t = byte after '[' */

	    /*
	     * Make sure there are only digits between "[" and "]".
	     */
	    while (*t != ']') {
		if (*t == '\0') {	/* uhoh - we have a potential line crosser */
		    valid = FALSE;
		    plx = TRUE;
		    break;
		}
		if (isdigit(UCH(*t++))) {
		    n++;
		    continue;
		} else {
		    valid = FALSE;
		    break;
		}
	    }

	    /*
	     * If the format is OK, we check to see if the value is what
	     * we expect.  If not, we have a random [nn] string in the text,
	     * and leave it alone.
	     *
	     * [It is *possible* to have a false match here, *if* there are
	     * two identical [nn] strings (including the numeric value of
	     * nn), one of which is the [tag], and the other being part of
	     * a document.  In such a case, the 1st [nn] string will get
	     * incremented; the 2nd one won't, which makes it a 50-50 chance
	     * of being correct, if and when such an unlikely juxtaposition
	     * of text ever occurs.  Further validation tests of the [nnn]
	     * string are probably not possible, since little of the actual
	     * anchor-associated-text is retained in the TextAnchor or the
	     * FormInfo structs.  Fortunately, I think the current method is
	     * more than adequate to weed out 99.999% of any possible false
	     * matches, just as it stands.  Caveat emptor.]
	     */
	    if ((valid) && (n > 0)) {
		val = atoi(p);
		if ((val == *old_val) || (*old_val == 0)) {	/* 0 matches all */
		    if (*old_val != 0)
			(*old_val)++;
		    val += incr;
		    sprintf(s, "%d", val);
		    new_n = strlen(s);
		    s += new_n;
		    p += n;

		    /*
		     * If the number of digits in an existing [tag] increased
		     * (eg, [99] --> [100], etc), we need to "adjust" its
		     * horizontal position, and that of all subsequent tags
		     * that may be on the same line.  PITA.
		     *
		     * [This seems to work as long as a tag isn't a line
		     * crosser; when it is, the position of anchors on either
		     * side of the split tag, seem to "float" and try to be
		     * as "centered" as possible.  Which means that simply
		     * incrementing the line_pos by the fixed value of the
		     * number of digits that got added to some tag in either
		     * line doesn't work quite right, and the text for (say)
		     * a button may get stomped on by another copy of itself,
		     * but offset by a few chars, when it is selected (eg,
		     * "Box Office" may end up looking like "BoBox Office" or
		     * "Box Officece", etc.
		     *
		     * Dunno how to fix that behavior ATT, but at least the
		     * tag numbers themselves are correct.  -KED /\oo/\ ]
		     */
		    if ((new_n -= n) != 0) {
			nxt_anchor = st_anchor;
			while ((nxt_anchor) &&
			       (nxt_anchor->line_num == a->line_num)) {
			    nxt_anchor->line_pos += new_n;
			    nxt_anchor = nxt_anchor->next;
			}
			if (st_anchor)
			    st_anchor = st_anchor->next;
		    }
		}
	    }

	    /*
	     * Unfortunately, valid [tag] strings *can* be split across two
	     * lines.  Perhaps it would be best to just prevent that from
	     * happening, but a look into that code, makes me wonder.  Anyway,
	     * we can handle such tags without *too* much trouble in here [I
	     * think], though since such animals are rather rare, it makes it
	     * a bit difficult to test thoroughly (ie, Beyond here, there be
	     * Dragons).
	     *
	     * We use lxbuf[] to deal with the two lines involved.
	     */
	    pre_n = strlen(p);	/* count of 1st part chars in this line */
	    post_n = strlen(ht->next->data);
	    if (plx
		&& (pre_n + post_n + 2 < (int) sizeof(lxbuf))) {
		strcpy(lx, p);	/* <- 1st part of a possible lx'ing tag */
		strcat(lx, ht->next->data);	/* tack on NEXT line          */

		t = lx;
		n = 0;
		valid = TRUE;

		/*
		 * Go hunting again for just digits, followed by tag end ']'.
		 */
		while (*t != ']') {
		    if (isdigit(UCH(*t++))) {
			n++;
			continue;
		    } else {
			valid = FALSE;
			break;
		    }
		}

		/*
		 * It *looks* like a line crosser; now we value test it to
		 * find out for sure [but see the "false match" warning,
		 * above], and if it matches, increment it into the buffer,
		 * along with the 2nd line's text.
		 */
		if ((valid)
		    && (n > 0)
		    && (n + post_n + 2) < MAX_LINE) {
		    val = atoi(lx);
		    if ((val == *old_val) || (*old_val == 0)) {
			if (*old_val != 0)
			    (*old_val)++;
			val += incr;
			sprintf(lx, "%d", val);
			new_n = strlen(lx);
			strcat(lx, strchr(ht->next->data, ']'));

			/*
			 * We keep the the same number of chars from the
			 * adjusted tag number in the current line; any
			 * extra chars due to a digits increase, will be
			 * stuffed into the next line.
			 *
			 * Keep track of any digits added, for the next
			 * pass through.
			 */
			s = strncpy(s, lx, pre_n) + pre_n;
			lx += pre_n;
			strcpy(ht->next->data, lx);

			*lx_val = new_n - n;
		    }
		}
		break;		/* had an lx'er, so we're done with this line */
	    }
	}
    }

    *s = '\0';

    n = strlen(ht->data);
    if (mode == CHOP) {
	*(buf + n) = '\0';
    } else if (strlen(buf) > ht->size) {
	/* we didn't allocate enough space originally - increase it */
	HTLine *temp;

	allocHTLine(temp, strlen(buf));
	if (!temp)
	    outofmem(__FILE__, "increment_tagged_htline");
	memcpy(temp, ht, LINE_SIZE(0));
#if defined(USE_COLOR_STYLE)
	POOLallocstyles(temp->styles, ht->numstyles);
	if (!temp->styles)
	    outofmem(__FILE__, "increment_tagged_htline");
	memcpy(temp->styles, ht->styles, sizeof(HTStyleChange) * ht->numstyles);
#endif
	ht = temp;
	ht->prev->next = ht;	/* Link in new line */
	ht->next->prev = ht;	/* Could be same node of course */
    }
    strcpy(ht->data, buf);

    return (strlen(buf) - n + fixup);
}

/*
 * Creates a new anchor and associated struct's appropriate for a form
 * TEXTAREA, and links them into the lists following the current anchor
 * position (as specified by arg1).
 *
 * Exits with arg1 now pointing at the new TextAnchor, and arg2 pointing
 * at the new, associated HTLine.
 *
 * --KED 02/13/99
 */
static void insert_new_textarea_anchor(TextAnchor **curr_anchor, HTLine **exit_htline)
{
    TextAnchor *anchor = *curr_anchor;
    HTLine *htline;

    TextAnchor *a = 0;
    FormInfo *f = 0;
    HTLine *l = 0;

    int curr_tag = 0;		/* 0 ==> match any [tag] number */
    int lx = 0;			/* 0 ==> no line crossing [tag]; it's a new line */
    int i;

    /*
     * Find line in the text that matches ending anchorline of
     * the TEXTAREA.
     *
     * [Yes, Virginia ...  we *do* have to go thru this for each
     * anchor being added, since there is NOT a 1-to-1 mapping
     * between anchors and htlines.  I suppose we could create
     * YAS (Yet Another Struct), but there are too many structs{}
     * floating around in here, as it is.  IMNSHO.]
     */
    for (htline = FirstHTLine(HTMainText), i = 0;
	 anchor->line_num != i; i++) {
	htline = htline->next;
	if (htline == HTMainText->last_line)
	    break;
    }

    /*
     * Clone and initialize the struct's needed to add a new TEXTAREA
     * anchor.
     */
    allocHTLine(l, MAX_LINE);
    POOLtypecalloc(TextAnchor, a);

    POOLtypecalloc(FormInfo, f);
    if (a == NULL || l == NULL || f == NULL)
	outofmem(__FILE__, "insert_new_textarea_anchor");

    /*  Init all the fields in the new TextAnchor.                 */
    /*  [anything "special" needed based on ->show_anchor value ?] */
    a->next = anchor->next;
    a->number = anchor->number;
    a->line_pos = anchor->line_pos;
    a->extent = anchor->extent;
    a->sgml_offset = SGML_offset();
    a->line_num = anchor->line_num + 1;
    LYCopyHiText(a, anchor);
    a->link_type = anchor->link_type;
    a->input_field = f;
    a->show_anchor = anchor->show_anchor;
    a->inUnderline = anchor->inUnderline;
    a->expansion_anch = TRUE;
    a->anchor = NULL;

    /*  Just the (seemingly) relevant fields in the new FormInfo.  */
    /*  [do we need to do anything "special" based on ->disabled]  */
    StrAllocCopy(f->name, anchor->input_field->name);
    f->number = anchor->input_field->number;
    f->type = anchor->input_field->type;
    StrAllocCopy(f->orig_value, "");
    f->size = anchor->input_field->size;
    f->maxlength = anchor->input_field->maxlength;
    f->no_cache = anchor->input_field->no_cache;
    f->disabled = anchor->input_field->disabled;
    f->value_cs = current_char_set;	/* use current setting - kw */

    /*  Init all the fields in the new HTLine (but see the #if).   */
    l->next = htline->next;
    l->prev = htline;
    l->offset = htline->offset;
    l->size = htline->size;
#if defined(USE_COLOR_STYLE)
    /* dup styles[] if needed [no need in TEXTAREA (?); leave 0's] */
    l->numstyles = htline->numstyles;
    /*we fork the pointers! */
    l->styles = htline->styles;
#endif
    strcpy(l->data, htline->data);

    /*
     * Link in the new HTLine.
     */
    htline->next->prev = l;
    htline->next = l;

    if (fields_are_numbered()) {
	a->number++;
	increment_tagged_htline(l, a, &lx, &curr_tag, 1, CHOP);
    }

    /*
     * If we're at the tail end of the TextAnchor or HTLine list(s),
     * the new node becomes the last node.
     */
    if (anchor == HTMainText->last_anchor)
	HTMainText->last_anchor = a;
    if (htline == HTMainText->last_line)
	HTMainText->last_line = l;

    /*
     * Link in the new TextAnchor and point the entry anchor arg at it;
     * point the entry HTLine arg at it, too.
     */
    anchor->next = a;
    *curr_anchor = a;

    *exit_htline = l->next;

    return;
}

/*
 * If new anchors were added to expand a TEXTAREA, we need to ripple the
 * new line numbers [and char counts ?] thru the subsequent anchors.
 *
 * If form lines are getting [nnn] tagged, we need to update the displayed
 * tag values to match (which means rerendering them ...  sigh).
 *
 * Finally, we need to update various HTMainText and other counts, etc.
 *
 * [dunno if the char counts really *need* to be done, or if we're using
 * the exactly proper values/algorithms ...  seems to be OK though ...]
 *
 * --KED 02/13/99
 */
static void update_subsequent_anchors(int newlines,
				      TextAnchor *start_anchor,
				      HTLine *start_htline,
				      int start_tag)
{
    TextAnchor *anchor;
    HTLine *htline = start_htline;

    int line_adj = 0;
    int tag_adj = 0;
    int lx = 0;
    int hang = 0;		/* for HANG detection of a nasty intermittent */
    int hang_detect = 100000;	/* ditto */

    CTRACE((tfp, "GridText: adjusting struct's to add %d new line(s)\n", newlines));

    /*
     * Update numeric fields of the rest of the anchors.
     *
     * [We bypass bumping ->number if it has a value of 0, which takes care
     * of the ->input_field->type == F_HIDDEN_TYPE (as well as any other
     * "hidden" anchors, if such things exist).  Seems like the "right
     * thing" to do.  I think.]
     */
    anchor = start_anchor->next;	/* begin updating with the NEXT anchor */
    while (anchor) {
	if (fields_are_numbered() &&
	    (anchor->number != 0))
	    anchor->number += newlines;
	anchor->line_num += newlines;
	anchor = anchor->next;
    }

    /*
     * Update/rerender anchor [tags], if they are being numbered.
     *
     * [If a number tag (eg, "[177]") is itself broken across a line
     * boundary, this fixup only partially works.  While the tag
     * numbering is done properly across the pair of lines, the
     * horizontal positioning on *either* side of the split, can get
     * out of sync by a char or two when it gets selected.  See the
     * [comments] in increment_tagged_htline() for some more detail.
     *
     * I suppose THE fix is to prevent such tag-breaking in the first
     * place (dunno where yet, though).  Ah well ...  at least the tag
     * numbers themselves are correct from top to bottom now.
     *
     * All that said, about the only time this will be a problem in
     * *practice*, is when a page has near 1000 links or more (possibly
     * after a TEXTAREA expansion), and has line crossing tag(s), and
     * the tag numbers in a line crosser go from initially all 3 digit
     * numbers, to some mix of 3 and 4 digits (or all 4 digits) as a
     * result of the expansion process.  Oh, you also need a "clump" of
     * anchors all on the same lines.
     *
     * Yes, it *can* happen, but in real life, it probably won't be
     * seen very much ...]
     *
     * [This may also be an artifact of bumping into the right hand
     * screen edge (or RHS margin), since we don't even *think* about
     * relocating an anchor to the following line, when [tag] digits
     * expansion pushes things too far in that direction.]
     */
    if (fields_are_numbered()) {
	anchor = start_anchor->next;
	while (htline != FirstHTLine(HTMainText)) {

	    while (anchor) {
		if ((anchor->number - newlines) == start_tag)
		    break;

		/*** A HANG (infinite loop) *has* occurred here, with */
		/*** the values of anchor and anchor->next being the  */
		/*** the same, OR with anchor->number "magically" and */
		/*** suddenly taking on an anchor-pointer-like value. */
		/***                                                  */
		/*** The same code and same doc have both passed and  */
		/*** failed at different times, which indicates some  */
		/*** sort of content/html dependency, or some kind of */
		/*** a "race" condition, but I'll be damned if I can  */
		/*** find it after tons of CTRACE's, printf()'s, gdb  */
		/*** breakpoints and watchpoints, etc.                */
		/***                                                  */
		/*** I have added a hang detector (with error msg and */
		/*** beep) here, to break the loop and warn the user, */
		/*** until it can be isolated and fixed.              */
		/***                                                  */
		/*** [One UGLY intermittent .. gak ..!  02/22/99 KED] */

		hang++;
		if ((anchor == anchor->next) || (hang >= hang_detect))
		    goto hang_detected;

		anchor = anchor->next;
	    }

	    if (anchor) {
		line_adj = increment_tagged_htline(htline, anchor, &lx,
						   &start_tag, newlines,
						   NOCHOP);
		htline->size += line_adj;
		tag_adj += line_adj;

	    } else {

		break;		/* out of anchors ... we're done */
	    }

	    htline = htline->next;
	}
    }

  finish:
    /*
     * Fixup various global variables.
     */
    nlinks += newlines;
    HTMainText->Lines += newlines;
    HTMainText->last_anchor_number += newlines;

    more_text = HText_canScrollDown();

    CTRACE((tfp, "GridText: TextAnchor and HTLine struct's adjusted\n"));

    return;

  hang_detected:		/* ugliness has happened; inform user and do the best we can */

    HTAlert(gettext("Hang Detect: TextAnchor struct corrupted - suggest aborting!"));
    goto finish;
}

/*
 * Transfer the initial contents of a TEXTAREA to a temp file, invoke the
 * user's editor on that file, then transfer the contents of the resultant
 * edited file back into the TEXTAREA (expanding the size of the area, if
 * required).
 *
 * Returns the number of lines that the cursor should be moved so that it
 * will end up on the 1st blank line of whatever number of trailing blank
 * lines there are in the TEXTAREA (there will *always* be at least one).
 *
 * --KED 02/01/99
 */
int HText_ExtEditForm(LinkInfo * form_link)
{
    struct stat stat_info;
    size_t size;

    char *ed_temp;
    FILE *fp;

    TextAnchor *anchor_ptr;
    TextAnchor *start_anchor = NULL;
    TextAnchor *end_anchor = NULL;
    BOOLEAN firstanchor = TRUE;
    BOOLEAN wrapalert = FALSE;

    char ed_offset[10];
    int start_line = 0;
    int entry_line = form_link->anchor_line_num;
    int exit_line = 0;
    int orig_cnt = 0;
    int line_cnt = 1;

    FormInfo *form = form_link->l_form;
    char *areaname = form->name;
    int form_num = form->number;

    HTLine *htline = NULL;

    char *ebuf;
    char *line;
    char *lp;
    char *cp;
    int match_tag = 0;
    int newlines = 0;
    int len, len0, len_in;
    int wanted_fieldlen_wrap = -1;	/* not yet asked; 0 means don't. */
    char *skip_at = NULL;
    int skip_num = 0, i;

    CTRACE((tfp, "GridText: entered HText_ExtEditForm()\n"));

    ed_temp = (char *) malloc(LY_MAXPATH);
    if ((fp = LYOpenTemp(ed_temp, "", "w")) == 0) {
	FREE(ed_temp);
	return (0);
    }

    /*
     * Begin at the beginning, to find 1st anchor in the TEXTAREA, then
     * write all of its lines (anchors) out to the edit temp file.
     *
     * [Finding the TEXTAREA we're actually *in* with these attributes
     * isn't foolproof.  The form_num isn't unique to a given TEXTAREA,
     * and there *could* be TEXTAREA's with the same "name".  If that
     * should ever be true, we'll actually get the data from the *1st*
     * TEXTAREA in the page that matches.  We should probably assign
     * a unique id to each TEXTAREA in a page, and match on that, to
     * avoid this (potential) problem.
     *
     * Since the odds of "false matches" *actually* happening in real
     * life seem rather small though, we'll hold off doing this, for a
     * rainy day ...]
     */
    anchor_ptr = HTMainText->first_anchor;

    while (anchor_ptr) {

	if ((anchor_ptr->link_type == INPUT_ANCHOR) &&
	    (anchor_ptr->input_field->type == F_TEXTAREA_TYPE) &&
	    (anchor_ptr->input_field->number == form_num) &&
	    !strcmp(anchor_ptr->input_field->name, areaname)) {

	    if (firstanchor) {
		firstanchor = FALSE;
		start_anchor = anchor_ptr;
		start_line = anchor_ptr->line_num;
	    }
	    orig_cnt++;

	    /*
	     * Write the anchors' text to the temp edit file.
	     */
	    fputs(anchor_ptr->input_field->value, fp);
	    fputc('\n', fp);

	} else {

	    if (!firstanchor)
		break;
	}
	anchor_ptr = anchor_ptr->next;
    }
    LYCloseTempFP(fp);

    CTRACE((tfp, "GridText: TEXTAREA name=|%s| dumped to tempfile\n", areaname));
    CTRACE((tfp, "GridText: invoking editor (%s) on tempfile\n", editor));

    /*
     * Go edit the TEXTAREA temp file, with the initial editor line
     * corresponding to the TEXTAREA line the cursor is on (if such
     * positioning is supported by the editor [as lynx knows it]).
     */
    ed_offset[0] = 0;		/* pre-ANSI compilers don't initialize aggregates - TD */
    if (((entry_line - start_line) > 0) && editor_can_position())
	sprintf(ed_offset, "%d", ((entry_line - start_line) + 1));

    edit_temporary_file(ed_temp, ed_offset, NULL);

    CTRACE((tfp, "GridText: returned from editor (%s)\n", editor));

    /*
     * Read back the edited temp file into our buffer.
     */
    if ((stat(ed_temp, &stat_info) < 0) ||
	!S_ISREG(stat_info.st_mode) ||
	((size = stat_info.st_size) == 0)) {
	size = 0;
	ebuf = typecalloc(char);

	if (!ebuf)
	    outofmem(__FILE__, "HText_ExtEditForm");
    } else {
	ebuf = typecallocn(char, size + 1);

	if (!ebuf) {
	    /*
	     * This could be huge - don't exit if we don't have enough
	     * memory for it.  With some luck, the user may be even able
	     * to recover the file manually from the temp space while
	     * the lynx session is not over.  - kw
	     */
	    free(ed_temp);
	    HTAlwaysAlert(NULL, MEMORY_EXHAUSTED_FILE);
	    return 0;
	}

	fp = fopen(ed_temp, "r");
	size = fread(ebuf, 1, size, fp);
	LYCloseInput(fp);
	ebuf[size] = '\0';	/* Terminate! - kw */
    }

    /*
     * Nuke any blank lines from the end of the edited data.
     */
    while ((size != 0)
	   && (CanTrimTextArea(UCH(ebuf[size - 1])) || (ebuf[size - 1] == '\0')))
	ebuf[--size] = '\0';

    /*
     * Copy each line from the temp file into the corresponding anchor
     * struct.  Add new lines to the TEXTAREA if needed.  (Always leave
     * the user with a blank line at the end of the TEXTAREA.)
     */
    if ((line = (char *) malloc(MAX_LINE)) == 0)
	outofmem(__FILE__, "HText_ExtEditForm");

    anchor_ptr = start_anchor;
    if (anchor_ptr->input_field->size <= 4 ||
	anchor_ptr->input_field->size >= MAX_LINE)
	wanted_fieldlen_wrap = 0;

    len = len_in = 0;
    lp = ebuf;

    while ((line_cnt <= orig_cnt) || (*lp) || ((len != 0) && (*lp == '\0'))) {

	if (skip_at) {
	    len0 = skip_at - lp;
	    strncpy(line, lp, len0);
	    line[len0] = '\0';
	    lp = skip_at + skip_num;
	    skip_at = NULL;
	    skip_num = 0;
	} else {
	    len0 = 0;
	}
	line[len0] = '\0';

	if ((cp = strchr(lp, '\n')) != 0)
	    len = len_in = cp - lp;
	else
	    len = len_in = strlen(lp);

	if (wanted_fieldlen_wrap < 0 && !wrapalert &&
	    len0 + len >= start_anchor->input_field->size &&
	    (cp = strchr(lp, ' ')) != NULL &&
	    (cp - lp) < start_anchor->input_field->size - 1) {
	    LYFixCursesOn("ask for confirmation:");
	    LYerase();		/* don't show previous state */
	    if (HTConfirmDefault(gettext("Wrap lines to fit displayed area?"),
				 NO)) {
		wanted_fieldlen_wrap = start_anchor->input_field->size - 1;
	    } else {
		wanted_fieldlen_wrap = 0;
	    }
	}
	if (wanted_fieldlen_wrap > 0 && len0 + len > wanted_fieldlen_wrap) {
	    for (i = wanted_fieldlen_wrap - len0;
		 i + len0 >= wanted_fieldlen_wrap / 4; i--) {
		if (isspace(UCH(lp[i]))) {
		    len = i + 1;
		    cp = lp + i;
		    if (cp[1] != '\n' &&
			isspace(UCH(cp[1])) &&
			!isspace(UCH(cp[2]))) {
			len++;
			cp++;
		    }
		    if (!isspace(UCH(cp[1]))) {
			while (*cp && *cp != '\r' && *cp != '\n' &&
			       (cp - lp) <= len + (3 * wanted_fieldlen_wrap / 4))
			    cp++;	/* search for next line break */
			if (*cp == '\r' && cp[1] == '\n')
			    cp++;
			if (*cp == '\n' &&
			    (cp[1] == '\r' || cp[1] == '\n' ||
			     !isspace(UCH(cp[1])))) {
			    *cp = ' ';
			    while (isspace(UCH(*(cp - 1)))) {
				skip_num++;
				cp--;
			    }
			    skip_at = cp;
			}
		    }
		    break;
		}
	    }
	}
	if (wanted_fieldlen_wrap > 0 && len0 + len > wanted_fieldlen_wrap) {
	    i = len - 1;
	    while (len0 + i + 1 > wanted_fieldlen_wrap &&
		   isspace(UCH(lp[i])))
		i--;
	    if (len0 + i + 1 > wanted_fieldlen_wrap)
		len = wanted_fieldlen_wrap - len0;
	}

	if (len0 + len >= MAX_LINE) {
	    if (!wrapalert) {
		LYFixCursesOn("show alert:");
		HTAlert(gettext("Very long lines have been wrapped!"));
		wrapalert = TRUE;
	    }
	    /*
	     * First try to find a space character for wrapping - kw
	     */
	    for (i = MAX_LINE - len0 - 1; i > 0; i--) {
		if (isspace(UCH(lp[i]))) {
		    len = i;
		    break;
		}
	    }
	    if (len0 + len >= MAX_LINE)
		len = MAX_LINE - len0 - 1;
	}

	strncat(line, lp, len);
	*(line + len0 + len) = '\0';

	cleanup_line_for_textarea(line, len0 + len);

	/*
	 * If there are more lines in the edit buffer than were in the
	 * original TEXTAREA, we need to add a new line/anchor, continuing
	 * on until the edit buffer is empty.
	 */
	if (line_cnt > orig_cnt) {
	    insert_new_textarea_anchor(&end_anchor, &htline);
	    anchor_ptr = end_anchor;	/* make the new anchor current */
	    newlines++;
	}

	/*
	 * Finally copy the new line from the edit buffer into the anchor.
	 */
	StrAllocCopy(anchor_ptr->input_field->value, line);

	/*
	 * Keep track of 1st blank line in any trailing blank lines, for
	 * later cursor repositioning.
	 */
	if (len0 + len > 0)
	    exit_line = 0;
	else if (exit_line == 0)
	    exit_line = anchor_ptr->line_num;

	/*
	 * And do the next line of edited text, for the next anchor ...
	 */
	lp += len;
	if (*lp && isspace(UCH(*lp)))
	    lp++;

	end_anchor = anchor_ptr;
	anchor_ptr = anchor_ptr->next;

	if (anchor_ptr)
	    match_tag = anchor_ptr->number;

	line_cnt++;
    }

    CTRACE((tfp, "GridText: edited text inserted into lynx struct's\n"));

    /*
     * If we've added any new lines/anchors, we need to adjust various
     * things in all anchor-bearing lines following the last newly added
     * line/anchor.  The fun stuff starts here ...
     */
    if (newlines > 0)
	update_subsequent_anchors(newlines, end_anchor, htline, match_tag);

    /*
     * Cleanup time.
     */
    FREE(line);
    FREE(ebuf);
    LYRemoveTemp(ed_temp);
    FREE(ed_temp);

    CTRACE((tfp, "GridText: exiting HText_ExtEditForm()\n"));

    /*
     * Return the offset needed to move the cursor from its current
     * (on entry) line number, to the 1st blank line of the trailing
     * (group of) blank line(s), which is where we want to be.  Let
     * the caller deal with moving us there, however ...  :-) ...
     */
    return (exit_line - entry_line);
}

/*
 * Expand the size of a TEXTAREA by a fixed number of lines (as specified
 * by arg2).
 *
 * --KED 02/14/99
 */
void HText_ExpandTextarea(LinkInfo * form_link, int newlines)
{
    TextAnchor *anchor_ptr;
    TextAnchor *end_anchor = NULL;
    BOOLEAN firstanchor = TRUE;

    FormInfo *form = form_link->l_form;
    char *areaname = form->name;
    int form_num = form->number;

    HTLine *htline = NULL;

    int match_tag = 0;
    int i;

    CTRACE((tfp, "GridText: entered HText_ExpandTextarea()\n"));

    if (newlines < 1)
	return;

    /*
     * Begin at the beginning, to find the TEXTAREA, then on to find
     * the last line (anchor) in it.
     *
     * [Finding the TEXTAREA we're actually *in* with these attributes
     * isn't foolproof.  The form_num isn't unique to a given TEXTAREA,
     * and there *could* be TEXTAREA's with the same "name".  If that
     * should ever be true, we'll actually expand the *1st* TEXTAREA
     * in the page that matches.  We should probably assign a unique
     * id to each TEXTAREA in a page, and match on that, to avoid this
     * (potential) problem.
     *
     * Since the odds of "false matches" *actually* happening in real
     * life seem rather small though, we'll hold off doing this, for a
     * rainy day ...]
     */
    anchor_ptr = HTMainText->first_anchor;

    while (anchor_ptr) {

	if ((anchor_ptr->link_type == INPUT_ANCHOR) &&
	    (anchor_ptr->input_field->type == F_TEXTAREA_TYPE) &&
	    (anchor_ptr->input_field->number == form_num) &&
	    !strcmp(anchor_ptr->input_field->name, areaname)) {

	    if (firstanchor)
		firstanchor = FALSE;

	    end_anchor = anchor_ptr;

	} else {

	    if (!firstanchor)
		break;
	}
	anchor_ptr = anchor_ptr->next;
    }

    for (i = 1; i <= newlines; i++) {
	insert_new_textarea_anchor(&end_anchor, &htline);

	/*
	 * Make the new line blank.
	 */
	StrAllocCopy(end_anchor->input_field->value, "");

	/*
	 * And go add another line ...
	 */
	if (end_anchor->next)
	    match_tag = end_anchor->next->number;
    }

    CTRACE((tfp, "GridText: %d blank line(s) added to TEXTAREA name=|%s|\n",
	    newlines, areaname));

    /*
     * We need to adjust various things in all anchor bearing lines
     * following the last newly added line/anchor.  Fun stuff.
     */
    update_subsequent_anchors(newlines, end_anchor, htline, match_tag);

    CTRACE((tfp, "GridText: exiting HText_ExpandTextarea()\n"));

    return;
}

/*
 * Insert the contents of a file into a TEXTAREA between the cursor line,
 * and the line preceding it.
 *
 * Returns the number of lines that the cursor should be moved so that it
 * will end up on the 1st line in the TEXTAREA following the inserted file
 * (if we decide to do that).
 *
 * --KED 02/21/99
 */
int HText_InsertFile(LinkInfo * form_link)
{
    struct stat stat_info;
    size_t size;

    FILE *fp;
    char *fn;

    TextAnchor *anchor_ptr;
    TextAnchor *prev_anchor = NULL;
    TextAnchor *end_anchor = NULL;
    BOOLEAN firstanchor = TRUE;
    BOOLEAN truncalert = FALSE;

    FormInfo *form = form_link->l_form;
    char *areaname = form->name;
    int form_num = form->number;

    HTLine *htline = NULL;

    TextAnchor *a = 0;
    FormInfo *f = 0;
    HTLine *l = 0;

    char *fbuf;
    char *line;
    char *lp;
    char *cp;
    int entry_line = form_link->anchor_line_num;
    int file_cs;
    int match_tag = 0;
    int newlines = 0;
    int len;
    int i;

    CTRACE((tfp, "GridText: entered HText_InsertFile()\n"));

    /*
     * Get the filename of the insert file.
     */
    if (!(fn = GetFileName())) {
	HTInfoMsg(FILE_INSERT_CANCELLED);
	CTRACE((tfp,
		"GridText: file insert cancelled - no filename provided\n"));
	return (0);
    }
    if (no_dotfiles || !show_dotfiles) {
	if (*LYPathLeaf(fn) == '.') {
	    HTUserMsg(FILENAME_CANNOT_BE_DOT);
	    return (0);
	}
    }

    /*
     * Read it into our buffer (abort on 0-length file).
     */
    if ((stat(fn, &stat_info) < 0) ||
	((size = stat_info.st_size) == 0)) {
	HTInfoMsg(FILE_INSERT_0_LENGTH);
	CTRACE((tfp,
		"GridText: file insert aborted - file=|%s|- was 0-length\n",
		fn));
	FREE(fn);
	return (0);

    } else {

	if ((fbuf = typecallocn(char, size + 1)) == NULL) {
	    /*
	     * This could be huge - don't exit if we don't have enough
	     * memory for it.  - kw
	     */
	    free(fn);
	    HTAlert(MEMORY_EXHAUSTED_FILE);
	    return 0;
	}

	/* Try to make the same assumption for the charset of the inserted
	 * file as we would for normal loading of that file, i.e. taking
	 * assume_local_charset and suffix mappings into account.
	 * If there is a mismatch with the display character set, characters
	 * may be displayed wrong, too bad; but the user has a chance to
	 * correct this by editing the lines, which will update f->value_cs
	 * again. - kw
	 */
	LYGetFileInfo(fn, 0, 0, 0, 0, 0, &file_cs);

	fp = fopen(fn, "r");
	if (!fp) {
	    free(fbuf);
	    free(fn);
	    HTAlert(FILE_CANNOT_OPEN_R);
	    return 0;
	}
	size = fread(fbuf, 1, size, fp);
	LYCloseInput(fp);
	FREE(fn);
	fbuf[size] = '\0';	/* Terminate! - kw */
    }

    /*
     * Begin at the beginning, to find the TEXTAREA we're in, then
     * the current cursorline.
     *
     * [Finding the TEXTAREA we're actually *in* with these attributes
     * isn't foolproof.  The form_num isn't unique to a given TEXTAREA,
     * and there *could* be TEXTAREA's with the same "name".  If that
     * should ever be true, we'll actually insert data into the *1st*
     * TEXTAREA in the page that matches.  We should probably assign
     * a unique id to each TEXTAREA in a page, and match on that, to
     * avoid this (potential) problem.
     *
     * Since the odds of "false matches" *actually* happening in real
     * life seem rather small though, we'll hold off doing this, for a
     * rainy day ...]
     */
    anchor_ptr = HTMainText->first_anchor;

    while (anchor_ptr) {

	if ((anchor_ptr->link_type == INPUT_ANCHOR) &&
	    (anchor_ptr->input_field->type == F_TEXTAREA_TYPE) &&
	    (anchor_ptr->input_field->number == form_num) &&
	    !strcmp(anchor_ptr->input_field->name, areaname)) {

	    if (anchor_ptr->line_num == entry_line)
		break;
	}
	prev_anchor = anchor_ptr;
	anchor_ptr = anchor_ptr->next;
    }

    /*
     * Clone a new TEXTAREA line/anchor using the cursorline anchor as
     * a template, but link it in BEFORE the cursorline anchor/htline.
     *
     * [We can probably combine this with insert_new_textarea_anchor()
     * along with a flag to indicate "insert before" as we do here,
     * or the "normal" mode of operation (add after "current" anchor/
     * line).  Beware of the differences ...  some are a bit subtle to
     * notice.]
     */
    for (htline = FirstHTLine(HTMainText), i = 0;
	 anchor_ptr->line_num != i; i++) {
	htline = htline->next;
	if (htline == HTMainText->last_line)
	    break;
    }

    allocHTLine(l, MAX_LINE);
    POOLtypecalloc(TextAnchor, a);

    POOLtypecalloc(FormInfo, f);
    if (a == NULL || l == NULL || f == NULL)
	outofmem(__FILE__, "HText_InsertFile");

    /*  Init all the fields in the new TextAnchor.                 */
    /*  [anything "special" needed based on ->show_anchor value ?] */
    a->next = anchor_ptr;
    a->number = anchor_ptr->number;
    a->line_pos = anchor_ptr->line_pos;
    a->extent = anchor_ptr->extent;
    a->sgml_offset = SGML_offset();
    a->line_num = anchor_ptr->line_num;
    LYCopyHiText(a, anchor_ptr);
    a->link_type = anchor_ptr->link_type;
    a->input_field = f;
    a->show_anchor = anchor_ptr->show_anchor;
    a->inUnderline = anchor_ptr->inUnderline;
    a->expansion_anch = TRUE;
    a->anchor = NULL;

    /*  Just the (seemingly) relevant fields in the new FormInfo.  */
    /*  [do we need to do anything "special" based on ->disabled]  */
    StrAllocCopy(f->name, anchor_ptr->input_field->name);
    f->number = anchor_ptr->input_field->number;
    f->type = anchor_ptr->input_field->type;
    StrAllocCopy(f->orig_value, "");
    f->size = anchor_ptr->input_field->size;
    f->maxlength = anchor_ptr->input_field->maxlength;
    f->no_cache = anchor_ptr->input_field->no_cache;
    f->disabled = anchor_ptr->input_field->disabled;
    f->value_cs = (file_cs >= 0) ? file_cs : current_char_set;

    /*  Init all the fields in the new HTLine (but see the #if).   */
    l->offset = htline->offset;
    l->size = htline->size;
#if defined(USE_COLOR_STYLE)
    /* dup styles[] if needed [no need in TEXTAREA (?); leave 0's] */
    l->numstyles = htline->numstyles;
    /*we fork the pointers! */
    l->styles = htline->styles;
#endif
    strcpy(l->data, htline->data);

    /*
     * If we're at the head of the TextAnchor list, the new node becomes
     * the first node.
     */
    if (anchor_ptr == HTMainText->first_anchor)
	HTMainText->first_anchor = a;

    /*
     * Link in the new TextAnchor, and corresponding HTLine.
     */
    if (prev_anchor)
	prev_anchor->next = a;

    htline = htline->prev;
    l->next = htline->next;
    l->prev = htline;
    htline->next->prev = l;
    htline->next = l;

    /*
     * update_subsequent_anchors() expects htline to point to 1st potential
     * line needing fixup; we need to do this just in case the inserted file
     * was only a single line (yes, it's pathological ...  ).
     */
    htline = htline->next;	/* ->new (current) htline, for 1st inserted line  */
    htline = htline->next;	/* ->1st potential (following) [tag] fixup htline */

    anchor_ptr = a;
    newlines++;

    /*
     * Copy each line from the insert file into the corresponding anchor
     * struct.
     *
     * Begin with the new line/anchor we just added (above the cursorline).
     */
    if ((line = (char *) malloc(MAX_LINE)) == 0)
	outofmem(__FILE__, "HText_InsertFile");

    match_tag = anchor_ptr->number;

    len = 0;
    lp = fbuf;

    while (*lp) {

	if ((cp = strchr(lp, '\n')) != 0)
	    len = cp - lp;
	else
	    len = strlen(lp);

	if (len >= MAX_LINE) {
	    if (!truncalert) {
		HTAlert(gettext("Very long lines have been truncated!"));
		truncalert = TRUE;
	    }
	    len = MAX_LINE - 1;
	    if (lp[len])
		lp[len + 1] = '\0';	/* prevent next iteration */
	}
	strncpy(line, lp, len);
	*(line + len) = '\0';

	cleanup_line_for_textarea(line, len);

	/*
	 * If not the first line from the insert file, we need to add
	 * a new line/anchor, continuing on until the buffer is empty.
	 */
	if (!firstanchor) {
	    insert_new_textarea_anchor(&end_anchor, &htline);
	    anchor_ptr = end_anchor;	/* make the new anchor current */
	    newlines++;
	}

	/*
	 * Copy the new line from the buffer into the anchor.
	 */
	StrAllocCopy(anchor_ptr->input_field->value, line);

	/*
	 * insert_new_textarea_anchor always uses current_char_set,
	 * we may want something else, so fix it up.  - kw
	 */
	if (file_cs >= 0)
	    anchor_ptr->input_field->value_cs = file_cs;

	/*
	 * And do the next line of insert text, for the next anchor ...
	 */
	lp += len;
	if (*lp)
	    lp++;

	firstanchor = FALSE;
	end_anchor = anchor_ptr;
	anchor_ptr = anchor_ptr->next;
    }

    CTRACE((tfp, "GridText: file inserted into lynx struct's\n"));

    /*
     * Now adjust various things in all anchor-bearing lines following the
     * last newly added line/anchor.  Some say this is the fun part ...
     */
    update_subsequent_anchors(newlines, end_anchor, htline, match_tag);

    /*
     * Cleanup time.
     */
    FREE(line);
    FREE(fbuf);

    CTRACE((tfp, "GridText: exiting HText_InsertFile()\n"));

    return (newlines);
}

#ifdef USE_COLOR_STYLE
static int GetColumn(void)
{
    int result;

#ifdef USE_SLANG
    result = SLsmg_get_column();
#else
    int y, x;

    LYGetYX(y, x);
    result = x;
    (void) y;
#endif
    return result;
}

static BOOL DidWrap(int y0, int x0)
{
    BOOL result = NO;

#ifndef USE_SLANG
    int y, x;

    LYGetYX(y, x);
    (void) x0;
    if (x >= DISPLAY_COLS || ((x == 0) && (y != y0)))
	result = YES;
#endif
    return result;
}
#endif /* USE_COLOR_STYLE */

/*
 * This function draws the part of line 'line', pointed by 'str' (which can be
 * non terminated with null - i.e., is line->data+N) drawing 'len' bytes (not
 * characters) of it.  It doesn't check whether the 'len' bytes crosses a
 * character boundary (if multibyte chars are in string).  Assumes that the
 * cursor is positioned in the place where the 1st char of string should be
 * drawn.
 *
 * This code is based on display_line.  This code was tested with ncurses only
 * (since no support for lss is availble for Slang) -HV.
 */
#ifdef USE_COLOR_STYLE
static void redraw_part_of_line(HTLine *line, const char *str,
				int len,
				HText *text)
{
    register int i;
    char buffer[7];
    const char *data, *end_of_data;
    size_t utf_extra = 0;

#ifdef USE_COLOR_STYLE
    int current_style = 0;
    int tcols, scols;
#endif
    char LastDisplayChar = ' ';
    int YP, XP;

    LYGetYX(YP, XP);

    i = XP;

    /* Set up the multibyte character buffer  */
    buffer[0] = buffer[1] = buffer[2] = '\0';

    data = str;
    end_of_data = data + len;
    i++;

    /* this assumes that the part of line to be drawn fits in the screen */
    while (data < end_of_data) {
	buffer[0] = *data;
	data++;

#if defined(USE_COLOR_STYLE)
#define CStyle line->styles[current_style]

	tcols = GetColumn();
	scols = StyleToCols(text, line, current_style);

	while (current_style < line->numstyles &&
	       tcols >= scols) {
	    LynxChangeStyle(CStyle.style, CStyle.direction);
	    current_style++;
	    scols = StyleToCols(text, line, current_style);
	}
#endif
	switch (buffer[0]) {

#ifndef USE_COLOR_STYLE
	case LY_UNDERLINE_START_CHAR:
	    if (dump_output_immediately && use_underscore) {
		LYaddch('_');
		i++;
	    } else {
		lynx_start_underline();
	    }
	    break;

	case LY_UNDERLINE_END_CHAR:
	    if (dump_output_immediately && use_underscore) {
		LYaddch('_');
		i++;
	    } else {
		lynx_stop_underline();
	    }
	    break;

	case LY_BOLD_START_CHAR:
	    lynx_start_bold();
	    break;

	case LY_BOLD_END_CHAR:
	    lynx_stop_bold();
	    break;

#endif
	case LY_SOFT_NEWLINE:
	    if (!dump_output_immediately) {
		LYaddch('+');
		i++;
	    }
	    break;

	case LY_SOFT_HYPHEN:
	    if (*data != '\0' ||
		isspace(UCH(LastDisplayChar)) ||
		LastDisplayChar == '-') {
		/*
		 * Ignore the soft hyphen if it is not the last character in
		 * the line.  Also ignore it if it first character following
		 * the margin, or if it is preceded by a white character (we
		 * loaded 'M' into LastDisplayChar if it was a multibyte
		 * character) or hyphen, though it should have been excluded by
		 * HText_appendCharacter() or by split_line() in those cases. 
		 * -FM
		 */
		break;
	    } else {
		/*
		 * Make it a hard hyphen and fall through.  -FM
		 */
		buffer[0] = '-';
	    }
	    /* FALLTHRU */

	default:
	    if (text->T.output_utf8 && is8bits(buffer[0])) {
		utf_extra = utf8_length(text->T.output_utf8, data - 1);
		LastDisplayChar = 'M';
	    }
	    if (utf_extra) {
		strncpy(&buffer[1], data, utf_extra);
		buffer[utf_extra + 1] = '\0';
		LYaddstr(buffer);
		buffer[1] = '\0';
		data += utf_extra;
		utf_extra = 0;
	    } else if (is_CJK2(buffer[0])) {
		/*
		 * For CJK strings, by Masanobu Kimura.
		 */
		if (i <= DISPLAY_COLS) {
		    buffer[1] = *data;
		    buffer[2] = '\0';
		    data++;
		    i++;
		    LYaddstr(buffer);
		    buffer[1] = '\0';
		    /*
		     * For now, load 'M' into LastDisplayChar, but we should
		     * check whether it's white and if so, use ' '.  I don't
		     * know if there actually are white CJK characters, and
		     * we're loading ' ' for multibyte spacing characters in
		     * this code set, but this will become an issue when the
		     * development code set's multibyte character handling is
		     * used.  -FM
		     */
		    LastDisplayChar = 'M';
		}
	    } else {
		LYaddstr(buffer);
		LastDisplayChar = buffer[0];
	    }
	    if (DidWrap(YP, XP))
		break;
	    i++;
	}			/* end of switch */
    }				/* end of while */

#ifndef USE_COLOR_STYLE
    lynx_stop_underline();
    lynx_stop_bold();
#else

    while (current_style < line->numstyles) {
	LynxChangeStyle(CStyle.style, CStyle.direction);
	current_style++;
    }

#undef CStyle
#endif
    return;
}
#endif /* USE_COLOR_STYLE */

#ifndef USE_COLOR_STYLE
/*
 * Function move_to_glyph is called from LYMoveToLink and does all
 * the real work for it.
 * The pair LYMoveToLink()/move_to_glyph() is similar to the pair
 * redraw_lines_of_link()/redraw_part_of_line(), some key differences:
 * LYMoveToLink/move_to_glyph redraw_*
 * -----------------------------------------------------------------
 * - used without color style           - used with color style
 * - handles showing WHEREIS target     - WHEREIS handled elsewhere
 * - handles only one line              - handles first two lines for
 *                                        hypertext anchors
 * - right columns position for UTF-8
 *   by redrawing as necessary
 * - currently used for highlight       - currently used for highlight
 *   ON and OFF                         OFF
 *
 * Eventually the two sets of function should be unified, and should handle
 * UTF-8 positioning, both lines of hypertext anchors, and WHEREIS in all
 * cases.  If possible.  The complex WHEREIS target logic in LYhighlight()
 * could then be completely removed.  - kw
 */
static void move_to_glyph(int YP,
			  int XP,
			  int XP_draw_min,
			  const char *data,
			  int datasize,
			  unsigned offset,
			  const char *target,
			  const char *hightext,
			  int flags,
			  BOOL utf_flag)
{
    char buffer[7];
    const char *end_of_data;
    size_t utf_extra = 0;

#if defined(SHOW_WHEREIS_TARGETS)
    const char *cp_tgt;
    int i_start_tgt = 0, i_after_tgt;
    int HitOffset, LenNeeded;
#endif /* SHOW_WHEREIS_TARGETS */
    BOOL intarget = NO;
    BOOL inunderline = NO;
    BOOL inbold = NO;
    BOOL drawing = NO;
    BOOL inU = NO;
    BOOL hadutf8 = NO;
    BOOL incurlink = NO;
    BOOL drawingtarget = NO;
    BOOL flag = NO;
    const char *sdata = data;
    char LastDisplayChar = ' ';

    int i = (int) offset;	/* FIXME: should be columns, not offset? */
    int last_i = DISPLAY_COLS;
    int XP_link = XP;		/* column of link */
    int XP_next = XP;		/* column to move to when done drawing */
    int linkvlen;

    int len;

    if (no_title)
	YP -= TITLE_LINES;

    if (flags & 1)
	flag = YES;
    if (flags & 2)
	inU = YES;
    /* Set up the multibyte character buffer  */
    buffer[0] = buffer[1] = buffer[2] = '\0';
    /*
     * Add offset, making sure that we do not
     * go over the COLS limit on the display.
     */
    if (hightext != 0) {
#ifdef WIDEC_CURSES
	len = strlen(hightext);
	last_i = i + LYstrExtent2(data, datasize);
#endif
	linkvlen = LYmbcsstrlen(hightext, utf_flag, YES);
    } else {
	linkvlen = 0;
    }
    if (i >= last_i)
	i = last_i - 1;

    /*
     * Scan through the data, making sure that we do not
     * go over the COLS limit on the display etc.
     */
    len = datasize;
    end_of_data = data + len;

#if defined(SHOW_WHEREIS_TARGETS)
    /*
     * If the target overlaps with the part of this line that
     * we are drawing, it will be emphasized.
     */
    i_after_tgt = i;
    if (target) {
	cp_tgt = LYno_attr_mb_strstr(sdata,
				     target,
				     utf_flag, YES,
				     &HitOffset,
				     &LenNeeded);
	if (cp_tgt) {
	    if ((int) offset + LenNeeded > last_i ||
		((int) offset + HitOffset >= XP + linkvlen)) {
		cp_tgt = NULL;
	    } else {
		i_start_tgt = i + HitOffset;
		i_after_tgt = i + LenNeeded;
	    }
	}
    } else {
	cp_tgt = NULL;
    }
#endif /* SHOW_WHEREIS_TARGETS */

    /*
     * Iterate through the line data from the start, keeping track of
     * the display ("glyph") position in i.  Drawing will be turned
     * on when either the first UTF-8 sequence (that occurs after
     * XP_draw_min) is found, or when we reach the link itself (if
     * highlight is non-NULL).  - kw
     */
    while ((i <= last_i) && data < end_of_data && (*data != '\0')) {

	if (data && hightext && i >= XP && !incurlink) {

	    /*
	     * We reached the position of link itself, and hightext is
	     * non-NULL.  We switch data from being a pointer into the HTLine
	     * to being a pointer into hightext.  Normally (as long as this
	     * routine is applied to normal hyperlink anchors) the text in
	     * hightext will be identical to that part of the HTLine that
	     * data was already pointing to, except that special attribute
	     * chars LY_BOLD_START_CHAR etc., have been stripped out (see
	     * HText_trimHightext).  So the switching should not result in
	     * any different display, but it ensures that it doesn't go
	     * unnoticed if somehow hightext got messed up somewhere else.
	     * This is also useful in preparation for using this function
	     * for something else than normal hyperlink anchors, i.e., form
	     * fields.
	     * Turn on drawing here or make sure it gets turned on before the
	     * next actual normal character is handled.  - kw
	     */
	    data = hightext;
	    len = strlen(hightext);
	    end_of_data = hightext + len;
	    last_i = i + len;
	    XP_next += linkvlen;
	    incurlink = YES;
#ifdef SHOW_WHEREIS_TARGETS
	    if (cp_tgt) {
		if (flag && i_after_tgt >= XP)
		    i_after_tgt = XP - 1;
	    }
#endif
	    /*
	     * The logic of where to set in-target drawing target etc.
	     * and when to react to it should be cleaned up (here and
	     * further below).  For now this seems to work but isn't
	     * very clear.  The complications arise from reproducing
	     * the behavior (previously done in LYhighlight()) for target
	     * strings that fall into or overlap a link:  use target
	     * emphasis for the target string, except for the first
	     * and last character of the anchor text if the anchor is
	     * highlighted as "current link".  - kw
	     */
	    if (!drawing) {
#ifdef SHOW_WHEREIS_TARGETS
		if (intarget) {
		    if (i_after_tgt > i) {
			LYmove(YP, i);
			if (flag) {
			    drawing = YES;
			    drawingtarget = NO;
			    if (inunderline)
				inU = YES;
			    lynx_start_link_color(flag, inU);
			} else {
			    drawing = YES;
			    drawingtarget = YES;
			    LYstartTargetEmphasis();
			}
		    }
		}
#endif /* SHOW_WHEREIS_TARGETS */
	    } else {
#ifdef SHOW_WHEREIS_TARGETS
		if (intarget && i_after_tgt > i) {
		    if (flag && (data == hightext)) {
			drawingtarget = NO;
			LYstopTargetEmphasis();
		    }
		} else if (!intarget)
#endif /* SHOW_WHEREIS_TARGETS */
		{
		    if (inunderline)
			inU = YES;
		    if (inunderline)
			lynx_stop_underline();
		    if (inbold)
			lynx_stop_bold();
		    lynx_start_link_color(flag, inU);
		}

	    }
	}
	if (i >= last_i || data >= end_of_data)
	    break;
	if ((buffer[0] = *data) == '\0')
	    break;
#if defined(SHOW_WHEREIS_TARGETS)
	/*
	 * Look for a subsequent occurrence of the target string,
	 * if we had a previous one and have now stepped past it.  - kw
	 */
	if (cp_tgt && i >= i_after_tgt) {
	    if (intarget) {

		if (incurlink && flag && i == last_i - 1)
		    cp_tgt = NULL;
		else
		    cp_tgt = LYno_attr_mb_strstr(sdata,
						 target,
						 utf_flag, YES,
						 &HitOffset,
						 &LenNeeded);
		if (cp_tgt) {
		    i_start_tgt = i + HitOffset;
		    i_after_tgt = i + LenNeeded;
		    if (incurlink) {
			if (flag && i_start_tgt == XP_link)
			    i_start_tgt++;
			if (flag && i_start_tgt == last_i - 1)
			    i_start_tgt++;
			if (flag && i_after_tgt >= last_i)
			    i_after_tgt = last_i - 1;
			if (flag && i_start_tgt >= last_i)
			    cp_tgt = NULL;
		    } else if (i_start_tgt == last_i) {
			if (flag)
			    i_start_tgt++;
		    }
		}
		if (!cp_tgt || i_start_tgt != i) {
		    intarget = NO;
		    if (drawing) {
			if (drawingtarget) {
			    drawingtarget = NO;
			    LYstopTargetEmphasis();
			    if (incurlink) {
				lynx_start_link_color(flag, inU);
			    }
			}
			if (!incurlink) {
			    if (inbold)
				lynx_start_bold();
			    if (inunderline)
				lynx_start_underline();
			}
		    }
		}
	    }
	}
#endif /* SHOW_WHEREIS_TARGETS */

	/*
	 * Advance data to point to the next input char (for the
	 * next round).  Advance sdata, used for searching for a
	 * target string, so that they stay in synch.  As long
	 * as we are not within the highlight text, data and sdata
	 * have identical values.  After we have switched data to
	 * point into hightext, sdata remains a pointer into the
	 * HTLine (so that we don't miss a partial target match at
	 * the end of the anchor text).  So sdata has to sometimes
	 * skip additional special attribute characters that are
	 * not present in highlight in order to stay in synch.  - kw
	 */
	data++;
	if (incurlink) {
	    while (IsNormalChar(*sdata)) {
		++sdata;
	    }
	}

	switch (buffer[0]) {

	case LY_UNDERLINE_START_CHAR:
	    if (!drawing || !incurlink)
		inunderline = YES;
	    if (drawing && !intarget && !incurlink)
		lynx_start_underline();
	    break;

	case LY_UNDERLINE_END_CHAR:
	    inunderline = NO;
	    if (drawing && !intarget && !incurlink)
		lynx_stop_underline();
	    break;

	case LY_BOLD_START_CHAR:
	    if (!drawing || !incurlink)
		inbold = YES;
	    if (drawing && !intarget && !incurlink)
		lynx_start_bold();
	    break;

	case LY_BOLD_END_CHAR:
	    inbold = NO;
	    if (drawing && !intarget && !incurlink)
		lynx_stop_bold();
	    break;

	case LY_SOFT_NEWLINE:
	    if (drawing) {
		LYaddch('+');
	    }
	    i++;
	    break;

	case LY_SOFT_HYPHEN:
	    if (*data != '\0' ||
		isspace(UCH(LastDisplayChar)) ||
		LastDisplayChar == '-') {
		/*
		 * Ignore the soft hyphen if it is not the last
		 * character in the line.  Also ignore it if it
		 * first character following the margin, or if it
		 * is preceded by a white character (we loaded 'M'
		 * into LastDisplayChar if it was a multibyte
		 * character) or hyphen, though it should have
		 * been excluded by HText_appendCharacter() or by
		 * split_line() in those cases.  -FM
		 */
		break;
	    } else {
		/*
		 * Make it a hard hyphen and fall through.  -FM
		 */
		buffer[0] = '-';
	    }
	    /* FALLTHRU */

	default:
	    /*
	     * We have got an actual normal displayable character, or
	     * the start of one.  Before proceeding check whether
	     * drawing needs to be turned on now.  - kw
	     */
#if defined(SHOW_WHEREIS_TARGETS)
	    if (incurlink && intarget && flag && i_after_tgt > i) {
		if (i == last_i - 1) {
		    i_after_tgt = i;
		} else if (i == last_i - 2 && HTCJK != NOCJK &&
			   is8bits(buffer[0])) {
		    i_after_tgt = i;
		    cp_tgt = NULL;
		    if (drawing) {
			if (drawingtarget) {
			    LYstopTargetEmphasis();
			    drawingtarget = NO;
			    lynx_start_link_color(flag, inU);
			}
		    }
		}
	    }
	    if (cp_tgt && i >= i_start_tgt && sdata > cp_tgt) {
		if (!intarget ||
		    (intarget && incurlink && !drawingtarget)) {

		    if (incurlink && drawing &&
			!(flag &&
			  (i == XP_link || i == last_i - 1))) {
			lynx_stop_link_color(flag, inU);
		    }
		    if (incurlink && !drawing) {
			LYmove(YP, i);
			if (inunderline)
			    inU = YES;
			if (flag && (i == XP_link || i == last_i - 1)) {
			    lynx_start_link_color(flag, inU);
			    drawingtarget = NO;
			} else {
			    LYstartTargetEmphasis();
			    drawingtarget = YES;
			}
			drawing = YES;
		    } else if (incurlink && drawing &&
			       intarget && !drawingtarget &&
			       (flag &&
				(i == XP_link))) {
			if (inunderline)
			    inU = YES;
			lynx_start_link_color(flag, inU);
		    } else if (drawing &&
			       !(flag &&
				 (i == XP_link || (incurlink && i == last_i - 1)))) {
			LYstartTargetEmphasis();
			drawingtarget = YES;
		    }
		    intarget = YES;
		}
	    } else
#endif /* SHOW_WHEREIS_TARGETS */
	    if (incurlink) {
		if (!drawing) {
		    LYmove(YP, i);
		    if (inunderline)
			inU = YES;
		    lynx_start_link_color(flag, inU);
		    drawing = YES;
		}
	    }

	    i++;
#ifndef WIDEC_CURSES
	    if (utf_flag && is8bits(buffer[0])) {
		hadutf8 = YES;
		utf_extra = utf8_length(utf_flag, data - 1);
		LastDisplayChar = 'M';
	    }
#endif
	    if (utf_extra) {
		strncpy(&buffer[1], data, utf_extra);
		buffer[utf_extra + 1] = '\0';
		if (!drawing && i >= XP_draw_min) {
		    LYmove(YP, i - 1);
		    drawing = YES;
#if defined(SHOW_WHEREIS_TARGETS)
		    if (intarget) {
			drawingtarget = YES;
			LYstartTargetEmphasis();
		    } else
#endif /* SHOW_WHEREIS_TARGETS */
		    {
			if (inbold)
			    lynx_start_bold();
			if (inunderline)
			    lynx_start_underline();
		    }
		}
		LYaddstr(buffer);
		buffer[1] = '\0';
		sdata += utf_extra;
		data += utf_extra;
		utf_extra = 0;
	    } else if (HTCJK != NOCJK && is8bits(buffer[0])) {
		/*
		 * For CJK strings, by Masanobu Kimura.
		 */
		if (drawing && (i <= last_i)) {
		    buffer[1] = *data;
		    LYaddstr(buffer);
		    buffer[1] = '\0';
		}
		i++;
		sdata++;
		data++;
		/*
		 * For now, load 'M' into LastDisplayChar, but we should
		 * check whether it's white and if so, use ' '.  I don't
		 * know if there actually are white CJK characters, and
		 * we're loading ' ' for multibyte spacing characters in
		 * this code set, but this will become an issue when the
		 * development code set's multibyte character handling is
		 * used.  -FM
		 */
		LastDisplayChar = 'M';
	    } else {
		if (drawing) {
		    LYaddstr(buffer);
		}
		LastDisplayChar = buffer[0];
	    }
	}			/* end of switch */
    }				/* end of while */

    if (!drawing) {
	LYmove(YP, XP_next);
	lynx_start_link_color(flag, inU);
    } else {
#if defined(SHOW_WHEREIS_TARGETS)
	if (drawingtarget) {
	    LYstopTargetEmphasis();
	    lynx_start_link_color(flag, inU);
	}
#endif /* SHOW_WHEREIS_TARGETS */
	if (hadutf8) {
	    LYtouchline(YP);
	}
    }
    return;
}
#endif /* !USE_COLOR_STYLE */

#ifndef USE_COLOR_STYLE
/*
 * Move cursor position to a link's place in the display.
 * The "moving to" is done by scanning through the line's
 * character data in the corresponding HTLine of HTMainText,
 * and starting to draw when a UTF-8 encoded non-ASCII character
 * is encountered before the link (with some protection against
 * overwriting form fields).  This refreshing of preceding data is
 * necessary for preventing curses's or slang's display logic from
 * getting too clever; their logic counts character positions wrong
 * since they don't know about multi-byte characters that take up
 * only one screen position.  So we have to make them forget their
 * idea of what's in a screen line drawn previously.
 * If hightext is non-NULL, it should be the anchor text for a normal
 * link as stored in a links[] element, and the anchor text will be
 * drawn too, with appropriate attributes.  - kw
 */
void LYMoveToLink(int cur,
		  const char *target,
		  const char *hightext,
		  int flag,
		  BOOL inU,
		  BOOL utf_flag)
{
#define pvtTITLE_HEIGHT 1
    HTLine *todr;
    int i, n = 0;
    int XP_draw_min = 0;
    int flags = ((flag == ON) ? 1 : 0) | (inU ? 2 : 0);

    /*
     * We need to protect changed form text fields preceding this
     * link on the same line against overwriting.  - kw
     */
    for (i = cur - 1; i >= 0; i++) {
	if (links[i].ly < links[cur].ly)
	    break;
	if (links[i].type == WWW_FORM_LINK_TYPE) {
	    XP_draw_min = links[i].ly + links[i].l_form->size;
	    break;
	}
    }

    /*  Find the right HTLine. */
    if (!HTMainText) {
	todr = NULL;
    } else if (HTMainText->stale) {
	todr = FirstHTLine(HTMainText);
	n = links[cur].ly - pvtTITLE_HEIGHT + HTMainText->top_of_screen;
    } else {
	todr = HTMainText->top_of_screen_line;
	n = links[cur].ly - pvtTITLE_HEIGHT;
    }
    for (i = 0; i < n && todr; i++) {
	todr = (todr == HTMainText->last_line) ? NULL : todr->next;
    }
    if (todr) {
	if (target && *target == '\0')
	    target = NULL;
	move_to_glyph(links[cur].ly, links[cur].lx, XP_draw_min,
		      todr->data, todr->size, todr->offset,
		      target, hightext, flags, utf_flag);
    } else {
	/*  This should not happen. */
	move_to_glyph(links[cur].ly, links[cur].lx, XP_draw_min,
		      "", 0, links[cur].lx,
		      target, hightext, flags, utf_flag);
    }
}
#endif /* !USE_COLOR_STYLE */

/*
 * This is used only if compiled with lss support.  It's called to redraw a
 * regular link when it's being unhighlighted in LYhighlight().
 */
#ifdef USE_COLOR_STYLE
void redraw_lines_of_link(int cur)
{
#define pvtTITLE_HEIGHT 1
    HTLine *todr1;
    int lines_back;
    int row, col, count;
    const char *text;

    if (HTMainText->next_line == HTMainText->last_line) {
	/* we are at the last page - that is partially filled */
	lines_back = HTMainText->Lines - (links[cur].ly - pvtTITLE_HEIGHT +
					  HTMainText->top_of_screen);
    } else {
	lines_back = display_lines - (links[cur].ly - pvtTITLE_HEIGHT);
    }
    todr1 = HTMainText->next_line;
    while (lines_back-- > 0)
	todr1 = todr1->prev;

    row = links[cur].ly;
    if (no_title)
	row -= TITLE_LINES;

    for (count = 0;
	 row <= display_lines && (text = LYGetHiliteStr(cur, count)) != NULL;
	 ++count) {
	col = LYGetHilitePos(cur, count);
	LYmove(row++, col);
	redraw_part_of_line(todr1, text, strlen(text), HTMainText);
	todr1 = todr1->next;
    }
#undef pvtTITLE_HEIGHT
    return;
}
#endif

#ifdef USE_PRETTYSRC
void HTMark_asSource(void)
{
    if (HTMainText)
	HTMainText->source = TRUE;
}
#endif

HTkcode HText_getKcode(HText *text)
{
    return text->kcode;
}

void HText_updateKcode(HText *text, HTkcode kcode)
{
    text->kcode = kcode;
}

HTkcode HText_getSpecifiedKcode(HText *text)
{
    return text->specified_kcode;
}

void HText_updateSpecifiedKcode(HText *text, HTkcode kcode)
{
    text->specified_kcode = kcode;
}

int HTMainText_Get_UCLYhndl(void)
{
    return (HTMainText ?
	    HTAnchor_getUCLYhndl(HTMainText->node_anchor, UCT_STAGE_MIME)
	    : -1);
}
