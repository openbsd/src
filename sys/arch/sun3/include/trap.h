/*	$NetBSD: trap.h,v 1.7 1994/11/21 21:34:23 gwr Exp $	*/

#include <m68k/trap.h>

/*
 * XXX - need documentation.
 * probably should be comment,
 * for compat code
 */
#define	T_BRKPT		T_TRAP15
#define	T_WATCHPOINT	16
