/*	$OpenBSD: rl2subr.c,v 1.2 1999/06/23 04:48:49 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * Low level card protocol access to the Proxim RangeLAN2 wireless 
 * network adaptor.
 *
 * Information and ideas gleaned from 
 *   - disassembly of Dave Koberstein's <davek@komacke.com> Linux driver 
 *     (which is built with Proxim source),
 *   - Yoichi Shinoda's <shinoda@cs.washington.edu> BSDI driver, and
 *   - Geoff Voelker's <voelker@cs.washington.edu> Linux port of the same.
 */

#include <sys/param.h> 
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <net/if.h>
        
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/rl2.h>
#include <dev/ic/rl2var.h>
#include <dev/ic/rl2reg.h>
#include <dev/ic/rl2cmd.h>

static int	rl2_tx_request __P((struct rl2_softc *, u_int16_t));
static int	rl2_tx_end __P((struct rl2_softc *));

/*
 * Disables or enables interrupts from the card. Returns the old 
 * interrupt-enable state.
 */
int
rl2_enable(sc, enable)
	struct rl2_softc * sc;
	int		enable;
{
	int		s;
	int		was_enabled;

	s = splhigh();
	was_enabled = (sc->sc_intsel & RL2_INTSEL_ENABLE) ? 1 : 0;
	if (enable != was_enabled) {
		if (enable)
			sc->sc_intsel |= 0x10;
		else
			sc->sc_intsel &=~0x10;
		_rl2_register_write_1(sc, RL2_REG_INTSEL, sc->sc_intsel);
	}
	splx(s);
	return (was_enabled);
}

/*
 * Perform a hard reset of the card.  Determines bus width (8 or
 * 16 bit), if sc->sc_width is unset.  Returns 0 on success.
 * Note: takes about 200ms at splhigh, meaning this is an expensive call,
 * but normal (error-free) operation of the card will not need more than
 * two resets - one at probe time, and the other when the interface is
 * brought up.
 */
int
rl2_reset(sc)
	struct rl2_softc * sc;
{
	int		s;
	int		i;
	int		status;
	u_int8_t	op = 0x00;

	s = splhigh();
	dprintf(" R[");
	if (sc->sc_cardtype & (RL2_CTYPE_UISA | RL2_CTYPE_ONE_PIECE))
		op = 0x04;
	if (rl2_status_read(sc) & RL2_STATUS_WAKEUP) {
		rl2_control_write(sc, op);
		rl2_control_write(sc, op | RL2_CONTROL_RESET);
		dprintf(" 7ms");
		DELAY(7000);
		rl2_control_write(sc, op);
		dprintf(" 7ms");
		DELAY(7000);
	}
	rl2_control_write(sc, op);
	rl2_control_write(sc, op);
	rl2_control_write(sc, op | RL2_CONTROL_BIT3);
	dprintf(" 67ms");
	DELAY(67000);
	rl2_status_write(sc, 0x00);
	if (sc->sc_cardtype & (RL2_CTYPE_UISA | RL2_CTYPE_ONE_PIECE))
		rl2_control_write(sc, 0x38); 
		/* RL2_CONTROL_BIT3 | RL2_CONTROL_RESET | RL2_CONTROL_16BIT */
	else
		rl2_control_write(sc, 0x2c);
		/* RL2_CONTROL_BIT3 | RL2_CONTROL_BIT2  | RL2_CONTROL_16BIT */
	dprintf(" 67ms");
	DELAY(67000);
	rl2_data_write_2(sc, 0xaa55);
	rl2_status_write(sc, 0x5a);
	splx(s);
	for (i = 0; i < 2000; i++) {		/* Proxim says 200 not 2000. */
		if ((status = rl2_status_read(sc)) == 0x5a)
			break;
		DELAY(1000);
	}
	dprintf(" (%dms)", i);
	s = splhigh();
	if (status != 0x5a) {
		splx(s);
		printf("%s: reset timeout\n", sc->sc_dev.dv_xname);
		dprintf("]=-1");
		return (-1);
	}
	if (sc->sc_width == 8) {
		if (sc->sc_cardtype & (RL2_CTYPE_UISA | RL2_CTYPE_ONE_PIECE))
			rl2_control_write(sc, RL2_CONTROL_BIT3);
		else
			rl2_control_write(sc, RL2_CONTROL_BIT3 | 
			    RL2_CONTROL_BIT2);
		rl2_data_write_1(sc, 0x20);
	} else if (sc->sc_width == 16) {
		rl2_data_write_2(sc, 0x0000);
	} else {
		if (rl2_data_read_2(sc) == 0x55aa) {
			rl2_data_write_2(sc, 0x0000);
			sc->sc_width = 16;
		} else {
			if (sc->sc_cardtype & (RL2_CTYPE_UISA | 
			    RL2_CTYPE_ONE_PIECE))
				rl2_control_write(sc, RL2_CONTROL_BIT3);
			else
				rl2_control_write(sc, RL2_CONTROL_BIT3 | 
				    RL2_CONTROL_BIT2);
			rl2_data_write_1(sc, 0x20);
			sc->sc_width = 8;
		}
		printf("%s: %d bit bus\n", sc->sc_dev.dv_xname, sc->sc_width);
	}
	rl2_status_write(sc, 0x00);
	sc->sc_intsel = 0;
	rl2_intsel_write(sc, sc->sc_irq);
	splx(s);
	dprintf("]");
	return (0);
}

