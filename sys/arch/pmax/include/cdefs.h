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

#if defined __GNUC__ && defined __STDC__
#define __warn_references(sym, msg)                  \
  static const char __evoke_link_warning_##sym[]     \
    __attribute__ ((section (".gnu.warning." #sym))) = msg;
#else
#define	__warn_references(sym,msg)	/* nothing */
#endif

#endif /* !_MACHINE_CDEFS_H_ */
