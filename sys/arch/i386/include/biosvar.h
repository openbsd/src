/*	$OpenBSD: biosvar.h,v 1.8 1997/08/22 20:10:21 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __BIOS_VAR_H__
#define __BIOS_VAR_H__

#define BOOTC_CHECK  0x00
#define BOOTC_BOOT   0x01
#define BOOTC_GETENV 0x02
#define BOOTC_SETENV 0x03
#define  BOOTV_BOOTDEV 1
#define  BOOTV_BDGMTRY 2
#define  BOOTV_CONSDEV 3
#define  BOOTV_APMCONN 4
#define BOOTC_GETC 0x10
#define BOOTC_PUTC 0x11
#define BOOTC_POLL 0x12

#define	BIOSNHEADS(d)	(((d)>>8)+1)
#define	BIOSNSECTS(d)	((d)&0xff)	/* sectors are 1-based */

/* BIOS media ID */
#define BIOSM_F320K	0xff	/* floppy ds/sd  8 spt */
#define	BIOSM_F160K	0xfe	/* floppy ss/sd  8 spt */
#define	BIOSM_F360K	0xfd	/* floppy ds/sd  9 spt */
#define	BIOSM_F180K	0xfc	/* floppy ss/sd  9 spt */
#define	BIOSM_ROMD	0xfa	/* ROM disk */
#define	BIOSM_F120M	0xf9	/* floppy ds/hd 15 spt 5.25" */
#define	BIOSM_F720K	0xf9	/* floppy ds/dd  9 spt 3.50" */
#define	BIOSM_HD	0xf8	/* hard drive */
#define	BIOSM_F144K	0xf0	/* floppy ds/hd 18 spt 3.50" */
#define	BIOSM_OTHER	0xf0	/* any other */

/*
 * Advanced Power Management (APM) BIOS driver for laptop PCs.
 * 
 * Copyright (c) 1994 by HOSOKAWA Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Aug, 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 * Oct, 1994	NetBSD port (1.0 BETA 10/2) by ukai
 */

#if defined(_KERNEL) || defined (_STANDALONE)

/* APM flags */
#define APM_16BIT_SUPPORT	0x01
#define APM_32BIT_SUPPORT	0x02
#define APM_CPUIDLE_SLOW	0x04
#define APM_DISABLED		0x08
#define APM_DISENGAGED		0x10

/* APM functions */
#define APM_INSTCHECK		0x5300
#define APM_REALCONNECT		0x5301
#define APM_PROT16CONNECT	0x5302
#define APM_PROT32CONNECT	0x5303
#define APM_DISCONNECTANY	0x5304
#define APM_CPUIDLE		0x5305
#define APM_CPUBUSY		0x5306
#define APM_SETPWSTATE		0x5307
#define APM_ENABLEDISABLEPM	0x5308
#define APM_RESTOREDEFAULT	0x5309
#define	APM_GETPWSTATUS		0x530a
#define APM_GETPMEVENT		0x530b
#define APM_GETPWSTATE		0x530c
#define APM_ENABLEDISABLEDPM	0x530d
#define APM_DRVVERSION		0x530e
#define APM_ENGAGEDISENGAGEPM	0x530f
#define APM_OEMFUNC		0x5380

/* error code */
#define APME_OK			0x00
#define APME_PMDISABLED		0x01
#define APME_REALESTABLISHED	0x02
#define APME_NOTCONNECTED	0x03
#define APME_PROT16ESTABLISHED	0x05
#define APME_PROT16NOTSUPPORTED	0x06
#define APME_PROT32ESTABLISHED	0x07
#define APME_PROT32NOTDUPPORTED	0x08
#define APME_UNKNOWNDEVICEID	0x09
#define APME_OUTOFRANGE		0x0a
#define APME_NOTENGAGED		0x0b
#define APME_CANTENTERSTATE	0x60
#define APME_NOPMEVENT		0x80
#define APME_NOAPMPRESENT	0x86

