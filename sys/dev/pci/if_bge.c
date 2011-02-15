/*	$OpenBSD: if_bge.c,v 1.304 2011/02/15 19:49:47 robert Exp $	*/

/*
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2001
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: if_bge.c,v 1.25 2002/11/14 23:54:49 sam Exp $
 */

/*
 * Broadcom BCM57xx/BCM590x family ethernet driver for OpenBSD.
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Engineer, Wind River Systems
 */

/*
 * The Broadcom BCM5700 is based on technology originally developed by
 * Alteon Networks as part of the Tigon I and Tigon II gigabit ethernet
 * MAC chips. The BCM5700, sometimes refered to as the Tigon III, has
 * two on-board MIPS R4000 CPUs and can have as much as 16MB of external
 * SSRAM. The BCM5700 supports TCP, UDP and IP checksum offload, Jumbo
 * frames, highly configurable RX filtering, and 16 RX and TX queues
 * (which, along with RX filter rules, can be used for QOS applications).
 * Other features, such as TCP segmentation, may be available as part
 * of value-added firmware updates. Unlike the Tigon I and Tigon II,
 * firmware images can be stored in hardware and need not be compiled
 * into the driver.
 *
 * The BCM5700 supports the PCI v2.2 and PCI-X v1.0 standards, and will
 * function in a 32-bit/64-bit 33/66MHz bus, or a 64-bit/133MHz bus.
 *
 * The BCM5701 is a single-chip solution incorporating both the BCM5700
 * MAC and a BCM5401 10/100/1000 PHY. Unlike the BCM5700, the BCM5701
 * does not support external SSRAM.
 *
 * Broadcom also produces a variation of the BCM5700 under the "Altima"
 * brand name, which is functionally similar but lacks PCI-X support.
 *
 * Without external SSRAM, you can only have at most 4 TX rings,
 * and the use of the mini RX ring is disabled. This seems to imply
 * that these features are simply not available on the BCM5701. As a
 * result, this driver does not implement any support for the mini RX
 * ring.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef __sparc64__
#include <sparc64/autoconf.h>
#include <dev/ofw/openfirm.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>
#include <dev/mii/brgphyreg.h>

#include <dev/pci/if_bgereg.h>

#define ETHER_MIN_NOPAD		(ETHER_MIN_LEN - ETHER_CRC_LEN) /* i.e., 60 */

const struct bge_revision * bge_lookup_rev(u_int32_t);
int bge_probe(struct device *, void *, void *);
void bge_attach(struct device *, struct device *, void *);
int bge_activate(struct device *, int);

struct cfattach bge_ca = {
	sizeof(struct bge_softc), bge_probe, bge_attach, NULL, bge_activate
};

struct cfdriver bge_cd = {
	NULL, "bge", DV_IFNET
};

void bge_txeof(struct bge_softc *);
void bge_rxeof(struct bge_softc *);

void bge_tick(void *);
void bge_stats_update(struct bge_softc *);
void bge_stats_update_regs(struct bge_softc *);
int bge_cksum_pad(struct mbuf *);
int bge_encap(struct bge_softc *, struct mbuf *, u_int32_t *);
int bge_compact_dma_runt(struct mbuf *);

int bge_intr(void *);
void bge_start(struct ifnet *);
int bge_ioctl(struct ifnet *, u_long, caddr_t);
void bge_init(void *);
void bge_stop_block(struct bge_softc *, bus_size_t, u_int32_t);
void bge_stop(struct bge_softc *);
void bge_watchdog(struct ifnet *);
int bge_ifmedia_upd(struct ifnet *);
void bge_ifmedia_sts(struct ifnet *, struct ifmediareq *);

u_int8_t bge_nvram_getbyte(struct bge_softc *, int, u_int8_t *);
int bge_read_nvram(struct bge_softc *, caddr_t, int, int);
u_int8_t bge_eeprom_getbyte(struct bge_softc *, int, u_int8_t *);
int bge_read_eeprom(struct bge_softc *, caddr_t, int, int);

void bge_iff(struct bge_softc *);

int bge_newbuf_jumbo(struct bge_softc *, int);
int bge_init_rx_ring_jumbo(struct bge_softc *);
void bge_fill_rx_ring_jumbo(struct bge_softc *);
void bge_free_rx_ring_jumbo(struct bge_softc *);

int bge_newbuf(struct bge_softc *, int);
int bge_init_rx_ring_std(struct bge_softc *);
void bge_rxtick(void *);
void bge_fill_rx_ring_std(struct bge_softc *);
void bge_free_rx_ring_std(struct bge_softc *);

void bge_free_tx_ring(struct bge_softc *);
int bge_init_tx_ring(struct bge_softc *);

void bge_chipinit(struct bge_softc *);
int bge_blockinit(struct bge_softc *);

u_int32_t bge_readmem_ind(struct bge_softc *, int);
void bge_writemem_ind(struct bge_softc *, int, int);
void bge_writereg_ind(struct bge_softc *, int, int);
void bge_writembx(struct bge_softc *, int, int);

int bge_miibus_readreg(struct device *, int, int);
void bge_miibus_writereg(struct device *, int, int, int);
void bge_miibus_statchg(struct device *);

void bge_reset(struct bge_softc *);
void bge_link_upd(struct bge_softc *);

#ifdef BGE_DEBUG
#define DPRINTF(x)	do { if (bgedebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (bgedebug >= (n)) printf x; } while (0)
int	bgedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/types and their names. Note: the
 * spec seems to indicate that the hardware still has Alteon's vendor
 * ID burned into it, though it will always be overridden by the vendor
 * ID in the EEPROM. Just to be safe, we cover all possibilities.
 */
const struct pci_matchid bge_devices[] = {
	{ PCI_VENDOR_ALTEON, PCI_PRODUCT_ALTEON_BCM5700 },
	{ PCI_VENDOR_ALTEON, PCI_PRODUCT_ALTEON_BCM5701 },

	{ PCI_VENDOR_ALTIMA, PCI_PRODUCT_ALTIMA_AC1000 },
	{ PCI_VENDOR_ALTIMA, PCI_PRODUCT_ALTIMA_AC1001 },
	{ PCI_VENDOR_ALTIMA, PCI_PRODUCT_ALTIMA_AC1003 },
	{ PCI_VENDOR_ALTIMA, PCI_PRODUCT_ALTIMA_AC9100 },

	{ PCI_VENDOR_APPLE, PCI_PRODUCT_APPLE_BCM5701 },

	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5700 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5701 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5702 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5702_ALT },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5702X },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5703 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5703_ALT },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5703X },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5704C },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5704S },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5704S_ALT },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5705 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5705F },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5705K },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5705M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5705M_ALT },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5714 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5714S },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5715 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5715S },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5717 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5718 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5720 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5721 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5722 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5723 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5724 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5750 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5750M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5751 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5751F },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5751M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5752 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5752M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5753 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5753F },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5753M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5754 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5754M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5755 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5755M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5756 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5761 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5761E },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5761S },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5761SE },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5764 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5780 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5780S },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5781 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5782 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5784 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5785F },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5785G },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5786 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5787 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5787F },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5787M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5788 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5789 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5901 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5901A2 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5903M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5906 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5906M },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57760 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57761 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57765 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57780 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57781 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57785 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57788 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57790 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57791 },
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM57795 },

	{ PCI_VENDOR_FUJITSU, PCI_PRODUCT_FUJITSU_PW008GE4 },
	{ PCI_VENDOR_FUJITSU, PCI_PRODUCT_FUJITSU_PW008GE5 },
	{ PCI_VENDOR_FUJITSU, PCI_PRODUCT_FUJITSU_PP250_450_LAN },

	{ PCI_VENDOR_SCHNEIDERKOCH, PCI_PRODUCT_SCHNEIDERKOCH_SK9D21 },

	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C996 }
};

#define BGE_IS_5705_PLUS(sc)		((sc)->bge_flags & BGE_5705_PLUS)
#define BGE_IS_5750_PLUS(sc)		((sc)->bge_flags & BGE_5750_PLUS)
#define BGE_IS_5755_PLUS(sc)		((sc)->bge_flags & BGE_5755_PLUS)
#define BGE_IS_5700_FAMILY(sc)		((sc)->bge_flags & BGE_5700_FAMILY)
#define BGE_IS_5714_FAMILY(sc)		((sc)->bge_flags & BGE_5714_FAMILY)
#define BGE_IS_JUMBO_CAPABLE(sc)	((sc)->bge_flags & BGE_JUMBO_CAPABLE)

static const struct bge_revision {
	u_int32_t		br_chipid;
	const char		*br_name;
} bge_revisions[] = {
	{ BGE_CHIPID_BCM5700_A0, "BCM5700 A0" },
	{ BGE_CHIPID_BCM5700_A1, "BCM5700 A1" },
	{ BGE_CHIPID_BCM5700_B0, "BCM5700 B0" },
	{ BGE_CHIPID_BCM5700_B1, "BCM5700 B1" },
	{ BGE_CHIPID_BCM5700_B2, "BCM5700 B2" },
	{ BGE_CHIPID_BCM5700_B3, "BCM5700 B3" },
	{ BGE_CHIPID_BCM5700_ALTIMA, "BCM5700 Altima" },
	{ BGE_CHIPID_BCM5700_C0, "BCM5700 C0" },
	{ BGE_CHIPID_BCM5701_A0, "BCM5701 A0" },
	{ BGE_CHIPID_BCM5701_B0, "BCM5701 B0" },
	{ BGE_CHIPID_BCM5701_B2, "BCM5701 B2" },
	{ BGE_CHIPID_BCM5701_B5, "BCM5701 B5" },
	/* the 5702 and 5703 share the same ASIC ID */
	{ BGE_CHIPID_BCM5703_A0, "BCM5702/5703 A0" },
	{ BGE_CHIPID_BCM5703_A1, "BCM5702/5703 A1" },
	{ BGE_CHIPID_BCM5703_A2, "BCM5702/5703 A2" },
	{ BGE_CHIPID_BCM5703_A3, "BCM5702/5703 A3" },
	{ BGE_CHIPID_BCM5703_B0, "BCM5702/5703 B0" },
	{ BGE_CHIPID_BCM5704_A0, "BCM5704 A0" },
	{ BGE_CHIPID_BCM5704_A1, "BCM5704 A1" },
	{ BGE_CHIPID_BCM5704_A2, "BCM5704 A2" },
	{ BGE_CHIPID_BCM5704_A3, "BCM5704 A3" },
	{ BGE_CHIPID_BCM5704_B0, "BCM5704 B0" },
	{ BGE_CHIPID_BCM5705_A0, "BCM5705 A0" },
	{ BGE_CHIPID_BCM5705_A1, "BCM5705 A1" },
	{ BGE_CHIPID_BCM5705_A2, "BCM5705 A2" },
	{ BGE_CHIPID_BCM5705_A3, "BCM5705 A3" },
	{ BGE_CHIPID_BCM5750_A0, "BCM5750 A0" },
	{ BGE_CHIPID_BCM5750_A1, "BCM5750 A1" },
	{ BGE_CHIPID_BCM5750_A3, "BCM5750 A3" },
	{ BGE_CHIPID_BCM5750_B0, "BCM5750 B0" },
	{ BGE_CHIPID_BCM5750_B1, "BCM5750 B1" },
	{ BGE_CHIPID_BCM5750_C0, "BCM5750 C0" },
	{ BGE_CHIPID_BCM5750_C1, "BCM5750 C1" },
	{ BGE_CHIPID_BCM5750_C2, "BCM5750 C2" },
	{ BGE_CHIPID_BCM5714_A0, "BCM5714 A0" },
	{ BGE_CHIPID_BCM5752_A0, "BCM5752 A0" },
	{ BGE_CHIPID_BCM5752_A1, "BCM5752 A1" },
	{ BGE_CHIPID_BCM5752_A2, "BCM5752 A2" },
	{ BGE_CHIPID_BCM5714_B0, "BCM5714 B0" },
	{ BGE_CHIPID_BCM5714_B3, "BCM5714 B3" },
	{ BGE_CHIPID_BCM5715_A0, "BCM5715 A0" },
	{ BGE_CHIPID_BCM5715_A1, "BCM5715 A1" },
	{ BGE_CHIPID_BCM5715_A3, "BCM5715 A3" },
	{ BGE_CHIPID_BCM5755_A0, "BCM5755 A0" },
	{ BGE_CHIPID_BCM5755_A1, "BCM5755 A1" },
	{ BGE_CHIPID_BCM5755_A2, "BCM5755 A2" },
	{ BGE_CHIPID_BCM5755_C0, "BCM5755 C0" },
	{ BGE_CHIPID_BCM5761_A0, "BCM5761 A0" },
	{ BGE_CHIPID_BCM5761_A1, "BCM5761 A1" },
	{ BGE_CHIPID_BCM5784_A0, "BCM5784 A0" },
	{ BGE_CHIPID_BCM5784_A1, "BCM5784 A1" },
	/* the 5754 and 5787 share the same ASIC ID */
	{ BGE_CHIPID_BCM5787_A0, "BCM5754/5787 A0" },
	{ BGE_CHIPID_BCM5787_A1, "BCM5754/5787 A1" },
	{ BGE_CHIPID_BCM5787_A2, "BCM5754/5787 A2" },
	{ BGE_CHIPID_BCM5906_A1, "BCM5906 A1" },
	{ BGE_CHIPID_BCM5906_A2, "BCM5906 A2" },
	{ BGE_CHIPID_BCM57780_A0, "BCM57780 A0" },
	{ BGE_CHIPID_BCM57780_A1, "BCM57780 A1" },

	{ 0, NULL }
};

/*
 * Some defaults for major revisions, so that newer steppings
 * that we don't know about have a shot at working.
 */
static const struct bge_revision bge_majorrevs[] = {
	{ BGE_ASICREV_BCM5700, "unknown BCM5700" },
	{ BGE_ASICREV_BCM5701, "unknown BCM5701" },
	/* 5702 and 5703 share the same ASIC ID */
	{ BGE_ASICREV_BCM5703, "unknown BCM5703" },
	{ BGE_ASICREV_BCM5704, "unknown BCM5704" },
	{ BGE_ASICREV_BCM5705, "unknown BCM5705" },
	{ BGE_ASICREV_BCM5750, "unknown BCM5750" },
	{ BGE_ASICREV_BCM5714_A0, "unknown BCM5714" },
	{ BGE_ASICREV_BCM5752, "unknown BCM5752" },
	{ BGE_ASICREV_BCM5780, "unknown BCM5780" },
	{ BGE_ASICREV_BCM5714, "unknown BCM5714" },
	{ BGE_ASICREV_BCM5755, "unknown BCM5755" },
	{ BGE_ASICREV_BCM5761, "unknown BCM5761" },
	{ BGE_ASICREV_BCM5784, "unknown BCM5784" },
	{ BGE_ASICREV_BCM5785, "unknown BCM5785" },
	/* 5754 and 5787 share the same ASIC ID */
	{ BGE_ASICREV_BCM5787, "unknown BCM5754/5787" },
	{ BGE_ASICREV_BCM5906, "unknown BCM5906" },
	{ BGE_ASICREV_BCM57780, "unknown BCM57780" },
	{ BGE_ASICREV_BCM5717, "unknown BCM5717" },
	{ BGE_ASICREV_BCM57765, "unknown BCM57765" },

	{ 0, NULL }
};

