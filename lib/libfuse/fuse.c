/* $OpenBSD: fuse.c,v 1.41 2017/12/15 16:40:33 jca Exp $ */
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <miscfs/fuse/fusefs.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fuse_opt.h"
#include "fuse_private.h"
#include "debug.h"

static volatile sig_atomic_t sigraised = 0;
static volatile sig_atomic_t signum = 0;
static struct fuse_context *ictx = NULL;
static int max_read = FUSEBUFMAXSIZE;

enum {
	KEY_DEBUG,
	KEY_FOREGROUND,
	KEY_HELP,
	KEY_HELP_WITHOUT_HEADER,
	KEY_VERSION,
	KEY_MAXREAD,
	KEY_STUB
};

static struct fuse_opt fuse_core_opts[] = {
	FUSE_OPT_KEY("-h",			KEY_HELP),
	FUSE_OPT_KEY("--help",			KEY_HELP),
	FUSE_OPT_KEY("-ho",			KEY_HELP_WITHOUT_HEADER),
	FUSE_OPT_KEY("-V",			KEY_VERSION),
	FUSE_OPT_KEY("--version",		KEY_VERSION),
	FUSE_OPT_KEY("max_read=",		KEY_MAXREAD),
	FUSE_OPT_KEY("debug",			KEY_DEBUG),
	FUSE_OPT_KEY("-d",			KEY_DEBUG),
	FUSE_OPT_KEY("-f",			KEY_FOREGROUND),
	FUSE_OPT_KEY("-s",			KEY_STUB),
	FUSE_OPT_KEY("use_ino",			KEY_STUB),
	FUSE_OPT_KEY("big_writes",		KEY_STUB),
	FUSE_OPT_KEY("default_permissions",	KEY_STUB),
	FUSE_OPT_KEY("fsname=",			KEY_STUB),
	FUSE_OPT_END
};

static void
ifuse_sighdlr(int num)
{
	if (!sigraised || (num == SIGCHLD)) {
		sigraised = 1;
		signum = num;
	}
}

static void
ifuse_try_unmount(struct fuse *f)
{
	pid_t child;

	signal(SIGCHLD, ifuse_sighdlr);

	/* unmount in another thread so fuse_loop() doesn't deadlock */
	child = fork();

	if (child < 0) {
		DPERROR(__func__);
		return;
	}

	if (child == 0) {
		fuse_remove_signal_handlers(fuse_get_session(f));
		errno = 0;
		fuse_unmount(f->fc->dir, f->fc);
		_exit(errno);
	}
}

static void
ifuse_child_exit(const struct fuse *f)
{
	int status;

	signal(SIGCHLD, SIG_DFL);
	if (waitpid(WAIT_ANY, &status, WNOHANG) == -1)
		fprintf(stderr, "fuse: %s\n", strerror(errno));

	if (WIFEXITED(status) && (WEXITSTATUS(status) != 0))
		fprintf(stderr, "fuse: %s: %s\n",
			f->fc->dir, strerror(WEXITSTATUS(status)));

	sigraised = 0;
	return;
}

int
fuse_loop(struct fuse *fuse)
{
	struct fusebuf fbuf;
	struct fuse_context ctx;
	struct fb_ioctl_xch ioexch;
	struct kevent ev;
	ssize_t n;
	int ret;

	if (fuse == NULL)
		return (-1);

	fuse->fc->kq = kqueue();
	if (fuse->fc->kq == -1)
		return (-1);

	EV_SET(&fuse->fc->event, fuse->fc->fd, EVFILT_READ, EV_ADD |
	    EV_ENABLE, 0, 0, 0);

	while (!fuse->fc->dead) {
		ret = kevent(fuse->fc->kq, &fuse->fc->event, 1, &ev, 1, NULL);
		if (ret == -1) {
			if (errno == EINTR) {
				switch (signum) {
				case SIGCHLD:
					ifuse_child_exit(fuse);
					break;
				case SIGHUP:
				case SIGINT:
				case SIGTERM:
					ifuse_try_unmount(fuse);
					break;
				default:
					fprintf(stderr, "%s: %s\n", __func__,
					    strsignal(signum));
				}
			} else
				DPERROR(__func__);
		} else if (ret > 0) {
			n = read(fuse->fc->fd, &fbuf, sizeof(fbuf));
			if (n != sizeof(fbuf)) {
				fprintf(stderr, "%s: bad fusebuf read\n",
				    __func__);
				return (-1);
			}

			/* check if there is data something present */
			if (fbuf.fb_len) {
				fbuf.fb_dat = malloc(fbuf.fb_len);
				if (fbuf.fb_dat == NULL)
					return (-1);
				ioexch.fbxch_uuid = fbuf.fb_uuid;
				ioexch.fbxch_len = fbuf.fb_len;
				ioexch.fbxch_data = fbuf.fb_dat;

				if (ioctl(fuse->fc->fd, FIOCGETFBDAT,
				    &ioexch)) {
					free(fbuf.fb_dat);
					return (-1);
				}
			}

			ctx.fuse = fuse;
			ctx.uid = fuse->conf.uid;
			ctx.gid = fuse->conf.gid;
			ctx.pid = fuse->conf.pid;
			ctx.umask = fuse->conf.umask;
			ctx.private_data = fuse->private_data;
			ictx = &ctx;

			ret = ifuse_exec_opcode(fuse, &fbuf);
			if (ret) {
				ictx = NULL;
				return (-1);
			}

			n = write(fuse->fc->fd, &fbuf, sizeof(fbuf));
			if (fbuf.fb_len) {
				if (fbuf.fb_dat == NULL) {
					fprintf(stderr, "%s: fb_dat is Null\n",
					    __func__);
					return (-1);
				}
				ioexch.fbxch_uuid = fbuf.fb_uuid;
				ioexch.fbxch_len = fbuf.fb_len;
				ioexch.fbxch_data = fbuf.fb_dat;

				if (ioctl(fuse->fc->fd, FIOCSETFBDAT, &ioexch)) {
					free(fbuf.fb_dat);
					return (-1);
				}
				free(fbuf.fb_dat);
			}
			ictx = NULL;

			if (n != FUSEBUFSIZE) {
				errno = EINVAL;
				return (-1);
			}
		}
	}

	return (0);
}
DEF(fuse_loop);

