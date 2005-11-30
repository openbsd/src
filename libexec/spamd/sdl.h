/*	$OpenBSD: sdl.h,v 1.3 2005/11/30 20:44:07 deraadt Exp $ */

/*
 * Copyright (c) 2003 Bob Beck, Kjell Wooding.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
