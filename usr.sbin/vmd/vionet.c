/*	$OpenBSD: vionet.c,v 1.6 2023/09/26 01:53:54 dv Exp $	*/

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
#include <sys/socket.h>

#include <dev/pci/virtio_pcireg.h>
#include <dev/pv/virtioreg.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

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

#define VIRTIO_NET_F_MAC	(1 << 5)
#define RXQ	0
#define TXQ	1

extern char *__progname;
extern struct vmd_vm *current_vm;

/* Device Globals */
struct event ev_tap;

static int vionet_rx(struct vionet_dev *);
static void vionet_rx_event(int, short, void *);
static uint32_t handle_io_read(struct viodev_msg *, struct virtio_dev *,
    int8_t *);
static int handle_io_write(struct viodev_msg *, struct virtio_dev *);
void vionet_notify_rx(struct virtio_dev *);
int vionet_notifyq(struct virtio_dev *);

static void dev_dispatch_vm(int, short, void *);
static void handle_sync_io(int, short, void *);

__dead void
vionet_main(int fd, int fd_vmm)
{
	struct virtio_dev	 dev;
	struct vionet_dev	*vionet = NULL;
	struct viodev_msg 	 msg;
	struct vmd_vm	 	 vm;
	struct vm_create_params	*vcp;
	ssize_t			 sz;
	int			 ret;

	/*
	 * stdio - needed for read/write to disk fds and channels to the vm.
	 * vmm + proc - needed to create shared vm mappings.
	 */
	if (pledge("stdio vmm proc", NULL) == -1)
		fatal("pledge");

	/* Receive our vionet_dev, mostly preconfigured. */
	sz = atomicio(read, fd, &dev, sizeof(dev));
	if (sz != sizeof(dev)) {
		ret = errno;
		log_warn("failed to receive vionet");
		goto fail;
	}
	if (dev.dev_type != VMD_DEVTYPE_NET) {
		ret = EINVAL;
		log_warn("received invalid device type");
		goto fail;
	}
	dev.sync_fd = fd;
	vionet = &dev.vionet;

	log_debug("%s: got vionet dev. tap fd = %d, syncfd = %d, asyncfd = %d"
	    ", vmm fd = %d", __func__, vionet->data_fd, dev.sync_fd,
	    dev.async_fd, fd_vmm);

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
	setproctitle("%s/vionet%d", vcp->vcp_name, vionet->idx);
	log_procinit("vm/%s/vionet%d", vcp->vcp_name, vionet->idx);

	/* Now that we have our vm information, we can remap memory. */
	ret = remap_guest_mem(&vm, fd_vmm);
	if (ret) {
		fatal("%s: failed to remap", __func__);
		goto fail;
	}

	/*
	 * We no longer need /dev/vmm access.
	 */
	close_fd(fd_vmm);
	if (pledge("stdio", NULL) == -1)
		fatal("pledge2");

	/* If we're restoring hardware, re-initialize virtqueue hva's. */
	if (vm.vm_state & VM_STATE_RECEIVED) {
		struct virtio_vq_info *vq_info;
		void *hva = NULL;

		vq_info = &dev.vionet.vq[TXQ];
		if (vq_info->q_gpa != 0) {
			log_debug("%s: restoring TX virtqueue for gpa 0x%llx",
			    __func__, vq_info->q_gpa);
			hva = hvaddr_mem(vq_info->q_gpa,
			    vring_size(VIONET_QUEUE_SIZE));
			if (hva == NULL)
				fatalx("%s: hva == NULL", __func__);
			vq_info->q_hva = hva;
		}

		vq_info = &dev.vionet.vq[RXQ];
		if (vq_info->q_gpa != 0) {
			log_debug("%s: restoring RX virtqueue for gpa 0x%llx",
			    __func__, vq_info->q_gpa);
			hva = hvaddr_mem(vq_info->q_gpa,
			    vring_size(VIONET_QUEUE_SIZE));
			if (hva == NULL)
				fatalx("%s: hva == NULL", __func__);
			vq_info->q_hva = hva;
		}
	}

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

	/* Wire up event handling for the tap fd. */
	log_debug("%s: wiring in tap fd handler (fd=%d)", __func__,
	    vionet->data_fd);
	event_set(&ev_tap, vionet->data_fd, EV_READ | EV_PERSIST,
	    vionet_rx_event, &dev);

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
	log_debug("%s: sending async ready message", __func__);
	ret = imsg_compose_event(&dev.async_iev, IMSG_DEVOP_MSG, 0, 0, -1,
	    &msg, sizeof(msg));
	if (ret == -1) {
		log_warnx("%s: failed to send async ready message!", __func__);
		goto fail;
	}

	/* Engage the event loop! */
	ret = event_dispatch();

	/* Cleanup */
	if (ret == 0) {
		close_fd(dev.sync_fd);
		close_fd(dev.async_fd);
		close_fd(vionet->data_fd);
		_exit(ret);
		/* NOTREACHED */
	}
fail:
	/* Try firing off a message to the vm saying we're dying. */
	memset(&msg, 0, sizeof(msg));
	msg.type = VIODEV_MSG_ERROR;
	msg.data = ret;
	imsg_compose(&dev.sync_iev.ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
	    sizeof(msg));
	imsg_flush(&dev.sync_iev.ibuf);

	close_fd(dev.sync_fd);
	close_fd(dev.async_fd);
	if (vionet != NULL)
		close_fd(vionet->data_fd);

	_exit(ret);
}

