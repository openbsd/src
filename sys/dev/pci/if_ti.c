/*	$OpenBSD: if_ti.c,v 1.73 2005/10/10 20:54:23 brad Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 * $FreeBSD: src/sys/pci/if_ti.c,v 1.25 2000/01/18 00:26:29 wpaul Exp $
 */

/*
 * Alteon Networks Tigon PCI gigabit ethernet driver for FreeBSD.
 * Manuals, sample driver and firmware source kits are available
 * from http://www.alteon.com/support/openkits.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Alteon Networks Tigon chip contains an embedded R4000 CPU,
 * gigabit MAC, dual DMA channels and a PCI interface unit. NICs
 * using the Tigon may have anywhere from 512K to 2MB of SRAM. The
 * Tigon supports hardware IP, TCP and UCP checksumming, multicast
 * filtering and jumbo (9014 byte) frames. The hardware is largely
 * controlled by firmware, which must be loaded into the NIC during
 * initialization.
 *
 * The Tigon 2 contains 2 R4000 CPUs and requires a newer firmware
 * revision, which supports new features such as extended commands,
 * extended jumbo receive ring desciptors and a mini receive ring.
 *
 * Alteon Networks is to be commended for releasing such a vast amount
 * of development material for the Tigon NIC without requiring an NDA
 * (although they really should have done it a long time ago). With
 * any luck, the other vendors will finally wise up and follow Alteon's
 * stellar example.
 *
 * The firmware for the Tigon 1 and 2 NICs is compiled directly into
 * this driver by #including it as a C header file. This bloats the
 * driver somewhat, but it's the easiest method considering that the
 * driver code and firmware code need to be kept in sync. The source
 * for the firmware is not provided with the FreeBSD distribution since
 * compiling it requires a GNU toolchain targeted for mips-sgi-irix5.3.
 *
 * The following people deserve special thanks:
 * - Terry Murphy of 3Com, for providing a 3c985 Tigon 1 board
 *   for testing
 * - Raymond Lee of Netgear, for providing a pair of Netgear
 *   GA620 Tigon 2 boards for testing
 * - Ulf Zimmermann, for bringing the GA260 to my attention and
 *   convincing me to write this driver.
 * - Andrew Gallatin for providing FreeBSD/Alpha support.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_tireg.h>
#include <dev/pci/if_tivar.h>

int ti_probe(struct device *, void *, void *);
void ti_attach(struct device *, struct device *, void *);

struct cfattach ti_ca = {
	sizeof(struct ti_softc), ti_probe, ti_attach
};

struct cfdriver ti_cd = {
	0, "ti", DV_IFNET
};

void ti_txeof_tigon1(struct ti_softc *);
void ti_txeof_tigon2(struct ti_softc *);
void ti_rxeof(struct ti_softc *);

void ti_stats_update(struct ti_softc *);
int ti_encap_tigon1(struct ti_softc *, struct mbuf *, u_int32_t *);
int ti_encap_tigon2(struct ti_softc *, struct mbuf *, u_int32_t *);

int ti_intr(void *);
void ti_start(struct ifnet *);
int ti_ioctl(struct ifnet *, u_long, caddr_t);
void ti_init(void *);
void ti_init2(struct ti_softc *);
void ti_stop(struct ti_softc *);
void ti_watchdog(struct ifnet *);
void ti_shutdown(void *);
int ti_ifmedia_upd(struct ifnet *);
void ti_ifmedia_sts(struct ifnet *, struct ifmediareq *);

u_int32_t ti_eeprom_putbyte(struct ti_softc *, int);
u_int8_t ti_eeprom_getbyte(struct ti_softc *, int, u_int8_t *);
int ti_read_eeprom(struct ti_softc *, caddr_t, int, int);

void ti_add_mcast(struct ti_softc *, struct ether_addr *);
void ti_del_mcast(struct ti_softc *, struct ether_addr *);
void ti_setmulti(struct ti_softc *);

void ti_mem_read(struct ti_softc *, u_int32_t, u_int32_t, void *);
void ti_mem_write(struct ti_softc *, u_int32_t, u_int32_t, const void*);
void ti_mem_set(struct ti_softc *, u_int32_t, u_int32_t);
void ti_loadfw(struct ti_softc *);
void ti_cmd(struct ti_softc *, struct ti_cmd_desc *);
void ti_cmd_ext(struct ti_softc *, struct ti_cmd_desc *,
    caddr_t, int);
void ti_handle_events(struct ti_softc *);
int ti_alloc_jumbo_mem(struct ti_softc *);
void *ti_jalloc(struct ti_softc *);
void ti_jfree(caddr_t, u_int, void *);
int ti_newbuf_std(struct ti_softc *, int, struct mbuf *, bus_dmamap_t);
int ti_newbuf_mini(struct ti_softc *, int, struct mbuf *, bus_dmamap_t);
int ti_newbuf_jumbo(struct ti_softc *, int, struct mbuf *);
int ti_init_rx_ring_std(struct ti_softc *);
void ti_free_rx_ring_std(struct ti_softc *);
int ti_init_rx_ring_jumbo(struct ti_softc *);
void ti_free_rx_ring_jumbo(struct ti_softc *);
int ti_init_rx_ring_mini(struct ti_softc *);
void ti_free_rx_ring_mini(struct ti_softc *);
void ti_free_tx_ring(struct ti_softc *);
int ti_init_tx_ring(struct ti_softc *);

int ti_64bitslot_war(struct ti_softc *);
int ti_chipinit(struct ti_softc *);
int ti_gibinit(struct ti_softc *);

const struct pci_matchid ti_devices[] = {
	{ PCI_VENDOR_NETGEAR, PCI_PRODUCT_NETGEAR_GA620 },
	{ PCI_VENDOR_NETGEAR, PCI_PRODUCT_NETGEAR_GA620T },
	{ PCI_VENDOR_ALTEON, PCI_PRODUCT_ALTEON_ACENIC },
	{ PCI_VENDOR_ALTEON, PCI_PRODUCT_ALTEON_ACENICT },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C985 },
	{ PCI_VENDOR_SGI, PCI_PRODUCT_SGI_TIGON },
	{ PCI_VENDOR_DEC, PCI_PRODUCT_DEC_PN9000SX },
};

/*
 * Send an instruction or address to the EEPROM, check for ACK.
 */
u_int32_t
ti_eeprom_putbyte(struct ti_softc *sc, int byte)
{
	int		i, ack = 0;

	/*
	 * Make sure we're in TX mode.
	 */
	TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_TXEN);

	/*
	 * Feed in each bit and strobe the clock.
	 */
	for (i = 0x80; i; i >>= 1) {
		if (byte & i) {
			TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_DOUT);
		} else {
			TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_DOUT);
		}
		DELAY(1);
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
		DELAY(1);
		TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
	}

	/*
	 * Turn off TX mode.
	 */
	TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_TXEN);

	/*
	 * Check for ack.
	 */
	TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
	ack = CSR_READ_4(sc, TI_MISC_LOCAL_CTL) & TI_MLC_EE_DIN;
	TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);

	return (ack);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.'
 * We have to send two address bytes since the EEPROM can hold
 * more than 256 bytes of data.
 */
