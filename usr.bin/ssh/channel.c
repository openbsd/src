/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file contains functions for generic socket connection forwarding.
 * There is also code for initiating connection forwarding for X11 connections,
 * arbitrary tcp/ip connections, and the authentication agent connection.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * SSH2 support added by Markus Friedl.
 * Copyright (c) 1999,2000 Markus Friedl.  All rights reserved.
 * Copyright (c) 1999 Dug Song.  All rights reserved.
 * Copyright (c) 1999 Theo de Raadt.  All rights reserved.
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
RCSID("$OpenBSD: channel.c,v 1.1 2001/05/30 12:55:08 markus Exp $");

#include "ssh2.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "misc.h"
#include "channel.h"
#include "compat.h"

/*
 * Pointer to an array containing all allocated channels.  The array is
 * dynamically extended as needed.
 */
Channel **channels = NULL;

/*
 * Size of the channel array.  All slots of the array must always be
 * initialized (at least the type field); unused slots set to NULL
 */
int channels_alloc = 0;

/*
 * Maximum file descriptor value used in any of the channels.  This is
 * updated in channel_new.
 */
int channel_max_fd = 0;


Channel *
channel_lookup(int id)
{
	Channel *c;

	if (id < 0 || id > channels_alloc) {
		log("channel_lookup: %d: bad id", id);
		return NULL;
	}
	c = channels[id];
	if (c == NULL) {
		log("channel_lookup: %d: bad id: channel free", id);
		return NULL;
	}
	return c;
}

/*
 * Register filedescriptors for a channel, used when allocating a channel or
 * when the channel consumer/producer is ready, e.g. shell exec'd
 */

void
channel_register_fds(Channel *c, int rfd, int wfd, int efd,
    int extusage, int nonblock)
{
	/* Update the maximum file descriptor value. */
	channel_max_fd = MAX(channel_max_fd, rfd);
	channel_max_fd = MAX(channel_max_fd, wfd);
	channel_max_fd = MAX(channel_max_fd, efd);

	/* XXX set close-on-exec -markus */

	c->rfd = rfd;
	c->wfd = wfd;
	c->sock = (rfd == wfd) ? rfd : -1;
	c->efd = efd;
	c->extended_usage = extusage;

	/* XXX ugly hack: nonblock is only set by the server */
	if (nonblock && isatty(c->rfd)) {
		debug("channel %d: rfd %d isatty", c->self, c->rfd);
		c->isatty = 1;
		if (!isatty(c->wfd)) {
			error("channel %d: wfd %d is not a tty?",
			    c->self, c->wfd);
		}
	} else {
		c->isatty = 0;
	}

	/* enable nonblocking mode */
	if (nonblock) {
		if (rfd != -1)
			set_nonblock(rfd);
		if (wfd != -1)
			set_nonblock(wfd);
		if (efd != -1)
			set_nonblock(efd);
	}
}

/*
 * Allocate a new channel object and set its type and socket. This will cause
 * remote_name to be freed.
 */

Channel *
channel_new(char *ctype, int type, int rfd, int wfd, int efd,
    int window, int maxpack, int extusage, char *remote_name, int nonblock)
{
	int i, found;
	Channel *c;

	/* Do initial allocation if this is the first call. */
	if (channels_alloc == 0) {
		chan_init();
		channels_alloc = 10;
		channels = xmalloc(channels_alloc * sizeof(Channel *));
		for (i = 0; i < channels_alloc; i++)
			channels[i] = NULL;
		/*
		 * Kludge: arrange a call to channel_stop_listening if we
		 * terminate with fatal().
		 */
		fatal_add_cleanup((void (*) (void *)) channel_stop_listening, NULL);
	}
	/* Try to find a free slot where to put the new channel. */
	for (found = -1, i = 0; i < channels_alloc; i++)
		if (channels[i] == NULL) {
			/* Found a free slot. */
			found = i;
			break;
		}
	if (found == -1) {
		/* There are no free slots.  Take last+1 slot and expand the array.  */
		found = channels_alloc;
		channels_alloc += 10;
		debug2("channel: expanding %d", channels_alloc);
		channels = xrealloc(channels, channels_alloc * sizeof(Channel *));
		for (i = found; i < channels_alloc; i++)
			channels[i] = NULL;
	}
	/* Initialize and return new channel. */
	c = channels[found] = xmalloc(sizeof(Channel));
	buffer_init(&c->input);
	buffer_init(&c->output);
	buffer_init(&c->extended);
	chan_init_iostates(c);
	channel_register_fds(c, rfd, wfd, efd, extusage, nonblock);
	c->self = found;
	c->type = type;
	c->ctype = ctype;
	c->local_window = window;
	c->local_window_max = window;
	c->local_consumed = 0;
	c->local_maxpacket = maxpack;
	c->remote_id = -1;
	c->remote_name = remote_name;
	c->remote_window = 0;
	c->remote_maxpacket = 0;
	c->cb_fn = NULL;
	c->cb_arg = NULL;
	c->cb_event = 0;
	c->dettach_user = NULL;
	c->input_filter = NULL;
	debug("channel %d: new [%s]", found, remote_name);
	return c;
}

