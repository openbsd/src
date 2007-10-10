/*	$OpenBSD: if_vge.c,v 1.34 2007/10/10 12:46:44 kettenis Exp $	*/
/*	$FreeBSD: if_vge.c,v 1.3 2004/09/11 22:13:25 wpaul Exp $	*/
/*
 * Copyright (c) 2004
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
 */

/*
 * VIA Networking Technologies VT612x PCI gigabit ethernet NIC driver.
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 *
 * Ported to OpenBSD by Peter Valchev <pvalchev@openbsd.org>
 */

/*
 * The VIA Networking VT6122 is a 32bit, 33/66MHz PCI device that
 * combines a tri-speed ethernet MAC and PHY, with the following
 * features:
 *
 *	o Jumbo frame support up to 16K
 *	o Transmit and receive flow control
 *	o IPv4 checksum offload
 *	o VLAN tag insertion and stripping
 *	o TCP large send
 *	o 64-bit multicast hash table filter
 *	o 64 entry CAM filter
 *	o 16K RX FIFO and 48K TX FIFO memory
 *	o Interrupt moderation
 *
 * The VT6122 supports up to four transmit DMA queues. The descriptors
 * in the transmit ring can address up to 7 data fragments; frames which
 * span more than 7 data buffers must be coalesced, but in general the
 * BSD TCP/IP stack rarely generates frames more than 2 or 3 fragments
 * long. The receive descriptors address only a single buffer.
 *
 * There are two peculiar design issues with the VT6122. One is that
 * receive data buffers must be aligned on a 32-bit boundary. This is
 * not a problem where the VT6122 is used as a LOM device in x86-based
 * systems, but on architectures that generate unaligned access traps, we
 * have to do some copying.
 *
 * The other issue has to do with the way 64-bit addresses are handled.
 * The DMA descriptors only allow you to specify 48 bits of addressing
 * information. The remaining 16 bits are specified using one of the
 * I/O registers. If you only have a 32-bit system, then this isn't
 * an issue, but if you have a 64-bit system and more than 4GB of
 * memory, you must have to make sure your network data buffers reside
 * in the same 48-bit 'segment.'
 *
 * Special thanks to Ryan Fu at VIA Networking for providing documentation
 * and sample NICs for testing.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/endian.h>
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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_vgereg.h>
#include <dev/pci/if_vgevar.h>

int vge_probe		(struct device *, void *, void *);
void vge_attach		(struct device *, struct device *, void *);

int vge_encap		(struct vge_softc *, struct mbuf *, int);

int vge_allocmem		(struct vge_softc *);
int vge_newbuf		(struct vge_softc *, int, struct mbuf *);
int vge_rx_list_init	(struct vge_softc *);
int vge_tx_list_init	(struct vge_softc *);
void vge_rxeof		(struct vge_softc *);
void vge_txeof		(struct vge_softc *);
int vge_intr		(void *);
void vge_tick		(void *);
void vge_start		(struct ifnet *);
int vge_ioctl		(struct ifnet *, u_long, caddr_t);
int vge_init		(struct ifnet *);
void vge_stop		(struct vge_softc *);
void vge_watchdog	(struct ifnet *);
int vge_ifmedia_upd	(struct ifnet *);
void vge_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

#ifdef VGE_EEPROM
void vge_eeprom_getword	(struct vge_softc *, int, u_int16_t *);
#endif
void vge_read_eeprom	(struct vge_softc *, caddr_t, int, int, int);

void vge_miipoll_start	(struct vge_softc *);
void vge_miipoll_stop	(struct vge_softc *);
int vge_miibus_readreg	(struct device *, int, int);
void vge_miibus_writereg (struct device *, int, int, int);
void vge_miibus_statchg	(struct device *);

void vge_cam_clear	(struct vge_softc *);
int vge_cam_set		(struct vge_softc *, uint8_t *);
void vge_setmulti	(struct vge_softc *);
void vge_reset		(struct vge_softc *);

struct cfattach vge_ca = {
	sizeof(struct vge_softc), vge_probe, vge_attach
};

struct cfdriver vge_cd = {
	0, "vge", DV_IFNET
};

#define VGE_PCI_LOIO             0x10
#define VGE_PCI_LOMEM            0x14

int vge_debug = 0;
#define DPRINTF(x)	if (vge_debug) printf x
#define DPRINTFN(n, x)	if (vge_debug >= (n)) printf x

const struct pci_matchid vge_devices[] = {
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT612x },
};

#ifdef VGE_EEPROM
/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
void
vge_eeprom_getword(struct vge_softc *sc, int addr, u_int16_t *dest)
{
	int			i;
	u_int16_t		word = 0;

	/*
	 * Enter EEPROM embedded programming mode. In order to
	 * access the EEPROM at all, we first have to set the
	 * EELOAD bit in the CHIPCFG2 register.
	 */
	CSR_SETBIT_1(sc, VGE_CHIPCFG2, VGE_CHIPCFG2_EELOAD);
	CSR_SETBIT_1(sc, VGE_EECSR, VGE_EECSR_EMBP/*|VGE_EECSR_ECS*/);

	/* Select the address of the word we want to read */
	CSR_WRITE_1(sc, VGE_EEADDR, addr);

	/* Issue read command */
	CSR_SETBIT_1(sc, VGE_EECMD, VGE_EECMD_ERD);

	/* Wait for the done bit to be set. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		if (CSR_READ_1(sc, VGE_EECMD) & VGE_EECMD_EDONE)
			break;
	}

	if (i == VGE_TIMEOUT) {
		printf("%s: EEPROM read timed out\n", sc->vge_dev.dv_xname);
		*dest = 0;
		return;
	}

	/* Read the result */
	word = CSR_READ_2(sc, VGE_EERDDAT);

	/* Turn off EEPROM access mode. */
	CSR_CLRBIT_1(sc, VGE_EECSR, VGE_EECSR_EMBP/*|VGE_EECSR_ECS*/);
	CSR_CLRBIT_1(sc, VGE_CHIPCFG2, VGE_CHIPCFG2_EELOAD);

	*dest = word;
}
#endif

/*
 * Read a sequence of words from the EEPROM.
 */
void
vge_read_eeprom(struct vge_softc *sc, caddr_t dest, int off, int cnt,
    int swap)
{
	int			i;
#ifdef VGE_EEPROM
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		vge_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}
#else
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		dest[i] = CSR_READ_1(sc, VGE_PAR0 + i);
#endif
}

void
vge_miipoll_stop(struct vge_softc *sc)
{
	int			i;

	CSR_WRITE_1(sc, VGE_MIICMD, 0);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if (CSR_READ_1(sc, VGE_MIISTS) & VGE_MIISTS_IIDL)
			break;
	}

	if (i == VGE_TIMEOUT)
		printf("%s: failed to idle MII autopoll\n", sc->vge_dev.dv_xname);
}

