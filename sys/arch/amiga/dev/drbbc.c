/*	$OpenBSD: drbbc.c,v 1.2 2002/03/14 01:26:28 millert Exp $	*/
/*	$NetBSD: drbbc.c,v 1.1 1997/07/17 23:29:30 is Exp $	*/

/*
 * Copyright (c) 1997 Ignatios Souvatzis.
 * All rights reserved.
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
 *	This product includes software developed by Ignatios Souvatzis
 *	for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#if 0
#include <machine/psl.h>
#endif
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>
#include <amiga/amiga/drcustom.h>
#include <amiga/dev/rtc.h>

#include <dev/ic/ds.h>

int draco_ds_read_bit(void *);
void draco_ds_write_bit(void *, int);
void draco_ds_reset(void *);

void drbbc_attach(struct device *, struct device *, void *);
int drbbc_match(struct device *, void *, void *);

time_t dracogettod(void);
#ifdef __NOTYET__
int dracosettod(time_t);
#endif

struct drbbc_softc {
	struct device sc_dev;
	struct ds_handle sc_dsh;
};

struct cfattach drbbc_ca = {
	sizeof(struct drbbc_softc),
	drbbc_match,
	drbbc_attach
};

struct cfdriver drbbc_cd = {  
        NULL, "drbbc", DV_DULL, NULL, 0
};      

struct drbbc_softc *drbbc_sc;

int
drbbc_match(pdp, match, auxp)
	struct device *pdp;
	void *match;
	void *auxp;
{
	struct cfdata *cfp = match;

	if (is_draco() && matchname(auxp, "drbbc") && (cfp->cf_unit == 0))
		return (1);
	else
		return (0);
}

void
drbbc_attach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	int i;
	struct drbbc_softc *sc;
	u_int8_t rombuf[8];

	sc = (struct drbbc_softc *)dp;

	sc->sc_dsh.ds_read_bit = draco_ds_read_bit;
	sc->sc_dsh.ds_write_bit = draco_ds_write_bit;
	sc->sc_dsh.ds_reset = draco_ds_reset;
	sc->sc_dsh.ds_hw_handle = (void *)(DRCCADDR + DRIOCTLPG*NBPG);

	sc->sc_dsh.ds_reset(sc->sc_dsh.ds_hw_handle);

	ds_write_byte(&sc->sc_dsh, DS_ROM_READ);
	for (i=0; i<8; ++i) 
		rombuf[i] = ds_read_byte(&sc->sc_dsh);

	hostid = (rombuf[3] << 24) + (rombuf[2] << 16) +
		(rombuf[1] << 8) + rombuf[7];

	printf(": ROM %02x %02x%02x%02x%02x%02x%02x %02x (DraCo sernum %ld)\n",
		rombuf[7], rombuf[6], rombuf[5], rombuf[4], 
		rombuf[3], rombuf[2], rombuf[1], rombuf[0],
		hostid); 
		
	gettod = dracogettod;
	settod = (void *)0;
	drbbc_sc = sc;
}

int
draco_ds_read_bit(p)
	void *p;
{
	struct drioct *draco_ioct;

	draco_ioct = p;

	while (draco_ioct->io_status & DRSTAT_CLKBUSY);

	draco_ioct->io_clockw1 = 0;

	while (draco_ioct->io_status & DRSTAT_CLKBUSY);

	return (draco_ioct->io_status & DRSTAT_CLKDAT);
}

void
draco_ds_write_bit(p, b)
	void *p;
	int b;
{
	struct drioct *draco_ioct;

	draco_ioct = p;

	while (draco_ioct->io_status & DRSTAT_CLKBUSY);

	if (b)
		draco_ioct->io_clockw1 = 0;
	else
		draco_ioct->io_clockw0 = 0;
}

void
draco_ds_reset(p)
	void *p;
{
	struct drioct *draco_ioct;

	draco_ioct = p;

	draco_ioct->io_clockrst = 0;
}

/*
 * We could return 1/256 of a seconds, but would need to change the interface
 */

time_t
dracogettod()
{
	u_int32_t clkbuf;

	drbbc_sc->sc_dsh.ds_reset(drbbc_sc->sc_dsh.ds_hw_handle);

	ds_write_byte(&drbbc_sc->sc_dsh, DS_ROM_SKIP);

	ds_write_byte(&drbbc_sc->sc_dsh, DS_MEM_READ_MEMORY);
	/* address of full seconds: */
	ds_write_byte(&drbbc_sc->sc_dsh, 0x03);
	ds_write_byte(&drbbc_sc->sc_dsh, 0x02);
	
	clkbuf = ds_read_byte(&drbbc_sc->sc_dsh)
	    + (ds_read_byte(&drbbc_sc->sc_dsh)<<8)
	    + (ds_read_byte(&drbbc_sc->sc_dsh)<<16)
	    + (ds_read_byte(&drbbc_sc->sc_dsh)<<24);

	/* BSD time is wr. 1.1.1970; AmigaOS time wrt. 1.1.1978 */

	clkbuf += (8*365 + 2) * 86400;	

	return ((time_t)clkbuf);
}
