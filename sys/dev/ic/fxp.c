/*	$OpenBSD: fxp.c,v 1.53 2004/05/18 22:37:25 beck Exp $	*/
/*	$NetBSD: if_fxp.c,v 1.2 1997/06/05 02:01:55 thorpej Exp $	*/

/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Modifications to support NetBSD:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: if_fxp.c,v 1.55 1998/08/04 08:53:12 dg Exp
 */

/*
 * Intel EtherExpress Pro/100B PCI Fast Ethernet driver
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <netinet/if_ether.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/fxpreg.h>
#include <dev/ic/fxpvar.h>

/*
 * NOTE!  On the Alpha, we have an alignment constraint.  The
 * card DMAs the packet immediately following the RFA.  However,
 * the first thing in the packet is a 14-byte Ethernet header.
 * This means that the packet is misaligned.  To compensate,
 * we actually offset the RFA 2 bytes into the cluster.  This
 * aligns the packet after the Ethernet header at a 32-bit
 * boundary.  HOWEVER!  This means that the RFA is misaligned!
 */
#define	RFA_ALIGNMENT_FUDGE	(2 + sizeof(bus_dmamap_t *))

/*
 * Inline function to copy a 16-bit aligned 32-bit quantity.
 */
static __inline void fxp_lwcopy(volatile u_int32_t *,
	volatile u_int32_t *);
static __inline void
fxp_lwcopy(src, dst)
	volatile u_int32_t *src, *dst;
{
	volatile u_int16_t *a = (u_int16_t *)src;
	volatile u_int16_t *b = (u_int16_t *)dst;

	b[0] = a[0];
	b[1] = a[1];
}

/*
 * Template for default configuration parameters.
 * See struct fxp_cb_config for the bit definitions.
 * Note, cb_command is filled in later.
 */
static u_char fxp_cb_config_template[] = {
	0x0, 0x0,		/* cb_status */
	0x0, 0x0,		/* cb_command */
	0xff, 0xff, 0xff, 0xff,	/* link_addr */
	0x16,	/*  0 Byte count. */
	0x08,	/*  1 Fifo limit */
	0x00,	/*  2 Adaptive ifs */
	0x00,	/*  3 void1 */
	0x00,	/*  4 rx_dma_bytecount */
	0x80,	/*  5 tx_dma_bytecount */
	0xb2,	/*  6 ctrl 1*/
	0x03,	/*  7 ctrl 2*/
	0x01,	/*  8 mediatype */
	0x00,	/*  9 void2 */
	0x26,	/* 10 ctrl3 */
	0x00,	/* 11 linear priority */
	0x60,	/* 12 interfrm_spacing */
	0x00,	/* 13 void31 */
	0xf2,	/* 14 void32 */
	0x48,	/* 15 promiscuous */
	0x00,	/* 16 void41 */
	0x40,	/* 17 void42 */
	0xf3,	/* 18 stripping */
	0x00,	/* 19 fdx_pin */
	0x3f,	/* 20 multi_ia */
	0x05	/* 21 mc_all */
};

void fxp_eeprom_shiftin(struct fxp_softc *, int, int);
void fxp_eeprom_putword(struct fxp_softc *, int, u_int16_t);
void fxp_write_eeprom(struct fxp_softc *, u_short *, int, int);
int fxp_mediachange(struct ifnet *);
void fxp_mediastatus(struct ifnet *, struct ifmediareq *);
void fxp_scb_wait(struct fxp_softc *);
void fxp_start(struct ifnet *);
int fxp_ioctl(struct ifnet *, u_long, caddr_t);
void fxp_init(void *);
void fxp_stop(struct fxp_softc *, int);
void fxp_watchdog(struct ifnet *);
int fxp_add_rfabuf(struct fxp_softc *, struct mbuf *);
int fxp_mdi_read(struct device *, int, int);
void fxp_mdi_write(struct device *, int, int, int);
void fxp_autosize_eeprom(struct fxp_softc *);
void fxp_statchg(struct device *);
void fxp_read_eeprom(struct fxp_softc *, u_int16_t *,
				    int, int);
void fxp_stats_update(void *);
void fxp_mc_setup(struct fxp_softc *, int);
void fxp_scb_cmd(struct fxp_softc *, u_int8_t);

/*
 * Set initial transmit threshold at 64 (512 bytes). This is
 * increased by 64 (512 bytes) at a time, to maximum of 192
 * (1536 bytes), if an underrun occurs.
 */
static int tx_threshold = 64;

/*
 * TxCB list index mask. This is used to do list wrap-around.
 */
#define FXP_TXCB_MASK	(FXP_NTXCB - 1)

/*
 * Maximum number of seconds that the receiver can be idle before we
 * assume it's dead and attempt to reset it by reprogramming the
 * multicast filter. This is part of a work-around for a bug in the
 * NIC. See fxp_stats_update().
 */
#define FXP_MAX_RX_IDLE	15

/*
 * Wait for the previous command to be accepted (but not necessarily
 * completed).
 */
void
fxp_scb_wait(sc)
	struct fxp_softc *sc;
{
	int i = 10000;

	while (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) && --i)
		DELAY(2);
	if (i == 0)
		printf("%s: warning: SCB timed out\n", sc->sc_dev.dv_xname);
}


void 
fxp_eeprom_shiftin(struct fxp_softc *sc, int data, int length)
{
	u_int16_t reg;
	int x;

	/*
	 * Shift in data.
	 */
	for (x = 1 << (length - 1); x; x >>= 1) {
		if (data & x)
			reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
		else
			reg = FXP_EEPROM_EECS;
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg | FXP_EEPROM_EESK);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
	}
}


