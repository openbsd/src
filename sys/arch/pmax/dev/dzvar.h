/*	$OpenBSD: dzvar.h,v 1.1 2000/08/19 18:36:19 maja Exp $	*/
/*	$NetBSD: dcvar.h,v 1.4 1997/05/28 14:21:39 jonathan Exp $	*/

/*
 * External declarations from DECstation dc serial driver.
 */

#ifdef _KERNEL
#ifndef _DZVAR_H
#define _DZVAR_H

#include <pmax/dev/pdma.h>

struct dz_softc {
	struct device sc_dv;
	struct pdma dz_pdma[4];
	struct	tty *dz_tty[4];
	/*
	 * Software copy of brk register since it isn't readable
	 */
	int	dz_brk;

	char	dz_19200;		/* this unit supports 19200 */
	char	dzsoftCAR;		/* mask, lines with carrier on (DSR) */
	char	dz_rtscts;		/* mask, lines with hw flow control */
	char	dz_modem;		/* mask, lines with  DTR wired  */
};

int	dzattach __P((struct dz_softc *sc, void *addr,
			int dtrmask, int rts_ctsmask,
			int speed, int consline));
int	dzintr __P((void * xxxunit));

/*
 * Following declaratios for console code.
 * XXX shuould be separated, or redesigned.
 */
extern int dzGetc __P ((dev_t dev));
extern int dzparam __P((register struct tty *tp, register struct termios *t));
extern void dzPutc __P((dev_t dev, int c));

struct dc7085regs;
void dz_consinit __P((dev_t dev, volatile struct dc7085regs *dcaddr));

/* QVSS-compatible in-kernel X input event parser, pointer tracker */
void	(*dzDivertXInput) __P((int cc)); /* X windows keyboard input routine */
void	(*dzMouseEvent) __P((int));	/* X windows mouse motion event routine */
void	(*dzMouseButtons) __P((int));	/* X windows mouse buttons event routine */

#endif	/* _DZVAR_H */
#endif	/* _KERNEL */
