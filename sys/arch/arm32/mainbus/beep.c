/* $NetBSD: beep.c,v 1.4 1996/03/27 22:08:36 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Simple beep sounds using VIDC
 */

/*
 * To use the driver, open /dev/beep and write lines. 
 * Each write will generate a beep  
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <dev/cons.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/irqhandler.h>
#include <machine/katelib.h>
#include <machine/iomd.h>
#include <machine/vidc.h>
#include <machine/pmap.h>
#include <machine/beep.h>
#include <arm32/mainbus/mainbus.h>
#include <arm32/mainbus/waveform.h>

#include "beep.h"

struct beep_softc {
	struct device sc_device;
	irqhandler_t sc_ih;
	int sc_iobase;
	int sc_open;
	int sc_count;
	u_int sc_sound_cur0; 
	u_int sc_sound_end0; 
	u_int sc_sound_cur1; 
	u_int sc_sound_end1; 
	vm_offset_t sc_buffer0;
	vm_offset_t sc_buffer1;
};

int	beepprobe	__P((struct device *parent, void *match, void *aux));
void	beepattach	__P((struct device *parent, struct device *self, void *aux));
int	beepopen	__P((dev_t, int, int, struct proc *));
int	beepclose	__P((dev_t, int, int, struct proc *));
int	beepintr	__P((struct beep_softc *sc));
void	beepdma		__P((struct beep_softc *sc, int buf));

struct cfattach beep_ca = {
	sizeof(struct beep_softc), beepprobe, beepattach
};

struct cfdriver	beep_cd = {
	NULL, "beep", DV_TTY
};


int
beepprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
/*	struct mainbus_attach_args *mb = aux;*/
	int id;

/* Make sure we have an IOMD we understand */

	id = ReadByte(IOMD_ID0) | (ReadByte(IOMD_ID1) << 8);

/* So far I only know about this IOMD */

	switch (id) {
	case RPC600_IOMD_ID:
		return(1);
		break;
	default:
		printf("beep: Unknown IOMD id=%04x", id);
		break;
	}
	return(0);
}


void
beepattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct beep_softc *sc = (void *)self;
	struct mainbus_attach_args *mb = aux;
    
	sc->sc_iobase = mb->mb_iobase;
	sc->sc_open = 0;
	sc->sc_count = 0;
    
	sc->sc_buffer0 = kmem_alloc(kernel_map, NBPG);
	if (sc->sc_buffer0 == 0) 
		panic("beep: Cannot allocate buffer memory");
	if ((sc->sc_buffer0 & (NBPG -1)) != 0)
		panic("beep: Cannot allocate page aligned buffer");
	sc->sc_buffer1 = sc->sc_buffer0;

	sc->sc_sound_cur0 = pmap_extract(kernel_pmap,
	    (vm_offset_t)sc->sc_buffer0 & PG_FRAME);
	sc->sc_sound_end0 = (sc->sc_sound_cur0 + NBPG - 16) | 0x00000000;
	sc->sc_sound_cur1 = pmap_extract(kernel_pmap,
	    (vm_offset_t)sc->sc_buffer1 & PG_FRAME);
	sc->sc_sound_end1 = (sc->sc_sound_cur1 + NBPG - 16) | 0x00000000;

	bcopy(beep_waveform, (void *)sc->sc_buffer0, sizeof(beep_waveform));

/* Reset the sound DMA channel */

	WriteWord(IOMD_SD0CURA, sc->sc_sound_cur0);
	WriteWord(IOMD_SD0ENDA, sc->sc_sound_end0 | 0xc0000000);
	WriteWord(IOMD_SD0CURB, sc->sc_sound_cur1);
	WriteWord(IOMD_SD0ENDB, sc->sc_sound_end1 | 0xc0000000);
    
	WriteByte(IOMD_SD0CR, 0x90);

/* Install an IRQ handler */

	sc->sc_ih.ih_func = beepintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_NONE;
	sc->sc_ih.ih_name = "dma snd ch 0";

	if (irq_claim(IRQ_DMASCH0, &sc->sc_ih))
		panic("Cannot claim DMASCH0 IRQ for beep%d", parent->dv_unit);

	disable_irq(IRQ_DMASCH0);

/*
	printf(" [ buf0=%08x:%08x->%08x buf1=%08x:%08x->%08x ]",
	    (u_int)sc->sc_buffer0, sc->sc_sound_cur0, sc->sc_sound_end0,
	    (u_int)sc->sc_buffer1, sc->sc_sound_cur1, sc->sc_sound_end1);
*/
	printf("\n");

/* Set sample rate to 32us */

	WriteWord(VIDC_BASE, VIDC_SFR | 32);

