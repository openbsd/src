/*	$OpenBSD: macromasm.s,v 1.9 2005/02/20 18:08:08 martin Exp $	*/
/*	$NetBSD: macromasm.s,v 1.18 2000/11/15 07:15:36 scottr Exp $	*/

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

/*
 * Mac ROM Glue assembly
 */


#include "assym.h"
#include <machine/asm.h>
#include <machine/trap.h>


	/* Define this symbol as global with (v) value */
#define loglob(g, v)	\
	.global _/**/g ;\
	.set	_/**/g, v

	/* Define this as an externally available function */
#define function(f)	\
	.global _/**/f ;\
	_/**/f:

	/* Return from a pascal function; pop (pbytes) number of bytes */
	/*  passed as parameters.  Should have picked up "pascal" extension */
	/*  to GCC... */
#define pascalret(pbytes)	\
	movl	sp@+, a0		/* get PC (I hate Pascal) */ ; \
	addl	# pbytes , sp		/* pop params (I hate Pascal) */ ; \
	jra	a0@			/* return (I hate Pascal) */ ; \


/*
 * MacOS low-memory global variables.
 */
	loglob(ADBBase, 0xcf8)		/* ptr to ADB driver variables */
	loglob(ADBYMM, 0xd18)		/* Yet more memory used by ADB/PM */
	loglob(ADBDelay, 0xcea) 	/* 8s of dbras per ADB delay */
	loglob(ROMBase, 0x2ae)		/* ptr to ROM Base */
	loglob(Lvl1DT, 0x192)		/* VIA 1 interrupt table */
	loglob(Lvl2DT, 0x1b2)		/* VIA 2 interrupt table? */
	loglob(JADBProc, 0x6b8) 	/* ADBReinit pre/post-processing */
	loglob(jADBOp, 0x5f0)		/* pointer to ADBOp */
	loglob(DeviceList, 0x8a8)	/* ptr to first device entry */
	loglob(KbdLast, 0x218)		/* ptr to first device entry */
	loglob(KbdType, 0x21E)		/* ptr to first device entry */
	loglob(JKybdTask, 0x21A)	/* keyboard task jump ptr? */
	loglob(Lo3Bytes, 0x31a) 	/* contains 0x00ffffff */
	loglob(MinusOne, 0xa06) 	/* contains 0xffffffff */
	loglob(MMU32Bit, 0xcb2) 	/* MMU mode (uh-oh) 1 = 32 bit? */
	loglob(CPUFlag, 0x12f)		/* CPU type */
	loglob(MacJmp, 0x120)		/* ?? */
	loglob(Scratch8, 0x9fa) 	/* 8-byte scratch area */
	loglob(Scratch20, 0x1e4)	/* 20-byte scratch area */
	loglob(Ticks, 0x16a)		/* ticks since system startup */
	loglob(Time, 0x20c)		/* Sec since midnight, 1-1-1904 */
	loglob(TimeDBRA, 0xd00) 	/* dbra's per millisecond (short) */
	loglob(ToolScratch, 0x9ce)	/* another 8-byte scratch area */
	loglob(VIA, 0x1d4)		/* VIA1 base address */
	loglob(mrg_VIA2, 0xcec) 	/* VIA2 base address */
	loglob(SCCRd, 0x1d8)		/* SCC read base address */
	loglob(FinderName, 0x2e0)	/* Name of finder */
	loglob(jSwapMMU, 0xdbc) 	/* ptr to MMU swap routine */
	loglob(ADBState, 0xde0) 	/* ptr to ADB state information? */
	loglob(jUnimplTrap, 0x61c)	/* ptr to UnimplTrap routine */
	loglob(jEgret, 0x648)		/* ptr to Egret trap routine */
	loglob(HwCfgFlags , 0xb22)	/* 2 bytes, h/w config flags */
	loglob(HwCfgFlags2, 0xdd0)	/* 4 bytes, more h/w config flags */
	loglob(HwCfgFlags3, 0xdd4)	/* 4 bytes, more h/w config flags */
	loglob(ADBReInit_JTBL, 0xdd8)	/* 4 bytes, pointer to patch table */
	loglob(jClkNoMem, 0x54c)	/* Pointer to ClkNoMem function */
	loglob(PramTransfer, 0x1e4)	/* Transfer buffer used with PRam */
	loglob(SysParam, 0x1f8) 	/* Place where PRam data gets stored */
	loglob(ExpandMem, 0x2b6)	/* pointer to Expanded Memory used by */
					/*   newer ADB routines */
	loglob(VBLQueue, 0x160)		/* Vertical blanking Queue, unused ? */
	loglob(VBLQueue_head, 0x162)	/* Vertical blanking Queue, head */
	loglob(VBLQueue_tail, 0x166)	/* Vertical blanking Queue, tail */
	loglob(jDTInstall, 0xd9c)	/* Deferred task mgr trap handler */

	loglob(InitEgretJTVec, 0x2010)	/* pointer to a jump table for */
					/* InitEgret on AV machines */
	loglob(jCacheFlush, 0x6f4)	/* setup_pm() needs this */

