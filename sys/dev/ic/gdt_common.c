/*	$OpenBSD: gdt_common.c,v 1.5 2000/03/01 22:38:51 niklas Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/gdtreg.h>
#include <dev/ic/gdtvar.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int	gdt_async_event __P((struct gdt_softc *, int));
void	gdt_chain __P((struct gdt_softc *));
void	gdt_clear_events __P((struct gdt_softc *));
void	gdt_copy_internal_data __P((struct scsi_xfer *, u_int8_t *, size_t));
struct scsi_xfer *gdt_dequeue __P((struct gdt_softc *));
void	gdt_enqueue __P((struct gdt_softc *, struct scsi_xfer *, int));
void	gdt_enqueue_ccb __P((struct gdt_softc *, struct gdt_ccb *));
void	gdt_eval_mapping __P((u_int32_t, int *, int *, int *));
int	gdt_exec_ccb __P((struct gdt_ccb *));
void	gdt_free_ccb __P((struct gdt_softc *, struct gdt_ccb *));
struct gdt_ccb *gdt_get_ccb __P((struct gdt_softc *, int));
int	gdt_internal_cache_cmd __P((struct scsi_xfer *));
int	gdt_internal_cmd __P((struct gdt_softc *, u_int8_t, u_int16_t,
    u_int32_t, u_int32_t, u_int32_t));
int	gdt_raw_scsi_cmd __P((struct scsi_xfer *));
int	gdt_scsi_cmd __P((struct scsi_xfer *));
void	gdt_start_ccbs __P((struct gdt_softc *));
int	gdt_sync_event __P((struct gdt_softc *, int, u_int8_t,
    struct scsi_xfer *));
void	gdt_timeout __P((void *));
int	gdt_wait __P((struct gdt_softc *, struct gdt_ccb *, int));
void	gdt_watchdog __P((void *));

struct cfdriver gdt_cd = {
	NULL, "gdt", DV_DULL
};

struct scsi_adapter gdt_switch = {
	gdt_scsi_cmd, gdtminphys, 0, 0,
};

struct scsi_adapter gdt_raw_switch = {
	gdt_raw_scsi_cmd, gdtminphys, 0, 0,
};

struct scsi_device gdt_dev = {
	NULL, NULL, NULL, NULL
};

u_int8_t gdt_polling;
u_int8_t gdt_from_wait;
struct gdt_softc *gdt_wait_gdt;
int	gdt_wait_index;
#ifdef GDT_DEBUG
int	gdt_debug = GDT_DEBUG;
#endif

int
gdt_attach(gdt)
	struct gdt_softc *gdt;
{
	u_int16_t cdev_cnt;
	int i, id, drv_cyls, drv_hds, drv_secs, error;

	gdt_polling = 1;
	gdt_from_wait = 0;
	gdt_clear_events(gdt);

	TAILQ_INIT(&gdt->sc_free_ccb);
	TAILQ_INIT(&gdt->sc_ccbq);
	LIST_INIT(&gdt->sc_queue);

	/* Initialize the ccbs */
	for (i = 0; i < GDT_MAXCMDS; i++) {
		gdt->sc_ccbs[i].gc_cmd_index = i + 2;
		error = bus_dmamap_create(gdt->sc_dmat,
		    (GDT_MAXOFFSETS - 1) << PGSHIFT, GDT_MAXOFFSETS,
		    (GDT_MAXOFFSETS - 1) << PGSHIFT, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &gdt->sc_ccbs[i].gc_dmamap_xfer);
		if (error) {
			printf("%s: cannot create ccb dmamap (%d)",
			    gdt->sc_dev.dv_xname, error);
			return (1);
		}
		(void)gdt_ccb_set_cmd(gdt->sc_ccbs + i, GDT_GCF_UNUSED);
		TAILQ_INSERT_TAIL(&gdt->sc_free_ccb, &gdt->sc_ccbs[i],
		    gc_chain);
	}

	/* Fill in the prototype scsi_link. */
	gdt->sc_link.adapter_softc = gdt;
	gdt->sc_link.adapter = &gdt_switch;
	gdt->sc_link.adapter_target = 7;
	gdt->sc_link.device = &gdt_dev;
	gdt->sc_link.openings = GDT_MAXCMDS;	/* XXX what is optimal? */
	gdt->sc_link.adapter_buswidth =
	    (gdt->sc_class & GDT_FC) ? GDT_MAXID : GDT_MAX_HDRIVES;

	if (!gdt_internal_cmd(gdt, GDT_SCREENSERVICE, GDT_INIT, 0, 0, 0)) {
		printf("screen service initialization error %d\n",
		     gdt->sc_status);
		return (1);
	}

	if (!gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_INIT, GDT_LINUX_OS, 0,
	    0)) {
		printf("cache service initialization error %d\n",
		    gdt->sc_status);
		return (1);
	}

	if (!gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_MOUNT, 0xffff, 1,
	    0)) {
		printf("cache service mount error %d\n",
		    gdt->sc_status);
		return (1);
	}

	if (!gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_INIT, GDT_LINUX_OS, 0,
	    0)) {
		printf("cache service post-mount initialization error %d\n",
		    gdt->sc_status);
		return (1);
	}
	cdev_cnt = (u_int16_t)gdt->sc_info;

	/* Detect number of busses */
	gdt_enc32(gdt->sc_scratch + GDT_IOC_VERSION, GDT_IOC_NEWEST);
	gdt->sc_scratch[GDT_IOC_LIST_ENTRIES] = GDT_MAXBUS;
	gdt->sc_scratch[GDT_IOC_FIRST_CHAN] = 0;
	gdt->sc_scratch[GDT_IOC_LAST_CHAN] = GDT_MAXBUS - 1;
	gdt_enc32(gdt->sc_scratch + GDT_IOC_LIST_OFFSET, GDT_IOC_HDR_SZ);
	if (gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_IOCTL,
	    GDT_IOCHAN_RAW_DESC, GDT_INVALID_CHANNEL,
	    GDT_IOC_HDR_SZ + GDT_RAWIOC_SZ)) {
		gdt->sc_bus_cnt = gdt->sc_scratch[GDT_IOC_CHAN_COUNT];
		for (i = 0; i < gdt->sc_bus_cnt; i++) {
			id = gdt->sc_scratch[GDT_IOC_HDR_SZ +
			    i * GDT_RAWIOC_SZ + GDT_RAWIOC_PROC_ID];
			gdt->sc_bus_id[id] = id < GDT_MAXID ? id : 0xff;
		}

	} else {
		/* New method failed, use fallback. */
		gdt_enc32(gdt->sc_scratch + GDT_GETCH_CHANNEL_NO, i);
		for (i = 0; i < GDT_MAXBUS; i++) {
			if (!gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_IOCTL,
			    GDT_SCSI_CHAN_CNT | GDT_L_CTRL_PATTERN,
			    GDT_IO_CHANNEL | GDT_INVALID_CHANNEL,
			    GDT_GETCH_SZ)) {
				if (i == 0) {
					printf("cannot get channel count, "
					    "error %d\n", gdt->sc_status);
					return (1);
				}
				break;
			}
			gdt->sc_bus_id[i] =
			    (gdt->sc_scratch[GDT_GETCH_SIOP_ID] < GDT_MAXID) ?
			    gdt->sc_scratch[GDT_GETCH_SIOP_ID] : 0xff;
		}
		gdt->sc_bus_cnt = i;
	}

	/* Read cache confgiuration */
	if (!gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_IOCTL, GDT_CACHE_INFO,
	    GDT_INVALID_CHANNEL, GDT_CINFO_SZ)) {
		printf("cannot get cache info, error %d\n", gdt->sc_status);
		return (1);
	}
	gdt->sc_cpar.cp_version =
	    gdt_dec32(gdt->sc_scratch + GDT_CPAR_VERSION);
	gdt->sc_cpar.cp_state = gdt_dec16(gdt->sc_scratch + GDT_CPAR_STATE);
	gdt->sc_cpar.cp_strategy =
	    gdt_dec16(gdt->sc_scratch + GDT_CPAR_STRATEGY);
	gdt->sc_cpar.cp_write_back =
	    gdt_dec16(gdt->sc_scratch + GDT_CPAR_WRITE_BACK);
	gdt->sc_cpar.cp_block_size =
	    gdt_dec16(gdt->sc_scratch + GDT_CPAR_BLOCK_SIZE);

	/* Read board information and features */
	gdt->sc_more_proc = 0;
	if (gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_IOCTL, GDT_BOARD_INFO,
	    GDT_INVALID_CHANNEL, GDT_BINFO_SZ)) {
		/* XXX A lot of these assignments can probably go later */
		gdt->sc_binfo.bi_ser_no =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_SER_NO);
		bcopy(gdt->sc_scratch + GDT_BINFO_OEM_ID,
		    gdt->sc_binfo.bi_oem_id, sizeof gdt->sc_binfo.bi_oem_id);
		gdt->sc_binfo.bi_ep_flags =
		    gdt_dec16(gdt->sc_scratch + GDT_BINFO_EP_FLAGS);
		gdt->sc_binfo.bi_proc_id =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_PROC_ID);
		gdt->sc_binfo.bi_memsize =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_MEMSIZE);
		gdt->sc_binfo.bi_mem_banks =
		    gdt->sc_scratch[GDT_BINFO_MEM_BANKS];
		gdt->sc_binfo.bi_chan_type =
		    gdt->sc_scratch[GDT_BINFO_CHAN_TYPE];
		gdt->sc_binfo.bi_chan_count =
		    gdt->sc_scratch[GDT_BINFO_CHAN_COUNT];
		gdt->sc_binfo.bi_rdongle_pres =
		    gdt->sc_scratch[GDT_BINFO_RDONGLE_PRES];
		gdt->sc_binfo.bi_epr_fw_ver =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_EPR_FW_VER);
		gdt->sc_binfo.bi_upd_fw_ver =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_UPD_FW_VER);
		gdt->sc_binfo.bi_upd_revision =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_UPD_REVISION);
		bcopy(gdt->sc_scratch + GDT_BINFO_TYPE_STRING,
		    gdt->sc_binfo.bi_type_string,
		    sizeof gdt->sc_binfo.bi_type_string);
		bcopy(gdt->sc_scratch + GDT_BINFO_RAID_STRING,
		    gdt->sc_binfo.bi_raid_string,
		    sizeof gdt->sc_binfo.bi_raid_string);
		gdt->sc_binfo.bi_update_pres =
		    gdt->sc_scratch[GDT_BINFO_UPDATE_PRES];
		gdt->sc_binfo.bi_xor_pres =
		    gdt->sc_scratch[GDT_BINFO_XOR_PRES];
		gdt->sc_binfo.bi_prom_type =
		    gdt->sc_scratch[GDT_BINFO_PROM_TYPE];
		gdt->sc_binfo.bi_prom_count =
		    gdt->sc_scratch[GDT_BINFO_PROM_COUNT];
		gdt->sc_binfo.bi_dup_pres =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_DUP_PRES);
		gdt->sc_binfo.bi_chan_pres =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_CHAN_PRES);
		gdt->sc_binfo.bi_mem_pres =
		    gdt_dec32(gdt->sc_scratch + GDT_BINFO_MEM_PRES);
		gdt->sc_binfo.bi_ft_bus_system =
		    gdt->sc_scratch[GDT_BINFO_FT_BUS_SYSTEM];
		gdt->sc_binfo.bi_subtype_valid =
		    gdt->sc_scratch[GDT_BINFO_SUBTYPE_VALID];
		gdt->sc_binfo.bi_board_subtype =
		    gdt->sc_scratch[GDT_BINFO_BOARD_SUBTYPE];
		gdt->sc_binfo.bi_rampar_pres =
		    gdt->sc_scratch[GDT_BINFO_RAMPAR_PRES];

		if (gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_IOCTL,
		    GDT_BOARD_FEATURES, GDT_INVALID_CHANNEL, GDT_BFEAT_SZ)) {
			gdt->sc_bfeat.bf_chaining =
			    gdt->sc_scratch[GDT_BFEAT_CHAINING];
			gdt->sc_bfeat.bf_striping =
			    gdt->sc_scratch[GDT_BFEAT_STRIPING];
			gdt->sc_bfeat.bf_mirroring =
			    gdt->sc_scratch[GDT_BFEAT_MIRRORING];
			gdt->sc_bfeat.bf_raid =
			    gdt->sc_scratch[GDT_BFEAT_RAID];
			gdt->sc_more_proc = 1;
		}
	} else {
		/* XXX Not implemented yet */		
	}

	/* Read more information */
	if (gdt->sc_more_proc) {
		/* XXX Not implemented yet */		
	}

	if (!gdt_internal_cmd(gdt, GDT_SCSIRAWSERVICE, GDT_INIT, 0, 0, 0)) {
		printf("raw service initialization error %d\n",
		    gdt->sc_status);
		return (1);
	}

	/* Set/get features raw service (scatter/gather) */
	gdt->sc_raw_feat = 0;
	if (gdt_internal_cmd(gdt, GDT_SCSIRAWSERVICE, GDT_SET_FEAT,
	    GDT_SCATTER_GATHER, 0, 0))
		if (gdt_internal_cmd(gdt, GDT_SCSIRAWSERVICE, GDT_GET_FEAT, 0,
		    0, 0))
			gdt->sc_raw_feat = gdt->sc_info;

	/* Set/get features cache service (scatter/gather) */
	gdt->sc_cache_feat = 0;
	if (gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_SET_FEAT, 0,
	    GDT_SCATTER_GATHER, 0))
		if (gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_GET_FEAT, 0, 0,
		    0))
			gdt->sc_cache_feat = gdt->sc_info;

	/* XXX Linux reserve drives here, potentially */

	/* Scan for cache devices */
	for (i = 0; i < cdev_cnt && i < GDT_MAX_HDRIVES; i++)
		if (gdt_internal_cmd(gdt, GDT_CACHESERVICE, GDT_INFO, i, 0,
		    0)) {
			gdt->sc_hdr[i].hd_present = 1;
			gdt->sc_hdr[i].hd_size = gdt->sc_info;

			/*
			 * Evaluate mapping (sectors per head, heads per cyl)
			 */
			gdt->sc_hdr[i].hd_size &= ~GDT_SECS32;
			if (gdt->sc_info2 == 0)
				gdt_eval_mapping(gdt->sc_hdr[i].hd_size,
				    &drv_cyls, &drv_hds, &drv_secs);
			else {
				drv_hds = gdt->sc_info2 & 0xff;
				drv_secs = (gdt->sc_info2 >> 8) & 0xff;
				drv_cyls = gdt->sc_hdr[i].hd_size / drv_hds /
				    drv_secs;
			}
			gdt->sc_hdr[i].hd_heads = drv_hds;
			gdt->sc_hdr[i].hd_secs = drv_secs;
			/* Round the size */
			gdt->sc_hdr[i].hd_size = drv_cyls * drv_hds * drv_secs;

			if (gdt_internal_cmd(gdt, GDT_CACHESERVICE,
			    GDT_DEVTYPE, i, 0, 0))
				gdt->sc_hdr[i].hd_devtype = gdt->sc_info;
		}

	printf("dpmem %x %d-bus %d cache device%s\n", gdt->sc_dpmembase,
	    gdt->sc_bus_cnt, cdev_cnt, cdev_cnt == 1 ? "" : "s");
	printf("%s: ver %x, cache %s, strategy %d, writeback %s, blksz %d\n",
	    gdt->sc_dev.dv_xname, gdt->sc_cpar.cp_version,
	    gdt->sc_cpar.cp_state ? "on" : "off", gdt->sc_cpar.cp_strategy,
	    gdt->sc_cpar.cp_write_back ? "on" : "off",
	    gdt->sc_cpar.cp_block_size);
