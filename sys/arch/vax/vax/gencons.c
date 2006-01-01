/*	$OpenBSD: gencons.c,v 1.16 2006/01/01 11:59:40 miod Exp $	*/
/*	$NetBSD: gencons.c,v 1.22 2000/01/24 02:40:33 matt Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	kd.c,v 1.2 1994/05/05 04:46:51 gwr Exp $
 */

 /* All bugs are subject to removal without further notice */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <dev/cons.h>

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/scb.h>
#include <machine/../vax/gencons.h>

struct tty *gencn_tty[4];

int consopened = 0;
int maxttys = 1;

int pr_txcs[4] = {PR_TXCS, PR_TXCS1, PR_TXCS2, PR_TXCS3};
int pr_rxcs[4] = {PR_RXCS, PR_RXCS1, PR_RXCS2, PR_RXCS3};
int pr_txdb[4] = {PR_TXDB, PR_TXDB1, PR_TXDB2, PR_TXDB3};
int pr_rxdb[4] = {PR_RXDB, PR_RXDB1, PR_RXDB2, PR_RXDB3};

cons_decl(gen);
cdev_decl(gencn);

int gencnparam(struct tty *, struct termios *);
void gencnstart(struct tty *);
void gencnrint(void *);
void gencntint(void *);

int
gencnopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct proc *p;
{
	int unit;
	struct tty *tp;

	unit = minor(dev);
	if (unit >= maxttys)
		return ENXIO;

	if (gencn_tty[unit] == NULL)
		gencn_tty[unit] = ttymalloc();

	tp = gencn_tty[unit];

	tp->t_oproc = gencnstart;
	tp->t_param = gencnparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		gencnparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return EBUSY;
	tp->t_state |= TS_CARR_ON;
	if (unit == 0)
		consopened = 1;
	mtpr(GC_RIE, pr_rxcs[unit]); /* Turn on interrupts */
	mtpr(GC_TIE, pr_txcs[unit]);

        return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
gencnclose(dev, flag, mode, p)
        dev_t dev;
        int flag, mode;
        struct proc *p;
{
	struct tty *tp = gencn_tty[minor(dev)];

	if (minor(dev) == 0)
		consopened = 0;
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return (0);
}

struct tty *
gencntty(dev_t dev)
{
	return gencn_tty[minor(dev)];
}

int
gencnread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = gencn_tty[minor(dev)];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
gencnwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = gencn_tty[minor(dev)];

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
gencnioctl(dev, cmd, data, flag, p)
        dev_t dev;
        u_long cmd;
        caddr_t data;
        int flag;
        struct proc *p;
{
	struct tty *tp = gencn_tty[minor(dev)];
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
 
	return ENOTTY;
}

void
gencnstart(struct tty *tp)
{
	struct clist *cl;
	int s, ch;

	s = spltty();
	if (tp->t_state & (TS_BUSY|TS_TTSTOP|TS_TIMEOUT))
		goto out;
	cl = &tp->t_outq;

	if(cl->c_cc){
		tp->t_state |= TS_BUSY;
		ch = getc(cl);
		mtpr(ch, pr_txdb[minor(tp->t_dev)]);
	} else {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)cl);
		}
		selwakeup(&tp->t_wsel);
	}

out:	splx(s);
}

void
gencnrint(void *arg)
{
	struct tty *tp = *(struct tty **) arg;
	int unit = (struct tty **) arg - gencn_tty;
	int i;

	i = mfpr(pr_rxdb[unit]) & 0377; /* Mask status flags etc... */

#ifdef DDB
	if (tp->t_dev == cn_tab->cn_dev) {
		int j = kdbrint(i);

		if (j == 1)	/* Escape received, just return */
			return;

		if (j == 2)	/* Second char wasn't 'D' */
			(*linesw[tp->t_line].l_rint)(27, tp);
	}
#endif

	(*linesw[tp->t_line].l_rint)(i, tp);
	return;
}

int
gencnstop(struct tty *tp, int flag)
{
	return 0;
}

