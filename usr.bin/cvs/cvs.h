/*	$OpenBSD: cvs.h,v 1.3 2004/07/14 04:32:42 jfb Exp $	*/
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

#include "rcs.h"

#define CVS_VERSION    "OpenCVS 0.1"


#define CVS_HIST_CACHE     128
#define CVS_HIST_NBFLD     6


#define CVS_REQ_TIMEOUT    300



#define CVS_CKSUM_LEN      33     /* length of a CVS checksum string */


/* operations */
#define CVS_OP_ADD          1
#define CVS_OP_ANNOTATE     2
#define CVS_OP_COMMIT       3
#define CVS_OP_DIFF         4
#define CVS_OP_TAG          5
#define CVS_OP_UPDATE       6




/* methods */
#define CVS_METHOD_NONE       0
#define CVS_METHOD_LOCAL      1    /* local access */
#define CVS_METHOD_SERVER     2    /* tunnel through CVS_RSH */
#define CVS_METHOD_PSERVER    3    /* cvs pserver */
#define CVS_METHOD_KSERVER    4    /* kerberos */
#define CVS_METHOD_GSERVER    5    /* gssapi server */
#define CVS_METHOD_EXT        6
#define CVS_METHOD_FORK       7    /* local but fork */

/* client/server protocol requests */
#define CVS_REQ_NONE          0
#define CVS_REQ_ROOT          1
#define CVS_REQ_VALIDREQ      2
#define CVS_REQ_VALIDRESP     3
#define CVS_REQ_DIRECTORY     4
#define CVS_REQ_MAXDOTDOT     5
#define CVS_REQ_STATICDIR     6
#define CVS_REQ_STICKY        7
#define CVS_REQ_ENTRY         8
#define CVS_REQ_ENTRYEXTRA    9
#define CVS_REQ_CHECKINTIME  10
#define CVS_REQ_MODIFIED     11
#define CVS_REQ_ISMODIFIED   12
#define CVS_REQ_UNCHANGED    13
#define CVS_REQ_USEUNCHANGED 14
#define CVS_REQ_NOTIFY       15
#define CVS_REQ_NOTIFYUSER   16
#define CVS_REQ_QUESTIONABLE 17
#define CVS_REQ_CASE         18
#define CVS_REQ_UTF8         19
#define CVS_REQ_ARGUMENT     20
#define CVS_REQ_ARGUMENTX    21
#define CVS_REQ_GLOBALOPT    22
#define CVS_REQ_GZIPSTREAM   23
#define CVS_REQ_KERBENCRYPT  24
#define CVS_REQ_GSSENCRYPT   25
#define CVS_REQ_PROTOENCRYPT 26
#define CVS_REQ_GSSAUTH      27
#define CVS_REQ_PROTOAUTH    28
#define CVS_REQ_READCVSRC2   29
#define CVS_REQ_READWRAP     30
#define CVS_REQ_ERRIFREADER  31
#define CVS_REQ_VALIDRCSOPT  32
#define CVS_REQ_READIGNORE   33
#define CVS_REQ_SET          34
#define CVS_REQ_XPANDMOD     35
#define CVS_REQ_CI           36
#define CVS_REQ_CHOWN        37
#define CVS_REQ_SETOWN       38
#define CVS_REQ_SETPERM      39
#define CVS_REQ_CHACL        40
#define CVS_REQ_LISTPERM     41
#define CVS_REQ_LISTACL      42
#define CVS_REQ_SETPASS      43
#define CVS_REQ_PASSWD       44
#define CVS_REQ_DIFF         45
#define CVS_REQ_STATUS       46
#define CVS_REQ_LS           47
#define CVS_REQ_TAG          48
#define CVS_REQ_IMPORT       49
#define CVS_REQ_ADMIN        50
#define CVS_REQ_HISTORY      51
#define CVS_REQ_WATCHERS     52
#define CVS_REQ_EDITORS      53
#define CVS_REQ_ANNOTATE     54
#define CVS_REQ_LOG          55
#define CVS_REQ_CO           56
#define CVS_REQ_EXPORT       57
#define CVS_REQ_RANNOTATE    58
#define CVS_REQ_INIT         59
#define CVS_REQ_UPDATE       60
#define CVS_REQ_ADD          62
#define CVS_REQ_REMOVE       63
#define CVS_REQ_NOOP         64
#define CVS_REQ_RTAG         65
#define CVS_REQ_RELEASE      66
#define CVS_REQ_RLOG         67
#define CVS_REQ_RDIFF        68
#define CVS_REQ_VERSION      69

#define CVS_REQ_MAX          69


/* responses */
#define CVS_RESP_OK           1
#define CVS_RESP_ERROR        2
#define CVS_RESP_VALIDREQ     3
#define CVS_RESP_CHECKEDIN    4
#define CVS_RESP_NEWENTRY     5
#define CVS_RESP_CKSUM        6
#define CVS_RESP_COPYFILE     7
#define CVS_RESP_UPDATED      8
#define CVS_RESP_CREATED      9
#define CVS_RESP_UPDEXIST    10
#define CVS_RESP_MERGED      11
#define CVS_RESP_PATCHED     12
#define CVS_RESP_RCSDIFF     13
#define CVS_RESP_MODE        14
#define CVS_RESP_MODTIME     15
#define CVS_RESP_REMOVED     16
#define CVS_RESP_RMENTRY     17
#define CVS_RESP_SETSTATDIR  18
#define CVS_RESP_CLRSTATDIR  19
#define CVS_RESP_SETSTICKY   20
#define CVS_RESP_CLRSTICKY   21
#define CVS_RESP_TEMPLATE    22
#define CVS_RESP_SETCIPROG   23
#define CVS_RESP_SETUPDPROG  24
#define CVS_RESP_NOTIFIED    25
#define CVS_RESP_MODXPAND    26
#define CVS_RESP_WRAPRCSOPT  27
#define CVS_RESP_M           28
#define CVS_RESP_MBINARY     29
#define CVS_RESP_E           30
#define CVS_RESP_F           31
#define CVS_RESP_MT          32




