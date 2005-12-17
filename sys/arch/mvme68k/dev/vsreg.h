/*	$OpenBSD: vsreg.h,v 1.6 2005/12/17 07:31:26 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from source contributed by Mark Bellon.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined(_M328REG_H_)
#define _M328REG_H_


typedef struct LONGV
{
        u_short  msw;
        u_short  lsw;
} LONGV;

#define MSW(x)  ((x).msw)
#define LSW(x)  ((x).lsw)

/*
 * macro to convert a unsigned long to a LONGV
 */

#define LV( a, b) \
{ \
  MSW( a ) = ( (( (unsigned long)(b) ) >> 16) & 0xffff ); \
  LSW( a ) = ( ( (unsigned long)(b) ) & 0xffff); \
}

/*
 * macro to convert a LONGV to a unsigned long
 */

#define VL( a, b) \
{ \
  a = ( (((unsigned long) MSW(b)) << 16 ) | (((unsigned long) LSW(b)) & 0x0ffff) ); \
}

#define         COUGAR          0x4220  /* board type (config. status area) */
#define         JAGUAR          0

/*
 *      JAGUAR specific device limits.
 */

#define JAGUAR_MIN_Q_SIZ                2       /* got'a have at least one! */
#define JAGUAR_MAX_Q_SIZ                2       /* can't have more */
#define JAGUAR_MAX_CTLR_CMDS            80      /* Interphase says so */

/*
 *      COUGAR specific device limits.
 */

#define COUGAR_MIN_Q_SIZ                2       /* got'a have at least one! */
#define COUGAR_CMDS_PER_256K            42      /* Interphase says so */

/*
 * Structures
 */

#define NUM_CQE                 10
#define MAX_IOPB                64
#define NUM_IOPB                NUM_CQE
#define S_IOPB_RES              (MAX_IOPB - sizeof(M328_short_IOPB))
#define S_SHORTIO               2048
#define S_IOPB                  sizeof(M328_IOPB)
#define S_CIB                   sizeof(M328_CIB)
#define S_MCSB                  sizeof(M328_MCSB)
#define S_MCE                   sizeof(M328_CQE)
#define S_CQE                   (sizeof(M328_CQE) * NUM_CQE)
#define S_HIOPB                 (sizeof(M328_IOPB) * NUM_IOPB)
#define S_HSB                   sizeof(M328_HSB)
#define S_CRB                   sizeof(M328_CRB)
#define S_CSS                   sizeof(M328_CSB)
#define S_NOT_HOST              (S_MCSB + S_MCE + S_CQE + S_HIOPB + S_IOPB + \
                                 S_CIB + S_HSB + S_CRB + S_IOPB + S_CSS)
#define S_HUS_FREE              (S_SHORTIO - S_NOT_HOST)

#define S_WQCF                  sizeof(M328_WQCF)

#define HOST_ID                 0x4321


/****************     Master Control Status Block (MCSB) *******************/

/*
 * defines for Master Status Register
 */

#define M_MSR_QFC               0x0004          /* queue flush complete */
#define M_MSR_BOK               0x0002          /* board OK */
#define M_MSR_CNA               0x0001          /* controller not available */

/*
 * defines for Master Control Register
 */

#define M_MCR_SFEN              0x2000          /* sysfail enable */
#define M_MCR_RES               0x1000          /* reset controller */
#define M_MCR_FLQ               0x0800          /* flush queue */
#define M_MCR_FLQR              0x0004          /* flush queue and report */
#define M_MCR_SQM               0x0001          /* start queue mode */

/*
 * defines for Interrupt on Queue Available Register
 */

#define M_IQAR_IQEA             0x8000  /* interrupt on queue entry avail */
#define M_IQAR_IQEH             0x4000  /* interrupt on queue half empty */
#define M_IQAR_ILVL             0x0700  /* interrupt lvl on queue available */
#define M_IQAR_IVCT             0x00FF  /* interrupt vector on queue avail */

/*
 * defines for Thaw Work Queue Register
 */

#define M_THAW_TWQN             0xff00          /* thaw work queue number */
#define M_THAW_TWQE             0x0001          /* thaw work queue enable */

