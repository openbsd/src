/*
 * Copyright (c) 1995 Daniel Widenfalk
 * Copyright (c) 1994 Christian E. Hopps
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)scsi.c	7.5 (Berkeley) 5/4/91
 */

/*
 * AMIGA Emulex FAS216 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/sfasreg.h>
#include <amiga/dev/sfasvar.h>
#include <amiga/dev/zbusvar.h>

void sfasinitialize __P((struct sfas_softc *));
void sfas_minphys   __P((struct buf *bp));
int  sfas_scsicmd   __P((struct scsi_xfer *xs));
int  sfas_donextcmd __P((struct sfas_softc *dev, struct sfas_pending *pendp));
void sfas_scsidone  __P((struct sfas_softc *dev, struct scsi_xfer *xs,
			 int stat));
void sfasintr	    __P((struct sfas_softc *dev));
void sfasiwait	    __P((struct sfas_softc *dev));
void sfasreset	    __P((struct sfas_softc *dev, int how));
int  sfasselect	    __P((struct sfas_softc *dev, struct sfas_pending *pendp,
			 unsigned char *cbuf, int clen,
			 unsigned char *buf, int len, int mode));
void sfasicmd	    __P((struct sfas_softc *dev, struct sfas_pending *pendp));

/*
 * Initialize these to make 'em patchable. Defaults to enable sync and discon.
 */
