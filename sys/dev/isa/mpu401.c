/*	$OpenBSD: mpu401.c,v 1.1 1999/01/02 00:02:42 niklas Exp $	*/
/*	$NetBSD: mpu401.c,v 1.3 1998/11/25 22:17:06 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/midi_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/mpu401var.h>

#define splaudio() splbio()	/* XXX found in audio_if.h normally */

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (mpu401debug) printf x
#define DPRINTFN(n,x)	if (mpu401debug >= (n)) printf x
int	mpu401debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define MPU401_NPORT	2
#define MPU_DATA		0
#define MPU_COMMAND	1
#define  MPU_RESET	0xff
#define  MPU_UART_MODE	0x3f
#define  MPU_ACK		0xfe
#define MPU_STATUS	1
#define  MPU_OUTPUT_BUSY	0x40
#define  MPU_INPUT_EMPTY	0x80

#define MPU_MAXWAIT	10000	/* usec/10 to wait */

#define MPU_GETSTATUS(iot, ioh) (bus_space_read_1(iot, ioh, MPU_STATUS))

int	mpu401_reset(struct mpu401_softc *);
static	__inline int mpu401_waitready(struct mpu401_softc *);
void	mpu401_readinput(struct mpu401_softc *);

int
mpu401_find(sc)
	struct mpu401_softc *sc;
{
	if (MPU_GETSTATUS(sc->iot, sc->ioh) == 0xff) {
		DPRINTF(("mpu401_find: No status\n"));
		goto bad;
	}
	sc->open = 0;
	sc->intr = 0;
	if (mpu401_reset(sc) == 0)
		return 1;
bad:
	return 0;
}

static __inline int
mpu401_waitready(sc)
	struct mpu401_softc *sc;
{
	int i;

	for(i = 0; i < MPU_MAXWAIT; i++) {
		if (!(MPU_GETSTATUS(sc->iot, sc->ioh) & MPU_OUTPUT_BUSY))
			return 0;
		delay(10);
	}
	return 1;
}

int
mpu401_reset(sc)
	struct mpu401_softc *sc;
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	int i;
	int s;

	if (mpu401_waitready(sc)) {
		DPRINTF(("mpu401_reset: not ready\n"));
		return EIO;
	}
	s = splaudio();		/* Don't let the interrupt get our ACK. */
	bus_space_write_1(iot, ioh, MPU_COMMAND, MPU_RESET);
	for(i = 0; i < 2*MPU_MAXWAIT; i++) {
		if (!(MPU_GETSTATUS(iot, ioh) & MPU_INPUT_EMPTY) &&
		    bus_space_read_1(iot, ioh, MPU_DATA) == MPU_ACK) {
			splx(s);
			return 0;
		}
	}
	splx(s);
	DPRINTF(("mpu401_reset: No ACK\n"));
	return EIO;
}

int
mpu401_open(sc, flags, iintr, ointr, arg)
	struct mpu401_softc *sc;
	int flags;
	void (*iintr)__P((void *, int));
	void (*ointr)__P((void *));
	void *arg;
{
        DPRINTF(("mpu401_open: sc=%p\n", sc));

	if (sc->open)
		return EBUSY;
	if (mpu401_reset(sc) != 0)
		return EIO;

	bus_space_write_1(sc->iot, sc->ioh, MPU_COMMAND, MPU_UART_MODE);
	sc->open = 1;
	sc->intr = iintr;
	sc->arg = arg;
	return 0;
}

void
mpu401_close(sc)
	struct mpu401_softc *sc;
{
        DPRINTF(("mpu401_close: sc=%p\n", sc));

	sc->open = 0;
	sc->intr = 0;
	mpu401_reset(sc); /* exit UART mode */
}

void
mpu401_readinput(sc)
	struct mpu401_softc *sc;
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t ioh = sc->ioh;
	int data;

	while(!(MPU_GETSTATUS(iot, ioh) & MPU_INPUT_EMPTY)) {
		data = bus_space_read_1(iot, ioh, MPU_DATA);
		DPRINTFN(3, ("mpu401_rea: sc=%p 0x%02x\n", sc, data));
		if (sc->intr)
			sc->intr(sc->arg, data);
	}
}

int
mpu401_output(sc, d)
	struct mpu401_softc *sc;
	int d;
{
	int s;

	DPRINTFN(3, ("mpu401_output: sc=%p 0x%02x\n", sc, d));
	if (!(MPU_GETSTATUS(sc->iot, sc->ioh) & MPU_INPUT_EMPTY)) {
		s = splaudio();
		mpu401_readinput(sc);
		splx(s);
	}
	if (mpu401_waitready(sc)) {
		DPRINTF(("mpu401_output: not ready\n"));
		return EIO;
	}
	bus_space_write_1(sc->iot, sc->ioh, MPU_DATA, d);
	return 0;
}

void
mpu401_intr(sc)
	struct mpu401_softc *sc;
{
	if (MPU_GETSTATUS(sc->iot, sc->ioh) & MPU_INPUT_EMPTY) {
		DPRINTF(("mpu401_intr: no data\n"));
		return;
	}
	mpu401_readinput(sc);
}