void
fxp_eeprom_putword(struct fxp_softc *sc, int offset, u_int16_t data)
{
	int i;

	/*
	 * Erase/write enable.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(sc, 0x4, 3);
	fxp_eeprom_shiftin(sc, 0x03 << (sc->eeprom_size - 2), sc->eeprom_size);
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	/*
	 * Shift in write opcode, address, data.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(sc, FXP_EEPROM_OPC_WRITE, 3);
	fxp_eeprom_shiftin(sc, offset, sc->eeprom_size);
	fxp_eeprom_shiftin(sc, data, 16);
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	/*
	 * Wait for EEPROM to finish up.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	DELAY(1);
	for (i = 0; i < 1000; i++) {
		if (CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) & FXP_EEPROM_EEDO)
			break;
		DELAY(50);
	}
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	/*
	 * Erase/write disable.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(sc, 0x4, 3);
	fxp_eeprom_shiftin(sc, 0, sc->eeprom_size);
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
}

void
fxp_write_eeprom(struct fxp_softc *sc, u_short *data, int offset, int words)
{
	int i;

	for (i = 0; i < words; i++)
		fxp_eeprom_putword(sc, offset + i, data[i]);
}



/*************************************************************
 * Operating system-specific autoconfiguration glue
 *************************************************************/

void	fxp_shutdown(void *);
void	fxp_power(int, void *);

struct cfdriver fxp_cd = {
	NULL, "fxp", DV_IFNET
};

/*
 * Device shutdown routine. Called at system shutdown after sync. The
 * main purpose of this routine is to shut off receiver DMA so that
 * kernel memory doesn't get clobbered during warmboot.
 */
void
fxp_shutdown(sc)
	void *sc;
{
	fxp_stop((struct fxp_softc *) sc, 0);
}

/*
 * Power handler routine. Called when the system is transitioning
 * into/out of power save modes.  As with fxp_shutdown, the main
 * purpose of this routine is to shut off receiver DMA so it doesn't
 * clobber kernel memory at the wrong time.
 */
void
fxp_power(why, arg)
	int why;
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct ifnet *ifp;
	int s;

	s = splimp();
	if (why != PWR_RESUME)
		fxp_stop(sc, 0);
	else {
		ifp = &sc->sc_arpcom.ac_if;
		if (ifp->if_flags & IFF_UP)
			fxp_init(sc);
	}
	splx(s);
}

/*************************************************************
 * End of operating system-specific autoconfiguration glue
 *************************************************************/

/*
 * Do generic parts of attach.
 */
int
fxp_attach_common(sc, enaddr, intrstr)
	struct fxp_softc *sc;
	u_int8_t *enaddr;
	const char *intrstr;
{
	struct ifnet *ifp;
	struct mbuf *m;
	bus_dmamap_t rxmap;
	u_int16_t data;
	int i, err;

	/*
	 * Reset to a stable state.
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SOFTWARE_RESET);
	DELAY(10);

	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(struct fxp_ctrl),
	    PAGE_SIZE, 0, &sc->sc_cb_seg, 1, &sc->sc_cb_nseg, BUS_DMA_NOWAIT))
		goto fail;
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_cb_seg, sc->sc_cb_nseg,
	    sizeof(struct fxp_ctrl), (caddr_t *)&sc->sc_ctrl,
	    BUS_DMA_NOWAIT)) {
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cb_seg, sc->sc_cb_nseg);
		goto fail;
	}
	if (bus_dmamap_create(sc->sc_dmat, sizeof(struct fxp_ctrl),
	    1, sizeof(struct fxp_ctrl), 0, BUS_DMA_NOWAIT,
	    &sc->tx_cb_map)) {
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_ctrl,
		    sizeof(struct fxp_ctrl));
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cb_seg, sc->sc_cb_nseg);
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->tx_cb_map, (caddr_t)sc->sc_ctrl,
	    sizeof(struct fxp_ctrl), NULL, BUS_DMA_NOWAIT)) {
		bus_dmamap_destroy(sc->sc_dmat, sc->tx_cb_map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_ctrl,
		    sizeof(struct fxp_ctrl));
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cb_seg, sc->sc_cb_nseg);
	}

	for (i = 0; i < FXP_NTXCB; i++) {
		if ((err = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    FXP_NTXSEG, MCLBYTES, 0, 0, &sc->txs[i].tx_map)) != 0) {
			printf("%s: unable to create tx dma map %d, error %d\n",
			    sc->sc_dev.dv_xname, i, err);
			goto fail;
		}
		sc->txs[i].tx_mbuf = NULL;
		sc->txs[i].tx_cb = sc->sc_ctrl->tx_cb + i;
		sc->txs[i].tx_off = offsetof(struct fxp_ctrl, tx_cb[i]);
		sc->txs[i].tx_next = &sc->txs[(i + 1) & FXP_TXCB_MASK];
	}
	bzero(sc->sc_ctrl, sizeof(struct fxp_ctrl));

	/*
	 * Pre-allocate some receive buffers.
	 */
	sc->sc_rxfree = 0;
	for (i = 0; i < FXP_NRFABUFS_MIN; i++) {
		if ((err = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxmaps[i])) != 0) {
			printf("%s: unable to create rx dma map %d, error %d\n",
			    sc->sc_dev.dv_xname, i, err);
			goto fail;
		}
		sc->rx_bufs++;
	}
	for (i = 0; i < FXP_NRFABUFS_MIN; i++)
		if (fxp_add_rfabuf(sc, NULL) != 0)
			goto fail;

	/*
	 * Find out how large of an SEEPROM we have.
	 */
	fxp_autosize_eeprom(sc);

	/*
	 * Get info about the primary PHY
	 */
	fxp_read_eeprom(sc, (u_int16_t *)&data, 6, 1);
	sc->phy_primary_addr = data & 0xff;
	sc->phy_primary_device = (data >> 8) & 0x3f;
	sc->phy_10Mbps_only = data >> 15;

	/*
	 * Read MAC address.
	 */
	fxp_read_eeprom(sc, (u_int16_t *)enaddr, 0, 3);

	ifp = &sc->sc_arpcom.ac_if;
	bcopy(enaddr, sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = fxp_ioctl;
	ifp->if_start = fxp_start;
	ifp->if_watchdog = fxp_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

	
	if (sc->sc_flags & FXPF_DISABLE_STANDBY) {
		fxp_read_eeprom(sc, &data, 10, 1);
		if (data & 0x02) {			/* STB enable */
			u_int16_t cksum;
			int i;
			printf("Disabling dynamic standby mode in EEPROM\n");
			data &= ~0x02;
			fxp_write_eeprom(sc, &data, 10, 1);
			printf("New EEPROM ID: 0x%x\n", data);
			cksum = 0;
			for (i = 0; i < (1 << sc->eeprom_size) - 1; i++) {
				fxp_read_eeprom(sc, &data, i, 1);
				cksum += data;
			}
			i = (1 << sc->eeprom_size) - 1;
			cksum = 0xBABA - cksum;
			fxp_read_eeprom(sc, &data, i, 1);
			fxp_write_eeprom(sc, &cksum, i, 1);
			printf("EEPROM checksum @ 0x%x: 0x%x -> 0x%x\n",
			    i, data, cksum);
		}
	}

#if NVLAN > 0
	/*
	 * Only 82558 and newer cards have a bit to ignore oversized frames.
	 */
	if (sc->not_82557)
		ifp->if_capabilities |= IFCAP_VLAN_MTU;
#endif

	printf(": %s, address %s\n", intrstr,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = fxp_mdi_read;
	sc->sc_mii.mii_writereg = fxp_mdi_write;
	sc->sc_mii.mii_statchg = fxp_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, fxp_mediachange,
	    fxp_mediastatus);
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, MIIF_NOISOLATE);
	/* If no phy found, just use auto mode */
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL,
		    0, NULL);
		printf("%s: no phy found, using manual mode\n",
		    sc->sc_dev.dv_xname);
	}

	if (ifmedia_match(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL, 0))
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	else if (ifmedia_match(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO, 0))
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
	else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	/*
	 * Let the system queue as many packets as we have available
	 * TX descriptors.
	 */
	IFQ_SET_MAXLEN(&ifp->if_snd, FXP_NTXCB - 1);
	ether_ifattach(ifp);

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing so could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	sc->sc_sdhook = shutdownhook_establish(fxp_shutdown, sc);

	/*
	 * Add suspend hook, for similiar reasons..
	 */
	sc->sc_powerhook = powerhook_establish(fxp_power, sc);

	/*
	 * Initialize timeout for statistics update.
	 */
	timeout_set(&sc->stats_update_to, fxp_stats_update, sc);

	return (0);

 fail:
	printf("%s: Failed to malloc memory\n", sc->sc_dev.dv_xname);
	if (sc->tx_cb_map != NULL) {
		bus_dmamap_unload(sc->sc_dmat, sc->tx_cb_map);
		bus_dmamap_destroy(sc->sc_dmat, sc->tx_cb_map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_ctrl,
		    sizeof(struct fxp_cb_tx) * FXP_NTXCB);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cb_seg, sc->sc_cb_nseg);
	}
	m = sc->rfa_headm;
	while (m != NULL) {
		rxmap = *((bus_dmamap_t *)m->m_ext.ext_buf);
		bus_dmamap_unload(sc->sc_dmat, rxmap);
		FXP_RXMAP_PUT(sc, rxmap);
		m = m_free(m);
	}
	return (ENOMEM);
}

