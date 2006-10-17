/*	$OpenBSD: malo.c,v 1.5 2006/10/17 10:31:26 claudio Exp $ */

/*
 * Copyright (c) 2006 Claudio Jeker <claudio@openbsd.org>
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
 */

#include "bpfilter.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>

#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/endian.h>
//#include <machine/intr.h>

#include <net/if.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/ic/malo.h>

#ifdef MALO_DEBUG
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/* internal structures and defines */
struct malo_node {
	struct ieee80211_node	ni;
};

struct malo_rx_data {
	bus_dmamap_t	map;
	struct mbuf	*m;
};

/* Rx descriptor used by HW */
struct malo_rx_desc {
	uint8_t		status;
	uint8_t		rssi;
	uint16_t	reserved1;	/* needs to be 1 */
	uint16_t	len;
	uint8_t		reserved2;	/* actually unkown */
	uint8_t		reserved3;
	uint32_t	physdata;	/* DMA address of data */
	uint32_t	physnext;	/* DMA address of next control block */
	uint32_t	id;		/* id for the host to id buffer ??? */
} __packed;

#define MALO_RX_RING_COUNT	48

/* firmware commands as found in a document describing the Libertas FW */
#define MALO_CMD_GET_HW_SPEC	0x0003
#define MALO_CMD_RESPONSE	0x8000

struct malo_cmdheader {
	uint16_t	cmd;
	uint16_t	size;		/* size of the command, incl. header */
	uint16_t	seqnum;		/* seems not to matter that much */
	uint16_t	result;		/* set to 0 on request */
	/* following the data payload, up to 256 bytes */
};

struct malo_hw_spec {
	uint16_t	HwVersion;
	uint16_t	NumOfWCB;	/* reserved */
	uint16_t	NumOfMCastAdr;
	uint8_t		PermanentAddress[6];
	uint16_t	RegionCode;
	uint16_t	NumberOfAntenna;
	uint32_t	FWReleaseNumber;
	uint32_t	RxPdRd1Ptr;
	uint32_t	RxPdRd2Ptr;
	uint32_t	RxPdWrPtr;
	uint32_t	CookiePtr;
} __packed;


#define malo_mem_write4(sc, off, x) \
	bus_space_write_4((sc)->sc_mem1_bt, (sc)->sc_mem1_bh, (off), (x))
#define malo_mem_write2(sc, off, x) \
	bus_space_write_2((sc)->sc_mem1_bt, (sc)->sc_mem1_bh, (off), (x))
#define malo_mem_write1(sc, off, x) \
	bus_space_write_1((sc)->sc_mem1_bt, (sc)->sc_mem1_bh, (off), (x))

#define malo_mem_read4(sc, off) \
	bus_space_read_4((sc)->sc_mem1_bt, (sc)->sc_mem1_bh, (off))
#define malo_mem_read1(sc, off) \
	bus_space_read_1((sc)->sc_mem1_bt, (sc)->sc_mem1_bh, (off))

#define malo_ctl_write4(sc, off, x) \
	bus_space_write_4((sc)->sc_mem2_bt, (sc)->sc_mem2_bh, (off), (x))
#define malo_ctl_read4(sc, off) \
	bus_space_read_4((sc)->sc_mem2_bt, (sc)->sc_mem2_bh, (off))
#define malo_ctl_read1(sc, off) \
	bus_space_read_1((sc)->sc_mem2_bt, (sc)->sc_mem2_bh, (off))

struct cfdriver malo_cd = {
	NULL, "malo", DV_IFNET
};

int	malo_alloc_cmd(struct malo_softc *sc);
void	malo_free_cmd(struct malo_softc *sc);
int	malo_alloc_rx_ring(struct malo_softc *sc, struct malo_rx_ring *ring,
	    int count);
