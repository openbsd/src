/*	$OpenBSD: version.c,v 1.6 2003/05/20 03:08:55 cloder Exp $	*/

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
	ewprintf(version);
	return TRUE;
}
