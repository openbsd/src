#
# $OpenBSD: fnetbsd.S,v 1.2 1996/05/30 22:14:05 niklas Exp $
# $NetBSD: fnetbsd.S,v 1.2 1996/05/15 21:16:45 is Exp $
#
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
# M68000 Hi-Performance Microprocessor Division
# M68060 Software Package Production Release 
# 
# M68060 Software Package Copyright (C) 1993, 1994, 1995, 1996 Motorola Inc.
# All rights reserved.
# 
# THE SOFTWARE is provided on an "AS IS" basis and without warranty.
# To the maximum extent permitted by applicable law,
# MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
# INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS
# FOR A PARTICULAR PURPOSE and any warranty against infringement with
# regard to the SOFTWARE (INCLUDING ANY MODIFIED VERSIONS THEREOF)
# and any accompanying written materials. 
# 
# To the maximum extent permitted by applicable law,
# IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
# (INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
# BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY LOSS)
# ARISING OF THE USE OR INABILITY TO USE THE SOFTWARE.
# 
# Motorola assumes no responsibility for the maintenance and support
# of the SOFTWARE.  
# 
# You are hereby granted a copyright license to use, modify, and distribute the
# SOFTWARE so long as this entire notice is retained without alteration
# in any modified and/or redistributed versions, and that such modified
# versions are clearly identified as such.
# No licenses are granted by implication, estoppel or otherwise under any
# patents or trademarks of Motorola, Inc.
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 
# Derived from: 
# fskeleton.s
#
# This file contains:
#	(1) example "Call-out"s
#	(2) example package entry code
#	(3) example "Call-out" table
#


#################################
# (1) EXAMPLE CALL-OUTS		#
#				#
# _060_fpsp_done()		#
# _060_real_ovfl()		#
# _060_real_unfl()		#
# _060_real_operr()		#
# _060_real_snan()		#
# _060_real_dz()		#
# _060_real_inex()		#
# _060_real_bsun()		#
# _060_real_fline()		#
# _060_real_fpu_disabled()	#
# _060_real_trap()		#
#################################

#
# _060_fpsp_done():
#
# This is the main exit point for the 68060 Floating-Point
# Software Package. For a normal exit, all 060FPSP routines call this
# routine. The operating system can do system dependent clean-up or
# simply execute an "rte" as with the sample code below.
#
	.global	_060_fpsp_done
_060_fpsp_done:
	rte

#
# _060_real_ovfl():
#
# This is the exit point for the 060FPSP when an enabled overflow exception
# is present. The routine below should point to the operating system handler 
# for enabled overflow conditions. The exception stack frame is an overflow
# stack frame. The FP state frame holds the EXCEPTIONAL OPERAND.
#
# The sample routine below simply clears the exception status bit and
# does an "rte".
#
	.global	_060_real_ovfl
_060_real_ovfl:
	fsave	sp@-
	movew	#0x6000,sp@(0x2)
	frestore	sp@+
	jmp	_fpfault

#
# _060_real_unfl():
#
# This is the exit point for the 060FPSP when an enabled underflow exception
# is present. The routine below should point to the operating system handler 
# for enabled underflow conditions. The exception stack frame is an underflow
# stack frame. The FP state frame holds the EXCEPTIONAL OPERAND.
#
# The sample routine below simply clears the exception status bit and
# does an "rte".
#
	.global	_060_real_unfl
_060_real_unfl:
	fsave	sp@-
	movew	#0x6000,sp@(0x2)
	frestore	sp@+
	jmp	_fpfault

#
# _060_real_operr():
#
# This is the exit point for the 060FPSP when an enabled operand error exception
# is present. The routine below should point to the operating system handler 
# for enabled operand error exceptions. The exception stack frame is an operand error
# stack frame. The FP state frame holds the source operand of the faulting
# instruction.
#
# The sample routine below simply clears the exception status bit and
# does an "rte".
#
	.global	_060_real_operr
