/*
 * Copyright (c) 1994-1996,1998-2000 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * For a brief history of sudo, please see the HISTORY file included
 * with this distribution.
 */

#define _SUDO_SUDO_C

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef HAVE_SETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif
#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
# ifdef __hpux
#  undef MAXINT
#  include <hpsecurity.h>
# else
#  include <sys/security.h>
# endif /* __hpux */
# include <prot.h>
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */
#ifdef HAVE_LOGINCAP
# include <login_cap.h>
# ifndef LOGIN_DEFROOTCLASS
#  define LOGIN_DEFROOTCLASS	"daemon"
# endif
#endif

#include "sudo.h"
#include "interfaces.h"
#include "version.h"

#ifndef STDC_HEADERS
extern char *getenv	__P((char *));
#endif /* STDC_HEADERS */

#ifndef lint
static const char rcsid[] = "$Sudo: sudo.c,v 1.278 2000/03/24 20:13:12 millert Exp $";
#endif /* lint */

/*
 * Local type declarations
 */
struct env_table {
    char *name;
    int len;
};

/*
 * Prototypes
 */
static int  parse_args			__P((void));
static void usage			__P((int));
static void usage_excl			__P((int));
static void check_sudoers		__P((void));
static int init_vars			__P((int));
static int set_loginclass		__P((struct passwd *));
static void add_env			__P((int));
static void clean_env			__P((char **, struct env_table *));
static void initial_setup		__P((void));
static void update_epasswd		__P((void));
extern struct passwd *sudo_getpwuid	__P((uid_t));
extern void list_matches		__P((void));

/*
 * Globals
 */
int Argc;
char **Argv;
int NewArgc = 0;
char **NewArgv = NULL;
struct sudo_user sudo_user;
FILE *sudoers_fp = NULL;
struct interface *interfaces;
int num_interfaces;
int tgetpass_flags;
extern int errorlineno;
static char *runas_homedir = NULL;	/* XXX */
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
static struct rlimit corelimit;
#endif /* RLIMIT_CORE */

/*
 * Table of "bad" envariables to remove and len for strncmp()
 */
static struct env_table badenv_table[] = {
    { "IFS=", 4 },
    { "LOCALDOMAIN=", 12 },
    { "RES_OPTIONS=", 12 },
    { "HOSTALIASES=", 12 },
    { "LD_", 3 },
    { "_RLD", 4 },
#ifdef __hpux
    { "SHLIB_PATH=", 11 },
#endif /* __hpux */
#ifdef _AIX
    { "LIBPATH=", 8 },
#endif /* _AIX */
#ifdef HAVE_KERB4
    { "KRB_CONF", 8 },
#endif /* HAVE_KERB4 */
#ifdef HAVE_KERB5
    { "KRB5_CONFIG", 11 },
#endif /* HAVE_KERB5 */
    { "ENV=", 4 },
    { "BASH_ENV=", 9 },
    { (char *) NULL, 0 }
};


int
main(argc, argv)
    int argc;
    char **argv;
{
    int validated;
    int fd;
    int cmnd_status;
    int sudo_mode;
    int sudoers_flags;
#ifdef POSIX_SIGNALS
    sigset_t set, oset;
#else
    int omask;
#endif /* POSIX_SIGNALS */
    extern char **environ;
    extern int printmatches;

    /* Must be done as the first thing... */
#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
    (void) set_auth_parameters(argc, argv);
# ifdef HAVE_INITPRIVS
    initprivs();
# endif
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */

    Argv = argv;
    Argc = argc;

    if (geteuid() != 0) {
	(void) fprintf(stderr, "Sorry, %s must be setuid root.\n", Argv[0]);
	exit(1);
    }

    /*
     * Block signals so the user cannot interrupt us at some point and
     * avoid the logging.
     */
#ifdef POSIX_SIGNALS
    (void) sigemptyset(&set);
    (void) sigaddset(&set, SIGINT);
    (void) sigaddset(&set, SIGQUIT);
    (void) sigaddset(&set, SIGTSTP);
    (void) sigprocmask(SIG_BLOCK, &set, &oset);
#else
    omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGTSTP));
