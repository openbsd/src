/* $OpenBSD: file.c,v 1.47 2015/07/12 09:51:25 tobias Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <errno.h>
#include <imsg.h>
#include <libgen.h>
#include <getopt.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "magic.h"
#include "xmalloc.h"

struct input_msg
{
	int		idx;

	struct stat	sb;
	int		error;

	char		link_path[PATH_MAX];
	int		link_error;
	int		link_target;
};

struct input_ack
{
	int		idx;
};

struct input_file
{
	struct magic		*m;
	struct input_msg	*msg;

	const char		*path;
	int			 fd;

	void			*base;
	size_t			 size;
	int			 mapped;
	char			*result;
};

extern char	*__progname;

__dead void	 usage(void);

static void	 send_message(struct imsgbuf *, void *, size_t, int);
static int	 read_message(struct imsgbuf *, struct imsg *, pid_t);

static void	 read_link(struct input_msg *, const char *);

static __dead void child(int, pid_t, int, char **);

static void	 test_file(struct input_file *, size_t);

static int	 try_stat(struct input_file *);
static int	 try_empty(struct input_file *);
static int	 try_access(struct input_file *);
static int	 try_text(struct input_file *);
static int	 try_magic(struct input_file *);
static int	 try_unknown(struct input_file *);

static int	 bflag;
static int	 cflag;
static int	 iflag;
static int	 Lflag;
static int	 sflag;
static int	 Wflag;

static char	*magicpath;
static FILE	*magicfp;

static struct option longopts[] = {
	{ "mime",      no_argument, NULL, 'i' },
	{ "mime-type", no_argument, NULL, 'i' },
	{ NULL,        0,           NULL, 0   }
};

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-bchiLsW] file ...\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int			 opt, pair[2], fd, idx;
	char			*home;
	struct passwd		*pw;
	struct imsgbuf		 ibuf;
	struct imsg		 imsg;
	struct input_msg	 msg;
	struct input_ack	*ack;
	pid_t			 pid, parent;

	tzset();

	for (;;) {
		opt = getopt_long(argc, argv, "bchiLsW", longopts, NULL);
		if (opt == -1)
			break;
		switch (opt) {
		case 'b':
			bflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'h':
			Lflag = 0;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'L':
			Lflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'W':
			Wflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (cflag) {
		if (argc != 0)
			usage();
	} else if (argc == 0)
		usage();

	magicfp = NULL;
	if (geteuid() != 0 && !issetugid()) {
		home = getenv("HOME");
		if (home == NULL || *home == '\0') {
			pw = getpwuid(getuid());
			if (pw != NULL)
				home = pw->pw_dir;
			else
				home = NULL;
		}
		if (home != NULL) {
			xasprintf(&magicpath, "%s/.magic", home);
			magicfp = fopen(magicpath, "r");
			if (magicfp == NULL)
				free(magicpath);
		}
	}
	if (magicfp == NULL) {
		magicpath = xstrdup("/etc/magic");
		magicfp = fopen(magicpath, "r");
	}
	if (magicfp == NULL)
		err(1, "%s", magicpath);

	parent = getpid();
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pair) != 0)
		err(1, "socketpair");
	pid = sandbox_fork(FILE_USER);
	if (pid == 0) {
		close(pair[0]);
		child(pair[1], parent, argc, argv);
	}
	close(pair[1]);

	fclose(magicfp);
	magicfp = NULL;

	if (cflag)
		goto wait_for_child;

	imsg_init(&ibuf, pair[0]);
	for (idx = 0; idx < argc; idx++) {
		memset(&msg, 0, sizeof msg);
		msg.idx = idx;

		if (strcmp(argv[idx], "-") == 0) {
			if (fstat(STDIN_FILENO, &msg.sb) == -1) {
				fd = -1;
				msg.error = errno;
			} else
				fd = STDIN_FILENO;
		} else if (lstat(argv[idx], &msg.sb) == -1) {
			fd = -1;
			msg.error = errno;
		} else {
			fd = open(argv[idx], O_RDONLY|O_NONBLOCK);
			if (fd == -1 && (errno == ENFILE || errno == EMFILE))
				err(1, "open");
			if (S_ISLNK(msg.sb.st_mode))
				read_link(&msg, argv[idx]);
		}
		send_message(&ibuf, &msg, sizeof msg, fd);

		if (read_message(&ibuf, &imsg, pid) == 0)
			break;
		if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof *ack)
			errx(1, "message too small");
		ack = imsg.data;
		if (ack->idx != idx)
			errx(1, "index not expected");
		imsg_free(&imsg);
	}

wait_for_child:
	close(pair[0]);
	while (wait(NULL) == -1 && errno != ECHILD) {
		if (errno != EINTR)
			err(1, "wait");
	}
	_exit(0); /* let the child flush */
}

static void
send_message(struct imsgbuf *ibuf, void *msg, size_t msglen, int fd)
{
	if (imsg_compose(ibuf, -1, -1, 0, fd, msg, msglen) != 1)
		err(1, "imsg_compose");
	if (imsg_flush(ibuf) != 0)
		err(1, "imsg_flush");
}