void
vge_miipoll_start(struct vge_softc *sc)
{
	int			i;

	/* First, make sure we're idle. */

	CSR_WRITE_1(sc, VGE_MIICMD, 0);
	CSR_WRITE_1(sc, VGE_MIIADDR, VGE_MIIADDR_SWMPL);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if (CSR_READ_1(sc, VGE_MIISTS) & VGE_MIISTS_IIDL)
			break;
	}

	if (i == VGE_TIMEOUT) {
		printf("%s: failed to idle MII autopoll\n", sc->vge_dev.dv_xname);
		return;
	}

	/* Now enable auto poll mode. */

	CSR_WRITE_1(sc, VGE_MIICMD, VGE_MIICMD_MAUTO);

	/* And make sure it started. */

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_MIISTS) & VGE_MIISTS_IIDL) == 0)
			break;
	}

	if (i == VGE_TIMEOUT)
		printf("%s: failed to start MII autopoll\n", sc->vge_dev.dv_xname);
}

int
vge_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct vge_softc	*sc = (struct vge_softc *)dev;
	int			i, s;
	u_int16_t		rval = 0;

	if (phy != (CSR_READ_1(sc, VGE_MIICFG) & 0x1F))
		return(0);

	s = splnet();

	vge_miipoll_stop(sc);

	/* Specify the register we want to read. */
	CSR_WRITE_1(sc, VGE_MIIADDR, reg);

	/* Issue read command. */
	CSR_SETBIT_1(sc, VGE_MIICMD, VGE_MIICMD_RCMD);

	/* Wait for the read command bit to self-clear. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_MIICMD) & VGE_MIICMD_RCMD) == 0)
			break;
	}

	if (i == VGE_TIMEOUT)
		printf("%s: MII read timed out\n", sc->vge_dev.dv_xname);
	else
		rval = CSR_READ_2(sc, VGE_MIIDATA);

	vge_miipoll_start(sc);
	splx(s);

	return (rval);
}

void
vge_miibus_writereg(struct device *dev, int phy, int reg, int data)
{
	struct vge_softc	*sc = (struct vge_softc *)dev;
	int			i, s;

	if (phy != (CSR_READ_1(sc, VGE_MIICFG) & 0x1F))
		return;

	s = splnet();
	vge_miipoll_stop(sc);

	/* Specify the register we want to write. */
	CSR_WRITE_1(sc, VGE_MIIADDR, reg);

	/* Specify the data we want to write. */
	CSR_WRITE_2(sc, VGE_MIIDATA, data);

	/* Issue write command. */
	CSR_SETBIT_1(sc, VGE_MIICMD, VGE_MIICMD_WCMD);

	/* Wait for the write command bit to self-clear. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_MIICMD) & VGE_MIICMD_WCMD) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		printf("%s: MII write timed out\n", sc->vge_dev.dv_xname);
	}

	vge_miipoll_start(sc);
	splx(s);
}

void
vge_cam_clear(struct vge_softc *sc)
{
	int			i;

	/*
	 * Turn off all the mask bits. This tells the chip
	 * that none of the entries in the CAM filter are valid.
	 * desired entries will be enabled as we fill the filter in.
	 */

	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_CAMMASK);
	CSR_WRITE_1(sc, VGE_CAMADDR, VGE_CAMADDR_ENABLE);
	for (i = 0; i < 8; i++)
		CSR_WRITE_1(sc, VGE_CAM0 + i, 0);

	/* Clear the VLAN filter too. */

	CSR_WRITE_1(sc, VGE_CAMADDR, VGE_CAMADDR_ENABLE|VGE_CAMADDR_AVSEL|0);
	for (i = 0; i < 8; i++)
		CSR_WRITE_1(sc, VGE_CAM0 + i, 0);

	CSR_WRITE_1(sc, VGE_CAMADDR, 0);
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_MAR);

	sc->vge_camidx = 0;
}

int
vge_cam_set(struct vge_softc *sc, uint8_t *addr)
{
	int			i, error = 0;

	if (sc->vge_camidx == VGE_CAM_MAXADDRS)
		return(ENOSPC);

	/* Select the CAM data page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_CAMDATA);

	/* Set the filter entry we want to update and enable writing. */
	CSR_WRITE_1(sc, VGE_CAMADDR, VGE_CAMADDR_ENABLE|sc->vge_camidx);

	/* Write the address to the CAM registers */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		CSR_WRITE_1(sc, VGE_CAM0 + i, addr[i]);

	/* Issue a write command. */
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_WRITE);

	/* Wake for it to clear. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_CAMCTL) & VGE_CAMCTL_WRITE) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		printf("%s: setting CAM filter failed\n", sc->vge_dev.dv_xname);
		error = EIO;
		goto fail;
	}

	/* Select the CAM mask page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_CAMMASK);

	/* Set the mask bit that enables this filter. */
	CSR_SETBIT_1(sc, VGE_CAM0 + (sc->vge_camidx/8),
	    1<<(sc->vge_camidx & 7));

	sc->vge_camidx++;

fail:
	/* Turn off access to CAM. */
	CSR_WRITE_1(sc, VGE_CAMADDR, 0);
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_MAR);

	return (error);
}

/*
 * Program the multicast filter. We use the 64-entry CAM filter
 * for perfect filtering. If there's more than 64 multicast addresses,
 * we use the hash filter instead.
 */
void
vge_setmulti(struct vge_softc *sc)
{
	struct arpcom		*ac = &sc->arpcom;
	struct ifnet		*ifp = &ac->ac_if;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	int			error;
	u_int32_t		h = 0, hashes[2] = { 0, 0 };

	/* First, zot all the multicast entries. */
	vge_cam_clear(sc);
	CSR_WRITE_4(sc, VGE_MAR0, 0);
	CSR_WRITE_4(sc, VGE_MAR1, 0);
	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * If the user wants allmulti or promisc mode, enable reception
	 * of all multicast frames.
	 */
	if (ifp->if_flags & IFF_PROMISC) {
allmulti:
		CSR_WRITE_4(sc, VGE_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, VGE_MAR1, 0xFFFFFFFF);
		ifp->if_flags |= IFF_ALLMULTI;
		return;
	}

	/* Now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN))
			goto allmulti;

		error = vge_cam_set(sc, enm->enm_addrlo);
		if (error)
			break;

		ETHER_NEXT_MULTI(step, enm);
	}

	/* If there were too many addresses, use the hash filter. */
	if (error) {
		vge_cam_clear(sc);

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			h = ether_crc32_be(enm->enm_addrlo,
			    ETHER_ADDR_LEN) >> 26;
			hashes[h >> 5] |= 1 << (h & 0x1f);

			ETHER_NEXT_MULTI(step, enm);
		}

		CSR_WRITE_4(sc, VGE_MAR0, hashes[0]);
		CSR_WRITE_4(sc, VGE_MAR1, hashes[1]);
	}
}

