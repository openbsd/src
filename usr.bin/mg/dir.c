/*	$OpenBSD: dir.c,v 1.16 2005/12/20 05:04:28 kjell Exp $	*/

/* This file is in the public domain. */

/*
 * Name:	MG 2a
 *		Directory management functions
 * Created:	Ron Flax (ron@vsedev.vse.com)
 *		Modified for MG 2a by Mic Kaczmarczik 03-Aug-1987
 */

#include "def.h"

char		*wdir;
static char	 cwd[NFILEN];

/*
 * Initialize anything the directory management routines need.
 */
void
dirinit(void)
{
	if ((wdir = getcwd(cwd, sizeof(cwd))) == NULL) {
		ewprintf("Can't get current directory!");
		chdir("/");
		(void)strlcpy(cwd, "/", sizeof(cwd));
	}
}

/*
 * Change current working directory.
 */
/* ARGSUSED */
int
changedir(int f, int n)
{
	char	bufc[NPAT], *bufp;

	(void)strlcpy(bufc, wdir, sizeof(bufc));
	if ((bufp = eread("Change default directory: ", bufc, NPAT,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (chdir(bufc) == -1) {
		ewprintf("Can't change dir to %s", bufc);
		return (FALSE);
	} else {
		if ((wdir = getcwd(cwd, sizeof(cwd))) == NULL)
			panic("Can't get current directory!");
		ewprintf("Current directory is now %s", wdir);
		return (TRUE);
	}
}

/*
 * Show current directory.
 */
/* ARGSUSED */
int
showcwdir(int f, int n)
{
	ewprintf("Current directory: %s", wdir);
	return (TRUE);
}
