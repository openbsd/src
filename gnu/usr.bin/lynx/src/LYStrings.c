/* $LynxId: LYStrings.c,v 1.258 2013/11/28 11:57:39 tom Exp $ */
#include <HTUtils.h>
#include <HTCJK.h>
#include <UCAux.h>
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <GridText.h>
#include <LYKeymap.h>
#include <LYClean.h>
#include <LYMail.h>
#include <LYNews.h>
#include <LYOptions.h>
#include <LYCharSets.h>
#include <HTAlert.h>
#include <HTString.h>
#include <LYCharUtils.h>
#include <HTList.h>
#include <HTParse.h>
#ifdef USE_MOUSE
#include <LYMainLoop.h>
#endif

#ifdef DJGPP_KEYHANDLER
#include <pc.h>
#include <keys.h>
#endif /* DJGPP_KEYHANDLER */

#ifdef USE_COLOR_STYLE
#include <LYHash.h>
#include <AttrList.h>
#endif

#ifdef USE_SCROLLBAR
#include <LYMainLoop.h>
#endif

#ifdef USE_CMD_LOGGING
#include <LYReadCFG.h>
#include <LYrcFile.h>
#endif

#include <LYShowInfo.h>
#include <LYLeaks.h>

#if defined(WIN_EX)
#undef  BUTTON_CTRL
#define BUTTON_CTRL	0	/* Quick hack */
#endif

#ifdef DEBUG_EDIT
#define CTRACE_EDIT(p) CTRACE(p)
#else
#define CTRACE_EDIT(p)		/*nothing */
#endif

#ifdef SUPPORT_MULTIBYTE_EDIT
#define IsWordChar(c) (isalnum(UCH(c)) || is8bits(c))
#else
#define IsWordChar(c) isalnum(UCH(c))
#endif

/*
 * The edit_history lists allow the user to press tab when entering URL to get
 * the closest match in the closet
 */
#define LYClosetSize 100

static HTList *URL_edit_history;
static HTList *MAIL_edit_history;

/* If you want to add mouse support for some new platform, it's fairly
 * simple to do.  Once you've determined the X and Y coordinates of
 * the mouse event, loop through the elements in the links[] array and
 * see if the coordinates fall within a highlighted link area.	If so,
 * the code must set mouse_link to the index of the chosen link,
 * and return a key value that corresponds to LYK_ACTIVATE.  The
 * LYK_ACTIVATE code in LYMainLoop.c will then check mouse_link
 * and activate that link.  If the mouse event didn't fall within a
 * link, the code should just set mouse_link to -1 and return -1. --AMK
 */

/* The number of the link selected w/ the mouse (-1 if none) */
static int mouse_link = -1;

static int have_levent;

#if defined(USE_MOUSE) && defined(NCURSES)
static MEVENT levent;
#endif

/* Return the value of mouse_link */
int peek_mouse_levent(void)
{
#if defined(USE_MOUSE) && defined(NCURSES)
    if (have_levent > 0) {
	ungetmouse(&levent);
	have_levent--;
	return 1;
    }
#endif
    return 0;
}

/* Return the value of mouse_link, erasing it */
int get_mouse_link(void)
{
    int t;

    t = mouse_link;
    mouse_link = -1;
    if (t < 0)
	t = -1;			/* Backward compatibility. */
    return t;
}

/* Return the value of mouse_link */
int peek_mouse_link(void)
{
    return mouse_link;
}

int fancy_mouse(WINDOW * win, int row,
		int *position)
{
    int cmd = LYK_DO_NOTHING;

#ifdef USE_MOUSE
/*********************************************************************/

#if defined(WIN_EX) && defined(PDCURSES)

    request_mouse_pos();

    if (BUTTON_STATUS(1)
	&& (MOUSE_X_POS >= getbegx(win) &&
	    MOUSE_X_POS < (getbegx(win) + getmaxx(win)))) {
	int mypos = MOUSE_Y_POS - getbegy(win);
	int delta = mypos - row;

	if (mypos + 1 == getmaxy(win)) {
	    /* At the decorative border: scroll forward */
	    if (BUTTON_STATUS(1) & BUTTON1_TRIPLE_CLICKED)
		cmd = LYK_END;
	    else if (BUTTON_STATUS(1) & BUTTON1_DOUBLE_CLICKED)
		cmd = LYK_NEXT_PAGE;
	    else
		cmd = LYK_NEXT_LINK;
	} else if (mypos >= getmaxy(win)) {
	    if (BUTTON_STATUS(1) & (BUTTON1_DOUBLE_CLICKED | BUTTON1_TRIPLE_CLICKED))
		cmd = LYK_END;
	    else
		cmd = LYK_NEXT_PAGE;
	} else if (mypos == 0) {
	    /* At the decorative border: scroll back */
	    if (BUTTON_STATUS(1) & BUTTON1_TRIPLE_CLICKED)
		cmd = LYK_HOME;
	    else if (BUTTON_STATUS(1) & BUTTON1_DOUBLE_CLICKED)
		cmd = LYK_PREV_PAGE;
	    else
		cmd = LYK_PREV_LINK;
	} else if (mypos < 0) {
	    if (BUTTON_STATUS(1) & (BUTTON1_DOUBLE_CLICKED | BUTTON1_TRIPLE_CLICKED))
		cmd = LYK_HOME;
	    else
		cmd = LYK_PREV_PAGE;
#ifdef KNOW_HOW_TO_TOGGLE
	} else if (BUTTON_STATUS(1) & (BUTTON_CTRL)) {
	    cur_selection += delta;
	    cmd = LYX_TOGGLE;
#endif
	} else if (BUTTON_STATUS(1) & (BUTTON_ALT | BUTTON_SHIFT | BUTTON_CTRL)) {
	    /* Probably some unrelated activity, such as selecting some text.
	     * Select, but do nothing else.
	     */
	    *position += delta;
	    cmd = -1;
	} else {
	    /* No scrolling or overflow checks necessary. */
	    *position += delta;
	    cmd = LYK_ACTIVATE;
	}
    } else if (BUTTON_STATUS(1) & (BUTTON3_CLICKED | BUTTON3_DOUBLE_CLICKED | BUTTON3_TRIPLE_CLICKED)) {
	cmd = LYK_QUIT;
    }
#else
#if defined(NCURSES)
#define ButtonModifiers (BUTTON_ALT | BUTTON_SHIFT | BUTTON_CTRL)
    MEVENT event;

    getmouse(&event);
    if ((event.bstate & (BUTTON1_CLICKED |
			 BUTTON1_DOUBLE_CLICKED |
			 BUTTON1_TRIPLE_CLICKED))) {
	int mypos = event.y - getbegy(win);
	int delta = mypos - row;

	if ((event.x < getbegx(win) ||
	     event.x >= (getbegx(win) + getmaxx(win)))
	    && !(event.bstate & ButtonModifiers))
	    return LYK_QUIT;	/* User clicked outside, wants to quit? */
	if (mypos + 1 == getmaxy(win)) {
	    /* At the decorative border: scroll forward */
	    if (event.bstate & BUTTON1_TRIPLE_CLICKED)
		cmd = LYK_END;
	    else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
		cmd = LYK_NEXT_PAGE;
	    else
		cmd = LYK_NEXT_LINK;
	} else if (mypos >= getmaxy(win)) {
	    if (event.bstate & (BUTTON1_DOUBLE_CLICKED |
				BUTTON1_TRIPLE_CLICKED))
		cmd = LYK_END;
	    else
		cmd = LYK_NEXT_PAGE;
	} else if (mypos == 0) {
	    /* At the decorative border: scroll back */
	    if (event.bstate & BUTTON1_TRIPLE_CLICKED)
		cmd = LYK_HOME;
	    else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
		cmd = LYK_PREV_PAGE;
	    else
		cmd = LYK_PREV_LINK;
	} else if (mypos < 0) {
	    if (event.bstate & (BUTTON1_DOUBLE_CLICKED |
				BUTTON1_TRIPLE_CLICKED))
		cmd = LYK_HOME;
	    else
		cmd = LYK_PREV_PAGE;
#ifdef KNOW_HOW_TO_TOGGLE
	} else if (event.bstate & (BUTTON_CTRL)) {
	    cur_selection += delta;
	    cmd = LYX_TOGGLE;
#endif
	} else if (event.x <= getbegx(win) + 1 ||
		   event.x >= getbegx(win) + getmaxx(win) - 2) {
	    /* Click on left or right border for positioning without
	     * immediate action: select, but do nothing else.
	     * Actually, allow an error of one position inwards. - kw
	     */
	    *position += delta;
	    cmd = -1;
	} else if (event.bstate & ButtonModifiers) {
	    /* Probably some unrelated activity, such as selecting some text.
	     * Select, but do nothing else.
	     */
	    /* Possibly this is never returned by ncurses, so this case
	     * may be useless depending on situation (kind of mouse support
	     * and library versions). - kw
	     */
	    *position += delta;
	    cmd = -1;
	} else {
	    /* No scrolling or overflow checks necessary. */
	    *position += delta;
	    cmd = LYK_ACTIVATE;
	}
    } else if (event.bstate & (BUTTON3_CLICKED | BUTTON3_DOUBLE_CLICKED | BUTTON3_TRIPLE_CLICKED)) {
	cmd = LYK_QUIT;
    }
#endif /* NCURSES */
#endif /* PDCURSES */

/************************************************************************/
#endif /* USE_MOUSE */
    (void) win;
    (void) row;
    (void) position;

    return cmd;
}

/*
 * Manage the collection of edit-histories
 */
static HTList *whichRecall(RecallType recall)
{
    HTList **list;

    switch (recall) {
    case RECALL_CMD:
	return LYcommandList();
    case RECALL_MAIL:
	list = &MAIL_edit_history;
	break;
    default:
	list = &URL_edit_history;
	break;
    }
    if (*list == 0)
	*list = HTList_new();
    return *list;
}

/*
 * Remove the oldest item in the closet
 */
static void LYRemoveFromCloset(HTList *list)
{
    void *data = HTList_removeFirstObject(list);

    if (data != 0)
	FREE(data);
}

void LYCloseCloset(RecallType recall)
{
    HTList *list = whichRecall(recall);

    while (!HTList_isEmpty(list)) {
	LYRemoveFromCloset(list);
    }
    HTList_delete(list);	/* should already be empty */
}

/*
 * Strategy:  We begin at the top and search downwards.  We return the first
 * match, i.e., the newest since we search from the top.  This should be made
 * more intelligent, but works for now.
 */
static char *LYFindInCloset(RecallType recall, char *base)
{
    HTList *list = whichRecall(recall);
    char *data;
    size_t len = strlen(base);

    while (!HTList_isEmpty(list)) {
	data = (char *) HTList_nextObject(list);
	if (data != NULL && !StrNCmp(base, data, len))
	    return (data);
    }

    return (0);
}

static void LYAddToCloset(RecallType recall, char *str)
{
    HTList *list = whichRecall(recall);
    char *data = NULL;

    StrAllocCopy(data, str);
    HTList_addObject(list, data);
    while (HTList_count(list) > LYClosetSize)
	LYRemoveFromCloset(list);
}

#ifdef USE_MOUSE
static int XYdist(int x1,
		  int y1,
		  int x2,
		  int y2,
		  int dx2)
{
    int xerr = 3 * (x2 - x1), yerr = 9 * (y2 - y1);

    if (xerr < 0)
	xerr = 3 * (x1 - x2 - dx2) + 1;		/* pos after string not really in it */
    if (xerr < 0)
	xerr = 0;
    if (yerr < 0)
	yerr = -yerr;
    if (!yerr)			/* same line is good */
	return (xerr > 0) ? (xerr * 2 - 1) : 0;
    if (xerr < 9 && yerr)	/* x-dist of 3 cell better than y-dist of 1 cell */
	yerr += (9 - xerr);
    return 2 * xerr + yerr;	/* Subjective factor; ratio -> approx. 6 / 9 */
/*
old: (IZ 1999-07-30)
 3  2  2  2  1  1  1 XX XX XX XX XX  0  1  1  1  2  2  2  3  3
 4\ 3  3  3  2  2  2  2  2  2  2  2  2  2  2  2  3  3  3/ 4  4
 5  4  4  4\ 3  3  3  3  3  3  3  3  3  3  3  3/ 4  4  4  5  5
 6  5  5  5  4  4  4  4  4  4  4  4  4  4  4  4  5  5  5  6  5
now: (kw 1999-10-23)
41 35 29|23 17 11  5 XX XX XX XX XX  1  7 13 19 25|31 37 43 49
   45 39 33\27 24 21 18 18 18 18 18 19 22 25 28/34 40 46 50
      48 42 36 33 30\27 27 27 27 27 28/31 34 37 43 49
         51 45 42 39 36 36 36 36 36 37 40 43 46 49
               51 48 45 45 45 45 45 46 49 52
*/
}

/* Given X and Y coordinates of a mouse event, set mouse_link to the
 * index of the corresponding hyperlink, or set mouse_link to -1 if no
 * link matches the event.  Returns -1 if no link matched the click,
 * or a keycode that must be returned from LYgetch() to activate the
 * link.
 */

static int set_clicked_link(int x,
			    int y,
			    int code,
			    int clicks)
{
    int left = 6;
    int right = LYcolLimit - 5;

    /* yes, I am assuming that my screen will be a certain width. */
    int i;
    int c = -1;

    if (y == (LYlines - 1) || y == 0) {		/* First or last row */
	/* XXXX In fact # is not always at x==0?  KANJI_CODE_OVERRIDE? */
	int toolbar = (y == 0 && HText_hasToolbar(HTMainText));

	mouse_link = -2;
	if (x == 0 && toolbar)	/* On '#' */
	    c = LAC_TO_LKC0(LYK_TOOLBAR);
#if defined(CAN_CUT_AND_PASTE) && defined(USE_COLOR_STYLE)
	else if (y == 0 && x == LYcolLimit && s_hot_paste != NOSTYLE)
	    c = LAC_TO_LKC0(LYK_PASTE_URL);
#endif
	else if (clicks > 1) {
	    if (x < left + toolbar)
		c = (code == FOR_PROMPT && y)
		    ? HOME_KEY : LAC_TO_LKC0(LYK_MAIN_MENU);
	    else if (x > right)
		c = (code == FOR_PROMPT && y)
		    ? END_KEY : LAC_TO_LKC0(LYK_VLINKS);
	    else if (y)		/* Last row */
		c = LAC_TO_LKC0(LYK_END);
	    else		/* First row */
		c = LAC_TO_LKC0(LYK_HOME);
	} else {
	    if (x < left + toolbar)
		c = (code == FOR_PROMPT && y)
		    ? LTARROW_KEY
		    : (
#ifdef USE_COLOR_STYLE
			  (s_forw_backw != NOSTYLE && x - toolbar >= 3)
			  ? LAC_TO_LKC0(LYK_NEXT_DOC)
			  : LAC_TO_LKC0(LYK_PREV_DOC)
#else
			  LAC_TO_LKC0(LYK_NEXT_DOC)
#endif
		    );
	    else if (x > right)
		c = (code == FOR_PROMPT && y)
		    ? RTARROW_KEY : LAC_TO_LKC0(LYK_HISTORY);
	    else if (y)		/* Last row */
		c = LAC_TO_LKC0(LYK_NEXT_PAGE);
	    else		/* First row */
		c = LAC_TO_LKC0(LYK_PREV_PAGE);
	}
#ifdef USE_SCROLLBAR
    } else if (x == (LYcols - 1) && LYShowScrollbar && LYsb_begin >= 0) {
	int h = display_lines - 2 * (LYsb_arrow != 0);

	mouse_link = -2;
	y -= 1 + (LYsb_arrow != 0);
	if (y < 0)
	    return LAC_TO_LKC0(LYK_UP_TWO);
	if (y >= h)
	    return LAC_TO_LKC0(LYK_DOWN_TWO);

	if (clicks >= 2) {
	    double frac = (1. * y) / (h - 1);
	    int l = HText_getNumOfLines() + 1;	/* NOL() off by one? */

	    l -= display_lines;
	    if (l > 0)
		LYSetNewline((int) (frac * l + 1 + 0.5));
	    return LYReverseKeymap(LYK_DO_NOTHING);
	}

	if (y < LYsb_begin)
	    return LAC_TO_LKC0(LYK_PREV_PAGE);
	if (y >= LYsb_end)
	    return LAC_TO_LKC0(LYK_NEXT_PAGE);
	mouse_link = -1;	/* No action in edit fields */
#endif
    } else {
	int mouse_err = 29, /* subjctv-dist better than this for approx stuff */ cur_err;

	/* Loop over the links and see if we can get a match */
	for (i = 0; i < nlinks; i++) {
	    int len, lx = links[i].lx, is_text = 0;
	    int count = 0;
	    const char *text = LYGetHiliteStr(i, count);

	    if (links[i].type == WWW_FORM_LINK_TYPE
		&& F_TEXTLIKE(links[i].l_form->type))
		is_text = 1;

	    /* Check the first line of the link */
	    if (text != NULL) {
		if (is_text)
		    len = links[i].l_form->size;
		else
		    len = (int) LYstrCells(text);
		cur_err = XYdist(x, y, links[i].lx, links[i].ly, len);
		/* Check the second line */
		while (cur_err > 0
		       && (text = LYGetHiliteStr(i, ++count)) != NULL) {
		    /* Note that there is at most one hightext if is_text */
		    int cur_err_2 = XYdist(x, y,
					   LYGetHilitePos(i, count),
					   links[i].ly + count,
					   (int) LYstrCells(text));

		    cur_err = HTMIN(cur_err, cur_err_2);
		}
		if (cur_err > 0 && is_text)
		    cur_err--;	/* a bit of preference for text fields,
				   enter field if hit exactly at end - kw */
		if (cur_err == 0) {
		    int cury, curx;

		    LYGetYX(cury, curx);
		    /* double-click, if we care:
		       submit text submit fields. - kw */
		    if (clicks > 1 && is_text &&
			links[i].l_form->type == F_TEXT_SUBMIT_TYPE) {
			if (code != FOR_INPUT
			/* submit current input field directly */
			    || !(cury == y &&
				 (curx >= lx) &&
				 ((curx - lx) <= len))) {
			    c = LAC_TO_LKC0(LYK_MOUSE_SUBMIT);
			    mouse_link = i;
			} else {
			    c = LAC_TO_LKC0(LYK_MOUSE_SUBMIT);
			    mouse_link = -1;
			}
			mouse_err = 0;
			break;
		    }
		    if (code != FOR_INPUT
		    /* Do not pick up the current input field */
			|| !((cury == y && (curx >= lx) && ((curx - lx) <= len)))) {
			if (is_text) {
			    have_levent = 1;
#if defined(TEXTFIELDS_MAY_NEED_ACTIVATION) && defined(INACTIVE_INPUT_STYLE_VH)
			    if (x == links[i].lx && y == links[i].ly)
				textinput_redrawn = FALSE;
#endif /* TEXTFIELDS_MAY_NEED_ACTIVATION && INACTIVE_INPUT_STYLE_VH */
			}
			mouse_link = i;
		    } else
			mouse_link = -1;
		    mouse_err = 0;
		    break;
		} else if (cur_err < mouse_err) {
		    mouse_err = cur_err;
		    mouse_link = i;
		}
	    }
	}
	/*
	 * If a link was hit, we must look for a key which will activate
	 * LYK_ACTIVATE We expect to find LYK_ACTIVATE (it's usually mapped to
	 * the Enter key).
	 */
	if (mouse_link >= 0) {
	    if (mouse_err == 0) {
		if (c == -1)
		    c = LAC_TO_LKC0(LYK_ACTIVATE);
	    } else if (mouse_err >= 0)
		c = LAC_TO_LKC0(LYK_CHANGE_LINK);
	} else {
	    if (2 * y > LYlines) {	/* Bottom Half of the screen */
		if (4 * y < 3 * LYlines) {
		    c = LAC_TO_LKC0(LYK_DOWN_TWO);	/* Third quarter */
		} else
		    c = LAC_TO_LKC0(LYK_DOWN_HALF);	/* Fourth quarter */
	    } else {		/* Upper Half of the screen */
		if (4 * y < LYlines) {
		    c = LAC_TO_LKC0(LYK_UP_HALF);	/* First quarter */
		} else
		    c = LAC_TO_LKC0(LYK_UP_TWO);	/* Second quarter */
	    }
	}
    }
    return c;
}
#endif /* USE_MOUSE */

/*
 * LYstrncpy() ensures that the copied strings end with a nul byte.
 * The nul is written to the n+1 position of the target.
 */
char *LYstrncpy(char *target,
		const char *source,
		int n)
{
    char *val = target;
    int len;

    if (source == 0)
	source = "";
    len = (int) strlen(source);

    if (n > 0) {
	if (n > len)
	    n = len;
	(void) StrNCpy(target, source, n);
    } else {
	n = 0;
    }
    target[n] = '\0';
    return val;
}

#define IS_NEW_GLYPH(ch) (utf_flag && (UCH(ch)&0xc0) != 0x80)
#define IS_UTF_EXTRA(ch) (utf_flag && (UCH(ch)&0xc0) == 0x80)

/*
 * LYmbcsstrncpy() terminates strings with a null byte.  It takes account of
 * multibyte characters.  The source string is copied until either end of string
 * or max number of either bytes or glyphs (mbcs sequences) (CJK or UTF8).  The
 * utf_flag argument should be TRUE for UTF8.  - KW & FM
 */
char *LYmbcsstrncpy(char *target,
		    const char *source,
		    int n_bytes,
		    int n_glyphs,
		    int utf_flag)
{
    char *val = target;
    int i_bytes = 0, i_glyphs = 0;

    if (n_bytes < 0)
	n_bytes = 0;
    if (n_glyphs < 0)
	n_glyphs = 0;

    for (; *source != '\0' && i_bytes < n_bytes; i_bytes++) {
	if (IS_NEW_GLYPH(*source)) {
	    if (i_glyphs++ >= n_glyphs) {
		*target = '\0';
		return val;
	    }
	}
	*(target++) = *(source++);
    }
    *target = '\0';

    return val;
}

/*
 * LYmbcs_skip_glyphs() skips a given number of character positions in a string
 * and returns the resulting pointer.  It takes account of UTF-8 encoded
 * characters.  - KW
 */
const char *LYmbcs_skip_glyphs(const char *data,
			       int n_glyphs,
			       int utf_flag)
{
    int i_glyphs = 0;

    if (n_glyphs < 0)
	n_glyphs = 0;

    if (non_empty(data)) {
	if (!utf_flag) {
	    while (n_glyphs-- > 0) {
		if (!*++data)
		    break;
	    }
	} else {
	    while (*data) {
		if (IS_NEW_GLYPH(*data)) {
		    if (i_glyphs++ >= n_glyphs) {
			break;
		    }
		}
		data++;
	    }
	}
    }
    return data;
}

/*
 * LYmbcs_skip_cells() skips a given number of display positions in a string
 * and returns the resulting pointer.  It takes account of UTF-8 encoded
 * characters.  - TD
 */
const char *LYmbcs_skip_cells(const char *data,
			      int n_cells,
			      int utf_flag)
{
    const char *result;
    int actual;
    int target = n_cells;

    do {
	result = LYmbcs_skip_glyphs(data, target--, utf_flag);
	actual = LYstrExtent2(data, (int) (result - data));
    } while ((actual > 0) && (actual > n_cells));
    return result;
}

/*
 * LYmbcsstrlen() returns the printable length of a string that might contain
 * IsSpecial or multibyte (CJK or UTF8) characters.  - FM
 *
 * Counts glyph cells if count_gcells is set.  (Full-width characters in CJK
 * mode count as two.) Counts character glyphs if count_gcells is unset. 
 * (Full- width characters in CJK mode count as one.) - kw
 */
