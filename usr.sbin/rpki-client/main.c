/*	$OpenBSD: main.c,v 1.186 2022/01/26 14:42:39 claudio Exp $ */
/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/tree.h>
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

const char	*tals[TALSZ_MAX];
const char	*taldescs[TALSZ_MAX];
unsigned int	 talrepocnt[TALSZ_MAX];
size_t		 talsz;

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
int	filemode;
int	rrdpon = 1;
int	repo_timeout;

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

time_t
getmonotime(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		err(1, "clock_gettime");
	return (ts.tv_sec);
}

void
entity_free(struct entity *ent)
{
	if (ent == NULL)
		return;

	free(ent->path);
	free(ent->file);
	free(ent->data);
	free(ent);
}

/*
 * Read a queue entity from the descriptor.
 * Matched by entity_buffer_req().
 * The pointer must be passed entity_free().
 */
void
entity_read_req(struct ibuf *b, struct entity *ent)
{
	io_read_buf(b, &ent->type, sizeof(ent->type));
	io_read_buf(b, &ent->repoid, sizeof(ent->repoid));
	io_read_buf(b, &ent->talid, sizeof(ent->talid));
	io_read_str(b, &ent->path);
	io_read_str(b, &ent->file);
	io_read_buf_alloc(b, (void **)&ent->data, &ent->datasz);
}

/*
 * Write the queue entity.
 * Matched by entity_read_req().
 */
static void
entity_write_req(const struct entity *ent)
{
	struct ibuf *b;

	b = io_new_buffer();
	io_simple_buffer(b, &ent->type, sizeof(ent->type));
	io_simple_buffer(b, &ent->repoid, sizeof(ent->repoid));
	io_simple_buffer(b, &ent->talid, sizeof(ent->talid));
	io_str_buffer(b, ent->path);
	io_str_buffer(b, ent->file);
	io_buf_buffer(b, ent->data, ent->datasz);
	io_close_buffer(&procq, b);
}

static void
entity_write_repo(struct repo *rp)
{
	struct ibuf *b;
	enum rtype type = RTYPE_REPO;
	unsigned int repoid;
	char *path, *altpath;
	int talid = 0;

	repoid = repo_id(rp);
	path = repo_basedir(rp, 0);
	altpath = repo_basedir(rp, 1);
	b = io_new_buffer();
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &repoid, sizeof(repoid));
	io_simple_buffer(b, &talid, sizeof(talid));
	io_str_buffer(b, path);
	io_str_buffer(b, altpath);
	io_buf_buffer(b, NULL, 0);
	io_close_buffer(&procq, b);
	free(path);
	free(altpath);
}

/*
 * Scan through all queued requests and see which ones are in the given
 * repo, then flush those into the parser process.
 */
void
entityq_flush(struct entityq *q, struct repo *rp)
{
	struct entity	*p, *np;

	entity_write_repo(rp);

	TAILQ_FOREACH_SAFE(p, q, entries, np) {
		entity_write_req(p);
		TAILQ_REMOVE(q, p, entries);
		entity_free(p);
	}
}

/*
 * Add the heap-allocated file to the queue for processing.
 */
static void
entityq_add(char *path, char *file, enum rtype type, struct repo *rp,
    unsigned char *data, size_t datasz, int talid)
{
	struct entity	*p;

	if ((p = calloc(1, sizeof(struct entity))) == NULL)
		err(1, NULL);

	p->type = type;
	p->talid = talid;
	p->path = path;
	if (rp != NULL)
		p->repoid = repo_id(rp);
	p->file = file;
	p->data = data;
	p->datasz = (data != NULL) ? datasz : 0;

	entity_queue++;

	/*
	 * Write to the queue if there's no repo or the repo has already
	 * been loaded else enqueue it for later.
	 */

	if (rp == NULL || !repo_queued(rp, p)) {
		entity_write_req(p);
		entity_free(p);
	}
}