void
vge_reset(struct vge_softc *sc)
{
	int			i;

	CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_SOFTRESET);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(5);
		if ((CSR_READ_1(sc, VGE_CRS1) & VGE_CR1_SOFTRESET) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		printf("%s: soft reset timed out", sc->vge_dev.dv_xname);
		CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_STOP_FORCE);
		DELAY(2000);
	}

	DELAY(5000);

	CSR_SETBIT_1(sc, VGE_EECSR, VGE_EECSR_RELOAD);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(5);
		if ((CSR_READ_1(sc, VGE_EECSR) & VGE_EECSR_RELOAD) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		printf("%s: EEPROM reload timed out\n", sc->vge_dev.dv_xname);
		return;
	}

	CSR_CLRBIT_1(sc, VGE_CHIPCFG0, VGE_CHIPCFG0_PACPI);
}

/*
 * Probe for a VIA gigabit chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int
vge_probe(struct device *dev, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, vge_devices,
	    sizeof(vge_devices)/sizeof(vge_devices[0])));
}

/*
 * Allocate memory for RX/TX rings
 */
int
vge_allocmem(struct vge_softc *sc)
{
	int			nseg, rseg;
	int			i, error;

	nseg = 32;

	/* Allocate DMA'able memory for the TX ring */

	error = bus_dmamap_create(sc->sc_dmat, VGE_TX_LIST_SZ, 1,
	    VGE_TX_LIST_SZ, 0, BUS_DMA_ALLOCNOW,
	    &sc->vge_ldata.vge_tx_list_map);
	if (error)
		return (ENOMEM);
	error = bus_dmamem_alloc(sc->sc_dmat, VGE_TX_LIST_SZ,
	    ETHER_ALIGN, 0,
	    &sc->vge_ldata.vge_tx_listseg, 1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't alloc TX list\n", sc->vge_dev.dv_xname);
		return (ENOMEM);
	}

	/* Load the map for the TX ring. */
	error = bus_dmamem_map(sc->sc_dmat, &sc->vge_ldata.vge_tx_listseg,
	     1, VGE_TX_LIST_SZ,
	     (caddr_t *)&sc->vge_ldata.vge_tx_list, BUS_DMA_NOWAIT);
	memset(sc->vge_ldata.vge_tx_list, 0, VGE_TX_LIST_SZ);
	if (error) {
		printf("%s: can't map TX dma buffers\n",
		    sc->vge_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat, &sc->vge_ldata.vge_tx_listseg, rseg);
		return (ENOMEM);
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->vge_ldata.vge_tx_list_map,
	    sc->vge_ldata.vge_tx_list, VGE_TX_LIST_SZ, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load TX dma map\n", sc->vge_dev.dv_xname);
		bus_dmamap_destroy(sc->sc_dmat, sc->vge_ldata.vge_tx_list_map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->vge_ldata.vge_tx_list,
		    VGE_TX_LIST_SZ);
		bus_dmamem_free(sc->sc_dmat, &sc->vge_ldata.vge_tx_listseg, rseg);
		return (ENOMEM);
	}

	/* Create DMA maps for TX buffers */

	for (i = 0; i < VGE_TX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES * nseg, nseg,
		    MCLBYTES, 0, BUS_DMA_ALLOCNOW,
		    &sc->vge_ldata.vge_tx_dmamap[i]);
		if (error) {
			printf("%s: can't create DMA map for TX\n",
			    sc->vge_dev.dv_xname);
			return (ENOMEM);
		}
	}

	/* Allocate DMA'able memory for the RX ring */

	error = bus_dmamap_create(sc->sc_dmat, VGE_RX_LIST_SZ, 1,
	    VGE_RX_LIST_SZ, 0, BUS_DMA_ALLOCNOW,
	    &sc->vge_ldata.vge_rx_list_map);
	if (error)
		return (ENOMEM);
	error = bus_dmamem_alloc(sc->sc_dmat, VGE_RX_LIST_SZ, VGE_RING_ALIGN,
	    0, &sc->vge_ldata.vge_rx_listseg, 1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't alloc RX list\n", sc->vge_dev.dv_xname);
		return (ENOMEM);
	}

	/* Load the map for the RX ring. */

	error = bus_dmamem_map(sc->sc_dmat, &sc->vge_ldata.vge_rx_listseg,
	     1, VGE_RX_LIST_SZ,
	     (caddr_t *)&sc->vge_ldata.vge_rx_list, BUS_DMA_NOWAIT);
	memset(sc->vge_ldata.vge_rx_list, 0, VGE_RX_LIST_SZ);
	if (error) {
		printf("%s: can't map RX dma buffers\n",
		    sc->vge_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat, &sc->vge_ldata.vge_rx_listseg, rseg);
		return (ENOMEM);
	}
	error = bus_dmamap_load(sc->sc_dmat, sc->vge_ldata.vge_rx_list_map,
	    sc->vge_ldata.vge_rx_list, VGE_RX_LIST_SZ, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load RX dma map\n", sc->vge_dev.dv_xname);
		bus_dmamap_destroy(sc->sc_dmat, sc->vge_ldata.vge_rx_list_map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->vge_ldata.vge_rx_list,
		    VGE_RX_LIST_SZ);
		bus_dmamem_free(sc->sc_dmat, &sc->vge_ldata.vge_rx_listseg, rseg);
		return (ENOMEM);
	}

	/* Create DMA maps for RX buffers */

	for (i = 0; i < VGE_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES * nseg, nseg,
		    MCLBYTES, 0, BUS_DMA_ALLOCNOW,
		    &sc->vge_ldata.vge_rx_dmamap[i]);
		if (error) {
			printf("%s: can't create DMA map for RX\n",
			    sc->vge_dev.dv_xname);
			return (ENOMEM);
		}
	}

	return (0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
vge_attach(struct device *parent, struct device *self, void *aux)
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int16_t		as[3];
	struct vge_softc	*sc = (struct vge_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	struct ifnet		*ifp;
	int			error = 0, i;
	bus_size_t		iosize;

	/*
	 * Map control/status registers.
	 */
	if (pci_mapreg_map(pa, VGE_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->vge_btag, &sc->vge_bhandle, NULL, &iosize, 0)) {
		if (pci_mapreg_map(pa, VGE_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
		    &sc->vge_btag, &sc->vge_bhandle, NULL, &iosize, 0)) {
			printf(": can't map mem or i/o space\n");
			return;
		}
	}

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->vge_intrhand = pci_intr_establish(pc, ih, IPL_NET, vge_intr, sc,
	    sc->vge_dev.dv_xname);
	if (sc->vge_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		return;
	}
	printf(": %s", intrstr);

	sc->sc_dmat = pa->pa_dmat;

	/* Reset the adapter. */
	vge_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	vge_read_eeprom(sc, (caddr_t)as, VGE_EE_EADDR, 3, 0);
	for (i = 0; i < 3; i++) {
		eaddr[(i * 2) + 0] = as[i] & 0xff;
		eaddr[(i * 2) + 1] = as[i] >> 8;
	}

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	printf(", address %s\n",
	    ether_sprintf(sc->arpcom.ac_enaddr));

	error = vge_allocmem(sc);

	if (error)
		return;

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vge_ioctl;
	ifp->if_start = vge_start;
	ifp->if_watchdog = vge_watchdog;
	ifp->if_init = vge_init;
	ifp->if_baudrate = 1000000000;
#ifdef VGE_JUMBO
	ifp->if_hardmtu = VGE_JUMBO_MTU;
#endif
	IFQ_SET_MAXLEN(&ifp->if_snd, VGE_IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU | IFCAP_CSUM_IPv4 |
				IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;

#ifdef VGE_VLAN
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	/* Set interface name */
	strlcpy(ifp->if_xname, sc->vge_dev.dv_xname, IFNAMSIZ);

	/* Do MII setup */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = vge_miibus_readreg;
	sc->sc_mii.mii_writereg = vge_miibus_writereg;
	sc->sc_mii.mii_statchg = vge_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0,
	    vge_ifmedia_upd, vge_ifmedia_sts);
	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->vge_dev.dv_xname);
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	timeout_set(&sc->timer_handle, vge_tick, sc);

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
}

