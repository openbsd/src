#
# $OpenBSD: netbsd.S,v 1.3 1997/07/06 07:46:19 downsj Exp $
# $NetBSD: netbsd.S,v 1.3 1997/06/27 23:32:09 is Exp $
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
# os.s
#
# This file contains:
#	- example "Call-Out"s required by both the ISP and FPSP.
#

#
# make the copyright notice appear in the binary:
#
	.include	"copyright.S"

#################################
# EXAMPLE CALL-OUTS 		#
# 				#
# _060_dmem_write()		#
# _060_dmem_read()		#
# _060_imem_read()		#
# _060_dmem_read_byte()		#
# _060_dmem_read_word()		#
# _060_dmem_read_long()		#
# _060_imem_read_word()		#
# _060_imem_read_long()		#
# _060_dmem_write_byte()	#
# _060_dmem_write_word()	#
# _060_dmem_write_long()	#
#				#
# _060_real_trace()		#
# _060_real_access()		#
#################################

# 
# Each IO routine checks to see if the memory write/read is to/from user
# or supervisor application space. The examples below use simple "move"
# instructions for supervisor mode applications and call _copyin()/_copyout()
# for user mode applications.
# When installing the 060SP, the _copyin()/_copyout() equivalents for a 
# given operating system should be substituted.
#
# The addresses within the 060SP are guaranteed to be on the stack.
# The result is that Unix processes are allowed to sleep as a consequence
# of a page fault during a _copyout.
#

#
# _060_dmem_write():
#
# Writes to data memory while in supervisor mode.
#
# INPUTS:
#	a0 - supervisor source address	
#	a1 - user destination address
#	d0 - number of bytes to write	
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	.global	_060_dmem_write
_060_dmem_write:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	beqs	user_write
super_write:
	moveb	a0@+,a1@+	|# copy 1 byte
	subql	#0x1,d0		|# decr byte counter
	bnes	super_write	|# quit if ctr = 0
	clrl	d1		|# return success
	rts
user_write:
	movel	d0,sp@-		|# pass: counter
	movel	a1,sp@-		|# pass: user dst
	movel	a0,sp@-		|# pass: supervisor src
	bsrl	_copyout	|# write byte to user mem
	movel	d0,d1		|# return success
	addl	#0xc,sp		|# clear 3 lw params
	rts

#
# _060_imem_read(), _060_dmem_read():
#
# Reads from data/instruction memory while in supervisor mode.
#
# INPUTS:
#	a0 - user source address
#	a1 - supervisor destination address
#	d0 - number of bytes to read
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	.global	_060_imem_read
	.global	_060_dmem_read
_060_imem_read:
_060_dmem_read:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	beqs	user_read
super_read:
	moveb	a0@+,a1@+	|# copy 1 byte
	subql	#0x1,d0		|# decr byte counter
	bnes	super_read	|# quit if ctr = 0
	clrl	d1		|# return success
	rts
user_read:
	movel	d0,sp@-		|# pass: counter
	movel	a1,sp@-		|# pass: super dst
	movel	a0,sp@-		|# pass: user src
	bsrl	_copyin		|# read byte from user mem
	movel	d0,d1		|# return success
	addl	#0xc,sp		|# clear 3 lw params
	rts

#
# _060_dmem_read_byte():
# 
# Read a data byte from user memory.
#
# INPUTS:
#	a0 - user source address
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - data byte in d0
#	d1 - 0 = success, !0 = failure
#
	.global	_060_dmem_read_byte
_060_dmem_read_byte:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	bnes	dmrbs		|# supervisor
dmrbu:
	clrl	sp@-		|# clear space on stack for result
	movel	#0x1,sp@-	|# pass: # bytes to copy
	pea	sp@(0x7)	|# pass: dst addr (stack)
	movel	a0,sp@-		|# pass: src addr (user mem)
	bsrl	_copyin		|# "copy in" the data
	movel	d0,d1		|# return success
	addl	#0xc,sp		|# delete params
	movel	sp@+,d0		|# put answer in d0
	rts
dmrbs:
	clrl	d0		|# clear whole longword
	moveb	a0@,d0		|# fetch super byte
	clrl	d1		|# return success
	rts

#
# _060_imem_read_word():
# Read an instruction word from user memory.
#
# _060_dmem_read_word():
# Read a data word from user memory.
#
# INPUTS:
#	a0 - user source address
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - data word in d0
#	d1 - 0 = success, !0 = failure
#
	.global	_060_imem_read_word
	.global	_060_dmem_read_word

