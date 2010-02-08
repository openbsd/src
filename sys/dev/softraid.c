/* $OpenBSD: softraid.c,v 1.190 2010/02/08 23:28:06 krw Exp $ */
/*
 * Copyright (c) 2007, 2008, 2009 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2008 Chris Kuethe <ckuethe@openbsd.org>
 * Copyright (c) 2009 Joel Sing <jsing@openbsd.org>
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
#include <sys/pool.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/sensors.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/workq.h>
#include <sys/kthread.h>

#ifdef AOE
#include <sys/mbuf.h>
#include <net/if_aoe.h>
#endif /* AOE */

#include <crypto/cryptodev.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

/* #define SR_FANCY_STATS */

#ifdef SR_DEBUG
#define SR_FANCY_STATS
uint32_t	sr_debug = 0
		    /* | SR_D_CMD */
		    /* | SR_D_MISC */
		    /* | SR_D_INTR */
		    /* | SR_D_IOCTL */
		    /* | SR_D_CCB */
		    /* | SR_D_WU */
		    /* | SR_D_META */
		    /* | SR_D_DIS */
		    /* | SR_D_STATE */
		;
#endif

int		sr_match(struct device *, void *, void *);
void		sr_attach(struct device *, struct device *, void *);
int		sr_detach(struct device *, int);
int		sr_activate(struct device *, int);

struct cfattach softraid_ca = {
	sizeof(struct sr_softc), sr_match, sr_attach, sr_detach,
	sr_activate
};

struct cfdriver softraid_cd = {
	NULL, "softraid", DV_DULL
};

/* scsi & discipline */
int			sr_scsi_cmd(struct scsi_xfer *);
void			sr_minphys(struct buf *bp, struct scsi_link *sl);
void			sr_copy_internal_data(struct scsi_xfer *,
			    void *, size_t);
int			sr_scsi_ioctl(struct scsi_link *, u_long,
			    caddr_t, int, struct proc *);
int			sr_ioctl(struct device *, u_long, caddr_t);
int			sr_ioctl_inq(struct sr_softc *, struct bioc_inq *);
int			sr_ioctl_vol(struct sr_softc *, struct bioc_vol *);
int			sr_ioctl_disk(struct sr_softc *, struct bioc_disk *);
int			sr_ioctl_setstate(struct sr_softc *,
			    struct bioc_setstate *);
int			sr_ioctl_createraid(struct sr_softc *,
			    struct bioc_createraid *, int);
int			sr_ioctl_deleteraid(struct sr_softc *,
			    struct bioc_deleteraid *);
int			sr_ioctl_discipline(struct sr_softc *,
			    struct bioc_discipline *);
void			sr_chunks_unwind(struct sr_softc *,
			    struct sr_chunk_head *);
void			sr_discipline_free(struct sr_discipline *);
void			sr_discipline_shutdown(struct sr_discipline *);
int			sr_discipline_init(struct sr_discipline *, int);

/* utility functions */
void			sr_shutdown(void *);
void			sr_uuid_get(struct sr_uuid *);
void			sr_uuid_print(struct sr_uuid *, int);
void			sr_checksum_print(u_int8_t *);
int			sr_boot_assembly(struct sr_softc *);
int			sr_already_assembled(struct sr_discipline *);
int			sr_hotspare(struct sr_softc *, dev_t);
void			sr_hotspare_rebuild(struct sr_discipline *);
int			sr_rebuild_init(struct sr_discipline *, dev_t);
void			sr_rebuild(void *);
void			sr_rebuild_thread(void *);
void			sr_roam_chunks(struct sr_discipline *);
int			sr_chunk_in_use(struct sr_softc *, dev_t);

/* don't include these on RAMDISK */
#ifndef SMALL_KERNEL
void			sr_sensors_refresh(void *);
int			sr_sensors_create(struct sr_discipline *);
void			sr_sensors_delete(struct sr_discipline *);
#endif

/* metadata */
int			sr_meta_probe(struct sr_discipline *, dev_t *, int);
int			sr_meta_attach(struct sr_discipline *, int);
int			sr_meta_rw(struct sr_discipline *, dev_t, void *,
			    size_t, daddr64_t, long);
int			sr_meta_clear(struct sr_discipline *);
void			sr_meta_chunks_create(struct sr_softc *,
			    struct sr_chunk_head *);
void			sr_meta_init(struct sr_discipline *,
			    struct sr_chunk_head *);

/* hotplug magic */
void			sr_disk_attach(struct disk *, int);

struct sr_hotplug_list {
	void			(*sh_hotplug)(struct sr_discipline *,
				    struct disk *, int);
	struct sr_discipline	*sh_sd;

	SLIST_ENTRY(sr_hotplug_list) shl_link;
};
SLIST_HEAD(sr_hotplug_list_head, sr_hotplug_list);

struct			sr_hotplug_list_head	sr_hotplug_callbacks;
extern void		(*softraid_disk_attach)(struct disk *, int);

/* scsi glue */
struct scsi_adapter sr_switch = {
	sr_scsi_cmd, sr_minphys, NULL, NULL, sr_scsi_ioctl
};

struct scsi_device sr_dev = {
	NULL, NULL, NULL, NULL
};

/* native metadata format */
int			sr_meta_native_bootprobe(struct sr_softc *,
			    struct device *, struct sr_metadata_list_head *);
#define SR_META_NOTCLAIMED	(0)
#define SR_META_CLAIMED		(1)
int			sr_meta_native_probe(struct sr_softc *,
			   struct sr_chunk *);
int			sr_meta_native_attach(struct sr_discipline *, int);
int			sr_meta_native_write(struct sr_discipline *, dev_t,
			    struct sr_metadata *,void *);

#ifdef SR_DEBUG
void			sr_meta_print(struct sr_metadata *);
#else
#define			sr_meta_print(m)
#endif

/* the metadata driver should remain stateless */
struct sr_meta_driver {
	daddr64_t		smd_offset;	/* metadata location */
	u_int32_t		smd_size;	/* size of metadata */

	int			(*smd_probe)(struct sr_softc *,
				   struct sr_chunk *);
	int			(*smd_attach)(struct sr_discipline *, int);
	int			(*smd_detach)(struct sr_discipline *);
	int			(*smd_read)(struct sr_discipline *, dev_t,
				    struct sr_metadata *, void *);
	int			(*smd_write)(struct sr_discipline *, dev_t,
				    struct sr_metadata *, void *);
	int			(*smd_validate)(struct sr_discipline *,
				    struct sr_metadata *, void *);
} smd[] = {
	{ SR_META_OFFSET, SR_META_SIZE * 512,
	  sr_meta_native_probe, sr_meta_native_attach, NULL,
	  sr_meta_native_read, sr_meta_native_write, NULL },
	{ 0, 0, NULL, NULL, NULL, NULL }
};

int
sr_meta_attach(struct sr_discipline *sd, int force)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head	*cl;
	struct sr_chunk		*ch_entry;
	int			rv = 1, i = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_attach(%d)\n", DEVNAME(sc));

	/* in memory copy of metadata */
	sd->sd_meta = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO);
	if (!sd->sd_meta) {
		printf("%s: could not allocate memory for metadata\n",
		    DEVNAME(sc));
		goto bad;
	}

	if (sd->sd_meta_type != SR_META_F_NATIVE) {
		/* in memory copy of foreign metadata */
		sd->sd_meta_foreign = malloc(smd[sd->sd_meta_type].smd_size,
		    M_DEVBUF, M_ZERO);
		if (!sd->sd_meta_foreign) {
			/* unwind frees sd_meta */
			printf("%s: could not allocate memory for foreign "
			    "metadata\n", DEVNAME(sc));
			goto bad;
		}
	}

	/* we have a valid list now create an array index */
	cl = &sd->sd_vol.sv_chunk_list;
	SLIST_FOREACH(ch_entry, cl, src_link) {
		i++;
	}
	sd->sd_vol.sv_chunks = malloc(sizeof(struct sr_chunk *) * i,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* fill out chunk array */
	i = 0;
	SLIST_FOREACH(ch_entry, cl, src_link)
		sd->sd_vol.sv_chunks[i++] = ch_entry;

	/* attach metadata */
	if (smd[sd->sd_meta_type].smd_attach(sd, force))
		goto bad;

	rv = 0;
bad:
	return (rv);
}

int
sr_meta_probe(struct sr_discipline *sd, dev_t *dt, int no_chunk)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct vnode		*vn;
	struct sr_chunk		*ch_entry, *ch_prev = NULL;
	struct sr_chunk_head	*cl;
	char			devname[32];
	int			i, d, type, found, prevf, error;
	dev_t			dev;

	DNPRINTF(SR_D_META, "%s: sr_meta_probe(%d)\n", DEVNAME(sc), no_chunk);

	if (no_chunk == 0)
		goto unwind;


	cl = &sd->sd_vol.sv_chunk_list;

	for (d = 0, prevf = SR_META_F_INVALID; d < no_chunk; d++) {
		ch_entry = malloc(sizeof(struct sr_chunk), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		/* keep disks in user supplied order */
		if (ch_prev)
			SLIST_INSERT_AFTER(ch_prev, ch_entry, src_link);
		else
			SLIST_INSERT_HEAD(cl, ch_entry, src_link);
		ch_prev = ch_entry;
		dev = dt[d];
		ch_entry->src_dev_mm = dev;

		if (dev == NODEV) {
			ch_entry->src_meta.scm_status = BIOC_SDOFFLINE;
			continue;
		} else {
			sr_meta_getdevname(sc, dev, devname, sizeof(devname));
			if (bdevvp(dev, &vn)) {
				printf("%s:, sr_meta_probe: can't allocate "
				    "vnode\n", DEVNAME(sc));
				goto unwind;
			}

			/*
			 * XXX leaving dev open for now; move this to attach
			 * and figure out the open/close dance for unwind.
			 */
			error = VOP_OPEN(vn, FREAD | FWRITE, NOCRED, 0);
			if (error) {
				DNPRINTF(SR_D_META,"%s: sr_meta_probe can't "
				    "open %s\n", DEVNAME(sc), devname);
				vput(vn);
				goto unwind;
			}

			strlcpy(ch_entry->src_devname, devname,
			    sizeof(ch_entry->src_devname));
			ch_entry->src_vn = vn;
		}

		/* determine if this is a device we understand */
		for (i = 0, found = SR_META_F_INVALID; smd[i].smd_probe; i++) {
			type = smd[i].smd_probe(sc, ch_entry);
			if (type == SR_META_F_INVALID)
				continue;
			else {
				found = type;
				break;
			}
		}

		if (found == SR_META_F_INVALID)
			goto unwind;
		if (prevf == SR_META_F_INVALID)
			prevf = found;
		if (prevf != found) {
			DNPRINTF(SR_D_META, "%s: prevf != found\n",
			    DEVNAME(sc));
			goto unwind;
		}
	}

	return (prevf);
unwind:
	return (SR_META_F_INVALID);
}

void
sr_meta_getdevname(struct sr_softc *sc, dev_t dev, char *buf, int size)
{
	int			maj, unit, part;
	char			*name;

	DNPRINTF(SR_D_META, "%s: sr_meta_getdevname(%p, %d)\n",
	    DEVNAME(sc), buf, size);

	if (!buf)
		return;

	maj = major(dev);
	part = DISKPART(dev);
	unit = DISKUNIT(dev);

	name = findblkname(maj);
	if (name == NULL)
		return;

	snprintf(buf, size, "%s%d%c", name, unit, part + 'a');
}

int
sr_meta_rw(struct sr_discipline *sd, dev_t dev, void *md, size_t sz,
    daddr64_t ofs, long flags)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct buf		b;
	int			rv = 1;

	DNPRINTF(SR_D_META, "%s: sr_meta_rw(0x%x, %p, %d, %llu 0x%x)\n",
	    DEVNAME(sc), dev, md, sz, ofs, flags);

	bzero(&b, sizeof(b));

	if (md == NULL) {
		printf("%s: read invalid metadata pointer\n", DEVNAME(sc));
		goto done;
	}
	b.b_flags = flags | B_PHYS;
	b.b_blkno = ofs;
	b.b_bcount = sz;
	b.b_bufsize = sz;
	b.b_resid = sz;
	b.b_data = md;
	b.b_error = 0;
	b.b_proc = curproc;
	b.b_dev = dev;
	b.b_iodone = NULL;
	if (bdevvp(dev, &b.b_vp)) {
		printf("%s: sr_meta_rw: can't allocate vnode\n", DEVNAME(sc));
		goto done;
	}
	if ((b.b_flags & B_READ) == 0)
		b.b_vp->v_numoutput++;

	LIST_INIT(&b.b_dep);
	VOP_STRATEGY(&b);
	biowait(&b);

	if (b.b_flags & B_ERROR) {
		printf("%s: 0x%x i/o error on block %llu while reading "
		    "metadata %d\n", DEVNAME(sc), dev, b.b_blkno, b.b_error);
		goto done;
	}
	rv = 0;
done:
	if (b.b_vp)
		vput(b.b_vp);

	return (rv);
}

int
sr_meta_clear(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_chunk		*ch_entry;
	void			*m;
	int			rv = 1;

	DNPRINTF(SR_D_META, "%s: sr_meta_clear\n", DEVNAME(sc));

	if (sd->sd_meta_type != SR_META_F_NATIVE) {
		printf("%s: sr_meta_clear can not clear foreign metadata\n",
		    DEVNAME(sc));
		goto done;
	}

	m = malloc(SR_META_SIZE * 512, M_DEVBUF, M_WAITOK | M_ZERO);
	SLIST_FOREACH(ch_entry, cl, src_link) {
		if (sr_meta_native_write(sd, ch_entry->src_dev_mm, m, NULL)) {
			/* XXX mark disk offline */
			DNPRINTF(SR_D_META, "%s: sr_meta_clear failed to "
			    "clear %s\n", ch_entry->src_devname);
			rv++;
			continue;
		}
		bzero(&ch_entry->src_meta, sizeof(ch_entry->src_meta));
		bzero(&ch_entry->src_opt, sizeof(ch_entry->src_opt));
	}

	bzero(sd->sd_meta, SR_META_SIZE * 512);

	free(m, M_DEVBUF);
	rv = 0;
done:
	return (rv);
}

void
sr_meta_chunks_create(struct sr_softc *sc, struct sr_chunk_head *cl)
{
	struct sr_chunk		*ch_entry;
	struct sr_uuid		uuid;
	int			cid = 0;
	char			*name;
	u_int64_t		max_chunk_sz = 0, min_chunk_sz;

	DNPRINTF(SR_D_META, "%s: sr_meta_chunks_create\n", DEVNAME(sc));

	sr_uuid_get(&uuid);

	/* fill out stuff and get largest chunk size while looping */
	SLIST_FOREACH(ch_entry, cl, src_link) {
		name = ch_entry->src_devname;
		ch_entry->src_meta.scmi.scm_size = ch_entry->src_size;
		ch_entry->src_meta.scmi.scm_chunk_id = cid++;
		ch_entry->src_meta.scm_status = BIOC_SDONLINE;
		strlcpy(ch_entry->src_meta.scmi.scm_devname, name,
		    sizeof(ch_entry->src_meta.scmi.scm_devname));
		bcopy(&uuid, &ch_entry->src_meta.scmi.scm_uuid,
		    sizeof(ch_entry->src_meta.scmi.scm_uuid));

		if (ch_entry->src_meta.scmi.scm_size > max_chunk_sz)
			max_chunk_sz = ch_entry->src_meta.scmi.scm_size;
	}

	/* get smallest chunk size */
	min_chunk_sz = max_chunk_sz;
	SLIST_FOREACH(ch_entry, cl, src_link)
		if (ch_entry->src_meta.scmi.scm_size < min_chunk_sz)
			min_chunk_sz = ch_entry->src_meta.scmi.scm_size;

	/* equalize all sizes */
	SLIST_FOREACH(ch_entry, cl, src_link)
		ch_entry->src_meta.scmi.scm_coerced_size = min_chunk_sz;

	/* whine if chunks are not the same size */
	if (min_chunk_sz != max_chunk_sz)
		printf("%s: chunk sizes are not equal; up to %llu blocks "
		    "wasted per chunk\n",
		    DEVNAME(sc), max_chunk_sz - min_chunk_sz);
}

