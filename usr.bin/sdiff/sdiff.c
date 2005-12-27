/*	$OpenBSD: sdiff.c,v 1.11 2005/12/27 05:13:14 tedu Exp $ */

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "extern.h"

#define WIDTH 130
/*
 * Each column must be at least one character wide, plus three
 * characters between the columns (space, [<|>], space).
 */
#define WIDTH_MIN 5

/* A single diff line. */
struct diffline {
	SIMPLEQ_ENTRY(diffline) diffentries;
	const char	*left;
	char		 div;
	const char	*right;
};

static void astrcat(char **, const char *);
static void enqueue(const char *, const char, const char *);
static void freediff(const struct diffline *);
static void int_usage(void);
static int parsecmd(FILE *, FILE *);
static void printa(FILE *, size_t);
static void printc(FILE *, size_t, FILE *, size_t);
static void printcol(const char *, size_t *, const size_t);
static void printd(FILE *, FILE *, size_t);
static void println(const char *, const char, const char *);
static void processq(void);
static void prompt(const char *, const char *);
static void undiff(char *);
__dead static void usage(void);
static char *xfgets(FILE *);

SIMPLEQ_HEAD(, diffline) diffhead = SIMPLEQ_HEAD_INITIALIZER(diffhead);
size_t	 line_width;	/* width of a line (two columns and divider) */
size_t	 width;		/* width of each column */
size_t	 file1ln, file2ln;	/* line number of file1 and file2 */
int	 lflag;		/* print only left column for identical lines */
int	 sflag;		/* skip identical lines */
FILE	*outfile;	/* file to save changes to */

static struct option longopts[] = {
	{ "text",			no_argument,		NULL,	'a' },
	{ "ignore-blank-lines",		no_argument,		NULL,	'B' },
	{ "ignore-space-change",	no_argument,		NULL,	'b' },
	{ "minimal",			no_argument,		NULL,	'd' },
	{ "ignore-tab-expansion",	no_argument,		NULL,	'E' },
	{ "diff-program",		required_argument,	NULL,	'F' },
	{ "speed-large-files",		no_argument,		NULL,	'H' },
	{ "ignore-matching-lines",	required_argument,	NULL,	'I' },
	{ "left-column",		no_argument,		NULL,	'l' },
	{ "output",			required_argument,	NULL,	'o' },
	{ "strip-trailing-cr",		no_argument,		NULL,	'S' },
	{ "suppress-common-lines",	no_argument,		NULL,	's' },
	{ "expand-tabs",		no_argument,		NULL,	't' },
	{ "ignore-all-space",		no_argument,		NULL,	'W' },
	{ "width",			required_argument,	NULL,	'w' },
	{ NULL,				0,			NULL,	 0  }
};

