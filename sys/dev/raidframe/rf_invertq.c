/*	$OpenBSD: rf_invertq.c,v 1.1 1999/01/11 14:29:26 niklas Exp $	*/
/*	$NetBSD: rf_invertq.c,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
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

/* :  
 * Log: rf_invertq.c,v 
 * Revision 1.5  1996/07/29 16:36:36  jimz
 * include rf_archs.h here, not rf_invertq.h, to avoid VPATH
 * problems in OSF/1 kernel
 *
 * Revision 1.4  1995/11/30  15:57:27  wvcii
 * added copyright info
 *
 */

#ifdef _KERNEL
#define KERNEL
#endif

#include "rf_archs.h"
#include "rf_pqdeg.h"
#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <raidframe/du_data/rf_invertq.h>
#else
#include "rf_invertq.h" /* XXX this is a hack. */
#endif /* !__NetBSD__ && !__OpenBSD__ */
#else /* KERNEL */
#include "rf_invertq.h"
#endif /* KERNEL */
