/*	$OpenBSD: cvs.h,v 1.42 2005/01/13 17:53:34 jfb Exp $	*/
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

#include <sys/param.h>
#include <stdio.h>
#include <dirent.h>

#include "rcs.h"
#include "file.h"

#define CVS_VERSION_MAJOR 0
#define CVS_VERSION_MINOR 2
#define CVS_VERSION       "OpenCVS 0.2"


#define CVS_HIST_CACHE     128
#define CVS_HIST_NBFLD     6


#define CVS_CKSUM_LEN      33     /* length of a CVS checksum string */


/* operations */
#define CVS_OP_UNKNOWN      0
#define CVS_OP_ADD          1
#define CVS_OP_ANNOTATE     2
#define CVS_OP_CHECKOUT     3
#define CVS_OP_COMMIT       4
#define CVS_OP_DIFF         5
#define CVS_OP_HISTORY      6
#define CVS_OP_IMPORT       7
#define CVS_OP_INIT         8
#define CVS_OP_LOG          9
#define CVS_OP_REMOVE      10
#define CVS_OP_SERVER      11
#define CVS_OP_STATUS      12
#define CVS_OP_TAG         13
#define CVS_OP_UPDATE      14
#define CVS_OP_VERSION     15

#define CVS_OP_ANY         64     /* all operations */


/* methods */
#define CVS_METHOD_NONE       0
#define CVS_METHOD_LOCAL      1    /* local access */
#define CVS_METHOD_SERVER     2    /* tunnel through CVS_RSH */
#define CVS_METHOD_PSERVER    3    /* cvs pserver */
#define CVS_METHOD_KSERVER    4    /* kerberos */
#define CVS_METHOD_GSERVER    5    /* gssapi server */
#define CVS_METHOD_EXT        6
#define CVS_METHOD_FORK       7    /* local but fork */

#define CVS_CMD_MAXNAMELEN   16
#define CVS_CMD_MAXALIAS      2
#define CVS_CMD_MAXDESCRLEN  64
#define CVS_CMD_MAXARG      128


/* defaults */
#define CVS_SERVER_DEFAULT  "cvs"
#define CVS_RSH_DEFAULT     "ssh"
#define CVS_EDITOR_DEFAULT  "vi"


/* server-side paths */
#define CVS_PATH_ROOT         "CVSROOT"
#define CVS_PATH_COMMITINFO   CVS_PATH_ROOT "/commitinfo"
#define CVS_PATH_CONFIG       CVS_PATH_ROOT "/config"
#define CVS_PATH_CVSIGNORE    CVS_PATH_ROOT "/cvsignore"
#define CVS_PATH_CVSWRAPPERS  CVS_PATH_ROOT "/cvswrappers"
#define CVS_PATH_EDITINFO     CVS_PATH_ROOT "/editinfo"
#define CVS_PATH_HISTORY      CVS_PATH_ROOT "/history"
#define CVS_PATH_LOGINFO      CVS_PATH_ROOT "/loginfo"
#define CVS_PATH_MODULES      CVS_PATH_ROOT "/modules"
#define CVS_PATH_NOTIFY       CVS_PATH_ROOT "/notify"
#define CVS_PATH_RCSINFO      CVS_PATH_ROOT "/rcsinfo"
#define CVS_PATH_TAGINFO      CVS_PATH_ROOT "/taginfo"
#define CVS_PATH_VERIFYMSG    CVS_PATH_ROOT "/verifymsg"


/* client-side paths */
#define CVS_PATH_RC             ".cvsrc"
#define CVS_PATH_CVSDIR         "CVS"
#define CVS_PATH_ENTRIES        CVS_PATH_CVSDIR "/Entries"
#define CVS_PATH_STATICENTRIES  CVS_PATH_CVSDIR "/Entries.Static"
#define CVS_PATH_LOGENTRIES     CVS_PATH_CVSDIR "/Entries.Log"
#define CVS_PATH_ROOTSPEC       CVS_PATH_CVSDIR "/Root"
#define CVS_PATH_REPOSITORY     CVS_PATH_CVSDIR "/Repository"


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


#define CVS_ROOT_CONNECTED    0x01

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
};

#define CVS_SETVR(rt, rq)  ((rt)->cr_vrmask[(rq) / 8] |=  (1 << ((rq) % 8)))
#define CVS_GETVR(rt, rq)  ((rt)->cr_vrmask[(rq) / 8] &   (1 << ((rq) % 8)))
#define CVS_CLRVR(rt, rq)  ((rt)->cr_vrmask[(rq) / 8] &= ~(1 << ((rq) % 8)))
#define CVS_RSTVR(rt)      memset((rt)->cr_vrmask, 0, sizeof((rt)->cr_vrmask))


#define CVS_HIST_ADDED    'A'
#define CVS_HIST_EXPORT   'E'
#define CVS_HIST_RELEASE  'F'
#define CVS_HIST_MODIFIED 'M'
#define CVS_HIST_CHECKOUT 'O'
#define CVS_HIST_COMMIT   'R'
#define CVS_HIST_TAG      'T'


#define CVS_DATE_DUMMY  "dummy timestamp"
#define CVS_DATE_DMSEC  (time_t)-1

