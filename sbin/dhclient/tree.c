/*	$OpenBSD: tree.c,v 1.10 2004/04/14 20:22:27 henning Exp $	*/

/* Routines for manipulating parse trees... */

/*
 * Copyright (c) 1995, 1996, 1997 The Internet Software Consortium.
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

extern int h_errno;

static time_t	tree_evaluate_recurse(int *, unsigned char **, int *,
		    struct tree *);
static time_t	do_host_lookup(int *, unsigned char **, int *,
		    struct dns_host_entry *);
static void	do_data_copy(int *, unsigned char **, int *, unsigned char *,
		    int);

pair
cons(caddr_t car, pair cdr)
{
	pair foo = calloc(1, sizeof(*foo));
	if (!foo)
		error("no memory for cons.");
	foo->car = car;
	foo->cdr = cdr;
	return (foo);
}

struct dns_host_entry *
enter_dns_host(char *name)
{
	struct dns_host_entry *dh;
	int len = strlen(name) + 1;

	if (!(dh = calloc(1, sizeof(struct dns_host_entry)))
	    || !(dh->hostname = calloc(1, len)))
		error("Can't allocate space for new host.");
	strlcpy(dh->hostname, name, len);
	return (dh);
}

int
tree_evaluate(struct tree_cache *tree_cache)
{
	unsigned char *bp = tree_cache->value;
	int bc = tree_cache->buf_size;
	int bufix = 0;

	/*
	 * If there's no tree associated with this cache, it evaluates
	 * to a constant and that was detected at startup.
	 */
	if (!tree_cache->tree)
		return (1);

	/* Try to evaluate the tree without allocating more memory... */
	tree_cache->timeout = tree_evaluate_recurse(&bufix, &bp, &bc,
	    tree_cache->tree);

	/* No additional allocation needed? */
	if (bufix <= bc) {
		tree_cache->len = bufix;
		return (1);
	}

	/*
	 * If we can't allocate more memory, return with what we have
	 * (maybe nothing).
	 */
	if (!(bp = calloc(1, bufix)))
		return (0);

	/* Record the change in conditions... */
	bc = bufix;
	bufix = 0;

	/*
	 * Note that the size of the result shouldn't change on the
	 * second call to tree_evaluate_recurse, since we haven't
	 * changed the ``current'' time.
	 */
	tree_evaluate_recurse(&bufix, &bp, &bc, tree_cache->tree);

	/*
	 * Free the old buffer if needed, then store the new buffer
	 * location and size and return.
	 */
	free(tree_cache->value);
	tree_cache->value = bp;
	tree_cache->len = bufix;
	tree_cache->buf_size = bc;
	return (1);
}

static time_t
tree_evaluate_recurse(int *bufix, unsigned char **bufp, int *bufcount,
    struct tree *tree)
{
	int limit;
	time_t t1, t2;

	switch (tree->op) {
	case TREE_CONCAT:
		t1 = tree_evaluate_recurse(bufix, bufp, bufcount,
		    tree->data.concat.left);
		t2 = tree_evaluate_recurse(bufix, bufp, bufcount,
		    tree->data.concat.right);
		if (t1 > t2)
			return (t2);
		return (t1);

	case TREE_HOST_LOOKUP:
		return (do_host_lookup(bufix, bufp, bufcount,
		    tree->data.host_lookup.host));

	case TREE_CONST:
		do_data_copy(bufix, bufp, bufcount,
		    tree->data.const_val.data,
		    tree->data.const_val.len);
		t1 = MAX_TIME;
		return (t1);

	case TREE_LIMIT:
		limit = *bufix + tree->data.limit.limit;
		t1 = tree_evaluate_recurse(bufix, bufp, bufcount,
		    tree->data.limit.tree);
		*bufix = limit;
		return (t1);

	default:
		warn("Bad node id in tree: %d.", tree->op);
		t1 = MAX_TIME;
		return (t1);
	}
}

static time_t
do_host_lookup(int *bufix, unsigned char **bufp, int *bufcount,
    struct dns_host_entry *dns)
{
	struct hostent *h;
	int i;
	int new_len;

	/* If the record hasn't timed out, just copy the data and return. */
	if (cur_time <= dns->timeout) {
		do_data_copy(bufix, bufp, bufcount,
		    dns->data, dns->data_len);
		return (dns->timeout);
	}

	/* Otherwise, look it up... */
	h = gethostbyname(dns->hostname);
	if (h == NULL) {
		switch (h_errno) {
		case HOST_NOT_FOUND:
			warn("%s: host unknown.", dns->hostname);
			break;
		case TRY_AGAIN:
			warn("%s: temporary name server failure",
			    dns->hostname);
			break;
		case NO_RECOVERY:
			warn("%s: name server failed", dns->hostname);
			break;
		case NO_DATA:
			warn("%s: no A record associated with address",
			    dns->hostname);
		}
		/* Okay to try again after a minute. */
		return (cur_time + 60);
	}

	/* Count the number of addresses we got... */
	for (i = 0; h->h_addr_list[i]; i++)
		;

	/* Do we need to allocate more memory? */
	new_len = i * h->h_length;
	if (dns->buf_len < i) {
		unsigned char *buf = calloc(1, new_len);
		/* If we didn't get more memory, use what we have. */
		if (!buf) {
			new_len = dns->buf_len;
			if (!dns->buf_len) {
				dns->timeout = cur_time + 60;
				return (dns->timeout);
			}
		} else {
			if (dns->data)
				free(dns->data);
			dns->data = buf;
			dns->buf_len = new_len;
		}
	}

	/*
	 * Addresses are conveniently stored one to the buffer, so we
	 * have to copy them out one at a time... :'(
	 */
	for (i = 0; i < new_len / h->h_length; i++) {
		memcpy(dns->data + h->h_length * i,
			h->h_addr_list[i], h->h_length);
	}
	dns->data_len = new_len;

	/*
	 * Set the timeout for an hour from now.
	 * XXX: This should really use the time on the DNS reply.
	 */
	dns->timeout = cur_time + 3600;

	do_data_copy(bufix, bufp, bufcount, dns->data, dns->data_len);
	return (dns->timeout);
}

static void
do_data_copy(int *bufix, unsigned char **bufp, int *bufcount,
    unsigned char *data, int len)
{
	int space = *bufcount - *bufix;

	/* If there's more space than we need, use only what we need. */
	if (space > len)
		space = len;

	/*
	 * Copy as much data as will fit, then increment the buffer
	 * index by the amount we actually had to copy, which could be
	 * more.
	 */
	if (space > 0)
		memcpy(*bufp + *bufix, data, space);
	*bufix += len;
}
