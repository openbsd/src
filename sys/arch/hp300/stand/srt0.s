/*	$OpenBSD: srt0.s,v 1.4 1997/07/13 07:21:53 downsj Exp $	*/
/*	$NetBSD: srt0.s,v 1.2 1997/03/10 08:00:47 thorpej Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

	.globl	begin
	.globl	_end
	.globl	_edata
	.globl	_main
	.globl	_configure
	.globl	__rtt
	.globl	_bootdev,_howto,_lowram,_machineid
	.globl	_internalhpib

	STACK =	   0xfffff000	| below the ROM page
	BOOTTYPE = 0xfffffdc0
	LOWRAM =   0xfffffdce
	SYSFLAG =  0xfffffed2	| system flags
	MSUS =	   0xfffffedc	| MSUS (?) structure
	VECTORS =  0xfffffee0	| beginning of jump vectors
	NMIRESET = 0xffffff9c	| reset vector
	BUSERR =   0xfffffffc
	MAXADDR =  0xfffff000
	NBPG =	   4096
	MMUCMD =   0x005f400c	| MMU command/status register

	.data
_bootdev:
	.long	0
_howto:
	.long	0
_lowram:
	.long	0
_machineid:
	.long	0

	.text
begin:
	movl	#STACK,sp
	moveq	#47,d0		| # of vectors - 1
	movl	#VECTORS+2,a0	| addr part of first vector
vecloop:
	movl	#trap,a0@	| make it direct to trap
	addql	#6,a0		| move to next vector addr
	dbf	d0,vecloop	| go til done
	movl	#NMIRESET,a0	| NMI keyboard reset addr
	movl	#nmi,a0@	| catch in reset routine
/*
 * Determine our SPU type and look for internal HP-IB
 */
	lea	_machineid,a0
	movl	#0x808,d0
	movc	d0,cacr		| clear and disable on-chip cache(s)
	movl	#0x200,d0	| data freeze bit
	movc	d0,cacr		|   only exists on 68030
	movc	cacr,d0		| read it back
	tstl	d0		| zero?
	jeq	not68030	| yes, we have 68020/68040

	movl	#0x808,d0
	movc	d0,cacr		| clear data freeze bit again

	/*
	 * 68030 models
	 */

	movl	#0x80,MMUCMD	| set magic cookie
	movl	MMUCMD,d0	| read it back
	btst	#7,d0		| cookie still on?
	jeq	not370		| no, 360 or 375
	movl	#4,a0@		| consider a 370 for now
	movl	#0,MMUCMD	| clear magic cookie
	movl	MMUCMD,d0	| read it back
	btst	#7,d0		| still on?
	jeq	ihpibcheck	| no, a 370
	movl	#5,a0@		| yes, must be a 340
	jra	ihpibcheck

not370:
	movl	#3,a0@		| type is at least a 360
	movl	#0,MMUCMD	| clear magic cookie2
	movl	MMUCMD,d0	| read it back
	btst	#16,d0		| still on?
	jeq	ihpibcheck	| no, a 360
	lsrl	#8,d0		| save MMU ID
	andl	#0xff,d0
	cmpb	#1,d0		| are we a 345?
	jeq	isa345
	cmpb	#3,d0		| how about a 375?
	jeq	isa375
	movl	#8,a0@		| must be a 400
	jra	ihpibcheck
isa345:
	movl	#6,a0@
	jra	ihpibcheck
isa375:
	movl	#7,a0@
	jra	ihpibcheck

	/*
	 * End of 68030 section
	 */

not68030:
	bset	#31,d0		| data cache enable bit
	movc	d0,cacr		|   only exists on 68040
	movc	cacr,d0		| read it back
	tstl	d0		| zero?
	beq	is68020		| yes, we have 68020
	moveq	#0,d0		| now turn it back off
	movec	d0,cacr		|   before we access any data

	.long	0x4e7b0004	| movc d0,itt0
	.long	0x4e7b0005	| movc d0,itt1
	.long	0x4e7b0006	| movc d0,dtt0
	.long	0x4e7b0007	| movc d0,dtt1
	.word	0xf4d8		| cinva bc

	/*
	 * 68040 models
	 */

	movl	MMUCMD,d0	| get MMU register
	lsrl	#8,d0
	andl	#0xff,d0
	cmpb	#5,d0		| are we a 425t?
	jeq	isa425
	cmpb	#7,d0		| how about 425s?
	jeq	isa425
	cmpb	#4,d0		| or a 433t?
	jeq	isa433
	cmpb	#6,d0		| last chance...
	jeq	isa433
	movl	#9,a0@		| guess we're a 380
	jra	ihpibcheck
