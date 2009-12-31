/*	$OpenBSD: biovar.h,v 1.37 2009/12/31 14:00:45 jsing Exp $	*/

/*
 * Copyright (c) 2002 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2005 Marco Peereboom.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Devices getting ioctls through this interface should use ioctl class 'B'
 * and command numbers starting from 32, lower ones are reserved for generic
 * ioctls.  All ioctl data must be structures which start with a void *
 * cookie.
 */

#include <sys/types.h>

struct bio_common {
	void		*bc_cookie;
};

/* convert name to a cookie */
#define BIOCLOCATE _IOWR('B', 0, struct bio_locate)
struct bio_locate {
	void		*bl_cookie;
	char		*bl_name;
};

#ifdef _KERNEL
int	bio_register(struct device *, int (*)(struct device *, u_long,
    caddr_t));
void	bio_unregister(struct device *);
#endif

#define BIOCINQ _IOWR('B', 32, struct bioc_inq)
struct bioc_inq {
	void		*bi_cookie;

	char		bi_dev[16];	/* controller device */
	int		bi_novol;	/* nr of volumes */
	int		bi_nodisk;	/* nr of total disks */
};

#define BIOCDISK _IOWR('B', 33, struct bioc_disk)
/* structure that represents a disk in a RAID volume */
struct bioc_disk {
	void		*bd_cookie;

	u_int16_t	bd_channel;
	u_int16_t	bd_target;
	u_int16_t	bd_lun;
	u_int16_t	bd_other_id;	/* unused for now  */

	int		bd_volid;	/* associate with volume */
	int		bd_diskid;	/* virtual disk */
	int		bd_status;	/* current status */
#define BIOC_SDONLINE		0x00
#define BIOC_SDONLINE_S		"Online"
#define BIOC_SDOFFLINE		0x01
#define BIOC_SDOFFLINE_S	"Offline"
#define BIOC_SDFAILED		0x02
#define BIOC_SDFAILED_S 	"Failed"
#define BIOC_SDREBUILD		0x03
#define BIOC_SDREBUILD_S	"Rebuild"
#define BIOC_SDHOTSPARE		0x04
#define BIOC_SDHOTSPARE_S	"Hot spare"
#define BIOC_SDUNUSED		0x05
#define BIOC_SDUNUSED_S		"Unused"
#define BIOC_SDSCRUB		0x06
#define BIOC_SDSCRUB_S		"Scrubbing"
#define BIOC_SDINVALID		0xff
#define BIOC_SDINVALID_S	"Invalid"
	u_quad_t	bd_size;	/* size of the disk */

	char		bd_vendor[32];	/* scsi string */
	char		bd_serial[32];	/* serial number */
	char		bd_procdev[16];	/* processor device */
};

#define BIOCVOL _IOWR('B', 34, struct bioc_vol)
/* structure that represents a RAID volume */
struct bioc_vol {
	void		*bv_cookie;
	int		bv_volid;	/* volume id */

	int16_t		bv_percent;	/* percent done operation */
	u_int16_t	bv_seconds;	/* seconds of progress so far */

	int		bv_status;	/* current status */
#define BIOC_SVONLINE		0x00
#define BIOC_SVONLINE_S		"Online"
#define BIOC_SVOFFLINE		0x01
#define BIOC_SVOFFLINE_S	"Offline"
#define BIOC_SVDEGRADED		0x02
#define BIOC_SVDEGRADED_S	"Degraded"
#define BIOC_SVBUILDING		0x03
#define BIOC_SVBUILDING_S	"Building"
#define BIOC_SVSCRUB		0x04
#define BIOC_SVSCRUB_S		"Scrubbing"
#define BIOC_SVREBUILD		0x05
#define BIOC_SVREBUILD_S	"Rebuild"
#define BIOC_SVINVALID		0xff
#define BIOC_SVINVALID_S	"Invalid"
	u_quad_t	bv_size;	/* size of the disk */
	int		bv_level;	/* raid level */
	int		bv_nodisk;	/* nr of drives */

	char		bv_dev[16];	/* device */
	char		bv_vendor[32];	/* scsi string */
};