u_int8_t
ti_eeprom_getbyte(struct ti_softc *sc, int addr, u_int8_t *dest)
{
	int		i;
	u_int8_t		byte = 0;

	EEPROM_START;

	/*
	 * Send write control code to EEPROM.
	 */
	if (ti_eeprom_putbyte(sc, EEPROM_CTL_WRITE)) {
		printf("%s: failed to send write command, status: %x\n",
		    sc->sc_dv.dv_xname, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}

	/*
	 * Send first byte of address of byte we want to read.
	 */
	if (ti_eeprom_putbyte(sc, (addr >> 8) & 0xFF)) {
		printf("%s: failed to send address, status: %x\n",
		    sc->sc_dv.dv_xname, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}
	/*
	 * Send second byte address of byte we want to read.
	 */
	if (ti_eeprom_putbyte(sc, addr & 0xFF)) {
		printf("%s: failed to send address, status: %x\n",
		    sc->sc_dv.dv_xname, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}

	EEPROM_STOP;
	EEPROM_START;
	/*
	 * Send read control code to EEPROM.
	 */
	if (ti_eeprom_putbyte(sc, EEPROM_CTL_READ)) {
		printf("%s: failed to send read command, status: %x\n",
		    sc->sc_dv.dv_xname, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}

	/*
	 * Start reading bits from EEPROM.
	 */
	TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_TXEN);
	for (i = 0x80; i; i >>= 1) {
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
		DELAY(1);
		if (CSR_READ_4(sc, TI_MISC_LOCAL_CTL) & TI_MLC_EE_DIN)
			byte |= i;
		TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
		DELAY(1);
	}

	EEPROM_STOP;

	/*
	 * No ACK generated for read, so just return byte.
	 */

	*dest = byte;

	return (0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
int
ti_read_eeprom(struct ti_softc *sc, caddr_t dest, int off, int cnt)
{
	int			err = 0, i;
	u_int8_t		byte = 0;

	for (i = 0; i < cnt; i++) {
		err = ti_eeprom_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return (err ? 1 : 0);
}

/*
 * NIC memory read function.
 * Can be used to copy data from NIC local memory.
 */
void
ti_mem_read(struct ti_softc *sc, u_int32_t addr, u_int32_t len, void *buf)
{
	int			segptr, segsize, cnt;
	caddr_t			ptr;

	segptr = addr;
	cnt = len;
	ptr = buf;

	while(cnt) {
		if (cnt < TI_WINLEN)
			segsize = cnt;
		else
			segsize = TI_WINLEN - (segptr % TI_WINLEN);
		CSR_WRITE_4(sc, TI_WINBASE, (segptr & ~(TI_WINLEN - 1)));
		bus_space_read_region_4(sc->ti_btag, sc->ti_bhandle,
		    TI_WINDOW + (segptr & (TI_WINLEN - 1)), (u_int32_t *)ptr,
		    segsize / 4);
		ptr += segsize;
		segptr += segsize;
		cnt -= segsize;
	}
}

/*
 * NIC memory write function.
 * Can be used to copy data into  NIC local memory.
 */
void
ti_mem_write(struct ti_softc *sc, u_int32_t addr, u_int32_t len,
    const void *buf)
{
	int			segptr, segsize, cnt;
	const char		*ptr;

	segptr = addr;
	cnt = len;
	ptr = buf;

	while(cnt) {
		if (cnt < TI_WINLEN)
			segsize = cnt;
		else
			segsize = TI_WINLEN - (segptr % TI_WINLEN);
		CSR_WRITE_4(sc, TI_WINBASE, (segptr & ~(TI_WINLEN - 1)));
		bus_space_write_region_4(sc->ti_btag, sc->ti_bhandle,
		    TI_WINDOW + (segptr & (TI_WINLEN - 1)), (u_int32_t *)ptr,
		    segsize / 4);
		ptr += segsize;
		segptr += segsize;
		cnt -= segsize;
	}
}

/*
 * NIC memory write function.
 * Can be used to clear a section of NIC local memory.
 */
void
ti_mem_set(struct ti_softc *sc, u_int32_t addr, u_int32_t len)
{
	int			segptr, segsize, cnt;

	segptr = addr;
	cnt = len;

	while(cnt) {
		if (cnt < TI_WINLEN)
			segsize = cnt;
		else
			segsize = TI_WINLEN - (segptr % TI_WINLEN);
		CSR_WRITE_4(sc, TI_WINBASE, (segptr & ~(TI_WINLEN - 1)));
		bus_space_set_region_4(sc->ti_btag, sc->ti_bhandle,
		    TI_WINDOW + (segptr & (TI_WINLEN - 1)), 0, segsize / 4);
		segptr += segsize;
		cnt -= segsize;
	}
}

/*
 * Load firmware image into the NIC. Check that the firmware revision
 * is acceptable and see if we want the firmware for the Tigon 1 or
 * Tigon 2.
 */
void
ti_loadfw(struct ti_softc *sc)
{
	struct tigon_firmware *tf;
	u_char *buf = NULL;
	size_t buflen;
	char *name;
	int error;

	switch(sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		name = "tigon1";
		break;
	case TI_HWREV_TIGON_II:
		name = "tigon2";
		break;
	default:
		printf("%s: can't load firmware: unknown hardware rev\n",
		    sc->sc_dv.dv_xname);
		return;
	}
	
	error = loadfirmware(name, &buf, &buflen);
	if (error)
		return;
	tf = (struct tigon_firmware *)buf;
	if (tf->FwReleaseMajor != TI_FIRMWARE_MAJOR ||
	    tf->FwReleaseMinor != TI_FIRMWARE_MINOR ||
	    tf->FwReleaseFix != TI_FIRMWARE_FIX) {
		printf("%s: firmware revision mismatch; want "
		    "%d.%d.%d, got %d.%d.%d\n", sc->sc_dv.dv_xname,
		    TI_FIRMWARE_MAJOR, TI_FIRMWARE_MINOR,
		    TI_FIRMWARE_FIX, tf->FwReleaseMajor,
		    tf->FwReleaseMinor, tf->FwReleaseFix);
		free(buf, M_DEVBUF);
		return;
	}
	ti_mem_write(sc, tf->FwTextAddr, tf->FwTextLen,
	    (caddr_t)&tf->data[tf->FwTextOffset]);
	ti_mem_write(sc, tf->FwRodataAddr, tf->FwRodataLen,
	    (caddr_t)&tf->data[tf->FwRodataOffset]);
	ti_mem_write(sc, tf->FwDataAddr, tf->FwDataLen,
	    (caddr_t)&tf->data[tf->FwDataOffset]);
	ti_mem_set(sc, tf->FwBssAddr, tf->FwBssLen);
	ti_mem_set(sc, tf->FwSbssAddr, tf->FwSbssLen);
	CSR_WRITE_4(sc, TI_CPU_PROGRAM_COUNTER, tf->FwStartAddr);
	free(buf, M_DEVBUF);
}

/*
 * Send the NIC a command via the command ring.
 */
void
ti_cmd(struct ti_softc *sc, struct ti_cmd_desc *cmd)
{
	u_int32_t		index;

	index = sc->ti_cmd_saved_prodidx;
	CSR_WRITE_4(sc, TI_GCR_CMDRING + (index * 4), *(u_int32_t *)(cmd));
	TI_INC(index, TI_CMD_RING_CNT);
	CSR_WRITE_4(sc, TI_MB_CMDPROD_IDX, index);
	sc->ti_cmd_saved_prodidx = index;
}

/*
 * Send the NIC an extended command. The 'len' parameter specifies the
 * number of command slots to include after the initial command.
 */
void
ti_cmd_ext(struct ti_softc *sc, struct ti_cmd_desc *cmd, caddr_t arg,
    int len)
{
	u_int32_t		index;
	int		i;

	index = sc->ti_cmd_saved_prodidx;
	CSR_WRITE_4(sc, TI_GCR_CMDRING + (index * 4), *(u_int32_t *)(cmd));
	TI_INC(index, TI_CMD_RING_CNT);
	for (i = 0; i < len; i++) {
		CSR_WRITE_4(sc, TI_GCR_CMDRING + (index * 4),
		    *(u_int32_t *)(&arg[i * 4]));
		TI_INC(index, TI_CMD_RING_CNT);
	}
	CSR_WRITE_4(sc, TI_MB_CMDPROD_IDX, index);
	sc->ti_cmd_saved_prodidx = index;
}

/*
 * Handle events that have triggered interrupts.
 */
void
ti_handle_events(struct ti_softc *sc)
{
	struct ti_event_desc	*e;

	if (sc->ti_rdata->ti_event_ring == NULL)
		return;

	while (sc->ti_ev_saved_considx != sc->ti_ev_prodidx.ti_idx) {
		e = &sc->ti_rdata->ti_event_ring[sc->ti_ev_saved_considx];
		switch (TI_EVENT_EVENT(e)) {
		case TI_EV_LINKSTAT_CHANGED:
			sc->ti_linkstat = TI_EVENT_CODE(e);
			break;
		case TI_EV_ERROR:
			if (TI_EVENT_CODE(e) == TI_EV_CODE_ERR_INVAL_CMD)
				printf("%s: invalid command\n",
				    sc->sc_dv.dv_xname);
			else if (TI_EVENT_CODE(e) == TI_EV_CODE_ERR_UNIMP_CMD)
				printf("%s: unknown command\n",
				    sc->sc_dv.dv_xname);
			else if (TI_EVENT_CODE(e) == TI_EV_CODE_ERR_BADCFG)
				printf("%s: bad config data\n",
				    sc->sc_dv.dv_xname);
			break;
		case TI_EV_FIRMWARE_UP:
			ti_init2(sc);
			break;
		case TI_EV_STATS_UPDATED:
			ti_stats_update(sc);
			break;
		case TI_EV_RESET_JUMBO_RING:
		case TI_EV_MCAST_UPDATED:
			/* Who cares. */
			break;
		default:
			printf("%s: unknown event: %d\n", sc->sc_dv.dv_xname,
			       TI_EVENT_EVENT(e));
			break;
		}
		/* Advance the consumer index. */
		TI_INC(sc->ti_ev_saved_considx, TI_EVENT_RING_CNT);
		CSR_WRITE_4(sc, TI_GCR_EVENTCONS_IDX, sc->ti_ev_saved_considx);
	}
}

/*
 * Memory management for the jumbo receive ring is a pain in the
 * butt. We need to allocate at least 9018 bytes of space per frame,
 * _and_ it has to be contiguous (unless you use the extended
 * jumbo descriptor format). Using malloc() all the time won't
 * work: malloc() allocates memory in powers of two, which means we
 * would end up wasting a considerable amount of space by allocating
 * 9K chunks. We don't have a jumbo mbuf cluster pool. Thus, we have
 * to do our own memory management.
 *
 * The driver needs to allocate a contiguous chunk of memory at boot
 * time. We then chop this up ourselves into 9K pieces and use them
 * as external mbuf storage.
 *
 * One issue here is how much memory to allocate. The jumbo ring has
 * 256 slots in it, but at 9K per slot than can consume over 2MB of
 * RAM. This is a bit much, especially considering we also need
 * RAM for the standard ring and mini ring (on the Tigon 2). To
 * save space, we only actually allocate enough memory for 64 slots
 * by default, which works out to between 500 and 600K. This can
 * be tuned by changing a #define in if_tireg.h.
 */

int
ti_alloc_jumbo_mem(struct ti_softc *sc)
{
	caddr_t ptr, kva;
	bus_dma_segment_t seg;
	int i, rseg, state, error;
	struct ti_jpool_entry *entry;

	state = error = 0;

	/* Grab a big chunk o' storage. */
	if (bus_dmamem_alloc(sc->sc_dmatag, TI_JMEM, PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", sc->sc_dv.dv_xname);
		return (ENOBUFS);
	}

	state = 1;
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg, TI_JMEM, &kva,
	    BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		    sc->sc_dv.dv_xname, TI_JMEM);
		error = ENOBUFS;
		goto out;
	}

	state = 2;
	if (bus_dmamap_create(sc->sc_dmatag, TI_JMEM, 1, TI_JMEM, 0,
	    BUS_DMA_NOWAIT, &sc->ti_cdata.ti_rx_jumbo_map)) {
		printf("%s: can't create dma map\n", sc->sc_dv.dv_xname);
		error = ENOBUFS;
		goto out;
	}

	state = 3;
	if (bus_dmamap_load(sc->sc_dmatag, sc->ti_cdata.ti_rx_jumbo_map, kva,
	    TI_JMEM, NULL, BUS_DMA_NOWAIT)) {
		printf("%s: can't load dma map\n", sc->sc_dv.dv_xname);
		error = ENOBUFS;
		goto out;
	}

	state = 4;
	sc->ti_cdata.ti_jumbo_buf = (caddr_t)kva;

	SLIST_INIT(&sc->ti_jfree_listhead);
	SLIST_INIT(&sc->ti_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc->ti_cdata.ti_jumbo_buf;
	for (i = 0; i < TI_JSLOTS; i++) {
		sc->ti_cdata.ti_jslots[i].ti_buf = ptr;
		sc->ti_cdata.ti_jslots[i].ti_inuse = 0;
		ptr += TI_JLEN;
		entry = malloc(sizeof(struct ti_jpool_entry),
			       M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			sc->ti_cdata.ti_jumbo_buf = NULL;
			printf("%s: no memory for jumbo buffer queue\n",
			    sc->sc_dv.dv_xname);
			error = ENOBUFS;
			goto out;
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc->ti_jfree_listhead, entry, jpool_entries);
	}
out:
	if (error != 0) {
		switch (state) {
		case 4:
			bus_dmamap_unload(sc->sc_dmatag,
			    sc->ti_cdata.ti_rx_jumbo_map);
		case 3:
			bus_dmamap_destroy(sc->sc_dmatag,
			    sc->ti_cdata.ti_rx_jumbo_map);
		case 2:
			bus_dmamem_unmap(sc->sc_dmatag, kva, TI_JMEM);
		case 1:
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			break;
		default:
			break;
		}
	}

	return (error);
}

/*
 * Allocate a jumbo buffer.
 */
void *
ti_jalloc(struct ti_softc *sc)
{
	struct ti_jpool_entry   *entry;

	entry = SLIST_FIRST(&sc->ti_jfree_listhead);

	if (entry == NULL)
		return (NULL);

	SLIST_REMOVE_HEAD(&sc->ti_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->ti_jinuse_listhead, entry, jpool_entries);
	sc->ti_cdata.ti_jslots[entry->slot].ti_inuse = 1;
	return (sc->ti_cdata.ti_jslots[entry->slot].ti_buf);
}

/*
 * Release a jumbo buffer.
 */
void
ti_jfree(caddr_t buf, u_int size, void *arg)
{
	struct ti_softc		*sc;
	int			i;
	struct ti_jpool_entry	*entry;

	/* Extract the softc struct pointer. */
	sc = (struct ti_softc *)arg;

	if (sc == NULL)
		panic("ti_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */
	i = ((vaddr_t)buf - (vaddr_t)sc->ti_cdata.ti_jumbo_buf) / TI_JLEN;

	if ((i < 0) || (i >= TI_JSLOTS))
		panic("ti_jfree: asked to free buffer that we don't manage!");
	else if (sc->ti_cdata.ti_jslots[i].ti_inuse == 0)
		panic("ti_jfree: buffer already free!");

	sc->ti_cdata.ti_jslots[i].ti_inuse--;
	if(sc->ti_cdata.ti_jslots[i].ti_inuse == 0) {
		entry = SLIST_FIRST(&sc->ti_jinuse_listhead);
		if (entry == NULL)
			panic("ti_jfree: buffer not in use!");
		entry->slot = i;
		SLIST_REMOVE_HEAD(&sc->ti_jinuse_listhead, jpool_entries);
		SLIST_INSERT_HEAD(&sc->ti_jfree_listhead,
				  entry, jpool_entries);
	}
}

/*
 * Intialize a standard receive ring descriptor.
 */
int
ti_newbuf_std(struct ti_softc *sc, int i, struct mbuf *m,
    bus_dmamap_t dmamap)
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (dmamap == NULL) {
		/* if (m) panic() */

		if (bus_dmamap_create(sc->sc_dmatag, MCLBYTES, 1, MCLBYTES,
				      0, BUS_DMA_NOWAIT, &dmamap)) {
			printf("%s: can't create recv map\n",
			       sc->sc_dv.dv_xname);
			return (ENOMEM);
		}
	} else if (m == NULL)
		bus_dmamap_unload(sc->sc_dmatag, dmamap);

	sc->ti_cdata.ti_rx_std_map[i] = dmamap;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

		m_adj(m_new, ETHER_ALIGN);

		if (bus_dmamap_load_mbuf(sc->sc_dmatag, dmamap, m_new,
					 BUS_DMA_NOWAIT))
			return (ENOBUFS);

	} else {
		/*
		 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
		m_adj(m_new, ETHER_ALIGN);
	}

	sc->ti_cdata.ti_rx_std_chain[i] = m_new;
	r = &sc->ti_rdata->ti_rx_std_ring[i];
	TI_HOSTADDR(r->ti_addr) = dmamap->dm_segs[0].ds_addr;
	r->ti_type = TI_BDTYPE_RECV_BD;
	r->ti_flags = TI_BDFLAG_IP_CKSUM;
	r->ti_len = dmamap->dm_segs[0].ds_len;
	r->ti_idx = i;

	if ((dmamap->dm_segs[0].ds_addr & ~(MCLBYTES - 1)) !=
	    ((dmamap->dm_segs[0].ds_addr + dmamap->dm_segs[0].ds_len - 1) & 
	     ~(MCLBYTES - 1)))
	    panic("%s: overwritten!!!", sc->sc_dv.dv_xname);

	return (0);
}

/*
 * Intialize a mini receive ring descriptor. This only applies to
 * the Tigon 2.
 */
int
ti_newbuf_mini(struct ti_softc *sc, int i, struct mbuf *m,
    bus_dmamap_t dmamap)
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (dmamap == NULL) {
		/* if (m) panic() */

		if (bus_dmamap_create(sc->sc_dmatag, MHLEN, 1, MHLEN,
				      0, BUS_DMA_NOWAIT, &dmamap)) {
			printf("%s: can't create recv map\n",
			       sc->sc_dv.dv_xname);
			return (ENOMEM);
		}
	} else if (m == NULL)
		bus_dmamap_unload(sc->sc_dmatag, dmamap);

	sc->ti_cdata.ti_rx_mini_map[i] = dmamap;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);
		m_new->m_len = m_new->m_pkthdr.len = MHLEN;
		m_adj(m_new, ETHER_ALIGN);

		if (bus_dmamap_load_mbuf(sc->sc_dmatag, dmamap, m_new,
					 BUS_DMA_NOWAIT))
		return (ENOBUFS);

	} else {
		/*
		 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_data = m_new->m_pktdat;
		m_new->m_len = m_new->m_pkthdr.len = MHLEN;
	}

	r = &sc->ti_rdata->ti_rx_mini_ring[i];
	sc->ti_cdata.ti_rx_mini_chain[i] = m_new;
	TI_HOSTADDR(r->ti_addr) = dmamap->dm_segs[0].ds_addr;
	r->ti_type = TI_BDTYPE_RECV_BD;
	r->ti_flags = TI_BDFLAG_MINI_RING | TI_BDFLAG_IP_CKSUM;
	r->ti_len = dmamap->dm_segs[0].ds_len;
	r->ti_idx = i;

	return (0);
}

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
int
ti_newbuf_jumbo(struct ti_softc *sc, int i, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (m == NULL) {
		caddr_t			buf = NULL;

		/* Allocate the mbuf. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);

		/* Allocate the jumbo buffer */
		buf = ti_jalloc(sc);
		if (buf == NULL) {
			m_freem(m_new);
			return (ENOBUFS);
		}

		/* Attach the buffer to the mbuf. */
		m_new->m_len = m_new->m_pkthdr.len = ETHER_MAX_LEN_JUMBO;
		MEXTADD(m_new, buf, ETHER_MAX_LEN_JUMBO, 0, ti_jfree, sc);
	} else {
		/*
		 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_data = m_new->m_ext.ext_buf;
		m_new->m_ext.ext_size = ETHER_MAX_LEN_JUMBO;
	}

	m_adj(m_new, ETHER_ALIGN);
	/* Set up the descriptor. */
	r = &sc->ti_rdata->ti_rx_jumbo_ring[i];
	sc->ti_cdata.ti_rx_jumbo_chain[i] = m_new;
	TI_HOSTADDR(r->ti_addr) = TI_JUMBO_DMA_ADDR(sc, m_new);
	r->ti_type = TI_BDTYPE_RECV_JUMBO_BD;
	r->ti_flags = TI_BDFLAG_JUMBO_RING | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return (0);
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB of memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
int
ti_init_rx_ring_std(struct ti_softc *sc)
{
	int		i;
	struct ti_cmd_desc	cmd;

	for (i = 0; i < TI_SSLOTS; i++) {
		if (ti_newbuf_std(sc, i, NULL, 0) == ENOBUFS)
			return (ENOBUFS);
	}

	TI_UPDATE_STDPROD(sc, i - 1);
	sc->ti_std = i - 1;

	return (0);
}

void
ti_free_rx_ring_std(struct ti_softc *sc)
{
	int		i;

	for (i = 0; i < TI_STD_RX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_rx_std_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_rx_std_chain[i]);
			sc->ti_cdata.ti_rx_std_chain[i] = NULL;
			bus_dmamap_destroy(sc->sc_dmatag,
					   sc->ti_cdata.ti_rx_std_map[i]);
			sc->ti_cdata.ti_rx_std_map[i] = 0;
		}
		bzero((char *)&sc->ti_rdata->ti_rx_std_ring[i],
		    sizeof(struct ti_rx_desc));
	}
}

int
ti_init_rx_ring_jumbo(struct ti_softc *sc)
{
	int		i;
	struct ti_cmd_desc	cmd;

	for (i = 0; i < TI_JUMBO_RX_RING_CNT; i++) {
		if (ti_newbuf_jumbo(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	};

	TI_UPDATE_JUMBOPROD(sc, i - 1);
	sc->ti_jumbo = i - 1;

	return (0);
}

void
ti_free_rx_ring_jumbo(struct ti_softc *sc)
{
	int		i;

	for (i = 0; i < TI_JUMBO_RX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_rx_jumbo_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_rx_jumbo_chain[i]);
			sc->ti_cdata.ti_rx_jumbo_chain[i] = NULL;
		}
		bzero((char *)&sc->ti_rdata->ti_rx_jumbo_ring[i],
		    sizeof(struct ti_rx_desc));
	}
}

int
ti_init_rx_ring_mini(struct ti_softc *sc)
{
	int		i;

	for (i = 0; i < TI_MSLOTS; i++) {
		if (ti_newbuf_mini(sc, i, NULL, 0) == ENOBUFS)
			return (ENOBUFS);
	};

	TI_UPDATE_MINIPROD(sc, i - 1);
	sc->ti_mini = i - 1;

	return (0);
}

void
ti_free_rx_ring_mini(struct ti_softc *sc)
{
	int		i;

	for (i = 0; i < TI_MINI_RX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_rx_mini_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_rx_mini_chain[i]);
			sc->ti_cdata.ti_rx_mini_chain[i] = NULL;
			bus_dmamap_destroy(sc->sc_dmatag,
					   sc->ti_cdata.ti_rx_mini_map[i]);
			sc->ti_cdata.ti_rx_mini_map[i] = 0;
		}
		bzero((char *)&sc->ti_rdata->ti_rx_mini_ring[i],
		    sizeof(struct ti_rx_desc));
	}
}

void
ti_free_tx_ring(struct ti_softc *sc)
{
	int		i;
	struct ti_txmap_entry *entry;

	if (sc->ti_rdata->ti_tx_ring == NULL)
		return;

	for (i = 0; i < TI_TX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_tx_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_tx_chain[i]);
			sc->ti_cdata.ti_tx_chain[i] = NULL;
			SLIST_INSERT_HEAD(&sc->ti_tx_map_listhead,
					    sc->ti_cdata.ti_tx_map[i], link);
			sc->ti_cdata.ti_tx_map[i] = 0;
		}
		bzero((char *)&sc->ti_rdata->ti_tx_ring[i],
		    sizeof(struct ti_tx_desc));
	}

	while ((entry = SLIST_FIRST(&sc->ti_tx_map_listhead))) {
		SLIST_REMOVE_HEAD(&sc->ti_tx_map_listhead, link);
		bus_dmamap_destroy(sc->sc_dmatag, entry->dmamap);
		free(entry, M_DEVBUF);
	}
}

