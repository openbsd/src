/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines to mess around with filenames (and files).
 * Much of this is very OS dependent.
 *
 * Modified for illumos/POSIX -- it uses native glob(3C) rather than
 * popen to a shell to perform the expansion.
 */

#include <sys/stat.h>

#include <glob.h>
#include <stdarg.h>

#include "less.h"

extern int force_open;
extern int secure;
extern int use_lessopen;
extern int ctldisp;
extern int utf_mode;
extern IFILE curr_ifile;
extern IFILE old_ifile;
extern char openquote;
extern char closequote;

/*
 * Remove quotes around a filename.
 */
char *
shell_unquote(char *str)
{
	char *name;
	char *p;

	name = p = ecalloc(strlen(str)+1, sizeof (char));
	if (*str == openquote) {
		str++;
		while (*str != '\0') {
			if (*str == closequote) {
				if (str[1] != closequote)
					break;
				str++;
			}
			*p++ = *str++;
		}
	} else {
		char *esc = get_meta_escape();
		int esclen = strlen(esc);
		while (*str != '\0') {
			if (esclen > 0 && strncmp(str, esc, esclen) == 0)
				str += esclen;
			*p++ = *str++;
		}
	}
	*p = '\0';
	return (name);
}

/*
 * Get the shell's escape character.
 */
char *
get_meta_escape(void)
{
	char *s;

	s = lgetenv("LESSMETAESCAPE");
	if (s == NULL)
		s = "\\";
	return (s);
}

/*
 * Get the characters which the shell considers to be "metacharacters".
 */
static char *
metachars(void)
{
	static char *mchars = NULL;

	if (mchars == NULL) {
		mchars = lgetenv("LESSMETACHARS");
		if (mchars == NULL)
			mchars = DEF_METACHARS;
	}
	return (mchars);
}

/*
 * Is this a shell metacharacter?
 */
static int
metachar(char c)
{
	return (strchr(metachars(), c) != NULL);
}

/*
 * Insert a backslash before each metacharacter in a string.
 */
char *
shell_quote(const char *s)
{
	const char *p;
	char *r;
	char *newstr;
	int len;
	char *esc = get_meta_escape();
	int esclen = strlen(esc);
	int use_quotes = 0;
	int have_quotes = 0;

	/*
	 * Determine how big a string we need to allocate.
	 */
	len = 1; /* Trailing null byte */
	for (p = s;  *p != '\0';  p++) {
		len++;
		if (*p == openquote || *p == closequote)
			have_quotes = 1;
		if (metachar(*p)) {
			if (esclen == 0) {
				/*
				 * We've got a metachar, but this shell
				 * doesn't support escape chars.  Use quotes.
				 */
				use_quotes = 1;
			} else {
				/*
				 * Allow space for the escape char.
				 */
				len += esclen;
			}
		}
	}
	/*
	 * Allocate and construct the new string.
	 */
	if (use_quotes) {
		/* We can't quote a string that contains quotes. */
		if (have_quotes)
			return (NULL);
		newstr  = easprintf("%c%s%c", openquote, s, closequote);
	} else {
		newstr = r = ecalloc(len, sizeof (char));
		while (*s != '\0') {
			if (metachar(*s)) {
				/*
				 * Add the escape char.
				 */
				(void) strlcpy(r, esc, newstr + len - p);
				r += esclen;
			}
			*r++ = *s++;
		}
		*r = '\0';
	}
	return (newstr);
}

/*
 * Return a pathname that points to a specified file in a specified directory.
 * Return NULL if the file does not exist in the directory.
 */
static char *
dirfile(const char *dirname, const char *filename)
{
	char *pathname;
	char *qpathname;
	int f;

	if (dirname == NULL || *dirname == '\0')
		return (NULL);
	/*
	 * Construct the full pathname.
	 */
	pathname = easprintf("%s/%s", dirname, filename);
	/*
	 * Make sure the file exists.
	 */
	qpathname = shell_unquote(pathname);
	f = open(qpathname, O_RDONLY);
	if (f < 0) {
		free(pathname);
		pathname = NULL;
	} else {
		(void) close(f);
	}
	free(qpathname);
	return (pathname);
}

/*
 * Return the full pathname of the given file in the "home directory".
 */
char *
homefile(char *filename)
{
	return (dirfile(lgetenv("HOME"), filename));
}

