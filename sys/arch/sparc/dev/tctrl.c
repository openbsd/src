/*	$OpenBSD: tctrl.c,v 1.6 2004/05/10 09:05:52 miod Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/ts102reg.h>
#include <sparc/dev/tctrlvar.h>

const char *tctrl_ext_statuses[16] = {
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

struct tctrl_softc {
	struct device sc_dev;
	struct uctrl_regs *sc_regs;
	struct intrhand sc_ih;
	int sc_node;
	unsigned int sc_junk;
	unsigned int sc_ext_status;
	unsigned int sc_video_accel;
	unsigned int sc_pending;
#define	TCTRL_SEND_BITPORT		0x0001
#define	TCTRL_SEND_POWEROFF		0x0002
#define	TCTRL_SEND_RD_EXT_STATUS	0x0004
#define	TCTRL_SEND_RD_EVENT_STATUS	0x0008
#define	TCTRL_SEND_BITPORT_NOP		0x0010
#define	TCTRL_SEND_BRIGHTNESS		0x0020
#define	TCTRL_SEND_BRIGHTNESS_NOP	0x0040
	enum { TCTRL_IDLE, TCTRL_ARGS,
		TCTRL_ACK, TCTRL_DATA } sc_state;
	u_int8_t sc_cmdbuf[16];
	u_int8_t sc_rspbuf[16];
	u_int8_t sc_bitport;
	u_int8_t sc_tft_on;
	u_int8_t sc_brightness;
	u_int8_t sc_op;
	u_int8_t sc_cmdoff;
	u_int8_t sc_cmdlen;
	u_int8_t sc_rspoff;
	u_int8_t sc_rsplen;

	struct evcnt sc_intrcnt;	/* interrupt counting */
};

int tctrl_match(struct device *, void *, void *);
void tctrl_attach(struct device *, struct device *, void *);

void tctrl_write_data(struct tctrl_softc *, u_int8_t);
u_int8_t tctrl_read_data(struct tctrl_softc *);
int tctrl_intr(void *);
void tctrl_setup_bitport(struct tctrl_softc *, int);
void tctrl_setup_brightness(struct tctrl_softc *, int, int);
void tctrl_process_response(struct tctrl_softc *);

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
	int pri;
	unsigned int i, v;

	/*
	 * We're living on a sbus slot that looks like an obio that
	 * looks like an sbus slot.
	 */
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

	sc->sc_node = ca->ca_ra.ra_node;

	printf(" pri %d\n", pri);

	sc->sc_tft_on = 1;

	/* clear any pending data.
	 */
	for (i = 0; i < 10000; i++) {
		if ((TS102_UCTRL_STS_RXNE_STA & sc->sc_regs->stat) == 0)
			break;
		v = sc->sc_regs->data;
		sc->sc_regs->stat = TS102_UCTRL_STS_RXNE_STA;
	}

	sc->sc_ih.ih_fun = tctrl_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, -1);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	/* See what the external status is
	 */
	sc->sc_pending |= TCTRL_SEND_RD_EXT_STATUS;
	do {
		tctrl_intr(sc);
	} while (sc->sc_state != TCTRL_IDLE);

	if (sc->sc_ext_status != 0) {
		const char *sep;

		printf("%s: ", sc->sc_dev.dv_xname);
		v = sc->sc_ext_status;
		for (i = 0, sep = ""; v != 0; i++, v >>= 1) {
			if (v & 1) {
				printf("%s%s", sep, tctrl_ext_statuses[i]);
				sep = ", ";
			}
		}
		printf("\n");
	}

	/*
	 * Get a few status values.
	 */
	sc->sc_video_accel =
	    sc->sc_ext_status & TS102_EXT_STATUS_MAIN_POWER_AVAILABLE;
	sc->sc_pending |= TCTRL_SEND_BITPORT_NOP;
	do {
		tctrl_intr(sc);
	} while (sc->sc_state != TCTRL_IDLE);
	sc->sc_pending |= TCTRL_SEND_BRIGHTNESS_NOP;
	do {
		tctrl_intr(sc);
	} while (sc->sc_state != TCTRL_IDLE);

	sc->sc_regs->intr = TS102_UCTRL_INT_RXNE_REQ|TS102_UCTRL_INT_RXNE_MSK;
}