/* Set the stereo postions to centred for all channels */

	WriteWord(VIDC_BASE, VIDC_SIR0 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR1 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR2 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR3 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR4 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR5 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR6 | SIR_CENTRE);
	WriteWord(VIDC_BASE, VIDC_SIR7 | SIR_CENTRE);
}


int
beepopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct beep_softc *sc;
	int unit = minor(dev);
	int s;
    
	if (unit >= beep_cd.cd_ndevs)
		return(ENXIO);

	sc = beep_cd.cd_devs[unit];
	if (!sc) return(ENXIO);

/* HACK hack hack */

	s = splhigh();
	if (sc->sc_open) {
		(void)splx(s);
		return(EBUSY);
	}

	++sc->sc_open;   
	(void)splx(s); 

	return(0);
}


int
beepclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct beep_softc *sc = beep_cd.cd_devs[unit];
	int s;

	if (sc->sc_open == 0) return(ENXIO);
    
/* HACK hack hack */

	s = splhigh();
	--sc->sc_open;
	(void)splx(s);
      
	return(0);
}


void
beep_generate(void)
{
	struct beep_softc *sc = beep_cd.cd_devs[0];
/*	int status;*/

	if (sc->sc_count > 0) {
/*		printf("beep: active\n");*/
		return;
	}
/*	printf("beep: generate ");*/
	++sc->sc_count;
    
/*	status = ReadByte(IOMD_SD0ST);
	printf("st=%02x\n", status);*/
	WriteByte(IOMD_SD0CR, 0x90);
	WriteByte(IOMD_SD0CR, 0x30);
	beepdma(sc, 0);
}


int
beepioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct beep_softc *sc = beep_cd.cd_devs[minor(dev)];
	int rate;
	struct wavebuffer *wave = (struct wavebuffer *)data;

	switch (cmd) {
	case BEEP_GENERATE:
		beep_generate();
		break;

	case BEEP_SETRATE:
		rate = *(int *)data;
	    
		if (rate < 3 || rate > 255)
			return(EINVAL);

		WriteWord(VIDC_BASE, VIDC_SFR | rate);
		break;

	case BEEP_SET:
		printf("set %08x\n", (u_int)data);
		printf("set %08x %08x\n", (u_int)wave->addr, wave->size);
		if (wave->size < 16 || wave->size > NBPG)
			return(ENXIO);
		copyin(wave->addr, (char *)sc->sc_buffer0, wave->size);
		sc->sc_sound_end0 = (sc->sc_sound_cur0 + wave->size - 16);
		sc->sc_sound_end1 = (sc->sc_sound_cur1 + wave->size - 16);
		break;

	default:
		return(ENXIO);
		break;
	}   

	return(0);
}


int
beepintr(sc)
	struct beep_softc *sc;
{
/*	printf("beepintr: %02x,%02x,%02x,%d\n", ReadByte(IOMD_DMARQ),
	    ReadByte(IOMD_SD0CR), ReadByte(IOMD_SD0ST), sc->sc_count);*/
	WriteByte(IOMD_DMARQ, 0x10);
	--sc->sc_count;
	if (sc->sc_count <= 0) {
		WriteWord(IOMD_SD0CURB, sc->sc_sound_cur1);
		WriteWord(IOMD_SD0ENDB, sc->sc_sound_end1 | (1 << 30));
		disable_irq(IRQ_DMASCH0);
/*		printf("stop:st=%02x\n", ReadByte(IOMD_SD0ST));*/
		return(1);
	}

	beepdma(sc, sc->sc_count & 1);
	return(1);
}


void
beepdma(sc, buf)
	struct beep_softc *sc;
	int buf;
{
	int status;
/*	printf("beep:dma %d", buf);    */
	status = ReadByte(IOMD_SD0ST);
/*	printf("st=%02x\n", status);*/

	if (buf == 0) {
		WriteWord(IOMD_SD0CURA, sc->sc_sound_cur0);
		WriteWord(IOMD_SD0ENDA, sc->sc_sound_end0);
		WriteWord(IOMD_SD0CURB, sc->sc_sound_cur1);
		WriteWord(IOMD_SD0ENDB, sc->sc_sound_end1 | (1 << 30));
	}

	if (buf == 1) {
		WriteWord(IOMD_SD0CURB, sc->sc_sound_cur1);
		WriteWord(IOMD_SD0ENDB, sc->sc_sound_end1 | (1 << 30));
	}

/*	status = ReadByte(IOMD_SD0ST);
	printf("st=%02x\n", status);*/

	enable_irq(IRQ_DMASCH0);
}

/* End of beep.c */
