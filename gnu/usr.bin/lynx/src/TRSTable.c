/*		Simple table object
 *		===================
 * Authors
 *	KW	Klaus Weide <kweide@enteract.com>
 * History:
 *   2 Jul 1999	KW	Created.
 */

#include <HTUtils.h>
#include <HTStyle.h>		/* for HT_LEFT, HT_CENTER, HT_RIGHT */
#include <LYCurses.h>
#include <TRSTable.h>
#include <LYGlobalDefs.h>

#include <LYLeaks.h>

#ifdef SAVE_TIME_NOT_SPACE
#define CELLS_GROWBY 16
#define ROWS_GROWBY 16
#else
#define CELLS_GROWBY 2
#define ROWS_GROWBY 2
#endif

#ifdef USE_CURSES_PADS
#  define MAX_STBL_POS (LYwideLines ? MAX_COLS - 1 : LYcolLimit)
#else
#  define MAX_STBL_POS (LYcolLimit)
#endif

/* must be different from HT_ALIGN_NONE and HT_LEFT, HT_CENTER etc.: */
#define RESERVEDCELL (-2)	/* cell's alignment field is overloaded, this
				   value means cell was reserved by ROWSPAN */
#define EOCOLG (-2)		/* sumcols' Line field isn't used for line info, this
				   special value means end of COLGROUP */
#ifndef NO_AGGRESSIVE_NEWROW
#  define NO_AGGRESSIVE_NEWROW	0
#endif

typedef enum {
    CS_invalid = -1,		/* cell "before the first",
				   or empty lines after [ce]bc,
				   or TRST aborted */
    CS__new = 0,
    CS__0new,			/* new, at BOL */
    CS__0eb,			/* starts at BOL, empty, break */
    CS__eb,			/* empty, break */
    CS__0cb,			/* starts at BOL, content, break */
    CS__cb,			/* content, break */
    CS__0ef,			/* starts at BOL, empty, finished */
    CS__ef,			/* empty, finished */
    CS__0cf,			/* starts at BOL, content, finished */
    CS__cf,			/* content, finished */
    CS__ebc,			/* empty, break, more content (maybe @BOL) */
    CS__cbc			/* content, break, more content (maybe @BOL) */
} cellstate_t;

typedef struct _STable_states {
    cellstate_t prev_state;	/* Contents type of the previous cell */
    cellstate_t state;		/* Contents type of the worked-on cell */
    int lineno;			/* Start line of the current cell */
    int icell_core;		/* -1 or the 1st cell with <BR></TD> on row */
    int x_td;			/* x start pos of the current cell or -1 */
    int pending_len;		/* For multiline cells, the length of
				   the part on the first line (if
				   state is CS__0?[ec]b) (??), or 0 */
} STable_states;

typedef struct _STable_cellinfo {
    int cLine;			/* lineno in doc (zero-based): -1 for
				   contentless cells (and cells we do
				   not want to measure and count?),
				   line-of-the-start otherwise.  */
    int pos;			/* column where cell starts */
    int len;			/* number of character positions */
    int colspan;		/* number of columns to span */
    int alignment;		/* one of HT_LEFT, HT_CENTER, HT_RIGHT,
				   or RESERVEDCELL */
} STable_cellinfo;

enum ended_state {
    ROW_not_ended,
    ROW_ended_by_endtr,
    ROW_ended_by_splitline
};

#define HAS_END_OF_CELL			1
#define HAS_BEG_OF_CELL			2
#define IS_CONTINUATION_OF_CELL		4
#define OFFSET_IS_VALID			8
#define OFFSET_IS_VALID_LAST_CELL	0x10
#define BELIEVE_OFFSET			0x20

typedef struct _STable_rowinfo {
    /* Each row may be displayed on many display lines, but we fix up
       positions of cells on this display line only: */
    int Line;			/* lineno in doc (zero-based) */
    int ncells;			/* number of table cells */

    /* What is the meaning of this?!  It is set if:
       [search for      def of fixed_line       below]

       a1) a non-last cell is not at BOL,
       a2) a non-last cell has something on the first line,
       b) a >=3-lines-cell not at BOL, the first row non-empty, the 2nd empty;
       c) a multiline cell not at BOL, the first row non-empty, the rest empty;
       d) a multiline cell not at BOL, the first row non-empty;
       e) a singleline non-empty cell not at BOL;

       Summary: have seen a cell which is one of:
       (Notation: B: at BOL; L: last; E: the first row is non-empty)

       bcde:    !B && !E
       a1:      !L && !B
       a2:      !L && !E

       Or: has at least two of !B, !L, !E, or: has at most one of B,L,E.

       REMARK: If this variable is not set, but icell_core is, Line is
       reset to the line of icell_core.
     */
    BOOL fixed_line;		/* if we have a 'core' line of cells */
    enum ended_state ended;	/* if we saw </tr> etc */
    int content;		/* Whether contains end-of-cell etc */
    int offset;			/* >=0 after line break in a multiline cell */
    int allocated;		/* number of table cells allocated */
    STable_cellinfo *cells;
    int alignment;		/* global align attribute for this row */
} STable_rowinfo;

struct _STable_info {
#ifdef EXP_NESTED_TABLES
    struct _STable_info *enclosing;	/* The table which contain us */
    struct _TextAnchor *enclosing_last_anchor_before_stbl;
#endif
    int startline;		/* lineno where table starts (zero-based) */
    int nrows;			/* number of rows */
    int ncols;			/* number of rows */
    int maxlen;			/* sum of max. cell lengths of any row */
    int maxpos;			/* max. of max. cell pos's of any row */
    int allocated_rows;		/* number of rows allocated */
    int allocated_sumcols;	/* number of sumcols allocated */
    int ncolinfo;		/* number of COL info collected */
    STable_cellinfo *sumcols;	/* for summary (max len/pos) col info */
    STable_rowinfo *rows;
    STable_rowinfo rowspans2eog;
    short alignment;		/* global align attribute for this table */
    short rowgroup_align;	/* align default for current group of rows */
    short pending_colgroup_align;
    int pending_colgroup_next;
    STable_states s;
};

/*
 *  Functions and structures in this source file keep track of positions.
 *  They don't know about the character data in those lines, or about
 *  the HText and HTLine structures.  GridText.c doesn't know about our
 *  structures.  It should stay that way.
 *
 *  The basic idea: we let the code in HTML.c/GridText.c produce and format
 *  output "as usual", i.e. as without Simple Table support.  We keep track
 *  of the positions in the generated output where cells and rows start (or
 *  end).  If all goes well, that preliminary output (stored in HText/HTLine
 *  structures) can be fixed up when the TABLE end tag is processed, by just
 *  inserting spaces in the right places (and possibly changing alignment).
 *  If all goes not well, we already have a safe fallback.
 *
 *  Note that positions passed to and from these functions should be
 *  in terms of screen positions, not just byte counts in a HTLine.data
 *  (cf. line->data vs. HText_TrueLineSize).
 *
 *  Memory is allocated dynamically, so we can have tables of arbitrary
 *  length.  On allocation error we just return and error indication
 *  instead of outofmem(), so caller can give up table tracking and maybe
 *  recover memory.
 *
 *  Implemented:
 *  - ALIGN={left,right,center,justify} applied to individual table cells
 *    ("justify" is treated as "left")
 *  - Inheritance of horizontal alignment according to HTML 4.0
 *  - COLSPAN >1 (may work incorrectly for some tables?)
 *  - ROWSPAN >1 (reserving cells in following rows)
 *  - Line breaks at start of first cell or at end of last cell are treated
 *    as if they were not part of the cell and row.  This allows us to
 *    cooperate with one way in which tables have been made friendly to
 *    browsers without any table support.
 *  Missing, but can be added:
 *  - Support for COLGROUP/COL
 *  - Tables wider than display.  The limitation is not here but in GridText.c
 *    etc.  If horizontal scrolling were implemented there, the mechanisms
 *    here coudl deal with wide tables (just change MAX_STBL_POS code).
 *  Missing, unlikely to add:
 *  - Support for non-LTR directionality.  A general problem, support is
 *    lacking throughout the lynx code.
 *  - Support for most other table-related attributes.  Most of them are
 *    for decorative purposes.
 *  Impossible or very unlikely (because it doesn't fit the model):
 *  - Any cell contents of more than one line, line breaks within cells.
 *    Anything that requires handling cell contents as paragraphs (block
 *    elements), like reflowing.  Vertical alignment.
 */
static int Stbl_finishCellInRow(STable_rowinfo *me, STable_states *s, int end_td,
				int lineno,
				int pos);
static int Stbl_finishRowInTable(STable_info *me);

