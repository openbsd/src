/*	$OpenBSD: privsep.c,v 1.33 2015/03/15 00:41:28 millert Exp $	*/

/*
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <net/bpf.h>
#include <net/pfvar.h>

#include <rpc/rpc.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "interface.h"
#include "privsep.h"
#include "pfctl_parser.h"

/*
 * tcpdump goes through four states: STATE_INIT is where the
 * bpf device and the input file is opened. In STATE_BPF, the
 * pcap filter gets set. STATE_FILTER is used for parsing
 * /etc/services and /etc/protocols and opening the output
 * file. STATE_RUN is the packet processing part.
 */

enum priv_state {
	STATE_INIT,		/* initial state */
	STATE_BPF,		/* input file/device opened */
	STATE_FILTER,		/* filter applied */
	STATE_RUN		/* running and accepting network traffic */
};

#define ALLOW(action)	(1 << (action))

/*
 * Set of maximum allowed actions.
 */
static const int allowed_max[] = {
	/* INIT */	ALLOW(PRIV_OPEN_BPF) | ALLOW(PRIV_OPEN_DUMP) |
			ALLOW(PRIV_SETFILTER),
	/* BPF */	ALLOW(PRIV_SETFILTER),
	/* FILTER */	ALLOW(PRIV_OPEN_OUTPUT) | ALLOW(PRIV_GETSERVENTRIES) |
			ALLOW(PRIV_GETPROTOENTRIES) |
			ALLOW(PRIV_ETHER_NTOHOST) | ALLOW(PRIV_INIT_DONE),
	/* RUN */	ALLOW(PRIV_GETHOSTBYADDR) | ALLOW(PRIV_ETHER_NTOHOST) |
			ALLOW(PRIV_GETRPCBYNUMBER) | ALLOW(PRIV_GETLINES) |
			ALLOW(PRIV_LOCALTIME)
};

/*
 * Default set of allowed actions. More actions get added
 * later depending on the supplied parameters.
 */
static int allowed_ext[] = {
	/* INIT */	ALLOW(PRIV_SETFILTER),
	/* BPF */	ALLOW(PRIV_SETFILTER),
	/* FILTER */	ALLOW(PRIV_GETSERVENTRIES),
	/* RUN */	ALLOW(PRIV_GETLINES) | ALLOW(PRIV_LOCALTIME)
};

struct ftab {
	char *name;
	int max;
	int count;
};

static struct ftab file_table[] = {{"/etc/appletalk.names", 1, 0},
				   {PF_OSFP_FILE, 1, 0}};

#define NUM_FILETAB (sizeof(file_table) / sizeof(struct ftab))

int		debug_level = LOG_INFO;
int		priv_fd = -1;
volatile	pid_t child_pid = -1;
static volatile	sig_atomic_t cur_state = STATE_INIT;

extern void	set_slave_signals(void);

static void	impl_open_bpf(int, int *);
static void	impl_open_dump(int, const char *);
static void	impl_open_output(int, const char *);
static void	impl_setfilter(int, char *, int *);
static void	impl_init_done(int, int *);
static void	impl_gethostbyaddr(int);
static void	impl_ether_ntohost(int);
static void	impl_getrpcbynumber(int);
static void	impl_getserventries(int);
static void	impl_getprotoentries(int);
static void	impl_localtime(int fd);
static void	impl_getlines(int);

static void	test_state(int, int);
static void	logmsg(int, const char *, ...);

