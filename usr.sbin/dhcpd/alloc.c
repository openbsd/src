/*	$OpenBSD: alloc.c,v 1.10 2010/01/01 20:30:24 krw Exp $	*/

/* Memory allocation... */

/*
 * Copyright (c) 1995, 1996, 1998 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include "dhcpd.h"

struct lease_state *free_lease_states;

void *
dmalloc(int size, char *name)
{
	void *foo = calloc(size, sizeof(char));

	if (!foo)
		warning("No memory for %s.", name);
	return (foo);
}

void
dfree(void *ptr, char *name)
{
	if (!ptr) {
		warning("dfree %s: free on null pointer.", name);
		return;
	}
	free(ptr);
}

struct tree_cache *free_tree_caches;

struct tree_cache *
new_tree_cache(char *name)
{
	struct tree_cache *rval;

	if (free_tree_caches) {
		rval = free_tree_caches;
		free_tree_caches = (struct tree_cache *)(rval->value);
	} else {
		rval = dmalloc(sizeof(struct tree_cache), name);
		if (!rval)
			error("unable to allocate tree cache for %s.", name);
	}
	return (rval);
}

void
free_tree_cache(struct tree_cache *ptr)
{
	ptr->value = (unsigned char *)free_tree_caches;
	free_tree_caches = ptr;
}

struct lease_state *
new_lease_state(char *name)
{
	struct lease_state *rval;

	if (free_lease_states) {
		rval = free_lease_states;
		free_lease_states =
			(struct lease_state *)(free_lease_states->next);
	} else
		rval = dmalloc (sizeof (struct lease_state), name);
	return (rval);
}

void
free_lease_state(struct lease_state *ptr, char *name)
{
	if (ptr->prl)
		dfree(ptr->prl, name);
	ptr->next = free_lease_states;
	free_lease_states = ptr;
}
