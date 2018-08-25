/*	$OpenBSD: efiboot.c,v 1.23 2018/08/25 00:12:14 yasuoka Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
 * Copyright (c) 2016 Mark Kettenis
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
#include <sys/queue.h>
#include <dev/cons.h>
#include <sys/disklabel.h>

#include <efi.h>
#include <efiapi.h>
#include <efiprot.h>
#include <eficonsctl.h>

#include <lib/libkern/libkern.h>
#include <stand/boot/cmd.h>

#include "disk.h"
#include "efiboot.h"
#include "eficall.h"
#include "fdt.h"
#include "libsa.h"

EFI_SYSTEM_TABLE	*ST;
EFI_BOOT_SERVICES	*BS;
EFI_RUNTIME_SERVICES	*RS;
EFI_HANDLE		 IH, efi_bootdp;

EFI_PHYSICAL_ADDRESS	 heap;
UINTN			 heapsiz = 1 * 1024 * 1024;
EFI_MEMORY_DESCRIPTOR	*mmap;
UINTN			 mmap_key;
UINTN			 mmap_ndesc;
UINTN			 mmap_descsiz;
UINT32			 mmap_version;

static EFI_GUID		 imgp_guid = LOADED_IMAGE_PROTOCOL;
static EFI_GUID		 blkio_guid = BLOCK_IO_PROTOCOL;
static EFI_GUID		 devp_guid = DEVICE_PATH_PROTOCOL;

int efi_device_path_depth(EFI_DEVICE_PATH *dp, int);
int efi_device_path_ncmp(EFI_DEVICE_PATH *, EFI_DEVICE_PATH *, int);
static void efi_heap_init(void);
static void efi_memprobe_internal(void);
static void efi_timer_init(void);
static void efi_timer_cleanup(void);

EFI_STATUS
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	extern char		*progname;
	EFI_LOADED_IMAGE	*imgp;
	EFI_DEVICE_PATH		*dp = NULL;
	EFI_STATUS		 status;

	ST = systab;
	BS = ST->BootServices;
	RS = ST->RuntimeServices;
	IH = image;

	/* disable reset by watchdog after 5 minutes */
	EFI_CALL(BS->SetWatchdogTimer, 0, 0, 0, NULL);

	status = EFI_CALL(BS->HandleProtocol, image, &imgp_guid,
	    (void **)&imgp);
	if (status == EFI_SUCCESS)
		status = EFI_CALL(BS->HandleProtocol, imgp->DeviceHandle,
		    &devp_guid, (void **)&dp);
	if (status == EFI_SUCCESS)
		efi_bootdp = dp;

	progname = "BOOTARM";

	boot(0);

	return (EFI_SUCCESS);
}

static SIMPLE_TEXT_OUTPUT_INTERFACE *conout;
static SIMPLE_INPUT_INTERFACE *conin;

void
efi_cons_probe(struct consdev *cn)
{
	cn->cn_pri = CN_MIDPRI;
	cn->cn_dev = makedev(12, 0);
}

void
efi_cons_init(struct consdev *cp)
{
	conin = ST->ConIn;
	conout = ST->ConOut;
}

int
efi_cons_getc(dev_t dev)
{
	EFI_INPUT_KEY	 key;
	EFI_STATUS	 status;
#if 0
	UINTN		 dummy;
#endif
	static int	 lastchar = 0;

	if (lastchar) {
		int r = lastchar;
		if ((dev & 0x80) == 0)
			lastchar = 0;
		return (r);
	}

	status = conin->ReadKeyStroke(conin, &key);
	while (status == EFI_NOT_READY || key.UnicodeChar == 0) {
		if (dev & 0x80)
			return (0);
		/*
		 * XXX The implementation of WaitForEvent() in U-boot
		 * is broken and neverreturns.
		 */
#if 0
		BS->WaitForEvent(1, &conin->WaitForKey, &dummy);
#endif
		status = conin->ReadKeyStroke(conin, &key);
	}

	if (dev & 0x80)
		lastchar = key.UnicodeChar;

	return (key.UnicodeChar);
}

void
efi_cons_putc(dev_t dev, int c)
{
	CHAR16	buf[2];

	if (c == '\n')
		efi_cons_putc(dev, '\r');

	buf[0] = c;
	buf[1] = 0;

	conout->OutputString(conout, buf);
}

static void
efi_heap_init(void)
{
	EFI_STATUS	 status;

	status = EFI_CALL(BS->AllocatePages, AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(heapsiz), &heap);
	if (status != EFI_SUCCESS)
		panic("BS->AllocatePages()");
}

EFI_BLOCK_IO	*disk;

