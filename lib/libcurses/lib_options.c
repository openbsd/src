
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

#include "curses.priv.h"
#include <stdlib.h>
#include "term.h"	/* keypad_xmit, keypad_local, meta_on, meta_off */
			/* cursor_visible,cursor_normal,cursor_invisible */

int has_ic(void)
{
	T(("has_ic() called"));
	return (insert_character || parm_ich)
		&& (delete_character || parm_dch);
}

int has_il(void)
{
	T(("has_il() called"));
	return (insert_line || parm_insert_line)
		&& (delete_line || parm_delete_line);
}

int idlok(WINDOW *win,  bool flag)
{
	T(("idlok(%p,%d) called", win, flag));

   	win->_idlok = flag && (has_il() || change_scroll_region);
	return OK; 
}


int idcok(WINDOW *win, bool flag)
{
	T(("idcok(%p,%d) called", win, flag));

	win->_idcok = flag && has_ic();

	return OK; 
}


int clearok(WINDOW *win, bool flag)
{
	T(("clearok(%p,%d) called", win, flag));

   	if (win == curscr)
	    newscr->_clear = flag;
	else
	    win->_clear = flag;
	return OK; 
}


int immedok(WINDOW *win, bool flag)
{
	T(("immedok(%p,%d) called", win, flag));

   	win->_immed = flag;
	return OK; 
}

int leaveok(WINDOW *win, bool flag)
{
	T(("leaveok(%p,%d) called", win, flag));

   	win->_leaveok = flag;
   	if (flag == TRUE)
   		curs_set(0);
   	else
   		curs_set(1);
	return OK; 
}


int scrollok(WINDOW *win, bool flag)
{
	T(("scrollok(%p,%d) called", win, flag));

   	win->_scroll = flag;
	return OK; 
}

int halfdelay(int t)
{
	T(("halfdelay(%d) called", t));

	if (t < 1 || t > 255)
		return ERR;

	cbreak();
	SP->_cbreak = t+1;
	return OK;
}

int nodelay(WINDOW *win, bool flag)
{
	T(("nodelay(%p,%d) called", win, flag));

   	if (flag == TRUE)
		win->_delay = 0;
	else win->_delay = -1;
	return OK;
}

int notimeout(WINDOW *win, bool f)
{
	T(("notimout(%p,%d) called", win, f));

	win->_notimeout = f;
	return OK;
}

int wtimeout(WINDOW *win, int delay)
{
	T(("wtimeout(%p,%d) called", win, delay));

	win->_delay = delay;
	return OK;
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
	return OK; 
}

int keypad(WINDOW *win, bool flag)
{
	T(("keypad(%p,%d) called", win, flag));

   	win->_use_keypad = flag;
	return (_nc_keypad(flag));
}


int meta(WINDOW *win, bool flag)
{
	T(("meta(%p,%d) called", win, flag));

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
	return OK; 
}

/* curs_set() moved here to narrow the kernel interface */

int curs_set(int vis)
{
int cursor = SP->_cursor;

	T(("curs_set(%d)", vis));

	if (vis < 0 || vis > 2)
		return ERR;

	switch(vis) {
	case 2:
		if (cursor_visible)
		{
			TPUTS_TRACE("cursor_visible");
			putp(cursor_visible);
		}
		break;
	case 1:
		if (cursor_normal)
		{
			TPUTS_TRACE("cursor_normal");
			putp(cursor_normal);
		}
		break;
	case 0:
		if (cursor_invisible)
		{
			TPUTS_TRACE("cursor_invisible");
			putp(cursor_invisible);
		}
		break;
	}
	SP->_cursor = vis;
	return cursor;	
}

/*
**      init_keytry()
**
**      Construct the try for the current terminal's keypad keys.
**
*/


static struct  try *newtry;

static void init_keytry(void)
{
    newtry = NULL;
	
#include "keys.tries"

	SP->_keytry = newtry;
}


static void add_to_try(char *str, short code)
{
static bool     out_of_memory = FALSE;
struct try      *ptr, *savedptr;

	if (! str  ||  out_of_memory)
	    	return;
	
	if (newtry != NULL)    {
    	ptr = savedptr = newtry;
	    
       	for (;;) {
	       	while (ptr->ch != (unsigned char) *str
		       &&  ptr->sibling != NULL)
	       		ptr = ptr->sibling;
	    
	       	if (ptr->ch == (unsigned char) *str) {
	    		if (*(++str)) {
	           		if (ptr->child != NULL)
	           			ptr = ptr->child;
               		else
	           			break;
	    		} else {
	        		ptr->value = code;
					return;
	   			}
			} else {
	    		if ((ptr->sibling = (struct try *) malloc(sizeof *ptr)) == NULL) {
	        		out_of_memory = TRUE;
					return;
	    		}
		    
	    		savedptr = ptr = ptr->sibling;
	    		ptr->child = ptr->sibling = NULL;
			if (*str == '\200')
				ptr->ch = '\0';
			else
				ptr->ch = (unsigned char) *str; 
	    		str++;
	    		ptr->value = (short) NULL;
	    
           		break;
	       	}
	   	} /* end for (;;) */  
	} else {   /* newtry == NULL :: First sequence to be added */
	    	savedptr = ptr = newtry = (struct try *) malloc(sizeof *ptr);
	    
	    	if (ptr == NULL) {
	        	out_of_memory = TRUE;
				return;
	    	}
	    
	    	ptr->child = ptr->sibling = NULL;
		if (*str == '\200')
			ptr->ch = '\0';
		else
			ptr->ch = (unsigned char) *str; 
	    	str++;
	    	ptr->value = (short) NULL;
	}
	
	    /* at this point, we are adding to the try.  ptr->child == NULL */
	    
	while (*str) {
	   	ptr->child = (struct try *) malloc(sizeof *ptr);
	    
	   	ptr = ptr->child;
	   
	   	if (ptr == NULL) {
	       	out_of_memory = TRUE;
		
			ptr = savedptr;
			while (ptr != NULL) {
		    	savedptr = ptr->child;
		    	free(ptr);
		    	ptr = savedptr;
			}
		
			return;
		}
	    
	   	ptr->child = ptr->sibling = NULL;
		if (*str == '\200')
			ptr->ch = '\0';
		else
			ptr->ch = (unsigned char) *str; 
	    	str++;
	   	ptr->value = (short) NULL;
	}
	
	ptr->value = code;
	return;
}

int typeahead(int fd)
{

	T(("typeahead(%d) called", fd));
	SP->_checkfd = fd;
	return OK;
}

