/*	$OpenBSD: gpr.c,v 1.11 2005/01/27 17:04:55 millert Exp $	*/

/*
 * Copyright (c) 2002, Federico G. Schwindt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * A driver for the Gemplus GPR400 SmartCard reader.
 *
 * The gpr400 driver written by Wolf Geldmacher <wgeldmacher@paus.ch> for
 * Linux was used as documentation.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/pcmcia/gprio.h>

/* Registers in I/O space (32 bytes) */
#define GPR400_HAP_CTRL		0x00	/* Handshake and PRG Control	*/
#define  GPR400_RESET		  0x01	/* Master reset			*/
#define  GPR400_IREQ		  0x02	/* Interrupt request		*/
#define  GPR400_INTR		  0x04	/* Interrupt			*/
					/* bits 3..8 PRG control	*/
#define GPR400_PD_CTRL		0x01	/* PRG data			*/
/* bytes 3..32 used for data exchange */

/* Registers in attribute memory (read only) */ 
#define GPR400_SETUP		0x018	/* General Setup 		*/
#define  GPR400_LOCK_MASK	 0x08	/* 0: locked, 1: unlocked	*/
#define GPR400_REG1		0x01a	/* SmartCard Reg. 1 		*/
#define  GPR400_DET_MASK	 0x08	/* 0: in the reader, 1: removed	*/
#define  GPR400_INS_MASK	 0x80	/* 0: not inserted, 1: inserted	*/
#define GPR400_REG2		0x01c	/* SmartCard Reg. 2 		*/
#define GPR400_CAC		0x01e	/* Clock and Control 		*/

/* TLV */
#define GPR400_CLOSE		0x10	/* Close session		*/
#define GPR400_OPEN		0x20	/* Open session			*/
#define GPR400_APDU		0x30	/* APDU exchange		*/
#define GPR400_POWER		0x40	/* Power down/Standby		*/
					/* 0: Power down, 1: Standby	*/
#define GPR400_SELECT		0x50	/* Select card			*/
#define  GPR400_DRV0		 0x00	/* Downloaded driver 0		*/
#define  GPR400_ISODRV		 0x02	/* ISO7816-3 driver		*/
#define  GPR400_CLK_MASK	 0x08	/* 0: 3.68MHz, 1: 7.36MHz	*/
#define GPR400_STATUS		0xA0	/* Reader status		*/

#define GPR400_CONT		0x04	/* Chain block			*/

#define GPR400_MEM_LEN		0x1000

#define GPRUNIT(x)		(minor(x) & 0x0f)

#ifdef GPRDEBUG
int gprdebug;
#define DPRINTF(x)		if (gprdebug) printf x
#else
#define DPRINTF(x)
#endif

struct gpr_softc {
	struct device			sc_dev;

	struct pcmcia_function         *sc_pf;

	bus_space_handle_t		sc_ioh;
	bus_space_tag_t			sc_iot;

	struct pcmcia_io_handle		sc_pioh;
	int				sc_iowin;

	bus_space_handle_t		sc_memh;
	bus_space_tag_t			sc_memt;

	struct pcmcia_mem_handle	sc_pmemh;
	int				sc_memwin;
	bus_addr_t			sc_offset;

	void *				sc_ih;
};

int	gpr_match(struct device *, void *, void *);
void	gpr_attach(struct device *, struct device *, void *);
int	gpr_detach(struct device *, int);
int	gpr_activate(struct device *, enum devact);

int	gpropen(dev_t, int, int, struct proc *);
int	gprclose(dev_t, int, int, struct proc *);
int	gprioctl(dev_t, u_long, caddr_t, int, struct proc *);

int	gpr_intr(void *);

int	tlvput(struct gpr_softc *, int, u_int8_t *, int);

struct cfattach gpr_ca = {
	sizeof(struct gpr_softc), gpr_match, gpr_attach, gpr_detach,
	    gpr_activate
};

struct cfdriver gpr_cd = {
	NULL, "gpr", DV_DULL
};

