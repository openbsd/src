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
RCSID("$OpenBSD: channel-core.c,v 1.1 2001/05/30 12:55:07 markus Exp $");

#include "ssh1.h"
#include "ssh2.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "channel.h"
#include "compat.h"
#include "canohost.h"

extern int channels_alloc;
extern int channel_max_fd;
extern Channel **channels;

void	 port_open_helper(Channel *c, char *rtype);

/*
 * 'channel_pre*' are called just before select() to add any bits relevant to
 * channels in the select bitmasks.
 */
/*
 * 'channel_post*': perform any appropriate operations for channels which
 * have events pending.
 */
typedef void chan_fn(Channel *c, fd_set * readset, fd_set * writeset);
chan_fn *channel_pre[SSH_CHANNEL_MAX_TYPE];
chan_fn *channel_post[SSH_CHANNEL_MAX_TYPE];

void
channel_pre_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	FD_SET(c->sock, readset);
}

void
channel_pre_connecting(Channel *c, fd_set * readset, fd_set * writeset)
{
	debug3("channel %d: waiting for connection", c->self);
	FD_SET(c->sock, writeset);
}

void
channel_pre_open_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->input) < packet_get_maxsize())
		FD_SET(c->sock, readset);
	if (buffer_len(&c->output) > 0)
		FD_SET(c->sock, writeset);
}

void
channel_pre_open_15(Channel *c, fd_set * readset, fd_set * writeset)
{
	/* test whether sockets are 'alive' for read/write */
	if (c->istate == CHAN_INPUT_OPEN)
		if (buffer_len(&c->input) < packet_get_maxsize())
			FD_SET(c->sock, readset);
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (buffer_len(&c->output) > 0) {
			FD_SET(c->sock, writeset);
		} else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
			chan_obuf_empty(c);
		}
	}
}

void
channel_pre_open_20(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (c->istate == CHAN_INPUT_OPEN &&
	    c->remote_window > 0 &&
	    buffer_len(&c->input) < c->remote_window)
		FD_SET(c->rfd, readset);
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (buffer_len(&c->output) > 0) {
			FD_SET(c->wfd, writeset);
		} else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
			chan_obuf_empty(c);
		}
	}
	/** XXX check close conditions, too */
	if (c->efd != -1) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    buffer_len(&c->extended) > 0)
			FD_SET(c->efd, writeset);
		else if (c->extended_usage == CHAN_EXTENDED_READ &&
		    buffer_len(&c->extended) < c->remote_window)
			FD_SET(c->efd, readset);
	}
}

void
channel_pre_input_draining(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->input) == 0) {
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
		c->type = SSH_CHANNEL_CLOSED;
		debug("channel %d: closing after input drain.", c->self);
	}
}

void
channel_pre_output_draining(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->output) == 0)
		chan_mark_dead(c);
	else
		FD_SET(c->sock, writeset);
}

void
channel_pre_x11_open_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	int ret = x11_check_cookie(&c->output);
	if (ret == 1) {
		/* Start normal processing for the channel. */
		c->type = SSH_CHANNEL_OPEN;
		channel_pre_open_13(c, readset, writeset);
	} else if (ret == -1) {
		/*
		 * We have received an X11 connection that has bad
		 * authentication information.
		 */
		log("X11 connection rejected because of wrong authentication.");
		buffer_clear(&c->input);
		buffer_clear(&c->output);
		close(c->sock);
		c->sock = -1;
		c->type = SSH_CHANNEL_CLOSED;
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
	}
}

void
channel_pre_x11_open(Channel *c, fd_set * readset, fd_set * writeset)
{
	int ret = x11_check_cookie(&c->output);
	if (ret == 1) {
		c->type = SSH_CHANNEL_OPEN;
		if (compat20)
			channel_pre_open_20(c, readset, writeset);
		else
			channel_pre_open_15(c, readset, writeset);
	} else if (ret == -1) {
		debug("X11 rejected %d i%d/o%d", c->self, c->istate, c->ostate);
		chan_read_failed(c);	/** force close? */
		chan_write_failed(c);
		debug("X11 closed %d i%d/o%d", c->self, c->istate, c->ostate);
	}
}

