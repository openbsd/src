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
RCSID("$OpenBSD: channel-x11.c,v 1.1 2001/05/30 12:55:08 markus Exp $");

#include "ssh1.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "compat.h"
#include "channel.h"

/* Maximum number of fake X11 displays to try. */
#define MAX_DISPLAYS  1000

/* Saved X11 authentication protocol name. */
char *x11_saved_proto = NULL;

/* Saved X11 authentication data.  This is the real data. */
char *x11_saved_data = NULL;
u_int x11_saved_data_len = 0;

/*
 * Fake X11 authentication data.  This is what the server will be sending us;
 * we should replace any occurrences of this by the real data.
 */
char *x11_fake_data = NULL;
u_int x11_fake_data_len;

extern int IPv4or6;

#define	NUM_SOCKS	10

/*
 * This is a special state for X11 authentication spoofing.  An opened X11
 * connection (when authentication spoofing is being done) remains in this
 * state until the first packet has been completely read.  The authentication
 * data in that packet is then substituted by the real data if it matches the
 * fake data, and the channel is put into normal mode.
 * XXX All this happens at the client side.
 * Returns: 0 = need more data, -1 = wrong cookie, 1 = ok
 */
int
x11_check_cookie(Buffer *b)
{
	u_char *ucp;
	u_int proto_len, data_len;

	/* Check if the fixed size part of the packet is in buffer. */
	if (buffer_len(b) < 12)
		return 0;

	/* Parse the lengths of variable-length fields. */
	ucp = (u_char *) buffer_ptr(b);
	if (ucp[0] == 0x42) {	/* Byte order MSB first. */
		proto_len = 256 * ucp[6] + ucp[7];
		data_len = 256 * ucp[8] + ucp[9];
	} else if (ucp[0] == 0x6c) {	/* Byte order LSB first. */
		proto_len = ucp[6] + 256 * ucp[7];
		data_len = ucp[8] + 256 * ucp[9];
	} else {
		debug("Initial X11 packet contains bad byte order byte: 0x%x",
		      ucp[0]);
		return -1;
	}

	/* Check if the whole packet is in buffer. */
	if (buffer_len(b) <
	    12 + ((proto_len + 3) & ~3) + ((data_len + 3) & ~3))
		return 0;

	/* Check if authentication protocol matches. */
	if (proto_len != strlen(x11_saved_proto) ||
	    memcmp(ucp + 12, x11_saved_proto, proto_len) != 0) {
		debug("X11 connection uses different authentication protocol.");
		return -1;
	}
	/* Check if authentication data matches our fake data. */
	if (data_len != x11_fake_data_len ||
	    memcmp(ucp + 12 + ((proto_len + 3) & ~3),
		x11_fake_data, x11_fake_data_len) != 0) {
		debug("X11 auth data does not match fake data.");
		return -1;
	}
	/* Check fake data length */
	if (x11_fake_data_len != x11_saved_data_len) {
		error("X11 fake_data_len %d != saved_data_len %d",
		    x11_fake_data_len, x11_saved_data_len);
		return -1;
	}
	/*
	 * Received authentication protocol and data match
	 * our fake data. Substitute the fake data with real
	 * data.
	 */
	memcpy(ucp + 12 + ((proto_len + 3) & ~3),
	    x11_saved_data, x11_saved_data_len);
	return 1;
}

/*
 * Creates an internet domain socket for listening for X11 connections.
 * Returns a suitable value for the DISPLAY variable, or NULL if an error
 * occurs.
 */
