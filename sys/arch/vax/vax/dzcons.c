/*	$OpenBSD: dzcons.c,v 1.6 2006/01/01 11:59:40 miod Exp $	*/
/*	$NetBSD: dzcons.c,v 1.5 1997/03/22 12:51:01 ragge Exp $	*/
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
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/../vax/gencons.h>

volatile unsigned char *ka410_intreq = (void *)KA410_INTREQ;
volatile unsigned char *ka410_intclr = (void *)KA410_INTCLR;
volatile unsigned char *ka410_intmsk = (void *)KA410_INTMSK;



/*----------------------------------------------------------------------*/

#define REG(name)     short name; short X##name##X;
static volatile struct {/* base address of DZ-controller: 0x200A0000 */
  REG(csr);           /* 00 Csr: control/status register */
  REG(rbuf);          /* 04 Rbuf/Lpr: receive buffer/line param reg. */
  REG(tcr);           /* 08 Tcr: transmit console register */
  REG(tdr);           /* 0C Msr/Tdr: modem status reg/transmit data reg */
  REG(lpr0);          /* 10 Lpr0: */
  REG(lpr1);          /* 14 Lpr0: */
  REG(lpr2);          /* 18 Lpr0: */
  REG(lpr3);          /* 1C Lpr0: */
} *dz = (void *)0x200A0000; 
#undef REG

void dzcnputc ();

int
dzcngetc(dev) 
	dev_t dev;
{
	int c;
	int mapen;
	int imsk;

	imsk = *ka410_intmsk;		/* save interrupt-mask */
	*ka410_intmsk = 0;		/* disable console-receive interrupt! */

	do {
              while ((dz->csr & 0x80) == 0); /* Wait for char */
              c = dz->rbuf & 0xff;
	} while (c == 17 || c == 19);		/* ignore XON/XOFF */

	*ka410_intclr = 0x80;		/* clear the interrupt request */
	*ka410_intmsk = imsk;		/* restore interrupt-mask */

	if (c == 13)
		c = 10;
	return (c);
}

struct	tty *dzcn_tty[1];

int	dzcnparam();
void	dzcnstart();

int	ka410_consintr_enable(void);

int
dzcnopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct proc *p;
{
        int unit;
        struct tty *tp;

        unit = minor(dev);
        if (unit) return ENXIO;

	if (dzcn_tty[0] == NULL)
		dzcn_tty[0] = ttymalloc();

	tp = dzcn_tty[0];

        tp->t_oproc = dzcnstart;
        tp->t_param = dzcnparam;
        tp->t_dev = dev;
        if ((tp->t_state & TS_ISOPEN) == 0) {
                tp->t_state |= TS_WOPEN;
                ttychars(tp);
                tp->t_iflag = TTYDEF_IFLAG;
                tp->t_oflag = TTYDEF_OFLAG;
                tp->t_cflag = TTYDEF_CFLAG;
                tp->t_lflag = TTYDEF_LFLAG;
                tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
                dzcnparam(tp, &tp->t_termios);
                ttsetwater(tp);
        } else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
                return EBUSY;
        tp->t_state |= TS_CARR_ON;
	ka410_consintr_enable();	/* Turn on interrupts */
	

        return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
dzcnclose(dev, flag, mode, p)
        dev_t dev;
        int flag, mode;
        struct proc *p;
{
        int unit = minor(dev);
        struct tty *tp = dzcn_tty[0];

        (*linesw[tp->t_line].l_close)(tp, flag);
        ttyclose(tp);
        return (0);
}

struct tty *
dzcntty(dev)
	dev_t dev;
{

	return dzcn_tty[0]; /* XXX */
}

int
dzcnread(dev, uio, flag)
        dev_t dev;
        struct uio *uio;
        int flag;
{
        int unit = minor(dev);
        struct tty *tp = dzcn_tty[0];

        return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dzcnwrite(dev, uio, flag)
        dev_t dev;
        struct uio *uio;
        int flag;
{
        int unit = minor(dev);
        struct tty *tp = dzcn_tty[0];

        return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
dzcnioctl(dev, cmd, data, flag, p)
        dev_t dev;
        int cmd;
        caddr_t data;
        int flag;
        struct proc *p;
{
        int error;
        int unit = minor(dev);
        struct tty *tp = dzcn_tty[0];

        error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
        if (error >= 0)
                return error;
        error = ttioctl(tp, cmd, data, flag, p);
        if (error >= 0) return error;
 
	return ENOTTY;
}

void
dzcnstart(tp)
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
		dz->tdr = ch;
	} else {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)cl);
		}
		selwakeup(&tp->t_wsel);
	}

out:	splx(s);
}

dzcnrint()
{
	struct tty *tp;
	int i, j;

	tp = dzcn_tty[0];
	while ((dz->csr & 0x80) == 0); /* Wait for char */
	i = dz->rbuf & 0xff;

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

void
dzcnstop(tp, flag)
        struct tty *tp;
        int flag;
{

}

dzcntint()
{
	struct tty *tp;

	tp = dzcn_tty[0];
	tp->t_state &= ~TS_BUSY;

	dzcnstart(tp);
}

int
dzcnparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
        /* XXX - These are ignored... */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = t->c_cflag;
	return 0;
}

void
dzcnprobe(cndev)
	struct	consdev *cndev;
{
	int i;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		break;

	default:
		return;
	}

	for (i = 0; i < nchrdev; i++)
		if (cdevsw[i].d_open == dzcnopen) {
			cndev->cn_dev = makedev(i,0);
			cndev->cn_pri = CN_NORMAL;
			return;
		}
}

int
dzcninit(cndev)
	struct	consdev *cndev;
{
	dz = (void *)uvax_phys2virt ((int) dz);
	ka410_intreq = (void *)uvax_phys2virt ((int)ka410_intreq);
	ka410_intclr = (void *)uvax_phys2virt ((int)ka410_intclr);
	ka410_intmsk = (void *)uvax_phys2virt ((int)ka410_intmsk);

	dz->csr = 0;    /* Disable scanning until initting is done */
	dz->rbuf = 0;   /* Turn off line 0's receiver */
	dz->rbuf = 1;   /* Turn off line 1's receiver */
	dz->rbuf = 2;   /* Turn off line 2's receiver */
	                /* Leave line 3 alone */
	dz->tcr = 8;    /* Turn off all but line 3's xmitter */
	dz->csr = 0x20; /* Turn scanning back on */
}


void
dzcnputc(dev,ch)
	dev_t	dev;
	int	ch;
{
	int	imsk;
	int timeout = 1<<15;            /* don't hang the machine! */

	imsk = *ka410_intmsk;
	*ka410_intmsk = 0;

	while ((dz->csr & 0x8000) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;
	dz->tdr = ch;                    /* Put the  character */

	*ka410_intmsk = imsk;
}
