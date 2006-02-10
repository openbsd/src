/*	$OpenBSD: cvs.h,v 1.101 2006/02/10 10:15:48 xsa Exp $	*/
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

#ifndef CVS_H
#define CVS_H

#include "rcs.h"
#include "file.h"
#include "xmalloc.h"

#define CVS_VERSION	"OpenCVS 0.3"

#define CVS_HIST_CACHE	128
#define CVS_HIST_NBFLD	6


#define CVS_CKSUM_LEN	33	/* length of a CVS checksum string */

/* error codes */
#define CVS_EX_ERR	-1
#define CVS_EX_OK	0
#define CVS_EX_USAGE	1
#define CVS_EX_DATA	2
#define CVS_EX_PROTO	3
#define CVS_EX_FILE	4
#define CVS_EX_BADROOT	5

/* operations */
#define CVS_OP_UNKNOWN		0
#define CVS_OP_ADD		1
#define CVS_OP_ADMIN		2
#define CVS_OP_ANNOTATE		3
#define CVS_OP_CHECKOUT		4
#define CVS_OP_COMMIT		5
#define CVS_OP_DIFF		6
#define CVS_OP_EDIT		7
#define CVS_OP_EDITORS		8
#define CVS_OP_EXPORT		9
#define CVS_OP_HISTORY		10
#define CVS_OP_IMPORT		11
#define CVS_OP_INIT		12
#define CVS_OP_LOG		13
#define CVS_OP_RANNOTATE	14
#define CVS_OP_RDIFF		15
#define CVS_OP_RELEASE		16
#define CVS_OP_REMOVE		17
#define CVS_OP_RLOG		18
#define CVS_OP_RTAG		19
#define CVS_OP_SERVER		20
#define CVS_OP_STATUS		21
#define CVS_OP_TAG		22
#define CVS_OP_UNEDIT		23
#define CVS_OP_UPDATE		24
#define CVS_OP_VERSION		25
#define CVS_OP_WATCH		26
#define CVS_OP_WATCHERS		27

#define CVS_OP_ANY		64	/* all operations */


/* methods */
#define CVS_METHOD_NONE		0
#define CVS_METHOD_LOCAL	1	/* local access */
#define CVS_METHOD_SERVER	2	/* tunnel through CVS_RSH */
#define CVS_METHOD_PSERVER	3	/* cvs pserver */
#define CVS_METHOD_KSERVER	4	/* kerberos */
#define CVS_METHOD_GSERVER	5	/* gssapi server */
#define CVS_METHOD_EXT		6
#define CVS_METHOD_FORK		7	/* local but fork */

#define CVS_CMD_MAXNAMELEN	16
#define CVS_CMD_MAXALIAS	2
#define CVS_CMD_MAXDESCRLEN	64
#define CVS_CMD_MAXARG		128


/* defaults */
#define CVS_SERVER_DEFAULT	"cvs"
#define CVS_RSH_DEFAULT		"ssh"
#define CVS_EDITOR_DEFAULT	"vi"
#define CVS_TMPDIR_DEFAULT	"/tmp"

/* extensions */
#define CVS_DESCR_FILE_EXT	",t"

/* server-side paths */
#define CVS_PATH_ROOT		"CVSROOT"
#define CVS_PATH_EMPTYDIR	CVS_PATH_ROOT "/Emptydir"
#define CVS_PATH_COMMITINFO	CVS_PATH_ROOT "/commitinfo"
#define CVS_PATH_CONFIG		CVS_PATH_ROOT "/config"
#define CVS_PATH_CVSIGNORE	CVS_PATH_ROOT "/cvsignore"
#define CVS_PATH_CVSWRAPPERS	CVS_PATH_ROOT "/cvswrappers"
#define CVS_PATH_EDITINFO	CVS_PATH_ROOT "/editinfo"
#define CVS_PATH_HISTORY	CVS_PATH_ROOT "/history"
#define CVS_PATH_LOGINFO	CVS_PATH_ROOT "/loginfo"
#define CVS_PATH_MODULES	CVS_PATH_ROOT "/modules"
#define CVS_PATH_NOTIFY_R	CVS_PATH_ROOT "/notify"
#define CVS_PATH_RCSINFO	CVS_PATH_ROOT "/rcsinfo"
#define CVS_PATH_TAGINFO	CVS_PATH_ROOT "/taginfo"
#define CVS_PATH_VERIFYMSG	CVS_PATH_ROOT "/verifymsg"


