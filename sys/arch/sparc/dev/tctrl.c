/*	$OpenBSD: tctrl.c,v 1.20 2009/02/26 17:19:47 oga Exp $	*/
/*	$NetBSD: tctrl.c,v 1.2 1999/08/11 00:46:06 matt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * The /dev/apm{,ctl} interface code falls under the following license
 * terms:
 *
 * Copyright (c) 1998-2001 Michael Shalayeff. All rights reserved.
 * Copyright (c) 1995 John T. Kohl.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/timeout.h>

#include <machine/apmvar.h>
#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <sparc/sparc/auxioreg.h>

#include <sparc/dev/ts102reg.h>
#include <sparc/dev/tctrlvar.h>

/*
 * Flags to control kernel display
 *	SCFLAG_NOPRINT:		do not output APM power messages due to
 *				a power change event.
 *
 *	SCFLAG_PCTPRINT:	do not output APM power messages due to
 *				to a power change event unless the battery
 *				percentage changes.
 */

#define SCFLAG_NOPRINT	0x0008000
#define SCFLAG_PCTPRINT	0x0004000
#define SCFLAG_PRINT	(SCFLAG_NOPRINT|SCFLAG_PCTPRINT)

const char *tctrl_ext_status[16] = {
	"main power available",
	"internal battery attached",
	"external battery attached",
	"external VGA attached",
	"external keyboard attached",
	"external mouse attached",
	"lid down",
	"internal battery charging",
	"external battery charging",
	"internal battery discharging",
	"external battery discharging",
};

/* Request "packet" */
struct tctrl_req {
	u_int8_t	cmdbuf[16];
	u_int		cmdlen;
	u_int8_t	rspbuf[16];
	u_int		rsplen;
};

struct tctrl_softc {
	struct device sc_dev;
	struct uctrl_regs *sc_regs;
	struct intrhand sc_ih;
	u_int	sc_ext_status;
	u_int	sc_flags;
#define	TCTRL_SEND_REQUEST		0x0001
#define	TCTRL_ISXT			0x0002
	u_int	sc_wantdata;
	enum { TCTRL_IDLE, TCTRL_ARGS,
		TCTRL_ACK, TCTRL_DATA } sc_state;
	u_int8_t sc_cmdbuf[16];
	u_int8_t sc_rspbuf[16];
	u_int8_t sc_tft_on;
	u_int8_t sc_pcmcia_on;
	u_int8_t sc_brightness;
	u_int8_t sc_op;
	u_int	sc_cmdoff;
	u_int	sc_cmdlen;
	u_int	sc_rspoff;
	u_int	sc_rsplen;
	u_int	sc_rspack;
	u_int	sc_bellfreq;
	u_int	sc_bellvol;

	struct timeout sc_tmo;

	/* /dev/apm{,ctl} fields */
	struct klist sc_note;
	u_int	sc_apmflags;

	/* external video control callback */
	void (*sc_evcb)(void *, int);
	void *sc_evdata;
};

int	tctrl_match(struct device *, void *, void *);
void	tctrl_attach(struct device *, struct device *, void *);

void	tctrl_bell(struct tctrl_softc *, int, int);
void	tctrl_brightness(struct tctrl_softc *, int, int);
void	tctrl_init_lcd(struct tctrl_softc *);
int	tctrl_intr(void *);
void	tctrl_lcd(struct tctrl_softc *, int, int);
u_int8_t tctrl_read_data(struct tctrl_softc *);
void	tctrl_read_event_status(void *);
void	tctrl_read_ext_status(struct tctrl_softc *);
int	tctrl_request(struct tctrl_softc *, struct tctrl_req *);
void	tctrl_tft(struct tctrl_softc *);
void	tctrl_write_data(struct tctrl_softc *, u_int8_t);

int	apm_record_event(struct tctrl_softc *, u_int);

struct cfattach tctrl_ca = {
	sizeof(struct tctrl_softc), tctrl_match, tctrl_attach
};

struct cfdriver tctrl_cd = {
	NULL, "tctrl", DV_DULL
};

int
tctrl_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	/*
	 * Tadpole 3GX/3GS uses "uctrl" for the Tadpole Microcontroller
	 * (which is really part of the TS102 PCMCIA controller, but there
	 * exists a distinct OpenProm node for the microcontroller interface).
	 */
	if (strcmp("uctrl", ra->ra_name))
		return (0);

	return (1);
}

