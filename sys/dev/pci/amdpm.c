/*	$OpenBSD: amdpm.c,v 1.7 2006/01/05 08:54:27 grange Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/timeout.h>
#ifdef __HAVE_TIMECOUNTER
#include <sys/timetc.h>
#endif

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/amdpmreg.h>

#include <dev/rndvar.h>
#include <dev/i2c/i2cvar.h>

#ifdef AMDPM_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define AMDPM_SMBUS_DELAY	100
#define AMDPM_SMBUS_TIMEOUT	1

#ifdef __HAVE_TIMECOUNTER
u_int amdpm_get_timecount(struct timecounter *tc);

#ifndef AMDPM_FREQUENCY
#define AMDPM_FREQUENCY 3579545
#endif

static struct timecounter amdpm_timecounter = {
	amdpm_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffff,		/* counter_mask */
	AMDPM_FREQUENCY,	/* frequency */
	"AMDPM",		/* name */
	1000			/* quality */
};
#endif

struct amdpm_softc {
	struct device sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_tag;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;		/* PMxx space */
	int sc_poll;

	struct timeout sc_rnd_ch;
#ifdef AMDPM_RND_COUNTERS
	struct evcnt sc_rnd_hits;
	struct evcnt sc_rnd_miss;
	struct evcnt sc_rnd_data[256];
#endif

	struct i2c_controller sc_i2c_tag;
	struct lock sc_i2c_lock;
	struct {
		i2c_op_t op;
		void *buf;
		size_t len;
		int flags;
		volatile int error;
	} sc_i2c_xfer;
};

int	amdpm_match(struct device *, void *, void *);
void	amdpm_attach(struct device *, struct device *, void *);
void	amdpm_rnd_callout(void *);

int	amdpm_i2c_acquire_bus(void *, int);
void	amdpm_i2c_release_bus(void *, int);
int	amdpm_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
	    void *, size_t, int);

int	amdpm_intr(void *);

struct cfattach amdpm_ca = {
	sizeof(struct amdpm_softc), amdpm_match, amdpm_attach
};

struct cfdriver amdpm_cd = {
	NULL, "amdpm", DV_DULL
};

#ifdef AMDPM_RND_COUNTERS
#define	AMDPM_RNDCNT_INCR(ev)	(ev)->ev_count++
#else
#define	AMDPM_RNDCNT_INCR(ev)	/* nothing */
#endif

const struct pci_matchid amdpm_ids[] = {
	{ PCI_VENDOR_AMD, PCI_PRODUCT_AMD_766_PMC },
	{ PCI_VENDOR_AMD, PCI_PRODUCT_AMD_PBC768_PMC }
};

int
amdpm_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, amdpm_ids,
	    sizeof(amdpm_ids) / sizeof(amdpm_ids[0])));
}

void
amdpm_attach(struct device *parent, struct device *self, void *aux)
{
	struct amdpm_softc *sc = (struct amdpm_softc *) self;
	struct pci_attach_args *pa = aux;
	struct i2cbus_attach_args iba;
	struct timeval tv1, tv2;
	pcireg_t cfg_reg, reg;
	int i;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_iot = pa->pa_iot;
	sc->sc_poll = 1; /* XXX */

	cfg_reg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_CONFREG);
	if ((cfg_reg & AMDPM_PMIOEN) == 0) {
		printf(": PMxx space isn't enabled\n");
		return;
	}
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_PMPTR);
	if (bus_space_map(sc->sc_iot, AMDPM_PMBASE(reg), AMDPM_PMSIZE,
	    0, &sc->sc_ioh)) {
		printf(": failed to map PMxx space\n");
		return;
	}

#ifdef __HAVE_TIMECOUNTER
	if ((cfg_reg & AMDPM_TMRRST) == 0 &&
	    (cfg_reg & AMDPM_STOPTMR) == 0 &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_PBC768_PMC) {
		printf(": %d-bit timer at %dHz",
		    (cfg_reg & AMDPM_TMR32) ? 32 : 24,
		    amdpm_timecounter.tc_frequency);

		amdpm_timecounter.tc_priv = sc;
		if (cfg_reg & AMDPM_TMR32)
			amdpm_timecounter.tc_counter_mask = 0xffffffffu;
		tc_init(&amdpm_timecounter);
	}
