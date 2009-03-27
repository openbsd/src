/*	$OpenBSD: iopi2c.c,v 1.3 2009/03/27 16:02:41 oga Exp $	*/
/*	$NetBSD: iopi2c.c,v 1.3 2005/12/11 12:16:51 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Intel i80321 I/O Processor I2C Controller Unit support.
 */
#include <sys/param.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/i2c/i2cvar.h>

#include <arm/xscale/iopi2creg.h>
#include <arm/xscale/iopi2cvar.h>

static int iopiic_acquire_bus(void *, int);
static void iopiic_release_bus(void *, int);

static int iopiic_send_start(void *, int);
static int iopiic_send_stop(void *, int);
static int iopiic_initiate_xfer(void *, uint16_t, int);
static int iopiic_read_byte(void *, uint8_t *, int);
static int iopiic_write_byte(void *, uint8_t, int);

struct cfdriver iopiic_cd = {
	NULL, "iopiic", DV_DULL
};

void
iopiic_attach(struct iopiic_softc *sc)
{
	struct i2cbus_attach_args iba;

	sc->sc_i2c.ic_exec = NULL;
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = iopiic_acquire_bus;
	sc->sc_i2c.ic_release_bus = iopiic_release_bus;
	sc->sc_i2c.ic_send_start = iopiic_send_start;
	sc->sc_i2c.ic_send_stop = iopiic_send_stop;
	sc->sc_i2c.ic_initiate_xfer = iopiic_initiate_xfer;
	sc->sc_i2c.ic_read_byte = iopiic_read_byte;
	sc->sc_i2c.ic_write_byte = iopiic_write_byte;

	bzero(&iba, sizeof iba);
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_i2c;
	(void) config_found(&sc->sc_dev, &iba, iicbus_print);
}

static int
iopiic_acquire_bus(void *cookie, int flags)
{
	struct iopiic_softc *sc = cookie;

	/* XXX What should we do for the polling case? */
	if (flags & I2C_F_POLL)
		return (0);

	return (rw_enter(&sc->sc_buslock, RW_WRITE | RW_INTR));
}

static void
iopiic_release_bus(void *cookie, int flags)
{
	struct iopiic_softc *sc = cookie;

	/* XXX See above. */
	if (flags & I2C_F_POLL)
		return;

	rw_exit(&sc->sc_buslock);
}

#define	IOPIIC_TIMEOUT		100	/* protocol timeout, in uSecs */

static int
iopiic_wait(struct iopiic_softc *sc, int bit, int flags)
{
	uint32_t isr;
	int timeout, error=0;

	/* XXX We never sleep, we always poll.  Fix me. */

	/*
	 * For some reason, we seem to run into problems if we poll
	 * the ISR while the transfer is in progress--at least on the
	 * i80312.  The condition that we're looking for never seems
	 * to appear on a read, and it's not clear why; perhaps reads
	 * of the I2C register file interfere with its proper operation?
	 * For now, just delay for a while up front.
	 *
	 * We _really_ need this to be interrupt-driven, but a problem
	 * with that is that the i80312 has no way to mask interrupts...
	 * So we need to deal with that.  For DMA and AAU, too, for that
	 * matter.
	 * Note that delay(100) doesn't quite work on the npwr w/ m41t00.
	 */
	delay(200);
	for (timeout = IOPIIC_TIMEOUT; timeout != 0; timeout--) {
		isr = bus_space_read_4(sc->sc_st, sc->sc_sh, IIC_ISR);
		if (isr & (bit | IIC_ISR_BED))
			break;
		delay(1);
	}

	if (isr & (IIC_ISR_BED | (bit & IIC_ISR_ALD)))
		error = EIO;
	else if (isr & (bit & ~IIC_ISR_ALD))
		error = 0;
	else
		error = ETIMEDOUT;

#if 0
	if (error)
		printf("%s: iopiic_wait, (%08x) error %d: ISR = 0x%08x\n",
		    sc->sc_dev.dv_xname, bit, error, isr);
#endif

	/*
	 * The IIC_ISR is Read/Clear apart from the bottom 4 bits, which are
	 * read-only. So simply write back our copy of the ISR to clear any
	 * latched status.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_ISR, isr);

	return (error);
}

static int
iopiic_send_start(void *cookie, int flags)
{
	struct iopiic_softc *sc = cookie;

	/*
	 * This may only work in conjunction with a data transfer;
	 * we might need to un-export the "start" primitive.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_ICR,
	    sc->sc_icr | IIC_ICR_START);
	delay(IOPIIC_TIMEOUT);

	return (0);
}

static int
iopiic_send_stop(void *cookie, int flags)
{
	struct iopiic_softc *sc = cookie;

	/*
	 * The STOP bit is only used in conjunction with
	 * a data transfer, so we need to use MA in this
	 * case.
	 *
	 * Consider adding an I2C_F_STOP so we can
	 * do a read-with-STOP and write-with-STOP.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_ICR,
	    sc->sc_icr | IIC_ICR_MA);
	delay(IOPIIC_TIMEOUT);

	return (0);
}

static int
iopiic_initiate_xfer(void *cookie, uint16_t addr, int flags)
{
	struct iopiic_softc *sc = cookie;
	int error, rd_req = (flags & I2C_F_READ) != 0;
	uint32_t idbr;

	/* We only support 7-bit addressing. */
	if ((addr & 0x78) == 0x78)
		return (EINVAL);

	idbr = (addr << 1) | (rd_req ? 1 : 0);
	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_IDBR, idbr);
	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_ICR,
	    sc->sc_icr | IIC_ICR_START | IIC_ICR_TB);

	error = iopiic_wait(sc, IIC_ISR_ITE, flags);
#if 0
	if (error)
		printf("%s: failed to initiate %s xfer\n", sc->sc_dev.dv_xname,
		    rd_req ? "read" : "write");
#endif
	return (error);
}

static int
iopiic_read_byte(void *cookie, uint8_t *bytep, int flags)
{
	struct iopiic_softc *sc = cookie;
	int error, last_byte = (flags & I2C_F_LAST) != 0,
	    send_stop = (flags & I2C_F_STOP) != 0;

	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_ICR,
	    sc->sc_icr | IIC_ICR_TB | (last_byte ? IIC_ICR_NACK : 0) |
	    (send_stop ? IIC_ICR_STOP : 0));
	if ((error = iopiic_wait(sc, IIC_ISR_IRF | IIC_ISR_ALD, flags)) == 0)
		*bytep = bus_space_read_4(sc->sc_st, sc->sc_sh, IIC_IDBR);
#if 0
	if (error)
		printf("%s: read byte failed\n", sc->sc_dev.dv_xname);
#endif

	return (error);
}

static int
iopiic_write_byte(void *cookie, uint8_t byte, int flags)
{
	struct iopiic_softc *sc = cookie;
	int error, send_stop = (flags & I2C_F_STOP) != 0;

	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_IDBR, byte);
	bus_space_write_4(sc->sc_st, sc->sc_sh, IIC_ICR,
	    sc->sc_icr | IIC_ICR_TB | (send_stop ? IIC_ICR_STOP : 0));
	error = iopiic_wait(sc, IIC_ISR_ITE | IIC_ISR_ALD, flags);

#if 0
	if (error)
		printf("%s: write byte failed\n", sc->sc_dev.dv_xname);
#endif

	return (error);
}