int
fxp_detach(sc)
	struct fxp_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	/* Unhook our tick handler. */
	timeout_del(&sc->stats_update_to);

	/* Detach any PHYs we might have. */
	if (LIST_FIRST(&sc->sc_mii.mii_phys) != NULL)
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete any remaining media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);

	shutdownhook_disestablish(sc->sc_sdhook);
	powerhook_disestablish(sc->sc_powerhook);

	return (0);
}

/*
 * From NetBSD:
 *
 * Figure out EEPROM size.
 *
 * 559's can have either 64-word or 256-word EEPROMs, the 558
 * datasheet only talks about 64-word EEPROMs, and the 557 datasheet
 * talks about the existence of 16 to 256 word EEPROMs.
 *
 * The only known sizes are 64 and 256, where the 256 version is used
 * by CardBus cards to store CIS information.
 *
 * The address is shifted in msb-to-lsb, and after the last
 * address-bit the EEPROM is supposed to output a `dummy zero' bit,
 * after which follows the actual data. We try to detect this zero, by
 * probing the data-out bit in the EEPROM control register just after
 * having shifted in a bit. If the bit is zero, we assume we've
 * shifted enough address bits. The data-out should be tri-state,
 * before this, which should translate to a logical one.
 *
 * Other ways to do this would be to try to read a register with known
 * contents with a varying number of address bits, but no such
 * register seem to be available. The high bits of register 10 are 01
 * on the 558 and 559, but apparently not on the 557.
 * 
 * The Linux driver computes a checksum on the EEPROM data, but the
 * value of this checksum is not very well documented.
 */
void
fxp_autosize_eeprom(sc)
	struct fxp_softc *sc;
{
	u_int16_t reg;
	int x;

	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	/*
	 * Shift in read opcode.
	 */
	for (x = 3; x > 0; x--) {
		if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
			reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
		} else {
			reg = FXP_EEPROM_EECS;
		}
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
		    reg | FXP_EEPROM_EESK);
		DELAY(4);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(4);
	}
	/*
	 * Shift in address.
	 * Wait for the dummy zero following a correct address shift.
	 */
	for (x = 1; x <= 8; x++) {
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			FXP_EEPROM_EECS | FXP_EEPROM_EESK);
		DELAY(4);
		if ((CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) & FXP_EEPROM_EEDO) == 0)
			break;
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		DELAY(4);
	}
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(4);
	sc->eeprom_size = x;
}





/*
 * Read from the serial EEPROM. Basically, you manually shift in
 * the read opcode (one bit at a time) and then shift in the address,
 * and then you shift out the data (all of this one bit at a time).
 * The word size is 16 bits, so you have to provide the address for
 * every 16 bits of data.
 */
void
fxp_read_eeprom(sc, data, offset, words)
	struct fxp_softc *sc;
	u_short *data;
	int offset;
	int words;
{
	u_int16_t reg;
	int i, x;

	for (i = 0; i < words; i++) {
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		/*
		 * Shift in read opcode.
		 */
		for (x = 3; x > 0; x--) {
			if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(4);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(4);
		}
		/*
		 * Shift in address.
		 */
		for (x = sc->eeprom_size; x > 0; x--) {
			if ((i + offset) & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(4);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(4);
		}
		reg = FXP_EEPROM_EECS;
		data[i] = 0;
		/*
		 * Shift out data.
		 */
		for (x = 16; x > 0; x--) {
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(4);
			if (CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) &
			    FXP_EEPROM_EEDO)
				data[i] |= (1 << (x - 1));
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(4);
		}
		data[i] = letoh16(data[i]);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
		DELAY(4);
	}
}

/*
 * Start packet transmission on the interface.
 */
