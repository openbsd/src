/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* $arla: xfs_queue.h,v 1.1 2002/11/28 06:24:23 lha Exp $ */

/* Inspirered by the queue macros in BSD.  */

#ifndef NNPFS_QUEUE_H
#define NNPFS_QUEUE_H 1

/* Define a head. */
#define NNPQUEUE_HEAD(name, type)					\
struct name {								\
    struct type *nnpq_first;						\
    struct type **nnpq_last;						\
}

/* Defining a entry. */
#define NNPQUEUE_ENTRY(type)						\
struct {								\
    struct type *nnpq_next, **nnpq_prev;				\
}

/* Is the queue empty. */
#define NNPQUEUE_EMPTY(head)						\
    ((head)->nnpq_first == NULL)

/* Init head. */
#define NNPQUEUE_INIT(head) do {					\
    (head)->nnpq_first = NULL;						\
    (head)->nnpq_last = &(head)->nnpq_first;				\
} while (0)

/* Insert elm after listelm, field is named. */
#define NNPQUEUE_INSERT_HEAD(head, elm, field) do {			\
    if (((elm)->field.nnpq_next = (head)->nnpq_first) != NULL)		\
    	(head)->nnpq_first->field.nnpq_prev =				\
    	    &(elm)->field.nnpq_next;					\
    else								\
    	(head)->nnpq_last = &(elm)->field.nnpq_next;			\
    (head)->nnpq_first = (elm);						\
    (elm)->field.nnpq_prev = &(head)->nnpq_first;			\
} while (0)

/* Remove an entry. */
#define NNPQUEUE_REMOVE(elm,head,field) do {				\
    if (((elm)->field.nnpq_next) != NULL)				\
    	(elm)->field.nnpq_next->field.nnpq_prev = 			\
    	    (elm)->field.nnpq_prev;					\
    else								\
    	(head)->nnpq_last = (elm)->field.nnpq_prev;			\
    *(elm)->field.nnpq_prev = (elm)->field.nnpq_next;			\
} while(0)

/* Iterate over the list. */
#define NNPQUEUE_FOREACH(var,head,field)				\
    for ((var) = ((head)->nnpq_first);					\
	(var);								\
	(var) = ((var)->field.nnpq_next))

#endif /* NNPFS_QUEUE_H */
