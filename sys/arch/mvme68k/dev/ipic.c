/*	$OpenBSD: ipic.c,v 1.16 2005/11/24 22:43:16 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/ipicreg.h>
#include <mvme68k/dev/mcreg.h>

void	ipicattach(struct device *, struct device *, void *);
int	ipicmatch(struct device *, void *, void *);

int	ipicprint(void *, const char *);
int	ipicscan(struct device *, void *, void *);
caddr_t	ipicmap(struct ipicsoftc *, caddr_t, int);
void	ipicunmap(struct ipicsoftc *, caddr_t, int);

struct cfattach ipic_ca = {
	sizeof(struct ipicsoftc), ipicmatch, ipicattach
};

struct cfdriver ipic_cd = {
	NULL, "ipic", DV_DULL
};

int
ipicmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	struct confargs *ca = args;
	struct ipicreg *ipic = (struct ipicreg *)ca->ca_vaddr;

	if (badvaddr((vaddr_t)ipic, 1) || ipic->ipic_chipid != IPIC_CHIPID)
		return (0);
	return (1);
}

int
ipicprint(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	printf(" slot %d", ca->ca_offset);
	if (ca->ca_vec > 0)
		printf(" vec %d", ca->ca_vec);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
ipicscan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct ipicsoftc *sc = (struct ipicsoftc *)parent;
	struct confargs oca;
	int slot, n = 0;
	vaddr_t ipv;
	paddr_t ipp;
	struct ipid *ipid;

	/* XXX can we determine IPIC_IPSPACE automatically? */
	for (slot = 0; slot < sc->sc_nip; slot++) {
		ipp = sc->sc_ipspace + (slot * IPIC_IP_MODSIZE);
		if (badpaddr((paddr_t)ipp + IPIC_IP_IDOFFSET, 2))
			continue;

		ipv = mapiodev(ipp, NBPG);
		if (ipv == 0)
			continue;

		ipid = (struct ipid *)(ipv + IPIC_IP_IDOFFSET);
		if (ipid->ipid_A != 'A' || ipid->ipid_P != 'P' ||
		    ipid->ipid_I != 'I' || ipid->ipid_C != 'C' ||
		    (u_char)cf->cf_loc[0] != ipid->ipid_manu ||
		    (u_char)cf->cf_loc[1] != ipid->ipid_prod) {
			unmapiodev(ipv, NBPG);
			continue;
		}

		bzero(&oca, sizeof oca);
		oca.ca_bustype = BUS_IP;
		oca.ca_offset = slot;		/* slot number */
		oca.ca_paddr = ipp;
		oca.ca_vaddr = ipv;
		oca.ca_len = NBPG;
		oca.ca_ipl = cf->cf_loc[2];
		oca.ca_vec = cf->cf_loc[3];
		if (oca.ca_ipl > 0 && oca.ca_vec == -1)
			oca.ca_vec = intr_findvec(255, 0);

		oca.ca_name = cf->cf_driver->cd_name;

		if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0) {
			unmapiodev(ipv, NBPG);
			continue;
		}
		config_attach(parent, cf, &oca, ipicprint);
		n++;
	}
	return (n);
}

void
ipicattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct ipicsoftc *sc = (struct ipicsoftc *)self;
	struct confargs *ca = args;

	sc->sc_ipic = (struct ipicreg *)ca->ca_vaddr;
	sc->sc_ipspace = IPIC_IPSPACE;
	sc->sc_nip = 2;

	/* 
	 * Bug in IP2 chip. ipic_chiprev should be 0x01 if 
	 * the MC chip is rev 1. XXX - smurph
	 */
	if (sys_mc->mc_chiprev == 0x01) 
		printf(": rev 1\n");
	else
		printf(": rev %d\n", sc->sc_ipic->ipic_chiprev);

	sc->sc_ipic->ipic_reset = IPIC_RESET;
	delay(2);

	config_search(ipicscan, self, args);
}

caddr_t
ipicmap(sc, addr, len)
	struct ipicsoftc *sc;
	caddr_t addr;
	int len;
{
	return (NULL);
}

void
ipicunmap(sc, addr, len)
	struct ipicsoftc *sc;
	caddr_t addr;
	int len;
{
}

int
ipicintr_establish(vec, ih, name)
	int vec;
	struct intrhand *ih;
	const char *name;
{
	return intr_establish(vec, ih, name);
}
