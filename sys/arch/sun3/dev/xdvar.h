/* $NetBSD: xdvar.h,v 1.1 1995/10/30 20:58:19 gwr Exp $ */

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
 * x d v a r . h 
 *
 * this file defines the software structure we use to control the 
 * 753/7053.
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 */

/*
 * i/o request: wrapper for hardware's iopb data structure
 */

struct xd_iorq {
  struct xd_iopb *iopb;             /* address of matching iopb */
  struct xdc_softc *xdc;            /* who we are working with */
  struct xd_softc *xd;              /* which disk */
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

#define XD_SUB_MASK 0xf0            /* mask bits for state */
#define XD_SUB_FREE 0x00            /* free */
#define XD_SUB_NORM 0x10            /* normal I/O request */
#define XD_SUB_WAIT 0x20            /* normal I/O request in the 
                                             context of a process */
#define XD_SUB_POLL 0x30            /* polled mode */
#define XD_SUB_DONE 0x40            /* not active, but can't be free'd yet */
#define XD_SUB_NOQ  0x50            /* don't queue, just submit (internal) */

#define XD_STATE(X) ((X) & XD_SUB_MASK) /* extract state from mode */
#define XD_NEWSTATE(OLD, NEW) (((OLD) & ~XD_SUB_MASK) |(NEW)) /* new state */


/*
 * other mode data
 */

#define XD_MODE_VERBO 0x08          /* print error messages */
#define XD_MODE_B144  0x04          /* handling a bad144 sector */


/*
 * software timers and flags
 */

#define XDC_SUBWAITLIM 4   /* max number of "done" IOPBs there can be
				where we still allow a SUB_WAIT command */
#define XDC_TICKCNT (5*hz) /* call xdc_tick on this interval (5 sec) */
#define XDC_MAXTTL     2   /* max number of xd ticks to live */
#define XDC_NOUNIT (-1)    /* for xdcmd: no unit number */

/*
 * a "xd_softc" structure contains per-disk state info.
 */

struct xd_softc {
  struct device sc_dev;            /* device struct, reqd by autoconf */
  struct dkdevice sc_dk;           /* dkdevice: hook for iostat */
  struct xdc_softc *parent;        /* parent */
  u_short flags;                   /* flags */
  u_short state;                   /* device state */
  int xd_drive;                    /* unit number */
  /* geometry */
  u_short ncyl, acyl, pcyl;        /* number of cyl's */
  u_short sectpercyl;              /* nhead*nsect */
  u_char nhead;                    /* number of heads */
  u_char nsect;                    /* number of sectors per track */
  u_char hw_spt;                   /* as above, but includes spare sectors */
  struct dkbad dkb;                /* bad144 sectors */
};


/*
 * flags
 */

#define XD_WLABEL 0x0001           /* write label */
/*
 * state
 */

#define XD_DRIVE_UNKNOWN 0         /* never talked to it */
#define XD_DRIVE_ATTACHING 1       /* attach in progress */
#define XD_DRIVE_NOLABEL 2         /* drive on-line, no label */
#define XD_DRIVE_ONLINE  3         /* drive is on-line */

/*
 * a "xdc_softc" structure contains per-disk-controller state info,
 * including a list of active controllers.
 */

struct xdc_softc {
  struct device sc_dev;            /* device struct, reqd by autoconf */
  struct evcnt sc_intrcnt;         /* event counter (for vmstat -i) */

  struct xdc *xdc;                 /* vaddr of vme registers */

  struct xd_softc *sc_drives[XDC_MAXDEV];   /* drives on this controller */
  int ipl;                         /* interrupt level */
  int vector;                      /* interrupt vector */

  struct xd_iorq *reqs;            /* i/o requests */
  struct xd_iopb *iopbase;         /* iopb base addr (maps iopb->iorq) */
  struct xd_iopb *dvmaiopb;        /* iopb base in DVMA space, not kvm */
  struct buf sc_wq;                /* queue'd IOPBs for this controller */
  char freereq[XDC_MAXIOPB];       /* free list (stack) */
  char waitq[XDC_MAXIOPB];         /* wait queue */
  char nfree;                      /* number of iopbs free */
  char nrun;                       /* number running */
  char nwait;                      /* number of waiting iopbs */
  char ndone;                      /* number of done IORQs */
  char waithead;                   /* head of queue */
  char waitend;                    /* end of queue */
};

/*
 * reset blast modes
 */

#define XD_RSET_NONE (-1)          /* restart all requests */
#define XD_RSET_ALL  (-2)          /* don't restart anything */
