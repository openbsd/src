/*	$OpenBSD: tty.h,v 1.3 2004/12/18 20:55:52 millert Exp $	*/

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

#include <termios.h>
typedef struct termios TTY_state;

/* Flags for set_tty() */
#define TF_NONE		0x00
#define TF_WAIT		0x01	/* drain output, even it requires sleep() */
#define TF_MIPSKLUDGE	0x02	/* kludge to unwedge RISC/os 5.0 tty driver */

EXTERN int		tty_fd I__(-1);	/* dup'd tty file descriptor */
EXTERN int		tty_devtty;	/* true if tty_fd is from /dev/tty */
EXTERN TTY_state	tty_state;	/* saved tty state */

extern int	get_tty(int fd, TTY_state *ts);
extern int	set_tty(int fd, TTY_state *ts, int flags);
extern void	tty_init(int init_ttystate);
extern void	tty_close(void);

/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef I__
