/*	$NetBSD: trap.h,v 1.1 1995/02/13 23:07:58 cgd Exp $	*/

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

#define	T_ASTFLT	0x00
#define	T_UNAFLT	0x01
#define	T_ARITHFLT	0x02

#define	T_IFLT		0x10
#define	T_BPT		(T_IFLT|0x00)
#define	T_BUGCHK	(T_IFLT|0x01)
#define	T_GENTRAP	(T_IFLT|0x02)
#define	T_FPDISABLED	(T_IFLT|0x03)
#define	T_OPDEC		(T_IFLT|0x04)

#define	T_MMFLT		0x20
#define	T_INVALTRANS	(T_MMFLT|0x00)
#define	T_ACCESS	(T_MMFLT|0x01)
#define	T_FOR		(T_MMFLT|0x02)
#define	T_FOE		(T_MMFLT|0x03)
#define	T_FOW		(T_MMFLT|0x04)

#define	T_USER		0x80		/* user-mode flag or'ed with type */
