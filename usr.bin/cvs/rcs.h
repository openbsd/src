/*	$OpenBSD: rcs.h,v 1.80 2007/10/09 12:59:53 tobias Exp $	*/
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
#include "buf.h"

#define RCS_DIFF_MAXARG		32
#define RCS_DIFF_DIV \
	"==================================================================="

#define RCSDIR			"RCS"
#define RCS_FILE_EXT		",v"

#define RCS_HEAD_BRANCH		"HEAD"
#define RCS_HEAD_INIT		"1.1"
#define RCS_HEAD_REV		((RCSNUM *)(-1))

#define RCS_CONFLICT_MARKER1	"<<<<<<< "
#define RCS_CONFLICT_MARKER2	">>>>>>> "
#define RCS_CONFLICT_MARKER3	"=======\n"

#define RCS_SYM_INVALCHAR	"$,.:;@"


#define RCS_MAGIC_BRANCH	".0."
#define RCS_STATE_EXP		"Exp"
#define RCS_STATE_DEAD		"dead"

/* lock types */
#define RCS_LOCK_INVAL		(-1)
#define RCS_LOCK_LOOSE		0
#define RCS_LOCK_STRICT		1


/*
 * Keyword expansion table
 */
#define RCS_KW_AUTHOR		0x1000
#define RCS_KW_DATE		0x2000
#define RCS_KW_LOG		0x4000
#define RCS_KW_NAME		0x8000
#define RCS_KW_RCSFILE		0x0100
#define RCS_KW_REVISION		0x0200
#define RCS_KW_SOURCE		0x0400
#define RCS_KW_STATE		0x0800
#define RCS_KW_FULLPATH		0x0010
#define RCS_KW_MDOCDATE		0x0020

#define RCS_KW_ID \
	(RCS_KW_RCSFILE | RCS_KW_REVISION | RCS_KW_DATE \
	| RCS_KW_AUTHOR | RCS_KW_STATE)

#define RCS_KW_HEADER	(RCS_KW_ID | RCS_KW_FULLPATH)

/* RCS keyword expansion modes (kflags) */
#define RCS_KWEXP_NONE	0x00
#define RCS_KWEXP_NAME	0x01	/* include keyword name */
#define RCS_KWEXP_VAL	0x02	/* include keyword value */
#define RCS_KWEXP_LKR	0x04	/* include name of locker */
#define RCS_KWEXP_OLD	0x08	/* generate old keyword string */
#define RCS_KWEXP_ERR	0x10	/* mode has an error */

#define RCS_KWEXP_DEFAULT	(RCS_KWEXP_NAME | RCS_KWEXP_VAL)
#define RCS_KWEXP_KVL		(RCS_KWEXP_NAME | RCS_KWEXP_VAL | RCS_KWEXP_LKR)

#define RCS_KWEXP_INVAL(k) \
	((k & RCS_KWEXP_ERR) || \
	((k & RCS_KWEXP_OLD) && (k & ~RCS_KWEXP_OLD)))


struct rcs_kw {
	char	kw_str[16];
	int	kw_type;
};

#define RCS_NKWORDS	(sizeof(rcs_expkw)/sizeof(rcs_expkw[0]))

#define RCSNUM_MAXNUM	USHRT_MAX
#define RCSNUM_MAXLEN	64

#define RCSNUM_ISBRANCH(n)	((n)->rn_len % 2)
#define RCSNUM_ISBRANCHREV(n)	(!((n)->rn_len % 2) && ((n)->rn_len >= 4))
#define RCSNUM_NO_MAGIC		(1<<0)

/* file flags */
#define RCS_READ	  (1<<0)
#define RCS_WRITE	  (1<<1)
#define RCS_RDWR	  (RCS_READ|RCS_WRITE)
#define RCS_CREATE	  (1<<2)  /* create the file */
#define RCS_PARSE_FULLY   (1<<3)  /* fully parse it on open */