void	malo_free_rx_ring(struct malo_softc *sc, struct malo_rx_ring *ring);
int	malo_load_bootimg(struct malo_softc *sc);
int	malo_load_firmware(struct malo_softc *sc);
int	malo_send_cmd(struct malo_softc *sc, bus_addr_t addr, uint32_t waitfor);
int	malo_reset(struct malo_softc *sc);
int	malo_get_spec(struct malo_softc *sc);
int	malo_init(struct ifnet *ifp);
int	malo_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
void	malo_start(struct ifnet *ifp);
int	malo_stop(struct malo_softc *sc);
void	malo_watchdog(struct ifnet *ifp);
int	malo_newstate(struct ieee80211com *ic, enum ieee80211_state nstate,
	    int arg);
void	malo_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
	    int isnew);
struct ieee80211_node *
	malo_node_alloc(struct ieee80211com *ic);

/* supported rates */
const struct ieee80211_rateset  malo_rates_11b =
    { 4, { 2, 4, 11, 22 } };
const struct ieee80211_rateset  malo_rates_11g =
    { 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

int
malo_intr(void *arg)
{
	struct malo_softc *sc = arg;
	uint32_t status;

	status = malo_ctl_read4(sc, 0x0c30);
	if (status == 0xffffffff || status == 0)
		/* not for us */
		return (0);

	DPRINTF(("%s: INTERRUPT %08x\n", sc->sc_dev.dv_xname, status));

	if (status & 0x4) {
#ifdef MALO_DEBUG
		struct malo_cmdheader	*hdr = sc->sc_cmd_mem;
		int			 i;

		printf("%s: command answer", sc->sc_dev.dv_xname);
		for (i = 0; i < hdr->size; i++) {
			if (i % 16 == 0) 
				printf("\n%4i:", i);
			if (i % 4 == 0)
				printf(" ");
			printf("%02x", (int)*((u_char *)hdr + i));
		}
		printf("\n");
#endif
		/* wakeup caller */
		wakeup(sc);
	}

	/* just ack the interrupt */
	malo_ctl_write4(sc, 0x0c30, 0);
	return (1);
}

int
malo_attach(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int i;
#if 0
	/* ??? */
	//malo_ctl_write4(sc, 0x0c38, 0x1f);
	/* disable interrupts */
	malo_ctl_read4(sc, 0x0c30);
	malo_ctl_write4(sc, 0x0c30, 0);
	malo_ctl_write4(sc, 0x0c34, 0);
	malo_ctl_write4(sc, 0x0c3c, 0);
#endif
	/* allocate DMA structures */
	malo_alloc_cmd(sc);
	malo_alloc_rx_ring(sc, &sc->sc_rxring0, MALO_RX_RING_COUNT);
	malo_alloc_rx_ring(sc, &sc->sc_rxring1, MALO_RX_RING_COUNT);

	/* setup interface */
	ifp->if_softc = sc;
	ifp->if_init = malo_init;
	ifp->if_ioctl = malo_ioctl;
	ifp->if_start = malo_start;
	ifp->if_watchdog = malo_watchdog;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	/* set supported rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = malo_rates_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = malo_rates_11g;

	/* set channels */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags = IEEE80211_CHAN_PUREG;
	}

	/* set the rest */
	ic->ic_caps = IEEE80211_C_IBSS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;
	for (i = 0; i < 6; i++)
		ic->ic_myaddr[i] = malo_ctl_read1(sc, 0xa528 + i);

	/* show our mac address */
	printf(", address: %s\n", ether_sprintf(ic->ic_myaddr));

	/* attach interface */
	if_attach(ifp);
	ieee80211_ifattach(ifp);

	/* post attach vector functions */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = malo_newstate;
	ic->ic_newassoc = malo_newassoc;
	ic->ic_node_alloc = malo_node_alloc;

	ieee80211_media_init(ifp, ieee80211_media_change,
	    ieee80211_media_status);

#if NBPFILTER > 0
	/* TODO bpf mtap */
#endif

	return (0);
}

int
malo_detach(void *arg)
{
	struct malo_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	malo_stop(sc);
	ieee80211_ifdetach(ifp);
	if_detach(ifp);
	malo_free_cmd(sc);
	malo_free_rx_ring(sc, &sc->sc_rxring0);
	malo_free_rx_ring(sc, &sc->sc_rxring1);

	return (0);
}

