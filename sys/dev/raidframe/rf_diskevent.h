/*	$OpenBSD: rf_diskevent.h,v 1.1 1999/01/11 14:29:16 niklas Exp $	*/
/*	$NetBSD: rf_diskevent.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/* 
 * rf_diskevent.h
 * Adapted from original code by David Kotz (1994)
 *
 * The disk-device module is event driven.  This module keeps the event 
 * request mechanism, which is based on proteus SimRequests, 
 * abstracted away from the bulk of the disk device code. 
 *
 * Functions
 *  	DDEventInit
 *  	DDEventRequest
 *  	DDEventPrint
 *  	DDEventCancel
 */

/* :  
 * Log: rf_diskevent.h,v 
 * Revision 1.10  1996/06/10 11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.9  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.8  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.7  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.6  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.5  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.4  1995/12/01  15:57:16  root
 * added copyright info
 *
 */

#ifndef _RF__RF_DISKEVENT_H_
#define _RF__RF_DISKEVENT_H_

#include "rf_types.h"
#include "rf_heap.h"
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include "time.h"
#endif

#define RF_DD_NOTHING_THERE (-1)
#define RF_DD_DAGEVENT_ROW  (-3)
#define RF_DD_DAGEVENT_COL RF_DD_DAGEVENT_ROW

extern RF_TICS_t rf_cur_time;

/*
 * list of disk-device request types, 
 * initialized in diskdevice.c, 
 * used in diskevent.c
 */
typedef void (*RF_DDhandler)(int disk, RF_TICS_t eventTime);
struct RF_dd_handlers_s {
  RF_DDhandler  handler;  /* function implementing this event type */
  char          name[20]; /* name of that event type */
};
extern struct RF_dd_handlers_s rf_DDhandlers[];

int        rf_DDEventInit(RF_ShutdownList_t **listp);
void       rf_DDEventRequest(RF_TICS_t eventTime, int (*CompleteFunc)(),
	void *argument, RF_Owner_t owner, RF_RowCol_t row, RF_RowCol_t col,
	RF_Raid_t *raidPtr, void *diskid);
void       rf_DAGEventRequest(RF_TICS_t eventTime, RF_Owner_t owner,
	RF_RowCol_t row, RF_RowCol_t col, RF_RaidAccessDesc_t *desc,
	RF_Raid_t *raidPtr);
void       rf_DDPrintRequests(void);
int        rf_ProcessEvent(void);
RF_Owner_t rf_GetCurrentOwner(void);
void       rf_SetCurrentOwner(RF_Owner_t owner);
RF_TICS_t  rf_CurTime(void);

#endif /* !_RF__RF_DISKEVENT_H_ */