/* client-side paths */
#define CVS_PATH_RC		".cvsrc"
#define CVS_PATH_CVSDIR		"CVS"
#define CVS_PATH_BASEDIR	CVS_PATH_CVSDIR "/Base"
#define CVS_PATH_BASEREV	CVS_PATH_CVSDIR "/Baserev"
#define CVS_PATH_BASEREVTMP	CVS_PATH_CVSDIR "/Baserev.tmp"
#define CVS_PATH_CHECKINPROG	CVS_PATH_CVSDIR "/Checkin.prog"
#define CVS_PATH_ENTRIES	CVS_PATH_CVSDIR "/Entries"
#define CVS_PATH_STATICENTRIES	CVS_PATH_CVSDIR "/Entries.Static"
#define CVS_PATH_LOGENTRIES	CVS_PATH_CVSDIR "/Entries.Log"
#define CVS_PATH_BACKUPENTRIES	CVS_PATH_CVSDIR "/Entries.Backup"
#define CVS_PATH_NOTIFY		CVS_PATH_CVSDIR "/Notify"
#define CVS_PATH_NOTIFYTMP	CVS_PATH_CVSDIR "/Notify.tmp"
#define CVS_PATH_ROOTSPEC	CVS_PATH_CVSDIR "/Root"
#define CVS_PATH_REPOSITORY	CVS_PATH_CVSDIR "/Repository"
#define CVS_PATH_TAG		CVS_PATH_CVSDIR "/Tag"
#define CVS_PATH_TEMPLATE	CVS_PATH_CVSDIR "/Template"
#define CVS_PATH_UPDATEPROG	CVS_PATH_CVSDIR "/Update.prog"


/* flags for cmd_flags */
#define CVS_CMD_ALLOWSPEC	0x01
#define CVS_CMD_SENDARGS1	0x04
#define CVS_CMD_SENDARGS2	0x08
#define CVS_CMD_SENDDIR		0x10
#define CVS_CMD_PRUNEDIRS	0x20


struct cvs_cmd {
	int	 cmd_op;
	int	 cmd_req;
	char	 cmd_name[CVS_CMD_MAXNAMELEN];
	char	 cmd_alias[CVS_CMD_MAXALIAS][CVS_CMD_MAXNAMELEN];
	char	 cmd_descr[CVS_CMD_MAXDESCRLEN];
	char	*cmd_synopsis;
	char	*cmd_opts;
	char	*cmd_defargs;
	int	 file_flags;

	/* operations vector */
	int	 (*cmd_init)(struct cvs_cmd *, int, char **, int *);
	int	 (*cmd_pre_exec)(struct cvsroot *);
	int	 (*cmd_exec_remote)(CVSFILE *, void *);
	int	 (*cmd_exec_local)(CVSFILE *, void *);
	int	 (*cmd_post_exec)(struct cvsroot *);
	int	 (*cmd_cleanup)(void);

	/* flags for cvs_file_get() */
	int	 cmd_flags;
};

struct cvs_file;
struct cvs_dir;
struct cvs_flist;

struct cvs_var {
	char   *cv_name;
	char   *cv_val;
	TAILQ_ENTRY(cvs_var) cv_link;
};



struct cvs_op {
	u_int             co_op;
	uid_t             co_uid;    /* user performing the operation */
	char             *co_tag;    /* tag or branch, NULL if HEAD */
	char             *co_msg;    /* message string (on commit or add) */
	struct cvs_flist  co_files;
};


#define CVS_ROOT_CONNECTED	0x01

struct cvsroot {
	char   *cr_str;
	u_int   cr_method;
	char   *cr_buf;
	char   *cr_user;
	char   *cr_pass;
	char   *cr_host;
	char   *cr_dir;
	u_int   cr_port;
	u_int   cr_ref;

	/* connection data */
	u_int   cr_flags;
	FILE   *cr_srvin;
	FILE   *cr_srvout;
	FILE   *cr_srverr;
	char   *cr_version;     /* version of remote server */
	u_char  cr_vrmask[16];  /* mask of valid requests supported by server */

	TAILQ_ENTRY(cvsroot) root_cache;
};

#define CVS_SETVR(rt, rq) ((rt)->cr_vrmask[(rq) / 8] |=  (1 << ((rq) % 8)))
#define CVS_GETVR(rt, rq) ((rt)->cr_vrmask[(rq) / 8] &   (1 << ((rq) % 8)))
#define CVS_CLRVR(rt, rq) ((rt)->cr_vrmask[(rq) / 8] &= ~(1 << ((rq) % 8)))
#define CVS_RSTVR(rt)	memset((rt)->cr_vrmask, 0, sizeof((rt)->cr_vrmask))


