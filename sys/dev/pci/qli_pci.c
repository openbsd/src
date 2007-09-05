/* $OpenBSD: qli_pci.c,v 1.3 2007/09/05 11:05:44 marco Exp $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2007 David Collins <dave@davec.name>
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/rwlock.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/pci/qlireg.h>

#if NBIO > 0
#include <dev/biovar.h>
#include <sys/sensors.h>
#endif /* NBIO > 0 */

#define DEVNAME(_s)     ((_s)->sc_dev.dv_xname)

/* #define QLI_DEBUG */
#ifdef QLI_DEBUG
#define DPRINTF(x...)		do { if (qli_debug) printf(x); } while(0)
#define DNPRINTF(n,x...)	do { if (qli_debug & n) printf(x); } while(0)
#define	QLI_D_CMD		0x0001
#define	QLI_D_INTR		0x0002
#define	QLI_D_MISC		0x0004
#define	QLI_D_DMA		0x0008
#define	QLI_D_IOCTL		0x0010
#define	QLI_D_RW		0x0020
#define	QLI_D_MEM		0x0040
#define	QLI_D_CCB		0x0080
#define	QLI_D_SEM		0x0100
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#ifdef QLI_DEBUG
u_int32_t	qli_debug = 0
		    | QLI_D_CMD
		    | QLI_D_INTR
		    | QLI_D_MISC
		    | QLI_D_DMA
		    | QLI_D_IOCTL
		    | QLI_D_RW
		    | QLI_D_MEM
		    | QLI_D_CCB
		    | QLI_D_SEM
		;
#endif

struct qli_softc {
	struct device		sc_dev;

	void			*sc_ih;
	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_memsize;
	bus_dma_tag_t		sc_dmat;

	volatile struct qli_reg	*sc_reg;	/* pointer to registers */

	/* scsi ioctl from sd device */
	int			(*sc_ioctl)(struct device *, u_long, caddr_t);

	int			sc_ql4010;	/* if set we are a QL4010 HBA */
	u_int32_t		sc_resource;	/* nr for semaphores */

	struct rwlock		sc_lock;
	struct rwlock		sc_mbox_lock;
};

int		qli_scsi_cmd(struct scsi_xfer *);
int		qli_scsi_ioctl(struct scsi_link *, u_long, caddr_t, int,
		    struct proc *);
void		qliminphys(struct buf *bp);
int		qli_pci_find_device(void *);
int		qli_pci_match(struct device *, void *, void *);
void		qli_pci_attach(struct device *, struct device *, void *);
int		qli_ioctl(struct device *, u_long, caddr_t);
int		qli_lock_sem(struct qli_softc *, u_int32_t, u_int32_t);
void		qli_unlock_sem(struct qli_softc *, u_int32_t);
int		qli_lock_driver(struct qli_softc *);
void		qli_write(struct qli_softc *, volatile u_int32_t *, u_int32_t);
u_int32_t	qli_read(struct qli_softc *, volatile u_int32_t *);
void		qli_hw_reset(struct qli_softc *);
int		qli_soft_reset(struct qli_softc *);
int		qli_get_fw_state(struct qli_softc *, u_int32_t *);
int		qli_start_firmware(struct qli_softc *);
int		qli_mgmt(struct qli_softc *, int, u_int32_t *);
int		qli_intr(void *);
int		qli_attach(struct qli_softc *);
#ifndef SMALL_KERNEL
int		qli_create_sensors(struct qli_softc *);
#endif /* SMALL_KERNEL */


struct scsi_adapter qli_switch = {
	qli_scsi_cmd, qliminphys, 0, 0, qli_scsi_ioctl
};

struct scsi_device qli_dev = {
	NULL, NULL, NULL, NULL
};

struct cfdriver qli_cd = {
	NULL, "qli", DV_DULL
};

struct cfattach qli_pci_ca = {
	sizeof(struct qli_softc), qli_pci_match, qli_pci_attach
};