typedef struct mcsb
{                                       /* Master control/Status Block */
    volatile u_short      mcsb_MSR;       /* Master status register */
    volatile u_short      mcsb_MCR;       /* Master Control register */
    volatile u_short      mcsb_IQAR;      /* Interrupt on Queue Available Reg */
    volatile u_short      mcsb_QHDP;      /* Queue head pointer */
    volatile u_short      mcsb_THAW;      /* Thaw work Queue */
    volatile u_short      mcsb_RES0;      /* Reserved word 0 */
    volatile u_short      mcsb_RES1;      /* Reserved word 1 */
    volatile u_short      mcsb_RES2;      /* Reserved word 2 */
} M328_MCSB;

/**************** END Master Control Status Block (MCSB) *******************/

/****************     Scater/Gather Stuff                *******************/

typedef struct {
   union {
      unsigned short bytes :16;
   #define MAX_SG_BLOCK_SIZE	(1<<16)	/* the size *has* to be always *smaller* */
      struct {
         unsigned short :8;
         unsigned short gather :8;
      } scatter;
   } count;
   LONGV           address;
   unsigned short  link :1;
   unsigned short  :3;
   unsigned short  transfer_type :2;
   /* 				0x0 is reserved */
   #define SHORT_TRANSFER 			0x1	
   #define LONG_TRANSFER			0x2	
   #define SCATTER_GATTER_LIST_IN_SHORT_IO	0x3	
   unsigned short  memory_type :2;
   #define NORMAL_TYPE				0x0	
   #define BLOCK_MODE				0x1	
   /*				0x2 is reserved */
   /*				0x3 is reserved */
   unsigned short  address_modifier :8;
}sg_list_element_t;

typedef sg_list_element_t * scatter_gather_list_t;

#define MAX_SG_ELEMENTS 64

struct m328_sg {
   struct m328_sg  *up;
   int                     elements;
   int                     level;
   struct m328_sg  *down[MAX_SG_ELEMENTS];
   sg_list_element_t list[MAX_SG_ELEMENTS];
};

typedef struct m328_sg *M328_SG;

typedef struct {
   struct scsi_xfer  *xs;
   M328_SG           top_sg_list;
} M328_CMD;
/**************** END Scater/Gather Stuff                *******************/

/****************     Host Semaphore Block (HSB)         *******************/

typedef struct hsb
{                                       /* Host Semaphore Block */
    volatile u_short      hsb_INITQ;      /* Init MCE Flag */
    volatile u_short      hsb_WORKQ;      /* Work Queue number */
    volatile u_short      hsb_MAGIC;      /* Magic word */
    volatile u_short      hsb_RES0;       /* Reserved word */
} M328_HSB;

/**************** END Host Semaphore Block (HSB)         *******************/

/****************     Perform Diagnostics Command Format *******************/

typedef struct pdcf
{                                       /* Perform Diagnostics Cmd Format */
    volatile u_short      pdcf_CMD;       /* Command normally 0x40 */
    volatile u_short      pdcf_RES0;      /* Reserved word */
    volatile u_short      pdcf_STATUS;    /* Return Status */
    volatile u_short      pdcf_RES1;      /* Reserved Word */
    volatile u_short      pdcf_ROM;       /* ROM Test Results */
    volatile u_short      pdcf_BUFRAM;    /* Buffer RAM results */
    volatile u_short      pdcf_EVENT_RAM; /* Event Ram test Results */
    volatile u_short      pdcf_SCSI_PRI_PORT; /* SCSI Primary Port Reg test */
    volatile u_short      pdcf_SCSI_SEC_PORT; /* SCSI Secondary Port Reg test */
} M328_PDCF;

#define PDCF_SUCCESS            0xFFFF

/**************** END Perform Diagnostics Command Format *******************/

/***************      Controller Initialization Block (CIB) *****************/

/*
 * defines for Interrupt Vectors
 */

#define M_VECT_ILVL             0x0700  /* Interrupt Level */
#define M_VECT_IVCT             0x00FF  /* Interrupt Vector */

/*
 * defines for SCSI Bus ID Registers
 */

#define M_PSID_DFT              0x0008  /* default ID enable */
#define M_PSID_ID               0x0007  /* Primary/Secondary SCSI ID */

/*
 *      Error recovery flags.
 */

#define M_ERRFLGS_FOSR          0x0001  /* Freeze on SCSI bus reset */
#define M_ERRFLGS_RIN           0x0002  /* SCSI bus reset interrupt */
#define M_ERRFLGS_RSE           0x0004  /* Report COUGAR SCSI errors */

/*
 * Controller Initialization Block
 */

