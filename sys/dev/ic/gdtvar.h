/*	$OpenBSD: gdtvar.h,v 1.3 2000/03/01 22:38:51 niklas Exp $	*/

/*
 * Copyright (c) 1999, 2000 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * This driver would not have written if it was not for the hardware donations
 * from both ICP-Vortex and Öko.neT.  I want to thank them for their support.
 */

/* Debugging */
#ifdef GDT_DEBUG
#define GDT_DPRINTF(mask, args) if (gdt_debug & (mask)) printf args
#define GDT_D_INTR	0x01
#define GDT_D_MISC	0x02
#define GDT_D_CMD	0x04
#define GDT_D_QUEUE	0x08
#define GDT_D_IO	0x10
extern int gdt_debug;
#else
#define GDT_DPRINTF(mask, args)
#endif

/* Miscellaneous constants */
#define GDT_RETRIES		100000000	/* 100000 * 1us = 100s */
#define GDT_TIMEOUT		100000000	/* 100000 * 1us = 100s */
#define GDT_POLL_TIMEOUT	10000000	/* 10000 * 1us = 10s */
#define GDT_WATCH_TIMEOUT	10000000	/* 10000 * 1us = 10s */
#define GDT_SCRATCH_SZ		4096		/* 4KB scratch buffer */

/* Context structure for interrupt services */
struct gdt_intr_ctx {
	u_int32_t info, info2;
	u_int16_t cmd_status, service;
	u_int8_t istatus;
};

/*
 * A command contol block, one for each corresponding command index of the
 * controller.
 */
struct gdt_ccb {
	TAILQ_ENTRY(gdt_ccb) gc_chain;
	struct scsi_xfer *gc_xs;
	bus_dmamap_t gc_dmamap_xfer;
	int gc_timeout;
	u_int32_t gc_info;
	u_int32_t gc_blockno;
	u_int32_t gc_blockcnt;
	u_int16_t gc_status;
	u_int8_t gc_service;
	u_int8_t gc_cmd_index;
	u_int8_t gc_flags;
#define GDT_GCF_CMD_MASK	0x3
#define GDT_GCF_UNUSED		0	
#define GDT_GCF_INTERNAL	1
#define GDT_GCF_SCREEN 		2
#define GDT_GCF_SCSI 		3
#define GDT_GCF_WATCHDOG 	0x4
};

static __inline__ int gdt_ccb_set_cmd __P((struct gdt_ccb *, int));
static __inline__ int
gdt_ccb_set_cmd(ccb, flag)
	struct gdt_ccb *ccb;
	int flag;
{
	int rv = ccb->gc_flags & GDT_GCF_CMD_MASK;

	ccb->gc_flags &= ~GDT_GCF_CMD_MASK;
	ccb->gc_flags |= flag;
	return (rv);
}

struct gdt_softc {
	struct	device sc_dev;
	void   *sc_ih;
	struct	scsi_link sc_link;	/* Virtual SCSI bus for cache devs */
	struct	scsi_link *sc_raw_link;	/* Raw SCSI busses */

	int	sc_class;		/* Controller class */
#define GDT_ISA		0x01
#define GDT_EISA	0x02
#define GDT_PCI		0x03
#define GDT_PCINEW	0x04
#define GDT_MPR		0x05
#define GDT_CLASS_MASK	0x07
#define GDT_FC		0x10
#define GDT_CLASS(gdt)	((gdt)->sc_class & GDT_CLASS_MASK)

	bus_space_tag_t sc_dpmemt;
	bus_space_handle_t sc_dpmemh;
	bus_addr_t sc_dpmembase;
	bus_dma_tag_t sc_dmat;

	/* XXX These could go into a class-dependent opaque struct instead */
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_addr_t sc_iobase;

	u_int16_t sc_ic_all_size;

	struct gdt_ccb sc_ccbs[GDT_MAXCMDS];
	TAILQ_HEAD(, gdt_ccb) sc_free_ccb, sc_ccbq;
	LIST_HEAD(, scsi_xfer) sc_queue;
	struct scsi_xfer *sc_queuelast;

	u_int16_t sc_cmd_len;
	u_int16_t sc_cmd_off;
	u_int16_t sc_cmd_cnt;
	u_int8_t sc_cmd[GDT_CMD_SZ];

	u_int32_t sc_info;
	u_int32_t sc_info2;
	u_int16_t sc_status;
	u_int8_t sc_scratch[GDT_SCRATCH_SZ];

	u_int8_t sc_bus_cnt;
	u_int8_t sc_bus_id[GDT_MAXBUS];
	u_int8_t sc_more_proc;