int
tctrl_intr(void *arg)
{
	struct tctrl_softc *sc = arg;
	unsigned int v, d;
	int progress = 0;

    again:
	/* find out the cause(s) of the interrupt */
	v = sc->sc_regs->stat;

	/* clear the cause(s) of the interrupt */
	sc->sc_regs->stat = v;

	v &= ~(TS102_UCTRL_STS_RXO_STA|TS102_UCTRL_STS_TXE_STA);
	if (sc->sc_cmdoff >= sc->sc_cmdlen) {
		v &= ~TS102_UCTRL_STS_TXNF_STA;
	}
	if ((v == 0) && (sc->sc_pending == 0 || sc->sc_state != TCTRL_IDLE)) {
		return (progress);
	}

	progress = 1;
	if (v & TS102_UCTRL_STS_RXNE_STA) {
		d = tctrl_read_data(sc);
		switch (sc->sc_state) {
		case TCTRL_IDLE:
			if (d == 0xfa) {
				sc->sc_pending |= TCTRL_SEND_RD_EVENT_STATUS;
			} else {
				printf("%s: (op=0x%02x): unexpected data (0x%02x)\n",
					sc->sc_dev.dv_xname, sc->sc_op, d);
			}
			goto again;
		case TCTRL_ACK:
			if (d != 0xfe) {
				printf("%s: (op=0x%02x): unexpected ack value (0x%02x)\n",
					sc->sc_dev.dv_xname, sc->sc_op, d);
			}
#if 0
			printf(" ack=0x%02x", d);
#endif
			sc->sc_rsplen--;
			sc->sc_rspoff = 0;
			sc->sc_state = sc->sc_rsplen ? TCTRL_DATA : TCTRL_IDLE;
#if 0
			if (sc->sc_rsplen > 0) {
				printf(" [data(%u)]", sc->sc_rsplen);
			} else {
				printf(" [idle]\n");
			}
#endif
			goto again;
		case TCTRL_DATA:
			sc->sc_rspbuf[sc->sc_rspoff++] = d;
#if 0
			printf(" [%d]=0x%02x", sc->sc_rspoff-1, d);
#endif
			if (sc->sc_rspoff == sc->sc_rsplen) {
#if 0
				printf(" [idle]\n");
#endif
				sc->sc_state = TCTRL_IDLE;
				tctrl_process_response(sc);
			}
			goto again;
		default:
			printf("%s: (op=0x%02x): unexpected data (0x%02x) in state %d\n",
			       sc->sc_dev.dv_xname, sc->sc_op, d, sc->sc_state);
			goto again;
		}
	}
	if (sc->sc_state == TCTRL_IDLE) {
		sc->sc_cmdoff = 0;
		sc->sc_cmdlen = 0;
		if (sc->sc_pending & TCTRL_SEND_POWEROFF) {
			sc->sc_pending &= ~TCTRL_SEND_POWEROFF;
			sc->sc_cmdbuf[0] = TS102_OP_ADMIN_POWER_OFF;
			sc->sc_cmdlen = 1;
			sc->sc_rsplen = 0;
		} else if (sc->sc_pending & TCTRL_SEND_RD_EVENT_STATUS) {
			sc->sc_pending &= ~TCTRL_SEND_RD_EVENT_STATUS;
			sc->sc_cmdbuf[0] = TS102_OP_RD_EVENT_STATUS;
			sc->sc_cmdlen = 1;
			sc->sc_rsplen = 3;
		} else if (sc->sc_pending & TCTRL_SEND_RD_EXT_STATUS) {
			sc->sc_pending &= ~TCTRL_SEND_RD_EXT_STATUS;
			sc->sc_cmdbuf[0] = TS102_OP_RD_EXT_STATUS;
			sc->sc_cmdlen = 1;
			sc->sc_rsplen = 3;
		} else if (sc->sc_pending & TCTRL_SEND_BITPORT_NOP) {
			sc->sc_pending &= ~TCTRL_SEND_BITPORT_NOP;
			tctrl_setup_bitport(sc, 1);
		} else if (sc->sc_pending & TCTRL_SEND_BITPORT) {
			sc->sc_pending &= ~TCTRL_SEND_BITPORT;
			tctrl_setup_bitport(sc, 0);
		} else if (sc->sc_pending & TCTRL_SEND_BRIGHTNESS_NOP) {
			sc->sc_pending &= ~TCTRL_SEND_BRIGHTNESS_NOP;
			tctrl_setup_brightness(sc, 0xff, 0);
		} else if (sc->sc_pending & TCTRL_SEND_BRIGHTNESS) {
			sc->sc_pending &= ~TCTRL_SEND_BRIGHTNESS;
			tctrl_setup_brightness(sc, 0, sc->sc_brightness);
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
#if 0
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
#if 0
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
#if 0
			printf(" [args]");
#endif
		}
	}
	goto again;
}

