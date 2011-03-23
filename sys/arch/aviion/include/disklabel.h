/*	$OpenBSD: disklabel.h,v 1.11 2011/03/23 16:54:34 pirofti Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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

#ifndef	_MACHINE_DISKLABEL_H_
#define	_MACHINE_DISKLABEL_H_

#define	LABELSECTOR	1		/* sector containing label */
#define	LABELOFFSET	0		/* offset of label in sector */
#define	MAXPARTITIONS	16		/* number of partitions */

/*
 * AViiON native disk identification
 */

#define	VDM_SIGNATURE		0x1234abcd

#define	VDM_DISK_VERIFICATION_SECTOR		0
#define	VDM_DISK_VERIFICATION_OFFSET		0x1c8
#define	VDM_DISK_VERIFICATION_OFFSET_ALT	0x1c0

struct vdm_disk_verification {
	uint32_t	signature;
	uint32_t	version;
	uint32_t	unused[2];
};

#define	VDM_DISK_VERSION	0

struct vdm_boot_info {
	uint32_t	padding[6];
	uint32_t	signature;
	uint32_t	boot_start;
	uint32_t	boot_size;
	uint32_t	version;
};

#define	VDM_BOOT_INFO_VERSION	1
#define	VDM_BOOT_DEFAULT_SIZE	500

/*
 * MBR identification information is in <sys/disklabel.h>
 */

/* DG/UX VDM partition type */
#define	DOSPTYP_DGUX_VDM	0xdf

/*
 * DG/UX VDM structures
 */

#define	VDIT_SECTOR	1

struct vdm_self_id {
	union {
		uint8_t			_kind;
		uint32_t		_blkno;
	} u;
	uint32_t			node_number;
} __packed;

#define	VDM_ID_KIND(id)			((id)->u._kind)
#define	VDM_BLKNO_MASK			0x00ffffff	/* low 24 bits */
#define	VDM_ID_BLKNO(id)		((id)->u._blkno) & VDM_BLKNO_MASK)
#define	VDM_NO_NODE_NUMBER		012345670123

#define	VDIT_BLOCK			0x12
#define	VDIT_PORTION_HEADER_BLOCK	0x13
#define	VDIT_BLOCK_HEAD_BE		0x14
#define	VDIT_BLOCK_HEAD_LE		0x18

struct	vdit_block_header {
	struct vdm_self_id		id;
	uint32_t			nextblk;
	uint32_t			timestamp;
	uint32_t			secondary_vdit;
	uint16_t			chunksz;
	uint16_t			padding;
} __packed;

struct vdit_entry_header {
	uint16_t			type;
	uint16_t			size;
	uint32_t			timestamp;
} __packed;

#define	VDIT_ENTRY_SENTINEL		0x00
#define	VDIT_ENTRY_UNUSED		0x01
#define	VDIT_ENTRY_BOOT_INFO		0x02
#define	VDIT_ENTRY_SUBDRIVER_INFO	0x03
#define	VDIT_ENTRY_INSTANCE		0x04

#define	VDIT_NAME_MAX 0x20

struct vdit_instance_id {
	uint32_t			generation_timestamp;
	uint32_t			system_id;
} __packed;

struct vdit_boot_info_entry {
	uint16_t			version;
	struct vdit_instance_id		default_swap;
	struct vdit_instance_id		default_root;
} __packed;

struct vdit_subdriver_entry {
	uint16_t			version;
	uint32_t			subdriver_id;
	char				name[VDIT_NAME_MAX];
} __packed;

#define	VDM_SUBDRIVER_VDMPHYS		"vdmphys"
#define	VDM_SUBDRIVER_VDMPART		"vdmpart"
#define	VDM_SUBDRIVER_VDMAGGR		"vdmaggr"
#define	VDM_SUBDRIVER_VDMREMAP		"vdmremap"

struct vdit_instance_entry {
	uint16_t			version;
	char				name[VDIT_NAME_MAX];
	uint32_t			subdriver_id;
	struct vdit_instance_id		instance_id;
	uint8_t				exported;
} __packed;

#define	VDM_INSTANCE_OPENBSD		"OpenBSD"

struct vdit_vdmphys_instance {
	struct vdit_instance_entry	instance;
	uint16_t			version;
	uint16_t			mode;
#define	VDMPHYS_MODE_READONLY	0x00
#define	VDMPHYS_MODE_READWRITE	0x01
} __packed;

struct vdit_vdmpart_instance {
	struct vdit_instance_entry	instance;
	uint16_t			version;
	struct vdit_instance_id		child_instance;
	uint32_t			start_blkno;
	uint32_t			size;
	struct vdit_instance_id		remap_instance;
} __packed;

struct vdit_vdmaggr_instance {
	struct vdit_instance_entry	instance;
	uint16_t			version;
	uint16_t			aggr_count;
	uint32_t			stripe_size;
	struct vdit_instance_id		pieces[0];
} __packed;

struct vdit_vdmremap_instance {
	struct vdit_instance_entry	instance;
	uint16_t			version;
	struct vdit_instance_id		primary_remap_table;
	struct vdit_instance_id		secondary_remap_table;
	struct vdit_instance_id		remap_area;
} __packed;

#endif	/* _MACHINE_DISKLABEL_H_ */
