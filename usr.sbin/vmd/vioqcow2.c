/*	$OpenBSD: vioqcow2.c,v 1.3 2018/09/19 04:29:21 ccardenas Exp $	*/

/*
 * Copyright (c) 2018 Ori Bernstein <ori@eigenstate.org>
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

#include <sys/types.h>
#include <sys/stat.h>

#include <machine/vmmvar.h>
#include <dev/pci/pcireg.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <err.h>

#include "vmd.h"
#include "vmm.h"
#include "virtio.h"

#define QCOW2_COMPRESSED	0x4000000000000000ull
#define QCOW2_INPLACE		0x8000000000000000ull

#define QCOW2_DIRTY		(1 << 0)
#define QCOW2_CORRUPT		(1 << 1)

enum {
	ICFEATURE_DIRTY		= 1 << 0,
	ICFEATURE_CORRUPT	= 1 << 1,
};

enum {
	ACFEATURE_BITEXT	= 1 << 0,
};

struct qcheader {
	char magic[4];
	uint32_t version;
	uint64_t backingoff;
	uint32_t backingsz;
	uint32_t clustershift;
	uint64_t disksz;
	uint32_t cryptmethod;
	uint32_t l1sz;
	uint64_t l1off;
	uint64_t refoff;
	uint32_t refsz;
	uint32_t snapcount;
	uint64_t snapsz;
	/* v3 additions */
	uint64_t incompatfeatures;
	uint64_t compatfeatures;
	uint64_t autoclearfeatures;
	uint32_t reforder;	/* Bits = 1 << reforder */
	uint32_t headersz;
} __packed;

struct qcdisk {
	pthread_rwlock_t lock;
	struct qcdisk *base;
	struct qcheader header;

	int       fd;
	uint64_t *l1;
	off_t     end;
	uint32_t  clustersz;
	off_t	  disksz; /* In bytes */
	uint32_t cryptmethod;

	uint32_t l1sz;
	off_t	 l1off;

	off_t	 refoff;
	uint32_t refsz;

	uint32_t nsnap;
	off_t	 snapoff;

	/* v3 features */
	uint64_t incompatfeatures;
	uint64_t autoclearfeatures;
	uint32_t refssz;
	uint32_t headersz;
};

extern char *__progname;

static off_t xlate(struct qcdisk *, off_t, int *);
static int copy_cluster(struct qcdisk *, struct qcdisk *, off_t, off_t);
static off_t mkcluster(struct qcdisk *, struct qcdisk *, off_t, off_t);
static int inc_refs(struct qcdisk *, off_t, int);
static int qc2_openpath(struct qcdisk *, char *, int);
static int qc2_open(struct qcdisk *, int);
static ssize_t qc2_pread(void *, char *, size_t, off_t);
static ssize_t qc2_pwrite(void *, char *, size_t, off_t);
static void qc2_close(void *);

/*
 * Initializes a raw disk image backing file from an fd.
 * Stores the number of 512 byte sectors in *szp,
 * returning -1 for error, 0 for success.
 *
 * May open snapshot base images.
 */
int
virtio_init_qcow2(struct virtio_backing *file, off_t *szp, int fd)
{
	struct qcdisk *diskp;

	diskp = malloc(sizeof(struct qcdisk));
	if (diskp == NULL)
		return -1;
	if (qc2_open(diskp, fd) == -1) {
		log_warnx("%s: could not open qcow2 disk", __func__);
		free(diskp);
		return -1;
	}
	file->p = diskp;
	file->pread = qc2_pread;
	file->pwrite = qc2_pwrite;
	file->close = qc2_close;
	*szp = diskp->disksz;
	return 0;
}

static int
qc2_openpath(struct qcdisk *disk, char *path, int flags)
{
	int fd;

	fd = open(path, flags);
	if (fd < 0)
		return -1;
	return qc2_open(disk, fd);
}

