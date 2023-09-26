/*	$OpenBSD: vioblk.c,v 1.9 2023/09/26 01:53:54 dv Exp $	*/

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
struct iovec io_v[VIOBLK_QUEUE_SIZE];

static const char *disk_type(int);
static uint32_t handle_io_read(struct viodev_msg *, struct virtio_dev *,
    int8_t *);
static int handle_io_write(struct viodev_msg *, struct virtio_dev *);

static void vioblk_update_qs(struct vioblk_dev *);
static void vioblk_update_qa(struct vioblk_dev *);
static int vioblk_notifyq(struct vioblk_dev *);
static ssize_t vioblk_rw(struct vioblk_dev *, int, off_t,
    struct vring_desc *, struct vring_desc **);

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

	/*
	 * stdio - needed for read/write to disk fds and channels to the vm.
	 * vmm + proc - needed to create shared vm mappings.
	 */
	if (pledge("stdio vmm proc", NULL) == -1)
		fatal("pledge");

	/* Zero and initialize io work queue. */
	memset(io_v, 0, nitems(io_v)*sizeof(io_v[0]));

	/* Receive our virtio_dev, mostly preconfigured. */
	memset(&dev, 0, sizeof(dev));
	sz = atomicio(read, fd, &dev, sizeof(dev));
	if (sz != sizeof(dev)) {
		ret = errno;
		log_warn("failed to receive vioblk");
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
	    "async fd = %d, capacity = %lld seg_max = %u, vmm fd = %d",
	    __func__, vioblk->ndisk_fd, dev.sync_fd, dev.async_fd,
	    vioblk->capacity, vioblk->seg_max, fd_vmm);

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

	setproctitle("%s/vioblk%d", vcp->vcp_name, vioblk->idx);
	log_procinit("vm/%s/vioblk%d", vcp->vcp_name, vioblk->idx);

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
	vioblk->capacity = szp / 512;
	log_debug("%s: initialized vioblk%d with %s image (capacity=%lld)",
	    __func__, vioblk->idx, disk_type(type), vioblk->capacity);

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

static void
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

static void
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

/*
 * Process virtqueue notifications. If an unrecoverable error occurs, puts
 * device into a "needs reset" state.
 *
 * Returns 1 if an we need to assert an IRQ.
 */
