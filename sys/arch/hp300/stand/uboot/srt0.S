/*	$OpenBSD: srt0.S,v 1.7 2011/08/18 19:54:19 miod Exp $	*/
/*	$NetBSD: srt0.S,v 1.4 1997/05/12 07:56:00 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: srt0.c 1.18 92/12/21$
 *
 *	@(#)srt0.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Startup code for standalone system
 */

#include <machine/asm.h>
#include <machine/hp300spu.h>

	STACK =	   0xfffff000		| below the ROM page
	BOOTTYPE = 0xfffffdc0
	LOWRAM =   0xfffffdce
	SYSFLAG =  0xfffffed2		| system flags
	MSUS =	   0xfffffedc		| MSUS (?) structure
	VECTORS =  0xfffffee0		| beginning of jump vectors
	NMIRESET = 0xffffff9c		| reset vector
	BUSERR =   0xfffffffc
	MAXADDR =  0xfffff000
	NBPG =	   4096
	MMUCMD =   0x005f400c		| MMU command/status register

	.data
GLOBAL(bootdev)
	.long	0
GLOBAL(howto)
	.long	0
GLOBAL(lowram)
	.long	0
GLOBAL(machineid)
	.long	0
GLOBAL(mmuid)
	.long	0

	.text
ASENTRY_NOPROFILE(begin)
	movl	#STACK,sp
	moveq	#47,d0			| # of vectors - 1
	movl	#VECTORS+2,a0		| addr part of first vector
1:
	movl	#_ASM_LABEL(__trap),a0@	| make it direct to __trap
	addql	#6,a0			| move to next vector addr
	dbf	d0,1b			| go til done
	movl	#NMIRESET,a0		| NMI keyboard reset addr
	movl	#nmi,a0@		| catch in reset routine

/*
 * Determine our SPU type and look for internal HP-IB
 */
	lea	_C_LABEL(machineid),a0
	movl	#0x808,d0
	movc	d0,cacr			| clear and disable on-chip cache(s)
	movl	#0x200,d0		| data freeze bit
	movc	d0,cacr			|   only exists on 68030
	movc	cacr,d0			| read it back
	tstl	d0			| zero?
	jeq	not68030		| yes, we have 68020/68040

	/*
	 * 68030 models
	 */

	movl	#0x808,d0
	movc	d0,cacr			| clear data freeze bit again
	movl	#0x80,MMUCMD		| set magic cookie
	movl	MMUCMD,d0		| read it back
	btst	#7,d0			| cookie still on?
	jeq	not370			| no, 360 or 375
	movl	#HP_370,a0@		| consider a 370 for now
	movl	#0,MMUCMD		| clear magic cookie
	movl	MMUCMD,d0		| read it back
	btst	#7,d0			| still on?
	jeq	ihpibcheck		| no, a 370
	movl	#HP_340,a0@		| yes, must be a 340
	jra	ihpibcheck

not370:
	movl	#HP_36X,a0@		| type is at least a 360
	movl	#0,MMUCMD		| clear magic cookie2
	movl	MMUCMD,d0		| read it back
	btst	#16,d0			| still on?
	jeq	ihpibcheck		| no, a 360 or a 362
	lsrl	#MMUID_SHIFT,d0		| save MMU ID
	andl	#MMUID_MASK,d0
	movl	d0,_C_LABEL(mmuid)	| save mmuid
	cmpb	#MMUID_345,d0		| are we a 345?
	jeq	isa345
	cmpb	#MMUID_375,d0		| how about a 375?
	jeq	isa375
	movl	#HP_400,a0@		| must be a 400
	jra	ihpibcheck
isa345:
	movl	#HP_345,a0@
	jra	ihpibcheck
isa375:
	movl	#HP_375,a0@
	jra	ihpibcheck

	/*
	 * End of 68030 section
	 */

not68030:
	bset	#31,d0			| data cache enable bit
	movc	d0,cacr			|   only exists on 68040
	movc	cacr,d0			| read it back
	tstl	d0			| zero?
	beq	is68020			| yes, we have 68020
	moveq	#0,d0			| now turn it back off
	movec	d0,cacr			|   before we access any data

	/*
	 * 68040 models
	 */

	.long	0x4e7b0004		| movc d0,itt0
	.long	0x4e7b0005		| movc d0,itt1
	.long	0x4e7b0006		| movc d0,dtt0
	.long	0x4e7b0007		| movc d0,dtt1
	.word	0xf4d8			| cinva bc
	movl	MMUCMD,d0		| get MMU ID
	lsrl	#MMUID_SHIFT,d0
	andl	#MMUID_MASK,d0
	movl	d0,_C_LABEL(mmuid)	| save mmuid
	cmpb	#MMUID_425_T,d0		| are we a 425t?
	jeq	isa425
	cmpb	#MMUID_425_S,d0		| how about 425s?
	jeq	isa425
	cmpb	#MMUID_425_E,d0		| or maybe a 425e?
	jeq	isa425
	cmpb	#MMUID_433_T,d0		| or a 433t?
	jeq	isa433
	cmpb	#MMUID_433_S,d0		| maybe a 433s?
	jeq	isa433
	cmpb	#MMUID_385,d0		| then a 385?
	jeq	isa385
	cmpb	#MMUID_382,d0		| last chance...
	jeq	isa382
	movl	#HP_380,a0@		| guess we are a 380
	jra	ihpibcheck
