/*	$OpenBSD: efiboot.c,v 1.7 2015/11/03 10:56:38 yasuoka Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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
#include <cmd.h>
#include <stand/boot/bootarg.h>

#include "libsa.h"
#include "disk.h"

#include <efi.h>
#include <efiapi.h>
#include <efiprot.h>
#include <eficonsctl.h>

#include "efidev.h"
#include "efiboot.h"
#include "eficall.h"
#include "run_i386.h"

#define	KERN_LOADSPACE_SIZE	(32 * 1024 * 1024)

EFI_SYSTEM_TABLE	*ST;
EFI_BOOT_SERVICES	*BS;
EFI_RUNTIME_SERVICES	*RS;
EFI_HANDLE		 IH, efi_bootdp = NULL;
EFI_PHYSICAL_ADDRESS	 heap;
EFI_LOADED_IMAGE	*loadedImage;
UINTN			 heapsiz = 1 * 1024 * 1024;
UINTN			 mmap_key;
static EFI_GUID		 imgp_guid = LOADED_IMAGE_PROTOCOL;
static EFI_GUID		 blkio_guid = BLOCK_IO_PROTOCOL;
static EFI_GUID		 devp_guid = DEVICE_PATH_PROTOCOL;
u_long			 efi_loadaddr;

static void	 efi_heap_init(void);
static void	 efi_memprobe_internal(void);
static void	 efi_video_init(void);
static void	 efi_video_reset(void);
EFI_STATUS	 efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE *);

void (*run_i386)(u_long, u_long, int, int, int, int, int, int, int, int)
    __attribute__((noreturn));

extern int bios_bootdev;

EFI_STATUS
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	extern char		*progname;
	EFI_LOADED_IMAGE	*imgp;
	EFI_DEVICE_PATH		*dp0 = NULL, *dp;
	EFI_STATUS		 status;
	EFI_PHYSICAL_ADDRESS	 stack;

	ST = systab;
	BS = ST->BootServices;
	RS = ST->RuntimeServices;
	IH = image;

	efi_video_init();
	efi_heap_init();

	status = EFI_CALL(BS->HandleProtocol, image, &imgp_guid,
	    (void **)&imgp);
	if (status == EFI_SUCCESS)
		status = EFI_CALL(BS->HandleProtocol, imgp->DeviceHandle,
		    &devp_guid, (void **)&dp0);
	if (status == EFI_SUCCESS) {
		for (dp = dp0; !IsDevicePathEnd(dp);
		    dp = NextDevicePathNode(dp)) {
			if (DevicePathType(dp) == MEDIA_DEVICE_PATH)
				continue;
			if (DevicePathSubType(dp) == MEDIA_HARDDRIVE_DP) {
				bios_bootdev = 0x80;
				efi_bootdp = dp;
				break;
			}
			break;
		}
	}

#ifdef __amd64__
	/* allocate run_i386_start() on heap */
	if ((run_i386 = alloc(run_i386_size)) == NULL)
		panic("alloc() failed");
	memcpy(run_i386, run_i386_start, run_i386_size);
#endif

	/* can't use sa_cleanup since printf is used after sa_cleanup() */
	/* sa_cleanup = efi_cleanup; */

	progname = "EFIBOOT";

	/*
	 * Move the stack before calling boot().  UEFI on some machines
	 * locate the stack on our kernel load address.
	 */
	stack = heap + heapsiz;
#if defined(__amd64__)
	asm("movq	%0, %%rsp;"
	    "mov	%1, %%edi;"
	    "call	boot;"
	    :: "r"(stack - 32), "r"(bios_bootdev));
#else
	asm("movl	%0, %%esp;"
	    "movl	%1, (%%esp);"
	    "call	boot;"
	    :: "r"(stack - 32), "r"(bios_bootdev));
#endif
	/* must not reach here */
	return (EFI_SUCCESS);
}

void
efi_cleanup(void)
{
	EFI_STATUS	 status;

	efi_memprobe_internal();	/* sync the current map */
	status = EFI_CALL(BS->ExitBootServices, IH, mmap_key);
	if (status != EFI_SUCCESS)
		panic("ExitBootServices");
}

