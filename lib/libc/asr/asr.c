/*	$OpenBSD: asr.c,v 1.3 2012/04/15 22:25:14 eric Exp $	*/
/*
 * Copyright (c) 2010-2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr.h"
#include "asr_private.h"
#include "thread_private.h"

#define DEFAULT_CONFFILE	"/etc/resolv.conf"
#define DEFAULT_HOSTFILE	"/etc/hosts"
#define DEFAULT_CONF		"lookup bind file\nnameserver 127.0.0.1\n"
#define DEFAULT_LOOKUP		"lookup bind file"

#define RELOAD_DELAY		15 /* seconds */

static void asr_check_reload(struct asr *);
static struct asr_ctx *asr_ctx_create(void);
static void asr_ctx_ref(struct asr_ctx *);
static void asr_ctx_free(struct asr_ctx *);
static int asr_ctx_add_searchdomain(struct asr_ctx *, const char *);
static int asr_ctx_from_file(struct asr_ctx *, const char *);
static int asr_ctx_from_string(struct asr_ctx *, const char *);
static int asr_ctx_parse(const char*, int(*)(char**, int, struct asr_ctx*),
    struct asr_ctx *);
static int asr_parse_nameserver(struct sockaddr *, const char *);
static char *asr_hostalias(const char *, char *, size_t);
static int asr_ndots(const char *);
static void asr_ctx_envopts(struct asr_ctx *);
static int pass0(char **, int, struct asr_ctx *);

static void *__THREAD_NAME(_asr);
static struct asr *_asr = NULL;

/* Allocate and configure an async "resolver". */
struct asr *
async_resolver(const char *conf)
{
	static int	 init = 0;
	struct asr	*asr;

#ifdef DEBUG
	if (init == 0) {
		if (getenv("ASR_DEBUG"))
			asr_debug = 1;
		init = 1;
	}
#endif
	if ((asr = calloc(1, sizeof(*asr))) == NULL)
		goto fail;

	/* If not setuid/setgid, allow to use an alternate config. */
	if (conf == NULL && !issetugid())
		conf = getenv("ASR_CONFIG");

	if (conf == NULL)
		conf = DEFAULT_CONFFILE;

	if (conf[0] == '!') {
		/* Use the rest of the string as config file */
		if ((asr->a_ctx = asr_ctx_create()) == NULL)
			goto fail;
		if (asr_ctx_from_string(asr->a_ctx, conf + 1) == -1)
			goto fail;
	} else {
		/* Use the given config file */
		asr->a_path = strdup(conf);
		asr_check_reload(asr);
		if (asr->a_ctx == NULL) {
			if ((asr->a_ctx = asr_ctx_create()) == NULL)
				goto fail;
			if (asr_ctx_from_string(asr->a_ctx, DEFAULT_CONF) == -1)
				goto fail;
			asr_ctx_envopts(asr->a_ctx);
		}
	}

#ifdef DEBUG
	asr_dump(asr);
#endif
	return (asr);

    fail:
	if (asr) {
		if (asr->a_ctx)
			asr_ctx_free(asr->a_ctx);
		free(asr);
	}

	return (NULL);
}

/*
 * Free the "asr" async resolver (or the thread-local resolver if NULL).
 * Drop the reference to the current context.
 */
void
async_resolver_done(struct asr *asr)
{
	struct asr **priv;

	if (asr == NULL) {
		priv = _THREAD_PRIVATE(_asr, asr, &_asr);
		if (*priv == NULL)
			return;
		asr = *priv;
		*priv = NULL;
	}

	asr_ctx_unref(asr->a_ctx);
	if (asr->a_path)
		free(asr->a_path);
	free(asr);
}

/*
 * Cancel an async query.
 */
void
async_abort(struct async *as)
{
	async_free(as);
}

/*
 * Resume the "as" async query resolution.  Return one of ASYNC_COND,
 * ASYNC_YIELD or ASYNC_DONE and put query-specific return values in
 * the user-allocated memory at "ar".
 */