char *
x11_create_display_inet(int screen_number, int x11_display_offset)
{
	int display_number, sock;
	u_short port;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, n, num_socks = 0, socks[NUM_SOCKS];
	char display[512];
	char hostname[MAXHOSTNAMELEN];

	for (display_number = x11_display_offset;
	     display_number < MAX_DISPLAYS;
	     display_number++) {
		port = 6000 + display_number;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = IPv4or6;
		hints.ai_flags = AI_PASSIVE;		/* XXX loopback only ? */
		hints.ai_socktype = SOCK_STREAM;
		snprintf(strport, sizeof strport, "%d", port);
		if ((gaierr = getaddrinfo(NULL, strport, &hints, &aitop)) != 0) {
			error("getaddrinfo: %.100s", gai_strerror(gaierr));
			return NULL;
		}
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			sock = socket(ai->ai_family, SOCK_STREAM, 0);
			if (sock < 0) {
				error("socket: %.100s", strerror(errno));
				return NULL;
			}
			if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
				debug("bind port %d: %.100s", port, strerror(errno));
				shutdown(sock, SHUT_RDWR);
				close(sock);
				for (n = 0; n < num_socks; n++) {
					shutdown(socks[n], SHUT_RDWR);
					close(socks[n]);
				}
				num_socks = 0;
				break;
			}
			socks[num_socks++] = sock;
			if (num_socks == NUM_SOCKS)
				break;
		}
		freeaddrinfo(aitop);
		if (num_socks > 0)
			break;
	}
	if (display_number >= MAX_DISPLAYS) {
		error("Failed to allocate internet-domain X11 display socket.");
		return NULL;
	}
	/* Start listening for connections on the socket. */
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		if (listen(sock, 5) < 0) {
			error("listen: %.100s", strerror(errno));
			shutdown(sock, SHUT_RDWR);
			close(sock);
			return NULL;
		}
	}

	/* Set up a suitable value for the DISPLAY variable. */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		fatal("gethostname: %.100s", strerror(errno));
	snprintf(display, sizeof display, "%.400s:%d.%d", hostname,
		 display_number, screen_number);

	/* Allocate a channel for each socket. */
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		(void) channel_new("x11 listener",
		    SSH_CHANNEL_X11_LISTENER, sock, sock, -1,
		    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
		    0, xstrdup("X11 inet listener"), 1);
	}

	/* Return a suitable value for the DISPLAY environment variable. */
	return xstrdup(display);
}

#ifndef X_UNIX_PATH
#define X_UNIX_PATH "/tmp/.X11-unix/X"
#endif

static
int
connect_local_xsocket(u_int dnr)
{
	static const char *const x_sockets[] = {
		X_UNIX_PATH "%u",
		"/var/X/.X11-unix/X" "%u",
		"/usr/spool/sockets/X11/" "%u",
		NULL
	};
	int sock;
	struct sockaddr_un addr;
	const char *const * path;

	for (path = x_sockets; *path; ++path) {
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock < 0)
			error("socket: %.100s", strerror(errno));
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		snprintf(addr.sun_path, sizeof addr.sun_path, *path, dnr);
		if (connect(sock, (struct sockaddr *) & addr, sizeof(addr)) == 0)
			return sock;
		close(sock);
	}
	error("connect %.100s: %.100s", addr.sun_path, strerror(errno));
	return -1;
}

int
x11_connect_display(void)
{
	int display_number, sock = 0;
	const char *display;
	char buf[1024], *cp;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;

	/* Try to open a socket for the local X server. */
	display = getenv("DISPLAY");
	if (!display) {
		error("DISPLAY not set.");
		return -1;
	}
	/*
	 * Now we decode the value of the DISPLAY variable and make a
	 * connection to the real X server.
	 */

	/*
	 * Check if it is a unix domain socket.  Unix domain displays are in
	 * one of the following formats: unix:d[.s], :d[.s], ::d[.s]
	 */
	if (strncmp(display, "unix:", 5) == 0 ||
	    display[0] == ':') {
		/* Connect to the unix domain socket. */
		if (sscanf(strrchr(display, ':') + 1, "%d", &display_number) != 1) {
			error("Could not parse display number from DISPLAY: %.100s",
			      display);
			return -1;
		}
		/* Create a socket. */
		sock = connect_local_xsocket(display_number);
		if (sock < 0)
			return -1;

		/* OK, we now have a connection to the display. */
		return sock;
	}
	/*
	 * Connect to an inet socket.  The DISPLAY value is supposedly
	 * hostname:d[.s], where hostname may also be numeric IP address.
	 */
	strncpy(buf, display, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;
	cp = strchr(buf, ':');
	if (!cp) {
		error("Could not find ':' in DISPLAY: %.100s", display);
		return -1;
	}
	*cp = 0;
	/* buf now contains the host name.  But first we parse the display number. */
	if (sscanf(cp + 1, "%d", &display_number) != 1) {
		error("Could not parse display number from DISPLAY: %.100s",
		      display);
		return -1;
	}

	/* Look up the host address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", 6000 + display_number);
	if ((gaierr = getaddrinfo(buf, strport, &hints, &aitop)) != 0) {
		error("%.100s: unknown host. (%s)", buf, gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		/* Create a socket. */
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			debug("socket: %.100s", strerror(errno));
			continue;
		}
		/* Connect it to the display. */
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			debug("connect %.100s port %d: %.100s", buf,
			    6000 + display_number, strerror(errno));
			close(sock);
			continue;
		}
		/* Success */
		break;
	}
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect %.100s port %d: %.100s", buf, 6000 + display_number,
		    strerror(errno));
		return -1;
	}
	return sock;
}