void
fxp_start(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;
	struct fxp_txsw *txs = sc->sc_cbt_prod;
	struct fxp_cb_tx *txc;
	struct mbuf *m0, *m = NULL;
	int cnt = sc->sc_cbt_cnt, seg;

	if ((ifp->if_flags & (IFF_OACTIVE | IFF_RUNNING)) != IFF_RUNNING)
		return;

	while (1) {
		if (cnt >= (FXP_NTXCB - 2)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		txs = txs->tx_next;

		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (bus_dmamap_load_mbuf(sc->sc_dmat, txs->tx_map,
		    m0, BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				break;
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if (!(m->m_flags & M_EXT)) {
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			if (bus_dmamap_load_mbuf(sc->sc_dmat, txs->tx_map,
			    m, BUS_DMA_NOWAIT) != 0) {
				m_freem(m);
				break;
			}
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
			m = NULL;
		}

		txs->tx_mbuf = m0;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif

		FXP_MBUF_SYNC(sc, txs->tx_map, BUS_DMASYNC_PREWRITE);

		txc = txs->tx_cb;
		txc->tbd_number = txs->tx_map->dm_nsegs;
		txc->cb_status = 0;
		txc->cb_command = htole16(FXP_CB_COMMAND_XMIT | FXP_CB_COMMAND_SF);
		txc->tx_threshold = tx_threshold;
		for (seg = 0; seg < txs->tx_map->dm_nsegs; seg++) {
			txc->tbd[seg].tb_addr =
			    htole32(txs->tx_map->dm_segs[seg].ds_addr);
			txc->tbd[seg].tb_size =
			    htole32(txs->tx_map->dm_segs[seg].ds_len);
		}
		FXP_TXCB_SYNC(sc, txs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		++cnt;
		sc->sc_cbt_prod = txs;
	}

	if (cnt != sc->sc_cbt_cnt) {
		/* We enqueued at least one. */
		ifp->if_timer = 5;

		txs = sc->sc_cbt_prod;
		txs = txs->tx_next;
		sc->sc_cbt_prod = txs;
		txs->tx_cb->cb_command =
		    htole16(FXP_CB_COMMAND_I | FXP_CB_COMMAND_NOP | FXP_CB_COMMAND_S);
		FXP_TXCB_SYNC(sc, txs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		FXP_TXCB_SYNC(sc, sc->sc_cbt_prev,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		sc->sc_cbt_prev->tx_cb->cb_command &=
		    htole16(~(FXP_CB_COMMAND_S | FXP_CB_COMMAND_I));
		FXP_TXCB_SYNC(sc, sc->sc_cbt_prev,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		sc->sc_cbt_prev = txs;

		fxp_scb_wait(sc);
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_RESUME);

		sc->sc_cbt_cnt = cnt + 1;
	}
}

/*
 * Process interface interrupts.
 */
int
fxp_intr(arg)
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int8_t statack;
	bus_dmamap_t rxmap;
	int claimed = 0;
	int rnr = 0;
	
	/*
	 * If the interface isn't running, don't try to
	 * service the interrupt.. just ack it and bail.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		statack = CSR_READ_1(sc, FXP_CSR_SCB_STATACK);
		if (statack) {
			claimed = 1;
			CSR_WRITE_1(sc, FXP_CSR_SCB_STATACK, statack);
		}
		return claimed;
	}

	while ((statack = CSR_READ_1(sc, FXP_CSR_SCB_STATACK)) != 0) {
		claimed = 1;
		rnr = (statack & FXP_SCB_STATACK_RNR) ? 1 : 0;
		/*
		 * First ACK all the interrupts in this pass.
		 */
		CSR_WRITE_1(sc, FXP_CSR_SCB_STATACK, statack);

		/*
		 * Free any finished transmit mbuf chains.
		 */
		if (statack & (FXP_SCB_STATACK_CXTNO|FXP_SCB_STATACK_CNA)) {
			int txcnt = sc->sc_cbt_cnt;
			struct fxp_txsw *txs = sc->sc_cbt_cons;

			FXP_TXCB_SYNC(sc, txs,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

			while ((txcnt > 0) &&
			   ((txs->tx_cb->cb_status & htole16(FXP_CB_STATUS_C)) ||
			   (txs->tx_cb->cb_command & htole16(FXP_CB_COMMAND_NOP)))) {
				if (txs->tx_mbuf != NULL) {
					FXP_MBUF_SYNC(sc, txs->tx_map,
					    BUS_DMASYNC_POSTWRITE);
					bus_dmamap_unload(sc->sc_dmat,
					    txs->tx_map);
					m_freem(txs->tx_mbuf);
					txs->tx_mbuf = NULL;
				}
				--txcnt;
				txs = txs->tx_next;
				FXP_TXCB_SYNC(sc, txs,
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			}
			sc->sc_cbt_cons = txs;
			sc->sc_cbt_cnt = txcnt;
			ifp->if_timer = 0;
			ifp->if_flags &= ~IFF_OACTIVE;

			if (!IFQ_IS_EMPTY(&ifp->if_snd)) {
				/*
				 * Try to start more packets transmitting.
				 */
				fxp_start(ifp);
			}
		}
		/*
		 * Process receiver interrupts. If a no-resource (RNR)
		 * condition exists, get whatever packets we can and
		 * re-start the receiver.
		 */
		if (statack & (FXP_SCB_STATACK_FR | FXP_SCB_STATACK_RNR)) {
			struct mbuf *m;
			u_int8_t *rfap;
rcvloop:
			m = sc->rfa_headm;
			rfap = m->m_ext.ext_buf + RFA_ALIGNMENT_FUDGE;
			rxmap = *((bus_dmamap_t *)m->m_ext.ext_buf);
			bus_dmamap_sync(sc->sc_dmat, rxmap,
			    0, MCLBYTES, BUS_DMASYNC_POSTREAD |
			    BUS_DMASYNC_POSTWRITE);

			if (*(u_int16_t *)(rfap +
			    offsetof(struct fxp_rfa, rfa_status)) &
			    htole16(FXP_RFA_STATUS_C)) {
				if (*(u_int16_t *)(rfap +
				    offsetof(struct fxp_rfa, rfa_status)) &
				    htole16(FXP_RFA_STATUS_RNR))
					rnr = 1;

				/*
				 * Remove first packet from the chain.
				 */
				sc->rfa_headm = m->m_next;
				m->m_next = NULL;

				/*
				 * Add a new buffer to the receive chain.
				 * If this fails, the old buffer is recycled
				 * instead.
				 */
				if (fxp_add_rfabuf(sc, m) == 0) {
					u_int16_t total_len;

					total_len = htole16(*(u_int16_t *)(rfap +
					    offsetof(struct fxp_rfa,
					    actual_size))) &
					    (MCLBYTES - 1);
					if (total_len <
					    sizeof(struct ether_header)) {
						m_freem(m);
						goto rcvloop;
					}
					if (*(u_int16_t *)(rfap +
					    offsetof(struct fxp_rfa,
					    rfa_status)) &
					    htole16(FXP_RFA_STATUS_CRC)) {
						m_freem(m);
						goto rcvloop;
					}

					m->m_pkthdr.rcvif = ifp;
					m->m_pkthdr.len = m->m_len =
					    total_len;
#if NBPFILTER > 0
					if (ifp->if_bpf)
						bpf_mtap(ifp->if_bpf, m);
#endif /* NBPFILTER > 0 */
					ether_input_mbuf(ifp, m);
				}
				goto rcvloop;
			}
		}
		if (rnr) {
			rxmap = *((bus_dmamap_t *)
			    sc->rfa_headm->m_ext.ext_buf);
			fxp_scb_wait(sc);
			CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
				    rxmap->dm_segs[0].ds_addr +
				    RFA_ALIGNMENT_FUDGE);
			fxp_scb_cmd(sc, FXP_SCB_COMMAND_RU_START);
			
		}
	}
	return (claimed);
}

/*
 * Update packet in/out/collision statistics. The i82557 doesn't
 * allow you to access these counters without doing a fairly
 * expensive DMA to get _all_ of the statistics it maintains, so
 * we do this operation here only once per second. The statistics
 * counters in the kernel are updated from the previous dump-stats
 * DMA and then a new dump-stats DMA is started. The on-chip
 * counters are zeroed when the DMA completes. If we can't start
 * the DMA immediately, we don't wait - we just prepare to read
 * them again next time.
 */
void
fxp_stats_update(arg)
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct fxp_stats *sp = &sc->sc_ctrl->stats;
	int s;

	FXP_STATS_SYNC(sc, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	ifp->if_opackets += letoh32(sp->tx_good);
	ifp->if_collisions += letoh32(sp->tx_total_collisions);
	if (sp->rx_good) {
		ifp->if_ipackets += letoh32(sp->rx_good);
		sc->rx_idle_secs = 0;
	} else
		sc->rx_idle_secs++;
	ifp->if_ierrors +=
	    letoh32(sp->rx_crc_errors) +
	    letoh32(sp->rx_alignment_errors) +
	    letoh32(sp->rx_rnr_errors) +
	    letoh32(sp->rx_overrun_errors);
	/*
	 * If any transmit underruns occurred, bump up the transmit
	 * threshold by another 512 bytes (64 * 8).
	 */
	if (sp->tx_underruns) {
		ifp->if_oerrors += letoh32(sp->tx_underruns);
		if (tx_threshold < 192)
			tx_threshold += 64;
	}
	s = splimp();
	/*
	 * If we haven't received any packets in FXP_MAX_RX_IDLE seconds,
	 * then assume the receiver has locked up and attempt to clear
	 * the condition by reprogramming the multicast filter. This is
	 * a work-around for a bug in the 82557 where the receiver locks
	 * up if it gets certain types of garbage in the synchronization
	 * bits prior to the packet header. This bug is supposed to only
	 * occur in 10Mbps mode, but has been seen to occur in 100Mbps
	 * mode as well (perhaps due to a 10/100 speed transition).
	 */
	if (sc->rx_idle_secs > FXP_MAX_RX_IDLE) {
		sc->rx_idle_secs = 0;
		fxp_init(sc);
		splx(s);
		return;
	}
	/*
	 * If there is no pending command, start another stats
	 * dump. Otherwise punt for now.
	 */
	FXP_STATS_SYNC(sc, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	if (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) == 0) {
		/*
		 * Start another stats dump.
		 */
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_DUMPRESET);
	} else {
		/*
		 * A previous command is still waiting to be accepted.
		 * Just zero our copy of the stats and wait for the
		 * next timer event to update them.
		 */
		sp->tx_good = 0;
		sp->tx_underruns = 0;
		sp->tx_total_collisions = 0;

		sp->rx_good = 0;
		sp->rx_crc_errors = 0;
		sp->rx_alignment_errors = 0;
		sp->rx_rnr_errors = 0;
		sp->rx_overrun_errors = 0;
	}

	/* Tick the MII clock. */
	mii_tick(&sc->sc_mii);

	splx(s);
	/*
	 * Schedule another timeout one second from now.
	 */
	timeout_add(&sc->stats_update_to, hz);
}

/*
 * Stop the interface. Cancels the statistics updater and resets
 * the interface.
 */
void
fxp_stop(sc, drain)
	struct fxp_softc *sc;
	int drain;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;

	/*
	 * Turn down interface (done early to avoid bad interactions
	 * between panics, shutdown hooks, and the watchdog timer)
	 */
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/*
	 * Cancel stats updater.
	 */
	timeout_del(&sc->stats_update_to);
	mii_down(&sc->sc_mii);

	/*
	 * Issue software reset
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	/*
	 * Release any xmit buffers.
	 */
	for (i = 0; i < FXP_NTXCB; i++) {
		if (sc->txs[i].tx_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->txs[i].tx_map);
			m_freem(sc->txs[i].tx_mbuf);
			sc->txs[i].tx_mbuf = NULL;
		}
	}
	sc->sc_cbt_cnt = 0;

	if (drain) {
		bus_dmamap_t rxmap;
		struct mbuf *m;

		/*
		 * Free all the receive buffers then reallocate/reinitialize
		 */
		m = sc->rfa_headm;
		while (m != NULL) {
			rxmap = *((bus_dmamap_t *)m->m_ext.ext_buf);
			bus_dmamap_unload(sc->sc_dmat, rxmap);
			FXP_RXMAP_PUT(sc, rxmap);
			m = m_free(m);
			sc->rx_bufs--;
		}
		sc->rfa_headm = NULL;
		sc->rfa_tailm = NULL;
		for (i = 0; i < FXP_NRFABUFS_MIN; i++) {
			if (fxp_add_rfabuf(sc, NULL) != 0) {
				/*
				 * This "can't happen" - we're at splimp()
				 * and we just freed all the buffers we need
				 * above.
				 */
				panic("fxp_stop: no buffers!");
			}
			sc->rx_bufs++;
		}
	}
}

/*
 * Watchdog/transmission transmit timeout handler. Called when a
 * transmission is started on the interface, but no interrupt is
 * received before the timeout. This usually indicates that the
 * card has wedged for some reason.
 */
void
fxp_watchdog(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	fxp_init(sc);
}

/*
 * Submit a command to the i82557.
 */
void
fxp_scb_cmd(sc, cmd)
	struct fxp_softc *sc;
	u_int8_t cmd;
{
	if (cmd == FXP_SCB_COMMAND_CU_RESUME &&
	    (sc->sc_flags & FXPF_FIX_RESUME_BUG) != 0) {
		CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_CB_COMMAND_NOP);
		fxp_scb_wait(sc);
	}
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, cmd);
}

