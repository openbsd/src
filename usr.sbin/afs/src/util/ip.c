/*	$OpenBSD: ip.c,v 1.1.1.1 1998/09/14 21:53:23 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: ip.c,v 1.5 1998/03/13 04:38:09 assar Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include "ip.h"
#include <netdb.h>
#include <sys/socket.h>

/*
 * Get the address given the string representation addr.
 * Can be a name or a x.y.z.d given address.
 */

struct in_addr *
ipgetaddr (const char *addr, struct in_addr *ret)
{
     struct hostent *hostent;

     hostent = gethostbyname (addr);

     if (hostent == NULL) {
	  if ((ret->s_addr = inet_addr(addr)) == -1 )
	       return NULL;
     } else 
	  memcpy(ret,
		 hostent->h_addr_list[0], 
		 hostent->h_length);
     return ret;
}

/*
 * Get the canonical name for this address.
 * If no luck with dns, return dot-separated instead.
 */

char *ipgetname (struct in_addr *addr)
{
     struct hostent *hostent;

     hostent = gethostbyaddr ((const char *) addr, sizeof(*addr), AF_INET);

     if (hostent == NULL)
	  return inet_ntoa (*addr);
     else
	  return hostent->h_name;
}
