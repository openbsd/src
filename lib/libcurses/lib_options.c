
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/


/*
**	lib_options.c
**
**	The routines to handle option setting.
**
*/

#include <curses.priv.h>

#include <term.h>	/* keypad_xmit, keypad_local, meta_on, meta_off */
			/* cursor_visible,cursor_normal,cursor_invisible */

MODULE_ID("Id: lib_options.c,v 1.22 1997/05/01 23:46:18 Alexander.V.Lukyanov Exp $")

int has_ic(void)
{
	T((T_CALLED("has_ic()")));
	returnCode((insert_character || parm_ich
	   ||  (enter_insert_mode && exit_insert_mode))
	   &&  (delete_character || parm_dch));
}

int has_il(void)
{
	T((T_CALLED("has_il()")));
	returnCode((insert_line || parm_insert_line)
		&& (delete_line || parm_delete_line));
}

int idlok(WINDOW *win,  bool flag)
{
	T((T_CALLED("idlok(%p,%d)"), win, flag));

	_nc_idlok = win->_idlok = flag && (has_il() || change_scroll_region);
	returnCode(OK);
}


void idcok(WINDOW *win, bool flag)
{
	T((T_CALLED("idcok(%p,%d)"), win, flag));

	_nc_idcok = win->_idcok = flag && has_ic();

	returnVoid;
}


int clearok(WINDOW *win, bool flag)
{
	T((T_CALLED("clearok(%p,%d)"), win, flag));

	win->_clear = flag;
	returnCode(OK);
}


void immedok(WINDOW *win, bool flag)
{
	T((T_CALLED("immedok(%p,%d)"), win, flag));

	win->_immed = flag;

	returnVoid;
}

int leaveok(WINDOW *win, bool flag)
{
	T((T_CALLED("leaveok(%p,%d)"), win, flag));

	win->_leaveok = flag;
	if (flag == TRUE)
		curs_set(0);
	else
		curs_set(1);
	returnCode(OK);
}


int scrollok(WINDOW *win, bool flag)
{
	T((T_CALLED("scrollok(%p,%d)"), win, flag));

	win->_scroll = flag;
	returnCode(OK);
}

int halfdelay(int t)
{
	T((T_CALLED("halfdelay(%d)"), t));

	if (t < 1 || t > 255)
		returnCode(ERR);

	cbreak();
	SP->_cbreak = t+1;
	returnCode(OK);
}

int nodelay(WINDOW *win, bool flag)
{
	T((T_CALLED("nodelay(%p,%d)"), win, flag));

	if (flag == TRUE)
		win->_delay = 0;
	else win->_delay = -1;
	returnCode(OK);
}

int notimeout(WINDOW *win, bool f)
{
	T((T_CALLED("notimout(%p,%d)"), win, f));

	win->_notimeout = f;
	returnCode(OK);
}

int wtimeout(WINDOW *win, int delay)
{
	T((T_CALLED("wtimeout(%p,%d)"), win, delay));

	win->_delay = delay;
	returnCode(OK);
}

static void init_keytry(void);
static void add_to_try(char *, short);

/* Turn the keypad on/off
 *
 * Note:  we flush the output because changing this mode causes some terminals
 * to emit different escape sequences for cursor and keypad keys.  If we don't
 * flush, then the next wgetch may get the escape sequence that corresponds to
 * the terminal state _before_ switching modes.
 */
int _nc_keypad(bool flag)
{
	if (flag  &&  keypad_xmit)
	{
	    TPUTS_TRACE("keypad_xmit");
	    putp(keypad_xmit);
	    (void) fflush(SP->_ofp);
	}
	else if (! flag  &&  keypad_local)
	{
	    TPUTS_TRACE("keypad_local");
	    putp(keypad_local);
	    (void) fflush(SP->_ofp);
	}

	if (SP->_keytry == UNINITIALISED)
	    init_keytry();
	return(OK);
}

int keypad(WINDOW *win, bool flag)
{
	T((T_CALLED("keypad(%p,%d)"), win, flag));

	win->_use_keypad = flag;
	returnCode(_nc_keypad(flag));
}