typedef struct cib
{
    volatile u_short      cib_NCQE;       /* Number of Command Queue Entries */
    volatile u_short      cib_BURST;      /* DMA Burst count */
    volatile u_short      cib_NVECT;      /* Normal Completion Vector */
    volatile u_short      cib_EVECT;      /* Error Completion Vector */
    volatile u_short      cib_PID;        /* Primary SCSI Bus ID */
    volatile u_short      cib_SID;        /* Secondary SCSI Bus ID */
    volatile u_short      cib_CRBO;       /* Command Response Block Offset */
    volatile u_short      cib_SELECT_msw;/* Selection timeout in milli_second */
    volatile u_short      cib_SELECT_lsw;/* Selection timeout in milli_second */
    volatile u_short      cib_WQ0TIMO_msw;/* Work Q - timeout in 256 ms */
    volatile u_short      cib_WQ0TIMO_lsw;/* Work Q - timeout in 256 ms */
    volatile u_short      cib_VMETIMO_msw;/* VME Time out in 32 ms */
    volatile u_short      cib_VMETIMO_lsw;/* VME Time out in 32 ms */
    volatile u_short      cib_RES0[2];    /* Reserved words */
    volatile u_short      cib_OBMT;       /* offbrd CRB mtype/xfer type/ad mod */
    volatile u_short      cib_OBADDR_msw;/* host mem address for offboard CRB */
    volatile u_short      cib_OBADDR_lsw;/* host mem address for offboard CRB */
    volatile u_short      cib_ERR_FLGS;   /* error recovery flags */
    volatile u_short      cib_RES1;       /* reserved word */
    volatile u_short      cib_RES2;       /* reserved word */
    volatile u_short      cib_SBRIV;      /* SCSI Bus Reset Interrupt Vector */
    volatile u_char       cib_SOF0;       /* Synchronous offset (Bus 0) */
    volatile u_char       cib_SRATE0;     /* Sync negotiation rate (Bus 0) */
    volatile u_char       cib_SOF1;       /* Synchronous offset (Bus 1) */
    volatile u_char       cib_SRATE1;     /* Sync negotiation rate (Bus 1) */
} M328_CIB;

/**************** END Controller Initialization Block (CIB) *****************/

/****************     Command Queue Entry (CQE)          *******************/

/*
 * defines for Queue Entry Control Register
 */

#define M_QECR_IOPB             0x0F00  /* IOPB type (must be zero) */
#define M_QECR_HPC              0x0004  /* High Priority command */
#define M_QECR_AA               0x0002  /* abort acknowledge */
#define M_QECR_GO               0x0001  /* Go/Busy */

#define CQE_GO(qecr)            ((qecr) |= M_QECR_GO)
#define CQE_AA_GO(qecr)         ((qecr) |= (M_QECR_GO + M_QECR_AA))

typedef struct cqe
{                                       /* Command Queue Entry */
    volatile u_short      cqe_QECR;       /* Queue Entry Control Register */
    volatile u_short      cqe_IOPB_ADDR;  /* IOPB Address */
    volatile LONGV        cqe_CTAG;       /* Command Tag */
    volatile u_char       cqe_IOPB_LENGTH;/* IOPB Length */
    volatile u_char       cqe_WORK_QUEUE; /* Work Queue Number */
    volatile u_short      cqe_RES0;       /* Reserved word */
} M328_CQE;

/**************** END Command Queue Entry (CQE)          *******************/

/****************     Command Response Block (CRB)       *******************/

/*
 * defines for Command Response Status Word
 */

#define M_CRSW_SE               0x0800  /* SCSI error (COUGAR) */
#define M_CRSW_RST              0x0400  /* SCSI Bus reset (COUGAR) */
#define M_CRSW_SC               0x0080  /* status change */
#define M_CRSW_CQA              0x0040  /* Command queue entry available */
#define M_CRSW_QMS              0x0020  /* queue mode started */
#define M_CRSW_AQ               0x0010  /* abort queue */
#define M_CRSW_EX               0x0008  /* exception */
#define M_CRSW_ER               0x0004  /* error */
#define M_CRSW_CC               0x0002  /* command complete */
#define M_CRSW_CRBV             0x0001  /* cmd response block valid/clear */

#define CRB_CLR_DONE(crsw)      ((crsw) = 0)
#define CRB_CLR_ER(crsw)        ((crsw) &= ~M_CRSW_ER)

