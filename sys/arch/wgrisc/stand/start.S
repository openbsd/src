/*	$NetBSD: start.S,v 1.1 1995/01/18 06:19:01 mellon Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 */

/*
 * start.S -
 *
 *     Contains code that is the first executed at boot time.
 */

#include <machine/regdef.h>
#include <machine/cpu.h>
#include <machine/asm.h>

/*
 * Frame required for the debugger (if we have any)
 */
#define START_FRAME	((4 * 4) + 4 + 4)

	.globl	__start
__start:
	.set	noreorder
#ifdef __GP_SUPPORT__
	la      gp, _C_LABEL (_gp)
#endif
	la	sp, __start - START_FRAME		# Stack below program
	sw	zero, START_FRAME - 4(sp)	# Zero out old ra for debugger
	sw	zero, START_FRAME - 8(sp)	# Zero out old fp for debugger
	move	s0, a0				# save argc
	move	s1, a1				# save argv

	la	a0, _C_LABEL (edata)			# clear BSS
	la	a1, _C_LABEL (end)
	jal	_C_LABEL(bzero)			# bzero(edata, end - edata)
	subu	a1, a1, a0

	move	a0, s0				# restore argc
	jal	_C_LABEL(main)			# main(argc, argv)
	move	a1, s1				# restore argv

	j	_C_LABEL(Bios_Restart)		# restart...
	nop

/* dummy routine for gcc2 */
	.globl	_C_LABEL(__main)
_C_LABEL(__main):
	j	ra
	nop

#define Bios_Call(Name,Offset)	\
LEAF(Name);			\
	lw	v0,0x80001020;	\
	lw	v0,Offset(v0);	\
	jr	v0	;	\
	nop		;	\
	END(Name)

Bios_Call(Bios_Load,			0x00)
Bios_Call(Bios_Invoke,			0x04)
Bios_Call(Bios_Execute,			0x08)
Bios_Call(Bios_Halt,			0x0c)
Bios_Call(Bios_PowerDown,		0x10)
Bios_Call(Bios_Restart,			0x14)
Bios_Call(Bios_Reboot,			0x18)
Bios_Call(Bios_EnterInteractiveMode,	0x1c)
Bios_Call(Bios_Unused1,			0x20)
Bios_Call(Bios_GetPeer,			0x24)
Bios_Call(Bios_GetChild,		0x28)
Bios_Call(Bios_GetParent,		0x2c)
Bios_Call(Bios_GetConfigurationData,	0x30)
Bios_Call(Bios_AddChild,		0x34)
Bios_Call(Bios_DeleteComponent,		0x38)
Bios_Call(Bios_GetComponent,		0x3c)
Bios_Call(Bios_SaveConfiguration,	0x40)
Bios_Call(Bios_GetSystemId,		0x44)
Bios_Call(Bios_GetMemoryDescriptor,	0x48)
Bios_Call(Bios_Unused2,			0x4c)
Bios_Call(Bios_GetTime,			0x50)
Bios_Call(Bios_GetRelativeTime,		0x54)
Bios_Call(Bios_GetDirectoryEntry,	0x58)
Bios_Call(Bios_Open,			0x5c)
Bios_Call(Bios_Close,			0x60)
Bios_Call(Bios_Read,			0x64)
Bios_Call(Bios_GetReadStatus,		0x68)
Bios_Call(Bios_Write,			0x6c)
Bios_Call(Bios_Seek,			0x70)
Bios_Call(Bios_Mount,			0x74)
Bios_Call(Bios_GetEnvironmentVariable,	0x78)
Bios_Call(Bios_SetEnvironmentVariable,	0x7c)
Bios_Call(Bios_GetFileInformation,	0x80)
Bios_Call(Bios_SetFileInformation,	0x84)
Bios_Call(Bios_FlushAllCaches,		0x88)
Bios_Call(Bios_TestUnicodeCharacter,	0x8c)
Bios_Call(Bios_GetDisplayStatus,	0x90)