#if 0
	/* I wish I knew what these things were */
	loglob(MMUFlags, 0xcb0)
	loglob(MMUFluff, 0xcb3)
	loglob(MMUTbl, 0xcb4)
	loglob(MMUTblSize, 0xcb8)
	loglob(MMUType, 0xcb1)
#endif

	.text
	.even
	.global _panic
	.global _printf

#ifdef MRG_ADB		/* These functions are defined here if using the
			 * MRG_ADB method of accessing the ADB/PRAM/RTC. 
			 * They are in adb_direct.c. */
/*
 * Most of the following glue just takes C function calls, converts
 * the parameters to the MacOS Trap parameters, and then tries to
 * return the result correctly.  About the only thing our C functions
 * and MacOS' traps have in common is returning numerical results in
 * d0.
 *
 * If some code actually pulls down the a-trap line, we jump right
 * to the ROMs; none of this is called. 
 */

/* Initialize Utils, mainly XPRam */
	.global _InitUtil
	/*
	 * void
	 */
_InitUtil:
	.word 0xa03f
	rts


/* Initialize the ADB ------------------------------------------------------*/
	.global _ADBReInit
	/*
	 * void
	 */
_ADBReInit:
	.word	0xa07b
	clrl	d0
	rts


/* Set the ADB device info for a device; routine handler and so on ---------*/
	.global _SetADBInfo
	/*
	 * sp@(4)	ADBSetInfoBlock *info
	 * sp@(8)	int		adbAddr
	 */
_SetADBInfo:
	movl	sp@(4), a0
	movl	sp@(8), d0
	.word	0xa07a
	rts


/* Find the number of ADB devices in the device table ----------------------*/
	.global _CountADBs
	/*
	 * void
	 */
_CountADBs:
	.word 0xa077
	rts


/* Get ADB entry from index in table ---------------------------------------*/
	.global _GetIndADB
	/*
	 * sp@(4)	ADBDataBlock	*info
	 * sp@(8)	u_short 	devTableIndex
	 */
_GetIndADB:
	movl	sp@(4), a0
	movl	sp@(8), d0
	.word 0xa078
	rts


/* Get ADB device information ----------------------------------------------*/
	.global _GetADBInfo /*
	/*
	 * sp@(4)	ADBSetInfoBlock *info
	 * sp@(8)	int		adbAddr
	 */
_GetADBInfo:
	movl	sp@(4), a0
	movl	sp@(8), d0
	.word 0xa079
	rts


/* Perform an ADB transaction ----------------------------------------------*/
	.global _ADBOp
	/*
	 * sp@(4)	Ptr	buffer
	 * sp@(8)	Ptr	compRout
	 * sp@(12)	Ptr	data
	 * sp@(16)	short	commandNum
	 */
_ADBOp:
	lea	sp@(4), a0
	movl	sp@(16), d0
	.word 0xa07c
	rts
#endif /* ifdef MRG_ADB */


#if 0
/* My Own Trap (for testing.  returns 50.) ---------------------------------*/
	.global _MyOwnTrap
_MyOwnTrap:
	.word	0xa000
	rts


/* Known RTS (for testing) -------------------------------------------------*/
	.global _KnownRTS
_KnownRTS:
	.word	0xa001
	rts
#endif


/* Allocate memory ---------------------------------------------------------*/
	function(NewPtr)
	/*
	 * int size
	 */
	movl	sp@(4), d0
	.word	0xa71e		/* clear and sys */
	movl	a0, d0
	rts