int
gpr_match(struct device *parent, void *match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_GEMPLUS &&
	    pa->product == PCMCIA_PRODUCT_GEMPLUS_GPR400)
		return (1);

	return (0);
}

void
gpr_attach(struct device *parent, struct device *self, void *aux)
{
	struct gpr_softc *sc = (void *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	const char *intrstr;

	for (cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head); cfe;
	     cfe = SIMPLEQ_NEXT(cfe, cfe_list)) {

		if (!pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
		    cfe->iospace[0].length, cfe->iospace[0].length,
		    &sc->sc_pioh))
			break;
	}

	if (cfe == NULL) {
		printf(": can't alloc i/o space\n");
		goto fail_io_alloc;
	}

	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf)) {
		printf(": function enable failed\n");
		goto fail_enable;
	}

	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO, 0,
	    sc->sc_pioh.size, &sc->sc_pioh, &sc->sc_iowin)) {
		printf(": can't map i/o space\n");
		goto fail_io_map;
	}

	/*
	 * GPR400 has some registers in attribute memory as well.
	 */
	if (pcmcia_mem_alloc(pa->pf, GPR400_MEM_LEN, &sc->sc_pmemh)) {
		printf(": can't map mem space\n");
		goto fail_mem_alloc;
	}

	if (pcmcia_mem_map(pa->pf, PCMCIA_MEM_ATTR, pa->pf->ccr_base,
	    GPR400_MEM_LEN, &sc->sc_pmemh, &sc->sc_offset, &sc->sc_memwin)) {
		printf(": can't map memory\n");
		goto fail_mem_map;
	}

	sc->sc_pf = pa->pf;
	sc->sc_iot = sc->sc_pioh.iot;
	sc->sc_ioh = sc->sc_pioh.ioh;
	sc->sc_memt = sc->sc_pmemh.memt;
	sc->sc_memh = sc->sc_pmemh.memh;

	printf(" port 0x%lx/%d", sc->sc_pioh.addr, sc->sc_pioh.size);

	sc->sc_ih = pcmcia_intr_establish(pa->pf, IPL_TTY, gpr_intr, sc,
	    sc->sc_dev.dv_xname);
	intrstr = pcmcia_intr_string(psc->sc_pf, sc->sc_ih);
	printf("%s%s\n", *intrstr ? ", " : "", intrstr);
	if (sc->sc_ih != NULL)
		return;

fail_intr:
	pcmcia_mem_unmap(pa->pf, sc->sc_memwin);
fail_mem_map:
	pcmcia_mem_free(pa->pf, &sc->sc_pmemh);
fail_mem_alloc:
	pcmcia_io_unmap(pa->pf, sc->sc_iowin);
fail_io_map:
	pcmcia_function_disable(pa->pf);
fail_enable:
	pcmcia_io_free(pa->pf, &sc->sc_pioh);
fail_io_alloc:
	return;
}

int
gpr_detach(struct device *dev, int flags)
{
	struct gpr_softc *sc = (struct gpr_softc *)dev;

	pcmcia_io_unmap(sc->sc_pf, sc->sc_iowin);
	pcmcia_io_free(sc->sc_pf, &sc->sc_pioh);
	pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwin);
	pcmcia_mem_free(sc->sc_pf, &sc->sc_pmemh);

	return (0);
}

int
gpr_activate(struct device *dev, enum devact act)
{
	struct gpr_softc *sc = (struct gpr_softc *)dev;

	switch (act) {
	case DVACT_ACTIVATE:
		pcmcia_function_enable(sc->sc_pf);
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_TTY,
		    gpr_intr, sc, sc->sc_dev.dv_xname);
		break;

	case DVACT_DEACTIVATE:
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		pcmcia_function_disable(sc->sc_pf);
		break;
	}

	return (0);
}