int
main(int argc, char **argv)
{
	FILE *difffile, *origfile;
	size_t diffargc = 0, wflag = WIDTH;
	int ch, fd[2], status;
	pid_t pid;
	const char *cmd, **diffargv, *diffprog = "diff";

	/*
	 * Process diff flags.
	 */
	/*
	 * Allocate memory for diff arguments and NULL.
	 * Each flag has at most one argument, so doubling argc gives an
	 * upper limit of how many diff args can be passed.  argv[0],
	 * file1, and file2 won't have arguments so doubling them will
	 * waste some memory; however we need an extra space for the
	 * NULL at the end, so it sort of works out.
	 */
	if (!(diffargv = malloc(sizeof(char **) * argc * 2)))
		err(2, "main");

	/* Add first argument, the program name. */
	diffargv[diffargc++] = diffprog;

	while ((ch = getopt_long(argc, argv, "aBbdEHI:ilo:stWw:",
	    longopts, NULL)) != -1) {
		const char *errstr;

		switch (ch) {
		case 'a':
			diffargv[diffargc++] = "-a";
			break;
		case 'B':
			diffargv[diffargc++] = "-B";
			break;
		case 'b':
			diffargv[diffargc++] = "-b";
			break;
		case 'd':
			diffargv[diffargc++] = "-d";
			break;
		case 'E':
			diffargv[diffargc++] = "-E";
			break;
		case 'F':
			diffargv[0] = diffprog = optarg;
			break;
		case 'H':
			diffargv[diffargc++] = "-H";
			break;
		case 'I':
			diffargv[diffargc++] = "-I";
			diffargv[diffargc++] = optarg;
			break;
		case 'i':
			diffargv[diffargc++] = "-i";
			break;
		case 'l':
			lflag = 1;
			break;
		case 'o':
			if ((outfile = fopen(optarg, "w")) == NULL)
				err(2, "could not open: %s", optarg);
			break;
		case 'S':
			diffargv[diffargc++] = "--strip-trailing-cr";
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			diffargv[diffargc++] = "-t";
			break;
		case 'W':
			diffargv[diffargc++] = "-w";
			break;
		case 'w':
			wflag = strtonum(optarg, WIDTH_MIN,
			    INT_MAX, &errstr);
			if (errstr)
				errx(2, "width is %s: %s", errstr, optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	}
	argc -= optind;
	argv += optind;

	if (argc != 2) {
		usage();
		/* NOTREACHED */
	}

	/* file1 */
	diffargv[diffargc++] = argv[0];
	/* file2 */
	diffargv[diffargc++] = argv[1];
	/* Add NULL to end of array to indicate end of array. */
	diffargv[diffargc++] = NULL;

	/* Subtract column divider and divide by two. */
	width = (wflag - 3) / 2;
	/* Make sure line_width can fit in size_t. */
	if (width > (SIZE_T_MAX - 3) / 2)
		errx(2, "width is too large: %zu", width);
	line_width = width * 2 + 3;

	if (pipe(fd))
		err(2, "pipe");

	switch(pid = fork()) {
	case 0:
		/* child */
		/* We don't read from the pipe. */
		close(fd[0]);
		if (dup2(fd[1], STDOUT_FILENO) == -1)
			err(2, "child could not duplicate descriptor");
		/* Free unused descriptor. */
		close(fd[1]);

		execvp(diffprog, (char *const *)diffargv);
		err(2, "could not execute diff: %s", diffprog);
	case -1:
		err(2, "could not fork");
	}

	/* parent */
	/* We don't write to the pipe. */
	close(fd[1]);

	/* Open pipe to diff command. */
	if ((difffile = fdopen(fd[0], "r")) == NULL)
		err(2, "could not open diff pipe");
	/* If file1 was given as `-', open stdin. */
	/* XXX - Does not work. */
	if (strcmp(argv[0], "-") == 0)
		origfile = stdin;
	/* Otherwise, open as normal file. */
	else if ((origfile = fopen(argv[0], "r")) == NULL)
		err(2, "could not open file1: %s", argv[0]);
	/* Line numbers start at one. */
	file1ln = file2ln = 1;

	/* Read and parse diff output. */
	while (parsecmd(difffile, origfile) != EOF)
		;
	fclose(difffile);

	/* Wait for diff to exit. */
	if (waitpid(pid, &status, 0) == -1 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) >= 2)
		err(2, "diff exited abnormally");

	/* No more diffs, so print common lines. */
	while ((cmd = xfgets(origfile)))
		enqueue(cmd, ' ', lflag ? NULL : cmd);
	fclose(origfile);
	/* Process unmodified lines. */
	processq();

	/* Return diff exit status. */
	return (WEXITSTATUS(status));
}

/*
 * Prints an individual column (left or right), taking into account
 * that tabs are variable-width.  Takes a string, the current column
 * the cursor is on the screen, and the maximum value of the column.  
 * The column value is updated as we go along.
 */
static void
printcol(const char *s, size_t *col, const size_t col_max)
{

	for (; *s && *col < col_max; ++s) {
		size_t new_col;

		switch (*s) {
		case '\t':
			/*
			 * If rounding to next multiple of eight causes
			 * an integer overflow, just return.
			 */
			if (*col > SIZE_T_MAX - 8)
				return;

			/* Round to next multiple of eight. */
			new_col = (*col / 8 + 1) * 8;

			/*
			 * If printing the tab goes past the column
			 * width, don't print it and just quit.
			 */
			if (new_col > col_max)
				return;
			*col = new_col;
			break;

		default:
			++(*col);
		}

		putchar(*s);
	}
}

/*
 * Prompts user to either choose between two strings or edit one, both,
 * or neither.
 */
static void
prompt(const char *s1, const char *s2)
{
	const char *cmd;

	/* Print command prompt. */
	putchar('%');

	/* Get user input. */
	for (; (cmd = xfgets(stdin)); free((void *)cmd)) {
		const char *p;

		/* Skip leading whitespace. */
		for (p = cmd; isspace(*p); ++p)
			;

		switch (*p) {
		case 'e':
			/* Skip `e'. */
			++p;

			if (eparse(p, s1, s2) == -1)
				goto USAGE;
			break;

		case 'l':
			/* Choose left column as-is. */
			if (s1 != NULL)
				fprintf(outfile, "%s\n", s1);

			/* End of command parsing. */
			break;

		case 'q':
			goto QUIT;

		case 'r':
			/* Choose right column as-is. */
			if (s2 != NULL)
				fprintf(outfile, "%s\n", s2);

			/* End of command parsing. */
			break;

		case 's':
			sflag = 1;
			goto PROMPT;

		case 'v':
			sflag = 0;
			/* FALLTHROUGH */

		default:
			/* Interactive usage help. */
USAGE:
			int_usage();
PROMPT:
			putchar('%');

			/* Prompt user again. */
			continue;
		}

		free((void *)cmd);
		return;
	}

	/*
	 * If there was no error, we received an EOF from stdin, so we
	 * should quit.
	 */
QUIT:
	fclose(outfile);
	exit(0);
}

/*
 * Takes two strings, separated by a column divider.  NULL strings are
 * treated as empty columns.  If the divider is the ` ' character, the
 * second column is not printed (-l flag).  In this case, the second
 * string must be NULL.  When the second column is NULL, the divider
 * does not print the trailing space following the divider character.
 *
 * Takes into account that tabs can take multiple columns.
 */
static void
println(const char *s1, const char div, const char *s2)
{
	size_t col;

	/* Print first column.  Skips if s1 == NULL. */
	col = 0;
	if (s1) {
		/* Skip angle bracket and space. */
		printcol(s1, &col, width);

	}

	/* Only print left column. */
	if (div == ' ' && !s2) {
		putchar('\n');
		return;
	}

	/* Otherwise, we pad this column up to width. */
	for (; col < width; ++col)
		putchar(' ');

	/*
	 * Print column divider.  If there is no second column, we don't
	 * need to add the space for padding.
	 */
	if (!s2) {
		printf(" %c\n", div);
		return;
	}
	printf(" %c ", div);
	col += 3;

	/* Skip angle bracket and space. */
	printcol(s2, &col, line_width);

	putchar('\n');
}

/*
 * Reads a line from file and returns as a string.  If EOF is reached,
 * NULL is returned.  The returned string must be freed afterwards.
 */
static char *
xfgets(FILE *file)
{
	const char delim[3] = {'\0', '\0', '\0'};
	char *s;

	/* XXX - Is this necessary? */
	clearerr(file);

	if (!(s = fparseln(file, NULL, NULL, delim, 0)) &&
	    ferror(file))
		err(2, "error reading file");

	if (!s) {
		return (NULL);
	}

	return (s);
}

/*
 * Parse ed commands from diff and print lines from difffile
 * (lines to add or change) or origfile (lines to change or delete).
 * Returns EOF or not.
 */
static int
parsecmd(FILE *difffile, FILE *origfile)
{
	size_t file1start, file1end, file2start, file2end;
	/* ed command line and pointer to characters in line */
	char *line, *p, *q;
	const char *errstr;
	char c, cmd;

	/* Read ed command. */
	if (!(line = xfgets(difffile)))
		return (EOF);

	p = line;
	/* Go to character after line number. */
	while (isdigit(*p))
		++p;
	c = *p;
	*p++ = 0;
	file1start = strtonum(line, 0, INT_MAX, &errstr);
	if (errstr)
		errx(2, "file1 start is %s: %s", errstr, line);

	/* A range is specified for file1. */
	if (c == ',') {

		q = p;
		/* Go to character after file2end. */
		while (isdigit(*p))
			++p;
		c = *p;
		*p++ = 0;
		file1end = strtonum(q, 0, INT_MAX, &errstr);
		if (errstr)
			errx(2, "file1 end is %s: %s", errstr, line);
		if (file1start > file1end)
			errx(2, "invalid line range in file1: %s", line);

	} else
		file1end = file1start;

	cmd = c;
	/* Check that cmd is valid. */
	if (!(cmd == 'a' || cmd == 'c' || cmd == 'd'))
		errx(2, "ed command not recognized: %c: %s", cmd, line);

	q = p;
	/* Go to character after line number. */
	while (isdigit(*p))
		++p;
	c = *p;
	*p++ = 0;
	file2start = strtonum(q, 0, INT_MAX, &errstr);
	if (errstr)
		errx(2, "file2 start is %s: %s", errstr, line);

	/*
	 * There should either be a comma signifying a second line
	 * number or the line should just end here.
	 */
	if (c != ',' && c != '\0')
		errx(2, "invalid line range in file2: %c: %s", c, line);

	if (c == ',') {

		file2end = strtonum(p, 0, INT_MAX, &errstr);
		if (errstr)
			errx(2, "file2 end is %s: %s", errstr, line);
		if (file2start >= file2end)
			errx(2, "invalid line range in file2: %s", line);
	} else
		file2end = file2start;

	/* Appends happen _after_ stated line. */
	if (cmd == 'a') {
		if (file1start != file1end)
			errx(2, "append cannot have a file1 range: %s",
			    line);
		if (file1start == SIZE_T_MAX)
			errx(2, "file1 line range too high: %s", line);
		file1start = ++file1end;
	}
	/*
	 * I'm not sure what the deal is with the line numbers for
	 * deletes, though.
	 */
	else if (cmd == 'd') {
		if (file2start != file2end)
			errx(2, "delete cannot have a file2 range: %s",
			    line);
		if (file2start == SIZE_T_MAX)
			errx(2, "file2 line range too high: %s", line);
		file2start = ++file2end;
	}

	/* Skip unmodified lines. */
	for (; file1ln < file1start; ++file1ln, ++file2ln) {
		const char *line;

		if (!(line = xfgets(origfile)))
			errx(2, "file1 shorter than expected");

		/* If the -l flag was specified, print only left column. */
		enqueue(line, ' ', lflag ? NULL : line);
	}
	/* Process unmodified lines. */
	processq();

	switch (cmd) {
	case 'a':
		printa(difffile, file2end);
		break;

	case 'c':
		printc(origfile, file1end, difffile, file2end);
		break;

	case 'd':
		printd(origfile, difffile, file1end);
		break;

	default:
		errx(2, "invalid diff command: %c: %s", cmd, line);
	}

	return (0);
}

/*
 * Queues up a diff line.
 */
static void
enqueue(const char *left, const char div, const char *right)
{
	struct diffline *diffp;

	if (!(diffp = malloc(sizeof(struct diffline))))
		err(2, "enqueue");
	diffp->left = left;
	diffp->div = div;
	diffp->right = right;
	SIMPLEQ_INSERT_TAIL(&diffhead, diffp, diffentries);
}

/*
 * Free a diffline structure and its elements.
 */
static void
freediff(const struct diffline *diffp)
{

	if (diffp->left)
		free((void *)diffp->left);
	/*
	 * Free right string only if it is different than left.
	 * The strings are the same when the lines are identical.
	 */
	if (diffp->right && diffp->right != diffp->left)
		free((void *)diffp->right);
}

/*
 * Append second string into first.  Repeated appends to the same string
 * are cached, making this an O(n) function, where n = strlen(append).
 */
static void
astrcat(char **s, const char *append)
{
	/* Length of string in previous run. */
	static size_t offset = 0;
	size_t copied, newlen;
	/*
	 * String from previous run.  Compared to *s to see if we are
	 * dealing with the same string.  If so, we can use offset.
	 */
	const static char *oldstr = NULL;
	char *newstr;


	/*
	 * First string is NULL, so just copy append.
	 */
	if (!*s) {
		if (!(*s = strdup(append)))
			err(2, "astrcat");

		/* Keep track of string. */
		offset = strlen(*s);
		oldstr = *s;

		return;
	}

	/*
	 * *s is a string so concatenate.
	 */

	/* Did we process the same string in the last run? */
	/*
	 * If this is a different string from the one we just processed
	 * cache new string.
	 */
	if (oldstr != *s) {
		offset = strlen(*s);
		oldstr = *s;
	}

	/* Length = strlen(*s) + \n + strlen(append) + '\0'. */
	newlen = offset + 1 + strlen(append) + 1;

	/* Resize *s to fit new string. */
	newstr = realloc(*s, newlen);
	if (newstr == NULL)
		err(2, "astrcat");
	*s = newstr;

	/* Concatenate. */
	strlcpy(*s + offset, "\n", newlen - offset);
	copied = strlcat(*s + offset, append, newlen - offset);

	/* Store generated string's values. */
	offset = newlen - 1;
	oldstr = *s;
}

/*
 * Process diff set queue, printing, prompting, and saving each diff
 * line stored in queue.
 */
static void
processq(void)
{
	struct diffline *diffp;
	char div, *left, *right;

	/* Don't process empty queue. */
	if (SIMPLEQ_EMPTY(&diffhead))
		return;

	div = '\0';
	left = NULL;
	right = NULL;
	/*
	 * Go through set of diffs, concatenating each line in left or
	 * right column into two long strings, `left' and `right'.
	 */
	SIMPLEQ_FOREACH(diffp, &diffhead, diffentries) {
		/*
		 * Make sure that div is consistent throughout set.
		 * If div is set, compare to next entry's div.  They
		 * should be the same.  If div is not set, then store
		 * this as this set's div.
		 */
		if (!div)
			div = diffp->div;

		/*
		 * If the -s flag was not given or the lines are not
		 * identical then print columns.
		 */
		if (!sflag || diffp->div != ' ')
			println(diffp->left, diffp->div, diffp->right);

		/* Append new lines to diff set. */
		if (diffp->left)
			astrcat(&left, diffp->left);
		if (diffp->right)
			astrcat(&right, diffp->right);
	}

	/* Empty queue and free each diff line and its elements. */
	while (!SIMPLEQ_EMPTY(&diffhead)) {
		diffp = SIMPLEQ_FIRST(&diffhead);
		freediff(diffp);
		SIMPLEQ_REMOVE_HEAD(&diffhead, diffentries);
		free(diffp);
	}

	/* Write to outfile, prompting user if lines are different. */
	if (outfile) {
		if (div == ' ')
			fprintf(outfile, "%s\n", left);
		else
			prompt(left, right);
	}

	/* Free left and right. */
	if (left)
		free(left);
	if (right)
		free(right);
}

/*
 * Remove angle bracket in front of diff line.
 */
static void
undiff(char *s)
{
	size_t len;

	/* Remove angle bracket and space but keep the NUL. */
	len = strlen(s) - 2 + 1;
	/* Move everything two characters over. */
	memmove(s, s + 2, len);
}

/*
 * Print lines following an (a)ppend command.
 */
static void
printa(FILE *file, size_t line2)
{
	char *line;

	for (; file2ln <= line2; ++file2ln) {
		if (!(line = xfgets(file)))
			errx(2, "append ended early");
		undiff(line);
		enqueue(NULL, '>', line);
	}

	processq();
}

/*
 * Print lines following a (c)hange command, from file1ln to file1end
 * and from file2ln to file2end.
 */
static void
printc(FILE *file1, size_t file1end, FILE *file2, size_t file2end)
{
	struct fileline {
		SIMPLEQ_ENTRY(fileline) fileentries;
		const char	*line;
	};
	SIMPLEQ_HEAD(, fileline) delqhead = SIMPLEQ_HEAD_INITIALIZER(delqhead);
	char *line;

	/* Read lines to be deleted. */
	for (; file1ln <= file1end; ++file1ln) {
		struct fileline *linep;
		const char *line1, *line2;

		/* Read lines from both. */
		if (!(line1 = xfgets(file1)))
			errx(2, "error reading file1 in delete in change");
		if (!(line2 = xfgets(file2)))
			errx(2, "error reading diff in delete in change");

		/* Unused now. */
		free((void *)line2);

		/* Add to delete queue. */
		if (!(linep = malloc(sizeof(struct fileline))))
			err(2, "printc");
		linep->line = line1;
		SIMPLEQ_INSERT_TAIL(&delqhead, linep, fileentries);
	}

	/* There should be a divider here. */
	if (!(line = xfgets(file2)))
		errx(2, "error reading diff in change: expected divider");
	free(line);

#define getaddln(add) do {					\
	/* Read diff for line. */				\
	if (!((add) = xfgets(file2)))				\
		errx(2, "error reading add in change");		\
	/* Remove ``> ''. */					\
	undiff(add);						\
} while (0)
	/* Process changed lines.. */
	for (; !SIMPLEQ_EMPTY(&delqhead) && file2ln <= file2end;
	    ++file2ln) {
		struct fileline *del;
		char *add;

		/* Get add line. */
		getaddln(add);

		del = SIMPLEQ_FIRST(&delqhead);
		enqueue(del->line, '|', add);
		SIMPLEQ_REMOVE_HEAD(&delqhead, fileentries);
		/*
		 * Free fileline structure but not its elements since
		 * they are queued up.
		 */
		free(del);
	}
	processq();

	/* Process remaining lines to add. */
	for (; file2ln <= file2end; ++file2ln) {
		char *add;

		/* Get add line. */
		getaddln(add);

		enqueue(NULL, '>', add);
	}
	processq();
#undef getaddln

	/* Process remaining lines to delete. */
	while (!SIMPLEQ_EMPTY(&delqhead)) {
		struct fileline *filep;

		filep = SIMPLEQ_FIRST(&delqhead);
		enqueue(filep->line, '<', NULL);
		SIMPLEQ_REMOVE_HEAD(&delqhead, fileentries);
		free(filep);
	}
	processq();
}

/*
 * Print deleted lines from file, from file1ln to file1end.
 */
static void
printd(FILE *file1, FILE *file2, size_t file1end)
{
	const char *line1, *line2;

	/* Print out lines file1ln to line2. */
	for (; file1ln <= file1end; ++file1ln) {
		/* XXX - Why can't this handle stdin? */
		if (!(line1 = xfgets(file1)))
			errx(2, "file1 ended early in delete");
		if (!(line2 = xfgets(file2)))
			errx(2, "diff ended early in delete");
		free((void *)line2);
		enqueue(line1, '<', NULL);
	}
	processq();
}

/*
 * Interactive mode usage.
 */
static void
int_usage(void)
{
	puts("e:\tedit blank diff\n"
	    "eb:\tedit both diffs concatenated\n"
	    "el:\tedit left diff\n"
	    "er:\tedit right diff\n"
	    "l:\tchoose left diff\n"
	    "r:\tchoose right diff\n"
	    "s:\tsilent mode--don't print identical lines\n"
	    "v:\tverbose mode--print identical lines\n"
	    "q:\tquit");
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-abdilstW] [-I regexp] [-o outfile] [-w width] file1 file2\n",
	    __progname);
	exit(2);
}
