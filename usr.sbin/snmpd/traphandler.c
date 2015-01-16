/*	$OpenBSD: traphandler.c,v 1.2 2015/01/16 00:05:13 deraadt Exp $	*/
/*
 * Copyright (c) 2014 Bret Stephen Lambert <blambert@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/param.h>	/* nitems */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "ber.h"
#include "snmpd.h"
#include "mib.h"

int	 trapsock;
struct event trapev;
char	 trap_path[PATH_MAX];

void	 traphandler_init(struct privsep *, struct privsep_proc *, void *arg);
int	 traphandler_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 traphandler_bind(struct address *);
void	 traphandler_recvmsg(int, short, void *);
int	 traphandler_priv_recvmsg(struct privsep_proc *, struct imsg *);
int	 traphandler_fork_handler(struct privsep_proc *, struct imsg *);
int	 traphandler_parse(char *, size_t, struct ber_element **,
	    struct ber_element **, u_int *, struct ber_oid *);
void	 traphandler_v1translate(struct ber_oid *, u_int, u_int);

int	 trapcmd_cmp(struct trapcmd *, struct trapcmd *);
void	 trapcmd_exec(struct trapcmd *, struct sockaddr *,
	    struct ber_element *, char *, u_int);

char	*traphandler_hostname(struct sockaddr *, int);

RB_PROTOTYPE(trapcmd_tree, trapcmd, cmd_entry, trapcmd_cmp)
RB_GENERATE(trapcmd_tree, trapcmd, cmd_entry, trapcmd_cmp)

struct trapcmd_tree trapcmd_tree = RB_INITIALIZER(&trapcmd_tree);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	traphandler_dispatch_parent }
};

pid_t
traphandler(struct privsep *ps, struct privsep_proc *p)
{
	struct snmpd		*env = ps->ps_env;

	if (env->sc_traphandler &&
	    (trapsock = traphandler_bind(&env->sc_address)) == -1)
		fatal("could not create trap listener socket");

	return (proc_run(ps, p, procs, nitems(procs), traphandler_init,
	    NULL));
}

void
traphandler_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct snmpd		*env = ps->ps_env;

	if (!env->sc_traphandler)
		return;

	/* listen for SNMP trap messages */
	event_set(&trapev, trapsock, EV_READ|EV_PERSIST, traphandler_recvmsg,
	    ps);
	event_add(&trapev, NULL);
}

int
traphandler_bind(struct address *addr)
{
	int			 s;

	if ((s = snmpd_socket_af(&addr->ss, htons(SNMPD_TRAPPORT))) == -1)
		return (-1);

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;

	if (bind(s, (struct sockaddr *)&addr->ss, addr->ss.ss_len) == -1)
		goto bad;

	return (s);
 bad:
	close (s);
	return (-1);
}

void
traphandler_shutdown(void)
{
	event_del(&trapev);
	close(trapsock);
}

int
traphandler_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	default:
		break;
	}

	return (-1);
}

int
snmpd_dispatch_traphandler(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_ALERT:
		return (traphandler_priv_recvmsg(p, imsg));
	default:
		break;
	}

	return (-1);
}

void
traphandler_recvmsg(int fd, short events, void *arg)
{
	struct privsep		*ps = arg;
	char			 buf[8196];
	struct iovec		 iov[2];
	struct sockaddr_storage	 ss;
	socklen_t		 slen;
	ssize_t			 n;
	struct ber_element	*req, *iter;
	struct ber_oid		 trapoid;
	u_int			 uptime;

	slen = sizeof(ss);
	if ((n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&ss,
	    &slen)) == -1)
		return;

	if (traphandler_parse(buf, n, &req, &iter, &uptime, &trapoid) == -1)
		goto done;

	iov[0].iov_base = &ss;
	iov[0].iov_len = ss.ss_len;
	iov[1].iov_base = buf;
	iov[1].iov_len = n;

	/* Forward it to the parent process */
	if (proc_composev_imsg(ps, PROC_PARENT, -1, IMSG_ALERT,
	    -1, iov, 2) == -1)
		goto done;

 done:
	if (req != NULL)
		ber_free_elements(req);
	return;
}

