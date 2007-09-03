/*	$OpenBSD: init.c,v 1.40 2007/09/03 14:26:54 deraadt Exp $	*/
/*	$NetBSD: init.c,v 1.22 1996/05/15 23:29:33 jtc Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.2 (Berkeley) 4/28/95";
#else
static char rcsid[] = "$OpenBSD: init.c,v 1.40 2007/09/03 14:26:54 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/reboot.h>

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <unistd.h>
#include <util.h>

#ifdef SECURE
#include <pwd.h>
#endif

#ifdef LOGIN_CAP
#include <login_cap.h>
#endif

#include "pathnames.h"

/*
 * Sleep times; used to prevent thrashing.
 */
#define	GETTY_SPACING		 5	/* N secs minimum getty spacing */
#define	GETTY_SLEEP		30	/* sleep N secs after spacing problem */
#define	WINDOW_WAIT		 3	/* wait N secs after starting window */
#define	STALL_TIMEOUT		30	/* wait N secs after warning */
#define	DEATH_WATCH		10	/* wait N secs for procs to die */

/*
 * User-based resource limits.
 */
#define RESOURCE_RC		"daemon"
#define RESOURCE_WINDOW		"default"
#define RESOURCE_GETTY		"default"

#ifndef DEFAULT_STATE
#define DEFAULT_STATE		runcom
#endif

void handle(sig_t, ...);
void delset(sigset_t *, ...);

void stall(char *, ...);
void warning(char *, ...);
void emergency(char *, ...);
void disaster(int);
void badsys(int);

/*
 * We really need a recursive typedef...
 * The following at least guarantees that the return type of (*state_t)()
 * is sufficiently wide to hold a function pointer.
 */
typedef long (*state_func_t)(void);
typedef state_func_t (*state_t)(void);

state_func_t single_user(void);
state_func_t runcom(void);
state_func_t read_ttys(void);
state_func_t multi_user(void);
state_func_t clean_ttys(void);
state_func_t catatonia(void);
state_func_t death(void);
state_func_t hard_death(void);
state_func_t nice_death(void);

enum { AUTOBOOT, FASTBOOT } runcom_mode = AUTOBOOT;

void transition(state_t);
state_t requested_transition = DEFAULT_STATE;

void setctty(char *);

typedef struct init_session {
	int	se_index;		/* index of entry in ttys file */
	pid_t	se_process;		/* controlling process */
	time_t	se_started;		/* used to avoid thrashing */
	int	se_flags;		/* status of session */
#define	SE_SHUTDOWN	0x1		/* session won't be restarted */
#define	SE_PRESENT	0x2		/* session is in /etc/ttys */
#define	SE_DEVEXISTS	0x4		/* open does not result in ENODEV */
	char	*se_device;		/* filename of port */
	char	*se_getty;		/* what to run on that port */
	char	**se_getty_argv;	/* pre-parsed argument array */
	char	*se_window;		/* window system (started only once) */
	char	**se_window_argv;	/* pre-parsed argument array */
	struct	init_session *se_prev;
	struct	init_session *se_next;
} session_t;

void free_session(session_t *);
session_t *new_session(session_t *, int, struct ttyent *);
session_t *sessions;

char **construct_argv(char *);
void start_window_system(session_t *);
void collect_child(pid_t);
pid_t start_getty(session_t *);
void transition_handler(int);
void alrm_handler(int);
void setsecuritylevel(int);
int getsecuritylevel(void);
int setupargv(session_t *, struct ttyent *);
int clang;

#ifdef LOGIN_CAP
void setprocresources(char *);
#else
#define setprocresources(p)
#endif

void clear_session_logs(session_t *);

int start_session_db(void);
void add_session(session_t *);
void del_session(session_t *);
session_t *find_session(pid_t);
DB *session_db;

/*
 * The mother of all processes.
 */
