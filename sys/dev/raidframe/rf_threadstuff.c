/*	$OpenBSD: rf_threadstuff.c,v 1.2 1999/02/16 00:03:32 niklas Exp $	*/
/*	$NetBSD: rf_threadstuff.c,v 1.3 1999/02/05 00:06:18 oster Exp $	*/
/*
 * rf_threadstuff.c
 */
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

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_general.h"
#include "rf_shutdown.h"

static void mutex_destroyer(void *);
static void cond_destroyer(void *);
void    thread_wakeup(void *);

/*
 * Shared stuff
 */

static void 
mutex_destroyer(arg)
	void   *arg;
{
	int     rc;

	rc = rf_mutex_destroy(arg);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d auto-destroying mutex\n", rc);
	}
}

static void 
cond_destroyer(arg)
	void   *arg;
{
	int     rc;

	rc = rf_cond_destroy(arg);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d auto-destroying condition\n", rc);
	}
}

int 
_rf_create_managed_mutex(listp, m, file, line)
	RF_ShutdownList_t **listp;
RF_DECLARE_MUTEX(*m)
	char   *file;
	int     line;
{
	int     rc, rc1;

	rc = rf_mutex_init(m);
	if (rc)
		return (rc);
	rc = _rf_ShutdownCreate(listp, mutex_destroyer, (void *) m, file, line);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d adding shutdown entry\n", rc);
		rc1 = rf_mutex_destroy(m);
		if (rc1) {
			RF_ERRORMSG1("RAIDFRAME: Error %d destroying mutex\n", rc1);
		}
	}
	return (rc);
}

int 
_rf_create_managed_cond(listp, c, file, line)
	RF_ShutdownList_t **listp;
RF_DECLARE_COND(*c)
	char   *file;
	int     line;
{
	int     rc, rc1;

	rc = rf_cond_init(c);
	if (rc)
		return (rc);
	rc = _rf_ShutdownCreate(listp, cond_destroyer, (void *) c, file, line);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d adding shutdown entry\n", rc);
		rc1 = rf_cond_destroy(c);
		if (rc1) {
			RF_ERRORMSG1("RAIDFRAME: Error %d destroying cond\n", rc1);
		}
	}
	return (rc);
}

int 
_rf_init_managed_threadgroup(listp, g, file, line)
	RF_ShutdownList_t **listp;
	RF_ThreadGroup_t *g;
	char   *file;
	int     line;
{
	int     rc;

	rc = _rf_create_managed_mutex(listp, &g->mutex, file, line);
	if (rc)
		return (rc);
	rc = _rf_create_managed_cond(listp, &g->cond, file, line);
	if (rc)
		return (rc);
	g->created = g->running = g->shutdown = 0;
	return (0);
}

int 
_rf_destroy_threadgroup(g, file, line)
	RF_ThreadGroup_t *g;
	char   *file;
	int     line;
{
	int     rc1, rc2;

#if RF_DEBUG_ATOMIC > 0
	rc1 = _rf_mutex_destroy(&g->mutex, file, line);
	rc2 = _rf_cond_destroy(&g->cond, file, line);
#else				/* RF_DEBUG_ATOMIC > 0 */
	rc1 = rf_mutex_destroy(&g->mutex);
	rc2 = rf_cond_destroy(&g->cond);
#endif				/* RF_DEBUG_ATOMIC > 0 */
	if (rc1)
		return (rc1);
	return (rc2);
}

int 
_rf_init_threadgroup(g, file, line)
	RF_ThreadGroup_t *g;
	char   *file;
	int     line;
{
	int     rc;

#if RF_DEBUG_ATOMIC > 0
	rc = _rf_mutex_init(&g->mutex, file, line);
	if (rc)
		return (rc);
	rc = _rf_cond_init(&g->cond, file, line);
	if (rc) {
		_rf_mutex_destroy(&g->mutex, file, line);
		return (rc);
	}
#else				/* RF_DEBUG_ATOMIC > 0 */
	rc = rf_mutex_init(&g->mutex);
	if (rc)
		return (rc);
	rc = rf_cond_init(&g->cond);
	if (rc) {
		rf_mutex_destroy(&g->mutex);
		return (rc);
	}
#endif				/* RF_DEBUG_ATOMIC > 0 */
	g->created = g->running = g->shutdown = 0;
	return (0);
}


/*
 * Kernel
 */
int 
rf_mutex_init(m)
decl_simple_lock_data(, *m)
{
	simple_lock_init(m);
	return (0);
}

int 
rf_mutex_destroy(m)
decl_simple_lock_data(, *m)
{
	return (0);
}

int 
rf_cond_init(c)
RF_DECLARE_COND(*c)
{
	*c = 0;			/* no reason */
	return (0);
}

int 
rf_cond_destroy(c)
RF_DECLARE_COND(*c)
{
	return (0);
}
