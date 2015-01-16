/*	$OpenBSD: openbsd-syscalls.c,v 1.45 2015/01/16 00:19:12 deraadt Exp $	*/
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
#include <sys/types.h>

#include <sys/syscall.h>

#include <compat/linux/linux_syscall.h>

#define KTRACE
#define PTRACE
#define NFSCLIENT
#define NFSSERVER
#define SYSVSEM
#define SYSVMSG
#define SYSVSHM
#include <kern/syscalls.c>

#include <compat/linux/linux_syscalls.c>
#undef KTRACE
#undef PTRACE
#undef NFSCLIENT
#undef NFSSERVER
#undef SYSVSEM
#undef SYSVMSG
#undef SYSVSHM

#include <limits.h>

#include <sys/ioctl.h>
#include <sys/tree.h>
#include <dev/systrace.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "intercept.h"

struct emulation {
	const char *name;	/* Emulation name */
	char **sysnames;	/* Array of system call names */
	int  nsysnames;		/* Number of */
};

static struct emulation emulations[] = {
	{ "native",	syscallnames,		SYS_MAXSYSCALL },
	{ "linux",	linux_syscallnames,	LINUX_SYS_MAXSYSCALL },
	{ NULL,		NULL,			0 }
};

struct obsd_data {
	struct emulation *current;
	struct emulation *commit;
};

static int obsd_init(void);
static int obsd_attach(int, pid_t);
static int obsd_report(int, pid_t);
static int obsd_detach(int, pid_t);
static int obsd_open(void);
static struct intercept_pid *obsd_getpid(pid_t);
static void obsd_freepid(struct intercept_pid *);
static void obsd_clonepid(struct intercept_pid *, struct intercept_pid *);
static struct emulation *obsd_find_emulation(const char *);
static int obsd_set_emulation(pid_t, const char *);
static struct emulation *obsd_switch_emulation(struct obsd_data *);
static const char *obsd_syscall_name(pid_t, int);
static int obsd_syscall_number(const char *, const char *);
static short obsd_translate_policy(short);
static short obsd_translate_flags(short);
static int obsd_translate_errno(int);
static int obsd_answer(int, pid_t, u_int16_t, short, int, short,
    struct elevate *);
static int obsd_newpolicy(int);
static int obsd_assignpolicy(int, pid_t, int);
static int obsd_modifypolicy(int, int, int, short);
static int obsd_replace(int, pid_t, u_int16_t, struct intercept_replace *);
static int obsd_io(int, pid_t, int, void *, u_char *, size_t);
static int obsd_setcwd(int, pid_t, int);
static int obsd_restcwd(int);
static int obsd_argument(int, void *, int, void **);
static int obsd_read(int);
static int obsd_scriptname(int, pid_t, char *);

static int
obsd_init(void)
{
	return (0);
}

static int
obsd_attach(int fd, pid_t pid)
{
	if (ioctl(fd, STRIOCATTACH, &pid) == -1)
		return (-1);

	return (0);
}

static int
obsd_report(int fd, pid_t pid)
{
	if (ioctl(fd, STRIOCREPORT, &pid) == -1)
		return (-1);

	return (0);
}

static int
obsd_detach(int fd, pid_t pid)
{
	if (ioctl(fd, STRIOCDETACH, &pid) == -1)
		return (-1);

	return (0);
}

static int
obsd_open(void)
{
	char *path = "/dev/systrace";
	int fd, cfd = -1;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		warn("open: %s", path);
		return (-1);
	}

	if (ioctl(fd, STRIOCCLONE, &cfd) == -1) {
		warn("ioctl(STRIOCCLONE)");
		goto out;
	}

	if (fcntl(cfd, F_SETFD, FD_CLOEXEC) == -1)
		warn("fcntl(F_SETFD)");

 out:
	close (fd);
	return (cfd);
}

static struct intercept_pid *
obsd_getpid(pid_t pid)
{
	struct intercept_pid *icpid;
	struct obsd_data *data;

	icpid = intercept_getpid(pid);
	if (icpid == NULL)
		return (NULL);
	if (icpid->data != NULL)
		return (icpid);

	if ((icpid->data = malloc(sizeof(struct obsd_data))) == NULL)
		err(1, "%s:%d: malloc", __func__, __LINE__);

	data = icpid->data;
	data->current = &emulations[0];
	data->commit = NULL;

	return (icpid);
}