void
fxp_init(xsc)
	void *xsc;
{
	struct fxp_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct fxp_cb_config *cbp;
	struct fxp_cb_ias *cb_ias;
	struct fxp_cb_tx *txp;
	struct mbuf *m;
	bus_dmamap_t rxmap;
	int i, prm, allm, s, bufs;

	s = splimp();

	/*
	 * Cancel any pending I/O
	 */
	fxp_stop(sc, 0);

	/*
	 * Initialize base of CBL and RFA memory. Loading with zero
	 * sets it up for regular linear addressing.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, 0);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_BASE);

	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, 0);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_RU_BASE);

	/* Once through to set flags */
	fxp_mc_setup(sc, 0);

	/*
	 * Initialize base of dump-stats buffer.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
	    sc->tx_cb_map->dm_segs->ds_addr +
	    offsetof(struct fxp_ctrl, stats));
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_DUMP_ADR);

	cbp = &sc->sc_ctrl->u.cfg;
	/*
	 * This bcopy is kind of disgusting, but there are a bunch of must be
	 * zero and must be one bits in this structure and this is the easiest
	 * way to initialize them all to proper values.
	 */
	bcopy(fxp_cb_config_template, (void *)&cbp->cb_status,
		sizeof(fxp_cb_config_template));

	prm = (ifp->if_flags & IFF_PROMISC) ? 1 : 0;
	allm = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;

#if 0
	cbp->cb_status =	0;
	cbp->cb_command =	FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL;
	cbp->link_addr =	0xffffffff;	/* (no) next command */
	cbp->byte_count =	22;		/* (22) bytes to config */
	cbp->rx_fifo_limit =	8;	/* rx fifo threshold (32 bytes) */
	cbp->tx_fifo_limit =	0;	/* tx fifo threshold (0 bytes) */
	cbp->adaptive_ifs =	0;	/* (no) adaptive interframe spacing */
	cbp->rx_dma_bytecount =	0;	/* (no) rx DMA max */
	cbp->tx_dma_bytecount =	0;	/* (no) tx DMA max */
	cbp->dma_bce =		0;	/* (disable) dma max counters */
	cbp->late_scb =		0;	/* (don't) defer SCB update */
	cbp->tno_int =		0;	/* (disable) tx not okay interrupt */
	cbp->ci_int =		1;	/* interrupt on CU idle */
	cbp->save_bf =		prm;	/* save bad frames */
	cbp->disc_short_rx =	!prm;	/* discard short packets */
	cbp->underrun_retry =	1;	/* retry mode (1) on DMA underrun */
	cbp->mediatype =	!sc->phy_10Mbps_only; /* interface mode */
	cbp->nsai =		1;	/* (don't) disable source addr insert */
	cbp->preamble_length =	2;	/* (7 byte) preamble */
	cbp->loopback =		0;	/* (don't) loopback */
	cbp->linear_priority =	0;	/* (normal CSMA/CD operation) */
	cbp->linear_pri_mode =	0;	/* (wait after xmit only) */
	cbp->interfrm_spacing =	6;	/* (96 bits of) interframe spacing */
	cbp->promiscuous =	prm;	/* promiscuous mode */
	cbp->bcast_disable =	0;	/* (don't) disable broadcasts */
	cbp->crscdt =		0;	/* (CRS only) */
	cbp->stripping =	!prm;	/* truncate rx packet to byte count */
	cbp->padding =		1;	/* (do) pad short tx packets */
	cbp->rcv_crc_xfer =	0;	/* (don't) xfer CRC to host */
	cbp->long_rx =		sc->not_82557; /* (enable) long packets */
	cbp->force_fdx =	0;	/* (don't) force full duplex */
	cbp->fdx_pin_en =	1;	/* (enable) FDX# pin */
	cbp->multi_ia =		0;	/* (don't) accept multiple IAs */
	cbp->mc_all =		allm;
#else
	cbp->cb_command = htole16(FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL);
	if (allm) 
		cbp->mc_all |= 0x08;		/* accept all multicasts */
	else
		cbp->mc_all &= ~0x08;		/* reject all multicasts */

	if (prm) {
		cbp->promiscuous |= 1;		/* promiscuous mode */
		cbp->ctrl1 |= 0x80;		/* save bad frames */
		cbp->ctrl2 &= ~0x01;		/* save short packets */
		cbp->stripping &= ~0x01;	/* don't truncate rx packets */
	}
	else {
		cbp->promiscuous &= ~1;		/* no promiscuous mode */
		cbp->ctrl1 &= ~0x80;		/* discard bad frames */
		cbp->ctrl2 |= 0x01;		/* discard short packets */
		cbp->stripping |= 0x01;		/* truncate rx packets */
	}

	if(!sc->phy_10Mbps_only)			/* interface mode */
		cbp->mediatype |= 0x01;
	else
		cbp->mediatype &= ~0x01;

	if(sc->not_82557)			/* long packets */
		cbp->stripping |= 0x08;
	else
		cbp->stripping &= ~0x08;

	cbp->tx_dma_bytecount = 0; /* (no) tx DMA max, dma_dce = 0 ??? */
 	cbp->ctrl1 |= 0x08;	/* ci_int = 1 */
	cbp->ctrl3 |= 0x08;	/* nsai */
	cbp->fifo_limit = 0x08; /* tx and rx fifo limit */
	cbp->fdx_pin |= 0x80;	/* Enable full duplex setting by pin */
#endif

	/*
	 * Start the config command/DMA.
	 */
	fxp_scb_wait(sc);
	FXP_CFG_SYNC(sc, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->tx_cb_map->dm_segs->ds_addr +
	    offsetof(struct fxp_ctrl, u.cfg));
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	do {
		DELAY(1);
		FXP_CFG_SYNC(sc, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	} while ((cbp->cb_status & htole16(FXP_CB_STATUS_C)) == 0);

	/*
	 * Now initialize the station address.
	 */
	cb_ias = &sc->sc_ctrl->u.ias;
	cb_ias->cb_status = htole16(0);
	cb_ias->cb_command = htole16(FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL);
	cb_ias->link_addr = htole32(0xffffffff);
	bcopy(sc->sc_arpcom.ac_enaddr, (void *)cb_ias->macaddr,
	    sizeof(sc->sc_arpcom.ac_enaddr));

	/*
	 * Start the IAS (Individual Address Setup) command/DMA.
	 */
	fxp_scb_wait(sc);
	FXP_IAS_SYNC(sc, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->tx_cb_map->dm_segs->ds_addr +
	    offsetof(struct fxp_ctrl, u.ias));
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	do {
		DELAY(1);
		FXP_IAS_SYNC(sc, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	} while (!(cb_ias->cb_status & htole16(FXP_CB_STATUS_C)));

	/* Again, this time really upload the multicast addresses */
	fxp_mc_setup(sc, 1);

	/*
	 * Initialize transmit control block (TxCB) list.
	 */
	bzero(sc->sc_ctrl->tx_cb, sizeof(struct fxp_cb_tx) * FXP_NTXCB);
	txp = sc->sc_ctrl->tx_cb;
	for (i = 0; i < FXP_NTXCB; i++) {
		txp[i].cb_command = htole16(FXP_CB_COMMAND_NOP);
		txp[i].link_addr = htole32(sc->tx_cb_map->dm_segs->ds_addr +
		    offsetof(struct fxp_ctrl, tx_cb[(i + 1) & FXP_TXCB_MASK]));
		txp[i].tbd_array_addr =htole32(sc->tx_cb_map->dm_segs->ds_addr +
		    offsetof(struct fxp_ctrl, tx_cb[i].tbd[0]));
	}
	/*
	 * Set the suspend flag on the first TxCB and start the control
	 * unit. It will execute the NOP and then suspend.
	 */
	sc->sc_cbt_prev = sc->sc_cbt_prod = sc->sc_cbt_cons = sc->txs;
	sc->sc_cbt_cnt = 1;
	sc->sc_ctrl->tx_cb[0].cb_command = htole16(FXP_CB_COMMAND_NOP |
	    FXP_CB_COMMAND_S | FXP_CB_COMMAND_I);
	bus_dmamap_sync(sc->sc_dmat, sc->tx_cb_map, 0,
	    sc->tx_cb_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->tx_cb_map->dm_segs->ds_addr +
	    offsetof(struct fxp_ctrl, tx_cb[0]));
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);

	/*
	 * Initialize receiver buffer area - RFA.
	 */
	if (ifp->if_flags & IFF_UP)
		bufs = FXP_NRFABUFS_MAX;
	else
		bufs = FXP_NRFABUFS_MIN;
	if (sc->rx_bufs > bufs) {
		while (sc->rfa_headm != NULL && sc->rx_bufs-- > bufs) {
			rxmap = *((bus_dmamap_t *)m->m_ext.ext_buf);
			bus_dmamap_unload(sc->sc_dmat, rxmap);
			FXP_RXMAP_PUT(sc, rxmap);
			sc->rfa_headm = m_free(m);
		}
	} else if (sc->rx_bufs < bufs) {
		int err, tmp_rx_bufs = sc->rx_bufs;
		for (i = sc->rx_bufs; i < bufs; i++) {
			if ((err = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
			    MCLBYTES, 0, 0, &sc->sc_rxmaps[i])) != 0) {
				printf("%s: unable to create rx dma map %d, "
				  "error %d\n", sc->sc_dev.dv_xname, i, err);
				break;
			}
			sc->rx_bufs++;
		}
		for (i = tmp_rx_bufs; i < sc->rx_bufs; i++)
			if (fxp_add_rfabuf(sc, NULL) != 0)
				break;
	}
	fxp_scb_wait(sc);
	rxmap = *((bus_dmamap_t *)sc->rfa_headm->m_ext.ext_buf);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
	    rxmap->dm_segs[0].ds_addr + RFA_ALIGNMENT_FUDGE);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_RU_START);

	/*
	 * Set current media.
	 */
	mii_mediachg(&sc->sc_mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);

	/*
	 * Start stats updater.
	 */
	timeout_add(&sc->stats_update_to, hz);
}