u_int32_t
bge_readmem_ind(struct bge_softc *sc, int off)
{
	struct pci_attach_args	*pa = &(sc->bge_pa);

	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_MEMWIN_BASEADDR, off);
	return (pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_MEMWIN_DATA));
}

void
bge_writemem_ind(struct bge_softc *sc, int off, int val)
{
	struct pci_attach_args	*pa = &(sc->bge_pa);

	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_MEMWIN_BASEADDR, off);
	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_MEMWIN_DATA, val);
}

void
bge_writereg_ind(struct bge_softc *sc, int off, int val)
{
	struct pci_attach_args	*pa = &(sc->bge_pa);

	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_REG_BASEADDR, off);
	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_REG_DATA, val);
}

void
bge_writembx(struct bge_softc *sc, int off, int val)
{
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		off += BGE_LPMBX_IRQ0_HI - BGE_MBX_IRQ0_HI;

	CSR_WRITE_4(sc, off, val);
}

u_int8_t
bge_nvram_getbyte(struct bge_softc *sc, int addr, u_int8_t *dest)
{
	u_int32_t access, byte = 0;
	int i;

	/* Lock. */
	CSR_WRITE_4(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_SET1);
	for (i = 0; i < 8000; i++) {
		if (CSR_READ_4(sc, BGE_NVRAM_SWARB) & BGE_NVRAMSWARB_GNT1)
			break;
		DELAY(20);
	}
	if (i == 8000)
		return (1);

	/* Enable access. */
	access = CSR_READ_4(sc, BGE_NVRAM_ACCESS);
	CSR_WRITE_4(sc, BGE_NVRAM_ACCESS, access | BGE_NVRAMACC_ENABLE);

	CSR_WRITE_4(sc, BGE_NVRAM_ADDR, addr & 0xfffffffc);
	CSR_WRITE_4(sc, BGE_NVRAM_CMD, BGE_NVRAM_READCMD);
	for (i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_NVRAM_CMD) & BGE_NVRAMCMD_DONE) {
			DELAY(10);
			break;
		}
	}

	if (i == BGE_TIMEOUT * 10) {
		printf("%s: nvram read timed out\n", sc->bge_dev.dv_xname);
		return (1);
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_NVRAM_RDDATA);

	*dest = (swap32(byte) >> ((addr % 4) * 8)) & 0xFF;

	/* Disable access. */
	CSR_WRITE_4(sc, BGE_NVRAM_ACCESS, access);

	/* Unlock. */
	CSR_WRITE_4(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_CLR1);
	CSR_READ_4(sc, BGE_NVRAM_SWARB);

	return (0);
}

/*
 * Read a sequence of bytes from NVRAM.
 */

int
bge_read_nvram(struct bge_softc *sc, caddr_t dest, int off, int cnt)
{
	int err = 0, i;
	u_int8_t byte = 0;

	if (BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5906)
		return (1);

	for (i = 0; i < cnt; i++) {
		err = bge_nvram_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return (err ? 1 : 0);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.' The
 * BCM570x supports both the traditional bitbang interface and an
 * auto access interface for reading the EEPROM. We use the auto
 * access method.
 */
u_int8_t
bge_eeprom_getbyte(struct bge_softc *sc, int addr, u_int8_t *dest)
{
	int i;
	u_int32_t byte = 0;

	/*
	 * Enable use of auto EEPROM access so we can avoid
	 * having to use the bitbang method.
	 */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);

	/* Reset the EEPROM, load the clock period. */
	CSR_WRITE_4(sc, BGE_EE_ADDR,
	    BGE_EEADDR_RESET|BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	DELAY(20);

	/* Issue the read EEPROM command. */
	CSR_WRITE_4(sc, BGE_EE_ADDR, BGE_EE_READCMD | addr);

	/* Wait for completion */
	for(i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_EE_ADDR) & BGE_EEADDR_DONE)
			break;
	}

	if (i == BGE_TIMEOUT * 10) {
		printf("%s: eeprom read timed out\n", sc->bge_dev.dv_xname);
		return (1);
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_EE_DATA);

	*dest = (byte >> ((addr % 4) * 8)) & 0xFF;

	return (0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
int
bge_read_eeprom(struct bge_softc *sc, caddr_t dest, int off, int cnt)
{
	int err = 0, i;
	u_int8_t byte = 0;

	for (i = 0; i < cnt; i++) {
		err = bge_eeprom_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return (err ? 1 : 0);
}

int
bge_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct bge_softc *sc = (struct bge_softc *)dev;
	u_int32_t val, autopoll;
	int i;

	/*
	 * Broadcom's own driver always assumes the internal
	 * PHY is at GMII address 1. On some chips, the PHY responds
	 * to accesses at all addresses, which could cause us to
	 * bogusly attach the PHY 32 times at probe type. Always
	 * restricting the lookup to address 1 is simpler than
	 * trying to figure out which chips revisions should be
	 * special-cased.
	 */
	if (phy != 1)
		return (0);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_CLRBIT(sc, BGE_STS_AUTOPOLL);
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_READ|BGE_MICOMM_BUSY|
	    BGE_MIPHY(phy)|BGE_MIREG(reg));

	for (i = 0; i < 200; i++) {
		delay(1);
		val = CSR_READ_4(sc, BGE_MI_COMM);
		if (!(val & BGE_MICOMM_BUSY))
			break;
		delay(10);
	}

	if (i == 200) {
		printf("%s: PHY read timed out\n", sc->bge_dev.dv_xname);
		val = 0;
		goto done;
	}

	val = CSR_READ_4(sc, BGE_MI_COMM);

done:
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_SETBIT(sc, BGE_STS_AUTOPOLL);
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (val & BGE_MICOMM_READFAIL)
		return (0);

	return (val & 0xFFFF);
}

void
bge_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct bge_softc *sc = (struct bge_softc *)dev;
	u_int32_t autopoll;
	int i;

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		DELAY(40);
		BGE_STS_CLRBIT(sc, BGE_STS_AUTOPOLL);
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(10); /* 40 usec is supposed to be adequate */
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_WRITE|BGE_MICOMM_BUSY|
	    BGE_MIPHY(phy)|BGE_MIREG(reg)|val);

	for (i = 0; i < 200; i++) {
		delay(1);
		if (!(CSR_READ_4(sc, BGE_MI_COMM) & BGE_MICOMM_BUSY))
			break;
		delay(10);
	}

	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_SETBIT(sc, BGE_STS_AUTOPOLL);
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (i == 200) {
		printf("%s: PHY read timed out\n", sc->bge_dev.dv_xname);
	}
}

void
bge_miibus_statchg(struct device *dev)
{
	struct bge_softc *sc = (struct bge_softc *)dev;
	struct mii_data *mii = &sc->bge_mii;

	/*
	 * Get flow control negotiation result.
	 */
	if (IFM_SUBTYPE(mii->mii_media.ifm_cur->ifm_media) == IFM_AUTO &&
	    (mii->mii_media_active & IFM_ETH_FMASK) != sc->bge_flowflags) {
		sc->bge_flowflags = mii->mii_media_active & IFM_ETH_FMASK;
		mii->mii_media_active &= ~IFM_ETH_FMASK;
	}

	BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_PORTMODE);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX)
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_GMII);
	else
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_MII);

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
	else
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);

	/*
	 * 802.3x flow control
	 */
	if (sc->bge_flowflags & IFM_ETH_RXPAUSE)
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_FLOWCTL_ENABLE);
	else
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_FLOWCTL_ENABLE);

	if (sc->bge_flowflags & IFM_ETH_TXPAUSE)
		BGE_SETBIT(sc, BGE_TX_MODE, BGE_TXMODE_FLOWCTL_ENABLE);
	else
		BGE_CLRBIT(sc, BGE_TX_MODE, BGE_TXMODE_FLOWCTL_ENABLE);
}

/*
 * Intialize a standard receive ring descriptor.
 */
int
bge_newbuf(struct bge_softc *sc, int i)
{
	bus_dmamap_t		dmap = sc->bge_cdata.bge_rx_std_map[i];
	struct bge_rx_bd	*r = &sc->bge_rdata->bge_rx_std_ring[i];
	struct mbuf		*m;
	int			error;

	m = MCLGETI(NULL, M_DONTWAIT, &sc->arpcom.ac_if, MCLBYTES);
	if (!m)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	if (!(sc->bge_flags & BGE_RX_ALIGNBUG))
	    m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf(sc->bge_dmatag, dmap, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->bge_dmatag, dmap, 0, dmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);
	sc->bge_cdata.bge_rx_std_chain[i] = m;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_rx_std_ring) +
		i * sizeof (struct bge_rx_bd),
	    sizeof (struct bge_rx_bd),
	    BUS_DMASYNC_POSTWRITE);

	BGE_HOSTADDR(r->bge_addr, dmap->dm_segs[0].ds_addr);
	r->bge_flags = BGE_RXBDFLAG_END;
	r->bge_len = m->m_len;
	r->bge_idx = i;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_rx_std_ring) +
		i * sizeof (struct bge_rx_bd),
	    sizeof (struct bge_rx_bd),
	    BUS_DMASYNC_PREWRITE);

	sc->bge_std_cnt++;

	return (0);
}

/*
 * Initialize a Jumbo receive ring descriptor.
 */
int
bge_newbuf_jumbo(struct bge_softc *sc, int i)
{
	bus_dmamap_t		dmap = sc->bge_cdata.bge_rx_jumbo_map[i];
	struct bge_ext_rx_bd	*r = &sc->bge_rdata->bge_rx_jumbo_ring[i];
	struct mbuf		*m;
	int			error;

	m = MCLGETI(NULL, M_DONTWAIT, &sc->arpcom.ac_if, BGE_JLEN);
	if (!m)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = BGE_JUMBO_FRAMELEN;
	if (!(sc->bge_flags & BGE_RX_ALIGNBUG))
	    m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf(sc->bge_dmatag, dmap, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->bge_dmatag, dmap, 0, dmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);
	sc->bge_cdata.bge_rx_jumbo_chain[i] = m;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_rx_jumbo_ring) +
		i * sizeof (struct bge_ext_rx_bd),
	    sizeof (struct bge_ext_rx_bd),
	    BUS_DMASYNC_POSTWRITE);

	/*
	 * Fill in the extended RX buffer descriptor.
	 */
	r->bge_bd.bge_flags = BGE_RXBDFLAG_JUMBO_RING | BGE_RXBDFLAG_END;
	r->bge_bd.bge_idx = i;
	r->bge_len3 = r->bge_len2 = r->bge_len1 = 0;
	switch (dmap->dm_nsegs) {
	case 4:
		BGE_HOSTADDR(r->bge_addr3, dmap->dm_segs[3].ds_addr);
		r->bge_len3 = dmap->dm_segs[3].ds_len;
		/* FALLTHROUGH */
	case 3:
		BGE_HOSTADDR(r->bge_addr2, dmap->dm_segs[2].ds_addr);
		r->bge_len2 = dmap->dm_segs[2].ds_len;
		/* FALLTHROUGH */
	case 2:
		BGE_HOSTADDR(r->bge_addr1, dmap->dm_segs[1].ds_addr);
		r->bge_len1 = dmap->dm_segs[1].ds_len;
		/* FALLTHROUGH */
	case 1:
		BGE_HOSTADDR(r->bge_bd.bge_addr, dmap->dm_segs[0].ds_addr);
		r->bge_bd.bge_len = dmap->dm_segs[0].ds_len;
		break;
	default:
		panic("%s: %d segments", __func__, dmap->dm_nsegs);
	}

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_rx_jumbo_ring) +
		i * sizeof (struct bge_ext_rx_bd),
	    sizeof (struct bge_ext_rx_bd),
	    BUS_DMASYNC_PREWRITE);

	sc->bge_jumbo_cnt++;

	return (0);
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
int
bge_init_rx_ring_std(struct bge_softc *sc)
{
	int i;

	if (ISSET(sc->bge_flags, BGE_RXRING_VALID))
		return (0);

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (bus_dmamap_create(sc->bge_dmatag, MCLBYTES, 1, MCLBYTES, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &sc->bge_cdata.bge_rx_std_map[i]) != 0) {
			printf("%s: unable to create dmamap for slot %d\n",
			    sc->bge_dev.dv_xname, i);
			goto uncreate;
		}
		bzero((char *)&sc->bge_rdata->bge_rx_std_ring[i],
		    sizeof(struct bge_rx_bd));
	}

	sc->bge_std = BGE_STD_RX_RING_CNT - 1;
	sc->bge_std_cnt = 0;
	bge_fill_rx_ring_std(sc);

	SET(sc->bge_flags, BGE_RXRING_VALID);

	return (0);

uncreate:
	while (--i) {
		bus_dmamap_destroy(sc->bge_dmatag,
		    sc->bge_cdata.bge_rx_std_map[i]);
	}
	return (1);
}

void
bge_rxtick(void *arg)
{
	struct bge_softc *sc = arg;
	int s;

	s = splnet();
	if (ISSET(sc->bge_flags, BGE_RXRING_VALID) &&
	    sc->bge_std_cnt <= 8)
		bge_fill_rx_ring_std(sc);
	if (ISSET(sc->bge_flags, BGE_JUMBO_RXRING_VALID) &&
	    sc->bge_jumbo_cnt <= 8)
		bge_fill_rx_ring_jumbo(sc);
	splx(s);
}

void
bge_fill_rx_ring_std(struct bge_softc *sc)
{
	int i;
	int post = 0;

	i = sc->bge_std;
	while (sc->bge_std_cnt < BGE_STD_RX_RING_CNT) {
		BGE_INC(i, BGE_STD_RX_RING_CNT);

		if (bge_newbuf(sc, i) != 0)
			break;

		sc->bge_std = i;
		post = 1;
	}

	if (post)
		bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);

	/*
	 * bge always needs more than 8 packets on the ring. if we cant do
	 * that now, then try again later.
	 */
	if (sc->bge_std_cnt <= 8)
		timeout_add(&sc->bge_rxtimeout, 1);
}