void
tctrl_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct tctrl_softc *sc = (void *)self;
	u_int i, v;
	int pri;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n",
		    ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;

	if (ca->ca_ra.ra_nreg != 1) {
		printf(": expected 1 register, got %d\n",
		    ca->ca_ra.ra_nreg);
		return;
	}
	sc->sc_regs = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
	    ca->ca_ra.ra_reg[0].rr_len);

	printf(" pri %d\n", pri);

	/*
	 * We need to check if we are running on the SPARCbook S3XT, which
	 * needs extra work to control the TFT power.
	 */
	sc->sc_flags = 0;
	if (strcmp(mainbus_model, "Tadpole_S3000XT") == 0)
		sc->sc_flags |= TCTRL_ISXT;
	sc->sc_tft_on = 1;

	/* clear any pending data */
	for (i = 0; i < 10000; i++) {
		if ((TS102_UCTRL_STS_RXNE_STA & sc->sc_regs->stat) == 0)
			break;
		v = sc->sc_regs->data;
		sc->sc_regs->stat = TS102_UCTRL_STS_RXNE_STA;
	}

	sc->sc_ih.ih_fun = tctrl_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, -1, self->dv_xname);

	timeout_set(&sc->sc_tmo, tctrl_read_event_status, sc);

	/* See what the external status is */
	tctrl_read_ext_status(sc);
	if (sc->sc_ext_status != 0) {
		const char *sep;
		u_int len;

		v = sc->sc_ext_status;
		len = 0;
		sep = "";
		for (i = 0; v != 0; i++, v >>= 1) {
			if ((v & 1) == 0)
				continue;
			/* wrap to next line if necessary */
			if (len != 0 && len + strlen(sep) +
			    strlen(tctrl_ext_status[i]) > 80) {
				printf("\n");
				len = 0;
			}
			if (len == 0) {
				printf("%s: ", sc->sc_dev.dv_xname);
				len = 2 + strlen(sc->sc_dev.dv_xname);
				sep = "";
			}
			printf("%s%s", sep, tctrl_ext_status[i]);
			len += strlen(sep) + strlen(tctrl_ext_status[i]);
			sep = ", ";
		}
		if (len != 0)
			printf("\n");
	}

	/* Get a few status values */
	tctrl_bell(sc, 0xff, 0);
	tctrl_brightness(sc, 0xff, 0);

	/* Blank video if lid is closed during boot */
	if (sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN)
		tctrl_tft(sc);

	sc->sc_regs->intr = TS102_UCTRL_INT_RXNE_REQ|TS102_UCTRL_INT_RXNE_MSK;

	sc->sc_wantdata = 0;

	/* Initialize the LCD icons */
	tctrl_init_lcd(sc);
}

