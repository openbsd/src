/*	$OpenBSD: aac.c,v 1.9 2001/11/05 17:25:58 art Exp $	*/

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2000 Niklas Hallqvist
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 *	$FreeBSD: /c/ncvs/src/sys/dev/aac/aac.c,v 1.1 2000/09/13 03:20:34 msmith Exp $
 */

/*
 * Driver for the Adaptec 'FSA' family of PCI/SCSI RAID adapters.
 */

/*
 * This driver would not have rewritten for OpenBSD if it was not for the
 * hardware donation from Nocom.  I want to thank them for their support.
 * Of course, credit should go to Mike Smith for the original work he did
 * in the FreeBSD driver where I found lots of reusable code and inspiration.
 * - Niklas Hallqvist
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <vm/vm.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/aacreg.h>
#include <dev/ic/aacvar.h>
#include <dev/ic/aac_tables.h>

/* Geometry constants. */
#define AAC_MAXCYLS		1024
#define AAC_HEADS		64
#define AAC_SECS		32	/* mapping 64*32 */
#define AAC_MEDHEADS		127
#define AAC_MEDSECS		63	/* mapping 127*63 */
#define AAC_BIGHEADS		255
#define AAC_BIGSECS		63	/* mapping 255*63 */
#define AAC_SECS32		0x1f	/* round capacity */

void	aac_bio_complete __P((struct aac_ccb *));
void	aac_complete __P((void *, int));
void	aac_copy_internal_data __P((struct scsi_xfer *, u_int8_t *, size_t));
struct scsi_xfer *aac_dequeue __P((struct aac_softc *));
int	aac_dequeue_fib __P((struct aac_softc *, int, u_int32_t *,
    struct aac_fib **));
char   *aac_describe_code __P((struct aac_code_lookup *, u_int32_t));
void	aac_describe_controller __P((struct aac_softc *));
void	aac_enqueue __P((struct aac_softc *, struct scsi_xfer *, int));
void	aac_enqueue_ccb __P((struct aac_softc *, struct aac_ccb *));
int	aac_enqueue_fib __P((struct aac_softc *, int, u_int32_t, u_int32_t));
void	aac_eval_mapping __P((u_int32_t, int *, int *, int *));
int	aac_exec_ccb __P((struct aac_ccb *));
void	aac_free_ccb __P((struct aac_softc *, struct aac_ccb *));
struct aac_ccb *aac_get_ccb __P((struct aac_softc *, int));
#if 0
void	aac_handle_aif __P((struct aac_softc *, struct aac_aif_command *));
#endif
void	aac_host_command __P((struct aac_softc *));
void	aac_host_response __P((struct aac_softc *));
int	aac_init __P((struct aac_softc *));
int	aac_internal_cache_cmd __P((struct scsi_xfer *));
int	aac_map_command __P((struct aac_ccb *));
#ifdef AAC_DEBUG
void	aac_print_fib __P((struct aac_softc *, struct aac_fib *, char *));
#endif
int	aac_raw_scsi_cmd __P((struct scsi_xfer *));
int	aac_scsi_cmd __P((struct scsi_xfer *));
int	aac_start __P((struct aac_ccb *));
void	aac_start_ccbs __P((struct aac_softc *));
void	aac_startup __P((struct aac_softc *));
int	aac_sync_command __P((struct aac_softc *, u_int32_t, u_int32_t,
    u_int32_t, u_int32_t, u_int32_t, u_int32_t *));
int	aac_sync_fib __P((struct aac_softc *, u_int32_t, u_int32_t, void *,
    u_int16_t, void *, u_int16_t *));
void	aac_timeout __P((void *));
void	aac_unmap_command __P((struct aac_ccb *));
void	aac_watchdog __P((void *));

struct cfdriver aac_cd = {
	NULL, "aac", DV_DULL
};

struct scsi_adapter aac_switch = {
	aac_scsi_cmd, aacminphys, 0, 0,
};

struct scsi_adapter aac_raw_switch = {
	aac_raw_scsi_cmd, aacminphys, 0, 0,
};

struct scsi_device aac_dev = {
	NULL, NULL, NULL, NULL
};

/* i960Rx interface */    
int	aac_rx_get_fwstatus __P((struct aac_softc *));
void	aac_rx_qnotify __P((struct aac_softc *, int));
int	aac_rx_get_istatus __P((struct aac_softc *));
void	aac_rx_clear_istatus __P((struct aac_softc *, int));
void	aac_rx_set_mailbox __P((struct aac_softc *, u_int32_t, u_int32_t,
    u_int32_t, u_int32_t, u_int32_t));
int	aac_rx_get_mailboxstatus __P((struct aac_softc *));
void	aac_rx_set_interrupts __P((struct aac_softc *, int));

/* StrongARM interface */
int	aac_sa_get_fwstatus __P((struct aac_softc *));
void	aac_sa_qnotify __P((struct aac_softc *, int));
int	aac_sa_get_istatus __P((struct aac_softc *));
void	aac_sa_clear_istatus __P((struct aac_softc *, int));
void	aac_sa_set_mailbox __P((struct aac_softc *, u_int32_t, u_int32_t,
    u_int32_t, u_int32_t, u_int32_t));
int	aac_sa_get_mailboxstatus __P((struct aac_softc *));
void	aac_sa_set_interrupts __P((struct aac_softc *, int));

struct aac_interface aac_rx_interface = {
	aac_rx_get_fwstatus,
	aac_rx_qnotify,
	aac_rx_get_istatus,
	aac_rx_clear_istatus,
	aac_rx_set_mailbox,
	aac_rx_get_mailboxstatus,
	aac_rx_set_interrupts
};

struct aac_interface aac_sa_interface = {
	aac_sa_get_fwstatus,
	aac_sa_qnotify,
	aac_sa_get_istatus,
	aac_sa_clear_istatus,
	aac_sa_set_mailbox,
	aac_sa_get_mailboxstatus,
	aac_sa_set_interrupts
};

#ifdef AAC_DEBUG
int	aac_debug = AAC_DEBUG;
#endif

int
aac_attach(sc)
	struct aac_softc *sc;
{
	int i, error;
	bus_dma_segment_t seg;
	int nsegs;
	struct aac_ccb *ccb;

	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_ccbq);
	TAILQ_INIT(&sc->sc_completed);
	LIST_INIT(&sc->sc_queue);

	/* disable interrupts before we enable anything */
	AAC_MASK_INTERRUPTS(sc);

	/* mark controller as suspended until we get ourselves organised */
	sc->sc_state |= AAC_STATE_SUSPEND;

	/*
	 * Initialise the adapter.
	 */
	error = aac_init(sc);
	if (error)
		return (error);

	/* 
	 * Print a little information about the controller.
	 */
	aac_describe_controller(sc);

	/* Initialize the ccbs */
	for (i = 0; i < AAC_ADAP_NORM_CMD_ENTRIES; i++) {
		ccb = &sc->sc_ccbs[i];
		error = bus_dmamap_create(sc->sc_dmat,
		    (AAC_MAXSGENTRIES - 1) << PGSHIFT, AAC_MAXSGENTRIES,
		    (AAC_MAXSGENTRIES - 1) << PGSHIFT, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->ac_dmamap_xfer);
		if (error) {
			printf("%s: cannot create ccb dmamap (%d)",
			    sc->sc_dev.dv_xname, error);
			/* XXX cleanup */
			return (1);
		}

		/* allocate the FIB cluster in DMAable memory and load it */
		if (bus_dmamem_alloc(sc->sc_dmat, sizeof *ccb->ac_fib, 1, 0,
		    &seg, 1, &nsegs, BUS_DMA_NOWAIT)) {
			printf("%s: can't allocate FIB structure\n",
			    sc->sc_dev.dv_xname);
			/* XXX cleanup */
			return (1);
		}
		ccb->ac_fibphys = seg.ds_addr;
		if (bus_dmamem_map(sc->sc_dmat, &seg, nsegs,
		    sizeof *ccb->ac_fib, (caddr_t *)&ccb->ac_fib, 0)) {
			printf("%s: can't map FIB structure\n",
			    sc->sc_dev.dv_xname);
			/* XXX cleanup */
			return (1);
		}

		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, &sc->sc_ccbs[i],
		    ac_chain);
	}

	/* Fill in the prototype scsi_link. */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &aac_switch;
	sc->sc_link.device = &aac_dev;
	sc->sc_link.openings = AAC_ADAP_NORM_CMD_ENTRIES; /* XXX optimal? */
	sc->sc_link.adapter_buswidth = AAC_MAX_CONTAINERS;
	sc->sc_link.adapter_target = AAC_MAX_CONTAINERS;

	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

	return (0);
}

