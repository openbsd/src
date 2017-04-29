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

static void	file_engine(const char *);

int
main(int argc, char *argv[])
{
	int ch;
	
	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		errx(1, "mail.file: filename required");

	if (argc > 1)
		errx(1, "mail.file: only one filename is supported");

	file_engine(argv[0]);
	
	return (0);
}

static void
file_engine(const char *filename)
{
	int	fd;
	FILE    *fp;
	char	*line = NULL;
	size_t	linesize = 0;
	ssize_t	linelen;
	int	n;
	struct stat sb;
	int	escaped = 0;
	
	fd = open(filename, O_CREAT | O_APPEND | O_WRONLY, 0600);
	if (fd < 0)
		err(1, NULL);
	if (fstat(fd, &sb) < 0)
		err(1, NULL);
	if (S_ISREG(sb.st_mode) && flock(fd, LOCK_EX) < 0)
		err(1, NULL);

	if ((fp = fdopen(fd, "a")) == NULL)
		err(1, NULL);

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		line[strcspn(line, "\n")] = '\0';
		if (strncasecmp(line, "From ", 5) == 0) {
			if (!escaped)
				escaped = 1;
			else
				fprintf(fp, ">");
		}
		fprintf(fp, "%s\n", line);
	}
	free(line);
	if (ferror(stdin))
		goto truncate;

	if (fflush(fp) == -1)
		if (errno != EINVAL)
			goto truncate;

	if (fclose(fp) == EOF)
		goto truncate;
	
	exit(0);

truncate:
	n = errno;
	ftruncate(fd, sb.st_size);
	errno = n;
	err(1, NULL);
}
