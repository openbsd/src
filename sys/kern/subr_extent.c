/*	$OpenBSD: subr_extent.c,v 1.1 1996/08/07 17:27:53 deraadt Exp $	*/

/*
 * Copyright (c) 1996, Shawn Hsiao <shawn@alpha.secc.fju.edu.tw>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/extent.h>

LIST_HEAD(, emap) emap_list;
static initialized = 0;

struct emap *
find_emap(name)
	char *name;
{
	struct emap *e;

	if (!initialized) {
		LIST_INIT(&emap_list);
		initialized = 1;
	}

	for (e = emap_list.lh_first; e; e = e->emap_link.le_next) {
		if (!strcmp(e->name, name))
			break;
	}

	if (!e) {
		e = (struct emap *)malloc(sizeof(struct emap),
		    M_DEVBUF, M_WAITOK);
		e->name = (char *)malloc(strlen(name) + 1, M_DEVBUF, M_WAITOK);
		strcpy(e->name, name);
		LIST_INIT(&e->extent_list);
		LIST_INSERT_HEAD(&emap_list, e, emap_link);
	}
	return(e);
}

void
add_extent(e, base, size)
	struct emap *e;
	u_int32_t base, size;
{
	struct extent *this;

	this = (struct extent *)malloc(sizeof(struct extent),
	    M_DEVBUF, M_WAITOK);
	bzero(this, sizeof(struct extent));
	this->base = base;
	this->size = size;
	LIST_INSERT_HEAD(&e->extent_list, this, extent_link);
}

/*
 * return 0 if the region does not conflict with other's
 * return -1 if it does
 */
int
probe_extent(e, base, size)
	struct emap *e;
	u_int32_t base, size;
{
	struct extent *ptr;

	for (ptr = e->extent_list.lh_first; ptr;
	     ptr = ptr->extent_link.le_next) {
		if (ptr->base <= base && ptr->base + ptr->size > base)
			return(-1);
		if (ptr->base < base + size &&
		    ptr->base + ptr->size >= base + size)
			return (-1);
	}
	return(0);
}
