/* $OpenBSD $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
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
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>

#include <sys/mount.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <miscfs/specfs/specdev.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

#ifdef SR_DEBUG
uint32_t	sr_debug = 0
		    /* | SR_D_CMD */
		    | SR_D_MISC
		    /* | SR_D_INTR */
		    /* | SR_D_IOCTL*/
		    /* | SR_D_CCB */
		    /* | SR_D_WU  */
		    /* | SR_D_META*/
		    | SR_D_DIS
		;
#endif

struct cfattach softraid_ca = {
	sizeof(struct sr_softc),
	sr_probe,
	sr_attach,
	sr_detach,
	sr_activate
};

struct cfdriver softraid_cd = {
	NULL, "softraid", DV_DULL
};

int			sr_scsi_cmd(struct scsi_xfer *);
void			sr_minphys(struct buf *bp);
void			sr_copy_internal_data(struct scsi_xfer *,
			    void *, size_t);
int			sr_scsi_ioctl(struct scsi_link *, u_long,
			    caddr_t, int, struct proc *);
int			sr_ioctl(struct device *, u_long, caddr_t);
int			sr_ioctl_inq(struct sr_softc *, struct bioc_inq *);
int			sr_ioctl_vol(struct sr_softc *, struct bioc_vol *);
int			sr_ioctl_disk(struct sr_softc *, struct bioc_disk *);
int			sr_ioctl_createraid(struct sr_softc *,
			    struct bioc_createraid *);
int			sr_parse_chunks(struct sr_softc *, char *,
			    struct sr_chunk_head *);
void			sr_unwind_chunks(struct sr_softc *,
			    struct sr_chunk_head *);

/* work units & ccbs */
int			sr_alloc_ccb(struct sr_discipline *);
void			sr_free_ccb(struct sr_discipline *);
struct sr_ccb 		*sr_get_ccb(struct sr_discipline *);
void			sr_put_ccb(struct sr_ccb *);
int			sr_alloc_wu(struct sr_discipline *);
void			sr_free_wu(struct sr_discipline *);
struct sr_workunit 	*sr_get_wu(struct sr_discipline *);
void			sr_put_wu(struct sr_workunit *);

/* discipline functions */
int			sr_raid1_alloc_resources(struct sr_discipline *);
int			sr_raid1_free_resources(struct sr_discipline *);

int			sr_raid1_rw(struct sr_workunit *);
int			sr_raid1_inquiry(struct sr_workunit *);
int			sr_raid1_read_cap(struct sr_workunit *);
int			sr_raid1_tur(struct sr_workunit *);
int			sr_raid1_request_sense( struct sr_workunit *);
int			sr_raid1_start_stop(struct sr_workunit *);
int			sr_raid1_sync(struct sr_workunit *);
void			sr_raid1_intr(struct buf *);

struct scsi_adapter sr_switch = {
	sr_scsi_cmd, sr_minphys, NULL, NULL, sr_scsi_ioctl
};

struct scsi_device sr_dev = {
	NULL, NULL, NULL, NULL
};

int
sr_probe(struct device *parent, void *match, void *aux)
{
	struct sr_attach_args	*maa = aux;
	struct cfdata		*cf = match;

	if (strcmp(maa->maa_name, cf->cf_driver->cd_name))
		return (0);

	return (1);
}

int
sr_detach(struct device *self, int flags)
{
	return (0);
}

int
sr_activate(struct device *self, enum devact act)
{
	return (1);
}

