/*	$OpenBSD: if_nx.c,v 1.31 2007/05/01 02:20:13 reyk Exp $	*/

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
#include <sys/device.h>

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
#define NXDBG_FLASH	(1<<0)	/* debug flash access through ROMUSB */
#define NXDBG_WAIT	(1<<1)	/* poll registers */
#define NXDBG_CRBINIT	(1<<2)	/* SW register init from flash */
#define NXDBG_ALL	0xffff	/* enable all debugging messages */
int nx_debug = 0;
#define DPRINTF(_lvl, _arg...)	do {					\
	if (nx_debug & (_lvl))						\
		printf(_arg);						\
} while (0)
#define DPRINTREG(_lvl, _reg)	do {					\
	if (nx_debug & (_lvl))						\
		printf("%s: 0x%08x: %08x\n",				\
		    #_reg, _reg, nxb_read(sc, _reg));			\
} while (0)
#else
#define DPRINTREG(_lvl, _reg)
#define DPRINTF(_lvl, arg...)
#endif

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
	u_int8_t		 nxp_lladdr[ETHER_ADDR_LEN];

	struct nx_softc		*nxp_nx;
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

	pci_intr_handle_t	 sc_ih;

	u_int			 sc_flags;
#define NXFLAG_FWINVALID	 (1<<0)		/* update firmware from disk */

	struct nxb_info		 sc_nxbinfo;	/* Information from flash */
	struct nxb_imageinfo	 sc_nxbimage;	/* Image info from flash */

	int			 sc_window;	/* SW memory window */
	int			 sc_state;	/* Firmware state */
	u_int32_t		 sc_fwmajor;	/* Load image major rev */
	u_int32_t		 sc_fwminor;	/* Load image minor rev */
	u_int32_t		 sc_fwbuild;	/* Load image build rev */

	u_int32_t		 sc_nrxbuf;
	u_int32_t		 sc_ntxbuf;
	volatile u_int		 sc_txpending;

	struct nxb_port		 sc_nxp[NX_MAX_PORTS];	/* The nx ports */
	int			 sc_nports;
};

struct nx_softc {
	struct device		 nx_dev;
	struct arpcom		 nx_ac;
	struct mii_data		 nx_mii;

	struct nxb_softc	*nx_sc;			/* The nxb board */
	struct nxb_port		*nx_port;		/* Port information */
	void			*nx_ih;

	struct timeout		 nx_tick;
};

int	 nxb_match(struct device *, void *, void *);
void	 nxb_attach(struct device *, struct device *, void *);
int	 nxb_query(struct nxb_softc *sc);
int	 nxb_newstate(struct nxb_softc *, int);
void	 nxb_mountroot(void *);
int	 nxb_loadfirmware(struct nxb_softc *, struct nxb_firmware_header *,
	    u_int8_t **, size_t *);
int	 nxb_reloadfirmware(struct nxb_softc *, struct nxb_firmware_header *,
	    u_int8_t **, size_t *);
void	 nxb_reset(struct nxb_softc *);

u_int32_t nxb_read(struct nxb_softc *, bus_size_t);
void	 nxb_write(struct nxb_softc *, bus_size_t, u_int32_t);
int	 nxb_writehw(struct nxb_softc *, u_int32_t, u_int32_t);
void	 nxb_set_window(struct nxb_softc *, int);
int	 nxb_wait(struct nxb_softc *, bus_size_t, u_int32_t, u_int32_t,
	    int, u_int);
int	 nxb_read_rom(struct nxb_softc *, u_int32_t, u_int32_t *);

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
} nxb_boards[] = {
	{ NXB_BOARDTYPE_P2SB35_4G,	NXNIU_MODE_GBE, 4 },
	{ NXB_BOARDTYPE_P2SB31_10G,	NXNIU_MODE_XGE, 1 },
	{ NXB_BOARDTYPE_P2SB31_2G,	NXNIU_MODE_GBE, 2 },
	{ NXB_BOARDTYPE_P2SB31_10GIMEZ,	NXNIU_MODE_XGE, 2 },
	{ NXB_BOARDTYPE_P2SB31_10GHMEZ,	NXNIU_MODE_XGE, 2 },
	{ NXB_BOARDTYPE_P2SB31_10GCX4,	NXNIU_MODE_XGE, 1 }
};

