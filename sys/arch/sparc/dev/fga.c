/*	$OpenBSD: fga.c,v 1.15 2006/02/22 22:32:12 miod Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * This software was developed by Jason L. Wright under contract with
 * RTMX Incorporated (http://www.rtmx.com).
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the Force Gate Array 5000 (VME/SBus bridge) found
 * on FORCE CPU-5V boards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>
#include <machine/pmap.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>	/* for SBUS_BURST_* */

#include <sparc/dev/fgareg.h>
#include <sparc/dev/fgavar.h>
#include <machine/fgaio.h>

int	fgamatch(struct device *, void *, void *);
void	fgaattach(struct device *, struct device *, void *);
int	fvmematch(struct device *, void *, void *);
void	fvmeattach(struct device *, struct device *, void *);
int	fgaprint(void *, const char *);
int	fvmeprint(void *, const char *);
int	fvmescan(struct device *parent, void *, void *);

struct fga_softc {
	struct		device sc_dev;		/* base device */
	int		sc_node;		/* prom node */
	struct		fga_regs *sc_regs;	/* registers */
	struct		intrhand sc_ih1;	/* level 1 handler */
	struct		intrhand sc_ih2;	/* level 2 handler */
	struct		intrhand sc_ih3;	/* level 3 handler */
	struct		intrhand sc_ih4;	/* level 4 handler */
	struct		intrhand sc_ih5;	/* level 5 handler */
	struct		intrhand sc_ih6;	/* level 6 handler */
	struct		intrhand sc_ih7;	/* level 7 handler */
	struct		intrhand **sc_vmevec;	/* vectored handlers */
#ifdef DDB
	int		sc_abort;		/* abort switch enabled? */
#endif
	int		sc_nrange;		/* number of sbus ranges */
	struct		rom_range *sc_range;	/* sbus range data */
	u_int8_t	sc_established;		/* which hw intrs installed */
};

int	fgaopen(dev_t, int, int, struct proc *);
int	fgaclose(dev_t, int, int, struct proc *);
int	fgaioctl(dev_t, u_long, caddr_t, int, struct proc *);

int	fga_vmerangemap(struct fga_softc *, u_int32_t, u_int32_t,
    int, int, u_int32_t, struct confargs *);
int	fga_intr_establish(struct fga_softc *, int, int,
    struct intrhand *, const char *);
int	fga_hwintr_establish(struct fga_softc *, u_int8_t);

int	fga_hwintr1(void *);
int	fga_hwintr2(void *);
int	fga_hwintr3(void *);
int	fga_hwintr4(void *);
int	fga_hwintr5(void *);
int	fga_hwintr6(void *);
int	fga_hwintr7(void *);
int	fga_intrvec(struct fga_softc *, int);

struct cfattach fga_ca = {
	sizeof (struct fga_softc), fgamatch, fgaattach
};

struct cfdriver fga_cd = {
	NULL, "fga", DV_DULL
};

struct fvme_softc {
	struct		device sc_dv;
	u_int32_t	sc_vmeoffset;		/* vme range offset */
	u_int32_t	sc_len;			/* length of range */
	u_int32_t	sc_sbusoffset;		/* sbus phys address */
	u_int32_t	sc_type;		/* amcode type */
};

struct cfattach fvme_ca = {
	sizeof (struct fvme_softc), fvmematch, fvmeattach
};

struct cfdriver fvme_cd = {
	NULL, "fvme", DV_DULL
};

int
fgamatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp("VME", ra->ra_name))
		return (0);
	return (1);
}

