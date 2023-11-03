/*	$OpenBSD: virtio.c,v 1.110 2023/11/03 11:16:43 dv Exp $	*/

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

#include <sys/param.h>	/* PAGE_SIZE */
#include <sys/socket.h>
#include <sys/wait.h>

#include <machine/vmmvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pv/virtioreg.h>
#include <dev/pci/virtio_pcireg.h>
#include <dev/pv/vioblkreg.h>
#include <dev/pv/vioscsireg.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "pci.h"
#include "vioscsi.h"
#include "virtio.h"
#include "vmd.h"
#include "vmm.h"

extern struct vmd *env;
extern char *__progname;

struct viornd_dev viornd;
struct vioscsi_dev *vioscsi;
struct vmmci_dev vmmci;

/* Devices emulated in subprocesses are inserted into this list. */
SLIST_HEAD(virtio_dev_head, virtio_dev) virtio_devs;

#define MAXPHYS	(64 * 1024)	/* max raw I/O transfer size */

#define VIRTIO_NET_F_MAC	(1<<5)

#define VMMCI_F_TIMESYNC	(1<<0)
#define VMMCI_F_ACK		(1<<1)
#define VMMCI_F_SYNCRTC		(1<<2)

#define RXQ	0
#define TXQ	1

static int virtio_dev_launch(struct vmd_vm *, struct virtio_dev *);
static void virtio_dispatch_dev(int, short, void *);
static int handle_dev_msg(struct viodev_msg *, struct virtio_dev *);

const char *
virtio_reg_name(uint8_t reg)
{
	switch (reg) {
	case VIRTIO_CONFIG_DEVICE_FEATURES: return "device feature";
	case VIRTIO_CONFIG_GUEST_FEATURES: return "guest feature";
	case VIRTIO_CONFIG_QUEUE_PFN: return "queue address";
	case VIRTIO_CONFIG_QUEUE_SIZE: return "queue size";
	case VIRTIO_CONFIG_QUEUE_SELECT: return "queue select";
	case VIRTIO_CONFIG_QUEUE_NOTIFY: return "queue notify";
	case VIRTIO_CONFIG_DEVICE_STATUS: return "device status";
	case VIRTIO_CONFIG_ISR_STATUS: return "isr status";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI...VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 3:
		return "device config 0";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 5:
		return "device config 1";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8: return "device config 2";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 12: return "device config 3";
	case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 16: return "device config 4";
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
	struct virtio_vq_info *vq_info;

	/* Invalid queue? */
	if (viornd.cfg.queue_select > 0) {
		viornd.cfg.queue_size = 0;
		return;
	}

	vq_info = &viornd.vq[viornd.cfg.queue_select];

	/* Update queue pfn/size based on queue select */
	viornd.cfg.queue_pfn = vq_info->q_gpa >> 12;
	viornd.cfg.queue_size = vq_info->qs;
}

/* Update queue address */
void
viornd_update_qa(void)
{
	struct virtio_vq_info *vq_info;
	void *hva = NULL;

	/* Invalid queue? */
	if (viornd.cfg.queue_select > 0)
		return;

	vq_info = &viornd.vq[viornd.cfg.queue_select];
	vq_info->q_gpa = (uint64_t)viornd.cfg.queue_pfn * VIRTIO_PAGE_SIZE;

	hva = hvaddr_mem(vq_info->q_gpa, vring_size(VIORND_QUEUE_SIZE));
	if (hva == NULL)
		fatalx("viornd_update_qa");
	vq_info->q_hva = hva;
}

int
viornd_notifyq(void)
{
	size_t sz;
	int dxx, ret;
	uint16_t aidx, uidx;
	char *vr, *rnd_data;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;
	struct virtio_vq_info *vq_info;

	ret = 0;

	/* Invalid queue? */
	if (viornd.cfg.queue_notify > 0)
		return (0);

	vq_info = &viornd.vq[viornd.cfg.queue_notify];
	vr = vq_info->q_hva;
	if (vr == NULL)
		fatalx("%s: null vring", __func__);

	desc = (struct vring_desc *)(vr);
	avail = (struct vring_avail *)(vr + vq_info->vq_availoffset);
	used = (struct vring_used *)(vr + vq_info->vq_usedoffset);

	aidx = avail->idx & VIORND_QUEUE_MASK;
	uidx = used->idx & VIORND_QUEUE_MASK;

	dxx = avail->ring[aidx] & VIORND_QUEUE_MASK;

	sz = desc[dxx].len;
	if (sz > MAXPHYS)
		fatalx("viornd descriptor size too large (%zu)", sz);

	rnd_data = malloc(sz);

	if (rnd_data != NULL) {
		arc4random_buf(rnd_data, sz);
		if (write_mem(desc[dxx].addr, rnd_data, sz)) {
			log_warnx("viornd: can't write random data @ "
			    "0x%llx",
			    desc[dxx].addr);
		} else {
			/* ret == 1 -> interrupt needed */
			/* XXX check VIRTIO_F_NO_INTR */
			ret = 1;
			viornd.cfg.isr_status = 1;
			used->ring[uidx].id = dxx;
			used->ring[uidx].len = sz;
			__sync_synchronize();
			used->idx++;
		}
		free(rnd_data);
	} else
		fatal("memory allocation error for viornd data");

	return (ret);
}

int
virtio_rnd_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *unused, uint8_t sz)
{
	*intr = 0xFF;

	if (dir == 0) {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			log_warnx("%s: illegal write %x to %s",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			viornd.cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_PFN:
			viornd.cfg.queue_pfn = *data;
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
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = viornd.cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = viornd.cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_PFN:
			*data = viornd.cfg.queue_pfn;
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
			viornd.cfg.isr_status = 0;
			vcpu_deassert_pic_irq(viornd.vm_id, 0, viornd.irq);
			break;
		}
	}
	return (0);
}

int
vmmci_ctl(unsigned int cmd)
{
	struct timeval tv = { 0, 0 };

	if ((vmmci.cfg.device_status &
	    VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK) == 0)
		return (-1);

	if (cmd == vmmci.cmd)
		return (0);

	switch (cmd) {
	case VMMCI_NONE:
		break;
	case VMMCI_SHUTDOWN:
	case VMMCI_REBOOT:
		/* Update command */
		vmmci.cmd = cmd;

		/*
		 * vmm VMs do not support powerdown, send a reboot request
		 * instead and turn it off after the triple fault.
		 */
		if (cmd == VMMCI_SHUTDOWN)
			cmd = VMMCI_REBOOT;

		/* Trigger interrupt */
		vmmci.cfg.isr_status = VIRTIO_CONFIG_ISR_CONFIG_CHANGE;
		vcpu_assert_pic_irq(vmmci.vm_id, 0, vmmci.irq);

		/* Add ACK timeout */
		tv.tv_sec = VMMCI_TIMEOUT;
		evtimer_add(&vmmci.timeout, &tv);
		break;
	case VMMCI_SYNCRTC:
		if (vmmci.cfg.guest_feature & VMMCI_F_SYNCRTC) {
			/* RTC updated, request guest VM resync of its RTC */
			vmmci.cmd = cmd;

			vmmci.cfg.isr_status = VIRTIO_CONFIG_ISR_CONFIG_CHANGE;
			vcpu_assert_pic_irq(vmmci.vm_id, 0, vmmci.irq);
		} else {
			log_debug("%s: RTC sync skipped (guest does not "
			    "support RTC sync)\n", __func__);
		}
		break;
	default:
		fatalx("invalid vmmci command: %d", cmd);
	}

	return (0);
}

