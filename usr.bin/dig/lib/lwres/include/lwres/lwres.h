/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: lwres.h,v 1.3 2020/02/12 13:05:04 jsg Exp $ */

#ifndef LWRES_LWRES_H
#define LWRES_LWRES_H 1

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <stdio.h>

#include <lwres/result.h>

/*! \file lwres/lwres.h */

/*!
 * Design notes:
 *
 * Each opcode has two structures and three functions which operate on each
 * structure.  For example, using the "no operation/ping" opcode as an
 * example:
 *
 *	<ul><li>lwres_nooprequest_t:
 *
 *		lwres_nooprequest_render() takes a lwres_nooprequest_t and
 *		and renders it into wire format, storing the allocated
 *		buffer information in a passed-in buffer.  When this buffer
 *		is no longer needed, it must be freed by
 *		lwres_context_freemem().  All other memory used by the
 *		caller must be freed manually, including the
 *		lwres_nooprequest_t passed in.<br /><br />
 *
 *		lwres_nooprequest_parse() takes a wire format message and
 *		breaks it out into a lwres_nooprequest_t.  The structure
 *		must be freed via lwres_nooprequest_free() when it is no longer
 *		needed.<br /><br />
 *
 *		lwres_nooprequest_free() releases into the lwres_context_t
 *		any space allocated during parsing.</li>
 *
 *	<li>lwres_noopresponse_t:
 *
 *		The functions used are similar to the three used for
 *		requests, just with different names.</li></ul>
 *
 * Typically, the client will use request_render, response_parse, and
 * response_free, while the daemon will use request_parse, response_render,
 * and request_free.
 *
 * The basic flow of a typical client is:
 *
 *	\li fill in a request_t, and call the render function.
 *
 *	\li Transmit the buffer returned to the daemon.
 *
 *	\li Wait for a response.
 *
 *	\li When a response is received, parse it into a response_t.
 *
 *	\li free the request buffer using lwres_context_freemem().
 *
 *	\li free the response structure and its associated buffer using
 *	response_free().
 */

#define LWRES_USEIPV4		0x0001
#define LWRES_USEIPV6		0x0002

/*% lwres_addr_t */
typedef struct lwres_addr lwres_addr_t;

/*% lwres_addr */
struct lwres_addr {
	uint32_t			family;
	uint16_t			length;
	unsigned char			address[sizeof(struct in6_addr)];
	uint32_t			zone;
};

/*!
 * resolv.conf data
 */

#define LWRES_CONFMAXNAMESERVERS 3	/*%< max 3 "nameserver" entries */
#define LWRES_CONFMAXLWSERVERS 1	/*%< max 1 "lwserver" entry */
#define LWRES_CONFMAXSEARCH 8		/*%< max 8 domains in "search" entry */
#define LWRES_CONFMAXLINELEN 256	/*%< max size of a line */
#define LWRES_CONFMAXSORTLIST 10	/*%< max 10 */

/*% lwres_conf_t */
typedef struct {
	lwres_addr_t    nameservers[LWRES_CONFMAXNAMESERVERS];
	uint8_t	nsnext;		/*%< index for next free slot */

	lwres_addr_t	lwservers[LWRES_CONFMAXLWSERVERS];
	uint8_t	lwnext;		/*%< index for next free slot */

	char	       *domainname;

	char 	       *search[LWRES_CONFMAXSEARCH];
	uint8_t	searchnxt;	/*%< index for next free slot */

	struct {
		lwres_addr_t addr;
		/*% mask has a non-zero 'family' and 'length' if set */
		lwres_addr_t mask;
	} sortlist[LWRES_CONFMAXSORTLIST];
	uint8_t	sortlistnxt;

	uint8_t	resdebug;      /*%< non-zero if 'options debug' set */
	uint8_t	ndots;	       /*%< set to n in 'options ndots:n' */
	uint8_t	no_tld_query;  /*%< non-zero if 'options no_tld_query' */
	int	flags;
} lwres_conf_t;

#define LWRES_ADDRTYPE_V4		0x00000001U	/*%< ipv4 */
#define LWRES_ADDRTYPE_V6		0x00000002U	/*%< ipv6 */

#define LWRES_MAX_ALIASES		16		/*%< max # of aliases */
#define LWRES_MAX_ADDRS			64		/*%< max # of addrs */

extern const char *lwres_resolv_conf;

lwres_result_t
lwres_conf_parse(lwres_conf_t *confdata, const char *filename);
/**<
 * parses a resolv.conf-format file and stores the results in the structure
 * pointed to by *ctx.
 *
 * Requires:
 *	confdata != NULL
 *	filename != NULL && strlen(filename) > 0
 *
 * Returns:
 *	LWRES_R_SUCCESS on a successful parse.
 *	Anything else on error, although the structure may be partially filled
 *	in.
 */

void
lwres_conf_init(lwres_conf_t *confdata, int lwresflags);
/**<
 * sets all internal fields to a default state. Used to initialize a new
 * lwres_conf_t structure (not reset a used on).
 *
 * Requires:
 *	ctx != NULL
 */

void
lwres_conf_clear(lwres_conf_t *confdata);
/**<
 * frees all internally allocated memory in confdata. Uses the memory
 * routines supplied by ctx.
 *
 * Requires:
 *	confdata != NULL
 */

#endif /* LWRES_LWRES_H */
