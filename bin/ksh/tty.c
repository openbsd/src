/*	$OpenBSD: tty.c,v 1.2 1996/10/01 02:05:51 downsj Exp $	*/

#include "sh.h"
#include "ksh_stat.h"
#define EXTERN
#include "tty.h"
#undef EXTERN

int
get_tty(fd, ts)
	int fd;
	TTY_state *ts;
{
	int ret;

# ifdef HAVE_TERMIOS_H
	ret = tcgetattr(fd, ts);
# else /* HAVE_TERIOS_H */
#  ifdef HAVE_TERMIO_H
	ret = ioctl(fd, TCGETA, ts);
#  else /* HAVE_TERMIO_H */
	ret = ioctl(fd, TIOCGETP, &ts->sgttyb);
#   ifdef TIOCGATC
	if (ioctl(fd, TIOCGATC, &ts->lchars) < 0)
		ret = -1;
#   else
	if (ioctl(fd, TIOCGETC, &ts->tchars) < 0)
		ret = -1;
#    ifdef TIOCGLTC
	if (ioctl(fd, TIOCGLTC, &ts->ltchars) < 0)
		ret = -1;
#    endif /* TIOCGLTC */
#   endif /* TIOCGATC */
#  endif /* HAVE_TERMIO_H */
# endif /* HAVE_TERIOS_H */
	return ret;
}

int
set_tty(fd, ts, flags)
	int fd;
	TTY_state *ts;
	int flags;
{
	int ret = 0;

# ifdef HAVE_TERMIOS_H
	ret = tcsetattr(fd, TCSADRAIN, ts);
# else /* HAVE_TERIOS_H */
#  ifdef HAVE_TERMIO_H
#   ifndef TCSETAW				/* e.g. Cray-2 */
		/* first wait for output to drain */
#    ifdef TCSBRK
		if (ioctl(tty_fd, TCSBRK, 1) < 0)
			ret = -1;
#    else /* the following kludge is minimally intrusive, but sometimes fails */
		if (flags & TF_WAIT)
			sleep((unsigned)1);	/* fake it */
#    endif
#   endif /* !TCSETAW */
#   if defined(_BSD_SYSV) || !defined(TCSETAW)
/* _BSD_SYSV must force TIOCSETN instead of TIOCSETP (preserve type-ahead) */
		if (ioctl(tty_fd, TCSETA, ts) < 0)
			ret = -1;
#   else
		if (ioctl(tty_fd, TCSETAW, ts) < 0)
			ret = -1;
#   endif
#  else /* HAVE_TERMIO_H */
#   if defined(__mips) && (defined(_SYSTYPE_BSD43) || defined(__SYSTYPE_BSD43))
	/* Under RISC/os 5.00, bsd43 environment, after a tty driver
	 * generated interrupt (eg, INTR, TSTP), all output to tty is
	 * lost until a SETP is done (there must be a better way of
	 * doing this...).
	 */
	if (flags & TF_MIPSKLUDGE)
		ret = ioctl(fd, TIOCSETP, &ts->sgttyb);
	else
#   endif /* _SYSTYPE_BSD43 */
	    ret = ioctl(fd, TIOCSETN, &ts->sgttyb);
#   ifdef TIOCGATC
	if (ioctl(fd, TIOCSATC, &ts->lchars) < 0)
		ret = -1;
#   else
	if (ioctl(fd, TIOCSETC, &ts->tchars) < 0)
		ret = -1;
#    ifdef TIOCGLTC
	if (ioctl(fd, TIOCSLTC, &ts->ltchars) < 0)
		ret = -1;
#    endif /* TIOCGLTC */
#   endif /* TIOCGATC */
#  endif /* HAVE_TERMIO_H */
# endif /* HAVE_TERIOS_H */
	return ret;
}


/* Initialize tty_fd.  Used for saving/reseting tty modes upon
 * foreground job completion and for setting up tty process group.
 */
void
tty_init(init_ttystate)
	int init_ttystate;
{
	int	do_close = 1;
	int	tfd;

	if (tty_fd >= 0) {
		close(tty_fd);
		tty_fd = -1;
	}
	tty_devtty = 1;

	/* SCO can't job control on /dev/tty, so don't try... */
#if !defined(__SCO__)
	if ((tfd = open("/dev/tty", O_RDWR, 0)) < 0) {
#ifdef __NeXT
		/* rlogin on NeXT boxes does not set up the controlling tty,
		 * so force it to be done here...
		 */
		{
			extern char *ttyname ARGS((int));
			char *s = ttyname(isatty(2) ? 2 : 0);
			int fd;

			if (s && (fd = open(s, O_RDWR, 0)) >= 0) {
				close(fd);
				tfd = open("/dev/tty", O_RDWR, 0);
			}
		}
#endif /* __NeXT */

/* X11R5 xterm on mips doesn't set controlling tty properly - temporary hack */
# if !defined(__mips) || !(defined(_SYSTYPE_BSD43) || defined(__SYSTYPE_BSD43))
		if (tfd < 0) {
			tty_devtty = 0;
			warningf(FALSE,
				"No controlling tty (open /dev/tty: %s)",
				strerror(errno));
		}
# endif /* __mips  */
	}
#else /* !__SCO__ */
	tfd = -1;
#endif /* __SCO__ */

	if (tfd < 0) {
		do_close = 0;
		if (isatty(0))
			tfd = 0;
		else if (isatty(2))
			tfd = 2;
		else {
			warningf(FALSE, "Can't find tty file descriptor");
			return;
		}
	}
	if ((tty_fd = ksh_dupbase(tfd, FDBASE)) < 0) {
		warningf(FALSE, "j_ttyinit: dup of tty fd failed: %s",
			strerror(errno));
	} else if (fd_clexec(tty_fd) < 0) {
		warningf(FALSE, "j_ttyinit: can't set close-on-exec flag: %s",
			strerror(errno));
		close(tty_fd);
		tty_fd = -1;
	} else if (init_ttystate)
		get_tty(tty_fd, &tty_state);
	if (do_close)
		close(tfd);
}

void
tty_close()
{
	if (tty_fd >= 0) {
		close(tty_fd);
		tty_fd = -1;
	}
}
