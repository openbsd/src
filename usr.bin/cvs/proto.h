/*	$OpenBSD: proto.h,v 1.11 2006/01/02 09:42:20 xsa Exp $	*/
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

#ifndef PROTO_H
#define PROTO_H

#include "buf.h"
#include "file.h"


#define CVS_PROTO_MAXARG	256

#define CVS_REQ_TIMEOUT		300


/* client/server protocol requests */
#define CVS_REQ_NONE		0
#define CVS_REQ_ROOT		1
#define CVS_REQ_VALIDREQ	2
#define CVS_REQ_VALIDRESP	3
#define CVS_REQ_DIRECTORY	4
#define CVS_REQ_MAXDOTDOT	5
#define CVS_REQ_STATICDIR	6
#define CVS_REQ_STICKY		7
#define CVS_REQ_ENTRY		8
#define CVS_REQ_ENTRYEXTRA	9
#define CVS_REQ_CHECKINTIME	10
#define CVS_REQ_MODIFIED	11
#define CVS_REQ_ISMODIFIED	12
#define CVS_REQ_UNCHANGED	13
#define CVS_REQ_USEUNCHANGED	14
#define CVS_REQ_NOTIFY		15
#define CVS_REQ_NOTIFYUSER	16
#define CVS_REQ_QUESTIONABLE	17
#define CVS_REQ_CASE		18
#define CVS_REQ_UTF8		19
#define CVS_REQ_ARGUMENT	20
#define CVS_REQ_ARGUMENTX	21
#define CVS_REQ_GLOBALOPT	22
#define CVS_REQ_GZIPSTREAM	23
#define CVS_REQ_KERBENCRYPT	24
#define CVS_REQ_GSSENCRYPT	25
#define CVS_REQ_PROTOENCRYPT	26
#define CVS_REQ_GSSAUTH		27
#define CVS_REQ_PROTOAUTH	28
#define CVS_REQ_READCVSRC2	29
#define CVS_REQ_READWRAP	30
#define CVS_REQ_ERRIFREADER	31
#define CVS_REQ_VALIDRCSOPT	32
#define CVS_REQ_READIGNORE	33
#define CVS_REQ_SET		34
#define CVS_REQ_XPANDMOD	35
#define CVS_REQ_CI		36
#define CVS_REQ_CHOWN		37
#define CVS_REQ_SETOWN		38
#define CVS_REQ_SETPERM		39
#define CVS_REQ_CHACL		40
#define CVS_REQ_LISTPERM	41
#define CVS_REQ_LISTACL		42
#define CVS_REQ_SETPASS		43
#define CVS_REQ_PASSWD		44
#define CVS_REQ_DIFF		45
#define CVS_REQ_STATUS		46
#define CVS_REQ_LS		47
#define CVS_REQ_TAG		48
#define CVS_REQ_IMPORT		49
#define CVS_REQ_ADMIN		50
#define CVS_REQ_HISTORY		51
#define CVS_REQ_WATCHERS	52
#define CVS_REQ_EDITORS		53
#define CVS_REQ_ANNOTATE	54
#define CVS_REQ_LOG		55
#define CVS_REQ_CO		56
#define CVS_REQ_EXPORT		57
#define CVS_REQ_RANNOTATE	58
#define CVS_REQ_INIT		59
#define CVS_REQ_UPDATE		60
#define CVS_REQ_ADD		62
#define CVS_REQ_REMOVE		63
#define CVS_REQ_NOOP		64
#define CVS_REQ_RTAG		65
#define CVS_REQ_RELEASE		66
#define CVS_REQ_RLOG		67
#define CVS_REQ_RDIFF		68
#define CVS_REQ_VERSION		69
#define CVS_REQ_WATCH_ON	70
#define CVS_REQ_WATCH_OFF	71
#define CVS_REQ_WATCH_ADD	72
#define CVS_REQ_WATCH_REMOVE	73


#define CVS_REQ_MAX		73


/* responses */
#define CVS_RESP_NONE		0
#define CVS_RESP_OK		1
#define CVS_RESP_ERROR		2
#define CVS_RESP_VALIDREQ	3
#define CVS_RESP_CHECKEDIN	4
#define CVS_RESP_NEWENTRY	5
#define CVS_RESP_CKSUM		6
#define CVS_RESP_COPYFILE	7
#define CVS_RESP_UPDATED	8
#define CVS_RESP_CREATED	9
#define CVS_RESP_UPDEXIST	10
#define CVS_RESP_MERGED		11
#define CVS_RESP_PATCHED	12
#define CVS_RESP_RCSDIFF	13
#define CVS_RESP_MODE		14
#define CVS_RESP_MODTIME	15
#define CVS_RESP_REMOVED	16
#define CVS_RESP_RMENTRY	17
#define CVS_RESP_SETSTATDIR	18
#define CVS_RESP_CLRSTATDIR	19
#define CVS_RESP_SETSTICKY	20
#define CVS_RESP_CLRSTICKY	21
#define CVS_RESP_TEMPLATE	22
#define CVS_RESP_SETCIPROG	23
#define CVS_RESP_SETUPDPROG	24
#define CVS_RESP_NOTIFIED	25
#define CVS_RESP_MODXPAND	26
#define CVS_RESP_WRAPRCSOPT	27
#define CVS_RESP_M		28
#define CVS_RESP_MBINARY	29
#define CVS_RESP_E		30
#define CVS_RESP_F		31
#define CVS_RESP_MT		32

#define CVS_RESP_MAX	32

struct cvs_req {
	int	req_id;
	char	req_str[32];
	u_int	req_flags;
};

struct cvs_resp {
	u_int	resp_id;
	char	resp_str[32];
};


BUF	*cvs_recvfile(struct cvsroot *, mode_t *);
void	 cvs_sendfile(struct cvsroot *, const char *);
void	 cvs_connect(struct cvsroot *);
void	 cvs_disconnect(struct cvsroot *);

int		 cvs_req_handle(char *);
struct cvs_req	*cvs_req_getbyid(int);
struct cvs_req	*cvs_req_getbyname(const char *);
char		*cvs_req_getvalid(void);

int              cvs_resp_handle(struct cvsroot *, char *);
struct cvs_resp* cvs_resp_getbyid(int);
struct cvs_resp* cvs_resp_getbyname(const char *);
char*            cvs_resp_getvalid(void);

void	cvs_sendreq(struct cvsroot *, u_int, const char *);
void	cvs_getresp(struct cvsroot *);
int	cvs_sendresp(u_int, const char *);
void	cvs_getln(struct cvsroot *, char *, size_t);
void	cvs_senddir(struct cvsroot *, CVSFILE *);
void	cvs_sendarg(struct cvsroot *, const char *, int);
void	cvs_sendln(struct cvsroot *, const char *);
void	cvs_sendentry(struct cvsroot *, const CVSFILE *);
void	cvs_sendraw(struct cvsroot *, const void *, size_t);
size_t	cvs_recvraw(struct cvsroot *, void *, size_t);


#endif	/* PROTO_H */
