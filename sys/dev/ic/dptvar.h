/*	$OpenBSD: dptvar.h,v 1.6 2010/07/20 20:46:18 mk Exp $	*/
/*	$NetBSD: dptvar.h,v 1.5 1999/10/23 16:26:32 ad Exp $	*/

/*
 * Copyright (c) 1999 Andy Doran <ad@NetBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _IC_DPTVAR_H_
#define _IC_DPTVAR_H_ 1
#ifdef _KERNEL

#define	CCB_OFF(sc,m)	((u_long)(m) - (u_long)((sc)->sc_ccbs))

#define CCB_ALLOC	0x01	/* CCB allocated */
#define CCB_ABORT	0x02	/* abort has been issued on this CCB */
#define CCB_INTR	0x04	/* HBA interrupted for this CCB */
#define CCB_PRIVATE	0x08	/* ours; don't talk to scsipi when done */ 

struct dpt_ccb {
	struct eata_cp	ccb_eata_cp;		/* EATA command packet */
	struct eata_sg	ccb_sg[DPT_SG_SIZE];	/* SG element list */
	volatile int	ccb_flg;		/* CCB flags */
	int		ccb_timeout;		/* timeout in ms */
	u_int32_t	ccb_ccbpa;		/* physical addr of this CCB */
	bus_dmamap_t	ccb_dmamap_xfer;	/* dmamap for data xfers */
	int		ccb_hba_status;		/* from status packet */
	int		ccb_scsi_status;	/* from status packet */
	int		ccb_id;			/* unique ID of this CCB */
	SLIST_ENTRY(dpt_ccb) ccb_chain;		/* link to next CCB */
#ifdef __NetBSD__
	struct scsipi_sense_data ccb_sense;	/* SCSI sense data on error */
	struct scsipi_xfer *ccb_xs;		/* initiating SCSI command */
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	struct scsi_sense_data ccb_sense;
	struct scsi_xfer *ccb_xs;
#endif /* __OpenBSD__ */
};

struct dpt_softc {
	struct device sc_dv;		/* generic device data */
	bus_space_handle_t sc_ioh;	/* bus space handle */
#ifdef __NetBSD__
	struct scsipi_adapter sc_adapter;/* scsipi adapter */
	struct scsipi_link sc_link[3];	/* prototype link for each channel */
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	struct scsi_adapter sc_adapter;/* scsipi adapter */
	struct scsi_link sc_link[3];	/* prototype link for each channel */
#endif /* __OpenBSD__ */
	struct eata_cfg sc_ec;		/* EATA configuration data */
	bus_space_tag_t	sc_iot;		/* bus space tag */
	bus_dma_tag_t	sc_dmat;	/* bus DMA tag */
	bus_dmamap_t	sc_dmamap_ccb;	/* maps the CCBs */
	void	 	*sc_ih;		/* interrupt handler cookie */
	void		*sc_sdh;	/* shutdown hook */
	struct dpt_ccb	*sc_ccbs;	/* all our CCBs */
	struct eata_sp	*sc_statpack;	/* EATA status packet */
	int		sc_spoff;	/* status packet offset in dmamap */
	u_int32_t	sc_sppa;	/* status packet physical address */
	caddr_t		sc_scr;		/* scratch area */
	int		sc_scrlen;	/* scratch area length */
	int		sc_scroff;	/* scratch area offset in dmamap */
	u_int32_t	sc_scrpa;	/* scratch area physical address */
	int		sc_hbaid[3];	/* ID of HBA on each channel */
	int		sc_nccbs;	/* number of CCBs available */
	int		sc_open;	/* device is open */
	SLIST_HEAD(, dpt_ccb) sc_free_ccb;/* free ccb list */
};

int	dpt_intr(void *);
int	dpt_readcfg(struct dpt_softc *);
void	dpt_init(struct dpt_softc *, const char *);
void	dpt_shutdown(void *);
void	dpt_timeout(void *);
void	dpt_minphys(struct buf *, struct scsi_link *);
#ifdef __NetBSD__
int	dpt_scsi_cmd(struct scsipi_xfer *);
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
void	dpt_scsi_cmd(struct scsi_xfer *);
#endif /* __OpenBSD__ */
int	dpt_wait(struct dpt_softc *, u_int8_t, u_int8_t, int);
int	dpt_poll(struct dpt_softc *, struct dpt_ccb *);
int	dpt_cmd(struct dpt_softc *, struct eata_cp *, u_int32_t, int, int);
void	dpt_hba_inquire(struct dpt_softc *, struct eata_inquiry_data **);
void	dpt_reset_ccb(struct dpt_softc *, struct dpt_ccb *);
void	dpt_free_ccb(struct dpt_softc *, struct dpt_ccb *);
void	dpt_done_ccb(struct dpt_softc *, struct dpt_ccb *);
int	dpt_init_ccb(struct dpt_softc *, struct dpt_ccb *);
int	dpt_create_ccbs(struct dpt_softc *, struct dpt_ccb *, int);
struct dpt_ccb	*dpt_alloc_ccb(struct dpt_softc *, int);
#ifdef DEBUG
void	dpt_dump_sp(struct eata_sp *);
#endif

#endif	/* _KERNEL */
#endif	/* !defined _IC_DPTVAR_H_ */
