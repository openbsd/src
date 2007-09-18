/*	$OpenBSD: sendbug.c,v 1.51 2007/09/18 00:38:58 ray Exp $	*/

/*
 * Written by Ray Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"

#define _PATH_DMESG "/var/run/dmesg.boot"
#define DMESG_START "OpenBSD "

int	checkfile(const char *);
void	dmesg(FILE *);
int	editit(const char *);
void	init(void);
int	matchline(const char *, const char *, size_t);
int	prompt(void);
int	send_file(const char *, int);
int	sendmail(const char *);
void	template(FILE *);

const char *categories = "system user library documentation ports kernel "
    "alpha amd64 arm i386 m68k m88k mips ppc sgi sparc sparc64 vax";
char *version = "4.2";

struct passwd *pw;
char os[BUFSIZ], rel[BUFSIZ], mach[BUFSIZ], details[BUFSIZ];
char *fullname, *tmppath;
int Dflag, wantcleanup;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-DLPV]\n", __progname);
	exit(1);
}

void
cleanup()
{
	if (wantcleanup && tmppath && unlink(tmppath) == -1)
		warn("unlink");
}


int
main(int argc, char *argv[])
{
	int ch, c, fd, ret = 1;
	const char *tmpdir;
	struct stat sb;
	char *pr_form;
	time_t mtime;
	FILE *fp;

	while ((ch = getopt(argc, argv, "DLPV")) != -1)
		switch (ch) {
		case 'D':
			Dflag = 1;
			break;
		case 'L':
			printf("Known categories:\n");
			printf("%s\n\n", categories);
			exit(0);
		case 'P':
			init();
			template(stdout);
			exit(0);
		case 'V':
			printf("%s\n", version);
			exit(0);
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	if ((tmpdir = getenv("TMPDIR")) == NULL || tmpdir[0] == '\0')
		tmpdir = _PATH_TMP;
	if (asprintf(&tmppath, "%s%sp.XXXXXXXXXX", tmpdir,
	    tmpdir[strlen(tmpdir) - 1] == '/' ? "" : "/") == -1)
		err(1, "asprintf");
	if ((fd = mkstemp(tmppath)) == -1)
		err(1, "mkstemp");
	wantcleanup = 1;
	atexit(cleanup);
	if ((fp = fdopen(fd, "w+")) == NULL)
		err(1, "fdopen");

	init();

	pr_form = getenv("PR_FORM");
	if (pr_form) {
		char buf[BUFSIZ];
		size_t len;
		FILE *frfp;

		frfp = fopen(pr_form, "r");
		if (frfp == NULL) {
			warn("can't seem to read your template file "
			    "(`%s'), ignoring PR_FORM", pr_form);
			template(fp);
		} else {
			while (!feof(frfp)) {
				len = fread(buf, 1, sizeof buf, frfp);
				if (len == 0)
					break;
				if (fwrite(buf, 1, len, fp) != len)
					break;
			}
			fclose(frfp);
		}
	} else
		template(fp);

	if (fflush(fp) == EOF || fstat(fd, &sb) == -1 || fclose(fp) == EOF)
		err(1, "error creating template");
	mtime = sb.st_mtime;

 edit:
	if (editit(tmppath) == -1)
		err(1, "error running editor");

	if (stat(tmppath, &sb) == -1)
		err(1, "stat");
	if (mtime == sb.st_mtime)
		errx(1, "report unchanged, nothing sent");

 prompt:
	if (!checkfile(tmppath))
		fprintf(stderr, "fields are blank, must be filled in\n");
	c = prompt();
	switch (c) {
	case 'a':
	case EOF:
		wantcleanup = 0;
		errx(1, "unsent report in %s", tmppath);
	case 'e':
		goto edit;
	case 's':
		if (sendmail(tmppath) == -1)
			goto quit;
		break;
	default:
		goto prompt;
	}

	ret = 0;
quit:
	return (ret);
}

void
dmesg(FILE *fp)
{
	char buf[BUFSIZ];
	FILE *dfp;
	off_t offset = -1;

	dfp = fopen(_PATH_DMESG, "r");
	if (dfp == NULL) {
		warn("can't read dmesg");
		return;
	}

	fputs("\n"
	    "<dmesg is attached.>\n"
	    "<Feel free to delete or use the -D flag if it contains "
	    "sensitive information.>\n", fp);
	/* Find last dmesg. */
	for (;;) {
		off_t o;

		o = ftello(dfp);
		if (fgets(buf, sizeof(buf), dfp) == NULL)
			break;
		if (!strncmp(DMESG_START, buf, sizeof(DMESG_START) - 1))
			offset = o;
	}
	if (offset != -1) {
		size_t len;

		clearerr(dfp);
		fseeko(dfp, offset, SEEK_SET);
		while (offset != -1 && !feof(dfp)) {
			len = fread(buf, 1, sizeof buf, dfp);
			if (len == 0)
				break;
			if (fwrite(buf, 1, len, fp) != len)
				break;
		}
	}
	fclose(dfp);
}

