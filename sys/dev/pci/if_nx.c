/*	$OpenBSD: if_nx.c,v 1.48 2007/05/26 18:11:42 reyk Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@openbsd.org>
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

/*
 * Driver for the NetXen NX2031/NX2035 10Gb and Gigabit Ethernet chipsets,
 * see http://www.netxen.com/.
 *
 * This driver was made possible because NetXen Inc. provided hardware
 * and documentation. Thanks!
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/sensors.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_nxreg.h>

#ifdef NX_DEBUG
#define NXDBG_WAIT	(1<<0)	/* poll registers */
#define NXDBG_FLASH	(1<<1)	/* debug flash access through ROMUSB */
#define NXDBG_CRBINIT	(1<<2)	/* SW register init from flash */
#define NXDBG_STATE	(1<<3)	/* Firmware states */
#define NXDBG_WINDOW	(1<<4)	/* Memory windows */
#define NXDBG_INTR	(1<<5)	/* Interrupts */
#define NXDBG_TX	(1<<6)	/* Transmit */
#define NXDBG_ALL	0xfffe	/* enable nearly all debugging messages */
int nx_debug = 0;
#define DPRINTF(_lvl, _arg...)	do {					\
	if (nx_debug & (_lvl))						\
		printf(_arg);						\
} while (0)
#define DPRINTREG(_lvl, _reg)	do {					\
	if (nx_debug & (_lvl))						\
		printf("%s: 0x%08x: %08x\n",				\
		    #_reg, _reg, nxb_readcrb(sc, _reg));		\
} while (0)
#else
#define DPRINTREG(_lvl, _reg)
#define DPRINTF(_lvl, arg...)
#endif

#define DEVNAME(_s)	((_s)->_s##_dev.dv_xname)

#ifdef notyet
/*
 * The NetXen firmware and bootloader is about 800k big, don't even try
 * to load the alternative version from disk with small kernels used by
 * the install media. The driver can still try to use the primary firmware
 * and bootloader found in the controller's flash memory.
 */
#ifndef SMALL_KERNEL
#define NXB_LOADFIRMWARE
#endif
#endif

struct nx_softc;

struct nxb_port {
	u_int8_t		 nxp_id;
	u_int8_t		 nxp_mode;
	u_int8_t		 nxp_phy;
	u_int8_t		 nxp_lladdr[ETHER_ADDR_LEN];
	bus_size_t		 nxp_phyregion;

	struct nx_softc		*nxp_nx;
};

struct nxb_dmamem {
	bus_dmamap_t		 nxm_map;
	bus_dma_segment_t	 nxm_seg;
	int			 nxm_nsegs;
	size_t			 nxm_size;
	caddr_t			 nxm_kva;
	const char		*nxm_name;
};

struct nxb_softc {
	struct device		 sc_dev;

	pci_chipset_tag_t	 sc_pc;
	pcitag_t		 sc_tag;
	u_int			 sc_function;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;
	bus_space_tag_t		 sc_dbmemt;
	bus_space_handle_t	 sc_dbmemh;
	bus_size_t		 sc_dbmems;

	int			 sc_window;	/* SW memory window */
	int			 sc_ddrwindow;	/* PCI DDR memory window */
	int			 sc_qdrwindow;	/* PCI QDR memory window */

	pci_intr_handle_t	 sc_ih;

	u_int			 sc_flags;
#define NXFLAG_FWINVALID	 (1<<0)		/* update firmware from disk */

	struct nxb_info		 sc_nxbinfo;	/* Information from flash */
	struct nxb_imageinfo	 sc_nxbimage;	/* Image info from flash */

	int			 sc_state;	/* Firmware state */
	u_int32_t		 sc_fwmajor;	/* Load image major rev */
	u_int32_t		 sc_fwminor;	/* Load image minor rev */
	u_int32_t		 sc_fwbuild;	/* Load image build rev */

	struct nxb_port		 sc_nxp[NX_MAX_PORTS];	/* The nx ports */
	int			 sc_nports;

	struct timeout		 sc_reload;
	int			 sc_reloaded;

	struct ksensor		 sc_sensor;
	struct ksensordev	 sc_sensordev;
};

struct nx_buf {
	bus_dmamap_t		 nb_dmamap;
	struct mbuf		*nb_m;
};

struct nx_ringdata {
	/* Rx and Rx status descriptors */
	struct nxb_dmamem	 rd_rxdma;
	struct nx_rxdesc	*rd_rxring;
	struct nx_buf		 rd_rxbuf[NX_MAX_RX_DESC];
	struct nxb_dmamem	 rd_statusdma;
	struct nx_statusdesc	*rd_statusring;
	struct nx_buf		 rd_statusbuf[NX_MAX_RX_DESC];

	/* Tx descriptors */
	struct nxb_dmamem	 rd_txdma;
	struct nx_txdesc	*rd_txring;
	struct nx_buf		 rd_txbuf[NX_MAX_TX_DESC];
	u_int32_t		 rd_txproducer;
	volatile u_int		 rd_txpending;
};

struct nx_softc {
	struct device		 nx_dev;
	struct arpcom		 nx_ac;
	struct mii_data		 nx_mii;

	bus_space_handle_t	 nx_memh;		/* port phy subregion */

	struct nx_ringcontext	*nx_rc;			/* Rx, Tx, Status */
	struct nxb_dmamem	 nx_rcdma;

	struct nxb_softc	*nx_sc;			/* The nxb board */
	struct nxb_port		*nx_port;		/* Port information */
	void			*nx_ih;

	struct nx_ringdata	*nx_rings;

	struct timeout		 nx_tick;
};

int	 nxb_match(struct device *, void *, void *);
void	 nxb_attach(struct device *, struct device *, void *);
int	 nxb_query(struct nxb_softc *sc);
int	 nxb_newstate(struct nxb_softc *, int);
void	 nxb_reload(void *);
void	 nxb_mountroot(void *);
int	 nxb_loadfirmware(struct nxb_softc *, struct nxb_firmware_header *,
	    u_int8_t **, size_t *);
int	 nxb_reloadfirmware(struct nxb_softc *, struct nxb_firmware_header *,
	    u_int8_t **, size_t *);
void	 nxb_reset(struct nxb_softc *);

u_int32_t nxb_read(struct nxb_softc *, bus_size_t);
void	 nxb_write(struct nxb_softc *, bus_size_t, u_int32_t);
u_int32_t nxb_readcrb(struct nxb_softc *, bus_size_t);
void	 nxb_writecrb(struct nxb_softc *, bus_size_t, u_int32_t);
int	 nxb_writehw(struct nxb_softc *, u_int32_t, u_int32_t);
bus_size_t nxb_set_crbwindow(struct nxb_softc *, bus_size_t);
bus_size_t nxb_set_pciwindow(struct nxb_softc *, bus_size_t);
int	 nxb_wait(struct nxb_softc *, bus_size_t, u_int32_t, u_int32_t,
	    int, u_int);
int	 nxb_read_rom(struct nxb_softc *, u_int32_t, u_int32_t *);

void	 nxb_temp_sensor(void *);
int	 nxb_dmamem_alloc(struct nxb_softc *, struct nxb_dmamem *,
	    bus_size_t, const char *);
void	 nxb_dmamem_free(struct nxb_softc *, struct nxb_dmamem *);

int	 nx_match(struct device *, void *, void *);
void	 nx_attach(struct device *, struct device *, void *);
int	 nx_print(void *, const char *);
int	 nx_media_change(struct ifnet *);
void	 nx_media_status(struct ifnet *, struct ifmediareq *);
void	 nx_link_state(struct nx_softc *);
void	 nx_init(struct ifnet *);
void	 nx_start(struct ifnet *);
void	 nx_stop(struct ifnet *);
void	 nx_watchdog(struct ifnet *);
int	 nx_ioctl(struct ifnet *, u_long, caddr_t);
void	 nx_iff(struct nx_softc *);
void	 nx_tick(void *);
int	 nx_intr(void *);
void	 nx_setlladdr(struct nx_softc *, u_int8_t *);
void	 nx_doorbell(struct nx_softc *nx, u_int8_t, u_int8_t, u_int32_t);
u_int32_t nx_readphy(struct nx_softc *, bus_size_t);
void	 nx_writephy(struct nx_softc *, bus_size_t, u_int32_t);
u_int32_t nx_readcrb(struct nx_softc *, enum nxsw_portreg);
void	 nx_writecrb(struct nx_softc *, enum nxsw_portreg, u_int32_t);

int	 nx_alloc(struct nx_softc *);
void	 nx_free(struct nx_softc *);
struct mbuf *nx_getbuf(struct nx_softc *, bus_dmamap_t, int);
int	 nx_init_rings(struct nx_softc *);
void	 nx_free_rings(struct nx_softc *);

struct cfdriver nxb_cd = {
	0, "nxb", DV_DULL
};
struct cfattach nxb_ca = {
	sizeof(struct nxb_softc), nxb_match, nxb_attach
};

struct cfdriver nx_cd = {
	0, "nx", DV_IFNET
};
struct cfattach nx_ca = {
	sizeof(struct nx_softc), nx_match, nx_attach
};

const struct pci_matchid nxb_devices[] = {
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GXxR },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GCX4 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_4GCU },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_IMEZ },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_HMEZ },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_IMEZ_2 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_HMEZ_2 }
};