void
vmmci_ack(unsigned int cmd)
{
	struct timeval	 tv = { 0, 0 };

	switch (cmd) {
	case VMMCI_NONE:
		break;
	case VMMCI_SHUTDOWN:
		/*
		 * The shutdown was requested by the VM if we don't have
		 * a pending shutdown request.  In this case add a short
		 * timeout to give the VM a chance to reboot before the
		 * timer is expired.
		 */
		if (vmmci.cmd == 0) {
			log_debug("%s: vm %u requested shutdown", __func__,
			    vmmci.vm_id);
			tv.tv_sec = VMMCI_TIMEOUT;
			evtimer_add(&vmmci.timeout, &tv);
			return;
		}
		/* FALLTHROUGH */
	case VMMCI_REBOOT:
		/*
		 * If the VM acknowledged our shutdown request, give it
		 * enough time to shutdown or reboot gracefully.  This
		 * might take a considerable amount of time (running
		 * rc.shutdown on the VM), so increase the timeout before
		 * killing it forcefully.
		 */
		if (cmd == vmmci.cmd &&
		    evtimer_pending(&vmmci.timeout, NULL)) {
			log_debug("%s: vm %u acknowledged shutdown request",
			    __func__, vmmci.vm_id);
			tv.tv_sec = VMMCI_SHUTDOWN_TIMEOUT;
			evtimer_add(&vmmci.timeout, &tv);
		}
		break;
	case VMMCI_SYNCRTC:
		log_debug("%s: vm %u acknowledged RTC sync request",
		    __func__, vmmci.vm_id);
		vmmci.cmd = VMMCI_NONE;
		break;
	default:
		log_warnx("%s: illegal request %u", __func__, cmd);
		break;
	}
}

void
vmmci_timeout(int fd, short type, void *arg)
{
	log_debug("%s: vm %u shutdown", __progname, vmmci.vm_id);
	vm_shutdown(vmmci.cmd == VMMCI_REBOOT ? VMMCI_REBOOT : VMMCI_SHUTDOWN);
}

int
vmmci_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *unused, uint8_t sz)
{
	*intr = 0xFF;

	if (dir == 0) {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_FEATURES:
		case VIRTIO_CONFIG_QUEUE_SIZE:
		case VIRTIO_CONFIG_ISR_STATUS:
			log_warnx("%s: illegal write %x to %s",
			    __progname, *data, virtio_reg_name(reg));
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			vmmci.cfg.guest_feature = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_PFN:
			vmmci.cfg.queue_pfn = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			vmmci.cfg.queue_select = *data;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			vmmci.cfg.queue_notify = *data;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			vmmci.cfg.device_status = *data;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
			vmmci_ack(*data);
			break;
		}
	} else {
		switch (reg) {
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI:
			*data = vmmci.cmd;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 4:
			/* Update time once when reading the first register */
			gettimeofday(&vmmci.time, NULL);
			*data = (uint64_t)vmmci.time.tv_sec;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 8:
			*data = (uint64_t)vmmci.time.tv_sec << 32;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 12:
			*data = (uint64_t)vmmci.time.tv_usec;
			break;
		case VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI + 16:
			*data = (uint64_t)vmmci.time.tv_usec << 32;
			break;
		case VIRTIO_CONFIG_DEVICE_FEATURES:
			*data = vmmci.cfg.device_feature;
			break;
		case VIRTIO_CONFIG_GUEST_FEATURES:
			*data = vmmci.cfg.guest_feature;
			break;
		case VIRTIO_CONFIG_QUEUE_PFN:
			*data = vmmci.cfg.queue_pfn;
			break;
		case VIRTIO_CONFIG_QUEUE_SIZE:
			*data = vmmci.cfg.queue_size;
			break;
		case VIRTIO_CONFIG_QUEUE_SELECT:
			*data = vmmci.cfg.queue_select;
			break;
		case VIRTIO_CONFIG_QUEUE_NOTIFY:
			*data = vmmci.cfg.queue_notify;
			break;
		case VIRTIO_CONFIG_DEVICE_STATUS:
			*data = vmmci.cfg.device_status;
			break;
		case VIRTIO_CONFIG_ISR_STATUS:
			*data = vmmci.cfg.isr_status;
			vmmci.cfg.isr_status = 0;
			vcpu_deassert_pic_irq(vmmci.vm_id, 0, vmmci.irq);
			break;
		}
	}
	return (0);
}

int
virtio_get_base(int fd, char *path, size_t npath, int type, const char *dpath)
{
	switch (type) {
	case VMDF_RAW:
		return 0;
	case VMDF_QCOW2:
		return virtio_qcow2_get_base(fd, path, npath, dpath);
	}
	log_warnx("%s: invalid disk format", __func__);
	return -1;
}

