/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: sortlist.c,v 1.5 2001/03/26 23:36:00 gson Exp $ */

#include <config.h>

#include <isc/mem.h>
#include <isc/util.h>

#include <dns/acl.h>
#include <dns/result.h>

#include <named/globals.h>
#include <named/server.h>
#include <named/sortlist.h>

ns_sortlisttype_t
ns_sortlist_setup(dns_acl_t *acl, isc_netaddr_t *clientaddr, void **argp) {
	unsigned int i;

	if (acl == NULL)
		goto dont_sort;

	for (i = 0; i < acl->length; i++) {
		/*
		 * 'e' refers to the current 'top level statement'
		 * in the sortlist (see ARM).
		 */
		dns_aclelement_t *e = &acl->elements[i];
		dns_aclelement_t *matchelt = NULL;
		dns_acl_t *inner;

		if (e->type != dns_aclelementtype_nestedacl)
			goto dont_sort;

		inner = e->u.nestedacl;

		if (inner->length < 1 || inner->length > 2)
			goto dont_sort;

		if (inner->elements[0].negative)
			goto dont_sort;

		if (dns_aclelement_match(clientaddr, NULL,
					 &inner->elements[0],
					 &ns_g_server->aclenv,
					 &matchelt)) {
			if (inner->length == 2) {
				dns_aclelement_t *elt1 = &inner->elements[1];
				if (elt1->type == dns_aclelementtype_nestedacl)
					*argp = elt1->u.nestedacl;
				else if (elt1->type == dns_aclelementtype_localhost &&
					 ns_g_server->aclenv.localhost != NULL)
					*argp = ns_g_server->aclenv.localhost;
				else if (elt1->type == dns_aclelementtype_localnets &&
					 ns_g_server->aclenv.localnets != NULL)
					*argp = ns_g_server->aclenv.localnets;
				else
					goto dont_sort;
				return (NS_SORTLISTTYPE_2ELEMENT);
			} else {
				INSIST(matchelt != NULL);
				*argp = matchelt;
				return (NS_SORTLISTTYPE_1ELEMENT);
			}
		}
	}

	/* No match; don't sort. */
 dont_sort:
	*argp = NULL;
	return (NS_SORTLISTTYPE_NONE);
}

int
ns_sortlist_addrorder2(isc_netaddr_t *addr, void *arg) {
	dns_acl_t *sortacl = (dns_acl_t *) arg;
	int match;

	(void)dns_acl_match(addr, NULL, sortacl,
			    &ns_g_server->aclenv,
			    &match, NULL);
	if (match > 0)
		return (match);
	else if (match < 0)
		return (INT_MAX - (-match));
	else
		return (INT_MAX / 2);
}

int
ns_sortlist_addrorder1(isc_netaddr_t *addr, void *arg) {
	dns_aclelement_t *matchelt = (dns_aclelement_t *) arg;
	if (dns_aclelement_match(addr, NULL, matchelt,
				 &ns_g_server->aclenv,
				 NULL)) {
		return (0);
	} else {
		return (INT_MAX);
	}
}

void
ns_sortlist_byaddrsetup(dns_acl_t *sortlist_acl, isc_netaddr_t *client_addr,
		       dns_addressorderfunc_t *orderp,
		       void **argp)
{
	ns_sortlisttype_t sortlisttype;

	sortlisttype = ns_sortlist_setup(sortlist_acl, client_addr, argp);

	switch (sortlisttype) {
	case NS_SORTLISTTYPE_1ELEMENT:
		*orderp = ns_sortlist_addrorder1;
		break;
	case NS_SORTLISTTYPE_2ELEMENT:
		*orderp = ns_sortlist_addrorder2;
		break;
	case NS_SORTLISTTYPE_NONE:
		*orderp = NULL;
		break;
	default:
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "unexpected return from ns_sortlist_setup(): "
				 "%d", sortlisttype);
		break;
	}
}