/* try to decode a socks4 header */
int
channel_decode_socks4(Channel *c, fd_set * readset, fd_set * writeset)
{
	u_char *p, *host;
	int len, have, i, found;
	char username[256];	
	struct {
		u_int8_t version;
		u_int8_t command;
		u_int16_t dest_port;
		struct in_addr dest_addr;
	} s4_req, s4_rsp;

	debug2("channel %d: decode socks4", c->self);

	have = buffer_len(&c->input);
	len = sizeof(s4_req);
	if (have < len)
		return 0;
	p = buffer_ptr(&c->input);
	for (found = 0, i = len; i < have; i++) {
		if (p[i] == '\0') {
			found = 1;
			break;
		}
		if (i > 1024) {
			/* the peer is probably sending garbage */
			debug("channel %d: decode socks4: too long",
			    c->self);
			return -1;
		}
	}
	if (!found)
		return 0;
	buffer_get(&c->input, (char *)&s4_req.version, 1);
	buffer_get(&c->input, (char *)&s4_req.command, 1);
	buffer_get(&c->input, (char *)&s4_req.dest_port, 2);
	buffer_get(&c->input, (char *)&s4_req.dest_addr, 4);
	have = buffer_len(&c->input);
	p = buffer_ptr(&c->input);
	len = strlen(p);
	debug2("channel %d: decode socks4: user %s/%d", c->self, p, len);
	if (len > have)
		fatal("channel %d: decode socks4: len %d > have %d",
		    c->self, len, have);
	strlcpy(username, p, sizeof(username));
	buffer_consume(&c->input, len);
	buffer_consume(&c->input, 1);		/* trailing '\0' */

	host = inet_ntoa(s4_req.dest_addr);
	strlcpy(c->path, host, sizeof(c->path));
	c->host_port = ntohs(s4_req.dest_port);
	
	debug("channel %d: dynamic request: socks4 host %s port %u command %u",
	    c->self, host, c->host_port, s4_req.command);

	if (s4_req.command != 1) {
		debug("channel %d: cannot handle: socks4 cn %d",
		    c->self, s4_req.command);
		return -1;
	}
	s4_rsp.version = 0;			/* vn: 0 for reply */
	s4_rsp.command = 90;			/* cd: req granted */
	s4_rsp.dest_port = 0;			/* ignored */
	s4_rsp.dest_addr.s_addr = INADDR_ANY;	/* ignored */
	buffer_append(&c->output, (char *)&s4_rsp, sizeof(s4_rsp));
	return 1;
}

/* dynamic port forwarding */
void
channel_pre_dynamic(Channel *c, fd_set * readset, fd_set * writeset)
{
	u_char *p;
	int have, ret;

	have = buffer_len(&c->input);

	debug2("channel %d: pre_dynamic: have %d", c->self, have);
	/* buffer_dump(&c->input); */
	/* check if the fixed size part of the packet is in buffer. */
	if (have < 4) {
		/* need more */
		FD_SET(c->sock, readset);
		return;
	}
	/* try to guess the protocol */
	p = buffer_ptr(&c->input);
	switch (p[0]) {
	case 0x04:
		ret = channel_decode_socks4(c, readset, writeset);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret < 0) {
		chan_mark_dead(c);
	} else if (ret == 0) {
		debug2("channel %d: pre_dynamic: need more", c->self);
		/* need more */
		FD_SET(c->sock, readset);
	} else {
		/* switch to the next state */
		c->type = SSH_CHANNEL_OPENING;
		port_open_helper(c, "direct-tcpip");
	}
}

/* This is our fake X11 server socket. */
void
channel_post_x11_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	Channel *nc;
	struct sockaddr addr;
	int newsock;
	socklen_t addrlen;
	char buf[16384], *remote_ipaddr;
	int remote_port;

	if (FD_ISSET(c->sock, readset)) {
		debug("X11 connection requested.");
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept: %.100s", strerror(errno));
			return;
		}
		remote_ipaddr = get_peer_ipaddr(newsock);
		remote_port = get_peer_port(newsock);
		snprintf(buf, sizeof buf, "X11 connection from %.200s port %d",
		    remote_ipaddr, remote_port);

		nc = channel_new("accepted x11 socket",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, xstrdup(buf), 1);
		if (nc == NULL) {
			close(newsock);
			xfree(remote_ipaddr);
			return;
		}
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("x11");
			packet_put_int(nc->self);
			packet_put_int(c->local_window_max);
			packet_put_int(c->local_maxpacket);
			/* originator ipaddr and port */
			packet_put_cstring(remote_ipaddr);
			if (datafellows & SSH_BUG_X11FWD) {
				debug("ssh2 x11 bug compat mode");
			} else {
				packet_put_int(remote_port);
			}
			packet_send();
		} else {
			packet_start(SSH_SMSG_X11_OPEN);
			packet_put_int(nc->self);
			if (packet_get_protocol_flags() &
			    SSH_PROTOFLAG_HOST_IN_FWD_OPEN)
				packet_put_string(buf, strlen(buf));
			packet_send();
		}
		xfree(remote_ipaddr);
	}
}