#define CVS_CMD_MAXNAMELEN   16
#define CVS_CMD_MAXALIAS      2
#define CVS_CMD_MAXDESCRLEN  64


/* defaults */
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


struct cvs_op {
	u_int  co_op;
	uid_t  co_uid;    /* user performing the operation */
	char  *co_path;   /* target path of the operation */
	char  *co_tag;    /* tag or branch, NULL if HEAD */
};






struct cvsroot {
	u_int   cr_method;
	char   *cr_buf;
	char   *cr_user;
	char   *cr_pass;
	char   *cr_host;
	char   *cr_dir;
	u_int   cr_port;
};


#define CVS_HIST_ADDED    'A'
#define CVS_HIST_EXPORT   'E'
#define CVS_HIST_RELEASE  'F'
#define CVS_HIST_MODIFIED 'M'
#define CVS_HIST_CHECKOUT 'O'
#define CVS_HIST_COMMIT   'R'
#define CVS_HIST_TAG      'T'


#define CVS_ENT_NONE    0
#define CVS_ENT_FILE    1
#define CVS_ENT_DIR     2


struct cvs_ent {
	char    *ce_line;
	char    *ce_buf;
	u_int    ce_type;
	char    *ce_name;
	RCSNUM  *ce_rev;
	char    *ce_timestamp;
	char    *ce_opts;
	char    *ce_tag;
};

typedef struct cvs_entries {
	char    *cef_path;
	FILE    *cef_file;

	u_int    cef_nid;  /* next entry index to return for next() */

	struct cvs_ent **cef_entries;
	u_int            cef_nbent;
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



/* client command handlers */
int  cvs_add      (int, char **);
int  cvs_commit   (int, char **);
int  cvs_diff     (int, char **);
int  cvs_getlog   (int, char **);
int  cvs_history  (int, char **);
int  cvs_init     (int, char **);
int  cvs_server   (int, char **);
int  cvs_update   (int, char **);
int  cvs_version  (int, char **);


/* proto.c */
int         cvs_req_handle     (char *);
const char* cvs_req_getbyid    (int);
int         cvs_req_getbyname  (const char *);
char*       cvs_req_getvalid   (void);


int         cvs_resp_handle    (char *);
const char* cvs_resp_getbyid   (int);
int         cvs_resp_getbyname (const char *);
char*       cvs_resp_getvalid  (void);

int         cvs_sendfile       (const char *);
int         cvs_recvfile       (const char *);


/* from client.c */
int     cvs_client_connect     (void);
void    cvs_client_disconnect  (void);
int     cvs_client_sendreq     (u_int, const char *, int);
int     cvs_client_sendarg     (const char *, int);
int     cvs_client_sendln      (const char *);
int     cvs_client_sendraw     (const void *, size_t);
ssize_t cvs_client_recvraw     (void *, size_t);
int     cvs_client_getln       (char *, size_t);
int     cvs_client_senddir     (const char *);


/* from root.c */
struct cvsroot*  cvsroot_parse (const char *);
void             cvsroot_free  (struct cvsroot *);
struct cvsroot*  cvsroot_get   (const char *);


int     cvs_file_init       (void);
int     cvs_file_ignore     (const char *);
int     cvs_file_isignored  (const char *);
char**  cvs_file_getv       (const char *, int *);
void    cvs_file_free       (char **, int);


/* Entries API */
CVSENTRIES*      cvs_ent_open   (const char *, int);
struct cvs_ent*  cvs_ent_get    (CVSENTRIES *, const char *);
struct cvs_ent*  cvs_ent_next   (CVSENTRIES *);
int              cvs_ent_add    (CVSENTRIES *, struct cvs_ent *);
int              cvs_ent_remove (CVSENTRIES *, const char *);
struct cvs_ent*  cvs_ent_parse  (const char *);
void             cvs_ent_close  (CVSENTRIES *);

/* history API */
CVSHIST*         cvs_hist_open    (const char *);
void             cvs_hist_close   (CVSHIST *);
int              cvs_hist_parse   (CVSHIST *);
struct cvs_hent* cvs_hist_getnext (CVSHIST *);
int              cvs_hist_append  (CVSHIST *, struct cvs_hent *);


/* from util.c */
int    cvs_readrepo   (const char *, char *, size_t);
int    cvs_splitpath  (const char *, char *, size_t, char *, size_t);
int    cvs_modetostr  (mode_t, char *, size_t);
int    cvs_strtomode  (const char *, mode_t *);
int    cvs_cksum      (const char *, char *, size_t);
int    cvs_exec       (int, char **, int []);


#endif /* CVS_H */
