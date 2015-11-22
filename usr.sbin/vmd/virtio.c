/*	$OpenBSD: virtio.c,v 1.2 2015/11/22 21:51:32 reyk Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/virtioreg.h>
#include <dev/pci/vioblkreg.h>
#include <machine/vmmvar.h>
#include <machine/param.h>
#include "pci.h"
#include "vmd.h"
#include "virtio.h"
#include "loadfile.h"

extern char *__progname;

struct viornd_dev viornd;
struct vioblk_dev *vioblk;
struct vionet_dev *vionet;

int nr_vionet;

#define MAXPHYS	(64 * 1024)	/* max raw I/O transfer size */

#define VIRTIO_NET_F_MAC	(1<<5)


const char *
vioblk_cmd_name(uint32_t type)
{
	switch(type) {
	case VIRTIO_BLK_T_IN: return "read";
	case VIRTIO_BLK_T_OUT: return "write";
	case VIRTIO_BLK_T_SCSI_CMD: return "scsi read";
	case VIRTIO_BLK_T_SCSI_CMD_OUT: return "scsi write";
	case VIRTIO_BLK_T_FLUSH: return "flush";
	case VIRTIO_BLK_T_FLUSH_OUT: return "flush out";
	case VIRTIO_BLK_T_GET_ID: return "get id";
	default: return "unknown";
	}
}

static void
dump_descriptor_chain(struct vring_desc *desc, int16_t dxx)
{
	fprintf(stderr, "descriptor chain @ %d\n\r", dxx);
	do {
		fprintf(stderr, "desc @%d addr/len/flags/next = 0x%llx / 0x%x "
		    "/ 0x%x / 0x%x\n\r",
		    dxx,
		    desc[dxx].addr,
		    desc[dxx].len,
		    desc[dxx].flags,
		    desc[dxx].next);
		dxx = desc[dxx].next;
	} while (desc[dxx].flags & VRING_DESC_F_NEXT);

	fprintf(stderr, "desc @%d addr/len/flags/next = 0x%llx / 0x%x / 0x%x "
	    "/ 0x%x\n\r",
	    dxx,
	    desc[dxx].addr,
	    desc[dxx].len,
	    desc[dxx].flags,
	    desc[dxx].next);
}

static const char *
virtio_reg_name(uint8_t reg)
{
	switch(reg) {
	case VIRTIO_CONFIG_DEVICE_FEATURES: return "device feature";
	case VIRTIO_CONFIG_GUEST_FEATURES: return "guest feature";
	case VIRTIO_CONFIG_QUEUE_ADDRESS: return "queue address";
	case VIRTIO_CONFIG_QUEUE_SIZE: return "queue size";
	case VIRTIO_CONFIG_QUEUE_SELECT: return "queue select";
	case VIRTIO_CONFIG_QUEUE_NOTIFY: return "queue notify";
	case VIRTIO_CONFIG_DEVICE_STATUS: return "device status";
	case VIRTIO_CONFIG_ISR_STATUS: return "isr status";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI: return "device config 0";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4: return "device config 1";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8: return "device config 2";
	default: return "unknown";
	}
}

uint32_t
vring_size(uint32_t vq_size)
{
	uint32_t allocsize1, allocsize2;

	/* allocsize1: descriptor table + avail ring + pad */
	allocsize1 = VIRTQUEUE_ALIGN(sizeof(struct vring_desc) * vq_size
	    + sizeof(uint16_t) * (2 + vq_size));
	/* allocsize2: used ring + pad */
	allocsize2 = VIRTQUEUE_ALIGN(sizeof(uint16_t) * 2
	    + sizeof(struct vring_used_elem) * vq_size);

	return allocsize1 + allocsize2;
}

/* Update queue select */
void
viornd_update_qs(void)
{
	/* Invalid queue? */
	if (viornd.cfg.queue_select > 0) 
		return;

	/* Update queue address/size based on queue select */
	viornd.cfg.queue_address = viornd.vq[viornd.cfg.queue_select].qa;
	viornd.cfg.queue_size = viornd.vq[viornd.cfg.queue_select].qs;
}

/* Update queue address */
void
viornd_update_qa(void)
{
	/* Invalid queue? */
	if (viornd.cfg.queue_select > 0)
		return;

	viornd.vq[viornd.cfg.queue_select].qa = viornd.cfg.queue_address;
}

static int
push_virtio_ring(char *buf, uint32_t gpa, uint32_t vr_sz)
{
	uint32_t i;

	for (i = 0; i < vr_sz; i += VIRTIO_PAGE_SIZE) {
		if (write_page((uint32_t)gpa + i, buf + i, PAGE_SIZE, 0)) {
			return (1);
		}
	}

	return (0);
}

