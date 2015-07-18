/*	$OpenBSD: constraint.c,v 1.13 2015/07/18 20:32:38 bcook Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <tls.h>

#include "log.h"
#include "ntpd.h"

int	 constraint_addr_init(struct constraint *);
struct constraint *
	 constraint_byid(u_int32_t);
struct constraint *
	 constraint_byfd(int);
struct constraint *
	 constraint_bypid(pid_t);
int	 constraint_close(int);
void	 constraint_update(void);
void	 constraint_reset(void);
int	 constraint_cmp(const void *, const void *);

struct httpsdate *
	 httpsdate_init(const char *, const char *, const char *,
	    const char *, const u_int8_t *, size_t);
void	 httpsdate_free(void *);
int	 httpsdate_request(struct httpsdate *, struct timeval *);
void	*httpsdate_query(const char *, const char *, const char *,
	    const char *, const u_int8_t *, size_t,
	    struct timeval *, struct timeval *);

char	*tls_readline(struct tls *, size_t *, size_t *, struct timeval *);

extern u_int constraint_cnt;
extern u_int peer_cnt;

struct httpsdate {
	char			*tls_host;
	char			*tls_port;
	char			*tls_name;
	char			*tls_path;
	char			*tls_request;
	struct tls_config	*tls_config;
	struct tls		*tls_ctx;
	struct tm		 tls_tm;
};

int
constraint_init(struct constraint *cstr)
{
	cstr->state = STATE_NONE;
	cstr->fd = -1;
	cstr->last = getmonotime();
	cstr->constraint = 0;
	cstr->senderrors = 0;

	return (constraint_addr_init(cstr));
}

int
constraint_addr_init(struct constraint *cstr)
{
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct ntp_addr		*h;

	if (cstr->state == STATE_DNS_INPROGRESS)
		return (0);

	if (cstr->addr_head.a == NULL) {
		priv_dns(IMSG_CONSTRAINT_DNS, cstr->addr_head.name, cstr->id);
		cstr->state = STATE_DNS_INPROGRESS;
		return (0);
	}

	h = cstr->addr;
	switch (h->ss.ss_family) {
	case AF_INET:
		sa_in = (struct sockaddr_in *)&h->ss;
		if (ntohs(sa_in->sin_port) == 0)
			sa_in->sin_port = htons(443);
		cstr->state = STATE_DNS_DONE;
		break;
	case AF_INET6:
		sa_in6 = (struct sockaddr_in6 *)&h->ss;
		if (ntohs(sa_in6->sin6_port) == 0)
			sa_in6->sin6_port = htons(443);
		cstr->state = STATE_DNS_DONE;
		break;
	default:
		/* XXX king bula sez it? */
		fatalx("wrong AF in constraint_addr_init");
		/* NOTREACHED */
	}

	return (1);
}

