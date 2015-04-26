/* $OpenBSD: file.c,v 1.33 2015/04/26 19:53:50 nicm Exp $ */

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

#include <errno.h>
#include <libgen.h>
#include <getopt.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#include "file.h"
#include "magic.h"
#include "xmalloc.h"

struct input_file
{
	struct magic	*m;

	const char	*path;
	const char	*label;

	int		 fd;
	struct stat	 sb;
	const char	*error;

	void		*base;
	size_t           size;
	int		 mapped;
	char		*result;

	char		 link_path[PATH_MAX];
	const char	*link_error;
	int		 link_target;
};

extern char	*__progname;

__dead void	 usage(void);

static void	 prepare_file(struct input_file *, const char *, int *);
static void	 open_file(struct input_file *);
static void	 read_link(struct input_file *);
static void	 test_file(struct magic *, struct input_file *, int);

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

static struct option longopts[] = {
	{ "mime",      no_argument, NULL, 'i' },
	{ "mime-type", no_argument, NULL, 'i' },
	{ NULL,        0,           NULL, 0   }
};

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-bchiLsW] [file ...]\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct input_file	*files = NULL;
	int			 opt, i, width = 0;
	FILE			*f;
	struct magic		*m;
	char			*home, *path;
	struct passwd		*pw;

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

	f = NULL;
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
			xasprintf(&path, "%s/.magic", home);
			f = fopen(path, "r");
			if (f == NULL && errno != ENOENT)
				err(1, "%s", path);
			if (f == NULL)
				free(path);
		}
	}
	if (f == NULL) {
		path = xstrdup("/etc/magic");
		f = fopen(path, "r");
	}
	if (f == NULL)
		err(1, "%s", path);

	m = magic_load(f, path, cflag || Wflag);
	if (cflag) {
		magic_dump(m);
		exit(0);
	}

	files = xcalloc(argc, sizeof *files);
	for (i = 0; i < argc; i++)
		prepare_file(&files[i], argv[i], &width);
	for (i = 0; i < argc; i++) {
		open_file(&files[i]);
		test_file(m, &files[i], width);
	}
	exit(0);
}

static void
prepare_file(struct input_file *inf, const char *path, int *width)
{
	char	*label;
	int	 n;

	inf->path = xstrdup(path);

	n = xasprintf(&label, "%s:", inf->path);
	if (n > *width)
		*width = n;
	inf->label = label;
}

static void
open_file(struct input_file *inf)
{
	int	 retval;

	retval = lstat(inf->path, &inf->sb);
	if (retval == -1) {
		inf->error = strerror(errno);
		return;
	}

	if (S_ISLNK(inf->sb.st_mode))
		read_link(inf);
	inf->fd = open(inf->path, O_RDONLY|O_NONBLOCK);
}

static void
read_link(struct input_file *inf)
{
	struct stat	 sb;
	char		 path[PATH_MAX];
	char		*copy, *root;
	int		 used;
	ssize_t		 size;

	size = readlink(inf->path, path, sizeof path);
	if (size == -1) {
		inf->link_error = strerror(errno);
		return;
	}
	path[size] = '\0';

	if (*path == '/')
		strlcpy(inf->link_path, path, sizeof inf->link_path);
	else {
		copy = xstrdup(inf->path);

		root = dirname(copy);
		if (*root == '\0' || strcmp(root, ".") == 0 ||
		    strcmp (root, "/") == 0)
			strlcpy(inf->link_path, path, sizeof inf->link_path);
		else {
			used = snprintf(inf->link_path, sizeof inf->link_path,
			    "%s/%s", root, path);
			if (used < 0 || (size_t)used >= sizeof inf->link_path) {
				inf->link_error = strerror(ENAMETOOLONG);
				return;
			}
		}

		free(copy);
	}

	if (Lflag) {
		if (stat(inf->path, &inf->sb) == -1)
			inf->error = strerror(errno);
	} else {
		if (stat(inf->path, &sb) == -1)
			inf->link_target = errno;
	}
}