const struct nxb_board {
	enum nxb_board_types	brd_type;
	u_int			brd_mode;
	u_int			brd_nports;
	u_int			brd_phy;	/* The default PHY */
} nxb_boards[] = {
	{ NXB_BOARDTYPE_P2SB35_4G,	NXNIU_MODE_GBE, 4, IFM_1000_T },
	{ NXB_BOARDTYPE_P2SB31_10G,	NXNIU_MODE_XGE, 1, IFM_10G_SR },
	{ NXB_BOARDTYPE_P2SB31_2G,	NXNIU_MODE_GBE, 2, IFM_1000_T },
	{ NXB_BOARDTYPE_P2SB31_10GIMEZ,	NXNIU_MODE_XGE, 2, IFM_10G_SR },
	{ NXB_BOARDTYPE_P2SB31_10GHMEZ,	NXNIU_MODE_XGE, 2, IFM_10G_SR },
	{ NXB_BOARDTYPE_P2SB31_10GCX4,	NXNIU_MODE_XGE, 1, IFM_10G_CX4 }
};

/* Use mapping table, see if_nxreg.h for details */
const u_int32_t nx_swportreg[NXSW_PORTREG_MAX][NX_MAX_PORTS] = NXSW_PORTREGS;
#define NXSW_PORTREG(_p, _r)	(nx_swportreg[_r][nx->nx_port->nxp_id])

/*
 * Routines handling the physical ''nxb'' board
 */

int
nxb_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux,
	    nxb_devices, sizeof(nxb_devices) / sizeof(nxb_devices[0])));
}

void
nxb_attach(struct device *parent, struct device *self, void *aux)
{
	struct nxb_softc	*sc = (struct nxb_softc *)self;
	struct pci_attach_args	*pa = aux;
	pcireg_t		 memtype;
	const char		*intrstr;
	bus_size_t		 pcisize;
	paddr_t			 pciaddr;
	int			 i;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_function = pa->pa_function;
	sc->sc_window = -1;
	sc->sc_ddrwindow = -1;
	sc->sc_qdrwindow = -1;

	/*
	 * The NetXen NICs can have different PCI memory layouts which
	 * need some special handling in the driver. Support is limited
	 * to 32bit 128MB memory for now (the chipset uses a configurable
	 * window to access the complete memory range).
	 */
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, NXBAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		break;
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
	default:
		printf(": invalid memory type: 0x%x\n", memtype);
		return;
	}
	if (pci_mapreg_info(sc->sc_pc, sc->sc_tag, NXBAR0,
	    memtype, &pciaddr, &pcisize, NULL)) {
		printf(": failed to get pci info\n");
		return;
	}
	switch (pcisize) {
	case NXPCIMEM_SIZE_128MB:
		break;
	case NXPCIMEM_SIZE_32MB:
	default:
		printf(": invalid memory size: %ld\n", pcisize);
		return;
	}

	/* Finally map the PCI memory space */
	if (pci_mapreg_map(pa, NXBAR0, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map register memory\n");
		return;
	}
	if (pci_mapreg_map(pa, NXBAR4, memtype, 0, &sc->sc_dbmemt,
	    &sc->sc_dbmemh, NULL, &sc->sc_dbmems, 0) != 0) {
		printf(": unable to map doorbell memory\n");
		goto unmap1;
	}

	/* Get the board information and initialize the h/w */
	if (nxb_query(sc) != 0)
		goto unmap;

	/* Map the interrupt, the handlers will be attached later */
	if (pci_intr_map(pa, &sc->sc_ih) != 0) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, sc->sc_ih);
	printf(": %s\n", intrstr);

	for (i = 0; i < sc->sc_nports; i++)
		config_found(&sc->sc_dev, &sc->sc_nxp[i], nx_print);

	/* Initialize sensor data */
	strlcpy(sc->sc_sensordev.xname, DEVNAME(sc),
	    sizeof(sc->sc_sensordev.xname));
	sc->sc_sensor.type = SENSOR_TEMP;
	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);
	sensordev_install(&sc->sc_sensordev);

	timeout_set(&sc->sc_reload, nxb_reload, sc);
	mountroothook_establish(nxb_mountroot, sc);

	return;

 unmap:
	bus_space_unmap(sc->sc_dbmemt, sc->sc_dbmemh, sc->sc_dbmems);
	sc->sc_dbmems = 0;
 unmap1:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
nxb_query(struct nxb_softc *sc)
{
	struct nxb_info		*ni = &sc->sc_nxbinfo;
	struct nxb_userinfo	*nu;
	u_int32_t		*data, addr;
	u_int8_t		*ptr;
	const struct nxb_board	*board = NULL;
	u_int			 i, j, len;

	/*
	 * Get the board information from flash memory
	 */
	addr = NXFLASHMAP_INFO;
	len = sizeof(*ni) / sizeof(u_int32_t);
	data = (u_int32_t *)ni;
	for (i = 0; i < len; i++) {
		if (nxb_read_rom(sc, addr, data) != 0) {
			printf(": failed to get board info from flash\n");
			return (-1);
		}
		addr += sizeof(u_int32_t);
		data++;
	}

#ifdef NX_DEBUG
#define _NXBINFO(_e)	do {						\
	if (nx_debug & NXDBG_FLASH)					\
		printf("%s: %s: 0x%08x (%u)\n",				\
		    DEVNAME(sc), #_e, ni->_e, ni->_e);		\
} while (0)
	_NXBINFO(ni_hdrver);
	_NXBINFO(ni_board_mfg);
	_NXBINFO(ni_board_type);
	_NXBINFO(ni_board_num);
	_NXBINFO(ni_chip_id);
	_NXBINFO(ni_chip_minor);
	_NXBINFO(ni_chip_major);
	_NXBINFO(ni_chip_pkg);
	_NXBINFO(ni_chip_lot);
	_NXBINFO(ni_port_mask);
	_NXBINFO(ni_peg_mask);
	_NXBINFO(ni_icache);
	_NXBINFO(ni_dcache);
	_NXBINFO(ni_casper);
	_NXBINFO(ni_lladdr0_low);
	_NXBINFO(ni_lladdr1_low);
	_NXBINFO(ni_lladdr2_low);
	_NXBINFO(ni_lladdr3_low);
	_NXBINFO(ni_mnsync_mode);
	_NXBINFO(ni_mnsync_shift_cclk);
	_NXBINFO(ni_mnsync_shift_mclk);
	_NXBINFO(ni_mnwb_enable);
	_NXBINFO(ni_mnfreq_crystal);
	_NXBINFO(ni_mnfreq_speed);
	_NXBINFO(ni_mnorg);
	_NXBINFO(ni_mndepth);
	_NXBINFO(ni_mnranks0);
	_NXBINFO(ni_mnranks1);
	_NXBINFO(ni_mnrd_latency0);
	_NXBINFO(ni_mnrd_latency1);
	_NXBINFO(ni_mnrd_latency2);
	_NXBINFO(ni_mnrd_latency3);
	_NXBINFO(ni_mnrd_latency4);
	_NXBINFO(ni_mnrd_latency5);
	_NXBINFO(ni_mnrd_latency6);
	_NXBINFO(ni_mnrd_latency7);
	_NXBINFO(ni_mnrd_latency8);
	_NXBINFO(ni_mndll[0]);
	_NXBINFO(ni_mnddr_mode);
	_NXBINFO(ni_mnddr_extmode);
	_NXBINFO(ni_mntiming0);
	_NXBINFO(ni_mntiming1);
	_NXBINFO(ni_mntiming2);
	_NXBINFO(ni_snsync_mode);
	_NXBINFO(ni_snpt_mode);
	_NXBINFO(ni_snecc_enable);
	_NXBINFO(ni_snwb_enable);
	_NXBINFO(ni_snfreq_crystal);
	_NXBINFO(ni_snfreq_speed);
	_NXBINFO(ni_snorg);
	_NXBINFO(ni_sndepth);
	_NXBINFO(ni_sndll);
	_NXBINFO(ni_snrd_latency);
	_NXBINFO(ni_lladdr0_high);
	_NXBINFO(ni_lladdr1_high);
	_NXBINFO(ni_lladdr2_high);
	_NXBINFO(ni_lladdr3_high);
	_NXBINFO(ni_magic);
	_NXBINFO(ni_mnrd_imm);
	_NXBINFO(ni_mndll_override);
#undef _NXBINFO
#endif /* NX_DEBUG */

	/* Validate the board information from flash */
	if (ni->ni_hdrver != NXB_VERSION) {
		printf(": unsupported flash info header version %u\n",
		    ni->ni_hdrver);
		return (-1);
	}
	if (ni->ni_magic != NXB_MAGIC) {
		printf(": flash info magic value mismatch\n");
		return (-1);
	}

	/* Lookup the board */
	for (i = 0; i < (sizeof(nxb_boards) / sizeof(nxb_boards[0])); i++) {
		if (ni->ni_board_type == nxb_boards[i].brd_type) {
			board = &nxb_boards[i];
			break;
		}
	}
	if (board == NULL) {
		printf(": unsupported board type %u\n", ni->ni_board_type);
		return (-1);
	}

	/* Configure the ports */
	sc->sc_nports = board->brd_nports;
	for (i = 0; i < sc->sc_nports; i++) {
		sc->sc_nxp[i].nxp_id = i;
		sc->sc_nxp[i].nxp_mode = board->brd_mode;
		sc->sc_nxp[i].nxp_phy = board->brd_phy;
		switch (board->brd_mode) {
		case NXNIU_MODE_XGE:
			sc->sc_nxp[i].nxp_phyregion = NXNIU_XGE(i);
			break;
		case NXNIU_MODE_GBE:
			sc->sc_nxp[i].nxp_phyregion = NXNIU_GBE(i);
			break;
		case NXNIU_MODE_FC:
			sc->sc_nxp[i].nxp_phyregion = NXNIU_FC(i);
			break;
		}
	}

	/*
	 * Get the user information from flash memory
	 */
	if ((nu = (struct nxb_userinfo *)
	    malloc(sizeof(*nu), M_TEMP, M_NOWAIT)) == NULL) {
		printf(": failed to allocate user info\n");
		return (-1);
	}
	addr = NXFLASHMAP_USER;
	len = sizeof(*nu) / sizeof(u_int32_t);
	data = (u_int32_t *)nu;
	for (i = 0; i < len; i++) {
		if (nxb_read_rom(sc, addr, data) != 0) {
			printf(": failed to get user info from flash\n");
			free(nu, M_TEMP);
			return (-1);
		}
		addr += sizeof(u_int32_t);
		data++;
	}

	/* Copy the MAC addresses */
	for (i = 0; i < sc->sc_nports; i++) {
		ptr = (u_int8_t *)
		    &nu->nu_lladdr[i * NXB_MAX_PORT_LLADDRS];
		/* MAC address bytes are stored in a swapped order */
		for (j = 0; j < ETHER_ADDR_LEN; j++)
			sc->sc_nxp[i].nxp_lladdr[j] =
			    ptr[ETHER_ADDR_LEN - (j + 1)];
	}

	/* Make sure that the serial number is a NUL-terminated string */
	nu->nu_serial_num[31] = '\0';

	/* Copy flash image information */
	bcopy(&nu->nu_image, &sc->sc_nxbimage, sizeof(sc->sc_nxbimage));

#ifdef NX_DEBUG
#define _NXBUSER(_e)	do {						\
	if (nx_debug & NXDBG_FLASH)					\
		printf("%s: %s: 0x%08x (%u)\n",				\
		    DEVNAME(sc), #_e, nu->_e, nu->_e);		\
} while (0)
	_NXBUSER(nu_image.nim_bootld_ver);
	_NXBUSER(nu_image.nim_bootld_size);
	_NXBUSER(nu_image.nim_image_ver);
	_NXBUSER(nu_image.nim_image_size);
	_NXBUSER(nu_primary);
	_NXBUSER(nu_secondary);
	_NXBUSER(nu_subsys_id);
	DPRINTF(NXDBG_FLASH, "%s: nu_serial_num: %s\n",
	    DEVNAME(sc), nu->nu_serial_num);
	_NXBUSER(nu_bios_ver);
#undef _NXBUSER
#endif

	free(nu, M_TEMP);

	/*
	 * bootstrap the firmware, the status will be polled in the
	 * mountroot hook.
	 */
	nxb_newstate(sc, NX_S_BOOT);

	return (0);
}