void    
fgaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux, oca;
	struct fga_softc *sc = (struct fga_softc *)self;
	char *s;
	int i, rlen;

	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	/* map registers */
	if (ca->ca_ra.ra_nreg != 1) {
		printf(": expected 1 register, got %d\n", ca->ca_ra.ra_nreg);
		return;
	}
	sc->sc_regs = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
	    ca->ca_ra.ra_reg[0].rr_len);

	sc->sc_node = ca->ca_ra.ra_node;

	i = opennode("/iommu/sbus");
	if (i == 0) {
		printf(": no iommu or sbus found, unconfigured\n");
		return;
	}
	rlen = getproplen(i, "ranges");
	sc->sc_range =
	    (struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
	if (sc->sc_range == NULL) {
		printf(": PROM ranges too large: %d\n", rlen);
		return;
	}
	sc->sc_nrange = rlen / sizeof(struct rom_range);
	(void)getprop(i, "ranges", sc->sc_range, rlen);

	s = getpropstring(sc->sc_node, "model");
	printf(": model %s id %c%c%c\n", s,
	    sc->sc_regs->id[0], sc->sc_regs->id[1], sc->sc_regs->id[2]);

	/*
	 * The prom leaves at least one of the ranges "on", so make sure
	 * they are all off.
	 */
	for (i = 0; i < 16; i++)
		sc->sc_regs->vme_range[i] |= VME_RANGE_DECEN;

	sc->sc_regs->sbus_ssel[0] &= ~(SBUS_SSEL_Y);
	sc->sc_regs->sbus_ssel[0] |= SBUS_SSEL_Y_SLOT4;
	sc->sc_regs->sbus_ssel[1] &= ~(SBUS_SSEL_X | SBUS_SSEL_Y);
	sc->sc_regs->sbus_ssel[1] |= SBUS_SSEL_X_SLOT5 | SBUS_SSEL_Y_SLOT5;
	sc->sc_regs->sbus_ssel[2] &= ~(SBUS_SSEL_X | SBUS_SSEL_Y);
	sc->sc_regs->sbus_ssel[2] |= SBUS_SSEL_X_SLOT5 | SBUS_SSEL_Y_SLOT1;
	sc->sc_regs->sbus_ssel[3] &= ~(SBUS_SSEL_X | SBUS_SSEL_Y);
	sc->sc_regs->sbus_ssel[3] |= SBUS_SSEL_X_SLOT1 | SBUS_SSEL_Y_SLOT1;

	/*
	 * Map and attach vme<->sbus master ranges
	 */
	fga_vmerangemap(sc, 0xf0000000, 0x10000000,
	    VME_MASTER_CAP_D32 | VME_MASTER_CAP_A32, 1, 0, &oca);
	fga_vmerangemap(sc, 0xf0000000, 0x10000000,
	    VME_MASTER_CAP_D16 | VME_MASTER_CAP_A32, 4, 0, &oca);
	fga_vmerangemap(sc, 0x00000000, 0x01000000,
	    VME_MASTER_CAP_D16 | VME_MASTER_CAP_A24, 5, 0xe000000, &oca);
	fga_vmerangemap(sc, 0x00000000, 0x00010000,
	    VME_MASTER_CAP_D8 | VME_MASTER_CAP_A16, 5, 0x0ffc0000, &oca);
	fga_vmerangemap(sc, 0x00000000, 0x00010000,
	    VME_MASTER_CAP_D16 | VME_MASTER_CAP_A16, 5, 0x0ffd0000, &oca);
	fga_vmerangemap(sc, 0x00000000, 0x00010000,
	    VME_MASTER_CAP_D32 | VME_MASTER_CAP_A16, 5, 0x0ffe0000, &oca);

#ifdef DDB
	s = getpropstring(optionsnode, "abort-ena?");
	if (s && strcmp(s, "true") == 0) {
		sc->sc_abort = 1;
		fga_hwintr_establish(sc, IRQ_MAP_SINT7);
		sc->sc_regs->abort_irq_map &= ~IRQ_MAP_INT_MASK;
		sc->sc_regs->abort_irq_map |= IRQ_MAP_SINT7;
		sc->sc_regs->abort_irq_map &= ~IRQ_MAP_ENABLE;
	}
#endif
}

int
fgaprint(args, name)
	void *args;
	const char *name;
{
	struct confargs *ca = args;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	printf(" slot %d addr 0x%x", ca->ca_slot, ca->ca_offset);
	return (UNCONF);
}

/*
 * Map a region of VME space to a sbus slot/offset.
 */
