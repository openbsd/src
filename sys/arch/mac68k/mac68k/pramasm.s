/*	$OpenBSD: pramasm.s,v 1.4 1997/04/14 18:48:08 gene Exp $	*/
/*	$NetBSD: pramasm.s,v 1.4 1995/09/28 03:15:54 briggs Exp $	*/

/*
 * RTC toolkit version 1.08b, copyright 1995, erik vogan
 *
 * All rights and privledges to this code hereby donated
 * to the ALICE group for use in NetBSD.  see the copyright
 * below for more info...
 */
/*
 * Copyright (c) 1995 Erik Vogan
 * All rights reserved.
 *
 * This code is derived from software contributed to the Alice Group
 * by Erik Vogan.
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  The following are the C interface functions to RTC access functions
 * that are defined later in this file.
 */

#ifdef MRG_ADB		/* These routines are defined at all
                         * if using the MRG_ADB method for accessing
                         * the ADB/PRAM/RTC. */

	.text

	.even
.globl _readPram
_readPram:
	link	a6,#-4		|  create a little home for ourselves
	.word	0xa03f		|  _InitUtil to read PRam
	moveml	d1/a1,sp@-
	moveq	#0,d0		|  zero out length register
	moveb	a6@(19),d0	|  move the length byte in
	moveq	#0,d1		|  zero out location 
	moveb	a6@(15),d1	|  now get out PRam location
	lea	_SysParam,a1	|  start of PRam data
	movel	a6@(8),a0	|  get our data address
_readPramAgain:
	subql	#1,d0
	bcs	_readPramDone	|  see if we are through
	moveb	a1@(d1),a0@+	|  transfer byte
	addql	#1,d1		|  next byte
	jmp	_readPramAgain	|  do it again 
_readPramDone:
	clrw	d0
	moveml	sp@+,d1/a1
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller


.globl _writePram
_writePram:
	link	a6,#-4		|  create a little home for ourselves
	.word	0xa03f		|  _InitUtil to read PRam in the case it hasn't been read yet
	moveml	d1/a1,sp@-
	moveq	#0,d0		|  zero out length register
	moveb	a6@(19),d0	|  move the length byte in
	moveq	#0,d1		|  zero out location 
	moveb	a6@(15),d1	|  now get out PRam location
	lea	_SysParam,a1	|  start of PRam data
	movel	a6@(8),a0	|  get our data address
_writePramAgain:
	subql	#1,d0
	bcs	_writePramDone	|  see if we are through
	cmpil	#0x14,d1	|  check for end of _SysParam
	bcc	_writePramDone	|  do not write if beyond end
	moveb	a0@+,a1@(d1)	|  transfer byte
	addql	#1,d1		|  next byte
	jmp	_writePramAgain |  do it again 
_writePramDone:
	.word	0xa038		|  writeParam
	moveml	sp@+,d1/a1
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller


.globl _readExtPram
_readExtPram:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	movel	a6@(8),a0	|  get our data address
	.word	0xa051		|  and go read the data
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller

.globl _writeExtPram
_writeExtPram:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	movel	a6@(8),a0	|  get our data address
	.word	0xa052		|  and go write the data
	unlk a6 		|  clean up after ourselves
	rts			|  and return to caller

.globl _getPramTime
_getPramTime:
	link	a6,#-4		|  create a little home for ourselves
	.word	0xa03f		|  call the routine to read the time (_InitUtil)
	movel	_Time,d0
	unlk	a6		|  clean up after ourselves
	rts			|  and return to caller

.globl _setPramTime
_setPramTime:
	link	a6,#-4		|  create a little home for ourselves
	movel	a6@(8),d0	|  get the passed in long (seconds since 1904)
	.word	0xa03a		|  call the routine to write the time
	unlk	a6		|  clean up after ourselves
	rts			|  and return to caller