static void
obsd_freepid(struct intercept_pid *ipid)
{
	if (ipid->data != NULL)
		free(ipid->data);
}

static void
obsd_clonepid(struct intercept_pid *opid, struct intercept_pid *npid)
{
	if (opid->data == NULL) {
		npid->data = NULL;
		return;
	}

	if ((npid->data = malloc(sizeof(struct obsd_data))) == NULL)
		err(1, "%s:%d: malloc", __func__, __LINE__);
	memcpy(npid->data, opid->data, sizeof(struct obsd_data));
}

static struct emulation *
obsd_find_emulation(const char *name)
{
	struct emulation *tmp;

	tmp = emulations;
	while (tmp->name) {
		if (!strcmp(tmp->name, name))
			break;
		tmp++;
	}

	if (!tmp->name)
		return (NULL);

	return (tmp);
}

static int
obsd_set_emulation(pid_t pidnr, const char *name)
{
	struct emulation *tmp;
	struct intercept_pid *pid;
	struct obsd_data *data;

	if ((tmp = obsd_find_emulation(name)) == NULL)
		return (-1);

	pid = intercept_getpid(pidnr);
	if (pid == NULL)
		return (-1);
	data = pid->data;

	data->commit = tmp;

	return (0);
}

static struct emulation *
obsd_switch_emulation(struct obsd_data *data)
{
	data->current = data->commit;
	data->commit = NULL;

	return (data->current);
}

static const char *
obsd_syscall_name(pid_t pidnr, int number)
{
	struct intercept_pid *pid;
	struct emulation *current;

	pid = obsd_getpid(pidnr);
	if (pid == NULL)
		return (NULL);
	current = ((struct obsd_data *)pid->data)->current;

	if (number < 0 || number >= current->nsysnames)
		return (NULL);

	return (current->sysnames[number]);
}

static int
obsd_syscall_number(const char *emulation, const char *name)
{
	struct emulation *current;
	int i;

	current = obsd_find_emulation(emulation);
	if (current == NULL)
		return (-1);

	for (i = 0; i < current->nsysnames; i++)
		if (!strcmp(name, current->sysnames[i]))
			return (i);

	return (-1);
}

static short
obsd_translate_policy(short policy)
{
	switch (policy) {
	case ICPOLICY_ASK:
		return (SYSTR_POLICY_ASK);
	case ICPOLICY_PERMIT:
		return (SYSTR_POLICY_PERMIT);
	case ICPOLICY_NEVER:
	default:
		return (SYSTR_POLICY_NEVER);
	}
}

static short
obsd_translate_flags(short flags)
{
	switch (flags) {
	case ICFLAGS_RESULT:
		return (SYSTR_FLAGS_RESULT);
	default:
		return (0);
	}
}

static int
obsd_translate_errno(int nerrno)
{
	return (nerrno);
}

static int
obsd_answer(int fd, pid_t pid, u_int16_t seqnr, short policy, int nerrno,
    short flags, struct elevate *elevate)
{
	struct systrace_answer ans;

	memset(&ans, 0, sizeof(ans));
	ans.stra_pid = pid;
	ans.stra_seqnr = seqnr;
	ans.stra_policy = obsd_translate_policy(policy);
	ans.stra_flags = obsd_translate_flags(flags);
	ans.stra_error = obsd_translate_errno(nerrno);

	if (elevate != NULL) {
		if (elevate->e_flags & ELEVATE_UID) {
			ans.stra_flags |= SYSTR_FLAGS_SETEUID;
			ans.stra_seteuid = elevate->e_uid;
		}
		if (elevate->e_flags & ELEVATE_GID) {
			ans.stra_flags |= SYSTR_FLAGS_SETEGID;
			ans.stra_setegid = elevate->e_gid;
		}
	}

	if (ioctl(fd, STRIOCANSWER, &ans) == -1)
		return (-1);

	return (0);
}

static int 
obsd_scriptname(int fd, pid_t pid, char *scriptname)
{
	struct systrace_scriptname sn;

	sn.sn_pid = pid;
	strlcpy(sn.sn_scriptname, scriptname, sizeof(sn.sn_scriptname));

	return (ioctl(fd, STRIOCSCRIPTNAME, &sn));
}

