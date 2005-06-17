/*	$OpenBSD: net_addrcmp.c,v 1.10 2005/06/17 20:36:16 henning Exp $	*/

/*
 * Copyright (c) 1999 Theo de Raadt
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>

int
net_addrcmp(struct sockaddr *sa1, struct sockaddr *sa2)
{

	if (sa1->sa_len != sa2->sa_len)
		return (sa1->sa_len < sa2->sa_len) ? -1 : 1;
	if (sa1->sa_family != sa2->sa_family)
		return (sa1->sa_family < sa2->sa_family) ? -1 : 1;

	switch(sa1->sa_family) {
	case AF_INET:
		return (memcmp(&((struct sockaddr_in *)sa1)->sin_addr,
		    &((struct sockaddr_in *)sa2)->sin_addr,
		    sizeof(struct in_addr)));
	case AF_INET6:
		if (((struct sockaddr_in6 *)sa1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)sa2)->sin6_scope_id)
			return (((struct sockaddr_in6 *)sa1)->sin6_scope_id < 
			    ((struct sockaddr_in6 *)sa2)->sin6_scope_id)
			    ? -1 : 1;
		return memcmp(&((struct sockaddr_in6 *)sa1)->sin6_addr,
		    &((struct sockaddr_in6 *)sa2)->sin6_addr,
		    sizeof(struct in6_addr));
	case AF_LOCAL:
		return (strcmp(((struct sockaddr_un *)sa1)->sun_path,
		    ((struct sockaddr_un *)sa1)->sun_path));
	default:
		return -1;
	}
}