static void
rrdp_file_resp(unsigned int id, int ok)
{
	enum rrdp_msg type = RRDP_FILE;
	struct ibuf *b;

	b = io_new_buffer();
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &ok, sizeof(ok));
	io_close_buffer(&rrdpq, b);
}

void
rrdp_fetch(unsigned int id, const char *uri, const char *local,
    struct rrdp_session *s)
{
	enum rrdp_msg type = RRDP_START;
	struct ibuf *b;

	b = io_new_buffer();
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, local);
	io_str_buffer(b, uri);
	io_str_buffer(b, s->session_id);
	io_simple_buffer(b, &s->serial, sizeof(s->serial));
	io_str_buffer(b, s->last_mod);
	io_close_buffer(&rrdpq, b);
}

/*
 * Request a repository sync via rsync URI to directory local.
 */
void
rsync_fetch(unsigned int id, const char *uri, const char *local,
    const char *base)
{
	struct ibuf	*b;

	b = io_new_buffer();
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, local);
	io_str_buffer(b, base);
	io_str_buffer(b, uri);
	io_close_buffer(&rsyncq, b);
}

/*
 * Request a file from a https uri, data is written to the file descriptor fd.
 */
void
http_fetch(unsigned int id, const char *uri, const char *last_mod, int fd)
{
	struct ibuf	*b;

	b = io_new_buffer();
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, uri);
	io_str_buffer(b, last_mod);
	/* pass file as fd */
	b->fd = fd;
	io_close_buffer(&httpq, b);
}

/*
 * Request some XML file on behalf of the rrdp parser.
 * Create a pipe and pass the pipe endpoints to the http and rrdp process.
 */
static void
rrdp_http_fetch(unsigned int id, const char *uri, const char *last_mod)
{
	enum rrdp_msg type = RRDP_HTTP_INI;
	struct ibuf *b;
	int pi[2];

	if (pipe2(pi, O_CLOEXEC | O_NONBLOCK) == -1)
		err(1, "pipe");

	b = io_new_buffer();
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	b->fd = pi[0];
	io_close_buffer(&rrdpq, b);

	http_fetch(id, uri, last_mod, pi[1]);
}

void
rrdp_http_done(unsigned int id, enum http_result res, const char *last_mod)
{
	enum rrdp_msg type = RRDP_HTTP_FIN;
	struct ibuf *b;

	/* RRDP request, relay response over to the rrdp process */
	b = io_new_buffer();
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &res, sizeof(res));
	io_str_buffer(b, last_mod);
	io_close_buffer(&rrdpq, b);
}

/*
 * Add a file (CER, ROA, CRL) from an MFT file, RFC 6486.
 * These are always relative to the directory in which "mft" sits.
 */
static void
queue_add_from_mft(const char *path, const struct mftfile *file,
    struct repo *rp)
{
	char		*nfile, *npath = NULL;

	if (path != NULL)
		if ((npath = strdup(path)) == NULL)
			err(1, NULL);
	if ((nfile = strdup(file->file)) == NULL)
		err(1, NULL);

	entityq_add(npath, nfile, file->type, rp, NULL, 0, -1);
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
queue_add_from_mft_set(const struct mft *mft, const char *name, struct repo *rp)
{
	size_t			 i;
	const struct mftfile	*f;

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		if (f->type != RTYPE_CRL)
			continue;
		queue_add_from_mft(mft->path, f, rp);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		switch (f->type) {
		case RTYPE_CER:
		case RTYPE_ROA:
		case RTYPE_GBR:
			queue_add_from_mft(mft->path, f, rp);
			break;
		case RTYPE_CRL:
			continue;
		default:
			warnx("%s: unsupported file: %s", name, f->file);
		}
	}
}

/*
 * Add a local file to the queue of files to fetch.
 */