void
sr_attach(struct device *parent, struct device *self, void *aux)
{
	struct sr_softc		*sc = (void *)self;

	DNPRINTF(SR_D_MISC, "\n%s: sr_attach", DEVNAME(sc));

	rw_init(&sc->sc_lock, "sr_lock");

	if (bio_register(&sc->sc_dev, sr_ioctl) != 0)
		printf("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = sr_ioctl;

	printf("\n");
}

void
sr_minphys(struct buf *bp)
{
	DNPRINTF(SR_D_MISC, "sr_minphys: %d\n", bp->b_bcount);

	/* XXX currently using SR_MAXFER = MAXPHYS */
	if (bp->b_bcount > SR_MAXFER)
		bp->b_bcount = SR_MAXFER;
	minphys(bp);
}

void
sr_copy_internal_data(struct scsi_xfer *xs, void *v, size_t size)
{
	size_t 			copy_cnt;

	DNPRINTF(SR_D_MISC, "sr_copy_internal_data xs: %p size: %d\n",
	    xs, size);

	if (xs->datalen) {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(v, xs->data, copy_cnt);
	}
}

int
sr_alloc_ccb(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;
	int			i;

	if (!sd)
		return (1);

	DNPRINTF(SR_D_CCB, "%s: sr_alloc_ccb\n", DEVNAME(sd->sd_sc));

	if (sd->sd_ccb)
		return (1);

	sd->sd_ccb = malloc(sizeof(struct sr_ccb) * 
	    sd->sd_max_wu * sd->sd_max_ccb_per_wu, M_DEVBUF, M_WAITOK);
	memset(sd->sd_ccb, 0, sizeof(struct sr_ccb) *
	    sd->sd_max_wu * sd->sd_max_ccb_per_wu);
	TAILQ_INIT(&sd->sd_ccb_freeq);
	for (i = 0; i < sd->sd_max_wu * sd->sd_max_ccb_per_wu; i++) {
		ccb = &sd->sd_ccb[i];
		ccb->ccb_dis = sd;
		sr_put_ccb(ccb);
	}

	DNPRINTF(SR_D_CCB, "%s: sr_alloc_ccb ccb: %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_max_wu * sd->sd_max_ccb_per_wu);

	return (0);
}

void
sr_free_ccb(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;

	if (!sd)
		return;

	DNPRINTF(SR_D_CCB, "%s: sr_free_ccb %p\n", DEVNAME(sd->sd_sc), sd);

	while ((ccb = TAILQ_FIRST(&sd->sd_ccb_freeq)) != NULL)
		TAILQ_REMOVE(&sd->sd_ccb_freeq, ccb, ccb_link);

	if (sd->sd_ccb)
		free(sd->sd_ccb, M_DEVBUF);
}

struct sr_ccb *
sr_get_ccb(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;
	int			s;

	s = splbio();
	ccb = TAILQ_FIRST(&sd->sd_ccb_freeq);
	if (ccb) {
		TAILQ_REMOVE(&sd->sd_ccb_freeq, ccb, ccb_link);
		ccb->ccb_state = SR_CCB_INPROGRESS;
	}
	splx(s);

	DNPRINTF(SR_D_CCB, "%s: sr_get_ccb: %p\n", DEVNAME(sd->sd_sc),
	    ccb);

	return (ccb);
}

void
sr_put_ccb(struct sr_ccb *ccb)
{
	struct sr_discipline	*sd = ccb->ccb_dis;
	int			s;

	DNPRINTF(SR_D_CCB, "%s: sr_put_ccb: %p\n", DEVNAME(sd->sd_sc),
	    ccb);

	s = splbio();

	ccb->ccb_wu = NULL;
	ccb->ccb_state = SR_CCB_FREE;

	TAILQ_INSERT_TAIL(&sd->sd_ccb_freeq, ccb, ccb_link);
	splx(s);
}

int
sr_alloc_wu(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;
	int			i, no_wu;

	if (!sd)
		return (1);

	DNPRINTF(SR_D_WU, "%s: sr_alloc_wu %p %d\n", DEVNAME(sd->sd_sc),
	    sd, sd->sd_max_wu);

	if (sd->sd_wu)
		return (1);

	no_wu = sd->sd_max_wu;
	sd->sd_wu = malloc(sizeof(struct sr_workunit) * no_wu,
	    M_DEVBUF, M_WAITOK);
	memset(sd->sd_wu, 0, sizeof(struct sr_workunit) * no_wu);
	TAILQ_INIT(&sd->sd_wu_freeq);
	for (i = 0; i < no_wu; i++) {
		wu = &sd->sd_wu[i];
		wu->swu_dis = sd;
		sr_put_wu(wu);
	}

	return (0);
}

void
sr_free_wu(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;

	if (!sd)
		return;

	DNPRINTF(SR_D_WU, "%s: sr_free_wu %p\n", DEVNAME(sd->sd_sc), sd);

	while ((wu = TAILQ_FIRST(&sd->sd_wu_freeq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);

	if (sd->sd_wu)
		free(sd->sd_wu, M_DEVBUF);
}

void
sr_put_wu(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	int			s;

	DNPRINTF(SR_D_WU, "%s: sr_put_wu: %p\n", DEVNAME(sd->sd_sc), wu);

	s = splbio();

	wu->swu_xs = NULL;
	wu->swu_state = SR_WU_FREE;
	wu->swu_ios_complete = 0;
	wu->swu_io_count = 0;
	wu->swu_ios = NULL;

	TAILQ_INSERT_TAIL(&sd->sd_wu_freeq, wu, swu_link);
	splx(s);
}

struct sr_workunit *
sr_get_wu(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;
	int			s;

	s = splbio();
	wu = TAILQ_FIRST(&sd->sd_wu_freeq);
	if (wu) {
		TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);
		wu->swu_state = SR_WU_INPROGRESS;
	}
	splx(s);

	DNPRINTF(SR_D_WU, "%s: sr_get_wu: %p\n", DEVNAME(sd->sd_sc), wu);

	return (wu);
}

int
sr_scsi_cmd(struct scsi_xfer *xs)
{
	int			s;
	struct scsi_link	*link = xs->sc_link;
	struct sr_softc		*sc = link->adapter_softc;
	struct sr_workunit	*wu;
	struct sr_discipline	*sd;

	DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: scsibus: %d xs: %p "
	    "flags: %#x\n", DEVNAME(sc), link->scsibus, xs, xs->flags);

	sd = sc->sc_dis[link->scsibus];
	if (sd == NULL) {
		s = splhigh();
		sd = sc->sc_attach_dis;
		splx(s);

		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: attaching %p\n",
		    DEVNAME(sc), sd);
		if (sd == NULL) {
			wu = NULL;
			printf("%s: sr_scsi_cmd NULL discipline\n",
			    DEVNAME(sc));
			goto stuffup;
		}
	}

	if ((wu = sr_get_wu(sd)) == NULL) {
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd no wu\n", DEVNAME(sc));
		return (TRY_AGAIN_LATER);
	}

	xs->error = XS_NOERROR;
	wu->swu_xs = xs;

	/* if we have sense data let the midlayer know */
	if (sd->sd_scsi_sense.flags != SKEY_NO_SENSE &&
	    (!(xs->cmd->opcode == REQUEST_SENSE ||
	    xs->cmd->opcode == INQUIRY))) {
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd opcode 0x%02x sense "
		    "0x%02x 0x%02x\n", DEVNAME(sc), xs->cmd->opcode,
		    sd->sd_scsi_sense.add_sense_code,
		    sd->sd_scsi_sense.add_sense_code_qual);
		xs->error = XS_SENSE;
		xs->flags |= ITSDONE;
		goto complete;
	}

	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: READ/WRITE %02x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		if (sd->sd_scsi_rw(wu))
			goto stuffup;
		break;

	case SYNCHRONIZE_CACHE:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: SYNCHRONIZE_CACHE\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_sync(wu))
			goto stuffup;
		goto complete;

	case TEST_UNIT_READY:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: TEST_UNIT_READY\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_tur(wu))
			goto stuffup;
		goto complete;

	case START_STOP:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: START_STOP\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_start_stop(wu))
			goto stuffup;
		goto complete;

	case INQUIRY:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: INQUIRY\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_inquiry(wu))
			goto stuffup;
		goto complete;

	case READ_CAPACITY:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd READ CAPACITY\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_read_cap(wu))
			goto stuffup;
		goto complete;

	case REQUEST_SENSE:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd REQUEST SENSE\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_req_sense(wu))
			goto stuffup;
		goto complete;

	default:
		DNPRINTF(SR_D_CMD, "%s: unsupported scsi command %x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		/* XXX might need to add generic function to handle others */
		goto stuffup;
	}

	return (SUCCESSFULLY_QUEUED);
