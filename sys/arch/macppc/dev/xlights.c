/* $OpenBSD: xlights.c,v 1.3 2007/06/01 08:29:30 gwk Exp $ */
/*
 * Copyright (c) 2007 Gordon Willem Klok <gwk@openbsd,org>
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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/kthread.h>
#include <sys/timeout.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <macppc/dev/dbdma.h>
#include <macppc/dev/i2sreg.h>

struct xlights_softc {
	struct device 			sc_dev;
	int				sc_node;
	int				sc_intr;
	uint32_t			sc_freq;
	int 				sc_dmasts;

	u_char				*sc_reg;
	dbdma_regmap_t 			*sc_dma;
	bus_dma_tag_t			sc_dmat;
	bus_dmamap_t			sc_bufmap;
	bus_dma_segment_t       	sc_bufseg[1];
	dbdma_t				sc_dbdma;
	dbdma_command_t			*sc_dmacmd;
	uint32_t			*sc_buf;
	uint32_t			*sc_bufpos;

	struct timeout 			sc_tmo;
};

int xlights_match(struct device *, void *, void *);
void xlights_attach(struct device *, struct device *, void *);
int xlights_intr(void *);
void xlights_startdma(struct xlights_softc *);
void xlights_deferred(void *);
void xlights_theosDOT(void *);
void xlights_timeout(void *);
void xlights_pwm(struct xlights_softc *, u_char *, int);
extern void keylargo_fcr_enable(int, u_int32_t);
extern void keylargo_fcr_disable(int, u_int32_t);

struct cfattach xlights_ca = {
	sizeof(struct xlights_softc), xlights_match,
	xlights_attach
};

struct cfdriver xlights_cd = {
	NULL, "xlights", DV_DULL
};

#define BL_BUFSZ PAGE_SIZE
#define BL_DBDMA_CMDS 2

int
xlights_match(struct device *parent, void *arg, void *aux)
{
	struct confargs *ca = aux;
	int soundbus, soundchip, error;
	char compat[32];

	if (strcmp(ca->ca_name, "i2s") != 0)
		return 0;
	if ((soundbus = OF_child(ca->ca_node)) == 0)
		return 0;
	if ((soundchip = OF_child(soundbus)) == 0)
		return 0;

	error = OF_getprop(soundchip, "virtual", compat, sizeof(compat));
	if (error == -1) {
		error = OF_getprop(soundchip, "name", compat,
		    sizeof(compat));

		if (error == -1 || (strcmp(compat, "lightshow")) != 0)
			return 0;
	}

	/* we require at least 4 registers */
	if (ca->ca_nreg / sizeof(int) < 4)
		return 0;
	/* we require at least 3 interrupts */
	if (ca->ca_nintr / sizeof(int) < 6)
		return 0;

	return 1;
}

void
xlights_attach(struct device *parent, struct device *self, void *aux)
{
	struct xlights_softc *sc = (struct xlights_softc *)self;
	struct confargs *ca = aux;
	int nseg, error, intr[6];
	u_int32_t reg[4];

	sc->sc_node = OF_child(ca->ca_node);

	OF_getprop(sc->sc_node, "reg", reg, sizeof(reg));
	ca->ca_reg[0] += ca->ca_baseaddr;
	ca->ca_reg[2] += ca->ca_baseaddr;

	if ((sc->sc_reg = mapiodev(ca->ca_reg[0], ca->ca_reg[1])) == NULL) {
		printf(": cannot map registers\n");
		return;
	}
	sc->sc_dmat = ca->ca_dmat;

	if ((sc->sc_dma = mapiodev(ca->ca_reg[2], ca->ca_reg[3])) == NULL) {
		printf(": cannot map DMA registers\n");
		goto nodma;
	}

	if ((sc->sc_dbdma = dbdma_alloc(sc->sc_dmat, BL_DBDMA_CMDS)) == NULL) {
		printf(": cannot alloc DMA descriptors\n");
		goto nodbdma;
	 }
	sc->sc_dmacmd = sc->sc_dbdma->d_addr;

	if ((error = bus_dmamem_alloc(sc->sc_dmat, BL_BUFSZ, 0, 0,
		sc->sc_bufseg, 1, &nseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate DMA mem (%d)\n", error);
		goto nodmamem;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, sc->sc_bufseg, nseg,
	    BL_BUFSZ, (caddr_t *)&sc->sc_buf, BUS_DMA_NOWAIT))) {
		printf(": cannot map DMA mem (%d)\n", error);
		goto nodmamap;
	}
	sc->sc_bufpos = sc->sc_buf;

	if ((error = bus_dmamap_create(sc->sc_dmat, BL_BUFSZ, 1, BL_BUFSZ, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->sc_bufmap))) {
		printf(": cannot create DMA map (%d)\n", error);
		goto nodmacreate;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_bufmap, sc->sc_buf,
	    BL_BUFSZ, NULL, BUS_DMA_NOWAIT))) {
		printf(": cannot load DMA map (%d)\n", error);
		goto nodmaload;
	}
	/* XXX: Should probably extract this from the clock data
	 * property of the soundchip node */
	sc->sc_freq = 44100;

	OF_getprop(sc->sc_node, "interrupts", intr, sizeof(intr));
	sc->sc_intr = intr[2];
	printf(": irq %d\n", sc->sc_intr);

	keylargo_fcr_enable(I2SClockOffset, I2S0EN);
	out32rb(sc->sc_reg + I2S_INT, I2S_INT_CLKSTOPPEND);
	keylargo_fcr_disable(I2SClockOffset, I2S0CLKEN);
	for (error = 0; error < 1000; error++) {
		if (in32rb(sc->sc_reg + I2S_INT) & I2S_INT_CLKSTOPPEND) {
			error = 0;
			break;
		}
		delay(1);
	}
	if (error) {
		printf("%s: i2s timeout\n", sc->sc_dev.dv_xname);
		goto nodmaload;
	}

	mac_intr_establish(parent, sc->sc_intr, intr[3] ? IST_LEVEL :
	    IST_EDGE, IPL_AUDIO, xlights_intr, sc, sc->sc_dev.dv_xname);

	out32rb(sc->sc_reg + I2S_FORMAT, CLKSRC_VS);
	keylargo_fcr_enable(I2SClockOffset, I2S0CLKEN);

	kthread_create_deferred(xlights_deferred, sc);
	timeout_set(&sc->sc_tmo, xlights_timeout, sc);
	return;