void
sr_meta_init(struct sr_discipline *sd, struct sr_chunk_head *cl)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_metadata	*sm = sd->sd_meta;
	struct sr_meta_chunk	*im_sc;
	struct sr_meta_opt	*im_so;
	int			i, chunk_no;

	DNPRINTF(SR_D_META, "%s: sr_meta_init\n", DEVNAME(sc));

	if (!sm)
		return;

	/* initial metadata */
	sm->ssdi.ssd_magic = SR_MAGIC;
	sm->ssdi.ssd_version = SR_META_VERSION;
	sm->ssd_ondisk = 0;
	sm->ssdi.ssd_flags = sd->sd_meta_flags;

	/* get uuid from chunk 0 */
	bcopy(&sd->sd_vol.sv_chunks[0]->src_meta.scmi.scm_uuid,
	    &sm->ssdi.ssd_uuid,
	    sizeof(struct sr_uuid));

	/* volume is filled in createraid */

	/* add missing chunk bits */
	chunk_no = sm->ssdi.ssd_chunk_no;
	for (i = 0; i < chunk_no; i++) {
		im_sc = &sd->sd_vol.sv_chunks[i]->src_meta;
		im_sc->scmi.scm_volid = sm->ssdi.ssd_volid;
		sr_checksum(sc, im_sc, &im_sc->scm_checksum,
		    sizeof(struct sr_meta_chunk_invariant));

		/* carry optional meta also in chunk area */
		im_so = &sd->sd_vol.sv_chunks[i]->src_opt;
		bzero(im_so, sizeof(*im_so));
		if (sd->sd_type == SR_MD_CRYPTO) {
			sm->ssdi.ssd_opt_no = 1;
			im_so->somi.som_type = SR_OPT_CRYPTO;

			/*
			 * copy encrypted key / passphrase into optional
			 * metadata area
			 */
			bcopy(&sd->mds.mdd_crypto.scr_meta,
			    &im_so->somi.som_meta.smm_crypto,
			    sizeof(im_so->somi.som_meta.smm_crypto));

			sr_checksum(sc, im_so, im_so->som_checksum,
			    sizeof(struct sr_meta_opt_invariant));
		}
	}
}

void
sr_meta_save_callback(void *arg1, void *arg2)
{
	struct sr_discipline	*sd = arg1;
	int			s;

	s = splbio();

	if (sr_meta_save(arg1, SR_META_DIRTY))
		printf("%s: save metadata failed\n",
		    DEVNAME(sd->sd_sc));

	sd->sd_must_flush = 0;
	splx(s);
}

int
sr_meta_save(struct sr_discipline *sd, u_int32_t flags)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_metadata	*sm = sd->sd_meta, *m;
	struct sr_meta_driver	*s;
	struct sr_chunk		*src;
	struct sr_meta_chunk	*cm;
	struct sr_workunit	wu;
	struct sr_meta_opt	*om;
	int			i;

	DNPRINTF(SR_D_META, "%s: sr_meta_save %s\n",
	    DEVNAME(sc), sd->sd_meta->ssd_devname);

	if (!sm) {
		printf("%s: no in memory copy of metadata\n", DEVNAME(sc));
		goto bad;
	}

	/* meta scratchpad */
	s = &smd[sd->sd_meta_type];
	m = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO);
	if (!m) {
		printf("%s: could not allocate metadata scratch area\n",
		    DEVNAME(sc));
		goto bad;
	}

	if (sm->ssdi.ssd_opt_no > 1)
		panic("not yet save > 1 optional metadata members");

	/* from here on out metadata is updated */
restart:
	sm->ssd_ondisk++;
	sm->ssd_meta_flags = flags;
	bcopy(sm, m, sizeof(*m));

	for (i = 0; i < sm->ssdi.ssd_chunk_no; i++) {
		src = sd->sd_vol.sv_chunks[i];
		cm = (struct sr_meta_chunk *)(m + 1);
		bcopy(&src->src_meta, cm + i, sizeof(*cm));
	}

	/* optional metadata */
	om = (struct sr_meta_opt *)(cm + i);
	for (i = 0; i < sm->ssdi.ssd_opt_no; i++) {
		bcopy(&src->src_opt, om + i, sizeof(*om));
		sr_checksum(sc, om, &om->som_checksum,
		    sizeof(struct sr_meta_opt_invariant));
	}

	for (i = 0; i < sm->ssdi.ssd_chunk_no; i++) {
		src = sd->sd_vol.sv_chunks[i];

		/* skip disks that are offline */
		if (src->src_meta.scm_status == BIOC_SDOFFLINE)
			continue;

		/* calculate metadata checksum for correct chunk */
		m->ssdi.ssd_chunk_id = i;
		sr_checksum(sc, m, &m->ssd_checksum,
		    sizeof(struct sr_meta_invariant));

#ifdef SR_DEBUG
		DNPRINTF(SR_D_META, "%s: sr_meta_save %s: volid: %d "
		    "chunkid: %d checksum: ",
		    DEVNAME(sc), src->src_meta.scmi.scm_devname,
		    m->ssdi.ssd_volid, m->ssdi.ssd_chunk_id);

		if (sr_debug & SR_D_META)
			sr_checksum_print((u_int8_t *)&m->ssd_checksum);
		DNPRINTF(SR_D_META, "\n");
		sr_meta_print(m);
#endif

		/* translate and write to disk */
		if (s->smd_write(sd, src->src_dev_mm, m, NULL /* XXX */)) {
			printf("%s: could not write metadata to %s\n",
			    DEVNAME(sc), src->src_devname);
			/* restart the meta write */
			src->src_meta.scm_status = BIOC_SDOFFLINE;
			/* XXX recalculate volume status */
			goto restart;
		}
	}

	/* not all disciplines have sync */
	if (sd->sd_scsi_sync) {
		bzero(&wu, sizeof(wu));
		wu.swu_fake = 1;
		wu.swu_dis = sd;
		sd->sd_scsi_sync(&wu);
	}
	free(m, M_DEVBUF);
	return (0);
bad:
	return (1);
}

int
sr_meta_read(struct sr_discipline *sd)
{
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	struct sr_chunk_head 	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_metadata	*sm;
	struct sr_chunk		*ch_entry;
	struct sr_meta_chunk	*cp;
	struct sr_meta_driver	*s;
	struct sr_meta_opt	*om;
	void			*fm = NULL;
	int			no_disk = 0, got_meta = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_read\n", DEVNAME(sc));

	sm = malloc(SR_META_SIZE * 512, M_DEVBUF, M_WAITOK | M_ZERO);
	s = &smd[sd->sd_meta_type];
	if (sd->sd_meta_type != SR_META_F_NATIVE)
		fm = malloc(s->smd_size, M_DEVBUF, M_WAITOK | M_ZERO);

	cp = (struct sr_meta_chunk *)(sm + 1);
	SLIST_FOREACH(ch_entry, cl, src_link) {
		/* skip disks that are offline */
		if (ch_entry->src_meta.scm_status == BIOC_SDOFFLINE) {
			DNPRINTF(SR_D_META,
			    "%s: %s chunk marked offline, spoofing status\n",
			    DEVNAME(sc), ch_entry->src_devname);
			cp++; /* adjust chunk pointer to match failure */
			continue;
		} else if (s->smd_read(sd, ch_entry->src_dev_mm, sm, fm)) {
			/* read and translate */
			/* XXX mark chunk offline, elsewhere!! */
			ch_entry->src_meta.scm_status = BIOC_SDOFFLINE;
			cp++; /* adjust chunk pointer to match failure */
			DNPRINTF(SR_D_META, "%s: sr_meta_read failed\n",
			    DEVNAME(sc));
			continue;
		}

		if (sm->ssdi.ssd_magic != SR_MAGIC) {
			DNPRINTF(SR_D_META, "%s: sr_meta_read !SR_MAGIC\n",
			    DEVNAME(sc));
			continue;
		}

		/* validate metadata */
		if (sr_meta_validate(sd, ch_entry->src_dev_mm, sm, fm)) {
			DNPRINTF(SR_D_META, "%s: invalid metadata\n",
			    DEVNAME(sc));
			no_disk = -1;
			goto done;
		}

		/* assume first chunk contains metadata */
		if (got_meta == 0) {
			bcopy(sm, sd->sd_meta, sizeof(*sd->sd_meta));
			got_meta = 1;
		}

		bcopy(cp, &ch_entry->src_meta, sizeof(ch_entry->src_meta));

		if (sm->ssdi.ssd_opt_no > 1)
			panic("not yet read > 1 optional metadata members");

		if (sm->ssdi.ssd_opt_no) {
			om = (struct sr_meta_opt *) ((u_int8_t *)(sm + 1) +
			    sizeof(struct sr_meta_chunk) *
			    sm->ssdi.ssd_chunk_no);
			bcopy(om, &ch_entry->src_opt,
			    sizeof(ch_entry->src_opt));

			if (om->somi.som_type == SR_OPT_CRYPTO) {
				bcopy(
				    &ch_entry->src_opt.somi.som_meta.smm_crypto,
				    &sd->mds.mdd_crypto.scr_meta,
				    sizeof(sd->mds.mdd_crypto.scr_meta));
			}
		}

		cp++;
		no_disk++;
	}

	free(sm, M_DEVBUF);
	if (fm)
		free(fm, M_DEVBUF);

done:
	DNPRINTF(SR_D_META, "%s: sr_meta_read found %d parts\n", DEVNAME(sc),
	    no_disk);
	return (no_disk);
}

int
sr_meta_validate(struct sr_discipline *sd, dev_t dev, struct sr_metadata *sm,
    void *fm)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_meta_driver	*s;
#ifdef SR_DEBUG
	struct sr_meta_chunk	*mc;
#endif
	char			devname[32];
	int			rv = 1;
	u_int8_t		checksum[MD5_DIGEST_LENGTH];

	DNPRINTF(SR_D_META, "%s: sr_meta_validate(%p)\n", DEVNAME(sc), sm);

	sr_meta_getdevname(sc, dev, devname, sizeof(devname));

	s = &smd[sd->sd_meta_type];
	if (sd->sd_meta_type != SR_META_F_NATIVE)
		if (s->smd_validate(sd, sm, fm)) {
			printf("%s: invalid foreign metadata\n", DEVNAME(sc));
			goto done;
		}

	/*
	 * at this point all foreign metadata has been translated to the native
	 * format and will be treated just like the native format
	 */

	if (sm->ssdi.ssd_magic != SR_MAGIC) {
		printf("%s: not valid softraid metadata\n", DEVNAME(sc));
		goto done;
	}

	if (sm->ssdi.ssd_version != SR_META_VERSION) {
		printf("%s: %s can not read metadata version %u, expected %u\n",
		    DEVNAME(sc), devname, sm->ssdi.ssd_version,
		    SR_META_VERSION);
		goto done;
	}

	sr_checksum(sc, sm, &checksum, sizeof(struct sr_meta_invariant));
	if (bcmp(&checksum, &sm->ssd_checksum, sizeof(checksum))) {
		printf("%s: invalid metadata checksum\n", DEVNAME(sc));
		goto done;
	}

	/* XXX do other checksums */

#ifdef SR_DEBUG
	/* warn if disk changed order */
	mc = (struct sr_meta_chunk *)(sm + 1);
	if (strncmp(mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname, devname,
	    sizeof(mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname)))
		DNPRINTF(SR_D_META, "%s: roaming device %s -> %s\n",
		    DEVNAME(sc), mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname,
		    devname);
#endif

	/* we have meta data on disk */
	DNPRINTF(SR_D_META, "%s: sr_meta_validate valid metadata %s\n",
	    DEVNAME(sc), devname);

	rv = 0;
done:
	return (rv);
}

int
sr_meta_native_bootprobe(struct sr_softc *sc, struct device *dv,
    struct sr_metadata_list_head *mlh)
{
	struct vnode		*vn;
	struct disklabel	label;
	struct sr_metadata	*md = NULL;
	struct sr_discipline	*fake_sd = NULL;
	struct sr_metadata_list *mle;
	char			devname[32];
	dev_t			dev, devr;
	int			error, i, majdev;
	int			rv = SR_META_NOTCLAIMED;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe\n", DEVNAME(sc));

	majdev = findblkmajor(dv);
	if (majdev == -1)
		goto done;
	dev = MAKEDISKDEV(majdev, dv->dv_unit, RAW_PART);

	/*
	 * Use character raw device to avoid SCSI complaints about missing
	 * media on removable media devices.
	 */
	dev = MAKEDISKDEV(major(blktochr(dev)), dv->dv_unit, RAW_PART);
	if (cdevvp(dev, &vn)) {
		printf("%s:, sr_meta_native_bootprobe: can't allocate vnode\n",
		    DEVNAME(sc));
		goto done;
	}

	/* open device */
	error = VOP_OPEN(vn, FREAD, NOCRED, 0);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe open "
		    "failed\n", DEVNAME(sc));
		vput(vn);
		goto done;
	}

	/* get disklabel */
	error = VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)&label, FREAD, NOCRED, 0);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe ioctl "
		    "failed\n", DEVNAME(sc));
		VOP_CLOSE(vn, FREAD, NOCRED, 0);
		vput(vn);
		goto done;
	}

	/* we are done, close device */
	error = VOP_CLOSE(vn, FREAD, NOCRED, 0);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe close "
		    "failed\n", DEVNAME(sc));
		vput(vn);
		goto done;
	}
	vput(vn);

	md = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO);
	if (md == NULL) {
		printf("%s: not enough memory for metadata buffer\n",
		    DEVNAME(sc));
		goto done;
	}

	/* create fake sd to use utility functions */
	fake_sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_ZERO);
	if (fake_sd == NULL) {
		printf("%s: not enough memory for fake discipline\n",
		    DEVNAME(sc));
		goto done;
	}
	fake_sd->sd_sc = sc;
	fake_sd->sd_meta_type = SR_META_F_NATIVE;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (label.d_partitions[i].p_fstype != FS_RAID)
			continue;

		/* open partition */
		devr = MAKEDISKDEV(majdev, dv->dv_unit, i);
		if (bdevvp(devr, &vn)) {
			printf("%s:, sr_meta_native_bootprobe: can't allocate "
			    "vnode for partition\n", DEVNAME(sc));
			goto done;
		}
		error = VOP_OPEN(vn, FREAD, NOCRED, 0);
		if (error) {
			DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe "
			    "open failed, partition %d\n",
			    DEVNAME(sc), i);
			vput(vn);
			continue;
		}

		if (sr_meta_native_read(fake_sd, devr, md, NULL)) {
			printf("%s: native bootprobe could not read native "
			    "metadata\n", DEVNAME(sc));
			VOP_CLOSE(vn, FREAD, NOCRED, 0);
			vput(vn);
			continue;
		}

		/* are we a softraid partition? */
		if (md->ssdi.ssd_magic != SR_MAGIC) {
			VOP_CLOSE(vn, FREAD, NOCRED, 0);
			vput(vn);
			continue;
		}

		sr_meta_getdevname(sc, devr, devname, sizeof(devname));
		if (sr_meta_validate(fake_sd, devr, md, NULL) == 0) {
			if (md->ssdi.ssd_flags & BIOC_SCNOAUTOASSEMBLE) {
				DNPRINTF(SR_D_META, "%s: don't save %s\n",
				    DEVNAME(sc), devname);
			} else {
				/* XXX fix M_WAITOK, this is boot time */
				mle = malloc(sizeof(*mle), M_DEVBUF,
				    M_WAITOK | M_ZERO);
				bcopy(md, &mle->sml_metadata,
				    SR_META_SIZE * 512);
				mle->sml_mm = devr;
				mle->sml_vn = vn;
				SLIST_INSERT_HEAD(mlh, mle, sml_link);
				rv = SR_META_CLAIMED;
			}
		}

		/* we are done, close partition */
		VOP_CLOSE(vn, FREAD, NOCRED, 0);
		vput(vn);
	}

