/*	$OpenBSD: sdl.h,v 1.5 2007/03/29 17:39:53 kjell Exp $ */

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

#ifndef _SDL_H_
#define _SDL_H_

#include <sys/types.h>
#include <sys/socket.h>

/* spamd source list */
struct sdlist {
	char *tag;	/* sdlist source name */
	char *string;	/* Format (451) string with no smtp code or \r\n */
	struct sdentry *addrs;
	size_t naddrs;
};

/* yeah. Stolen from pf */
struct sdaddr {
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		u_int8_t		addr8[16];
		u_int16_t		addr16[8];
		u_int32_t		addr32[4];
	} _sda;		    /* 128-bit address */
#define v4	_sda.v4
#define v6	_sda.v6
#define addr8	_sda.addr8
#define addr16	_sda.addr16
#define addr32	_sda.addr32
};

/* spamd netblock (black) list */
struct sdentry {
	struct sdaddr sda;
	struct sdaddr sdm;
};


extern int	sdl_add(char *, char *, char **, int);
extern struct sdlist **sdl_lookup(struct sdlist *head,
	    int af, void * src);

#endif	/* _SDL_H_ */