/*
 * Sets the new 'wakeup' state. Returns the old wakeup state.
 * The special state value RL2_WAKEUP_SET should be used to wake the 
 * card up. The card can be partially put to sleep (presumably to save 
 * power) by sending it the 'Standby' command.
 */
u_int8_t
rl2_wakeup(sc, wnew)
	struct rl2_softc *	sc;
	u_int8_t		wnew;
{
	u_int8_t		wold, s;
	int			i;

	/* Save what the last-written values were. */
	wold = (sc->sc_status & 0x80) | (sc->sc_control & 0x10);

	if (wnew == RL2_WAKEUP_SET) {
		/* SetWakeupBit() */
		dprintf(" Ws[");
		rl2_status_set(sc, 0x80);
		if (0/*LLDInactivityTimeOut &&
		    (sc->sc_cardtype & RL2_CTYPE_OEM)*/) {
			dprintf (" 167ms");
			DELAY(167000);
		} else {
			dprintf (" .1ms");
			DELAY(100);
		}
		s = rl2_status_read(sc);
		rl2_control_set(sc, 0x10);
		if ((s & 0x80) != 0)
			for (i = 0; i < 9; i++) {
				dprintf(" 2ms");
				DELAY(2000);
				rl2_status_set(sc, 0x80);
			}
		dprintf("]");
	} else {
		/* ClearWakeupBit() */
		dprintf(" Wc[");
		if ((wnew & 0x80) == 0)
			rl2_status_clear(sc, 0x80);
		if ((wnew & 0x10) == 0)
			rl2_control_clear(sc, 0x10);
		dprintf("]");
	}
	return (wold);
}

/*
 * Performs the first (request) stage of transmitting a command message 
 * to the card. 'len' is the expected length of the message is needed.
 * Returns: 0 on success
 *          1 on timeout
 *          2 on NAK (card busy, and will need a rl2_clear_nak() after 100ms)
 */