#define CVS_HIST_ADDED		'A'
#define CVS_HIST_EXPORT		'E'
#define CVS_HIST_RELEASE	'F'
#define CVS_HIST_MODIFIED	'M'
#define CVS_HIST_CHECKOUT	'O'
#define CVS_HIST_COMMIT		'R'
#define CVS_HIST_TAG		'T'


#define CVS_DATE_DUMMY	"dummy timestamp"
#define CVS_DATE_DMSEC	(time_t)-1

#define CVS_ENT_NONE	0
#define CVS_ENT_FILE	1
#define CVS_ENT_DIR	2

#define CVS_ENT_REG		0
#define CVS_ENT_ADDED		1
#define CVS_ENT_REMOVED		2
#define CVS_ENT_UPTODATE	3

#define CVS_ENT_MAXLINELEN	1024

#define CVS_ENTF_SYNC	0x01	/* contents of disk and memory match */
#define CVS_ENTF_WR	0x02	/* file is opened for writing too */

#define STRIP_SLASH(p)					\
	do {						\
		size_t _slen;				\
		_slen = strlen(p);			\
		while ((_slen > 0) && (p[_slen - 1] == '/'))	\
			p[--_slen] = '\0';		\
	} while (0)
struct cvs_ent {
	char			*ce_buf;
	u_int16_t		 ce_type;
	u_int16_t		 ce_status;
	char			*ce_name;
	RCSNUM			*ce_rev;
	time_t			 ce_mtime;
	char			*ce_opts;
	char			*ce_tag;

	/*
	 * This variable is set to 1 if we have already processed this entry
	 * in the cvs_file_getdir() function. This is to avoid files being
	 * passed twice to the callbacks.
	 */
	int			processed;
	TAILQ_ENTRY(cvs_ent)	 ce_list;
};

typedef struct cvs_entries {
	char	*cef_path;
	char	*cef_bpath;
	u_int	 cef_flags;

	TAILQ_HEAD(cvsentrieshead, cvs_ent)	 cef_ent;
	struct cvs_ent		*cef_cur;
} CVSENTRIES;


struct cvs_hent {
	char	 ch_event;
	time_t	 ch_date;
	uid_t	 ch_uid;
	char	*ch_user;
	char	*ch_curdir;
	char	*ch_repo;
	RCSNUM	*ch_rev;
	char	*ch_arg;
};


typedef struct cvs_histfile {
	int	 chf_fd;
	char	*chf_buf;	/* read buffer */
	size_t	 chf_blen;	/* buffer size */
	size_t	 chf_bused;	/* bytes used in buffer */

	off_t	chf_off;	/* next read */
	u_int	chf_sindex;	/* history entry index of first in array */
	u_int	chf_cindex;	/* current index (for getnext()) */
	u_int	chf_nbhent;	/* number of valid entries in the array */

	struct cvs_hent	chf_hent[CVS_HIST_CACHE];

} CVSHIST;


extern char *cvs_req_modulename;
extern char *cvs_repo_base;
extern char *cvs_command;
extern char *cvs_editor;
extern char *cvs_homedir;
extern char *cvs_msg;
extern char *cvs_rsh;
extern char *cvs_tmpdir;

extern int  verbosity;
extern int  cvs_trace;
extern int  cvs_nolog;
extern int  cvs_compress;
extern int  cvs_cmdop;
extern int  cvs_nocase;
extern int  cvs_noexec;
extern int  cvs_readonly;
extern int  cvs_error;
extern CVSFILE *cvs_files;

extern struct cvs_cmd *cvs_cdt[];

extern struct cvs_cmd cvs_cmd_add;
extern struct cvs_cmd cvs_cmd_admin;
extern struct cvs_cmd cvs_cmd_annotate;
extern struct cvs_cmd cvs_cmd_checkout;
extern struct cvs_cmd cvs_cmd_commit;
extern struct cvs_cmd cvs_cmd_diff;
extern struct cvs_cmd cvs_cmd_edit;
extern struct cvs_cmd cvs_cmd_editors;
extern struct cvs_cmd cvs_cmd_export;
extern struct cvs_cmd cvs_cmd_history;
extern struct cvs_cmd cvs_cmd_import;
extern struct cvs_cmd cvs_cmd_init;
extern struct cvs_cmd cvs_cmd_log;
extern struct cvs_cmd cvs_cmd_login;
extern struct cvs_cmd cvs_cmd_logout;
extern struct cvs_cmd cvs_cmd_rdiff;
extern struct cvs_cmd cvs_cmd_release;
extern struct cvs_cmd cvs_cmd_remove;
extern struct cvs_cmd cvs_cmd_rlog;
extern struct cvs_cmd cvs_cmd_rtag;
extern struct cvs_cmd cvs_cmd_status;
extern struct cvs_cmd cvs_cmd_tag;
extern struct cvs_cmd cvs_cmd_update;
extern struct cvs_cmd cvs_cmd_version;
extern struct cvs_cmd cvs_cmd_server;
extern struct cvs_cmd cvs_cmd_unedit;
extern struct cvs_cmd cvs_cmd_watch;
extern struct cvs_cmd cvs_cmd_watchers;


