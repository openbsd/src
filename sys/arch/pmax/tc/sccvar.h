/*	$NetBSD: sccvar.h,v 1.2 1996/05/19 18:24:33 jonathan Exp $	*/

/*
 *
 */
extern int		sccGetc __P((dev_t));
extern void		sccPutc __P((dev_t, int));
extern int		sccparam __P((struct tty *, struct termios *));

/*
 * Minor device numbers for scc. Weird because B channel comes
 * first and the A channels are wired for keyboard/mouse and the
 * B channels for the comm port(s).
 *
 * XXX
 *
 * Even that is not true on the Personal Decstation, which has
 * a "desktop bus" for keyboard/mouse, and brings A and B channels
 * out to the bulkhead.  XXX more thought.
 */

#define	SCCCOMM2_PORT	0x0
#define	SCCMOUSE_PORT	0x1
#define	SCCCOMM3_PORT	0x2
#define	SCCKBD_PORT	0x3