int
tctrl_intr(void *arg)
{
	struct tctrl_softc *sc = arg;
	unsigned int v, d;
	int progress = 0;

again:
	/* find out the cause(s) of the interrupt */
	v = sc->sc_regs->stat & TS102_UCTRL_STS_MASK;

	/* clear the cause(s) of the interrupt */
	sc->sc_regs->stat = v;

	v &= ~(TS102_UCTRL_STS_RXO_STA|TS102_UCTRL_STS_TXE_STA);
	if (sc->sc_cmdoff >= sc->sc_cmdlen) {
		v &= ~TS102_UCTRL_STS_TXNF_STA;
		if (sc->sc_regs->intr & TS102_UCTRL_INT_TXNF_REQ) {
			sc->sc_regs->intr = 0;
			progress = 1;
		}
	}
	if (v == 0 && ((sc->sc_flags & TCTRL_SEND_REQUEST) == 0 ||
	    sc->sc_state != TCTRL_IDLE)) {
		return (progress);
	}

	progress = 1;
	if (v & TS102_UCTRL_STS_RXNE_STA) {
		d = tctrl_read_data(sc);
		switch (sc->sc_state) {
		case TCTRL_IDLE:
			if (d == TS102_UCTRL_INTR) {
				/* external event */
				timeout_add(&sc->sc_tmo, 1);
			} else {
				printf("%s: (op=0x%02x): unexpected data (0x%02x)\n",
					sc->sc_dev.dv_xname, sc->sc_op, d);
			}
			goto again;
		case TCTRL_ACK:
#ifdef TCTRLDEBUG
			printf(" ack=0x%02x", d);
#endif
			switch (d) {
			case TS102_UCTRL_ACK:
				sc->sc_rspack = 1;
				sc->sc_rsplen--;
				sc->sc_rspoff = 0;
				sc->sc_state =
				    sc->sc_rsplen ? TCTRL_DATA : TCTRL_IDLE;
				sc->sc_wantdata = sc->sc_rsplen ? 1 : 0;
#ifdef TCTRLDEBUG
				if (sc->sc_rsplen > 0) {
					printf(" [data(%u)]", sc->sc_rsplen);
				} else {
					printf(" [idle]\n");
				}
#endif
				goto again;
			default:
				printf("%s: (op=0x%02x): unexpected return value (0x%02x)\n",
					sc->sc_dev.dv_xname, sc->sc_op, d);
				/* FALLTHROUGH */
			case TS102_UCTRL_NACK:
				printf("%s: command %x failed\n",
				    sc->sc_dev.dv_xname, sc->sc_op);
				sc->sc_rspack = 0;
				sc->sc_wantdata = 0;
				sc->sc_state = TCTRL_IDLE;
				break;
			}
			break;
		case TCTRL_DATA:
			sc->sc_rspbuf[sc->sc_rspoff++] = d;
#ifdef TCTRLDEBUG
			printf(" [%d]=0x%02x", sc->sc_rspoff-1, d);
#endif
			if (sc->sc_rspoff == sc->sc_rsplen) {
#ifdef TCTRLDEBUG
				printf(" [idle]\n");
#endif
				sc->sc_state = TCTRL_IDLE;
				sc->sc_wantdata = 0;
			}
			goto again;
		default:
			printf("%s: (op=0x%02x): unexpected data (0x%02x) in state %d\n",
			       sc->sc_dev.dv_xname, sc->sc_op, d, sc->sc_state);
			goto again;
		}
	}
	if ((sc->sc_state == TCTRL_IDLE && sc->sc_wantdata == 0) ||
	    (sc->sc_flags & TCTRL_SEND_REQUEST)) {
		if (sc->sc_flags & TCTRL_SEND_REQUEST) {
			sc->sc_flags &= ~TCTRL_SEND_REQUEST;
			sc->sc_wantdata = 1;
		}
		if (sc->sc_cmdlen > 0) {
			sc->sc_regs->intr =
			    sc->sc_regs->intr | TS102_UCTRL_INT_TXNF_MSK
				|TS102_UCTRL_INT_TXNF_REQ;
			v = sc->sc_regs->stat;
		}
	}
	if ((sc->sc_cmdoff < sc->sc_cmdlen) && (v & TS102_UCTRL_STS_TXNF_STA)) {
		tctrl_write_data(sc, sc->sc_cmdbuf[sc->sc_cmdoff++]);
#ifdef TCTRLDEBUG
		if (sc->sc_cmdoff == 1) {
			printf("%s: op=0x%02x(l=%u)", sc->sc_dev.dv_xname,
				sc->sc_cmdbuf[0], sc->sc_rsplen);
		} else {
			printf(" [%d]=0x%02x", sc->sc_cmdoff-1,
				sc->sc_cmdbuf[sc->sc_cmdoff-1]);
		}
#endif
		if (sc->sc_cmdoff == sc->sc_cmdlen) {
			sc->sc_state = sc->sc_rsplen ? TCTRL_ACK : TCTRL_IDLE;
#ifdef TCTRLDEBUG
			printf(" %s", sc->sc_rsplen ? "[ack]" : "[idle]\n");
#endif
			if (sc->sc_cmdoff == 1) {
				sc->sc_op = sc->sc_cmdbuf[0];
			}
			sc->sc_regs->intr =
			    sc->sc_regs->intr & (~TS102_UCTRL_INT_TXNF_MSK
				   |TS102_UCTRL_INT_TXNF_REQ);
		} else if (sc->sc_state == TCTRL_IDLE) {
			sc->sc_op = sc->sc_cmdbuf[0];
			sc->sc_state = TCTRL_ARGS;
#ifdef TCTRLDEBUG
			printf(" [args]");
#endif
		}
	}
	goto again;
}

