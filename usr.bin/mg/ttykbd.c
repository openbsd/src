/*
 * Name:	MG 2a
 *		Termcap keyboard driver using key files
 * Created:	22-Nov-1987 Mic Kaczmarczik (mic@emx.cc.utexas.edu)
 */

#include	"def.h"
#ifdef	XKEYS

/*
 * Get keyboard character.  Very simple if you use keymaps and keys files.
 * Bob was right -- the old XKEYS code is not the right solution.
 * FKEYS code is not usefull other than to help debug FKEYS code in
 * extend.c.
 */

#ifdef FKEYS
char	*keystrings[] = { NULL } ;
#endif

/*
 * Turn on function keys using KS, then load a keys file, if available.
 * The keys file is located in the same manner as the startup file is,
 * depending on what startupfile() does on your system.
 */
extern	int	ttputc();

ttykeymapinit()
{
	extern	char *KS;
#ifndef	NO_STARTUP
	char *cp, *startupfile();

	if (cp = gettermtype()) {
		if (((cp = startupfile(cp)) != NULL)
			&& (load(cp) != TRUE))
			ewprintf("Error reading key initialization file");
	}
#endif
	if (KS && *KS)			/* turn on keypad	*/
		putpad(KS, 1);
}

/*
 * Clean up the keyboard -- called by tttidy()
 */
ttykeymaptidy()
{
	extern	char *KE;

	if (KE && *KE)
		putpad(KE, 1);	/* turn off keypad		*/
}

#endif
