/*	$OpenBSD: version.c,v 1.7 2003/06/01 23:58:04 art Exp $	*/

/*
 * This file contains the string that get written
 * out by the emacs-version command.
 */

#include "def.h"

const char	version[] = "Mg 2a";

/*
 * Display the version. All this does
 * is copy the version string onto the echo line.
 */
/* ARGSUSED */
int
showversion(int f, int n)
{
	ewprintf("%s", version);
	return TRUE;
}
