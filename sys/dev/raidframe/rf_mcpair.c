/*	$OpenBSD: rf_mcpair.c,v 1.1 1999/01/11 14:29:29 niklas Exp $	*/
/*	$NetBSD: rf_mcpair.c,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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

/* rf_mcpair.c
 * an mcpair is a structure containing a mutex and a condition variable.
 * it's used to block the current thread until some event occurs.
 */

/* :  
 * Log: rf_mcpair.c,v 
 * Revision 1.16  1996/06/19 22:23:01  jimz
 * parity verification is now a layout-configurable thing
 * not all layouts currently support it (correctly, anyway)
 *
 * Revision 1.15  1996/06/17  03:18:04  jimz
 * include shutdown.h for macroized ShutdownCreate
 *
 * Revision 1.14  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.13  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.12  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.11  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.10  1996/05/20  16:15:22  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.9  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.8  1996/05/16  16:04:42  jimz
 * convert to return-val on FREELIST init
 *
 * Revision 1.7  1996/05/16  14:47:21  jimz
 * rewrote to use RF_FREELIST
 *
 * Revision 1.6  1995/12/01  19:25:43  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_mcpair.h"
#include "rf_debugMem.h"
#include "rf_freelist.h"
#include "rf_shutdown.h"

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
#include <sys/proc.h>
#endif

static RF_FreeList_t *rf_mcpair_freelist;

#define RF_MAX_FREE_MCPAIR 128
#define RF_MCPAIR_INC       16
#define RF_MCPAIR_INITIAL   24

static int init_mcpair(RF_MCPair_t *);
static void clean_mcpair(RF_MCPair_t *);
static void rf_ShutdownMCPair(void *);



static int init_mcpair(t)
  RF_MCPair_t  *t;
{
	int rc;

	rc = rf_mutex_init(&t->mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
		return(rc);
	}
	rc = rf_cond_init(&t->cond);
	if (rc) {
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
		rf_mutex_destroy(&t->mutex);
		return(rc);
	}
	return(0);
}

static void clean_mcpair(t)
  RF_MCPair_t  *t;
{
	rf_mutex_destroy(&t->mutex);
	rf_cond_destroy(&t->cond);
}

static void rf_ShutdownMCPair(ignored)
  void  *ignored;
{
	RF_FREELIST_DESTROY_CLEAN(rf_mcpair_freelist,next,(RF_MCPair_t *),clean_mcpair);
}

int rf_ConfigureMCPair(listp)
  RF_ShutdownList_t  **listp;
{
	int rc;

	RF_FREELIST_CREATE(rf_mcpair_freelist, RF_MAX_FREE_MCPAIR,
		RF_MCPAIR_INC, sizeof(RF_MCPair_t));
	rc = rf_ShutdownCreate(listp, rf_ShutdownMCPair, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
			__FILE__, __LINE__, rc);
		rf_ShutdownMCPair(NULL);
		return(rc);
	}
	RF_FREELIST_PRIME_INIT(rf_mcpair_freelist, RF_MCPAIR_INITIAL,next,
		(RF_MCPair_t *),init_mcpair);
	return(0);
}

RF_MCPair_t *rf_AllocMCPair()
{
	RF_MCPair_t *t;

	RF_FREELIST_GET_INIT(rf_mcpair_freelist,t,next,(RF_MCPair_t *),init_mcpair);
	if (t) {
		t->flag = 0;
		t->next = NULL;
	}
	return(t);
}

void rf_FreeMCPair(t)
  RF_MCPair_t   *t;
{
	RF_FREELIST_FREE_CLEAN(rf_mcpair_freelist,t,next,clean_mcpair);
}

/* the callback function used to wake you up when you use an mcpair to wait for something */
void rf_MCPairWakeupFunc(mcpair)
  RF_MCPair_t  *mcpair;
{
	RF_LOCK_MUTEX(mcpair->mutex);
	mcpair->flag = 1;
#if 0
printf("MCPairWakeupFunc called!\n");
#endif
#ifdef KERNEL
	wakeup(&(mcpair->flag)); /* XXX Does this do anything useful!! GO */
	/*
	 * XXX Looks like the following is needed to truly get the 
	 * functionality they were looking for here... This could be a
	 * side-effect of my using a tsleep in the Net- and OpenBSD port
	 * though... XXX
	 */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
	wakeup(&(mcpair->cond)); /* XXX XXX XXX GO */
#endif
#else /* KERNEL */
	RF_SIGNAL_COND(mcpair->cond);
#endif /* KERNEL */
	RF_UNLOCK_MUTEX(mcpair->mutex);
}