done:
	if (fake_sd)
		free(fake_sd, M_DEVBUF);
	if (md)
		free(md, M_DEVBUF);

	return (rv);
}

int
sr_boot_assembly(struct sr_softc *sc)
{
	struct device		*dv;
	struct bioc_createraid	bc;
	struct sr_metadata_list_head mlh, kdh;
	struct sr_metadata_list *mle, *mlenext, *mle1, *mle2;
	struct sr_metadata	*metadata;
	struct sr_boot_volume_head bvh;
	struct sr_boot_volume	*vol, *vp1, *vp2;
	struct sr_meta_chunk	*hm;
	struct sr_chunk_head	*cl;
	struct sr_chunk		*hotspare, *chunk, *last;
	u_int32_t		chunk_id;
	u_int64_t		*ondisk = NULL;
	dev_t			*devs = NULL;
	char			devname[32];
	int			rv = 0, i;

	DNPRINTF(SR_D_META, "%s: sr_boot_assembly\n", DEVNAME(sc));

	SLIST_INIT(&mlh);

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class != DV_DISK)
			continue;

		/* Only check sd(4) and wd(4) devices. */
		if (strcmp(dv->dv_cfdata->cf_driver->cd_name, "sd") &&
		    strcmp(dv->dv_cfdata->cf_driver->cd_name, "wd"))
			continue;

		/* native softraid uses partitions */
		if (sr_meta_native_bootprobe(sc, dv, &mlh) == SR_META_CLAIMED)
			continue;

		/* probe non-native disks */
	}

	/*
	 * Create a list of volumes and associate chunks with each volume.
	 */

	SLIST_INIT(&bvh);
	SLIST_INIT(&kdh);

	for (mle = SLIST_FIRST(&mlh); mle != SLIST_END(&mlh); mle = mlenext) {

		mlenext = SLIST_NEXT(mle, sml_link);
		SLIST_REMOVE(&mlh, mle, sr_metadata_list, sml_link);

		metadata = (struct sr_metadata *)&mle->sml_metadata;
		mle->sml_chunk_id = metadata->ssdi.ssd_chunk_id;

		/* Handle key disks separately. */
		if (metadata->ssdi.ssd_level == SR_KEYDISK_LEVEL) {
			SLIST_INSERT_HEAD(&kdh, mle, sml_link);
			continue;
		}

		SLIST_FOREACH(vol, &bvh, sbv_link) {
			if (bcmp(&metadata->ssdi.ssd_uuid, &vol->sbv_uuid,
			    sizeof(metadata->ssdi.ssd_uuid)) == 0)
				break;
		}

		if (vol == NULL) {
			vol = malloc(sizeof(struct sr_boot_volume),
			    M_DEVBUF, M_NOWAIT | M_CANFAIL | M_ZERO);
			if (vol == NULL) {
				printf("%s: failed to allocate boot volume!\n",
				    DEVNAME(sc));
				goto unwind;
			}

			vol->sbv_level = metadata->ssdi.ssd_level;
			vol->sbv_volid = metadata->ssdi.ssd_volid;
			vol->sbv_chunk_no = metadata->ssdi.ssd_chunk_no;
			bcopy(&metadata->ssdi.ssd_uuid, &vol->sbv_uuid,
			    sizeof(metadata->ssdi.ssd_uuid));
			SLIST_INIT(&vol->sml);

			/* Maintain volume order. */
			vp2 = NULL;
			SLIST_FOREACH(vp1, &bvh, sbv_link) {
				if (vp1->sbv_volid > vol->sbv_volid)
					break;
				vp2 = vp1;
			}
			if (vp2 == NULL) {
				DNPRINTF(SR_D_META, "%s: insert volume %u "
				    "at head\n", DEVNAME(sc), vol->sbv_volid);
				SLIST_INSERT_HEAD(&bvh, vol, sbv_link);
			} else {
				DNPRINTF(SR_D_META, "%s: insert volume %u "
				    "after %u\n", DEVNAME(sc), vol->sbv_volid,
				    vp2->sbv_volid);
				SLIST_INSERT_AFTER(vp2, vol, sbv_link);
			}
		}

		/* Maintain chunk order. */
		mle2 = NULL;
		SLIST_FOREACH(mle1, &vol->sml, sml_link) {
			if (mle1->sml_chunk_id > mle->sml_chunk_id)
				break;
			mle2 = mle1;
		}
		if (mle2 == NULL) {
			DNPRINTF(SR_D_META, "%s: volume %u insert chunk %u "
			    "at head\n", DEVNAME(sc), vol->sbv_volid,
			    mle->sml_chunk_id);
			SLIST_INSERT_HEAD(&vol->sml, mle, sml_link);
		} else {
			DNPRINTF(SR_D_META, "%s: volume %u insert chunk %u "
			    "after %u\n", DEVNAME(sc), vol->sbv_volid,
			    mle->sml_chunk_id, mle2->sml_chunk_id);
			SLIST_INSERT_AFTER(mle2, mle, sml_link);
		}

		vol->sbv_dev_no++;
	}

	/* Allocate memory for device and ondisk version arrays. */
	devs = malloc(BIOC_CRMAXLEN * sizeof(dev_t), M_DEVBUF,
	    M_NOWAIT | M_CANFAIL);
	if (devs == NULL) {
		printf("%s: failed to allocate device array\n", DEVNAME(sc));
		goto unwind;
	}
	ondisk = malloc(BIOC_CRMAXLEN * sizeof(u_int64_t), M_DEVBUF,
	    M_NOWAIT | M_CANFAIL);
	if (ondisk == NULL) {
		printf("%s: failed to allocate ondisk array\n", DEVNAME(sc));
		goto unwind;
	}

	/*
	 * Assemble hotspare "volumes".
	 */
	SLIST_FOREACH(vol, &bvh, sbv_link) {

		/* Check if this is a hotspare "volume". */
		if (vol->sbv_level != SR_HOTSPARE_LEVEL ||
		    vol->sbv_chunk_no != 1)
			continue;

#ifdef SR_DEBUG
		DNPRINTF(SR_D_META, "%s: assembling hotspare volume ",
		    DEVNAME(sc));
		if (sr_debug & SR_D_META)
			sr_uuid_print(&vol->sbv_uuid, 0);
		DNPRINTF(SR_D_META, " volid %u with %u chunks\n",
		    vol->sbv_volid, vol->sbv_chunk_no);
#endif
	
		/* Create hotspare chunk metadata. */
		hotspare = malloc(sizeof(struct sr_chunk), M_DEVBUF,
		    M_NOWAIT | M_CANFAIL | M_ZERO);
		if (hotspare == NULL) {
			printf("%s: failed to allocate hotspare\n",
			    DEVNAME(sc));
			goto unwind;
		}

		mle = SLIST_FIRST(&vol->sml);
		sr_meta_getdevname(sc, mle->sml_mm, devname, sizeof(devname));
		hotspare->src_dev_mm = mle->sml_mm;
		hotspare->src_vn = mle->sml_vn;
		strlcpy(hotspare->src_devname, devname,
		    sizeof(hotspare->src_devname));
		hotspare->src_size = metadata->ssdi.ssd_size;

		hm = &hotspare->src_meta;
		hm->scmi.scm_volid = SR_HOTSPARE_VOLID;
		hm->scmi.scm_chunk_id = 0;
		hm->scmi.scm_size = metadata->ssdi.ssd_size;
		hm->scmi.scm_coerced_size = metadata->ssdi.ssd_size;
		strlcpy(hm->scmi.scm_devname, devname,
		    sizeof(hm->scmi.scm_devname));
		bcopy(&metadata->ssdi.ssd_uuid, &hm->scmi.scm_uuid,
		    sizeof(struct sr_uuid));

		sr_checksum(sc, hm, &hm->scm_checksum,
		    sizeof(struct sr_meta_chunk_invariant));

		hm->scm_status = BIOC_SDHOTSPARE;

		/* Add chunk to hotspare list. */
		rw_enter_write(&sc->sc_hs_lock);
		cl = &sc->sc_hotspare_list;
		if (SLIST_EMPTY(cl))
			SLIST_INSERT_HEAD(cl, hotspare, src_link);
		else {
			SLIST_FOREACH(chunk, cl, src_link)
				last = chunk;
			SLIST_INSERT_AFTER(last, hotspare, src_link);
		}
		sc->sc_hotspare_no++;
		rw_exit_write(&sc->sc_hs_lock);

	}

	/*
	 * Assemble RAID volumes.
	 */
	SLIST_FOREACH(vol, &bvh, sbv_link) {

		bzero(&bc, sizeof(bc));

		/* Check if this is a hotspare "volume". */
		if (vol->sbv_level == SR_HOTSPARE_LEVEL &&
		    vol->sbv_chunk_no == 1)
			continue;

#ifdef SR_DEBUG
		DNPRINTF(SR_D_META, "%s: assembling volume ", DEVNAME(sc));
		if (sr_debug & SR_D_META)
			sr_uuid_print(&vol->sbv_uuid, 0);
		DNPRINTF(SR_D_META, " volid %u with %u chunks\n",
		    vol->sbv_volid, vol->sbv_chunk_no);
#endif

		/*
		 * If this is a crypto volume, try to find a matching
		 * key disk...
		 */
		bc.bc_key_disk = NODEV;
		if (vol->sbv_level == 'C') {
			SLIST_FOREACH(mle, &kdh, sml_link) {
				metadata =
				    (struct sr_metadata *)&mle->sml_metadata;
				if (bcmp(&metadata->ssdi.ssd_uuid,
				    &vol->sbv_uuid,
				    sizeof(metadata->ssdi.ssd_uuid)) == 0) {
					bc.bc_key_disk = mle->sml_mm;
				}
			}
		}

		for (i = 0; i < BIOC_CRMAXLEN; i++) {
			devs[i] = NODEV; /* mark device as illegal */
			ondisk[i] = 0;
		}

		SLIST_FOREACH(mle, &vol->sml, sml_link) {
			metadata = (struct sr_metadata *)&mle->sml_metadata;
			chunk_id = metadata->ssdi.ssd_chunk_id;

			if (devs[chunk_id] != NODEV) {
				vol->sbv_dev_no--;
				sr_meta_getdevname(sc, mle->sml_mm, devname,
				    sizeof(devname));
				printf("%s: found duplicate chunk %u for "
				    "volume %u on device %s\n", DEVNAME(sc),
				    chunk_id, vol->sbv_volid, devname);
			}

			if (devs[chunk_id] == NODEV ||
			    metadata->ssd_ondisk > ondisk[chunk_id]) {
				devs[chunk_id] = mle->sml_mm;
				ondisk[chunk_id] = metadata->ssd_ondisk;
				DNPRINTF(SR_D_META, "%s: using ondisk "
				    "metadata version %llu for chunk %u\n",
				    DEVNAME(sc), ondisk[chunk_id], chunk_id);
			}
		}

		if (vol->sbv_chunk_no != vol->sbv_dev_no) {
			printf("%s: not all chunks were provided; "
			    "attempting to bring volume %d online\n",
			    DEVNAME(sc), vol->sbv_volid);
		}

		bc.bc_level = vol->sbv_level;
		bc.bc_dev_list_len = vol->sbv_chunk_no * sizeof(dev_t);
		bc.bc_dev_list = devs;
		bc.bc_flags = BIOC_SCDEVT;

		rw_enter_write(&sc->sc_lock);
		sr_ioctl_createraid(sc, &bc, 0);
		rw_exit_write(&sc->sc_lock);

		rv++;
	}

	/* done with metadata */
unwind:
	for (vp1 = SLIST_FIRST(&bvh); vp1 != SLIST_END(&bvh); vp1 = vp2) {
		vp2 = SLIST_NEXT(vp1, sbv_link);
		for (mle1 = SLIST_FIRST(&vp1->sml);
		    mle1 != SLIST_END(&vp1->sml); mle1 = mle2) {
			mle2 = SLIST_NEXT(mle1, sml_link);
			free(mle1, M_DEVBUF);
		}
		free(vp1, M_DEVBUF);
	}
	for (mle = SLIST_FIRST(&mlh); mle != SLIST_END(&mlh); mle = mle2) {
		mle2 = SLIST_NEXT(mle, sml_link);
		free(mle, M_DEVBUF);
	}
	SLIST_INIT(&mlh);

	if (devs)
		free(devs, M_DEVBUF);
	if (ondisk)
		free(ondisk, M_DEVBUF);

	return (rv);
}

int
sr_meta_native_probe(struct sr_softc *sc, struct sr_chunk *ch_entry)
{
	struct disklabel	label;
	char			*devname;
	int			error, part;
	daddr64_t		size;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_probe(%s)\n",
	   DEVNAME(sc), ch_entry->src_devname);

	devname = ch_entry->src_devname;
	part = DISKPART(ch_entry->src_dev_mm);

	/* get disklabel */
	error = VOP_IOCTL(ch_entry->src_vn, DIOCGDINFO, (caddr_t)&label, FREAD,
	    NOCRED, 0);
	if (error) {
		DNPRINTF(SR_D_META, "%s: %s can't obtain disklabel\n",
		    DEVNAME(sc), devname);
		goto unwind;
	}

	/* make sure the partition is of the right type */
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		DNPRINTF(SR_D_META,
		    "%s: %s partition not of type RAID (%d)\n", DEVNAME(sc),
		    devname,
		    label.d_partitions[part].p_fstype);
		goto unwind;
	}

	size = DL_GETPSIZE(&label.d_partitions[part]) -
	    SR_META_SIZE - SR_META_OFFSET;
	if (size <= 0) {
		DNPRINTF(SR_D_META, "%s: %s partition too small\n", DEVNAME(sc),
		    devname);
		goto unwind;
	}
	ch_entry->src_size = size;

	DNPRINTF(SR_D_META, "%s: probe found %s size %d\n", DEVNAME(sc),
	    devname, size);

	return (SR_META_F_NATIVE);
unwind:
	DNPRINTF(SR_D_META, "%s: invalid device: %s\n", DEVNAME(sc),
	    devname ? devname : "nodev");
	return (SR_META_F_INVALID);
}

