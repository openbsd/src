/*	$OpenBSD: vsbic.c,v 1.5 2010/01/10 00:10:23 krw Exp $	*/

/*
 * Copyright (c) 2008, 2009  Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies. And
 * I won't mind if you keep the disclaimer below.
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
 * MVME327A SCSI and floppy controller driver
 *
 * This driver is currently limited to the SCSI part of the board, which
 * is messy enough already.
 */

/* This card lives in an A24/D16 world, but is A32/D32 capable */
#define	__BUS_SPACE_RESTRICT_D16__

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <mvme88k/dev/vme.h>
#include <mvme88k/dev/bppvar.h>

/*
 * Channel packet structure
 */

struct vsbic_pkt {
	/* Command fields, read by firmware */
	uint8_t		cmd_cmd;
	uint8_t		cmd_cmd_ctrl;
	uint8_t		cmd_device;
#define	VSBIC_DEVICE_FLOPPY	0x01
#define	VSBIC_DEVICE_SCSI	0x05
#define	VSBIC_DEVICE_HOST	0x0f
	uint8_t		cmd_unit;
#define	VSBIC_UNIT(tgt,lun)	(((tgt) << 4) | (lun))
	uint16_t	reserved;
	uint8_t		cmd_addrmod;
	uint8_t		cmd_buswidth;
	uint32_t	cmd_addr1;
	uint32_t	cmd_addr2;
#define	cmd_link	cmd_addr2		/* used for free pkt linking */
	uint32_t	cmd_xfer_count;
	uint16_t	cmd_sg_count;
	uint16_t	cmd_parm1;
	uint16_t	cmd_parm2;
	uint16_t	cmd_parm3;

	/* Status fields, written by firmware */
	uint8_t		sts_error;
	uint8_t		sts_recovered;		/* error recovered by fw */
	uint16_t	sts_status;
#define	sts_sense	sts_status
	uint8_t		sts_retries;		/* # of retries performed */
	uint8_t		reserved2;
	uint16_t	sts_addr_high;		/* not aligned... */
	uint16_t	sts_addr_low;
	uint16_t	sts_xfer_high;		/* not aligned... */
	uint16_t	sts_xfer_low;
	uint16_t	sts_parm1;
	uint16_t	sts_parm2;
	uint16_t	sts_parm3;
	/* total size 0x30 bytes */
};

/*
 * Command values (cmd_cmd)
 */

#define	CMD_BPP_TEST			0x00
#define	CMD_READ			0x01
#define	CMD_WRITE			0x02
#define	CMD_READ_DESCRIPTOR		0x03
#define	CMD_WRITE_DESCRIPTOR		0x04
#define	CMD_FORMAT			0x05
#define	CMD_FIX_BAD_SPOT		0x06
#define	CMD_INQUIRY			0x09
#define	CMD_READ_STATUS			0x10
#define	CMD_LOAD_UNLOAD_RETENSION	0x11
#define	CMD_WRITE_FILEMARK		0x12
#define	CMD_REWIND			0x13
#define	CMD_ERASE			0x14
#define	CMD_SPACE			0x15
#define	CMD_ENABLE_TARGET		0x20
#define	CMD_DISABLE_TARGET		0x21
#define	CMD_RESERVE_UNIT		0x22
#define	CMD_RELEASE_UNIT		0x23
#define	CMD_RESET			0x25
#define	CMD_SCSI			0x26
#define	CMD_SELF_TEST			0x27
#define	CMD_TARGET_WAIT			0x28
#define	CMD_TARGET_EXECUTE		0x29
#define	CMD_DOWNLOAD			0x2a
#define	CMD_SET_SCSI_ADDRESS		0x2b
#define	CMD_OPEN			0x2d

/*
 * Error values (sts_error, sts_status)
 */

#define	ERR_OK				0x00
/* command parameter errors */
#define	ERR_BAD_DESCRIPTOR		0x01
#define	ERR_BAD_COMMAND			0x02
#define	ERR_UNIMPLEMENTED_COMMAND	0x03
#define	ERR_BAD_DRIVE			0x04
#define	ERR_BAD_LOGICAL_ADDRESS		0x05
#define	ERR_BAD_SG_TABLE		0x06
#define	ERR_UNIMPLEMENTED_DEVICE	0x07
#define	ERR_UNIT_NOT_INITIALIZED	0x08
/* media errors */
#define	ERR_NO_ID			0x10
#define	ERR_SEEK			0x11
#define	ERR_RELOCATED_TRACK		0x12
#define	ERR_BAD_ID			0x13
#define	ERR_DATA_SYNC_FAULT		0x14
#define	ERR_ECC				0x15
#define	ERR_RECORD_NOT_FOUND		0x16
#define	ERR_MEDIA			0x17
/* drive errors */
#define	ERR_DRIVE_FAULT			0x20
#define	ERR_WRITE_PROTECTED_MEDIA	0x21
#define	ERR_MOTOR_OFF			0x22
#define	ERR_DOOR_OPEN			0x23
#define	ERR_DRIVE_NOT_READY		0x24
#define	ERR_DRIVE_BUSY			0x25
/* VME DMA errors */
#define	ERR_BUS				0x30
#define	ERR_ALIGNMENT			0x31
#define	ERR_BUS_TIMEOUT			0x32
#define	ERR_INVALID_XFER_COUNT		0x33
/* disk format error */
#define	ERR_NOT_ENOUGH_ALTERNATES	0x40
#define	ERR_FORMAT_FAILED		0x41
#define	ERR_VERIFY			0x42
#define	ERR_BAD_FORMAT_PARAMETERS	0x43
#define	ERR_CANNOT_FIX_BAD_SPOT		0x44
#define	ERR_TOO_MANY_DEFECTS		0x45
/* MVME327A specific errors */
#define	ERR_SCSI			0x80	/* additional status available*/
#define	ERR_INDETERMINATE_MEDIA		0x81	/* no additional status */
#define	ERR_INDETERMINATE_HARDWARE	0x82
#define	ERR_BLANK_CHECK			0x83
#define	ERR_INCOMPLETE_EXTENDED_MESSAGE	0x84
#define	ERR_INVALID_RESELECTION		0x85
#define	ERR_NO_STATUS_RETURNED		0x86
#define	ERR_MESSAGE_OUT_NOT_TRANSFERRED	0x87
#define	ERR_MESSAGE_IN_NOT_RECEIVED	0x88
#define	ERR_INCOMPLETE_DATA_READ	0x89
#define	ERR_INCOMPLETE_DATA_WRITE	0x8a
#define	ERR_INCORRECT_CDB_SIZE		0x8b
#define	ERR_UNDEFINED_SCSI_PHASE	0x8c
#define	ERR_SELECT_TIMEOUT		0x8d
#define	ERR_BUS_RESET			0x8e
#define	ERR_INVALID_MESSAGE_RECEIVED	0x8f
#define	ERR_COMMAND_NOT_RECEIVED	0x90
#define	ERR_UNEXPECTED_STATUS_PHASE	0x91
#define	ERR_SCSI_SCRIPT_MISMATCH	0x92
#define	ERR_UNEXPECTED_DISCONNECT	0x93
#define	ERR_REQUEST_SENSE_FAILED	0x94
#define	ERR_NO_WRITE_DESCRIPTOR		0x95
#define	ERR_INCOMPLETE_DATA_TRANSFER	0x96
#define	ERR_OUT_OF_LOCAL_RESOURCES	0x97
#define	ERR_LOCAL_MEMORY_RESOURCES_LOST	0x98
#define	ERR_CHANNEL_RESERVED		0x99
#define	ERR_DEVICE_RESERVED		0x9a
#define	ERR_ALREADY_ENABLED		0x9b
#define	ERR_TARGET_NOT_ENABLED		0x9c
#define	ERR_UNSUPPORTED_CONTROLLER_TYPE	0x9d
#define	ERR_UNSUPPORTED_DEVICE_TYPE	0x9e
#define	ERR_BLOCK_SIZE_MISMATCH		0x9f
#define	ERR_INVALID_CYL_IN_DEFECT_LIST	0xa0
#define	ERR_INVALID_HEAD_IN_DEFECT_LIST	0xa1
#define	ERR_BLOCK_SIZE_MISMATCH_NF	0xa2	/* non fatal */
#define	ERR_SCSI_ID_UNCHANGED		0xa3
#define	ERR_SCSI_ID_CHANGED		0xa4
#define	ERR_NO_TARGET_ENABLE		0xa5
#define	ERR_CANNOT_DO_D32		0xa6
#define	ERR_CANNOT_DO_DMA		0xa7
#define	ERR_INVALID_BLOCK_SIZE		0xa8
#define	ERR_SPT_MISMATCH		0xa9
#define	ERR_HEAD_MISMATCH		0xaa
#define	ERR_CYL_MISMATCH		0xab
#define	ERR_INVALID_FLOPPY_PARAMETERS	0xac
#define	ERR_ALREADY_RESERVED		0xad
#define	ERR_WAS_NOT_RESERVED		0xae
#define	ERR_INVALID_SECTOR_NUMBER	0xaf
#define	ERR_SELFTEST_FAILED		0xcc