/*
 * Expand a string, substituting any "%" with the current filename,
 * and any "#" with the previous filename.
 * But a string of N "%"s is just replaced with N-1 "%"s.
 * Likewise for a string of N "#"s.
 * {{ This is a lot of work just to support % and #. }}
 */
char *
fexpand(char *s)
{
	char *fr, *to;
	int n;
	char *e;
	IFILE ifile;

#define	fchar_ifile(c) \
	((c) == '%' ? curr_ifile : (c) == '#' ? old_ifile : NULL)

	/*
	 * Make one pass to see how big a buffer we
	 * need to allocate for the expanded string.
	 */
	n = 0;
	for (fr = s;  *fr != '\0';  fr++) {
		switch (*fr) {
		case '%':
		case '#':
			if (fr > s && fr[-1] == *fr) {
				/*
				 * Second (or later) char in a string
				 * of identical chars.  Treat as normal.
				 */
				n++;
			} else if (fr[1] != *fr) {
				/*
				 * Single char (not repeated).  Treat specially.
				 */
				ifile = fchar_ifile(*fr);
				if (ifile == NULL)
					n++;
				else
					n += strlen(get_filename(ifile));
			}
			/*
			 * Else it is the first char in a string of
			 * identical chars.  Just discard it.
			 */
			break;
		default:
			n++;
			break;
		}
	}

	e = ecalloc(n+1, sizeof (char));

	/*
	 * Now copy the string, expanding any "%" or "#".
	 */
	to = e;
	for (fr = s;  *fr != '\0';  fr++) {
		switch (*fr) {
		case '%':
		case '#':
			if (fr > s && fr[-1] == *fr) {
				*to++ = *fr;
			} else if (fr[1] != *fr) {
				ifile = fchar_ifile(*fr);
				if (ifile == NULL) {
					*to++ = *fr;
				} else {
					(void) strlcpy(to, get_filename(ifile),
					    e + n + 1 - to);
					to += strlen(to);
				}
			}
			break;
		default:
			*to++ = *fr;
			break;
		}
	}
	*to = '\0';
	return (e);
}

/*
 * Return a blank-separated list of filenames which "complete"
 * the given string.
 */
char *
fcomplete(char *s)
{
	char *fpat;
	char *qs;

	if (secure)
		return (NULL);
	/*
	 * Complete the filename "s" by globbing "s*".
	 */
	fpat =  easprintf("%s*", s);

	qs = lglob(fpat);
	s = shell_unquote(qs);
	if (strcmp(s, fpat) == 0) {
		/*
		 * The filename didn't expand.
		 */
		free(qs);
		qs = NULL;
	}
	free(s);
	free(fpat);
	return (qs);
}

/*
 * Try to determine if a file is "binary".
 * This is just a guess, and we need not try too hard to make it accurate.
 */
int
bin_file(int f)
{
	int n;
	int bin_count = 0;
	char data[256];
	char *p;
	char *pend;

	if (!seekable(f))
		return (0);
	if (lseek(f, (off_t)0, SEEK_SET) == (off_t)-1)
		return (0);
	n = read(f, data, sizeof (data));
	pend = &data[n];
	for (p = data; p < pend; ) {
		LWCHAR c = step_char(&p, +1, pend);
		if (ctldisp == OPT_ONPLUS && IS_CSI_START(c)) {
			do {
				c = step_char(&p, +1, pend);
			} while (p < pend && is_ansi_middle(c));
		} else if (binary_char(c))
			bin_count++;
	}
	/*
	 * Call it a binary file if there are more than 5 binary characters
	 * in the first 256 bytes of the file.
	 */
	return (bin_count > 5);
}

/*
 * Try to determine the size of a file by seeking to the end.
 */
static off_t
seek_filesize(int f)
{
	off_t spos;

	spos = lseek(f, (off_t)0, SEEK_END);
	if (spos == (off_t)-1)
		return (-1);
	return (spos);
}

/*
 * Read a string from a file.
 * Return a pointer to the string in memory.
 */
static char *
readfd(FILE *fd)
{
	int len;
	int ch;
	char *buf;
	char *p;

	/*
	 * Make a guess about how many chars in the string
	 * and allocate a buffer to hold it.
	 */
	len = 100;
	buf = ecalloc(len, sizeof (char));
	for (p = buf; ; p++) {
		if ((ch = getc(fd)) == '\n' || ch == EOF)
			break;
		if (p >= buf + len-1) {
			/*
			 * The string is too big to fit in the buffer we have.
			 * Allocate a new buffer, twice as big.
			 */
			len *= 2;
			*p = '\0';
			p = ecalloc(len, sizeof (char));
			strlcpy(p, buf, len);
			free(buf);
			buf = p;
			p = buf + strlen(buf);
		}
		*p = (char)ch;
	}
	*p = '\0';
	return (buf);
}

