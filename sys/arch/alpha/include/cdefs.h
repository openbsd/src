/*	$OpenBSD: cdefs.h,v 1.6 1999/02/04 23:30:18 niklas Exp $	*/
/*	$NetBSD: cdefs.h,v 1.5 1996/10/12 18:08:12 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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

#ifdef __ELF__

#define	__indr_reference(sym,alias)	/* nada, since we do weak refs */

#ifdef __STDC__

#define	__weak_alias(alias,sym)						\
    __asm__(".weak " #alias " ; " #alias " = " #sym)
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning." #sym " ; .ascii \"" msg "\" ; .text")

#else /* !__STDC__ */

#define	__weak_alias(alias,sym)						\
    __asm__(".weak alias ; alias = sym")
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning.sym ; .ascii msg ; .text")

#endif /* !__STDC__ */

#else /* !__ELF__ */

/*
 * We don't support indirect references and don't do anything with warnings.
 */

#ifdef __STDC__
#define	__weak_alias(alias,sym)		__asm__(".weakext " #alias ", " #sym)
#else /* !__STDC__ */
#define	__weak_alias(alias,sym)		__asm__(".weakext alias, sym")
#endif /* !__STDC__ */
#define	__warn_references(sym,msg)	/* nothing */

#endif /* !__ELF__ */

#endif /* !_MACHINE_CDEFS_H_ */