/* Close all channel fd/socket. */

void
channel_close_fds(Channel *c)
{
	debug3("channel_close_fds: channel %d: r %d w %d e %d",
	    c->self, c->rfd, c->wfd, c->efd);

	if (c->sock != -1) {
		close(c->sock);
		c->sock = -1;
	}
	if (c->rfd != -1) {
		close(c->rfd);
		c->rfd = -1;
	}
	if (c->wfd != -1) {
		close(c->wfd);
		c->wfd = -1;
	}
	if (c->efd != -1) {
		close(c->efd);
		c->efd = -1;
	}
}

/* Free the channel and close its fd/socket. */

void
channel_free(Channel *c)
{
	char *s;
	int i, n;

	for (n = 0, i = 0; i < channels_alloc; i++)
		if (channels[i])
			n++;
	debug("channel_free: channel %d: %s, nchannels %d", c->self,
	    c->remote_name ? c->remote_name : "???", n);

	s = channel_open_message();
	debug3("channel_free: status: %s", s);
	xfree(s);

	if (c->dettach_user != NULL) {
		debug("channel_free: channel %d: dettaching channel user", c->self);
		c->dettach_user(c->self, NULL);
	}
	if (c->sock != -1)
		shutdown(c->sock, SHUT_RDWR);
	channel_close_fds(c);
	buffer_free(&c->input);
	buffer_free(&c->output);
	buffer_free(&c->extended);
	if (c->remote_name) {
		xfree(c->remote_name);
		c->remote_name = NULL;
	}
	channels[c->self] = NULL;
	xfree(c);
}


/*
 * Stops listening for channels, and removes any unix domain sockets that we
 * might have.
 */

void
channel_stop_listening()
{
	int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c != NULL) {
			switch (c->type) {
			case SSH_CHANNEL_AUTH_SOCKET:
				close(c->sock);
				unlink(c->path);
				channel_free(c);
				break;
			case SSH_CHANNEL_PORT_LISTENER:
			case SSH_CHANNEL_RPORT_LISTENER:
			case SSH_CHANNEL_X11_LISTENER:
				close(c->sock);
				channel_free(c);
				break;
			default:
				break;
			}
		}
	}
}

/*
 * Closes the sockets/fds of all channels.  This is used to close extra file
 * descriptors after a fork.
 */

void
channel_close_all()
{
	int i;

	for (i = 0; i < channels_alloc; i++)
		if (channels[i] != NULL)
			channel_close_fds(channels[i]);
}

/*
 * Returns true if no channel has too much buffered data, and false if one or
 * more channel is overfull.
 */

int
channel_not_very_much_buffered_data()
{
	u_int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c != NULL && c->type == SSH_CHANNEL_OPEN) {
			if (!compat20 && buffer_len(&c->input) > packet_get_maxsize()) {
				debug("channel %d: big input buffer %d",
				    c->self, buffer_len(&c->input));
				return 0;
			}
			if (buffer_len(&c->output) > packet_get_maxsize()) {
				debug("channel %d: big output buffer %d",
				    c->self, buffer_len(&c->output));
				return 0;
			}
		}
	}
	return 1;
}