static int
obsd_newpolicy(int fd)
{
	struct systrace_policy pol;

	pol.strp_op = SYSTR_POLICY_NEW;
	pol.strp_num = -1;
	pol.strp_maxents = 512;

	if (ioctl(fd, STRIOCPOLICY, &pol) == -1)
		return (-1);

	return (pol.strp_num);
}

static int
obsd_assignpolicy(int fd, pid_t pid, int num)
{
	struct systrace_policy pol;

	pol.strp_op = SYSTR_POLICY_ASSIGN;
	pol.strp_num = num;
	pol.strp_pid = pid;

	if (ioctl(fd, STRIOCPOLICY, &pol) == -1)
		return (-1);

	return (0);
}

static int
obsd_modifypolicy(int fd, int num, int code, short policy)
{
	struct systrace_policy pol;

	pol.strp_op = SYSTR_POLICY_MODIFY;
	pol.strp_num = num;
	pol.strp_code = code;
	pol.strp_policy = obsd_translate_policy(policy);

	if (ioctl(fd, STRIOCPOLICY, &pol) == -1)
		return (-1);

	return (0);
}

static int
obsd_replace(int fd, pid_t pid, u_int16_t seqnr,
    struct intercept_replace *repl)
{
	struct systrace_replace replace;
	size_t len, off;
	int i, ret;

	memset(&replace, 0, sizeof(replace));

	for (i = 0, len = 0; i < repl->num; i++) {
		len += repl->len[i];
	}

	replace.strr_pid = pid;
	replace.strr_seqnr = seqnr;
	replace.strr_nrepl = repl->num;
	replace.strr_base = malloc(len);
	replace.strr_len = len;
	if (replace.strr_base == NULL)
		err(1, "%s: malloc", __func__);

	for (i = 0, off = 0; i < repl->num; i++) {
		replace.strr_argind[i] = repl->ind[i];
		replace.strr_offlen[i] = repl->len[i];
		if (repl->len[i] == 0) {
			replace.strr_off[i] = (size_t)repl->address[i];
			continue;
		}

		replace.strr_off[i] = off;
		memcpy(replace.strr_base + off,
		    repl->address[i], repl->len[i]);
		if (repl->flags[i] & ICTRANS_NOLINKS) {
			replace.strr_flags[i] = SYSTR_NOLINKS;
		} else
			replace.strr_flags[i] = 0;

		off += repl->len[i];
	}

	ret = ioctl(fd, STRIOCREPLACE, &replace);
	if (ret == -1 && errno != EBUSY) {
		warn("%s: ioctl", __func__);
	}

	free(replace.strr_base);
	
	return (ret);
}

static int
obsd_io(int fd, pid_t pid, int op, void *addr, u_char *buf, size_t size)
{
	struct systrace_io io;
	extern int ic_abort;

	memset(&io, 0, sizeof(io));
	io.strio_pid = pid;
	io.strio_addr = buf;
	io.strio_len = size;
	io.strio_offs = addr;
	io.strio_op = (op == INTERCEPT_READ ? SYSTR_READ : SYSTR_WRITE);
	if (ioctl(fd, STRIOCIO, &io) == -1) {
		if (errno == EBUSY)
			ic_abort = 1;
		return (-1);
	}

	return (0);
}

static int
obsd_setcwd(int fd, pid_t pid, int atfd)
{
	struct systrace_getcwd gd;
	gd.strgd_pid = pid;
	gd.strgd_atfd = atfd;
	return (ioctl(fd, STRIOCGETCWD, &gd));
}

static int
obsd_restcwd(int fd)
{
	int res;
	if ((res = ioctl(fd, STRIOCRESCWD, 0)) == -1)
		warn("%s: ioctl", __func__); /* XXX */

	return (res);
}

static int
obsd_argument(int off, void *pargs, int argsize, void **pres)
{
	register_t *args = (register_t *)pargs;

	if (off >= argsize / sizeof(register_t))
		return (-1);

	*pres = (void *)args[off];

	return (0);
}