int
main(int argc, char *argv[])
{
	int c;
	struct sigaction sa;
	sigset_t mask;

	/* Dispose of random users. */
	if (getuid() != 0) {
		(void)fprintf(stderr, "init: %s\n", strerror(EPERM));
		exit (1);
	}

	/* System V users like to reexec init. */
	if (getpid() != 1) {
		(void)fprintf(stderr, "init: already running\n");
		exit (1);
	}

	/*
	 * Note that this does NOT open a file...
	 * Does 'init' deserve its own facility number?
	 */
	openlog("init", LOG_CONS|LOG_ODELAY, LOG_AUTH);

	/*
	 * Create an initial session.
	 */
	if (setsid() < 0)
		warning("initial setsid() failed: %m");

	/*
	 * Establish an initial user so that programs running
	 * single user do not freak out and die (like passwd).
	 */
	if (setlogin("root") < 0)
		warning("setlogin() failed: %m");

	/*
	 * This code assumes that we always get arguments through flags,
	 * never through bits set in some random machine register.
	 */
	while ((c = getopt(argc, argv, "sf")) != -1)
		switch (c) {
		case 's':
			requested_transition = single_user;
			break;
		case 'f':
			runcom_mode = FASTBOOT;
			break;
		default:
			warning("unrecognized flag '-%c'", c);
			break;
		}

	if (optind != argc)
		warning("ignoring excess arguments");

	/*
	 * We catch or block signals rather than ignore them,
	 * so that they get reset on exec.
	 */
	handle(badsys, SIGSYS, 0);
	handle(disaster, SIGABRT, SIGFPE, SIGILL, SIGSEGV,
	    SIGBUS, SIGXCPU, SIGXFSZ, 0);
	handle(transition_handler, SIGHUP, SIGTERM, SIGTSTP, SIGUSR1,
	    SIGUSR2, 0);
	handle(alrm_handler, SIGALRM, 0);
	sigfillset(&mask);
	delset(&mask, SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGSYS,
	    SIGXCPU, SIGXFSZ, SIGHUP, SIGTERM, SIGUSR1, SIGUSR2,
	    SIGTSTP, SIGALRM, 0);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	memset(&sa, 0, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	(void) sigaction(SIGTTIN, &sa, NULL);
	(void) sigaction(SIGTTOU, &sa, NULL);

	/*
	 * Paranoia.
	 */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/*
	 * Start the state machine.
	 */
	transition(requested_transition);

	/*
	 * Should never reach here.
	 */
	exit(1);
}

/*
 * Associate a function with a signal handler.
 */
void
handle(sig_t handler, ...)
{
	int sig;
	struct sigaction sa;
	sigset_t mask_everything;
	va_list ap;

	va_start(ap, handler);

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = handler;
	sigfillset(&mask_everything);

	while ((sig = va_arg(ap, int))) {
		sa.sa_mask = mask_everything;
		/* XXX SA_RESTART? */
		sa.sa_flags = sig == SIGCHLD ? SA_NOCLDSTOP : 0;
		sigaction(sig, &sa, NULL);
	}
	va_end(ap);
}

/*
 * Delete a set of signals from a mask.
 */
void
delset(sigset_t *maskp, ...)
{
	int sig;
	va_list ap;

	va_start(ap, maskp);
	while ((sig = va_arg(ap, int)))
		sigdelset(maskp, sig);
	va_end(ap);
}

/*
 * Log a message and sleep for a while (to give someone an opportunity
 * to read it and to save log or hardcopy output if the problem is chronic).
 * NB: should send a message to the session logger to avoid blocking.
 */
void
stall(char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vsyslog(LOG_ALERT, message, ap);
	va_end(ap);
	closelog();
	sleep(STALL_TIMEOUT);
}

/*
 * Like stall(), but doesn't sleep.
 * If cpp had variadic macros, the two functions could be #defines for another.
 * NB: should send a message to the session logger to avoid blocking.
 */
void
warning(char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vsyslog(LOG_ALERT, message, ap);
	va_end(ap);
	closelog();
}

/*
 * Log an emergency message.
 * NB: should send a message to the session logger to avoid blocking.
 */
void
emergency(char *message, ...)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	va_list ap;

	va_start(ap, message);
	vsyslog_r(LOG_EMERG, &sdata, message, ap);
	va_end(ap);
}

/*
 * Catch a SIGSYS signal.
 *
 * These may arise if a system does not support sysctl.
 * We tolerate up to 25 of these, then throw in the towel.
 */
void
badsys(int sig)
{
	static int badcount = 0;

	if (badcount++ < 25)
		return;
	disaster(sig);
}

/*
 * Catch an unexpected signal.
 */