int
sr_meta_native_attach(struct sr_discipline *sd, int force)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head 	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_metadata	*md = NULL;
	struct sr_chunk		*ch_entry, *ch_next;
	struct sr_uuid		uuid;
	u_int64_t		version = 0;
	int			sr, not_sr, rv = 1, d, expected = -1, old_meta = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_attach\n", DEVNAME(sc));

	md = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO);
	if (md == NULL) {
		printf("%s: not enough memory for metadata buffer\n",
		    DEVNAME(sc));
		goto bad;
	}

	bzero(&uuid, sizeof uuid);

	sr = not_sr = d = 0;
	SLIST_FOREACH(ch_entry, cl, src_link) {
		if (ch_entry->src_dev_mm == NODEV)
			continue;

		if (sr_meta_native_read(sd, ch_entry->src_dev_mm, md, NULL)) {
			printf("%s: could not read native metadata\n",
			    DEVNAME(sc));
			goto bad;
		}

		if (md->ssdi.ssd_magic == SR_MAGIC) {
			sr++;
			if (d == 0) {
				bcopy(&md->ssdi.ssd_uuid, &uuid, sizeof uuid);
				expected = md->ssdi.ssd_chunk_no;
				version = md->ssd_ondisk;
				d++;
				continue;
			} else if (bcmp(&md->ssdi.ssd_uuid, &uuid,
			    sizeof uuid)) {
				printf("%s: not part of the same volume\n",
				    DEVNAME(sc));
				goto bad;
			}
			if (md->ssd_ondisk != version) {
				old_meta++;
				version = MAX(md->ssd_ondisk, version);
			}
		} else
			not_sr++;
	}

	if (sr && not_sr) {
		printf("%s: not all chunks are of the native metadata format\n",
		     DEVNAME(sc));
		goto bad;
	}

	/* mixed metadata versions; mark bad disks offline */
	if (old_meta) {
		d = 0;
		for (ch_entry = SLIST_FIRST(cl); ch_entry != SLIST_END(cl);
		    ch_entry = ch_next, d++) {
			ch_next = SLIST_NEXT(ch_entry, src_link);

			/* XXX do we want to read this again? */
			if (ch_entry->src_dev_mm == NODEV)
				panic("src_dev_mm == NODEV");
			if (sr_meta_native_read(sd, ch_entry->src_dev_mm, md,
			    NULL))
				printf("%s: could not read native metadata\n",
				    DEVNAME(sc));
			if (md->ssd_ondisk != version)
				sd->sd_vol.sv_chunks[d]->src_meta.scm_status =
				    BIOC_SDOFFLINE;
		}
	}

	if (expected != sr && !force && expected != -1) {
		DNPRINTF(SR_D_META, "%s: not all chunks were provided, trying "
		    "anyway\n", DEVNAME(sc));
	}

	rv = 0;
bad:
	if (md)
		free(md, M_DEVBUF);
	return (rv);
}

int
sr_meta_native_read(struct sr_discipline *sd, dev_t dev,
    struct sr_metadata *md, void *fm)
{
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	DNPRINTF(SR_D_META, "%s: sr_meta_native_read(0x%x, %p)\n",
	    DEVNAME(sc), dev, md);

	return (sr_meta_rw(sd, dev, md, SR_META_SIZE * 512, SR_META_OFFSET,
	    B_READ));
}

int
sr_meta_native_write(struct sr_discipline *sd, dev_t dev,
    struct sr_metadata *md, void *fm)
{
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	DNPRINTF(SR_D_META, "%s: sr_meta_native_write(0x%x, %p)\n",
	    DEVNAME(sc), dev, md);

	return (sr_meta_rw(sd, dev, md, SR_META_SIZE * 512, SR_META_OFFSET,
	    B_WRITE));
}

void
sr_hotplug_register(struct sr_discipline *sd, void *func)
{
	struct sr_hotplug_list	*mhe;

	DNPRINTF(SR_D_MISC, "%s: sr_hotplug_register: %p\n",
	    DEVNAME(sd->sd_sc), func);

	/* make sure we aren't on the list yet */
	SLIST_FOREACH(mhe, &sr_hotplug_callbacks, shl_link)
		if (mhe->sh_hotplug == func)
			return;

	mhe = malloc(sizeof(struct sr_hotplug_list), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	mhe->sh_hotplug = func;
	mhe->sh_sd = sd;
	SLIST_INSERT_HEAD(&sr_hotplug_callbacks, mhe, shl_link);
}

void
sr_hotplug_unregister(struct sr_discipline *sd, void *func)
{
	struct sr_hotplug_list	*mhe;

	DNPRINTF(SR_D_MISC, "%s: sr_hotplug_unregister: %s %p\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname, func);

	/* make sure we are on the list yet */
	SLIST_FOREACH(mhe, &sr_hotplug_callbacks, shl_link)
		if (mhe->sh_hotplug == func) {
			SLIST_REMOVE(&sr_hotplug_callbacks, mhe,
			    sr_hotplug_list, shl_link);
			free(mhe, M_DEVBUF);
			if (SLIST_EMPTY(&sr_hotplug_callbacks))
				SLIST_INIT(&sr_hotplug_callbacks);
			return;
		}
}

void
sr_disk_attach(struct disk *diskp, int action)
{
	struct sr_hotplug_list	*mhe;

	SLIST_FOREACH(mhe, &sr_hotplug_callbacks, shl_link)
		if (mhe->sh_sd->sd_ready)
			mhe->sh_hotplug(mhe->sh_sd, diskp, action);
}

int
sr_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
sr_attach(struct device *parent, struct device *self, void *aux)
{
	struct sr_softc		*sc = (void *)self;

	DNPRINTF(SR_D_MISC, "\n%s: sr_attach", DEVNAME(sc));

	rw_init(&sc->sc_lock, "sr_lock");
	rw_init(&sc->sc_hs_lock, "sr_hs_lock");

	SLIST_INIT(&sr_hotplug_callbacks);
	SLIST_INIT(&sc->sc_hotspare_list);

	if (bio_register(&sc->sc_dev, sr_ioctl) != 0)
		printf("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = sr_ioctl;

	printf("\n");

	softraid_disk_attach = sr_disk_attach;

	sr_boot_assembly(sc);
}

int
sr_detach(struct device *self, int flags)
{
	return (0);
}

int
sr_activate(struct device *self, int act)
{
	return (1);
}

void
sr_minphys(struct buf *bp, struct scsi_link *sl)
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
	size_t			copy_cnt;

	DNPRINTF(SR_D_MISC, "sr_copy_internal_data xs: %p size: %d\n",
	    xs, size);

	if (xs->datalen) {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(v, xs->data, copy_cnt);
	}
}

int
sr_ccb_alloc(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;
	int			i;

	if (!sd)
		return (1);

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_alloc\n", DEVNAME(sd->sd_sc));

	if (sd->sd_ccb)
		return (1);

	sd->sd_ccb = malloc(sizeof(struct sr_ccb) *
	    sd->sd_max_wu * sd->sd_max_ccb_per_wu, M_DEVBUF, M_WAITOK | M_ZERO);
	TAILQ_INIT(&sd->sd_ccb_freeq);
	for (i = 0; i < sd->sd_max_wu * sd->sd_max_ccb_per_wu; i++) {
		ccb = &sd->sd_ccb[i];
		ccb->ccb_dis = sd;
		sr_ccb_put(ccb);
	}

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_alloc ccb: %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_max_wu * sd->sd_max_ccb_per_wu);

	return (0);
}

void
sr_ccb_free(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;

	if (!sd)
		return;

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_free %p\n", DEVNAME(sd->sd_sc), sd);

	while ((ccb = TAILQ_FIRST(&sd->sd_ccb_freeq)) != NULL)
		TAILQ_REMOVE(&sd->sd_ccb_freeq, ccb, ccb_link);

	if (sd->sd_ccb)
		free(sd->sd_ccb, M_DEVBUF);
}

struct sr_ccb *
sr_ccb_get(struct sr_discipline *sd)
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

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_get: %p\n", DEVNAME(sd->sd_sc),
	    ccb);

	return (ccb);
}

void
sr_ccb_put(struct sr_ccb *ccb)
{
	struct sr_discipline	*sd = ccb->ccb_dis;
	int			s;

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_put: %p\n", DEVNAME(sd->sd_sc),
	    ccb);

	s = splbio();

	ccb->ccb_wu = NULL;
	ccb->ccb_state = SR_CCB_FREE;
	ccb->ccb_target = -1;
	ccb->ccb_opaque = NULL;

	TAILQ_INSERT_TAIL(&sd->sd_ccb_freeq, ccb, ccb_link);

	splx(s);
}

int
sr_wu_alloc(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;
	int			i, no_wu;

	if (!sd)
		return (1);

	DNPRINTF(SR_D_WU, "%s: sr_wu_alloc %p %d\n", DEVNAME(sd->sd_sc),
	    sd, sd->sd_max_wu);

	if (sd->sd_wu)
		return (1);

	no_wu = sd->sd_max_wu;
	sd->sd_wu_pending = no_wu;

	sd->sd_wu = malloc(sizeof(struct sr_workunit) * no_wu,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	TAILQ_INIT(&sd->sd_wu_freeq);
	TAILQ_INIT(&sd->sd_wu_pendq);
	TAILQ_INIT(&sd->sd_wu_defq);
	for (i = 0; i < no_wu; i++) {
		wu = &sd->sd_wu[i];
		wu->swu_dis = sd;
		sr_wu_put(wu);
	}

	return (0);
}

void
sr_wu_free(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;

	if (!sd)
		return;

	DNPRINTF(SR_D_WU, "%s: sr_wu_free %p\n", DEVNAME(sd->sd_sc), sd);

	while ((wu = TAILQ_FIRST(&sd->sd_wu_freeq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);
	while ((wu = TAILQ_FIRST(&sd->sd_wu_pendq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_pendq, wu, swu_link);
	while ((wu = TAILQ_FIRST(&sd->sd_wu_defq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_defq, wu, swu_link);

	if (sd->sd_wu)
		free(sd->sd_wu, M_DEVBUF);
}

void
sr_wu_put(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct sr_ccb		*ccb;

	int			s;

	DNPRINTF(SR_D_WU, "%s: sr_wu_put: %p\n", DEVNAME(sd->sd_sc), wu);

	s = splbio();

	wu->swu_xs = NULL;
	wu->swu_state = SR_WU_FREE;
	wu->swu_ios_complete = 0;
	wu->swu_ios_failed = 0;
	wu->swu_ios_succeeded = 0;
	wu->swu_io_count = 0;
	wu->swu_blk_start = 0;
	wu->swu_blk_end = 0;
	wu->swu_collider = NULL;
	wu->swu_fake = 0;
	wu->swu_flags = 0;

	while ((ccb = TAILQ_FIRST(&wu->swu_ccb)) != NULL) {
		TAILQ_REMOVE(&wu->swu_ccb, ccb, ccb_link);
		sr_ccb_put(ccb);
	}
	TAILQ_INIT(&wu->swu_ccb);

	TAILQ_INSERT_TAIL(&sd->sd_wu_freeq, wu, swu_link);
	sd->sd_wu_pending--;

	/* wake up sleepers */
#ifdef DIAGNOSTIC
	if (sd->sd_wu_sleep < 0)
		panic("negative wu sleepers");
#endif /* DIAGNOSTIC */
	if (sd->sd_wu_sleep)
		wakeup(&sd->sd_wu_sleep);

	splx(s);
}

struct sr_workunit *
sr_wu_get(struct sr_discipline *sd, int canwait)
{
	struct sr_workunit	*wu;
	int			s;

	s = splbio();

	for (;;) {
		wu = TAILQ_FIRST(&sd->sd_wu_freeq);
		if (wu) {
			TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);
			wu->swu_state = SR_WU_INPROGRESS;
			sd->sd_wu_pending++;
			break;
		} else if (wu == NULL && canwait) {
			sd->sd_wu_sleep++;
			tsleep(&sd->sd_wu_sleep, PRIBIO, "sr_wu_get", 0);
			sd->sd_wu_sleep--;
		} else
			break;
	}

	splx(s);

	DNPRINTF(SR_D_WU, "%s: sr_wu_get: %p\n", DEVNAME(sd->sd_sc), wu);

	return (wu);
}

void
sr_scsi_done(struct sr_discipline *sd, struct scsi_xfer *xs)
{
	int			s;

	DNPRINTF(SR_D_DIS, "%s: sr_scsi_done: xs %p\n", DEVNAME(sd->sd_sc), xs);

	s = splbio();
	scsi_done(xs);
	splx(s);
}

int
sr_scsi_cmd(struct scsi_xfer *xs)
{
	int			s;
	struct scsi_link	*link = xs->sc_link;
	struct sr_softc		*sc = link->adapter_softc;
	struct sr_workunit	*wu = NULL;
	struct sr_discipline	*sd;

	DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: scsibus%d xs: %p "
	    "flags: %#x\n", DEVNAME(sc), link->scsibus, xs, xs->flags);

	sd = sc->sc_dis[link->scsibus];
	if (sd == NULL) {
		s = splhigh();
		sd = sc->sc_attach_dis;
		splx(s);

		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: attaching %p\n",
		    DEVNAME(sc), sd);
		if (sd == NULL) {
			printf("%s: sr_scsi_cmd NULL discipline\n",
			    DEVNAME(sc));
			goto stuffup;
		}
	}

	if (sd->sd_deleted) {
		printf("%s: %s device is being deleted, failing io\n",
		    DEVNAME(sc), sd->sd_meta->ssd_devname);
		goto stuffup;
	}

	/*
	 * we'll let the midlayer deal with stalls instead of being clever
	 * and sending sr_wu_get !(xs->flags & SCSI_NOSLEEP) in cansleep
	 */
	if ((wu = sr_wu_get(sd, 0)) == NULL) {
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd no wu\n", DEVNAME(sc));
		return (NO_CCB);
	}

	xs->error = XS_NOERROR;
	wu->swu_xs = xs;

	/* the midlayer will query LUNs so report sense to stop scanning */
	if (link->target != 0 || link->lun != 0) {
		DNPRINTF(SR_D_CMD, "%s: bad target:lun %d:%d\n",
		    DEVNAME(sc), link->target, link->lun);
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT |
		    SSD_ERRCODE_VALID;
		sd->sd_scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
		sd->sd_scsi_sense.add_sense_code = 0x25;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		sd->sd_scsi_sense.extra_len = 4;
		goto stuffup;
	}

	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case READ_16:
	case WRITE_COMMAND:
	case WRITE_BIG:
	case WRITE_16:
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
	case READ_CAPACITY_16:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd READ CAPACITY 0x%02x\n",
		    DEVNAME(sc), xs->cmd->opcode);
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
	if (sd && sd->sd_scsi_sense.error_code) {
		xs->error = XS_SENSE;
		bcopy(&sd->sd_scsi_sense, &xs->sense, sizeof(xs->sense));
		bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));
	} else {
		xs->error = XS_DRIVER_STUFFUP;
	}
complete:
	if (wu)
		sr_wu_put(wu);
	sr_scsi_done(sd, xs);
	return (COMPLETE);
}
int
sr_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	DNPRINTF(SR_D_IOCTL, "%s: sr_scsi_ioctl cmd: %#x\n",
	    DEVNAME((struct sr_softc *)link->adapter_softc), cmd);

	return (sr_ioctl(link->adapter_softc, cmd, addr));
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
		rv = sr_ioctl_setstate(sc, (struct bioc_setstate *)addr);
		break;

	case BIOCCREATERAID:
		DNPRINTF(SR_D_IOCTL, "createraid\n");
		rv = sr_ioctl_createraid(sc, (struct bioc_createraid *)addr, 1);
		break;

	case BIOCDELETERAID:
		rv = sr_ioctl_deleteraid(sc, (struct bioc_deleteraid *)addr);
		break;

	case BIOCDISCIPLINE:
		rv = sr_ioctl_discipline(sc, (struct bioc_discipline *)addr);
		break;

	default:
		DNPRINTF(SR_D_IOCTL, "invalid ioctl\n");
		rv = ENOTTY;
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
			disk += sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no;
		}

	strlcpy(bi->bi_dev, sc->sc_dev.dv_xname, sizeof(bi->bi_dev));
	bi->bi_novol = vol + sc->sc_hotspare_no;
	bi->bi_nodisk = disk + sc->sc_hotspare_no;

	return (0);
}

int
sr_ioctl_vol(struct sr_softc *sc, struct bioc_vol *bv)
{
	int			i, vol, rv = EINVAL;
	struct sr_discipline	*sd;
	struct sr_chunk		*hotspare;
	daddr64_t		rb, sz;

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bv->bv_volid)
			continue;

		sd = sc->sc_dis[i];
		bv->bv_status = sd->sd_vol_status;
		bv->bv_size = sd->sd_meta->ssdi.ssd_size << DEV_BSHIFT;
		bv->bv_level = sd->sd_meta->ssdi.ssd_level;
		bv->bv_nodisk = sd->sd_meta->ssdi.ssd_chunk_no;

