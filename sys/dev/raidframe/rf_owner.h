/*	$OpenBSD: rf_owner.h,v 1.1 1999/01/11 14:29:33 niklas Exp $	*/
/*	$NetBSD: rf_owner.h,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
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

/* :  
 * Log: rf_owner.h,v 
 * Revision 1.8  1996/08/20 14:36:51  jimz
 * add bufLen to RF_EventCreate_t to be able to include buffer length
 * when freeing buffer
 *
 * Revision 1.7  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.6  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.5  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.4  1995/12/01  19:44:30  root
 * added copyright info
 *
 */

#ifndef _RF__RF_OWNER_H_
#define _RF__RF_OWNER_H_

#include "rf_types.h"

struct RF_OwnerInfo_s {
  RF_RaidAccessDesc_t *desc;
  int                  owner;
  double               last_start;
  int                  done;
  int                  notFirst;
};

struct RF_EventCreate_s {
  RF_Raid_t       *raidPtr;
  RF_Script_t     *script;
  RF_OwnerInfo_t  *ownerInfo;
  char            *bufPtr;
  int              bufLen;
};

#endif /* !_RF__RF_OWNER_H_ */
