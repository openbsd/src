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
RCSID("$OpenBSD: channel-input.c,v 1.1 2001/05/30 12:55:07 markus Exp $");

#include "ssh1.h"
#include "ssh2.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "channel.h"
#include "compat.h"


void
channel_input_data(int type, int plen, void *ctxt)
{
	int id;
	char *data;
	u_int data_len;
	Channel *c;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received data for nonexistent channel %d.", id);

	/* Ignore any data for non-open channels (might happen on close) */
	if (c->type != SSH_CHANNEL_OPEN &&
	    c->type != SSH_CHANNEL_X11_OPEN)
		return;

	/* same for protocol 1.5 if output end is no longer open */
	if (!compat13 && c->ostate != CHAN_OUTPUT_OPEN)
		return;

	/* Get the data. */
	data = packet_get_string(&data_len);
	packet_done();

	if (compat20){
		if (data_len > c->local_maxpacket) {
			log("channel %d: rcvd big packet %d, maxpack %d",
			    c->self, data_len, c->local_maxpacket);
		}
		if (data_len > c->local_window) {
			log("channel %d: rcvd too much data %d, win %d",
			    c->self, data_len, c->local_window);
			xfree(data);
			return;
		}
		c->local_window -= data_len;
	}else{
		packet_integrity_check(plen, 4 + 4 + data_len, type);
	}
	buffer_append(&c->output, data, data_len);
	xfree(data);
}

void
channel_input_extended_data(int type, int plen, void *ctxt)
{
	int id;
	int tcode;
	char *data;
	u_int data_len;
	Channel *c;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	c = channel_lookup(id);

	if (c == NULL)
		packet_disconnect("Received extended_data for bad channel %d.", id);
	if (c->type != SSH_CHANNEL_OPEN) {
		log("channel %d: ext data for non open", id);
		return;
	}
	tcode = packet_get_int();
	if (c->efd == -1 ||
	    c->extended_usage != CHAN_EXTENDED_WRITE ||
	    tcode != SSH2_EXTENDED_DATA_STDERR) {
		log("channel %d: bad ext data", c->self);
		return;
	}
	data = packet_get_string(&data_len);
	packet_done();
	if (data_len > c->local_window) {
		log("channel %d: rcvd too much extended_data %d, win %d",
		    c->self, data_len, c->local_window);
		xfree(data);
		return;
	}
	debug2("channel %d: rcvd ext data %d", c->self, data_len);
	c->local_window -= data_len;
	buffer_append(&c->extended, data, data_len);
	xfree(data);
}

void
channel_input_ieof(int type, int plen, void *ctxt)
{
	int id;
	Channel *c;

	packet_integrity_check(plen, 4, type);

	id = packet_get_int();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received ieof for nonexistent channel %d.", id);
	chan_rcvd_ieof(c);
}

void
channel_input_close(int type, int plen, void *ctxt)
{
	int id;
	Channel *c;

	packet_integrity_check(plen, 4, type);

	id = packet_get_int();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received close for nonexistent channel %d.", id);

	/*
	 * Send a confirmation that we have closed the channel and no more
	 * data is coming for it.
	 */
	packet_start(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION);
	packet_put_int(c->remote_id);
	packet_send();

	/*
	 * If the channel is in closed state, we have sent a close request,
	 * and the other side will eventually respond with a confirmation.
	 * Thus, we cannot free the channel here, because then there would be
	 * no-one to receive the confirmation.  The channel gets freed when
	 * the confirmation arrives.
	 */
	if (c->type != SSH_CHANNEL_CLOSED) {
		/*
		 * Not a closed channel - mark it as draining, which will
		 * cause it to be freed later.
		 */
		buffer_consume(&c->input, buffer_len(&c->input));
		c->type = SSH_CHANNEL_OUTPUT_DRAINING;
	}
}

/* proto version 1.5 overloads CLOSE_CONFIRMATION with OCLOSE */
void
channel_input_oclose(int type, int plen, void *ctxt)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);
	packet_integrity_check(plen, 4, type);
	if (c == NULL)
		packet_disconnect("Received oclose for nonexistent channel %d.", id);
	chan_rcvd_oclose(c);
}

void
channel_input_close_confirmation(int type, int plen, void *ctxt)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);

	packet_done();
	if (c == NULL)
		packet_disconnect("Received close confirmation for "
		    "out-of-range channel %d.", id);
	if (c->type != SSH_CHANNEL_CLOSED)
		packet_disconnect("Received close confirmation for "
		    "non-closed channel %d (type %d).", id, c->type);
	channel_free(c);
}