/***********************************************************************
 * Disk
 ***********************************************************************/
struct disklist_lh efi_disklist;

void
efi_diskprobe(void)
{
	int			 i, bootdev;
	UINTN			 sz;
	EFI_STATUS		 status;
	EFI_HANDLE		*handles = NULL;
	EFI_BLOCK_IO		*blkio;
	EFI_BLOCK_IO_MEDIA	*media;
	struct diskinfo		*di;
	EFI_DEVICE_PATH		*dp, *bp;

	TAILQ_INIT(&efi_disklist);

	sz = 0;
	status = EFI_CALL(BS->LocateHandle, ByProtocol, &blkio_guid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		handles = alloc(sz);
		status = EFI_CALL(BS->LocateHandle, ByProtocol, &blkio_guid,
		    0, &sz, handles);
	}
	if (handles == NULL || EFI_ERROR(status))
		panic("BS->LocateHandle() returns %d", status);

	for (i = 0; i < sz / sizeof(EFI_HANDLE); i++) {
		bootdev = 0;
		status = EFI_CALL(BS->HandleProtocol, handles[i], &blkio_guid,
		    (void **)&blkio);
		if (EFI_ERROR(status))
			panic("BS->HandleProtocol() returns %d", status);

		media = blkio->Media;
		if (media->LogicalPartition)
			continue;
		di = alloc(sizeof(struct diskinfo));
		efid_init(di, blkio);

		if (efi_bootdp == NULL)
			goto next;
		status = EFI_CALL(BS->HandleProtocol, handles[i], &devp_guid,
		    (void **)&dp);
		if (EFI_ERROR(status))
			goto next;
		bp = efi_bootdp;
		while (1) {
			if (IsDevicePathEnd(dp)) {
				bootdev = 1;
				break;
			}
			if (memcmp(dp, bp, sizeof(EFI_DEVICE_PATH)) != 0 ||
			    memcmp(dp, bp, DevicePathNodeLength(dp)) != 0)
				break;
			dp = NextDevicePathNode(dp);
			bp = NextDevicePathNode(bp);
		}
next:
		if (bootdev)
			TAILQ_INSERT_HEAD(&efi_disklist, di, list);
		else
			TAILQ_INSERT_TAIL(&efi_disklist, di, list);
	}

	free(handles, sz);
}

/***********************************************************************
 * Memory
 ***********************************************************************/
bios_memmap_t		 bios_memmap[64];

static void
efi_heap_init(void)
{
	EFI_STATUS	 status;

	heap = HEAP_LIMIT;
	status = EFI_CALL(BS->AllocatePages, AllocateMaxAddress, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(heapsiz), &heap);
	if (status != EFI_SUCCESS)
		panic("BS->AllocatePages()");
}

void
efi_memprobe(void)
{
	u_int		 n = 0;
	bios_memmap_t	*bm;
	EFI_STATUS	 status;
	EFI_PHYSICAL_ADDRESS
			 addr = 0x10000000ULL;	/* Below 256MB */

	status = EFI_CALL(BS->AllocatePages, AllocateMaxAddress, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(KERN_LOADSPACE_SIZE), &addr);
	if (status != EFI_SUCCESS)
		panic("BS->AllocatePages()");
	efi_loadaddr = addr;

	printf(" mem[");
	efi_memprobe_internal();
	for (bm = bios_memmap; bm->type != BIOS_MAP_END; bm++) {
		if (bm->type == BIOS_MAP_FREE && bm->size > 12 * 1024) {
			if (n++ != 0)
				printf(" ");
			if (bm->size > 1024 * 1024)
				printf("%uM", bm->size / 1024 / 1024);
			else
				printf("%uK", bm->size / 1024);
		}
	}
	printf("]");
}