int
async_run(struct async *as, struct async_res *ar)
{
	int	r;

#ifdef DEBUG
	asr_printf("asr: async_run(%p, %p) %s ctx=[%p]\n",
		as, ar, asr_querystr(as->as_type), as->as_ctx);
#endif
	r = as->as_run(as, ar);

#ifdef DEBUG
	if (asr_debug) {
		asr_printf("asr: async_run(%p, %p) -> %s", as, ar,
		    asr_transitionstr(r));
		if (r == ASYNC_COND)
			asr_printf(" fd=%i timeout=%i\n",
			    ar->ar_fd, ar->ar_timeout);
		else
			asr_printf("\n");
		fflush(stderr);
	}
#endif
	if (r == ASYNC_DONE)
		async_free(as);

	return (r);
}

/*
 * Same as above, but run in a loop that handles the fd conditions result.
 */
int
async_run_sync(struct async *as, struct async_res *ar)
{
	struct pollfd		 fds[1];
	int			 r;

	while((r = async_run(as, ar)) == ASYNC_COND) {
		fds[0].fd = ar->ar_fd;
		fds[0].events = (ar->ar_cond == ASYNC_READ) ? POLLIN : POLLOUT;
	again:
		r = poll(fds, 1, ar->ar_timeout);
		if (r == -1 && errno == EINTR)
			goto again;
		if (r == -1) /* XXX Is it possible? and what to do if so? */
			err(1, "poll");
	}

	return (r);
}

/*
 * Create a new async request of the given "type" on the async context "ac".
 * Take a reference on it so it does not gets deleted while the async query
 * is running.
 */
struct async *
async_new(struct asr_ctx *ac, int type)
{
	struct async	*as;
#ifdef DEBUG
	asr_printf("asr: async_new(ctx=%p) type=%i refcount=%i\n",
	    ac, type, ac->ac_refcount);
#endif
	if ((as = calloc(1, sizeof(*as))) == NULL)
		return (NULL);

	ac->ac_refcount += 1;
	as->as_ctx = ac;
	as->as_fd = -1;
	as->as_type = type;
	as->as_state = ASR_STATE_INIT;

	return (as);
}

/*
 * Free an async query and unref the associated context.
 */
void
async_free(struct async *as)
{
#ifdef DEBUG
	asr_printf("asr: async_free(%p)\n", as);
#endif
	switch(as->as_type) {
	case ASR_SEND:
		if (as->as_fd != -1)
			close(as->as_fd);
		if (as->as.dns.obuf && !(as->as.dns.flags & ASYNC_EXTOBUF))
			free (as->as.dns.obuf);
		if (as->as.dns.ibuf && !(as->as.dns.flags & ASYNC_EXTIBUF))
			free (as->as.dns.ibuf);
		if (as->as.dns.dname)
			free(as->as.dns.dname);
		break;

	case ASR_SEARCH:
		if (as->as.search.subq)
			async_free(as->as.search.subq);
		if (as->as.search.name)
			free(as->as.search.name);
		break;

	case ASR_GETRRSETBYNAME:
		if (as->as.rrset.subq)
			async_free(as->as.rrset.subq);
		if (as->as.rrset.name)
			free(as->as.rrset.name);
		break;

	case ASR_GETHOSTBYNAME:
	case ASR_GETHOSTBYADDR:
		if (as->as.hostnamadr.subq)
			async_free(as->as.hostnamadr.subq);
		if (as->as.hostnamadr.name)
			free(as->as.hostnamadr.name);
		if (as->as.hostnamadr.dname)
			free(as->as.hostnamadr.dname);
		break;

	case ASR_GETNETBYNAME:
	case ASR_GETNETBYADDR:
		if (as->as.netnamadr.subq)
			async_free(as->as.netnamadr.subq);
		if (as->as.netnamadr.name)
			free(as->as.netnamadr.name);
		break;

	case ASR_GETADDRINFO:
		if (as->as.ai.subq)
			async_free(as->as.ai.subq);
		if (as->as.ai.aifirst)
			freeaddrinfo(as->as.ai.aifirst);
		if (as->as.ai.hostname)
			free(as->as.ai.hostname);
		if (as->as.ai.servname)
			free(as->as.ai.servname);
		break;

	case ASR_GETNAMEINFO:
		if (as->as.ni.subq)
			async_free(as->as.ni.subq);
		break;

	case ASR_HOSTADDR:
		if (as->as.host.name)
			free(as->as.host.name);
		if (as->as.host.subq)
			async_free(as->as.host.subq);
		if (as->as.host.pkt)
			free(as->as.host.pkt);
		if (as->as.host.file)
			fclose(as->as.host.file);
		break;
	}

	asr_ctx_unref(as->as_ctx);
	free(as);
}

