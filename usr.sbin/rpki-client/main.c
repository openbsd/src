/*	$OpenBSD: main.c,v 1.142 2021/06/03 15:10:05 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/resource.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <syslog.h>
#include <unistd.h>
#include <imsg.h>

#include "extern.h"
#include "version.h"

/*
 * Maximum number of TAL files we'll load.
 */
#define	TALSZ_MAX	8

size_t	entity_queue;
int	timeout = 60*60;
volatile sig_atomic_t killme;
void	suicide(int sig);

static struct filepath_tree	fpt = RB_INITIALIZER(&fpt);
static struct msgbuf		procq, rsyncq, httpq, rrdpq;
static int			cachefd, outdirfd;

const char	*bird_tablename = "ROAS";

int	verbose;
int	noop;
int	rrdpon;

struct stats	 stats;

/*
 * Log a message to stderr if and only if "verbose" is non-zero.
 * This uses the err(3) functionality.
 */
void
logx(const char *fmt, ...)
{
	va_list		 ap;

	if (verbose && fmt != NULL) {
		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
}

void
entity_free(struct entity *ent)
{

	if (ent == NULL)
		return;

	free(ent->pkey);
	free(ent->file);
	free(ent->descr);
	free(ent);
}

/*
 * Read a queue entity from the descriptor.
 * Matched by entity_buffer_req().
 * The pointer must be passed entity_free().
 */
void
entity_read_req(int fd, struct entity *ent)
{

	io_simple_read(fd, &ent->type, sizeof(enum rtype));
	io_str_read(fd, &ent->file);
	io_simple_read(fd, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_read_alloc(fd, (void **)&ent->pkey, &ent->pkeysz);
	io_str_read(fd, &ent->descr);
}

/*
 * Write the queue entity.
 * Matched by entity_read_req().
 */
static void
entity_write_req(const struct entity *ent)
{
	struct ibuf *b;

	if (filepath_add(&fpt, ent->file) == 0) {
		warnx("%s: File already visited", ent->file);
		return;
	}

	if ((b = ibuf_dynamic(sizeof(*ent), UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &ent->type, sizeof(ent->type));
	io_str_buffer(b, ent->file);
	io_simple_buffer(b, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_buffer(b, ent->pkey, ent->pkeysz);
	io_str_buffer(b, ent->descr);
	ibuf_close(&procq, b);
}

/*
 * Scan through all queued requests and see which ones are in the given
 * repo, then flush those into the parser process.
 */
void
entityq_flush(struct entityq *q, struct repo *rp)
{
	struct entity	*p, *np;

	TAILQ_FOREACH_SAFE(p, q, entries, np) {
		char *file = p->file;

		/*
		 * XXX fixup path here since the repo may change
		 * during load because of fallback. In that case
		 * the file path changes as well since RRDP and RSYNC
		 * can not share a common repo.
		 */
		p->file = repo_filename(rp, file);
		if (p->file == NULL)
			err(1, "can't construct repo filename");
		free(file);

		entity_write_req(p);
		TAILQ_REMOVE(q, p, entries);
		entity_free(p);
	}
}

/*
 * Add the heap-allocated file to the queue for processing.
 */
static void
entityq_add(char *file, enum rtype type, struct repo *rp,
    const unsigned char *pkey, size_t pkeysz, char *descr)
{
	struct entity	*p;

	if ((p = calloc(1, sizeof(struct entity))) == NULL)
		err(1, NULL);

	p->type = type;
	p->file = file;
	p->has_pkey = pkey != NULL;
	if (p->has_pkey) {
		p->pkeysz = pkeysz;
		if ((p->pkey = malloc(pkeysz)) == NULL)
			err(1, NULL);
		memcpy(p->pkey, pkey, pkeysz);
	}
	if (descr != NULL)
		if ((p->descr = strdup(descr)) == NULL)
			err(1, NULL);

	entity_queue++;

	/*
	 * Write to the queue if there's no repo or the repo has already
	 * been loaded else enqueue it for later.
	 */

	if (rp == NULL || !repo_queued(rp, p)) {
		/*
		 * XXX fixup path here since for repo path the
		 * file path has not yet been fixed here.
		 * This is a quick way to make this work but in
		 * the long run repos need to be passed to the parser.
		 */
		if (rp != NULL) {
			file = p->file;
			p->file = repo_filename(rp, file);
			if (p->file == NULL)
				err(1, "can't construct repo filename from %s",
				    file);
			free(file);
		}
		entity_write_req(p);
		entity_free(p);
	}
}

static void
rrdp_file_resp(size_t id, int ok)
{
	enum rrdp_msg type = RRDP_FILE;
	struct ibuf *b;

	if ((b = ibuf_open(sizeof(type) + sizeof(id) + sizeof(ok))) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &ok, sizeof(ok));
	ibuf_close(&rrdpq, b);
}

void
rrdp_fetch(size_t id, const char *uri, const char *local,
    struct rrdp_session *s)
{
	enum rrdp_msg type = RRDP_START;
	struct ibuf *b;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, local);
	io_str_buffer(b, uri);
	io_str_buffer(b, s->session_id);
	io_simple_buffer(b, &s->serial, sizeof(s->serial));
	io_str_buffer(b, s->last_mod);
	ibuf_close(&rrdpq, b);
}

/*
 * Request a repository sync via rsync URI to directory local.
 */
void
rsync_fetch(size_t id, const char *uri, const char *local)
{
	struct ibuf	*b;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, local);
	io_str_buffer(b, uri);
	ibuf_close(&rsyncq, b);
}

/*
 * Request a file from a https uri, data is written to the file descriptor fd.
 */
void
http_fetch(size_t id, const char *uri, const char *last_mod, int fd)
{
	struct ibuf	*b;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, uri);
	io_str_buffer(b, last_mod);
	/* pass file as fd */
	b->fd = fd;
	ibuf_close(&httpq, b);
}

/*
 * Request some XML file on behalf of the rrdp parser.
 * Create a pipe and pass the pipe endpoints to the http and rrdp process.
 */
static void
rrdp_http_fetch(size_t id, const char *uri, const char *last_mod)
{
	enum rrdp_msg type = RRDP_HTTP_INI;
	struct ibuf *b;
	int pi[2];

	if (pipe2(pi, O_CLOEXEC | O_NONBLOCK) == -1)
		err(1, "pipe");

	if ((b = ibuf_open(sizeof(type) + sizeof(id))) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	b->fd = pi[0];
	ibuf_close(&rrdpq, b);

	http_fetch(id, uri, last_mod, pi[1]);
}

void
rrdp_http_done(size_t id, enum http_result res, const char *last_mod)
{
	enum rrdp_msg type = RRDP_HTTP_FIN;
	struct ibuf *b;

	/* RRDP request, relay response over to the rrdp process */
	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &res, sizeof(res));
	io_str_buffer(b, last_mod);
	ibuf_close(&rrdpq, b);
}

/*
 * Add a file (CER, ROA, CRL) from an MFT file, RFC 6486.
 * These are always relative to the directory in which "mft" sits.
 */
static void
queue_add_from_mft(const char *mft, const struct mftfile *file, enum rtype type)
{
	char		*cp, *nfile;

	/* Construct local path from filename. */
	cp = strrchr(mft, '/');
	assert(cp != NULL);
	assert(cp - mft < INT_MAX);
	if (asprintf(&nfile, "%.*s/%s", (int)(cp - mft), mft, file->file) == -1)
		err(1, NULL);

	/*
	 * Since we're from the same directory as the MFT file, we know
	 * that the repository has already been loaded.
	 */

	entityq_add(nfile, type, NULL, NULL, 0, NULL);
}

/*
 * Loops over queue_add_from_mft() for all files.
 * The order here is important: we want to parse the revocation
 * list *before* we parse anything else.
 * FIXME: set the type of file in the mftfile so that we don't need to
 * keep doing the check (this should be done in the parser, where we
 * check the suffix anyway).
 */
static void
queue_add_from_mft_set(const struct mft *mft)
{
	size_t			 i, sz;
	const struct mftfile	*f;

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl") != 0)
			continue;
		queue_add_from_mft(mft->file, f, RTYPE_CRL);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl") == 0)
			continue;
		else if (strcasecmp(f->file + sz - 4, ".cer") == 0)
			queue_add_from_mft(mft->file, f, RTYPE_CER);
		else if (strcasecmp(f->file + sz - 4, ".roa") == 0)
			queue_add_from_mft(mft->file, f, RTYPE_ROA);
		else if (strcasecmp(f->file + sz - 4, ".gbr") == 0)
			queue_add_from_mft(mft->file, f, RTYPE_GBR);
		else
			logx("%s: unsupported file type: %s", mft->file,
			    f->file);
	}
}

/*
 * Add a local TAL file (RFC 7730) to the queue of files to fetch.
 */
static void
queue_add_tal(const char *file)
{
	char	*nfile, *buf;

	if ((nfile = strdup(file)) == NULL)
		err(1, NULL);
	buf = tal_read_file(file);

	/* Record tal for later reporting */
	if (stats.talnames == NULL) {
		if ((stats.talnames = strdup(file)) == NULL)
			err(1, NULL);
	} else {
		char *tmp;

		if (asprintf(&tmp, "%s %s", stats.talnames, file) == -1)
			err(1, NULL);
		free(stats.talnames);
		stats.talnames = tmp;
	}

	/* Not in a repository, so directly add to queue. */
	entityq_add(nfile, RTYPE_TAL, NULL, NULL, 0, buf);
	/* entityq_add makes a copy of buf */
	free(buf);
}

/*
 * Add URIs (CER) from a TAL file, RFC 8630.
 */
static void
queue_add_from_tal(struct tal *tal)
{
	struct repo	*repo;

	assert(tal->urisz);

	/* Look up the repository. */
	repo = ta_lookup(tal);

	entityq_add(NULL, RTYPE_CER, repo, tal->pkey,
	    tal->pkeysz, tal->descr);
}

/*
 * Add a manifest (MFT) found in an X509 certificate, RFC 6487.
 */
static void
queue_add_from_cert(const struct cert *cert)
{
	struct repo	*repo;
	char		*nfile;

	repo = repo_lookup(cert->repo, rrdpon ? cert->notify : NULL);
	if (repo == NULL) {
		warnx("%s: repository lookup failed", cert->repo);
		return;
	}

	if ((nfile = strdup(cert->mft)) == NULL)
		err(1, NULL);
	entityq_add(nfile, RTYPE_MFT, repo, NULL, 0, NULL);
}

/*
 * Process parsed content.
 * For non-ROAs, we grok for more data.
 * For ROAs, we want to extract the valid info.
 * In all cases, we gather statistics.
 */
static void
entity_process(int proc, struct stats *st, struct vrp_tree *tree)
{
	enum rtype	type;
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	int		 c;

	/*
	 * For most of these, we first read whether there's any content
	 * at all---this means that the syntactic parse failed (X509
	 * certificate, for example).
	 * We follow that up with whether the resources didn't parse.
	 */
	io_simple_read(proc, &type, sizeof(type));

	switch (type) {
	case RTYPE_TAL:
		st->tals++;
		tal = tal_read(proc);
		queue_add_from_tal(tal);
		tal_free(tal);
		break;
	case RTYPE_CER:
		st->certs++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->certs_fail++;
			break;
		}
		cert = cert_read(proc);
		if (cert->valid) {
			/*
			 * Process the revocation list from the
			 * certificate *first*, since it might mark that
			 * we're revoked and then we don't want to
			 * process the MFT.
			 */
			queue_add_from_cert(cert);
		} else
			st->certs_invalid++;
		cert_free(cert);
		break;
	case RTYPE_MFT:
		st->mfts++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->mfts_fail++;
			break;
		}
		mft = mft_read(proc);
		if (mft->stale)
			st->mfts_stale++;
		queue_add_from_mft_set(mft);
		mft_free(mft);
		break;
	case RTYPE_CRL:
		st->crls++;
		break;
	case RTYPE_ROA:
		st->roas++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->roas_fail++;
			break;
		}
		roa = roa_read(proc);
		if (roa->valid)
			roa_insert_vrps(tree, roa, &st->vrps, &st->uniqs);
		else
			st->roas_invalid++;
		roa_free(roa);
		break;
	case RTYPE_GBR:
		st->gbrs++;
		break;
	default:
		errx(1, "unknown entity type");
	}

	entity_queue--;
}

