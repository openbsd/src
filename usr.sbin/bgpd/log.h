/*	$OpenBSD: log.h,v 1.2 2004/05/21 11:58:32 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

static const char *statenames[] = {
	"None",
	"Idle",
	"Connect",
	"Active",
	"OpenSent",
	"OpenConfirm",
	"Established"
};

static const char *eventnames[] = {
	"None",
	"Start",
	"Stop",
	"Connection opened",
	"Connection closed",
	"Connection open failed",
	"Fatal error",
	"ConnectRetryTimer expired",
	"HoldTimer expired",
	"KeepaliveTimer expired",
	"OPEN message received",
	"KEEPALIVE message received",
	"UPDATE message received",
	"NOTIFICATION received"
};

static const char *errnames[] = {
	"none",
	"Header error",
	"error in OPEN message",
	"error in UPDATE message",
	"HoldTimer expired",
	"Finite State Machine error",
	"Cease"
};

static const char *suberr_header_names[] = {
	"none",
	"synchronization error",
	"wrong length",
	"unknown message type"
};

static const char *suberr_open_names[] = {
	"none",
	"version mismatch",
	"AS unacceptable",
	"BGPID invalid",
	"optional parameter error",
	"Authentication error",
	"unacceptable holdtime",
	"unsupported capability"
};

static const char *suberr_update_names[] = {
	"none",
	"attribute list error",
	"unknown well-known attribute",
	"well-known attribute missing",
	"attribute flags error",
	"attribute length wrong",
	"origin unacceptable",
	"loop detected",
	"nexthop unacceptable",
	"optional attribute error",
	"network unacceptable",
	"AS-Path unacceptable"
};

static const char *procnames[] = {
	"parent",
	"SE",
	"RDE"
};
