/*	$OpenBSD: xdreg.h,v 1.4 2004/04/12 22:12:32 jmc Exp $	*/
/*	$NetBSD: xdreg.h,v 1.3 1996/03/31 22:38:54 pk Exp $	*/

/*
 *
 * Copyright (c) 1995 Charles D. Cranor
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
 *      This product includes software developed by Charles D. Cranor.
 * 4. The name of the author may not be used to endorse or promote products
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

/*
 * x d r e g . h
 *
 * this file contains the description of the Xylogics 753/7053's hardware
 * data structures.
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 */

#define XDC_MAXDEV    4       /* max devices per controller */
#define XDC_RESETUSEC 1000000 /* max time for xdc reset (page 21 says 1sec) */
#define XDC_MAXIOPB   31      /* max number of iopbs that can be active */
#define XDC_MAXTIME   4*1000000 /* four seconds before we give up and reset */
#define XDC_MAXTRIES  4       /* max number of times to retry an operation */
#define XDC_THROTTLE  32      /* dma throttle */
#define XDC_INTERLEAVE 0      /* interleave (for format param) */
#define XDC_DPARAM     0      /* dparam (drive param) XDDP_EC32 or 0 */

/*
 * xdc device interface
 * (lives in VME address space)
 */

struct xdc {
  volatile u_char filler0;
  volatile u_char xdc_iopbaddr0;       /* iopb byte 0 (LSB) */
  volatile u_char filler1;
  volatile u_char xdc_iopbaddr1;       /* iopb byte 1 */
  volatile u_char filler2;
  volatile u_char xdc_iopbaddr2;       /* iopb byte 2 */
  volatile u_char filler3;
  volatile u_char xdc_iopbaddr3;       /* iopb byte 3 (MSB) */
  volatile u_char filler4;
  volatile u_char xdc_iopbamod;        /* iopb address modifier */
  volatile u_char filler5;
  volatile u_char xdc_csr;             /* control and status register */
  volatile u_char filler6;
  volatile u_char xdc_f_err;           /* fatal error register */
};

/*
 * xdc_iopbamod: addressing modes
 * When doing DMA, if the maximum address of the buffer is greater than
 * 24 bits then you must use the 32 bit mode.   Note that on many systems
 * (e.g. sun-4/300) DVMA space is smaller than 24 bits, so there is no
 * need for the 32 bit mode.   However, the 32-bit mode hooks are in
 * the driver in case it ever gets ported to an environment that needs it.
 */

#define XDC_ADDRMOD   0x3d    /* standard address modifer, 24 bit max */
#define XDC_ADDRMOD32 0x0d    /* 32 bit version above */

/*
 * xdc_csr
 */

#define XDC_RMAINTMD 0x80     /* reserved maintenance mode (write) */
#define XDC_BUSY     0x80     /* busy (read) */
#define XDC_F_ERROR  0x40     /* fatal error (read) */
#define XDC_MAINTMOD 0x20     /* maintenance mode (read/write) */
#define XDC_RESET    0x08     /* soft reset (read/write) */
#define XDC_ADDIOPB  0x04     /* add iopb/add pending (write) */
#define XDC_ADDING   0x04     /* iopb add is pending (read) */
#define XDC_CLRRIO   0x02     /* clear RIO (remove iopb) request (write) */
#define XDC_REMIOPB  0x02     /* remove iopb (read) */
#define XDC_RBUSYSEM 0x01     /* register busy semaphore (read/write) */

/*
 * Input/Output Parameter Block (iopb)
 *
 * all controller commands are done via iopb's.   to start a command you
 * must do this:
 * [1] allocate space in DVMA space for the iopb
 * [2] fill out all the fields of the iopb
 * [3] check to see if controller can accept an iopb (XDC_ADDING bit clear)
 * [4] put the DVMA address of the iopb in the xdc registers (in vme space)
 * [5] set the XDC_ADDIOPB bit in the xdc csr
 * [6] <command started>
 *
 * when the controller is done with a command it may interrupt (if you
 * ask it to) and it will set the XDC_REMIOPB bit in the csr.   the address
 * of the finished iopb will be in the xdc registers.   after that is
 * read, set the XDC_CLRRIO to clear the iopb out of memory.
 *
 * the format of the iopb is described in section 4 of the manual.
 */