struct fuse_chan *
fuse_mount(const char *dir, unused struct fuse_args *args)
{
	struct fusefs_args fargs;
	struct fuse_chan *fc;
	const char *errcause;

	if (dir == NULL)
		return (NULL);

	fc = calloc(1, sizeof(*fc));
	if (fc == NULL)
		return (NULL);

	fc->dir = realpath(dir, NULL);
	if (fc->dir == NULL)
		goto bad;

	if ((fc->fd = open("/dev/fuse0", O_RDWR)) == -1) {
		perror(__func__);
		goto bad;
	}

	fargs.fd = fc->fd;
	fargs.max_read = max_read;
	if (mount(MOUNT_FUSEFS, fc->dir, 0, &fargs)) {
		switch (errno) {
		case EMFILE:
			errcause = "mount table full";
			break;
		case EOPNOTSUPP:
			errcause = "filesystem not supported by kernel";
			break;
		default:
			errcause = strerror(errno);
			break;
		}
		fprintf(stderr, "%s on %s: %s\n", __func__, dir, errcause);
		goto bad;
	}

	return (fc);
bad:
	if (fc->fd != -1)
		close(fc->fd);
	free(fc->dir);
	free(fc);
	return (NULL);
}
DEF(fuse_mount);

void
fuse_unmount(const char *dir, struct fuse_chan *ch)
{
	if (ch == NULL || ch->dead)
		return;

	if (unmount(dir, MNT_UPDATE) == -1)
		DPERROR(__func__);
}
DEF(fuse_unmount);

int
fuse_is_lib_option(const char *opt)
{
	return (fuse_opt_match(fuse_core_opts, opt));
}

int
fuse_chan_fd(struct fuse_chan *ch)
{
	if (ch == NULL)
		return (-1);

	return (ch->fd);
}

struct fuse_session *
fuse_get_session(struct fuse *f)
{
	return (&f->se);
}
DEF(fuse_get_session);

int
fuse_loop_mt(unused struct fuse *fuse)
{
	return (-1);
}

struct fuse *
fuse_new(struct fuse_chan *fc, unused struct fuse_args *args,
    const struct fuse_operations *ops, unused size_t size,
    void *userdata)
{
	struct fuse *fuse;
	struct fuse_vnode *root;

	if (fc == NULL || ops == NULL)
		return (NULL);

	if ((fuse = calloc(1, sizeof(*fuse))) == NULL)
		return (NULL);

	/* copy fuse ops to their own structure */
	memcpy(&fuse->op, ops, sizeof(fuse->op));

	fuse->fc = fc;
	fuse->max_ino = FUSE_ROOT_INO;
	fuse->se.args = fuse;
	fuse->private_data = userdata;

	if ((root = alloc_vn(fuse, "/", FUSE_ROOT_INO, 0)) == NULL) {
		free(fuse);
		return (NULL);
	}

	tree_init(&fuse->vnode_tree);
	tree_init(&fuse->name_tree);
	if (!set_vn(fuse, root)) {
		free(fuse);
		return (NULL);
	}

	return (fuse);
}
DEF(fuse_new);

int
fuse_daemonize(int foreground)
{
	if (foreground)
		return (0);

	return (daemon(0,0));
}
DEF(fuse_daemonize);

void
fuse_destroy(struct fuse *f)
{
	if (f == NULL)
		return;

	/*
  	 * Even though these were allocated in fuse_mount(), we can't free them
 	 * in fuse_unmount() since fuse_loop() will not have terminated yet so
 	 * we free them here.
 	 */
	close(f->fc->fd);
	free(f->fc->dir);
	free(f->fc);
	free(f);
}
DEF(fuse_destroy);

void
fuse_remove_signal_handlers(unused struct fuse_session *se)
{
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
}
DEF(fuse_remove_signal_handlers);