void
tctrl_setup_bitport(struct tctrl_softc *sc, int nop)
{
	if (nop) {
		sc->sc_cmdbuf[0] = TS102_OP_CTL_BITPORT;
		sc->sc_cmdbuf[1] = 0xff;
		sc->sc_cmdbuf[2] = 0;
		sc->sc_cmdlen = 3;
		sc->sc_rsplen = 2;
	} else {
		if ((sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN)
		    || (!sc->sc_tft_on)) {
			sc->sc_cmdbuf[2] = TS102_BITPORT_TFTPWR;
		} else {
			sc->sc_cmdbuf[2] = 0;
		}
		sc->sc_cmdbuf[0] = TS102_OP_CTL_BITPORT;
		sc->sc_cmdbuf[1] = ~TS102_BITPORT_TFTPWR;
		sc->sc_cmdlen = 3;
		sc->sc_rsplen = 2;
	}
}

void
tctrl_setup_brightness(struct tctrl_softc *sc, int mask, int value)
{
	sc->sc_cmdbuf[0] = TS102_OP_CTL_TFT_BRIGHTNESS;
	sc->sc_cmdbuf[1] = mask;
	sc->sc_cmdbuf[2] = value;
	sc->sc_cmdlen = 3;
	sc->sc_rsplen = 2;
}

void
tctrl_process_response(struct tctrl_softc *sc)
{
	switch (sc->sc_op) {
	case TS102_OP_RD_EXT_STATUS: {
		unsigned int status = sc->sc_ext_status;
		sc->sc_ext_status = sc->sc_rspbuf[0] * 256 + sc->sc_rspbuf[1];
		status ^= sc->sc_ext_status;
		if (status & TS102_EXT_STATUS_MAIN_POWER_AVAILABLE) {
			printf("%s: main power %s\n", sc->sc_dev.dv_xname,
			    (sc->sc_ext_status & TS102_EXT_STATUS_MAIN_POWER_AVAILABLE) ?
			      "restored" : "removed");

			/* XXX reset video */
			sc->sc_video_accel = 0;
		}
#if 0
		if (status & TS102_EXT_STATUS_LID_DOWN) {
			printf("%s: lid %s\n", sc->sc_dev.dv_xname,
			    (sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN) ?
			      "closed" : "opened");
		}
#endif
		break;
	}
	case TS102_OP_RD_EVENT_STATUS: {
		unsigned int v = sc->sc_rspbuf[0] * 256 + sc->sc_rspbuf[1];
		if (v & TS102_EVENT_STATUS_SHUTDOWN_REQUEST) {
			printf("%s: SHUTDOWN REQUEST!\n", sc->sc_dev.dv_xname);
		}
#if 0
/* Obviously status change */
		if (v & TS102_EVENT_STATUS_VERY_LOW_POWER_WARNING) {
			printf("%s: VERY LOW POWER WARNING!\n", sc->sc_dev.dv_xname);
		}
#endif
		if (v & TS102_EVENT_STATUS_LOW_POWER_WARNING) {
			printf("%s: LOW POWER WARNING!\n", sc->sc_dev.dv_xname);
		}
		if (v & TS102_EVENT_STATUS_DC_STATUS_CHANGE) {
			sc->sc_pending |= TCTRL_SEND_RD_EXT_STATUS;
		}
		if (v & TS102_EVENT_STATUS_LID_STATUS_CHANGE) {
			sc->sc_pending |= TCTRL_SEND_RD_EXT_STATUS;
			sc->sc_pending |= TCTRL_SEND_BITPORT;
		}
		break;
	}
	case TS102_OP_CTL_BITPORT:
		sc->sc_bitport = (sc->sc_rspbuf[0] & sc->sc_cmdbuf[1]) ^ sc->sc_cmdbuf[2];
		break;
	case TS102_OP_CTL_TFT_BRIGHTNESS:
		sc->sc_brightness = sc->sc_rspbuf[0];
	default:
		break;
	}
}