int
priv_init(int argc, char **argv)
{
	int bpfd = -1;
	int i, socks[2], cmd, nflag = 0;
	struct passwd *pw;
	uid_t uid;
	gid_t gid;
	char *cmdbuf, *infile = NULL;
	char *RFileName = NULL;
	char *WFileName = NULL;
	sigset_t allsigs, oset;

	if (geteuid() != 0)
		errx(1, "need root privileges");

	closefrom(STDERR_FILENO + 1);
	for (i = 1; i < _NSIG; i++)
		signal(i, SIG_DFL);

	/* Create sockets */
	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, socks) == -1)
		err(1, "socketpair() failed");

	sigfillset(&allsigs);
	sigprocmask(SIG_BLOCK, &allsigs, &oset);

	child_pid = fork();
	if (child_pid < 0)
		err(1, "fork() failed");

	if (child_pid) {
		/* Parent, drop privileges to _tcpdump */
		pw = getpwnam("_tcpdump");
		if (pw == NULL)
			errx(1, "unknown user _tcpdump");

		/* chroot, drop privs and return */
		if (chroot(pw->pw_dir) != 0)
			err(1, "unable to chroot");
		if (chdir("/") != 0)
			err(1, "unable to chdir");

		/* drop to _tcpdump */
		if (setgroups(1, &pw->pw_gid) == -1)
			err(1, "setgroups() failed");
		if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
			err(1, "setresgid() failed");
		if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
			err(1, "setresuid() failed");
		endpwent();

		close(socks[0]);
		priv_fd = socks[1];

		set_slave_signals();
		sigprocmask(SIG_SETMASK, &oset, NULL);

		return (0);
	}

	sigprocmask(SIG_SETMASK, &oset, NULL);

	/* Child - drop suid privileges */
	gid = getgid();
	uid = getuid();

	if (setresgid(gid, gid, gid) == -1)
		err(1, "setresgid() failed");
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid() failed");

	/* parse the arguments for required options */
	opterr = 0;
	while ((i = getopt(argc, argv,
	    "ac:D:deE:fF:i:lLnNOopqr:s:StT:vw:xXy:Y")) != -1) {
		switch (i) {
		case 'n':
			nflag++;
			break;

		case 'r':
			RFileName = optarg;
			break;

		case 'w':
			WFileName = optarg;
			break;

		case 'F':
			infile = optarg;
			break;

		default:
			/* nothing */
			break;
		}
	}

	if (RFileName != NULL) {
		if (strcmp(RFileName, "-") != 0)
			allowed_ext[STATE_INIT] |= ALLOW(PRIV_OPEN_DUMP);
	} else
		allowed_ext[STATE_INIT] |= ALLOW(PRIV_OPEN_BPF);
	if (WFileName != NULL) {
		if (strcmp(WFileName, "-") != 0)
			allowed_ext[STATE_FILTER] |= ALLOW(PRIV_OPEN_OUTPUT);
		else
			allowed_ext[STATE_FILTER] |= ALLOW(PRIV_INIT_DONE);
	} else
		allowed_ext[STATE_FILTER] |= ALLOW(PRIV_INIT_DONE);
	if (!nflag) {
		allowed_ext[STATE_RUN] |= ALLOW(PRIV_GETHOSTBYADDR);
		allowed_ext[STATE_FILTER] |= ALLOW(PRIV_ETHER_NTOHOST);
		allowed_ext[STATE_RUN] |= ALLOW(PRIV_ETHER_NTOHOST);
		allowed_ext[STATE_RUN] |= ALLOW(PRIV_GETRPCBYNUMBER);
		allowed_ext[STATE_FILTER] |= ALLOW(PRIV_GETPROTOENTRIES);
	}

	if (infile)
		cmdbuf = read_infile(infile);
	else
		cmdbuf = copy_argv(&argv[optind]);

	setproctitle("[priv]");
	close(socks[1]);

	for (;;) {
		if (may_read(socks[0], &cmd, sizeof(int)))
			break;
		switch (cmd) {
		case PRIV_OPEN_BPF:
			test_state(cmd, STATE_BPF);
			impl_open_bpf(socks[0], &bpfd);
			break;
		case PRIV_OPEN_DUMP:
			test_state(cmd, STATE_BPF);
			impl_open_dump(socks[0], RFileName);
			break;
		case PRIV_OPEN_OUTPUT:
			test_state(cmd, STATE_RUN);
			impl_open_output(socks[0], WFileName);
			break;
		case PRIV_SETFILTER:
			test_state(cmd, STATE_FILTER);
			impl_setfilter(socks[0], cmdbuf, &bpfd);
			break;
		case PRIV_INIT_DONE:
			test_state(cmd, STATE_RUN);
			impl_init_done(socks[0], &bpfd);
			break;
		case PRIV_GETHOSTBYADDR:
			test_state(cmd, STATE_RUN);
			impl_gethostbyaddr(socks[0]);
			break;
		case PRIV_ETHER_NTOHOST:
			test_state(cmd, cur_state);
			impl_ether_ntohost(socks[0]);
			break;
		case PRIV_GETRPCBYNUMBER:
			test_state(cmd, STATE_RUN);
			impl_getrpcbynumber(socks[0]);
			break;
		case PRIV_GETSERVENTRIES:
			test_state(cmd, STATE_FILTER);
			impl_getserventries(socks[0]);
			break;
		case PRIV_GETPROTOENTRIES:
			test_state(cmd, STATE_FILTER);
			impl_getprotoentries(socks[0]);
			break;
		case PRIV_LOCALTIME:
			test_state(cmd, STATE_RUN);
			impl_localtime(socks[0]);
			break;
		case PRIV_GETLINES:
			test_state(cmd, STATE_RUN);
			impl_getlines(socks[0]);
			break;
		default:
			logmsg(LOG_ERR, "[priv]: unknown command %d", cmd);
			_exit(1);
			/* NOTREACHED */
		}
	}

	/* NOTREACHED */
	_exit(0);
}