int
constraint_query(struct constraint *cstr)
{
	int		 pipes[2];
	struct timeval	 rectv, xmttv;
	void		*ctx;
	static char	 hname[NI_MAXHOST];
	time_t		 now;
	struct iovec	 iov[2];

	now = getmonotime();

	switch (cstr->state) {
	case STATE_DNS_DONE:
		/* Proceed and query the time */
		break;
	case STATE_DNS_TEMPFAIL:
		/* Retry resolving the address */
		constraint_init(cstr);
		return (-1);
	case STATE_QUERY_SENT:
		if (cstr->last + CONSTRAINT_SCAN_TIMEOUT > now) {
			/* The caller should expect a reply */
			return (0);
		}

		/* Timeout, just kill the process to reset it. */
		kill(cstr->pid, SIGTERM);
		return (-1);
	case STATE_INVALID:
		if (cstr->last + CONSTRAINT_SCAN_INTERVAL > now) {
			/* Nothing to do */
			return (-1);
		}

		/* Reset and retry */
		cstr->senderrors = 0;
		constraint_close(cstr->fd);
		break;
	case STATE_REPLY_RECEIVED:
	default:
		/* Nothing to do */
		return (-1);
	}

	cstr->last = now;
	if (getnameinfo((struct sockaddr *)&cstr->addr->ss,
	    SA_LEN((struct sockaddr *)&cstr->addr->ss),
	    hname, sizeof(hname), NULL, 0,
	    NI_NUMERICHOST) != 0)
		fatalx("%s getnameinfo %s", __func__, cstr->addr_head.name);

	log_debug("constraint request to %s", hname);

	if (socketpair(AF_UNIX, SOCK_DGRAM, AF_UNSPEC, pipes) == -1)
		fatal("%s pipes", __func__);

	/* Fork child handlers */
	switch (cstr->pid = fork()) {
	case -1:
		cstr->senderrors++;
		close(pipes[0]);
		close(pipes[1]);
		return (-1);
	case 0:
		setproctitle("constraint from %s", hname);

		/* Child process */
		if (dup2(pipes[1], CONSTRAINT_PASSFD) == -1)
			fatal("%s dup2 CONSTRAINT_PASSFD", __func__);
		if (pipes[0] != CONSTRAINT_PASSFD)
			close(pipes[0]);
		if (pipes[1] != CONSTRAINT_PASSFD)
			close(pipes[1]);
		(void)closefrom(CONSTRAINT_PASSFD + 1);

		if (fcntl(CONSTRAINT_PASSFD, F_SETFD, FD_CLOEXEC) == -1)
			fatal("%s fcntl F_SETFD", __func__);

		cstr->fd = CONSTRAINT_PASSFD;
		imsg_init(&cstr->ibuf, cstr->fd);

		if ((ctx = httpsdate_query(hname,
		    CONSTRAINT_PORT, cstr->addr_head.name, cstr->addr_head.path,
		    conf->ca, conf->ca_len, &rectv, &xmttv)) == NULL) {
			/* Abort with failure but without warning */
			exit(1);
		}

		iov[0].iov_base = &rectv;
		iov[0].iov_len = sizeof(rectv);
		iov[1].iov_base = &xmttv;
		iov[1].iov_len = sizeof(xmttv);
		imsg_composev(&cstr->ibuf, IMSG_CONSTRAINT, 0, 0, -1, iov, 2);
		imsg_flush(&cstr->ibuf);

		/* Tear down the TLS connection after sending the result */
		httpsdate_free(ctx);

		_exit(0);
		/* NOTREACHED */
	default:
		/* Parent */
		close(pipes[1]);
		cstr->fd = pipes[0];
		cstr->state = STATE_QUERY_SENT;

		imsg_init(&cstr->ibuf, cstr->fd);
		break;
	}


	return (0);
}

void
constraint_check_child(void)
{
	struct constraint	*cstr;
	int			 status;
	int			 fail, sig;
	pid_t			 pid;

	do {
		pid = waitpid(WAIT_ANY, &status, WNOHANG);
		if (pid <= 0)
			continue;

		fail = sig = 0;
		if (WIFSIGNALED(status)) {
			sig = WTERMSIG(status);
		} else if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				fail = 1;
		} else
			fatalx("unexpected cause of SIGCHLD");

		if ((cstr = constraint_bypid(pid)) != NULL) {
			if (sig)
				fatalx("constraint %s, signal %d",
				    log_sockaddr((struct sockaddr *)
				    &cstr->addr->ss), sig);
			if (fail) {
				log_debug("no constraint reply from %s"
				    " received in time, next query %ds",
				    log_sockaddr((struct sockaddr *)
				    &cstr->addr->ss), CONSTRAINT_SCAN_INTERVAL);
			}

			if (fail || cstr->state < STATE_QUERY_SENT) {
				cstr->senderrors++;
				constraint_close(cstr->fd);
			}
		}
	} while (pid > 0 || (pid == -1 && errno == EINTR));
}

struct constraint *
constraint_byid(u_int32_t id)
{
	struct constraint	*cstr;

	TAILQ_FOREACH(cstr, &conf->constraints, entry) {
		if (cstr->id == id)
			return (cstr);
	}

	return (NULL);
}

struct constraint *
constraint_byfd(int fd)
{
	struct constraint	*cstr;

	TAILQ_FOREACH(cstr, &conf->constraints, entry) {
		if (cstr->fd == fd)
			return (cstr);
	}

	return (NULL);
}

struct constraint *
constraint_bypid(pid_t pid)
{
	struct constraint	*cstr;

	TAILQ_FOREACH(cstr, &conf->constraints, entry) {
		if (cstr->pid == pid)
			return (cstr);
	}

	return (NULL);
}

