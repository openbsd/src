/*	$OpenBSD: macrom.h,v 1.3 1996/05/26 18:36:25 briggs Exp $	*/
/*	$NetBSD: macrom.h,v 1.9 1996/05/25 14:45:35 briggs Exp $	*/

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
#include <machine/frame.h>


/* Low-memory Globals */
extern caddr_t		ROMBase;	/* Base address of ROM space */
extern caddr_t		ADBBase;	/* Base address of ADB scratch */
extern caddr_t		ADBYMM; 	/* Base address of yet more ADB mem */
extern caddr_t		ADBState;	/* Base address of ADB scratch ? */
extern void	(*JADBProc) __P((void)); /* ADBReInit pre/post processing */
extern void	(*Lvl1DT[8]) __P((void)); /* VIA1 interrupt table by bit */
extern void	(*Lvl2DT[8]) __P((void)); /* VIA2 interrupt table by bit */
extern void	(*jADBOp) __P((void));	/* low memory pointer to ADBOp */
extern void	(*jUnimplTrap) __P((void));	/* Unimplemented trap */
	/* loglob(KbdLast, 0x218)	* addr of last kbd to send */
	/* loglob(KbdType, 0x21E)	* type of last kbd to send */
extern void	(*JKybdTask) __P((void)); /* Keyboard task ptr? */
extern u_char		CPUFlag;	/* Type of CPU in this machine */
extern void	(*MacJmp) __P((void));	/* ??? */
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
extern void	(*jSwapMMU) __P((void)); /* Pointer to MMU Swap routine */
extern void	(*jEgret) __P((void));	/* Pointer to MMU Swap routine */
extern u_int16_t	HwCfgFlags;	/* Hardware config flags */
extern u_int32_t	HwCfgFlags2;	/* more hardware config flags */
extern u_int32_t	HwCfgFlags3;	/* more hardware config flags */
extern u_int32_t	ADBReInit_JTBL;	/* pointer to patch table */
extern void	(*jClkNoMem) __P((void)); /* pointer to ClkNoMem */
extern u_char		SysParam[20];	/* Place where PRam data gets stored */
extern caddr_t		ExpandMem;	/* pointer to Expanded Memory used by */
					/*  newer ADB routines (since LCIII) */
extern u_int16_t	VBLQueue;	/* Vertical blanking Queue, unused ? */
extern caddr_t		VBLQueue_head;	/* Vertical blanking Queue, head */
extern caddr_t		VBLQueue_tail;	/* Vertical blanking Queue, tail */

extern u_int32_t	**InitEgretJTVec; /* pointer to a jump table for */
					  /* InitEgret on AV machines */

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
short mrg_CountResources(
	u_int32_t type);
short Count_Resources(
	u_int32_t rsrc_type);
caddr_t *mrg_GetIndResource(
	u_int16_t index,
	u_int32_t type);


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
typedef struct rsrc_s {
	u_int16_t unknown[4];	/* ???? */
	u_int32_t next;		/* pointer to next resoure in list */
	u_int32_t body;		/* pointer to resource body? */
	u_int32_t name;		/* resource name */
	u_int16_t index;	/* ???? */
} rsrc_t;

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
	caddr_t Egret;
	caddr_t InitEgret;	/* Set up Buffer for Egret routines */
	caddr_t ADBReInit_JTBL;
	caddr_t ROMResourceMap; /* Address of first Resource in linked list */
	caddr_t FixDiv;
	caddr_t FixMul;
} romvec_t;

/*
 * Function prototypes.
 */

/* macrom.c */
void	mrg_setvectors __P((romvec_t *rom_vectors));
int	mrg_romready __P((void));
caddr_t	*Get_Ind_Resource __P((u_int32_t, u_int16_t));
void	mrg_initadbintr __P((void));
long	mrg_adbintr __P((void));
long	mrg_pmintr __P((void));
void	mrg_fixupROMBase __P((caddr_t, caddr_t));
int	mrg_Delay __P((void));
void	mrg_VBLQueue __P((void));
void	mrg_init_stub_1 __P((void));
void	mrg_init_stub_2 __P((void));
void	mrg_1sec_timer_tick __P((void));
void	mrg_lvl1dtpanic __P((void));
void	mrg_lvl2dtpanic __P((void));
void	mrg_jadbprocpanic __P((void));
void	mrg_jswapmmupanic __P((void));
void	mrg_jkybdtaskpanic __P((void));
void	mrg_notrap __P((void));
int	myowntrap __P((void));
int	mrg_NewPtr __P((void));
int	mrg_DisposPtr __P((void));
int	mrg_GetPtrSize __P((void));
int	mrg_SetPtrSize __P((void));
int	mrg_PostEvent __P((void));
int	mrg_SetTrapAddress __P((void));
void	mrg_StripAddress __P((void));
void	mrg_aline_super __P((struct frame *));
void	mrg_aline_user __P((void));
void	mrg_init __P((void));
void	mrg_FixDiv __P((void));
void	mrg_FixMul __P((void));

/* machdep.c */
int	mach_cputype __P((void));

/* Tracing aids */

/* trace all instructions, not just flow changes. */
#define tron() \
	asm("movw sr, d0 ; orw #0x8000, d0 ; movw d0, sr" : : : "d0")
#define troff() \
	asm("movw sr, d0 ; andw #0x3fff, d0 ; movw d0, sr" : : : "d0")