void
virtio_init(struct vmd_vm *vm, int child_cdrom,
    int child_disks[][VM_MAX_BASE_PER_DISK], int *child_taps)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	struct vm_create_params *vcp = &vmc->vmc_params;
	struct virtio_dev *dev;
	uint8_t id;
	uint8_t i, j;

	/* Virtio entropy device */
	if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
	    PCI_PRODUCT_QUMRANET_VIO_RNG, PCI_CLASS_SYSTEM,
	    PCI_SUBCLASS_SYSTEM_MISC,
	    PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_VIRTIO_ENTROPY, 1, NULL)) {
		log_warnx("%s: can't add PCI virtio rng device",
		    __progname);
		return;
	}

	if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_rnd_io, NULL)) {
		log_warnx("%s: can't add bar for virtio rng device",
		    __progname);
		return;
	}

	memset(&viornd, 0, sizeof(viornd));
	viornd.vq[0].qs = VIORND_QUEUE_SIZE;
	viornd.vq[0].vq_availoffset = sizeof(struct vring_desc) *
	    VIORND_QUEUE_SIZE;
	viornd.vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
	    sizeof(struct vring_desc) * VIORND_QUEUE_SIZE
	    + sizeof(uint16_t) * (2 + VIORND_QUEUE_SIZE));
	viornd.pci_id = id;
	viornd.irq = pci_get_dev_irq(id);
	viornd.vm_id = vcp->vcp_id;

	SLIST_INIT(&virtio_devs);

	if (vmc->vmc_nnics > 0) {
		for (i = 0; i < vmc->vmc_nnics; i++) {
			dev = calloc(1, sizeof(struct virtio_dev));
			if (dev == NULL) {
				log_warn("%s: calloc failure allocating vionet",
				    __progname);
				return;
			}
			/* Virtio network */
			dev->dev_type = VMD_DEVTYPE_NET;

			if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
				PCI_PRODUCT_QUMRANET_VIO_NET, PCI_CLASS_SYSTEM,
				PCI_SUBCLASS_SYSTEM_MISC, PCI_VENDOR_OPENBSD,
				PCI_PRODUCT_VIRTIO_NETWORK, 1, NULL)) {
				log_warnx("%s: can't add PCI virtio net device",
				    __progname);
				return;
			}
			dev->pci_id = id;
			dev->sync_fd = -1;
			dev->async_fd = -1;
			dev->vm_id = vcp->vcp_id;
			dev->vm_vmid = vm->vm_vmid;
			dev->irq = pci_get_dev_irq(id);

			/* The vionet pci bar function is called by the vcpu. */
			if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_pci_io,
			    dev)) {
				log_warnx("%s: can't add bar for virtio net "
				    "device", __progname);
				return;
			}

			dev->vionet.vq[RXQ].qs = VIONET_QUEUE_SIZE;
			dev->vionet.vq[RXQ].vq_availoffset =
			    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE;
			dev->vionet.vq[RXQ].vq_usedoffset = VIRTQUEUE_ALIGN(
				sizeof(struct vring_desc) * VIONET_QUEUE_SIZE
				+ sizeof(uint16_t) * (2 + VIONET_QUEUE_SIZE));
			dev->vionet.vq[RXQ].last_avail = 0;
			dev->vionet.vq[RXQ].notified_avail = 0;

			dev->vionet.vq[TXQ].qs = VIONET_QUEUE_SIZE;
			dev->vionet.vq[TXQ].vq_availoffset =
			    sizeof(struct vring_desc) * VIONET_QUEUE_SIZE;
			dev->vionet.vq[TXQ].vq_usedoffset = VIRTQUEUE_ALIGN(
				sizeof(struct vring_desc) * VIONET_QUEUE_SIZE
				+ sizeof(uint16_t) * (2 + VIONET_QUEUE_SIZE));
			dev->vionet.vq[TXQ].last_avail = 0;
			dev->vionet.vq[TXQ].notified_avail = 0;

			dev->vionet.data_fd = child_taps[i];

			/* MAC address has been assigned by the parent */
			memcpy(&dev->vionet.mac, &vmc->vmc_macs[i], 6);
			dev->vionet.cfg.device_feature = VIRTIO_NET_F_MAC;

			dev->vionet.lockedmac =
			    vmc->vmc_ifflags[i] & VMIFF_LOCKED ? 1 : 0;
			dev->vionet.local =
			    vmc->vmc_ifflags[i] & VMIFF_LOCAL ? 1 : 0;
			if (i == 0 && vmc->vmc_bootdevice & VMBOOTDEV_NET)
				dev->vionet.pxeboot = 1;
			memcpy(&dev->vionet.local_prefix,
			    &env->vmd_cfg.cfg_localprefix,
			    sizeof(dev->vionet.local_prefix));
			log_debug("%s: vm \"%s\" vio%u lladdr %s%s%s%s",
			    __func__, vcp->vcp_name, i,
			    ether_ntoa((void *)dev->vionet.mac),
			    dev->vionet.lockedmac ? ", locked" : "",
			    dev->vionet.local ? ", local" : "",
			    dev->vionet.pxeboot ? ", pxeboot" : "");

			/* Add the vionet to our device list. */
			dev->vionet.idx = i;
			SLIST_INSERT_HEAD(&virtio_devs, dev, dev_next);
		}
	}

	if (vmc->vmc_ndisks > 0) {
		for (i = 0; i < vmc->vmc_ndisks; i++) {
			dev = calloc(1, sizeof(struct virtio_dev));
			if (dev == NULL) {
				log_warn("%s: calloc failure allocating vioblk",
				    __progname);
				return;
			}

			/* One vioblk device for each disk defined in vcp */
			dev->dev_type = VMD_DEVTYPE_DISK;

			if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
			    PCI_PRODUCT_QUMRANET_VIO_BLOCK,
			    PCI_CLASS_MASS_STORAGE,
			    PCI_SUBCLASS_MASS_STORAGE_SCSI,
			    PCI_VENDOR_OPENBSD,
			    PCI_PRODUCT_VIRTIO_BLOCK, 1, NULL)) {
				log_warnx("%s: can't add PCI virtio block "
				    "device", __progname);
				return;
			}
			dev->pci_id = id;
			dev->sync_fd = -1;
			dev->async_fd = -1;
			dev->vm_id = vcp->vcp_id;
			dev->vm_vmid = vm->vm_vmid;
			dev->irq = pci_get_dev_irq(id);

			if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, virtio_pci_io,
			    &dev->vioblk)) {
				log_warnx("%s: can't add bar for virtio block "
				    "device", __progname);
				return;
			}
			dev->vioblk.vq[0].qs = VIOBLK_QUEUE_SIZE;
			dev->vioblk.vq[0].vq_availoffset =
			    sizeof(struct vring_desc) * VIOBLK_QUEUE_SIZE;
			dev->vioblk.vq[0].vq_usedoffset = VIRTQUEUE_ALIGN(
			    sizeof(struct vring_desc) * VIOBLK_QUEUE_SIZE
			    + sizeof(uint16_t) * (2 + VIOBLK_QUEUE_SIZE));
			dev->vioblk.vq[0].last_avail = 0;
			dev->vioblk.cfg.device_feature =
			    VIRTIO_BLK_F_SEG_MAX;
			dev->vioblk.seg_max = VIOBLK_SEG_MAX;

			/*
			 * Initialize disk fds to an invalid fd (-1), then
			 * set any child disk fds.
			 */
			memset(&dev->vioblk.disk_fd, -1,
			    sizeof(dev->vioblk.disk_fd));
			dev->vioblk.ndisk_fd = vmc->vmc_diskbases[i];
			for (j = 0; j < dev->vioblk.ndisk_fd; j++)
				dev->vioblk.disk_fd[j] = child_disks[i][j];

			dev->vioblk.idx = i;
			SLIST_INSERT_HEAD(&virtio_devs, dev, dev_next);
		}
	}

	/*
	 * Launch virtio devices that support subprocess execution.
	 */
	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		if (virtio_dev_launch(vm, dev) != 0)
			fatalx("failed to launch virtio device");
	}

	/* vioscsi cdrom */
	if (strlen(vmc->vmc_cdrom)) {
		vioscsi = calloc(1, sizeof(struct vioscsi_dev));
		if (vioscsi == NULL) {
			log_warn("%s: calloc failure allocating vioscsi",
			    __progname);
			return;
		}

		if (pci_add_device(&id, PCI_VENDOR_QUMRANET,
		    PCI_PRODUCT_QUMRANET_VIO_SCSI,
		    PCI_CLASS_MASS_STORAGE,
		    PCI_SUBCLASS_MASS_STORAGE_SCSI,
		    PCI_VENDOR_OPENBSD,
		    PCI_PRODUCT_VIRTIO_SCSI, 1, NULL)) {
			log_warnx("%s: can't add PCI vioscsi device",
			    __progname);
			return;
		}

		if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, vioscsi_io, vioscsi)) {
			log_warnx("%s: can't add bar for vioscsi device",
			    __progname);
			return;
		}

		for (i = 0; i < VIRTIO_MAX_QUEUES; i++) {
			vioscsi->vq[i].qs = VIOSCSI_QUEUE_SIZE;
			vioscsi->vq[i].vq_availoffset =
			    sizeof(struct vring_desc) * VIOSCSI_QUEUE_SIZE;
			vioscsi->vq[i].vq_usedoffset = VIRTQUEUE_ALIGN(
			    sizeof(struct vring_desc) * VIOSCSI_QUEUE_SIZE
			    + sizeof(uint16_t) * (2 + VIOSCSI_QUEUE_SIZE));
			vioscsi->vq[i].last_avail = 0;
		}
		if (virtio_raw_init(&vioscsi->file, &vioscsi->sz, &child_cdrom,
		    1) == -1) {
			log_warnx("%s: unable to determine iso format",
			    __func__);
			return;
		}
		vioscsi->locked = 0;
		vioscsi->lba = 0;
		vioscsi->n_blocks = vioscsi->sz / VIOSCSI_BLOCK_SIZE_CDROM;
		vioscsi->max_xfer = VIOSCSI_BLOCK_SIZE_CDROM;
		vioscsi->pci_id = id;
		vioscsi->vm_id = vcp->vcp_id;
		vioscsi->irq = pci_get_dev_irq(id);
	}

	/* virtio control device */
	if (pci_add_device(&id, PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_OPENBSD_CONTROL,
	    PCI_CLASS_COMMUNICATIONS,
	    PCI_SUBCLASS_COMMUNICATIONS_MISC,
	    PCI_VENDOR_OPENBSD,
	    PCI_PRODUCT_VIRTIO_VMMCI, 1, NULL)) {
		log_warnx("%s: can't add PCI vmm control device",
		    __progname);
		return;
	}

	if (pci_add_bar(id, PCI_MAPREG_TYPE_IO, vmmci_io, NULL)) {
		log_warnx("%s: can't add bar for vmm control device",
		    __progname);
		return;
	}

	memset(&vmmci, 0, sizeof(vmmci));
	vmmci.cfg.device_feature = VMMCI_F_TIMESYNC | VMMCI_F_ACK |
	    VMMCI_F_SYNCRTC;
	vmmci.vm_id = vcp->vcp_id;
	vmmci.irq = pci_get_dev_irq(id);
	vmmci.pci_id = id;

	evtimer_set(&vmmci.timeout, vmmci_timeout, NULL);
}