/*
 * Get a context from the given resolver. This takes a new reference to
 * the returned context, which *must* be explicitely dropped when done
 * using this context.
 */
struct asr_ctx *
asr_use_resolver(struct asr *asr)
{
	struct asr **priv;

	if (asr == NULL) {
		/* Use the thread-local resolver. */
#ifdef DEBUG
		asr_printf("using thread-local resolver\n");
#endif
		priv = _THREAD_PRIVATE(_asr, asr, &_asr);
		if (*priv == NULL) {
#ifdef DEBUG
			asr_printf("setting up thread-local resolver\n");
#endif
			*priv = async_resolver(NULL);
		}
		asr = *priv;
	}

	asr_check_reload(asr);
	asr_ctx_ref(asr->a_ctx);
	return (asr->a_ctx);
}

static void
asr_ctx_ref(struct asr_ctx *ac)
{
#ifdef DEBUG
	asr_printf("asr: asr_ctx_ref(ctx=%p) refcount=%i\n",
	    ac, ac->ac_refcount);
#endif
	ac->ac_refcount += 1;
}

/*
 * Drop a reference to an async context, freeing it if the reference
 * count drops to 0.
 */
void
asr_ctx_unref(struct asr_ctx *ac)
{
#ifdef DEBUG
	asr_printf("asr: asr_ctx_unref(ctx=%p) refcount=%i\n",
	    ac, ac->ac_refcount);
#endif
	if (--ac->ac_refcount)
		return;
	
	asr_ctx_free(ac);
}

static void
asr_ctx_free(struct asr_ctx *ac)
{
	int i;

	if (ac->ac_domain)
		free(ac->ac_domain);
	for(i = 0; i < ac->ac_nscount; i++)
		free(ac->ac_ns[i]);
	for(i = 0; i < ac->ac_domcount; i++)
		free(ac->ac_dom[i]);

	free(ac);
}

/*
 * Reload the configuration file if it has changed on disk.
 */
static void
asr_check_reload(struct asr *asr)
{
        struct stat	 st;
	struct asr_ctx	*ac;
	struct timespec	 tp;

	if (asr->a_path == NULL)
		return;

	if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1)
		return;

	if ((tp.tv_sec - asr->a_rtime) < RELOAD_DELAY)
		return;
	asr->a_rtime = tp.tv_sec;

#ifdef DEBUG
	asr_printf("asr: checking for update of \"%s\"\n", asr->a_path);
#endif

	if (stat(asr->a_path, &st) == -1 ||
	    asr->a_mtime == st.st_mtime ||
	    (ac = asr_ctx_create()) == NULL)
		return;
	asr->a_mtime = st.st_mtime;

#ifdef DEBUG
	asr_printf("asr: reloading config file\n");
#endif

	if (asr_ctx_from_file(ac, asr->a_path) == -1) {
		asr_ctx_free(ac);
		return;
	}

	asr_ctx_envopts(ac);
	if (asr->a_ctx)
		asr_ctx_unref(asr->a_ctx);
	asr->a_ctx = ac;
}

