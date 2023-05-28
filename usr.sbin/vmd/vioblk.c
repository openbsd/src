/*	$OpenBSD: vioblk.c,v 1.4 2023/05/28 05:28:50 asou Exp $	*/

/*
 * Copyright (c) 2023 Dave Voutila <dv@openbsd.org>
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
#include <sys/mman.h>
#include <sys/param.h> /* PAGE_SIZE */

#include <dev/pci/virtio_pcireg.h>
#include <dev/pv/vioblkreg.h>
#include <dev/pv/virtioreg.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "pci.h"
#include "virtio.h"
#include "vmd.h"

extern char *__progname;
extern struct vmd_vm *current_vm;

static const char *disk_type(int);
static uint32_t handle_io_read(struct viodev_msg *, struct virtio_dev *);
static int handle_io_write(struct viodev_msg *, struct virtio_dev *);
void vioblk_notify_rx(struct vioblk_dev *);
int vioblk_notifyq(struct vioblk_dev *);

static void dev_dispatch_vm(int, short, void *);
static void handle_sync_io(int, short, void *);

static const char *
disk_type(int type)
{
	switch (type) {
	case VMDF_RAW: return "raw";
	case VMDF_QCOW2: return "qcow2";
	}
	return "unknown";
}

__dead void
vioblk_main(int fd, int fd_vmm)
{
	struct virtio_dev	 dev;
	struct vioblk_dev	*vioblk;
	struct viodev_msg 	 msg;
	struct vmd_vm		 vm;
	struct vm_create_params	*vcp;
	ssize_t			 sz;
	off_t			 szp = 0;
	int			 i, ret, type;

	log_procinit("vioblk");

	/*
	 * stdio - needed for read/write to disk fds and channels to the vm.
	 * vmm + proc - needed to create shared vm mappings.
	 */
	if (pledge("stdio vmm proc", NULL) == -1)
		fatal("pledge");

	/* Receive our virtio_dev, mostly preconfigured. */
	memset(&dev, 0, sizeof(dev));
	sz = atomicio(read, fd, &dev, sizeof(dev));
	if (sz != sizeof(dev)) {
		ret = errno;
		log_warn("failed to receive vionet");
		goto fail;
	}
	if (dev.dev_type != VMD_DEVTYPE_DISK) {
		ret = EINVAL;
		log_warn("received invalid device type");
		goto fail;
	}
	dev.sync_fd = fd;
	vioblk = &dev.vioblk;

	log_debug("%s: got viblk dev. num disk fds = %d, sync fd = %d, "
	    "async fd = %d, sz = %lld maxfer = %d, vmm fd = %d", __func__,
	    vioblk->ndisk_fd, dev.sync_fd, dev.async_fd, vioblk->sz,
	    vioblk->max_xfer, fd_vmm);

	/* Receive our vm information from the vm process. */
	memset(&vm, 0, sizeof(vm));
	sz = atomicio(read, dev.sync_fd, &vm, sizeof(vm));
	if (sz != sizeof(vm)) {
		ret = EIO;
		log_warnx("failed to receive vm details");
		goto fail;
	}
	vcp = &vm.vm_params.vmc_params;
	current_vm = &vm;
	setproctitle("%s/vioblk[%d]", vcp->vcp_name, vioblk->idx);

	/* Now that we have our vm information, we can remap memory. */
	ret = remap_guest_mem(&vm, fd_vmm);
	if (ret) {
		log_warnx("failed to remap guest memory");
		goto fail;
	}

	/*
	 * We no longer need /dev/vmm access.
	 */
	close_fd(fd_vmm);
	if (pledge("stdio", NULL) == -1)
		fatal("pledge2");

	/* Initialize the virtio block abstractions. */
	type = vm.vm_params.vmc_disktypes[vioblk->idx];
	switch (type) {
	case VMDF_RAW:
		ret = virtio_raw_init(&vioblk->file, &szp, vioblk->disk_fd,
		    vioblk->ndisk_fd);
		break;
	case VMDF_QCOW2:
		ret = virtio_qcow2_init(&vioblk->file, &szp, vioblk->disk_fd,
		    vioblk->ndisk_fd);
		break;
	default:
		log_warnx("invalid disk image type");
		goto fail;
	}
	if (ret || szp < 0) {
		log_warnx("failed to init disk %s image", disk_type(type));
		goto fail;
	}
	vioblk->sz = szp / 512;
	log_debug("%s: initialized vioblk[%d] with %s image (sz=%lld)",
	    __func__, vioblk->idx, disk_type(type), vioblk->sz);

	/* If we're restoring hardware, reinitialize the virtqueue hva. */
	if (vm.vm_state & VM_STATE_RECEIVED)
		vioblk_update_qa(vioblk);

	/* Initialize libevent so we can start wiring event handlers. */
	event_init();

	/* Wire up an async imsg channel. */
	log_debug("%s: wiring in async vm event handler (fd=%d)", __func__,
		dev.async_fd);
	if (vm_device_pipe(&dev, dev_dispatch_vm)) {
		ret = EIO;
		log_warnx("vm_device_pipe");
		goto fail;
	}

	/* Configure our sync channel event handler. */
	log_debug("%s: wiring in sync channel handler (fd=%d)", __func__,
		dev.sync_fd);
	if (fcntl(dev.sync_fd, F_SETFL, O_NONBLOCK) == -1) {
		ret = errno;
		log_warn("%s: fcntl", __func__);
		goto fail;
	}
	imsg_init(&dev.sync_iev.ibuf, dev.sync_fd);
	dev.sync_iev.handler = handle_sync_io;
	dev.sync_iev.data = &dev;
	dev.sync_iev.events = EV_READ;
	imsg_event_add(&dev.sync_iev);

	/* Send a ready message over the sync channel. */
	log_debug("%s: telling vm %s device is ready", __func__, vcp->vcp_name);
	memset(&msg, 0, sizeof(msg));
	msg.type = VIODEV_MSG_READY;
	imsg_compose_event(&dev.sync_iev, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
	    sizeof(msg));

	/* Send a ready message over the async channel. */
	log_debug("%s: sending heartbeat", __func__);
	ret = imsg_compose_event(&dev.async_iev, IMSG_DEVOP_MSG, 0, 0, -1,
	    &msg, sizeof(msg));
	if (ret == -1) {
		log_warnx("%s: failed to send async ready message!", __func__);
		goto fail;
	}

	/* Engage the event loop! */
	ret = event_dispatch();

	if (ret == 0) {
		/* Clean shutdown. */
		close_fd(dev.sync_fd);
		close_fd(dev.async_fd);
		for (i = 0; i < (int)sizeof(vioblk->disk_fd); i++)
			close_fd(vioblk->disk_fd[i]);
		_exit(0);
		/* NOTREACHED */
	}

fail:
	/* Try letting the vm know we've failed something. */
	memset(&msg, 0, sizeof(msg));
	msg.type = VIODEV_MSG_ERROR;
	msg.data = ret;
	imsg_compose(&dev.sync_iev.ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
	    sizeof(msg));
	imsg_flush(&dev.sync_iev.ibuf);

	close_fd(dev.sync_fd);
	close_fd(dev.async_fd);
	for (i = 0; i < (int)sizeof(vioblk->disk_fd); i++)
		close_fd(vioblk->disk_fd[i]);
	_exit(ret);
	/* NOTREACHED */
}