int
ti_init_tx_ring(struct ti_softc *sc)
{
	int i;
	bus_dmamap_t dmamap;
	struct ti_txmap_entry *entry;

	sc->ti_txcnt = 0;
	sc->ti_tx_saved_considx = 0;
	CSR_WRITE_4(sc, TI_MB_SENDPROD_IDX, 0);

	SLIST_INIT(&sc->ti_tx_map_listhead);
	for (i = 0; i < TI_TX_RING_CNT; i++) {
		if (bus_dmamap_create(sc->sc_dmatag, ETHER_MAX_LEN_JUMBO,
		    TI_NTXSEG, MCLBYTES, 0, BUS_DMA_NOWAIT, &dmamap))
			return (ENOBUFS);

		entry = malloc(sizeof(*entry), M_DEVBUF, M_NOWAIT);
		if (!entry) {
			bus_dmamap_destroy(sc->sc_dmatag, dmamap);
			return (ENOBUFS);
		}
		entry->dmamap = dmamap;
		SLIST_INSERT_HEAD(&sc->ti_tx_map_listhead, entry, link);
	}

	return (0);
}

/*
 * The Tigon 2 firmware has a new way to add/delete multicast addresses,
 * but we have to support the old way too so that Tigon 1 cards will
 * work.
 */
