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
#include        "key.h"
/*
 * Get keyboard character.  Very simple if you use keymaps and keys files.
 * Bob was right -- the old XKEYS code is not the right solution.
 * FKEYS code is not usefull other than to help debug FKEYS code in
 * extend.c.
 */

char	*keystrings[] = { NULL } ;

int
dobindkey(str, func)
	char *str;
	char *func;
{
	int i;
	extern int bindkey(); /* XXX */

	for (i = 0; *str && i < MAXKEY; i++) {
		/* XXX - how to convert numbers? */
		if (*str != '\\')
			key.k_chars[i] = *str;
		else {
			switch(*++str) {
			case 't': case 'T':
				key.k_chars[i] = '\t';
				break;
			case 'n': case 'N':
				key.k_chars[i] = '\n';
				break;
			case 'r': case 'R':
				key.k_chars[i] = '\r';
				break;
			case 'e': case 'E':
				key.k_chars[i] = CCHR('[');
				break;
			}
		}
		str++;
	}
	key.k_count = i;
	return(bindkey(&map_table[0].p_map, func, key.k_chars, key.k_count));
}
#endif

/*
 * Turn on function keys using keypad_xmit, then load a keys file, if
 * available.  The keys file is located in the same manner as the startup
 * file is, depending on what startupfile() does on your system.
 */
extern	int	ttputc();

ttykeymapinit()
{
	char *cp;
#ifdef FKEYS
	/* Bind keypad function keys. */
	if (key_left)
		dobindkey(key_left, "backward-char");
	if (key_left)
		dobindkey(key_left, "backward-char");
	if (key_right)
		dobindkey(key_right, "forward-char");
	if (key_up)
		dobindkey(key_up, "previous-line");
	if (key_down)
		dobindkey(key_down, "next-line");
	if (key_beg)
		dobindkey(key_beg, "beginning-of-line");
	if (key_end)
		dobindkey(key_end, "end-of-line");
	if (key_npage)
		dobindkey(key_npage, "scroll-up");
	if (key_ppage)
		dobindkey(key_ppage, "scroll-down");
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
