/*	$OpenBSD: version.c,v 1.4 2001/01/29 01:58:10 niklas Exp $	*/

/*
 * This file contains the string that get written
 * out by the emacs-version command.
 */

#include "def.h"

char	version[] = "Mg 2a";

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