#endif /* POSIX_SIGNALS */

    /*
     * Setup signal handlers, turn off core dumps, and close open files.
     */
    initial_setup();

    /*
     * Set the prompt based on $SUDO_PROMPT (can be overridden by `-p')
     */
    user_prompt = getenv("SUDO_PROMPT");

    /* Parse our arguments. */
    sudo_mode = parse_args();

    /* Setup defaults data structures. */
    init_defaults();

    sudoers_flags = 0;
    if (sudo_mode & MODE_SHELL)
	user_cmnd = "shell";
    else
	switch (sudo_mode) {
	    case MODE_VERSION:
		(void) printf("Sudo version %s\n", version);
		if (getuid() == 0) {
		    putchar('\n');
		    dump_auth_methods();
		    dump_defaults();
		}
		exit(0);
		break;
	    case MODE_HELP:
		usage(0);
		break;
	    case MODE_VALIDATE:
		user_cmnd = "validate";
		sudoers_flags = def_ival(I_VERIFYPW);
		break;
	    case MODE_KILL:
	    case MODE_INVALIDATE:
		user_cmnd = "kill";
		sudoers_flags = PWCHECK_NEVER;
		break;
	    case MODE_LISTDEFS:
		list_options();
		exit(0);
		break;
	    case MODE_LIST:
		user_cmnd = "list";
		printmatches = 1;
		sudoers_flags = def_ival(I_LISTPW);
		break;
	}

    /* Must have a command to run... */
    if (user_cmnd == NULL && NewArgc == 0)
	usage(1);

    clean_env(environ, badenv_table);

    cmnd_status = init_vars(sudo_mode);

    /* At this point, ruid == euid == 0 */

    check_sudoers();	/* check mode/owner on _PATH_SUDOERS */

    add_env(!(sudo_mode & MODE_SHELL));	/* add in SUDO_* envariables */

    /* Validate the user but don't search for pseudo-commands. */
    validated = sudoers_lookup(sudoers_flags);

    /* This goes after the sudoers parse since we honor sudoers options. */
    if (sudo_mode == MODE_KILL || sudo_mode == MODE_INVALIDATE) {
	remove_timestamp((sudo_mode == MODE_KILL));
	exit(0);
    }

    if (validated & VALIDATE_ERROR)
	log_error(0, "parse error in %s near line %d", _PATH_SUDOERS,
	    errorlineno);

    /* Is root even allowed to run sudo? */
    if (user_uid == 0 && !def_flag(I_ROOT_SUDO)) {
	(void) fputs("You are already root, you don't need to use sudo.\n",
	    stderr);
	exit(1);
    }

    /* If no command line args and "set_home" is not set, error out. */
    if ((sudo_mode & MODE_IMPLIED_SHELL) && !def_flag(I_SHELL_NOARGS))
	usage(1);

    /* May need to set $HOME to target user. */
    if ((sudo_mode & MODE_SHELL) && def_flag(I_SET_HOME))
	sudo_mode |= MODE_RESET_HOME;

    /* Bail if a tty is required and we don't have one.  */
    if (def_flag(I_REQUIRETTY)) {
	if ((fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1)
	    log_error(NO_MAIL, "sorry, you must have a tty to run sudo");
	else
	    (void) close(fd);
    }

    /* Update encrypted password in user_password if sudoers said to.  */
    update_epasswd();

    /* Require a password unless the NOPASS tag was set.  */
    if (!(validated & FLAG_NOPASS))
	check_user();

    if (validated & VALIDATE_OK) {
	/* Finally tell the user if the command did not exist. */
	if (cmnd_status == NOT_FOUND_DOT) {
	    (void) fprintf(stderr, "%s: ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.\n", Argv[0], user_cmnd, user_cmnd, user_cmnd);
	    exit(1);
	} else if (cmnd_status == NOT_FOUND) {
	    (void) fprintf(stderr, "%s: %s: command not found\n", Argv[0],
		user_cmnd);
	    exit(1);
	}

	log_auth(validated, 1);
	if (sudo_mode == MODE_VALIDATE)
	    exit(0);
	else if (sudo_mode == MODE_LIST) {
	    list_matches();
	    exit(0);
	}

	/* Set $HOME for `sudo -H' */
	if ((sudo_mode & MODE_RESET_HOME) && runas_homedir)
	    (void) sudo_setenv("HOME", runas_homedir);

	/* This *must* have been set if we got a match but... */
	if (safe_cmnd == NULL) {
	    log_error(MSG_ONLY,
		"internal error, cmnd_safe never got set for %s; %s",
		user_cmnd,
		"please report this error at http://courtesan.com/sudo/bugs/");
	}

	if (def_ival(I_LOGFACSTR))
	    closelog();

	/* Reset signal mask before we exec. */
#ifdef POSIX_SIGNALS
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
#else
	(void) sigsetmask(omask);
#endif /* POSIX_SIGNALS */

	/* Override user's umask if configured to do so. */
	if (def_ival(I_UMASK) != 0777)
	    (void) umask(def_mode(I_UMASK));

	/* Replace the PATH envariable with a secure one. */
	if (def_str(I_SECURE_PATH) && !user_is_exempt())
	    if (sudo_setenv("PATH", def_str(I_SECURE_PATH))) {
		(void) fprintf(stderr, "%s: cannot allocate memory!\n",
		    Argv[0]);
		exit(1);
	    }

	/* Restore coredumpsize resource limit. */
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
	(void) setrlimit(RLIMIT_CORE, &corelimit);
#endif /* RLIMIT_CORE */

	/* Become specified user or root. */
	set_perms(PERM_RUNAS, sudo_mode);

#ifndef PROFILING
	if ((sudo_mode & MODE_BACKGROUND) && fork() > 0)
	    exit(0);
	else
	    EXEC(safe_cmnd, NewArgv);	/* run the command */
#else
	exit(0);
#endif /* PROFILING */
	/*
	 * If we got here then the exec() failed...
	 */
	(void) fprintf(stderr, "%s: unable to exec %s: %s\n",
	    Argv[0], safe_cmnd, strerror(errno));
	exit(-1);
    } else if ((validated & FLAG_NO_USER) || (validated & FLAG_NO_HOST)) {
	log_auth(validated, 1);
	exit(1);
    } else if (validated & VALIDATE_NOT_OK) {
	if (def_flag(I_PATH_INFO)) {
	    /*
	     * We'd like to not leak path info at all here, but that can
	     * *really* confuse the users.  To really close the leak we'd
	     * have to say "not allowed to run foo" even when the problem
	     * is just "no foo in path" since the user can trivially set
	     * their path to just contain a single dir.
	     */
	    log_auth(validated,
		!(cmnd_status == NOT_FOUND_DOT || cmnd_status == NOT_FOUND));
	    if (cmnd_status == NOT_FOUND)
		(void) fprintf(stderr, "%s: %s: command not found\n", Argv[0],
		    user_cmnd);
	    else if (cmnd_status == NOT_FOUND_DOT)
		(void) fprintf(stderr, "%s: ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.\n", Argv[0], user_cmnd, user_cmnd, user_cmnd);
	} else {
	    /* Just tell the user they are not allowed to run foo. */
	    log_auth(validated, 1);
	}
	exit(1);
    } else {
	/* should never get here */
	log_auth(validated, 1);
	exit(1);
    }
    exit(0);	/* not reached */
}