/* device code */
#define PMDV_APMBIOS		0x0000
#define PMDV_ALLDEV		0x0001
#define PMDV_DISP0		0x0100
#define PMDV_DISP1		0x0101
#define PMDV_2NDSTORAGE0	0x0200
#define PMDV_2NDSTORAGE1	0x0201
#define PMDV_2NDSTORAGE2	0x0202
#define PMDV_2NDSTORAGE3	0x0203
#define PMDV_PARALLEL0		0x0300
#define PMDV_PARALLEL1		0x0301
#define PMDV_SERIAL0		0x0400
#define PMDV_SERIAL1		0x0401
#define PMDV_SERIAL2		0x0402
#define PMDV_SERIAL3		0x0403
#define PMDV_SERIAL4		0x0404
#define PMDV_SERIAL5		0x0405
#define PMDV_SERIAL6		0x0406
#define PMDV_SERIAL7		0x0407
#define PMDV_NET0		0x0500
#define PMDV_NET1		0x0501
#define PMDV_NET2		0x0502
#define PMDV_NET3		0x0503
#define PMDV_PCMCIA0		0x0600
#define PMDV_PCMCIA1		0x0601
#define PMDV_PCMCIA2		0x0602
#define PMDV_PCMCIA3		0x0603
/* 0x0700 - 0xdfff	Reserved			*/
/* 0xe000 - 0xefff	OEM-defined power device IDs	*/
/* 0xf000 - 0xffff	Reserved			*/

/* Power state */
#define PMST_APMENABLED		0x0000
#define PMST_STANDBY		0x0001
#define PMST_SUSPEND		0x0002
#define PMST_OFF		0x0003
#define PMST_LASTREQNOTIFY	0x0004
#define PMST_LASTREQREJECT	0x0005
/* 0x0006 - 0x001f	Reserved system states		*/
/* 0x0020 - 0x003f	OEM-defined system states	*/
/* 0x0040 - 0x007f	OEM-defined device states	*/
/* 0x0080 - 0xffff	Reserved device states		*/

#define APM_MIN_ORDER		0x00
#define APM_MID_ORDER		0x80
#define APM_MAX_ORDER		0xff


/* power management event code */
#define PMEV_NOEVENT		0x0000
#define PMEV_STANDBYREQ		0x0001
#define PMEV_SUSPENDREQ		0x0002
#define PMEV_NORMRESUME		0x0003
#define PMEV_CRITRESUME		0x0004
#define PMEV_BATTERYLOW		0x0005
#define PMEV_POWERSTATECHANGE	0x0006
#define PMEV_UPDATETIME		0x0007
#define PMEV_CRITSUSPEND	0x0008
#define PMEV_USERSTANDBYREQ	0x0009
#define PMEV_USERSUSPENDREQ	0x000a
#define PMEV_STANDBYRESUME	0x000b
/* 0x000c - 0x00ff	Reserved system events	*/
/* 0x0100 - 0x01ff	Reserved device events	*/
/* 0x0200 - 0x02ff	OEM-defined APM events	*/
/* 0x0300 - 0xffff	Reserved		*/
#define PMEV_DEFAULT		0xffffffff	/* used for customization */

#ifdef _LOCORE
#define	DOINT(n)	int	$0x20+(n)
#else
#define	DOINT(n)	"int $0x20+(" #n ")"

extern struct BIOS_vars {
	/* XXX filled in assumption that last file opened is kernel */
	int	bios_dev;
	int	bios_geometry;

	u_int	bios_extmem;
	u_int	bios_cnvmem;

	u_int	apm_detail;
	u_int	apm_code32_base;
	u_int	apm_code16_base;
	u_int	apm_code_len;
	u_int	apm_data_base;
	u_int	apm_data_len;
	u_int	apm_entry;

	dev_t	boot_consdev;

}	BIOS_vars;

extern struct BIOS_regs {
	u_int32_t	biosr_ax;
	u_int32_t	biosr_cx;
	u_int32_t	biosr_dx;
	u_int32_t	biosr_bx;
	u_int32_t	biosr_bp;
	u_int32_t	biosr_si;
	u_int32_t	biosr_di;
	u_int32_t	biosr_ds;
	u_int32_t	biosr_es;
}	BIOS_regs;

struct EDD_CB {
	u_int8_t  edd_len;   /* size of packet */
	u_int8_t  edd_res;   /* reserved */
	u_int16_t edd_nblk;  /* # of blocks to transfer */
	u_int32_t edd_buf;   /* address of buffer */
	u_int64_t edd_daddr; /* starting block */
};

#ifdef _KERNEL
#include <machine/bus.h>

struct bios_attach_args {
	char *bios_busname;
	bus_space_tag_t bios_iot;
	bus_space_tag_t bios_memt;
};

struct consdev;

void bioscnprobe __P((struct consdev *));
void bioscninit __P((struct consdev *));
void bioscnputc __P((dev_t, int));
int bioscngetc __P((dev_t));
void bioscnpollc __P((dev_t, int));

#endif /* _KERNEL */
#endif /* _LOCORE */
#endif /* _KERNEL || _STANDALONE */

#endif /* __BIOS_VAR_H__ */
