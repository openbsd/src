/*	$NetBSD: vectors.s,v 1.3 1995/11/30 21:52:50 leo Exp $	*/

/*
 * Copyright (c) 1988 University of Utah
 * Copyright (c) 1990 Regents of the University of California.
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
 *	@(#)vectors.s	7.2 (Berkeley) 5/7/91
 */
	.text
	.globl	_buserr,_addrerr
	.globl	_illinst,_zerodiv,_chkinst,_trapvinst,_privinst,_trace
	.globl	_badtrap
	.globl	_spurintr,_lev1intr,_lev2intr,_lev3intr
	.globl	_lev4intr,_lev5intr,_lev6intr,_lev7intr
	.globl	_trap0,_trap1,_trap2,_trap15
	.globl	_fpfline, _fpunsupp, _fpfault
	.globl	_trap12, _badmfpint

Lvectab:
	.long	0x4ef80400	| 0: jmp 0x400:w (unused reset SSP)
	.long	0		| 1: NOT USED (reset PC)
	.long	_buserr		| 2: bus error
	.long	_addrerr	| 3: address error
	.long	_illinst	| 4: illegal instruction
	.long	_zerodiv	| 5: zero divide
	.long	_chkinst	| 6: CHK instruction
	.long	_trapvinst	| 7: TRAPV instruction
	.long	_privinst	| 8: privilege violation
	.long	_trace		| 9: trace
	.long	_illinst	| 10: line 1010 emulator
	.long	_fpfline	| 11: line 1111 emulator
	.long	_badtrap	| 12: unassigned, reserved
	.long	_coperr		| 13: coprocessor protocol violation
	.long	_fmterr		| 14: format error
	.long	_badtrap	| 15: uninitialized interrupt vector
	.long	_badtrap	| 16: unassigned, reserved
	.long	_badtrap	| 17: unassigned, reserved
	.long	_badtrap	| 18: unassigned, reserved
	.long	_badtrap	| 19: unassigned, reserved
	.long	_badtrap	| 20: unassigned, reserved
	.long	_badtrap	| 21: unassigned, reserved
	.long	_badtrap	| 22: unassigned, reserved
	.long	_badtrap	| 23: unassigned, reserved
	.long	_spurintr	| 24: spurious interrupt
	.long	_lev1intr	| 25: level 1 interrupt autovector
	.long	_lev2intr	| 26: level 2 interrupt autovector
	.long	_lev3intr	| 27: level 3 interrupt autovector
	.long	_lev4intr	| 28: level 4 interrupt autovector
	.long	_lev5intr	| 29: level 5 interrupt autovector
	.long	_lev6intr	| 30: level 6 interrupt autovector
	.long	_lev7intr	| 31: level 7 interrupt autovector
	.long	_trap0		| 32: syscalls
	.long	_trap1		| 33: sigreturn syscall or breakpoint
	.long	_trap2		| 34: breakpoint or sigreturn syscall
	.long	_illinst	| 35: TRAP instruction vector
	.long	_illinst	| 36: TRAP instruction vector
	.long	_illinst	| 37: TRAP instruction vector
	.long	_illinst	| 38: TRAP instruction vector
	.long	_illinst	| 39: TRAP instruction vector
	.long	_illinst	| 40: TRAP instruction vector
	.long	_illinst	| 41: TRAP instruction vector
	.long	_illinst	| 42: TRAP instruction vector
	.long	_illinst	| 43: TRAP instruction vector
	.long	_trap12		| 44: TRAP instruction vector
	.long	_illinst	| 45: TRAP instruction vector
	.long	_illinst	| 46: TRAP instruction vector
	.long	_trap15		| 47: TRAP instruction vector
#ifdef FPSP
	.globl	bsun, inex, dz, unfl, operr, ovfl, snan
	.long	bsun		| 48: FPCP branch/set on unordered cond
	.long	inex		| 49: FPCP inexact result
	.long	dz		| 50: FPCP divide by zero
	.long	unfl		| 51: FPCP underflow
	.long	operr		| 52: FPCP operand error
	.long	ovfl		| 53: FPCP overflow
	.long	snan		| 54: FPCP signalling NAN
#else
	.globl	_fpfault
	.long	_fpfault	| 48: FPCP branch/set on unordered cond
	.long	_fpfault	| 49: FPCP inexact result
	.long	_fpfault	| 50: FPCP divide by zero
	.long	_fpfault	| 51: FPCP underflow
	.long	_fpfault	| 52: FPCP operand error
	.long	_fpfault	| 53: FPCP overflow
	.long	_fpfault	| 54: FPCP signalling NAN
