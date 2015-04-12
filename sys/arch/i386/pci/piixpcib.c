/*	$OpenBSD: piixpcib.c,v 1.12 2015/04/12 18:37:54 mlarkin Exp $ */

/*
 * Copyright (c) 2007 Stefan Sperling <stsp@stsp.in-berlin.de>
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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
 *
 *-
 * Copyright (c) 2004, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Minoura Makoto, Matthew R. Green, and Jared D. McNeill.
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

/*
 * Special driver for the Intel PIIX4 bridges that attaches
 * instead of pcib(4). In addition to the core pcib(4) functionality this
 * driver provides support for Intel SpeedStep technology present in
 * some Pentium III CPUs.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/kvm86.h>

/* 0x47534943 == "ISGE" ('Intel Speedstep Gate E') */
#define	PIIXPCIB_ISGE			0x47534943
#define	PIIXPCIB_IST_CALL		0x0000e980
#define	PIIXPCIB_GSIC_CMD		0x82
#define	PIIXPCIB_DEFAULT_COMMAND	\
	((PIIXPCIB_ISGE & 0xffffff00) | (PIIXPCIB_GSIC_CMD & 0xff))

#define	PIIXPCIB_DEFAULT_SMI_PORT	0xb2
#define	PIIXPCIB_DEFAULT_SMI_DATA	0xb3

#define	PIIXPCIB_GETSTATE		1
#define	PIIXPCIB_SETSTATE		2
#define	PIIXPCIB_SPEEDSTEP_HIGH		0
#define	PIIXPCIB_SPEEDSTEP_LOW		1

struct piixpcib_softc {
	struct device sc_dev;

	int		sc_sig;
	int		sc_smi_port;
	int		sc_smi_data;
	int		sc_command;
	int		sc_flags;

	int		state;
};

int	piixpcib_match(struct device *, void *, void *);
void	piixpcib_attach(struct device *, struct device *, void *);

void	piixpcib_setperf(int);
int	piixpcib_cpuspeed(int *);

int	piixpcib_set_ownership(struct piixpcib_softc *);
int	piixpcib_configure_speedstep(struct piixpcib_softc *);
int	piixpcib_getset_state(struct piixpcib_softc *, int *, int);
void	piixpcib_int15_gsic_call(struct piixpcib_softc *);

/* arch/i386/pci/pcib.c */
extern void	pcibattach(struct device *, struct device *, void *);

/* arch/i386/i386/machdep.c */
#if !defined(SMALL_KERNEL)
extern void	p3_update_cpuspeed(void);
#endif

extern int cpu_pae;

struct cfattach piixpcib_ca = {
	sizeof(struct piixpcib_softc),
	piixpcib_match,
	piixpcib_attach
};

struct cfdriver piixpcib_cd = {
	NULL, "piixpcib", DV_DULL
};

struct piixpcib_softc *piixpcib_sc;
extern int setperf_prio;

const struct pci_matchid piixpcib_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82371AB_ISA}, /* PIIX4 */
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82440MX_PM},  /* PIIX4 in MX440 */
};

void
piixpcib_int15_gsic_call(struct piixpcib_softc *sc)
{
	struct kvm86regs regs;
	int cmd;

	memset(&regs, 0, sizeof(struct kvm86regs));
	regs.eax = PIIXPCIB_IST_CALL;
	regs.edx = PIIXPCIB_ISGE;
	kvm86_simplecall(0x15, &regs);

	if (regs.eax == PIIXPCIB_ISGE) {
		sc->sc_sig = regs.eax;
		sc->sc_smi_port = regs.ebx & 0xff;

		cmd = (regs.ebx >> 16) & 0xff;
		/* GSIC may return cmd 0x80, should be PIIXPCIB_GSIC_CMD */
		if (cmd == 0x80)
			cmd = PIIXPCIB_GSIC_CMD;
		sc->sc_command = (sc->sc_sig & 0xffffff00) | (cmd & 0xff);

		sc->sc_smi_data = regs.ecx;
		sc->sc_flags = regs.edx;
	}
}

int
piixpcib_set_ownership(struct piixpcib_softc *sc)
{
	int rv;
	paddr_t pmagic;
	char magic[] = "Copyright (c) 1999 Intel Corporation";

	pmap_extract(pmap_kernel(), (vaddr_t)magic, &pmagic);

	__asm volatile(
		"movl $0, %%edi\n\t"
		"out %%al, (%%dx)\n"
		: "=D" (rv)
		: "a" (sc->sc_command),
		  "b" (0),
		  "c" (0),
		  "d" (sc->sc_smi_port),
		  "S" (pmagic)
	);

	return (rv ? ENXIO : 0);
}