/*
 * Look up a text description of a numeric error code and return a pointer to
 * same.
 */
char *
aac_describe_code(table, code)
	struct aac_code_lookup *table;
	u_int32_t code;
{
	int i;

	for (i = 0; table[i].string != NULL; i++)
		if (table[i].code == code)
			return (table[i].string);
	return (table[i + 1].string);
}

void
aac_describe_controller(sc)
	struct aac_softc *sc;
{
	u_int8_t buf[AAC_FIB_DATASIZE];	/* XXX a bit big for the stack */
	u_int16_t bufsize;
	struct aac_adapter_info *info;
	u_int8_t arg;

	arg = 0;
	if (aac_sync_fib(sc, RequestAdapterInfo, 0, &arg, sizeof arg, &buf,
	    &bufsize)) {
		printf("%s: RequestAdapterInfo failed\n", sc->sc_dev.dv_xname);
		return;
	}
	if (bufsize != sizeof *info) {
		printf("%s: "
		    "RequestAdapterInfo returned wrong data size (%d != %d)\n",
		    sc->sc_dev.dv_xname, bufsize, sizeof *info);
		return;
	}
	info = (struct aac_adapter_info *)&buf[0];

	printf("%s: %s %dMHz, %dMB, %s (%d) Kernel %d.%d-%d\n",
	    sc->sc_dev.dv_xname,
	    aac_describe_code(aac_cpu_variant, info->CpuVariant),
	    info->ClockSpeed, info->TotalMem / (1024 * 1024),
	    aac_describe_code(aac_battery_platform, info->batteryPlatform),
	    info->batteryPlatform, info->KernelRevision.external.comp.major,
	    info->KernelRevision.external.comp.minor,
	    info->KernelRevision.external.comp.dash);

	/* save the kernel revision structure for later use */
	sc->sc_revision = info->KernelRevision;
}

int
aac_init(sc)
	struct aac_softc *sc;
{
	bus_dma_segment_t seg;
	int nsegs;
	int i, error;
	int state = 0;
	struct aac_adapter_init	*ip;
	u_int32_t code;
	u_int8_t *qaddr;

	/*
	 * First wait for the adapter to come ready.
	 */
	for (i = 0; i < AAC_BOOT_TIMEOUT * 1000; i++) {
		code = AAC_GET_FWSTATUS(sc);
		if (code & AAC_SELF_TEST_FAILED) {
			printf("%s: FATAL: selftest failed\n",
			    sc->sc_dev.dv_xname);
			return (ENXIO);
		}
		if (code & AAC_KERNEL_PANIC) {
			printf("%s: FATAL: controller kernel panic\n",
			    sc->sc_dev.dv_xname);
			return (ENXIO);
		}
		if (code & AAC_UP_AND_RUNNING)
			break;
		DELAY(1000);
	}
	if (i == AAC_BOOT_TIMEOUT * 1000) {
		printf("%s: FATAL: controller not coming ready, status %x\n",
		    sc->sc_dev.dv_xname, code);
		return (ENXIO);
	}

	if (bus_dmamem_alloc(sc->sc_dmat, sizeof *sc->sc_common, 1, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT)) {
		printf("%s: can't allocate common structure\n",
		    sc->sc_dev.dv_xname);
		return (ENOMEM);
	}
	state++;
	sc->sc_common_busaddr = seg.ds_addr;
	if (bus_dmamem_map(sc->sc_dmat, &seg, nsegs, sizeof *sc->sc_common,
	    (caddr_t *)&sc->sc_common, 0)) {
		printf("%s: can't map common structure\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto bail_out;
	}
	state++;
	bzero(sc->sc_common, sizeof *sc->sc_common);
    
	/*
	 * Fill in the init structure.  This tells the adapter about
	 * the physical location * of various important shared data
	 * structures.
	 */
	ip = &sc->sc_common->ac_init;
	ip->InitStructRevision = AAC_INIT_STRUCT_REVISION;

	ip->AdapterFibsPhysicalAddress =
	    sc->sc_common_busaddr + offsetof(struct aac_common, ac_fibs);
	ip->AdapterFibsVirtualAddress = &sc->sc_common->ac_fibs[0];
	ip->AdapterFibsSize = AAC_ADAPTER_FIBS * sizeof(struct aac_fib);
	ip->AdapterFibAlign = sizeof(struct aac_fib);

	ip->PrintfBufferAddress =
	    sc->sc_common_busaddr + offsetof(struct aac_common, ac_printf);
	ip->PrintfBufferSize = AAC_PRINTF_BUFSIZE;

	ip->HostPhysMemPages = 0;	/* not used? */
	ip->HostElapsedSeconds = 0;	/* reset later if invalid */

	/*
	 * Initialise FIB queues.  Note that it appears that the
	 * layout of the indexes and the segmentation of the entries
	 * is mandated by the adapter, which is only told about the
	 * base of the queue index fields.
	 *
	 * The initial values of the indices are assumed to inform the
	 * adapter of the sizes of the respective queues.
	 *
	 * The Linux driver uses a much more complex scheme whereby
	 * several header * records are kept for each queue.  We use a
	 * couple of generic list manipulation functions which
	 * 'know' the size of each list by virtue of a table.
	 */
	qaddr = &sc->sc_common->ac_qbuf[0] + AAC_QUEUE_ALIGN;
	qaddr -= (u_int32_t)qaddr % AAC_QUEUE_ALIGN; 	/* XXX not portable */
	sc->sc_queues = (struct aac_queue_table *)qaddr;
	ip->CommHeaderAddress = sc->sc_common_busaddr +
	    ((char *)sc->sc_queues - (char *)sc->sc_common);
	bzero(sc->sc_queues, sizeof(struct aac_queue_table));

	sc->sc_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_HOST_NORM_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_HOST_NORM_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_HOST_HIGH_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_HOST_HIGH_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_ADAP_NORM_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_ADAP_NORM_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_ADAP_HIGH_CMD_ENTRIES;
	sc->sc_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_ADAP_HIGH_CMD_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_HOST_NORM_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_HOST_NORM_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_HOST_HIGH_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_HOST_HIGH_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_ADAP_NORM_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_ADAP_NORM_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX] =
	    AAC_ADAP_HIGH_RESP_ENTRIES;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX] =
	    AAC_ADAP_HIGH_RESP_ENTRIES;
	sc->sc_qentries[AAC_HOST_NORM_CMD_QUEUE] =
	    &sc->sc_queues->qt_HostNormCmdQueue[0];
	sc->sc_qentries[AAC_HOST_HIGH_CMD_QUEUE] =
	    &sc->sc_queues->qt_HostHighCmdQueue[0];
	sc->sc_qentries[AAC_ADAP_NORM_CMD_QUEUE] =
	    &sc->sc_queues->qt_AdapNormCmdQueue[0];
	sc->sc_qentries[AAC_ADAP_HIGH_CMD_QUEUE] =
	    &sc->sc_queues->qt_AdapHighCmdQueue[0];
	sc->sc_qentries[AAC_HOST_NORM_RESP_QUEUE] =
	    &sc->sc_queues->qt_HostNormRespQueue[0];
	sc->sc_qentries[AAC_HOST_HIGH_RESP_QUEUE] =
	    &sc->sc_queues->qt_HostHighRespQueue[0];
	sc->sc_qentries[AAC_ADAP_NORM_RESP_QUEUE] =
	    &sc->sc_queues->qt_AdapNormRespQueue[0];
	sc->sc_qentries[AAC_ADAP_HIGH_RESP_QUEUE] =
	    &sc->sc_queues->qt_AdapHighRespQueue[0];

	/*
	 * Do controller-type-specific initialisation
	 */
	switch (sc->sc_hwif) {
	case AAC_HWIF_I960RX:
		AAC_SETREG4(sc, AAC_RX_ODBR, ~0);
		break;
	}

	/*
	 * Give the init structure to the controller.
	 */
	if (aac_sync_command(sc, AAC_MONKER_INITSTRUCT, 
	    sc->sc_common_busaddr + offsetof(struct aac_common, ac_init), 0, 0,
	    0, NULL)) {
		printf("%s: error establishing init structure\n",
		    sc->sc_dev.dv_xname);
		error = EIO;
		goto bail_out;
	}

	aac_startup(sc);

	return (0);

 bail_out:
	if (state > 1)
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_common,
		    sizeof *sc->sc_common);
	if (state > 0)
		bus_dmamem_free(sc->sc_dmat, &seg, 1);
	return (error);
}