static const char *cellstate_s(cellstate_t state)
{
    const char *result = "?";
    /* *INDENT-OFF* */
    switch (state) {
    case CS_invalid:	result = "CS_invalid";	break;
    case CS__new:	result = "CS__new";	break;
    case CS__0new:	result = "CS__0new";	break;
    case CS__0eb:	result = "CS__0eb";	break;
    case CS__eb:	result = "CS__eb";	break;
    case CS__0cb:	result = "CS__0cb";	break;
    case CS__cb:	result = "CS__cb";	break;
    case CS__0ef:	result = "CS__0ef";	break;
    case CS__ef:	result = "CS__ef";	break;
    case CS__0cf:	result = "CS__0cf";	break;
    case CS__cf:	result = "CS__cf";	break;
    case CS__ebc:	result = "CS__ebc";	break;
    case CS__cbc:	result = "CS__cbc";	break;
    }
    /* *INDENT-ON* */

    return result;
}

struct _STable_info *Stbl_startTABLE(short alignment)
{
    STable_info *me = typecalloc(STable_info);

    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_startTABLE(align=%d)\n", (int) alignment));
    if (me) {
	me->alignment = alignment;
	me->rowgroup_align = HT_ALIGN_NONE;
	me->pending_colgroup_align = HT_ALIGN_NONE;
	me->s.x_td = -1;
	me->s.icell_core = -1;
#ifdef EXP_NESTED_TABLES
	if (nested_tables)
	    me->enclosing = 0;
#endif
    }
    return me;
}

static void free_rowinfo(STable_rowinfo *me)
{
    if (me && me->allocated) {
	FREE(me->cells);
    }
}

void Stbl_free(STable_info *me)
{
    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_free()\n"));
    if (me && me->allocated_rows && me->rows) {
	int i;

	for (i = 0; i < me->allocated_rows; i++)
	    free_rowinfo(me->rows + i);
	FREE(me->rows);
    }
    free_rowinfo(&me->rowspans2eog);
    if (me)
	FREE(me->sumcols);
    FREE(me);
}

/*
 * Returns -1 on error, otherwise index of just-added table cell.
 */
static int Stbl_addCellToRow(STable_rowinfo *me, STable_cellinfo *colinfo, int ncolinfo,
			     STable_states *s,
			     int colspan,
			     int alignment,
			     int isheader,
			     int lineno,
			     int *ppos)
{
    STable_cellinfo *cells;
    int i;
    int last_colspan = me->ncells ?
    me->cells[me->ncells - 1].colspan : 1;
    cellstate_t newstate;
    int ret;

    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_addCellToRow, line=%d, pos=%d, colspan=%d\n",
	     lineno, *ppos, colspan));
    CTRACE2(TRACE_TRST,
	    (tfp,
	     " ncells=%d, stateLine=%d, pending_len=%d, pstate=%s, state=%s\n",
	     me->ncells, s->lineno, s->pending_len,
	     cellstate_s(s->prev_state), cellstate_s(s->state)));
    if (me->ncells == 0)
	s->prev_state = CS_invalid;
    else if (s->prev_state == CS_invalid ||
	     (s->state != CS__0new &&
	      s->state != CS__ef && s->state != CS__0ef))
	s->prev_state = s->state;

    if (me->ncells == 0 || *ppos == 0)
	newstate = CS__0new;
    else
	newstate = CS__new;

    if (me->ncells > 0 && s->pending_len > 0) {
	if (s->prev_state != CS__cbc)
	    me->cells[me->ncells - 1].len = s->pending_len;
	s->pending_len = 0;
    }
    s->x_td = *ppos;

    if (lineno != s->lineno) {
	if (!me->fixed_line) {
	    if (me->ncells == 0 || *ppos == 0) {
		switch (s->prev_state) {
		case CS_invalid:
		case CS__0new:
		case CS__0eb:
		case CS__0cb:
		case CS__0ef:
		case CS__0cf:
		    if (me->ncells > 0)
			for (i = me->ncells + last_colspan - 2;
			     i >= me->ncells - 1; i--) {
			    me->cells[i].pos = *ppos;
			    me->cells[i].cLine = lineno;
			}
		    me->Line = lineno;
		    break;
		case CS__new:
		case CS__eb:
		case CS__ef:
		case CS__cf:
		default:
		    break;
		case CS__cb:
		    *ppos = me->cells[me->ncells - 1].pos +
			me->cells[me->ncells - 1].len;
		}
	    } else {		/* last cell multiline, ncells != 0, pos != 0 */
		switch (s->prev_state) {
		case CS__0new:
		case CS__0eb:
		case CS__0ef:
		    /* Do not fail, but do not set fixed_line either */
		    break;
		case CS__cb:
		    goto trace_and_fail;
		case CS__cf:
		    goto trace_and_fail;
		case CS__0cb:
		case CS__0cf:
		    if (*ppos > me->cells[0].pos)
			me->Line = lineno;
		    me->fixed_line = YES;	/* type=a def of fixed_line i */
		    break;
		case CS__new:
		case CS__eb:
		case CS__ef:
		default:
		    me->fixed_line = YES;	/* type=e def of fixed_line ii */
		    break;
		case CS__cbc:
		    goto trace_and_fail;
		}
	    }
	}
	if (me->fixed_line && lineno != me->Line) {
	    switch (s->prev_state) {
	    case CS__cb:
	    case CS__cf:
		if (*ppos > 0)
		    goto trace_and_fail;
		else
		    *ppos = me->cells[me->ncells - 1].pos /* == 0 */  +
			me->cells[me->ncells - 1].len;
		break;
	    case CS__0cf:
	    case CS__0cb:
		if (*ppos == 0 || *ppos <= me->cells[0].pos)
		    *ppos = me->cells[me->ncells - 1].pos /* == 0 */  +
			me->cells[me->ncells - 1].len;
		break;
	    case CS__0new:
	    case CS__0ef:
	    case CS__0eb:
		break;
	    case CS__new:
	    case CS__eb:
	    case CS__ef:
	    default:
		*ppos = me->cells[me->ncells - 1].pos;
		break;
	    case CS__cbc:
		break;
	    case CS_invalid:
		break;
	    }
	}
	s->lineno = lineno;
    } else {			/* lineno == s->lineno: */
	switch (s->prev_state) {
	case CS_invalid:
	case CS__0new:
	case CS__0eb:		/* cannot happen */
	case CS__0cb:		/* cannot happen */
	case CS__0ef:
	case CS__0cf:		/* ##302?? set icell_core? or only in finish? */
	    break;
	case CS__eb:		/* cannot happen */
	case CS__cb:		/* cannot happen */
	case CS__ef:
	    break;
	case CS__ebc:		/* should have done smth in finish */
	case CS__cbc:		/* should have done smth in finish */
	    break;
	case CS__new:
	case CS__cf:
	    if (me->fixed_line && me->Line != lineno) {
		goto trace_and_fail;
	    } else {
		me->fixed_line = YES;
		me->Line = lineno;
	    }
	}
    }

    s->state = newstate;

    if (me->ncells > 0 && me->cells[me->ncells - 1].colspan > 1) {
	me->ncells += me->cells[me->ncells - 1].colspan - 1;
    }
    while (me->ncells < me->allocated &&
	   me->cells[me->ncells].alignment == RESERVEDCELL) {
	me->ncells++;
    }
    {
	int growby = 0;

	while (me->ncells + colspan + 1 > me->allocated + growby)
	    growby += CELLS_GROWBY;
	if (growby) {
	    if (me->allocated == 0 && !me->cells) {
		cells = typecallocn(STable_cellinfo, growby);
	    } else {
		cells = typeRealloc(STable_cellinfo, me->cells,
				      (me->allocated + growby));

		for (i = 0; cells && i < growby; i++) {
		    cells[me->allocated + i].alignment = HT_ALIGN_NONE;
		}
	    }
	    if (cells) {
		me->allocated += growby;
		me->cells = cells;
	    } else {
		goto trace_and_fail;
	    }
	}
    }

    me->cells[me->ncells].cLine = lineno;
    me->cells[me->ncells].pos = *ppos;
    me->cells[me->ncells].len = -1;
    me->cells[me->ncells].colspan = colspan;

    if (alignment != HT_ALIGN_NONE)
	me->cells[me->ncells].alignment = alignment;
    else {
	if (ncolinfo >= me->ncells + 1)
	    me->cells[me->ncells].alignment = colinfo[me->ncells].alignment;
	else
	    me->cells[me->ncells].alignment = me->alignment;
	if (me->cells[me->ncells].alignment == HT_ALIGN_NONE)
	    me->cells[me->ncells].alignment = me->alignment;
	if (me->cells[me->ncells].alignment == HT_ALIGN_NONE)
	    me->cells[me->ncells].alignment = isheader ? HT_CENTER : HT_LEFT;
    }
    for (i = me->ncells + 1; i < me->ncells + colspan; i++) {
	me->cells[i].cLine = lineno;
	me->cells[i].pos = *ppos;
	me->cells[i].len = -1;
	me->cells[i].colspan = 0;
	me->cells[i].alignment = HT_LEFT;
    }
    me->cells[me->ncells + colspan].pos = -1;	/* not yet used */
    me->ncells++;

    ret = me->ncells - 1;
  trace_and_return:
    CTRACE2(TRACE_TRST,
	    (tfp, " => prev_state=%s, state=%s, ret=%d\n",
	     cellstate_s(s->prev_state), cellstate_s(s->state), ret));
    return (ret);

  trace_and_fail:
    ret = -1;
    goto trace_and_return;
}

