/*	$OpenBSD: virtio.h,v 1.20 2017/08/12 20:24:57 mlarkin Exp $	*/

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

#include <dev/pv/virtioreg.h>

#define VIRTQUEUE_ALIGN(n)	(((n)+(VIRTIO_PAGE_SIZE-1))&    \
				    ~(VIRTIO_PAGE_SIZE-1))

/* Queue sizes must be power of two */
#define VIORND_QUEUE_SIZE	64
#define VIORND_QUEUE_MASK	(VIORND_QUEUE_SIZE - 1)

#define VIOBLK_QUEUE_SIZE	128
#define VIOBLK_QUEUE_MASK	(VIOBLK_QUEUE_SIZE - 1)

#define VIONET_QUEUE_SIZE	128
#define VIONET_QUEUE_MASK	(VIONET_QUEUE_SIZE - 1)

/* VMM Control Interface shutdown timeout (in seconds) */
#define VMMCI_TIMEOUT		3
#define VMMCI_SHUTDOWN_TIMEOUT	30

/* All the devices we support have either 1 or 2 queues */
#define VIRTIO_MAX_QUEUES	2

/*
 * This struct stores notifications from a virtio driver. There is
 * one such struct per virtio device.
 */
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

/*
 * A virtio device can have several virtqs. For example, vionet has one virtq
 * each for transmitting and receiving packets. This struct describes the state
 * of one virtq, such as their address in memory, size, offsets of rings, etc.
 * There is one virtio_vq_info per virtq.
 */
struct virtio_vq_info {
	/* Guest physical address of virtq */
	uint32_t qa;

	/* Queue size: number of queue entries in virtq */
	uint32_t qs;

	/*
	 * The offset of the 'available' ring within the virtq located at
	 * guest physical address qa above
	 */
	uint32_t vq_availoffset;

	/*
	 * The offset of the 'used' ring within the virtq located at guest
	 * physical address qa above
	 */
	uint32_t vq_usedoffset;

	/*
	 * The index into a slot of the 'available' ring that a virtio device
	 * can consume next
	 */
	uint16_t last_avail;

	/*
	 * The most recent index into the 'available' ring that a virtio
	 * driver notified to the host.
	 */
	uint16_t notified_avail;
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
	uint32_t max_xfer;
};

struct vionet_dev {
	pthread_mutex_t mutex;
	struct event event;

	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];

	int fd, rx_added;
	int rx_pending;
	uint32_t vm_id;
	uint32_t vm_vmid;
	int irq;
	uint8_t mac[6];

	int idx;
	int lockedmac;
	int local;
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

enum vmmci_cmd {
	VMMCI_NONE = 0,
	VMMCI_SHUTDOWN,
	VMMCI_REBOOT,
	VMMCI_SYNCRTC,
};

struct vmmci_dev {
	struct virtio_io_cfg cfg;
	struct event timeout;
	struct timeval time;
	enum vmmci_cmd cmd;
	uint32_t vm_id;
	int irq;
};

/* virtio.c */
void virtio_init(struct vmd_vm *, int *, int *);
int virtio_dump(int);
int virtio_restore(int, struct vmd_vm *, int *, int *);
uint32_t vring_size(uint32_t);

int virtio_rnd_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int viornd_dump(int);
int viornd_restore(int);
void viornd_update_qs(void);
void viornd_update_qa(void);
int viornd_notifyq(void);

int virtio_blk_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int vioblk_dump(int);
int vioblk_restore(int, struct vm_create_params *, int *);
void vioblk_update_qs(struct vioblk_dev *);
void vioblk_update_qa(struct vioblk_dev *);
int vioblk_notifyq(struct vioblk_dev *);

int virtio_net_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int vionet_dump(int);
int vionet_restore(int, struct vmd_vm *, int *);
void vionet_update_qs(struct vionet_dev *);
void vionet_update_qa(struct vionet_dev *);
int vionet_notifyq(struct vionet_dev *);
void vionet_notify_rx(struct vionet_dev *);
void vionet_process_rx(uint32_t);
int vionet_enq_rx(struct vionet_dev *, char *, ssize_t, int *);

int vmmci_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int vmmci_dump(int);
int vmmci_restore(int, uint32_t);
int vmmci_ctl(unsigned int);
void vmmci_ack(unsigned int);
void vmmci_timeout(int, short, void *);

const char *vioblk_cmd_name(uint32_t);

/* dhcp.c */
ssize_t dhcp_request(struct vionet_dev *, char *, size_t, char **);