/*
 * vionet_set_hostmac
 *
 * Sets the hardware address for the host-side tap(4) on a vionet_dev.
 *
 * This should only be called from the event-loop thread
 *
 * vm: pointer to the current vmd_vm instance
 * idx: index into the array of vionet_dev's for the target vionet_dev
 * addr: ethernet address to set
 */
void
vionet_set_hostmac(struct vmd_vm *vm, unsigned int idx, uint8_t *addr)
{
	struct vmop_create_params	*vmc = &vm->vm_params;
	struct virtio_dev		*dev;
	struct vionet_dev		*vionet = NULL;
	int ret;

	if (idx > vmc->vmc_nnics)
		fatalx("%s: invalid vionet index: %u", __func__, idx);

	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		if (dev->dev_type == VMD_DEVTYPE_NET
		    && dev->vionet.idx == idx) {
			vionet = &dev->vionet;
			break;
		}
	}
	if (vionet == NULL)
		fatalx("%s: dev == NULL, idx = %u", __func__, idx);

	/* Set the local vm process copy. */
	memcpy(vionet->hostmac, addr, sizeof(vionet->hostmac));

	/* Send the information to the device process. */
	ret = imsg_compose_event(&dev->async_iev, IMSG_DEVOP_HOSTMAC, 0, 0, -1,
	    vionet->hostmac, sizeof(vionet->hostmac));
	if (ret == -1) {
		log_warnx("%s: failed to queue hostmac to vionet dev %u",
		    __func__, idx);
		return;
	}
}

void
virtio_shutdown(struct vmd_vm *vm)
{
	int ret, status;
	pid_t pid = 0;
	struct virtio_dev *dev, *tmp;
	struct viodev_msg msg;
	struct imsgbuf *ibuf;

	/* Ensure that our disks are synced. */
	if (vioscsi != NULL)
		vioscsi->file.close(vioscsi->file.p, 0);

	/*
	 * Broadcast shutdown to child devices. We need to do this
	 * synchronously as we have already stopped the async event thread.
	 */
	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		memset(&msg, 0, sizeof(msg));
		msg.type = VIODEV_MSG_SHUTDOWN;
		ibuf = &dev->sync_iev.ibuf;
		ret = imsg_compose(ibuf, VIODEV_MSG_SHUTDOWN, 0, 0, -1,
		    &msg, sizeof(msg));
		if (ret == -1)
			fatalx("%s: failed to send shutdown to device",
			    __func__);
		if (imsg_flush(ibuf) == -1)
			fatalx("%s: imsg_flush", __func__);
	}

	/*
	 * Wait for all children to shutdown using a simple approach of
	 * iterating over known child devices and waiting for them to die.
	 */
	SLIST_FOREACH_SAFE(dev, &virtio_devs, dev_next, tmp) {
		log_debug("%s: waiting on device pid %d", __func__,
		    dev->dev_pid);
		do {
			pid = waitpid(dev->dev_pid, &status, WNOHANG);
		} while (pid == 0 || (pid == -1 && errno == EINTR));
		if (pid == dev->dev_pid)
			log_debug("%s: device for pid %d is stopped",
			    __func__, pid);
		else
			log_warnx("%s: unexpected pid %d", __func__, pid);
		free(dev);
	}
}

int
vmmci_restore(int fd, uint32_t vm_id)
{
	log_debug("%s: receiving vmmci", __func__);
	if (atomicio(read, fd, &vmmci, sizeof(vmmci)) != sizeof(vmmci)) {
		log_warnx("%s: error reading vmmci from fd", __func__);
		return (-1);
	}

	if (pci_set_bar_fn(vmmci.pci_id, 0, vmmci_io, NULL)) {
		log_warnx("%s: can't set bar fn for vmm control device",
		    __progname);
		return (-1);
	}
	vmmci.vm_id = vm_id;
	vmmci.irq = pci_get_dev_irq(vmmci.pci_id);
	memset(&vmmci.timeout, 0, sizeof(struct event));
	evtimer_set(&vmmci.timeout, vmmci_timeout, NULL);
	return (0);
}