int
constraint_close(int fd)
{
	struct constraint	*cstr;

	if ((cstr = constraint_byfd(fd)) == NULL) {
		log_warn("%s: fd %d: not found", __func__, fd);
		return (0);
	}

	msgbuf_clear(&cstr->ibuf.w);
	close(cstr->fd);
	cstr->fd = -1;
	cstr->last = getmonotime();

	if (cstr->addr == NULL || (cstr->addr = cstr->addr->next) == NULL) {
		/* Either a pool or all addresses have been tried */
		cstr->addr = cstr->addr_head.a;
		if (cstr->senderrors)
			cstr->state = STATE_INVALID;
		else if (cstr->state >= STATE_QUERY_SENT)
			cstr->state = STATE_DNS_DONE;

		return (1);
	}

	/* Go on and try the next resolved address for this constraint */
	return (constraint_init(cstr));
}

void
constraint_add(struct constraint *cstr)
{
	TAILQ_INSERT_TAIL(&conf->constraints, cstr, entry);
}

void
constraint_remove(struct constraint *cstr)
{
	TAILQ_REMOVE(&conf->constraints, cstr, entry);
	free(cstr->addr_head.name);
	free(cstr->addr_head.path);
	free(cstr);
}

int
constraint_dispatch_msg(struct pollfd *pfd)
{
	struct imsg		 imsg;
	struct constraint	*cstr;
	ssize_t			 n;
	struct timeval		 tv[2];
	double			 offset;

	if ((cstr = constraint_byfd(pfd->fd)) == NULL)
		return (0);

	if (!(pfd->revents & POLLIN))
		return (0);

	if ((n = imsg_read(&cstr->ibuf)) == -1 || n == 0) {
		constraint_close(pfd->fd);
		return (1);
	}

	for (;;) {
		if ((n = imsg_get(&cstr->ibuf, &imsg)) == -1) {
			constraint_close(pfd->fd);
			return (1);
		}
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CONSTRAINT:
			 if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(tv))
				fatalx("invalid IMSG_CONSTRAINT received");

			memcpy(tv, imsg.data, sizeof(tv));

			offset = gettime_from_timeval(&tv[0]) -
			    gettime_from_timeval(&tv[1]);

			log_info("constraint reply from %s: offset %f",
			    log_sockaddr((struct sockaddr *)&cstr->addr->ss),
			    offset);

			cstr->state = STATE_REPLY_RECEIVED;
			cstr->last = getmonotime();
			cstr->constraint = tv[0].tv_sec;

			constraint_update();
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}

	return (0);
}

void
constraint_dns(u_int32_t id, u_int8_t *data, size_t len)
{
	struct constraint	*cstr, *ncstr = NULL;
	u_int8_t		*p;
	struct ntp_addr		*h;

	if ((cstr = constraint_byid(id)) == NULL) {
		log_warnx("IMSG_CONSTRAINT_DNS with invalid constraint id");
		return;
	}
	if (cstr->addr != NULL) {
		log_warnx("IMSG_CONSTRAINT_DNS but addr != NULL!");
		return;
	}
	if (len == 0) {
		log_debug("%s FAILED", __func__);
		cstr->state = STATE_DNS_TEMPFAIL;
		return;
	}

	if ((len % sizeof(struct sockaddr_storage)) != 0)
		fatalx("IMSG_CONSTRAINT_DNS len");

	p = data;
	do {
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal("calloc ntp_addr");
		memcpy(&h->ss, p, sizeof(h->ss));
		p += sizeof(h->ss);
		len -= sizeof(h->ss);

		if (ncstr == NULL || cstr->addr_head.pool) {
			ncstr = new_constraint();
			ncstr->addr = h;
			ncstr->addr_head.a = h;
			ncstr->addr_head.name = strdup(cstr->addr_head.name);
			ncstr->addr_head.path = strdup(cstr->addr_head.path);
			if (ncstr->addr_head.name == NULL ||
			    ncstr->addr_head.path == NULL)
				fatal("calloc name");
			ncstr->addr_head.pool = cstr->addr_head.pool;
			ncstr->state = STATE_DNS_DONE;
			constraint_add(ncstr);
			constraint_cnt += constraint_init(ncstr);
		} else {
			h->next = ncstr->addr;
			ncstr->addr = h;
			ncstr->addr_head.a = h;
		}
	} while (len);

	constraint_remove(cstr);
}