#else				/* The following routines are the hardware
				 * specific routines for the machines that
				 * use the II-like method to access the PRAM,
				 * and are only defined when the MRG_ADB method
				 * isn't used to access the PRAM. */

/*
 *  The following are the C interface functions to RTC access functions
 * that are defined later in this file.
 */

	.text

	.even
.globl _readPramII
_readPramII:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	oriw	#0x0100,d0	|  and set up for non-extended read
	movel	a6@(8),a0	|  get our data address
	jbsr	_PRAMacc	|  and go read the data
	unlk a6			|  clean up after ourselves
	rts			|  and return to caller

.globl _writePramII
_writePramII:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	nop			|  and set up for non-extended write 
	movel	a6@(8),a0	|  get our data address
	jbsr	_PRAMacc	|  and go write the data
	unlk a6			|  clean up after ourselves
	rts			|  and return to caller

.globl _readExtPramII
_readExtPramII:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	oriw	#0x0300,d0	|  and set up for extended read
	movel	a6@(8),a0	|  get our data address
	jbsr	_PRAMacc	|  and go read the data
	unlk a6			|  clean up after ourselves
	rts			|  and return to caller

.globl _writeExtPramII
_writeExtPramII:
	link	a6,#-4		|  create a little home for ourselves
	moveq	#0,d0		|  zero out our future command register
	moveb	a6@(19),d0	|  move the length byte in
	swap	d0		|  and make that the MSW
	moveb	a6@(15),d0	|  now get out PRAM location
	oriw	#0x0200,d0	|  and set up for extended write
	movel	a6@(8),a0	|  get our data address
	jbsr	_PRAMacc	|  and go write the data
	unlk a6			|  clean up after ourselves
	rts			|  and return to caller

.globl _getPramTimeII
_getPramTimeII:
	link	a6,#-4		|  create a little home for ourselves
	jbsr	_readClock	|  call the routine to read the time
	unlk	a6		|  clean up after ourselves
	rts			|  and return to caller

.globl _setPramTimeII
_setPramTimeII:
	link	a6,#-4		|  create a little home for ourselves
	movel	a6@(8),d0	|  get the passed in long (seconds since 1904)
	jbsr	_writeClock	|  call the routine to write the time
	unlk	a6		|  clean up after ourselves
	rts			|  and return to caller

/*
 *  The following are the RTC access functions used by the interface
 * routines, above.
 */

_readClock:
	moveml	#0x7cc0, sp@-	| store off the regs we need
	moveq	#00,d0		| zero out our result reg
readagan:
	moveq	#00,d5		| and our temp result reg
	moveq	#03,d4		| set our count down reg to 4
	movel	#0x00000081,d1	| read sec byte 0 first
getSecb:
	bsr	_Transfer	| get that byte
	rorl	#8,d5		| shift our time to the right
	swap	d1		| we want to access our new data
	moveb	d1,d5		| move that byte to the spot we vacated
	swap	d1		| return our PRAM command to orig. config
	addqb	#4,d1		| increment to the next sec byte
	dbf	d4,getSecb	| any more bytes to get ?
	cmpl	d5,d0		| same secs value we as we just got ?
	beq	gotTime		| we got a good time value
	movel	d5,d0		| copy our current time to the compare reg
	bra	readagan	| read the time again
gotTime:
	rorl	#8,d0		| make that last shift to correctly order
				|  time bytes!!!
	movel	#0x00d50035,d1	| we have to set the write protect bit
				| so the clock doesn't run down !
	bsr	_Transfer	| (so sezs Apple...)
	moveml	sp@+, #0x033e	| restore our regs
	rts			| and return to caller

_writeClock:
	moveml	#0x78c0, sp@-	| store off the regs we need
	moveq	#03,d4		| set our count down reg to 4
	movel	#0x00550035,d1	| de-write-protect the PRAM
	bsr	_Transfer	| so we can set our value
	moveq	#1,d1		| write sec byte 0 first