int
malo_alloc_cmd(struct malo_softc *sc)
{
	int error, nsegs;

	error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1,
	    PAGE_SIZE, 0, BUS_DMA_ALLOCNOW, &sc->sc_cmd_dmam);
	if (error != 0) {
		printf("%s: can not create DMA tag\n", sc->sc_dev.dv_xname);
		return (-1);
	}

	error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE,
	    0, &sc->sc_cmd_dmas, 1, &nsegs, BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: error alloc dma memory\n", sc->sc_dev.dv_xname);
		return (-1);
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cmd_dmas, nsegs,
	    PAGE_SIZE, (caddr_t *)&sc->sc_cmd_mem, BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: error map dma memory\n", sc->sc_dev.dv_xname);
		return (-1);
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_cmd_dmam, 
	    sc->sc_cmd_mem, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: error load dma memory\n", sc->sc_dev.dv_xname);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_cmd_dmas, nsegs);
		return (-1);
	}

	sc->sc_cookie = sc->sc_cmd_mem;
	*sc->sc_cookie = htole32(0xaa55aa55);
	sc->sc_cmd_mem = sc->sc_cmd_mem + sizeof(uint32_t);
	sc->sc_cookie_dmaaddr = sc->sc_cmd_dmam->dm_segs[0].ds_addr;
	sc->sc_cmd_dmaaddr = sc->sc_cmd_dmam->dm_segs[0].ds_addr +
	    sizeof(uint32_t);

	return (0);
}

void
malo_free_cmd(struct malo_softc *sc)
{
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cmd_dmam);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_cookie, PAGE_SIZE);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cmd_dmas, 1);
}