_060_imem_read_word:
_060_dmem_read_word:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	bnes	dmrws		|# supervisor
dmrwu:
	clrl	sp@-		|# clear result space on stack
	movel	#0x2,sp@-	|# pass: # bytes to copy
	pea	sp@(0x6)	|# pass: dst addr (stack)
	movel	a0,sp@-		|# pass: src addr (user mem)
	bsrl	_copyin		|# "copy in" the data
	movel	d0,d1		|# return success
	addl	#0xc,sp		|# delete params
	movel	sp@+,d0		|# put answer in d0
	rts
dmrws:
	clrl	d0		|# clear whole longword
	movew	a0@,d0		|# fetch super word
	clrl	d1		|# return success
	rts

# 
# _060_imem_read_long():
# Read an instruction longword from user memory.
#
# _060_dmem_read_long():
# Read an data longword from user memory.
# 

#
# INPUTS:
#	a0 - user source address
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - data longword in d0
#	d1 - 0 = success, !0 = failure
#

	.global	_060_dmem_read_long
	.global	_060_imem_read_long

_060_imem_read_long:
_060_dmem_read_long:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	bnes	dmrls		|# supervisor
dmrlu:
	subql	#0x4,sp		|# clear result space on stack
	movel	#0x4,sp@-	|# pass: # bytes to copy
	pea	sp@(0x4)	|# pass: dst addr (stack)
	movel	a0,sp@-		|# pass: src addr (user mem)
	bsrl	_copyin		|# "copy in" the data
	movel	d0,d1		|# return success
	addl	#0xc,sp		|# delete params
	movel	sp@+,d0		|# put answer in d0
	rts
dmrls:
	movel	a0@,d0		|# fetch super longword
	clrl	d1		|# return success
	rts

#
# _060_dmem_write_byte():
#
# Write a data byte to user memory.
#
# INPUTS:
#	a0 - user destination address
# 	d0 - data byte in d0
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	.global	_060_dmem_write_byte
_060_dmem_write_byte:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	bnes	dmwbs		|# supervisor
dmwbu:
	movel	d0,sp@-		|# put src on stack
	movel	#0x1,sp@-	|# pass: # bytes to copy
	movel	a0,sp@-		|# pass: dst addr (user mem)
	pea	sp@(0xb)	|# pass: src addr (stack)
	bsrl	_copyout	|# "copy out" the data
	movel	d0,d1		|# return success
	addl	#0x10,sp	|# delete params + src
	rts
dmwbs:
	moveb	d0,a0@		|# store super byte
	clrl	d1		|# return success
	rts

#
# _060_dmem_write_word():
#
# Write a data word to user memory.
#
# INPUTS:
#	a0 - user destination address
# 	d0 - data word in d0
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	.global	_060_dmem_write_word
_060_dmem_write_word:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	bnes	dmwws		|# supervisor
dmwwu:
	movel	d0,sp@-		|# put src on stack
	movel	#0x2,sp@-	|# pass: # bytes to copy
	movel	a0,sp@-		|# pass: dst addr (user mem)
	pea	sp@(0xa)	|# pass: src addr (stack)
	bsrl	_copyout	|# "copy out" the data
	movel	d0,d1		|# return success
	addl	#0x10,sp	|# delete params + src
	rts
dmwws:
	movew	d0,a0@		|# store super word
	clrl	d1		|# return success
	rts

#
# _060_dmem_write_long():
#
# Write a data longword to user memory.
#
# INPUTS:
#	a0 - user destination address
# 	d0 - data longword in d0
# 	a6@(0x4),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	.global	_060_dmem_write_long
_060_dmem_write_long:
	btst	#0x5,a6@(0x4)	|# check for supervisor state
	bnes	dmwls		|# supervisor
dmwlu:
	movel	d0,sp@-		|# put src on stack
	movel	#0x4,sp@-	|# pass: # bytes to copy
	movel	a0,sp@-		|# pass: dst addr (user mem)
	pea	sp@(0x8)	|# pass: src addr (stack)
	bsrl	_copyout	|# "copy out" the data
	movel	d0,d1		|# return success
	addl	#0x10,sp	|# delete params + src
	rts
dmwls:
	movel	d0,a0@		|# store super longword
	clrl	d1		|# return success
	rts

############################################################################

#
# _060_real_trace():
#
# This is the exit point for the 060FPSP when an instruction is being traced
# and there are no other higher priority exceptions pending for this instruction
# or they have already been processed.
#
# The sample code below simply executes an "rte".
#
	.global	_060_real_trace,_trace
_060_real_trace:
	jra	_trace

#
# _060_real_access():
#
# This is the exit point for the 060FPSP when an access error exception
# is encountered. The routine below should point to the operating system
# handler for access error exceptions. The exception stack frame is an
# 8-word access error frame.
#
# We jump directly to the 68060 buserr handler.
# If we had a sane ld, we could use use that entry point directly...
#
	.globl	_060_real_access,_buserr60
_060_real_access:
	jra	_buserr60

	.include	"inetbsd.S"
	.include	"fnetbsd.S"
