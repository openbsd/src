/*	$OpenBSD: crontab.c,v 1.23 2001/10/24 17:28:16 millert Exp $	*/
/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$OpenBSD: crontab.c,v 1.23 2001/10/24 17:28:16 millert Exp $";
#endif

/* crontab - install and manage per-user crontab files
 * vix 02may87 [RCS has the rest of the log]
 * vix 26jan87 [original]
 */

#define	MAIN_PROGRAM

#include "cron.h"

#define NHEADER_LINES 3

enum opt_t	{ opt_unknown, opt_list, opt_delete, opt_edit, opt_replace };

#if DEBUGGING
static char	*Options[] = { "???", "list", "delete", "edit", "replace" };
static char	*getoptargs = "u:lerx:";
#else
static char	*getoptargs = "u:ler";
#endif

static	PID_T		Pid;
static	char		User[MAX_UNAME], RealUser[MAX_UNAME];
static	char		Filename[MAX_FNAME], TempFilename[MAX_FNAME];
static	FILE		*NewCrontab;
static	int		CheckErrorCount;
static	enum opt_t	Option;
static	struct passwd	*pw;
static	void		list_cmd(void),
			delete_cmd(void),
			edit_cmd(void),
			poke_daemon(void),
			check_error(const char *),
			parse_args(int c, char *v[]);
static	int		replace_cmd(void);
static	void		clean_turds __P((int));

static void
usage(const char *msg) {
	fprintf(stderr, "%s: usage error: %s\n", ProgramName, msg);
	fprintf(stderr, "usage:\t%s [-u user] file\n", ProgramName);
	fprintf(stderr, "\t%s [-u user] [ -e | -l | -r ]\n", ProgramName);
	fprintf(stderr, "\t\t(default operation is replace, per 1003.2)\n");
	fprintf(stderr, "\t-e\t(edit user's crontab)\n");
	fprintf(stderr, "\t-l\t(list user's crontab)\n");
	fprintf(stderr, "\t-r\t(delete user's crontab)\n");
	exit(ERROR_EXIT);
}

int
main(int argc, char *argv[]) {
	int	exitstatus;

	Pid = getpid();
	ProgramName = argv[0];

	setlocale(LC_ALL, "");

#if defined(BSD)
	setlinebuf(stderr);
#endif
	parse_args(argc, argv);		/* sets many globals, opens a file */
	set_cron_uid();
	set_cron_cwd();
	if (!allowed(User)) {
		fprintf(stderr,
			"You (%s) are not allowed to use this program (%s)\n",
			User, ProgramName);
		fprintf(stderr, "See crontab(1) for more information\n");
		log_it(RealUser, Pid, "AUTH", "crontab command not allowed");
		exit(ERROR_EXIT);
	}
	exitstatus = OK_EXIT;
	switch (Option) {
	case opt_list:
		list_cmd();
		break;
	case opt_delete:
		delete_cmd();
		break;
	case opt_edit:
		edit_cmd();
		break;
	case opt_replace:
		if (replace_cmd() < 0)
			exitstatus = ERROR_EXIT;
		break;
	default:
		abort();
	}
	exit(0);
	/*NOTREACHED*/
}

