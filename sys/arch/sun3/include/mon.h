/*	$NetBSD: mon.h,v 1.19 1996/11/20 18:57:12 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file derived from kernel/mach/sun3.md/machMon.h from the
 * sprite distribution.
 *
 * In particular, this file came out of the Walnut Creek cdrom collection
 * which contained no warnings about any possible copyright infringement.
 * It was also indentical to a file in the sprite kernel tar file found on
 * allspice.berkeley.edu.
 * It also written in the annoying sprite coding style.  I've made
 * efforts not to heavily edit their file, just ifdef parts out. -- glass
 */

#ifndef _MACHINE_MON_H
#define _MACHINE_MON_H
/*
 * machMon.h --
 *
 *     Structures, constants and defines for access to the sun monitor.
 *     These are translated from the sun monitor header file "sunromvec.h".
 *
 * NOTE: The file keyboard.h in the monitor directory has all sorts of useful
 *       keyboard stuff defined.  I haven't attempted to translate that file
 *       because I don't need it.  If anyone wants to use it, be my guest.
 *
 * Copyright (C) 1985 Regents of the University of California
 * All rights reserved.
 *
 *
 * Header: /cdrom/src/kernel/Cvsroot/kernel/mach/sun3.md/machMon.h,v 9.1 90/10/03 13:52:34 mgbaker Exp SPRITE (Berkeley)
 */

#ifndef _MACHMON
#define _MACHMON

/*
 * The memory addresses for the PROM, and the EEPROM.
 * On the sun2 these addresses are actually 0x00EF??00
 * but only the bottom 24 bits are looked at so these still
 * work ok.
 */

#define PROM_BASE       0x0fef0000

/*
 * Structure set up by the boot command to pass arguments to the program that
 * is booted.
 */
typedef struct bootparam {
	char		*argPtr[8];	/* String arguments */
	char		strings[100];	/* String table for string arguments */
	char		devName[2];	/* Device name */
	int		ctlrNum;	/* Controller number */
	int		unitNum;	/* Unit number */
	int		partNum;	/* Partition/file number */
	char		*fileName;	/* File name, points into strings */
	struct boottab   *bootDevice;	/* Defined in saio.h */
} MachMonBootParam;

/*
 * Here is the structure of the vector table which is at the front of the boot
 * rom.  The functions defined in here are explained below.
 *
 * NOTE: This struct has references to the structures keybuf and globram which
 *       I have not translated.  If anyone needs to use these they should
 *       translate these structs into Sprite format.
 */
typedef struct {
	char		*initSp;		/* Initial system stack ptr  
						 * for hardware */
	int		(*startMon)();		/* Initial PC for hardware */

	int		*diagberr;		/* Bus err handler for diags */

	/* 
	 * Monitor and hardware revision and identification
	 */

	struct bootparam **bootParam;		/* Info for bootstrapped pgm */
 	unsigned	*memorySize;		/* Usable memory in bytes */

	/* 
	 * Single-character input and output 
	 */

	unsigned char	(*getChar)();		/* Get char from input source */
	int		(*putChar)();		/* Put char to output sink */
	int		(*mayGet)();		/* Maybe get char, or -1 */
	int		(*mayPut)();		/* Maybe put char, or -1 */
	unsigned char	*echo;			/* Should getchar echo? */
	unsigned char	*inSource;		/* Input source selector */
	unsigned char	*outSink;		/* Output sink selector */

	/* 
	 * Keyboard input (scanned by monitor nmi routine) 
	 */

	int		(*getKey)();		/* Get next key if one exists */
	int		(*initGetKey)();	/* Initialize get key */
	unsigned int	*translation;		/* Kbd translation selector 
						   (see keyboard.h in sun 
						    monitor code) */
	unsigned char	*keyBid;		/* Keyboard ID byte */
	int		*screen_x;		/* V2: Screen x pos (R/O) */
	int		*screen_y;		/* V2: Screen y pos (R/O) */
	struct keybuf	*keyBuf;		/* Up/down keycode buffer */

	/*
	 * Monitor revision level.
	 */

	char		*monId;

	/* 
	 * Frame buffer output and terminal emulation 
	 */

	int		(*fbWriteChar)();	/* Write a character to FB */
	int		*fbAddr;		/* Address of frame buffer */
	char		**font;			/* Font table for FB */
	int		(*fbWriteStr)();	/* Quickly write string to FB */

	/* 
	 * Reboot interface routine -- resets and reboots system.  No return. 
	 */

	int		(*reBoot)();		/* e.g. reBoot("xy()vmunix") */

	/* 
	 * Line input and parsing 
	 */

	unsigned char	*lineBuf;		/* The line input buffer */
	unsigned char	**linePtr;		/* Cur pointer into linebuf */
	int		*lineSize;		/* length of line in linebuf */
	int		(*getLine)();		/* Get line from user */
	unsigned char	(*getNextChar)();	/* Get next char from linebuf */
	unsigned char	(*peekNextChar)();	/* Peek at next char */
	int		*fbThere;		/* =1 if frame buffer there */
	int		(*getNum)();		/* Grab hex num from line */

	/* 
	 * Print formatted output to current output sink 
	 */

	int		(*printf)();		/* Similar to "Kernel printf" */
	int		(*printHex)();		/* Format N digits in hex */

	/*
	 * Led stuff 
	 */

	unsigned char	*leds;			/* RAM copy of LED register */
	int		(*setLeds)();		/* Sets LED's and RAM copy */

	/* 
	 * Non-maskable interrupt  (nmi) information
	 */ 

	int		(*nmiAddr)();		/* Addr for level 7 vector */
	int		(*abortEntry)();	/* Entry for keyboard abort */
	int		*nmiClock;		/* Counts up in msec */

	/*
	 * Frame buffer type: see <sun/fbio.h>
	 */

	int		*fbType;

	/* 
	 * Assorted other things 
	 */

	unsigned	romvecVersion;		/* Version # of Romvec */ 
	struct globram  *globRam;		/* monitor global variables */
	caddr_t		kbdZscc;		/* Addr of keyboard in use */

	int		*keyrInit;		/* ms before kbd repeat */
	unsigned char	*keyrTick; 		/* ms between repetitions */
	unsigned	*memoryAvail;		/* V1: Main mem usable size */
	long		*resetAddr;		/* where to jump on a reset */
	long		*resetMap;		/* pgmap entry for resetaddr */
						/* Really struct pgmapent *  */
	int		(*exitToMon)();		/* Exit from user program */
	unsigned char	**memorybitmap;		/* V1: &{0 or &bits} */
	void		(*setcxsegmap)();	/* Set seg in any context */
	void		(**vector_cmd)();	/* V2: Handler for 'v' cmd */
	int		dummy1z;
	int		dummy2z;
	int		dummy3z;
	int		dummy4z;
} MachMonRomVector;