const char *
vioblk_cmd_name(uint32_t type)
{
	switch (type) {
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

void
vioblk_update_qa(struct vioblk_dev *dev)
{
	struct virtio_vq_info *vq_info;
	void *hva = NULL;

	/* Invalid queue? */
	if (dev->cfg.queue_select > 0)
		return;

	vq_info = &dev->vq[dev->cfg.queue_select];
	vq_info->q_gpa = (uint64_t)dev->cfg.queue_pfn * VIRTIO_PAGE_SIZE;

	hva = hvaddr_mem(vq_info->q_gpa, vring_size(VIOBLK_QUEUE_SIZE));
	if (hva == NULL)
		fatal("vioblk_update_qa");
	vq_info->q_hva = hva;
}

void
vioblk_update_qs(struct vioblk_dev *dev)
{
	struct virtio_vq_info *vq_info;

	/* Invalid queue? */
	if (dev->cfg.queue_select > 0) {
		dev->cfg.queue_size = 0;
		return;
	}

	vq_info = &dev->vq[dev->cfg.queue_select];

	/* Update queue pfn/size based on queue select */
	dev->cfg.queue_pfn = vq_info->q_gpa >> 12;
	dev->cfg.queue_size = vq_info->qs;
}

static void
vioblk_free_info(struct ioinfo *info)
{
	if (!info)
		return;
	free(info->buf);
	free(info);
}

static struct ioinfo *
vioblk_start_read(struct vioblk_dev *dev, off_t sector, size_t sz)
{
	struct ioinfo *info;

	/* Limit to 64M for now */
	if (sz > (1 << 26)) {
		log_warnx("%s: read size exceeded 64M", __func__);
		return (NULL);
	}

	info = calloc(1, sizeof(*info));
	if (!info)
		goto nomem;
	info->buf = malloc(sz);
	if (info->buf == NULL)
		goto nomem;
	info->len = sz;
	info->offset = sector * VIRTIO_BLK_SECTOR_SIZE;
	info->file = &dev->file;
	return info;

nomem:
	free(info);
	log_warn("malloc error vioblk read");
	return (NULL);
}


static const uint8_t *
vioblk_finish_read(struct ioinfo *info)
{
	struct virtio_backing *file;

	file = info->file;
	if (file == NULL || file->pread == NULL) {
		log_warnx("%s: XXX null?!", __func__);
		return NULL;
	}
	if (file->pread(file->p, info->buf, info->len, info->offset) != info->len) {
		info->error = errno;
		log_warn("vioblk read error");
		return NULL;
	}

	return info->buf;
}

static struct ioinfo *
vioblk_start_write(struct vioblk_dev *dev, off_t sector,
    paddr_t addr, size_t len)
{
	struct ioinfo *info;

	/* Limit to 64M for now */
	if (len > (1 << 26)) {
		log_warnx("%s: write size exceeded 64M", __func__);
		return (NULL);
	}

	info = calloc(1, sizeof(*info));
	if (!info)
		goto nomem;

	info->buf = malloc(len);
	if (info->buf == NULL)
		goto nomem;
	info->len = len;
	info->offset = sector * VIRTIO_BLK_SECTOR_SIZE;
	info->file = &dev->file;

	if (read_mem(addr, info->buf, info->len)) {
		vioblk_free_info(info);
		return NULL;
	}

	return info;

nomem:
	free(info);
	log_warn("malloc error vioblk write");
	return (NULL);
}

static int
vioblk_finish_write(struct ioinfo *info)
{
	struct virtio_backing *file;

	file = info->file;
	if (file->pwrite(file->p, info->buf, info->len, info->offset) != info->len) {
		log_warn("vioblk write error");
		return EIO;
	}
	return 0;
}

/*
 * XXX in various cases, ds should be set to VIRTIO_BLK_S_IOERR, if we can
 */
int
vioblk_notifyq(struct vioblk_dev *dev)
{
	uint16_t idx, cmd_desc_idx, secdata_desc_idx, ds_desc_idx;
	uint8_t ds;
	int cnt;
	off_t secbias;
	char *vr;
	struct vring_desc *desc, *cmd_desc, *secdata_desc, *ds_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_blk_req_hdr cmd;
	struct virtio_vq_info *vq_info;

	/* Invalid queue? */
	if (dev->cfg.queue_notify > 0)
		return (0);

	vq_info = &dev->vq[dev->cfg.queue_notify];
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: null vring", __func__);

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);

	idx = vq_info->last_avail & VIOBLK_QUEUE_MASK;

	if ((avail->idx & VIOBLK_QUEUE_MASK) == idx) {
		log_debug("%s - nothing to do?", __func__);
		return (0);
	}

	while (idx != (avail->idx & VIOBLK_QUEUE_MASK)) {

		ds = VIRTIO_BLK_S_IOERR;
		cmd_desc_idx = avail->ring[idx] & VIOBLK_QUEUE_MASK;
		cmd_desc = &desc[cmd_desc_idx];

		if ((cmd_desc->flags & VRING_DESC_F_NEXT) == 0) {
			log_warnx("unchained vioblk cmd descriptor received "
			    "(idx %d)", cmd_desc_idx);
			goto out;
		}

		/* Read command from descriptor ring */
		if (cmd_desc->flags & VRING_DESC_F_WRITE) {
			log_warnx("vioblk: unexpected writable cmd descriptor "
			    "%d", cmd_desc_idx);
			goto out;
		}
		if (read_mem(cmd_desc->addr, &cmd, sizeof(cmd))) {
			log_warnx("vioblk: command read_mem error @ 0x%llx",
			    cmd_desc->addr);
			goto out;
		}

		switch (cmd.type) {
		case VIRTIO_BLK_T_IN:
			/* first descriptor */
			secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];

			if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
				log_warnx("unchained vioblk data descriptor "
				    "received (idx %d)", cmd_desc_idx);
				goto out;
			}

			cnt = 0;
			secbias = 0;
			do {
				struct ioinfo *info;
				const uint8_t *secdata;

				if ((secdata_desc->flags & VRING_DESC_F_WRITE)
				    == 0) {
					log_warnx("vioblk: unwritable data "
					    "descriptor %d", secdata_desc_idx);
					goto out;
				}

				info = vioblk_start_read(dev,
				    cmd.sector + secbias, secdata_desc->len);

				if (info == NULL) {
					log_warnx("vioblk: can't start read");
					goto out;
				}

				/* read the data, use current data descriptor */
				secdata = vioblk_finish_read(info);
				if (secdata == NULL) {
					vioblk_free_info(info);
					log_warnx("vioblk: block read error, "
					    "sector %lld", cmd.sector);
					goto out;
				}

				if (write_mem(secdata_desc->addr, secdata,
					secdata_desc->len)) {
					log_warnx("can't write sector "
					    "data to gpa @ 0x%llx",
					    secdata_desc->addr);
					vioblk_free_info(info);
					goto out;
				}

				vioblk_free_info(info);

				secbias += (secdata_desc->len /
				    VIRTIO_BLK_SECTOR_SIZE);
				secdata_desc_idx = secdata_desc->next &
				    VIOBLK_QUEUE_MASK;
				secdata_desc = &desc[secdata_desc_idx];

				/* Guard against infinite chains */
				if (++cnt >= VIOBLK_QUEUE_SIZE) {
					log_warnx("%s: descriptor table "
					    "invalid", __func__);
					goto out;
				}
			} while (secdata_desc->flags & VRING_DESC_F_NEXT);

			ds_desc_idx = secdata_desc_idx;
			ds_desc = secdata_desc;

			ds = VIRTIO_BLK_S_OK;
			break;
		case VIRTIO_BLK_T_OUT:
			secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];

			if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
				log_warnx("wr vioblk: unchained vioblk data "
				    "descriptor received (idx %d)",
				    cmd_desc_idx);
				goto out;
			}

			if (secdata_desc->len > dev->max_xfer) {
				log_warnx("%s: invalid read size %d requested",
				    __func__, secdata_desc->len);
				goto out;
			}

			cnt = 0;
			secbias = 0;
			do {
				struct ioinfo *info;

				if (secdata_desc->flags & VRING_DESC_F_WRITE) {
					log_warnx("wr vioblk: unexpected "
					    "writable data descriptor %d",
					    secdata_desc_idx);
					goto out;
				}

				info = vioblk_start_write(dev,
				    cmd.sector + secbias,
				    secdata_desc->addr, secdata_desc->len);

				if (info == NULL) {
					log_warnx("wr vioblk: can't read "
					    "sector data @ 0x%llx",
					    secdata_desc->addr);
					goto out;
				}

				if (vioblk_finish_write(info)) {
					log_warnx("wr vioblk: disk write "
					    "error");
					vioblk_free_info(info);
					goto out;
				}

				vioblk_free_info(info);

				secbias += secdata_desc->len /
				    VIRTIO_BLK_SECTOR_SIZE;

				secdata_desc_idx = secdata_desc->next &
				    VIOBLK_QUEUE_MASK;
				secdata_desc = &desc[secdata_desc_idx];

				/* Guard against infinite chains */
				if (++cnt >= VIOBLK_QUEUE_SIZE) {
					log_warnx("%s: descriptor table "
					    "invalid", __func__);
					goto out;
				}
			} while (secdata_desc->flags & VRING_DESC_F_NEXT);

			ds_desc_idx = secdata_desc_idx;
			ds_desc = secdata_desc;

			ds = VIRTIO_BLK_S_OK;
			break;
		case VIRTIO_BLK_T_FLUSH:
		case VIRTIO_BLK_T_FLUSH_OUT:
			ds_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			ds_desc = &desc[ds_desc_idx];

			ds = VIRTIO_BLK_S_UNSUPP;
			break;
		case VIRTIO_BLK_T_GET_ID:
			secdata_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			secdata_desc = &desc[secdata_desc_idx];

			/*
			 * We don't support this command yet. While it's not
			 * officially part of the virtio spec (will be in v1.2)
			 * there's no feature to negotiate. Linux drivers will
			 * often send this command regardless.
			 *
			 * When the command is received, it should appear as a
			 * chain of 3 descriptors, similar to the IN/OUT
			 * commands. The middle descriptor should have have a
			 * length of VIRTIO_BLK_ID_BYTES bytes.
			 */
			if ((secdata_desc->flags & VRING_DESC_F_NEXT) == 0) {
				log_warnx("id vioblk: unchained vioblk data "
				    "descriptor received (idx %d)",
				    cmd_desc_idx);
				goto out;
			}

			/* Skip the data descriptor. */
			ds_desc_idx = secdata_desc->next & VIOBLK_QUEUE_MASK;
			ds_desc = &desc[ds_desc_idx];

			ds = VIRTIO_BLK_S_UNSUPP;
			break;
		default:
			log_warnx("%s: unsupported command 0x%x", __func__,
			    cmd.type);
			ds_desc_idx = cmd_desc->next & VIOBLK_QUEUE_MASK;
			ds_desc = &desc[ds_desc_idx];

			ds = VIRTIO_BLK_S_UNSUPP;
			break;
		}

		if ((ds_desc->flags & VRING_DESC_F_WRITE) == 0) {
			log_warnx("%s: ds descriptor %d unwritable", __func__,
			    ds_desc_idx);
			goto out;
		}
		if (write_mem(ds_desc->addr, &ds, sizeof(ds))) {
			log_warnx("%s: can't write device status data @ 0x%llx",
			    __func__, ds_desc->addr);
			goto out;
		}

		dev->cfg.isr_status = 1;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].id = cmd_desc_idx;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].len = cmd_desc->len;
		__sync_synchronize();
		used->idx++;

		vq_info->last_avail = avail->idx & VIOBLK_QUEUE_MASK;
		idx = (idx + 1) & VIOBLK_QUEUE_MASK;
	}