int
vge_newbuf(struct vge_softc *sc, int idx, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;
	struct vge_rx_desc	*r;
	bus_dmamap_t		rxmap = sc->vge_ldata.vge_rx_dmamap[idx];
	int			i;

	if (m == NULL) {
		/* Allocate a new mbuf */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);

		/* Allocate a cluster */
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return (ENOBUFS);
		}

		m = m_new;
	} else
		m->m_data = m->m_ext.ext_buf;

	m->m_len = m->m_pkthdr.len = MCLBYTES;
	/* Fix-up alignment so payload is doubleword-aligned */
	/* XXX m_adj(m, ETHER_ALIGN); */

	if (bus_dmamap_load_mbuf(sc->sc_dmat, rxmap, m, BUS_DMA_NOWAIT))
		return (ENOBUFS);

	if (rxmap->dm_nsegs > 1)
		goto out;

	/* Map the segments into RX descriptors */
	r = &sc->vge_ldata.vge_rx_list[idx];

	if (letoh32(r->vge_sts) & VGE_RDSTS_OWN) {
		printf("%s: tried to map a busy RX descriptor\n",
		    sc->vge_dev.dv_xname);
		goto out;
	}
	r->vge_buflen = htole16(VGE_BUFLEN(rxmap->dm_segs[0].ds_len) | VGE_RXDESC_I);
	r->vge_addrlo = htole32(VGE_ADDR_LO(rxmap->dm_segs[0].ds_addr));
	r->vge_addrhi = htole16(VGE_ADDR_HI(rxmap->dm_segs[0].ds_addr) & 0xFFFF);
	r->vge_sts = htole32(0);
	r->vge_ctl = htole32(0);

	/*
	 * Note: the manual fails to document the fact that for
	 * proper operation, the driver needs to replenish the RX
	 * DMA ring 4 descriptors at a time (rather than one at a
	 * time, like most chips). We can allocate the new buffers
	 * but we should not set the OWN bits until we're ready
	 * to hand back 4 of them in one shot.
	 */
#define VGE_RXCHUNK 4
	sc->vge_rx_consumed++;
	if (sc->vge_rx_consumed == VGE_RXCHUNK) {
		for (i = idx; i != idx - sc->vge_rx_consumed; i--)
			sc->vge_ldata.vge_rx_list[i].vge_sts |=
			    htole32(VGE_RDSTS_OWN);
		sc->vge_rx_consumed = 0;
	}

	sc->vge_ldata.vge_rx_mbuf[idx] = m;

	bus_dmamap_sync(sc->sc_dmat, rxmap, 0,
	    rxmap->dm_mapsize, BUS_DMASYNC_PREREAD);

	return (0);
out:
	DPRINTF(("vge_newbuf: out of memory\n"));
	if (m_new != NULL)
		m_freem(m_new);
	return (ENOMEM);
}