stuffup:
	xs->error = XS_DRIVER_STUFFUP;
	xs->flags |= ITSDONE;
complete:
	s = splbio();
	scsi_done(xs);
	splx(s);
	if (wu)
		sr_put_wu(wu);
	return (COMPLETE);
}
int
sr_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	DNPRINTF(SR_D_IOCTL, "%s: sr_scsi_ioctl cmd: %#x\n",
	    DEVNAME((struct sr_softc *)link->adapter_softc), cmd);

	return (ENOTTY);
}

int
sr_ioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct sr_softc		*sc = (struct sr_softc *)dev;
	int			rv = 0;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl ", DEVNAME(sc));

	rw_enter_write(&sc->sc_lock);

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(SR_D_IOCTL, "inq\n");
		rv = sr_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		DNPRINTF(SR_D_IOCTL, "vol\n");
		rv = sr_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		DNPRINTF(SR_D_IOCTL, "disk\n");
		rv = sr_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		DNPRINTF(SR_D_IOCTL, "alarm\n");
		/*rv = sr_ioctl_alarm(sc, (struct bioc_alarm *)addr); */
		break;

	case BIOCBLINK:
		DNPRINTF(SR_D_IOCTL, "blink\n");
		/*rv = sr_ioctl_blink(sc, (struct bioc_blink *)addr); */
		break;

	case BIOCSETSTATE:
		DNPRINTF(SR_D_IOCTL, "setstate\n");
		/*rv = sr_ioctl_setstate(sc, (struct bioc_setstate *)addr); */
		break;

	case BIOCCREATERAID:
		DNPRINTF(SR_D_IOCTL, "createraid\n");
		rv = sr_ioctl_createraid(sc, (struct bioc_createraid *)addr);
		break;

	default:
		DNPRINTF(SR_D_IOCTL, "invalid ioctl\n");
		rv = EINVAL;
	}

	rw_exit_write(&sc->sc_lock);

	return (rv);
}

int
sr_ioctl_inq(struct sr_softc *sc, struct bioc_inq *bi)
{
	int			i, vol, disk;

	for (i = 0, vol = 0, disk = 0; i < SR_MAXSCSIBUS; i++)
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i]) {
			vol++;
			disk += sc->sc_dis[i]->sd_vol.sv_meta.svm_no_chunk;
		}

	strlcpy(bi->bi_dev, sc->sc_dev.dv_xname, sizeof(bi->bi_dev));
	bi->bi_novol = vol;
	bi->bi_nodisk = disk;

	return (0);
}

