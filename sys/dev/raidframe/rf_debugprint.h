/*	$OpenBSD: rf_debugprint.h,v 1.3 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_debugprint.h,v 1.3 1999/02/05 00:06:08 oster Exp $	*/

/*
 * rf_debugprint.h
 */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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

#ifndef	_RF__RF_DEBUGPRINT_H_
#define	_RF__RF_DEBUGPRINT_H_

int  rf_ConfigureDebugPrint(RF_ShutdownList_t **);
void rf_clear_debug_print_buffer(void);
void rf_debug_printf(char *, void *, void *, void *, void *, void *, void *,
	void *, void *);
void rf_print_debug_buffer(void);
void rf_spill_debug_buffer(char *);

#endif	/* ! _RF__RF_DEBUGPRINT_H_ */
