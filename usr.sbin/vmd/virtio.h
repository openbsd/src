/*	$OpenBSD: virtio.h,v 1.49 2023/09/26 01:53:54 dv Exp $	*/

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

#include <sys/types.h>

#include <dev/pv/virtioreg.h>
#include <net/if_tun.h>

#include <event.h>

#include "vmd.h"

#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#define VIRTQUEUE_ALIGN(n)	(((n)+(VIRTIO_PAGE_SIZE-1))&    \
				    ~(VIRTIO_PAGE_SIZE-1))
#define ALIGNSZ(sz, align)	((sz + align - 1) & ~(align - 1))
#define MIN(a,b)		(((a)<(b))?(a):(b))

/* Queue sizes must be power of two and less than IOV_MAX (1024). */
#define VIORND_QUEUE_SIZE	64
#define VIORND_QUEUE_MASK	(VIORND_QUEUE_SIZE - 1)

#define VIOBLK_QUEUE_SIZE	128
#define VIOBLK_QUEUE_MASK	(VIOBLK_QUEUE_SIZE - 1)
#define VIOBLK_SEG_MAX		(VIOBLK_QUEUE_SIZE - 2)

#define VIOSCSI_QUEUE_SIZE	128
#define VIOSCSI_QUEUE_MASK	(VIOSCSI_QUEUE_SIZE - 1)

#define VIONET_QUEUE_SIZE	256
#define VIONET_QUEUE_MASK	(VIONET_QUEUE_SIZE - 1)

/* Virtio network device is backed by tap(4), so inherit limits */
#define VIONET_HARD_MTU		TUNMRU
#define VIONET_MIN_TXLEN	ETHER_HDR_LEN
#define VIONET_MAX_TXLEN	VIONET_HARD_MTU + ETHER_HDR_LEN

/* VMM Control Interface shutdown timeout (in seconds) */
#define VMMCI_TIMEOUT		3
#define VMMCI_SHUTDOWN_TIMEOUT	120

/* All the devices we support have either 1, 2 or 3 queues */
/* viornd - 1 queue
 * vioblk - 1 queue
 * vionet - 2 queues
 * vioscsi - 3 queues
 */
#define VIRTIO_MAX_QUEUES	3

#define MAXPHYS	(64 * 1024)	/* max raw I/O transfer size */

/*
 * Rename the address config register to be more descriptive.
 */
#define VIRTIO_CONFIG_QUEUE_PFN	VIRTIO_CONFIG_QUEUE_ADDRESS
#define DEVICE_NEEDS_RESET	VIRTIO_CONFIG_DEVICE_STATUS_DEVICE_NEEDS_RESET
#define DESC_WRITABLE(/* struct vring_desc */ x)	\
	(((x)->flags & VRING_DESC_F_WRITE) ? 1 : 0)


/*
 * VM <-> Device messaging.
 */
struct viodev_msg {
	uint8_t type;
#define VIODEV_MSG_INVALID	0
#define VIODEV_MSG_READY	1
#define VIODEV_MSG_ERROR	2
#define VIODEV_MSG_KICK		3
#define VIODEV_MSG_IO_READ	4
#define VIODEV_MSG_IO_WRITE	5
#define VIODEV_MSG_DUMP		6
#define VIODEV_MSG_SHUTDOWN	7

	uint16_t reg;		/* VirtIO register */
	uint8_t io_sz;		/* IO instruction size */
	uint8_t vcpu;		/* VCPU id */
	uint8_t irq;		/* IRQ number */

	int8_t state;		/* Interrupt state toggle (if any) */
#define INTR_STATE_ASSERT	 1
#define INTR_STATE_NOOP		 0
#define INTR_STATE_DEASSERT	-1

	uint32_t data;		/* Data (if any) */
	uint8_t data_valid;	/* 1 if data field is populated. */
} __packed;

/*
 * This struct stores notifications from a virtio driver. There is
 * one such struct per virtio device.
 */
struct virtio_io_cfg {
	uint32_t device_feature;
	uint32_t guest_feature;
	uint32_t queue_pfn;
	uint16_t queue_size;
	uint16_t queue_select;
	uint16_t queue_notify;
	uint8_t device_status;
	uint8_t isr_status;
};

struct virtio_backing {
	void  *p;
	ssize_t (*pread)(void *, char *, size_t, off_t);
	ssize_t (*preadv)(void *, struct iovec *, int, off_t);
	ssize_t (*pwrite)(void *, char *, size_t, off_t);
	ssize_t (*pwritev)(void *, struct iovec *, int, off_t);
	void (*close)(void *, int);
};