static int
rl2_tx_request(sc, len)
	struct rl2_softc *	sc;
	u_int16_t		len;
{
	/* TxRequest() */
	int			s;
	int			i;
	u_int8_t		status;

	/* u_int8_t w; */
	/* w = rl2_wakeup(sc, RL2_WAKEUP_SET); */

	dprintf(" Tr[");
	if (sc->sc_width == 16) {
		rl2_status_tx_write(sc, 0x01);
		rl2_data_write_2(sc, len);
		rl2_status_tx_int(sc);

		s = spl0();
		for (i = 0; i < 600; i++) {
			status = rl2_status_tx_read(sc);
			if (status == 0x02 || status == 0x05)
				break;
			DELAY(1000);
		}
		splx(s);
		dprintf(" %dms", i);
		if (status == 0x02)
			goto success;
		if (status == 0x05)
			goto error;
	} else if (sc->sc_width == 8) {
		rl2_status_tx_write(sc, 0x06);
		rl2_data_write_1(sc, len & 0x00ff);
		rl2_status_tx_int(sc);
		s = spl0();
		for (i = 0; i < 6800; i++) {
			status = rl2_status_tx_read(sc);
			if (status == 0x07)
				break;
			DELAY(1000);
		}
		splx(s);
		dprintf(" %dms", i);
		if (status == 0x07) {
			rl2_data_write_1(sc, (len & 0xff00) >> 8);
			rl2_status_tx_write(sc, 0x01);
			s = spl0();
			for (i = 0; i < 600; i++) {
				status = rl2_status_tx_read(sc);
				if (status == 0x02 || status == 0x05)
					break;
				DELAY(1000);
			}
			splx(s);
			dprintf(" %dms", i);
			if (status == 0x02)
				goto success;
			if (status == 0x05)
				goto error;
		}
	}
#ifdef DIAGNOSTIC
	else
		panic("rln: bus width");
#endif

	printf("%s: tx_request timed out, status 0x%02x", 
	    sc->sc_dev.dv_xname, status);
	dprintf("]=(1)");
	return (1);

error:
	/* XXX Will need to clear nak within 100 ms. */
	dprintf("]=2");
#ifdef DIAGNOSTIC
	printf("%s: tx protocol fault (nak)\n", sc->sc_dev.dv_xname);
#endif
	return (2);

success:
	/* rl2_wakeup(sc, w); */
	dprintf("]=0");
	return (0);
}

/*
 * Performs the third (and final) stage of transmitting a command
 * message to the card.
 * Returns: 0 on command success.
 *          non-zero on failure (card will need reset)
 */
static int
rl2_tx_end(sc)
	struct rl2_softc * sc;
{
	/* EndOfTx() */
	int		i;
	int		s;
	u_int8_t	status;

	dprintf(" Te[");
	s = spl0();
	for (i = 0; i < 600; i++) {
		status = rl2_status_tx_read(sc);
		if (status == 0x03)
			break;
		DELAY(1000);
	}
	splx(s);
	if (status == 0x03) {
		rl2_status_tx_write(sc, 0x00);
		dprintf("]=0");
		return (0);
	} else {
		printf("%s: tx cmd failed (%02x)\n", sc->sc_dev.dv_xname,
		    status);
		/* XXX Needs reset? */
		dprintf("]=-1");
		return (-1);
	}
}

/*
 * Performs first (request) stage of receiving a message from the card.
 * Returns: 0 on failure,
 *          n>0 on success, where 'n' is the length of the message
 */

int
rl2_rx_request(sc, timeo)
	struct rl2_softc *	sc;
	int 			timeo;	/* milliseconds */
{
	/* RxRequest */
	int			s;
	int			len = 0;
	int			i;
	u_int8_t		status;
	u_int8_t		hi, lo;

	dprintf(" Rr[");
	status = rl2_status_rx_read(sc);

	/* Short wait for states 1|5|6. */
	s = spl0();
	for (i = 0; i < timeo; i++) {
		if (status == 0x60 || status == 0x10 || status == 0x50)
			break;
		DELAY(1000);
		status = rl2_status_rx_read(sc);
	}
	splx(s);
	dprintf(" (%dms)",i);

	if (sc->sc_width == 16) {
		if (status != 0x10)
			goto badstatus_quiet;
		/* Read 2 octets. */
		len = rl2_data_read_2(sc);
	} else if (sc->sc_width == 8) {
		if (status != 0x60)
			goto badstatus_quiet;
		/* Read low octet. */
		lo = rl2_data_read_1(sc);
		rl2_status_rx_write(sc, 0x70);
		rl2_status_rx_int(sc);
		s = spl0();
		for (i = 0; i < 600; i++) {
			status = rl2_status_rx_read(sc);
			if (status == 0x10)
				break;
			DELAY(1000);
		}
		splx(s);
		if (status != 0x10)
			goto badstatus;
		/* Read high octet. */
		hi = rl2_data_read_1(sc);
		len = lo | (hi << 8);
	}
#ifdef DIAGNOSTIC
	else
		panic("rl2: bus width %d", sc->sc_width);
#endif

	dprintf(" len=%d]", len);
	return (len);

badstatus:
	printf("%s: rx_request timed out, status %02x\n", 
	    sc->sc_dev.dv_xname, status);
badstatus_quiet:
	if (status == 0x50)
		printf("%s: rx protocol error (nak)\n", sc->sc_dev.dv_xname);
	dprintf("]");
	return (-1);
}