struct	qli_pci_device {
	pcireg_t	qpd_vendor;
	pcireg_t	qpd_product;
	pcireg_t	qpd_subvendor;
	pcireg_t	qpd_subproduct;
	char		*qpd_model;
	uint32_t	qpd_flags;
} qli_pci_devices[] = {
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP4022_HBA,
	  0,			0,		"",			0 },
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP4010_HBA,
	  0,			0,		"",			0 },
	{ 0 }
};

int
qli_pci_find_device(void *aux) {
	struct pci_attach_args	*pa = aux;
	int			i;

	for (i = 0; qli_pci_devices[i].qpd_vendor; i++) {
		if (qli_pci_devices[i].qpd_vendor == PCI_VENDOR(pa->pa_id) &&
		    qli_pci_devices[i].qpd_product == PCI_PRODUCT(pa->pa_id)) {
		    	DNPRINTF(QLI_D_MISC, "qli_pci_find_device: %i\n", i);
			return (i);
		}
	}

	return (-1);
}

int
qli_pci_match(struct device *parent, void *match, void *aux)
{
	int			i;

	if ((i = qli_pci_find_device(aux)) != -1) {
		DNPRINTF(QLI_D_MISC,
		    "qli_pci_match: vendor: %04x  product: %04x\n",
		    qli_pci_devices[i].qpd_vendor,
		    qli_pci_devices[i].qpd_product);

		return (1);
	}
	return (0);
}

void
qli_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct qli_softc	*sc = (struct qli_softc *)self;
	struct pci_attach_args	*pa = aux;
	const char		*intrstr;
	pci_intr_handle_t	ih;
	pcireg_t		memtype;
	int			r;

	/* find the appropriate memory base */
	for (r = PCI_MAPREG_START; r < PCI_MAPREG_END; r += sizeof(memtype)) {
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, r);
		if ((memtype & PCI_MAPREG_TYPE_MASK) == PCI_MAPREG_TYPE_MEM)
			break;
	}
	if (r >= PCI_MAPREG_END) {
		printf(": unable to locate system interface registers\n");
		return;
	}
	if (pci_mapreg_map(pa, r, memtype, BUS_SPACE_MAP_LINEAR, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_memsize, 0)) {
		printf(": can't map controller pci space\n");
		return;
	}
	sc->sc_dmat = pa->pa_dmat;

	/* establish interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, qli_intr, sc,
	    DEVNAME(sc));
	if (!sc->sc_ih) {
		printf(": can't establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		goto unmap;
	}

	/* retrieve kva for register access */
	sc->sc_reg = bus_space_vaddr(sc->sc_memt, sc->sc_memh);
	if (sc->sc_reg == NULL) {
		printf(": can't map registers into kernel\n");
		goto intrdis;
	}
	printf(": %s\n", intrstr);

	sc->sc_ql4010 =
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_QLOGIC_ISP4010_HBA;

	if (qli_attach(sc)) {
		printf("%s: can't attach\n", DEVNAME(sc));
		goto intrdis;
	}

	return;
intrdis:
	pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
unmap:
	sc->sc_ih = NULL;
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_memsize);
}

void
qliminphys(struct buf *bp)
{
	DNPRINTF(QLI_D_MISC, "qliminphys: %d\n", bp->b_bcount);

	/* XXX currently using QLI_MAXFER = MAXPHYS */
	if (bp->b_bcount > QLI_MAXFER)
		bp->b_bcount = QLI_MAXFER;
	minphys(bp);
}