void
bge_free_rx_ring_std(struct bge_softc *sc)
{
	bus_dmamap_t dmap;
	struct mbuf *m;
	int i;

	if (!ISSET(sc->bge_flags, BGE_RXRING_VALID))
		return;

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		dmap = sc->bge_cdata.bge_rx_std_map[i];
		m = sc->bge_cdata.bge_rx_std_chain[i];
		if (m != NULL) {
			bus_dmamap_sync(sc->bge_dmatag, dmap, 0,
			    dmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_dmatag, dmap);
			m_freem(m);
			sc->bge_cdata.bge_rx_std_chain[i] = NULL;
		}
		bus_dmamap_destroy(sc->bge_dmatag, dmap);
		sc->bge_cdata.bge_rx_std_map[i] = NULL;
		bzero((char *)&sc->bge_rdata->bge_rx_std_ring[i],
		    sizeof(struct bge_rx_bd));
	}

	CLR(sc->bge_flags, BGE_RXRING_VALID);
}

int
bge_init_rx_ring_jumbo(struct bge_softc *sc)
{
	volatile struct bge_rcb *rcb;
	int i;

	if (ISSET(sc->bge_flags, BGE_JUMBO_RXRING_VALID))
		return (0);

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (bus_dmamap_create(sc->bge_dmatag, BGE_JLEN, 4, BGE_JLEN, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &sc->bge_cdata.bge_rx_jumbo_map[i]) != 0) {
			printf("%s: unable to create dmamap for slot %d\n",
			    sc->bge_dev.dv_xname, i);
			goto uncreate;
		}
		bzero((char *)&sc->bge_rdata->bge_rx_jumbo_ring[i],
		    sizeof(struct bge_ext_rx_bd));
	}

	sc->bge_jumbo = BGE_JUMBO_RX_RING_CNT - 1;
	sc->bge_jumbo_cnt = 0;
	bge_fill_rx_ring_jumbo(sc);

	SET(sc->bge_flags, BGE_JUMBO_RXRING_VALID);

	rcb = &sc->bge_rdata->bge_info.bge_jumbo_rx_rcb;
	rcb->bge_maxlen_flags =
	    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_USE_EXT_RX_BD);
	CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);

	return (0);

uncreate:
	while (--i) {
		bus_dmamap_destroy(sc->bge_dmatag,
		    sc->bge_cdata.bge_rx_jumbo_map[i]);
	}
	return (1);
}

void
bge_fill_rx_ring_jumbo(struct bge_softc *sc)
{
	int i;
	int post = 0;

	i = sc->bge_jumbo;
	while (sc->bge_jumbo_cnt < BGE_JUMBO_RX_RING_CNT) {
		BGE_INC(i, BGE_JUMBO_RX_RING_CNT);

		if (bge_newbuf_jumbo(sc, i) != 0)
			break;

		sc->bge_jumbo = i;
		post = 1;
	}

	if (post)
		bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);

	/*
	 * bge always needs more than 8 packets on the ring. if we cant do
	 * that now, then try again later.
	 */
	if (sc->bge_jumbo_cnt <= 8)
		timeout_add(&sc->bge_rxtimeout, 1);
}

void
bge_free_rx_ring_jumbo(struct bge_softc *sc)
{
	bus_dmamap_t dmap;
	struct mbuf *m;
	int i;

	if (!ISSET(sc->bge_flags, BGE_JUMBO_RXRING_VALID))
		return;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		dmap = sc->bge_cdata.bge_rx_jumbo_map[i];
		m = sc->bge_cdata.bge_rx_jumbo_chain[i];
		if (m != NULL) {
			bus_dmamap_sync(sc->bge_dmatag, dmap, 0,
			    dmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_dmatag, dmap);
			m_freem(m);
			sc->bge_cdata.bge_rx_jumbo_chain[i] = NULL;
		}
		bus_dmamap_destroy(sc->bge_dmatag, dmap);
		sc->bge_cdata.bge_rx_jumbo_map[i] = NULL;
		bzero((char *)&sc->bge_rdata->bge_rx_jumbo_ring[i],
		    sizeof(struct bge_ext_rx_bd));
	}

	CLR(sc->bge_flags, BGE_JUMBO_RXRING_VALID);
}

void
bge_free_tx_ring(struct bge_softc *sc)
{
	int i;
	struct txdmamap_pool_entry *dma;

	if (!(sc->bge_flags & BGE_TXRING_VALID))
		return;

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_tx_chain[i]);
			sc->bge_cdata.bge_tx_chain[i] = NULL;
			SLIST_INSERT_HEAD(&sc->txdma_list, sc->txdma[i],
					    link);
			sc->txdma[i] = 0;
		}
		bzero((char *)&sc->bge_rdata->bge_tx_ring[i],
		    sizeof(struct bge_tx_bd));
	}

	while ((dma = SLIST_FIRST(&sc->txdma_list))) {
		SLIST_REMOVE_HEAD(&sc->txdma_list, link);
		bus_dmamap_destroy(sc->bge_dmatag, dma->dmamap);
		free(dma, M_DEVBUF);
	}

	sc->bge_flags &= ~BGE_TXRING_VALID;
}

int
bge_init_tx_ring(struct bge_softc *sc)
{
	int i;
	bus_dmamap_t dmamap;
	struct txdmamap_pool_entry *dma;

	if (sc->bge_flags & BGE_TXRING_VALID)
		return (0);

	sc->bge_txcnt = 0;
	sc->bge_tx_saved_considx = 0;

	/* Initialize transmit producer index for host-memory send ring. */
	sc->bge_tx_prodidx = 0;
	bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);

	/* NIC-memory send ring not used; initialize to zero. */
	bge_writembx(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);

	SLIST_INIT(&sc->txdma_list);
	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (bus_dmamap_create(sc->bge_dmatag, BGE_JLEN,
		    BGE_NTXSEG, BGE_JLEN, 0, BUS_DMA_NOWAIT,
		    &dmamap))
			return (ENOBUFS);
		if (dmamap == NULL)
			panic("dmamap NULL in bge_init_tx_ring");
		dma = malloc(sizeof(*dma), M_DEVBUF, M_NOWAIT);
		if (dma == NULL) {
			printf("%s: can't alloc txdmamap_pool_entry\n",
			    sc->bge_dev.dv_xname);
			bus_dmamap_destroy(sc->bge_dmatag, dmamap);
			return (ENOMEM);
		}
		dma->dmamap = dmamap;
		SLIST_INSERT_HEAD(&sc->txdma_list, dma, link);
	}

	sc->bge_flags |= BGE_TXRING_VALID;

	return (0);
}

void
bge_iff(struct bge_softc *sc)
{
	struct arpcom		*ac = &sc->arpcom;
	struct ifnet		*ifp = &ac->ac_if;
	struct ether_multi	*enm;
	struct ether_multistep  step;
	u_int8_t		hashes[16];
	u_int32_t		h, rxmode;

	/* First, zot all the existing filters. */
	rxmode = CSR_READ_4(sc, BGE_RX_MODE) & ~BGE_RXMODE_RX_PROMISC;
	ifp->if_flags &= ~IFF_ALLMULTI;
	memset(hashes, 0x00, sizeof(hashes));

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		rxmode |= BGE_RXMODE_RX_PROMISC;
	} else if (ac->ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		memset(hashes, 0xff, sizeof(hashes));
	} else {
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			h = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

			setbit(hashes, h & 0x7F);

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	bus_space_write_raw_region_4(sc->bge_btag, sc->bge_bhandle, BGE_MAR0,
	    hashes, sizeof(hashes));
	CSR_WRITE_4(sc, BGE_RX_MODE, rxmode);
}

/*
 * Do endian, PCI and DMA initialization.
 */
void
bge_chipinit(struct bge_softc *sc)
{
	struct pci_attach_args	*pa = &(sc->bge_pa);
	u_int32_t dma_rw_ctl;
	int i;

	/* Set endianness before we access any non-PCI registers. */
	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_MISC_CTL,
	    BGE_INIT);

	/* Clear the MAC control register */
	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * Clear the MAC statistics block in the NIC's
	 * internal memory.
	 */
	for (i = BGE_STATS_BLOCK;
	    i < BGE_STATS_BLOCK_END + 1; i += sizeof(u_int32_t))
		BGE_MEMWIN_WRITE(pa->pa_pc, pa->pa_tag, i, 0);

	for (i = BGE_STATUS_BLOCK;
	    i < BGE_STATUS_BLOCK_END + 1; i += sizeof(u_int32_t))
		BGE_MEMWIN_WRITE(pa->pa_pc, pa->pa_tag, i, 0);

	/*
	 * Set up the PCI DMA control register.
	 */
	dma_rw_ctl = BGE_PCIDMARWCTL_RD_CMD_SHIFT(6) |
	    BGE_PCIDMARWCTL_WR_CMD_SHIFT(7);

	if (sc->bge_flags & BGE_PCIE) {
		/* Read watermark not used, 128 bytes for write. */
		dma_rw_ctl |= BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
	} else if (sc->bge_flags & BGE_PCIX) {
		/* PCI-X bus */
		if (BGE_IS_5714_FAMILY(sc)) {
			/* 256 bytes for read and write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(2) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(2);

			if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5780)
				dma_rw_ctl |= BGE_PCIDMARWCTL_ONEDMA_ATONCE_GLOBAL;
			else
				dma_rw_ctl |= BGE_PCIDMARWCTL_ONEDMA_ATONCE_LOCAL;
		} else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
			/* 1536 bytes for read, 384 bytes for write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(7) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
		} else {
			/* 384 bytes for read and write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(3) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3) |
			    (0x0F);
		}

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
			u_int32_t tmp;

			/* Set ONEDMA_ATONCE for hardware workaround. */
			tmp = CSR_READ_4(sc, BGE_PCI_CLKCTL) & 0x1f;
			if (tmp == 6 || tmp == 7)
				dma_rw_ctl |=
				    BGE_PCIDMARWCTL_ONEDMA_ATONCE_GLOBAL;

			/* Set PCI-X DMA write workaround. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_ASRT_ALL_BE;
		}
	} else {
		/* Conventional PCI bus: 256 bytes for read and write. */
		dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(7) |
		    BGE_PCIDMARWCTL_WR_WAT_SHIFT(7);

		if (BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5705 &&
		    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5750)
			dma_rw_ctl |= 0x0F;
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701)
		dma_rw_ctl |= BGE_PCIDMARWCTL_USE_MRM |
		    BGE_PCIDMARWCTL_ASRT_ALL_BE;
 
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704)
		dma_rw_ctl &= ~BGE_PCIDMARWCTL_MINDMA;

	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_DMA_RW_CTL, dma_rw_ctl);

	/*
	 * Set up general mode register.
	 */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS|
		    BGE_MODECTL_MAC_ATTN_INTR|BGE_MODECTL_HOST_SEND_BDS|
		    BGE_MODECTL_TX_NO_PHDR_CSUM);

	/*
	 * BCM5701 B5 have a bug causing data corruption when using
	 * 64-bit DMA reads, which can be terminated early and then
	 * completed later as 32-bit accesses, in combination with
	 * certain bridges.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701 &&
	    sc->bge_chipid == BGE_CHIPID_BCM5701_B5)
		BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_FORCE_PCI32);

	/*
	 * Disable memory write invalidate.  Apparently it is not supported
	 * properly by these devices.
	 */
	PCI_CLRBIT(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    PCI_COMMAND_INVALIDATE_ENABLE);

#ifdef __brokenalpha__
	/*
	 * Must insure that we do not cross an 8K (bytes) boundary
	 * for DMA reads.  Our highest limit is 1K bytes.  This is a
	 * restriction on some ALPHA platforms with early revision
	 * 21174 PCI chipsets, such as the AlphaPC 164lx
	 */
	PCI_SETBIT(pa->pa_pc, pa->pa_tag, BGE_PCI_DMA_RW_CTL,
	    BGE_PCI_READ_BNDRY_1024);
#endif

	/* Set the timer prescaler (always 66MHz) */
	CSR_WRITE_4(sc, BGE_MISC_CFG, 65 << 1/*BGE_32BITTIME_66MHZ*/);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		DELAY(40);	/* XXX */

		/* Put PHY into ready state */
		BGE_CLRBIT(sc, BGE_MISC_CFG, BGE_MISCCFG_EPHY_IDDQ);
		CSR_READ_4(sc, BGE_MISC_CFG); /* Flush */
		DELAY(40);
	}
}

