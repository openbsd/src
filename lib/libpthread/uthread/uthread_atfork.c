/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 * $OpenBSD: uthread_atfork.c,v 1.1 1999/01/17 23:46:26 d Exp $
 */

#include <stdlib.h>
#include <sys/queue.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

struct atfork_entry {
	void (*handler)(void);
	TAILQ_ENTRY(atfork_entry) entries;
};

static TAILQ_HEAD(atfork_list, atfork_entry) atfork_head[3] =
	{ TAILQ_HEAD_INITIALIZER(atfork_head[PTHREAD_ATFORK_PREPARE]),
	  TAILQ_HEAD_INITIALIZER(atfork_head[PTHREAD_ATFORK_PARENT]),
	  TAILQ_HEAD_INITIALIZER(atfork_head[PTHREAD_ATFORK_CHILD]) };

void
_thread_atfork(which)
{
	struct atfork_list *head;
	struct atfork_entry *ae;

	head = &atfork_head[which];

	/* Call the fork handlers in order: */
	for (ae = head->tqh_first; ae != NULL; ae = ae->entries.tqe_next)
		(*ae->handler)();
}

int
pthread_atfork(prepare, parent, child)
	void (*prepare)(void);
	void (*parent)(void);
	void (*child)(void);
{
	int ret = 0;
	struct atfork_entry *prepare_entry = NULL;
	struct atfork_entry *parent_entry = NULL;
	struct atfork_entry *child_entry = NULL;

	if (ret == 0 && prepare != NULL) {
		/* Allocate space for the prepare handler: */
	        if ((prepare_entry = malloc(sizeof *prepare_entry)) != NULL)
			prepare_entry->handler = prepare;
	        else
			ret = -1;
	}

	if (ret == 0 && parent != NULL) {
		/* Allocate space for the parent handler: */
	        if ((parent_entry = malloc(sizeof *parent_entry)) != NULL)
			parent_entry->handler = parent;
	        else
			ret = -1;
	}

	if (ret == 0 && child != NULL) {
		/* Allocate space for the child handler: */
	        if ((child_entry = malloc(sizeof *child_entry)) != NULL)
			child_entry->handler = child;
	        else
			ret = -1;
	}

	if (ret == 0) {
		/* Insert the handlers into the handler lists: */
		if (prepare_entry != NULL)
			TAILQ_INSERT_HEAD(&atfork_head[PTHREAD_ATFORK_PREPARE],
			    prepare_entry, entries);
		if (parent_entry != NULL)
			TAILQ_INSERT_TAIL(&atfork_head[PTHREAD_ATFORK_PARENT],
			    parent_entry, entries);
		if (child_entry != NULL)
			TAILQ_INSERT_TAIL(&atfork_head[PTHREAD_ATFORK_CHILD],
			    child_entry, entries);
	} else {
		/* Release unused resources: */
		if (prepare_entry)
			free(prepare_entry);
		if (child_entry)
			free(child_entry);
		if (parent_entry)
			free(parent_entry);
	}

	return (ret);
}
#endif
