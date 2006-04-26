/*	$OpenBSD: worklist.h,v 1.1 2006/04/26 02:55:13 joris Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#ifndef WORKLIST_H
#define WORKLIST_H

struct rcs_worklist {
	char					wkl_path[MAXPATHLEN];
	volatile SLIST_ENTRY(rcs_worklist)	wkl_list;
};

SLIST_HEAD(rcs_wklhead, rcs_worklist);

void rcs_worklist_add(const char *, struct rcs_wklhead *);
void rcs_worklist_run(struct rcs_wklhead *, void (*cb)(struct rcs_worklist *));
void rcs_worklist_clean(struct rcs_wklhead *, void (*cb)(struct rcs_worklist *));

void rcs_worklist_unlink(struct rcs_worklist *);

extern struct rcs_wklhead rcs_temp_files;

#endif