#endif

	if (cfg_reg & AMDPM_RNGEN) {
		/* Check to see if we can read data from the RNG. */
		(void) bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    AMDPM_RNGDATA);
		/* benchmark the RNG */
		microtime(&tv1);
		for (i = 2 * 1024; i--; ) {
			while(!(bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    AMDPM_RNGSTAT) & AMDPM_RNGDONE))
				;
			(void) bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    AMDPM_RNGDATA);
		}
		microtime(&tv2);

		timersub(&tv2, &tv1, &tv1);
		if (tv1.tv_sec)
			tv1.tv_usec += 1000000 * tv1.tv_sec;
		printf(": rng active, %dKb/sec", 8 * 1000000 / tv1.tv_usec);

#ifdef AMDPM_RND_COUNTERS
			evcnt_attach_dynamic(&sc->sc_rnd_hits, EVCNT_TYPE_MISC,
			    NULL, sc->sc_dev.dv_xname, "rnd hits");
			evcnt_attach_dynamic(&sc->sc_rnd_miss, EVCNT_TYPE_MISC,
			    NULL, sc->sc_dev.dv_xname, "rnd miss");
			for (i = 0; i < 256; i++) {
				evcnt_attach_dynamic(&sc->sc_rnd_data[i],
				    EVCNT_TYPE_MISC, NULL, sc->sc_dev.dv_xname,
				    "rnd data");
			}
#endif
		timeout_set(&sc->sc_rnd_ch, amdpm_rnd_callout, sc);
		amdpm_rnd_callout(sc);
	}

	/* Attach I2C bus */
	lockinit(&sc->sc_i2c_lock, PRIBIO | PCATCH, "iiclk", 0, 0);
	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = amdpm_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = amdpm_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = amdpm_i2c_exec;

	bzero(&iba, sizeof iba);
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_i2c_tag;
	config_found(self, &iba, iicbus_print);

	printf("\n");
}

void
amdpm_rnd_callout(void *v)
{
	struct amdpm_softc *sc = v;
	u_int32_t reg;
#ifdef AMDPM_RND_COUNTERS
	int i;
#endif

	if ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_RNGSTAT) &
	    AMDPM_RNGDONE) != 0) {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_RNGDATA);
		add_true_randomness(reg);
#ifdef AMDPM_RND_COUNTERS
		AMDPM_RNDCNT_INCR(&sc->sc_rnd_hits);
		for (i = 0; i < sizeof(reg); i++, reg >>= NBBY)
			AMDPM_RNDCNT_INCR(&sc->sc_rnd_data[reg & 0xff]);
#endif
	} else
		AMDPM_RNDCNT_INCR(&sc->sc_rnd_miss);
	timeout_add(&sc->sc_rnd_ch, 1);
}

#ifdef __HAVE_TIMECOUNTER
u_int
amdpm_get_timecount(struct timecounter *tc)
{
	struct amdpm_softc *sc = tc->tc_priv;
	u_int u2;
#if 0
	u_int u1, u3;
#endif

	u2 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_TMR);
#if 0
	u3 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_TMR);
	do {
		u1 = u2;
		u2 = u3;
		u3 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_TMR);
	} while (u1 > u2 || u2 > u3);
#endif
	return (u2);
}
#endif

int
amdpm_i2c_acquire_bus(void *cookie, int flags)
{
	struct amdpm_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return (0);

	return (lockmgr(&sc->sc_i2c_lock, LK_EXCLUSIVE, NULL));
}

void
amdpm_i2c_release_bus(void *cookie, int flags)
{
	struct amdpm_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return;

	lockmgr(&sc->sc_i2c_lock, LK_RELEASE, NULL);
}

