/*	$OpenBSD: mpt_openbsd.h,v 1.13 2004/11/03 00:59:56 marco Exp $	*/
/*	$NetBSD: mpt_netbsd.h,v 1.2 2003/04/16 23:02:14 thorpej Exp $	*/

/*
 * Copyright (c) 2004 Milos Urbanek
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2000, 2001 by Greg Ansley, Adam Prewett
 *
 * Partially derived from Matt Jacobs ISP driver.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

/* 
 * mpt_openbsd.h:
 *
 * OpenBSD-specific definitions for LSI Fusion adapters.
 *
 * Adapted from the NetBSD "mpt" driver by Milos Urbanek for
 * ZOOM International, s.r.o.
 */

#ifndef _DEV_IC_MPT_OPENBSD_H_
#define	_DEV_IC_MPT_OPENBSD_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/timeout.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/mpt_mpilib.h>

/*
 * macro to convert from milliseconds to hz without integer overflow
 * Default version using only 32bits arithmetics.
 * 64bit port can define 64bit version in their <machine/param.h>
 * 0x20000 is safe for hz < 20000
 */
#ifndef mstohz
#define mstohz(ms) \
	    (((ms +0u) / 1000u) * hz)
#endif

/* Max MPT Reply we are willing to accept (must be a power of 2). */
#define	MPT_REPLY_SIZE		128

#define	MPT_MAX_REQUESTS(mpt)	((mpt)->is_fc ? 1024 : 256)
#define	MPT_REQUEST_AREA	512
#define	MPT_SENSE_SIZE		32	/* included in MPT_REQUEST_AREA */
#define	MPT_REQ_MEM_SIZE(mpt)	(MPT_MAX_REQUESTS(mpt) * MPT_REQUEST_AREA)

/*
 * We cannot tell prior to getting IOC facts how big the IOC's request
 * area is. Because of this we cannot tell at compile time how many
 * simple SG elements we can fit within an IOC request prior to having
 * to put in a chain element.
 *
 * Experimentally we know that the Ultra4 parts have a 96 byte request
 * element size and the Fibre Channel units have a 144 byte request
 * element size. Therefore, if we have 512-32 (== 480) bytes of request
 * area to play with, we have room for between 3 and 5 request sized
 * regions- the first of which is the command  plus a simple SG list,
 * the rest of which are chained continuation SG lists. Given that the
 * normal request we use is 48 bytes w/o the first SG element, we can
 * assume we have 480-48 == 432 bytes to have simple SG elements and/or
 * chain elements. If we assume 32 bit addressing, this works out to
 * 54 SG or chain elements. If we assume 5 chain elements, then we have
 * a maximum of 49 seperate actual SG segments. 
 */
#define	MPT_SGL_MAX		49

#define	MPT_RQSL(mpt)		((mpt)->request_frame_size << 2)
#define	MPT_NSGL(mpt)		(MPT_RQSL(mpt) / sizeof(SGE_SIMPLE32))

#define	MPT_NSGL_FIRST(mpt)					\
	((((mpt)->request_frame_size << 2) -			\
	  sizeof(MSG_SCSI_IO_REQUEST) -				\
	  sizeof(SGE_IO_UNION)) / sizeof(SGE_SIMPLE32))

/*
 * Convert a physical address returned from IOC to a virtual address
 * needed to access the data.
 */
#define	MPT_REPLY_PTOV(m, x)					\
	((void *)(&(m)->reply[(((x) << 1) - (m)->reply_phys)]))

enum mpt_req_state {
	REQ_FREE,
	REQ_IN_PROGRESS,
	REQ_TIMEOUT,
	REQ_ON_CHIP,
	REQ_DONE
};
typedef struct req_entry {
	uint16_t	index;		/* index of this entry */
	struct scsi_xfer *xfer;		/* scsipi xfer request */
	void		*req_vbuf;	/* virtual address of entry */
	void		*sense_vbuf;	/* virtual address of sense data */
	bus_addr_t	req_pbuf;	/* physical address of entry */
	bus_addr_t	sense_pbuf;	/* physical address of sense data */
	bus_dmamap_t	dmap;		/* DMA map for data buffer */
	SLIST_ENTRY(req_entry) link;	/* pointer to next in list */
	enum mpt_req_state debug;	/* debugging */
	uint32_t	sequence;	/* sequence number */
} request_t;