isa425:
	movl	#10,a0@
	jra	ihpibcheck
isa433:
	movl	#11,a0@
	jra	ihpibcheck

	/*
	 * End 68040 section
	 */

	/*
	 * 68020 models
	 */

is68020:
	movl	#1,a0@		| consider a 330 for now
	movl	#1,MMUCMD	| a 68020, write HP MMU location
	movl	MMUCMD,d0	| read it back
	btst	#0,d0		| zero?
	jeq	ihpibcheck	| yes, a 330
	movl	#0,a0@		| no, consider a 320 for now
	movl	#0x80,MMUCMD	| set magic cookie
	movl	MMUCMD,d0	| read it back
	btst	#7,d0		| cookie still on?
	jeq	ihpibcheck	| no, just a 320
	movl	#2,a0@		| yes, a 350

	/*
	 * End 68020 section
	 */

ihpibcheck:
	movl	#0,MMUCMD	| make sure MMU is off
	btst	#5,SYSFLAG	| do we have an internal HP-IB?
	jeq	boottype	| yes, continue
	clrl	_internalhpib	| no, clear the internal address
/*
 * If this is a reboot, extract howto/bootdev stored by kernel
 */
boottype:
	cmpw	#12,BOOTTYPE	| is this a reboot (REQ_REBOOT)?
	jne	notreboot	| no, skip
	lea	MAXADDR,a0	| find last page
	movl	a0@+,d7		| and extract howto, bootdev
	movl	a0@+,d6		|   from where doboot() left them
	jra	boot1
/*
 * At this point we do not know which logical device the MSUS select
 * code refers to so we cannot construct bootdev.  So we just punt
 * and let configure() construct it.
 */
notreboot:
	moveq	#0,d6		| make sure bootdev is invalid
	cmpw	#18,BOOTTYPE	| does the user want to interact?
	jeq	askme		| yes, go to it
	moveq	#0,d7		| default to RB_AUTOBOOT
	jra	boot1
askme:
	moveq	#3,d7		| default to RB_SINGLE|RB_ASKNAME
boot1:
	movl	d6,_bootdev	| save bootdev and howto
	movl	d7,_howto	|   globally so all can access
	movl	LOWRAM,d0	| read lowram value from bootrom
	/*
	 * Must preserve the scratch area for the BOOT ROM.
	 * Round up to the next 8k boundary.
	 */
	addl	#((2*NBPG)-1),d0
	andl	#-(2*NBPG),d0
	movl	d0,_lowram	| stash that value
start:
	movl	#_edata,a2	| start of BSS
	movl	#_end,a3	| end
clr:
	clrb	a2@+		| clear BSS
	cmpl	a2,a3		| done?
	bne	clr		| no, keep going
	jsr	_configure	| configure critical devices
	jsr	_main		| lets go
__rtt:
	movl	#3,_howto	| restarts get RB_SINGLE|RB_ASKNAME
	jmp	start

/*
 * probe a location and see if it causes a bus error
 */
	.globl	_badaddr
_badaddr:
	movl	BUSERR,__bsave	| save ROM bus error handler address
	movl	sp,__ssave	| and current stack pointer
	movl	#catchbad,BUSERR| plug in our handler
	movl	sp@(4),a0	| address to probe
	movw	a0@,d1		| do it
	movl	__bsave,BUSERR	| if we got here, it did not fault
	clrl	d0		| return that this was not a bad addr
	rts

catchbad:
	movl	__bsave,BUSERR	| got a bus error, so restore old handler
	movl	__ssave,sp	| manually restore stack
	moveq	#1,d0		| indicate that we got a fault
	rts			| return to caller of badaddr()

__bsave:
	.long	0
__ssave:
	.long	0

	.globl	_trap
trap:
	moveml	#0xFFFF,sp@-	| save registers
	movl	sp,sp@-		| push pointer to frame
	jsr	_trap		| call C routine to deal with it
	tstl	d0
	jeq	Lstop
	addql	#4,sp
	moveml	sp@+,#0x7FFF
	addql	#8,sp
	rte
Lstop:
	stop	#0x2700		| stop cold

nmi:
	movw	#18,BOOTTYPE	| mark as system switch
	jsr	_kbdnmi		| clear the interrupt, and
				|   reset the system
	stop	#0		| SCREEEECH!

	.globl _call_req_reboot
_call_req_reboot:
	jmp	0x1A4		| call ROM reboot function
	rts			| XXX: just in case?

	.globl	_romout
_romout:
	movl	sp@(4),d0	| line number
	movl	sp@(8),a0	| string
	jsr	0x150		| do it
	rts