void
port_open_helper(Channel *c, char *rtype)
{
	int direct;
	char buf[1024];
	char *remote_ipaddr = get_peer_ipaddr(c->sock);
	u_short remote_port = get_peer_port(c->sock);

	direct = (strcmp(rtype, "direct-tcpip") == 0);

	snprintf(buf, sizeof buf,
	    "%s: listening port %d for %.100s port %d, "
	    "connect from %.200s port %d",
	    rtype, c->listening_port, c->path, c->host_port,
	    remote_ipaddr, remote_port);

	xfree(c->remote_name);
	c->remote_name = xstrdup(buf);

	if (compat20) {
		packet_start(SSH2_MSG_CHANNEL_OPEN);
		packet_put_cstring(rtype);
		packet_put_int(c->self);
		packet_put_int(c->local_window_max);
		packet_put_int(c->local_maxpacket);
		if (direct) {
			/* target host, port */
			packet_put_cstring(c->path);
			packet_put_int(c->host_port);
		} else {
			/* listen address, port */
			packet_put_cstring(c->path);
			packet_put_int(c->listening_port);
		}
		/* originator host and port */
		packet_put_cstring(remote_ipaddr);
		packet_put_int(remote_port);
		packet_send();
	} else {
		packet_start(SSH_MSG_PORT_OPEN);
		packet_put_int(c->self);
		packet_put_cstring(c->path);
		packet_put_int(c->host_port);
		if (packet_get_protocol_flags() &
		    SSH_PROTOFLAG_HOST_IN_FWD_OPEN)
			packet_put_cstring(c->remote_name);
		packet_send();
	}
	xfree(remote_ipaddr);
}

/*
 * This socket is listening for connections to a forwarded TCP/IP port.
 */
void
channel_post_port_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	Channel *nc;
	struct sockaddr addr;
	int newsock, nextstate;
	socklen_t addrlen;
	char *rtype;

	if (FD_ISSET(c->sock, readset)) {
		debug("Connection to port %d forwarding "
		    "to %.100s port %d requested.",
		    c->listening_port, c->path, c->host_port);

		rtype = (c->type == SSH_CHANNEL_RPORT_LISTENER) ?
		    "forwarded-tcpip" : "direct-tcpip";
		nextstate = (c->host_port == 0 &&
		    c->type != SSH_CHANNEL_RPORT_LISTENER) ?
		    SSH_CHANNEL_DYNAMIC : SSH_CHANNEL_OPENING;

		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept: %.100s", strerror(errno));
			return;
		}
		nc = channel_new(rtype,
		    nextstate, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, xstrdup(rtype), 1);
		if (nc == NULL) {
			error("channel_post_port_listener: no new channel:");
			close(newsock);
			return;
		}
		nc->listening_port = c->listening_port;
		nc->host_port = c->host_port;
		strlcpy(nc->path, c->path, sizeof(nc->path));

		if (nextstate != SSH_CHANNEL_DYNAMIC)
			port_open_helper(nc, rtype);
	}
}

/*
 * This is the authentication agent socket listening for connections from
 * clients.
 */
void
channel_post_auth_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	Channel *nc;
	char *name;
	int newsock;
	struct sockaddr addr;
	socklen_t addrlen;

	if (FD_ISSET(c->sock, readset)) {
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept from auth socket: %.100s", strerror(errno));
			return;
		}
		name = xstrdup("accepted auth socket");
		nc = channel_new("accepted auth socket",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, name, 1);
		if (nc == NULL) {
			error("channel_post_auth_listener: channel_new failed");
			xfree(name);
			close(newsock);
		}
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("auth-agent@openssh.com");
			packet_put_int(nc->self);
			packet_put_int(c->local_window_max);
			packet_put_int(c->local_maxpacket);
		} else {
			packet_start(SSH_SMSG_AGENT_OPEN);
			packet_put_int(nc->self);
		}
		packet_send();
	}
}

