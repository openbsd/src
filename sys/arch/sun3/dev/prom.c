/*	$NetBSD: prom.c,v 1.15 1995/04/10 06:14:57 mycroft Exp $	*/

/*
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Adam Glass BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "prom.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/mon.h>

#include <dev/cons.h>
#include "../sun3/interreg.h"

/*
 * cleanup:
 * get autoconfiguration right, right style
 * not a true serial driver but a tty driver, i.e no carrier
 * make sure start is non-blocking
 * add read support via timeouts
 */

void promattach __P((struct device *, struct device *, void *));

struct prom_softc {
	struct device sc_dev;
	int sc_flags;
	int sc_nopen;
};
struct	tty *prom_tty[NPROM];

struct cfdriver promcd = {
	NULL, "prom", always_match, promattach, DV_TTY, sizeof(struct prom_softc)
};

#define UNIT_TO_PROM_SC(unit) promcd.cd_devs[unit]
#ifndef PROM_RECEIVE_FREQ
#define PROM_RECEIVE_FREQ 10
#endif    

int promopen __P((dev_t, int, int, struct proc *));
int promclose __P((dev_t, int, int, struct proc *));
int promread __P((dev_t, struct uio *, int));
int promwrite __P((dev_t, struct uio *, int));
int promioctl __P((dev_t, int, caddr_t, int, struct proc *));
int promstop __P((struct tty *, int));

static int promparam __P((struct tty *, struct termios *));
static void promstart __P((struct tty *));
static void promreceive __P((void *));

void
promattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{

	printf("\n");		
}

int
promopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;
	struct prom_softc *sc;
	int unit, result;

	unit = minor(dev);
	if (unit >= promcd.cd_ndevs)
		return ENXIO;
	sc = UNIT_TO_PROM_SC(unit);
	if (sc == NULL)
		return ENXIO;

	if (prom_tty[unit] == NULL)
		tp = prom_tty[unit] = ttymalloc();
	else
		tp = prom_tty[unit];

	tp->t_oproc = promstart;
	tp->t_param = promparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		}
		promparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		return EBUSY;
	}

	tp->t_state |= TS_CARR_ON;
	result = (*linesw[tp->t_line].l_open)(dev, tp);
	if (result)
		return result;
	timeout(promreceive, tp, hz/PROM_RECEIVE_FREQ);
	return 0;
}

int
promclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;
	int unit;
	struct prom_softc *sc;

	unit = minor(dev);
	sc = UNIT_TO_PROM_SC(unit);
	tp = prom_tty[unit];

	(*linesw[tp->t_line].l_close)(tp, flag);
	return ttyclose(tp);
}

int
promread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;

	tp = prom_tty[minor(dev)];
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
promwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;

	tp = prom_tty[minor(dev)];
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
promioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct tty *tp;
	int error;

	tp = prom_tty[minor(dev)];

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	return ENOTTY;
}

void
promstart(tp)
	struct tty *tp;
{
	int s, c, count;
	u_char outbuf[50];
	u_char *bufp;

	s = spltty();
	if (tp->t_state & (TS_BUSY|TS_TTSTOP|TS_TIMEOUT))
		goto out;
	tp->t_state |= TS_BUSY;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
		    tp->t_state &=~ TS_ASLEEP;
		    wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	count = q_to_b(&tp->t_outq, outbuf, 49);
	if (count) {
		outbuf[count] = '\0';
		(void) splhigh();
		mon_printf("%s", outbuf);
		(void) spltty();
	}
	tp->t_state &= ~TS_BUSY;
out:
	splx(s);
}

void
promstop(tp, flag)
	struct tty *tp;
	int flag;
{

}

static int
promparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct prom_softc *sc;

	if (t->c_ispeed == 0 || (t->c_ispeed != t->c_ospeed))
		return EINVAL;
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

static void
promreceive(arg)
	void *arg;
{
	struct tty *tp = arg;
	int c, s;
	extern unsigned int orig_nmi_vector;
	extern int nmi_intr();

	s = spltty();
	if (tp->t_state & TS_ISOPEN) {
		if ((tp->t_state & TS_BUSY) == 0) {
			set_clk_mode(0, IREG_CLOCK_ENAB_7|IREG_CLOCK_ENAB_5, 0);
			isr_add_custom(7, orig_nmi_vector);
			set_clk_mode(IREG_CLOCK_ENAB_7, 0, 1);
			c = mon_may_getchar();
			set_clk_mode(0, IREG_CLOCK_ENAB_7|IREG_CLOCK_ENAB_5, 0);
			isr_add_custom(7, nmi_intr);
			set_clk_mode(IREG_CLOCK_ENAB_5, 0, 1);
			if (c != -1)
				(*linesw[tp->t_line].l_rint)(c, tp);
		}
		timeout(promreceive, tp, hz/PROM_RECEIVE_FREQ);
	}
	splx(s);
}

void
promcnprobe(cp)
	struct consdev *cp;
{
	int prommajor;

	/* locate the major number */
	for (prommajor = 0; prommajor < nchrdev; prommajor++)
		if (cdevsw[prommajor].d_open == promopen)
			break;

	cp->cn_dev = makedev(prommajor, 0);
	cp->cn_pri = CN_INTERNAL;	/* will always exist but you don't
					 * want to use it unless you have to
					 */
}

void
promcninit(cp)
	struct consdev *cp;
{

	mon_printf("console on prom0\n");
}

int
promcngetc(dev)
	dev_t dev;
{

	mon_printf("not sure how to do promcngetc() yet\n");
}

/*
 * Console kernel output character routine.
 */
void
promcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s;

	s = splhigh();
	if (minor(dev) != 0)
		mon_printf("non unit 0 prom console???\n");
	mon_putchar(c);
	splx(s);
}