int
nxb_newstate(struct nxb_softc *sc, int newstate)
{
	int	 oldstate = sc->sc_state;

	sc->sc_state = newstate;
	DPRINTF(NXDBG_STATE, "%s(%s) state %d -> %d\n",
	    DEVNAME(sc), __func__, oldstate, newstate);

	switch (newstate) {
	case NX_S_RESET:
		timeout_del(&sc->sc_reload);
		nxb_reset(sc);
		break;
	case NX_S_BOOT:
		/*
		 * Initialize and bootstrap the device
		 */
		nxb_writecrb(sc, NXSW_CMD_PRODUCER_OFF, 0);
		nxb_writecrb(sc, NXSW_CMD_CONSUMER_OFF, 0);
		nxb_writecrb(sc, NXSW_DRIVER_VER, NX_FIRMWARE_VER);
		nxb_writecrb(sc, NXROMUSB_GLB_PEGTUNE,
		    NXROMUSB_GLB_PEGTUNE_DONE);
		break;
	case NX_S_LOADED:
		/*
		 * Initially wait for the device to become ready
		 */
		assert(oldstate == NX_S_BOOT);
		timeout_del(&sc->sc_reload);
		if (nxb_wait(sc, NXSW_CMDPEG_STATE, NXSW_CMDPEG_INIT_DONE,
		    NXSW_CMDPEG_STATE_M, 1, 2000000) != 0) {
			printf("%s: bootstrap failed, code 0x%x\n",
			    DEVNAME(sc),
			    nxb_readcrb(sc, NXSW_CMDPEG_STATE));
			sc->sc_state = NX_S_FAIL;
			return (-1);
		}
		break;
	case NX_S_RELOADED:
		assert(oldstate == NX_S_RESET || oldstate == NX_S_BOOT);
		/*
		 * Wait for the device to become ready
		 */
		sc->sc_reloaded = 20000;
		timeout_add(&sc->sc_reload, hz / 100);
		break;
	case NX_S_READY:
		nxb_temp_sensor(sc);
		break;
	case NX_S_FAIL:
		if (oldstate == NX_S_RELOADED)
			printf("%s: failed to reset the firmware, "
			    "code 0x%x\n", DEVNAME(sc),
			    nxb_readcrb(sc, NXSW_CMDPEG_STATE));
		break;
	default:
		/* no action */
		break;
	}

	return (0);
}

void
nxb_mountroot(void *arg)
{
	struct nxb_softc	*sc = (struct nxb_softc *)arg;

	/*
	 * Poll the status of the running firmware.
	 */
	if (nxb_newstate(sc, NX_S_LOADED) != 0)
		return;

	/* Start sensor */
	sensor_task_register(sc, nxb_temp_sensor, NX_POLL_SENSOR);

	/*
	 * Get and validate the loaded firmware version
	 */
	sc->sc_fwmajor = nxb_readcrb(sc, NXSW_FW_VERSION_MAJOR);
	sc->sc_fwminor = nxb_readcrb(sc, NXSW_FW_VERSION_MINOR);
	sc->sc_fwbuild = nxb_readcrb(sc, NXSW_FW_VERSION_BUILD);
	printf("%s: firmware %u.%u.%u", DEVNAME(sc),
	    sc->sc_fwmajor, sc->sc_fwminor, sc->sc_fwbuild);
	if (sc->sc_fwmajor != NX_FIRMWARE_MAJOR ||
	    sc->sc_fwminor != NX_FIRMWARE_MINOR) {
		printf(", requires %u.%u.xx (%u.%u.%u)",
		    NX_FIRMWARE_MAJOR, NX_FIRMWARE_MINOR,
		    NX_FIRMWARE_MAJOR, NX_FIRMWARE_MINOR,
		    NX_FIRMWARE_BUILD);
		sc->sc_flags |= NXFLAG_FWINVALID;
	}
	printf("\n");

	nxb_newstate(sc, NX_S_RESET);
}

void
nxb_reload(void *arg)
{
	struct nxb_softc	*sc = (struct nxb_softc *)arg;
	u_int32_t		 val;

	/*
	 * Check if the device is ready, other re-schedule or timeout
	 */
	val = nxb_readcrb(sc, NXSW_CMDPEG_STATE);
	if (((val & NXSW_CMDPEG_STATE_M) != NXSW_CMDPEG_INIT_DONE) &&
	    ((val & NXSW_CMDPEG_STATE_M) != NXSW_CMDPEG_INIT_ACK)) {
		if (!sc->sc_reloaded--)
			nxb_newstate(sc, NX_S_FAIL);
		else
			timeout_add(&sc->sc_reload, hz / 100);
		return;
	}
	nxb_writecrb(sc, NXSW_MPORT_MODE, NXSW_MPORT_MODE_NFUNC);
	nxb_writecrb(sc, NXSW_CMDPEG_STATE, NXSW_CMDPEG_INIT_ACK);

	/* Firmware is ready for operation, allow interrupts etc. */
	nxb_newstate(sc, NX_S_READY);
}

int
nxb_loadfirmware(struct nxb_softc *sc, struct nxb_firmware_header *fh,
    u_int8_t **fw, size_t *fwlen)
{
#ifdef NXB_LOADFIRMWARE
	u_int8_t	*mem;
	size_t		 memlen;

	/*
	 * Load a supported bootloader and firmware image from disk
	 */
	if (loadfirmware("nxb", &mem, &memlen) != 0)
		return (-1);

	if ((memlen) < sizeof(*fh))
		goto fail;
	bcopy(mem, fh, sizeof(*fh));
	if (ntohl(fh->fw_hdrver) != NX_FIRMWARE_HDRVER)
		goto fail;

	*fw = mem;
	*fwlen = memlen;

	return (0);
 fail:
	free(mem, M_DEVBUF);
#endif
	return (-1);
}

int
nxb_reloadfirmware(struct nxb_softc *sc, struct nxb_firmware_header *fh,
    u_int8_t **fw, size_t *fwlen)
{
	u_int8_t	*mem;
	size_t		 memlen;
	u_int32_t	 addr, *data;
	u_int		 i;
	size_t		 bsize = NXFLASHMAP_BOOTLDSIZE;

	/*
	 * Load the images from flash, setup a fake firmware header
	 */
	memlen = bsize + sizeof(*fh);
	mem = (u_int8_t *)malloc(memlen, M_DEVBUF, M_NOWAIT);
	if (mem == NULL)
		return (-1);

	fh->fw_hdrver = htonl(NX_FIRMWARE_HDRVER);
	fh->fw_image_ver = htonl(sc->sc_nxbimage.nim_image_ver);
	fh->fw_image_size = 0;	/* Reload firmware image from flash */
	fh->fw_bootld_ver = htonl(sc->sc_nxbimage.nim_bootld_ver);
	fh->fw_bootld_size = htonl(bsize);
	bcopy(fh, mem, sizeof(*fh));

	addr = NXFLASHMAP_BOOTLOADER;
	data = (u_int32_t *)(mem + sizeof(*fh));
	for (i = 0; i < (bsize / 4); i++) {
		if (nxb_read_rom(sc, addr, data) != 0)
			goto fail;
		addr += sizeof(u_int32_t);
		*data++;
	}

	*fw = mem;
	*fwlen = memlen;

	return (0);
 fail:
	free(mem, M_DEVBUF);
	return (-1);
}

