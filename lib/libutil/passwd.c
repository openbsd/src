/*	$OpenBSD: passwd.c,v 1.18 1998/06/22 14:41:47 millert Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: passwd.c,v 1.18 1998/06/22 14:41:47 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <limits.h>

#include "util.h"

#define NUM_OPTIONS     2       /* Number of hardcoded defaults */

static void	pw_cont __P((int sig));

static const char options[NUM_OPTIONS][2][80] =
{
	{"localcipher", "blowfish,4"},
	{"ypcipher", "old"}
};

static char pw_defdir[] = "/etc";
static char *pw_dir = pw_defdir;
static char *pw_lck;

/* Removes trailers. */
static void
remove_trailing_space(line)
	char   *line;
{
	char   *p;
	/* Remove trailing spaces */
	p = line;
	while (isspace(*p))
		p++;
	memcpy(line, p, strlen(p) + 1);

	p = line + strlen(line) - 1;
	while (isspace(*p))
		p--;
	*(p + 1) = '\0';
}


/* Get one line, remove trailers */
static int
read_line(fp, line, max)
	FILE   *fp;
	char   *line;
	int     max;
{
	char   *p;
	/* Read one line of config */
	if (fgets(line, max, fp) == 0)
		return 0;
	if (!(p = strchr(line, '\n'))) {
		warnx("line too long");
		return 0;
	}
	*p = '\0';

	/* Remove comments */
	if ((p = strchr(line, '#')))
		*p = '\0';

	remove_trailing_space(line);
	return 1;
}


static const char *
pw_default(option)
	char   *option;
{
	int     i;
	for (i = 0; i < NUM_OPTIONS; i++)
		if (!strcmp(options[i][0], option))
			return options[i][1];
	return NULL;
}

char *
pw_file(nm)
	const char *nm;
{
	const char *p = strrchr(nm, '/');
	char *new_nm;

	if (p)
		p++;
	else
		p = nm;
	new_nm = malloc(strlen(pw_dir) + strlen(p) + 2);
	if (!new_nm)
		return NULL;
	sprintf(new_nm, "%s/%s", pw_dir, p);
	return new_nm;
}


/*
 * Retrieve password information from the /etc/passwd.conf file,
 * at the moment this is only for choosing the cipher to use.
 * It could easily be used for other authentication methods as
 * well. 
 */
void
pw_getconf(data, max, key, option)
	char	*data;
	size_t	max;
	const char *key;
	const char *option;
{
	FILE   *fp;
	char    line[LINE_MAX];
	static char result[LINE_MAX];
	char   *p;
	int     got = 0;
	int     found = 0;

	result[0] = '\0';

	p = pw_file(_PATH_PASSWDCONF);
	if (!p || (fp = fopen(p, "r")) == NULL) {
		if (p)
			free(p);
		if((p = (char *)pw_default(option))) {
			strncpy(data, p, max - 1);
			data[max - 1] = '\0';
		} else
			data[0] = '\0';
		return;
	}
	free(p);

	while (!found && (got || read_line(fp, line, LINE_MAX))) {
		got = 0;
		if (strncmp(key, line, strlen(key)) ||
		    line[strlen(key)] != ':')
			continue;

		/* Now we found our specified key */
		while (read_line(fp, line, LINE_MAX)) {
		     char   *p2;
		     /* Leaving key field */
		     if (strchr(line, ':')) {
			  got = 1;
			  break;
		     }
		     p2 = line;
		     if (!(p = strsep(&p2, "=")) || p2 == NULL)
			  continue;
		     remove_trailing_space(p);
		     if (!strncmp(p, option, strlen(option))) {
			  remove_trailing_space(p2);
			  strcpy(result, p2);
			  found = 1;
			  break;
		     }
		}
	}
	fclose(fp);

	/* 
	 * If we got no result and were looking for a default
	 * value, try hard coded defaults.
	 */

	if (!strlen(result) && !strcmp(key,"default") &&
	    (p=(char *)pw_default(option)))
		strncpy(data, p, max - 1);
	else 
		strncpy(data, result, max - 1);
	data[max - 1] = '\0';
}


