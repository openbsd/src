/*	$NetBSD: gencons.c,v 1.7 1996/01/28 12:11:57 ragge Exp $	*/

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

#include "sys/param.h"
#include "sys/proc.h"
#include "sys/systm.h"
#include "sys/ioctl.h"
#include "sys/tty.h"
#include "sys/file.h"
#include "sys/conf.h"
#include "sys/device.h"
#include "sys/reboot.h"

#include "dev/cons.h"

#include "machine/mtpr.h"
#include "machine/../vax/gencons.h"

struct	tty *gencn_tty[1];

int consinied = 0;

int	gencnparam();
void	gencnstart();

int
gencnopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct proc *p;
{
        int unit;
        struct tty *tp;

        unit = minor(dev);
        if (unit) return ENXIO;

	tp = gencn_tty[0];

        tp->t_oproc = gencnstart;
        tp->t_param = gencnparam;
        tp->t_dev = dev;
        if ((tp->t_state & TS_ISOPEN) == 0) {
                tp->t_state |= TS_WOPEN;
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
	mtpr(GC_RIE, PR_RXCS); /* Turn on interrupts */
	mtpr(GC_TIE, PR_TXCS);

        return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
gencnclose(dev, flag, mode, p)
        dev_t dev;
        int flag, mode;
        struct proc *p;
{
        int unit = minor(dev);
        struct tty *tp = gencn_tty[0];

        (*linesw[tp->t_line].l_close)(tp, flag);
        ttyclose(tp);
        return (0);
}

struct tty *
gencntty(dev)
	dev_t dev;
{
	return gencn_tty[0]; /* XXX */
}

int
gencnread(dev, uio, flag)
        dev_t dev;
        struct uio *uio;
        int flag;
{
        int unit = minor(dev);
        struct tty *tp = gencn_tty[0];

        return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
gencnwrite(dev, uio, flag)
        dev_t dev;
        struct uio *uio;
        int flag;
{
        int unit = minor(dev);
        struct tty *tp = gencn_tty[0];
        return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
gencnioctl(dev, cmd, data, flag, p)
        dev_t dev;
        int cmd;
        caddr_t data;
        int flag;
        struct proc *p;
{
        int error;
        int unit = minor(dev);
        struct tty *tp = gencn_tty[0];

        error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
        if (error >= 0)
                return error;
        error = ttioctl(tp, cmd, data, flag, p);
        if (error >= 0) return error;
 
	return ENOTTY;
}

void
gencnstart(tp)
        struct tty *tp;
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
		mtpr(ch, PR_TXDB);
	} else {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)cl);
		}
		selwakeup(&tp->t_wsel);
	}

out:	splx(s);
}

gencnrint()
{
	struct tty *tp;
	int i, j;

	tp = gencn_tty[0];
	i = mfpr(PR_RXDB);

#ifdef DDB
	j = kdbrint(i);

	if (j == 1)	/* Escape received, just return */
		return;

	if (j == 2)	/* Second char wasn't 'D' */
		(*linesw[tp->t_line].l_rint)(27, tp);
#endif

	(*linesw[tp->t_line].l_rint)(i,tp);
	return;
}

int
gencnstop(tp, flag)
        struct tty *tp;
        int flag;
{
}

gencntint()
{
	struct tty *tp;

	tp = gencn_tty[0];
	tp->t_state &= ~TS_BUSY;

	gencnstart(tp);
}

int
gencnparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
        /* XXX - These are ignored... */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = t->c_cflag;
	return 0;
}

int
gencnprobe(cndev)
	struct	consdev *cndev;
{
	int i;

	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i].d_open == gencnopen) {
			cndev->cn_dev = makedev(i,0);
			cndev->cn_pri = CN_NORMAL;
			break;
		}
	return 0;
}

int
gencninit(cndev)
	struct	consdev *cndev;
{
}

gencnslask()
{
	gencn_tty[0] = ttymalloc();
}

int
gencnputc(dev,ch)
	dev_t	dev;
	int	ch;
{
	while ((mfpr(PR_TXCS) & GC_RDY) == 0) /* Wait until xmit ready */
		;
	mtpr(ch, PR_TXDB);	/* xmit character */
	if(ch == 10)
		gencnputc(dev, 13); /* CR/LF */

}

int
gencngetc(dev)
	dev_t	dev;
{
	int i;

	while ((mfpr(PR_RXCS) & GC_DON) == 0) /* Receive chr */
		;
	i = mfpr(PR_RXDB) & 0x7f;
	if (i == 13)
		i = 10;
	return i;
}

conout(str)
	char *str;
{
	while (*str)
		gencnputc(0, *str++);
}