static void
efi_memprobe_internal(void)
{
	EFI_STATUS		 status;
	UINTN			 mapkey, mmsiz, siz;
	UINT32			 mmver;
	EFI_MEMORY_DESCRIPTOR	*mm0, *mm;
	int			 i, n;
	bios_memmap_t		*bm, bm0;

	cnvmem = extmem = 0;
	bios_memmap[0].type = BIOS_MAP_END;

	siz = 0;
	status = EFI_CALL(BS->GetMemoryMap, &siz, NULL, &mapkey, &mmsiz,
	    &mmver);
	if (status != EFI_BUFFER_TOO_SMALL)
		panic("cannot get the size of memory map");
	mm0 = alloc(siz);
	status = EFI_CALL(BS->GetMemoryMap, &siz, mm0, &mapkey, &mmsiz, &mmver);
	if (status != EFI_SUCCESS)
		panic("cannot get the memory map");
	n = siz / mmsiz;
	mmap_key = mapkey;

	for (i = 0, mm = mm0; i < n; i++, mm = NextMemoryDescriptor(mm, mmsiz)){
		bm0.type = BIOS_MAP_END;
		bm0.addr = mm->PhysicalStart;
		bm0.size = mm->NumberOfPages * EFI_PAGE_SIZE;
		if (mm->Type == EfiReservedMemoryType ||
		    mm->Type == EfiUnusableMemory ||
		    mm->Type == EfiRuntimeServicesCode ||
		    mm->Type == EfiRuntimeServicesData)
			bm0.type = BIOS_MAP_RES;
		else if (mm->Type == EfiLoaderCode ||
		    mm->Type == EfiLoaderData ||
		    mm->Type == EfiBootServicesCode ||
		    mm->Type == EfiBootServicesData ||
		    mm->Type == EfiConventionalMemory)
			bm0.type = BIOS_MAP_FREE;
		else if (mm->Type == EfiACPIReclaimMemory)
			bm0.type = BIOS_MAP_ACPI;
		else if (mm->Type == EfiACPIMemoryNVS)
			bm0.type = BIOS_MAP_NVS;
		else
			/*
			 * XXX Is there anything to do for EfiMemoryMappedIO
			 * XXX EfiMemoryMappedIOPortSpace EfiPalCode?
			 */
			bm0.type = BIOS_MAP_RES;

		for (bm = bios_memmap; bm->type != BIOS_MAP_END; bm++) {
			if (bm->type != bm0.type)
				continue;
			if (bm->addr <= bm0.addr &&
			    bm0.addr <= bm->addr + bm->size) {
				bm->size = bm0.addr + bm0.size - bm->addr;
				break;
			} else if (bm0.addr <= bm->addr &&
			    bm->addr <= bm0.addr + bm0.size) {
				bm->size = bm->addr + bm->size - bm0.addr;
				bm->addr = bm0.addr;
				break;
			}
		}
		if (bm->type == BIOS_MAP_END) {
			*bm = bm0;
			(++bm)->type = BIOS_MAP_END;
		}
	}
	for (bm = bios_memmap; bm->type != BIOS_MAP_END; bm++) {
		if (bm->addr < 0x0a0000)	/* Below memory hole */
			cnvmem =
			    max(cnvmem, (bm->addr + bm->size) / 1024);
		if (bm->addr >= 0x10000 /* Above the memory hole */ &&
		    bm->addr / 1024 == extmem + 1024)
			extmem += bm->size / 1024;
	}
	free(mm0, siz);
}

/***********************************************************************
 * Console
 ***********************************************************************/
static SIMPLE_TEXT_OUTPUT_INTERFACE     *conout = NULL;
static SIMPLE_INPUT_INTERFACE           *conin;
static EFI_GUID				 con_guid
					    = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static EFI_GUID				 gop_guid
					    = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
struct efi_video {
	int	cols;
	int	rows;
} efi_video[32];