/*
 * Initialize timezone, set umask, fill in ``sudo_user'' struct and
 * load the ``interfaces'' array.
 */
static int
init_vars(sudo_mode)
    int sudo_mode;
{
    char *p, thost[MAXHOSTNAMELEN];

    /* Sanity check command from user. */
    if (user_cmnd == NULL && strlen(NewArgv[0]) >= MAXPATHLEN) {
	(void) fprintf(stderr, "%s: %s: Pathname too long\n", Argv[0],
	    NewArgv[0]);
	exit(1);
    }

#ifdef HAVE_TZSET
    (void) tzset();		/* set the timezone if applicable */
#endif /* HAVE_TZSET */

    /* Default value for cmnd and cwd, overridden later. */
    if (user_cmnd == NULL)
	user_cmnd = NewArgv[0];
    (void) strcpy(user_cwd, "unknown");

    /*
     * We avoid gethostbyname() if possible since we don't want
     * sudo to block if DNS or NIS is hosed.
     * "host" is the (possibly fully-qualified) hostname and
     * "shost" is the unqualified form of the hostname.
     */
    if ((gethostname(thost, sizeof(thost)))) {
	user_host = "localhost";
	log_error(USE_ERRNO|MSG_ONLY, "can't get hostname");
    } else
	user_host = estrdup(thost);
    if (def_flag(I_FQDN))
	set_fqdn();
    else {
	if ((p = strchr(user_host, '.'))) {
	    *p = '\0';
	    user_shost = estrdup(user_host);
	    *p = '.';
	} else {
	    user_shost = user_host;
	}
    }

    if ((p = ttyname(STDIN_FILENO)) || (p = ttyname(STDOUT_FILENO))) {
	if (strncmp(p, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
	    p += sizeof(_PATH_DEV) - 1;
	user_tty = estrdup(p);
    } else
	user_tty = "unknown";

    /*
     * Get a local copy of the user's struct passwd with the shadow password
     * if necessary.  It is assumed that euid is 0 at this point so we
     * can read the shadow passwd file if necessary.
     */
    if ((sudo_user.pw = sudo_getpwuid(getuid())) == NULL) {
	/* Need to make a fake struct passwd for logging to work. */
	struct passwd pw;
	char pw_name[MAX_UID_T_LEN + 1];

	pw.pw_uid = getuid();
	(void) sprintf(pw_name, "%ld", (long) pw.pw_uid);
	pw.pw_name = pw_name;
	sudo_user.pw = &pw;

	log_error(0, "uid %ld does not exist in the passwd file!",
	    (long) pw.pw_uid);
    }

    /* It is now safe to use log_error() and set_perms() */

    /*
     * Get current working directory.  Try as user, fall back to root.
     */
    set_perms(PERM_USER, sudo_mode);
    if (!getcwd(user_cwd, sizeof(user_cwd))) {
	set_perms(PERM_ROOT, sudo_mode);
	if (!getcwd(user_cwd, sizeof(user_cwd))) {
	    (void) fprintf(stderr, "%s: Can't get working directory!\n",
			   Argv[0]);
	    (void) strcpy(user_cwd, "unknown");
	}
    } else
	set_perms(PERM_ROOT, sudo_mode);

    /*
     * Load the list of local ip addresses and netmasks into
     * the interfaces array.
     */
    load_interfaces();

    /*
     * If we were given the '-s' option (run shell) we need to redo
     * NewArgv and NewArgc.
     */
    if ((sudo_mode & MODE_SHELL)) {
	char **dst, **src = NewArgv;

	NewArgv = (char **) emalloc (sizeof(char *) * (++NewArgc + 1));
	if (user_shell && *user_shell) {
	    NewArgv[0] = user_shell;
	} else {
	    (void) fprintf(stderr, "%s: Unable to determine shell.", Argv[0]);
	    exit(1);
	}

	/* copy the args from Argv */
	for (dst = NewArgv + 1; (*dst = *src) != NULL; ++src, ++dst)
	    ;
    }

    /* Resolve the path and return. */
    if ((sudo_mode & MODE_RUN))
	return(find_path(NewArgv[0], &user_cmnd));
    else
	return(FOUND);
}

/*
 * Command line argument parsing, can't use getopt(3).
 */
static int
parse_args()
{
    int rval = MODE_RUN;		/* what mode is suod to be run in? */
    int excl = 0;			/* exclusive arg, no others allowed */

    NewArgv = Argv + 1;
    NewArgc = Argc - 1;

    if (NewArgc == 0) {			/* no options and no command */
	rval |= (MODE_IMPLIED_SHELL | MODE_SHELL);
	return(rval);
    }

    while (NewArgc > 0 && NewArgv[0][0] == '-') {
	if (NewArgv[0][1] != '\0' && NewArgv[0][2] != '\0') {
	    (void) fprintf(stderr, "%s: Please use single character options\n",
		Argv[0]);
	    usage(1);
	}

	switch (NewArgv[0][1]) {
	    case 'p':
		/* Must have an associated prompt. */
		if (NewArgv[1] == NULL)
		    usage(1);

		user_prompt = NewArgv[1];

		/* Shift Argv over and adjust Argc. */
		NewArgc--;
		NewArgv++;
		break;
	    case 'u':
		/* Must have an associated runas user. */
		if (NewArgv[1] == NULL)
		    usage(1);

		user_runas = &NewArgv[1];

		/* Shift Argv over and adjust Argc. */
		NewArgc--;
		NewArgv++;
		break;
#ifdef HAVE_LOGINCAP
	    case 'c':
		/* Must have an associated login class. */
		if (NewArgv[1] == NULL)
		    usage(1);

		login_class = NewArgv[1];
		def_flag(I_LOGINCLASS) = TRUE;

		/* Shift Argv over and adjust Argc. */
		NewArgc--;
		NewArgv++;
		break;
#endif
	    case 'b':
		rval |= MODE_BACKGROUND;
		break;
	    case 'v':
		rval = MODE_VALIDATE;
		if (excl && excl != 'v')
		    usage_excl(1);
		excl = 'v';
		break;
	    case 'k':
		rval = MODE_INVALIDATE;
		if (excl && excl != 'k')
		    usage_excl(1);
		excl = 'k';
		break;
	    case 'K':
		rval = MODE_KILL;
		if (excl && excl != 'K')
		    usage_excl(1);
		excl = 'K';
		break;
	    case 'L':
		rval = MODE_LISTDEFS;
		if (excl && excl != 'L')
		    usage_excl(1);
		excl = 'L';
		break;
	    case 'l':
		rval = MODE_LIST;
		if (excl && excl != 'l')
		    usage_excl(1);
		excl = 'l';
		break;
	    case 'V':
		rval = MODE_VERSION;
		if (excl && excl != 'V')
		    usage_excl(1);
		excl = 'V';
		break;
	    case 'h':
		rval = MODE_HELP;
		if (excl && excl != 'h')
		    usage_excl(1);
		excl = 'h';
		break;
	    case 's':
		rval |= MODE_SHELL;
		if (excl && excl != 's')
		    usage_excl(1);
		excl = 's';
		break;
	    case 'H':
		rval |= MODE_RESET_HOME;
		break;
	    case 'S':
		tgetpass_flags |= TGP_STDIN;
		break;
	    case '-':
		NewArgc--;
		NewArgv++;
		if (rval == MODE_RUN)
		    rval |= (MODE_IMPLIED_SHELL | MODE_SHELL);
		return(rval);
	    case '\0':
		(void) fprintf(stderr, "%s: '-' requires an argument\n",
		    Argv[0]);
		usage(1);
	    default:
		(void) fprintf(stderr, "%s: Illegal option %s\n", Argv[0],
		    NewArgv[0]);
		usage(1);
	}
	NewArgc--;
	NewArgv++;
    }

    if (NewArgc > 0 && !(rval & MODE_RUN))
	usage(1);

    return(rval);
}

/*
 * Add sudo-specific variables into the environment.
 * Sets ``cmnd_args'' as a side effect.
 */
static void
add_env(contiguous)
    int contiguous;
{
    char idstr[MAX_UID_T_LEN + 1];
    size_t size;
    char *buf;

    /* Add the SUDO_COMMAND envariable (cmnd + args). */
    size = strlen(user_cmnd) + 1;
    if (NewArgc > 1) {
	char *to, **from;

	if (contiguous) {
	    size += (size_t) (NewArgv[NewArgc-1] - NewArgv[1]) +
		    strlen(NewArgv[NewArgc-1]) + 1;
	} else {
	    for (from = &NewArgv[1]; *from; from++)
		size += strlen(*from) + 1;
	}

	buf = (char *) emalloc(size);

	/*
	 * Copy the command and it's arguments info buf.
	 */
	(void) strcpy(buf, user_cmnd);
	to = buf + strlen(user_cmnd);
	for (from = &NewArgv[1]; *from; from++) {
	    *to++ = ' ';
	    (void) strcpy(to, *from);
	    to += strlen(*from);
	}
    } else {
	buf = user_cmnd;
    }
    if (sudo_setenv("SUDO_COMMAND", buf)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
    if (NewArgc > 1)
	free(buf);

    /* Grab a pointer to the flat arg string from the environment. */
    if (NewArgc > 1 && (user_args = getenv("SUDO_COMMAND"))) {
	if ((user_args = strchr(user_args, ' ')))
	    user_args++;
	else
	    user_args = NULL;
    }

    /* Add the SUDO_USER environment variable. */
    if (sudo_setenv("SUDO_USER", user_name)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /* Add the SUDO_UID environment variable. */
    (void) sprintf(idstr, "%ld", (long) user_uid);
    if (sudo_setenv("SUDO_UID", idstr)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /* Add the SUDO_GID environment variable. */
    (void) sprintf(idstr, "%ld", (long) user_gid);
    if (sudo_setenv("SUDO_GID", idstr)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /* Set PS1 if SUDO_PS1 is set. */
    if ((buf = getenv("SUDO_PS1")))
	if (sudo_setenv("PS1", buf)) {
	    (void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	    exit(1);
	}
}

/*
 * Sanity check sudoers mode/owner/type.
 * Leaves a file pointer to the sudoers file open in ``fp''.
 */
static void
check_sudoers()
{
    struct stat statbuf;
    int rootstat, i;
    char c;

    /*
     * Fix the mode and group on sudoers file from old default.
     * Only works if filesystem is readable/writable by root.
     */
    if ((rootstat = lstat(_PATH_SUDOERS, &statbuf)) == 0 &&
	SUDOERS_UID == statbuf.st_uid && SUDOERS_MODE != 0400 &&
	(statbuf.st_mode & 0007777) == 0400) {

	if (chmod(_PATH_SUDOERS, SUDOERS_MODE) == 0) {
	    (void) fprintf(stderr, "%s: fixed mode on %s\n",
		Argv[0], _PATH_SUDOERS);
	    statbuf.st_mode |= SUDOERS_MODE;
	    if (statbuf.st_gid != SUDOERS_GID) {
		if (!chown(_PATH_SUDOERS,(uid_t) -1,SUDOERS_GID)) {
		    (void) fprintf(stderr, "%s: set group on %s\n",
			Argv[0], _PATH_SUDOERS);
		    statbuf.st_gid = SUDOERS_GID;
		} else {
		    (void) fprintf(stderr,"%s: Unable to set group on %s: %s\n",
			Argv[0], _PATH_SUDOERS, strerror(errno));
		}
	    }
	} else {
	    (void) fprintf(stderr, "%s: Unable to fix mode on %s: %s\n",
		Argv[0], _PATH_SUDOERS, strerror(errno));
	}
    }

    /*
     * Sanity checks on sudoers file.  Must be done as sudoers
     * file owner.  We already did a stat as root, so use that
     * data if we can't stat as sudoers file owner.
     */
    set_perms(PERM_SUDOERS, 0);

    if (rootstat != 0 && lstat(_PATH_SUDOERS, &statbuf) != 0)
	log_error(USE_ERRNO, "can't stat %s", _PATH_SUDOERS);
    else if (!S_ISREG(statbuf.st_mode))
	log_error(0, "%s is not a regular file", _PATH_SUDOERS);
    else if ((statbuf.st_mode & 07777) != SUDOERS_MODE)
	log_error(0, "%s is mode 0%o, should be 0%o", _PATH_SUDOERS,
	    (statbuf.st_mode & 07777), SUDOERS_MODE);
    else if (statbuf.st_uid != SUDOERS_UID)
	log_error(0, "%s is owned by uid %ld, should be %d", _PATH_SUDOERS,
	    (long) statbuf.st_uid, SUDOERS_UID);
    else if (statbuf.st_gid != SUDOERS_GID)
	log_error(0, "%s is owned by gid %ld, should be %d", _PATH_SUDOERS,
	    (long) statbuf.st_gid, SUDOERS_GID);
    else {
	/* Solaris sometimes returns EAGAIN so try 10 times */
	for (i = 0; i < 10 ; i++) {
	    errno = 0;
	    if ((sudoers_fp = fopen(_PATH_SUDOERS, "r")) == NULL ||
		fread(&c, sizeof(c), 1, sudoers_fp) != 1) {
		sudoers_fp = NULL;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		    break;
	    } else
		break;
	    sleep(1);
	}
	if (sudoers_fp == NULL)
	    log_error(USE_ERRNO, "can't open %s", _PATH_SUDOERS);
    }

    set_perms(PERM_ROOT, 0);		/* change back to root */
}

/*
 * Remove environment variables that match the entries in badenv_table.
 */
static void
clean_env(envp, badenv_table)
    char **envp;
    struct env_table *badenv_table;
{
    struct env_table *bad;
    char **cur;

    /*
     * Remove any envars that match entries in badenv_table.
     */
    for (cur = envp; *cur; cur++) {
	for (bad = badenv_table; bad->name; bad++) {
	    if (strncmp(*cur, bad->name, bad->len) == 0) {
		/* Got a match so remove it. */
		char **move;

		for (move = cur; *move; move++)
		    *move = *(move + 1);

		cur--;

		break;
	    }
	}
    }
}

/*
 * Set real and effective uids and gids based on perm.
 */
void
set_perms(perm, sudo_mode)
    int perm;
    int sudo_mode;
{
    struct passwd *pw;

    /*
     * First, set real & effective uids to root.
     * If perm is PERM_ROOT then we don't need to do anything else.
     */
    if (setuid(0)) {
	perror("setuid(0)");
	exit(1);
    }

    switch (perm) {
	case PERM_USER:
    	    	    	        (void) setgid(user_gid);

    	    	    	        if (seteuid(user_uid)) {
    	    	    	            perror("seteuid(user_uid)");
    	    	    	            exit(1);
    	    	    	        }
			      	break;
				
	case PERM_FULL_USER:
    	    	    	        (void) setgid(user_gid);

				if (setuid(user_uid)) {
				    perror("setuid(user_uid)");
				    exit(1);
				}
			      	break;

	case PERM_RUNAS:
				/* XXX - add group/gid support */
				if (**user_runas == '#') {
				    if (setuid(atoi(*user_runas + 1))) {
					(void) fprintf(stderr,
					    "%s: cannot set uid to %s: %s\n",
					    Argv[0], *user_runas, strerror(errno));
					exit(1);
				    }
				} else {
				    if (!(pw = getpwnam(*user_runas))) {
					(void) fprintf(stderr,
					    "%s: no passwd entry for %s!\n",
					    Argv[0], *user_runas);
					exit(1);
				    }

				    /* Set $USER and $LOGNAME to target user */
				    if (def_flag(I_LOGNAME)) {
					if (sudo_setenv("USER", pw->pw_name)) {
					    (void) fprintf(stderr,
						"%s: cannot allocate memory!\n",
						Argv[0]);
					    exit(1);
					}
					if (sudo_setenv("LOGNAME", pw->pw_name)) {
					    (void) fprintf(stderr,
						"%s: cannot allocate memory!\n",
						Argv[0]);
					    exit(1);
					}
				    }

				    if (def_flag(I_LOGINCLASS)) {
					/*
					 * setusercontext() will set uid/gid/etc
					 * for us so no need to do it below.
					 */
					if (set_loginclass(pw) > 0)
					    break;
				    }

				    if (setgid(pw->pw_gid)) {
					(void) fprintf(stderr,
					    "%s: cannot set gid to %ld: %s\n",
					    Argv[0], (long) pw->pw_gid,
					    strerror(errno));
					exit(1);
				    }
#ifdef HAVE_INITGROUPS
				    /*
				     * Initialize group vector only if are
				     * going to run as a non-root user.
				     */
				    if (strcmp(*user_runas, "root") != 0 &&
					initgroups(*user_runas, pw->pw_gid)
					== -1) {
					(void) fprintf(stderr,
					    "%s: cannot set group vector: %s\n",
					    Argv[0], strerror(errno));
					exit(1);
				    }
#endif /* HAVE_INITGROUPS */
				    if (setuid(pw->pw_uid)) {
					(void) fprintf(stderr,
					    "%s: cannot set uid to %ld: %s\n",
					    Argv[0], (long) pw->pw_uid,
					    strerror(errno));
					exit(1);
				    }
				    if (sudo_mode & MODE_RESET_HOME)
					runas_homedir = pw->pw_dir;
				}
				break;

	case PERM_SUDOERS:
				if (setgid(SUDOERS_GID)) {
				    perror("setgid(SUDOERS_GID)");
				    exit(1);
				}

				/*
				 * If SUDOERS_UID == 0 and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == 0) {
				    if ((SUDOERS_MODE & 040) && seteuid(1)) {
					perror("seteuid(1)");
					exit(1);
				    }
				} else {
				    if (seteuid(SUDOERS_UID)) {
					perror("seteuid(SUDOERS_UID)");
					exit(1);
				    }
				}
			      	break;
    }
}

/*
 * Close all open files (except std*) and turn off core dumps.
 */
static void
initial_setup()
{
    int fd, maxfd;
#ifdef HAVE_SETRLIMIT
    struct rlimit rl;
#endif
#ifdef POSIX_SIGNALS
    struct sigaction sa;
#endif

#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
    /*
     * Turn off core dumps.
     */
    (void) getrlimit(RLIMIT_CORE, &corelimit);
    rl.rlim_cur = rl.rlim_max = 0;
    (void) setrlimit(RLIMIT_CORE, &rl);
#endif /* RLIMIT_CORE */

    /*
     * Close any open fd's other than stdin, stdout and stderr.
     */
#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX) - 1;
#else
    maxfd = getdtablesize() - 1;
#endif /* HAVE_SYSCONF */
#ifdef RLIMIT_NOFILE
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
	if (rl.rlim_max != RLIM_INFINITY && rl.rlim_max <= maxfd)
	    maxfd = rl.rlim_max - 1;
    }
#endif /* RLIMIT_NOFILE */

    for (fd = maxfd; fd > STDERR_FILENO; fd--)
	(void) close(fd);

    /* Catch children as they die... */
#ifdef POSIX_SIGNALS
    (void) memset((VOID *)&sa, 0, sizeof(sa));
    sa.sa_handler = reapchild;
    (void) sigaction(SIGCHLD, &sa, NULL);
#else
    (void) signal(SIGCHLD, reapchild);
#endif /* POSIX_SIGNALS */
}

#ifdef HAVE_LOGINCAP
static int
set_loginclass(pw)
    struct passwd *pw;
{
    login_cap_t *lc;
    int errflags;

    /*
     * Don't make it a fatal error if the user didn't specify the login
     * class themselves.  We do this because if login.conf gets
     * corrupted we want the admin to be able to use sudo to fix it.
     */
    if (login_class)
	errflags = NO_MAIL|MSG_ONLY;
    else
	errflags = NO_MAIL|MSG_ONLY|NO_EXIT;

    if (login_class && strcmp(login_class, "-") != 0) {
	if (strcmp(*user_runas, "root") != 0 && user_uid != 0) {
	    (void) fprintf(stderr, "%s: only root can use -c %s\n",
		Argv[0], login_class);
	    exit(1);
	}
    } else {
	login_class = pw->pw_class;
	if (!login_class || !*login_class)
	    login_class =
		(pw->pw_uid == 0) ? LOGIN_DEFROOTCLASS : LOGIN_DEFCLASS;
    }

    lc = login_getclass(login_class);
    if (!lc || !lc->lc_class || strcmp(lc->lc_class, login_class) != 0) {
	log_error(errflags, "unknown login class: %s", login_class);
	return(0);
    }
    
    /* Set everything except the environment and umask.  */
    if (setusercontext(lc, pw, pw->pw_uid,
	LOGIN_SETUSER|LOGIN_SETGROUP|LOGIN_SETRESOURCES|LOGIN_SETPRIORITY) < 0)
	log_error(NO_MAIL|USE_ERRNO|MSG_ONLY,
	    "setusercontext() failed for login class %s", login_class);

    login_close(lc);
    return(1);
}
#else
static int
set_loginclass(pw)
    struct passwd *pw;
{
    return(0);
}
#endif /* HAVE_LOGINCAP */

/*
 * Look up the fully qualified domain name and set user_host and user_shost.
 */
void
set_fqdn()
{
    struct hostent *hp;
    char *p;

    if (def_flag(I_FQDN)) {
	if (!(hp = gethostbyname(user_host))) {
	    log_error(USE_ERRNO|MSG_ONLY|NO_EXIT,
		"unable to lookup %s via gethostbyname()", user_host);
	} else {
	    free(user_host);
	    user_host = estrdup(hp->h_name);
	}
    }
    if (user_shost != user_host)
	free(user_shost);
    if ((p = strchr(user_host, '.'))) {
	*p = '\0';
	user_shost = estrdup(user_host);
	*p = '.';
    } else {
	user_shost = user_host;
    }
}

/*
 * If the sudoers file says to prompt for a different user's password,
 * update the encrypted password in user_passwd accordingly.
 */
static void
update_epasswd()
{
    struct passwd *pw;

    /* We may be configured to prompt for a password other than the user's */
    if (def_ival(I_ROOTPW)) {
	if ((pw = getpwuid(0)) == NULL)
	    log_error(0, "uid 0 does not exist in the passwd file!");
	free(user_passwd);
	user_passwd = estrdup(sudo_getepw(pw));
    } else if (def_ival(I_RUNASPW)) {
	if ((pw = getpwnam(def_str(I_RUNAS_DEF))) == NULL)
	    log_error(0, "user %s does not exist in the passwd file!",
		def_str(I_RUNAS_DEF));
	free(user_passwd);
	user_passwd = estrdup(sudo_getepw(pw));
    } else if (def_ival(I_TARGETPW)) {
	if (**user_runas == '#') {
	    if ((pw = getpwuid(atoi(*user_runas + 1))) == NULL)
		log_error(0, "uid %s does not exist in the passwd file!",
		    user_runas);
	} else {
	    if ((pw = getpwnam(*user_runas)) == NULL)
		log_error(0, "user %s does not exist in the passwd file!",
		    user_runas);
	}
	free(user_passwd);
	user_passwd = estrdup(sudo_getepw(pw));
    }
}

/*
 * Tell which options are mutually exclusive and exit.
 */
static void
usage_excl(exit_val)
    int exit_val;
{
    (void) fprintf(stderr,
	"Only one of the -h, -k, -K, -l, -s, -v or -V options may be used\n");
    usage(exit_val);
}

/*
 * Give usage message and exit.
 */
static void
usage(exit_val)
    int exit_val;
{
    (void) fprintf(stderr,
	"usage: %s -V | -h | -L | -l | -v | -k | -K | [-H] [-S] [-b]\n%*s",
	Argv[0], (int) strlen(Argv[0]) + 8, " ");
#ifdef HAVE_LOGINCAP
    (void) fprintf(stderr, "[-p prompt] [-u username/#uid] [-c class] -s | <command>\n");
#else
    (void) fprintf(stderr, "[-p prompt] [-u username/#uid] -s | <command>\n");
#endif
    exit(exit_val);
}