void
qli_write(struct qli_softc *sc, volatile u_int32_t *p, u_int32_t v)
{
	DNPRINTF(QLI_D_RW, "%s: qw 0x%x 0x%08x\n", DEVNAME(sc),
	    (u_int8_t *)p - (u_int8_t *)sc->sc_reg, v);

	*p = letoh32(v);
	bus_space_barrier(sc->sc_memt, sc->sc_memh,
	    (u_int8_t *)p - (u_int8_t *)sc->sc_reg, 4, BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
qli_read(struct qli_softc *sc, volatile u_int32_t *p)
{
	u_int32_t		v;

	bus_space_barrier(sc->sc_memt, sc->sc_memh,
	    (u_int8_t *)p - (u_int8_t *)sc->sc_reg, 4, BUS_SPACE_BARRIER_READ);
	v = letoh32(*p);

	DNPRINTF(QLI_D_RW, "%s: qr 0x%x 0x%08x\n", DEVNAME(sc),
	    (u_int8_t *)p - (u_int8_t *)sc->sc_reg, v);
	return (v);
}

void
qli_hw_reset(struct qli_softc *sc)
{
	u_int32_t		s;

	DNPRINTF(QLI_D_MISC, "%s: qli_hw_reset\n", DEVNAME(sc));

	/* clear scsi reset interrupt bit or soft reset won't work */
	s = qli_read(sc, &sc->sc_reg->qlr_ctrl_status);
	if (s & QLI_REG_CTRLSTAT_SCSI_RESET_INTR)
		qli_write(sc, &sc->sc_reg->qlr_ctrl_status, QLI_SET_MASK(s));

	/* issue soft reset */
	qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
	    QLI_SET_MASK(QLI_REG_CTRLSTAT_SOFT_RESET));
}

int
qli_soft_reset(struct qli_softc *sc)
{
	int			rv = 1, i, failed;
	u_int32_t		s;

	DNPRINTF(QLI_D_MISC, "%s: qli_soft_reset\n", DEVNAME(sc));

	qli_hw_reset(sc);

	/* wait until net reset bit is cleared */
	for (i = 0; i < QLI_SOFT_RESET_RETRIES; i++) {
		s = qli_read(sc, &sc->sc_reg->qlr_ctrl_status);
		if ((s & QLI_REG_CTRLSTAT_NET_RESET_INTR) == 0)
			break;
		delay(1000000); /* 1s */
	}
	s = qli_read(sc, &sc->sc_reg->qlr_ctrl_status);
	if (s & QLI_REG_CTRLSTAT_NET_RESET_INTR) {
		printf("%s: qli_soft_reset: net reset intr bit not cleared\n",
		    DEVNAME(sc));
		/* XXX set the same bit per the linux driver */
		qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
		    QLI_SET_MASK(QLI_REG_CTRLSTAT_NET_RESET_INTR));
	}

	/* wait for soft reset to complete */
	for (i = 0, failed = 1; i < QLI_SOFT_RESET_RETRIES; i++) {
		s = qli_read(sc, &sc->sc_reg->qlr_ctrl_status);
		if ((s & QLI_REG_CTRLSTAT_SOFT_RESET) == 0) {
			failed = 0;
			break;
		}
		delay(1000000); /* 1s */
	}

	/* check if scsi reset interrupt is cleared */
	s = qli_read(sc, &sc->sc_reg->qlr_ctrl_status);
	if (s & QLI_REG_CTRLSTAT_SCSI_RESET_INTR) {
		printf("%s: qli_soft_reset: scsi reset intr bit not cleared\n",
		    DEVNAME(sc));
		/* XXX set the same bit per the linux driver */
		qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
		    QLI_SET_MASK(QLI_REG_CTRLSTAT_SCSI_RESET_INTR));
	}

	if (failed) {
		/* force the soft reset */
		printf("%s: qli_soft_reset: soft reset failed\n", DEVNAME(sc));
		qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
		    QLI_SET_MASK(QLI_REG_CTRLSTAT_FORCE_SOFT_RESET));
		for (i = 0; i < QLI_SOFT_RESET_RETRIES; i++) {
			s = qli_read(sc, &sc->sc_reg->qlr_ctrl_status);
			if ((s & QLI_REG_CTRLSTAT_FORCE_SOFT_RESET) == 0) {
				rv = 0;
				break;
			}
			delay(1000000); /* 1s */
		}
	} else
		rv = 0;

	return (rv);
}

