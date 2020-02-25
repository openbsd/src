/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: event.c,v 1.3 2020/02/25 05:00:43 jsg Exp $ */

/*!
 * \file
 * \author Principal Author: Bob Halley
 */

#include <stdlib.h>
#include <isc/event.h>

#include <isc/util.h>

/***
 *** Events.
 ***/

static void
destroy(isc_event_t *event) {
	free(event);
}

isc_event_t *
isc_event_allocate(void *sender, isc_eventtype_t type,
		   isc_taskaction_t action, void *arg, size_t size)
{
	isc_event_t *event;

	REQUIRE(size >= sizeof(struct isc_event));
	REQUIRE(action != NULL);

	event = malloc(size);
	if (event == NULL)
		return (NULL);

	ISC_EVENT_INIT(event, size, 0, NULL, type, action, arg,
		       sender, destroy);

	return (event);
}

void
isc_event_free(isc_event_t **eventp) {
	isc_event_t *event;

	REQUIRE(eventp != NULL);
	event = *eventp;
	REQUIRE(event != NULL);

	REQUIRE(!ISC_LINK_LINKED(event, ev_link));
	REQUIRE(!ISC_LINK_LINKED(event, ev_ratelink));

	if (event->ev_destroy != NULL)
		(event->ev_destroy)(event);

	*eventp = NULL;
}
