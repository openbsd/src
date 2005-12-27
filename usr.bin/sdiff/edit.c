/*	$OpenBSD: edit.c,v 1.3 2005/12/27 04:18:07 tedu Exp $ */

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

__dead static void cleanup(const char *);
static void edit(const char *);
static char *xmktemp(const char *);

static void
cleanup(const char *filename)
{
	if (unlink(filename))
		err(2, "could not delete: %s", filename);
	exit(2);
}

/*
 * Takes the name of a file and opens it with an editor.
 */
static void
edit(const char *filename)
{
	int status;
	pid_t pid;
	const char *editor;

	editor = getenv("VISUAL");
	if (editor == NULL)
		editor = getenv("EDITOR");
	if (editor == NULL)
		editor = "vi";

	/* Start editor on temporary file. */
	switch (pid = fork()) {
	case 0:
		/* child */
		execlp(editor, editor, filename, (void *)NULL);
		warn("could not execute editor: %s", editor);
		cleanup(filename);
		/* NOTREACHED */
	case -1:
		warn("could not fork");
		cleanup(filename);
		/* NOTREACHED */
	}

	/* parent */
	/* Wait for editor to exit. */
	if (waitpid(pid, &status, 0) == -1) {
		warn("waitpid");
		cleanup(filename);
		/* NOTREACHED */
	}

	/* Check that editor terminated normally. */
	if (!WIFEXITED(status)) {
		warn("%s terminated abnormally", editor);
		cleanup(filename);
		/* NOTREACHED */
	}
}

/*
 * Creates and returns the name of a temporary file.  Takes a string
 * (or NULL) is written to the temporary file.  The returned string
 * needs to be freed.
 */
static char *
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
		err(2, "could not allocate memory");

	/* Create temp file. */
	if ((fd = mkstemp(filename)) == -1)
		err(2, "could not create temporary file");

	/* If we don't write anything to the file, just close. */
	if (s == NULL) {
		if (close(fd)) {
			warn("could not close %s", filename);
			cleanup(filename);
			/* NOTREACHED */
		}

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
	if (fclose(file)) {
		warn("could not close %s", filename);
		cleanup(filename);
		/* NOTREACHED */
	}

	return (filename);
}

/*
 * Parse edit command.  Returns 0 on success, -1 on error.
 */
int
eparse(const char *cmd, const char *left, const char *right)
{
	FILE *file;
	size_t nread, nwritten;
	const char *filename;
	char buf[BUFSIZ], *text;

	assert(cmd);

	/* Skip whitespace. */
	while (isspace(*cmd))
		++cmd;

	text = NULL;
	switch (*cmd) {
	case '\0':
		/* Edit empty file. */
		break;

	case 'b':
		/* Both strings. */
		if (left == NULL)
			goto RIGHT;
		if (right == NULL) 
			goto LEFT;

		/* Neither column is blank, so print both. */
		if (asprintf(&text, "%s%s\n", left, right) == -1)
			err(2, "could not allocate memory");
		break;

LEFT:
	case 'l':
		/* Skip if there is no left column. */
		if (left == NULL)
			break;

		if (asprintf(&text, "%s\n", left) == -1)
			err(2, "could not allocate memory");

		break;

RIGHT:
	case 'r':
		/* Skip if there is no right column. */
		if (right == NULL)
			break;

		if (asprintf(&text, "%s\n", right) == -1)
			err(2, "could not allocate memory");

		break;

	default:
		return (-1);
	}

	/* Create temp file. */
	filename = xmktemp(text);

	/* text is no longer used. */
	free(text);

	/* Edit temp file. */
	edit(filename);

	/* Open temporary file. */
	if (!(file = fopen(filename, "r"))) {
		warn("could not open edited file: %s", filename);
		cleanup(filename);
		/* NOTREACHED */
	}

	/* Copy temporary file contents to output file. */
	for (nread = sizeof(buf); nread == sizeof(buf);) {
		nread = fread(buf, sizeof(*buf), sizeof(buf), file);
		/* Test for error or end of file. */
		if (nread != sizeof(buf) &&
		    (ferror(file) || !feof(file))) {
			warnx("error reading edited file: %s", filename);
			cleanup(filename);
			/* NOTREACHED */
		}

		/*
		 * If we have nothing to read, break out of loop
		 * instead of writing nothing.
		 */
		if (!nread)
			break;

		/* Write data we just read. */
		nwritten = fwrite(buf, sizeof(*buf), nread, outfile);
		if (nwritten != nread) {
			warnx("error writing to output file");
			cleanup(filename);
			/* NOTREACHED */
		}
	}

	/* We've reached the end of the temporary file, so remove it. */
	if (unlink(filename))
		warn("could not delete: %s", filename);
	if (fclose(file))
		warn("could not close: %s", filename);

	/* filename was malloc()ed in xmktemp(). */
	free(filename);

	return (0);
}
