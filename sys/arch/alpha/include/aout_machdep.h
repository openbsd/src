/*	$OpenBSD: aout_machdep.h,v 1.3 2011/03/23 16:54:33 pirofti Exp $	*/
/*	$NetBSD: aout_machdep.h,v 1.1 1995/02/13 23:07:37 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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

#ifndef _MACHINE_AOUT_MACHDEP_H_
#define	_MACHINE_AOUT_MACHDEP_H_

#define cpu_exec_aout_makecmds(p, epp)	ENOEXEC

/* Size of a page in an object file. */
#define	__LDPGSZ	8192

#endif /* !_MACHINE_AOUT_MACHDEP_H_ */
