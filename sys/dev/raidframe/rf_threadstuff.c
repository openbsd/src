/*	$OpenBSD: rf_threadstuff.c,v 1.1 1999/01/11 14:29:53 niklas Exp $	*/
/*	$NetBSD: rf_threadstuff.c,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
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

#ifdef _KERNEL
#define KERNEL
#endif


#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_general.h"
#include "rf_shutdown.h"

static void mutex_destroyer(void *);
static void cond_destroyer(void *);
void thread_wakeup(void *);

/*
 * Shared stuff
 */

static void mutex_destroyer(arg)
  void  *arg;
{
	int rc;

	rc = rf_mutex_destroy(arg);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d auto-destroying mutex\n", rc);
	}
}

static void cond_destroyer(arg)
  void  *arg;
{
	int rc;

	rc = rf_cond_destroy(arg);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d auto-destroying condition\n", rc);
	}
}

int _rf_create_managed_mutex(listp, m, file, line)
  RF_ShutdownList_t  **listp;
  RF_DECLARE_MUTEX(*m)
  char  *file;
  int    line;
{
	int rc, rc1;

	rc = rf_mutex_init(m);
	if (rc)
		return(rc);
	rc = _rf_ShutdownCreate(listp, mutex_destroyer, (void *)m, file, line);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d adding shutdown entry\n", rc);
		rc1 = rf_mutex_destroy(m);
		if (rc1) {
			RF_ERRORMSG1("RAIDFRAME: Error %d destroying mutex\n", rc1);
		}
	}
	return(rc);
}

int _rf_create_managed_cond(listp, c, file, line)
  RF_ShutdownList_t  **listp;
  RF_DECLARE_COND(*c)
  char  *file;
  int    line;
{
	int rc, rc1;

	rc = rf_cond_init(c);
	if (rc)
		return(rc);
	rc = _rf_ShutdownCreate(listp, cond_destroyer, (void *)c, file, line);
	if (rc) {
		RF_ERRORMSG1("RAIDFRAME: Error %d adding shutdown entry\n", rc);
		rc1 = rf_cond_destroy(c);
		if (rc1) {
			RF_ERRORMSG1("RAIDFRAME: Error %d destroying cond\n", rc1);
		}
	}
	return(rc);
}

int _rf_init_managed_threadgroup(listp, g, file, line)
  RF_ShutdownList_t  **listp;
  RF_ThreadGroup_t    *g;
  char                *file;
  int                  line;
{
	int rc;

	rc = _rf_create_managed_mutex(listp, &g->mutex, file, line);
	if (rc)
		return(rc);
	rc = _rf_create_managed_cond(listp, &g->cond, file, line);
	if (rc)
		return(rc);
	g->created = g->running = g->shutdown = 0;
	return(0);
}

int _rf_destroy_threadgroup(g, file, line)
  RF_ThreadGroup_t  *g;
  char              *file;
  int                line;
{
  int rc1, rc2;

#if RF_DEBUG_ATOMIC > 0
  rc1 = _rf_mutex_destroy(&g->mutex, file, line);
  rc2 = _rf_cond_destroy(&g->cond, file, line);
#else /* RF_DEBUG_ATOMIC > 0 */
  rc1 = rf_mutex_destroy(&g->mutex);
  rc2 = rf_cond_destroy(&g->cond);
#endif /* RF_DEBUG_ATOMIC > 0 */
  if (rc1)
    return(rc1);
  return(rc2);
}

int _rf_init_threadgroup(g, file, line)
  RF_ThreadGroup_t  *g;
  char              *file;
  int                line;
{
  int rc;

#if RF_DEBUG_ATOMIC > 0
  rc = _rf_mutex_init(&g->mutex, file, line);
  if (rc)
    return(rc);
  rc = _rf_cond_init(&g->cond, file, line);
  if (rc) {
    _rf_mutex_destroy(&g->mutex, file, line);
    return(rc);
  }
#else /* RF_DEBUG_ATOMIC > 0 */
  rc = rf_mutex_init(&g->mutex);
  if (rc)
    return(rc);
  rc = rf_cond_init(&g->cond);
  if (rc) {
    rf_mutex_destroy(&g->mutex);
    return(rc);
  }
#endif /* RF_DEBUG_ATOMIC > 0 */
  g->created = g->running = g->shutdown = 0;
  return(0);
}

