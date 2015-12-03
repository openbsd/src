/*	$OpenBSD: virtio.h,v 1.3 2015/12/03 08:42:11 reyk Exp $	*/

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

#include <dev/pci/virtioreg.h>

#define VIRTQUEUE_ALIGN(n)      (((n)+(VIRTIO_PAGE_SIZE-1))&    \
	~(VIRTIO_PAGE_SIZE-1))

/* Queue sizes must be power of two */
#define VIORND_QUEUE_SIZE	64
#define VIORND_QUEUE_MASK	(VIORND_QUEUE_SIZE - 1)

#define VIOBLK_QUEUE_SIZE	64
#define VIOBLK_QUEUE_MASK	(VIOBLK_QUEUE_SIZE - 1)

#define VIONET_QUEUE_SIZE	64
#define VIONET_QUEUE_MASK	(VIONET_QUEUE_SIZE - 1)

/* All the devices we support have either 1 or 2 queues */
#define VIRTIO_MAX_QUEUES	2

struct virtio_io_cfg {
	uint32_t device_feature;
	uint32_t guest_feature;
	uint32_t queue_address;
	uint16_t queue_size;
	uint16_t queue_select;
	uint16_t queue_notify;
	uint8_t device_status;
	uint8_t isr_status;
};

struct virtio_vq_info {
	uint32_t qa;
	uint32_t qs;
	uint32_t vq_availoffset;
	uint32_t vq_usedoffset;
	uint16_t last_avail;
};

struct viornd_dev {
	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];
};

struct vioblk_dev {
	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];

	int fd;
	uint64_t sz;
};

struct vionet_dev {
	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];

	int fd, rx_added;
	uint8_t mac[6];
};

struct virtio_net_hdr {
	uint8_t flags;
	uint8_t gso_type;
	uint16_t hdr_len;
	uint16_t gso_size;
	uint16_t csum_start;
	uint16_t csum_offset;

	/*
	 * num_buffers is only used if VIRTIO_NET_F_MRG_RXBUF is negotiated.
	 * vmd(8) doesn't negotiate that, but the field is listed here
	 * for completeness sake.
	 */
/*	uint16_t num_buffers; */
};


void virtio_init(struct vm_create_params *, int *, int *);
uint32_t vring_size(uint32_t);

int virtio_rnd_io(int, uint16_t, uint32_t *, uint8_t *, void *);
void viornd_update_qs(void);
void viornd_update_qa(void);
int viornd_notifyq(void);

int virtio_blk_io(int, uint16_t, uint32_t *, uint8_t *, void *);
void vioblk_update_qs(struct vioblk_dev *);
void vioblk_update_qa(struct vioblk_dev *);
int vioblk_notifyq(struct vioblk_dev *);

int virtio_net_io(int, uint16_t, uint32_t *, uint8_t *, void *);
void vionet_update_qs(struct vionet_dev *);
void vionet_update_qa(struct vionet_dev *);
int vionet_notifyq(struct vionet_dev *);
void vionet_notify_rx(struct vionet_dev *);
int vionet_process_rx(void);
int vionet_enq_rx(struct vionet_dev *, char *, ssize_t, int *);

const char *vioblk_cmd_name(uint32_t);