/*
 * SCSI specific command packet
 */

struct vsbic_scsi {
	uint32_t	link;
	uint16_t	ctrl;
#define	SCSI_CTRL_DMA		0x8000	/* allow DMA operation */
#define	SCSI_CTRL_SYNC		0x4000	/* synchronous phase */
#define	SCSI_CTRL_PAR		0x2000	/* enable parity checking */
#define	SCSI_CTRL_SCHK		0x1000	/* manual status checking */
#define	SCSI_CTRL_D32		0x0800	/* use D32 mode for data transfer */
#define	SCSI_CTRL_BYTESWAP	0x0400	/* byteswap D16 words */
#define	SCSI_CTRL_SG		0x0200	/* use scatter/gather transfer list */
#define	SCSI_CTRL_LINK		0x0100	/* linked scsi command */
#define	SCSI_CTRL_NO_ATN	0x0080	/* do not assert ATN during select */
	uint8_t		cmdlen;
	uint8_t		reserved;
	uint8_t		cdb[12];
	uint32_t	datalen;
	uint32_t	dataptr;
	uint8_t		status;
	uint8_t		initiator;
	uint8_t		msgin_flag;
	uint8_t		msgout_flag;
#define	SCSI_MSG_INTERNAL	0x00	/* bytes in structure */
#define	SCSI_MSG_EXTERNAL	0xff	/* bytes pointed to by ptr */
	uint16_t	msgin_len;
	uint16_t	msgin_ptr_high;		/* not aligned... */
	uint16_t	msgin_ptr_low;
	uint8_t		msgin[6];
	uint16_t	msgout_len;
	uint16_t	msgout_ptr_high;	/* not aligned... */
	uint16_t	msgout_ptr_low;
	uint8_t		msgout[6];
	uint8_t		script[8];		/* script phases */
#define	SCRIPT_DATA_OUT			0x00
#define	SCRIPT_DATA_IN			0x01
#define	SCRIPT_COMMAND			0x02
#define	SCRIPT_STATUS			0x03
#define	SCRIPT_MSG_OUT			0x06
#define	SCRIPT_MSG_IN			0x07
#define	SCRIPT_END			0x08
#define	SCRIPT_END_LINKED		0x09
	/* total size 0x40 bytes */
};

/*
 * Scatter/gather list element
 */

struct vsbic_sg {
	uint32_t	addr;
	uint32_t	size;
};

#define	VSBIC_MAXSG		(MAXPHYS / PAGE_SIZE)

/*
 * Complete SCSI command structure: packet, SCSI specific structure, S/G list.
 * Nothing needs them to be contiguous but it will make our life simpler
 * this way.
 */
struct vsbic_cmd {
	struct vsbic_pkt	pkt;
	struct vsbic_scsi	scsi;
	struct vsbic_sg		sg[VSBIC_MAXSG];
};

/*
 * The host firmware addresses the VME bus with A23 set to 1, and uses
 * an extension address register to provide the top 9 bits of the A32
 * address.  We thus need to make sure that none of the structures we
 * share with the firmware cross a 8MB boundary.
 */

#define	VSBIC_MEM_BOUNDARY	(1 << 23)

/*
 * A note on resource usage:
 *
 * - this driver uses 9 channels; one per SCSI target, one for polled
 *   commands (which does not cause interrupts); and a 9th one to issue
 *   reset commands when necessary, with a higher priority than the other
 *   channels.
 *
 * - for each target channel, there numopenings commands (packet, scsi
 *   specific packet and s/g list) allocated.
 * - for the polling channel, there is only one command.
 * - for the reset channel, there is one command (packet) per target.
 *
 * - for each channel, there are as many envelopes as possible commands
 *   on the channel, plus two (one NULL envelope per pipe) per channel.
 */

#define	VSBIC_NUMOPENINGS	2
#define	VSBIC_NTARGETS		7
#define	VSBIC_NCHANNELS		(VSBIC_NTARGETS + 2)

#define	VSBIC_TARGET_CHANNEL(sc,tgt)	(tgt)		/* 0..7 */
#define	VSBIC_RESET_CHANNEL(sc)		(sc->sc_id)	/* within 0..7 */
#define	VSBIC_POLLING_CHANNEL(sc)	(8)

#define	VSBIC_NPACKETS \
	((VSBIC_NTARGETS * VSBIC_NUMOPENINGS) + VSBIC_NTARGETS + 1)
#define	VSBIC_NENVELOPES	(VSBIC_NPACKETS + 2 * VSBIC_NCHANNELS)

#define	VSBIC_NCCBS		(VSBIC_NTARGETS * VSBIC_NUMOPENINGS)

/*
 * Per command information
 */
struct vsbic_ccb {
	struct vsbic_ccb	*ccb_next;
	struct vsbic_cmd	*ccb_cmd;	/* associated command */

	struct scsi_xfer	*ccb_xs;	/* associated request */

	int			 ccb_xsflags;	/* copy of ccb_xs->flags */
	int			 ccb_flags;
#define	CCBF_SENSE			0x01	/* request sense sent */

