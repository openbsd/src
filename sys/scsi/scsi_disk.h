/*	$NetBSD: scsi_disk.h,v 1.9 1996/03/19 03:07:02 mycroft Exp $	*/

/*
 * SCSI interface description
 */

/*
 * Some lines of this file come from a file of the name "scsi.h"
 * distributed by OSF as part of mach2.5,
 *  so the following disclaimer has been kept.
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 * 
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Largely written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with 
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

/*
 * SCSI command format
 */

#ifndef	_SCSI_SCSI_DISK_H
#define _SCSI_SCSI_DISK_H 1

struct scsi_reassign_blocks {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_rw {
	u_int8_t opcode;
	u_int8_t addr[3];
#define	SRW_TOPADDR	0x1F	/* only 5 bits here */
	u_int8_t length;
	u_int8_t control;
};

struct scsi_rw_big {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRWB_RELADDR	0x01
	u_int8_t addr[4];
	u_int8_t reserved;
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_read_capacity {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t addr[4];
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_start_stop {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t how;
#define	SSS_STOP		0x00
#define	SSS_START		0x01
#define	SSS_LOEJ		0x02
	u_int8_t control;
};



/*
 * Opcodes
 */

#define	REASSIGN_BLOCKS		0x07
#define	READ_COMMAND		0x08
#define WRITE_COMMAND		0x0a
#define MODE_SELECT		0x15
#define MODE_SENSE		0x1a
#define START_STOP		0x1b
#define PREVENT_ALLOW		0x1e
#define	READ_CAPACITY		0x25
#define	READ_BIG		0x28
#define WRITE_BIG		0x2a


struct scsi_read_cap_data {
	u_int8_t addr[4];
	u_int8_t length[4];
};

struct scsi_reassign_blocks_data {
	u_int8_t reserved[2];
	u_int8_t length[2];
	struct {
		u_int8_t dlbaddr[4];
	} defect_descriptor[1];
};

union disk_pages {
#define	DISK_PGCODE	0x3F	/* only 6 bits valid */
	struct page_disk_format {
		u_int8_t pg_code;	/* page code (should be 3) */
		u_int8_t pg_length;	/* page length (should be 0x16) */
		u_int8_t trk_z[2];	/* tracks per zone */
		u_int8_t alt_sec[2];	/* alternate sectors per zone */
		u_int8_t alt_trk_z[2];	/* alternate tracks per zone */
		u_int8_t alt_trk_v[2];	/* alternate tracks per volume */
		u_int8_t ph_sec_t[2];	/* physical sectors per track */
		u_int8_t bytes_s[2];	/* bytes per sector */
		u_int8_t interleave[2];	/* interleave */
		u_int8_t trk_skew[2];	/* track skew factor */
		u_int8_t cyl_skew[2];	/* cylinder skew */
		u_int8_t flags;		/* various */
#define	DISK_FMT_SURF	0x10
#define	DISK_FMT_RMB	0x20
#define	DISK_FMT_HSEC	0x40
#define	DISK_FMT_SSEC	0x80
		u_int8_t reserved2;
		u_int8_t reserved3;
	} disk_format;
	struct page_rigid_geometry {
		u_int8_t pg_code;	/* page code (should be 4) */
		u_int8_t pg_length;	/* page length (should be 0x16)	*/
		u_int8_t ncyl[3];	/* number of cylinders */
		u_int8_t nheads;	/* number of heads */
		u_int8_t st_cyl_wp[3];	/* starting cyl., write precomp */
		u_int8_t st_cyl_rwc[3];	/* starting cyl., red. write cur */
		u_int8_t driv_step[2];	/* drive step rate */
		u_int8_t land_zone[3];	/* landing zone cylinder */
		u_int8_t sp_sync_ctl;	/* spindle synch control */
#define SPINDLE_SYNCH_MASK	0x03	/* mask of valid bits */
#define SPINDLE_SYNCH_NONE	0x00	/* synch disabled or not supported */
#define SPINDLE_SYNCH_SLAVE	0x01	/* disk is a slave */
#define SPINDLE_SYNCH_MASTER	0x02	/* disk is a master */
#define SPINDLE_SYNCH_MCONTROL	0x03	/* disk is a master control */
		u_int8_t rot_offset;	/* rotational offset (for spindle synch) */
		u_int8_t reserved1;
		u_int8_t rpm[2];	/* media rotation speed */
		u_int8_t reserved2;
		u_int8_t reserved3;
    	} rigid_geometry;
};

#endif /* _SCSI_SCSI_DISK_H */