#if 1
	printf("%s: raw feat %x cache feat %x\n", gdt->sc_dev.dv_xname,
	    gdt->sc_raw_feat, gdt->sc_cache_feat);
#endif

	config_found(&gdt->sc_dev, &gdt->sc_link, scsiprint);

	MALLOC(gdt->sc_raw_link, struct scsi_link *,
	    gdt->sc_bus_cnt * sizeof (struct scsi_link), M_DEVBUF, M_NOWAIT);
	bzero(gdt->sc_raw_link, gdt->sc_bus_cnt * sizeof (struct scsi_link));

	for (i = 0; i < gdt->sc_bus_cnt; i++) {
		/* Fill in the prototype scsi_link. */
		gdt->sc_raw_link[i].adapter_softc = gdt;
		gdt->sc_raw_link[i].adapter = &gdt_raw_switch;
		gdt->sc_raw_link[i].adapter_target = 7;
		gdt->sc_raw_link[i].device = &gdt_dev;
		gdt->sc_raw_link[i].openings = 4;	/* XXX a guess */
		gdt->sc_raw_link[i].adapter_buswidth =
		    (gdt->sc_class & GDT_FC) ? GDT_MAXID : 16;	/* XXX */

		config_found(&gdt->sc_dev, &gdt->sc_raw_link[i], scsiprint);
	}

	gdt_polling = 0;
	return (0);
}