/*
 * Change media according to request.
 */
int
fxp_mediachange(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;

	mii_mediachg(&sc->sc_mii);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
void
fxp_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct fxp_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

/*
 * Add a buffer to the end of the RFA buffer list.
 * Return 0 if successful, 1 for failure. A failure results in
 * adding the 'oldm' (if non-NULL) on to the end of the list -
 * tossing out its old contents and recycling it.
 * The RFA struct is stuck at the beginning of mbuf cluster and the
 * data pointer is fixed up to point just past it.
 */
int
fxp_add_rfabuf(sc, oldm)
	struct fxp_softc *sc;
	struct mbuf *oldm;
{
	u_int32_t v;
	struct mbuf *m;
	u_int8_t *rfap;
	bus_dmamap_t rxmap = NULL;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			if (oldm == NULL)
				return 1;
			m = oldm;
			m->m_data = m->m_ext.ext_buf;
		}
		if (oldm == NULL) {
			rxmap = FXP_RXMAP_GET(sc);
			*((bus_dmamap_t *)m->m_ext.ext_buf) = rxmap;
			bus_dmamap_load(sc->sc_dmat, rxmap,
			    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
			    BUS_DMA_NOWAIT);
		} else if (oldm == m)
			rxmap = *((bus_dmamap_t *)oldm->m_ext.ext_buf);
		else {
			rxmap = *((bus_dmamap_t *)oldm->m_ext.ext_buf);
			bus_dmamap_unload(sc->sc_dmat, rxmap);
			bus_dmamap_load(sc->sc_dmat, rxmap,
			    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
			    BUS_DMA_NOWAIT);
			*mtod(m, bus_dmamap_t *) = rxmap;
		}
	} else {
		if (oldm == NULL)
			return 1;
		m = oldm;
		m->m_data = m->m_ext.ext_buf;
		rxmap = *mtod(m, bus_dmamap_t *);
	}

	/*
	 * Move the data pointer up so that the incoming data packet
	 * will be 32-bit aligned.
	 */
	m->m_data += RFA_ALIGNMENT_FUDGE;

	/*
	 * Get a pointer to the base of the mbuf cluster and move
	 * data start past it.
	 */
	rfap = m->m_data;
	m->m_data += sizeof(struct fxp_rfa);
	*(u_int16_t *)(rfap + offsetof(struct fxp_rfa, size)) =
	    htole16(MCLBYTES - sizeof(struct fxp_rfa) - RFA_ALIGNMENT_FUDGE);

	/*
	 * Initialize the rest of the RFA.  Note that since the RFA
	 * is misaligned, we cannot store values directly.  Instead,
	 * we use an optimized, inline copy.
	 */
	*(u_int16_t *)(rfap + offsetof(struct fxp_rfa, rfa_status)) = 0;
	*(u_int16_t *)(rfap + offsetof(struct fxp_rfa, rfa_control)) =
	    htole16(FXP_RFA_CONTROL_EL);
	*(u_int16_t *)(rfap + offsetof(struct fxp_rfa, actual_size)) = 0;

	v = -1;
	fxp_lwcopy(&v,
	    (u_int32_t *)(rfap + offsetof(struct fxp_rfa, link_addr)));
	fxp_lwcopy(&v,
	    (u_int32_t *)(rfap + offsetof(struct fxp_rfa, rbd_addr)));

	bus_dmamap_sync(sc->sc_dmat, rxmap, 0, MCLBYTES,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	 * If there are other buffers already on the list, attach this
	 * one to the end by fixing up the tail to point to this one.
	 */
	if (sc->rfa_headm != NULL) {
		sc->rfa_tailm->m_next = m;
		v = htole32(rxmap->dm_segs[0].ds_addr + RFA_ALIGNMENT_FUDGE);
		rfap = sc->rfa_tailm->m_ext.ext_buf + RFA_ALIGNMENT_FUDGE;
		fxp_lwcopy(&v,
		    (u_int32_t *)(rfap + offsetof(struct fxp_rfa, link_addr)));
		*(u_int16_t *)(rfap + offsetof(struct fxp_rfa, rfa_control)) &=
		    htole16((u_int16_t)~FXP_RFA_CONTROL_EL);
		/* XXX we only need to sync the control struct */
		bus_dmamap_sync(sc->sc_dmat,
		    *((bus_dmamap_t *)sc->rfa_tailm->m_ext.ext_buf), 0,
			MCLBYTES, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	} else
		sc->rfa_headm = m;

	sc->rfa_tailm = m;

	return (m == oldm);
}

volatile int
fxp_mdi_read(self, phy, reg)
	struct device *self;
	int phy;
	int reg;
{
	struct fxp_softc *sc = (struct fxp_softc *)self;
	int count = 10000;
	int value;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_READ << 26) | (reg << 16) | (phy << 21));

	while (((value = CSR_READ_4(sc, FXP_CSR_MDICONTROL)) & 0x10000000) == 0
	    && count--)
		DELAY(10);

	if (count <= 0)
		printf("%s: fxp_mdi_read: timed out\n", sc->sc_dev.dv_xname);

	return (value & 0xffff);
}