/*
 * The Tadpole microcontroller is not preprogrammed with icon
 * representations.  The machine boots with the DC-IN light as
 * a blank (all 0x00) and the other lights, as 4 rows of horizontal
 * bars.  The below code initializes the few icons the system will use
 * to sane values.
 *
 * Programming the icons is simple.  It is a 5x8 matrix, with each row a
 * bitfield in the order 0x10 0x08 0x04 0x02 0x01.
 */

static void tctrl_set_glyph(struct tctrl_softc *, u_int, const u_int8_t *);

static const u_int8_t
    tctrl_glyph_dc[] = { 0x00, 0x00, 0x1f, 0x00, 0x15, 0x00, 0x00, 0x00 },
#if 0
    tctrl_glyph_bs[] = { 0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00 },
    tctrl_glyph_w1[] = { 0x0c, 0x16, 0x10, 0x15, 0x10, 0x16, 0x0c, 0x00 },
    tctrl_glyph_w2[] = { 0x0c, 0x0d, 0x01, 0x15, 0x01, 0x0d, 0x0c, 0x00 },
    tctrl_glyph_l1[] = { 0x00, 0x04, 0x08, 0x13, 0x08, 0x04, 0x00, 0x00 },
    tctrl_glyph_l2[] = { 0x00, 0x04, 0x02, 0x19, 0x02, 0x04, 0x00, 0x00 },
#endif
    tctrl_glyph_pc[] = { 0x00, 0x0e, 0x0e, 0x1f, 0x1f, 0x1f, 0x1f, 0x00 };

void
tctrl_init_lcd(struct tctrl_softc *sc)
{
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_DC_GOOD, tctrl_glyph_dc);
#if 0
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_BACKSLASH, tctrl_glyph_bs);
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_WAN1, tctrl_glyph_w1);
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_WAN2, tctrl_glyph_w2);
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_LAN1, tctrl_glyph_l1);
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_LAN2, tctrl_glyph_l2);
#endif
	tctrl_set_glyph(sc, TS102_BLK_OFF_DEF_PCMCIA, tctrl_glyph_pc);
}

static void
tctrl_set_glyph(struct tctrl_softc *sc, u_int glyph, const u_int8_t *data)
{
	struct tctrl_req req;

	req.cmdbuf[0] = TS102_OP_BLK_DEF_SPCL_CHAR;
	req.cmdbuf[1] = 8;
	req.cmdbuf[2] = glyph;
	bcopy(data, req.cmdbuf + 3, 8);
	req.cmdlen = 3 + 8;
	req.rsplen = 1;

	tctrl_request(sc, &req);
}