void
efi_diskprobe(void)
{
	int			 i, depth = -1;
	UINTN			 sz;
	EFI_STATUS		 status;
	EFI_HANDLE		*handles = NULL;
	EFI_BLOCK_IO		*blkio;
	EFI_BLOCK_IO_MEDIA	*media;
	EFI_DEVICE_PATH		*dp;

	sz = 0;
	status = EFI_CALL(BS->LocateHandle, ByProtocol, &blkio_guid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		handles = alloc(sz);
		status = EFI_CALL(BS->LocateHandle, ByProtocol, &blkio_guid,
		    0, &sz, handles);
	}
	if (handles == NULL || EFI_ERROR(status))
		return;

	if (efi_bootdp != NULL)
		depth = efi_device_path_depth(efi_bootdp, MEDIA_DEVICE_PATH);

	/*
	 * U-Boot incorrectly represents devices with a single
	 * MEDIA_DEVICE_PATH component.  In that case include that
	 * component into the matching, otherwise we'll blindly select
	 * the first device.
	 */
	if (depth == 0)
		depth = 1;

	for (i = 0; i < sz / sizeof(EFI_HANDLE); i++) {
		status = EFI_CALL(BS->HandleProtocol, handles[i], &blkio_guid,
		    (void **)&blkio);
		if (EFI_ERROR(status))
			panic("BS->HandleProtocol() returns %d", status);

		media = blkio->Media;
		if (media->LogicalPartition || !media->MediaPresent)
			continue;

		if (efi_bootdp == NULL || depth == -1)
			continue;
		status = EFI_CALL(BS->HandleProtocol, handles[i], &devp_guid,
		    (void **)&dp);
		if (EFI_ERROR(status))
			continue;
		if (efi_device_path_ncmp(efi_bootdp, dp, depth) == 0) {
			disk = blkio;
			break;
		}
	}

	free(handles, sz);
}

/*
 * Determine the number of nodes up to, but not including, the first
 * node of the specified type.
 */
int
efi_device_path_depth(EFI_DEVICE_PATH *dp, int dptype)
{
	int	i;

	for (i = 0; !IsDevicePathEnd(dp); dp = NextDevicePathNode(dp), i++) {
		if (DevicePathType(dp) == dptype)
			return (i);
	}

	return (-1);
}

int
efi_device_path_ncmp(EFI_DEVICE_PATH *dpa, EFI_DEVICE_PATH *dpb, int deptn)
{
	int	 i, cmp;

	for (i = 0; i < deptn; i++) {
		if (IsDevicePathEnd(dpa) || IsDevicePathEnd(dpb))
			return ((IsDevicePathEnd(dpa) && IsDevicePathEnd(dpb))
			    ? 0 : (IsDevicePathEnd(dpa))? -1 : 1);
		cmp = DevicePathNodeLength(dpa) - DevicePathNodeLength(dpb);
		if (cmp)
			return (cmp);
		cmp = memcmp(dpa, dpb, DevicePathNodeLength(dpa));
		if (cmp)
			return (cmp);
		dpa = NextDevicePathNode(dpa);
		dpb = NextDevicePathNode(dpb);
	}

	return (0);
}

struct board_id {
	const char *name;
	uint32_t board_id;
};

struct board_id board_id_table[] = {
	{ "arm,vexpress",			2272 },
	{ "google,snow",			3774 },
	{ "google,spring",			3774 },
	{ "samsung,universal_c210",		2838 },
	{ "ti,am335x-bone",			3589 },
	{ "ti,omap3-beagle",			1546 },
	{ "ti,omap3-beagle-xm",			1546 },
	{ "ti,omap4-panda",			2791 },
};

char *bootmac = NULL;
static EFI_GUID fdt_guid = FDT_TABLE_GUID;

#define	efi_guidcmp(_a, _b)	memcmp((_a), (_b), sizeof(EFI_GUID))

