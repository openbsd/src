/* $NetBSD: sfasvar.h,v 1.1 1996/01/31 23:26:49 mark Exp $ */

/*
 * Copyright (c) 1995 Daniel Widenfalk
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
 *      This product includes software developed by Daniel Widenfalk
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _SFASVAR_H_
#define _SFASVAR_H_

#ifndef _SFASREG_H_
#include <arm32/podulebus/sfasreg.h>
#endif

/*
 * MAXCHAIN is the anticipated maximum number of chain blocks needed. This
 * assumes that we are NEVER requested to transfer more than MAXPHYS bytes.
 */
#define MAXCHAIN	(MAXPHYS/NBPG+2)

/*
 * Maximum number of requests standing by. Could be anything, but I think 9
 * looks nice :-) NOTE: This does NOT include requests already started!
 */
#define MAXPENDING	9	/* 7 IDs + 2 extra */

/*
 * DMA chain block. If flg == SFAS_CHAIN_PRG or flg == SFAS_CHAIN_BUMP then
 * ptr is a VIRTUAL adress. If flg == SFAS_CHAIN_DMA then ptr is a PHYSICAL
 * adress.
 */
struct	sfas_dma_chain {
	vm_offset_t	ptr;
	u_short		len;
	short		flg;
};
#define SFAS_CHAIN_DMA	0x00
#define SFAS_CHAIN_BUMP	0x01
#define SFAS_CHAIN_PRG	0x02


/*
 * This struct contains the necessary info for a pending request. Pointer to
 * a scsi_xfer struct.
 */
struct	sfas_pending {
	TAILQ_ENTRY(sfas_pending) link;
	struct scsi_xfer	 *xs;
};

/*
 * nexus contains all active data for one SCSI unit. Parts of the info in this
 * struct survives between scsi commands.
 */
struct nexus {
	struct	scsi_xfer 	*xs;		/* Pointer to request */

	u_char			 ID;		/* ID message to be sent */
	u_char			 clen;		/* scsi command length + */
	u_char			 cbuf[14];	/* the actual command bytes */

	struct sfas_dma_chain	 dma[MAXCHAIN];	/* DMA chain blocks */
	short			 max_link;	/* Maximum used of above */
	short			 cur_link;	/* Currently handled block */

	u_char			*buf;		/* Virtual adress of data */
	int			 len;		/* Bytes left to transfer */

	vm_offset_t		 dma_buf;	/* Current DMA adress */
	int			 dma_len;	/* Current DMA length */

	vm_offset_t		 dma_blk_ptr;	/* Current chain adress */
	int			 dma_blk_len;	/* Current chain length */
	u_char			 dma_blk_flg;	/* Current chain flags */

	u_char			 state;		/* Nexus state, see below */
	u_short			 flags;		/* Nexus flags, see below */

	short			 period;	/* Sync period to request */
	u_char			 offset;	/* Sync offset to request */

	u_char			 syncper;	/* FAS216 variable storage */
	u_char			 syncoff;	/* FAS216 variable storage */
	u_char			 config3;	/* FAS216 variable storage */

	u_char			 lun_unit;	/* (Lun<<4) | Unit of nexus */
	u_char			 status;	/* Status byte from unit*/

};

/* SCSI nexus_states */
#define SFAS_NS_IDLE		0	/* Nexus idle */
#define SFAS_NS_SELECTED	1	/* Last command was a SELECT command */
#define SFAS_NS_DATA_IN		2	/* Last command was a TRANSFER_INFO */
					/* command during a data in phase */
#define SFAS_NS_DATA_OUT	3	/* Last command was a TRANSFER_INFO */
					/* command during a data out phase */
#define SFAS_NS_STATUS		4	/* We have send a COMMAND_COMPLETE */
					/* command and are awaiting status */
#define SFAS_NS_MSG_IN		5	/* Last phase was MESSAGE IN */
#define SFAS_NS_MSG_OUT		6	/* Last phase was MESSAGE OUT */
#define SFAS_NS_SVC		7	/* We have sent the command */
#define SFAS_NS_DISCONNECTING	8	/* We have recieved a disconnect msg */
#define SFAS_NS_DISCONNECTED	9	/* We are disconnected */
#define SFAS_NS_RESELECTED	10	/* We was reselected */
#define SFAS_NS_DONE		11	/* Done. Prephsase to FINISHED */
#define SFAS_NS_FINISHED	12	/* Realy done. Call scsi_done */
#define SFAS_NS_SENSE		13	/* We are requesting sense */
#define SFAS_NS_RESET		14	/* We are reseting this unit */

/* SCSI nexus flags */
#define SFAS_NF_UNIT_BUSY	0x0001	/* Unit is not available */

#define SFAS_NF_SELECT_ME	0x0002	/* Nexus is set up, waiting for bus */

#define SFAS_NF_REQUEST_SENSE	0x0004	/* We should request sense */
#define SFAS_NF_SENSING		0x0008	/* We are sensing */

#define SFAS_NF_HAS_MSG		0x0010	/* We have recieved a complete msg */