int LYmbcsstrlen(const char *str,
		 int utf_flag,
		 int count_gcells)
{
    int i, j, len = 0;

    if (non_empty(str)) {
#ifdef WIDEC_CURSES
	if (count_gcells) {
	    len = LYstrCells(str);
	} else
#endif
	{
	    for (i = 0; str[i] != '\0'; i++) {
		if (!IsSpecialAttrChar(str[i])) {
		    len++;
		    if (IS_NEW_GLYPH(str[i])) {
			j = 0;
			while (IsNormalChar(str[(i + 1)]) &&
			       j < 5 &&
			       IS_UTF_EXTRA(str[(i + 1)])) {
			    i++;
			    j++;
			}
		    } else if (!utf_flag && IS_CJK_TTY && !count_gcells &&
			       is8bits(str[i]) &&
			       IsNormalChar(str[(i + 1)])) {
			i++;
		    }
		}
	    }
	}
    }
    return (len);
}

#undef GetChar

#ifdef USE_SLANG
#if defined(VMS)
#define GetChar() ttgetc()
#elif defined(__DJGPP__)
#define GetChar() getxkey()	/* HTDos.c */
#elif defined(__CYGWIN__)
#define GetChar SLkp_getkey
#else
#define GetChar (int)SLang_getkey
#endif
#else /* curses */
#if defined(DJGPP)
#define GetChar() (djgpp_idle_loop(), wgetch(LYtopwindow()))
#elif defined(NCURSES_VERSION) && defined(__BEOS__)
#define GetChar() myGetCharNodelay()
#elif defined(NCURSES)
#define GetChar() wgetch(LYtopwindow())
#endif
#endif

#ifdef USE_CURSES_NODELAY
/* PDCurses - until version 2.7 in 2005 - defined ERR as 0, unlike other
 * versions of curses.  Generally both EOF and ERR are defined as -1's. 
 * However, there is a special case (see HTCheckForInterrupt()) to handle a
 * case where no select() function is used in the win32 environment.
 *
 * HTCheckForInterrupt() uses nodelay() in this special case to check for
 * pending input.  That normally returns ERR.  But LYgetch_for() checks the
 * return value of this function for EOF (to handle some antique runtime
 * libraries which did not set the state for feof/ferror).  Returning a zero
 * (0) is safer since normally that is not mapped to any commands, and will be
 * ignored by lynx.
 */
static int myGetCharNodelay(void)
{
    int c = wgetch(LYwin);

    if (c == -1)
	c = 0;

    return c;
}
#else
#define myGetCharNodelay() wgetch(LYwin)
#endif

#if !defined(GetChar) && defined(PDCURSES) && defined(PDC_BUILD) && PDC_BUILD >= 2401
/* PDCurses sends back key-modifiers that we don't use, but would waste time
 * upon, e.g., repainting the status line
 */
static int myGetChar(void)
{
    int c;
    BOOL done = FALSE;

    do {
	switch (c = myGetCharNodelay()) {
	case KEY_SHIFT_L:
	case KEY_SHIFT_R:
	case KEY_CONTROL_L:
	case KEY_CONTROL_R:
	case KEY_ALT_L:
	case KEY_ALT_R:
	case KEY_RESIZE:
	    break;
	default:
	    done = TRUE;
	    break;
	}
    } while (!done);

    return c;
}
#define GetChar() myGetChar()
#endif

#if !defined(GetChar) && defined(SNAKE)
#define GetChar() wgetch(LYwin)
#endif

#if !defined(GetChar) && defined(VMS)
#define GetChar() ttgetc()
#endif

#if !defined(GetChar)
#ifdef HAVE_KEYPAD
#define GetChar() getch()
#else
#ifndef USE_GETCHAR
#define USE_GETCHAR
#endif /* !USE_GETCHAR */
#define GetChar() getchar()	/* used to be "getc(stdin)" and "getch()" */
#endif /* HAVE_KEYPAD */
#endif /* !defined(GetChar) */

#if defined(USE_SLANG) && defined(USE_MOUSE)
static int sl_parse_mouse_event(int *x, int *y, int *button)
{
    /* "ESC [ M" has already been processed.  There more characters are
     * expected:  BUTTON X Y
     */
    *button = (int) SLang_getkey();
    switch (*button) {
    case 040:			/* left button */
    case 041:			/* middle button */
    case 042:			/* right button */
	*button -= 040;
	break;

    default:			/* Hmmm.... */
	SLang_flush_input();
	return -1;
    }

    *x = (int) SLang_getkey();
    if (*x == CH_ESC)		/* Undo 7-bit replace for large x - kw */
	*x = (int) SLang_getkey() + 64 - 33;
    else
	*x -= 33;
    *y = (int) SLang_getkey();
    if (*y == CH_ESC)		/* Undo 7-bit replace for large y - kw */
	*y = (int) SLang_getkey() + 64 - 33;
    else
	*y -= 33;
    return 0;
}

static int sl_read_mouse_event(int code)
{
    int mouse_x, mouse_y, button;

    mouse_link = -1;
    if (-1 != sl_parse_mouse_event(&mouse_x, &mouse_y, &button)) {
	if (button == 0)	/* left */
	    return set_clicked_link(mouse_x, mouse_y, FOR_PANEL, 1);

	if (button == 1)	/* middle */
	    return LYReverseKeymap(LYK_VIEW_BOOKMARK);

	if (button == 2)	/* right */
	{
	    /* Right button: go back to prev document.
	     * The problem is that we need to determine
	     * what to return to achieve this.
	     */
	    return LYReverseKeymap(LYK_PREV_DOC);
	}
    }
    if (code == FOR_INPUT || code == FOR_PROMPT)
	return DO_NOTHING;
    else
	return -1;
}
#endif /* USE_SLANG and USE_MOUSE */

static BOOLEAN csi_is_csi = TRUE;
void ena_csi(int flag)
{
    csi_is_csi = (BOOLEAN) flag;
}

#if defined(USE_KEYMAPS)

#ifdef USE_SLANG
#define define_key(string, code) \
	SLkm_define_keysym ((SLFUTURE_CONST char*)(string), \
			    (unsigned) code, \
			    Keymap_List)
#if SLANG_VERSION < 20000
#define expand_substring(target, first, last, final) \
 	(SLexpand_escaped_string(target, \
				 DeConst(first), \
				 DeConst(last), 1)
static int SLang_get_error(void)
{
    return SLang_Error;
}
#else
int LY_Slang_UTF8_Mode = 0;

#define expand_substring(target, first, last, final) \
	(SLexpand_escaped_string(target, \
				 DeConst(first), \
				 DeConst(last), \
				 LY_Slang_UTF8_Mode), 1)
#endif

static SLKeyMap_List_Type *Keymap_List;

/* This value should be larger than anything in LYStrings.h */
#define MOUSE_KEYSYM 0x0400
#endif

/*
 * For ncurses, we use the predefined keysyms, since that lets us also reuse
 * the CSI logic and other special cases for VMS, NCSA telnet, etc.
 */
#ifdef USE_SLANG
# ifdef VMS
#  define EXTERN_KEY(string,string1,lynx,curses) {string,lynx}
# else
#  define EXTERN_KEY(string,string1,lynx,curses) {string,lynx},{string1,lynx}
# endif
# define INTERN_KEY(string,lynx,curses)          {string,lynx,lynx}
#else
# define INTERN_KEY(string,lynx,curses)          {string,curses,lynx}
# define EXTERN_KEY(string,string1,lynx,curses)  {string,curses,lynx}
#endif

typedef struct {
    const char *string;
    int value;
    LYExtraKeys internal;
} Keysym_String_List;
/* *INDENT-OFF* */
static Keysym_String_List Keysym_Strings [] =
{
    INTERN_KEY( "UPARROW",	UPARROW_KEY,	KEY_UP ),
    INTERN_KEY( "DNARROW",	DNARROW_KEY,	KEY_DOWN ),
    INTERN_KEY( "RTARROW",	RTARROW_KEY,	KEY_RIGHT ),
    INTERN_KEY( "LTARROW",	LTARROW_KEY,	KEY_LEFT ),
    INTERN_KEY( "PGDOWN",	PGDOWN_KEY,	KEY_NPAGE ),
    INTERN_KEY( "PGUP",		PGUP_KEY,	KEY_PPAGE ),
    INTERN_KEY( "HOME",		HOME_KEY,	KEY_HOME ),
    INTERN_KEY( "END",		END_KEY,	KEY_END ),
    INTERN_KEY( "F1",		F1_KEY,		KEY_F(1) ),
    INTERN_KEY( "F2",		F2_KEY,		KEY_F(2) ),
    INTERN_KEY( "F3",		F3_KEY,		KEY_F(3) ),
    INTERN_KEY( "F4",		F4_KEY,		KEY_F(4) ),
    INTERN_KEY( "F5",		F5_KEY,		KEY_F(5) ),
    INTERN_KEY( "F6",		F6_KEY,		KEY_F(7) ),
    INTERN_KEY( "F7",		F7_KEY,		KEY_F(7) ),
    INTERN_KEY( "F8",		F8_KEY,		KEY_F(8) ),
    INTERN_KEY( "F9",		F9_KEY,		KEY_F(9) ),
    INTERN_KEY( "F10",		F10_KEY,	KEY_F(10) ),
    INTERN_KEY( "F11",		F11_KEY,	KEY_F(11) ),
    INTERN_KEY( "F12",		F12_KEY,	KEY_F(12) ),
    INTERN_KEY( "DO_KEY",	DO_KEY,		KEY_F(16) ),
    INTERN_KEY( "FIND_KEY",	FIND_KEY,	KEY_FIND ),
    INTERN_KEY( "SELECT_KEY",	SELECT_KEY,	KEY_SELECT ),
    INTERN_KEY( "INSERT_KEY",	INSERT_KEY,	KEY_IC ),
    INTERN_KEY( "REMOVE_KEY",	REMOVE_KEY,	KEY_DC ),
    INTERN_KEY( "DO_NOTHING",	DO_NOTHING,	DO_NOTHING|LKC_ISLKC ),
    INTERN_KEY( "BACKTAB_KEY",	BACKTAB_KEY,	BACKTAB_KEY ),
    INTERN_KEY( NULL,		UNKNOWN_KEY,	ERR )
};
/* *INDENT-ON* */

#ifdef NCURSES_VERSION
/*
 * Ncurses stores the termcap/terminfo names in arrays sorted to match the
 * array of strings in the TERMTYPE struct.
 */
static int lookup_tiname(char *name, NCURSES_CONST char *const *names)
{
    int code;

    for (code = 0; names[code] != 0; code++)
	if (!strcmp(names[code], name))
	    return code;
    return -1;
}

static const char *expand_tiname(const char *first, size_t len, char **result, char *final)
{
    char name[BUFSIZ];
    int code;

    LYStrNCpy(name, first, len);
    if ((code = lookup_tiname(name, strnames)) >= 0
	|| (code = lookup_tiname(name, strfnames)) >= 0) {
	if (cur_term->type.Strings[code] != 0) {
	    LYStrNCpy(*result, cur_term->type.Strings[code], (final - *result));
	    (*result) += strlen(*result);
	}
    }
    return first + len;
}

static const char *expand_tichar(const char *first, char **result, char *final)
{
    int ch;
    int limit = 0;
    int radix = 0;
    int value = 0;
    const char *name = 0;

    switch (ch = *first++) {
    case 'E':
    case 'e':
	value = 27;
	break;
    case 'a':
	name = "bel";
	break;
    case 'b':
	value = '\b';
	break;
    case 'f':
	value = '\f';
	break;
    case 'n':
	value = '\n';
	break;
    case 'r':
	value = '\r';
	break;
    case 't':
	value = '\t';
	break;
    case 'v':
	value = '\v';
	break;
    case 'd':
	radix = 10;
	limit = 3;
	break;
    case 'x':
	radix = 16;
	limit = 2;
	break;
    default:
	if (isdigit(ch)) {
	    radix = 8;
	    limit = 3;
	    first--;
	} else {
	    value = *first;
	}
	break;
    }

    if (radix != 0) {
	char *last = 0;
	char tmp[80];

	LYStrNCpy(tmp, first, limit);
	value = (int) strtol(tmp, &last, radix);
	if (last != 0 && last != tmp)
	    first += (last - tmp);
    }

    if (name != 0) {
	(void) expand_tiname(name, strlen(name), result, final);
    } else {
	**result = (char) value;
	(*result) += 1;
    }

    return first;
}

static BOOLEAN expand_substring(char *target,
				const char *first,
				const char *last,
				char *final)
{
    int ch;

    while (first < last) {
	switch (ch = *first++) {
	case ESCAPE:
	    first = expand_tichar(first, &target, final);
	    break;
	case '^':
	    ch = *first++;
	    if (ch == LPAREN) {
		const char *s = StrChr(first, RPAREN);
		char *was = target;

		if (s == 0)
		    s = first + strlen(first);
		first = expand_tiname(first, (size_t) (s - first), &target, final);
		if (target == was)
		    return FALSE;
		if (*first)
		    first++;
	    } else if (ch == '?') {	/* ASCII delete? */
		*target++ = 127;
	    } else if ((ch & 0x3f) < 0x20) {	/* ASCII control char? */
		*target++ = (char) (ch & 0x1f);
	    } else {
		*target++ = '^';
		first--;	/* not legal... */
	    }
	    break;
	case 0:		/* convert nulls for terminfo */
	    ch = 0200;
	    /* FALLTHRU */
	default:
	    *target++ = (char) ch;
	    break;
	}
    }
    *target = '\0';
    return TRUE;
}
#endif

static void unescaped_char(const char *parse, int *keysym)
{
    size_t len = strlen(parse);
    char buf[BUFSIZ];

    if (len >= 3) {
	(void) expand_substring(buf,
				parse + 1,
				parse + len - 1,
				buf + sizeof(buf) - 1);
	if (strlen(buf) == 1)
	    *keysym = *buf;
    }
}

static BOOLEAN unescape_string(char *source, char *target, char *final)
{
    BOOLEAN ok = FALSE;

    if (*source == SQUOTE) {
	int keysym = -1;

	unescaped_char(source, &keysym);
	if (keysym >= 0) {
	    target[0] = (char) keysym;
	    target[1] = '\0';
	    ok = TRUE;
	}
    } else if (*source == DQUOTE) {
	if (expand_substring(target, source + 1, source + strlen(source) - 1, final))
	    ok = TRUE;
	(void) final;
    }
    return ok;
}

static Keysym_String_List *lookupKeysymByName(const char *name)
{
    Keysym_String_List *k;
    Keysym_String_List *result = 0;

    k = Keysym_Strings;
    while (k->string != NULL) {
	if (0 == strcasecomp(k->string, name)) {
	    result = k;
	    break;
	}
	k++;
    }
    return result;
}

int map_string_to_keysym(const char *str, int *keysym)
{
    int modifier = 0;

    *keysym = -1;

    if (strncasecomp(str, "LAC:", 4) == 0) {
	char *other = StrChr(str + 4, ':');

	if (other) {
	    int othersym = lecname_to_lec(other + 1);
	    char buf[BUFSIZ];

	    if (othersym >= 0 && other - str - 4 < BUFSIZ) {
		LYStrNCpy(buf, str + 4, (other - str - 4));
		*keysym = lacname_to_lac(buf);
		if (*keysym >= 0) {
		    *keysym = LACLEC_TO_LKC0(*keysym, othersym);
		    return (*keysym);
		}
	    }
	}
	*keysym = lacname_to_lac(str + 4);
	if (*keysym >= 0) {
	    *keysym = LAC_TO_LKC0(*keysym);
	    return (*keysym);
	}
    } else if (strncasecomp(str, "Meta-", 5) == 0) {
	str += 5;
	modifier = LKC_MOD2;
	if (*str) {
	    size_t len = strlen(str);

	    if (len == 1) {
		return (*keysym = (UCH(str[0])) | modifier);
	    } else if (len == 2 && str[0] == '^' &&
		       (isalpha(UCH(str[1])) ||
			(TOASCII(str[1]) >= '@' && TOASCII(str[1]) <= '_'))) {
		return (*keysym = FROMASCII(UCH(str[1] & 0x1f)) | modifier);
	    } else if (len == 2 && str[0] == '^' &&
		       str[1] == '?') {
		return (*keysym = CH_DEL | modifier);
	    }
	    if (*str == '^' || *str == '\\') {
		char buf[BUFSIZ];

		(void) expand_substring(buf,
					str,
					str + HTMIN(len, 28),
					buf + sizeof(buf) - 1);
		if (strlen(buf) <= 1)
		    return (*keysym = (UCH(buf[0])) | modifier);
	    }
	}
    } else if (*str == SQUOTE) {
	unescaped_char(str, keysym);
    } else if (isdigit(UCH(*str))) {
	char *tmp;
	long value = strtol(str, &tmp, 0);

	if (!isalnum(UCH(*tmp))) {
	    *keysym = (int) value;
#ifndef USE_SLANG
	    if (*keysym > 255)
		*keysym |= LKC_ISLKC;	/* caller should remove this flag - kw */
#endif
	}
    } else {
	Keysym_String_List *k = lookupKeysymByName(str);

	if (k != 0) {
	    *keysym = k->value;
	}
    }

    if (*keysym >= 0)
	*keysym |= modifier;
    return (*keysym);
}

LYExtraKeys LYnameToExtraKeys(const char *name)
{
    Keysym_String_List *k = lookupKeysymByName(name);
    LYExtraKeys result = UNKNOWN_KEY;

    if (k != 0)
	result = k->internal;
    return result;
}

const char *LYextraKeysToName(LYExtraKeys code)
{
    Keysym_String_List *k;
    const char *result = 0;

    k = Keysym_Strings;
    while (k->string != NULL) {
	if (k->internal == code) {
	    result = k->string;
	    break;
	}
	k++;
    }
    return result;
}

/*
 * Starting at a nonblank character, skip over a token, counting quoted and
 * escaped characters.
 */
static char *skip_keysym(char *parse)
{
    int quoted = 0;
    int escaped = 0;

    while (*parse) {
	if (escaped) {
	    escaped = 0;
	} else if (quoted) {
	    if (*parse == ESCAPE) {
		escaped = 1;
	    } else if (*parse == quoted) {
		quoted = 0;
	    }
	} else if (*parse == ESCAPE) {
	    escaped = 1;
	} else if (*parse == DQUOTE || *parse == SQUOTE) {
	    quoted = *parse;
	} else if (isspace(UCH(*parse))) {
	    break;
	}
	parse++;
    }
    return (quoted || escaped) ? 0 : parse;
}

/*
 * The first token is the string to define, the second is the name (of the
 * keysym) to define it to.
 */
#define MY_TRACE(p) CTRACE2(TRACE_CFG, p)

static int setkey_cmd(char *parse)
{
    char *s, *t;
    int keysym;
    char buf[BUFSIZ];

    MY_TRACE((tfp, "KEYMAP(PA): in=%s", parse));	/* \n-terminated */
    if ((s = skip_keysym(parse)) != 0) {
	if (isspace(UCH(*s))) {
	    *s++ = '\0';
	    s = LYSkipBlanks(s);
	    if ((t = skip_keysym(s)) == 0) {
		MY_TRACE((tfp, "KEYMAP(SKIP) no key expansion found\n"));
		return -1;
	    }
	    if (t != s)
		*t = '\0';
	    if (map_string_to_keysym(s, &keysym) >= 0) {
		if (!unescape_string(parse, buf, buf + sizeof(buf) - 1)) {
		    MY_TRACE((tfp, "KEYMAP(SKIP) could unescape key\n"));
		    return 0;	/* Trace the failure and continue. */
		}
		if (LYTraceLogFP == 0) {
		    MY_TRACE((tfp, "KEYMAP(DEF) keysym=%#x\n", keysym));
		} else {
		    MY_TRACE((tfp, "KEYMAP(DEF) keysym=%#x, seq='%s'\n",
			      keysym, buf));
		}
		return define_key(buf, keysym);
	    } else {
		MY_TRACE((tfp, "KEYMAP(SKIP) could not map to keysym\n"));
	    }
	} else {
	    MY_TRACE((tfp, "KEYMAP(SKIP) junk after key description: '%s'\n", s));
	}
    } else {
	MY_TRACE((tfp, "KEYMAP(SKIP) no key description\n"));
    }
    return -1;
}
#undef MY_TRACE

static int unsetkey_cmd(char *parse)
{
    char *s = skip_keysym(parse);

    if (s != parse) {
	*s = '\0';
#ifdef NCURSES_VERSION
	/*
	 * This won't work with Slang.  Remove the definition for the given
	 * keysym.
	 */
	{
	    int keysym;

	    if (map_string_to_keysym(parse, &keysym) >= 0)
		define_key((char *) 0, keysym);
	}
#endif
#ifdef USE_SLANG
	/* Slang implements this, for undefining the string which is associated
	 * with a keysym (the reverse of what we normally want, but may
	 * occasionally find useful).
	 */
	SLang_undefine_key(parse, Keymap_List);
	if (SLang_get_error())
	    return -1;
#endif
    }
    return 0;
}

#ifdef FNAMES_8_3
#define FNAME_LYNX_KEYMAPS "_lynxkey.map"
#else
#define FNAME_LYNX_KEYMAPS ".lynx-keymaps"
#endif /* FNAMES_8_3 */

static int read_keymap_file(void)
{
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	int (*func) (char *s);
    } table[] = {
	{ "setkey",   setkey_cmd },
	{ "unsetkey", unsetkey_cmd },
    };
    /* *INDENT-ON* */

    char *line = NULL;
    FILE *fp;
    char file[LY_MAXPATH];
    int linenum;
    size_t n;

    LYAddPathToHome(file, sizeof(file), FNAME_LYNX_KEYMAPS);

    if ((fp = fopen(file, "r")) == 0)
	return 0;

    linenum = 0;
    while (LYSafeGets(&line, fp) != 0) {
	char *s = LYSkipBlanks(line);

	linenum++;

	if ((*s == 0) || (*s == '#'))
	    continue;

	for (n = 0; n < TABLESIZE(table); n++) {
	    size_t len = strlen(table[n].name);

	    if (strlen(s) > len && !StrNCmp(s, table[n].name, len)
		&& (*(table[n].func)) (LYSkipBlanks(s + len)) < 0)
		fprintf(stderr, FAILED_READING_KEYMAP, linenum, file);
	}
    }
    FREE(line);
    LYCloseInput(fp);
    return 0;
}

