/* $OpenBSD: softraid.c,v 1.132 2009/05/11 14:06:21 jsing Exp $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2008 Chris Kuethe <ckuethe@openbsd.org>
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
/* #define SR_UNIT_TEST */

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
int		sr_activate(struct device *, enum devact);

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
void			sr_checksum(struct sr_softc *, void *, void *,
			    u_int32_t);
int			sr_boot_assembly(struct sr_softc *);
int			sr_already_assembled(struct sr_discipline *);

/* don't include these on RAMDISK */
#ifndef SMALL_KERNEL
void			sr_sensors_refresh(void *);
int			sr_sensors_create(struct sr_discipline *);
void			sr_sensors_delete(struct sr_discipline *);
#endif

/* metadata */
int			sr_meta_probe(struct sr_discipline *, dev_t *, int);
int			sr_meta_attach(struct sr_discipline *, int);
void			sr_meta_getdevname(struct sr_softc *, dev_t, char *,
			    int);
int			sr_meta_rw(struct sr_discipline *, dev_t, void *,
			    size_t, daddr64_t, long);
int			sr_meta_clear(struct sr_discipline *);
int			sr_meta_read(struct sr_discipline *);
int			sr_meta_save(struct sr_discipline *, u_int32_t);
int			sr_meta_validate(struct sr_discipline *, dev_t,
			    struct sr_metadata *, void *);
void			sr_meta_chunks_create(struct sr_softc *,
			    struct sr_chunk_head *);
void			sr_meta_init(struct sr_discipline *,
			    struct sr_chunk_head *);

/* native metadata format */
int			sr_meta_native_bootprobe(struct sr_softc *,
			    struct device *, struct sr_metadata_list_head *);
#define SR_META_NOTCLAIMED	(0)
#define SR_META_CLAIMED		(1)
int			sr_meta_native_probe(struct sr_softc *,
			   struct sr_chunk *);
int			sr_meta_native_attach(struct sr_discipline *, int);
int			sr_meta_native_read(struct sr_discipline *, dev_t,
			    struct sr_metadata *, void *);
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
	  sr_meta_native_read , sr_meta_native_write, NULL },
#define SR_META_F_NATIVE	0
	{ 0, 0, NULL, NULL, NULL, NULL }
