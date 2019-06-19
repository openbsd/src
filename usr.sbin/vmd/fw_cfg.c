/*	$OpenBSD: fw_cfg.c,v 1.1 2018/12/10 21:30:33 claudio Exp $	*/
/*
 * Copyright (c) 2018 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/uio.h>
#include <machine/vmmvar.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "proc.h"
#include "vmd.h"
#include "vmm.h"
#include "fw_cfg.h"

#define	FW_CFG_SIGNATURE	0x0000
#define	FW_CFG_ID		0x0001
#define	FW_CFG_NOGRAPHIC	0x0004
#define	FW_CFG_FILE_DIR		0x0019
#define	FW_CFG_FILE_FIRST	0x0020

#define FW_CFG_DMA_SIGNATURE	0x51454d5520434647ULL /* QEMU CFG */

struct fw_cfg_dma_access {
	uint32_t	control;
#define FW_CFG_DMA_ERROR	0x0001
#define FW_CFG_DMA_READ		0x0002
#define FW_CFG_DMA_SKIP		0x0004
#define FW_CFG_DMA_SELECT	0x0008
#define FW_CFG_DMA_WRITE	0x0010	/* not implemented */
	uint32_t	length;
	uint64_t	address;
};

struct fw_cfg_file {
	uint32_t	size;
	uint16_t	selector;
	uint16_t	reserved;
	char		name[56];
};

extern char *__progname;

static struct fw_cfg_state {
	size_t offset;
	size_t size;
	uint8_t *data;
} fw_cfg_state;

static uint64_t	fw_cfg_dma_addr;

static int	fw_cfg_select_file(uint16_t);
static void	fw_cfg_file_dir(void);

void
fw_cfg_init(struct vmop_create_params *vmc)
{
	const char *bootorder = NULL;
	unsigned int sd = 0;

	/* do not double print chars on serial port */
	fw_cfg_add_file("etc/screen-and-debug", &sd, sizeof(sd));

	switch (vmc->vmc_bootdevice) {
	case VMBOOTDEV_DISK:
		bootorder = "/pci@i0cf8/*@2\nHALT";
		break;
	case VMBOOTDEV_CDROM:
		bootorder = "/pci@i0cf8/*@4/*@0/*@0,100\nHALT";
		break;
	case VMBOOTDEV_NET:
		/* XXX not yet */
		bootorder = "HALT";
		break;
	}
	if (bootorder)
		fw_cfg_add_file("bootorder", bootorder, strlen(bootorder) + 1);
}

int
fw_cfg_dump(int fd)
{
	log_debug("%s: sending fw_cfg state", __func__);
	if (atomicio(vwrite, fd, &fw_cfg_dma_addr,
	    sizeof(fw_cfg_dma_addr)) != sizeof(fw_cfg_dma_addr)) {
		log_warnx("%s: error writing fw_cfg to fd", __func__);
		return -1;
	}
	if (atomicio(vwrite, fd, &fw_cfg_state.offset,
	    sizeof(fw_cfg_state.offset)) != sizeof(fw_cfg_state.offset)) {
		log_warnx("%s: error writing fw_cfg to fd", __func__);
		return -1;
	}
	if (atomicio(vwrite, fd, &fw_cfg_state.size,
	    sizeof(fw_cfg_state.size)) != sizeof(fw_cfg_state.size)) {
		log_warnx("%s: error writing fw_cfg to fd", __func__);
		return -1;
	}
	if (fw_cfg_state.size != 0)
		if (atomicio(vwrite, fd, fw_cfg_state.data,
		    fw_cfg_state.size) != fw_cfg_state.size) {
			log_warnx("%s: error writing fw_cfg to fd", __func__);
			return (-1);
		}
	return 0;
}

int
fw_cfg_restore(int fd)
{
	log_debug("%s: receiving fw_cfg state", __func__);
	if (atomicio(read, fd, &fw_cfg_dma_addr,
	    sizeof(fw_cfg_dma_addr)) != sizeof(fw_cfg_dma_addr)) {
		log_warnx("%s: error reading fw_cfg from fd", __func__);
		return -1;
	}
	if (atomicio(read, fd, &fw_cfg_state.offset,
	    sizeof(fw_cfg_state.offset)) != sizeof(fw_cfg_state.offset)) {
		log_warnx("%s: error reading fw_cfg from fd", __func__);
		return -1;
	}
	if (atomicio(read, fd, &fw_cfg_state.size,
	    sizeof(fw_cfg_state.size)) != sizeof(fw_cfg_state.size)) {
		log_warnx("%s: error reading fw_cfg from fd", __func__);
		return -1;
	}
	fw_cfg_state.data = NULL;
	if (fw_cfg_state.size != 0) {
		if ((fw_cfg_state.data = malloc(fw_cfg_state.size)) == NULL)
			fatal("%s", __func__);
		if (atomicio(read, fd, fw_cfg_state.data,
		    fw_cfg_state.size) != fw_cfg_state.size) {
			log_warnx("%s: error reading fw_cfg from fd", __func__);
			return -1;
		}
	}
	return 0;
}