int
viornd_notifyq(void)
{
	uint64_t q_gpa;
	uint32_t vr_sz, i;
	int ret;
	char *buf, *rnd_data;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;

	ret = 0;

	/* Invalid queue? */
	if (viornd.cfg.queue_notify > 0)
		return (0);

	vr_sz = vring_size(VIORND_QUEUE_SIZE);
	q_gpa = viornd.vq[viornd.cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	buf = malloc(vr_sz);
	if (buf == NULL) {
		fprintf(stderr, "malloc error getting viornd ring\n\r");
		return (0);
	}

	bzero(buf, vr_sz);

	for (i = 0; i < vr_sz; i += VIRTIO_PAGE_SIZE) {
		if (read_page((uint32_t)q_gpa + i, buf + i, PAGE_SIZE, 0)) {
			free(buf);
			return (0);
		}
	}

	desc = (struct vring_desc *)(buf);
	avail = (struct vring_avail *)(buf +
	    viornd.vq[viornd.cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(buf +
	    viornd.vq[viornd.cfg.queue_notify].vq_usedoffset);

	/* XXX sanity check len here */
	rnd_data = malloc(desc[avail->ring[avail->idx]].len);

	if (rnd_data != NULL) {
		arc4random_buf(rnd_data, desc[avail->ring[avail->idx]].len);
		if (write_page((uint32_t)(desc[avail->ring[avail->idx]].addr),
		    rnd_data, desc[avail->ring[avail->idx]].len, 0)) {
			fprintf(stderr, "viornd: can't write random data @ "
			    "0x%llx\n\r",
			    desc[avail->ring[avail->idx]].addr);
		} else {
			/* ret == 1 -> interrupt needed */
			/* XXX check VIRTIO_F_NO_INTR */
			ret = 1;
			viornd.cfg.isr_status = 1;
			used->ring[used->idx].id = avail->ring[avail->idx];
			used->ring[used->idx].len =
			    desc[avail->ring[avail->idx]].len;
			used->idx++;

			if (push_virtio_ring(buf, q_gpa, vr_sz)) {
				fprintf(stderr, "viornd: error writing vio "
				    "ring\n\r");
			}
			
		}
		free(rnd_data);
	}

	free(buf);

	return (ret);
}

int
virtio_rnd_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *unused)
{
	*intr = 0xFF;

	if (dir == 0) {
		switch(reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			fprintf(stderr, "%s: illegal write %x to %s\n\r",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			viornd.cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			viornd.cfg.queue_address = *data;
			viornd_update_qa();
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			viornd.cfg.queue_select = *data;
			viornd_update_qs();
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			viornd.cfg.queue_notify = *data;
			if (viornd_notifyq())
				*intr = 1;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			viornd.cfg.device_status = *data;
			break;
		}
	} else {
		switch(reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = viornd.cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = viornd.cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = viornd.cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = viornd.cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = viornd.cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = viornd.cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = viornd.cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = viornd.cfg.isr_status;
			break;
		}
	}
	return (0);
}

void
vioblk_update_qa(struct vioblk_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 0)
		return;

	dev->vq[dev->cfg.queue_select].qa = dev->cfg.queue_address;
}

void
vioblk_update_qs(struct vioblk_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 0)
		return;

	/* Update queue address/size based on queue select */
	dev->cfg.queue_address = dev->vq[dev->cfg.queue_select].qa;
	dev->cfg.queue_size = dev->vq[dev->cfg.queue_select].qs;
}

static char *
vioblk_do_read(struct vioblk_dev *dev, off_t sector, ssize_t sz)
{
	char *buf;

	buf = malloc(sz);

	if (buf == NULL) {
		fprintf(stderr, "malloc errror vioblk read\n\r");
		return (NULL);
	}

	if (lseek(dev->fd, sector * VIRTIO_BLK_SECTOR_SIZE,
	    SEEK_SET) == -1) {
		fprintf(stderr, "seek error in vioblk read: %d\n\r",
		    errno);
		free(buf);
		return (NULL);
	}
 
	if (read(dev->fd, buf, sz) != sz) {
		fprintf(stderr, "vioblk read error: %d\n\r",
		    errno);
		free(buf);
		return (NULL);
	}

	return buf;
}

static int
vioblk_do_write(struct vioblk_dev *dev, off_t sector, char *buf, ssize_t sz)
{
	if (lseek(dev->fd, sector * VIRTIO_BLK_SECTOR_SIZE,
	    SEEK_SET) == -1) {
		fprintf(stderr, "seek error in vioblk write: %d\n\r",
		    errno);
		return (1);
	}

	if (write(dev->fd, buf, sz) != sz) {
		fprintf(stderr, "vioblk write error: %d\n\r",
		    errno);
		free(buf);
		return (1);
	}

	return (0);
}

/*
 * XXX this function needs a cleanup block, lots of free(blah); return (0)
 *     in various cases, ds should be set to VIRTIO_BLK_S_IOERR, if we can
 * XXX cant trust ring data from VM, be extra cautious.
 */
int
vioblk_notifyq(struct vioblk_dev *dev)
{
	uint64_t q_gpa;
	uint32_t vr_sz, i, j, len; //, osz;
	uint16_t idx, cmd_desc_idx, secdata_desc_idx, ds_desc_idx; //, dxx;
	uint8_t ds;
	int ret;
	off_t secbias;
	char *vr, *secdata;
	struct vring_desc *desc, *cmd_desc, *secdata_desc, *ds_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_blk_req_hdr cmd;

	ret = 0;

	/* Invalid queue? */
	if (dev->cfg.queue_notify > 0)
		return (0);

	vr_sz = vring_size(VIOBLK_QUEUE_SIZE);
	q_gpa = dev->vq[dev->cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = malloc(vr_sz);
	if (vr == NULL) {
		fprintf(stderr, "malloc error getting vioblk ring\n\r");
		return (0);
	}

	bzero(vr, vr_sz);

	for (i = 0; i < vr_sz; i += VIRTIO_PAGE_SIZE) {
		if (read_page((uint32_t)q_gpa + i, vr + i, PAGE_SIZE, 0)) {
			fprintf(stderr, "error reading gpa 0x%x\n\r",
			    (uint32_t)q_gpa + i);
			free(vr);
			return (0);
		}
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_usedoffset);


	idx = dev->vq[dev->cfg.queue_notify].last_avail & VIOBLK_QUEUE_MASK;

	if ((avail->idx & VIOBLK_QUEUE_MASK) == idx) {
		fprintf(stderr, "vioblk queue notify - nothing to do?\n\r");
		free(vr);
		return (0);
	}

	cmd_desc_idx = avail->ring[idx] & VIOBLK_QUEUE_MASK;
	cmd_desc = &desc[cmd_desc_idx];

	if ((cmd_desc->flags & VRING_DESC_F_NEXT) == 0) {
		fprintf(stderr, "unchained vioblk cmd descriptor received "
		    "(idx %d)\n\r", cmd_desc_idx);
		free(vr);
		return (0);
	}

	/* Read command from descriptor ring */
	if (read_page((uint32_t)cmd_desc->addr, &cmd, cmd_desc->len, 0)) {
		fprintf(stderr, "vioblk: command read_page error @ 0x%llx\n\r",
		    cmd_desc->addr);
		free(vr);
		return (0);
	}

	switch (cmd.type) {
	case VIRTIO_BLK_T_IN:
		/* first descriptor */
		secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
		secdata_desc = &desc[secdata_desc_idx];

		if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
			fprintf(stderr, "unchained vioblk data descriptor "
			    "received (idx %d)\n\r", cmd_desc_idx);
			free(vr);
			return (0);
		}

		secbias = 0;
		do {
			/* read the data (use current data descriptor) */
			/*
			 * XXX waste to malloc secdata in vioblk_do_read
			 * and free it here over and over
			 */
			secdata = vioblk_do_read(dev, cmd.sector + secbias,
			    (ssize_t)secdata_desc->len);
			if (secdata == NULL) {
				fprintf(stderr, "vioblk: block read error, "
				    "sector %lld\n\r", cmd.sector);
				free(vr);
				return (0);
			}
			
			j = 0;
			while (j < secdata_desc->len) {
				if (secdata_desc->len - j >= PAGE_SIZE)
					len = PAGE_SIZE;
				else
					len = secdata_desc->len - j;

				if (write_page((uint32_t)secdata_desc->addr + j,
				    secdata + j, len, 0)) {
					fprintf(stderr, "can't write sector "
					    "data to gpa @ 0x%llx\n\r",
					    secdata_desc->addr);
					dump_descriptor_chain(desc, cmd_desc_idx);
					free(vr);
					free(secdata);
					return (0);
				}

				j += PAGE_SIZE;
			}	

			free(secdata);

			secbias += (secdata_desc->len / VIRTIO_BLK_SECTOR_SIZE);
			secdata_desc_idx = secdata_desc->next &
			    VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];
		} while (secdata_desc->flags & VRING_DESC_F_NEXT);

		ds_desc_idx = secdata_desc_idx;
		ds_desc = secdata_desc;

		ds = VIRTIO_BLK_S_OK;
		if (write_page((uint32_t)ds_desc->addr,
		    &ds, ds_desc->len, 0)) {
			fprintf(stderr, "can't write device status data @ "
			    "0x%llx\n\r", ds_desc->addr);
			dump_descriptor_chain(desc, cmd_desc_idx);
			free(vr);
			return (0);
		}


		ret = 1;
		dev->cfg.isr_status = 1;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].id = cmd_desc_idx;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].len = cmd_desc->len;
		used->idx++;

		dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
		    VIOBLK_QUEUE_MASK;

		if (push_virtio_ring(vr, q_gpa, vr_sz)) {
			fprintf(stderr, "vioblk: error writing vio ring\n\r");
		}
		break;
	case VIRTIO_BLK_T_OUT:
		secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
		secdata_desc = &desc[secdata_desc_idx];

		if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
			fprintf(stderr, "wr vioblk: unchained vioblk data "
			   "descriptor received (idx %d)\n\r", cmd_desc_idx);
			free(vr);
			return (0);
		}

		secdata = malloc(MAXPHYS);
		if (secdata == NULL) {
			fprintf(stderr, "wr vioblk: malloc error, len %d\n\r",
			    secdata_desc->len);
			free(vr);
			return (0);
		}

		secbias = 0;
		do {
			j = 0;
			while (j < secdata_desc->len) {
				if (secdata_desc->len - j >= PAGE_SIZE)
					len = PAGE_SIZE;
				else
					len = secdata_desc->len - j;
				if (read_page((uint32_t)secdata_desc->addr + j,
				    secdata + j, len, 0)) {
					fprintf(stderr, "wr vioblk: can't read "
					    "sector data @ 0x%llx\n\r",
					    secdata_desc->addr);
					dump_descriptor_chain(desc,
					     cmd_desc_idx);
					free(vr);
					free(secdata);
					return (0);
				}

				j += PAGE_SIZE;
			}	

			if (vioblk_do_write(dev, cmd.sector + secbias,
			    secdata, (ssize_t)secdata_desc->len)) {
				fprintf(stderr, "wr vioblk: disk write "
				    "error\n\r");
				free(vr);
				free(secdata);
				return (0);
			}

			secbias += secdata_desc->len / VIRTIO_BLK_SECTOR_SIZE;

			secdata_desc_idx = secdata_desc->next &
			    VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];
		} while (secdata_desc->flags & VRING_DESC_F_NEXT);

		free(secdata);

		ds_desc_idx = secdata_desc_idx;
		ds_desc = secdata_desc;

		ds = VIRTIO_BLK_S_OK;
		if (write_page((uint32_t)ds_desc->addr,
		    &ds, ds_desc->len, 0)) {
			fprintf(stderr, "wr vioblk: can't write device status "
			    "data @ 0x%llx\n\r", ds_desc->addr);
			dump_descriptor_chain(desc, cmd_desc_idx);
			free(vr);
			return (0);
		}

		ret = 1;
		dev->cfg.isr_status = 1;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].id = cmd_desc_idx;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].len = cmd_desc->len;
		used->idx++;

		dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
		    VIOBLK_QUEUE_MASK;
		if (push_virtio_ring(vr, q_gpa, vr_sz))
			fprintf(stderr, "wr vioblk: error writing vio ring\n\r");
		break;
	case VIRTIO_BLK_T_FLUSH:
	case VIRTIO_BLK_T_FLUSH_OUT:
		ds_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
		ds_desc = &desc[ds_desc_idx];

		ds = VIRTIO_BLK_S_OK;
		if (write_page((uint32_t)ds_desc->addr,
		    &ds, ds_desc->len, 0)) {
			fprintf(stderr, "fl vioblk: can't write device status "
			    "data @ 0x%llx\n\r", ds_desc->addr);
			dump_descriptor_chain(desc, cmd_desc_idx);
			free(vr);
			return (0);
		}

		ret = 1;
		dev->cfg.isr_status = 1;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].id = cmd_desc_idx;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].len = cmd_desc->len;
		used->idx++;

		dev->vq[dev->cfg.queue_notify].last_avail = avail->idx &
		    VIOBLK_QUEUE_MASK;
		if (push_virtio_ring(vr, q_gpa, vr_sz)) {
			fprintf(stderr, "fl vioblk: error writing vio ring\n\r");
		}
		break;
	}

	free(vr);

	return (ret);
}