/*
 * Update the gpa and hva of the virtqueue.
 */
void
vionet_update_qa(struct vionet_dev *dev)
{
	struct virtio_vq_info *vq_info;
	void *hva = NULL;

	/* Invalid queue? */
	if (dev->cfg.queue_select > 1)
		return;

	vq_info = &dev->vq[dev->cfg.queue_select];
	vq_info->q_gpa = (uint64_t)dev->cfg.queue_pfn * VIRTIO_PAGE_SIZE;
	dev->cfg.queue_pfn = vq_info->q_gpa >> 12;

	if (vq_info->q_gpa == 0)
		vq_info->q_hva = NULL;

	hva = hvaddr_mem(vq_info->q_gpa, vring_size(VIONET_QUEUE_SIZE));
	if (hva == NULL)
		fatalx("%s: hva == NULL", __func__);

	vq_info->q_hva = hva;
}

/*
 * Update the queue size.
 */
void
vionet_update_qs(struct vionet_dev *dev)
{
	struct virtio_vq_info *vq_info;

	/* Invalid queue? */
	if (dev->cfg.queue_select > 1) {
		log_warnx("%s: !!! invalid queue selector %d", __func__,
		    dev->cfg.queue_select);
		dev->cfg.queue_size = 0;
		return;
	}

	vq_info = &dev->vq[dev->cfg.queue_select];

	/* Update queue pfn/size based on queue select */
	dev->cfg.queue_pfn = vq_info->q_gpa >> 12;
	dev->cfg.queue_size = vq_info->qs;
}

/*
 * vionet_enq_rx
 *
 * Take a given packet from the host-side tap and copy it into the guest's
 * buffers utilizing the rx virtio ring. If the packet length is invalid
 * (too small or too large) or if there are not enough buffers available,
 * the packet is dropped.
 */