void *
efi_makebootargs(char *bootargs, uint32_t *board_id)
{
	void *fdt = NULL;
	u_char bootduid[8];
	u_char zero[8] = { 0 };
	uint64_t uefi_system_table = htobe64((uintptr_t)ST);
	void *node;
	size_t len;
	int i;

	for (i = 0; i < ST->NumberOfTableEntries; i++) {
		if (efi_guidcmp(&fdt_guid,
		    &ST->ConfigurationTable[i].VendorGuid) == 0)
			fdt = ST->ConfigurationTable[i].VendorTable;
	}

	if (!fdt_init(fdt))
		return NULL;

	node = fdt_find_node("/chosen");
	if (!node)
		return NULL;

	len = strlen(bootargs) + 1;
	fdt_node_add_property(node, "bootargs", bootargs, len);

	/* Pass DUID of the boot disk. */
	memcpy(&bootduid, diskinfo.disklabel.d_uid, sizeof(bootduid));
	if (memcmp(bootduid, zero, sizeof(bootduid)) != 0) {
		fdt_node_add_property(node, "openbsd,bootduid", bootduid,
		    sizeof(bootduid));
	}

	/* Pass netboot interface address. */
	if (bootmac)
		fdt_node_add_property(node, "openbsd,bootmac", bootmac, 6);

	/* Pass EFI system table. */
	fdt_node_add_property(node, "openbsd,uefi-system-table",
	    &uefi_system_table, sizeof(uefi_system_table));

	/* Placeholders for EFI memory map. */
	fdt_node_add_property(node, "openbsd,uefi-mmap-start", zero, 8);
	fdt_node_add_property(node, "openbsd,uefi-mmap-size", zero, 4);
	fdt_node_add_property(node, "openbsd,uefi-mmap-desc-size", zero, 4);
	fdt_node_add_property(node, "openbsd,uefi-mmap-desc-ver", zero, 4);

	fdt_finalize();

	node = fdt_find_node("/");
	for (i = 0; i < nitems(board_id_table); i++) {
		if (fdt_node_is_compatible(node, board_id_table[i].name)) {
			*board_id = board_id_table[i].board_id;
			break;
		}
	}

	return fdt;
}

void
efi_updatefdt(void)
{
	uint64_t uefi_mmap_start = htobe64((uintptr_t)mmap);
	uint32_t uefi_mmap_size = htobe32(mmap_ndesc * mmap_descsiz);
	uint32_t uefi_mmap_desc_size = htobe32(mmap_descsiz);
	uint32_t uefi_mmap_desc_ver = htobe32(mmap_version);
	void *node;

	node = fdt_find_node("/chosen");
	if (!node)
		return;

	/* Pass EFI memory map. */
	fdt_node_set_property(node, "openbsd,uefi-mmap-start",
	    &uefi_mmap_start, sizeof(uefi_mmap_start));
	fdt_node_set_property(node, "openbsd,uefi-mmap-size",
	    &uefi_mmap_size, sizeof(uefi_mmap_size));
	fdt_node_set_property(node, "openbsd,uefi-mmap-desc-size",
	    &uefi_mmap_desc_size, sizeof(uefi_mmap_desc_size));
	fdt_node_set_property(node, "openbsd,uefi-mmap-desc-ver",
	    &uefi_mmap_desc_ver, sizeof(uefi_mmap_desc_ver));

	fdt_finalize();
}

u_long efi_loadaddr;

void
machdep(void)
{
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS status;

	cninit();

	/*
	 * The kernel expects to be loaded at offset 0x00300000 into a
	 * block of memory aligned on a 256MB boundary.  We allocate a
	 * block of 32MB of memory, which gives us plenty of room for
	 * growth.
	 */
	for (addr = 0x10000000; addr <= 0xf0000000; addr += 0x10000000) {
		status = BS->AllocatePages(AllocateAddress, EfiLoaderData,
		    EFI_SIZE_TO_PAGES(32 * 1024 * 1024), &addr);
		if (status == EFI_SUCCESS) {
			efi_loadaddr = addr;
			break;
		}
	}
	if (efi_loadaddr == 0)
		printf("Can't allocate memory\n");

	efi_heap_init();
	efi_timer_init();
	efi_diskprobe();
	efi_pxeprobe();
}

void
efi_cleanup(void)
{
	int		 retry;
	EFI_STATUS	 status;

	efi_timer_cleanup();

	/* retry once in case of failure */
	for (retry = 1; retry >= 0; retry--) {
		efi_memprobe_internal();	/* sync the current map */
		efi_updatefdt();
		status = EFI_CALL(BS->ExitBootServices, IH, mmap_key);
		if (status == EFI_SUCCESS)
			break;
		if (retry == 0)
			panic("ExitBootServices failed (%d)", status);
	}
}

void
_rtt(void)
{
#ifdef EFI_DEBUG
	printf("Hit any key to reboot\n");
	efi_cons_getc(0);
#endif
	/*
	 * XXX ResetSystem doesn't seem to work on U-Boot 2017.03 on
	 * the CuBox-i.  So trigger an unimplemented instruction trap
	 * instead.
	 */
#if 1
	asm volatile(".word 0xa000f7f0\n");
#else
	RS->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
#endif
	for (;;)
		continue;
}

/*
 * U-Boot only implements the GetTime() Runtime Service if it has been
 * configured with CONFIG_DM_RTC.  Most board configurations don't
 * include that option, so we can't use it to implement our boot
 * prompt timeout.  Instead we use timer events to simulate a clock
 * that ticks ever second.
 */