/*
 * Functions defined in the vector:
 *
 *
 * getChar -- Return the next character from the input source
 *
 *     unsigned char getChar()
 *
 * putChar -- Write the given character to the output source.
 *
 *     void putChar(ch)
 *	   char ch;	
 *
 * mayGet -- Maybe get a character from the current input source.  Return -1 
 *           if don't return a character.
 *
 * 	int mayGet()
 *	
 * mayPut -- Maybe put a character to the current output source.   Return -1
 *           if no character output.
 *
 *	int  mayPut(ch)
 *	    char ch;
 *
 * getKey -- Returns a key code (if up/down codes being returned),
 * 	     a byte of ASCII (if that's requested),
 * 	     NOKEY (if no key has been hit).
 *
 *	int getKey()
 *	
 * initGetKey --  Initialize things for get key.
 *
 *	void initGetKey()
 *
 * fbWriteChar -- Write a character to the frame buffer
 *
 *	void fwritechar(ch)
 *	    unsigned char ch;
 *
 * fbWriteStr -- Write a string to the frame buffer.
 *
 *   	void fwritestr(addr,len)
 *  	    register unsigned char *addr;	/ * String to be written * /
 *  	    register short len;			/ * Length of string * /
 *
 * getLine -- read the next input line into a global buffer
 *
 *	getline(echop)
 *          int echop;	/ * 1 if should echo input, 0 if not * /
 *
 * getNextChar -- return the next character from the global line buffer.
 *
 *	unsigned char getNextChar()
 *
 * peekNextChar -- look at the next character in the global line buffer.
 *
 *	unsigned char peekNextChar()
 *
 * getNum -- Grab hex num from the global line buffer.
 *
 *	int getNum()
 *
 * printf -- Scaled down version of C library printf.  Only %d, %x, %s, and %c
 * 	     are recognized.
 *
 * printhex -- prints rightmost <digs> hex digits of <val>
 *
 *      printhex(val,digs)
 *          register int val;
 *     	    register int digs;
 *
 * abortEntry -- Entry for keyboard abort.
 *
 *     abortEntry()
 */

/*
 * Where the rom vector is defined.
 */

#define	romVectorPtr	((MachMonRomVector *) PROM_BASE)

/*
 * Functions and defines to access the monitor.
 */

#define mon_printf (romVectorPtr->printf)
#define mon_putchar (romVectorPtr->putChar)
#define mon_may_getchar (romVectorPtr->mayGet)
#define mon_exit_to_mon (romVectorPtr->exitToMon)
#define mon_reboot (romVectorPtr->reBoot)
#define mon_panic(x) { mon_printf(x); mon_exit_to_mon();}

#define mon_setcxsegmap(context, va, sme) \
     romVectorPtr->setcxsegmap(context, va, sme)
#define romp (romVectorPtr)

/*
 * MONSTART and MONEND denote the range of the damn monitor.
 * 
 * supposedly you can steal pmegs within this range that do not contain
 * valid pages. 
 */
#define MONSTART     0x0FE00000
#define MONEND       0x0FF00000

/*
 * These describe the monitor's short segment which it basically uses to map
 * one stupid page that it uses for storage.  MONSHORTPAGE is the page,
 * and MONSHORTSEG is the segment that it is in.  If this sounds dumb to
 * you, it is.  I can change the pmeg, but not the virtual address.
 * Sun defines these with the high nibble set to 0xF.  I believe this was
 * for the monitor source which accesses this piece of memory with addressing
 * limitations or some such crud.  I haven't replicated this here, because
 * it is confusing, and serves no obvious purpose if you aren't the monitor.
 *
 */

#define MONSHORTPAGE 0x0FFFE000	
#define MONSHORTSEG  0x0FFE0000     

#endif /* _MACHMON */
#endif /* MACHINE_MON_H */     