#define SR_META_F_INVALID	-1
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
	sd->sd_meta = malloc(SR_META_SIZE * 512 , M_DEVBUF, M_ZERO);
	if (!sd->sd_meta) {
		printf("%s: could not allocate memory for metadata\n",
		    DEVNAME(sc));
		goto bad;
	}

	if (sd->sd_meta_type != SR_META_F_NATIVE) {
		/* in memory copy of foreign metadata */
		sd->sd_meta_foreign =  malloc(smd[sd->sd_meta_type].smd_size ,
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
	struct bdevsw		*bdsw;
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
		dev = dt[d];
		sr_meta_getdevname(sc, dev, devname, sizeof(devname));
		bdsw = bdevsw_lookup(dev);

		/*
		 * XXX leaving dev open for now; move this to attach and figure
		 * out the open/close dance for unwind.
		 */
		error = bdsw->d_open(dev, FREAD | FWRITE , S_IFBLK, curproc);
		if (error) {
			DNPRINTF(SR_D_META,"%s: sr_meta_probe can't open %s\n",
			    DEVNAME(sc), devname);
			/* XXX device isn't open but will be closed anyway */
			goto unwind;
		}

		ch_entry = malloc(sizeof(struct sr_chunk), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		/* keep disks in user supplied order */
		if (ch_prev)
			SLIST_INSERT_AFTER(ch_prev, ch_entry, src_link);
		else
			SLIST_INSERT_HEAD(cl, ch_entry, src_link);
		ch_prev = ch_entry;
		strlcpy(ch_entry->src_devname, devname,
		   sizeof(ch_entry->src_devname));
		ch_entry->src_dev_mm = dev;

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

	if (md == NULL) {
		printf("%s: read invalid metadata pointer\n", DEVNAME(sc));
		goto done;
	}

	bzero(&b, sizeof(b));
	b.b_flags = flags;
	b.b_blkno = ofs;
	b.b_bcount = sz;
	b.b_bufsize = sz;
	b.b_resid = sz;
	b.b_data = md;
	b.b_error = 0;
	b.b_proc = curproc;
	b.b_dev = dev;
	b.b_vp = NULL;
	b.b_iodone = NULL;
	LIST_INIT(&b.b_dep);
	bdevsw_lookup(b.b_dev)->d_strategy(&b);
	biowait(&b);

	if (b.b_flags & B_ERROR) {
		printf("%s: 0x%x i/o error on block %lld while reading "
		    "metadata %d\n", DEVNAME(sc), dev, b.b_blkno, b.b_error);
		goto done;
	}
	rv = 0;
done:
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

	m = malloc(SR_META_SIZE * 512 , M_DEVBUF, M_WAITOK | M_ZERO);
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
		bcopy(&uuid,  &ch_entry->src_meta.scmi.scm_uuid,
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
	m = malloc(SR_META_SIZE * 512 , M_DEVBUF, M_ZERO);
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

		/* calculate metdata checksum for correct chunk */
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

	/* not al disciplines have sync */
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
		fm = malloc(s->smd_size , M_DEVBUF, M_WAITOK | M_ZERO);

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
	struct sr_meta_chunk	*mc;
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

	if (sm->ssdi.ssd_version != SR_META_VERSION) {
		printf("%s: %s can not read metadata version %d, expected %d\n",
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

	/* warn if disk changed order */
	mc = (struct sr_meta_chunk *)(sm + 1);
	if (strncmp(mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname, devname,
	    sizeof(mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname)))
		printf("%s: roaming device %s -> %s\n", DEVNAME(sc),
		    mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname, devname);

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
	struct bdevsw		*bdsw;
	struct disklabel	label;
	struct sr_metadata	*md;
	struct sr_discipline	*fake_sd;
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
	bdsw = &bdevsw[majdev];

	/*
	 * The devices are being opened with S_IFCHR instead of
	 * S_IFBLK so that the SCSI mid-layer does not whine when
	 * media is not inserted in certain devices like zip drives
	 * and such.
	 */

	/* open device */
	error = (*bdsw->d_open)(dev, FREAD, S_IFCHR, curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe open "
		    "failed\n" , DEVNAME(sc));
		goto done;
	}

	/* get disklabel */
	error = (*bdsw->d_ioctl)(dev, DIOCGDINFO, (void *)&label, FREAD,
	    curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe ioctl "
		    "failed\n", DEVNAME(sc));
		error = (*bdsw->d_close)(dev, FREAD, S_IFCHR, curproc);
		goto done;
	}

	/* we are done, close device */
	error = (*bdsw->d_close)(dev, FREAD, S_IFCHR, curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe close "
		    "failed\n", DEVNAME(sc));
		goto done;
	}

	md = malloc(SR_META_SIZE * 512 , M_DEVBUF, M_ZERO);
	if (md == NULL) {
		printf("%s: not enough memory for metadata buffer\n",
		    DEVNAME(sc));
		goto done;
	}

	/* create fake sd to use utility functions */
	fake_sd = malloc(sizeof(struct sr_discipline) , M_DEVBUF, M_ZERO);
	if (fake_sd == NULL) {
		printf("%s: not enough memory for fake discipline\n",
		    DEVNAME(sc));
		goto nosd;
	}
	fake_sd->sd_sc = sc;
	fake_sd->sd_meta_type = SR_META_F_NATIVE;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (label.d_partitions[i].p_fstype != FS_RAID)
			continue;

		/* open partition */
		devr = MAKEDISKDEV(majdev, dv->dv_unit, i);
		error = (*bdsw->d_open)(devr, FREAD, S_IFCHR, curproc);
		if (error) {
			DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe "
			    "open failed, partition %d\n",
			    DEVNAME(sc), i);
			continue;
		}

		if (sr_meta_native_read(fake_sd, devr, md, NULL)) {
			printf("%s: native bootprobe could not read native "
			    "metadata\n", DEVNAME(sc));
			continue;
		}

		/* are we a softraid partition? */
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
				SLIST_INSERT_HEAD(mlh, mle, sml_link);
				rv = SR_META_CLAIMED;
			}
		}

		/* we are done, close partition */
		error = (*bdsw->d_close)(devr, FREAD, S_IFCHR, curproc);
		if (error) {
			DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe "
			    "close failed\n", DEVNAME(sc));
			continue;
		}
	}

	free(fake_sd, M_DEVBUF);
