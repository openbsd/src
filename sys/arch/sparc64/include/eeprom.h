/*	$NetBSD: eeprom.h,v 1.2 1999/12/30 16:25:17 eeh Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Structure/definitions for the Sun3/Sun4 EEPROM.
 *
 * This information is published in the Sun document:
 * "PROM User's Manual", part number 800-1736010.
 */


/*
 * Note that most places where the PROM stores a "true/false" flag,
 * the true value is 0x12 and false is the usual zero.  Such flags
 * all take the values EE_TRUE or EE_FALSE so this file does not
 * need to define so many value macros.
 */
#define	EE_TRUE 0x12
#define	EE_FALSE   0

struct eeprom {

	/* 0x00 */
	u_char	eeTestArea[4];		/* Factory Defined */
	u_short	eeWriteCount[4];	/*    ||      ||   */
	u_char	eeChecksum[4];  	/*    ||      ||   */
	u_int32_t	eeLastHwUpdate; 	/*    ||      ||   */

	/* 0x14 */
	u_char	eeInstalledMem; 	/* Megabytes */
	u_char	eeMemTestSize;		/*     ||    */

	/* 0x16 */
	u_char	eeScreenSize;
#define	EE_SCR_1152X900 	0x00
#define	EE_SCR_1024X1024	0x12
#define EE_SCR_1600X1280	0x13
#define EE_SCR_1440X1440	0x14

	u_char	eeWatchDogDoesReset;	/* Watchdog timeout action:
					 * true:  reset/reboot
					 * false: return to monitor
					 */
	/* 0x18 */
	u_char	eeBootDevStored;	/* Is the boot device stored:
					 * true:  use stored device spec.
					 * false: use default (try all)
					 */
	/* Stored boot device spec. i.e.: "sd(Ctlr,Unit,Part)" */
	u_char	eeBootDevName[2];	/* xy,xd,sd,ie,le,st,xt,mt,...	*/
	u_char	eeBootDevCtlr;
	u_char	eeBootDevUnit;
	u_char	eeBootDevPart;

	/* 0x1E */
	u_char	eeKeyboardType;		/* zero for sun keyboards */
	u_char	eeConsole;		/* What to use for the console	*/
#define	EE_CONS_BW		0x00	/* - On-board B&W / keyboard	*/
#define	EE_CONS_TTYA		0x10	/* - serial port A		*/
#define	EE_CONS_TTYB		0x11	/* - serial port B		*/
#define	EE_CONS_COLOR		0x12	/* - Color FB / keyboard	*/
#define	EE_CONS_P4OPT		0x20	/* - Option board on P4		*/

	/* 0x20 */
	u_char	eeCustomBanner;		/* Is there a custom banner:
					 * true:  use text at 0x68
					 * false: use Sun banner
					 */

	u_char	eeKeyClick;		/* true/false */

	/* Boot device with "Diag" switch in Diagnostic mode: */
	u_char	eeDiagDevName[2];
	u_char	eeDiagDevCtlr;
	u_char	eeDiagDevUnit;
	u_char	eeDiagDevPart;

	/* Video white-on-black (not implemented) */
	u_char	eeWhiteOnBlack;		/* true/false */

	/* 0x28 */
	char	eeDiagPath[40];		/* path name of diag program	*/

	/* 0x50 */
	u_char	eeTtyCols;		/* normally 80 */
	u_char	eeTtyRows;		/* normally 34 */
	u_char	ee_x52[6];		/* unused */

	/* 0x58 */
	/* Default parameters for tty A and tty B: */
	struct	eeTtyDef {
	    u_char	eetBaudSet;	/* Is the baud rate set?
					 * true:  use values here
					 * false: use default (9600)
					 */
	    u_char	eetBaudHi;	/* i.e. 96..  */
	    u_char	eetBaudLo;	/*      ..00  */
	    u_char	eetNoRtsDtr;	/* true: disable H/W flow
					 * false: enable H/W flow */
	    u_char	eet_pad[4];
	} eeTtyDefA, eeTtyDefB;

	/* 0x68 */
	char eeBannerString[80];	/* see eeCustomBanner above */

	/* 0xB8 */
	u_short	eeTestPattern;		/* must be 0xAA55 */
	u_short ee_xBA;			/* unused */

	/* 0xBC */
	/* Configuration data.  Hopefully we don't need it. */
	struct eeConf {
	    u_char	eecData[16];
	} eeConf[12+1];

	/* 0x18c */
	u_char	eeAltKeyTable;		/* What Key table to use:
					 * 0x58: EEPROM tables
					 * else: PROM key tables
					 */
	u_char	eeKeyboardLocale;	/* extended keyboard type */
	u_char	eeKeyboardID;		/* for EEPROM key tables  */
	u_char	eeCustomLogo;		/* true: use eeLogoBitmap */

	/* 0x190 */
	u_char	eeKeymapLC[0x80];
	u_char	eeKeymapUC[0x80];

	/* 0x290 */
	u_char	eeLogoBitmap[64][8];	/* 64x64 bit custom logo */

	/* 0x490 */
	u_char	ee_x490[0x500-0x490];	/* unused */

	/* Other stuff we don't care about... */
	/* 0x500 */
	u_char	eeReserved[0x100];
	/* 0x600 */
	u_char	eeROM_Area[0x100];
	/* 0x700 */
	u_char	eeUnixArea[0x100];
};

/*
 * The size of the eeprom on machines with the old clock is 2k.  However,
 * on machines with the new clock (and the `eeprom' in the nvram area)
 * there are only 2040 bytes available. (???).  Since we really only
 * care about the `diagnostic' area, we'll use it's size when dealing
 * with the eeprom in general.
 */
#define EEPROM_SIZE		0x500

#ifdef	_KERNEL
extern	char *eeprom_va;
int	eeprom_uio __P((struct uio *));
#endif	/* _KERNEL */