void
tctrl_read_event_status(void *arg)
{
	struct tctrl_softc *sc = (struct tctrl_softc *)arg;
	struct tctrl_req req;
	unsigned int v;

	req.cmdbuf[0] = TS102_OP_RD_EVENT_STATUS;
	req.cmdlen = 1;
	req.rsplen = 3;

	tctrl_request(sc, &req);

	v = req.rspbuf[0] * 256 + req.rspbuf[1];

	/*
	 * Read the new external status value if necessary
	 */
	if (v & (TS102_EVENT_STATUS_DC_STATUS_CHANGE |
	    TS102_EVENT_STATUS_LID_STATUS_CHANGE |
	    TS102_EVENT_STATUS_EXTERNAL_VGA_STATUS_CHANGE))
		tctrl_read_ext_status(sc);

	if (v & TS102_EVENT_STATUS_SHUTDOWN_REQUEST) {
		printf("%s: SHUTDOWN REQUEST!\n", sc->sc_dev.dv_xname);
	}
#ifdef TCTRLDEBUG
	/* Obviously status change */
	if (v & TS102_EVENT_STATUS_VERY_LOW_POWER_WARNING) {
		if (sc->sc_apmflags & SCFLAG_PCTPRINT)
			printf("%s: Battery level change\n",
			    sc->sc_dev.dv_xname);
	}
#endif
	if (v & TS102_EVENT_STATUS_LOW_POWER_WARNING) {
		if ((sc->sc_apmflags & SCFLAG_NOPRINT) == 0)
			printf("%s: LOW POWER WARNING!\n", sc->sc_dev.dv_xname);
		apm_record_event(sc, APM_BATTERY_LOW);
	}
	if (v & TS102_EVENT_STATUS_DC_STATUS_CHANGE) {
		if ((sc->sc_apmflags & SCFLAG_NOPRINT) == 0)
			printf("%s: main power %s\n", sc->sc_dev.dv_xname,
			    (sc->sc_ext_status & TS102_EXT_STATUS_MAIN_POWER_AVAILABLE) ?
			      "restored" : "removed");
		apm_record_event(sc, APM_POWER_CHANGE);
#if 0 /* automatically done for us */
		tctrl_lcd(sc, ~TS102_LCD_DC_OK,
		    sc->sc_ext_status & TS102_EXT_STATUS_MAIN_POWER_AVAILABLE ?
		      TS102_LCD_DC_OK : 0);
#endif
	}
	if (v & TS102_EVENT_STATUS_LID_STATUS_CHANGE) {
		/* blank or restore video if necessary */
		if (sc->sc_tft_on)
			tctrl_tft(sc);
#ifdef TCTRLDEBUG
		printf("%s: lid %s\n", sc->sc_dev.dv_xname,
		    (sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN) ?
		      "closed" : "opened");
#endif
	}
	if (v & TS102_EVENT_STATUS_EXTERNAL_VGA_STATUS_CHANGE) {
		printf("%s: external vga %s\n", sc->sc_dev.dv_xname,
		    sc->sc_ext_status & TS102_EXT_STATUS_EXTERNAL_VGA_ATTACHED ?
		      "attached" : "detached");
#ifdef TCTRLDEBUG
		req.cmdbuf[0] = TS102_OP_RD_EXT_VGA_PORT;
		req.cmdlen = 1;
		req.rsplen = 2;
		tctrl_request(sc, &req);
		printf("%s: vga status %x\n", sc->sc_dev.dv_xname,
		    req.rspbuf[0]);
#endif
		if (sc->sc_evcb != NULL)
			(*sc->sc_evcb)(sc->sc_evdata, sc->sc_ext_status &
			    TS102_EXT_STATUS_EXTERNAL_VGA_ATTACHED);
	}
}

void
tctrl_read_ext_status(struct tctrl_softc *sc)
{
	struct tctrl_req req;

	req.cmdbuf[0] = TS102_OP_RD_EXT_STATUS;
	req.cmdlen = 1;
	req.rsplen = 3;
#ifdef TCTRLDEBUG
	printf("tctrl_read_ext_status: before, ext_status = %x\n",
	    sc->sc_ext_status);
#endif

	tctrl_request(sc, &req);

	sc->sc_ext_status = req.rspbuf[0] * 256 + req.rspbuf[1];

#ifdef TCTRLDEBUG
	printf("tctrl_read_ext_status: after, ext_status = %x\n",
	    sc->sc_ext_status);
#endif
}

void
tctrl_bell(struct tctrl_softc *sc, int mask, int value)
{
	struct tctrl_req req;

	req.cmdbuf[0] = TS102_OP_CTL_SPEAKER_VOLUME;
	req.cmdbuf[1] = mask;
	req.cmdbuf[2] = value;
	req.cmdlen = 3;
	req.rsplen = 2;

	tctrl_request(sc, &req);

	/*
	 * Note that rspbuf[0] returns the previous value, before any
	 * adjustment happened.
	 */
	if (mask == 0)
		sc->sc_bellvol = value;
	else
		sc->sc_bellvol = req.rspbuf[0];
}

void
tctrl_brightness(struct tctrl_softc *sc, int mask, int value)
{
	struct tctrl_req req;

	req.cmdbuf[0] = TS102_OP_CTL_TFT_BRIGHTNESS;
	req.cmdbuf[1] = mask;
	req.cmdbuf[2] = value;
	req.cmdlen = 3;
	req.rsplen = 2;

	tctrl_request(sc, &req);

	/*
	 * Note that rspbuf[0] returns the previous value, before any
	 * adjustment happened.
	 */
	if (mask == 0)
		sc->sc_brightness = value;
	else
		sc->sc_brightness = req.rspbuf[0];
}

