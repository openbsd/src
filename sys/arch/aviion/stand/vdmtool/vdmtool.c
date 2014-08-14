/*	$OpenBSD: vdmtool.c,v 1.5 2014/08/14 17:55:28 tobias Exp $	*/

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

#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <sys/disklabel.h>

void	initialize(int);
void	report(int);
void	usage(void);

int	verbose;

#define	VDM_BLOCK_SIZE	0x200

void
usage()
{
	fprintf(stderr, "usage: vdmtool [-iv] rawdev\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	int fd;
	int iflag = 0;
	int exitcode = 0;
	char *realpath;

	while ((c = getopt(argc, argv, "iqv")) != -1) {
		switch (c) {
		case 'i':
			iflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 1)
		usage();

	fd = opendev(argv[optind], iflag ? O_RDWR : O_RDONLY, OPENDEV_PART,
	    &realpath);
	if (fd < 0)
		err(1, "open(%s)", realpath);

	if (iflag)
		initialize(fd);
	else
		report(fd);

	close(fd);
	exit(exitcode);
}

void
read_sector(int fd, uint32_t secno, void *buf)
{
	if (lseek(fd, (off_t)secno * VDM_BLOCK_SIZE, SEEK_SET) == -1)
		err(1, "lseek");

	if (read(fd, buf, VDM_BLOCK_SIZE) != VDM_BLOCK_SIZE)
		err(1, "read(%d,%08x)", fd, secno);
}

void
write_sector(int fd, uint32_t secno, void *buf)
{
	if (lseek(fd, (off_t)secno * VDM_BLOCK_SIZE, SEEK_SET) == -1)
		err(1, "lseek");

	if (write(fd, buf, VDM_BLOCK_SIZE) != VDM_BLOCK_SIZE)
		err(1, "write(%d,%08x)", fd, secno);
}

void
analyze_label_sector(uint8_t *sector, struct vdm_label **dl,
    struct vdm_boot_info **dbi)
{
	struct vdm_label *l;
	struct vdm_boot_info *bi;

	l = (struct vdm_label *)(sector + VDM_LABEL_OFFSET);
	if (betoh32(l->signature) != VDM_LABEL_SIGNATURE) {
		l = (struct vdm_label *)(sector + VDM_LABEL_OFFSET_ALT);
		if (betoh32(l->signature) != VDM_LABEL_SIGNATURE)
			l = NULL;
	}

	if (l != NULL) {
		bi = (struct vdm_boot_info *)
		    (sector + VDM_BLOCK_SIZE - sizeof *bi);
		if (betoh32(bi->signature) != VDM_LABEL_SIGNATURE)
			bi = NULL;
	} else
		bi = NULL;

	*dl = l;
	*dbi = bi;
}

uint32_t
get_vdit_size(int fd, uint32_t secno)
{
	uint8_t sector[VDM_BLOCK_SIZE];
	struct vdit_block_header *hdr;
	uint32_t cursize = 0;
	int kind = VDIT_BLOCK_HEAD_BE;

	for (;;) {
		read_sector(fd, secno, sector);
		hdr = (struct vdit_block_header *)sector;
		if (VDM_ID_KIND(&hdr->id) != kind) {
			printf("unexpected VDIT block kind "
			    "on sector %08x: %02x\n",
			    secno, VDM_ID_KIND(&hdr->id));
			return 0;
		}

		if (verbose)
			printf("sector %08x: vdit frag type %02x, length %04x, "
			    "next frag at %08x\n",
			    secno, VDM_ID_KIND(&hdr->id),
			    betoh16(hdr->chunksz), secno);

		cursize += betoh16(hdr->chunksz);
		if (betoh32(hdr->nextblk) == VDM_NO_BLK_NUMBER)
			break;

		secno = betoh32(hdr->nextblk);
		kind = VDIT_PORTION_HEADER_BLOCK;
	}

	return cursize;
}

vdit_id_t	subdriver_vdmphys_id;
vdit_id_t	subdriver_vdmpart_id;
vdit_id_t	subdriver_vdmaggr_id;
vdit_id_t	subdriver_vdmremap_id;

void
register_subdriver(uint8_t *buf)
{
	struct vdit_subdriver_entry entry;
	vdit_id_t id, *regid;

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	memcpy(&id, &entry.subdriver_id, sizeof id);
	id = betoh32(id);

	if (strcmp(entry.name, VDM_SUBDRIVER_VDMPHYS) == 0)
		regid = &subdriver_vdmphys_id;
	else if (strcmp(entry.name, VDM_SUBDRIVER_VDMPART) == 0)
		regid = &subdriver_vdmpart_id;
	else if (strcmp(entry.name, VDM_SUBDRIVER_VDMAGGR) == 0)
		regid = &subdriver_vdmaggr_id;
	else if (strcmp(entry.name, VDM_SUBDRIVER_VDMREMAP) == 0)
		regid = &subdriver_vdmremap_id;
	else
		regid = NULL;

	if (regid != NULL) {
		if (*regid != 0)
			printf("WARNING: subdriver \"%s\" overridden\n",
			    entry.name);
		*regid = id;
	}
}

void
print_vdit_instance_id(struct vdit_instance_id *buf, const char *descr)
{
	struct vdit_instance_id instance;

	memcpy(&instance, buf, sizeof instance);
	instance.generation_timestamp = betoh32(instance.generation_timestamp);
	instance.system_id = betoh32(instance.system_id);
	if (instance.generation_timestamp != 0 || instance.system_id != 0 ||
	    verbose)
		printf("%s id %08x:%08x",
		    descr, instance.generation_timestamp, instance.system_id);
}

uint32_t
print_vdit_boot_info(uint8_t *buf, uint32_t size)
{
	struct vdit_boot_info_entry entry;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	printf("\tboot info: version %02x", entry.version);
	print_vdit_instance_id(&entry.default_swap, " default swap");
	print_vdit_instance_id(&entry.default_root, " default root");
	printf("\n");

	return size - sizeof entry;
}

uint32_t
print_vdit_subdriver_info(uint8_t *buf, uint32_t size)
{
	struct vdit_subdriver_entry entry;
	vdit_id_t id;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	printf("\tsubdriver: version %02x", entry.version);
	memcpy(&id, &entry.subdriver_id, sizeof id);
	id = betoh32(id);
	printf(" id %08x name \"%s\"\n", id, entry.name);

	return size - sizeof entry;
}

uint32_t
print_vdmphys_instance(uint8_t *buf, uint32_t size)
{
	struct vdit_vdmphys_instance entry;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	entry.mode = betoh16(entry.mode);
	printf("\tvdmphys: version %02x mode %02x\n",
	    entry.version, entry.mode);

	return size - sizeof entry;
}

uint32_t
print_vdmpart_instance(uint8_t *buf, uint32_t size)
{
	struct vdit_vdmpart_instance entry;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	entry.start_blkno = betoh32(entry.start_blkno);
	entry.size = betoh32(entry.size);
	printf("\tvdmpart: version %02x", entry.version);
	print_vdit_instance_id(&entry.child_instance, " child");
	printf("\n");
	printf("\t\tstarting block %08x size %08x",
	    entry.start_blkno, entry.size);
	print_vdit_instance_id(&entry.remap_instance, " remap");
	printf("\n");

	return size - sizeof entry;
}

uint32_t
print_vdmaggr_instance(uint8_t *buf, uint32_t size)
{
	struct vdit_vdmaggr_instance entry;
	struct vdit_instance_id *aggr;
	uint32_t aggrsize;
	uint stripe;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	entry.aggr_count = betoh16(entry.aggr_count);
	entry.stripe_size = betoh32(entry.stripe_size);
	printf("\tvdmaggr: version %02x count %02x stripe size %08x\n",
	    entry.version, entry.aggr_count, entry.stripe_size);

	aggrsize = entry.aggr_count * sizeof(struct vdit_instance_id) +
	    sizeof entry;
	if (size < aggrsize) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02x)\n",
		    size, aggrsize);
		return 0;
	}

	aggr = (struct vdit_instance_id *)(buf + sizeof entry);
	for (stripe = 0; stripe < entry.aggr_count; stripe++) {
		printf("\t\tstripe %u", stripe);
		print_vdit_instance_id(aggr++, "");
		printf("\n");
	}

	return size - aggrsize;
}