/*
 * Execute a shell command.
 * Return a pointer to a pipe connected to the shell command's standard output.
 */
static FILE *
shellcmd(char *cmd)
{
	FILE *fd;

	char *shell;

	shell = lgetenv("SHELL");
	if (shell != NULL && *shell != '\0') {
		char *scmd;
		char *esccmd;

		/*
		 * Read the output of <$SHELL -c cmd>.
		 * Escape any metacharacters in the command.
		 */
		esccmd = shell_quote(cmd);
		if (esccmd == NULL) {
			fd = popen(cmd, "r");
		} else {
			scmd = easprintf("%s -c %s", shell, esccmd);
			free(esccmd);
			fd = popen(scmd, "r");
			free(scmd);
		}
	} else {
		fd = popen(cmd, "r");
	}
	/*
	 * Redirection in `popen' might have messed with the
	 * standard devices.  Restore binary input mode.
	 */
	return (fd);
}

/*
 * Expand a filename, doing any system-specific metacharacter substitutions.
 */
char *
lglob(char *filename)
{
	char *gfilename;
	char *ofilename;
	glob_t list;
	int i;
	int length;
	char *p;
	char *qfilename;

	ofilename = fexpand(filename);
	if (secure)
		return (ofilename);
	filename = shell_unquote(ofilename);

	/*
	 * The globbing function returns a list of names.
	 */

#ifndef	GLOB_TILDE
#define	GLOB_TILDE	0
#endif
#ifndef	GLOB_LIMIT
#define	GLOB_LIMIT	0
#endif
	if (glob(filename, GLOB_TILDE | GLOB_LIMIT, NULL, &list) != 0) {
		free(filename);
		return (ofilename);
	}
	length = 1; /* Room for trailing null byte */
	for (i = 0; i < list.gl_pathc; i++) {
		p = list.gl_pathv[i];
		qfilename = shell_quote(p);
		if (qfilename != NULL) {
			length += strlen(qfilename) + 1;
			free(qfilename);
		}
	}
	gfilename = ecalloc(length, sizeof (char));
	for (i = 0; i < list.gl_pathc; i++) {
		p = list.gl_pathv[i];
		qfilename = shell_quote(p);
		if (qfilename != NULL) {
			if (i != 0) {
				(void) strlcat(gfilename, " ", length);
			}
			(void) strlcat(gfilename, qfilename, length);
			free(qfilename);
		}
	}
	globfree(&list);
	free(filename);
	free(ofilename);
	return (gfilename);
}

/*
 * Expand LESSOPEN or LESSCLOSE.  Returns a newly allocated string
 * on success, NULL otherwise.
 */
static char *
expand_pct_s(const char *fmt, ...)
{
	int		n;
	int		len;
	char		*r, *d;
	const char	*f[3];		/* max expansions + 1 for NULL */
	va_list		ap;

	va_start(ap, fmt);
	for (n = 0; n < ((sizeof (f)/sizeof (f[0])) - 1); n++) {
		f[n] = (const char *)va_arg(ap, const char *);
		if (f[n] == NULL) {
			break;
		}
	}
	va_end(ap);
	f[n] = NULL;	/* terminate list */

	len = strlen(fmt) + 1;
	for (n = 0; f[n] != NULL; n++) {
		len += strlen(f[n]);	/* technically could -2 for "%s" */
	}
	r = ecalloc(len, sizeof (char));

	for (n = 0, d = r; *fmt != 0; ) {
		if (*fmt != '%') {
			*d++ = *fmt++;
			continue;
		}
		fmt++;
		/* Permit embedded "%%" */
		switch (*fmt) {
		case '%':
			*d++ = '%';
			fmt++;
			break;
		case 's':
			if (f[n] == NULL) {
				va_end(ap);
				free(r);
				return (NULL);
			}
			(void) strlcpy(d, f[n++], r + len - d);
			fmt++;
			d += strlen(d);
			break;
		default:
			va_end(ap);
			free(r);
			return (NULL);
		}
	}
	*d = '\0';
	return (r);
}

/*
 * See if we should open a "replacement file"
 * instead of the file we're about to open.
 */