static void setup_vtXXX_keymap(void)
{
    /* *INDENT-OFF* */
    static Keysym_String_List table[] = {
	INTERN_KEY( "\033[A",	UPARROW_KEY,	KEY_UP ),
	INTERN_KEY( "\033OA",	UPARROW_KEY,	KEY_UP ),
	INTERN_KEY( "\033[B",	DNARROW_KEY,	KEY_DOWN ),
	INTERN_KEY( "\033OB",	DNARROW_KEY,	KEY_DOWN ),
	INTERN_KEY( "\033[C",	RTARROW_KEY,	KEY_RIGHT ),
	INTERN_KEY( "\033OC",	RTARROW_KEY,	KEY_RIGHT ),
	INTERN_KEY( "\033[D",	LTARROW_KEY,	KEY_LEFT ),
	INTERN_KEY( "\033OD",	LTARROW_KEY,	KEY_LEFT ),
	INTERN_KEY( "\033[1~",	FIND_KEY,	KEY_FIND ),
	INTERN_KEY( "\033[2~",	INSERT_KEY,	KEY_IC ),
	INTERN_KEY( "\033[3~",	REMOVE_KEY,	KEY_DC ),
	INTERN_KEY( "\033[4~",	SELECT_KEY,	KEY_SELECT ),
	INTERN_KEY( "\033[5~",	PGUP_KEY,	KEY_PPAGE ),
	INTERN_KEY( "\033[6~",	PGDOWN_KEY,	KEY_NPAGE ),
	INTERN_KEY( "\033[7~",	HOME_KEY,	KEY_HOME),
	INTERN_KEY( "\033[8~",	END_KEY,	KEY_END ),
	INTERN_KEY( "\033[11~",	F1_KEY,		KEY_F(1) ),
	INTERN_KEY( "\033[28~",	F1_KEY,		KEY_F(1) ),
	INTERN_KEY( "\033OP",	F1_KEY,		KEY_F(1) ),
	INTERN_KEY( "\033[OP",	F1_KEY,		KEY_F(1) ),
	INTERN_KEY( "\033[29~",	DO_KEY,		KEY_F(16) ),
#if defined(USE_SLANG)
#if defined(__WIN32__) || defined(__MINGW32__)
	INTERN_KEY( "\xE0H",	UPARROW_KEY,	KEY_UP ),
	INTERN_KEY( "\xE0P",	DNARROW_KEY,	KEY_DOWN ),
	INTERN_KEY( "\xE0M",	RTARROW_KEY,	KEY_RIGHT ),
	INTERN_KEY( "\xE0K",	LTARROW_KEY,	KEY_LEFT ),
	INTERN_KEY( "\xE0R",	INSERT_KEY,	KEY_IC ),
	INTERN_KEY( "\xE0S",	REMOVE_KEY,	KEY_DC ),
	INTERN_KEY( "\xE0I",	PGUP_KEY,	KEY_PPAGE ),
	INTERN_KEY( "\xE0Q",	PGDOWN_KEY,	KEY_NPAGE ),
	INTERN_KEY( "\xE0G",	HOME_KEY,	KEY_HOME),
	INTERN_KEY( "\xE0O",	END_KEY,	KEY_END ),
#endif
#if !defined(VMS)
	INTERN_KEY(	"^(ku)", UPARROW_KEY,	KEY_UP ),
	INTERN_KEY(	"^(kd)", DNARROW_KEY,	KEY_DOWN ),
	INTERN_KEY(	"^(kr)", RTARROW_KEY,	KEY_RIGHT ),
	INTERN_KEY(	"^(kl)", LTARROW_KEY,	KEY_LEFT ),
	INTERN_KEY(	"^(@0)", FIND_KEY,	KEY_FIND ),
	INTERN_KEY(	"^(kI)", INSERT_KEY,	KEY_IC ),
	INTERN_KEY(	"^(kD)", REMOVE_KEY,	KEY_DC ),
	INTERN_KEY(	"^(*6)", SELECT_KEY,	KEY_SELECT ),
	INTERN_KEY(	"^(kP)", PGUP_KEY,	KEY_PPAGE ),
	INTERN_KEY(	"^(kN)", PGDOWN_KEY,	KEY_NPAGE ),
	INTERN_KEY(	"^(@7)", END_KEY,	KEY_END ),
	INTERN_KEY(	"^(kh)", HOME_KEY,	KEY_HOME),
	INTERN_KEY(	"^(k1)", F1_KEY,	KEY_F(1) ),
	INTERN_KEY(	"^(k2)", F2_KEY,	KEY_F(2) ),
	INTERN_KEY(	"^(k3)", F3_KEY,	KEY_F(3) ),
	INTERN_KEY(	"^(k4)", F4_KEY,	KEY_F(4) ),
	INTERN_KEY(	"^(k5)", F5_KEY,	KEY_F(5) ),
	INTERN_KEY(	"^(k6)", F6_KEY,	KEY_F(6) ),
	INTERN_KEY(	"^(k7)", F7_KEY,	KEY_F(7) ),
	INTERN_KEY(	"^(k8)", F8_KEY,	KEY_F(8) ),
	INTERN_KEY(	"^(k9)", F9_KEY,	KEY_F(9) ),
	INTERN_KEY(	"^(k;)", F10_KEY,	KEY_F(10) ),
	INTERN_KEY(	"^(F1)", F11_KEY,	KEY_F(11) ),
	INTERN_KEY(	"^(F2)", F12_KEY,	KEY_F(12) ),
	INTERN_KEY(	"^(F6)", DO_KEY,	KEY_F(16) ),
#endif /* !VMS */
#endif /* SLANG */
    };
    /* *INDENT-ON* */

    size_t n;

    for (n = 0; n < TABLESIZE(table); n++)
	define_key(table[n].string, table[n].value);
}

int lynx_initialize_keymaps(void)
{
#ifdef USE_SLANG
    int i;
    char keybuf[2];

    /* The escape sequences may contain embedded termcap strings.  Make
     * sure the library is initialized for that.
     */
    SLtt_get_terminfo();

    if (NULL == (Keymap_List = SLang_create_keymap("Lynx", NULL)))
	return -1;

    keybuf[1] = 0;
    for (i = 1; i < 256; i++) {
	keybuf[0] = (char) i;
	define_key(keybuf, i);
    }

    setup_vtXXX_keymap();
    define_key("\033[M", MOUSE_KEYSYM);

    if (SLang_get_error())
	SLang_exit_error("Unable to initialize keymaps");
#else
    setup_vtXXX_keymap();
#endif
    return read_keymap_file();
}

#endif /* USE_KEYMAPS */

#if defined(USE_MOUSE) && (defined(NCURSES))
static int LYmouse_menu(int x, int y, int atlink, int code)
{
#define ENT_ONLY_DOC	1
#define ENT_ONLY_LINK	2
    /* *INDENT-OFF* */
    static const struct {
	const char *txt;
	int  action;
	unsigned int  flag;
    } possible_entries[] = {
	{"Quit",			LYK_ABORT,		ENT_ONLY_DOC},
	{"Home page",			LYK_MAIN_MENU,		ENT_ONLY_DOC},
	{"Previous document",		LYK_PREV_DOC,		ENT_ONLY_DOC},
	{"Beginning of document",	LYK_HOME,		ENT_ONLY_DOC},
	{"Page up",			LYK_PREV_PAGE,		ENT_ONLY_DOC},
	{"Half page up",		LYK_UP_HALF,		ENT_ONLY_DOC},
	{"Two lines up",		LYK_UP_TWO,		ENT_ONLY_DOC},
	{"History",			LYK_HISTORY,		ENT_ONLY_DOC},
	{"Help",			LYK_HELP,		0},
	{"Do nothing (refresh)",	LYK_REFRESH,		0},
	{"Load again",			LYK_RELOAD,		ENT_ONLY_DOC},
	{"Edit Doc URL and load",	LYK_ECGOTO,		ENT_ONLY_DOC},
	{"Edit Link URL and load",	LYK_ELGOTO,		ENT_ONLY_LINK},
	{"Show info",			LYK_INFO,		0},
	{"Search",			LYK_WHEREIS,		ENT_ONLY_DOC},
	{"Print",			LYK_PRINT,		ENT_ONLY_DOC},
	{"Two lines down",		LYK_DOWN_TWO,		ENT_ONLY_DOC},
	{"Half page down",		LYK_DOWN_HALF,		ENT_ONLY_DOC},
	{"Page down",			LYK_NEXT_PAGE,		ENT_ONLY_DOC},
	{"End of document",		LYK_END,		ENT_ONLY_DOC},
	{"Bookmarks",			LYK_VIEW_BOOKMARK,	ENT_ONLY_DOC},
	{"Cookie jar",			LYK_COOKIE_JAR,		ENT_ONLY_DOC},
#ifdef USE_CACHEJAR
	{"Cache jar",			LYK_CACHE_JAR,		ENT_ONLY_DOC},
#endif
	{"Search index",		LYK_INDEX_SEARCH,	ENT_ONLY_DOC},
	{"Set Options",			LYK_OPTIONS,		ENT_ONLY_DOC},
	{"Activate this link",		LYK_MOUSE_SUBMIT,	ENT_ONLY_LINK},
	{"Download",			LYK_DOWNLOAD,		ENT_ONLY_LINK}
    };
    /* *INDENT-ON* */

#define TOTAL_MENUENTRIES	TABLESIZE(possible_entries)
    const char *choices[TOTAL_MENUENTRIES + 1];
    int actions[TOTAL_MENUENTRIES];

    int c, c1, retlac;
    unsigned filter_out = (unsigned) (atlink ? ENT_ONLY_DOC : ENT_ONLY_LINK);

    c = c1 = 0;
    while (c < (int) TOTAL_MENUENTRIES) {
	if (!(possible_entries[c].flag & filter_out)) {
	    choices[c1] = possible_entries[c].txt;
	    actions[c1++] = possible_entries[c].action;
	}
	c++;
    }
    choices[c1] = NULL;

    /* Somehow the mouse is over the number instead of being over the
       name, so we decrease x. */
    c = LYChoosePopup((atlink ? 2 : 10) - 1, y, (x > 5 ? x - 5 : 1),
		      choices, c1, FALSE, TRUE);

    /*
     * LYhandlePopupList() wasn't really meant to be used outside of old-style
     * Options menu processing.  One result of mis-using it here is that we
     * have to deal with side-effects regarding SIGINT signal handler and the
     * term_options global variable.  - kw
     */
    if (term_options) {
	retlac = LYK_DO_NOTHING;
	term_options = FALSE;
    } else {
	retlac = actions[c];
    }

    if (code == FOR_INPUT && mouse_link == -1) {
	switch (retlac) {
	case LYK_ABORT:
	    retlac = LYK_QUIT;	/* a bit softer... */
	    /* fall through */
	case LYK_MAIN_MENU:
	case LYK_PREV_DOC:
	case LYK_HOME:
	case LYK_PREV_PAGE:
	case LYK_UP_HALF:
	case LYK_UP_TWO:
	case LYK_HISTORY:
	case LYK_HELP:
/*	    case LYK_REFRESH:*/
	case LYK_RELOAD:
	case LYK_ECGOTO:
	case LYK_INFO:
	case LYK_WHEREIS:
	case LYK_PRINT:
	case LYK_DOWN_TWO:
	case LYK_DOWN_HALF:
	case LYK_NEXT_PAGE:
	case LYK_END:
	case LYK_VIEW_BOOKMARK:
	case LYK_COOKIE_JAR:
#ifdef USE_CACHEJAR
	case LYK_CACHE_JAR:
#endif
	case LYK_INDEX_SEARCH:
	case LYK_OPTIONS:
	    mouse_link = -3;	/* so LYgetch_for() passes it on - kw */
	}
    }
    if (retlac == LYK_DO_NOTHING ||
	retlac == LYK_REFRESH) {
	mouse_link = -1;	/* mainloop should not change cur link - kw */
    }
    if (code == FOR_INPUT && retlac == LYK_DO_NOTHING) {
	repaint_main_statusline(FOR_INPUT);
    }
    return retlac;
}
#endif /* USE_MOUSE && (NCURSES || PDCURSES) */

#if defined(USE_KEYMAPS) && defined(USE_SLANG)
/************************************************************************/

static int current_sl_modifier = 0;

/* We cannot guarantee the type for 'GetChar', and should not use a cast. */
static int myGetChar(void)
{
    int i = GetChar();

    if (i == 0)			/* trick to get NUL char through - kw */
	current_sl_modifier = LKC_ISLKC;
    return i;
}

static int LYgetch_for(int code)
{
    SLang_Key_Type *key;
    int keysym;

    current_sl_modifier = 0;

    key = SLang_do_key(Keymap_List, myGetChar);
    if ((key == NULL) || (key->type != SLKEY_F_KEYSYM)) {
#if defined(__WIN32__) || defined(__MINGW32__)
	if ((key == NULL) && (current_sl_modifier == LKC_ISLKC)) {
	    key = SLang_do_key(Keymap_List, myGetChar);
	    keysym = key->f.keysym;
	    switch (keysym) {
	    case 'H':
		keysym = UPARROW_KEY;
		break;
	    case 'P':
		keysym = DNARROW_KEY;
		break;
	    case 'M':
		keysym = RTARROW_KEY;
		break;
	    case 'K':
		keysym = LTARROW_KEY;
		break;
	    case 'R':
		keysym = INSERT_KEY;
		break;
	    case 'S':
		keysym = REMOVE_KEY;
		break;
	    case 'I':
		keysym = PGUP_KEY;
		break;
	    case 'Q':
		keysym = PGDOWN_KEY;
		break;
	    case 'G':
		keysym = HOME_KEY;
		break;
	    case 'O':
		keysym = END_KEY;
		break;
	    case ';':
		keysym = F1_KEY;
		break;
	    }
	    return (keysym);
	}
#endif
	return (current_sl_modifier ? 0 : DO_NOTHING);
    } else {
	keysym = (int) key->f.keysym;

#if defined (USE_MOUSE)
	if (keysym == MOUSE_KEYSYM)
	    return sl_read_mouse_event(code);
#endif

	if (keysym < 0) {
	    return 0;

	} else if (keysym & (LKC_ISLECLAC | LKC_ISLAC)) {
	    return (keysym);
	} else {
	    current_sl_modifier = 0;
	    if (LKC_HAS_ESC_MOD(keysym)) {
		current_sl_modifier = LKC_MOD2;
		keysym &= LKC_MASK;
	    }

	    if (keysym + 1 >= KEYMAP_SIZE) {
		return 0;
	    } else {
		return (keysym | current_sl_modifier);
	    }
	}
    }
}

/************************************************************************/
#else /* NOT  defined(USE_KEYMAPS) && defined(USE_SLANG) */

/*
 * LYgetch() translates some escape sequences and may fake noecho.
 */
#define found_CSI(first,second) ((second) == '[' || (first) == 155)
#define found_TLD(value)	((value) == '~')

