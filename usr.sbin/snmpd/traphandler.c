/*	$OpenBSD: traphandler.c,v 1.19 2021/01/05 18:12:15 martijn Exp $	*/

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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ber.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

char	 trap_path[PATH_MAX];

void	 traphandler_init(struct privsep *, struct privsep_proc *, void *arg);
int	 traphandler_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 traphandler_bind(struct address *);
void	 traphandler_recvmsg(int, short, void *);
int	 traphandler_priv_recvmsg(struct privsep_proc *, struct imsg *);
int	 traphandler_fork_handler(struct privsep_proc *, struct imsg *);
int	 traphandler_parse(struct ber_element *, char *, struct sockaddr *);
struct ber_element *
	 traphandler_v1translate(struct ber_element *, char *, int);
int	 trapcmd_cmp(struct trapcmd *, struct trapcmd *);
void	 trapcmd_exec(struct trapcmd *, struct sockaddr *,
	    struct ber_element *);

char	*traphandler_hostname(struct sockaddr *, int);

RB_PROTOTYPE(trapcmd_tree, trapcmd, cmd_entry, trapcmd_cmp)
RB_GENERATE(trapcmd_tree, trapcmd, cmd_entry, trapcmd_cmp)

struct trapcmd_tree trapcmd_tree = RB_INITIALIZER(&trapcmd_tree);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	traphandler_dispatch_parent }
};

void
traphandler(struct privsep *ps, struct privsep_proc *p)
{
	struct snmpd		*env = ps->ps_env;
	struct address		*h;

	if (env->sc_traphandler) {
		TAILQ_FOREACH(h, &env->sc_addresses, entry) {
			if (h->type != SOCK_DGRAM)
				continue;
			if ((h->fd = traphandler_bind(h)) == -1)
				fatal("could not create trap listener socket");
		}
	}

	proc_run(ps, p, procs, nitems(procs), traphandler_init, NULL);
}

void
traphandler_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct snmpd		*env = ps->ps_env;
	struct address		*h;

	if (pledge("stdio id proc recvfd exec", NULL) == -1)
		fatal("pledge");

	if (!env->sc_traphandler)
		return;

	/* listen for SNMP trap messages */
	TAILQ_FOREACH(h, &env->sc_addresses, entry) {
		event_set(&h->ev, h->fd, EV_READ|EV_PERSIST,
		    traphandler_recvmsg, NULL);
		event_add(&h->ev, NULL);
	}
}

int
traphandler_bind(struct address *addr)
{
	int			 s;
	char			 buf[512];
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;

	if (addr->ss.ss_family == AF_INET) {
		sin = (struct sockaddr_in *)&(addr->ss);
		sin->sin_port = htons(162);
	} else {
		sin6 = (struct sockaddr_in6 *)&(addr->ss);
		sin6->sin6_port = htons(162);
	}
	if ((s = snmpd_socket_af(&addr->ss, SOCK_DGRAM)) == -1)
		return (-1);

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;

	if (bind(s, (struct sockaddr *)&addr->ss, addr->ss.ss_len) == -1)
		goto bad;

	if (print_host(&addr->ss, buf, sizeof(buf)) == NULL)
		goto bad;

	log_info("traphandler: listening on %s:%s", buf, SNMPD_TRAPPORT);

	return (s);
 bad:
	close (s);
	return (-1);
}

void
traphandler_shutdown(void)
{
	struct address		*h;

	TAILQ_FOREACH(h, &snmpd_env->sc_addresses, entry) {
		event_del(&h->ev);
		close(h->fd);
	}
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
	struct ber		 ber = {0};
	struct ber_element	*msg = NULL, *pdu;
	char			 buf[8196];
	struct sockaddr_storage	 ss;
	socklen_t		 slen;
	ssize_t			 n;
	int			 vers;
	char			*community;

	slen = sizeof(ss);
	if ((n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&ss,
	    &slen)) == -1)
		return;

	ober_set_application(&ber, smi_application);
	ober_set_readbuf(&ber, buf, n);

	if ((msg = ober_read_elements(&ber, NULL)) == NULL)
		goto parsefail;

	if (ober_scanf_elements(msg, "{dse", &vers, &community, &pdu) == -1)
		goto parsefail;

	switch (vers) {
	case SNMP_V1:
		if (pdu->be_type != SNMP_C_TRAP)
			goto parsefail;
		break;
	case SNMP_V2:
		if (pdu->be_type != SNMP_C_TRAPV2)
			goto parsefail;
		break;
	default:
		goto parsefail;
	}

	(void)traphandler_parse(pdu, community, (struct sockaddr *)&ss);

