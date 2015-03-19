/*	$OpenBSD: version.c,v 1.10 2015/03/19 21:22:15 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 * This file contains the string that gets written
 * out by the emacs-version command.
 */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>

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
	return (TRUE);
}
