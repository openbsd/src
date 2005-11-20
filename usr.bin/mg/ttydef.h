/*	$OpenBSD: ttydef.h,v 1.10 2005/11/20 03:53:45 deraadt Exp $	*/

/* This file is in the public domain. */

#ifndef TTYDEF_H
#define TTYDEF_H

/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define STANDOUT_GLITCH			/* possible standout glitch	 */
#define TERMCAP				/* for possible use in ttyio.c	 */

#ifdef undef
#define MOVE_STANDOUT			/* don't move in standout mode	 */
#endif /* undef */

#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00

#endif /* TTYDEF_H */
