/*	$OpenBSD: event.h,v 1.3 2004/12/07 17:10:56 tedu Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EVENT_H
#define EVENT_H

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <pwd.h>


#define CVS_EVENT_ADD         1
#define CVS_EVENT_ANNOTATE    2
#define CVS_EVENT_CHECKOUT    3
#define CVS_EVENT_COMMIT      4
#define CVS_EVENT_DIFF        5
#define CVS_EVENT_HISTORY     6
#define CVS_EVENT_IMPORT      7
#define CVS_EVENT_REMOVE      8
#define CVS_EVENT_UPDATE      9
#define CVS_EVENT_TAG        10


struct cvs_event {
	u_int          ev_type;
	uid_t          ev_user;
	struct timeval ev_time;

	char          *ev_branch;

	LIST_HEAD(, cvs_file) ev_files;   /* files affected by this event */
};

#endif /* EVENT_H */
