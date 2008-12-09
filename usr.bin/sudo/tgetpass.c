/*
 * Copyright (c) 1996, 1998-2005, 2007-2008
 *	Todd C. Miller <Todd.Miller@courtesan.com>
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

#ifdef __TANDEM
# include <floss.h>
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_BSDTYPES_H
# include <sys/bsdtypes.h>
#endif /* HAVE_SYS_BSDTYPES_H */
#include <sys/time.h>
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
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# else
#  include <sgtty.h>
#  include <sys/ioctl.h>
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

#include "sudo.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: tgetpass.c,v 1.126 2008/12/09 20:55:49 millert Exp $";
#endif /* lint */

#ifndef TCSASOFT
# define TCSASOFT	0
#endif
#ifndef ECHONL
# define ECHONL	0
#endif

#ifndef _POSIX_VDISABLE
# ifdef VDISABLE
#  define _POSIX_VDISABLE	VDISABLE
# else
#  define _POSIX_VDISABLE	0
# endif
#endif

/*
 * Compat macros for non-termios systems.
 */
#ifndef HAVE_TERMIOS_H
# ifdef HAVE_TERMIO_H
#  undef termios
#  define termios		termio
#  define tcgetattr(f, t)	ioctl(f, TCGETA, t)
#  define tcsetattr(f, a, t)	ioctl(f, a, t)
#  undef TCSAFLUSH
#  define TCSAFLUSH		TCSETAF
# else
#  undef termios
#  define termios		sgttyb
#  define c_lflag		sg_flags
#  define tcgetattr(f, t)	ioctl(f, TIOCGETP, t)
#  define tcsetattr(f, a, t)	ioctl(f, a, t)
#  undef TCSAFLUSH
#  define TCSAFLUSH		TIOCSETP
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

static volatile sig_atomic_t signo;

static void handler __P((int));
static char *getln __P((int, char *, size_t));
static char *sudo_askpass __P((const char *));

/*
 * Like getpass(3) but with timeout and echo flags.
 */
