/*	$OpenBSD: sbicvar.h,v 1.9 2010/03/27 16:04:24 miod Exp $ */

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *
 *  @(#)scsivar.h   7.1 (Berkeley) 5/8/90
 */
#ifndef _SBICVAR_H_
#define _SBICVAR_H_
#include <sys/malloc.h>


/*
 * DMA chains are used for Scatter-Gather DMA.
 */
struct  dma_chain {
    int     dc_count;
    char    *dc_addr;
};

/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct sbic_acb {
    TAILQ_ENTRY(sbic_acb)   chain;
    struct scsi_xfer        *xs;        /* SCSI xfer ctrl block from above */
    int                     flags;      /* Status */
#define ACB_FREE        0x00
#define ACB_ACTIVE      0x01
#define ACB_DONE        0x04
#define ACB_CHKSENSE    0x08
#define ACB_BBUF        0x10    /* DMA input needs to be copied from bounce */
#define ACB_DATAIN      0x20    /* DMA direction flag */

    struct  scsi_generic    cmd;        /* SCSI command block */
    int                     clen;
    struct  dma_chain       sc_kv;      /* Virtual address of whole DMA */
    struct  dma_chain       sc_pa;      /* Physical address of DMA segment */
    u_long                  sc_tcnt;    /* number of bytes for this DMA */
    u_short                 sc_dmacmd;  /* Internal data for this DMA */
    char                    *pa_addr;   /* XXXX initial phys addr */
};

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */
struct sbic_tinfo {
    int                     cmds;       /* #commands processed */
    int                     dconns;     /* #disconnects */
    int                     senses;     /* #request sense commands sent */
    int                     lubusy;     /* What local units/subr. are busy? */
} tinfo_t;

struct  sbic_softc {
    struct  device          sc_dev;
    struct  intrhand	    sc_dmaih;
    struct  intrhand	    sc_sbicih;
    struct  target_sync {
        u_char  state;
        u_char  period;
        u_char  offset;
    } sc_sync[8];
    u_char                  target;     /* Currently active target */
    u_char                  lun;
    struct  scsi_link       sc_link;    /* proto for sub devices */
    sbic_regmap_p           sc_sbicp;   /* the SBIC */
    int                     sc_ipl;

    /* Lists of command blocks */
    TAILQ_HEAD(acb_list, sbic_acb)  free_list,
                                    ready_list,
                                    nexus_list;

    struct sbic_acb         *sc_nexus;  /* current command */
    struct sbic_acb         sc_acb[8];  /* the real command blocks */
    struct sbic_tinfo       sc_tinfo[8];

    struct  scsi_xfer       *sc_xs;     /* transfer from high level code */
    u_char                  sc_flags;
    u_char                  sc_stat[2];
    u_char                  sc_msg[7];
    u_long                  sc_clkfreq;
    u_long                  sc_tcnt;    /* number of bytes transferred */
    u_short                 sc_dmacmd;  /* used by dma drivers */
    u_long                  sc_dmamask; /* dma valid mem mask */
#ifdef DEBUG
    u_short                 sc_dmatimo; /* dma timeout */
#endif
    struct  dma_chain       *sc_cur;
    struct  dma_chain       *sc_last;
    int  (*sc_dmago)(struct sbic_softc *, char *, int, int);
    int  (*sc_dmanext)(struct sbic_softc *);
    void (*sc_enintr)(struct sbic_softc *);
    void (*sc_dmastop)(struct sbic_softc *);
};

/*
 * sc_flags
 */
#define SBICF_ALIVE         0x01    /* controller initialized */
#define SBICF_DCFLUSH       0x02    /* need flush for overlap after dma finishes */
#define SBICF_SELECTED      0x04    /* bus is in selected state. */
#define SBICF_ICMD          0x08    /* Immediate command in execution */
#define SBICF_BADDMA        0x10    /* controller can only DMA to ztwobus space */
#define SBICF_INTR          0x40    /* SBICF interrupt expected */
#define SBICF_INDMA         0x80    /* not used yet, DMA I/O in progress */

/*
 * sync states
 */
#define SYNC_START          0   /* no sync handshake started */
#define SYNC_SENT           1   /* we sent sync request, no answer yet */
#define SYNC_DONE           2   /* target accepted our (or inferior) settings,
                                   or it rejected the request and we stay async */

#define PHASE               0x07    /* mask for psns/pctl phase */
#define DATA_OUT_PHASE      0x00
#define DATA_IN_PHASE       0x01
#define CMD_PHASE           0x02
#define STATUS_PHASE        0x03
#define BUS_FREE_PHASE      0x04
#define ARB_SEL_PHASE       0x05    /* Fuji chip combines arbitration with sel. */
#define MESG_OUT_PHASE      0x06
#define MESG_IN_PHASE       0x07

#define MSG_CMD_COMPLETE    0x00
#define MSG_EXT_MESSAGE     0x01
#define MSG_SAVE_DATA_PTR   0x02
#define MSG_RESTORE_PTR     0x03
#define MSG_DISCONNECT      0x04
#define MSG_INIT_DETECT_ERROR   0x05
#define MSG_ABORT           0x06
#define MSG_REJECT          0x07
#define MSG_NOOP            0x08
#define MSG_PARITY_ERROR    0x09
#define MSG_BUS_DEVICE_RESET    0x0C
#define MSG_IDENTIFY        0x80
#define MSG_IDENTIFY_DR     0xc0    /* (disconnect/reconnect allowed) */
#define MSG_SYNC_REQ        0x01

#define MSG_ISIDENTIFY(x)   ((x) & MSG_IDENTIFY)


#define STS_CHECKCOND       0x02    /* Check Condition (ie., read sense) */
#define STS_CONDMET         0x04    /* Condition Met (ie., search worked) */
#define STS_BUSY            0x08
#define STS_INTERMED        0x10    /* Intermediate status sent */
#define STS_EXT             0x80    /* Extended status valid */


/*
 * States returned by our state machine
 */

#define SBIC_STATE_ERROR    -1
#define SBIC_STATE_DONE     0
#define SBIC_STATE_RUNNING  1
#define SBIC_STATE_DISCONNECT   2


struct buf;
struct scsi_xfer;

void sbic_scsicmd(struct scsi_xfer *);

#endif /* _SBICVAR_H_ */
