/*	$OpenBSD: ttydef.h,v 1.11 2015/03/19 21:48:05 bcallah Exp $	*/

/* This file is in the public domain. */

#ifndef TTYDEF_H
#define TTYDEF_H

/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define STANDOUT_GLITCH			/* possible standout glitch	 */

#ifdef undef
#define MOVE_STANDOUT			/* don't move in standout mode	 */
#endif /* undef */

#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00

#endif /* TTYDEF_H */
