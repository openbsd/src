/*	$OpenBSD: ttydef.h,v 1.7 2004/07/08 22:15:42 deraadt Exp $	*/

#ifndef TTYDEF_H
#define TTYDEF_H

/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define GOSLING				/* Compile in fancy display.	 */
/* #define	MEMMAP		      *//* Not memory mapped video.	 */

/* #define	MOVE_STANDOUT	      *//* don't move in standout mode	 */
#define STANDOUT_GLITCH			/* possible standout glitch	 */
#define TERMCAP				/* for possible use in ttyio.c	 */

#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00

#endif /* TTYDEF_H */