void
tadpole_powerdown(void)
{
	struct tctrl_softc *sc;
	int i, s;

	if (tctrl_cd.cd_devs == NULL
	    || tctrl_cd.cd_ndevs == 0
	    || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *) tctrl_cd.cd_devs[0];
	s = splhigh();
	sc->sc_pending |= TCTRL_SEND_POWEROFF;
	for (i = 0; i < 10000; i++) {
		tctrl_intr(sc);
		DELAY(1);
	}
	splx(s);
}

void
tadpole_set_brightness(int value)
{
	struct tctrl_softc *sc;
	int s;

	if (tctrl_cd.cd_devs == NULL
	    || tctrl_cd.cd_ndevs == 0
	    || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *) tctrl_cd.cd_devs[0];
	s = splhigh();
	if (value != sc->sc_brightness) {
		sc->sc_brightness = value;
		sc->sc_pending |= TCTRL_SEND_BRIGHTNESS;
		tctrl_intr(sc);
	}
	splx(s);
}

int
tadpole_get_brightness()
{
	struct tctrl_softc *sc;

	if (tctrl_cd.cd_devs == NULL
	    || tctrl_cd.cd_ndevs == 0
	    || tctrl_cd.cd_devs[0] == NULL) {
		return 0;
	}

	sc = (struct tctrl_softc *) tctrl_cd.cd_devs[0];
	return sc->sc_brightness;
}

void
tadpole_set_video(int enabled)
{
	struct tctrl_softc *sc;
	int s;

	if (tctrl_cd.cd_devs == NULL
	    || tctrl_cd.cd_ndevs == 0
	    || tctrl_cd.cd_devs[0] == NULL) {
		return;
	}

	sc = (struct tctrl_softc *) tctrl_cd.cd_devs[0];
	s = splhigh();
	if (sc->sc_tft_on ^ enabled) {
		sc->sc_tft_on = enabled;
		if (sc->sc_ext_status & TS102_EXT_STATUS_LID_DOWN) {
			splx(s);
			return;
		}
		sc->sc_pending |= TCTRL_SEND_BITPORT;
		tctrl_intr(sc);
	}
	splx(s);
}

unsigned int
tadpole_get_video()
{
	struct tctrl_softc *sc;
	unsigned int status;

	if (tctrl_cd.cd_devs == NULL
	    || tctrl_cd.cd_ndevs == 0
	    || tctrl_cd.cd_devs[0] == NULL) {
		return 0;
	}

	sc = (struct tctrl_softc *) tctrl_cd.cd_devs[0];
	status = 0;

	if (sc->sc_tft_on)
		status |= TV_ON;
	if (sc->sc_video_accel)
		status |= TV_ACCEL;

	return status;
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