isa425:
	movl	#HP_425,a0@
	jra	ihpibcheck
isa433:
	movl	#HP_433,a0@
	jra	ihpibcheck
isa385:
	movl	#HP_385,a0@
	jra	ihpibcheck
isa382:
	movl	#HP_382,a0@
	jra	ihpibcheck

	/*
	 * End 68040 section
	 */

	/*
	 * 68020 models
	 */

is68020:
	movl	#HP_330,a0@		| consider a 330 for now
	movl	#1,MMUCMD		| a 68020, write HP MMU location
	movl	MMUCMD,d0		| read it back
	btst	#0,d0			| zero?
	jeq	ihpibcheck		| yes, a 330
	movl	#HP_320,a0@		| no, consider a 320 for now
	movl	#0x80,MMUCMD		| set magic cookie
	movl	MMUCMD,d0		| read it back
	btst	#7,d0			| cookie still on?
	jeq	ihpibcheck		| no, just a 320
	movl	#HP_350,a0@		| yes, a 350

	/*
	 * End 68020 section
	 */

ihpibcheck:
	movl	#0,MMUCMD		| make sure MMU is off
	btst	#5,SYSFLAG		| do we have an internal HP-IB?
	jeq	boottype		| yes, continue
	clrl	_C_LABEL(internalhpib)	| no, clear the internal address
/*
 * If this is a reboot, extract howto/bootdev stored by kernel
 */
boottype:
	cmpw	#12,BOOTTYPE		| is this a reboot (REQ_REBOOT)?
	jne	notreboot		| no, skip
	lea	MAXADDR,a0		| find last page
	movl	a0@+,d7			| and extract howto, bootdev
	movl	a0@+,d6			|   from where doboot() left them
	jra	boot1
/*
 * At this point we do not know which logical device the MSUS select
 * code refers to so we cannot construct bootdev.  So we just punt
 * and let configure() construct it.
 */
notreboot:
	moveq	#0,d6			| make sure bootdev is invalid
	cmpw	#18,BOOTTYPE		| does the user want to interact?
	jeq	askme			| yes, go to it
	moveq	#0,d7			| default to RB_AUTOBOOT
	jra	boot1
askme:
	moveq	#3,d7			| default to RB_SINGLE|RB_ASKNAME
boot1:
	movl	d6,_C_LABEL(bootdev)	| save bootdev and howto
	movl	d7,_C_LABEL(howto)	|   globally so all can access
	movl	LOWRAM,d0		| read lowram value from bootrom
	/*
	 * Must preserve the scratch area for the BOOT ROM.
	 * Round up to the next 8k boundary.
	 */
	addl	#((2*NBPG)-1),d0
	andl	#-(2*NBPG),d0
	movl	d0,_C_LABEL(lowram)	| stash that value
start:
	movl	#_C_LABEL(edata),a2	| start of BSS
	movl	#_C_LABEL(end),a3	| end
1:
	clrb	a2@+			| clear BSS
	cmpl	a2,a3			| done?
	bne	1b			| no, keep going
	jsr	_C_LABEL(configure)	| configure critical devices
	jsr	_C_LABEL(main)		| lets go
GLOBAL(_rtt)
	movl	#3,_C_LABEL(howto)	| restarts get RB_SINGLE|RB_ASKNAME
	jmp	start

/*
 * probe a location and see if it causes a bus error
 */
ENTRY_NOPROFILE(badaddr)
	movl	BUSERR,_C_LABEL(_bsave)	| save ROM bus error handler address
	movl	sp,_C_LABEL(_ssave)	| and current stack pointer
	movl	#catchbad,BUSERR	| plug in our handler
	movl	sp@(4),a0		| address to probe
	movw	a0@,d1			| do it
	movl	_C_LABEL(_bsave),BUSERR	| if we got here, it did not fault
	clrl	d0			| return that this was not a bad addr
	rts

catchbad:
	movl	_C_LABEL(_bsave),BUSERR	| got a bus error, so restore
					|   old handler
	movl	_C_LABEL(_ssave),sp	| manually restore stack
	moveq	#1,d0			| indicate that we got a fault
	rts				| return to caller of badaddr()

	.data
GLOBAL(_bsave)
	.long	0

GLOBAL(_ssave)
	.long	0

ASENTRY_NOPROFILE(__trap)
	moveml	#0xFFFF,sp@-		| save registers
	movl	sp,sp@-			| push pointer to frame
	jsr	_C_LABEL(trap)		| call C routine to deal with it
	tstl	d0
	jeq	Lstop
	addql	#4,sp
	moveml	sp@+,#0x7FFF
	addql	#8,sp
	rte
Lstop:
	stop	#0x2700			| stop cold

ASENTRY_NOPROFILE(nmi)
	movw	#18,BOOTTYPE		| mark as system switch
	jsr	_C_LABEL(kbdnmi)	| clear the interrupt, and
					|   reset the system
	stop	#0			| SCREEEECH!

ENTRY_NOPROFILE(call_req_reboot)
	jmp	0x1A4			| call ROM reboot function
	rts				| XXX: just in case?

ENTRY_NOPROFILE(romout)
	movl	sp@(4),d0		| line number
	movl	sp@(8),a0		| string
	jsr	0x150			| do it
	rts