static void
efi_video_init(void)
{
	EFI_CONSOLE_CONTROL_PROTOCOL	*conctrl = NULL;
	int				 i, mode80x25, mode100x31;
	UINTN				 cols, rows;
	EFI_STATUS			 status;

	conout = ST->ConOut;
	status = EFI_CALL(BS->LocateProtocol, &con_guid, NULL,
	    (void **)&conctrl);
	if (status == EFI_SUCCESS)
		(void)EFI_CALL(conctrl->SetMode, conctrl,
			EfiConsoleControlScreenText);
	mode80x25 = -1;
	mode100x31 = -1;
	for (i = 0; ; i++) {
		status = EFI_CALL(conout->QueryMode, conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			break;
		if (mode80x25 < 0 && cols == 80 && rows == 25)
			mode80x25 = i;
		if (mode100x31 < 0 && cols == 100 && rows == 31)
			mode100x31 = i;
		if (i < nitems(efi_video)) {
			efi_video[i].cols = cols;
			efi_video[i].rows = rows;
		}
	}
	if (mode100x31 >= 0)
		EFI_CALL(conout->SetMode, conout, mode100x31);
	else if (mode80x25 >= 0)
		EFI_CALL(conout->SetMode, conout, mode80x25);
	conin = ST->ConIn;
	efi_video_reset();
}

static void
efi_video_reset(void)
{
	EFI_CALL(conout->EnableCursor, conout, TRUE);
	EFI_CALL(conout->SetAttribute, conout,
	    EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	EFI_CALL(conout->ClearScreen, conout);
}

void
efi_cons_probe(struct consdev *cn)
{
	cn->cn_pri = CN_MIDPRI;
	cn->cn_dev = makedev(12, 0);
	printf(" pc%d", minor(cn->cn_dev));
}

void
efi_cons_init(struct consdev *cp)
{
}

int
efi_cons_getc(dev_t dev)
{
	EFI_INPUT_KEY	 key;
	EFI_STATUS	 status;
	UINTN		 dummy;
	static int	 lastchar = 0;

	if (lastchar) {
		int r = lastchar;
		if ((dev & 0x80) == 0)
			lastchar = 0;
		return (r);
	}

	status = EFI_CALL(conin->ReadKeyStroke, conin, &key);
	while (status == EFI_NOT_READY) {
		if (dev & 0x80)
			return (0);
		EFI_CALL(BS->WaitForEvent, 1, &conin->WaitForKey, &dummy);
		status = EFI_CALL(conin->ReadKeyStroke, conin, &key);
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

	EFI_CALL(conout->OutputString, conout, buf);
}

int
efi_cons_getshifts(dev_t dev)
{
	/* XXX */
	return (0);
}

/* XXX: serial console is not supporte yet */
int comspeed = 9600;
int com_addr = -1;
int com_speed = -1;

/***********************************************************************
 * Miscellaneous
 ***********************************************************************/
/*
 * ACPI GUID is confusing in UEFI spec.
 * {EFI_,}_ACPI_20_TABLE_GUID or EFI_ACPI_TABLE_GUID means
 * ACPI 2.0 or abobe.
 */
static EFI_GUID acpi_guid = ACPI_20_TABLE_GUID;
static EFI_GUID smbios_guid = SMBIOS_TABLE_GUID;

#define	efi_guidcmp(_a, _b)	memcmp((_a), (_b), sizeof(EFI_GUID))

void
efi_makebootargs(void)
{
	int			 i;
	EFI_STATUS		 status;
	EFI_GRAPHICS_OUTPUT	*gop;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION
				*gopi;
	bios_efiinfo_t		 ei;
	int			 bestmode = -1;
	UINTN			 sz, gopsiz, bestsiz = 0;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION
				*info;

	memset(&ei, 0, sizeof(ei));
	/*
	 * ACPI, BIOS configuration table
	 */
	for (i = 0; i < ST->NumberOfTableEntries; i++) {
		if (efi_guidcmp(&acpi_guid,
		    &ST->ConfigurationTable[i].VendorGuid) == 0)
			ei.config_acpi = (intptr_t)
			    ST->ConfigurationTable[i].VendorTable;
		else if (efi_guidcmp(&smbios_guid,
		    &ST->ConfigurationTable[i].VendorGuid) == 0)
			ei.config_smbios = (intptr_t)
			    ST->ConfigurationTable[i].VendorTable;
	}

	/*
	 * Frame buffer
	 */
	status = EFI_CALL(BS->LocateProtocol, &gop_guid, NULL,
	    (void **)&gop);
	if (!EFI_ERROR(status)) {
		for (i = 0; ; i++) {
			status = EFI_CALL(gop->QueryMode, gop, i, &sz, &info);
			if (EFI_ERROR(status))
				break;
			gopsiz = info->HorizontalResolution *
			    info->VerticalResolution;
			if (gopsiz > bestsiz) {
				bestmode = i;
				bestsiz = gopsiz;
			}
		}
		if (bestmode >= 0) {
			status = EFI_CALL(gop->SetMode, gop, bestmode);
			if (EFI_ERROR(status))
				panic("GOP setmode failed(%d)", status);
		}

		gopi = gop->Mode->Info;
		switch (gopi->PixelFormat) {
		case PixelBlueGreenRedReserved8BitPerColor:
			ei.fb_red_mask      = 0x00ff0000;
			ei.fb_green_mask    = 0x0000ff00;
			ei.fb_blue_mask     = 0x000000ff;
			ei.fb_reserved_mask = 0xff000000;
			break;
		case PixelRedGreenBlueReserved8BitPerColor:
			ei.fb_red_mask      = 0x000000ff;
			ei.fb_green_mask    = 0x0000ff00;
			ei.fb_blue_mask     = 0x00ff0000;
			ei.fb_reserved_mask = 0xff000000;
			break;
		case PixelBitMask:
			ei.fb_red_mask = gopi->PixelInformation.RedMask;
			ei.fb_green_mask = gopi->PixelInformation.GreenMask;
			ei.fb_blue_mask = gopi->PixelInformation.BlueMask;
			ei.fb_reserved_mask =
			    gopi->PixelInformation.ReservedMask;
			break;
		default:
			break;
		}
		ei.fb_addr = gop->Mode->FrameBufferBase;
		ei.fb_size = gop->Mode->FrameBufferSize;
		ei.fb_height = gopi->VerticalResolution;
		ei.fb_width = gopi->HorizontalResolution;
		ei.fb_pixpsl = gopi->PixelsPerScanLine;
	}

	addbootarg(BOOTARG_EFIINFO, sizeof(ei), &ei);
}

void
_rtt(void)
{
#ifdef EFI_DEBUG
	printf("Hit any key to reboot\n");
	efi_cons_getc(0);
#endif
	EFI_CALL(RS->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
	while (1) { }
}

time_t
getsecs(void)
{
	EFI_TIME		t;
	time_t			r = 0;
	int			y = 0;
	int			daytab[][14] = {
	    { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	    { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	};
#define isleap(_y) (((_y) % 4) == 0 && (((_y) % 100) != 0 || ((_y) % 400) == 0))

	EFI_CALL(ST->RuntimeServices->GetTime, &t, NULL);

	/* Calc days from UNIX epoch */
	r = (t.Year - 1970) * 365;
	for (y = 1970; y < t.Year; y++) {
		if (isleap(y))
			r++;
	}
	r += daytab[isleap(t.Year)? 1 : 0][t.Month] + t.Day;

	/* Calc secs */
	r *= 60 * 60 * 24;
	r += ((t.Hour * 60) + t.Minute) * 60 + t.Second;
	if (-24 * 60 < t.TimeZone && t.TimeZone < 24 * 60)
		r += t.TimeZone * 60;

	return (r);
}

/***********************************************************************
 * Commands
 ***********************************************************************/
int
Xexit_efi(void)
{
	EFI_CALL(BS->Exit, IH, 0, 0, NULL);
	while (1) { }
	return (0);
}

int
Xvideo_efi(void)
{
	int	 i, mode = -1;
	char	*p;

	for (i = 0; i < nitems(efi_video) && efi_video[i].cols > 0; i++) {
		printf("Mode %d: %d x %d\n", i,
		    efi_video[i].cols, efi_video[i].rows);
	}
	if (cmd.argc == 2) {
		p = cmd.argv[1];
		mode = strtol(p, &p, 10);
	}
	printf("\nCurrent Mode = %d\n", conout->Mode->Mode);
	if (0 <= mode && mode < i) {
		EFI_CALL(conout->SetMode, conout, mode);
		efi_video_reset();
	}

	return (0);
}

int
Xpoweroff_efi(void)
{
	EFI_CALL(RS->ResetSystem, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
	return (0);
}