/*
 * Probe for containers, create disks.
 */
void
aac_startup (sc)
	struct aac_softc *sc;
{
	struct aac_mntinfo mi;
	struct aac_mntinforesponse mir;
	u_int16_t rsize;	
	int i, drv_cyls, drv_hds, drv_secs;

	/* loop over possible containers */
	mi.Command = VM_NameServe;
	mi.MntType = FT_FILESYS;
	for (i = 0; i < AAC_MAX_CONTAINERS; i++) {
		/* request information on this container */
		mi.MntCount = i;
		if (aac_sync_fib(sc, ContainerCommand, 0, &mi, sizeof mi, &mir,
		    &rsize)) {
			printf("%s: error probing container %d",
			    sc->sc_dev.dv_xname, i);
			continue;
		}
		/* check response size */
		if (rsize != sizeof mir) {
			printf("%s: container info response wrong size "
			    "(%d should be %d)",
			    sc->sc_dev.dv_xname, rsize, sizeof mir);
			continue;
		}

		/* 
		 * Check container volume type for validity.  Note
		 * that many of the possible types * may never show
		 * up.
		 */
		if (mir.Status == ST_OK &&
		    mir.MntTable[0].VolType != CT_NONE) {
			AAC_DPRINTF(AAC_D_MISC,
			    ("%d: id %x  name '%.16s'  size %u  type %d", i,
			    mir.MntTable[0].ObjectId,
			    mir.MntTable[0].FileSystemName,
			    mir.MntTable[0].Capacity,
			    mir.MntTable[0].VolType));

			sc->sc_hdr[i].hd_present = 1;
			sc->sc_hdr[i].hd_size = mir.MntTable[0].Capacity;

			/*
			 * Evaluate mapping (sectors per head, heads per cyl)
			 */
			sc->sc_hdr[i].hd_size &= ~AAC_SECS32;
			aac_eval_mapping(sc->sc_hdr[i].hd_size, &drv_cyls,
			    &drv_hds, &drv_secs);
			sc->sc_hdr[i].hd_heads = drv_hds;
			sc->sc_hdr[i].hd_secs = drv_secs;
			/* Round the size */
			sc->sc_hdr[i].hd_size = drv_cyls * drv_hds * drv_secs;

			sc->sc_hdr[i].hd_devtype = mir.MntTable[0].VolType;

			/* XXX Save the name too for use in IDENTIFY later */
		}
	}

	/* mark the controller up */
	sc->sc_state &= ~AAC_STATE_SUSPEND;

	/* enable interrupts now */
	AAC_UNMASK_INTERRUPTS(sc);
}

void
aac_eval_mapping(size, cyls, heads, secs)
	u_int32_t size;
	int *cyls, *heads, *secs;
{
	*cyls = size / AAC_HEADS / AAC_SECS;
	if (*cyls < AAC_MAXCYLS) {
		*heads = AAC_HEADS;
		*secs = AAC_SECS;
	} else {
		/* Too high for 64 * 32 */
		*cyls = size / AAC_MEDHEADS / AAC_MEDSECS;
		if (*cyls < AAC_MAXCYLS) {
			*heads = AAC_MEDHEADS;
			*secs = AAC_MEDSECS;
		} else {
			/* Too high for 127 * 63 */
			*cyls = size / AAC_BIGHEADS / AAC_BIGSECS;
			*heads = AAC_BIGHEADS;
			*secs = AAC_BIGSECS;
		}
	}
}

int
aac_raw_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	AAC_DPRINTF(AAC_D_CMD, ("aac_raw_scsi_cmd "));

	/* XXX Not yet implemented */
	xs->error = XS_DRIVER_STUFFUP;
	return (COMPLETE);
}