typedef struct crb
{                                       /* Command Response Block */
    volatile u_short    crb_CRSW;       /* Command Response Status Word */
    volatile u_short    crb_RES0;       /* Reserved word */
    volatile LONGV      crb_CTAG;       /* Command Tag */
    volatile u_char     crb_IOPB_LENGTH;/* IOPB Length */
    volatile u_char     crb_WORK_QUEUE; /* Work Queue Number */
    volatile u_short    crb_RES1;       /* Reserved word */
} M328_CRB;

/**************** END Command Response Block (CRB)       *******************/

/***********     Controller Error Vector Status Block (CEVSB) **************/

typedef struct cevsb
{                                       /* Command Response Block */
    volatile u_short      cevsb_CRSW;     /* Command Response Status Word */
    volatile u_char       cevsb_TYPE;     /* IOPB type */
    volatile u_char       cevsb_RES0;     /* Reserved byte */
    volatile LONGV        cevsb_CTAG;     /* Command Tag */
    volatile u_char       cevsb_IOPB_LENGTH;/* IOPB Length */
    volatile u_char       cevsb_WORK_QUEUE;/* Work Queue Number */
    volatile u_short      cevsb_RES1;     /* Reserved word */
    volatile u_char       cevsb_RES2;     /* Reserved byte */
    volatile u_char       cevsb_ERROR;    /* error code */
    volatile u_short      cevsb_AUXERR;   /* COUGAR error code */
} M328_CEVSB;

/***********  END Controller Error Vector Status Block (CEVSB) **************/

/****************     Configuration Status Block (CSB)   *******************/

typedef struct csb
{                                       /* Configuration Status Blk */
    volatile u_short      csb_TYPE;       /* 0x0=JAGUAR, 0x4220=COUGAR */
    volatile u_char       csb_RES1;       /* Reserved byte */
    volatile u_char       csb_PCODE[3];   /* Product Code */
    volatile u_short      csb_RES2;       /* Reserved word */
    volatile u_char       csb_RES3;       /* Reserved byte */
    volatile u_char       csb_PVAR;       /* Product Variation */
    volatile u_short      csb_RES4;       /* Reserved word */
    volatile u_char       csb_RES5;       /* Reserved byte */
    volatile u_char       csb_FREV[3];    /* Firmware Revision level */
    volatile u_short      csb_RES6;       /* Reserved word */
    volatile u_char       csb_FDATE[8];   /* Firmware Release date */
    volatile u_short      csb_SSIZE;      /* System memory size in Kbytes */
    volatile u_short      csb_BSIZE;      /* Buffer memory size in Kbytes */
    volatile u_short      csb_RES8;       /* Reserved word */
    volatile u_char       csb_PFECID;     /* Primary Bus FEC ID */
    volatile u_char       csb_SFECID;     /* Secondard Bus FEC ID */
    volatile u_char       csb_PID;        /* Primary Bus ID */
    volatile u_char       csb_SID;        /* Secondary Bus ID */
    volatile u_char       csb_LPDS;       /* Last Primary Device Selected */
    volatile u_char       csb_LSDS;       /* Last Secondary Device Selected */
    volatile u_char       csb_PPS;        /* Primary Phase Sense */
    volatile u_char       csb_SPS;        /* Secondary Phase Sense */
    volatile u_char       csb_RES10;      /* Reserved byte */
    volatile u_char       csb_DBID;       /* Daughter Board ID */
    volatile u_char       csb_RES11;      /* Reserved byte */
    volatile u_char       csb_SDS;        /* Software DIP Switch */
    volatile u_short      csb_RES12;      /* Reserved word */
    volatile u_short      csb_FWQR;       /* Frozen Work Queues Register */
    volatile u_char       csb_RES13[72];  /* Reserved bytes */
} M328_CSB;

/**************** END Configuration Status Block (CSB)   *******************/

/****************     IOPB Format (IOPB)                 *******************/

/*
 * defines for IOPB Option Word
 */

#define M_OPT_HEAD_TAG          0x3000  /* head of queue command queue tag */
#define M_OPT_ORDERED_TAG       0x2000  /* order command queue tag */
#define M_OPT_SIMPLE_TAG        0x1000  /* simple command queue tag */
#define M_OPT_GO_WIDE           0x0800  /* use WIDE transfers */
#define M_OPT_DIR               0x0100  /* VME direction bit */
#define M_OPT_SG_BLOCK          0x0008  /* scatter/gather in 512 byte blocks */
#define M_OPT_SS                0x0004  /* Suppress synchronous transfer */
#define M_OPT_SG                0x0002  /* scatter/gather bit */
#define M_OPT_IE                0x0001  /* Interrupt enable */