nosd:
	free(md, M_DEVBUF);
done:
	return (rv);
}

int
sr_boot_assembly(struct sr_softc *sc)
{
	struct device		*dv;
	struct sr_metadata_list_head mlh;
	struct sr_metadata_list *mle, *mle2;
	struct sr_metadata	*m1, *m2;
	struct bioc_createraid	bc;
	int			rv = 0, no_dev;
	dev_t			*dt = NULL;

	DNPRINTF(SR_D_META, "%s: sr_boot_assembly\n", DEVNAME(sc));

	SLIST_INIT(&mlh);

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class != DV_DISK)
			continue;

		/* XXX is there  a better way of excluding some devices? */
		if (!strncmp(dv->dv_xname, "fd", 2) ||
		    !strncmp(dv->dv_xname, "cd", 2) ||
		    !strncmp(dv->dv_xname, "rx", 2))
			continue;

		/* native softraid uses partitions */
		if (sr_meta_native_bootprobe(sc, dv, &mlh) == SR_META_CLAIMED)
			continue;

		/* probe non-native disks */
	}

	/*
	 * XXX poor mans hack that doesn't keep disks in order and does not
	 * roam disks correctly.  replace this with something smarter that
	 * orders disks by volid, chunkid and uuid.
	 */
	dt = malloc(BIOC_CRMAXLEN, M_DEVBUF, M_WAITOK);
	SLIST_FOREACH(mle, &mlh, sml_link) {
		/* chunk used already? */
		if (mle->sml_used)
			continue;

		no_dev = 0;
		m1 = (struct sr_metadata *)&mle->sml_metadata;
		bzero(dt, BIOC_CRMAXLEN);
		SLIST_FOREACH(mle2, &mlh, sml_link) {
			/* chunk used already? */
			if (mle2->sml_used)
				continue;

			m2 = (struct sr_metadata *)&mle2->sml_metadata;

			/* are we the same volume? */
			if (m1->ssdi.ssd_volid != m2->ssdi.ssd_volid)
				continue;

			/* same uuid? */
			if (bcmp(&m1->ssdi.ssd_uuid, &m2->ssdi.ssd_uuid,
			    sizeof(m1->ssdi.ssd_uuid)))
				continue;

			/* sanity */
			if (dt[m2->ssdi.ssd_chunk_id]) {
				printf("%s: chunk id already in use; can not "
				    "assemble volume\n", DEVNAME(sc));
				goto unwind;
			}
			dt[m2->ssdi.ssd_chunk_id] = mle2->sml_mm;
			no_dev++;
			mle2->sml_used = 1;
		}
		if (m1->ssdi.ssd_chunk_no != no_dev) {
			printf("%s: not assembling partial disk that used to "
			    "be volume %d\n", DEVNAME(sc),
			    m1->ssdi.ssd_volid);
			continue;
		}

		bzero(&bc, sizeof(bc));
		bc.bc_level = m1->ssdi.ssd_level;
		bc.bc_dev_list_len = no_dev * sizeof(dev_t);
		bc.bc_dev_list = dt;
		bc.bc_flags = BIOC_SCDEVT;
		sr_ioctl_createraid(sc, &bc, 0);
		rv++;
	}

	/* done with metadata */
unwind:
	for (mle = SLIST_FIRST(&mlh); mle != SLIST_END(&mlh); mle = mle2) {
		mle2 = SLIST_NEXT(mle, sml_link);
		free(mle, M_DEVBUF);
	}
	SLIST_INIT(&mlh);

	if (dt)
		free(dt, M_DEVBUF);

	return (rv);
}