int
sr_ioctl_vol(struct sr_softc *sc, struct bioc_vol *bv)
{
	int			i, vol, rv = EINVAL;
	struct sr_volume	*mv;

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bv->bv_volid)
			continue;

		mv = &sc->sc_dis[i]->sd_vol;
		bv->bv_status = mv->sv_meta.svm_status;
		bv->bv_size = mv->sv_meta.svm_size;
		bv->bv_level = mv->sv_meta.svm_level;
		bv->bv_nodisk = mv->sv_meta.svm_no_chunk;
		strlcpy(bv->bv_dev, mv->sv_meta.svm_devname,
		    sizeof(bv->bv_dev));
		strlcpy(bv->bv_vendor, mv->sv_meta.svm_vendor,
		    sizeof(bv->bv_vendor));
		rv = 0;
		break;
	}

	return (rv);
}

int
sr_ioctl_disk(struct sr_softc *sc, struct bioc_disk *bd)
{
	int			i, vol, rv = EINVAL;
	struct sr_chunk		*mc;

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bd->bd_volid)
			continue;

		mc = sc->sc_dis[i]->sd_vol.sv_chunks[bd->bd_diskid];
		bd->bd_status = mc->src_meta.scm_status;
		bd->bd_size = mc->src_meta.scm_size;
		strlcpy(bd->bd_vendor, mc->src_meta.scm_devname,
		    sizeof(bd->bd_vendor));
		rv = 0;
		break;
	}

	return (rv);
}

int
sr_ioctl_createraid(struct sr_softc *sc, struct bioc_createraid *bc)
{
	char			*devl;
	int			i, s, no_chunk, rv = EINVAL, sb = -1;
	size_t			bytes = 0;
	u_quad_t		vol_size, max_chunk_sz = 0, min_chunk_sz;
	struct sr_chunk_head	*cl;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*ch_entry;
	struct device		*dev, *dev2;
	struct scsibus_attach_args saa;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_createraid\n", DEVNAME(sc));

	/* user input */
	devl = malloc(bc->bc_dev_list_len + 1, M_DEVBUF, M_WAITOK);
	memset(devl, 0, bc->bc_dev_list_len + 1);
	copyinstr(bc->bc_dev_list, devl, bc->bc_dev_list_len, &bytes);
	DNPRINTF(SR_D_IOCTL, "%s\n", devl);

	sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_WAITOK);
	memset(sd, 0, sizeof(struct sr_discipline));
	sd->sd_sc = sc;
	cl = &sd->sd_vol.sv_chunk_list;

	/* check if we have valid user input */
	SLIST_INIT(cl);
	if ((no_chunk = sr_parse_chunks(sc, devl, cl)) == -1)
		goto unwind;

	/* we have a valid list now create an array index into it */
	sd->sd_vol.sv_chunks = malloc(sizeof(struct sr_chunk *) * no_chunk,
	    M_DEVBUF, M_WAITOK);
	memset(sd->sd_vol.sv_chunks, 0,
	    sizeof(struct sr_chunk *) * no_chunk);
	i = 0;
	SLIST_FOREACH(ch_entry, cl, src_link) {
		sd->sd_vol.sv_chunks[i++] = ch_entry;
		/* while looping get the largest chunk size */
		if (ch_entry->src_meta.scm_size > max_chunk_sz)
			max_chunk_sz = ch_entry->src_meta.scm_size;
	}
	min_chunk_sz = max_chunk_sz;
	SLIST_FOREACH(ch_entry, cl, src_link) {
		if (ch_entry->src_meta.scm_size < min_chunk_sz)
			min_chunk_sz = ch_entry->src_meta.scm_size;
	}
	/* whine if chunks are not the same size */
	if (min_chunk_sz != max_chunk_sz)
		printf("%s: chunk sizes are not equal.  Wasted blocks per "
		    "chunk: %llu\n",
		    DEVNAME(sc), max_chunk_sz - min_chunk_sz);

	switch (bc->bc_level) {
	case 1:
		if (no_chunk < 2)
			goto unwind;

		/* fill out discipline members */
		sd->sd_type = SR_MD_RAID1;
		sd->sd_max_ccb_per_wu = no_chunk;
		sd->sd_max_wu = SR_RAID1_NOWU;
		strlcpy(sd->sd_name, "RAID 1", sizeof(sd->sd_name));
		vol_size = min_chunk_sz - (1 + 1 + no_chunk + SR_META_FUDGE);

		/* XXX coerce all chunks here */

		/* setup pointers */
		sd->sd_alloc_resources = sr_raid1_alloc_resources;
		sd->sd_free_resources = sr_raid1_free_resources;

		sd->sd_scsi_inquiry = sr_raid1_inquiry;
		sd->sd_scsi_read_cap = sr_raid1_read_cap;
		sd->sd_scsi_tur = sr_raid1_tur;
		sd->sd_scsi_req_sense = sr_raid1_request_sense;
		sd->sd_scsi_start_stop = sr_raid1_start_stop;
		sd->sd_scsi_sync = sr_raid1_sync;
		sd->sd_scsi_rw = sr_raid1_rw;
		break;
	default:
		goto unwind;
	}

	/* fill out all volume meta data */
	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_createraid: max_chunk_sz: "
	    "%llu min_chunk_sz: %llu vol_size: %llu\n", DEVNAME(sc),
	    max_chunk_sz, min_chunk_sz, vol_size);
	sd->sd_vol.sv_meta.svm_no_chunk = no_chunk;
	sd->sd_vol.sv_meta.svm_size = vol_size;
	sd->sd_vol.sv_meta.svm_status = BIOC_SVONLINE;
	sd->sd_vol.sv_meta.svm_level = bc->bc_level;
	strlcpy(sd->sd_vol.sv_meta.svm_vendor, "OPENBSD",
	    sizeof(sd->sd_vol.sv_meta.svm_vendor));
	snprintf(sd->sd_vol.sv_meta.svm_product,
	    sizeof(sd->sd_vol.sv_meta.svm_product), "SR %s", sd->sd_name);
	snprintf(sd->sd_vol.sv_meta.svm_revision,
	    sizeof(sd->sd_vol.sv_meta.svm_revision), "%03d",
	    SR_META_VERSION);

	/* allocate all resources */
	sd->sd_alloc_resources(sd);

	/* metadata SHALL be fully filled in at this point */
	sd->sd_link.openings = 1; /* sc->sc_max_cmds; */
	sd->sd_link.device = &sr_dev;
	sd->sd_link.device_softc = sc;
	sd->sd_link.adapter_softc = sc;
	sd->sd_link.adapter = &sr_switch;
	sd->sd_link.adapter_target = SR_MAX_LD;
	sd->sd_link.adapter_buswidth = 1;
	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sd->sd_link;

	/* we passed all checks return ENXIO if volume can't be created */
	rv = ENXIO;

	/* clear sense data */
	bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));
	sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
	sd->sd_scsi_sense.segment = 0;
	sd->sd_scsi_sense.flags = SKEY_NO_SENSE;
	*(u_int32_t*)sd->sd_scsi_sense.info = htole32(0);
	sd->sd_scsi_sense.extra_len = 0;

	/* use temporary discipline pointer */
	s = splhigh();
	sc->sc_attach_dis = sd;
	splx(s);
	dev2 = config_found(&sc->sc_dev, &saa, scsiprint);
	s = splhigh();
	sc->sc_attach_dis = NULL;
	splx(s);
	TAILQ_FOREACH(dev, &alldevs, dv_list)
		if (dev->dv_parent == dev2)
			break;
	if (dev == NULL)
		goto unwind;

	strlcpy(sd->sd_vol.sv_meta.svm_devname, dev->dv_xname,
	    sizeof(sd->sd_vol.sv_meta.svm_devname));
	DNPRINTF(SR_D_IOCTL, "%s: sr device added: %s on scsibus: %d\n",
	    DEVNAME(sc), dev->dv_xname, sd->sd_link.scsibus);

	sc->sc_dis[sd->sd_link.scsibus] = sd;
	sb = sd->sd_link.scsibus;

	rv = 0;
	return (rv);

