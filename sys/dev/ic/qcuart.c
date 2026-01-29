/*	$OpenBSD: qcuart.c,v 1.1 2026/01/29 11:23:35 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <dev/cons.h>

#include <dev/ic/qcuartvar.h>

#define GENI_STATUS			0x040
#define  GENI_STATUS_M_CMD_ACTIVE	(1U << 0)
#define  GENI_STATUS_S_CMD_ACTIVE	(1U << 12)
#define GENI_UART_TX_TRANS_LEN		0x270
#define GENI_M_CMD0			0x600
#define  GENI_M_CMD0_OPCODE_UART_START_TX (1U << 27)
#define GENI_M_IRQ_STATUS		0x610
#define GENI_M_IRQ_EN			0x614
#define GENI_M_IRQ_CLEAR		0x618
#define  GENI_M_IRQ_CMD_DONE		(1U << 0)
#define  GENI_M_IRQ_TX_FIFO_WATERMARK	(1U << 30)
#define  GENI_M_IRQ_SEC_IRQ		(1U << 31)
#define GENI_S_CMD0			0x630
#define  GENI_S_CMD0_OPCODE_UART_START_RX (1U << 27)
#define GENI_S_IRQ_STATUS		0x640
#define GENI_S_IRQ_EN			0x644
#define GENI_S_IRQ_CLEAR		0x648
#define  GENI_S_IRQ_RX_FIFO_WATERMARK	(1U << 26)
#define  GENI_S_IRQ_RX_FIFO_LAST	(1U << 27)
#define GENI_TX_FIFO			0x700
#define GENI_RX_FIFO			0x780
#define GENI_TX_FIFO_STATUS		0x800
#define  GENI_TX_FIFO_STATUS_WC_MASK	0xfffffff
#define GENI_RX_FIFO_STATUS		0x804
#define  GENI_RX_FIFO_STATUS_WC_MASK	0x1ffffff
#define GENI_TX_FIFO_WATERMARK		0x80c

#define GENI_SPACE			0x1000

#define QCUART_TX_WATERMARK		2

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

cdev_decl(com);
cdev_decl(qcuart);

#define DEVUNIT(x)	(minor(x) & 0x7f)
#define DEVCUA(x)	(minor(x) & 0x80)

struct cdevsw qcuartdev = cdev_tty_init(3, qcuart);

struct cfdriver qcuart_cd = {
	NULL, "qcuart", DV_TTY
};

bus_space_tag_t	qcuartconsiot;
bus_space_handle_t qcuartconsioh;

struct qcuart_softc *qcuart_sc(dev_t);

void	qcuart_softintr(void *);
void	qcuart_start(struct tty *);

int	qcuartcnattach(bus_space_tag_t, bus_addr_t);
int	qcuartcngetc(dev_t);
void	qcuartcnputc(dev_t, int);
void	qcuartcnpollc(dev_t, int);

void
qcuart_attach_common(struct qcuart_softc *sc, int console)
{
	int maj;

	if (console) {
		/* Locate the major number. */
		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == qcuartopen)
				break;
		cn_tab->cn_dev = makedev(maj, sc->sc_dev.dv_unit);
		printf(": console");
	}

	/* Disable all interrupts. */
	HWRITE4(sc, GENI_M_IRQ_EN, 0);
	HWRITE4(sc, GENI_S_IRQ_EN, 0);

	sc->sc_si = softintr_establish(IPL_TTY, qcuart_softintr, sc);
	if (sc->sc_si == NULL) {
		printf(": can't establish soft interrupt\n");
		return;
	}

	printf("\n");
}

void
qcuart_tx_intr(struct qcuart_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	
	if (ISSET(tp->t_state, TS_BUSY)) {
		CLR(tp->t_state, TS_BUSY | TS_FLUSH);
		if (sc->sc_halt > 0)
			wakeup(&tp->t_outq);
		(*linesw[tp->t_line].l_start)(tp);
	}
}