void
pw_setdir(dir)
	const char *dir;
{
	char *p;

	if (strcmp (dir, pw_dir) == 0)
		return;
	if (pw_dir != pw_defdir)
		free(pw_dir);
	pw_dir = strdup(dir);
	if (pw_lck) {
		p = pw_file(pw_lck);
		free(pw_lck);
		pw_lck = p;
	}
}


int
pw_lock(retries)
	int retries;
{
	int i, fd;
	mode_t old_mode;

	if (!pw_lck)
		return (-1);
	/* Acquire the lock file.  */
	old_mode = umask(0);
	fd = open(pw_lck, O_WRONLY|O_CREAT|O_EXCL, 0600);
	for (i = 0; i < retries && fd < 0 && errno == EEXIST; i++) {
		sleep(1);
		fd = open(pw_lck, O_WRONLY|O_CREAT|O_EXCL, 0600);
	}
	umask(old_mode);
	return (fd);
}

int
pw_mkdb()
{
	int pstat;
	pid_t pid;
	struct stat sb;

	/* A zero length passwd file is never ok */
	if (pw_lck && stat(pw_lck, &sb) == 0) {
		if (sb.st_size == 0) {
			warnx("%s is zero length", pw_lck);
			return (-1);
		}
	}

	pid = vfork();
	if (pid == -1)
		return (-1);
	if (pid == 0) {
		if (pw_lck)
			execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", "-d", pw_dir,
			    pw_lck, NULL);
		_exit(1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		return (-1);
	return (0);
}

int
pw_abort()
{
	return (pw_lck ? unlink(pw_lck) : -1);
}

/* Everything below this point is intended for the convenience of programs
 * which allow a user to interactively edit the passwd file.  Errors in the
 * routines below will cause the process to abort. */

static pid_t editpid = -1;

static void
pw_cont(sig)
	int sig;
{

	if (editpid != -1)
		kill(editpid, sig);
}

void
pw_init()
{
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU, &rlim);
	(void)setrlimit(RLIMIT_FSIZE, &rlim);
	(void)setrlimit(RLIMIT_STACK, &rlim);
	(void)setrlimit(RLIMIT_DATA, &rlim);
	(void)setrlimit(RLIMIT_RSS, &rlim);

	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rlim);

	/* Turn off signals. */
	(void)signal(SIGALRM, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGCONT, pw_cont);

	if (!pw_lck)
		pw_lck = pw_file(_PATH_MASTERPASSWD_LOCK);
}

void
pw_edit(notsetuid, filename)
	int notsetuid;
	const char *filename;
{
	int pstat;
	char *p, *editor;
	char *argp[] = {"sh", "-c", NULL, NULL};

#ifdef __GNUC__
	(void)&editor;
#endif
	if (!filename) {
		filename = pw_lck;
		if (!filename)
			return;
	}

	if (!(editor = getenv("EDITOR")))
		editor = _PATH_VI;

	p = malloc(strlen(editor) + 1 + strlen(filename) + 1);
	if (p == NULL)
		return;
	sprintf(p, "%s %s", editor, filename);
	argp[2] = p;

	switch(editpid = vfork()) {
	case -1:			/* error */
		free(p);
		return;
	case 0:				/* child */
		if (notsetuid) {
			setgid(getgid());
			setuid(getuid());
		}
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}

	free(p);
	for (;;) {
		editpid = waitpid(editpid, (int *)&pstat, WUNTRACED);
		if (editpid == -1)
			pw_error(editor, 1, 1);
		else if (WIFSTOPPED(pstat))
			raise(WSTOPSIG(pstat));
		else if (WIFEXITED(pstat) && WEXITSTATUS(pstat) == 0)
			break;
		else
			pw_error(editor, 1, 1);
	}
	editpid = -1;
}

void
pw_prompt()
{
	int first, c;

	(void)printf("re-edit the password file? [y]: ");
	(void)fflush(stdout);
	first = c = getchar();
	while (c != '\n' && c != EOF)
		c = getchar();
	if (first == 'n')
		pw_error(NULL, 0, 0);
}

void
pw_copy(ffd, tfd, pw)
	int ffd, tfd;
	struct passwd *pw;
{
	FILE   *from, *to;
	int	done;
	char   *p, buf[8192];
	char   *master = pw_file(_PATH_MASTERPASSWD);