unwind:
	if (sb != -1)
		sc->sc_dis[sb] = NULL;

	/* XXX free scsibus */

	if (sd) {
		if (sd->sd_free_resources)
			sd->sd_free_resources(sd);
		if (sd->sd_vol.sv_chunks)
			free(sd->sd_vol.sv_chunks, M_DEVBUF);
		free(sd, M_DEVBUF);
		sr_unwind_chunks(sc, cl);

	}
	return (rv);
}

int
sr_parse_chunks(struct sr_softc *sc, char *lst, struct sr_chunk_head *cl)
{
	struct sr_chunk		*ch_entry, *ch_next, *ch_prev;
	struct nameidata	nd;
	struct disklabel	label;
	struct vattr		va;
	int			error;
	char			ss, *name;
	char			*s, *e;
	u_int32_t		sz = 0;
	int			i;
	char			dummy = 0;

	DNPRINTF(SR_D_IOCTL, "%s: sr_parse_chunks\n", DEVNAME(sc));

	if (!lst) {
		lst = &dummy;
		goto bad;
	}

	s = e = lst;
	ch_prev = NULL;
	/* make sure we have a valid device lst like /dev/sdNa,/dev/sdNNa */
	while (*e != '\0') {
		if (*e == ',')
			s = e + 1;
		else if (*(e + 1) == '\0' || *(e + 1) == ',') {
			sz = e - s + 1;
			/* got one */
			ch_entry = malloc(sizeof(struct sr_chunk),
			    M_DEVBUF, M_WAITOK);
			memset(ch_entry, 0, sizeof(struct sr_chunk));

			if (sz  + 1 > sizeof(ch_entry->src_meta.scm_devname))
				goto unwind;

			strlcpy(ch_entry->src_meta.scm_devname, s, sz + 1);

			/* keep disks in user supplied order */
			if (ch_prev)
				SLIST_INSERT_AFTER(ch_prev, ch_entry, src_link);
			else
				SLIST_INSERT_HEAD(cl, ch_entry, src_link);
			ch_prev = ch_entry;
		}
		e++;
	}

	/* check for dups */
	SLIST_FOREACH(ch_entry, cl, src_link) {
		SLIST_FOREACH(ch_next, cl, src_link) {
			if (ch_next == ch_entry)
				continue;

			if (!strcmp(ch_next->src_meta.scm_devname,
			    ch_entry->src_meta.scm_devname))
				goto unwind;
		}
	}

	/* fill out chunk list */
	i = 0;
	SLIST_FOREACH(ch_entry, cl, src_link) {
		/* printf("name: %s\n", ch_entry->src_meta.scm_devname); */
		name = ch_entry->src_meta.scm_devname;
		ch_entry->src_dev_vn = NULL;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, name, curproc);

		/* open disk */
		error = vn_open(&nd, FREAD | FWRITE, 0);
		if (error) {
			printf("%s: could not open %s error %d\n", DEVNAME(sc),
			    name, error);
			goto unwind; /* open failed */
		}
		ch_entry->src_dev_vn = nd.ni_vp;

		/* partition already in use? */
		if (nd.ni_vp->v_usecount > 1) {
			printf("%s: %s in use\n", DEVNAME(sc), name);
			goto unwind;
		}

		/* get attributes */
		error = VOP_GETATTR(nd.ni_vp, &va, curproc->p_ucred, curproc);
		if (error) {
			printf("%s: %s can't retrieve attributes\n",
			    DEVNAME(sc), name);
			goto unwind;
		}

		/* is partition VBLK? */
		if (va.va_type != VBLK) {
			printf("%s: %s not of VBLK type\n",
			    DEVNAME(sc), name);
			goto unwind;
		}

		/* make sure we are not a raw partition */
		if (DISKPART(nd.ni_vp->v_rdev) == RAW_PART) {
			printf("%s: %s can not use raw partitions\n",
			    DEVNAME(sc), name);
			goto unwind;
		}

		/* get disklabel */
		error = VOP_IOCTL(nd.ni_vp, DIOCGDINFO, (caddr_t)&label,
		    FREAD, curproc->p_ucred, curproc);
		if (error) {
			printf("%s: %s could not read disklabel err %d\n",
			    DEVNAME(sc), name, error);
			goto unwind; /* disklabel failed */
		}

		/* get partition size */
		ss = name[strlen(name) - 1];
		ch_entry->src_meta.scm_size =
		    label.d_partitions['a' - ss].p_size;
		if (ch_entry->src_meta.scm_size == 0) {
			printf("%s: %s partition size = 0\n",
			    DEVNAME(sc), name);
			goto unwind;
		}

		/* make sure the partition is of the right type */
		if (label.d_partitions['a' - ss].p_fstype != FS_RAID) {
			printf("%s: %s partition not of type RAID\n",
			    DEVNAME(sc), name);
			goto unwind;
		}

		/* XXX check for stale metadata */

		ch_entry->src_meta.scm_chunk_id = i++;
		ch_entry->src_dev_mm = nd.ni_vp->v_rdev; /* major/minor */

		DNPRINTF(SR_D_IOCTL, "%s: found %s size %d\n", DEVNAME(sc),
		    name, ch_entry->src_meta.scm_size);

		/* mark chunk online */
		ch_entry->src_meta.scm_status = BIOC_SDONLINE;

		/* unlock the vnode */
		VOP_UNLOCK(ch_entry->src_dev_vn, 0, curproc);
	}

	return (i);

