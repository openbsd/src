/*	$OpenBSD: rf_rst.h,v 1.1 1999/01/11 14:29:49 niklas Exp $	*/
/*	$NetBSD: rf_rst.h,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

/* rf_rst.h - defines raidSim trace entry */

/* :  
 * Log: rf_rst.h,v 
 * Revision 1.7  1996/07/18 22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.6  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.5  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1995/12/06  15:03:15  root
 * added copyright info
 *
 */

#ifndef _RF__RF_RST_H_
#define _RF__RF_RST_H_

#include "rf_types.h"

typedef struct RF_ScriptTraceEntry_s {
    RF_int32   blkno;
    RF_int32   size;
    double     delay;
    RF_int16   pid;
    RF_int8    op;
    RF_int8    async_flag;
} RF_ScriptTraceEntry_t;

typedef struct RF_ScriptTraceEntryList_s  RF_ScriptTraceEntryList_t;
struct RF_ScriptTraceEntryList_s {
   RF_ScriptTraceEntry_t       entry;
   RF_ScriptTraceEntryList_t  *next;
};

#endif /* !_RF__RF_RST_H_ */
