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

/* $ISC: thread.c,v 1.17 2001/07/09 21:06:21 gson Exp $ */

#include <config.h>

#include <process.h>

#include <isc/thread.h>

isc_result_t
isc_thread_create(isc_threadfunc_t start, isc_threadarg_t arg,
		  isc_thread_t *threadp)
{
	isc_thread_t thread;
	unsigned int id;

	thread = (isc_thread_t)_beginthreadex(NULL, 0, start, arg, 0, &id);
	if (thread == NULL) {
		/* XXX */
		return (ISC_R_UNEXPECTED);
	}

	*threadp = thread;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_thread_join(isc_thread_t thread, isc_threadresult_t *rp) {
	DWORD result;

	result = WaitForSingleObject(thread, INFINITE);
	if (result != WAIT_OBJECT_0) {
		/* XXX */
		return (ISC_R_UNEXPECTED);
	}
	if (rp != NULL && !GetExitCodeThread(thread, rp)) {
		/* XXX */
		return (ISC_R_UNEXPECTED);
	}
	(void)CloseHandle(thread);

	return (ISC_R_SUCCESS);
}

void
isc_thread_setconcurrency(unsigned int level) {
	/*
	 * This is unnecessary on Win32 systems, but is here so that the
	 * call exists
	 */
}