int
qli_get_fw_state(struct qli_softc *sc, u_int32_t *mbox)
{
	int			rv = 1;

	DNPRINTF(QLI_D_MISC, "%s: qli_get_fw_state\n", DEVNAME(sc));

	bzero(mbox, sizeof(mbox));
	mbox[0] = QLI_MBOX_OPC_GET_FW_STATE;
	if (qli_mgmt(sc, 1, mbox))
		goto done;

	DNPRINTF(QLI_D_MISC, "%s: qli_get_fw_state: state: 0x%08x\n",
	    DEVNAME(sc), mbox[1]);
	rv = 0;
done:
	return (rv);
}

int
qli_lock_driver(struct qli_softc *sc)
{
	int			i, rv = 1;

	DNPRINTF(QLI_D_SEM, "%s: qli_lock_driver\n", DEVNAME(sc));

	for (i = 0; i < QLI_SEM_MAX_RETRIES; i++) {
		if (qli_lock_sem(sc, QLI_SEM_DRIVER(sc),
		    QLI_SEM_DRIVER_MASK(sc))) {
			DNPRINTF(QLI_D_SEM, "%s: qli_lock_driver: semaphore"
			    " not acquired, retry %d\n", DEVNAME(sc), i);
			if (cold)
				delay(1000000); /* 1s */
			else
				while (tsleep(sc, PRIBIO + 1, "qlisem", hz) !=
				    EWOULDBLOCK)
					;
		} else {
			DNPRINTF(QLI_D_SEM, "%s: qli_lock_driver: semaphore"
			    " acquired\n", DEVNAME(sc));
			rv = 0;
			break;
		}
	}
	return (rv);
}

void
qli_unlock_sem(struct qli_softc *sc, u_int32_t mask)
{
	DNPRINTF(QLI_D_SEM, "%s: qli_unlock_sem: 0x%08x released\n",
	    DEVNAME(sc), mask);

	qli_write(sc, QLI_SEMAPHORE(sc), mask);
}

int
qli_lock_sem(struct qli_softc *sc, u_int32_t shift, u_int32_t mask)
{
	int			rv = 1;
	u_int32_t		v, s;

	s = sc->sc_resource << shift;
	qli_write(sc, QLI_SEMAPHORE(sc), s | mask);
	v = qli_read(sc, QLI_SEMAPHORE(sc));

	if ((v & (mask >> 16)) == s)
		rv = 0;

	DNPRINTF(QLI_D_SEM, "%s: qli_lock_sem: mask: 0x%08x shift: 0x%08x "
	    "s: 0x%08x v: 0x%08x did %sacquire semaphore \n", DEVNAME(sc),
	    mask, shift, s, v, rv ? "not " : "");

	return (rv);
}

