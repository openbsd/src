/*	$NetBSD: cdefs.h,v 1.4 1995/12/15 01:17:04 jonathan Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define	_C_LABEL(x)	_STRING(x)

#define	__indr_references(sym,msg)	/* nothing */
#define	__warn_references(sym,msg)	/* nothing */

/* Kernel-only .sections for kernel copyright */
#ifdef _KERNEL

#ifdef __STDC__
#define	__KERNEL_SECTIONSTRING(_sec, _str)				\
	__asm__(".section " #_sec " ; .asciz \"" _str "\" ; .text")
#else
#define	__KERNEL_SECTIONSTRING(_sec, _str)				\
	__asm__(".section _sec ; .asciz _str ; .text")
#endif

#define	__KERNEL_RCSID(_n, _s)		__KERNEL_SECTIONSTRING(.ident, _s)
#define	__KERNEL_COPYRIGHT(_n, _s)	__KERNEL_SECTIONSTRING(.copyright, _s)

#ifdef NO_KERNEL_RCSIDS
#undef __KERNEL_RCSID
#define	__KERNEL_RCSID(_n, _s)		/* nothing */
#endif

#endif /* _KERNEL */

#endif /* !_MACHINE_CDEFS_H_ */