#define CVS_ENT_NONE    0
#define CVS_ENT_FILE    1
#define CVS_ENT_DIR     2


#define CVS_ENTF_SYNC   0x01    /* contents of disk and memory match */
#define CVS_ENTF_WR     0x02    /* file is opened for writing too */


struct cvs_ent {
	char    *ce_buf;
	u_int    ce_type;
	char    *ce_name;
	RCSNUM  *ce_rev;
	time_t   ce_mtime;
	char    *ce_opts;
	char    *ce_tag;
	TAILQ_ENTRY(cvs_ent) ce_list;
};

typedef struct cvs_entries {
	char    *cef_path;
	FILE    *cef_file;
	u_int    cef_flags;

	TAILQ_HEAD(, cvs_ent) cef_ent;
	struct cvs_ent       *cef_cur;
} CVSENTRIES;


struct cvs_hent {
	char    ch_event;
	time_t  ch_date;
	uid_t   ch_uid;
	char   *ch_user;
	char   *ch_curdir;
	char   *ch_repo;
	RCSNUM *ch_rev;
	char   *ch_arg;
};


typedef struct cvs_histfile {
	int     chf_fd;
	char   *chf_buf;       /* read buffer */
	size_t  chf_blen;      /* buffer size */
	size_t  chf_bused;     /* bytes used in buffer */

	off_t   chf_off;       /* next read */
	u_int   chf_sindex;    /* history entry index of first in array */
	u_int   chf_cindex;    /* current index (for getnext()) */
	u_int   chf_nbhent;    /* number of valid entries in the array */

	struct cvs_hent chf_hent[CVS_HIST_CACHE];

} CVSHIST;


#ifdef CVS

extern char *cvs_command;
extern char *cvs_editor;
extern char *cvs_msg;
extern char *cvs_rsh;

extern int  verbosity;
extern int  cvs_trace;
extern int  cvs_nolog;
extern int  cvs_compress;
extern int  cvs_cmdop;
extern int  cvs_nocase;
extern int  cvs_readonly;

extern CVSFILE *cvs_files;

#endif


/* client command handlers */
int  cvs_add      (int, char **);
int  cvs_annotate (int, char **);
int  cvs_checkout (int, char **);
int  cvs_commit   (int, char **);
int  cvs_diff     (int, char **);
int  cvs_getlog   (int, char **);
int  cvs_history  (int, char **);
int  cvs_import   (int, char **);
int  cvs_init     (int, char **);
int  cvs_remove   (int, char **);
int  cvs_server   (int, char **);
int  cvs_status   (int, char **);
int  cvs_tag      (int, char **);
int  cvs_update   (int, char **);
int  cvs_version  (int, char **);


int         cvs_var_set   (const char *, const char *);
int         cvs_var_unset (const char *);
const char* cvs_var_get   (const char *);


/* from root.c */
struct cvsroot*  cvsroot_parse (const char *);
void             cvsroot_free  (struct cvsroot *);
struct cvsroot*  cvsroot_get   (const char *);


/* Entries API */
CVSENTRIES*      cvs_ent_open   (const char *, int);
struct cvs_ent*  cvs_ent_get    (CVSENTRIES *, const char *);
struct cvs_ent*  cvs_ent_next   (CVSENTRIES *);
int              cvs_ent_add    (CVSENTRIES *, struct cvs_ent *);
int              cvs_ent_addln  (CVSENTRIES *, const char *);
int              cvs_ent_remove (CVSENTRIES *, const char *);
int              cvs_ent_write  (CVSENTRIES *);
struct cvs_ent*  cvs_ent_parse  (const char *);
void             cvs_ent_close  (CVSENTRIES *);
void             cvs_ent_free   (struct cvs_ent *);
struct cvs_ent*  cvs_ent_getent (const char *);

/* history API */
CVSHIST*         cvs_hist_open    (const char *);
void             cvs_hist_close   (CVSHIST *);
int              cvs_hist_parse   (CVSHIST *);
struct cvs_hent* cvs_hist_getnext (CVSHIST *);
int              cvs_hist_append  (CVSHIST *, struct cvs_hent *);

/* from logmsg.c */
char*  cvs_logmsg_open (const char *);
char*  cvs_logmsg_get  (const char *, struct cvs_flist *, struct cvs_flist *, struct cvs_flist *);
int    cvs_logmsg_send (struct cvsroot *, const char *);

/* from util.c */
#define CVS_DATE_CTIME  0
#define CVS_DATE_RFC822 1

int    cvs_readrepo   (const char *, char *, size_t);
time_t cvs_datesec    (const char *, int, int);
int    cvs_modetostr  (mode_t, char *, size_t);
int    cvs_strtomode  (const char *, mode_t *);
int    cvs_splitpath  (const char *, char *, size_t, char **);
int    cvs_mkadmin    (CVSFILE *, mode_t);
int    cvs_cksum      (const char *, char *, size_t);
int    cvs_exec       (int, char **, int []);
int    cvs_getargv    (const char *, char **, int);
char** cvs_makeargv   (const char *, int *);
void   cvs_freeargv   (char **, int);


#endif /* CVS_H */