out:
	return (1);
}

static void
dev_dispatch_vm(int fd, short event, void *arg)
{
	struct virtio_dev	*dev = (struct virtio_dev *)arg;
	struct imsgev		*iev = &dev->async_iev;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg	 	 imsg;
	ssize_t			 n = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("%s: imsg_read", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: pipe dead (EV_READ)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("%s: msgbuf_write", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: pipe dead (EV_WRITE)", __func__);
			event_del(&iev->ev);
			event_loopbreak();
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get", __func__);
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_VMDOP_PAUSE_VM:
			log_debug("%s: pausing", __func__);
			break;
		case IMSG_VMDOP_UNPAUSE_VM:
			log_debug("%s: unpausing", __func__);
			break;
		default:
			log_warnx("%s: unhandled imsg type %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

/*
 * Synchronous IO handler.
 *
 */
static void
handle_sync_io(int fd, short event, void *arg)
{
	struct virtio_dev *dev = (struct virtio_dev *)arg;
	struct imsgev *iev = &dev->sync_iev;
	struct imsgbuf *ibuf = &iev->ibuf;
	struct viodev_msg msg;
	struct imsg imsg;
	ssize_t n;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("%s: imsg_read", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: vioblk pipe dead (EV_READ)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("%s: msgbuf_write", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			log_debug("%s: vioblk pipe dead (EV_WRITE)", __func__);
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatalx("%s: imsg_get (n=%ld)", __func__, n);
		if (n == 0)
			break;

		/* Unpack our message. They ALL should be dev messeges! */
		IMSG_SIZE_CHECK(&imsg, &msg);
		memcpy(&msg, imsg.data, sizeof(msg));
		imsg_free(&imsg);

		switch (msg.type) {
		case VIODEV_MSG_DUMP:
			/* Dump device */
			n = atomicio(vwrite, dev->sync_fd, dev, sizeof(*dev));
			if (n != sizeof(*dev)) {
				log_warnx("%s: failed to dump vioblk device",
				    __func__);
				break;
			}
		case VIODEV_MSG_IO_READ:
			/* Read IO: make sure to send a reply */
			msg.data = handle_io_read(&msg, dev);
			msg.data_valid = 1;
			imsg_compose_event(iev, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
			    sizeof(msg));
			break;
		case VIODEV_MSG_IO_WRITE:
			/* Write IO: no reply needed */
			if (handle_io_write(&msg, dev) == 1)
				virtio_assert_pic_irq(dev, 0);
			break;
		case VIODEV_MSG_SHUTDOWN:
			event_del(&dev->sync_iev.ev);
			event_loopbreak();
			return;
		default:
			fatalx("%s: invalid msg type %d", __func__, msg.type);
		}
	}
	imsg_event_add(iev);
}

static int
handle_io_write(struct viodev_msg *msg, struct virtio_dev *dev)
{
	struct vioblk_dev *vioblk = &dev->vioblk;
	uint32_t data = msg->data;
	int intr = 0;

	switch (msg->reg) {
	case VIRTIO_CONFIG_DEVICE_FEATURES:
	case VIRTIO_CONFIG_QUEUE_SIZE:
	case VIRTIO_CONFIG_ISR_STATUS:
		log_warnx("%s: illegal write %x to %s", __progname, data,
		    virtio_reg_name(msg->reg));
		break;
	case VIRTIO_CONFIG_GUEST_FEATURES:
		vioblk->cfg.guest_feature = data;
		break;
	case VIRTIO_CONFIG_QUEUE_PFN:
		vioblk->cfg.queue_pfn = data;
		vioblk_update_qa(vioblk);
		break;
	case VIRTIO_CONFIG_QUEUE_SELECT:
		vioblk->cfg.queue_select = data;
		vioblk_update_qs(vioblk);
		break;
	case VIRTIO_CONFIG_QUEUE_NOTIFY:
		vioblk->cfg.queue_notify = data;
		if (vioblk_notifyq(vioblk))
			intr = 1;
		break;
	case VIRTIO_CONFIG_DEVICE_STATUS:
		vioblk->cfg.device_status = data;
		if (vioblk->cfg.device_status == 0) {
			vioblk->cfg.guest_feature = 0;
			vioblk->cfg.queue_pfn = 0;
			vioblk_update_qa(vioblk);
			vioblk->cfg.queue_size = 0;
			vioblk_update_qs(vioblk);
			vioblk->cfg.queue_select = 0;
			vioblk->cfg.queue_notify = 0;
			vioblk->cfg.isr_status = 0;
			vioblk->vq[0].last_avail = 0;
			vioblk->vq[0].notified_avail = 0;
			virtio_deassert_pic_irq(dev, msg->vcpu);
		}
		break;
	default:
		break;
	}
	return (intr);
}

static uint32_t
handle_io_read(struct viodev_msg *msg, struct virtio_dev *dev)
{
	struct vioblk_dev *vioblk = &dev->vioblk;
	uint8_t sz = msg->io_sz;
	uint32_t data;

	if (msg->data_valid)
		data = msg->data;
	else
		data = 0;

	switch (msg->reg) {
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
		switch (sz) {
		case 4:
			data = (uint32_t)(vioblk->sz);
			break;
		case 2:
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->sz) & 0xFFFF;
			break;
		case 1:
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz) & 0xFF;
			break;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 8) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 16) & 0xFF;
		} else if (sz == 2) {
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->sz >> 16) & 0xFFFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 24) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
		switch (sz) {
		case 4:
			data = (uint32_t)(vioblk->sz >> 32);
			break;
		case 2:
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->sz >> 32) & 0xFFFF;
			break;
		case 1:
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 32) & 0xFF;
			break;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 40) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 6:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 48) & 0xFF;
		} else if (sz == 2) {
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->sz >> 48) & 0xFFFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 7:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->sz >> 56) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8:
		switch (sz) {
		case 4:
			data = (uint32_t)(vioblk->max_xfer);
			break;
		case 2:
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->max_xfer) & 0xFFFF;
			break;
		case 1:
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->max_xfer) & 0xFF;
			break;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 9:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->max_xfer >> 8) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 10:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->max_xfer >> 16) & 0xFF;
		} else if (sz == 2) {
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->max_xfer >> 16)
			    & 0xFFFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 11:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->max_xfer >> 24) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_FEATURES:
		data = vioblk->cfg.device_feature;
		break;
	case VIRTIO_CONFIG_GUEST_FEATURES:
		data = vioblk->cfg.guest_feature;
		break;
	case VIRTIO_CONFIG_QUEUE_PFN:
		data = vioblk->cfg.queue_pfn;
		break;
	case VIRTIO_CONFIG_QUEUE_SIZE:
		data = vioblk->cfg.queue_size;
		break;
	case VIRTIO_CONFIG_QUEUE_SELECT:
		data = vioblk->cfg.queue_select;
		break;
	case VIRTIO_CONFIG_QUEUE_NOTIFY:
		data = vioblk->cfg.queue_notify;
		break;
	case VIRTIO_CONFIG_DEVICE_STATUS:
		data = vioblk->cfg.device_status;
		break;
	case VIRTIO_CONFIG_ISR_STATUS:
		data = vioblk->cfg.isr_status;
		vioblk->cfg.isr_status = 0;
		virtio_deassert_pic_irq(dev, 0);
		break;
	default:
		return (0xFFFFFFFF);
	}

	return (data);
}