#define BIOCALARM _IOWR('B', 35, struct bioc_alarm)
struct bioc_alarm {
	void		*ba_cookie;
	int		ba_opcode;

	int		ba_status;	/* only used with get state */
#define BIOC_SADISABLE		0x00	/* disable alarm */
#define BIOC_SAENABLE		0x01	/* enable alarm */
#define BIOC_SASILENCE		0x02	/* silence alarm */
#define BIOC_GASTATUS		0x03	/* get status */
#define BIOC_SATEST		0x04	/* test alarm */
};

#define BIOCBLINK _IOWR('B', 36, struct bioc_blink)
struct bioc_blink {
	void		*bb_cookie;
	u_int16_t	bb_channel;
	u_int16_t	bb_target;

	int		bb_status;	/* current status */
#define BIOC_SBUNBLINK		0x00	/* disable blinking */
#define BIOC_SBBLINK		0x01	/* enable blink */
#define BIOC_SBALARM		0x02	/* enable alarm blink */
};

#define BIOCSETSTATE _IOWR('B', 37, struct bioc_setstate)
struct bioc_setstate {
	void		*bs_cookie;
	u_int16_t	bs_channel;
	u_int16_t	bs_target;
	u_int16_t	bs_lun;
	u_int16_t	bs_other_id_type; /* use other_id instead of ctl */
#define BIOC_SSOTHER_UNUSED	0x00
#define BIOC_SSOTHER_DEVT	0x01
	int		bs_other_id;	/* cram dev_t or other id in here */

	int		bs_status;	/* change to this status */
#define BIOC_SSONLINE		0x00	/* online disk */
#define BIOC_SSOFFLINE		0x01	/* offline disk */
#define BIOC_SSHOTSPARE		0x02	/* mark as hotspare */
#define BIOC_SSREBUILD		0x03	/* rebuild on this disk */
	int		bs_volid;	/* volume id for rebuild */
};

#define BIOCCREATERAID _IOWR('B', 38, struct bioc_createraid)
struct bioc_createraid {
	void		*bc_cookie;
	void		*bc_dev_list;
	u_int16_t	bc_dev_list_len;
	int32_t		bc_key_disk;
#define BIOC_CRMAXLEN		1024
	u_int16_t	bc_level;
	u_int32_t	bc_flags;
#define BIOC_SCFORCE		0x01	/* do not assemble, force create */
#define BIOC_SCDEVT		0x02	/* dev_t array or string in dev_list */
#define BIOC_SCNOAUTOASSEMBLE	0x04	/* do not assemble during autoconf */
	u_int32_t	bc_opaque_size;
	u_int32_t	bc_opaque_flags;
#define	BIOC_SOINVALID		0x00	/* no opaque pointer */
#define	BIOC_SOIN		0x01	/* kernel perspective direction */
#define BIOC_SOOUT		0x02	/* kernel perspective direction */
	u_int32_t	bc_opaque_status;
#define	BIOC_SOINOUT_FAILED	0x00	/* operation failed */
#define	BIOC_SOINOUT_OK		0x01	/* operation succeeded */
	void		*bc_opaque;
};

#define BIOCDELETERAID _IOWR('B', 39, struct bioc_deleteraid)
struct bioc_deleteraid {
	void		*bd_cookie;
	u_int32_t	bd_flags;
#define BIOC_SDCLEARMETA	0x01	/* clear metadata region */
	char		bd_dev[16];	/* device */
};

#define BIOCDISCIPLINE _IOWR('B', 40, struct bioc_discipline)
struct bioc_discipline {
	void		*bd_cookie;
	char		bd_dev[16];
	u_int32_t	bd_cmd;
	u_int32_t	bd_size;
	void		*bd_data;
};

/* kernel and userspace defines */
#define BIOC_INQ		0x0001
#define BIOC_DISK		0x0002
#define BIOC_VOL		0x0004
#define BIOC_ALARM		0x0008
#define BIOC_BLINK		0x0010
#define BIOC_SETSTATE		0x0020
#define BIOC_CREATERAID		0x0040
#define BIOC_DELETERAID		0x0080
#define BIOC_DISCIPLINE		0x0100

/* user space defines */
#define BIOC_DEVLIST		0x10000