u_char	sfas_inhibit_sync[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
u_char	sfas_inhibit_disc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

#ifdef DEBUG
#define QPRINTF(a) if (sfas_debug > 1) printf a
int	sfas_debug = 0;
#else
#define QPRINTF
#endif

/*
 * default minphys routine for sfas based controllers
 */
void
sfas_minphys(bp)
	struct buf *bp;
{

	/*
	 * No max transfer at this level.
	 */
	minphys(bp);
}

/*
 * Initialize the nexus structs.
 */
void
sfas_init_nexus(dev, nexus)
	struct sfas_softc *dev;
	struct nexus	  *nexus;
{
	bzero(nexus, sizeof(struct nexus));

	nexus->state	= SFAS_NS_IDLE;
	nexus->period	= 200;
	nexus->offset	= 0;
	nexus->syncper	= 5;
	nexus->syncoff	= 0;
	nexus->config3	= dev->sc_config3 & ~SFAS_CFG3_FASTSCSI;
}

void
sfasinitialize(dev)
	struct sfas_softc *dev;
{
	sfas_regmap_p	 rp;
	u_int		*pte, page;
	int		 i;
	u_int		inhibit_sync;
	extern u_long	scsi_nosync;
	extern int	shift_nosync;

	dev->sc_led_status = 0;

	TAILQ_INIT(&dev->sc_xs_pending);
	TAILQ_INIT(&dev->sc_xs_free);

/*
 * Initialize the sfas_pending structs and link them into the free list. We
 * have to set vm_link_data.pages to 0 or the vm FIX won't work.
 */
	for(i=0; i<MAXPENDING; i++) {
#ifdef SFAS_NEED_VM_PATCH
		dev->sc_xs_store[i].vm_link_data.pages = 0;
#endif
		TAILQ_INSERT_TAIL(&dev->sc_xs_free, &dev->sc_xs_store[i],
				  link);
	}

/*
 * Calculate the correct clock conversion factor 2 <= factor <= 8, i.e. set
 * the factor to clock_freq / 5 (int).
 */
	if (dev->sc_clock_freq <= 10)
		dev->sc_clock_conv_fact = 2;
	if (dev->sc_clock_freq <= 40)
		dev->sc_clock_conv_fact = 2+((dev->sc_clock_freq-10)/5);
	else
		panic("sfasinitialize: Clock frequence too high");

/* Setup and save the basic configuration registers */
	dev->sc_config1 = (dev->sc_host_id & SFAS_CFG1_BUS_ID_MASK);
	dev->sc_config2 = SFAS_CFG2_FEATURES_ENABLE;
	dev->sc_config3 = (dev->sc_clock_freq > 25 ? SFAS_CFG3_FASTCLK : 0);

/* Precalculate timeout value and clock period. */
	dev->sc_timeout_val  = 1+dev->sc_timeout*dev->sc_clock_freq/
				 (7.682*dev->sc_clock_conv_fact);
	dev->sc_clock_period = 1000/dev->sc_clock_freq;

	sfasreset(dev, 1 | 2);	/* Reset Chip and Bus */

	dev->sc_units_disconnected = 0;
	dev->sc_msg_in_len = 0;
	dev->sc_msg_out_len = 0;

	dev->sc_flags = 0;

	if (scsi_nosync) {
		inhibit_sync = (scsi_nosync >> shift_nosync) & 0xff;
		shift_nosync += 8;
		for (i = 0; i < 8; ++i)
			if (inhibit_sync & (1 << i))
				sfas_inhibit_sync[i] = 1;
	}

	for(i=0; i<8; i++)
		sfas_init_nexus(dev, &dev->sc_nexus[i]);

/*
 * Setup bump buffer. If dev->sc_bump_pa has the upper bits set, we should
 * allocate z2-mem else we can allocate "any" memory. This code should check
 * that the bump-buffer is LW aligned, but I think alloc_z2mem/kmem_alloc
 * does that.
 */
	if (dev->sc_bump_pa & 0xFF000000) {
		dev->sc_bump_va = (u_char *)alloc_z2mem(dev->sc_bump_sz);
		if (isztwomem(dev->sc_bump_va))
			dev->sc_bump_pa = kvtop(dev->sc_bump_va);
		else
			dev->sc_bump_pa = (vm_offset_t)
					  PREP_DMA_MEM(dev->sc_bump_va);
	} else {
		dev->sc_bump_va = (u_char *)kmem_alloc(kernel_map,
						       dev->sc_bump_sz);
		dev->sc_bump_pa = kvtop(dev->sc_bump_va);
	}

/*
 * Setup pages to noncachable, that way we don't have to flush the cache
 * every time we need "bumped" transfer.
 */
	pte = kvtopte(dev->sc_bump_va);
	page= (u_int)dev->sc_bump_pa & PG_FRAME;

	*pte = PG_V | PG_RW | PG_CI | page;
	TBIAS();

	printf(": dmabuf 0x%x", dev->sc_bump_pa);

/*
 * FIX
 * The scsi drivers tend to allocate buffers from the kernel stacks. When the
 * kernel goes to sleep, it does a contect-switch thus removing the mapping
 * to the stack. To work around this we allocate MAXPHYS+alignment bytes
 * of virtual memory to which we can later map physical memory to.
 */
#ifdef SFAS_NEED_VM_PATCH
	vm_map_lock(kernel_map);

/* Locate available space. */
	if (vm_map_findspace(kernel_map, 0, MAXPHYS+NBPG,
			     (vm_offset_t *)&dev->sc_vm_link)) {
		vm_map_unlock(kernel_map);
		panic("SFAS_SCSICMD: No VM space available.");
	} else {
		int	offset;

/*
 * Map space to virtual memory in kernel_map. This vm will always be available
 * to us during interrupt time.
 */
		offset = (vm_offset_t)dev->sc_vm_link - VM_MIN_KERNEL_ADDRESS;
		printf(" vmlnk %x", dev->sc_vm_link);
		vm_object_reference(kernel_object);
		vm_map_insert(kernel_map, kernel_object, offset,
			      (vm_offset_t)dev->sc_vm_link,
			      (vm_offset_t)dev->sc_vm_link+(MAXPHYS+NBPG));
		vm_map_unlock(kernel_map);
	}

	dev->sc_vm_link_pages = 0;
#endif
}

#ifdef SFAS_NEED_VM_PATCH
/*
 * Remove our memory-FIX mapping
 */
void
sfas_unlink_vm_link(dev)
	struct sfas_softc *dev;
{
	if (dev->sc_flags & SFAS_HAS_VM_LINK) {
		physunaccess(dev->sc_vm_link, dev->sc_vm_link_pages*NBPG);
		dev->sc_vm_link_pages = 0;
		dev->sc_flags &= ~SFAS_HAS_VM_LINK;
	}
}

/*
 * Setup a physical-to-virtual mapping to work around the above mentioned
 * bug in the scsi drivers
 */
void
sfas_link_vm_link(dev, vm_link_data)
	struct sfas_softc	*dev;
	struct vm_link_data	*vm_link_data;
{
	int	i;

	if (dev->sc_flags & SFAS_HAS_VM_LINK)
		sfas_unlink_vm_link(dev);

	dev->sc_vm_link_pages = vm_link_data->pages;

	if (vm_link_data->pages) {
		for(i=0; i<vm_link_data->pages; i++)
			physaccess(dev->sc_vm_link+i*NBPG, vm_link_data->pa[i],
				   NBPG, PG_CI);

		dev->sc_flags |= SFAS_HAS_VM_LINK;
	}
}
#endif

/*
 * used by specific sfas controller
 */
int
sfas_scsicmd(struct scsi_xfer *xs)
{
	struct sfas_softc	*dev;
	struct scsi_link	*slp;
	struct sfas_pending	*pendp;
	int			 flags, s, target;
#ifdef SFAS_NEED_VM_PATCH
	struct vm_link_data	 vm_link_data;
#endif

	slp = xs->sc_link;
	dev = slp->adapter_softc;
	flags = xs->flags;
	target = slp->target;

	if (flags & SCSI_DATA_UIO)
		panic("sfas: scsi data uio requested");

	if ((flags & SCSI_POLL) && (dev->sc_flags & SFAS_ACTIVE))
		panic("sfas_scsicmd: busy");

/* Get hold of a sfas_pending block. */
	s = splbio();
	pendp = dev->sc_xs_free.tqh_first;
	if (pendp == NULL) {
		splx(s);
		return(TRY_AGAIN_LATER);
	}
	TAILQ_REMOVE(&dev->sc_xs_free, pendp, link);
	pendp->xs = xs;
	splx(s);

#ifdef SFAS_NEED_VM_PATCH
	pendp->vm_link_data.offset = 0;
	pendp->vm_link_data.pages = 0;

/*
 * We need our FIX vm-link if:
 *  1) We are NOT using polled IO.
 *  2) Out data source/destination is not in the u-stack area.
 */
	if (!(flags & SCSI_POLL) && (
#ifdef M68040
	    ((mmutype == MMU_68040) && ((vm_offset_t)xs->data >= 0xFFFC0000)) &&
#endif
		       ((vm_offset_t)xs->data >= 0xFF000000))) {
		vm_offset_t	 sva;
		short		 n;

/* Extract and store the physical adresses of the data block */
		sva = (vm_offset_t)xs->data & PG_FRAME;

		pendp->vm_link_data.offset = (vm_offset_t)xs->data & PGOFSET;
		pendp->vm_link_data.pages  = round_page(xs->data+xs->datalen-
							sva)/NBPG;

		for(n=0; n<pendp->vm_link_data.pages; n++)
			pendp->vm_link_data.pa[n] = kvtop(sva + n*NBPG);
	}
#endif

/* If the chip if busy OR the unit is busy, we have to wait for out turn. */
	if ((dev->sc_flags & SFAS_ACTIVE) ||
	    (dev->sc_nexus[target].flags & SFAS_NF_UNIT_BUSY)) {
		s = splbio();
		TAILQ_INSERT_TAIL(&dev->sc_xs_pending, pendp, link);
		splx(s);
	} else
		sfas_donextcmd(dev, pendp);

	return((flags & SCSI_POLL) ? COMPLETE : SUCCESSFULLY_QUEUED);
}

/*
 * Actually select the unit, whereby the whole scsi-process is started.
 */
int
sfas_donextcmd(dev, pendp)
	struct sfas_softc	*dev;
	struct sfas_pending	*pendp;
{
	int	s;

/*
 * Special case for scsi unit reset. I think this is waterproof. We first
 * select the unit during splbio. We then cycle through the generated
 * interrupts until the interrupt routine signals that the unit has
 * acknowledged the reset. After that we have to wait a reset to select
 * delay before anything else can happend.
 */
	if (pendp->xs->flags & SCSI_RESET) {
		struct nexus	*nexus;

		s = splbio();
		while(!sfasselect(dev, pendp, 0, 0, 0, 0, SFAS_SELECT_K)) {
			splx(s);
			delay(10);
			s = splbio();
		}

		nexus = dev->sc_cur_nexus;
		while(nexus->flags & SFAS_NF_UNIT_BUSY) {
			sfasiwait(dev);
			sfasintr(dev);
		}

		nexus->flags |= SFAS_NF_UNIT_BUSY;
		splx(s);

		sfasreset(dev, 0);

		s = splbio();
		nexus->flags &= ~SFAS_NF_UNIT_BUSY;
		splx(s);
	}

/*
 * If we are polling, go to splbio and perform the command, else we poke
 * the scsi-bus via sfasgo to get the interrupt machine going.
 */
	if (pendp->xs->flags & SCSI_POLL) {
		s = splbio();
		sfasicmd(dev, pendp);
		TAILQ_INSERT_TAIL(&dev->sc_xs_free, pendp, link);
		splx(s);
	} else {
		sfasgo(dev, pendp);
		return;
	}
}

void
sfas_scsidone(dev, xs, stat)
	struct sfas_softc *dev;
	struct scsi_xfer *xs;
	int		 stat;
{
	struct sfas_pending	*pendp;
	int			 s;

	xs->status = stat;

	if (stat == 0)
		xs->resid = 0;
	else {
		switch(stat) {
		case SCSI_CHECK:
		/* If we get here we have valid sense data. Faults during
		 * sense is handeled elsewhere and will generate a
		 * XS_DRIVER_STUFFUP. */
			xs->error = XS_SENSE;
			break;
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
		case -1:
			xs->error = XS_DRIVER_STUFFUP;
			QPRINTF(("sfas_scsicmd() bad %x\n", stat));
			break;
		default:
			xs->error = XS_TIMEOUT;
			break;
		}
	}

	xs->flags |= ITSDONE;

/* Steal the next command from the queue so that one unit can't hog the bus. */
	s = splbio();
	pendp = dev->sc_xs_pending.tqh_first;
	while(pendp) {
		if (!(dev->sc_nexus[pendp->xs->sc_link->target].flags &
		      SFAS_NF_UNIT_BUSY))
			break;
		pendp = pendp->link.tqe_next;
	}

	if (pendp != NULL) {
		TAILQ_REMOVE(&dev->sc_xs_pending, pendp, link);
	}

	splx(s);
	scsi_done(xs);

	if (pendp)
		sfas_donextcmd(dev, pendp);
}

/*
 * There are two kinds of reset:
 *  1) CHIP-bus reset. This also implies a SCSI-bus reset.
 *  2) SCSI-bus reset.
 * After the appropriate resets have been performed we wait a reset to select
 * delay time.
 */
void
sfasreset(dev, how)
	struct sfas_softc *dev;
	int		 how;
{
	sfas_regmap_p	rp;
	int		i, s;

	rp = dev->sc_fas;

	if (how & 1) {
		for(i=0; i<8; i++)
			sfas_init_nexus(dev, &dev->sc_nexus[i]);

		*rp->sfas_command = SFAS_CMD_RESET_CHIP;
		delay(1);
		*rp->sfas_command = SFAS_CMD_NOP;

		*rp->sfas_config1 = dev->sc_config1;
		*rp->sfas_config2 = dev->sc_config2;
		*rp->sfas_config3 = dev->sc_config3;
		*rp->sfas_timeout = dev->sc_timeout_val;
		*rp->sfas_clkconv = dev->sc_clock_conv_fact &
					SFAS_CLOCK_CONVERSION_MASK;
	}

	if (how & 2) {
		for(i=0; i<8; i++)
			sfas_init_nexus(dev, &dev->sc_nexus[i]);

		s = splbio();

		*rp->sfas_command = SFAS_CMD_RESET_SCSI_BUS;
		delay(100);

/* Skip interrupt generated by RESET_SCSI_BUS */
		while(*rp->sfas_status & SFAS_STAT_INTERRUPT_PENDING) {
			dev->sc_status = *rp->sfas_status;
			dev->sc_interrupt = *rp->sfas_interrupt;

			delay(100);
		}

		dev->sc_status = *rp->sfas_status;
		dev->sc_interrupt = *rp->sfas_interrupt;

		splx(s);
	}

	if (dev->sc_config_flags & SFAS_SLOW_START)
		delay(4*250000); /* RESET to SELECT DELAY*4 for slow devices */
	else
		delay(250000);	 /* RESET to SELECT DELAY */
}

/*
 * Save active data pointers to the nexus block currently active.
 */
void
sfas_save_pointers(dev)
	struct sfas_softc *dev;
{
	struct nexus	*nx;

	nx = dev->sc_cur_nexus;
	if (nx) {
		nx->cur_link	= dev->sc_cur_link;
		nx->max_link	= dev->sc_max_link;
		nx->buf		= dev->sc_buf;
		nx->len		= dev->sc_len;
		nx->dma_len	= dev->sc_dma_len;
		nx->dma_buf	= dev->sc_dma_buf;
		nx->dma_blk_flg	= dev->sc_dma_blk_flg;
		nx->dma_blk_len	= dev->sc_dma_blk_len;
		nx->dma_blk_ptr	= dev->sc_dma_blk_ptr;
	}
}

/*
 * Restore data pointers from the currently active nexus block.
 */
void
sfas_restore_pointers(dev)
	struct sfas_softc *dev;
{
	struct nexus	*nx;

	nx = dev->sc_cur_nexus;
	if (nx) {
		dev->sc_cur_link    = nx->cur_link;
		dev->sc_max_link    = nx->max_link;
		dev->sc_buf	    = nx->buf;
		dev->sc_len	    = nx->len;
		dev->sc_dma_len	    = nx->dma_len;
		dev->sc_dma_buf	    = nx->dma_buf;
		dev->sc_dma_blk_flg = nx->dma_blk_flg;
		dev->sc_dma_blk_len = nx->dma_blk_len;
		dev->sc_dma_blk_ptr = nx->dma_blk_ptr;
		dev->sc_chain	    = nx->dma;
		dev->sc_unit	    = (nx->lun_unit & 0x0F);
		dev->sc_lun	    = (nx->lun_unit & 0xF0) >> 4;
	}
}

/*
 * sfasiwait is used during interrupt and polled IO to wait for an event from
 * the FAS chip. This function MUST NOT BE CALLED without interrupt disabled.
 */
void
sfasiwait(dev)
	struct sfas_softc *dev;
{
	sfas_regmap_p	rp;

/*
 * If SFAS_DONT_WAIT is set, we have already grabbed the interrupt info
 * elsewhere. So we don't have to wait for it.
 */
	if (dev->sc_flags & SFAS_DONT_WAIT) {
		dev->sc_flags &= ~SFAS_DONT_WAIT;
		return;
	}

	rp = dev->sc_fas;

/* Wait for FAS chip to signal an interrupt. */
	while(!(*rp->sfas_status & SFAS_STAT_INTERRUPT_PENDING))
		delay(1);

/* Grab interrupt info from chip. */
	dev->sc_status = *rp->sfas_status;
	dev->sc_interrupt = *rp->sfas_interrupt;
	if (dev->sc_interrupt & SFAS_INT_RESELECTED) {
		dev->sc_resel[0] = *rp->sfas_fifo;
		dev->sc_resel[1] = *rp->sfas_fifo;
	}
}

/*
 * Transfer info to/from device. sfas_ixfer uses polled IO+sfasiwait so the
 * rules that apply to sfasiwait also applies here.
 */
void
sfas_ixfer(dev)
	struct sfas_softc *dev;
{
	sfas_regmap_p	 rp;
	u_char		*buf;
	int		 len, mode, phase;

	rp = dev->sc_fas;
	buf = dev->sc_buf;
	len = dev->sc_len;

/*
 * Decode the scsi phase to determine whether we are reading or writing.
 * mode == 1 => READ, mode == 0 => WRITE
 */
	phase = dev->sc_status & SFAS_STAT_PHASE_MASK;
	mode = (phase == SFAS_PHASE_DATA_IN);

	while(len && ((dev->sc_status & SFAS_STAT_PHASE_MASK) == phase))
		if (mode) {
			*rp->sfas_command = SFAS_CMD_TRANSFER_INFO;

			sfasiwait(dev);

			*buf++ = *rp->sfas_fifo;
			len--;
		} else {
			len--;
			*rp->sfas_fifo = *buf++;
			*rp->sfas_command = SFAS_CMD_TRANSFER_INFO;

			sfasiwait(dev);
		}

/* Update buffer pointers to reflect the sent/recieved data. */
	dev->sc_buf = buf;
	dev->sc_len = len;

/*
 * Since the last sfasiwait will be a phase-change, we can't wait for it
 * again later, so  we have to signal that.
 */
	dev->sc_flags |= SFAS_DONT_WAIT;
}

/*
 * Build a Synchronous Data Transfer Request message
 */
void
sfas_build_sdtrm(dev, period, offset)
	struct sfas_softc *dev;
	int		  period;
	int		  offset;
{
	dev->sc_msg_out[0] = 0x01;
	dev->sc_msg_out[1] = 0x03;
	dev->sc_msg_out[2] = 0x01;
	dev->sc_msg_out[3] = period/4;
	dev->sc_msg_out[4] = offset;
	dev->sc_msg_out_len= 5;
}

/*
 * Arbitate the scsi bus and select the unit
 */
int
sfas_select_unit(dev, target)
	struct sfas_softc *dev;
	short		  target;
{
	sfas_regmap_p	 rp;
	struct nexus	*nexus;
	int		 s, retcode, i;
	u_char		 cmd;

	s = splbio();	/* Do this at splbio so that we won't be disturbed. */

	retcode = 0;

	nexus = &dev->sc_nexus[target];

/*
 * Check if the chip is busy. If not the we mark it as so and hope that nobody
 * reselects us until we have grabbed the bus.
 */
	if (!(dev->sc_flags & SFAS_ACTIVE) && !dev->sc_sel_nexus) {
		dev->sc_flags |= SFAS_ACTIVE;

		rp = dev->sc_fas;

		*rp->sfas_syncper = nexus->syncper;
		*rp->sfas_syncoff = nexus->syncoff;
		*rp->sfas_config3 = nexus->config3;

		*rp->sfas_config1 = dev->sc_config1;
		*rp->sfas_timeout = dev->sc_timeout_val;
		*rp->sfas_dest_id = target;

/* If nobody has stolen the bus, we can send a select command to the chip. */
		if (!(*rp->sfas_status & SFAS_STAT_INTERRUPT_PENDING)) {
			*rp->sfas_fifo = nexus->ID;
			if ((nexus->flags & (SFAS_NF_DO_SDTR | SFAS_NF_RESET))
			    || (dev->sc_msg_out_len != 0))
				cmd = SFAS_CMD_SEL_ATN_STOP;
			else {
				for(i=0; i<nexus->clen; i++)
					*rp->sfas_fifo = nexus->cbuf[i];

				cmd = SFAS_CMD_SEL_ATN;
			}

			dev->sc_sel_nexus = nexus;

			*rp->sfas_command = cmd;
			retcode = 1;
		}
	}

	splx(s);
	return(retcode);
}

/*
 * Grab the nexus if available else return 0.
 */
struct nexus *
sfas_arbitate_target(dev, target)
	struct sfas_softc *dev;
	int		  target;
{
	struct nexus	*nexus;
	int		 s;

/*
 * This is realy simple. Raise interrupt level to splbio. Grab the nexus and
 * leave.
 */
	nexus = &dev->sc_nexus[target];

	s = splbio();

	if (nexus->flags & SFAS_NF_UNIT_BUSY)
		nexus = 0;
	else
		nexus->flags |= SFAS_NF_UNIT_BUSY;

	splx(s);
	return(nexus);
}

/*
 * Setup a nexus for use. Initializes command, buffer pointers and dma chain.
 */
void
sfas_setup_nexus(dev, nexus, pendp, cbuf, clen, buf, len, mode)
	struct sfas_softc	*dev;
	struct nexus		*nexus;
	struct sfas_pending	*pendp;
	unsigned char		*cbuf;
	int			 clen;
	unsigned char		*buf;
	int			 len;
	int			 mode;
{
	char	sync, target, lun;

	target = pendp->xs->sc_link->target;
	lun    = pendp->xs->sc_link->lun;

/*
 * Adopt mode to reflect the config flags.
 * If we can't use DMA we can't use synch transfer. Also check the
 * sfas_inhibit_xxx[target] flags.
 */
	if ((dev->sc_config_flags & (SFAS_NO_SYNCH | SFAS_NO_DMA)) ||
	    sfas_inhibit_sync[target])
		mode &= ~SFAS_SELECT_S;

	if ((dev->sc_config_flags & SFAS_NO_RESELECT) ||
	    sfas_inhibit_disc[target])
		mode &= ~SFAS_SELECT_R;

	nexus->xs		= pendp->xs;
#ifdef SFAS_NEED_VM_PATCH
	nexus->vm_link_data	= pendp->vm_link_data;
#endif

/* Setup the nexus struct. */
	nexus->ID	   = ((mode & SFAS_SELECT_R) ? 0xC0 : 0x80) | lun;
	nexus->clen	   = clen;
	bcopy(cbuf, nexus->cbuf, nexus->clen);
	nexus->cbuf[1] |= lun << 5;		/* Fix the lun bits */
	nexus->cur_link	   = 0;
	nexus->dma_len	   = 0;
	nexus->dma_buf	   = 0;
	nexus->dma_blk_len = 0;
	nexus->dma_blk_ptr = 0;
	nexus->len	   = len;
	nexus->buf	   = buf;
	nexus->lun_unit	   = (lun << 4) | target;
	nexus->state	   = SFAS_NS_SELECTED;

/* We must keep these flags. All else must be zero. */
	nexus->flags	  &= SFAS_NF_UNIT_BUSY	 | SFAS_NF_REQUEST_SENSE
			   | SFAS_NF_SYNC_TESTED | SFAS_NF_SELECT_ME;

/*
 * If we are requesting sense, reflect that in the flags so that we can handle
 * error in sense data correctly
 */
	if (nexus->flags & SFAS_NF_REQUEST_SENSE) {
		nexus->flags &= ~SFAS_NF_REQUEST_SENSE;
		nexus->flags |=  SFAS_NF_SENSING;
	}

	if (mode & SFAS_SELECT_I)
		nexus->flags |= SFAS_NF_IMMEDIATE;
	if (mode & SFAS_SELECT_K)
		nexus->flags |= SFAS_NF_RESET;

	sync  = ((mode & SFAS_SELECT_S) ? 1 : 0);

/* We can't use sync during polled IO. */
	if (sync && (mode & SFAS_SELECT_I))
		sync = 0;

	if (!sync &&
	    ((nexus->flags & SFAS_NF_SYNC_TESTED) && (nexus->offset != 0))) {
		/*
		 * If the scsi unit is set to synch transfer and we don't want
		 * that, we have to renegotiate.
		 */

		nexus->flags |= SFAS_NF_DO_SDTR;
		nexus->period = 200;
		nexus->offset = 0;
	} else if (sync && !(nexus->flags & SFAS_NF_SYNC_TESTED)) {
		/*
		 * If the scsi unit is not set to synch transfer and we want
		 * that, we have to negotiate. This should realy base the
		 * period on the clock frequence rather than just check if
		 * >25Mhz
		 */

		nexus->flags |= SFAS_NF_DO_SDTR;
		nexus->period = ((dev->sc_clock_freq>25) ? 100 : 200);
		nexus->offset = 8;

		/* If the user has a long cable, we want to limit the period */
		if ((nexus->period == 100) &&
		    (dev->sc_config_flags & SFAS_SLOW_CABLE))
			nexus->period = 200;
	}

/*
 * Fake a dma-block for polled IO. This way we can use the same code to handle
 * reselection. Much nicer this way.
 */
	if ((mode & SFAS_SELECT_I) || (dev->sc_config_flags & SFAS_NO_DMA)) {
		nexus->dma[0].ptr = (vm_offset_t)buf;
		nexus->dma[0].len = len;
		nexus->dma[0].flg = SFAS_CHAIN_PRG;
		nexus->max_link   = 1;
	} else {
#ifdef SFAS_NEED_VM_PATCH
		if (nexus->vm_link_data.pages)
			sfas_link_vm_link(dev, &nexus->vm_link_data);
#endif
		nexus->max_link = dev->sc_build_dma_chain(dev, nexus->dma,
							  buf, len);
	}

/* Flush the caches. (If needed) */
	if ((mmutype == MMU_68040) && len && !(mode & SFAS_SELECT_I))
		dma_cachectl(buf, len);
}

int
sfasselect(dev, pendp, cbuf, clen, buf, len, mode)
	struct sfas_softc	*dev;
	struct sfas_pending	*pendp;
	unsigned char		*cbuf;
	int			 clen;
	unsigned char		*buf;
	int			 len;
	int			 mode;
{
	struct nexus	*nexus;

/* Get the nexus struct. */
	nexus = sfas_arbitate_target(dev, pendp->xs->sc_link->target);
	if (nexus == NULL)
		return(0);

/* Setup the nexus struct. */
	sfas_setup_nexus(dev, nexus, pendp, cbuf, clen, buf, len, mode);

/* Post it to the interrupt machine. */
	sfas_select_unit(dev, pendp->xs->sc_link->target);

	return(1);
}

void
sfas_request_sense(dev, nexus)
	struct sfas_softc *dev;
	struct nexus	 *nexus;
{
	struct scsi_xfer	*xs;
	struct sfas_pending	 pend;
	struct scsi_sense	 rqs;
	int			 stat, mode;

	xs = nexus->xs;

/* Fake a sfas_pending structure. */
	pend.vm_link_data.pages	= 0;
	pend.xs			= xs;

	rqs.opcode = REQUEST_SENSE;
	rqs.byte2 = xs->sc_link->lun << 5;
#ifdef not_yet
	rqs.length=xs->req_sense_length?xs->req_sense_length:sizeof(xs->sense);
#else
	rqs.length=sizeof(xs->sense);
#endif

	rqs.unused[0] = rqs.unused[1] = rqs.control = 0;

/*
 * If we are requesting sense during polled IO, we have to sense with polled
 * IO too.
 */
	mode = SFAS_SELECT_RS;
	if (nexus->flags & SFAS_NF_IMMEDIATE)
		mode = SFAS_SELECT_I;

/* Setup the nexus struct for sensing. */
	sfas_setup_nexus(dev, nexus, &pend, (char *)&rqs, sizeof(rqs),
			(char *)&xs->sense, rqs.length, mode);

/* Post it to the interrupt machine. */
	sfas_select_unit(dev, xs->sc_link->target);
}

int
sfasgo(dev, pendp)
	struct sfas_softc   *dev;
	struct sfas_pending *pendp;
{
	int	 s;
	char	*buf;

	buf    = pendp->xs->data;

/*
 * If we need the vm FIX, make buf reflect that.
 */
#ifdef SFAS_NEED_VM_PATCH
	if (pendp->vm_link_data.pages)
		buf = dev->sc_vm_link + pendp->vm_link_data.offset;
#endif

	if (sfasselect(dev, pendp, (char *)pendp->xs->cmd, pendp->xs->cmdlen,
		      buf, pendp->xs->datalen, SFAS_SELECT_RS)) {
		/*
		 * We got the command going so the sfas_pending struct is now
		 * free to reuse.
		 */

		s = splbio();
		TAILQ_INSERT_TAIL(&dev->sc_xs_free, pendp, link);
		splx(s);
	} else {
		/*
		 * We couldn't make the command fly so we have to wait. The
		 * struct MUST be inserted at the head to keep the order of
		 * the commands.
		 */

		s = splbio();
		TAILQ_INSERT_HEAD(&dev->sc_xs_pending, pendp, link);
		splx(s);
	}

	return(0);
}

/*
 * Part one of the interrupt machine. Error checks and reselection test.
 * We don't know if we have an active nexus here!
 */
int
sfas_pretests(dev, rp)
	struct sfas_softc *dev;
	sfas_regmap_p	  rp;
{
	struct nexus	*nexus;
	int		 i, s;

	if (dev->sc_interrupt & SFAS_INT_SCSI_RESET_DETECTED) {
		/*
		 * Cleanup and notify user. Lets hope that this is all we
		 * have to do
		 */

		for(i=0; i<8; i++) {
			if (dev->sc_nexus[i].xs)
				sfas_scsidone(dev, dev->sc_nexus[i].xs, -2);

			sfas_init_nexus(dev, &dev->sc_nexus[i]);
		}
		printf("sfasintr: SCSI-RESET detected!");
		return(-1);
	}

	if (dev->sc_interrupt & SFAS_INT_ILLEGAL_COMMAND) {
		/* Something went terrible wrong! Dump some data and panic! */

		printf("FIFO:");
		while(*rp->sfas_fifo_flags & SFAS_FIFO_COUNT_MASK)
			printf(" %x", *rp->sfas_fifo);
		printf("\n");

		printf("CMD: %x\n", *rp->sfas_command);
		panic("sfasintr: ILLEGAL COMMAND!");
	}

	if (dev->sc_interrupt & SFAS_INT_RESELECTED) {
		/* We were reselected. Set the chip as busy */

		s = splbio();
		dev->sc_flags |= SFAS_ACTIVE;
		if (dev->sc_sel_nexus) {
			dev->sc_sel_nexus->flags |= SFAS_NF_SELECT_ME;
			dev->sc_sel_nexus = 0;
		}
		splx(s);

		if (dev->sc_units_disconnected) {
			/* Find out who reselected us. */

			dev->sc_resel[0] &= ~(1<<dev->sc_host_id);

			for(i=0; i<8; i++)
				if (dev->sc_resel[0] & (1<<i))
					break;

			if (i == 8)
				panic("Illegal reselection!");

			if (dev->sc_nexus[i].state == SFAS_NS_DISCONNECTED) {
				/*
				 * This unit had disconnected, so we reconnect
				 * it.
				 */

				dev->sc_cur_nexus = &dev->sc_nexus[i];
				nexus = dev->sc_cur_nexus;

				*rp->sfas_syncper = nexus->syncper;
				*rp->sfas_syncoff = nexus->syncoff;
				*rp->sfas_config3 = nexus->config3;

				*rp->sfas_dest_id = i & 7;

				dev->sc_units_disconnected--;
				dev->sc_msg_in_len= 0;

#ifdef SFAS_NEED_VM_PATCH
				if (nexus->vm_link_data.pages)
				  sfas_link_vm_link(dev, &nexus->vm_link_data);
#endif

				/* Restore active pointers. */
				sfas_restore_pointers(dev);

				nexus->state = SFAS_NS_RESELECTED;

				*rp->sfas_command = SFAS_CMD_MESSAGE_ACCEPTED;

				return(1);
			}
		}

		/* Somehow we got an illegal reselection. Dump and panic. */
		printf("sfasintr: resel[0] %x resel[1] %x disconnected %d\n",
		       dev->sc_resel[0], dev->sc_resel[1],
		       dev->sc_units_disconnected);
		panic("sfasintr: Unexpected reselection!");
	}

	return(0);
}

/*
 * Part two of the interrupt machine. Handle disconnection and post command
 * processing. We know that we have an active nexus here.
 */
int
sfas_midaction(dev, rp, nexus)
	struct sfas_softc *dev;
	sfas_regmap_p	  rp;
	struct nexus	 *nexus;
{
	int	i, left, len, s;
	u_char	status, msg;

	if (dev->sc_interrupt & SFAS_INT_DISCONNECT) {
		s = splbio();
		dev->sc_cur_nexus = 0;

		/* Mark chip as busy and clean up the chip FIFO. */
		dev->sc_flags &= ~SFAS_ACTIVE;
		*rp->sfas_command = SFAS_CMD_FLUSH_FIFO;

#ifdef SFAS_NEED_VM_PATCH
		sfas_unlink_vm_link(dev);
#endif

		/* Let the nexus state reflect what we have to do. */
		switch(nexus->state) {
		case SFAS_NS_SELECTED:
			dev->sc_sel_nexus = 0;
			nexus->flags &= ~SFAS_NF_SELECT_ME;

			/*
			 * We were trying to select the unit. Probably no unit
			 * at this ID.
			 */
			nexus->xs->resid = dev->sc_len;

			nexus->status = -2;
			nexus->flags &= ~SFAS_NF_UNIT_BUSY;
			nexus->state = SFAS_NS_FINISHED;
			break;

		case SFAS_NS_SENSE:
			/*
			 * Oops! We have to request sense data from this unit.
			 * Do so.
			 */
			dev->sc_led(dev, 0);
			nexus->flags |= SFAS_NF_REQUEST_SENSE;
			sfas_request_sense(dev, nexus);
			break;

		case SFAS_NS_DONE:
			/* All done. */
			nexus->xs->resid = dev->sc_len;

			nexus->flags &= ~SFAS_NF_UNIT_BUSY;
			nexus->state  = SFAS_NS_FINISHED;
			dev->sc_led(dev, 0);
			break;

		case SFAS_NS_DISCONNECTING:
			/*
			 * We have recieved a DISCONNECT message, so we are
			 * doing a normal disconnection.
			 */
			nexus->state = SFAS_NS_DISCONNECTED;

			dev->sc_units_disconnected++;
			break;

		case SFAS_NS_RESET:
			/*
			 * We were reseting this SCSI-unit. Clean up the
			 * nexus struct.
			 */
			dev->sc_led(dev, 0);
			sfas_init_nexus(dev, nexus);
			break;

		default:
			/*
			 * Unexpected disconnection! Cleanup and exit. This
			 * shouldn't cause any problems.
			 */
			printf("sfasintr: Unexpected disconnection\n");
			printf("sfasintr: u %x s %d p %d f %x c %x\n",
			       nexus->lun_unit, nexus->state,
			       dev->sc_status & SFAS_STAT_PHASE_MASK,
			       nexus->flags, nexus->cbuf[0]);

			nexus->xs->resid = dev->sc_len;

			nexus->flags &= ~SFAS_NF_UNIT_BUSY;
			nexus->state = SFAS_NS_FINISHED;
			nexus->status = -3;

			dev->sc_led(dev, 0);
			break;
		}

		/*
		 * If we have disconnected units, we MUST enable reselection
		 * within 250ms.
		 */
		if (dev->sc_units_disconnected &&
		    !(dev->sc_flags & SFAS_ACTIVE))
			*rp->sfas_command = SFAS_CMD_ENABLE_RESEL;

		splx(s);

		/* Select the first pre-initialized nexus we find. */
		for(i=0; i<8; i++)
			if (dev->sc_nexus[i].flags & SFAS_NF_SELECT_ME)
				if (sfas_select_unit(dev, i) == 2)
					break;

		/* Does any unit need sense data? */
		for(i=0; i<8; i++)
			if (dev->sc_nexus[i].flags & SFAS_NF_REQUEST_SENSE) {
				sfas_request_sense(dev, &dev->sc_nexus[i]);
				break;
			}

		/* We are done with this nexus! */
		if (nexus->state == SFAS_NS_FINISHED)
			sfas_scsidone(dev, nexus->xs, nexus->status);

		return(1);
	}

	switch(nexus->state) {
	case SFAS_NS_SELECTED:
		dev->sc_cur_nexus = nexus;
		dev->sc_sel_nexus = 0;

		nexus->flags &= ~SFAS_NF_SELECT_ME;

		/*
		 * We have selected a unit. Setup chip, restore pointers and
		 * light the led.
		 */
		*rp->sfas_syncper = nexus->syncper;
		*rp->sfas_syncoff = nexus->syncoff;
		*rp->sfas_config3 = nexus->config3;

		sfas_restore_pointers(dev);

		if (!(nexus->flags & SFAS_NF_SENSING))
			nexus->status	= 0xFF;
		dev->sc_msg_in[0] = 0xFF;
		dev->sc_msg_in_len= 0;

		dev->sc_led(dev, 1);

		break;

	case SFAS_NS_DATA_IN:
	case SFAS_NS_DATA_OUT:
		/* We have transfered data. */
		if (dev->sc_dma_len)
			if (dev->sc_cur_link < dev->sc_max_link) {
				/*
				 * Clean up dma and at the same time get how
				 * many bytes that were NOT transfered.
				 */
			  left = dev->sc_setup_dma(dev, 0, 0, SFAS_DMA_CLEAR);
			  len  = dev->sc_dma_len;

			  if (nexus->state == SFAS_NS_DATA_IN) {
			    /*
			     * If we were bumping we may have had an odd length
			     * which means that there may be bytes left in the
			     * fifo. We also need to move the data from the
			     * bump buffer to the actual memory.
			     */
			    if (dev->sc_dma_buf == dev->sc_bump_pa)
			    {
			      while((*rp->sfas_fifo_flags&SFAS_FIFO_COUNT_MASK)
				    && left)
				dev->sc_bump_va[len-(left--)] = *rp->sfas_fifo;

			      bcopy(dev->sc_bump_va, dev->sc_buf, len-left);
			    }
			  } else {
			    /* Count any unsent bytes and flush them. */
			    left+= *rp->sfas_fifo_flags & SFAS_FIFO_COUNT_MASK;
			    *rp->sfas_command = SFAS_CMD_FLUSH_FIFO;
			  }

			  /*
			   * Update pointers/length to reflect the transfered
			   * data.
			   */
			  dev->sc_len -= len-left;
			  dev->sc_buf += len-left;

			  dev->sc_dma_buf += len-left;
			  dev->sc_dma_len  = left;

			  dev->sc_dma_blk_ptr += len-left;
			  dev->sc_dma_blk_len -= len-left;

			  /*
			   * If it was the end of a dma block, we select the
			   * next to begin with.
			   */
			  if (!dev->sc_dma_blk_len)
			    dev->sc_cur_link++;
			}
		break;

	case SFAS_NS_STATUS:
		/*
		 * If we were not sensing, grab the status byte. If we were
		 * sensing and we got a bad status, let the user know.
		 */

		status = *rp->sfas_fifo;
		msg = *rp->sfas_fifo;

		if (!(nexus->flags & SFAS_NF_SENSING))
			nexus->status = status;
		else if (status != 0)
			nexus->status = -1;

		/*
		 * Preload the command complete message. Handeled in
		 * sfas_postaction.
		 */
		dev->sc_msg_in[0] = msg;
		dev->sc_msg_in_len = 1;
		nexus->flags |= SFAS_NF_HAS_MSG;
		break;

	default:
		break;
	}

	return(0);
}

/*
 * Part three of the interrupt machine. Handle phase changes (and repeated
 * phase passes). We know that we have an active nexus here.
 */
int
sfas_postaction(dev, rp, nexus)
	struct sfas_softc *dev;
	sfas_regmap_p	  rp;
	struct nexus	 *nexus;
{
	int	i, left, len;
	u_char	cmd;
	short	offset, period;

	cmd = 0;

	switch(dev->sc_status & SFAS_STAT_PHASE_MASK) {
	case SFAS_PHASE_DATA_OUT:
	case SFAS_PHASE_DATA_IN:
		if ((dev->sc_status & SFAS_STAT_PHASE_MASK) ==
		    SFAS_PHASE_DATA_OUT)
			nexus->state = SFAS_NS_DATA_OUT;
		else
			nexus->state = SFAS_NS_DATA_IN;

		/* Make DMA ready to accept new data. Load active pointers
		 * from the DMA block. */
		dev->sc_setup_dma(dev, 0, 0, SFAS_DMA_CLEAR);
		if (dev->sc_cur_link < dev->sc_max_link) {
			if (!dev->sc_dma_blk_len) {
				dev->sc_dma_blk_ptr =
				    dev->sc_chain[dev->sc_cur_link].ptr;
				dev->sc_dma_blk_len =
				    dev->sc_chain[dev->sc_cur_link].len;
				dev->sc_dma_blk_flg =
				    dev->sc_chain[dev->sc_cur_link].flg;
			}

			/* We should use polled IO here. */
			if (dev->sc_dma_blk_flg == SFAS_CHAIN_PRG) {
				sfas_ixfer(dev);
				dev->sc_cur_link++;
				dev->sc_dma_len = 0;
				break;
			} else if (dev->sc_dma_blk_flg == SFAS_CHAIN_BUMP)
				len = dev->sc_dma_blk_len;
			else
				len = dev->sc_need_bump(dev,
				    dev->sc_dma_blk_ptr, dev->sc_dma_blk_len);

			/*
			 * If len != 0 we must bump the data, else we just
			 * DMA it straight into memory.
			 */
			if (len) {
				dev->sc_dma_buf = dev->sc_bump_pa;
				dev->sc_dma_len = len;

				if (nexus->state == SFAS_NS_DATA_OUT)
					bcopy(dev->sc_buf, dev->sc_bump_va,
					    dev->sc_dma_len);
			} else {
				dev->sc_dma_buf = dev->sc_dma_blk_ptr;
				dev->sc_dma_len = dev->sc_dma_blk_len;
			}

			/* Load DMA with adress and length of transfer. */
			dev->sc_setup_dma(dev, dev->sc_dma_buf, dev->sc_dma_len,
			    ((nexus->state == SFAS_NS_DATA_OUT)
			    ?  SFAS_DMA_WRITE : SFAS_DMA_READ));

			cmd = SFAS_CMD_TRANSFER_INFO | SFAS_CMD_DMA;
		} else {
			/*
			 * Hmmm, the unit wants more info than we have or has
			 * more than we want. Let the chip handle that.
			 */

			*rp->sfas_tc_low = 0;
			*rp->sfas_tc_mid = 1;
			*rp->sfas_tc_high = 0;
			cmd = SFAS_CMD_TRANSFER_PAD;
		}
		break;

	case SFAS_PHASE_COMMAND:
		/* The scsi unit wants the command, send it. */
		nexus->state = SFAS_NS_SVC;

		*rp->sfas_command = SFAS_CMD_FLUSH_FIFO;
		for(i=0; i<5; i++);

		for(i=0; i<nexus->clen; i++)
			*rp->sfas_fifo = nexus->cbuf[i];
		cmd = SFAS_CMD_TRANSFER_INFO;
		break;

	case SFAS_PHASE_STATUS:
		/*
		 * We've got status phase. Request status and command
		 * complete message.
		 */
		nexus->state = SFAS_NS_STATUS;
		cmd = SFAS_CMD_COMMAND_COMPLETE;
		break;

	case SFAS_PHASE_MESSAGE_OUT:
		/*
		 * Either the scsi unit wants us to send a message or we have
		 * asked for it by seting the ATN bit.
		 */
		nexus->state = SFAS_NS_MSG_OUT;

		*rp->sfas_command = SFAS_CMD_FLUSH_FIFO;

		if (nexus->flags & SFAS_NF_DO_SDTR) {
			/* Send a Synchronous Data Transfer Request. */

			sfas_build_sdtrm(dev, nexus->period, nexus->offset);
			nexus->flags |= SFAS_NF_SDTR_SENT;
			nexus->flags &= ~SFAS_NF_DO_SDTR;
		} else if (nexus->flags & SFAS_NF_RESET) {
			/* Send a reset scsi unit message. */

			dev->sc_msg_out[0] = 0x0C;
			dev->sc_msg_out_len = 1;
			nexus->state = SFAS_NS_RESET;
			nexus->flags &= ~SFAS_NF_RESET;
		} else if (dev->sc_msg_out_len == 0) {
			/* Don't know what to send so we send a NOP message. */

			dev->sc_msg_out[0] = 0x08;
			dev->sc_msg_out_len = 1;
		}

		cmd = SFAS_CMD_TRANSFER_INFO;

		for(i=0; i<dev->sc_msg_out_len; i++)
			*rp->sfas_fifo = dev->sc_msg_out[i];
		dev->sc_msg_out_len = 0;

		break;

	case SFAS_PHASE_MESSAGE_IN:
		/* Receive a message from the scsi unit. */
		nexus->state = SFAS_NS_MSG_IN;

		while(!(nexus->flags & SFAS_NF_HAS_MSG)) {
			*rp->sfas_command = SFAS_CMD_TRANSFER_INFO;
			sfasiwait(dev);

			dev->sc_msg_in[dev->sc_msg_in_len++] = *rp->sfas_fifo;

			/* Check if we got all the bytes in the message. */
			if (dev->sc_msg_in[0] >= 0x80)       ;
			else if (dev->sc_msg_in[0] >= 0x30)  ;
			else if (((dev->sc_msg_in[0] >= 0x20) &&
				  (dev->sc_msg_in_len == 2)) ||
				 ((dev->sc_msg_in[0] != 0x01) &&
				  (dev->sc_msg_in_len == 1))) {
				nexus->flags |= SFAS_NF_HAS_MSG;
				break;
			} else {
			  if (dev->sc_msg_in_len >= 2)
			    if ((dev->sc_msg_in[1]+2) == dev->sc_msg_in_len) {
				nexus->flags |= SFAS_NF_HAS_MSG;
				break;
			    }
			}

			*rp->sfas_command = SFAS_CMD_MESSAGE_ACCEPTED;
			sfasiwait(dev);

			if ((dev->sc_status & SFAS_STAT_PHASE_MASK) !=
			    SFAS_PHASE_MESSAGE_IN)
				break;
		}

		cmd = SFAS_CMD_MESSAGE_ACCEPTED;
		if (nexus->flags & SFAS_NF_HAS_MSG) {
			/* We have a message. Decode it. */

			switch(dev->sc_msg_in[0]) {
			case 0x00:	/* COMMAND COMPLETE */
				if ((nexus->status == SCSI_CHECK) &&
				    !(nexus->flags & SFAS_NF_SENSING))
					nexus->state = SFAS_NS_SENSE;
				else
					nexus->state = SFAS_NS_DONE;
				break;
			case 0x04:	/* DISCONNECT */
				nexus->state = SFAS_NS_DISCONNECTING;
				break;
			case 0x02:	/* SAVE DATA POINTER */
				sfas_save_pointers(dev);
				break;
			case 0x03:	/* RESTORE DATA POINTERS */
				sfas_restore_pointers(dev);
				break;
			case 0x07:	/* MESSAGE REJECT */
				/*
				 * If we had sent a SDTR and we got a message
				 * reject, the scsi docs say that we must go
				 * to async transfer.
				 */
				if (nexus->flags & SFAS_NF_SDTR_SENT) {
					nexus->flags &= ~SFAS_NF_SDTR_SENT;

					nexus->config3 &= ~SFAS_CFG3_FASTSCSI;
					nexus->syncper = 5;
					nexus->syncoff = 0;

					*rp->sfas_syncper = nexus->syncper;
					*rp->sfas_syncoff = nexus->syncoff;
					*rp->sfas_config3 = nexus->config3;
				} else
				/*
				 * Something was rejected but we don't know
				 * what! PANIC!
				 */
				  panic("sfasintr: Unknown message rejected!");
				break;
			case 0x08:	/* MO OPERATION */
				break;
			case 0x01:	/* EXTENDED MESSAGE */
				switch(dev->sc_msg_in[2]) {
				case 0x01:/* SYNC. DATA TRANSFER REQUEST */
					/* Decode the SDTR message. */
					period = 4*dev->sc_msg_in[3];
					offset = dev->sc_msg_in[4];

					/*
					 * Make sure that the specs are within
					 * chip limits. Note that if we
					 * initiated the negotiation the specs
					 * WILL be withing chip limits. If it
					 * was the scsi unit that initiated
					 * the negotiation, the specs may be
					 * to high.
					 */
					if (offset > 16)
						offset = 16;
					if ((period < 200) &&
					    (dev->sc_clock_freq <= 25))
						period = 200;

					if (offset == 0)
					       period = 5*dev->sc_clock_period;

					nexus->syncper = period/
							  dev->sc_clock_period;
					nexus->syncoff = offset;

					if (period < 200)
					  nexus->config3 |= SFAS_CFG3_FASTSCSI;
					else
					  nexus->config3 &=~SFAS_CFG3_FASTSCSI;

					nexus->flags |= SFAS_NF_SYNC_TESTED;

					*rp->sfas_syncper = nexus->syncper;
					*rp->sfas_syncoff = nexus->syncoff;
					*rp->sfas_config3 = nexus->config3;

					/*
					 * Hmmm, it seems that the scsi unit
					 * initiated sync negotiation, so lets
					 * reply acording to scsi-2 standard.
					 */
					if (!(nexus->flags& SFAS_NF_SDTR_SENT))
					{
					  if ((dev->sc_config_flags &
					       SFAS_NO_SYNCH) ||
					      (dev->sc_config_flags &
					       SFAS_NO_DMA) ||
					      sfas_inhibit_sync[
							nexus->lun_unit & 7]) {
					          period = 200;
					          offset = 0;
					  }

					  nexus->offset = offset;
					  nexus->period = period;
					  nexus->flags |= SFAS_NF_DO_SDTR;
					  *rp->sfas_command = SFAS_CMD_SET_ATN;
					}

					nexus->flags &= ~SFAS_NF_SDTR_SENT;
					break;

				case 0x00: /* MODIFY DATA POINTERS */
				case 0x02: /* EXTENDED IDENTIFY (SCSI-1) */
				case 0x03: /* WIDE DATA TRANSFER REQUEST */
			        default:
					/* Reject any unhandeled messages. */

					dev->sc_msg_out[0] = 0x07;
					dev->sc_msg_out_len = 1;
					*rp->sfas_command = SFAS_CMD_SET_ATN;
					cmd = SFAS_CMD_MESSAGE_ACCEPTED;
					break;
				}
				break;

			default:
				/* Reject any unhandeled messages. */

				dev->sc_msg_out[0] = 0x07;
				dev->sc_msg_out_len = 1;
				*rp->sfas_command = SFAS_CMD_SET_ATN;
				cmd = SFAS_CMD_MESSAGE_ACCEPTED;
				break;
			}
			nexus->flags &= ~SFAS_NF_HAS_MSG;
			dev->sc_msg_in_len = 0;
		}
		break;
	default:
		printf("SFASINTR: UNKNOWN PHASE! phase: %d\n",
		       dev->sc_status & SFAS_STAT_PHASE_MASK);
		dev->sc_led(dev, 0);
		sfas_scsidone(dev, nexus->xs, -4);

		return(-1);
	}

	if (cmd)
		*rp->sfas_command = cmd;

	return(0);
}

/*
 * Stub for interrupt machine.
 */
void
sfasintr(dev)
	struct sfas_softc *dev;
{
	sfas_regmap_p	 rp;
	struct nexus	*nexus;
	int		 s;

	rp = dev->sc_fas;

	if (!sfas_pretests(dev, rp)) {
		nexus = dev->sc_cur_nexus;
		if (nexus == NULL)
			nexus = dev->sc_sel_nexus;

		if (nexus)
			if (!sfas_midaction(dev, rp, nexus))
				sfas_postaction(dev, rp, nexus);
	}
}

/*
 * sfasicmd is used to perform IO when we can't use interrupts. sfasicmd
 * emulates the normal environment by waiting for the chip and calling
 * sfasintr.
 */
void
sfasicmd(dev, pendp)
	struct sfas_softc   *dev;
	struct sfas_pending *pendp;
{
	sfas_regmap_p	 rp;
	struct nexus	*nexus;

	nexus = &dev->sc_nexus[pendp->xs->sc_link->target];
	rp = dev->sc_fas;

	if (!sfasselect(dev, pendp, (char *)pendp->xs->cmd, pendp->xs->cmdlen,
			(char *)pendp->xs->data, pendp->xs->datalen,
			SFAS_SELECT_I))
		panic("sfasicmd: Couldn't select unit");

	while(nexus->state != SFAS_NS_FINISHED) {
		sfasiwait(dev);
		sfasintr(dev);
	}

	nexus->flags &= ~SFAS_NF_SYNC_TESTED;
}