unwind:
	sr_unwind_chunks(sc, cl);
bad:
	printf("%s: invalid device list %s\n", DEVNAME(sc), lst);

	return (-1);
}

void
sr_unwind_chunks(struct sr_softc *sc, struct sr_chunk_head *cl)
{
	struct sr_chunk		*ch_entry, *ch_next;

	DNPRINTF(SR_D_IOCTL, "%s: sr_unwind_chunks\n", DEVNAME(sc));

	for (ch_entry = SLIST_FIRST(cl);
	    ch_entry != SLIST_END(cl); ch_entry = ch_next) {
		ch_next = SLIST_NEXT(ch_entry, src_link);

		if (ch_entry->src_dev_vn != NULL) {
			vn_close(ch_entry->src_dev_vn, FREAD | FWRITE,
			    curproc->p_ucred, curproc);
		}

		free(ch_entry, M_DEVBUF);
	}
	SLIST_INIT(cl);
}

/* RAID 1 functions */
int
sr_raid1_alloc_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_alloc_wu(sd);
	sr_alloc_ccb(sd);

	rv = 0;
	return (rv);
}

int
sr_raid1_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_free_wu(sd);
	sr_free_ccb(sd);

	rv = 0;
	return (rv);
}

int
sr_raid1_inquiry(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_inquiry_data inq;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_inquiry\n", DEVNAME(sd->sd_sc));

	bzero(&inq, sizeof(inq));
	inq.device = T_DIRECT;
	inq.dev_qual2 = 0;
	inq.version = 2;
	inq.response_format = 2;
	inq.additional_length = 32;
	strlcpy(inq.vendor, sd->sd_vol.sv_meta.svm_vendor,
	    sizeof(inq.vendor));
	strlcpy(inq.product, sd->sd_vol.sv_meta.svm_product,
	    sizeof(inq.product));
	strlcpy(inq.revision, sd->sd_vol.sv_meta.svm_revision,
	    sizeof(inq.revision));
	sr_copy_internal_data(xs, &inq, sizeof(inq));

	return (0);
}

