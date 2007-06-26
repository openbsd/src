/*	$OpenBSD: cvs.h,v 1.137 2007/06/26 18:02:43 xsa Exp $	*/
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

#include <signal.h>

#include "config.h"
#include "file.h"
#include "log.h"
#include "worklist.h"
#include "repository.h"
#include "util.h"
#include "xmalloc.h"

#define CVS_VERSION_MINOR	"0"
#define CVS_VERSION_MAJOR	"1"
#define CVS_VERSION_PORT

#define CVS_VERSION		\
	"OpenCVS version "	\
	CVS_VERSION_MAJOR "." CVS_VERSION_MINOR CVS_VERSION_PORT

#define CVS_HIST_CACHE	128
#define CVS_HIST_NBFLD	6

#define CVS_CKSUM_LEN	MD5_DIGEST_STRING_LENGTH

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
#define CVS_UMASK_DEFAULT	002

/* extensions */
#define CVS_DESCR_FILE_EXT	",t"

/* server-side paths */
#define CVS_PATH_DEVNULL	"/dev/null"
#define CVS_PATH_ROOT		"CVSROOT"
#define CVS_PATH_EMPTYDIR	CVS_PATH_ROOT "/Emptydir"
#define CVS_PATH_CHECKOUTLIST	CVS_PATH_ROOT "/checkoutlist"
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
#define CVS_PATH_VALTAGS	CVS_PATH_ROOT "/val-tags"
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
#define CVS_PATH_ATTIC		"Attic"

/* history stuff */
#define CVS_HISTORY_TAG			0
#define CVS_HISTORY_CHECKOUT		1
#define CVS_HISTORY_EXPORT		2
#define CVS_HISTORY_RELEASE		3
#define CVS_HISTORY_UPDATE_REMOVE	4
#define CVS_HISTORY_UPDATE_CO		5
#define CVS_HISTORY_UPDATE_MERGED	6
#define CVS_HISTORY_UPDATE_MERGED_ERR	7
#define CVS_HISTORY_COMMIT_MODIFIED	8
#define CVS_HISTORY_COMMIT_ADDED	9
#define CVS_HISTORY_COMMIT_REMOVED	10

void	cvs_history_add(int, struct cvs_file *, const char *);

struct cvs_cmd {
	u_int	 cmd_op;
	u_int	 cmd_req;
	char	 cmd_name[CVS_CMD_MAXNAMELEN];
	char	 cmd_alias[CVS_CMD_MAXALIAS][CVS_CMD_MAXNAMELEN];
	char	 cmd_descr[CVS_CMD_MAXDESCRLEN];
	char	*cmd_synopsis;
	char	*cmd_opts;
	char	*cmd_defargs;

	int	(*cmd)(int, char **);
};

struct cvsroot;

struct cvs_recursion {
	void	(*enterdir)(struct cvs_file *);
	void	(*leavedir)(struct cvs_file *);
	void	(*fileproc)(struct cvs_file *);
	int	flags;
};

#define CR_RECURSE_DIRS		0x01
#define CR_ATTIC		0x02
#define CR_REPO			0x04

struct cvs_var {
	char   *cv_name;
	char   *cv_val;
	TAILQ_ENTRY(cvs_var) cv_link;
};

TAILQ_HEAD(, cvs_var) cvs_variables;

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

#define ENT_NOSYNC	0
#define ENT_SYNC	1

#define STRIP_SLASH(p)					\
	do {						\
		size_t _slen;				\
		_slen = strlen(p);			\
		while ((_slen > 0) && (p[_slen - 1] == '/'))	\
			p[--_slen] = '\0';		\
	} while (0)

struct cvs_ent {
	char		*ce_buf;
	char		*ce_name;
	char		*ce_opts;
	char		*ce_tag;
	char		*ce_conflict;
	time_t		 ce_mtime;
	u_int16_t	 ce_type;
	u_int16_t	 ce_status;
	RCSNUM		*ce_rev;
};