void
qcuart_rx_intr(struct qcuart_softc *sc)
{
	uint32_t stat;
	u_char c;
	int *p;

	p = sc->sc_ibufp;
	for (;;) {
		stat = HREAD4(sc, GENI_RX_FIFO_STATUS);
		if ((stat & GENI_RX_FIFO_STATUS_WC_MASK) == 0)
			break;

		c = HREAD4(sc, GENI_RX_FIFO);
		if (p >= sc->sc_ibufend)
			sc->sc_floods++;
		else
			*p++ = c;
	}
	if (sc->sc_ibufp != p) {
		sc->sc_ibufp = p;
		softintr_schedule(sc->sc_si);
	}
}

int
qcuart_intr(void *arg)
{
	struct qcuart_softc *sc = arg;
	struct tty *tp = sc->sc_tty;
	uint32_t m_stat, s_stat;

	m_stat = HREAD4(sc, GENI_M_IRQ_STATUS);
	s_stat = HREAD4(sc, GENI_S_IRQ_STATUS);
	HWRITE4(sc, GENI_M_IRQ_CLEAR, m_stat);
	HWRITE4(sc, GENI_S_IRQ_CLEAR, s_stat);
	m_stat &= HREAD4(sc, GENI_M_IRQ_EN);
	
	if (tp == NULL)
		return 0;

	if (m_stat & GENI_M_IRQ_CMD_DONE)
		qcuart_tx_intr(sc);

	if (m_stat & GENI_M_IRQ_SEC_IRQ)
		qcuart_rx_intr(sc);

	return m_stat ? 1 : 0;
}

void
qcuart_softintr(void *arg)
{
	struct qcuart_softc *sc = arg;
	struct tty *tp = sc->sc_tty;
	int *ibufp, *ibufend;
	int s;

	if (sc->sc_ibufp == sc->sc_ibuf)
		return;

	s = spltty();

	ibufp = sc->sc_ibuf;
	ibufend = sc->sc_ibufp;

	if (ibufp == ibufend) {
		splx(s);
		return;
	}

	sc->sc_ibufp = sc->sc_ibuf = (ibufp == sc->sc_ibufs[0]) ?
	    sc->sc_ibufs[1] : sc->sc_ibufs[0];
	sc->sc_ibufhigh = sc->sc_ibuf + QCUART_IHIGHWATER;
	sc->sc_ibufend = sc->sc_ibuf + QCUART_IBUFSIZE;

	if (tp == NULL || !ISSET(tp->t_state, TS_ISOPEN)) {
		splx(s);
		return;
	}

	splx(s);

	while (ibufp < ibufend) {
		int i = *ibufp++;
#ifdef DDB
		if (tp->t_dev == cn_tab->cn_dev) {
			int j = db_rint(i);

			if (j == 1)	/* Escape received, skip */
				continue;
			if (j == 2)	/* Second char wasn't 'D' */
				(*linesw[tp->t_line].l_rint)(27, tp);
		}
#endif
		(*linesw[tp->t_line].l_rint)(i, tp);
	}
}

int
qcuart_param(struct tty *tp, struct termios *t)
{
	struct qcuart_softc *sc = qcuart_sc(tp->t_dev);
	int ospeed = t->c_ospeed;

	/* Check requested parameters. */
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
	case CS6:
	case CS7:
		return EINVAL;
	case CS8:
		break;
	}

	if (ospeed != 0) {
		while (ISSET(tp->t_state, TS_BUSY)) {
			int error;

			sc->sc_halt++;
			error = ttysleep(tp, &tp->t_outq,
			    TTOPRI | PCATCH, "qcuprm");
			sc->sc_halt--;
			if (error) {
				qcuart_start(tp);
				return error;
			}
		}
	}

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	/* Just to be sure... */
	qcuart_start(tp);
	return 0;
}

void
qcuart_start(struct tty *tp)
{
	struct qcuart_softc *sc = qcuart_sc(tp->t_dev);
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		goto out;
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_TTSTOP) || sc->sc_halt > 0)
		goto out;
	ttwakeupwr(tp);
	if (tp->t_outq.c_cc == 0)
		goto out;
	SET(tp->t_state, TS_BUSY);

	/* Enable Tx completion interrupts. */
	HSET4(sc, GENI_M_IRQ_EN, GENI_M_IRQ_CMD_DONE);

	/* Send a single character. */
	HWRITE4(sc, GENI_UART_TX_TRANS_LEN, 1);
	HWRITE4(sc, GENI_M_CMD0, GENI_M_CMD0_OPCODE_UART_START_TX);
	HWRITE4(sc, GENI_TX_FIFO, getc(&tp->t_outq));

