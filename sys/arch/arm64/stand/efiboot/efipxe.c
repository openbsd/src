/*	$OpenBSD: efipxe.c,v 1.3 2018/02/06 20:35:21 naddy Exp $	*/
/*
 * Copyright (c) 2017 Patrick Wildt <patrick@blueri.se>
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

#include <sys/param.h>
#include <sys/disklabel.h>

#include <libsa.h>
#include <lib/libsa/tftp.h>

#include <efi.h>
#include <efiapi.h>
#include "eficall.h"
#include "efiboot.h"
#include "disk.h"

extern EFI_BOOT_SERVICES	*BS;
extern EFI_DEVICE_PATH		*efi_bootdp;

extern char			*bootmac;
static UINT8			 boothw[16];
static EFI_IP_ADDRESS		 bootip, servip;
static EFI_GUID			 devp_guid = DEVICE_PATH_PROTOCOL;
static EFI_GUID			 pxe_guid = EFI_PXE_BASE_CODE_PROTOCOL;
static EFI_PXE_BASE_CODE	*PXE = NULL;

extern int	 efi_device_path_depth(EFI_DEVICE_PATH *dp, int);
extern int	 efi_device_path_ncmp(EFI_DEVICE_PATH *, EFI_DEVICE_PATH *, int);

/*
 * TFTP initial probe.  This function discovers PXE handles and tries
 * to figure out if there has already been a successfull PXE handshake.
 * If so, set the PXE variable.
 */
void
efi_pxeprobe(void)
{
	EFI_PXE_BASE_CODE *pxe;
	EFI_DEVICE_PATH *dp0;
	EFI_HANDLE *handles;
	EFI_STATUS status;
	UINTN nhandles;
	int i, depth;

	if (efi_bootdp == NULL)
		return;

	status = EFI_CALL(BS->LocateHandleBuffer, ByProtocol, &pxe_guid, NULL,
	    &nhandles, &handles);
	if (status != EFI_SUCCESS)
		return;

	for (i = 0; i < nhandles; i++) {
		EFI_PXE_BASE_CODE_DHCPV4_PACKET *dhcp = NULL;

		status = EFI_CALL(BS->HandleProtocol, handles[i],
		    &devp_guid, (void **)&dp0);
		if (status != EFI_SUCCESS)
			continue;

		depth = efi_device_path_depth(efi_bootdp, MEDIA_DEVICE_PATH);
		if (efi_device_path_ncmp(efi_bootdp, dp0, depth))
			continue;

		status = EFI_CALL(BS->HandleProtocol, handles[i], &pxe_guid,
		    (void **)&pxe);
		if (status != EFI_SUCCESS)
			continue;

		if (pxe->Mode == NULL)
			continue;

		if (pxe->Mode->DhcpAckReceived) {
			dhcp = (EFI_PXE_BASE_CODE_DHCPV4_PACKET *)
			    &pxe->Mode->DhcpAck;
		}
		if (pxe->Mode->PxeReplyReceived) {
			dhcp = (EFI_PXE_BASE_CODE_DHCPV4_PACKET *)
			    &pxe->Mode->PxeReply;
		}

		if (dhcp) {
			memcpy(&bootip, dhcp->BootpYiAddr, sizeof(bootip));
			memcpy(&servip, dhcp->BootpSiAddr, sizeof(servip));
			memcpy(boothw, dhcp->BootpHwAddr, sizeof(boothw));
			bootmac = boothw;
			PXE = pxe;
			break;
		}
	}
}

/*
 * TFTP filesystem layer implementation.
 */
struct tftp_handle {
	unsigned char	*inbuf;	/* input buffer */
	size_t		 inbufsize;
	off_t		 inbufoff;
};

struct fs_ops tftp_fs = {
	tftp_open, tftp_close, tftp_read, tftp_write, tftp_seek,
	tftp_stat, tftp_readdir
};

