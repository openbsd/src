/*	$OpenBSD: rf_callback.c,v 1.3 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_callback.c,v 1.3 1999/02/05 00:06:06 oster Exp $	*/

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

/*****************************************************************************
 *
 * rf_callback.c -- Code to manipulate callback descriptor.
 *
 *****************************************************************************/


#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_callback.h"
#include "rf_debugMem.h"
#include "rf_freelist.h"
#include "rf_shutdown.h"

static RF_FreeList_t *rf_callback_freelist;

void rf_ShutdownCallback(void *);

#define	RF_MAX_FREE_CALLBACK	64
#define	RF_CALLBACK_INC		 4
#define	RF_CALLBACK_INITIAL	 4

void
rf_ShutdownCallback(void *ignored)
{
	RF_FREELIST_DESTROY(rf_callback_freelist, next, (RF_CallbackDesc_t *));
}

int
rf_ConfigureCallback(RF_ShutdownList_t **listp)
{
	int rc;

	RF_FREELIST_CREATE(rf_callback_freelist, RF_MAX_FREE_CALLBACK,
	    RF_CALLBACK_INC, sizeof(RF_CallbackDesc_t));
	if (rf_callback_freelist == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_ShutdownCallback, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d"
		    " rc=%d.\n", __FILE__, __LINE__, rc);
		rf_ShutdownCallback(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME(rf_callback_freelist, RF_CALLBACK_INITIAL, next,
	    (RF_CallbackDesc_t *));
	return (0);
}

RF_CallbackDesc_t *
rf_AllocCallbackDesc(void)
{
	RF_CallbackDesc_t *p;

	RF_FREELIST_GET(rf_callback_freelist, p, next, (RF_CallbackDesc_t *));
	return (p);
}

void
rf_FreeCallbackDesc(RF_CallbackDesc_t *p)
{
	RF_FREELIST_FREE(rf_callback_freelist, p, next);
}