void
nxb_reset(struct nxb_softc *sc)
{
	struct nxb_firmware_header	 fh;
	u_int8_t			*fw = NULL;
	size_t				 fwlen = 0;
	int				 bootsz, imagesz;
	u_int				 i;
	u_int32_t			*data, addr, addr1, val, ncrb;
	bus_size_t			 reg;

	/* Reset the SW state */
	nxb_writecrb(sc, NXSW_CMDPEG_STATE, 0);

	/*
	 * Load the firmware from disk or from flash
	 */
	bzero(&fh, sizeof(fh));
	if (sc->sc_flags & NXFLAG_FWINVALID) {
		if (nxb_loadfirmware(sc, &fh, &fw, &fwlen) != 0) {
			printf("%s: failed to load firmware from disk\n",
			    DEVNAME(sc));
			goto fail;
		}
	} else {
		if (nxb_reloadfirmware(sc, &fh, &fw, &fwlen) != 0) {
			printf("%s: failed to reload firmware from flash\n",
			    DEVNAME(sc));
			goto fail;
		}
	}

	/*
	 * Validate the information found in the extra header
	 */
	val = ntohl(fh.fw_image_ver);
	sc->sc_fwmajor = (val & NXB_IMAGE_MAJOR_M) >> NXB_IMAGE_MAJOR_S;
	sc->sc_fwminor = (val & NXB_IMAGE_MINOR_M) >> NXB_IMAGE_MINOR_S;
	sc->sc_fwbuild = (val & NXB_IMAGE_BUILD_M) >> NXB_IMAGE_BUILD_S;
	if (sc->sc_flags & NXFLAG_FWINVALID)
		printf("%s: using firmware %u.%u.%u\n", DEVNAME(sc),
		    sc->sc_fwmajor, sc->sc_fwminor, sc->sc_fwbuild);
	if (sc->sc_fwmajor != NX_FIRMWARE_MAJOR ||
	    sc->sc_fwminor != NX_FIRMWARE_MINOR) {
		printf("%s: unsupported firmware version\n",
		    DEVNAME(sc));
		goto fail;
	}

	bootsz = ntohl(fh.fw_bootld_size);
	imagesz = ntohl(fh.fw_image_size);
	if ((imagesz + bootsz) != (fwlen - sizeof(fh)) ||
	    (imagesz % 4) || (bootsz % 4)) {
		printf("%s: invalid firmware image\n", DEVNAME(sc));
		goto fail;
	}

	/*
	 * Reset the SW registers
	 */

	/* 1. Halt the hardware */
	nxb_writecrb(sc, NXROMUSB_GLB_SW_RESET, NXROMUSB_GLB_SW_RESET_DEF);

	/* 2. Read the CRBINIT area from flash memory */
	addr = NXFLASHMAP_CRBINIT_0;
	if (nxb_read_rom(sc, addr, &ncrb) != 0)
		goto fail1;
	ncrb &= NXFLASHMAP_CRBINIT_M;
	if (ncrb == 0 || ncrb > NXFLASHMAP_CRBINIT_MAX)
		goto fail1;	/* ignore CRBINIT and skip step */

	/* 3. Write the CRBINIT area to PCI memory */
	for (i = 0; i < ncrb; i++) {
		addr = NXFLASHMAP_CRBINIT_0 + (i * 8);
		nxb_read_rom(sc, addr + 4, &val);
		nxb_read_rom(sc, addr + 8, &addr1);

		if (nxb_writehw(sc, addr1, val) != 0)
			goto fail1;
	}

	/* 4. Reset the Protocol Processing Engine */
	val = nxb_readcrb(sc, NXROMUSB_GLB_SW_RESET) &
	    ~NXROMUSB_GLB_SW_RESET_PPE;
	nxb_writecrb(sc, NXROMUSB_GLB_SW_RESET, val);

	/* 5. Reset the D & I caches */
	nxb_writecrb(sc, NXPPE_D(0x0e), 0x1e);
	nxb_writecrb(sc, NXPPE_D(0x4c), 0x8);
	nxb_writecrb(sc, NXPPE_I(0x4c), 0x8);

	/* 6. Clear the Protocol Processing Engine */
	nxb_writecrb(sc, NXPPE_0(0x8), 0);
	nxb_writecrb(sc, NXPPE_0(0xc), 0);
	nxb_writecrb(sc, NXPPE_1(0x8), 0);
	nxb_writecrb(sc, NXPPE_1(0xc), 0);
	nxb_writecrb(sc, NXPPE_2(0x8), 0);
	nxb_writecrb(sc, NXPPE_2(0xc), 0);
	nxb_writecrb(sc, NXPPE_3(0x8), 0);
	nxb_writecrb(sc, NXPPE_3(0xc), 0);

	/*
	 * Load the images into RAM
	 */

	/* Reset casper boot chip */
	nxb_writecrb(sc, NXROMUSB_GLB_CAS_RESET, NXROMUSB_GLB_CAS_RESET_ENABLE);

	addr = NXFLASHMAP_BOOTLOADER;
	data = (u_int32_t *)(fw + sizeof(fh));
	for (i = 0; i < (bootsz / 4); i++) {
		reg = nxb_set_pciwindow(sc, addr);
		if (reg == ~0ULL)
			goto fail1;
		nxb_write(sc, reg, *data);
		addr += sizeof(u_int32_t);
		data++;
	}
	if (imagesz) {
		addr = NXFLASHMAP_FIRMWARE_0;
		for (i = 0; i < (imagesz / 4); i++) {
			reg = nxb_set_pciwindow(sc, addr);
			if (reg == ~0ULL)
				goto fail1;
			nxb_write(sc, reg, *data);
			addr += sizeof(u_int32_t);
			data++;
		}
		/* tell the bootloader to load the firmware image from RAM */
		nxb_writecrb(sc, NXSW_BOOTLD_CONFIG, NXSW_BOOTLD_CONFIG_RAM);
	} else {
		/* tell the bootloader to load the firmware image from flash */
		nxb_writecrb(sc, NXSW_BOOTLD_CONFIG, NXSW_BOOTLD_CONFIG_ROM);
	}

	/* Power on the clocks and unreset the casper boot chip */
	nxb_writecrb(sc, NXROMUSB_GLB_CHIPCLKCONTROL,
	    NXROMUSB_GLB_CHIPCLKCONTROL_ON);
	nxb_writecrb(sc, NXROMUSB_GLB_CAS_RESET,
	    NXROMUSB_GLB_CAS_RESET_DISABLE);
	free(fw, M_DEVBUF);
	fw = NULL;

	/*
	 * bootstrap the newly loaded firmware and wait for completion
	 */
	nxb_newstate(sc, NX_S_BOOT);
	if (nxb_newstate(sc, NX_S_RELOADED) != 0)
		goto fail;
	return;
 fail1:
	printf("%s: failed to reset firmware\n", DEVNAME(sc));
 fail:
	nxb_newstate(sc, NX_S_FAIL);
	if (fw != NULL)
		free(fw, M_DEVBUF);
}

u_int32_t
nxb_read(struct nxb_softc *sc, bus_size_t reg)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, reg));
}

void
nxb_write(struct nxb_softc *sc, bus_size_t reg, u_int32_t val)
{
	bus_space_write_4(sc->sc_memt, sc->sc_memh, reg, val);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
nxb_readcrb(struct nxb_softc *sc, bus_size_t reg)
{
	reg = nxb_set_crbwindow(sc, reg);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, reg));
}

void
nxb_writecrb(struct nxb_softc *sc, bus_size_t reg, u_int32_t val)
{
	reg = nxb_set_crbwindow(sc, reg);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, reg, val);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
nxb_writehw(struct nxb_softc *sc, u_int32_t addr, u_int32_t val)
{
	/* Translation table of NIC addresses to PCI addresses */
	static const u_int16_t hwtrans[] = {
		0x29a0, 0x7730, 0x2950, 0x2a50, 0x0000, 0x0d00,
		0x1b10, 0x0e60, 0x0e00, 0x0e10, 0x0e20, 0x0e30,
		0x7000, 0x7010, 0x7020, 0x7030, 0x7040, 0x3400,
		0x3410, 0x3420, 0x3430, 0x3450, 0x3440, 0x3c00,
		0x3c10, 0x3c20, 0x3c30, 0x3c50, 0x3c40, 0x4100,
		0x0000, 0x0d10, 0x0000, 0x0000, 0x4160, 0x0c60,
		0x0c70, 0x0c80, 0x7580, 0x7590, 0x4170, 0x0000,
		0x0890, 0x70a0, 0x70b0, 0x70c0, 0x08d0, 0x08e0,
		0x70f0, 0x4050, 0x4200, 0x4210, 0x0000, 0x0880,
		0x0000, 0x0000, 0x0000, 0x0000, 0x7180, 0x0000
	};
	u_int32_t base = (addr & NXMEMMAP_HWTRANS_M) >> 16;
	bus_size_t reg = ~0;
	u_int i, timo = 1;

	for (i = 0; i < (sizeof(hwtrans) / sizeof(hwtrans[0])); i++) {
		if (hwtrans[i] == base) {
			reg = i << 20;
			break;
		}
	}
	if (reg == ~0)
		return (-1);		/* Invalid address */
	reg += (addr & ~NXMEMMAP_HWTRANS_M) + NXPCIMAP_CRB;

	/* Write value to the register, enable some workarounds */
	if (reg == NXSW_BOOTLD_CONFIG)
		return (0);
	else if (reg == NXROMUSB_GLB_SW_RESET) {
		val = NXROMUSB_GLB_SW_RESET_XDMA;
		timo = hz;
	}
	nxb_writecrb(sc, reg, val);
	delay(timo);

	DPRINTF(NXDBG_CRBINIT, "%s(%s) addr 0x%08x -> reg 0x%08x, "
	    "val 0x%08x\n", DEVNAME(sc), __func__, addr, reg, val);

	return (0);
}

