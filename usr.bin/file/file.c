/* $OpenBSD: file.c,v 1.64 2017/07/01 21:07:13 brynet Exp $ */

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
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "magic.h"
#include "xmalloc.h"

struct input_file {
	struct magic	*m;

	const char	*path;
	struct stat	 sb;
	int		 fd;
	int		 error;

	char		 link_path[PATH_MAX];
	int		 link_error;
	int		 link_target;

	void		*base;
	size_t		 size;
	int		 mapped;
	char		*result;
};

extern char	*__progname;

__dead void	 usage(void);

static void	 prepare_input(struct input_file *, const char *);

static void	 read_link(struct input_file *, const char *);

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

static struct option longopts[] = {
	{ "brief",       no_argument, NULL, 'b' },
	{ "dereference", no_argument, NULL, 'L' },
	{ "mime",        no_argument, NULL, 'i' },
	{ "mime-type",   no_argument, NULL, 'i' },
	{ NULL,          0,           NULL, 0   }
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
	int			 opt, idx;
	char			*home, *magicpath;
	struct passwd		*pw;
	FILE			*magicfp = NULL;
	struct magic		*m;
	struct input_file	*inf = NULL;
	size_t			 len, width = 0;

	if (pledge("stdio rpath getpw id", NULL) == -1)
		err(1, "pledge");

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

	if (!cflag) {
		inf = xcalloc(argc, sizeof *inf);
		for (idx = 0; idx < argc; idx++) {
			len = strlen(argv[idx]) + 1;
			if (len > width)
				width = len;
			prepare_input(&inf[idx], argv[idx]);
		}
	}

	tzset();

	if (pledge("stdio getpw id", NULL) == -1)
		err(1, "pledge");

	if (geteuid() == 0) {
		pw = getpwnam(FILE_USER);
		if (pw == NULL)
			errx(1, "unknown user %s", FILE_USER);
		if (setgroups(1, &pw->pw_gid) != 0)
			err(1, "setgroups");
		if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) != 0)
			err(1, "setresgid");
		if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) != 0)
			err(1, "setresuid");
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	m = magic_load(magicfp, magicpath, cflag || Wflag);
	if (cflag) {
		magic_dump(m);
		exit(0);
	}
	fclose(magicfp);

	for (idx = 0; idx < argc; idx++) {
		inf[idx].m = m;
		test_file(&inf[idx], width);
		if (inf[idx].fd != -1 && inf[idx].fd != STDIN_FILENO)
			close(inf[idx].fd);
	}
	exit(0);
}

static void
prepare_input(struct input_file *inf, const char *path)
{
	int	fd, mode, error;

	inf->path = path;

	if (strcmp(path, "-") == 0) {
		if (fstat(STDIN_FILENO, &inf->sb) == -1) {
			inf->error = errno;
			inf->fd = -1;
			return;
		}
		inf->fd = STDIN_FILENO;
		return;
	}

	if (Lflag)
		error = stat(path, &inf->sb);
	else
		error = lstat(path, &inf->sb);
	if (error == -1) {
		inf->error = errno;
		inf->fd = -1;
		return;
	}

	/* We don't need them, so don't open directories or symlinks. */
	mode = inf->sb.st_mode;
	if (!S_ISDIR(mode) && !S_ISLNK(mode)) {
		fd = open(path, O_RDONLY|O_NONBLOCK);
		if (fd == -1 && (errno == ENFILE || errno == EMFILE))
			err(1, "open");
	} else
		fd = -1;
	if (S_ISLNK(mode))
		read_link(inf, path);
	inf->fd = fd;
}

static void
read_link(struct input_file *inf, const char *path)
{
	struct stat	 sb;
	char		 lpath[PATH_MAX];
	char		*copy, *root;
	int		 used;
	ssize_t		 size;

	size = readlink(path, lpath, sizeof lpath - 1);
	if (size == -1) {
		inf->link_error = errno;
		return;
	}
	lpath[size] = '\0';

	if (*lpath == '/')
		strlcpy(inf->link_path, lpath, sizeof inf->link_path);
	else {
		copy = xstrdup(path);

		root = dirname(copy);
		if (*root == '\0' || strcmp(root, ".") == 0 ||
		    strcmp (root, "/") == 0)
			strlcpy(inf->link_path, lpath, sizeof inf->link_path);
		else {
			used = snprintf(inf->link_path, sizeof inf->link_path,
			    "%s/%s", root, lpath);
			if (used < 0 || (size_t)used >= sizeof inf->link_path) {
				inf->link_error = ENAMETOOLONG;
				free(copy);
				return;
			}
		}

		free(copy);
	}

	if (!Lflag && stat(path, &sb) == -1)
		inf->link_target = errno;
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
			return (NULL);
		}
		if (got == 0)
			break;
		next = (char *)next + got;
		left -= got;
	}
	*used = size - left;
	return (buffer);
}

static int
load_file(struct input_file *inf)
{
	size_t	used;

	if (inf->sb.st_size == 0 && S_ISREG(inf->sb.st_mode))
		return (0); /* empty file */
	if (inf->sb.st_size == 0 || inf->sb.st_size > FILE_READ_SIZE)
		inf->size = FILE_READ_SIZE;
	else
		inf->size = inf->sb.st_size;

	if (!S_ISREG(inf->sb.st_mode))
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
	if (inf->error != 0) {
		xasprintf(&inf->result, "cannot stat '%s' (%s)", inf->path,
		    strerror(inf->error));
		return (1);
	}
	if (sflag || strcmp(inf->path, "-") == 0) {
		switch (inf->sb.st_mode & S_IFMT) {
		case S_IFIFO:
			if (strcmp(inf->path, "-") != 0)
				break;
		case S_IFBLK:
		case S_IFCHR:
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
		if (inf->link_error != 0) {
			xasprintf(&inf->result, "unreadable symlink '%s' (%s)",
			    inf->path, strerror(inf->link_error));
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
		    (long)major(inf->sb.st_rdev),
		    (long)minor(inf->sb.st_rdev));
		return (1);
	case S_IFCHR:
		xasprintf(&inf->result, "character special (%ld/%ld)",
		    (long)major(inf->sb.st_rdev),
		    (long)minor(inf->sb.st_rdev));
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

	if (inf->sb.st_size == 0 && S_ISREG(inf->sb.st_mode))
		return (0); /* empty file */
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