static void
queue_add_file(const char *file, enum rtype type, int talid)
{
	unsigned char	*buf = NULL;
	char		*nfile;
	size_t		 len = 0;

	if (!filemode || strncmp(file, "rsync://", strlen("rsync://")) != 0) {
		buf = load_file(file, &len);
		if (buf == NULL)
			err(1, "%s", file);
	}

	if ((nfile = strdup(file)) == NULL)
		err(1, NULL);
	/* Not in a repository, so directly add to queue. */
	entityq_add(NULL, nfile, type, NULL, buf, len, talid);
}

/*
 * Add URIs (CER) from a TAL file, RFC 8630.
 */
static void
queue_add_from_tal(struct tal *tal)
{
	struct repo	*repo;
	unsigned char	*data;
	char		*nfile;

	assert(tal->urisz);

	if ((taldescs[tal->id] = strdup(tal->descr)) == NULL)
		err(1, NULL);

	/* figure out the TA filename, must be done before repo lookup */
	nfile = strrchr(tal->uri[0], '/');
	assert(nfile != NULL);
	if ((nfile = strdup(nfile + 1)) == NULL)
		err(1, NULL);

	/* Look up the repository. */
	repo = ta_lookup(tal->id, tal);
	if (repo == NULL) {
		free(nfile);
		return;
	}

	/* steal the pkey from the tal structure */
	data = tal->pkey;
	tal->pkey = NULL;
	entityq_add(NULL, nfile, RTYPE_CER, repo, data, tal->pkeysz, tal->id);
}

/*
 * Add a manifest (MFT) found in an X509 certificate, RFC 6487.
 */
static void
queue_add_from_cert(const struct cert *cert)
{
	struct repo	*repo;
	char		*nfile, *npath;
	const char	*uri, *repouri, *file;
	size_t		 repourisz;

	repo = repo_lookup(cert->talid, cert->repo,
	    rrdpon ? cert->notify : NULL);
	if (repo == NULL)
		return;

	/*
	 * Figure out the cert filename and path by chopping up the
	 * MFT URI in the cert based on the repo base URI.
	 */
	uri = cert->mft;
	repouri = repo_uri(repo);
	repourisz = strlen(repouri);
	if (strncmp(repouri, cert->mft, repourisz) != 0) {
		warnx("%s: URI %s outside of repository", repouri, uri);
		return;
	}
	uri += repourisz + 1;	/* skip base and '/' */
	file = strrchr(uri, '/');
	if (file == NULL) {
		npath = NULL;
		if ((nfile = strdup(uri)) == NULL)
			err(1, NULL);
	} else {
		if ((npath = strndup(uri, file - uri)) == NULL)
			err(1, NULL);
		if ((nfile = strdup(file + 1)) == NULL)
			err(1, NULL);
	}

	entityq_add(npath, nfile, RTYPE_MFT, repo, NULL, 0, -1);
}

/*
 * Process parsed content.
 * For non-ROAs, we grok for more data.
 * For ROAs, we want to extract the valid info.
 * In all cases, we gather statistics.
 */
static void
entity_process(struct ibuf *b, struct stats *st, struct vrp_tree *tree,
    struct brk_tree *brktree)
{
	enum rtype	 type;
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	char		*file;
	int		 c;

	/*
	 * For most of these, we first read whether there's any content
	 * at all---this means that the syntactic parse failed (X509
	 * certificate, for example).
	 * We follow that up with whether the resources didn't parse.
	 */
	io_read_buf(b, &type, sizeof(type));
	io_read_str(b, &file);

	/* in filemode messages can be ignored, only the accounting matters */
	if (filemode)
		goto done;

	if (filepath_add(&fpt, file) == 0) {
		warnx("%s: File already visited", file);
		goto done;
	}

