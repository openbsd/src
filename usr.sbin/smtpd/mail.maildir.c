/*
 * Copyright (c) 2017 Gilles Chehade <gilles@poolp.org>
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	MAILADDR_ESCAPE		"!#$%&'*/?^`{|}~"

static int	maildir_subdir(const char *, char *, size_t);
static void	maildir_engine(const char *);
static int	mkdirs_component(const char *, mode_t);
static int	mkdirs(const char *, mode_t);

int
main(int argc, char *argv[])
{
	int	ch;

	if (! geteuid())
		errx(1, "mail.maildir: may not be executed as root");

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		errx(1, "mail.maildir: only one maildir is allowed");

	maildir_engine(argv[0]);

	return (0);
}

static int
maildir_subdir(const char *extension, char *dest, size_t len)
{
	char		*sanitized;

	if (strlcpy(dest, extension, len) >= len)
		return 0;

	for (sanitized = dest; *sanitized; sanitized++)
		if (strchr(MAILADDR_ESCAPE, *sanitized))
			*sanitized = ':';

	return 1;
}

static void
maildir_engine(const char *dirname)
{
	char	root[PATH_MAX];
	char	tmp[PATH_MAX];
	char	new[PATH_MAX];
	char	subdir[PATH_MAX];
	int	fd;
	FILE    *fp;
	char	*line = NULL;
	size_t	linesize = 0;
	ssize_t	linelen;
	struct stat	sb;
	char	*home;
	char	*extension;


	if (dirname == NULL) {
		if ((home = getenv("HOME")) == NULL)
			err(1, NULL);
		(void)snprintf(root, sizeof root, "%s/Maildir", home);
		dirname = root;
	}
	
	if ((extension = getenv("EXTENSION")) != NULL) {
		memset(subdir, 0, sizeof subdir);
		if (! maildir_subdir(extension, subdir, sizeof(subdir)))
			err(1, NULL);

		if (subdir[0]) {
			(void)snprintf(tmp, sizeof tmp, "%s/.%s", dirname, subdir);
			if (stat(tmp, &sb) != -1)
				dirname = tmp;
		}
	}

	if (mkdirs(dirname, 0700) < 0 && errno != EEXIST)
		err(1, NULL);

	if (chdir(dirname) < 0)
		err(1, NULL);

	if (mkdir("cur", 0700) < 0 && errno != EEXIST)
		err(1, NULL);
	if (mkdir("tmp", 0700) < 0 && errno != EEXIST)
		err(1, NULL);
	if (mkdir("new", 0700) < 0 && errno != EEXIST)
		err(1, NULL);

	(void)snprintf(tmp, sizeof tmp, "tmp/%lld.%08x.%s",
	    (long long int) time(NULL),
	    arc4random(),
	    "localhost");

	fd = open(tmp, O_CREAT | O_EXCL | O_WRONLY, 0600);
	if (fd < 0)
		err(1, NULL);
	if ((fp = fdopen(fd, "w")) == NULL)
		err(1, NULL);

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		line[strcspn(line, "\n")] = '\0';
		fprintf(fp, "%s\n", line);
	}
	free(line);
	if (ferror(stdin))
		err(1, NULL);

	
	if (fflush(fp) == EOF ||
	    ferror(fp) ||
	    fsync(fd) < 0 ||
	    fclose(fp) == EOF)
		err(1, NULL);

	(void)snprintf(new, sizeof new, "new/%s", tmp + 4);
	if (rename(tmp, new) < 0)
		err(1, NULL);

	exit(0);
}


static int
mkdirs_component(const char *path, mode_t mode)
{
	struct stat	sb;

	if (stat(path, &sb) == -1) {
		if (errno != ENOENT)
			return 0;
		if (mkdir(path, mode | S_IWUSR | S_IXUSR) == -1)
			return 0;
	}
	else if (!S_ISDIR(sb.st_mode))
		return 0;

	return 1;
}

static int
mkdirs(const char *path, mode_t mode)
{
	char	 buf[PATH_MAX];
	int	 i = 0;
	int	 done = 0;
	const char	*p;

	/* absolute path required */
	if (*path != '/')
		return 0;

	/* make sure we don't exceed PATH_MAX */
	if (strlen(path) >= sizeof buf)
		return 0;

	memset(buf, 0, sizeof buf);
	for (p = path; *p; p++) {
		if (*p == '/') {
			if (buf[0] != '\0')
				if (!mkdirs_component(buf, mode))
					return 0;
			while (*p == '/')
				p++;
			buf[i++] = '/';
			buf[i++] = *p;
			if (*p == '\0' && ++done)
				break;
			continue;
		}
		buf[i++] = *p;
	}
	if (!done)
		if (!mkdirs_component(buf, mode))
			return 0;

	if (chmod(path, mode) == -1)
		return 0;

	return 1;
}
