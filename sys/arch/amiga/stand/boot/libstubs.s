/*
 * $OpenBSD: libstubs.s,v 1.1 1997/01/16 09:26:36 niklas Exp $
 * $NetBSD: libstubs.s,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
 *
 *
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Exec.library functions.
 */
	.comm _SysBase,4

	.globl _OpenLibrary
_OpenLibrary:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	movl	sp@(12),d0
	jsr	a6@(-0x228)
	movl	sp@+,a6
	rts
#ifdef notyet
	.globl _CloseLibrary
_CloseLibrary:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x19e)
	movl	sp@+,a6
	rts
#endif
	.globl _CreateIORequest
_CreateIORequest:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a0
	movl	sp@(12),d0
	jsr	a6@(-0x28e)
	movl	sp@+,a6
	rts

	.globl _CreateMsgPort
_CreateMsgPort:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	jsr	a6@(-0x29a)
	movl	sp@+,a6
	rts
	
#ifdef notyet
	.globl _DeleteMsgPort
_DeleteMsgPort:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a0
	jsr	a6@(-0x2a0)
	movl	sp@+,a6
	rts
	
	.globl _DeleteIORequest
_DeleteIORequest:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a0
	jsr	a6@(-0x294)
	movl	sp@+,a6
	rts
#endif
	
	.globl	_OpenDevice
_OpenDevice:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a0
	movl	sp@(12),d0
	movl	sp@(16),a1
	movl	sp@(20),d1
	jsr	a6@(-0x1bc)
	movl	sp@+,a6
	rts

	.globl	_DoIO
_DoIO:	
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x1c8)
	movl	sp@+,a6
	rts
#ifdef nomore
	.globl	_CheckIO
_CheckIO:	
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x1d4)
	movl	sp@+,a6
	rts
#endif
	.globl	_WaitIO
_WaitIO:	
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x1da)
	movl	sp@+,a6
	rts

	.globl	_SendIO
_SendIO:	
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x1ce)
	movl	sp@+,a6
	rts

	.globl	_AbortIO
_AbortIO:	
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x1e0)
	movl	sp@+,a6
	rts

	.globl	_WaitPort
_WaitPort:	
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a0
	jsr	a6@(-0x180)
	movl	sp@+,a6
	rts

#ifndef DOINLINES
	.globl _CacheClearU
_CacheClearU:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	jsr	a6@(-0x27c)
	movl	sp@+,a6
	rts
#endif
	.globl _CachePreDMA
_CachePreDMA:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a0
	movl	sp@(12),a1
	movl	sp@(16),d0
	jsr	a6@(-0x2fa)
	movl	sp@+,a6
	rts

	.globl _FindResident
_FindResident:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x60)
	movl	sp@+,a6
	rts

	.globl _OpenResource
_OpenResource:
	movl	a6,sp@-
	movl	pc@(_SysBase:w),a6
	movl	sp@(8),a1
	jsr	a6@(-0x1f2)
	movl	sp@+,a6
	rts
#ifdef notyet
	.globl _Forbid
_Forbid:
	movl	a6,sp@-
	movl	pc@(_SysBase:W),a6
	jsr	a6@(-0x84)
	movl	sp@+,a6
	rts

	.globl _Permit
_Permit:
	movl	a6,sp@-
	movl	pc@(_SysBase:W),a6
	jsr	a6@(-0x8a)
	movl	sp@+,a6
	rts
#endif

/*
 * Intuition.library functions.
 */

	.comm _IntuitionBase,4

	.globl _OpenScreenTagList
_OpenScreenTagList:
	movl	a6,sp@-
	movl	pc@(_IntuitionBase:w),a6
	movl	sp@(8),a0
	movl	sp@(12),a1
	jsr	a6@(-0x264)
	movl	sp@+,a6
	rts

	.globl _OpenWindowTagList
_OpenWindowTagList:
	movl	a6,sp@-
	movl	pc@(_IntuitionBase:w),a6
	movl	sp@(8),a0
	movl	sp@(12),a1
	jsr	a6@(-0x25e)
	movl	sp@+,a6
	rts
#ifdef nomore
	.globl _mytime
_mytime:
	movl	a6,sp@-
	movl	pc@(_IntuitionBase:w),a6
	subql	#8,sp
	movl	sp,a0
	lea	sp@(4),a1
	jsr	a6@(-0x54)
	movl	sp@+,d0
	addql	#4,sp
	movl	sp@+,a6
	rts
#endif
	.comm _ExpansionBase,4
	.globl _FindConfigDev
_FindConfigDev:
	movl	a6,sp@-
	movl	_ExpansionBase,a6
	movl	sp@(8),a0
	movl	sp@(12),d0
	movl	sp@(16),d1
	jsr	a6@(-0x48)
	movl	sp@+,a6
	rts
