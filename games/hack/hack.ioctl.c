/*	$OpenBSD: hack.ioctl.c,v 1.3 2001/08/06 22:59:13 pjanzen Exp $	*/

/*
 * Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: hack.ioctl.c,v 1.3 2001/08/06 22:59:13 pjanzen Exp $";
#endif /* not lint */

/* This cannot be part of hack.tty.c (as it was earlier) since on some
   systems (e.g. MUNIX) the include files <termio.h> and <sgtty.h>
   define the same constants, and the C preprocessor complains. */
#include <stdio.h>
#include <termios.h>
#include "config.h"
struct termios termios;

getioctls() {
	(void) tcgetattr(fileno(stdin), &termios);
}

setioctls() {
	(void) tcsetattr(fileno(stdin), TCSADRAIN, &termios);
}

#ifdef SUSPEND		/* implies BSD */
#include	<signal.h>
dosuspend() {
#ifdef SIGTSTP
	if(signal(SIGTSTP, SIG_IGN) == SIG_DFL) {
		settty((char *) 0);
		(void) signal(SIGTSTP, SIG_DFL);
		(void) kill(0, SIGTSTP);
		gettty();
		setftty();
		docrt();
	} else {
		pline("I don't think your shell has job control.");
	}
#else /* SIGTSTP */
	pline("Sorry, it seems we have no SIGTSTP here. Try ! or S.");
#endif /* SIGTSTP */
	return(0);
}
#endif /* SUSPEND */