/* Performs part of the second (transfer) stage of receiving a data message. */
void
rl2_rx_pdata(sc, buf, len, pd)
	struct rl2_softc *	sc;
	void *			buf;
	int			len;
	struct rl2_pdata *	pd;
{
	char *			data = (char *)buf;

	if (pd->p_nremain) {
		*data++ = pd->p_data;
		if (--len == 0)
			return;
	}

	pd->p_nremain = 0;

	if (sc->sc_width == 16) {
		/* Round down to the closest even multiple. */
		rl2_data_read_multi_2(sc, data, len / 2);
#ifdef RL2DEBUG_REG
		dprintf(" D>"); 
		dprinthex(data, len);
#endif
		if (len & 1) {
			/* Read the last octet plus a bit extra. */
			union {
				u_int16_t w;
				u_int8_t  b[2];
			} u;

			u.w = rl2_data_read_2(sc);
			data[len - 1] = u.b[0];
			pd->p_data = u.b[1];
			pd->p_nremain = 1;
#ifdef RL2DEBUG_REG
			dprintf(" D>{%02x%02x}", u.b[0], u.b[1]); 
#endif
		}
	} else if (sc->sc_width == 8) {
		rl2_data_read_multi_1(sc, data, len);
#ifdef RL2DEBUG_REG
		dprintf(" D>"); 
		dprinthex(data, len);
#endif
		if (len & 1) {
			/* Must read multiples of two. */
			pd->p_data = rl2_data_read_1(sc);
			pd->p_nremain = 1;
#ifdef RL2DEBUG_REG
			dprintf(" D>{%02x}", pd->p_data); 
#endif
		}
	}

}

int
rl2_rx_data(sc, buf, len)
	struct rl2_softc *	sc;
	void *			buf;
	int			len;
{
	/* RxData() */
	struct rl2_pdata	pd = { 0, 0 };
	int			s;
	int			i;
	u_int8_t		status;

	dprintf(" Rd[");
	rl2_status_rx_write(sc, 0x20);
	rl2_status_rx_int(sc);
	s = spl0();
	for (i = 0; i < 600; i++) {
		status = rl2_status_rx_read(sc);
		if (status == 0x40)
			break;
		DELAY(1000);
	}
	splx(s);
	if (status != 0x40) {
		dprintf("]=-1");
		return (-1);
	}

	rl2_rx_pdata(sc, buf, len, &pd);
#ifdef DIAGNOSTIC
	/* We should have nothing left over. */
	if (pd.p_nremain || len & 1)
		panic("rl2_rx_data: leftover");
#endif

	dprintf("]=0");
	return (0);
}

void
rl2_rx_end(sc)
	struct rl2_softc * sc;
{
	/* EndOfRx() */

	dprintf(" Re[");
	rl2_status_rx_write(sc, 0x30);
	rl2_status_rx_int(sc);
	/* rl2_wakeup(sc, 0); */
	dprintf("]");
}

/* Clear a transmission NAK from the card. */
void
rl2_clear_nak(sc)
	struct rl2_softc * sc;
{
	/* ClearNAK() */