void
fxp_statchg(self)
	struct device *self;
{
	struct fxp_softc *sc = (struct fxp_softc *)self;

	/*
	 * Determine whether or not we have to work-around the
	 * Resume Bug.
	 */
	if (sc->sc_flags & FXPF_HAS_RESUME_BUG) {
		if (IFM_TYPE(sc->sc_mii.mii_media_active) == IFM_10_T)
			sc->sc_flags |= FXPF_FIX_RESUME_BUG;
		else
			sc->sc_flags &= ~FXPF_FIX_RESUME_BUG;
	}
}

void
fxp_mdi_write(self, phy, reg, value)
	struct device *self;
	int phy;
	int reg;
	int value;
{
	struct fxp_softc *sc = (struct fxp_softc *)self;
	int count = 10000;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_WRITE << 26) | (reg << 16) | (phy << 21) |
	    (value & 0xffff));

	while((CSR_READ_4(sc, FXP_CSR_MDICONTROL) & 0x10000000) == 0 &&
	    count--)
		DELAY(10);

	if (count <= 0)
		printf("%s: fxp_mdi_write: timed out\n", sc->sc_dev.dv_xname);
}

int
fxp_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct fxp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			fxp_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			 register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			 if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
				    LLADDR(ifp->if_sadl);
			 else
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl),
				    ifp->if_addrlen);
			 /* Set new address. */
			 fxp_init(sc);
			 break;
		    }
