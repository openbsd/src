/*	$OpenBSD: rcs.h,v 1.4 2004/12/16 17:16:18 jfb Exp $	*/
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

#ifndef RCS_H
#define RCS_H

#include <sys/types.h>
#include <sys/queue.h>

#include <time.h>

#include "buf.h"

#define RCS_DIFF_MAXARG   32
#define RCS_DIFF_DIV \
	"==================================================================="

#define RCS_FILE_EXT   ",v"

#define RCS_HEAD_INIT  "1.1"


/* open modes */
#define RCS_MODE_READ   0x01
#define RCS_MODE_WRITE  0x02
#define RCS_MODE_RDWR   (RCS_MODE_READ|RCS_MODE_WRITE)


/* file flags */
#define RCS_RF_PARSED  0x01   /* file has been parsed */
#define RCS_RF_SYNCED  0x02   /* in-memory copy is in sync with disk copy */
#define RCS_RF_SLOCK   0x04   /* strict lock */

/* delta flags */
#define RCS_RD_DEAD   0x01     /* dead */


typedef struct rcs_num {
	u_int      rn_len;
	u_int16_t *rn_id;
} RCSNUM;


struct rcs_sym {
	char    *rs_name;
	RCSNUM  *rs_num;
	TAILQ_ENTRY(rcs_sym) rs_list;
};

struct rcs_lock {
	RCSNUM   *rl_num;

	TAILQ_ENTRY(rcs_lock) rl_list;
};


struct rcs_branch {
	RCSNUM  *rb_num;
	TAILQ_ENTRY(rcs_branch) rb_list;
};

struct rcs_dlist {
	struct rcs_delta *tqh_first;
	struct rcs_delta **tqh_last;
};

struct rcs_delta {
	RCSNUM      *rd_num;
	RCSNUM      *rd_next;
	u_int        rd_flags;
	struct tm    rd_date;
	char        *rd_author;
	char        *rd_state;
	char        *rd_log;
	char        *rd_text;

	struct rcs_dlist rd_snodes;

	TAILQ_HEAD(, rcs_branch) rd_branches;
	TAILQ_ENTRY(rcs_delta)  rd_list;
};


typedef struct rcs_file {
	char   *rf_path;
	u_int   rf_ref;
	u_int   rf_mode;
	u_int   rf_flags;

	RCSNUM *rf_head;
	RCSNUM *rf_branch;
	char   *rf_comment;
	char   *rf_expand;
	char   *rf_desc;

	struct rcs_dlist rf_delta;
	TAILQ_HEAD(rcs_slist, rcs_sym)   rf_symbols;
	TAILQ_HEAD(rcs_llist, rcs_lock)  rf_locks;

	void   *rf_pdata;
} RCSFILE;


RCSFILE*  rcs_open         (const char *, u_int);
void      rcs_close        (RCSFILE *);
int       rcs_parse        (RCSFILE *);
int       rcs_write        (RCSFILE *);
int       rcs_addsym       (RCSFILE *, const char *, RCSNUM *);
int       rcs_rmsym        (RCSFILE *, const char *);
BUF*      rcs_getrev       (RCSFILE *, RCSNUM *);
BUF*      rcs_gethead      (RCSFILE *);
RCSNUM*   rcs_getrevbydate (RCSFILE *, struct tm *);

BUF*      rcs_patch     (const char *, const char *);
size_t    rcs_stresc    (int, const char *, char *, size_t *);

RCSNUM*   rcsnum_alloc  (void);
void      rcsnum_free   (RCSNUM *);
int       rcsnum_aton   (const char *, char **, RCSNUM *);
char*     rcsnum_tostr  (const RCSNUM *, char *, size_t);
int       rcsnum_cpy    (const RCSNUM *, RCSNUM *, u_int);
int       rcsnum_cmp    (const RCSNUM *, const RCSNUM *, u_int);

/* from cache.c */
int       rcs_cache_init    (u_int);
RCSFILE  *rcs_cache_fetch   (const char *path);
int       rcs_cache_store   (RCSFILE *);
void      rcs_cache_destroy (void);

#endif /* RCS_H */