int
aac_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct aac_softc *sc = link->adapter_softc;
	u_int8_t target = link->target;
	struct aac_ccb *ccb;
	u_int32_t blockno, blockcnt;
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	aac_lock_t lock;
	int retval = SUCCESSFULLY_QUEUED;

	AAC_DPRINTF(AAC_D_CMD, ("aac_scsi_cmd "));

	xs->error = XS_NOERROR;

	if (target >= AAC_MAX_CONTAINERS || !sc->sc_hdr[target].hd_present ||
	    link->lun != 0) {
		/*
		 * XXX Should be XS_SENSE but that would require setting up a
		 * faked sense too.
		 */
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
	}

	lock = AAC_LOCK(sc);

	/* Don't double enqueue if we came from aac_chain. */
	if (xs != LIST_FIRST(&sc->sc_queue))
		aac_enqueue(sc, xs, 0);

	while ((xs = aac_dequeue(sc))) {
		xs->error = XS_NOERROR;
		ccb = NULL;
		link = xs->sc_link;
		target = link->target;

		switch (xs->cmd->opcode) {
		case TEST_UNIT_READY:
		case REQUEST_SENSE:
		case INQUIRY:
		case MODE_SENSE:
		case START_STOP:
		case READ_CAPACITY:
#if 0
		case VERIFY:
#endif
			if (!aac_internal_cache_cmd(xs)) {
				AAC_UNLOCK(sc, lock);
				return (TRY_AGAIN_LATER);
			}
			xs->flags |= ITSDONE;
			scsi_done(xs);
			goto ready;

		case PREVENT_ALLOW:
			AAC_DPRINTF(AAC_D_CMD, ("PREVENT/ALLOW "));
			/* XXX Not yet implemented */
			xs->error = XS_NOERROR;
			xs->flags |= ITSDONE;
			scsi_done(xs);
			goto ready;

		case SYNCHRONIZE_CACHE:
			AAC_DPRINTF(AAC_D_CMD, ("SYNCHRONIZE_CACHE "));
			/* XXX Not yet implemented */
			xs->error = XS_NOERROR;
			xs->flags |= ITSDONE;
			scsi_done(xs);
			goto ready;

		default:
			AAC_DPRINTF(AAC_D_CMD,
			    ("unknown opc %d ", xs->cmd->opcode));
			/* XXX Not yet implemented */
			xs->error = XS_DRIVER_STUFFUP;
			xs->flags |= ITSDONE;
			scsi_done(xs);
			goto ready;

		case READ_COMMAND:
		case READ_BIG:
		case WRITE_COMMAND:
		case WRITE_BIG:
			AAC_DPRINTF(AAC_D_CMD,
			    ("rw opc %d ", xs->cmd->opcode));

			if (xs->cmd->opcode != SYNCHRONIZE_CACHE) {
				/* A read or write operation. */
				if (xs->cmdlen == 6) {
					rw = (struct scsi_rw *)xs->cmd;
					blockno = _3btol(rw->addr) &
					    (SRW_TOPADDR << 16 | 0xffff);
					blockcnt =
					    rw->length ? rw->length : 0x100;
				} else {
					rwb = (struct scsi_rw_big *)xs->cmd;
					blockno = _4btol(rwb->addr);
					blockcnt = _2btol(rwb->length);
				}
				if (blockno >= sc->sc_hdr[target].hd_size ||
				    blockno + blockcnt >
				    sc->sc_hdr[target].hd_size) {
					printf(
					    "%s: out of bounds %u-%u >= %u\n",
					    sc->sc_dev.dv_xname, blockno,
					    blockcnt,
					    sc->sc_hdr[target].hd_size);
					/*
					 * XXX Should be XS_SENSE but that
					 * would require setting up a faked
					 * sense too.
					 */
					xs->error = XS_DRIVER_STUFFUP;
					xs->flags |= ITSDONE;
					scsi_done(xs);
					goto ready;
				}
			}

			ccb = aac_get_ccb(sc, xs->flags);

			/*
			 * Are we out of commands, something is wrong.
			 * 
			 */
			if (ccb == NULL) {
				printf("%s: no ccb in aac_scsi_cmd",
				    sc->sc_dev.dv_xname);
				xs->error = XS_DRIVER_STUFFUP;
				xs->flags |= ITSDONE;
				scsi_done(xs);
				goto ready;
			}

			ccb->ac_blockno = blockno;
			ccb->ac_blockcnt = blockcnt;
			ccb->ac_xs = xs;
			ccb->ac_timeout = xs->timeout;

			if (xs->cmd->opcode != SYNCHRONIZE_CACHE &&
			    aac_map_command(ccb)) {
				aac_free_ccb(sc, ccb);
				xs->error = XS_DRIVER_STUFFUP;
				xs->flags |= ITSDONE;
				scsi_done(xs);
				goto ready;
			}

			aac_enqueue_ccb(sc, ccb);
			/* XXX what if enqueue did not start a transfer? */
			if (xs->flags & SCSI_POLL) {
#if 0
				if (!aac_wait(sc, ccb, ccb->ac_timeout)) {
					AAC_UNLOCK(sc, lock);
					printf("%s: command timed out\n",
					    sc->sc_dev.dv_xname);
					xs->error = XS_TIMEOUT;
					return (TRY_AGAIN_LATER);
				}
				xs->flags |= ITSDONE;
				scsi_done(xs);
#endif
			}
		}

	ready:
		/*
		 * Don't process the queue if we are polling.
		 */
		if (xs->flags & SCSI_POLL) {
			retval = COMPLETE;
			break;
		}
	}

	AAC_UNLOCK(sc, lock);
	return (retval);
}

void
aac_copy_internal_data(xs, data, size)
	struct scsi_xfer *xs;
	u_int8_t *data;
	size_t size;
{
	size_t copy_cnt;

	AAC_DPRINTF(AAC_D_MISC, ("aac_copy_internal_data "));

	if (!xs->datalen)
		printf("uio move not yet supported\n");
	else {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(data, xs->data, copy_cnt);
	}
}

/* Emulated SCSI operation on cache device */
int
aac_internal_cache_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct aac_softc *sc = link->adapter_softc;
	struct scsi_inquiry_data inq;
	struct scsi_sense_data sd;
	struct {
		struct scsi_mode_header hd;
		struct scsi_blk_desc bd;
		union scsi_disk_pages dp;
	} mpd;
	struct scsi_read_cap_data rcd;
	u_int8_t target = link->target;

	AAC_DPRINTF(AAC_D_CMD, ("aac_internal_cache_cmd "));

	switch (xs->cmd->opcode) {
	case TEST_UNIT_READY:
	case START_STOP:
#if 0
	case VERIFY:
#endif
		AAC_DPRINTF(AAC_D_CMD, ("opc %d tgt %d ", xs->cmd->opcode,
		    target));
		break;

	case REQUEST_SENSE:
		AAC_DPRINTF(AAC_D_CMD, ("REQUEST SENSE tgt %d ", target));
		bzero(&sd, sizeof sd);
		sd.error_code = 0x70;
		sd.segment = 0;
		sd.flags = SKEY_NO_SENSE;
		aac_enc32(sd.info, 0);
		sd.extra_len = 0;
		aac_copy_internal_data(xs, (u_int8_t *)&sd, sizeof sd);
		break;

	case INQUIRY:
		AAC_DPRINTF(AAC_D_CMD, ("INQUIRY tgt %d devtype %x ", target,
		    sc->sc_hdr[target].hd_devtype));
		bzero(&inq, sizeof inq);
		/* XXX How do we detect removable/CD-ROM devices?  */
		inq.device = T_DIRECT;
		inq.dev_qual2 = 0;
		inq.version = 2;
		inq.response_format = 2;
		inq.additional_length = 32;
		strcpy(inq.vendor, "Adaptec");
		sprintf(inq.product, "Container #%02d", target);
		strcpy(inq.revision, "   ");
		aac_copy_internal_data(xs, (u_int8_t *)&inq, sizeof inq);
		break;

	case MODE_SENSE:
		AAC_DPRINTF(AAC_D_CMD, ("MODE SENSE tgt %d ", target));

		bzero(&mpd, sizeof mpd);
		switch (((struct scsi_mode_sense *)xs->cmd)->page) {
		case 4:
			/* scsi_disk.h says this should be 0x16 */
			mpd.dp.rigid_geometry.pg_length = 0x16;
			mpd.hd.data_length = sizeof mpd.hd + sizeof mpd.bd +
			    mpd.dp.rigid_geometry.pg_length;
			mpd.hd.blk_desc_len = sizeof mpd.bd;

			/* XXX */
			mpd.hd.dev_spec = 0;
			_lto3b(AAC_BLOCK_SIZE, mpd.bd.blklen);
			mpd.dp.rigid_geometry.pg_code = 4;
			_lto3b(sc->sc_hdr[target].hd_size /
			    sc->sc_hdr[target].hd_heads /
			    sc->sc_hdr[target].hd_secs,
			    mpd.dp.rigid_geometry.ncyl);
			mpd.dp.rigid_geometry.nheads =
			    sc->sc_hdr[target].hd_heads;
			aac_copy_internal_data(xs, (u_int8_t *)&mpd,
			    sizeof mpd);
			break;

		default:
			printf("%s: mode sense page %d not simulated\n",
			    sc->sc_dev.dv_xname,
			    ((struct scsi_mode_sense *)xs->cmd)->page);
			xs->error = XS_DRIVER_STUFFUP;
			return (0);
		}
		break;

	case READ_CAPACITY:
		AAC_DPRINTF(AAC_D_CMD, ("READ CAPACITY tgt %d ", target));
		bzero(&rcd, sizeof rcd);
		_lto4b(sc->sc_hdr[target].hd_size - 1, rcd.addr);
		_lto4b(AAC_BLOCK_SIZE, rcd.length);
		aac_copy_internal_data(xs, (u_int8_t *)&rcd, sizeof rcd);
		break;

	default:
		printf("aac_internal_cache_cmd got bad opcode: %d\n",
		    xs->cmd->opcode);
		xs->error = XS_DRIVER_STUFFUP;
		return (0);
	}

	xs->error = XS_NOERROR;
	return (1);
}

/*
 * Take an interrupt.
 */
