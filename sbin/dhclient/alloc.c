/*	$OpenBSD: alloc.c,v 1.6 2004/04/14 20:15:47 henning Exp $	*/

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

struct dhcp_packet *dhcp_free_list;
struct packet *packet_free_list;

void *
dmalloc(int size, char *name)
{
	void *foo = calloc(size, sizeof(char));

	if (!foo)
		warn("No memory for %s.", name);
	return (foo);
}

void
dfree(void *ptr, char *name)
{
	if (!ptr) {
		warn("dfree %s: free on null pointer.", name);
		return;
	}
	free(ptr);
}

struct string_list *
new_string_list(size_t size, char * name)
{
	struct string_list *rval;

	rval = dmalloc(sizeof(struct string_list) + size, name);
	if (rval != NULL)
		rval->string = ((char *)rval) + sizeof(struct string_list);
	return (rval);
}

struct hash_table *
new_hash_table(int count, char *name)
{
	struct hash_table *rval;

	rval = dmalloc(sizeof(struct hash_table) -
	    (DEFAULT_HASH_SIZE * sizeof(struct hash_bucket *)) +
	    (count * sizeof(struct hash_bucket *)), name);
	if (rval == NULL)
		return (NULL);
	rval->hash_count = count;
	return (rval);
}

struct hash_bucket *
new_hash_bucket(char *name)
{
	struct hash_bucket *rval = dmalloc(sizeof(struct hash_bucket), name);

	return (rval);
}

void
free_hash_bucket(struct hash_bucket *ptr, char *name)
{
	dfree(ptr, name);
}