/*
 * User
 */

#if !defined(KERNEL) && !defined(SIMULATE)

#if RF_DEBUG_ATOMIC > 0

static RF_ATEnt_t rf_atent_list;
static RF_ATEnt_t *rf_atent_done_list=NULL;

static pthread_mutex_t rf_atent_mutex;

void rf_atent_init()
{
	int rc;

	rc = pthread_mutex_init(&rf_atent_mutex, pthread_mutexattr_default);
	if (rc) {
		fprintf(stderr, "ERROR: rc=%d creating rf_atent_mutex\n", rc);
		fflush(stderr);
		RF_PANIC();
	}
	rf_atent_list.next = rf_atent_list.prev = &rf_atent_list;
}

#define ATENT_TYPE(_e_) ((((_e_)->type == 0)||((_e_)->type > 2)) ? 0 : (_e_)->type)
#define ATENT_OTYPE(_e_) ((((_e_)->otype == 0)||((_e_)->otype > 2)) ? 0 : (_e_)->otype)

void rf_atent_shutdown()
{
	int rc, num_freed[3], num_not_freed[3];
	RF_ATEnt_t *r, *n;

	num_freed[0] = num_freed[1] = num_freed[2] = 0;
	num_not_freed[0] = num_not_freed[1] = num_not_freed[2] = 0;
	printf("rf_atent_shutdown:\n");
	for(r=rf_atent_list.next;r!=&rf_atent_list;r=r->next) {
		printf("r=%lx type=%d file=%s line=%d\n", r, r->type, r->file, r->line);
		num_not_freed[ATENT_TYPE(r)]++;
	}
	rc = pthread_mutex_destroy(&rf_atent_mutex);
	if (rc) {
		fprintf(stderr, "ERROR: rc=%d destroying rf_atent_mutex\n", rc);
		fflush(stderr);
		RF_PANIC();
	}
	for(r=rf_atent_done_list;r;r=n) {
		n = r->next;
		num_freed[ATENT_OTYPE(r)]++;
		free(r);
	}
	printf("%d mutexes not freed %d conditions not freed %d bogus not freed\n",
		num_not_freed[1], num_not_freed[2], num_not_freed[0]);
	printf("%d mutexes freed %d conditions freed %d bogus freed\n",
		num_freed[1], num_freed[2], num_freed[0]);
	fflush(stdout);
	fflush(stderr);
}

static RF_ATEnt_t *AllocATEnt(file,line)
  char  *file;
  int    line;
{
	RF_ATEnt_t *t;

	t = (RF_ATEnt_t *)malloc(sizeof(RF_ATEnt_t));
	if (t == NULL) {
		RF_PANIC();
	}
	t->file = file;
	t->line = line;
	t->type = 0;
	return(t);
}

static void FreeATEnt(t)
  RF_ATEnt_t  *t;
{
	t->otype = t->type;
	t->type = 0;
	t->next = rf_atent_done_list;
	rf_atent_done_list = t;
}

int _rf_mutex_init(m, file, line)
  RF_ATEnt_t  **m;
  char         *file;
  int           line;
{
	RF_ATEnt_t *a;
	int rc;

	a = AllocATEnt(file,line);
	rc = pthread_mutex_init(&a->m, pthread_mutexattr_default);
	if (rc == 0) {
		pthread_mutex_lock(&rf_atent_mutex);
		a->next = rf_atent_list.next;
		a->prev = &rf_atent_list;
		a->type = RF_ATENT_M;
		a->next->prev = a;
		a->prev->next = a;
		pthread_mutex_unlock(&rf_atent_mutex);
	}
	else {
		fprintf(stderr, "ERROR: rc=%d allocating mutex %s:%d\n",
			rc, file, line);
		fflush(stderr);
		RF_PANIC();
	}
	*m = a;
	return(0);
}