struct xd_iopb {
                                 /* section 4.1.1: byte 0 */
  volatile u_char errs:1;        /* error summary bit, only valid if
				    "done" is set.  must clear "done"
				    and "errs" bits before starting an
				    operation */
  volatile u_char done:1;        /* "done" bit */
  volatile u_char chen:1;        /* chain enable, "next iopb" is valid,
				    note xd returns one iopb at a time */
  volatile u_char sgm:1;         /* scatter/gather mode */
  volatile u_char comm:4;        /* command number (see table 4-2) */
#define XDCMD_NOP 0x0            /* no-op */
#define XDCMD_WR  0x1            /* write */
#define XDCMD_RD  0x2            /* read */
#define XDCMD_SK  0x3            /* seek */
#define XDCMD_RST 0x4            /* drive reset */
#define XDCMD_WRP 0x5            /* write params */
#define XDCMD_RDP 0x6            /* read params */
#define XDCMD_XWR 0x7            /* extended write */
#define XDCMD_XRD 0x8            /* extended read */
#define XDCMD_TST 0x9            /* diagnostic tests */
                                 /* 0xa to 0xf are reserved */
                                 /* section 4.1.2: byte 1 */
  volatile u_char errno;         /* status byte 1 (non-zero if error) */
                                 /* section 4.1.3: byte 2 */
  volatile u_char status;        /* status byte 2 (see below) */
#define XDST_SR   0x40           /* slipped revolution */
#define XDST_CSE  0x20           /* count sectors executed */
#define XDST_WRPT 0x10           /* write protected drive */
#define XDST_DFLT 0x08           /* disk fault */
#define XDST_SKER 0x04           /* seek error: >max, or timeout */
#define XDST_ONCL 0x02           /* on-cylinder */
#define XDST_DRDY 0x01           /* drive is ready! */
                                 /* section 4.1.4: byte 3 */
  volatile u_char istat;         /* internal status: reserved for xylogics */
                                 /* section 4.1.5: byte 4 */
  volatile u_char subfun;        /* sub-function of command (see below) */
#define XDFUN_R   0x00           /* XDCMD_SK: report current addr */
#define XDFUN_SR  0x01           /* XDCMD_SK: seek and report addr */
#define XDFUN_SRI 0x02           /* XDCMD_SK: start seek, report comp imm */
#define XDFUN_CTL 0x00           /* XDCMD_{WRP,RDP}: controller params */
#define XDFUN_DRV 0x80           /* XDCMD_{WRP,RDP}: drive params */
#define XDFUN_FMT 0x81           /* XDCMD_{WRP,RDP}: format params,XWR form.*/
#define XDFUN_STX 0xa0           /* XDCMD_RDP: read drive status extended */
#define XDFUN_THD 0x80           /* XDCMD_{XWR,XRD}: track headers */
#define XDFUN_VFY 0x81           /* XDCMD_XRD: verify data */
#define XDFUN_HDR 0x82           /* XDCMD_{XWR,XRD}: header, verify,data, ecc*/
#define XDFUN_DM  0xa0           /* XDCMD_{XWR,XRD}: defect map */
#define XDFUN_DMX 0xa1           /* XDCMD_{XWR,XRD}: defect map extended */
                                 /* section 4.1.6: byte 5 */
  volatile u_char fixd:1;        /* fixed media (vs removable) */
  volatile u_char reserved1:4;   /* reserved */
  volatile u_char unit:3;        /* unit number */
                                 /* note: 6 to 13 are overloaded (see below) */
                                 /* section 4.1.7: byte 6 */
  volatile u_char lll:5;         /* linked list length */
  volatile u_char intl:3;        /* interrupt level */
                                 /* section 4.1.8: byte 7 */
  volatile u_char intr_vec;      /* interrupt vector */
                                 /* section 4.1.9: bytes 8 and 9 */
  volatile u_short sectcnt;      /* sector count (# to xfer) */
                                 /* section 4.1.10: byte a and b */
  volatile u_short cylno;        /* cylinder number */
                                 /* section 4.1.11: byte c */
  volatile u_char headno;        /* head number */
                                 /* section 4.1.12: byte d */
  volatile u_char sectno;        /* sector number */
                                 /* section 4.1.13: byte e */
  volatile u_char addrmod;       /* addr modifier (bits 7,6 must be zero) */
                                 /* section 4.1.14: byte f */
  volatile u_char naddrmod;      /* next (in chain) address iobp ad. modifer */
                                 /* section 4.1.15: bytes 0x10 to 0x13 */
  volatile u_long daddr;         /* DMA data address */
                                 /* section 4.1.16: bytes 0x14 to 0x17 */
  volatile u_long nextiopb;      /* next iopb (in chain) address */
                                 /* section 4.1.17: bytes 0x18, 0x19 */
  volatile u_short cksum;        /* iopb checksum */
                                 /* section 4.1.18: bytes 0x1a, 0x1b */
  volatile u_short eccpattern;   /* ECC pattern word (ecc mode 0) */
                                 /* section 4.1.19: bytes 0x1c, 0x1d */
  volatile u_short eccoffword;   /* ECC offset word (ecc mode 0) */
};