#ifdef CRYPTO
		if (sd->sd_meta->ssdi.ssd_level == 'C' &&
		    sd->mds.mdd_crypto.key_disk != NULL)
			bv->bv_nodisk++;
#endif

		if (bv->bv_status == BIOC_SVREBUILD) {
			sz = sd->sd_meta->ssdi.ssd_size;
			rb = sd->sd_meta->ssd_rebuild;
			if (rb > 0)
				bv->bv_percent = 100 -
				    ((sz * 100 - rb * 100) / sz) - 1;
			else
				bv->bv_percent = 0;
		}
		strlcpy(bv->bv_dev, sd->sd_meta->ssd_devname,
		    sizeof(bv->bv_dev));
		strlcpy(bv->bv_vendor, sd->sd_meta->ssdi.ssd_vendor,
		    sizeof(bv->bv_vendor));
		rv = 0;
		goto done;
	}

	/* Check hotspares list. */
	SLIST_FOREACH(hotspare, &sc->sc_hotspare_list, src_link) {
		vol++;
		if (vol != bv->bv_volid)
			continue;

		bv->bv_status = BIOC_SVONLINE;
		bv->bv_size = hotspare->src_meta.scmi.scm_size << DEV_BSHIFT;
		bv->bv_level = -1;	/* Hotspare. */
		bv->bv_nodisk = 1;
		strlcpy(bv->bv_dev, hotspare->src_meta.scmi.scm_devname,
		    sizeof(bv->bv_dev));
		strlcpy(bv->bv_vendor, hotspare->src_meta.scmi.scm_devname,
		    sizeof(bv->bv_vendor));
		rv = 0;
		goto done;
	}

done:
	return (rv);
}

int
sr_ioctl_disk(struct sr_softc *sc, struct bioc_disk *bd)
{
	int			i, vol, rv = EINVAL, id;
	struct sr_chunk		*src, *hotspare;

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bd->bd_volid)
			continue;

		id = bd->bd_diskid;

		if (id < sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no)
			src = sc->sc_dis[i]->sd_vol.sv_chunks[id];
#ifdef CRYPTO
		else if (id == sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no &&
		    sc->sc_dis[i]->sd_meta->ssdi.ssd_level == 'C' &&
		    sc->sc_dis[i]->mds.mdd_crypto.key_disk != NULL)
			src = sc->sc_dis[i]->mds.mdd_crypto.key_disk;
#endif
		else
			break;

		bd->bd_status = src->src_meta.scm_status;
		bd->bd_size = src->src_meta.scmi.scm_size << DEV_BSHIFT;
		bd->bd_channel = vol;
		bd->bd_target = id;
		strlcpy(bd->bd_vendor, src->src_meta.scmi.scm_devname,
		    sizeof(bd->bd_vendor));
		rv = 0;
		goto done;
	}

	/* Check hotspares list. */
	SLIST_FOREACH(hotspare, &sc->sc_hotspare_list, src_link) {
		vol++;
		if (vol != bd->bd_volid)
			continue;

		if (bd->bd_diskid != 0)
			break;

		bd->bd_status = hotspare->src_meta.scm_status;
		bd->bd_size = hotspare->src_meta.scmi.scm_size << DEV_BSHIFT;
		bd->bd_channel = vol;
		bd->bd_target = bd->bd_diskid;
		strlcpy(bd->bd_vendor, hotspare->src_meta.scmi.scm_devname,
		    sizeof(bd->bd_vendor));
		rv = 0;
		goto done;
	}

done:
	return (rv);
}

int
sr_ioctl_setstate(struct sr_softc *sc, struct bioc_setstate *bs)
{
	int			rv = EINVAL;
	int			i, vol, found, c;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*ch_entry;
	struct sr_chunk_head	*cl;

	if (bs->bs_other_id_type == BIOC_SSOTHER_UNUSED)
		goto done;

	if (bs->bs_status == BIOC_SSHOTSPARE) {
		rv = sr_hotspare(sc, (dev_t)bs->bs_other_id);
		goto done;
	}

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bs->bs_volid)
			continue;
		sd = sc->sc_dis[i];
		break;
	}
	if (sd == NULL)
		goto done;

	switch (bs->bs_status) {
	case BIOC_SSOFFLINE:
		/* Take chunk offline */
		found = c = 0;
		cl = &sd->sd_vol.sv_chunk_list;
		SLIST_FOREACH(ch_entry, cl, src_link) {
			if (ch_entry->src_dev_mm == bs->bs_other_id) {
				found = 1;
				break;
			}
			c++;
		}
		if (found == 0) {
			printf("%s: chunk not part of array\n", DEVNAME(sc));
			goto done;
		}
		
		/* XXX: check current state first */
		sd->sd_set_chunk_state(sd, c, BIOC_SSOFFLINE);
		
		if (sr_meta_save(sd, SR_META_DIRTY)) {
			printf("%s: could not save metadata to %s\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);
			goto done;
		}
		rv = 0;
		break;

	case BIOC_SDSCRUB:
		break;

	case BIOC_SSREBUILD:
		rv = sr_rebuild_init(sd, (dev_t)bs->bs_other_id);
		break;

	default:
		printf("%s: unsupported state request %d\n",
		    DEVNAME(sc), bs->bs_status);
	}

done:
	return (rv);
}

int
sr_chunk_in_use(struct sr_softc *sc, dev_t dev)
{
	struct sr_discipline	*sd;
	struct sr_chunk		*chunk;
	int			i, c;

	/* See if chunk is already in use. */
	for (i = 0; i < SR_MAXSCSIBUS; i++) {
		if (!sc->sc_dis[i])
			continue;
		sd = sc->sc_dis[i];
		for (c = 0; c < sd->sd_meta->ssdi.ssd_chunk_no; c++) {
			chunk = sd->sd_vol.sv_chunks[c];
			if (chunk->src_dev_mm == dev)
				return chunk->src_meta.scm_status;
		}
	}

	/* Check hotspares list. */
	SLIST_FOREACH(chunk, &sc->sc_hotspare_list, src_link)
		if (chunk->src_dev_mm == dev)
			return chunk->src_meta.scm_status;

	return BIOC_SDINVALID;
}

int
sr_hotspare(struct sr_softc *sc, dev_t dev)
{
	struct sr_discipline	*sd = NULL;
	struct sr_metadata	*sm = NULL;
	struct sr_meta_chunk    *hm;
	struct sr_chunk_head	*cl;
	struct sr_chunk		*hotspare, *chunk, *last;
	struct sr_uuid		uuid;
	struct disklabel	label;
	struct vnode		*vn;
	daddr64_t		size;
	char			devname[32];
	int			rv = EINVAL;
	int			c, part, open = 0;

	/*
	 * Add device to global hotspares list.
	 */

	sr_meta_getdevname(sc, dev, devname, sizeof(devname));

	/* Make sure chunk is not already in use. */
	c = sr_chunk_in_use(sc, dev);
	if (c != BIOC_SDINVALID && c != BIOC_SDOFFLINE) {
		if (c == BIOC_SDHOTSPARE)
			printf("%s: %s is already a hotspare\n",
			    DEVNAME(sc), devname);
		else
			printf("%s: %s is already in use\n",
			    DEVNAME(sc), devname);
		goto done;
	}

	/* XXX - See if there is an existing degraded volume... */

	/* Open device. */
	if (bdevvp(dev, &vn)) {
		printf("%s:, sr_hotspare: can't allocate vnode\n", DEVNAME(sc));
		goto done;
	}
	if (VOP_OPEN(vn, FREAD | FWRITE, NOCRED, 0)) {
		DNPRINTF(SR_D_META,"%s: sr_hotspare cannot open %s\n",
		    DEVNAME(sc), devname);
		vput(vn);
		goto fail;
	}
	open = 1; /* close dev on error */

	/* Get partition details. */
	part = DISKPART(dev);
	if (VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)&label, FREAD, NOCRED, 0)) {
		DNPRINTF(SR_D_META, "%s: sr_hotspare ioctl failed\n",
		    DEVNAME(sc));
		VOP_CLOSE(vn, FREAD | FWRITE, NOCRED, 0);
		vput(vn);
		goto fail;
	}
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		printf("%s: %s partition not of type RAID (%d)\n",
		    DEVNAME(sc), devname,
		    label.d_partitions[part].p_fstype);
		goto fail;
	}

	/* Calculate partition size. */
	size = DL_GETPSIZE(&label.d_partitions[part]) -
	    SR_META_SIZE - SR_META_OFFSET;

	/*
	 * Create and populate chunk metadata.
	 */

	sr_uuid_get(&uuid);
	hotspare = malloc(sizeof(struct sr_chunk), M_DEVBUF, M_WAITOK | M_ZERO);

	hotspare->src_dev_mm = dev;
	hotspare->src_vn = vn;
	strlcpy(hotspare->src_devname, devname, sizeof(hm->scmi.scm_devname));
	hotspare->src_size = size;

	hm = &hotspare->src_meta;
	hm->scmi.scm_volid = SR_HOTSPARE_VOLID;
	hm->scmi.scm_chunk_id = 0;
	hm->scmi.scm_size = size;
	hm->scmi.scm_coerced_size = size;
	strlcpy(hm->scmi.scm_devname, devname, sizeof(hm->scmi.scm_devname));
	bcopy(&uuid, &hm->scmi.scm_uuid, sizeof(struct sr_uuid));

	sr_checksum(sc, hm, &hm->scm_checksum,
	    sizeof(struct sr_meta_chunk_invariant));

	hm->scm_status = BIOC_SDHOTSPARE;

	/*
	 * Create and populate our own discipline and metadata.
	 */

	sm = malloc(sizeof(struct sr_metadata), M_DEVBUF, M_WAITOK | M_ZERO);
	sm->ssdi.ssd_magic = SR_MAGIC;
	sm->ssdi.ssd_version = SR_META_VERSION;
	sm->ssd_ondisk = 0;
	sm->ssdi.ssd_flags = 0;
	bcopy(&uuid, &sm->ssdi.ssd_uuid, sizeof(struct sr_uuid));
	sm->ssdi.ssd_chunk_no = 1;
	sm->ssdi.ssd_volid = SR_HOTSPARE_VOLID;
	sm->ssdi.ssd_level = SR_HOTSPARE_LEVEL;
	sm->ssdi.ssd_size = size;
	strlcpy(sm->ssdi.ssd_vendor, "OPENBSD", sizeof(sm->ssdi.ssd_vendor));
	snprintf(sm->ssdi.ssd_product, sizeof(sm->ssdi.ssd_product),
	    "SR %s", "HOTSPARE");
	snprintf(sm->ssdi.ssd_revision, sizeof(sm->ssdi.ssd_revision),
	    "%03d", SR_META_VERSION);

	sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_WAITOK | M_ZERO);
	sd->sd_sc = sc;
	sd->sd_meta = sm;
	sd->sd_meta_type = SR_META_F_NATIVE;
	sd->sd_vol_status = BIOC_SVONLINE;
	strlcpy(sd->sd_name, "HOTSPARE", sizeof(sd->sd_name));

	/* Add chunk to volume. */
	sd->sd_vol.sv_chunks = malloc(sizeof(struct sr_chunk *), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	sd->sd_vol.sv_chunks[0] = hotspare;
	SLIST_INIT(&sd->sd_vol.sv_chunk_list);
	SLIST_INSERT_HEAD(&sd->sd_vol.sv_chunk_list, hotspare, src_link);

	/* Save metadata. */
	if (sr_meta_save(sd, SR_META_DIRTY)) {
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), devname);
		goto fail;
	}
	
	/*
	 * Add chunk to hotspare list.
	 */
	rw_enter_write(&sc->sc_hs_lock);
	cl = &sc->sc_hotspare_list;
	if (SLIST_EMPTY(cl))
		SLIST_INSERT_HEAD(cl, hotspare, src_link);
	else {
		SLIST_FOREACH(chunk, cl, src_link)
			last = chunk;
		SLIST_INSERT_AFTER(last, hotspare, src_link);
	}
	sc->sc_hotspare_no++;
	rw_exit_write(&sc->sc_hs_lock);

	rv = 0;
	goto done;

fail:
	if (hotspare)
		free(hotspare, M_DEVBUF);

done:
	if (sd && sd->sd_vol.sv_chunks)
		free(sd->sd_vol.sv_chunks, M_DEVBUF);
	if (sd)
		free(sd, M_DEVBUF);
	if (sm)
		free(sm, M_DEVBUF);
	if (open) {
		VOP_CLOSE(vn, FREAD | FWRITE, NOCRED, 0);
		vput(vn);
	}

	return (rv);
}

void
sr_hotspare_rebuild_callback(void *arg1, void *arg2)
{
	sr_hotspare_rebuild((struct sr_discipline *)arg1);
}

void
sr_hotspare_rebuild(struct sr_discipline *sd)
{
	struct sr_chunk_head	*cl;
	struct sr_chunk		*hotspare, *chunk = NULL;
	struct sr_workunit	*wu;
	struct sr_ccb           *ccb;
	int			i, s, chunk_no, busy;

	/*
	 * Attempt to locate a hotspare and initiate rebuild.
	 */

	for (i = 0; i < sd->sd_meta->ssdi.ssd_chunk_no; i++) {
		if (sd->sd_vol.sv_chunks[i]->src_meta.scm_status ==
		    BIOC_SDOFFLINE) {
			chunk_no = i;
			chunk = sd->sd_vol.sv_chunks[i];
			break;
		}
	}

	if (chunk == NULL) {
		printf("%s: no offline chunk found on %s!\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname);
		return;
	}

	/* See if we have a suitable hotspare... */
	rw_enter_write(&sd->sd_sc->sc_hs_lock);
	cl = &sd->sd_sc->sc_hotspare_list;
	SLIST_FOREACH(hotspare, cl, src_link)
		if (hotspare->src_size >= chunk->src_size)
			break;

	if (hotspare != NULL) {

		printf("%s: %s volume degraded, will attempt to "
		    "rebuild on hotspare %s\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname, hotspare->src_devname);

		/*
		 * Ensure that all pending I/O completes on the failed chunk
		 * before trying to initiate a rebuild.
		 */
		i = 0;
		do {
			busy = 0;

			s = splbio();
			TAILQ_FOREACH(wu, &sd->sd_wu_pendq, swu_link) {
        			TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link) {
                			if (ccb->ccb_target == chunk_no)
						busy = 1;
				}
			}
			TAILQ_FOREACH(wu, &sd->sd_wu_defq, swu_link) {
        			TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link) {
                			if (ccb->ccb_target == chunk_no)
						busy = 1;
				}
			}
			splx(s);

			if (busy) {
				tsleep(sd, PRIBIO, "sr_hotspare", hz);
				i++;
			}

		} while (busy && i < 120);

		DNPRINTF(SR_D_META, "%s: waited %i seconds for I/O to "
		    "complete on failed chunk %s\n", DEVNAME(sd->sd_sc),
		    i, chunk->src_devname);

		if (busy) {
			printf("%s: pending I/O failed to complete on "
			    "failed chunk %s, hotspare rebuild aborted...\n",
			    DEVNAME(sd->sd_sc), chunk->src_devname);
			goto done;
		}

		s = splbio();
		rw_enter_write(&sd->sd_sc->sc_lock);
		if (sr_rebuild_init(sd, hotspare->src_dev_mm) == 0) {

			/* Remove hotspare from available list. */
			sd->sd_sc->sc_hotspare_no--;
			SLIST_REMOVE(cl, hotspare, sr_chunk, src_link);
			free(hotspare, M_DEVBUF);

		}
		rw_exit_write(&sd->sd_sc->sc_lock);
		splx(s);
	}
done:
	rw_exit_write(&sd->sd_sc->sc_hs_lock);
}