/* returns -1 on error, 0 otherwise */
/* assumes cells have already been allocated (but may need more) */
static int Stbl_reserveCellsInRow(STable_rowinfo *me, int icell,
				  int colspan)
{
    STable_cellinfo *cells;
    int i;
    int growby = icell + colspan - me->allocated;

    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_reserveCellsInRow(icell=%d, colspan=%d\n",
	     icell, colspan));
    if (growby > 0) {
	cells = typeRealloc(STable_cellinfo, me->cells,
			      (me->allocated + growby));

	if (cells) {
	    for (i = 0; i < growby; i++) {
		cells[me->allocated + i].alignment = HT_ALIGN_NONE;
	    }
	    me->allocated += growby;
	    me->cells = cells;
	} else {
	    return -1;
	}
    }
    for (i = icell; i < icell + colspan; i++) {
	me->cells[i].cLine = -1;
	me->cells[i].pos = -1;
	me->cells[i].len = -1;
	me->cells[i].colspan = 0;
	me->cells[i].alignment = RESERVEDCELL;
    }
    me->cells[icell].colspan = colspan;
    return 0;
}

/* Returns -1 on failure. */
static int Stbl_finishCellInRow(STable_rowinfo *me, STable_states *s, int end_td,
				int lineno,
				int pos)
{
    STable_cellinfo *lastcell;
    cellstate_t newstate = CS_invalid;
    int multiline = NO, empty;
    int ret;

    CTRACE2(TRACE_TRST,
	    (tfp,
	     "TRST:Stbl_finishCellInRow line=%d pos=%d end_td=%d ncells=%d pnd_len=%d\n",
	     lineno, pos, (int) end_td, me->ncells, s->pending_len));

    if (me->ncells <= 0)
	return -1;
    lastcell = me->cells + (me->ncells - 1);
    multiline = (lineno != lastcell->cLine || lineno != s->lineno);
    empty = multiline ? (pos == 0) : (pos <= s->x_td);

    CTRACE2(TRACE_TRST,
	    (tfp,
	     " [lines: lastCell=%d state=%d multi=%d] empty=%d (prev)state=(%s) %s\n",
	     lastcell->cLine, s->lineno, multiline, empty,
	     cellstate_s(s->prev_state), cellstate_s(s->state)));

    if (multiline) {
	if ((end_td & TRST_ENDCELL_MASK) == TRST_ENDCELL_LINEBREAK) {
	    switch (s->state) {
	    case CS_invalid:
		newstate = empty ? CS_invalid : CS__cbc;
		break;
	    case CS__0new:
		newstate = empty ? CS__0eb : CS__0cb;
		break;
	    case CS__0eb:
		newstate = empty ? CS__0eb : CS__ebc;
		s->state = newstate;
		if (me->fixed_line) {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : -1);
		} else {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : 0);
		}
		goto trace_and_return;
	    case CS__0cb:
		if (!me->fixed_line) {
		    if (!empty) {
			if (s->icell_core == -1)
			    me->Line = -1;
		    }
		}
		if (s->pending_len && empty) {	/* First line non-empty */
		    if ((me->fixed_line && me->Line == lastcell->cLine) ||
			s->icell_core == me->ncells - 1)
			lastcell->len = s->pending_len;
		    s->pending_len = 0;
		}		/* @@@ for empty do smth. about ->Line / ->icell_core !! */
		newstate = empty ? CS__0cb : CS__cbc;	/* ##474_needs_len!=-1? */
		break;
	    case CS__0ef:
	    case CS__0cf:
		break;
	    case CS__new:
		newstate = empty ? CS__eb : CS__cb;
		break;
	    case CS__eb:	/* ##484_set_pending_ret_0_if_empty? */
		newstate = empty ? CS__eb : CS__ebc;
		s->state = newstate;
		if (me->fixed_line) {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : -1);
		} else {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : -1);
		}
		goto trace_and_return;
	    case CS__cb:
		if (s->pending_len && empty) {	/* ##496: */
		    lastcell->len = s->pending_len;
		    s->pending_len = 0;
		}		/* @@@ for empty do smth. about ->Line / ->icell_core !! */
		ret = -1;
		if (empty) {
		    if (!me->fixed_line) {
			me->fixed_line = YES;	/* type=b def of fixed_line i */
			me->Line = lastcell->cLine;	/* should've happened in break */
		    } else {
			if (me->Line != lastcell->cLine)
			    goto trace_and_return;
		    }
		} else {
		    if (!me->fixed_line) {
			me->fixed_line = YES;	/* type=b def of fixed_line ii */
			me->Line = lastcell->cLine;	/* should've happened in break */
		    }
		    s->state = CS__cbc;
		    goto trace_and_return;
		}
		newstate = empty ? CS__cb : CS__cbc;
		break;
	    case CS__ef:
		ret = 0;
		goto trace_and_return;
	    case CS__cf:
		ret = lastcell->len;	/* ##523_change_state? */
		goto trace_and_return;
	    case CS__cbc:
		if (!me->fixed_line) {
		    if (empty) {
			if (s->icell_core == -1)	/* ##528??: */
			    me->Line = lineno;
			/* lastcell->Line = lineno; */
		    } else {	/* !empty */
			if (s->icell_core == -1)
			    me->Line = -1;
		    }
		}
		s->pending_len = 0;
		newstate = empty ? CS_invalid : CS__cbc;
		break;
	    default:
		break;
	    }
	} else {		/* multiline cell, processing </TD>: */
	    s->x_td = -1;
	    switch (s->state) {
	    case CS_invalid:
		/* ##540_return_-1_for_invalid_if_len!: */
		if (!empty && lastcell->len > 0) {
		    newstate = CS__0cf;
		    s->state = newstate;
		    ret = -1;
		    goto trace_and_return;
		}
		/* ##541_set_len_0_Line_-1_sometimes: */
		lastcell->len = 0;
		lastcell->cLine = -1;
		/* fall thru ##546 really fall thru??: */
		newstate = empty ? CS_invalid : CS__cbc;
		break;
	    case CS__0new:
		newstate = empty ? CS__0ef : CS__0cf;
		break;
	    case CS__0eb:
		newstate = empty ? CS__0ef : CS__0cf;	/* ebc?? */
		s->state = newstate;
		if (me->fixed_line) {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : -1);
		} else {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : 0);
		}
		goto trace_and_return;
	    case CS__0cb:
		if (s->pending_len) {
		    if (empty)
			lastcell->len = s->pending_len;
		    else
			lastcell->len = 0;
		    s->pending_len = 0;
		}
		if (!me->fixed_line) {
		    if (empty) {
			if (s->icell_core == -1)
			    /* first cell before <BR></TD> => the core cell */
			    s->icell_core = me->ncells - 1;
			/* lastcell->cLine = lineno; */
		    } else {	/* !empty */
			if (s->icell_core == -1)
			    me->Line = -1;
		    }
		}
		if (s->pending_len && empty) {
		    lastcell->len = s->pending_len;
		    s->pending_len = 0;
		}		/* @@@ for empty do smth. about ->Line / ->icell_core !! */
		newstate = empty ? CS__0cf : CS__cbc;
		break;
	    case CS__0ef:
		newstate = CS__0ef;
		/* FALLTHRU */
	    case CS__0cf:
		break;
	    case CS__new:
		newstate = empty ? CS__ef : CS__cf;
		break;
	    case CS__eb:
		newstate = empty ? CS__ef : CS__ef;	/* ##579??? !!!!! */
		s->state = newstate;
		if (me->fixed_line) {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : -1);
		} else {
		    if (empty)
			ret = (lastcell->len <= 0 ? 0 : lastcell->len);
		    else
			ret = (lastcell->len <= 0 ? 0 : -1);
		}
		goto trace_and_return;
	    case CS__cb:
		if (s->pending_len && empty) {
		    lastcell->len = s->pending_len;
		    s->pending_len = 0;
		}
		ret = -1;
		if (empty) {
		    if (!me->fixed_line) {
			me->fixed_line = YES;	/* type=c def of fixed_line */
			me->Line = lastcell->cLine;	/* should've happened in break */
		    } else {
			if (me->Line != lastcell->cLine)
			    goto trace_and_return;
		    }
		} else {
		    goto trace_and_return;
		}
		newstate = empty ? CS__cf : CS__cbc;
		break;
	    case CS__ef:	/* ignored error */
	    case CS__cf:	/* ignored error */
		break;
	    case CS__ebc:	/* ##540_handle_ebc: */
		lastcell->len = 0;
		if (!me->fixed_line) {
		    if (!empty) {
			if (s->icell_core == -1)
			    lastcell->cLine = -1;
		    }
		}
		s->pending_len = 0;
		newstate = empty ? CS_invalid : CS__cbc;
		break;
	    case CS__cbc:	/* ##586 */
		lastcell->len = 0;	/* ##613 */
		ret = -1;
		if (me->fixed_line && me->Line == lastcell->cLine)
		    goto trace_and_return;
		if (!me->fixed_line) {
		    if (empty) {
			if (s->icell_core == -1)
			    me->Line = lineno;
		    }
		}
		s->pending_len = 0;	/* ##629 v */
		newstate = empty ? CS_invalid : CS__cbc;
		break;
	    }
	}
    } else {			/* (!multiline) */
	if ((end_td & TRST_ENDCELL_MASK) == TRST_ENDCELL_LINEBREAK) {
	    switch (s->state) {
	    case CS_invalid:
	    case CS__0new:
		s->pending_len = empty ? 0 : pos - lastcell->pos;
		newstate = empty ? CS__0eb : CS__0cb;
		s->state = newstate;
		ret = 0;	/* or 0 for xlen to s->pending_len?? */
		goto trace_and_return;
	    case CS__0eb:	/* cannot happen */
		newstate = CS__eb;
		break;
	    case CS__0cb:	/* cannot happen */
		newstate = CS__cb;
		break;
	    case CS__0ef:
	    case CS__0cf:
		break;
	    case CS__new:
		ret = -1;
		if (!empty && s->prev_state == CS__cbc)		/* ##609: */
		    goto trace_and_return;
		if (!empty) {
		    if (!me->fixed_line) {
			me->fixed_line = YES;	/* type=d def of fixed_line */
			me->Line = lineno;
		    } else {
			if (me->Line != lineno)
			    goto trace_and_return;
		    }
		}
		newstate = empty ? CS__eb : CS__cb;
		s->state = newstate;
		if (!me->fixed_line) {
		    s->pending_len = empty ? 0 : pos - lastcell->pos;
		    ret = 0;
		    goto trace_and_return;
		} else {
		    s->pending_len = 0;
		    lastcell->len = empty ? 0 : pos - lastcell->pos;
		    ret = lastcell->len;
		    goto trace_and_return;
		}
	    case CS__eb:	/* cannot happen */
		newstate = empty ? CS__eb : CS__ebc;
		break;
	    case CS__cb:	/* cannot happen */
		newstate = empty ? CS__cb : CS__cbc;
		break;
	    case CS__ef:
		ret = 0;
		goto trace_and_return;
	    case CS__cf:
		ret = lastcell->len;
		goto trace_and_return;
	    case CS__cbc:	/* ??? */
		break;
	    default:
		break;
	    }
	} else {		/* !multiline, processing </TD>: */
	    s->x_td = -1;
	    switch (s->state) {
	    case CS_invalid:	/* ##691_no_lastcell_len_for_invalid: */
		if (!(me->fixed_line && me->Line == lastcell->cLine))
		    lastcell->len = 0;
		/* FALLTHRU */
	    case CS__0new:
		newstate = empty ? CS__0ef : CS__0cf;
		break;		/* ##630 */
	    case CS__0eb:
		newstate = empty ? CS__0ef : CS__0ef;
		break;		/* ??? */
	    case CS__0cb:
		newstate = empty ? CS__0cf : CS__cbc;
		break;		/* ??? */
	    case CS__0ef:
		newstate = CS__0ef;
		break;		/* ??? */
	    case CS__0cf:
		break;		/* ??? */
	    case CS__new:
		ret = -1;
		if (!empty && s->prev_state == CS__cbc)
		    goto trace_and_return;
		if (!empty) {	/* ##642_set_fixed!: */
		    if (!me->fixed_line) {
			me->fixed_line = YES;	/* type=e def of fixed_line */
			me->Line = lineno;
		    } else {
			if (me->Line != lineno)
			    goto trace_and_return;
		    }
		}
		if (lastcell->len < 0)
		    lastcell->len = empty ? 0 : pos - lastcell->pos;
		newstate = empty ? CS__ef : CS__cf;
		s->state = newstate;
		ret = ((me->fixed_line && lineno != me->Line)
		       ? -1 : lastcell->len);
		goto trace_and_return;
	    case CS__eb:
		newstate = empty ? CS__ef : CS__cf;
		break;		/* ??? */
	    case CS__cb:
		newstate = empty ? CS__cf : CS__cf;
		break;		/* ??? */
	    case CS__ef:	/* ignored error */
	    case CS__cf:	/* ignored error */
	    default:
		break;
	    }
	    lastcell->len = pos - lastcell->pos;
	}			/* if (!end_td) ... else */
    }				/* if (multiline) ... else */

    s->state = newstate;
    ret = lastcell->len;