int
fga_vmerangemap(sc, vmebase, vmelen, vmecap, sbusslot, sbusoffset, oca)
	struct fga_softc *sc;
	u_int32_t vmebase, vmelen;
	int vmecap;
	int sbusslot;
	u_int32_t sbusoffset;
	struct confargs *oca;
{
	struct fga_regs *regs = sc->sc_regs;
	u_int32_t rval;
	u_int8_t sval;
	int range, i, srange;

	for (i = 0; i < FVME_MAX_RANGES; i++) {
		if (regs->vme_range[i] & VME_RANGE_DECEN)
			break;
	}
	if (i == FVME_MAX_RANGES)
		return (-1);
	range = i;

	for (srange = 0; srange < sc->sc_nrange; srange++) {
		if (sbusslot == sc->sc_range[srange].cspace)
			break;
	}
	if (srange == sc->sc_nrange)
		return (-1);

	bzero(oca, sizeof(*oca));
	oca->ca_bustype = BUS_FGA;
	oca->ca_slot = sbusslot;
	oca->ca_offset = sc->sc_range[srange].poffset | sbusoffset;
	oca->ca_ra.ra_name = "fvme";
	oca->ca_ra.ra_nreg = 2;
	oca->ca_ra.ra_reg[0].rr_len = vmelen;
	oca->ca_ra.ra_reg[0].rr_paddr =
	    (void *)(sc->sc_range[srange].poffset | sbusoffset);
	oca->ca_ra.ra_reg[0].rr_iospace = sbusslot;
	oca->ca_ra.ra_reg[1].rr_iospace = vmecap;
	oca->ca_ra.ra_reg[1].rr_paddr = (void *)vmebase;
	oca->ca_ra.ra_reg[1].rr_len = vmelen;

	/* 1. Setup slot select register for this range. */
	switch (sbusslot) {
	case 1:
		sval = SBUS_SSEL_Y_SLOT1;
		break;
	case 2:
		sval = SBUS_SSEL_Y_SLOT2;
		break;
	case 3:
		sval = SBUS_SSEL_Y_SLOT3;
		break;
	case 4:
		sval = SBUS_SSEL_Y_SLOT4;
		break;
	case 5:
		sval = SBUS_SSEL_Y_SLOT5;
		break;
	default:
		return (-1);
	}

	if (range & 1) {
		regs->sbus_ssel[range >> 1] &= ~SBUS_SSEL_Y;
		regs->sbus_ssel[range >> 1] |= sval;
	} else {
		regs->sbus_ssel[range >> 1] &= ~SBUS_SSEL_X;
		regs->sbus_ssel[range >> 1] |= sval << 4;
	}

	/* 2. Setup and enable the VME master range. */
	rval = regs->vme_range[range];
	rval &= ~(VME_RANGE_VMAE | VME_RANGE_VMAT);
	rval |= (vmebase >> 13) & (VME_RANGE_VMAE | VME_RANGE_VMAT);
	rval &= ~VME_RANGE_VMRCC;
	rval |= ((sbusoffset << 4) | (vmelen << 3)) & VME_RANGE_VMRCC;
	rval &= ~VME_RANGE_DECEN;
	regs->vme_range[range] = rval;

	/* 3. Setup addr/data capabilities for the range. */
	regs->vme_master_cap[range] &=
	    ~(VME_MASTER_CAP_DATA | VME_MASTER_CAP_ADDR);
	regs->vme_master_cap[range] |= vmecap;

	(void)config_found(&sc->sc_dev, oca, fgaprint);

	return (0);
}

int
fga_hwintr1(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	u_int32_t bits = 0, stat;
	int r = 0, s;

	s = splhigh();
	stat = regs->intr_stat;
	splx(s);

	if ((stat & INTR_STAT_VMEIRQ1) == 0) {
		bits |= INTR_STAT_VMEIRQ1;
		r |= fga_intrvec(sc, regs->viack_emu1);
	}

	s = splhigh();
	regs->intr_stat &= ~bits;
	splx(s);

	return (r);
}

int
fga_hwintr2(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	int r = 0;

	if ((regs->intr_stat & INTR_STAT_VMEIRQ2) == 0)
		r |= fga_intrvec(sc, regs->viack_emu2);

	return (r);
}

