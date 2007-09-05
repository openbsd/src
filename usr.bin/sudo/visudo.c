/*
 * Copyright (c) 1996, 1998-2005 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

/*
 * Lock the sudoers file for safe editing (ala vipw) and check for parse errors.
 */

#define _SUDO_MAIN

#ifdef __TANDEM
# include <floss.h>
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifndef __TANDEM
# include <sys/file.h>
#endif
#include <sys/wait.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>
#include <pwd.h>
#if TIME_WITH_SYS_TIME
# include <time.h>
#endif
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#ifndef HAVE_TIMESPEC
# include <emul/timespec.h>
#endif

#include "sudo.h"
#include "version.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: visudo.c,v 1.166.2.10 2007/09/01 13:39:13 millert Exp $";
#endif /* lint */

struct sudoersfile {
    char *path;
    char *tpath;
    int fd;
    off_t orig_size;
    struct timespec orig_mtim;
};

/*
 * Function prototypes
 */
static void usage		__P((void)) __attribute__((__noreturn__));
static char whatnow		__P((void));
static RETSIGTYPE Exit		__P((int));
static void edit_sudoers	__P((struct sudoersfile *, char *, char *, int));
static void visudo		__P((struct sudoersfile *, char *, char *));
static void setup_signals	__P((void));
static void install_sudoers	__P((struct sudoersfile *, int));
static int check_syntax		__P(());
static int run_command		__P((char *, char **));
static char *get_args		__P((char *));
static char *get_editor		__P((char **));
static FILE *open_sudoers	__P((struct sudoersfile *));

int command_matches		__P((char *, char *));
int addr_matches		__P((char *));
int hostname_matches		__P((char *, char *, char *));
int netgr_matches		__P((char *, char *, char *, char *));
int usergr_matches		__P((char *, char *, struct passwd *));
int userpw_matches		__P((char *, char *, struct passwd *));
void init_parser		__P((void));
void yyerror			__P((char *));
void yyrestart			__P((FILE *));

/*
 * External globals exported by the parser
 */
extern FILE *yyin;
extern int errorlineno;
extern int pedantic;
extern int quiet;

/* For getopt(3) */
extern char *optarg;
extern int optind;

/*
 * Globals
 */
char **Argv;
struct sudo_user sudo_user;
int Argc, parse_error = FALSE;
static struct sudoersfile sudoers;

int
main(argc, argv)
    int argc;
    char **argv;
{
    char *args, *editor;
    int ch, checkonly, n, oldperms;

    /* Initialize sudoers struct. */
    sudoers.path = _PATH_SUDOERS;
    sudoers.tpath = _PATH_SUDOERS_TMP;
    sudoers.fd = -1;

    /* Warn about aliases that are used before being defined. */
    pedantic = 1;

    Argv = argv;
    if ((Argc = argc) < 1)
	usage();

    /*
     * Arg handling.
     */
    checkonly = oldperms = FALSE;
    while ((ch = getopt(argc, argv, "Vcf:sq")) != -1) {
	switch (ch) {
	    case 'V':
		(void) printf("%s version %s\n", getprogname(), version);
		exit(0);
	    case 'c':
		checkonly++;		/* check mode */
		break;
	    case 'f':			/* sudoers file path */
		sudoers.path = optarg;
		easprintf(&sudoers.tpath, "%s.tmp", optarg);
		oldperms = TRUE;
		break;
	    case 's':
		pedantic++;		/* strict mode */
		break;
	    case 'q':
		quiet++;		/* quiet mode */
		break;
	    default:
		usage();
	}
    }
    argc -= optind;
    argv += optind;
    if (argc)
	usage();

    /* Mock up a fake sudo_user struct. */
    user_host = user_shost = user_cmnd = "";
    if ((sudo_user.pw = getpwuid(getuid())) == NULL)
	errx(1, "you don't exist in the passwd database");

    /* Setup defaults data structures. */
    init_defaults();

    if (checkonly)
	exit(check_syntax());

    /*
     * Open and parse the existing sudoers file(s) in quiet mode to highlight
     * any existing errors and to pull in editor and env_editor conf values.
     */  
    if ((yyin = open_sudoers(&sudoers)) == NULL)
	err(1, "%s", sudoers.path);
    n = quiet;
    quiet = 1;
    init_parser();
    yyparse();
    parse_error = FALSE;
    quiet = n;

    /* Edit sudoers, check for parse errors and re-edit on failure. */
    editor = get_editor(&args);
    visudo(&sudoers, editor, args);

    /* Install the new sudoers file. */
    install_sudoers(&sudoers, oldperms);

    exit(0);
}

