/*	$OpenBSD: pmdb_machdep.h,v 1.3 2003/01/27 00:06:14 jason Exp $	*/

#define BREAKPOINT              { 0x7C, 0x81, 0x08, 0x08 }
#define BREAKPOINT_LEN          4
#define BREAKPOINT_DECR_PC      4