int
sr_meta_native_probe(struct sr_softc *sc, struct sr_chunk *ch_entry)
{
	struct disklabel	label;
	char			*devname;
	int			error, part;
	daddr64_t		size;
	struct bdevsw		*bdsw;
	dev_t			dev;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_probe(%s)\n",
	   DEVNAME(sc), ch_entry->src_devname);

	dev = ch_entry->src_dev_mm;
	devname = ch_entry->src_devname;
	bdsw = bdevsw_lookup(dev);
	part = DISKPART(dev);

	/* get disklabel */
	error = bdsw->d_ioctl(dev, DIOCGDINFO, (void *)&label, FREAD, curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: %s can't obtain disklabel\n",
		    DEVNAME(sc), devname);
		goto unwind;
	}

	/* make sure the partition is of the right type */
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		DNPRINTF(SR_D_META,
		    "%s: %s partition not of type RAID (%d)\n", DEVNAME(sc) ,
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

	md = malloc(SR_META_SIZE * 512 , M_DEVBUF, M_ZERO);
	if (md == NULL) {
		printf("%s: not enough memory for metadata buffer\n",
		    DEVNAME(sc));
		goto bad;
	}

	bzero(&uuid, sizeof uuid);

	sr = not_sr = d = 0;
	SLIST_FOREACH(ch_entry, cl, src_link) {
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
		/* XXX make this smart so that we can bring up degraded disks */
		printf("%s: not all chunks were provided\n", DEVNAME(sc));
		goto bad;
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

struct scsi_adapter sr_switch = {
	sr_scsi_cmd, sr_minphys, NULL, NULL, sr_scsi_ioctl
};

struct scsi_device sr_dev = {
	NULL, NULL, NULL, NULL
};

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

	if (bio_register(&sc->sc_dev, sr_ioctl) != 0)
		printf("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = sr_ioctl;

	printf("\n");

	sr_boot_assembly(sc);
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

	while ((ccb = TAILQ_FIRST(&wu->swu_ccb)) != NULL) {
		TAILQ_REMOVE(&wu->swu_ccb, ccb, ccb_link);
		sr_ccb_put(ccb);
	}
	TAILQ_INIT(&wu->swu_ccb);

	TAILQ_INSERT_TAIL(&sd->sd_wu_freeq, wu, swu_link);
	sd->sd_wu_pending--;

	splx(s);
}

struct sr_workunit *
sr_wu_get(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;
	int			s;

	s = splbio();

	wu = TAILQ_FIRST(&sd->sd_wu_freeq);
	if (wu) {
		TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);
		wu->swu_state = SR_WU_INPROGRESS;
	}
	sd->sd_wu_pending++;

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
	struct sr_workunit	*wu;
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
			wu = NULL;
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

	if ((wu = sr_wu_get(sd)) == NULL) {
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
	if (sd->sd_scsi_sense.error_code) {
		xs->error = XS_SENSE;
		bcopy(&sd->sd_scsi_sense, &xs->sense, sizeof(xs->sense));
		bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));
	} else {
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
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
	bi->bi_novol = vol;
	bi->bi_nodisk = disk;

	return (0);
}

int
sr_ioctl_vol(struct sr_softc *sc, struct bioc_vol *bv)
{
	int			i, vol, rv = EINVAL;
	struct sr_discipline	*sd;

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
		strlcpy(bv->bv_dev, sd->sd_meta->ssd_devname,
		    sizeof(bv->bv_dev));
		strlcpy(bv->bv_vendor, sd->sd_meta->ssdi.ssd_vendor,
		    sizeof(bv->bv_vendor));
		rv = 0;
		break;
	}

	return (rv);
}

int
sr_ioctl_disk(struct sr_softc *sc, struct bioc_disk *bd)
{
	int			i, vol, rv = EINVAL, id;
	struct sr_chunk		*src;

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bd->bd_volid)
			continue;

		id = bd->bd_diskid;
		if (id >= sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no)
			break;

		src = sc->sc_dis[i]->sd_vol.sv_chunks[id];
		bd->bd_status = src->src_meta.scm_status;
		bd->bd_size = src->src_meta.scmi.scm_size << DEV_BSHIFT;
		bd->bd_channel = vol;
		bd->bd_target = id;
		strlcpy(bd->bd_vendor, src->src_meta.scmi.scm_devname,
		    sizeof(bd->bd_vendor));
		rv = 0;
		break;
	}

	return (rv);
}