void
disaster(int sig)
{
	emergency("fatal signal: %s", strsignal(sig));

	sleep(STALL_TIMEOUT);
	_exit(sig);		/* reboot */
}

/*
 * Get the security level of the kernel.
 */
int
getsecuritylevel(void)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;
	size_t len;

	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	len = sizeof curlevel;
	if (sysctl(name, 2, &curlevel, &len, NULL, 0) == -1) {
		emergency("cannot get kernel security level: %s",
		    strerror(errno));
		return (-1);
	}
	return (curlevel);
#else
	return (-1);
#endif
}

/*
 * Set the security level of the kernel.
 */
void
setsecuritylevel(int newlevel)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;

	curlevel = getsecuritylevel();
	if (newlevel == curlevel)
		return;
	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	if (sysctl(name, 2, NULL, NULL, &newlevel, sizeof newlevel) == -1) {
		emergency(
		    "cannot change kernel security level from %d to %d: %s",
		    curlevel, newlevel, strerror(errno));
		return;
	}
#ifdef SECURE
	warning("kernel security level changed from %d to %d",
	    curlevel, newlevel);
#endif
#endif
}

/*
 * Change states in the finite state machine.
 * The initial state is passed as an argument.
 */
void
transition(state_t s)
{
	for (;;)
		s = (state_t) (*s)();
}

/*
 * Close out the accounting files for a login session.
 * NB: should send a message to the session logger to avoid blocking.
 */
void
clear_session_logs(session_t *sp)
{
	char *line = sp->se_device + sizeof(_PATH_DEV) - 1;

	if (logout(line))
		logwtmp(line, "", "");
}

/*
 * Start a session and allocate a controlling terminal.
 * Only called by children of init after forking.
 */
void
setctty(char *name)
{
	int fd;

	(void) revoke(name);
	sleep(2);			/* leave DTR low */
	if ((fd = open(name, O_RDWR)) == -1) {
		stall("can't open %s: %m", name);
		_exit(1);
	}
	if (login_tty(fd) == -1) {
		stall("can't get %s for controlling terminal: %m", name);
		_exit(1);
	}
}

/*
 * Bring the system up single user.
 */