/* 
 * Construct a fully-qualified domain name for the given name and domain.
 * If "name" ends with a '.' it is considered as a FQDN by itself.
 * Otherwise, the domain, which must be a FQDN, is appended to "name" (it
 * may have a leading dot which would be ignored). If the domain is null,
 * then "." is used. Return the length of the constructed FQDN or (0) on
 * error.
 */
size_t
asr_make_fqdn(const char *name, const char *domain, char *buf, size_t buflen)
{
	size_t	len;

	if (domain == NULL)
		domain = ".";
	else if ((len = strlen(domain)) == 0)
		return (0);
	else if (domain[len -1] != '.')
		return (0);

	len = strlen(name);
	if (len == 0) {
		strlcpy(buf, domain, buflen);
	} else if (name[len - 1] !=  '.') {
		if (domain[0] == '.')
			domain += 1;
		strlcpy(buf, name, buflen);
		strlcat(buf, ".", buflen);
		strlcat(buf, domain, buflen);
	} else {
		strlcpy(buf, name, buflen);
	}

	return (strlen(buf));
}

/*
 * Concatenate a name and a domain name. The result has no trailing dot.
 */
size_t
asr_domcat(const char *name, const char *domain, char *buf, size_t buflen)
{
	size_t	r;

	r = asr_make_fqdn(name, domain, buf, buflen);
	if (r == 0)
		return (0);
	buf[r - 1] = '\0';

	return (r - 1);
}

/*
 * Count the dots in a string.
 */
static int
asr_ndots(const char *s)
{
	int n;

	for(n = 0; *s; s++)
		if (*s == '.')
			n += 1;

	return (n);
}

/* 
 * Allocate a new empty context.
 */
static struct asr_ctx *
asr_ctx_create(void)
{
	struct asr_ctx	*ac;

	if ((ac = calloc(1, sizeof(*ac))) == NULL)
		return (NULL);

	ac->ac_options = RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
	ac->ac_refcount = 1;
	ac->ac_ndots = 1;
	ac->ac_family[0] = AF_INET;
	ac->ac_family[1] = AF_INET6;
	ac->ac_family[2] = -1;

	ac->ac_hostfile = DEFAULT_HOSTFILE;

	ac->ac_nscount = 0;
	ac->ac_nstimeout = 1000;
	ac->ac_nsretries = 3;

	return (ac);
}

/*
 * Add a search domain to the async context.
 */
static int
asr_ctx_add_searchdomain(struct asr_ctx *ac, const char *domain)
{
	char buf[MAXDNAME];

	if (ac->ac_domcount == ASR_MAXDOM)
		return (-1);

	if (asr_make_fqdn(domain, NULL, buf, sizeof(buf)) == 0)
		return (-1);

	if ((ac->ac_dom[ac->ac_domcount] = strdup(buf)) == NULL)
		return (0);

	ac->ac_domcount += 1;

	return (1);
}

/*
 * Pass on a split config line.
 */