static void
fw_cfg_reset_state(void)
{
	free(fw_cfg_state.data);
	fw_cfg_state.offset = 0;
	fw_cfg_state.size = 0;
	fw_cfg_state.data = NULL;
}

static void
fw_cfg_set_state(void *data, size_t len)
{
	if ((fw_cfg_state.data = malloc(len)) == NULL) {
		log_warn("%s", __func__);
		return;
	}
	memcpy(fw_cfg_state.data, data, len);
	fw_cfg_state.size = len;
	fw_cfg_state.offset = 0;
}

static void
fw_cfg_select(uint16_t selector)
{
	uint16_t one = 1;
	uint32_t id = htole32(0x3);

	fw_cfg_reset_state();
	switch (selector) {
	case FW_CFG_SIGNATURE:
		fw_cfg_set_state("QEMU", 4);
		break;
	case FW_CFG_ID:
		fw_cfg_set_state(&id, sizeof(id));
		break;
	case FW_CFG_NOGRAPHIC:
		fw_cfg_set_state(&one, sizeof(one));
		break;
	case FW_CFG_FILE_DIR:
		fw_cfg_file_dir();
		break;
	default:
		if (!fw_cfg_select_file(selector))
			log_debug("%s: unhandled selector %x",
			    __func__, selector);
		break;
	}
}

static void
fw_cfg_handle_dma(struct fw_cfg_dma_access *fw)
{
	uint32_t len = 0, control = fw->control;

	fw->control = 0;
	if (control & FW_CFG_DMA_SELECT) {
		uint16_t selector = control >> 16;
		log_debug("%s: selector 0x%04x", __func__, selector);
		fw_cfg_select(selector);
	}

	/* calculate correct length of operation */
	if (fw_cfg_state.offset < fw_cfg_state.size)
		len = fw_cfg_state.size - fw_cfg_state.offset;
	if (len > fw->length)
		len = fw->length;

	if (control & FW_CFG_DMA_WRITE) {
		fw->control |= FW_CFG_DMA_ERROR;
	} else if (control & FW_CFG_DMA_READ) {
		if (write_mem(fw->address,
		    fw_cfg_state.data + fw_cfg_state.offset, len)) {
			log_warnx("%s: write_mem error", __func__);
			fw->control |= FW_CFG_DMA_ERROR;
		}
		/* clear rest of buffer */
		if (len < fw->length)
			if (write_mem(fw->address + len, NULL,
			    fw->length - len)) {
			log_warnx("%s: write_mem error", __func__);
			fw->control |= FW_CFG_DMA_ERROR;
		}
	}
	fw_cfg_state.offset += len;

	if (fw_cfg_state.offset == fw_cfg_state.size)
		fw_cfg_reset_state();
}

uint8_t
vcpu_exit_fw_cfg(struct vm_run_params *vrp)
{
	uint32_t data = 0;
	struct vm_exit *vei = vrp->vrp_exit;

	get_input_data(vei, &data);

	switch (vei->vei.vei_port) {
	case FW_CFG_IO_SELECT:
		if (vei->vei.vei_dir == VEI_DIR_IN) {
			log_warnx("%s: fw_cfg: read from selector port "
			    "unsupported", __progname);
			set_return_data(vei, 0);
			break;
		}
		log_debug("%s: selector 0x%04x", __func__, data);
		fw_cfg_select(data);
		break;
	case FW_CFG_IO_DATA:
		if (vei->vei.vei_dir == VEI_DIR_OUT) {
			log_debug("%s: fw_cfg: discarding data written to "
			    "data port", __progname);
			break;
		}
		/* fw_cfg only defines 1-byte reads via IO port */
		if (fw_cfg_state.offset < fw_cfg_state.size) {
			set_return_data(vei,
			    fw_cfg_state.data[fw_cfg_state.offset++]);
			if (fw_cfg_state.offset == fw_cfg_state.size)
				fw_cfg_reset_state();
		} else
			set_return_data(vei, 0);
		break;
	}

	return 0xFF;
}