int
vge_tx_list_init(struct vge_softc *sc)
{
	bzero ((char *)sc->vge_ldata.vge_tx_list, VGE_TX_LIST_SZ);
	bzero ((char *)&sc->vge_ldata.vge_tx_mbuf,
	    (VGE_TX_DESC_CNT * sizeof(struct mbuf *)));

	bus_dmamap_sync(sc->sc_dmat,
	    sc->vge_ldata.vge_tx_list_map, 0,
	    sc->vge_ldata.vge_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	sc->vge_ldata.vge_tx_prodidx = 0;
	sc->vge_ldata.vge_tx_considx = 0;
	sc->vge_ldata.vge_tx_free = VGE_TX_DESC_CNT;

	return (0);
}

/* Init RX descriptors and allocate mbufs with vge_newbuf()
 * A ring is used, and last descriptor points to first. */
int
vge_rx_list_init(struct vge_softc *sc)
{
	int			i;

	bzero ((char *)sc->vge_ldata.vge_rx_list, VGE_RX_LIST_SZ);
	bzero ((char *)&sc->vge_ldata.vge_rx_mbuf,
	    (VGE_RX_DESC_CNT * sizeof(struct mbuf *)));

	sc->vge_rx_consumed = 0;

	for (i = 0; i < VGE_RX_DESC_CNT; i++) {
		if (vge_newbuf(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	}

	/* Flush the RX descriptors */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->vge_ldata.vge_rx_list_map,
	    0, sc->vge_ldata.vge_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->vge_ldata.vge_rx_prodidx = 0;
	sc->vge_rx_consumed = 0;
	sc->vge_head = sc->vge_tail = NULL;

	return (0);
}

/*
 * RX handler. We support the reception of jumbo frames that have
 * been fragmented across multiple 2K mbuf cluster buffers.
 */
void
vge_rxeof(struct vge_softc *sc)
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	int			lim = 0;
	struct vge_rx_desc	*cur_rx;
	u_int32_t		rxstat, rxctl;

	ifp = &sc->arpcom.ac_if;
	i = sc->vge_ldata.vge_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->vge_ldata.vge_rx_list_map,
	    0, sc->vge_ldata.vge_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	while (!VGE_OWN(&sc->vge_ldata.vge_rx_list[i])) {
		struct mbuf *m0 = NULL;

		cur_rx = &sc->vge_ldata.vge_rx_list[i];
		m = sc->vge_ldata.vge_rx_mbuf[i];
		total_len = VGE_RXBYTES(cur_rx);
		rxstat = letoh32(cur_rx->vge_sts);
		rxctl = letoh32(cur_rx->vge_ctl);

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->sc_dmat,
		    sc->vge_ldata.vge_rx_dmamap[i],
		    0, sc->vge_ldata.vge_rx_dmamap[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat,
		    sc->vge_ldata.vge_rx_dmamap[i]);

		/*
		 * If the 'start of frame' bit is set, this indicates
		 * either the first fragment in a multi-fragment receive,
		 * or an intermediate fragment. Either way, we want to
		 * accumulate the buffers.
		 */
		if (rxstat & VGE_RXPKT_SOF) {
			DPRINTF(("vge_rxeof: SOF\n"));
			m->m_len = MCLBYTES;
			if (sc->vge_head == NULL)
				sc->vge_head = sc->vge_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->vge_tail->m_next = m;
				sc->vge_tail = m;
			}
			vge_newbuf(sc, i, NULL);
			VGE_RX_DESC_INC(i);
			continue;
		}

		/*
		 * Bad/error frames will have the RXOK bit cleared.
		 * However, there's one error case we want to allow:
		 * if a VLAN tagged frame arrives and the chip can't
		 * match it against the CAM filter, it considers this
		 * a 'VLAN CAM filter miss' and clears the 'RXOK' bit.
		 * We don't want to drop the frame though: our VLAN
		 * filtering is done in software.
		 */
		if (!(rxstat & VGE_RDSTS_RXOK) && !(rxstat & VGE_RDSTS_VIDM)
		    && !(rxstat & VGE_RDSTS_CSUMERR)) {
			ifp->if_ierrors++;
			/*
			 * If this is part of a multi-fragment packet,
			 * discard all the pieces.
			 */
			if (sc->vge_head != NULL) {
				m_freem(sc->vge_head);
				sc->vge_head = sc->vge_tail = NULL;
			}
			vge_newbuf(sc, i, m);
			VGE_RX_DESC_INC(i);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (vge_newbuf(sc, i, NULL) == ENOBUFS) {
			if (sc->vge_head != NULL) {
				m_freem(sc->vge_head);
				sc->vge_head = sc->vge_tail = NULL;
			}

			m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
			    total_len - ETHER_CRC_LEN + ETHER_ALIGN,
			    0, ifp, NULL);
			vge_newbuf(sc, i, m);
			if (m0 == NULL) {
				ifp->if_ierrors++;
				continue;
			}
			m_adj(m0, ETHER_ALIGN);
			m = m0;

			VGE_RX_DESC_INC(i);
			continue;
		}

		VGE_RX_DESC_INC(i);

		if (sc->vge_head != NULL) {
			m->m_len = total_len % MCLBYTES;
			/*
			 * Special case: if there's 4 bytes or less
			 * in this buffer, the mbuf can be discarded:
			 * the last 4 bytes is the CRC, which we don't
			 * care about anyway.
			 */
			if (m->m_len <= ETHER_CRC_LEN) {
				sc->vge_tail->m_len -=
				    (ETHER_CRC_LEN - m->m_len);
				m_freem(m);
			} else {
				m->m_len -= ETHER_CRC_LEN;
				m->m_flags &= ~M_PKTHDR;
				sc->vge_tail->m_next = m;
			}
			m = sc->vge_head;
			sc->vge_head = sc->vge_tail = NULL;
			m->m_pkthdr.len = total_len - ETHER_CRC_LEN;
		} else
			m->m_pkthdr.len = m->m_len =
			    (total_len - ETHER_CRC_LEN);

#ifdef __STRICT_ALIGNMENT
		bcopy(m->m_data, m->m_data + ETHER_ALIGN,
		    total_len);
		m->m_data += ETHER_ALIGN;
#endif
		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do RX checksumming */

		/* Check IP header checksum */
		if ((rxctl & VGE_RDCTL_IPPKT) &&
		    (rxctl & VGE_RDCTL_IPCSUMOK))
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_IN_OK;

		/* Check TCP/UDP checksum */
		if ((rxctl & (VGE_RDCTL_TCPPKT|VGE_RDCTL_UDPPKT)) &&
		    (rxctl & VGE_RDCTL_PROTOCSUMOK))
			m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
		ether_input_mbuf(ifp, m);

		lim++;
		if (lim == VGE_RX_DESC_CNT)
			break;
	}

	/* Flush the RX DMA ring */
	bus_dmamap_sync(sc->sc_dmat,
	    sc->vge_ldata.vge_rx_list_map,
	    0, sc->vge_ldata.vge_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->vge_ldata.vge_rx_prodidx = i;
	CSR_WRITE_2(sc, VGE_RXDESC_RESIDUECNT, lim);
}

void
vge_txeof(struct vge_softc *sc)
{
	struct ifnet		*ifp;
	u_int32_t		txstat;
	int			idx;

	ifp = &sc->arpcom.ac_if;
	idx = sc->vge_ldata.vge_tx_considx;

	/* Invalidate the TX descriptor list */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->vge_ldata.vge_tx_list_map,
	    0, sc->vge_ldata.vge_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	/* Transmitted frames can be now free'd from the TX list */
	while (idx != sc->vge_ldata.vge_tx_prodidx) {
		txstat = letoh32(sc->vge_ldata.vge_tx_list[idx].vge_sts);
		if (txstat & VGE_TDSTS_OWN)
			break;

		m_freem(sc->vge_ldata.vge_tx_mbuf[idx]);
		sc->vge_ldata.vge_tx_mbuf[idx] = NULL;
		bus_dmamap_unload(sc->sc_dmat,
		    sc->vge_ldata.vge_tx_dmamap[idx]);
		if (txstat & (VGE_TDSTS_EXCESSCOLL|VGE_TDSTS_COLL))
			ifp->if_collisions++;
		if (txstat & VGE_TDSTS_TXERR)
			ifp->if_oerrors++;
		else
			ifp->if_opackets++;

		sc->vge_ldata.vge_tx_free++;
		VGE_TX_DESC_INC(idx);
	}

	/* No changes made to the TX ring, so no flush needed */

	if (idx != sc->vge_ldata.vge_tx_considx) {
		sc->vge_ldata.vge_tx_considx = idx;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * If not all descriptors have been released reaped yet,
	 * reload the timer so that we will eventually get another
	 * interrupt that will cause us to re-enter this routine.
	 * This is done in case the transmitter has gone idle.
	 */
	if (sc->vge_ldata.vge_tx_free != VGE_TX_DESC_CNT)
		CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_TIMER0_ENABLE);
}

void
vge_tick(void *xsc)
{
	struct vge_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii = &sc->sc_mii;
	int s;

	s = splnet();

	mii_tick(mii);

	if (sc->vge_link) {
		if (!(mii->mii_media_status & IFM_ACTIVE)) {
			sc->vge_link = 0;
			ifp->if_link_state = LINK_STATE_DOWN;
			if_link_state_change(ifp);
		}
	} else {
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->vge_link = 1;
			if (mii->mii_media_status & IFM_FDX)
				ifp->if_link_state = LINK_STATE_FULL_DUPLEX;
			else if (mii->mii_media_status & IFM_HDX)
				ifp->if_link_state = LINK_STATE_HALF_DUPLEX;
			else
				ifp->if_link_state = LINK_STATE_UP;
			if_link_state_change(ifp);
			if (!IFQ_IS_EMPTY(&ifp->if_snd))
				vge_start(ifp);
		}
	}
	timeout_add(&sc->timer_handle, hz);
	splx(s);
}

int
vge_intr(void *arg)
{
	struct vge_softc	*sc = arg;
	struct ifnet		*ifp;
	u_int32_t		status;
	int			claimed = 0;

	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return 0;

	/* Disable interrupts */
	CSR_WRITE_1(sc, VGE_CRC3, VGE_CR3_INT_GMSK);

	for (;;) {
		status = CSR_READ_4(sc, VGE_ISR);
		DPRINTFN(3, ("vge_intr: status=%#x\n", status));

		/* If the card has gone away the read returns 0xffffffff. */
		if (status == 0xFFFFFFFF)
			break;

		if (status) {
			CSR_WRITE_4(sc, VGE_ISR, status);
		}

		if ((status & VGE_INTRS) == 0)
			break;

		claimed = 1;

		if (status & (VGE_ISR_RXOK|VGE_ISR_RXOK_HIPRIO))
			vge_rxeof(sc);

		if (status & (VGE_ISR_RXOFLOW|VGE_ISR_RXNODESC)) {
			DPRINTFN(2, ("vge_intr: RX error, recovering\n"));
			vge_rxeof(sc);
			CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_RUN);
			CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_WAK);
		}

		if (status & (VGE_ISR_TXOK0|VGE_ISR_TIMER0))
			vge_txeof(sc);

		if (status & (VGE_ISR_TXDMA_STALL|VGE_ISR_RXDMA_STALL)) {
			DPRINTFN(2, ("DMA_STALL\n"));
			vge_init(ifp);
		}

		if (status & VGE_ISR_LINKSTS) {
			timeout_del(&sc->timer_handle);
			vge_tick(sc);
		}
	}

	/* Re-enable interrupts */
	CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_GMSK);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		vge_start(ifp);

	return (claimed);
}