bus_size_t
nxb_set_pciwindow(struct nxb_softc *sc, bus_size_t reg)
{
	int32_t		 window = -1;
	bus_size_t	 wreg = ~0ULL;

	/*
	 * Get the correct offset in the mapped PCI space
	 */
	if (reg <= NXADDR_DDR_NET_END) {
		window = (reg >> NXDDR_WINDOW_S) & NXDDR_WINDOW_M;
		reg -= (window * NXDDR_WINDOW_SIZE);
		if (sc->sc_ddrwindow != window) {
			sc->sc_ddrwindow = window;
			wreg = NXDDR_WINDOW(sc->sc_function);
		}
	} else if (reg >= NXADDR_OCM0 && reg <= NXADDR_OCM0_END) {
		reg -= NXADDR_OCM0;
		reg += NXPCIMAP_OCM0;
	} else if (reg >= NXADDR_OCM1 && reg <= NXADDR_OCM1_END) {
		reg -= NXADDR_OCM1;
		reg += NXPCIMAP_OCM1;
	} else if (reg >= NXADDR_QDR_NET && reg <= NXADDR_QDR_NET_END) {
		reg -= NXADDR_QDR_NET;
		window = (reg >> NXDDR_WINDOW_S) & NXDDR_WINDOW_M;
		reg -= (window * NXQDR_WINDOW_SIZE);
		reg += NXPCIMAP_QDR_NET;
		if (sc->sc_qdrwindow != window) {
			sc->sc_qdrwindow = window;
			wreg = NXQDR_WINDOW(sc->sc_function);
		}
	} else
		reg = ~0ULL;

	/*
	 * Update the PCI window
	 */
	if (wreg != ~0ULL) {
		DPRINTF(NXDBG_WINDOW, "%s(%s) reg 0x%08x window 0x%08x\n",
		    DEVNAME(sc), __func__, sc->sc_window, wreg, window);

		nxb_write(sc, wreg, window);
		(void)nxb_read(sc, wreg);
	}

	return (reg);
}

bus_size_t
nxb_set_crbwindow(struct nxb_softc *sc, bus_size_t reg)
{
	int		 window = 0;

	/* Set the correct CRB window */
	if ((reg >> NXCRB_WINDOW_S) & NXCRB_WINDOW_M) {
		window = 1;
		reg -= NXCRB_WINDOW_SIZE;
	}
	if (sc->sc_window == window)
		return (reg);
	nxb_write(sc, NXCRB_WINDOW(sc->sc_function),
	    window << NXCRB_WINDOW_S);
	(void)nxb_read(sc, NXCRB_WINDOW(sc->sc_function));

	DPRINTF(NXDBG_WINDOW, "%s(%s) window %d -> %d reg 0x%08x\n",
	    DEVNAME(sc), __func__, sc->sc_window, window,
	    reg + (window * NXCRB_WINDOW_SIZE));

	sc->sc_window = window;

	return (reg);
}

int
nxb_wait(struct nxb_softc *sc, bus_size_t reg, u_int32_t val,
    u_int32_t mask, int is_set, u_int timeout)
{
	u_int i;
	u_int32_t data;

	for (i = timeout; i > 0; i--) {
		data = nxb_readcrb(sc, reg) & mask;
		if (is_set) {
			if (data == val)
				goto done;
		} else {
			if (data != val)
				goto done;
		}
		delay(10);
	}

	return (-1);
 done:
	DPRINTF(NXDBG_WAIT, "%s(%s) "
	    "reg 0x%08x completed after %d/%d iterations\n",
	    DEVNAME(sc), __func__, reg, i, timeout);
	return (0);
}

int
nxb_read_rom(struct nxb_softc *sc, u_int32_t addr, u_int32_t *val)
{
	u_int32_t data;
	int ret = 0;

	/*
	 * Need to set a lock and the lock ID to access the flash
	 */
	ret = nxb_wait(sc, NXSEM_FLASH_LOCK,
	    NXSEM_FLASH_LOCKED, NXSEM_FLASH_LOCK_M, 1, 10000);
	if (ret != 0) {
		DPRINTF(NXDBG_FLASH, "%s(%s): ROM lock timeout\n",
		    DEVNAME(sc), __func__);
		return (-1);
	}
	nxb_writecrb(sc, NXSW_ROM_LOCK_ID, NXSW_ROM_LOCK_DRV);

	/*
	 * Setup ROM data transfer
	 */

	/* Set the ROM address */
	nxb_writecrb(sc, NXROMUSB_ROM_ADDR, addr);

	/* The delay is needed to prevent bursting on the chipset */
	nxb_writecrb(sc, NXROMUSB_ROM_ABYTE_CNT, 3);
	delay(100);
	nxb_writecrb(sc, NXROMUSB_ROM_DUMMY_BYTE_CNT, 0);

	/* Set opcode and wait for completion */
	nxb_writecrb(sc, NXROMUSB_ROM_OPCODE, NXROMUSB_ROM_OPCODE_READ);
	ret = nxb_wait(sc, NXROMUSB_GLB_STATUS,
	    NXROMUSB_GLB_STATUS_DONE, NXROMUSB_GLB_STATUS_DONE, 1, 100);
	if (ret != 0) {
		DPRINTF(NXDBG_FLASH, "%s(%s): ROM operation timeout\n",
		    DEVNAME(sc), __func__);
		goto unlock;
	}

	/* Reset counters */
	nxb_writecrb(sc, NXROMUSB_ROM_ABYTE_CNT, 0);
	delay(100);
	nxb_writecrb(sc, NXROMUSB_ROM_DUMMY_BYTE_CNT, 0);

	/* Finally get the value */
	data = nxb_readcrb(sc, NXROMUSB_ROM_RDATA);

	/* Flash data is stored in little endian */
	*val = letoh32(data);

 unlock:
	/*
	 * Release the lock
	 */
	(void)nxb_readcrb(sc, NXSEM_FLASH_UNLOCK);

	return (ret);
}

void
nxb_temp_sensor(void *arg)
{
	struct nxb_softc	*sc = (struct nxb_softc *)arg;
	u_int32_t		 data, val, state;

	if (sc->sc_state != NX_S_READY) {
		sc->sc_sensor.flags = SENSOR_FUNKNOWN;
		return;
	}

	data = nxb_readcrb(sc, NXSW_TEMP);
	state = (data & NXSW_TEMP_STATE_M) >> NXSW_TEMP_STATE_S;
	val = (data & NXSW_TEMP_VAL_M) >> NXSW_TEMP_VAL_S;

	switch (state) {
	case NXSW_TEMP_STATE_NONE:
		sc->sc_sensor.status = SENSOR_S_UNSPEC;
		break;
	case NXSW_TEMP_STATE_OK:
		sc->sc_sensor.status = SENSOR_S_OK;
		break;
	case NXSW_TEMP_STATE_WARN:
		sc->sc_sensor.status = SENSOR_S_WARN;
		break;
	case NXSW_TEMP_STATE_CRIT:
		sc->sc_sensor.status = SENSOR_S_CRIT;
		break;
	default:
		sc->sc_sensor.flags = SENSOR_FUNKNOWN;
		return;
	}
	sc->sc_sensor.value = val * 1000000 + 273150000;
	sc->sc_sensor.flags = 0;
}