static int
obsd_read(int fd)
{
	struct str_message msg;
	struct intercept_pid *icpid;
	struct obsd_data *data;
	struct emulation *current;

	char name[SYSTR_EMULEN+1];
	const char *sysname;
	u_int16_t seqnr;
	pid_t pid;
	int code;

	if (read(fd, &msg, sizeof(msg)) != sizeof(msg))
		return (-1);

	icpid = obsd_getpid(msg.msg_pid);
	if (icpid == NULL)
		return (-1);
	data = icpid->data;

	current = data->current;
	
	seqnr = msg.msg_seqnr;
	pid = msg.msg_pid;
	switch (msg.msg_type) {
	case SYSTR_MSG_ASK:
		code = msg.msg_data.msg_ask.code;
		sysname = obsd_syscall_name(pid, code);

		intercept_syscall(fd, pid, seqnr, msg.msg_policy,
		    sysname, code, current->name,
		    (void *)msg.msg_data.msg_ask.args,
		    msg.msg_data.msg_ask.argsize);
		break;

	case SYSTR_MSG_RES:
		code = msg.msg_data.msg_ask.code;
		sysname = obsd_syscall_name(pid, code);

		/* Switch emulation around at the right time */
		if (data->commit != NULL) {
			current = obsd_switch_emulation(data);
		}

		intercept_syscall_result(fd, pid, seqnr, msg.msg_policy,
		    sysname, code, current->name,
		    (void *)msg.msg_data.msg_ask.args,
		    msg.msg_data.msg_ask.argsize,
		    msg.msg_data.msg_ask.result,
		    msg.msg_data.msg_ask.rval);
		break;

	case SYSTR_MSG_EMUL:
		memcpy(name, msg.msg_data.msg_emul.emul, SYSTR_EMULEN);
		name[SYSTR_EMULEN] = '\0';

		if (obsd_set_emulation(pid, name) == -1)
			errx(1, "%s:%d: set_emulation(%s)",
			    __func__, __LINE__, name);

		if (icpid->execve_code == -1) {
			icpid->execve_code = 0;

			/* A running attach fake a exec cb */
			current = obsd_switch_emulation(data);

			intercept_syscall_result(fd,
			    pid, seqnr, msg.msg_policy,
			    "execve", 0, current->name,
			    NULL, 0, 0, NULL);
			break;
		}

		if (obsd_answer(fd, pid, seqnr, 0, 0, 0, NULL) == -1)
			err(1, "%s:%d: answer", __func__, __LINE__);
		break;

	case SYSTR_MSG_UGID: {
		struct str_msg_ugid *msg_ugid;
		
		msg_ugid = &msg.msg_data.msg_ugid;

		intercept_ugid(icpid, msg_ugid->uid, msg_ugid->uid);

		if (obsd_answer(fd, pid, seqnr, 0, 0, 0, NULL) == -1)
			err(1, "%s:%d: answer", __func__, __LINE__);
		break;
	}
	case SYSTR_MSG_CHILD:
		intercept_child_info(msg.msg_pid,
		    msg.msg_data.msg_child.new_pid);
		break;
#ifdef SYSTR_MSG_EXECVE
	case SYSTR_MSG_EXECVE: {
		struct str_msg_execve *msg_execve = &msg.msg_data.msg_execve;
		
		intercept_newimage(fd, pid, msg.msg_policy, current->name,
		    msg_execve->path, NULL);

		if (obsd_answer(fd, pid, seqnr, 0, 0, 0, NULL) == -1)
			err(1, "%s:%d: answer", __func__, __LINE__);
		break;
	}
#endif

#ifdef SYSTR_MSG_POLICYFREE
	case SYSTR_MSG_POLICYFREE:
		intercept_policy_free(msg.msg_policy);
		break;
#endif
	}
	return (0);
}

struct intercept_system intercept = {
	"openbsd",
	obsd_init,
	obsd_open,
	obsd_attach,
	obsd_detach,
	obsd_report,
	obsd_read,
	obsd_syscall_number,
	obsd_setcwd,
	obsd_restcwd,
	obsd_io,
	obsd_argument,
	obsd_answer,
	obsd_newpolicy,
	obsd_assignpolicy,
	obsd_modifypolicy,
	obsd_replace,
	obsd_clonepid,
	obsd_freepid,
	obsd_scriptname,
};