/*
 * Encapsulate an mbuf chain into the TX ring by combining it w/
 * the descriptors.
 */
int
vge_encap(struct vge_softc *sc, struct mbuf *m_head, int idx)
{
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	bus_dmamap_t		txmap;
	struct vge_tx_desc	*d = NULL;
	struct vge_tx_frag	*f;
	struct mbuf		*mnew = NULL;
	int			error, frag;
	u_int32_t		vge_flags;
#if NVLAN > 0
	struct ifvlan		*ifv = NULL;

	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

	vge_flags = 0;

	if (m_head->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT)
		vge_flags |= VGE_TDCTL_IPCSUM;
	if (m_head->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
		vge_flags |= VGE_TDCTL_TCPCSUM;
	if (m_head->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT)
		vge_flags |= VGE_TDCTL_UDPCSUM;

	txmap = sc->vge_ldata.vge_tx_dmamap[idx];
repack:
	error = bus_dmamap_load_mbuf(sc->sc_dmat, txmap,
	    m_head, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't map mbuf (error %d)\n",
		    sc->vge_dev.dv_xname, error);
		return (ENOBUFS);
	}

	d = &sc->vge_ldata.vge_tx_list[idx];
	/* If owned by chip, fail */
	if (letoh32(d->vge_sts) & VGE_TDSTS_OWN)
		return (ENOBUFS);

	for (frag = 0; frag < txmap->dm_nsegs; frag++) {
		/* Check if we have used all 7 fragments. */
		if (frag == VGE_TX_FRAGS)
			break;
		f = &d->vge_frag[frag];
		f->vge_buflen = htole16(VGE_BUFLEN(txmap->dm_segs[frag].ds_len));
		f->vge_addrlo = htole32(VGE_ADDR_LO(txmap->dm_segs[frag].ds_addr));
		f->vge_addrhi = htole16(VGE_ADDR_HI(txmap->dm_segs[frag].ds_addr) & 0xFFFF);
	}

	/*
	 * We used up all 7 fragments!  Now what we have to do is
	 * copy the data into a mbuf cluster and map that.
	 */
	if (frag == VGE_TX_FRAGS) {
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL)
			return (ENOBUFS);

		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(mnew, M_DONTWAIT);
			if (!(mnew->m_flags & M_EXT)) {
				m_freem(mnew);
				return (ENOBUFS);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,
		    mtod(mnew, caddr_t));
		mnew->m_pkthdr.len = mnew->m_len = m_head->m_pkthdr.len;
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		m_freem(m_head);
		m_head = mnew;
		goto repack;
	}

	/* This chip does not do auto-padding */
	if (m_head->m_pkthdr.len < VGE_MIN_FRAMELEN) {
		f = &d->vge_frag[frag];

		f->vge_buflen = htole16(VGE_BUFLEN(VGE_MIN_FRAMELEN -
		    m_head->m_pkthdr.len));
		f->vge_addrlo = htole32(VGE_ADDR_LO(txmap->dm_segs[0].ds_addr));
		f->vge_addrhi = htole16(VGE_ADDR_HI(txmap->dm_segs[0].ds_addr) & 0xFFFF);
		m_head->m_pkthdr.len = VGE_MIN_FRAMELEN;
		frag++;
	}
	/* For some reason, we need to tell the card fragment + 1 */
	frag++;

	bus_dmamap_sync(sc->sc_dmat, txmap, 0, txmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	d->vge_sts = htole32(m_head->m_pkthdr.len << 16);
	d->vge_ctl = htole32(vge_flags|(frag << 28) | VGE_TD_LS_NORM);

	if (m_head->m_pkthdr.len > ETHERMTU + ETHER_HDR_LEN)
		d->vge_ctl |= htole32(VGE_TDCTL_JUMBO);

	sc->vge_ldata.vge_tx_dmamap[idx] = txmap;
	sc->vge_ldata.vge_tx_mbuf[idx] = m_head;
	sc->vge_ldata.vge_tx_free--;
	sc->vge_ldata.vge_tx_list[idx].vge_sts |= htole32(VGE_TDSTS_OWN);

	/*
	 * Set up hardware VLAN tagging.
	 */
#if NVLAN > 0
	if (ifv != NULL) {
		sc->vge_ldata.vge_tx_list[idx].vge_ctl |=
		    htole32(htons(ifv->ifv_tag) | VGE_TDCTL_VTAG);
	}
#endif

	idx++;
	if (mnew == NULL) {
		/* if mbuf is coalesced, it is already dequeued */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
	}
	return (0);
}

