/*	$OpenBSD: promcons.c,v 1.13 2010/04/12 12:57:51 tedu Exp $	*/
/*	$NetBSD: promcons.c,v 1.5 1996/11/13 22:20:55 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <dev/cons.h>

#include <machine/rpb.h>
#include <machine/prom.h>

static struct  tty *prom_tty[1];
static struct  timeout prom_to;

void promstart(struct tty *);
void promtimeout(void *);
int promparam(struct tty *, struct termios *);
cdev_decl(prom);
cons_decl(prom);

int
promopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp;
	int s;
	int error = 0, setuptimeout = 0;
 
	if (unit >= 1)
		return ENXIO;

	s = spltty();

	if (prom_tty[unit] == NULL) {
		tp = prom_tty[unit] = ttymalloc();
	} else
		tp = prom_tty[unit];

	tp->t_oproc = promstart;
	tp->t_param = promparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN|TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = 9600;
		ttsetwater(tp);

		setuptimeout = 1;
	} else if (tp->t_state&TS_XCLUDE && suser(p, 0) != 0) {
		splx(s);
		return EBUSY;
	}

	splx(s);

	error = (*linesw[tp->t_line].l_open)(dev, tp, p);
	if (error == 0 && setuptimeout) {
		timeout_set(&prom_to, promtimeout, tp);
		timeout_add(&prom_to, 1);
	}
	return error;
}
 
int
promclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp = prom_tty[unit];

	timeout_del(&prom_to);
	(*linesw[tp->t_line].l_close)(tp, flag, p);
	ttyclose(tp);
	return 0;
}
 
int
promread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = prom_tty[minor(dev)];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
promwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = prom_tty[minor(dev)];
 
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
	int unit = minor(dev);
	struct tty *tp = prom_tty[unit];
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	return ENOTTY;
}

int
promparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	return 0;
}

void
promstart(tp)
	struct tty *tp;
{
	int s;

	s = spltty();
	if (tp->t_state & (TS_TTSTOP | TS_BUSY))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		promcnputc(tp->t_dev, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;
out:
	splx(s);
}

/*
 * Stop output on a line.
 */
int
promstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
	return 0;
}

void
promtimeout(v)
	void *v;
{
	struct tty *tp = v;
	u_char c;

	while (promcnlookc(tp->t_dev, &c)) {
		if (tp->t_state & TS_ISOPEN)
			(*linesw[tp->t_line].l_rint)(c, tp);
	}
	timeout_add(&prom_to, 1);
}

struct tty *
promtty(dev)
	dev_t dev;
{

	if (minor(dev) != 0)
		panic("promtty: bogus");

	return prom_tty[0];
}