static int
qc2_open(struct qcdisk *disk, int fd)
{
	char basepath[PATH_MAX];
	struct stat st;
	struct qcheader header;
	uint64_t backingoff;
	uint32_t backingsz;
	size_t i;
	int version;

	pthread_rwlock_init(&disk->lock, NULL);
	disk->fd = fd;
	disk->base = NULL;
	disk->l1 = NULL;

	if (pread(fd, &header, sizeof header, 0) != sizeof header) {
		log_warn("%s: short read on header", __func__);
		goto error;
	}
	if (strncmp(header.magic, "QFI\xfb", 4) != 0) {
		log_warn("%s: invalid magic numbers", __func__);
		goto error;
	}

	disk->clustersz		= (1ull << be32toh(header.clustershift));
	disk->disksz		= be64toh(header.disksz);
	disk->cryptmethod	= be32toh(header.cryptmethod);
	disk->l1sz		= be32toh(header.l1sz);
	disk->l1off		= be64toh(header.l1off);
	disk->refsz		= be32toh(header.refsz);
	disk->refoff		= be64toh(header.refoff);
	disk->nsnap		= be32toh(header.snapcount);
	disk->snapoff		= be64toh(header.snapsz);

	/*
	 * The additional features here are defined as 0 in the v2 format,
	 * so as long as we clear the buffer before parsing, we don't need
	 * to check versions here.
	 */
	disk->incompatfeatures = be64toh(header.incompatfeatures);
	disk->autoclearfeatures = be64toh(header.autoclearfeatures);
	disk->refssz = be32toh(header.refsz);
	disk->headersz = be32toh(header.headersz);

	/*
	 * We only know about the dirty or corrupt bits here.
	 */
	if (disk->incompatfeatures & ~(QCOW2_DIRTY|QCOW2_CORRUPT)) {
		log_warnx("%s: unsupported features %llx", __func__,
		    disk->incompatfeatures & ~(QCOW2_DIRTY|QCOW2_CORRUPT));
		goto error;
	}

	disk->l1 = calloc(disk->l1sz, sizeof *disk->l1);
	if (!disk->l1)
		goto error;
	if (pread(disk->fd, disk->l1, 8*disk->l1sz, disk->l1off)
	    != 8*disk->l1sz) {
		log_warn("%s: unable to read qcow2 L1 table", __func__);
		goto error;
	}
	for (i = 0; i < disk->l1sz; i++)
		disk->l1[i] = be64toh(disk->l1[i]);
	version = be32toh(header.version);
	if (version != 2 && version != 3) {
		log_warn("%s: unknown qcow2 version %d", __func__, version);
		goto error;
	}

	backingoff = be64toh(header.backingoff);
	backingsz = be32toh(header.backingsz);
	if (backingsz != 0) {
		/*
		 * FIXME: we need to figure out a way of opening these things,
		 * otherwise we just crash with a pledge violation.
		 */
		log_warn("%s: unsupported external snapshot images", __func__);
		goto error;

		if (backingsz >= sizeof basepath - 1) {
			log_warn("%s: snapshot path too long", __func__);
			goto error;
		}
		if (pread(fd, basepath, backingsz, backingoff) != backingsz) {
			log_warn("%s: could not read snapshot base name",
			    __func__);
			goto error;
		}
		basepath[backingsz] = 0;

		disk->base = calloc(1, sizeof(struct qcdisk));
		if (!disk->base)
			goto error;
		if (qc2_openpath(disk->base, basepath, O_RDONLY) == -1) {
			log_warn("%s: could not open %s", basepath, __func__);
			goto error;
		}
		if (disk->base->clustersz != disk->clustersz) {
			log_warn("%s: all disks must share clustersize",
			    __func__);
			goto error;
		}
	}
	if (fstat(fd, &st) == -1) {
		log_warn("%s: unable to stat disk", __func__);
		goto error;
	}

	disk->end = st.st_size;

	log_debug("opened qcow2 disk version %d:", version);
	log_debug("size:\t%lld", disk->disksz);
	log_debug("end:\t%lld", disk->end);
	log_debug("nsnap:\t%d", disk->nsnap);
	return 0;
error:
	qc2_close(disk);
	return -1;
}

static ssize_t
qc2_pread(void *p, char *buf, size_t len, off_t off)
{
	struct qcdisk *disk, *d;
	off_t phys_off, end, cluster_off;
	ssize_t sz, rem;

	disk = p;
	end = off + len;
	if (off < 0 || end > disk->disksz)
		return -1;

	/* handle head chunk separately */
	rem = len;
	while (off != end) {
		for (d = disk; d; d = d->base)
			if ((phys_off = xlate(d, off, NULL)) > 0)
				break;
		/* Break out into chunks. This handles
		 * three cases:
		 *
		 *    |----+====|========|====+    |
		 *
		 * Either we are at the start of the read,
		 * and the cluster has some leading bytes.
		 * This means that we are reading the tail
		 * of the cluster, and our size is:
		 *
		 * 	clustersz - (off % clustersz).
		 *
		 * Otherwise, we're reading the middle section.
		 * We're already aligned here, so we can just
		 * read the whole cluster size. Or we're at the
		 * tail, at which point we just want to read the
		 * remaining bytes.
		 */
		cluster_off = off % disk->clustersz;
		sz = disk->clustersz - cluster_off;
		if (sz > rem)
			sz = rem;
		/*
		 * If we're within the disk, but don't have backing bytes,
		 * just read back zeros.
		 */
		if (!d)
			bzero(buf, sz);
		else if (pread(d->fd, buf, sz, phys_off) != sz)
			return -1;
		off += sz;
		buf += sz;
		rem -= sz;
	}
	return len;
}