int
bge_blockinit(struct bge_softc *sc)
{
	volatile struct bge_rcb		*rcb;
	vaddr_t			rcb_addr;
	int			i;
	bge_hostaddr		taddr;
	u_int32_t		val;

	/*
	 * Initialize the memory window pointer register so that
	 * we can access the first 32K of internal NIC RAM. This will
	 * allow us to set up the TX send ring RCBs and the RX return
	 * ring RCBs, plus other things which live in NIC memory.
	 */
	CSR_WRITE_4(sc, BGE_PCI_MEMWIN_BASEADDR, 0);

	/* Configure mbuf memory pool */
	if (BGE_IS_5700_FAMILY(sc)) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR,
		    BGE_BUFFPOOL_1);

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704)
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
		else
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);

		/* Configure DMA resource pool */
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_BASEADDR,
		    BGE_DMA_DESCRIPTORS);
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LEN, 0x2000);
	}

	/* Configure mbuf pool watermarks */
	/* new Broadcom docs strongly recommend these: */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57765) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x2a);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0xa0);
	} else if (BGE_IS_5705_PLUS(sc)) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x04);
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x10);
		} else {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x10);
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);
		}
	} else {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x50);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x20);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);
	}

	/* Configure DMA resource watermarks */
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LOWAT, 5);
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_HIWAT, 10);

	/* Enable buffer manager */
	CSR_WRITE_4(sc, BGE_BMAN_MODE,
	    BGE_BMANMODE_ENABLE|BGE_BMANMODE_LOMBUF_ATTN);

	/* Poll for buffer manager start indication */
	for (i = 0; i < 2000; i++) {
		if (CSR_READ_4(sc, BGE_BMAN_MODE) & BGE_BMANMODE_ENABLE)
			break;
		DELAY(10);
	}

	if (i == 2000) {
		printf("%s: buffer manager failed to start\n",
		    sc->bge_dev.dv_xname);
		return (ENXIO);
	}

	/* Enable flow-through queues */
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	/* Wait until queue initialization is complete */
	for (i = 0; i < 2000; i++) {
		if (CSR_READ_4(sc, BGE_FTQ_RESET) == 0)
			break;
		DELAY(10);
	}

	if (i == 2000) {
		printf("%s: flow-through queue init failed\n",
		    sc->bge_dev.dv_xname);
		return (ENXIO);
	}

	/* Initialize the standard RX ring control block */
	rcb = &sc->bge_rdata->bge_info.bge_std_rx_rcb;
	BGE_HOSTADDR(rcb->bge_hostaddr, BGE_RING_DMA_ADDR(sc, bge_rx_std_ring));
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57765)
		rcb->bge_maxlen_flags = (BGE_RCB_MAXLEN_FLAGS(512, 0) |
					(ETHER_MAX_DIX_LEN << 2));
	else if (BGE_IS_5705_PLUS(sc))
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(512, 0);
	else
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(ETHER_MAX_DIX_LEN, 0);
	rcb->bge_nicaddr = BGE_STD_RX_RINGS;
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_HI, rcb->bge_hostaddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_LO, rcb->bge_hostaddr.bge_addr_lo);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_NICADDR, rcb->bge_nicaddr);

	/*
	 * Initialize the Jumbo RX ring control block
	 * We set the 'ring disabled' bit in the flags
	 * field until we're actually ready to start
	 * using this ring (i.e. once we set the MTU
	 * high enough to require it).
	 */
	if (BGE_IS_JUMBO_CAPABLE(sc)) {
		rcb = &sc->bge_rdata->bge_info.bge_jumbo_rx_rcb;
		BGE_HOSTADDR(rcb->bge_hostaddr,
		    BGE_RING_DMA_ADDR(sc, bge_rx_jumbo_ring));
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0,
		    BGE_RCB_FLAG_USE_EXT_RX_BD | BGE_RCB_FLAG_RING_DISABLED);
		rcb->bge_nicaddr = BGE_JUMBO_RX_RINGS;

		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_HI,
		    rcb->bge_hostaddr.bge_addr_hi);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_LO,
		    rcb->bge_hostaddr.bge_addr_lo);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_NICADDR,
		    rcb->bge_nicaddr);

		/* Set up dummy disabled mini ring RCB */
		rcb = &sc->bge_rdata->bge_info.bge_mini_rx_rcb;
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED);
		CSR_WRITE_4(sc, BGE_RX_MINI_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);

		bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
		    offsetof(struct bge_ring_data, bge_info),
		    sizeof (struct bge_gib),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}

	/* Choose de-pipeline mode for BCM5906 A0, A1 and A2. */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5906_A0 ||
		    sc->bge_chipid == BGE_CHIPID_BCM5906_A1 ||
		    sc->bge_chipid == BGE_CHIPID_BCM5906_A2)
			CSR_WRITE_4(sc, BGE_ISO_PKT_TX,
			    (CSR_READ_4(sc, BGE_ISO_PKT_TX) & ~3) | 2);
	}
	/*
	 * Set the BD ring replenish thresholds. The recommended
	 * values are 1/8th the number of descriptors allocated to
	 * each ring, but since we try to avoid filling the entire
	 * ring we set these to the minimal value of 8.  This needs to
	 * be done on several of the supported chip revisions anyway,
	 * to work around HW bugs.
	 */
	CSR_WRITE_4(sc, BGE_RBDI_STD_REPL_THRESH, 8);
	CSR_WRITE_4(sc, BGE_RBDI_JUMBO_REPL_THRESH, 8);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57765) {
		CSR_WRITE_4(sc, BGE_STD_REPL_LWM, 4);
		CSR_WRITE_4(sc, BGE_JUMBO_REPL_LWM, 4);
	}

	/*
	 * Disable all unused send rings by setting the 'ring disabled'
	 * bit in the flags field of all the TX send ring control blocks.
	 * These are located in NIC memory.
	 */
	rcb_addr = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	for (i = 0; i < BGE_TX_RINGS_EXTSSRAM_MAX; i++) {
		RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, 0);
		rcb_addr += sizeof(struct bge_rcb);
	}

	/* Configure TX RCB 0 (we use only the first ring) */
	rcb_addr = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_tx_ring));
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, rcb_addr, bge_nicaddr,
		    BGE_NIC_TXRING_ADDR(0, BGE_TX_RING_CNT));
	if (BGE_IS_5700_FAMILY(sc))
		RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(BGE_TX_RING_CNT, 0));

	/* Disable all unused RX return rings */
	rcb_addr = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	for (i = 0; i < BGE_RX_RINGS_MAX; i++) {
		RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_hi, 0);
		RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_lo, 0);
		RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt,
			BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, 0);
		bge_writembx(sc, BGE_MBX_RX_CONS0_LO +
		    (i * (sizeof(u_int64_t))), 0);
		rcb_addr += sizeof(struct bge_rcb);
	}

	/* Initialize RX ring indexes */
	bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, 0);
	bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, 0);
	bge_writembx(sc, BGE_MBX_RX_MINI_PROD_LO, 0);

	/*
	 * Set up RX return ring 0
	 * Note that the NIC address for RX return rings is 0x00000000.
	 * The return rings live entirely within the host, so the
	 * nicaddr field in the RCB isn't used.
	 */
	rcb_addr = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_rx_return_ring));
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, 0x00000000);
	RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
	    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt, 0));

	/* Set random backoff seed for TX */
	CSR_WRITE_4(sc, BGE_TX_RANDOM_BACKOFF,
	    sc->arpcom.ac_enaddr[0] + sc->arpcom.ac_enaddr[1] +
	    sc->arpcom.ac_enaddr[2] + sc->arpcom.ac_enaddr[3] +
	    sc->arpcom.ac_enaddr[4] + sc->arpcom.ac_enaddr[5] +
	    BGE_TX_BACKOFF_SEED_MASK);

	/* Set inter-packet gap */
	CSR_WRITE_4(sc, BGE_TX_LENGTHS, 0x2620);

	/*
	 * Specify which ring to use for packets that don't match
	 * any RX rules.
	 */
	CSR_WRITE_4(sc, BGE_RX_RULES_CFG, 0x08);

	/*
	 * Configure number of RX lists. One interrupt distribution
	 * list, sixteen active lists, one bad frames class.
	 */
	CSR_WRITE_4(sc, BGE_RXLP_CFG, 0x181);

	/* Inialize RX list placement stats mask. */
	CSR_WRITE_4(sc, BGE_RXLP_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_RXLP_STATS_CTL, 0x1);

	/* Disable host coalescing until we get it set up */
	CSR_WRITE_4(sc, BGE_HCC_MODE, 0x00000000);

	/* Poll to make sure it's shut down. */
	for (i = 0; i < 2000; i++) {
		if (!(CSR_READ_4(sc, BGE_HCC_MODE) & BGE_HCCMODE_ENABLE))
			break;
		DELAY(10);
	}

	if (i == 2000) {
		printf("%s: host coalescing engine failed to idle\n",
		    sc->bge_dev.dv_xname);
		return (ENXIO);
	}

	/* Set up host coalescing defaults */
	CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS, sc->bge_rx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS, sc->bge_tx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS, sc->bge_rx_max_coal_bds);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS, sc->bge_tx_max_coal_bds);
	if (BGE_IS_5700_FAMILY(sc)) {
		CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS_INT, 0);
		CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS_INT, 0);
	}
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 0);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 0);

	/* Set up address of statistics block */
	if (BGE_IS_5700_FAMILY(sc)) {
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_HI, 0);
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_LO,
			    BGE_RING_DMA_ADDR(sc, bge_info.bge_stats));

		CSR_WRITE_4(sc, BGE_HCC_STATS_BASEADDR, BGE_STATS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_BASEADDR, BGE_STATUS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATS_TICKS, sc->bge_stat_ticks);
	}

	/* Set up address of status block */
	BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_status_block));
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_HI, taddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_LO, taddr.bge_addr_lo);

	sc->bge_rdata->bge_status_block.bge_idx[0].bge_rx_prod_idx = 0;
	sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx = 0;

	/* Turn on host coalescing state machine */
	CSR_WRITE_4(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);

	/* Turn on RX BD completion state machine and enable attentions */
	CSR_WRITE_4(sc, BGE_RBDC_MODE,
	    BGE_RBDCMODE_ENABLE|BGE_RBDCMODE_ATTN);

	/* Turn on RX list placement state machine */
	CSR_WRITE_4(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);

	/* Turn on RX list selector state machine. */
	if (BGE_IS_5700_FAMILY(sc))
		CSR_WRITE_4(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);

	val = BGE_MACMODE_TXDMA_ENB | BGE_MACMODE_RXDMA_ENB |
	    BGE_MACMODE_RX_STATS_CLEAR | BGE_MACMODE_TX_STATS_CLEAR |
	    BGE_MACMODE_RX_STATS_ENB | BGE_MACMODE_TX_STATS_ENB |
	    BGE_MACMODE_FRMHDR_DMA_ENB;

	if (sc->bge_flags & BGE_PHY_FIBER_TBI)
	    val |= BGE_PORTMODE_TBI;
	else if (sc->bge_flags & BGE_PHY_FIBER_MII)
	    val |= BGE_PORTMODE_GMII;
	else
	    val |= BGE_PORTMODE_MII;

	/* Turn on DMA, clear stats */
	CSR_WRITE_4(sc, BGE_MAC_MODE, val);

	/* Set misc. local control, enable interrupts on attentions */
	CSR_WRITE_4(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_ONATTN);

#ifdef notdef
	/* Assert GPIO pins for PHY reset */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUT0|
	    BGE_MLC_MISCIO_OUT1|BGE_MLC_MISCIO_OUT2);
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUTEN0|
	    BGE_MLC_MISCIO_OUTEN1|BGE_MLC_MISCIO_OUTEN2);
#endif

	/* Turn on DMA completion state machine */
	if (BGE_IS_5700_FAMILY(sc))
		CSR_WRITE_4(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);

	val = BGE_WDMAMODE_ENABLE|BGE_WDMAMODE_ALL_ATTNS;

	/* Enable host coalescing bug fix. */
	if (BGE_IS_5755_PLUS(sc))
		val |= BGE_WDMAMODE_STATUS_TAG_FIX;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785)
		val |= BGE_WDMAMODE_BURST_ALL_DATA;

	/* Turn on write DMA state machine */
	CSR_WRITE_4(sc, BGE_WDMA_MODE, val);

	val = BGE_RDMAMODE_ENABLE|BGE_RDMAMODE_ALL_ATTNS;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717)
		val |= BGE_RDMAMODE_MULT_DMA_RD_DIS;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57780)
		val |= BGE_RDMAMODE_BD_SBD_CRPT_ATTN |
		       BGE_RDMAMODE_MBUF_RBD_CRPT_ATTN |
		       BGE_RDMAMODE_MBUF_SBD_CRPT_ATTN;

	if (sc->bge_flags & BGE_PCIE)
		val |= BGE_RDMAMODE_FIFO_LONG_BURST;

	/* Turn on read DMA state machine */
	CSR_WRITE_4(sc, BGE_RDMA_MODE, val);

	/* Turn on RX data completion state machine */
	CSR_WRITE_4(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);

	/* Turn on RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);

	/* Turn on RX data and RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RDBDI_MODE, BGE_RDBDIMODE_ENABLE);

	/* Turn on Mbuf cluster free state machine */
	if (BGE_IS_5700_FAMILY(sc))
		CSR_WRITE_4(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	/* Turn on send BD completion state machine */
	CSR_WRITE_4(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	val = BGE_SDCMODE_ENABLE;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761)
		val |= BGE_SDCMODE_CDELAY;

	/* Turn on send data completion state machine */
	CSR_WRITE_4(sc, BGE_SDC_MODE, val);

	/* Turn on send data initiator state machine */
	CSR_WRITE_4(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);

	/* Turn on send BD initiator state machine */
	CSR_WRITE_4(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);

	/* Turn on send BD selector state machine */
	CSR_WRITE_4(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);

	CSR_WRITE_4(sc, BGE_SDI_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_SDI_STATS_CTL,
	    BGE_SDISTATSCTL_ENABLE|BGE_SDISTATSCTL_FASTER);

	/* ack/clear link change events */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);

	/* Enable PHY auto polling (for MII/GMII only) */
	if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
		CSR_WRITE_4(sc, BGE_MI_STS, BGE_MISTS_LINK);
 	} else {
		BGE_STS_SETBIT(sc, BGE_STS_AUTOPOLL);
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL|10<<16);
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700)
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
	}

	/*
	 * Clear any pending link state attention.
	 * Otherwise some link state change events may be lost until attention
	 * is cleared by bge_intr() -> bge_link_upd() sequence.
	 * It's not necessary on newer BCM chips - perhaps enabling link
	 * state change attentions implies clearing pending attention.
	 */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);

	/* Enable link state change attentions. */
	BGE_SETBIT(sc, BGE_MAC_EVT_ENB, BGE_EVTENB_LINK_CHANGED);

	return (0);
}

const struct bge_revision *
bge_lookup_rev(u_int32_t chipid)
{
	const struct bge_revision *br;

	for (br = bge_revisions; br->br_name != NULL; br++) {
		if (br->br_chipid == chipid)
			return (br);
	}

	for (br = bge_majorrevs; br->br_name != NULL; br++) {
		if (br->br_chipid == BGE_ASICREV(chipid))
			return (br);
	}

	return (NULL);
}

/*
 * Probe for a Broadcom chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match. Note
 * that since the Broadcom controller contains VPD support, we
 * can get the device name string from the controller itself instead
 * of the compiled-in string. This is a little slow, but it guarantees
 * we'll always announce the right product name.
 */
int
bge_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, bge_devices, nitems(bge_devices)));
}

