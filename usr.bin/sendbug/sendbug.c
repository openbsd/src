/*	$OpenBSD: sendbug.c,v 1.9 2007/03/23 03:30:52 ray Exp $	*/

/*
 * Written by Ray Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"

int init(void);
int prompt(void);
int send_file(const char *, int dst);
int sendmail(const char *);
void template(FILE *);

struct passwd *pw;
const char *categories = "system user library documentation ports kernel "
    "alpha amd64 arm i386 m68k m88k mips ppc sgi sparc sparc64 vax";
char os[BUFSIZ], rel[BUFSIZ], mach[BUFSIZ];
char *fullname;
char *version = "4.2";
void
usage(void)
{
	fprintf(stderr, "usage: sendbug [-LPV]\n");
}

int
main(int argc, char *argv[])
{
	const char *editor, *tmpdir;
	char *argp[] = {"sh", "-c", NULL, NULL}, *tmppath = NULL;
	int ch, c, fd, ret = 1;
	struct stat sb;
	time_t mtime;
	FILE *fp;

	while ((ch = getopt(argc, argv, "LPV")) != -1)
		switch (ch) {
		case 'L':
			printf("Known categories:\n");
			printf("%s\n\n", categories);
			exit(0);
		case 'P':
			if (init() == -1)
				exit(1);
			template(stdout);
			exit(0);
		case 'V':
			printf("%s\n", version);
			exit(0);
		default:
			usage();
			exit(1);
		}

	if (argc > 1) {
		usage();
		exit(1);
	}

	if ((tmpdir = getenv("TMPDIR")) == NULL || tmpdir[0] == '\0')
		tmpdir = _PATH_TMP;
	if (asprintf(&tmppath, "%s%sp.XXXXXXXXXX", tmpdir,
	    tmpdir[strlen(tmpdir) - 1] == '/' ? "" : "/") == -1) {
		warn("asprintf");
		goto quit;
	}
	if ((fd = mkstemp(tmppath)) == -1)
		err(1, "mkstemp");
	if ((fp = fdopen(fd, "w+")) == NULL) {
		warn("fdopen");
		goto cleanup;
	}

	if (init() == -1)
		goto cleanup;

	template(fp);

	if (fflush(fp) == EOF || fstat(fd, &sb) == -1 || fclose(fp) == EOF) {
		warn("error creating template");
		goto cleanup;
	}
	mtime = sb.st_mtime;

 edit:
	if ((editor = getenv("EDITOR")) == NULL)
		editor = "vi";
	switch (fork()) {
	case -1:
		warn("fork");
		goto cleanup;
	case 0:
		if (asprintf(&argp[2], "%s %s", editor, tmppath) == -1)
			err(1, "asprintf");
		execv(_PATH_BSHELL, argp);
		err(1, "execv");
	default:
		wait(NULL);
		break;
	}

	if (stat(tmppath, &sb) == -1) {
		warn("stat");
		goto cleanup;
	}
	if (mtime == sb.st_mtime) {
		warnx("report unchanged, nothing sent");
		goto cleanup;
	}

 prompt:
	c = prompt();
	switch (c) {
	case 'a':
	case EOF:
		warnx("unsent report in %s", tmppath);
		goto quit;
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

 cleanup:
	if (tmppath && unlink(tmppath) == -1)
		warn("unlink");

 quit:
	return (ret);
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
sendmail(const char *tmppath)
{
	int filedes[2];

	if (pipe(filedes) == -1) {
		warn("pipe: unsent report in %s", tmppath);
		return (-1);
	}
	switch (fork()) {
	case -1:
		warn("fork error: unsent report in %s",
		    tmppath);
		return (-1);
	case 0:
		close(filedes[1]);
		if (dup2(filedes[0], STDIN_FILENO) == -1) {
			warn("dup2 error: unsent report in %s",
			    tmppath);
			return (-1);
		}
		close(filedes[0]);
		execl("/usr/sbin/sendmail", "sendmail",
		    "-oi", "-t", (void *)NULL);
		warn("sendmail error: unsent report in %s",
		    tmppath);
		return (-1);
	default:
		close(filedes[0]);
		/* Pipe into sendmail. */
		if (send_file(tmppath, filedes[1]) == -1) {
			warn("send_file error: unsent report in %s",
			    tmppath);
			return (-1);
		}
		close(filedes[1]);
		wait(NULL);
		break;
	}
	return (0);
}