state_func_t
single_user(void)
{
	pid_t pid, wpid;
	int status;
	sigset_t mask;
	char shell[MAXPATHLEN];		/* Allocate space here */
	char name[MAXPATHLEN];		/* Name (argv[0]) of shell */
	char *argv[2];
#ifdef SECURE
	struct ttyent *typ;
	struct passwd *pp;
	static const char banner[] =
		"Enter root password, or ^D to go multi-user\n";
	char *clear, *password;
#endif

	/* Init shell and name */
	strlcpy(shell, _PATH_BSHELL, sizeof shell);
	strlcpy(name, "-sh", sizeof name);

	/*
	 * If the kernel is in secure mode, downgrade it to insecure mode.
	 */
	if (getsecuritylevel() > 0)
		setsecuritylevel(0);

	if ((pid = fork()) == 0) {
		/*
		 * Start the single user session.
		 */
		setctty(_PATH_CONSOLE);

#ifdef SECURE
		/*
		 * Check the root password.
		 * We don't care if the console is 'on' by default;
		 * it's the only tty that can be 'off' and 'secure'.
		 */
		typ = getttynam("console");
		pp = getpwnam("root");
		if (typ && (typ->ty_status & TTY_SECURE) == 0 && pp &&
		    *pp->pw_passwd) {
			write(STDERR_FILENO, banner, sizeof banner - 1);
			for (;;) {
				clear = getpass("Password:");
				if (clear == 0 || *clear == '\0')
					_exit(0);
				password = crypt(clear, pp->pw_passwd);
				memset(clear, 0, _PASSWORD_LEN);
				if (strcmp(password, pp->pw_passwd) == 0)
					break;
				warning("single-user login failed\n");
			}
		}
		endttyent();
		endpwent();
#endif /* SECURE */

#ifdef DEBUGSHELL
		{
			char altshell[128], *cp = altshell;
			int num;

#define	SHREQUEST \
	"Enter pathname of shell or RETURN for sh: "

			(void)write(STDERR_FILENO,
			    SHREQUEST, sizeof(SHREQUEST) - 1);
			while ((num = read(STDIN_FILENO, cp, 1)) != -1 &&
			    num != 0 && *cp != '\n' && cp < &altshell[127])
				cp++;
			*cp = '\0';

			/* Copy in alternate shell */
			if (altshell[0] != '\0'){
				char *p;

				/* Binary to exec */
				strlcpy(shell, altshell, sizeof shell);

				/* argv[0] */
				p = strrchr(altshell, '/');
				if(p == NULL) p = altshell;
				else p++;

				name[0] = '-';
				strlcpy(&name[1], p, sizeof name -1);
			}
		}
#endif /* DEBUGSHELL */

		/*
		 * Unblock signals.
		 * We catch all the interesting ones,
		 * and those are reset to SIG_DFL on exec.
		 */
		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, NULL);

		/*
		 * Fire off a shell.
		 * If the default one doesn't work, try the Bourne shell.
		 */
		argv[0] = name;
		argv[1] = NULL;
		setenv("PATH", _PATH_STDPATH, 1);
		execv(shell, argv);
		emergency("can't exec %s for single user: %m", shell);

		argv[0] = "-sh";
		argv[1] = NULL;
		execv(_PATH_BSHELL, argv);
		emergency("can't exec %s for single user: %m", _PATH_BSHELL);
		sleep(STALL_TIMEOUT);
		_exit(1);
	}

	if (pid == -1) {
		/*
		 * We are seriously hosed.  Do our best.
		 */
		emergency("can't fork single-user shell, trying again");
		while (waitpid(-1, NULL, WNOHANG) > 0)
			continue;
		return (state_func_t) single_user;
	}

	requested_transition = 0;
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid);
		if (wpid == -1) {
			if (errno == EINTR)
				continue;
			warning("wait for single-user shell failed: %m; restarting");
			return (state_func_t) single_user;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("init: shell stopped, restarting\n");
			kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid && !requested_transition);

	if (requested_transition)
		return (state_func_t) requested_transition;

	if (!WIFEXITED(status)) {
		if (WTERMSIG(status) == SIGKILL) {
			/*
			 *  reboot(8) killed shell?
			 */
			warning("single user shell terminated.");
			sleep(STALL_TIMEOUT);
			_exit(0);
		} else {
			warning("single user shell terminated, restarting");
			return (state_func_t) single_user;
		}
	}

	runcom_mode = FASTBOOT;
	return (state_func_t) runcom;
}

/*
 * Run the system startup script.
 */
state_func_t
runcom(void)
{
	pid_t pid, wpid;
	int status;
	char *argv[4];
	struct sigaction sa;

	if ((pid = fork()) == 0) {
		memset(&sa, 0, sizeof sa);
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;
		(void) sigaction(SIGTSTP, &sa, NULL);
		(void) sigaction(SIGHUP, &sa, NULL);

		setctty(_PATH_CONSOLE);

		argv[0] = "sh";
		argv[1] = _PATH_RUNCOM;
		argv[2] = runcom_mode == AUTOBOOT ? "autoboot" : 0;
		argv[3] = 0;

		sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

		setprocresources(RESOURCE_RC);

		execv(_PATH_BSHELL, argv);
		stall("can't exec %s for %s: %m", _PATH_BSHELL, _PATH_RUNCOM);
		_exit(1);	/* force single user mode */
	}

	if (pid == -1) {
		emergency("can't fork for %s on %s: %m",
			_PATH_BSHELL, _PATH_RUNCOM);
		while (waitpid(-1, NULL, WNOHANG) > 0)
			continue;
		sleep(STALL_TIMEOUT);
		return (state_func_t) single_user;
	}

	/*
	 * Copied from single_user().  This is a bit paranoid.
	 */
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid);
		if (wpid == -1) {
			if (errno == EINTR)
				continue;
			warning("wait for %s on %s failed: %m; going to single user mode",
			    _PATH_BSHELL, _PATH_RUNCOM);
			return (state_func_t) single_user;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("init: %s on %s stopped, restarting\n",
			    _PATH_BSHELL, _PATH_RUNCOM);
			kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid);

	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM &&
	    requested_transition == catatonia) {
		/* /etc/rc executed /sbin/reboot; wait for the end quietly */
		sigset_t s;

		sigfillset(&s);
		for (;;)
			sigsuspend(&s);
	}

	if (!WIFEXITED(status)) {
		warning("%s on %s terminated abnormally, going to single user mode",
		    _PATH_BSHELL, _PATH_RUNCOM);
		return (state_func_t) single_user;
	}

	if (WEXITSTATUS(status))
		return (state_func_t) single_user;

	runcom_mode = AUTOBOOT;		/* the default */
	/* NB: should send a message to the session logger to avoid blocking. */
	logwtmp("~", "reboot", "");
	return (state_func_t) read_ttys;
}