	rl2_status_tx_write(sc, 0x08);
	rl2_status_tx_int(sc);
}

/*
 * Send a command message to the card. Returns;
 *	2: NAK
 *	-1: failure
 *	0: success
 */
int
rl2_msg_tx_start(sc, buf, pktlen, state)
	struct rl2_softc *	sc;
	void *			buf;
	int			pktlen;
	struct rl2_msg_tx_state * state;
{
	struct rl2_mm_cmd *	cmd = (struct rl2_mm_cmd *)buf;
	int			ret;

	state->ien = rl2_enable(sc, 0);
	state->pd.p_nremain = 0;

	if (!(cmd->cmd_letter == 'A' && cmd->cmd_fn == 6)) 	/* Standby. */
		state->w = rl2_wakeup(sc, RL2_WAKEUP_SET); 
	else
		state->w = RL2_WAKEUP_NOCHANGE;

	ret = rl2_tx_request(sc, pktlen);
	if (ret == 2) {
		rl2_clear_nak(sc);
		if (sc->sc_cardtype & RL2_CTYPE_OEM) {
			/* XXX Needs reset? */
		}
		ret = 2;
	}
	else if (ret == 1) {
		/* Timeout. */
		rl2_status_tx_write(sc, 0x04);
		ret = -1;
	}
	return (ret);
}

void
rl2_msg_tx_data(sc, buf, len, state)
	struct rl2_softc *	sc;
	void *			buf;
	u_int16_t		len;
	struct rl2_msg_tx_state * state;
{
	char *			data = (char *)buf;

	if (sc->sc_width == 16 && state->pd.p_nremain) {
		/* XXX htons() needed? */
		union {
			u_int8_t  b[2];
			u_int16_t w;
		} u;

		u.b[0] = state->pd.p_data;
		if (len) {
			u.b[1] = *data++;
			len--;
		} else
			u.b[1] = '\0';
#ifdef RL2DEBUG_REG
		dprintf(" D<%02x%02x", u.b[0], u.b[1]);
#endif
		rl2_data_write_2(sc, u.w);
		state->pd.p_nremain = 0;
	} 

	if (len) {
		if (sc->sc_width == 16) {
			if (len >= 2)
				rl2_data_write_multi_2(sc, buf, len / 2);
			if (len & 1) {
				state->pd.p_nremain = 1;
				state->pd.p_data = data[len - 1];
			}
		} else if (sc->sc_width == 8)
			rl2_data_write_multi_1(sc, buf, len);
#ifdef DIAGNOSTIC
		else
			panic("rl2_msg_tx_data width %d", sc->sc_width);
#endif
#ifdef RL2DEBUG_REG
		dprintf(" D<"); 
		dprinthex(data, len);
#endif
	}
}


int
rl2_msg_tx_end(sc, state)
	struct rl2_softc *	sc;
	struct rl2_msg_tx_state * state;
{
	int			ret;

	/* Flush the tx buffer. */
	if (state->pd.p_nremain)
		rl2_msg_tx_data(sc, NULL, 0, state);

#ifdef DIAGNOSTIC
	if (state->pd.p_nremain)
		panic("rl2_msg_tx_end remain %d", state->pd.p_nremain);
#endif
	ret = rl2_tx_end(sc);
	if (sc->sc_arpcom.ac_if.if_flags & IFF_OACTIVE) {
		state->w |= 0x10 | 0x80;
	}
	rl2_wakeup(sc, state->w);
	rl2_enable(sc, state->ien);
	return (ret);
}

/* Return the next unique sequence number to use for a transmitted command */
u_int8_t
rl2_newseq(sc)
	struct rl2_softc * sc;
{
	int s;
	u_int8_t seq;

	s = splhigh();
	seq = sc->sc_pktseq++;
	if (sc->sc_pktseq > 0x7c)
		sc->sc_pktseq = 0;
	splx(s);
	return (seq);
}

