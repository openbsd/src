/*	$OpenBSD: cvsd.h,v 1.1.1.1 2004/07/13 22:02:40 jfb Exp $	*/
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

#include <pwd.h>
#include <signal.h>

#include "cvs.h"

#define CVSD_USER   "_cvsd"
#define CVSD_GROUP  "_cvsd"

#define CVSD_CHILD_DEFMIN    3
#define CVSD_CHILD_DEFMAX    5



#define CVSD_FPERM  (S_IRUSR | S_IWUSR)
#define CVSD_DPERM  (S_IRWXU)


/* requests */
#define CVSD_MSG_GETUID    1
#define CVSD_MSG_GETUNAME  2
#define CVSD_MSG_PASSFD    3   /* server passes client file descriptor */
#define CVSD_MSG_SETIDLE   4   /* client has no further processing to do */

/* replies */
#define CVSD_MSG_UID       128
#define CVSD_MSG_UNAME     129

#define CVSD_MSG_SHUTDOWN  253
#define CVSD_MSG_OK        254
#define CVSD_MSG_ERROR     255

#define CVSD_MSG_MAXLEN    256


/* message structure to pass data between the parent and the chrooted child */
struct cvsd_msg {
	u_int8_t  cm_type;
	u_int8_t  cm_len;    /* length of message data in bytes */
};


struct cvsd_child {
	pid_t  ch_pid;
	int    ch_sock;

	TAILQ_ENTRY(cvsd_child) ch_list;
};



extern int foreground;

extern volatile sig_atomic_t running;
extern volatile sig_atomic_t restart;






int  cvsd_checkperms (const char *);
int  cvsd_forkchild  (void);


/* from aclparse.y */
int    cvs_acl_parse  (const char *);
u_int  cvs_acl_eval   (struct cvs_op *);

/* from msg.c */
int    cvsd_sendmsg (int, u_int, const void *, size_t);
int    cvsd_recvmsg (int, u_int *, void *, size_t *);

#endif /* CVSD_H */
