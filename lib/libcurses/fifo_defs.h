/*	$OpenBSD: fifo_defs.h,v 1.1 1997/12/03 05:21:08 millert Exp $	*/


/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
 * Common macros for lib_getch.c, lib_ungetch.c
 *
 * Id: fifo_defs.h,v 1.1 1997/10/19 02:31:46 tom Exp $
 */

#ifndef FIFO_DEFS_H
#define FIFO_DEFS_H 1

#define head	SP->_fifohead
#define tail	SP->_fifotail
/* peek points to next uninterpreted character */
#define peek	SP->_fifopeek

#define h_inc() { head == FIFO_SIZE-1 ? head = 0 : head++; if (head == tail) head = -1, tail = 0;}
#define h_dec() { head == 0 ?  head = FIFO_SIZE-1 : head--; if (head == tail) tail = -1;}
#define t_inc() { tail == FIFO_SIZE-1 ? tail = 0 : tail++; if (tail == head) tail = -1;}
#define t_dec() { tail == 0 ?  tail = FIFO_SIZE-1 : tail--; if (head == tail) fifo_clear();}
#define p_inc() { peek == FIFO_SIZE-1 ? peek = 0 : peek++;}

#define cooked_key_in_fifo()	(head!=-1 && peek!=head)
#define raw_key_in_fifo()	(head!=-1 && peek!=tail)

#undef HIDE_EINTR

#endif /* FIFO_DEFS_H */
