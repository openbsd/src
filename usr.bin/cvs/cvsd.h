/*	$OpenBSD: cvsd.h,v 1.8 2005/02/22 22:33:01 jfb Exp $	*/
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

#ifndef CVSD_H
#define CVSD_H

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <pwd.h>
#include <grp.h>
#include <signal.h>

#include "cvs.h"

#define CVSD_USER   "_cvsd"
#define CVSD_GROUP  "_cvsd"

#define CVSD_PATH_CONF    "/etc/cvsd.conf"
#define CVSD_PATH_CHILD   "/usr/sbin/cvsd-child"

#define CVSD_CHILD_DEFMAX    5
#define CVSD_CHILD_SOCKFD    3


#define CVSD_FPERM  (S_IRUSR | S_IWUSR)
#define CVSD_DPERM  (S_IRWXU)


/* requests */
#define CVSD_MSG_GETUID    1
#define CVSD_MSG_GETUNAME  2
#define CVSD_MSG_GETGID    3
#define CVSD_MSG_GETGNAME  4
#define CVSD_MSG_PASSFD    5   /* server passes client file descriptor */
#define CVSD_MSG_SETIDLE   6   /* client has no further processing to do */

/* replies */
#define CVSD_MSG_UID       128
#define CVSD_MSG_UNAME     129
#define CVSD_MSG_GID       130
#define CVSD_MSG_GNAME     131

#define CVSD_MSG_SHUTDOWN  253
#define CVSD_MSG_OK        254
#define CVSD_MSG_ERROR     255

#define CVSD_MSG_MAXLEN    256


#define CVSD_SET_ROOT     1
#define CVSD_SET_CHMIN    2
#define CVSD_SET_CHMAX    3
#define CVSD_SET_ADDR     4
#define CVSD_SET_SOCK     5
#define CVSD_SET_USER     6
#define CVSD_SET_GROUP    7
#define CVSD_SET_MODDIR   8


#define CVSD_ST_UNKNOWN      0
#define CVSD_ST_IDLE         1
#define CVSD_ST_BUSY         2
#define CVSD_ST_DEAD         3
#define CVSD_ST_STOPPED      4


/* message structure to pass data between the parent and the chrooted child */
struct cvsd_msg {
	u_int8_t  cm_type;
	u_int8_t  cm_len;    /* length of message data in bytes */
};


struct cvsd_addr {
	sa_family_t ca_fam;
	union {
		struct sockaddr_in  sin;
		struct sockaddr_in6 sin6;
	} ca_addr;
};


struct cvsd_child {
	pid_t  ch_pid;
	int    ch_sock;
	u_int  ch_state;

	TAILQ_ENTRY(cvsd_child) ch_list;
};


/*
 * The following structures are used to vehicle information to and from the
 * cvsd-child process handling the cvs session.
 */

struct cvsd_req {
	int  cr_op;		/* operation (see CVS_OP_* in cvs.h) */
	int  cr_nfiles;
};

struct cvsd_resp {
	int cr_code;
};


/* cvsd-child response codes */
#define CVSD_RESP_OK      0
#define CVSD_RESP_INVREQ  1	/* invalid request */
#define CVSD_RESP_DENIED  2	/* access denied */
#define CVSD_RESP_SYSERR  3	/* system error */
#define CVSD_RESP_RDONLY  4	/* repository is read-only */
#define CVSD_RESP_INVFILE 5	/* one or more files are unknown */
#define CVSD_RESP_INVMOD  6


extern uid_t    cvsd_uid;
extern gid_t    cvsd_gid;


int                 cvsd_set        (int, ...);
struct cvsd_child*  cvsd_child_fork (int);
int                 cvsd_child_reap (void);


/* from conf.y */
int    cvs_conf_read (const char *);
u_int  cvs_acl_eval  (struct cvs_op *);

/* from msg.c */
int    cvsd_sendmsg (int, u_int, const void *, size_t);
int    cvsd_recvmsg (int, u_int *, void *, size_t *);
int    cvsd_sendfd  (int, int);
int    cvsd_recvfd  (int);


struct cvsd_sess*  cvsd_sess_alloc  (int);
void               cvsd_sess_free   (struct cvsd_sess *);


#endif /* CVSD_H */
