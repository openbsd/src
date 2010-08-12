/*	$OpenBSD: get_addrs.c,v 1.9 2010/08/12 23:31:29 tedu Exp $	*/
/*	$NetBSD: get_addrs.c,v 1.3 1994/12/09 02:14:14 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#include "talk.h"
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include "talk_ctl.h"

void
get_addrs(char *my_machine_name, char *his_machine_name)
{
	struct hostent *hp;
	struct servent *sp;

	msg.pid = htonl(getpid());
	/* look up the address of the local host */
	hp = gethostbyname(my_machine_name);
	if (hp == NULL)
		errx(1, "%s: %s", my_machine_name, hstrerror(h_errno));
	bcopy(hp->h_addr, &my_machine_addr, hp->h_length);
	/*
	 * If the callee is on-machine, just copy the
	 * network address, otherwise do a lookup...
	 */
	if (strcmp(his_machine_name, my_machine_name)) {
		hp = gethostbyname(his_machine_name);
		if (hp == NULL)
			errx(1, "%s: %s", his_machine_name, hstrerror(h_errno));
		bcopy(hp->h_addr, &his_machine_addr, hp->h_length);
	} else
		his_machine_addr = my_machine_addr;
	/* find the server's port */
	sp = getservbyname("ntalk", "udp");
	if (sp == NULL)
		errx(1, "ntalk/udp: service is not registered.");
	daemon_port = sp->s_port;
}