void
ti_add_mcast(struct ti_softc *sc, struct ether_addr *addr)
{
	struct ti_cmd_desc	cmd;
	u_int16_t		*m;
	u_int32_t		ext[2] = {0, 0};

	m = (u_int16_t *)&addr->ether_addr_octet[0];

	switch(sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		CSR_WRITE_4(sc, TI_GCR_MAR0, htons(m[0]));
		CSR_WRITE_4(sc, TI_GCR_MAR1, (htons(m[1]) << 16) | htons(m[2]));
		TI_DO_CMD(TI_CMD_ADD_MCAST_ADDR, 0, 0);
		break;
	case TI_HWREV_TIGON_II:
		ext[0] = htons(m[0]);
		ext[1] = (htons(m[1]) << 16) | htons(m[2]);
		TI_DO_CMD_EXT(TI_CMD_EXT_ADD_MCAST, 0, 0, (caddr_t)&ext, 2);
		break;
	default:
		printf("%s: unknown hwrev\n", sc->sc_dv.dv_xname);
		break;
	}
}

void
ti_del_mcast(struct ti_softc *sc, struct ether_addr *addr)
{
	struct ti_cmd_desc	cmd;
	u_int16_t		*m;
	u_int32_t		ext[2] = {0, 0};

	m = (u_int16_t *)&addr->ether_addr_octet[0];

	switch(sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		CSR_WRITE_4(sc, TI_GCR_MAR0, htons(m[0]));
		CSR_WRITE_4(sc, TI_GCR_MAR1, (htons(m[1]) << 16) | htons(m[2]));
		TI_DO_CMD(TI_CMD_DEL_MCAST_ADDR, 0, 0);
		break;
	case TI_HWREV_TIGON_II:
		ext[0] = htons(m[0]);
		ext[1] = (htons(m[1]) << 16) | htons(m[2]);
		TI_DO_CMD_EXT(TI_CMD_EXT_DEL_MCAST, 0, 0, (caddr_t)&ext, 2);
		break;
	default:
		printf("%s: unknown hwrev\n", sc->sc_dv.dv_xname);
		break;
	}
}

/*
 * Configure the Tigon's multicast address filter.
 *
 * The actual multicast table management is a bit of a pain, thanks to
 * slight brain damage on the part of both Alteon and us. With our
 * multicast code, we are only alerted when the multicast address table
 * changes and at that point we only have the current list of addresses:
 * we only know the current state, not the previous state, so we don't
 * actually know what addresses were removed or added. The firmware has
 * state, but we can't get our grubby mits on it, and there is no 'delete
 * all multicast addresses' command. Hence, we have to maintain our own
 * state so we know what addresses have been programmed into the NIC at
 * any given time.
 */
void
ti_setmulti(struct ti_softc *sc)
{
	struct ifnet		*ifp;
	struct arpcom		*ac = &sc->arpcom;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	struct ti_cmd_desc	cmd;
	struct ti_mc_entry	*mc;
	u_int32_t		intrs;
 
	ifp = &sc->arpcom.ac_if;

allmulti:
	if (ifp->if_flags & IFF_ALLMULTI) {
		TI_DO_CMD(TI_CMD_SET_ALLMULTI, TI_CMD_CODE_ALLMULTI_ENB, 0);
		return;
	} else {
		TI_DO_CMD(TI_CMD_SET_ALLMULTI, TI_CMD_CODE_ALLMULTI_DIS, 0);
	}

	/* Disable interrupts. */
	intrs = CSR_READ_4(sc, TI_MB_HOSTINTR);
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	/* First, zot all the existing filters. */
	while (SLIST_FIRST(&sc->ti_mc_listhead) != NULL) {
		mc = SLIST_FIRST(&sc->ti_mc_listhead);
		ti_del_mcast(sc, &mc->mc_addr);
		SLIST_REMOVE_HEAD(&sc->ti_mc_listhead, mc_entries);
		free(mc, M_DEVBUF);
	}

	/* Now program new ones. */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/* Re-enable interrupts. */
			CSR_WRITE_4(sc, TI_MB_HOSTINTR, intrs);

			ifp->if_flags |= IFF_ALLMULTI;
			goto allmulti;
		}
		mc = malloc(sizeof(struct ti_mc_entry), M_DEVBUF, M_NOWAIT);
		if (mc == NULL)
			panic("ti_setmulti");
		bcopy(enm->enm_addrlo, (char *)&mc->mc_addr, ETHER_ADDR_LEN);
		SLIST_INSERT_HEAD(&sc->ti_mc_listhead, mc, mc_entries);
		ti_add_mcast(sc, &mc->mc_addr);
		ETHER_NEXT_MULTI(step, enm);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, intrs);
}

/*
 * Check to see if the BIOS has configured us for a 64 bit slot when
 * we aren't actually in one. If we detect this condition, we can work
 * around it on the Tigon 2 by setting a bit in the PCI state register,
 * but for the Tigon 1 we must give up and abort the interface attach.
 */
int
ti_64bitslot_war(struct ti_softc *sc)
{
	if (!(CSR_READ_4(sc, TI_PCI_STATE) & TI_PCISTATE_32BIT_BUS)) {
		CSR_WRITE_4(sc, 0x600, 0);
		CSR_WRITE_4(sc, 0x604, 0);
		CSR_WRITE_4(sc, 0x600, 0x5555AAAA);
		if (CSR_READ_4(sc, 0x604) == 0x5555AAAA) {
			if (sc->ti_hwrev == TI_HWREV_TIGON)
				return (EINVAL);
			else {
				TI_SETBIT(sc, TI_PCI_STATE,
				    TI_PCISTATE_32BIT_BUS);
				return (0);
			}
		}
	}

	return (0);
}

/*
 * Do endian, PCI and DMA initialization. Also check the on-board ROM
 * self-test results.
 */
