/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
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

#include "includes.h"
RCSID("$OpenBSD: channel-auth.c,v 1.1 2001/05/30 12:55:07 markus Exp $");

#include "ssh1.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "channel.h"
#include "key.h"
#include "authfd.h"
#include "uidswap.h"


/* Name and directory of socket for authentication agent forwarding. */
static char *auth_sock_name = NULL;
static char *auth_sock_dir = NULL;

/* Sends a message to the server to request authentication fd forwarding. */

void
auth_request_forwarding()
{
	packet_start(SSH_CMSG_AGENT_REQUEST_FORWARDING);
	packet_send();
	packet_write_wait();
}

/*
 * Returns the name of the forwarded authentication socket.  Returns NULL if
 * there is no forwarded authentication socket.  The returned value points to
 * a static buffer.
 */

char *
auth_get_socket_name()
{
	return auth_sock_name;
}

/* removes the agent forwarding socket */

void
cleanup_socket(void)
{
	unlink(auth_sock_name);
	rmdir(auth_sock_dir);
}

/*
 * This is called to process SSH_CMSG_AGENT_REQUEST_FORWARDING on the server.
 * This starts forwarding authentication requests.
 */

int
auth_input_request_forwarding(struct passwd * pw)
{
	Channel *nc;
	int sock;
	struct sockaddr_un sunaddr;

	if (auth_get_socket_name() != NULL) {
		error("authentication forwarding requested twice.");
		return 0;
	}

	/* Temporarily drop privileged uid for mkdir/bind. */
	temporarily_use_uid(pw);

	/* Allocate a buffer for the socket name, and format the name. */
	auth_sock_name = xmalloc(MAXPATHLEN);
	auth_sock_dir = xmalloc(MAXPATHLEN);
	strlcpy(auth_sock_dir, "/tmp/ssh-XXXXXXXX", MAXPATHLEN);

	/* Create private directory for socket */
	if (mkdtemp(auth_sock_dir) == NULL) {
		packet_send_debug("Agent forwarding disabled: "
		    "mkdtemp() failed: %.100s", strerror(errno));
		restore_uid();
		xfree(auth_sock_name);
		xfree(auth_sock_dir);
		auth_sock_name = NULL;
		auth_sock_dir = NULL;
		return 0;
	}
	snprintf(auth_sock_name, MAXPATHLEN, "%s/agent.%d",
		 auth_sock_dir, (int) getpid());

	if (atexit(cleanup_socket) < 0) {
		int saved = errno;
		cleanup_socket();
		packet_disconnect("socket: %.100s", strerror(saved));
	}
	/* Create the socket. */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		packet_disconnect("socket: %.100s", strerror(errno));

	/* Bind it to the name. */
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	strncpy(sunaddr.sun_path, auth_sock_name,
		sizeof(sunaddr.sun_path));

	if (bind(sock, (struct sockaddr *) & sunaddr, sizeof(sunaddr)) < 0)
		packet_disconnect("bind: %.100s", strerror(errno));

	/* Restore the privileged uid. */
	restore_uid();

	/* Start listening on the socket. */
	if (listen(sock, 5) < 0)
		packet_disconnect("listen: %.100s", strerror(errno));

	/* Allocate a channel for the authentication agent socket. */
	nc = channel_new("auth socket",
	    SSH_CHANNEL_AUTH_SOCKET, sock, sock, -1,
	    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
	    0, xstrdup("auth socket"), 1);
	if (nc == NULL) {
		error("auth_input_request_forwarding: channel_new failed");
		close(sock);
		return 0;
	}
	strlcpy(nc->path, auth_sock_name, sizeof(nc->path));
	return 1;
}

/* This is called to process an SSH_SMSG_AGENT_OPEN message. */

void
auth_input_open_request(int type, int plen, void *ctxt)
{
	Channel *c = NULL;
	int remote_id, sock;
	char *name;

	packet_integrity_check(plen, 4, type);

	/* Read the remote channel number from the message. */
	remote_id = packet_get_int();

	/*
	 * Get a connection to the local authentication agent (this may again
	 * get forwarded).
	 */
	sock = ssh_get_authentication_socket();

	/*
	 * If we could not connect the agent, send an error message back to
	 * the server. This should never happen unless the agent dies,
	 * because authentication forwarding is only enabled if we have an
	 * agent.
	 */
	if (sock >= 0) {
		name = xstrdup("authentication agent connection");
		c = channel_new("", SSH_CHANNEL_OPEN, sock, sock,
		    -1, 0, 0, 0, name, 1);
		if (c == NULL) {
			error("auth_input_open_request: channel_new failed");
			xfree(name);
			close(sock);
		} else {
			c->remote_id = remote_id;
		}
	}
	if (c == NULL) {
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
	} else {
		/* Send a confirmation to the remote host. */
		debug("Forwarding authentication connection.");
		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_id);
		packet_put_int(c->self);
	}
	packet_send();
}
