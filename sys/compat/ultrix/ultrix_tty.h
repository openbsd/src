/*	$NetBSD: ultrix_tty.h,v 1.2 1996/01/04 19:03:36 jonathan Exp $	*/

/* From:  NetBSD sunos.h,v 1.4 1995/03/04 09:50:00 pk Exp 	*/


struct ultrix_ttysize {
	int	ts_row;
	int	ts_col;
};

/*
 *  Ultrix includes (BRL-derived?)  SysV compatibile termio
 * as well as termios.  This is the termio structure.
 */

struct ultrix_termio {
	u_short	c_iflag;
	u_short	c_oflag;
	u_short	c_cflag;
	u_short	c_lflag;
	char	c_line;
	unsigned char c_cc[10];	/* 8 for SunOS */
};
#define ULTRIX_TCGETA	_IOR('t', 91, struct ultrix_termio)
#define ULTRIX_TCSETA	_IOW('t', 90, struct ultrix_termio)
#define ULTRIX_TCSETAW	_IOW('t', 89, struct ultrix_termio)
#define ULTRIX_TCSETAF	_IOW('t', 88, struct ultrix_termio)

/*
 * Ultrix POSIX-compatible termios.
 * Very similar to SunOS but with more c_cc entries (gag)
 */
struct ultrix_termios {
	u_long	c_iflag;
	u_long	c_oflag;
	u_long	c_cflag;
	u_long	c_lflag;
	u_char	c_cc[19]; /* 17 for Sun */
	u_char	c_line;
};

#define ULTRIX_TCXONC	_IO('T', 6)
#define ULTRIX_TCFLSH	_IO('T', 7)
#define ULTRIX_TCGETS	_IOR('t', 85, struct ultrix_termios)
#define ULTRIX_TCSETS	_IOW('t', 84, struct ultrix_termios) /* set termios */
#define ULTRIX_TCSETSW	_IOW('t', 83, struct ultrix_termios) /* Drain&set,*/
#define ULTRIX_TCSETSF	_IOW('t', 82, struct ultrix_termios) /*Drainflush,set*/
#define ULTRIX_TCSNDBRK	_IO('T', 12)
#define ULTRIX_TCDRAIN	_IO('T', 13)