int _rf_mutex_destroy(m, file, line)
  RF_ATEnt_t  **m;
  char         *file;
  int           line;
{
	RF_ATEnt_t *r;
	int rc;

	r = *m;
	rc = pthread_mutex_destroy(&r->m);
	if (rc) {
		fprintf(stderr, "ERROR: rc=%d destroying mutex %s:%d\n",
			rc, file, line);
		fflush(stderr);
		RF_PANIC();
	}
	pthread_mutex_lock(&rf_atent_mutex);
	r->next->prev = r->prev;
	r->prev->next = r->next;
	FreeATEnt(r);
	pthread_mutex_unlock(&rf_atent_mutex);
	*m = NULL;
	return(0);
}

int _rf_cond_init(c, file, line)
  RF_ATEnt_t  **c;
  char         *file;
  int           line;
{
	RF_ATEnt_t *a;
	int rc;

	a = AllocATEnt(file,line);
	rc = pthread_cond_init(&a->c, pthread_condattr_default);
	if (rc == 0) {
		pthread_mutex_lock(&rf_atent_mutex);
		a->next = rf_atent_list.next;
		a->prev = &rf_atent_list;
		a->next->prev = a;
		a->prev->next = a;
		a->type = RF_ATENT_C;
		pthread_mutex_unlock(&rf_atent_mutex);
	}
	else {
		fprintf(stderr, "ERROR: rc=%d allocating cond %s:%d\n",
			rc, file, line);
		fflush(stderr);
		RF_PANIC();
	}
	*c = a;
	return(0);
}

int _rf_cond_destroy(c, file, line)
  RF_ATEnt_t  **c;
  char         *file;
  int           line;
{
	RF_ATEnt_t *r;
	int rc;

	r = *c;
	rc = pthread_cond_destroy(&r->c);
	if (rc) {
		fprintf(stderr, "ERROR: rc=%d destroying cond %s:%d\n",
			rc, file, line);
		fflush(stderr);
		RF_PANIC();
	}
	pthread_mutex_lock(&rf_atent_mutex);
	r->next->prev = r->prev;
	r->prev->next = r->next;
	FreeATEnt(r);
	pthread_mutex_unlock(&rf_atent_mutex);
	*c = NULL;
	return(0);
}

#else /* RF_DEBUG_ATOMIC > 0 */

int rf_mutex_init(m)
  pthread_mutex_t  *m;
{
#ifdef __osf__
	return(pthread_mutex_init(m, pthread_mutexattr_default));
#endif /* __osf__ */
#ifdef AIX
	return(pthread_mutex_init(m, &pthread_mutexattr_default));
#endif /* AIX */
}

int rf_mutex_destroy(m)
  pthread_mutex_t  *m;
{
	return(pthread_mutex_destroy(m));
}

int rf_cond_init(c)
  pthread_cond_t  *c;
{
#ifdef __osf__
	return(pthread_cond_init(c, pthread_condattr_default));
#endif /* __osf__ */
#ifdef AIX
	return(pthread_cond_init(c, &pthread_condattr_default));
#endif /* AIX */
}

int rf_cond_destroy(c)
  pthread_cond_t  *c;
{
	return(pthread_cond_destroy(c));
}

#endif /* RF_DEBUG_ATOMIC > 0 */

#endif /* !KERNEL && !SIMULATE */

/*
 * Kernel
 */
#ifdef KERNEL
int rf_mutex_init(m)
  decl_simple_lock_data(,*m)
{
	simple_lock_init(m);
	return(0);
}

int rf_mutex_destroy(m)
  decl_simple_lock_data(,*m)
{
	return(0);
}

int rf_cond_init(c)
  RF_DECLARE_COND(*c)
{
	*c = 0; /* no reason */
	return(0);
}

int rf_cond_destroy(c)
  RF_DECLARE_COND(*c)
{
	return(0);
}


#endif /* KERNEL */

/*
 * Simulator
 */
#ifdef SIMULATE
int rf_mutex_init(m)
  RF_DECLARE_MUTEX(*m)
{
	return(0);
}

int rf_mutex_destroy(m)
  RF_DECLARE_MUTEX(*m)
{
	return(0);
}

int rf_cond_init(c)
  RF_DECLARE_COND(*c)
{
	return(0);
}

int rf_cond_destroy(c)
  RF_DECLARE_COND(*c)
{
	return(0);
}
#endif /* SIMULATE */