/*
 * Transmit a command message to, and (optionally) receive a response
 * message from the card.  Each transmitted message has a sequence
 * number, and corresponding reply messages have the same sequence
 * number.  We use the sequence numbers to index the mailboxes so
 * that rl2softintr() can signal this routine when it has serviced
 * and correctly received a response.
 */

int
rl2_msg_txrx(sc, tx, txlen, rx, rxlen)
	struct rl2_softc *	sc;
	void *			tx;
	int 			txlen;
	void *			rx;
	int			rxlen;
{
	struct rl2_mm_cmd *	txc = (struct rl2_mm_cmd *)tx;
	struct rl2_mm_cmd *	rxc = (struct rl2_mm_cmd *)rx;
	struct rl2_msg_tx_state state;
	int			ien;
	int			ret;

#ifdef DIAGNOSTIC
	if (rx != NULL && rxlen < sizeof *rxc)
		panic("rl2_msg_txrx");
#endif

	txc->cmd_seq = rl2_newseq(sc);

#ifdef RL2DUMP
	printf("%s: send %c%d seq %d data ", sc->sc_dev.dv_xname, 
	    txc->cmd_letter, txc->cmd_fn, txc->cmd_seq);
	RL2DUMPHEX(txc, sizeof *txc);
	printf(":");
	RL2DUMPHEX((char *)tx + sizeof *txc, txlen - sizeof *txc);
	printf("\n");
#endif

	if (rx != NULL)
		if (rl2_mbox_create(sc, txc->cmd_seq, rx, rxlen) < 0)
			/* Mailbox collision. */
			return (-1);

	/* Start the transfer. */
	if ((ret = rl2_msg_tx_start(sc, tx, txlen, &state))) {
		if (rx != NULL)
			rl2_mbox_wait(sc, txc->cmd_seq, -1);
		return (ret);
	}

	/* Always send an even number of octets. */
	rl2_msg_tx_data(sc, tx, (txlen + 1) & ~1, &state);

	/* End the transmission. */
	if ((ret = rl2_msg_tx_end(sc, &state))) {
		/* Destroy mailbox. */
		if (rx != NULL)
			rl2_mbox_wait(sc, txc->cmd_seq, -1);
		return (ret);
	}

	/* Don't wait for reply if there is nowhere to put it. */
	if (rx == NULL)
		return (0);

	/* Enable interrupts if not already. */
	ien = rl2_enable(sc, 1);

	/* Wait for the reply message. */
	if (rl2_mbox_wait(sc, txc->cmd_seq, 2000) <= 0) {
		printf("%s: lost message %c%d seq %d\n", sc->sc_dev.dv_xname,
			txc->cmd_letter, txc->cmd_fn, txc->cmd_seq);
		rl2_enable(sc, ien);
		return (-1);
	}
	rl2_enable(sc, ien);

#ifdef RL2DUMP
	printf("%s: recv %c%d seq %d data ", sc->sc_dev.dv_xname, 
	    rxc->cmd_letter, rxc->cmd_fn, rxc->cmd_seq);
	RL2DUMPHEX(rxc, sizeof *rxc);
	printf(":");
	RL2DUMPHEX(((char *)rx) + sizeof *rxc, rxlen - sizeof *rxc);
	printf("\n");
#endif

	/* Check for errors in the received message. */
	if (rxc->cmd_error & 0x80) {
		printf("%s: command error 0x%02x command %c%d\n",
			sc->sc_dev.dv_xname,
			rxc->cmd_error & 0x7f,
			rxc->cmd_letter, rxc->cmd_fn);
		return (-1);
	}

	return (0);
}

/*
 * Mailboxes provide a simple way to tell the interrupt
 * service routine that someone is expecting a reply message.
 * Mailboxes are identified by the message sequence number
 * and also hold a pointer to storage supplied by the waiter.
 */