int
aac_intr(arg)
	void *arg;
{
	struct aac_softc *sc = arg;
	u_int16_t reason;
	int claimed = 0;

	AAC_DPRINTF(AAC_D_INTR, ("aac_intr(%p) ", sc));

	reason = AAC_GET_ISTATUS(sc);
	AAC_DPRINTF(AAC_D_INTR, ("istatus 0x%04x ", reason));

	/* controller wants to talk to the log?  XXX should we defer this? */
	if (reason & AAC_DB_PRINTF) {
		if (sc->sc_common->ac_printf[0]) {
			printf("%s: ** %.*s", sc->sc_dev.dv_xname,
			    AAC_PRINTF_BUFSIZE, sc->sc_common->ac_printf);
			sc->sc_common->ac_printf[0] = 0;
		}
		AAC_CLEAR_ISTATUS(sc, AAC_DB_PRINTF);
		AAC_QNOTIFY(sc, AAC_DB_PRINTF);
		claimed = 1;
	}

	/* Controller has a message for us? */
	if (reason & AAC_DB_COMMAND_READY) {
		aac_host_command(sc);
		AAC_CLEAR_ISTATUS(sc, AAC_DB_COMMAND_READY);
		claimed = 1;
	}
    
	/* Controller has a response for us? */
	if (reason & AAC_DB_RESPONSE_READY) {
		aac_host_response(sc);
		AAC_CLEAR_ISTATUS(sc, AAC_DB_RESPONSE_READY);
		claimed = 1;
	}

	/*
	 * Spurious interrupts that we don't use - reset the mask and clear
	 * the interrupts.
	 */
	if (reason & (AAC_DB_SYNC_COMMAND | AAC_DB_COMMAND_NOT_FULL |
            AAC_DB_RESPONSE_NOT_FULL)) {
		AAC_UNMASK_INTERRUPTS(sc);
		AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND |
		    AAC_DB_COMMAND_NOT_FULL | AAC_DB_RESPONSE_NOT_FULL);
		claimed = 1;
	}

	return (claimed);
}

/*
 * Handle notification of one or more FIBs coming from the controller.
 */
void
aac_host_command(struct aac_softc *sc)
{
	struct aac_fib *fib;
	u_int32_t fib_size;

	for (;;) {
		if (aac_dequeue_fib(sc, AAC_HOST_NORM_CMD_QUEUE, &fib_size,
		    &fib))
			break;	/* nothing to do */

		switch(fib->Header.Command) {
		case AifRequest:
#if 0
			aac_handle_aif(sc,
			    (struct aac_aif_command *)&fib->data[0]);
#endif

			break;
		default:
			printf("%s: unknown command from controller\n",
			    sc->sc_dev.dv_xname);
			AAC_PRINT_FIB(sc, fib);
			break;
		}

		/* XXX reply to FIBs requesting responses ?? */
		/* XXX how do we return these FIBs to the controller? */
	}
}

/*
 * Handle notification of one or more FIBs completed by the controller
 */
void
aac_host_response(struct aac_softc *sc)
{
	struct aac_ccb *ccb;
	struct aac_fib *fib;
	u_int32_t fib_size;

	for (;;) {
		/* look for completed FIBs on our queue */
		if (aac_dequeue_fib(sc, AAC_HOST_NORM_RESP_QUEUE, &fib_size,
		    &fib))
			break;	/* nothing to do */
	
		/* get the command, unmap and queue for later processing */
		ccb = (struct aac_ccb *)fib->Header.SenderData;
		if (ccb == NULL) {
			AAC_PRINT_FIB(sc, fib);
		} else {
			timeout_del(&ccb->ac_xs->stimeout);
			aac_unmap_command(ccb);		/* XXX defer? */
			aac_enqueue_completed(ccb);
		}
	}

	/* handle completion processing */
	aac_complete(sc, 0);
}

/*
 * Process completed commands.
 */
void
aac_complete(void *context, int pending)
{
	struct aac_softc *sc = (struct aac_softc *)context;
	struct aac_ccb *ccb;

	/* pull completed commands off the queue */
	for (;;) {
		ccb = aac_dequeue_completed(sc);
		if (ccb == NULL)
			return;
		ccb->ac_flags |= AAC_ACF_COMPLETED;

#if 0
		/* is there a completion handler? */
		if (ccb->ac_complete != NULL) {
			ccb->ac_complete(ccb);
		} else {
			/* assume that someone is sleeping on this command */
			wakeup(ccb);
		}
#else
		aac_bio_complete(ccb);
#endif
	}
}

/*
 * Handle a bio-instigated command that has been completed.
 */
void
aac_bio_complete(struct aac_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ac_xs;
	struct aac_softc *sc = xs->sc_link->adapter_softc;
	struct buf *bp = xs->bp;
	struct aac_blockread_response *brr;
	struct aac_blockwrite_response *bwr;
	AAC_FSAStatus status;

	/* fetch relevant status and then release the command */
	if (bp->b_flags & B_READ) {
		brr = (struct aac_blockread_response *)&ccb->ac_fib->data[0];
		status = brr->Status;
	} else {
		bwr = (struct aac_blockwrite_response *)&ccb->ac_fib->data[0];
		status = bwr->Status;
	}
	aac_free_ccb(sc, ccb);

	/* fix up the bio based on status */
	if (status == ST_OK) {
		bp->b_resid = 0;
	} else {
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
	
		/* XXX be more verbose? */
		printf("%s: I/O error %d (%s)\n", sc->sc_dev.dv_xname,
		    status, AAC_COMMAND_STATUS(status));
	}
	scsi_done(xs);
}

/*
 * Send a synchronous command to the controller and wait for a result.
 */
int
aac_sync_command(sc, command, arg0, arg1, arg2, arg3, sp)
	struct aac_softc *sc;
	u_int32_t command;
	u_int32_t arg0;
	u_int32_t arg1;
	u_int32_t arg2;
	u_int32_t arg3;
	u_int32_t *sp;
{
	int i;
	u_int32_t status;
	aac_lock_t lock = AAC_LOCK(sc);

	/* populate the mailbox */
	AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3);

	/* ensure the sync command doorbell flag is cleared */
	AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

	/* then set it to signal the adapter */
	AAC_QNOTIFY(sc, AAC_DB_SYNC_COMMAND);
	DELAY(AAC_SYNC_DELAY);

	/* spin waiting for the command to complete */
	for (i = 0; i < AAC_IMMEDIATE_TIMEOUT * 1000; i++) {
		if (AAC_GET_ISTATUS(sc) & AAC_DB_SYNC_COMMAND);
			break;
		DELAY(1000);
	}
	if (i == AAC_IMMEDIATE_TIMEOUT * 1000) {
		AAC_UNLOCK(sc, lock);
		return (EIO);
	}

	/* clear the completion flag */
	AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

	/* get the command status */
	status = AAC_GET_MAILBOXSTATUS(sc);
	AAC_UNLOCK(sc, lock);
	if (sp != NULL)
		*sp = status;
	return (0);	/* check command return status? */
}

/*
 * Send a synchronous FIB to the controller and wait for a result.
 */
int
aac_sync_fib(sc, command, xferstate, data, datasize, result, resultsize)
	struct aac_softc *sc;
	u_int32_t command;
	u_int32_t xferstate;
	void *data;
	u_int16_t datasize;
	void *result;
	u_int16_t *resultsize;
{
	struct aac_fib *fib = &sc->sc_common->ac_sync_fib;

	if (datasize > AAC_FIB_DATASIZE)
		return (EINVAL);

	/*
	 * Set up the sync FIB
	 */
	fib->Header.XferState = AAC_FIBSTATE_HOSTOWNED |
	    AAC_FIBSTATE_INITIALISED | AAC_FIBSTATE_EMPTY;
	fib->Header.XferState |= xferstate;
	fib->Header.Command = command;
	fib->Header.StructType = AAC_FIBTYPE_TFIB;
	fib->Header.Size = sizeof fib + datasize;
	fib->Header.SenderSize = sizeof *fib;
	fib->Header.SenderFibAddress = (u_int32_t)fib;
	fib->Header.ReceiverFibAddress =
	    sc->sc_common_busaddr + offsetof(struct aac_common, ac_sync_fib);

	/*
	 * Copy in data.
	 */
	if (data != NULL) {
		bcopy(data, fib->data, datasize);
		fib->Header.XferState |=
		    AAC_FIBSTATE_FROMHOST | AAC_FIBSTATE_NORM;
	}

	/*
	 * Give the FIB to the controller, wait for a response.
	 */
	if (aac_sync_command(sc, AAC_MONKER_SYNCFIB,
	    fib->Header.ReceiverFibAddress, 0, 0, 0, NULL)) {
		return (EIO);
	}

	/* 
	 * Copy out the result
	 */
	if (result != NULL) {
		*resultsize = fib->Header.Size - sizeof fib->Header;
		bcopy(fib->data, result, *resultsize);
	}
	return (0);
}