#endif
		default:
			fxp_init(sc);
			break;
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN) {
			error = EINVAL;
		} else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

	case SIOCSIFFLAGS:
		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP)
			fxp_init(sc);
		else if (ifp->if_flags & IFF_RUNNING)
			fxp_stop(sc, 1);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			fxp_init(sc);
			error = 0;
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, command);
		break;

	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

/*
 * Program the multicast filter.
 *
 * We have an artificial restriction that the multicast setup command
 * must be the first command in the chain, so we take steps to ensure
 * this. By requiring this, it allows us to keep up the performance of
 * the pre-initialized command ring (esp. link pointers) by not actually
 * inserting the mcsetup command in the ring - i.e. its link pointer
 * points to the TxCB ring, but the mcsetup descriptor itself is not part
 * of it. We then can do 'CU_START' on the mcsetup descriptor and have it
 * lead into the regular TxCB ring when it completes.
 *
 * This function must be called at splimp.
 */
void
fxp_mc_setup(sc, doit)
	struct fxp_softc *sc;
	int doit;
{
	struct fxp_cb_mcs *mcsp = &sc->sc_ctrl->u.mcs;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct ether_multistep step;
	struct ether_multi *enm;
	int nmcasts;

	/*
	 * Initialize multicast setup descriptor.
	 */
	mcsp->cb_status = htole16(0);
	mcsp->cb_command = htole16(FXP_CB_COMMAND_MCAS | FXP_CB_COMMAND_EL);
	mcsp->link_addr = htole32(-1);

	nmcasts = 0;
	if (!(ifp->if_flags & IFF_ALLMULTI)) {
		ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
		while (enm != NULL) {
			if (nmcasts >= MAXMCADDR) {
				ifp->if_flags |= IFF_ALLMULTI;
				nmcasts = 0;
				break;
			}

			/* Punt on ranges. */
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
			    sizeof(enm->enm_addrlo)) != 0) {
				ifp->if_flags |= IFF_ALLMULTI;
				nmcasts = 0;
				break;
			}
			bcopy(enm->enm_addrlo,
			    (void *)&mcsp->mc_addr[nmcasts][0], ETHER_ADDR_LEN);
			nmcasts++;
			ETHER_NEXT_MULTI(step, enm);
		}
	}
	if (doit == 0)
		return;
	mcsp->mc_cnt = htole16(nmcasts * ETHER_ADDR_LEN);

	/*
	 * Wait until command unit is not active. This should never
	 * be the case when nothing is queued, but make sure anyway.
	 */
	while ((CSR_READ_1(sc, FXP_CSR_SCB_RUSCUS) >> 6) != FXP_SCB_CUS_IDLE);

	/*
	 * Start the multicast setup command.
	 */
	fxp_scb_wait(sc);
	FXP_MCS_SYNC(sc, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->tx_cb_map->dm_segs->ds_addr +
	    offsetof(struct fxp_ctrl, u.mcs));
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);

	do {
		DELAY(1);
		FXP_MCS_SYNC(sc, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	} while (!(mcsp->cb_status & htole16(FXP_CB_STATUS_C)));
}