/*
 * Main transmit routine.
 */
void
vge_start(struct ifnet *ifp)
{
	struct vge_softc	*sc;
	struct mbuf		*m_head = NULL;
	int			idx, pidx = 0;

	sc = ifp->if_softc;

	if (!sc->vge_link || ifp->if_flags & IFF_OACTIVE)
		return;

	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	idx = sc->vge_ldata.vge_tx_prodidx;

	pidx = idx - 1;
	if (pidx < 0)
		pidx = VGE_TX_DESC_CNT - 1;

	while (sc->vge_ldata.vge_tx_mbuf[idx] == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif

		if (vge_encap(sc, m_head, idx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		sc->vge_ldata.vge_tx_list[pidx].vge_frag[0].vge_buflen |=
		    htole16(VGE_TXDESC_Q);

		pidx = idx;
		VGE_TX_DESC_INC(idx);
	}

	if (idx == sc->vge_ldata.vge_tx_prodidx) {
		return;
	}

	/* Flush the TX descriptors */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->vge_ldata.vge_tx_list_map,
	    0, sc->vge_ldata.vge_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	/* Issue a transmit command. */
	CSR_WRITE_2(sc, VGE_TXQCSRS, VGE_TXQCSR_WAK0);

	sc->vge_ldata.vge_tx_prodidx = idx;

	/*
	 * Use the countdown timer for interrupt moderation.
	 * 'TX done' interrupts are disabled. Instead, we reset the
	 * countdown timer, which will begin counting until it hits
	 * the value in the SSTIMER register, and then trigger an
	 * interrupt. Each time we set the TIMER0_ENABLE bit, the
	 * the timer count is reloaded. Only when the transmitter
	 * is idle will the timer hit 0 and an interrupt fire.
	 */
	CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_TIMER0_ENABLE);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

int
vge_init(struct ifnet *ifp)
{
	struct vge_softc	*sc = ifp->if_softc;
	int			i;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	vge_stop(sc);
	vge_reset(sc);

	/* Initialize RX descriptors list */
	if (vge_rx_list_init(sc) == ENOBUFS) {
		printf("%s: init failed: no memory for RX buffers\n",
		    sc->vge_dev.dv_xname);
		vge_stop(sc);
		return (ENOBUFS);
	}
	/* Initialize TX descriptors */
	if (vge_tx_list_init(sc) == ENOBUFS) {
		printf("%s: init failed: no memory for TX buffers\n",
		    sc->vge_dev.dv_xname);
		vge_stop(sc);
		return (ENOBUFS);
	}

	/* Set our station address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		CSR_WRITE_1(sc, VGE_PAR0 + i, sc->arpcom.ac_enaddr[i]);

	/*
	 * Set receive FIFO threshold. Also allow transmission and
	 * reception of VLAN tagged frames.
	 */
	CSR_CLRBIT_1(sc, VGE_RXCFG, VGE_RXCFG_FIFO_THR|VGE_RXCFG_VTAGOPT);
	CSR_SETBIT_1(sc, VGE_RXCFG, VGE_RXFIFOTHR_128BYTES|VGE_VTAG_OPT2);

	/* Set DMA burst length */
	CSR_CLRBIT_1(sc, VGE_DMACFG0, VGE_DMACFG0_BURSTLEN);
	CSR_SETBIT_1(sc, VGE_DMACFG0, VGE_DMABURST_128);

	CSR_SETBIT_1(sc, VGE_TXCFG, VGE_TXCFG_ARB_PRIO|VGE_TXCFG_NONBLK);

	/* Set collision backoff algorithm */
	CSR_CLRBIT_1(sc, VGE_CHIPCFG1, VGE_CHIPCFG1_CRANDOM|
	    VGE_CHIPCFG1_CAP|VGE_CHIPCFG1_MBA|VGE_CHIPCFG1_BAKOPT);
	CSR_SETBIT_1(sc, VGE_CHIPCFG1, VGE_CHIPCFG1_OFSET);

	/* Disable LPSEL field in priority resolution */
	CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_LPSEL_DIS);

	/*
	 * Load the addresses of the DMA queues into the chip.
	 * Note that we only use one transmit queue.
	 */

	CSR_WRITE_4(sc, VGE_TXDESC_ADDR_LO0,
	    VGE_ADDR_LO(sc->vge_ldata.vge_tx_listseg.ds_addr));
	CSR_WRITE_2(sc, VGE_TXDESCNUM, VGE_TX_DESC_CNT - 1);

	CSR_WRITE_4(sc, VGE_RXDESC_ADDR_LO,
	    VGE_ADDR_LO(sc->vge_ldata.vge_rx_listseg.ds_addr));
	CSR_WRITE_2(sc, VGE_RXDESCNUM, VGE_RX_DESC_CNT - 1);
	CSR_WRITE_2(sc, VGE_RXDESC_RESIDUECNT, VGE_RX_DESC_CNT);

	/* Enable and wake up the RX descriptor queue */
	CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_RUN);
	CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_WAK);

	/* Enable the TX descriptor queue */
	CSR_WRITE_2(sc, VGE_TXQCSRS, VGE_TXQCSR_RUN0);

	/* Set up the receive filter -- allow large frames for VLANs. */
	CSR_WRITE_1(sc, VGE_RXCTL, VGE_RXCTL_RX_UCAST|VGE_RXCTL_RX_GIANT);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		CSR_SETBIT_1(sc, VGE_RXCTL, VGE_RXCTL_RX_PROMISC);
	}

	/* Set capture broadcast bit to capture broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST) {
		CSR_SETBIT_1(sc, VGE_RXCTL, VGE_RXCTL_RX_BCAST);
	}

	/* Set multicast bit to capture multicast frames. */
	if (ifp->if_flags & IFF_MULTICAST) {
		CSR_SETBIT_1(sc, VGE_RXCTL, VGE_RXCTL_RX_MCAST);
	}

	/* Init the cam filter. */
	vge_cam_clear(sc);

	/* Init the multicast filter. */
	vge_setmulti(sc);

	/* Enable flow control */

	CSR_WRITE_1(sc, VGE_CRS2, 0x8B);

	/* Enable jumbo frame reception (if desired) */

	/* Start the MAC. */
	CSR_WRITE_1(sc, VGE_CRC0, VGE_CR0_STOP);
	CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_NOPOLL);
	CSR_WRITE_1(sc, VGE_CRS0,
	    VGE_CR0_TX_ENABLE|VGE_CR0_RX_ENABLE|VGE_CR0_START);

	/*
	 * Configure one-shot timer for microsecond
	 * resulution and load it for 500 usecs.
	 */
	CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_TIMER0_RES);
	CSR_WRITE_2(sc, VGE_SSTIMER, 400);

	/*
	 * Configure interrupt moderation for receive. Enable
	 * the holdoff counter and load it, and set the RX
	 * suppression count to the number of descriptors we
	 * want to allow before triggering an interrupt.
	 * The holdoff timer is in units of 20 usecs.
	 */

