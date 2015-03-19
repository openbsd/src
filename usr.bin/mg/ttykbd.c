/*	$OpenBSD: ttykbd.c,v 1.18 2015/03/19 21:22:15 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 * Name:	MG 2a
 *		Terminfo keyboard driver using key files
 * Created:	22-Nov-1987 Mic Kaczmarczik (mic@emx.cc.utexas.edu)
 */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <term.h>

#include "def.h"
#include "kbd.h"

/*
 * Get keyboard character.  Very simple if you use keymaps and keys files.
 */

char	*keystrings[] = {NULL};

/*
 * Turn on function keys using keypad_xmit, then load a keys file, if
 * available.  The keys file is located in the same manner as the startup
 * file is, depending on what startupfile() does on your system.
 */
void
ttykeymapinit(void)
{
	char	*cp;

	/* Bind keypad function keys. */
	if (key_left)
		dobindkey(fundamental_map, "backward-char", key_left);
	if (key_right)
		dobindkey(fundamental_map, "forward-char", key_right);
	if (key_up)
		dobindkey(fundamental_map, "previous-line", key_up);
	if (key_down)
		dobindkey(fundamental_map, "next-line", key_down);
	if (key_beg)
		dobindkey(fundamental_map, "beginning-of-line", key_beg);
	else if (key_home)
		dobindkey(fundamental_map, "beginning-of-line", key_home);
	if (key_end)
		dobindkey(fundamental_map, "end-of-line", key_end);
	if (key_npage)
		dobindkey(fundamental_map, "scroll-up", key_npage);
	if (key_ppage)
		dobindkey(fundamental_map, "scroll-down", key_ppage);
	if (key_dc)
		dobindkey(fundamental_map, "delete-char", key_dc);

	if ((cp = getenv("TERM"))) {
		if (((cp = startupfile(cp)) != NULL) && (load(cp) != TRUE))
			ewprintf("Error reading key initialization file");
	}
	if (keypad_xmit)
		/* turn on keypad */
		putpad(keypad_xmit, 1);
}

/*
 * Clean up the keyboard -- called by tttidy()
 */
void
ttykeymaptidy(void)
{
	if (keypad_local)
		/* turn off keypad */
		putpad(keypad_local, 1);
}