	bus_dmamap_t		 ccb_dmamap;	/* DMA map for data transfer */
	bus_size_t		 ccb_dmalen;
};

struct vsbic_softc {
	struct bpp_softc	 sc_bpp;
	bus_dma_tag_t		 sc_dmat;

	struct scsi_link	 sc_link;
	uint			 sc_id;		/* host adapter ID */

	int			 sc_vec;	/* interrupt vector */
	int			 sc_ipl;	/* interrupt level */
	struct intrhand		 sc_ih;

	bus_dmamap_t		 sc_chanmap;	/* channel pool */
	bus_dma_segment_t	 sc_chanseg;
#define	sc_chanpa	sc_chanseg.ds_addr
	vaddr_t			 sc_chanva;

	bus_dmamap_t		 sc_envmap;	/* envelope pool */
	bus_dma_segment_t	 sc_envseg;
#define	sc_envpa	sc_envseg.ds_addr
	vaddr_t			 sc_envva;

	bus_dmamap_t		 sc_cmdmap;	/* packet pool */
	bus_dma_segment_t	 sc_cmdseg;
#define	sc_cmdpa	sc_cmdseg.ds_addr
	vaddr_t			 sc_cmdva;

	struct vsbic_cmd	*sc_cmd_free;	/* head of free cmd list */
	struct vsbic_ccb	*sc_ccb_free;	/* head of free ccb list */
	struct vsbic_ccb	*sc_ccb_active;	/* head of active ccb list */

	/* bpp channel array */
	struct bpp_chan		 sc_chan[VSBIC_NCHANNELS];
};

#define	DEVNAME(sc)	((sc)->sc_bpp.sc_dev.dv_xname)

void	vsbic_activate_ccb(struct vsbic_softc *, struct vsbic_ccb *,
	    struct vsbic_cmd *);
int	vsbic_alloc_physical(struct vsbic_softc *, bus_dmamap_t *,
	    bus_dma_segment_t *, vaddr_t *, bus_size_t, const char *);
void	vsbic_attach(struct device *, struct device *, void *);
int	vsbic_channel_intr(struct vsbic_softc *, struct bpp_chan *);
paddr_t	vsbic_chan_pa(struct bpp_softc *, struct bpp_channel *);
void	vsbic_chan_sync(struct bpp_softc *, struct bpp_channel *, int);
struct vsbic_ccb *
	vsbic_cmd_ccb(struct vsbic_softc *, struct vsbic_cmd *);
int	vsbic_create_ccbs(struct vsbic_softc *, uint);
int	vsbic_create_channels(struct vsbic_softc *, uint);
int	vsbic_create_cmds(struct vsbic_softc *, uint);
struct vsbic_cmd *
	vsbic_dequeue_cmd(struct vsbic_softc *, struct bpp_chan *);
paddr_t	vsbic_env_pa(struct bpp_softc *, struct bpp_envelope *);
void	vsbic_env_sync(struct bpp_softc *, struct bpp_envelope *, int);
struct bpp_envelope *
	vsbic_env_va(struct bpp_softc *, paddr_t);
void	vsbic_free_ccb(struct vsbic_softc *, struct vsbic_ccb *);
struct vsbic_ccb *
	vsbic_get_ccb(struct vsbic_softc *);
struct vsbic_cmd *
	vsbic_get_cmd(struct vsbic_softc *);
int	vsbic_intr(void *);
int	vsbic_load_command(struct vsbic_softc *, struct vsbic_ccb *,
	    struct vsbic_cmd *, struct scsi_link *, struct scsi_generic *, int,
	    uint8_t *, int);
int	vsbic_match(struct device *, void *, void *);
void	vsbic_poll(struct vsbic_softc *, struct vsbic_ccb *);
void	vsbic_put_cmd(struct vsbic_softc *, struct vsbic_cmd *);
void	vsbic_queue_cmd(struct vsbic_softc *, struct bpp_chan *,
	    struct bpp_envelope *, struct vsbic_cmd *);
int	vsbic_request_sense(struct vsbic_softc *, struct vsbic_ccb *);
void	vsbic_reset_command(struct vsbic_softc *, struct vsbic_cmd *,
	    struct scsi_link *);
int	vsbic_scsicmd(struct scsi_xfer *);
int	vsbic_scsireset(struct vsbic_softc *, struct scsi_xfer *);
void	vsbic_timeout(void *);
void	vsbic_wrapup(struct vsbic_softc *, struct vsbic_ccb *);

const struct cfattach vsbic_ca = {
	sizeof(struct vsbic_softc), vsbic_match, vsbic_attach
};

struct cfdriver vsbic_cd = {
	NULL, "vsbic", DV_DULL
};

struct scsi_adapter vsbic_swtch = {
	vsbic_scsicmd,
	scsi_minphys
};

struct scsi_device vsbic_scsidev = {
	NULL,
	NULL,
	NULL,
	NULL
};

#define	MVME327_CSR_ID		0xff
#define	MVME327_CSR_SIZE	0x100

int
vsbic_match(struct device *device, void *cf, void *args)
{
	struct confargs *ca = args;
	bus_space_tag_t iot = ca->ca_iot;
	bus_space_handle_t ioh;
	int rc;
	uint8_t id;

	if (bus_space_map(iot, ca->ca_paddr, MVME327_CSR_SIZE, 0, &ioh) != 0)
		return 0;

	rc = badaddr((vaddr_t)bus_space_vaddr(iot, ioh) + MVME327_CSR_ID, 1);
	if (rc == 0) {
		/* Check the SCSI ID is sane */
		id = bus_space_read_1(iot, ioh, MVME327_CSR_ID);
		if (id & ~0x07)
			rc = 1;
	}
	bus_space_unmap(iot, ioh, MVME327_CSR_SIZE);

	return (rc == 0);
}

