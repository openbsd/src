/*	$NetBSD: limits.h,v 1.10 1996/03/19 03:09:03 jonathan Exp $	*/

#include <mips/limits.h>

#ifdef _KERNEL
#define CLK_TCK		60		/* ticks per second */
#endif