/*
 * Validate received message
 */
int
traphandler_parse(char *buf, size_t n, struct ber_element **req,
    struct ber_element **vbinds, u_int *uptime, struct ber_oid *trapoid)
{
	struct ber		 ber;
	struct ber_element	*elm;
	u_int			 vers, gtype, etype;

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_set_application(&ber, smi_application);
	ber_set_readbuf(&ber, buf, n);

	if ((*req = ber_read_elements(&ber, NULL)) == NULL)
		goto done;

	if (ber_scanf_elements(*req, "{dSe", &vers, &elm) == -1)
		goto done;

	switch (vers) {
	case SNMP_V1:
		if (ber_scanf_elements(elm, "{oSddd",
		    trapoid, &gtype, &etype, uptime) == -1)
			goto done;
		traphandler_v1translate(trapoid, gtype, etype);
		break;

	case SNMP_V2:
		if (ber_scanf_elements(elm, "{SSSS{e}}", &elm) == -1 ||
		    ber_scanf_elements(elm, "{SdS}{So}e",
		    uptime, trapoid, vbinds) == -1)
			goto done;
		break;

	default:
		log_warnx("unsupported SNMP trap version '%d'", vers);
		goto done;
	}

	ber_free(&ber);
	return (0);

 done:
	ber_free(&ber);
	if (*req)
		ber_free_elements(*req);
	*req = NULL;
	return (-1);
}

void
traphandler_v1translate(struct ber_oid *oid, u_int gtype, u_int etype)
{
	/* append 'specific trap' number to 'enterprise specific' traps */
	if (gtype >= 6) {
		oid->bo_id[oid->bo_n] = 0;
		oid->bo_id[oid->bo_n + 1] = etype;
		oid->bo_n += 2;
	}
}

int
traphandler_priv_recvmsg(struct privsep_proc *p, struct imsg *imsg)
{
	ssize_t			 n;
	pid_t			 pid;

	if ((n = IMSG_DATA_SIZE(imsg)) <= 0)
		return (-1);			/* XXX */

	switch ((pid = fork())) {
	case 0:
		traphandler_fork_handler(p, imsg);
		/* NOTREACHED */
	case -1:
		log_warn("%s: couldn't fork traphandler", __func__);
		return (0);
	default:
		log_debug("forked process %i to handle trap", pid);
		return (0);
	}
	/* NOTREACHED */
}

int
traphandler_fork_handler(struct privsep_proc *p, struct imsg *imsg)
{
	char			 oidbuf[SNMP_MAX_OID_STRLEN];
	struct sockaddr		*sa;
	char			*buf;
	ssize_t			 n;
	struct ber_element	*req, *iter;
	struct trapcmd		*cmd;
	struct ber_oid		 trapoid;
	u_int			 uptime;
	struct passwd		*pw;
	extern int		 debug;

	pw = p->p_ps->ps_pw;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("traphandler_fork_handler: cannot drop privileges");

	closefrom(STDERR_FILENO + 1);
	log_init(debug);

	n = IMSG_DATA_SIZE(imsg);

	sa = imsg->data;
	n -= sa->sa_len;
	buf = (char *)imsg->data + sa->sa_len;

	if (traphandler_parse(buf, n, &req, &iter, &uptime, &trapoid) == -1)
		fatalx("couldn't parse SNMP trap message");

	smi_oid2string(&trapoid, oidbuf, sizeof(oidbuf), 0);
	if ((cmd = trapcmd_lookup(&trapoid)) != NULL)
		trapcmd_exec(cmd, sa, iter, oidbuf, uptime);

	if (req != NULL)
		ber_free_elements(req);

	exit(0);
}