int
fuse_set_signal_handlers(unused struct fuse_session *se)
{
	signal(SIGHUP, ifuse_sighdlr);
	signal(SIGINT, ifuse_sighdlr);
	signal(SIGTERM, ifuse_sighdlr);
	signal(SIGPIPE, SIG_IGN);
	return (0);
}

static void
dump_help(void)
{
	fprintf(stderr, "FUSE options:\n"
	    "    -d   -o debug          enable debug output (implies -f)\n"
	    "    -f                     run in foreground\n"
	    "    -V                     print fuse version\n"
	    "\n");
}

static void
dump_version(void)
{
	fprintf(stderr, "FUSE library version: %d.%d\n", FUSE_MAJOR_VERSION,
	    FUSE_MINOR_VERSION);
}

static int
ifuse_process_opt(void *data, const char *arg, int key,
    unused struct fuse_args *args)
{
	struct fuse_core_opt *opt = data;
	struct stat st;
	const char *err;
	int res;

	switch (key) {
		case KEY_STUB:
			return (0);
		case KEY_DEBUG:
			ifuse_debug_init();
			/* falls through */
		case KEY_FOREGROUND:
			opt->foreground = 1;
			return (0);
		case KEY_HELP:
		case KEY_HELP_WITHOUT_HEADER:
			dump_help();
			return (-1);
		case KEY_VERSION:
			dump_version();
			return (-1);
		case KEY_MAXREAD:
			res = strtonum(arg, 0, FUSEBUFMAXSIZE, &err);
			if (err) {
				fprintf(stderr, "fuse: max_read %s\n", err);
				return (-1);
			}
			max_read = res;
			break;
		case FUSE_OPT_KEY_NONOPT:
			if (opt->mp == NULL) {
				opt->mp = realpath(arg, opt->mp);
				if (opt->mp == NULL) {
					fprintf(stderr, "fuse: realpath: "
					    "%s : %s\n", arg, strerror(errno));
					return (-1);
				}

				res = stat(opt->mp, &st);
				if (res == -1) {
					fprintf(stderr, "fuse: bad mount point "
					    "%s : %s\n", arg, strerror(errno));
					return (-1);
				}

				if (!S_ISDIR(st.st_mode)) {
					fprintf(stderr, "fuse: bad mount point "
					    "%s : %s\n", arg,
					    strerror(ENOTDIR));
					return (-1);
				}
			} else {
				fprintf(stderr, "fuse: invalid argument %s\n",
				    arg);
				return (-1);
			}
			break;
		default:
			fprintf(stderr, "fuse: unknown option %s\n", arg);
			return (-1);
	}
	return (0);
}

int
fuse_parse_cmdline(struct fuse_args *args, char **mp, int *mt, int *fg)
{
	struct fuse_core_opt opt;

	bzero(&opt, sizeof(opt));
	if (fuse_opt_parse(args, &opt, fuse_core_opts, ifuse_process_opt) == -1)
		return (-1);

	if (opt.mp == NULL) {
		fprintf(stderr, "fuse: missing mountpoint parameter\n");
		return (-1);
	}

	if (mp != NULL) {
		*mp = strdup(opt.mp);
		if (*mp == NULL)
			return (-1);
	}

	if (mt != NULL)
		*mt = 0;

	if (fg != NULL)
		*fg = opt.foreground;

	return (0);
}
DEF(fuse_parse_cmdline);

struct fuse_context *
fuse_get_context(void)
{
	return (ictx);
}
DEF(fuse_get_context);

int
fuse_version(void)
{
	return (FUSE_VERSION);
}

void
fuse_teardown(struct fuse *fuse, char *mp)
{
	if (fuse == NULL || mp == NULL)
		return;

	fuse_unmount(mp, fuse->fc);
	fuse_destroy(fuse);
}

int
fuse_invalidate(unused struct fuse *f, unused const char *path)
{
	return (EINVAL);
}

struct fuse *
fuse_setup(int argc, char **argv, const struct fuse_operations *ops,
    size_t size, char **mp, int *mt, void *data)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *fc;
	struct fuse *fuse;
	char *dir;
	int fg;

	dir = NULL;
	if (fuse_parse_cmdline(&args, &dir, mt, &fg))
		goto err;

	fuse_daemonize(fg);

	if ((fc = fuse_mount(dir, NULL)) == NULL)
		goto err;

	if ((fuse = fuse_new(fc, NULL, ops, size, data)) == NULL) {
		fuse_unmount(dir, fc);
		close(fc->fd);
		free(fc->dir);
		free(fc);
		goto err;
	}

	if (fuse_set_signal_handlers(fuse_get_session(fuse)) == -1) {
		fuse_unmount(dir, fc);
		fuse_destroy(fuse);
		goto err;
	}

	if (mp != NULL)
		*mp = dir;

	return (fuse);
err:
	free(dir);
	return (NULL);
}
DEF(fuse_setup);

int
fuse_main(int argc, char **argv, const struct fuse_operations *ops, void *data)
{
	struct fuse *fuse;

	fuse = fuse_setup(argc, argv, ops, sizeof(*ops), NULL, NULL, data);
	if (!fuse)
		return (-1);

	return (fuse_loop(fuse));
}
