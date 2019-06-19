/*	$OpenBSD: virtio.h,v 1.34 2018/12/06 09:20:06 claudio Exp $	*/

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
#define ALIGNSZ(sz, align)	((sz + align - 1) & ~(align - 1))
#define MIN(a,b)		(((a)<(b))?(a):(b))

/* Queue sizes must be power of two */
#define VIORND_QUEUE_SIZE	64
#define VIORND_QUEUE_MASK	(VIORND_QUEUE_SIZE - 1)

#define VIOBLK_QUEUE_SIZE	128
#define VIOBLK_QUEUE_MASK	(VIOBLK_QUEUE_SIZE - 1)

#define VIOSCSI_QUEUE_SIZE	128
#define VIOSCSI_QUEUE_MASK	(VIOSCSI_QUEUE_SIZE - 1)

#define VIONET_QUEUE_SIZE	256
#define VIONET_QUEUE_MASK	(VIONET_QUEUE_SIZE - 1)

/* VMM Control Interface shutdown timeout (in seconds) */
#define VMMCI_TIMEOUT		3
#define VMMCI_SHUTDOWN_TIMEOUT	30

/* All the devices we support have either 1, 2 or 3 queues */
/* viornd - 1 queue
 * vioblk - 1 queue
 * vionet - 2 queues
 * vioscsi - 3 queues
 */
#define VIRTIO_MAX_QUEUES	3

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

struct virtio_backing {
	void  *p;
	ssize_t  (*pread)(void *p, char *buf, size_t len, off_t off);
	ssize_t  (*pwrite)(void *p, char *buf, size_t len, off_t off);
	void (*close)(void *p, int);
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

/*
 * Each virtio driver has a notifyq method where one or more messages
 * are ready to be processed on a given virtq.  As such, various
 * pieces of information are needed to provide ring accounting while
 * processing a given message such as virtq indexes, vring pointers, and
 * vring descriptors.
 */
struct virtio_vq_acct {

	/* index of previous avail vring message */
	uint16_t idx;

	/* index of current message containing the request */
	uint16_t req_idx;

	/* index of current message containing the response */
	uint16_t resp_idx;

	/* vring descriptor pointer */
	struct vring_desc *desc;

	/* vring descriptor pointer for request header and data */
	struct vring_desc *req_desc;

	/* vring descriptor pointer for response header and data */
	struct vring_desc *resp_desc;

	/* pointer to the available vring */
	struct vring_avail *avail;

	/* pointer to the used vring */
	struct vring_used *used;
};

struct viornd_dev {
	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];

	uint8_t pci_id;
	int irq;
	uint32_t vm_id;
};

struct vioblk_dev {
	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];
	struct virtio_backing file;

	uint64_t sz;
	uint32_t max_xfer;

	uint8_t pci_id;
	int irq;
	uint32_t vm_id;
};

/* vioscsi will use at least 3 queues - 5.6.2 Virtqueues
 * Current implementation will use 3
 * 0 - control
 * 1 - event
 * 2 - requests
 */
struct vioscsi_dev {
	struct virtio_io_cfg cfg;

	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];

	struct virtio_backing file;

	/* is the device locked */
	int locked;
	/* size of iso file in bytes */
	uint64_t sz;
	/* last block address read */
	uint64_t lba;
	/* number of blocks represented in iso */
	uint64_t n_blocks;
	uint32_t max_xfer;

	uint8_t pci_id;
	uint32_t vm_id;
	int irq;
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
	int pxeboot;

	uint8_t pci_id;
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

	uint8_t pci_id;
};

struct ioinfo {
	struct virtio_backing *file;
	uint8_t *buf;
	ssize_t len;
	off_t offset;
	int error;
};

/* virtio.c */
void virtio_init(struct vmd_vm *, int, int[][VM_MAX_BASE_PER_DISK], int *);
void virtio_shutdown(struct vmd_vm *);
int virtio_dump(int);
int virtio_restore(int, struct vmd_vm *, int,
    int[][VM_MAX_BASE_PER_DISK], int *);
uint32_t vring_size(uint32_t);

int virtio_rnd_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int viornd_dump(int);
int viornd_restore(int, struct vm_create_params *);
void viornd_update_qs(void);
void viornd_update_qa(void);
int viornd_notifyq(void);

ssize_t virtio_qcow2_get_base(int, char *, size_t, const char *);
int virtio_qcow2_create(const char *, const char *, long);
int virtio_qcow2_init(struct virtio_backing *, off_t *, int*, size_t);
int virtio_raw_create(const char *, long);
int virtio_raw_init(struct virtio_backing *, off_t *, int*, size_t);

int virtio_blk_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int vioblk_dump(int);
int vioblk_restore(int, struct vmop_create_params *,
    int[][VM_MAX_BASE_PER_DISK]);
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
int vionet_notify_tx(struct vionet_dev *);
void vionet_process_rx(uint32_t);
int vionet_enq_rx(struct vionet_dev *, char *, ssize_t, int *);

int vmmci_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int vmmci_dump(int);
int vmmci_restore(int, uint32_t);
int vmmci_ctl(unsigned int);
void vmmci_ack(unsigned int);
void vmmci_timeout(int, short, void *);

const char *vioblk_cmd_name(uint32_t);
int vioscsi_dump(int);
int vioscsi_restore(int, struct vm_create_params *, int);

/* dhcp.c */
ssize_t dhcp_request(struct vionet_dev *, char *, size_t, char **);

/* vioscsi.c */
int vioscsi_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
void vioscsi_update_qs(struct vioscsi_dev *);
void vioscsi_update_qa(struct vioscsi_dev *);
int vioscsi_notifyq(struct vioscsi_dev *);
