/*	$NetBSD: macglobals.s,v 1.3 1996/05/14 04:00:48 briggs Exp $	*/

/* Copyright 1994 by Bradley A. Grantham, All rights reserved */

/*
 * MacOS global variable space; storage for global variables used by
 * MacROMGlue routines (see macrom.c, macrom.h macromasm.s)
 */

	.text
	.space 0x2a00	/* did I miss something? this is a bad fix for
			   someone who is writing over low mem */
/* changed from 0xf00 to 0x2a00 as some routine running before ADBReInit
   chooses to write to 0x1fb8.  With the trap table from 0x0 to 0x3ff,
   this additional space of 0x2a00 should be sufficient  (WRU) */ 

/*
 * This has not been included for some time and things seem to still
 * be working.
	.space 0x1000
 */