static void
parse_args(int argc, char *argv[]) {
	int argch;

	if (!(pw = getpwuid(getuid()))) {
		fprintf(stderr, "%s: your UID isn't in the passwd file.\n",
			ProgramName);
		fprintf(stderr, "bailing out.\n");
		exit(ERROR_EXIT);
	}
	if (strlen(pw->pw_name) >= sizeof User) {
		fprintf(stderr, "username too long\n");
		exit(ERROR_EXIT);
	}
	strcpy(User, pw->pw_name);
	strcpy(RealUser, User);
	Filename[0] = '\0';
	Option = opt_unknown;
	while (-1 != (argch = getopt(argc, argv, getoptargs))) {
		switch (argch) {
		case 'x':
			if (!set_debug_flags(optarg))
				usage("bad debug option");
			break;
		case 'u':
			if (MY_UID(pw) != ROOT_UID) {
				fprintf(stderr,
					"must be privileged to use -u\n");
				exit(ERROR_EXIT);
			}
			if (!(pw = getpwnam(optarg))) {
				fprintf(stderr, "%s:  user `%s' unknown\n",
					ProgramName, optarg);
				exit(ERROR_EXIT);
			}
			if (strlen(optarg) >= sizeof User)
				usage("username too long");
			(void) strcpy(User, optarg);
			break;
		case 'l':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_list;
			break;
		case 'r':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_delete;
			break;
		case 'e':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_edit;
			break;
		default:
			usage("unrecognized option");
		}
	}

	endpwent();

	if (Option != opt_unknown) {
		if (argv[optind] != NULL)
			usage("no arguments permitted after this option");
	} else {
		if (argv[optind] != NULL) {
			Option = opt_replace;
			if (strlen(argv[optind]) >= sizeof Filename)
				usage("filename too long");
			(void) strcpy (Filename, argv[optind]);
		} else
			usage("file name must be specified for replace");
	}

	if (Option == opt_replace) {
		/* we have to open the file here because we're going to
		 * chdir(2) into /var/cron before we get around to
		 * reading the file.
		 */
		if (!strcmp(Filename, "-"))
			NewCrontab = stdin;
		else {
			/* relinquish the setuid status of the binary during
			 * the open, lest nonroot users read files they should
			 * not be able to read.  we can't use access() here
			 * since there's a race condition.  thanks go out to
			 * Arnt Gulbrandsen <agulbra@pvv.unit.no> for spotting
			 * the race.
			 */

			if (swap_uids() < OK) {
				perror("swapping uids");
				exit(ERROR_EXIT);
			}
			if (!(NewCrontab = fopen(Filename, "r"))) {
				perror(Filename);
				exit(ERROR_EXIT);
			}
			if (swap_uids_back() < OK) {
				perror("swapping uids back");
				exit(ERROR_EXIT);
			}
		}
	}

	Debug(DMISC, ("user=%s, file=%s, option=%s\n",
		      User, Filename, Options[(int)Option]))
}


static void
list_cmd(void) {
	char n[MAX_FNAME];
	FILE *f;
	int ch;

	log_it(RealUser, Pid, "LIST", User);
	if (!glue_strings(n, sizeof n, SPOOL_DIR, User, '/')) {
		fprintf(stderr, "path too long\n");
		exit(ERROR_EXIT);
	}
	if (!(f = fopen(n, "r"))) {
		if (errno == ENOENT)
			fprintf(stderr, "no crontab for %s\n", User);
		else
			perror(n);
		exit(ERROR_EXIT);
	}

	/* file is open. copy to stdout, close.
	 */
	Set_LineNum(1)
	while (EOF != (ch = get_char(f)))
		putchar(ch);
	fclose(f);
}

static void
delete_cmd(void) {
	char n[MAX_FNAME];

	log_it(RealUser, Pid, "DELETE", User);
	if (!glue_strings(n, sizeof n, SPOOL_DIR, User, '/')) {
		fprintf(stderr, "path too long\n");
		exit(ERROR_EXIT);
	}
	if (unlink(n) != 0) {
		if (errno == ENOENT)
			fprintf(stderr, "no crontab for %s\n", User);
		else
			perror(n);
		exit(ERROR_EXIT);
	}
	poke_daemon();
}

static void
check_error(const char *msg) {
	CheckErrorCount++;
	fprintf(stderr, "\"%s\":%d: %s\n", Filename, LineNumber-1, msg);
}