int
viornd_restore(int fd, struct vmd_vm *vm)
{
	void *hva = NULL;

	log_debug("%s: receiving viornd", __func__);
	if (atomicio(read, fd, &viornd, sizeof(viornd)) != sizeof(viornd)) {
		log_warnx("%s: error reading viornd from fd", __func__);
		return (-1);
	}
	if (pci_set_bar_fn(viornd.pci_id, 0, virtio_rnd_io, NULL)) {
		log_warnx("%s: can't set bar fn for virtio rng device",
		    __progname);
		return (-1);
	}
	viornd.vm_id = vm->vm_params.vmc_params.vcp_id;
	viornd.irq = pci_get_dev_irq(viornd.pci_id);

	hva = hvaddr_mem(viornd.vq[0].q_gpa, vring_size(VIORND_QUEUE_SIZE));
	if (hva == NULL)
		fatal("failed to restore viornd virtqueue");
	viornd.vq[0].q_hva = hva;

	return (0);
}

int
vionet_restore(int fd, struct vmd_vm *vm, int *child_taps)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	struct vm_create_params *vcp = &vmc->vmc_params;
	struct virtio_dev *dev;
	uint8_t i;

	if (vmc->vmc_nnics == 0)
		return (0);

	for (i = 0; i < vmc->vmc_nnics; i++) {
		dev = calloc(1, sizeof(struct virtio_dev));
		if (dev == NULL) {
			log_warn("%s: calloc failure allocating vionet",
			    __progname);
			return (-1);
		}

		log_debug("%s: receiving virtio network device", __func__);
		if (atomicio(read, fd, dev, sizeof(struct virtio_dev))
		    != sizeof(struct virtio_dev)) {
			log_warnx("%s: error reading vionet from fd",
			    __func__);
			return (-1);
		}

		/* Virtio network */
		if (dev->dev_type != VMD_DEVTYPE_NET) {
			log_warnx("%s: invalid device type", __func__);
			return (-1);
		}

		dev->sync_fd = -1;
		dev->async_fd = -1;
		dev->vm_id = vcp->vcp_id;
		dev->vm_vmid = vm->vm_vmid;
		dev->irq = pci_get_dev_irq(dev->pci_id);

		if (pci_set_bar_fn(dev->pci_id, 0, virtio_pci_io, dev)) {
			log_warnx("%s: can't set bar fn for virtio net "
			    "device", __progname);
			return (-1);
		}

		dev->vionet.data_fd = child_taps[i];
		dev->vionet.idx = i;

		SLIST_INSERT_HEAD(&virtio_devs, dev, dev_next);
	}

	return (0);
}

int
vioblk_restore(int fd, struct vmd_vm *vm,
    int child_disks[][VM_MAX_BASE_PER_DISK])
{
	struct vmop_create_params *vmc = &vm->vm_params;
	struct virtio_dev *dev;
	uint8_t i, j;

	if (vmc->vmc_ndisks == 0)
		return (0);

	for (i = 0; i < vmc->vmc_ndisks; i++) {
		dev = calloc(1, sizeof(struct virtio_dev));
		if (dev == NULL) {
			log_warn("%s: calloc failure allocating vioblks",
			    __progname);
			return (-1);
		}

		log_debug("%s: receiving vioblk", __func__);
		if (atomicio(read, fd, dev, sizeof(struct virtio_dev))
		    != sizeof(struct virtio_dev)) {
			log_warnx("%s: error reading vioblk from fd", __func__);
			return (-1);
		}
		if (dev->dev_type != VMD_DEVTYPE_DISK) {
			log_warnx("%s: invalid device type", __func__);
			return (-1);
		}

		dev->sync_fd = -1;
		dev->async_fd = -1;

		if (pci_set_bar_fn(dev->pci_id, 0, virtio_pci_io, dev)) {
			log_warnx("%s: can't set bar fn for virtio block "
			    "device", __progname);
			return (-1);
		}
		dev->vm_id = vmc->vmc_params.vcp_id;
		dev->irq = pci_get_dev_irq(dev->pci_id);

		memset(&dev->vioblk.disk_fd, -1, sizeof(dev->vioblk.disk_fd));
		dev->vioblk.ndisk_fd = vmc->vmc_diskbases[i];
		for (j = 0; j < dev->vioblk.ndisk_fd; j++)
			dev->vioblk.disk_fd[j] = child_disks[i][j];

		dev->vioblk.idx = i;
		SLIST_INSERT_HEAD(&virtio_devs, dev, dev_next);
	}
	return (0);
}

int
vioscsi_restore(int fd, struct vmd_vm *vm, int child_cdrom)
{
	void *hva = NULL;
	unsigned int i;

	if (!strlen(vm->vm_params.vmc_cdrom))
		return (0);

	vioscsi = calloc(1, sizeof(struct vioscsi_dev));
	if (vioscsi == NULL) {
		log_warn("%s: calloc failure allocating vioscsi", __progname);
		return (-1);
	}

	log_debug("%s: receiving vioscsi", __func__);

	if (atomicio(read, fd, vioscsi, sizeof(struct vioscsi_dev)) !=
	    sizeof(struct vioscsi_dev)) {
		log_warnx("%s: error reading vioscsi from fd", __func__);
		return (-1);
	}

	if (pci_set_bar_fn(vioscsi->pci_id, 0, vioscsi_io, vioscsi)) {
		log_warnx("%s: can't set bar fn for vmm control device",
		    __progname);
		return (-1);
	}

	vioscsi->vm_id = vm->vm_params.vmc_params.vcp_id;
	vioscsi->irq = pci_get_dev_irq(vioscsi->pci_id);

	/* vioscsi uses 3 virtqueues. */
	for (i = 0; i < 3; i++) {
		hva = hvaddr_mem(vioscsi->vq[i].q_gpa,
		    vring_size(VIOSCSI_QUEUE_SIZE));
		if (hva == NULL)
			fatal("failed to restore vioscsi virtqueue");
		vioscsi->vq[i].q_hva = hva;
	}

	return (0);
}

int
virtio_restore(int fd, struct vmd_vm *vm, int child_cdrom,
    int child_disks[][VM_MAX_BASE_PER_DISK], int *child_taps)
{
	struct virtio_dev *dev;
	int ret;

	SLIST_INIT(&virtio_devs);

	if ((ret = viornd_restore(fd, vm)) == -1)
		return (ret);

	if ((ret = vioblk_restore(fd, vm, child_disks)) == -1)
		return (ret);

	if ((ret = vioscsi_restore(fd, vm, child_cdrom)) == -1)
		return (ret);

	if ((ret = vionet_restore(fd, vm, child_taps)) == -1)
		return (ret);

	if ((ret = vmmci_restore(fd, vm->vm_params.vmc_params.vcp_id)) == -1)
		return (ret);

	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		if (virtio_dev_launch(vm, dev) != 0)
			fatalx("%s: failed to restore virtio dev", __func__);
	}

	return (0);
}