parsefail:
	ober_free(&ber);
	if (msg != NULL)
		ober_free_elements(msg);
}

/*
 * Validate received message
 */
int
traphandler_parse(struct ber_element *pdu, char *community, struct sockaddr *sa)
{
	struct privsep		*ps = &snmpd_env->sc_ps;
	struct ber		 ber = {0};
	struct ber_element	*vblist = NULL, *elm, *elm2;
	struct ber_oid		 o1, o2, snmpTrapOIDOID;
	struct ber_oid		 snmpTrapOID, sysUpTimeOID;
	int			 sysUpTime;
	struct iovec		 iov[2];
	void			*buf;
	ssize_t			 buflen;
	int			 ret = -1;

	switch (pdu->be_type) {
	case SNMP_C_TRAP:
		vblist = traphandler_v1translate(pdu, community, 0);
		if (vblist == NULL)
			goto done;
		break;
	case SNMP_C_TRAPV2:
		if (ober_scanf_elements(pdu, "{SSe}", &elm) == -1)
			goto done;
		if (elm->be_type != BER_TYPE_INTEGER)
			goto done;
		vblist = ober_unlink_elements(elm);
		break;
	default:
		log_warnx("unsupported SNMP trap version '%d'", pdu->be_type);
		goto done;
	}

	(void)ober_string2oid("1.3.6.1.2.1.1.3.0", &sysUpTimeOID);
	(void)ober_string2oid("1.3.6.1.6.3.1.1.4.1.0", &snmpTrapOIDOID);
	if (ober_scanf_elements(vblist, "{{od}{oo}", &o1, &sysUpTime, &o2,
	    &snmpTrapOID) == -1 ||
	    ober_oid_cmp(&o1, &sysUpTimeOID) != 0 ||
	    ober_oid_cmp(&o2, &snmpTrapOIDOID) != 0) {
		goto done;
	}
	(void)ober_scanf_elements(vblist, "{Se", &elm);
	for (elm = elm->be_next; elm != NULL; elm = elm->be_next) {
		if (ober_scanf_elements(elm, "{oe}", &o1, &elm2) == -1 ||
		    elm2->be_next != NULL)
			goto done;
	}

	ober_set_application(&ber, smi_application);

	if ((buflen = ober_write_elements(&ber, vblist)) == -1 ||
	    ober_get_writebuf(&ber, &buf) == -1)
		goto done;

	iov[0].iov_base = sa;
	iov[0].iov_len = sa->sa_len;
	iov[1].iov_base = buf;
	iov[1].iov_len = buflen;

	/* Forward it to the parent process */
	if (proc_composev(ps, PROC_PARENT, IMSG_ALERT, iov, 2) == -1)
		goto done;

	ret = 0;
done:
	ober_free(&ber);
	if (vblist != NULL)
		ober_free_elements(vblist);
	return ret;
}

