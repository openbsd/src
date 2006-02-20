/*	$OpenBSD: common.c,v 1.1 2006/02/20 08:38:18 otto Exp $	*/

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"

void
cleanup(const char *filename)
{
	if (unlink(filename))
		err(2, "could not delete: %s", filename);
	exit(2);
}

/*
 * Creates and returns the name of a temporary file.  Takes a string
 * (or NULL) is written to the temporary file.  The returned string
 * needs to be freed.
 */
char *
xmktemp(const char *s)
{
	FILE *file;
	int fd;
	const char *tmpdir;
	char *filename;

	/* If TMPDIR is set, use it; otherwise use /tmp. */
	if (!(tmpdir = getenv("TMPDIR")))
		tmpdir = "/tmp";
	if (asprintf(&filename, "%s/sdiff.XXXXXXXXXX", tmpdir) == -1)
		err(2, "xmktemp");

	/* Create temp file. */
	if ((fd = mkstemp(filename)) == -1)
		err(2, "could not create temporary file");

	/* If we don't write anything to the file, just close. */
	if (s == NULL) {
		close(fd);

		return (filename);
	}

	/* Open temp file for writing. */
	if ((file = fdopen(fd, "w")) == NULL) {
		warn("could not open %s", filename);
		cleanup(filename);
		/* NOTREACHED */
	}

	/* Write to file. */
	if (fputs(s, file)) {
		warn("could not write to %s", filename);
		cleanup(filename);
		/* NOTREACHED */
	}

	/* Close temp file. */
	fclose(file);

	return (filename);
}