int
sr_rebuild_init(struct sr_discipline *sd, dev_t dev)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			rv = EINVAL, part;
	int			c, found, open = 0;
	char			devname[32];
	struct vnode		*vn;
	daddr64_t		size, csize;
	struct disklabel	label;
	struct sr_meta_chunk	*old, *new;

	/*
	 * Attempt to initiate a rebuild onto the specified device.
	 */

	if (!(sd->sd_capabilities & SR_CAP_REBUILD)) {
		printf("%s: discipline does not support rebuild\n",
		    DEVNAME(sc));
		goto done;
	}

	/* make sure volume is in the right state */
	if (sd->sd_vol_status == BIOC_SVREBUILD) {
		printf("%s: rebuild already in progress\n", DEVNAME(sc));
		goto done;
	}
	if (sd->sd_vol_status != BIOC_SVDEGRADED) {
		printf("%s: %s not degraded\n", DEVNAME(sc),
		    sd->sd_meta->ssd_devname);
		goto done;
	}

	/* find offline chunk */
	for (c = 0, found = -1; c < sd->sd_meta->ssdi.ssd_chunk_no; c++)
		if (sd->sd_vol.sv_chunks[c]->src_meta.scm_status ==
		    BIOC_SDOFFLINE) {
			found = c;
			new = &sd->sd_vol.sv_chunks[c]->src_meta;
			if (c > 0)
				break; /* roll at least once over the for */
		} else {
			csize = sd->sd_vol.sv_chunks[c]->src_meta.scmi.scm_size;
			old = &sd->sd_vol.sv_chunks[c]->src_meta;
			if (found != -1)
				break;
		}
	if (found == -1) {
		printf("%s: no offline chunks available for rebuild\n",
		    DEVNAME(sc));
		goto done;
	}

	/* populate meta entry */
	sr_meta_getdevname(sc, dev, devname, sizeof(devname));
	if (bdevvp(dev, &vn)) {
		printf("%s:, sr_rebuild_init: can't allocate vnode\n",
		    DEVNAME(sc));
		goto done;
	}

	if (VOP_OPEN(vn, FREAD | FWRITE, NOCRED, 0)) {
		DNPRINTF(SR_D_META,"%s: sr_ioctl_setstate can't "
		    "open %s\n", DEVNAME(sc), devname);
		vput(vn);
		goto done;
	}
	open = 1; /* close dev on error */

	/* get partition */
	part = DISKPART(dev);
	if (VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)&label, FREAD, NOCRED, 0)) {
		DNPRINTF(SR_D_META, "%s: sr_ioctl_setstate ioctl failed\n",
		    DEVNAME(sc));
		goto done;
	}
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		printf("%s: %s partition not of type RAID (%d)\n",
		    DEVNAME(sc), devname,
		    label.d_partitions[part].p_fstype);
		goto done;
	}

	/* is partition large enough? */
	size = DL_GETPSIZE(&label.d_partitions[part]) -
	    SR_META_SIZE - SR_META_OFFSET;
	if (size < csize) {
		printf("%s: partition too small, at least %llu B required\n",
		    DEVNAME(sc), csize << DEV_BSHIFT);
		goto done;
	} else if (size > csize)
		printf("%s: partition too large, wasting %llu B\n",
		    DEVNAME(sc), (size - csize) << DEV_BSHIFT);

	/* make sure we are not stomping on some other partition */
	c = sr_chunk_in_use(sc, dev);
	if (c != BIOC_SDINVALID && c != BIOC_SDOFFLINE) {
		printf("%s: %s is already in use\n", DEVNAME(sc), devname);
		goto done;
	}

	/* Reset rebuild counter since we rebuilding onto a new chunk. */
	sd->sd_meta->ssd_rebuild = 0;

	/* recreate metadata */
	open = 0; /* leave dev open from here on out */
	sd->sd_vol.sv_chunks[found]->src_dev_mm = dev;
	sd->sd_vol.sv_chunks[found]->src_vn = vn;
	new->scmi.scm_volid = old->scmi.scm_volid;
	new->scmi.scm_chunk_id = found;
	strlcpy(new->scmi.scm_devname, devname,
	    sizeof new->scmi.scm_devname);
	new->scmi.scm_size = size;
	new->scmi.scm_coerced_size = old->scmi.scm_coerced_size;
	bcopy(&old->scmi.scm_uuid, &new->scmi.scm_uuid,
	    sizeof new->scmi.scm_uuid);
	sr_checksum(sc, new, &new->scm_checksum,
	    sizeof(struct sr_meta_chunk_invariant));
	sd->sd_set_chunk_state(sd, found, BIOC_SDREBUILD);
	if (sr_meta_save(sd, SR_META_DIRTY)) {
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), devname);
		open = 1;
		goto done;
	}

	printf("%s: rebuild of %s started on %s\n", DEVNAME(sc),
	    sd->sd_meta->ssd_devname, devname);

	sd->sd_reb_abort = 0;
	kthread_create_deferred(sr_rebuild, sd);

	rv = 0;
done:
	if (open) {
		VOP_CLOSE(vn, FREAD | FWRITE, NOCRED, 0);
		vput(vn);
	}

	return (rv);
}

void
sr_roam_chunks(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk		*chunk;
	struct sr_meta_chunk	*meta;
	int			roamed = 0;

	/* Have any chunks roamed? */
	SLIST_FOREACH(chunk, &sd->sd_vol.sv_chunk_list, src_link) {
		
		meta = &chunk->src_meta;
	
		if (strncmp(meta->scmi.scm_devname, chunk->src_devname,
		    sizeof(meta->scmi.scm_devname))) {

			printf("%s: roaming device %s -> %s\n", DEVNAME(sc),
			    meta->scmi.scm_devname, chunk->src_devname);

			strlcpy(meta->scmi.scm_devname, chunk->src_devname,
			    sizeof(meta->scmi.scm_devname));

			roamed++;
		}
	}

	if (roamed)
		sr_meta_save(sd, SR_META_DIRTY);
}

int
sr_ioctl_createraid(struct sr_softc *sc, struct bioc_createraid *bc, int user)
{
	dev_t			*dt;
	int			i, s, no_chunk, rv = EINVAL, vol;
	int			no_meta, updatemeta = 0;
	struct sr_chunk_head	*cl;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*ch_entry;
	struct device		*dev, *dev2;
	struct scsibus_attach_args saa;
	char			devname[32];

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_createraid(%d)\n",
	    DEVNAME(sc), user);

	/* user input */
	if (bc->bc_dev_list_len > BIOC_CRMAXLEN)
		goto unwind;

	dt = malloc(bc->bc_dev_list_len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (user) {
		if (copyin(bc->bc_dev_list, dt, bc->bc_dev_list_len) != 0)
			goto unwind;
	} else
		bcopy(bc->bc_dev_list, dt, bc->bc_dev_list_len);

	/* Initialise discipline. */
	sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_WAITOK | M_ZERO);
	sd->sd_sc = sc;
	if (sr_discipline_init(sd, bc->bc_level)) {
		printf("%s: could not initialize discipline\n", DEVNAME(sc));
		goto unwind;
	}

	no_chunk = bc->bc_dev_list_len / sizeof(dev_t);
	cl = &sd->sd_vol.sv_chunk_list;
	SLIST_INIT(cl);

	/* Ensure that chunks are not already in use. */
	for (i = 0; i < no_chunk; i++) {
		if (sr_chunk_in_use(sc, dt[i]) != BIOC_SDINVALID) {
			sr_meta_getdevname(sc, dt[i], devname, sizeof(devname));
			printf("%s: chunk %s already in use\n",
			    DEVNAME(sc), devname);
			goto unwind;
		}
	}

	sd->sd_meta_type = sr_meta_probe(sd, dt, no_chunk);
	if (sd->sd_meta_type == SR_META_F_INVALID) {
		printf("%s: invalid metadata format\n", DEVNAME(sc));
		goto unwind;
	}

	if (sr_meta_attach(sd, bc->bc_flags & BIOC_SCFORCE)) {
		printf("%s: can't attach metadata type %d\n", DEVNAME(sc),
		    sd->sd_meta_type);
		goto unwind;
	}

	/* force the raid volume by clearing metadata region */
	if (bc->bc_flags & BIOC_SCFORCE) {
		/* make sure disk isn't up and running */
		if (sr_meta_read(sd))
			if (sr_already_assembled(sd)) {
				printf("%s: disk ", DEVNAME(sc));
				sr_uuid_print(&sd->sd_meta->ssdi.ssd_uuid, 0);
				printf(" is currently in use; can't force "
				    "create\n");
				goto unwind;
			}

		if (sr_meta_clear(sd)) {
			printf("%s: failed to clear metadata\n", DEVNAME(sc));
			goto unwind;
		}
	}

	if ((no_meta = sr_meta_read(sd)) == 0) {
		/* fill out all chunk metadata */
		sr_meta_chunks_create(sc, cl);
		ch_entry = SLIST_FIRST(cl);

		sd->sd_vol_status = BIOC_SVONLINE;
		sd->sd_meta->ssdi.ssd_level = bc->bc_level;
		sd->sd_meta->ssdi.ssd_chunk_no = no_chunk;

		/* Make the volume UUID available. */
		bcopy(&ch_entry->src_meta.scmi.scm_uuid,
		    &sd->sd_meta->ssdi.ssd_uuid,
		    sizeof(sd->sd_meta->ssdi.ssd_uuid));

		if (sd->sd_create) {
			if ((i = sd->sd_create(sd, bc, no_chunk,
			    ch_entry->src_meta.scmi.scm_coerced_size))) {
				rv = i;
				goto unwind;
			}
		}

		/* fill out all volume metadata */
		DNPRINTF(SR_D_IOCTL,
		    "%s: sr_ioctl_createraid: vol_size: %lld\n",
		    DEVNAME(sc), sd->sd_meta->ssdi.ssd_size);
		strlcpy(sd->sd_meta->ssdi.ssd_vendor, "OPENBSD",
		    sizeof(sd->sd_meta->ssdi.ssd_vendor));
		snprintf(sd->sd_meta->ssdi.ssd_product,
		    sizeof(sd->sd_meta->ssdi.ssd_product), "SR %s",
		    sd->sd_name);
		snprintf(sd->sd_meta->ssdi.ssd_revision,
		    sizeof(sd->sd_meta->ssdi.ssd_revision), "%03d",
		    SR_META_VERSION);

		sd->sd_meta_flags = bc->bc_flags & BIOC_SCNOAUTOASSEMBLE;
		updatemeta = 1;
	} else if (no_meta == no_chunk) {
		if (sd->sd_meta->ssd_meta_flags & SR_META_DIRTY)
			printf("%s: %s was not shutdown properly\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);
		if (user == 0 && sd->sd_meta_flags & BIOC_SCNOAUTOASSEMBLE) {
			DNPRINTF(SR_D_META, "%s: disk not auto assembled from "
			    "metadata\n", DEVNAME(sc));
			goto unwind;
		}
		if (sr_already_assembled(sd)) {
			printf("%s: disk ", DEVNAME(sc));
			sr_uuid_print(&sd->sd_meta->ssdi.ssd_uuid, 0);
			printf(" already assembled\n");
			goto unwind;
		}

		if (sd->sd_assemble) {
			if ((i = sd->sd_assemble(sd, bc, no_chunk))) {
				rv = i;
				goto unwind;
			}
		}

		DNPRINTF(SR_D_META, "%s: disk assembled from metadata\n",
		    DEVNAME(sc));
		updatemeta = 0;
	} else if (no_meta == -1) {
		printf("%s: one of the chunks has corrupt metadata; aborting "
		    "assembly\n", DEVNAME(sc));
		goto unwind;
	} else {
		if (sr_already_assembled(sd)) {
			printf("%s: disk ", DEVNAME(sc));
			sr_uuid_print(&sd->sd_meta->ssdi.ssd_uuid, 0);
			printf(" already assembled; will not partial "
			    "assemble it\n");
			goto unwind;
		}

		if (sd->sd_assemble) {
			if ((i = sd->sd_assemble(sd, bc, no_chunk))) {
				rv = i;
				goto unwind;
			}
		}

		printf("%s: trying to bring up %s degraded\n", DEVNAME(sc),
		    sd->sd_meta->ssd_devname);
	}

	/* metadata SHALL be fully filled in at this point */

	/* Make sure that metadata level matches assembly level. */
	if (sd->sd_meta->ssdi.ssd_level != bc->bc_level) {
		printf("%s: volume level does not match metadata level!\n",
		    DEVNAME(sc));
		goto unwind;
	}

	/* allocate all resources */
	if ((rv = sd->sd_alloc_resources(sd)))
		goto unwind;

	/* Adjust flags if necessary. */
	if ((sd->sd_capabilities & SR_CAP_AUTO_ASSEMBLE) &&
	    (bc->bc_flags & BIOC_SCNOAUTOASSEMBLE) !=
	    (sd->sd_meta->ssdi.ssd_flags & BIOC_SCNOAUTOASSEMBLE)) {
		sd->sd_meta->ssdi.ssd_flags &= ~BIOC_SCNOAUTOASSEMBLE;
		sd->sd_meta->ssdi.ssd_flags |=
		    bc->bc_flags & BIOC_SCNOAUTOASSEMBLE;
	}

	if (sd->sd_capabilities & SR_CAP_SYSTEM_DISK) {
		/* set volume status */
		sd->sd_set_vol_state(sd);
		if (sd->sd_vol_status == BIOC_SVOFFLINE) {
			printf("%s: %s offline, will not be brought online\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);
			goto unwind;
		}

		/* setup scsi midlayer */
		if (sd->sd_openings)
			sd->sd_link.openings = sd->sd_openings(sd);
		else
			sd->sd_link.openings = sd->sd_max_wu;
		sd->sd_link.device = &sr_dev;
		sd->sd_link.device_softc = sc;
		sd->sd_link.adapter_softc = sc;
		sd->sd_link.adapter = &sr_switch;
		sd->sd_link.adapter_target = SR_MAX_LD;
		sd->sd_link.adapter_buswidth = 1;
		bzero(&saa, sizeof(saa));
		saa.saa_sc_link = &sd->sd_link;

		/*
		 * we passed all checks return ENXIO if volume can't be created
		 */
		rv = ENXIO;

		/* clear sense data */
		bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));

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

		DNPRINTF(SR_D_IOCTL, "%s: sr device added: %s on scsibus%d\n",
		    DEVNAME(sc), dev->dv_xname, sd->sd_link.scsibus);

		sc->sc_dis[sd->sd_link.scsibus] = sd;
		for (i = 0, vol = -1; i <= sd->sd_link.scsibus; i++)
			if (sc->sc_dis[i])
				vol++;
		sd->sd_scsibus_dev = dev2;

		rv = 0;
		if (updatemeta) {
			/* fill out remaining volume metadata */
			sd->sd_meta->ssdi.ssd_volid = vol;
			strlcpy(sd->sd_meta->ssd_devname, dev->dv_xname,
			    sizeof(sd->sd_meta->ssd_devname));
			sr_meta_init(sd, cl);
		} else {
			if (strncmp(sd->sd_meta->ssd_devname, dev->dv_xname,
			    sizeof(dev->dv_xname))) {
				printf("%s: volume %s is roaming, it used to "
				    "be %s, updating metadata\n",
				    DEVNAME(sc), dev->dv_xname,
				    sd->sd_meta->ssd_devname);

				sd->sd_meta->ssdi.ssd_volid = vol;
				strlcpy(sd->sd_meta->ssd_devname, dev->dv_xname,
				    sizeof(sd->sd_meta->ssd_devname));
			}
		}

		/* Update device name on any chunks which roamed. */
		sr_roam_chunks(sd);

#ifndef SMALL_KERNEL
		if (sr_sensors_create(sd))
			printf("%s: unable to create sensor for %s\n",
			    DEVNAME(sc), dev->dv_xname);
		else
			sd->sd_vol.sv_sensor_valid = 1;
#endif /* SMALL_KERNEL */
	} else {
		/* we are not an os disk */
		if (updatemeta) {
			/* fill out remaining volume metadata */
			sd->sd_meta->ssdi.ssd_volid = 0;
			strlcpy(sd->sd_meta->ssd_devname, ch_entry->src_devname,
			    sizeof(sd->sd_meta->ssd_devname));
			sr_meta_init(sd, cl);
		}
		if (sd->sd_start_discipline(sd))
			goto unwind;
	}

	/* save metadata to disk */
	rv = sr_meta_save(sd, SR_META_DIRTY);
	sd->sd_shutdownhook = shutdownhook_establish(sr_shutdown, sd);

	if (sd->sd_vol_status == BIOC_SVREBUILD)
		kthread_create_deferred(sr_rebuild, sd);

	sd->sd_ready = 1;

	return (rv);