int
init(void)
{
	size_t len;
	int sysname[2];

	if ((pw = getpwuid(getuid())) == NULL) {
		warn("getpwuid");
		return (-1);
	}

	/* Get full name. */
	len = strcspn(pw->pw_gecos, ",");
	if ((fullname = malloc(len + 1)) == NULL) {
		warn("malloc");
		return (-1);
	}
	memcpy(fullname, pw->pw_gecos, len);
	fullname[len] = '\0';

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_OSTYPE;
	len = sizeof(os) - 1;
	if (sysctl(sysname, 2, &os, &len, NULL, 0) == -1) {
		warn("sysctl");
		return (-1);
	}

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_OSRELEASE;
	len = sizeof(rel) - 1;
	if (sysctl(sysname, 2, &rel, &len, NULL, 0) == -1) {
		warn("sysctl");
		return (-1);
	}

	sysname[0] = CTL_HW;
	sysname[1] = HW_MACHINE;
	len = sizeof(mach) - 1;
	if (sysctl(sysname, 2, &mach, &len, NULL, 0) == -1) {
		warn("sysctl");
		return (-1);
	}

	return (0);
}

int
send_file(const char *file, int dst)
{
	int blank = 0;
	size_t len;
	char *buf;
	FILE *fp;

	if ((fp = fopen(file, "r")) == NULL)
		return (-1);
	while ((buf = fgetln(fp, &len))) {
		/* Skip lines starting with "SENDBUG". */
		if (len >= sizeof("SENDBUG") - 1 &&
		    memcmp(buf, "SENDBUG", sizeof("SENDBUG") - 1) == 0)
			continue;
		if (len == 1 && buf[0] == '\n')
			blank = 1;
		/* Skip comments, but only if we encountered a blank line. */
		while (len) {
			char *sp, *ep = NULL;
			size_t copylen;

			if (blank && (sp = memchr(buf, '<', len)) != NULL)
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

void
template(FILE *fp)
{
	fprintf(fp, "SENDBUG: -*- sendbug -*-\n");
	fprintf(fp, "SENDBUG: Lines starting with `SENDBUG' will be removed automatically, as\n");
	fprintf(fp, "SENDBUG: will all comments (text enclosed in `<' and `>').              \n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG: Choose from the following categories:\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG: %s\n", categories);
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "To: %s\n", "gnats@openbsd.org");
	fprintf(fp, "Subject: \n");
	fprintf(fp, "From: %s\n", pw->pw_name);
	fprintf(fp, "Cc: \n");
	fprintf(fp, "Reply-To: %s\n", pw->pw_name);
	fprintf(fp, "X-sendbug-version: %s\n", version);
	fprintf(fp, "\n");
	fprintf(fp, "\n");
	fprintf(fp, ">Submitter-Id:\tnet\n");
	fprintf(fp, ">Originator:\t%s\n", fullname);
	fprintf(fp, ">Organization:\n");
	fprintf(fp, "net\n");
	fprintf(fp, ">Synopsis:\t<synopsis of the problem (one line)>\n");
	fprintf(fp, ">Severity:\t<[ non-critical | serious | critical ] (one line)>\n");
	fprintf(fp, ">Priority:\t<[ low | medium | high ] (one line)>\n");
	fprintf(fp, ">Category:\t<PR category (one line)>\n");
	fprintf(fp, ">Class:\t\t<[ sw-bug | doc-bug | change-request | support ] (one line)>\n");
	fprintf(fp, ">Release:\t<release number or tag (one line)>\n");
	fprintf(fp, ">Environment:\n");
	fprintf(fp, "\t<machine, os, target, libraries (multiple lines)>\n");
	fprintf(fp, "\tSystem      : %s %s\n", os, rel);
	fprintf(fp, "\tArchitecture: %s.%s\n", os, mach);
	fprintf(fp, "\tMachine     : %s\n", mach);
	fprintf(fp, ">Description:\n");
	fprintf(fp, "\t<precise description of the problem (multiple lines)>\n");
	fprintf(fp, ">How-To-Repeat:\n");
	fprintf(fp, "\t<code/input/activities to reproduce the problem (multiple lines)>\n");
	fprintf(fp, ">Fix:\n");
	fprintf(fp, "\t<how to correct or work around the problem, if known (multiple lines)>\n");
}
