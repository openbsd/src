/*	$OpenBSD: alloc.c,v 1.3 2004/02/04 12:16:56 henning Exp $	*/

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
	return foo;
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

struct packet *
new_packet(char *name)
{
	struct packet *rval;

	rval = (struct packet *)dmalloc(sizeof(struct packet), name);
	return rval;
}

struct dhcp_packet *
new_dhcp_packet(char *name)
{
	struct dhcp_packet *rval;

	rval = (struct dhcp_packet *)dmalloc(sizeof(struct dhcp_packet),
	    name);
	return rval;
}

struct tree *
new_tree(char *name)
{
	struct tree *rval = dmalloc(sizeof(struct tree), name);

	return rval;
}

struct string_list *
new_string_list(size_t size, char * name)
{
	struct string_list *rval;

	rval = dmalloc(sizeof(struct string_list) + size, name);
	if (rval != NULL)
		rval->string = ((char *)rval) + sizeof(struct string_list);
	return rval;
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
	return rval;
}

struct hash_table *
new_hash_table(int count, char *name)
{
	struct hash_table *rval;

	rval = dmalloc(sizeof (struct hash_table) -
	    (DEFAULT_HASH_SIZE * sizeof(struct hash_bucket *)) +
	    (count * sizeof(struct hash_bucket *)), name);
	if (rval == NULL)
		return NULL;
	rval->hash_count = count;
	return rval;
}

struct hash_bucket *
new_hash_bucket(char *name)
{
	struct hash_bucket *rval = dmalloc(sizeof(struct hash_bucket), name);

	return rval;
}

struct lease *
new_leases(int n, char *name)
{
	struct lease *rval = dmalloc(n * sizeof(struct lease), name);

	return rval;
}

struct lease *
new_lease(char *name)
{
	struct lease *rval = dmalloc(sizeof(struct lease), name);

	return rval;
}

struct subnet *
new_subnet(char *name)
{
	struct subnet *rval = dmalloc(sizeof(struct subnet), name);

	return rval;
}

struct class *
new_class(char *name)
{
	struct class *rval = dmalloc(sizeof(struct class), name);

	return rval;
}

struct shared_network *
new_shared_network(char *name)
{
	struct shared_network *rval =
	    dmalloc(sizeof(struct shared_network), name);

	return rval;
}

struct group *
new_group(char *name)
{
	struct group *rval =
	    dmalloc(sizeof(struct group), name);

	return rval;
}

struct protocol *
new_protocol(char *name)
{
	struct protocol *rval = dmalloc(sizeof(struct protocol), name);

	return rval;
}

struct lease_state *free_lease_states;

struct lease_state *
new_lease_state(char *name)
{
	struct lease_state *rval;

	if (free_lease_states) {
		rval = free_lease_states;
		free_lease_states =
		    (struct lease_state *)(free_lease_states->next);
	} else
		rval = dmalloc(sizeof (struct lease_state), name);
	return rval;
}

struct domain_search_list *
new_domain_search_list(char *name)
{
	struct domain_search_list *rval =
	    dmalloc(sizeof (struct domain_search_list), name);

	return rval;
}

struct name_server *
new_name_server(char *name)
{
	struct name_server *rval =
	    dmalloc(sizeof (struct name_server), name);

	return rval;
}

void
free_name_server(struct name_server *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_domain_search_list(struct domain_search_list *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_lease_state(struct lease_state *ptr, char *name)
{
	if (ptr->prl)
		dfree(ptr->prl, name);
	ptr->next = free_lease_states;
	free_lease_states = ptr;
}

void
free_protocol(struct protocol *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_group(struct group *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_shared_network(struct shared_network *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_class(struct class *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_subnet(struct subnet *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_lease(struct lease *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_hash_bucket(struct hash_bucket *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_hash_table(struct hash_table *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_tree_cache(struct tree_cache *ptr, char *name)
{
	ptr->value = (unsigned char *)free_tree_caches;
	free_tree_caches = ptr;
}

void
free_packet(struct packet *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_dhcp_packet(struct dhcp_packet *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_tree(struct tree *ptr, char *name)
{
	dfree(ptr, name);
}

void
free_string_list(struct string_list *ptr, char *name)
{
	dfree(ptr, name);
}