void
vsbic_attach(struct device *parent, struct device *self, void *args)
{
	struct confargs *ca = args;
	struct vsbic_softc *sc = (struct vsbic_softc *)self;
	struct bpp_softc *bsc = &sc->sc_bpp;
	struct scsibus_attach_args saa;
	bus_space_handle_t ioh;
	int tmp;

	if (ca->ca_ipl < IPL_BIO)
		ca->ca_ipl = IPL_BIO;

	printf("\n");

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, MVME327_CSR_SIZE,
	    BUS_SPACE_MAP_LINEAR, &ioh) != 0) {
		printf("%s: can't map registers!\n", DEVNAME(sc));
		return;
	}

	bpp_attach(bsc, ca->ca_iot, ioh);
	sc->sc_ipl = ca->ca_ipl;
	sc->sc_vec = ca->ca_vec;
	sc->sc_dmat = ca->ca_dmat;

	/*
	 * Setup envelope and channel memory utility functions.
	 *
	 * Since the MVME327 shares no memory with the host cpu, all the
	 * resources will be allocated in physical memory, and will need
	 * explicit cache operations.
	 */
	bsc->bpp_chan_pa = vsbic_chan_pa;
	bsc->bpp_chan_sync = vsbic_chan_sync;
	bsc->bpp_env_pa = vsbic_env_pa;
	bsc->bpp_env_va = vsbic_env_va;
	bsc->bpp_env_sync = vsbic_env_sync;

	sc->sc_id = bus_space_read_1(bsc->sc_iot, bsc->sc_ioh, MVME327_CSR_ID);

	if (bpp_reset(bsc) != 0) {
		printf("%s: reset failed\n", DEVNAME(sc));
		return;
	}

	sc->sc_ih.ih_fn = vsbic_intr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_wantframe = 0;
	sc->sc_ih.ih_ipl = ca->ca_ipl;

	/*
	 * Allocate memory for the channel headers, envelopes and packets.
	 */

	if (vsbic_alloc_physical(sc, &sc->sc_envmap, &sc->sc_envseg,
	    &sc->sc_envva, VSBIC_NENVELOPES * sizeof(struct bpp_envelope),
	    "envelope") != 0)
		return;

	bpp_initialize_envelopes(bsc, (struct bpp_envelope *)sc->sc_envva,
	    VSBIC_NENVELOPES);

	if (vsbic_create_cmds(sc, VSBIC_NPACKETS) != STATUS_OK) {
		/* XXX free resources */
		return;
	}

	if (vsbic_create_ccbs(sc, VSBIC_NCCBS) != 0) {
		/* XXX free resources */
		return;
	}

	if (vsbic_create_channels(sc, VSBIC_NCHANNELS) != STATUS_OK) {
		/* XXX free resources */
		return;
	}

	vmeintr_establish(sc->sc_vec, &sc->sc_ih, DEVNAME(sc));

	sc->sc_link.adapter = &vsbic_swtch;
	sc->sc_link.adapter_buswidth = 8;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_id;
	sc->sc_link.device = &vsbic_scsidev;
	sc->sc_link.openings = VSBIC_NUMOPENINGS;

	bzero(&saa, sizeof saa);
	saa.saa_sc_link = &sc->sc_link;

	tmp = bootpart;
	if (ca->ca_paddr != bootaddr)
		bootpart = -1;
	config_found(self, &saa, scsiprint);
	bootpart = tmp;
}

/*
 * Various simple routines to get the addresses of the various structures
 * shared with the MVME327, as well as to copyback caches.
 */

paddr_t
vsbic_chan_pa(struct bpp_softc *bsc, struct bpp_channel *va)
{
	struct vsbic_softc *sc = (struct vsbic_softc *)bsc;
	paddr_t pa = sc->sc_chanpa + ((vaddr_t)va - sc->sc_chanva);

#ifdef VSBIC_DEBUG
	printf("chan: va %p -> pa %p\n", va, pa);
#endif

	return pa;
}

void
vsbic_chan_sync(struct bpp_softc *bsc, struct bpp_channel *va, int fl)
{
	struct vsbic_softc *sc = (struct vsbic_softc *)bsc;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_chanmap,
	    (vaddr_t)va - sc->sc_chanva, sizeof(struct bpp_channel), fl);
}

paddr_t
vsbic_env_pa(struct bpp_softc *bsc, struct bpp_envelope *va)
{
	struct vsbic_softc *sc = (struct vsbic_softc *)bsc;
	paddr_t pa = sc->sc_envpa + ((vaddr_t)va - sc->sc_envva);

#ifdef VSBIC_DEBUG
	printf("env: va %p -> pa %p\n", va, pa);
#endif

	return pa;
}

struct bpp_envelope *
vsbic_env_va(struct bpp_softc *bsc, paddr_t pa)
{
	struct vsbic_softc *sc = (struct vsbic_softc *)bsc;
	vaddr_t va = sc->sc_envva + (pa - sc->sc_envpa);

#ifdef VSBIC_DEBUG
	printf("env: pa %p -> va %p\n", pa, va);
#endif

	return (struct bpp_envelope *)va;
}

void
vsbic_env_sync(struct bpp_softc *bsc, struct bpp_envelope *va, int fl)
{
	struct vsbic_softc *sc = (struct vsbic_softc *)bsc;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_envmap,
	    (vaddr_t)va - sc->sc_envva, sizeof(struct bpp_envelope), fl);
}

static inline
paddr_t	vsbic_cmd_pa(struct vsbic_softc *sc, struct vsbic_cmd *va)
{
	return sc->sc_cmdpa + ((vaddr_t)va - sc->sc_cmdva);
}

static inline
struct vsbic_cmd *vsbic_cmd_va(struct vsbic_softc *sc, paddr_t pa)
{
	return (struct vsbic_cmd *)(sc->sc_cmdva + (pa - sc->sc_cmdpa));
}

static inline
void vsbic_cmd_sync(struct vsbic_softc *sc, struct vsbic_cmd *va, int fl)
{
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cmdmap,
	    (vaddr_t)va - sc->sc_cmdva, sizeof(struct vsbic_cmd), fl);
}

/*
 * Allocate contiguous physical memory, not crossing 8 MB boundaries.
 */
int
vsbic_alloc_physical(struct vsbic_softc *sc, bus_dmamap_t *dmamap,
    bus_dma_segment_t *dmaseg, vaddr_t *va, bus_size_t len, const char *what)
{
	int nseg;
	int rc;

	len = round_page(len);

	rc = bus_dmamem_alloc(sc->sc_dmat, len, 0, 0, dmaseg, 1, &nseg,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to allocate %s memory: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail1;
	}

	rc = bus_dmamem_map(sc->sc_dmat, dmaseg, nseg, len,
	    (caddr_t *)va, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (rc != 0) {
		printf("%s: unable to map %s memory: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail2;
	}

	rc = bus_dmamap_create(sc->sc_dmat, len, 1, len, VSBIC_MEM_BOUNDARY,
	    BUS_DMA_NOWAIT /* | BUS_DMA_ALLOCNOW */, dmamap);
	if (rc != 0) {
		printf("%s: unable to create %s dma map: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail3;
	}

	rc = bus_dmamap_load(sc->sc_dmat, *dmamap, (void *)*va, len, NULL,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to load %s dma map: error %d\n",
		    DEVNAME(sc), what, rc);
		goto fail4;
	}

	return 0;

fail4:
	bus_dmamap_destroy(sc->sc_dmat, *dmamap);
fail3:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)*va, PAGE_SIZE);
fail2:
	bus_dmamem_free(sc->sc_dmat, dmaseg, 1);
fail1:
	return rc;
}

/*
 * CSR operations
 */