void
bge_attach(struct device *parent, struct device *self, void *aux)
{
	struct bge_softc	*sc = (struct bge_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	const struct bge_revision *br;
	pcireg_t		pm_ctl, memtype, subid;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	bus_size_t		size;
	bus_dma_segment_t	seg;
	int			rseg, gotenaddr = 0;
	u_int32_t		hwcfg = 0;
	u_int32_t		mac_addr = 0;
	u_int32_t		misccfg;
	struct ifnet		*ifp;
	caddr_t			kva;
#ifdef __sparc64__
	char			name[32];
#endif

	sc->bge_pa = *pa;

	subid = pci_conf_read(pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	/*
	 * Map control/status registers.
	 */
	DPRINTFN(5, ("Map control/status regs\n"));

	DPRINTFN(5, ("pci_mapreg_map\n"));
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BGE_PCI_BAR0);
	if (pci_mapreg_map(pa, BGE_PCI_BAR0, memtype, 0, &sc->bge_btag,
	    &sc->bge_bhandle, NULL, &size, 0)) {
		printf(": can't find mem space\n");
		return;
	}

	DPRINTFN(5, ("pci_intr_map\n"));
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail_1;
	}

	DPRINTFN(5, ("pci_intr_string\n"));
	intrstr = pci_intr_string(pc, ih);

	/*
	 * Kludge for 5700 Bx bug: a hardware bug (PCIX byte enable?)
	 * can clobber the chip's PCI config-space power control registers,
	 * leaving the card in D3 powersave state.
	 * We do not have memory-mapped registers in this state,
	 * so force device into D0 state before starting initialization.
	 */
	pm_ctl = pci_conf_read(pc, pa->pa_tag, BGE_PCI_PWRMGMT_CMD);
	pm_ctl &= ~(PCI_PWR_D0|PCI_PWR_D1|PCI_PWR_D2|PCI_PWR_D3);
	pm_ctl |= (1 << 8) | PCI_PWR_D0 ; /* D0 state */
	pci_conf_write(pc, pa->pa_tag, BGE_PCI_PWRMGMT_CMD, pm_ctl);
	DELAY(1000);	/* 27 usec is allegedly sufficent */

	/*
	 * Save ASIC rev.
	 */
	sc->bge_chipid =
	     (pci_conf_read(pc, pa->pa_tag, BGE_PCI_MISC_CTL)
	      >> BGE_PCIMISCCTL_ASICREV_SHIFT);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_USE_PRODID_REG) {
		if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5717 ||
		    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5718 ||
		    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5724)
			sc->bge_chipid = pci_conf_read(pc, pa->pa_tag,
			    BGE_PCI_GEN2_PRODID_ASICREV);
		else if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57761 ||
			 PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57765 ||
			 PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57781 ||
			 PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57785 ||
			 PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57791 ||
			 PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57795)
			sc->bge_chipid = pci_conf_read(pc, pa->pa_tag,
			    BGE_PCI_GEN15_PRODID_ASICREV);
		else
			sc->bge_chipid = pci_conf_read(pc, pa->pa_tag,
			    BGE_PCI_PRODID_ASICREV);
	}

	printf(", ");
	br = bge_lookup_rev(sc->bge_chipid);
	if (br == NULL)
		printf("unknown ASIC (0x%x)", sc->bge_chipid);
	else
		printf("%s (0x%x)", br->br_name, sc->bge_chipid);

	/*
	 * PCI Express or PCI-X controller check.
	 */
	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_PCIEXPRESS,
	    NULL, NULL) != 0) {
		sc->bge_flags |= BGE_PCIE;
	} else {
		if ((pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_PCISTATE) &
		    BGE_PCISTATE_PCI_BUSMODE) == 0)
			sc->bge_flags |= BGE_PCIX;
	}

	/*
	 * SEEPROM check.
	 */
#ifdef __sparc64__
	/*
	 * Onboard interfaces on UltraSPARC systems generally don't
	 * have a SEEPROM fitted.  These interfaces, and cards that
	 * have FCode, are named "network" by the PROM, whereas cards
	 * without FCode show up as "ethernet".  Since we don't really
	 * need the information from the SEEPROM on cards that have
	 * FCode it's fine to pretend they don't have one.
	 */
	if (OF_getprop(PCITAG_NODE(pa->pa_tag), "name", name,
	    sizeof(name)) > 0 && strcmp(name, "network") == 0)
		sc->bge_flags |= BGE_NO_EEPROM;
#endif
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704)
		sc->bge_flags |= BGE_5700_FAMILY;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5714_A0 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5780 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5714)
		sc->bge_flags |= BGE_5714_FAMILY;

	/* Intentionally exclude BGE_ASICREV_BCM5906 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5755 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5787 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57765 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57780)
		sc->bge_flags |= BGE_5755_PLUS;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5750 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5752 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906 ||
	    BGE_IS_5755_PLUS(sc) ||
	    BGE_IS_5714_FAMILY(sc))
		sc->bge_flags |= BGE_5750_PLUS;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 ||
	    BGE_IS_5750_PLUS(sc))
		sc->bge_flags |= BGE_5705_PLUS;

	/*
	 * When using the BCM5701 in PCI-X mode, data corruption has
	 * been observed in the first few bytes of some received packets.
	 * Aligning the packet buffer in memory eliminates the corruption.
	 * Unfortunately, this misaligns the packet payloads.  On platforms
	 * which do not support unaligned accesses, we will realign the
	 * payloads by copying the received packets.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701 &&
	    sc->bge_flags & BGE_PCIX)
		sc->bge_flags |= BGE_RX_ALIGNBUG;

	if (BGE_IS_5700_FAMILY(sc))
		sc->bge_flags |= BGE_JUMBO_CAPABLE;

	if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701) &&
	    PCI_VENDOR(subid) == DELL_VENDORID)
		sc->bge_flags |= BGE_NO_3LED;

	misccfg = CSR_READ_4(sc, BGE_MISC_CFG);
	misccfg &= BGE_MISCCFG_BOARD_ID_MASK;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 &&
	    (misccfg == BGE_MISCCFG_BOARD_ID_5788 ||
	     misccfg == BGE_MISCCFG_BOARD_ID_5788M))
		sc->bge_flags |= BGE_IS_5788;

	if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 &&
	     (misccfg == 0x4000 || misccfg == 0x8000)) ||
	    (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 &&
	     PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	     (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5901 ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5901A2 ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5705F)) ||
	    (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	     (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5751F ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5753F ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5787F)) ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57790 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		sc->bge_flags |= BGE_10_100_ONLY;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 &&
	     (sc->bge_chipid != BGE_CHIPID_BCM5705_A0 &&
	      sc->bge_chipid != BGE_CHIPID_BCM5705_A1)) ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		sc->bge_flags |= BGE_NO_ETH_WIRE_SPEED;

	if (sc->bge_chipid == BGE_CHIPID_BCM5701_A0 ||
	    sc->bge_chipid == BGE_CHIPID_BCM5701_B0)
		sc->bge_flags |= BGE_PHY_CRC_BUG;
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5703_AX ||
	    BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5704_AX)
		sc->bge_flags |= BGE_PHY_ADC_BUG;
	if (sc->bge_chipid == BGE_CHIPID_BCM5704_A0)
		sc->bge_flags |= BGE_PHY_5704_A0_BUG;

	if ((BGE_IS_5705_PLUS(sc)) &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5906 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5717 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5785 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM57765 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM57780) {
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5755 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5787) {
			if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BROADCOM_BCM5722 &&
			    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BROADCOM_BCM5756)
				sc->bge_flags |= BGE_PHY_JITTER_BUG;
			if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5755M)
				sc->bge_flags |= BGE_PHY_ADJUST_TRIM;
		} else
			sc->bge_flags |= BGE_PHY_BER_BUG;
	}

	/* Try to reset the chip. */
	DPRINTFN(5, ("bge_reset\n"));
	bge_reset(sc);

	bge_chipinit(sc);

#ifdef __sparc64__
	if (!gotenaddr) {
		if (OF_getprop(PCITAG_NODE(pa->pa_tag), "local-mac-address",
		    sc->arpcom.ac_enaddr, ETHER_ADDR_LEN) == ETHER_ADDR_LEN)
			gotenaddr = 1;
	}
#endif

	/*
	 * Get station address from the EEPROM.
	 */
	if (!gotenaddr) {
		mac_addr = bge_readmem_ind(sc, 0x0c14);
		if ((mac_addr >> 16) == 0x484b) {
			sc->arpcom.ac_enaddr[0] = (u_char)(mac_addr >> 8);
			sc->arpcom.ac_enaddr[1] = (u_char)mac_addr;
			mac_addr = bge_readmem_ind(sc, 0x0c18);
			sc->arpcom.ac_enaddr[2] = (u_char)(mac_addr >> 24);
			sc->arpcom.ac_enaddr[3] = (u_char)(mac_addr >> 16);
			sc->arpcom.ac_enaddr[4] = (u_char)(mac_addr >> 8);
			sc->arpcom.ac_enaddr[5] = (u_char)mac_addr;
			gotenaddr = 1;
		}
	}
	if (!gotenaddr) {
		int mac_offset = BGE_EE_MAC_OFFSET;

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
			mac_offset = BGE_EE_MAC_OFFSET_5906;

		if (bge_read_nvram(sc, (caddr_t)&sc->arpcom.ac_enaddr,
		    mac_offset + 2, ETHER_ADDR_LEN) == 0)
			gotenaddr = 1;
	}
	if (!gotenaddr && (!(sc->bge_flags & BGE_NO_EEPROM))) {
		if (bge_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr,
		    BGE_EE_MAC_OFFSET + 2, ETHER_ADDR_LEN) == 0)
			gotenaddr = 1;
	}

#ifdef __sparc64__
	if (!gotenaddr) {
		extern void myetheraddr(u_char *);

		myetheraddr(sc->arpcom.ac_enaddr);
		gotenaddr = 1;
	}
#endif

	if (!gotenaddr) {
		printf(": failed to read station address\n");
		goto fail_1;
	}

	/* Allocate the general information block and ring buffers. */
	sc->bge_dmatag = pa->pa_dmat;
	DPRINTFN(5, ("bus_dmamem_alloc\n"));
	if (bus_dmamem_alloc(sc->bge_dmatag, sizeof(struct bge_ring_data),
			     PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't alloc rx buffers\n");
		goto fail_1;
	}
	DPRINTFN(5, ("bus_dmamem_map\n"));
	if (bus_dmamem_map(sc->bge_dmatag, &seg, rseg,
			   sizeof(struct bge_ring_data), &kva,
			   BUS_DMA_NOWAIT)) {
		printf(": can't map dma buffers (%lu bytes)\n",
		    sizeof(struct bge_ring_data));
		goto fail_2;
	}
	DPRINTFN(5, ("bus_dmamem_create\n"));
	if (bus_dmamap_create(sc->bge_dmatag, sizeof(struct bge_ring_data), 1,
	    sizeof(struct bge_ring_data), 0,
	    BUS_DMA_NOWAIT, &sc->bge_ring_map)) {
		printf(": can't create dma map\n");
		goto fail_3;
	}
	DPRINTFN(5, ("bus_dmamem_load\n"));
	if (bus_dmamap_load(sc->bge_dmatag, sc->bge_ring_map, kva,
			    sizeof(struct bge_ring_data), NULL,
			    BUS_DMA_NOWAIT)) {
		goto fail_4;
	}

	DPRINTFN(5, ("bzero\n"));
	sc->bge_rdata = (struct bge_ring_data *)kva;

	bzero(sc->bge_rdata, sizeof(struct bge_ring_data));

	/* Set default tuneable values. */
	sc->bge_stat_ticks = BGE_TICKS_PER_SEC;
	sc->bge_rx_coal_ticks = 150;
	sc->bge_rx_max_coal_bds = 64;
	sc->bge_tx_coal_ticks = 300;
	sc->bge_tx_max_coal_bds = 400;

	/* 5705 limits RX return ring to 512 entries. */
	if (BGE_IS_5700_FAMILY(sc) ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57765)
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT;
	else
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT_5705;

	/* Set up ifnet structure */
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bge_ioctl;
	ifp->if_start = bge_start;
	ifp->if_watchdog = bge_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, BGE_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);

	/* lwm must be greater than the replenish threshold */
	m_clsetwms(ifp, MCLBYTES, 17, BGE_STD_RX_RING_CNT);
	m_clsetwms(ifp, BGE_JLEN, 17, BGE_JUMBO_RX_RING_CNT);

	DPRINTFN(5, ("bcopy\n"));
	bcopy(sc->bge_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	/*
	 * 5700 B0 chips do not support checksumming correctly due
	 * to hardware bugs.
	 */
	if (sc->bge_chipid != BGE_CHIPID_BCM5700_B0)
		ifp->if_capabilities |= IFCAP_CSUM_IPv4;
#if 0	/* TCP/UDP checksum offload breaks with pf(4) */
		ifp->if_capabilities |= IFCAP_CSUM_TCPv4|IFCAP_CSUM_UDPv4;
#endif

	if (BGE_IS_JUMBO_CAPABLE(sc))
		ifp->if_hardmtu = BGE_JUMBO_MTU;

	/*
	 * Do MII setup.
	 */
	DPRINTFN(5, ("mii setup\n"));
	sc->bge_mii.mii_ifp = ifp;
	sc->bge_mii.mii_readreg = bge_miibus_readreg;
	sc->bge_mii.mii_writereg = bge_miibus_writereg;
	sc->bge_mii.mii_statchg = bge_miibus_statchg;

	/*
	 * Figure out what sort of media we have by checking the hardware
	 * config word in the first 32K of internal NIC memory, or fall back to
	 * examining the EEPROM if necessary.  Note: on some BCM5700 cards,
	 * this value seems to be unset. If that's the case, we have to rely on
	 * identifying the NIC by its PCI subsystem ID, as we do below for the
	 * SysKonnect SK-9D41.
	 */
	if (bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_SIG) == BGE_MAGIC_NUMBER)
		hwcfg = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_NICCFG);
	else if (!(sc->bge_flags & BGE_NO_EEPROM)) {
		if (bge_read_eeprom(sc, (caddr_t)&hwcfg, BGE_EE_HWCFG_OFFSET,
		    sizeof(hwcfg))) {
			printf(": failed to read media type\n");
			goto fail_5;
		}
		hwcfg = ntohl(hwcfg);
	}

	/* The SysKonnect SK-9D41 is a 1000baseSX card. */
	if (PCI_PRODUCT(subid) == SK_SUBSYSID_9D41 ||
	    (hwcfg & BGE_HWCFG_MEDIA) == BGE_MEDIA_FIBER) {
		if (BGE_IS_5714_FAMILY(sc) ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717)
		    sc->bge_flags |= BGE_PHY_FIBER_MII;
		else
		    sc->bge_flags |= BGE_PHY_FIBER_TBI;
	}

	/* Hookup IRQ last. */
	DPRINTFN(5, ("pci_intr_establish\n"));
	sc->bge_intrhand = pci_intr_establish(pc, ih, IPL_NET, bge_intr, sc,
	    sc->bge_dev.dv_xname);
	if (sc->bge_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_5;
	}

	/*
	 * A Broadcom chip was detected. Inform the world.
	 */
	printf(": %s, address %s\n", intrstr,
	    ether_sprintf(sc->arpcom.ac_enaddr));

	if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
		ifmedia_init(&sc->bge_ifmedia, IFM_IMASK, bge_ifmedia_upd,
		    bge_ifmedia_sts);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_1000_SX|IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->bge_ifmedia, IFM_ETHER|IFM_AUTO);
		sc->bge_ifmedia.ifm_media = sc->bge_ifmedia.ifm_cur->ifm_media;
	} else {
		int mii_flags;

		/*
		 * Do transceiver setup.
		 */
		ifmedia_init(&sc->bge_mii.mii_media, 0, bge_ifmedia_upd,
			     bge_ifmedia_sts);
		mii_flags = MIIF_DOPAUSE;
		if (sc->bge_flags & BGE_PHY_FIBER_MII)
			mii_flags |= MIIF_HAVEFIBER;
		mii_attach(&sc->bge_dev, &sc->bge_mii, 0xffffffff,
			   MII_PHY_ANY, MII_OFFSET_ANY, mii_flags);
		
		if (LIST_FIRST(&sc->bge_mii.mii_phys) == NULL) {
			printf("%s: no PHY found!\n", sc->bge_dev.dv_xname);
			ifmedia_add(&sc->bge_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL, 0, NULL);
			ifmedia_set(&sc->bge_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL);
		} else
			ifmedia_set(&sc->bge_mii.mii_media,
				    IFM_ETHER|IFM_AUTO);
	}

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->bge_timeout, bge_tick, sc);
	timeout_set(&sc->bge_rxtimeout, bge_rxtick, sc);
	return;