int
sr_raid1_read_cap(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_read_cap_data rcd;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_read_cap\n", DEVNAME(sd->sd_sc));

	bzero(&rcd, sizeof(rcd));
	_lto4b(sd->sd_vol.sv_meta.svm_size, rcd.addr);
	_lto4b(512, rcd.length);
	sr_copy_internal_data(xs, &rcd, sizeof(rcd));

	return (0);
}

int
sr_raid1_tur(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_tur\n", DEVNAME(sd->sd_sc));

	if (sd->sd_vol.sv_meta.svm_status == BIOC_SVOFFLINE) {
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
		sd->sd_scsi_sense.segment = 0;
		sd->sd_scsi_sense.flags = SKEY_NOT_READY;
		sd->sd_scsi_sense.add_sense_code = 0x04;
		sd->sd_scsi_sense.add_sense_code_qual = 0x11;
		*(u_int32_t*)sd->sd_scsi_sense.info = htole32(0);
		sd->sd_scsi_sense.extra_len = 0;
		return (1);
	} else if (sd->sd_vol.sv_meta.svm_status == BIOC_SVINVALID) {
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
		sd->sd_scsi_sense.segment = 0;
		sd->sd_scsi_sense.flags = SKEY_HARDWARE_ERROR;
		sd->sd_scsi_sense.add_sense_code = 0x05;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		*(u_int32_t*)sd->sd_scsi_sense.info = htole32(0);
		sd->sd_scsi_sense.extra_len = 0;
		return (1);
	}

	return (0);
}

int
sr_raid1_request_sense(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_request_sense\n",
	    DEVNAME(sd->sd_sc));

	/* use latest sense data */
	sr_copy_internal_data(xs, &sd->sd_scsi_sense,
	    sizeof(sd->sd_scsi_sense));

	/* clear sense data */
	bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));
	sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
	sd->sd_scsi_sense.segment = 0;
	sd->sd_scsi_sense.flags = SKEY_NO_SENSE;
	*(u_int32_t*)sd->sd_scsi_sense.info = htole32(0);
	sd->sd_scsi_sense.extra_len = 0;

	return (0);
}

int
sr_raid1_start_stop(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_start_stop	*ss = (struct scsi_start_stop *)xs->cmd;
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_start_stop\n",
	    DEVNAME(sd->sd_sc));

	if (!ss)
		return (rv);

	if (ss->byte2 == 0x00) {
		/* START */
		if (sd->sd_vol.sv_meta.svm_status == BIOC_SVOFFLINE) {
			/* bring volume online */
			/* XXX check to see if volume can be brough online */
			sd->sd_vol.sv_meta.svm_status = BIOC_SVONLINE;
		}
		rv = 0;
	} else /* XXX is this the check? if (byte == 0x01) */ {
		/* STOP */
		if (sd->sd_vol.sv_meta.svm_status == BIOC_SVONLINE) {
			/* bring volume offline */
			sd->sd_vol.sv_meta.svm_status = BIOC_SVOFFLINE;
		}
		rv = 0;
	}

	return (rv);
}

int
sr_raid1_sync(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct sr_chunk		*ch_entry;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_sync\n", DEVNAME(sd->sd_sc));

	/* drain all io */
	SLIST_FOREACH(ch_entry, &sd->sd_vol.sv_chunk_list, src_link) {
		DNPRINTF(SR_D_DIS, "%s: %s: %s: sync\n", DEVNAME(sd->sd_sc),
		    sd->sd_vol.sv_meta.svm_devname,
		    ch_entry->src_meta.scm_devname);
		/* XXX we want MNT_WAIT but that hangs in fdisk */
		VOP_FSYNC(ch_entry->src_dev_vn, curproc->p_ucred,
		    0 /* MNT_WAIT */, curproc);
	}

	return (0);
}

