/*	$OpenBSD: if_lmc_obsd.c,v 1.5 2000/02/01 18:01:41 chris Exp $ */
/*	$NetBSD: if_lmc_nbsd.c,v 1.1 1999/03/25 03:32:43 explorer Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#if defined(__FreeBSD__)
#include <machine/clock.h>
#elif defined(__bsdi__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#endif

#if defined(__NetBSD__)
#include <dev/pci/pcidevs.h>
#include "rnd.h"
#if NRND > 0
#include <sys/rnd.h>
#endif
#endif

#if defined(__OpenBSD__)
#include <dev/pci/pcidevs.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <net/if_sppp.h>
#endif

#if defined(__bsdi__)
#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net/netisr.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/if_p2p.h>
#include <net/if_c_hdlc.h>
#endif

#if defined(__FreeBSD__)
#include <vm/pmap.h>
#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/dc21040reg.h>
#define INCLUDE_PATH_PREFIX "pci/"
#endif
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#include <i386/pci/ic/dc21040.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#include <i386/pci/pci.h>

#define	INCLUDE_PATH_PREFIX	"i386/pci/"
#endif /* __bsdi__ */

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <machine/bus.h>
#if defined(__alpha__) && defined(__NetBSD__)
#include <machine/intr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#define	INCLUDE_PATH_PREFIX	"dev/pci/"
#endif /* __NetBSD__ */

/*
 * Sigh.  Every OS puts these in different places.  NetBSD and FreeBSD use
 * a C preprocessor that allows this hack, but BSDI does not.  Grr.
 */
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include INCLUDE_PATH_PREFIX "if_lmc_types.h"
#include INCLUDE_PATH_PREFIX "if_lmcioctl.h"
#include INCLUDE_PATH_PREFIX "if_lmcvar.h"
#else /* BSDI */
#include "i386/pci/if_lmctypes.h"
#include "i386/pci/if_lmcioctl.h"
#include "i386/pci/if_lmcvar.h"
#endif

/*
 * This file is INCLUDED (gross, I know, but...)
 */

static void lmc_shutdown(void *arg);

static int
lmc_pci_probe(struct device *parent,
#if defined (__BROKEN_INDIRECT_CONFIG) || defined(__OpenBSD__)
	       void *match,
#else
	       struct cfdata *match,
#endif
	       void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	u_int32_t id;

	/*
	 * check first for the DEC chip we expect to find.  We expect
	 * 21140A, pass 2.2 or higher.
	 */
	if (PCI_VENDORID(pa->pa_id) != PCI_VENDOR_DEC)
		return 0;
	if (PCI_CHIPID(pa->pa_id) != PCI_PRODUCT_DEC_21140)
		return 0;
	id = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFRV) & 0xff;
	if (id < 0x22)
		return 0;

	/*
	 * Next, check the subsystem ID and see if it matches what we
	 * expect.
	 */
	id = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SSID);
	if (PCI_VENDORID(id) != PCI_VENDOR_LMC)
		return 0;
	if ((PCI_CHIPID(id) != PCI_PRODUCT_LMC_HSSI)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_DS3)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_SSI)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_DS1))
		return 0;

	return 10; /* must be > than any other tulip driver */
}

static void  lmc_pci_attach(struct device * const parent,
			     struct device * const self, void * const aux);

struct cfattach lmc_ca = {
    sizeof(lmc_softc_t), lmc_pci_probe, lmc_pci_attach
};

struct cfdriver lmc_cd = {
	0, "lmc", DV_IFNET
};

static void
lmc_pci_attach(struct device * const parent,
		struct device * const self, void * const aux)
{
	u_int32_t revinfo, cfdainfo, id, ssid;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
#if 0
	vm_offset_t pa_csrs;
#endif
	unsigned csroffset = LMC_PCI_CSROFFSET;
	unsigned csrsize = LMC_PCI_CSRSIZE;
	lmc_csrptr_t csr_base;
	lmc_spl_t s;
	lmc_intrfunc_t (*intr_rtn)(void *) = lmc_intr_normal;
	lmc_softc_t * const sc = (lmc_softc_t *) self;
	struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
	extern lmc_media_t lmc_hssi_media;
	extern lmc_media_t lmc_ds3_media;
	extern lmc_media_t lmc_t1_media;
	extern lmc_media_t lmc_ssi_media;