putSecb:
	swap	d1		| we want access to data byte of command
	moveb	d0,d1		| set our first secs byte
	swap	d1		| and return command to orig. config
	bsr	_Transfer	| write that byte
	rorl	#8,d0		| shift our time to the right 
	addqb	#4,d1		| increment to the next sec byte
	dbf	d4,putSecb	| any more bytes to put ?
	movel	#0x00d50035,d1	| we have to set the write protect bit
				| so the clock doesn't run down !
	bsr	_Transfer	| (so sezs Apple...)
	moveml	sp@+, #0x031e	| restore our regs
	rts			| and return to caller

_PRAMacc:
	moveml	#0xf8c0, sp@-	| store off the regs we'll use
	moveq	#00,d3		| zero out our command reg
	moveq	#00,d4		| zero out our count reg too
	swap	d0		| we want the length byte
	movew	d0,d4		| copy length byte to our counter reg
	swap	d0		| and return command reg to prior state
	subqb	#1,d4		| predecrement counter for use w/ DBF
	movew	d0,d2		| copy command to d2
	rorw	#8,d2		| rotate copy to examine flags
	roxrw	#1,d2		| read/write bit out of param.
	roxlb	#1,d3		| and into command reg
	tstb	d3		| was it read (1) or write (0) ?
	bne	NoWrit		| go around de-write protect logic
	movel	#0x00550035,d1	| clear write protect bit of PRAM
				| (we really only need to zero the high
				|  bit, but other patterns don't work! )
	moveml	#0x3000, sp@-	| store off the regs that'll change
	bsr	_Transfer	| and go de-write protect RTC
	moveml	sp@+, #0x000c	| reclaim our reg values
NoWrit:
	andib	#1,d2		| isolate the extended command bit
	beq	oldPRAM		| it's zero, so do old PRAM style access
NuPRAM:
	moveb	d0,d2		| reget our PRAM location
	lslw	#4,d3		| insert our template blanks
	moveq	#2,d1		| set bit counter for 3 cycles
threebit:
	roxlb	#1,d2		| rotate address bit from d2
	roxlw	#1,d3		| and into command in d3
	dbf	d1,threebit	| until we've done bits 7-5
	lslw	#1,d3		| and add a bit spacer
	moveq	#4,d1		| ok, 5 bits to go...
fivebit:
	roxlb	#1,d2		| another addr bit out of d2
	roxlw	#1,d3		| and into command template in d3
	dbf	d1,fivebit	| til we've done bit 4-0
	lslw	#2,d3		| more bit magic
	oriw	#0x3880,d3	| set extended command bits
	bra	Loaddata	| go load the rest of command for xfer rtn
oldPRAM:
	moveb	d0,d2		| reget our PRAM location
	lslb	#1,d3		| add a template blank (bit)
	rolb	#4,d2		| get low nibble of PRAM loc ready
	moveq	#3,d1		| set our bit counter for 4 cycles
fourbit:
	roxlb	#1,d2		| bit out of PRAM loc
	roxlb	#1,d3		| and bit into PRAM command
	dbf	d1,fourbit	| until we've done the low nibble
	lslb	#2,d3		| bump bits to type of command byte
	orib	#0x41,d3	| set command bits (for access to $0-F!)
	btst	#4,d2		| change to access $10-13 ?
	beq	Loaddata	| nope, should stay the way it is
	andib	#0x8F,d3	| clear bits 4-6 of current command
	orib	#0x20,d3	| and set bit 5 (now accesses $10-13)
Loaddata:
	moveb	a0@,d1		| get our (data/dummy) byte into d1
	swap	d1		| move (data/dummy) byte to MSW
	movew	d3,d1		| now move command into d1
tagain:	
	bsr	_Transfer	| now execute that command
	swap	d1		| we want access to (data/dummy) byte
	moveb	d1,a0@+		| move (data/dummy) byte back to a0,
	moveb	a0@,d1		| NEXT VICTIM!!
	swap	d1		| now we want to tweak the command
	addqw	#4,d1		| increment our memory addr by 1 (this even 
				| works if we want to dump across 32 byte
				| boundries for an extended command!!!
				| thanks to the oriw #$3880 above !!!)
	dbf	d4,tagain	| repeat until we've got all we want
	movel	#0x00d50035,d1	| remember that command to write the wp byte ?
				| set the high bit in the wp reg (Apple sezs
				| this way the battery won't wear down !! )
	bsr	_Transfer	| so we'll play by the rules
	moveml	sp@+, #0x031f	| restore all our registers
	rts			| and return to our gracious caller

_Transfer:
	movew	sr,sp@-		| store the SR (we'll change it!)
	oriw	#0x0700,sr	| disable all interrupts
	moveal	_Via1Base,a1	| move VIA1 addr in reference reg
	moveq	#0,d2		| zero out d2 (it'll hold VIA1 reg B contents)
	moveb	a1@,d2		| and get VIA1 reg B contents
	andib	#0xF8,d2	| don't touch any but RTC bits
				| (and zero all those)
	movew	d1,d3		| we want to manipulate our command
	andiw	#0xFF00,d3	| zero the LSB
	beq	oldPRAMc	| do an old PRAM style command
xPRAMc:
	rorw	#8,d1		| swap the command bytes (1st byte of 2)
	bsr	writebyte	| and write the command byte
	rorw	#8,d1		| swap the command bytes again (2nd byte of 2)
	bsr	writebyte	| write that byte to RTC too
	moveq	#0x1F,d3	| r/w bit is $F for an extended command
				| (but command is swapped to MSW!! so add $10)
	bra	Rwbrnch		| go figure out if it's a read or a write cmd
oldPRAMc:
	bsr	writebyte	| only one byte for an old PRAM command
	moveq	#0x17,d3	| r/w bit is $7 for and old PRAM command
				| ( command is swapped to MSW, add $10)
Rwbrnch:
	swap	d1		| better get that (data/dummy) byte ready
	btst	d3,d1		| test bit no. d3 of reg d1 (read or write ?)
	beq	Wtrue		| 0 = write, 1 = read (branch on write)
Rtrue:
	bsr	readbyte	| read a byte from the RTC
	bra	Cleanup		| and call mom to clean up after us
Wtrue:
	bsr	writebyte	| write the data to the RTC
Cleanup:
	swap	d1		| move command to LSW again
	bset	#2,a1@		| bring the RTC enable line high (end of xfer)
	movew	sp@+,sr		| restore prior interrupt status
	rts			| and return to caller

writebyte:
	moveq	#7,d3		| set our bit counter to 8
wagain:	
	lsrb	#1,d2		| ditch the old data channel value
	roxlb	#1,d1		| and move a new value to X
	roxlb	#1,d2		| now move value from X to data channel
	moveb	d2,a1@		| set our VIA1 reg B contents to match
	bset	#1,a1@		| and finish strobing the clock line
	dbf	d3,wagain	| do this until we've sent a whole byte
	lsrb	#1,d2		| ditch the data channel value one last time
	roxlb	#1,d1		| get rid of the extra X bit we've carried
	lslb	#1,d2		| and restore d2 to prior status
	rts			| return to caller

readbyte:
	moveq	#7,d3		| set our bit counter to 8
	bclr	#0,a1@(0x0400)	| set VIA1 reg B data line to input
ragain:
	bclr	#1,a1@		| strobe the clock line to make
	bset	#1,a1@		| the data valid
	moveb	a1@,d2		| and get out data byte	
	lsrb	#1,d2		| get the data channel value to X
	roxlb	#1,d1		| and move X to data byte
	dbf	d3,ragain	| do this until we've received a whole byte
	bset	#0,a1@(0x0400)	| and return RTC data line to output
	rts			| return to caller

#endif 				/* ifndef MRG_ADB */