#endif


	.long	_fpunsupp	| 55: FPCP unimplemented data type
	.long	_badtrap	| 56: unassigned, reserved
	.long	_badtrap	| 57: unassigned, reserved
	.long	_badtrap	| 58: unassigned, reserved
	.long	_badtrap	| 59: unassigned, reserved
	.long	_badtrap	| 60: unassigned, reserved
	.long	_badtrap	| 61: unassigned, reserved
	.long	_badtrap	| 62: unassigned, reserved
	.long	_badtrap	| 63: unassigned, reserved

	/*
	 * MFP 1 auto vectors (ipl 6)
	 */
	.long	_badmfpint	|  64: parallel port - BUSY
	.long	_badmfpint	|  65: modem port 1 - DCD
	.long	_badmfpint	|  66: modem port 1 - CTS
	.long	_badmfpint	|  67: unassigned
	.long	_badmfpint	|  68: modem port 1 baudgen (Timer D)
#ifdef STATCLOCK
	.long	mfp_timc	|  69: Timer C {stat,prof}clock
#else
	.long	_badmfpint	|  69: Timer C
#endif /* STATCLOCK */
	.long	mfp_kbd		|  70: KBD/MIDI IRQ
	.long	mfp_fd_acsi	|  71: FDC/ACSI DMA
	.long	_badmfpint	|  72: Display enable counter
	.long	_badmfpint	|  73: modem port 1 - XMIT error
	.long	_badmfpint	|  74: modem port 1 - XMIT buffer empty
	.long	_badmfpint	|  75: modem port 1 - RCV error	
	.long	_badmfpint	|  76: modem port 1 - RCV buffer full
	.long	mfp_tima	|  77: Timer A (System clock)
	.long	_badmfpint	|  78: modem port 1 - RI
	.long	_badmfpint	|  79: Monochrome detect

	/*
	 * MFP 2 auto vectors (ipl 6)
	 */
	.long	_badmfpint	|  80: I/O pin 1 J602
	.long	_badmfpint	|  81: I/O pin 3 J602
	.long	_badmfpint	|  82: SCC-DMA
	.long	_badmfpint	|  83: modem port 2 - RI
	.long	_badmfpint	|  84: serial port 1 baudgen (Timer D)
	.long	_badmfpint	|  85: TCCLC SCC (Timer C)
	.long	_badmfpint	|  86: FDC Drive Ready
	.long	mfp2_5380dm	|  87: SCSI DMA
	.long	_badmfpint	|  88: Display enable (Timer B)
	.long	_badmfpint	|  89: serial port 1 - XMIT error
	.long	_badmfpint	|  90: serial port 1 - XMIT buffer empty
	.long	_badmfpint	|  91: serial port 1 - RCV error
	.long	_badmfpint	|  92: serial port 1 - RCV buffer full
	.long	_badmfpint	|  93: Timer A
	.long	_badmfpint	|  94: RTC
	.long	mfp2_5380	|  95: SCSI 5380

	/*
	 * Interrupts from the 8530 SCC
	 */
	.long	sccint		|  96: SCC Tx empty channel B
	.long	_badtrap	|  97: Not used
	.long	sccint		|  98: SCC Ext./Status Channel B
	.long	_badtrap	|  99: Not used
	.long	sccint		| 100: SCC Rx Channel B
	.long	_badtrap	| 101: Not used
	.long	sccint		| 102: SCC Special Rx cond.  Channel B
	.long	_badtrap	| 103: Not used
	.long	sccint		| 104: SCC Tx empty channel A
	.long	_badtrap	| 105: Not used
	.long	sccint		| 106: SCC Ext./Status Channel A
	.long	_badtrap	| 107: Not used
	.long	sccint		| 108: SCC Rx Channel A
	.long	_badtrap	| 109: Not used
	.long	sccint		| 110: SCC Special Rx cond.  Channel A
	.long	_badtrap	| 111: Not used

#define BADTRAP16	.long	_badtrap,_badtrap,_badtrap,_badtrap,\
				_badtrap,_badtrap,_badtrap,_badtrap,\
				_badtrap,_badtrap,_badtrap,_badtrap,\
				_badtrap,_badtrap,_badtrap,_badtrap
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
	BADTRAP16		| 112-255: user interrupt vectors