int
vionet_enq_rx(struct vionet_dev *dev, char *pkt, size_t sz, int *spc)
{
	uint16_t dxx, idx, hdr_desc_idx, chain_hdr_idx;
	char *vr = NULL;
	size_t bufsz = 0, off = 0, pkt_offset = 0, chunk_size = 0;
	size_t chain_len = 0;
	struct vring_desc *desc, *pkt_desc, *hdr_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_vq_info *vq_info;
	struct virtio_net_hdr hdr;
	size_t hdr_sz;

	if (sz < VIONET_MIN_TXLEN || sz > VIONET_MAX_TXLEN) {
		log_warnx("%s: invalid packet size", __func__);
		return (0);
	}

	hdr_sz = sizeof(hdr);

	if (!(dev->cfg.device_status
	    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK)) {
		log_warnx("%s: driver not ready", __func__);
		return (0);
	}

	vq_info = &dev->vq[RXQ];
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: vr == NULL", __func__);


	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);

	idx = vq_info->last_avail & VIONET_QUEUE_MASK;
	if ((vq_info->notified_avail & VIONET_QUEUE_MASK) == idx) {
		log_debug("%s: insufficient available buffer capacity, "
		    "dropping packet.", __func__);
		return (0);
	}

	hdr_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
	hdr_desc = &desc[hdr_desc_idx];

	dxx = hdr_desc_idx;
	chain_hdr_idx = dxx;
	chain_len = 0;

	/* Process the descriptor and walk any potential chain. */
	do {
		off = 0;
		pkt_desc = &desc[dxx];
		if (!(pkt_desc->flags & VRING_DESC_F_WRITE)) {
			log_warnx("%s: invalid descriptor, not writable",
			    __func__);
			return (0);
		}

		/* How much data do we get to write? */
		if (sz - bufsz > pkt_desc->len)
			chunk_size = pkt_desc->len;
		else
			chunk_size = sz - bufsz;

		if (chain_len == 0) {
			off = hdr_sz;
			if (chunk_size == pkt_desc->len)
				chunk_size -= off;
		}

		/* Write a chunk of data if we need to */
		if (chunk_size && write_mem(pkt_desc->addr + off,
			pkt + pkt_offset, chunk_size)) {
			log_warnx("%s: failed to write to buffer 0x%llx",
			    __func__, pkt_desc->addr);
			return (0);
		}

		chain_len += chunk_size + off;
		bufsz += chunk_size;
		pkt_offset += chunk_size;

		dxx = pkt_desc->next & VIONET_QUEUE_MASK;
	} while (bufsz < sz && pkt_desc->flags & VRING_DESC_F_NEXT);

	/* Move our marker in the ring...*/
	vq_info->last_avail = (vq_info->last_avail + 1) &
	    VIONET_QUEUE_MASK;

	/* Prepend the virtio net header in the first buffer. */
	memset(&hdr, 0, sizeof(hdr));
	hdr.hdr_len = hdr_sz;
	if (write_mem(hdr_desc->addr, &hdr, hdr_sz)) {
	    log_warnx("vionet: rx enq header write_mem error @ 0x%llx",
		hdr_desc->addr);
	    return (0);
	}

	/* Update the index field in the used ring. This must be done last. */
	dev->cfg.isr_status = 1;
	*spc = (vq_info->notified_avail - vq_info->last_avail)
	    & VIONET_QUEUE_MASK;

	/* Update the list of used buffers. */
	used->ring[used->idx & VIONET_QUEUE_MASK].id = chain_hdr_idx;
	used->ring[used->idx & VIONET_QUEUE_MASK].len = chain_len;
	__sync_synchronize();
	used->idx++;

	return (1);
}

/*
 * vionet_rx
 *
 * Enqueue data that was received on a tap file descriptor
 * to the vionet device queue.
 */
static int
vionet_rx(struct vionet_dev *dev)
{
	char buf[PAGE_SIZE];
	int num_enq = 0, spc = 0;
	struct ether_header *eh;
	ssize_t sz;

	do {
		sz = read(dev->data_fd, buf, sizeof(buf));
		if (sz == -1) {
			/*
			 * If we get EAGAIN, No data is currently available.
			 * Do not treat this as an error.
			 */
			if (errno != EAGAIN)
				log_warn("%s: read error", __func__);
		} else if (sz > 0) {
			eh = (struct ether_header *)buf;
			if (!dev->lockedmac ||
			    ETHER_IS_MULTICAST(eh->ether_dhost) ||
			    memcmp(eh->ether_dhost, dev->mac,
			    sizeof(eh->ether_dhost)) == 0)
				num_enq += vionet_enq_rx(dev, buf, sz, &spc);
		} else if (sz == 0) {
			log_debug("%s: no data", __func__);
			break;
		}
	} while (spc > 0 && sz > 0);

	return (num_enq);
}