/*
 * A virtio device can have several virtqs. For example, vionet has one virtq
 * each for transmitting and receiving packets. This struct describes the state
 * of one virtq, such as their address in memory, size, offsets of rings, etc.
 * There is one virtio_vq_info per virtq.
 */
struct virtio_vq_info {
	/* Guest physical address of virtq */
	uint64_t q_gpa;

	/* Host virtual address of virtq */
	void *q_hva;

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

	int disk_fd[VM_MAX_BASE_PER_DISK];	/* fds for disk image(s) */
	uint8_t ndisk_fd;	/* number of valid disk fds */
	uint64_t capacity;	/* size in 512 byte sectors */
	uint32_t seg_max;	/* maximum number of segments */

	unsigned int idx;
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
	struct virtio_io_cfg cfg;
	struct virtio_vq_info vq[VIRTIO_MAX_QUEUES];

	int data_fd;		/* fd for our tap device */

	uint8_t mac[6];
	uint8_t hostmac[6];
	int lockedmac;
	int local;
	int pxeboot;
	struct local_prefix local_prefix;

	unsigned int idx;
};

struct virtio_dev {
	union {
		struct vioblk_dev vioblk;
		struct vionet_dev vionet;
	};

	struct imsgev async_iev;
	struct imsgev sync_iev;

	int sync_fd;		/* fd for synchronous channel */
	int async_fd;		/* fd for async channel */

	uint8_t pci_id;
	uint32_t vm_id;
	uint32_t vm_vmid;
	int irq;

	pid_t dev_pid;
	char dev_type;
	SLIST_ENTRY(virtio_dev) dev_next;
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

/* XXX to be removed once vioscsi is adapted to vectorized io. */
struct ioinfo {
	uint8_t *buf;
	ssize_t len;
	off_t offset;
};

/* virtio.c */
void virtio_init(struct vmd_vm *, int, int[][VM_MAX_BASE_PER_DISK], int *);
void virtio_broadcast_imsg(struct vmd_vm *, uint16_t, void *, uint16_t);
void virtio_stop(struct vmd_vm *);
void virtio_start(struct vmd_vm *);
void virtio_shutdown(struct vmd_vm *);
int virtio_dump(int);
int virtio_restore(int, struct vmd_vm *, int, int[][VM_MAX_BASE_PER_DISK],
    int *);
const char *virtio_reg_name(uint8_t);
uint32_t vring_size(uint32_t);
int vm_device_pipe(struct virtio_dev *, void (*)(int, short, void *));
int virtio_pci_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
void virtio_assert_pic_irq(struct virtio_dev *, int);
void virtio_deassert_pic_irq(struct virtio_dev *, int);

int virtio_rnd_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int viornd_dump(int);
int viornd_restore(int, struct vmd_vm *);
void viornd_update_qs(void);
void viornd_update_qa(void);
int viornd_notifyq(void);

ssize_t virtio_qcow2_get_base(int, char *, size_t, const char *);
int virtio_qcow2_create(const char *, const char *, uint64_t);
int virtio_qcow2_init(struct virtio_backing *, off_t *, int*, size_t);
int virtio_raw_create(const char *, uint64_t);
int virtio_raw_init(struct virtio_backing *, off_t *, int*, size_t);

int vioblk_dump(int);
int vioblk_restore(int, struct vmd_vm *, int[][VM_MAX_BASE_PER_DISK]);

int vionet_dump(int);
int vionet_restore(int, struct vmd_vm *, int *);
void vionet_update_qs(struct vionet_dev *);
void vionet_update_qa(struct vionet_dev *);
int vionet_notifyq(struct virtio_dev *);
void vionet_notify_rx(struct virtio_dev *);
int vionet_notify_tx(struct virtio_dev *);
void vionet_process_rx(uint32_t);
int vionet_enq_rx(struct vionet_dev *, char *, size_t, int *);
void vionet_set_hostmac(struct vmd_vm *, unsigned int, uint8_t *);

int vmmci_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
int vmmci_dump(int);
int vmmci_restore(int, uint32_t);
int vmmci_ctl(unsigned int);
void vmmci_ack(unsigned int);
void vmmci_timeout(int, short, void *);

const char *vioblk_cmd_name(uint32_t);
int vioscsi_dump(int);
int vioscsi_restore(int, struct vmd_vm *, int);

/* dhcp.c */
ssize_t dhcp_request(struct virtio_dev *, char *, size_t, char **);

/* vioscsi.c */
int vioscsi_io(int, uint16_t, uint32_t *, uint8_t *, void *, uint8_t);
void vioscsi_update_qs(struct vioscsi_dev *);
void vioscsi_update_qa(struct vioscsi_dev *);
int vioscsi_notifyq(struct vioscsi_dev *);

#endif /* _VIRTIO_H_ */
