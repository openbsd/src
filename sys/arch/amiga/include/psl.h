/*	$NetBSD: psl.h,v 1.7 1994/10/26 02:06:31 cgd Exp $	*/

#ifndef _MACHINE_PSL_H_
#define _MACHINE_PSL_H_

/* Interrupt priority `levels'; not mutually exclusive. */
#define	IPL_NONE	-1
#define	IPL_BIO		3	/* block I/O */
#define	IPL_NET		3	/* network */
#define	IPL_TTY		4	/* terminal */
#define	IPL_CLOCK	4	/* clock */
#define	IPL_IMP		4	/* memory allocation */

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#include <m68k/psl.h>

#endif