/*
 * some commands overload bytes 6 to 0x13 of the iopb with different meanings.
 * these commands include:
 *   section 4.2: controller parameters
 *   section 4.3: drive parameters
 *   sectino 4.4: format parameters
 *
 * note that the commands that overload the iopb are not part of the
 * "critical data path" of the driver.   so, we handle them by defining
 * alternate iopb structures for these commands... it only costs us an
 * extra pointer.
 */

/*
 * controller parameters iopb: redefines bytes: 8 -> 0xe, 0x10 -> 0x13
 */

struct xd_iopb_ctrl {
  volatile u_char same[8];       /* same as xd_iopb */
                                 /* section 4.2.1: byte 8 */
  volatile u_char param_a;       /* param A (see below) */
#define XDPA_AUD  0x80           /* auto-update iopb fields when cmd done */
#define XDPA_TMOD 0x40           /* long-word transfer mode (vs word) */
#define XDPA_DACF 0x20           /* ignore vme ACFAIL signal */
#define XDPA_ICS  0x10           /* checksum check (adds 100usec per xfer) */
#define XDPA_EDT  0x08           /* enable on-board DMA timeout timer */
#define XDPA_NPRM 0x04           /* enable VME non-priv request mode */
                                 /* rest must be zero */
                                 /* section 4.2.2: byte 9 */
  volatile u_char param_b;       /* param B (see below) */
#define XDPB_TDT 0xc0            /* throttle dead time (see 8.11, below) */
#define XDPB_ROR 0x10            /* release on request */
#define XDPB_DRA 0x01            /* disable read ahead */
                                 /* TDT values: */
#define XDPB_TDT_0USEC    0x00   /* no TDT */
#define XDPB_TDT_3_2USEC  0x40   /* 3.2 usec */
#define XDPB_TDT_6_4USEC  0x80   /* 6.4 usec */
#define XDPB_TDT_12_8USEC 0xc0   /* 12.8 usec */
                                 /* section 4.2.3: byte a */
  volatile u_char param_c;       /* param C (see below) */
#define XDPC_OVS  0x80           /* over-lapped seek */
#define XDPC_COP  0x40           /* command optimiziation (elevator alg.) */
#define XDPC_ASR  0x10           /* auto-seek retry */
#define XDPC_RBC  0x04           /* retry before correction if ECC error */
#define XDPC_ECCM 0x03           /* ECC mode (0, 1, and 2) */
#define XDPC_ECC0 0x00           /* ECC mode 0 */
#define XDPC_ECC1 0x01           /* ECC mode 1 */
#define XDPC_ECC2 0x02           /* ECC mode 2 */
                                 /* section 4.2.4: byte b */
  volatile u_char throttle;      /* max dma xfers per master (0==256) */
                                 /* section 4.2.5: byte c */
  volatile u_char eprom_lvl;     /* EPROM release level */
  volatile u_char delay;         /* delay (see note below) */
                                 /* section 4.2.6: byte e */
  volatile u_char ctype;         /* controller type */
#define XDCT_753 0x53            /* xylogic 753/7053 */
  volatile u_char same2;         /* byte f: same as xd_iopb */
                                 /* section 4.2.7: byte 0x10, 0x11 */
  volatile u_short eprom_partno; /* eprom part number */
                                 /* section 4.2.8: byte 12 */
  volatile u_char eprom_rev;     /* eprom revision number */
};