/*
 * Edit the sudoers file.
 * Returns TRUE on success, else FALSE.
 */
static void
edit_sudoers(sp, editor, args, lineno)
    struct sudoersfile *sp;
    char *editor, *args;
    int lineno;
{
    int ac;				/* argument count */
    char **av;				/* argument vector for run_command */
    char *cp;				/* scratch char pointer */
    char linestr[64];			/* string version of lineno */
    struct timespec ts1, ts2;		/* time before and after edit */
    struct stat sb;			/* stat buffer */

    /* Make timestamp on temp file match original. */
    (void) touch(-1, sp->tpath, &sp->orig_mtim);

    /* Find the length of the argument vector */
    ac = 3 + (lineno > 0);
    if (args) {
        int wasblank;

        ac++;
        for (wasblank = FALSE, cp = args; *cp; cp++) {
            if (isblank((unsigned char) *cp))
                wasblank = TRUE;
            else if (wasblank) {
                wasblank = FALSE;
                ac++;
            }
        }
    }

    /* Build up argument vector for the command */
    av = emalloc2(ac, sizeof(char *));
    if ((av[0] = strrchr(editor, '/')) != NULL)
	av[0]++;
    else
	av[0] = editor;
    ac = 1;
    if (lineno > 0) {
	(void) snprintf(linestr, sizeof(linestr), "+%d", lineno);
	av[ac++] = linestr;
    }
    if (args) {
	for ((cp = strtok(args, " \t")); cp; (cp = strtok(NULL, " \t")))
	    av[ac++] = cp;
    }
    av[ac++] = sp->tpath;
    av[ac++] = NULL;

    /*
     * Do the edit:
     *  We cannot check the editor's exit value against 0 since
     *  XPG4 specifies that vi's exit value is a function of the
     *  number of errors during editing (?!?!).
     */
    gettime(&ts1);
    if (run_command(editor, av) != -1) {
	gettime(&ts2);
	/*
	 * Sanity checks.
	 */
	if (stat(sp->tpath, &sb) < 0) {
	    warnx("cannot stat temporary file (%s), %s unchanged",
		sp->tpath, sp->path);
	    Exit(-1);
	}
	if (sb.st_size == 0) {
	    warnx("zero length temporary file (%s), %s unchanged",
		sp->tpath, sp->path);
	    Exit(-1);
	}
    } else {
	warnx("editor (%s) failed, %s unchanged", editor, sp->path);
	Exit(-1);
    }

    /* Check to see if the user changed the file. */
    if (sp->orig_size == sb.st_size &&
	sp->orig_mtim.tv_sec == mtim_getsec(sb) &&
	sp->orig_mtim.tv_nsec == mtim_getnsec(sb)) {
	/*
	 * If mtime and size match but the user spent no measurable
	 * time in the editor we can't tell if the file was changed.
	 */
#ifdef HAVE_TIMESPECSUB2
	timespecsub(&ts1, &ts2);
#else
	timespecsub(&ts1, &ts2, &ts2);
#endif
	if (timespecisset(&ts2)) {
	    warnx("%s unchanged", sp->tpath);
	    Exit(0);
	}
    }
}

/*
 * Parse sudoers after editing and re-edit any ones that caused a parse error.
 * Returns TRUE on success, else FALSE.
 */
static void
visudo(sp, editor, args)
    struct sudoersfile *sp;
    char *editor, *args;
{
    int ch;

    /*
     * Parse the edited sudoers file and do sanity checking
     */
    do {
	edit_sudoers(sp, editor, args, errorlineno);

	yyin = fopen(sp->tpath, "r+");
	if (yyin == NULL) {
	    warnx("can't re-open temporary file (%s), %s unchanged.",
		sp->tpath, sp->path);
	    Exit(-1);
	}

	/* Add missing newline at EOF if needed. */
	if (fseek(yyin, -1, SEEK_END) == 0 && (ch = fgetc(yyin)) != '\n')
	    fputc('\n', yyin);
	rewind(yyin);

	/* Clean slate for each parse */
	user_runas = NULL;
	init_defaults();
	init_parser();

	/* Parse the sudoers temp file */
	yyrestart(yyin);
	if (yyparse() && parse_error != TRUE) {
	    warnx("unabled to parse temporary file (%s), unknown error",
		sp->tpath);
	    parse_error = TRUE;
	}
	fclose(yyin);

	/*
	 * Got an error, prompt the user for what to do now
	 */
	if (parse_error) {
	    switch (whatnow()) {
		case 'Q' :	parse_error = FALSE;	/* ignore parse error */
				break;
		case 'x' :	Exit(0);
				break;
	    }
	}
    } while (parse_error);
}