struct cvs_ent_line {
	char	*buf;
	TAILQ_ENTRY(cvs_ent_line) entries_list;
};

typedef struct cvs_entries {
	char	*cef_path;
	char	*cef_bpath;
	char	*cef_lpath;

	TAILQ_HEAD(, cvs_ent_line)	 cef_ent;
} CVSENTRIES;

extern struct cvs_wklhead temp_files;
extern volatile sig_atomic_t sig_received;
extern volatile sig_atomic_t cvs_quit;
extern struct cvsroot *current_cvsroot;
extern char *cvs_tagname;
extern char *cvs_command;
extern char *cvs_editor;
extern char *cvs_homedir;
extern char *cvs_msg;
extern char *cvs_rsh;
extern char *cvs_tmpdir;
extern char *import_repository;
extern char *cvs_server_path;

extern int  cvs_umask;
extern int  verbosity;
extern int  cvs_trace;
extern int  cvs_nolog;
extern int  cvs_compress;
extern int  cvs_cmdop;
extern int  cvs_nocase;
extern int  cvs_noexec;
extern int  cvs_readonly;
extern int  cvs_readonlyfs;
extern int  cvs_error;
extern int  cvs_server_active;

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

/* cmd.c */
struct cvs_cmd	*cvs_findcmd(const char *);
struct cvs_cmd	*cvs_findcmdbyreq(u_int);

/* cvs.c */
int		 cvs_var_set(const char *, const char *);
int		 cvs_var_unset(const char *);
const char	*cvs_var_get(const char *);
void		 cvs_cleanup(void);

/* date.y */
time_t		 cvs_date_parse(const char *);

/* entries.c */
struct cvs_ent	*cvs_ent_parse(const char *);
struct cvs_ent	*cvs_ent_get(CVSENTRIES *, const char *);
CVSENTRIES	*cvs_ent_open(const char *);
void	 	cvs_ent_add(CVSENTRIES *, const char *);
void	 	cvs_ent_remove(CVSENTRIES *, const char *);
void	 	cvs_ent_close(CVSENTRIES *, int);
void		cvs_ent_free(struct cvs_ent *);
int		cvs_ent_exists(CVSENTRIES *, const char *);
void		cvs_parse_tagfile(char *, char **, char **, int *);
void		cvs_write_tagfile(const char *, char *, char *, int);

/* root.c */
struct cvsroot	*cvsroot_parse(const char *);
struct cvsroot	*cvsroot_get(const char *);
void		 cvsroot_remove(struct cvsroot *);

/* logmsg.c */
char *	cvs_logmsg_read(const char *path);
char *	cvs_logmsg_create(struct cvs_flisthead *, struct cvs_flisthead *,
	struct cvs_flisthead *);

/* misc stuff */
void	cvs_update_local(struct cvs_file *);
void	cvs_update_enterdir(struct cvs_file *);
void	cvs_update_leavedir(struct cvs_file *);
void	cvs_checkout_file(struct cvs_file *, RCSNUM *, int);
int	update_has_conflict_markers(struct cvs_file *);

#define CO_MERGE	0x01
#define CO_SETSTICKY	0x02
#define CO_DUMP		0x04
#define CO_COMMIT	0x08

/* commands */
int	cvs_add(int, char **);
int	cvs_admin(int, char **);
int	cvs_annotate(int, char **);
int	cvs_checkout(int, char **);
int	cvs_commit(int, char **);
int	cvs_diff(int, char **);
int	cvs_edit(int, char **);
int	cvs_editors(int, char **);
int	cvs_export(int, char **);
int	cvs_getlog(int, char **);
int	cvs_history(int, char **);
int	cvs_import(int, char **);
int	cvs_init(int, char **);
int	cvs_release(int, char **);
int	cvs_remove(int, char **);
int	cvs_status(int, char **);
int	cvs_tag(int, char **);
int	cvs_unedit(int, char **);
int	cvs_update(int, char **);
int	cvs_version(int, char **);
int	cvs_watch(int, char **);
int	cvs_watchers(int, char **);


#endif
