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
RCSID("$OpenBSD: channel-tcpfwd.c,v 1.2 2001/05/30 16:22:46 markus Exp $");

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "channel.h"
#include "compat.h"

/*
 * Data structure for storing which hosts are permitted for forward requests.
 * The local sides of any remote forwards are stored in this array to prevent
 * a corrupt remote server from accessing arbitrary TCP/IP ports on our local
 * network (which might be behind a firewall).
 */
typedef struct {
	char *host_to_connect;		/* Connect to 'host'. */
	u_short port_to_connect;	/* Connect to 'port'. */
	u_short listen_port;		/* Remote side should listen port number. */
} ForwardPermission;

/* List of all permitted host/port pairs to connect. */
static ForwardPermission permitted_opens[SSH_MAX_FORWARDS_PER_DIRECTION];

/* Number of permitted host/port pairs in the array. */
static int num_permitted_opens = 0;
/*
 * If this is true, all opens are permitted.  This is the case on the server
 * on which we have to trust the client anyway, and the user could do
 * anything after logging in anyway.
 */
static int all_opens_permitted = 0;

/* AF_UNSPEC or AF_INET or AF_INET6 */
extern int IPv4or6;

/*
 * Initiate forwarding of connections to local port "port" through the secure
 * channel to host:port from remote side.
 */
int
channel_request_local_forwarding(u_short listen_port, const char *host_to_connect,
    u_short port_to_connect, int gateway_ports)
{
	return channel_request_forwarding(
	    NULL, listen_port,
	    host_to_connect, port_to_connect,
	    gateway_ports, /*remote_fwd*/ 0);
}

/*
 * If 'remote_fwd' is true we have a '-R style' listener for protocol 2
 * (SSH_CHANNEL_RPORT_LISTENER).
 */
int
channel_request_forwarding(
    const char *listen_address, u_short listen_port,
    const char *host_to_connect, u_short port_to_connect,
    int gateway_ports, int remote_fwd)
{
	Channel *c;
	int success, sock, on = 1, type;
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	const char *host;
	struct linger linger;

	success = 0;

	if (remote_fwd) {
		host = listen_address;
		type = SSH_CHANNEL_RPORT_LISTENER;
	} else {
		host = host_to_connect;
		type = SSH_CHANNEL_PORT_LISTENER;
	}

	if (strlen(host) > SSH_CHANNEL_PATH_LEN - 1) {
		error("Forward host name too long.");
		return success;
	}

	/* XXX listen_address is currently ignored */
	/*
	 * getaddrinfo returns a loopback address if the hostname is
	 * set to NULL and hints.ai_flags is not AI_PASSIVE
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_flags = gateway_ports ? AI_PASSIVE : 0;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", listen_port);
	if (getaddrinfo(NULL, strport, &hints, &aitop) != 0)
		packet_disconnect("getaddrinfo: fatal error");

	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("channel_request_forwarding: getnameinfo failed");
			continue;
		}
		/* Create a port to listen for the host. */
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			/* this is no error since kernel may not support ipv6 */
			verbose("socket: %.100s", strerror(errno));
			continue;
		}
		/*
		 * Set socket options.  We would like the socket to disappear
		 * as soon as it has been closed for whatever reason.
		 */
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));
		linger.l_onoff = 1;
		linger.l_linger = 5;
		setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&linger, sizeof(linger));
		debug("Local forwarding listening on %s port %s.", ntop, strport);

		/* Bind the socket to the address. */
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			/* address can be in use ipv6 address is already bound */
			verbose("bind: %.100s", strerror(errno));
			close(sock);
			continue;
		}
		/* Start listening for connections on the socket. */
		if (listen(sock, 5) < 0) {
			error("listen: %.100s", strerror(errno));
			close(sock);
			continue;
		}
		/* Allocate a channel number for the socket. */
		c = channel_new("port listener", type, sock, sock, -1,
		    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
		    0, xstrdup("port listener"), 1);
		if (c == NULL) {
			error("channel_request_forwarding: channel_new failed");
			close(sock);
			continue;
		}
		strlcpy(c->path, host, sizeof(c->path));
		c->host_port = port_to_connect;
		c->listening_port = listen_port;
		success = 1;
	}
	if (success == 0)
		error("channel_request_forwarding: cannot listen to port: %d",
		    listen_port);
	freeaddrinfo(aitop);
	return success;
}

/*
 * Initiate forwarding of connections to port "port" on remote host through
 * the secure channel to host:port from local side.
 */

