/*	$OpenBSD: ttydef.h,v 1.8 2005/04/03 02:09:28 db Exp $	*/

#ifndef TTYDEF_H
#define TTYDEF_H

/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define GOSLING				/* Compile in fancy display.	 */
#define STANDOUT_GLITCH			/* possible standout glitch	 */
#define TERMCAP				/* for possible use in ttyio.c	 */

#ifdef undef
#define MEMMAP					/* Not memory mapped video.	 */
#define MOVE_STANDOUT				/* don't move in standout mode	 */
#endif /* undef */

#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00

#endif /* TTYDEF_H */