void
gdt_eval_mapping(size, cyls, heads, secs)
	u_int32_t size;
	int *cyls, *heads, *secs;
{
	*cyls = size / GDT_HEADS / GDT_SECS;
	if (*cyls < GDT_MAXCYLS) {
		*heads = GDT_HEADS;
		*secs = GDT_SECS;
	} else {
		/* Too high for 64 * 32 */
		*cyls = size / GDT_MEDHEADS / GDT_MEDSECS;
		if (*cyls < GDT_MAXCYLS) {
			*heads = GDT_MEDHEADS;
			*secs = GDT_MEDSECS;
		} else {
			/* Too high for 127 * 63 */
			*cyls = size / GDT_BIGHEADS / GDT_BIGSECS;
			*heads = GDT_BIGHEADS;
			*secs = GDT_BIGSECS;
		}
	}
}

/*
 * Insert a command into the driver queue, either at the front or at the tail.
 * It's ok to overload the freelist link as these structures are never on
 * the freelist at this time.
 */
void
gdt_enqueue(gdt, xs, infront)
	struct gdt_softc *gdt;
	struct scsi_xfer *xs;
	int infront;
{
	if (infront || LIST_FIRST(&gdt->sc_queue) == NULL) {
		if (LIST_FIRST(&gdt->sc_queue) == NULL)
			gdt->sc_queuelast = xs;
		LIST_INSERT_HEAD(&gdt->sc_queue, xs, free_list);
		return;
	}
	LIST_INSERT_AFTER(gdt->sc_queuelast, xs, free_list);
	gdt->sc_queuelast = xs;
}