	switch (type) {
	case RTYPE_TAL:
		st->tals++;
		tal = tal_read(b);
		queue_add_from_tal(tal);
		tal_free(tal);
		break;
	case RTYPE_CER:
		st->certs++;
		io_read_buf(b, &c, sizeof(c));
		if (c == 0) {
			st->certs_fail++;
			break;
		}
		cert = cert_read(b);
		if (cert->purpose == CERT_PURPOSE_CA) {
			/*
			 * Process the revocation list from the
			 * certificate *first*, since it might mark that
			 * we're revoked and then we don't want to
			 * process the MFT.
			 */
			queue_add_from_cert(cert);
		} else if (cert->purpose == CERT_PURPOSE_BGPSEC_ROUTER) {
			cert_insert_brks(brktree, cert);
			st->brks++;
		} else
			st->certs_fail++;
		cert_free(cert);
		break;
	case RTYPE_MFT:
		st->mfts++;
		io_read_buf(b, &c, sizeof(c));
		if (c == 0) {
			st->mfts_fail++;
			break;
		}
		mft = mft_read(b);
		if (!mft->stale)
			queue_add_from_mft_set(mft, file,
			    repo_byid(mft->repoid));
		else
			st->mfts_stale++;
		mft_free(mft);
		break;
	case RTYPE_CRL:
		st->crls++;
		break;
	case RTYPE_ROA:
		st->roas++;
		io_read_buf(b, &c, sizeof(c));
		if (c == 0) {
			st->roas_fail++;
			break;
		}
		roa = roa_read(b);
		if (roa->valid)
			roa_insert_vrps(tree, roa, &st->vrps, &st->uniqs);
		else
			st->roas_invalid++;
		roa_free(roa);
		break;
	case RTYPE_GBR:
		st->gbrs++;
		break;
	case RTYPE_FILE:
		break;
	default:
		errx(1, "unknown entity type %d", type);
	}

done:
	free(file);
	entity_queue--;
}

static void
rrdp_process(struct ibuf *b)
{
	enum rrdp_msg type;
	enum publish_type pt;
	struct rrdp_session s;
	char *uri, *last_mod, *data;
	char hash[SHA256_DIGEST_LENGTH];
	size_t dsz;
	unsigned int id;
	int ok;

	io_read_buf(b, &type, sizeof(type));
	io_read_buf(b, &id, sizeof(id));

	switch (type) {
	case RRDP_END:
		io_read_buf(b, &ok, sizeof(ok));
		rrdp_finish(id, ok);
		break;
	case RRDP_HTTP_REQ:
		io_read_str(b, &uri);
		io_read_str(b, &last_mod);
		rrdp_http_fetch(id, uri, last_mod);
		break;
	case RRDP_SESSION:
		io_read_str(b, &s.session_id);
		io_read_buf(b, &s.serial, sizeof(s.serial));
		io_read_str(b, &s.last_mod);
		rrdp_save_state(id, &s);
		free(s.session_id);
		free(s.last_mod);
		break;
	case RRDP_FILE:
		io_read_buf(b, &pt, sizeof(pt));
		if (pt != PUB_ADD)
			io_read_buf(b, &hash, sizeof(hash));
		io_read_str(b, &uri);
		io_read_buf_alloc(b, (void **)&data, &dsz);

		ok = rrdp_handle_file(id, pt, uri, hash, sizeof(hash),
		    data, dsz);
		rrdp_file_resp(id, ok);

		free(uri);
		free(data);
		break;
	case RRDP_CLEAR:
		rrdp_clear(id);
		break;
	default:
		errx(1, "unexpected rrdp response");
	}
}

/*
 * Assign filenames ending in ".tal" in "/etc/rpki" into "tals",
 * returning the number of files found and filled-in.
 * This may be zero.
 * Don't exceded "max" filenames.
 */
static size_t
tal_load_default(void)
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
		if (s >= TALSZ_MAX)
			err(1, "too many tal files found in %s",
			    confdir);
		if (asprintf(&path, "%s/%s", confdir, dp->d_name) == -1)
			err(1, NULL);
		tals[s++] = path;
	}
	closedir(dirp);
	return s;
}