/*
 * vionet_rx_event
 *
 * Called when new data can be received on the tap fd of a vionet device.
 */
static void
vionet_rx_event(int fd, short kind, void *arg)
{
	struct virtio_dev *dev = (struct virtio_dev *)arg;

	if (vionet_rx(&dev->vionet) > 0)
		virtio_assert_pic_irq(dev, 0);
}

void
vionet_notify_rx(struct virtio_dev *dev)
{
	struct vionet_dev *vionet = &dev->vionet;
	struct vring_avail *avail;
	struct virtio_vq_info *vq_info;
	char *vr;

	vq_info = &vionet->vq[RXQ];
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: vr == NULL", __func__);

	/* Compute offset into avail ring */
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	vq_info->notified_avail = avail->idx - 1;
}

int
vionet_notifyq(struct virtio_dev *dev)
{
	struct vionet_dev *vionet = &dev->vionet;
	int ret = 0;

	switch (vionet->cfg.queue_notify) {
	case RXQ:
		vionet_notify_rx(dev);
		break;
	case TXQ:
		ret = vionet_notify_tx(dev);
		break;
	default:
		/*
		 * Catch the unimplemented queue ID 2 (control queue) as
		 * well as any bogus queue IDs.
		 */
		log_debug("%s: notify for unimplemented queue ID %d",
		    __func__, vionet->cfg.queue_notify);
		break;
	}

	return (ret);
}