int
ti_chipinit(struct ti_softc *sc)
{
	u_int32_t		cacheline;
	u_int32_t		pci_writemax = 0;
	u_int32_t		chip_rev;

	/* Initialize link to down state. */
	sc->ti_linkstat = TI_EV_CODE_LINK_DOWN;

	/* Set endianness before we access any non-PCI registers. */
#if 0 && BYTE_ORDER == BIG_ENDIAN
	CSR_WRITE_4(sc, TI_MISC_HOST_CTL,
	    TI_MHC_BIGENDIAN_INIT | (TI_MHC_BIGENDIAN_INIT << 24));
#else
	CSR_WRITE_4(sc, TI_MISC_HOST_CTL,
	    TI_MHC_LITTLEENDIAN_INIT | (TI_MHC_LITTLEENDIAN_INIT << 24));
#endif

	/* Check the ROM failed bit to see if self-tests passed. */
	if (CSR_READ_4(sc, TI_CPU_STATE) & TI_CPUSTATE_ROMFAIL) {
		printf("%s: board self-diagnostics failed!\n",
		    sc->sc_dv.dv_xname);
		return (ENODEV);
	}

	/* Halt the CPU. */
	TI_SETBIT(sc, TI_CPU_STATE, TI_CPUSTATE_HALT);

	/* Figure out the hardware revision. */
	chip_rev = CSR_READ_4(sc, TI_MISC_HOST_CTL) & TI_MHC_CHIP_REV_MASK;
	switch(chip_rev) {
	case TI_REV_TIGON_I:
		sc->ti_hwrev = TI_HWREV_TIGON;
		break;
	case TI_REV_TIGON_II:
		sc->ti_hwrev = TI_HWREV_TIGON_II;
		break;
	default:
		printf("\n");
		printf("%s: unsupported chip revision: %x\n",
		    chip_rev, sc->sc_dv.dv_xname);
		return (ENODEV);
	}

	/* Do special setup for Tigon 2. */
	if (sc->ti_hwrev == TI_HWREV_TIGON_II) {
		TI_SETBIT(sc, TI_CPU_CTL_B, TI_CPUSTATE_HALT);
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_SRAM_BANK_512K);
		TI_SETBIT(sc, TI_MISC_CONF, TI_MCR_SRAM_SYNCHRONOUS);
	}

	/* Set up the PCI state register. */
	CSR_WRITE_4(sc, TI_PCI_STATE, TI_PCI_READ_CMD|TI_PCI_WRITE_CMD);
	if (sc->ti_hwrev == TI_HWREV_TIGON_II) {
		TI_SETBIT(sc, TI_PCI_STATE, TI_PCISTATE_USE_MEM_RD_MULT);
	}

	/* Clear the read/write max DMA parameters. */
	TI_CLRBIT(sc, TI_PCI_STATE, (TI_PCISTATE_WRITE_MAXDMA|
	    TI_PCISTATE_READ_MAXDMA));

	/* Get cache line size. */
	cacheline = CSR_READ_4(sc, TI_PCI_BIST) & 0xFF;

	/*
	 * If the system has set enabled the PCI memory write
	 * and invalidate command in the command register, set
	 * the write max parameter accordingly. This is necessary
	 * to use MWI with the Tigon 2.
	 */
	if (CSR_READ_4(sc, TI_PCI_CMDSTAT) & PCI_COMMAND_INVALIDATE_ENABLE) {
		switch(cacheline) {
		case 1:
		case 4:
		case 8:
		case 16:
		case 32:
		case 64:
			break;
		default:
		/* Disable PCI memory write and invalidate. */
			CSR_WRITE_4(sc, TI_PCI_CMDSTAT, CSR_READ_4(sc,
			    TI_PCI_CMDSTAT) & ~PCI_COMMAND_INVALIDATE_ENABLE);
			break;
		}
	}

#ifdef __brokenalpha__
	/*
	 * From the Alteon sample driver:
	 * Must insure that we do not cross an 8K (bytes) boundary
	 * for DMA reads.  Our highest limit is 1K bytes.  This is a
	 * restriction on some ALPHA platforms with early revision
	 * 21174 PCI chipsets, such as the AlphaPC 164lx
	 */
	TI_SETBIT(sc, TI_PCI_STATE, pci_writemax|TI_PCI_READMAX_1024);
#else
	TI_SETBIT(sc, TI_PCI_STATE, pci_writemax);
#endif

	/* This sets the min dma param all the way up (0xff). */
	TI_SETBIT(sc, TI_PCI_STATE, TI_PCISTATE_MINDMA);

	/* Configure DMA variables. */
#if BYTE_ORDER == BIG_ENDIAN
	CSR_WRITE_4(sc, TI_GCR_OPMODE, TI_OPMODE_BYTESWAP_BD |
	    TI_OPMODE_BYTESWAP_DATA | TI_OPMODE_WORDSWAP_BD |
	    TI_OPMODE_WARN_ENB | TI_OPMODE_FATAL_ENB |
	    TI_OPMODE_DONT_FRAG_JUMBO);
#else
	CSR_WRITE_4(sc, TI_GCR_OPMODE, TI_OPMODE_BYTESWAP_DATA|
	    TI_OPMODE_WORDSWAP_BD|TI_OPMODE_DONT_FRAG_JUMBO|
	    TI_OPMODE_WARN_ENB|TI_OPMODE_FATAL_ENB);
#endif

	/* Recommended settings from Tigon manual. */
	CSR_WRITE_4(sc, TI_GCR_DMA_WRITECFG, TI_DMA_STATE_THRESH_8W);
	CSR_WRITE_4(sc, TI_GCR_DMA_READCFG, TI_DMA_STATE_THRESH_8W);

	if (ti_64bitslot_war(sc)) {
		printf("%s: bios thinks we're in a 64 bit slot, "
		    "but we aren't", sc->sc_dv.dv_xname);
		return (EINVAL);
	}

	return (0);
}

/*
 * Initialize the general information block and firmware, and
 * start the CPU(s) running.
 */
int
ti_gibinit(struct ti_softc *sc)
{
	struct ti_rcb		*rcb;
	int			i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Disable interrupts for now. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	/*
	 * Tell the chip where to find the general information block.
	 * While this struct could go into >4GB memory, we allocate it in a
	 * single slab with the other descriptors, and those don't seem to
	 * support being located in a 64-bit region.
	 */
	CSR_WRITE_4(sc, TI_GCR_GENINFO_HI, 0);
	CSR_WRITE_4(sc, TI_GCR_GENINFO_LO,
		    TI_RING_DMA_ADDR(sc, ti_info) & 0xffffffff);

	/* Load the firmware into SRAM. */
	ti_loadfw(sc);

	/* Set up the contents of the general info and ring control blocks. */

	/* Set up the event ring and producer pointer. */
	rcb = &sc->ti_rdata->ti_info.ti_ev_rcb;

	TI_HOSTADDR(rcb->ti_hostaddr) = TI_RING_DMA_ADDR(sc, ti_event_ring);
	rcb->ti_flags = 0;
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_ev_prodidx_ptr) =
		TI_RING_DMA_ADDR(sc, ti_ev_prodidx_r);
	sc->ti_ev_prodidx.ti_idx = 0;
	CSR_WRITE_4(sc, TI_GCR_EVENTCONS_IDX, 0);
	sc->ti_ev_saved_considx = 0;

	/* Set up the command ring and producer mailbox. */
	rcb = &sc->ti_rdata->ti_info.ti_cmd_rcb;

	TI_HOSTADDR(rcb->ti_hostaddr) = TI_GCR_NIC_ADDR(TI_GCR_CMDRING);
	rcb->ti_flags = 0;
	rcb->ti_max_len = 0;
	for (i = 0; i < TI_CMD_RING_CNT; i++) {
		CSR_WRITE_4(sc, TI_GCR_CMDRING + (i * 4), 0);
	}
	CSR_WRITE_4(sc, TI_GCR_CMDCONS_IDX, 0);
	CSR_WRITE_4(sc, TI_MB_CMDPROD_IDX, 0);
	sc->ti_cmd_saved_prodidx = 0;

	/*
	 * Assign the address of the stats refresh buffer.
	 * We re-use the current stats buffer for this to
	 * conserve memory.
	 */
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_refresh_stats_ptr) =
		TI_RING_DMA_ADDR(sc, ti_info.ti_stats);

	/* Set up the standard receive ring. */
	rcb = &sc->ti_rdata->ti_info.ti_std_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) =
		TI_RING_DMA_ADDR(sc, ti_rx_std_ring);
	rcb->ti_max_len = ETHER_MAX_LEN;
	rcb->ti_flags = 0;
	rcb->ti_flags |= TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;

	/* Set up the jumbo receive ring. */
	rcb = &sc->ti_rdata->ti_info.ti_jumbo_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = TI_RING_DMA_ADDR(sc, ti_rx_jumbo_ring);
	rcb->ti_max_len = ETHER_MAX_LEN_JUMBO;
	rcb->ti_flags = 0;
	rcb->ti_flags |= TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;

	/*
	 * Set up the mini ring. Only activated on the
	 * Tigon 2 but the slot in the config block is
	 * still there on the Tigon 1.
	 */
	rcb = &sc->ti_rdata->ti_info.ti_mini_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = TI_RING_DMA_ADDR(sc, ti_rx_mini_ring);
	rcb->ti_max_len = MHLEN - ETHER_ALIGN;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		rcb->ti_flags = TI_RCB_FLAG_RING_DISABLED;
	else
		rcb->ti_flags = 0;
	rcb->ti_flags |= TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;

	/*
	 * Set up the receive return ring.
	 */
	rcb = &sc->ti_rdata->ti_info.ti_return_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = TI_RING_DMA_ADDR(sc,ti_rx_return_ring);
	rcb->ti_flags = 0;
	rcb->ti_max_len = TI_RETURN_RING_CNT;
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_return_prodidx_ptr) =
	    TI_RING_DMA_ADDR(sc, ti_return_prodidx_r);

	/*
	 * Set up the tx ring. Note: for the Tigon 2, we have the option
	 * of putting the transmit ring in the host's address space and
	 * letting the chip DMA it instead of leaving the ring in the NIC's
	 * memory and accessing it through the shared memory region. We
	 * do this for the Tigon 2, but it doesn't work on the Tigon 1,
	 * so we have to revert to the shared memory scheme if we detect
	 * a Tigon 1 chip.
	 */
	CSR_WRITE_4(sc, TI_WINBASE, TI_TX_RING_BASE);
	bzero((char *)sc->ti_rdata->ti_tx_ring,
	    TI_TX_RING_CNT * sizeof(struct ti_tx_desc));
	rcb = &sc->ti_rdata->ti_info.ti_tx_rcb;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		rcb->ti_flags = 0;
	else
		rcb->ti_flags = TI_RCB_FLAG_HOST_RING;
	rcb->ti_flags |= TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
#if NVLAN > 0
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;
#endif
	rcb->ti_max_len = TI_TX_RING_CNT;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		TI_HOSTADDR(rcb->ti_hostaddr) = TI_TX_RING_BASE;
	else
		TI_HOSTADDR(rcb->ti_hostaddr) =
			TI_RING_DMA_ADDR(sc, ti_tx_ring);
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_tx_considx_ptr) =
		TI_RING_DMA_ADDR(sc, ti_tx_considx_r);

	TI_RING_DMASYNC(sc, ti_info, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	/* Set up tuneables */
	CSR_WRITE_4(sc, TI_GCR_RX_COAL_TICKS, (sc->ti_rx_coal_ticks / 10));
	CSR_WRITE_4(sc, TI_GCR_TX_COAL_TICKS, sc->ti_tx_coal_ticks);
	CSR_WRITE_4(sc, TI_GCR_STAT_TICKS, sc->ti_stat_ticks);
	CSR_WRITE_4(sc, TI_GCR_RX_MAX_COAL_BD, sc->ti_rx_max_coal_bds);
	CSR_WRITE_4(sc, TI_GCR_TX_MAX_COAL_BD, sc->ti_tx_max_coal_bds);
	CSR_WRITE_4(sc, TI_GCR_TX_BUFFER_RATIO, sc->ti_tx_buf_ratio);

	/* Turn interrupts on. */
	CSR_WRITE_4(sc, TI_GCR_MASK_INTRS, 0);
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	/* Start CPU. */
	TI_CLRBIT(sc, TI_CPU_STATE, (TI_CPUSTATE_HALT|TI_CPUSTATE_STEP));

	return (0);
}