/* Create a mailbox for filling. */
int
rl2_mbox_create(sc, seq, buf, len)
	struct rl2_softc *	sc;
	u_int8_t		seq;
	void *			buf;
	size_t			len;
{
	int			s;
	struct rl2_mbox *	mb = &sc->sc_mbox[seq];

	dprintf(" <create %d", seq);

#ifdef DIAGNOSTIC
	if (seq > RL2_NMBOX)
		panic("mbox create");
#endif

	s = splhigh();
	if (mb->mb_state != RL2MBOX_VOID) {
#ifdef DIAGNOSTIC
		printf("mbox collision");
#endif
		splx(s);
		return (-1);
	}
	mb->mb_buf = buf;
	mb->mb_len = len;
	mb->mb_actlen = 0;
	mb->mb_state = RL2MBOX_EMPTY;
	dprintf(" empty>");
	splx(s);
	return (0);
}


/* Wait for a mailbox to be filled. */
int
rl2_mbox_wait(sc, seq, timeo)
	struct rl2_softc *	sc;
	u_int8_t		seq;
	int			timeo;
{
	int			i;
	int			s;
	int			ret;
	volatile struct rl2_mbox * mb = &sc->sc_mbox[seq];
	extern int		cold;

	dprintf(" <wait %d", seq);

#ifdef DIAGNOSTIC
	if (seq > RL2_NMBOX)
		panic("mbox wait");
#endif
	if (cold) {
		/* Autoconfiguration - spin at spl0. */
		s = spl0();
		i = 0;
		while (mb->mb_state == RL2MBOX_EMPTY && i < timeo) {
			DELAY(hz); /* 1 tick. */
			i++;
		}
		if (i)
			dprintf(" %dms", i);
		while (mb->mb_state == RL2MBOX_FILLING) 
			;
		splx(s);
	} else {
		tsleep((void *)mb, PRIBIO, "rl2mbox", timeo);
		if (mb->mb_state == RL2MBOX_FILLING)
			/* XXX Could race. */
			tsleep((void *)mb, PRIBIO, "rl2mbox", 0);
	}

	s = splhigh();
#ifdef DIAGNOSTIC
	if (mb->mb_state != RL2MBOX_EMPTY && mb->mb_state != RL2MBOX_FILLED)
		panic("mbox wait %d", mb->mb_state);
#endif
	ret = mb->mb_actlen;
	mb->mb_state = RL2MBOX_VOID;
	dprintf(" void>=%d", ret);
	splx(s);
	return (ret);
}

/* Lock a mailbox for filling. */
int
rl2_mbox_lock(sc, seq, bufp, lenp)
	struct rl2_softc *	sc;
	u_int8_t		seq;
	void **			bufp;
	size_t *		lenp;
{
	int			s;
	struct rl2_mbox *	mb = &sc->sc_mbox[seq];

	dprintf(" <lock %d", seq);

	s = splhigh();
#ifdef DIAGNOSTIC
	if (seq > RL2_NMBOX)
		panic("mbox lock");
#endif
	if (mb->mb_state != RL2MBOX_EMPTY) {
		splx(s);
		dprintf(" ?>");
		return (-1);
	}

	mb->mb_state = RL2MBOX_FILLING;
	dprintf(" filling>");
	*bufp = mb->mb_buf;
	*lenp = mb->mb_len;

	splx(s);
	return (0);
}

/* Unlock a mailbox and inform the waiter of the actual number of octets. */
void
rl2_mbox_unlock(sc, seq, actlen)
	struct rl2_softc *	sc;
	u_int8_t		seq;
	size_t			actlen;
{
	int			s;
	struct rl2_mbox *	mb = &sc->sc_mbox[seq];

	dprintf(" <unlock %d", seq);

	s = splhigh();
#ifdef DIAGNOSTIC
	if (seq > RL2_NMBOX)
		panic("mbox unlock seq");
	if (mb->mb_state != RL2MBOX_FILLING)
		panic("mbox unlock");
#endif
	mb->mb_state = RL2MBOX_FILLED;
	dprintf(" filled>");
	mb->mb_actlen = actlen;
	wakeup(mb);
	splx(s);
}