int
vionet_notify_tx(struct virtio_dev *dev)
{
	uint16_t idx, pkt_desc_idx, hdr_desc_idx, dxx, cnt;
	size_t pktsz, chunk_size = 0;
	ssize_t dhcpsz = 0;
	int num_enq, ofs, spc = 0;
	char *vr = NULL, *pkt = NULL, *dhcppkt = NULL;
	struct vionet_dev *vionet = &dev->vionet;
	struct vring_desc *desc, *pkt_desc, *hdr_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_vq_info *vq_info;
	struct ether_header *eh;

	if (!(vionet->cfg.device_status
	    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK)) {
		log_warnx("%s: driver not ready", __func__);
		return (0);
	}

	vq_info = &vionet->vq[TXQ];
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: vr == NULL", __func__);

	/* Compute offsets in ring of descriptors, avail ring, and used ring */
	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);

	num_enq = 0;

	idx = vq_info->last_avail & VIONET_QUEUE_MASK;

	if ((avail->idx & VIONET_QUEUE_MASK) == idx)
		return (0);

	while ((avail->idx & VIONET_QUEUE_MASK) != idx) {
		hdr_desc_idx = avail->ring[idx] & VIONET_QUEUE_MASK;
		hdr_desc = &desc[hdr_desc_idx];
		pktsz = 0;

		cnt = 0;
		dxx = hdr_desc_idx;
		do {
			pktsz += desc[dxx].len;
			dxx = desc[dxx].next & VIONET_QUEUE_MASK;

			/*
			 * Virtio 1.0, cs04, section 2.4.5:
			 *  "The number of descriptors in the table is defined
			 *   by the queue size for this virtqueue: this is the
			 *   maximum possible descriptor chain length."
			 */
			if (++cnt >= VIONET_QUEUE_SIZE) {
				log_warnx("%s: descriptor table invalid",
				    __func__);
				goto out;
			}
		} while (desc[dxx].flags & VRING_DESC_F_NEXT);

		pktsz += desc[dxx].len;

		/* Remove virtio header descriptor len */
		pktsz -= hdr_desc->len;

		/* Drop packets violating device MTU-based limits */
		if (pktsz < VIONET_MIN_TXLEN || pktsz > VIONET_MAX_TXLEN) {
			log_warnx("%s: invalid packet size %lu", __func__,
			    pktsz);
			goto drop_packet;
		}
		pkt = malloc(pktsz);
		if (pkt == NULL) {
			log_warn("malloc error alloc packet buf");
			goto out;
		}

		ofs = 0;
		pkt_desc_idx = hdr_desc->next & VIONET_QUEUE_MASK;
		pkt_desc = &desc[pkt_desc_idx];

		while (pkt_desc->flags & VRING_DESC_F_NEXT) {
			/* must be not writable */
			if (pkt_desc->flags & VRING_DESC_F_WRITE) {
				log_warnx("unexpected writable tx desc "
				    "%d", pkt_desc_idx);
				goto out;
			}

			/* Check we don't read beyond allocated pktsz */
			if (pkt_desc->len > pktsz - ofs) {
				log_warnx("%s: descriptor len past pkt len",
				    __func__);
				chunk_size = pktsz - ofs;
			} else
				chunk_size = pkt_desc->len;

			/* Read packet from descriptor ring */
			if (read_mem(pkt_desc->addr, pkt + ofs, chunk_size)) {
				log_warnx("vionet: packet read_mem error "
				    "@ 0x%llx", pkt_desc->addr);
				goto out;
			}

			ofs += pkt_desc->len;
			pkt_desc_idx = pkt_desc->next & VIONET_QUEUE_MASK;
			pkt_desc = &desc[pkt_desc_idx];
		}

		/* Now handle tail descriptor - must be not writable */
		if (pkt_desc->flags & VRING_DESC_F_WRITE) {
			log_warnx("unexpected writable tx descriptor %d",
			    pkt_desc_idx);
			goto out;
		}

		/* Check we don't read beyond allocated pktsz */
		if (pkt_desc->len > pktsz - ofs) {
			log_warnx("%s: descriptor len past pkt len", __func__);
			chunk_size = pktsz - ofs - pkt_desc->len;
		} else
			chunk_size = pkt_desc->len;

		/* Read packet from descriptor ring */
		if (read_mem(pkt_desc->addr, pkt + ofs, chunk_size)) {
			log_warnx("vionet: packet read_mem error @ "
			    "0x%llx", pkt_desc->addr);
			goto out;
		}

		/* reject other source addresses */
		if (vionet->lockedmac && pktsz >= ETHER_HDR_LEN &&
		    (eh = (struct ether_header *)pkt) &&
		    memcmp(eh->ether_shost, vionet->mac,
		    sizeof(eh->ether_shost)) != 0)
			log_debug("vionet: wrong source address %s for vm %d",
			    ether_ntoa((struct ether_addr *)
			    eh->ether_shost), dev->vm_id);
		else if (vionet->local &&
		    (dhcpsz = dhcp_request(dev, pkt, pktsz, &dhcppkt)) != -1) {
			log_debug("vionet: dhcp request,"
			    " local response size %zd", dhcpsz);

		/* XXX signed vs unsigned here, funky cast */
		} else if (write(vionet->data_fd, pkt, pktsz) != (int)pktsz) {
			log_warnx("vionet: tx failed writing to tap: "
			    "%d", errno);
			goto out;
		}

	drop_packet:
		vionet->cfg.isr_status = 1;
		used->ring[used->idx & VIONET_QUEUE_MASK].id = hdr_desc_idx;
		used->ring[used->idx & VIONET_QUEUE_MASK].len = hdr_desc->len;
		__sync_synchronize();
		used->idx++;

		vq_info->last_avail = avail->idx & VIONET_QUEUE_MASK;
		idx = (idx + 1) & VIONET_QUEUE_MASK;

		num_enq++;

		free(pkt);
		pkt = NULL;
	}

	if (dhcpsz > 0)
		vionet_enq_rx(vionet, dhcppkt, dhcpsz, &spc);

out:
	free(pkt);
	free(dhcppkt);

	return (1);
}

