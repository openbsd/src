/*	$OpenBSD: param.h,v 1.15 2011/04/07 15:45:16 miod Exp $	*/
/*	$NetBSD: param.h,v 1.9 2002/03/24 03:37:23 thorpej Exp $	*/

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name "RiscBSD" nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_ARM_PARAM_H_
#define	_ARM_PARAM_H_

#define MACHINE_ARCH	"arm"
#define _MACHINE_ARCH	arm

/*
 * Machine dependent constants for ARM6+ processors
 */
/* These are defined in the Port File before it includes
 * this file. */

#define	PAGE_SHIFT	12		/* LOG2(NBPG) */
#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	PAGE_SIZE	(1 << PAGE_SHIFT)	/* bytes/page */
#define	NBPG		(1 << PAGE_SHIFT)	/* bytes/page */
#define	PAGE_MASK	(PAGE_SIZE - 1)
#define PGOFSET		(PAGE_SIZE - 1)
#define	NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define UPAGES          2               /* pages of u-area */
#define USPACE          (UPAGES * PAGE_SIZE) /* total size of u-area */
#define	USPACE_ALIGN	(0)		/* u-area alignment 0-none */

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	PAGE_SIZE	/* default message buffer size */
#endif

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((64 * 1024 * 1024) >> PAGE_SHIFT)

/* Constants used to divide the USPACE area */

/*
 * The USPACE area contains :
 * 1. the user structure for the process
 * 2. the fp context for FP emulation
 * 3. the kernel (svc) stack
 * 4. the undefined instruction stack
 *
 * The layout of the area looks like this
 *
 * | user area | FP context | undefined stack | kernel stack |
 *
 * The size of the user area is known.
 * The size of the FP context is variable depending of the FP emulator
 * in use and whether there is hardware FP support. However we can put
 * an upper limit on it.
 * The undefined stack needs to be at least 512 bytes. This is a requirement
 * if the FP emulators
 * The kernel stack should be at least 4K is size.
 *
 * The stack top addresses are used to set the stack pointers. The stack bottom
 * addresses at the addresses monitored by the diagnostic code for stack overflows
 *
 */

#define FPCONTEXTSIZE			(0x100)
#define USPACE_SVC_STACK_TOP		(USPACE)
#define USPACE_SVC_STACK_BOTTOM		(USPACE_SVC_STACK_TOP - 0x1000)
#define	USPACE_UNDEF_STACK_TOP		(USPACE_SVC_STACK_BOTTOM - 0x10)
#define USPACE_UNDEF_STACK_BOTTOM	(sizeof(struct user) + FPCONTEXTSIZE + 10)

#ifdef _KERNEL
#ifndef _LOCORE
void	delay (unsigned);
#define DELAY(x)	delay(x)
#endif
#endif

/*
 * Machine dependent constants for all ARM processors
 */

/*
 * For KERNEL code:
 *	MACHINE must be defined by the individual port.  This is so that
 *	uname returns the correct thing, etc.
 *
 *	MACHINE_ARCH may be defined by individual ports as a temporary
 *	measure while we're finishing the conversion to ELF.
 *
 * For non-KERNEL code:
 *	If ELF, MACHINE and MACHINE_ARCH are forced to "arm/armeb".
 */


#define	MID_MACHINE	MID_ARM6

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNBYTES		(sizeof(int) - 1)
#define ALIGN(p)		(((u_long)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)
/* ARM-specific macro to align a stack pointer (downwards). */
#define STACKALIGNBYTES		(8 - 1)
#define STACKALIGN(p)		((u_long)(p) &~ STACKALIGNBYTES)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define	BLKDEV_IOSIZE	2048

#ifndef MAXPHYS
#define	MAXPHYS		(64 * 1024)		/* max I/O transfer size */
#endif

/* pages ("clicks") to disk blocks */
#define	ctod(x)	((x) << (PAGE_SHIFT - DEV_BSHIFT))
#define	dtoc(x)	((x) >> (PAGE_SHIFT - DEV_BSHIFT))

#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	((bytes) >> DEV_BSHIFT)
#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((db) << DEV_BSHIFT)

/*
 * Constants related to network buffer management.
 */
#define	NMBCLUSTERS	4096		/* map size, max cluster allocation */

#define ovbcopy bcopy

#if defined(_KERNEL) && !defined(_LOCORE)
#include <sys/param.h>
#include <machine/cpu.h>
#endif

#endif	/* _ARM_PARAM_H_ */