static int
pass0(char **tok, int n, struct asr_ctx *ac)
{
	int		 i, j, d;
	const char	*e;
	struct sockaddr_storage	ss;

	if (!strcmp(tok[0], "nameserver")) {
		if (ac->ac_nscount == ASR_MAXNS)
			return (0);
		if (n != 2)
			return (0);
		if (asr_parse_nameserver((struct sockaddr*)&ss, tok[1]))
			return (0);
		if ((ac->ac_ns[ac->ac_nscount] = calloc(1, ss.ss_len)) == NULL)
			return (0);
		memmove(ac->ac_ns[ac->ac_nscount], &ss, ss.ss_len);
		ac->ac_nscount += 1;

	} else if (!strcmp(tok[0], "domain")) {
		if (n != 2)
			return (0);
		if (ac->ac_domain)
			return (0);
		ac->ac_domain = strdup(tok[1]);

	} else if (!strcmp(tok[0], "lookup")) {
		/* ignore the line if we already set lookup */
		if (ac->ac_dbcount != 0)
			return (0);
		if (n - 1 > ASR_MAXDB)
			return (0);
		/* ensure that each lookup is only given once */
		for(i = 1; i < n; i++)
			for(j = i + 1; j < n; j++)
				if (!strcmp(tok[i], tok[j]))
					return (0);
		for(i = 1; i < n; i++, ac->ac_dbcount++) {
			if (!strcmp(tok[i], "yp")) {
				ac->ac_db[i-1] = ASR_DB_YP;
			} else if (!strcmp(tok[i], "bind")) {
				ac->ac_db[i-1] = ASR_DB_DNS;
			} else if (!strcmp(tok[i], "file")) {
				ac->ac_db[i-1] = ASR_DB_FILE;
			} else {
				/* ignore the line */
				ac->ac_dbcount = 0;
				return (0);
			}
		}
	} else if (!strcmp(tok[0], "search")) {
		/* resolv.conf says the last line wins */
		for(i = 0; i < ac->ac_domcount; i++)
			free(ac->ac_dom[i]);
		ac->ac_domcount = 0;
		for(i = 1; i < n; i++)
			asr_ctx_add_searchdomain(ac, tok[i]);

	} else if (!strcmp(tok[0], "family")) {
		if (n == 1 || n > 3)
			return (0);
		for (i = 1; i < n; i++)
			if (strcmp(tok[i], "inet4") && strcmp(tok[i], "inet6"))
				return (0);
		for (i = 1; i < n; i++)
			ac->ac_family[i - 1] = strcmp(tok[i], "inet4") ? \
			    AF_INET6 : AF_INET;
		ac->ac_family[i - 1] = -1;

	} else if (!strcmp(tok[0], "options")) {
		for(i = 1; i < n; i++) {
			if (!strcmp(tok[i], "tcp"))
				ac->ac_options |= RES_USEVC;
			else if ((!strncmp(tok[i], "ndots:", 6))) {
				e = NULL;
				d = strtonum(tok[i] + 6, 1, 16, &e);
				if (e == NULL)
					ac->ac_ndots = d;
			}
		}
	}

	return (0);
}

/*
 * Setup an async context with the config specified in the string "str".
 */
static int
asr_ctx_from_string(struct asr_ctx *ac, const char *str)
{
	char		 buf[512], *ch;

	asr_ctx_parse(str, pass0, ac);

	if (ac->ac_dbcount == 0) {
		/* No lookup directive */
		asr_ctx_parse(DEFAULT_LOOKUP, pass0, ac);
	}

	if (ac->ac_nscount == 0)
		asr_ctx_parse("nameserver 127.0.0.1", pass0, ac);

	if (ac->ac_domain == NULL)
		if (gethostname(buf, sizeof buf) == 0) {
			ch = strchr(buf, '.');
			if (ch)
				ac->ac_domain = strdup(ch + 1);
			else /* Assume root. see resolv.conf(5) */
				ac->ac_domain = strdup("");
		}

	/* If no search domain was specified, use the local subdomains */
	if (ac->ac_domcount == 0)
		for(ch = ac->ac_domain; ch; ) {
			asr_ctx_add_searchdomain(ac, ch);
			ch = strchr(ch, '.');
			if (ch && asr_ndots(++ch) == 0)
				break;
		}

	return (0);
}

/*
 * Setup the "ac" async context from the file at location "path".
 */
static int
asr_ctx_from_file(struct asr_ctx *ac, const char *path)
{
	FILE	*cf;
	char	 buf[4096];
	ssize_t	 r;

	cf = fopen(path, "r");
	if (cf == NULL)
		return (-1);

	r = fread(buf, 1, sizeof buf - 1, cf);
	if (feof(cf) == 0) {
#ifdef DEBUG
		asr_printf("asr: config file too long: \"%s\"\n", path);
#endif
		r = -1;
	}
	fclose(cf);
	if (r == -1)
		return (-1);
	buf[r] = '\0';
 
	return asr_ctx_from_string(ac, buf);
}