/*
 * Set the owner and mode on a sudoers temp file and
 * move it into place.  Returns TRUE on success, else FALSE.
 */
static void
install_sudoers(sp, oldperms)
    struct sudoersfile *sp;
    int oldperms;
{
    struct stat sb;

    /*
     * Change mode and ownership of temp file so when
     * we move it to sp->path things are kosher.
     */
    if (oldperms) {
	/* Use perms of the existing file.  */
#ifdef HAVE_FSTAT
	if (fstat(sp->fd, &sb) == -1)
#else
	if (stat(sp->path, &sb) == -1)
#endif
	    err(1, "can't stat %s", sp->path);
	(void) chown(sp->tpath, sb.st_uid, sb.st_gid);
	(void) chmod(sp->tpath, sb.st_mode & 0777);
    } else {
	if (chown(sp->tpath, SUDOERS_UID, SUDOERS_GID) != 0) {
	    warn("unable to set (uid, gid) of %s to (%d, %d)",
		sp->tpath, SUDOERS_UID, SUDOERS_GID);
	    Exit(-1);
	}
	if (chmod(sp->tpath, SUDOERS_MODE) != 0) {
	    warn("unable to change mode of %s to 0%o", sp->tpath, SUDOERS_MODE);
	    Exit(-1);
	}
    }

    /*
     * Now that sp->tpath is sane (parses ok) it needs to be
     * rename(2)'d to sp->path.  If the rename(2) fails we try using
     * mv(1) in case sp->tpath and sp->path are on different file systems.
     */
    if (rename(sp->tpath, sp->path) != 0) {
	if (errno == EXDEV) {
	    char *av[4];
	    warnx("%s and %s not on the same file system, using mv to rename",
	      sp->tpath, sp->path);

	    /* Build up argument vector for the command */
	    if ((av[0] = strrchr(_PATH_MV, '/')) != NULL)
		av[0]++;
	    else
		av[0] = _PATH_MV;
	    av[1] = sp->tpath;
	    av[2] = sp->path;
	    av[3] = NULL;

	    /* And run it... */
	    if (run_command(_PATH_MV, av)) {
		warnx("command failed: '%s %s %s', %s unchanged",
		    _PATH_MV, sp->tpath, sp->path, sp->path);
		Exit(-1);
	    }
	} else {
	    warn("error renaming %s, %s unchanged", sp->tpath, sp->path);
	    Exit(-1);
	}
    }
}

/*
 * Dummy *_matches routines.
 * These exist to allow us to use the same parser as sudo(8).
 */
int
command_matches(path, sudoers_args)
    char *path;
    char *sudoers_args;
{
    return(TRUE);
}

int
addr_matches(n)
    char *n;
{
    return(TRUE);
}

int
hostname_matches(s, l, p)
    char *s, *l, *p;
{
    return(TRUE);
}

int
usergr_matches(g, u, pw)
    char *g, *u;
    struct passwd *pw;
{
    return(TRUE);
}

int
userpw_matches(s, u, pw)
    char *s, *u;
    struct passwd *pw;
{
    return(TRUE);
}

int
netgr_matches(n, h, sh, u)
    char *n, *h, *sh, *u;
{
    return(TRUE);
}

void
set_fqdn()
{
    return;
}

int
set_runaspw(user)
    char *user;
{
    extern int sudolineno, used_runas;

    if (used_runas) {
	(void) fprintf(stderr,
	    "%s: runas_default set after old value is in use near line %d\n",
	    pedantic > 1 ? "Error" : "Warning", sudolineno);
	if (pedantic > 1)
	    yyerror(NULL);
    }
    return(TRUE);
}

int
user_is_exempt()
{
    return(TRUE);
}

void
init_envtables()
{
    return;
}

/*
 * Assuming a parse error occurred, prompt the user for what they want
 * to do now.  Returns the first letter of their choice.
 */
static char
whatnow()
{
    int choice, c;

    for (;;) {
	(void) fputs("What now? ", stdout);
	choice = getchar();
	for (c = choice; c != '\n' && c != EOF;)
	    c = getchar();

	switch (choice) {
	    case EOF:
		choice = 'x';
		/* FALLTHROUGH */
	    case 'e':
	    case 'x':
	    case 'Q':
		return(choice);
	    default:
		(void) puts("Options are:");
		(void) puts("  (e)dit sudoers file again");
		(void) puts("  e(x)it without saving changes to sudoers file");
		(void) puts("  (Q)uit and save changes to sudoers file (DANGER!)\n");
	}
    }
}