static int LYgetch_for(int code)
{
    int a, b, c, d = -1;
    int current_modifier = 0;
    BOOLEAN done_esc = FALSE;

    (void) code;

    have_levent = 0;

  re_read:
#if !defined(UCX) || !defined(VAXC)	/* errno not modifiable ? */
    if (errno == EINTR)
	set_errno(0);		/* reset - kw */
#endif /* UCX && VAXC */
#ifndef USE_SLANG
    clearerr(stdin);		/* needed here for ultrix and SOCKETSHR, but why? - FM */
#endif /* !USE_SLANG */
#if !defined(USE_SLANG) || defined(VMS) || defined(DJGPP_KEYHANDLER)
    c = GetChar();
    lynx_nl2crlf(FALSE);
#else
    if (LYCursesON) {
	c = GetChar();
	lynx_nl2crlf(FALSE);
    } else {
	c = getchar();
	if (c == EOF && errno == EINTR)		/* Ctrl-Z causes EINTR in getchar() */
	    clearerr(stdin);
	if (feof(stdin) || ferror(stdin) || c == EOF) {
#ifdef IGNORE_CTRL_C
	    if (sigint)
		sigint = FALSE;
#endif /* IGNORE_CTRL_C */
	    CTRACE((tfp, "GETCH: Translate ^C to ^G.\n"));
	    return (LYCharINTERRUPT2);	/* use ^G to cancel whatever called us. */
	}
    }
#endif /* !USE_SLANG || VMS */

    CTRACE((tfp, "GETCH%d: Got %#x.\n", code, c));
#ifdef MISC_EXP
    if (LYNoZapKey > 1 && errno != EINTR &&
	(c == EOF
#ifdef USE_SLANG
	 || (c == 0xFFFF)
#endif
	)) {

	CTRACE((tfp,
		"nozap: Got EOF, curses %s, stdin is %p, LYNoZapKey reduced from %d to 0.\n",
		LYCursesON ? "on" : "off", (void *) stdin, LYNoZapKey));
	LYNoZapKey = 0;		/* 2 -> 0 */
	if (LYReopenInput() > 0) {
	    if (LYCursesON) {
		stop_curses();
		start_curses();
		LYrefresh();
	    }
	    goto re_read;
	}
    }
#endif /* MISC_EXP */

#ifdef USE_GETCHAR
    if (c == EOF && errno == EINTR)	/* Ctrl-Z causes EINTR in getchar() */
	goto re_read;
#else
    if (c == EOF && errno == EINTR) {

#if defined(HAVE_SIZECHANGE) || defined(USE_SLANG)
	CTRACE((tfp, "Got EOF with EINTR, recent_sizechange so far is %d\n",
		recent_sizechange));
	if (!recent_sizechange) {	/* not yet detected by ourselves */
	    size_change(0);
	    CTRACE((tfp, "Now recent_sizechange is %d\n", recent_sizechange));
	}
#else /* HAVE_SIZECHANGE || USE_SLANG */
	CTRACE((tfp, "Got EOF with EINTR, recent_sizechange is %d\n",
		recent_sizechange));
#endif /* HAVE_SIZECHANGE || USE_SLANG */
#if !defined(UCX) || !defined(VAXC)	/* errno not modifiable ? */
	set_errno(0);		/* reset - kw */
#endif /* UCX && VAXC */
	return (DO_NOTHING);
    }
#endif /* USE_GETCHAR */

#ifdef USE_SLANG
    if (c == 0xFFFF && LYCursesON) {
#ifdef IGNORE_CTRL_C
	if (sigint) {
	    sigint = FALSE;
	    goto re_read;
	}
#endif /* IGNORE_CTRL_C */
	return (LYCharINTERRUPT2);	/* use ^G to cancel whatever called us. */
    }
#else /* not USE_SLANG: */
    if (feof(stdin) || ferror(stdin) || c == EOF) {
	if (recent_sizechange)
	    return (LYCharINTERRUPT2);	/* use ^G to cancel whatever called us. */
#ifdef IGNORE_CTRL_C
	if (sigint) {
	    sigint = FALSE;
	    /* clearerr(stdin);  don't need here if stays above - FM */
	    goto re_read;
	}
#endif /* IGNORE_CTRL_C */
#if !defined(USE_GETCHAR) && !defined(VMS) && !defined(NCURSES)
	if (c == ERR && errno == EINTR)		/* may have been handled signal - kw */
	    goto re_read;
#endif /* USE_GETCHAR */

	cleanup();
	exit_immediately(EXIT_SUCCESS);
    }
#endif /* USE_SLANG */

    if (!escape_bound
	&& (c == CH_ESC || (csi_is_csi && c == UCH(CH_ESC_PAR)))) {
	/* handle escape sequence  S/390 -- gil -- 2024 */
	done_esc = TRUE;	/* Flag: we did it, not keypad() */
	b = GetChar();

	if (b == '[' || b == 'O') {
	    a = GetChar();
	} else {
	    a = b;
	}

	switch (a) {
	case 'A':
	    c = UPARROW_KEY;
	    break;
	case 'B':
	    c = DNARROW_KEY;
	    break;
	case 'C':
	    c = RTARROW_KEY;
	    break;
	case 'D':
	    c = LTARROW_KEY;
	    break;
	case 'q':		/* vt100 application keypad 1 */
	    c = END_KEY;
	    break;
	case 'r':		/* vt100 application keypad 2 */
	    c = DNARROW_KEY;
	    break;
	case 's':		/* vt100 application keypad 3 */
	    c = PGDOWN_KEY;
	    break;
	case 't':		/* vt100 application keypad 4 */
	    c = LTARROW_KEY;
	    break;
	case 'v':		/* vt100 application keypad 6 */
	    c = RTARROW_KEY;
	    break;
	case 'w':		/* vt100 application keypad 7 */
	    c = HOME_KEY;
	    break;
	case 'x':		/* vt100 application keypad 8 */
	    c = UPARROW_KEY;
	    break;
	case 'y':		/* vt100 application keypad 9 */
	    c = PGUP_KEY;
	    break;
	case 'M':
#if defined(USE_SLANG) && defined(USE_MOUSE)
	    if (found_CSI(c, b)) {
		c = sl_read_mouse_event(code);
	    } else
#endif
		c = '\n';	/* keypad enter on pc ncsa telnet */
	    break;

	case 'm':
#ifdef VMS
	    if (b != 'O')
#endif /* VMS */
		c = '-';	/* keypad on pc ncsa telnet */
	    break;
	case 'k':
	    if (b == 'O')
		c = '+';	/* keypad + on my xterminal :) */
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case 'l':
#ifdef VMS
	    if (b != 'O')
#endif /* VMS */
		c = '+';	/* keypad on pc ncsa telnet */
	    break;
	case 'P':
#ifdef VMS
	    if (b != 'O')
#endif /* VMS */
		c = F1_KEY;
	    break;
	case 'u':
#ifdef VMS
	    if (b != 'O')
#endif /* VMS */
		c = F1_KEY;	/* macintosh help button */
	    break;
	case 'p':
#ifdef VMS
	    if (b == 'O')
#endif /* VMS */
		c = '0';	/* keypad 0 */
	    break;
	case '1':		/* VTxxx  Find  */
	    if (found_CSI(c, b) && found_TLD(d = GetChar()))
		c = FIND_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '2':
	    if (found_CSI(c, b)) {
		if (found_TLD(d = GetChar()))	/* VTxxx Insert */
		    c = INSERT_KEY;
		else if ((d == '8' ||
			  d == '9') &&
			 found_TLD(GetChar())) {
		    if (d == '8')	/* VTxxx   Help */
			c = F1_KEY;
		    else if (d == '9')	/* VTxxx    Do  */
			c = DO_KEY;
		    d = -1;
		}
	    } else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '3':			     /** VTxxx Delete **/
	    if (found_CSI(c, b) && found_TLD(d = GetChar()))
		c = REMOVE_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '4':			     /** VTxxx Select **/
	    if (found_CSI(c, b) && found_TLD(d = GetChar()))
		c = SELECT_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '5':			     /** VTxxx PrevScreen **/
	    if (found_CSI(c, b) && found_TLD(d = GetChar()))
		c = PGUP_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '6':			     /** VTxxx NextScreen **/
	    if (found_CSI(c, b) && found_TLD(d = GetChar()))
		c = PGDOWN_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '[':			     /** Linux F1-F5: ^[[[A etc. **/
	    if (found_CSI(c, b)) {
		if ((d = GetChar()) == 'A')
		    c = F1_KEY;
		break;
	    }
	    /* FALLTHRU */
	default:
	    if (c == CH_ESC && a == b && !found_CSI(c, b)) {
		current_modifier = LKC_MOD2;
		c = a;
		/* We're not yet done if ESC + curses-keysym: */
		done_esc = (BOOL) ((a & ~0xFF) == 0);
		break;
	    }
	    CTRACE((tfp, "Unknown key sequence: %d:%d:%d\n", c, b, a));
	    CTRACE_SLEEP(MessageSecs);
	    break;
	}
	if (isdigit(a) && found_CSI(c, b) && d != -1 && !found_TLD(d))
	    d = GetChar();
	if (!done_esc && (a & ~0xFF) == 0) {
	    if (a == b && !found_CSI(c, b) && c == CH_ESC) {
		current_modifier = LKC_MOD2;
		c = a;
		done_esc = TRUE;
	    } else {
		done_esc = TRUE;
	    }
	}
    }
#ifdef USE_KEYMAPS
    /* Extract a single code if two are merged: */
    if (c >= 0 && (c & LKC_ISLECLAC)) {
	if (!(code == FOR_INPUT || code == FOR_PROMPT))
	    c = LKC2_TO_LKC(c);
    } else if (c >= 0 && (c & LKC_ISLKC)) {
	c &= ~LKC_ISLKC;
	done_esc = TRUE;	/* already a lynxkeycode, skip keypad switches - kw */
    }
    if (c >= 0 && LKC_HAS_ESC_MOD(c)) {
	current_modifier = LKC_MOD2;
	c &= LKC_MASK;
    }
    if (c >= 0 && (c & (LKC_ISLECLAC | LKC_ISLAC))) {
	done_esc = TRUE;	/* already a lynxactioncode, skip keypad switches - iz */
    }
#endif
    if (done_esc) {
	/* don't do keypad() switches below, we already got it - kw */
    } else {
#ifdef HAVE_KEYPAD
	/*
	 * Convert keypad() mode keys into Lynx defined keys.
	 */
	switch (c) {
	case KEY_DOWN:		/* The four arrow keys ... */
	    c = DNARROW_KEY;
	    break;
	case KEY_UP:
	    c = UPARROW_KEY;
	    break;
	case KEY_LEFT:
	    c = LTARROW_KEY;
	    break;
	case KEY_RIGHT:	/* ... */
	    c = RTARROW_KEY;
	    break;
#if defined(PDCURSES)		/* for NEC PC-9800 1998/08/30 (Sun) 21:50:35 */
	case KEY_C2:
	    c = DNARROW_KEY;
	    break;
	case KEY_A2:
	    c = UPARROW_KEY;
	    break;
	case KEY_B1:
	    c = LTARROW_KEY;
	    break;
	case KEY_B3:
	    c = RTARROW_KEY;
	    break;
	case PAD0:		/* PC-9800 Ins */
	    c = INSERT_KEY;
	    break;
	case PADSTOP:		/* PC-9800 DEL */
	    c = REMOVE_KEY;
	    break;
#endif /* PDCURSES */
	case KEY_HOME:		/* Home key (upward+left arrow) */
	    c = HOME_KEY;
	    break;
	case KEY_CLEAR:	/* Clear screen */
	    c = 18;		/* CTRL-R */
	    break;
	case KEY_NPAGE:	/* Next page */
	    c = PGDOWN_KEY;
	    break;
	case KEY_PPAGE:	/* Previous page */
	    c = PGUP_KEY;
	    break;
	case KEY_LL:		/* home down or bottom (lower left) */
	    c = END_KEY;
	    break;
#if defined(KEY_A1) && defined(KEY_C3)
	    /* The keypad is arranged like this: */
	    /*    a1    up    a3   */
	    /*   left   b2  right  */
	    /*    c1   down   c3   */
	case KEY_A1:		/* upper left of keypad */
	    c = HOME_KEY;
	    break;
	case KEY_A3:		/* upper right of keypad */
	    c = PGUP_KEY;
	    break;
	case KEY_B2:		/* center of keypad */
	    c = DO_NOTHING;
	    break;
	case KEY_C1:		/* lower left of keypad */
	    c = END_KEY;
	    break;
	case KEY_C3:		/* lower right of keypad */
	    c = PGDOWN_KEY;
	    break;
#endif /* defined(KEY_A1) && defined(KEY_C3) */
#ifdef KEY_ENTER
	case KEY_ENTER:	/* enter/return      */
	    c = '\n';
	    break;
#endif /* KEY_ENTER */
#ifdef PADENTER			/* PDCURSES */
	case PADENTER:
	    c = '\n';
	    break;
#endif /* PADENTER */
#ifdef KEY_END
	case KEY_END:		/* end key           001 */
	    c = END_KEY;
	    break;
#endif /* KEY_END */
#ifdef KEY_HELP
	case KEY_HELP:		/* help key          001 */
	    c = F1_KEY;
	    break;
#endif /* KEY_HELP */
#ifdef KEY_BACKSPACE
	case KEY_BACKSPACE:
	    c = CH_DEL;		/* backspace key (delete, not Ctrl-H)  S/390 -- gil -- 2041 */
	    break;
#endif /* KEY_BACKSPACE */
	case KEY_F(1):
	    c = F1_KEY;		/* VTxxx Help */
	    break;
#if defined(KEY_F) && !defined(__DJGPP__) && !defined(_WINDOWS)
	case KEY_F(16):
	    c = DO_KEY;		/* VTxxx Do */
	    break;
#endif /* KEY_F */
#ifdef KEY_REDO
	case KEY_REDO:		/* VTxxx Do */
	    c = DO_KEY;
	    break;
#endif /* KEY_REDO */
#ifdef KEY_FIND
	case KEY_FIND:
	    c = FIND_KEY;	/* VTxxx Find */
	    break;
#endif /* KEY_FIND */
#ifdef KEY_SELECT
	case KEY_SELECT:
	    c = SELECT_KEY;	/* VTxxx Select */
	    break;
#endif /* KEY_SELECT */
#ifdef KEY_IC
	case KEY_IC:
	    c = INSERT_KEY;	/* VTxxx Insert */
	    break;
#endif /* KEY_IC */
#ifdef KEY_DC
	case KEY_DC:
	    c = REMOVE_KEY;	/* VTxxx Remove */
	    break;
#endif /* KEY_DC */
#ifdef KEY_BTAB
	case KEY_BTAB:
	    c = BACKTAB_KEY;	/* Back tab, often Shift-Tab */
	    break;
#endif /* KEY_BTAB */
#ifdef KEY_RESIZE
	case KEY_RESIZE:	/* size change detected by ncurses */
#if defined(HAVE_SIZECHANGE) || defined(USE_SLANG)
	    /* Make call to detect new size, if that may be implemented.
	     * The call may set recent_sizechange (except for USE_SLANG),
	     * which will tell mainloop() to refresh. - kw
	     */
	    CTRACE((tfp, "Got KEY_RESIZE, recent_sizechange so far is %d\n",
		    recent_sizechange));
	    size_change(0);
	    CTRACE((tfp, "Now recent_sizechange is %d\n", recent_sizechange));
#else /* HAVE_SIZECHANGE || USE_SLANG */
	    CTRACE((tfp, "Got KEY_RESIZE, recent_sizechange is %d\n",
		    recent_sizechange));
#endif /* HAVE_SIZECHANGE || USE_SLANG */
	    if (!recent_sizechange) {
#if defined(NCURSES)
		/*
		 * Work-around for scenario (Linux libc5) where we got a
		 * recent sizechange before reading KEY_RESIZE.  If we do
		 * not reset the flag, we'll next get an EOF read, which
		 * causes Lynx to exit.
		 */
		recent_sizechange = TRUE;
#endif
		/*
		 * May be just the delayed effect of mainloop()'s call to
		 * resizeterm().  Pretend we haven't read anything yet, don't
		 * return.  - kw
		 */
		goto re_read;
	    }
	    /*
	     * Yep, we agree there was a change.  Return now so that the caller
	     * can react to it.  - kw
	     */
	    c = DO_NOTHING;
	    break;
#endif /* KEY_RESIZE */

/* The following maps PDCurses keys away from lynx reserved values */
#if (defined(_WINDOWS) || defined(__DJGPP__)) && !defined(USE_SLANG)
	case KEY_F(2):
	    c = 0x213;
	    break;
	case KEY_F(3):
	    c = 0x214;
	    break;
	case KEY_F(4):
	    c = 0x215;
	    break;
	case KEY_F(5):
	    c = 0x216;
	    break;
	case KEY_F(6):
	    c = 0x217;
	    break;
	case KEY_F(7):
	    c = 0x218;
	    break;
#endif /* PDCurses */

#if defined(USE_MOUSE)
/********************************************************************/

#if defined(NCURSES) || defined(PDCURSES)
	case KEY_MOUSE:
	    CTRACE((tfp, "KEY_MOUSE\n"));
	    if (code == FOR_CHOICE) {
		c = MOUSE_KEY;	/* Will be processed by the caller */
	    }
#if defined(NCURSES)
	    else if (code == FOR_SINGLEKEY) {
		MEVENT event;

		getmouse(&event);	/* Completely ignore event - kw */
		c = DO_NOTHING;
	    }
#endif
	    else {
#if defined(NCURSES)
		MEVENT event;
		int err;
		int lac = LYK_UNKNOWN;

		c = -1;
		mouse_link = -1;
		err = getmouse(&event);
		if (err != OK) {
		    CTRACE((tfp, "Mouse error: no event available!\n"));
		    return (code == FOR_PANEL ? 0 : DO_NOTHING);
		}
		levent = event;	/* Allow setting pos in entry fields */
		if (event.bstate & BUTTON1_CLICKED) {
		    c = set_clicked_link(event.x, event.y, code, 1);
		} else if (event.bstate & BUTTON1_DOUBLE_CLICKED) {
		    c = set_clicked_link(event.x, event.y, code, 2);
		    if (c == LAC_TO_LKC0(LYK_MOUSE_SUBMIT) &&
			code == FOR_INPUT)
			lac = LYK_MOUSE_SUBMIT;
		} else if (event.bstate & BUTTON3_CLICKED) {
		    c = LAC_TO_LKC0(LYK_PREV_DOC);
		} else if (code == FOR_PROMPT
		    /* Cannot ignore: see LYCurses.c */
			   || (event.bstate &
			       (BUTTON1_PRESSED | BUTTON1_RELEASED
				| BUTTON2_PRESSED | BUTTON2_RELEASED
				| BUTTON3_PRESSED | BUTTON3_RELEASED))) {
		    /* Completely ignore - don't return anything, to
		       avoid canceling the prompt - kw */
		    goto re_read;
		} else if (event.bstate & BUTTON2_CLICKED) {
		    int atlink;

		    c = set_clicked_link(event.x, event.y, code, 1);
		    atlink = (c == LAC_TO_LKC0(LYK_ACTIVATE));
		    if (!atlink)
			mouse_link = -1;	/* Forget about approx stuff. */

		    lac = LYmouse_menu(event.x, event.y, atlink, code);
		    if (lac == LYK_MOUSE_SUBMIT) {
			if (mouse_link == -1)
			    lac = LYK_ACTIVATE;
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
			else if (mouse_link >= 0 &&
				 textfields_need_activation &&
				 links[mouse_link].type == WWW_FORM_LINK_TYPE &&
				 F_TEXTLIKE(links[mouse_link].l_form->type))
			    lac = LYK_ACTIVATE;
#endif
		    }
		    if (lac == LYK_ACTIVATE && mouse_link == -1) {
			HTAlert(gettext("No link chosen"));
			lac = LYK_REFRESH;
		    }
		    c = LAC_TO_LKC(lac);
		}
#if NCURSES_MOUSE_VERSION > 1
		else if (event.bstate & BUTTON4_PRESSED) {
		    c = LAC_TO_LKC(LYK_UP_HALF);
		} else if (event.bstate & BUTTON5_PRESSED) {
		    c = LAC_TO_LKC(LYK_DOWN_HALF);
		}
#endif
		if (code == FOR_INPUT && mouse_link == -1 &&
		    lac != LYK_REFRESH &&
		    lac != LYK_MOUSE_SUBMIT) {
		    ungetmouse(&event);		/* Caller will process this. */
		    wgetch(LYwin);	/* ungetmouse puts KEY_MOUSE back */
		    c = MOUSE_KEY;
		}
#else /* pdcurses version */

#define H_CMD_AREA	6
#define HIST_CMD_2	12
#define V_CMD_AREA	1

		int left = H_CMD_AREA;
		int right = (LYcolLimit - H_CMD_AREA - 1);

		/* yes, I am assuming that my screen will be a certain width. */

		int tick_count;
		char *p = NULL;
		char mouse_info[128];
		static int old_click = 0;	/* [m Sec] */

		c = -1;
		mouse_link = -1;

		if (!system_is_NT) {
		    tick_count = GetTickCount();

		    /* Guard Mouse button miss click */
		    if ((tick_count - old_click) < 700) {
			c = DO_NOTHING;
			break;
		    } else {
			old_click = tick_count;
		    }
		}
		request_mouse_pos();

		if (BUTTON_STATUS(1) & BUTTON_PRESSED) {
		    if (MOUSE_Y_POS > (LYlines - V_CMD_AREA - 1)) {
			/* Screen BOTTOM */
			if (MOUSE_X_POS < left) {
			    c = LTARROW_KEY;
			    p = "<-";
			} else if (MOUSE_X_POS < HIST_CMD_2) {
			    c = RTARROW_KEY;
			    p = "->";
			} else if (MOUSE_X_POS > right) {
			    c = 'z';
			    p = "Cancel";
			} else {
			    c = PGDOWN_KEY;
			    p = "PGDOWN";
			}
		    } else if (MOUSE_Y_POS < V_CMD_AREA) {
			/* Screen TOP */
			if (MOUSE_X_POS < left) {
			    c = LTARROW_KEY;
			    p = "<-";
			} else if (MOUSE_X_POS < HIST_CMD_2) {
			    c = RTARROW_KEY;
			    p = "->";
			} else if (MOUSE_X_POS > right) {
			    c = 'z';
			    p = "Cancel";
			} else {
			    c = PGUP_KEY;
			    p = "PGUP";
			}
		    } else {
			c = set_clicked_link(MOUSE_X_POS,
					     MOUSE_Y_POS,
					     FOR_PANEL, 1);
		    }
		}
		if (p && c != -1) {
		    sprintf(mouse_info, "Mouse = 0x%x, [%s]", c, p);
		    SetConsoleTitle(mouse_info);
		}
#endif /* !(WIN_EX) */
		if ((c + 1) >= KEYMAP_SIZE && (c & LKC_ISLAC))
		    return (c);
	    }
	    break;
#endif /* NCURSES || PDCURSES */

/********************************************************************/
#endif /* USE_MOUSE */

	}
#endif /* HAVE_KEYPAD */
#ifdef DJGPP_KEYHANDLER
	switch (c) {
	case K_Down:		/* The four arrow keys ... */
	case K_EDown:
	    c = DNARROW_KEY;
	    break;
	case K_Up:
	case K_EUp:
	    c = UPARROW_KEY;
	    break;
	case K_Left:
	case K_ELeft:
	    c = LTARROW_KEY;
	    break;
	case K_Right:		/* ... */
	case K_ERight:
	    c = RTARROW_KEY;
	    break;
	case K_Home:		/* Home key (upward+left arrow) */
	case K_EHome:
	    c = HOME_KEY;
	    break;
	case K_PageDown:	/* Next page */
	case K_EPageDown:
	    c = PGDOWN_KEY;
	    break;
	case K_PageUp:		/* Previous page */
	case K_EPageUp:
	    c = PGUP_KEY;
	    break;
	case K_End:		/* home down or bottom (lower left) */
	case K_EEnd:
	    c = END_KEY;
	    break;
	case K_F1:		/* F1 key */
	    c = F1_KEY;
	    break;
	case K_Insert:		/* Insert key */
	case K_EInsert:
	    c = INSERT_KEY;
	    break;
	case K_Delete:		/* Delete key */
	case K_EDelete:
	    c = REMOVE_KEY;
	    break;
	case K_Alt_Escape:	/* Alt-Escape */
	    c = 0x1a7;
	    break;
	case K_Control_At:	/* CTRL-@ */
	    c = 0x1a8;
	    break;
	case K_Alt_Backspace:	/* Alt-Backspace */
	    c = 0x1a9;
	    break;
	case K_BackTab:	/* BackTab */
	    c = BACKTAB_KEY;
	    break;
	}
#endif /* DGJPP_KEYHANDLER */
#if defined(USE_SLANG) && (defined(__DJGPP__) || defined(__CYGWIN__)) && !defined(DJGPP_KEYHANDLER)  && !defined(USE_KEYMAPS)
	switch (c) {
	case SL_KEY_DOWN:	/* The four arrow keys ... */
	    c = DNARROW_KEY;
	    break;
	case SL_KEY_UP:
	    c = UPARROW_KEY;
	    break;
	case SL_KEY_LEFT:
	    c = LTARROW_KEY;
	    break;
	case SL_KEY_RIGHT:	/* ... */
	    c = RTARROW_KEY;
	    break;
	case SL_KEY_HOME:	/* Home key (upward+left arrow) */
	case SL_KEY_A1:	/* upper left of keypad */
	    c = HOME_KEY;
	    break;
	case SL_KEY_NPAGE:	/* Next page */
	case SL_KEY_C3:	/* lower right of keypad */
	    c = PGDOWN_KEY;
	    break;
	case SL_KEY_PPAGE:	/* Previous page */
	case SL_KEY_A3:	/* upper right of keypad */
	    c = PGUP_KEY;
	    break;
	case SL_KEY_END:	/* home down or bottom (lower left) */
	case SL_KEY_C1:	/* lower left of keypad */
	    c = END_KEY;
	    break;
	case SL_KEY_F(1):	/* F1 key */
	    c = F1_KEY;
	    break;
	case SL_KEY_IC:	/* Insert key */
	    c = INSERT_KEY;
	    break;
	case SL_KEY_DELETE:	/* Delete key */
	    c = REMOVE_KEY;
	    break;
	}
#endif /* USE_SLANG && __DJGPP__ && !DJGPP_KEYHANDLER && !USE_KEYMAPS */
    }

    if (c & (LKC_ISLAC | LKC_ISLECLAC)) {
	return (c);
    } else if ((c + 1) >= KEYMAP_SIZE) {
	/*
	 * Don't return raw values for KEYPAD symbols which we may have missed
	 * in the switch above if they are obviously invalid when used as an
	 * index into (e.g.) keypad[].  - KW
	 */
	return (0);
    } else {
	return (c | current_modifier);
    }
}

/************************************************************************/
#endif /* NOT  defined(USE_KEYMAPS) && defined(USE_SLANG) */

int LYgetch(void)
{
    return LYReadCmdKey(FOR_PANEL);
}

/*
 * Read a single keystroke, allows mouse-selection.
 */
int LYgetch_choice(void)
{
    int ch = LYReadCmdKey(FOR_CHOICE);

    if (ch == LYCharINTERRUPT1)
	ch = LYCharINTERRUPT2;	/* treat ^C the same as ^G */
    return ch;
}

/*
 * Read a single keystroke, allows mouse events.
 */
int LYgetch_input(void)
{
    int ch = LYReadCmdKey(FOR_INPUT);

    if (ch == LYCharINTERRUPT1)
	ch = LYCharINTERRUPT2;	/* treat ^C the same as ^G */
    return ch;
}

/*
 * Read a single keystroke, ignoring case by translating it to uppercase.
 * Ignore mouse events, if any.
 */
int LYgetch_single(void)
{
    int ch = LYReadCmdKey(FOR_SINGLEKEY);

    if (ch == LYCharINTERRUPT1)
	ch = LYCharINTERRUPT2;	/* treat ^C the same as ^G */
    else if (ch > 0 && ch < 256)
	ch = TOUPPER(ch);	/* will ignore case of result */
    return ch;
}

/*
 * Convert a null-terminated string to lowercase
 */
void LYLowerCase(char *arg_buffer)
{
    register unsigned char *buffer = (unsigned char *) arg_buffer;
    size_t i;

    for (i = 0; buffer[i]; i++) {
#ifdef SUPPORT_MULTIBYTE_EDIT	/* 1998/11/23 (Mon) 17:04:55 */
	if ((buffer[i] & 0x80) != 0
	    && buffer[i + 1] != 0) {
	    if ((kanji_code == SJIS) && IS_SJIS_X0201KANA(UCH((buffer[i])))) {
		continue;
	    }
	    i++;
	} else {
	    buffer[i] = UCH(TOLOWER(buffer[i]));
	}
#else
	buffer[i] = TOLOWER(buffer[i]);
#endif
    }
}

/*
 * Convert a null-terminated string to uppercase
 */
void LYUpperCase(char *arg_buffer)
{
    register unsigned char *buffer = (unsigned char *) arg_buffer;
    size_t i;

    for (i = 0; buffer[i]; i++) {
#ifdef SUPPORT_MULTIBYTE_EDIT	/* 1998/11/23 (Mon) 17:05:10 */
	if ((buffer[i] & 0x80) != 0
	    && buffer[i + 1] != 0) {
	    if ((kanji_code == SJIS) && IS_SJIS_X0201KANA(UCH((buffer[i])))) {
		continue;
	    }
	    i++;
	} else {
	    buffer[i] = UCH(TOUPPER(buffer[i]));
	}
#else
	buffer[i] = UCH(TOUPPER(buffer[i]));
#endif
    }
}

/*
 * Remove newlines from a string, returning true if we removed any.
 */
BOOLEAN LYRemoveNewlines(char *buffer)
{
    BOOLEAN result = FALSE;

    if (buffer != 0) {
	register char *buf = buffer;

	for (; *buf && *buf != '\n' && *buf != '\r'; buf++) ;
	if (*buf) {
	    /* runs very seldom */
	    char *old = buf;

	    for (; *old; old++) {
		if (*old != '\n' && *old != '\r')
		    *buf++ = *old;
	    }
	    *buf = '\0';
	    result = TRUE;
	}
    }
    return result;
}

/*
 * Remove leading/trailing whitespace from a string, reduce runs of embedded
 * whitespace to single blanks.
 */
char *LYReduceBlanks(char *buffer)
{
    if (non_empty(buffer)) {
	LYTrimLeading(buffer);
	LYTrimTrailing(buffer);
	convert_to_spaces(buffer, TRUE);
    }
    return buffer;
}

/*
 * Remove ALL whitespace from a string (including embedded blanks), and returns
 * a pointer to the end of the trimmed string.
 */
char *LYRemoveBlanks(char *buffer)
{
    char *result = NULL;

    if (buffer != 0) {
	register char *buf = buffer;

	for (; *buf && !isspace(UCH(*buf)); buf++) ;
	if (*buf) {
	    /* runs very seldom */
	    char *old = buf;

	    for (; *old; old++) {
		if (!isspace(UCH(*old)))
		    *buf++ = *old;
	    }
	    *buf = '\0';
	}
	result = buf;
    }
    return result;
}

/*
 * Skip whitespace
 */
char *LYSkipBlanks(char *buffer)
{
    while (isspace(UCH((*buffer))))
	buffer++;
    return buffer;
}

/*
 * Skip non-whitespace
 */
char *LYSkipNonBlanks(char *buffer)
{
    while (*buffer != 0 && !isspace(UCH((*buffer))))
	buffer++;
    return buffer;
}

/*
 * Skip const whitespace
 */
const char *LYSkipCBlanks(const char *buffer)
{
    while (isspace(UCH((*buffer))))
	buffer++;
    return buffer;
}

/*
 * Skip const non-whitespace
 */
const char *LYSkipCNonBlanks(const char *buffer)
{
    while (*buffer != 0 && !isspace(UCH((*buffer))))
	buffer++;
    return buffer;
}

/*
 * Trim leading blanks from a string
 */
void LYTrimLeading(char *buffer)
{
    char *skipped = LYSkipBlanks(buffer);

    while ((*buffer++ = *skipped++) != 0) ;
}

/*
 * Trim trailing newline(s) from a string
 */
char *LYTrimNewline(char *buffer)
{
    size_t i = strlen(buffer);

    while (i != 0 && (buffer[i - 1] == '\n' || buffer[i - 1] == '\r'))
	buffer[--i] = 0;
    return buffer;
}

/*
 * Trim trailing blanks from a string
 */
void LYTrimTrailing(char *buffer)
{
    size_t i = strlen(buffer);

    while (i != 0 && isspace(UCH(buffer[i - 1])))
	buffer[--i] = 0;
}

/* 1997/11/10 (Mon) 14:26:10, originally string_short() in LYExterns.c, but
 * moved here because LYExterns is not always configured.
 */
char *LYElideString(char *str,
		    int cut_pos)
{
    char buff[MAX_LINE], *s, *d;
    static char s_str[MAX_LINE];
    int len;

    LYStrNCpy(buff, str, sizeof(buff) - 1);
    len = (int) strlen(buff);
    if (len > (LYcolLimit - 9)) {
	buff[cut_pos] = '.';
	buff[cut_pos + 1] = '.';
	for (s = (buff + len) - (LYcolLimit - 9) + cut_pos + 1,
	     d = (buff + cut_pos) + 2;
	     s >= buff &&
	     d >= buff &&
	     d < buff + LYcols &&
	     (*d++ = *s++) != 0;) ;
	buff[LYcols] = 0;
    }
    strcpy(s_str, buff);
    return (s_str);
}

/*
 * Trim a startfile, returning true if it looks like one of the Lynx tags.
 */
BOOLEAN LYTrimStartfile(char *buffer)
{
    BOOLEAN result = FALSE;

    LYTrimHead(buffer);
    if (isLYNXEXEC(buffer) ||
	isLYNXPROG(buffer)) {
	/*
	 * The original implementations of these schemes expected white space
	 * without hex escaping, and did not check for hex escaping, so we'll
	 * continue to support that, until that code is redone in conformance
	 * with SGML principles.  - FM
	 */
	HTUnEscapeSome(buffer, " \r\n\t");
	convert_to_spaces(buffer, TRUE);
	result = TRUE;
    }
    return result;
}

/*
 * Escape unsafe characters in startfile, except for lynx internal URLs.
 */
void LYEscapeStartfile(char **buffer)
{
    if (!LYTrimStartfile(*buffer)) {
	char *escaped = HTEscapeUnsafe(*buffer);

	StrAllocCopy(*buffer, escaped);
	FREE(escaped);
    }
}

/*
 * Trim all blanks from startfile, except for lynx internal URLs.
 */
void LYTrimAllStartfile(char *buffer)
{
    if (!LYTrimStartfile(buffer)) {
	LYRemoveBlanks(buffer);
    }
}

/*
 * Display the current value of the string and allow the user to edit it.
 */

/*
 * Shorthand to get rid of the "edit->suchandsos".
 */