/*
 * Execute an editor on the specified pathname, which is interpreted
 * from the shell.  This means flags may be included.
 *
 * Returns -1 on error, or the exit value on success.
 */
int
editit(const char *pathname)
{
	char *argp[] = {"sh", "-c", NULL, NULL}, *ed, *p;
	sig_t sighup, sigint, sigquit;
	pid_t pid;
	int saved_errno, st;

	ed = getenv("VISUAL");
	if (ed == NULL || ed[0] == '\0')
		ed = getenv("EDITOR");
	if (ed == NULL || ed[0] == '\0')
		ed = _PATH_VI;
	if (asprintf(&p, "%s %s", ed, pathname) == -1)
		return (-1);
	argp[2] = p;

	sighup = signal(SIGHUP, SIG_IGN);
	sigint = signal(SIGINT, SIG_IGN);
	sigquit = signal(SIGQUIT, SIG_IGN);
	if ((pid = fork()) == -1)
		goto fail;
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			goto fail;
	free(p);
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	if (!WIFEXITED(st)) {
		errno = EINTR;
		return (-1);
	}
	return (WEXITSTATUS(st));

 fail:
	saved_errno = errno;
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	free(p);
	errno = saved_errno;
	return (-1);
}

int
prompt(void)
{
	int c, ret;

	fpurge(stdin);
	fprintf(stderr, "a)bort, e)dit, or s)end: ");
	fflush(stderr);
	ret = getchar();
	if (ret == EOF || ret == '\n')
		return (ret);
	do {
		c = getchar();
	} while (c != EOF && c != '\n');
	return (ret);
}

int
sendmail(const char *pathname)
{
	int filedes[2];

	if (pipe(filedes) == -1) {
		warn("pipe: unsent report in %s", pathname);
		return (-1);
	}
	switch (fork()) {
	case -1:
		warn("fork error: unsent report in %s",
		    pathname);
		return (-1);
	case 0:
		close(filedes[1]);
		if (dup2(filedes[0], STDIN_FILENO) == -1) {
			warn("dup2 error: unsent report in %s",
			    pathname);
			return (-1);
		}
		close(filedes[0]);
		execl("/usr/sbin/sendmail", "sendmail",
		    "-oi", "-t", (void *)NULL);
		warn("sendmail error: unsent report in %s",
		    pathname);
		return (-1);
	default:
		close(filedes[0]);
		/* Pipe into sendmail. */
		if (send_file(pathname, filedes[1]) == -1) {
			warn("send_file error: unsent report in %s",
			    pathname);
			return (-1);
		}
		close(filedes[1]);
		wait(NULL);
		break;
	}
	return (0);
}