/*
 * Open the session database.
 *
 * NB: We could pass in the size here; is it necessary?
 */
int
start_session_db(void)
{
	if (session_db && (*session_db->close)(session_db))
		emergency("session database close: %s", strerror(errno));
	if ((session_db = dbopen(NULL, O_RDWR, 0, DB_HASH, NULL)) == 0) {
		emergency("session database open: %s", strerror(errno));
		return (1);
	}
	return (0);
}

/*
 * Add a new login session.
 */
void
add_session(session_t *sp)
{
	DBT key;
	DBT data;

	key.data = &sp->se_process;
	key.size = sizeof sp->se_process;
	data.data = &sp;
	data.size = sizeof sp;

	if ((*session_db->put)(session_db, &key, &data, 0))
		emergency("insert %d: %s", sp->se_process, strerror(errno));
}

/*
 * Delete an old login session.
 */
void
del_session(session_t *sp)
{
	DBT key;

	key.data = &sp->se_process;
	key.size = sizeof sp->se_process;

	if ((*session_db->del)(session_db, &key, 0))
		emergency("delete %d: %s", sp->se_process, strerror(errno));
}

/*
 * Look up a login session by pid.
 */
session_t *
find_session(pid_t pid)
{
	DBT key;
	DBT data;
	session_t *ret;

	key.data = &pid;
	key.size = sizeof pid;
	if ((*session_db->get)(session_db, &key, &data, 0) != 0)
		return (0);
	memcpy(&ret, data.data, sizeof(ret));
	return (ret);
}

/*
 * Construct an argument vector from a command line.
 */
char **
construct_argv(char *command)
{
	int argc = 0;
	char **argv = (char **) calloc((strlen(command) + 1) / 2 + 1,
	    sizeof (char *));
	static const char separators[] = " \t";

	if (argv == NULL)
		return (0);

	if ((argv[argc++] = strtok(command, separators)) == 0) {
		free(argv);
		return (0);
	}
	while ((argv[argc++] = strtok(NULL, separators)))
		continue;
	return (argv);
}

/*
 * Deallocate a session descriptor.
 */
void
free_session(session_t *sp)
{
	free(sp->se_device);
	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv);
	}
	if (sp->se_window) {
		free(sp->se_window);
		free(sp->se_window_argv);
	}
	free(sp);
}

/*
 * Allocate a new session descriptor.
 */
session_t *
new_session(session_t *sprev, int session_index, struct ttyent *typ)
{
	session_t *sp;

	if ((typ->ty_status & TTY_ON) == 0 ||
	    typ->ty_name == 0 ||
	    typ->ty_getty == 0)
		return (0);

	sp = (session_t *) malloc(sizeof (session_t));
	memset(sp, 0, sizeof *sp);

	sp->se_flags = SE_PRESENT;
	sp->se_index = session_index;

	if (asprintf(&sp->se_device, "%s%s", _PATH_DEV, typ->ty_name) == -1)
		err(1, "asprintf");

	if (setupargv(sp, typ) == 0) {
		free_session(sp);
		return (0);
	}

	sp->se_next = 0;
	if (sprev == 0) {
		sessions = sp;
		sp->se_prev = 0;
	} else {
		sprev->se_next = sp;
		sp->se_prev = sprev;
	}

	return (sp);
}

/*
 * Calculate getty and if useful window argv vectors.
 */