#define IsDirty   edit->efIsDirty
#define IsHidden  edit->efIsMasked
#define StartX    edit->efStartX
#define StartY    edit->efStartY
#define Buffer    edit->efBuffer
#define EditAt    edit->efEditAt	/* current editing position (bytes) */
#define BufInUse  edit->efBufInUse	/* length (bytes) */
#define BufAlloc  edit->efBufAlloc
#define BufLimit  edit->efBufLimit
#define DpyWidth  edit->efWidth
#define DpyStart  edit->efDpyStart	/* display-start (columns) */
#define PanMargin edit->efPanMargin
#define IsPanned  edit->efIsPanned
#define PadChar   edit->efPadChar
#ifdef ENHANCED_LINEEDIT
#define EditMark  edit->efEditMark
#endif
#define InputMods edit->efInputMods
#define Offs2Col  edit->efOffs2Col

#define enableEditMark() \
	if (EditMark < 0) \
	    EditMark = -(1 + EditMark)

#define disableEditMark() \
	if (EditMark >= 0) \
	    EditMark = -(1 + EditMark)

#ifdef ENHANCED_LINEEDIT
static bstring *killbuffer;
#endif

static void updateMargin(FieldEditor * edit)
{
    if ((int) BufAlloc > DpyWidth) {	/* Need panning? */
	if (DpyWidth > 4)
	    IsPanned = TRUE;

	/*
	 * Figure out margins.  If too big, we do a lot of unnecessary
	 * scrolling.  If too small, user doesn't have sufficient look-ahead. 
	 * Let's say 25% for each margin, upper bound is 10 columns.
	 */
	PanMargin = DpyWidth / 4;
	if (PanMargin > 10)
	    PanMargin = 10;
    }
}

/*
 * Before using an array position, make sure that the array is long enough.
 * Reallocate if needed.
 */
static void ExtendEditor(FieldEditor * edit, int position)
{
    size_t need = (size_t) (++position);

    if (need >= BufAlloc && (BufLimit == 0 || need < BufLimit)) {
	CTRACE((tfp, "ExtendEditor from %lu to %lu\n",
		(unsigned long) BufAlloc,
		(unsigned long) need));
	Buffer = typeRealloc(char, Buffer, need);
	Offs2Col = typeRealloc(int, Offs2Col, need + 1);

	BufAlloc = need;
	updateMargin(edit);
    }
}

void LYFinishEdit(FieldEditor * edit)
{
    CTRACE((tfp, "LYFinishEdit:%s\n", NonNull(Buffer)));

    FREE(Buffer);
    FREE(Offs2Col);
}

void LYSetupEdit(FieldEditor * edit, char *old_value, size_t buffer_limit, int display_limit)
{
    CTRACE((tfp, "LYSetupEdit buffer %lu, display %d:%s\n",
	    (unsigned long) buffer_limit,
	    display_limit,
	    old_value));

    BufLimit = buffer_limit;
    if (buffer_limit == 0)
	buffer_limit = strlen(old_value) + 1;

    /*
     * Initialize edit record
     */
    LYGetYX(StartY, StartX);
    PadChar = ' ';
    IsDirty = TRUE;
    IsPanned = FALSE;
    InputMods = 0;

    BufAlloc = buffer_limit;
    DpyWidth = display_limit;
    PanMargin = 0;
    EditAt = (int) strlen(old_value);
#ifdef ENHANCED_LINEEDIT
    EditMark = -1;		/* pos=0, but do not show it yet */
#endif
    DpyStart = 0;

    updateMargin(edit);

    BufInUse = strlen(old_value);
    Buffer = typecallocn(char, BufAlloc + 1);

    if (Buffer == 0)
	outofmem(__FILE__, "LYSetupEdit");

    LYStrNCpy(Buffer, old_value, buffer_limit);
    Offs2Col = typecallocn(int, BufAlloc + 1);

    if (Offs2Col == 0)
	outofmem(__FILE__, "LYSetupEdit");
}

#ifdef SUPPORT_MULTIBYTE_EDIT

/*
 * MBCS positioning routines below are specific to SUPPORT_MULTIBYTE_EDIT code.
 * Currently they handle UTF-8 and (hopefully) CJK.
 * Current encoding is recognized using defines below.
 *
 * LYmbcs* functions don't look very convenient to use here...
 * Do we really need utf_flag as an argument?
 *
 * It is set (see IS_UTF8_TTY) for every invocation out there, and they use
 * HTCJK flag internally anyway.  Something like LYmbcsstrnlen == mbcs_glyphs
 * would be useful to work with string slices -Sergej Kvachonok 
 */

#define IS_UTF8_EXTRA(x) (((unsigned char)(x) & 0300) == 0200)

/*
 * Counts glyphs in a multibyte (sub)string s of length len.
 */
static int mbcs_glyphs(char *s, int len)
{
    int glyphs = 0;
    int i;

    if (IS_UTF8_TTY) {
	for (i = 0; s[i] && i < len; i++)
	    if (!IS_UTF8_EXTRA(s[i]))
		glyphs++;
    } else if (IS_CJK_TTY) {
	for (i = 0; s[i] && i < len; i++, glyphs++)
	    if (is8bits(s[i]))
		i++;
    } else {
	glyphs = len;
    }
    return glyphs;
}

/*
 * Calculates offset in bytes of a glyph at cell position pos.
 */
static int mbcs_skip(char *s, int pos)
{
    int p, i;

    if (IS_UTF8_TTY) {
	for (i = 0, p = 0; s[i]; i++) {
	    if (!IS_UTF8_EXTRA(s[i]))
		p++;
	    if (p > pos)
		break;
	}
    } else if (IS_CJK_TTY) {
	for (p = i = 0; s[i] && p < pos; p++, i++)
	    if (is8bits(s[i]))
		i++;
    } else {
	i = pos;
    }

    return i;
}

/*
 * Given a string that would display (at least) the given number of cells,
 * determine the number of multibyte characters that comprised those cells.
 */
static int cell2char(char *s, int cells)
{
    int result = 0;
    int len = (int) strlen(s);
    int pos;
    int have;

    CTRACE_EDIT((tfp, "cell2char(%d) %d:%s\n", cells, len, s));
    /* FIXME - make this a binary search */
    if (len != 0) {
	for (pos = 0; pos <= len; ++pos) {
	    have = LYstrExtent2(s, pos);
	    CTRACE_EDIT((tfp, "  %2d:%2d:%.*s\n", pos, have, pos, s));
	    if (have >= cells) {
		break;
	    }
	}
	if (pos > len)
	    pos = len;
    } else {
	pos = 0;
    }
    result = mbcs_glyphs(s, pos);
    CTRACE_EDIT((tfp, "->%d\n", result));
    return result;
}

#endif /* SUPPORT_MULTIBYTE_EDIT */

#ifdef EXP_KEYBOARD_LAYOUT
static int map_active = 0;

#else
#define map_active 0
#endif

int LYEditInsert(FieldEditor * edit, unsigned const char *s,
		 int len,
		 int map GCC_UNUSED,
		 int maxMessage)
{
    int length = (int) strlen(Buffer);
    int remains = (int) BufAlloc - (length + len);
    int edited = 0, overflow = 0;

    /*
     * ch is (presumably) printable character.
     */
    if (remains < 0) {
	overflow = 1;
	len = 0;
	if ((int) BufAlloc > length)	/* Insert as much as we can */
	    len = (int) BufAlloc - length;
	else
	    goto finish;
    }
    ExtendEditor(edit, length + len);
    Buffer[length + len] = '\0';
    for (; length >= EditAt; length--)	/* Make room */
	Buffer[length + len] = Buffer[length];
#ifdef EXP_KEYBOARD_LAYOUT
    if (map < 0)
	map = map_active;
    if (map && IS_UTF8_TTY) {
	int off = EditAt;
	unsigned const char *e = s + len;
	char *tail = 0;

	while (s < e) {
	    char utfbuf[8];
	    int l = 1;

	    utfbuf[0] = (char) *s;
	    if (*s < 128 && LYKbLayouts[current_layout][*s]) {
		UCode_t ucode = LYKbLayouts[current_layout][*s];

		if (ucode > 127) {
		    if (UCConvertUniToUtf8(ucode, utfbuf)) {
			l = (int) strlen(utfbuf);
			remains -= l - 1;
			if (remains < 0) {
			    if (tail)
				strcpy(Buffer + off, tail);
			    FREE(tail);
			    len = off;
			    overflow = 1;
			    goto finish;
			}
			if (l > 1 && !tail)
			    StrAllocCopy(tail, Buffer + EditAt + len);
		    } else
			utfbuf[0] = '?';
		} else
		    utfbuf[0] = (char) ucode;
	    }
	    StrNCpy(Buffer + off, utfbuf, l);
	    edited = 1;
	    off += l;
	    s++;
	}
	if (tail)
	    strcpy(Buffer + off, tail);
	len = off - EditAt;
	FREE(tail);
    } else if (map) {
	unsigned const char *e = s + len;
	unsigned char *t = (unsigned char *) Buffer + EditAt;

	while (s < e) {
	    int ch;

	    if (*s < 128 && LYKbLayouts[current_layout][*s]) {
		ch = UCTransUniChar((UCode_t) LYKbLayouts[current_layout][*s],
				    current_char_set);
		if (ch < 0)
		    ch = '?';
	    } else
		ch = *s;
	    *t = UCH(ch);
	    t++, s++;
	}
	edited = 1;
    } else
#endif /* defined EXP_KEYBOARD_LAYOUT */
    {
	StrNCpy(Buffer + EditAt, (const char *) s, len);
	edited = 1;
    }

  finish:
    EditAt += len;
    BufInUse += (size_t) len;
    if (edited)
	IsDirty = TRUE;
    if (overflow && maxMessage)
	_statusline(MAXLEN_REACHED_DEL_OR_MOV);
#ifdef ENHANCED_LINEEDIT
    if (EditMark > EditAt)
	EditMark += len;
    else if (EditMark < -(1 + EditAt))
	EditMark -= len;
    disableEditMark();
#endif
    return edited;
}

/*
 * Do one edit-operation, given the input 'ch' and working buffer 'edit'.
 *
 * If the input is processed, returns zero.
 * If the action should be performed outside of line-editing mode, return -ch.
 * Otherwise, e.g., returns 'ch'.
 */
int LYDoEdit(FieldEditor * edit, int ch,
	     int action,
	     int maxMessage)
{
    int i;
    int length;
    unsigned char uch;
    int offset;

    if ((int) BufAlloc <= 0)
	return (0);		/* Be defensive */

    BufInUse = strlen(&Buffer[0]);
    length = (int) BufInUse;

    switch (action) {
#ifdef EXP_KEYBOARD_LAYOUT
    case LYE_SWMAP:
	/*
	 * Turn input character mapping on or off.
	 */
	map_active = ~map_active;
	break;
#endif
#ifndef CJK_EX
    case LYE_AIX:
	/*
	 * Handle CJK characters, or as a valid character in the current
	 * display character set.  Otherwise, we treat this as LYE_ENTER.
	 */
	if (!IS_CJK_TTY && LYlowest_eightbit[current_char_set] > 0x97)
	    return (ch);
	/* FALLTHRU */
#endif
    case LYE_CHAR:
	uch = UCH(ch);
	LYEditInsert(edit, &uch, 1, map_active, maxMessage);
	return 0;		/* All changes already registered */

    case LYE_C1CHAR:
	/*
	 * ch is the second part (in most cases, a capital letter) of a 7-bit
	 * replacement for a character in the 8-bit C1 control range.
	 *
	 * This is meant to undo transformations like 0x81 -> 0x1b 0x41 (ESC A)
	 * etc., done by slang on Unix and possibly some comm programs.  It's
	 * an imperfect workaround that doesn't work for all such characters.
	 */
	ch &= 0xFF;
	if (ch + 64 >= LYlowest_eightbit[current_char_set])
	    ch += 64;

	if (EditAt <= ((int) BufAlloc) && BufInUse < BufAlloc) {
#ifdef ENHANCED_LINEEDIT
	    if (EditMark > EditAt)
		EditMark++;
	    else if (EditMark < -(1 + EditAt))
		EditMark--;
	    disableEditMark();
#endif
	    ExtendEditor(edit, length + 1);
	    for (i = length; i >= EditAt; i--)	/* Make room */
		Buffer[i + 1] = Buffer[i];
	    Buffer[length + 1] = '\0';
	    Buffer[EditAt] = (char) ch;
	    EditAt++;
	} else {
	    if (maxMessage) {
		_statusline(MAXLEN_REACHED_DEL_OR_MOV);
	    }
	    return (ch);
	}
	break;

    case LYE_BACKW:		/* go backward one word */
	while (EditAt && !IsWordChar(Buffer[EditAt - 1]))
	    EditAt--;
	while (EditAt && IsWordChar(UCH(Buffer[EditAt - 1])))
	    EditAt--;
	break;

    case LYE_FORWW:		/* go forward one word */
	while (IsWordChar(UCH(Buffer[EditAt])))
	    EditAt++;
	while (!IsWordChar(Buffer[EditAt]) && Buffer[EditAt])
	    EditAt++;
	break;

    case LYE_ERASE:		/* erase the line */
	Buffer[0] = '\0';
#ifdef ENHANCED_LINEEDIT
	EditMark = -1;		/* Do not show the mark */
#endif
	/* FALLTHRU */

    case LYE_BOL:		/* go to beginning of line  */
	EditAt = 0;
	break;

    case LYE_EOL:		/* go to end of line  */
	EditAt = length;
	break;

    case LYE_DELNW:		/* delete next word  */
	offset = EditAt;
	LYDoEdit(edit, 0, LYE_FORWW, FALSE);
	offset = EditAt - offset;
	EditAt -= offset;

	goto shrink;		/* right below */

    case LYE_DELPW:		/* delete previous word  */
	offset = EditAt;
	LYDoEdit(edit, 0, LYE_BACKW, FALSE);
	offset -= EditAt;

      shrink:
	for (i = EditAt; i < length - offset + 1; i++)
	    Buffer[i] = Buffer[i + offset];
#ifdef ENHANCED_LINEEDIT
	disableEditMark();
	if (EditMark <= -(1 + EditAt + offset))
	    EditMark += offset;	/* Shift it */
	if (-(1 + EditAt + offset) < EditMark && EditMark < -(1 + EditAt))
	    EditMark = -(1 + EditAt);	/* Set to the current position */
#endif

	break;

    case LYE_DELBL:		/* delete from cursor to beginning of line */
	for (i = EditAt; i < length + 1; i++)
	    Buffer[i - EditAt] = Buffer[i];

#ifdef ENHANCED_LINEEDIT
	disableEditMark();
	if (EditMark <= -(1 + EditAt))
	    EditMark += EditAt;	/* Shift it */
	else
	    EditMark = -1;	/* Reset it */
#endif
	EditAt = 0;
	break;

    case LYE_DELEL:		/* delete from cursor to end of line */
	Buffer[EditAt] = '\0';
#ifdef ENHANCED_LINEEDIT
	disableEditMark();
	if (EditMark <= -(1 + EditAt))
	    EditMark = -1;	/* Reset it */
#endif
	break;

    case LYE_DELN:		/* delete next character */
	if (EditAt >= length)
	    break;
#ifndef SUPPORT_MULTIBYTE_EDIT
	EditAt++;
#else
	EditAt += mbcs_skip(Buffer + EditAt, 1);
#endif
	/* FALLTHRU */

    case LYE_DELP:		/* delete previous character */
	if (length == 0 || EditAt == 0)
	    break;

#ifndef SUPPORT_MULTIBYTE_EDIT
#ifdef ENHANCED_LINEEDIT
	disableEditMark();
	if (EditMark <= -(1 + EditAt))
	    EditMark++;
#endif
	EditAt--;
	for (i = EditAt; i < length; i++)
	    Buffer[i] = Buffer[i + 1];
#else /* SUPPORT_MULTIBYTE_EDIT */
	offset = EditAt - mbcs_skip(Buffer, mbcs_glyphs(Buffer, EditAt) - 1);
	EditAt -= offset;
	for (i = EditAt; i < length - offset + 1; i++)
	    Buffer[i] = Buffer[i + offset];

#ifdef ENHANCED_LINEEDIT
	disableEditMark();
	if (EditMark <= -(1 + EditAt))
	    EditMark += offset;	/* Shift it */
#endif

#endif /* SUPPORT_MULTIBYTE_EDIT */
	break;

    case LYE_FORW_RL:
    case LYE_FORW:		/* move cursor forward */
#ifndef SUPPORT_MULTIBYTE_EDIT
	if (EditAt < length)
	    EditAt++;
#else
	if (EditAt < length)
	    EditAt += mbcs_skip(Buffer + EditAt, 1);
#endif
	else if (action == LYE_FORW_RL)
	    return -ch;
	break;

    case LYE_BACK_LL:
    case LYE_BACK:		/* move cursor backward */
#ifndef SUPPORT_MULTIBYTE_EDIT
	if (EditAt > 0)
	    EditAt--;
#else
	if (EditAt > 0)
	    EditAt = mbcs_skip(Buffer, mbcs_glyphs(Buffer, EditAt) - 1);
#endif
	else if (action == LYE_BACK_LL)
	    return -ch;
	break;

#ifdef ENHANCED_LINEEDIT
    case LYE_TPOS:
	/*
	 * Transpose characters - bash or ksh(emacs not gmacs) style
	 */

#ifdef SUPPORT_MULTIBYTE_EDIT
	if (IS_UTF8_TTY || IS_CJK_TTY)
	    break;		/* Can't help it now */
#endif

	if (length <= 1 || EditAt == 0)
	    return (ch);
	if (EditAt == length)
	    EditAt--;
	enableEditMark();
	if (EditMark == EditAt || EditMark == EditAt + 1)
	    EditMark = EditAt - 1;
	disableEditMark();
	if (Buffer[EditAt - 1] == Buffer[EditAt]) {
	    EditAt++;
	    break;
	}
	i = Buffer[EditAt - 1];
	Buffer[EditAt - 1] = Buffer[EditAt];
	Buffer[EditAt++] = (char) i;
	break;

    case LYE_SETMARK:		/* Emacs-like set-mark-command */
	EditMark = EditAt;
	return (0);

    case LYE_XPMARK:		/* Emacs-like exchange-point-and-mark */
	enableEditMark();
	if (EditMark == EditAt)
	    return (0);
	i = EditAt;
	EditAt = EditMark;
	EditMark = i;
	break;

    case LYE_KILLREG:		/* Emacs-like kill-region */
	enableEditMark();
	if (EditMark == EditAt) {
	    BStrFree(killbuffer);
	    return (0);
	}
	if (EditMark > EditAt)
	    LYDoEdit(edit, 0, LYE_XPMARK, FALSE);
	{
	    int reglen = EditAt - EditMark;

	    BStrCopy1(killbuffer, Buffer + EditMark, reglen);
	    for (i = EditMark; Buffer[i + reglen]; i++)
		Buffer[i] = Buffer[i + reglen];
	    Buffer[i] = Buffer[i + reglen];	/* terminate */
	    EditAt = EditMark;
	}
	disableEditMark();
	break;

    case LYE_YANK:		/* Emacs-like yank */
	if (!killbuffer) {
	    EditMark = -(1 + EditAt);
	    return (0);
	} else {
	    int yanklen = killbuffer->len;

	    if ((EditAt + yanklen) <= (int) BufAlloc &&
		BufInUse + (size_t) yanklen <= BufAlloc) {

		ExtendEditor(edit, EditAt + yanklen);

		EditMark = -(1 + EditAt);

		for (i = length; i >= EditAt; i--)	/* Make room */
		    Buffer[i + yanklen] = Buffer[i];
		for (i = 0; i < yanklen; i++)
		    Buffer[EditAt++] = killbuffer->str[i];

	    } else if (maxMessage) {
		_statusline(MAXLEN_REACHED_DEL_OR_MOV);
	    }
	}
	break;

#endif /* ENHANCED_LINEEDIT */

    case LYE_UPPER:
	LYUpperCase(Buffer);
	break;

    case LYE_LOWER:
	LYLowerCase(Buffer);
	break;

    default:
	return (ch);
    }
    IsDirty = TRUE;
    BufInUse = strlen(&Buffer[0]);
    return (0);
}

/*
 *  This function prompts for a choice or page number.
 *  If a 'g' or 'p' suffix is included, that will be
 *  loaded into c.  Otherwise, c is zeroed. - FM & LE
 */
int get_popup_number(const char *msg,
		     int *c,
		     int *rel)
{
    bstring *temp = NULL;
    int result = 0;

    /*
     * Load the c argument into the prompt buffer.
     */
    BStrCopy0(temp, "?");
    temp->str[0] = (char) *c;

    _statusline(msg);

    /*
     * Get the number, possibly with a suffix, from the user.
     */
    if (LYgetBString(&temp, FALSE, 0, NORECALL) < 0 || isBEmpty(temp)) {
	HTInfoMsg(CANCELLED);
	*c = '\0';
	*rel = '\0';
    } else {
	char *p = temp->str;

	*rel = '\0';
	result = atoi(p);
	while (isdigit(UCH(*p)))
	    ++p;
	switch (*p) {
	case '+':
	case '-':
	    /* 123+ or 123- */
	    *rel = *p++;
	    *c = *p;
	    break;
	default:
	    *c = *p++;
	    *rel = *p;
	    break;
	case 0:
	    break;
	}

	/*
	 * If we had a 'g' or 'p' suffix, load it into c.  Otherwise, zero c.  Then
	 * return the number.
	 */
	if (*p == 'g' || *p == 'G') {
	    *c = 'g';
	} else if (*p == 'p' || *p == 'P') {
	    *c = 'p';
	} else {
	    *c = '\0';
	}
	if (*rel != '+' && *rel != '-')
	    *rel = 0;
    }
    BStrFree(temp);
    return result;
}

#ifdef USE_COLOR_STYLE
#  define TmpStyleOn(s)		curses_style((s), STACK_ON)
#  define TmpStyleOff(s)	curses_style((s), STACK_OFF)
#else
#  define TmpStyleOn(s)
#  define TmpStyleOff(s)
#endif /* defined USE_COLOR_STYLE */

static void remember_column(FieldEditor * edit, int offset)
{
    int y0, x0;

#if defined(USE_SLANG)
    y0 = 0;
    x0 = SLsmg_get_column();
#elif defined(USE_CURSES_PADS)
    getyx(LYwin, y0, x0);
#else
    getyx(stdscr, y0, x0);
#endif
    Offs2Col[offset] = x0;

    (void) y0;
    (void) x0;
}

static void fill_edited_line(int prompting GCC_UNUSED, int length, int ch)
{
    int i;

    TmpStyleOn(prompting ? s_prompt_edit_pad : s_aedit_pad);

    for (i = 0; i < length; i++) {
	LYaddch(UCH(ch));
    }

    TmpStyleOff(prompting ? s_prompt_edit_pad : s_aedit_pad);
}

/*
 * Multibyte string display subroutine.
 * FieldEditor fields retain their values as byte offsets.
 * All external logic still works fine with byte values.
 */
