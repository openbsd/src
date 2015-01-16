/*	$OpenBSD: sendbug.c,v 1.69 2015/01/16 06:40:11 deraadt Exp $	*/

/*
 * Written by Ray Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/types.h>
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
#define BEGIN64 "begin-base64 "
#define END64 "===="

int	checkfile(const char *);
void	debase(void);
void	dmesg(FILE *);
int	editit(const char *);
void	hwdump(FILE *);
void	init(void);
int	matchline(const char *, const char *, size_t);
int	prompt(void);
int	send_file(const char *, int);
int	sendmail(const char *);
void	template(FILE *);
void	usbdevs(FILE *);

const char *categories = "system user library documentation kernel "
    "alpha amd64 arm hppa i386 m88k mips64 powerpc sh sparc sparc64 vax";
char *version = "5.5";
const char *comment[] = {
	"<synopsis of the problem (one line)>",
	"<PR category (one line)>",
	"<precise description of the problem (multiple lines)>",
	"<code/input/activities to reproduce the problem (multiple lines)>",
	"<how to correct or work around the problem, if known (multiple lines)>"
};

struct passwd *pw;
char os[BUFSIZ], rel[BUFSIZ], mach[BUFSIZ], details[BUFSIZ];
const char *tmpdir;
char *tmppath;
int Dflag, Pflag, wantcleanup;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-DEPV]\n", __progname);
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
	struct stat sb;
	char *pr_form;
	time_t mtime;
	FILE *fp;

	while ((ch = getopt(argc, argv, "DEPV")) != -1)
		switch (ch) {
		case 'D':
			Dflag = 1;
			break;
		case 'E':
			debase();
			exit(0);
		case 'P':
			Pflag = 1;
			break;
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

	if (Pflag) {
		init();
		template(stdout);
		exit(0);
	}

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

void
usbdevs(FILE *ofp)
{
	char buf[BUFSIZ];
	FILE *ifp;
	size_t len;

	if ((ifp = popen("usbdevs -v", "r")) != NULL) {
		while (!feof(ifp)) {
			len = fread(buf, 1, sizeof buf, ifp);
			if (len == 0)
				break;
			if (fwrite(buf, 1, len, ofp) != len)
				break;
		}
		pclose(ifp);
	}
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
	sig_t sighup, sigint, sigquit, sigchld;
	pid_t pid;
	int saved_errno, st, ret = -1;

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
	sigchld = signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == -1)
		goto fail;
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			goto fail;
	if (!WIFEXITED(st))
		errno = EINTR;
	else
		ret = WEXITSTATUS(st);

 fail:
	saved_errno = errno;
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	(void)signal(SIGCHLD, sigchld);
	free(p);
	errno = saved_errno;
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
		execl(_PATH_SENDMAIL, "sendmail",
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
	size_t len;
	int sysname[2];
	char *cp;

	if ((pw = getpwuid(getuid())) == NULL)
		err(1, "getpwuid");

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
	char *buf, *lbuf;
	FILE *fp;
	int rval = -1, saved_errno;

	if ((fp = fopen(file, "r")) == NULL)
		return (-1);
	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
			--len;
		} else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				goto end;
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		/* Skip lines starting with "SENDBUG". */
		if (strncmp(buf, "SENDBUG", sizeof("SENDBUG") - 1) == 0)
			continue;
		while (len) {
			char *sp = NULL, *ep = NULL;
			size_t copylen;

			if ((sp = strchr(buf, '<')) != NULL) {
				size_t i;

				for (i = 0; i < sizeof(comment) / sizeof(*comment); ++i) {
					size_t commentlen = strlen(comment[i]);

					if (strncmp(sp, comment[i], commentlen) == 0) {
						ep = sp + commentlen - 1;
						break;
					}
				}
			}
			/* Length of string before comment. */
			if (ep)
				copylen = sp - buf;
			else
				copylen = len;
			if (atomicio(vwrite, dst, buf, copylen) != copylen)
				goto end;
			if (!ep)
				break;
			/* Skip comment. */
			len -= ep - buf + 1;
			buf = ep + 1;
		}
		if (atomicio(vwrite, dst, "\n", 1) != 1)
			goto end;
	}
	rval = 0;
 end:
	saved_errno = errno;
	free(lbuf);
	fclose(fp);
	errno = saved_errno;
	return (rval);
}

/*
 * Does line start with `s' and end with non-comment and non-whitespace?
 * Note: Does not treat `line' as a C string.
 */