out:
	splx(s);
}

int
qcuartopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct qcuart_softc *sc = qcuart_sc(dev);
	struct tty *tp;
	int error;
	int s;

	if (sc == NULL)
		return ENXIO;

	s = spltty();
	if (sc->sc_tty == NULL)
		tp = sc->sc_tty = ttymalloc(0);
	else
		tp = sc->sc_tty;
	splx(s);

	tp->t_oproc = qcuart_start;
	tp->t_param = qcuart_param;
	tp->t_dev = dev;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed =
		    sc->sc_conspeed ? sc->sc_conspeed : B115200;
		
		s = spltty();

		qcuart_param(tp, &tp->t_termios);
		ttsetwater(tp);

		sc->sc_ibufp = sc->sc_ibuf = sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + QCUART_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + QCUART_IBUFSIZE;

		/* Enable Rx interrupts. */
		HSET4(sc, GENI_S_IRQ_EN,
		    GENI_S_IRQ_RX_FIFO_WATERMARK | GENI_S_IRQ_RX_FIFO_LAST);
		HSET4(sc, GENI_M_IRQ_EN, GENI_M_IRQ_SEC_IRQ);

		/* Start Rx engine. */
		HWRITE4(sc, GENI_S_CMD0, GENI_S_CMD0_OPCODE_UART_START_RX);

		/* No carrier detect support. */
		SET(tp->t_state, TS_CARR_ON);
	} else if (ISSET(tp->t_state, TS_XCLUDE) && suser(p) != 0)
		return EBUSY;
	else
		s = spltty();

	if (DEVCUA(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return EBUSY;
		}
		sc->sc_cua = 1;		/* We go into CUA mode. */
	} else {
		if (ISSET(flag, O_NONBLOCK) && sc->sc_cua) {
			/* Opening TTY non-blocking... but the CUA is busy. */
			splx(s);
			return EBUSY;
		} else {
			while (sc->sc_cua) {
				SET(tp->t_state, TS_WOPEN);
				error = ttysleep(tp, &tp->t_rawq,
				    TTIPRI | PCATCH, ttopen);
				/*
				 * If TS_WOPEN has been reset, that means the
				 * cua device has been closed.
				 * We don't want to fail in that case,
				 * so just go around again.
				 */
				if (error && ISSET(tp->t_state, TS_WOPEN)) {
					CLR(tp->t_state, TS_WOPEN);
					splx(s);
					return error;
				}
			}
		}
	}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp, p);
}

int
qcuartclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct qcuart_softc *sc = qcuart_sc(dev);
	struct tty *tp = sc->sc_tty;
	int s;

	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag, p);
	s = spltty();
	if (!ISSET(tp->t_state, TS_WOPEN)) {
		/* Disable interrupts. */
		HWRITE4(sc, GENI_M_IRQ_EN, 0);
		HWRITE4(sc, GENI_S_IRQ_EN, 0);
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);
	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);

	return 0;
}

int
qcuartread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = qcuarttty(dev);

	if (tp == NULL)
		return ENODEV;
	
	return (*linesw[tp->t_line].l_read)(tp, uio, flag);
}

int
qcuartwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = qcuarttty(dev);

	if (tp == NULL)
		return ENODEV;
	
	return (*linesw[tp->t_line].l_write)(tp, uio, flag);
}

int
qcuartioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct qcuart_softc *sc = qcuart_sc(dev);
	struct tty *tp;
	int error;

	if (sc == NULL)
		return ENODEV;

	tp = sc->sc_tty;
	if (tp == NULL)
		return ENXIO;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch(cmd) {
	case TIOCSBRK:
	case TIOCCBRK:
	case TIOCSDTR:
	case TIOCCDTR:
	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMGET:
	case TIOCGFLAGS:
		break;
	case TIOCSFLAGS:
		error = suser(p);
		if (error != 0)
			return EPERM;
		break;
	default:
		return ENOTTY;
	}

	return 0;
}