int
constraint_cmp(const void *a, const void *b)
{
	return (*(const time_t *)a - *(const time_t *)b);
}

void
constraint_update(void)
{
	struct constraint *cstr;
	int	 cnt, i;
	time_t	*sum;
	time_t	 now;

	now = getmonotime();

	cnt = 0;
	TAILQ_FOREACH(cstr, &conf->constraints, entry) {
		if (cstr->state != STATE_REPLY_RECEIVED)
			continue;
		cnt++;
	}

	if ((sum = calloc(cnt, sizeof(time_t))) == NULL)
		fatal("calloc");

	i = 0;
	TAILQ_FOREACH(cstr, &conf->constraints, entry) {
		if (cstr->state != STATE_REPLY_RECEIVED)
			continue;
		sum[i++] = cstr->constraint + (now - cstr->last);
	}

	qsort(sum, cnt, sizeof(time_t), constraint_cmp);

	/* calculate median */
	i = cnt / 2;
	if (cnt % 2 == 0)
		if (sum[i - 1] < sum[i])
			i -= 1;

	conf->constraint_last = now;
	conf->constraint_median = sum[i];

	free(sum);
}

void
constraint_reset(void)
{
	struct constraint *cstr;

	TAILQ_FOREACH(cstr, &conf->constraints, entry) {
		if (cstr->state == STATE_QUERY_SENT)
			continue;
		constraint_close(cstr->fd);
	}
	conf->constraint_errors = 0;
}

int
constraint_check(double val)
{
	struct timeval	tv;
	double		constraint;
	time_t		now;

	if (conf->constraint_median == 0)
		return (0);

	/* Calculate the constraint with the current offset */
	now = getmonotime();
	tv.tv_sec = conf->constraint_median + (now - conf->constraint_last);
	tv.tv_usec = 0;
	constraint = gettime_from_timeval(&tv);

	if (((val - constraint) > CONSTRAINT_MARGIN) ||
	    ((constraint - val) > CONSTRAINT_MARGIN)) {
		/* XXX get new constraint if too many errors happened */
		if (conf->constraint_errors++ >
		    (CONSTRAINT_ERROR_MARGIN * peer_cnt)) {
			constraint_reset();
		}

		return (-1);
	}

	return (0);
}

struct httpsdate *
httpsdate_init(const char *hname, const char *port, const char *name,
    const char *path, const u_int8_t *ca, size_t ca_len)
{
	struct httpsdate	*httpsdate = NULL;

	if (tls_init() == -1)
		return (NULL);

	if ((httpsdate = calloc(1, sizeof(*httpsdate))) == NULL)
		goto fail;

	if (name == NULL)
		name = hname;

	if ((httpsdate->tls_host = strdup(hname)) == NULL ||
	    (httpsdate->tls_port = strdup(port)) == NULL ||
	    (httpsdate->tls_name = strdup(name)) == NULL ||
	    (httpsdate->tls_path = strdup(path)) == NULL)
		goto fail;

	if (asprintf(&httpsdate->tls_request,
	    "HEAD %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
	    httpsdate->tls_path, httpsdate->tls_name) == -1)
		goto fail;

	if ((httpsdate->tls_config = tls_config_new()) == NULL)
		goto fail;

	if (tls_config_set_ciphers(httpsdate->tls_config, "compat") != 0)
		goto fail;

	/* XXX we have to pre-resolve, so name and host are not equal */
	tls_config_insecure_noverifyname(httpsdate->tls_config);

	if (ca == NULL || ca_len == 0)
		tls_config_insecure_noverifycert(httpsdate->tls_config);
	else
		tls_config_set_ca_mem(httpsdate->tls_config, ca, ca_len);

	return (httpsdate);

 fail:
	httpsdate_free(httpsdate);
	return (NULL);
}

