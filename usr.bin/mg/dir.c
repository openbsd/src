/*
 * Name:	MG 2a
 *		Directory management functions
 * Created:	Ron Flax (ron@vsedev.vse.com)
 *		Modified for MG 2a by Mic Kaczmarczik 03-Aug-1987
 */

#include "def.h"

#ifndef NO_DIR
#ifndef	getwd			/* may be a #define */
char	*getwd();
#endif
char	*wdir;
static char cwd[NFILEN];

/*
 * Initialize anything the directory management routines need
 */
dirinit()
{
	if (!(wdir = getwd(cwd)))
		panic("Can't get current directory!");
}

/*
 * Change current working directory
 */
/*ARGSUSED*/
changedir(f, n)
{
	register int s;
	char bufc[NPAT];

	if ((s=ereply("Change default directory: ", bufc, NPAT)) != TRUE)
		return(s);
	if (bufc[0] == '\0')
		(VOID) strcpy(bufc, wdir);
	if (chdir(bufc) == -1) {
		ewprintf("Can't change dir to %s", bufc);
		return(FALSE);
	} else {
		if (!(wdir = getwd(cwd)))
			panic("Can't get current directory!");
		ewprintf("Current directory is now %s", wdir);
		return(TRUE);
	}
}

/*
 * Show current directory
 */
/*ARGSUSED*/
showcwdir(f, n)
{
	ewprintf("Current directory: %s", wdir);
	return(TRUE);
}
#endif