#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	if (ret == -1 && pos == 0)
	    ret = 0;		/* XXXX Hack to allow trailing <P> in multiline cells. */
    }
#endif

/*    lastcell->len = pos - lastcell->pos; */
  trace_and_return:
    CTRACE2(TRACE_TRST,
	    (tfp, " => prev_state=%s, state=%s, return=%d\n",
	     cellstate_s(s->prev_state), cellstate_s(s->state), ret));
    return ret;
}

/*
 * Reserve cells, each of given colspan, in (rowspan-1) rows after the current
 * row of rowspan>1.  If rowspan==0, use special 'row' rowspans2eog to keep
 * track of rowspans that are to remain in effect until the end of the row
 * group (until next THEAD/TFOOT/TBODY) or table.
 */
static int Stbl_reserveCellsInTable(STable_info *me, int icell,
				    int colspan,
				    int rowspan)
{
    STable_rowinfo *rows, *row;
    int growby;
    int i;

    if (me->nrows <= 0)
	return -1;		/* must already have at least one row */

    CTRACE2(TRACE_TRST,
	    (tfp,
	     "TRST:Stbl_reserveCellsInTable(icell=%d, colspan=%d, rowspan=%d)\n",
	     icell, colspan, rowspan));
    if (rowspan == 0) {
	if (!me->rowspans2eog.cells) {
	    me->rowspans2eog.cells = typecallocn(STable_cellinfo, icell + colspan);

	    if (!me->rowspans2eog.cells)
		return 0;	/* fail silently */
	    else
		me->rowspans2eog.allocated = icell + colspan;
	}
	Stbl_reserveCellsInRow(&me->rowspans2eog, icell, colspan);
    }

    growby = me->nrows + rowspan - 1 - me->allocated_rows;
    if (growby > 0) {
	rows = typeRealloc(STable_rowinfo, me->rows,
			     (me->allocated_rows + growby));

	if (!rows)
	    return 0;		/* ignore silently, no free memory, may be recoverable */
	for (i = 0; i < growby; i++) {
	    row = rows + me->allocated_rows + i;
	    row->allocated = 0;
	    row->offset = 0;
	    row->content = 0;
	    if (!me->rowspans2eog.allocated) {
		row->cells = NULL;
	    } else {
		row->cells = typecallocn(STable_cellinfo,
					 me->rowspans2eog.allocated);

		if (row->cells) {
		    row->allocated = me->rowspans2eog.allocated;
		    memcpy(row->cells, me->rowspans2eog.cells,
			   row->allocated * sizeof(STable_cellinfo));
		}
	    }
	    row->ncells = 0;
	    row->fixed_line = NO;
	    row->alignment = HT_ALIGN_NONE;
	}
	me->allocated_rows += growby;
	me->rows = rows;
    }
    for (i = me->nrows;
	 i < (rowspan == 0 ? me->allocated_rows : me->nrows + rowspan - 1);
	 i++) {
	if (!me->rows[i].allocated) {
	    me->rows[i].cells = typecallocn(STable_cellinfo, icell + colspan);

	    if (!me->rows[i].cells)
		return 0;	/* fail silently */
	    else
		me->rows[i].allocated = icell + colspan;
	}
	Stbl_reserveCellsInRow(me->rows + i, icell, colspan);
    }
    return 0;
}