int
fga_hwintr3(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	int r = 0;

	/* vme irq 3 */
	if ((regs->intr_stat & INTR_STAT_VMEIRQ3) == 0)
		r |= fga_intrvec(sc, regs->viack_emu3);

	return (r);
}

int
fga_hwintr4(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	int r = 0;

	if ((regs->intr_stat & INTR_STAT_VMEIRQ4) == 0)
		r |= fga_intrvec(sc, regs->viack_emu4);

	return (r);
}

int
fga_hwintr5(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	int r = 0;

	if ((regs->intr_stat & INTR_STAT_VMEIRQ5) == 0)
		r |= fga_intrvec(sc, regs->viack_emu5);

	return (r);
}

int
fga_hwintr6(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	int r = 0;

	if ((regs->intr_stat & INTR_STAT_VMEIRQ6) == 0)
		r |= fga_intrvec(sc, regs->viack_emu6);

	return (r);
}

int
fga_hwintr7(v)
	void *v;
{
	struct fga_softc *sc = v;
	struct fga_regs *regs = sc->sc_regs;
	int r = 0, s;
	u_int32_t bits = 0, stat;

	s = splhigh();
	stat = regs->intr_stat;
	splx(s);

#ifdef DDB
	if (sc->sc_abort && (stat & INTR_STAT_ABORT) == 0) {
		bits |= INTR_STAT_ABORT;
		r |= 1;
		Debugger();
	}
#endif

	if ((regs->intr_stat & INTR_STAT_VMEIRQ7) == 0) {
		bits |= INTR_STAT_VMEIRQ7;
		r |= fga_intrvec(sc, regs->viack_emu7);
	}

	s = splhigh();
	regs->intr_stat &= ~bits;
	splx(s);

	return (r);
}

/*
 * Handle a VME vectored interrupt.
 */
int
fga_intrvec(sc, vec)
	struct fga_softc *sc;
	u_int8_t vec;
{
	struct intrhand *ih;
	int r, s = 0;

	if (sc->sc_vmevec == NULL)
		return (0);

	for (ih = sc->sc_vmevec[vec]; ih; ih = ih->ih_next) {
		r = (*ih->ih_fun)(ih->ih_arg);
		if (r > 0) {
			ih->ih_count.ec_count++;
			return (r);
		}
		s |= r;
	}

	return (s);
}

/*
 * Establish a VME level/vector interrupt.
 */
int
fga_intr_establish(sc, vec, level, ih, name)
	struct fga_softc *sc;
	int vec, level;
	struct intrhand *ih;
	const char *name;
{
	struct intrhand *ihs;
	const u_int8_t level_to_sint[] = {
		IRQ_MAP_INT,
		IRQ_MAP_SINT1,
		IRQ_MAP_SINT2,
		IRQ_MAP_SINT3,
		IRQ_MAP_SINT4,
		IRQ_MAP_SINT5,
		IRQ_MAP_SINT6,
		IRQ_MAP_SINT7
	};
	const u_int8_t level_to_irqmap[] = {0xff, 6, 5, 4, 3, 2, 1, 0};

#ifdef DIAGNOSTIC
	if (level < 1 || level > 7)
		panic("fga_level");
#endif

	/* setup vector handler */
	if (sc->sc_vmevec == NULL) {
		sc->sc_vmevec = (struct intrhand **)malloc(256 *
		    sizeof(struct intrhand *), M_DEVBUF, M_NOWAIT);
		if (sc->sc_vmevec == NULL)
			panic("fga_addirq");
		bzero(sc->sc_vmevec, 256 * sizeof(struct intrhand *));
	}
	if (sc->sc_vmevec[vec] == NULL)
		sc->sc_vmevec[vec] = ih;
	else {
		ihs = sc->sc_vmevec[vec];
		while (ihs->ih_next)
			ihs = ihs->ih_next;
		ih->ih_next = ih;
	}

	ih->ih_vec = level;
	evcount_attach(&ih->ih_count, name, &ih->ih_vec, &evcount_intr);

	/* setup hardware handler */
	fga_hwintr_establish(sc, level_to_sint[level]);

	/* setup/enable vme -> sbus irq mapping */
	sc->sc_regs->virq_map[level_to_irqmap[level]] &= ~IRQ_MAP_INT_MASK;
	sc->sc_regs->virq_map[level_to_irqmap[level]] |= level_to_sint[level];
	sc->sc_regs->virq_map[level_to_irqmap[level]] &= ~IRQ_MAP_ENABLE;

	return (0);
}