static void
edit_cmd(void) {
	char n[MAX_FNAME], q[MAX_TEMPSTR], *editor;
	FILE *f;
	int ch, t, x;
	struct stat statbuf;
	struct timespec mtimespec;
	struct timeval tv[2];
	off_t size;
	WAIT_T waiter;
	PID_T pid, xpid;

	log_it(RealUser, Pid, "BEGIN EDIT", User);
	if (!glue_strings(n, sizeof n, SPOOL_DIR, User, '/')) {
		fprintf(stderr, "path too long\n");
		exit(ERROR_EXIT);
	}
	if (!(f = fopen(n, "r"))) {
		if (errno != ENOENT) {
			perror(n);
			exit(ERROR_EXIT);
		}
		fprintf(stderr, "no crontab for %s - using an empty one\n",
			User);
		if (!(f = fopen(_PATH_DEVNULL, "r"))) {
			perror(_PATH_DEVNULL);
			exit(ERROR_EXIT);
		}
	}

	if (fstat(fileno(f), &statbuf) < 0) {
		perror("fstat");
		goto fatal;
	}
	size = statbuf.st_size;
	memcpy(&mtimespec, &statbuf.st_mtimespec, sizeof(mtimespec));
	TIMESPEC_TO_TIMEVAL(&tv[0], &statbuf.st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &statbuf.st_mtimespec);

	/* Turn off signals. */
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);

	if (!glue_strings(Filename, sizeof Filename, _PATH_TMP,
	    "crontab.XXXXXXXXXX", '/')) {
		fprintf(stderr, "path too long\n");
		goto fatal;
	}
	if (-1 == (t = mkstemp(Filename))) {
		perror(Filename);
		goto fatal;
	}
#ifdef HAS_FCHOWN
	if (fchown(t, MY_UID(pw), MY_GID(pw)) < 0) {
		perror("fchown");
		goto fatal;
	}
#else
	if (chown(Filename, MY_UID(pw), MY_GID(pw)) < 0) {
		perror("chown");
		goto fatal;
	}
