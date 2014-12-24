/*	$OpenBSD: disksubr.c,v 1.60 2014/12/24 22:48:27 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
 * Copyright (c) 1996 Theo de Raadt
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/malloc.h>

char	*extract_vdit_portion(char *, const char *, unsigned int, unsigned int,
	    int);
int	 readvditlabel(struct buf *, void (*)(struct buf *), struct disklabel *,
	    daddr_t *, int, struct vdm_boot_info *);
int	 readvdmlabel(struct buf *, void (*)(struct buf *), struct disklabel *,
	    daddr_t *, int);

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 */
int
readdisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp,
    int spoofonly)
{
	struct buf *bp = NULL;
	int error;

	if ((error = initdisklabel(lp)))
		goto done;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/*
	 * Check for a VDIT or native-in-VDM label first.
	 * If a valid VDM signature is found, but neither a VDIT nor a
	 * native label are found, do not attempt to check for any other
	 * label scheme.
	 */
	error = readvdmlabel(bp, strat, lp, NULL, spoofonly);
	if (error == 0)
		goto done;
	if (error == ENOENT) {
		error = EINVAL;
		goto done;
	}

	error = readdoslabel(bp, strat, lp, NULL, spoofonly);
	if (error == 0)
		goto done;

#if defined(CD9660)
	error = iso_disklabelspoof(dev, strat, lp);
	if (error == 0)
		goto done;
#endif
#if defined(UDF)
	error = udf_disklabelspoof(dev, strat, lp);
	if (error == 0)
		goto done;
#endif

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	disk_change = 1;
	return (error);
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp)
{
	daddr_t partoff = -1;
	int error = EIO;
	int offset;
	struct disklabel *dlp;
	struct buf *bp = NULL;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	error = readvdmlabel(bp, strat, lp, &partoff, 1);
	if (error == 0 || error == ENOENT) {
		bp->b_blkno = partoff + LABELSECTOR;
		offset = LABELOFFSET;
	} else if (readdoslabel(bp, strat, lp, &partoff, 1) == 0) {
		bp->b_blkno = DL_BLKTOSEC(lp, partoff + DOS_LABELSECTOR) *
		    DL_BLKSPERSEC(lp);
		offset = DL_BLKOFFSET(lp, partoff + DOS_LABELSECTOR);
	} else {
		error = EIO;
		goto done;
	}

	/* Read it in, slap the new label in, and write it back out */
	bp->b_bcount = lp->d_secsize;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

	dlp = (struct disklabel *)(bp->b_data + offset);
	*dlp = *lp;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_WRITE | B_RAW);
	(*strat)(bp);
	error = biowait(bp);

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	disk_change = 1;
	return (error);
}

/*
 * Search for a VDM "label" (which does not describe any partition).
 * If one is found, search for either a VDIT label, or a native OpenBSD
 * label in the first sector.
 */
int
readvdmlabel(struct buf *bp, void (*strat)(struct buf *), struct disklabel *lp,
    daddr_t *partoffp, int spoofonly)
{
	struct vdm_label *vdl;
	struct vdm_boot_info *vbi;
	int error = 0;

	/*
	 * Read first sector and check for a VDM label.
	 * Note that a VDM label is only required for bootable disks, and
	 * may not be followed by a VDIT.
	 */

	bp->b_blkno = VDM_LABEL_SECTOR;
	bp->b_bcount = lp->d_secsize;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		return error;

	vdl = (struct vdm_label *)(bp->b_data + VDM_LABEL_OFFSET);
	if (vdl->signature != VDM_LABEL_SIGNATURE)
		vdl = (struct vdm_label *)(bp->b_data + VDM_LABEL_OFFSET_ALT);
	if (vdl->signature != VDM_LABEL_SIGNATURE)
		return EINVAL;

	/*
	 * If the disk is a bootable disk, remember the boot block area, to
	 * be able to check that the VDIT does not overwrite it.
	 */

	vbi = (struct vdm_boot_info *)(bp->b_data + dbtob(1) - sizeof *vbi);
	if (vbi->signature != VDM_LABEL_SIGNATURE || vbi->boot_start == 0)
		vbi = NULL;

	if (vbi != NULL && vbi->boot_start == VDIT_SECTOR)
		return EINVAL;

	if (vbi != NULL && vbi->boot_start + vbi->boot_size > DL_GETBSTART(lp))
		DL_SETBSTART(lp, vbi->boot_start + vbi->boot_size);

	error = readvditlabel(bp, strat, lp, partoffp, spoofonly, vbi);
	if (error == 0)
		return 0;

	/*
	 * Valid VDIT information, but no OpenBSD vdmpart found.
	 * Do not try to read a native label.
	 */
	if (error == ENOENT)
		return error;

	if (partoffp != NULL)
		*partoffp = 0;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly != 0)
		return 0;

	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		return error;

	return checkdisklabel(bp->b_data + LABELOFFSET, lp, 
	    DL_GETBSTART(lp), DL_GETBEND(lp));
}

