/* $OpenBSD: efi.h,v 1.1 2017/05/25 21:21:18 kettenis Exp $ */

/* Public Domain */

#ifndef _MACHINE_EFI_H_
#define _MACHINE_EFI_H_

typedef uint8_t		UINT8;
typedef uint32_t	UINT32;
typedef uint64_t	UINT64;
typedef uint64_t	EFI_PHYSICAL_ADDRESS;
typedef uint64_t	EFI_VIRTUAL_ADDRESS;

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

#endif /* _MACHINE_EFI_H_ */