/* internal flags */
#define RCS_PARSED	  (1<<4)  /* file has been parsed */
#define RCS_SYNCED	  (1<<5)  /* in-mem copy is sync with disk copy */
#define RCS_SLOCK	  (1<<6)  /* strict lock */

/* parser flags */
#define PARSED_DELTAS     (1<<7)  /* all deltas are parsed */
#define PARSED_DESC       (1<<8)  /* the description is parsed */
#define PARSED_DELTATEXTS (1<<9)  /* all delta texts are parsed */

/* delta flags */
#define RCS_RD_DEAD	0x01	/* dead */
#define RCS_RD_SELECT	0x02	/* select for operation */

/* RCS error codes */
#define RCS_ERR_NOERR	0
#define RCS_ERR_NOENT	1
#define RCS_ERR_DUPENT	2
#define RCS_ERR_BADNUM	3
#define RCS_ERR_BADSYM	4
#define RCS_ERR_PARSE	5
#define RCS_ERR_ERRNO	255

/* used for rcs_checkout_rev */
#define CHECKOUT_REV_CREATED	1
#define CHECKOUT_REV_MERGED	2
#define CHECKOUT_REV_REMOVED	3
#define CHECKOUT_REV_UPDATED	4

typedef struct rcs_num {
	u_int		 rn_len;
	u_int16_t	*rn_id;
} RCSNUM;


struct rcs_access {
	char			*ra_name;
	uid_t			 ra_uid;
	TAILQ_ENTRY(rcs_access)	 ra_list;
};

struct rcs_sym {
	char			*rs_name;
	RCSNUM			*rs_num;
	TAILQ_ENTRY(rcs_sym)	 rs_list;
};

struct rcs_lock {
	char	*rl_name;
	RCSNUM	*rl_num;

	TAILQ_ENTRY(rcs_lock)	rl_list;
};


struct rcs_branch {
	RCSNUM			*rb_num;
	TAILQ_ENTRY(rcs_branch)	rb_list;
};

TAILQ_HEAD(rcs_dlist, rcs_delta);

struct rcs_delta {
	RCSNUM		*rd_num;
	RCSNUM		*rd_next;
	int		 rd_flags;
	struct tm	 rd_date;
	char		*rd_author;
	char		*rd_state;
	char		*rd_log;
	char		*rd_locker;
	u_char		*rd_text;
	size_t		 rd_tlen;

	TAILQ_HEAD(, rcs_branch)	rd_branches;
	TAILQ_ENTRY(rcs_delta)		rd_list;
};


typedef struct rcs_file {
	int	fd;
	int	 rf_dead;
	char	*rf_path;
	mode_t	 rf_mode;
	int	 rf_flags;

	RCSNUM	*rf_head;
	RCSNUM	*rf_branch;
	char	*rf_comment;
	char	*rf_expand;
	char	*rf_desc;

	u_int					rf_ndelta;
	struct rcs_dlist			rf_delta;
	TAILQ_HEAD(rcs_alist, rcs_access)	rf_access;
	TAILQ_HEAD(rcs_slist, rcs_sym)		rf_symbols;
	TAILQ_HEAD(rcs_llist, rcs_lock)		rf_locks;

	void	*rf_pdata;
} RCSFILE;

extern int rcs_errno;
struct cvs_line;
struct cvs_lines;