void
channel_post_connecting(Channel *c, fd_set * readset, fd_set * writeset)
{
	int err = 0;
	int sz = sizeof(err);

	if (FD_ISSET(c->sock, writeset)) {
		if (getsockopt(c->sock, SOL_SOCKET, SO_ERROR, (char *)&err,
		    &sz) < 0) {
			err = errno;
			error("getsockopt SO_ERROR failed");
		}
		if (err == 0) {
			debug("channel %d: connected", c->self);
			c->type = SSH_CHANNEL_OPEN;
			if (compat20) {
				packet_start(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION);
				packet_put_int(c->remote_id);
				packet_put_int(c->self);
				packet_put_int(c->local_window);
				packet_put_int(c->local_maxpacket);
			} else {
				packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
				packet_put_int(c->remote_id);
				packet_put_int(c->self);
			}
		} else {
			debug("channel %d: not connected: %s",
			    c->self, strerror(err));
			if (compat20) {
				packet_start(SSH2_MSG_CHANNEL_OPEN_FAILURE);
				packet_put_int(c->remote_id);
				packet_put_int(SSH2_OPEN_CONNECT_FAILED);
				if (!(datafellows & SSH_BUG_OPENFAILURE)) {
					packet_put_cstring(strerror(err));
					packet_put_cstring("");
				}
			} else {
				packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
				packet_put_int(c->remote_id);
			}
			chan_mark_dead(c);
		}
		packet_send();
	}
}

int
channel_handle_rfd(Channel *c, fd_set * readset, fd_set * writeset)
{
	char buf[16*1024];
	int len;

	if (c->rfd != -1 &&
	    FD_ISSET(c->rfd, readset)) {
		len = read(c->rfd, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			return 1;
		if (len <= 0) {
			debug("channel %d: read<=0 rfd %d len %d",
			    c->self, c->rfd, len);
			if (c->type != SSH_CHANNEL_OPEN) {
				debug("channel %d: not open", c->self);
				chan_mark_dead(c);
				return -1;
			} else if (compat13) {
				buffer_consume(&c->output, buffer_len(&c->output));
				c->type = SSH_CHANNEL_INPUT_DRAINING;
				debug("channel %d: input draining.", c->self);
			} else {
				chan_read_failed(c);
			}
			return -1;
		}
		if(c->input_filter != NULL) {
			if (c->input_filter(c, buf, len) == -1) {
				debug("channel %d: filter stops", c->self);
				chan_read_failed(c);
			}
		} else {
			buffer_append(&c->input, buf, len);
		}
	}
	return 1;
}
int
channel_handle_wfd(Channel *c, fd_set * readset, fd_set * writeset)
{
	struct termios tio;
	int len;

	/* Send buffered output data to the socket. */
	if (c->wfd != -1 &&
	    FD_ISSET(c->wfd, writeset) &&
	    buffer_len(&c->output) > 0) {
		len = write(c->wfd, buffer_ptr(&c->output),
		    buffer_len(&c->output));
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			return 1;
		if (len <= 0) {
			if (c->type != SSH_CHANNEL_OPEN) {
				debug("channel %d: not open", c->self);
				chan_mark_dead(c);
				return -1;
			} else if (compat13) {
				buffer_consume(&c->output, buffer_len(&c->output));
				debug("channel %d: input draining.", c->self);
				c->type = SSH_CHANNEL_INPUT_DRAINING;
			} else {
				chan_write_failed(c);
			}
			return -1;
		}
		if (compat20 && c->isatty) {
			if (tcgetattr(c->wfd, &tio) == 0 &&
			    !(tio.c_lflag & ECHO) && (tio.c_lflag & ICANON)) {
				/*
				 * Simulate echo to reduce the impact of
				 * traffic analysis. We need to match the
				 * size of a SSH2_MSG_CHANNEL_DATA message
				 * (4 byte channel id + data)
				 */
				packet_send_ignore(4 + len);
				packet_send();
			}
		}
		buffer_consume(&c->output, len);
		if (compat20 && len > 0) {
			c->local_consumed += len;
		}
	}
	return 1;
}
int
channel_handle_efd(Channel *c, fd_set * readset, fd_set * writeset)
{
	char buf[16*1024];
	int len;

/** XXX handle drain efd, too */
	if (c->efd != -1) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    FD_ISSET(c->efd, writeset) &&
		    buffer_len(&c->extended) > 0) {
			len = write(c->efd, buffer_ptr(&c->extended),
			    buffer_len(&c->extended));
			debug2("channel %d: written %d to efd %d",
			    c->self, len, c->efd);
			if (len < 0 && (errno == EINTR || errno == EAGAIN))
				return 1;
			if (len <= 0) {
				debug2("channel %d: closing write-efd %d",
				    c->self, c->efd);
				close(c->efd);
				c->efd = -1;
			} else {
				buffer_consume(&c->extended, len);
				c->local_consumed += len;
			}
		} else if (c->extended_usage == CHAN_EXTENDED_READ &&
		    FD_ISSET(c->efd, readset)) {
			len = read(c->efd, buf, sizeof(buf));
			debug2("channel %d: read %d from efd %d",
			     c->self, len, c->efd);
			if (len < 0 && (errno == EINTR || errno == EAGAIN))
				return 1;
			if (len <= 0) {
				debug2("channel %d: closing read-efd %d",
				    c->self, c->efd);
				close(c->efd);
				c->efd = -1;
			} else {
				buffer_append(&c->extended, buf, len);
			}
		}
	}
	return 1;
}
int
channel_check_window(Channel *c)
{
	if (c->type == SSH_CHANNEL_OPEN &&
	    !(c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD)) &&
	    c->local_window < c->local_window_max/2 &&
	    c->local_consumed > 0) {
		packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
		packet_put_int(c->remote_id);
		packet_put_int(c->local_consumed);
		packet_send();
		debug2("channel %d: window %d sent adjust %d",
		    c->self, c->local_window,
		    c->local_consumed);
		c->local_window += c->local_consumed;
		c->local_consumed = 0;
	}
	return 1;
}