ssize_t
qc2_pwrite(void *p, char *buf, size_t len, off_t off)
{
	struct qcdisk *disk, *d;
	off_t phys_off, cluster_off, end;
	ssize_t sz, rem;
	int inplace;

	d = p;
	disk = p;
	inplace = 1;
	end = off + len;
	if (off < 0 || end > disk->disksz)
		return -1;
	rem = len;
	while (off != end) {
		/* See the read code for a summary of the computation */
		cluster_off = off % disk->clustersz;
		sz = disk->clustersz - cluster_off;
		if (sz > rem)
			sz = rem;

		phys_off = xlate(disk, off, &inplace);
		if (phys_off == -1)
			return -1;
		/*
		 * If we couldn't find the cluster in the writable disk,
		 * see if it exists in the base image. If it does, we
		 * need to copy it before the write. The copy happens
		 * in the '!inplace' if clause below te search.
		 */
		if (phys_off == 0)
			for (d = disk->base; d; d = d->base)
				if ((phys_off = xlate(d, off, NULL)) > 0)
					break;
		if (!inplace || phys_off == 0)
			phys_off = mkcluster(disk, d, off, phys_off);
		if (phys_off == -1)
			return -1;
		if (pwrite(disk->fd, buf, sz, phys_off) != sz)
			return -1;
		off += sz;
		buf += sz;
		rem -= sz;
	}
	return len;
}

static void
qc2_close(void *p)
{
	struct qcdisk *disk;

	disk = p;
	if (disk->base)
		qc2_close(disk->base);
	close(disk->fd);
	free(disk->l1);
	free(disk);
}

/*
 * Translates a virtual offset into an on-disk offset.
 * Returns:
 * 	-1 on error
 * 	 0 on 'not found'
 * 	>0 on found
 */
static off_t
xlate(struct qcdisk *disk, off_t off, int *inplace)
{
	off_t l2sz, l1off, l2tab, l2off, cluster, clusteroff;
	uint64_t buf;


	/*
	 * Clear out inplace flag -- xlate misses should not
	 * be flagged as updatable in place. We will still
	 * return 0 from them, but this leaves less surprises
	 * in the API.
	 */
	if (inplace)
		*inplace = 0;
	pthread_rwlock_rdlock(&disk->lock);
	if (off < 0)
		goto err;

	l2sz = disk->clustersz / 8;
	l1off = (off / disk->clustersz) / l2sz;
	if (l1off >= disk->l1sz)
		goto err;

	l2tab = disk->l1[l1off];
	l2tab &= ~QCOW2_INPLACE;
	if (l2tab == 0) {
		pthread_rwlock_unlock(&disk->lock);
		return 0;
	}
	l2off = (off / disk->clustersz) % l2sz;
	pread(disk->fd, &buf, sizeof(buf), l2tab + l2off*8);
	cluster = be64toh(buf);
	/*
	 * cluster may be 0, but all future operations don't affect
	 * the return value.
	 */
	if (inplace)
		*inplace = !!(cluster & QCOW2_INPLACE);
	if (cluster & QCOW2_COMPRESSED) {
		log_warn("%s: compressed clusters unsupported", __func__);
		goto err;
	}
	pthread_rwlock_unlock(&disk->lock);
	clusteroff = 0;
	cluster &= ~QCOW2_INPLACE;
	if (cluster)
		clusteroff = off % disk->clustersz;
	return cluster + clusteroff;
err:
	pthread_rwlock_unlock(&disk->lock);
	return -1;
}

/*
 * Allocates a new cluster on disk, creating a new L2 table
 * if needed. The cluster starts off with a refs of one,
 * and the writable bit set.
 *
 * Returns -1 on error, and the physical address within the
 * cluster of the write offset if it exists.
 */