_060_real_operr:
	fsave	sp@-
	movew	#0x6000,sp@(0x2)
	frestore	sp@+
	jmp	_fpfault

#
# _060_real_snan():
#
# This is the exit point for the 060FPSP when an enabled signalling NaN exception
# is present. The routine below should point to the operating system handler 
# for enabled signalling NaN exceptions. The exception stack frame is a signalling NaN
# stack frame. The FP state frame holds the source operand of the faulting
# instruction.
#
# The sample routine below simply clears the exception status bit and
# does an "rte".
#
	.global	_060_real_snan
_060_real_snan:
	fsave	sp@-
	movew	#0x6000,sp@(0x2)
	frestore	sp@+
	jmp	_fpfault

#
# _060_real_dz():
#
# This is the exit point for the 060FPSP when an enabled divide-by-zero exception
# is present. The routine below should point to the operating system handler 
# for enabled divide-by-zero exceptions. The exception stack frame is a divide-by-zero
# stack frame. The FP state frame holds the source operand of the faulting
# instruction.
#
# The sample routine below simply clears the exception status bit and
# does an "rte".
#
	.global	_060_real_dz
_060_real_dz:
	fsave	sp@-
	movew	#0x6000,sp@(0x2)
	frestore	sp@+
	jmp	_fpfault

#
# _060_real_inex():
#
# This is the exit point for the 060FPSP when an enabled inexact exception
# is present. The routine below should point to the operating system handler 
# for enabled inexact exceptions. The exception stack frame is an inexact
# stack frame. The FP state frame holds the source operand of the faulting
# instruction.
#
# The sample routine below simply clears the exception status bit and
# does an "rte".
#
	.global	_060_real_inex
_060_real_inex:
	fsave	sp@-
	movew	#0x6000,sp@(0x2)
	frestore	sp@+
	jmp	_fpfault

#
# _060_real_bsun():
#
# This is the exit point for the 060FPSP when an enabled bsun exception
# is present. The routine below should point to the operating system handler 
# for enabled bsun exceptions. The exception stack frame is a bsun
# stack frame.
#
# The sample routine below clears the exception status bit, clears the NaN
# bit in the FPSR, and does an "rte". The instruction that caused the 
# bsun will now be re-executed but with the NaN FPSR bit cleared.
#
	.global	_060_real_bsun
_060_real_bsun:
	fsave	sp@-

	fmovel	fpsr,sp@-
	andib	#0xfe,sp@
	fmovel	sp@+,fpsr

	addl	#0xc,sp
	jmp	_fpfault

#
# _060_real_fline():
#
# This is the exit point for the 060FPSP when an F-Line Illegal exception is 
# encountered. Three different types of exceptions can enter the F-Line exception
# vector number 11: FP Unimplemented Instructions, FP implemented instructions when
# the FPU is disabled, and F-Line Illegal instructions. The 060FPSP module
# _fpsp_fline() distinguishes between the three and acts appropriately. F-Line
# Illegals branch here.
# 
	.global	_060_real_fline
_060_real_fline:
	jmp	_fpfault

#
# _060_real_fpu_disabled():
#
# This is the exit point for the 060FPSP when an FPU disabled exception is 
# encountered. Three different types of exceptions can enter the F-Line exception
# vector number 11: FP Unimplemented Instructions, FP implemented instructions when
# the FPU is disabled, and F-Line Illegal instructions. The 060FPSP module
# _fpsp_fline() distinguishes between the three and acts appropriately. FPU disabled
# exceptions branch here.
#
# The sample code below enables the FPU, sets the PC field in the exception stack
# frame to the PC of the instruction causing the exception, and does an "rte".
# The execution of the instruction then proceeds with an enabled floating-point
# unit.
#
	.global	_060_real_fpu_disabled