static int
vioblk_notifyq(struct vioblk_dev *dev)
{
	uint32_t cmd_len;
	uint16_t idx, cmd_desc_idx;
	uint8_t ds;
	off_t offset;
	ssize_t sz;
	int is_write, notify, i;
	char *vr;
	struct vring_desc *table, *desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_blk_req_hdr *cmd;
	struct virtio_vq_info *vq_info;

	/* Invalid queue? */
	if (dev->cfg.queue_notify > 0)
		return (0);

	vq_info = &dev->vq[dev->cfg.queue_notify];
	idx = vq_info->last_avail;
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: null vring", __func__);

	/* Compute offsets in table of descriptors, avail ring, and used ring */
	table = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);

	while (idx != avail->idx) {
		/* Retrieve Command descriptor. */
		cmd_desc_idx = avail->ring[idx & VIOBLK_QUEUE_MASK];
		desc = &table[cmd_desc_idx];
		cmd_len = desc->len;

		/*
		 * Validate Command descriptor. It should be chained to another
		 * descriptor and not be itself writable.
		 */
		if ((desc->flags & VRING_DESC_F_NEXT) == 0) {
			log_warnx("%s: unchained cmd descriptor", __func__);
			goto reset;
		}
		if (DESC_WRITABLE(desc)) {
			log_warnx("%s: invalid cmd descriptor state", __func__);
			goto reset;
		}

		/* Retrieve the vioblk command request. */
		cmd = hvaddr_mem(desc->addr, sizeof(*cmd));
		if (cmd == NULL)
			goto reset;

		/* Advance to the 2nd descriptor. */
		desc = &table[desc->next & VIOBLK_QUEUE_MASK];

		/* Process each available command & chain. */
		switch (cmd->type) {
		case VIRTIO_BLK_T_IN:
		case VIRTIO_BLK_T_OUT:
			/* Read (IN) & Write (OUT) */
			is_write = (cmd->type == VIRTIO_BLK_T_OUT) ? 1 : 0;
			offset = cmd->sector * VIRTIO_BLK_SECTOR_SIZE;
			sz = vioblk_rw(dev, is_write, offset, table, &desc);
			if (sz == -1)
				ds = VIRTIO_BLK_S_IOERR;
			else
				ds = VIRTIO_BLK_S_OK;
			break;
		case VIRTIO_BLK_T_GET_ID:
			/*
			 * We don't support this command yet. While it's not
			 * officially part of the virtio spec (will be in v1.2)
			 * there's no feature to negotiate. Linux drivers will
			 * often send this command regardless.
			 */
			ds = VIRTIO_BLK_S_UNSUPP;
		default:
			log_warnx("%s: unsupported vioblk command %d", __func__,
			    cmd->type);
			ds = VIRTIO_BLK_S_UNSUPP;
			break;
		}

		/* Advance to the end of the chain, if needed. */
		i = 0;
		while (desc->flags & VRING_DESC_F_NEXT) {
			desc = &table[desc->next & VIOBLK_QUEUE_MASK];
			if (++i >= VIOBLK_QUEUE_SIZE) {
				/*
				 * If we encounter an infinite/looping chain,
				 * not much we can do but say we need a reset.
				 */
				log_warnx("%s: descriptor chain overflow",
				    __func__);
				goto reset;
			}
		}

		/* Provide the status of our command processing. */
		if (!DESC_WRITABLE(desc)) {
			log_warnx("%s: status descriptor unwritable", __func__);
			goto reset;
		}
		/* Overkill as ds is 1 byte, but validates gpa. */
		if (write_mem(desc->addr, &ds, sizeof(ds)))
			log_warnx("%s: can't write device status data "
			    "@ 0x%llx",__func__, desc->addr);

		dev->cfg.isr_status |= 1;
		notify = 1;

		used->ring[used->idx & VIOBLK_QUEUE_MASK].id = cmd_desc_idx;
		used->ring[used->idx & VIOBLK_QUEUE_MASK].len = cmd_len;

		__sync_synchronize();
		used->idx++;
		idx++;
	}

	vq_info->last_avail = idx;
	return (notify);

reset:
	/*
	 * When setting the "needs reset" flag, the driver is notified
	 * via a configuration change interrupt.
	 */
	dev->cfg.device_status |= DEVICE_NEEDS_RESET;
	dev->cfg.isr_status |= VIRTIO_CONFIG_ISR_CONFIG_CHANGE;
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
	int			 verbose;

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
		case IMSG_CTL_VERBOSE:
			IMSG_SIZE_CHECK(&imsg, &verbose);
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
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
	int8_t intr = INTR_STATE_NOOP;

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
			msg.data = handle_io_read(&msg, dev, &intr);
			msg.data_valid = 1;
			msg.state = intr;
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
		/* XXX We should be stricter about status checks. */
		if (!(vioblk->cfg.device_status & DEVICE_NEEDS_RESET)) {
			vioblk->cfg.queue_notify = data;
			if (vioblk_notifyq(vioblk))
				intr = 1;
		}
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
handle_io_read(struct viodev_msg *msg, struct virtio_dev *dev, int8_t *intr)
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
			data = (uint32_t)(vioblk->capacity);
			break;
		case 2:
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->capacity) & 0xFFFF;
			break;
		case 1:
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity) & 0xFF;
			break;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 8) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 16) & 0xFF;
		} else if (sz == 2) {
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->capacity >> 16) & 0xFFFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 24) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
		switch (sz) {
		case 4:
			data = (uint32_t)(vioblk->capacity >> 32);
			break;
		case 2:
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->capacity >> 32) & 0xFFFF;
			break;
		case 1:
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 32) & 0xFF;
			break;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 40) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 6:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 48) & 0xFF;
		} else if (sz == 2) {
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->capacity >> 48) & 0xFFFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 7:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->capacity >> 56) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 12:
		switch (sz) {
		case 4:
			data = (uint32_t)(vioblk->seg_max);
			break;
		case 2:
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->seg_max) & 0xFFFF;
			break;
		case 1:
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->seg_max) & 0xFF;
			break;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 13:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->seg_max >> 8) & 0xFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 14:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->seg_max >> 16) & 0xFF;
		} else if (sz == 2) {
			data &= 0xFFFF0000;
			data |= (uint32_t)(vioblk->seg_max >> 16)
			    & 0xFFFF;
		}
		/* XXX handle invalid sz */
		break;
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 15:
		if (sz == 1) {
			data &= 0xFFFFFF00;
			data |= (uint32_t)(vioblk->seg_max >> 24) & 0xFF;
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
		if (intr != NULL)
			*intr = INTR_STATE_DEASSERT;
		break;
	default:
		return (0xFFFFFFFF);
	}

	return (data);
}