#endif
	if (!(NewCrontab = fdopen(t, "r+"))) {
		perror("fdopen");
		goto fatal;
	}

	Set_LineNum(1)

	/* ignore the top few comments since we probably put them there.
	 */
	for (x = 0; x < NHEADER_LINES; x++) {
		ch = get_char(f);
		if (EOF == ch)
			break;
		if ('#' != ch) {
			putc(ch, NewCrontab);
			break;
		}
		while (EOF != (ch = get_char(f)))
			if (ch == '\n')
				break;
		if (EOF == ch)
			break;
	}

	/* copy the rest of the crontab (if any) to the temp file.
	 */
	if (EOF != ch)
		while (EOF != (ch = get_char(f)))
			putc(ch, NewCrontab);
	fclose(f);
	if (fflush(NewCrontab) < OK) {
		perror(Filename);
		exit(ERROR_EXIT);
	}
	(void)futimes(t, tv);
 again:
	rewind(NewCrontab);
	if (ferror(NewCrontab)) {
		fprintf(stderr, "%s: error while writing new crontab to %s\n",
			ProgramName, Filename);
 fatal:
		unlink(Filename);
		exit(ERROR_EXIT);
	}

	if (((editor = getenv("VISUAL")) == NULL || *editor == '\0') &&
	    ((editor = getenv("EDITOR")) == NULL || *editor == '\0')) {
		editor = EDITOR;
	}

	/* we still have the file open.  editors will generally rewrite the
	 * original file rather than renaming/unlinking it and starting a
	 * new one; even backup files are supposed to be made by copying
	 * rather than by renaming.  if some editor does not support this,
	 * then don't use it.  the security problems are more severe if we
	 * close and reopen the file around the edit.
	 */

	switch (pid = fork()) {
	case -1:
		perror("fork");
		goto fatal;
	case 0:
		/* child */
		if (setgid(MY_GID(pw)) < 0) {
			perror("setgid(getgid())");
			exit(ERROR_EXIT);
		}
		if (setuid(MY_UID(pw)) < 0) {
			perror("setuid(getuid())");
			exit(ERROR_EXIT);
		}
		if (chdir(_PATH_TMP) < 0) {
			perror(_PATH_TMP);
			exit(ERROR_EXIT);
		}
		if (!glue_strings(q, sizeof q, editor, Filename, ' ')) {
			fprintf(stderr, "%s: editor command line too long\n",
			    ProgramName);
			exit(ERROR_EXIT);
		}
		execlp(_PATH_BSHELL, _PATH_BSHELL, "-c", q, (char *)NULL);
		perror(editor);
		exit(ERROR_EXIT);
		/*NOTREACHED*/
	default:
		/* parent */
		break;
	}

	/* parent */
	for (;;) {
		xpid = waitpid(pid, &waiter, WUNTRACED);
		if (xpid == -1) {
			if (errno != EINTR)
				fprintf(stderr, "%s: waitpid() failed waiting for PID %ld from \"%s\": %s\n",
					ProgramName, (long)pid, editor, strerror(errno));
		} else if (xpid != pid) {
			fprintf(stderr, "%s: wrong PID (%ld != %ld) from \"%s\"\n",
				ProgramName, (long)xpid, (long)pid, editor);
			goto fatal;
		} else if (WIFSTOPPED(waiter)) {
			raise(WSTOPSIG(waiter));
		} else if (WIFEXITED(waiter) && WEXITSTATUS(waiter)) {
			fprintf(stderr, "%s: \"%s\" exited with status %d\n",
				ProgramName, editor, WEXITSTATUS(waiter));
			goto fatal;
		} else if (WIFSIGNALED(waiter)) {
			fprintf(stderr,
				"%s: \"%s\" killed; signal %d (%score dumped)\n",
				ProgramName, editor, WTERMSIG(waiter),
				WCOREDUMP(waiter) ?"" :"no ");
			goto fatal;
		} else
			break;
	}
	(void)signal(SIGHUP, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	if (fstat(t, &statbuf) < 0) {
		perror("fstat");
		goto fatal;
	}
	if (timespeccmp(&mtimespec, &statbuf.st_mtimespec, -) == 0 &&
	    size == statbuf.st_size) {   
		fprintf(stderr, "%s: no changes made to crontab\n",
			ProgramName);
		goto remove;
	}
	fprintf(stderr, "%s: installing new crontab\n", ProgramName);
	switch (replace_cmd()) {
	case 0:
		break;
	case -1:
		for (;;) {
			printf("Do you want to retry the same edit? ");
			fflush(stdout);
			q[0] = '\0';
			(void) fgets(q, sizeof q, stdin);
			switch (islower(q[0]) ? q[0] : tolower(q[0])) {
			case 'y':
				goto again;
			case 'n':
				goto abandon;
			default:
				fprintf(stderr, "Enter Y or N\n");
			}
		}
		/*NOTREACHED*/
	case -2:
	abandon:
		fprintf(stderr, "%s: edits left in %s\n",
			ProgramName, Filename);
		goto done;
	default:
		fprintf(stderr, "%s: panic: bad switch() in replace_cmd()\n",
			ProgramName);
		goto fatal;
	}
 remove:
	unlink(Filename);
 done:
	log_it(RealUser, Pid, "END EDIT", User);
}


/* returns	0	on success
 *		-1	on syntax error
 *		-2	on install error
 */