int
amdpm_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct amdpm_softc *sc = cookie;
	u_int8_t *b;
	u_int16_t st, ctl, data;
	int retries;

	DPRINTF(("%s: exec: op %d, addr 0x%x, cmdlen %d, len %d, flags 0x%x\n",
	    sc->sc_dev.dv_xname, op, addr, cmdlen, len, flags));

	/* Check if there's a transfer already running */
	st = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBSTAT);
	DPRINTF(("%s: exec: st 0x%b\n", sc->sc_dev.dv_xname, st,
	    AMDPM_SMBSTAT_BITS));
	if (st & AMDPM_SMBSTAT_BSY)
		return (1);

	if (cold || sc->sc_poll)
		flags |= I2C_F_POLL;

	if (!I2C_OP_STOP_P(op) || cmdlen > 1 || len > 2)
		return (1);

	/* Setup transfer */
	sc->sc_i2c_xfer.op = op;
	sc->sc_i2c_xfer.buf = buf;
	sc->sc_i2c_xfer.len = len;
	sc->sc_i2c_xfer.flags = flags;
	sc->sc_i2c_xfer.error = 0;

	/* Set slave address and transfer direction */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBADDR,
	    AMDPM_SMBADDR_ADDR(addr) |
	    (I2C_OP_READ_P(op) ? AMDPM_SMBADDR_READ : 0));

	b = (void *)cmdbuf;
	if (cmdlen > 0)
		/* Set command byte */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, AMDPM_SMBCMD, b[0]);

	if (I2C_OP_WRITE_P(op)) {
		/* Write data */
		data = 0;
		b = buf;
		if (len > 0)
			data = b[0];
		if (len > 1)
			data |= ((u_int16_t)b[1] << 8);
		if (len > 0)
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    AMDPM_SMBDATA, data);
	}

	/* Set SMBus command */
	if (len == 0)
		ctl = AMDPM_SMBCTL_CMD_BYTE;
	else if (len == 1)
		ctl = AMDPM_SMBCTL_CMD_BDATA;
	else if (len == 2)
		ctl = AMDPM_SMBCTL_CMD_WDATA;

	if ((flags & I2C_F_POLL) == 0)
		ctl |= AMDPM_SMBCTL_CYCEN;

	/* Start transaction */
	ctl |= AMDPM_SMBCTL_START;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBCTL, ctl);

	if (flags & I2C_F_POLL) {
		/* Poll for completion */
		DELAY(AMDPM_SMBUS_DELAY);
		for (retries = 1000; retries > 0; retries--) {
			st = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    AMDPM_SMBSTAT);
			if ((st & AMDPM_SMBSTAT_HBSY) == 0)
				break;
			DELAY(AMDPM_SMBUS_DELAY);
		}
		if (st & AMDPM_SMBSTAT_HBSY)
			goto timeout;
		amdpm_intr(sc);
	} else {
		/* Wait for interrupt */
		if (tsleep(sc, PRIBIO, "iicexec", AMDPM_SMBUS_TIMEOUT * hz))
			goto timeout;
	}

	if (sc->sc_i2c_xfer.error)
		return (1);

	return (0);

timeout:
	/*
	 * Transfer timeout. Kill the transaction and clear status bits.
	 */
	printf("%s: timeout, status 0x%b\n", sc->sc_dev.dv_xname, st,
	    AMDPM_SMBSTAT_BITS);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBCTL,
	    AMDPM_SMBCTL_ABORT);
	DELAY(AMDPM_SMBUS_DELAY);
	st = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBSTAT);
	if ((st & AMDPM_SMBSTAT_ABRT) == 0)
		printf("%s: transaction abort failed, status 0x%b\n",
		    sc->sc_dev.dv_xname, st, AMDPM_SMBSTAT_BITS);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBSTAT, st);
	return (1);
}

int
amdpm_intr(void *arg)
{
	struct amdpm_softc *sc = arg;
	u_int16_t st, data;
	u_int8_t *b;
	size_t len;

	/* Read status */
	st = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBSTAT);
	if ((st & AMDPM_SMBSTAT_HBSY) != 0 || (st & (AMDPM_SMBSTAT_ABRT |
	    AMDPM_SMBSTAT_COL | AMDPM_SMBSTAT_PRERR | AMDPM_SMBSTAT_CYC |
	    AMDPM_SMBSTAT_TO | AMDPM_SMBSTAT_SNP | AMDPM_SMBSTAT_SLV |
	    AMDPM_SMBSTAT_SMBA)) == 0)
		/* Interrupt was not for us */
		return (0);

	DPRINTF(("%s: intr: st 0x%b\n", sc->sc_dev.dv_xname, st,
	    AMDPM_SMBSTAT_BITS));

	/* Clear status bits */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMDPM_SMBSTAT, st);

	/* Check for errors */
	if (st & (AMDPM_SMBSTAT_COL | AMDPM_SMBSTAT_PRERR |
	    AMDPM_SMBSTAT_TO)) {
		sc->sc_i2c_xfer.error = 1;
		goto done;
	}

	if (st & AMDPM_SMBSTAT_CYC) {
		if (I2C_OP_WRITE_P(sc->sc_i2c_xfer.op))
			goto done;

		/* Read data */
		b = sc->sc_i2c_xfer.buf;
		len = sc->sc_i2c_xfer.len;
		if (len > 0) {
			data = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    AMDPM_SMBDATA);
			b[0] = data & 0xff;
		}
		if (len > 1)
			b[1] = (data >> 8) & 0xff;
	}

done:
	if ((sc->sc_i2c_xfer.flags & I2C_F_POLL) == 0)
		wakeup(sc);
	return (1);
}
