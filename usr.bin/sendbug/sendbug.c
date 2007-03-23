/*	$OpenBSD: sendbug.c,v 1.14 2007/03/23 15:46:40 deraadt Exp $	*/

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

#include <ctype.h>
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

void init(void);
int prompt(void);
int send_file(const char *, int dst);
int sendmail(const char *);
void template(FILE *);

const char *categories = "system user library documentation ports kernel "
    "alpha amd64 arm i386 m68k m88k mips ppc sgi sparc sparc64 vax";
char *version = "4.2";

struct passwd *pw;
char os[BUFSIZ], rel[BUFSIZ], mach[BUFSIZ], details[BUFSIZ];
char *fullname;
char *tmppath;
int wantcleanup;

__dead void
usage(void)
{
	fprintf(stderr, "usage: sendbug [-LPV]\n");
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
	const char *editor, *tmpdir;
	char *argp[] = {"sh", "-c", NULL, NULL}, *pr_form;
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
			init();
			template(stdout);
			exit(0);
		case 'V':
			printf("%s\n", version);
			exit(0);
		default:
			usage();
		}

	if (argc > 1) {
		usage();
	}

	if ((tmpdir = getenv("TMPDIR")) == NULL || tmpdir[0] == '\0')
		tmpdir = _PATH_TMP;
	if (asprintf(&tmppath, "%s%sp.XXXXXXXXXX", tmpdir,
	    tmpdir[strlen(tmpdir) - 1] == '/' ? "" : "/") == -1) {
		err(1, "asprintf");
	}
	if ((fd = mkstemp(tmppath)) == -1)
		err(1, "mkstemp");
	wantcleanup = 1;
	atexit(cleanup);
	if ((fp = fdopen(fd, "w+")) == NULL) {
		err(1, "fdopen");
	}

	init();

	pr_form = getenv("PR_FORM");
	if (pr_form) {
		char buf[BUFSIZ];
		size_t len;
		FILE *frfp;

		frfp = fopen(pr_form, "r");
		if (frfp == NULL) {
			fprintf(stderr, "sendbug: can't seem to read your"
			    " template file (`%s'), ignoring PR_FORM\n",
			    pr_form);
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

	if (fflush(fp) == EOF || fstat(fd, &sb) == -1 || fclose(fp) == EOF) {
		err(1, "error creating template");
	}
	mtime = sb.st_mtime;

 edit:
	if ((editor = getenv("EDITOR")) == NULL)
		editor = "vi";
	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		wantcleanup = 0;
		if (asprintf(&argp[2], "%s %s", editor, tmppath) == -1)
			err(1, "asprintf");
		execv(_PATH_BSHELL, argp);
		err(1, "execv");
	default:
		wait(NULL);
		break;
	}

	if (stat(tmppath, &sb) == -1) {
		err(1, "stat");
	}
	if (mtime == sb.st_mtime) {
		errx(1, "report unchanged, nothing sent");
	}

 prompt:
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

void
init(void)
{
	size_t len = 0, namelen;
	int sysname[2];
	const char *src;
	char *dst, *cp;

	if ((pw = getpwuid(getuid())) == NULL) {
		err(1, "getpwuid");
	}
	namelen = strlen(pw->pw_name);

	/* Add length of expanded '&', minus existing '&'. */
	src = pw->pw_gecos;
	src += strcspn(src, ",&");
	while (*src == '&') {
		len += namelen - 1;
		/* Look for next '&', skipping the one we just found. */
		src += 1 + strcspn(src, ",&");
	}
	/* Add full name length, including all those '&' we skipped. */
	len += src - pw->pw_gecos;
	if ((fullname = malloc(len + 1)) == NULL) {
		err(1, "malloc");
	}
	dst = fullname;
	src = pw->pw_gecos;
	while (*src != ',' && *src != '\0') {
		/* Copy text up to ',' or '&' and skip. */
		len = strcspn(src, ",&");
		memcpy(dst, src, len);
		dst += len;
		src += len;
		/* Replace '&' with login. */
		if (*src == '&') {
			memcpy(dst, pw->pw_name, namelen);
			*dst = toupper((unsigned char)*dst);
			dst += namelen;
			++src;
		}
	}
	*dst = '\0';

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_OSTYPE;
	len = sizeof(os) - 1;
	if (sysctl(sysname, 2, &os, &len, NULL, 0) == -1) {
		err(1, "sysctl");
	}

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_OSRELEASE;
	len = sizeof(rel) - 1;
	if (sysctl(sysname, 2, &rel, &len, NULL, 0) == -1) {
		err(1, "sysctl");
	}

	sysname[0] = CTL_KERN;
	sysname[1] = KERN_VERSION;
	len = sizeof(details) - 1;
	if (sysctl(sysname, 2, &details, &len, NULL, 0) == -1) {
		err(1, "sysctl");
	}

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
	if (sysctl(sysname, 2, &mach, &len, NULL, 0) == -1) {
		err(1, "sysctl");
	}

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
			char *sp = NULL, *ep = NULL;
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
	fprintf(fp, "\tDetails     : %s\n", details);
	fprintf(fp, "\tArchitecture: %s.%s\n", os, mach);
	fprintf(fp, "\tMachine     : %s\n", mach);
	fprintf(fp, ">Description:\n");
	fprintf(fp, "\t<precise description of the problem (multiple lines)>\n");
	fprintf(fp, ">How-To-Repeat:\n");
	fprintf(fp, "\t<code/input/activities to reproduce the problem (multiple lines)>\n");
	fprintf(fp, ">Fix:\n");
	fprintf(fp, "\t<how to correct or work around the problem, if known (multiple lines)>\n");
}
