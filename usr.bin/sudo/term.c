/*
 * Copyright (c) 2009 Todd C. Miller <Todd.Miller@courtesan.com>
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
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
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

#ifndef TCSASOFT
# define TCSASOFT	0
#endif
#ifndef ECHONL
# define ECHONL		0
#endif
#ifndef IEXTEN
# define IEXTEN		0
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
# else /* SGTTY */
#  undef termios
#  define termios		sgttyb
#  define c_lflag		sg_flags
#  define tcgetattr(f, t)	ioctl(f, TIOCGETP, t)
#  define tcsetattr(f, a, t)	ioctl(f, a, t)
#  undef TCSAFLUSH
#  define TCSAFLUSH		TIOCSETP
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

typedef struct termios sudo_term_t;

static sudo_term_t term, oterm;
static int changed;
int term_erase;
int term_kill;

int
term_restore(fd)
    int fd;
{
    if (changed) {
	if (tcsetattr(fd, TCSAFLUSH|TCSASOFT, &oterm) != 0)
	    return(0);
	changed = 0;
    }
    return(1);
}

int
term_noecho(fd)
    int fd;
{
    if (!changed && tcgetattr(fd, &oterm) != 0)
	return(0);
    (void) memcpy(&term, &oterm, sizeof(term));
    CLR(term.c_lflag, ECHO|ECHONL);
#ifdef VSTATUS
    term.c_cc[VSTATUS] = _POSIX_VDISABLE;
#endif
    if (tcsetattr(fd, TCSAFLUSH|TCSASOFT, &term) == 0) {
	changed = 1;
	return(1);
    }
    return(0);
}

#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H)
int
term_raw(fd)
    int fd;
{
    if (!changed && tcgetattr(fd, &oterm) != 0)
	return(0);
    (void) memcpy(&term, &oterm, sizeof(term));
    /* Set terminal to half-cooked mode */
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    CLR(term.c_lflag, ECHO | ECHONL | ICANON | IEXTEN);
    SET(term.c_lflag, ISIG);
#ifdef VSTATUS
    term.c_cc[VSTATUS] = _POSIX_VDISABLE;
#endif
    if (tcsetattr(fd, TCSAFLUSH|TCSASOFT, &term) == 0) {
	term_erase = term.c_cc[VERASE];
	term_kill = term.c_cc[VKILL];
	changed = 1;
	return(1);
    }
    return(0);
}

#else /* SGTTY */

int
term_raw(fd)
    int fd;
{
    if (!changed && ioctl(fd, TIOCGETP, &oterm) != 0)
	return(0);
    (void) memcpy(&term, &oterm, sizeof(term));
    /* Set terminal to half-cooked mode */
    CLR(term.c_lflag, ECHO);
    SET(term.sg_flags, CBREAK);
    if (ioctl(fd, TIOCSETP, &term) == 0) {
	term_erase = term.sg_erase;
	term_kill = term.sg_kill;
	changed = 1;
	return(1);
    }
    return(0);
}

#endif