int
setupargv(session_t *sp, struct ttyent *typ)
{
	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv);
	}
	if (asprintf(&sp->se_getty, "%s %s", typ->ty_getty, typ->ty_name) == -1)
		err(1, "asprintf");
	sp->se_getty_argv = construct_argv(sp->se_getty);
	if (sp->se_getty_argv == 0) {
		warning("can't parse getty for port %s", sp->se_device);
		free(sp->se_getty);
		sp->se_getty = 0;
		return (0);
	}
	if (typ->ty_window) {
		if (sp->se_window)
			free(sp->se_window);
		sp->se_window = strdup(typ->ty_window);
		if (sp->se_window == NULL) {
			warning("can't allocate window");
			return (0);
		}
		sp->se_window_argv = construct_argv(sp->se_window);
		if (sp->se_window_argv == NULL) {
			warning("can't parse window for port %s",
			    sp->se_device);
			free(sp->se_window);
			sp->se_window = NULL;
			return (0);
		}
	}
	return (1);
}

/*
 * Walk the list of ttys and create sessions for each active line.
 */
state_func_t
read_ttys(void)
{
	int session_index = 0;
	session_t *sp, *snext;
	struct ttyent *typ;

	/*
	 * Destroy any previous session state.
	 * There shouldn't be any, but just in case...
	 */
	for (sp = sessions; sp; sp = snext) {
		if (sp->se_process)
			clear_session_logs(sp);
		snext = sp->se_next;
		free_session(sp);
	}
	sessions = 0;
	if (start_session_db())
		return (state_func_t) single_user;

	/*
	 * Allocate a session entry for each active port.
	 * Note that sp starts at 0.
	 */
	while ((typ = getttyent()))
		if ((snext = new_session(sp, ++session_index, typ)))
			sp = snext;

	endttyent();

	return (state_func_t) multi_user;
}

/*
 * Start a window system running.
 */
void
start_window_system(session_t *sp)
{
	pid_t pid;
	sigset_t mask;

	if ((pid = fork()) == -1) {
		emergency("can't fork for window system on port %s: %m",
		    sp->se_device);
		/* hope that getty fails and we can try again */
		return;
	}

	if (pid)
		return;

	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	if (setsid() < 0)
		emergency("setsid failed (window) %m");

	setprocresources(RESOURCE_WINDOW);

	execv(sp->se_window_argv[0], sp->se_window_argv);
	stall("can't exec window system '%s' for port %s: %m",
	    sp->se_window_argv[0], sp->se_device);
	_exit(1);
}

/*
 * Start a login session running.
 * For first open, man-handle tty directly to determine if it
 * really exists. It is not efficient to spawn gettys on devices
 * that do not exist.
 */
pid_t
start_getty(session_t *sp)
{
	pid_t pid;
	sigset_t mask;
	time_t current_time = time(NULL);
	int p[2], new = 1;

	if (sp->se_flags & SE_DEVEXISTS)
		new = 0;

	if (new) {
		if (pipe(p) == -1)
			return (-1);
	}

	/*
	 * fork(), not vfork() -- we can't afford to block.
	 */
	if ((pid = fork()) == -1) {
		emergency("can't fork for getty on port %s: %m", sp->se_device);
		return (-1);
	}

	if (pid) {
		if (new) {
			char c;

			close(p[1]);
			if (read(p[0], &c, 1) != 1) {
				close(p[0]);
				return (-1);
			}
			close(p[0]);
			if (c == '1')
				sp->se_flags |= SE_DEVEXISTS;
			else
				sp->se_flags |= SE_SHUTDOWN;
		}
		return (pid);
	}
	if (new) {
		int fd;

		close(p[0]);
		fd = open(sp->se_device, O_RDONLY | O_NONBLOCK, 0666);
		if (fd == -1 && (errno == ENXIO || errno == ENOENT ||
		    errno == EISDIR)) {
			(void)write(p[1], "0", 1);
			close(p[1]);
			_exit(1);
		}
		(void)write(p[1], "1", 1);
		close(p[1]);
		close(fd);
		sleep(1);
	}

	if (current_time > sp->se_started &&
	    current_time - sp->se_started < GETTY_SPACING) {
		warning("getty repeating too quickly on port %s, sleeping",
		    sp->se_device);
		sleep((unsigned) GETTY_SLEEP);
	}

	if (sp->se_window) {
		start_window_system(sp);
		sleep(WINDOW_WAIT);
	}

	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	setprocresources(RESOURCE_GETTY);

	execv(sp->se_getty_argv[0], sp->se_getty_argv);
	stall("can't exec getty '%s' for port %s: %m",
	    sp->se_getty_argv[0], sp->se_device);
	_exit(1);
}

