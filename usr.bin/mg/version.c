/*
 * This file contains the string that get written
 * out by the emacs-version command.
 */

#define TRUE	1	/* include "def.h" when things get more complicated */

char version[] = "Mg 2a (formerly MicroGnuEmacs)";

/*
 * Display the version. All this does
 * is copy the version string onto the echo line.
 */
/*ARGSUSED*/
showversion(f, n)
int f, n;
{
	ewprintf(version);
	return TRUE;
}