/*
 * Establish a hardware interrupt, making sure we're not there already.
 */
int
fga_hwintr_establish(sc, sint)
	struct fga_softc *sc;
	u_int8_t sint;
{
	const int sint_to_pri[] = {0, 2, 3, 5, 6, 7, 8, 9};

	if (sc->sc_established & (1 << sint))
		return (0);

	switch (sint) {
	case 1:
		sc->sc_ih1.ih_fun = fga_hwintr1;
		sc->sc_ih1.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih1, -1, NULL);
		break;
	case 2:
		sc->sc_ih2.ih_fun = fga_hwintr2;
		sc->sc_ih2.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih2, -1, NULL);
		break;
	case 3:
		sc->sc_ih3.ih_fun = fga_hwintr3;
		sc->sc_ih3.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih3, -1, NULL);
		break;
	case 4:
		sc->sc_ih4.ih_fun = fga_hwintr4;
		sc->sc_ih4.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih4, -1, NULL);
		break;
	case 5:
		sc->sc_ih5.ih_fun = fga_hwintr5;
		sc->sc_ih5.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih5, -1, NULL);
		break;
	case 6:
		sc->sc_ih6.ih_fun = fga_hwintr6;
		sc->sc_ih6.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih6, -1, NULL);
		break;
	case 7:
		sc->sc_ih7.ih_fun = fga_hwintr7;
		sc->sc_ih7.ih_arg = sc;
		intr_establish(sint_to_pri[sint], &sc->sc_ih7, -1, NULL);
		break;
#ifdef DIAGNOSTIC
	default:
		panic("fga_sint");
#endif
	}

	sc->sc_established |= 1 << sint;
	return (0);
}

int
fgaopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct fga_softc *sc;

	if (fga_cd.cd_ndevs == 0)
		return (ENXIO);
	sc = fga_cd.cd_devs[0];
	if (sc == NULL)
		return (ENXIO);
	return (0);
}

int
fgaclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	return (0);
}

int
fgaioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct fga_softc *sc = fga_cd.cd_devs[0];
	struct fga_sem *fsem = (struct fga_sem *)data;
	int error = 0;

	switch (cmd) {
	case FGAIOCSEM:
		if (fsem->fgasem_num >= 48) {
			error = ENOENT;
			break;
		}
		sc->sc_regs->sem[fsem->fgasem_num] = 0xff;
		break;
	case FGAIOGSEM:
		if (fsem->fgasem_num >= 48) {
			error = ENOENT;
			break;
		}
		fsem->fgasem_val =
		    (sc->sc_regs->sem[fsem->fgasem_num] & MBOX_SEM) ? 0 : 1;
		break;
	case FGAIOCMBX:
		if (fsem->fgasem_num >= 16) {
			error = ENOENT;
			break;
		}
		sc->sc_regs->mbox[fsem->fgasem_num] = 0xff;
		break;
	case FGAIOGMBX:
		if (fsem->fgasem_num >= 16) {
			error = ENOENT;
			break;
		}
		fsem->fgasem_val =
		    sc->sc_regs->mbox[fsem->fgasem_num] ? 0 : 1;
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

int
fvmematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
        struct romaux *ra = &ca->ca_ra;

	if (strcmp("fvme", ra->ra_name) || ca->ca_bustype != BUS_FGA)
		return (0);

	return (1);
}

