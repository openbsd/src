/*	$OpenBSD: tty.h,v 1.1.1.1 1996/08/14 06:19:12 downsj Exp $	*/

/*
	tty.h -- centralized definitions for a variety of terminal interfaces

	created by DPK, Oct. 1986

	Rearranged to work with autoconf, added TTY_state, get_tty/set_tty
						Michael Rendell, May '94

	last edit:	30-Jul-1987	D A Gwyn
*/

/* some useful #defines */
#ifdef EXTERN
# define I__(i) = i
#else
# define I__(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#ifdef	HAVE_TERMIOS_H
# include <termios.h>
typedef struct termios TTY_state;
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
#  if _BSD_SYSV			/* BRL UNIX System V emulation */
#   ifndef NTTYDISC
#    define	TIOCGETD	_IOR( 't', 0, int )
#    define	TIOCSETD	_IOW( 't', 1, int )
#    define	NTTYDISC	2
#   endif
#   ifndef TIOCSTI
#    define	TIOCSTI		_IOW( 't', 114, char )
#   endif
#   ifndef TIOCSPGRP
#    define	TIOCSPGRP	_IOW( 't', 118, int )
#   endif
#  endif /* _BSD_SYSV */
typedef struct termio TTY_state;
# else /* HAVE_TERMIO_H */
/* Assume BSD tty stuff.  Uses TIOCGETP, TIOCSETN; uses TIOCGATC/TIOCSATC if
 * available, otherwise it uses TIOCGETC/TIOCSETC (also uses TIOCGLTC/TIOCSLTC
 * if available)
 */
#  ifdef _MINIX
#   include <sgtty.h>
#   define TIOCSETN	TIOCSETP
#  else
#   include <sys/ioctl.h>
#  endif
typedef struct {
	struct sgttyb	sgttyb;
#  ifdef TIOCGATC
	struct lchars	lchars;
#  else /* TIOCGATC */
	struct tchars	tchars;
#   ifdef TIOCGLTC
	struct ltchars	ltchars;
#   endif /* TIOCGLTC */
#  endif /* TIOCGATC */
} TTY_state;
# endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

/* Flags for set_tty() */
#define TF_NONE		0x00
#define TF_WAIT		0x01	/* drain output, even it requires sleep() */
#define TF_MIPSKLUDGE	0x02	/* kludge to unwedge RISC/os 5.0 tty driver */

EXTERN int		tty_fd I__(-1);	/* dup'd tty file descriptor */
EXTERN int		tty_devtty;	/* true if tty_fd is from /dev/tty */
EXTERN TTY_state	tty_state;	/* saved tty state */

extern int	get_tty ARGS((int fd, TTY_state *ts));
extern int	set_tty ARGS((int fd, TTY_state *ts, int flags));
extern void	tty_init ARGS((int init_ttystate));
extern void	tty_close ARGS((void));

/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef I__