void
channel_request_remote_forwarding(u_short listen_port,
    const char *host_to_connect, u_short port_to_connect)
{
	int payload_len, type, success = 0;

	/* Record locally that connection to this host/port is permitted. */
	if (num_permitted_opens >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("channel_request_remote_forwarding: too many forwards");

	/* Send the forward request to the remote side. */
	if (compat20) {
		const char *address_to_bind = "0.0.0.0";
		packet_start(SSH2_MSG_GLOBAL_REQUEST);
		packet_put_cstring("tcpip-forward");
		packet_put_char(0);			/* boolean: want reply */
		packet_put_cstring(address_to_bind);
		packet_put_int(listen_port);
		packet_send();
		packet_write_wait();
		/* Assume that server accepts the request */
		success = 1;
	} else {
		packet_start(SSH_CMSG_PORT_FORWARD_REQUEST);
		packet_put_int(listen_port);
		packet_put_cstring(host_to_connect);
		packet_put_int(port_to_connect);
		packet_send();
		packet_write_wait();

		/* Wait for response from the remote side. */
		type = packet_read(&payload_len);
		switch (type) {
		case SSH_SMSG_SUCCESS:
			success = 1;
			break;
		case SSH_SMSG_FAILURE:
			log("Warning: Server denied remote port forwarding.");
			break;
		default:
			/* Unknown packet */
			packet_disconnect("Protocol error for port forward request:"
			    "received packet type %d.", type);
		}
	}
	if (success) {
		permitted_opens[num_permitted_opens].host_to_connect = xstrdup(host_to_connect);
		permitted_opens[num_permitted_opens].port_to_connect = port_to_connect;
		permitted_opens[num_permitted_opens].listen_port = listen_port;
		num_permitted_opens++;
	}
}

/*
 * This is called after receiving CHANNEL_FORWARDING_REQUEST.  This initates
 * listening for the port, and sends back a success reply (or disconnect
 * message if there was an error).  This never returns if there was an error.
 */

void
channel_input_port_forward_request(int is_root, int gateway_ports)
{
	u_short port, host_port;
	char *hostname;

	/* Get arguments from the packet. */
	port = packet_get_int();
	hostname = packet_get_string(NULL);
	host_port = packet_get_int();

	/*
	 * Check that an unprivileged user is not trying to forward a
	 * privileged port.
	 */
	if (port < IPPORT_RESERVED && !is_root)
		packet_disconnect("Requested forwarding of port %d but user is not root.",
				  port);
	/* Initiate forwarding */
	channel_request_local_forwarding(port, hostname, host_port, gateway_ports);

	/* Free the argument string. */
	xfree(hostname);
}

/*
 * Permits opening to any host/port if permitted_opens[] is empty.  This is
 * usually called by the server, because the user could connect to any port
 * anyway, and the server has no way to know but to trust the client anyway.
 */
void
channel_permit_all_opens()
{
	if (num_permitted_opens == 0)
		all_opens_permitted = 1;
}

void
channel_add_permitted_opens(char *host, int port)
{
	if (num_permitted_opens >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("channel_request_remote_forwarding: too many forwards");
	debug("allow port forwarding to host %s port %d", host, port);

	permitted_opens[num_permitted_opens].host_to_connect = xstrdup(host);
	permitted_opens[num_permitted_opens].port_to_connect = port;
	num_permitted_opens++;

	all_opens_permitted = 0;
}

void
channel_clear_permitted_opens(void)
{
	int i;

	for (i = 0; i < num_permitted_opens; i++)
		xfree(permitted_opens[i].host_to_connect);
	num_permitted_opens = 0;

}


/* return socket to remote host, port */
int
connect_to(const char *host, u_short port)
{
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	int gaierr;
	int sock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0) {
		error("connect_to %.100s: unknown host (%s)", host,
		    gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("connect_to: getnameinfo failed");
			continue;
		}
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			error("socket: %.100s", strerror(errno));
			continue;
		}
		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
			fatal("connect_to: F_SETFL: %s", strerror(errno));
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0 &&
		    errno != EINPROGRESS) {
			error("connect_to %.100s port %s: %.100s", ntop, strport,
			    strerror(errno));
			close(sock);
			continue;	/* fail -- try next */
		}
		break; /* success */

	}
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect_to %.100s port %d: failed.", host, port);
		return -1;
	}
	/* success */
	return sock;
}

int
channel_connect_by_listen_adress(u_short listen_port)
{
	int i;

	for (i = 0; i < num_permitted_opens; i++)
		if (permitted_opens[i].listen_port == listen_port)
			return connect_to(
			    permitted_opens[i].host_to_connect,
			    permitted_opens[i].port_to_connect);
	error("WARNING: Server requests forwarding for unknown listen_port %d",
	    listen_port);
	return -1;
}

/* Check if connecting to that port is permitted and connect. */
int
channel_connect_to(const char *host, u_short port)
{
	int i, permit;

	permit = all_opens_permitted;
	if (!permit) {
		for (i = 0; i < num_permitted_opens; i++)
			if (permitted_opens[i].port_to_connect == port &&
			    strcmp(permitted_opens[i].host_to_connect, host) == 0)
				permit = 1;

	}
	if (!permit) {
		log("Received request to connect to host %.100s port %d, "
		    "but the request was denied.", host, port);
		return -1;
	}
	return connect_to(host, port);
}