static void
impl_open_bpf(int fd, int *bpfd)
{
	int snaplen, promisc, err;
	u_int dlt, dirfilt;
	char device[IFNAMSIZ];
	size_t iflen;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_OPEN_BPF received");

	must_read(fd, &snaplen, sizeof(int));
	must_read(fd, &promisc, sizeof(int));
	must_read(fd, &dlt, sizeof(u_int));
	must_read(fd, &dirfilt, sizeof(u_int));
	iflen = read_string(fd, device, sizeof(device), __func__);
	if (iflen == 0)
		errx(1, "Invalid interface size specified");
	*bpfd = pcap_live(device, snaplen, promisc, dlt, dirfilt);
	err = errno;
	if (*bpfd < 0)
		logmsg(LOG_DEBUG,
		    "[priv]: failed to open bpf device for %s: %s",
		    device, strerror(errno));
	send_fd(fd, *bpfd);
	must_write(fd, &err, sizeof(int));
	/* do not close bpfd until filter is set */
}

static void
impl_open_dump(int fd, const char *RFileName)
{
	int file, err = 0;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_OPEN_DUMP received");

	if (RFileName == NULL) {
		file = -1;
		logmsg(LOG_ERR, "[priv]: No offline file specified");
	} else {
		file = open(RFileName, O_RDONLY, 0);
		err = errno;
		if (file < 0)
			logmsg(LOG_DEBUG, "[priv]: failed to open %s: %s",
			    RFileName, strerror(errno));
	}
	send_fd(fd, file);
	must_write(fd, &err, sizeof(int));
	if (file >= 0)
		close(file);
}

static void
impl_open_output(int fd, const char *WFileName)
{
	int file, err;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_OPEN_OUTPUT received");

	file = open(WFileName, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	err = errno;
	send_fd(fd, file);
	must_write(fd, &err, sizeof(int));
	if (file < 0)
		logmsg(LOG_DEBUG, "[priv]: failed to open %s: %s",
		    WFileName, strerror(err));
	else
		close(file);
}

static void
impl_setfilter(int fd, char *cmdbuf, int *bpfd)
{
	logmsg(LOG_DEBUG, "[priv]: msg PRIV_SETFILTER received");

	if (setfilter(*bpfd, fd, cmdbuf))
		logmsg(LOG_DEBUG, "[priv]: setfilter() failed");
	close(*bpfd);	/* done with bpf descriptor */
	*bpfd = -1;
}

static void
impl_init_done(int fd, int *bpfd)
{
	int ret;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_INIT_DONE received");

	close(*bpfd);	/* done with bpf descriptor */
	*bpfd = -1;
	ret = 0;
	must_write(fd, &ret, sizeof(ret));
}

static void
impl_gethostbyaddr(int fd)
{
	char hostname[HOST_NAME_MAX+1];
	size_t hostname_len;
	int addr_af;
	struct hostent *hp;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_GETHOSTBYADDR received");

	/* Expecting: address block, address family */
	hostname_len = read_block(fd, hostname, sizeof(hostname), __func__);
	if (hostname_len == 0)
		_exit(1);
	must_read(fd, &addr_af, sizeof(int));
	hp = gethostbyaddr(hostname, hostname_len, addr_af);
	if (hp == NULL)
		write_zero(fd);
	else
		write_string(fd, hp->h_name);
}

static void
impl_ether_ntohost(int fd)
{
	struct ether_addr ether;
	char hostname[HOST_NAME_MAX+1];

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_ETHER_NTOHOST received");

	/* Expecting: ethernet address */
	must_read(fd, &ether, sizeof(ether));
	if (ether_ntohost(hostname, &ether) == -1)
		write_zero(fd);
	else
		write_string(fd, hostname);
}

static void
impl_getrpcbynumber(int fd)
{
	int rpc;
	struct rpcent *rpce;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_GETRPCBYNUMBER received");

	must_read(fd, &rpc, sizeof(int));
	rpce = getrpcbynumber(rpc);
	if (rpce == NULL)
		write_zero(fd);
	else
		write_string(fd, rpce->r_name);
}

static void
impl_getserventries(int fd)
{
	struct servent *sp;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_GETSERVENTRIES received");

	for (;;) {
		sp = getservent();
		if (sp == NULL) {
			write_zero(fd);
			break;
		} else {
			write_string(fd, sp->s_name);
			must_write(fd, &sp->s_port, sizeof(int));
			write_string(fd, sp->s_proto);
		}
	}
	endservent();
}

static void
impl_getprotoentries(int fd)
{
	struct protoent *pe;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_GETPROTOENTRIES received");

	for (;;) {
		pe = getprotoent();
		if (pe == NULL) {
			write_zero(fd);
			break;
		} else {
			write_string(fd, pe->p_name);
			must_write(fd, &pe->p_proto, sizeof(int));
		}
	}
	endprotoent();
}

/* read the time and send the corresponding localtime and gmtime
 * results back to the unprivileged process */
static void
impl_localtime(int fd)
{
	struct tm *lt, *gt;
	time_t t;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_LOCALTIME received");

	must_read(fd, &t, sizeof(time_t));

	/* this must be done separately, since they apparently use the
	 * same local buffer */
	if ((lt = localtime(&t)) == NULL)
		errx(1, "localtime()");
	must_write(fd, lt, sizeof(*lt));

	if ((gt = gmtime(&t)) == NULL)
		errx(1, "gmtime()");
	must_write(fd, gt, sizeof(*gt));

	if (lt->tm_zone == NULL)
		write_zero(fd);
	else
		write_string(fd, lt->tm_zone);
}

static void
impl_getlines(int fd)
{
	FILE *fp;
	char *buf, *lbuf, *file;
	size_t len, fid;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_GETLINES received");

	must_read(fd, &fid, sizeof(size_t));
	if (fid >= NUM_FILETAB)
		errx(1, "invalid file id");

	file = file_table[fid].name;

	if (file == NULL)
		errx(1, "invalid file referenced");

	if (file_table[fid].count >= file_table[fid].max)
		errx(1, "maximum open count exceeded for %s", file);

	file_table[fid].count++;

	if ((fp = fopen(file, "r")) == NULL) {
		write_zero(fd);
		return;
	}

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = (char *)malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		write_string(fd, buf);

		if (lbuf != NULL) {
			free(lbuf);
			lbuf = NULL;
		}
	}
	write_zero(fd);
	fclose(fp);
}