typedef struct mpt_softc {
	struct device	mpt_dev;		/* base device glue */

	int		verbose;
	int		is_fc;
	int		bus;

	/* IOC facts */
	uint16_t	mpt_global_credits;
	uint16_t	request_frame_size;
	uint8_t		mpt_max_devices;
	uint8_t		mpt_max_buses;
	uint8_t         fw_download_boot;
	uint32_t        fw_image_size;
	uint32_t	im_support;

	/* Port facts */
	uint16_t	mpt_ini_id;

	/* Device configuration information */
	union {
		struct mpt_spi_cfg {
			fCONFIG_PAGE_SCSI_PORT_0	_port_page0;
			fCONFIG_PAGE_SCSI_PORT_1	_port_page1;
			fCONFIG_PAGE_SCSI_PORT_2	_port_page2;
			fCONFIG_PAGE_SCSI_DEVICE_0	_dev_page0[16];
			fCONFIG_PAGE_SCSI_DEVICE_1	_dev_page1[16];
			uint16_t			_negotiated_speed[16];
			uint16_t			_tag_enable;
			uint16_t			_disc_enable;
			uint16_t			_update_params0;
			uint16_t			_update_params1;
			uint16_t			_report_xfer_mode;
		} spi;
#define	mpt_port_page0		cfg.spi._port_page0
#define	mpt_port_page1		cfg.spi._port_page1
#define	mpt_port_page2		cfg.spi._port_page2
#define	mpt_dev_page0		cfg.spi._dev_page0
#define	mpt_dev_page1		cfg.spi._dev_page1
#define	mpt_negotiated_speed	cfg.spi._negotiated_speed
#define	mpt_tag_enable		cfg.spi._tag_enable
#define	mpt_disc_enable		cfg.spi._disc_enable
#define	mpt_update_params0	cfg.spi._update_params0
#define	mpt_update_params1	cfg.spi._update_params1
#define	mpt_report_xfer_mode	cfg.spi._report_xfer_mode

		struct mpt_fc_cfg {
			uint8_t		nada;
		} fc;
	} cfg;

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_dma_tag_t		sc_dmat;

	/* Reply memory */
	bus_dmamap_t		reply_dmap;
	char			*reply;
	bus_addr_t		reply_phys;

	/* Request memory */
	bus_dmamap_t		request_dmap;
	char			*request;
	bus_addr_t		request_phys;

	/* scsi linkage */
	request_t		*request_pool;
	SLIST_HEAD(req_queue, req_entry) request_free_list;

	struct scsi_link	sc_link;
	struct scsi_adapter	sc_adapter;

	uint32_t		sequence;	/* sequence number */
	uint32_t		timeouts;	/* timeout count */
	uint32_t		success;	/* success after timeout */

	uint8_t                 upload_fw;      /* If set, do a fw upload */
	/* Firmware memory */
	bus_dmamap_t            fw_dmap;
	int                     fw_rseg;
	bus_dma_segment_t       fw_seg;
	char                    *fw;

	/* Companion part in a 929 or 1030, or NULL. */
	struct mpt_softc	*mpt2;

	/* To restore configuration after hard reset. */
	void			(*sc_set_config_regs)(struct mpt_softc *);
} mpt_softc_t;

#define	MPT_SYNC_REQ(mpt, req, ops)				\
	bus_dmamap_sync((mpt)->sc_dmat, (mpt)->request_dmap,	\
	    (req)->req_pbuf - (mpt)->request_phys,		\
	    MPT_REQUEST_AREA, (ops))

#define	mpt_read(mpt, reg)					\
	bus_space_read_4((mpt)->sc_st, (mpt)->sc_sh, (reg))
#define	mpt_write(mpt, reg, val)				\
	bus_space_write_4((mpt)->sc_st, (mpt)->sc_sh, (reg), (val))

void	mpt_attach(mpt_softc_t *);
int	mpt_dma_mem_alloc(mpt_softc_t *);
int	mpt_intr(void *);
void	mpt_prt(mpt_softc_t *, const char *, ...);


#define	mpt_set_config_regs(mpt)				\
do {								\
	if ((mpt)->sc_set_config_regs != NULL)			\
		(*(mpt)->sc_set_config_regs)((mpt));		\
} while (/*CONSTCOND*/0)

#endif /* _DEV_IC_MPT_OPENBSD_H_ */
