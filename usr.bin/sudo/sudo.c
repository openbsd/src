/*
 * Copyright (c) 1993-1996,1998-2003 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#ifdef HAVE_SETRLIMIT
# include <sys/time.h>
# include <sys/resource.h>
#endif
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
# if defined(HAVE_MEMORY_H) && !defined(STDC_HEADERS)
#  include <memory.h>
# endif
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
# ifdef __hpux
#  undef MAXINT
#  include <hpsecurity.h>
# else
#  include <sys/security.h>
# endif /* __hpux */
# include <prot.h>
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */
#ifdef HAVE_LOGIN_CAP_H
# include <login_cap.h>
# ifndef LOGIN_DEFROOTCLASS
#  define LOGIN_DEFROOTCLASS	"daemon"
# endif
#endif

#include "sudo.h"
#include "interfaces.h"
#include "version.h"

#ifndef lint
static const char rcsid[] = "$Sudo: sudo.c,v 1.334 2003/04/01 15:02:49 millert Exp $";
#endif /* lint */

/*
 * Prototypes
 */
static int init_vars			__P((int));
static int parse_args			__P((void));
static void check_sudoers		__P((void));
static void initial_setup		__P((void));
static void set_loginclass		__P((struct passwd *));
static void usage			__P((int));
static void usage_excl			__P((int));
static struct passwd *get_authpw	__P((void));
extern void list_matches		__P((void));
extern char **rebuild_env		__P((int, char **));
extern char **zero_env			__P((char **));
extern struct passwd *sudo_getpwnam	__P((const char *));
extern struct passwd *sudo_getpwuid	__P((uid_t));

/*
 * Globals
 */
int Argc;
char **Argv;
int NewArgc = 0;
char **NewArgv = NULL;
struct sudo_user sudo_user;
struct passwd *auth_pw;
FILE *sudoers_fp = NULL;
struct interface *interfaces;
int num_interfaces;
int tgetpass_flags;
uid_t timestamp_uid;
extern int errorlineno;
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
static struct rlimit corelimit;
#endif /* RLIMIT_CORE */
#ifdef HAVE_LOGIN_CAP_H
login_cap_t *lc;
#endif /* HAVE_LOGIN_CAP_H */
#ifdef HAVE_BSD_AUTH_H
char *login_style;
#endif /* HAVE_BSD_AUTH_H */
void (*set_perms) __P((int));


int
main(argc, argv, envp)
    int argc;
    char **argv;
    char **envp;
{
    int validated;
    int fd;
    int cmnd_status;
    int sudo_mode;
    int pwflag;
    char **new_environ;
    sigaction_t sa, saved_sa_int, saved_sa_quit, saved_sa_tstp, saved_sa_chld;
    extern int printmatches;
    extern char **environ;

    /* Must be done as the first thing... */
#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
    (void) set_auth_parameters(argc, argv);
# ifdef HAVE_INITPRIVS
    initprivs();
# endif
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */

    /* Zero out the environment. */
    environ = zero_env(envp);

    Argv = argv;
    Argc = argc;

    if (geteuid() != 0) {
	(void) fprintf(stderr, "Sorry, %s must be setuid root.\n", Argv[0]);
	exit(1);
    }

    /*
     * Signal setup:
     *	Ignore keyboard-generated signals so the user cannot interrupt
     *  us at some point and avoid the logging.
     *  Install handler to wait for children when they exit.
     */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    (void) sigaction(SIGINT, &sa, &saved_sa_int);
    (void) sigaction(SIGQUIT, &sa, &saved_sa_quit);
    (void) sigaction(SIGTSTP, &sa, &saved_sa_tstp);
    sa.sa_handler = reapchild;
    (void) sigaction(SIGCHLD, &sa, &saved_sa_chld);

    /*
     * Turn off core dumps, close open files and setup set_perms().
     */
    initial_setup();
    setpwent();

    /* Parse our arguments. */
    sudo_mode = parse_args();

    /* Setup defaults data structures. */
    init_defaults();

    /* Load the list of local ip addresses and netmasks.  */
    load_interfaces();

    pwflag = 0;
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
		    dump_interfaces();
		}
		exit(0);
		break;
	    case MODE_HELP:
		usage(0);
		break;
	    case MODE_VALIDATE:
		user_cmnd = "validate";
		pwflag = I_VERIFYPW_I;
		break;
	    case MODE_KILL:
	    case MODE_INVALIDATE:
		user_cmnd = "kill";
		pwflag = -1;
		break;
	    case MODE_LISTDEFS:
		list_options();
		exit(0);
		break;
	    case MODE_LIST:
		user_cmnd = "list";
		pwflag = I_LISTPW_I;
		printmatches = 1;
		break;
	}

    /* Must have a command to run... */
    if (user_cmnd == NULL && NewArgc == 0)
	usage(1);

    cmnd_status = init_vars(sudo_mode);

    check_sudoers();	/* check mode/owner on _PATH_SUDOERS */

    /* Validate the user but don't search for pseudo-commands. */
    validated = sudoers_lookup(pwflag);

    /*
     * If we are using set_perms_posix() and the stay_setuid flag was not set,
     * set the real, effective and saved uids to 0 and use set_perms_nosuid()
     * instead of set_perms_posix().
     */