/*
 * defines for IOPB Address Type and Modifier
 */

#define M_ADR_TRANS             0x0C00  /* transfer type */
#define M_ADR_MEMT              0x0300  /* memory type */
#define M_ADR_MOD               0x00FF  /* VME address modifier */
#define M_ADR_SG_LINK           0x8000  /* Scatter/Gather Link bit */

/*
 * defines for IOPB Unit Address on SCSI Bus
 */

#define M_UNIT_EXT_LUN          0xFF00  /* Extended Address */
#define M_UNIT_EXT              0x0080  /* Extended Address Enable */
#define M_UNIT_BUS              0x0040  /* SCSI Bus Selection */
#define M_UNIT_LUN              0x0038  /* Logical Unit Number */
#define M_UNIT_ID               0x0007  /* SCSI Device ID */

typedef struct short_iopb
{
    volatile u_short      iopb_CMD;       /* IOPB Command code */
    volatile u_short      iopb_OPTION;    /* IOPB Option word */
    volatile u_short      iopb_STATUS;    /* IOPB Return Status word */
    volatile u_short      iopb_RES0;      /* IOPB Reserved word */
    volatile u_char       iopb_NVCT;      /* IOPB Normal completion Vector */
    volatile u_char       iopb_EVCT;      /* IOPB Error  completion Vector */
    volatile u_short      iopb_LEVEL;     /* IOPB Interrupt Level */
    volatile u_short      iopb_RES1;      /* IOPB Reserved word */
    volatile u_short      iopb_ADDR;      /* IOPB Address type and modifer */
    volatile LONGV        iopb_BUFF;      /* IOPB Buffer Address */
    volatile LONGV        iopb_LENGTH;    /* IOPB Max-Transfer Length */
    volatile LONGV        iopb_SGTTL;     /* IOPB Scatter/Gather Total Transfer len */
    volatile u_short      iopb_RES4;      /* IOPB Reserved word */
    volatile u_short      iopb_UNIT;      /* IOPB Unit address on SCSI bus */
} M328_short_IOPB;

typedef struct iopb
{
    volatile u_short      iopb_CMD;       /* IOPB Command code */
    volatile u_short      iopb_OPTION;    /* IOPB Option word */
    volatile u_short      iopb_STATUS;    /* IOPB Return Status word */
    volatile u_short      iopb_RES0;      /* IOPB Reserved word */
    volatile u_char       iopb_NVCT;      /* IOPB Normal completion Vector */
    volatile u_char       iopb_EVCT;      /* IOPB Error  completion Vector */
    volatile u_short      iopb_LEVEL;     /* IOPB Interrupt Level */
    volatile u_short      iopb_RES1;      /* IOPB Reserved word */
    volatile u_short      iopb_ADDR;      /* IOPB Address type and modifer */
    volatile LONGV        iopb_BUFF;      /* IOPB Buffer Address */
    volatile LONGV        iopb_LENGTH;    /* IOPB Max-Transfer Length */
    volatile LONGV        iopb_SGTTL;     /* IOPB Scatter/Gather Total Transfer len */
    volatile u_short      iopb_RES4;      /* IOPB Reserved word */
    volatile u_short      iopb_UNIT;      /* IOPB Unit address on SCSI bus */
    u_short      iopb_SCSI[S_IOPB_RES/2]; /* IOPB SCSI words for pass thru */
} M328_IOPB;

/**************** END IOPB Format (IOPB)                 *******************/

/****************     Initialize Work Queue Command Format (WQCF) ***********/

#define M_WOPT_IWQ              0x8000          /* initialize work queue */
#define M_WOPT_PE               0x0008          /* parity check enable */
#define M_WOPT_FE               0x0004          /* freeze on error enable */
#define M_WOPT_TM               0x0002          /* target mode enable */
#define M_WOPT_AE               0x0001          /* abort enable */