/*
 * Pull a command off the front of the driver queue.
 */
struct scsi_xfer *
gdt_dequeue(gdt)
	struct gdt_softc *gdt;
{
	struct scsi_xfer *xs;

	xs = LIST_FIRST(&gdt->sc_queue);
	LIST_REMOVE(xs, free_list);

	if (LIST_FIRST(&gdt->sc_queue) == NULL)
		gdt->sc_queuelast = NULL;

	return (xs);
}

/* Start a SCSI operation on a cache device */
int
gdt_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct gdt_softc *gdt = link->adapter_softc;
	u_int8_t target = link->target;
	struct gdt_ccb *ccb;
	int dontqueue = 0;
	u_int32_t blockno, blockcnt;
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	bus_dmamap_t xfer;
	int error;

	GDT_DPRINTF(GDT_D_CMD, ("gdt_scsi_cmd "));

	if (target >= GDT_MAX_HDRIVES || !gdt->sc_hdr[target].hd_present ||
	    link->lun != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}

	xs->error = XS_NOERROR;
	ccb = NULL;

	GDT_LOCK_GDT(gdt);
	if (!gdt_polling && gdt->sc_test_busy(gdt)) {
		/* Don't double enqueue if we came from gdt_chain. */
		if (xs != LIST_FIRST(&gdt->sc_queue))
			gdt_enqueue(gdt, xs, 0);
		GDT_UNLOCK_GDT(gdt);
		return (SUCCESSFULLY_QUEUED);
	}
	GDT_UNLOCK_GDT(gdt);

	switch (xs->cmd->opcode) {
	case TEST_UNIT_READY:
	case REQUEST_SENSE:
	case INQUIRY:
	case MODE_SENSE:
	case START_STOP:
	case READ_CAPACITY:
	case SYNCHRONIZE_CACHE:
#if 0
	case VERIFY:
#endif
		return (gdt_internal_cache_cmd(xs) ? COMPLETE :
		    TRY_AGAIN_LATER);

	case PREVENT_ALLOW:
		GDT_DPRINTF(GDT_D_CMD, ("PREVENT/ALLOW "));
		/* XXX Not yet implemented */
		xs->error = XS_NOERROR;
		return (COMPLETE);

	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		GDT_LOCK_GDT(gdt);

		/*
		 * When chaining commands we may get called with the
		 * first command in the queue, recognize this case
		 * easily.
		 */
		if (xs == LIST_FIRST(&gdt->sc_queue))
			xs = gdt_dequeue(gdt);
		else {
			/* A new command chain, start from the beginning.  */
			gdt->sc_cmd_off = 0;

			/* Don't resort to queuing if we are polling.  */
			dontqueue = xs->flags & SCSI_POLL;

			/*
			 * The queue, if existent, must be processed first,
			 * before the new command can be run.
			 */
			if (LIST_FIRST(&gdt->sc_queue) != NULL) {
				/* If order cannot be preserved, punt.  */
				if (dontqueue) {
					GDT_UNLOCK_GDT(gdt);
					xs->error = XS_DRIVER_STUFFUP;
					return (TRY_AGAIN_LATER);
				}

				/*
				 * Enqueue the new command, ponder on the front
				 * command of the queue instead.
				 */
				gdt_enqueue(gdt, xs, 0);
				xs = gdt_dequeue(gdt);
			}
		}

		if (xs->cmdlen == 6) {
			rw = (struct scsi_rw *)xs->cmd;
			blockno =
			    _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
			blockcnt = rw->length ? rw->length : 0x100;
		} else {
			rwb = (struct scsi_rw_big *)xs->cmd;
			blockno = _4btol(rwb->addr);
			blockcnt = _2btol(rwb->length);
		}
		if (blockno >= gdt->sc_hdr[target].hd_size ||
		    blockno + blockcnt > gdt->sc_hdr[target].hd_size) {
			GDT_UNLOCK_GDT(gdt);
			printf("%s: out of bounds %u-%u >= %u\n",
			    gdt->sc_dev.dv_xname, blockno, blockcnt,
			    gdt->sc_hdr[target].hd_size);
			scsi_done(xs);
			xs->error = XS_DRIVER_STUFFUP;
			return (COMPLETE);
		}

		ccb = gdt_get_ccb(gdt, xs->flags);

		/*
		 * Are we out of commands, then queue.  If we cannot queue,
		 * then punt.
		 */
		if (ccb == NULL) {
			if (dontqueue) {
				GDT_UNLOCK_GDT(gdt);
				scsi_done(xs);
				xs->error = XS_DRIVER_STUFFUP;
				return (COMPLETE);
			}
			if (xs->error) {
				GDT_UNLOCK_GDT(gdt);
				scsi_done(xs);
				return (COMPLETE);
			}

			/* Put back on the queue, in the front.  */
			gdt_enqueue(gdt, xs, 1);
			GDT_UNLOCK_GDT(gdt);
			return (SUCCESSFULLY_QUEUED);
		}

		ccb->gc_blockno = blockno;
		ccb->gc_blockcnt = blockcnt;
		ccb->gc_xs = xs;
		ccb->gc_timeout = xs->timeout;
		ccb->gc_service = GDT_CACHESERVICE;
		gdt_ccb_set_cmd(ccb, GDT_GCF_SCSI);

		xfer = ccb->gc_dmamap_xfer;
		error = bus_dmamap_load(gdt->sc_dmat, xfer, xs->data, 
		    xs->datalen, NULL, (xs->flags & SCSI_NOSLEEP) ? 
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
		if (error) {
			printf("%s: gdt_scsi_cmd: ", gdt->sc_dev.dv_xname); 
			if (error == EFBIG)
				printf("more than %d dma segs\n",
				    GDT_MAXOFFSETS);
			else
				printf("error %d loading dma map\n", error);
		
			xs->error = XS_DRIVER_STUFFUP;
			gdt_free_ccb(gdt, ccb);
			return (COMPLETE);
		}
		bus_dmamap_sync(gdt->sc_dmat, xfer,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);

		gdt_enqueue_ccb(gdt, ccb);
		/* XXX what if enqueue did not start a transfer? */
		if (gdt_polling) {
			if (!gdt_wait(gdt, ccb, ccb->gc_timeout)) {
				GDT_UNLOCK_GDT(gdt);
				printf("%s: command %d timed out\n",
				    gdt->sc_dev.dv_xname, ccb->gc_cmd_index);
				xs->error = XS_TIMEOUT;
				return (TRY_AGAIN_LATER);
			}
		}

		GDT_UNLOCK_GDT(gdt);

		if (gdt_polling) {
 			scsi_done(xs);
			return (COMPLETE);
		}
		return (SUCCESSFULLY_QUEUED);

	default:
		GDT_DPRINTF(GDT_D_CMD, ("unknown opc %d ", xs->cmd->opcode));
		/* XXX Not yet implemented */
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}
}

