/* $NetBSD: disklabel.h,v 1.2 1996/03/06 23:17:51 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * disklabel.h
 *
 * machine specific disk label info
 *
 * Created      : 04/10/94
 */

#ifndef _ARM32_DISKLABEL_H_
#define _ARM32_DISKLABEL_H_

#define LABELSECTOR	1		/* sector containing label */
#define LABELOFFSET	0		/* offset of label in sector */
#define MAXPARTITIONS	8		/* number of partitions */
#define RAW_PART	2		/* raw partition: XX?c */

#define NRISCBSD_PARTITIONS MAXPARTITIONS

#define PARTITION_TYPE_UNUSED  0
#define PARTITION_TYPE_ADFS    1
#define PARTITION_TYPE_RISCIX  2

#define PARTITION_FORMAT_RISCIX  2
#define PARTITION_FORMAT_RISCBSD 0x42

#define FILECORE_BOOT_SECTOR 6

/* Stuff to deal with RISCiX partitions */

#define NRISCIX_PARTITIONS 8
#define RISCIX_PARTITION_OFFSET 8

struct riscix_partition {
	u_int rp_start;
	u_int rp_length;
	u_int rp_type;
	char rp_name[16];
};

struct riscix_partition_table {
	u_int pad0;
	u_int pad1;
	struct riscix_partition partitions[NRISCIX_PARTITIONS];
};

  
#include <sys/dkbad.h>

struct riscbsd_partition {
	u_int rp_start;
	u_int rp_length;
	u_int rp_type;
	char rp_name[16];
};

struct cpu_disklabel {
	u_int pad0;
	u_int pad1;
	struct riscbsd_partition partitions[NRISCBSD_PARTITIONS];
	struct dkbad bad;
};

struct filecore_bootblock {
	u_char  padding0[0x1c0];
	u_char  log2secsize;
	u_char  secspertrack;
	u_char  heads;
	u_char  density;
	u_char  idlen;
	u_char  log2bpmb;
	u_char  skew;
	u_char  bootoption;
	u_char  lowsector;
	u_char  nzones;
	u_short zone_spare;
	u_int   root;
	u_int   disc_size;
	u_short disc_id;
	u_char  disc_name[10];
	u_int   disc_type;

	u_char  padding1[24];

	u_char partition_type;
	u_char partition_cyl_low;
	u_char partition_cyl_high;
	u_char checksum;
};

#ifdef _KERNEL
struct disklabel;
int	bounds_check_with_label __P((struct buf *, struct disklabel *, int));
#endif /* _KERNEL */

#endif /* _ARM32_DISKLABEL_H_ */

/* End of disklabel.h */