int
sr_ioctl_setstate(struct sr_softc *sc, struct bioc_setstate *bs)
{
	int			rv = EINVAL;

#ifdef SR_UNIT_TEST
	int			i, vol, state, found, tg;
	struct sr_discipline	*sd;
	struct sr_chunk		*ch_entry;
	struct sr_chunk_head 	*cl;

	if (bs->bs_other_id_type == BIOC_SSOTHER_UNUSED)
		goto done;

	for (i = 0, vol = -1; i < SR_MAXSCSIBUS; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bs->bs_volid)
			continue;
		sd = sc->sc_dis[i];

		found = 0;
		tg = 0;
		cl = &sd->sd_vol.sv_chunk_list;
		SLIST_FOREACH(ch_entry, cl, src_link) {
			if (ch_entry->src_dev_mm == bs->bs_other_id) {
				found = 1;
				break;
			}
			tg++;
		}
		if (found == 0)
			goto done;

		switch (bs->bs_status) {
		case BIOC_SSONLINE:
			state = BIOC_SDONLINE;
			break;
		case BIOC_SSOFFLINE:
			state = BIOC_SDOFFLINE;
			break;
		case BIOC_SSHOTSPARE:
			state = BIOC_SDHOTSPARE;
			break;
		case BIOC_SSREBUILD:
			state = BIOC_SDREBUILD;
			break;
		default:
			printf("invalid state %d\n", bs->bs_status);
			goto done;
		}

		sd->sd_set_chunk_state(sd, tg, bs->bs_status);

		rv = 0;

		break;
	}

done:
#endif
	return (rv);
}

int
sr_ioctl_createraid(struct sr_softc *sc, struct bioc_createraid *bc, int user)
{
	dev_t			*dt;
	int			i, s, no_chunk, rv = EINVAL, vol;
	int			no_meta, updatemeta = 0, disk = 1;
	u_int64_t		vol_size;
	int32_t			strip_size = 0;
	struct sr_chunk_head	*cl;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*ch_entry;
	struct device		*dev, *dev2;
	struct scsibus_attach_args saa;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_createraid(%d)\n",
	    DEVNAME(sc), user);

	/* user input */
	if (bc->bc_dev_list_len > BIOC_CRMAXLEN)
		goto unwind;

	dt = malloc(bc->bc_dev_list_len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (user)
		copyin(bc->bc_dev_list, dt, bc->bc_dev_list_len);
	else
		bcopy(bc->bc_dev_list, dt, bc->bc_dev_list_len);

	sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_WAITOK | M_ZERO);
	sd->sd_sc = sc;

	no_chunk = bc->bc_dev_list_len / sizeof(dev_t);
	cl = &sd->sd_vol.sv_chunk_list;
	SLIST_INIT(cl);

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

		/* no metadata available */
		switch (bc->bc_level) {
		case 0:
			if (no_chunk < 2)
				goto unwind;
			strlcpy(sd->sd_name, "RAID 0", sizeof(sd->sd_name));
			/*
			 * XXX add variable strip size later even though
			 * MAXPHYS is really the clever value, users like
			 * to tinker with that type of stuff
			 */
			strip_size = MAXPHYS;
			vol_size =
			    ch_entry->src_meta.scmi.scm_coerced_size * no_chunk;
			break;
		case 1:
			if (no_chunk < 2)
				goto unwind;
			strlcpy(sd->sd_name, "RAID 1", sizeof(sd->sd_name));
			vol_size = ch_entry->src_meta.scmi.scm_coerced_size;
			break;
#ifdef AOE
#ifdef not_yet
		case 'A':
			/* target */
			if (no_chunk != 1)
				goto unwind;
			strlcpy(sd->sd_name, "AOE TARG", sizeof(sd->sd_name));
			vol_size = ch_entry->src_meta.scmi.scm_coerced_size;
			break;
		case 'a':
			/* initiator */
			if (no_chunk != 1)
				goto unwind;
			strlcpy(sd->sd_name, "AOE INIT", sizeof(sd->sd_name));
			break;
#endif /* not_yet */
#endif /* AOE */
#ifdef CRYPTO
		case 'C':
			DNPRINTF(SR_D_IOCTL,
			    "%s: sr_ioctl_createraid: no_chunk %d\n",
			    DEVNAME(sc), no_chunk);

			if (no_chunk != 1)
				goto unwind;

			/* no hint available yet */
			if (bc->bc_opaque_flags & BIOC_SOOUT) {
				bc->bc_opaque_status = BIOC_SOINOUT_FAILED;
				rv = 0;
				goto unwind;
			}

			if (!(bc->bc_flags & BIOC_SCNOAUTOASSEMBLE))
				goto unwind;

			if (sr_crypto_get_kdf(bc, sd))
				goto unwind;

			strlcpy(sd->sd_name, "CRYPTO", sizeof(sd->sd_name));
			vol_size = ch_entry->src_meta.scmi.scm_size;

			sr_crypto_create_keys(sd);

			break;
#endif /* CRYPTO */
		default:
			goto unwind;
		}

		/* fill out all volume metadata */
		DNPRINTF(SR_D_IOCTL,
		    "%s: sr_ioctl_createraid: vol_size: %lld\n",
		    DEVNAME(sc), vol_size);
		sd->sd_meta->ssdi.ssd_chunk_no = no_chunk;
		sd->sd_meta->ssdi.ssd_size = vol_size;
		sd->sd_vol_status = BIOC_SVONLINE;
		sd->sd_meta->ssdi.ssd_level = bc->bc_level;
		sd->sd_meta->ssdi.ssd_strip_size = strip_size;
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
#ifdef CRYPTO
		/* provide userland with kdf hint */
		if (bc->bc_opaque_flags & BIOC_SOOUT) {
			if (bc->bc_opaque == NULL)
				goto unwind;

			if (sizeof(sd->mds.mdd_crypto.scr_meta.scm_kdfhint) <
			    bc->bc_opaque_size)
				goto unwind;

			if (copyout(sd->mds.mdd_crypto.scr_meta.scm_kdfhint,
			    bc->bc_opaque, bc->bc_opaque_size))
				goto unwind;

			/* we're done */
			bc->bc_opaque_status = BIOC_SOINOUT_OK;
			rv = 0;
			goto unwind;
		}
		/* get kdf with maskkey from userland */
		if (bc->bc_opaque_flags & BIOC_SOIN) {
			if (sr_crypto_get_kdf(bc, sd))
				goto unwind;
		}