void
aacminphys(bp)
	struct buf *bp;
{
#if 0
	u_int8_t *buf = bp->b_data;
	paddr_t pa;
	long off;
#endif

	AAC_DPRINTF(AAC_D_MISC, ("aacminphys(0x%x) ", bp));

#if 1
#if 0	/* As this is way more than MAXPHYS it's really not necessary. */
	if (bp->b_bcount > ((AAC_MAXOFFSETS - 1) * PAGE_SIZE))
		bp->b_bcount = ((AAC_MAXOFFSETS - 1) * PAGE_SIZE);
#endif
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

/*
 * Read the current firmware status word.
 */
int
aac_sa_get_fwstatus(sc)
	struct aac_softc *sc;
{
	return (AAC_GETREG4(sc, AAC_SA_FWSTATUS));
}

int
aac_rx_get_fwstatus(sc)
	struct aac_softc *sc;
{
	return (AAC_GETREG4(sc, AAC_RX_FWSTATUS));
}

/*
 * Notify the controller of a change in a given queue
 */

void
aac_sa_qnotify(sc, qbit)
	struct aac_softc *sc;
	int qbit;
{
	AAC_SETREG2(sc, AAC_SA_DOORBELL1_SET, qbit);
}

void
aac_rx_qnotify(sc, qbit)
	struct aac_softc *sc;
	int qbit;
{
	AAC_SETREG4(sc, AAC_RX_IDBR, qbit);
}

/*
 * Get the interrupt reason bits
 */
int
aac_sa_get_istatus(sc)
	struct aac_softc *sc;
{
	return (AAC_GETREG2(sc, AAC_SA_DOORBELL0));
}

int
aac_rx_get_istatus(sc)
	struct aac_softc *sc;
{
	return (AAC_GETREG4(sc, AAC_RX_ODBR));
}

/*
 * Clear some interrupt reason bits
 */
void
aac_sa_clear_istatus(sc, mask)
	struct aac_softc *sc;
	int mask;
{
	AAC_SETREG2(sc, AAC_SA_DOORBELL0_CLEAR, mask);
}

void
aac_rx_clear_istatus(sc, mask)
	struct aac_softc *sc;
	int mask;
{
	AAC_SETREG4(sc, AAC_RX_ODBR, mask);
}

/*
 * Populate the mailbox and set the command word
 */
void
aac_sa_set_mailbox(sc, command, arg0, arg1, arg2, arg3)
	struct aac_softc *sc;
	u_int32_t command;
	u_int32_t arg0;
	u_int32_t arg1;
	u_int32_t arg2;
	u_int32_t arg3;
{
	AAC_SETREG4(sc, AAC_SA_MAILBOX, command);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 16, arg3);
}

void
aac_rx_set_mailbox(sc, command, arg0, arg1, arg2, arg3)
	struct aac_softc *sc;
	u_int32_t command;
	u_int32_t arg0;
	u_int32_t arg1;
	u_int32_t arg2;
	u_int32_t arg3;
{
	AAC_SETREG4(sc, AAC_RX_MAILBOX, command);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 16, arg3);
}

/*
 * Fetch the immediate command status word
 */
int
aac_sa_get_mailboxstatus(sc)
	struct aac_softc *sc;
{
	return (AAC_GETREG4(sc, AAC_SA_MAILBOX));
}

int
aac_rx_get_mailboxstatus(sc)
	struct aac_softc *sc;
{
	return (AAC_GETREG4(sc, AAC_RX_MAILBOX));
}

/*
 * Set/clear interrupt masks
 */
void
aac_sa_set_interrupts(sc, enable)
	struct aac_softc *sc;
	int enable;
{
	if (enable)
		AAC_SETREG2((sc), AAC_SA_MASK0_CLEAR, AAC_DB_INTERRUPTS);
	else
		AAC_SETREG2((sc), AAC_SA_MASK0_SET, ~0);
}

void
aac_rx_set_interrupts(sc, enable)
	struct aac_softc *sc;
	int enable;
{
	if (enable)
		AAC_SETREG4(sc, AAC_RX_OIMR, ~AAC_DB_INTERRUPTS);
	else
		AAC_SETREG4(sc, AAC_RX_OIMR, ~0);
}

struct aac_ccb *
aac_get_ccb(sc, flags)
	struct aac_softc *sc;
	int flags;
{
	struct aac_ccb *ccb;
	aac_lock_t lock;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_get_ccb(%p, 0x%x) ", sc, flags));

	lock = AAC_LOCK(sc);

	for (;;) {
		ccb = TAILQ_FIRST(&sc->sc_free_ccb);
		if (ccb != NULL)
			break;
		if (flags & SCSI_NOSLEEP)
			goto bail_out;
		tsleep(&sc->sc_free_ccb, PRIBIO, "aac_ccb", 0);
	}

	TAILQ_REMOVE(&sc->sc_free_ccb, ccb, ac_chain);

	/* initialise the command/FIB */
	ccb->ac_sgtable = NULL;
	ccb->ac_flags = 0;
	ccb->ac_fib->Header.XferState = AAC_FIBSTATE_EMPTY;
	ccb->ac_fib->Header.StructType = AAC_FIBTYPE_TFIB;
	ccb->ac_fib->Header.Flags = 0;
	ccb->ac_fib->Header.SenderSize = sizeof(struct aac_fib);

	/* 
	 * These are duplicated in aac_start to cover the case where an
	 * intermediate stage may have destroyed them.  They're left
	 * initialised here for debugging purposes only.
	 */
	ccb->ac_fib->Header.SenderFibAddress = (u_int32_t)ccb->ac_fib;
	ccb->ac_fib->Header.ReceiverFibAddress = ccb->ac_fibphys;

 bail_out:
	AAC_UNLOCK(sc, lock);
	return (ccb);
}

void
aac_free_ccb(sc, ccb)
	struct aac_softc *sc;
	struct aac_ccb *ccb;
{
	aac_lock_t lock;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_free_ccb(%p, %p) ", sc, ccb));

	lock = AAC_LOCK(sc);

	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, ac_chain);

	/* If the free list was empty, wake up potential waiters. */
	if (TAILQ_NEXT(ccb, ac_chain) == NULL)
		wakeup(&sc->sc_free_ccb);

	AAC_UNLOCK(sc, lock);
}

void
aac_enqueue_ccb(sc, ccb)
	struct aac_softc *sc;
	struct aac_ccb *ccb;
{
	AAC_DPRINTF(AAC_D_QUEUE, ("aac_enqueue_ccb(%p, %p) ", sc, ccb));

	timeout_set(&ccb->ac_xs->stimeout, aac_timeout, ccb);
	TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, ac_chain);
	aac_start_ccbs(sc);
}

void
aac_start_ccbs(sc)
	struct aac_softc *sc;
{
	struct aac_ccb *ccb;
	struct scsi_xfer *xs;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_start_ccbs(%p) ", sc));