/* Remove reserved cells in trailing rows that were added for rowspan,
 * to be used when a THEAD/TFOOT/TBODY ends. */
static void Stbl_cancelRowSpans(STable_info *me)
{
    int i;

    CTRACE2(TRACE_TRST, (tfp, "TRST:Stbl_cancelRowSpans()"));
    for (i = me->nrows; i < me->allocated_rows; i++) {
	if (!me->rows[i].ncells) {	/* should always be the case */
	    FREE(me->rows[i].cells);
	    me->rows[i].allocated = 0;
	}
    }
    free_rowinfo(&me->rowspans2eog);
    me->rowspans2eog.allocated = 0;
}

/*
 * Returns -1 on error, otherwise index of just-added table row.
 */
int Stbl_addRowToTable(STable_info *me, int alignment,
		       int lineno)
{
    STable_rowinfo *rows, *row;
    STable_states *s = &me->s;

    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_addRowToTable(alignment=%d, lineno=%d)\n",
	     alignment, lineno));
    if (me->nrows > 0 && me->rows[me->nrows - 1].ncells > 0) {
	if (s->pending_len > 0)
	    me->rows[me->nrows - 1].cells[
					     me->rows[me->nrows - 1].ncells - 1
		].len =
		s->pending_len;
	s->pending_len = 0;
    }
    Stbl_finishRowInTable(me);
    if (me->nrows > 0 && me->rows[me->nrows - 1].Line == lineno)
	me->rows[me->nrows - 1].Line = -1;
    s->pending_len = 0;
    s->x_td = -1;

    {
	int i;
	int growby = 0;

	while (me->nrows + 2 > me->allocated_rows + growby)
	    growby += ROWS_GROWBY;
	if (growby) {
	    if (me->allocated_rows == 0 && !me->rows) {
		rows = typecallocn(STable_rowinfo, growby);
	    } else {
		rows = typeRealloc(STable_rowinfo, me->rows,
				     (me->allocated_rows + growby));

		for (i = 0; rows && i < growby; i++) {
		    row = rows + me->allocated_rows + i;
		    if (!me->rowspans2eog.allocated) {
			row->allocated = 0;
			row->cells = NULL;
		    } else {
			row->cells = typecallocn(STable_cellinfo,
						 me->rowspans2eog.allocated);

			if (row->cells) {
			    row->allocated = me->rowspans2eog.allocated;
			    memcpy(row->cells, me->rowspans2eog.cells,
				   row->allocated * sizeof(STable_cellinfo));
			} else {
			    FREE(rows);
			    break;
			}
		    }
		    row->ncells = 0;
		    row->fixed_line = NO;
		    row->alignment = HT_ALIGN_NONE;
		    row->offset = 0;
		    row->content = 0;
		}
	    }
	    if (rows) {
		me->allocated_rows += growby;
		me->rows = rows;
	    } else {
		return -1;
	    }
	}
    }

    me->rows[me->nrows].Line = lineno;
    if (me->nrows == 0)
	me->startline = lineno;
    if (alignment != HT_ALIGN_NONE)
	me->rows[me->nrows].alignment = alignment;
    else
	me->rows[me->nrows].alignment =
	    (me->rowgroup_align == HT_ALIGN_NONE) ?
	    me->alignment : me->rowgroup_align;
    me->nrows++;
    if (me->pending_colgroup_next > me->ncolinfo) {
	me->ncolinfo = me->pending_colgroup_next;
	me->pending_colgroup_next = 0;
    }
    me->rows[me->nrows].Line = -1;	/* not yet used */
    me->rows[me->nrows].ended = ROW_not_ended;	/* No </tr> yet */
    return (me->nrows - 1);
}

/*
 * Returns -1 on error, otherwise current number of rows.
 */
static int Stbl_finishRowInTable(STable_info *me)
{
    STable_rowinfo *lastrow;
    STable_states *s = &me->s;
    int ncells;

    CTRACE2(TRACE_TRST, (tfp, "TRST:Stbl_finishRowInTable()\n"));
    if (!me->rows || !me->nrows)
	return -1;		/* no row started! */
    lastrow = me->rows + (me->nrows - 1);
    ncells = lastrow->ncells;
    lastrow->ended = ROW_ended_by_endtr;
    if (lastrow->ncells > 0) {
	if (s->pending_len > 0)
	    lastrow->cells[lastrow->ncells - 1].len = s->pending_len;
	s->pending_len = 0;
    }
    s->prev_state = s->state = CS_invalid;
    s->lineno = -1;

    if (s->icell_core >= 0 && !lastrow->fixed_line &&
	lastrow->cells[s->icell_core].cLine >= 0)
	lastrow->Line = lastrow->cells[s->icell_core].cLine;
    s->icell_core = -1;
    return (me->nrows);
}

static void update_sumcols0(STable_cellinfo *sumcols,
			    STable_rowinfo *lastrow,
			    int pos,
			    int len,
			    int icell,
			    int ispan,
			    int allocated_sumcols)
{
    int i;

    if (len > 0) {
	int sumpos = pos;
	int prevsumpos = sumcols[icell + ispan].pos;
	int advance;

	if (ispan > 0) {
	    if (lastrow->cells[icell].pos + len > sumpos)
		sumpos = lastrow->cells[icell].pos + len;
	    if (sumcols[icell + ispan - 1].pos +
		sumcols[icell + ispan - 1].len >
		sumpos)
		sumpos = sumcols[icell + ispan - 1].pos +
		    sumcols[icell + ispan - 1].len;
	}
	advance = sumpos - prevsumpos;
	if (advance > 0) {
	    for (i = icell + ispan; i < allocated_sumcols; i++) {
		if (ispan > 0 && sumcols[i].colspan < -1) {
		    if (i + sumcols[i].colspan < icell + ispan) {
			advance = sumpos - sumcols[i].pos;
			if (i > 0)
			    advance = HTMAX(advance,
					    sumcols[i - 1].pos +
					    sumcols[i - 1].len
					    - (sumcols[i].pos));
			if (advance <= 0)
			    break;
		    }
		}
		if (sumcols[i].pos >= 0)
		    sumcols[i].pos += advance;
		else {
		    sumcols[i].pos = sumpos;
		    break;
		}
	    }
	}
    }
}

static int get_remaining_colspan(STable_rowinfo *me,
				 STable_cellinfo *colinfo,
				 int ncolinfo,
				 int colspan,
				 int ncols_sofar)
{
    int i;
    int last_colspan = me->ncells ?
    me->cells[me->ncells - 1].colspan : 1;

    if (ncolinfo == 0 || me->ncells + last_colspan > ncolinfo) {
	colspan = HTMAX(TRST_MAXCOLSPAN,
			ncols_sofar - (me->ncells + last_colspan - 1));
    } else {
	for (i = me->ncells + last_colspan - 1; i < ncolinfo - 1; i++)
	    if (colinfo[i].cLine == EOCOLG)
		break;
	colspan = i - (me->ncells + last_colspan - 2);
    }
    return colspan;
}

