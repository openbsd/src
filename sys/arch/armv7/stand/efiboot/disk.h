#ifndef _DISK_H
#define _DISK_H

typedef struct efi_diskinfo {
	EFI_BLOCK_IO		*blkio;
	UINT32			 mediaid;
} *efi_diskinfo_t;

struct diskinfo {
	struct efi_diskinfo ed;
	struct disklabel disklabel;

	u_int sc_part;
};

extern struct diskinfo diskinfo;

#endif /* _DISK_H */