void
channel_post_open_1(Channel *c, fd_set * readset, fd_set * writeset)
{
	channel_handle_rfd(c, readset, writeset);
	channel_handle_wfd(c, readset, writeset);
}

void
channel_post_open_2(Channel *c, fd_set * readset, fd_set * writeset)
{
	channel_handle_rfd(c, readset, writeset);
	channel_handle_wfd(c, readset, writeset);
	channel_handle_efd(c, readset, writeset);

	channel_check_window(c);
}

void
channel_post_output_drain_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	int len;
	/* Send buffered output data to the socket. */
	if (FD_ISSET(c->sock, writeset) && buffer_len(&c->output) > 0) {
		len = write(c->sock, buffer_ptr(&c->output),
			    buffer_len(&c->output));
		if (len <= 0)
			buffer_consume(&c->output, buffer_len(&c->output));
		else
			buffer_consume(&c->output, len);
	}
}

void
channel_handler_init_20(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_20;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_RPORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	channel_pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open_2;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_RPORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open_2;
}

void
channel_handler_init_13(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_13;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open_13;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_INPUT_DRAINING] =	&channel_pre_input_draining;
	channel_pre[SSH_CHANNEL_OUTPUT_DRAINING] =	&channel_pre_output_draining;
	channel_pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	channel_pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open_1;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_OUTPUT_DRAINING] =	&channel_post_output_drain_13;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open_1;
}

void
channel_handler_init_15(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_15;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	channel_pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;

	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open_1;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open_1;
}

void
channel_handler_init(void)
{
	int i;
	for(i = 0; i < SSH_CHANNEL_MAX_TYPE; i++) {
		channel_pre[i] = NULL;
		channel_post[i] = NULL;
	}
	if (compat20)
		channel_handler_init_20();
	else if (compat13)
		channel_handler_init_13();
	else
		channel_handler_init_15();
}

void
channel_handler(chan_fn *ftab[], fd_set * readset, fd_set * writeset)
{
	static int did_init = 0;
	int i;
	Channel *c;

	if (!did_init) {
		channel_handler_init();
		did_init = 1;
	}
	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		if (ftab[c->type] != NULL)
			(*ftab[c->type])(c, readset, writeset);
		if (chan_is_dead(c)) {
			/*
			 * we have to remove the fd's from the select mask
			 * before the channels are free'd and the fd's are
			 * closed
			 */
			if (c->wfd != -1)
				FD_CLR(c->wfd, writeset);
			if (c->rfd != -1)
				FD_CLR(c->rfd, readset);
			if (c->efd != -1) {
				if (c->extended_usage == CHAN_EXTENDED_READ)
					FD_CLR(c->efd, readset);
				if (c->extended_usage == CHAN_EXTENDED_WRITE)
					FD_CLR(c->efd, writeset);
			}
			channel_free(c);
		}
	}
}

