/*	$OpenBSD: xyreg.h,v 1.4 2004/04/12 22:12:32 jmc Exp $	*/
/*	$NetBSD: xyreg.h,v 1.3 1996/03/31 22:39:02 pk Exp $	*/

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
 * x y r e g . h
 *
 * this file contains the description of the Xylogics 450/451's hardware
 * data structures.
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 */

#define XYC_MAXDEV    2       /* max devices per controller */
#define XYC_CTLIOPB   XYC_MAXDEV /* controller's iopb */
#define XYC_RESETUSEC 1000000 /* max time for xyc reset (same as xdc?) */
#define XYC_MAXIOPB   (XYC_MAXDEV+1)
			      /* max number of iopbs that can be active */
#define XYC_MAXTIME   4*1000000 /* four seconds before we give up and reset */
#define XYC_MAXTRIES  4       /* max number of times to retry an operation */
#define XYC_INTERLEAVE 1      /* interleave (from disk label?) */
#define XYFM_BPS	0x200 /* must be 512! */

/*
 * xyc device interface
 * (lives in VME address space)   [note: bytes are swapped!]
 */

struct xyc {
  volatile u_char xyc_reloc_hi;        /* iopb relocation (low byte) */
  volatile u_char xyc_reloc_lo;        /* iopb relocation (high byte) */
  volatile u_char xyc_addr_hi;         /* iopb address (low byte) */
  volatile u_char xyc_addr_lo;         /* iopb address (high byte) */
  volatile u_char xyc_rsetup;          /* reset/update reg */
  volatile u_char xyc_csr;             /* control and status register */
};

/*
 * xyc_csr
 */

#define XYC_GBSY     0x80     /* go/busy */
#define XYC_ERR	     0x40     /* error */
#define XYC_DERR     0x20     /* double error! */
#define XYC_IPND     0x10     /* interrupt pending */
#define XYC_ADRM     0x08     /* 24-bit addressing */
#define XYC_AREQ     0x04     /* attention request */
#define XYC_AACK     0x02     /* attention ack. */
#define XYC_DRDY     0x01     /* drive ready */

/*
 * Input/Output Parameter Block (iopb)
 *
 * all controller commands are done via iopb's.   to start a command you
 * must do this:
 * [1] allocate space in DVMA space for the iopb
 * [2] fill out all the fields of the iopb
 * [3] if the controller isn't busy, start the iopb by loading the address
 *     and reloc in the xyc's registers and setting the "go" bit [done]
 * [4] controller busy: set AREQ bit, and wait for AACK bit.
 *     add iopb to the chain, and clear AREQ to resume I/O
 *
 * when the controller is done with a command it may interrupt (if you
 * ask it to) and it will set the XYC_IPND bit in the csr.   clear
 * the interrupt by writing one to this bit.
 *
 * the format of the iopb is described in section 2.4 of the manual.
 * note that it is byte-swapped on the sun.
 */

