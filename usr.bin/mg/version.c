/*	$OpenBSD: version.c,v 1.5 2001/05/23 22:24:26 mickey Exp $	*/

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
showversion(f, n)
	int f, n;
{
	ewprintf(version);
	return TRUE;
}