typedef struct wqcf
{                                       /* Initialize Work Queue Cmd Format*/
    volatile u_short      wqcf_CMD;       /* Command Normally (0x42) */
    volatile u_short      wqcf_OPTION;    /* Command Options */
    volatile u_short      wqcf_STATUS;    /* Return Status */
    volatile u_short      wqcf_RES0;      /* Reserved word */
    volatile u_char       wqcf_NVCT;      /* Normal Completion Vector */
    volatile u_char       wqcf_EVCT;      /* Error Completion Vector */
    volatile u_short      wqcf_ILVL;      /* Interrupt Level */
    volatile u_short      wqcf_RES1[8];   /* Reserved words */
    volatile u_short      wqcf_WORKQ;     /* Work Queue Number */
    volatile u_short      wqcf_WOPT;      /* Work Queue Options */
    volatile u_short      wqcf_SLOTS;     /* Number of slots in Work Queues */
    volatile u_short      wqcf_RES2;      /* Reserved word */
    volatile LONGV        wqcf_CMDTO;     /* Command timeout */
    volatile u_short      wqcf_RES3;      /* Reserved word */
} M328_WQCF;

/**************** END Initialize Work Queue Command Format (WQCF) ***********/

/****************     SCSI Reset Command Format (SRCF) ***********/

typedef struct srcf
{                                       /* SCSI Reset Cmd Format*/
    volatile u_short      srcf_CMD;       /* Command Normally (0x22) */
    volatile u_short      srcf_OPTION;    /* Command Options */
    volatile u_short      srcf_STATUS;    /* Return Status */
    volatile u_short      srcf_RES0;      /* Reserved word */
    volatile u_char       srcf_NVCT;      /* Normal Completion Vector */
    volatile u_char       srcf_EVCT;      /* Error Completion Vector */
    volatile u_short      srcf_ILVL;      /* Interrupt Level */
    volatile u_short      srcf_RES1[8];   /* Reserved words */
    volatile u_short      srcf_BUSID;     /* SCSI bus ID to reset */
} M328_SRCF;

/**************** END SCSI Reset Command Format (SRCF) ***********/

/****************     Device Reinitialize Command Format (DRCF) ***********/

typedef struct drcf
{                                       /* Device Reinitialize Cmd Format*/
    volatile u_short      drcf_CMD;       /* Command Normally (0x4C) */
    volatile u_short      drcf_OPTION;    /* Command Options */
    volatile u_short      drcf_STATUS;    /* Return Status */
    volatile u_short      drcf_RES0;      /* Reserved word */
    volatile u_char       drcf_NVCT;      /* Normal Completion Vector */
    volatile u_char       drcf_EVCT;      /* Error Completion Vector */
    volatile u_short      drcf_ILVL;      /* Interrupt Level */
    volatile u_short      drcf_RES1[9];   /* Reserved words */
    volatile u_short      drcf_UNIT;      /* Unit Address */
} M328_DRCF;

/**************** END SCSI Reset Command Format (SRCF) ***********/

/**************** Host Down Loadable Firmware (HDLF) ***********/

typedef struct hdlf
{                                       /* Host Down Loadable Firmware cmd */
    volatile u_short      hdlf_CMD;       /* Command Normally (0x4F) */
    volatile u_short      hdlf_OPTION;    /* Command Options */
    volatile u_short      hdlf_STATUS;    /* Return Status */
    volatile u_short      hdlf_RES0;      /* Reserved word */
    volatile u_char       hdlf_NVCT;      /* Normal Completion Vector */
    volatile u_char       hdlf_EVCT;      /* Error Completion Vector */
    volatile u_short      hdlf_ILVL;      /* Interrupt Level */
    volatile u_short      hdlf_RES1;      /* Reserved word */
    volatile u_short      hdlf_ADDR;      /* Address type and modifer */
    volatile LONGV        hdlf_BUFF;      /* Buffer Address */
    volatile LONGV        hdlf_LENGTH;    /* Max-Transfer Length */
    volatile LONGV        hdlf_CSUM;      /* Checksum */
    volatile u_short      hdlf_RES2;      /* Reserved word */
    volatile u_short      hdlf_SEQ;       /* Sequence number */
    volatile u_short      hdlf_RES3[6];   /* Reserved words */
} M328_HDLF;

#define M328_INITIALIZE_DOWNLOAD        0x0010
#define M328_TRANSFER_PACKET            0x0020
#define M328_PROGRAM_FLASH              0x0040
#define M328_MOTOROLA_S_RECORDS         0x1000

/**************** END SCSI Reset Command Format (SRCF) ***********/

/****************     Short I/O Format                   *******************/

