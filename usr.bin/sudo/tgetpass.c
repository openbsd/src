/*
 * Copyright (c) 1996, 1998-2001 Todd C. Miller <Todd.Miller@courtesan.com>
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
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_BSDTYPES_H
# include <sys/bsdtypes.h>
#endif /* HAVE_SYS_BSDTYPES_H */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
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
static const char rcsid[] = "$Sudo: tgetpass.c,v 1.104 2002/12/13 18:20:34 millert Exp $";
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
 * Abstract method of getting at the term flags.
 */
#undef TERM
#undef tflags
#ifdef HAVE_TERMIOS_H
# define TERM			termios
# define tflags			c_lflag
# define term_getattr(f, t)	tcgetattr(f, t)
# define term_setattr(f, t)	tcsetattr(f, TCSAFLUSH|TCSASOFT, t)
#else
# ifdef HAVE_TERMIO_H
# define TERM			termio
# define tflags			c_lflag
# define term_getattr(f, t)	ioctl(f, TCGETA, t)
# define term_setattr(f, t)	ioctl(f, TCSETAF, t)
# else
#  define TERM			sgttyb
#  define tflags		sg_flags
#  define term_getattr(f, t)	ioctl(f, TIOCGETP, t)
#  define term_setattr(f, t)	ioctl(f, TIOCSETP, t)
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

static volatile sig_atomic_t signo;

static char *tgetline __P((int, char *, size_t, int));
static void handler __P((int));

/*
 * Like getpass(3) but with timeout and echo flags.
 */
char *
tgetpass(prompt, timeout, flags)
    const char *prompt;
    int timeout;
    int flags;
{
    sigaction_t sa, saveint, savehup, savequit, saveterm;
    sigaction_t savetstp, savettin, savettou;
    static char buf[SUDO_PASS_MAX + 1];
    int input, output, save_errno;
    struct TERM term, oterm;
    char *pass;

restart:
    /* Open /dev/tty for reading/writing if possible else use stdin/stderr. */
    if ((flags & TGP_STDIN) ||
	(input = output = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1) {
	input = STDIN_FILENO;
	output = STDERR_FILENO;
    }

    /*
     * Catch signals that would otherwise cause the user to end
     * up with echo turned off in the shell.  Don't worry about
     * things like SIGALRM and SIGPIPE for now.
     */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;		/* don't restart system calls */
    sa.sa_handler = handler;
    (void) sigaction(SIGINT, &sa, &saveint);
    (void) sigaction(SIGHUP, &sa, &savehup);
    (void) sigaction(SIGQUIT, &sa, &savequit);
    (void) sigaction(SIGTERM, &sa, &saveterm);
    (void) sigaction(SIGTSTP, &sa, &savetstp);
    (void) sigaction(SIGTTIN, &sa, &savettin);
    (void) sigaction(SIGTTOU, &sa, &savettou);

    /* Turn echo off/on as specified by flags.  */
    if (term_getattr(input, &oterm) == 0) {
	(void) memcpy(&term, &oterm, sizeof(term));
	if (!(flags & TGP_ECHO))
	    term.tflags &= ~(ECHO | ECHONL);
#ifdef VSTATUS
	term.c_cc[VSTATUS] = _POSIX_VDISABLE;
#endif
	(void) term_setattr(input, &term);
    } else {
	memset(&term, 0, sizeof(term));
	memset(&oterm, 0, sizeof(oterm));
    }

    if (prompt)
	(void) write(output, prompt, strlen(prompt));

    pass = tgetline(input, buf, sizeof(buf), timeout);
    save_errno = errno;

    if (!(term.tflags & ECHO))
	(void) write(output, "\n", 1);

    /* Restore old tty settings and signals. */
    if (memcmp(&term, &oterm, sizeof(term)) != 0)
	(void) term_setattr(input, &oterm);
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
		signo = 0;
		goto restart;
	}
    }

    errno = save_errno;
    return(pass);
}

/*
 * Get a line of input (optionally timing out) and place it in buf.
 */
static char *
tgetline(fd, buf, bufsiz, timeout)
    int fd;
    char *buf;
    size_t bufsiz;
    int timeout;
{
    fd_set *readfds = NULL;
    struct timeval tv;
    size_t left;
    char *cp;
    char c;
    int n;

    if (bufsiz == 0) {
	errno = EINVAL;
	return(NULL);			/* sanity */
    }

    cp = buf;
    left = bufsiz;

    /*
     * Timeout of <= 0 means no timeout.
     */
    if (timeout > 0) {
	/* Setup for select(2) */
	n = howmany(fd + 1, NFDBITS) * sizeof(fd_mask);
	readfds = (fd_set *) emalloc(n);
	(void) memset((VOID *)readfds, 0, n);

	/* Set timeout for select */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	while (--left) {
	    FD_SET(fd, readfds);

	    /* Make sure there is something to read (or timeout) */
	    while ((n = select(fd + 1, readfds, 0, 0, &tv)) == -1 &&
		errno == EAGAIN)
		;
	    if (n <= 0) {
		free(readfds);
		return(NULL);		/* timeout or interrupt */
	    }

	    /* Read a character, exit loop on error, EOF or EOL */
	    n = read(fd, &c, 1);
	    if (n != 1 || c == '\n' || c == '\r')
		break;
	    *cp++ = c;
	}
	free(readfds);
    } else {
	/* Keep reading until out of space, EOF, error, or newline */
	n = -1;
	while (--left && (n = read(fd, &c, 1)) == 1 && c != '\n' && c != '\r')
	    *cp++ = c;
    }
    *cp = '\0';

    return(n == -1 ? NULL : buf);
}

static void handler(s)
    int s;
{
    signo = s;
}