const struct fvme_types {
	int data_cap;
	int addr_cap;
	const char *name;
	int bustype;
} fvme_types[] = {
	{0, 0, "a16d8", BUS_FGA_A16D8},
	{0, 1, "a24d8", BUS_FGA_A24D8},
	{0, 2, "a32d8", BUS_FGA_A32D8},
	{1, 0, "a16d16", BUS_FGA_A16D16},
	{1, 1, "a24d16", BUS_FGA_A24D16},
	{1, 2, "a32d16", BUS_FGA_A32D16},
	{2, 0, "a16d32", BUS_FGA_A16D32},
	{2, 1, "a24d32", BUS_FGA_A24D32},
	{2, 2, "a32d32", BUS_FGA_A32D32},
	{3, 0, "a16blt", -1},
	{3, 1, "a24blt", -1},
	{3, 2, "a32blt", -1},
	{4, 0, "a16mblt", -1},
	{4, 1, "a24mblt", -1},
	{4, 2, "a32mblt", -1},
	{-1, -1, "", -1},
};

void    
fvmeattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	const struct fvme_types *p;
	int dtype, atype;

	atype = (ca->ca_ra.ra_reg[1].rr_iospace & VME_MASTER_CAP_ADDR) >> 2;
	dtype = (ca->ca_ra.ra_reg[1].rr_iospace & VME_MASTER_CAP_DATA) >> 5;
	for (p = fvme_types; p->data_cap != -1; p++) {
		if (p->data_cap == dtype && p->addr_cap == atype)
			break;
	}
	if (p->data_cap == -1) {
		printf(" unknown addr/data capability\n");
		return;
	}
	printf(": %s", p->name);
	if (p->bustype == -1) {
		printf(" unsupported\n");
		return;
	}
	ca->ca_ra.ra_reg[1].rr_iospace = p->bustype;

	printf(" offset 0x%x len 0x%x\n", ca->ca_ra.ra_reg[1].rr_paddr,
	   ca->ca_ra.ra_reg[1].rr_len);

	(void)config_search(fvmescan, self, aux);
}

int
fvmescan(parent, child, aux)
	struct device *parent;
	void *child, *aux;
{
	struct cfdata *cf = child;
	struct confargs *ca = aux, oca;
	int plen, paddr;

	if (cf->cf_loc[0] == -1)
		return (0);

	if ((unsigned)cf->cf_loc[0] < (unsigned)ca->ca_ra.ra_reg[1].rr_paddr)
		return (0);

	paddr = cf->cf_loc[0] - (int)ca->ca_ra.ra_reg[1].rr_paddr;
	paddr = (int)ca->ca_ra.ra_reg[0].rr_paddr + paddr;
	plen = cf->cf_loc[0] - (int)ca->ca_ra.ra_reg[1].rr_paddr;
	plen = ca->ca_ra.ra_reg[1].rr_len - plen;

	oca.ca_bustype = ca->ca_ra.ra_reg[1].rr_iospace;
	oca.ca_offset = cf->cf_loc[0];

	oca.ca_ra.ra_nintr = 1;
	oca.ca_ra.ra_intr[0].int_pri = cf->cf_loc[1];
	oca.ca_ra.ra_intr[0].int_vec = cf->cf_loc[2];
	oca.ca_ra.ra_name = cf->cf_driver->cd_name;

	oca.ca_ra.ra_nreg = 1;
	oca.ca_ra.ra_reg[0].rr_paddr = (void *)paddr;
	oca.ca_ra.ra_reg[0].rr_iospace = ca->ca_ra.ra_reg[0].rr_iospace;
	oca.ca_ra.ra_reg[0].rr_len = plen;
	oca.ca_ra.ra_reg[0].rr_paddr =
	    mapdev(&oca.ca_ra.ra_reg[0], TMPMAP_VA, 0, NBPG);

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0) {
		bus_untmp();
		return (0);
	}
	bus_untmp();

	oca.ca_ra.ra_reg[0].rr_paddr = (void *)paddr;
	config_attach(parent, cf, &oca, fvmeprint);
	return (1);
}

int
fvmeprint(args, name)
	void *args;
	const char *name;
{
	struct confargs *ca = args;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	printf(" addr 0x%x", ca->ca_offset);
	return (UNCONF);
}

int
fvmeintrestablish(dsc, vec, level, ih, name)
	struct device *dsc;
	int vec, level;
	struct intrhand *ih;
	const char *name;
{
	struct fga_softc *fsc = (struct fga_softc *)dsc->dv_parent;

	return (fga_intr_establish(fsc, vec, level, ih, name));
}