#define SFAS_NF_DO_SDTR		0x0020	/* We should send a SDTR */
#define SFAS_NF_SDTR_SENT	0x0040	/* We have sent a SDTR */
#define SFAS_NF_SYNC_TESTED	0x0080	/* We have negotiated sync */

#define SFAS_NF_RESET		0x0100	/* Reset this nexus */
#define SFAS_NF_IMMEDIATE	0x0200	/* We are operating from sfasicmd */

#define SFAS_NF_DEBUG		0x8000	/* As it says: DEBUG */

struct	sfas_softc {
	struct	device		 sc_dev;	/* System required struct */
	struct	scsi_link	 sc_link;	/* For sub devices */
	irqhandler_t		 sc_ih;		/* Interrupt chain struct */

	TAILQ_HEAD(,sfas_pending) sc_xs_pending;
	TAILQ_HEAD(,sfas_pending) sc_xs_free;
	struct	sfas_pending 	 sc_xs_store[MAXPENDING];

	sfas_regmap_p		 sc_fas;	/* FAS216 Address */
	void			*sc_spec;	/* Board-specific data */

	u_char			*sc_bump_va;	/* Bumpbuf virtual adr */
	vm_offset_t		 sc_bump_pa;	/* Bumpbuf physical adr */
	int			 sc_bump_sz;	/* Bumpbuf size */

/* Configuration registers, must be set BEFORE sfasinitialize */
	u_char			 sc_clock_freq;
	u_short			 sc_timeout;
	u_char			 sc_host_id;
	u_char			 sc_config_flags;

/* Generic DMA functions */
	int		       (*sc_setup_dma)();
	int		       (*sc_build_dma_chain)();
	int		       (*sc_need_bump)();

/* Generic Led data */
	int			 sc_led_status;
	void		       (*sc_led)();

/* Nexus list */
	struct nexus		 sc_nexus[8];
	struct nexus		*sc_cur_nexus;
	struct nexus		*sc_sel_nexus;

/* Current transfer data */
	u_char			*sc_buf;	/* va */
	int			 sc_len;

	vm_offset_t		 sc_dma_buf;	/* pa */
	int			 sc_dma_len;
	vm_offset_t		 sc_dma_blk_ptr;
	int			 sc_dma_blk_len;
	short			 sc_dma_blk_flg;

	struct sfas_dma_chain	*sc_chain;	/* Current DMA chain */
	short			 sc_max_link;
	short			 sc_cur_link;

/* Interrupt registers */
	u_char			 sc_status;
	u_char			 sc_interrupt;
	u_char			 sc_resel[2];

	u_char			 sc_units_disconnected;

/* Storage for FAS216 config registers (current values) */
	u_char			 sc_config1;
	u_char			 sc_config2;
	u_char			 sc_config3;
	u_char			 sc_clock_conv_fact;
	u_char			 sc_timeout_val;
	u_char			 sc_clock_period;

	u_char			 sc_msg_in[7];
	u_char			 sc_msg_in_len;

	u_char			 sc_msg_out[7];
	u_char			 sc_msg_out_len;

	u_char			 sc_unit;
	u_char			 sc_lun;
	u_char			 sc_flags;
};

#define SFAS_DMA_READ	0
#define SFAS_DMA_WRITE	1
#define SFAS_DMA_CLEAR	2

/* sc_flags */
#define SFAS_ACTIVE	 0x01
#define SFAS_DONT_WAIT	 0x02

/* SCSI Selection modes */
#define SFAS_SELECT	0x00	/* Normal selection: No sync, no resel */
#define SFAS_SELECT_R	0x01	/* Reselection allowed */
#define SFAS_SELECT_S	0x02	/* Synchronous transfer allowed */
#define SFAS_SELECT_I	0x04	/* Selection for sfasicmd */
#define SFAS_SELECT_K	0x08	/* Send a BUS DEVICE RESET message (Kill) */

/* Nice abbreviations of the above */
#define SFAS_SELECT_RS	(SFAS_SELECT_R|SFAS_SELECT_S)
#define SFAS_SELECT_RI	(SFAS_SELECT_R|SFAS_SELECT_I)
#define SFAS_SELECT_SI	(SFAS_SELECT_S|SFAS_SELECT_I)
#define SFAS_SELECT_RSI	(SFAS_SELECT_R|SFAS_SELECT_S|SFAS_SELECT_I)

/* sc_config_flags */
#define SFAS_NO_SYNCH	 0x01	/* Disable synchronous transfer */
#define SFAS_NO_DMA	 0x02	/* Do not use DMA! EVER! */
#define SFAS_NO_RESELECT 0x04	/* Do not allow relesection */
#define SFAS_SLOW_CABLE	 0x08	/* Cable is "unsafe" for fast scsi-2 */
#define SFAS_SLOW_START	 0x10	/* There are slow starters on the bus */

void	sfasinitialize __P((struct sfas_softc *sc));
void	sfas_minphys   __P((struct buf *bp));
int	sfas_scsicmd   __P((struct scsi_xfer *));

#endif /* _SFASVAR_H_ */