/* XXX Currently only for cacheservice, returns 0 if busy */
int
gdt_exec_ccb(ccb)
	struct gdt_ccb *ccb;
{
	struct scsi_xfer *xs = ccb->gc_xs;
	struct scsi_link *link = xs->sc_link;
	struct gdt_softc *gdt = link->adapter_softc;
	u_int8_t target = link->target;
	u_int32_t sg_canz;
	bus_dmamap_t xfer;
	int i;

	GDT_DPRINTF(GDT_D_CMD, ("gdt_exec_ccb(%p, %p) ", xs, ccb));

	gdt->sc_cmd_cnt = 0;
	/*
	 * XXX Yeah I know it's a always-true condition, but that may change
	 * later.
	 */
	if (gdt->sc_cmd_cnt == 0)
		gdt->sc_set_sema0(gdt);

	gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX, ccb->gc_cmd_index);
	gdt_enc32(gdt->sc_cmd + GDT_CMD_BOARDNODE, GDT_LOCALBOARD);
	gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DEVICENO,
	    target);

	switch (xs->cmd->opcode) {
	case PREVENT_ALLOW:
		/* XXX PREVENT_ALLOW support goes here */

		gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKNO,
		    1);
		sg_canz = 0;
		break;

	case WRITE_COMMAND:
	case WRITE_BIG:
		/* XXX WRITE_THR could be supported too */
		gdt->sc_cmd[GDT_CMD_OPCODE] = GDT_WRITE;
		break;

	case READ_COMMAND:
	case READ_BIG:
		gdt->sc_cmd[GDT_CMD_OPCODE] = GDT_READ;
		break;
	}

	if (xs->cmd->opcode != PREVENT_ALLOW) {
		gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKNO,
		    ccb->gc_blockno);
		gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKCNT,
		    ccb->gc_blockcnt);
	}

	xfer = ccb->gc_dmamap_xfer;
	if (gdt->sc_cache_feat & GDT_SCATTER_GATHER) {
		gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DESTADDR,
		    0xffffffff);
		for (i = 0; i < xfer->dm_nsegs; i++) {
			gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
			    GDT_CACHE_SG_LST + i * GDT_SG_SZ + GDT_SG_PTR,
			    xfer->dm_segs[i].ds_addr);
			gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
			    GDT_CACHE_SG_LST + i * GDT_SG_SZ + GDT_SG_LEN,
			    xfer->dm_segs[i].ds_len);
			GDT_DPRINTF(GDT_D_IO, ("#%d va %p pa %p len %x\n", i,
			    buf, xfer->dm_segs[i].ds_addr,
			    xfer->dm_segs[i].ds_len));
		}
		sg_canz = xfer->dm_nsegs;
		gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_LST +
		    sg_canz * GDT_SG_SZ + GDT_SG_LEN, 0);
	} else {
		/* XXX Hardly correct */
		gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DESTADDR,
		    xfer->dm_segs[0].ds_addr);
		sg_canz = 0;
	}
	gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_CANZ, sg_canz);

	gdt->sc_cmd_len =
	    roundup(GDT_CMD_UNION + GDT_CACHE_SG_LST + sg_canz * GDT_SG_SZ,
	    sizeof (u_int32_t));

	if (gdt->sc_cmd_cnt > 0 &&
	    gdt->sc_cmd_off + gdt->sc_cmd_len + GDT_DPMEM_COMMAND_OFFSET >
	    gdt->sc_ic_all_size) {
		printf("%s: DPMEM overflow\n", gdt->sc_dev.dv_xname);
		gdt_free_ccb(gdt, ccb);
		xs->error = XS_BUSY;
		return (0);
	}

	gdt->sc_copy_cmd(gdt, ccb);
	gdt->sc_release_event(gdt, ccb);

	xs->error = XS_NOERROR;
	xs->resid = 0;
	return (1);
}