int
nxb_dmamem_alloc(struct nxb_softc *sc, struct nxb_dmamem *nxm,
    bus_size_t size, const char *mname)
{
	nxm->nxm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, nxm->nxm_size, 1,
	    nxm->nxm_size, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &nxm->nxm_map) != 0)
		return (1);
	if (bus_dmamem_alloc(sc->sc_dmat, nxm->nxm_size,
	    NX_DMA_ALIGN, 0, &nxm->nxm_seg, 1, &nxm->nxm_nsegs,
	    BUS_DMA_NOWAIT) != 0)
		goto destroy;
	if (bus_dmamem_map(sc->sc_dmat, &nxm->nxm_seg, nxm->nxm_nsegs,
	    nxm->nxm_size, &nxm->nxm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;
	if (bus_dmamap_load(sc->sc_dmat, nxm->nxm_map, nxm->nxm_kva,
	    nxm->nxm_size, NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(nxm->nxm_kva, nxm->nxm_size);
	nxm->nxm_name = mname;

	return (0);
 unmap:
	bus_dmamem_unmap(sc->sc_dmat, nxm->nxm_kva, nxm->nxm_size);
 free:
	bus_dmamem_free(sc->sc_dmat, &nxm->nxm_seg, 1);
 destroy:
	bus_dmamap_destroy(sc->sc_dmat, nxm->nxm_map);
	return (1);
}

void
nxb_dmamem_free(struct nxb_softc *sc, struct nxb_dmamem *nxm)
{
	bus_dmamap_unload(sc->sc_dmat, nxm->nxm_map);
	bus_dmamem_unmap(sc->sc_dmat, nxm->nxm_kva, nxm->nxm_size);
	bus_dmamem_free(sc->sc_dmat, &nxm->nxm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, nxm->nxm_map);
}

/*
 * Routines handling the virtual ''nx'' ports
 */

int
nx_match(struct device *parent, void *match, void *aux)
{
	struct nxb_port		*nxp = (struct nxb_port *)aux;

	if (nxp->nxp_id >= NX_MAX_PORTS)
		return (0);

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
	case NXNIU_MODE_GBE:
		return (1);
	case NXNIU_MODE_FC:
		/* FibreChannel mode is not documented and not supported */
		return (0);
	}

	return (0);
}

void
nx_attach(struct device *parent, struct device *self, void *aux)
{
	struct nxb_softc	*sc = (struct nxb_softc *)parent;
	struct nx_softc		*nx = (struct nx_softc *)self;
	struct nxb_port		*nxp = (struct nxb_port *)aux;
	struct ifnet		*ifp;

	nx->nx_sc = sc;
	nx->nx_port = nxp;
	nxp->nxp_nx = nx;

	if (bus_space_subregion(sc->sc_memt, sc->sc_memh,
	    nxp->nxp_phyregion, NXNIU_PORT_SIZE, &nx->nx_memh) != 0) {
		printf(": unable to map port subregion\n");
		return;
	}

	nx->nx_ih = pci_intr_establish(sc->sc_pc, sc->sc_ih, IPL_NET,
	    nx_intr, nx, DEVNAME(nx));
	if (nx->nx_ih == NULL) {
		printf(": unable to establish interrupt\n");
		return;
	}

	if (nx_alloc(nx) != 0) {
		printf(": unable to allocate ring or dma memory\n");
		pci_intr_disestablish(sc->sc_pc, nx->nx_ih);
		return;
	}

	bcopy(nxp->nxp_lladdr, nx->nx_ac.ac_enaddr, ETHER_ADDR_LEN);
	printf(": address %s\n", ether_sprintf(nx->nx_ac.ac_enaddr));

	ifp = &nx->nx_ac.ac_if;
	ifp->if_softc = nx;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nx_ioctl;
	ifp->if_start = nx_start;
	ifp->if_watchdog = nx_watchdog;
	ifp->if_hardmtu = NX_JUMBO_MTU;
	strlcpy(ifp->if_xname, DEVNAME(nx), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, NX_MAX_TX_DESC - 1);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
		    IFCAP_CSUM_UDPv4;
#endif
	if (nxp->nxp_mode == NXNIU_MODE_GBE)
		ifp->if_baudrate = IF_Gbps(1);
	else
		ifp->if_baudrate = ULONG_MAX;	/* XXX fix if_baudrate */

	ifmedia_init(&nx->nx_mii.mii_media, 0,
	    nx_media_change, nx_media_status);
	ifmedia_add(&nx->nx_mii.mii_media, IFM_ETHER|nxp->nxp_phy, 0, NULL);
	ifmedia_set(&nx->nx_mii.mii_media, IFM_ETHER|nxp->nxp_phy);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&nx->nx_tick, nx_tick, nx);
	timeout_add(&nx->nx_tick, hz);
}

int
nx_print(void *aux, const char *parentname)
{
	struct nxb_port		*nxp = (struct nxb_port *)aux;

	if (parentname)
		printf("nx port %u at %s",
		    nxp->nxp_id, parentname);
	else
		printf(" port %u", nxp->nxp_id);
	return (UNCONF);
}

int
nx_media_change(struct ifnet *ifp)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_port		*nxp = nx->nx_port;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		/* XXX */
		break;
	case NXNIU_MODE_GBE:
		mii_mediachg(&nx->nx_mii);
		break;
	}

	return (0);
}

void
nx_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_softc	*sc = nx->nx_sc;
	struct nxb_port		*nxp = nx->nx_port;
	u_int32_t		 val;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		imr->ifm_active = IFM_ETHER | nxp->nxp_phy | IFM_FDX;
		imr->ifm_status = IFM_AVALID;
		nx_link_state(nx);
		if (!LINK_STATE_IS_UP(ifp->if_link_state))
			break;
		imr->ifm_status |= IFM_ACTIVE;

		/* Flow control */
		imr->ifm_active |= IFM_FLOW | IFM_ETH_RXPAUSE;
		val = nxb_readcrb(sc, NXNIU_XGE_PAUSE_CONTROL);
		if (((val >> NXNIU_XGE_PAUSE_S(nxp->nxp_id)) &
		    NXNIU_XGE_PAUSE_M) == 0)
			imr->ifm_active |= IFM_ETH_TXPAUSE;
		break;
	case NXNIU_MODE_GBE:
		mii_pollstat(&nx->nx_mii);
		imr->ifm_active = nx->nx_mii.mii_media_active;
		imr->ifm_status = nx->nx_mii.mii_media_status;
		mii_mediachg(&nx->nx_mii);
		break;
	}
}

void
nx_link_state(struct nx_softc *nx)
{
	struct nxb_port		*nxp = nx->nx_port;
	struct nxb_softc	*sc = nx->nx_sc;
	struct ifnet		*ifp = &nx->nx_ac.ac_if;
	u_int32_t		 status = 0;
	int			 link_state = LINK_STATE_DOWN;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		status = nxb_readcrb(sc, NXSW_XG_STATE) >> (nxp->nxp_id * 8);
		if (status & NXSW_XG_LINK_UP)
			link_state = LINK_STATE_FULL_DUPLEX;
		if (ifp->if_link_state != link_state) {
			ifp->if_link_state = link_state;
			if_link_state_change(ifp);
		}
		break;
	case NXNIU_MODE_GBE:
		mii_tick(&nx->nx_mii);
		break;
	}
}

void
nx_watchdog(struct ifnet *ifp)
{
	return;
}

int
nx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			 s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &nx->nx_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&nx->nx_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				nx_iff(nx);
			else
				nx_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				nx_stop(ifp);
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &nx->nx_ac) :
		    ether_delmulti(ifr, &nx->nx_ac);
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &nx->nx_mii.mii_media, cmd);
		break;

	default:
		error = ENOTTY;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			nx_iff(nx);
		error = 0;
	}

	splx(s);

	return (error);
}

void
nx_init(struct ifnet *ifp)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_softc	*sc = nx->nx_sc;
	int			 s;

	if (sc->sc_state != NX_S_READY)
		return;

	s = splnet();

	if (nxb_wait(sc, NXSW_PORTREG(nx, NXSW_RCVPEG_STATE),
	    NXSW_RCVPEG_INIT_DONE, NXSW_RCVPEG_STATE_M, 1, 2000) != 0) {
		printf("%s: receive engine not ready, code 0x%x\n",
		    DEVNAME(nx), nx_readcrb(nx, NXSW_RCVPEG_STATE));
		goto done;
	}

	nx_setlladdr(nx, LLADDR(ifp->if_sadl));

	if (nx_init_rings(nx) != 0)
		goto done;

	/* Set and enable interrupts */
	nxb_writecrb(sc, NXSW_GLOBAL_INT_COAL, 0);	/* XXX */
	nxb_writecrb(sc, NXSW_INT_COAL_MODE, 0);	/* XXX */
	nxb_writecrb(sc, NXISR_INT_MASK, NXISR_INT_MASK_ENABLE);
	nxb_writecrb(sc, NXISR_INT_VECTOR, 0);
	nxb_writecrb(sc, NXISR_TARGET_MASK, NXISR_TARGET_MASK_ENABLE);

	DPRINTF(NXDBG_INTR, "%s(%s) enabled interrupts\n",
	    DEVNAME(nx), __func__);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 done:
	splx(s);
}

void
nx_stop(struct ifnet *ifp)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_softc	*sc = nx->nx_sc;
	int			 s;

	s = splnet();
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* Disable interrupts */
	nxb_writecrb(sc, NXISR_INT_MASK, 0);
	nxb_writecrb(sc, NXISR_INT_VECTOR, 0);
	nxb_writecrb(sc, NXISR_TARGET_MASK, 0);

	nx_free_rings(nx);
	splx(s);
}

#define NX_INC(_x, _y)		(_x) = ((_x) + 1) % (_y)
#define NX_TXURN_WARN(_rd)	((_rd)->rd_txpending >= (NX_MAX_TX_DESC - 5))
#define NX_TXURN(_rd)		((_rd)->rd_txpending >= NX_MAX_TX_DESC)