/*
 * Probe for a Tigon chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match.
 */
int
ti_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ti_devices,
	    sizeof(ti_devices)/sizeof(ti_devices[0])));
}

void
ti_attach(struct device *parent, struct device *self, void *aux)
{
	struct ti_softc *sc = (struct ti_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t size;
	bus_dma_segment_t seg;
	int rseg;
	struct ifnet *ifp;
	caddr_t kva;

	/*
	 * Map control/status registers.
	 */

	if (pci_mapreg_map(pa, TI_PCI_LOMEM,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->ti_btag, &sc->ti_bhandle, NULL, &size, 0)) {
 		printf(": can't map mem space\n");
		return;
 	}

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail_1;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->ti_intrhand = pci_intr_establish(pc, ih, IPL_NET, ti_intr, sc,
	    self->dv_xname);
	if (sc->ti_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_1;
	}

	if (ti_chipinit(sc)) {
		printf("%s: chip initialization failed\n", sc->sc_dv.dv_xname);
		goto fail_2;
	}

	/* Zero out the NIC's on-board SRAM. */
	ti_mem_set(sc, 0x2000, 0x100000 - 0x2000);

	/* Init again -- zeroing memory may have clobbered some registers. */
	if (ti_chipinit(sc)) {
		printf("%s: chip initialization failed\n", sc->sc_dv.dv_xname);
		goto fail_2;
	}

	/*
	 * Get station address from the EEPROM. Note: the manual states
	 * that the MAC address is at offset 0x8c, however the data is
	 * stored as two longwords (since that's how it's loaded into
	 * the NIC). This means the MAC address is actually preceded
	 * by two zero bytes. We need to skip over those.
	 */
	if (ti_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr,
				TI_EE_MAC_OFFSET + 2, ETHER_ADDR_LEN)) {
		printf("%s: failed to read station address\n",
		    sc->sc_dv.dv_xname);
		free(sc, M_DEVBUF);
		goto fail_2;
	}

	/*
	 * A Tigon chip was detected. Inform the world.
	 */
	printf(": %s, address %s\n", intrstr,
	     ether_sprintf(sc->arpcom.ac_enaddr));

	/* Allocate the general information block and ring buffers. */
	sc->sc_dmatag = pa->pa_dmat;
	if (bus_dmamem_alloc(sc->sc_dmatag, sizeof(struct ti_ring_data),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", sc->sc_dv.dv_xname);
		goto fail_2;
	}
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
	    sizeof(struct ti_ring_data), &kva, BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		       sc->sc_dv.dv_xname, sizeof(struct ti_ring_data));
		goto fail_3;
	}
	if (bus_dmamap_create(sc->sc_dmatag, sizeof(struct ti_ring_data), 1,
	    sizeof(struct ti_ring_data), 0, BUS_DMA_NOWAIT,
	    &sc->ti_ring_map)) {
		printf("%s: can't create dma map\n", sc->sc_dv.dv_xname);
		goto fail_4;
	}
	if (bus_dmamap_load(sc->sc_dmatag, sc->ti_ring_map, kva,
	    sizeof(struct ti_ring_data), NULL, BUS_DMA_NOWAIT)) {
		goto fail_5;
	}
	sc->ti_rdata = (struct ti_ring_data *)kva;
	bzero(sc->ti_rdata, sizeof(struct ti_ring_data));

	/* Try to allocate memory for jumbo buffers. */
	if (ti_alloc_jumbo_mem(sc)) {
		printf("%s: jumbo buffer allocation failed\n",
		    sc->sc_dv.dv_xname);
		goto fail_5;
	}

	/*
	 * We really need a better way to tell a 1000baseTX card
	 * from a 1000baseSX one, since in theory there could be
	 * OEMed 1000baseTX cards from lame vendors who aren't
	 * clever enough to change the PCI ID. For the moment
	 * though, the AceNIC is the only copper card available.
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ALTEON &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ALTEON_ACENICT)
		sc->ti_copper = 1;
	/* Ok, it's not the only copper card available */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NETGEAR &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NETGEAR_GA620T)
		sc->ti_copper = 1;

	/* Set default tuneable values. */
	sc->ti_stat_ticks = 2 * TI_TICKS_PER_SEC;
	sc->ti_rx_coal_ticks = TI_TICKS_PER_SEC / 5000;
	sc->ti_tx_coal_ticks = TI_TICKS_PER_SEC / 500;
	sc->ti_rx_max_coal_bds = 64;
	sc->ti_tx_max_coal_bds = 128;
	sc->ti_tx_buf_ratio = 21;

	/* Set up ifnet structure */
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ti_ioctl;
	ifp->if_start = ti_start;
	ifp->if_watchdog = ti_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, TI_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dv.dv_xname, ifp->if_xname, IFNAMSIZ);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	/* Set up ifmedia support. */
	ifmedia_init(&sc->ifmedia, IFM_IMASK, ti_ifmedia_upd, ti_ifmedia_sts);
	if (sc->ti_copper) {
		/*
		 * Copper cards allow manual 10/100 mode selection,
		 * but not manual 1000baseTX mode selection. Why?
		 * Because currently there's no way to specify the
		 * master/slave setting through the firmware interface,
		 * so Alteon decided to just bag it and handle it
		 * via autonegotiation.
		 */
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_1000_T, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_1000_T|IFM_FDX, 0, NULL);
	} else {
		/* Fiber cards don't support 10/100 modes. */
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
	}
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	shutdownhook_establish(ti_shutdown, sc);
	return;

fail_5:
	bus_dmamap_destroy(sc->sc_dmatag, sc->ti_ring_map);

fail_4:
	bus_dmamem_unmap(sc->sc_dmatag, kva,
	    sizeof(struct ti_ring_data));

fail_3:
	bus_dmamem_free(sc->sc_dmatag, &seg, rseg);

fail_2:
	pci_intr_disestablish(pc, sc->ti_intrhand);

fail_1:
	bus_space_unmap(sc->ti_btag, sc->ti_bhandle, size);
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle three possibilities here:
 * 1) the frame is from the mini receive ring (can only happen)
 *    on Tigon 2 boards)
 * 2) the frame is from the jumbo receive ring
 * 3) the frame is from the standard receive ring
 */

void
ti_rxeof(struct ti_softc *sc)
{
	struct ifnet		*ifp;
	struct ti_cmd_desc	cmd;

	ifp = &sc->arpcom.ac_if;

	while(sc->ti_rx_saved_considx != sc->ti_return_prodidx.ti_idx) {
		struct ti_rx_desc	*cur_rx;
		u_int32_t		rxidx;
		struct mbuf		*m = NULL;
		int			sumflags = 0;
		bus_dmamap_t		dmamap;

		cur_rx =
		    &sc->ti_rdata->ti_rx_return_ring[sc->ti_rx_saved_considx];
		rxidx = cur_rx->ti_idx;
		TI_INC(sc->ti_rx_saved_considx, TI_RETURN_RING_CNT);

		if (cur_rx->ti_flags & TI_BDFLAG_JUMBO_RING) {
			TI_INC(sc->ti_jumbo, TI_JUMBO_RX_RING_CNT);
			m = sc->ti_cdata.ti_rx_jumbo_chain[rxidx];
			sc->ti_cdata.ti_rx_jumbo_chain[rxidx] = NULL;
			if (cur_rx->ti_flags & TI_BDFLAG_ERROR) {
				ifp->if_ierrors++;
				ti_newbuf_jumbo(sc, sc->ti_jumbo, m);
				continue;
			}
			if (ti_newbuf_jumbo(sc, sc->ti_jumbo, NULL)
			    == ENOBUFS) {
				struct mbuf             *m0;
				m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
				    cur_rx->ti_len + ETHER_ALIGN, 0, ifp, NULL);
				ti_newbuf_jumbo(sc, sc->ti_jumbo, m);
				if (m0 == NULL) {
					ifp->if_ierrors++;
					continue;
				}
				m_adj(m0, ETHER_ALIGN);
				m = m0;
			}
		} else if (cur_rx->ti_flags & TI_BDFLAG_MINI_RING) {
			TI_INC(sc->ti_mini, TI_MINI_RX_RING_CNT);
			m = sc->ti_cdata.ti_rx_mini_chain[rxidx];
			sc->ti_cdata.ti_rx_mini_chain[rxidx] = NULL;
			dmamap = sc->ti_cdata.ti_rx_mini_map[rxidx];
			sc->ti_cdata.ti_rx_mini_map[rxidx] = 0;
			if (cur_rx->ti_flags & TI_BDFLAG_ERROR) {
				ifp->if_ierrors++;
				ti_newbuf_mini(sc, sc->ti_mini, m, dmamap);
				continue;
			}
			if (ti_newbuf_mini(sc, sc->ti_mini, NULL, dmamap)
			    == ENOBUFS) {
				ifp->if_ierrors++;
				ti_newbuf_mini(sc, sc->ti_mini, m, dmamap);
				continue;
			}
		} else {
			TI_INC(sc->ti_std, TI_STD_RX_RING_CNT);
			m = sc->ti_cdata.ti_rx_std_chain[rxidx];
			sc->ti_cdata.ti_rx_std_chain[rxidx] = NULL;
			dmamap = sc->ti_cdata.ti_rx_std_map[rxidx];
			sc->ti_cdata.ti_rx_std_map[rxidx] = 0;
			if (cur_rx->ti_flags & TI_BDFLAG_ERROR) {
				ifp->if_ierrors++;
				ti_newbuf_std(sc, sc->ti_std, m, dmamap);
				continue;
			}
			if (ti_newbuf_std(sc, sc->ti_std, NULL, dmamap)
			    == ENOBUFS) {
				ifp->if_ierrors++;
				ti_newbuf_std(sc, sc->ti_std, m, dmamap);
				continue;
			}
		}

		if (m == NULL)
			panic("%s: couldn't get mbuf", sc->sc_dv.dv_xname);

		m->m_pkthdr.len = m->m_len = cur_rx->ti_len;
		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
		/*
	 	 * Handle BPF listeners. Let the BPF user see the packet.
	 	 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		if ((cur_rx->ti_ip_cksum ^ 0xffff) == 0)
			sumflags |= M_IPV4_CSUM_IN_OK;
		m->m_pkthdr.csum_flags = sumflags;
		sumflags = 0;

		ether_input_mbuf(ifp, m);
	}

	/* Only necessary on the Tigon 1. */
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		CSR_WRITE_4(sc, TI_GCR_RXRETURNCONS_IDX,
		    sc->ti_rx_saved_considx);

	TI_UPDATE_STDPROD(sc, sc->ti_std);
	TI_UPDATE_MINIPROD(sc, sc->ti_mini);
	TI_UPDATE_JUMBOPROD(sc, sc->ti_jumbo);
}