void LYRefreshEdit(FieldEditor * edit)
{
    /* bytes and characters are not the same thing */
#if defined(DEBUG_EDIT)
    int all_bytes;
#endif
    int pos_bytes = EditAt;
    int dpy_bytes;
    int lft_bytes;		/* base of string which is displayed */

    /* cells refer to display-columns on the screen */
    int all_cells;		/* total of display-cells in Buffer */
    int dpy_cells;		/* number of cells which are displayed */
    int lft_cells;		/* number of cells before display (on left) */
    int pos_cells;		/* number of display-cells up to EditAt */

#if defined(SUPPORT_MULTIBYTE_EDIT)
    int dpy_chars;
    int lft_chars;

#if defined(DEBUG_EDIT)
    int all_chars;
    int pos_chars;
#endif
#endif

    /* other data */
    int i;
    int padsize;
    char *str;
    int lft_shift = 0;
    int rgt_shift = 0;

#ifdef USE_COLOR_STYLE
    int estyle;
#endif
    int prompting = 0;

    (void) pos_bytes;

    /*
     * If we've made no changes, or if there is nothing to display, just leave.
     */
    if (!IsDirty || (DpyWidth == 0))
	return;

    CTRACE((tfp, "LYRefreshEdit:%s\n", Buffer));

    IsDirty = FALSE;

    BufInUse = strlen(&Buffer[0]);

    all_cells = LYstrCells(Buffer);
    pos_cells = LYstrExtent2(Buffer, EditAt);

#if defined(SUPPORT_MULTIBYTE_EDIT) && defined(DEBUG_EDIT)
    all_bytes = (int) BufInUse;
    lft_chars = mbcs_glyphs(Buffer, DpyStart);
    pos_chars = mbcs_glyphs(Buffer, EditAt);
    all_chars = mbcs_glyphs(Buffer, all_bytes);
#endif

    /*
     * Now we have:
     *                .--DpyWidth--.
     *      +---------+=============+-----------+
     *      |         |M           M|           |   (M=PanMargin)
     *      +---------+=============+-----------+
     *      0         DpyStart                   BufInUse
     *
     * Insertion point can be anywhere between 0 and stringlength.  Calculate
     * a new display starting point.
     *
     * First, make Lynx scroll several columns at a time as needed when
     * extending the string.   Doing this helps with lowspeed connections.
     */

    lft_bytes = DpyStart;
    lft_cells = LYstrExtent2(Buffer, DpyStart);

    if ((lft_cells + DpyWidth) <= all_cells) {
	if (pos_cells >= (lft_cells + DpyWidth) - PanMargin) {
	    lft_cells = (pos_cells - DpyWidth) + PanMargin;
#ifdef SUPPORT_MULTIBYTE_EDIT
	    lft_chars = cell2char(Buffer, lft_cells);
	    lft_bytes = mbcs_skip(Buffer, lft_chars);
#else
	    lft_bytes = lft_cells;
#endif /* SUPPORT_MULTIBYTE_EDIT */
	}
    }

    if (pos_cells < lft_cells + PanMargin) {
	lft_cells = pos_cells - PanMargin;
	if (lft_cells < 0)
	    lft_cells = 0;
#ifdef SUPPORT_MULTIBYTE_EDIT
	lft_chars = cell2char(Buffer, lft_cells);
	lft_bytes = mbcs_skip(Buffer, lft_chars);
#else
	lft_bytes = lft_cells;
#endif /* SUPPORT_MULTIBYTE_EDIT */
    }

    LYmove(StartY, StartX);

    /*
     * Draw the left scrolling-indicator now, to avoid the complication of
     * overwriting part of a multicolumn character which may lie in the first
     * position.
     */
    if (IsPanned && lft_cells) {
	CTRACE_EDIT((tfp, "Draw left scroll-indicator\n"));
	TmpStyleOn(prompting ? s_prompt_edit_arr : s_aedit_arr);
	LYmove(StartY, StartX);
	LYaddch(ACS_LARROW);
	TmpStyleOff(prompting ? s_prompt_edit_arr : s_aedit_arr);
	lft_shift = 1;
    }

    str = &Buffer[lft_bytes];
    DpyStart = lft_bytes;

    dpy_cells = all_cells - lft_cells;
    CTRACE_EDIT((tfp, "Comparing dpy_cells %d > (%d - %d)\n",
		 dpy_cells, DpyWidth, lft_shift));
    if (dpy_cells > (DpyWidth - lft_shift)) {
	rgt_shift = 1;
	dpy_cells = (DpyWidth - lft_shift - rgt_shift);
    }
    for (;;) {
#ifdef SUPPORT_MULTIBYTE_EDIT
	dpy_chars = cell2char(str, dpy_cells);
	dpy_bytes = mbcs_skip(str, dpy_chars);
#else
	dpy_bytes = dpy_cells;
#endif /* SUPPORT_MULTIBYTE_EDIT */
	/*
	 * The last character on the display may be multicolumn, and if we take
	 * away a single cell for the right scroll-indicator, that would force
	 * us to display fewer characters.  Check for that, and recompute.
	 */
	if (rgt_shift && *str) {
	    int old_cells = dpy_cells;

	    dpy_cells = LYstrExtent2(str, dpy_bytes);
	    if (dpy_cells > old_cells)
		dpy_cells = old_cells - 1;

	    CTRACE_EDIT((tfp, "Comparing cells %d vs %d\n", dpy_cells, old_cells));
	    if (dpy_cells < old_cells) {
		CTRACE_EDIT((tfp, "Recomputing...\n"));
		continue;
	    }
	}
	break;
    }

    CTRACE_EDIT((tfp, "BYTES left %2d pos %2d dpy %2d all %2d\n",
		 lft_bytes, pos_bytes, dpy_bytes, all_bytes));
    CTRACE_EDIT((tfp, "CELLS left %2d pos %2d dpy %2d all %2d\n",
		 lft_cells, pos_cells, dpy_cells, all_cells));
    CTRACE_EDIT((tfp, "CHARS left %2d pos %2d dpy %2d all %2d\n",
		 lft_chars, pos_chars, dpy_chars, all_chars));

#ifdef USE_COLOR_STYLE
    /*
     * If this is the last screen line, set attributes to normal, should only
     * be needed for color styles.  The curses function may be used directly to
     * avoid complications.  - kw
     */
    if (StartY == (LYlines - 1))
	prompting = 1;
    if (prompting) {
	estyle = s_prompt_edit;
    } else {
	estyle = s_aedit;
    }
    CTRACE2(TRACE_STYLE,
	    (tfp, "STYLE.getstr: switching to <edit.%s>.\n",
	     prompting ? "prompt" : "active"));
    if (estyle != NOSTYLE) {
	curses_style(estyle, STACK_ON);
    } else {
	(void) wattrset(LYwin, A_NORMAL);	/* need to do something about colors? */
    }
#endif
    if (IsHidden) {
	BOOL utf_flag = IS_UTF8_TTY;
	int cell = 0;

	fill_edited_line(0, dpy_cells, '*');

	i = 0;
	do {
	    const char *last = str + i;
	    const char *next = LYmbcs_skip_glyphs(last, 1, utf_flag);
	    int j = (int) (next - str);

	    while (i < j) {
		Offs2Col[i++] = cell + StartX;
	    }
	    cell += LYstrExtent2(last, (int) (next - last));
	} while (i < dpy_bytes);
	Offs2Col[i] = cell + StartX;
    } else {
#if defined(ENHANCED_LINEEDIT) && defined(USE_COLOR_STYLE)
	if (EditMark >= 0 && DpyStart > EditMark)
	    TmpStyleOn(prompting ? s_prompt_sel : s_aedit_sel);
#endif
	remember_column(edit, 0);
	for (i = 0; i < dpy_bytes; i++) {
#if defined(ENHANCED_LINEEDIT) && defined(USE_COLOR_STYLE)
	    if (EditMark >= 0 && ((DpyStart + i == EditMark && EditAt > EditMark)
				  || (DpyStart + i == EditAt && EditAt < EditMark)))
		TmpStyleOn(prompting ? s_prompt_sel : s_aedit_sel);
	    if (EditMark >= 0 && ((DpyStart + i == EditMark && EditAt < EditMark)
				  || (DpyStart + i == EditAt && EditAt > EditMark)))
		TmpStyleOff(prompting ? s_prompt_sel : s_aedit_sel);
#endif
	    if (str[i] == 1 || str[i] == 2 ||
		(UCH(str[i]) == 160 &&
		 !(HTPassHighCtrlRaw || IS_CJK_TTY ||
		   (LYCharSet_UC[current_char_set].enc != UCT_ENC_8859 &&
		    !(LYCharSet_UC[current_char_set].like8859
		      & UCT_R_8859SPECL))))) {
		LYaddch(' ');
	    } else if (str[i] == '\t') {
		int col = Offs2Col[i] - StartX;

		/*
		 * Like LYwaddnstr(), expand tabs from the beginning of the
		 * field.
		 */
		while (++col % 8)
		    LYaddch(' ');
		LYaddch(' ');
	    } else {
		LYaddch(UCH(str[i]));
	    }
	    remember_column(edit, i + 1);
	}
#if defined(ENHANCED_LINEEDIT) && defined(USE_COLOR_STYLE)
	if (EditMark >= 0 &&
	    ((DpyStart + dpy_bytes <= EditMark && DpyStart + dpy_bytes > EditAt)
	     || (DpyStart + dpy_bytes > EditMark
		 && DpyStart + dpy_bytes <= EditAt))) {
	    TmpStyleOff(prompting ? s_prompt_sel : s_aedit_sel);
	}
#endif
    }

    /*
     * Erase rest of input area.
     */
    padsize = DpyWidth - (Offs2Col[dpy_bytes] - StartX);
    fill_edited_line(prompting, padsize, PadChar);

    /*
     * Scrolling indicators.
     */
    if (IsPanned && dpy_bytes && rgt_shift) {
	CTRACE((tfp, "Draw right-scroller offset (%d + %d)\n",
		dpy_cells, lft_shift));
	TmpStyleOn(prompting ? s_prompt_edit_arr : s_aedit_arr);
	LYmove(StartY, StartX + dpy_cells + lft_shift);
	LYaddch(ACS_RARROW);
	TmpStyleOff(prompting ? s_prompt_edit_arr : s_aedit_arr);
    }

    /*
     * Finally, move the cursor to the point where the next edit will occur.
     */
    LYmove(StartY, Offs2Col[EditAt - DpyStart]);

#ifdef USE_COLOR_STYLE
    if (estyle != NOSTYLE)
	curses_style(estyle, STACK_OFF);
#endif
    LYrefresh();
}

static void reinsertEdit(FieldEditor * edit, char *result)
{
    if (result != 0) {
	LYDoEdit(edit, '\0', LYE_ERASE, FALSE);
	while (*result != '\0') {
	    LYLineEdit(edit, (int) (*result), FALSE);
	    result++;
	}
    }
}

static int caselessCmpList(const void *a,
			   const void *b)
{
    return strcasecomp(*(STRING2PTR) a, *(STRING2PTR) b);
}

static int normalCmpList(const void *a,
			 const void *b)
{
    return strcmp(*(STRING2PTR) a, *(STRING2PTR) b);
}

static char **sortedList(HTList *list, int ignorecase)
{
    size_t count = (unsigned) HTList_count(list);
    size_t j = 0;
    size_t k, jk;
    char **result = typecallocn(char *, count + 1);

    if (result == 0)
	outofmem(__FILE__, "sortedList");

    assert(result != 0);

    while (!HTList_isEmpty(list))
	result[j++] = (char *) HTList_nextObject(list);

    if (count > 1) {
	qsort((char *) result, count, sizeof(*result),
	      ignorecase ? caselessCmpList : normalCmpList);

	/* remove duplicate entries from the sorted index */
	for (j = 0; result[j] != 0; j++) {
	    k = j;
	    while (result[k] != 0
		   && !strcmp(result[j], result[k])) {
		k++;
	    }
	    k--;
	    if (j != k) {
		for (jk = j;; jk++) {
		    result[jk] = result[jk + k - j];
		    if (result[jk] == 0)
			break;
		}
	    }
	}
    }

    return result;
}

int LYarrayLength(STRING2PTR list)
{
    int result = 0;

    while (*list++ != 0)
	result++;
    return result;
}

int LYarrayWidth(STRING2PTR list)
{
    int result = 0;
    int check;

    while (*list != 0) {
	check = (int) strlen(*list++);
	if (check > result)
	    result = check;
    }
    return result;
}

static void FormatChoiceNum(char *target,
			    int num_choices,
			    int choice,
			    const char *value)
{
    if (num_choices >= 0) {
	int digits = (num_choices > 9) ? 2 : 1;

	sprintf(target, "%*d: %.*s",
		digits, (choice + 1),
		MAX_LINE - 9 - digits, value);
    } else {
	LYStrNCpy(target, value, MAX_LINE - 1);
    }
}

static unsigned options_width(STRING2PTR list)
{
    unsigned width = 0;
    int count = 0;

    while (list[count] != 0) {
	unsigned ncells = (unsigned) LYstrCells(list[count]);

	if (ncells > width) {
	    width = ncells;
	}
	count++;
    }
    return width;
}

static void draw_option(WINDOW * win, int entry,
			int width,
			int reversed,
			int num_choices,
			int number,
			const char *value)
{
    char Cnum[MAX_LINE];

    (void) width;

    FormatChoiceNum(Cnum, num_choices, number, "");
#ifdef USE_SLANG
    SLsmg_gotorc(win->top_y + entry, (win->left_x + 2));
    LYaddstr(Cnum);
    if (reversed)
	SLsmg_set_color(2);
    SLsmg_write_nstring((SLFUTURE_CONST char *) value, (unsigned) win->width);
    if (reversed)
	SLsmg_set_color(0);
#else
    wmove(win, entry, 1);
    LynxWChangeStyle(win, s_menu_entry, STACK_ON);
    waddch(win, ' ');
    LynxWChangeStyle(win, s_menu_entry, STACK_OFF);
    LynxWChangeStyle(win, s_menu_number, STACK_ON);
    waddstr(win, Cnum);
    LynxWChangeStyle(win, s_menu_number, STACK_OFF);
#ifdef USE_COLOR_STYLE
    LynxWChangeStyle(win, reversed ? s_menu_active : s_menu_entry, STACK_ON);
#else
    if (reversed)
	wstart_reverse(win);
#endif
    LYpaddstr(win, width, value);
#ifdef USE_COLOR_STYLE
    LynxWChangeStyle(win, reversed ? s_menu_active : s_menu_entry, STACK_OFF);
#else
    if (reversed)
	wstop_reverse(win);
#endif
    LynxWChangeStyle(win, s_menu_entry, STACK_ON);
    waddch(win, ' ');
    LynxWChangeStyle(win, s_menu_entry, STACK_OFF);
#endif /* USE_SLANG */
}

/*
 * This function offers the choices for values of an option via a popup window
 * which functions like that for selection of options in a form.  - FM
 *
 * Also used for mouse popups with ncurses; this is indicated by for_mouse.
 */
