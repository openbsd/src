/*	$OpenBSD: rf_states.h,v 1.1 1999/01/11 14:29:51 niklas Exp $	*/
/*	$NetBSD: rf_states.h,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II, Robby Findler
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
 * Log: rf_states.h,v 
 * Revision 1.5  1996/05/27 18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.4  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1996/05/06  22:08:28  wvcii
 * added copyright info and change log
 *
 * Revision 1.1  1995/07/06  14:23:39  robby
 * Initial revision
 *
 */

#ifndef _RF__RF_STATES_H_
#define _RF__RF_STATES_H_

#include "rf_types.h"

void rf_ContinueRaidAccess(RF_RaidAccessDesc_t *desc);
void rf_ContinueDagAccess(RF_DagList_t *dagList);
int rf_State_LastState(RF_RaidAccessDesc_t *desc);
int rf_State_IncrAccessCount(RF_RaidAccessDesc_t *desc);
int rf_State_DecrAccessCount(RF_RaidAccessDesc_t *desc);
int rf_State_Quiesce(RF_RaidAccessDesc_t *desc);
int rf_State_Map(RF_RaidAccessDesc_t *desc);
int rf_State_Lock(RF_RaidAccessDesc_t *desc);
int rf_State_CreateDAG(RF_RaidAccessDesc_t *desc);
int rf_State_ExecuteDAG(RF_RaidAccessDesc_t *desc);
int rf_State_ProcessDAG(RF_RaidAccessDesc_t *desc);
int rf_State_Cleanup(RF_RaidAccessDesc_t *desc);

#endif /* !_RF__RF_STATES_H_ */