#if !defined(HAVE_SETRESUID) && !defined(HAVE_SETREUID) && \
    !defined(NO_SAVED_IDS) && defined(_SC_SAVED_IDS) && defined(_SC_VERSION)
    if (!def_flag(I_STAY_SETUID) && set_perms == set_perms_posix) {
	if (setuid(0)) {
	    perror("setuid(0)");
	    exit(1);
	}
	set_perms = set_perms_nosuid;
    }
#endif

    /*
     * Look up runas user passwd struct.  If we are given a uid then
     * there may be no corresponding passwd(5) entry (which is OK).
     */
    if (**user_runas == '#') {
	runas_pw = sudo_getpwuid(atoi(*user_runas + 1));
	if (runas_pw == NULL) {
	    runas_pw = emalloc(sizeof(struct passwd));
	    (void) memset((VOID *)runas_pw, 0, sizeof(struct passwd));
	    runas_pw->pw_uid = atoi(*user_runas + 1);
	}
    } else {
	runas_pw = sudo_getpwnam(*user_runas);
	if (runas_pw == NULL)
	    log_error(NO_MAIL|MSG_ONLY, "no passwd entry for %s!", *user_runas);
    }

    /*
     * Look up the timestamp dir owner if one is specified.
     */
    if (def_str(I_TIMESTAMPOWNER)) {
	struct passwd *pw;

	if (*def_str(I_TIMESTAMPOWNER) == '#')
	    pw = getpwuid(atoi(def_str(I_TIMESTAMPOWNER) + 1));
	else
	    pw = getpwnam(def_str(I_TIMESTAMPOWNER));
	if (!pw)
	    log_error(0, "timestamp owner (%s): No such user",
		def_str(I_TIMESTAMPOWNER));
	timestamp_uid = pw->pw_uid;
    }

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
	(void) fprintf(stderr,
	    "Sorry, %s has been configured to not allow root to run it.\n",
	    Argv[0]);
	exit(1);
    }

    /* If given the -P option, set the "preserve_groups" flag. */
    if (sudo_mode & MODE_PRESERVE_GROUPS)
	def_flag(I_PRESERVE_GROUPS) = TRUE;

    /* If no command line args and "set_home" is not set, error out. */
    if ((sudo_mode & MODE_IMPLIED_SHELL) && !def_flag(I_SHELL_NOARGS))
	usage(1);

    /* May need to set $HOME to target user if we are running a command. */
    if ((sudo_mode & MODE_RUN) && (def_flag(I_ALWAYS_SET_HOME) ||
	((sudo_mode & MODE_SHELL) && def_flag(I_SET_HOME))))
	sudo_mode |= MODE_RESET_HOME;

    /* Bail if a tty is required and we don't have one.  */
    if (def_flag(I_REQUIRETTY)) {
	if ((fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1)
	    log_error(NO_MAIL, "sorry, you must have a tty to run sudo");
	else
	    (void) close(fd);
    }

    /* Fill in passwd struct based on user we are authenticating as.  */
    auth_pw = get_authpw();

    /* Require a password unless the NOPASS tag was set.  */
    if (!(validated & FLAG_NOPASS))
	check_user();

    /* Build up custom environment that avoids any nasty bits. */
    new_environ = rebuild_env(sudo_mode, envp);

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

	/* This *must* have been set if we got a match but... */
	if (safe_cmnd == NULL) {
	    log_error(MSG_ONLY,
		"internal error, safe_cmnd never got set for %s; %s",
		user_cmnd,
		"please report this error at http://courtesan.com/sudo/bugs/");
	}

	/* Override user's umask if configured to do so. */
	if (def_ival(I_UMASK) != 0777)
	    (void) umask(def_mode(I_UMASK));

	/* Restore coredumpsize resource limit. */
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
	(void) setrlimit(RLIMIT_CORE, &corelimit);
#endif /* RLIMIT_CORE */

	/* Become specified user or root. */
	set_perms(PERM_RUNAS);

	/* Close the password and group files */
	endpwent();
	endgrent();

	/* Install the new environment. */
	environ = new_environ;

	/* Restore signal handlers before we exec. */
	(void) sigaction(SIGINT, &saved_sa_int, NULL);
	(void) sigaction(SIGQUIT, &saved_sa_quit, NULL);
	(void) sigaction(SIGTSTP, &saved_sa_tstp, NULL);
	(void) sigaction(SIGCHLD, &saved_sa_chld, NULL);

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
	exit(127);
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
    int nohostname, rval;

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
    (void) strlcpy(user_cwd, "unknown", sizeof(user_cwd));

    /*
     * We avoid gethostbyname() if possible since we don't want
     * sudo to block if DNS or NIS is hosed.
     * "host" is the (possibly fully-qualified) hostname and
     * "shost" is the unqualified form of the hostname.
     */
    nohostname = gethostname(thost, sizeof(thost));
    if (nohostname)
	user_host = user_shost = "localhost";
    else {
	user_host = estrdup(thost);
	if (def_flag(I_FQDN)) {
	    /* Defer call to set_fqdn() until log_error() is safe. */
	    user_shost = user_host;
	} else {
	    if ((p = strchr(user_host, '.'))) {
		*p = '\0';
		user_shost = estrdup(user_host);
		*p = '.';
	    } else {
		user_shost = user_host;
	    }
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
	(void) snprintf(pw_name, sizeof(pw_name), "%lu",
	    (unsigned long) pw.pw_uid);
	pw.pw_name = pw_name;
	sudo_user.pw = &pw;

	log_error(0, "uid %lu does not exist in the passwd file!",
	    (unsigned long) pw.pw_uid);
    }
    if (user_shell == NULL || *user_shell == '\0')
	user_shell = sudo_user.pw->pw_shell;

    /* It is now safe to use log_error() and set_perms() */

    /*
     * Must defer set_fqdn() until it is safe to call log_error()
     */
    if (def_flag(I_FQDN))
	set_fqdn();

    if (nohostname)
	log_error(USE_ERRNO|MSG_ONLY, "can't get hostname");

    /*
     * Get current working directory.  Try as user, fall back to root.
     */
    set_perms(PERM_USER);
    if (!getcwd(user_cwd, sizeof(user_cwd))) {
	set_perms(PERM_ROOT);
	if (!getcwd(user_cwd, sizeof(user_cwd))) {
	    (void) fprintf(stderr, "%s: Can't get working directory!\n",
			   Argv[0]);
	    (void) strlcpy(user_cwd, "unknown", sizeof(user_cwd));
	}
    } else
	set_perms(PERM_ROOT);

    /*
     * If we were given the '-s' option (run shell) we need to redo
     * NewArgv and NewArgc.
     */
    if ((sudo_mode & MODE_SHELL)) {
	char **dst, **src = NewArgv;

	NewArgv = (char **) emalloc2((++NewArgc + 1), sizeof(char *));
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

    /* Set login class if applicable. */
    set_loginclass(sudo_user.pw);

    /* Resolve the path and return. */
    if ((sudo_mode & MODE_RUN)) {
	/* XXX - should call this as runas user, not root. */
	rval = find_path(NewArgv[0], &user_cmnd, user_path);
	if (rval != FOUND) {
	    /* Failed as root, try as invoking user. */
	    set_perms(PERM_USER);
	    rval = find_path(NewArgv[0], &user_cmnd, user_path);
	    set_perms(PERM_ROOT);
	}

	/* set user_args */
	if (NewArgc > 1) {
	    char *to, **from;
	    size_t size, n;

	    /* If MODE_SHELL not set then NewArgv is contiguous so just count */
	    if (!(sudo_mode & MODE_SHELL)) {
		size = (size_t) (NewArgv[NewArgc-1] - NewArgv[1]) +
			strlen(NewArgv[NewArgc-1]) + 1;
	    } else {
		for (size = 0, from = NewArgv + 1; *from; from++)
		    size += strlen(*from) + 1;
	    }

	    /* alloc and copy. */
	    user_args = (char *) emalloc(size);
	    for (to = user_args, from = NewArgv + 1; *from; from++) {
		n = strlcpy(to, *from, size - (to - user_args));
		if (n >= size - (to - user_args)) {
		    (void) fprintf(stderr,
			"%s: internal error, init_vars() overflow\n", Argv[0]);
		    exit(1);
		}
		to += n;
		*to++ = ' ';
	    }
	    *--to = '\0';
	}
    } else
	rval = FOUND;

    return(rval);
}

/*
 * Command line argument parsing, can't use getopt(3).
 */
static int
parse_args()
{
    int rval = MODE_RUN;		/* what mode is sudo to be run in? */
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
#ifdef HAVE_BSD_AUTH_H
	    case 'a':
		/* Must have an associated authentication style. */
		if (NewArgv[1] == NULL)
		    usage(1);

		login_style = NewArgv[1];

		/* Shift Argv over and adjust Argc. */
		NewArgc--;
		NewArgv++;
		break;
#endif
#ifdef HAVE_LOGIN_CAP_H
	    case 'c':
		/* Must have an associated login class. */
		if (NewArgv[1] == NULL)
		    usage(1);

		login_class = NewArgv[1];
		def_flag(I_USE_LOGINCLASS) = TRUE;

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
	    case 'P':
		rval |= MODE_PRESERVE_GROUPS;
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
    if ((rootstat = stat_sudoers(_PATH_SUDOERS, &statbuf)) == 0 &&
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
    set_perms(PERM_SUDOERS);

    if (rootstat != 0 && stat_sudoers(_PATH_SUDOERS, &statbuf) != 0)
	log_error(USE_ERRNO, "can't stat %s", _PATH_SUDOERS);
    else if (!S_ISREG(statbuf.st_mode))
	log_error(0, "%s is not a regular file", _PATH_SUDOERS);
    else if (statbuf.st_size == 0)
	log_error(0, "%s is zero length", _PATH_SUDOERS);
    else if ((statbuf.st_mode & 07777) != SUDOERS_MODE)
	log_error(0, "%s is mode 0%o, should be 0%o", _PATH_SUDOERS,
	    (statbuf.st_mode & 07777), SUDOERS_MODE);
    else if (statbuf.st_uid != SUDOERS_UID)
	log_error(0, "%s is owned by uid %lu, should be %lu", _PATH_SUDOERS,
	    (unsigned long) statbuf.st_uid, SUDOERS_UID);
    else if (statbuf.st_gid != SUDOERS_GID)
	log_error(0, "%s is owned by gid %lu, should be %lu", _PATH_SUDOERS,
	    (unsigned long) statbuf.st_gid, SUDOERS_GID);
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

    set_perms(PERM_ROOT);		/* change back to root */
}

/*
 * Close all open files (except std*) and turn off core dumps.
 * Also sets the set_perms() pointer to the correct function.
 */
static void
initial_setup()
{
    int fd, maxfd;
#ifdef HAVE_SETRLIMIT
    struct rlimit rl;
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

    /*
     * Make set_perms point to the correct function.
     * If we are using setresuid() or setreuid() we only need to set this
     * once.  If we are using POSIX saved uids we will switch to
     * set_perms_nosuid after sudoers has been parsed if the "stay_suid"
     * option is not set.
     */
#if defined(HAVE_SETRESUID) || defined(HAVE_SETREUID)
    set_perms = set_perms_suid;
#else
# if !defined(NO_SAVED_IDS) && defined(_SC_SAVED_IDS) && defined(_SC_VERSION)
    if (sysconf(_SC_SAVED_IDS) == 1 && sysconf(_SC_VERSION) >= 199009)
	set_perms = set_perms_posix;
    else
# endif
	set_perms = set_perms_nosuid;
#endif /* HAVE_SETRESUID || HAVE_SETREUID */
}

#ifdef HAVE_LOGIN_CAP_H
static void
set_loginclass(pw)
    struct passwd *pw;
{
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
	if (!lc)
	    lc = login_getclass(NULL);	/* needed for login_getstyle() later */
    }
}
#else
static void
set_loginclass(pw)
    struct passwd *pw;
{
}
#endif /* HAVE_LOGIN_CAP_H */

/*
 * Look up the fully qualified domain name and set user_host and user_shost.
 */
void
set_fqdn()
{
    struct hostent *hp;
    char *p;

    if (!(hp = gethostbyname(user_host))) {
	log_error(MSG_ONLY|NO_EXIT,
	    "unable to lookup %s via gethostbyname()", user_host);
    } else {
	if (user_shost != user_host)
	    free(user_shost);
	free(user_host);
	user_host = estrdup(hp->h_name);
    }
    if ((p = strchr(user_host, '.'))) {
	*p = '\0';
	user_shost = estrdup(user_host);
	*p = '.';
    } else {
	user_shost = user_host;
    }
}

/*
 * Get passwd entry for the user we are going to authenticate as.
 * By default, this is the user invoking sudo...
 */
static struct passwd *
get_authpw()
{
    struct passwd *pw;

    if (def_ival(I_ROOTPW)) {
	if ((pw = sudo_getpwuid(0)) == NULL)
	    log_error(0, "uid 0 does not exist in the passwd file!");
    } else if (def_ival(I_RUNASPW)) {
	if ((pw = sudo_getpwnam(def_str(I_RUNAS_DEFAULT))) == NULL)
	    log_error(0, "user %s does not exist in the passwd file!",
		def_str(I_RUNAS_DEFAULT));
    } else if (def_ival(I_TARGETPW)) {
	if (**user_runas == '#') {
	    if ((pw = sudo_getpwuid(atoi(*user_runas + 1))) == NULL)
		log_error(0, "uid %s does not exist in the passwd file!",
		    user_runas);
	} else {
	    if ((pw = sudo_getpwnam(*user_runas)) == NULL)
		log_error(0, "user %s does not exist in the passwd file!",
		    user_runas);
	}
    } else
	pw = sudo_user.pw;

    return(pw);
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

    (void) fprintf(stderr, "usage: sudo -V | -h | -L | -l | -v | -k | -K | %s",
	"[-H] [-P] [-S] [-b] [-p prompt]\n            [-u username/#uid] ");
#ifdef HAVE_LOGIN_CAP_H
    (void) fprintf(stderr, "[-c class] ");
#endif
#ifdef HAVE_BSD_AUTH_H
    (void) fprintf(stderr, "[-a auth_type] ");
#endif
    (void) fprintf(stderr, "-s | <command>\n");
    exit(exit_val);
}