static off_t
mkcluster(struct qcdisk *disk, struct qcdisk *base, off_t off, off_t src_phys)
{
	off_t l2sz, l1off, l2tab, l2off, cluster, clusteroff, orig;
	uint64_t buf;
	int fd;

	pthread_rwlock_wrlock(&disk->lock);

	cluster = -1;
	fd = disk->fd;
	/* L1 entries always exist */
	l2sz = disk->clustersz / 8;
	l1off = off / (disk->clustersz * l2sz);
	if (l1off >= disk->l1sz)
		goto fail;

	/*
	 * Align disk to cluster size, for ftruncate: Not strictly
	 * required, but it easier to eyeball buggy write offsets,
	 * and helps performance a bit.
	 */
	disk->end = (disk->end + disk->clustersz - 1) & ~(disk->clustersz - 1);

	l2tab = disk->l1[l1off];
	l2off = (off / disk->clustersz) % l2sz;
	/* We may need to create or clone an L2 entry to map the block */
	if (l2tab == 0 || (l2tab & QCOW2_INPLACE) == 0) {
		orig = l2tab & ~QCOW2_INPLACE;
		l2tab = disk->end;
		disk->end += disk->clustersz;
		if (ftruncate(disk->fd, disk->end) == -1) {
			perror("ftruncate");
			goto fail;
		}

		/*
		 * If we translated, found a L2 entry, but it needed to
		 * be copied, copy it.
		 */
		if (orig != 0 && copy_cluster(disk, disk, l2tab, orig) == -1) {
			perror("move cluster");
			goto fail;
		}
		/* Update l1 -- we flush it later */
		disk->l1[l1off] = l2tab | QCOW2_INPLACE;
		if (inc_refs(disk, l2tab, 1) == -1) {
			perror("refs");
			goto fail;
		}
	}
	l2tab &= ~QCOW2_INPLACE;

	/* Grow the disk */
	if (ftruncate(disk->fd, disk->end + disk->clustersz) < 0)
		goto fail;
	if (src_phys > 0)
		if (copy_cluster(disk, base, disk->end, src_phys) == -1)
			goto fail;
	cluster = disk->end;
	disk->end += disk->clustersz;
	buf = htobe64(cluster | QCOW2_INPLACE);
	if (pwrite(disk->fd, &buf, sizeof buf, l2tab + l2off*8) != sizeof(buf))
		goto fail;

	/* TODO: lazily sync: currently VMD doesn't close things */
	buf = htobe64(disk->l1[l1off]);
	if (pwrite(disk->fd, &buf, sizeof buf, disk->l1off + 8*l1off) != 8)
		goto fail;
	if (inc_refs(disk, cluster, 1) == -1)
		goto fail;

	pthread_rwlock_unlock(&disk->lock);
	clusteroff = off % disk->clustersz;
	return cluster + clusteroff;

fail:
	pthread_rwlock_unlock(&disk->lock);
	return -1;
}

/* Copies a cluster containing src to dst. Src and dst need not be aligned. */
static int
copy_cluster(struct qcdisk *disk, struct qcdisk *base, off_t dst, off_t src)
{
	char *scratch;

	scratch = alloca(disk->clustersz);
	if (!scratch)
		err(1, "out of memory");
	src &= ~(disk->clustersz - 1);
	dst &= ~(disk->clustersz - 1);
	if (pread(base->fd, scratch, disk->clustersz, src) == -1)
		return -1;
	if (pwrite(disk->fd, scratch, disk->clustersz, dst) == -1)
		return -1;
	return 0;
}

static int
inc_refs(struct qcdisk *disk, off_t off, int newcluster)
{
	off_t l1off, l1idx, l2idx, l2cluster;
	size_t nper;
	uint16_t refs;
	uint64_t buf;

	off &= ~QCOW2_INPLACE;
	nper = disk->clustersz / 2;
	l1idx = (off / disk->clustersz) / nper;
	l2idx = (off / disk->clustersz) % nper;
	l1off = disk->refoff + 8*l1idx;
	if (pread(disk->fd, &buf, sizeof buf, l1off) != 8)
		return -1;

	l2cluster = be64toh(buf);
	if (l2cluster == 0) {
		l2cluster = disk->end;
		disk->end += disk->clustersz;
		if (ftruncate(disk->fd, disk->end) < 0) {
			log_warn("%s: refs block grow fail", __func__);
			return -1;
		}
		buf = htobe64(l2cluster);
		if (pwrite(disk->fd, &buf, sizeof buf, l1off) != 8) {
			return -1;
		}
	}

	refs = 1;
	if (!newcluster) {
		if (pread(disk->fd, &refs, sizeof refs, l2cluster+2*l2idx) != 2)
			return -1;
		refs = be16toh(refs) + 1;
	}
	refs = htobe16(refs);
	if (pwrite(disk->fd, &refs, sizeof refs, l2cluster + 2*l2idx) != 2) {
		log_warn("%s: could not write ref block", __func__);
		return -1;
	}
	return 0;
}