static void
check_fs_size(int fd, const char *cachedir)
{
	struct statvfs	fs;
	const long long minsize = 500 * 1024 * 1024;
	const long long minnode = 300 * 1000;

	if (fstatvfs(fd, &fs) == -1)
		err(1, "statfs %s", cachedir);

	if (fs.f_bavail < minsize / fs.f_frsize || fs.f_favail < minnode) {
		fprintf(stderr, "WARNING: rpki-client may need more than "
		    "the available disk space\n"
		    "on the file-system holding %s.\n", cachedir);
		fprintf(stderr, "available space: %lldkB, "
		    "suggested minimum %lldkB\n",
		    (long long)fs.f_bavail * fs.f_frsize / 1024,
		    minsize / 1024);
		fprintf(stderr, "available inodes %lld, "
		    "suggested minimum %lld\n\n",
		    (long long)fs.f_favail, minnode);
		fflush(stderr);
	}
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
	int		 rc, c, st, proc, rsync, http, rrdp, hangup = 0;
	int		 fl = SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK;
	size_t		 i;
	pid_t		 pid, procpid, rsyncpid, httppid, rrdppid;
	int		 fd[2];
	struct pollfd	 pfd[NPFD];
	struct msgbuf	*queues[NPFD];
	struct ibuf	*b, *httpbuf = NULL, *procbuf = NULL;
	struct ibuf	*rrdpbuf = NULL, *rsyncbuf = NULL;
	char		*rsync_prog = "openrsync";
	char		*bind_addr = NULL;
	const char	*cachedir = NULL, *outputdir = NULL;
	const char	*errs, *name;
	struct vrp_tree	 vrps = RB_INITIALIZER(&vrps);
	struct brk_tree  brks = RB_INITIALIZER(&brks);
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
	repo_timeout = timeout / 4;

	if (pledge("stdio rpath wpath cpath inet fattr dns sendfd recvfd "
	    "proc exec unveil", NULL) == -1)
		err(1, "pledge");

	while ((c = getopt(argc, argv, "b:Bcd:e:fjnorRs:t:T:vV")) != -1)
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
		case 'f':
			filemode = 1;
			noop = 1;
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
			if (timeout == 0)
				repo_timeout = 24*60*60;
			else if (timeout < 1)
				errx(1, "-s: %i too small", timeout);
			else
				repo_timeout = timeout / 4;
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

	if (!filemode) {
		if (argc == 1)
			outputdir = argv[0];
		else if (argc > 1)
			goto usage;

		if (outputdir == NULL) {
			warnx("output directory required");
			goto usage;
		}
	} else {
		if (argc == 0)
			goto usage;
		outputdir = NULL;
	}

	if (cachedir == NULL) {
		warnx("cache directory required");
		goto usage;
	}

	signal(SIGPIPE, SIG_IGN);

	if ((cachefd = open(cachedir, O_RDONLY | O_DIRECTORY)) == -1)
		err(1, "cache directory %s", cachedir);
	if (outputdir != NULL) {
		if ((outdirfd = open(outputdir, O_RDONLY | O_DIRECTORY)) == -1)
			err(1, "output directory %s", outputdir);
		if (outformats == 0)
			outformats = FORMAT_OPENBGPD;
	}

	check_fs_size(cachefd, cachedir);


	if (talsz == 0)
		talsz = tal_load_default();
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
		queue_add_file(tals[i], RTYPE_TAL, i);

	if (filemode) {
		while (*argv != NULL)
			queue_add_file(*argv++, RTYPE_FILE, 0);
	}

	/* change working directory to the cache directory */
	if (fchdir(cachefd) == -1)
		err(1, "fchdir");

	while (entity_queue > 0 && !killme) {
		int polltim;

		for (i = 0; i < NPFD; i++) {
			pfd[i].events = POLLIN;
			if (queues[i]->queued)
				pfd[i].events |= POLLOUT;
		}

		polltim = repo_check_timeout(INFTIM);

		if ((c = poll(pfd, NPFD, polltim)) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}

		for (i = 0; i < NPFD; i++) {
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				warnx("poll[%zu]: bad fd", i);
				hangup = 1;
			}
			if (pfd[i].revents & POLLHUP)
				hangup = 1;
			if (pfd[i].revents & POLLOUT) {
				switch (msgbuf_write(queues[i])) {
				case 0:
					warnx("write[%zu]: "
					    "connection closed", i);
					hangup = 1;
					break;
				case -1:
					warn("write[%zu]", i);
					hangup = 1;
					break;
				}
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
			b = io_buf_read(rsync, &rsyncbuf);
			if (b != NULL) {
				unsigned int id;
				int ok;

				io_read_buf(b, &id, sizeof(id));
				io_read_buf(b, &ok, sizeof(ok));
				rsync_finish(id, ok);
				ibuf_free(b);
			}
		}

		if ((pfd[2].revents & POLLIN)) {
			b = io_buf_read(http, &httpbuf);
			if (b != NULL) {
				unsigned int id;
				enum http_result res;
				char *last_mod;

				io_read_buf(b, &id, sizeof(id));
				io_read_buf(b, &res, sizeof(res));
				io_read_str(b, &last_mod);
				http_finish(id, res, last_mod);
				free(last_mod);
				ibuf_free(b);
			}
		}

		/*
		 * Handle RRDP requests here.
		 */
		if ((pfd[3].revents & POLLIN)) {
			b = io_buf_read(rrdp, &rrdpbuf);
			if (b != NULL) {
				rrdp_process(b);
				ibuf_free(b);
			}
		}

		/*
		 * The parser has finished something for us.
		 * Dequeue these one by one.
		 */

		if ((pfd[0].revents & POLLIN)) {
			b = io_buf_read(proc, &procbuf);
			if (b != NULL) {
				entity_process(b, &stats, &vrps, &brks);
				ibuf_free(b);
			}
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
		errx(1, "not all files processed, giving up");

	/* if processing in filemode the process is done, no cleanup */
	if (filemode)
		return rc;

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

	if (outputfiles(&vrps, &brks, &stats))
		rc = 1;

	logx("Processing time %lld seconds "
	    "(%lld seconds user, %lld seconds system)",
	    (long long)stats.elapsed_time.tv_sec,
	    (long long)stats.user_time.tv_sec,
	    (long long)stats.system_time.tv_sec);
	logx("Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)",
	    stats.roas, stats.roas_fail, stats.roas_invalid);
	logx("BGPsec Router Certificates: %zu", stats.brks);
	logx("Certificates: %zu (%zu invalid)",
	    stats.certs, stats.certs_fail);
	logx("Trust Anchor Locators: %zu (%zu invalid)",
	    stats.tals, talsz - stats.tals);
	logx("Manifests: %zu (%zu failed parse, %zu stale)",
	    stats.mfts, stats.mfts_fail, stats.mfts_stale);
	logx("Certificate revocation lists: %zu", stats.crls);
	logx("Ghostbuster records: %zu", stats.gbrs);
	logx("Repositories: %zu", stats.repos);
	logx("Cleanup: removed %zu files, %zu directories, %zu superfluous",
	    stats.del_files, stats.del_dirs, stats.extra_files);
	logx("VRP Entries: %zu (%zu unique)", stats.vrps, stats.uniqs);

	/* Memory cleanup. */
	repo_free();

	return rc;

usage:
	fprintf(stderr,
	    "usage: rpki-client [-BcjnoRrVv] [-b sourceaddr] [-d cachedir]"
	    " [-e rsync_prog]\n"
	    "                   [-s timeout] [-T table] [-t tal]"
	    " [outputdir]\n"
	    "       rpki-client [-Vv] [-d cachedir] [-t tal] -f file ...\n");
	return 1;
}