void
gdt_copy_internal_data(xs, data, size)
	struct scsi_xfer *xs;
	u_int8_t *data;
	size_t size;
{
	size_t copy_cnt;

	GDT_DPRINTF(GDT_D_MISC, ("gdt_copy_internal_data "));

	if (!xs->datalen)
		printf("uio move not yet supported\n");
	else {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(data, xs->data, copy_cnt);
	}

	
}

/* Emulated SCSI operation on cache device */
int
gdt_internal_cache_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct gdt_softc *gdt = link->adapter_softc;
	struct scsi_inquiry_data inq;
	struct scsi_sense_data sd;
	struct {
		struct scsi_mode_header hd;
		struct scsi_blk_desc bd;
		union scsi_disk_pages dp;
	} mpd;
	struct scsi_read_cap_data rcd;
	u_int8_t target = link->target;

	GDT_DPRINTF(GDT_D_CMD, ("gdt_internal_cache_cmd "));

	switch (xs->cmd->opcode) {
	case TEST_UNIT_READY:
	case START_STOP:
#if 0
	case VERIFY:
#endif
		GDT_DPRINTF(GDT_D_CMD, ("opc %d tgt %d ", xs->cmd->opcode,
		    target));
		break;

	case SYNCHRONIZE_CACHE:
		GDT_DPRINTF(GDT_D_CMD, ("SYNCHRONIZE CACHE tgt %d ", target));
		break;

	case REQUEST_SENSE:
		GDT_DPRINTF(GDT_D_CMD, ("REQUEST SENSE tgt %d ", target));
		bzero(&sd, sizeof sd);
		sd.error_code = 0x70;
		sd.segment = 0;
		sd.flags = SKEY_NO_SENSE;
		gdt_enc32(sd.info, 0);
		sd.extra_len = 0;
		gdt_copy_internal_data(xs, (u_int8_t *)&sd, sizeof sd);
		break;

	case INQUIRY:
		GDT_DPRINTF(GDT_D_CMD, ("INQUIRY tgt %d devtype %x ", target,
		    gdt->sc_hdr[target].hd_devtype));
		bzero(&inq, sizeof inq);
		inq.device =
		    (gdt->sc_hdr[target].hd_devtype & 4) ? T_CDROM : T_DIRECT;
		inq.dev_qual2 =
		    (gdt->sc_hdr[target].hd_devtype & 1) ? SID_REMOVABLE : 0;
		inq.version = 2;
		inq.response_format = 2;
		inq.additional_length = 32;
		strcpy(inq.vendor, "ICP    ");
		sprintf(inq.product, "Host drive  #%02d", target);
		strcpy(inq.revision, "   ");
		gdt_copy_internal_data(xs, (u_int8_t *)&inq, sizeof inq);
		break;

	case MODE_SENSE:
		GDT_DPRINTF(GDT_D_CMD, ("MODE SENSE tgt %d ", target));

		bzero(&mpd, sizeof mpd);
		switch (((struct scsi_mode_sense *)xs->cmd)->page) {
		case 4:
			/* scsi_disk.h says this should be 0x16 */
			mpd.dp.rigid_geometry.pg_length = 0x16;
			mpd.hd.data_length = sizeof mpd.hd + sizeof mpd.bd +
			    mpd.dp.rigid_geometry.pg_length;
			mpd.hd.blk_desc_len = sizeof mpd.bd;

			/* XXX */
			mpd.hd.dev_spec =
			    (gdt->sc_hdr[target].hd_devtype & 2) ? 0x80 : 0;
			_lto3b(GDT_SECTOR_SIZE, mpd.bd.blklen);
			mpd.dp.rigid_geometry.pg_code = 4;
			_lto3b(gdt->sc_hdr[target].hd_size /
			    gdt->sc_hdr[target].hd_heads /
			    gdt->sc_hdr[target].hd_secs,
			    mpd.dp.rigid_geometry.ncyl);
			mpd.dp.rigid_geometry.nheads =
			    gdt->sc_hdr[target].hd_heads;
			gdt_copy_internal_data(xs, (u_int8_t *)&mpd,
			    sizeof mpd);
			break;

		default:
			printf("%s: mode sense page %d not simulated\n",
			    gdt->sc_dev.dv_xname,
       			    ((struct scsi_mode_sense *)xs->cmd)->page);
			xs->error = XS_DRIVER_STUFFUP;
			return (0);
		}
		break;

	case READ_CAPACITY:
		GDT_DPRINTF(GDT_D_CMD, ("READ CAPACITY tgt %d ", target));
		bzero(&rcd, sizeof rcd);
		_lto4b(gdt->sc_hdr[target].hd_size - 1, rcd.addr);
		_lto4b(GDT_SECTOR_SIZE, rcd.length);
		gdt_copy_internal_data(xs, (u_int8_t *)&rcd, sizeof rcd);
		break;

	default:
		printf("gdt_internal_cache_cmd got bad opcode: %d\n",
		    xs->cmd->opcode);
		xs->error = XS_DRIVER_STUFFUP;
		return (0);
	}

	xs->error = XS_NOERROR;
	return (1);
}

/* Start a raw SCSI operation */
int
gdt_raw_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	GDT_DPRINTF(GDT_D_CMD, ("gdt_raw_scsi_cmd "));

	/* XXX Not yet implemented */
	xs->error = XS_DRIVER_STUFFUP;
	return (COMPLETE);
}

void
gdt_clear_events(gdt)
	struct gdt_softc *gdt;
{
	GDT_DPRINTF(GDT_D_MISC, ("gdt_clear_events(%p) ", gdt));

	/* XXX To be implemented */
}

int
gdt_async_event(gdt, service)
	struct gdt_softc *gdt;
	int service;
{
	GDT_DPRINTF(GDT_D_INTR, ("gdt_async_event(%p, %d) ", gdt, service));

	if (service == GDT_SCREENSERVICE) {
		/* XXX To be implemented */
	} else {
		/* XXX To be implemented */
	}

	return (0);
}