/*
 * Collect exit status for a child.
 * If an exiting login, start a new login running.
 */
void
collect_child(pid_t pid)
{
	session_t *sp, *sprev, *snext;

	if (sessions == NULL)
		return;

	if ((sp = find_session(pid)) == NULL)
		return;

	clear_session_logs(sp);
	login_fbtab(sp->se_device + sizeof(_PATH_DEV) - 1, 0, 0);
	del_session(sp);
	sp->se_process = 0;

	if (sp->se_flags & SE_SHUTDOWN) {
		if ((sprev = sp->se_prev))
			sprev->se_next = sp->se_next;
		else
			sessions = sp->se_next;
		if ((snext = sp->se_next))
			snext->se_prev = sp->se_prev;
		free_session(sp);
		return;
	}

	if ((pid = start_getty(sp)) == -1) {
		/* serious trouble */
		requested_transition = clean_ttys;
		return;
	}

	sp->se_process = pid;
	sp->se_started = time(NULL);
	add_session(sp);
}

/*
 * Catch a signal and request a state transition.
 */
void
transition_handler(int sig)
{

	switch (sig) {
	case SIGHUP:
		requested_transition = clean_ttys;
		break;
	case SIGTERM:
		requested_transition = death;
		break;
	case SIGUSR1:
		requested_transition = nice_death;
		break;
	case SIGUSR2:
		requested_transition = hard_death;
		break;
	case SIGTSTP:
		requested_transition = catatonia;
		break;
	default:
		requested_transition = 0;
		break;
	}
}

/*
 * Take the system multiuser.
 */
state_func_t
multi_user(void)
{
	pid_t pid;
	session_t *sp;

	requested_transition = 0;

	/*
	 * If the administrator has not set the security level to -1
	 * to indicate that the kernel should not run multiuser in secure
	 * mode, and the run script has not set a higher level of security
	 * than level 1, then put the kernel into secure mode.
	 */
	if (getsecuritylevel() == 0)
		setsecuritylevel(1);

	for (sp = sessions; sp; sp = sp->se_next) {
		if (sp->se_process)
			continue;
		if ((pid = start_getty(sp)) == -1) {
			/* serious trouble */
			requested_transition = clean_ttys;
			break;
		}
		sp->se_process = pid;
		sp->se_started = time(NULL);
		add_session(sp);
	}

	while (!requested_transition)
		if ((pid = waitpid(-1, NULL, 0)) != -1)
			collect_child(pid);

	return (state_func_t) requested_transition;
}

/*
 * This is an n-squared algorithm.  We hope it isn't run often...
 */
state_func_t
clean_ttys(void)
{
	session_t *sp, *sprev;
	struct ttyent *typ;
	int session_index = 0;
	int devlen;

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags &= ~SE_PRESENT;

	devlen = sizeof(_PATH_DEV) - 1;
	while ((typ = getttyent())) {
		++session_index;

		for (sprev = 0, sp = sessions; sp; sprev = sp, sp = sp->se_next)
			if (strcmp(typ->ty_name, sp->se_device + devlen) == 0)
				break;

		if (sp) {
			sp->se_flags |= SE_PRESENT;
			if (sp->se_index != session_index) {
				warning("port %s changed utmp index from %d to %d",
				    sp->se_device, sp->se_index,
				    session_index);
				sp->se_index = session_index;
			}
			if ((typ->ty_status & TTY_ON) == 0 ||
			    typ->ty_getty == 0) {
				sp->se_flags |= SE_SHUTDOWN;
				kill(sp->se_process, SIGHUP);
				continue;
			}
			sp->se_flags &= ~SE_SHUTDOWN;
			if (setupargv(sp, typ) == 0) {
				warning("can't parse getty for port %s",
				    sp->se_device);
				sp->se_flags |= SE_SHUTDOWN;
				kill(sp->se_process, SIGHUP);
			}
			continue;
		}

		new_session(sprev, session_index, typ);
	}

	endttyent();

	for (sp = sessions; sp; sp = sp->se_next)
		if ((sp->se_flags & SE_PRESENT) == 0) {
			sp->se_flags |= SE_SHUTDOWN;
			kill(sp->se_process, SIGHUP);
		}

	return (state_func_t) multi_user;
}