void
tctrl_tft(struct tctrl_softc *sc)
{
	struct tctrl_req req;
	int enable;

	enable = (sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN) == 0 &&
	    sc->sc_tft_on;

	req.cmdbuf[0] = TS102_OP_CTL_BITPORT;
	req.cmdbuf[1] = ~TS102_BITPORT_TFTPWR;
	req.cmdbuf[2] = enable ? 0 : TS102_BITPORT_TFTPWR;
	req.cmdlen = 3;
	req.rsplen = 2;

	if ((sc->sc_flags & TCTRL_ISXT) != 0 && enable) {
		sb_auxregbisc(0, AUXIO_TFT, 0);
		delay(100000);	/* XXX is such a long delay really necessary? */
	}

	tctrl_request(sc, &req);

	if ((sc->sc_flags & TCTRL_ISXT) != 0 && !enable) {
		delay(100000);	/* XXX is such a long delay really necessary? */
		sb_auxregbisc(0, 0, AUXIO_TFT);
	}
}

void
tctrl_lcd(struct tctrl_softc *sc, int mask, int value)
{
	struct tctrl_req req;

	req.cmdbuf[0] = TS102_OP_CTL_LCD;

	/*
	 * The mask setup for this particular command is *very* bizarre
	 * and totally undocumented.
	 * One would expect the cmdlen and rsplen to be 5 and 3,
	 * respectively, as well.  Though luck, they are not...
	 */

	req.cmdbuf[1] = mask & 0xff;
	req.cmdbuf[4] = (mask >> 8) & 0x01;

	req.cmdbuf[2] = value & 0xff;
	req.cmdbuf[3] = (value >> 8 & 0x01);

	req.cmdlen = 3;
	req.rsplen = 2;

	tctrl_request(sc, &req);
}

int
tctrl_request(struct tctrl_softc *sc, struct tctrl_req *req)
{
	int s, rv;

	while (sc->sc_wantdata != 0) {
		DELAY(1);
	}

	s = splhigh();
	sc->sc_flags |= TCTRL_SEND_REQUEST;
	bcopy(req->cmdbuf, sc->sc_cmdbuf, req->cmdlen);
	sc->sc_wantdata = 1;
	sc->sc_rsplen = req->rsplen;
	sc->sc_cmdlen = req->cmdlen;
	sc->sc_cmdoff = sc->sc_rspoff = 0;

	do {
		tctrl_intr(sc);
	} while (sc->sc_state != TCTRL_IDLE);

	sc->sc_wantdata = 0;	/* just in case... */

	rv = sc->sc_rspack;
	if (rv != 0)
		bcopy(sc->sc_rspbuf, req->rspbuf, sc->sc_rsplen);
	else
		bzero(req->rspbuf, req->rsplen);	/* safety */
	splx(s);

	return (rv);
}

void
tctrl_write_data(sc, v)
	struct tctrl_softc *sc;
	u_int8_t v;
{
	unsigned int i;

	for (i = 0; i < 100; i++)  {
		if (sc->sc_regs->stat & TS102_UCTRL_STS_TXNF_STA)
			break;
	}
	sc->sc_regs->data = v;
}

u_int8_t
tctrl_read_data(sc)
	struct tctrl_softc *sc;
{ 
	unsigned int i, v;

	for (i = 0; i < 100000; i++) {
		if (sc->sc_regs->stat & TS102_UCTRL_STS_RXNE_STA)
			break;
		DELAY(1);
	}

	v = sc->sc_regs->data;
	sc->sc_regs->stat = TS102_UCTRL_STS_RXNE_STA;
	return v;
}

/*
 * External interfaces, used by the display and pcmcia drivers, as well
 * as the powerdown code.
 */

void
tadpole_powerdown(void)
{
	struct tctrl_softc *sc;
	struct tctrl_req req;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	req.cmdbuf[0] = TS102_OP_ADMIN_POWER_OFF;
	req.cmdlen = 1;
	req.rsplen = 1;

	tctrl_request(sc, &req);
}

void
tadpole_set_brightness(int value)
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	if (value != sc->sc_brightness)
		tctrl_brightness(sc, 0, value);
}

int
tadpole_get_brightness()
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return 0;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	return sc->sc_brightness;
}

