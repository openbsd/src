/*	$OpenBSD: rf_debugprint.h,v 1.1 1999/01/11 14:29:13 niklas Exp $	*/
/*	$NetBSD: rf_debugprint.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
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
/*
 * :  
 * Log: rf_debugprint.h,v 
 * Revision 1.4  1996/06/10 11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.3  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:55:43  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_DEBUGPRINT_H_
#define _RF__RF_DEBUGPRINT_H_

int rf_ConfigureDebugPrint(RF_ShutdownList_t **listp);
void rf_clear_debug_print_buffer(void);
void rf_debug_printf(char *s, void *a1, void *a2, void *a3, void *a4,
	void *a5, void *a6, void *a7, void *a8);
void rf_print_debug_buffer(void);
void rf_spill_debug_buffer(char *fname);

#endif /* !_RF__RF_DEBUGPRINT_H_ */