static int
replace_cmd(void) {
	char n[MAX_FNAME], envstr[MAX_ENVSTR];
	FILE *tmp;
	int ch, eof, fd;
	int error = 0;
	entry *e;
	time_t now = time(NULL);
	char **envp = env_init();
	struct stat sb;

	if (envp == NULL) {
		fprintf(stderr, "%s: Cannot allocate memory.\n", ProgramName);
		return (-2);
	}
	if (!glue_strings(TempFilename, sizeof TempFilename, SPOOL_DIR,
	    "tmp.XXXXXXXXXX", '/')) {
		TempFilename[0] = '\0';
		fprintf(stderr, "path too long\n");
		return (-2);
	}
	if ((fd = mkstemp(TempFilename)) == -1 || !(tmp = fdopen(fd, "w+"))) {
		perror(TempFilename);
		if (fd != -1) {
			close(fd);
			unlink(TempFilename);
		}
		TempFilename[0] = '\0';
		return (-2);
	}

	(void) signal(SIGHUP, clean_turds);
	(void) signal(SIGINT, clean_turds);
	(void) signal(SIGQUIT, clean_turds);

	/* write a signature at the top of the file.
	 *
	 * VERY IMPORTANT: make sure NHEADER_LINES agrees with this code.
	 */
	fprintf(tmp, "# DO NOT EDIT THIS FILE - edit the master and reinstall.\n");
	fprintf(tmp, "# (%s installed on %-24.24s)\n", Filename, ctime(&now));
	fprintf(tmp, "# (Cron version -- %s)\n", rcsid);

	/* copy the crontab to the tmp
	 */
	rewind(NewCrontab);
	Set_LineNum(1)
	while (EOF != (ch = get_char(NewCrontab)))
		putc(ch, tmp);
	ftruncate(fileno(tmp), ftell(tmp));	/* XXX redundant with "w+"? */
	fflush(tmp);  rewind(tmp);

	if (ferror(tmp)) {
		fprintf(stderr, "%s: error while writing new crontab to %s\n",
			ProgramName, TempFilename);
		fclose(tmp);
		error = -2;
		goto done;
	}

	/* check the syntax of the file being installed.
	 */

	/* BUG: was reporting errors after the EOF if there were any errors
	 * in the file proper -- kludged it by stopping after first error.
	 *		vix 31mar87
	 */
	Set_LineNum(1 - NHEADER_LINES)
	CheckErrorCount = 0;  eof = FALSE;
	while (!CheckErrorCount && !eof) {
		switch (load_env(envstr, tmp)) {
		case ERR:
			eof = TRUE;
			break;
		case FALSE:
			e = load_entry(tmp, check_error, pw, envp);
			if (e)
				free(e);
			break;
		case TRUE:
			break;
		}
	}

	if (CheckErrorCount != 0) {
		fprintf(stderr, "errors in crontab file, can't install.\n");
		fclose(tmp);
		error = -1;
		goto done;
	}

	if (fstat(fileno(tmp), &sb))
		sb.st_gid = -1;

#ifdef HAS_FCHOWN
	if (fchown(fileno(tmp), ROOT_UID, sb.st_gid) < OK) {
		perror("fchown");
		fclose(tmp);
		error = -2;
		goto done;
	}
#else
	if (chown(TempFilename, ROOT_UID, sb.st_gid) < OK) {
		perror("chown");
		fclose(tmp);
		error = -2;
		goto done;
	}
#endif

#ifdef HAS_FCHMOD
	if (fchmod(fileno(tmp), 0600) < OK) {
		perror("fchmod");
		fclose(tmp);
		error = -2;
		goto done;
	}
#else
	if (chmod(TempFilename, 0600) < OK) {
		perror("chmod");
		fclose(tmp);
		error = -2;
		goto done;
	}
#endif

	if (fclose(tmp) == EOF) {
		perror("fclose");
		error = -2;
		goto done;
	}

	if (!glue_strings(n, sizeof n, SPOOL_DIR, User, '/')) {
		fprintf(stderr, "path too long\n");
		error = -2;
		goto done;
	}
	if (rename(TempFilename, n)) {
		fprintf(stderr, "%s: error renaming %s to %s\n",
			ProgramName, TempFilename, n);
		perror("rename");
		error = -2;
		goto done;
	}
	TempFilename[0] = '\0';
	log_it(RealUser, Pid, "REPLACE", User);

	poke_daemon();

done:
	(void) signal(SIGHUP, SIG_DFL);
	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	if (TempFilename[0]) {
		(void) unlink(TempFilename);
		TempFilename[0] = '\0';
	}
	return (error);
}

static void
poke_daemon() {
	if (utime(SPOOL_DIR, NULL) < OK) {
		fprintf(stderr, "crontab: can't update mtime on spooldir\n");
		perror(SPOOL_DIR);
		return;
	}
}

static void
clean_turds(signo)
	int signo;
{
	if (TempFilename[0])
		(void) unlink(TempFilename);
	if (signo) {
		(void) signal(signo, SIG_DFL);
		(void) raise(signo);
	}
}