int
sr_raid1_rw(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_ccb		*ccb;
	int			rv = 1, ios, x, i;
	daddr_t			blk;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_rw 0x%02x\n", DEVNAME(sd->sd_sc),
	    xs->cmd->opcode);


	if (xs->datalen == 0) {
		printf("%s: %s: illegal block count\n",
		    DEVNAME(sd->sd_sc), sd->sd_vol.sv_meta.svm_devname);
		goto bad;
	}

	if (xs->cmdlen == 10)
		blk = _4btol(((struct scsi_rw_big *)xs->cmd)->addr);
	else if (xs->cmdlen == 6)
		blk = _3btol(((struct scsi_rw *)xs->cmd)->addr);
	else {
		printf("%s: %s: illegal cmdlen\n", DEVNAME(sd->sd_sc),
		    sd->sd_vol.sv_meta.svm_devname);
		goto bad;
	}

	if (xs->flags & SCSI_DATA_IN)
		ios = 1;
	else
		ios = sd->sd_vol.sv_meta.svm_no_chunk;

	for (i = 0; i < ios; i++) {
		ccb = sr_get_ccb(sd);
		if (!ccb) {
			/* should never happen but handle more gracefully */
			printf("%s: %s: too many ccbs queued\n",
			    DEVNAME(sd->sd_sc),
			    sd->sd_vol.sv_meta.svm_devname);
			goto bad;
		}

		if (xs->flags & SCSI_POLL) {
			ccb->ccb_buf.b_flags = 0;
			ccb->ccb_buf.b_iodone = NULL;
		} else {
			ccb->ccb_buf.b_flags = B_CALL;
			ccb->ccb_buf.b_iodone = sr_raid1_intr;
		}

		ccb->ccb_buf.b_blkno = blk;
		ccb->ccb_buf.b_bcount = xs->datalen;
		ccb->ccb_buf.b_bufsize = xs->datalen;
		ccb->ccb_buf.b_resid = xs->datalen;
		ccb->ccb_buf.b_data = xs->data;
		ccb->ccb_buf.b_error = 0;
		ccb->ccb_buf.b_proc = curproc;

		wu->swu_io_count = ios;
		if (xs->flags & SCSI_DATA_IN) {
			/* interleave reads */
			x = sd->mds.mdd_raid1.sr1_counter %
			    sd->sd_vol.sv_meta.svm_no_chunk;
			sd->mds.mdd_raid1.sr1_counter++;
			ccb->ccb_buf.b_flags |= B_READ;
		} else {
			/* writes go on all disks */
			x = i;
			ccb->ccb_buf.b_flags |= B_WRITE;
		}

		ccb->ccb_buf.b_dev = sd->sd_vol.sv_chunks[x]->src_dev_mm;
		ccb->ccb_buf.b_vp = sd->sd_vol.sv_chunks[x]->src_dev_vn;

		LIST_INIT(&ccb->ccb_buf.b_dep);

		ccb->ccb_wu = wu;

		DNPRINTF(SR_D_DIS, "%s: %s: sr_raid1: b_bcount: %d "
		    "b_blkno: %x b_flags 0x%0x b_data %p\n",
		    DEVNAME(sd->sd_sc), sd->sd_vol.sv_meta.svm_devname,
		    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_blkno,
		    ccb->ccb_buf.b_flags, ccb->ccb_buf.b_data);

		/* vprint("despatch: ", ccb->ccb_buf.b_vp); */
		ccb->ccb_buf.b_vp->v_numoutput++;
		VOP_STRATEGY(&ccb->ccb_buf);
		if (xs->flags & SCSI_POLL) {
			/* polling, wait for completion */
			biowait(&ccb->ccb_buf);
			if (ccb->ccb_buf.b_flags & B_ERROR) {
				printf("%s: %s: %s: i/o error on block %d\n",
				    DEVNAME(sd->sd_sc),
				    sd->sd_vol.sv_meta.svm_devname,
				    sd->sd_vol.sv_chunks[x]->src_meta.scm_devname,
				    ccb->ccb_buf.b_blkno);
			/* don't abort other ios because of error */
			}
			sr_put_ccb(ccb);
		}
	}

	rv = 0;
bad:

	return (rv);
}

void
sr_raid1_intr(struct buf *bp)
{
	struct sr_ccb		*ccb = (struct sr_ccb *)bp;
	struct sr_workunit	*wu = ccb->ccb_wu;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_softc		*sc = sd->sd_sc;
	int			s;

	DNPRINTF(SR_D_INTR, "%s: sr_intr bp %x xs %x\n",
	    DEVNAME(sc), bp, xs);

	DNPRINTF(SR_D_INTR, "%s: sr_intr: b_bcount: %d b_resid: %d"
	    " b_flags: 0x%0x\n", DEVNAME(sc), ccb->ccb_buf.b_bcount,
	    ccb->ccb_buf.b_resid, ccb->ccb_buf.b_flags);

	if (ccb->ccb_buf.b_flags & B_ERROR) {
		printf("%s: i/o error on block %d\n", DEVNAME(sc),
		    ccb->ccb_buf.b_blkno);
		wu->swu_state = SR_WU_FAILED;
	}

	sr_put_ccb(ccb);

	wu->swu_ios_complete++;
	DNPRINTF(SR_D_INTR, "%s: sr_intr: comp: %d count: %d\n",
	    DEVNAME(sc), wu->swu_ios_complete, wu->swu_io_count);
	if (wu->swu_ios_complete == wu->swu_io_count) {
		/* do something smarter here instead of failing the whole WU */
		if (wu->swu_state == SR_WU_FAILED)
			xs->error = XS_DRIVER_STUFFUP;
		else
			xs->error = XS_NOERROR;

		xs->resid = 0;
		xs->flags |= ITSDONE;

		s = splbio();
		scsi_done(xs);
		splx(s);
		sr_put_wu(wu);
	}
}