/* Returns true if any channel is still open. */

int
channel_still_open()
{
	int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
			continue;
		case SSH_CHANNEL_LARVAL:
			if (!compat20)
				fatal("cannot happen: SSH_CHANNEL_LARVAL");
			continue;
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
			return 1;
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: OUT_DRAIN");
			return 1;
		default:
			fatal("channel_still_open: bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	return 0;
}

/* Returns the id of an open channel suitable for keepaliving */

int
channel_find_open()
{
	int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
			return i;
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: OUT_DRAIN");
			return i;
		default:
			fatal("channel_find_open: bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	return -1;
}


/*
 * Returns a message describing the currently open forwarded connections,
 * suitable for sending to the client.  The message contains crlf pairs for
 * newlines.
 */

char *
channel_open_message()
{
	Buffer buffer;
	Channel *c;
	char buf[1024], *cp;
	int i;

	buffer_init(&buffer);
	snprintf(buf, sizeof buf, "The following connections are open:\r\n");
	buffer_append(&buffer, buf, strlen(buf));
	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_ZOMBIE:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			snprintf(buf, sizeof buf, "  #%d %.300s (t%d r%d i%d/%d o%d/%d fd %d/%d)\r\n",
			    c->self, c->remote_name,
			    c->type, c->remote_id,
			    c->istate, buffer_len(&c->input),
			    c->ostate, buffer_len(&c->output),
			    c->rfd, c->wfd);
			buffer_append(&buffer, buf, strlen(buf));
			continue;
		default:
			fatal("channel_open_message: bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	buffer_append(&buffer, "\0", 1);
	cp = xstrdup(buffer_ptr(&buffer));
	buffer_free(&buffer);
	return cp;
}

void
channel_start_open(int id)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_open: %d: bad id", id);
		return;
	}
	debug("send channel open %d", id);
	packet_start(SSH2_MSG_CHANNEL_OPEN);
	packet_put_cstring(c->ctype);
	packet_put_int(c->self);
	packet_put_int(c->local_window);
	packet_put_int(c->local_maxpacket);
}
void
channel_open(int id)
{
	/* XXX REMOVE ME */
	channel_start_open(id);
	packet_send();
}
void
channel_request(int id, char *service, int wantconfirm)
{
	channel_request_start(id, service, wantconfirm);
	packet_send();
	debug("channel request %d: %s", id, service) ;
}
void
channel_request_start(int id, char *service, int wantconfirm)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_request: %d: bad id", id);
		return;
	}
	packet_start(SSH2_MSG_CHANNEL_REQUEST);
	packet_put_int(c->remote_id);
	packet_put_cstring(service);
	packet_put_char(wantconfirm);
}
void
channel_register_callback(int id, int mtype, channel_callback_fn *fn, void *arg)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_register_callback: %d: bad id", id);
		return;
	}
	c->cb_event = mtype;
	c->cb_fn = fn;
	c->cb_arg = arg;
}
void
channel_register_cleanup(int id, channel_callback_fn *fn)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_register_cleanup: %d: bad id", id);
		return;
	}
	c->dettach_user = fn;
}
void
channel_cancel_cleanup(int id)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_cancel_cleanup: %d: bad id", id);
		return;
	}
	c->dettach_user = NULL;
}
void
channel_register_filter(int id, channel_filter_fn *fn)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_register_filter: %d: bad id", id);
		return;
	}
	c->input_filter = fn;
}

void
channel_set_fds(int id, int rfd, int wfd, int efd,
    int extusage, int nonblock)
{
	Channel *c = channel_lookup(id);
	if (c == NULL || c->type != SSH_CHANNEL_LARVAL)
		fatal("channel_activate for non-larval channel %d.", id);
	channel_register_fds(c, rfd, wfd, efd, extusage, nonblock);
	c->type = SSH_CHANNEL_OPEN;
	/* XXX window size? */
	c->local_window = c->local_window_max = c->local_maxpacket * 2;
	packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
	packet_put_int(c->remote_id);
	packet_put_int(c->local_window);
	packet_send();
}
