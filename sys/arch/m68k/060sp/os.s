#
# $OpenBSD: os.s,v 1.2 1996/05/30 22:15:04 niklas Exp $
# $NetBSD: os.s,v 1.2 1996/05/15 19:49:06 is Exp $
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
# os.s
#
# This file contains:
#	- example "Call-Out"s required by both the ISP and FPSP.
#


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
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	global		_060_dmem_write
_060_dmem_write:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	beq.b		user_write
super_write:
	mov.b		(%a0)+,(%a1)+		# copy 1 byte
	subq.l		&0x1,%d0		# decr byte counter
	bne.b		super_write		# quit if ctr = 0
	clr.l		%d1			# return success
	rts
user_write:
	mov.l		%d0,-(%sp)		# pass: counter
	mov.l		%a1,-(%sp)		# pass: user dst
	mov.l		%a0,-(%sp)		# pass: supervisor src
	bsr.l		_copyout		# write byte to user mem
	mov.l		%d0,%d1			# return success
	add.l		&0xc, %sp		# clear 3 lw params
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
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	global 		_060_imem_read
	global		_060_dmem_read
_060_imem_read:
_060_dmem_read:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	beq.b		user_read
super_read:
	mov.b		(%a0)+,(%a1)+		# copy 1 byte
	subq.l		&0x1,%d0		# decr byte counter
	bne.b		super_read		# quit if ctr = 0
	clr.l		%d1			# return success
	rts
user_read:
	mov.l		%d0,-(%sp)		# pass: counter
	mov.l		%a1,-(%sp)		# pass: super dst
	mov.l		%a0,-(%sp)		# pass: user src
	bsr.l		_copyin			# read byte from user mem
	mov.l		%d0,%d1			# return success
	add.l		&0xc,%sp		# clear 3 lw params
	rts

#
# _060_dmem_read_byte():
# 
# Read a data byte from user memory.
#
# INPUTS:
#	a0 - user source address
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - data byte in d0
#	d1 - 0 = success, !0 = failure
#
	global 		_060_dmem_read_byte
_060_dmem_read_byte:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		dmrbs			# supervisor
dmrbu:	clr.l		-(%sp)			# clear space on stack for result
	mov.l		&0x1,-(%sp)		# pass: # bytes to copy
	pea		0x7(%sp)		# pass: dst addr (stack)
	mov.l		%a0,-(%sp)		# pass: src addr (user mem)
	bsr.l		_copyin			# "copy in" the data
	mov.l		%d0,%d1			# return success
	add.l		&0xc,%sp		# delete params
	mov.l		(%sp)+,%d0		# put answer in d0
	rts
dmrbs:	clr.l		%d0			# clear whole longword
	mov.b		(%a0),%d0		# fetch super byte
	clr.l		%d1			# return success
	rts

#
# _060_dmem_read_word():
# 
# Read a data word from user memory.
#
# INPUTS:
#	a0 - user source address
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - data word in d0
#	d1 - 0 = success, !0 = failure
#
	global 		_060_dmem_read_word
_060_dmem_read_word:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		dmrws			# supervisor
dmrwu:	clr.l		-(%sp)			# clear space on stack for result
	mov.l		&0x2,-(%sp)		# pass: # bytes to copy
	pea		0x6(%sp)		# pass: dst addr (stack)
	mov.l		%a0,-(%sp)		# pass: src addr (user mem)
	bsr.l		_copyin			# "copy in" the data
	mov.l		%d0,%d1			# return success
	add.l		&0xc,%sp		# delete params
	mov.l		(%sp)+,%d0		# put answer in d0
	rts
dmrws:	clr.l		%d0			# clear whole longword
	mov.w		(%a0), %d0		# fetch super word
	clr.l		%d1			# return success
	rts

#
# _060_dmem_read_long():
# 

#
# INPUTS:
#	a0 - user source address
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - data longword in d0
#	d1 - 0 = success, !0 = failure
#
	global 		_060_dmem_read_long
_060_dmem_read_long:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		dmrls			# supervisor
dmrlu:	subq.l		&0x4,%sp		# clear space on stack for result
	mov.l		&0x4,-(%sp)		# pass: # bytes to copy
	pea		0x4(%sp)		# pass: dst addr (stack)
	mov.l		%a0,-(%sp)		# pass: src addr (user mem)
	bsr.l		_copyin			# "copy in" the data
	mov.l		%d0,%d1			# return success
	add.l		&0xc,%sp		# delete params
	mov.l		(%sp)+,%d0		# put answer in d0
	rts
dmrls:	mov.l		(%a0),%d0		# fetch super longword
	clr.l		%d1			# return success
	rts

#
# _060_dmem_write_byte():
#
# Write a data byte to user memory.
#
# INPUTS:
#	a0 - user destination address
# 	d0 - data byte in d0
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	global 		_060_dmem_write_byte
_060_dmem_write_byte:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		dmwbs			# supervisor
dmwbu:	mov.l		%d0,-(%sp)		# put src on stack
	mov.l		&0x1,-(%sp)		# pass: # bytes to copy
	mov.l		%a0,-(%sp)		# pass: dst addr (user mem)
	pea		0xb(%sp)		# pass: src addr (stack)
	bsr.l		_copyout		# "copy out" the data
	mov.l		%d0,%d1			# return success
	add.l		&0x10,%sp		# delete params + src
	rts
dmwbs:	mov.b		%d0,(%a0)		# store super byte
	clr.l		%d1			# return success
	rts

