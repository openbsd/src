/*	$OpenBSD: sudo.c,v 1.13 1999/03/29 20:29:06 millert Exp $	*/

/*
 * CU sudo version 1.5.9 (based on Root Group sudo version 1.1)
 * Copyright (c) 1994,1996,1998,1999 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * This software comes with no waranty whatsoever, use at your own risk.
 *
 * Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 */

/*
 *  sudo version 1.1 allows users to execute commands as root
 *  Copyright (C) 1991  The Root Group, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **************************************************************************
 *
 *   sudo.c
 *
 *   This is the main() routine for sudo
 *
 *   sudo is a program to allow users to execute commands 
 *   as root.  The commands are defined in a global network-
 *   wide file and can be distributed.
 *
 *   sudo has been hacked far and wide.  Too many people to
 *   know about.  It's about time to come up with a secure
 *   version that will work well in a network.
 *
 *   This most recent version is done by:
 *
 *              Jeff Nieusma <nieusma@rootgroup.com>
 *              Dave Hieb    <davehieb@rootgroup.com>
 *
 *   However, due to the fact that both of the above are no longer
 *   working at Root Group, I am maintaining the "CU version" of
 *   sudo.
 *		Todd Miller  <Todd.Miller@courtesan.com>
 */

#define MAIN

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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef HAVE_DCE
#include <pthread.h>
#endif /* HAVE_DCE */
#ifdef HAVE_KERB5
#include <krb5.h>
#endif /* HAVE_KERB5 */

#include "sudo.h"
#include "version.h"

#ifndef STDC_HEADERS
extern char *getenv	__P((char *));
#endif /* STDC_HEADERS */

#ifndef lint
static const char rcsid[] = "$Sudo: sudo.c,v 1.222 1999/03/29 04:05:12 millert Exp $";
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
static void load_globals		__P((int));
static int check_sudoers		__P((void));
static int load_cmnd			__P((int));
static void add_env			__P((int));
static void clean_env			__P((char **, struct env_table *));
extern int  user_is_exempt		__P((void));
extern struct passwd *sudo_getpwuid	__P((uid_t));
extern void list_matches		__P((void));

/*
 * Globals
 */
int Argc;
char **Argv;
int NewArgc = 0;
char **NewArgv = NULL;
struct passwd *user_pw_ent;
char *runas_user = RUNAS_DEFAULT;
char *cmnd = NULL;
char *cmnd_args = NULL;
char *tty = "unknown";
char *prompt;
char host[MAXHOSTNAMELEN];
char *shost;
char cwd[MAXPATHLEN];
FILE *sudoers_fp = NULL;
struct stat cmnd_st;
static char *runas_homedir = NULL;
extern struct interface *interfaces;
extern int num_interfaces;
extern int printmatches;
int arg_prompt = 0;	/* was -p used? */
#ifdef HAVE_KERB5
krb5_context sudo_context = NULL;
char *realm = NULL;
int xrealm = 0;
#endif /* HAVE_KERB5 */

/*
 * Table of "bad" envariables to remove and len for strncmp()
 */
