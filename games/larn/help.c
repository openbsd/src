/*	$OpenBSD: help.c,v 1.3 2000/06/29 07:55:41 pjanzen Exp $	*/
/*	$NetBSD: help.c,v 1.4 1997/10/18 20:03:24 christos Exp $	*/

/* help.c		Larn is copyrighted 1986 by Noah Morgan. */
#ifndef lint
static char rcsid[] = "$OpenBSD: help.c,v 1.3 2000/06/29 07:55:41 pjanzen Exp $";
#endif /* not lint */

#include <unistd.h>

#include "header.h"
#include "extern.h"
/*
 *	help function to display the help info
 *
 *	format of the .larn.help file
 *
 *	1st character of file:	# of pages of help available (ascii digit)
 *	page (23 lines) for the introductory message (not counted in above)
 *	pages of help text (23 lines per page)
 */
void
help()
{
	int	i, j;
	char	tmbuf[128];	/* intermediate translation buffer
					 * when not a VT100 */
	if ((j = openhelp()) < 0)
		return;		/* open the help file and get # pages */
	for (i = 0; i < 23; i++)
		lgetl();	/* skip over intro message */
	for (; j > 0; j--) {
		clear();
		for (i = 0; i < 23; i++)
		{
			tmcapcnv(tmbuf, lgetl());
			lprcat(tmbuf);
		}		/* intercept \33's */
		if (j > 1) {
			lprcat("    ---- Press ");
			lstandout("return");
			lprcat(" to exit, ");
			lstandout("space");
			lprcat(" for more help ---- ");
			i = 0;
			while ((i != ' ') && (i != '\n') && (i != '\33'))
				i = lgetchar();
			if ((i == '\n') || (i == '\33')) {
				lrclose();
				setscroll();
				drawscreen();
				return;
			}
		}
	}
	lrclose();
	retcont();
	drawscreen();
}

/*
 *	function to display the welcome message and background
 */
void
welcome()
{
	int	i;
	char	tmbuf[128];	/* intermediate translation buffer
					 * when not a VT100 */
	if (openhelp() < 0)
		return;		/* open the help file */
	clear();
	for (i = 0; i < 23; i++)
	{
		tmcapcnv(tmbuf, lgetl());
		lprcat(tmbuf);
	}			/* intercept \33's */
	lrclose();
	retcont();		/* press return to continue */
}

/*
 *	function to say press return to continue and reset scroll when done
 */
void
retcont()
{
	cursor(1, 24);
	lprcat("Press ");
	lstandout("return");
	lprcat(" to continue: ");
	while (lgetchar() != '\n');
	setscroll();
}

/*
 *	routine to open the help file and return the first character - '0'
 */
int
openhelp()
{
	if (lopen(helpfile) < 0) {
		lprintf("Can't open help file \"%s\" ", helpfile);
		lflush();
		sleep(4);
		drawscreen();
		setscroll();
		return (-1);
	}
	resetscroll();
	return (lgetc() - '0');
}