void
priv_init_done(void)
{
	int ret;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", __func__);

	write_command(priv_fd, PRIV_INIT_DONE);
	must_read(priv_fd, &ret, sizeof(int));
}

/* Reverse address resolution; response is placed into res, and length of
 * response is returned (zero on error) */
size_t
priv_gethostbyaddr(char *addr, size_t addr_len, int af, char *res, size_t res_len)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	write_command(priv_fd, PRIV_GETHOSTBYADDR);
	write_block(priv_fd, addr_len, addr);
	must_write(priv_fd, &af, sizeof(int));

	return (read_string(priv_fd, res, res_len, __func__));
}

size_t
priv_ether_ntohost(char *name, size_t name_len, struct ether_addr *e)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	write_command(priv_fd, PRIV_ETHER_NTOHOST);
	must_write(priv_fd, e, sizeof(*e));

	/* Read the host name */
	return (read_string(priv_fd, name, name_len, __func__));
}

size_t
priv_getrpcbynumber(int rpc, char *progname, size_t progname_len)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	write_command(priv_fd, PRIV_GETRPCBYNUMBER);
	must_write(priv_fd, &rpc, sizeof(int));

	return read_string(priv_fd, progname, progname_len, __func__);
}

/* start getting service entries */
void
priv_getserventries(void)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	write_command(priv_fd, PRIV_GETSERVENTRIES);
}

/* retrieve a service entry, should be called repeatedly after calling
   priv_getserventries(), until it returns zero. */
size_t
priv_getserventry(char *name, size_t name_len, int *port, char *prot,
    size_t prot_len)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	/* read the service name */
	if (read_string(priv_fd, name, name_len, __func__) == 0)
		return 0;

	/* read the port */
	must_read(priv_fd, port, sizeof(int));

	/* read the protocol */
	return (read_string(priv_fd, prot, prot_len, __func__));
}

/* start getting ip protocol entries */
void
priv_getprotoentries(void)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	write_command(priv_fd, PRIV_GETPROTOENTRIES);
}

/* retrieve a ip protocol entry, should be called repeatedly after calling
   priv_getprotoentries(), until it returns zero. */
size_t
priv_getprotoentry(char *name, size_t name_len, int *num)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	/* read the proto name */
	if (read_string(priv_fd, name, name_len, __func__) == 0)
		return 0;

	/* read the num */
	must_read(priv_fd, num, sizeof(int));

	return (1);
}