/* Free memory -------------------------------------------------------------*/
	function(DisposPtr)
	/*
	 * Ptr ptr
	 */
	movl	sp@(4), a0
	.word	0xa01f
	rts


/* Get size of allocated memory --------------------------------------------*/
	function(GetPtrSize)
	/*
	 * Ptr ptr
	 */
	movl	sp@(4), a0
	.word	0xa021
	rts


/* Extend allocated memory -------------------------------------------------*/
	function(SetPtrSize)
	/*
	 * Ptr ptr
	 * int bytesdiff
	 */
	movl	sp@(4), a0
	movl	sp@(8), d0
	.word	0xa020
	rts


/* Resource manager */
	.data
	.global _mrg_ResErr
_mrg_ResErr:
	.word	0

	.text
/* Return the current Resource Manager Error -------------------------------*/
	function(ResError)
	/*
	 * void
	 */
	movl	d2, sp@-		| Toolbox trap may alter d0-d2, a0, a1
					| but C caller would save d1,a0,a1
	clrw	sp@-			| space for return arg (ugh)
	.word	0xa9af			| ResError
	movw	sp@+, d0
	movl	sp@+, d2		| restore d2
	rts     

	/* pascal */ function(mrg_ResError)
	/*
	 * sp@(4)	:short
	 */
#if defined(MRG_SHOWTRAPS)
		movml	#0xc0c0, sp@-
		pea	LRE_enter
		jbsr	_printf
		addql	#4, sp
		movml	sp@+, #0x0303
#endif
	movw	_mrg_ResErr, sp@(4)
	| movw	d0, sp@(4)
	pascalret(0)

LRE_enter:
	.asciz	"mrg: ResError()\n"
	.even

/* Find a resource in open resource files ----------------------------------*/
	function(GetResource)
	/*
	 * sp@(4)	u_int theType
	 * sp@(8)	short theID
	 */
	movl	sp@(8), a1
	movl	sp@(4), a0
	movl	d2, sp@-		| Toolbox trap may alter d0-d2, a0, a1
					| but C caller would save d1,a0,a1
	clrl	sp@-			| space for :Handle
	movl	a0, sp@-
	movw	a1, sp@-		| pascal parameters upside down
	.word	0xa9a0			| GetResource
	movl	sp@+, d0		| return Handle
	movl	sp@+, d2		| restore registers
	rts

	/* pascal */ function(mrg_GetResource)
	/*
	 * sp@(10)	:Handle
	 * sp@(6)	u_int	theType
	 * sp@(4)	short	theID
	 */
	/* For now, we return NIL, because, well, we have no resources. */
#if defined(MRG_SHOWTRAPS)
		movml	#0xc0c0, sp@-
		movw	sp@(20), d0
		movl	sp@(22), d1
		movl	d0, sp@-
		movl	d1, sp@-
		pea	LGR_enter
		jbsr	_printf
		addl	#12, sp
		movml	sp@+, #0x0303
#endif
	clrl	d0			| okay to change d0 ?
	movl	d0, sp@(10)		| return value is NIL
	movl	#-192, d0		| resNotFound; that's pretty accurate.
	movw	d0, _mrg_ResErr 	| set current ResMan error
	pascalret(6)			| I hate Pascal.



function(mrg_CountResources)
/* Original from WRU: 960120
 * sp@(4)	u_int32_t  rsrc_type
 * sp@(8)	u_int16_t  nr_of_rsrcs
 */
	movl 	sp@(4), d0
  	movl	d0, sp@-
	jbsr	_Count_Resources
	addl	#4, sp			| pop C params
	movw	d0, sp@(8)		| store result
	pascalret(4)

function(mrg_GetIndResource)
/* Original from WRU: 960120
 * sp@(4)	u_int16_t  rsrc_index
 * sp@(6)	u_int32_t  rsrc_type
 * sp@(10)	caddr_t   *rsrc_handle
 */
	movl	sp@(6), a0
	clrl	d0
	movw	sp@(4), d0
  	movl	d0, sp@-
	movl	a0, sp@-
	jbsr	_Get_Ind_Resource
	addl	#8, sp		| pop C params
	movl	d0, sp@(10)	| store result
	pascalret(6)

