/*
 *	Termcap terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

#define GOSLING				/* Compile in fancy display.	*/
/* #define	MEMMAP		*/	/* Not memory mapped video.	*/

#define NROW	66			/* (maximum) Rows.		*/
#define NCOL	132			/* (maximum) Columns.		*/
/* #define	MOVE_STANDOUT */	/* don't move in standout mode	*/
#define STANDOUT_GLITCH			/* possible standout glitch	*/
#define TERMCAP				/* for possible use in ttyio.c	*/

#define getkbd()	(ttgetc())

#ifndef XKEYS
#define ttykeymapinit() {}
#endif

extern	int tputs();
#define	putpad(str, num)	tputs(str, num, ttputc)

#define	KFIRST	K00
#define	KLAST	K00