unwind:
	sr_discipline_shutdown(sd);

	/* XXX - use internal status values! */
	if (rv == EAGAIN)
		rv = 0;

	return (rv);
}

int
sr_ioctl_deleteraid(struct sr_softc *sc, struct bioc_deleteraid *dr)
{
	struct sr_discipline	*sd = NULL;
	int			rv = 1;
	int			i;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_deleteraid %s\n", DEVNAME(sc),
	    dr->bd_dev);

	for (i = 0; i < SR_MAXSCSIBUS; i++)
		if (sc->sc_dis[i]) {
			if (!strncmp(sc->sc_dis[i]->sd_meta->ssd_devname,
			    dr->bd_dev,
			    sizeof(sc->sc_dis[i]->sd_meta->ssd_devname))) {
				sd = sc->sc_dis[i];
				break;
			}
		}

	if (sd == NULL)
		goto bad;

	sd->sd_deleted = 1;
	sd->sd_meta->ssdi.ssd_flags = BIOC_SCNOAUTOASSEMBLE;
	sr_shutdown(sd);

	rv = 0;
bad:
	return (rv);
}

int
sr_ioctl_discipline(struct sr_softc *sc, struct bioc_discipline *bd)
{
	struct sr_discipline	*sd = NULL;
	int			i, rv = 1;

	/* Dispatch a discipline specific ioctl. */

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_discipline %s\n", DEVNAME(sc),
	    bd->bd_dev);

	for (i = 0; i < SR_MAXSCSIBUS; i++)
		if (sc->sc_dis[i]) {
			if (!strncmp(sc->sc_dis[i]->sd_meta->ssd_devname,
			    bd->bd_dev,
			    sizeof(sc->sc_dis[i]->sd_meta->ssd_devname))) {
				sd = sc->sc_dis[i];
				break;
			}
		}

	if (sd && sd->sd_ioctl_handler)
		rv = sd->sd_ioctl_handler(sd, bd);

	return (rv);
}

void
sr_chunks_unwind(struct sr_softc *sc, struct sr_chunk_head *cl)
{
	struct sr_chunk		*ch_entry, *ch_next;

	DNPRINTF(SR_D_IOCTL, "%s: sr_chunks_unwind\n", DEVNAME(sc));

	if (!cl)
		return;

	for (ch_entry = SLIST_FIRST(cl);
	    ch_entry != SLIST_END(cl); ch_entry = ch_next) {
		ch_next = SLIST_NEXT(ch_entry, src_link);

		DNPRINTF(SR_D_IOCTL, "%s: sr_chunks_unwind closing: %s\n",
		    DEVNAME(sc), ch_entry->src_devname);
		if (ch_entry->src_vn) {
			/*
			 * XXX - explicitly lock the vnode until we can resolve
			 * the problem introduced by vnode aliasing... specfs
			 * has no locking, whereas ufs/ffs does!
			 */
			vn_lock(ch_entry->src_vn, LK_EXCLUSIVE | LK_RETRY, 0);
			VOP_CLOSE(ch_entry->src_vn, FREAD | FWRITE, NOCRED, 0);
			vput(ch_entry->src_vn);
		}
		free(ch_entry, M_DEVBUF);
	}
	SLIST_INIT(cl);
}

void
sr_discipline_free(struct sr_discipline *sd)
{
	struct sr_softc		*sc;
	int			i;

	if (!sd)
		return;

	sc = sd->sd_sc;

	DNPRINTF(SR_D_DIS, "%s: sr_discipline_free %s\n",
	    DEVNAME(sc),
	    sd->sd_meta ? sd->sd_meta->ssd_devname : "nodev");
	if (sd->sd_free_resources)
		sd->sd_free_resources(sd);
	if (sd->sd_vol.sv_chunks)
		free(sd->sd_vol.sv_chunks, M_DEVBUF);
	if (sd->sd_meta)
		free(sd->sd_meta, M_DEVBUF);
	if (sd->sd_meta_foreign)
		free(sd->sd_meta_foreign, M_DEVBUF);

	for (i = 0; i < SR_MAXSCSIBUS; i++)
		if (sc->sc_dis[i] == sd) {
			sc->sc_dis[i] = NULL;
			break;
		}

	free(sd, M_DEVBUF);
}

void
sr_discipline_shutdown(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			s;

	if (!sd || !sc)
		return;

	DNPRINTF(SR_D_DIS, "%s: sr_discipline_shutdown %s\n", DEVNAME(sc),
	    sd->sd_meta ? sd->sd_meta->ssd_devname : "nodev");

	s = splbio();

	sd->sd_ready = 0;

	if (sd->sd_shutdownhook)
		shutdownhook_disestablish(sd->sd_shutdownhook);

	/* make sure there isn't a sync pending and yield */
	wakeup(sd);
	while (sd->sd_sync || sd->sd_must_flush)
		if (tsleep(&sd->sd_sync, MAXPRI, "sr_down", 60 * hz) ==
		    EWOULDBLOCK)
			break;

#ifndef SMALL_KERNEL
	sr_sensors_delete(sd);
#endif /* SMALL_KERNEL */

	if (sd->sd_scsibus_dev)
		config_detach(sd->sd_scsibus_dev, DETACH_FORCE);

	sr_chunks_unwind(sc, &sd->sd_vol.sv_chunk_list);

	if (sd)
		sr_discipline_free(sd);

	splx(s);
}

int
sr_discipline_init(struct sr_discipline *sd, int level)
{
	int			rv = 1;

	switch (level) {
	case 0:
		sr_raid0_discipline_init(sd);
		break;
	case 1:
		sr_raid1_discipline_init(sd);
		break;
	case 4:
		sr_raidp_discipline_init(sd, SR_MD_RAID4);
		break;
	case 5:
		sr_raidp_discipline_init(sd, SR_MD_RAID5);
		break;
	case 6:
		sr_raid6_discipline_init(sd);
		break;
#ifdef AOE
	/* AOE target. */
	case 'A':
		sr_aoe_server_discipline_init(sd);
		break;
	/* AOE initiator. */
	case 'a':
		sr_aoe_discipline_init(sd);
		break;
#endif
#ifdef CRYPTO
	case 'C':
		sr_crypto_discipline_init(sd);
		break;
#endif
	default:
		goto bad;
	}

	rv = 0;
bad:
	return (rv);
}

int
sr_raid_inquiry(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_inquiry_data inq;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_inquiry\n", DEVNAME(sd->sd_sc));

	bzero(&inq, sizeof(inq));
	inq.device = T_DIRECT;
	inq.dev_qual2 = 0;
	inq.version = 2;
	inq.response_format = 2;
	inq.additional_length = 32;
	strlcpy(inq.vendor, sd->sd_meta->ssdi.ssd_vendor,
	    sizeof(inq.vendor));
	strlcpy(inq.product, sd->sd_meta->ssdi.ssd_product,
	    sizeof(inq.product));
	strlcpy(inq.revision, sd->sd_meta->ssdi.ssd_revision,
	    sizeof(inq.revision));
	sr_copy_internal_data(xs, &inq, sizeof(inq));

	return (0);
}

int
sr_raid_read_cap(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_read_cap_data rcd;
	struct scsi_read_cap_data_16 rcd16;
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_read_cap\n", DEVNAME(sd->sd_sc));

	if (xs->cmd->opcode == READ_CAPACITY) {
		bzero(&rcd, sizeof(rcd));
		if (sd->sd_meta->ssdi.ssd_size > 0xffffffffllu)
			_lto4b(0xffffffff, rcd.addr);
		else
			_lto4b(sd->sd_meta->ssdi.ssd_size, rcd.addr);
		_lto4b(512, rcd.length);
		sr_copy_internal_data(xs, &rcd, sizeof(rcd));
		rv = 0;
	} else if (xs->cmd->opcode == READ_CAPACITY_16) {
		bzero(&rcd16, sizeof(rcd16));
		_lto8b(sd->sd_meta->ssdi.ssd_size, rcd16.addr);
		_lto4b(512, rcd16.length);
		sr_copy_internal_data(xs, &rcd16, sizeof(rcd16));
		rv = 0;
	}

	return (rv);
}

int
sr_raid_tur(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_tur\n", DEVNAME(sd->sd_sc));

	if (sd->sd_vol_status == BIOC_SVOFFLINE) {
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
		sd->sd_scsi_sense.flags = SKEY_NOT_READY;
		sd->sd_scsi_sense.add_sense_code = 0x04;
		sd->sd_scsi_sense.add_sense_code_qual = 0x11;
		sd->sd_scsi_sense.extra_len = 4;
		return (1);
	} else if (sd->sd_vol_status == BIOC_SVINVALID) {
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
		sd->sd_scsi_sense.flags = SKEY_HARDWARE_ERROR;
		sd->sd_scsi_sense.add_sense_code = 0x05;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		sd->sd_scsi_sense.extra_len = 4;
		return (1);
	}

	return (0);
}

int
sr_raid_request_sense(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_request_sense\n",
	    DEVNAME(sd->sd_sc));

	/* use latest sense data */
	bcopy(&sd->sd_scsi_sense, &xs->sense, sizeof(xs->sense));

	/* clear sense data */
	bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));

	return (0);
}

int
sr_raid_start_stop(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_start_stop	*ss = (struct scsi_start_stop *)xs->cmd;
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_start_stop\n",
	    DEVNAME(sd->sd_sc));

	if (!ss)
		return (rv);

	if (ss->byte2 == 0x00) {
		/* START */
		if (sd->sd_vol_status == BIOC_SVOFFLINE) {
			/* bring volume online */
			/* XXX check to see if volume can be brought online */
			sd->sd_vol_status = BIOC_SVONLINE;
		}
		rv = 0;
	} else /* XXX is this the check? if (byte == 0x01) */ {
		/* STOP */
		if (sd->sd_vol_status == BIOC_SVONLINE) {
			/* bring volume offline */
			sd->sd_vol_status = BIOC_SVOFFLINE;
		}
		rv = 0;
	}

	return (rv);
}

int
sr_raid_sync(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	int			s, rv = 0, ios;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_sync\n", DEVNAME(sd->sd_sc));

	/* when doing a fake sync don't count the wu */
	ios = wu->swu_fake ? 0 : 1;

	s = splbio();
	sd->sd_sync = 1;

	while (sd->sd_wu_pending > ios)
		if (tsleep(sd, PRIBIO, "sr_sync", 15 * hz) == EWOULDBLOCK) {
			DNPRINTF(SR_D_DIS, "%s: sr_raid_sync timeout\n",
			    DEVNAME(sd->sd_sc));
			rv = 1;
			break;
		}

	sd->sd_sync = 0;
	splx(s);

	wakeup(&sd->sd_sync);

	return (rv);
}

void
sr_raid_startwu(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct sr_ccb		*ccb;

	splassert(IPL_BIO);

	if (wu->swu_state == SR_WU_RESTART)
		/*
		 * no need to put the wu on the pending queue since we
		 * are restarting the io
		 */
		 ;
	else
		/* move wu to pending queue */
		TAILQ_INSERT_TAIL(&sd->sd_wu_pendq, wu, swu_link);

	/* start all individual ios */
	TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link) {
		VOP_STRATEGY(&ccb->ccb_buf);
	}
}

void
sr_checksum_print(u_int8_t *md5)
{
	int			i;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
		printf("%02x", md5[i]);
}

void
sr_checksum(struct sr_softc *sc, void *src, void *md5, u_int32_t len)
{
	MD5_CTX			ctx;

	DNPRINTF(SR_D_MISC, "%s: sr_checksum(%p %p %d)\n", DEVNAME(sc), src,
	    md5, len);

	MD5Init(&ctx);
	MD5Update(&ctx, src, len);
	MD5Final(md5, &ctx);
}

void
sr_uuid_get(struct sr_uuid *uuid)
{
	arc4random_buf(uuid->sui_id, sizeof(uuid->sui_id));
	/* UUID version 4: random */
	uuid->sui_id[6] &= 0x0f;
	uuid->sui_id[6] |= 0x40;
	/* RFC4122 variant */
	uuid->sui_id[8] &= 0x3f;
	uuid->sui_id[8] |= 0x80;
}

void
sr_uuid_print(struct sr_uuid *uuid, int cr)
{
	printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
	    "%02x%02x%02x%02x%02x%02x",
	    uuid->sui_id[0], uuid->sui_id[1],
	    uuid->sui_id[2], uuid->sui_id[3],
	    uuid->sui_id[4], uuid->sui_id[5],
	    uuid->sui_id[6], uuid->sui_id[7],
	    uuid->sui_id[8], uuid->sui_id[9],
	    uuid->sui_id[10], uuid->sui_id[11],
	    uuid->sui_id[12], uuid->sui_id[13],
	    uuid->sui_id[14], uuid->sui_id[15]);

	if (cr)
		printf("\n");
}

int
sr_already_assembled(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			i;

	for (i = 0; i < SR_MAXSCSIBUS; i++)
		if (sc->sc_dis[i])
			if (!bcmp(&sd->sd_meta->ssdi.ssd_uuid,
			    &sc->sc_dis[i]->sd_meta->ssdi.ssd_uuid,
			    sizeof(sd->sd_meta->ssdi.ssd_uuid)))
				return (1);

	return (0);
}

int32_t
sr_validate_stripsize(u_int32_t b)
{
	int			s = 0;

	if (b % 512)
		return (-1);

	while ((b & 1) == 0) {
		b >>= 1;
		s++;
	}

	/* only multiple of twos */
	b >>= 1;
	if (b)
		return(-1);

	return (s);
}

void
sr_shutdown(void *arg)
{
	struct sr_discipline	*sd = arg;
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	DNPRINTF(SR_D_DIS, "%s: sr_shutdown %s\n",
	    DEVNAME(sc), sd->sd_meta->ssd_devname);

	/* abort rebuild and drain io */
	sd->sd_reb_abort = 1;
	while (sd->sd_reb_active)
		tsleep(sd, PWAIT, "sr_shutdown", 1);

	sr_meta_save(sd, 0);

	sr_discipline_shutdown(sd);
}