uint32_t
print_vdmremap_instance(uint8_t *buf, uint32_t size)
{
	struct vdit_vdmremap_instance entry;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	printf("\tvdmremap: version %02x", entry.version);
	print_vdit_instance_id(&entry.primary_remap_table,
	    " primary remap table");
	printf("\n");
	print_vdit_instance_id(&entry.secondary_remap_table,
	    "\t\tsecondary remap table");
	printf("\n");
	print_vdit_instance_id(&entry.remap_area, "\t\tremap area");
	printf("\n");

	return size - sizeof entry;
}

uint32_t
print_vdit_instance_info(uint8_t *buf, uint32_t size)
{
	struct vdit_instance_entry entry;
	vdit_id_t id;

	if (size < sizeof entry) {
		printf("\tTRUNCATED ENTRY (%02x bytes, expected %02zx)\n",
		    size, sizeof entry);
		return 0;
	}

	memcpy(&entry, buf, sizeof entry);
	entry.version = betoh16(entry.version);
	printf("\tinstance: version %02x name \"%s\"\n",
	    entry.version, entry.name);
	memcpy(&id, &entry.subdriver_id, sizeof id);
	id = betoh32(id);
	printf("\t\tsubdriver id %08x", id);
	print_vdit_instance_id(&entry.instance_id, "");
	printf(" export %d\n", entry.exported);

	if (id == subdriver_vdmphys_id)
		return print_vdmphys_instance(buf, size);
	if (id == subdriver_vdmpart_id)
		return print_vdmpart_instance(buf, size);
	if (id == subdriver_vdmaggr_id)
		return print_vdmaggr_instance(buf, size);
	if (id == subdriver_vdmremap_id)
		return print_vdmremap_instance(buf, size);

	return size - sizeof entry;
}