int
malo_alloc_rx_ring(struct malo_softc *sc, struct malo_rx_ring *ring, int count)
{
	struct malo_rx_desc *desc;
	struct malo_rx_data *data;
	int i, nsegs, error;

	ring->count = count;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    count * sizeof(struct malo_rx_desc), 1,
	    count * sizeof(struct malo_rx_desc), 0,
	    BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    count * sizeof(struct malo_rx_desc),
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * sizeof(struct malo_rx_desc), (caddr_t *)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * sizeof(struct malo_rx_desc), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(ring->desc, count * sizeof(struct malo_rx_desc));
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct malo_rx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	bzero(ring->data, count * sizeof (struct malo_rx_data));
	for (i = 0; i < count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		desc->reserved1 = htole16(1);
		desc->physdata = htole32(data->map->dm_segs->ds_addr);
		desc->physnext = htole32(ring->physaddr +
		    (i + 1) % count * sizeof(struct malo_rx_desc));
		desc->id = i;	/* ignored by the card ??? */
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return (0);

fail:	malo_free_rx_ring(sc, ring);
	return (error);
}

void
malo_free_rx_ring(struct malo_softc *sc, struct malo_rx_ring *ring)
{
	struct malo_rx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
		    ring->count * sizeof(struct malo_rx_desc));
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
malo_load_bootimg(struct malo_softc *sc)
{
	char *name = "mrv8k-b.fw";
	uint8_t	*ucode;
	size_t size, count;
	int error;

	/* load boot firmware */
	if ((error = loadfirmware(name, &ucode, &size)) != 0) {
		DPRINTF(("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name));
		return (EIO);
	}

	/*
	 * It seems we are putting this code directly onto the stack of
	 * the ARM cpu. I don't know why we need to instruct the DMA
	 * engine to move the code. This is a big riddle without docu.
	 */
	DPRINTF(("%s: loading boot firmware\n", sc->sc_dev.dv_xname));
	malo_mem_write2(sc, 0xbef8, 0x001);
	malo_mem_write2(sc, 0xbefa, size);
	malo_mem_write4(sc, 0xbefc, 0);

	for (count = 0; count < size; count++)
		malo_mem_write1(sc, 0xbf00 + count, ucode[count]);

	/*
	 * we loaded the firmware into card memory now tell the CPU
	 * to fetch the code and execute it. The memory mapped via the
	 * first bar is internaly mapped to 0xc0000000.
	 */
	if (malo_send_cmd(sc, 0xc000bef8, 5) != 0) {
		printf("%s: timeout at boot firmware load!\n",
		    sc->sc_dev.dv_xname);
		free(ucode, M_DEVBUF);
		return (ETIMEDOUT);
	} 
	free(ucode, M_DEVBUF);

	/* tell the card we're done and... */
	malo_mem_write2(sc, 0xbef8, 0x001);
	malo_mem_write2(sc, 0xbefa, 0);
	malo_mem_write4(sc, 0xbefc, 0);
	malo_send_cmd(sc, 0xc000bef8, 0);

	/* give card a bit time to init */
	delay(50);
	DPRINTF(("%s: boot firmware loaded\n", sc->sc_dev.dv_xname));

	return (0);
}

int
malo_load_firmware(struct malo_softc *sc)
{
	struct malo_cmdheader *hdr;
	char *name = "mrv8k-f.fw";
	void *data;
	uint8_t *ucode;
	size_t size, count, bsize;
	int sn, error;

	/* load real firmware now */
	if ((error = loadfirmware(name, &ucode, &size)) != 0) {
		DPRINTF(("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name));
		return (EIO);
	}

	DPRINTF(("%s: loading firmware\n", sc->sc_dev.dv_xname));
	hdr = sc->sc_cmd_mem;
	data = hdr + 1;
	sn = 1;
	for (count = 0; count < size; count += bsize) {
		bsize = MIN(256, size - count);

		hdr->cmd = htole16(0x0001);
		hdr->size = htole16(bsize);
		hdr->seqnum = htole16(sn++);
		hdr->result = 0;

		bcopy(ucode + count, data, bsize);

		bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
		if (malo_send_cmd(sc, sc->sc_cmd_dmaaddr, 5) != 0) {
			printf("%s: timeout at firmware upload!\n",
			    sc->sc_dev.dv_xname);
			free(ucode, M_DEVBUF);
			return (ETIMEDOUT);
		}

		delay(100);
	}
	free(ucode, M_DEVBUF);

	DPRINTF(("%s: firmware upload finished\n", sc->sc_dev.dv_xname));
	
	hdr->cmd = htole16(0x0001);
	hdr->size = 0;
	hdr->seqnum = htole16(sn++);
	hdr->result = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	if (malo_send_cmd(sc, sc->sc_cmd_dmaaddr, 0xf0f1f2f4) != 0) {
		printf("%s: timeout at firmware load!\n", sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* give card a bit time to load firmware */
	delay(20000);
	DPRINTF(("%s: firmware loaded\n", sc->sc_dev.dv_xname));

	return (0);
}

int
malo_send_cmd(struct malo_softc *sc, bus_addr_t addr, uint32_t waitfor)
{
	int i;

	malo_ctl_write4(sc, 0x0c10, (uint32_t)addr);
	malo_ctl_read4(sc, 0x0c14);
	malo_ctl_write4(sc, 0x0c18, 2); /* CPU_TRANSFER_CMD */
	malo_ctl_read4(sc, 0x0c14);

	if (waitfor == 0)
		return (0);

	/* wait for the DMA engine to finish the transfer */
	for (i = 0; i < 100; i++) {
		delay(50);
		if (malo_ctl_read4(sc, 0x0c14) == waitfor);
			break;
	}

	if (i == 100)
		return (ETIMEDOUT);

	return (0);
}

int
malo_reset(struct malo_softc *sc)
{
	struct malo_cmdheader *hdr = sc->sc_cmd_mem;

	hdr->cmd = htole16(5);
	hdr->size = htole16(sizeof(*hdr));
	hdr->seqnum = 1;
	hdr->result = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	malo_send_cmd(sc, sc->sc_cmd_dmaaddr, 0);
	tsleep(sc, 0, "malorst", hz);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);

	if (hdr->cmd & MALO_CMD_RESPONSE)
		return (0);
	else
		return (ETIMEDOUT);
}

int
malo_get_spec(struct malo_softc *sc)
{
	struct malo_cmdheader *hdr = sc->sc_cmd_mem;
	struct malo_hw_spec *spec;

	hdr->cmd = htole16(MALO_CMD_GET_HW_SPEC);
	hdr->size = htole16(sizeof(*hdr) + sizeof(*spec));
	hdr->seqnum = htole16(42);	/* the one and only */
	hdr->result = 0;
	spec = (struct malo_hw_spec *)(hdr + 1);

	DPRINTF(("%s: fw cmd %04x size %d\n", sc->sc_dev.dv_xname,
	    hdr->cmd, hdr->size));

	bzero(spec, sizeof(*spec));
	memset(spec->PermanentAddress, 0xff, ETHER_ADDR_LEN);
	spec->CookiePtr = htole32(sc->sc_cookie_dmaaddr);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	malo_send_cmd(sc, sc->sc_cmd_dmaaddr, 0);
	tsleep(sc, 0, "malospc", hz);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmd_dmam, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);

	if ((hdr->cmd & MALO_CMD_RESPONSE) == 0)
		return (ETIMEDOUT);

	/* XXX get the data form the buffer and feed it to ieee80211 */
	DPRINTF(("%s: get_hw_spec: V%x R%x, #Mcast %d, Regcode %d, #Ant %d\n",
	    sc->sc_dev.dv_xname, htole16(spec->HwVersion),
	    htole32(spec->FWReleaseNumber), htole16(spec->NumOfMCastAdr),
	    htole16(spec->RegionCode), htole16(spec->NumberOfAntenna)));

	/* tell the DMA engine where our rings are */
	malo_mem_write4(sc, letoh32(spec->RxPdRd1Ptr) & 0xffff,
	    htole32(sc->sc_rxring0.physaddr));
	malo_mem_write4(sc, letoh32(spec->RxPdRd2Ptr) & 0xffff,
	    htole32(sc->sc_rxring1.physaddr));

	return (0);
}

int
malo_init(struct ifnet *ifp)
{
	struct malo_softc *sc = ifp->if_softc;
	int error;

	DPRINTF(("%s: malo_init\n", ifp->if_xname));
	if (sc->sc_enable)
		sc->sc_enable(sc);

	/* ???, what is this for, seems unnecessary */
	/* malo_ctl_write4(sc, 0x0c38, 0x1f); */
	/* disable interrupts */
	malo_ctl_read4(sc, 0x0c30);
	malo_ctl_write4(sc, 0x0c30, 0);
	malo_ctl_write4(sc, 0x0c34, 0);
	malo_ctl_write4(sc, 0x0c3c, 0);

	/* load firmware */
	malo_load_bootimg(sc);
	malo_load_firmware(sc);

	/* enable interrupts */
	malo_ctl_write4(sc, 0x0c34, 0x1f);
	malo_ctl_read4(sc, 0x0c14);
	malo_ctl_write4(sc, 0x0c3c, 0x1f);
	malo_ctl_read4(sc, 0x0c14);

	if ((error = malo_get_spec(sc)))
		return (error);

	ifp->if_flags |= IFF_RUNNING;

	return (0);
}

int
malo_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct malo_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				malo_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				malo_stop(sc);
		}
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);

	return (error);
}

void
malo_start(struct ifnet *ifp)
{

}

int
malo_stop(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	/* try to reset card, if the firmware is loaded */
	if (ifp->if_flags & IFF_RUNNING)
		malo_reset(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	DPRINTF(("%s: malo_stop\n", ifp->if_xname));
	if (sc->sc_disable)
		sc->sc_disable(sc);

	return (0);
}

void
malo_watchdog(struct ifnet *ifp)
{

}

int
malo_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	return (0);
}

void
malo_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{

}

struct ieee80211_node *
malo_node_alloc(struct ieee80211com *ic)
{
	struct malo_node *wn;

	wn = malloc(sizeof(struct malo_node), M_DEVBUF, M_NOWAIT);
	if (wn == NULL)
		return (NULL);

	bzero(wn, sizeof(struct malo_node));

	return ((struct ieee80211_node *)wn);
}