int
sr_validate_io(struct sr_workunit *wu, daddr64_t *blk, char *func)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: %s 0x%02x\n", DEVNAME(sd->sd_sc), func,
	    xs->cmd->opcode);

	if (sd->sd_vol_status == BIOC_SVOFFLINE) {
		DNPRINTF(SR_D_DIS, "%s: %s device offline\n",
		    DEVNAME(sd->sd_sc), func);
		goto bad;
	}

	if (xs->datalen == 0) {
		printf("%s: %s: illegal block count for %s\n",
		    DEVNAME(sd->sd_sc), func, sd->sd_meta->ssd_devname);
		goto bad;
	}

	if (xs->cmdlen == 10)
		*blk = _4btol(((struct scsi_rw_big *)xs->cmd)->addr);
	else if (xs->cmdlen == 16)
		*blk = _8btol(((struct scsi_rw_16 *)xs->cmd)->addr);
	else if (xs->cmdlen == 6)
		*blk = _3btol(((struct scsi_rw *)xs->cmd)->addr);
	else {
		printf("%s: %s: illegal cmdlen for %s\n",
		    DEVNAME(sd->sd_sc), func, sd->sd_meta->ssd_devname);
		goto bad;
	}

	wu->swu_blk_start = *blk;
	wu->swu_blk_end = *blk + (xs->datalen >> DEV_BSHIFT) - 1;

	if (wu->swu_blk_end > sd->sd_meta->ssdi.ssd_size) {
		DNPRINTF(SR_D_DIS, "%s: %s out of bounds start: %lld "
		    "end: %lld length: %d\n",
		    DEVNAME(sd->sd_sc), func, wu->swu_blk_start,
		    wu->swu_blk_end, xs->datalen);

		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT |
		    SSD_ERRCODE_VALID;
		sd->sd_scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
		sd->sd_scsi_sense.add_sense_code = 0x21;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		sd->sd_scsi_sense.extra_len = 4;
		goto bad;
	}

	rv = 0;
bad:
	return (rv);
}

int
sr_check_io_collision(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct sr_workunit	*wup;

	splassert(IPL_BIO);

	/* walk queue backwards and fill in collider if we have one */
	TAILQ_FOREACH_REVERSE(wup, &sd->sd_wu_pendq, sr_wu_list, swu_link) {
		if (wu->swu_blk_end < wup->swu_blk_start ||
		    wup->swu_blk_end < wu->swu_blk_start)
			continue;

		/* we have an LBA collision, defer wu */
		wu->swu_state = SR_WU_DEFERRED;
		if (wup->swu_collider)
			/* wu is on deferred queue, append to last wu */
			while (wup->swu_collider)
				wup = wup->swu_collider;

		wup->swu_collider = wu;
		TAILQ_INSERT_TAIL(&sd->sd_wu_defq, wu, swu_link);
		sd->sd_wu_collisions++;
		goto queued;
	}

	return (0);
queued:
	return (1);
}

void
sr_rebuild(void *arg)
{
	struct sr_discipline	*sd = arg;
	struct sr_softc		*sc = sd->sd_sc;

	if (kthread_create(sr_rebuild_thread, sd, &sd->sd_background_proc,
	    DEVNAME(sc)) != 0)
		printf("%s: unable to start backgound operation\n",
		    DEVNAME(sc));
}

void
sr_rebuild_thread(void *arg)
{
	struct sr_discipline	*sd = arg;
	struct sr_softc		*sc = sd->sd_sc;
	daddr64_t		whole_blk, partial_blk, blk, sz, lba;
	daddr64_t		psz, rb, restart;
	uint64_t		mysize = 0;
	struct sr_workunit	*wu_r, *wu_w;
	struct scsi_xfer	xs_r, xs_w;
	struct scsi_rw_16	cr, cw;
	int			c, s, slept, percent = 0, old_percent = -1;
	u_int8_t		*buf;

	whole_blk = sd->sd_meta->ssdi.ssd_size / SR_REBUILD_IO_SIZE;
	partial_blk = sd->sd_meta->ssdi.ssd_size % SR_REBUILD_IO_SIZE;

	restart = sd->sd_meta->ssd_rebuild / SR_REBUILD_IO_SIZE;
	if (restart > whole_blk) {
		printf("%s: bogus rebuild restart offset, starting from 0\n",
		    DEVNAME(sc));
		restart = 0;
	}
	if (restart) {
		/*
		 * XXX there is a hole here; there is a posibility that we
		 * had a restart however the chunk that was supposed to
		 * be rebuilt is no longer valid; we can reach this situation
		 * when a rebuild is in progress and the box crashes and
		 * on reboot the rebuild chunk is different (like zero'd or
		 * replaced).  We need to check the uuid of the chunk that is
		 * being rebuilt to assert this.
		 */
		psz = sd->sd_meta->ssdi.ssd_size;
		rb = sd->sd_meta->ssd_rebuild;
		if (rb > 0)
			percent = 100 - ((psz * 100 - rb * 100) / psz) - 1;
		else
			percent = 0;
		printf("%s: resuming rebuild on %s at %llu%%\n",
		    DEVNAME(sc), sd->sd_meta->ssd_devname, percent);
	}

	sd->sd_reb_active = 1;

	buf = malloc(SR_REBUILD_IO_SIZE << DEV_BSHIFT, M_DEVBUF, M_WAITOK);
	for (blk = restart; blk <= whole_blk; blk++) {
		if (blk == whole_blk)
			sz = partial_blk;
		else
			sz = SR_REBUILD_IO_SIZE;
		mysize += sz;
		lba = blk * sz;

		/* get some wu */
		if ((wu_r = sr_wu_get(sd, 1)) == NULL)
			panic("%s: rebuild exhausted wu_r", DEVNAME(sc));
		if ((wu_w = sr_wu_get(sd, 1)) == NULL)
			panic("%s: rebuild exhausted wu_w", DEVNAME(sc));

		/* setup read io */
		bzero(&xs_r, sizeof xs_r);
		bzero(&cr, sizeof cr);
		xs_r.error = XS_NOERROR;
		xs_r.flags = SCSI_DATA_IN;
		xs_r.datalen = sz << DEV_BSHIFT;
		xs_r.data = buf;
		xs_r.cmdlen = 16;
		cr.opcode = READ_16;
		_lto4b(sz, cr.length);
		_lto8b(lba, cr.addr);
		xs_r.cmd = (struct scsi_generic *)&cr;
		wu_r->swu_flags |= SR_WUF_REBUILD;
		wu_r->swu_xs = &xs_r;
		if (sd->sd_scsi_rw(wu_r)) {
			printf("%s: could not create read io\n",
			    DEVNAME(sc));
			goto fail;
		}

		/* setup write io */
		bzero(&xs_w, sizeof xs_w);
		bzero(&cw, sizeof cw);
		xs_w.error = XS_NOERROR;
		xs_w.flags = SCSI_DATA_OUT;
		xs_w.datalen = sz << DEV_BSHIFT;
		xs_w.data = buf;
		xs_w.cmdlen = 16;
		cw.opcode = WRITE_16;
		_lto4b(sz, cw.length);
		_lto8b(lba, cw.addr);
		xs_w.cmd = (struct scsi_generic *)&cw;
		wu_w->swu_flags |= SR_WUF_REBUILD;
		wu_w->swu_xs = &xs_w;
		if (sd->sd_scsi_rw(wu_w)) {
			printf("%s: could not create write io\n",
			    DEVNAME(sc));
			goto fail;
		}

		/*
		 * collide with the read io so that we get automatically
		 * started when the read is done
		 */
		wu_w->swu_state = SR_WU_DEFERRED;
		wu_r->swu_collider = wu_w;
		s = splbio();
		TAILQ_INSERT_TAIL(&sd->sd_wu_defq, wu_w, swu_link);

		/* schedule io */
		if (sr_check_io_collision(wu_r))
			goto queued;

		sr_raid_startwu(wu_r);
queued:
		splx(s);

		/* wait for read completion */
		slept = 0;
		while ((wu_w->swu_flags & SR_WUF_REBUILDIOCOMP) == 0) {
			tsleep(wu_w, PRIBIO, "sr_rebuild", 0);
			slept = 1;
		}
		/* yield if we didn't sleep */
		if (slept == 0)
			tsleep(sc, PWAIT, "sr_yield", 1);

		sr_wu_put(wu_r);
		sr_wu_put(wu_w);

		sd->sd_meta->ssd_rebuild = lba;

		/* save metadata every percent */
		psz = sd->sd_meta->ssdi.ssd_size;
		rb = sd->sd_meta->ssd_rebuild;
		if (rb > 0)
			percent = 100 - ((psz * 100 - rb * 100) / psz) - 1;
		else
			percent = 0;
		if (percent != old_percent && blk != whole_blk) {
			if (sr_meta_save(sd, SR_META_DIRTY))
				printf("%s: could not save metadata to %s\n",
				    DEVNAME(sc), sd->sd_meta->ssd_devname);
			old_percent = percent;
		}

		if (sd->sd_reb_abort)
			goto abort;
	}

	/* all done */
	sd->sd_meta->ssd_rebuild = 0;
	for (c = 0; c < sd->sd_meta->ssdi.ssd_chunk_no; c++)
		if (sd->sd_vol.sv_chunks[c]->src_meta.scm_status ==
		    BIOC_SDREBUILD) {
			sd->sd_set_chunk_state(sd, c, BIOC_SDONLINE);
			break;
		}

abort:
	if (sr_meta_save(sd, SR_META_DIRTY))
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), sd->sd_meta->ssd_devname);
fail:
	free(buf, M_DEVBUF);
	sd->sd_reb_active = 0;
	kthread_exit(0);
}

#ifndef SMALL_KERNEL
int
sr_sensors_create(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			rv = 1;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_sensors_create\n",
	    DEVNAME(sc), sd->sd_meta->ssd_devname);

	strlcpy(sd->sd_vol.sv_sensordev.xname, DEVNAME(sc),
	    sizeof(sd->sd_vol.sv_sensordev.xname));

	sd->sd_vol.sv_sensor.type = SENSOR_DRIVE;
	sd->sd_vol.sv_sensor.status = SENSOR_S_UNKNOWN;
	strlcpy(sd->sd_vol.sv_sensor.desc, sd->sd_meta->ssd_devname,
	    sizeof(sd->sd_vol.sv_sensor.desc));

	sensor_attach(&sd->sd_vol.sv_sensordev, &sd->sd_vol.sv_sensor);

	if (sc->sc_sensors_running == 0) {
		if (sensor_task_register(sc, sr_sensors_refresh, 10) == NULL)
			goto bad;
		sc->sc_sensors_running = 1;
	}
	sensordev_install(&sd->sd_vol.sv_sensordev);

	rv = 0;
bad:
	return (rv);
}

void
sr_sensors_delete(struct sr_discipline *sd)
{
	DNPRINTF(SR_D_STATE, "%s: sr_sensors_delete\n", DEVNAME(sd->sd_sc));

	if (sd->sd_vol.sv_sensor_valid)
		sensordev_deinstall(&sd->sd_vol.sv_sensordev);
}

void
sr_sensors_refresh(void *arg)
{
	struct sr_softc		*sc = arg;
	struct sr_volume	*sv;
	struct sr_discipline	*sd;
	int			i, vol;

	DNPRINTF(SR_D_STATE, "%s: sr_sensors_refresh\n", DEVNAME(sc));

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (!sc->sc_dis[i])
			continue;

		sd = sc->sc_dis[i];
		sv = &sd->sd_vol;

		switch(sd->sd_vol_status) {
		case BIOC_SVOFFLINE:
			sv->sv_sensor.value = SENSOR_DRIVE_FAIL;
			sv->sv_sensor.status = SENSOR_S_CRIT;
			break;

		case BIOC_SVDEGRADED:
			sv->sv_sensor.value = SENSOR_DRIVE_PFAIL;
			sv->sv_sensor.status = SENSOR_S_WARN;
			break;

		case BIOC_SVSCRUB:
		case BIOC_SVONLINE:
			sv->sv_sensor.value = SENSOR_DRIVE_ONLINE;
			sv->sv_sensor.status = SENSOR_S_OK;
			break;

		default:
			sv->sv_sensor.value = 0; /* unknown */
			sv->sv_sensor.status = SENSOR_S_UNKNOWN;
		}
	}
}
#endif /* SMALL_KERNEL */

#ifdef SR_FANCY_STATS
void				sr_print_stats(void);

void
sr_print_stats(void)
{
	struct sr_softc		*sc;
	struct sr_discipline	*sd;
	int			i, vol;

	for (i = 0; i < softraid_cd.cd_ndevs; i++)
		if (softraid_cd.cd_devs[i]) {
			sc = softraid_cd.cd_devs[i];
			/* we'll only have one softc */
			break;
		}

	if (!sc) {
		printf("no softraid softc found\n");
		return;
	}

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (!sc->sc_dis[i])
			continue;

		sd = sc->sc_dis[i];
		printf("%s: ios pending: %d  collisions %llu\n",
		    sd->sd_meta->ssd_devname,
		    sd->sd_wu_pending,
		    sd->sd_wu_collisions);
	}
}
#endif /* SR_FANCY_STATS */

#ifdef SR_DEBUG
void
sr_meta_print(struct sr_metadata *m)
{
	int			i;
	struct sr_meta_chunk	*mc;
	struct sr_meta_opt	*mo;

	if (!(sr_debug & SR_D_META))
		return;

	printf("\tssd_magic 0x%llx\n", m->ssdi.ssd_magic);
	printf("\tssd_version %d\n", m->ssdi.ssd_version);
	printf("\tssd_flags 0x%x\n", m->ssdi.ssd_flags);
	printf("\tssd_uuid ");
	sr_uuid_print(&m->ssdi.ssd_uuid, 1);
	printf("\tssd_chunk_no %d\n", m->ssdi.ssd_chunk_no);
	printf("\tssd_chunk_id %d\n", m->ssdi.ssd_chunk_id);
	printf("\tssd_opt_no %d\n", m->ssdi.ssd_opt_no);
	printf("\tssd_volid %d\n", m->ssdi.ssd_volid);
	printf("\tssd_level %d\n", m->ssdi.ssd_level);
	printf("\tssd_size %lld\n", m->ssdi.ssd_size);
	printf("\tssd_devname %s\n", m->ssd_devname);
	printf("\tssd_vendor %s\n", m->ssdi.ssd_vendor);
	printf("\tssd_product %s\n", m->ssdi.ssd_product);
	printf("\tssd_revision %s\n", m->ssdi.ssd_revision);
	printf("\tssd_strip_size %d\n", m->ssdi.ssd_strip_size);
	printf("\tssd_checksum ");
	sr_checksum_print(m->ssd_checksum);
	printf("\n");
	printf("\tssd_meta_flags 0x%x\n", m->ssd_meta_flags);
	printf("\tssd_ondisk %llu\n", m->ssd_ondisk);

	mc = (struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < m->ssdi.ssd_chunk_no; i++, mc++) {
		printf("\t\tscm_volid %d\n", mc->scmi.scm_volid);
		printf("\t\tscm_chunk_id %d\n", mc->scmi.scm_chunk_id);
		printf("\t\tscm_devname %s\n", mc->scmi.scm_devname);
		printf("\t\tscm_size %lld\n", mc->scmi.scm_size);
		printf("\t\tscm_coerced_size %lld\n",mc->scmi.scm_coerced_size);
		printf("\t\tscm_uuid ");
		sr_uuid_print(&mc->scmi.scm_uuid, 1);
		printf("\t\tscm_checksum ");
		sr_checksum_print(mc->scm_checksum);
		printf("\n");
		printf("\t\tscm_status %d\n", mc->scm_status);
	}

	mo = (struct sr_meta_opt *)(mc);
	for (i = 0; i < m->ssdi.ssd_opt_no; i++, mo++) {
		printf("\t\t\tsom_type %d\n", mo->somi.som_type);
		printf("\t\t\tsom_checksum ");
		sr_checksum_print(mo->som_checksum);
		printf("\n");
	}
}

void
sr_dump_mem(u_int8_t *p, int len)
{
	int			i;

	for (i = 0; i < len; i++)
		printf("%02x ", *p++);
	printf("\n");
}

#endif /* SR_DEBUG */