int
viornd_dump(int fd)
{
	log_debug("%s: sending viornd", __func__);

	viornd.vq[0].q_hva = NULL;

	if (atomicio(vwrite, fd, &viornd, sizeof(viornd)) != sizeof(viornd)) {
		log_warnx("%s: error writing viornd to fd", __func__);
		return (-1);
	}
	return (0);
}

int
vmmci_dump(int fd)
{
	log_debug("%s: sending vmmci", __func__);

	if (atomicio(vwrite, fd, &vmmci, sizeof(vmmci)) != sizeof(vmmci)) {
		log_warnx("%s: error writing vmmci to fd", __func__);
		return (-1);
	}
	return (0);
}

int
vionet_dump(int fd)
{
	struct virtio_dev	*dev, temp;
	struct viodev_msg	 msg;
	struct imsg		 imsg;
	struct imsgbuf		*ibuf = NULL;
	size_t			 sz;
	int			 ret;

	log_debug("%s: dumping vionet", __func__);

	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		if (dev->dev_type != VMD_DEVTYPE_NET)
			continue;

		memset(&msg, 0, sizeof(msg));
		memset(&imsg, 0, sizeof(imsg));

		ibuf = &dev->sync_iev.ibuf;
		msg.type = VIODEV_MSG_DUMP;

		ret = imsg_compose(ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
		    sizeof(msg));
		if (ret == -1) {
			log_warnx("%s: failed requesting dump of vionet[%d]",
			    __func__, dev->vionet.idx);
			return (-1);
		}
		if (imsg_flush(ibuf) == -1) {
			log_warnx("%s: imsg_flush", __func__);
			return (-1);
		}

		sz = atomicio(read, dev->sync_fd, &temp, sizeof(temp));
		if (sz != sizeof(temp)) {
			log_warnx("%s: failed to dump vionet[%d]", __func__,
			    dev->vionet.idx);
			return (-1);
		}

		/* Clear volatile state. Will reinitialize on restore. */
		temp.vionet.vq[RXQ].q_hva = NULL;
		temp.vionet.vq[TXQ].q_hva = NULL;
		temp.async_fd = -1;
		temp.sync_fd = -1;
		memset(&temp.async_iev, 0, sizeof(temp.async_iev));
		memset(&temp.sync_iev, 0, sizeof(temp.sync_iev));

		if (atomicio(vwrite, fd, &temp, sizeof(temp)) != sizeof(temp)) {
			log_warnx("%s: error writing vionet to fd", __func__);
			return (-1);
		}
	}

	return (0);
}

int
vioblk_dump(int fd)
{
	struct virtio_dev	*dev, temp;
	struct viodev_msg	 msg;
	struct imsg		 imsg;
	struct imsgbuf		*ibuf = NULL;
	size_t			 sz;
	int			 ret;

	log_debug("%s: dumping vioblk", __func__);

	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		if (dev->dev_type != VMD_DEVTYPE_DISK)
			continue;

		memset(&msg, 0, sizeof(msg));
		memset(&imsg, 0, sizeof(imsg));

		ibuf = &dev->sync_iev.ibuf;
		msg.type = VIODEV_MSG_DUMP;

		ret = imsg_compose(ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
		    sizeof(msg));
		if (ret == -1) {
			log_warnx("%s: failed requesting dump of vioblk[%d]",
			    __func__, dev->vioblk.idx);
			return (-1);
		}
		if (imsg_flush(ibuf) == -1) {
			log_warnx("%s: imsg_flush", __func__);
			return (-1);
		}


		sz = atomicio(read, dev->sync_fd, &temp, sizeof(temp));
		if (sz != sizeof(temp)) {
			log_warnx("%s: failed to dump vioblk[%d]", __func__,
			    dev->vioblk.idx);
			return (-1);
		}

		/* Clear volatile state. Will reinitialize on restore. */
		temp.vioblk.vq[0].q_hva = NULL;
		temp.async_fd = -1;
		temp.sync_fd = -1;
		memset(&temp.async_iev, 0, sizeof(temp.async_iev));
		memset(&temp.sync_iev, 0, sizeof(temp.sync_iev));

		if (atomicio(vwrite, fd, &temp, sizeof(temp)) != sizeof(temp)) {
			log_warnx("%s: error writing vioblk to fd", __func__);
			return (-1);
		}
	}

	return (0);
}

int
vioscsi_dump(int fd)
{
	unsigned int i;

	if (vioscsi == NULL)
		return (0);

	log_debug("%s: sending vioscsi", __func__);

	for (i = 0; i < 3; i++)
		vioscsi->vq[i].q_hva = NULL;

	if (atomicio(vwrite, fd, vioscsi, sizeof(struct vioscsi_dev)) !=
	    sizeof(struct vioscsi_dev)) {
		log_warnx("%s: error writing vioscsi to fd", __func__);
		return (-1);
	}
	return (0);
}

int
virtio_dump(int fd)
{
	int ret;

	if ((ret = viornd_dump(fd)) == -1)
		return ret;

	if ((ret = vioblk_dump(fd)) == -1)
		return ret;

	if ((ret = vioscsi_dump(fd)) == -1)
		return ret;

	if ((ret = vionet_dump(fd)) == -1)
		return ret;

	if ((ret = vmmci_dump(fd)) == -1)
		return ret;

	return (0);
}

void virtio_broadcast_imsg(struct vmd_vm *vm, uint16_t type, void *data,
    uint16_t datalen)
{
	struct virtio_dev *dev;
	int ret;

	SLIST_FOREACH(dev, &virtio_devs, dev_next) {
		ret = imsg_compose_event(&dev->async_iev, type, 0, 0, -1, data,
		    datalen);
		if (ret == -1) {
			log_warnx("%s: failed to broadcast imsg type %u",
			    __func__, type);
		}
	}

}

void
virtio_stop(struct vmd_vm *vm)
{
	return virtio_broadcast_imsg(vm, IMSG_VMDOP_PAUSE_VM, NULL, 0);
}

void
virtio_start(struct vmd_vm *vm)
{
	return virtio_broadcast_imsg(vm, IMSG_VMDOP_UNPAUSE_VM, NULL, 0);
}

/*
 * Fork+exec a child virtio device. Returns 0 on success.
 */