/*
 * Allocate/update select bitmasks and add any bits relevant to channels in
 * select bitmasks.
 */
void
channel_prepare_select(fd_set **readsetp, fd_set **writesetp, int *maxfdp,
    int rekeying)
{
	int n;
	u_int sz;

	n = MAX(*maxfdp, channel_max_fd);

	sz = howmany(n+1, NFDBITS) * sizeof(fd_mask);
	if (*readsetp == NULL || n > *maxfdp) {
		if (*readsetp)
			xfree(*readsetp);
		if (*writesetp)
			xfree(*writesetp);
		*readsetp = xmalloc(sz);
		*writesetp = xmalloc(sz);
		*maxfdp = n;
	}
	memset(*readsetp, 0, sz);
	memset(*writesetp, 0, sz);

	if (!rekeying)
		channel_handler(channel_pre, *readsetp, *writesetp);
}

/*
 * After select, perform any appropriate operations for channels which have
 * events pending.
 */
void
channel_after_select(fd_set * readset, fd_set * writeset)
{
	channel_handler(channel_post, readset, writeset);
}


/* If there is data to send to the connection, enqueue some of it now. */

void
channel_output_poll()
{
	int len, i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;

		/*
		 * We are only interested in channels that can have buffered
		 * incoming data.
		 */
		if (compat13) {
			if (c->type != SSH_CHANNEL_OPEN &&
			    c->type != SSH_CHANNEL_INPUT_DRAINING)
				continue;
		} else {
			if (c->type != SSH_CHANNEL_OPEN)
				continue;
		}
		if (compat20 &&
		    (c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD))) {
			/* XXX is this true? */
			debug2("channel %d: no data after CLOSE", c->self);
			continue;
		}

		/* Get the amount of buffered data for this channel. */
		if ((c->istate == CHAN_INPUT_OPEN ||
		    c->istate == CHAN_INPUT_WAIT_DRAIN) &&
		    (len = buffer_len(&c->input)) > 0) {
			/*
			 * Send some data for the other side over the secure
			 * connection.
			 */
			if (compat20) {
				if (len > c->remote_window)
					len = c->remote_window;
				if (len > c->remote_maxpacket)
					len = c->remote_maxpacket;
			} else {
				if (packet_is_interactive()) {
					if (len > 1024)
						len = 512;
				} else {
					/* Keep the packets at reasonable size. */
					if (len > packet_get_maxsize()/2)
						len = packet_get_maxsize()/2;
				}
			}
			if (len > 0) {
				packet_start(compat20 ?
				    SSH2_MSG_CHANNEL_DATA : SSH_MSG_CHANNEL_DATA);
				packet_put_int(c->remote_id);
				packet_put_string(buffer_ptr(&c->input), len);
				packet_send();
				buffer_consume(&c->input, len);
				c->remote_window -= len;
			}
		} else if (c->istate == CHAN_INPUT_WAIT_DRAIN) {
			if (compat13)
				fatal("cannot happen: istate == INPUT_WAIT_DRAIN for proto 1.3");
			/*
			 * input-buffer is empty and read-socket shutdown:
			 * tell peer, that we will not send more data: send IEOF
			 */
			chan_ibuf_empty(c);
		}
		/* Send extended data, i.e. stderr */
		if (compat20 &&
		    c->remote_window > 0 &&
		    (len = buffer_len(&c->extended)) > 0 &&
		    c->extended_usage == CHAN_EXTENDED_READ) {
			debug2("channel %d: rwin %d elen %d euse %d",
			    c->self, c->remote_window, buffer_len(&c->extended),
			    c->extended_usage);
			if (len > c->remote_window)
				len = c->remote_window;
			if (len > c->remote_maxpacket)
				len = c->remote_maxpacket;
			packet_start(SSH2_MSG_CHANNEL_EXTENDED_DATA);
			packet_put_int(c->remote_id);
			packet_put_int(SSH2_EXTENDED_DATA_STDERR);
			packet_put_string(buffer_ptr(&c->extended), len);
			packet_send();
			buffer_consume(&c->extended, len);
			c->remote_window -= len;
			debug2("channel %d: sent ext data %d", c->self, len);
		}
	}
}