	if (!master)
		pw_error(NULL, 0, 1);
	if (!(from = fdopen(ffd, "r")))
		pw_error(master, 1, 1);
	if (!(to = fdopen(tfd, "w")))
		pw_error(pw_lck ? pw_lck : NULL, pw_lck ? 1 : 0, 1);

	for (done = 0; fgets(buf, sizeof(buf), from);) {
		if (!strchr(buf, '\n')) {
			warnx("%s: line too long", master);
			pw_error(NULL, 0, 1);
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		if (!(p = strchr(buf, ':'))) {
			warnx("%s: corrupted entry", master);
			pw_error(NULL, 0, 1);
		}
		*p = '\0';
		if (strcmp(buf, pw->pw_name)) {
			*p = ':';
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		(void)fprintf(to, "%s:%s:%d:%d:%s:%d:%d:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid,
		    pw->pw_class, pw->pw_change, pw->pw_expire, pw->pw_gecos,
		    pw->pw_dir, pw->pw_shell);
		done = 1;
		if (ferror(to))
			goto err;
	}
	if (!done)
		(void)fprintf(to, "%s:%s:%d:%d:%s:%d:%d:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid,
		    pw->pw_class, pw->pw_change, pw->pw_expire, pw->pw_gecos,
		    pw->pw_dir, pw->pw_shell);

	if (ferror(to))
err:
	pw_error(NULL, 0, 1);
	(void)fclose(to);
}

int
pw_scan(bp, pw, flags)
	char *bp;
	struct passwd *pw;
	int *flags;
{
	u_long id;
	int root;
	char *p, *sh, *p2;

	if (flags != (int *)NULL)
		*flags = 0;

	if (!(pw->pw_name = strsep(&bp, ":")))		/* login */
		goto fmt;
	root = !strcmp(pw->pw_name, "root");

	if (!(pw->pw_passwd = strsep(&bp, ":")))	/* passwd */
		goto fmt;

	if (!(p = strsep(&bp, ":")))			/* uid */
		goto fmt;
	id = strtoul(p, &p2, 10);
	if (root && id) {
		warnx("root uid should be 0");
		return (0);
	}
	if (*p2 != '\0') {
		warnx("illegal uid field");
		return (0);
	}
	if (id >= UINT_MAX) {
		/* errno is set to ERANGE by strtoul(3) */
		warnx("uid greater than %u", UINT_MAX-1);
		return (0);
	}
	pw->pw_uid = (uid_t)id;
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOUID;

	if (!(p = strsep(&bp, ":")))			/* gid */
		goto fmt;
	id = strtoul(p, &p2, 10);
	if (*p2 != '\0') {
		warnx("illegal gid field");
		return (0);
	}
	if (id > UINT_MAX) {
		/* errno is set to ERANGE by strtoul(3) */
		warnx("gid greater than %u", UINT_MAX-1);
		return (0);
	}
	pw->pw_gid = (gid_t)id;
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOGID;

	pw->pw_class = strsep(&bp, ":");		/* class */
	if (!(p = strsep(&bp, ":")))			/* change */
		goto fmt;
	pw->pw_change = atol(p);
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOCHG;
	if (!(p = strsep(&bp, ":")))			/* expire */
		goto fmt;
	pw->pw_expire = atol(p);
	if ((*p == '\0') && (flags != (int *)NULL))
		*flags |= _PASSWORD_NOEXP;
	pw->pw_gecos = strsep(&bp, ":");		/* gecos */
	pw->pw_dir = strsep(&bp, ":");			/* directory */
	if (!(pw->pw_shell = strsep(&bp, ":")))		/* shell */
		goto fmt;

	p = pw->pw_shell;
	if (root && *p) {				/* empty == /bin/sh */
		for (setusershell();;) {
			if (!(sh = getusershell())) {
				warnx("warning, unknown root shell");
				break;
			}
			if (!strcmp(p, sh))
				break;	
		}
		endusershell();
	}

	if ((p = strsep(&bp, ":"))) {			/* too many */
fmt:		warnx("corrupted entry");
		return (0);
	}

	return (1);
}

void
pw_error(name, err, eval)
	const char *name;
	int err, eval;
{
	char   *master = pw_file(_PATH_MASTERPASSWD);

	if (err)
		warn(name);
	if (master)
		warnx("%s: unchanged", master);
	pw_abort();
	exit(eval);
}