void
trapcmd_exec(struct trapcmd *cmd, struct sockaddr *sa,
    struct ber_element *iter, char *trapoid, u_int uptime)
{
	char			 oidbuf[SNMP_MAX_OID_STRLEN];
	struct ber_oid		 oid;
	struct ber_element	*elm;
	int			 n, s[2], status = 0;
	char			*value, *host;
	pid_t			 child = -1;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, s) == -1) {
		log_warn("could not create pipe for OID '%s'",
		    smi_oid2string(cmd->cmd_oid, oidbuf, sizeof(oidbuf), 0));
		return;
	}

	switch (child = fork()) {
	case 0:
		dup2(s[1], STDIN_FILENO);

		close(s[0]);
		close(s[1]);

		closefrom(STDERR_FILENO + 1);

		/* path to command is in argv[0], args follow */
		execve(cmd->cmd_argv[0], cmd->cmd_argv, NULL);

		/* this shouldn't happen */
		log_warn("could not exec trap command for OID '%s'",
		    smi_oid2string(cmd->cmd_oid, oidbuf, sizeof(oidbuf), 0));
		_exit(1);
		/* NOTREACHED */

	case -1:
		log_warn("could not fork trap command for OID '%s'",
		    smi_oid2string(cmd->cmd_oid, oidbuf, sizeof(oidbuf), 0));
		close(s[0]);
		close(s[1]);
		return;
	}

	close(s[1]);

	host = traphandler_hostname(sa, 0);
	if (dprintf(s[0], "%s\n", host) == -1)
		goto out;

	host = traphandler_hostname(sa, 1);
	if (dprintf(s[0], "%s\n", host) == -1)
		goto out;

	if (dprintf(s[0],
	    "iso.org.dod.internet.mgmt.mib-2.system.sysUpTime.0 %u\n",
	    uptime) == -1)
		goto out;

	if (dprintf(s[0],
	    "iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects."
	    "snmpTrap.snmpTrapOID.0 %s\n", trapoid) == -1)
		goto out;

	for (; iter != NULL; iter = iter->be_next) {
		if (ber_scanf_elements(iter, "{oe}", &oid, &elm) == -1)
			goto out;
		if ((value = smi_print_element(elm)) == NULL)
			goto out;
		smi_oid2string(&oid, oidbuf, sizeof(oidbuf), 0);
		n = dprintf(s[0], "%s %s\n", oidbuf, value);
		free(value);
		if (n == -1)
			goto out;
	}
 out:
	close(s[0]);
	waitpid(child, &status, 0);

	if (WIFSIGNALED(status)) {
		log_warnx("child %i exited due to receipt of signal %i",
		    child, WTERMSIG(status));
	} else if (WEXITSTATUS(status) != 0) {
		log_warnx("child %i exited with status %i",
		    child, WEXITSTATUS(status));
	} else {
		log_debug("child %i finished", child);
	}
		close(s[1]);

	return;
}

char *
traphandler_hostname(struct sockaddr *sa, int numeric)
{
	static char	 buf[NI_MAXHOST];
	int		 flag = 0;

	if (numeric)
		flag = NI_NUMERICHOST;

	bzero(buf, sizeof(buf));
	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0, flag) != 0)
		return ("Unknown");

	return (buf);
}

struct trapcmd *
trapcmd_lookup(struct ber_oid *oid)
{
	struct trapcmd	key, *res;

	bzero(&key, sizeof(key));
	key.cmd_oid = oid;

	if ((res = RB_FIND(trapcmd_tree, &trapcmd_tree, &key)) == NULL)
		res = key.cmd_maybe;
	return (res);
}

int
trapcmd_cmp(struct trapcmd *cmd1, struct trapcmd *cmd2)
{
	int ret;

	ret = ber_oid_cmp(cmd2->cmd_oid, cmd1->cmd_oid);
	switch (ret) {
	case 2:
		/* cmd1 is a child of cmd2 */
		cmd1->cmd_maybe = cmd2;
		return (1);
	default:
		return (ret);
	}
	/* NOTREACHED */
}

int
trapcmd_add(struct trapcmd *cmd)
{
	return (RB_INSERT(trapcmd_tree, &trapcmd_tree, cmd) != NULL);
}

void
trapcmd_free(struct trapcmd *cmd)
{
	RB_REMOVE(trapcmd_tree, &trapcmd_tree, cmd);
	free(cmd->cmd_argv);
	free(cmd->cmd_oid);
	free(cmd);
}