static int
read_message(struct imsgbuf *ibuf, struct imsg *imsg, pid_t from)
{
	int	n;

	if ((n = imsg_read(ibuf)) == -1)
		err(1, "imsg_read");
	if (n == 0)
		return (0);

	if ((n = imsg_get(ibuf, imsg)) == -1)
		err(1, "imsg_get");
	if (n == 0)
		return (0);

	if ((pid_t)imsg->hdr.pid != from)
		errx(1, "PIDs don't match");

	return (n);

}

static void
read_link(struct input_msg *msg, const char *path)
{
	struct stat	 sb;
	char		 lpath[PATH_MAX];
	char		*copy, *root;
	int		 used;
	ssize_t		 size;

	size = readlink(path, lpath, sizeof lpath - 1);
	if (size == -1) {
		msg->link_error = errno;
		return;
	}
	lpath[size] = '\0';

	if (*lpath == '/')
		strlcpy(msg->link_path, lpath, sizeof msg->link_path);
	else {
		copy = xstrdup(path);

		root = dirname(copy);
		if (*root == '\0' || strcmp(root, ".") == 0 ||
		    strcmp (root, "/") == 0)
			strlcpy(msg->link_path, lpath, sizeof msg->link_path);
		else {
			used = snprintf(msg->link_path, sizeof msg->link_path,
			    "%s/%s", root, lpath);
			if (used < 0 || (size_t)used >= sizeof msg->link_path) {
				msg->link_error = ENAMETOOLONG;
				free(copy);
				return;
			}
		}

		free(copy);
	}

	if (Lflag) {
		if (stat(path, &msg->sb) == -1)
			msg->error = errno;
	} else {
		if (stat(path, &sb) == -1)
			msg->link_target = errno;
	}
}

static __dead void
child(int fd, pid_t parent, int argc, char **argv)
{
	struct magic		*m;
	struct imsgbuf		 ibuf;
	struct imsg		 imsg;
	struct input_msg	*msg;
	struct input_ack	 ack;
	struct input_file	 inf;
	int			 i, idx;
	size_t			 len, width = 0;

	m = magic_load(magicfp, magicpath, cflag || Wflag);
	if (cflag) {
		magic_dump(m);
		exit(0);
	}

	for (i = 0; i < argc; i++) {
		len = strlen(argv[i]) + 1;
		if (len > width)
			width = len;
	}

	imsg_init(&ibuf, fd);
	for (;;) {
		if (read_message(&ibuf, &imsg, parent) == 0)
			break;
		if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof *msg)
			errx(1, "message too small");
		msg = imsg.data;

		idx = msg->idx;
		if (idx < 0 || idx >= argc)
			errx(1, "index out of range");

		memset(&inf, 0, sizeof inf);
		inf.m = m;
		inf.msg = msg;

		inf.path = argv[idx];
		inf.fd = imsg.fd;

		test_file(&inf, width);

		if (imsg.fd != -1)
			close(imsg.fd);
		imsg_free(&imsg);

		ack.idx = idx;
		send_message(&ibuf, &ack, sizeof ack, -1);
	}
	exit(0);
}

static void *
fill_buffer(int fd, size_t size, size_t *used)
{
	static void	*buffer;
	ssize_t		 got;
	size_t		 left;
	void		*next;

	if (buffer == NULL)
		buffer = xmalloc(FILE_READ_SIZE);

	next = buffer;
	left = size;
	while (left != 0) {
		got = read(fd, next, left);
		if (got == -1) {
			if (errno == EINTR)
				continue;
			return NULL;
		}
		if (got == 0)
			break;
		next = (char *)next + got;
		left -= got;
	}
	*used = size - left;
	return buffer;
}

static int
load_file(struct input_file *inf)
{
	size_t	used;

	if (inf->msg->sb.st_size == 0 && S_ISREG(inf->msg->sb.st_mode))
		return (0); /* empty file */
	if (inf->msg->sb.st_size == 0 || inf->msg->sb.st_size > FILE_READ_SIZE)
		inf->size = FILE_READ_SIZE;
	else
		inf->size = inf->msg->sb.st_size;

	if (!S_ISREG(inf->msg->sb.st_mode))
		goto try_read;

	inf->base = mmap(NULL, inf->size, PROT_READ, MAP_PRIVATE, inf->fd, 0);
	if (inf->base == MAP_FAILED)
		goto try_read;
	inf->mapped = 1;
	return (0);

try_read:
	inf->base = fill_buffer(inf->fd, inf->size, &used);
	if (inf->base == NULL) {
		xasprintf(&inf->result, "cannot read '%s' (%s)", inf->path,
		    strerror(errno));
		return (1);
	}
	inf->size = used;
	return (0);
}