#ifdef notyet
	CSR_WRITE_1(sc, VGE_INTCTL1, VGE_INTCTL_TXINTSUP_DISABLE);
	/* Select the interrupt holdoff timer page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_INTHLDOFF);
	CSR_WRITE_1(sc, VGE_INTHOLDOFF, 10); /* ~200 usecs */

	/* Enable use of the holdoff timer. */
	CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_HOLDOFF);
	CSR_WRITE_1(sc, VGE_INTCTL1, VGE_INTCTL_SC_RELOAD);

	/* Select the RX suppression threshold page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_RXSUPPTHR);
	CSR_WRITE_1(sc, VGE_RXSUPPTHR, 64); /* interrupt after 64 packets */

	/* Restore the page select bits. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_MAR);
#endif

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, VGE_IMR, VGE_INTRS);
	CSR_WRITE_4(sc, VGE_ISR, 0);
	CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_GMSK);

	/* Restore BMCR state */
	mii_mediachg(&sc->sc_mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->vge_if_flags = 0;
	sc->vge_link = 0;

	if (!timeout_pending(&sc->timer_handle))
		timeout_add(&sc->timer_handle, hz);

	return (0);
}

/*
 * Set media options.
 */
int
vge_ifmedia_upd(struct ifnet *ifp)
{
	struct vge_softc *sc = ifp->if_softc;

	return (mii_mediachg(&sc->sc_mii));
}

/*
 * Report current media status.
 */
void
vge_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct vge_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

void
vge_miibus_statchg(struct device *dev)
{
	struct vge_softc	*sc = (struct vge_softc *)dev;
	struct mii_data		*mii;
	struct ifmedia_entry	*ife;

	mii = &sc->sc_mii;
	ife = mii->mii_media.ifm_cur;

	/*
	 * If the user manually selects a media mode, we need to turn
	 * on the forced MAC mode bit in the DIAGCTL register. If the
	 * user happens to choose a full duplex mode, we also need to
	 * set the 'force full duplex' bit. This applies only to
	 * 10Mbps and 100Mbps speeds. In autoselect mode, forced MAC
	 * mode is disabled, and in 1000baseT mode, full duplex is
	 * always implied, so we turn on the forced mode bit but leave
	 * the FDX bit cleared.
	 */

	switch (IFM_SUBTYPE(ife->ifm_media)) {
	case IFM_AUTO:
		CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_MACFORCE);
		CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		break;
	case IFM_1000_T:
		CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_MACFORCE);
		CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		break;
	case IFM_100_TX:
	case IFM_10_T:
		CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_MACFORCE);
		if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
			CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		} else {
			CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		}
		break;
	default:
		printf("%s: unknown media type: %x\n",
		    sc->vge_dev.dv_xname, IFM_SUBTYPE(ife->ifm_media));
		break;
	}
}

int
vge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct vge_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct ifaddr		*ifa = (struct ifaddr *) data;
	int			s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			vge_init(ifp);
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif
		default:
			vge_init(ifp);
			break;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->vge_if_flags & IFF_PROMISC)) {
				CSR_SETBIT_1(sc, VGE_RXCTL,
				    VGE_RXCTL_RX_PROMISC);
				vge_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->vge_if_flags & IFF_PROMISC) {
				CSR_CLRBIT_1(sc, VGE_RXCTL,
				    VGE_RXCTL_RX_PROMISC);
				vge_setmulti(sc);
                        } else
				vge_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vge_stop(sc);
		}
		sc->vge_if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				vge_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, command);
		break;
	default:
		error = ENOTTY;
		break;
	}

	splx(s);
	return (error);
}

void
vge_watchdog(struct ifnet *ifp)
{
	struct vge_softc *sc = ifp->if_softc;
	int s;

	s = splnet();
	printf("%s: watchdog timeout\n", sc->vge_dev.dv_xname);
	ifp->if_oerrors++;

	vge_txeof(sc);
	vge_rxeof(sc);

	vge_init(ifp);

	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
vge_stop(struct vge_softc *sc)
{
	int			i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;
	if (timeout_pending(&sc->timer_handle))
		timeout_del(&sc->timer_handle);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	CSR_WRITE_1(sc, VGE_CRC3, VGE_CR3_INT_GMSK);
	CSR_WRITE_1(sc, VGE_CRS0, VGE_CR0_STOP);
	CSR_WRITE_4(sc, VGE_ISR, 0xFFFFFFFF);
	CSR_WRITE_2(sc, VGE_TXQCSRC, 0xFFFF);
	CSR_WRITE_1(sc, VGE_RXQCSRC, 0xFF);
	CSR_WRITE_4(sc, VGE_RXDESC_ADDR_LO, 0);

	if (sc->vge_head != NULL) {
		m_freem(sc->vge_head);
		sc->vge_head = sc->vge_tail = NULL;
	}

	/* Free the TX list buffers. */
	for (i = 0; i < VGE_TX_DESC_CNT; i++) {
		if (sc->vge_ldata.vge_tx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->vge_ldata.vge_tx_dmamap[i]);
			m_freem(sc->vge_ldata.vge_tx_mbuf[i]);
			sc->vge_ldata.vge_tx_mbuf[i] = NULL;
		}
	}

	/* Free the RX list buffers. */
	for (i = 0; i < VGE_RX_DESC_CNT; i++) {
		if (sc->vge_ldata.vge_rx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->vge_ldata.vge_rx_dmamap[i]);
			m_freem(sc->vge_ldata.vge_rx_mbuf[i]);
			sc->vge_ldata.vge_rx_mbuf[i] = NULL;
		}
	}
}
