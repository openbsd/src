/*	$OpenBSD: look_up.c,v 1.11 2009/10/27 23:59:44 deraadt Exp $	*/
/*	$NetBSD: look_up.c,v 1.3 1994/12/09 02:14:21 jtc Exp $	*/

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
#include <errno.h>
#include <unistd.h>
#include "talk_ctl.h"

/*
 * See if the local daemon has an invitation for us.
 */
int
check_local(void)
{
	CTL_RESPONSE response;

	/* the rest of msg was set up in get_names */
#ifdef MSG_EOR
	/* copy new style sockaddr to old, swap family (short in old) */
	msg.ctl_addr = *(struct osockaddr *)&ctl_addr;
	msg.ctl_addr.sa_family = htons(ctl_addr.sin_family);
#else
	msg.ctl_addr = *(struct sockaddr *)&ctl_addr;
#endif
	/* must be initiating a talk */
	if (!look_for_invite(&response))
		return (0);
	/*
	 * There was an invitation waiting for us,
	 * so connect with the other (hopefully waiting) party
	 */
	current_state = "Waiting to connect with caller";
	do {
		if (ntohs(response.addr.sa_family) != AF_INET)
			quit("Response uses invalid network address", 1);
		errno = 0;
		if (connect(sockt, (struct sockaddr *)&response.addr,
			    sizeof (response.addr)) != -1)
			return (1);
	} while (errno == EINTR);
	if (errno == ECONNREFUSED) {
		/*
		 * The caller gave up, but his invitation somehow
		 * was not cleared. Clear it and initiate an
		 * invitation. (We know there are no newer invitations,
		 * the talkd works LIFO.)
		 */
		ctl_transact(his_machine_addr, msg, DELETE, &response);
		close(sockt);
		open_sockt();
		return (0);
	}
	quit("Unable to connect with initiator", 1);
	/*NOTREACHED*/
	return (0);
}

/*
 * Look for an invitation on 'machine'
 */
int
look_for_invite(CTL_RESPONSE *rp)
{
	current_state = "Checking for invitation on caller's machine";
	ctl_transact(his_machine_addr, msg, LOOK_UP, rp);
	/* the switch is for later options, such as multiple invitations */
	switch (rp->answer) {

	case SUCCESS:
		msg.id_num = htonl(rp->id_num);
		return (1);

	default:
		/* there wasn't an invitation waiting for us */
		return (0);
	}
}