fail_5:
	bus_dmamap_unload(sc->bge_dmatag, sc->bge_ring_map);

fail_4:
	bus_dmamap_destroy(sc->bge_dmatag, sc->bge_ring_map);

fail_3:
	bus_dmamem_unmap(sc->bge_dmatag, kva,
	    sizeof(struct bge_ring_data));

fail_2:
	bus_dmamem_free(sc->bge_dmatag, &seg, rseg);

fail_1:
	bus_space_unmap(sc->bge_btag, sc->bge_bhandle, size);
}

int
bge_activate(struct device *self, int act)
{
	struct bge_softc *sc = (struct bge_softc *)self;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int rv = 0;

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		break;
	case DVACT_SUSPEND:
		rv = config_activate_children(self, act);
		if (ifp->if_flags & IFF_RUNNING)
			bge_stop(sc);
		break;
	case DVACT_RESUME:
		if (ifp->if_flags & IFF_UP)
			bge_init(sc);
		rv = config_activate_children(self, act);
		break;
	}
	return (rv);
}

void
bge_reset(struct bge_softc *sc)
{
	struct pci_attach_args *pa = &sc->bge_pa;
	pcireg_t cachesize, command, pcistate, new_pcistate;
	u_int32_t reset;
	int i, val = 0;

	/* Save some important PCI state. */
	cachesize = pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_CACHESZ);
	command = pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_CMD);
	pcistate = pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_PCISTATE);

	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS|BGE_PCIMISCCTL_MASK_PCI_INTR|
	    BGE_PCIMISCCTL_ENDIAN_WORDSWAP|BGE_PCIMISCCTL_PCISTATE_RW);

	/* Disable fastboot on controllers that support it. */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5752 ||
	    BGE_IS_5755_PLUS(sc))
		CSR_WRITE_4(sc, BGE_FASTBOOT_PC, 0);

	reset = BGE_MISCCFG_RESET_CORE_CLOCKS|(65<<1);

	if (sc->bge_flags & BGE_PCIE) {
		if (CSR_READ_4(sc, 0x7e2c) == 0x60) {
			/* PCI Express 1.0 system */
			CSR_WRITE_4(sc, 0x7e2c, 0x20);
		}
		if (sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
			/*
			 * Prevent PCI Express link training
			 * during global reset.
			 */
			CSR_WRITE_4(sc, BGE_MISC_CFG, (1<<29));
			reset |= (1<<29);
		}
	}

	/*
	 * Set GPHY Power Down Override to leave GPHY
	 * powered up in D0 uninitialized.
	 */
	if (BGE_IS_5705_PLUS(sc))
		reset |= BGE_MISCCFG_KEEP_GPHY_POWER;

	/* Issue global reset */
	bge_writereg_ind(sc, BGE_MISC_CFG, reset);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		u_int32_t status, ctrl;

		status = CSR_READ_4(sc, BGE_VCPU_STATUS);
		CSR_WRITE_4(sc, BGE_VCPU_STATUS,
		    status | BGE_VCPU_STATUS_DRV_RESET);
		ctrl = CSR_READ_4(sc, BGE_VCPU_EXT_CTRL);
		CSR_WRITE_4(sc, BGE_VCPU_EXT_CTRL,
		    ctrl & ~BGE_VCPU_EXT_CTRL_HALT_CPU);

		sc->bge_flags |= BGE_NO_EEPROM;
	}

	DELAY(1000);

	if (sc->bge_flags & BGE_PCIE) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5750_A0) {
			pcireg_t v;

			DELAY(500000); /* wait for link training to complete */
			v = pci_conf_read(pa->pa_pc, pa->pa_tag, 0xc4);
			pci_conf_write(pa->pa_pc, pa->pa_tag, 0xc4, v | (1<<15));
		}

		/*
		 * Set PCI Express max payload size to 128 bytes
		 * and clear error status.
		 */
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    BGE_PCI_CONF_DEV_CTRL, 0xf5000);
	}

	/* Reset some of the PCI state that got zapped by reset */
	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS|BGE_PCIMISCCTL_MASK_PCI_INTR|
	    BGE_PCIMISCCTL_ENDIAN_WORDSWAP|BGE_PCIMISCCTL_PCISTATE_RW);
	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_CACHESZ, cachesize);
	pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_CMD, command);
	bge_writereg_ind(sc, BGE_MISC_CFG, (65 << 1));

	/* Enable memory arbiter. */
	if (BGE_IS_5714_FAMILY(sc)) {
		u_int32_t val;

		val = CSR_READ_4(sc, BGE_MARB_MODE);
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE | val);
	} else
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);

 	/*
	 * Prevent PXE restart: write a magic number to the
	 * general communications memory at 0xB50.
	 */
	bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM, BGE_MAGIC_NUMBER);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		for (i = 0; i < BGE_TIMEOUT; i++) {
			val = CSR_READ_4(sc, BGE_VCPU_STATUS);
			if (val & BGE_VCPU_STATUS_INIT_DONE)
				break;
			DELAY(100);
		}

		if (i >= BGE_TIMEOUT)
			printf("%s: reset timed out\n", sc->bge_dev.dv_xname);
	} else {
		/*
		 * Poll until we see 1's complement of the magic number.
		 * This indicates that the firmware initialization
		 * is complete.  We expect this to fail if no SEEPROM
		 * is fitted.
		 */
		for (i = 0; i < BGE_TIMEOUT; i++) {
			val = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM);
			if (val == ~BGE_MAGIC_NUMBER)
				break;
			DELAY(10);
		}

		if (i >= BGE_TIMEOUT && (!(sc->bge_flags & BGE_NO_EEPROM)))
			printf("%s: firmware handshake timed out\n",
			   sc->bge_dev.dv_xname);
	}

	/*
	 * XXX Wait for the value of the PCISTATE register to
	 * return to its original pre-reset state. This is a
	 * fairly good indicator of reset completion. If we don't
	 * wait for the reset to fully complete, trying to read
	 * from the device's non-PCI registers may yield garbage
	 * results.
	 */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		new_pcistate = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    BGE_PCI_PCISTATE);
		if ((new_pcistate & ~BGE_PCISTATE_RESERVED) ==
		    (pcistate & ~BGE_PCISTATE_RESERVED))
			break;
		DELAY(10);
	}
	if ((new_pcistate & ~BGE_PCISTATE_RESERVED) != 
	    (pcistate & ~BGE_PCISTATE_RESERVED)) {
		DPRINTFN(5, ("%s: pcistate failed to revert\n",
		    sc->bge_dev.dv_xname));
	}

	/* Fix up byte swapping */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS);

	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * The 5704 in TBI mode apparently needs some special
	 * adjustment to insure the SERDES drive level is set
	 * to 1.2V.
	 */
	if (sc->bge_flags & BGE_PHY_FIBER_TBI &&
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
		u_int32_t serdescfg;

		serdescfg = CSR_READ_4(sc, BGE_SERDES_CFG);
		serdescfg = (serdescfg & ~0xFFF) | 0x880;
		CSR_WRITE_4(sc, BGE_SERDES_CFG, serdescfg);
	}

	if (sc->bge_flags & BGE_PCIE &&
	    sc->bge_chipid != BGE_CHIPID_BCM5750_A0 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5717 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5785 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM57765) {
		u_int32_t v;

		/* Enable PCI Express bug fix */
		v = CSR_READ_4(sc, 0x7c00);
		CSR_WRITE_4(sc, 0x7c00, v | (1<<25));
	}
	DELAY(10000);
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle two possibilities here:
 * 1) the frame is from the jumbo receive ring
 * 2) the frame is from the standard receive ring
 */

void
bge_rxeof(struct bge_softc *sc)
{
	struct ifnet *ifp;
	uint16_t rx_prod, rx_cons;
	int stdcnt = 0, jumbocnt = 0;
	bus_dmamap_t dmamap;
	bus_addr_t offset, toff;
	bus_size_t tlen;
	int tosync;

	rx_cons = sc->bge_rx_saved_considx;
	rx_prod = sc->bge_rdata->bge_status_block.bge_idx[0].bge_rx_prod_idx;

	/* Nothing to do */
	if (rx_cons == rx_prod)
		return;

	ifp = &sc->arpcom.ac_if;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_status_block),
	    sizeof (struct bge_status_block),
	    BUS_DMASYNC_POSTREAD);

	offset = offsetof(struct bge_ring_data, bge_rx_return_ring);
	tosync = rx_prod - rx_cons;

	toff = offset + (rx_cons * sizeof (struct bge_rx_bd));

	if (tosync < 0) {
		tlen = (sc->bge_return_ring_cnt - rx_cons) *
		    sizeof (struct bge_rx_bd);
		bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
		    toff, tlen, BUS_DMASYNC_POSTREAD);
		tosync = -tosync;
	}

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offset, tosync * sizeof (struct bge_rx_bd),
	    BUS_DMASYNC_POSTREAD);

	while (rx_cons != rx_prod) {
		struct bge_rx_bd	*cur_rx;
		u_int32_t		rxidx;
		struct mbuf		*m = NULL;

		cur_rx = &sc->bge_rdata->bge_rx_return_ring[rx_cons];

		rxidx = cur_rx->bge_idx;
		BGE_INC(rx_cons, sc->bge_return_ring_cnt);

		if (cur_rx->bge_flags & BGE_RXBDFLAG_JUMBO_RING) {
			m = sc->bge_cdata.bge_rx_jumbo_chain[rxidx];
			sc->bge_cdata.bge_rx_jumbo_chain[rxidx] = NULL;

			jumbocnt++;
			sc->bge_jumbo_cnt--;

			dmamap = sc->bge_cdata.bge_rx_jumbo_map[rxidx];
			bus_dmamap_sync(sc->bge_dmatag, dmamap, 0,
			    dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_dmatag, dmamap);

			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				m_freem(m);
				continue;
			}
		} else {
			m = sc->bge_cdata.bge_rx_std_chain[rxidx];
			sc->bge_cdata.bge_rx_std_chain[rxidx] = NULL;

			stdcnt++;
			sc->bge_std_cnt--;

			dmamap = sc->bge_cdata.bge_rx_std_map[rxidx];
			bus_dmamap_sync(sc->bge_dmatag, dmamap, 0,
			    dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_dmatag, dmamap);

			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				m_freem(m);
				continue;
			}
		}

		ifp->if_ipackets++;
#ifdef __STRICT_ALIGNMENT
		/*
		 * The i386 allows unaligned accesses, but for other
		 * platforms we must make sure the payload is aligned.
		 */
		if (sc->bge_flags & BGE_RX_ALIGNBUG) {
			bcopy(m->m_data, m->m_data + ETHER_ALIGN,
			    cur_rx->bge_len);
			m->m_data += ETHER_ALIGN;
		}
#endif
		m->m_pkthdr.len = m->m_len = cur_rx->bge_len - ETHER_CRC_LEN; 
		m->m_pkthdr.rcvif = ifp;

		/*
		 * 5700 B0 chips do not support checksumming correctly due
		 * to hardware bugs.
		 */
		if (sc->bge_chipid != BGE_CHIPID_BCM5700_B0) {
			if (cur_rx->bge_flags & BGE_RXBDFLAG_IP_CSUM) {
				if (cur_rx->bge_ip_csum == 0xFFFF)
					m->m_pkthdr.csum_flags |=
					    M_IPV4_CSUM_IN_OK;
				else
					m->m_pkthdr.csum_flags |=
					    M_IPV4_CSUM_IN_BAD;
			}
			if (cur_rx->bge_flags & BGE_RXBDFLAG_TCP_UDP_CSUM &&
			    m->m_pkthdr.len >= ETHER_MIN_NOPAD) {
				if (cur_rx->bge_tcp_udp_csum == 0xFFFF)
					m->m_pkthdr.csum_flags |=
					    M_TCP_CSUM_IN_OK|M_UDP_CSUM_IN_OK;
			}
		}

#if NVLAN > 0
		if (cur_rx->bge_flags & BGE_RXBDFLAG_VLAN_TAG) {
			m->m_pkthdr.ether_vtag = cur_rx->bge_vlan_tag;
			m->m_flags |= M_VLANTAG;
		}
#endif

#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet.
		 */
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		ether_input_mbuf(ifp, m);
	}

	sc->bge_rx_saved_considx = rx_cons;
	bge_writembx(sc, BGE_MBX_RX_CONS0_LO, sc->bge_rx_saved_considx);
	if (stdcnt)
		bge_fill_rx_ring_std(sc);
	if (jumbocnt)
		bge_fill_rx_ring_jumbo(sc);
}

