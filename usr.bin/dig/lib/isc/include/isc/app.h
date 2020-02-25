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

/* $Id: app.h,v 1.8 2020/02/25 13:18:31 jsg Exp $ */

#ifndef ISC_APP_H
#define ISC_APP_H 1

/*****
 ***** Module Info
 *****/

/*! \file isc/app.h
 * \brief ISC Application Support
 *
 * Dealing with program termination can be difficult, especially in a
 * multithreaded program.  The routines in this module help coordinate
 * the shutdown process.  They are used as follows by the initial (main)
 * thread of the application:
 *
 *\li		isc_app_start();	Call very early in main(), before
 *					any other threads have been created.
 *
 *\li		isc_app_run();		This will post any on-run events,
 *					and then block until application
 *					shutdown is requested.  A shutdown
 *					request is made by calling
 *					isc_app_shutdown(), or by sending
 *					SIGINT or SIGTERM to the process.
 *					After isc_app_run() returns, the
 *					application should shutdown itself.
 *
 *\li		isc_app_finish();	Call very late in main().
 *
 * Use of this module is not required.  In particular, isc_app_start() is
 * NOT an ISC library initialization routine.
 *
 * This module also supports per-thread 'application contexts'.  With this
 * mode, a thread-based application will have a separate context, in which
 * it uses other ISC library services such as tasks or timers.  Signals are
 * not caught in this mode, so that the application can handle the signals
 * in its preferred way.
 *
 * \li MP:
 *	Clients must ensure that isc_app_start(), isc_app_run(), and
 *	isc_app_finish() are called at most once.  isc_app_shutdown()
 *	is safe to use by any thread (provided isc_app_start() has been
 *	called previously).
 *
 *	The same note applies to isc_app_ctxXXX() functions, but in this case
 *	it's a per-thread restriction.
 *
 * \li Reliability:
 *	No anticipated impact.
 *
 * \li Resources:
 *	None.
 *
 * \li Security:
 *	No anticipated impact.
 *
 * \li Standards:
 *	None.
 */

#include <isc/eventclass.h>
#include <isc/result.h>

#define ISC_APPEVENT_FIRSTEVENT		(ISC_EVENTCLASS_APP + 0)
#define ISC_APPEVENT_SHUTDOWN		(ISC_EVENTCLASS_APP + 1)
#define ISC_APPEVENT_LASTEVENT		(ISC_EVENTCLASS_APP + 65535)

isc_result_t
isc_app_start(void);
/*!<
 * \brief Start an ISC library application.
 *
 * Notes:
 *	This call should be made before any other ISC library call, and as
 *	close to the beginning of the application as possible.
 *
 * Requires:
 *\li	'ctx' is a valid application context (for app_ctxstart()).
 */

isc_result_t
isc_app_onrun(isc_task_t *task, isc_taskaction_t action,
	      void *arg);
/*!<
 * \brief Request delivery of an event when the application is run.
 *
 * Requires:
 *\li	isc_app_start() has been called.
 *\li	'ctx' is a valid application context (for app_ctxonrun()).
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 */

isc_result_t
isc_app_run(void);
/*!<
 * \brief Run an ISC library application.
 *
 * Notes:
 *\li	The caller (typically the initial thread of an application) will
 *	block until shutdown is requested.  When the call returns, the
 *	caller should start shutting down the application.
 *
 * Requires:
 *\li	isc_app_[ctx]start() has been called.
 *
 * Ensures:
 *\li	Any events requested via isc_app_onrun() will have been posted (in
 *	FIFO order) before isc_app_run() blocks.
 *\li	'ctx' is a valid application context (for app_ctxrun()).
 *
 * Returns:
 *\li	ISC_R_SUCCESS			Shutdown has been requested.
 */

isc_result_t
isc_app_shutdown(void);
/*!<
 * \brief Request application shutdown.
 *
 * Notes:
 *\li	It is safe to call isc_app_shutdown() multiple times.  Shutdown will
 *	only be triggered once.
 *
 * Requires:
 *\li	isc_app_[ctx]run() has been called.
 *\li	'ctx' is a valid application context (for app_ctxshutdown()).
 *
 * Returns:
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_UNEXPECTED
 */

#endif /* ISC_APP_H */