static int
virtio_dev_launch(struct vmd_vm *vm, struct virtio_dev *dev)
{
	char *nargv[12], num[32], vmm_fd[32], vm_name[VM_NAME_MAX], t[2];
	pid_t dev_pid;
	int data_fds[VM_MAX_BASE_PER_DISK], sync_fds[2], async_fds[2], ret = 0;
	size_t i, data_fds_sz, sz = 0;
	struct viodev_msg msg;
	struct imsg imsg;
	struct imsgev *iev = &dev->sync_iev;

	switch (dev->dev_type) {
	case VMD_DEVTYPE_NET:
		data_fds[0] = dev->vionet.data_fd;
		data_fds_sz = 1;
		log_debug("%s: launching vionet%d",
		    vm->vm_params.vmc_params.vcp_name, dev->vionet.idx);
		break;
	case VMD_DEVTYPE_DISK:
		memcpy(&data_fds, dev->vioblk.disk_fd, sizeof(data_fds));
		data_fds_sz = dev->vioblk.ndisk_fd;
		log_debug("%s: launching vioblk%d",
		    vm->vm_params.vmc_params.vcp_name, dev->vioblk.idx);
		break;
		/* NOTREACHED */
	default:
		log_warn("%s: invalid device type", __func__);
		return (EINVAL);
	}

	/* We need two channels: one synchronous (IO reads) and one async. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sync_fds) == -1) {
		log_warn("failed to create socketpair");
		return (errno);
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, async_fds) == -1) {
		log_warn("failed to create async socketpair");
		return (errno);
	}

	/* Keep communication channels open after exec. */
	if (fcntl(sync_fds[1], F_SETFD, 0)) {
		ret = errno;
		log_warn("%s: fcntl", __func__);
		goto err;
	}
	if (fcntl(async_fds[1], F_SETFD, 0)) {
		ret = errno;
		log_warn("%s: fcnt", __func__);
		goto err;
	}

	/* Fork... */
	dev_pid = fork();
	if (dev_pid == -1) {
		ret = errno;
		log_warn("%s: fork failed", __func__);
		goto err;
	}

	if (dev_pid > 0) {
		/* Parent */
		close_fd(sync_fds[1]);
		close_fd(async_fds[1]);

		/* Save the child's pid to help with cleanup. */
		dev->dev_pid = dev_pid;

		/* Set the channel fds to the child's before sending. */
		dev->sync_fd = sync_fds[1];
		dev->async_fd = async_fds[1];

		/* Close data fds. Only the child device needs them now. */
		for (i = 0; i < data_fds_sz; i++)
			close_fd(data_fds[i]);

		/* Set our synchronous channel to non-blocking. */
		if (fcntl(sync_fds[0], F_SETFL, O_NONBLOCK) == -1) {
			ret = errno;
			log_warn("%s: fcntl", __func__);
			goto err;
		}

		/* 1. Send over our configured device. */
		log_debug("%s: sending '%c' type device struct", __func__,
			dev->dev_type);
		sz = atomicio(vwrite, sync_fds[0], dev, sizeof(*dev));
		if (sz != sizeof(*dev)) {
			log_warnx("%s: failed to send device", __func__);
			ret = EIO;
			goto err;
		}

		/* 2. Send over details on the VM (including memory fds). */
		log_debug("%s: sending vm message for '%s'", __func__,
			vm->vm_params.vmc_params.vcp_name);
		sz = atomicio(vwrite, sync_fds[0], vm, sizeof(*vm));
		if (sz != sizeof(*vm)) {
			log_warnx("%s: failed to send vm details", __func__);
			ret = EIO;
			goto err;
		}

		/*
		 * Initialize our imsg channel to the child device. The initial
		 * communication will be synchronous. We expect the child to
		 * report itself "ready" to confirm the launch was a success.
		 */
		imsg_init(&iev->ibuf, sync_fds[0]);
		do
			ret = imsg_read(&iev->ibuf);
		while (ret == -1 && errno == EAGAIN);
		if (ret == 0 || ret == -1) {
			log_warnx("%s: failed to receive ready message from "
			    "'%c' type device", __func__, dev->dev_type);
			ret = EIO;
			goto err;
		}
		ret = 0;

		log_debug("%s: receiving reply", __func__);
		if (imsg_get(&iev->ibuf, &imsg) < 1) {
			log_warnx("%s: imsg_get", __func__);
			ret = EIO;
			goto err;
		}
		IMSG_SIZE_CHECK(&imsg, &msg);
		memcpy(&msg, imsg.data, sizeof(msg));
		imsg_free(&imsg);

		if (msg.type != VIODEV_MSG_READY) {
			log_warnx("%s: expected ready message, got type %d",
			    __func__, msg.type);
			ret = EINVAL;
			goto err;
		}
		log_debug("%s: device reports ready via sync channel",
		    __func__);

		/*
		 * Wire in the async event handling, but after reverting back
		 * to the parent's fd's.
		 */
		dev->sync_fd = sync_fds[0];
		dev->async_fd = async_fds[0];
		vm_device_pipe(dev, virtio_dispatch_dev);
	} else {
		/* Child */
		close_fd(async_fds[0]);
		close_fd(sync_fds[0]);

		/* Keep data file descriptors open after exec. */
		for (i = 0; i < data_fds_sz; i++) {
			log_debug("%s: marking fd %d !close-on-exec", __func__,
			    data_fds[i]);
			if (fcntl(data_fds[i], F_SETFD, 0)) {
				ret = errno;
				log_warn("%s: fcntl", __func__);
				goto err;
			}
		}

		memset(&nargv, 0, sizeof(nargv));
		memset(num, 0, sizeof(num));
		snprintf(num, sizeof(num), "%d", sync_fds[1]);
		memset(vmm_fd, 0, sizeof(vmm_fd));
		snprintf(vmm_fd, sizeof(vmm_fd), "%d", env->vmd_fd);
		memset(vm_name, 0, sizeof(vm_name));
		snprintf(vm_name, sizeof(vm_name), "%s",
		    vm->vm_params.vmc_params.vcp_name);

		t[0] = dev->dev_type;
		t[1] = '\0';

		nargv[0] = env->argv0;
		nargv[1] = "-X";
		nargv[2] = num;
		nargv[3] = "-t";
		nargv[4] = t;
		nargv[5] = "-i";
		nargv[6] = vmm_fd;
		nargv[7] = "-p";
		nargv[8] = vm_name;
		nargv[9] = "-n";
		nargv[10] = NULL;

		if (env->vmd_verbose == 1) {
			nargv[10] = VMD_VERBOSE_1;
			nargv[11] = NULL;
		} else if (env->vmd_verbose > 1) {
			nargv[10] = VMD_VERBOSE_2;
			nargv[11] = NULL;
		}

		/* Control resumes in vmd.c:main(). */
		execvp(nargv[0], nargv);

		ret = errno;
		log_warn("%s: failed to exec device", __func__);
		_exit(ret);
		/* NOTREACHED */
	}

	return (ret);

err:
	close_fd(sync_fds[0]);
	close_fd(sync_fds[1]);
	close_fd(async_fds[0]);
	close_fd(async_fds[1]);
	return (ret);
}