int LYhandlePopupList(int cur_choice,
		      int ly,
		      int lx,
		      STRING2PTR choices,
		      int width,
		      int i_length,
		      int disabled,
		      int for_mouse)
{
    BOOLEAN numbered = (BOOLEAN) (keypad_mode != NUMBERS_AS_ARROWS);
    int c = 0, cmd = 0, i = 0, j = 0, rel = 0;
    int orig_choice;
    WINDOW *form_window;
    int num_choices = 0;
    int max_choices = 0;
    int top, bottom, length = -1;
    int window_offset = 0;
    int lines_to_show;
    char Cnum[64];
    int Lnum;
    int npages;
    static bstring *prev_target = NULL;		/* Search string buffer */
    static bstring *next_target = NULL;		/* Next search buffer */
    static BOOL first = TRUE;
    char *cp;
    int ch = 0;
    RecallType recall;
    int QueryTotal;
    int QueryNum;
    BOOLEAN FirstRecall = TRUE;
    BOOLEAN ReDraw = FALSE;
    int number;
    char buffer[MAX_LINE];
    const char *popup_status_msg = NULL;
    STRING2PTR Cptr = NULL;

#define CAN_SCROLL_DOWN	1
#define CAN_SCROLL_UP	2
#define CAN_SCROLL	4
    int can_scroll = 0, can_scroll_was = 0;

    orig_choice = cur_choice;
    if (cur_choice < 0)
	cur_choice = 0;

    /*
     * Initialize the search string buffer. - FM
     */
    if (first) {
	BStrCopy0(next_target, "");
	first = FALSE;
    }
    BStrCopy0(prev_target, "");
    QueryTotal = (search_queries ? HTList_count(search_queries) : 0);
    recall = ((QueryTotal >= 1) ? RECALL_URL : NORECALL);
    QueryNum = QueryTotal;

    /*
     * Count the number of choices to be displayed, where num_choices ranges
     * from 0 to n, and set width to the longest choice string length.  Also
     * set Lnum to the length for the highest choice number, then decrement
     * num_choices so as to be zero-based.  The window width will be based on
     * the sum of width and Lnum.  - FM
     */
    num_choices = LYarrayLength(choices) - 1;
    if (width <= 0)
	width = (int) options_width(choices);
    if (numbered) {
	sprintf(Cnum, "%d: ", num_choices);
	Lnum = (int) strlen(Cnum);
	max_choices = num_choices;
    } else {
	Lnum = 0;
	max_choices = -1;
    }

    /*
     * Let's assume for the sake of sanity that ly is the number corresponding
     * to the line the choice is on.
     *
     * Let's also assume that cur_choice is the number of the item that should
     * be initially selected, as 0 being the first item.
     *
     * So what we have, is the top equal to the current screen line subtracting
     * the cur_choice + 1 (the one must be for the top line we will draw in a
     * box).  If the top goes under 0, consider it 0.
     */
    top = ly - (cur_choice + 1);
    if (top < 0)
	top = 0;

    /*
     * Check and see if we need to put the i_length parameter up to the number
     * of real choices.
     */
    if (i_length < 1) {
	i_length = num_choices;
    } else {
	/*
	 * Otherwise, it is really one number too high.
	 */
	i_length--;
    }

    /*
     * The bottom is the value of the top plus the number of options to view
     * plus 3 (one for the top line, one for the bottom line, and one to offset
     * the 0 counted in the num_choices).
     */
    bottom = top + i_length + 3;

    /*
     * Set lines_to_show based on the user_mode global.
     */
    if (user_mode == NOVICE_MODE)
	lines_to_show = LYlines - 4;
    else
	lines_to_show = LYlines - 2;

    if (for_mouse && user_mode == NOVICE_MODE && lines_to_show > 2)
	lines_to_show--;

    /*
     * Hmm...  If the bottom goes beyond the number of lines available,
     */
    if (bottom > lines_to_show) {
	/*
	 * Position the window at the top if we have more choices than will fit
	 * in the window.
	 */
	if ((i_length + 3) > lines_to_show) {
	    top = 0;
	    bottom = (top + (i_length + 3));
	    if (bottom > lines_to_show)
		bottom = (lines_to_show + 1);
	} else {
	    /*
	     * Try to position the window so that the selected choice will
	     * appear where the selection box currently is positioned.  It
	     * could end up too high, at this point, but we'll move it down
	     * latter, if that has happened.
	     */
	    top = (lines_to_show + 1) - (i_length + 3);
	    bottom = (lines_to_show + 1);
	}
    }

    /*
     * This is really fun, when the length is 4, it means 0 to 4, or 5.
     */
    length = (bottom - top) - 2;
    if (length <= num_choices)
	can_scroll = CAN_SCROLL;

    /*
     * Move the window down if it's too high.
     */
    if (bottom < ly + 2) {
	bottom = ly + 2;
	if (bottom > lines_to_show + 1)
	    bottom = lines_to_show + 1;
	top = bottom - length - 2;
    }

    if (for_mouse) {
	int check = (Lnum + (int) width + 4);
	int limit = LYcols;

	/* shift horizontally to lie within screen width, if possible */
	if (check < limit) {
	    if (lx - 1 + check > limit)
		lx = limit + 1 - check;
	    else if (lx <= 0)
		lx = 1;
	}
    }

    /*
     * Set up the overall window, including the boxing characters ('*'), if it
     * all fits.  Otherwise, set up the widest window possible.  - FM
     */
    width += Lnum;
    bottom -= top;

    if (num_choices <= 0
	|| cur_choice > num_choices
	|| (form_window = LYstartPopup(&top,
				       &lx,
				       &bottom,
				       &width)) == 0)
	return (orig_choice);

    width -= Lnum;
    bottom += top;

    /*
     * Clear the command line and write the popup statusline.  - FM
     */
    if (disabled) {
	popup_status_msg = CHOICE_LIST_UNM_MSG;
    } else if (!for_mouse) {
	popup_status_msg = CHOICE_LIST_MESSAGE;
#if defined(USE_MOUSE) && (defined(NCURSES) || defined(PDCURSES))
    } else {
	popup_status_msg =
	    gettext("Left mouse button or return to select, arrow keys to scroll.");
#endif
    }
    _statusline(popup_status_msg);

    /*
     * Set up the window_offset for choices.
     * cur_choice ranges from 0...n
     * length ranges from 0...m
     */
    if (cur_choice >= length) {
	window_offset = cur_choice - length + 1;
    }

    /*
     * Compute the number of popup window pages.  - FM
     */
    npages = ((num_choices + 1) > length) ?
	(((num_choices + 1) + (length - 1)) / (length))
	: 1;
    /*
     * OH!  I LOVE GOTOs!  hack hack hack
     */
  redraw:

    /*
     * Display the boxed choices.
     */
    for (i = 0; i <= num_choices; i++) {
	if (i >= window_offset && i - window_offset < length) {
	    draw_option(form_window, ((i + 1) - window_offset), width, FALSE,
			max_choices, i, choices[i]);
	}
    }
    LYbox(form_window, !numbered);
    Cptr = NULL;

    /*
     * Loop on user input.
     */
    while (cmd != LYK_ACTIVATE) {
	int row = ((i + 1) - window_offset);

	/* Show scroll indicators. */
	if (can_scroll) {
	    can_scroll = ((window_offset ? CAN_SCROLL_UP : 0)
			  | (num_choices - window_offset >= length
			     ? CAN_SCROLL_DOWN : 0));
	    if (~can_scroll & can_scroll_was) {		/* Need to redraw */
		LYbox(form_window, !numbered);
		can_scroll_was = 0;
	    }
	    if (can_scroll & ~can_scroll_was & CAN_SCROLL_UP) {
		wmove(form_window, 1, Lnum + width + 3);
		LynxWChangeStyle(form_window, s_menu_sb, STACK_ON);
		waddch(form_window, ACS_UARROW);
		LynxWChangeStyle(form_window, s_menu_sb, STACK_OFF);
	    }
	    if (can_scroll & ~can_scroll_was & CAN_SCROLL_DOWN) {
		wmove(form_window, length, Lnum + width + 3);
		LynxWChangeStyle(form_window, s_menu_sb, STACK_ON);
		waddch(form_window, ACS_DARROW);
		LynxWChangeStyle(form_window, s_menu_sb, STACK_OFF);
	    }
	}

	/*
	 * Unreverse cur choice.
	 */
	if (Cptr != NULL) {
	    draw_option(form_window, row, width, FALSE,
			max_choices, i, Cptr[i]);
	}
	Cptr = choices;
	i = cur_choice;
	row = ((cur_choice + 1) - window_offset);
	draw_option(form_window, row, width, TRUE,
		    max_choices, cur_choice, Cptr[cur_choice]);
	LYstowCursor(form_window, row, 1);

	c = LYgetch_choice();
	if (term_options || LYCharIsINTERRUPT(c)) {	/* Control-C or Control-G */
	    cmd = LYK_QUIT;
#ifndef USE_SLANG
	} else if (c == MOUSE_KEY) {
	    if ((cmd = fancy_mouse(form_window, row, &cur_choice)) < 0)
		goto redraw;
	    if (cmd == LYK_ACTIVATE)
		break;
#endif
	} else {
	    cmd = LKC_TO_LAC(keymap, c);
	}
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    cmd = LYK_QUIT;
	}
#endif /* VMS */

	switch (cmd) {
	case LYK_F_LINK_NUM:
	    c = '\0';
	    /* FALLTHRU */
	case LYK_1:		/* FALLTHRU */
	case LYK_2:		/* FALLTHRU */
	case LYK_3:		/* FALLTHRU */
	case LYK_4:		/* FALLTHRU */
	case LYK_5:		/* FALLTHRU */
	case LYK_6:		/* FALLTHRU */
	case LYK_7:		/* FALLTHRU */
	case LYK_8:		/* FALLTHRU */
	case LYK_9:
	    /*
	     * Get a number from the user, possibly with a 'g' or 'p' suffix
	     * (which will be loaded into c).  - FM & LE
	     */
	    number = get_popup_number(SELECT_OPTION_NUMBER, &c, &rel);

	    /* handle + or - suffix */
	    CTRACE((tfp, "got popup option number %d, ", number));
	    CTRACE((tfp, "rel='%c', c='%c', cur_choice=%d\n",
		    rel, c, cur_choice));
	    if (c == 'p') {
		int curpage = ((cur_choice + 1) > length) ?
		(((cur_choice + 1) + (length - 1)) / (length))
		: 1;

		CTRACE((tfp, "  curpage=%d\n", curpage));
		if (rel == '+')
		    number = curpage + number;
		else if (rel == '-')
		    number = curpage - number;
	    } else if (rel == '+') {
		number = cur_choice + number + 1;
	    } else if (rel == '-') {
		number = cur_choice - number + 1;
	    }
	    if (rel)
		CTRACE((tfp, "new number=%d\n", number));
	    /*
	     * Check for a 'p' suffix.  - FM
	     */
	    if (c == 'p') {
		/*
		 * Treat 1 or less as the first page.  - FM
		 */
		if (number <= 1) {
		    if (window_offset == 0) {
			HTUserMsg(ALREADY_AT_OPTION_BEGIN);
			_statusline(popup_status_msg);
			break;
		    }
		    window_offset = 0;
		    cur_choice = 0;
		    _statusline(popup_status_msg);
		    goto redraw;
		}

		/*
		 * Treat a number equal to or greater than the number of pages
		 * as the last page.  - FM
		 */
		if (number >= npages) {
		    if (window_offset >= ((num_choices - length) + 1)) {
			HTUserMsg(ALREADY_AT_OPTION_END);
			_statusline(popup_status_msg);
			break;
		    }
		    window_offset = ((npages - 1) * length);
		    if (window_offset > (num_choices - length)) {
			window_offset = (num_choices - length + 1);
		    }
		    if (cur_choice < window_offset)
			cur_choice = window_offset;
		    _statusline(popup_status_msg);
		    goto redraw;
		}

		/*
		 * We want an intermediate page.  - FM
		 */
		if (((number - 1) * length) == window_offset) {
		    char *msg = 0;

		    HTSprintf0(&msg, ALREADY_AT_OPTION_PAGE, number);
		    HTUserMsg(msg);
		    FREE(msg);
		    _statusline(popup_status_msg);
		    break;
		}
		cur_choice = window_offset = ((number - 1) * length);
		_statusline(popup_status_msg);
		goto redraw;

	    }

	    /*
	     * Check for a positive number, which signifies that a choice
	     * should be sought.  - FM
	     */
	    if (number > 0) {
		/*
		 * Decrement the number so as to correspond with our cur_choice
		 * values.  - FM
		 */
		number--;

		/*
		 * If the number is in range and had no legal suffix, select
		 * the indicated choice.  - FM
		 */
		if (number <= num_choices && c == '\0') {
		    cur_choice = number;
		    cmd = LYK_ACTIVATE;
		    break;
		}

		/*
		 * Verify that we had a 'g' suffix, and act on the number.  -
		 * FM
		 */
		if (c == 'g') {
		    if (cur_choice == number) {
			/*
			 * The choice already is current.  - FM
			 */
			char *msg = 0;

			HTSprintf0(&msg, OPTION_ALREADY_CURRENT, (number + 1));
			HTUserMsg(msg);
			FREE(msg);
			_statusline(popup_status_msg);
			break;
		    }

		    if (number <= num_choices) {
			/*
			 * The number is in range and had a 'g' suffix, so make
			 * it the current option, scrolling if needed.  - FM
			 */
			j = (number - cur_choice);
			cur_choice = number;
			if ((j > 0) &&
			    (cur_choice - window_offset) >= length) {
			    window_offset += j;
			    if (window_offset > (num_choices - length + 1))
				window_offset = (num_choices - length + 1);
			} else if ((cur_choice - window_offset) < 0) {
			    window_offset -= abs(j);
			    if (window_offset < 0)
				window_offset = 0;
			}
			_statusline(popup_status_msg);
			goto redraw;
		    }

		    /*
		     * Not in range.  - FM
		     */
		    HTUserMsg(BAD_OPTION_NUM_ENTERED);
		}
	    }

	    /*
	     * Restore the popup statusline.  - FM
	     */
	    _statusline(popup_status_msg);
	    break;

	case LYK_PREV_LINK:
	case LYK_LPOS_PREV_LINK:
	case LYK_FASTBACKW_LINK:
	case LYK_UP_LINK:

	    if (cur_choice > 0)
		cur_choice--;

	    /*
	     * Scroll the window up if necessary.
	     */
	    if ((cur_choice - window_offset) < 0) {
		window_offset--;
		goto redraw;
	    }
	    break;

	case LYK_NEXT_LINK:
	case LYK_LPOS_NEXT_LINK:
	case LYK_FASTFORW_LINK:
	case LYK_DOWN_LINK:
	    if (cur_choice < num_choices)
		cur_choice++;

	    /*
	     * Scroll the window down if necessary
	     */
	    if ((cur_choice - window_offset) >= length) {
		window_offset++;
		goto redraw;
	    }
	    break;

	case LYK_NEXT_PAGE:
	    /*
	     * Okay, are we on the last page of the list?  If not then,
	     */
	    if (window_offset != (num_choices - length + 1)) {
		/*
		 * Modify the current choice to not be a coordinate in the
		 * list, but a coordinate on the item selected in the window.
		 */
		cur_choice -= window_offset;

		/*
		 * Page down the proper length for the list.  If simply to far,
		 * back up.
		 */
		window_offset += length;
		if (window_offset > (num_choices - length)) {
		    window_offset = (num_choices - length + 1);
		}

		/*
		 * Readjust the current selection to be a list coordinate
		 * rather than window.  Redraw this thing.
		 */
		cur_choice += window_offset;
		goto redraw;
	    } else if (cur_choice < num_choices) {
		/*
		 * Already on last page of the list so just redraw it with the
		 * last item selected.
		 */
		cur_choice = num_choices;
	    }
	    break;

	case LYK_PREV_PAGE:
	    /*
	     * Are we on the first page of the list?  If not then,
	     */
	    if (window_offset != 0) {
		/*
		 * Modify the current selection to not be a list coordinate,
		 * but a window coordinate.
		 */
		cur_choice -= window_offset;

		/*
		 * Page up the proper length.  If too far, back up.
		 */
		window_offset -= length;
		if (window_offset < 0) {
		    window_offset = 0;
		}

		/*
		 * Readjust the current choice.
		 */
		cur_choice += window_offset;
		goto redraw;
	    } else if (cur_choice > 0) {
		/*
		 * Already on the first page so just back up to the first item.
		 */
		cur_choice = 0;
	    }
	    break;

	case LYK_HOME:
	    cur_choice = 0;
	    if (window_offset > 0) {
		window_offset = 0;
		goto redraw;
	    }
	    break;

	case LYK_END:
	    cur_choice = num_choices;
	    if (window_offset != (num_choices - length + 1)) {
		window_offset = (num_choices - length + 1);
		goto redraw;
	    }
	    break;

	case LYK_DOWN_TWO:
	    cur_choice += 2;
	    if (cur_choice > num_choices)
		cur_choice = num_choices;

	    /*
	     * Scroll the window down if necessary.
	     */
	    if ((cur_choice - window_offset) >= length) {
		window_offset += 2;
		if (window_offset > (num_choices - length + 1))
		    window_offset = (num_choices - length + 1);
		goto redraw;
	    }
	    break;

	case LYK_UP_TWO:
	    cur_choice -= 2;
	    if (cur_choice < 0)
		cur_choice = 0;

	    /*
	     * Scroll the window up if necessary.
	     */
	    if ((cur_choice - window_offset) < 0) {
		window_offset -= 2;
		if (window_offset < 0)
		    window_offset = 0;
		goto redraw;
	    }
	    break;

	case LYK_DOWN_HALF:
	    cur_choice += (length / 2);
	    if (cur_choice > num_choices)
		cur_choice = num_choices;

	    /*
	     * Scroll the window down if necessary.
	     */
	    if ((cur_choice - window_offset) >= length) {
		window_offset += (length / 2);
		if (window_offset > (num_choices - length + 1))
		    window_offset = (num_choices - length + 1);
		goto redraw;
	    }
	    break;

	case LYK_UP_HALF:
	    cur_choice -= (length / 2);
	    if (cur_choice < 0)
		cur_choice = 0;

	    /*
	     * Scroll the window up if necessary.
	     */
	    if ((cur_choice - window_offset) < 0) {
		window_offset -= (length / 2);
		if (window_offset < 0)
		    window_offset = 0;
		goto redraw;
	    }
	    break;

	case LYK_REFRESH:
	    lynx_force_repaint();
	    LYrefresh();
	    break;

	case LYK_NEXT:
	    if (recall && isBEmpty(next_target)) {
		/*
		 * We got a 'n'ext command with no prior query specified within
		 * the popup window.  See if one was entered when the popup was
		 * retracted, and if so, assume that's what's wanted.  Note
		 * that it will become the default within popups, unless
		 * another is entered within a popup.  If the within popup
		 * default is to be changed at that point, use WHEREIS ('/')
		 * and enter it, or the up- or down-arrow keys to seek any of
		 * the previously entered queries, regardless of whether they
		 * were entered within or outside of a popup window.  - FM
		 */
		if ((cp = (char *) HTList_objectAt(search_queries,
						   0)) != NULL) {
		    BStrCopy0(next_target, cp);
		    QueryNum = 0;
		    FirstRecall = FALSE;
		}
	    }
	    BStrCopy(prev_target, next_target);
	    /* FALLTHRU */
	case LYK_WHEREIS:
	    if (isBEmpty(prev_target)) {
		_statusline(ENTER_WHEREIS_QUERY);
		if ((ch = LYgetBString(&prev_target, FALSE, 0, recall)) < 0) {
		    /*
		     * User cancelled the search via ^G.  - FM
		     */
		    HTInfoMsg(CANCELLED);
		    goto restore_popup_statusline;
		}
	    }

	  check_recall:
	    if (isBEmpty(prev_target) &&
		!(recall && (ch == UPARROW_KEY || ch == DNARROW_KEY))) {
		/*
		 * No entry.  Simply break.  - FM
		 */
		HTInfoMsg(CANCELLED);
		goto restore_popup_statusline;
	    }

	    if (recall && ch == UPARROW_KEY) {
		if (FirstRecall) {
		    /*
		     * Use the current string or last query in the list.  - FM
		     */
		    FirstRecall = FALSE;
		    if (!isBEmpty(next_target)) {
			for (QueryNum = (QueryTotal - 1);
			     QueryNum > 0; QueryNum--) {
			    if ((cp = (char *) HTList_objectAt(search_queries,
							       QueryNum))
				!= NULL &&
				!strcmp(next_target->str, cp)) {
				break;
			    }
			}
		    } else {
			QueryNum = 0;
		    }
		} else {
		    /*
		     * Go back to the previous query in the list.  - FM
		     */
		    QueryNum++;
		}
		if (QueryNum >= QueryTotal) {
		    /*
		     * Roll around to the last query in the list.  - FM
		     */
		    QueryNum = 0;
		}
		if ((cp = (char *) HTList_objectAt(search_queries,
						   QueryNum)) != NULL) {
		    BStrCopy0(prev_target, cp);
		    if (!isBEmpty(next_target) &&
			!strcmp(next_target->str, prev_target->str)) {
			_statusline(EDIT_CURRENT_QUERY);
		    } else if ((!isBEmpty(next_target) && QueryTotal == 2) ||
			       (isBEmpty(next_target) && QueryTotal == 1)) {
			_statusline(EDIT_THE_PREV_QUERY);
		    } else {
			_statusline(EDIT_A_PREV_QUERY);
		    }
		    if ((ch = LYgetBString(&prev_target,
					   FALSE, 0, recall)) < 0) {
			/*
			 * User cancelled the search via ^G.  - FM
			 */
			HTInfoMsg(CANCELLED);
			goto restore_popup_statusline;
		    }
		    goto check_recall;
		}
	    } else if (recall && ch == DNARROW_KEY) {
		if (FirstRecall) {
		    /*
		     * Use the current string or first query in the list.  - FM
		     */
		    FirstRecall = FALSE;
		    if (!isBEmpty(next_target)) {
			for (QueryNum = 0;
			     QueryNum < (QueryTotal - 1); QueryNum++) {
			    if ((cp = (char *) HTList_objectAt(search_queries,
							       QueryNum))
				!= NULL &&
				!strcmp(next_target->str, cp)) {
				break;
			    }
			}
		    } else {
			QueryNum = (QueryTotal - 1);
		    }
		} else {
		    /*
		     * Advance to the next query in the list.  - FM
		     */
		    QueryNum--;
		}
		if (QueryNum < 0) {
		    /*
		     * Roll around to the first query in the list.  - FM
		     */
		    QueryNum = (QueryTotal - 1);
		}
		if ((cp = (char *) HTList_objectAt(search_queries,
						   QueryNum)) != NULL) {
		    BStrCopy0(prev_target, cp);
		    if (isBEmpty(next_target) &&
			!strcmp(next_target->str, prev_target->str)) {
			_statusline(EDIT_CURRENT_QUERY);
		    } else if ((!isBEmpty(next_target) && QueryTotal == 2) ||
			       (isBEmpty(next_target) && QueryTotal == 1)) {
			_statusline(EDIT_THE_PREV_QUERY);
		    } else {
			_statusline(EDIT_A_PREV_QUERY);
		    }
		    if ((ch = LYgetBString(&prev_target,
					   FALSE, 0, recall)) < 0) {
			/*
			 * User cancelled the search via ^G. - FM
			 */
			HTInfoMsg(CANCELLED);
			goto restore_popup_statusline;
		    }
		    goto check_recall;
		}
	    }
	    /*
	     * Replace the search string buffer with the new target.  - FM
	     */
	    BStrCopy(next_target, prev_target);
	    HTAddSearchQuery(next_target->str);

	    /*
	     * Start search at the next choice.  - FM
	     */
	    for (j = 1; Cptr[i + j] != NULL; j++) {
		FormatChoiceNum(buffer, max_choices, (i + j), Cptr[i + j]);
		if (LYcase_sensitive) {
		    if (strstr(buffer, next_target->str) != NULL)
			break;
		} else {
		    if (LYstrstr(buffer, next_target->str) != NULL)
			break;
		}
	    }
	    if (Cptr[i + j] != NULL) {
		/*
		 * We have a hit, so make that choice the current.  - FM
		 */
		cur_choice += j;
		/*
		 * Scroll the window down if necessary.
		 */
		if ((cur_choice - window_offset) >= length) {
		    window_offset += j;
		    if (window_offset > (num_choices - length + 1))
			window_offset = (num_choices - length + 1);
		    ReDraw = TRUE;
		}
		goto restore_popup_statusline;
	    }

	    /*
	     * If we started at the beginning, it can't be present.  - FM
	     */
	    if (cur_choice == 0) {
		HTUserMsg2(STRING_NOT_FOUND, next_target->str);
		goto restore_popup_statusline;
	    }

	    /*
	     * Search from the beginning to the current choice.  - FM
	     */
	    for (j = 0; j < cur_choice; j++) {
		FormatChoiceNum(buffer, max_choices, (j + 1), Cptr[j]);
		if (LYcase_sensitive) {
		    if (strstr(buffer, next_target->str) != NULL)
			break;
		} else {
		    if (LYstrstr(buffer, next_target->str) != NULL)
			break;
		}
	    }
	    if (j < cur_choice) {
		/*
		 * We have a hit, so make that choice the current.  - FM
		 */
		j = (cur_choice - j);
		cur_choice -= j;
		/*
		 * Scroll the window up if necessary.
		 */
		if ((cur_choice - window_offset) < 0) {
		    window_offset -= j;
		    if (window_offset < 0)
			window_offset = 0;
		    ReDraw = TRUE;
		}
		goto restore_popup_statusline;
	    }

	    /*
	     * Didn't find it in the preceding choices either.  - FM
	     */
	    HTUserMsg2(STRING_NOT_FOUND, next_target->str);

	  restore_popup_statusline:
	    /*
	     * Restore the popup statusline and reset the search variables.  -
	     * FM
	     */
	    _statusline(popup_status_msg);
	    BStrCopy0(prev_target, "");
	    QueryTotal = (search_queries ? HTList_count(search_queries)
			  : 0);
	    recall = ((QueryTotal >= 1) ? RECALL_URL : NORECALL);
	    QueryNum = QueryTotal;
	    if (ReDraw == TRUE) {
		ReDraw = FALSE;
		goto redraw;
	    }
	    break;

	case LYK_QUIT:
	case LYK_ABORT:
	case LYK_PREV_DOC:
	case LYK_INTERRUPT:
	    cur_choice = orig_choice;
	    cmd = LYK_ACTIVATE;	/* to exit */
	    break;
	}
    }
    LYstopPopup();

    return (disabled ? orig_choice : cur_choice);
}

/*
 * Allow the user to edit a string.
 */
int LYgetBString(bstring **inputline,
		 int hidden,
		 size_t max_cols,
		 RecallType recall)
{
    int x, y;
    int ch;
    int xlec = -2;
    int last_xlec = -1;
    int last_xlkc = -1;
    FieldEditor MyEdit, *edit = &MyEdit;

#ifdef SUPPORT_MULTIBYTE_EDIT
    BOOL refresh_mb = TRUE;
#endif /* SUPPORT_MULTIBYTE_EDIT */
    BOOL done = FALSE;
    int result = -1;

    CTRACE((tfp, "called LYgetBString hidden %d, recall %d\n", hidden, (int) recall));

    LYGetYX(y, x);		/* Use screen from cursor position to eol */

    (void) y;
    (void) x;

    if (*inputline == NULL)	/* caller may not have initialized this */
	BStrCopy0(*inputline, "");

    LYSetupEdit(edit, (*inputline)->str, max_cols, LYcolLimit - x);
    IsHidden = (BOOL) hidden;
#ifdef FEPCTRL
    fep_on();
#endif

    while (!done) {
      beginning:
#ifndef SUPPORT_MULTIBYTE_EDIT
	LYRefreshEdit(edit);
#else /* SUPPORT_MULTIBYTE_EDIT */
	if (refresh_mb)
	    LYRefreshEdit(edit);
#endif /* SUPPORT_MULTIBYTE_EDIT */
	ch = LYReadCmdKey(FOR_PROMPT);
#ifdef SUPPORT_MULTIBYTE_EDIT
#ifdef CJK_EX			/* for SJIS code */
	if (!refresh_mb
	    && (EditBinding(ch) != LYE_CHAR))
	    goto beginning;
#else
	if (!refresh_mb
	    && (EditBinding(ch) != LYE_CHAR)
	    && (EditBinding(ch) != LYE_AIX))
	    goto beginning;
#endif
#endif /* SUPPORT_MULTIBYTE_EDIT */

	if (term_letter || term_options
#ifdef VMS
	    || HadVMSInterrupt
#endif /* VMS */
#ifndef DISABLE_NEWS
	    || term_message
#endif
	    ) {
#ifdef VMS
	    HadVMSInterrupt = FALSE;
#endif /* VMS */
	    ch = LYCharINTERRUPT2;
	}

	if (recall != NORECALL && (ch == UPARROW_KEY || ch == DNARROW_KEY)) {
	    BStrCopy0(*inputline, Buffer);
	    LYAddToCloset(recall, Buffer);
	    CTRACE((tfp, "LYgetstr(%s) recall\n", (*inputline)->str));
#ifdef FEPCTRL
	    fep_off();
#endif
	    LYFinishEdit(edit);
	    result = ch;
	    done = TRUE;
	    break;
	}
	ch |= InputMods;
	InputMods = 0;
	if (last_xlkc != -1) {
	    if (ch == last_xlkc)
		ch |= LKC_MOD3;
	    last_xlkc = -1;	/* consumed */
	}
#ifndef WIN_EX
	if (LKC_TO_LAC(keymap, ch) == LYK_REFRESH)
	    goto beginning;
#endif
	last_xlec = xlec;
	xlec = EditBinding(ch);
	if ((xlec & LYE_DF) && !(xlec & LYE_FORM_LAC)) {
	    last_xlkc = ch;
	    xlec &= ~LYE_DF;
	} else {
	    last_xlkc = -1;
	}
	switch (xlec) {
	case LYE_SETM1:
	    InputMods |= LKC_MOD1;
	    break;
	case LYE_SETM2:
	    InputMods |= LKC_MOD2;
	    break;
	case LYE_TAB:
	    if (xlec == last_xlec && recall != NORECALL) {
		HTList *list = whichRecall(recall);

		if (!HTList_isEmpty(list)) {
		    char **data = sortedList(list, (BOOL) (recall == RECALL_CMD));
		    int old_y, old_x;
		    int cur_choice = 0;
		    int num_options = LYarrayLength((STRING2PTR) data);

		    while (cur_choice < num_options
			   && strcasecomp(data[cur_choice], Buffer) < 0)
			cur_choice++;

		    LYGetYX(old_y, old_x);
		    cur_choice = LYhandlePopupList(cur_choice,
						   0,
						   old_x,
						   (STRING2PTR) data,
						   -1,
						   -1,
						   FALSE,
						   FALSE);
		    if (cur_choice >= 0) {
			if (recall == RECALL_CMD)
			    _statusline(": ");
			reinsertEdit(edit, data[cur_choice]);
		    }
		    LYmove(old_y, old_x);
		    FREE(data);
		}
	    } else {
		reinsertEdit(edit, LYFindInCloset(recall, Buffer));
	    }
	    break;

#ifndef CJK_EX
	case LYE_AIX:
	    /*
	     * Handle CJK characters, or as a valid character in the current
	     * display character set.  Otherwise, we treat this as LYE_ENTER.
	     */
	    if (ch != '\t' &&
		(IS_CJK_TTY ||
		 LYlowest_eightbit[current_char_set] <= 0x97)) {
		LYLineEdit(edit, ch, FALSE);
		break;
	    }
	    /* FALLTHRU */
#endif
	case LYE_ENTER:
	    BStrCopy0(*inputline, Buffer);
	    if (!hidden)
		LYAddToCloset(recall, Buffer);
	    CTRACE((tfp, "LYgetstr(%s) LYE_ENTER\n", (*inputline)->str));
#ifdef FEPCTRL
	    fep_off();
#endif
	    LYFinishEdit(edit);
	    result = ch;
	    done = TRUE;
	    break;

#ifdef CAN_CUT_AND_PASTE
	case LYE_PASTE:
	    {
		unsigned char *s = (unsigned char *) get_clip_grab(), *e;
		size_t len;

		if (!s)
		    break;
		len = strlen((const char *) s);
		e = s + len;

		if (len != 0) {
		    unsigned char *e1 = s;

		    while (e1 < e) {
			if (*e1 < ' ') {	/* Stop here? */
			    if (e1 > s)
				LYEditInsert(edit, s, (int) (e1 - s),
					     map_active, TRUE);
			    s = e1;
			    if (*e1 == '\t') {	/* Replace by space */
				LYEditInsert(edit,
					     (unsigned const char *) " ",
					     1,
					     map_active,
					     TRUE);
				s = ++e1;
			    } else {
				break;
			    }
			} else {
			    ++e1;
			}
		    }
		    if (e1 > s) {
			LYEditInsert(edit, s, (int) (e1 - s), map_active, TRUE);
		    }
		}
		get_clip_release();
		break;
	    }
#endif

	case LYE_ABORT:
	    CTRACE((tfp, "LYgetstr LYE_ABORT\n"));
#ifdef FEPCTRL
	    fep_off();
#endif
	    LYFinishEdit(edit);
	    BStrCopy0(*inputline, "");
	    done = TRUE;
	    break;

	case LYE_STOP:
	    CTRACE((tfp, "LYgetstr LYE_STOP\n"));
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
	    textfields_need_activation = TRUE;
	    LYFinishEdit(edit);
	    BStrCopy0(*inputline, "");
	    done = TRUE;
	    break;
#else
#ifdef ENHANCED_LINEEDIT
	    disableEditMark();
#endif
	    break;
#endif

	case LYE_LKCMD:
	    /*
	     * Used only in form_getstr() for invoking the LYK_F_LINK_NUM
	     * prompt when in form text fields.  - FM
	     */
	    break;

	case LYE_FORM_PASS:
	    /*
	     * Used in form_getstr() to end line editing and pass on the input
	     * char/lynxkeycode.  Here it is just ignored.  - kw
	     */
	    break;

	default:
	    if (xlec & LYE_FORM_LAC) {
		/*
		 * Used in form_getstr() to end line editing and pass on the
		 * lynxkeycode already containing a lynxactioncode.  Here it is
		 * just ignored.  - kw
		 */
		break;
	    }
#ifndef SUPPORT_MULTIBYTE_EDIT
	    LYLineEdit(edit, ch, FALSE);
#else /* SUPPORT_MULTIBYTE_EDIT */
	    if (LYLineEdit(edit, ch, FALSE) == 0) {
		if (refresh_mb && IS_CJK_TTY && (0x81 <= ch) && (ch <= 0xfe))
		    refresh_mb = FALSE;
		else
		    refresh_mb = TRUE;
	    } else {
		if (!refresh_mb) {
		    LYDoEdit(edit, 0, LYE_DELP, FALSE);
		}
	    }
#endif /* SUPPORT_MULTIBYTE_EDIT */
	}
    }
    return result;
}

