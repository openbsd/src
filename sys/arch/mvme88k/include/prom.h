/*	$OpenBSD: prom.h,v 1.17 2007/12/15 19:35:52 miod Exp $ */
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#ifndef __MACHINE_PROM_H__
#define __MACHINE_PROM_H__

/* BUG trap vector */
#define	MVMEPROM_VECTOR		496

#define MVMEPROM_INCHR		0x00
#define MVMEPROM_INSTAT		0x01
#define MVMEPROM_INLN		0x02
#define MVMEPROM_READSTR	0x03
#define MVMEPROM_READLN		0x04
#define MVMEPROM_DSKRD		0x10
#define MVMEPROM_DSKWR		0x11
#define MVMEPROM_DSKCFIG	0x12
#define MVMEPROM_DSKFMT		0x14
#define MVMEPROM_DSKCTRL	0x15
#define MVMEPROM_NETFOPEN	0x1b
#define MVMEPROM_NETFREAD	0x1c
#define MVMEPROM_NETCTRL	0x1d
#define MVMEPROM_OUTCHR		0x20
#define MVMEPROM_OUTSTR		0x21
#define MVMEPROM_OUTSTRCRLF	0x22
#define MVMEPROM_WRITE		0x23
#define MVMEPROM_WRITELN	0x24
#define	MVMEPROM_OUTCRLF	0x26
#define MVMEPROM_DELAY		0x43
#define MVMEPROM_RTC_RD		0x53
#define MVMEPROM_EXIT		0x63
#define MVMEPROM_GETBRDID	0x70
#define MVMEPROM_ENVIRON	0x71
#define	MVMEPROM_FORKMPU	0x100

#define NETCTRLCMD_GETETHER	1

#define ENVIRONCMD_WRITE	1
#define ENVIRONCMD_READ		2
#define ENVIRONTYPE_EOL		0
#define ENVIRONTYPE_START	1
#define ENVIRONTYPE_DISKBOOT	2
#define ENVIRONTYPE_ROMBOOT	3
#define ENVIRONTYPE_NETBOOT	4
#define ENVIRONTYPE_MEMSIZE	5

#define	FORKMPU_NOT_IDLE	-1
#define	FORKMPU_BAD_ADDRESS	-2
#define	FORKMPU_NO_MPU		-3

#ifndef LOCORE
struct mvmeprom_netctrl {
	u_char	ctrl;
	u_char	dev;
	u_short	status;
	u_long	cmd;
	u_long	addr;
	u_long	len;
	u_long	flags;
};

struct mvmeprom_netfopen {
	u_char	ctrl;
	u_char	dev;
	u_short	status;
	char	filename[64];
};

struct mvmeprom_netfread {
	u_char	ctrl;
	u_char	dev;
	u_short	status;
	u_long	addr;
	u_short	bytes;
	u_short	blk;
	u_long	timeout;
};

struct prom_environ_hdr {
	u_char	type;
	u_char	len;
};

struct mvmeprom_brdid {
	u_long	eye_catcher;
	u_char	rev;
	u_char	month;
	u_char	day;
	u_char	year;
	u_short	size;
	u_short	rsv1;
	u_short	model;
	u_char	suffix[2];
	u_short	options;
	u_char	family;
	u_char	cpu;
	u_short	ctrlun;
	u_short	devlun;
	u_short	devtype;
	u_short	devnum;
	u_long	bug;
	u_char	version[4];
	u_char	serial[12];		/* SBC serial number */
	u_char	id[16];			/* SBC id */
	u_char	pwa[16];		/* printed wiring assembly number */
	u_char	speed[4];		/* cpu speed */
	u_char	etheraddr[6];		/* mac address, all zero if no ether */
	u_char	fill[2];
	u_char	scsiid[2];		/* local SCSI id */
	u_char	sysid[8];		/* system id - nothing on mvme187 */
	u_char	brd1_pwb[8];		/* memory board 1 pwb */
	u_char	brd1_serial[8];		/* memory board 1 serial */
	u_char	brd2_pwb[8];		/* memory board 2 pwb */
	u_char	brd2_serial[8];		/* memory board 2 serial */
	u_char	reserved[153];
	u_char	cksum[1];
};

struct mvmeprom_time {
        u_char	year_BCD;
        u_char	month_BCD;
        u_char	day_BCD;
        u_char	wday_BCD;
        u_char	hour_BCD;
        u_char	min_BCD;
        u_char	sec_BCD;
        u_char	cal_BCD;
};

struct mvmeprom_dskio {
	u_char	ctrl_lun;
	u_char	dev_lun;
	u_short	status;
	void	*pbuffer;
	u_long	blk_num;
	u_short	blk_cnt;
	u_char	flag;
#define BUG_FILE_MARK	0x80
#define IGNORE_FILENUM	0x02
#define END_OF_FILE	0x01
	u_char	addr_mod;
};
#define MVMEPROM_BLOCK_SIZE	256

extern unsigned long bugvec[32], sysbugvec[32];	/* BUG trap vector copies */

#endif	/* _LOCORE */

#ifndef RB_NOSYM
#define RB_NOSYM 0x4000
#endif
#endif /* __MACHINE_PROM_H__ */