/*
 * Initialize an async imsg channel for a virtio device.
 */
int
vm_device_pipe(struct virtio_dev *dev, void (*cb)(int, short, void *))
{
	struct imsgev *iev = &dev->async_iev;
	int fd = dev->async_fd;

	log_debug("%s: initializing '%c' device pipe (fd=%d)", __func__,
	    dev->dev_type, fd);

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		log_warn("failed to set nonblocking mode on vm device pipe");
		return (-1);
	}

	imsg_init(&iev->ibuf, fd);
	iev->handler = cb;
	iev->data = dev;
	iev->events = EV_READ;
	imsg_event_add(iev);

	return (0);
}

void
virtio_dispatch_dev(int fd, short event, void *arg)
{
	struct virtio_dev	*dev = (struct virtio_dev*)arg;
	struct imsgev		*iev = &dev->async_iev;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct viodev_msg	 msg;
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
		case IMSG_DEVOP_MSG:
			IMSG_SIZE_CHECK(&imsg, &msg);
			memcpy(&msg, imsg.data, sizeof(msg));
			handle_dev_msg(&msg, dev);
			break;
		default:
			log_warnx("%s: got non devop imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}


static int
handle_dev_msg(struct viodev_msg *msg, struct virtio_dev *gdev)
{
	uint32_t vm_id = gdev->vm_id;
	int irq = gdev->irq;

	switch (msg->type) {
	case VIODEV_MSG_KICK:
		if (msg->state == INTR_STATE_ASSERT)
			vcpu_assert_pic_irq(vm_id, msg->vcpu, irq);
		else if (msg->state == INTR_STATE_DEASSERT)
			vcpu_deassert_pic_irq(vm_id, msg->vcpu, irq);
		break;
	case VIODEV_MSG_READY:
		log_debug("%s: device reports ready", __func__);
		break;
	case VIODEV_MSG_ERROR:
		log_warnx("%s: device reported error", __func__);
		break;
	case VIODEV_MSG_INVALID:
	case VIODEV_MSG_IO_READ:
	case VIODEV_MSG_IO_WRITE:
		/* FALLTHROUGH */
	default:
		log_warnx("%s: unsupported device message type %d", __func__,
		    msg->type);
		return (1);
	}

	return (0);
};

/*
 * Called by the VM process while processing IO from the VCPU thread.
 *
 * N.b. Since the VCPU thread calls this function, we cannot mutate the event
 * system. All ipc messages must be sent manually and cannot be queued for
 * the event loop to push them. (We need to perform a synchronous read, so
 * this isn't really a big deal.)
 */
int
virtio_pci_io(int dir, uint16_t reg, uint32_t *data, uint8_t *intr,
    void *cookie, uint8_t sz)
{
	struct virtio_dev *dev = (struct virtio_dev *)cookie;
	struct imsgbuf *ibuf = &dev->sync_iev.ibuf;
	struct imsg imsg;
	struct viodev_msg msg;
	ssize_t n;
	int ret = 0;

	memset(&msg, 0, sizeof(msg));
	msg.reg = reg;
	msg.io_sz = sz;

	if (dir == 0) {
		msg.type = VIODEV_MSG_IO_WRITE;
		msg.data = *data;
		msg.data_valid = 1;
	} else
		msg.type = VIODEV_MSG_IO_READ;

	if (msg.type == VIODEV_MSG_IO_WRITE) {
		/*
		 * Write request. No reply expected.
		 */
		ret = imsg_compose(ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
		    sizeof(msg));
		if (ret == -1) {
			log_warn("%s: failed to send async io event to virtio"
			    " device", __func__);
			return (ret);
		}
		if (imsg_flush(ibuf) == -1) {
			log_warnx("%s: imsg_flush (write)", __func__);
			return (-1);
		}
	} else {
		/*
		 * Read request. Requires waiting for a reply.
		 */
		ret = imsg_compose(ibuf, IMSG_DEVOP_MSG, 0, 0, -1, &msg,
		    sizeof(msg));
		if (ret == -1) {
			log_warnx("%s: failed to send sync io event to virtio"
			    " device", __func__);
			return (ret);
		}
		if (imsg_flush(ibuf) == -1) {
			log_warnx("%s: imsg_flush (read)", __func__);
			return (-1);
		}

		/* Read our reply. */
		do
			n = imsg_read(ibuf);
		while (n == -1 && errno == EAGAIN);
		if (n == 0 || n == -1) {
			log_warn("%s: imsg_read (n=%ld)", __func__, n);
			return (-1);
		}
		if ((n = imsg_get(ibuf, &imsg)) == -1) {
			log_warn("%s: imsg_get (n=%ld)", __func__, n);
			return (-1);
		}
		if (n == 0) {
			log_warnx("%s: invalid imsg", __func__);
			return (-1);
		}

		IMSG_SIZE_CHECK(&imsg, &msg);
		memcpy(&msg, imsg.data, sizeof(msg));
		imsg_free(&imsg);

		if (msg.type == VIODEV_MSG_IO_READ && msg.data_valid) {
#if DEBUG
			log_debug("%s: got sync read response (reg=%s)",
			    __func__, virtio_reg_name(msg.reg));
#endif /* DEBUG */
			*data = msg.data;
			/*
			 * It's possible we're asked to {de,}assert after the
			 * device performs a register read.
			 */
			if (msg.state == INTR_STATE_ASSERT)
				vcpu_assert_pic_irq(dev->vm_id, msg.vcpu, msg.irq);
			else if (msg.state == INTR_STATE_DEASSERT)
				vcpu_deassert_pic_irq(dev->vm_id, msg.vcpu, msg.irq);
		} else {
			log_warnx("%s: expected IO_READ, got %d", __func__,
			    msg.type);
			return (-1);
		}
	}

	return (0);
}

void
virtio_assert_pic_irq(struct virtio_dev *dev, int vcpu)
{
	struct viodev_msg msg;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.irq = dev->irq;
	msg.vcpu = vcpu;
	msg.type = VIODEV_MSG_KICK;
	msg.state = INTR_STATE_ASSERT;

	ret = imsg_compose_event(&dev->async_iev, IMSG_DEVOP_MSG, 0, 0, -1,
	    &msg, sizeof(msg));
	if (ret == -1)
		log_warnx("%s: failed to assert irq %d", __func__, dev->irq);
}

void
virtio_deassert_pic_irq(struct virtio_dev *dev, int vcpu)
{
	struct viodev_msg msg;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.irq = dev->irq;
	msg.vcpu = vcpu;
	msg.type = VIODEV_MSG_KICK;
	msg.state = INTR_STATE_DEASSERT;

	ret = imsg_compose_event(&dev->async_iev, IMSG_DEVOP_MSG, 0, 0, -1,
	    &msg, sizeof(msg));
	if (ret == -1)
		log_warnx("%s: failed to deassert irq %d", __func__, dev->irq);
}