struct vsreg
{
    M328_MCSB   sh_MCSB;           /* Master Control / Status Block */
    M328_CQE    sh_MCE;            /* Master Command Entry */
    M328_CQE    sh_CQE[NUM_CQE];   /* Command Queue Entry */
    M328_IOPB   sh_IOPB[NUM_IOPB]; /* Host IOPB   Space */
    M328_IOPB   sh_MCE_IOPB;       /* Host MCE IOPB Space */
    M328_CIB    sh_CIB;            /* Controller Initialization Block */
    volatile u_char sh_HUS[S_HUS_FREE];/* Host Usable Space */
    M328_HSB    sh_HSB;            /* Host Semaphore Block */
    M328_CRB    sh_CRB;            /* Command Response Block */
    M328_IOPB   sh_RET_IOPB;       /* Returned IOPB */
    M328_CSB    sh_CSS;            /* Controller Specific Space/Block */
};

#define CRSW sc->sc_vsreg->sh_CRB.crb_CRSW
#define THAW_REG sc->sc_vsreg->sh_MCSB.mcsb_THAW
#define THAW(x) THAW_REG=((u_char)x << 8);THAW_REG |= M_THAW_TWQE
#define QUEUE_FZN(x) (sc->sc_vsreg->sh_CSS.csb_FWQR & (1 << x))
#define SELECTION_TIMEOUT               250     /* milliseconds */
#define VME_BUS_TIMEOUT                 0xF     /* units of 30ms */
#define M328_INFINITE_TIMEOUT           0       /* wait forever */

/**************** END Short I/O Format                   *******************/

/*
 * Scatter gather structure
 */

typedef struct ipsg
{
    volatile u_short      sg_count;       /* byte/entry count */
    volatile u_short      sg_addrhi;      /* datablock/entry address high */
    volatile u_short      sg_addrlo;      /* datablock/entry address low */
    volatile u_short      sg_meminfo;     /* memory information */
}IPSG;

#define MACSI_SG        256     /* number of MACSI scat/gat entries     */
#define S_MACSI_SG      (MACSI_SG * sizeof(IPSG))
#define MACSI_SG_RSIZE  65535   /* max len of each scatter/gather entry */

/*
 *  SCSI IOPB definitions
 */

#define IOPB_PASS_THRU      0x20    /* SCSI Pass Through commands */
#define IOPB_PASS_THRU_EXT  0x21    /* SCSI Pass Through Extended commands */
#define IOPB_RESET          0x22    /* SCSI Reset bus */

/*
 *  SCSI Control IOPB's
 */

#define CNTR_DIAG           0x40        /* Perform Diagnostics */
#define CNTR_INIT           0x41        /* Initialize Controller */
#define CNTR_INIT_WORKQ     0x42        /* Initialize Work Queue */
#define CNTR_DUMP_INIT      0x43        /* Dump Initialization Parameters */
#define CNTR_DUMP_WORDQ     0x44        /* Dump work Queue Parameters */
#define CNTR_CANCEL_IOPB    0x48        /* Cancel command tag */
#define CNTR_FLUSH_WORKQ    0x49        /* Flush Work Queue */
#define CNTR_DEV_REINIT     0x4C        /* Reinitialize Device */
#define CNTR_ISSUE_ABORT    0x4E        /* An abort has been issued */
#define CNTR_DOWNLOAD_FIRMWARE 0x4F     /* Download firmware (COUGAR) */


/*
 *  Memory types
 */

#define MEMT_16BIT              1       /* 16 Bit Memory type */
#define MEMT_32BIT              2       /* 32 Bit Memory type */
#define MEMT_SHIO               3       /* Short I/O Memory type */
#define MEMTYPE         MEMT_32BIT      /* do 32-bit transfers */

/*
 *  Transfer types
 */

#define TT_NORMAL               0       /* Normal Mode Tranfers */
#define TT_BLOCK                1       /* block  Mode Tranfers */
#define TT_DISABLE_INC_ADDR     2       /* Disable Incrementing Addresses */
#define TT_D64                  3       /* D64 Mode Transfers */

/*
 *      Error codes.
 */

#define MACSI_GOOD_STATUS       0x00    /* Good status */
#define MACSI_QUEUE_FULL        0x01    /* The work queue is full */
#define MACSI_CMD_CODE_ERR      0x04    /* The IOPB command field is invalid */
#define MACSI_QUEUE_NUMBER_ERR  0x05    /* Invalid queue number */

#define RESET_BUS_STATUS        0x11    /* SCSI bus reset IOPB forced this */
#define NO_SECONDARY_PORT       0x12    /* second SCSI bus not available */
#define SCSI_DEVICE_IS_RESET    0x14    /* device has been reset */
#define CMD_ABORT_BY_RESET      0x15    /* device has been reset */

