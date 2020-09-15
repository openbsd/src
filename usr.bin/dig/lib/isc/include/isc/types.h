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

/* $Id: types.h,v 1.7 2020/09/15 11:47:42 florian Exp $ */

#ifndef ISC_TYPES_H
#define ISC_TYPES_H 1

/*! \file isc/types.h
 * \brief
 * OS-specific types, from the OS-specific include directories.
 */

/*
 * XXXDCL This is just for ISC_LIST and ISC_LINK, but gets all of the other
 * list macros too.
 */
#include <isc/list.h>

/* Core Types.  Alphabetized by defined type. */

typedef struct isc_buffer		isc_buffer_t;		/*%< Buffer */
typedef ISC_LIST(isc_buffer_t)		isc_bufferlist_t;	/*%< Buffer List */
typedef struct isc_event		isc_event_t;		/*%< Event */
typedef ISC_LIST(isc_event_t)		isc_eventlist_t;	/*%< Event List */
typedef unsigned int			isc_eventtype_t;	/*%< Event Type */
typedef struct isc_lex			isc_lex_t;		/*%< Lex */
typedef struct isc_log 			isc_log_t;		/*%< Log */
typedef struct isc_logcategory		isc_logcategory_t;	/*%< Log Category */
typedef struct isc_logconfig		isc_logconfig_t;	/*%< Log Configuration */
typedef struct isc_logmodule		isc_logmodule_t;	/*%< Log Module */
typedef struct isc_region		isc_region_t;		/*%< Region */
typedef unsigned int			isc_result_t;		/*%< Result */
typedef struct isc_socket		isc_socket_t;		/*%< Socket */
typedef struct isc_socketevent		isc_socketevent_t;	/*%< Socket Event */
typedef struct isc_socketmgr		isc_socketmgr_t;	/*%< Socket Manager */
typedef struct isc_symtab		isc_symtab_t;		/*%< Symbol Table */
typedef struct isc_task			isc_task_t;		/*%< Task */
typedef struct isc_taskmgr		isc_taskmgr_t;		/*%< Task Manager */
typedef struct isc_textregion		isc_textregion_t;	/*%< Text Region */
typedef struct isc_timer		isc_timer_t;		/*%< Timer */
typedef struct isc_timermgr		isc_timermgr_t;		/*%< Timer Manager */

typedef void (*isc_taskaction_t)(isc_task_t *, isc_event_t *);

#endif /* ISC_TYPES_H */