static void *
fill_buffer(struct input_file *inf)
{
	static void	*buffer;
	ssize_t		 got;
	size_t		 left;
	void		*next;

	if (buffer == NULL)
		buffer = xmalloc(FILE_READ_SIZE);

	next = buffer;
	left = inf->size;
	while (left != 0) {
		got = read(inf->fd, next, left);
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

	return buffer;
}

static int
load_file(struct input_file *inf)
{
	int	available;

	inf->size = inf->sb.st_size;
	if (inf->size > FILE_READ_SIZE)
		inf->size = FILE_READ_SIZE;
	if (S_ISFIFO(inf->sb.st_mode)) {
		if (ioctl(inf->fd, FIONREAD, &available) == -1) {
			xasprintf(&inf->result,  "cannot read '%s' (%s)",
			    inf->path, strerror(errno));
			return (1);
		}
		inf->size = available;
	} else if (!S_ISREG(inf->sb.st_mode) && inf->size == 0)
		inf->size = FILE_READ_SIZE;
	if (inf->size == 0)
		return (0);

	inf->base = mmap(NULL, inf->size, PROT_READ, MAP_PRIVATE, inf->fd, 0);
	if (inf->base == MAP_FAILED) {
		inf->base = fill_buffer(inf);
		if (inf->base == NULL) {
			xasprintf(&inf->result, "cannot read '%s' (%s)",
			    inf->path, strerror(errno));
			return (1);
		}
	} else
		inf->mapped = 1;
	return (0);
}

static int
try_stat(struct input_file *inf)
{
	if (inf->error != NULL) {
		xasprintf(&inf->result, "cannot stat '%s' (%s)", inf->path,
		    inf->error);
		return (1);
	}
	if (sflag) {
		switch (inf->sb.st_mode & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
		case S_IFREG:
			return (0);
		}
	}

	if (iflag && (inf->sb.st_mode & S_IFMT) != S_IFREG) {
		xasprintf(&inf->result, "application/x-not-regular-file");
		return (1);
	}


	switch (inf->sb.st_mode & S_IFMT) {
	case S_IFDIR:
		xasprintf(&inf->result, "directory");
		return (1);
	case S_IFLNK:
		if (inf->link_error != NULL) {
			xasprintf(&inf->result, "unreadable symlink '%s' (%s)",
			    inf->path, inf->link_error);
			return (1);
		}
		if (inf->link_target == ELOOP)
			xasprintf(&inf->result, "symbolic link in a loop");
		else if (inf->link_target != 0) {
			xasprintf(&inf->result, "broken symbolic link to '%s'",
			    inf->link_path);
		} else {
			xasprintf(&inf->result, "symbolic link to '%s'",
			    inf->link_path);
		}
		return (1);
	case S_IFSOCK:
		xasprintf(&inf->result, "socket");
		return (1);
	case S_IFBLK:
		xasprintf(&inf->result, "block special (%ld/%ld)",
		    (long)major(inf->sb.st_rdev), (long)minor(inf->sb.st_rdev));
		return (1);
	case S_IFCHR:
		xasprintf(&inf->result, "character special (%ld/%ld)",
		    (long)major(inf->sb.st_rdev), (long)minor(inf->sb.st_rdev));
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

	if (inf->sb.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))
		strlcat(tmp, "writable, ", sizeof tmp);
	if (inf->sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))
		strlcat(tmp, "executable, ", sizeof tmp);
	if (S_ISREG(inf->sb.st_mode))
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
test_file(struct magic *m, struct input_file *inf, int width)
{
	int	stop;

	inf->m = m;

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
	else
		printf("%-*s %s\n", width, inf->label, inf->result);
	free(inf->result);

	if (inf->mapped && inf->base != NULL)
		munmap(inf->base, inf->size);
	inf->base = NULL;

	if (inf->fd != -1)
		close(inf->fd);
	free((void *)inf->label);
	free((void *)inf->path);
}