/*
 * I'd like to take a moment here to talk about the calling convention
 * for ToolBox routines.  Inside Mac "Operating System Utilities,"
 * page 8-16, "About the Trap Manager," states that ToolBox routines
 * may alter D0-D2 and A0-A1.  However, a crucial bit of code in
 * ADBReInit on the Mac II, 0x40807834, does not save its own D1 or A1
 * before calling GetResource.	Therefore, it is imperative that our
 * MacBSD ToolBox trap handler save at least D1, D2, A0, and A1.  I
 * believe that the system uses D0 in most places to hold the function's
 * return value, as in "movl	sp@+, d0", and so I don't think it's
 * that necessary to save d0 unless we find a specific case of ugliness.
 *
 * It surprises me during every moment that I deal with the Macintosh
 * architecture how wonderful and ugly it really is.  I continue to find
 * pieces of beautiful, elegant code, reduced to crap by another following
 * piece of code which uses global offsets, doesn't save registers, and
 * makes assumptions.  If only it was consistent, Mac ROMs would be a
 * true example to programmers everywhere.  As it stands, it is an example
 * of a different kind. 	-Brad Grantham, September 5th, 1994
 */

LGR_enter:
	.asciz	"GetResource('0x%x', %d)\n"
	.even


/*
 * 1010 line emulator; A-line trap
 * (we fake MacOS traps from here)
 */
	.global _mrg_aline_super
	.global _mrg_ToolBoxtraps
	.global _alinetrap
_alinetrap:
	clrl	sp@-		| pad SR to longword (I still don't know
				| why we do this.)
	moveml	#0xffff,sp@-	| save registers
	movl	sp, sp@-	| save pointer to frame
	movw	sp@(FR_HW + 4), d0	| retrieve status register
	andw	#PSL_S, d0	| supervisor state?
	bne	Lalnosup	| branch if supervisor
	addql	#4, sp		| pop frame ptr
	movql	#T_ILLINST, d0	| user-mode fault
	jra	_ASM_LABEL(fault)
Lalnosup:
#define FR_PC (FR_HW+2)
	movl	sp@(FR_PC + 4), a0	| retrieve PC
	movw	a0@, d0 	| retrieve trap word
	btst	#11, d0 	| ToolBox trap?
	bne	Laltoolbox	| branch if ToolBox
	jbsr	_mrg_aline_super	| supervisor a-line trap
Lalrts:
	addql	#4, sp		| pop frame ptr
	movw	sp@(FR_HW), sr	| restore status register (I hate MacOS traps)
	movl	sp@(FR_PC), a0	| move PC to correct location
	movl	a0, sp@(FR_PC+2)
	moveml	sp@+, #0xffff	| restore registers (some of which may have
				| been magically changed)
	addql	#8, sp		| pop alignment long, make stack look like
				| ordinary jbsr
	tstw	d0		| Gotta do this because call might depend on it
	rts			| Go home (God, this is ugly.)
Laltoolbox:
	addql	#4, sp		| pop frame ptr
#if defined(MRG_DEBUG)
		movml	#0xC0C0, sp@-	| better save
		pea	LalP1	        
		jbsr	_printf
			| printf ("Toolbox trap\n"); 
		lea	sp@(4), sp	| pop
		movml	sp@+, #0x0303	| restore
#endif
	movl	a0, a1		| save PC
	movw	sp@(FR_HW), sr	| restore status register
#if defined(MRG_DEBUG)
		movml	#0xC0C0, sp@-	| better save
		movw	sr, sp@-
		clrw	sp@-		| coerce to int
		pea	LalP2
		jbsr	_printf
			| printf ("Status register 0x%x\n", sr);
		lea	sp@(8), sp	| pop
		movml	sp@+, #0x0303	| restore
#endif
	btst	#10, d0 	| auto-pop the jump address?
	beq	Lalnoauto	| branch if no auto-pop
	pea	Lalautopanic	| I really don't know how to handle this
	jbsr	_panic
Lalnoauto:
	addl	#2, a1		| add 2 to PC