#ifdef EXP_NESTED_TABLES
/* Returns -1 on failure, 1 if faking was performed, 0 if not needed. */
static int Stbl_fakeFinishCellInTable(STable_info *me,
				      STable_rowinfo *lastrow,
				      int lineno,
				      int finishing)	/* Processing finish or start */
{
    STable_states *s = &me->s;
    int fake = 0;

    switch (s->state) {		/* We care only about trailing <BR> */
    case CS_invalid:
    case CS__0new:
    case CS__0ef:
    case CS__0cf:
    case CS__new:
    case CS__cbc:
    case CS__ef:
    case CS__cf:
    default:
	/* <BR></TD> may produce these (XXXX instead of CS__cbf?).  But if
	   finishing==0, the caller already checked that we are on a
	   different line.  */
	if (finishing == 0)
	    fake = 1;
	break;			/* Either can't happen, or may be ignored */
    case CS__eb:
    case CS__0eb:
    case CS__0cb:
    case CS__cb:
	fake = 1;
	break;
    }
    if (fake) {
	/* The previous action we did was putting a linebreak.  Now we
	   want to put another one.  Fake necessary
	   </TD></TR><TR><TD></TD><TD> (and possibly </TD>) instead. */
	int ncells = lastrow->ncells;
	int i;
	int al = lastrow->alignment;
	int cs = lastrow->cells[lastrow->ncells - 1].colspan;
	int rs = 1;		/* XXXX How to find rowspan? */
	int ih = 0;		/* XXXX How to find is_header? */
	int end_td = (TRST_ENDCELL_ENDTD | TRST_FAKING_CELLS);
	int need_reserved = 0;
	int prev_reserved_last = -1;
	STable_rowinfo *prev_row;
	int prev_row_n2 = lastrow - me->rows;

	CTRACE2(TRACE_TRST,
		(tfp,
		 "TRST:Stbl_fakeFinishCellInTable(lineno=%d, finishing=%d) START FAKING\n",
		 lineno, finishing));

	/* Although here we use pos=0, this may commit the previous
	   cell which had <BR> as a last element.  This may overflow
	   the screen width, so the additional checks performed in
	   Stbl_finishCellInTable (comparing to Stbl_finishCellInRow)
	   are needed. */
	if (finishing) {
	    /* Fake </TD> at BOL */
	    if (Stbl_finishCellInTable(me, end_td, lineno, 0, 0) < 0) {
		return -1;
	    }
	}

	/* Fake </TR> at BOL */
/* Stbl_finishCellInTable(lineno, 0, 0); *//* Needed? */

	/* Fake <TR> at BOL */
	if (Stbl_addRowToTable(me, al, lineno) < 0) {
	    return -1;
	}
	lastrow = me->rows + (me->nrows - 1);
	lastrow->content = IS_CONTINUATION_OF_CELL;
	for (i = 0; i < lastrow->allocated; i++) {
	    if (lastrow->cells[i].alignment == RESERVEDCELL) {
		need_reserved = 1;
		break;
	    }
	}

	prev_row = me->rows + prev_row_n2;
	for (i = ncells; i < prev_row->allocated; i++) {
	    if (prev_row->cells[i].alignment == RESERVEDCELL)
		prev_reserved_last = i;
	}
	if (need_reserved || prev_reserved_last >= 0) {
	    /* Oups, we are going to stomp over a line which somebody
	       cares about already, or the previous line had reserved
	       cells which were not skipped over.

	       Remember that STable_rowinfo is about logical (TR)
	       table lines, not displayed lines.  We need to duplicate
	       the reservation structure when we fake new logical lines.  */
	    int prev_row_n = prev_row - me->rows;
	    STable_rowinfo *rows = typeRealloc(STable_rowinfo, me->rows,
					       (me->allocated_rows + 1));
	    int need_cells = prev_reserved_last + 1;
	    int n;

	    if (!rows)
		return -1;	/* ignore silently, no free memory, may be recoverable */

	    CTRACE2(TRACE_TRST,
		    (tfp, "TRST:Stbl_fakeFinishCellInTable REALLOC ROWSPAN\n"));
	    me->rows = rows;
	    lastrow = me->rows + (me->nrows - 1);
	    prev_row = me->rows + prev_row_n;
	    me->allocated_rows++;

	    /* Insert a duplicate row after lastrow */
	    for (n = me->allocated_rows - me->nrows - 1; n >= 0; --n)
		lastrow[n + 1] = lastrow[n];

	    /* Ignore cells, they belong to the next row now */
	    lastrow->allocated = 0;
	    lastrow->cells = 0;
	    if (need_cells) {
		lastrow->cells = typecallocn(STable_cellinfo, need_cells);

		/* ignore silently, no free memory, may be recoverable */
		if (!lastrow->cells) {
		    return -1;
		}
		lastrow->allocated = need_cells;
		memcpy(lastrow->cells, prev_row->cells,
		       lastrow->allocated * sizeof(STable_cellinfo));

		i = -1;
		while (++i < ncells) {
		    /* Stbl_addCellToTable grants RESERVEDCELL, but we do not
		       want this action for fake cells.
		       XXX Maybe always fake RESERVEDCELL instead of explicitly
		       creating/destroying cells?  */
		    if (lastrow->cells[i].alignment == RESERVEDCELL)
			lastrow->cells[i].alignment = HT_LEFT;
		}
	    }
	}

	/* Fake <TD></TD>...<TD> (and maybe a </TD>) at BOL. */
	CTRACE2(TRACE_TRST,
		(tfp, "TRST:Stbl_fakeFinishCellInTable FAKE %d elts%s\n",
		 ncells, (finishing ? ", last unfinished" : "")));
	i = 0;
	while (++i <= ncells) {
	    /* XXXX A lot of args may be wrong... */
	    if (Stbl_addCellToTable(me, (i == ncells ? cs : 1), rs, al,
				    ih, lineno, 0, 0) < 0) {
		return -1;
	    }
	    lastrow->content &= ~HAS_BEG_OF_CELL;	/* BEG_OF_CELL was fake */
	    /* We cannot run out of width here, so it is safe to not
	       call Stbl_finishCellInTable(), but Stbl_finishCellInRow. */
	    if (!finishing || (i != ncells)) {
		if (Stbl_finishCellInRow(lastrow, s, end_td, lineno, 0) < 0) {
		    return -1;
		}
	    }
	}
	CTRACE2(TRACE_TRST,
		(tfp,
		 "TRST:Stbl_fakeFinishCellInTable(lineno=%d) FINISH FAKING\n",
		 lineno));
	return 1;
    }
    return 0;
}
#endif

/*
 * Returns -1 on error, otherwise 0.
 */
int Stbl_addCellToTable(STable_info *me, int colspan,
			int rowspan,
			int alignment,
			int isheader,
			int lineno,
			int offset_not_used_yet GCC_UNUSED,
			int pos)
{
    STable_states *s = &me->s;
    STable_rowinfo *lastrow;
    STable_cellinfo *sumcols, *sumcol;
    int i, icell, ncells, sumpos;

    CTRACE2(TRACE_TRST,
	    (tfp,
	     "TRST:Stbl_addCellToTable(lineno=%d, pos=%d, isheader=%d, cs=%d, rs=%d, al=%d)\n",
	     lineno, pos, (int) isheader, colspan, rowspan, alignment));
    if (!me->rows || !me->nrows)
	return -1;		/* no row started! */
    /* ##850_fail_if_fail?? */
    if (me->rows[me->nrows - 1].ended != ROW_not_ended)
	Stbl_addRowToTable(me, alignment, lineno);
    Stbl_finishCellInTable(me, TRST_ENDCELL_ENDTD, lineno, 0, pos);
    lastrow = me->rows + (me->nrows - 1);

#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	/* If the last cell was finished by <BR></TD>, we need to fake an
	   appropriate amount of cells */
	if (!NO_AGGRESSIVE_NEWROW && pos == 0 && lastrow->ncells > 0
	    && lastrow->cells[lastrow->ncells - 1].cLine != lineno) {
	    int rc = Stbl_fakeFinishCellInTable(me, lastrow, lineno, 0);

	    if (rc < 0)
		return -1;
	    if (rc)
		lastrow = me->rows + (me->nrows - 1);
	}
    }
#endif
    if (colspan == 0) {
	colspan = get_remaining_colspan(lastrow, me->sumcols, me->ncolinfo,
					colspan, me->ncols);
    }
    ncells = lastrow->ncells;	/* remember what it was before adding cell. */
    icell = Stbl_addCellToRow(lastrow, me->sumcols, me->ncolinfo, s,
			      colspan, alignment, isheader,
			      lineno, &pos);
    if (icell < 0)
	return icell;
    if (me->nrows == 1 && me->startline < lastrow->Line)
	me->startline = lastrow->Line;

    if (rowspan != 1) {
	Stbl_reserveCellsInTable(me, icell, colspan, rowspan);
	/* me->rows may now have been realloc'd, make lastrow valid pointer */
	lastrow = me->rows + (me->nrows - 1);
    }
    lastrow->content |= HAS_BEG_OF_CELL;

    {
	int growby = 0;

	while (icell + colspan + 1 > me->allocated_sumcols + growby)
	    growby += CELLS_GROWBY;
	if (growby) {
	    if (me->allocated_sumcols == 0 && !me->sumcols) {
		sumcols = typecallocn(STable_cellinfo, growby);
	    } else {
		sumcols = typeRealloc(STable_cellinfo, me->sumcols,
				        (me->allocated_sumcols + growby));

		for (i = 0; sumcols && i < growby; i++) {
		    sumcol = sumcols + me->allocated_sumcols + i;
		    sumcol->pos = sumcols[me->allocated_sumcols - 1].pos;
		    sumcol->len = 0;
		    sumcol->colspan = 0;
		    sumcol->cLine = 0;
		    sumcol->alignment = HT_ALIGN_NONE;
		}
	    }
	    if (sumcols) {
		me->allocated_sumcols += growby;
		me->sumcols = sumcols;
	    } else {
		return -1;
	    }
	}
    }
    if (icell + 1 > me->ncols) {
	me->ncols = icell + 1;
    }
    if (colspan > 1 && colspan + me->sumcols[icell + colspan].colspan > 0)
	me->sumcols[icell + colspan].colspan = -colspan;
    sumpos = pos;
    if (ncells > 0)
	sumpos += me->sumcols[ncells - 1].pos - lastrow->cells[ncells - 1].pos;
    update_sumcols0(me->sumcols, lastrow, sumpos,
		    sumpos - ((ncells > 0)
			      ? me->sumcols[icell].pos
			      : me->sumcols[icell].pos),
		    icell, 0, me->allocated_sumcols);

    me->maxpos = me->sumcols[me->allocated_sumcols - 1].pos;
    if (me->maxpos > /* @@@ max. line length we can accept */ MAX_STBL_POS)
	return -1;
    return 0;
}