#endif	/* CRYPTO */
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
		printf("%s: trying to bring up %s degraded\n", DEVNAME(sc),
		    sd->sd_meta->ssd_devname);
	}

	/* metadata SHALL be fully filled in at this point */

	if (sr_discipline_init(sd, bc->bc_level)) {
		printf("%s: could not initialize discipline\n", DEVNAME(sc));
		goto unwind;
	}

	/* allocate all resources */
	if ((rv = sd->sd_alloc_resources(sd)))
		goto unwind;

	if (disk) {
		/* set volume status */
		sd->sd_set_vol_state(sd);

		/* setup scsi midlayer */
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

	return (rv);
unwind:
	sr_discipline_shutdown(sd);

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

void
sr_chunks_unwind(struct sr_softc *sc, struct sr_chunk_head *cl)
{
	struct sr_chunk		*ch_entry, *ch_next;
	dev_t			dev;

	DNPRINTF(SR_D_IOCTL, "%s: sr_chunks_unwind\n", DEVNAME(sc));

	if (!cl)
		return;

	for (ch_entry = SLIST_FIRST(cl);
	    ch_entry != SLIST_END(cl); ch_entry = ch_next) {
		ch_next = SLIST_NEXT(ch_entry, src_link);

		dev = ch_entry->src_dev_mm;
		DNPRINTF(SR_D_IOCTL, "%s: sr_chunks_unwind closing: %s\n",
		    DEVNAME(sc), ch_entry->src_devname);
		if (dev != NODEV)
			bdevsw_lookup(dev)->d_close(dev, FWRITE, S_IFBLK,
			    curproc);

		free(ch_entry, M_DEVBUF);
	}
	SLIST_INIT(cl);
}

void
sr_discipline_free(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			i;

	if (!sd)
		return;

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
		bdevsw_lookup(ccb->ccb_buf.b_dev)->d_strategy(&ccb->ccb_buf);
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
