/*	$OpenBSD: intercept.h,v 1.11 2002/08/04 04:15:50 provos Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _INTERCEPT_H_
#define _INTERCEPT_H_
#include <sys/queue.h>

struct intercept_pid;
struct intercept_replace;

struct intercept_system {
	char *name;
	int (*init)(void);
	int (*open)(void);
	int (*attach)(int, pid_t);
	int (*detach)(int, pid_t);
	int (*report)(int, pid_t);
	int (*read)(int);
	int (*getsyscallnumber)(const char *, const char *);
	char *(*getcwd)(int, pid_t, char *, size_t);
	int (*restcwd)(int);
	int (*io)(int, pid_t, int, void *, u_char *, size_t);
	int (*getarg)(int, void *, int, void **);
	int (*answer)(int, pid_t, u_int32_t, short, int, short);
	int (*newpolicy)(int);
	int (*assignpolicy)(int, pid_t, int);
	int (*policy)(int, int, int, short);
	int (*replace)(int, pid_t, struct intercept_replace *);
	void (*clonepid)(struct intercept_pid *, struct intercept_pid *);
	void (*freepid)(struct intercept_pid *);
};

#define INTERCEPT_READ	1
#define INTERCEPT_WRITE	2

#define ICPOLICY_ASK	0
#define ICPOLICY_PERMIT	-1
#define ICPOLICY_KILL	-2
#define ICPOLICY_NEVER	1	/* overloaded with errno values > 1 */

#define ICLINK_NONE	0	/* do not resolve symlinks */
#define ICLINK_ALL	1	/* resolve all symlinks */
#define ICLINK_NOLAST	2	/* do not resolve last component */

#define ICFLAGS_RESULT	1

struct intercept_pid {
	SPLAY_ENTRY(intercept_pid) next;
	pid_t pid;
	pid_t ppid;		/* parent pid */

	short policynr;
	int execve_code;
	short execve_policy;
	char *name;		/* name of current process image */
	char *newname;		/* image name to be committed by execve */

#define ICFLAGS_UIDKNOWN	0x01
#define ICFLAGS_GIDKNOWN	0x02
	int flags;

	uid_t uid;		/* current uid */
	gid_t gid;		/* current gid */

	void *data;

	int uflags;	/* Flags that can be used by external application */
};

#define INTERCEPT_MAXSYSCALLARGS	10

struct intercept_translate {
	char *name;
	int (*translate)(struct intercept_translate *, int, pid_t, void *);
	int (*print)(char *, size_t, struct intercept_translate *);
	int off2;
	int off;
	u_char trans_valid;
	void *trans_addr;
	void *trans_addr2;
	void *trans_data;
	size_t trans_size;
	char *trans_print;
	TAILQ_ENTRY(intercept_translate) next;
};

struct intercept_replace {
	int num;
	int ind[INTERCEPT_MAXSYSCALLARGS];
	u_char *address[INTERCEPT_MAXSYSCALLARGS];
	size_t len[INTERCEPT_MAXSYSCALLARGS];
};

TAILQ_HEAD(intercept_tlq, intercept_translate);

int intercept_init(void);
pid_t intercept_run(int, int, char *, char * const *);
int intercept_open(void);
int intercept_attach(int, pid_t);
int intercept_attachpid(int, pid_t, char *);
int intercept_detach(int, pid_t);
int intercept_read(int);
int intercept_newpolicy(int);
int intercept_assignpolicy(int, pid_t, int);
int intercept_modifypolicy(int, int, const char *, const char *, short);
void intercept_child_info(pid_t, pid_t);

int intercept_replace_init(struct intercept_replace *);
int intercept_replace_add(struct intercept_replace *, int, u_char *, size_t);
int intercept_replace(int, pid_t, struct intercept_replace *);

int intercept_register_sccb(char *, char *,
    short (*)(int, pid_t, int, const char *, int, const char *, void *, int,
	struct intercept_tlq *, void *),
    void *);
void *intercept_sccb_cbarg(char *, char *);

int intercept_register_gencb(short (*)(int, pid_t, int, const char *, int, const char *, void *, int, void *), void *);
int intercept_register_execcb(void (*)(int, pid_t, int, const char *, const char *, void *), void *);

struct intercept_translate *intercept_register_translation(char *, char *,
    int, struct intercept_translate *);
int intercept_translate(struct intercept_translate *, int, pid_t, int, void *, int);
char *intercept_translate_print(struct intercept_translate *);

#define intercept_register_transstring(x,y,z)	\
	intercept_register_translation(x, y, z, &ic_translate_string)
#define intercept_register_transfn(x,y,z)	\
	intercept_register_translation(x, y, z, &ic_translate_filename)
#define intercept_register_translink(x,y,z)	\
	intercept_register_translation(x, y, z, &ic_translate_linkname)

extern struct intercept_translate ic_translate_string;
extern struct intercept_translate ic_translate_filename;
extern struct intercept_translate ic_translate_linkname;
extern struct intercept_translate ic_translate_unlinkname;
extern struct intercept_translate ic_translate_connect;

void intercept_freepid(pid_t);
struct intercept_pid *intercept_getpid(pid_t);
int intercept_existpids(void);

char *intercept_get_string(int, pid_t, void *);
char *intercept_filename(int, pid_t, void *, int);
void intercept_syscall(int, pid_t, u_int16_t, int, const char *, int,
    const char *, void *, int);
void intercept_syscall_result(int, pid_t, u_int16_t, int, const char *, int,
    const char *, void *, int, int, void *);

#endif /* _INTERCEPT_H_ */
