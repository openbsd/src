/*
 * Copyright (c) 1996, 1998-2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <sys/param.h>
#include <sys/types.h>
#ifdef HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h>
#endif /* HAVE_SYS_BSDTYPES_H */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#else
#include <sgtty.h>
#include <sys/ioctl.h>
#endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: tgetpass.c,v 1.95 2000/02/27 03:48:56 millert Exp $";
#endif /* lint */

#ifndef TCSASOFT
#define TCSASOFT	0
#endif /* TCSASOFT */

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
# define term_setattr(f, t)	ioctl(f, TCSETA, t)
# else
#  define TERM			sgttyb
#  define tflags		sg_flags
#  define term_getattr(f, t)	ioctl(f, TIOCGETP, t)
#  define term_setattr(f, t)	ioctl(f, TIOCSETP, t)
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

static char *tgetline __P((int, char *, size_t, int));

/*
 * Like getpass(3) but with timeout and echo flags.
 */
char *
tgetpass(prompt, timeout, flags)
    const char *prompt;
    int timeout;
    int flags;
{
    struct TERM term, oterm;
    int input, output;
    static char buf[SUDO_PASS_MAX + 1];

    /* Open /dev/tty for reading/writing if possible else use stdin/stderr. */
    if ((flags & TGP_STDIN) ||
	(input = output = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1) {
	input = STDIN_FILENO;
	output = STDERR_FILENO;
    }

    if (prompt)
	(void) write(output, prompt, strlen(prompt) + 1);

    /* Turn echo off/on as specified by flags.  */
    (void) term_getattr(input, &oterm);
    (void) memcpy(&term, &oterm, sizeof(term));
    if ((flags & TGP_ECHO) && !(term.tflags & ECHO))
	term.tflags |= ECHO;
    else if (!(flags & TGP_ECHO) && (term.tflags & ECHO))
	term.tflags &= ~ECHO;
    (void) term_setattr(input, &term);

    buf[0] = '\0';
    tgetline(input, buf, sizeof(buf), timeout);

    /* Restore old tty flags.  */
    (void) term_setattr(input, &oterm);

    if (!(flags & TGP_ECHO))
	(void) write(output, "\n", 1);

    if (input != STDIN_FILENO)
	(void) close(input);

    return(buf);
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
    size_t left;
    int n;
    fd_set *readfds = NULL;
    struct timeval tv;
    char c;
    char *cp;

    if (bufsiz == 0)
	return(NULL);			/* sanity */

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
		errno == EINTR)
		;
	    if (n == 0)
		return(NULL);		/* timeout */

	    /* Read a character, exit loop on error, EOF or EOL */
	    n = read(fd, &c, 1);
	    if (n != 1 || c == '\n' || c == '\r')
		break;
	    *cp++ = c;
	}
	free(readfds);
    } else {
	/* Keep reading until out of space, EOF, error, or newline */
	while (--left && (n = read(fd, &c, 1)) == 1 && c != '\n' && c != '\r')
	    *cp++ = c;
    }
    *cp = '\0';

    return(cp == buf ? NULL : buf);
}