/*
 * Block further logins.
 */
state_func_t
catatonia(void)
{
	session_t *sp;

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags |= SE_SHUTDOWN;

	return (state_func_t) multi_user;
}

/*
 * Note SIGALRM.
 */
void
alrm_handler(int sig)
{
	clang = 1;
}

int death_howto = RB_HALT;

/*
 * Bring the system down nicely, then we must powerdown because something
 * is very wrong.
 */
state_func_t
hard_death(void)
{
	death_howto |= RB_POWERDOWN;	
	return nice_death();
}

/*
 * Bring the system down to single user nicely, after run the shutdown script.
 */
state_func_t
nice_death(void)
{
	session_t *sp;
	int i;
	pid_t pid;
	static const int death_sigs[3] = { SIGHUP, SIGTERM, SIGKILL };
	int status;

	for (sp = sessions; sp; sp = sp->se_next) {
		sp->se_flags &= ~SE_PRESENT;
		sp->se_flags |= SE_SHUTDOWN;
		kill(sp->se_process, SIGHUP);
	}

	/* terminate the accounting process */
	acct(NULL);

	/* NB: should send a message to the session logger to avoid blocking. */
	logwtmp("~", "shutdown", "");

	if (access(_PATH_RUNCOM, R_OK) != -1) {
		pid_t pid;
		struct sigaction sa;

		switch ((pid = fork())) {
		case -1:
			break;
		case 0:

			memset(&sa, 0, sizeof sa);
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			sa.sa_handler = SIG_IGN;
			(void) sigaction(SIGTSTP, &sa, NULL);
			(void) sigaction(SIGHUP, &sa, NULL);

			setctty(_PATH_CONSOLE);

			sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

			execl(_PATH_BSHELL, "sh", _PATH_RUNCOM, "shutdown",
			    (char *)NULL);
			stall("can't exec %s for %s %s: %m", _PATH_BSHELL,
			    _PATH_RUNCOM, "shutdown");
			_exit(1);
		default:
			waitpid(pid, &status, 0);
			if (WIFEXITED(status) && WEXITSTATUS(status) == 2)
				death_howto |= RB_POWERDOWN;
		}
	}

	for (i = 0; i < 3; ++i) {
		if (kill(-1, death_sigs[i]) == -1 && errno == ESRCH)
			goto die;

		clang = 0;
		alarm(DEATH_WATCH);
		do {
			if ((pid = waitpid(-1, NULL, 0)) != -1)
				collect_child(pid);
		} while (clang == 0 && errno != ECHILD);

		if (errno == ECHILD)
			goto die;
	}

	warning("some processes would not die; ps axl advised");

die:
	reboot(death_howto);

	/* ... and if that fails.. oh well */
	return (state_func_t) single_user;
}

/*
 * Bring the system down to single user.
 */
state_func_t
death(void)
{
	session_t *sp;
	int i;
	pid_t pid;
	static const int death_sigs[3] = { SIGHUP, SIGTERM, SIGKILL };

	/* terminate the accounting process */
	acct(NULL);

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags |= SE_SHUTDOWN;

	/* NB: should send a message to the session logger to avoid blocking. */
	logwtmp("~", "shutdown", "");

	for (i = 0; i < 3; ++i) {
		if (kill(-1, death_sigs[i]) == -1 && errno == ESRCH)
			return (state_func_t) single_user;

		clang = 0;
		alarm(DEATH_WATCH);
		do {
			if ((pid = waitpid(-1, NULL, 0)) != -1)
				collect_child(pid);
		} while (clang == 0 && errno != ECHILD);

		if (errno == ECHILD)
			return (state_func_t) single_user;
	}

	warning("some processes would not die; ps axl advised");

	return (state_func_t) single_user;
}

#ifdef LOGIN_CAP
void
setprocresources(char *class)
{
	login_cap_t *lc;

	if ((lc = login_getclass(class)) != NULL) {
		setusercontext(lc, NULL, 0,
		    LOGIN_SETPRIORITY|LOGIN_SETRESOURCES|LOGIN_SETUMASK);
		login_close(lc);
	}
}
#endif