	while ((ccb = TAILQ_FIRST(&sc->sc_ccbq)) != NULL) {

		xs = ccb->ac_xs;
		if (ccb->ac_flags & AAC_ACF_WATCHDOG)
			timeout_del(&xs->stimeout);

		if (aac_exec_ccb(ccb) == 0) {
			ccb->ac_flags |= AAC_ACF_WATCHDOG;
			timeout_set(&ccb->ac_xs->stimeout, aac_watchdog, ccb);
			timeout_add(&xs->stimeout,
			    (AAC_WATCH_TIMEOUT * hz) / 1000);
			break;
		}
		TAILQ_REMOVE(&sc->sc_ccbq, ccb, ac_chain);

		if ((xs->flags & SCSI_POLL) == 0) {
			timeout_set(&ccb->ac_xs->stimeout, aac_timeout, ccb);
			timeout_add(&xs->stimeout,
			    (ccb->ac_timeout * hz) / 1000);
		}
	}
}

int
aac_exec_ccb(ccb)
	struct aac_ccb *ccb;
{
	struct scsi_xfer *xs = ccb->ac_xs;
	struct scsi_link *link = xs->sc_link;
	u_int8_t target = link->target;
	int i;
	struct aac_fib *fib;
	struct aac_blockread *br;
	struct aac_blockwrite *bw;
	bus_dmamap_t xfer;

	AAC_DPRINTF(AAC_D_CMD, ("aac_exec_ccb(%p, %p) ", xs, ccb));

	/* build the FIB */
	fib = ccb->ac_fib;
	fib->Header.XferState = AAC_FIBSTATE_HOSTOWNED |
	    AAC_FIBSTATE_INITIALISED | AAC_FIBSTATE_FROMHOST |
	    AAC_FIBSTATE_REXPECTED | AAC_FIBSTATE_NORM;
	fib->Header.Command = ContainerCommand;
	fib->Header.Size = sizeof(struct aac_fib_header);

	switch (xs->cmd->opcode) {
	case PREVENT_ALLOW:
	case SYNCHRONIZE_CACHE:
		if (xs->cmd->opcode == PREVENT_ALLOW) {
			/* XXX PREVENT_ALLOW support goes here */
		} else {
			AAC_DPRINTF(AAC_D_CMD,
			    ("SYNCHRONIZE CACHE tgt %d ", target));
		}
		break;

	case WRITE_COMMAND:
	case WRITE_BIG:
		bw = (struct aac_blockwrite *)&fib->data[0];
		bw->Command = VM_CtBlockWrite;
		bw->ContainerId = target;
		bw->BlockNumber = ccb->ac_blockno;
		bw->ByteCount = ccb->ac_blockcnt * DEV_BSIZE;
		bw->Stable = CUNSTABLE;	/* XXX what's appropriate here? */
		fib->Header.Size += sizeof(struct aac_blockwrite);
		ccb->ac_sgtable = &bw->SgMap;
		break;

	case READ_COMMAND:
	case READ_BIG:
		br = (struct aac_blockread *)&fib->data[0];
		br->Command = VM_CtBlockRead;
		br->ContainerId = target;
		br->BlockNumber = ccb->ac_blockno;
		br->ByteCount = ccb->ac_blockcnt * DEV_BSIZE;
		fib->Header.Size += sizeof(struct aac_blockread);
		ccb->ac_sgtable = &br->SgMap;
		break;
	}

	if (xs->cmd->opcode != PREVENT_ALLOW &&
	    xs->cmd->opcode != SYNCHRONIZE_CACHE) {
		xfer = ccb->ac_dmamap_xfer;
		ccb->ac_sgtable->SgCount = xfer->dm_nsegs;
		for (i = 0; i < xfer->dm_nsegs; i++) {
			ccb->ac_sgtable->SgEntry[i].SgAddress =
			    xfer->dm_segs[i].ds_addr;
			ccb->ac_sgtable->SgEntry[i].SgByteCount =
			    xfer->dm_segs[i].ds_len;
			AAC_DPRINTF(AAC_D_IO,
			    ("#%d va %p pa %p len %x\n", i, buf,
			    xfer->dm_segs[i].ds_addr,
			    xfer->dm_segs[i].ds_len));
		}

		/* update the FIB size for the s/g count */
		fib->Header.Size += xfer->dm_nsegs *
		    sizeof(struct aac_sg_entry);
	}

	aac_start(ccb);

	xs->error = XS_NOERROR;
	xs->resid = 0;
	return (1);
}

/********************************************************************************
 * Deliver a command to the controller; allocate controller resources at the
 * last moment when possible.
 */
int
aac_start(struct aac_ccb *ccb)
{
	struct aac_softc *sc = ccb->ac_xs->sc_link->adapter_softc;

#if 0
	/* get the command mapped */
	aac_map_command(ccb);
#endif

	/* fix up the address values */
	ccb->ac_fib->Header.SenderFibAddress = (u_int32_t)ccb->ac_fib;
	ccb->ac_fib->Header.ReceiverFibAddress = ccb->ac_fibphys;

	/* save a pointer to the command for speedy reverse-lookup */
	ccb->ac_fib->Header.SenderData = (u_int32_t)ccb; /* XXX ack, sizing */

	/* put the FIB on the outbound queue */
	if (aac_enqueue_fib(sc, AAC_ADAP_NORM_CMD_QUEUE,
	    ccb->ac_fib->Header.Size, ccb->ac_fib->Header.ReceiverFibAddress))
		return (EBUSY);

	return (0);
}

/*
 * Map a command into controller-visible space.
 */
int
aac_map_command(struct aac_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ac_xs;
	struct aac_softc *sc = xs->sc_link->adapter_softc;
	int error;

#if 0
	/* don't map more than once */
	if (ccb->ac_flags & AAC_CMD_MAPPED)
		return;
#endif

	if (xs->datalen != 0) {
		error = bus_dmamap_load(sc->sc_dmat, ccb->ac_dmamap_xfer,
		    xs->data, xs->datalen, NULL,
		    (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT :
		    BUS_DMA_WAITOK);
		if (error) {
			printf("%s: aac_scsi_cmd: ", sc->sc_dev.dv_xname);
			if (error == EFBIG)
				printf("more than %d dma segs\n",
				    AAC_MAXSGENTRIES);
			else
				printf("error %d loading dma map\n", error);
			return (error);
		}

		bus_dmamap_sync(sc->sc_dmat, ccb->ac_dmamap_xfer, 0,
		    ccb->ac_dmamap_xfer->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);
	}

#if 0
	ccb->ac_flags |= AAC_CMD_MAPPED;
#endif
	return (0);
}

/*
 * Unmap a command from controller-visible space.
 */
void
aac_unmap_command(struct aac_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ac_xs;
	struct aac_softc *sc = xs->sc_link->adapter_softc;

#if 0
	if (!(ccb->ac_flags & AAC_CMD_MAPPED))
		return;
#endif

	if (xs->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ac_dmamap_xfer, 0,
		    ccb->ac_dmamap_xfer->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, ccb->ac_dmamap_xfer);
	}
#if 0
	ccb->ac_flags &= ~AAC_CMD_MAPPED;
#endif
}

void
aac_timeout(arg)
	void *arg;
{
	struct aac_ccb *ccb = arg;
	struct scsi_link *link = ccb->ac_xs->sc_link;
	struct aac_softc *sc = link->adapter_softc;
	aac_lock_t lock;

	sc_print_addr(link);
	printf("timed out\n");

	/* XXX Test for multiple timeouts */

	ccb->ac_xs->error = XS_TIMEOUT;
	lock = AAC_LOCK(sc);
	aac_enqueue_ccb(sc, ccb);
	AAC_UNLOCK(sc, lock);
}