#
# _060_dmem_write_word():
#
# Write a data word to user memory.
#
# INPUTS:
#	a0 - user destination address
# 	d0 - data word in d0
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	global 		_060_dmem_write_word
_060_dmem_write_word:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		dmwws			# supervisor
dmwwu:	mov.l		%d0,-(%sp)		# put src on stack
	mov.l		&0x2,-(%sp)		# pass: # bytes to copy
	mov.l		%a0,-(%sp)		# pass: dst addr (user mem)
	pea		0xa(%sp)		# pass: src addr (stack)
	bsr.l		_copyout		# "copy out" the data
	mov.l		%d0,%d1			# return success
	add.l		&0x10,%sp		# delete params + src
	rts
dmwws:	mov.w		%d0,(%a0)		# store super word
	clr.l		%d1			# return success
	rts

#
# _060_dmem_write_long():
#
# Write a data longword to user memory.
#
# INPUTS:
#	a0 - user destination address
# 	d0 - data longword in d0
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d1 - 0 = success, !0 = failure
#
	global 		_060_dmem_write_long
_060_dmem_write_long:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		dmwls			# supervisor
dmwlu:	mov.l		%d0,-(%sp)		# put src on stack
	mov.l		&0x4,-(%sp)		# pass: # bytes to copy
	mov.l		%a0,-(%sp)		# pass: dst addr (user mem)
	pea		0x8(%sp)		# pass: src addr (stack)
	bsr.l		_copyout		# "copy out" the data
	mov.l		%d0,%d1			# return success
	add.l		&0x10,%sp		# delete params + src
	rts
dmwls:	mov.l		%d0,(%a0)		# store super longword
	clr.l		%d1			# return success
	rts

#
# _060_imem_read_word():
# 
# Read an instruction word from user memory.
#
# INPUTS:
#	a0 - user source address
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - instruction word in d0
#	d1 - 0 = success, !0 = failure
#
	global 		_060_imem_read_word
_060_imem_read_word:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		imrws			# supervisor
imrwu:	clr.l		-(%sp)			# clear space on stack for result
	mov.l		&0x2,-(%sp)		# pass: # bytes to copy
	pea		0x6(%sp)		# pass: dst addr (stack)
	mov.l		%a0,-(%sp)		# pass: src addr (user mem)
	bsr.l		_copyin			# "copy in" the data
	mov.l		%d0,%d1			# return success
	add.l		&0xc,%sp		# delete params
	mov.l		(%sp)+,%d0		# put answer in d0
	rts
imrws:	mov.w		(%a0),%d0		# fetch super word
	clr.l		%d1			# return success
	rts

#
# _060_imem_read_long():
# 
# Read an instruction longword from user memory.
#
# INPUTS:
#	a0 - user source address
# 	0x4(%a6),bit5 - 1 = supervisor mode, 0 = user mode
# OUTPUTS:
#	d0 - instruction longword in d0
#	d1 - 0 = success, !0 = failure
#
	global 		_060_imem_read_long
_060_imem_read_long:
	btst		&0x5,0x4(%a6)		# check for supervisor state
	bne.b		imrls			# supervisor
imrlu:	subq.l		&0x4,%sp		# clear space on stack for result
	mov.l		&0x4,-(%sp)		# pass: # bytes to copy
	pea		0x4(%sp)		# pass: dst addr (stack)
	mov.l		%a0,-(%sp)		# pass: src addr (user mem)
	bsr.l		_copyin			# "copy in" the data
	mov.l		%d0,%d1			# return success
	add.l		&0xc,%sp		# delete params
	mov.l		(%sp)+,%d0		# put answer in d0
	rts
imrls:	mov.l		(%a0),%d0		# fetch super longword
	clr.l		%d1			# return success
	rts

################################################

#
# Use these routines if your kernel doesn't have _copyout/_copyin equivalents.
# Assumes that D0/D1/A0/A1 are scratch registers. The _copyin/_copyout
# below assume that the SFC/DFC have been set previously.
#

#
# int _copyout(supervisor_addr, user_addr, nbytes)
#
	global 		_copyout
_copyout:
	mov.l		4(%sp),%a0		# source
	mov.l		8(%sp),%a1		# destination
	mov.l		12(%sp),%d0		# count
moreout:
	mov.b		(%a0)+,%d1		# fetch supervisor byte
	movs.b		%d1,(%a1)+		# store user byte
	subq.l		&0x1,%d0		# are we through yet?
	bne.w		moreout			# no; so, continue
	rts

#
# int _copyin(user_addr, supervisor_addr, nbytes)
#
	global 		_copyin
_copyin:
	mov.l		4(%sp),%a0		# source
	mov.l		8(%sp),%a1		# destination
	mov.l		12(%sp),%d0		# count
morein:
	movs.b		(%a0)+,%d1		# fetch user byte
	mov.b		%d1,(%a1)+		# write supervisor byte
	subq.l		&0x1,%d0		# are we through yet?
	bne.w		morein			# no; so, continue
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
	global		_060_real_trace
_060_real_trace:
	rte

#
# _060_real_access():
#
# This is the exit point for the 060FPSP when an access error exception
# is encountered. The routine below should point to the operating system
# handler for access error exceptions. The exception stack frame is an
# 8-word access error frame.
#
# The sample routine below simply executes an "rte" instruction which
# is most likely the incorrect thing to do and could put the system
# into an infinite loop.
#
	global		_060_real_access
_060_real_access:
	rte