struct xy_iopb {
				 /* section 2.4.2: byte 1 */
  volatile u_char resv1:1;	 /* reserved */
  volatile u_char iei:1;	 /* interrupt on each IOPB done */
  volatile u_char ierr:1;	 /* interrupt on error (no effect on 450) */
  volatile u_char hdp:1;	 /* hold dual port drive */
  volatile u_char asr:1;	 /* autoseek retry */
  volatile u_char eef:1;	 /* enable extended fn. (overlap seek) */
  volatile u_char ecm:2;	 /* ECC correction mode */
#define XY_ECM 2		 /* use mode 2 (see section 2.4.2) */
                                 /* section 2.4.1: byte 0 */
  volatile u_char aud:1;	 /* auto-update iopb */
  volatile u_char relo:1;	 /* enable multibus relocation (>16bit addrs)*/
  volatile u_char chen:1;        /* chain enable, "next iopb" is valid */
  volatile u_char ien:1;	 /* interrupt enable */
  volatile u_char com:4;	 /* command */
#define XYCMD_NOP 0x0            /* no-op */
#define XYCMD_WR  0x1            /* write */
#define XYCMD_RD  0x2            /* read */
#define XYCMD_WTH 0x3		 /* write track headers */
#define XYCMD_RTH 0x4		 /* read track headers */
#define XYCMD_SK  0x5            /* seek */
#define XYCMD_RST 0x6            /* drive reset */
#define XYCMD_WFM 0x7		 /* write format */
#define XYCMD_RDH 0x8		 /* read header, data, and ECC */
#define XYCMD_RDS 0x9		 /* read drive status */
#define XYCMD_WRH 0xa		 /* write header, data, and ECC */
#define XYCMD_SDS 0xb		 /* set drive size */
#define XYCMD_ST  0xc		 /* self test */
#define XYCMD_R   0xd		 /* reserved */
#define XYCMD_MBL 0xe		 /* maint. buffer load */
#define XYCMD_MBD 0xf		 /* main. buffer dump */
                                 /* section 2.4.4: byte 3 */
  volatile u_char errno;	 /* error or completion code */
                                 /* section 2.4.3: byte 2 */
  volatile u_char errs:1;        /* error summary bit */
  volatile u_char resv2:2;	 /* reserved */
  volatile u_char ctyp:3;	 /* controller type */
#define XYCT_450 1		 /* the 450 controller */
  volatile u_char resv3:1;	 /* reserved */
  volatile u_char done:1;	 /* done! */
                                 /* section 2.4.6: byte 5 */
  volatile u_char dt:2;		 /* drive type */
#define XYC_MAXDT 3		 /* largest drive type possible */
  volatile u_char resv4:4;	 /* reserved */
  volatile u_char unit:2;	 /* unit # */
                                 /* section 2.4.5: byte 4 */
  volatile u_char bw:1;		 /* byte(1)/word(0) xfer size */
  volatile u_char intlv:4;	 /* interleave factor (0=1:1, 1=2:1, etc.) */
  volatile u_char thro:3;	 /* dma throttle (0=2,1=4,2=8, etc...) */
#define XY_THRO 4		 /* 4 == 32 dma cycles */
                                 /* section 2.4.8: byte 7 */
  volatile u_char sect;		 /* sector # */
                                 /* section 2.4.7: byte 6 */
  volatile u_char head;		 /* head # */
                                 /* section 2.4.9: byte 8,9 */
  volatile u_short cyl;		 /* cyl # */
                                 /* section 2.4.10: byte a,b */
  volatile u_short scnt;	 /* sector count, also drive status */
#define xy_dr_status scnt
#define XYS_ONCL 0x80		 /* on-cylinder (active LOW) */
#define XYS_DRDY 0x40		 /* drive ready (active LOW) */
#define XYS_WRPT 0x20		 /* write protect */
#define XYS_DPB  0x10		 /* dual-port busy */
#define XYS_SKER 0x08		 /* hard seek error */
#define XYS_DFLT 0x04		 /* disk fault */
                                 /* section 2.4.11: byte c,d */
  volatile u_short dataa;	 /* data address */
                                 /* section 2.4.12: byte e,f */
  volatile u_short datar;	 /* data relocation pointer */
				 /* section 2.4.14: byte 11 */
  volatile u_char subfn;	 /* sub-function */
				 /* section 2.4.13: byte 10 */
  volatile u_char hoff;		 /* head offset for fixed/removable drives */
				 /* section 2.4.15: byte 12,13 */
  volatile u_short nxtiopb;	 /* next iopb address (same relocation) */
				 /* section 2.4.16: byte 14,15 */
  volatile u_short eccpat;	 /* ecc pattern */
				 /* section 2.4.17: byte 16,17 */
  volatile u_short eccaddr;	 /* ecc address */
};


/*
 * errors (section 2.4.4.1)
 */

/* software error codes */
#define XY_ERR_FAIL 0xff         /* general total failure */
#define XY_ERR_DERR 0xfe	 /* double error */
/* no error */
#define XY_ERR_AOK  0x00         /* success */

#define XY_ERR_IPEN 0x01	 /* interrupt pending */
#define XY_ERR_BCFL 0x03	 /* busy conflict */
#define XY_ERR_TIMO 0x04	 /* operation timeout */
#define XY_ERR_NHDR 0x05	 /* header not found */
#define XY_ERR_HARD 0x06	 /* hard ECC error */
#define XY_ERR_ICYL 0x07	 /* illegal cylinder address */
#define XY_ERR_ISEC 0x0a	 /* illegal sector address */
#define XY_ERR_SMAL 0x0d	 /* last sector too small */
#define XY_ERR_SACK 0x0e	 /* slave ACK error (non-existent memory) */
#define XY_ERR_CHER 0x12	 /* cylinder and head/header error */
#define XY_ERR_SRTR 0x13	 /* auto-seek retry successful */
#define XY_ERR_WPRO 0x14	 /* write-protect error */
#define XY_ERR_UIMP 0x15	 /* unimplemented command */
#define XY_ERR_DNRY 0x16	 /* drive not ready */
#define XY_ERR_SZER 0x17	 /* sector count zero */
#define XY_ERR_DFLT 0x18	 /* drive faulted */
#define XY_ERR_ISSZ 0x19	 /* illegal sector size */
#define XY_ERR_SLTA 0x1a	 /* self test a */
#define XY_ERR_SLTB 0x1b	 /* self test b */
#define XY_ERR_SLTC 0x1c	 /* self test c */
#define XY_ERR_SOFT 0x1e	 /* soft ECC error */
#define XY_ERR_SFOK 0x1f	 /* soft ECC error recovered */
#define XY_ERR_IHED 0x20	 /* illegal head */
#define XY_ERR_DSEQ 0x21	 /* disk sequencer error */
#define XY_ERR_SEEK 0x25	 /* seek error */


/* error actions */
#define XY_ERA_PROG 0x10	 /* program error: quit */
#define XY_ERA_SOFT 0x30         /* soft error: we recovered */
#define XY_ERA_HARD 0x40         /* hard error: retry */
#define XY_ERA_RSET 0x60         /* hard error: reset, then retry */
#define XY_ERA_WPRO 0x90         /* write protected */


