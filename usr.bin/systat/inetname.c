/*	$OpenBSD: inetname.c,v 1.2 2015/01/16 00:03:37 deraadt Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/signal.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"

const char *
inet6name(struct in6_addr *in6)
{
        static char line[NI_MAXHOST];
        struct sockaddr_in6 sin6;
        int flags;

        flags = nflag ? NI_NUMERICHOST : 0;
        if (IN6_IS_ADDR_UNSPECIFIED(in6))
                return "*";
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        sin6.sin6_len = sizeof(struct sockaddr_in6);
        sin6.sin6_addr = *in6;
        if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
            line, sizeof(line), NULL, 0, flags) == 0)
                return line;
        return "?";
}

const char *
inetname(struct in_addr in)
{
        static char line[NI_MAXHOST];
        struct sockaddr_in si;
        int flags, e;

        flags = nflag ? NI_NUMERICHOST : 0;
        if (in.s_addr == INADDR_ANY)
                return "*";

        memset(&si, 0, sizeof(si));
        si.sin_family = AF_INET;
        si.sin_len = sizeof(struct sockaddr_in);
        si.sin_addr = in;

        e = getnameinfo((struct sockaddr *)&si, si.sin_len,
                        line, sizeof(line), NULL, 0, flags);

        if (e == 0)
                return line;

        error("Lookup: %s", gai_strerror(e));

        return "?";
}