void
httpsdate_free(void *arg)
{
	struct httpsdate *httpsdate = arg;
	if (httpsdate == NULL)
		return;
	if (httpsdate->tls_ctx)
		tls_close(httpsdate->tls_ctx);
	tls_free(httpsdate->tls_ctx);
	tls_config_free(httpsdate->tls_config);
	free(httpsdate->tls_host);
	free(httpsdate->tls_port);
	free(httpsdate->tls_name);
	free(httpsdate->tls_path);
	free(httpsdate->tls_request);
	free(httpsdate);
}

int
httpsdate_request(struct httpsdate *httpsdate, struct timeval *when)
{
	size_t	 outlen = 0, maxlength = CONSTRAINT_MAXHEADERLENGTH;
	char	*line, *p;

	if ((httpsdate->tls_ctx = tls_client()) == NULL)
		goto fail;

	if (tls_configure(httpsdate->tls_ctx, httpsdate->tls_config) == -1)
		goto fail;

	if (tls_connect(httpsdate->tls_ctx,
	    httpsdate->tls_host, httpsdate->tls_port) == -1) {
		log_debug("tls failed: %s: %s", httpsdate->tls_host,
		    tls_error(httpsdate->tls_ctx));
		goto fail;
	}

	if (tls_write(httpsdate->tls_ctx,
	    httpsdate->tls_request, strlen(httpsdate->tls_request),
	    &outlen) == -1)
		goto fail;

	while ((line = tls_readline(httpsdate->tls_ctx, &outlen,
	    &maxlength, when)) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';

		if ((p = strchr(line, ' ')) == NULL || *p == '\0')
			goto next;
		*p++ = '\0';
		if (strcasecmp("Date:", line) != 0)
			goto next;

		/*
		 * Expect the date/time format as IMF-fixdate which is
		 * mandated by HTTP/1.1 in the new RFC 7231 and was
		 * preferred by RFC 2616.  Other formats would be RFC 850
		 * or ANSI C's asctime() - the latter doesn't include
		 * the timezone which is required here.
		 */
		if (strptime(p, "%a, %d %h %Y %T %Z",
		    &httpsdate->tls_tm) == NULL) {
			log_warnx("unsupported date format");
			free(line);
			return (-1);
		}

		free(line);
		break;
 next:
		free(line);
	}


	return (0);
 fail:
	httpsdate_free(httpsdate);
	return (-1);
}

void *
httpsdate_query(const char *hname, const char *port, const char *name,
    const char *path, const u_int8_t *ca, size_t ca_len,
    struct timeval *rectv, struct timeval *xmttv)
{
	struct httpsdate	*httpsdate;
	struct timeval		 when;
	time_t			 t;

	if ((httpsdate = httpsdate_init(hname, port, name, path,
	    ca, ca_len)) == NULL)
		return (NULL);

	if (httpsdate_request(httpsdate, &when) == -1)
		return (NULL);

	/* Return parsed date as local time */
	t = timegm(&httpsdate->tls_tm);

	/* Report parsed Date: as "received time" */
	rectv->tv_sec = t;
	rectv->tv_usec = 0;

	/* And add delay as "transmit time" */
	xmttv->tv_sec = when.tv_sec;
	xmttv->tv_usec = when.tv_usec;

	return (httpsdate);
}

/* Based on SSL_readline in ftp/fetch.c */
char *
tls_readline(struct tls *tls, size_t *lenp, size_t *maxlength,
    struct timeval *when)
{
	size_t i, len, nr;
	char *buf, *q, c;
	int ret;

	len = 128;
	if ((buf = malloc(len)) == NULL)
		fatal("Can't allocate memory for transfer buffer");
	for (i = 0; ; i++) {
		if (i >= len - 1) {
			if ((q = reallocarray(buf, len, 2)) == NULL)
				fatal("Can't expand transfer buffer");
			buf = q;
			len *= 2;
		}
 again:
		ret = tls_read(tls, &c, 1, &nr);
		if (ret == TLS_READ_AGAIN)
			goto again;
		if (ret != 0) {
			/* SSL read error, ignore */
			free(buf);
			return (NULL);
		}

		if (maxlength != NULL && (*maxlength)-- == 0) {
			log_warnx("maximum length exceeded");
			return (NULL);
		}

		buf[i] = c;
		if (c == '\n')
			break;
	}
	*lenp = i;
	if (gettimeofday(when, NULL) == -1)
		fatal("gettimeofday");
	return (buf);
}
