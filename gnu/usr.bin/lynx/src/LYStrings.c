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

#ifdef EXP_CMD_LOGGING
#include <LYReadCFG.h>
#endif

#include <LYShowInfo.h>
#include <LYLeaks.h>

#if defined(WIN_EX)
#undef  BUTTON_CTRL
#define BUTTON_CTRL	0	/* Quick hack */
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
	&& (MOUSE_X_POS >= getbegx(win)
	    && (MOUSE_X_POS < (getbegx(win) + getmaxx(win))))) {
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
    MEVENT event;

    getmouse(&event);
    if ((event.bstate & (BUTTON1_CLICKED
			 | BUTTON1_DOUBLE_CLICKED
			 | BUTTON1_TRIPLE_CLICKED))) {
	int mypos = event.y - getbegy(win);
	int delta = mypos - row;

	if ((event.x < getbegx(win) || event.x >= (getbegx(win) + getmaxx(win)))
	    && !(event.bstate & (BUTTON_ALT | BUTTON_SHIFT | BUTTON_CTRL)))
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
	    if (event.bstate & (BUTTON1_DOUBLE_CLICKED
				| BUTTON1_TRIPLE_CLICKED))
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
	    if (event.bstate & (BUTTON1_DOUBLE_CLICKED
				| BUTTON1_TRIPLE_CLICKED))
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
	} else if (event.bstate & (BUTTON_ALT | BUTTON_SHIFT | BUTTON_CTRL)) {
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
    unsigned len = strlen(base);

    while (!HTList_isEmpty(list)) {
	data = (char *) HTList_nextObject(list);
	if (!strncmp(base, data, len))
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
		    ? HOME : LAC_TO_LKC0(LYK_MAIN_MENU);
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
		    ? LTARROW
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
		    ? RTARROW : LAC_TO_LKC0(LYK_HISTORY);
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
		    len = strlen(text);
		cur_err = XYdist(x, y, links[i].lx, links[i].ly, len);
		/* Check the second line */
		while (cur_err > 0
		       && (text = LYGetHiliteStr(i, ++count)) != NULL) {
		    /* Note that there is at most one hightext if is_text */
		    int cur_err_2 = XYdist(x, y,
					   LYGetHilitePos(i, count),
					   links[i].ly + count,
					   strlen(text));

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
			    || !(cury == y && (curx >= lx) && ((curx - lx) <= len))) {
			    c = LAC_TO_LKC0(LYK_SUBMIT);
			    mouse_link = i;
			} else {
			    c = LAC_TO_LKC0(LYK_SUBMIT);
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
 * LYstrncpy() terminates strings with a null byte.  Writes a null byte into
 * the n+1 byte of dst.
 */
char *LYstrncpy(char *dst,
		const char *src,
		int n)
{
    char *val;
    int len;

    if (src == 0)
	src = "";
    len = strlen(src);

    if (n < 0)
	n = 0;

    val = strncpy(dst, src, n);
    if (len < n)
	*(dst + len) = '\0';
    else
	*(dst + n) = '\0';
    return val;
}

#define IS_NEW_GLYPH(ch) (utf_flag && (UCH(ch)&0xc0) != 0x80)
#define IS_UTF_EXTRA(ch) (utf_flag && (UCH(ch)&0xc0) == 0x80)

/*
 * LYmbcsstrncpy() terminates strings with a null byte.  It takes account of
 * multibyte characters.  The src string is copied until either end of string
 * or max number of either bytes or glyphs (mbcs sequences) (CJK or UTF8).  The
 * utf_flag argument should be TRUE for UTF8.  - KW & FM
 */
char *LYmbcsstrncpy(char *dst,
		    const char *src,
		    int n_bytes,
		    int n_glyphs,
		    BOOL utf_flag)
{
    char *val = dst;
    int i_bytes = 0, i_glyphs = 0;

    if (n_bytes < 0)
	n_bytes = 0;
    if (n_glyphs < 0)
	n_glyphs = 0;

    for (; *src != '\0' && i_bytes < n_bytes; i_bytes++) {
	if (IS_NEW_GLYPH(*src)) {
	    if (i_glyphs++ >= n_glyphs) {
		*dst = '\0';
		return val;
	    }
	}
	*(dst++) = *(src++);
    }
    *dst = '\0';

    return val;
}

/*
 * LYmbcs_skip_glyphs() skips a given number of display positions in a string
 * and returns the resulting pointer.  It takes account of UTF-8 encoded
 * characters.  - KW
 */
const char *LYmbcs_skip_glyphs(const char *data,
			       int n_glyphs,
			       BOOL utf_flag)
{
    int i_glyphs = 0;

    if (n_glyphs < 0)
	n_glyphs = 0;

    if (!data)
	return NULL;
    if (!utf_flag)
	return (data + n_glyphs);

    while (*data) {
	if (IS_NEW_GLYPH(*data)) {
	    if (i_glyphs++ >= n_glyphs) {
		return data;
	    }
	}
	data++;
    }
    return data;
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
		 BOOL utf_flag,
		 BOOL count_gcells)
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
		    } else if (!utf_flag && HTCJK != NOCJK && !count_gcells &&
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
    *button = SLang_getkey();
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

    *x = SLang_getkey();
    if (*x == CH_ESC)		/* Undo 7-bit replace for large x - kw */
	*x = SLang_getkey() + 64 - 33;
    else
	*x -= 33;
    *y = SLang_getkey();
    if (*y == CH_ESC)		/* Undo 7-bit replace for large y - kw */
	*y = SLang_getkey() + 64 - 33;
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
void ena_csi(BOOLEAN flag)
{
    csi_is_csi = flag;
}

#if defined(USE_KEYMAPS)

#ifdef USE_SLANG
#define define_key(string, code) \
	SLkm_define_keysym ((char*)(string), code, Keymap_List)
#if SLANG_VERSION < 20000
#define expand_substring(dst, first, last, final) \
 	(SLexpand_escaped_string(dst, (char *)first, (char *)last), 1)
static int SLang_get_error(void)
{
    return SLang_Error;
}
#else
int LY_Slang_UTF8_Mode = 0;

#define expand_substring(dst, first, last, final) \
	(SLexpand_escaped_string(dst, (char *)first, (char *)last, LY_Slang_UTF8_Mode), 1)
#endif

static SLKeyMap_List_Type *Keymap_List;

/* This value should be larger than anything in LYStrings.h */
#define MOUSE_KEYSYM 0x0400
#endif

#define SQUOTE '\''
#define DQUOTE '"'
#define ESCAPE '\\'
#define LPAREN '('
#define RPAREN ')'

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
# define INTERN_KEY(string,lynx,curses)          {string,lynx}
#else
#define INTERN_KEY(string,lynx,curses)           {string,curses}
#define EXTERN_KEY(string,string1,lynx,curses)   {string,curses}
#endif

typedef struct {
    const char *string;
    int value;
} Keysym_String_List;
/* *INDENT-OFF* */
static Keysym_String_List Keysym_Strings [] =
{
    INTERN_KEY( "UPARROW",	UPARROW,	KEY_UP ),
    INTERN_KEY( "DNARROW",	DNARROW,	KEY_DOWN ),
    INTERN_KEY( "RTARROW",	RTARROW,	KEY_RIGHT ),
    INTERN_KEY( "LTARROW",	LTARROW,	KEY_LEFT ),
    INTERN_KEY( "PGDOWN",	PGDOWN,		KEY_NPAGE ),
    INTERN_KEY( "PGUP",		PGUP,		KEY_PPAGE ),
    INTERN_KEY( "HOME",		HOME,		KEY_HOME ),
    INTERN_KEY( "END",		END_KEY,	KEY_END ),
    INTERN_KEY( "F1",		F1,		KEY_F(1) ),
    INTERN_KEY( "DO_KEY",	DO_KEY,		KEY_F(16) ),
    INTERN_KEY( "FIND_KEY",	FIND_KEY,	KEY_FIND ),
    INTERN_KEY( "SELECT_KEY",	SELECT_KEY,	KEY_SELECT ),
    INTERN_KEY( "INSERT_KEY",	INSERT_KEY,	KEY_IC ),
    INTERN_KEY( "REMOVE_KEY",	REMOVE_KEY,	KEY_DC ),
    INTERN_KEY( "DO_NOTHING",	DO_NOTHING,	DO_NOTHING|LKC_ISLKC ),
    INTERN_KEY( NULL,		-1,		ERR )
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

    strncpy(name, first, len);
    name[len] = '\0';
    if ((code = lookup_tiname(name, strnames)) >= 0
	|| (code = lookup_tiname(name, strfnames)) >= 0) {
	if (cur_term->type.Strings[code] != 0) {
	    LYstrncpy(*result, cur_term->type.Strings[code], final - *result);
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

	LYstrncpy(tmp, first, limit);
	value = strtol(tmp, &last, radix);
	if (last != 0 && last != tmp)
	    first += (last - tmp);
    }

    if (name != 0) {
	(void) expand_tiname(name, strlen(name), result, final);
    } else {
	**result = value;
	(*result) += 1;
    }

    return first;
}

static int expand_substring(char *dst, const char *first, const char *last, char *final)
{
    int ch;

    while (first < last) {
	switch (ch = *first++) {
	case ESCAPE:
	    first = expand_tichar(first, &dst, final);
	    break;
	case '^':
	    ch = *first++;
	    if (ch == LPAREN) {
		const char *s = strchr(first, RPAREN);
		char *was = dst;

		if (s == 0)
		    s = first + strlen(first);
		first = expand_tiname(first, s - first, &dst, final);
		if (dst == was)
		    return 0;
		if (*first)
		    first++;
	    } else if (ch == '?') {	/* ASCII delete? */
		*dst++ = 127;
	    } else if ((ch & 0x3f) < 0x20) {	/* ASCII control char? */
		*dst++ = (ch & 0x1f);
	    } else {
		*dst++ = '^';
		first--;	/* not legal... */
	    }
	    break;
	case 0:		/* convert nulls for terminfo */
	    ch = 0200;
	    /* FALLTHRU */
	default:
	    *dst++ = ch;
	    break;
	}
    }
    *dst = '\0';
    return 1;
}
#endif

static void unescaped_char(const char *parse, int *keysym)
{
    size_t len = strlen(parse);
    char buf[BUFSIZ];

    if (len >= 3) {
	expand_substring(buf, parse + 1, parse + len - 1, buf + sizeof(buf) - 1);
	if (strlen(buf) == 1)
	    *keysym = *buf;
    }
}

static BOOLEAN unescape_string(char *src, char *dst, char *final)
{
    BOOLEAN ok = FALSE;

    if (*src == SQUOTE) {
	int keysym = -1;

	unescaped_char(src, &keysym);
	if (keysym >= 0) {
	    dst[0] = keysym;
	    dst[1] = '\0';
	    ok = TRUE;
	}
    } else if (*src == DQUOTE) {
	ok = expand_substring(dst, src + 1, src + strlen(src) - 1, final);
	(void) final;
    }
    return ok;
}

int map_string_to_keysym(const char *str, int *keysym)
{
    int modifier = 0;

    *keysym = -1;

    if (strncasecomp(str, "LAC:", 4) == 0) {
	char *other = strchr(str + 4, ':');

	if (other) {
	    int othersym = lecname_to_lec(other + 1);
	    char buf[BUFSIZ];

	    if (othersym >= 0 && other - str - 4 < BUFSIZ) {
		strncpy(buf, str + 4, other - str - 4);
		buf[other - str - 4] = '\0';
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
    }
    if (strncasecomp(str, "Meta-", 5) == 0) {
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

		expand_substring(buf,
				 str,
				 str + HTMIN(len, 28),
				 buf + sizeof(buf) - 1);
		if (strlen(buf) <= 1)
		    return (*keysym = (UCH(buf[0])) | modifier);
	    }
	}
    }
    if (*str == SQUOTE) {
	unescaped_char(str, keysym);
    } else if (isdigit(UCH(*str))) {
	char *tmp;
	long value = strtol(str, &tmp, 0);

	if (!isalnum(UCH(*tmp))) {
	    *keysym = value;
#ifndef USE_SLANG
	    if (*keysym > 255)
		*keysym |= LKC_ISLKC;	/* caller should remove this flag - kw */
#endif
	}
    } else {
	Keysym_String_List *k;

	k = Keysym_Strings;
	while (k->string != NULL) {
	    if (0 == strcmp(k->string, str)) {
		*keysym = k->value;
		break;
	    }
	    k++;
	}
    }

    if (*keysym >= 0)
	*keysym |= modifier;
    return (*keysym);
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
static int setkey_cmd(char *parse)
{
    char *s, *t;
    int keysym;
    char buf[BUFSIZ];

    CTRACE((tfp, "KEYMAP(PA): in=%s", parse));	/* \n-terminated */
    if ((s = skip_keysym(parse)) != 0) {
	if (isspace(UCH(*s))) {
	    *s++ = '\0';
	    s = LYSkipBlanks(s);
	    if ((t = skip_keysym(s)) == 0) {
		CTRACE((tfp, "KEYMAP(SKIP) no key expansion found\n"));
		return -1;
	    }
	    if (t != s)
		*t = '\0';
	    if (map_string_to_keysym(s, &keysym) >= 0) {
		if (!unescape_string(parse, buf, buf + sizeof(buf) - 1)) {
		    CTRACE((tfp, "KEYMAP(SKIP) could unescape key\n"));
		    return 0;	/* Trace the failure and continue. */
		}
		if (LYTraceLogFP == 0) {
		    CTRACE((tfp, "KEYMAP(DEF) keysym=%#x\n", keysym));
		} else {
		    CTRACE((tfp, "KEYMAP(DEF) keysym=%#x, seq='%s'\n", keysym, buf));
		}
		return define_key(buf, keysym);
	    } else {
		CTRACE((tfp, "KEYMAP(SKIP) could not map to keysym\n"));
	    }
	} else {
	    CTRACE((tfp, "KEYMAP(SKIP) junk after key description: '%s'\n", s));
	}
    } else {
	CTRACE((tfp, "KEYMAP(SKIP) no key description\n"));
    }
    return -1;
}

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

	    if (strlen(s) > len && !strncmp(s, table[n].name, len)
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
	INTERN_KEY( "\033[A",	UPARROW,	KEY_UP ),
	INTERN_KEY( "\033OA",	UPARROW,	KEY_UP ),
	INTERN_KEY( "\033[B",	DNARROW,	KEY_DOWN ),
	INTERN_KEY( "\033OB",	DNARROW,	KEY_DOWN ),
	INTERN_KEY( "\033[C",	RTARROW,	KEY_RIGHT ),
	INTERN_KEY( "\033OC",	RTARROW,	KEY_RIGHT ),
	INTERN_KEY( "\033[D",	LTARROW,	KEY_LEFT ),
	INTERN_KEY( "\033OD",	LTARROW,	KEY_LEFT ),
	INTERN_KEY( "\033[1~",	FIND_KEY,	KEY_FIND ),
	INTERN_KEY( "\033[2~",	INSERT_KEY,	KEY_IC ),
	INTERN_KEY( "\033[3~",	REMOVE_KEY,	KEY_DC ),
	INTERN_KEY( "\033[4~",	SELECT_KEY,	KEY_SELECT ),
	INTERN_KEY( "\033[5~",	PGUP,		KEY_PPAGE ),
	INTERN_KEY( "\033[6~",	PGDOWN,		KEY_NPAGE ),
	INTERN_KEY( "\033[7~",	HOME,		KEY_HOME),
	INTERN_KEY( "\033[8~",	END_KEY,	KEY_END ),
	INTERN_KEY( "\033[11~",	F1,		KEY_F(1) ),
	INTERN_KEY( "\033[28~",	F1,		KEY_F(1) ),
	INTERN_KEY( "\033OP",	F1,		KEY_F(1) ),
	INTERN_KEY( "\033[OP",	F1,		KEY_F(1) ),
	INTERN_KEY( "\033[29~",	DO_KEY,		KEY_F(16) ),
#if defined(USE_SLANG) && (defined(__WIN32__) || defined(__MINGW32__))
	INTERN_KEY( "\xE0H",	UPARROW,	KEY_UP ),
	INTERN_KEY( "\xE0P",	DNARROW,	KEY_DOWN ),
	INTERN_KEY( "\xE0M",	RTARROW,	KEY_RIGHT ),
	INTERN_KEY( "\xE0K",	LTARROW,	KEY_LEFT ),
	INTERN_KEY( "\xE0R",	INSERT_KEY,	KEY_IC ),
	INTERN_KEY( "\xE0S",	REMOVE_KEY,	KEY_DC ),
	INTERN_KEY( "\xE0I",	PGUP,		KEY_PPAGE ),
	INTERN_KEY( "\xE0Q",	PGDOWN,		KEY_NPAGE ),
	INTERN_KEY( "\xE0G",	HOME,		KEY_HOME),
	INTERN_KEY( "\xE0O",	END_KEY,	KEY_END ),
#endif
#if defined(USE_SLANG) && !defined(VMS)
	INTERN_KEY(	"^(ku)", UPARROW,	KEY_UP ),
	INTERN_KEY(	"^(kd)", DNARROW,	KEY_DOWN ),
	INTERN_KEY(	"^(kr)", RTARROW,	KEY_RIGHT ),
	INTERN_KEY(	"^(kl)", LTARROW,	KEY_LEFT ),
	INTERN_KEY(	"^(@0)", FIND_KEY,	KEY_FIND ),
	INTERN_KEY(	"^(kI)", INSERT_KEY,	KEY_IC ),
	INTERN_KEY(	"^(kD)", REMOVE_KEY,	KEY_DC ),
	INTERN_KEY(	"^(*6)", SELECT_KEY,	KEY_SELECT ),
	INTERN_KEY(	"^(kP)", PGUP,		KEY_PPAGE ),
	INTERN_KEY(	"^(kN)", PGDOWN,	KEY_NPAGE ),
	INTERN_KEY(	"^(@7)", END_KEY,	KEY_END ),
	INTERN_KEY(	"^(kh)", HOME,		KEY_HOME),
	INTERN_KEY(	"^(k1)", F1,		KEY_F(1) ),
	INTERN_KEY(	"^(F6)", DO_KEY,	KEY_F(16) ),
#endif /* SLANG && !VMS */
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
	{"Search index",		LYK_INDEX_SEARCH,	ENT_ONLY_DOC},
	{"Set Options",			LYK_OPTIONS,		ENT_ONLY_DOC},
	{"Activate this link",		LYK_SUBMIT,		ENT_ONLY_LINK},
	{"Download",			LYK_DOWNLOAD,		ENT_ONLY_LINK}
    };
    /* *INDENT-ON* */

#define TOTAL_MENUENTRIES	TABLESIZE(possible_entries)
    const char *choices[TOTAL_MENUENTRIES + 1];
    int actions[TOTAL_MENUENTRIES];

    int c, c1, retlac, filter_out = (atlink ? ENT_ONLY_DOC : ENT_ONLY_LINK);

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
		keysym = UPARROW;
		break;
	    case 'P':
		keysym = DNARROW;
		break;
	    case 'M':
		keysym = RTARROW;
		break;
	    case 'K':
		keysym = LTARROW;
		break;
	    case 'R':
		keysym = INSERT_KEY;
		break;
	    case 'S':
		keysym = REMOVE_KEY;
		break;
	    case 'I':
		keysym = PGUP;
		break;
	    case 'Q':
		keysym = PGDOWN;
		break;
	    case 'G':
		keysym = HOME;
		break;
	    case 'O':
		keysym = END_KEY;
		break;
	    case ';':
		keysym = F1;
		break;
	    }
	    return (keysym);
	}
#endif

	return (current_sl_modifier ? 0 : DO_NOTHING);
    }

    keysym = key->f.keysym;

#if defined (USE_MOUSE)
    if (keysym == MOUSE_KEYSYM)
	return sl_read_mouse_event(code);
#endif

    if (keysym < 0)
	return 0;

    if (keysym & (LKC_ISLECLAC | LKC_ISLAC))
	return (keysym);

    current_sl_modifier = 0;
    if (LKC_HAS_ESC_MOD(keysym)) {
	current_sl_modifier = LKC_MOD2;
	keysym &= LKC_MASK;
    }

    if (keysym + 1 >= KEYMAP_SIZE)
	return 0;

    return (keysym | current_sl_modifier);
}

/************************************************************************/
#else /* NOT  defined(USE_KEYMAPS) && defined(USE_SLANG) */

/*
 * LYgetch() translates some escape sequences and may fake noecho.
 */
#define found_CSI(first,second) ((second) == '[' || (first) == 155)

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

    CTRACE((tfp, "GETCH: Got %#x.\n", c));
#ifdef MISC_EXP
    if (LYNoZapKey > 1 && errno != EINTR &&
	(c == EOF
#ifdef USE_SLANG
	 || (c == 0xFFFF)
#endif
	)) {

	CTRACE((tfp,
		"nozap: Got EOF, curses %s, stdin is %p, LYNoZapKey reduced from %d to 0.\n",
		LYCursesON ? "on" : "off", stdin, LYNoZapKey));
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
	    c = UPARROW;
	    break;
	case 'B':
	    c = DNARROW;
	    break;
	case 'C':
	    c = RTARROW;
	    break;
	case 'D':
	    c = LTARROW;
	    break;
	case 'q':		/* vt100 application keypad 1 */
	    c = END_KEY;
	    break;
	case 'r':		/* vt100 application keypad 2 */
	    c = DNARROW;
	    break;
	case 's':		/* vt100 application keypad 3 */
	    c = PGDOWN;
	    break;
	case 't':		/* vt100 application keypad 4 */
	    c = LTARROW;
	    break;
	case 'v':		/* vt100 application keypad 6 */
	    c = RTARROW;
	    break;
	case 'w':		/* vt100 application keypad 7 */
	    c = HOME;
	    break;
	case 'x':		/* vt100 application keypad 8 */
	    c = UPARROW;
	    break;
	case 'y':		/* vt100 application keypad 9 */
	    c = PGUP;
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
		c = F1;
	    break;
	case 'u':
#ifdef VMS
	    if (b != 'O')
#endif /* VMS */
		c = F1;		/* macintosh help button */
	    break;
	case 'p':
#ifdef VMS
	    if (b == 'O')
#endif /* VMS */
		c = '0';	/* keypad 0 */
	    break;
	case '1':		/* VTxxx  Find  */
	    if (found_CSI(c, b) && (d = GetChar()) == '~')
		c = FIND_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '2':
	    if (found_CSI(c, b)) {
		if ((d = GetChar()) == '~')	/* VTxxx Insert */
		    c = INSERT_KEY;
		else if ((d == '8' ||
			  d == '9') &&
			 GetChar() == '~') {
		    if (d == '8')	/* VTxxx   Help */
			c = F1;
		    else if (d == '9')	/* VTxxx    Do  */
			c = DO_KEY;
		    d = -1;
		}
	    } else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '3':			     /** VTxxx Delete **/
	    if (found_CSI(c, b) && (d = GetChar()) == '~')
		c = REMOVE_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '4':			     /** VTxxx Select **/
	    if (found_CSI(c, b) && (d = GetChar()) == '~')
		c = SELECT_KEY;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '5':			     /** VTxxx PrevScreen **/
	    if (found_CSI(c, b) && (d = GetChar()) == '~')
		c = PGUP;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '6':			     /** VTxxx NextScreen **/
	    if (found_CSI(c, b) && (d = GetChar()) == '~')
		c = PGDOWN;
	    else
		done_esc = FALSE;	/* we have another look below - kw */
	    break;
	case '[':			     /** Linux F1-F5: ^[[[A etc. **/
	    if (found_CSI(c, b)) {
		if ((d = GetChar()) == 'A')
		    c = F1;
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
	if (isdigit(a) && found_CSI(c, b) && d != -1 && d != '~')
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
	    c = DNARROW;
	    break;
	case KEY_UP:
	    c = UPARROW;
	    break;
	case KEY_LEFT:
	    c = LTARROW;
	    break;
	case KEY_RIGHT:	/* ... */
	    c = RTARROW;
	    break;
#if defined(SH_EX) && defined(DOSPATH)	/* for NEC PC-9800 1998/08/30 (Sun) 21:50:35 */
	case KEY_C2:
	    c = DNARROW;
	    break;
	case KEY_A2:
	    c = UPARROW;
	    break;
	case KEY_B1:
	    c = LTARROW;
	    break;
	case KEY_B3:
	    c = RTARROW;
	    break;
	case PAD0:		/* PC-9800 Ins */
	    c = INSERT_KEY;
	    break;
	case PADSTOP:		/* PC-9800 DEL */
	    c = REMOVE_KEY;
	    break;
#endif /* SH_EX */
	case KEY_HOME:		/* Home key (upward+left arrow) */
	    c = HOME;
	    break;
	case KEY_CLEAR:	/* Clear screen */
	    c = 18;		/* CTRL-R */
	    break;
	case KEY_NPAGE:	/* Next page */
	    c = PGDOWN;
	    break;
	case KEY_PPAGE:	/* Previous page */
	    c = PGUP;
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
	    c = HOME;
	    break;
	case KEY_A3:		/* upper right of keypad */
	    c = PGUP;
	    break;
	case KEY_B2:		/* center of keypad */
	    c = DO_NOTHING;
	    break;
	case KEY_C1:		/* lower left of keypad */
	    c = END_KEY;
	    break;
	case KEY_C3:		/* lower right of keypad */
	    c = PGDOWN;
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
	    c = F1;
	    break;
#endif /* KEY_HELP */
#ifdef KEY_BACKSPACE
	case KEY_BACKSPACE:
	    c = CH_DEL;		/* backspace key (delete, not Ctrl-H)  S/390 -- gil -- 2041 */
	    break;
#endif /* KEY_BACKSPACE */
	case KEY_F(1):
	    c = F1;		/* VTxxx Help */
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
#if 0				/* assumption seems flawed? */
		/* Not detected by us or already processed by us.  It can
		 * happens that ncurses lags behind us in detecting the change,
		 * since its own SIGTSTP handler is not installed so detecting
		 * happened *at the end* of the last refresh.  Tell it to
		 * refresh again...  - kw
		 */
		LYrefresh();
#endif
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
		    if (c == LAC_TO_LKC0(LYK_SUBMIT) && code == FOR_INPUT)
			lac = LYK_SUBMIT;
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
		    if (lac == LYK_SUBMIT) {
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
#if 0				/* Probably not necessary any more - kw */
		    lynx_force_repaint();
		    LYrefresh();
#endif
		}
		if (code == FOR_INPUT && mouse_link == -1 &&
		    lac != LYK_REFRESH && lac != LYK_SUBMIT) {
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
			    c = LTARROW;
			    p = "<-";
			} else if (MOUSE_X_POS < HIST_CMD_2) {
			    c = RTARROW;
			    p = "->";
			} else if (MOUSE_X_POS > right) {
			    c = 'z';
			    p = "Cancel";
			} else {
			    c = PGDOWN;
			    p = "PGDOWN";
			}
		    } else if (MOUSE_Y_POS < V_CMD_AREA) {
			/* Screen TOP */
			if (MOUSE_X_POS < left) {
			    c = LTARROW;
			    p = "<-";
			} else if (MOUSE_X_POS < HIST_CMD_2) {
			    c = RTARROW;
			    p = "->";
			} else if (MOUSE_X_POS > right) {
			    c = 'z';
			    p = "Cancel";
			} else {
			    c = PGUP;
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
	    c = DNARROW;
	    break;
	case K_Up:
	case K_EUp:
	    c = UPARROW;
	    break;
	case K_Left:
	case K_ELeft:
	    c = LTARROW;
	    break;
	case K_Right:		/* ... */
	case K_ERight:
	    c = RTARROW;
	    break;
	case K_Home:		/* Home key (upward+left arrow) */
	case K_EHome:
	    c = HOME;
	    break;
	case K_PageDown:	/* Next page */
	case K_EPageDown:
	    c = PGDOWN;
	    break;
	case K_PageUp:		/* Previous page */
	case K_EPageUp:
	    c = PGUP;
	    break;
	case K_End:		/* home down or bottom (lower left) */
	case K_EEnd:
	    c = END_KEY;
	    break;
	case K_F1:		/* F1 key */
	    c = F1;
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
	    c = DNARROW;
	    break;
	case SL_KEY_UP:
	    c = UPARROW;
	    break;
	case SL_KEY_LEFT:
	    c = LTARROW;
	    break;
	case SL_KEY_RIGHT:	/* ... */
	    c = RTARROW;
	    break;
	case SL_KEY_HOME:	/* Home key (upward+left arrow) */
	case SL_KEY_A1:	/* upper left of keypad */
	    c = HOME;
	    break;
	case SL_KEY_NPAGE:	/* Next page */
	case SL_KEY_C3:	/* lower right of keypad */
	    c = PGDOWN;
	    break;
	case SL_KEY_PPAGE:	/* Previous page */
	case SL_KEY_A3:	/* upper right of keypad */
	    c = PGUP;
	    break;
	case SL_KEY_END:	/* home down or bottom (lower left) */
	case SL_KEY_C1:	/* lower left of keypad */
	    c = END_KEY;
	    break;
	case SL_KEY_F(1):	/* F1 key */
	    c = F1;
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

    if (c & (LKC_ISLAC | LKC_ISLECLAC))
	return (c);
    if ((c + 1) >= KEYMAP_SIZE) {
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

    for (i = 0; buffer[i]; i++)
#ifdef SUPPORT_MULTIBYTE_EDIT	/* 1998/11/23 (Mon) 17:04:55 */
    {
	if ((buffer[i] & 0x80) != 0
	    && buffer[i + 1] != 0) {
	    if ((kanji_code == SJIS) && IS_SJIS_X0201KANA(UCH((buffer[i])))) {
		continue;
	    }
	    i++;
	} else {
	    buffer[i] = UCH(TOLOWER(buffer[i]));
	}
    }
#else
	buffer[i] = TOLOWER(buffer[i]);
#endif
}

/*
 * Convert a null-terminated string to uppercase
 */
void LYUpperCase(char *arg_buffer)
{
    register unsigned char *buffer = (unsigned char *) arg_buffer;
    size_t i;

    for (i = 0; buffer[i]; i++)
#ifdef SUPPORT_MULTIBYTE_EDIT	/* 1998/11/23 (Mon) 17:05:10 */
    {
	if ((buffer[i] & 0x80) != 0
	    && buffer[i + 1] != 0) {
	    if ((kanji_code == SJIS) && IS_SJIS_X0201KANA(UCH((buffer[i])))) {
		continue;
	    }
	    i++;
	} else {
	    buffer[i] = UCH(TOUPPER(buffer[i]));
	}
    }
#else
	buffer[i] = UCH(TOUPPER(buffer[i]));
#endif
}

/*
 * Remove newlines from a string, returning true if we removed any.
 */
BOOLEAN LYRemoveNewlines(char *buffer)
{
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
	    return TRUE;
	}
    }
    return FALSE;
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
	return buf;
    }
    return NULL;
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

    LYstrncpy(buff, str, sizeof(buff) - 1);
    len = strlen(buff);
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
	return TRUE;
    }
    return FALSE;
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
 *  Display the current value of the string and allow the user
 *  to edit it.
 */

#define EDREC	 EditFieldData

/*
 * Shorthand to get rid of all most of the "edit->suchandsos".
 */
#define Buf	 edit->buffer
#define Pos	 edit->pos
#define StrLen	 edit->strlen
#define MaxLen	 edit->maxlen
#define DspWdth  edit->dspwdth
#define DspStart edit->xpan
#define Margin	 edit->margin
#ifdef ENHANCED_LINEEDIT
#define Mark	 edit->mark
#endif

#ifdef ENHANCED_LINEEDIT
static char killbuffer[1024] = "\0";
#endif

void LYSetupEdit(EDREC * edit, char *old,
		 int maxstr,
		 int maxdsp)
{
    /*
     * Initialize edit record
     */
    LYGetYX(edit->sy, edit->sx);
    edit->pad = ' ';
    edit->dirty = TRUE;
    edit->panon = FALSE;
    edit->current_modifiers = 0;

    MaxLen = maxstr;
    DspWdth = maxdsp;
    Margin = 0;
    Pos = strlen(old);
#ifdef ENHANCED_LINEEDIT
    Mark = -1;			/* pos=0, but do not show it yet */
#endif
    DspStart = 0;

    if (maxstr > maxdsp) {	/* Need panning? */
	if (DspWdth > 4)	/* Else "{}" take up precious screen space */
	    edit->panon = TRUE;

	/*
	 * Figure out margins.  If too big, we do a lot of unnecessary
	 * scrolling.  If too small, user doesn't have sufficient look-ahead. 
	 * Let's say 25% for each margin, upper bound is 10 columns.
	 */
	Margin = DspWdth / 4;
	if (Margin > 10)
	    Margin = 10;
    }

    LYstrncpy(edit->buffer, old, maxstr);
    StrLen = strlen(edit->buffer);
}

#ifdef SUPPORT_MULTIBYTE_EDIT

static int prev_pos(EDREC * edit, int pos)
{
    int i = 0;

    if (pos <= 0)
	return 0;
    if (HTCJK == NOCJK)
	return (pos - 1);
    else {
	while (i < pos - 1) {
	    int c;

	    c = Buf[i];
	    if (is8bits(c) &&
		!((kanji_code == SJIS) && IS_SJIS_X0201KANA(UCH(c)))) {
		i++;
	    }
	    i++;
	}
	if (i == pos)
	    return (i - 2);
	else
	    return i;
    }
}
#endif /* SUPPORT_MULTIBYTE_EDIT */

#ifdef EXP_KEYBOARD_LAYOUT
static int map_active = 0;

#else
#define map_active 0
#endif

int LYEditInsert(EDREC * edit, unsigned const char *s,
		 int len,
		 int map GCC_UNUSED,
		 BOOL maxMessage)
{
    int length = strlen(Buf);
    int remains = MaxLen - (length + len);
    int edited = 0, overflow = 0;

    /*
     * ch is (presumably) printable character.
     */
    if (remains < 0) {
	overflow = 1;
	len = 0;
	if (MaxLen > length)	/* Insert as much as we can */
	    len = MaxLen - length;
	else
	    goto finish;
    }
    Buf[length + len] = '\0';
    for (; length >= Pos; length--)	/* Make room */
	Buf[length + len] = Buf[length];
#ifdef EXP_KEYBOARD_LAYOUT
    if (map < 0)
	map = map_active;
    if (map && LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8) {
	int off = Pos;
	unsigned const char *e = s + len;
	char *tail = 0;

	while (s < e) {
	    char utfbuf[8];
	    int l = 1;

	    utfbuf[0] = *s;
	    if (*s < 128 && LYKbLayouts[current_layout][*s]) {
		UCode_t ucode = LYKbLayouts[current_layout][*s];

		if (ucode > 127) {
		    if (UCConvertUniToUtf8(ucode, utfbuf)) {
			l = strlen(utfbuf);
			remains -= l - 1;
			if (remains < 0) {
			    if (tail)
				strcpy(Buf + off, tail);
			    FREE(tail);
			    len = off;
			    overflow = 1;
			    goto finish;
			}
			if (l > 1 && !tail)
			    StrAllocCopy(tail, Buf + Pos + len);
		    } else
			utfbuf[0] = '?';
		} else
		    utfbuf[0] = UCH(ucode);
	    }
	    strncpy(Buf + off, utfbuf, l);
	    edited = 1;
	    off += l;
	    s++;
	}
	if (tail)
	    strcpy(Buf + off, tail);
	len = off - Pos;
	FREE(tail);
    } else if (map) {
	unsigned const char *e = s + len;
	unsigned char *t = (unsigned char *) Buf + Pos;

	while (s < e) {
	    int ch;

	    if (*s < 128 && LYKbLayouts[current_layout][*s]) {
		ch = UCTransUniChar(LYKbLayouts[current_layout][*s],
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
	strncpy(Buf + Pos, (const char *) s, len);
	edited = 1;
    }

  finish:
    Pos += len;
    StrLen += len;
    if (edited)
	edit->dirty = TRUE;
    if (overflow && maxMessage)
	_statusline(MAXLEN_REACHED_DEL_OR_MOV);
#ifdef ENHANCED_LINEEDIT
    if (Mark > Pos)
	Mark += len;
    else if (Mark < -1 - Pos)
	Mark -= len;
    if (Mark >= 0)
	Mark = -1 - Mark;	/* Disable it */
#endif
    return edited;
}

int LYEdit1(EDREC * edit, int ch,
	    int action,
	    BOOL maxMessage)
{				/* returns 0    character processed
				 *         -ch  if action should be performed outside of line-editing mode
				 *         ch   otherwise
				 */
    int i;
    int length;
    unsigned char uch;

    if (MaxLen <= 0)
	return (0);		/* Be defensive */

    length = strlen(&Buf[0]);
    StrLen = length;

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
	 * Hex 97.
	 * Fall through as a character for CJK, or if this is a valid character
	 * in the current display character set.  Otherwise, we treat this as
	 * LYE_ENTER.
	 */
	if (HTCJK == NOCJK && LYlowest_eightbit[current_char_set] > 0x97)
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
	 * etc.  done by slang on Unix and possibly some comm programs.  It's
	 * an imperfect workaround that doesn't work for all such characters.
	 */
	ch &= 0xFF;
	if (ch + 64 >= LYlowest_eightbit[current_char_set])
	    ch += 64;

	if (Pos <= (MaxLen) && StrLen < (MaxLen)) {
#ifdef ENHANCED_LINEEDIT
	    if (Mark > Pos)
		Mark++;
	    else if (Mark < -1 - Pos)
		Mark--;
	    if (Mark >= 0)
		Mark = -1 - Mark;	/* Disable it */
#endif
	    for (i = length; i >= Pos; i--)	/* Make room */
		Buf[i + 1] = Buf[i];
	    Buf[length + 1] = '\0';
	    Buf[Pos] = UCH(ch);
	    Pos++;
	} else {
	    if (maxMessage) {
		_statusline(MAXLEN_REACHED_DEL_OR_MOV);
	    }
	    return (ch);
	}
	break;

    case LYE_BACKW:
#ifndef SUPPORT_MULTIBYTE_EDIT
	/*
	 * Backword.
	 * Definition of word is very naive:  1 or more a/n characters.
	 */
	while (Pos && !isalnum(Buf[Pos - 1]))
	    Pos--;
	while (Pos && isalnum(Buf[Pos - 1]))
	    Pos--;
#else /* SUPPORT_MULTIBYTE_EDIT */
	/*
	 * Backword.
	 * Definition of word is very naive:  1 or more a/n characters, or 1 or
	 * more multibyte character.
	 */
	{
	    int pos0;

	    pos0 = prev_pos(edit, Pos);
	    while (Pos &&
		   (HTCJK == NOCJK || !is8bits(Buf[pos0])) &&
		   !isalnum(UCH(Buf[pos0]))) {
		Pos = pos0;
		pos0 = prev_pos(edit, Pos);
	    }
	    if (HTCJK != NOCJK && is8bits(Buf[pos0])) {
		while (Pos && is8bits(Buf[pos0])) {
		    Pos = pos0;
		    pos0 = prev_pos(edit, Pos);
		}
	    } else {
		while (Pos
		       && !is8bits(Buf[pos0])
		       && isalnum(UCH(Buf[pos0]))) {
		    Pos = pos0;
		    pos0 = prev_pos(edit, Pos);
		}
	    }
	}
#endif /* SUPPORT_MULTIBYTE_EDIT */
	break;

    case LYE_FORWW:
	/*
	 * Word forward.
	 */
#ifndef SUPPORT_MULTIBYTE_EDIT
	while (isalnum(Buf[Pos]))
	    Pos++;		/* '\0' is not a/n */
	while (!isalnum(Buf[Pos]) && Buf[Pos])
	    Pos++;
#else /* SUPPORT_MULTIBYTE_EDIT */
	if (HTCJK != NOCJK && is8bits(Buf[Pos])) {
	    while (is8bits(Buf[Pos]))
		Pos += 2;
	} else {
	    while (!is8bits(Buf[Pos]) && isalnum(Buf[Pos]))
		Pos++;		/* '\0' is not a/n */
	}
	while ((HTCJK == NOCJK || !is8bits(Buf[Pos])) &&
	       !isalnum(UCH(Buf[Pos])) && Buf[Pos])
	    Pos++;
#endif /* SUPPORT_MULTIBYTE_EDIT */
	break;

    case LYE_ERASE:
	/*
	 * Erase the line to start fresh.
	 */
	Buf[0] = '\0';
#ifdef ENHANCED_LINEEDIT
	Mark = -1;		/* Do not show the mark */
#endif
	/* fall through */

    case LYE_BOL:
	/*
	 * Go to first column.
	 */
	Pos = 0;
	break;

    case LYE_EOL:
	/*
	 * Go to last column.
	 */
	Pos = length;
	break;

    case LYE_DELNW:
	/*
	 * Delete next word.
	 */
	{
	    int pos0 = Pos;

	    LYEdit1(edit, 0, LYE_FORWW, FALSE);
	    while (Pos > pos0)
		LYEdit1(edit, 0, LYE_DELP, FALSE);
	}
	break;

    case LYE_DELPW:
	/*
	 * Delete previous word.
	 */
	{
	    int pos0 = Pos;

	    LYEdit1(edit, 0, LYE_BACKW, FALSE);
	    pos0 -= Pos;
	    while (pos0--)
		LYEdit1(edit, 0, LYE_DELN, FALSE);
	}
	break;

    case LYE_DELBL:
	/*
	 * Delete from current cursor position back to BOL.
	 */
	{
	    int pos0 = Pos;

	    while (pos0--)
		LYEdit1(edit, 0, LYE_DELP, FALSE);
	}
	break;

    case LYE_DELEL:		/* @@@ */
	/*
	 * Delete from current cursor position thru EOL.
	 */
	{
	    int pos0 = Pos;

	    LYEdit1(edit, 0, LYE_EOL, FALSE);
	    pos0 = Pos - pos0;
	    while (pos0--)
		LYEdit1(edit, 0, LYE_DELP, FALSE);
	}
	break;

    case LYE_DELN:
	/*
	 * Delete next character (I-beam style cursor), or current character
	 * (box/underline style cursor).
	 */
	if (Pos >= length)
	    break;
#ifdef SUPPORT_MULTIBYTE_EDIT
	if (HTCJK != NOCJK && is8bits(Buf[Pos]))
	    Pos++;
#endif
	Pos++;
	/* fall through - DO NOT RELOCATE the LYE_DELN case wrt LYE_DELP */

    case LYE_DELP:
	/*
	 * Delete preceding character.
	 */
#ifndef SUPPORT_MULTIBYTE_EDIT
	if (length == 0 || Pos == 0)
	    break;
#ifdef ENHANCED_LINEEDIT
	if (Mark >= 0)
	    Mark = -1 - Mark;	/* Disable it */
	if (Mark <= -1 - Pos)
	    Mark++;
#endif
	Pos--;
	for (i = Pos; i < length; i++)
	    Buf[i] = Buf[i + 1];
	i--;
#else /* SUPPORT_MULTIBYTE_EDIT */
	{
	    int offset = 1;
	    int pos0 = Pos;

	    if (length == 0 || Pos == 0)
		break;
	    if (HTCJK != NOCJK) {
		Pos = prev_pos(edit, pos0);
		offset = pos0 - Pos;
	    } else
		Pos--;
	    for (i = Pos; i < length; i++)
		Buf[i] = Buf[i + offset];
	    i -= offset;
#ifdef ENHANCED_LINEEDIT
	    if (Mark >= 0)
		Mark = -1 - Mark;	/* Disable it */
	    if (Mark <= -1 - Pos)
		Mark += offset;
#endif
	}
#endif /* SUPPORT_MULTIBYTE_EDIT */
	Buf[i] = 0;
	break;

    case LYE_FORW_RL:
    case LYE_FORW:
	/*
	 * Move cursor to the right.
	 */
#ifndef SUPPORT_MULTIBYTE_EDIT
	if (Pos < length)
	    Pos++;
#else /* SUPPORT_MULTIBYTE_EDIT */
	if (Pos < length) {
	    Pos++;
	    if (HTCJK != NOCJK && is8bits(Buf[Pos - 1]))
		Pos++;
	}
#endif /* SUPPORT_MULTIBYTE_EDIT */
	else if (action == LYE_FORW_RL)
	    return -ch;
	break;

    case LYE_BACK_LL:
    case LYE_BACK:
	/*
	 * Left-arrow move cursor to the left.
	 */
#ifndef SUPPORT_MULTIBYTE_EDIT
	if (Pos > 0)
	    Pos--;
#else /* SUPPORT_MULTIBYTE_EDIT */
	if (Pos > 0) {
	    if (HTCJK != NOCJK)
		Pos = prev_pos(edit, Pos);
	    else
		Pos--;
	}
#endif /* SUPPORT_MULTIBYTE_EDIT */
	else if (action == LYE_BACK_LL)
	    return -ch;
	break;

#ifdef ENHANCED_LINEEDIT
    case LYE_TPOS:
	/*
	 * Transpose characters - bash or ksh(emacs not gmacs) style
	 */
	if (length <= 1 || Pos == 0)
	    return (ch);
	if (Pos == length)
	    Pos--;
	if (Mark < 0)
	    Mark = -1 - Mark;	/* Temporary enable it */
	if (Mark == Pos || Mark == Pos + 1)
	    Mark = Pos - 1;
	if (Mark >= 0)
	    Mark = -1 - Mark;	/* Disable it */
	if (Buf[Pos - 1] == Buf[Pos]) {
	    Pos++;
	    break;
	}
	i = Buf[Pos - 1];
	Buf[Pos - 1] = Buf[Pos];
	Buf[Pos++] = (char) i;
	break;

    case LYE_SETMARK:
	/*
	 * primitive emacs-like set-mark-command
	 */
	Mark = Pos;
	return (0);

    case LYE_XPMARK:
	/*
	 * emacs-like exchange-point-and-mark
	 */
	if (Mark < 0)
	    Mark = -1 - Mark;	/* Enable it */
	if (Mark == Pos)
	    return (0);
	i = Pos;
	Pos = Mark;
	Mark = i;
	break;

    case LYE_KILLREG:
	/*
	 * primitive emacs-like kill-region
	 */
	if (Mark < 0)
	    Mark = -1 - Mark;	/* Enable it */
	if (Mark == Pos) {
	    killbuffer[0] = '\0';
	    return (0);
	}
	if (Mark > Pos)
	    LYEdit1(edit, 0, LYE_XPMARK, FALSE);
	{
	    int reglen = Pos - Mark;

	    LYstrncpy(killbuffer, &Buf[Mark],
		      HTMIN(reglen, (int) sizeof(killbuffer) - 1));
	    for (i = Mark; Buf[i + reglen]; i++)
		Buf[i] = Buf[i + reglen];
	    Buf[i] = Buf[i + reglen];	/* terminate */
	    Pos = Mark;
	}
	if (Mark >= 0)
	    Mark = -1 - Mark;	/* Disable it */
	break;

    case LYE_YANK:
	/*
	 * primitive emacs-like yank
	 */
	if (!killbuffer[0]) {
	    Mark = -1 - Pos;
	    return (0);
	} {
	    int yanklen = strlen(killbuffer);

	    if (Pos + yanklen <= (MaxLen) && StrLen + yanklen <= (MaxLen)) {
		Mark = -1 - Pos;

		for (i = length; i >= Pos; i--)		/* Make room */
		    Buf[i + yanklen] = Buf[i];
		for (i = 0; i < yanklen; i++)
		    Buf[Pos++] = UCH(killbuffer[i]);

	    } else if (maxMessage) {
		_statusline(MAXLEN_REACHED_DEL_OR_MOV);
	    }
	}
	break;

#endif /* ENHANCED_LINEEDIT */

    case LYE_UPPER:
	LYUpperCase(Buf);
	break;

    case LYE_LOWER:
	LYLowerCase(Buf);
	break;

    default:
	return (ch);
    }
    edit->dirty = TRUE;
    StrLen = strlen(&Buf[0]);
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
    char temp[120];
    char *p = temp;
    int num;

    /*
     * Load the c argument into the prompt buffer.
     */
    temp[0] = (char) *c;
    temp[1] = '\0';
    _statusline(msg);

    /*
     * Get the number, possibly with a suffix, from the user.
     */
    if (LYgetstr(temp, VISIBLE, sizeof(temp), NORECALL) < 0 || *temp == 0) {
	HTInfoMsg(CANCELLED);
	*c = '\0';
	*rel = '\0';
	return (0);
    }

    *rel = '\0';
    num = atoi(p);
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
    return num;
}

#ifdef USE_COLOR_STYLE
#  define TmpStyleOn(s)		curses_style((s), STACK_ON)
#  define TmpStyleOff(s)	curses_style((s), STACK_OFF)
#else
#  define TmpStyleOn(s)
#  define TmpStyleOff(s)
#endif /* defined USE_COLOR_STYLE */

void LYRefreshEdit(EDREC * edit)
{
    int i;
    int length;
    int nrdisplayed;
    int padsize;
    char *str;
    char buffer[3];

#ifdef SUPPORT_MULTIBYTE_EDIT
    int begin_multi = 0;
    int end_multi = 0;
#endif /* SUPPORT_MULTIBYTE_EDIT */
#ifdef USE_COLOR_STYLE
    int estyle, prompting = 0;
#endif

    buffer[0] = buffer[1] = buffer[2] = '\0';
    if (!edit->dirty || (DspWdth == 0))
	return;
    edit->dirty = FALSE;

    length = strlen(&Buf[0]);
    edit->strlen = length;
    /*
     * Now we have:
     *                .--DspWdth---.
     *      +---------+=============+-----------+
     *      |         |M           M|           |   (M=margin)
     *      +---------+=============+-----------+
     *      0         DspStart                   length
     *
     * Insertion point can be anywhere between 0 and stringlength.  Figure out
     * new display starting point.
     *
     * The first "if" below makes Lynx scroll several columns at a time when
     * extending the string.  Looks awful, but that way we can keep up with
     * data entry at low baudrates.
     */
    if ((DspStart + DspWdth) <= length) {
	if (Pos >= (DspStart + DspWdth) - Margin) {
#ifndef SUPPORT_MULTIBYTE_EDIT
	    DspStart = (Pos - DspWdth) + Margin;
#else /* SUPPORT_MULTIBYTE_EDIT */
	    if (HTCJK != NOCJK) {
		int tmp = (Pos - DspWdth) + Margin;

		while (DspStart < tmp) {
		    if (is8bits(Buf[DspStart]))
			DspStart++;
		    DspStart++;
		}
	    } else {
		DspStart = (Pos - DspWdth) + Margin;
	    }
#endif /* SUPPORT_MULTIBYTE_EDIT */
	}
    }

    if (Pos < DspStart + Margin) {
#ifndef SUPPORT_MULTIBYTE_EDIT
	DspStart = Pos - Margin;
	if (DspStart < 0)
	    DspStart = 0;
#else /* SUPPORT_MULTIBYTE_EDIT */
	if (HTCJK != NOCJK) {
	    int tmp = Pos - Margin;

	    DspStart = 0;
	    while (DspStart < tmp) {
		if (is8bits(Buf[DspStart]))
		    DspStart++;
		DspStart++;
	    }
	} else {
	    DspStart = Pos - Margin;
	    if (DspStart < 0)
		DspStart = 0;
	}
#endif /* SUPPORT_MULTIBYTE_EDIT */
    }

    str = &Buf[DspStart];
#ifdef SUPPORT_MULTIBYTE_EDIT
    if (HTCJK != NOCJK && is8bits(str[0]))
	begin_multi = 1;
#endif /* SUPPORT_MULTIBYTE_EDIT */

    nrdisplayed = length - DspStart;
    if (nrdisplayed > DspWdth)
	nrdisplayed = DspWdth;

    LYmove(edit->sy, edit->sx);
#ifdef USE_COLOR_STYLE
    /*
     * If this is the last screen line, set attributes to normal, should only
     * be needed for color styles.  The curses function may be used directly to
     * avoid complications.  - kw
     */
    if (edit->sy == (LYlines - 1))
	prompting = 1;
    if (prompting)
	estyle = s_prompt_edit;
    else
	estyle = s_aedit;
    CTRACE2(TRACE_STYLE,
	    (tfp, "STYLE.getstr: switching to <edit.%s>.\n",
	     prompting ? "prompt" : "active"));
    if (estyle != NOSTYLE)
	curses_style(estyle, STACK_ON);
    else
	wattrset(LYwin, A_NORMAL);	/* need to do something about colors? */
#endif
    if (edit->hidden) {
	for (i = 0; i < nrdisplayed; i++)
	    LYaddch('*');
    } else {
#if defined(ENHANCED_LINEEDIT) && defined(USE_COLOR_STYLE)
	if (Mark >= 0 && DspStart > Mark)
	    TmpStyleOn(prompting ? s_prompt_sel : s_aedit_sel);
#endif
	for (i = 0; i < nrdisplayed; i++) {
#if defined(ENHANCED_LINEEDIT) && defined(USE_COLOR_STYLE)
	    if (Mark >= 0 && ((DspStart + i == Mark && Pos > Mark)
			      || (DspStart + i == Pos && Pos < Mark)))
		TmpStyleOn(prompting ? s_prompt_sel : s_aedit_sel);
	    if (Mark >= 0 && ((DspStart + i == Mark && Pos < Mark)
			      || (DspStart + i == Pos && Pos > Mark)))
		TmpStyleOff(prompting ? s_prompt_sel : s_aedit_sel);
#endif
	    if ((buffer[0] = str[i]) == 1 || buffer[0] == 2 ||
		(UCH(buffer[0]) == 160 &&
		 !(HTPassHighCtrlRaw || HTCJK != NOCJK ||
		   (LYCharSet_UC[current_char_set].enc != UCT_ENC_8859 &&
		    !(LYCharSet_UC[current_char_set].like8859
		      & UCT_R_8859SPECL))))) {
		LYaddch(' ');
#ifdef SUPPORT_MULTIBYTE_EDIT
		end_multi = 0;
#endif /* SUPPORT_MULTIBYTE_EDIT */
	    } else {
		/* For CJK strings, by Masanobu Kimura */
		if (HTCJK != NOCJK && is8bits(buffer[0])) {
		    if (i < (nrdisplayed - 1))
			buffer[1] = str[++i];
#ifdef SUPPORT_MULTIBYTE_EDIT
		    end_multi = (i < nrdisplayed);
#if !(defined(USE_SLANG) || defined(WIDEC_CURSES))
		    {
			int ii, yy, xx;

			LYGetYX(yy, xx);
			for (ii = 0; buffer[ii] != '\0'; ++ii)
			    LYaddch(' ');
			LYrefresh();
			LYmove(yy, xx);
		    }
#endif /* USE_SLANG */
#endif /* SUPPORT_MULTIBYTE_EDIT */
		    LYaddstr(buffer);
		    buffer[1] = '\0';
		} else {
		    LYaddstr(buffer);
#ifdef SUPPORT_MULTIBYTE_EDIT
		    end_multi = 0;
#endif /* SUPPORT_MULTIBYTE_EDIT */
		}
	    }
	}
#if defined(ENHANCED_LINEEDIT) && defined(USE_COLOR_STYLE)
	if (Mark >= 0 &&
	    ((DspStart + nrdisplayed <= Mark && DspStart + nrdisplayed > Pos)
	     || (DspStart + nrdisplayed > Mark
		 && DspStart + nrdisplayed <= Pos)))
	    TmpStyleOff(prompting ? s_prompt_sel : s_aedit_sel);
#endif
    }

    /*
     * Erase rest of input area.
     */
    padsize = DspWdth - nrdisplayed;
    if (padsize) {
	TmpStyleOn(prompting ? s_prompt_edit_pad : s_aedit_pad);
	while (padsize--)
	    LYaddch(UCH(edit->pad));
	TmpStyleOff(prompting ? s_prompt_edit_pad : s_aedit_pad);
    }

    /*
     * Scrolling indicators.
     */
    if (edit->panon) {
	if ((DspStart + nrdisplayed) < length) {
	    int add_space = 0;

	    TmpStyleOn(prompting ? s_prompt_edit_arr : s_aedit_arr);
#ifdef SUPPORT_MULTIBYTE_EDIT
	    if (end_multi)
		add_space = 1;
#endif
	    LYmove(edit->sy, edit->sx + nrdisplayed - 1 - add_space);
	    if (add_space)
		LYaddch(' ');	/* Needed with styles? */
	    LYaddch(ACS_RARROW);
	    TmpStyleOff(prompting ? s_prompt_edit_arr : s_aedit_arr);
	}
	if (DspStart) {
	    TmpStyleOn(prompting ? s_prompt_edit_arr : s_aedit_arr);
	    LYmove(edit->sy, edit->sx);
	    LYaddch(ACS_LARROW);
#ifdef SUPPORT_MULTIBYTE_EDIT
	    if (begin_multi)
		LYaddch(' ');	/* Needed with styles? */
#endif /* SUPPORT_MULTIBYTE_EDIT */
	    TmpStyleOff(prompting ? s_prompt_edit_arr : s_aedit_arr);
	}
    }

    LYmove(edit->sy, edit->sx + Pos - DspStart);

#ifdef USE_COLOR_STYLE
    if (estyle != NOSTYLE)
	curses_style(estyle, STACK_OFF);
#endif
    LYrefresh();
}

static void reinsertEdit(EditFieldData *edit, char *result)
{
    if (result != 0) {
	LYEdit1(edit, '\0', LYE_ERASE, FALSE);
	while (*result != '\0') {
	    LYLineEdit(edit, (int) (*result), FALSE);
	    result++;
	}
    }
}

static int caselessCmpList(const void *a,
			   const void *b)
{
    return strcasecomp(*(const char *const *) a, *(const char *const *) b);
}

static int normalCmpList(const void *a,
			 const void *b)
{
    return strcmp(*(const char *const *) a, *(const char *const *) b);
}

static char **sortedList(HTList *list, BOOL ignorecase)
{
    unsigned count = HTList_count(list);
    unsigned j = 0;
    unsigned k, jk;
    char **result = typecallocn(char *, count + 1);

    if (result == 0)
	outofmem(__FILE__, "sortedList");

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

int LYarrayLength(const char **list)
{
    int result = 0;

    while (*list++ != 0)
	result++;
    return result;
}

int LYarrayWidth(const char **list)
{
    int result = 0;
    int check;

    while (*list != 0) {
	check = strlen(*list++);
	if (check > result)
	    result = check;
    }
    return result;
}

static void FormatChoiceNum(char *dst,
			    int num_choices,
			    int choice,
			    const char *value)
{
    if (num_choices >= 0) {
	int digits = (num_choices > 9) ? 2 : 1;

	sprintf(dst, "%*d: %.*s",
		digits, (choice + 1),
		MAX_LINE - 9 - digits, value);
    } else {
	LYstrncpy(dst, value, MAX_LINE - 1);
    }
}

static unsigned options_width(const char **list)
{
    unsigned width = 0;
    int count = 0;

    while (list[count] != 0) {
	if (strlen(list[count]) > width) {
	    width = strlen(list[count]);
	}
	count++;
    }
    return width;
}

static void draw_option(WINDOW * win, int entry,
			int width,
			BOOL reversed,
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
    SLsmg_write_nstring((char *) value, win->width);
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
		      const char **choices,
		      int width,
		      int i_length,
		      int disabled,
		      BOOLEAN for_mouse,
		      BOOLEAN numbered)
{
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
    static char prev_target[MAX_LINE];	/* Search string buffer */
    static char prev_target_buffer[MAX_LINE];	/* Next search buffer */
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
    const char **Cptr = NULL;

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
	*prev_target_buffer = '\0';
	first = FALSE;
    }
    *prev_target = '\0';
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
	width = options_width(choices);
    if (numbered) {
	sprintf(Cnum, "%d: ", num_choices);
	Lnum = strlen(Cnum);
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
    LYmove((LYlines - 2), 0);
    LYclrtoeol();
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
    LYbox(form_window, (BOOLEAN) !numbered);
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
		LYbox(form_window, (BOOLEAN) !numbered);
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
	    if (recall && *prev_target_buffer == '\0') {
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
		    LYstrncpy(prev_target_buffer,
			      cp,
			      sizeof(prev_target_buffer) - 1);
		    QueryNum = 0;
		    FirstRecall = FALSE;
		}
	    }
	    strcpy(prev_target, prev_target_buffer);
	    /* FALLTHRU */
	case LYK_WHEREIS:
	    if (*prev_target == '\0') {
		_statusline(ENTER_WHEREIS_QUERY);
		if ((ch = LYgetstr(prev_target, VISIBLE,
				   sizeof(prev_target_buffer),
				   recall)) < 0) {
		    /*
		     * User cancelled the search via ^G.  - FM
		     */
		    HTInfoMsg(CANCELLED);
		    goto restore_popup_statusline;
		}
	    }

	  check_recall:
	    if (*prev_target == '\0' &&
		!(recall && (ch == UPARROW || ch == DNARROW))) {
		/*
		 * No entry.  Simply break.  - FM
		 */
		HTInfoMsg(CANCELLED);
		goto restore_popup_statusline;
	    }

	    if (recall && ch == UPARROW) {
		if (FirstRecall) {
		    /*
		     * Use the current string or last query in the list.  - FM
		     */
		    FirstRecall = FALSE;
		    if (*prev_target_buffer) {
			for (QueryNum = (QueryTotal - 1);
			     QueryNum > 0; QueryNum--) {
			    if ((cp = (char *) HTList_objectAt(search_queries,
							       QueryNum))
				!= NULL &&
				!strcmp(prev_target_buffer, cp)) {
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
		    LYstrncpy(prev_target, cp, sizeof(prev_target) - 1);
		    if (*prev_target_buffer &&
			!strcmp(prev_target_buffer, prev_target)) {
			_statusline(EDIT_CURRENT_QUERY);
		    } else if ((*prev_target_buffer && QueryTotal == 2) ||
			       (!(*prev_target_buffer) &&
				QueryTotal == 1)) {
			_statusline(EDIT_THE_PREV_QUERY);
		    } else {
			_statusline(EDIT_A_PREV_QUERY);
		    }
		    if ((ch = LYgetstr(prev_target, VISIBLE,
				       sizeof(prev_target_buffer), recall)) < 0) {
			/*
			 * User cancelled the search via ^G.  - FM
			 */
			HTInfoMsg(CANCELLED);
			goto restore_popup_statusline;
		    }
		    goto check_recall;
		}
	    } else if (recall && ch == DNARROW) {
		if (FirstRecall) {
		    /*
		     * Use the current string or first query in the list.  - FM
		     */
		    FirstRecall = FALSE;
		    if (*prev_target_buffer) {
			for (QueryNum = 0;
			     QueryNum < (QueryTotal - 1); QueryNum++) {
			    if ((cp = (char *) HTList_objectAt(search_queries,
							       QueryNum))
				!= NULL &&
				!strcmp(prev_target_buffer, cp)) {
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
		    LYstrncpy(prev_target, cp, sizeof(prev_target) - 1);
		    if (*prev_target_buffer &&
			!strcmp(prev_target_buffer, prev_target)) {
			_statusline(EDIT_CURRENT_QUERY);
		    } else if ((*prev_target_buffer &&
				QueryTotal == 2) ||
			       (!(*prev_target_buffer) &&
				QueryTotal == 1)) {
			_statusline(EDIT_THE_PREV_QUERY);
		    } else {
			_statusline(EDIT_A_PREV_QUERY);
		    }
		    if ((ch = LYgetstr(prev_target, VISIBLE,
				       sizeof(prev_target_buffer),
				       recall)) < 0) {
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
	    strcpy(prev_target_buffer, prev_target);
	    HTAddSearchQuery(prev_target_buffer);

	    /*
	     * Start search at the next choice.  - FM
	     */
	    for (j = 1; Cptr[i + j] != NULL; j++) {
		FormatChoiceNum(buffer, max_choices, (i + j), Cptr[i + j]);
		if (case_sensitive) {
		    if (strstr(buffer, prev_target_buffer) != NULL)
			break;
		} else {
		    if (LYstrstr(buffer, prev_target_buffer) != NULL)
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
		HTUserMsg2(STRING_NOT_FOUND, prev_target_buffer);
		goto restore_popup_statusline;
	    }

	    /*
	     * Search from the beginning to the current choice.  - FM
	     */
	    for (j = 0; j < cur_choice; j++) {
		FormatChoiceNum(buffer, max_choices, (j + 1), Cptr[j]);
		if (case_sensitive) {
		    if (strstr(buffer, prev_target_buffer) != NULL)
			break;
		} else {
		    if (LYstrstr(buffer, prev_target_buffer) != NULL)
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
	    HTUserMsg2(STRING_NOT_FOUND, prev_target_buffer);

	  restore_popup_statusline:
	    /*
	     * Restore the popup statusline and reset the search variables.  -
	     * FM
	     */
	    _statusline(popup_status_msg);
	    *prev_target = '\0';
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

#define CurModif MyEdit.current_modifiers

int LYgetstr(char *inputline,
	     int hidden,
	     size_t bufsize,
	     RecallType recall)
{
    int x, y, MaxStringSize;
    int ch;
    int xlec = -2;
    int last_xlec = -1;
    int last_xlkc = -1;
    EditFieldData MyEdit;

#ifdef SUPPORT_MULTIBYTE_EDIT
    BOOL refresh_mb = TRUE;
#endif /* SUPPORT_MULTIBYTE_EDIT */

    LYGetYX(y, x);		/* Use screen from cursor position to eol */
    MaxStringSize = (bufsize < sizeof(MyEdit.buffer)) ?
	(bufsize - 1) : (sizeof(MyEdit.buffer) - 1);
    LYSetupEdit(&MyEdit, inputline, MaxStringSize, LYcolLimit - x);
    MyEdit.hidden = (BOOL) hidden;

    CTRACE((tfp, "called LYgetstr\n"));
    for (;;) {
      again:
#ifndef SUPPORT_MULTIBYTE_EDIT
	LYRefreshEdit(&MyEdit);
#else /* SUPPORT_MULTIBYTE_EDIT */
	if (refresh_mb)
	    LYRefreshEdit(&MyEdit);
#endif /* SUPPORT_MULTIBYTE_EDIT */
	ch = LYReadCmdKey(FOR_PROMPT);
#ifdef SUPPORT_MULTIBYTE_EDIT
#ifdef CJK_EX			/* for SJIS code */
	if (!refresh_mb
	    && (EditBinding(ch) != LYE_CHAR))
	    goto again;
#else
	if (!refresh_mb
	    && (EditBinding(ch) != LYE_CHAR)
	    && (EditBinding(ch) != LYE_AIX))
	    goto again;
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

	if (recall != NORECALL && (ch == UPARROW || ch == DNARROW)) {
	    LYstrncpy(inputline, MyEdit.buffer, (int) bufsize);
	    LYAddToCloset(recall, MyEdit.buffer);
	    CTRACE((tfp, "LYgetstr(%s) recall\n", inputline));
	    return (ch);
	}
	ch |= CurModif;
	CurModif = 0;
	if (last_xlkc != -1) {
	    if (ch == last_xlkc)
		ch |= LKC_MOD3;
	    last_xlkc = -1;	/* consumed */
	}
#ifndef WIN_EX
	if (LKC_TO_LAC(keymap, ch) == LYK_REFRESH)
	    goto again;
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
	    /*
	     * Set flag for modifier 1.
	     */
	    CurModif |= LKC_MOD1;
	    break;
	case LYE_SETM2:
	    /*
	     * Set flag for modifier 2.
	     */
	    CurModif |= LKC_MOD2;
	    break;
	case LYE_TAB:
	    if (xlec == last_xlec && recall != NORECALL) {
		HTList *list = whichRecall(recall);

		if (!HTList_isEmpty(list)) {
		    char **data = sortedList(list, (BOOL) (recall == RECALL_CMD));
		    int old_y, old_x;
		    int cur_choice = 0;
		    int num_options = LYarrayLength((const char **) data);

		    while (cur_choice < num_options
			   && strcasecomp(data[cur_choice], MyEdit.buffer) < 0)
			cur_choice++;

		    LYGetYX(old_y, old_x);
		    cur_choice = LYhandlePopupList(cur_choice,
						   0,
						   old_x,
						   (const char **) data,
						   -1,
						   -1,
						   FALSE,
						   FALSE,
						   TRUE);
		    if (cur_choice >= 0) {
			if (recall == RECALL_CMD)
			    _statusline(": ");
			reinsertEdit(&MyEdit, data[cur_choice]);
		    }
		    LYmove(old_y, old_x);
		    FREE(data);
		}
	    } else {
		reinsertEdit(&MyEdit, LYFindInCloset(recall, MyEdit.buffer));
	    }
	    break;

#ifndef CJK_EX			/* 1997/11/03 (Mon) 20:13:45 */
	case LYE_AIX:
	    /*
	     * Hex 97.
	     * Treat as a character for CJK, or if this is a valid character in
	     * the current display character set.  Otherwise, we treat this as
	     * LYE_ENTER.
	     */
	    if (ch != '\t' &&
		(HTCJK != NOCJK ||
		 LYlowest_eightbit[current_char_set] <= 0x97)) {
		LYLineEdit(&MyEdit, ch, FALSE);
		break;
	    }
	    /* FALLTHRU */
#endif
	case LYE_ENTER:
	    /*
	     * Terminate the string and return.
	     */
	    LYstrncpy(inputline, MyEdit.buffer, (int) bufsize);
	    if (!hidden)
		LYAddToCloset(recall, MyEdit.buffer);
	    CTRACE((tfp, "LYgetstr(%s) LYE_ENTER\n", inputline));
	    return (ch);

#ifdef CAN_CUT_AND_PASTE
	    /* 1998/10/01 (Thu) 15:05:49 */

	case LYE_PASTE:
	    {
		unsigned char *s = (unsigned char *) get_clip_grab(), *e;
		int len;

		if (!s)
		    break;
		len = strlen((const char *) s);
		e = s + len;

		if (len > 0) {
		    unsigned char *e1 = s;

		    while (e1 < e) {
			if (*e1 < ' ') {	/* Stop here? */
			    if (e1 > s)
				LYEditInsert(&MyEdit, s, e1 - s, map_active, TRUE);
			    s = e1;
			    if (*e1 == '\t') {	/* Replace by space */
				LYEditInsert(&MyEdit,
					     (unsigned const char *) " ",
					     1,
					     map_active,
					     TRUE);
				s = ++e1;
			    } else
				break;
			} else
			    ++e1;
		    }
		    if (e1 > s)
			LYEditInsert(&MyEdit, s, e1 - s, map_active, TRUE);
		}
		get_clip_release();
		break;
	    }
#endif

	case LYE_ABORT:
	    /*
	     * Control-C or Control-G aborts.
	     */
	    inputline[0] = '\0';
	    CTRACE((tfp, "LYgetstr LYE_ABORT\n"));
	    return (-1);

	case LYE_STOP:
	    /*
	     * Deactivate.
	     */
	    CTRACE((tfp, "LYgetstr LYE_STOP\n"));
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
	    textfields_need_activation = TRUE;
	    return (-1);
#else
#ifdef ENHANCED_LINEEDIT
	    if (Mark >= 0)
		Mark = -1 - Mark;	/* Disable it */
#endif
#endif
	    break;

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
	    LYLineEdit(&MyEdit, ch, FALSE);
#else /* SUPPORT_MULTIBYTE_EDIT */
	    if (LYLineEdit(&MyEdit, ch, FALSE) == 0) {
		if (refresh_mb && HTCJK != NOCJK && (0x81 <= ch) && (ch <= 0xfe))
		    refresh_mb = FALSE;
		else
		    refresh_mb = TRUE;
	    } else {
		if (!refresh_mb) {
		    LYEdit1(&MyEdit, 0, LYE_DELP, FALSE);
		}
	    }
#endif /* SUPPORT_MULTIBYTE_EDIT */
	}
    }
}

const char *LYLineeditHelpURL(void)
{
    static int lasthelp_lineedit = -1;
    static char helpbuf[LY_MAXPATH] = "\0";
    static char *phelp = &helpbuf[0];

    if (lasthelp_lineedit == current_lineedit)
	return &helpbuf[0];
    if (lasthelp_lineedit == -1) {
	LYstrncpy(helpbuf, helpfilepath, sizeof(helpbuf) - 1);
	phelp += strlen(helpbuf);
    }
    if (LYLineeditHelpURLs[current_lineedit] &&
	strlen(LYLineeditHelpURLs[current_lineedit]) &&
	(strlen(LYLineeditHelpURLs[current_lineedit]) <=
	 sizeof(helpbuf) - (phelp - helpbuf))) {
	LYstrncpy(phelp, LYLineeditHelpURLs[current_lineedit],
		  sizeof(helpbuf) - (phelp - helpbuf) - 1);
	lasthelp_lineedit = current_lineedit;
	return (&helpbuf[0]);
    }
    return NULL;
}

/*
 * A replacement for 'strsep()'
 */
char *LYstrsep(char **stringp,
	       const char *delim)
{
    char *tmp, *out;

    if (isEmpty(stringp))	/* nothing to do? */
	return 0;		/* then don't fall on our faces */

    out = *stringp;		/* save the start of the string */
    tmp = strpbrk(*stringp, delim);
    if (tmp) {
	*tmp = '\0';		/* terminate the substring with \0 */
	*stringp = ++tmp;	/* point at the next substring */
    } else
	*stringp = 0;		/* this was the last substring: */
    /* let caller see he's done */
    return out;
}

/*
 * LYstrstr will find the first occurrence of the string pointed to by tarptr
 * in the string pointed to by chptr.  It returns NULL if string not found.  It
 * is a case insensitive search.
 */
char *LYstrstr(char *chptr,
	       const char *tarptr)
{
    int len = strlen(tarptr);

    for (; *chptr != '\0'; chptr++) {
	if (0 == UPPER8(*chptr, *tarptr)) {
	    if (0 == strncasecomp8(chptr + 1, tarptr + 1, len - 1))
		return (chptr);
	}
    }				/* end for */

    return (NULL);		/* string not found or initial chptr was empty */
}

/*
 * LYno_attr_char_case_strstr will find the first occurrence of the
 * string pointed to by tarptr in the string pointed to by chptr.
 * It ignores the characters:  LY_UNDERLINE_START_CHAR and
 *			       LY_UNDERLINE_END_CHAR
 *			       LY_BOLD_START_CHAR
 *			       LY_BOLD_END_CHAR
 *			       LY_SOFT_HYPHEN
 *			       if present in chptr.
 * It is a case insensitive search.
 */
const char *LYno_attr_char_case_strstr(const char *chptr,
				       const char *tarptr)
{
    register const char *tmpchptr, *tmptarptr;

    if (!chptr)
	return (NULL);

    while (IsSpecialAttrChar(*chptr) && *chptr != '\0')
	chptr++;

    for (; *chptr != '\0'; chptr++) {
	if (0 == UPPER8(*chptr, *tarptr)) {
	    /*
	     * See if they line up.
	     */
	    tmpchptr = chptr + 1;
	    tmptarptr = tarptr + 1;

	    if (*tmptarptr == '\0')	/* one char target */
		return (chptr);

	    while (1) {
		if (!IsSpecialAttrChar(*tmpchptr)) {
		    if (0 != UPPER8(*tmpchptr, *tmptarptr))
			break;
		    tmpchptr++;
		    tmptarptr++;
		} else {
		    tmpchptr++;
		}
		if (*tmptarptr == '\0')
		    return (chptr);
		if (*tmpchptr == '\0')
		    break;
	    }
	}
    }				/* end for */

    return (NULL);
}

/*
 * LYno_attr_char_strstr will find the first occurrence of the
 * string pointed to by tarptr in the string pointed to by chptr.
 * It ignores the characters:  LY_UNDERLINE_START_CHAR and
 *			       LY_UNDERLINE_END_CHAR
 *			       LY_BOLD_START_CHAR
 *			       LY_BOLD_END_CHAR
 *			       LY_SOFT_HYPHEN
 *			       if present in chptr.
 * It is a case sensitive search.
 */
const char *LYno_attr_char_strstr(const char *chptr,
				  const char *tarptr)
{
    register const char *tmpchptr, *tmptarptr;

    if (!chptr)
	return (NULL);

    while (IsSpecialAttrChar(*chptr) && *chptr != '\0')
	chptr++;

    for (; *chptr != '\0'; chptr++) {
	if ((*chptr) == (*tarptr)) {
	    /*
	     * See if they line up.
	     */
	    tmpchptr = chptr + 1;
	    tmptarptr = tarptr + 1;

	    if (*tmptarptr == '\0')	/* one char target */
		return (chptr);

	    while (1) {
		if (!IsSpecialAttrChar(*tmpchptr)) {
		    if ((*tmpchptr) != (*tmptarptr))
			break;
		    tmpchptr++;
		    tmptarptr++;
		} else {
		    tmpchptr++;
		}
		if (*tmptarptr == '\0')
		    return (chptr);
		if (*tmpchptr == '\0')
		    break;
	    }
	}
    }				/* end for */

    return (NULL);
}

/*
 * LYno_attr_mbcs_case_strstr will find the first occurrence of the string
 * pointed to by tarptr in the string pointed to by chptr.  It takes account of
 * MultiByte Character Sequences (UTF8).  The physical lengths of the displayed
 * string up to the start and end (= next position after) of the target string
 * are returned in *nstartp and *nendp if the search is successful.
 *
 * These lengths count glyph cells if count_gcells is set.  (Full-width
 * characters in CJK mode count as two.) Normally that's what we want.  They
 * count actual glyphs if count_gcells is unset.  (Full-width characters in CJK
 * mode count as one.)
 *
 * It ignores the characters: LY_UNDERLINE_START_CHAR and
 *			      LY_UNDERLINE_END_CHAR
 *			      LY_BOLD_START_CHAR
 *			      LY_BOLD_END_CHAR
 *			      LY_SOFT_HYPHEN
 *			      if present in chptr.
 * It assumes UTF8 if utf_flag is set.
 * It is a case insensitive search.  - KW & FM
 */
const char *LYno_attr_mbcs_case_strstr(const char *chptr,
				       const char *tarptr,
				       BOOL utf_flag,
				       BOOL count_gcells,
				       int *nstartp,
				       int *nendp)
{
    const char *tmpchptr;
    const char *tmptarptr;
    int len = 0;
    int offset;

    if (!(chptr && tarptr))
	return (NULL);

    /*
     * Skip initial IsSpecial chars.  - FM
     */
    while (IsSpecialAttrChar(*chptr) && *chptr != '\0')
	chptr++;

    /*
     * Seek a first target match.  - FM
     */
    for (; *chptr != '\0'; chptr++) {
	if ((!utf_flag && HTCJK != NOCJK && is8bits(*chptr) &&
	     *chptr == *tarptr &&
	     IsNormalChar(*(chptr + 1))) ||
	    (0 == UPPER8(*chptr, *tarptr))) {
	    int tarlen = 0;

	    offset = len;
	    len++;

	    /*
	     * See if they line up.
	     */
	    tmpchptr = (chptr + 1);
	    tmptarptr = (tarptr + 1);

	    if (*tmptarptr == '\0') {
		/*
		 * One char target.
		 */
		if (nstartp)
		    *nstartp = offset;
		if (nendp)
		    *nendp = len;
		return (chptr);
	    }
	    if (!utf_flag && HTCJK != NOCJK && is8bits(*chptr) &&
		*chptr == *tarptr &&
		IsNormalChar(*tmpchptr)) {
		/*
		 * Check the CJK multibyte.  - FM
		 */
		if (*tmpchptr == *tmptarptr) {
		    /*
		     * It's a match.  Advance to next char.  - FM
		     */
		    tmpchptr++;
		    tmptarptr++;
		    if (count_gcells)
			tarlen++;
		    if (*tmptarptr == '\0') {
			/*
			 * One character match.  - FM
			 */
			if (nstartp)
			    *nstartp = offset;
			if (nendp)
			    *nendp = len + tarlen;
			return (chptr);
		    }
		} else {
		    /*
		     * It's not a match, so go back to seeking a first target
		     * match.  - FM
		     */
		    chptr++;
		    if (count_gcells)
			len++;
		    continue;
		}
	    }
	    /*
	     * See if the rest of the target matches.  - FM
	     */
	    while (1) {
		if (!IsSpecialAttrChar(*tmpchptr)) {
		    if (!utf_flag && HTCJK != NOCJK && is8bits(*tmpchptr)) {
			if (*tmpchptr == *tmptarptr &&
			    *(tmpchptr + 1) == *(tmptarptr + 1) &&
			    !IsSpecialAttrChar(*(tmpchptr + 1))) {
			    tmpchptr++;
			    tmptarptr++;
			    if (count_gcells)
				tarlen++;
			} else {
			    break;
			}
		    } else if (0 != UPPER8(*tmpchptr, *tmptarptr)) {
			break;
		    }

		    if (!IS_UTF_EXTRA(*tmptarptr)) {
			tarlen++;
		    }
		    tmpchptr++;
		    tmptarptr++;

		} else {
		    tmpchptr++;
		}

		if (*tmptarptr == '\0') {
		    if (nstartp)
			*nstartp = offset;
		    if (nendp)
			*nendp = len + tarlen;
		    return (chptr);
		}
		if (*tmpchptr == '\0')
		    break;
	    }
	} else if (!(IS_UTF_EXTRA(*chptr) ||
		     IsSpecialAttrChar(*chptr))) {
	    if (!utf_flag && HTCJK != NOCJK && is8bits(*chptr) &&
		IsNormalChar(*(chptr + 1))) {
		chptr++;
		if (count_gcells)
		    len++;
	    }
	    len++;
	}
    }				/* end for */

    return (NULL);
}

/*
 * LYno_attr_mbcs_strstr will find the first occurrence of the string pointed
 * to by tarptr in the string pointed to by chptr.
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
 * It ignores the characters: LY_UNDERLINE_START_CHAR and
 *			      LY_UNDERLINE_END_CHAR
 *			      LY_BOLD_START_CHAR
 *			      LY_BOLD_END_CHAR
 *			      LY_SOFT_HYPHEN
 *			      if present in chptr.
 * It assumes UTF8 if utf_flag is set.
 * It is a case sensitive search.  - KW & FM
 */
const char *LYno_attr_mbcs_strstr(const char *chptr,
				  const char *tarptr,
				  BOOL utf_flag,
				  BOOL count_gcells,
				  int *nstartp,
				  int *nendp)
{
    const char *tmpchptr;
    const char *tmptarptr;
    int len = 0;
    int offset;

    if (!(chptr && tarptr))
	return (NULL);

    /*
     * Skip initial IsSpecial chars.  - FM
     */
    while (IsSpecialAttrChar(*chptr) && *chptr != '\0')
	chptr++;

    /*
     * Seek a first target match.  - FM
     */
    for (; *chptr != '\0'; chptr++) {
	if ((*chptr) == (*tarptr)) {
	    int tarlen = 0;

	    offset = len;
	    len++;

	    /*
	     * See if they line up.
	     */
	    tmpchptr = (chptr + 1);
	    tmptarptr = (tarptr + 1);

	    if (*tmptarptr == '\0') {
		/*
		 * One char target.
		 */
		if (nstartp)
		    *nstartp = offset;
		if (nendp)
		    *nendp = len;
		return (chptr);
	    }
	    if (!utf_flag && HTCJK != NOCJK && is8bits(*chptr) &&
		IsNormalChar(*tmpchptr)) {
		/*
		 * Check the CJK multibyte.  - FM
		 */
		if (*tmpchptr == *tmptarptr) {
		    /*
		     * It's a match.  Advance to next char.  - FM
		     */
		    tmpchptr++;
		    tmptarptr++;
		    if (count_gcells)
			tarlen++;
		    if (*tmptarptr == '\0') {
			/*
			 * One character match.  - FM
			 */
			if (nstartp)
			    *nstartp = offset;
			if (nendp)
			    *nendp = len + tarlen;
			return (chptr);
		    }
		} else {
		    /*
		     * It's not a match, so go back to seeking a first target
		     * match.  - FM
		     */
		    chptr++;
		    if (count_gcells)
			len++;
		    continue;
		}
	    }
	    /*
	     * See if the rest of the target matches.  - FM
	     */
	    while (1) {
		if (!IsSpecialAttrChar(*tmpchptr)) {
		    if (!utf_flag && HTCJK != NOCJK && is8bits(*tmpchptr)) {
			if (*tmpchptr == *tmptarptr &&
			    *(tmpchptr + 1) == *(tmptarptr + 1) &&
			    !IsSpecialAttrChar(*(tmpchptr + 1))) {
			    tmpchptr++;
			    tmptarptr++;
			    if (count_gcells)
				tarlen++;
			} else {
			    break;
			}
		    } else if ((*tmpchptr) != (*tmptarptr)) {
			break;
		    }

		    if (!IS_UTF_EXTRA(*tmptarptr)) {
			tarlen++;
		    }
		    tmpchptr++;
		    tmptarptr++;
		} else {
		    tmpchptr++;
		}

		if (*tmptarptr == '\0') {
		    if (nstartp)
			*nstartp = offset;
		    if (nendp)
			*nendp = len + tarlen;
		    return (chptr);
		}
		if (*tmpchptr == '\0')
		    break;
	    }
	} else if (!(IS_UTF_EXTRA(*chptr) ||
		     IsSpecialAttrChar(*chptr))) {
	    if (!utf_flag && HTCJK != NOCJK && is8bits(*chptr) &&
		IsNormalChar(*(chptr + 1))) {
		chptr++;
		if (count_gcells)
		    len++;
	    }
	    len++;
	}
    }				/* end for */

    return (NULL);
}

/*
 * Allocate a new copy of a string, and returns it.
 */
char *SNACopy(char **dest,
	      const char *src,
	      int n)
{
    FREE(*dest);
    if (src) {
	*dest = typeMallocn(char, n + 1);

	if (*dest == NULL) {
	    CTRACE((tfp, "Tried to malloc %d bytes\n", n));
	    outofmem(__FILE__, "SNACopy");
	}
	strncpy(*dest, src, n);
	*(*dest + n) = '\0';	/* terminate */
    }
    return *dest;
}

/*
 * String Allocate and Concatenate.
 */
char *SNACat(char **dest,
	     const char *src,
	     int n)
{
    if (non_empty(src)) {
	if (*dest) {
	    int length = strlen(*dest);

	    *dest = (char *) realloc(*dest, length + n + 1);
	    if (*dest == NULL)
		outofmem(__FILE__, "SNACat");
	    strncpy(*dest + length, src, n);
	    *(*dest + length + n) = '\0';	/* terminate */
	} else {
	    *dest = typeMallocn(char, n + 1);

	    if (*dest == NULL)
		outofmem(__FILE__, "SNACat");
	    memcpy(*dest, src, n);
	    (*dest)[n] = '\0';	/* terminate */
	}
    }
    return *dest;
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

    /*
     * Make check for sure.
     */
    if (upper <= 0)
	return (upper);

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
	if (diff < 0)
	    low = i + 1;
	if (diff > 0)
	    high = i;
	if (diff == 0)
	    return (unicode_to_lower_case[i].lower);
    }

    return (upper);		/* if we came here */
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
    /* if they are the same or one is a null characters return immediately. */
    if (ch1 == ch2)
	return 0;
    if (!ch2)
	return UCH(ch1);
    else if (!ch1)
	return -UCH(ch2);

    /* case-insensitive match for us-ascii */
    if (UCH(TOASCII(ch1)) < 128 && UCH(TOASCII(ch2)) < 128)
	return (TOUPPER(ch1) - TOUPPER(ch2));

    /* case-insensitive match for upper half */
    if (UCH(TOASCII(ch1)) > 127 &&	/* S/390 -- gil -- 2066 */
	UCH(TOASCII(ch2)) > 127) {
	if (DisplayCharsetMatchLocale)
	    return (TOUPPER(ch1) - TOUPPER(ch2));	/* old-style */
	else {
	    long uni_ch2 = UCTransToUni((char) ch2, current_char_set);
	    long uni_ch1;

	    if (uni_ch2 < 0)
		return UCH(ch1);
	    uni_ch1 = UCTransToUni((char) ch1, current_char_set);
	    return (UniToLowerCase(uni_ch1) - UniToLowerCase(uni_ch2));
	}
    }

    return (-10);		/* mismatch, if we come to here */
}

/*
 * Replaces 'fgets()' calls into a fixed-size buffer with reads into a buffer
 * that is allocated.  When an EOF or error is found, the buffer is freed
 * automatically.
 */
char *LYSafeGets(char **src,
		 FILE *fp)
{
    char buffer[BUFSIZ];
    char *result = 0;

    if (src != 0)
	result = *src;
    if (result != 0)
	*result = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	if (*buffer)
	    result = StrAllocCat(result, buffer);
	if (strchr(buffer, '\n') != 0)
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
    if (src != 0)
	*src = result;
    return result;
}

#ifdef EXP_CMD_LOGGING
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
	unsigned len;

	while ((ch < 0) && LYSafeGets(&buffer, cmd_script) != 0) {
	    LYTrimTrailing(buffer);
	    src = LYSkipBlanks(buffer);
	    tmp = LYSkipNonBlanks(src);
	    switch (len = (tmp - src)) {
	    case 4:
		if (!strncasecomp(src, "exit", 4))
		    exit_immediately(0);
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
		    CTRACE((tfp, "LYSetConfigValue(%s, %s)\n", src, tmp));
		    LYSetConfigValue(src, tmp);
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
#endif /* EXP_CMD_LOGGING */