void
nx_start(struct ifnet *ifp)
{
	struct nx_softc		*nx = (struct nx_softc *)ifp->if_softc;
	struct nxb_softc	*sc = nx->nx_sc;
	struct nxb_port		*nxp = nx->nx_port;
	struct nx_ringdata	*rd = nx->nx_rings;
	struct nxb_dmamem	*txm;
	struct mbuf		*m;
	struct nx_buf		*nb;
	struct nx_txdesc	*txd;
	bus_dmamap_t		 map;
	u_int64_t		 port = nxp->nxp_id, nsegs, len;
	u_int32_t		 producer;
	u_int			 i, idx, tx = 0;

	if ((ifp->if_flags & IFF_RUNNING) == 0||
	    (ifp->if_flags & IFF_OACTIVE) ||
	    IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	txm = &rd->rd_txdma;
	producer = rd->rd_txproducer;

	for (;;) {
		if (NX_TXURN(rd)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		idx = rd->rd_txproducer;
		if (idx >= NX_MAX_TX_DESC) {
			printf("%s: tx idx is corrupt\n", DEVNAME(nx));
			ifp->if_oerrors++;
			break;
		}

		txd = &rd->rd_txring[idx];
		bzero(txd, sizeof(*txd));

		nb = &rd->rd_txbuf[idx];
		if (nb->nb_m != NULL) {
			printf("%s: tx ring is corrupt\n", DEVNAME(nx));
			ifp->if_oerrors++;
			break;
		}

		/*
		 * we're committed to sending it now. if we cant map it into
		 * dma memory then we drop it.
		 */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		map = nb->nb_dmamap;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
		    BUS_DMA_NOWAIT) != 0) {
			m_freem(m);
			printf("%s: could not load mbuf dma map", DEVNAME(nx));
			ifp->if_oerrors++;
			break;
		}
		if (map->dm_nsegs > 4) {
			m_freem(m);
			printf("%s: too many segments for tx", DEVNAME(nx));
			ifp->if_oerrors++;
			break;
		}
		nb->nb_m = m;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, nb->nb_m, BPF_DIRECTION_OUT);
#endif

		len = 0;
		nsegs = map->dm_nsegs;
		txd->tx_buflength = 0;
		for (i = 0; i < nsegs; i++) {
			len += map->dm_segs[i].ds_len;
			switch (i) {
			case 0:
				txd->tx_buflength |= (map->dm_segs[i].ds_len <<
				    NX_TXDESC_BUFLENGTH1_S) &
				    NX_TXDESC_BUFLENGTH1_M;
				txd->tx_addr1 = map->dm_segs[i].ds_addr;
				break;
			case 1:
				txd->tx_buflength |= (map->dm_segs[i].ds_len <<
				    NX_TXDESC_BUFLENGTH2_S) &
				    NX_TXDESC_BUFLENGTH2_M;
				txd->tx_addr2 = map->dm_segs[i].ds_addr;
				break;
			case 2:
				txd->tx_buflength |= (map->dm_segs[i].ds_len <<
				    NX_TXDESC_BUFLENGTH3_S) &
				    NX_TXDESC_BUFLENGTH3_M;
				txd->tx_addr3 = map->dm_segs[i].ds_addr;
				break;
			case 3:
				txd->tx_buflength |= (map->dm_segs[i].ds_len <<
				    NX_TXDESC_BUFLENGTH4_S) &
				    NX_TXDESC_BUFLENGTH4_M;
				txd->tx_addr4 = map->dm_segs[i].ds_addr;
				break;
			}
		}
		txd->tx_word0 =
		    ((NX_TXDESC0_OP_TX << NX_TXDESC0_OP_S) & NX_TXDESC0_OP_M) |
		    ((nsegs << NX_TXDESC0_NBUF_S) & NX_TXDESC0_NBUF_M) |
		    ((len << NX_TXDESC0_LENGTH_S) & NX_TXDESC0_LENGTH_M);
		txd->tx_word2 =
		    ((idx << NX_TXDESC2_HANDLE_S) & NX_TXDESC2_HANDLE_M) |
		    ((port << NX_TXDESC2_PORT_S) & NX_TXDESC2_PORT_M) |
		    ((port << NX_TXDESC2_CTXID_S) & NX_TXDESC2_CTXID_M);

		DPRINTF(NXDBG_TX, "%s(%s): txd w0:%016llx w2:%016llx "
		    "a1:%016llx a2:%016llx a3:%016llx a4:%016llx len:%016llx\n",
		    DEVNAME(nx), __func__, txd->tx_word0, txd->tx_word2,
		    txd->tx_addr1, txd->tx_addr2, txd->tx_addr3, txd->tx_addr4,
		    txd->tx_buflength);

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		ifp->if_opackets++;
		rd->rd_txpending++;

		NX_INC(rd->rd_txproducer, NX_MAX_TX_DESC);

		tx = 1;
	}

	bus_dmamap_sync(sc->sc_dmat, txm->nxm_map, 0, txm->nxm_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (tx) {
		DPRINTF(NXDBG_TX, "%s(%s): producer 0x%08x\n",
		    DEVNAME(nx), __func__, producer);
		nxb_writecrb(sc, NXSW_CMD_PRODUCER_OFF, producer);

		/* Ring... */
		nx_doorbell(nx, NXDB_PEGID_TX,
		    NXDB_OPCODE_CMD_PROD, producer);
	}

	return;
}

void
nx_iff(struct nx_softc *nx)
{
	return;
}

void
nx_tick(void *arg)
{
	struct nx_softc		*nx = (struct nx_softc *)arg;
	struct nxb_softc	*sc = nx->nx_sc;

	if (sc->sc_state != NX_S_READY)
		goto done;

	nx_link_state(nx);

 done:
	timeout_add(&nx->nx_tick, hz);
}

int
nx_intr(void *arg)
{
	struct nx_softc		*nx = (struct nx_softc *)arg;
	struct nxb_port		*nxp = nx->nx_port;
	struct nxb_softc	*sc = nx->nx_sc;
	u_int32_t		 inv;
	u_int			 i;

	if (sc->sc_state != NX_S_READY)
		return (0);

	/* Is the interrupt for us? */
	inv = nxb_read(sc, NXISR_INT_VECTOR);
	if ((inv & NXISR_INT_VECTOR_PORT(nxp->nxp_id)) == 0)
		return (0);
	DPRINTF(NXDBG_INTR, "%s(%s): int vector 0x%08x\n",
	    DEVNAME(nx), __func__, inv);

	nxb_writecrb(sc, NXISR_INT_MASK, NXISR_INT_MASK_DISABLE);
	for (i = 0; i < 32; i++) {
		nxb_writecrb(sc, NXISR_TARGET_MASK, NXISR_TARGET_MASK_DISABLE);
		inv = nxb_readcrb(sc, NXISR_INT_VECTOR);
		if ((inv & NXISR_INT_VECTOR_PORT(nxp->nxp_id)) == 0)
			break;
	}
	if (inv)
		printf("%s: failed to disable interrupt\n", DEVNAME(nx));

	/* Ring... */
	DPRINTF(NXDBG_INTR, "%s(%s): consumer 0x%08x\n",
	    DEVNAME(nx), __func__, nx->nx_rc->rc_txconsumer);

	nxb_writecrb(sc, NXISR_INT_MASK, NXISR_INT_MASK_ENABLE);

	return (1);
}

void
nx_setlladdr(struct nx_softc *nx, u_int8_t *lladdr)
{
	struct nxb_softc	*sc = nx->nx_sc;

	nxb_set_crbwindow(sc, NXMEMMAP_WINDOW0_START);
	bus_space_write_region_1(sc->sc_memt, nx->nx_memh,
	    NX_XGE_STATION_ADDR_HI, lladdr, ETHER_ADDR_LEN);
	bus_space_barrier(sc->sc_memt, nx->nx_memh,
	    NX_XGE_STATION_ADDR_HI, ETHER_ADDR_LEN, BUS_SPACE_BARRIER_WRITE);
}

void
nx_doorbell(struct nx_softc *nx, u_int8_t id, u_int8_t cmd, u_int32_t count)
{
	struct nxb_softc	*sc = nx->nx_sc;
	struct nxb_port		*nxp = nx->nx_port;
	u_int32_t		 data;

	/* Create the doorbell message */
	data = ((NXDB_PEGID_S << id) & NXDB_PEGID_M) |
	    ((NXDB_COUNT_S << count) & NXDB_COUNT_M) |
	    ((NXDB_CTXID_S << nxp->nxp_id) & NXDB_CTXID_M) |
	    ((NXDB_OPCODE_S << cmd) & NXDB_OPCODE_M) |
	    NXDB_PRIVID;

	bus_space_write_4(sc->sc_dbmemt, sc->sc_dbmemh, 0, data);
	bus_space_barrier(sc->sc_dbmemt, sc->sc_dbmemh, 0, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
nx_readphy(struct nx_softc *nx, bus_size_t reg)
{
	struct nxb_softc	*sc = nx->nx_sc;

	nxb_set_crbwindow(sc, NXMEMMAP_WINDOW0_START);
	bus_space_barrier(sc->sc_memt, nx->nx_memh, reg, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, nx->nx_memh, reg));
}

void
nx_writephy(struct nx_softc *nx, bus_size_t reg, u_int32_t val)
{
	struct nxb_softc	*sc = nx->nx_sc;

	nxb_set_crbwindow(sc, NXMEMMAP_WINDOW0_START);
	bus_space_write_4(sc->sc_memt, nx->nx_memh, reg, val);
	bus_space_barrier(sc->sc_memt, nx->nx_memh, reg, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
nx_readcrb(struct nx_softc *nx, enum nxsw_portreg n)
{
	struct nxb_softc	*sc = nx->nx_sc;
	u_int32_t		 reg;

	reg = NXSW_PORTREG(nx, n);
	nxb_set_crbwindow(sc, NXMEMMAP_WINDOW1_START);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, reg));
}

void
nx_writecrb(struct nx_softc *nx, enum nxsw_portreg n, u_int32_t val)
{
	struct nxb_softc	*sc = nx->nx_sc;
	u_int32_t		 reg;

	reg = NXSW_PORTREG(nx, n);
	nxb_set_crbwindow(sc, NXMEMMAP_WINDOW1_START);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, reg, val);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, reg, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
nx_alloc(struct nx_softc *nx)
{
	struct nxb_softc	*sc = nx->nx_sc;
	struct nxb_port		*nxp = nx->nx_port;
	struct nxb_dmamem	*nxm;
	struct nx_ringcontext	*rc;
	u_int64_t		 addr;

	/*
	 * One DMA'ed ring context per virtual port
	 */
	nxm = &nx->nx_rcdma;
	if (nxb_dmamem_alloc(sc, nxm, sizeof(*rc), "ringcontext") != 0)
		return (1);

	rc = (struct nx_ringcontext *)nxm->nxm_kva;

	/* Initialize the ring context */
	rc->rc_id = htole32(nxp->nxp_id);
	addr = nxm->nxm_map->dm_segs[0].ds_addr +	/* physaddr */
	    offsetof(struct nx_ringcontext, rc_txconsumer);
	rc->rc_txconsumeroff = htole64(addr);
	nx->nx_rc = rc;

	/*
	 * Ring data used by the driver
	 */
	nx->nx_rings = (struct nx_ringdata *)
	    malloc(sizeof(struct nx_ringdata), M_NOWAIT, M_DEVBUF);
	if (nx->nx_rings == NULL) {
		nxb_dmamem_free(sc, nxm);
		return (1);
	}
	bzero(nx->nx_rings, sizeof(struct nx_ringdata));

	return (0);
}

void
nx_free(struct nx_softc *nx)
{
	struct nxb_softc	*sc = nx->nx_sc;
	nxb_dmamem_free(sc, &nx->nx_rcdma);
}

struct mbuf *
nx_getbuf(struct nx_softc *nx, bus_dmamap_t map, int wait)
{
	struct nxb_softc	*sc = nx->nx_sc;
	struct mbuf		*m = NULL;

	MGETHDR(m, wait ? M_WAIT : M_DONTWAIT, MT_DATA);
	if (m == NULL)
		goto merr;

	MCLGET(m, wait ? M_WAIT : M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0)
		goto merr;
	m->m_len = m->m_pkthdr.len = MCLBYTES;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
	    wait ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT) != 0) {
		printf("%s: could not load mbuf dma map", DEVNAME(nx));
		goto err;
	}

	return (m);
 merr:
	printf("%s: unable to allocate mbuf", DEVNAME(nx));
 err:
	if (m != NULL)
		m_freem(m);
	return (NULL);
}