void
tadpole_set_video(int enabled)
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	if (sc->sc_tft_on ^ enabled) {
		sc->sc_tft_on = enabled;
		/* nothing to do if the lid is down */
		if ((sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN) == 0)
			tctrl_tft(sc);
	}
}

u_int
tadpole_get_video()
{
	struct tctrl_softc *sc;
	unsigned int status;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return 0;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	status = sc->sc_tft_on ? TV_ON : 0;

	return status;
}

void
tadpole_register_extvideo(void (*cb)(void *, int), void *data)
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	sc->sc_evcb = cb;
	sc->sc_evdata = data;

	(*cb)(data, sc->sc_ext_status & TS102_EXT_STATUS_EXTERNAL_VGA_ATTACHED);
}

void
tadpole_set_pcmcia(int slot, int enabled)
{
	struct tctrl_softc *sc;
	int mask;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];
	mask = 1 << slot;
	enabled = enabled ? mask : 0;
	if ((sc->sc_pcmcia_on ^ enabled) & mask) {
		sc->sc_pcmcia_on ^= mask;
		tctrl_lcd(sc, ~TS102_LCD_PCMCIA_ACTIVE,
		    sc->sc_pcmcia_on ? TS102_LCD_PCMCIA_ACTIVE : 0);
	}
}

int
tadpole_bell(u_int duration, u_int freq, u_int volume)
{
	struct tctrl_softc *sc;
	struct tctrl_req req;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return (0);
	}

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];

	/* Adjust frequency if necessary (first time or frequence change) */
	if (freq > 0 && freq <= 0xffff && freq != sc->sc_bellfreq) {
		req.cmdbuf[0] = TS102_OP_CMD_SET_BELL_FREQ;
		req.cmdbuf[1] = (freq >> 8) & 0xff;
		req.cmdbuf[2] = freq & 0xff;
		req.cmdlen = 3;
		req.rsplen = 1;

		tctrl_request(sc, &req);

		sc->sc_bellfreq = freq;
	}

	/* Adjust volume if necessary */
	if (volume >= 0 && volume <= 100) {
		volume = (volume * 255) / 100;
		if (volume != sc->sc_bellvol)
			tctrl_bell(sc, 0, volume);

	}

	req.cmdbuf[0] = TS102_OP_CMD_RING_BELL;
	req.cmdbuf[1] = (duration >> 8) & 0xff;
	req.cmdbuf[2] = duration & 0xff;
	req.cmdlen = 3;
	req.rsplen = 1;

	tctrl_request(sc, &req);

	return (1);
}

/*
 * /dev/apm{,ctl} interface code
 */

#define	APMUNIT(dev)	(minor(dev)&0xf0)
#define	APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

int	apmkqfilter(dev_t dev, struct knote *kn);
void	filt_apmrdetach(struct knote *kn);
int	filt_apmread(struct knote *kn, long hint);

struct filterops apmread_filtops =
	{ 1, NULL, filt_apmrdetach, filt_apmread};

#define	SCFLAG_OREAD 	(1 << 0)
#define	SCFLAG_OWRITE	(1 << 1)
#define	SCFLAG_OPEN	(SCFLAG_OREAD|SCFLAG_OWRITE)

int
apmopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tctrl_softc *sc;
	int error = 0;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return (ENXIO);
	}

	/* apm0 only */
	if (APMUNIT(dev) != 0)
		return (ENODEV);

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];

	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		if (!(flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		if (sc->sc_apmflags & SCFLAG_OWRITE) {
			error = EBUSY;
			break;
		}
		sc->sc_apmflags |= SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		if (!(flag & FREAD) || (flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		sc->sc_apmflags |= SCFLAG_OREAD;
		break;
	default:
		error = ENXIO;
		break;
	}
	return (error);
}

int
apmclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return (ENXIO);
	}

	/* apm0 only */
	if (APMUNIT(dev) != 0)
		return (ENODEV);

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];

	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		sc->sc_apmflags &= ~SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		sc->sc_apmflags &= ~SCFLAG_OREAD;
		break;
	}
	return (0);
}