/*
 * Assign filenames ending in ".tal" in "/etc/rpki" into "tals",
 * returning the number of files found and filled-in.
 * This may be zero.
 * Don't exceded "max" filenames.
 */
static size_t
tal_load_default(const char *tals[], size_t max)
{
	static const char *confdir = "/etc/rpki";
	size_t s = 0;
	char *path;
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir(confdir);
	if (dirp == NULL)
		err(1, "open %s", confdir);
	while ((dp = readdir(dirp)) != NULL) {
		if (fnmatch("*.tal", dp->d_name, FNM_PERIOD) == FNM_NOMATCH)
			continue;
		if (s >= max)
			err(1, "too many tal files found in %s",
			    confdir);
		if (asprintf(&path, "%s/%s", confdir, dp->d_name) == -1)
			err(1, NULL);
		tals[s++] = path;
	}
	closedir(dirp);
	return s;
}

void
suicide(int sig __attribute__((unused)))
{
	killme = 1;
}

#define NPFD	4

int
main(int argc, char *argv[])
{
	int		 rc, c, st, proc, rsync, http, rrdp, ok,
			 hangup = 0, fl = SOCK_STREAM | SOCK_CLOEXEC;
	size_t		 i, id, outsz = 0, talsz = 0;
	pid_t		 pid, procpid, rsyncpid, httppid, rrdppid;
	int		 fd[2];
	struct pollfd	 pfd[NPFD];
	struct msgbuf	*queues[NPFD];
	struct roa	**out = NULL;
	char		*rsync_prog = "openrsync";
	char		*bind_addr = NULL;
	const char	*cachedir = NULL, *outputdir = NULL;
	const char	*tals[TALSZ_MAX], *errs, *name;
	struct vrp_tree	 v = RB_INITIALIZER(&v);
	struct rusage	ru;
	struct timeval	start_time, now_time;

	gettimeofday(&start_time, NULL);

	/* If started as root, priv-drop to _rpki-client */
	if (getuid() == 0) {
		struct passwd *pw;

		pw = getpwnam("_rpki-client");
		if (!pw)
			errx(1, "no _rpki-client user to revoke to");
		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
			err(1, "unable to revoke privs");
	}
	cachedir = RPKI_PATH_BASE_DIR;
	outputdir = RPKI_PATH_OUT_DIR;

	if (pledge("stdio rpath wpath cpath inet fattr dns sendfd recvfd "
	    "proc exec unveil", NULL) == -1)
		err(1, "pledge");

	while ((c = getopt(argc, argv, "b:Bcd:e:jnorRs:t:T:vV")) != -1)
		switch (c) {
		case 'b':
			bind_addr = optarg;
			break;
		case 'B':
			outformats |= FORMAT_BIRD;
			break;
		case 'c':
			outformats |= FORMAT_CSV;
			break;
		case 'd':
			cachedir = optarg;
			break;
		case 'e':
			rsync_prog = optarg;
			break;
		case 'j':
			outformats |= FORMAT_JSON;
			break;
		case 'n':
			noop = 1;
			break;
		case 'o':
			outformats |= FORMAT_OPENBGPD;
			break;
		case 'R':
			rrdpon = 0;
			break;
		case 'r':
			rrdpon = 1;
			break;
		case 's':
			timeout = strtonum(optarg, 0, 24*60*60, &errs);
			if (errs)
				errx(1, "-s: %s", errs);
			break;
		case 't':
			if (talsz >= TALSZ_MAX)
				err(1,
				    "too many tal files specified");
			tals[talsz++] = optarg;
			break;
		case 'T':
			bird_tablename = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			fprintf(stderr, "rpki-client %s\n", RPKI_VERSION);
			return 0;
		default:
			goto usage;
		}

	argv += optind;
	argc -= optind;
	if (argc == 1)
		outputdir = argv[0];
	else if (argc > 1)
		goto usage;

	signal(SIGPIPE, SIG_IGN);

	if (cachedir == NULL) {
		warnx("cache directory required");
		goto usage;
	}
	if (outputdir == NULL) {
		warnx("output directory required");
		goto usage;
	}

	if ((cachefd = open(cachedir, O_RDONLY | O_DIRECTORY, 0)) == -1)
		err(1, "cache directory %s", cachedir);
	if ((outdirfd = open(outputdir, O_RDONLY | O_DIRECTORY, 0)) == -1)
		err(1, "output directory %s", outputdir);

	if (outformats == 0)
		outformats = FORMAT_OPENBGPD;

	if (talsz == 0)
		talsz = tal_load_default(tals, TALSZ_MAX);
	if (talsz == 0)
		err(1, "no TAL files found in %s", "/etc/rpki");

	/*
	 * Create the file reader as a jailed child process.
	 * It will be responsible for reading all of the files (ROAs,
	 * manifests, certificates, etc.) and returning contents.
	 */

	if (socketpair(AF_UNIX, fl, 0, fd) == -1)
		err(1, "socketpair");
	if ((procpid = fork()) == -1)
		err(1, "fork");

	if (procpid == 0) {
		close(fd[1]);

		setproctitle("parser");
		/* change working directory to the cache directory */
		if (fchdir(cachefd) == -1)
			err(1, "fchdir");

		if (timeout)
			alarm(timeout);

		/* Only allow access to the cache directory. */
		if (unveil(".", "r") == -1)
			err(1, "%s: unveil", cachedir);
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
		proc_parser(fd[0]);
		errx(1, "parser process returned");
	}

	close(fd[0]);
	proc = fd[1];

	/*
	 * Create a process that will do the rsync'ing.
	 * This process is responsible for making sure that all the
	 * repositories referenced by a certificate manifest (or the
	 * TAL) exists and has been downloaded.
	 */

	if (!noop) {
		if (socketpair(AF_UNIX, fl, 0, fd) == -1)
			err(1, "socketpair");
		if ((rsyncpid = fork()) == -1)
			err(1, "fork");

		if (rsyncpid == 0) {
			close(proc);
			close(fd[1]);

			setproctitle("rsync");
			/* change working directory to the cache directory */
			if (fchdir(cachefd) == -1)
				err(1, "fchdir");

			if (timeout)
				alarm(timeout);

			if (pledge("stdio rpath proc exec unveil", NULL) == -1)
				err(1, "pledge");

			proc_rsync(rsync_prog, bind_addr, fd[0]);
			errx(1, "rsync process returned");
		}

		close(fd[0]);
		rsync = fd[1];
	} else {
		rsync = -1;
		rsyncpid = -1;
	}

	/*
	 * Create a process that will fetch data via https.
	 * With every request the http process receives a file descriptor
	 * where the data should be written to.
	 */

	if (!noop) {
		if (socketpair(AF_UNIX, fl, 0, fd) == -1)
			err(1, "socketpair");
		if ((httppid = fork()) == -1)
			err(1, "fork");

		if (httppid == 0) {
			close(proc);
			close(rsync);
			close(fd[1]);

			setproctitle("http");
			/* change working directory to the cache directory */
			if (fchdir(cachefd) == -1)
				err(1, "fchdir");

			if (timeout)
				alarm(timeout);

			if (pledge("stdio rpath inet dns recvfd", NULL) == -1)
				err(1, "pledge");

			proc_http(bind_addr, fd[0]);
			errx(1, "http process returned");
		}

		close(fd[0]);
		http = fd[1];
	} else {
		http = -1;
		httppid = -1;
	}

	/*
	 * Create a process that will process RRDP.
	 * The rrdp process requires the http process to fetch the various
	 * XML files and does this via the main process.
	 */

	if (!noop && rrdpon) {
		if (socketpair(AF_UNIX, fl, 0, fd) == -1)
			err(1, "socketpair");
		if ((rrdppid = fork()) == -1)
			err(1, "fork");

		if (rrdppid == 0) {
			close(proc);
			close(rsync);
			close(http);
			close(fd[1]);

			setproctitle("rrdp");
			/* change working directory to the cache directory */
			if (fchdir(cachefd) == -1)
				err(1, "fchdir");

			if (timeout)
				alarm(timeout);

			if (pledge("stdio recvfd", NULL) == -1)
				err(1, "pledge");

			proc_rrdp(fd[0]);
			/* NOTREACHED */
		}

		close(fd[0]);
		rrdp = fd[1];
	} else {
		rrdp = -1;
		rrdppid = -1;
	}

	if (timeout) {
		/*
		 * Commit suicide eventually
		 * cron will normally start a new one
		 */
		alarm(timeout);
		signal(SIGALRM, suicide);
	}

	/* TODO unveil cachedir and outputdir, no other access allowed */
	if (pledge("stdio rpath wpath cpath fattr sendfd", NULL) == -1)
		err(1, "pledge");

	msgbuf_init(&procq);
	msgbuf_init(&rsyncq);
	msgbuf_init(&httpq);
	msgbuf_init(&rrdpq);
	procq.fd = proc;
	rsyncq.fd = rsync;
	httpq.fd = http;
	rrdpq.fd = rrdp;

	/*
	 * The main process drives the top-down scan to leaf ROAs using
	 * data downloaded by the rsync process and parsed by the
	 * parsing process.
	 */

	pfd[0].fd = proc;
	queues[0] = &procq;
	pfd[1].fd = rsync;
	queues[1] = &rsyncq;
	pfd[2].fd = http;
	queues[2] = &httpq;
	pfd[3].fd = rrdp;
	queues[3] = &rrdpq;

	/*
	 * Prime the process with our TAL file.
	 * This will contain (hopefully) links to our manifest and we
	 * can get the ball rolling.
	 */

	for (i = 0; i < talsz; i++)
		queue_add_tal(tals[i]);

	/* change working directory to the cache directory */
	if (fchdir(cachefd) == -1)
		err(1, "fchdir");

	while (entity_queue > 0 && !killme) {
		for (i = 0; i < NPFD; i++) {
			pfd[i].events = POLLIN;
			if (queues[i]->queued)
				pfd[i].events |= POLLOUT;
		}

		if ((c = poll(pfd, NPFD, INFTIM)) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}

		for (i = 0; i < NPFD; i++) {
			if (pfd[i].revents & (POLLERR|POLLNVAL))
				errx(1, "poll[%zu]: bad fd", i);
			if (pfd[i].revents & POLLHUP) {
				warnx("poll[%zu]: hangup", i);
				hangup = 1;
			}
			if (pfd[i].revents & POLLOUT) {
				/*
				 * XXX work around deadlocks because of
				 * blocking read vs non-blocking writes.
				 */
				if (i > 1)
					io_socket_nonblocking(pfd[i].fd);
				switch (msgbuf_write(queues[i])) {
				case 0:
					errx(1, "write[%zu]: "
					    "connection closed", i);
				case -1:
					err(1, "write[%zu]", i);
				}
				if (i > 1)
					io_socket_blocking(pfd[i].fd);
			}
		}
		if (hangup)
			break;

		/*
		 * Check the rsync and http process.
		 * This means that one of our modules has completed
		 * downloading and we can flush the module requests into
		 * the parser process.
		 */

		if ((pfd[1].revents & POLLIN)) {
			io_simple_read(rsync, &id, sizeof(id));
			io_simple_read(rsync, &ok, sizeof(ok));
			rsync_finish(id, ok);
		}

		if ((pfd[2].revents & POLLIN)) {
			enum http_result res;
			char *last_mod;

			io_simple_read(http, &id, sizeof(id));
			io_simple_read(http, &res, sizeof(res));
			io_str_read(http, &last_mod);
			http_finish(id, res, last_mod);
			free(last_mod);
		}

		/*
		 * Handle RRDP requests here.
		 */
		if ((pfd[3].revents & POLLIN)) {
			enum rrdp_msg type;
			enum publish_type pt;
			struct rrdp_session s;
			char *uri, *last_mod, *data;
			char hash[SHA256_DIGEST_LENGTH];
			size_t dsz;

			io_simple_read(rrdp, &type, sizeof(type));
			io_simple_read(rrdp, &id, sizeof(id));

			switch (type) {
			case RRDP_END:
				io_simple_read(rrdp, &ok, sizeof(ok));
				rrdp_finish(id, ok);
				break;
			case RRDP_HTTP_REQ:
				io_str_read(rrdp, &uri);
				io_str_read(rrdp, &last_mod);
				rrdp_http_fetch(id, uri, last_mod);
				break;
			case RRDP_SESSION:
				io_str_read(rrdp, &s.session_id);
				io_simple_read(rrdp, &s.serial,
				    sizeof(s.serial));
				io_str_read(rrdp, &s.last_mod);
				rrdp_save_state(id, &s);
				free(s.session_id);
				free(s.last_mod);
				break;
			case RRDP_FILE:
				io_simple_read(rrdp, &pt, sizeof(pt));
				if (pt != PUB_ADD)
					io_simple_read(rrdp, &hash,
					    sizeof(hash));
				io_str_read(rrdp, &uri);
				io_buf_read_alloc(rrdp, (void **)&data, &dsz);

				ok = rrdp_handle_file(id, pt, uri,
				    hash, sizeof(hash), data, dsz);
				rrdp_file_resp(id, ok);

				free(uri);
				free(data);
				break;
			default:
				errx(1, "unexpected rrdp response");
			}
		}

		/*
		 * The parser has finished something for us.
		 * Dequeue these one by one.
		 */

		if ((pfd[0].revents & POLLIN)) {
			entity_process(proc, &stats, &v);
		}
	}

	signal(SIGALRM, SIG_DFL);
	if (killme) {
		syslog(LOG_CRIT|LOG_DAEMON,
		    "excessive runtime (%d seconds), giving up", timeout);
		errx(1, "excessive runtime (%d seconds), giving up", timeout);
	}

	/*
	 * For clean-up, close the input for the parser and rsync
	 * process.
	 * This will cause them to exit, then we reap them.
	 */

	close(proc);
	close(rsync);
	close(http);
	close(rrdp);

	rc = 0;
	for (;;) {
		pid = waitpid(WAIT_ANY, &st, 0);
		if (pid == -1) {
			if (errno == EINTR)
				continue;
			if (errno == ECHILD)
				break;
			err(1, "wait");
		}

		if (pid == procpid)
			name = "parser";
		else if (pid == rsyncpid)
			name = "rsync";
		else if (pid == httppid)
			name = "http";
		else if (pid == rrdppid)
			name = "rrdp";
		else
			name = "unknown";

		if (WIFSIGNALED(st)) {
			warnx("%s terminated signal %d", name, WTERMSIG(st));
			rc = 1;
		} else if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
			warnx("%s process exited abnormally", name);
			rc = 1;
		}
	}

	/* processing did not finish because of error */
	if (entity_queue != 0)
		return 1;

	logx("all files parsed: generating output");

	repo_cleanup(&fpt);

	gettimeofday(&now_time, NULL);
	timersub(&now_time, &start_time, &stats.elapsed_time);
	if (getrusage(RUSAGE_SELF, &ru) == 0) {
		stats.user_time = ru.ru_utime;
		stats.system_time = ru.ru_stime;
	}
	if (getrusage(RUSAGE_CHILDREN, &ru) == 0) {
		timeradd(&stats.user_time, &ru.ru_utime, &stats.user_time);
		timeradd(&stats.system_time, &ru.ru_stime, &stats.system_time);
	}

	/* change working directory to the output directory */
	if (fchdir(outdirfd) == -1)
		err(1, "fchdir output dir");

	if (outputfiles(&v, &stats))
		rc = 1;


	logx("Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)",
	    stats.roas, stats.roas_fail, stats.roas_invalid);
	logx("Certificates: %zu (%zu failed parse, %zu invalid)",
	    stats.certs, stats.certs_fail, stats.certs_invalid);
	logx("Trust Anchor Locators: %zu", stats.tals);
	logx("Manifests: %zu (%zu failed parse, %zu stale)",
	    stats.mfts, stats.mfts_fail, stats.mfts_stale);
	logx("Certificate revocation lists: %zu", stats.crls);
	logx("Ghostbuster records: %zu", stats.gbrs);
	logx("Repositories: %zu", stats.repos);
	logx("Cleanup: removed %zu files, %zu directories",
	    stats.del_files, stats.del_dirs);
	logx("VRP Entries: %zu (%zu unique)", stats.vrps, stats.uniqs);

	/* Memory cleanup. */
	repo_free();

	for (i = 0; i < outsz; i++)
		roa_free(out[i]);
	free(out);

	return rc;

usage:
	fprintf(stderr,
	    "usage: rpki-client [-BcjnoRrVv] [-b sourceaddr] [-d cachedir]"
	    " [-e rsync_prog]\n"
	    "                   [-s timeout] [-T table] [-t tal]"
	    " [outputdir]\n");
	return 1;
}