struct ber_element *
traphandler_v1translate(struct ber_element *pdu, char *community, int proxy)
{
	struct ber_oid trapoid, enterprise, oid, snmpTrapAddressOid;
	struct ber_oid snmpTrapCommunityOid, snmpTrapEnterpriseOid;
	struct ber_element *elm, *last, *vblist, *vb0 = NULL;
	void *agent_addr;
	size_t agent_addrlen;
	int generic_trap, specific_trap, time_stamp;
	int hasaddress = 0, hascommunity = 0, hasenterprise = 0;

	if (ober_scanf_elements(pdu, "{oxddde", &enterprise, &agent_addr,
	    &agent_addrlen, &generic_trap, &specific_trap, &time_stamp,
	    &vblist) == -1 ||
	    agent_addrlen != 4 ||
	    vblist->be_type != BER_TYPE_SEQUENCE) {
		errno = EINVAL;
		return NULL;
	}
	switch (generic_trap) {
	case 0:
		(void)ober_string2oid("1.3.6.1.6.3.1.1.5.1", &trapoid);
		break;
	case 1:
		(void)ober_string2oid("1.3.6.1.6.3.1.1.5.2", &trapoid);
		break;
	case 2:
		(void)ober_string2oid("1.3.6.1.6.3.1.1.5.3", &trapoid);
		break;
	case 3:
		(void)ober_string2oid("1.3.6.1.6.3.1.1.5.4", &trapoid);
		break;
	case 4:
		(void)ober_string2oid("1.3.6.1.6.3.1.1.5.5", &trapoid);
		break;
	case 5:
		(void)ober_string2oid("1.3.6.1.6.3.1.1.5.6", &trapoid);
		break;
	case 6:
		trapoid = enterprise;
		if (trapoid.bo_n + 2 > BER_MAX_OID_LEN) {
			errno = EINVAL;
			return NULL;
		}
		trapoid.bo_id[trapoid.bo_n++] = 0;
		trapoid.bo_id[trapoid.bo_n++] = specific_trap;
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	/* work aronud net-snmp's snmptrap: It adds an EOC element in vblist */
	if (vblist->be_len != 0)
		vb0 = ober_unlink_elements(vblist);

	if ((vblist = ober_add_sequence(NULL)) == NULL) {
		if (vb0 != NULL)
			ober_free_elements(vb0);
		return NULL;
	}
	if (ober_printf_elements(vblist, "{od}{oO}e", "1.3.6.1.2.1.1.3.0",
	    time_stamp, "1.3.6.1.6.3.1.1.4.1.0", &trapoid, vb0) == NULL) {
		if (vb0 != 0)
			ober_free_elements(vb0);
		ober_free_elements(vblist);
		return NULL;
	}

	if (proxy) {
		(void)ober_string2oid("1.3.6.1.6.3.18.1.3.0",
		    &snmpTrapAddressOid);
		(void)ober_string2oid("1.3.6.1.6.3.18.1.4.0",
		    &snmpTrapCommunityOid);
		(void)ober_string2oid("1.3.6.1.6.3.1.1.4.3.0",
		    &snmpTrapEnterpriseOid);
		for (elm = vblist->be_sub; elm != NULL; elm = elm->be_next) {
			if (ober_get_oid(elm->be_sub, &oid) == -1) {
				ober_free_elements(vblist);
				return NULL;
			}
			if (ober_oid_cmp(&oid, &snmpTrapAddressOid) == 0)
				hasaddress = 1;
			else if (ober_oid_cmp(&oid, &snmpTrapCommunityOid) == 0)
				hascommunity = 1;
			else if (ober_oid_cmp(&oid,
			    &snmpTrapEnterpriseOid) == 0)
				hasenterprise = 1;
			last = elm;
		}
		if (!hasaddress || !hascommunity || !hasenterprise) {
			if (ober_printf_elements(last, "{Oxt}{Os}{OO}",
			    &snmpTrapAddressOid, agent_addr, 4,
			    BER_CLASS_APPLICATION, SNMP_T_IPADDR,
			    &snmpTrapCommunityOid, community,
			    &snmpTrapEnterpriseOid, &enterprise) == NULL) {
				ober_free_elements(vblist);
				return NULL;
			}
		}
	}
	return vblist;
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
	struct privsep		*ps = p->p_ps;
	struct snmpd		*env = ps->ps_env;
	struct ber		 ber = {0};
	struct sockaddr		*sa;
	char			*buf;
	ssize_t			 n;
	struct ber_element	*vblist;
	struct ber_oid		 trapoid;
	struct trapcmd		*cmd;
	struct passwd		*pw;
	int			 verbose;

	pw = ps->ps_pw;
	verbose = log_getverbose();

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("traphandler_fork_handler: cannot drop privileges");

	closefrom(STDERR_FILENO + 1);

	log_init((env->sc_flags & SNMPD_F_DEBUG) ? 1 : 0, LOG_DAEMON);
	log_setverbose(verbose);
	log_procinit(p->p_title);

	n = IMSG_DATA_SIZE(imsg);

	sa = imsg->data;
	n -= sa->sa_len;
	buf = (char *)imsg->data + sa->sa_len;

	ober_set_application(&ber, smi_application);
	ober_set_readbuf(&ber, buf, n);

	if ((vblist = ober_read_elements(&ber, NULL)) == NULL)
		fatalx("couldn't parse SNMP trap message");
	ober_free(&ber);

	(void)ober_scanf_elements(vblist, "{S{So", &trapoid);
	if ((cmd = trapcmd_lookup(&trapoid)) != NULL)
		trapcmd_exec(cmd, sa, vblist->be_sub);

	ober_free_elements(vblist);

	exit(0);
}

void
trapcmd_exec(struct trapcmd *cmd, struct sockaddr *sa,
    struct ber_element *vb)
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

	for (; vb != NULL; vb = vb->be_next) {
		if (ober_scanf_elements(vb, "{oe}", &oid, &elm) == -1)
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

	ret = ober_oid_cmp(cmd2->cmd_oid, cmd1->cmd_oid);
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