int
qli_start_firmware(struct qli_softc *sc)
{
	int			rv = 1, reset_required = 1, config_required = 0;
	int			boot_required = 0, i;
	u_int32_t		mbox[QLI_MBOX_SIZE], r;

	DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware\n", DEVNAME(sc));

	if (qli_lock_driver(sc)) {
		printf("%s: could not acquire global driver semaphore, "
		    "aborting firmware bring-up\n", DEVNAME(sc));
		goto done;
	}

	if (qli_read(sc, QLI_PORT_CTRL(sc)) & QLI_PORT_CTRL_INITIALIZED) {
		/* Hardware has been initialized */
		DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: hardware has "
		    "been initialized\n", DEVNAME(sc));

		if (qli_read(sc, &sc->sc_reg->qlr_mbox[0]) == 0) {
			/* firmware is not running */
			DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: fw "
			    "not running\n", DEVNAME(sc));
			reset_required = 0;
			config_required = 1;
		} else {
			qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
			    QLI_SET_MASK(QLI_REG_CTRLSTAT_SCSI_RESET_INTR));

			/* issue command to fw to find out if we are up */
			bzero(mbox, sizeof(mbox));
			if (qli_get_fw_state(sc, mbox)) {
				/* command failed, reset chip */
				DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: "
				    "firmware in unknown state, reseting "
				    "chip\n", DEVNAME(sc));
			} else {
				if (mbox[1] & QLI_MBOX_STATE_CONFIG_WAIT) {
					config_required = 1;
					reset_required = 0;
				}
			}
		}
	}

	if (reset_required) {
		if (qli_soft_reset(sc)) {
			printf("%s: soft reset failed, aborting firmware "
			    "bring-up\n", DEVNAME(sc));
			goto done;
		}
		config_required = 1;

		if (qli_lock_driver(sc)) {
			printf("%s: could not acquire global driver semaphore "
			    "after reseting chip, aborting firmware bring-up\n",
			    DEVNAME(sc));
			goto done;
		}
	}

	if (config_required) {
		DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: configuring "
		    "firmware\n", DEVNAME(sc));

		if (qli_lock_sem(sc, QLI_SEM_FLASH(sc),
		    QLI_SEM_FLASH_MASK(sc))) {
			printf("%s: could not lock flash during firmware "
			    "bring-up\n", DEVNAME(sc));
			goto unlock_driver;
		}

		if (qli_lock_sem(sc, QLI_SEM_NVRAM(sc),
		    QLI_SEM_NVRAM_MASK(sc))) {
			printf("%s: could not lock nvram during firmware "
			    "bring-up\n", DEVNAME(sc));
			qli_unlock_sem(sc, QLI_SEM_FLASH_MASK(sc));
			goto unlock_driver;
		}

		if (0 /*qli_validate_nvram(sc)*/) {
			printf("%s: invalid NVRAM checksum.  Flash your "
			    "controller", DEVNAME(sc));

			if (sc->sc_ql4010)
				r = QLI_EXT_HW_CFG_DEFAULT_QL4010;
			else
				r = QLI_EXT_HW_CFG_DEFAULT_QL4022;
		} else {
			/* XXX success, get NVRAM settings instead of default */
			if (sc->sc_ql4010)
				r = QLI_EXT_HW_CFG_DEFAULT_QL4010;
			else
				r = QLI_EXT_HW_CFG_DEFAULT_QL4022;
			printf("%s: fixme, get NVRAM settings, using defaults"
			    " for now\n", DEVNAME(sc));

		}

		/* upper 16 bits are write mask; enable everything */
		qli_write(sc, QLI_EXT_HW_CFG(sc), (0xffff << 16 ) | r);

		qli_unlock_sem(sc, QLI_SEM_NVRAM_MASK(sc));
		qli_unlock_sem(sc, QLI_SEM_FLASH_MASK(sc));

		boot_required = 1;
	}

	if (boot_required) {
		/* boot firmware */
		DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: booting "
		    "firmware\n", DEVNAME(sc));

		/* stuff random value in mbox[7] to randomize source ports */
		/* XXX use random ne instead of 1234  */
		qli_write(sc, &sc->sc_reg->qlr_mbox[7], 1234);

		/* XXX linux driver sets ACB v2 into mbox[6] */

		qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
			QLI_SET_MASK(QLI_REG_CTRLSTAT_BOOT_ENABLE));

		/* wait for firmware to come up */
		for (i = 0; i < 60 * 4 /* up to 60 seconds */; i ++) {
			if (qli_read(sc, &sc->sc_reg->qlr_ctrl_status) &
			    QLI_SET_MASK(QLI_REG_CTRLSTAT_SCSI_PROC_INTR))
				break;
			if (qli_read(sc, &sc->sc_reg->qlr_mbox[0]) ==
			    QLI_MBOX_STATUS_COMMAND_COMPLETE)
				break;
			DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: waiting "
			    "for firmware, retry = %d\n", DEVNAME(sc), i);

			delay(250000); /* 250ms */
		}
		if (qli_read(sc, &sc->sc_reg->qlr_mbox[0]) ==
		    QLI_MBOX_STATUS_COMMAND_COMPLETE) {
			/* firmware is done booting */
			qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
			    QLI_SET_MASK(QLI_REG_CTRLSTAT_SCSI_PROC_INTR));

			DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: firmware "
			    "booting complete\n", DEVNAME(sc));

			rv = 0;
		}
		else {
			DNPRINTF(QLI_D_MISC, "%s: qli_start_firmware: firmware "
			    "booting failed\n", DEVNAME(sc));
			rv = 1;
		}
	}