int
vsbic_create_channels(struct vsbic_softc *sc, uint cnt)
{
	struct bpp_softc *bsc = &sc->sc_bpp;
	struct bpp_chan *chan;
	struct bpp_channel *channel;
	int priority, ipl, vec;
	uint i;
	int rc;

	rc = vsbic_alloc_physical(sc, &sc->sc_chanmap, &sc->sc_chanseg,
	    &sc->sc_chanva, cnt * sizeof(struct bpp_channel), "channel");
	if (rc != 0)
		return STATUS_ERRNO + rc;

 	chan = sc->sc_chan;
	channel = (struct bpp_channel *)sc->sc_chanva;
	for (i = 0; i < cnt; i++, chan++, channel++) {
		chan->ch = channel;

		if (i == VSBIC_RESET_CHANNEL(sc))
			priority = BPP_PRIORITY_HIGHEST + 0x20;
		else
			priority = BPP_PRIORITY_HIGHEST + 0x40;
		if (i == VSBIC_POLLING_CHANNEL(sc)) {
			ipl = 0;
			vec = 0;
		} else {
			ipl = sc->sc_ipl;
			vec = sc->sc_vec;
		}
		rc = bpp_create_channel(bsc, chan, priority, ipl, vec);

		if (rc != STATUS_OK) {
			printf("%s: error creating channel for target %d: %x\n",
			    DEVNAME(sc), i, rc);
			return rc;
		}
	}

	return STATUS_OK;
}

/*
 * Command (packet) management
 */

/*
 * Create a set of commands, and put them on a free list.
 */
int
vsbic_create_cmds(struct vsbic_softc *sc, uint cnt)
{
	struct vsbic_cmd *cmd;
	int rc;

	rc = vsbic_alloc_physical(sc, &sc->sc_cmdmap, &sc->sc_cmdseg,
	    &sc->sc_cmdva, cnt * sizeof(struct vsbic_cmd), "packet");
	if (rc != 0)
		return STATUS_ERRNO + rc;

	sc->sc_cmd_free = NULL;
	cmd = (struct vsbic_cmd *)sc->sc_cmdva;
	while (cnt-- != 0)
		vsbic_put_cmd(sc, cmd++);

	return STATUS_OK;
}

/*
 * Get a new command from the free list.
 */
struct vsbic_cmd *
vsbic_get_cmd(struct vsbic_softc *sc)
{
	struct vsbic_cmd *cmd;

	cmd = sc->sc_cmd_free;
	if (cmd != NULL) {
		sc->sc_cmd_free = (struct vsbic_cmd *)cmd->pkt.cmd_link;
		memset(cmd, 0, sizeof(*cmd));
	}

	return cmd;
}

/*
 * Put a command into the free list.
 */
void
vsbic_put_cmd(struct vsbic_softc *sc, struct vsbic_cmd *cmd)
{
	cmd->pkt.cmd_link = (uint32_t)sc->sc_cmd_free;
	sc->sc_cmd_free = cmd;
}

/*
 * Dequeue an envelope from the status pipe, and return the command it
 * was carrying. The envelope itself is returned to the free list.
 */
struct vsbic_cmd *
vsbic_dequeue_cmd(struct vsbic_softc *sc, struct bpp_chan *chan)
{
	struct bpp_softc *bsc = &sc->sc_bpp;
	struct vsbic_cmd *cmd;
	paddr_t cmdpa;

	if (bpp_dequeue_envelope(bsc, chan, &cmdpa) != 0)
		return NULL;

	cmd = vsbic_cmd_va(sc, cmdpa);
	vsbic_cmd_sync(sc, cmd, BUS_DMASYNC_POSTREAD);

#ifdef VSBIC_DEBUG
	printf("%s: %s() -> %p\n", DEVNAME(sc), __func__, cmd);
#endif
	return cmd;
}

/*
 * Send a command packet.
 */
void
vsbic_queue_cmd(struct vsbic_softc *sc, struct bpp_chan *chan,
    struct bpp_envelope *tail, struct vsbic_cmd *cmd)
{
	struct bpp_softc *bsc = &sc->sc_bpp;
	paddr_t cmdpa;

	vsbic_cmd_sync(sc, cmd, BUS_DMASYNC_PREWRITE);
	cmdpa = vsbic_cmd_pa(sc, cmd);

#ifdef VSBIC_DEBUG
	printf("%s: %s(%p)\n", DEVNAME(sc), __func__, cmd);
#endif

	bpp_queue_envelope(bsc, chan, tail, cmdpa);
}

/*
 * CCB management
 */

/*
 * Create a set of ccb, and put them on a free list.
 */