int
matchline(const char *s, const char *line, size_t linelen)
{
	size_t slen;
	int iscomment;

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
	iscomment = 0;
	while (linelen) {
		if (iscomment) {
			if (*line == '>')
				iscomment = 0;
		} else if (*line == '<')
			iscomment = 1;
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
	int category = 0, synopsis = 0;
	char *buf;

	if ((fp = fopen(pathname, "r")) == NULL) {
		warn("%s", pathname);
		return (0);
	}
	while ((buf = fgetln(fp, &len))) {
		if (matchline(">Category:", buf, len))
			category = 1;
		else if (matchline(">Synopsis:", buf, len))
			synopsis = 1;
	}
	fclose(fp);
	return (category && synopsis);
}

void
template(FILE *fp)
{
	fprintf(fp, "SENDBUG: -*- sendbug -*-\n");
	fprintf(fp, "SENDBUG: Lines starting with `SENDBUG' will"
	    " be removed automatically.\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG: Choose from the following categories:\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG: %s\n", categories);
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "SENDBUG:\n");
	fprintf(fp, "To: %s\n", "bugs@openbsd.org");
	fprintf(fp, "Subject: \n");
	fprintf(fp, "From: %s\n", pw->pw_name);
	fprintf(fp, "Cc: %s\n", pw->pw_name);
	fprintf(fp, "Reply-To: %s\n", pw->pw_name);
	fprintf(fp, "\n");
	fprintf(fp, ">Synopsis:\t%s\n", comment[0]);
	fprintf(fp, ">Category:\t%s\n", comment[1]);
	fprintf(fp, ">Environment:\n");
	fprintf(fp, "\tSystem      : %s %s\n", os, rel);
	fprintf(fp, "\tDetails     : %s\n", details);
	fprintf(fp, "\tArchitecture: %s.%s\n", os, mach);
	fprintf(fp, "\tMachine     : %s\n", mach);
	fprintf(fp, ">Description:\n");
	fprintf(fp, "\t%s\n", comment[2]);
	fprintf(fp, ">How-To-Repeat:\n");
	fprintf(fp, "\t%s\n", comment[3]);
	fprintf(fp, ">Fix:\n");
	fprintf(fp, "\t%s\n", comment[4]);

	if (!Dflag) {
		int root;

		fprintf(fp, "\n");
		root = !geteuid();
		if (!root)
			fprintf(fp, "SENDBUG: Run sendbug as root "
			    "if this is an ACPI report!\n");
		fprintf(fp, "SENDBUG: dmesg%s and usbdevs are attached.\n"
		    "SENDBUG: Feel free to delete or use the -D flag if they "
		    "contain sensitive information.\n",
		    root ? ", pcidump, acpidump" : "");
		fputs("\ndmesg:\n", fp);
		dmesg(fp);
		fputs("\nusbdevs:\n", fp);
		usbdevs(fp);
		if (root)
			hwdump(fp);
	}
}

void
hwdump(FILE *ofp)
{
	char buf[BUFSIZ];
	FILE *ifp;
	char *cmd, *acpidir;
	size_t len;

	if (gethostname(buf, sizeof(buf)) == -1)
		err(1, "gethostname");
	buf[strcspn(buf, ".")] = '\0';

	if (asprintf(&acpidir, "%s%sp.XXXXXXXXXX", tmpdir,
	    tmpdir[strlen(tmpdir) - 1] == '/' ? "" : "/") == -1)
		err(1, "asprintf");
	if (mkdtemp(acpidir) == NULL)
		err(1, "mkdtemp");

	if (asprintf(&cmd, "echo \"\\npcidump:\"; pcidump -xxv; "
	    "echo \"\\nacpidump:\"; cd %s && acpidump -o %s; "
	    "for i in *; do b64encode $i $i; done; rm -rf %s",
	    acpidir, buf, acpidir) == -1)
		err(1, "asprintf");

	if ((ifp = popen(cmd, "r")) != NULL) {
		while (!feof(ifp)) {
			len = fread(buf, 1, sizeof buf, ifp);
			if (len == 0)
				break;
			if (fwrite(buf, 1, len, ofp) != len)
				break;
		}
		pclose(ifp);
	}
	free(cmd);
	free(acpidir);
}

void
debase(void)
{
	char buf[BUFSIZ];
	FILE *fp = NULL;
	size_t len;

	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		len = strlen(buf);
		if (!strncmp(buf, BEGIN64, sizeof(BEGIN64) - 1)) {
			if (fp)
				errx(1, "double begin");
			fp = popen("b64decode", "w");
			if (!fp)
				errx(1, "popen b64decode");
		}
		if (fp && fwrite(buf, 1, len, fp) != len)
			errx(1, "pipe error");
		if (!strncmp(buf, END64, sizeof(END64) - 1)) {
			if (pclose(fp) == -1)
				errx(1, "pclose b64decode");
			fp = NULL;
		}
	}
}