/*
 * Returns -1 on error, otherwise 0.
 */
int Stbl_finishCellInTable(STable_info *me, int end_td,
			   int lineno,
			   int offset,
			   int pos)
{
    STable_states *s = &me->s;
    STable_rowinfo *lastrow;
    int len, xlen, icell;
    int i;

    CTRACE2(TRACE_TRST,
	    (tfp,
	     "TRST:Stbl_finishCellInTable(lineno=%d, pos=%d, off=%d, end_td=%d)\n",
	     lineno, pos, offset, (int) end_td));
    if (me->nrows == 0)
	return -1;
    lastrow = me->rows + (me->nrows - 1);
    icell = lastrow->ncells - 1;
    if (icell < 0)
	return icell;
    if (s->x_td == -1) {	/* Stray </TD> or just-in-case, as on </TR> */
	if ((end_td & TRST_ENDCELL_MASK) == TRST_ENDCELL_LINEBREAK)
	    lastrow->ended = ROW_ended_by_splitline;
	return 0;
    }
#ifdef EXP_NESTED_TABLES
    if (nested_tables) {
	if (!NO_AGGRESSIVE_NEWROW && !(end_td & TRST_FAKING_CELLS)) {
	    int rc = Stbl_fakeFinishCellInTable(me, lastrow, lineno, 1);

	    if (rc) {
		if (rc < 0)
		    return -1;
		lastrow = me->rows + (me->nrows - 1);
		icell = lastrow->ncells - 1;
	    }
	}
    }
#endif
    len = Stbl_finishCellInRow(lastrow, s, end_td, lineno, pos);
    if (len == -1)
	return len;
    xlen = (len > 0) ? len : s->pending_len;	/* ##890 use xlen if fixed_line?: */
    if (lastrow->Line == lineno)
	len = xlen;
    if (lastrow->cells[icell].colspan > 1) {
	/*
	 * @@@ This is all a too-complicated mess; do we need
	 * sumcols len at all, or is pos enough??
	 * Answer: sumcols len is at least used for center/right
	 * alignment, and should probably continue to be used there;
	 * all other uses are probably not necessary.
	 */
	int spanlen = 0, spanlend = 0;

	for (i = icell; i < icell + lastrow->cells[icell].colspan; i++) {
	    if (me->sumcols[i].len > 0) {
		spanlen += me->sumcols[i].len;
		if (i > icell)
		    spanlen++;
	    }
	    spanlend = HTMAX(spanlend,
			     me->sumcols[i + 1].pos - me->sumcols[icell].pos);
	}
	if (spanlend)
	    spanlend--;
	if (spanlend > spanlen)
	    spanlen = spanlend;
	/* @@@ could overcount? */
	if (len > spanlen)
	    me->maxlen += (len - spanlen);
    } else if (len > me->sumcols[icell].len) {
	if (me->sumcols[icell + 1].colspan >= -1)
	    me->maxlen += (len - me->sumcols[icell].len);
	me->sumcols[icell].len = len;
    }

    if (len > 0) {
	update_sumcols0(me->sumcols, lastrow, pos, len,
			icell, lastrow->cells[icell].colspan,
			me->allocated_sumcols);
	me->maxpos = me->sumcols[me->allocated_sumcols - 1].pos;
    }

    if ((end_td & TRST_ENDCELL_MASK) == TRST_ENDCELL_LINEBREAK) {
	lastrow->ended = ROW_ended_by_splitline;
	lastrow->content |= BELIEVE_OFFSET;
	lastrow->offset = offset;
    }
#ifdef EXP_NESTED_TABLES	/* maxlen may already include contribution of a cell in this column */
    if (nested_tables) {
	if (me->maxlen > MAX_STBL_POS)
	    return -1;
    } else
#endif
    {
	if (me->maxlen + (xlen - len) > MAX_STBL_POS)
	    return -1;
    }
    if (me->maxpos > /* @@@ max. line length we can accept */ MAX_STBL_POS)
	return -1;

    if (lineno != lastrow->Line) {
	/* @@@ Do something here?  Or is it taken care of in
	   Stbl_finishCellInRow ? */
    }

    return 0;
}

/*
 * Returns -1 on error, otherwise 0.
 */
int Stbl_addColInfo(STable_info *me, int colspan,
		    short alignment,
		    BOOL isgroup)
{
    STable_cellinfo *sumcols, *sumcol;
    int i, icolinfo;

    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_addColInfo(cs=%d, al=%d, isgroup=%d)\n",
	     colspan, alignment, (int) isgroup));
    if (isgroup) {
	if (me->pending_colgroup_next > me->ncolinfo)
	    me->ncolinfo = me->pending_colgroup_next;
	me->pending_colgroup_next = me->ncolinfo + colspan;
	if (me->ncolinfo > 0)
	    me->sumcols[me->ncolinfo - 1].cLine = EOCOLG;
	me->pending_colgroup_align = alignment;
    } else {
	for (i = me->pending_colgroup_next - 1;
	     i >= me->ncolinfo + colspan; i--)
	    me->sumcols[i].alignment = HT_ALIGN_NONE;
	me->pending_colgroup_next = me->ncolinfo + colspan;
    }
    icolinfo = me->ncolinfo;
    if (!isgroup)
	me->ncolinfo += colspan;

    {
	int growby = 0;

	while (icolinfo + colspan + 1 > me->allocated_sumcols + growby)
	    growby += CELLS_GROWBY;
	if (growby) {
	    if (me->allocated_sumcols == 0) {
		sumcols = typecallocn(STable_cellinfo, growby);
	    } else {
		sumcols = typeRealloc(STable_cellinfo, me->sumcols,
				        (me->allocated_sumcols + growby));

		for (i = 0; sumcols && i < growby; i++) {
		    sumcol = sumcols + me->allocated_sumcols + i;
		    sumcol->pos = sumcols[me->allocated_sumcols - 1].pos;
		    sumcol->len = 0;
		    sumcol->colspan = 0;
		    sumcol->cLine = 0;
		}
	    }
	    if (sumcols) {
		me->allocated_sumcols += growby;
		me->sumcols = sumcols;
	    } else {
		return -1;
	    }
	}
    }

    if (alignment == HT_ALIGN_NONE)
	alignment = me->pending_colgroup_align;
    for (i = icolinfo; i < icolinfo + colspan; i++) {
	me->sumcols[i].alignment = alignment;
    }
    return 0;
}

/*
 * Returns -1 on error, otherwise 0.
 */
int Stbl_finishColGroup(STable_info *me)
{
    CTRACE2(TRACE_TRST, (tfp, "TRST:Stbl_finishColGroup()\n"));
    if (me->pending_colgroup_next >= me->ncolinfo) {
	me->ncolinfo = me->pending_colgroup_next;
	if (me->ncolinfo > 0)
	    me->sumcols[me->ncolinfo - 1].cLine = EOCOLG;
    }
    me->pending_colgroup_next = 0;
    me->pending_colgroup_align = HT_ALIGN_NONE;
    return 0;
}

int Stbl_addRowGroup(STable_info *me, short alignment)
{
    CTRACE2(TRACE_TRST, (tfp, "TRST:Stbl_addRowGroup()\n"));
    Stbl_cancelRowSpans(me);
    me->rowgroup_align = alignment;
    return 0;			/* that's all! */
}

