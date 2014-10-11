/*	$OpenBSD: sdl.c,v 1.19 2014/10/11 03:25:16 doug Exp $ */

/*
 * Copyright (c) 2003-2007 Bob Beck.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * sdl.c - Implement spamd source lists
 *
 * This consists of everything we need to do to determine which lists
 * someone is on. Spamd gets the connecting address, and looks it up
 * against all lists to determine what deferral messages to feed back
 * to the connecting machine. - The redirection to spamd will happen
 * from pf in the kernel, first macth will rdr to us. Spamd (along with
 * setup) must keep track of *all* matches, so as to tell someone all the
 * lists that they are on.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl.h"

static void sdl_free(struct sdlist *);
static void sdl_clear(struct sdlist *);
int match_addr(struct sdaddr *a, struct sdaddr *m, struct sdaddr *b,
    sa_family_t af);

extern int debug;
struct sdlist *blacklists = NULL;
int blc = 0, blu = 0;

int
sdl_add(char *sdname, char *sdstring, char ** addrs, int addrc)
{
	int i, idx = -1;
	char astring[40];
	unsigned int maskbits;
	struct sdaddr *m, *n;

	/*
	 * if a blacklist of same tag name is already there, replace it,
	 * otherwise append.
	 */
	for (i = 0; i < blu; i++) {
		if (strcmp(blacklists[i].tag, sdname) == 0) {
			idx = i;
			break;
		}
	}
	if (idx != -1) {
		if (debug > 0)
			printf("replacing list %s; %d new entries\n",
			    blacklists[idx].tag, addrc);
		sdl_free(&blacklists[idx]);
	} else {
		if (debug > 0)
			printf("adding list %s; %d entries\n", sdname, addrc);
		idx = blu;
	}
	if (idx == blu && blu == blc) {
		struct sdlist *tmp;

		tmp = reallocarray(blacklists, blc + 128,
		    sizeof(struct sdlist));
		if (tmp == NULL)
			return (-1);
		blacklists = tmp;
		blc += 128;
		sdl_clear(&blacklists[idx]);
	}

	if ((blacklists[idx].tag = strdup(sdname)) == NULL)
		goto misc_error;
	if ((blacklists[idx].string = strdup(sdstring)) == NULL)
		goto misc_error;

	blacklists[idx].naddrs = addrc;

	/*
	 * Cycle through addrs, converting. We assume they are correctly
	 * formatted v4 and v6 addrs, if they don't all convert correctly, the
	 * add fails. Each address should be address/maskbits
	 */
	blacklists[idx].addrs = calloc(addrc, sizeof(struct sdentry));
	if (blacklists[idx].addrs == NULL)
		goto misc_error;

	for (i = 0; i < addrc; i++) {
		int j, k, af;

		n = &blacklists[idx].addrs[i].sda;
		m = &blacklists[idx].addrs[i].sdm;

		j = sscanf(addrs[i], "%39[^/]/%u", astring, &maskbits);
		if (j != 2)
			goto parse_error;
		if (maskbits > 128)
			goto parse_error;
		/*
		 * sanity check! we don't allow a 0 mask -
		 * don't blacklist the entire net.
		 */
		if (maskbits == 0)
			goto parse_error;
		if (strchr(astring, ':') != NULL)
			af = AF_INET6;
		else
			af = AF_INET;
		if (af == AF_INET && maskbits > 32)
			goto parse_error;
		j = inet_pton(af, astring, n);
		if (j != 1)
			goto parse_error;
		if (debug > 0)
			printf("added %s/%u\n", astring, maskbits);

		/* set mask, borrowed from pf */
		k = 0;
		for (j = 0; j < 4; j++)
			m->addr32[j] = 0;
		while (maskbits >= 32) {
			m->addr32[k++] = 0xffffffff;
			maskbits -= 32;
		}
		for (j = 31; j > 31 - maskbits; --j)
			m->addr32[k] |= (1 << j);
		if (maskbits)
			m->addr32[k] = htonl(m->addr32[k]);

		/* mask off address bits that won't ever be used */
		for (j = 0; j < 4; j++)
			n->addr32[j] = n->addr32[j] & m->addr32[j];
	}
	if (idx == blu) {
		blu++;
		blacklists[blu].tag = NULL;
	}
	return (0);
 parse_error:
	if (debug > 0)
		printf("sdl_add: parse error, \"%s\"\n", addrs[i]);
 misc_error:
	sdl_free(&blacklists[idx]);
	return (-1);
}

void
sdl_del(char *sdname)
{
	int i, idx = -1;

	for (i = 0; i < blu; i++) {
		if (strcmp(blacklists[i].tag, sdname) == 0) {
			idx = i;
			break;
		}
	}
	if (idx != -1) {
		if (debug > 0)
			printf("clearing list %s\n", sdname);
		free(blacklists[idx].string);
		free(blacklists[idx].addrs);
		blacklists[idx].string = NULL;
		blacklists[idx].addrs = NULL;
		blacklists[idx].naddrs = 0;
	}
}

/*
 * Return 1 if the addresses a (with mask m) matches address b
 * otherwise return 0. It is assumed that address a has been
 * pre-masked out, we only need to mask b.
 */
int
match_addr(struct sdaddr *a, struct sdaddr *m, struct sdaddr *b,
    sa_family_t af)
{
	int	match = 0;

	switch (af) {
	case AF_INET:
		if ((a->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0]))
			match++;
		break;
	case AF_INET6:
		if (((a->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0])) &&
		    ((a->addr32[1]) ==
		    (b->addr32[1] & m->addr32[1])) &&
		    ((a->addr32[2]) ==
		    (b->addr32[2] & m->addr32[2])) &&
		    ((a->addr32[3]) ==
		    (b->addr32[3] & m->addr32[3])))
			match++;
		break;
	}
	return (match);
}


/*
 * Given an address and address family
 * return list of pointers to matching nodes. or NULL if none.
 */
struct sdlist **
sdl_lookup(struct sdlist *head, int af, void * src)
{
	int i, matches = 0;
	struct sdlist *sdl;
	struct sdentry *sda;
	struct sdaddr *source = (struct sdaddr *) src;
	int sdnewlen = 0;
	struct sdlist **sdnew = NULL;

	if (head == NULL)
		return (NULL);
	else
		sdl = head;
	while (sdl->tag != NULL) {
		for (i = 0; i < sdl->naddrs; i++) {
			sda = sdl->addrs + i;
			if (match_addr(&sda->sda, &sda->sdm, source, af)) {
				if (matches == sdnewlen) {
					struct sdlist **tmp;

					tmp = reallocarray(sdnew,
					    sdnewlen + 128,
					    sizeof(struct sdlist *));
					if (tmp == NULL)
						/*
						 * XXX out of memory -
						 * return what we have
						 */
						return (sdnew);
					sdnew = tmp;
					sdnewlen += 128;
				}
				sdnew[matches]= sdl;
				matches++;
				sdnew[matches]=NULL;
				break;
			}
		}
		sdl++;
	}
	return (sdnew);
}

static void
sdl_free(struct sdlist *sdl)
{
	free(sdl->tag);
	free(sdl->string);
	free(sdl->addrs);
	sdl_clear(sdl);
}

static void
sdl_clear(struct sdlist *sdl)
{
	sdl->tag = NULL;
	sdl->string = NULL;
	sdl->addrs = NULL;
	sdl->naddrs = 0;
}