int
gpropen(dev_t dev, int flags, int mode, struct proc *p)
{
	int unit = GPRUNIT(dev);
	struct gpr_softc *sc;

	DPRINTF(("%s: flags %d, mode %d\n", __func__, flags, mode));

	if (unit >= gpr_cd.cd_ndevs ||
	    (sc = gpr_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	return (tlvput(sc, GPR400_SELECT, "\x02", 1));
}

int
gprclose(dev_t dev, int flags, int mode, struct proc *p)
{
	int unit = GPRUNIT(dev);
	struct gpr_softc *sc = gpr_cd.cd_devs[unit];

	DPRINTF(("%s: flags %d, mode %d\n", __func__, flags, mode));

	(void)tlvput(sc, GPR400_CLOSE, (u_int8_t *)0, 0);

	return (0);
}

int
gprioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	int unit = GPRUNIT(dev);
	struct gpr_softc *sc = gpr_cd.cd_devs[unit];
	int error;

	DPRINTF(("%s: cmd %d, flags 0x%x\n", __func__, cmd, flags));

	switch (cmd) {
	case GPR_RESET:
		/*
		 * To reset and power up the reader, set bit 0 in the
		 * HAP register for at least 5us and wait for 20ms.
		 */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, GPR400_HAP_CTRL,
		    GPR400_RESET);
		delay(10);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, GPR400_HAP_CTRL, 0);
		tsleep(sc, PWAIT, "gpreset", hz / 40);
		/* FALLTHROUGH */

	case GPR_SELECT:
		error = tlvput(sc, GPR400_SELECT, "\x02", 1);
		break;

	case GPR_POWER:
		{
			u_int8_t *mode;

			if (*(int *)addr)
				mode = "\x01";	/* Standby	*/
			else
				mode = "\x00";	/* Power down	*/

			error = tlvput(sc, GPR400_POWER, mode, 1);
		}
		break;

	case GPR_CLOSE:
		error = tlvput(sc, GPR400_CLOSE, (u_int8_t *)0, 0);
		break;

	case GPR_RAM:
		{
			struct gpr400_ram r;

			bus_space_read_region_1(sc->sc_memt, sc->sc_memh,
		    	    sc->sc_offset, &r, sizeof(struct gpr400_ram));
			error = copyout(&r, addr, sizeof(struct gpr400_ram));
		}
		break;

	case GPR_CMD:
	case GPR_OPEN:
	case GPR_STATUS:
	case GPR_TLV:
	default:
		error = EINVAL;
		break;
	};

	return (error);
}

int
gpr_intr(void *arg)
{
	struct gpr_softc *sc = arg;
	u_int8_t val;

	DPRINTF(("%s: got interrupt\n", __func__));

	/* Ack interrupt */
	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, GPR400_HAP_CTRL);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, GPR400_HAP_CTRL,
	    val & ~GPR400_INTR);

	wakeup(sc);

	return (1);
}

int
tlvput(struct gpr_softc *sc, int cmd, u_int8_t *data, int len)
{
	int resid, ret;

	DPRINTF(("%s: cmd 0x%x, data %p, len %d\n", __func__,
	    cmd, data, len));

	resid = len;
	do {
		int n, s;

		n = min(resid, 28);
		resid -= n;

		if (resid)
			cmd |= GPR400_CONT;
		else
			cmd &= ~GPR400_CONT;

		DPRINTF(("%s: sending cmd 0x%x, len %d, left %d\n",
		    __func__, cmd, n, resid));

		bus_space_write_1(sc->sc_iot, sc->sc_ioh, 0x02, cmd);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, 0x03, n);

		if (n) {
			bus_space_write_region_1(sc->sc_iot, sc->sc_ioh,
			    0x04, data, n);
			data += n;
		}

		s = spltty();

		/* Tell the reader to process this command. */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, GPR400_HAP_CTRL,
		    GPR400_IREQ);

		tsleep(sc, PCATCH, "tlvput", 0);

		splx(s);

		/* Read the status.	*/
		ret = bus_space_read_1(sc->sc_iot, sc->sc_ioh, 0x04);

		DPRINTF(("%s: ret %d\n", __func__, ret));

		if (ret != 0x00 || (!resid && ret != 0xe7))
			return (EIO);

	} while (resid > 0);

	return (0);
}