/*
 * Note on byte 0xd ("delay"): This byte is not documented in the
 * Xylogics manual.  However, I contacted Xylogics and found out what
 * it does.  The controller sorts read commands into groups of
 * contiguous sectors.  After it processes a group of contiguous
 * sectors rather than immediately going on to the next group of
 * contiguous sectors, the controller can delay for a certain amount
 * of time in hopes of getting another cluster of reads in the same
 * area of the disk (thus avoiding a long seek).  Byte 0xd controls
 * how long it waits before giving up and going on and doing the next
 * contiguous cluster.
 *
 * it is unclear what unit the delay is in, but it looks like sun
 * uses the value "20" for sun3's, and "0" for sparc, except for the
 * 4/300 (where it is "4").   [see /sys/sundev/xd_conf.c on any 4.1.3
 * machine for how sun configures its controller...]
 */

#define XDC_DELAY_SUN3  20
#define XDC_DELAY_4_300 4
#define XDC_DELAY_SPARC 0

/*
 * drive parameters iopb: redefines bytes: 6, 8, 9, a, b, c, d, e
 */

struct xd_iopb_drive {
  volatile u_char same[6];       /* same as xd_iopb */
                                 /* section 4.3.1: byte 6 */
  volatile u_char dparam_ipl;    /* drive params | interrupt level */
#define XDDP_EC32 0x10           /* 32 bit ECC mode */
  volatile u_char same1;         /* byte 7: same */
                                 /* section 4.3.2: byte 8 */
  volatile u_char maxsect;       /* max sector/last head (<= byte d) */
                                 /* section 4.3.3: byte 9 */
  volatile u_char headoff;       /* head offset */
                                 /* section 4.3.4: bytes 0xa, 0xb */
  volatile u_short maxcyl;       /* max cyl (zero based!) */
                                 /* section 4.3.5: byte 0xc */
  volatile u_char maxhead;       /* max head (zero based!) */
                                 /* section 4.3.6: byte 0xd */
  volatile u_char maxsector;     /* max sector of disk (zero based!) */
                                 /* section 4.3.7: byte 0xe */
  volatile u_char sectpertrk;    /* sectors per track, not zero base, no runt*/
};

/*
 * format parameters iopb: redefines bytes: 6, 8, 9, a, b, c, d, 0x10, 0x11
 */

struct xd_iopb_format {
  volatile u_char same[6];       /* smae as xd_iopb */
                                 /* section 4.4.1: byte 6 */
  volatile u_char interleave_ipl;/* (interleave << 4) | interupt level */
                                 /* interleave ratio 1:1 to 16:1 */
  volatile u_char same1;         /* byte 7: same */
                                 /* section 4.4.2: byte 8 */
  volatile u_char field1;        /* >= 1, xylogic says should be 1 */
#define XDFM_FIELD1 0x01         /* xylogic value */
                                 /* section 4.4.3: byte 9 */
  volatile u_char field2;        /* >0, field1+field2 <= 255 */
#define XDFM_FIELD2 0x0a         /* xylogic value */
                                 /* section 4.4.4: byte a */
  volatile u_char field3;        /* >= field1+field2 */
#define XDFM_FIELD3 0x1b         /* xylogic value */
                                 /* section 4.4.5: byte b */
  volatile u_char field4;        /* field4 */
#define XDFM_FIELD4 0x14         /* xylogic value */
                                 /* section 4.4.6: bytes 0xc, 0xd */
  volatile u_short bytespersec;  /* bytes per sector */
#define XDFM_BPS    0x200        /* must be 512! */
  volatile u_char same2[2];      /* bytes e, f */
                                 /* section 4.4.7: byte 0x10 */
  volatile u_char field6;        /* field 6 */
#define XDFM_FIELD6 0x0a         /* xylogic value */
                                 /* section 4.4.8: byte 0x11 */
  volatile u_char field7;        /* field 7, >= 1 */
#define XDFM_FIELD7 0x03         /* xylogic value */
};


/*
 * errors: errors come from either the fatal error register or the
 * iopb
 */

#define XD_ERA_MASK 0xf0         /* error action mask */
#define XD_ERA_PROG 0x10         /* program error */
#define XD_ERA_PRG2 0x20         /* program error */
#define XD_ERA_SOFT 0x30         /* soft error: we recovered */
#define XD_ERA_HARD 0x40         /* hard error: retry */
#define XD_ERA_RSET 0x60         /* hard error: reset, then retry */
#define XD_ERA_WPRO 0x90         /* write protected */