void
bge_txeof(struct bge_softc *sc)
{
	struct bge_tx_bd *cur_tx = NULL;
	struct ifnet *ifp;
	struct txdmamap_pool_entry *dma;
	bus_addr_t offset, toff;
	bus_size_t tlen;
	int tosync;
	struct mbuf *m;

	/* Nothing to do */
	if (sc->bge_tx_saved_considx ==
	    sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx)
		return;

	ifp = &sc->arpcom.ac_if;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_status_block),
	    sizeof (struct bge_status_block),
	    BUS_DMASYNC_POSTREAD);

	offset = offsetof(struct bge_ring_data, bge_tx_ring);
	tosync = sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx -
	    sc->bge_tx_saved_considx;

	toff = offset + (sc->bge_tx_saved_considx * sizeof (struct bge_tx_bd));

	if (tosync < 0) {
		tlen = (BGE_TX_RING_CNT - sc->bge_tx_saved_considx) *
		    sizeof (struct bge_tx_bd);
		bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
		    toff, tlen, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		tosync = -tosync;
	}

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offset, tosync * sizeof (struct bge_tx_bd),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->bge_tx_saved_considx !=
	    sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx) {
		u_int32_t		idx = 0;

		idx = sc->bge_tx_saved_considx;
		cur_tx = &sc->bge_rdata->bge_tx_ring[idx];
		if (cur_tx->bge_flags & BGE_TXBDFLAG_END)
			ifp->if_opackets++;
		m = sc->bge_cdata.bge_tx_chain[idx];
		if (m != NULL) {
			sc->bge_cdata.bge_tx_chain[idx] = NULL;
			dma = sc->txdma[idx];
			bus_dmamap_sync(sc->bge_dmatag, dma->dmamap, 0,
			    dma->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bge_dmatag, dma->dmamap);
			SLIST_INSERT_HEAD(&sc->txdma_list, dma, link);
			sc->txdma[idx] = NULL;

			m_freem(m);
		}
		sc->bge_txcnt--;
		BGE_INC(sc->bge_tx_saved_considx, BGE_TX_RING_CNT);
	}

	if (sc->bge_txcnt < BGE_TX_RING_CNT - 16)
		ifp->if_flags &= ~IFF_OACTIVE;
	if (sc->bge_txcnt == 0)
		ifp->if_timer = 0;
}

int
bge_intr(void *xsc)
{
	struct bge_softc *sc;
	struct ifnet *ifp;
	u_int32_t statusword;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	/* It is possible for the interrupt to arrive before
	 * the status block is updated prior to the interrupt.
	 * Reading the PCI State register will confirm whether the
	 * interrupt is ours and will flush the status block.
	 */

	/* read status word from status block */
	statusword = sc->bge_rdata->bge_status_block.bge_status;

	if ((statusword & BGE_STATFLAG_UPDATED) ||
	    (!(CSR_READ_4(sc, BGE_PCI_PCISTATE) & BGE_PCISTATE_INTR_NOT_ACTIVE))) {

		/* Ack interrupt and stop others from occurring. */
		bge_writembx(sc, BGE_MBX_IRQ0_LO, 1);
			
		/* clear status word */
		sc->bge_rdata->bge_status_block.bge_status = 0;

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
		    statusword & BGE_STATFLAG_LINKSTATE_CHANGED ||
		    BGE_STS_BIT(sc, BGE_STS_LINK_EVT))
			bge_link_upd(sc);

		if (ifp->if_flags & IFF_RUNNING) {
			/* Check RX return ring producer/consumer */
			bge_rxeof(sc);

			/* Check TX ring producer/consumer */
			bge_txeof(sc);
		}

		/* Re-enable interrupts. */
		bge_writembx(sc, BGE_MBX_IRQ0_LO, 0);

		bge_start(ifp);

		return (1);
	} else
		return (0);
}

void
bge_tick(void *xsc)
{
	struct bge_softc *sc = xsc;
	struct mii_data *mii = &sc->bge_mii;
	int s;

	s = splnet();

	if (BGE_IS_5705_PLUS(sc))
		bge_stats_update_regs(sc);
	else
		bge_stats_update(sc);

	if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
		/*
		 * Since in TBI mode auto-polling can't be used we should poll
		 * link status manually. Here we register pending link event
		 * and trigger interrupt.
		 */
		BGE_STS_SETBIT(sc, BGE_STS_LINK_EVT);
		BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
	} else {
		/*
		 * Do not touch PHY if we have link up. This could break
		 * IPMI/ASF mode or produce extra input errors.
		 * (extra input errors was reported for bcm5701 & bcm5704).
		 */
		if (!BGE_STS_BIT(sc, BGE_STS_LINK))
			mii_tick(mii);
	}       

	timeout_add_sec(&sc->bge_timeout, 1);

	splx(s);
}

void
bge_stats_update_regs(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;

	ifp->if_collisions += CSR_READ_4(sc, BGE_MAC_STATS +
	    offsetof(struct bge_mac_stats_regs, etherStatsCollisions));

	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_DROPS);

	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_ERRORS);

	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_OUT_OF_BDS);
}

void
bge_stats_update(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	bus_size_t stats = BGE_MEMWIN_START + BGE_STATS_BLOCK;
	u_int32_t cnt;

#define READ_STAT(sc, stats, stat) \
	  CSR_READ_4(sc, stats + offsetof(struct bge_stats, stat))

	cnt = READ_STAT(sc, stats, txstats.etherStatsCollisions.bge_addr_lo);
	ifp->if_collisions += (u_int32_t)(cnt - sc->bge_tx_collisions);
	sc->bge_tx_collisions = cnt;

	cnt = READ_STAT(sc, stats, ifInDiscards.bge_addr_lo);
	ifp->if_ierrors += (u_int32_t)(cnt - sc->bge_rx_discards);
	sc->bge_rx_discards = cnt;

	cnt = READ_STAT(sc, stats, ifInErrors.bge_addr_lo);
	ifp->if_ierrors += (u_int32_t)(cnt - sc->bge_rx_inerrors);
	sc->bge_rx_inerrors = cnt;

	cnt = READ_STAT(sc, stats, nicNoMoreRxBDs.bge_addr_lo);
	ifp->if_ierrors += (u_int32_t)(cnt - sc->bge_rx_overruns);
	sc->bge_rx_overruns = cnt;

	cnt = READ_STAT(sc, stats, txstats.ifOutDiscards.bge_addr_lo);
	ifp->if_oerrors += (u_int32_t)(cnt - sc->bge_tx_discards);
	sc->bge_tx_discards = cnt;

#undef READ_STAT
}

/*
 * Compact outbound packets to avoid bug with DMA segments less than 8 bytes.
 */
int
bge_compact_dma_runt(struct mbuf *pkt)
{
	struct mbuf	*m, *prev, *n = NULL;
	int 		totlen, newprevlen;

	prev = NULL;
	totlen = 0;

	for (m = pkt; m != NULL; prev = m,m = m->m_next) {
		int mlen = m->m_len;
		int shortfall = 8 - mlen ;

		totlen += mlen;
		if (mlen == 0)
			continue;
		if (mlen >= 8)
			continue;

		/* If we get here, mbuf data is too small for DMA engine.
		 * Try to fix by shuffling data to prev or next in chain.
		 * If that fails, do a compacting deep-copy of the whole chain.
		 */

		/* Internal frag. If fits in prev, copy it there. */
		if (prev && M_TRAILINGSPACE(prev) >= m->m_len) {
			bcopy(m->m_data,
			      prev->m_data+prev->m_len,
			      mlen);
			prev->m_len += mlen;
			m->m_len = 0;
			/* XXX stitch chain */
			prev->m_next = m_free(m);
			m = prev;
			continue;
		} else if (m->m_next != NULL &&
			   M_TRAILINGSPACE(m) >= shortfall &&
			   m->m_next->m_len >= (8 + shortfall)) {
			/* m is writable and have enough data in next, pull up. */

			bcopy(m->m_next->m_data,
			      m->m_data+m->m_len,
			      shortfall);
			m->m_len += shortfall;
			m->m_next->m_len -= shortfall;
			m->m_next->m_data += shortfall;
		} else if (m->m_next == NULL || 1) {
			/* Got a runt at the very end of the packet.
			 * borrow data from the tail of the preceding mbuf and
			 * update its length in-place. (The original data is still
			 * valid, so we can do this even if prev is not writable.)
			 */

			/* if we'd make prev a runt, just move all of its data. */
#ifdef DEBUG
			KASSERT(prev != NULL /*, ("runt but null PREV")*/);
			KASSERT(prev->m_len >= 8 /*, ("runt prev")*/);
#endif
			if ((prev->m_len - shortfall) < 8)
				shortfall = prev->m_len;

			newprevlen = prev->m_len - shortfall;

			MGET(n, M_NOWAIT, MT_DATA);
			if (n == NULL)
				return (ENOBUFS);
			KASSERT(m->m_len + shortfall < MLEN
				/*,
				  ("runt %d +prev %d too big\n", m->m_len, shortfall)*/);

			/* first copy the data we're stealing from prev */
			bcopy(prev->m_data + newprevlen, n->m_data, shortfall);

			/* update prev->m_len accordingly */
			prev->m_len -= shortfall;

			/* copy data from runt m */
			bcopy(m->m_data, n->m_data + shortfall, m->m_len);

			/* n holds what we stole from prev, plus m */
			n->m_len = shortfall + m->m_len;

			/* stitch n into chain and free m */
			n->m_next = m->m_next;
			prev->m_next = n;
			/* KASSERT(m->m_next == NULL); */
			m->m_next = NULL;
			m_free(m);
			m = n;	/* for continuing loop */
		}
	}
	return (0);
}

/*
 * Pad outbound frame to ETHER_MIN_NOPAD for an unusual reason.
 * The bge hardware will pad out Tx runts to ETHER_MIN_NOPAD,
 * but when such padded frames employ the bge IP/TCP checksum offload,
 * the hardware checksum assist gives incorrect results (possibly
 * from incorporating its own padding into the UDP/TCP checksum; who knows).
 * If we pad such runts with zeros, the onboard checksum comes out correct.
 */
int
bge_cksum_pad(struct mbuf *m)
{
	int padlen = ETHER_MIN_NOPAD - m->m_pkthdr.len;
	struct mbuf *last;

	/* If there's only the packet-header and we can pad there, use it. */
	if (m->m_pkthdr.len == m->m_len && M_TRAILINGSPACE(m) >= padlen) {
		last = m;
	} else {
		/*
		 * Walk packet chain to find last mbuf. We will either
		 * pad there, or append a new mbuf and pad it.
		 */
		for (last = m; last->m_next != NULL; last = last->m_next);
		if (M_TRAILINGSPACE(last) < padlen) {
			/* Allocate new empty mbuf, pad it. Compact later. */
			struct mbuf *n;

			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == NULL)
				return (ENOBUFS);
			n->m_len = 0;
			last->m_next = n;
			last = n;
		}
	}
	
	/* Now zero the pad area, to avoid the bge cksum-assist bug. */
	memset(mtod(last, caddr_t) + last->m_len, 0, padlen);
	last->m_len += padlen;
	m->m_pkthdr.len += padlen;

	return (0);
}

/*
 * Encapsulate an mbuf chain in the tx ring by coupling the mbuf data
 * pointers to descriptors.
 */
int
bge_encap(struct bge_softc *sc, struct mbuf *m_head, u_int32_t *txidx)
{
	struct bge_tx_bd	*f = NULL;
	u_int32_t		frag, cur;
	u_int16_t		csum_flags = 0;
	struct txdmamap_pool_entry *dma;
	bus_dmamap_t dmamap;
	int			i = 0;

	cur = frag = *txidx;

	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT)
			csum_flags |= BGE_TXBDFLAG_IP_CSUM;
		if (m_head->m_pkthdr.csum_flags & (M_TCPV4_CSUM_OUT |
		    M_UDPV4_CSUM_OUT)) {
			csum_flags |= BGE_TXBDFLAG_TCP_UDP_CSUM;
			if (m_head->m_pkthdr.len < ETHER_MIN_NOPAD &&
			    bge_cksum_pad(m_head) != 0)
				return (ENOBUFS);
		}
	}

	if (!(BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX))
		goto doit;

	/*
	 * bcm5700 Revision B silicon cannot handle DMA descriptors with
	 * less than eight bytes.  If we encounter a teeny mbuf
	 * at the end of a chain, we can pad.  Otherwise, copy.
	 */
	if (bge_compact_dma_runt(m_head) != 0)
		return (ENOBUFS);