char *
open_altfile(char *filename, int *pf, void **pfd)
{
	char *lessopen;
	char *cmd;
	FILE *fd;
	int returnfd = 0;

	if (!use_lessopen || secure)
		return (NULL);
	ch_ungetchar(-1);
	if ((lessopen = lgetenv("LESSOPEN")) == NULL)
		return (NULL);
	while (*lessopen == '|') {
		/*
		 * If LESSOPEN starts with a |, it indicates
		 * a "pipe preprocessor".
		 */
		lessopen++;
		returnfd++;
	}
	if (*lessopen == '-') {
		/*
		 * Lessopen preprocessor will accept "-" as a filename.
		 */
		lessopen++;
	} else {
		if (strcmp(filename, "-") == 0)
			return (NULL);
	}

	if ((cmd = expand_pct_s(lessopen, filename, NULL)) == NULL) {
		error("Invalid LESSOPEN variable", NULL);
		return (NULL);
	}
	fd = shellcmd(cmd);
	free(cmd);
	if (fd == NULL) {
		/*
		 * Cannot create the pipe.
		 */
		return (NULL);
	}
	if (returnfd) {
		int f;
		char c;

		/*
		 * Read one char to see if the pipe will produce any data.
		 * If it does, push the char back on the pipe.
		 */
		f = fileno(fd);
		if (read(f, &c, 1) != 1) {
			/*
			 * Pipe is empty.
			 * If more than 1 pipe char was specified,
			 * the exit status tells whether the file itself
			 * is empty, or if there is no alt file.
			 * If only one pipe char, just assume no alt file.
			 */
			int status = pclose(fd);
			if (returnfd > 1 && status == 0) {
				*pfd = NULL;
				*pf = -1;
				return (estrdup(FAKE_EMPTYFILE));
			}
			return (NULL);
		}
		ch_ungetchar(c);
		*pfd = (void *) fd;
		*pf = f;
		return (estrdup("-"));
	}
	cmd = readfd(fd);
	pclose(fd);
	if (*cmd == '\0')
		/*
		 * Pipe is empty.  This means there is no alt file.
		 */
		return (NULL);
	return (cmd);
}

/*
 * Close a replacement file.
 */
void
close_altfile(char *altfilename, char *filename, void *pipefd)
{
	char *lessclose;
	FILE *fd;
	char *cmd;

	if (secure)
		return;
	if (pipefd != NULL) {
		pclose((FILE *)pipefd);
	}
	if ((lessclose = lgetenv("LESSCLOSE")) == NULL)
		return;
	cmd = expand_pct_s(lessclose, filename, altfilename, NULL);
	if (cmd == NULL) {
		error("Invalid LESSCLOSE variable", NULL);
		return;
	}
	fd = shellcmd(cmd);
	free(cmd);
	if (fd != NULL)
		(void) pclose(fd);
}

/*
 * Is the specified file a directory?
 */
int
is_dir(char *filename)
{
	int isdir = 0;
	int r;
	struct stat statbuf;

	filename = shell_unquote(filename);

	r = stat(filename, &statbuf);
	isdir = (r >= 0 && S_ISDIR(statbuf.st_mode));
	free(filename);
	return (isdir);
}

/*
 * Returns NULL if the file can be opened and
 * is an ordinary file, otherwise an error message
 * (if it cannot be opened or is a directory, etc.)
 */
char *
bad_file(char *filename)
{
	char *m = NULL;

	filename = shell_unquote(filename);
	if (!force_open && is_dir(filename)) {
		m = easprintf("%s is a directory", filename);
	} else {
		int r;
		struct stat statbuf;

		r = stat(filename, &statbuf);
		if (r < 0) {
			m = errno_message(filename);
		} else if (force_open) {
			m = NULL;
		} else if (!S_ISREG(statbuf.st_mode)) {
			m = easprintf("%s is not a regular file (use -f to "
			    "see it)", filename);
		}
	}
	free(filename);
	return (m);
}

/*
 * Return the size of a file, as cheaply as possible.
 * In Unix, we can stat the file.
 */
off_t
filesize(int f)
{
	struct stat statbuf;

	if (fstat(f, &statbuf) >= 0)
		return (statbuf.st_size);
	return (seek_filesize(f));
}

/*
 * Return last component of a pathname.
 */
char *
last_component(char *name)
{
	char *slash;

	for (slash = name + strlen(name);  slash > name; ) {
		--slash;
		if (*slash == '/')
			return (slash + 1);
	}
	return (name);
}