uint8_t *
print_vdit_entry(uint8_t *buf)
{
	struct vdit_entry_header hdr;
	uint32_t remaining, cnt;
	uint8_t *rembuf;

	memcpy(&hdr, buf, sizeof hdr);
	hdr.type = betoh16(hdr.type);
	hdr.size = betoh16(hdr.size);

	printf("vdit entry: type %02x size %02x\n", hdr.type, hdr.size);
	if (hdr.type == VDIT_ENTRY_SENTINEL)
		return NULL;

	remaining = hdr.size - sizeof hdr;

	switch (hdr.type) {
	case VDIT_ENTRY_UNUSED:
		remaining = 0;	/* don't print anything */
		break;
	case VDIT_ENTRY_BOOT_INFO:
		remaining = print_vdit_boot_info(buf + sizeof hdr, remaining);
		break;
	case VDIT_ENTRY_SUBDRIVER_INFO:
		register_subdriver(buf + sizeof hdr);
		remaining = print_vdit_subdriver_info(buf + sizeof hdr,
		    remaining);
		break;
	case VDIT_ENTRY_INSTANCE:
		remaining = print_vdit_instance_info(buf + sizeof hdr,
		    remaining);
		break;
	}

	if (remaining == 4) {
		/* timestamp */
		remaining -= 4;
	}

	if (remaining != 0 && verbose) {
		printf("\t%02x bytes unparsed", remaining);
		rembuf = buf + hdr.size - remaining;
		cnt = 0;
		while (remaining-- != 0) {
			if (cnt % 16 == 0)
				printf("\n    ");
			printf("%02x ", *rembuf++);
			cnt++;
		}
		printf("\n");
	}

	return buf + hdr.size;
}

uint8_t *
append_vdit_sector(uint32_t secno, uint8_t *buf, uint8_t *sector, int kind)
{
	struct vdit_block_header *hdr;

	hdr = (struct vdit_block_header *)sector;
	if (VDM_ID_KIND(&hdr->id) != kind) {
		printf("unexpected block kind on sector %08x: %02x\n",
		    secno, VDM_ID_KIND(&hdr->id));
		return NULL;
	}

#ifdef DEBUG
	printf("sector %08x: vdit block %08x\n",
	    secno, VDM_ID_BLKNO(&hdr->id));
#endif

	memcpy(buf, sector + sizeof *hdr, VDM_BLOCK_SIZE - (sizeof *hdr));
	return buf + VDM_BLOCK_SIZE - (sizeof *hdr);
}

uint8_t *
append_vdit_portion(int fd, uint32_t secno, uint8_t *buf, uint8_t *sector,
    int kind)
{
	struct vdit_block_header *hdr;
	u_int chunksz;

	hdr = (struct vdit_block_header *)sector;
	if (VDM_ID_KIND(&hdr->id) != kind) {
		printf("unexpected block kind on sector %08x: %02x\n",
		    secno, VDM_ID_KIND(&hdr->id));
		return NULL;
	}

	/* store first sector of the portion */
	chunksz = betoh16(hdr->chunksz);
	buf = append_vdit_sector(secno, buf, sector, kind);
	chunksz--;
	secno++;

	/* do the others */
	while (chunksz-- != 0) {
		read_sector(fd, secno, sector);
		buf = append_vdit_sector(secno, buf, sector, VDIT_BLOCK);
		if (buf == NULL)
			return NULL;
		secno++;
	}

	return buf;
}

