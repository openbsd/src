/*
 * Name:	MG 2a
 *		Terminfo keyboard driver using key files
 * Created:	22-Nov-1987 Mic Kaczmarczik (mic@emx.cc.utexas.edu)
 */

#include	"def.h"
#include	"kbd.h"
#ifdef	XKEYS
#include	<term.h>

#ifdef  FKEYS
/*
 * Get keyboard character.  Very simple if you use keymaps and keys files.
 * Bob was right -- the old XKEYS code is not the right solution.
 * FKEYS code is not usefull other than to help debug FKEYS code in
 * extend.c.
 */

char	*keystrings[] = { NULL } ;
#endif

/*
 * Turn on function keys using keypad_xmit, then load a keys file, if
 * available.  The keys file is located in the same manner as the startup
 * file is, depending on what startupfile() does on your system.
 */
extern	int	ttputc();

ttykeymapinit()
{
	extern int dobindkey();	/* XXX */
	char *cp;
#ifdef FKEYS
	/* Bind keypad function keys. */
	if (key_left)
		dobindkey(map_table[0].p_map, "backward-char", key_left);
	if (key_right)
		dobindkey(map_table[0].p_map, "forward-char", key_right);
	if (key_up)
		dobindkey(map_table[0].p_map, "previous-line", key_up);
	if (key_down)
		dobindkey(map_table[0].p_map, "next-line", key_down);
	if (key_beg)
		dobindkey(map_table[0].p_map, "beginning-of-line", key_beg);
	if (key_end)
		dobindkey(map_table[0].p_map, "end-of-line", key_end);
	if (key_npage)
		dobindkey(map_table[0].p_map, "scroll-up", key_npage);
	if (key_ppage)
		dobindkey(map_table[0].p_map, "scroll-down", key_ppage);
#endif
#ifndef	NO_STARTUP
	if (cp = gettermtype()) {
		extern char *startupfile();
		if (((cp = startupfile(cp)) != NULL)
			&& (load(cp) != TRUE))
			ewprintf("Error reading key initialization file");
	}
#endif
	if (keypad_xmit)			/* turn on keypad	*/
		putpad(keypad_xmit, 1);
}

/*
 * Clean up the keyboard -- called by tttidy()
 */
ttykeymaptidy()
{

	if (keypad_local)
		putpad(keypad_local, 1);	/* turn off keypad	*/
}

#endif