int
nx_init_rings(struct nx_softc *nx)
{
	struct nxb_softc	*sc = nx->nx_sc;
	struct nxb_port		*nxp = nx->nx_port;
	struct nxb_dmamem	*nxm;
	struct nx_ringcontext	*rc = nx->nx_rc;
	struct nx_ringdata	*rd = nx->nx_rings;
	struct nx_rxcontext	*rxc;
	struct nx_buf		*nb;
	struct nx_rxdesc	*rxd;
	u_int64_t		 addr;
	int			 i, size;

	nxm = &nx->nx_rcdma;
	bus_dmamap_sync(sc->sc_dmat, nxm->nxm_map, 0, nxm->nxm_size,
	    BUS_DMASYNC_PREWRITE);

	/*
	 * Rx descriptors
	 */
	nxm = &rd->rd_rxdma;
	size = NX_MAX_RX_DESC * sizeof(struct nx_rxdesc);
	if (nxb_dmamem_alloc(sc, nxm, size, "rxdesc") != 0) {
		printf("%s: failed to alloc rx dma memory\n",
		    DEVNAME(nx));
		return (1);
	}

	rd->rd_rxring = (struct nx_rxdesc *)nxm->nxm_kva;
	addr = nxm->nxm_map->dm_segs[0].ds_addr;
	rxc = &rc->rc_rxcontext[NX_RX_CONTEXT];
	rxc->rxc_ringaddr = htole64(addr);
	rxc->rxc_ringsize = htole32(NX_MAX_RX_DESC);

	/* Rx buffers */
	for (i = 0; i < NX_MAX_RX_DESC; i++) {
		nb = &rd->rd_rxbuf[i];
		rxd = rd->rd_rxring + i;

		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &nb->nb_dmamap) != 0) {
			printf("%s: unable to create dmamap for rx %d\n",
			    DEVNAME(nx), i);
			goto fail;
		}

		nb->nb_m = nx_getbuf(nx, nb->nb_dmamap, NX_NOWAIT);
		if (nb->nb_m == NULL) {
			bus_dmamap_destroy(sc->sc_dmat, nb->nb_dmamap);
			goto fail;
		}

		bus_dmamap_sync(sc->sc_dmat, nb->nb_dmamap, 0,
		    nb->nb_m->m_pkthdr.len, BUS_DMASYNC_PREREAD);

		addr = nb->nb_dmamap->dm_segs[0].ds_addr;
		rxd->rx_addr = htole64(addr);
		rxd->rx_length = htole32(nb->nb_m->m_pkthdr.len);
		rxd->rx_handle = htole16(i);
	}

	/* XXX Jumbo Rx descriptors/buffers */

	/*
	 * Rx status descriptors
	 */
	nxm = &rd->rd_statusdma;
	size = NX_MAX_STATUS_DESC * sizeof(struct nx_statusdesc);
	if (nxb_dmamem_alloc(sc, nxm, size, "statusdesc") != 0) {
		printf("%s: failed to alloc status dma memory\n",
		    DEVNAME(nx));
		return (1);
	}

	rd->rd_statusring = (struct nx_statusdesc *)nxm->nxm_kva;
	addr = nxm->nxm_map->dm_segs[0].ds_addr;
	rc->rc_statusringaddr = htole64(addr);
	rc->rc_statusringsize = htole32(NX_MAX_STATUS_DESC);

	/*
	 * Tx descriptors
	 */
	nxm = &rd->rd_txdma;
	size = NX_MAX_TX_DESC * sizeof(struct nx_txdesc);
	if (nxb_dmamem_alloc(sc, nxm, size, "txdesc") != 0) {
		printf("%s: failed to alloc tx dma memory\n",
		    DEVNAME(nx));
		return (1);
	}

	rd->rd_txring = (struct nx_txdesc *)nxm->nxm_kva;
	addr = nxm->nxm_map->dm_segs[0].ds_addr;
	rc->rc_txringaddr = htole64(addr);
	rc->rc_txringsize = htole32(NX_MAX_TX_DESC);

	/* Tx buffers */
	for (i = 0; i < NX_MAX_TX_DESC; i++) {
		nb = &rd->rd_txbuf[i];
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 4,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &nb->nb_dmamap) != 0) {
			printf("%s: unable to create dmamap for tx %d\n",
			    DEVNAME(nx), i);
			goto fail;
		}
		nb->nb_m = NULL;
	}

	/*
	 * Sync DMA and write the ring context address to hardware
	 */
	nxm = &nx->nx_rcdma;
	bus_dmamap_sync(sc->sc_dmat, nxm->nxm_map, 0, nxm->nxm_size,
	    BUS_DMASYNC_POSTWRITE);

	addr = nxm->nxm_map->dm_segs[0].ds_addr;
	nx_writecrb(nx, NXSW_CONTEXT_ADDR_LO, addr & 0xffffffff);
	nx_writecrb(nx, NXSW_CONTEXT_ADDR_HI, addr >> 32);
	nx_writecrb(nx, NXSW_CONTEXT, nxp->nxp_id | NXSW_CONTEXT_SIG);

	return (0);

 fail:
	nx_free_rings(nx);
	return (1);
}

void
nx_free_rings(struct nx_softc *nx)
{
	struct nxb_softc	*sc = nx->nx_sc;
	struct nxb_port		*nxp = nx->nx_port;
	struct nx_ringdata	*rd = nx->nx_rings;
	struct nxb_dmamem	*nxm;
	struct nx_buf		*nb;
	int			 i;

	for (i = 0; i < NX_MAX_RX_DESC; i++) {
		nb = &rd->rd_rxbuf[i];
		if (nb->nb_dmamap == NULL)
			continue;
		bus_dmamap_sync(sc->sc_dmat, nb->nb_dmamap, 0,
		    nb->nb_m->m_pkthdr.len, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, nb->nb_dmamap);
		bus_dmamap_destroy(sc->sc_dmat, nb->nb_dmamap);
		if (nb->nb_m != NULL)
			m_freem(nb->nb_m);
		nb->nb_m = NULL;
	}

	for (i = 0; i < NX_MAX_TX_DESC; i++) {
		nb = &rd->rd_txbuf[i];
		if (nb->nb_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, nb->nb_dmamap);
		if (nb->nb_m != NULL)
			m_freem(nb->nb_m);
		nb->nb_m = NULL;
	}

	if (rd->rd_rxdma.nxm_size)
		nxb_dmamem_free(sc, &rd->rd_rxdma);
	if (rd->rd_statusdma.nxm_size)
		nxb_dmamem_free(sc, &rd->rd_statusdma);
	if (rd->rd_txdma.nxm_size)
		nxb_dmamem_free(sc, &rd->rd_txdma);

	nxm = &nx->nx_rcdma;
	bzero(nx->nx_rc, sizeof(struct nx_ringcontext));
	bus_dmamap_sync(sc->sc_dmat, nxm->nxm_map, 0, nxm->nxm_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	nx_writecrb(nx, NXSW_CONTEXT_ADDR_LO, 0);
	nx_writecrb(nx, NXSW_CONTEXT_ADDR_HI, 0);
	nx_writecrb(nx, NXSW_CONTEXT_SIG, nxp->nxp_id | NXSW_CONTEXT_RESET);
}