unlock_driver:
	qli_unlock_sem(sc, QLI_SEM_DRIVER_MASK(sc));
done:
	return (rv);
}

int
qli_mgmt(struct qli_softc *sc, int len, u_int32_t *mbox)
{
	int			rv = 1, s, i;
	u_int32_t		x;

	DNPRINTF(QLI_D_MISC, "%s: qli_mgmt\n", DEVNAME(sc));

	if (!mbox)
		goto done;

	s = splbio();
	rw_enter_write(&sc->sc_mbox_lock);

	if (qli_read(sc, &sc->sc_reg->qlr_ctrl_status) &
	    QLI_REG_CTRLSTAT_SCSI_PROC_INTR) {
		/* this should not happen */
		printf("%s: qli_mgmt called while interrupt is pending\n",
		    DEVNAME(sc));
		qli_intr(sc);
	}

	/* mbox[0] needs to be written last so write backwards */
	DNPRINTF(QLI_D_MISC, "%s: qli_mgmt: ", DEVNAME(sc));
	for (i = QLI_MBOX_SIZE - 1; i >= 0; i--) {
		DNPRINTF(QLI_D_MISC, "mbox[%d] = 0x%08x ", i, mbox[i]);
		qli_write(sc, &sc->sc_reg->qlr_mbox[i], i < len ? mbox[i] : 0);
	}
	DNPRINTF(QLI_D_MISC, "\n");

	/* notify chip it has to deal with mailbox */
	qli_write(sc, &sc->sc_reg->qlr_ctrl_status,
	    QLI_SET_MASK(QLI_REG_CTRLSTAT_EP_INTR));

	/* wait for completion */
	if (cold)
		for (i = 0; i < 6000000 /* up to a minute */; i++) {
			delay(10);
			if ((qli_read(sc, &sc->sc_reg->qlr_ctrl_status) &
			    (QLI_REG_CTRLSTAT_SCSI_RESET_INTR |
			    QLI_REG_CTRLSTAT_SCSI_COMPL_INTR |
			    QLI_REG_CTRLSTAT_SCSI_PROC_INTR)))
				break;
		}
	else {
		/* XXX tsleep */
	}

	DNPRINTF(QLI_D_MISC, "%s: qli_mgmt: ", DEVNAME(sc));
	for (i = 0; i < QLI_MBOX_SIZE; i++) {
		DNPRINTF(QLI_D_MISC, "mbox[%d] = 0x%08x ", i, qli_read(sc,
		    &sc->sc_reg->qlr_mbox[i]));
	}
	DNPRINTF(QLI_D_MISC, "\n");

	x = qli_read(sc, &sc->sc_reg->qlr_mbox[0]);
	switch (x) {
	case QLI_MBOX_STATUS_COMMAND_COMPLETE:
		for (i = 0; i < QLI_MBOX_SIZE; i++)
			mbox[i] = qli_read(sc, &sc->sc_reg->qlr_mbox[i]);
		rv = 0;
		break;
	default:
		printf("%s: qli_mgmt: mailbox failed opcode 0x%08x failed "
		    "with error code 0x%08x\n", DEVNAME(sc), mbox[0], x);
	}

	rw_exit_write(&sc->sc_mbox_lock);
	splx(s);
done:
	return (rv);
}

