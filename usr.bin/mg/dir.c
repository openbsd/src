/*	$OpenBSD: dir.c,v 1.17 2006/05/02 17:10:25 kjell Exp $	*/

/* This file is in the public domain. */

/*
 * Name:	MG 2a
 *		Directory management functions
 * Created:	Ron Flax (ron@vsedev.vse.com)
 *		Modified for MG 2a by Mic Kaczmarczik 03-Aug-1987
 */

#include "def.h"

static char	 mgcwd[NFILEN];

/*
 * Initialize anything the directory management routines need.
 */
void
dirinit(void)
{
	mgcwd[0] = '\0';
	if (getcwd(mgcwd, sizeof(mgcwd)) == NULL) {
		ewprintf("Can't get current directory!");
		chdir("/");
	}
	(void)strlcat(mgcwd, "/", sizeof(mgcwd));
}

/*
 * Change current working directory.
 */
/* ARGSUSED */
int
changedir(int f, int n)
{
	char	bufc[NFILEN], *bufp;

	(void)strlcpy(bufc, mgcwd, sizeof(bufc));
	if ((bufp = eread("Change default directory: ", bufc, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (chdir(bufc) == -1) {
		ewprintf("Can't change dir to %s", bufc);
		return (FALSE);
	} else {
		if ((bufp = getcwd(mgcwd, sizeof(mgcwd))) == NULL)
			panic("Can't get current directory!");
		ewprintf("Current directory is now %s", bufp);
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
	ewprintf("Current directory: %s", mgcwd);
	return (TRUE);
}

int
getcwdir(char *buf, size_t len)
{
	if (strlcpy(buf, mgcwd, len) >= len)
		return (FALSE);

	return (TRUE);
}
