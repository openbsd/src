/*	$NetBSD: macglobals.s,v 1.2 1995/08/16 13:18:24 briggs Exp $	*/

/* Copyright 1994 by Bradley A. Grantham, All rights reserved */

/*
 * MacOS global variable space; storage for global variables used by
 * MacROMGlue routines (see macrom.c, macrom.h macromasm.s)
 */

	.text
	.space 0xF00	/* did I miss something? this is a bad fix for
			   someone who is writing over low mem */
/*
 * This has not been included for some time and things seem to still
 * be working.
	.space 0x1000
 */