/*
 * Parse a configuration string.  Lines are read one by one, comments are
 * stripped and the remaining line is split into tokens which are passed
 * to the "cb" callback function.  Parsing stops if the callback returns
 * non-zero.
 */
static int
asr_ctx_parse(const char *str, int (*cb)(char**, int, struct asr_ctx*),
    struct asr_ctx *ac)
{
	size_t		 len;
	const char	*line;
	char		 buf[1024];
	char		*tok[10], **tp, *cp;
	int		 ntok;

	line = str;
	while (*line) {
		len = strcspn(line, "\n\0");
		if (len < sizeof buf) {
			memmove(buf, line, len);
			buf[len] = '\0';
		} else
			buf[0] = '\0';
		line += len;
		if (*line == '\n')
			line++;
		buf[strcspn(buf, ";#")] = '\0';
		for(cp = buf, tp = tok, ntok = 0;
		    tp < &tok[10] && (*tp = strsep(&cp, " \t")) != NULL; )
			if (**tp != '\0') {
				tp++;
				ntok++;
			}
		*tp = NULL;

		if (tok[0] == NULL)
			continue;

		if (cb(tok, ntok, ac))
			break;
	}

	return (0);
}

/*
 * Check for environment variables altering the configuration as described
 * in resolv.conf(5).  Altough not documented there, this feature is disabled
 * for setuid/setgid programs.
 */
static void
asr_ctx_envopts(struct asr_ctx *ac)
{
	char	buf[4096], *e;
	size_t	s;

	if (issetugid()) {
		ac->ac_options |= RES_NOALIASES;
		return;
	}

	if ((e = getenv("RES_OPTIONS")) != NULL) {
		strlcpy(buf, "options ", sizeof buf);
		strlcat(buf, e, sizeof buf);
		s = strlcat(buf, "\n", sizeof buf);
		s = strlcat(buf, "\n", sizeof buf);
		if (s < sizeof buf)
			asr_ctx_parse(buf, pass0, ac);
	}

	if ((e = getenv("LOCALDOMAIN")) != NULL) {
		strlcpy(buf, "search ", sizeof buf);
		strlcat(buf, e, sizeof buf);
		s = strlcat(buf, "\n", sizeof buf);
		if (s < sizeof buf)
			asr_ctx_parse(buf, pass0, ac);
	}
}

/*
 * Parse a resolv.conf(5) nameserver string into a sockaddr.
 */ 
static int
asr_parse_nameserver(struct sockaddr *sa, const char *s)
{
	const char	*estr;
	char		 buf[256];
	char		*port = NULL;
	in_port_t	 portno = 53;

	if (*s == '[') {
		strlcpy(buf, s + 1, sizeof buf);
		s = buf;
		port = strchr(buf, ']');
		if (port == NULL)
			return (-1);
		*port++ = '\0';
		if (*port != ':')
			return (-1);
		port++;
	}
	
	if (port) {
		portno = strtonum(port, 1, USHRT_MAX, &estr);
		if (estr)
			return (-1);
	}

	if (sockaddr_from_str(sa, PF_UNSPEC, s) == -1)
		return (-1);

	if (sa->sa_family == PF_INET)
		((struct sockaddr_in *)sa)->sin_port = htons(portno);
	else if (sa->sa_family == PF_INET6)
		((struct sockaddr_in6 *)sa)->sin6_port = htons(portno);

	return (0);
}

/*
 * Turn a (uncompressed) DNS domain name into a regular nul-terminated string
 * where labels are separated by dots. The result is put into the "buf" buffer,
 * truncated if it exceeds "max" chars. The function returns "buf".
 */