uint8_t *
read_vdit(int fd, uint32_t secno, size_t *vditsize)
{
	uint8_t sector[VDM_BLOCK_SIZE];
	struct vdit_block_header hdr;
	uint32_t vdit_size;
	uint8_t *buf, *curbuf;
	int first = 1;

	vdit_size = get_vdit_size(fd, secno);
#ifdef DEBUG
	printf("vdit size: %02x sectors\n", vdit_size);
#endif

	buf = (uint8_t *)malloc(VDM_BLOCK_SIZE * vdit_size);
	if (buf == NULL)
		err(1, "malloc");
	memset(buf, 0, VDM_BLOCK_SIZE * vdit_size);

	curbuf = buf;
	for (;;) {
		/* read first sector of portion */
		read_sector(fd, secno, sector);
		memcpy(&hdr, sector, sizeof hdr);

		curbuf = append_vdit_portion(fd, secno, curbuf, sector,
		    first ? VDIT_BLOCK_HEAD_BE : VDIT_PORTION_HEADER_BLOCK);
		if (curbuf == NULL) {
			free(buf);
			return NULL;
		}
		first = 0;

		if (hdr.nextblk == VDM_NO_BLK_NUMBER)
			break;

		secno = betoh32(hdr.nextblk);
	}

	if (verbose)
		printf("vdit final size: 0x%zx bytes\n", curbuf - buf);

	*vditsize = curbuf - buf;
	return buf;
}

void
report(int fd)
{
	uint8_t *vdit, *vdit2, *tmpvdit;
	size_t vditsize, vditsize2;
	struct vdm_label *dl;
	struct vdm_boot_info *bi;
	struct disklabel *lp;
	uint8_t sector[VDM_BLOCK_SIZE];
	struct vdit_block_header *hdr;

	read_sector(fd, VDM_LABEL_SECTOR, sector);
	analyze_label_sector(sector, &dl, &bi);

	if (dl == NULL)
		return;

	printf("label version %04x\n", betoh16(dl->version));

	if (bi != NULL)
		printf("disk boot info: start %08x size %08x version %08x\n",
		    betoh32(bi->boot_start),
		    betoh32(bi->boot_size), betoh32(bi->version));

	read_sector(fd, VDIT_SECTOR, sector);
	hdr = (struct vdit_block_header *)sector;
	if (VDM_ID_KIND(&hdr->id) != VDIT_BLOCK_HEAD_BE) {
		lp = (struct disklabel *)(sector + LABELOFFSET);
		if (lp->d_magic == DISKMAGIC && lp->d_magic2 == DISKMAGIC) {
			if (verbose)
				printf("no VDIT but a native OpenBSD label\n");
			return;
		}
		errx(3, "unexpected block kind on sector %08x: %02x",
		    1, VDM_ID_KIND(&hdr->id));
	}

	vdit = read_vdit(fd, 1, &vditsize);
	if (vdit != NULL) {
		tmpvdit = vdit;
		while (tmpvdit != NULL)
			tmpvdit = print_vdit_entry(tmpvdit);

		vdit2 = read_vdit(fd, betoh32(hdr->secondary_vdit), &vditsize2);
		if (vdit2 == NULL)
			printf("can't read backup VDIT\n");
		else {
			if (vditsize2 < vditsize) {
				printf("WARNING: backup VDIT is smaller "
				    "than main VDIT!\n");
				vditsize = vditsize2;
			}
			if (memcmp(vdit, vdit2, vditsize) != 0)
				printf("VDIT and backup VDIT differ!\n");
			free(vdit2);
		}

		free(vdit);
	}
}

/*
 * Build a minimal VDM label and boot area.
 * Allows you to shoot yourself in the foot, badly.
 */
void
initialize(int fd)
{
	uint8_t sector[VDM_BLOCK_SIZE];
	struct vdm_label dl;
	struct vdm_boot_info bi;

	memset(sector, 0, sizeof sector);
	memset(&dl, 0, sizeof dl);
	memset(&bi, 0, sizeof bi);

	dl.signature = htobe32(VDM_LABEL_SIGNATURE);
	bi.signature = htobe32(VDM_LABEL_SIGNATURE);
	bi.boot_start = htobe32(8);
	bi.boot_size = htobe32(VDM_BOOT_DEFAULT_SIZE);
	bi.version = htobe32(VDM_BOOT_INFO_VERSION);

	memcpy(sector + VDM_LABEL_OFFSET_ALT, &dl, sizeof dl);
	memcpy(sector + VDM_BLOCK_SIZE - sizeof bi, &bi, sizeof bi);

	write_sector(fd, VDM_LABEL_SECTOR, sector);
}