/* software error codes */
#define XD_ERR_FAIL 0xff         /* general total failure */
/* no error */
#define XD_ERR_AOK  0x00         /* success */
/* non-retryable programming error */
#define XD_ERR_ICYL 0x10         /* illegal cyl */
#define XD_ERR_IHD  0x11         /* illegal head */
#define XD_ERR_ISEC 0x12         /* illegal sector */
#define XD_ERR_CZER 0x13         /* count zero */
#define XD_ERR_UIMP 0x14         /* unknown command */
#define XD_ERR_IF1  0x15         /* illegal field 1 */
#define XD_ERR_IF2  0x16         /* illegal field 2 */
#define XD_ERR_IF3  0x17         /* illegal field 3 */
#define XD_ERR_IF4  0x18         /* illegal field 4 */
#define XD_ERR_IF5  0x19         /* illegal field 5 */
#define XD_ERR_IF6  0x1a         /* illegal field 6 */
#define XD_ERR_IF7  0x1b         /* illegal field 7 */
#define XD_ERR_ISG  0x1c         /* illegal scatter/gather */
#define XD_ERR_ISPT 0x1d         /* not enough sectors per track */
#define XD_ERR_ALGN 0x1e         /* next iopb allignment error */
#define XD_ERR_SGAL 0x1f         /* scatter gather address alignment error */
#define XD_ERR_SGEC 0x20         /* scatter gather with auto ECC */
/* successfully recovered soft errors */
#define XD_ERR_SECC 0x30         /* soft ecc corrected */
#define XD_ERR_SIGN 0x31         /* ecc ignored */
#define XD_ERR_ASEK 0x32         /* auto-seek retry recovered */
#define XD_ERR_RTRY 0x33         /* soft retry recovered */
/* hard errors: please retry */
#define XD_ERR_HECC 0x40         /* hard data ECC */
#define XD_ERR_NHDR 0x41         /* header not found */
#define XD_ERR_NRDY 0x42         /* drive not ready */
#define XD_ERR_TOUT 0x43         /* timeout */
#define XD_ERR_VTIM 0x44         /* VME DMA timeout */
#define XD_ERR_DSEQ 0x45         /* disk sequencer error */
#define XD_ERR_HDEC 0x48         /* header ECC error */
#define XD_ERR_RVFY 0x49         /* ready verify */
#define XD_ERR_VFER 0x4a         /* fatal VME DMA error */
#define XD_ERR_VBUS 0x4b         /* VME bus error */
/* hard error: reset and retry */
#define XD_ERR_DFLT 0x60         /* drive fault */
#define XD_ERR_HECY 0x61         /* header error/cyl */
#define XD_ERR_HEHD 0x62         /* header error/head */
#define XD_ERR_NOCY 0x63         /* not on cylinder */
#define XD_ERR_SEEK 0x64         /* seek error */
/* fatal hardware error */
#define XD_ERR_ILSS 0x70         /* illegal sector size */
/* misc */
#define XD_ERR_SEC  0x80         /* soft ecc */
/* requires manual intervention */
#define XD_ERR_WPER 0x90         /* write protected */
/* FATAL errors */
#define XD_ERR_IRAM 0xe1         /* IRAM self test failed */
#define XD_ERR_MT3  0xe3         /* maint test 3 failed (DSKCEL RAM) */
#define XD_ERR_MT4  0xe4         /* maint test 4 failed (Header shift reg) */
#define XD_ERR_MT5  0xe5         /* maint test 5 failed (VMEDMA regs) */
#define XD_ERR_MT6  0xe6         /* maint test 6 failed (REGCEL chip) */
#define XD_ERR_MT7  0xe7         /* maint test 7 failed (buff. parity) */
#define XD_ERR_MT8  0xe8         /* maint test 8 failed (fifo) */
#define XD_ERR_IOCK 0xf0         /* iopb checksume miscompare */
#define XD_ERR_IODM 0xf1         /* iopb dma fatal */
#define XD_ERR_IOAL 0xf2         /* iopb allignment error */
#define XD_ERR_FIRM 0xf3         /* firmware error n*/
#define XD_ERR_MMOD 0xf5         /* illegal maint mode test number */
#define XD_ERR_ACFL 0xf6         /* ACFAIL asserted */