void
gencntint(void *arg)
{
	struct tty *tp = *(struct tty **) arg;

	tp->t_state &= ~TS_BUSY;

	gencnstart(tp);
}

int
gencnparam(struct tty *tp, struct termios *t)
{
	/* XXX - These are ignored... */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

void
gencnprobe(struct consdev *cndev)
{
	if ((vax_cputype < VAX_TYP_UV2) || /* All older has MTPR console */
	    (vax_boardtype == VAX_BTYP_9RR) ||
	    (vax_boardtype == VAX_BTYP_630) ||
	    (vax_boardtype == VAX_BTYP_650) ||
	    (vax_boardtype == VAX_BTYP_660) ||
	    (vax_boardtype == VAX_BTYP_670) ||
	    (vax_boardtype == VAX_BTYP_1301) ||
	    (vax_boardtype == VAX_BTYP_1303) ||
	    (vax_boardtype == VAX_BTYP_1305)) {
		cndev->cn_dev = makedev(25, 0);
		cndev->cn_pri = CN_NORMAL;
	}
}

void
gencninit(struct consdev *cndev)
{

	/* Allocate interrupt vectors */
	scb_vecalloc(SCB_G0R, gencnrint, &gencn_tty[0], SCB_ISTACK, NULL);
	scb_vecalloc(SCB_G0T, gencntint, &gencn_tty[0], SCB_ISTACK, NULL);

	if (vax_cputype == VAX_TYP_8SS) {
		maxttys = 4;
		scb_vecalloc(SCB_G1R, gencnrint, &gencn_tty[1], SCB_ISTACK, NULL);
		scb_vecalloc(SCB_G1T, gencntint, &gencn_tty[1], SCB_ISTACK, NULL);

		scb_vecalloc(SCB_G2R, gencnrint, &gencn_tty[2], SCB_ISTACK, NULL);
		scb_vecalloc(SCB_G2T, gencntint, &gencn_tty[2], SCB_ISTACK, NULL);

		scb_vecalloc(SCB_G3R, gencnrint, &gencn_tty[3], SCB_ISTACK, NULL);
		scb_vecalloc(SCB_G3T, gencntint, &gencn_tty[3], SCB_ISTACK, NULL);
	}
	mtpr(0, PR_RXCS);
	mtpr(0, PR_TXCS);
	mtpr(0, PR_TBIA); /* ??? */
}

void
gencnputc(dev_t dev, int ch)
{
#ifdef VAX8800
	/*
	 * On KA88 we may get C-S/C-Q from the console.
	 * XXX - this will cause a loop at spltty() in kernel and will
	 * interfere with other console communication. Fortunately
	 * kernel printf's are uncommon.
	 */
	if (vax_cputype == VAX_TYP_8NN) {
		int s = spltty();

		while (mfpr(PR_RXCS) & GC_DON) {
			if ((mfpr(PR_RXDB) & 0x7f) == 19) {
				while (1) {
					while ((mfpr(PR_RXCS) & GC_DON) == 0)
						;
					if ((mfpr(PR_RXDB) & 0x7f) == 17)
						break;
				}
			}
		}
		splx(s);
	}
#endif

	while ((mfpr(PR_TXCS) & GC_RDY) == 0) /* Wait until xmit ready */
		;
	mtpr(ch, PR_TXDB);	/* xmit character */
	if(ch == 10)
		gencnputc(dev, 13); /* CR/LF */

}

int
gencngetc(dev_t dev)
{
	int i;

	while ((mfpr(PR_RXCS) & GC_DON) == 0) /* Receive chr */
		;
	i = mfpr(PR_RXDB) & 0x7f;
	if (i == 13)
		i = 10;
	return i;
}

void 
gencnpollc(dev_t dev, int pollflag)
{
	if (pollflag)  {
		mtpr(0, PR_RXCS);
		mtpr(0, PR_TXCS); 
	} else if (consopened) {
		mtpr(GC_RIE, PR_RXCS);
		mtpr(GC_TIE, PR_TXCS);
	}
}