/*
 * Install signal handlers for visudo.
 */
static void
setup_signals()
{
	sigaction_t sa;

	/*
	 * Setup signal handlers to cleanup nicely.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = Exit;
	(void) sigaction(SIGTERM, &sa, NULL);
	(void) sigaction(SIGHUP, &sa, NULL);
	(void) sigaction(SIGINT, &sa, NULL);
	(void) sigaction(SIGQUIT, &sa, NULL);
}

static int
run_command(path, argv)
    char *path;
    char **argv;
{
    int status;
    pid_t pid;
    sigset_t set, oset;

    (void) sigemptyset(&set);
    (void) sigaddset(&set, SIGCHLD);
    (void) sigprocmask(SIG_BLOCK, &set, &oset);

    switch (pid = fork()) {
	case -1:
	    warn("unable to run %s", path);
	    Exit(-1);
	    break;	/* NOTREACHED */
	case 0:
	    (void) sigprocmask(SIG_SETMASK, &oset, NULL);
	    endpwent();
	    closefrom(STDERR_FILENO + 1);
	    execv(path, argv);
	    warn("unable to run %s", path);
	    _exit(127);
	    break;	/* NOTREACHED */
    }

#ifdef sudo_waitpid
    pid = sudo_waitpid(pid, &status, 0);
#else
    pid = wait(&status);
#endif

    (void) sigprocmask(SIG_SETMASK, &oset, NULL);

    if (pid == -1 || !WIFEXITED(status))
	return(-1);
    return(WEXITSTATUS(status));
}

static int
check_syntax()
{

    if ((yyin = fopen(sudoers.path, "r")) == NULL) {
	if (!quiet)
	    warn("unable to open %s", sudoers.path);
	exit(1);
    }
    init_parser();
    if (yyparse() && parse_error != TRUE) {
	if (!quiet)
	    warnx("failed to parse %s file, unknown error", sudoers.path);
	parse_error = TRUE;
    }
    if (!quiet){
	if (parse_error)
	    (void) printf("parse error in %s near line %d\n", sudoers.path,
		errorlineno);
	else
	    (void) printf("%s file parsed OK\n", sudoers.path);
    }

    return(parse_error == TRUE);
}

static FILE *
open_sudoers(sp)
    struct sudoersfile *sp;
{
    struct stat sb;
    ssize_t nread;
    FILE *fp;
    char buf[PATH_MAX*2];
    int tfd;

    /* Open and lock sudoers. */
    sp->fd = open(sp->path, O_RDWR | O_CREAT, SUDOERS_MODE);
    if (sp->fd == -1)
	err(1, "%s", sp->path);
    if (!lock_file(sp->fd, SUDO_TLOCK))
	errx(1, "%s busy, try again later", sp->path);
    if ((fp = fdopen(sp->fd, "r")) == NULL)
	err(1, "%s", sp->path);

    /* Stash sudoers size and mtime. */
#ifdef HAVE_FSTAT
    if (fstat(sp->fd, &sb) == -1)
#else
    if (stat(sp->path, &sb) == -1)
#endif
	err(1, "can't stat %s", sp->path);
    sp->orig_size = sb.st_size;
    sp->orig_mtim.tv_sec = mtim_getsec(sb);
    sp->orig_mtim.tv_nsec = mtim_getnsec(sb);