/*
 * Search for a VDIT volume information. If one is found, search for a
 * vdmpart instance of name "OpenBSD". If one is found, set the disklabel
 * bounds to the area it spans, and attempt to read a native label within
 * it.
 */
int
readvditlabel(struct buf *bp, void (*strat)(struct buf *), struct disklabel *lp,
    daddr_t *partoffp, int spoofonly, struct vdm_boot_info *vbi)
{
	struct buf *sbp = NULL;
	struct vdit_block_header *vbh;
	struct vdit_entry_header *veh;
	char *vdit_storage = NULL, *vdit_end;
	size_t vdit_size;
	unsigned int largest_chunk, vdit_blkno;
	int expected_kind;
	daddr_t blkno;
	int error = 0;
	vdit_id_t *vdmpart_id;
	struct vdit_vdmpart_instance *bsd_vdmpart;

	/*
	 * Figure out the size of the first VDIT.
	 */

	vdit_size = largest_chunk = 0;
	expected_kind = VDIT_BLOCK_HEAD_BE;
	blkno = VDIT_SECTOR;
	for (;;) { 
		bp->b_blkno = blkno;
		bp->b_bcount = lp->d_secsize;
		CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
		SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
		(*strat)(bp);
		if ((error = biowait(bp)) != 0)
			return error;

		vbh = (struct vdit_block_header *)bp->b_data;
		if (VDM_ID_KIND(&vbh->id) != expected_kind ||
		    VDM_ID_BLKNO(&vbh->id) != vdit_size ||
		    vbh->id.node_number != VDM_NO_NODE_NUMBER)
			return EINVAL;

		if (vbi != NULL) {
			if ((blkno >= vbi->boot_start &&
			     blkno < vbi->boot_start + vbi->boot_size) ||
			    (blkno + vbh->chunksz - 1 >= vbi->boot_start &&
			     blkno + vbh->chunksz - 1 <
			     vbi->boot_start + vbi->boot_size))
			return EINVAL;
		}

		if (vbh->chunksz > largest_chunk)
			largest_chunk = vbh->chunksz;
		vdit_size += vbh->chunksz;
		if (vbh->nextblk == VDM_NO_BLK_NUMBER)
			break;
		blkno = vbh->nextblk;
		if (blkno >= DL_GETDSIZE(lp))
			return EINVAL;
		expected_kind = VDIT_PORTION_HEADER_BLOCK;
	}

	/*
	 * Now read the first VDIT.
	 */

	vdit_size *= dbtob(1) - sizeof(struct vdit_block_header);
	vdit_storage = malloc(vdit_size, M_DEVBUF, M_WAITOK);
	largest_chunk = dbtob(largest_chunk);
	sbp = geteblk(largest_chunk);
	sbp->b_dev = bp->b_dev;

	vdit_end = vdit_storage;
	expected_kind = VDIT_BLOCK_HEAD_BE;
	blkno = VDIT_SECTOR;
	vdit_blkno = 0;
	for (;;) {
		sbp->b_blkno = blkno;
		sbp->b_bcount = largest_chunk;
		CLR(sbp->b_flags, B_READ | B_WRITE | B_DONE);
		SET(sbp->b_flags, B_BUSY | B_READ | B_RAW);
		(*strat)(sbp);
		if ((error = biowait(sbp)) != 0)
			goto done;

		vbh = (struct vdit_block_header *)sbp->b_data;
		if (VDM_ID_KIND(&vbh->id) != expected_kind) {
			error = EINVAL;
			goto done;
		}

		vdit_end = extract_vdit_portion(vdit_end, sbp->b_data,
		    vbh->chunksz, vdit_blkno, expected_kind);
		if (vdit_end == NULL) {
			error = EINVAL;
			goto done;
		}
		if (vbh->nextblk == VDM_NO_BLK_NUMBER)
			break;
		vdit_blkno += vbh->chunksz;
		blkno = vbh->nextblk;
		expected_kind = VDIT_PORTION_HEADER_BLOCK;
	}

	/*
	 * Walk the VDIT entries.
	 *
	 * If we find an OpenBSD vdmpart, we'll set our disk area bounds to
	 * its area, and will read a label from there.
	 */

	vdmpart_id = NULL;
	bsd_vdmpart = NULL;

	veh = (struct vdit_entry_header *)vdit_storage;
	while ((caddr_t)veh < vdit_end) {
		switch (veh->type) {
		case VDIT_ENTRY_SUBDRIVER_INFO:
		    {
			struct vdit_subdriver_entry *vse;

			vse = (struct vdit_subdriver_entry *)(veh + 1);
			if (strcmp(vse->name, VDM_SUBDRIVER_VDMPART) == 0)
				vdmpart_id = &vse->subdriver_id;
		    }
			break;
		case VDIT_ENTRY_INSTANCE:
		    {
			struct vdit_instance_entry *vie;

			vie = (struct vdit_instance_entry *)(veh + 1);
			if (strcmp(vie->name, VDM_INSTANCE_OPENBSD) == 0) {
				if (vdmpart_id != NULL &&
				    memcmp(vdmpart_id, &vie->subdriver_id,
				      sizeof(vdit_id_t)) == 0) {
					/* found it! */
					if (bsd_vdmpart != NULL) {
						bsd_vdmpart = NULL;
						veh->type = VDIT_ENTRY_SENTINEL;
					} else
						bsd_vdmpart = (struct
						    vdit_vdmpart_instance *)vie;
				}
			}
		    }
			break;
		}
		if (veh->type == VDIT_ENTRY_SENTINEL)
			break;
		veh = (struct vdit_entry_header *)((char *)veh + veh->size);
	}

	if (bsd_vdmpart != NULL) {
		uint32_t start, size;

		memcpy(&start, &bsd_vdmpart->start_blkno, sizeof(uint32_t));
		memcpy(&size, &bsd_vdmpart->size, sizeof(uint32_t));

		if (start >= DL_GETDSIZE(lp) ||
		    start + size > DL_GETDSIZE(lp)) {
			error = EINVAL;
			goto done;
		}

		if (partoffp != NULL) {
			*partoffp = start;
			goto done;
		} else {
			DL_SETBSTART(lp, start);
			DL_SETBEND(lp, start + size);
		}

		/*
		 * Now read the native label.
		 */

		if (spoofonly == 0) {
			bp->b_blkno = start + LABELSECTOR;
			bp->b_bcount = lp->d_secsize;
			CLR(bp->b_flags, B_READ | B_WRITE | B_DONE);
			SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
			(*strat)(bp);
			if ((error = biowait(bp)) != 0)
				goto done;

			error = checkdisklabel(bp->b_data + LABELOFFSET, lp,
			    start, start + size);
		}
	} else {
		/*
		 * VDM label, but no OpenBSD vdmpart partition found.
		 * XXX is it worth registering the whole disk as a
		 * XXX `don't touch' vendor partition in that case?
		 */
		error = ENOENT;
		goto done;
	}

done:
	free(vdit_storage, M_DEVBUF, vdit_size);
	if (sbp != NULL) {
		sbp->b_flags |= B_INVAL;
		brelse(sbp);
	}

	return error;
}

/*
 * Process a contiguous chunk of VDIT, verifying and removing each block header
 * as we go.
 */
char *
extract_vdit_portion(char *dst, const char *src, unsigned int nsec,
    unsigned int vdit_blkno, int kind)
{
	struct vdit_block_header *vbh;

	for (; nsec != 0; nsec--) {
		vbh = (struct vdit_block_header *)src;
		if (VDM_ID_KIND(&vbh->id) != kind ||
		    VDM_ID_BLKNO(&vbh->id) != vdit_blkno ||
		    vbh->id.node_number != VDM_NO_NODE_NUMBER)
			return NULL;
		kind = VDIT_BLOCK;

		memcpy(dst, src + sizeof *vbh, dbtob(1) - sizeof *vbh);
		dst += dbtob(1) - sizeof *vbh;
		src += dbtob(1);
		vdit_blkno++;
	}

	return dst;
}