RCSFILE			*rcs_open(const char *, int, int, ...);
void			 rcs_close(RCSFILE *);
RCSNUM			*rcs_head_get(RCSFILE *);
int			 rcs_head_set(RCSFILE *, RCSNUM *);
const RCSNUM		*rcs_branch_get(RCSFILE *);
int			 rcs_branch_set(RCSFILE *, const RCSNUM *);
int			 rcs_access_add(RCSFILE *, const char *);
int			 rcs_access_remove(RCSFILE *, const char *);
int			 rcs_access_check(RCSFILE *, const char *);
struct rcs_delta	*rcs_findrev(RCSFILE *, RCSNUM *);
int			 rcs_sym_add(RCSFILE *, const char *, RCSNUM *);
int			 rcs_sym_check(const char *);
struct rcs_sym		*rcs_sym_get(RCSFILE *, const char *);
int			 rcs_sym_remove(RCSFILE *, const char *);
RCSNUM			*rcs_sym_getrev(RCSFILE *, const char *);
RCSNUM			*rcs_translate_tag(const char *, RCSFILE *);
int			 rcs_lock_getmode(RCSFILE *);
int			 rcs_lock_setmode(RCSFILE *, int);
int			 rcs_lock_add(RCSFILE *, const char *, RCSNUM *);
int			 rcs_lock_remove(RCSFILE *, const char *, RCSNUM *);
BUF			*rcs_getrev(RCSFILE *, RCSNUM *);
int			 rcs_deltatext_set(RCSFILE *, RCSNUM *, BUF *);
const char		*rcs_desc_get(RCSFILE *);
void			 rcs_desc_set(RCSFILE *, const char *);
const char		*rcs_comment_lookup(const char *);
const char		*rcs_comment_get(RCSFILE *);
void			 rcs_comment_set(RCSFILE *, const char *);
BUF			*rcs_kwexp_buf(BUF *, RCSFILE *, RCSNUM *);
void			 rcs_kwexp_set(RCSFILE *, int);
int			 rcs_kwexp_get(RCSFILE *);
int			 rcs_rev_add(RCSFILE *, RCSNUM *, const char *, time_t,
			     const char *);
time_t			 rcs_rev_getdate(RCSFILE *, RCSNUM *);
int			 rcs_rev_setlog(RCSFILE *, RCSNUM *, const char *);
int			 rcs_rev_remove(RCSFILE *, RCSNUM *);
int			 rcs_state_set(RCSFILE *, RCSNUM *, const char *);
const char		*rcs_state_get(RCSFILE *, RCSNUM *);
int			 rcs_state_check(const char *);
RCSNUM			*rcs_tag_resolve(RCSFILE *, const char *);
const char		*rcs_errstr(int);
void			 rcs_write(RCSFILE *);
void			 rcs_rev_write_stmp(RCSFILE *,  RCSNUM *, char *, int);
void			 rcs_rev_write_fd(RCSFILE *, RCSNUM *, int, int);
struct cvs_lines	*rcs_rev_getlines(RCSFILE *, RCSNUM *,
			     struct cvs_line ***);
void			 rcs_annotate_getlines(RCSFILE *, RCSNUM *,
			     struct cvs_line ***);
BUF			*rcs_rev_getbuf(RCSFILE *, RCSNUM *, int);

int	rcs_kflag_get(const char *);
void	rcs_kflag_usage(void);
int	rcs_kw_expand(RCSFILE *, u_char *, size_t, size_t *);

RCSNUM	*rcsnum_alloc(void);
RCSNUM	*rcsnum_parse(const char *);
RCSNUM	*rcsnum_brtorev(const RCSNUM *);
RCSNUM	*rcsnum_revtobr(const RCSNUM *);
RCSNUM	*rcsnum_inc(RCSNUM *);
RCSNUM	*rcsnum_dec(RCSNUM *);
RCSNUM	*rcsnum_branch_root(RCSNUM *);
void	 rcsnum_free(RCSNUM *);
int	 rcsnum_aton(const char *, char **, RCSNUM *);
char	*rcsnum_tostr(const RCSNUM *, char *, size_t);
void	 rcsnum_cpy(const RCSNUM *, RCSNUM *, u_int);
int	 rcsnum_cmp(RCSNUM *, RCSNUM *, u_int);
int	 rcsnum_differ(RCSNUM *, RCSNUM *);

extern int rcsnum_flags;

#endif	/* RCS_H */