/*
 * This is called when SSH_SMSG_X11_OPEN is received.  The packet contains
 * the remote channel number.  We should do whatever we want, and respond
 * with either SSH_MSG_OPEN_CONFIRMATION or SSH_MSG_OPEN_FAILURE.
 */

void
x11_input_open(int type, int plen, void *ctxt)
{
	Channel *c = NULL;
	int remote_id, sock = 0;
	char *remote_host;

	debug("Received X11 open request.");

	remote_id = packet_get_int();

	if (packet_get_protocol_flags() & SSH_PROTOFLAG_HOST_IN_FWD_OPEN) {
		remote_host = packet_get_string(NULL);
	} else {
		remote_host = xstrdup("unknown (remote did not supply name)");
	}
	packet_done();

	/* Obtain a connection to the real X display. */
	sock = x11_connect_display();
	if (sock != -1) {
		/* Allocate a channel for this connection. */
		c = channel_new("connected x11 socket",
		    SSH_CHANNEL_X11_OPEN, sock, sock, -1, 0, 0, 0,
		    remote_host, 1);
		if (c == NULL) {
			error("x11_input_open: channel_new failed");
			close(sock);
		} else {
			c->remote_id = remote_id;
		}
	}
	if (c == NULL) {
		/* Send refusal to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
	} else {
		/* Send a confirmation to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_id);
		packet_put_int(c->self);
	}
	packet_send();
}

/* dummy protocol handler that denies SSH-1 requests (agent/x11) */
void
deny_input_open(int type, int plen, void *ctxt)
{
	int rchan = packet_get_int();
	switch(type){
	case SSH_SMSG_AGENT_OPEN:
		error("Warning: ssh server tried agent forwarding.");
		break;
	case SSH_SMSG_X11_OPEN:
		error("Warning: ssh server tried X11 forwarding.");
		break;
	default:
		error("deny_input_open: type %d plen %d", type, plen);
		break;
	}
	error("Warning: this is probably a break in attempt by a malicious server.");
	packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
	packet_put_int(rchan);
	packet_send();
}

/*
 * Requests forwarding of X11 connections, generates fake authentication
 * data, and enables authentication spoofing.
 * This should be called in the client only.
 */
void
x11_request_forwarding_with_spoofing(int client_session_id,
    const char *proto, const char *data)
{
	u_int data_len = (u_int) strlen(data) / 2;
	u_int i, value, len;
	char *new_data;
	int screen_number;
	const char *cp;
	u_int32_t rand = 0;

	cp = getenv("DISPLAY");
	if (cp)
		cp = strchr(cp, ':');
	if (cp)
		cp = strchr(cp, '.');
	if (cp)
		screen_number = atoi(cp + 1);
	else
		screen_number = 0;

	/* Save protocol name. */
	x11_saved_proto = xstrdup(proto);

	/*
	 * Extract real authentication data and generate fake data of the
	 * same length.
	 */
	x11_saved_data = xmalloc(data_len);
	x11_fake_data = xmalloc(data_len);
	for (i = 0; i < data_len; i++) {
		if (sscanf(data + 2 * i, "%2x", &value) != 1)
			fatal("x11_request_forwarding: bad authentication data: %.100s", data);
		if (i % 4 == 0)
			rand = arc4random();
		x11_saved_data[i] = value;
		x11_fake_data[i] = rand & 0xff;
		rand >>= 8;
	}
	x11_saved_data_len = data_len;
	x11_fake_data_len = data_len;

	/* Convert the fake data into hex. */
	len = 2 * data_len + 1;
	new_data = xmalloc(len);
	for (i = 0; i < data_len; i++)
		snprintf(new_data + 2 * i, len - 2 * i,
		    "%02x", (u_char) x11_fake_data[i]);

	/* Send the request packet. */
	if (compat20) {
		channel_request_start(client_session_id, "x11-req", 0);
		packet_put_char(0);	/* XXX bool single connection */
	} else {
		packet_start(SSH_CMSG_X11_REQUEST_FORWARDING);
	}
	packet_put_cstring(proto);
	packet_put_cstring(new_data);
	packet_put_int(screen_number);
	packet_send();
	packet_write_wait();
	xfree(new_data);
}