int Stbl_finishTABLE(STable_info *me)
{
    STable_states *s = &me->s;
    int i;
    int curpos = 0;

    CTRACE2(TRACE_TRST, (tfp, "TRST:Stbl_finishTABLE()\n"));
    if (!me || me->nrows <= 0 || me->ncols <= 0) {
	return -1;
    }
    if (me->nrows > 0 && me->rows[me->nrows - 1].ncells > 0) {
	if (s->pending_len > 0)
	    me->rows[me->nrows - 1].cells[
					     me->rows[me->nrows - 1].ncells - 1
		].len = s->pending_len;
	s->pending_len = 0;
    }
    Stbl_finishRowInTable(me);
    /* take into account offsets on multi-line cells.
       XXX We cannot do it honestly, since two cells on the same row may
       participate in multi-line table entries, and we preserve only
       one offset per row.  This implementation may ignore
       horizontal offsets for the last row of a multirow table entry.  */
    for (i = 0; i < me->nrows - 1; i++) {
	int j = i + 1, leading = i, non_empty = 0;
	STable_rowinfo *nextrow = me->rows + j;
	int minoffset, have_offsets;
	int foundcell = -1, max_width;

	if ((nextrow->content & (IS_CONTINUATION_OF_CELL | HAS_BEG_OF_CELL | BELIEVE_OFFSET))
	    != (IS_CONTINUATION_OF_CELL | BELIEVE_OFFSET))
	    continue;		/* Not a continuation line */
	minoffset = nextrow[-1].offset;		/* Line before first continuation */
	CTRACE2(TRACE_TRST, (tfp,
			     "TRST:Stbl_finishTABLE, l=%d, offset=%d, ended=%u.\n",
			     i, nextrow[-1].offset, nextrow[-1].ended));

	/* Find the common part of the requested offsets */
	while (j < me->nrows
	       && ((nextrow->content &
		    (IS_CONTINUATION_OF_CELL
		     | HAS_BEG_OF_CELL
		     | BELIEVE_OFFSET))
		   == (IS_CONTINUATION_OF_CELL | BELIEVE_OFFSET))) {
	    if (minoffset > nextrow->offset)
		minoffset = nextrow->offset;
	    CTRACE2(TRACE_TRST,
		    (tfp,
		     "TRST:Stbl_finishTABLE, l=%d, offset=%d, ended=%u.\n",
		     j, nextrow->offset, nextrow[-1].ended));
	    nextrow++;
	    j++;
	}
	i = j - 1;		/* Continue after this line */
	/* Cancel the common part of offsets */
	j = leading;		/* Restart */
	nextrow = me->rows + j;	/* Line before first continuation */
	have_offsets = 0;
	nextrow->content |= OFFSET_IS_VALID_LAST_CELL;
	while (j <= i) {	/* A continuation line */
	    nextrow->offset -= minoffset;
	    nextrow->content |= OFFSET_IS_VALID;
	    if (nextrow->offset)
		have_offsets = 1;
	    nextrow++;
	    j++;
	}
	if (!have_offsets)
	    continue;		/* No offsets to deal with */

	/* Find the cell number */
	foundcell = -1;
	j = leading + 1;	/* Restart */
	nextrow = me->rows + j;	/* First continuation line */
	while (foundcell == -1 && j <= i) {	/* A continuation line */
	    int curcell = -1;

	    while (foundcell == -1 && ++curcell < nextrow->ncells)
		if (nextrow->cells[curcell].len)
		    foundcell = curcell, non_empty = j;
	    nextrow++;
	    j++;
	}
	if (foundcell == -1)	/* Can it happen? */
	    continue;
	/* Find the max width */
	max_width = 0;
	j = leading;		/* Restart */
	nextrow = me->rows + j;	/* Include the pre-continuation line */
	while (j <= i) {	/* A continuation line */
	    if (nextrow->ncells > foundcell) {
		int curwid = nextrow->cells[foundcell].len + nextrow->offset;

		if (curwid > max_width)
		    max_width = curwid;
	    }
	    nextrow++;
	    j++;
	}
	/* Update the widths */
	j = non_empty;		/* Restart from the first nonempty */
	nextrow = me->rows + j;
	/* Register the increase of the width */
	update_sumcols0(me->sumcols, me->rows + non_empty,
			0 /* width only */ , max_width,
			foundcell, nextrow->cells[foundcell].colspan,
			me->allocated_sumcols);
	j = leading;		/* Restart from pre-continuation */
	nextrow = me->rows + j;
	while (j <= i) {	/* A continuation line */
	    if (nextrow->ncells > foundcell)
		nextrow->cells[foundcell].len = max_width;
	    nextrow++;
	    j++;
	}
    }				/* END of Offsets processing */

    for (i = 0; i < me->ncols; i++) {
	if (me->sumcols[i].pos < curpos) {
	    me->sumcols[i].pos = curpos;
	} else {
	    curpos = me->sumcols[i].pos;
	}
	if (me->sumcols[i].len > 0) {
	    curpos += me->sumcols[i].len;
	}
    }
    /* need to recheck curpos: though it is checked each time a cell
       is added, sometimes the result is ignored, as in split_line(). */
    return (curpos > MAX_STBL_POS ? -1 : me->ncols);
}

short Stbl_getAlignment(STable_info *me)
{
    return (short) (me ? me->alignment : HT_ALIGN_NONE);
}

static int get_fixup_positions(STable_rowinfo *me, int *oldpos,
			       int *newpos,
			       STable_cellinfo *sumcols)
{
    int i = 0, ip = 0;
    int next_i, newlen;
    int ninserts;

    if (!me)
	return -1;
    while (i < me->ncells) {
	int offset;

	next_i = i + HTMAX(1, me->cells[i].colspan);
	if (me->cells[i].cLine != me->Line) {
	    if (me->cells[i].cLine > me->Line)
		break;
	    i = next_i;
	    continue;
	}
	oldpos[ip] = me->cells[i].pos;
	if ((me->content & OFFSET_IS_VALID)
	    && (i == me->ncells - 1
		|| !((me->content & OFFSET_IS_VALID_LAST_CELL))))
	    offset = me->offset;
	else
	    offset = 0;
	newpos[ip] = sumcols[i].pos + offset;
	if ((me->cells[i].alignment == HT_CENTER ||
	     me->cells[i].alignment == HT_RIGHT) &&
	    me->cells[i].len > 0) {
	    newlen = sumcols[next_i].pos - newpos[ip] - 1;
	    newlen = HTMAX(newlen, sumcols[i].len);
	    if (me->cells[i].len < newlen) {
		if (me->cells[i].alignment == HT_RIGHT) {
		    newpos[ip] += newlen - me->cells[i].len;
		} else {
		    newpos[ip] += (newlen - me->cells[i].len) / 2;
		}
	    }
	}
	ip++;
	i = next_i;
    }
    ninserts = ip;
    return ninserts;
}

/*
 * Returns -1 if we have no row for this lineno, or for other error,
 *	    0 or greater (number of oldpos/newpos pairs) if we have
 *	      a table row.
 */
int Stbl_getFixupPositions(STable_info *me, int lineno,
			   int *oldpos,
			   int *newpos)
{
    STable_rowinfo *row;
    int j;
    int ninserts = -1;

    if (!me || !me->nrows)
	return -1;
    for (j = 0; j < me->nrows; j++) {
	row = me->rows + j;
	if (row->Line == lineno) {
	    ninserts = get_fixup_positions(row, oldpos, newpos,
					   me->sumcols);
	    break;
	}
    }
    return ninserts;
}

int Stbl_getStartLine(STable_info *me)
{
    if (!me)
	return -1;
    else
	return me->startline;
}

#ifdef EXP_NESTED_TABLES

int Stbl_getStartLineDeep(STable_info *me)
{
    if (!me)
	return -1;
    while (me->enclosing)
	me = me->enclosing;
    return me->startline;
}

void Stbl_update_enclosing(STable_info *me, int max_width,
			   int last_lineno)
{
    int l;

    if (!me || !me->enclosing || !max_width)
	return;
    CTRACE2(TRACE_TRST,
	    (tfp, "TRST:Stbl_update_enclosing, width=%d, lines=%d...%d.\n",
	     max_width, me->startline, last_lineno));
    for (l = me->startline; l <= last_lineno; l++) {
	/* Fake <BR> in appropriate positions */
	if (Stbl_finishCellInTable(me->enclosing,
				   TRST_ENDCELL_LINEBREAK,
				   l,
				   0,
				   max_width) < 0) {
	    /* It is not handy to let the caller delete me->enclosing,
	       and it does not buy us anything.  Do it directly. */
	    STable_info *stbl = me->enclosing;

	    CTRACE2(TRACE_TRST, (tfp,
				 "TRST:Stbl_update_enclosing: width too large, aborting enclosing\n"));
	    me->enclosing = 0;
	    while (stbl) {
		STable_info *enclosing = stbl->enclosing;

		Stbl_free(stbl);
		stbl = enclosing;
	    }
	    break;
	}
    }
    return;
}

void Stbl_set_enclosing(STable_info *me, STable_info *enclosing, struct _TextAnchor *enclosing_last_anchor_before_stbl)
{
    if (!me)
	return;
    me->enclosing = enclosing;
    me->enclosing_last_anchor_before_stbl = enclosing_last_anchor_before_stbl;
}

STable_info *Stbl_get_enclosing(STable_info *me)
{
    if (!me)
	return 0;
    return me->enclosing;
}

struct _TextAnchor *Stbl_get_last_anchor_before(STable_info *me)
{
    if (!me)
	return 0;
    return me->enclosing_last_anchor_before_stbl;
}
#endif