uint8_t
vcpu_exit_fw_cfg_dma(struct vm_run_params *vrp)
{
	struct fw_cfg_dma_access fw_dma;
	uint32_t data = 0;
	struct vm_exit *vei = vrp->vrp_exit;

	if (vei->vei.vei_size != 4) {
		log_debug("%s: fw_cfg_dma: discarding data written to "
		    "dma addr", __progname);
		if (vei->vei.vei_dir == VEI_DIR_OUT)
			fw_cfg_dma_addr = 0;
		return 0xFF;
	}

	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		get_input_data(vei, &data);
		switch (vei->vei.vei_port) {
		case FW_CFG_IO_DMA_ADDR_HIGH:
			fw_cfg_dma_addr = (uint64_t)be32toh(data) << 32;
			break;
		case FW_CFG_IO_DMA_ADDR_LOW:
			fw_cfg_dma_addr |= be32toh(data);

			/* writing least significant half triggers operation */
			if (read_mem(fw_cfg_dma_addr, &fw_dma, sizeof(fw_dma)))
				break;
			/* adjust byteorder */
			fw_dma.control = be32toh(fw_dma.control);
			fw_dma.length = be32toh(fw_dma.length);
			fw_dma.address = be64toh(fw_dma.address);

			fw_cfg_handle_dma(&fw_dma);

			/* just write control byte back */
			data = be32toh(fw_dma.control);
			if (write_mem(fw_cfg_dma_addr, &data, sizeof(data)))
				break;

			/* done, reset base address */
			fw_cfg_dma_addr = 0;
			break;
		}
	} else {
		uint64_t sig = htobe64(FW_CFG_DMA_SIGNATURE);
		switch (vei->vei.vei_port) {
		case FW_CFG_IO_DMA_ADDR_HIGH:
			set_return_data(vei, sig >> 32);
			break;
		case FW_CFG_IO_DMA_ADDR_LOW:
			set_return_data(vei, sig & 0xffffffff);
			break;
		}
	}
	return 0xFF;
}

static uint16_t file_id = FW_CFG_FILE_FIRST;

struct fw_cfg_file_entry {
	TAILQ_ENTRY(fw_cfg_file_entry)	entry;
	struct fw_cfg_file		file;
	void				*data;
};

TAILQ_HEAD(, fw_cfg_file_entry) fw_cfg_files =
					TAILQ_HEAD_INITIALIZER(fw_cfg_files);

static struct fw_cfg_file_entry *
fw_cfg_lookup_file(const char *name)
{
	struct fw_cfg_file_entry *f;

	TAILQ_FOREACH(f, &fw_cfg_files, entry) {
		if (strcmp(name, f->file.name) == 0)
			return f;
	}
	return NULL;
}

void
fw_cfg_add_file(const char *name, const void *data, size_t len)
{
	struct fw_cfg_file_entry *f;

	if (fw_cfg_lookup_file(name))
		fatalx("%s: fw_cfg: file %s exists", __progname, name);

	if ((f = calloc(sizeof(f), 1)) == NULL)
		fatal("%s", __func__);

	if ((f->data = malloc(len)) == NULL)
		fatal("%s", __func__);

	if (strlcpy(f->file.name, name, sizeof(f->file.name)) >=
	    sizeof(f->file.name))
		fatalx("%s: fw_cfg: file name too long", __progname);

	f->file.size = htobe32(len);
	f->file.selector = htobe16(file_id++);
	memcpy(f->data, data, len);

	TAILQ_INSERT_TAIL(&fw_cfg_files, f, entry);
}

static int
fw_cfg_select_file(uint16_t id)
{
	struct fw_cfg_file_entry *f;

	id = htobe16(id);
	TAILQ_FOREACH(f, &fw_cfg_files, entry)
		if (f->file.selector == id) {
			size_t size = be32toh(f->file.size);
			fw_cfg_set_state(f->data, size);
			log_debug("%s: accessing file %s", __func__,
			    f->file.name);
			return 1;
		}
	return 0;
}

static void
fw_cfg_file_dir(void)
{
	struct fw_cfg_file_entry *f;
	struct fw_cfg_file *fp;
	uint32_t count = 0;
	uint32_t *data;
	size_t size;

	TAILQ_FOREACH(f, &fw_cfg_files, entry)
		count++;

	size = sizeof(count) + count * sizeof(struct fw_cfg_file);
	if ((data = malloc(size)) == NULL)
		fatal("%s", __func__);
	*data = htobe32(count);
	fp = (struct fw_cfg_file *)(data + 1);

	log_debug("%s: file directory with %d files", __func__, count);
	TAILQ_FOREACH(f, &fw_cfg_files, entry) {
		log_debug("  %6dB %04x %s", be32toh(f->file.size),
		    be16toh(f->file.selector), f->file.name);
		memcpy(fp, &f->file, sizeof(f->file));
		fp++;
	}

	/* XXX should sort by name but SeaBIOS does not care */

	fw_cfg_set_state(data, size);
}