    /* Create the temp file. */
    tfd = open(sp->tpath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (tfd < 0)
	err(1, "%s", sp->tpath);

    /* Install signal handlers to clean up temp file if we are killed. */
    setup_signals();

    /* Copy sp->path -> sp->tpath. */
    if (sp->orig_size != 0) {
	while ((nread = read(sp->fd, buf, sizeof(buf))) > 0)
	    if (write(tfd, buf, nread) != nread) {
		warn("write error");
		Exit(-1);
	    }

	/* Add missing newline at EOF if needed. */
	if (nread > 0 && buf[nread - 1] != '\n') {
	    buf[0] = '\n';
	    write(tfd, buf, 1);
	}
    }
    (void) close(tfd);
    rewind(fp);

    return(fp);
}

static char *
get_editor(args)
    char **args;
{
    char *Editor, *EditorArgs, *EditorPath, *UserEditor, *UserEditorArgs;

    /*
     * Check VISUAL and EDITOR environment variables to see which editor
     * the user wants to use (we may not end up using it though).
     * If the path is not fully-qualified, make it so and check that
     * the specified executable actually exists.
     */
    UserEditorArgs = NULL;
    if ((UserEditor = getenv("VISUAL")) == NULL || *UserEditor == '\0')
	UserEditor = getenv("EDITOR");
    if (UserEditor && *UserEditor == '\0')
	UserEditor = NULL;
    else if (UserEditor) {
	UserEditorArgs = get_args(UserEditor);
	if (find_path(UserEditor, &Editor, NULL, getenv("PATH")) == FOUND) {
	    UserEditor = Editor;
	} else {
	    if (def_env_editor) {
		/* If we are honoring $EDITOR this is a fatal error. */
		warnx("specified editor (%s) doesn't exist!", UserEditor);
		Exit(-1);
	    } else {
		/* Otherwise, just ignore $EDITOR. */
		UserEditor = NULL;
	    }
	}
    }

    /*
     * See if we can use the user's choice of editors either because
     * we allow any $EDITOR or because $EDITOR is in the allowable list.
     */
    Editor = EditorArgs = EditorPath = NULL;
    if (def_env_editor && UserEditor) {
	Editor = UserEditor;
	EditorArgs = UserEditorArgs;
    } else if (UserEditor) {
	struct stat editor_sb;
	struct stat user_editor_sb;
	char *base, *userbase;

	if (stat(UserEditor, &user_editor_sb) != 0) {
	    /* Should never happen since we already checked above. */
	    warn("unable to stat editor (%s)", UserEditor);
	    Exit(-1);
	}
	EditorPath = estrdup(def_editor);
	Editor = strtok(EditorPath, ":");
	do {
	    EditorArgs = get_args(Editor);
	    /*
	     * Both Editor and UserEditor should be fully qualified but
	     * check anyway...
	     */
	    if ((base = strrchr(Editor, '/')) == NULL)
		continue;
	    if ((userbase = strrchr(UserEditor, '/')) == NULL) {
		Editor = NULL;
		break;
	    }
	    base++, userbase++;

	    /*
	     * We compare the basenames first and then use stat to match
	     * for sure.
	     */
	    if (strcmp(base, userbase) == 0) {
		if (stat(Editor, &editor_sb) == 0 && S_ISREG(editor_sb.st_mode)
		    && (editor_sb.st_mode & 0000111) &&
		    editor_sb.st_dev == user_editor_sb.st_dev &&
		    editor_sb.st_ino == user_editor_sb.st_ino)
		    break;
	    }
	} while ((Editor = strtok(NULL, ":")));
    }

    /*
     * Can't use $EDITOR, try each element of def_editor until we
     * find one that exists, is regular, and is executable.
     */
    if (Editor == NULL || *Editor == '\0') {
	efree(EditorPath);
	EditorPath = estrdup(def_editor);
	Editor = strtok(EditorPath, ":");
	do {
	    EditorArgs = get_args(Editor);
	    if (sudo_goodpath(Editor, NULL))
		break;
	} while ((Editor = strtok(NULL, ":")));

	/* Bleah, none of the editors existed! */
	if (Editor == NULL || *Editor == '\0') {
	    warnx("no editor found (editor path = %s)", def_editor);
	    Exit(-1);
	}
    }
    *args = EditorArgs;
    return(Editor);
}

/*
 * Split out any command line arguments and return them.
 */
static char *
get_args(cmnd)
    char *cmnd;
{
    char *args;

    args = cmnd;
    while (*args && !isblank((unsigned char) *args))
	args++;
    if (*args) {
	*args++ = '\0';
	while (*args && isblank((unsigned char) *args))
	    args++;
    }
    return(*args ? args : NULL);
}

/*
 * Unlink the sudoers temp file (if it exists) and exit.
 * Used in place of a normal exit() and as a signal handler.
 * A positive parameter indicates we were called as a signal handler.
 */
static RETSIGTYPE
Exit(sig)
    int sig;
{
#define	emsg	 " exiting due to signal.\n"

    (void) unlink(sudoers.tpath);

    if (sig > 0) {
	write(STDERR_FILENO, getprogname(), strlen(getprogname()));
	write(STDERR_FILENO, emsg, sizeof(emsg) - 1);
	_exit(sig);
    }
    exit(-sig);
}

static void
usage()
{
    (void) fprintf(stderr, "usage: %s [-c] [-q] [-s] [-V] [-f sudoers]\n",
	getprogname());
    exit(1);
}