int
qli_attach(struct qli_softc *sc)
{
	/* struct scsibus_attach_args saa; */
	int			rv = 1;
	u_int32_t		f, mbox[QLI_MBOX_SIZE];

	DNPRINTF(QLI_D_MISC, "%s: qli_attach\n", DEVNAME(sc));

	rw_init(&sc->sc_lock, "qli_lock");
	rw_init(&sc->sc_mbox_lock, "qli_mbox_lock");

	if (sc->sc_ql4010)
		sc->sc_resource = QLI_SEM_4010_SCSI;
	else {
		f = qli_read(sc, &sc->sc_reg->qlr_ctrl_status) &
		    QLI_REG_CTRLSTAT_FUNC_MASK;
		sc->sc_resource = f >> 8;
	}
	DNPRINTF(QLI_D_MISC, "%s: qli_attach resource: %d\n", DEVNAME(sc),
	    sc->sc_resource);

	if (qli_start_firmware(sc)) {
		printf("%s: could not start firmware\n", DEVNAME(sc));
		goto done;
	}

	bzero(mbox, sizeof(mbox));
	mbox[0] = QLI_MBOX_OPC_ABOUT_FIRMWARE;
	if (qli_mgmt(sc, 4, mbox)) {
		printf("%s: about firmware command failed\n", DEVNAME(sc));
		goto done;
	}
	printf("%s: version %d.%d.%d.%d\n", DEVNAME(sc), mbox[1], mbox[2],
	    mbox[3], mbox[4]);

#if NBIO > 0
	if (bio_register(&sc->sc_dev, qli_ioctl) != 0)
		panic("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = qli_ioctl;

#ifndef SMALL_KERNEL
	if (qli_create_sensors(sc) != 0)
		printf("%s: unable to create sensors\n", DEVNAME(sc));
#endif /* SMALL_KERNEL */
#endif /* NBIO > 0 */

done:
	return (rv);
}

int
qli_scsi_cmd(struct scsi_xfer *xs)
{
#ifdef QLI_DEBUG
	struct scsi_link	*link = xs->sc_link;
	struct qli_softc	*sc = link->adapter_softc;

	DNPRINTF(QLI_D_CMD, "%s: qli_scsi_cmd opcode: %#x\n",
	    DEVNAME(sc), xs->cmd->opcode);
#endif

	goto stuffup;

	return (SUCCESSFULLY_QUEUED);

stuffup:
	xs->error = XS_DRIVER_STUFFUP;
	xs->flags |= ITSDONE;
	scsi_done(xs);
	return (COMPLETE);
}

int
qli_intr(void *arg)
{
#ifdef QLI_DEBUG
	struct qli_softc	*sc = arg;

	DNPRINTF(QLI_D_INTR, "%s: qli_intr %#x\n", DEVNAME(sc), sc);
#endif

	return (0);
}

int
qli_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	struct qli_softc	*sc = (struct qli_softc *)link->adapter_softc;

	DNPRINTF(QLI_D_IOCTL, "%s: qli_scsi_ioctl\n", DEVNAME(sc));

	if (sc->sc_ioctl)
		return (sc->sc_ioctl(link->adapter_softc, cmd, addr));
	else
		return (ENOTTY);
}

#if NBIO > 0
int
qli_ioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct qli_softc	*sc = (struct qli_softc *)dev;
	int			error = EINVAL;

	DNPRINTF(QLI_D_IOCTL, "%s: qli_ioctl ", DEVNAME(sc));

	rw_enter_write(&sc->sc_lock);

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(QLI_D_IOCTL, "inq\n");
		break;

	case BIOCVOL:
		DNPRINTF(QLI_D_IOCTL, "vol\n");
		break;

	case BIOCDISK:
		DNPRINTF(QLI_D_IOCTL, "disk\n");
		break;

	case BIOCALARM:
		DNPRINTF(QLI_D_IOCTL, "alarm\n");
		break;

	case BIOCBLINK:
		DNPRINTF(QLI_D_IOCTL, "blink\n");
		break;

	case BIOCSETSTATE:
		DNPRINTF(QLI_D_IOCTL, "setstate\n");
		break;

	default:
		DNPRINTF(QLI_D_IOCTL, " invalid ioctl\n");
		error = EINVAL;
	}

	rw_exit_write(&sc->sc_lock);

	return (error);
}
#endif /* NBIO > 0 */

#ifndef SMALL_KERNEL
int
qli_create_sensors(struct qli_softc *sc)
{
	return (1);
}
#endif /* SMALL_KERNEL */
