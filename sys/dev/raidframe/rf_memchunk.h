/*	$OpenBSD: rf_memchunk.h,v 1.1 1999/01/11 14:29:30 niklas Exp $	*/
/*	$NetBSD: rf_memchunk.h,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
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

/* header file for rf_memchunk.c.  See comments there */

/* :  
 * Log: rf_memchunk.h,v 
 * Revision 1.8  1996/06/10 11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.7  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.6  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.5  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.4  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1995/12/01  19:25:56  root
 * added copyright info
 *
 */

#ifndef _RF__RF_MEMCHUNK_H_
#define _RF__RF_MEMCHUNK_H_

#include "rf_types.h"

struct RF_ChunkDesc_s {
  int              size;
  int              reuse_count;
  char            *buf;
  RF_ChunkDesc_t  *next;
};

int rf_ConfigureMemChunk(RF_ShutdownList_t **listp);
RF_ChunkDesc_t *rf_GetMemChunk(int size);
void rf_ReleaseMemChunk(RF_ChunkDesc_t *chunk);

#endif /* !_RF__RF_MEMCHUNK_H_ */