int
apmioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct tctrl_softc *sc;
	struct tctrl_req req;
	struct apm_power_info *power;
	u_int8_t c;
	int error = 0;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return (ENXIO);
	}

	/* apm0 only */
	if (APMUNIT(dev) != 0)
		return (ENODEV);

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];

	switch (cmd) {
		/* some ioctl names from linux */
	case APM_IOC_STANDBY:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = EOPNOTSUPP;	/* XXX */
		break;
	case APM_IOC_SUSPEND:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = EOPNOTSUPP;	/* XXX */
		break;
	case APM_IOC_PRN_CTL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			int flag = *(int *)data;
			switch (flag) {
			case APM_PRINT_ON:	/* enable printing */
				sc->sc_apmflags &= ~SCFLAG_PRINT;
				break;
			case APM_PRINT_OFF: /* disable printing */
				sc->sc_apmflags &= ~SCFLAG_PRINT;
				sc->sc_apmflags |= SCFLAG_NOPRINT;
				break;
			case APM_PRINT_PCT: /* disable some printing */
				sc->sc_apmflags &= ~SCFLAG_PRINT;
				sc->sc_apmflags |= SCFLAG_PCTPRINT;
				break;
			default:
				error = EINVAL;
				break;
			}
		}
		break;
	case APM_IOC_GETPOWER:
	        power = (struct apm_power_info *)data;

		if (sc->sc_ext_status &
		    TS102_EXT_STATUS_INTERNAL_BATTERY_ATTACHED) {
			req.cmdbuf[0] = TS102_OP_RD_INT_CHARGE_RATE;
			req.cmdlen = 1;
			req.rsplen = 2;
			tctrl_request(sc, &req);
			if (req.rspbuf[0] != 0)
				power->battery_state = APM_BATT_CHARGING;
			else
				power->battery_state = APM_BATT_UNKNOWN;

			req.cmdbuf[0] = TS102_OP_RD_INT_CHARGE_LEVEL;
			req.cmdlen = 1;
			req.rsplen = 3;
			tctrl_request(sc, &req);

			c = req.rspbuf[0];
			if (c >= TS102_CHARGE_UNKNOWN)
				power->battery_life = 0;
			else {
				power->battery_life = c;
				if (power->battery_state != APM_BATT_CHARGING) {
					if (c < 0x20)
						power->battery_state =
						    APM_BATT_CRITICAL;
					else if (c < 0x40)
						power->battery_state =
						    APM_BATT_HIGH;
					else if (c < 0x66)
						power->battery_state =
						    APM_BATT_HIGH;
				}
			}
		} else {
			power->battery_state = APM_BATTERY_ABSENT;
			power->battery_life = 0;
		}
		power->minutes_left = (u_int)-1;	/* unknown */

		if (sc->sc_ext_status & TS102_EXT_STATUS_MAIN_POWER_AVAILABLE)
			power->ac_state = APM_AC_ON;
		else
			power->ac_state = APM_AC_OFF;
		break;
	case APM_IOC_STANDBY_REQ:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = EOPNOTSUPP;	/* XXX */
		break;
	case APM_IOC_SUSPEND_REQ:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = EOPNOTSUPP;	/* XXX */
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

int
apm_record_event(struct tctrl_softc *sc, u_int type)
{
	static int apm_evindex;

	/* skip if no user waiting */
	if ((sc->sc_apmflags & SCFLAG_OPEN) == 0)
		return (1);

	apm_evindex++;
	KNOTE(&sc->sc_note, APM_EVENT_COMPOSE(type, apm_evindex));

	return (0);
}

void
filt_apmrdetach(struct knote *kn)
{
	struct tctrl_softc *sc = (struct tctrl_softc *)kn->kn_hook;

	SLIST_REMOVE(&sc->sc_note, kn, knote, kn_selnext);
}

int
filt_apmread(struct knote *kn, long hint)
{
	/* XXX weird kqueue_scan() semantics */
	if (hint && !kn->kn_data)
		kn->kn_data = (int)hint;

	return (1);
}

int
apmkqfilter(dev_t dev, struct knote *kn)
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_ndevs == 0 || tctrl_cd.cd_devs[0] == NULL) {
		return (ENXIO);
	}

	/* apm0 only */
	if (APMUNIT(dev) != 0)
		return (ENODEV);

	sc = (struct tctrl_softc *)tctrl_cd.cd_devs[0];

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &apmread_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (caddr_t)sc;
	SLIST_INSERT_HEAD(&sc->sc_note, kn, kn_selnext);

	return (0);
}