int
qcuartstop(struct tty *tp, int flag)
{
	return 0;
}

struct tty *
qcuarttty(dev_t dev)
{
	struct qcuart_softc *sc = qcuart_sc(dev);

	if (sc == NULL)
		return NULL;
	return sc->sc_tty;
}

struct qcuart_softc *
qcuart_sc(dev_t dev)
{
	int unit = DEVUNIT(dev);

	if (unit >= qcuart_cd.cd_ndevs)
		return NULL;
	return (struct qcuart_softc *)qcuart_cd.cd_devs[unit];
}

int
qcuartcnattach(bus_space_tag_t iot, bus_addr_t iobase)
{
	static struct consdev qcuartcons = {
		NULL, NULL, qcuartcngetc, qcuartcnputc, qcuartcnpollc, NULL,
		NODEV, CN_MIDPRI
	};
	int maj;

	qcuartconsiot = iot;
	if (bus_space_map(iot, iobase, GENI_SPACE, 0, &qcuartconsioh))
		return ENOMEM;

	/* Look for major of com(4) to replace. */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == comopen)
			break;
	if (maj == nchrdev)
		return ENXIO;

	cn_tab = &qcuartcons;
	cn_tab->cn_dev = makedev(maj, 0);
	cdevsw[maj] = qcuartdev; 	/* KLUDGE */

	return 0;
}

int
qcuartcngetc(dev_t dev)
{
	bus_space_tag_t iot = qcuartconsiot;
	bus_space_handle_t ioh = qcuartconsioh;
	uint32_t stat;
	uint8_t c;

	bus_space_write_4(iot, ioh, GENI_S_CMD0,
	    GENI_S_CMD0_OPCODE_UART_START_RX);

	stat = bus_space_read_4(iot, ioh, GENI_M_IRQ_STATUS);
	bus_space_write_4(iot, ioh, GENI_M_IRQ_CLEAR, stat);
	stat = bus_space_read_4(iot, ioh, GENI_S_IRQ_STATUS);
	bus_space_write_4(iot, ioh, GENI_S_IRQ_CLEAR, stat);

	for (;;) {
		stat = bus_space_read_4(iot, ioh, GENI_RX_FIFO_STATUS);
		if (stat & GENI_RX_FIFO_STATUS_WC_MASK)
			break;
		CPU_BUSY_CYCLE();
	}

	c = bus_space_read_4(iot, ioh, GENI_RX_FIFO);
	return c;
}

void
qcuartcnputc(dev_t dev, int c)
{
	bus_space_tag_t iot = qcuartconsiot;
	bus_space_handle_t ioh = qcuartconsioh;
	uint32_t stat;

	bus_space_write_4(iot, ioh, GENI_TX_FIFO_WATERMARK,
	    QCUART_TX_WATERMARK);

	bus_space_write_4(iot, ioh, GENI_UART_TX_TRANS_LEN, 1);
	bus_space_write_4(iot, ioh, GENI_M_CMD0,
	    GENI_M_CMD0_OPCODE_UART_START_TX);

	for (;;) {
		stat = bus_space_read_4(iot, ioh, GENI_M_IRQ_STATUS);
		if (stat & GENI_M_IRQ_TX_FIFO_WATERMARK)
			break;
		CPU_BUSY_CYCLE();
	}
	bus_space_write_4(iot, ioh, GENI_TX_FIFO, c);

	bus_space_write_4(iot, ioh, GENI_M_IRQ_CLEAR,
	    GENI_M_IRQ_TX_FIFO_WATERMARK);

	for (;;) {
		stat = bus_space_read_4(iot, ioh, GENI_M_IRQ_STATUS);
		if (stat & GENI_M_IRQ_CMD_DONE)
			break;
		CPU_BUSY_CYCLE();
	}
	bus_space_write_4(iot, ioh, GENI_M_IRQ_CLEAR, GENI_M_IRQ_CMD_DONE);
}

void
qcuartcnpollc(dev_t dev, int on)
{
}