/*
 * Emulate read/write io. Walks the descriptor chain, collecting io work and
 * then emulates the read or write.
 *
 * On success, returns bytes read/written.
 * On error, returns -1 and descriptor (desc) remains at its current position.
 */
static ssize_t
vioblk_rw(struct vioblk_dev *dev, int is_write, off_t offset,
    struct vring_desc *desc_tbl, struct vring_desc **desc)
{
	struct iovec *iov = NULL;
	ssize_t sz = 0;
	size_t io_idx = 0;		/* Index into iovec workqueue. */
	size_t xfer_sz = 0;		/* Total accumulated io bytes. */

	do {
		iov = &io_v[io_idx];

		/*
		 * Reads require writable descriptors. Writes require
		 * non-writeable descriptors.
		 */
		if ((!is_write) ^ DESC_WRITABLE(*desc)) {
			log_warnx("%s: invalid descriptor for %s command",
			    __func__, is_write ? "write" : "read");
			return (-1);
		}

		/* Collect the IO segment information. */
		iov->iov_len = (size_t)(*desc)->len;
		iov->iov_base = hvaddr_mem((*desc)->addr, iov->iov_len);
		if (iov->iov_base == NULL)
			return (-1);

		/* Move our counters. */
		xfer_sz += iov->iov_len;
		io_idx++;

		/* Guard against infinite chains */
		if (io_idx >= nitems(io_v)) {
			log_warnx("%s: descriptor table "
			    "invalid", __func__);
			return (-1);
		}

		/* Advance to the next descriptor. */
		*desc = &desc_tbl[(*desc)->next & VIOBLK_QUEUE_MASK];
	} while ((*desc)->flags & VRING_DESC_F_NEXT);

	/*
	 * Validate the requested block io operation alignment and size.
	 * Checking offset is just an extra caution as it is derived from
	 * a disk sector and is done for completeness in bounds checking.
	 */
	if (offset % VIRTIO_BLK_SECTOR_SIZE != 0 &&
	    xfer_sz % VIRTIO_BLK_SECTOR_SIZE != 0) {
		log_warnx("%s: unaligned read", __func__);
		return (-1);
	}
	if (xfer_sz > SSIZE_MAX) {	/* iovec_copyin limit */
		log_warnx("%s: invalid %s size: %zu", __func__,
		    is_write ? "write" : "read", xfer_sz);
		return (-1);
	}

	/* Emulate the Read or Write operation. */
	if (is_write)
		sz = dev->file.pwritev(dev->file.p, io_v, io_idx, offset);
	else
		sz = dev->file.preadv(dev->file.p, io_v, io_idx, offset);
	if (sz != (ssize_t)xfer_sz) {
		log_warnx("%s: %s failure at offset 0x%llx, xfer_sz=%zu, "
		    "sz=%ld", __func__, (is_write ? "write" : "read"), offset,
		    xfer_sz, sz);
		return (-1);
	}

	return (sz);
}