	revinfo  = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFRV) & 0xFF;
	id       = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFID);
	cfdainfo = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFDA);
	ssid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SSID);

	switch (PCI_CHIPID(ssid)) {
	case PCI_PRODUCT_LMC_HSSI:
		printf(": Lan Media Corporation HSSI\n");
		sc->lmc_media = &lmc_hssi_media;
		break;
	case PCI_PRODUCT_LMC_DS3:
		printf(": Lan Media Corporation DS3\n");
		sc->lmc_media = &lmc_ds3_media;
		break;
	case PCI_PRODUCT_LMC_SSI:
		printf(": Lan Media Corporation SSI\n");
		sc->lmc_media = &lmc_ssi_media;
		break;
	case PCI_PRODUCT_LMC_DS1:
		printf(": Lan Media Corporation T1\n");
		sc->lmc_media = &lmc_t1_media;
		break;
	}

        sc->lmc_pci_busno = parent;
        sc->lmc_pci_devno = pa->pa_device;

	sc->lmc_chipid = LMC_21140A;
	sc->lmc_features |= LMC_HAVE_STOREFWD;
	if (sc->lmc_chipid == LMC_21140A && revinfo <= 0x22)
		sc->lmc_features |= LMC_HAVE_RXBADOVRFLW;

	if (cfdainfo & (TULIP_CFDA_SLEEP | TULIP_CFDA_SNOOZE)) {
		cfdainfo &= ~(TULIP_CFDA_SLEEP | TULIP_CFDA_SNOOZE);
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_CFDA, cfdainfo);
		DELAY(11 * 1000);
	}

	bcopy(self->dv_xname, sc->lmc_if.if_xname, IFNAMSIZ);
	sc->lmc_if.if_softc = sc;
	sc->lmc_pc = pa->pa_pc;

	sc->lmc_revinfo = revinfo;
	sc->lmc_if.if_softc = sc;

	csr_base = 0;
	{
		bus_space_tag_t iot, memt;
		bus_space_handle_t ioh, memh;
		int ioh_valid, memh_valid;

#if defined(__NetBSD__) || defined(__OpenBSD__)

		ioh_valid = (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO,
					    0, &iot, &ioh, NULL, NULL) == 0);
		memh_valid = (pci_mapreg_map(pa, PCI_CBMA,
					     PCI_MAPREG_TYPE_MEM |
					     PCI_MAPREG_MEM_TYPE_32BIT,
					     0, &memt, &memh, NULL,
					     NULL) == 0);
#endif


		if (memh_valid) {
			sc->lmc_bustag = memt;
			sc->lmc_bushandle = memh;
		} else if (ioh_valid) {
			sc->lmc_bustag = iot;
			sc->lmc_bushandle = ioh;
		} else {
			printf("%s: unable to map device registers\n",
			       sc->lmc_dev.dv_xname);
			return;
		}
		/* Make sure bus mastering is enabled. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
			       pci_conf_read(pa->pa_pc, pa->pa_tag,
					     PCI_COMMAND_STATUS_REG) |
			       PCI_COMMAND_MASTER_ENABLE);
	}

	lmc_initcsrs(sc, csr_base + csroffset, csrsize);
	lmc_initring(sc, &sc->lmc_rxinfo, sc->lmc_rxdescs,
		       LMC_RXDESCS);
	lmc_initring(sc, &sc->lmc_txinfo, sc->lmc_txdescs,
		       LMC_TXDESCS);

	lmc_gpio_mkinput(sc, 0xff);
	sc->lmc_gpio = 0;  /* drive no signals yet */

	sc->lmc_media->defaults(sc);

	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN); /* down */

	/*
	 * Make sure there won't be any interrupts or such...
	 */
	LMC_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);

	/*
	 * Wait 10 microseconds (actually 50 PCI cycles but at 
	 * 33MHz that comes to two microseconds but wait a
	 * bit longer anyways)
	 */
	DELAY(100);

	lmc_read_macaddr(sc);

        if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
                         pa->pa_intrline, &intrhandle)) {
		 printf("%s: couldn't map interrupt\n",
			sc->lmc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle);

#if defined(__OpenBSD__)
	sc->lmc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
						intr_rtn, sc, self->dv_xname);
#else
	sc->lmc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
						intr_rtn, sc);
#endif

	if (sc->lmc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		       sc->lmc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

        printf("%s: pass %d.%d, serial " LMC_EADDR_FMT ", %s\n",
               sc->lmc_dev.dv_xname,
               (sc->lmc_revinfo & 0xF0) >> 4, sc->lmc_revinfo & 0x0F,
               LMC_EADDR_ARGS(sc->lmc_enaddr), intrstr);

        sc->lmc_ats = shutdownhook_establish(lmc_shutdown, sc);
        if (sc->lmc_ats == NULL)
		printf("%s: warning: couldn't establish shutdown hook\n",
		       sc->lmc_xname);

	s = LMC_RAISESPL();
	lmc_dec_reset(sc);
	lmc_reset(sc);
	lmc_attach(sc);
	LMC_RESTORESPL(s);
}

static void
lmc_shutdown(void *arg)
{
	lmc_softc_t * const sc = arg;
	LMC_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	DELAY(10);

	sc->lmc_miireg16 = 0;  /* deassert ready, and all others too */
	lmc_led_on(sc, LMC_MII16_LED_ALL);
}