/* localtime() replacement: ask the privileged process for localtime and
 * gmtime, cache the localtime for about one minute i.e. until one of the
 * fields other than seconds changes. The check is done using gmtime
 * values since they are the same in parent and child. */
struct	tm *
priv_localtime(const time_t *t)
{
	static struct tm lt, gt0;
	static struct tm *gt = NULL;
	static char zone[PATH_MAX];

	if (gt != NULL) {
		gt = gmtime(t);
		gt0.tm_sec = gt->tm_sec;
		gt0.tm_zone = gt->tm_zone;

		if (memcmp(gt, &gt0, sizeof(struct tm)) == 0) {
			lt.tm_sec = gt0.tm_sec;
			return &lt;
		}
	}

	write_command(priv_fd, PRIV_LOCALTIME);
	must_write(priv_fd, t, sizeof(time_t));
	must_read(priv_fd, &lt, sizeof(lt));
	must_read(priv_fd, &gt0, sizeof(gt0));

	if (read_string(priv_fd, zone, sizeof(zone), __func__))
		lt.tm_zone = zone;
	else
		lt.tm_zone = NULL;

	gt0.tm_zone = NULL;
	gt = &gt0;

	return &lt;
}

/* start getting lines from a file */
void
priv_getlines(size_t sz)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	write_command(priv_fd, PRIV_GETLINES);
	must_write(priv_fd, &sz, sizeof(size_t));
}

/* retrieve a line from a file, should be called repeatedly after calling
   priv_getlines(), until it returns zero. */
size_t
priv_getline(char *line, size_t line_len)
{
	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	/* read the line */
	return (read_string(priv_fd, line, line_len, __func__));
}

/* Read all data or return 1 for error. */
int
may_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			return (1);
		default:
			pos += res;
		}
	}
	return (0);
}

/* Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh. */
void
must_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/* Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh. */
void
must_write(int fd, const void *buf, size_t n)
{
	const char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = write(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			/* FALLTHROUGH */
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/* test for a given state, and possibly increase state */
static void
test_state(int action, int next)
{
	if (cur_state < 0 || cur_state > STATE_RUN) {
		logmsg(LOG_ERR, "[priv] Invalid state: %d", cur_state);
		_exit(1);
	}
	if ((allowed_max[cur_state] & allowed_ext[cur_state]
	    & ALLOW(action)) == 0) {
		logmsg(LOG_ERR, "[priv] Invalid action %d in state %d",
		    action, cur_state);
		_exit(1);
	}
	if (next < cur_state) {
		logmsg(LOG_ERR, "[priv] Invalid next state: %d < %d",
		    next, cur_state);
		_exit(1);
	}

	cur_state = next;
}

static void
logmsg(int pri, const char *message, ...)
{
	va_list ap;
	if (pri > debug_level)
		return;
	va_start(ap, message);

	vfprintf(stderr, message, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

/* write a command to the peer */
void
write_command(int fd, int cmd)
{
	must_write(fd, &cmd, sizeof(cmd));
}

/* write a zero 'length' to signal an error to read_{string|block} */
void
write_zero(int fd)
{
	size_t len = 0;
	must_write(fd, &len, sizeof(size_t));
}

/* send a string */
void
write_string(int fd, const char *str)
{
	size_t len;

	len = strlen(str) + 1;
	must_write(fd, &len, sizeof(size_t));
	must_write(fd, str, len);
}

/* send a block of data of given size */
void
write_block(int fd, size_t size, const char *str)
{
	must_write(fd, &size, sizeof(size_t));
	must_write(fd, str, size);
}

/* read a string from the channel, return 0 if error, or total size of
 * the buffer, including the terminating '\0' */
size_t
read_string(int fd, char *buf, size_t size, const char *func)
{
	size_t len;

	len = read_block(fd, buf, size, func);
	if (len == 0)
		return (0);

	if (buf[len - 1] != '\0')
		errx(1, "%s: received invalid string", func);

	return (len);
}

/* read a block of data from the channel, return length of data, or 0
 * if error */
size_t
read_block(int fd, char *buf, size_t size, const char *func)
{
	size_t len;
	/* Expect back an integer size, and then a string of that length */
	must_read(fd, &len, sizeof(size_t));

	/* Check there was no error (indicated by a return of 0) */
	if (len == 0)
		return (0);

	/* Make sure we aren't overflowing the passed in buffer */
	if (size < len)
		errx(1, "%s: overflow attempt in return", func);

	/* Read the string and make sure we got all of it */
	must_read(fd, buf, len);
	return (len);
}
