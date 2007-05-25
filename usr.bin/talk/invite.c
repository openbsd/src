/*	$OpenBSD: invite.c,v 1.13 2007/05/25 21:27:16 krw Exp $	*/
/*	$NetBSD: invite.c,v 1.3 1994/12/09 02:14:18 jtc Exp $	*/

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

#ifndef lint
#if 0
static char sccsid[] = "@(#)invite.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] = "$OpenBSD: invite.c,v 1.13 2007/05/25 21:27:16 krw Exp $";
#endif /* not lint */

#include "talk.h"
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <setjmp.h>
#include <unistd.h>
#include "talk_ctl.h"

#define STRING_LENGTH 158

/*
 * There wasn't an invitation waiting, so send a request containing
 * our sockt address to the remote talk daemon so it can invite
 * him
 */

/*
 * The msg.id's for the invitations
 * on the local and remote machines.
 * These are used to delete the
 * invitations.
 */
int	local_id, remote_id;
jmp_buf invitebuf;

void
invite_remote(void)
{
	int new_sockt;
	struct itimerval itimer;
	CTL_RESPONSE response;
	struct sockaddr rp;
	socklen_t rplen = sizeof(struct sockaddr);
	struct hostent *rphost;
	char rname[STRING_LENGTH];

	itimer.it_value.tv_sec = RING_WAIT;
	itimer.it_value.tv_usec = 0;
	itimer.it_interval = itimer.it_value;
	if (listen(sockt, 5) != 0)
		quit("Error on attempt to listen for caller", 1);
#ifdef MSG_EOR
	/* copy new style sockaddr to old, swap family (short in old) */
	msg.addr = *(struct osockaddr *)&my_addr;  /* XXX new to old  style*/
	msg.addr.sa_family = htons(my_addr.sin_family);
#else
	msg.addr = *(struct sockaddr *)&my_addr;
#endif
	msg.id_num = htonl(-1);		/* an impossible id_num */
	invitation_waiting = 1;
	announce_invite();
	/*
	 * Shut off the automatic messages for a while,
	 * so we can use the interrupt timer to resend the invitation.
	 * We no longer turn automatic messages back on to avoid a bonus
	 * message after we've connected; this is okay even though end_msgs()
	 * gets called again in main().
	 */
	end_msgs();
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
	message("Waiting for your party to respond");
	signal(SIGALRM, re_invite);
	(void) setjmp(invitebuf);
	while ((new_sockt = accept(sockt, &rp, &rplen)) == -1) {
		if (errno == EINTR || errno == ECONNABORTED)
			continue;
		quit("Unable to connect with your party", 1);
	}
	close(sockt);
	sockt = new_sockt;

	/*
	 * Have the daemons delete the invitations now that we
	 * have connected.
	 */
	msg.id_num = htonl(local_id);
	ctl_transact(my_machine_addr, msg, DELETE, &response);
	msg.id_num = htonl(remote_id);
	ctl_transact(his_machine_addr, msg, DELETE, &response);
	invitation_waiting = 0;

	/*
	 * Check to see if the other guy is coming from the machine
	 * we expect.
	 */
	if (his_machine_addr.s_addr !=
	    ((struct sockaddr_in *)&rp)->sin_addr.s_addr) {
		rphost = gethostbyaddr((char *) &((struct sockaddr_in
		    *)&rp)->sin_addr, sizeof(struct in_addr), AF_INET);
		if (rphost)
			snprintf(rname, STRING_LENGTH,
			    "Answering talk request from %s@%s", msg.r_name,
			    rphost->h_name);
		else
			snprintf(rname, STRING_LENGTH,
			    "Answering talk request from %s@%s", msg.r_name,
			    inet_ntoa(((struct sockaddr_in *)&rp)->sin_addr));
		message(rname);
	}
}

/*
 * Routine called on interrupt to re-invite the callee
 */
void
re_invite(int dummy)
{
	message("Ringing your party again");
	/* force a re-announce */
	msg.id_num = htonl(remote_id + 1);
	announce_invite();
	longjmp(invitebuf, 1);
}

static	char *answers[] = {
	"answer #0",					/* SUCCESS */
	"Your party is not logged on",			/* NOT_HERE */
	"Target machine is too confused to talk to us",	/* FAILED */
	"Target machine does not recognize us",		/* MACHINE_UNKNOWN */
	"Your party is refusing messages",		/* PERMISSION_REFUSED */
	"Target machine can not handle remote talk",	/* UNKNOWN_REQUEST */
	"Target machine indicates protocol mismatch",	/* BADVERSION */
	"Target machine indicates protocol botch (addr)",/* BADADDR */
	"Target machine indicates protocol botch (ctl_addr)",/* BADCTLADDR */
};
#define NANSWERS (sizeof (answers) / sizeof (answers[0]))

/*
 * Transmit the invitation and process the response
 */
void
announce_invite(void)
{
	CTL_RESPONSE response;

	current_state = "Trying to connect to your party's talk daemon";
	ctl_transact(his_machine_addr, msg, ANNOUNCE, &response);
	remote_id = response.id_num;
	if (response.answer != SUCCESS)
		quit(response.answer < NANSWERS ? answers[response.answer] : NULL, 0);
	/* leave the actual invitation on my talk daemon */
	ctl_transact(my_machine_addr, msg, LEAVE_INVITE, &response);
	local_id = response.id_num;
}

/*
 * Tell the daemon to remove your invitation
 */
void
send_delete(void)
{

	msg.type = DELETE;
	/*
	 * This is just a extra clean up, so just send it
	 * and don't wait for an answer
	 */
	msg.id_num = htonl(remote_id);
	daemon_addr.sin_addr = his_machine_addr;
	if (sendto(ctl_sockt, &msg, sizeof (msg), 0,
	    (struct sockaddr *)&daemon_addr,
	    sizeof (daemon_addr)) != sizeof(msg))
		warn("send_delete (remote)");
	msg.id_num = htonl(local_id);
	daemon_addr.sin_addr = my_machine_addr;
	if (sendto(ctl_sockt, &msg, sizeof (msg), 0,
	    (struct sockaddr *)&daemon_addr,
	    sizeof (daemon_addr)) != sizeof (msg))
		warn("send_delete (local)");
}