void
aac_watchdog(arg)
	void *arg;
{
	struct aac_ccb *ccb = arg;
	struct scsi_link *link = ccb->ac_xs->sc_link;
	struct aac_softc *sc = link->adapter_softc;
	aac_lock_t lock;

	lock = AAC_LOCK(sc);
	ccb->ac_flags &= ~AAC_ACF_WATCHDOG;
	aac_start_ccbs(sc);
	AAC_UNLOCK(sc, lock);
}
/*
 * Insert a command into the driver queue, either at the front or at the tail.
 * It's ok to overload the freelist link as these structures are never on
 * the freelist at this time.
 */
void
aac_enqueue(sc, xs, infront)
	struct aac_softc *sc;
	struct scsi_xfer *xs;
	int infront;
{
	if (infront || LIST_FIRST(&sc->sc_queue) == NULL) {
		if (LIST_FIRST(&sc->sc_queue) == NULL)
			sc->sc_queuelast = xs;
		LIST_INSERT_HEAD(&sc->sc_queue, xs, free_list);
		return;
	}
	LIST_INSERT_AFTER(sc->sc_queuelast, xs, free_list);
	sc->sc_queuelast = xs;
}

/*
 * Pull a command off the front of the driver queue.
 */
struct scsi_xfer *
aac_dequeue(sc)
	struct aac_softc *sc;
{
	struct scsi_xfer *xs;

	xs = LIST_FIRST(&sc->sc_queue);
	if (xs == NULL)
		return (NULL);
	LIST_REMOVE(xs, free_list);

	if (LIST_FIRST(&sc->sc_queue) == NULL)
		sc->sc_queuelast = NULL;

	return (xs);
}

/********************************************************************************
 * Adapter-space FIB queue manipulation
 *
 * Note that the queue implementation here is a little funky; neither the PI or
 * CI will ever be zero.  This behaviour is a controller feature.
 */
static struct {
	int size;
	int notify;
} aac_qinfo[] = {
	{ AAC_HOST_NORM_CMD_ENTRIES, AAC_DB_COMMAND_NOT_FULL },
	{ AAC_HOST_HIGH_CMD_ENTRIES, 0 },
	{ AAC_ADAP_NORM_CMD_ENTRIES, AAC_DB_COMMAND_READY },
	{ AAC_ADAP_HIGH_CMD_ENTRIES, 0 },
	{ AAC_HOST_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_NOT_FULL },
	{ AAC_HOST_HIGH_RESP_ENTRIES, 0 },
	{ AAC_ADAP_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_READY },
	{ AAC_ADAP_HIGH_RESP_ENTRIES, 0 }
};

/*
 * Atomically insert an entry into the nominated queue, returns 0 on success
 * or EBUSY if the queue is full.
 *
 * XXX Note that it would be more efficient to defer notifying the controller
 * in the case where we may be inserting several entries in rapid succession,
 * but implementing this usefully is difficult.
 */
int
aac_enqueue_fib(struct aac_softc *sc, int queue, u_int32_t fib_size,
    u_int32_t fib_addr)
{
	u_int32_t pi, ci;
	int error;
	aac_lock_t lock;

	lock = AAC_LOCK(sc);

	/* get the producer/consumer indices */
	pi = sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
	ci = sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

	/* wrap the queue? */
	if (pi >= aac_qinfo[queue].size)
		pi = 0;

	/* check for queue full */
	if ((pi + 1) == ci) {
		error = EBUSY;
		goto out;
	}

	/* populate queue entry */
	(sc->sc_qentries[queue] + pi)->aq_fib_size = fib_size;
	(sc->sc_qentries[queue] + pi)->aq_fib_addr = fib_addr;

	/* update producer index */
	sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX] = pi + 1;

	/* notify the adapter if we know how */
	if (aac_qinfo[queue].notify != 0)
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

	error = 0;

out:
	AAC_UNLOCK(sc, lock);
	return (error);
}

/*
 * Atomically remove one entry from the nominated queue, returns 0 on success
 * or ENOENT if the queue is empty.
 */
int
aac_dequeue_fib(struct aac_softc *sc, int queue, u_int32_t *fib_size,
    struct aac_fib **fib_addr)
{
	u_int32_t pi, ci;
	int error;
	aac_lock_t lock;

	lock = AAC_LOCK(sc);

	/* get the producer/consumer indices */
	pi = sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
	ci = sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

	/* check for queue empty */
	if (ci == pi) {
		error = ENOENT;
		goto out;
	}
    
	/* wrap the queue? */
	if (ci >= aac_qinfo[queue].size)
		ci = 0;

	/* fetch the entry */
	*fib_size = (sc->sc_qentries[queue] + ci)->aq_fib_size;
	*fib_addr =
	    (struct aac_fib *)(sc->sc_qentries[queue] + ci)->aq_fib_addr;

	/* update consumer index */
	sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX] = ci + 1;

	/* if we have made the queue un-full, notify the adapter */
	if (((pi + 1) == ci) && (aac_qinfo[queue].notify != 0))
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);
	error = 0;

out:
	AAC_UNLOCK(sc, lock);
	return (error);
}

#ifdef AAC_DEBUG
/*
 * Print a FIB
 */
void
aac_print_fib(struct aac_softc *sc, struct aac_fib *fib, char *caller)
{
	printf("%s: FIB @ %p\n", caller, fib);
	printf("  XferState %b\n", fib->Header.XferState, "\20"
	    "\1HOSTOWNED"
	    "\2ADAPTEROWNED"
	    "\3INITIALISED"
	    "\4EMPTY"
	    "\5FROMPOOL"
	    "\6FROMHOST"
	    "\7FROMADAP"
	    "\10REXPECTED"
	    "\11RNOTEXPECTED"
	    "\12DONEADAP"
	    "\13DONEHOST"
	    "\14HIGH"
	    "\15NORM"
	    "\16ASYNC"
	    "\17PAGEFILEIO"
	    "\20SHUTDOWN"
	    "\21LAZYWRITE"
	    "\22ADAPMICROFIB"
	    "\23BIOSFIB"
	    "\24FAST_RESPONSE"
	    "\25APIFIB\n");
	printf("  Command         %d\n", fib->Header.Command);
	printf("  StructType      %d\n", fib->Header.StructType);
	printf("  Flags           0x%x\n", fib->Header.Flags);
	printf("  Size            %d\n", fib->Header.Size);
	printf("  SenderSize      %d\n", fib->Header.SenderSize);
	printf("  SenderAddress   0x%x\n", fib->Header.SenderFibAddress);
	printf("  ReceiverAddress 0x%x\n", fib->Header.ReceiverFibAddress);
	printf("  SenderData      0x%x\n", fib->Header.SenderData);
	switch(fib->Header.Command) {
	case ContainerCommand: {
		struct aac_blockread *br = (struct aac_blockread *)fib->data;
		struct aac_blockwrite *bw = (struct aac_blockwrite *)fib->data;
		struct aac_sg_table *sg = NULL;
		int i;

		if (br->Command == VM_CtBlockRead) {
			printf("  BlockRead: container %d  0x%x/%d\n", 
			    br->ContainerId, br->BlockNumber, br->ByteCount);
			    sg = &br->SgMap;
		}
		if (bw->Command == VM_CtBlockWrite) {
			printf("  BlockWrite: container %d  0x%x/%d (%s)\n", 
			    bw->ContainerId, bw->BlockNumber, bw->ByteCount,
			    bw->Stable == CSTABLE ? "stable" : "unstable");
			sg = &bw->SgMap;
		}
		if (sg != NULL) {
			printf("  %d s/g entries\n", sg->SgCount);
			for (i = 0; i < sg->SgCount; i++)
				printf("  0x%08x/%d\n",
				       sg->SgEntry[i].SgAddress,
				       sg->SgEntry[i].SgByteCount);
		}
		break;
	}
	default:
		printf("   %16D\n", fib->data, " ");
		printf("   %16D\n", fib->data + 16, " ");
	break;
	}
}
#endif