int
vsbic_create_ccbs(struct vsbic_softc *sc, uint cnt)
{
	struct vsbic_ccb *ccb;
	int rc;

	ccb = (struct vsbic_ccb *)malloc(cnt * sizeof(struct vsbic_ccb),
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (ccb == NULL) {
		printf("%s: unable to allocate CCB memory\n", DEVNAME(sc));
		return ENOMEM;
	}

	sc->sc_ccb_free = NULL;
	sc->sc_ccb_active = NULL;
	while (cnt-- != 0) {
		/*
		 * Create a DMA map for data transfers.
		 */
		rc = bus_dmamap_create(sc->sc_dmat, MAXPHYS, VSBIC_MAXSG,
		    MAXPHYS, VSBIC_MEM_BOUNDARY,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->ccb_dmamap);
		if (rc != 0) {
			printf("%s: unable to create CCB data dma map"
			    ": error %d\n", DEVNAME(sc), rc);
			return rc;
		}

		vsbic_free_ccb(sc, ccb++);
	}

	return 0;
}

/*
 * Get a new ccb from the free list.
 */
struct vsbic_ccb *
vsbic_get_ccb(struct vsbic_softc *sc)
{
	struct vsbic_ccb *ccb;

	ccb = sc->sc_ccb_free;
	if (ccb != NULL) {
		sc->sc_ccb_free = ccb->ccb_next;
		ccb->ccb_next = NULL;
		ccb->ccb_cmd = NULL;
		ccb->ccb_xs = NULL;
		ccb->ccb_flags = 0;
	}

	return ccb;
}

/*
 * Put a ccb into the free list.
 */
void
vsbic_free_ccb(struct vsbic_softc *sc, struct vsbic_ccb *ccb)
{
	ccb->ccb_next = sc->sc_ccb_free;
	sc->sc_ccb_free = ccb;
}

/*
 * Retrieve the active ccb associated to a command, and remove it from
 * the active list.
 * Since there won't be many currently active commands, storing them
 * in a list is acceptable.
 */
struct vsbic_ccb *
vsbic_cmd_ccb(struct vsbic_softc *sc, struct vsbic_cmd *cmd)
{
	struct vsbic_ccb *ccb, *prev;

	for (prev = NULL, ccb = sc->sc_ccb_active; ccb != NULL;
	    prev = ccb, ccb = ccb->ccb_next) {
		if (ccb->ccb_cmd == cmd) {
			if (prev == NULL)
				sc->sc_ccb_active = ccb->ccb_next;
			else
				prev->ccb_next = ccb->ccb_next;
			break;
		}
	}

	return ccb;
}

/*
 * Put a ccb into the active list.
 */
void
vsbic_activate_ccb(struct vsbic_softc *sc, struct vsbic_ccb *ccb,
    struct vsbic_cmd *cmd)
{
	struct vsbic_ccb *tmp;

	ccb->ccb_cmd = cmd;

	/* insert at end of list */
	if (sc->sc_ccb_active == NULL)
		sc->sc_ccb_active = ccb;
	else {
		for (tmp = sc->sc_ccb_active; tmp->ccb_next != NULL;
		    tmp = tmp->ccb_next)
			;
		tmp->ccb_next = ccb;
	}
}

/*
 * SCSI Layer Interface
 */

/*
 * Setup a command packet according to the SCSI command to send
 */
int
vsbic_load_command(struct vsbic_softc *sc, struct vsbic_ccb *ccb,
    struct vsbic_cmd *c, struct scsi_link *sl, struct scsi_generic *cmd,
    int cmdlen, uint8_t *data, int datalen)
{
	bus_dma_segment_t *seg;
	struct vsbic_sg *sgelem;
	int nsegs, segno;
	uint ctrl;
	uint8_t *msgout;
	uint8_t *script;
	int rc;

#ifdef VSBIC_DEBUG
	printf("%s: command %02x len %d data %d tgt %d:%d\n",
	    DEVNAME(sc), cmd->opcode, cmdlen, datalen,
	    sl->target, sl->lun);
#endif

	/*
	 * Setup DMA map for data transfer.
	 */

	if (ISSET(ccb->ccb_xsflags, SCSI_DATA_IN | SCSI_DATA_OUT)) {
		ccb->ccb_dmalen = (bus_size_t)datalen;
		rc = bus_dmamap_load(sc->sc_dmat, ccb->ccb_dmamap, data,
		    ccb->ccb_dmalen, NULL, BUS_DMA_STREAMING |
		    (ISSET(ccb->ccb_xsflags,SCSI_NOSLEEP) ?
		      BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
		    (ISSET(ccb->ccb_xsflags, SCSI_DATA_IN) ?
		      BUS_DMA_READ : BUS_DMA_WRITE));
		if (rc != 0)
			return rc;

		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmalen, ISSET(ccb->ccb_xsflags, SCSI_DATA_IN) ?
		      BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

		nsegs = ccb->ccb_dmamap->dm_nsegs;
	} else
		nsegs = 0;

	/*
	 * Setup command packet.
	 */

	if (cmd->opcode == INQUIRY && nsegs == 1)
		c->pkt.cmd_cmd = CMD_INQUIRY;
	else
		c->pkt.cmd_cmd = CMD_SCSI;
	c->pkt.cmd_device = VSBIC_DEVICE_SCSI;
	c->pkt.cmd_unit = VSBIC_UNIT(sl->target, sl->lun);
	c->pkt.cmd_addrmod = ADRM_EXT_S_D;
	c->pkt.cmd_buswidth = MEMT_D32;

	if (c->pkt.cmd_cmd == CMD_INQUIRY) {
		c->pkt.cmd_cmd_ctrl = 0;
		seg = ccb->ccb_dmamap->dm_segs;
		c->pkt.cmd_addr2 = htobe32(seg->ds_addr);
		c->pkt.cmd_sg_count = 0;
		c->pkt.cmd_parm2 = htobe16(datalen >> 16);
		c->pkt.cmd_parm3 = htobe16(datalen & 0xffff);
	} else {
		c->pkt.cmd_addr1 = htobe32(vsbic_cmd_pa(sc, c) +
		    offsetof(struct vsbic_cmd, scsi));

		/*
		 * Setup SCSI specific packet.
		 */

		ctrl = SCSI_CTRL_PAR | SCSI_CTRL_SCHK;
		if (nsegs != 0) {
			ctrl |= SCSI_CTRL_D32;
			/* manual recommands PIO for small transfers */
			if (datalen > 0x100)
				ctrl |= SCSI_CTRL_DMA;
		}
		if (cmd->opcode == REQUEST_SENSE)
			ctrl |= SCSI_CTRL_NO_ATN;
		c->scsi.cmdlen = cmdlen;
		memcpy(c->scsi.cdb, cmd, cmdlen);
		c->scsi.datalen = htobe32(datalen);
		c->scsi.msgin_flag = SCSI_MSG_INTERNAL;
		c->scsi.msgout_flag = SCSI_MSG_INTERNAL;

		/*
		 * Setup scatter/gather information.
		 */

		if (nsegs != 0) {
			seg = ccb->ccb_dmamap->dm_segs;
			if (nsegs == 1) {
				c->scsi.dataptr = htobe32(seg->ds_addr);
			} else {
				sgelem = c->sg;
				ctrl |= SCSI_CTRL_SG;
				for (segno = 0; segno < nsegs; seg++, segno++) {
					sgelem->addr = htobe32(seg->ds_addr);
					sgelem->size = htobe32(seg->ds_len);
					sgelem++;
				}
				c->pkt.cmd_sg_count = htobe16(nsegs);
				c->scsi.dataptr = htobe32(vsbic_cmd_pa(sc, c) +
				    offsetof(struct vsbic_cmd, sg));
			}
			c->pkt.cmd_addr2 = c->scsi.dataptr;
		}

		c->scsi.ctrl = ctrl;

		/*
		 * Setup SCSI ``script'' - really the list of phases to expect,
		 * as well as the message bytes to send during the message out
		 * phase if necessary.
		 *
		 * Since this driver doesn't support synchronous transfers yet,
		 * this is really easy to build.
		 */

		msgout = c->scsi.msgout;
		script = c->scsi.script;

		/* always message out unless the command is request sense */
		if (cmd->opcode != REQUEST_SENSE) {
			*script++ = SCRIPT_MSG_OUT;
			*msgout++ = MSG_IDENTIFY(sl->lun, 1);
		}
		*script++ = SCRIPT_COMMAND;
		if (ISSET(ccb->ccb_xsflags, SCSI_DATA_IN))
			*script++ = SCRIPT_DATA_IN;
		else if (ISSET(ccb->ccb_xsflags, SCSI_DATA_OUT))
			*script++ = SCRIPT_DATA_OUT;
		*script++ = SCRIPT_STATUS;
		*script++ = SCRIPT_MSG_IN;
		*script++ = SCRIPT_END;

		c->scsi.msgout_len = msgout - c->scsi.msgout;
	}

	return 0;
}

/*
 * Setup a reset command
 */
void
vsbic_reset_command(struct vsbic_softc *sc, struct vsbic_cmd *cmd,
    struct scsi_link *sl)
{
	cmd->pkt.cmd_cmd = CMD_RESET;
	cmd->pkt.cmd_device = VSBIC_DEVICE_SCSI;
	if (sl != NULL) {
		/* the lun is ignored for reset commands */
		cmd->pkt.cmd_unit = VSBIC_UNIT(sl->target, 0 /* sl->lun */);
	} else {
		/* reset whole bus */
		cmd->pkt.cmd_unit = VSBIC_UNIT(8, 0);
	}
}

/*
 * Try and send a command to the board.
 */
int
vsbic_scsicmd(struct scsi_xfer *xs)
{
	struct scsi_link *sl = xs->sc_link;
	struct vsbic_softc *sc = (struct vsbic_softc *)sl->adapter_softc;
	struct bpp_softc *bsc = &sc->sc_bpp;
	struct vsbic_ccb *ccb;
	struct bpp_envelope *env;
	struct vsbic_cmd *cmd;
	uint ch;
	int rc;
	int s;

	s = splbio();

#ifdef DIAGNOSTIC
	/*
	 * We shouldn't receive commands on the host adapter ID, this
	 * channel is reserved for reset commands.
	 */
	if (sl->target == sc->sc_id) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		splx(s);
		return COMPLETE;
	}
#endif

	/*
	 * CDB larger than 12 bytes are not supported, as well as
	 * odd-sized data transfers.
	 * Sense data borrowed from gdt(4).
	 */
	if (xs->cmdlen > 12 ||
	    (ISSET(xs->flags, SCSI_DATA_IN | SCSI_DATA_OUT) &&
	     (xs->datalen & 1) != 0)) {
#ifdef VSBIC_DEBUG
		printf("%s: can't issue command, cdb len %d data len %x\n",
		    DEVNAME(sc), xs->cmdlen, xs->datalen);
#endif
		memset(&xs->sense, 0, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | SSD_ERRCODE_CURRENT;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		/* 0x20 illcmd, 0x24 illfield */
		xs->sense.add_sense_code = xs->cmdlen > 12 ? 0x20 : 0x24;
		xs->error = XS_SENSE;
		scsi_done(xs);
		splx(s);
		return COMPLETE;
	}

	/*
	 * Get a CCB, an envelope and a command packet.
	 */

	ccb = vsbic_get_ccb(sc);
	if (ccb == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free CCB\n", DEVNAME(sc));
#endif
		splx(s);
		return NO_CCB;
	}

	env = bpp_get_envelope(bsc);
	if (env == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free envelope\n", DEVNAME(sc));
#endif
		vsbic_free_ccb(sc, ccb);
		splx(s);
		return NO_CCB;
	}
	cmd = vsbic_get_cmd(sc);
	if (cmd == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free command\n", DEVNAME(sc));
#endif
		bpp_put_envelope(bsc, env);
		vsbic_free_ccb(sc, ccb);
		splx(s);
		return NO_CCB;
	}

	ccb->ccb_xs = xs;
	ccb->ccb_xsflags = xs->flags;
	timeout_set(&xs->stimeout, vsbic_timeout, ccb);

	/*
	 * Build command script.
	 */

	rc = vsbic_load_command(sc, ccb, cmd, sl, xs->cmd, xs->cmdlen,
	    xs->data, xs->datalen);
	if (rc != 0) {
		printf("%s: unable to load DMA map: error %d\n",
		    DEVNAME(sc), rc);
		vsbic_put_cmd(sc, cmd);
		bpp_put_envelope(bsc, env);
		vsbic_free_ccb(sc, ccb);
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}

	/*
	 * Send the command to the hardware.
	 */

	vsbic_activate_ccb(sc, ccb, cmd);
	if (ISSET(xs->flags, SCSI_POLL))
		ch = VSBIC_POLLING_CHANNEL(sc);
	else
		ch = VSBIC_TARGET_CHANNEL(sc, sl->target);
	vsbic_queue_cmd(sc, &sc->sc_chan[ch], env, cmd);

	if (ISSET(xs->flags, SCSI_POLL)) {
		splx(s);
		vsbic_poll(sc, ccb);
		return (COMPLETE);
	} else {
		timeout_add_msec(&xs->stimeout, xs->timeout);
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}
}

/*
 * Send a request sense command. Invoked at splbio().
 */
int
vsbic_request_sense(struct vsbic_softc *sc, struct vsbic_ccb *ccb)
{
	struct bpp_softc *bsc = &sc->sc_bpp;
	struct scsi_xfer *xs = ccb->ccb_xs;
	struct scsi_link *sl = xs->sc_link;
	struct bpp_envelope *env;
	struct vsbic_cmd *cmd;
	struct scsi_sense ss;
	int rc;

	env = bpp_get_envelope(bsc);
	if (env == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free envelope\n", DEVNAME(sc));
#endif
		return EAGAIN;
	}
	cmd = vsbic_get_cmd(sc);
	if (cmd == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free command\n", DEVNAME(sc));
#endif
		bpp_put_envelope(bsc, env);
		return EAGAIN;
	}

	memset(&ss, 0, sizeof ss);
	ss.opcode = REQUEST_SENSE;
	ss.byte2 = sl->lun << 5;
	ss.length = sizeof(xs->sense);

	ccb->ccb_xsflags = (ccb->ccb_xsflags & SCSI_NOSLEEP) |
	    SCSI_DATA_IN | SCSI_POLL;
	rc = vsbic_load_command(sc, ccb, cmd, sl,
	    (struct scsi_generic *)&ss, sizeof ss,
	    (uint8_t *)&xs->sense, sizeof(xs->sense));
	if (rc != 0) {
		vsbic_put_cmd(sc, cmd);
		bpp_put_envelope(bsc, env);
		return rc;
	}

	vsbic_activate_ccb(sc, ccb, cmd);
	vsbic_queue_cmd(sc, &sc->sc_chan[VSBIC_POLLING_CHANNEL(sc)], env, cmd);
	if (xs->timeout > 1000)
		xs->timeout = 1000;
	vsbic_poll(sc, ccb);

	return 0;
}

/*
 * Reset a target. Invoked at splbio().
 */
int
vsbic_scsireset(struct vsbic_softc *sc, struct scsi_xfer *xs)
{
	struct bpp_softc *bsc = &sc->sc_bpp;
	struct bpp_chan *chan;
	struct scsi_link *sl;
	struct bpp_envelope *env;
	struct vsbic_cmd *cmd;

	/*
	 * Get an envelope and a command packet.
	 */

	env = bpp_get_envelope(bsc);
	if (env == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free envelope\n", DEVNAME(sc));
#endif
		return EAGAIN;
	}
	cmd = vsbic_get_cmd(sc);
	if (cmd == NULL) {
#ifdef VSBIC_DEBUG
		printf("%s: no free command\n", DEVNAME(sc));
#endif
		bpp_put_envelope(bsc, env);
		return EAGAIN;
	}

	if (xs == NULL)
		sl = NULL;
	else
		sl = xs->sc_link;

	vsbic_reset_command(sc, cmd, sl);

	chan = &sc->sc_chan[VSBIC_RESET_CHANNEL(sc)];
	vsbic_queue_cmd(sc, chan, env, cmd);

	return 0;
}

/*
 * Wrapup task after a command has completed (or failed).
 */
void
vsbic_wrapup(struct vsbic_softc *sc, struct vsbic_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ccb_xs;

	timeout_del(&xs->stimeout);

	if (xs->error == XS_NOERROR) {
		switch (xs->status) {
		case SCSI_OK:
			xs->error = XS_NOERROR;
			break;
		case SCSI_CHECK:
			/*
			 * Send a request sense command. If we can't, or
			 * it fails, don't insist and fail the command.
			 */
			if (!ISSET(ccb->ccb_flags, CCBF_SENSE)) {
				SET(ccb->ccb_flags, CCBF_SENSE);
				xs->error = XS_SENSE;
				if (vsbic_request_sense(sc, ccb) == 0)
					return;
			}
			xs->error = XS_DRIVER_STUFFUP;
			break;
		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
	}

	vsbic_free_ccb(sc, ccb);
	scsi_done(xs);
}


/*
 * Polled command operation.
 */

void
vsbic_poll(struct vsbic_softc *sc, struct vsbic_ccb *ccb)
{
	struct bpp_chan *chan = &sc->sc_chan[VSBIC_POLLING_CHANNEL(sc)];
	struct scsi_xfer *xs = ccb->ccb_xs;
	int s;
	int tmo;

	tmo = ccb->ccb_xs->timeout;
	s = splbio();
	for (;;) {
		if (vsbic_channel_intr(sc, chan) != 0)
			if (ISSET(xs->flags, ITSDONE))
				break;

		/*
		 * It is safe to lower spl while waiting, since the polling
		 * channel never interrupts, and thus we do not risk
		 * vsbic_intr() processing this request behind our back.
		 */
		splx(s);
		delay(1000);
		tmo--;
		s = splbio();
		if (tmo == 0)
			break;
	}

	if (tmo == 0) {
		vsbic_timeout(ccb);
		if (!ISSET(xs->flags, ITSDONE)) {
			/*
			 * We have sent a reset command. Poll for its
			 * completion.
			 */
			chan = &sc->sc_chan[VSBIC_RESET_CHANNEL(sc)];
			for (;;) {
				if (vsbic_channel_intr(sc, chan) != 0)
					if (ISSET(xs->flags, ITSDONE))
						break;

				delay(10000);	/* 10ms */
			}
		}
	}

	/*
	 * vsbic_intr() does not invoke vsbic_wrapup for polled
	 * commands, since we need the scsi_xfer to remain valid
	 * after the commands completes.
	 */
	vsbic_wrapup(sc, ccb);

	splx(s);
}

void
vsbic_timeout(void *v)
{
	struct vsbic_ccb *ccb = (struct vsbic_ccb *)v;
	struct scsi_xfer *xs = ccb->ccb_xs;
	struct scsi_link *sl = xs->sc_link;
	struct vsbic_softc *sc = (struct vsbic_softc *)sl->adapter_softc;
	int s;

	sc_print_addr(sl);
	printf("SCSI command 0x%x timeout\n", xs->cmd->opcode);

	s = splbio();

	if (vsbic_scsireset(sc, xs) != 0) {
		sc_print_addr(sl);
		printf("unable to reset SCSI bus\n");

		xs->error = XS_TIMEOUT;
		xs->status = SCSI_TERMINATED;
		if (ISSET(ccb->ccb_xsflags, SCSI_POLL)) {
			SET(xs->flags, ITSDONE);
			/* caller will invoke vsbic_wrapup() later */
		} else
			vsbic_wrapup(sc, ccb);

		/*
		 * This is very likely to hose at least this device,
		 * until another SCSI bus reset is tried, since the
		 * command will never complete...
		 *
		 * We could remember this situation and issue a bus
		 * reset as soon as an envelope is available, but
		 * the initial envelope number computation is supposed
		 * to prevent this from occuring.
		 */
	}

	/*
	 * If we have been able to send the reset command, make
	 * sure we are forcing an error condition, to report the
	 * correct error.
	 */
	xs->error = XS_TIMEOUT;
	xs->status = SCSI_TERMINATED;

	splx(s);
}

/*
 * Interrupt Handler
 */

int
vsbic_intr(void *arg)
{
	struct vsbic_softc *sc = arg;
	struct bpp_chan *chan;
	uint ch;

	splassert(IPL_BIO);

	/*
	 * There is no easy way to know which channel caused the interrupt
	 * (unless we register as many different interrupts as channels...),
	 * so check all of them.
	 *
	 * This means that we will sometimes dequeue commands before
	 * receiving their channel interrupts... therefore, we always
	 * claim the interrupt for the kernel not to be confused.
	 */

	/*
	 * Check the reset channel first, then check all real target channels.
	 */

	chan = &sc->sc_chan[VSBIC_RESET_CHANNEL(sc)];
	(void)vsbic_channel_intr(sc, chan);

	for (ch = 0; ch < 8; ch++) {
		if (ch == VSBIC_RESET_CHANNEL(sc))
			continue;
		chan = &sc->sc_chan[ch];
		(void)vsbic_channel_intr(sc, chan);
	}

	return 1;
}

int
vsbic_channel_intr(struct vsbic_softc *sc, struct bpp_chan *chan)
{
	struct vsbic_cmd *cmd;
	struct vsbic_ccb *ccb;
	struct scsi_xfer *xs;
	uint32_t xferlen;
	uint error, status;

	cmd = vsbic_dequeue_cmd(sc, chan);
	if (cmd == NULL)
		return 0;

	/* retrieve associated ccb, if any */
	ccb = vsbic_cmd_ccb(sc, cmd);

	xferlen = (betoh16(cmd->pkt.sts_xfer_high) << 16) |
	    betoh16(cmd->pkt.sts_xfer_low);
	error = cmd->pkt.sts_error;
	status = betoh16(cmd->pkt.sts_status);

#ifdef VSBIC_DEBUG
	printf("channel %d: cmd %p ccb %p xfer %x error %02x\n",
	    chan->ch->ch_num, cmd, ccb, xferlen, error);
	printf("  status %04x sts %02x parm3 %x",
	    status, cmd->scsi.status, cmd->pkt.sts_parm3);
	if (error == ERR_OK && cmd->pkt.sts_recovered != ERR_OK)
		printf(" recovered error %02x tries %d",
		    cmd->pkt.sts_recovered, cmd->pkt.sts_retries);
	printf("\n");
#endif

	if (ccb != NULL) {
		xs = ccb->ccb_xs;

		if (ISSET(ccb->ccb_xsflags, SCSI_DATA_IN | SCSI_DATA_OUT)) {
			if (!ISSET(ccb->ccb_flags, CCBF_SENSE))
				xs->resid = xs->datalen - xferlen;
			bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
			    ccb->ccb_dmalen,
			    ISSET(ccb->ccb_xsflags, SCSI_DATA_IN) ?
			      BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
		}

		if (xs->error == XS_NOERROR) {
			switch (error) {
			case ERR_SELECT_TIMEOUT:
				xs->error = XS_SELTIMEOUT;
				break;
			case ERR_BUS_RESET:
				xs->error = XS_RESET;
				break;
			case ERR_REQUEST_SENSE_FAILED:
				xs->error = XS_SHORTSENSE;
				break;
			case ERR_INVALID_XFER_COUNT:
				if (status == ERR_INCOMPLETE_DATA_TRANSFER)
					xs->error = XS_NOERROR;
				else
					xs->error = XS_DRIVER_STUFFUP;
				break;
			case ERR_OK:
				xs->error = XS_NOERROR;
				xs->status = cmd->scsi.status;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		}
	}

	vsbic_put_cmd(sc, cmd);

	if (ccb != NULL) {
		if (ISSET(ccb->ccb_xsflags, SCSI_POLL)) {
			SET(xs->flags, ITSDONE);
			/* caller will invoke vsbic_wrapup() later */
		} else
			vsbic_wrapup(sc, ccb);
	}

	return 1;
}