struct env_table badenv_table[] = {
    { "IFS=", 4 },
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


/********************************************************************
 *
 *  main()
 *
 *  the driving force behind sudo...
 */

int main(argc, argv)
    int argc;
    char **argv;
{
    int rtn, serrno;
    int cmnd_status = FOUND;
    int sudo_mode = MODE_RUN;
    extern char ** environ;

#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
    (void) set_auth_parameters(argc, argv);
#  ifdef HAVE_INITPRIVS
    initprivs();
#  endif
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */

    Argv = argv;
    Argc = argc;

    if (geteuid() != 0) {
	(void) fprintf(stderr, "Sorry, %s must be setuid root.\n", Argv[0]);
	exit(1);
    }

    /*
     * Close all file descriptors to make sure we have a nice
     * clean slate from which to work.  
     */
#ifdef HAVE_SYSCONF
    for (rtn = sysconf(_SC_OPEN_MAX) - 1; rtn > 2; rtn--)
	(void) close(rtn);
#else
    for (rtn = getdtablesize() - 1; rtn > 2; rtn--)
	(void) close(rtn);
#endif /* HAVE_SYSCONF */

    /*
     * set the prompt based on $SUDO_PROMPT (can be overridden by `-p')
     */
    if ((prompt = getenv("SUDO_PROMPT")) == NULL)
	prompt = PASSPROMPT;

    /*
     * parse our arguments
     */
    sudo_mode = parse_args();
 
    switch (sudo_mode) {
	case MODE_VERSION:
	case MODE_HELP:
	    (void) printf("CU Sudo version %s\n", version);
	    if (sudo_mode == MODE_VERSION)
		exit(0);
	    else
		usage(0);
	    break;
	case MODE_VALIDATE:
	    cmnd = "validate";
	    break;
    	case MODE_KILL:
	    cmnd = "kill";
	    break;
	case MODE_LIST:
	    cmnd = "list";
	    printmatches = 1;
	    break;
    }

    /* must have a command to run unless got -s */
    if (cmnd == NULL && NewArgc == 0 && !(sudo_mode & MODE_SHELL))
	usage(1);

    clean_env(environ, badenv_table);

    load_globals(sudo_mode);	/* load global variables used throughout sudo */

    /*
     * If we got the '-s' option (run shell) we need to redo NewArgv
     * and NewArgc.  This can only be done after load_globals().
     */
    if ((sudo_mode & MODE_SHELL)) {
	char **dst, **src = NewArgv;

	NewArgv = (char **) emalloc (sizeof(char *) * (++NewArgc + 1));

	/* add the shell as argv[0] */
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

    rtn = check_sudoers();	/* check mode/owner on _PATH_SUDO_SUDOERS */
    if (rtn != ALL_SYSTEMS_GO) {
	serrno = errno;
	log_error(rtn);
	set_perms(PERM_FULL_USER, sudo_mode);
	errno = serrno;
	inform_user(rtn);
	exit(1);
    }

#ifdef SECURE_PATH
    /* replace the PATH envariable with a secure one */
    if (!user_is_exempt() && sudo_setenv("PATH", SECURE_PATH)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
#endif /* SECURE_PATH */

    if ((sudo_mode & MODE_RUN)) {
	cmnd_status = load_cmnd(sudo_mode); /* load the cmnd global variable */
    } else if (sudo_mode == MODE_KILL) {
	remove_timestamp();	/* remove the timestamp ticket file */
	exit(0);
    }

    add_env(!(sudo_mode & MODE_SHELL));	/* add in SUDO_* envariables */

    /* validate the user but don't search for pseudo-commands */
    rtn = validate((sudo_mode != MODE_VALIDATE && sudo_mode != MODE_LIST));

    switch (rtn) {

	case VALIDATE_OK:
	    check_user();
	    /* fallthrough */

	case VALIDATE_OK_NOPASS:
	    /* finally tell the user if the command did not exist */
	    if (cmnd_status == NOT_FOUND_DOT) {
		(void) fprintf(stderr, "%s: ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.\n", Argv[0], cmnd, cmnd, cmnd);
		exit(1);
	    } else if (cmnd_status == NOT_FOUND) {
		(void) fprintf(stderr, "%s: %s: command not found\n", Argv[0],
		    cmnd);
		exit(1);
	    }

	    log_error(ALL_SYSTEMS_GO);
	    if (sudo_mode == MODE_VALIDATE)
		exit(0);
	    else if (sudo_mode == MODE_LIST) {
		list_matches();
		exit(0);
	    }

	    /* become specified user or root */
	    set_perms(PERM_RUNAS, sudo_mode);

	    /* set $HOME for `sudo -H' */
	    if ((sudo_mode & MODE_RESET_HOME) && runas_homedir)
		(void) sudo_setenv("HOME", runas_homedir);

#ifndef PROFILING
	    if ((sudo_mode & MODE_BACKGROUND) && fork() > 0) {
		exit(0);
	    } else {
		/*
		 * Make sure we are not being spoofed.  The stat should
		 * be cheap enough to make this almost bulletproof.
		 */
		if (cmnd_st.st_dev) {
		    struct stat st;

		    if (stat(cmnd, &st) < 0) {
			(void) fprintf(stderr, "%s: unable to stat %s: ",
					Argv[0], cmnd);
			perror("");
			exit(1);
		    }

		    if (st.st_dev != cmnd_st.st_dev ||
			st.st_ino != cmnd_st.st_ino) {
			/* log and send mail, then bitch */
			log_error(SPOOF_ATTEMPT);
			inform_user(SPOOF_ATTEMPT);
			exit(1);
		    }
		}
		EXEC(cmnd, NewArgv);	/* run the command */
	    }
#else
	    exit(0);
#endif /* PROFILING */
	    /*
	     * If we got here then the exec() failed...
	     */
	    (void) fprintf(stderr, "%s: ", Argv[0]);
	    perror(cmnd);
	    exit(-1);
	    break;

	case VALIDATE_NOT_OK:
	    check_user();

#ifndef DONT_LEAK_PATH_INFO
	    log_error(rtn);
	    if (cmnd_status == NOT_FOUND_DOT)
		(void) fprintf(stderr, "%s: ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.\n", Argv[0], cmnd, cmnd, cmnd);
	    else if (cmnd_status == NOT_FOUND)
		(void) fprintf(stderr, "%s: %s: command not found\n", Argv[0],
		    cmnd);
	    else
		inform_user(rtn);
	    exit(1);
	    break;
#endif /* DONT_LEAK_PATH_INFO */

	default:
	    log_error(rtn);
	    inform_user(rtn);
	    exit(1);
	    break;
    }
}



/**********************************************************************
 *
 *  load_globals()
 *
 *  This function primes these important global variables:
 *  user_pw_ent, host, cwd, interfaces.
 */

static void load_globals(sudo_mode)
    int sudo_mode;
{
    char *p;
#ifdef FQDN
    struct hostent *h_ent;
#endif /* FQDN */
#ifdef HAVE_KERB5 
    krb5_error_code retval;
    char *lrealm;
#endif /* HAVE_KERB5 */

#ifdef HOST_IN_LOG
    /*
     * Logging routines may use shost so set to a dummy value for now.
     */
    shost = strcpy(host, "localhost");
#endif

    /*
     * Get a local copy of the user's struct passwd with the shadow password
     * if necesary.  It is assumed that euid is 0 at this point so we
     * can read the shadow passwd file if necesary.
     */
    if ((user_pw_ent = sudo_getpwuid(getuid())) == NULL) {
	/* need to make a fake user_pw_ent */
	struct passwd pw;
	char pw_name[MAX_UID_T_LEN + 1];

	/* fill in uid and name fields with the uid */
	pw.pw_uid = getuid();
	(void) sprintf(pw_name, "%ld", (long) pw.pw_uid);
	pw.pw_name = pw_name;
	user_pw_ent = &pw;

	/* complain, log, and die */
	log_error(GLOBAL_NO_PW_ENT);
	inform_user(GLOBAL_NO_PW_ENT);
	exit(1);
    }

#ifdef HAVE_KERB5
    if (retval = krb5_init_context(&sudo_context)) {
	log_error(GLOBAL_KRB5_INIT_ERR);
	inform_user(GLOBAL_KRB5_INIT_ERR);
	exit(1);
    }
    krb5_init_ets(sudo_context);

    if (retval = krb5_get_default_realm(sudo_context, &lrealm)) {
	log_error(GLOBAL_KRB5_INIT_ERR);
	inform_user(GLOBAL_KRB5_INIT_ERR);
	exit(1);
    }

    if (realm) {
	if (strcmp(realm, lrealm) != 0)
	    xrealm = 1; /* User supplied realm is not the system default */
	free(lrealm);
    } else
	realm = lrealm;

    if (!arg_prompt) {
	p = emalloc(strlen(user_name) + strlen(realm) + 17);
	sprintf(p, "Password for %s@%s: ", user_name, realm);
	prompt = p;
    }
#endif /* HAVE_KERB5 */

    /* Set euid == user and ruid == root */
    set_perms(PERM_ROOT, sudo_mode);
    set_perms(PERM_USER, sudo_mode);

#ifdef HAVE_TZSET
    (void) tzset();		/* set the timezone if applicable */
#endif /* HAVE_TZSET */

    /*
     * Need to get tty early since it's used for logging
     */
    if ((p = (char *) ttyname(0)) || (p = (char *) ttyname(1))) {
	if (strncmp(p, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
	    p += sizeof(_PATH_DEV) - 1;
	tty = estrdup(p);
    }

#ifdef SUDO_UMASK
    (void) umask((mode_t)SUDO_UMASK);
#endif /* SUDO_UMASK */

#ifdef NO_ROOT_SUDO
    if (user_uid == 0) {
	(void) fprintf(stderr,
		       "You are already root, you don't need to use sudo.\n");
	exit(1);
    }
#endif

    /*
     * so we know where we are... (do as user)
     */
    if (!getcwd(cwd, sizeof(cwd))) {
	/* try as root... */
	set_perms(PERM_ROOT, sudo_mode);
	if (!getcwd(cwd, sizeof(cwd))) {
	    (void) fprintf(stderr, "%s: Can't get working directory!\n",
			   Argv[0]);
	    (void) strcpy(cwd, "unknown");
	}
	set_perms(PERM_USER, sudo_mode);
    }

    /*
     * load the host global variable from gethostname() and use
     * gethostbyname() if we want to be sure it is fully qualified.
     */
    if ((gethostname(host, sizeof(host)))) {
	strcpy(host, "localhost");
	log_error(GLOBAL_NO_HOSTNAME);
	inform_user(GLOBAL_NO_HOSTNAME);
	exit(2);
    }
#ifdef FQDN
    if ((h_ent = gethostbyname(host)) == NULL)
	log_error(GLOBAL_HOST_UNREGISTERED);
    else
	strcpy(host, h_ent -> h_name);
#endif /* FQDN */

    /*
     * "host" is the (possibly fully-qualified) hostname and
     * "shost" is the unqualified form of the hostname.
     */
    if ((p = strchr(host, '.'))) {
	*p = '\0';
	shost = estrdup(host);
	*p = '.';
    } else {
	shost = &host[0];
    }

    /*
     * load a list of ip addresses and netmasks into
     * the interfaces array.
     */
    load_interfaces();
}



/**********************************************************************
 *
 * parse_args()
 *
 *  this function parses the arguments to sudo
 */

static int parse_args()
{
    int ret = MODE_RUN;			/* what mode is suod to be run in? */
    int excl = 0;			/* exclusive arg, no others allowed */

    NewArgv = Argv + 1;
    NewArgc = Argc - 1;

#ifdef SHELL_IF_NO_ARGS
    if (Argc < 2) {			/* no options and no command */
	ret |= MODE_SHELL;
	return(ret);
    }
#else
    if (Argc < 2)			/* no options and no command */
	usage(1);
#endif /* SHELL_IF_NO_ARGS */

    while (NewArgc > 0 && NewArgv[0][0] == '-') {
	if (NewArgv[0][1] != '\0' && NewArgv[0][2] != '\0') {
	    (void) fprintf(stderr, "%s: Please use single character options\n",
		Argv[0]);
	    usage(1);
	}

	if (excl)
	    usage(1);			/* only one -? option allowed */

	switch (NewArgv[0][1]) {
#ifdef HAVE_KERB5
	    case 'r':
		/* must have an associated realm */
		if (NewArgv[1] == NULL)
		    usage(1);

		realm = NewArgv[1];

		/* shift Argv over and adjust Argc */
		NewArgc--;
		NewArgv++;
		break;
#endif /* HAVE_KERB5 */
	    case 'p':
		/* must have an associated prompt */
		if (NewArgv[1] == NULL)
		    usage(1);

		prompt = NewArgv[1];
		arg_prompt = 1;

		/* shift Argv over and adjust Argc */
		NewArgc--;
		NewArgv++;
		break;
	    case 'u':
		/* must have an associated runas user */
		if (NewArgv[1] == NULL)
		    usage(1);

		runas_user = NewArgv[1];

		/* shift Argv over and adjust Argc */
		NewArgc--;
		NewArgv++;
		break;
	    case 'b':
		ret |= MODE_BACKGROUND;
		break;
	    case 'v':
		ret = MODE_VALIDATE;
		excl++;
		break;
	    case 'k':
		ret = MODE_KILL;
		excl++;
		break;
	    case 'l':
		ret = MODE_LIST;
		excl++;
		break;
	    case 'V':
		ret = MODE_VERSION;
		excl++;
		break;
	    case 'h':
		ret = MODE_HELP;
		excl++;
		break;
	    case 's':
		ret |= MODE_SHELL;
#ifdef SHELL_SETS_HOME
		ret |= MODE_RESET_HOME;
#endif /* SHELL_SETS_HOME */
		break;
	    case 'H':
		ret |= MODE_RESET_HOME;
		break;
	    case '-':
		NewArgc--;
		NewArgv++;
#ifdef SHELL_IF_NO_ARGS
		if (ret == MODE_RUN)
		    ret |= MODE_SHELL;
#endif /* SHELL_IF_NO_ARGS */
		return(ret);
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

    if (NewArgc > 0 && (ret == MODE_VALIDATE || ret == MODE_KILL ||
			ret == MODE_LIST))
	usage(1);

    return(ret);
}



/**********************************************************************
 *
 * usage()
 *
 *  this function just gives you instructions and exits
 */

static void usage(exit_val)
    int exit_val;
{
    (void) fprintf(stderr,
		   "usage: %s -V | -h | -l | -v | -k | -H | [-b] [-p prompt] ",
		   Argv[0]);
#ifdef HAVE_KERB5
    (void) fprintf(stderr, "[-r realm] ");
#endif /* HAVE_KERB5 */
    (void) fprintf(stderr, "[-u username/#uid] -s | <command>\n");
    exit(exit_val);
}



/**********************************************************************
 *
 * add_env()
 *
 *  this function adds sudo-specific variables into the environment
 */

static void add_env(contiguous)
    int contiguous;
{
    char idstr[MAX_UID_T_LEN + 1];
    size_t size;
    char *buf;

    /* add the SUDO_COMMAND envariable (cmnd + args) */
    size = strlen(cmnd) + 1;
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
	 * Copy the command and it's arguments info buf
	 */
	(void) strcpy(buf, cmnd);
	to = buf + strlen(cmnd);
	for (from = &NewArgv[1]; *from; from++) {
	    *to++ = ' ';
	    (void) strcpy(to, *from);
	    to += strlen(*from);
	}
    } else {
	buf = cmnd;
    }
    if (sudo_setenv("SUDO_COMMAND", buf)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
    if (NewArgc > 1)
	(void) free(buf);

    /* grab a pointer to the flat arg string from the environment */
    if (NewArgc > 1 && (cmnd_args = getenv("SUDO_COMMAND"))) {
	if ((cmnd_args = strchr(cmnd_args, ' ')))
	    cmnd_args++;
	else
	    cmnd_args = NULL;
    }

    /* add the SUDO_USER envariable */
    if (sudo_setenv("SUDO_USER", user_name)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /* add the SUDO_UID envariable */
    (void) sprintf(idstr, "%ld", (long) user_uid);
    if (sudo_setenv("SUDO_UID", idstr)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /* add the SUDO_GID envariable */
    (void) sprintf(idstr, "%ld", (long) user_gid);
    if (sudo_setenv("SUDO_GID", idstr)) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /* set PS1 if SUDO_PS1 is set */
    if ((buf = getenv("SUDO_PS1")))
	if (sudo_setenv("PS1", buf)) {
	    (void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	    exit(1);
	}
}



/**********************************************************************
 *
 *  load_cmnd()
 *
 *  This function sets the cmnd global variable
 *  Returns 1 on success, 0 on failure.
 */

static int load_cmnd(sudo_mode)
    int sudo_mode;
{
    int retval;

    if (strlen(NewArgv[0]) >= MAXPATHLEN) {
	errno = ENAMETOOLONG;
	(void) fprintf(stderr, "%s: %s: Pathname too long\n", Argv[0],
		       NewArgv[0]);
	exit(1);
    }

    /*
     * Resolve the path
     */
    if ((retval = find_path(NewArgv[0], &cmnd)) != FOUND)
	cmnd = NewArgv[0];
    return(retval);
}



/**********************************************************************
 *
 *  check_sudoers()
 *
 *  This function check to see that the sudoers file is owned by
 *  uid SUDOERS_UID, gid SUDOERS_GID and is mode SUDOERS_MODE.
 */

static int check_sudoers()
{
    struct stat statbuf;
    int rootstat, i;
    char c;
    int rtn = ALL_SYSTEMS_GO;

    /*
     * Fix the mode and group on sudoers file from old default.
     * Only works if filesystem is readable/writable by root.
     */
    set_perms(PERM_ROOT, 0);
    if ((rootstat = lstat(_PATH_SUDO_SUDOERS, &statbuf)) == 0 &&
	SUDOERS_UID == statbuf.st_uid && SUDOERS_MODE != 0400 &&
	(statbuf.st_mode & 0007777) == 0400) {

	if (chmod(_PATH_SUDO_SUDOERS, SUDOERS_MODE) == 0) {
	    (void) fprintf(stderr, "%s: fixed mode on %s\n",
		Argv[0], _PATH_SUDO_SUDOERS);
	    if (statbuf.st_gid != SUDOERS_GID) {
		if (!chown(_PATH_SUDO_SUDOERS,GID_NO_CHANGE,SUDOERS_GID)) {
		    (void) fprintf(stderr, "%s: set group on %s\n",
			Argv[0], _PATH_SUDO_SUDOERS);
		    statbuf.st_gid = SUDOERS_GID;
		} else {
		    (void) fprintf(stderr,"%s: Unable to set group on %s: ",
			Argv[0], _PATH_SUDO_SUDOERS);
		    perror("");
		}
	    }
	} else {
	    (void) fprintf(stderr, "%s: Unable to fix mode on %s: ",
		Argv[0], _PATH_SUDO_SUDOERS);
	    perror("");
	}
    }

    /*
     * Sanity checks on sudoers file.  Must be done as sudoers
     * file owner.  We already did a stat as root, so use that
     * data if we can't stat as sudoers file owner.
     */
    set_perms(PERM_SUDOERS, 0);

    if (rootstat != 0 && lstat(_PATH_SUDO_SUDOERS, &statbuf) != 0)
	rtn = NO_SUDOERS_FILE;
    else if (!S_ISREG(statbuf.st_mode))
	rtn = SUDOERS_NOT_FILE;
    else if ((statbuf.st_mode & 0007777) != SUDOERS_MODE)
	rtn = SUDOERS_WRONG_MODE;
    else if (statbuf.st_uid != SUDOERS_UID || statbuf.st_gid != SUDOERS_GID)
	rtn = SUDOERS_WRONG_OWNER;
    else {
	/* Solaris sometimes returns EAGAIN so try 10 times */
	for (i = 0; i < 10 ; i++) {
	    errno = 0;
	    if ((sudoers_fp = fopen(_PATH_SUDO_SUDOERS, "r")) == NULL ||
		fread(&c, sizeof(c), 1, sudoers_fp) != 1) {
		sudoers_fp = NULL;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		    break;
	    } else
		break;
	    sleep(1);
	}
	if (sudoers_fp == NULL) {
	    fprintf(stderr, "%s: cannot open %s: ", Argv[0], _PATH_SUDO_SUDOERS);
	    perror("");
	    rtn = NO_SUDOERS_FILE;
	}
    }

    set_perms(PERM_ROOT, 0);
    set_perms(PERM_USER, 0);

    return(rtn);
}



/**********************************************************************
 *
 * set_perms()
 *
 *  this function sets real and effective uids and gids based on perm.
 */

void set_perms(perm, sudo_mode)
    int perm;
    int sudo_mode;
{
    struct passwd *pw;

    switch (perm) {
	case PERM_ROOT:
				if (setuid(0)) {
				    perror("setuid(0)");
				    exit(1);
				}
			      	break;

	case PERM_USER: 
    	    	    	        (void) setgid(user_gid);

    	    	    	        if (seteuid(user_uid)) {
    	    	    	            perror("seteuid(user_uid)");
    	    	    	            exit(1); 
    	    	    	        }
			      	break;
				
	case PERM_FULL_USER: 
				if (setuid(0)) {
				    perror("setuid(0)");
				    exit(1);
				}

    	    	    	        (void) setgid(user_gid);

				if (setuid(user_uid)) {
				    perror("setuid(user_uid)");
				    exit(1);
				}

			      	break;
	case PERM_RUNAS:
				if (setuid(0)) {
				    perror("setuid(0)");
				    exit(1);
				}
				
				/* XXX - add group/gid support */
				if (*runas_user == '#') {
				    if (setuid(atoi(runas_user + 1))) {
					(void) fprintf(stderr,
					    "%s: cannot set uid to %s: ",
					    Argv[0], runas_user);
					perror("");
					exit(1);
				    }
				} else {
				    if (!(pw = getpwnam(runas_user))) {
					(void) fprintf(stderr,
					    "%s: no passwd entry for %s!\n",
					    Argv[0], runas_user);
					exit(1);
				    }

				    /* Set $USER to match target user */
				    if (sudo_setenv("USER", pw->pw_name)) {
					(void) fprintf(stderr,
					    "%s: cannot allocate memory!\n",
					    Argv[0]);
					exit(1);
				    }

				    if (setgid(pw->pw_gid)) {
					(void) fprintf(stderr,
					    "%s: cannot set gid to %d: ",  
					    Argv[0], pw->pw_gid);
					perror("");
					exit(1);
				    }

				    /*
				     * Initialize group vector only if are
				     * going to run as a non-root user.
				     */
				    if (strcmp(runas_user, "root") != 0 &&
					initgroups(runas_user, pw->pw_gid)
					== -1) {
					(void) fprintf(stderr,
					    "%s: cannot set group vector ",
					    Argv[0]);
					perror("");
					exit(1);
				    }

				    if (setuid(pw->pw_uid)) {
					(void) fprintf(stderr,
					    "%s: cannot set uid to %d: ",  
					    Argv[0], pw->pw_uid);
					perror("");
					exit(1);
				    }
				    if (sudo_mode & MODE_RESET_HOME)
					runas_homedir = pw->pw_dir;
				}

				break;
	case PERM_SUDOERS: 
				if (setuid(0)) {
				    perror("setuid(0)");
				    exit(1);
				}

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



/**********************************************************************
 *
 * clean_env()
 *
 *  This function removes things from the environment that match the
 *  entries in badenv_table.  It would be nice to add in the SUDO_*
 *  variables here as well but cmnd has not been defined at this point.
 */

static void clean_env(envp, badenv_table)
    char **envp;
    struct env_table *badenv_table;
{
    struct env_table *bad;
    char **cur;

    /*
     * Remove any envars that match entries in badenv_table
     */
    for (cur = envp; *cur; cur++) {
	for (bad = badenv_table; bad -> name; bad++) {
	    if (strncmp(*cur, bad -> name, bad -> len) == 0) {
		/* got a match so remove it */
		char **move;

		for (move = cur; *move; move++)
		    *move = *(move + 1);

		cur--;

		break;
	    }
	}
    }
}