EFI_EVENT timer;
int ticks;

static VOID
efi_timer(EFI_EVENT event, VOID *context)
{
	ticks++;
}

static void
efi_timer_init(void)
{
	EFI_STATUS status;

	status = BS->CreateEvent(EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
	    efi_timer, NULL, &timer);
	if (status == EFI_SUCCESS)
		status = BS->SetTimer(timer, TimerPeriodic, 10000000);
	if (EFI_ERROR(status))
		printf("Can't create timer\n");
}

static void
efi_timer_cleanup(void)
{
	BS->CloseEvent(timer);
}

time_t
getsecs(void)
{
	return ticks;
}

/*
 * Various device-related bits.
 */

void
devboot(dev_t dev, char *p)
{
	if (disk)
		strlcpy(p, "sd0a", 5);
	else
		strlcpy(p, "tftp0a", 7);
}

int
cnspeed(dev_t dev, int sp)
{
	return 115200;
}

char *
ttyname(int fd)
{
	return "com0";
}

dev_t
ttydev(char *name)
{
	return NODEV;
}

#define MAXDEVNAME	16

/*
 * Parse a device spec.
 *
 * [A-Za-z]*[0-9]*[A-Za-z]:file
 *    dev   uint    part
 */
int
devparse(const char *fname, int *dev, int *unit, int *part, const char **file)
{
	const char *s;

	*unit = 0;	/* default to wd0a */
	*part = 0;
	*dev  = 0;

	s = strchr(fname, ':');
	if (s != NULL) {
		int devlen;
		int i, u, p = 0;
		struct devsw *dp;
		char devname[MAXDEVNAME];

		devlen = s - fname;
		if (devlen > MAXDEVNAME)
			return (EINVAL);

		/* extract device name */
		for (i = 0; isalpha(fname[i]) && (i < devlen); i++)
			devname[i] = fname[i];
		devname[i] = 0;

		if (!isdigit(fname[i]))
			return (EUNIT);

		/* device number */
		for (u = 0; isdigit(fname[i]) && (i < devlen); i++)
			u = u * 10 + (fname[i] - '0');

		if (!isalpha(fname[i]))
			return (EPART);

		/* partition number */
		if (i < devlen)
			p = fname[i++] - 'a';

		if (i != devlen)
			return (ENXIO);

		/* check device name */
		for (dp = devsw, i = 0; i < ndevs; dp++, i++) {
			if (dp->dv_name && !strcmp(devname, dp->dv_name))
				break;
		}

		if (i >= ndevs)
			return (ENXIO);

		*unit = u;
		*part = p;
		*dev  = i;
		fname = ++s;
	}

	*file = fname;

	return (0);
}

int
devopen(struct open_file *f, const char *fname, char **file)
{
	struct devsw *dp;
	int dev, unit, part, error;

	error = devparse(fname, &dev, &unit, &part, (const char **)file);
	if (error)
		return (error);

	dp = &devsw[dev];
	f->f_dev = dp;

	return (*dp->dv_open)(f, unit, part);
}

static void
efi_memprobe_internal(void)
{
	EFI_STATUS		 status;
	UINTN			 mapkey, mmsiz, siz;
	UINT32			 mmver;
	EFI_MEMORY_DESCRIPTOR	*mm;
	int			 n;

	free(mmap, mmap_ndesc * mmap_descsiz);

	siz = 0;
	status = EFI_CALL(BS->GetMemoryMap, &siz, NULL, &mapkey, &mmsiz,
	    &mmver);
	if (status != EFI_BUFFER_TOO_SMALL)
		panic("cannot get the size of memory map");
	mm = alloc(siz);
	status = EFI_CALL(BS->GetMemoryMap, &siz, mm, &mapkey, &mmsiz, &mmver);
	if (status != EFI_SUCCESS)
		panic("cannot get the memory map");
	n = siz / mmsiz;
	mmap = mm;
	mmap_key = mapkey;
	mmap_ndesc = n;
	mmap_descsiz = mmsiz;
	mmap_version = mmver;
}

/*
 * Commands
 */

int Xexit_efi(void);
int Xpoweroff_efi(void);

const struct cmd_table cmd_machine[] = {
	{ "exit",	CMDT_CMD, Xexit_efi },
	{ "poweroff",	CMDT_CMD, Xpoweroff_efi },
	{ NULL, 0 }
};

int
Xexit_efi(void)
{
	EFI_CALL(BS->Exit, IH, 0, 0, NULL);
	for (;;)
		continue;
	return (0);
}

int
Xpoweroff_efi(void)
{
	EFI_CALL(RS->ResetSystem, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
	return (0);
}