#if defined(MRG_DEBUG)
		movml	#0xC0C0, sp@-	| better save
		movl	a1, sp@-
		pea	LalP4
		jbsr	_printf
			| printf ("return address is 0x%x\n", new pc);
		lea	sp@(8), sp	| pop
		movml	sp@+, #0x0303	| restore
#endif
	movl	a1, sp@(FR_PC+2)	| push new return address
	movl	d0, d1		| just in case of panic
	andl	#0x3ff, d0	| d0 = trap number
#if defined(MRG_DEBUG)
		movml	#0xC0C0, sp@-	| better save
		movl	d0, sp@-
		pea	LalP5
		jbsr	_printf
			| printf ("trap number is 0x%x\n", trapnum);
		lea	sp@(8), sp	| pop
		movml	sp@+, #0x0303	| restore
#endif
	lsll	#2, d0		| ptr = 4 bytes
	lea	_mrg_ToolBoxtraps, a0
	addl	d0, a0		| get trap address
	movl	a0@, a0
	bne	Laltbok 	| branch on trap addr non-zero
	movl	d1, sp@+	| trap word
	pea	Laltbnotrap
	jbsr	_printf
	pea	Laltbnogo
	jbsr	_panic
Laltbok:
#if defined(MRG_DEBUG)
		movml	#0xC0C0, sp@-	| better save
		movl	a0, sp@-
		pea	LalP6
		jbsr	_printf
			| printf ("trap address is 0x%x\n", trapaddr);
		lea	sp@(8), sp	| pop
		movml	sp@+, #0x0303	| restore
#endif
	movl	a0, sp@(FR_HW)	| we will RTS to trap routine (ick)
	moveml	sp@+, #0xffff	| restore registers
	addql	#4, sp		| pop alignment long
	rts			| go for it

Lalautopanic:
	.asciz	"mrg: A-line ToolBox trap wanted auto-pop; I don't know how"
Laltbnotrap:
	.asciz	"mrg: Don't know how to handle this trap: 0x%x\n"
Laltbnogo:
	.asciz	"mrg: can't go on"
LalP1:
	.asciz	"mrg: TB!"
LalP2:
	.asciz	" sr 0x%x"
LalP4:
	.asciz	" ret 0x%x"
LalP5:
	.asciz	" #%d"
LalP6:
	.asciz	" addr 0x%x\n"
	.even


	.data
	.global _traceloopptr
	.global _traceloopstart
	.global _traceloopend

_traceloopstart:
	.space	20 * 4		| save last 20 program counters on trace trap
_traceloopend:
_traceloopptr:
	.long	_traceloopstart

	.text
	.global _mrg_tracetrap
_mrg_tracetrap:
	movl	d0, sp@-		| save d0
	movl	a0, sp@-		| save a0
	movl	sp@(0x10), d0		| address of instruction
		| sp@	old a0
		|sp@(4) old d0
		|sp@(8) old sr
		|sp@(10) old PC
		|sp@(14) exception vector
		|sp@(16) address of instruction
#if defined(MRG_FOLLOW)
		movml	#0xc0c0, sp@-
		movl	d0, sp@-
		pea	Ltraceprint
		jbsr	_printf 	| printf("PC is %x\n", pc);
		addql	#8, sp
		movml	sp@+,#0x0303
		tstl	d0
#endif
	beq	LPCiszero		| if PC goes to zero, freak!
	movl	_traceloopptr, a0	| ptr = traceloopptr;
	movl	d0, a0@+		| *ptr++ = PC;
	cmpl	#_traceloopend, a0	| if(ptr == traceloopend)
	bne	Lnotpast		|  {
	movl	#_traceloopstart, a0	|   ptr = traceloopstart;
Lnotpast:				|  }
	movl	a0, _traceloopptr	| traceloopptr = ptr;
	movl	sp@+, a0		| restore a0
	movl	sp@+, d0		| restore d0
	rte				| everything cool, return.
LPCiszero:
	movl	sp@+, a0		| restore a0
	movl	sp@+, d0		| restore d0
		movml	#0xc0c0, sp@-
		pea	LtracePCzero
		jbsr	_panic		| printf("PC is zero!", pc);
		addql	#4, sp
		movml	sp@+,#0x0303

Ltraceprint:
	.asciz	"tracing, pc at 0x%08x\n"
LtracePCzero:
	.asciz	"PC went to zero!\n"
	.even