char*
asr_strdname(const char *_dname, char *buf, size_t max)
{
	const unsigned char *dname = _dname;
	char	*res;
	size_t	 left, n, count;

	if (_dname[0] == 0) {
		strlcpy(buf, ".", max);
		return buf;
	}

	res = buf;
	left = max - 1;
	for (n = 0; dname[0] && left; n += dname[0]) {
		count = (dname[0] < (left - 1)) ? dname[0] : (left - 1);
		memmove(buf, dname + 1, count);
		dname += dname[0] + 1;
		left -= count;
		buf += count;
		if (left) {
			left -= 1;
			*buf++ = '.';
		}
	}
	buf[0] = 0;

	return (res);
}

/*
 * Read and split the next line from the given namedb file.
 * Return -1 on error, or put the result in the "tokens" array of
 * size "ntoken" and returns the number of token on the line.
 */
int
asr_parse_namedb_line(FILE *file, char **tokens, int ntoken)
{
	size_t	  len;
	char	 *buf, *cp, **tp;
	int	  ntok;

  again:
	if ((buf = fgetln(file, &len)) == NULL)
		return (-1);

	if (buf[len - 1] == '\n')
		len--;

	buf[len] = '\0';
	buf[strcspn(buf, "#")] = '\0';
	for(cp = buf, tp = tokens, ntok = 0;
	    ntok < ntoken && (*tp = strsep(&cp, " \t")) != NULL;)
		if (**tp != '\0') {
			tp++;
			ntok++;
		}
	*tp = NULL;
	if (tokens[0] == NULL)
		goto again;

	return (ntok);
}

/*
 * Update the async context so that it uses the next configured DB.
 * Return 0 on success, or -1 if no more DBs is available.
 */
int
asr_iter_db(struct async *as)
{
	if (as->as_db_idx >= as->as_ctx->ac_dbcount) {
#ifdef DEBUG
		asr_printf("asr_iter_db: done\n");
#endif
		return (-1);
	}

	as->as_db_idx += 1;
	as->as_ns_idx = 0;
#ifdef DEBUG
	asr_printf("asr_iter_db: %i\n", as->as_db_idx);
#endif
	return (0);
}

/*
 * Set the async context nameserver index to the next nameserver of the
 * currently used DB (assuming it is DNS), cycling over the list until the
 * maximum retry counter is reached.  Return 0 on success, or -1 if all
 * nameservers were used.
 */
int
asr_iter_ns(struct async *as)
{
	for (;;) {
		if (as->as_ns_cycles >= as->as_ctx->ac_nsretries)
			return (-1);

		as->as_ns_idx += 1;
		if (as->as_ns_idx <= as->as_ctx->ac_nscount)
			break;
		as->as_ns_idx = 0;
		as->as_ns_cycles++;
#ifdef DEBUG
		asr_printf("asr: asr_iter_ns(): cycle %i\n", as->as_ns_cycles);
#endif
	}

	return (0);
}

enum {
	DOM_INIT,
	DOM_DOMAIN,
	DOM_DONE
};

/*
 * Implement the search domain strategy.
 *
 * This function works as a generator that constructs complete domains in
 * buffer "buf" of size "len" for the given host name "name", according to the
 * search rules defined by the resolving context.  It is supposed to be called
 * multiple times (with the same name) to generate the next possible domain
 * name, if any.
 *
 * It returns 0 if it could generate a new domain name, or -1 when all
 * possibilites have been exhausted.
 */