_060_real_fpu_disabled:
	movel	d0,sp@-		|# enabled the fpu

	.short	0x4e7a,0x0808		|* movec.l pcr,d0
	bclr	#0x1,d0
	.short	0x4e7b,0x0808		|* movec.l d0,pcr
	movel	sp@+,d0

	movel	sp@(0xc),sp@(0x2)		|# set "Current PC"
	rte

#
# _060_real_trap():
#
# This is the exit point for the 060FPSP when an emulated "ftrapcc" instruction
# discovers that the trap condition is true and it should branch to the operating
# system handler for the trap exception vector number 7.
#
# The sample code below simply executes an "rte".
#
	.global	_060_real_trap
_060_real_trap:
	rte

#############################################################################

##################################
# (2) EXAMPLE PACKAGE ENTRY CODE #
##################################

	.global	_060_fpsp_snan
_060_fpsp_snan:
	bral	_FP_CALL_TOP+0x80+0x00

	.global	_060_fpsp_operr
_060_fpsp_operr:
	bral	_FP_CALL_TOP+0x80+0x08

	.global	_060_fpsp_ovfl
_060_fpsp_ovfl:
	bral	_FP_CALL_TOP+0x80+0x10

	.global	_060_fpsp_unfl
_060_fpsp_unfl:
	bral	_FP_CALL_TOP+0x80+0x18

	.global	_060_fpsp_dz
_060_fpsp_dz:
	bral	_FP_CALL_TOP+0x80+0x20

	.global	_060_fpsp_inex
_060_fpsp_inex:
	bral	_FP_CALL_TOP+0x80+0x28

	.global	_060_fpsp_fline
_060_fpsp_fline:
	bral	_FP_CALL_TOP+0x80+0x30

	.global	_060_fpsp_unsupp
_060_fpsp_unsupp:
	bral	_FP_CALL_TOP+0x80+0x38

	.global	_060_fpsp_effadd
_060_fpsp_effadd:
	bral	_FP_CALL_TOP+0x80+0x40

#############################################################################

################################
# (3) EXAMPLE CALL-OUT SECTION #
################################

# The size of this section MUST be 128 bytes!!!

	.global	_FP_CALL_TOP
_FP_CALL_TOP:
	.long	_060_real_bsun-_FP_CALL_TOP
	.long	_060_real_snan-_FP_CALL_TOP
	.long	_060_real_operr-_FP_CALL_TOP
	.long	_060_real_ovfl-_FP_CALL_TOP
	.long	_060_real_unfl-_FP_CALL_TOP
	.long	_060_real_dz-_FP_CALL_TOP
	.long	_060_real_inex-_FP_CALL_TOP
	.long	_060_real_fline-_FP_CALL_TOP
	.long	_060_real_fpu_disabled-_FP_CALL_TOP
	.long	_060_real_trap-_FP_CALL_TOP
	.long	_060_real_trace-_FP_CALL_TOP
	.long	_060_real_access-_FP_CALL_TOP
	.long	_060_fpsp_done-_FP_CALL_TOP

	.long	0x00000000,0x00000000,0x00000000

	.long	_060_imem_read-_FP_CALL_TOP
	.long	_060_dmem_read-_FP_CALL_TOP
	.long	_060_dmem_write-_FP_CALL_TOP
	.long	_060_imem_read_word-_FP_CALL_TOP
	.long	_060_imem_read_long-_FP_CALL_TOP
	.long	_060_dmem_read_byte-_FP_CALL_TOP
	.long	_060_dmem_read_word-_FP_CALL_TOP
	.long	_060_dmem_read_long-_FP_CALL_TOP
	.long	_060_dmem_write_byte-_FP_CALL_TOP
	.long	_060_dmem_write_word-_FP_CALL_TOP
	.long	_060_dmem_write_long-_FP_CALL_TOP

	.long	0x00000000

	.long	0x00000000,0x00000000,0x00000000,0x00000000

#############################################################################

# 060 FPSP KERNEL PACKAGE NEEDS TO GO HERE!!!

	.include	"fpsp.S"

