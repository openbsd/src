/*	$OpenBSD: rlnsubr.c,v 1.1 1999/07/30 13:43:36 d Exp $	*/
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

#include <dev/ic/rln.h>
#include <dev/ic/rlnvar.h>
#include <dev/ic/rlnreg.h>
#include <dev/ic/rlncmd.h>

static int	rln_tx_request __P((struct rln_softc *, u_int16_t));
static int	rln_tx_end __P((struct rln_softc *));

/*
 * Disables or enables interrupts from the card. Returns the old 
 * interrupt-enable state.
 */
int
rln_enable(sc, enable)
	struct rln_softc * sc;
	int		enable;
{
	int		s;
	int		was_enabled;

	s = splhigh();
	was_enabled = (sc->sc_intsel & RLN_INTSEL_ENABLE) ? 1 : 0;
	if (enable != was_enabled) {
		if (enable)
			sc->sc_intsel |= RLN_INTSEL_ENABLE;
		else
			sc->sc_intsel &=~RLN_INTSEL_ENABLE;
		_rln_register_write_1(sc, RLN_REG_INTSEL, sc->sc_intsel);
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
rln_reset(sc)
	struct rln_softc * sc;
{
	int		s;
	int		i;
	int		status;
	u_int8_t	op = 0x00;

	s = splhigh();
	dprintf(" R[");
	if (sc->sc_cardtype & (RLN_CTYPE_UISA | RLN_CTYPE_ONE_PIECE))
		op = 0x04;
	if (rln_status_read(sc) & RLN_STATUS_WAKEUP) {
		rln_control_write(sc, op);
		rln_control_write(sc, op | RLN_CONTROL_RESET);
		dprintf(" 7ms");
		DELAY(7000);
		rln_control_write(sc, op);
		dprintf(" 7ms");
		DELAY(7000);
	}
	rln_control_write(sc, op);
	rln_control_write(sc, op);
	rln_control_write(sc, op | RLN_CONTROL_BIT3);
	dprintf(" 67ms");
	DELAY(67000);
	rln_status_write(sc, 0x00);
	if (sc->sc_cardtype & (RLN_CTYPE_UISA | RLN_CTYPE_ONE_PIECE))
		rln_control_write(sc, 0x38); 
		/* RLN_CONTROL_BIT3 | RLN_CONTROL_RESET | RLN_CONTROL_16BIT */
	else
		rln_control_write(sc, 0x2c);
		/* RLN_CONTROL_BIT3 | RLN_CONTROL_BIT2  | RLN_CONTROL_16BIT */
	dprintf(" 67ms");
	DELAY(67000);
	rln_data_write_2(sc, 0xaa55);
	rln_status_write(sc, 0x5a);
	splx(s);
	for (i = 0; i < 200 *5; i++) {		/* Proxim says 200. */
		if ((status = rln_status_read(sc)) == 0x5a)
			break;
		DELAY(1000);
	}
	dprintf(" (%dms)", i);
	s = splhigh();
	if (status != 0x5a) {
		splx(s);
		/* Only winge if bus width not yet probed */
		if (sc->sc_width != 0)
			printf("%s: reset timeout\n", sc->sc_dev.dv_xname);
		dprintf("]=-1");
		return (-1);
	}
	if (sc->sc_width == 8) {
		if (sc->sc_cardtype & (RLN_CTYPE_UISA | RLN_CTYPE_ONE_PIECE))
			rln_control_write(sc, RLN_CONTROL_BIT3);
		else
			rln_control_write(sc, RLN_CONTROL_BIT3 | 
			    RLN_CONTROL_BIT2);
		rln_data_write_1(sc, 0x20);
	} else if (sc->sc_width == 16) {
		rln_data_write_2(sc, 0x0000);
	} else {
		if (rln_data_read_2(sc) == 0x55aa) {
			rln_data_write_2(sc, 0x0000);
			sc->sc_width = 16;
		} else {
			if (sc->sc_cardtype & (RLN_CTYPE_UISA | 
			    RLN_CTYPE_ONE_PIECE))
				rln_control_write(sc, RLN_CONTROL_BIT3);
			else
				rln_control_write(sc, RLN_CONTROL_BIT3 | 
				    RLN_CONTROL_BIT2);
			rln_data_write_1(sc, 0x20);
			sc->sc_width = 8;
		}
		/* printf("%s: %d bit bus\n", sc->sc_dev.dv_xname, 
		   sc->sc_width); */
	}
	rln_status_write(sc, 0x00);
	sc->sc_intsel = 0;
	rln_intsel_write(sc, sc->sc_irq);
	splx(s);
	dprintf("]");
	return (0);
}

/*
 * Sets the new 'wakeup' state. Returns the old wakeup state.
 * The special state value RLN_WAKEUP_SET should be used to wake the 
 * card up. The card can be partially put to sleep (presumably to save 
 * power) by sending it the 'Standby' command.
 */
u_int8_t
rln_wakeup(sc, wnew)
	struct rln_softc *	sc;
	u_int8_t		wnew;
{
	u_int8_t		wold, s;
	int			i;

	/* Save what the last-written values were. */
	wold = (sc->sc_status & RLN_STATUS_WAKEUP) |
	    (sc->sc_control & RLN_CONTROL_RESET);

	if (wnew == RLN_WAKEUP_SET) {
		/* SetWakeupBit() */
		dprintf(" Ws[");
		rln_status_set(sc, RLN_STATUS_WAKEUP);
		if (0/*LLDInactivityTimeOut &&
		    (sc->sc_cardtype & RLN_CTYPE_OEM)*/) {
			dprintf (" 167ms");
			DELAY(167000);
		} else {
			dprintf (" .1ms");
			DELAY(100);
		}
		s = rln_status_read(sc);
		rln_control_set(sc, RLN_CONTROL_RESET);
		if ((s & RLN_STATUS_WAKEUP) != 0)
			for (i = 0; i < 9; i++) {
				dprintf(" 2ms");
				DELAY(2000);
				rln_status_set(sc, RLN_STATUS_WAKEUP);
			}
		dprintf("]");
	} else {
		/* ClearWakeupBit() */
		dprintf(" Wc[");
		if ((wnew & RLN_STATUS_WAKEUP) == 0)
			rln_status_clear(sc, RLN_STATUS_WAKEUP);
		if ((wnew & RLN_CONTROL_RESET) == 0)
			rln_control_clear(sc, RLN_CONTROL_RESET);
		dprintf("]");
	}
	return (wold);
}

/*
 * Performs the first (request) stage of transmitting a command message 
 * to the card. 'len' is the expected length of the message is needed.
 * Returns: 0 on success
 *          1 on timeout
 *          2 on NAK (card busy, and will need a rln_clear_nak() after 100ms)
 */
static int
rln_tx_request(sc, len)
	struct rln_softc *	sc;
	u_int16_t		len;
{
	/* TxRequest() */
	int			s;
	int			i;
	u_int8_t		status;

	/* u_int8_t w; */
	/* w = rln_wakeup(sc, RLN_WAKEUP_SET); */

	dprintf(" Tr[");
	if (sc->sc_width == 16) {
		rln_status_tx_write(sc, RLN_STATUS_TX_HILEN_AVAIL);
		rln_data_write_2(sc, len);
		rln_status_tx_int(sc);

		s = spl0();
		for (i = 0; i < 600; i++) {
			status = rln_status_tx_read(sc);
			if (status == RLN_STATUS_TX_HILEN_ACCEPT || 
			    status == RLN_STATUS_TX_ERROR)
				break;
			DELAY(1000);
		}
		splx(s);
		dprintf(" %dms", i);
		if (status == RLN_STATUS_TX_HILEN_ACCEPT)
			goto success;
		if (status == RLN_STATUS_TX_ERROR)
			goto error;
	} else if (sc->sc_width == 8) {
		rln_status_tx_write(sc, RLN_STATUS_TX_LOLEN_AVAIL);
		rln_data_write_1(sc, len & 0xff);
		rln_status_tx_int(sc);
		s = spl0();
		for (i = 0; i < 6800; i++) {
			status = rln_status_tx_read(sc);
			if (status == RLN_STATUS_TX_LOLEN_ACCEPT)
				break;
			DELAY(1000);
		}
		splx(s);
		dprintf(" %dms", i);
		if (status == RLN_STATUS_TX_LOLEN_ACCEPT) {
			rln_data_write_1(sc, (len >> 8) & 0xff);
			rln_status_tx_write(sc, RLN_STATUS_TX_HILEN_AVAIL);
			s = spl0();
			for (i = 0; i < 600; i++) {
				status = rln_status_tx_read(sc);
				if (status == RLN_STATUS_TX_HILEN_ACCEPT || 
				    status == RLN_STATUS_TX_ERROR)
					break;
				DELAY(1000);
			}
			splx(s);
			dprintf(" %dms", i);
			if (status == RLN_STATUS_TX_HILEN_ACCEPT)
				goto success;
			if (status == RLN_STATUS_TX_ERROR)
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
	/* Will need to clear nak within 100 ms. */
	dprintf("]=2");
#ifdef DIAGNOSTIC
	printf("%s: tx protocol fault (nak)\n", sc->sc_dev.dv_xname);
#endif
	return (2);

success:
	/* rln_wakeup(sc, w); */
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
rln_tx_end(sc)
	struct rln_softc * sc;
{
	/* EndOfTx() */
	int		i;
	int		s;
	u_int8_t	status;

	dprintf(" Te[");
	s = spl0();
	for (i = 0; i < 600; i++) {
		status = rln_status_tx_read(sc);
		if (status == RLN_STATUS_TX_XFR_COMPLETE)
			break;
		DELAY(1000);
	}
	splx(s);
	if (status == RLN_STATUS_TX_XFR_COMPLETE) {
		rln_status_tx_write(sc, RLN_STATUS_TX_IDLE);
		dprintf("]=0");
		return (0);
	} else {
		printf("%s: tx cmd failed (%02x)\n", sc->sc_dev.dv_xname,
		    status);
		rln_need_reset(sc);
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
rln_rx_request(sc, timeo)
	struct rln_softc *	sc;
	int 			timeo;	/* milliseconds */
{
	/* RxRequest */
	int			s;
	int			len = 0;
	int			i;
	u_int8_t		status;
	u_int8_t		hi, lo;

	dprintf(" Rr[");
	status = rln_status_rx_read(sc);

	/* Short wait for states 1|5|6. */
	s = spl0();
	for (i = 0; i < timeo; i++) {
		if (status == RLN_STATUS_RX_LOLEN_AVAIL || 
		    status == RLN_STATUS_RX_HILEN_AVAIL || 
		    status == RLN_STATUS_RX_ERROR)
			break;
		DELAY(1000);
		status = rln_status_rx_read(sc);
	}
	splx(s);
	dprintf(" (%dms)",i);

	if (sc->sc_width == 16) {
		if (status != RLN_STATUS_RX_HILEN_AVAIL)
			goto badstatus_quiet;
		/* Read 2 octets. */
		len = rln_data_read_2(sc);
	} else if (sc->sc_width == 8) {
		if (status != RLN_STATUS_RX_LOLEN_AVAIL)
			goto badstatus_quiet;
		/* Read low octet. */
		lo = rln_data_read_1(sc);
		rln_status_rx_write(sc, RLN_STATUS_RX_LOLEN_ACCEPT);
		rln_status_rx_int(sc);
		s = spl0();
		for (i = 0; i < 600; i++) {
			status = rln_status_rx_read(sc);
			if (status == RLN_STATUS_RX_HILEN_AVAIL)
				break;
			DELAY(1000);
		}
		splx(s);
		if (status != RLN_STATUS_RX_HILEN_AVAIL)
			goto badstatus;
		/* Read high octet. */
		hi = rln_data_read_1(sc);
		len = lo | (hi << 8);
	}
#ifdef DIAGNOSTIC
	else
		panic("rln: bus width %d", sc->sc_width);
#endif

	dprintf(" len=%d]", len);
	return (len);

badstatus:
	printf("%s: rx_request timed out, status %02x\n", 
	    sc->sc_dev.dv_xname, status);
badstatus_quiet:
	if (status == RLN_STATUS_RX_ERROR)
		printf("%s: rx protocol error (nak)\n", sc->sc_dev.dv_xname);
	dprintf("]");
	return (-1);
}

/* Performs part of the second (transfer) stage of receiving a data message. */
void
rln_rx_pdata(sc, buf, len, pd)
	struct rln_softc *	sc;
	void *			buf;
	int			len;
	struct rln_pdata *	pd;
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
		rln_data_read_multi_2(sc, data, len / 2);
#ifdef RLNDEBUG_REG
		dprintf(" D>"); 
		dprinthex(data, len);
#endif
		if (len & 1) {
			/* Read the last octet plus a bit extra. */
			union {
				u_int16_t w;
				u_int8_t  b[2];
			} u;

			u.w = rln_data_read_2(sc);
			data[len - 1] = u.b[0];
			pd->p_data = u.b[1];
			pd->p_nremain = 1;
#ifdef RLNDEBUG_REG
			dprintf(" D>{%02x%02x}", u.b[0], u.b[1]); 
#endif
		}
	} else if (sc->sc_width == 8) {
		rln_data_read_multi_1(sc, data, len);
#ifdef RLNDEBUG_REG
		dprintf(" D>"); 
		dprinthex(data, len);
#endif
		if (len & 1) {
			/* Must read multiples of two. */
			pd->p_data = rln_data_read_1(sc);
			pd->p_nremain = 1;
#ifdef RLNDEBUG_REG
			dprintf(" D>{%02x}", pd->p_data); 
#endif
		}
	}

}

int
rln_rx_data(sc, buf, len)
	struct rln_softc *	sc;
	void *			buf;
	int			len;
{
	/* RxData() */
	struct rln_pdata	pd = { 0, 0 };
	int			s;
	int			i;
	u_int8_t		status;

	dprintf(" Rd[");
	rln_status_rx_write(sc, RLN_STATUS_RX_HILEN_ACCEPT);
	rln_status_rx_int(sc);
	s = spl0();
	for (i = 0; i < 600; i++) {
		status = rln_status_rx_read(sc);
		if (status == RLN_STATUS_RX_XFR)
			break;
		DELAY(1000);
	}
	splx(s);
	if (status != RLN_STATUS_RX_XFR) {
		dprintf("]=-1");
		return (-1);
	}

	rln_rx_pdata(sc, buf, len, &pd);
#ifdef DIAGNOSTIC
	/* We should have nothing left over. */
	if (pd.p_nremain || len & 1)
		panic("rln_rx_data: leftover");
#endif

	dprintf("]=0");
	return (0);
}

void
rln_rx_end(sc)
	struct rln_softc * sc;
{
	/* EndOfRx() */

	dprintf(" Re[");
	rln_status_rx_write(sc, RLN_STATUS_RX_XFR_COMPLETE);
	rln_status_rx_int(sc);
	/* rln_wakeup(sc, 0); */
	dprintf("]");
}

/* Clear a transmission NAK from the card. */
void
rln_clear_nak(sc)
	struct rln_softc * sc;
{
	/* ClearNAK() */

	rln_status_tx_write(sc, RLN_STATUS_CLRNAK);
	rln_status_tx_int(sc);
}

/*
 * Send a command message to the card. Returns;
 *	2: NAK
 *	-1: failure
 *	0: success
 */
int
rln_msg_tx_start(sc, buf, pktlen, state)
	struct rln_softc *	sc;
	void *			buf;
	int			pktlen;
	struct rln_msg_tx_state * state;
{
	struct rln_mm_cmd *	cmd = (struct rln_mm_cmd *)buf;
	int			ret;

	state->ien = rln_enable(sc, 0);
	state->pd.p_nremain = 0;

	if (!(cmd->cmd_letter == 'A' && cmd->cmd_fn == 6)) 	/* Standby. */
		state->w = rln_wakeup(sc, RLN_WAKEUP_SET); 
	else
		state->w = RLN_WAKEUP_NOCHANGE;

	ret = rln_tx_request(sc, pktlen);
	if (ret == 2) {
		rln_clear_nak(sc);
		if (sc->sc_cardtype & RLN_CTYPE_OEM)
			rln_need_reset(sc);
		ret = 2;
	}
	else if (ret == 1) {
		/* Timeout. */
		rln_status_tx_write(sc, RLN_STATUS_TX_XFR);
		ret = -1;
	}
	return (ret);
}

void
rln_msg_tx_data(sc, buf, len, state)
	struct rln_softc *	sc;
	void *			buf;
	u_int16_t		len;
	struct rln_msg_tx_state * state;
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
#ifdef RLNDEBUG_REG
		dprintf(" D<%02x%02x", u.b[0], u.b[1]);
#endif
		rln_data_write_2(sc, u.w);
		state->pd.p_nremain = 0;
	} 

	if (len) {
		if (sc->sc_width == 16) {
			if (len >= 2)
				rln_data_write_multi_2(sc, buf, len / 2);
			if (len & 1) {
				state->pd.p_nremain = 1;
				state->pd.p_data = data[len - 1];
			}
		} else if (sc->sc_width == 8)
			rln_data_write_multi_1(sc, buf, len);
#ifdef DIAGNOSTIC
		else
			panic("rln_msg_tx_data width %d", sc->sc_width);
#endif
#ifdef RLNDEBUG_REG
		dprintf(" D<"); 
		dprinthex(data, len);
#endif
	}
}


int
rln_msg_tx_end(sc, state)
	struct rln_softc *	sc;
	struct rln_msg_tx_state * state;
{
	int			ret;

	/* Flush the tx buffer. */
	if (state->pd.p_nremain)
		rln_msg_tx_data(sc, NULL, 0, state);

#ifdef DIAGNOSTIC
	if (state->pd.p_nremain)
		panic("rln_msg_tx_end remain %d", state->pd.p_nremain);
#endif
	ret = rln_tx_end(sc);
	if (sc->sc_arpcom.ac_if.if_flags & IFF_OACTIVE)
		state->w = RLN_WAKEUP_NOCHANGE;
	rln_wakeup(sc, state->w);
	rln_enable(sc, state->ien);
	return (ret);
}

/* Return the next unique sequence number to use for a transmitted command */
u_int8_t
rln_newseq(sc)
	struct rln_softc * sc;
{
	int s;
	u_int8_t seq;

	s = splhigh();
	seq = sc->sc_pktseq++;
	if (sc->sc_pktseq > RLN_MAXSEQ)
		sc->sc_pktseq = 0;
	splx(s);
	return (seq);
}

/*
 * Transmit a command message to, and (optionally) receive a response
 * message from the card.  Each transmitted message has a sequence
 * number, and corresponding reply messages have the same sequence
 * number.  We use the sequence numbers to index the mailboxes so
 * that rlnsoftintr() can signal this routine when it has serviced
 * and correctly received a response.
 */

int
rln_msg_txrx(sc, tx, txlen, rx, rxlen)
	struct rln_softc *	sc;
	void *			tx;
	int 			txlen;
	void *			rx;
	int			rxlen;
{
	struct rln_mm_cmd *	txc = (struct rln_mm_cmd *)tx;
	struct rln_mm_cmd *	rxc = (struct rln_mm_cmd *)rx;
	struct rln_msg_tx_state state;
	int			ien;
	int			ret;

#ifdef DIAGNOSTIC
	if (rx != NULL && rxlen < sizeof *rxc)
		panic("rln_msg_txrx");
#endif

	txc->cmd_seq = rln_newseq(sc);

#ifdef RLNDUMP
	printf("%s: send %c%d seq %d data ", sc->sc_dev.dv_xname, 
	    txc->cmd_letter, txc->cmd_fn, txc->cmd_seq);
	RLNDUMPHEX(txc, sizeof *txc);
	printf(":");
	RLNDUMPHEX((char *)tx + sizeof *txc, txlen - sizeof *txc);
	printf("\n");
#endif

	if (rx != NULL)
		if (rln_mbox_create(sc, txc->cmd_seq, rx, rxlen) < 0)
			/* Mailbox collision. */
			return (-1);

	/* Start the transfer. */
	if ((ret = rln_msg_tx_start(sc, tx, txlen, &state))) {
		if (rx != NULL)
			rln_mbox_wait(sc, txc->cmd_seq, -1);
		return (ret);
	}

	/* Always send an even number of octets. */
	rln_msg_tx_data(sc, tx, (txlen + 1) & ~1, &state);

	/* End the transmission. */
	if ((ret = rln_msg_tx_end(sc, &state))) {
		/* Destroy mailbox. */
		if (rx != NULL)
			rln_mbox_wait(sc, txc->cmd_seq, -1);
		return (ret);
	}

	/* Don't wait for reply if there is nowhere to put it. */
	if (rx == NULL)
		return (0);

	/* Enable interrupts if not already. */
	ien = rln_enable(sc, 1);

	/* Wait for the reply message. */
	if (rln_mbox_wait(sc, txc->cmd_seq, 2000) <= 0) {
		printf("%s: lost message %c%d seq %d\n", sc->sc_dev.dv_xname,
			txc->cmd_letter, txc->cmd_fn, txc->cmd_seq);
		rln_enable(sc, ien);
		return (-1);
	}
	rln_enable(sc, ien);

#ifdef RLNDUMP
	printf("%s: recv %c%d seq %d data ", sc->sc_dev.dv_xname, 
	    rxc->cmd_letter, rxc->cmd_fn, rxc->cmd_seq);
	RLNDUMPHEX(rxc, sizeof *rxc);
	printf(":");
	RLNDUMPHEX(((char *)rx) + sizeof *rxc, rxlen - sizeof *rxc);
	printf("\n");
#endif

	/* Check for errors in the received message. */
	if (rxc->cmd_error & 0x80) {
		printf("%s: command error 0x%02x command %c%d\n",
			sc->sc_dev.dv_xname,
			rxc->cmd_error & ~0x80,
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
 * The interrupt service routine signals the mailbox when it
 * gets the reply message.
 */

/* Create a mailbox for filling. */
int
rln_mbox_create(sc, seq, buf, len)
	struct rln_softc *	sc;
	u_int8_t		seq;
	void *			buf;
	size_t			len;
{
	int			s;
	struct rln_mbox *	mb = &sc->sc_mbox[seq];

	dprintf(" <create %d", seq);

#ifdef DIAGNOSTIC
	if (seq > RLN_NMBOX)
		panic("mbox create");
#endif

	s = splhigh();
	if (mb->mb_state != RLNMBOX_VOID) {
#ifdef DIAGNOSTIC
		printf("mbox collision");
#endif
		splx(s);
		return (-1);
	}
	mb->mb_buf = buf;
	mb->mb_len = len;
	mb->mb_actlen = 0;
	mb->mb_state = RLNMBOX_EMPTY;
	dprintf(" empty>");
	splx(s);
	return (0);
}


/* Wait for a mailbox to be filled. */
int
rln_mbox_wait(sc, seq, timeo)
	struct rln_softc *	sc;
	u_int8_t		seq;
	int			timeo;
{
	int			i;
	int			s;
	int			ret;
	volatile struct rln_mbox * mb = &sc->sc_mbox[seq];
	extern int		cold;

	dprintf(" <wait %d", seq);

#ifdef DIAGNOSTIC
	if (seq > RLN_NMBOX)
		panic("mbox wait");
#endif
	if (cold) {
		/* Autoconfiguration - spin at spl0. */
		s = spl0();
		i = 0;
		while (mb->mb_state == RLNMBOX_EMPTY && i < timeo) {
			DELAY(1000);
			i++;
		}
		if (i)
			dprintf(" %dms", i);
		while (mb->mb_state == RLNMBOX_FILLING) 
			;
		splx(s);
	} else {
		tsleep((void *)mb, PRIBIO, "rlnmbox", hz * timeo / 1000);
		if (mb->mb_state == RLNMBOX_FILLING) {
			/* Must wait until filled. */
			s = spl0();
			while (mb->mb_state == RLNMBOX_FILLING)
				;
			splx(s);
		}
	}

	s = splhigh();
#ifdef DIAGNOSTIC
	if (mb->mb_state != RLNMBOX_EMPTY && mb->mb_state != RLNMBOX_FILLED)
		panic("mbox wait %d", mb->mb_state);
#endif
	ret = mb->mb_actlen;
	mb->mb_state = RLNMBOX_VOID;
	dprintf(" void>=%d", ret);
	splx(s);
	return (ret);
}

/* Lock a mailbox for filling. */
int
rln_mbox_lock(sc, seq, bufp, lenp)
	struct rln_softc *	sc;
	u_int8_t		seq;
	void **			bufp;
	size_t *		lenp;
{
	int			s;
	struct rln_mbox *	mb = &sc->sc_mbox[seq];

	dprintf(" <lock %d", seq);

	s = splhigh();
#ifdef DIAGNOSTIC
	if (seq > RLN_NMBOX)
		panic("mbox lock");
#endif
	if (mb->mb_state != RLNMBOX_EMPTY) {
		splx(s);
		dprintf(" ?>");
		return (-1);
	}

	mb->mb_state = RLNMBOX_FILLING;
	dprintf(" filling>");
	*bufp = mb->mb_buf;
	*lenp = mb->mb_len;

	splx(s);
	return (0);
}

/* Unlock a mailbox and inform the waiter of the actual number of octets. */
void
rln_mbox_unlock(sc, seq, actlen)
	struct rln_softc *	sc;
	u_int8_t		seq;
	size_t			actlen;
{
	int			s;
	struct rln_mbox *	mb = &sc->sc_mbox[seq];

	dprintf(" <unlock %d", seq);

	s = splhigh();
#ifdef DIAGNOSTIC
	if (seq > RLN_NMBOX)
		panic("mbox unlock seq");
	if (mb->mb_state != RLNMBOX_FILLING)
		panic("mbox unlock");
#endif
	mb->mb_state = RLNMBOX_FILLED;
	dprintf(" filled>");
	mb->mb_actlen = actlen;
	wakeup(mb);
	splx(s);
}