void
init(void)
{
	size_t amp, len, gecoslen, namelen;
	int sysname[2];
	char ch, *cp;

	if ((pw = getpwuid(getuid())) == NULL)
		err(1, "getpwuid");
	namelen = strlen(pw->pw_name);

	/* Count number of '&'. */
	for (amp = 0, cp = pw->pw_gecos; *cp && *cp != ','; ++cp)
		if (*cp == '&')
			++amp;

	/* Truncate gecos to full name. */
	gecoslen = cp - pw->pw_gecos;
	pw->pw_gecos[gecoslen] = '\0';

	/* Expanded str = orig str - '&' chars + concatenated logins. */
	len = gecoslen - amp + (amp * namelen) + 1;
	if ((fullname = malloc(len)) == NULL)
		err(1, "malloc");

	/* Upper case first char of login. */
	ch = pw->pw_name[0];
	pw->pw_name[0] = toupper((unsigned char)pw->pw_name[0]);

	cp = pw->pw_gecos;
	fullname[0] = '\0';
	while (cp != NULL) {
		char *token;

		token = strsep(&cp, "&");
		if (token != pw->pw_gecos &&
		    strlcat(fullname, pw->pw_name, len) >= len)
			errx(1, "truncated string");
		if (strlcat(fullname, token, len) >= len)
			errx(1, "truncated string");
	}
	/* Restore case of first char of login. */
	pw->pw_name[0] = ch;

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_OSTYPE;
	len = sizeof(os) - 1;
	if (sysctl(sysname, 2, &os, &len, NULL, 0) == -1)
		err(1, "sysctl");

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_OSRELEASE;
	len = sizeof(rel) - 1;
	if (sysctl(sysname, 2, &rel, &len, NULL, 0) == -1)
		err(1, "sysctl");

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_VERSION;
	len = sizeof(details) - 1;
	if (sysctl(sysname, 2, &details, &len, NULL, 0) == -1)
		err(1, "sysctl");

	cp = strchr(details, '\n');
	if (cp) {
		cp++;
		if (*cp)
			*cp++ = '\t';
		if (*cp)
			*cp++ = '\t';
		if (*cp)
			*cp++ = '\t';
	}

	sysname[0] = CTL_HW;
	sysname[1] = HW_MACHINE;
	len = sizeof(mach) - 1;
	if (sysctl(sysname, 2, &mach, &len, NULL, 0) == -1)
		err(1, "sysctl");
}

int
send_file(const char *file, int dst)
{
	size_t len;
	char *buf;
	FILE *fp;
	int blank = 0, dmesg_line = 0;

	if ((fp = fopen(file, "r")) == NULL)
		return (-1);
	while ((buf = fgetln(fp, &len))) {
		/* Skip lines starting with "SENDBUG". */
		if (len >= sizeof("SENDBUG") - 1 &&
		    memcmp(buf, "SENDBUG", sizeof("SENDBUG") - 1) == 0)
			continue;
		/* Are we done with the headers? */
		if (!blank && len == 1 && buf[0] == '\n')
			blank = 1;
		/* Have we reached the dmesg? */
		if (blank && !dmesg_line &&
		    len >= sizeof(DMESG_START) - 1 &&
		    memcmp(buf, DMESG_START, sizeof(DMESG_START) - 1) == 0)
			dmesg_line = 1;
		/* Skip comments between headers and dmesg. */
		while (len) {
			char *sp = NULL, *ep = NULL;
			size_t copylen;

			if (blank && !dmesg_line &&
			    (sp = memchr(buf, '<', len)) != NULL)
				ep = memchr(sp, '>', len - (sp - buf + 1));
			/* Length of string before comment. */
			if (ep)
				copylen = sp - buf;
			else
				copylen = len;
			if (atomicio(vwrite, dst, buf, copylen) != copylen) {
				int saved_errno = errno;

				fclose(fp);
				errno = saved_errno;
				return (-1);
			}
			if (!ep)
				break;
			/* Skip comment. */
			len -= ep - buf + 1;
			buf = ep + 1;
		}
	}
	fclose(fp);
	return (0);
}

/*
 * Does line start with `s' and end with non-comment and non-whitespace?
 * Note: Does not treat `line' as a C string.
 */