char *
tgetpass(prompt, timeout, flags)
    const char *prompt;
    int timeout;
    int flags;
{
    sigaction_t sa, savealrm, saveint, savehup, savequit, saveterm;
    sigaction_t savetstp, savettin, savettou;
    struct termios term, oterm;
    char *pass;
    static char buf[SUDO_PASS_MAX + 1];
    int input, output, save_errno;

    (void) fflush(stdout);

    /* If using a helper program to get the password, run it instead. */
    if (ISSET(flags, TGP_ASKPASS) && user_askpass)
	return(sudo_askpass(prompt));

restart:
    signo = 0;
    pass = NULL;
    save_errno = 0;
    /* Open /dev/tty for reading/writing if possible else use stdin/stderr. */
    if (ISSET(flags, TGP_STDIN) ||
	(input = output = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1) {
	input = STDIN_FILENO;
	output = STDERR_FILENO;
    }

    /*
     * Catch signals that would otherwise cause the user to end
     * up with echo turned off in the shell.
     */
    zero_bytes(&sa, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_INTERRUPT;	/* don't restart system calls */
    sa.sa_handler = handler;
    (void) sigaction(SIGALRM, &sa, &savealrm);
    (void) sigaction(SIGINT, &sa, &saveint);
    (void) sigaction(SIGHUP, &sa, &savehup);
    (void) sigaction(SIGQUIT, &sa, &savequit);
    (void) sigaction(SIGTERM, &sa, &saveterm);
    (void) sigaction(SIGTSTP, &sa, &savetstp);
    (void) sigaction(SIGTTIN, &sa, &savettin);
    (void) sigaction(SIGTTOU, &sa, &savettou);

    /* Turn echo off/on as specified by flags.  */
    if (tcgetattr(input, &oterm) == 0) {
	(void) memcpy(&term, &oterm, sizeof(term));
	if (!ISSET(flags, TGP_ECHO))
	    CLR(term.c_lflag, ECHO|ECHONL);
#ifdef VSTATUS
	term.c_cc[VSTATUS] = _POSIX_VDISABLE;
#endif
	(void) tcsetattr(input, TCSAFLUSH|TCSASOFT, &term);
    } else {
	zero_bytes(&term, sizeof(term));
	zero_bytes(&oterm, sizeof(oterm));
    }

    /* No output if we are already backgrounded. */
    if (signo != SIGTTOU && signo != SIGTTIN) {
	if (prompt)
	    (void) write(output, prompt, strlen(prompt));

	if (timeout > 0)
	    alarm(timeout);
	pass = getln(input, buf, sizeof(buf));
	alarm(0);
	save_errno = errno;

	if (!ISSET(term.c_lflag, ECHO))
	    (void) write(output, "\n", 1);
    }

    /* Restore old tty settings and signals. */
    if (memcmp(&term, &oterm, sizeof(term)) != 0) {
	while (tcsetattr(input, TCSAFLUSH|TCSASOFT, &oterm) == -1 &&
	    errno == EINTR)
	    continue;
    }
    (void) sigaction(SIGALRM, &savealrm, NULL);
    (void) sigaction(SIGINT, &saveint, NULL);
    (void) sigaction(SIGHUP, &savehup, NULL);
    (void) sigaction(SIGQUIT, &savequit, NULL);
    (void) sigaction(SIGTERM, &saveterm, NULL);
    (void) sigaction(SIGTSTP, &savetstp, NULL);
    (void) sigaction(SIGTTIN, &savettin, NULL);
    (void) sigaction(SIGTTOU, &savettou, NULL);
    if (input != STDIN_FILENO)
	(void) close(input);

    /*
     * If we were interrupted by a signal, resend it to ourselves
     * now that we have restored the signal handlers.
     */
    if (signo) {
	kill(getpid(), signo);
	switch (signo) {
	    case SIGTSTP:
	    case SIGTTIN:
	    case SIGTTOU:
		goto restart;
	}
    }

    if (save_errno)
	errno = save_errno;
    return(pass);
}

/*
 * Fork a child and exec sudo-askpass to get the password from the user.
 */
static char *
sudo_askpass(prompt)
    const char *prompt;
{
    static char buf[SUDO_PASS_MAX + 1], *pass;
    sigaction_t sa, saved_sa_pipe;
    int pfd[2];
    pid_t pid;

    if (pipe(pfd) == -1)
	error(1, "unable to create pipe");

    if ((pid = fork()) == -1)
	error(1, "unable to fork");

    if (pid == 0) {
	/* child, point stdout to output side of the pipe and exec askpass */
	(void) dup2(pfd[1], STDOUT_FILENO);
	set_perms(PERM_FULL_USER);
	closefrom(STDERR_FILENO + 1);
	execl(user_askpass, user_askpass, prompt, (char *)NULL);
	warning("unable to run %s", user_askpass);
	_exit(255);
    }

    /* Ignore SIGPIPE in case child exits prematurely */
    zero_bytes(&sa, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    (void) sigaction(SIGPIPE, &sa, &saved_sa_pipe);

    /* Get response from child (askpass) and restore SIGPIPE handler */
    (void) close(pfd[1]);
    pass = getln(pfd[0], buf, sizeof(buf));
    (void) close(pfd[0]);
    (void) sigaction(SIGPIPE, &saved_sa_pipe, NULL);

    return(pass);
}

static char *
getln(fd, buf, bufsiz)
    int fd;
    char *buf;
    size_t bufsiz;
{
    ssize_t nr = -1;
    char *cp = buf;
    char c = '\0';

    if (bufsiz == 0) {
	errno = EINVAL;
	return(NULL);			/* sanity */
    }

    while (--bufsiz) {
	nr = read(fd, &c, 1);
	if (nr != 1 || c == '\n' || c == '\r')
	    break;
	*cp++ = c;
    }
    *cp = '\0';

    return(nr == 1 ? buf : NULL);
}

static void
handler(s)
    int s;
{
    if (s != SIGALRM)
	signo = s;
}

int
tty_present()
{
    int fd;

    if ((fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) != -1)
	close(fd);
    return(fd != -1);
}