void
ti_txeof_tigon1(struct ti_softc *sc)
{
	struct ifnet		*ifp;
	struct ti_txmap_entry	*entry;
	int			active = 1;

	ifp = &sc->arpcom.ac_if;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->ti_tx_saved_considx != sc->ti_tx_considx.ti_idx) {
		u_int32_t		idx = 0;
		struct ti_tx_desc	txdesc;

		idx = sc->ti_tx_saved_considx;
		ti_mem_read(sc, TI_TX_RING_BASE + idx * sizeof(txdesc),
			    sizeof(txdesc), (caddr_t)&txdesc);

		if (txdesc.ti_flags & TI_BDFLAG_END)
			ifp->if_opackets++;

		if (sc->ti_cdata.ti_tx_chain[idx] != NULL) {
			m_freem(sc->ti_cdata.ti_tx_chain[idx]);
			sc->ti_cdata.ti_tx_chain[idx] = NULL;

			entry = sc->ti_cdata.ti_tx_map[idx];
			bus_dmamap_sync(sc->sc_dmatag, entry->dmamap, 0,
			    entry->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->sc_dmatag, entry->dmamap);
			SLIST_INSERT_HEAD(&sc->ti_tx_map_listhead, entry,
			    link);
			sc->ti_cdata.ti_tx_map[idx] = NULL;

		}
		sc->ti_txcnt--;
		TI_INC(sc->ti_tx_saved_considx, TI_TX_RING_CNT);
		ifp->if_timer = 0;

		active = 0;
	}

	if (!active)
		ifp->if_flags &= ~IFF_OACTIVE;
}

void
ti_txeof_tigon2(struct ti_softc *sc)
{
	struct ti_tx_desc	*cur_tx = NULL;
	struct ifnet		*ifp;
	struct ti_txmap_entry	*entry;

	ifp = &sc->arpcom.ac_if;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->ti_tx_saved_considx != sc->ti_tx_considx.ti_idx) {
		u_int32_t		idx = 0;

		idx = sc->ti_tx_saved_considx;
		cur_tx = &sc->ti_rdata->ti_tx_ring[idx];

		if (cur_tx->ti_flags & TI_BDFLAG_END)
			ifp->if_opackets++;
		if (sc->ti_cdata.ti_tx_chain[idx] != NULL) {
			m_freem(sc->ti_cdata.ti_tx_chain[idx]);
			sc->ti_cdata.ti_tx_chain[idx] = NULL;

			entry = sc->ti_cdata.ti_tx_map[idx];
			bus_dmamap_sync(sc->sc_dmatag, entry->dmamap, 0,
			    entry->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->sc_dmatag, entry->dmamap);
			SLIST_INSERT_HEAD(&sc->ti_tx_map_listhead, entry,
			    link);
			sc->ti_cdata.ti_tx_map[idx] = NULL;

		}
		sc->ti_txcnt--;
		TI_INC(sc->ti_tx_saved_considx, TI_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;
}

int
ti_intr(void *xsc)
{
	struct ti_softc		*sc;
	struct ifnet		*ifp;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	/* XXX checking this register is expensive. */
	/* Make sure this is really our interrupt. */
	if (!(CSR_READ_4(sc, TI_MISC_HOST_CTL) & TI_MHC_INTSTATE))
		return (0);

	/* Ack interrupt and stop others from occurring. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	if (ifp->if_flags & IFF_RUNNING) {
		/* Check RX return ring producer/consumer */
		ti_rxeof(sc);

		/* Check TX ring producer/consumer */
		if (sc->ti_hwrev == TI_HWREV_TIGON)
			ti_txeof_tigon1(sc);
		else
			ti_txeof_tigon2(sc);
	}

	ti_handle_events(sc);

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		ti_start(ifp);

	return (1);
}

void
ti_stats_update(struct ti_softc *sc)
{
	struct ifnet		*ifp;
	struct ti_stats		*stats = &sc->ti_rdata->ti_info.ti_stats;

	ifp = &sc->arpcom.ac_if;

	TI_RING_DMASYNC(sc, ti_info.ti_stats, BUS_DMASYNC_POSTREAD);

	ifp->if_collisions += stats->dot3StatsSingleCollisionFrames +
		stats->dot3StatsMultipleCollisionFrames +
		stats->dot3StatsExcessiveCollisions +
		stats->dot3StatsLateCollisions -
		ifp->if_collisions;

	TI_RING_DMASYNC(sc, ti_info.ti_stats, BUS_DMASYNC_PREREAD);
}

/*
 * Encapsulate an mbuf chain in the tx ring by coupling the mbuf data
 * pointers to descriptors.
 */