int
matchline(const char *s, const char *line, size_t linelen)
{
	size_t slen;
	int comment;

	slen = strlen(s);
	/* Is line shorter than string? */
	if (linelen <= slen)
		return (0);
	/* Does line start with string? */
	if (memcmp(line, s, slen) != 0)
		return (0);
	/* Does line contain anything but comments and whitespace? */
	line += slen;
	linelen -= slen;
	comment = 0;
	while (linelen) {
		if (comment) {
			if (*line == '>')
				comment = 0;
		} else if (*line == '<')
			comment = 1;
		else if (!isspace((unsigned char)*line))
			return (1);
		++line;
		--linelen;
	}
	return (0);
}

/*
 * Are all required fields filled out?
 */
int
checkfile(const char *pathname)
{
	FILE *fp;
	size_t len;
	int category, class, priority, release, severity, synopsis;
	char *buf;

	if ((fp = fopen(pathname, "r")) == NULL) {
		warn("%s", pathname);
		return (0);
	}
	category = class = priority = release = severity = synopsis = 0;
	while ((buf = fgetln(fp, &len))) {
		if (matchline(">Category:", buf, len))
			category = 1;
		else if (matchline(">Class:", buf, len))
			class = 1;
		else if (matchline(">Priority:", buf, len))
			priority = 1;
		else if (matchline(">Release:", buf, len))
			release = 1;
		else if (matchline(">Severity:", buf, len))
			severity = 1;
		else if (matchline(">Synopsis:", buf, len))
			synopsis = 1;
	}
	fclose(fp);
	return (category && class && priority && release && severity &&
	    synopsis);
}

void
template(FILE *fp)
{
	fprintf(fp, "SENDBUG: -*- sendbug -*-\n");
	fprintf(fp, "SENDBUG: Lines starting with `SENDBUG' will"
	    " be removed automatically, as\n");
	fprintf(fp, "SENDBUG: will all comments (text enclosed in `<' and `>').\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG: Choose from the following categories:\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG: %s\n", categories);
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "To: %s\n", "gnats@openbsd.org");
	fprintf(fp, "Subject: \n");
	fprintf(fp, "From: %s\n", pw->pw_name);
	fprintf(fp, "Cc: %s\n", pw->pw_name);
	fprintf(fp, "Reply-To: %s\n", pw->pw_name);
	fprintf(fp, "X-sendbug-version: %s\n", version);
	fprintf(fp, "\n");
	fprintf(fp, "\n");
	fprintf(fp, ">Submitter-Id:\tnet\n");
	fprintf(fp, ">Originator:\t%s\n", fullname);
	fprintf(fp, ">Organization:\n");
	fprintf(fp, "net\n");
	fprintf(fp, ">Synopsis:\t<synopsis of the problem (one line)>\n");
	fprintf(fp, ">Severity:\t"
	    "<[ non-critical | serious | critical ] (one line)>\n");
	fprintf(fp, ">Priority:\t<[ low | medium | high ] (one line)>\n");
	fprintf(fp, ">Category:\t<PR category (one line)>\n");
	fprintf(fp, ">Class:\t\t"
	    "<[ sw-bug | doc-bug | change-request | support ] (one line)>\n");
	fprintf(fp, ">Release:\t<release number or tag (one line)>\n");
	fprintf(fp, ">Environment:\n");
	fprintf(fp, "\t<machine, os, target, libraries (multiple lines)>\n");
	fprintf(fp, "\tSystem      : %s %s\n", os, rel);
	fprintf(fp, "\tDetails     : %s\n", details);
	fprintf(fp, "\tArchitecture: %s.%s\n", os, mach);
	fprintf(fp, "\tMachine     : %s\n", mach);
	fprintf(fp, ">Description:\n");
	fprintf(fp, "\t<precise description of the problem (multiple lines)>\n");
	fprintf(fp, ">How-To-Repeat:\n");
	fprintf(fp, "\t<code/input/activities to reproduce the problem"
	    " (multiple lines)>\n");
	fprintf(fp, ">Fix:\n");
	fprintf(fp, "\t<how to correct or work around the problem,"
	    " if known (multiple lines)>\n");

	if (!Dflag)
		dmesg(fp);
}
