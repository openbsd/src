/*	$OpenBSD: ttydef.h,v 1.12 2015/03/24 22:28:10 bcallah Exp $	*/

/* This file is in the public domain. */

#ifndef TTYDEF_H
#define TTYDEF_H

/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define STANDOUT_GLITCH			/* possible standout glitch	 */

#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00

#endif /* TTYDEF_H */