static void
dev_dispatch_vm(int fd, short event, void *arg)
{
	struct virtio_dev	*dev = arg;
	struct vionet_dev	*vionet = &dev->vionet;
	struct imsgev		*iev = &dev->async_iev;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg	 	 imsg;
	ssize_t			 n = 0;
	int			 verbose;

	if (dev == NULL)
		fatalx("%s: missing vionet pointer", __func__);

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
			event_loopexit(NULL);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get", __func__);
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DEVOP_HOSTMAC:
			IMSG_SIZE_CHECK(&imsg, vionet->hostmac);
			memcpy(vionet->hostmac, imsg.data,
			    sizeof(vionet->hostmac));
			log_debug("%s: set hostmac", __func__);
			break;
		case IMSG_VMDOP_PAUSE_VM:
			log_debug("%s: pausing", __func__);
			event_del(&ev_tap);
			break;
		case IMSG_VMDOP_UNPAUSE_VM:
			log_debug("%s: unpausing", __func__);
			if (vionet->cfg.device_status
			    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK)
				event_add(&ev_tap, NULL);
			break;
		case IMSG_CTL_VERBOSE:
			IMSG_SIZE_CHECK(&imsg, &verbose);
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
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
				log_warnx("%s: failed to dump vionet device",
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
			event_del(&ev_tap);
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
	struct vionet_dev *vionet = &dev->vionet;
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
		vionet->cfg.guest_feature = data;
		break;
	case VIRTIO_CONFIG_QUEUE_PFN:
		vionet->cfg.queue_pfn = data;
		vionet_update_qa(vionet);
		break;
	case VIRTIO_CONFIG_QUEUE_SELECT:
		vionet->cfg.queue_select = data;
		vionet_update_qs(vionet);
		break;
	case VIRTIO_CONFIG_QUEUE_NOTIFY:
		vionet->cfg.queue_notify = data;
		if (vionet_notifyq(dev))
			intr = 1;
		break;
	case VIRTIO_CONFIG_DEVICE_STATUS:
		vionet->cfg.device_status = data;
		if (vionet->cfg.device_status == 0) {
			vionet->cfg.guest_feature = 0;

			vionet->cfg.queue_pfn = 0;
			vionet_update_qa(vionet);

			vionet->cfg.queue_size = 0;
			vionet_update_qs(vionet);

			vionet->cfg.queue_select = 0;
			vionet->cfg.queue_notify = 0;
			vionet->cfg.isr_status = 0;
			vionet->vq[RXQ].last_avail = 0;
			vionet->vq[RXQ].notified_avail = 0;
			vionet->vq[TXQ].last_avail = 0;
			vionet->vq[TXQ].notified_avail = 0;
			virtio_deassert_pic_irq(dev, msg->vcpu);
		}
		event_del(&ev_tap);
		if (vionet->cfg.device_status
		    & VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK) {
			if (event_add(&ev_tap, NULL))
				log_warn("%s: could not initialize virtio tap "
				    "event handler", __func__);
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
	struct vionet_dev *vionet = &dev->vionet;
	uint32_t data;

	switch (msg->reg) {
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 1:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 2:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
		data = vionet->mac[msg->reg -
		    VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI];
		break;
	case VIRTIO_CONFIG_DEVICE_FEATURES:
		data = vionet->cfg.device_feature;
		break;
	case VIRTIO_CONFIG_GUEST_FEATURES:
		data = vionet->cfg.guest_feature;
		break;
	case VIRTIO_CONFIG_QUEUE_PFN:
		data = vionet->cfg.queue_pfn;
		break;
	case VIRTIO_CONFIG_QUEUE_SIZE:
		data = vionet->cfg.queue_size;
		break;
	case VIRTIO_CONFIG_QUEUE_SELECT:
		data = vionet->cfg.queue_select;
		break;
	case VIRTIO_CONFIG_QUEUE_NOTIFY:
		data = vionet->cfg.queue_notify;
		break;
	case VIRTIO_CONFIG_DEVICE_STATUS:
		data = vionet->cfg.device_status;
		break;
	case VIRTIO_CONFIG_ISR_STATUS:
		data = vionet->cfg.isr_status;
		vionet->cfg.isr_status = 0;
		if (intr != NULL)
			*intr = INTR_STATE_DEASSERT;
		break;
	default:
		return (0xFFFFFFFF);
	}

	return (data);
}
