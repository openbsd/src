/* $NetBSD: katelib.h,v 1.5 1996/03/28 21:35:51 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * katelib.h
 *
 * Prototypes for machine specific functions. Most of these
 * could be inlined.
 *
 * This should not really be a separate header file. Eventually I will merge
 * this into other header files once I have decided where the declarations
 * should go. 
 *
 * Created      : 18/09/94
 *
 * Based on kate/katelib/prototypes.h
 */

#include <sys/types.h>

#ifdef _KERNEL

/* Assembly modules */

/* In setcpsr.S */

u_int SetCPSR __P((u_int, u_int));
u_int GetCPSR __P((void));

/* In coproc15.S */

void tlbflush __P((void));
void tlbpurge __P((u_int));
void idcflush __P((void));
void cpu_control __P((u_int));
void cpu_domains __P((u_int));
void setttb __P((u_int));

u_int cpu_id __P((void));
u_int cpu_faultstatus __P((void));
u_int cpu_faultaddress __P((void));

/* In setstack.S */

void set_stackptr __P((u_int, u_int));
u_int get_stackptr __P((u_int));

/* In blockio.S */

void insw __P((u_int /*io*/, u_int /*dest*/, u_int /*size*/));
void outsw __P((u_int /*io*/, u_int /*src*/, u_int /*size*/));
void insw16 __P((u_int /*io*/, u_int /*dest*/, u_int /*size*/));
void outsw16 __P((u_int /*io*/, u_int /*src*/, u_int /*size*/));

/* Macros for reading and writing words, shorts, bytes */

#define WriteWord(a, b) \
*((volatile unsigned int *)(a)) = (b)

#define ReadWord(a) \
(*((volatile unsigned int *)(a)))

#define WriteShort(a, b) \
*((volatile unsigned int *)(a)) = ((b) | ((b) << 16))

#define ReadShort(a) \
((*((volatile unsigned int *)(a))) & 0xffff)

#define WriteByte(a, b) \
*((volatile unsigned char *)(a)) = (b)

#define ReadByte(a) \
(*((volatile unsigned char *)(a)))

/* Define in/out macros */

#define inb(port)		ReadByte((port))
#define outb(port, byte)	WriteByte((port), (byte))
#define inw(port)		ReadShort((port))
#define outw(port, word)	WriteShort((port), (word))
#define inl(port)		ReadWord((port))
#define outl(port, lword)	WriteWord((port), (lword))

/* Prototypes that are wandering the streets */

#ifdef _ARM32_FRAME_H
void postmortem __P((trapframe_t */*frame*/));
#endif
u_int traceback __P(());
u_int simpletraceback __P(());
u_int irqtraceback __P((u_int, u_int));
int shell __P(());
void kstack_stuff __P((struct proc */*p*/));
void boot0 __P(());
void bootsync __P((void));
#endif

/* End of katelib.h */
