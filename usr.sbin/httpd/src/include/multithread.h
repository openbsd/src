/* $OpenBSD: multithread.h,v 1.6 2005/03/28 23:26:51 niallo Exp $ */

#ifndef APACHE_MULTITHREAD_H
#define APACHE_MULTITHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define MULTI_OK (0)
#define MULTI_TIMEOUT (1)
#define MULTI_ERR (2)

typedef void mutex;
typedef void semaphore;
typedef void thread;
typedef void event;

/*
 * Ambarish: Need to do the right stuff on multi-threaded unix
 * I believe this is terribly ugly
 */
#define APACHE_TLS
/* Only define the ones actually used, for now */
extern void *ap_dummy_mutex;

#define ap_create_mutex(name)	((mutex *)ap_dummy_mutex)
#define ap_acquire_mutex(mutex_id)	((int)MULTI_OK)
#define ap_release_mutex(mutex_id)	((int)MULTI_OK)
#define ap_destroy_mutex(mutex_id)

#ifdef __cplusplus
}
#endif

#endif /* !APACHE_MULTITHREAD_H */
