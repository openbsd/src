/* $OpenBSD: efi.h,v 1.3 2018/06/24 10:38:44 kettenis Exp $ */

/* Public Domain */

#ifndef _MACHINE_EFI_H_
#define _MACHINE_EFI_H_

typedef uint8_t		UINT8;
typedef int16_t		INT16;
typedef uint16_t	UINT16;
typedef uint32_t	UINT32;
typedef uint64_t	UINT64;
typedef u_long		UINTN;
typedef uint16_t	CHAR16;
typedef void		VOID;
typedef uint64_t	EFI_PHYSICAL_ADDRESS;
typedef uint64_t	EFI_VIRTUAL_ADDRESS;
typedef UINTN		EFI_STATUS;
typedef VOID		*EFI_HANDLE;

typedef VOID		*EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef VOID		*EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef VOID		*EFI_BOOT_SERVICES;

typedef struct {
	UINT32	Data1;
	UINT16	Data2;
	UINT16	Data3;
	UINT8	Data4[8];
} EFI_GUID;

#define EFI_ACPI_20_TABLE_GUID \
  { 0x8868e871, 0xe4f1, 0x11d3, \
    { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81} }

typedef enum {
	EfiReservedMemoryType,
	EfiLoaderCode,
	EfiLoaderData,
	EfiBootServicesCode,
	EfiBootServicesData,
	EfiRuntimeServicesCode,
	EfiRuntimeServicesData,
	EfiConventionalMemory,
	EfiUnusableMemory,
	EfiACPIReclaimMemory,
	EfiACPIMemoryNVS,
	EfiMemoryMappedIO,
	EfiMemoryMappedIOPortSpace,
	EfiPalCode,
	EfiPersistentMemory,
        EfiMaxMemoryType
} EFI_MEMORY_TYPE;

#define EFI_MEMORY_UC			0x0000000000000001
#define EFI_MEMORY_WC			0x0000000000000002
#define EFI_MEMORY_WT			0x0000000000000004
#define EFI_MEMORY_WB			0x0000000000000008
#define EFI_MEMORY_UCE			0x0000000000000010
#define EFI_MEMORY_WP			0x0000000000001000
#define EFI_MEMORY_RP			0x0000000000002000
#define EFI_MEMORY_XP			0x0000000000004000
#define EFI_MEMORY_NV			0x0000000000008000
#define EFI_MEMORY_MORE_RELIABLE	0x0000000000010000
#define EFI_MEMORY_RO			0x0000000000020000
#define EFI_MEMORY_RUNTIME		0x8000000000000000

#define EFI_MEMORY_DESCRIPTOR_VERSION  1

typedef struct {
	UINT32			Type;
	UINT32			Pad;
	EFI_PHYSICAL_ADDRESS	PhysicalStart;
	EFI_VIRTUAL_ADDRESS	VirtualStart;
	UINT64			NumberOfPages;
	UINT64			Attribute;
} EFI_MEMORY_DESCRIPTOR;

#define NextMemoryDescriptor(Ptr, Size) \
	((EFI_MEMORY_DESCRIPTOR *)(((UINT8 *)Ptr) + Size))

typedef struct {
	UINT64				Signature;
	UINT32				Revision;
	UINT32				HeaderSize;
	UINT32				CRC32;
	UINT32				Reserved;
} EFI_TABLE_HEADER;

typedef struct {
	UINT16				Year;
	UINT8				Month;
	UINT8				Day;
	UINT8				Hour;
	UINT8				Minute;
	UINT8				Second;
	UINT8				Pad1;
	UINT32				Nanosecond;
	INT16				TimeZone;
	UINT8				Daylight;
	UINT8				Pad2;
} EFI_TIME;

typedef VOID		*EFI_TIME_CAPABILITIES;

typedef EFI_STATUS (*EFI_GET_TIME)(EFI_TIME *, EFI_TIME_CAPABILITIES *);
typedef EFI_STATUS (*EFI_SET_TIME)(EFI_TIME *);
typedef EFI_STATUS (*EFI_SET_VIRTUAL_ADDRESS_MAP)(UINTN, UINTN, UINT32, EFI_MEMORY_DESCRIPTOR *);

typedef struct {
	EFI_TABLE_HEADER		Hdr;
	EFI_GET_TIME			GetTime;
	EFI_SET_TIME			SetTime;
	VOID				*GetWakeupTime;
	VOID				*SetWakeupTime;

	EFI_SET_VIRTUAL_ADDRESS_MAP	SetVirtualAddressMap;
} EFI_RUNTIME_SERVICES;

typedef struct {
	EFI_GUID			VendorGuid;
	VOID				*VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
	EFI_TABLE_HEADER		Hdr;
	CHAR16				*FirmwareVendor;
	UINT32				FirmwareRevision;
	EFI_HANDLE			ConsoleInHandle;
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL	*ConIn;
	EFI_HANDLE			ConsoleOutHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL	*ConOut;
	EFI_HANDLE			StandardErrorHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL	*StdErr;
	EFI_RUNTIME_SERVICES		*RuntimeServices;
	EFI_BOOT_SERVICES		*BootServices;
	UINTN				NumberOfTableEntries;
	EFI_CONFIGURATION_TABLE		*ConfigurationTable;
} EFI_SYSTEM_TABLE;

#define EFI_SUCCESS	0

#define	efi_guidcmp(_a, _b)	memcmp((_a), (_b), sizeof(EFI_GUID))

#endif /* _DEV_ACPI_EFI_H_ */