struct cvs_cmd	*cvs_findcmd(const char *);
struct cvs_cmd	*cvs_findcmdbyreq(int);
int		 cvs_startcmd(struct cvs_cmd *, int, char **);
int		 cvs_server(int, char **);

int		 cvs_var_set(const char *, const char *);
int		 cvs_var_unset(const char *);
const char	*cvs_var_get(const char *);


/* root.c */
struct cvsroot	*cvsroot_parse(const char *);
void		 cvsroot_remove(struct cvsroot *);
struct cvsroot	*cvsroot_get(const char *);


/* entries.c */
CVSENTRIES	*cvs_ent_open(const char *, int);
struct cvs_ent	*cvs_ent_get(CVSENTRIES *, const char *);
struct cvs_ent	*cvs_ent_next(CVSENTRIES *);
int		 cvs_ent_add(CVSENTRIES *, struct cvs_ent *);
int		 cvs_ent_addln(CVSENTRIES *, const char *);
int		 cvs_ent_remove(CVSENTRIES *, const char *, int);
int		 cvs_ent_write(CVSENTRIES *);
struct cvs_ent	*cvs_ent_parse(const char *);
void		 cvs_ent_close(CVSENTRIES *);
void		 cvs_ent_free(struct cvs_ent *);

/* history API */
CVSHIST		*cvs_hist_open(const char *);
void		 cvs_hist_close(CVSHIST *);
int		 cvs_hist_parse(CVSHIST *);
struct cvs_hent	*cvs_hist_getnext(CVSHIST *);
int		 cvs_hist_append(CVSHIST *, struct cvs_hent *);

/* logmsg.c */
char	*cvs_logmsg_open(const char *);
char	*cvs_logmsg_get(const char *, struct cvs_flist *,
	    struct cvs_flist *, struct cvs_flist *);
void	cvs_logmsg_send(struct cvsroot *, const char *);

/* date.y */
time_t	cvs_date_parse(const char *);

/* util.c */

struct cvs_line {
	char			*l_line;
	int			 l_lineno;
	TAILQ_ENTRY(cvs_line)	 l_list;
};

TAILQ_HEAD(cvs_tqh, cvs_line);

struct cvs_lines {
	int		l_nblines;
	char		*l_data;
	struct cvs_tqh	l_lines;
};

int	  cvs_readrepo(const char *, char *, size_t);
void	  cvs_modetostr(mode_t, char *, size_t);
void	  cvs_strtomode(const char *, mode_t *);
void	  cvs_splitpath(const char *, char *, size_t, char **);
int	  cvs_mkadmin(const char *, const char *, const char *, char *,
		char *, int);
int	  cvs_cksum(const char *, char *, size_t);
int	  cvs_exec(int, char **, int []);
int	  cvs_getargv(const char *, char **, int);
int	  cvs_chdir(const char *, int);
int	  cvs_rename(const char *, const char *);
int	  cvs_unlink(const char *);
int	  cvs_rmdir(const char *);
int	  cvs_create_dir(const char *, int, char *, char *);
char	 *cvs_rcs_getpath(CVSFILE *, char *, size_t);
char	**cvs_makeargv(const char *, int *);
void	  cvs_freeargv(char **, int);
void	  cvs_write_tagfile(char *, char *, int);
void	  cvs_parse_tagfile(char **, char **, int *);
size_t	  cvs_path_cat(const char *, const char *, char *, size_t);
time_t	  cvs_hack_time(time_t, int);

BUF			*cvs_patchfile(const char *, const char *,
			    int (*p)(struct cvs_lines *, struct cvs_lines *));
struct cvs_lines	*cvs_splitlines(const char *);
void			cvs_freelines(struct cvs_lines *);

/* XXX */
int			rcs_patch_lines(struct cvs_lines *, struct cvs_lines *);
int	cvs_checkout_rev(RCSFILE *, RCSNUM *, CVSFILE *, char *, int, int, ...);

#endif
