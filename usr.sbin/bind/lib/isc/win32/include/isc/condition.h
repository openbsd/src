/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: condition.h,v 1.13 2001/01/09 21:58:59 bwelling Exp $ */

#ifndef ISC_CONDITION_H
#define ISC_CONDITION_H 1

#include <windows.h>

#include <isc/lang.h>
#include <isc/mutex.h>
#include <isc/types.h>

typedef struct isc_condition {
	HANDLE 		events[2];
	unsigned int	waiters;
} isc_condition_t;

ISC_LANG_BEGINDECLS

isc_result_t
isc_condition_init(isc_condition_t *);

isc_result_t
isc_condition_wait(isc_condition_t *, isc_mutex_t *);

isc_result_t
isc_condition_signal(isc_condition_t *);

isc_result_t
isc_condition_broadcast(isc_condition_t *);

isc_result_t
isc_condition_destroy(isc_condition_t *);

isc_result_t
isc_condition_waituntil(isc_condition_t *, isc_mutex_t *, isc_time_t *);

ISC_LANG_ENDDECLS

#endif /* ISC_CONDITION_H */