int
virtio_blk_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *cookie)
{
	struct vioblk_dev *dev = (struct vioblk_dev *)cookie;

	*intr = 0xFF;

	if (dir == 0) {
		switch(reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			fprintf(stderr, "%s: illegal write %x to %s\n\r",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			dev->cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			dev->cfg.queue_address = *data;
			vioblk_update_qa(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			dev->cfg.queue_select = *data;
			vioblk_update_qs(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			dev->cfg.queue_notify = *data;
			if (vioblk_notifyq(dev))
				*intr = 1;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			dev->cfg.device_status = *data;
			break;
		default:
			break;
		}
	} else {
		switch(reg) {
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
			*data = (uint32_t)(dev->sz >> 32);
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
			*data = (uint32_t)(dev->sz);
			break;
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = dev->cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = dev->cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = dev->cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = dev->cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = dev->cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = dev->cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = dev->cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = dev->cfg.isr_status;
			break;
		}
	}
	return (0);
}

int
virtio_net_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *cookie)
{
	struct vionet_dev *dev = (struct vionet_dev *)cookie;

	*intr = 0xFF;

	if (dir == 0) {
		switch(reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			fprintf(stderr, "%s: illegal write %x to %s\n\r",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			dev->cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			dev->cfg.queue_address = *data;
			vionet_update_qa(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			dev->cfg.queue_select = *data;
			vionet_update_qs(dev);
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			dev->cfg.queue_notify = *data;
			if (vionet_notifyq(dev))
				*intr = 1;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			dev->cfg.device_status = *data;
			break;
		default:
			break;
		}
	} else {
		switch(reg) {
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
			*data = dev->mac[reg -
			    VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI];
			break;
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = dev->cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = dev->cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_ADDRESS:
			*data = dev->cfg.queue_address;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = dev->cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = dev->cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = dev->cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = dev->cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = dev->cfg.isr_status;
			break;
		}
	}
	return (0);
}

void
vionet_update_qa(struct vionet_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 1)
		return;

	dev->vq[dev->cfg.queue_select].qa = dev->cfg.queue_address;
}

void
vionet_update_qs(struct vionet_dev *dev)
{
	/* Invalid queue? */
	if (dev->cfg.queue_select > 1) 
		return;

	/* Update queue address/size based on queue select */
	dev->cfg.queue_address = dev->vq[dev->cfg.queue_select].qa;
	dev->cfg.queue_size = dev->vq[dev->cfg.queue_select].qs;
}

int
vionet_enq_rx(struct vionet_dev *dev, char *pkt, ssize_t sz, int *spc)
{
	uint64_t q_gpa;
	uint32_t vr_sz, i;
	uint16_t idx, pkt_desc_idx, hdr_desc_idx;
	int ret;
	char *vr;
	struct vring_desc *desc, *pkt_desc, *hdr_desc;
	struct vring_avail *avail;
	struct vring_used *used;

	ret = 0;

	vr_sz = vring_size(VIONET_QUEUE_SIZE);
	q_gpa = dev->vq[0].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = malloc(vr_sz);
	if (vr == NULL) {
		fprintf(stderr, "rx enq :malloc error getting vionet ring\n\r");
		return (0);
	}

	bzero(vr, vr_sz);

	for (i = 0; i < vr_sz; i += VIRTIO_PAGE_SIZE) {
		if (read_page((uint32_t)q_gpa + i, vr + i, PAGE_SIZE, 0)) {
			fprintf(stderr, "rx enq: error reading gpa 0x%x\n\r",
			    (uint32_t)q_gpa + i);
			free(vr);
			return (0);
		}
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr +
	    dev->vq[0].vq_availoffset);
	used = (struct vring_used *)(vr +
	    dev->vq[0].vq_usedoffset);

	idx = dev->vq[0].last_avail & VIONET_QUEUE_MASK;

	if ((avail->idx & VIONET_QUEUE_MASK) == idx) {
		fprintf(stderr, "vionet queue notify - no space, dropping "
		    "packet\n\r");
		free(vr);
		return (0);
	}

	hdr_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
	hdr_desc = &desc[hdr_desc_idx];

	pkt_desc_idx = hdr_desc->next & VIONET_QUEUE_MASK;
	pkt_desc = &desc[pkt_desc_idx];

	/* must be not readable */
	if ((pkt_desc->flags & VRING_DESC_F_WRITE) == 0) {
		fprintf(stderr, "unexpected readable rx descriptor %d\n\r",
		    pkt_desc_idx);
		free(vr);
		return (0);
	}

	/* Write packet to descriptor ring */
	if (write_page((uint32_t)pkt_desc->addr, pkt, sz, 0)) {
		fprintf(stderr, "vionet: rx enq packet write_page error @ "
		    "0x%llx\n\r", pkt_desc->addr);
		free(vr);
		return (0);
	}

	ret = 1;
	dev->cfg.isr_status = 1;
	used->ring[used->idx & VIONET_QUEUE_MASK].id = hdr_desc_idx;
	used->ring[used->idx & VIONET_QUEUE_MASK].len = hdr_desc->len + sz;
	used->idx++;
	dev->vq[0].last_avail = (dev->vq[0].last_avail + 1);
	*spc = avail->idx - dev->vq[0].last_avail;
	if (push_virtio_ring(vr, q_gpa, vr_sz)) {
		fprintf(stderr, "vionet: error writing vio ring\n\r");
	}

	free(vr);

	return (ret);
}

int
vionet_process_rx(void)
{
	int i, num_enq, spc, hasdata;
	ssize_t sz;
	char *buf;
	struct pollfd pfd;

	num_enq = 0;
	buf = malloc(PAGE_SIZE);
	for (i = 0 ; i < nr_vionet; i++) {
		if (!vionet[i].rx_added)
			continue;

		spc = 1;
		hasdata = 1;
		bzero(buf, PAGE_SIZE);
		bzero(&pfd, sizeof(struct pollfd));
		pfd.fd = vionet[i].fd;
		pfd.events = POLLIN;
		while (spc && hasdata) {
			hasdata = poll(&pfd, 1, 0);
			if (hasdata == 1) {
				sz = read(vionet[i].fd, buf, PAGE_SIZE);
				if (sz != 0) {
					num_enq += vionet_enq_rx(&vionet[i],
					    buf, sz, &spc);
				} else if (sz == 0) {
					fprintf(stderr, "process_rx: no data"
					    "\n\r");
					hasdata = 0;
				}
			}
		}
	}

	free(buf);

	/*
	 * XXX returns the number of packets enqueued across all vionet, which
	 * may not be right for VMs with more than one vionet.
	 */
	return (num_enq);
}

void
vionet_notify_rx(struct vionet_dev *dev)
{
	uint64_t q_gpa;
	uint32_t vr_sz, i;
	uint16_t idx, pkt_desc_idx;
	char *vr;
	struct vring_desc *desc, *pkt_desc;
	struct vring_avail *avail;
	struct vring_used *used;

	vr_sz = vring_size(VIONET_QUEUE_SIZE);
	q_gpa = dev->vq[dev->cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = malloc(vr_sz);
	if (vr == NULL) {
		fprintf(stderr, "malloc error getting vionet ring\n\r");
		return;
	}

	bzero(vr, vr_sz);

	for (i = 0; i < vr_sz; i += VIRTIO_PAGE_SIZE) {
		if (read_page((uint32_t)q_gpa + i, vr + i, PAGE_SIZE, 0)) {
			fprintf(stderr, "error reading gpa 0x%x\n\r",
			    (uint32_t)q_gpa + i);
			free(vr);
			return;
		}
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_usedoffset);

	idx = dev->vq[dev->cfg.queue_notify].last_avail & VIONET_QUEUE_MASK;
	pkt_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
	pkt_desc = &desc[pkt_desc_idx];

	dev->rx_added = 1;

	free(vr);
}


/*
 * XXX this function needs a cleanup block, lots of free(blah); return (0)
 * XXX cant trust ring data from VM, be extra cautious.
 * XXX advertise link status to guest
 */
int
vionet_notifyq(struct vionet_dev *dev)
{
	uint64_t q_gpa;
	uint32_t vr_sz, i;
	uint16_t idx, pkt_desc_idx, hdr_desc_idx, dxx;
	size_t pktsz;
	int ret, num_enq, ofs;
	char *vr, *pkt;
	struct vring_desc *desc, *pkt_desc, *hdr_desc;
	struct vring_avail *avail;
	struct vring_used *used;

	ret = 0;

	/* Invalid queue? */
	if (dev->cfg.queue_notify != 1) {
		vionet_notify_rx(dev);
		return (0);
	}

	vr_sz = vring_size(VIONET_QUEUE_SIZE);
	q_gpa = dev->vq[dev->cfg.queue_notify].qa;
	q_gpa = q_gpa * VIRTIO_PAGE_SIZE;

	vr = malloc(vr_sz);
	if (vr == NULL) {
		fprintf(stderr, "malloc error getting vionet ring\n\r");
		return (0);
	}

	bzero(vr, vr_sz);

	for (i = 0; i < vr_sz; i += VIRTIO_PAGE_SIZE) {
		if (read_page((uint32_t)q_gpa + i, vr + i, PAGE_SIZE, 0)) {
			fprintf(stderr, "error reading gpa 0x%x\n\r",
			    (uint32_t)q_gpa + i);
			free(vr);
			return (0);
		}
	}

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_availoffset);
	used = (struct vring_used *)(vr +
	    dev->vq[dev->cfg.queue_notify].vq_usedoffset);

	num_enq = 0;

	idx = dev->vq[dev->cfg.queue_notify].last_avail & VIONET_QUEUE_MASK;

	if ((avail->idx & VIONET_QUEUE_MASK) == idx) {
		fprintf(stderr, "vionet tx queue notify - nothing to do?\n\r");
		free(vr);
		return (0);
	}

	while ((avail->idx & VIONET_QUEUE_MASK) != idx) {

		hdr_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
		hdr_desc = &desc[hdr_desc_idx];
		pktsz = 0;

		dxx = hdr_desc_idx;
		do {
			pktsz += desc[dxx].len;
			dxx = desc[dxx].next;
		} while (desc[dxx].flags & VRING_DESC_F_NEXT);

		pktsz += desc[dxx].len;

		/* Remove virtio header descriptor len */
		pktsz -= hdr_desc->len;

		/*
		 * XXX check sanity pktsz
		 * XXX too long and  > PAGE_SIZE checks
		 *     (PAGE_SIZE can be relaxed to 16384 later)
		 */
		pkt = malloc(pktsz);
		if (pkt == NULL) {
			fprintf(stderr, "malloc error alloc packet buf\n\r");
			free(vr);
			return (0);
		}

		ofs = 0;
		pkt_desc_idx = hdr_desc->next & VIONET_QUEUE_MASK;
		pkt_desc = &desc[pkt_desc_idx];
		
		while (pkt_desc->flags & VRING_DESC_F_NEXT) {
			/* must be not writable */
			if (pkt_desc->flags & VRING_DESC_F_WRITE) {
				fprintf(stderr, "unexpceted writable tx desc "
				    "%d\n\r", pkt_desc_idx);
				free(vr);
				return (0);
			}

			/* Read packet from descriptor ring */
			if (read_page((uint32_t)pkt_desc->addr, pkt + ofs,
			    pkt_desc->len, 0)) {
				fprintf(stderr, "vionet: packet read_page error "
				    "@ 0x%llx\n\r", pkt_desc->addr);
				free(pkt);
				free(vr);
				return (0);
			}

			ofs += pkt_desc->len;
			pkt_desc_idx = pkt_desc->next & VIONET_QUEUE_MASK;
			pkt_desc = &desc[pkt_desc_idx];
		}

		/* Now handle tail descriptor - must be not writable */
		if (pkt_desc->flags & VRING_DESC_F_WRITE) {
			fprintf(stderr, "unexpceted writable tx descriptor %d"
			    "\n\r", pkt_desc_idx);
			free(vr);
			return (0);
		}

		/* Read packet from descriptor ring */
		if (read_page((uint32_t)pkt_desc->addr, pkt + ofs,
		    pkt_desc->len, 0)) {
			fprintf(stderr, "vionet: packet read_page error @ "
			    "0x%llx\n\r", pkt_desc->addr);
			free(pkt);
			free(vr);
			return (0);
		}

		/* XXX signed vs unsigned here, funky cast */
		if (write(dev->fd, pkt, pktsz) != (int)pktsz) {
			fprintf(stderr, "vionet: tx failed writing to tap: "
			    "%d\n\r", errno);
			free(pkt);
			free(vr);
			return (0);
		}

		ret = 1;
		dev->cfg.isr_status = 1;
		used->ring[used->idx & VIONET_QUEUE_MASK].id = hdr_desc_idx;
		used->ring[used->idx & VIONET_QUEUE_MASK].len = hdr_desc->len;
		used->idx++;

		dev->vq[dev->cfg.queue_notify].last_avail =
		    (dev->vq[dev->cfg.queue_notify].last_avail + 1);
		num_enq++;

		idx = dev->vq[dev->cfg.queue_notify].last_avail &
		    VIONET_QUEUE_MASK;
	}

	if (push_virtio_ring(vr, q_gpa, vr_sz)) {
		fprintf(stderr, "vionet: tx error writing vio ring\n\r");
	}

	free(vr);
	free(pkt);

	return (ret);
}

void
virtio_init(struct vm_create_params *vcp, int *child_disks, int *child_taps)
{
	uint8_t id;
	uint8_t i;
	off_t sz;

	/* Virtio entropy device */
	if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
	    PCI_PRODUCT_QUMRANET_VIO_RNG, PCI_CLASS_SYSTEM,
	    PCI_SUBCLASS_SYSTEM_MISC,
	    PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_VIRTIO_ENTROPY, 1, NULL)) {
		fprintf(stderr, "%s: can't add PCI virtio rng device\n",
		    __progname);
		return;
	}

	if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_rnd_io, NULL)) {
		fprintf(stderr, "%s: can't add bar for virtio rng device\n",
		    __progname);
		return;
	}

	bzero(&viornd, sizeof(viornd));
	viornd.vq[0].qs = VIORND_QUEUE_SIZE;
	viornd.vq[0].vq_availoffset = sizeof(struct vring_desc) *
	    VIORND_QUEUE_SIZE;
	viornd.vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
	    sizeof(struct vring_desc) * VIORND_QUEUE_SIZE
	    + sizeof(uint16_t) * (2 + VIORND_QUEUE_SIZE));


	vioblk = malloc(sizeof(struct vioblk_dev) * vcp->vcp_ndisks);
	if (vioblk == NULL) {
		fprintf(stderr, "%s: malloc failure allocating vioblks\n",
		    __progname);
		return;
	}

	bzero(vioblk, sizeof(struct vioblk_dev) * vcp->vcp_ndisks);

	/* One virtio block device for each disk defined in vcp */
	for (i = 0; i < vcp->vcp_ndisks; i++) {
		if ((sz = lseek(child_disks[i], 0, SEEK_END)) == -1)
			continue;

		if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
		    PCI_PRODUCT_QUMRANET_VIO_BLOCK, PCI_CLASS_MASS_STORAGE,
		    PCI_SUBCLASS_MASS_STORAGE_SCSI,
		    PCI_VENDOR_OPENBSD,
		    PCI_PRODUCT_VIRTIO_BLOCK, 1, NULL)) {
			fprintf(stderr, "%s: can't add PCI virtio block "
			    "device\n", __progname);
			return;
		}
		if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_blk_io,
		    &vioblk[i])) {
			fprintf(stderr, "%s: can't add bar for virtio block "
			    "device\n", __progname);
			return;
		}
		vioblk[i].vq[0].qs = VIOBLK_QUEUE_SIZE;
		vioblk[i].vq[0].vq_availoffset = sizeof(struct vring_desc) *
		    VIORND_QUEUE_SIZE;
		vioblk[i].vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
		    sizeof(struct vring_desc) * VIOBLK_QUEUE_SIZE
		    + sizeof(uint16_t) * (2 + VIOBLK_QUEUE_SIZE));
		vioblk[i].vq[0].last_avail = 0;
		vioblk[i].fd = child_disks[i];
		vioblk[i].sz = sz / 512;
	}

	vionet = malloc(sizeof(struct vionet_dev) * vcp->vcp_nnics);
	if (vionet == NULL) {
		fprintf(stderr, "%s: malloc failure allocating vionets\n",
		    __progname);
		return;
	}

	bzero(vionet, sizeof(struct vionet_dev) * vcp->vcp_nnics);

	nr_vionet = vcp->vcp_nnics;
	/* Virtio network */
	for (i = 0; i < vcp->vcp_nnics; i++) {
		if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
		    PCI_PRODUCT_QUMRANET_VIO_NET, PCI_CLASS_SYSTEM,
		    PCI_SUBCLASS_SYSTEM_MISC,
		    PCI_VENDOR_OPENBSD,
		    PCI_PRODUCT_VIRTIO_NETWORK, 1, NULL)) {
			fprintf(stderr, "%s: can't add PCI virtio net device\n",
			    __progname);
			return;
		}

		if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_net_io,
		    &vionet[i])) {
			fprintf(stderr, "%s: can't add bar for virtio net "
			    "device\n", __progname);
			return;
		}

		vionet[i].vq[0].qs = VIONET_QUEUE_SIZE;
		vionet[i].vq[0].vq_availoffset = sizeof(struct vring_desc) *
		    VIONET_QUEUE_SIZE;
		vionet[i].vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
		    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE
		    + sizeof(uint16_t) * (2 + VIONET_QUEUE_SIZE));
		vionet[i].vq[0].last_avail = 0;
		vionet[i].vq[1].qs = VIONET_QUEUE_SIZE;
		vionet[i].vq[1].vq_availoffset = sizeof(struct vring_desc) *
		    VIONET_QUEUE_SIZE;
		vionet[i].vq[1].vq_usedoffset = VIRTQUEUE_ALIGN(
		    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE
		    + sizeof(uint16_t) * (2 + VIONET_QUEUE_SIZE));
		vionet[i].vq[1].last_avail = 0;
		vionet[i].fd = child_taps[i];

#if 0
		/* User defined MAC */
		vionet[i].cfg.device_feature = VIRTIO_NET_F_MAC;
		bcopy(&vcp->vcp_macs[i], &vionet[i].mac, 6);
#endif
	}
}