static int
try_stat(struct input_file *inf)
{
	if (inf->msg->error != 0) {
		xasprintf(&inf->result, "cannot stat '%s' (%s)", inf->path,
		    strerror(inf->msg->error));
		return (1);
	}
	if (sflag || strcmp(inf->path, "-") == 0) {
		switch (inf->msg->sb.st_mode & S_IFMT) {
		case S_IFIFO:
			if (strcmp(inf->path, "-") != 0)
				break;
		case S_IFBLK:
		case S_IFCHR:
		case S_IFREG:
			return (0);
		}
	}

	if (iflag && (inf->msg->sb.st_mode & S_IFMT) != S_IFREG) {
		xasprintf(&inf->result, "application/x-not-regular-file");
		return (1);
	}

	switch (inf->msg->sb.st_mode & S_IFMT) {
	case S_IFDIR:
		xasprintf(&inf->result, "directory");
		return (1);
	case S_IFLNK:
		if (inf->msg->link_error != 0) {
			xasprintf(&inf->result, "unreadable symlink '%s' (%s)",
			    inf->path, strerror(inf->msg->link_error));
			return (1);
		}
		if (inf->msg->link_target == ELOOP)
			xasprintf(&inf->result, "symbolic link in a loop");
		else if (inf->msg->link_target != 0) {
			xasprintf(&inf->result, "broken symbolic link to '%s'",
			    inf->msg->link_path);
		} else {
			xasprintf(&inf->result, "symbolic link to '%s'",
			    inf->msg->link_path);
		}
		return (1);
	case S_IFSOCK:
		xasprintf(&inf->result, "socket");
		return (1);
	case S_IFBLK:
		xasprintf(&inf->result, "block special (%ld/%ld)",
		    (long)major(inf->msg->sb.st_rdev),
		    (long)minor(inf->msg->sb.st_rdev));
		return (1);
	case S_IFCHR:
		xasprintf(&inf->result, "character special (%ld/%ld)",
		    (long)major(inf->msg->sb.st_rdev),
		    (long)minor(inf->msg->sb.st_rdev));
		return (1);
	case S_IFIFO:
		xasprintf(&inf->result, "fifo (named pipe)");
		return (1);
	}
	return (0);
}

static int
try_empty(struct input_file *inf)
{
	if (inf->size != 0)
		return (0);

	if (iflag)
		xasprintf(&inf->result, "application/x-empty");
	else
		xasprintf(&inf->result, "empty");
	return (1);
}

static int
try_access(struct input_file *inf)
{
	char tmp[256] = "";

	if (inf->fd != -1)
		return (0);

	if (inf->msg->sb.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))
		strlcat(tmp, "writable, ", sizeof tmp);
	if (inf->msg->sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))
		strlcat(tmp, "executable, ", sizeof tmp);
	if (S_ISREG(inf->msg->sb.st_mode))
		strlcat(tmp, "regular file, ", sizeof tmp);
	strlcat(tmp, "no read permission", sizeof tmp);

	inf->result = xstrdup(tmp);
	return (1);
}

static int
try_text(struct input_file *inf)
{
	const char	*type, *s;
	int		 flags;

	flags = MAGIC_TEST_TEXT;
	if (iflag)
		flags |= MAGIC_TEST_MIME;

	type = text_get_type(inf->base, inf->size);
	if (type == NULL)
		return (0);

	s = magic_test(inf->m, inf->base, inf->size, flags);
	if (s != NULL) {
		inf->result = xstrdup(s);
		return (1);
	}

	s = text_try_words(inf->base, inf->size, flags);
	if (s != NULL) {
		if (iflag)
			inf->result = xstrdup(s);
		else
			xasprintf(&inf->result, "%s %s text", type, s);
		return (1);
	}

	if (iflag)
		inf->result = xstrdup("text/plain");
	else
		xasprintf(&inf->result, "%s text", type);
	return (1);
}

static int
try_magic(struct input_file *inf)
{
	const char	*s;
	int		 flags;

	flags = 0;
	if (iflag)
		flags |= MAGIC_TEST_MIME;

	s = magic_test(inf->m, inf->base, inf->size, flags);
	if (s != NULL) {
		inf->result = xstrdup(s);
		return (1);
	}
	return (0);
}

static int
try_unknown(struct input_file *inf)
{
	if (iflag)
		xasprintf(&inf->result, "application/x-not-regular-file");
	else
		xasprintf(&inf->result, "data");
	return (1);
}

static void
test_file(struct input_file *inf, size_t width)
{
	char	*label;
	int	 stop;

	stop = 0;
	if (!stop)
		stop = try_stat(inf);
	if (!stop)
		stop = try_access(inf);
	if (!stop)
		stop = load_file(inf);
	if (!stop)
		stop = try_empty(inf);
	if (!stop)
		stop = try_magic(inf);
	if (!stop)
		stop = try_text(inf);
	if (!stop)
		stop = try_unknown(inf);

	if (bflag)
		printf("%s\n", inf->result);
	else {
		if (strcmp(inf->path, "-") == 0)
			xasprintf(&label, "/dev/stdin:");
		else
			xasprintf(&label, "%s:", inf->path);
		printf("%-*s %s\n", (int)width, label, inf->result);
		free(label);
	}
	free(inf->result);

	if (inf->mapped && inf->base != NULL)
		munmap(inf->base, inf->size);
}