int
tftp_open(char *path, struct open_file *f)
{
	struct tftp_handle *tftpfile;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS status;
	UINT64 size;

	if (PXE == NULL)
		return ENXIO;

	tftpfile = alloc(sizeof(*tftpfile));
	if (tftpfile == NULL)
		return ENOMEM;
	memset(tftpfile, 0, sizeof(*tftpfile));

	status = EFI_CALL(PXE->Mtftp, PXE, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
	    NULL, FALSE, &size, NULL, &servip, path, NULL, FALSE);
	if (status != EFI_SUCCESS) {
		free(tftpfile, sizeof(*tftpfile));
		return ENOENT;
	}
	tftpfile->inbufsize = size;

	if (tftpfile->inbufsize == 0)
		goto out;

	status = EFI_CALL(BS->AllocatePages, AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(tftpfile->inbufsize), &addr);
	if (status != EFI_SUCCESS) {
		free(tftpfile, sizeof(*tftpfile));
		return ENOMEM;
	}
	tftpfile->inbuf = (unsigned char *)((paddr_t)addr);

	status = EFI_CALL(PXE->Mtftp, PXE, EFI_PXE_BASE_CODE_TFTP_READ_FILE,
	    tftpfile->inbuf, FALSE, &size, NULL, &servip, path, NULL, FALSE);
	if (status != EFI_SUCCESS) {
		free(tftpfile, sizeof(*tftpfile));
		return ENXIO;
	}
out:
	f->f_fsdata = tftpfile;
	return 0;
}

int
tftp_close(struct open_file *f)
{
	struct tftp_handle *tftpfile = f->f_fsdata;

	if (tftpfile->inbuf != NULL)
		EFI_CALL(BS->FreePages, (paddr_t)tftpfile->inbuf,
		    EFI_SIZE_TO_PAGES(tftpfile->inbufsize));
	free(tftpfile, sizeof(*tftpfile));
	return 0;
}

int
tftp_read(struct open_file *f, void *addr, size_t size, size_t *resid)
{
	struct tftp_handle *tftpfile = f->f_fsdata;
	size_t toread;

	if (size > tftpfile->inbufsize - tftpfile->inbufoff)
		toread = tftpfile->inbufsize - tftpfile->inbufoff;
	else
		toread = size;

	if (toread != 0) {
		memcpy(addr, tftpfile->inbuf + tftpfile->inbufoff, toread);
		tftpfile->inbufoff += toread;
	}

	if (resid != NULL)
		*resid = size - toread;
	return 0;
}

int
tftp_write(struct open_file *f, void *start, size_t size, size_t *resid)
{
	return EROFS;
}

off_t
tftp_seek(struct open_file *f, off_t offset, int where)
{
	struct tftp_handle *tftpfile = f->f_fsdata;

	switch(where) {
	case SEEK_CUR:
		if (tftpfile->inbufoff + offset < 0 ||
		    tftpfile->inbufoff + offset > tftpfile->inbufsize) {
			errno = EOFFSET;
			break;
		}
		tftpfile->inbufoff += offset;
		return (tftpfile->inbufoff);
	case SEEK_SET:
		if (offset < 0 || offset > tftpfile->inbufsize) {
			errno = EOFFSET;
			break;
		}
		tftpfile->inbufoff = offset;
		return (tftpfile->inbufoff);
	case SEEK_END:
		tftpfile->inbufoff = tftpfile->inbufsize;
		return (tftpfile->inbufoff);
	default:
		errno = EINVAL;
	}
	return((off_t)-1);
}

int
tftp_stat(struct open_file *f, struct stat *sb)
{
	struct tftp_handle *tftpfile = f->f_fsdata;

	sb->st_mode = 0444;
	sb->st_nlink = 1;
	sb->st_uid = 0;
	sb->st_gid = 0;
	sb->st_size = tftpfile->inbufsize;

	return 0;
}

int
tftp_readdir(struct open_file *f, char *name)
{
	return EOPNOTSUPP;
}

/*
 * Dummy TFTP network device.
 */
int
tftpopen(struct open_file *f, ...)
{
	u_int unit, part;
	va_list ap;

	va_start(ap, f);
	unit = va_arg(ap, u_int);
	part = va_arg(ap, u_int);
	va_end(ap);

	/* No PXE set -> no PXE available */
	if (PXE == NULL)
		return 1;

	if (unit != 0)
		return 1;

	return 0;
}

int
tftpclose(struct open_file *f)
{
	return 0;
}

int
tftpioctl(struct open_file *f, u_long cmd, void *data)
{
	return EOPNOTSUPP;
}

int
tftpstrategy(void *devdata, int rw, daddr32_t blk, size_t size, void *buf,
	size_t *rsize)
{
	return EOPNOTSUPP;
}