	struct {
		u_int8_t hd_present;
		u_int8_t hd_is_logdrv;
		u_int8_t hd_is_arraydrv;
		u_int8_t hd_is_master;
		u_int8_t hd_is_parity;
		u_int8_t hd_is_hotfix;
		u_int8_t hd_master_no;
		u_int8_t hd_lock;
		u_int8_t hd_heads;
		u_int8_t hd_secs;
		u_int16_t hd_devtype;
		u_int32_t hd_size;
		u_int8_t hd_ldr_no;
		u_int8_t hd_rw_attribs;
		u_int32_t hd_start_sec;
	} sc_hdr[GDT_MAX_HDRIVES];

	struct {
		u_int32_t cp_version;
		u_int16_t cp_state;
		u_int16_t cp_strategy;
		u_int16_t cp_write_back;
		u_int16_t cp_block_size;
	} sc_cpar;

	struct {
		u_int32_t bi_ser_no;		/* serial number */
		u_int8_t bi_oem_id[2];		/* u_int8_t OEM ID */
		u_int16_t bi_ep_flags;		/* eprom flags */
		u_int32_t bi_proc_id;		/* processor ID */
		u_int32_t bi_memsize;		/* memory size (bytes) */
		u_int8_t bi_mem_banks;		/* memory banks */
		u_int8_t bi_chan_type;		/* channel type */
		u_int8_t bi_chan_count;		/* channel count */
		u_int8_t bi_rdongle_pres;	/* dongle present */
		u_int32_t bi_epr_fw_ver;	/* (eprom) firmware ver */
		u_int32_t bi_upd_fw_ver;	/* (update) firmware ver */
		u_int32_t bi_upd_revision;	/* update revision */
		char bi_type_string[16];	/* char controller name */
		char bi_raid_string[16];	/* char RAID firmware name */
		u_int8_t bi_update_pres;	/* update present? */
		u_int8_t bi_xor_pres;		/* XOR engine present */
		u_int8_t bi_prom_type;		/* ROM type (eprom/flash) */
		u_int8_t bi_prom_count;		/* number of ROM devices */
		u_int32_t bi_dup_pres;		/* duplexing module pres? */
		u_int32_t bi_chan_pres;		/* # of exp. channels */
		u_int32_t bi_mem_pres;		/* memory expansion inst? */
		u_int8_t bi_ft_bus_system;	/* fault bus supported? */
		u_int8_t bi_subtype_valid;	/* board_subtype valid */
		u_int8_t bi_board_subtype;	/* subtype/hardware level */
		u_int8_t bi_rampar_pres;	/* RAM parity check hw? */
	} sc_binfo;

	struct {
		u_int8_t bf_chaining;	/* chaining supported */
		u_int8_t bf_striping;	/* striping (RAID-0) supported */
		u_int8_t bf_mirroring;	/* mirroring (RAID-1) supported */
		u_int8_t bf_raid;	/* RAID-4/5/10 supported */
	} sc_bfeat;

	u_int16_t sc_raw_feat;
	u_int16_t sc_cache_feat;

	int sc_spl;

	void (*sc_copy_cmd) __P((struct gdt_softc *, struct gdt_ccb *));
	u_int8_t (*sc_get_status) __P((struct gdt_softc *));
	void (*sc_intr) __P((struct gdt_softc *, struct gdt_intr_ctx *));
	void (*sc_release_event) __P((struct gdt_softc *, struct gdt_ccb *));
	void (*sc_set_sema0) __P((struct gdt_softc *));
	int (*sc_test_busy) __P((struct gdt_softc *));
};

/* XXX These have to become spinlocks in case of SMP */
#define GDT_LOCK_GDT(gdt) (gdt)->sc_spl = splbio()
#define GDT_UNLOCK_GDT(gdt) splx((gdt)->sc_spl)

void	gdtminphys __P((struct buf *));
int	gdt_attach __P((struct gdt_softc *));
int	gdt_intr __P((void *));

#ifdef __GNUC__
/* These all require correctly aligned buffers */
static __inline__ void gdt_enc16 __P((u_int8_t *, u_int16_t));
static __inline__ void gdt_enc32 __P((u_int8_t *, u_int32_t));
static __inline__ u_int16_t gdt_dec16 __P((u_int8_t *));
static __inline__ u_int32_t gdt_dec32 __P((u_int8_t *));

static __inline__ void
gdt_enc16(addr, value)
	u_int8_t *addr;
	u_int16_t value;
{
	*(u_int16_t *)addr = htole16(value);
}

static __inline__ void
gdt_enc32(addr, value)
	u_int8_t *addr;
	u_int32_t value;
{
	*(u_int32_t *)addr = htole32(value);
}

static __inline__ u_int16_t
gdt_dec16(addr)
	u_int8_t *addr;
{
	return letoh16(*(u_int16_t *)addr);
}

static __inline__ u_int32_t
gdt_dec32(addr)
	u_int8_t *addr;
{
	return letoh32(*(u_int32_t *)addr);
}
#endif

#if defined(__alpha__)
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t)(va))
#endif

extern u_int8_t gdt_polling;