int
gdt_sync_event(gdt, service, index, xs)
	struct gdt_softc *gdt;
	int service;
	u_int8_t index;
	struct scsi_xfer *xs;
{
	GDT_DPRINTF(GDT_D_INTR,
	    ("gdt_sync_event(%p, %d, %d, %p) ", gdt, service, index, xs));

	if (service == GDT_SCREENSERVICE) {
		/* XXX To be implemented */
		return (0);
	} else {
		if (gdt->sc_status == GDT_S_OK) {
			/* XXX To be implemented */
		} else {
			/* XXX To be implemented */
			return (0);
		}
	}

	return (1);
}

int
gdt_intr(arg)
	void *arg;
{
	struct gdt_softc *gdt = arg;
	struct gdt_intr_ctx ctx;
	int chain = 1, sync_val = 0;
	struct scsi_xfer *xs;
	int prev_cmd;
	struct gdt_ccb *ccb;

	GDT_DPRINTF(GDT_D_INTR, ("gdt_intr(%p) ", gdt));

	/* If polling and we were not called from gdt_wait, just return */
	if (gdt_polling && !gdt_from_wait)
		return (0);

	if (!gdt_polling)
		GDT_LOCK_GDT(gdt);

	ctx.istatus = gdt->sc_get_status(gdt);
	if (!ctx.istatus) {
		GDT_UNLOCK_GDT(gdt);
		gdt->sc_status = GDT_S_NO_STATUS;
		return (0);
	}

	gdt_wait_index = 0;
	ctx.service = ctx.info2 = 0;

	gdt->sc_intr(gdt, &ctx);

	gdt->sc_status = ctx.cmd_status;
	gdt->sc_info = ctx.info;
	gdt->sc_info2 = ctx.info2;

	if (gdt_from_wait) {
		gdt_wait_gdt = gdt;
		gdt_wait_index = ctx.istatus;
	}

	switch (ctx.istatus) {
	case GDT_ASYNCINDEX:
		gdt_async_event(gdt, ctx.service);
		goto finish;

	case GDT_SPEZINDEX:
		/* XXX Not yet implemented */
		chain = 0;
		goto finish;
	}

	ccb = &gdt->sc_ccbs[ctx.istatus - 2];
	xs = ccb->gc_xs;
	if (!gdt_polling)
		untimeout(gdt_timeout, ccb);
	ctx.service = ccb->gc_service;
	prev_cmd = ccb->gc_flags & GDT_GCF_CMD_MASK;
	bus_dmamap_sync(gdt->sc_dmat, ccb->gc_dmamap_xfer,
	    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(gdt->sc_dmat, ccb->gc_dmamap_xfer);
	gdt_free_ccb(gdt, ccb);
	switch (prev_cmd) {
	case GDT_GCF_UNUSED:
		/* XXX Not yet implemented */
		chain = 0;
		goto finish;
	case GDT_GCF_INTERNAL:
		chain = 0;
		goto finish;
	}

	sync_val = gdt_sync_event(gdt, ctx.service, ctx.istatus, xs);

 finish:
	if (!gdt_polling)
		GDT_UNLOCK_GDT(gdt);

	switch (sync_val) {
	case 1:
		xs->flags |= ITSDONE;
		scsi_done(xs);
		break;

	case 2:
		gdt_enqueue(gdt, xs, 0);
	}

	if (chain)
		gdt_chain(gdt);
	return (1);
}

void
gdtminphys(bp)
	struct buf *bp;
{
#if 0
	u_int8_t *buf = bp->b_data;
	paddr_t pa;
	long off;
#endif

	GDT_DPRINTF(GDT_D_MISC, ("gdtminphys(0x%x) ", bp));

#if 1
	/* As this is way more than MAXPHYS it's really not necessary. */
	if (bp->b_bcount > ((GDT_MAXOFFSETS - 1) * PAGE_SIZE))
		bp->b_bcount = ((GDT_MAXOFFSETS - 1) * PAGE_SIZE);
#else
	for (off = PAGE_SIZE, pa = vtophys(buf); off < bp->b_bcount;
	    off += PAGE_SIZE)
		if (pa + off != vtophys(buf + off)) {
			bp->b_bcount = off;
			break;
		}
#endif
	minphys(bp);
}

int
gdt_wait(gdt, ccb, timeout)
	struct gdt_softc *gdt;
	struct gdt_ccb *ccb;
	int timeout;
{
	int rv = 0;

	GDT_DPRINTF(GDT_D_MISC,
	    ("gdt_wait(%p, %p, %d) ", gdt, ccb, timeout));

	gdt_from_wait = 1;
	do {
		if (gdt_intr(gdt) && gdt == gdt_wait_gdt &&
		    ccb->gc_cmd_index == gdt_wait_index) {
			rv = 1;
			break;
		}
		DELAY(1);
	} while (--timeout);
	gdt_from_wait = 0;

	while (gdt->sc_test_busy(gdt))
		DELAY(0);		/* XXX correct? */

	return (rv);
}

int
gdt_internal_cmd(gdt, service, opcode, arg1, arg2, arg3)
	struct gdt_softc *gdt;
	u_int8_t service;
	u_int16_t opcode;
	u_int32_t arg1, arg2, arg3;
{
	int retries;
	struct gdt_ccb *ccb;

	GDT_DPRINTF(GDT_D_CMD, ("gdt_internal_cmd(%p, %d, %d, %d, %d, %d) ",
	    gdt, service, opcode, arg1, arg2, arg3));

	bzero(gdt->sc_cmd, GDT_CMD_SZ);

	for (retries = GDT_RETRIES; ; ) {
		ccb = gdt_get_ccb(gdt, SCSI_NOSLEEP);
		if (ccb == NULL) {
			printf("%s: no free command index found\n",
			    gdt->sc_dev.dv_xname);
			return (0);
		}
		ccb->gc_service = service;
		gdt_ccb_set_cmd(ccb, GDT_GCF_INTERNAL);

		gdt->sc_set_sema0(gdt);
		gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
		    ccb->gc_cmd_index);
		gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, opcode);
		gdt_enc32(gdt->sc_cmd + GDT_CMD_BOARDNODE, GDT_LOCALBOARD);

		switch (service) {
		case GDT_CACHESERVICE:
			if (opcode == GDT_IOCTL) {
				gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
				    GDT_IOCTL_SUBFUNC, arg1);
				gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
				    GDT_IOCTL_CHANNEL, arg2);
				gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION +
				    GDT_IOCTL_PARAM_SIZE, (u_int16_t)arg3);
				gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
				    GDT_IOCTL_P_PARAM,
				    vtophys(gdt->sc_scratch));
			} else {
				gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION +
				    GDT_CACHE_DEVICENO, (u_int16_t)arg1);
				gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
				    GDT_CACHE_BLOCKNO, arg2);
			}
			break;

		case GDT_SCSIRAWSERVICE:
			gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
			    GDT_RAW_DIRECTION, arg1);
			gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_BUS] =
			    (u_int8_t)arg2;
			gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_TARGET] =
			    (u_int8_t)arg3;
			gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_LUN] =
			    (u_int8_t)(arg3 >> 8);
		}

		gdt->sc_cmd_len = GDT_CMD_SZ;
		gdt->sc_cmd_off = 0;
		gdt->sc_cmd_cnt = 0;
		gdt->sc_copy_cmd(gdt, ccb);
		gdt->sc_release_event(gdt, ccb);
		DELAY(20);
		if (!gdt_wait(gdt, ccb, GDT_POLL_TIMEOUT))
			return (0);
		if (gdt->sc_status != GDT_S_BSY || --retries == 0)
			break;
		DELAY(1);
	}
	return (gdt->sc_status == GDT_S_OK);
}