#define VME_BUS_ERROR           0x20    /* There was a VME BUS error */
#define VME_BUS_ACC_TIMEOUT     0x21
#define VME_BUS_BAD_ADDR        0x23
#define VME_BUS_BAD_MEM_TYPE    0x24
#define VME_BUS_BAD_COUNT       0x25
#define VME_BUS_FETCH_ERROR     0x26
#define VME_BUS_FETCH_TIMEOUT   0x27
#define VME_BUS_POST_ERROR      0x28
#define VME_BUS_POST_TIMEOUT    0x29
#define VME_BUS_BAD_FETCH_ADDR  0x2A
#define VME_BUS_BAD_POST_ADDR   0x2B
#define VME_BUS_SG_FETCH        0x2C
#define VME_BUS_SG_TIMEOUT      0x2D
#define VME_BUS_SG_COUNT        0x2E

#define SCSI_SELECTION_TO       0x30    /* select time out */
#define SCSI_DISCONNECT_TIMEOUT 0x31    /* disconnect timeout */
#define SCSI_ABNORMAL_SEQ       0x32    /* abnormal sequence */
#define SCSI_DISCONNECT_ERR     0x33    /* disconnect error */
#define SCSI_XFER_EXCEPTION     0x34    /* transfer cnt exception */
#define SCSI_PARITY_ERROR       0x35    /* parity error */

#define DEVICE_NO_IOPB          0x82    /* IOPB no available */
#define IOPB_CTLR_EHX           0x83    /* IOPB counter exhausted */
#define IOPB_DIR_ERROR          0x84    /* IOPB direction wrong */
#define COUGAR_ERROR            0x86    /* COUGAR unrecoverable error */
#define MACSI_INCORRECT_HARDWARE 0x90    /* Insufficient memory */
#define MACSI_ILGL_IOPB_VAL     0x92    /* Invalid field in the IOPB */
#define MACSI_ILLEGAL_IMAGE     0x9C    /* Submitted fails reuested action */
#define IOPB_TYPE_ERR           0xC0    /* IOPB type not 0 */
#define IOPB_TIMEOUT            0xC1    /* IOPB timed out */

#define COUGAR_PANIC            0xFF    /* COUGAR paniced */

#define MACSI_INVALID_TIMEOUT   0x843   /* The SCSI byte to byte timer expired */

/*
 *      Handy vector macro.
 */

#define VEC(c, vec)     (((c) -> mc_ipl << 8) + (vec))

/*
 *      VME addressing modes
 */

#define ADRM_STD_S_P            0x3E    /* Standard Supervisory Program */
#define ADRM_STD_S_D            0x3D    /* Standard Supervisory Data */
#define ADRM_STD_N_P            0x3A    /* Standard Normal Program */
#define ADRM_STD_N_D            0x39    /* Standard Normal Data */
#define ADRM_SHT_S_IO           0x2D    /* Short Supervisory IO */
#define ADRM_SHT_N_IO           0x29    /* Short Normal IO */
#define ADRM_EXT_S_P            0x0E    /* Extended Supervisory Program */
#define ADRM_EXT_S_D            0x0D    /* Extended Supervisory Data */
#define ADRM_EXT_N_P            0x0A    /* Extended Normal Program */
#define ADRM_EXT_N_D            0x09    /* Extended Normal Data */
#define ADRM_EXT_S_BM           0x0F    /* Extended Supervisory Block Mode */
#define ADRM_EXT_S_D64          0x0C    /* Extended Supervisory D64 Mode */

#define ADDR_MOD        ( (TT_NORMAL << 10) | (MEMTYPE << 8) | ADRM_EXT_S_D )
#define BLOCK_MOD       ( (TT_BLOCK << 10) | (MEMTYPE << 8) | ADRM_EXT_S_BM )
#define D64_MOD         ( (TT_D64 << 10) | (MEMTYPE << 8) | ADRM_EXT_S_D64 )
#define SHIO_MOD        ( (TT_NORMAL << 10) | (MEMT_SHIO << 8) | ADRM_SHT_N_IO)

/*
 * Scatter/gather functions
 */

M328_SG vs_alloc_scatter_gather(void);
void    vs_dealloc_scatter_gather(M328_SG sg);
M328_SG vs_build_memory_structure(struct scsi_xfer *xs, M328_IOPB *iopb);

#endif /* _M328REG_H_ */