int
ti_encap_tigon1(struct ti_softc *sc, struct mbuf *m_head, u_int32_t *txidx)
{
	u_int32_t		frag, cur, cnt = 0;
	struct ti_txmap_entry	*entry;
	bus_dmamap_t		txmap;
	struct ti_tx_desc	txdesc;
	int			i = 0;
#if NVLAN > 0
	struct ifvlan		*ifv = NULL;

	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

	entry = SLIST_FIRST(&sc->ti_tx_map_listhead);
	if (entry == NULL)
		return (ENOBUFS);
	txmap = entry->dmamap;

	cur = frag = *txidx;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	if (bus_dmamap_load_mbuf(sc->sc_dmatag, txmap, m_head,
	    BUS_DMA_NOWAIT))
		return (ENOBUFS);

	for (i = 0; i < txmap->dm_nsegs; i++) {
		if (sc->ti_cdata.ti_tx_chain[frag] != NULL)
			break;

		memset(&txdesc, 0, sizeof(txdesc));

		TI_HOSTADDR(txdesc.ti_addr) = txmap->dm_segs[i].ds_addr;
		txdesc.ti_len = txmap->dm_segs[i].ds_len & 0xffff;

		txdesc.ti_flags = 0;
#if NVLAN > 0
		if (ifv != NULL) {
			txdesc.ti_flags |= TI_BDFLAG_VLAN_TAG;
			txdesc.ti_vlan_tag = ifv->ifv_tag;
		}
#endif

		ti_mem_write(sc, TI_TX_RING_BASE + frag * sizeof(txdesc),
			     sizeof(txdesc), (caddr_t)&txdesc);

		/*
		 * Sanity check: avoid coming within 16 descriptors
		 * of the end of the ring.
		 */
		if ((TI_TX_RING_CNT - (sc->ti_txcnt + cnt)) < 16)
			return (ENOBUFS);
		cur = frag;
		TI_INC(frag, TI_TX_RING_CNT);
		cnt++;
	}

	if (frag == sc->ti_tx_saved_considx)
		return (ENOBUFS);

	txdesc.ti_flags |= TI_BDFLAG_END;
	ti_mem_write(sc, TI_TX_RING_BASE + cur * sizeof(txdesc),
		     sizeof(txdesc), (caddr_t)&txdesc);

	bus_dmamap_sync(sc->sc_dmatag, txmap, 0, txmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	sc->ti_cdata.ti_tx_chain[cur] = m_head;
	SLIST_REMOVE_HEAD(&sc->ti_tx_map_listhead, link);
	sc->ti_cdata.ti_tx_map[cur] = entry;
	sc->ti_txcnt += cnt;

	*txidx = frag;

	return (0);
}

/*
 * Encapsulate an mbuf chain in the tx ring by coupling the mbuf data
 * pointers to descriptors.
 */
int
ti_encap_tigon2(struct ti_softc *sc, struct mbuf *m_head, u_int32_t *txidx)
{
	struct ti_tx_desc	*f = NULL;
	u_int32_t		frag, cur, cnt = 0;
	struct ti_txmap_entry	*entry;
	bus_dmamap_t		txmap;
	int			i = 0;
#if NVLAN > 0
	struct ifvlan		*ifv = NULL;

	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

	entry = SLIST_FIRST(&sc->ti_tx_map_listhead);
	if (entry == NULL)
		return (ENOBUFS);
	txmap = entry->dmamap;

	cur = frag = *txidx;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	if (bus_dmamap_load_mbuf(sc->sc_dmatag, txmap, m_head,
	    BUS_DMA_NOWAIT))
		return (ENOBUFS);

	for (i = 0; i < txmap->dm_nsegs; i++) {
		f = &sc->ti_rdata->ti_tx_ring[frag];

		if (sc->ti_cdata.ti_tx_chain[frag] != NULL)
			break;

		TI_HOSTADDR(f->ti_addr) = txmap->dm_segs[i].ds_addr;
		f->ti_len = txmap->dm_segs[i].ds_len & 0xffff;
		f->ti_flags = 0;
#if NVLAN > 0
		if (ifv != NULL) {
			f->ti_flags |= TI_BDFLAG_VLAN_TAG;
			f->ti_vlan_tag = ifv->ifv_tag;
		} else {
			f->ti_vlan_tag = 0;
		}
#endif
		/*
		 * Sanity check: avoid coming within 16 descriptors
		 * of the end of the ring.
		 */
		if ((TI_TX_RING_CNT - (sc->ti_txcnt + cnt)) < 16)
			return(ENOBUFS);
		cur = frag;
		TI_INC(frag, TI_TX_RING_CNT);
		cnt++;
	}

	if (frag == sc->ti_tx_saved_considx)
		return(ENOBUFS);

	sc->ti_rdata->ti_tx_ring[cur].ti_flags |= TI_BDFLAG_END;

	bus_dmamap_sync(sc->sc_dmatag, txmap, 0, txmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	TI_RING_DMASYNC(sc, ti_tx_ring[cur], BUS_DMASYNC_POSTREAD);

	sc->ti_cdata.ti_tx_chain[cur] = m_head;
	SLIST_REMOVE_HEAD(&sc->ti_tx_map_listhead, link);
	sc->ti_cdata.ti_tx_map[cur] = entry;
	sc->ti_txcnt += cnt;

	*txidx = frag;

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
void
ti_start(struct ifnet *ifp)
{
	struct ti_softc		*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		prodidx = 0;
	int			pkts = 0, error;

	sc = ifp->if_softc;

	prodidx = CSR_READ_4(sc, TI_MB_SENDPROD_IDX);

	while(sc->ti_cdata.ti_tx_chain[prodidx] == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (sc->ti_hwrev == TI_HWREV_TIGON)
			error = ti_encap_tigon1(sc, m_head, &prodidx);
		else
			error = ti_encap_tigon2(sc, m_head, &prodidx);

		if (error) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		pkts++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif
	}
	if (pkts == 0)
		return;

	/* Transmit */
	CSR_WRITE_4(sc, TI_MB_SENDPROD_IDX, prodidx);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
ti_init(void *xsc)
{
	struct ti_softc		*sc = xsc;
        int			s;

	s = splnet();

	/* Cancel pending I/O and flush buffers. */
	ti_stop(sc);

	/* Init the gen info block, ring control blocks and firmware. */
	if (ti_gibinit(sc)) {
		printf("%s: initialization failure\n", sc->sc_dv.dv_xname);
		splx(s);
		return;
	}

	splx(s);
}

void
ti_init2(struct ti_softc *sc)
{
	struct ti_cmd_desc	cmd;
	struct ifnet		*ifp;
	u_int16_t		*m;
	struct ifmedia		*ifm;
	int			tmp;

	ifp = &sc->arpcom.ac_if;

	/* Specify MTU and interface index. */
	CSR_WRITE_4(sc, TI_GCR_IFINDEX, sc->sc_dv.dv_unit);
	CSR_WRITE_4(sc, TI_GCR_IFMTU,
		ETHER_MAX_LEN_JUMBO + ETHER_VLAN_ENCAP_LEN);
	TI_DO_CMD(TI_CMD_UPDATE_GENCOM, 0, 0);

	/* Load our MAC address. */
	m = (u_int16_t *)&sc->arpcom.ac_enaddr[0];
	CSR_WRITE_4(sc, TI_GCR_PAR0, htons(m[0]));
	CSR_WRITE_4(sc, TI_GCR_PAR1, (htons(m[1]) << 16) | htons(m[2]));
	TI_DO_CMD(TI_CMD_SET_MAC_ADDR, 0, 0);

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC) {
		TI_DO_CMD(TI_CMD_SET_PROMISC_MODE, TI_CMD_CODE_PROMISC_ENB, 0);
	} else {
		TI_DO_CMD(TI_CMD_SET_PROMISC_MODE, TI_CMD_CODE_PROMISC_DIS, 0);
	}

	/* Program multicast filter. */
	ti_setmulti(sc);

	/*
	 * If this is a Tigon 1, we should tell the
	 * firmware to use software packet filtering.
	 */
	if (sc->ti_hwrev == TI_HWREV_TIGON) {
		TI_DO_CMD(TI_CMD_FDR_FILTERING, TI_CMD_CODE_FILT_ENB, 0);
	}

	/* Init RX ring. */
	if (ti_init_rx_ring_std(sc) == ENOBUFS)
		panic("not enough mbufs for rx ring");

	/* Init jumbo RX ring. */
	ti_init_rx_ring_jumbo(sc);

	/*
	 * If this is a Tigon 2, we can also configure the
	 * mini ring.
	 */
	if (sc->ti_hwrev == TI_HWREV_TIGON_II)
		ti_init_rx_ring_mini(sc);

	CSR_WRITE_4(sc, TI_GCR_RXRETURNCONS_IDX, 0);
	sc->ti_rx_saved_considx = 0;

	/* Init TX ring. */
	ti_init_tx_ring(sc);

	/* Tell firmware we're alive. */
	TI_DO_CMD(TI_CMD_HOST_STATE, TI_CMD_CODE_STACK_UP, 0);

	/* Enable host interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Make sure to set media properly. We have to do this
	 * here since we have to issue commands in order to set
	 * the link negotiation and we can't issue commands until
	 * the firmware is running.
	 */
	ifm = &sc->ifmedia;
	tmp = ifm->ifm_media;
	ifm->ifm_media = ifm->ifm_cur->ifm_media;
	ti_ifmedia_upd(ifp);
	ifm->ifm_media = tmp;
}

/*
 * Set media options.
 */
int
ti_ifmedia_upd(struct ifnet *ifp)
{
	struct ti_softc		*sc;
	struct ifmedia		*ifm;
	struct ti_cmd_desc	cmd;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		CSR_WRITE_4(sc, TI_GCR_GLINK, TI_GLNK_PREF|TI_GLNK_1000MB|
		    TI_GLNK_FULL_DUPLEX|TI_GLNK_RX_FLOWCTL_Y|
		    TI_GLNK_AUTONEGENB|TI_GLNK_ENB);
		CSR_WRITE_4(sc, TI_GCR_LINK, TI_LNK_100MB|TI_LNK_10MB|
		    TI_LNK_FULL_DUPLEX|TI_LNK_HALF_DUPLEX|
		    TI_LNK_AUTONEGENB|TI_LNK_ENB);
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_BOTH, 0);
		break;
	case IFM_1000_SX:
	case IFM_1000_T:
		CSR_WRITE_4(sc, TI_GCR_GLINK, TI_GLNK_PREF|TI_GLNK_1000MB|
		    TI_GLNK_RX_FLOWCTL_Y|TI_GLNK_ENB);
		CSR_WRITE_4(sc, TI_GCR_LINK, 0);
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
			TI_SETBIT(sc, TI_GCR_GLINK, TI_GLNK_FULL_DUPLEX);
		}
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_GIGABIT, 0);
		break;
	case IFM_100_FX:
	case IFM_10_FL:
	case IFM_100_TX:
	case IFM_10_T:
		CSR_WRITE_4(sc, TI_GCR_GLINK, 0);
		CSR_WRITE_4(sc, TI_GCR_LINK, TI_LNK_ENB|TI_LNK_PREF);
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_FX ||
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_100MB);
		} else {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_10MB);
		}
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_FULL_DUPLEX);
		} else {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_HALF_DUPLEX);
		}
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_10_100, 0);
		break;
	}

	return (0);
}

/*
 * Report current media status.
 */
void
ti_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ti_softc		*sc;
	u_int32_t		media = 0;

	sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (sc->ti_linkstat == TI_EV_CODE_LINK_DOWN)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if (sc->ti_linkstat == TI_EV_CODE_GIG_LINK_UP) {
		media = CSR_READ_4(sc, TI_GCR_GLINK_STAT);
		if (sc->ti_copper)
			ifmr->ifm_active |= IFM_1000_T;
		else
			ifmr->ifm_active |= IFM_1000_SX;
		if (media & TI_GLNK_FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
	} else if (sc->ti_linkstat == TI_EV_CODE_LINK_UP) {
		media = CSR_READ_4(sc, TI_GCR_LINK_STAT);
		if (sc->ti_copper) {
			if (media & TI_LNK_100MB)
				ifmr->ifm_active |= IFM_100_TX;
			if (media & TI_LNK_10MB)
				ifmr->ifm_active |= IFM_10_T;
		} else {
			if (media & TI_LNK_100MB)
				ifmr->ifm_active |= IFM_100_FX;
			if (media & TI_LNK_10MB)
				ifmr->ifm_active |= IFM_10_FL;
		}
		if (media & TI_LNK_FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		if (media & TI_LNK_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
	}
}

int
ti_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ti_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	int			s, error = 0;
	struct ti_cmd_desc	cmd;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ti_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif /* INET */
		default:
			ti_init(sc);
			break;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ETHERMTU_JUMBO)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.
			 */
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->ti_if_flags & IFF_PROMISC)) {
				TI_DO_CMD(TI_CMD_SET_PROMISC_MODE,
				    TI_CMD_CODE_PROMISC_ENB, 0);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->ti_if_flags & IFF_PROMISC) {
				TI_DO_CMD(TI_CMD_SET_PROMISC_MODE,
				    TI_CMD_CODE_PROMISC_DIS, 0);
			} else
				ti_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				ti_stop(sc);
			}
		}
		sc->ti_if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				ti_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

void
ti_watchdog(struct ifnet *ifp)
{
	struct ti_softc		*sc;

	sc = ifp->if_softc;

	printf("%s: watchdog timeout -- resetting\n", sc->sc_dv.dv_xname);
	ti_stop(sc);
	ti_init(sc);

	ifp->if_oerrors++;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
ti_stop(struct ti_softc *sc)
{
	struct ifnet		*ifp;
	struct ti_cmd_desc	cmd;

	ifp = &sc->arpcom.ac_if;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* Disable host interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);
	/*
	 * Tell firmware we're shutting down.
	 */
	TI_DO_CMD(TI_CMD_HOST_STATE, TI_CMD_CODE_STACK_DOWN, 0);

	/* Halt and reinitialize. */
	ti_chipinit(sc);
	ti_mem_set(sc, 0x2000, 0x100000 - 0x2000);
	ti_chipinit(sc);

	/* Free the RX lists. */
	ti_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	ti_free_rx_ring_jumbo(sc);

	/* Free mini RX list. */
	ti_free_rx_ring_mini(sc);

	/* Free TX buffers. */
	ti_free_tx_ring(sc);

	sc->ti_ev_prodidx.ti_idx = 0;
	sc->ti_return_prodidx.ti_idx = 0;
	sc->ti_tx_considx.ti_idx = 0;
	sc->ti_tx_saved_considx = TI_TXCONS_UNSET;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
void
ti_shutdown(void *xsc)
{
	struct ti_softc		*sc;

	sc = xsc;

	ti_chipinit(sc);
}
