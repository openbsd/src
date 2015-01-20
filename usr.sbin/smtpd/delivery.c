/*	$OpenBSD: delivery.c,v 1.6 2015/01/20 17:37:54 deraadt Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"

extern struct delivery_backend delivery_backend_mbox;
extern struct delivery_backend delivery_backend_mda;
extern struct delivery_backend delivery_backend_maildir;
extern struct delivery_backend delivery_backend_filename;
extern struct delivery_backend delivery_backend_lmtp;

struct delivery_backend *
delivery_backend_lookup(enum action_type type)
{
	switch (type) {
	case A_MBOX:
		return &delivery_backend_mbox;
	case A_MDA:
		return &delivery_backend_mda;
	case A_MAILDIR:
		return &delivery_backend_maildir;
	case A_FILENAME:
		return &delivery_backend_filename;
	case A_LMTP:
		return &delivery_backend_lmtp;
	default:
		break;
	}
	return NULL;
}