extern int ifqmaxlen;

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

	nxb_set_window(sc, 1);

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
		    sc->sc_dev.dv_xname, #_e, ni->_e, ni->_e);		\
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
		    sc->sc_dev.dv_xname, #_e, nu->_e, nu->_e);		\
} while (0)
	_NXBUSER(nu_image.nim_bootld_ver);
	_NXBUSER(nu_image.nim_bootld_size);
	_NXBUSER(nu_image.nim_image_ver);
	_NXBUSER(nu_image.nim_image_size);
	_NXBUSER(nu_primary);
	_NXBUSER(nu_secondary);
	_NXBUSER(nu_subsys_id);
	DPRINTF(NXDBG_FLASH, "%s: nu_serial_num: %s\n",
	    sc->sc_dev.dv_xname, nu->nu_serial_num);
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

	switch (newstate) {
	case NX_S_RESET:
		nxb_reset(sc);
		break;
	case NX_S_BOOT:
		/*
		 * Initialize and bootstrap the device
		 */
		nxb_write(sc, NXSW_CMD_PRODUCER_OFF, 0);
		nxb_write(sc, NXSW_CMD_CONSUMER_OFF, 0);
		nxb_write(sc, NXSW_CMD_ADDR_LO, 0);
		nxb_write(sc, NXSW_DRIVER_VER, NX_FIRMWARE_VER);
		nxb_write(sc, NXROMUSB_GLB_PEGTUNE, NXROMUSB_GLB_PEGTUNE_DONE);
		break;
	case NX_S_LOADED:
		/*
		 * Wait for the device to become ready
		 */
		assert(oldstate == NX_S_BOOT);
		if (nxb_wait(sc, NXSW_CMDPEG_STATE, NXSW_CMDPEG_INIT_DONE,
		    NXSW_CMDPEG_STATE_M, 1, 2000000) != 0) {
			printf("%s: bootstrap failed, code 0x%x\n",
			    sc->sc_dev.dv_xname,
			    nxb_read(sc, NXSW_CMDPEG_STATE));
			sc->sc_state = NX_S_FAIL;
			return (-1);
		}
		break;
	case NX_S_READY:
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

	/*
	 * Get and validate the loaded firmware version
	 */
	sc->sc_fwmajor = nxb_read(sc, NXSW_FW_VERSION_MAJOR);
	sc->sc_fwminor = nxb_read(sc, NXSW_FW_VERSION_MINOR);
	sc->sc_fwbuild = nxb_read(sc, NXSW_FW_VERSION_BUILD);
	printf("%s: firmware %u.%u.%u", sc->sc_dev.dv_xname,
	    sc->sc_fwmajor, sc->sc_fwminor, sc->sc_fwbuild);
	if (sc->sc_fwmajor != NX_FIRMWARE_MAJOR ||
	    sc->sc_fwminor != NX_FIRMWARE_MINOR) {
		printf(", requires %u.%u.xx (%u.%u.%u)\n",
		    NX_FIRMWARE_MAJOR, NX_FIRMWARE_MINOR,
		    NX_FIRMWARE_MAJOR, NX_FIRMWARE_MINOR,
		    NX_FIRMWARE_BUILD);

		sc->sc_flags |= NXFLAG_FWINVALID;
		nxb_newstate(sc, NX_S_RESET);
		return;
	}
	printf("\n");

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

	/*
	 * Load the images from flash, setup a fake firmware header
	 */
	memlen = sc->sc_nxbimage.nim_bootld_size + sizeof(*fh);
	mem = (u_int8_t *)malloc(memlen, M_DEVBUF, M_NOWAIT);
	if (mem == NULL)
		return (-1);

	fh->fw_hdrver = htonl(NX_FIRMWARE_HDRVER);
	fh->fw_image_ver = htonl(sc->sc_nxbimage.nim_image_ver);
	fh->fw_image_size = 0;	/* Reload firmware image from flash */
	fh->fw_bootld_ver = htonl(sc->sc_nxbimage.nim_bootld_ver);
	fh->fw_bootld_size = htonl(sc->sc_nxbimage.nim_bootld_size);
	bcopy(fh, mem, sizeof(*fh));

	addr = NXFLASHMAP_BOOTLOADER;
	data = (u_int32_t *)(mem + sizeof(*fh));
	for (i = 0; i < (sc->sc_nxbimage.nim_bootld_size / 4); i++) {
		if (nxb_read_rom(sc, addr, data) != 0)
			goto fail;
		addr += sizeof(u_int32_t);
		data++;
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
	u_int32_t			*data, addr, val, ncrb;
	bus_size_t			 reg;

	bzero(&fh, sizeof(fh));
	if (sc->sc_flags & NXFLAG_FWINVALID) {
		if (nxb_loadfirmware(sc, &fh, &fw, &fwlen) != 0) {
			printf("%s: failed to load firmware from disk\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	} else {
		if (nxb_reloadfirmware(sc, &fh, &fw, &fwlen) != 0) {
			printf("%s: failed to reload firmware from flash\n",
			    sc->sc_dev.dv_xname);
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
		printf("%s: using firmware %u.%u.%u\n", sc->sc_dev.dv_xname,
		    sc->sc_fwmajor, sc->sc_fwminor, sc->sc_fwbuild);
	if (sc->sc_fwmajor != NX_FIRMWARE_MAJOR ||
	    sc->sc_fwminor != NX_FIRMWARE_MINOR) {
		printf("%s: unsupported firmware version\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bootsz = ntohl(fh.fw_bootld_size);
	imagesz = ntohl(fh.fw_image_size);
	if ((imagesz + bootsz) != (fwlen - sizeof(fh)) ||
	    (imagesz % 4) || (bootsz % 4)) {
		printf("%s: invalid firmware image\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Reset the SW registers
	 */

	/* 1. Halt the hardware */
	nxb_write(sc, NXROMUSB_GLB_SW_RESET, NXROMUSB_GLB_SW_RESET_DEF);

	/* 2. Read the CRBINIT area from flash memory */
	addr = NXFLASHMAP_CRBINIT_0;
	if (nxb_read_rom(sc, addr, &ncrb) != 0)
		goto fail1;
	ncrb &= NXFLASHMAP_CRBINIT_M;
	if (ncrb == 0 || ncrb > NXFLASHMAP_CRBINIT_MAX)
		goto load;	/* ignore CRBINIT and skip step */

	/* 3. Write the CRBINIT are to PCI memory */
	for (i = 0; i < ncrb; i++) {
		reg = i * 8;

		reg += 4;
		nxb_read_rom(sc, reg, &val);
		reg += 4;
		nxb_read_rom(sc, reg, &addr);

		if (nxb_writehw(sc, addr, val) != 0)
			goto fail1;
	}

	/* 4. Reset the Protocol Processing Engine */
	nxb_write(sc, NXROMUSB_GLB_SW_RESET,
	    nxb_read(sc, NXROMUSB_GLB_SW_RESET) &
	    ~NXROMUSB_GLB_SW_RESET_PPE);

	/* 5. Reset the D & I caches */
	nxb_set_window(sc, 0);
	nxb_write(sc, NXPPE_D(0x0e), 0x1e);
	nxb_write(sc, NXPPE_D(0x4c), 0x8);
	nxb_write(sc, NXPPE_I(0x4c), 0x8);

	/* 6. Clear the Protocol Processing Engine */
	nxb_write(sc, NXPPE_0(0x8), 0);
	nxb_write(sc, NXPPE_0(0xc), 0);
	nxb_write(sc, NXPPE_1(0x8), 0);
	nxb_write(sc, NXPPE_1(0xc), 0);
	nxb_write(sc, NXPPE_2(0x8), 0);
	nxb_write(sc, NXPPE_2(0xc), 0);
	nxb_write(sc, NXPPE_3(0x8), 0);
	nxb_write(sc, NXPPE_3(0xc), 0);
	nxb_set_window(sc, 1);

 load:
	/*
	 * Load the images into RAM
	 */

	/* Reset casper boot chip */
	nxb_write(sc, NXROMUSB_GLB_CAS_RESET, NXROMUSB_GLB_CAS_RESET_ENABLE);

	reg = NXFLASHMAP_BOOTLOADER;
	data = (u_int32_t *)(fw + sizeof(fh));
	for (i = 0; i < (bootsz / 4); i++) {
		nxb_write(sc, reg, *data);
		addr += sizeof(u_int32_t);
		data++;
	}
	if (imagesz) {
		reg = NXFLASHMAP_FIRMWARE_0;
		for (i = 0; i < (imagesz / 4); i++) {
			nxb_write(sc, reg, *data);
			addr += sizeof(u_int32_t);
			data++;
		}
		/* tell the bootloader to load the firmware image from RAM */
		nxb_write(sc, NXSW_BOOTLD_CONFIG, NXSW_BOOTLD_CONFIG_RAM);
	}

	/* Power on the clocks and unreset the casper boot chip */
	nxb_write(sc, NXROMUSB_GLB_CHIPCLKCONTROL,
	    NXROMUSB_GLB_CHIPCLKCONTROL_ON);
	nxb_write(sc, NXROMUSB_GLB_CAS_RESET, NXROMUSB_GLB_CAS_RESET_DISABLE);

	/*
	 * bootstrap the newly loaded firmware and wait for completion
	 */
	nxb_newstate(sc, NX_S_BOOT);
	if (nxb_newstate(sc, NX_S_LOADED) != 0)
		goto fail;

	/* Firmware is ready for operation, allow interrupts etc. */
	nxb_newstate(sc, NX_S_READY);
	goto done;
 fail1:
	printf("%s: failed to reset firmware\n", sc->sc_dev.dv_xname);
 fail:
	nxb_newstate(sc, NX_S_FAIL);
 done:
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

int
nxb_writehw(struct nxb_softc *sc, u_int32_t addr, u_int32_t val)
{
	/* Translation table of NIC addresses to PCI addresses */
	static u_int16_t hwtrans[] = {
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
	u_int i, window = 0;

	for (i = 0; i < (sizeof(hwtrans) / sizeof(hwtrans[0])); i++) {
		if (hwtrans[i] == base) {
			reg = i << 20;
			break;
		}
	}
	if (reg == ~0)
		return (-1);		/* Invalid address */
	reg += (addr & ~NXMEMMAP_HWTRANS_M);

	/* Translate window addresst to PCI memory offset */
	if (reg >= NXMEMMAP_WINDOW0_START && reg <= NXMEMMAP_WINDOW0_END)
		window = 0;
	else if (reg >= NXMEMMAP_WINDOW1_START && reg <= NXMEMMAP_WINDOW1_END) {
		reg -= NXMEMMAP_WINDOW1_START;
		window = 1;
	} else
		return (-1);
	reg += NXPCIMAP_CRB;

	/* Write value to the register, enable some workarounds */
	if (reg == NXSW_BOOTLD_CONFIG)
		return (0);
	nxb_set_window(sc, window);
	nxb_write(sc, reg, val);
	nxb_set_window(sc, 1);
	if (reg == NXROMUSB_GLB_SW_RESET)
		delay(1000);
	else
		delay(1);

	DPRINTF(NXDBG_CRBINIT, "%s(%s) addr 0x%08x -> reg 0x%08x, "
	    "val 0x%08x, window %d\n",
	    sc->sc_dev.dv_xname, __func__, addr, reg, val, window);

	return (0);
}

void
nxb_set_window(struct nxb_softc *sc, int window)
{
	u_int32_t val;

	if (sc->sc_window == window)
		return;
	assert(window == 0 || window == 1);
	val = nxb_read(sc, NXCRB_WINDOW(sc->sc_function));
	if (window)
		val |= NXCRB_WINDOW_1;
	else
		val &= ~NXCRB_WINDOW_1;
	nxb_write(sc, NXCRB_WINDOW(sc->sc_function), val);
	sc->sc_window = window;
}

int
nxb_wait(struct nxb_softc *sc, bus_size_t reg, u_int32_t val,
    u_int32_t mask, int is_set, u_int timeout)
{
	u_int i;
	u_int32_t data;

	for (i = timeout; i > 0; i--) {
		data = nxb_read(sc, reg) & mask;
		if (is_set) {
			if (data == val)
				goto done;
		} else {
			if (data != val)
				goto done;
		}
		delay(1);
	}

	return (-1);
 done:
	DPRINTF(NXDBG_WAIT, "%s(%s) "
	    "reg 0x%08x completed after %d/%d iterations\n",
	    sc->sc_dev.dv_xname, __func__, reg, i, timeout);
	return (0);
}

int
nxb_read_rom(struct nxb_softc *sc, u_int32_t addr, u_int32_t *val)
{
	u_int32_t data;
	int ret = 0;

	/* Must be called from window 1 */
	assert(sc->sc_window == 1);

	/*
	 * Need to set a lock and the lock ID to access the flash
	 */
	ret = nxb_wait(sc, NXSEM_FLASH_LOCK,
	    NXSEM_FLASH_LOCKED, NXSEM_FLASH_LOCK_M, 1, 10000);
	if (ret != 0) {
		DPRINTF(NXDBG_FLASH, "%s(%s): ROM lock timeout\n",
		    sc->sc_dev.dv_xname, __func__);
		return (-1);
	}
	nxb_write(sc, NXSW_ROM_LOCK_ID, NXSW_ROM_LOCK_DRV);

	/*
	 * Setup ROM data transfer
	 */

	/* Set the ROM address */
	nxb_write(sc, NXROMUSB_ROM_ADDR, addr);

	/* The delay is needed to prevent bursting on the chipset */
	nxb_write(sc, NXROMUSB_ROM_ABYTE_CNT, 3);
	delay(100);
	nxb_write(sc, NXROMUSB_ROM_DUMMY_BYTE_CNT, 0);

	/* Set opcode and wait for completion */
	nxb_write(sc, NXROMUSB_ROM_OPCODE, NXROMUSB_ROM_OPCODE_READ);
	ret = nxb_wait(sc, NXROMUSB_GLB_STATUS,
	    NXROMUSB_GLB_STATUS_DONE, NXROMUSB_GLB_STATUS_DONE, 1, 100);
	if (ret != 0) {
		DPRINTF(NXDBG_FLASH, "%s(%s): ROM operation timeout\n",
		    sc->sc_dev.dv_xname, __func__);
		goto unlock;
	}

	/* Reset counters */
	nxb_write(sc, NXROMUSB_ROM_ABYTE_CNT, 0);
	delay(100);
	nxb_write(sc, NXROMUSB_ROM_DUMMY_BYTE_CNT, 0);

	/* Finally get the value */
	data = nxb_read(sc, NXROMUSB_ROM_RDATA);

	/* Flash data is stored in little endian */
	*val = letoh32(data);

 unlock:
	/*
	 * Release the lock
	 */
	(void)nxb_read(sc, NXSEM_FLASH_UNLOCK);

	return (ret);
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

	nx->nx_ih = pci_intr_establish(sc->sc_pc, sc->sc_ih, IPL_NET,
	    nx_intr, nx, nx->nx_dev.dv_xname);
	if (nx->nx_ih == NULL) {
		printf(": unable to establish interrupt\n");
		return;
	}

	bcopy(nxp->nxp_lladdr, nx->nx_ac.ac_enaddr, ETHER_ADDR_LEN);
	printf(", address %s\n", ether_sprintf(nx->nx_ac.ac_enaddr));

	ifp = &nx->nx_ac.ac_if;
	ifp->if_softc = nx;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nx_ioctl;
	ifp->if_start = nx_start;
	ifp->if_watchdog = nx_watchdog;
	ifp->if_hardmtu = NX_JUMBO_MTU;
	strlcpy(ifp->if_xname, nx->nx_dev.dv_xname, IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->sc_ntxbuf - 1);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
		    IFCAP_CSUM_UDPv4;
#endif

	ifmedia_init(&nx->nx_mii.mii_media, 0,
	    nx_media_change, nx_media_status);
	ifmedia_add(&nx->nx_mii.mii_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&nx->nx_mii.mii_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&nx->nx_tick, nx_tick, nx);
	timeout_add(&nx->nx_tick, hz);

	return;
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
	struct nxb_port		*nxp = nx->nx_port;

	switch (nxp->nxp_mode) {
	case NXNIU_MODE_XGE:
		imr->ifm_active = IFM_ETHER | IFM_AUTO;
		imr->ifm_status = IFM_AVALID;
		nx_link_state(nx);
		if (LINK_STATE_IS_UP(ifp->if_link_state) &&
		    ifp->if_flags & IFF_UP)
			imr->ifm_status |= IFM_ACTIVE;
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
		status = nxb_read(sc, NXSW_XG_STATE) >> (nxp->nxp_id * 8);
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
	struct ifaddr		*ifa;
	struct ifreq		*ifr;
	int			 s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &nx->nx_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
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
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &nx->nx_ac) :
		    ether_delmulti(ifr, &nx->nx_ac);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				nx_iff(nx);
			error = 0;
		}
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
			nx_init(ifp);
		error = 0;
	}

	splx(s);

	return (error);
}

void
nx_init(struct ifnet *ifp)
{
	return;
}

void
nx_start(struct ifnet *ifp)
{
	return;
}

void
nx_stop(struct ifnet *ifp)
{
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
	struct nxb_softc	*sc = nx->nx_sc;

	if (sc->sc_state != NX_S_READY)
		return (0);

	return (0);
}