/*
 * Use this for fixed-buffer edits which have not been converted to use
 * LYgetBString().
 */
int LYgetstr(char *inputline,	/* fixed-size buffer for input/output */
	     int hidden,	/* true to suppress from command-history */
	     size_t bufsize,	/* sizeof(inputline) */
	     RecallType recall)	/* type of command-history */
{
    int ch;
    bstring *my_bstring = NULL;

    BStrCopy0(my_bstring, inputline);
    if (my_bstring != 0) {
	ch = LYgetBString(&my_bstring, hidden, bufsize, recall);
	if (ch >= 0 && my_bstring != 0)
	    LYStrNCpy(inputline, my_bstring->str, bufsize);
	BStrFree(my_bstring);
    } else {
	ch = -1;
    }
    return ch;
}

const char *LYLineeditHelpURL(void)
{
    static int lasthelp_lineedit = -1;
    static char helpbuf[LY_MAXPATH] = "\0";
    static char *phelp = &helpbuf[0];
    const char *result = NULL;

    if (lasthelp_lineedit == current_lineedit) {
	result = helpbuf;
    } else {
	const char *source = LYLineeditHelpURLs[current_lineedit];
	size_t available;

	if (lasthelp_lineedit == -1) {
	    LYStrNCpy(helpbuf, helpfilepath, sizeof(helpbuf) - 1);
	    phelp += strlen(helpbuf);
	}
	available = (sizeof(helpbuf) - (size_t) (phelp - helpbuf));
	if (non_empty(source) &&
	    (strlen(source) <= available)) {
	    LYStrNCpy(phelp, source, available);
	    lasthelp_lineedit = current_lineedit;
	    result = helpbuf;
	}
    }
    return result;
}

/*
 * Wrapper for sscanf to ensure that lynx can "always" read a POSIX float.
 * In some locales, the decimal point changes.
 */
int LYscanFloat2(const char **source, float *result)
{
    int count = 0;
    char *temp;
    const char *src = *source;

    src = LYSkipCBlanks(src);
    *result = 0.0;
    if (StrChr(src, '.') != 0) {
	long frc_part = 0;
	float scale = 1.0;

	if (*src != '.') {
	    temp = NULL;
	    frc_part = strtol(src, &temp, 10);
	    *result = (float) frc_part;
	    src = temp;
	}
	if (src != 0 && *src == '.') {
	    ++src;
	    if (isdigit(UCH(*src))) {
		temp = NULL;
		frc_part = strtol(src, &temp, 10);
		if (temp != 0) {
		    int digits = (int) (temp - src);

		    while (digits-- > 0)
			scale *= (float) 10.0;
		    *result += ((float) frc_part / scale);
		}
		src = temp;
	    }
	}
	if (src != 0 && *src != '\0' && StrChr(" \t+", *src) == 0) {
	    char *extra = (char *) malloc(2 + strlen(src));

	    if (extra != 0) {
		extra[0] = '1';
		strcpy(extra + 1, src);
		if (sscanf(extra, "%f", &scale) == 1) {
		    *result *= scale;
		}
		FREE(extra);
		src = LYSkipCNonBlanks(src);
	    } else {
		src = 0;
	    }
	}
	if (src != 0)
	    count = 1;
    } else {
	count = sscanf(src, "%f", result);
	src = LYSkipCNonBlanks(src);
    }
    CTRACE2(TRACE_CFG,
	    (tfp, "LYscanFloat \"%s\" -> %f (%s)\n",
	     *source, *result,
	     count ? "ok" : "error"));
    *source = src;
    return count;
}

int LYscanFloat(const char *source, float *result)
{
    const char *temp = source;

    return LYscanFloat2(&temp, result);
}

/*
 * A replacement for 'strsep()'
 */
char *LYstrsep(char **stringp,
	       const char *delim)
{
    char *marker;
    char *result = 0;

    if (non_empty(stringp)) {
	result = *stringp;	/* will return the old value */
	marker = strpbrk(*stringp, delim);
	if (marker) {
	    *marker = '\0';	/* terminate the substring */
	    *stringp = ++marker;	/* point to the next substring */
	} else {
	    *stringp = 0;	/* this was the last */
	}
    }
    return result;
}

/*
 * LYstrstr finds the first occurrence of the string pointed to by needle
 * in the string pointed to by haystack.
 *
 * It returns NULL if the string is not found.
 *
 * It is a case insensitive search.
 */
char *LYstrstr(char *haystack,
	       const char *needle)
{
    int len = (int) strlen(needle);
    char *result = NULL;

    for (; *haystack != '\0'; haystack++) {
	if (0 == UPPER8(*haystack, *needle)) {
	    if (0 == strncasecomp8(haystack + 1, needle + 1, len - 1)) {
		result = haystack;
		break;
	    }
	}
    }

    return (result);
}

#define SkipSpecialChars(p) \
	while (IsSpecialAttrChar(*p) && *p != '\0') \
	    p++

/*
 * LYno_attr_char_case_strstr finds the first occurrence of the
 * string pointed to by needle in the string pointed to by haystack.
 *
 * It ignores special characters, e.g., LY_UNDERLINE_START_CHAR in haystack.
 *
 * It is a case insensitive search.
 */
const char *LYno_attr_char_case_strstr(const char *haystack,
				       const char *needle)
{
    const char *refptr, *tstptr;
    const char *result = NULL;

    if (haystack != NULL && needle != NULL) {

	SkipSpecialChars(haystack);

	for (; *haystack != '\0' && (result == NULL); haystack++) {
	    if (0 == UPPER8(*haystack, *needle)) {
		refptr = haystack + 1;
		tstptr = needle + 1;

		if (*tstptr == '\0') {
		    result = haystack;
		    break;
		}

		while (1) {
		    if (!IsSpecialAttrChar(*refptr)) {
			if (0 != UPPER8(*refptr, *tstptr))
			    break;
			refptr++;
			tstptr++;
		    } else {
			refptr++;
		    }
		    if (*tstptr == '\0') {
			result = haystack;
			break;
		    }
		    if (*refptr == '\0')
			break;
		}
	    }
	}
    }

    return (result);
}

/*
 * LYno_attr_char_strstr finds the first occurrence of the
 * string pointed to by needle in the string pointed to by haystack.
 * It ignores special characters, e.g., LY_UNDERLINE_START_CHAR in haystack.
 *
 * It is a case sensitive search.
 */
const char *LYno_attr_char_strstr(const char *haystack,
				  const char *needle)
{
    const char *refptr, *tstptr;
    const char *result = NULL;

    if (haystack != NULL && needle != NULL) {

	SkipSpecialChars(haystack);

	for (; *haystack != '\0' && (result == NULL); haystack++) {
	    if ((*haystack) == (*needle)) {
		refptr = haystack + 1;
		tstptr = needle + 1;

		if (*tstptr == '\0') {
		    result = haystack;
		    break;
		}

		while (1) {
		    if (!IsSpecialAttrChar(*refptr)) {
			if ((*refptr) != (*tstptr))
			    break;
			refptr++;
			tstptr++;
		    } else {
			refptr++;
		    }
		    if (*tstptr == '\0') {
			result = haystack;
			break;
		    } else if (*refptr == '\0') {
			break;
		    }
		}
	    }
	}
    }

    return (result);
}

/*
 * LYno_attr_mbcs_case_strstr finds the first occurrence of the string pointed
 * to by needle in the string pointed to by haystack.  It takes account of
 * MultiByte Character Sequences (UTF8).  The physical lengths of the displayed
 * string up to the start and end (= next position after) of the target string
 * are returned in *nstartp and *nendp if the search is successful.
 *
 * These lengths count glyph cells if count_gcells is set.  (Full-width
 * characters in CJK mode count as two.) Normally that's what we want.  They
 * count actual glyphs if count_gcells is unset.  (Full-width characters in CJK
 * mode count as one.)
 *
 * It ignores special characters, e.g., LY_UNDERLINE_START_CHAR in haystack.
 *
 * It assumes UTF8 if utf_flag is set.
 *
 * It is a case insensitive search.
 */
const char *LYno_attr_mbcs_case_strstr(const char *haystack,
				       const char *needle,
				       int utf_flag,
				       int count_gcells,
				       int *nstartp,
				       int *nendp)
{
    const char *refptr;
    const char *tstptr;
    int len = 0;
    int offset;
    const char *result = NULL;

    if (haystack != NULL && needle != NULL) {

	SkipSpecialChars(haystack);

	for (; *haystack != '\0' && (result == NULL); haystack++) {
	    if ((!utf_flag && IS_CJK_TTY && is8bits(*haystack) &&
		 *haystack == *needle &&
		 IsNormalChar(*(haystack + 1))) ||
		(0 == UPPER8(*haystack, *needle))) {
		int tarlen = 0;

		offset = len;
		len++;

		refptr = (haystack + 1);
		tstptr = (needle + 1);

		if (*tstptr == '\0') {
		    if (nstartp)
			*nstartp = offset;
		    if (nendp)
			*nendp = len;
		    result = haystack;
		    break;
		}
		if (!utf_flag && IS_CJK_TTY && is8bits(*haystack) &&
		    *haystack == *needle &&
		    IsNormalChar(*refptr)) {
		    /* handle a CJK multibyte string */
		    if (*refptr == *tstptr) {
			refptr++;
			tstptr++;
			if (count_gcells)
			    tarlen++;
			if (*tstptr == '\0') {
			    if (nstartp)
				*nstartp = offset;
			    if (nendp)
				*nendp = len + tarlen;
			    result = haystack;
			    break;
			}
		    } else {
			/* not a match */
			haystack++;
			if (count_gcells)
			    len++;
			continue;
		    }
		}
		/* compare the remainder of the string */
		while (1) {
		    if (!IsSpecialAttrChar(*refptr)) {
			if (!utf_flag && IS_CJK_TTY && is8bits(*refptr)) {
			    if (*refptr == *tstptr &&
				*(refptr + 1) == *(tstptr + 1) &&
				!IsSpecialAttrChar(*(refptr + 1))) {
				refptr++;
				tstptr++;
				if (count_gcells)
				    tarlen++;
			    } else {
				break;
			    }
			} else if (0 != UPPER8(*refptr, *tstptr)) {
			    break;
			}

			if (!IS_UTF_EXTRA(*tstptr)) {
			    tarlen++;
			}
			refptr++;
			tstptr++;

		    } else {
			refptr++;
		    }

		    if (*tstptr == '\0') {
			if (nstartp)
			    *nstartp = offset;
			if (nendp)
			    *nendp = len + tarlen;
			result = haystack;
			break;
		    }
		    if (*refptr == '\0')
			break;
		}
	    } else if (!(IS_UTF_EXTRA(*haystack) ||
			 IsSpecialAttrChar(*haystack))) {
		if (!utf_flag && IS_CJK_TTY && is8bits(*haystack) &&
		    IsNormalChar(*(haystack + 1))) {
		    haystack++;
		    if (count_gcells)
			len++;
		}
		len++;
	    }
	}
    }

    return (result);
}

/*
 * LYno_attr_mbcs_strstr finds the first occurrence of the string pointed
 * to by needle in the string pointed to by haystack.
 *
 * It takes account of CJK and MultiByte Character Sequences (UTF8).  The
 * physical lengths of the displayed string up to the start and end (= next
 * position after) the target string are returned in *nstartp and *nendp if the
 * search is successful.
 *
 * These lengths count glyph cells if count_gcells is set.  (Full-width
 * characters in CJK mode count as two.) Normally that's what we want.  They
 * count actual glyphs if count_gcells is unset.  (Full-width characters in CJK
 * mode count as one.)
 *
 * It ignores special characters, e.g., LY_UNDERLINE_START_CHAR in haystack.
 *
 * It assumes UTF8 if utf_flag is set.
 *
 * It is a case sensitive search.
 */
const char *LYno_attr_mbcs_strstr(const char *haystack,
				  const char *needle,
				  int utf_flag,
				  int count_gcells,
				  int *nstartp,
				  int *nendp)
{
    const char *refptr;
    const char *tstptr;
    int len = 0;
    int offset;
    const char *result = NULL;

    if (haystack != NULL && needle != NULL) {

	SkipSpecialChars(haystack);

	for (; *haystack != '\0' && (result == NULL); haystack++) {
	    if ((*haystack) == (*needle)) {
		int tarlen = 0;

		offset = len;
		len++;

		refptr = (haystack + 1);
		tstptr = (needle + 1);

		if (*tstptr == '\0') {
		    if (nstartp)
			*nstartp = offset;
		    if (nendp)
			*nendp = len;
		    result = haystack;
		    break;
		} else if (!utf_flag &&
			   IS_CJK_TTY &&
			   is8bits(*haystack) &&
			   IsNormalChar(*refptr)) {
		    /* handle a CJK multibyte string */
		    if (*refptr == *tstptr) {
			/* found match */
			refptr++;
			tstptr++;
			if (count_gcells)
			    tarlen++;
			if (*tstptr == '\0') {
			    if (nstartp)
				*nstartp = offset;
			    if (nendp)
				*nendp = len + tarlen;
			    result = haystack;
			    break;
			}
		    } else {
			/* not a match - restart comparison */
			haystack++;
			if (count_gcells)
			    len++;
			continue;
		    }
		}
		/* compare the remainder of the string */
		while (1) {
		    if (!IsSpecialAttrChar(*refptr)) {
			if (!utf_flag && IS_CJK_TTY && is8bits(*refptr)) {
			    if (*refptr == *tstptr &&
				*(refptr + 1) == *(tstptr + 1) &&
				!IsSpecialAttrChar(*(refptr + 1))) {
				refptr++;
				tstptr++;
				if (count_gcells)
				    tarlen++;
			    } else {
				break;
			    }
			} else if ((*refptr) != (*tstptr)) {
			    break;
			}

			if (!IS_UTF_EXTRA(*tstptr)) {
			    tarlen++;
			}
			refptr++;
			tstptr++;
		    } else {
			refptr++;
		    }

		    if (*tstptr == '\0') {
			if (nstartp)
			    *nstartp = offset;
			if (nendp)
			    *nendp = len + tarlen;
			result = haystack;
			break;
		    }
		    if (*refptr == '\0')
			break;
		}
	    } else if (!(IS_UTF_EXTRA(*haystack) ||
			 IsSpecialAttrChar(*haystack))) {
		if (!utf_flag && IS_CJK_TTY && is8bits(*haystack) &&
		    IsNormalChar(*(haystack + 1))) {
		    haystack++;
		    if (count_gcells)
			len++;
		}
		len++;
	    }
	}
    }
    return (result);
}

/*
 * Allocate and return a copy of a string.
 * see StrAllocCopy
 */
char *SNACopy(char **target,
	      const char *source,
	      size_t n)
{
    FREE(*target);
    if (source) {
	*target = typeMallocn(char, n + 1);

	if (*target == NULL) {
	    CTRACE((tfp, "Tried to malloc %lu bytes\n", (unsigned long) n));
	    outofmem(__FILE__, "SNACopy");
	    assert(*target != NULL);
	}
	LYStrNCpy(*target, source, n);
    }
    return *target;
}

/*
 * Combinate string allocation and concatenation.
 * see StrAllocCat
 */
char *SNACat(char **target,
	     const char *source,
	     size_t n)
{
    if (non_empty(source)) {
	if (*target) {
	    size_t length = strlen(*target);

	    *target = typeRealloc(char, *target, length + n + 1);

	    if (*target == NULL)
		outofmem(__FILE__, "SNACat");
	    assert(*target != NULL);
	    LYStrNCpy(*target + length, source, n);
	} else {
	    *target = typeMallocn(char, n + 1);

	    if (*target == NULL)
		outofmem(__FILE__, "SNACat");
	    assert(*target != NULL);
	    MemCpy(*target, source, n);
	    (*target)[n] = '\0';	/* terminate */
	}
    }
    return *target;
}

#include <caselower.h>

/*
 * Returns lowercase equivalent for unicode,
 * transparent output if no equivalent found.
 */
static long UniToLowerCase(long upper)
{
    size_t i, high, low;
    long diff = 0;
    long result = upper;

    if (upper > 0) {
	/*
	 * Try unicode_to_lower_case[].
	 */
	low = 0;
	high = TABLESIZE(unicode_to_lower_case);
	while (low < high) {
	    /*
	     * Binary search.
	     */
	    i = (low + (high - low) / 2);
	    diff = (unicode_to_lower_case[i].upper - upper);
	    if (diff < 0) {
		low = i + 1;
	    } else if (diff > 0) {
		high = i;
	    } else if (diff == 0) {
		result = unicode_to_lower_case[i].lower;
		break;
	    }
	}
    }

    return result;
}

/*
 *   UPPER8 ?
 *   it was "TOUPPER(a) - TOUPPER(b)" in its previous life...
 *
 *   It was realized that case-insensitive user search
 *   got information about upper/lower mapping from TOUPPER
 *   (precisely from "(TOUPPER(a) - TOUPPER(b))==0")
 *   and depends on locale in its 8bit mapping. -
 *   Usually fails with DOS/WINDOWS display charsets
 *   as well as on non-UNIX systems.
 *
 *   So use unicode case mapping.
 */
int UPPER8(int ch1, int ch2)
{
    int result = 0;

    if (ch1 == ch2) {
	result = 0;
    } else if (!ch2) {
	result = UCH(ch1);
    } else if (!ch1) {
	result = -UCH(ch2);
    } else if (UCH(TOASCII(ch1)) < 128 && UCH(TOASCII(ch2)) < 128) {
	/* case-insensitive match for us-ascii */
	result = (TOUPPER(ch1) - TOUPPER(ch2));
    } else if (UCH(TOASCII(ch1)) > 127 &&
	       UCH(TOASCII(ch2)) > 127) {
	/* case-insensitive match for upper half */
	if (DisplayCharsetMatchLocale) {
	    result = (TOUPPER(ch1) - TOUPPER(ch2));	/* old-style */
	} else {
	    long uni_ch2 = UCTransToUni((char) ch2, current_char_set);
	    long uni_ch1;

	    if (uni_ch2 < 0) {
		result = UCH(ch1);
	    } else {
		uni_ch1 = UCTransToUni((char) ch1, current_char_set);
		result = (int) (UniToLowerCase(uni_ch1) - UniToLowerCase(uni_ch2));
	    }
	}
    } else {
	result = -10;		/* mismatch */
    }

    return result;
}

/*
 * Replaces 'fgets()' calls into a fixed-size buffer with reads into a buffer
 * that is allocated.  When an EOF or error is found, the buffer is freed
 * automatically.
 */
char *LYSafeGets(char **target,
		 FILE *fp)
{
    char buffer[BUFSIZ];
    char *result = 0;

    if (target != 0)
	result = *target;
    if (result != 0)
	*result = 0;

    while (fgets(buffer, (int) sizeof(buffer), fp) != NULL) {
	if (*buffer)
	    result = StrAllocCat(result, buffer);
	if (StrChr(buffer, '\n') != 0)
	    break;
    }
    if (ferror(fp)) {
	FREE(result);
    } else if (feof(fp) && result && *result == '\0') {
	/*
	 * If the file ends in the middle of a line, return the partial line;
	 * if another call is made after this, it will return NULL.  - kw
	 */
	FREE(result);
    }
    if (target != 0)
	*target = result;
    return result;
}

#ifdef USE_CMD_LOGGING
static FILE *cmd_logfile;
static FILE *cmd_script;

void LYOpenCmdLogfile(int argc,
		      char **argv)
{
    int n;

    if (lynx_cmd_logfile != 0) {
	cmd_logfile = LYNewTxtFile(lynx_cmd_logfile);
	if (cmd_logfile != 0) {
	    fprintf(cmd_logfile, "# Command logfile created by %s %s (%s)\n",
		    LYNX_NAME, LYNX_VERSION, LYVersionDate());
	    for (n = 0; n < argc; n++) {
		fprintf(cmd_logfile, "# Arg%d = %s\n", n, argv[n]);
	    }
	}
    }
}

BOOL LYHaveCmdScript(void)
{
    return (BOOL) (cmd_script != 0);
}

void LYOpenCmdScript(void)
{
    if (lynx_cmd_script != 0) {
	cmd_script = fopen(lynx_cmd_script, TXT_R);
	CTRACE((tfp, "LYOpenCmdScript(%s) %s\n",
		lynx_cmd_script,
		cmd_script != 0 ? "SUCCESS" : "FAIL"));
    }
}

int LYReadCmdKey(int mode)
{
    int ch = -1;

    if (cmd_script != 0) {
	char *buffer = 0;
	char *src;
	char *tmp;

	while ((ch < 0) && LYSafeGets(&buffer, cmd_script) != 0) {
	    LYTrimTrailing(buffer);
	    src = LYSkipBlanks(buffer);
	    tmp = LYSkipNonBlanks(src);
	    switch ((unsigned) (tmp - src)) {
	    case 4:
		if (!strncasecomp(src, "exit", 4))
		    exit_immediately(EXIT_SUCCESS);
		break;
	    case 3:
		if (!strncasecomp(src, "key", 3)) {
		    ch = LYStringToKeycode(LYSkipBlanks(tmp));
		} else if (!strncasecomp(src, "set", 3)) {
		    src = LYSkipBlanks(tmp);
		    tmp = src;
		    while (*tmp != '\0') {
			if (isspace(UCH(*tmp)) || *tmp == '=')
			    break;
			++tmp;
		    }
		    if (*tmp != '\0') {
			*tmp++ = '\0';
			tmp = LYSkipBlanks(tmp);
		    }
		    if (LYSetConfigValue(src, tmp)) {
			CTRACE((tfp, "LYSetConfigValue(%s, %s)\n", src, tmp));
		    } else if (LYsetRcValue(src, tmp)) {
			CTRACE((tfp, "LYsetRcValue(%s, %s)\n", src, tmp));
		    } else {
			CTRACE((tfp, "?? set ignored %s\n", src));
		    }
		}
		break;
	    }
	}
	if (feof(cmd_script)) {
	    fclose(cmd_script);
	    cmd_script = 0;
	}
	if (ch >= 0) {
	    LYSleepReplay();
	    LYrefresh();
	}
	FREE(buffer);
    } else {
	ch = LYgetch_for(mode);
    }
    CTRACE((tfp, "LYReadCmdKey(%d) ->%s (%#x)\n",
	    mode, LYKeycodeToString(ch, TRUE), ch));
    LYWriteCmdKey(ch);
    return ch;
}

/*
 * Write a LYKeymapCode 'ch' to the logfile.
 */
void LYWriteCmdKey(int ch)
{
    if (cmd_logfile != 0) {
	fprintf(cmd_logfile, "key %s\n", LYKeycodeToString(ch, FALSE));
    }
}

void LYCloseCmdLogfile(void)
{
    if (cmd_logfile != 0) {
	LYCloseOutput(cmd_logfile);
	cmd_logfile = 0;
    }
    if (cmd_script != 0) {
	LYCloseInput(cmd_script);
	cmd_script = 0;
    }
    FREE(lynx_cmd_logfile);
    FREE(lynx_cmd_script);
}
#endif /* USE_CMD_LOGGING */