int
piixpcib_configure_speedstep(struct piixpcib_softc *sc)
{
	int rv;

	sc->sc_sig = -1;

	/* setup some defaults */
	sc->sc_smi_port = PIIXPCIB_DEFAULT_SMI_PORT;
	sc->sc_smi_data = PIIXPCIB_DEFAULT_SMI_DATA;
	sc->sc_command = PIIXPCIB_DEFAULT_COMMAND;
	sc->sc_flags = 0;

	if (cpu_pae)
		return ENODEV;

	piixpcib_int15_gsic_call(sc);

	/* If signature doesn't match, bail out */
	if (sc->sc_sig != PIIXPCIB_ISGE)
		return ENODEV;

	if (piixpcib_set_ownership(sc) != 0) {
		printf(": unable to claim ownership from BIOS, "
		    "SpeedStep disabled");
		return ENXIO;
	}

	rv = piixpcib_getset_state(sc, &sc->state, PIIXPCIB_GETSTATE);
	if (rv != 0) {
		printf(": cannot determine CPU power state, "
		    "SpeedStep disabled");
		return ENXIO;
	}

	/* save the sc for IO tag/handle */
	piixpcib_sc = sc;

	return 0;
}

int
piixpcib_match(struct device *parent, void *match, void *aux)
{
	if (pci_matchbyid((struct pci_attach_args *)aux, piixpcib_devices,
	    sizeof(piixpcib_devices) / sizeof(piixpcib_devices[0])))
		return (2);	/* supersede pcib(4) */
	return (0);
}

void
piixpcib_attach(struct device *parent, struct device *self, void *aux)
{
	struct piixpcib_softc *sc = (struct piixpcib_softc *)self;

	if (setperf_prio < 2) {
		/* Set up SpeedStep. */
		if (piixpcib_configure_speedstep(sc) == 0) {
			printf(": SpeedStep");

			/* Hook into hw.setperf sysctl */
			cpu_setperf = piixpcib_setperf;
			setperf_prio = 2;
		}
	}

	/* Provide core pcib(4) functionality */
	pcibattach(parent, self, aux);
}

int
piixpcib_getset_state(struct piixpcib_softc *sc, int *state, int function)
{
	int new_state;
	int rv;
	int eax;

#ifdef DIAGNOSTIC
	if (function != PIIXPCIB_GETSTATE &&
	    function != PIIXPCIB_SETSTATE) {
		printf("%s: %s called with invalid function %d\n",
		    sc->sc_dev.dv_xname, __func__, function);
		return EINVAL;
	}
#endif

	__asm volatile(
		"movl $0, %%edi\n\t"
		"out %%al, (%%dx)\n"
		: "=a" (eax),
		  "=b" (new_state),
		  "=D" (rv)
		: "a" (sc->sc_command),
		  "b" (function),
		  "c" (*state),
		  "d" (sc->sc_smi_port),
		  "S" (0)
	);

	*state = new_state & 1;

	switch (function) {
	case PIIXPCIB_GETSTATE:
		if (eax)
			return ENXIO;
		break;
	case PIIXPCIB_SETSTATE:
		if (rv)
			return ENXIO;
		break;
	}

	return 0;
}

void
piixpcib_setperf(int level)
{
	struct piixpcib_softc *sc;
	int new_state;
	int tries, rv, s;

 	sc = piixpcib_sc;

#ifdef DIAGNOSTIC
	if (sc == NULL) {
		printf("%s: no cookie", __func__);
		return;
	}
#endif

	/* Only two states are available */
	if (level <= 50)
		new_state = PIIXPCIB_SPEEDSTEP_LOW;
	else
		new_state = PIIXPCIB_SPEEDSTEP_HIGH;

	if (sc->state == new_state)
		return;

	tries = 5;
	s = splhigh();

	do {
		rv = piixpcib_getset_state(sc, &new_state,
		    PIIXPCIB_SETSTATE);
		if (rv)
			delay(200);
	} while (rv && --tries);

	splx(s);

#ifdef DIAGNOSTIC
	if (rv)
		printf("%s: setting CPU power state failed",
		    sc->sc_dev.dv_xname);
#endif

	sc->state = new_state;

	/* Force update of hw.cpuspeed.
	 *
	 * XXX: First generation SpeedStep is only present in some
	 * Pentium III CPUs and we are lacking a reliable method to
	 * determine CPU freqs corresponding to low and high power state.
	 *
	 * And yes, I've tried it the way the Linux speedstep-smi
	 * driver does it, thank you very much. It doesn't work
	 * half the time (my machine has more than 4MHz ;-) and
	 * even crashes some machines without specific workarounds.
	 *
	 * So call p3_update_cpuspeed() from arch/i386/i386/machdep.c
	 * instead. Works fine on my Thinkpad X21.
	 *
	 * BUT: Apparently, if the bus is busy, the transition may be
	 * delayed and retried under control of evil SMIs we cannot
	 * control. So busy-wait a short while before updating hw.cpuspeed
	 * to decrease chances of picking up the old CPU speed.
	 * There seems to be no reliable fix for this.
	 */
	delay(200);
#if !defined(SMALL_KERNEL)
	p3_update_cpuspeed();
#endif
}
