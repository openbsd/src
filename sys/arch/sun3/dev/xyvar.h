/* $NetBSD: xyvar.h,v 1.2 1996/01/07 22:03:22 thorpej Exp $ */

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
 * x y v a r . h 
 *
 * this file defines the software structure we use to control the 
 * 450/451.
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 */

/*
 * i/o request: wrapper for hardware's iopb data structure
 */

struct xy_iorq {
  struct xy_iopb *iopb;             /* address of matching iopb */
  struct xyc_softc *xyc;            /* who we are working with */
  struct xy_softc *xy;              /* which disk */
  int ttl;                          /* time to live */
  int mode;                         /* current mode (state+other data) */
  int tries;                        /* number of times we have tried it */
  int errno;                        /* error number if we fail */
  int lasterror;		    /* last error we got */
  int blockno;                      /* starting block no for this xfer */
  int sectcnt;                      /* number of sectors in xfer */
  char *dbuf;                       /* KVA of data buffer (advances) */
  char *dbufbase;                   /* base of dbuf */
  struct buf *buf;                  /* for NORM */
};

/*
 * state
 */

#define XY_SUB_MASK 0xf0            /* mask bits for state */
#define XY_SUB_FREE 0x00            /* free */
#define XY_SUB_NORM 0x10            /* normal I/O request */
#define XY_SUB_WAIT 0x20            /* normal I/O request in the 
                                             context of a process */
#define XY_SUB_POLL 0x30            /* polled mode */
#define XY_SUB_DONE 0x40            /* not active, but can't be free'd yet */
#define XY_SUB_NOQ  0x50            /* don't queue, just submit (internal) */

#define XY_STATE(X) ((X) & XY_SUB_MASK) /* extract state from mode */
#define XY_NEWSTATE(OLD, NEW) (((OLD) & ~XY_SUB_MASK) |(NEW)) /* new state */


/*
 * other mode data
 */

#define XY_MODE_VERBO 0x08          /* print error messages */
#define XY_MODE_B144  0x04          /* handling a bad144 sector */


/*
 * software timers and flags
 */

#define XYC_SUBWAITLIM 4   /* max number of "done" IOPBs there can be
				where we still allow a SUB_WAIT command */
#define XYC_TICKCNT (5*hz) /* call xyc_tick on this interval (5 sec) */
#define XYC_MAXTTL     2   /* max number of xy ticks to live */
#define XYC_NOUNIT (-1)    /* for xycmd: no unit number */

/*
 * a "xy_softc" structure contains per-disk state info.
 */

struct xy_softc {
  struct device sc_dev;            /* device struct, reqd by autoconf */
  struct disk sc_dk;               /* generic disk info */
  struct xyc_softc *parent;        /* parent */
  u_short flags;                   /* flags */
  u_short state;                   /* device state */
  int xy_drive;                    /* unit number */
  int drive_type;		   /* drive type (as per disk) */
  /* geometry */
  u_short ncyl, acyl, pcyl;        /* number of cyl's */
  u_short sectpercyl;              /* nhead*nsect */
  u_char nhead;                    /* number of heads */
  u_char nsect;                    /* number of sectors per track */
  u_char hw_spt;                   /* as above, but includes spare sectors */
  struct xy_iorq *xyrq;		   /* this disk's ioreq structure */
  struct buf xyq;		   /* queue'd I/O requests */
  struct dkbad dkb;                /* bad144 sectors */
};

/*
 * flags
 */

#define XY_WLABEL 0x0001           /* write label */
/*
 * state
 */

#define XY_DRIVE_UNKNOWN 0         /* never talked to it */
#define XY_DRIVE_ATTACHING 1       /* attach in progress */
#define XY_DRIVE_NOLABEL 2         /* drive on-line, no label */
#define XY_DRIVE_ONLINE  3         /* drive is on-line */

/*
 * a "xyc_softc" structure contains per-disk-controller state info,
 * including a list of active controllers.
 */

struct xyc_softc {
  struct device sc_dev;            /* device struct, reqd by autoconf */
  struct evcnt sc_intrcnt;         /* event counter (for vmstat -i) */

  struct xyc *xyc;                 /* vaddr of vme registers */

  struct xy_softc *sc_drives[XYC_MAXDEV];   /* drives on this controller */
  int ipl;                         /* interrupt level */
  int vector;                      /* interrupt vector */

  struct xy_iorq *reqs;            /* i/o requests */
  struct xy_iopb *iopbase;         /* iopb base addr (maps iopb->iorq) */
  struct xy_iopb *dvmaiopb;        /* iopb base in DVMA space, not kvm */

  struct xy_iorq *ciorq;	   /* controller's iorq */
  struct xy_iopb *ciopb;	   /* controller's iopb */

  int xy_hand;			   /* hand */
  struct xy_iorq *xy_chain[XYC_MAXIOPB];
				   /* current chain */
  int no_ols;			   /* disable overlap seek for stupid 450s */
};

/*
 * reset blast modes
 */

#define XY_RSET_NONE (struct xy_iorq *)(-1)   /* restart all requests */
#define XY_RSET_ALL  (struct xy_iorq *)(-2)   /* don't restart anything */