nodmaload:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_bufmap);
nodmacreate:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_buf, BL_BUFSZ);
nodmamap:
	bus_dmamem_free(sc->sc_dmat, sc->sc_bufseg, nseg);
nodmamem:
	dbdma_free(sc->sc_dbdma);
nodbdma:
	unmapiodev((void *)sc->sc_dma, ca->ca_reg[3]);
nodma:
	unmapiodev(sc->sc_reg, ca->ca_reg[1]);
}

void
xlights_deferred(void *v)
{
	kthread_create(xlights_theosDOT, v, NULL, "xlights");
}

void
xlights_theosDOT(void *v)
{
	struct xlights_softc *sc = (struct xlights_softc *)v;
	u_char leds[16];
	int offset, i, s, speed;

	while (1) {
		speed = (averunnable.ldavg[0] / 100) / 2;
		for (offset = 8; offset < 16; offset++) {
			s = offset - 2;
			if (s < 8) {
				for (i = 1; i <= (8 - s); i++)
					leds[i - 1] = 0x7f / i;
				s += 8 - s;
			}
			for (i = offset; i >= s; i--)
				leds[i] = 0xff / (offset + 1 - s);

			xlights_pwm(sc, leds, speed);
			bzero(leds, sizeof(leds));
		}
		for (offset = 7; offset >= 0; offset--) {
			s = offset + 2;
			if (s > 7) {
				for (i = 1; i <= (s - 7); i++)
					leds[15 - (i - 1)] = (0xff / (s - 7)) / i;
				s-= s - 7;
			}
			for (i = offset; i <= s; i++)
				leds[i] = 0xff  / (offset + 1) - s;
			xlights_pwm(sc, leds, speed);
			bzero(leds, sizeof(leds));
		}
	}
}

void
xlights_pwm(struct xlights_softc *sc, u_char *leds, int msecs)
{
	uint32_t *p;
	int s, l, k, nsamp;
	uint32_t freq[16] = {0};

	p = sc->sc_bufpos;

	nsamp = sc->sc_freq / 1000 * msecs;

	for (k = 1; k < nsamp;) {
		if (p - sc->sc_buf < BL_BUFSZ / sizeof(uint32_t)) {
			s = 0;
			for (l = 0; l < 16; l++) {
				s >>= 1;
				if (leds[l] &&
				    ((k - freq[l]) == (0xff / leds[l]))) {
					s |= 1 << 15;
					freq[l] = k;
				}
			}
			*p = (s << 17) | (s >> 15);
			p++;
			k++;
			sc->sc_bufpos++;
		} else {
			xlights_startdma(sc);
			while (sc->sc_dmasts)
				tsleep(sc->sc_buf, PWAIT, "blinken", 0);
			sc->sc_bufpos = p = sc->sc_buf;
		}
	}
}

void
xlights_startdma(struct xlights_softc *sc)
{
	dbdma_command_t *cmdp = sc->sc_dmacmd;

	sc->sc_dmasts = 1;
	timeout_add(&sc->sc_tmo, 250);

	DBDMA_BUILD(cmdp, DBDMA_CMD_OUT_LAST, 0,
	    sc->sc_bufmap->dm_segs[0].ds_len,
	    sc->sc_bufmap->dm_segs[0].ds_addr, DBDMA_INT_ALWAYS,
	    DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	cmdp++;

	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0, DBDMA_INT_NEVER,
	    DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	dbdma_start(sc->sc_dma, sc->sc_dbdma);
}

void
xlights_timeout(void *v)
{
	struct xlights_softc *sc = (struct xlights_softc *)v;

	dbdma_reset(sc->sc_dma);
	timeout_del(&sc->sc_tmo);
	sc->sc_dmasts = 0;
	wakeup(sc->sc_buf);
}

int
xlights_intr(void *v)
{
	struct xlights_softc *sc = (struct xlights_softc *)v;
	int status;
	dbdma_command_t *cmd;

	cmd = sc->sc_dmacmd;
	status = dbdma_ld16(&cmd->d_status);
	if (sc->sc_dmasts) {
		sc->sc_dmasts = 0;
		timeout_del(&sc->sc_tmo);
		wakeup(sc->sc_buf);
		return (1);
	}
	return (0);
}