struct gdt_ccb *
gdt_get_ccb(gdt, flags)
	struct gdt_softc *gdt;
	int flags;
{
	struct gdt_ccb *ccb;

	GDT_DPRINTF(GDT_D_QUEUE, ("gdt_get_ccb(%p, 0x%x) ", gdt, flags));

	GDT_LOCK_GDT(gdt);

	for (;;) {
		ccb = TAILQ_FIRST(&gdt->sc_free_ccb);
		if (ccb != NULL)
			break;
		if (flags & SCSI_NOSLEEP)
			goto bail_out;
		tsleep(&gdt->sc_free_ccb, PRIBIO, "gdt_ccb", 0);
	}

	TAILQ_REMOVE(&gdt->sc_free_ccb, ccb, gc_chain);

 bail_out:
	GDT_UNLOCK_GDT(gdt);
	return (ccb);
}

void
gdt_free_ccb(gdt, ccb)
	struct gdt_softc *gdt;
	struct gdt_ccb *ccb;
{
	GDT_DPRINTF(GDT_D_QUEUE, ("gdt_free_ccb(%p, %p) ", gdt, ccb));

	GDT_LOCK_GDT(gdt);

	TAILQ_INSERT_HEAD(&gdt->sc_free_ccb, ccb, gc_chain);

	/* If the free list was empty, wake up potential waiters. */
	if (TAILQ_NEXT(ccb, gc_chain) == NULL)
		wakeup(&gdt->sc_free_ccb);

	GDT_UNLOCK_GDT(gdt);
}

void
gdt_enqueue_ccb(gdt, ccb)
	struct gdt_softc *gdt;
	struct gdt_ccb *ccb;
{
	GDT_DPRINTF(GDT_D_QUEUE, ("gdt_enqueue_ccb(%p, %p) ", gdt, ccb));

	TAILQ_INSERT_TAIL(&gdt->sc_ccbq, ccb, gc_chain);
	gdt_start_ccbs(gdt);
}

void
gdt_start_ccbs(gdt)
	struct gdt_softc *gdt;
{
	struct gdt_ccb *ccb;

	GDT_DPRINTF(GDT_D_QUEUE, ("gdt_start_ccbs(%p) ", gdt));

	while ((ccb = TAILQ_FIRST(&gdt->sc_ccbq)) != NULL) {
		if (ccb->gc_flags & GDT_GCF_WATCHDOG)
			untimeout(gdt_watchdog, ccb);

		if (gdt_exec_ccb(ccb) == 0) {
			ccb->gc_flags |= GDT_GCF_WATCHDOG;
			timeout(gdt_watchdog, ccb,
			    (GDT_WATCH_TIMEOUT * hz) / 1000);
			break;
		}
		TAILQ_REMOVE(&gdt->sc_ccbq, ccb, gc_chain);

		if ((ccb->gc_xs->flags & SCSI_POLL) == 0)
			timeout(gdt_timeout, ccb,
			    (ccb->gc_timeout * hz) / 1000);
	}
}

void
gdt_chain(gdt)
	struct gdt_softc *gdt;
{
	GDT_DPRINTF(GDT_D_INTR, ("gdt_chain(%p) ", gdt));

	if (LIST_FIRST(&gdt->sc_queue))
		gdt_scsi_cmd(LIST_FIRST(&gdt->sc_queue));
}

void
gdt_timeout(arg)
	void *arg;
{
	struct gdt_ccb *ccb = arg;
	struct scsi_link *link = ccb->gc_xs->sc_link;
	struct gdt_softc *gdt = link->adapter_softc;

	sc_print_addr(link);
	printf("timed out\n");

	/* XXX Test for multiple timeouts */

	ccb->gc_xs->error = XS_TIMEOUT;
	GDT_LOCK_GDT(gdt);
	gdt_enqueue_ccb(gdt, ccb);
	GDT_UNLOCK_GDT(gdt);
}

void
gdt_watchdog(arg)
	void *arg;
{
	struct gdt_ccb *ccb = arg;
	struct scsi_link *link = ccb->gc_xs->sc_link;
	struct gdt_softc *gdt = link->adapter_softc;

	GDT_LOCK_GDT(gdt);
	ccb->gc_flags &= ~GDT_GCF_WATCHDOG;
	gdt_start_ccbs(gdt);
	GDT_UNLOCK_GDT(gdt);
}