int
asr_iter_domain(struct async *as, const char *name, char * buf, size_t len)
{
	char	*alias;

	switch(as->as_dom_step) {

	case DOM_INIT:
		/* First call */

		/*
		 * If "name" is an FQDN, that's the only result and we
		 * don't try anything else.
		 */
		if (strlen(name) && name[strlen(name) - 1] ==  '.') {
#ifdef DEBUG
			asr_printf("asr: asr_iter_domain(\"%s\") fqdn\n", name);
#endif
			as->as_dom_flags |= ASYNC_DOM_FQDN;
			as->as_dom_step = DOM_DONE;
			return (asr_domcat(name, NULL, buf, len));
		}

		/*
		 * If "name" has no dots, it might be an alias. If so,
		 * That's also the only result.
		 */
		if ((as->as_ctx->ac_options & RES_NOALIASES) == 0 &&
		    asr_ndots(name) == 0 &&
		    (alias = asr_hostalias(name, buf, len)) != NULL) {
#ifdef DEBUG
			asr_printf("asr: asr_iter_domain(\"%s\") is alias "
			    "\"%s\"\n", name, alias);
#endif
			as->as_dom_flags |= ASYNC_DOM_HOSTALIAS;
			as->as_dom_step = DOM_DONE;
			return (asr_domcat(alias, NULL, buf, len));
		}

		/*
		 * Otherwise, we iterate through the specified search domains.
		 */
		as->as_dom_step = DOM_DOMAIN;
		as->as_dom_idx = 0;

		/*
		 * If "name" as enough dots, use it as-is first, as indicated
		 * in resolv.conf(5).
		 */
		if ((asr_ndots(name)) >= as->as_ctx->ac_ndots) {
#ifdef DEBUG
			asr_printf("asr: asr_iter_domain(\"%s\") ndots\n",
			    name);
#endif
			as->as_dom_flags |= ASYNC_DOM_NDOTS;
			strlcpy(buf, name, len);
			return (0);
		}
		/* Otherwise, starts using the search domains */
		/* FALLTHROUGH */

	case DOM_DOMAIN:
		if (as->as_dom_idx < as->as_ctx->ac_domcount) {
#ifdef DEBUG
			asr_printf("asr: asr_iter_domain(\"%s\") "
			    "domain \"%s\"\n", name,
			    as->as_ctx->ac_dom[as->as_dom_idx]);
#endif
			as->as_dom_flags |= ASYNC_DOM_DOMAIN;
			return (asr_domcat(name,
			    as->as_ctx->ac_dom[as->as_dom_idx++], buf, len));
		}

		/* No more domain to try. */

		as->as_dom_step = DOM_DONE;

		/*
		 * If the name was not tried as an absolute name before,
		 * do it now.
		 */
		if (!(as->as_dom_flags & ASYNC_DOM_NDOTS)) {
#ifdef DEBUG
			asr_printf("asr: asr_iter_domain(\"%s\") as is\n",
			    name);
#endif
			as->as_dom_flags |= ASYNC_DOM_ASIS;
			strlcpy(buf, name, len);
			return (0);
		}
		/* Otherwise, we are done. */

	case DOM_DONE:
	default:
#ifdef DEBUG
		asr_printf("asr: asr_iter_domain(\"%s\") done\n", name);
#endif
		return (-1);
	}	
}

/*
 * Check if the hostname "name" is a user-defined alias as per hostname(7).
 * If so, copies the result in the buffer "abuf" of size "abufsz" and
 * return "abuf". Otherwise return NULL.
 */
static char *
asr_hostalias(const char *name, char *abuf, size_t abufsz)
{
	FILE	 *fp;
	size_t	  len;
	char	 *file, *buf, *cp, **tp, *tokens[2];
	int	  ntok;

	file = getenv("HOSTALIASES");
	if (file == NULL || issetugid() != 0 || (fp = fopen(file, "r")) == NULL)
		return (NULL);

#ifdef DEBUG
	asr_printf("asr: looking up aliases in \"%s\"\n", file);
#endif

	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			len--;
		buf[len] = '\0';
		for(cp = buf, tp = tokens, ntok = 0;
		    ntok < 2 && (*tp = strsep(&cp, " \t")) != NULL; )
			if (**tp != '\0') {
				tp++;
				ntok++;
			}
		if (ntok != 2)
			continue;
		if (!strcasecmp(tokens[0], name)) {
			if (strlcpy(abuf, tokens[1], abufsz) > abufsz)
				continue;
#ifdef DEBUG
			asr_printf("asr: found alias \"%s\"\n", abuf);
#endif
			fclose(fp);
			return (abuf);
		}
	}

	fclose(fp);
	return (NULL);
}