doit:
	dma = SLIST_FIRST(&sc->txdma_list);
	if (dma == NULL)
		return (ENOBUFS);
	dmamap = dma->dmamap;

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	if (bus_dmamap_load_mbuf(sc->bge_dmatag, dmamap, m_head,
	    BUS_DMA_NOWAIT))
		return (ENOBUFS);

	/* Check if we have enough free send BDs. */
	if (sc->bge_txcnt + dmamap->dm_nsegs >= BGE_TX_RING_CNT)
		goto fail_unload;

	for (i = 0; i < dmamap->dm_nsegs; i++) {
		f = &sc->bge_rdata->bge_tx_ring[frag];
		if (sc->bge_cdata.bge_tx_chain[frag] != NULL)
			break;
		BGE_HOSTADDR(f->bge_addr, dmamap->dm_segs[i].ds_addr);
		f->bge_len = dmamap->dm_segs[i].ds_len;
		f->bge_flags = csum_flags;
		f->bge_vlan_tag = 0;
#if NVLAN > 0
		if (m_head->m_flags & M_VLANTAG) {
			f->bge_flags |= BGE_TXBDFLAG_VLAN_TAG;
			f->bge_vlan_tag = m_head->m_pkthdr.ether_vtag;
		}
#endif
		cur = frag;
		BGE_INC(frag, BGE_TX_RING_CNT);
	}

	if (i < dmamap->dm_nsegs)
		goto fail_unload;

	bus_dmamap_sync(sc->bge_dmatag, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if (frag == sc->bge_tx_saved_considx)
		goto fail_unload;

	sc->bge_rdata->bge_tx_ring[cur].bge_flags |= BGE_TXBDFLAG_END;
	sc->bge_cdata.bge_tx_chain[cur] = m_head;
	SLIST_REMOVE_HEAD(&sc->txdma_list, link);
	sc->txdma[cur] = dma;
	sc->bge_txcnt += dmamap->dm_nsegs;

	*txidx = frag;

	return (0);

fail_unload:
	bus_dmamap_unload(sc->bge_dmatag, dmamap);

	return (ENOBUFS);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
void
bge_start(struct ifnet *ifp)
{
	struct bge_softc *sc;
	struct mbuf *m_head;
	u_int32_t prodidx;
	int pkts;

	sc = ifp->if_softc;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;
	if (!BGE_STS_BIT(sc, BGE_STS_LINK))
		return;

	prodidx = sc->bge_tx_prodidx;

	for (pkts = 0; !IFQ_IS_EMPTY(&ifp->if_snd);) {
		if (sc->bge_txcnt > BGE_TX_RING_CNT - 16) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (bge_encap(sc, m_head, &prodidx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		pkts++;

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
	}
	if (pkts == 0)
		return;

	/* Transmit */
	bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);

	sc->bge_tx_prodidx = prodidx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
bge_init(void *xsc)
{
	struct bge_softc *sc = xsc;
	struct ifnet *ifp;
	u_int16_t *m;
	u_int32_t rxmode;
	int s;

	s = splnet();

	ifp = &sc->arpcom.ac_if;

	/* Cancel pending I/O and flush buffers. */
	bge_stop(sc);
	bge_reset(sc);
	bge_chipinit(sc);

	/*
	 * Init the various state machines, ring
	 * control blocks and firmware.
	 */
	if (bge_blockinit(sc)) {
		printf("%s: initialization failure\n", sc->bge_dev.dv_xname);
		splx(s);
		return;
	}

	/* Specify MRU. */
	if (BGE_IS_JUMBO_CAPABLE(sc))
		CSR_WRITE_4(sc, BGE_RX_MTU,
			BGE_JUMBO_FRAMELEN + ETHER_VLAN_ENCAP_LEN);
	else
		CSR_WRITE_4(sc, BGE_RX_MTU,
			ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);

	/* Load our MAC address. */
	m = (u_int16_t *)&sc->arpcom.ac_enaddr[0];
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_LO, htons(m[0]));
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_HI, (htons(m[1]) << 16) | htons(m[2]));

	if (!(ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)) {
		/* Disable hardware decapsulation of VLAN frames. */
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_KEEP_VLAN_DIAG);
	}

	/* Program promiscuous mode and multicast filters. */
	bge_iff(sc);

	/* Init RX ring. */
	bge_init_rx_ring_std(sc);

	/*
	 * Workaround for a bug in 5705 ASIC rev A0. Poll the NIC's
	 * memory to insure that the chip has in fact read the first
	 * entry of the ring.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5705_A0) {
		u_int32_t		v, i;
		for (i = 0; i < 10; i++) {
			DELAY(20);
			v = bge_readmem_ind(sc, BGE_STD_RX_RINGS + 8);
			if (v == (MCLBYTES - ETHER_ALIGN))
				break;
		}
		if (i == 10)
			printf("%s: 5705 A0 chip failed to load RX ring\n",
			    sc->bge_dev.dv_xname);
	}

	/* Init Jumbo RX ring. */
	if (BGE_IS_JUMBO_CAPABLE(sc))
		bge_init_rx_ring_jumbo(sc);

	/* Init our RX return ring index */
	sc->bge_rx_saved_considx = 0;

	/* Init our RX/TX stat counters. */
	sc->bge_tx_collisions = 0;
	sc->bge_rx_discards = 0;
	sc->bge_rx_inerrors = 0;
	sc->bge_rx_overruns = 0;
	sc->bge_tx_discards = 0;

	/* Init TX ring. */
	bge_init_tx_ring(sc);

	/* Turn on transmitter */
	BGE_SETBIT(sc, BGE_TX_MODE, BGE_TXMODE_ENABLE);

	rxmode = BGE_RXMODE_ENABLE;

	if (BGE_IS_5755_PLUS(sc))
		rxmode |= BGE_RXMODE_RX_IPV6_CSUM_ENABLE;

	/* Turn on receiver */
	BGE_SETBIT(sc, BGE_RX_MODE, rxmode);

	CSR_WRITE_4(sc, BGE_MAX_RX_FRAME_LOWAT, 2);

	/* Tell firmware we're alive. */
	BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Enable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_CLEAR_INTA);
	BGE_CLRBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	bge_writembx(sc, BGE_MBX_IRQ0_LO, 0);

	bge_ifmedia_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	timeout_add_sec(&sc->bge_timeout, 1);
}

/*
 * Set media options.
 */
int
bge_ifmedia_upd(struct ifnet *ifp)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->bge_mii;
	struct ifmedia *ifm = &sc->bge_ifmedia;

	/* If this is a 1000baseX NIC, enable the TBI port. */
	if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return (EINVAL);
		switch(IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
			/*
			 * The BCM5704 ASIC appears to have a special
			 * mechanism for programming the autoneg
			 * advertisement registers in TBI mode.
			 */
			if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
				u_int32_t sgdig;
				sgdig = CSR_READ_4(sc, BGE_SGDIG_STS);
				if (sgdig & BGE_SGDIGSTS_DONE) {
					CSR_WRITE_4(sc, BGE_TX_TBI_AUTONEG, 0);
					sgdig = CSR_READ_4(sc, BGE_SGDIG_CFG);
					sgdig |= BGE_SGDIGCFG_AUTO |
					    BGE_SGDIGCFG_PAUSE_CAP |
					    BGE_SGDIGCFG_ASYM_PAUSE;
					CSR_WRITE_4(sc, BGE_SGDIG_CFG,
					    sgdig | BGE_SGDIGCFG_SEND);
					DELAY(5);
					CSR_WRITE_4(sc, BGE_SGDIG_CFG, sgdig);
				}
			}
			break;
		case IFM_1000_SX:
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
				BGE_CLRBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			} else {
				BGE_SETBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			}
			break;
		default:
			return (EINVAL);
		}
		/* XXX 802.3x flow control for 1000BASE-SX */
		return (0);
	}

	BGE_STS_SETBIT(sc, BGE_STS_LINK_EVT);
	if (mii->mii_instance) {
		struct mii_softc *miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	/*
	 * Force an interrupt so that we will call bge_link_upd
	 * if needed and clear any pending link state attention.
	 * Without this we are not getting any further interrupts
	 * for link state changes and thus will not UP the link and
	 * not be able to send in bge_start. The only way to get
	 * things working was to receive a packet and get a RX intr.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    sc->bge_flags & BGE_IS_5788)
		BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
	else
		BGE_SETBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_COAL_NOW);

	return (0);
}

/*
 * Report current media status.
 */
void
bge_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->bge_mii;

	if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;
		if (CSR_READ_4(sc, BGE_MAC_STS) &
		    BGE_MACSTAT_TBI_PCS_SYNCHED) {
			ifmr->ifm_status |= IFM_ACTIVE;
		} else {
			ifmr->ifm_active |= IFM_NONE;
			return;
		}
		ifmr->ifm_active |= IFM_1000_SX;
		if (CSR_READ_4(sc, BGE_MAC_MODE) & BGE_MACMODE_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
		else
			ifmr->ifm_active |= IFM_FDX;
		return;
	}

	mii_pollstat(mii);
	ifmr->ifm_status = mii->mii_media_status;
	ifmr->ifm_active = (mii->mii_media_active & ~IFM_ETH_FMASK) |
	    sc->bge_flowflags;
}

int
bge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct bge_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	int s, error = 0;
	struct mii_data *mii;

	s = splnet();

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			bge_init(sc);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->arpcom, ifa);
#endif /* INET */
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				bge_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				bge_stop(sc);
		}
		break;

	case SIOCSIFMEDIA:
		/* XXX Flow control is not supported for 1000BASE-SX */
		if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
			ifr->ifr_media &= ~IFM_ETH_FMASK;
			sc->bge_flowflags = 0;
		}

		/* Flow control requires full-duplex mode. */
		if (IFM_SUBTYPE(ifr->ifr_media) == IFM_AUTO ||
		    (ifr->ifr_media & IFM_FDX) == 0) {
		    	ifr->ifr_media &= ~IFM_ETH_FMASK;
		}
		if (IFM_SUBTYPE(ifr->ifr_media) != IFM_AUTO) {
			if ((ifr->ifr_media & IFM_ETH_FMASK) == IFM_FLOW) {
				/* We can do both TXPAUSE and RXPAUSE. */
				ifr->ifr_media |=
				    IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			}
			sc->bge_flowflags = ifr->ifr_media & IFM_ETH_FMASK;
		}
		/* FALLTHROUGH */
	case SIOCGIFMEDIA:
		if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
			error = ifmedia_ioctl(ifp, ifr, &sc->bge_ifmedia,
			    command);
		} else {
			mii = &sc->bge_mii;
			error = ifmedia_ioctl(ifp, ifr, &mii->mii_media,
			    command);
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->arpcom, command, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			bge_iff(sc);
		error = 0;
	}

	splx(s);
	return (error);
}

void
bge_watchdog(struct ifnet *ifp)
{
	struct bge_softc *sc;

	sc = ifp->if_softc;

	printf("%s: watchdog timeout -- resetting\n", sc->bge_dev.dv_xname);

	bge_init(sc);

	ifp->if_oerrors++;
}

void
bge_stop_block(struct bge_softc *sc, bus_size_t reg, u_int32_t bit)
{
	int i;

	BGE_CLRBIT(sc, reg, bit);

	for (i = 0; i < BGE_TIMEOUT; i++) {
		if ((CSR_READ_4(sc, reg) & bit) == 0)
			return;
		delay(100);
	}

	DPRINTFN(5, ("%s: block failed to stop: reg 0x%lx, bit 0x%08x\n",
	    sc->bge_dev.dv_xname, (u_long) reg, bit));
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
bge_stop(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct ifmedia_entry *ifm;
	struct mii_data *mii;
	int mtmp, itmp;

	timeout_del(&sc->bge_timeout);
	timeout_del(&sc->bge_rxtimeout);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/*
	 * Disable all of the receiver blocks
	 */
	bge_stop_block(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);
	bge_stop_block(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	bge_stop_block(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);
	if (BGE_IS_5700_FAMILY(sc))
		bge_stop_block(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);
	bge_stop_block(sc, BGE_RDBDI_MODE, BGE_RBDIMODE_ENABLE);
	bge_stop_block(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	bge_stop_block(sc, BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE);

	/*
	 * Disable all of the transmit blocks
	 */
	bge_stop_block(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	bge_stop_block(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	bge_stop_block(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	bge_stop_block(sc, BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE);
	bge_stop_block(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	if (BGE_IS_5700_FAMILY(sc))
		bge_stop_block(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);
	bge_stop_block(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/*
	 * Shut down all of the memory managers and related
	 * state machines.
	 */
	bge_stop_block(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);
	bge_stop_block(sc, BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE);
	if (BGE_IS_5700_FAMILY(sc))
		bge_stop_block(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	if (BGE_IS_5700_FAMILY(sc)) {
		bge_stop_block(sc, BGE_BMAN_MODE, BGE_BMANMODE_ENABLE);
		bge_stop_block(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);
	}

	/* Disable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	bge_writembx(sc, BGE_MBX_IRQ0_LO, 1);

	/*
	 * Tell firmware we're shutting down.
	 */
	BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Free the RX lists. */
	bge_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	if (BGE_IS_JUMBO_CAPABLE(sc))
		bge_free_rx_ring_jumbo(sc);

	/* Free TX buffers. */
	bge_free_tx_ring(sc);

	/*
	 * Isolate/power down the PHY, but leave the media selection
	 * unchanged so that things will be put back to normal when
	 * we bring the interface back up.
	 */
	if (!(sc->bge_flags & BGE_PHY_FIBER_TBI)) {
		mii = &sc->bge_mii;
		itmp = ifp->if_flags;
		ifp->if_flags |= IFF_UP;
		ifm = mii->mii_media.ifm_cur;
		mtmp = ifm->ifm_media;
		ifm->ifm_media = IFM_ETHER|IFM_NONE;
		mii_mediachg(mii);
		ifm->ifm_media = mtmp;
		ifp->if_flags = itmp;
	}

	sc->bge_tx_saved_considx = BGE_TXCONS_UNSET;

	/* Clear MAC's link state (PHY may still have link UP). */
	BGE_STS_CLRBIT(sc, BGE_STS_LINK);
}

void
bge_link_upd(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mii_data *mii = &sc->bge_mii;
	u_int32_t status;
	int link;

	/* Clear 'pending link event' flag */
	BGE_STS_CLRBIT(sc, BGE_STS_LINK_EVT);

	/*
	 * Process link state changes.
	 * Grrr. The link status word in the status block does
	 * not work correctly on the BCM5700 rev AX and BX chips,
	 * according to all available information. Hence, we have
	 * to enable MII interrupts in order to properly obtain
	 * async link changes. Unfortunately, this also means that
	 * we have to read the MAC status register to detect link
	 * changes, thereby adding an additional register access to
	 * the interrupt handler.
	 *
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_MI_INTERRUPT) {
			mii_pollstat(mii);

			if (!BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
				BGE_STS_SETBIT(sc, BGE_STS_LINK);
			else if (BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE))
				BGE_STS_CLRBIT(sc, BGE_STS_LINK);

			/* Clear the interrupt */
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
			bge_miibus_readreg(&sc->bge_dev, 1, BRGPHY_MII_ISR);
			bge_miibus_writereg(&sc->bge_dev, 1, BRGPHY_MII_IMR,
			    BRGPHY_INTRS);
		}
		return;
	} 

	if (sc->bge_flags & BGE_PHY_FIBER_TBI) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_TBI_PCS_SYNCHED) {
			if (!BGE_STS_BIT(sc, BGE_STS_LINK)) {
				BGE_STS_SETBIT(sc, BGE_STS_LINK);
				if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704)
					BGE_CLRBIT(sc, BGE_MAC_MODE,
					    BGE_MACMODE_TBI_SEND_CFGS);
				CSR_WRITE_4(sc, BGE_MAC_STS, 0xFFFFFFFF);
				status = CSR_READ_4(sc, BGE_MAC_MODE);
				ifp->if_link_state =
				    (status & BGE_MACMODE_HALF_DUPLEX) ?
				    LINK_STATE_HALF_DUPLEX :
				    LINK_STATE_FULL_DUPLEX;
				if_link_state_change(ifp);
				ifp->if_baudrate = IF_Gbps(1);
			}
		} else if (BGE_STS_BIT(sc, BGE_STS_LINK)) {
			BGE_STS_CLRBIT(sc, BGE_STS_LINK);
			ifp->if_link_state = LINK_STATE_DOWN;
			if_link_state_change(ifp);
			ifp->if_baudrate = 0;
		}
        /*
	 * Discard link events for MII/GMII cards if MI auto-polling disabled.
	 * This should not happen since mii callouts are locked now, but
	 * we keep this check for debug.
	 */
	} else if (BGE_STS_BIT(sc, BGE_STS_AUTOPOLL)) {
		/* 
		 * Some broken BCM chips have BGE_STATFLAG_LINKSTATE_CHANGED bit
		 * in status word always set. Workaround this bug by reading
		 * PHY link status directly.
		 */
		link = (CSR_READ_4(sc, BGE_MI_STS) & BGE_MISTS_LINK)?
		    BGE_STS_LINK : 0;

		if (BGE_STS_BIT(sc, BGE_STS_LINK) != link) {
			mii_pollstat(mii);

			if (!BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
				BGE_STS_SETBIT(sc, BGE_STS_LINK);
			else if (BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE))
				BGE_STS_CLRBIT(sc, BGE_STS_LINK);
		}
	}

	/* Clear the attention */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);
}
