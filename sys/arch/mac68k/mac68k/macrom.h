/*	$NetBSD: macrom.h,v 1.5 1995/09/17 21:28:37 briggs Exp $	*/

/*-
 * Copyright (C) 1994	Bradley A. Grantham
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/types.h>


/* Low-memory Globals */
extern caddr_t		ROMBase;	/* Base address of ROM space */
extern caddr_t		ADBBase;	/* Base address of ADB scratch */
extern caddr_t		ADBYMM; 	/* Base address of yet more ADB mem */
extern caddr_t		ADBState;	/* Base address of ADB scratch ? */
extern void		(*JADBProc)();	/* ADBReInit pre/post processing */
extern void		(*Lvl1DT[8])(); /* VIA1 interrupt table by bit */
extern void		(*Lvl2DT[8])(); /* VIA2 interrupt table by bit */
extern void		(*jADBOp)();	/* low memory pointer to ADBOp */
extern void		(*jUnimplTrap)();	/* Unimplemented trap */
	/* loglob(KbdLast, 0x218)	/* addr of last kbd to send */
	/* loglob(KbdType, 0x21E)	/* type of last kbd to send */
extern void		(*JKybdTask)(); /* Keyboard task ptr? */
extern u_char		CPUFlag;	/* Type of CPU in this machine */
extern void		(*MacJmp)();	/* ??? */
extern u_long		Lo3Bytes;	/* 0x00ffffff */
extern u_long		MinusOne;	/* 0xffffffff */
extern u_short		MMU32Bit;	/* MMU mode; short? */
extern u_char		Scratch8[8];	/* 8-byte scratch */
extern u_char		Scratch20[20];	/* 20-byte scratch */
extern u_long		Ticks;		/* ticks since startup */
extern u_long		Time;		/* ticks since startup */
extern u_short		TimeDBRA;	/* DBRA's per milli */
extern u_short		ADBDelay;	/* DBRAs per ADB loop, / 8 */
extern u_char		ToolScratch[8]; /* Yet another 8-byte scratch area */
extern caddr_t		VIA;		/* VIA1 base address */
extern caddr_t		mrg_VIA2;	/* VIA2 base address */
extern caddr_t		SCCRd;		/* SCC read base address */
extern u_char		FinderName[20]; /* FinderName - Pascal string */
extern void		(*jSwapMMU)();	/* Pointer to MMU Swap routine */
extern void		(*jEgret)();	/* Pointer to MMU Swap routine */
extern u_int16_t	HwCfgFlags;	/* Hardware config flags */
extern u_int32_t	HwCfgFlags2;	/* more hardware config flags */
extern u_int32_t	HwCfgFlags3;	/* more hardware config flags */
extern u_int32_t	ADBReInit_JTBL;	/* pointer to patch table */
extern void		(*jClkNoMem)(); /* pointer to ClkNoMem */
extern u_char		SysParam[20];	/* Place where PRam data gets stored */
	/* Types */

typedef caddr_t Ptr;
typedef caddr_t *Handle;

/* ADB Manager */
typedef struct {
	Ptr siServiceRtPtr;
	Ptr siDataAreaAddr;
} ADBSetInfoBlock;
typedef struct {
	unsigned char	devType;
	unsigned char	origADBAddr;
	Ptr		dbServiceRtPtr;
	Ptr		dbDataAreaAdd;
} ADBDataBlock;


	/* Trap Flesh; these functions are C, not Pascal */

/* trap tests */
int MyOwnTrap(
	void);
void KnownRTS(
	void);

/* ADB Manager */
int SetADBInfo(
	ADBSetInfoBlock *info,
	int		adbAddr);
int CountADBs(  
	void);
int GetIndADB(
	ADBDataBlock	*info,
	int		index);
int GetADBInfo(
	ADBDataBlock	*info,
	int		adbAddr);
void ADBReInit(
	void);
int ADBOp(
	Ptr	buffer, 	/* note different order of parameters */
	Ptr	compRout,
	Ptr	data,
	short	commandNum);
void ADBAlternateInit(
	void);

/* Memory Manager */
Ptr NewPtr(
	int size);
int DisposPtr(
	Ptr ptr);
int GetPtrSize(
	Ptr ptr);
int SetPtrSize(
	Ptr ptr,
	int newbytes);

/* Resource Manager */
Handle GetResource(
	u_int theType,
	short	theID);
short ResError(
	void);


	/* Mac ROM Glue globals for BSD kernel */
extern caddr_t mrg_romadbintr;
extern caddr_t mrg_ADBIntrPtr;
extern u_char mrg_GetResource[];	/* type is almost a lie; 
					call it an array of bytes of code */
extern u_char mrg_ResError[];


	/* MacOS Error Codes */
#define noErr		0
#define memFullErr	-108
#define memWZErr	-111
#define resNotFound	-192


	/* Dump instruction trace */
void dumptrace(void);


	/* Stuff for configuring ROM Glue */
typedef struct romvec_s {
	char *romident; 	/* just to print it out */
	caddr_t adbintr;	/* where is ADB interrupt */
	caddr_t pmintr; 	/* where is ADB/PM interrupt, on machines */
				/*  that have it */
	caddr_t adb130intr;	/* ADBBase[0x130] interrupt; don't know */
				/*  what it is, but it's important.  Don't */
				/*  you love reverse engineering? */
	caddr_t CountADBs;
	caddr_t GetIndADB;
	caddr_t GetADBInfo;
	caddr_t SetADBInfo;
	caddr_t ADBReInit;
	caddr_t ADBOp;
	caddr_t PMgrOp; 	/* On machines that have it */
	caddr_t WriteParam;
	caddr_t SetDateTime;
	caddr_t InitUtil;
	caddr_t ReadXPRam;
	caddr_t WriteXPRam;
	caddr_t jClkNoMem;
	caddr_t ADBAlternateInit;	/* more fundamental than ABDReInit */
	caddr_t InitEgret;	/* Set up Buffer for Egret routines */
} romvec_t;
void mrg_setvectors(romvec_t *rom_vectors);
int mrg_romready(void);


	/* Tracing aids */
/* trace all instructions, not just flow changes. */
#define tron() \
	asm("movw sr, d0 ; orw #0x8000, d0 ; movw d0, sr" : : : "d0")
#define troff() \
	asm("movw sr, d0 ; andw #0x3fff, d0 ; movw d0, sr" : : : "d0")