void
channel_input_open_confirmation(int type, int plen, void *ctxt)
{
	int id, remote_id;
	Channel *c;

	if (!compat20)
		packet_integrity_check(plen, 4 + 4, type);

	id = packet_get_int();
	c = channel_lookup(id);

	if (c==NULL || c->type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open confirmation for "
		    "non-opening channel %d.", id);
	remote_id = packet_get_int();
	/* Record the remote channel number and mark that the channel is now open. */
	c->remote_id = remote_id;
	c->type = SSH_CHANNEL_OPEN;

	if (compat20) {
		c->remote_window = packet_get_int();
		c->remote_maxpacket = packet_get_int();
		packet_done();
		if (c->cb_fn != NULL && c->cb_event == type) {
			debug2("callback start");
			c->cb_fn(c->self, c->cb_arg);
			debug2("callback done");
		}
		debug("channel %d: open confirm rwindow %d rmax %d", c->self,
		    c->remote_window, c->remote_maxpacket);
	}
}

char *
reason2txt(int reason)
{
	switch(reason) {
	case SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED:
		return "administratively prohibited";
	case SSH2_OPEN_CONNECT_FAILED:
		return "connect failed";
	case SSH2_OPEN_UNKNOWN_CHANNEL_TYPE:
		return "unknown channel type";
	case SSH2_OPEN_RESOURCE_SHORTAGE:
		return "resource shortage";
	}
	return "unknown reason";
}

void
channel_input_open_failure(int type, int plen, void *ctxt)
{
	int id, reason;
	char *msg = NULL, *lang = NULL;
	Channel *c;

	if (!compat20)
		packet_integrity_check(plen, 4, type);

	id = packet_get_int();
	c = channel_lookup(id);

	if (c==NULL || c->type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open failure for "
		    "non-opening channel %d.", id);
	if (compat20) {
		reason = packet_get_int();
		if (!(datafellows & SSH_BUG_OPENFAILURE)) {
			msg  = packet_get_string(NULL);
			lang = packet_get_string(NULL);
		}
		packet_done();
		log("channel %d: open failed: %s%s%s", id,
		    reason2txt(reason), msg ? ": ": "", msg ? msg : "");
		if (msg != NULL)
			xfree(msg);
		if (lang != NULL)
			xfree(lang);
	}
	/* Free the channel.  This will also close the socket. */
	channel_free(c);
}

void
channel_input_channel_request(int type, int plen, void *ctxt)
{
	int id;
	Channel *c;

	id = packet_get_int();
	c = channel_lookup(id);

	if (c == NULL ||
	    (c->type != SSH_CHANNEL_OPEN && c->type != SSH_CHANNEL_LARVAL))
		packet_disconnect("Received request for "
		    "non-open channel %d.", id);
	if (c->cb_fn != NULL && c->cb_event == type) {
		debug2("callback start");
		c->cb_fn(c->self, c->cb_arg);
		debug2("callback done");
	} else {
		char *service = packet_get_string(NULL);
		debug("channel %d: rcvd request for %s", c->self, service);
		debug("cb_fn %p cb_event %d", c->cb_fn , c->cb_event);
		xfree(service);
	}
}

void
channel_input_window_adjust(int type, int plen, void *ctxt)
{
	Channel *c;
	int id, adjust;

	if (!compat20)
		return;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	c = channel_lookup(id);

	if (c == NULL || c->type != SSH_CHANNEL_OPEN) {
		log("Received window adjust for "
		    "non-open channel %d.", id);
		return;
	}
	adjust = packet_get_int();
	packet_done();
	debug2("channel %d: rcvd adjust %d", id, adjust);
	c->remote_window += adjust;
}

void
channel_input_port_open(int type, int plen, void *ctxt)
{
	Channel *c = NULL;
	u_short host_port;
	char *host, *originator_string;
	int remote_id, sock = -1;

	remote_id = packet_get_int();
	host = packet_get_string(NULL);
	host_port = packet_get_int();

	if (packet_get_protocol_flags() & SSH_PROTOFLAG_HOST_IN_FWD_OPEN) {
		originator_string = packet_get_string(NULL);
	} else {
		originator_string = xstrdup("unknown (remote did not supply name)");
	}
	packet_done();
	sock = channel_connect_to(host, host_port);
	if (sock != -1) {
		c = channel_new("connected socket",
		    SSH_CHANNEL_CONNECTING, sock, sock, -1, 0, 0, 0,
		    originator_string, 1);
		if (c == NULL) {
			error("channel_input_port_open: channel_new failed");
			close(sock);
		} else {
			c->remote_id = remote_id;
		}
	}
	if (c == NULL) {
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
		packet_send();
	}
	xfree(host);
}
