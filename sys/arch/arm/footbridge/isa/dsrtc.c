/*	$OpenBSD: dsrtc.c,v 1.2 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: dsrtc.c,v 1.5 2003/03/23 14:12:26 chris Exp $	*/

/*
 * Copyright (c) 1998 Mark Brinicombe.
 * Copyright (c) 1998 Causality Limited.
 * All rights reserved.
 *
 * Written by Mark Brinicombe, Causality Limited
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUASLITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/rtc.h>

#include <arm/footbridge/todclockvar.h>
#include <arm/footbridge/isa/ds1687reg.h>

#include <dev/isa/isavar.h>

#define NRTC_PORTS	2

struct dsrtc_softc {
	struct device	sc_dev;
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
};

void dsrtcattach (struct device *parent, struct device *self, void *aux);
int dsrtcmatch (struct device *parent, void *cf, void *aux);
int ds1687_read (struct dsrtc_softc *sc, int addr);
void ds1687_write (struct dsrtc_softc *sc, int addr, int data);
int ds1687_ram_read (struct dsrtc_softc *sc, int addr);
void ds1687_ram_write (struct dsrtc_softc *sc, int addr, int data);
static void ds1687_bank_select (struct dsrtc_softc *, int);
static int dsrtc_write (void *, rtc_t *);
static int dsrtc_read (void *, rtc_t *);

int
ds1687_read(sc, addr)
	struct dsrtc_softc *sc;
	int addr;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, RTC_ADDR_REG, addr);
	return(bus_space_read_1(sc->sc_iot, sc->sc_ioh, RTC_DATA_REG));
}

void
ds1687_write(sc, addr, data)
	struct dsrtc_softc *sc;
	int addr;
	int data;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, RTC_ADDR_REG, addr);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, RTC_DATA_REG, data);
}

static void
ds1687_bank_select(sc, bank)
	struct dsrtc_softc *sc;
	int bank;
{
	int data;

	data = ds1687_read(sc, RTC_REG_A);
	data &= ~RTC_REG_A_BANK_MASK;
	if (bank)
		data |= RTC_REG_A_BANK1;
	ds1687_write(sc, RTC_REG_A, data);
}

#if 0
/* Nothing uses these yet */
int
ds1687_ram_read(sc, addr)
	struct dsrtc_softc *sc;
	int addr;
{
	if (addr < RTC_PC_RAM_SIZE)
		return(ds1687_read(sc, RTC_PC_RAM_START + addr));

	addr -= RTC_PC_RAM_SIZE;
	if (addr < RTC_BANK0_RAM_SIZE)
		return(ds1687_read(sc, RTC_BANK0_RAM_START + addr));		

	addr -= RTC_BANK0_RAM_SIZE;
	if (addr < RTC_EXT_RAM_SIZE) {
		int data;

		ds1687_bank_select(sc, 1);
		ds1687_write(sc, RTC_EXT_RAM_ADDRESS, addr);
		data = ds1687_read(sc, RTC_EXT_RAM_DATA);
		ds1687_bank_select(sc, 0);
		return(data);
	}
	return(-1);
}

void
ds1687_ram_write(sc, addr, val)
	struct dsrtc_softc *sc;
	int addr;
	int val;
{
	if (addr < RTC_PC_RAM_SIZE)
		return(ds1687_write(sc, RTC_PC_RAM_START + addr, val));

	addr -= RTC_PC_RAM_SIZE;
	if (addr < RTC_BANK0_RAM_SIZE)
		return(ds1687_write(sc, RTC_BANK0_RAM_START + addr, val));

	addr -= RTC_BANK0_RAM_SIZE;
	if (addr < RTC_EXT_RAM_SIZE) {
		ds1687_bank_select(sc, 1);
		ds1687_write(sc, RTC_EXT_RAM_ADDRESS, addr);
		ds1687_write(sc, RTC_EXT_RAM_DATA, val);
		ds1687_bank_select(sc, 0);
	}
}
#endif

static int
dsrtc_write(arg, rtc)
	void *arg;
	rtc_t *rtc;
{
	struct dsrtc_softc *sc = arg;

	ds1687_write(sc, RTC_SECONDS, rtc->rtc_sec);
	ds1687_write(sc, RTC_MINUTES, rtc->rtc_min);
	ds1687_write(sc, RTC_HOURS, rtc->rtc_hour);
	ds1687_write(sc, RTC_DAYOFMONTH, rtc->rtc_day);
	ds1687_write(sc, RTC_MONTH, rtc->rtc_mon);
	ds1687_write(sc, RTC_YEAR, rtc->rtc_year);
	ds1687_bank_select(sc, 1);
	ds1687_write(sc, RTC_CENTURY, rtc->rtc_cen);
	ds1687_bank_select(sc, 0);
	return(1);
}

static int
dsrtc_read(arg, rtc)
	void *arg;
	rtc_t *rtc;
{
	struct dsrtc_softc *sc = arg;

	rtc->rtc_micro = 0;
	rtc->rtc_centi = 0;
	rtc->rtc_sec   = ds1687_read(sc, RTC_SECONDS);
	rtc->rtc_min   = ds1687_read(sc, RTC_MINUTES);
	rtc->rtc_hour  = ds1687_read(sc, RTC_HOURS);
	rtc->rtc_day   = ds1687_read(sc, RTC_DAYOFMONTH);
	rtc->rtc_mon   = ds1687_read(sc, RTC_MONTH);
	rtc->rtc_year  = ds1687_read(sc, RTC_YEAR);
	ds1687_bank_select(sc, 1);
	rtc->rtc_cen   = ds1687_read(sc, RTC_CENTURY); 
	ds1687_bank_select(sc, 0);

	return(1);
}

/* device and attach structures */
struct cfattach ds1687rtc_ca = {
	sizeof(struct dsrtc_softc), dsrtcmatch, dsrtcattach
};

struct cfdriver ds1687rtc_cd = {
	NULL, "dsrtc", DV_DULL
};

/*
 * dsrtcmatch()
 *
 * Validate the IIC address to make sure its an RTC we understand
 */

int
dsrtcmatch(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct isa_attach_args *ia = aux;

#ifdef __NetBSD__
	if (ia->ia_nio < 1 ||
	    ia->ia_iobase == -1)
		return (0);

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = NRTC_PORTS;
#else
	ia->ia_iosize = NRTC_PORTS;
#endif


#if 0
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;
#endif

	return(1);
}

/*
 * dsrtcattach()
 *
 * Attach the rtc device
 */

void
dsrtcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct dsrtc_softc *sc = (struct dsrtc_softc *)self;
	struct isa_attach_args *ia = aux;
	struct todclock_attach_args ta;
	
	sc->sc_iot = ia->ia_iot;
	if (bus_space_map(sc->sc_iot, ia->ia_iobase,
	    ia->ia_iosize, 0, &sc->sc_ioh)) {
		printf(": cannot map I/O space\n");
		return;
	}

	ds1687_write(sc, RTC_REG_A, RTC_REG_A_DV1);
	ds1687_write(sc, RTC_REG_B, RTC_REG_B_BINARY | RTC_REG_B_24_HOUR);

	if (!(ds1687_read(sc, RTC_REG_D) & RTC_REG_D_VRT))
		printf(": lithium cell is dead, RTC unreliable");
	printf("\n");

	ta.ta_name = "todclock";
	ta.ta_rtc_arg = sc;
	ta.ta_rtc_write = dsrtc_write; 
	ta.ta_rtc_read = dsrtc_read;
	ta.ta_flags = 0;
	config_found(self, &ta, NULL);
}

/* End of dsrtc.c */