int meta(WINDOW *win GCC_UNUSED, bool flag)
{
	T((T_CALLED("meta(%p,%d)"), win, flag));

	SP->_use_meta = flag;

	if (flag  &&  meta_on)
	{
	    TPUTS_TRACE("meta_on");
	    putp(meta_on);
	}
	else if (! flag  &&  meta_off)
	{
	    TPUTS_TRACE("meta_off");
	    putp(meta_off);
	}
	returnCode(OK);
}

/* curs_set() moved here to narrow the kernel interface */

int curs_set(int vis)
{
int cursor = SP->_cursor;

	T((T_CALLED("curs_set(%d)"), vis));

	if (vis < 0 || vis > 2)
		returnCode(ERR);

	if (vis == cursor)
		returnCode(cursor);

	switch(vis) {
	case 2:
		if (cursor_visible)
		{
			TPUTS_TRACE("cursor_visible");
			putp(cursor_visible);
		}
		else
			returnCode(ERR);
		break;
	case 1:
		if (cursor_normal)
		{
			TPUTS_TRACE("cursor_normal");
			putp(cursor_normal);
		}
		else
			returnCode(ERR);
		break;
	case 0:
		if (cursor_invisible)
		{
			TPUTS_TRACE("cursor_invisible");
			putp(cursor_invisible);
		}
		else
			returnCode(ERR);
		break;
	}
	SP->_cursor = vis;
	(void) fflush(SP->_ofp);

	returnCode(cursor==-1 ? 1 : cursor);
}

/*
**      init_keytry()
**
**      Construct the try for the current terminal's keypad keys.
**
*/


static struct  tries *newtry;

static void init_keytry(void)
{
	newtry = 0;

/* LINT_PREPRO
#if 0*/
#include <keys.tries>
/* LINT_PREPRO
#endif*/

	SP->_keytry = newtry;
}


static void add_to_try(char *str, short code)
{
static bool     out_of_memory = FALSE;
struct tries    *ptr, *savedptr;

	if (! str  ||  out_of_memory)
		return;

	if (newtry != 0) {
		ptr = savedptr = newtry;

		for (;;) {
			while (ptr->ch != (unsigned char) *str
			       &&  ptr->sibling != 0)
				ptr = ptr->sibling;
	
			if (ptr->ch == (unsigned char) *str) {
				if (*(++str)) {
					if (ptr->child != 0)
						ptr = ptr->child;
					else
						break;
				} else {
					ptr->value = code;
					return;
				}
			} else {
				if ((ptr->sibling = typeCalloc(struct tries,1)) == 0) {
					out_of_memory = TRUE;
					return;
				}

				savedptr = ptr = ptr->sibling;
				if (*str == '\200')
					ptr->ch = '\0';
				else
					ptr->ch = (unsigned char) *str;
				str++;
				ptr->value = 0;

				break;
			}
		} /* end for (;;) */
	} else {   /* newtry == 0 :: First sequence to be added */
		savedptr = ptr = newtry = typeCalloc(struct tries,1);

		if (ptr == 0) {
			out_of_memory = TRUE;
				return;
		}

		if (*str == '\200')
			ptr->ch = '\0';
		else
			ptr->ch = (unsigned char) *str;
		str++;
		ptr->value = 0;
	}

	    /* at this point, we are adding to the try.  ptr->child == 0 */

	while (*str) {
		ptr->child = typeCalloc(struct tries,1);

		ptr = ptr->child;

		if (ptr == 0) {
			out_of_memory = TRUE;

			ptr = savedptr;
			while (ptr != 0) {
				savedptr = ptr->child;
				free(ptr);
				ptr = savedptr;
			}

			return;
		}

		if (*str == '\200')
			ptr->ch = '\0';
		else
			ptr->ch = (unsigned char) *str;
		str++;
		ptr->value = 0;
	}

	ptr->value = code;
	return;
}

int typeahead(int fd)
{
	T((T_CALLED("typeahead(%d)"), fd));
	SP->_checkfd = fd;
	returnCode(OK);
}

/*
**      has_key()
**
**      Return TRUE if the current terminal has the given key
**
*/


static int has_key_internal(int keycode, struct tries *tp)
{
    if (!tp)
	return(FALSE);
    else if (tp->value == keycode)
	return(TRUE);
    else
	return(has_key_internal(keycode, tp->child)
	       || has_key_internal(keycode, tp->sibling));
}

int has_key(int keycode)
{
    T((T_CALLED("has_key(%d)"), keycode));
    returnCode(has_key_internal(keycode, SP->_keytry));
}
