/*	$OpenBSD: dwc2_hcd.c,v 1.9 2015/02/10 23:38:13 uebayasi Exp $	*/
/*	$NetBSD: dwc2_hcd.c,v 1.15 2014/11/24 10:14:14 skrll Exp $	*/

/*
 * hcd.c - DesignWare HS OTG Controller host-mode routines
 *
 * Copyright (C) 2004-2013 Synopsys, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains the core HCD code, and implements the Linux hc_driver
 * API
 */

#if 0
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dwc2_hcd.c,v 1.15 2014/11/24 10:14:14 skrll Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/task.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/dwc2/linux/kernel.h>
#include <dev/usb/dwc2/linux/list.h>

#include <dev/usb/dwc2/dwc2.h>
#include <dev/usb/dwc2/dwc2var.h>

#include <dev/usb/dwc2/dwc2_core.h>
#include <dev/usb/dwc2/dwc2_hcd.h>

/**
 * dwc2_dump_channel_info() - Prints the state of a host channel
 *
 * @hsotg: Programming view of DWC_otg controller
 * @chan:  Pointer to the channel to dump
 *
 * Must be called with interrupt disabled and spinlock held
 *
 * NOTE: This function will be removed once the peripheral controller code
 * is integrated and the driver is stable
 */
#ifdef VERBOSE_DEBUG
static void dwc2_dump_channel_info(struct dwc2_hsotg *hsotg,
				   struct dwc2_host_chan *chan)
{
	int num_channels = hsotg->core_params->host_channels;
	struct dwc2_qh *qh;
	u32 hcchar;
	u32 hcsplt;
	u32 hctsiz;
	u32 hc_dma;
	int i;

	if (chan == NULL)
		return;

	hcchar = DWC2_READ_4(hsotg, HCCHAR(chan->hc_num));
	hcsplt = DWC2_READ_4(hsotg, HCSPLT(chan->hc_num));
	hctsiz = DWC2_READ_4(hsotg, HCTSIZ(chan->hc_num));
	hc_dma = DWC2_READ_4(hsotg, HCDMA(chan->hc_num));

	dev_dbg(hsotg->dev, "  Assigned to channel %p:\n", chan);
	dev_dbg(hsotg->dev, "    hcchar 0x%08x, hcsplt 0x%08x\n",
		hcchar, hcsplt);
	dev_dbg(hsotg->dev, "    hctsiz 0x%08x, hc_dma 0x%08x\n",
		hctsiz, hc_dma);
	dev_dbg(hsotg->dev, "    dev_addr: %d, ep_num: %d, ep_is_in: %d\n",
		chan->dev_addr, chan->ep_num, chan->ep_is_in);
	dev_dbg(hsotg->dev, "    ep_type: %d\n", chan->ep_type);
	dev_dbg(hsotg->dev, "    max_packet: %d\n", chan->max_packet);
	dev_dbg(hsotg->dev, "    data_pid_start: %d\n", chan->data_pid_start);
	dev_dbg(hsotg->dev, "    xfer_started: %d\n", chan->xfer_started);
	dev_dbg(hsotg->dev, "    halt_status: %d\n", chan->halt_status);
	dev_dbg(hsotg->dev, "    xfer_buf: %p\n", chan->xfer_buf);
	dev_dbg(hsotg->dev, "    xfer_dma: %08lx\n",
		(unsigned long)chan->xfer_dma);
	dev_dbg(hsotg->dev, "    xfer_len: %d\n", chan->xfer_len);
	dev_dbg(hsotg->dev, "    qh: %p\n", chan->qh);
	dev_dbg(hsotg->dev, "  NP inactive sched:\n");
	list_for_each_entry(qh, &hsotg->non_periodic_sched_inactive,
			    qh_list_entry)
		dev_dbg(hsotg->dev, "    %p\n", qh);
	dev_dbg(hsotg->dev, "  NP active sched:\n");
	list_for_each_entry(qh, &hsotg->non_periodic_sched_active,
			    qh_list_entry)
		dev_dbg(hsotg->dev, "    %p\n", qh);
	dev_dbg(hsotg->dev, "  Channels:\n");
	for (i = 0; i < num_channels; i++) {
		struct dwc2_host_chan *ch = hsotg->hc_ptr_array[i];

		dev_dbg(hsotg->dev, "    %2d: %p\n", i, ch);
	}
}
#endif /* VERBOSE_DEBUG */

/*
 * Processes all the URBs in a single list of QHs. Completes them with
 * -ETIMEDOUT and frees the QTD.
 *
 * Must be called with interrupt disabled and spinlock held
 */
static void dwc2_kill_urbs_in_qh_list(struct dwc2_hsotg *hsotg,
				      struct list_head *qh_list)
{
	struct dwc2_qh *qh, *qh_tmp;
	struct dwc2_qtd *qtd, *qtd_tmp;

	list_for_each_entry_safe(qh, qh_tmp, qh_list, qh_list_entry) {
		list_for_each_entry_safe(qtd, qtd_tmp, &qh->qtd_list,
					 qtd_list_entry) {
			dwc2_host_complete(hsotg, qtd, -ETIMEDOUT);
			dwc2_hcd_qtd_unlink_and_free(hsotg, qtd, qh);
		}
	}
}

static void dwc2_qh_list_free(struct dwc2_hsotg *hsotg,
			      struct list_head *qh_list)
{
	struct dwc2_qtd *qtd, *qtd_tmp;
	struct dwc2_qh *qh, *qh_tmp;
	unsigned long flags;

	if (!qh_list->next)
		/* The list hasn't been initialized yet */
		return;

	spin_lock_irqsave(&hsotg->lock, flags);

	/* Ensure there are no QTDs or URBs left */
	dwc2_kill_urbs_in_qh_list(hsotg, qh_list);

	list_for_each_entry_safe(qh, qh_tmp, qh_list, qh_list_entry) {
		dwc2_hcd_qh_unlink(hsotg, qh);

		/* Free each QTD in the QH's QTD list */
		list_for_each_entry_safe(qtd, qtd_tmp, &qh->qtd_list,
					 qtd_list_entry)
			dwc2_hcd_qtd_unlink_and_free(hsotg, qtd, qh);

		spin_unlock_irqrestore(&hsotg->lock, flags);
		dwc2_hcd_qh_free(hsotg, qh);
		spin_lock_irqsave(&hsotg->lock, flags);
	}

	spin_unlock_irqrestore(&hsotg->lock, flags);
}

/*
 * Responds with an error status of -ETIMEDOUT to all URBs in the non-periodic
 * and periodic schedules. The QTD associated with each URB is removed from
 * the schedule and freed. This function may be called when a disconnect is
 * detected or when the HCD is being stopped.
 *
 * Must be called with interrupt disabled and spinlock held
 */
static void dwc2_kill_all_urbs(struct dwc2_hsotg *hsotg)
{
	dwc2_kill_urbs_in_qh_list(hsotg, &hsotg->non_periodic_sched_inactive);
	dwc2_kill_urbs_in_qh_list(hsotg, &hsotg->non_periodic_sched_active);
	dwc2_kill_urbs_in_qh_list(hsotg, &hsotg->periodic_sched_inactive);
	dwc2_kill_urbs_in_qh_list(hsotg, &hsotg->periodic_sched_ready);
	dwc2_kill_urbs_in_qh_list(hsotg, &hsotg->periodic_sched_assigned);
	dwc2_kill_urbs_in_qh_list(hsotg, &hsotg->periodic_sched_queued);
}

/**
 * dwc2_hcd_start() - Starts the HCD when switching to Host mode
 *
 * @hsotg: Pointer to struct dwc2_hsotg
 */
void dwc2_hcd_start(struct dwc2_hsotg *hsotg)
{
	u32 hprt0;

	if (hsotg->op_state == OTG_STATE_B_HOST) {
		/*
		 * Reset the port. During a HNP mode switch the reset
		 * needs to occur within 1ms and have a duration of at
		 * least 50ms.
		 */
		hprt0 = dwc2_read_hprt0(hsotg);
		hprt0 |= HPRT0_RST;
		DWC2_WRITE_4(hsotg, HPRT0, hprt0);
	}

	queue_delayed_work(hsotg->wq_otg, &hsotg->start_work,
			   msecs_to_jiffies(50));
}

/* Must be called with interrupt disabled and spinlock held */
static void dwc2_hcd_cleanup_channels(struct dwc2_hsotg *hsotg)
{
	int num_channels = hsotg->core_params->host_channels;
	struct dwc2_host_chan *channel;
	u32 hcchar;
	int i;

	if (hsotg->core_params->dma_enable <= 0) {
		/* Flush out any channel requests in slave mode */
		for (i = 0; i < num_channels; i++) {
			channel = hsotg->hc_ptr_array[i];
			if (!list_empty(&channel->hc_list_entry))
				continue;
			hcchar = DWC2_READ_4(hsotg, HCCHAR(i));
			if (hcchar & HCCHAR_CHENA) {
				hcchar &= ~(HCCHAR_CHENA | HCCHAR_EPDIR);
				hcchar |= HCCHAR_CHDIS;
				DWC2_WRITE_4(hsotg, HCCHAR(i), hcchar);
			}
		}
	}

	for (i = 0; i < num_channels; i++) {
		channel = hsotg->hc_ptr_array[i];
		if (!list_empty(&channel->hc_list_entry))
			continue;
		hcchar = DWC2_READ_4(hsotg, HCCHAR(i));
		if (hcchar & HCCHAR_CHENA) {
			/* Halt the channel */
			hcchar |= HCCHAR_CHDIS;
			DWC2_WRITE_4(hsotg, HCCHAR(i), hcchar);
		}

		dwc2_hc_cleanup(hsotg, channel);
		list_add_tail(&channel->hc_list_entry, &hsotg->free_hc_list);
		/*
		 * Added for Descriptor DMA to prevent channel double cleanup in
		 * release_channel_ddma(), which is called from ep_disable when
		 * device disconnects
		 */
		channel->qh = NULL;
	}
}

/**
 * dwc2_hcd_disconnect() - Handles disconnect of the HCD
 *
 * @hsotg: Pointer to struct dwc2_hsotg
 *
 * Must be called with interrupt disabled and spinlock held
 */
void dwc2_hcd_disconnect(struct dwc2_hsotg *hsotg)
{
	u32 intr;

	/* Set status flags for the hub driver */
	hsotg->flags.b.port_connect_status_change = 1;
	hsotg->flags.b.port_connect_status = 0;

	/*
	 * Shutdown any transfers in process by clearing the Tx FIFO Empty
	 * interrupt mask and status bits and disabling subsequent host
	 * channel interrupts.
	 */
	intr = DWC2_READ_4(hsotg, GINTMSK);
	intr &= ~(GINTSTS_NPTXFEMP | GINTSTS_PTXFEMP | GINTSTS_HCHINT);
	DWC2_WRITE_4(hsotg, GINTMSK, intr);
	intr = GINTSTS_NPTXFEMP | GINTSTS_PTXFEMP | GINTSTS_HCHINT;
	DWC2_WRITE_4(hsotg, GINTSTS, intr);

	/*
	 * Turn off the vbus power only if the core has transitioned to device
	 * mode. If still in host mode, need to keep power on to detect a
	 * reconnection.
	 */
	if (dwc2_is_device_mode(hsotg)) {
		if (hsotg->op_state != OTG_STATE_A_SUSPEND) {
			dev_dbg(hsotg->dev, "Disconnect: PortPower off\n");
			DWC2_WRITE_4(hsotg, HPRT0, 0);
		}

		dwc2_disable_host_interrupts(hsotg);
	}

	/* Respond with an error status to all URBs in the schedule */
	dwc2_kill_all_urbs(hsotg);

	if (dwc2_is_host_mode(hsotg))
		/* Clean up any host channels that were in use */
		dwc2_hcd_cleanup_channels(hsotg);

	dwc2_host_disconnect(hsotg);

	dwc2_root_intr(hsotg->hsotg_sc);
}

/**
 * dwc2_hcd_rem_wakeup() - Handles Remote Wakeup
 *
 * @hsotg: Pointer to struct dwc2_hsotg
 */
static void dwc2_hcd_rem_wakeup(struct dwc2_hsotg *hsotg)
{
	if (hsotg->lx_state == DWC2_L2)
		hsotg->flags.b.port_suspend_change = 1;
	else
		hsotg->flags.b.port_l1_change = 1;

	dwc2_root_intr(hsotg->hsotg_sc);
}

/**
 * dwc2_hcd_stop() - Halts the DWC_otg host mode operations in a clean manner
 *
 * @hsotg: Pointer to struct dwc2_hsotg
 *
 * Must be called with interrupt disabled and spinlock held
 */
void dwc2_hcd_stop(struct dwc2_hsotg *hsotg)
{
	dev_dbg(hsotg->dev, "DWC OTG HCD STOP\n");

	/*
	 * The root hub should be disconnected before this function is called.
	 * The disconnect will clear the QTD lists (via ..._hcd_urb_dequeue)
	 * and the QH lists (via ..._hcd_endpoint_disable).
	 */

	/* Turn off all host-specific interrupts */
	dwc2_disable_host_interrupts(hsotg);

	/* Turn off the vbus power */
	dev_dbg(hsotg->dev, "PortPower off\n");
	DWC2_WRITE_4(hsotg, HPRT0, 0);
}

int
dwc2_hcd_urb_enqueue(struct dwc2_hsotg *hsotg, struct dwc2_hcd_urb *urb,
		     void **ep_handle, gfp_t mem_flags)
{
	struct dwc2_softc *sc = hsotg->hsotg_sc;
	struct dwc2_qtd *qtd;
	u32 intr_mask;
	int retval;
	int dev_speed;

	if (!hsotg->flags.b.port_connect_status) {
		/* No longer connected */
		dev_err(hsotg->dev, "Not connected\n");
		return -ENODEV;
	}

	dev_speed = dwc2_host_get_speed(hsotg, urb->priv);

	/* Some core configurations cannot support LS traffic on a FS root port */
	if ((dev_speed == USB_SPEED_LOW) &&
	    (hsotg->hw_params.fs_phy_type == GHWCFG2_FS_PHY_TYPE_DEDICATED) &&
	    (hsotg->hw_params.hs_phy_type == GHWCFG2_HS_PHY_TYPE_UTMI)) {
		u32 hprt0 = DWC2_READ_4(hsotg, HPRT0);
		u32 prtspd = (hprt0 & HPRT0_SPD_MASK) >> HPRT0_SPD_SHIFT;

		if (prtspd == HPRT0_SPD_FULL_SPEED) {
			return -ENODEV;
		}
	}

	qtd = pool_get(&sc->sc_qtdpool, PR_NOWAIT);
	if (!qtd)
		return -ENOMEM;

	memset(qtd, 0, sizeof(*qtd));

	dwc2_hcd_qtd_init(qtd, urb);
	retval = dwc2_hcd_qtd_add(hsotg, qtd, (struct dwc2_qh **)ep_handle,
				  mem_flags);
	if (retval) {
		dev_err(hsotg->dev,
			"DWC OTG HCD URB Enqueue failed adding QTD. Error status %d\n",
			retval);
		pool_put(&sc->sc_qtdpool, qtd);
		return retval;
	}

	intr_mask = DWC2_READ_4(hsotg, GINTMSK);
	if (!(intr_mask & GINTSTS_SOF)) {
		enum dwc2_transaction_type tr_type;

		if (qtd->qh->ep_type == USB_ENDPOINT_XFER_BULK &&
		    !(qtd->urb->flags & URB_GIVEBACK_ASAP))
			/*
			 * Do not schedule SG transactions until qtd has
			 * URB_GIVEBACK_ASAP set
			 */
			return 0;

		tr_type = dwc2_hcd_select_transactions(hsotg);
		if (tr_type != DWC2_TRANSACTION_NONE)
			dwc2_hcd_queue_transactions(hsotg, tr_type);
	}

	return 0;
}

/* Must be called with interrupt disabled and spinlock held */
int
dwc2_hcd_urb_dequeue(struct dwc2_hsotg *hsotg,
				struct dwc2_hcd_urb *urb)
{
	struct dwc2_qh *qh;
	struct dwc2_qtd *urb_qtd;

	urb_qtd = urb->qtd;
	if (!urb_qtd) {
		dev_dbg(hsotg->dev, "## Urb QTD is NULL ##\n");
		return -EINVAL;
	}

	qh = urb_qtd->qh;
	if (!qh) {
		dev_dbg(hsotg->dev, "## Urb QTD QH is NULL ##\n");
		return -EINVAL;
	}

	urb->priv = NULL;

	if (urb_qtd->in_process && qh->channel) {
#ifdef VERBOSE_DEBUG
		dwc2_dump_channel_info(hsotg, qh->channel);
#endif
		/* The QTD is in process (it has been assigned to a channel) */
		if (hsotg->flags.b.port_connect_status)
			/*
			 * If still connected (i.e. in host mode), halt the
			 * channel so it can be used for other transfers. If
			 * no longer connected, the host registers can't be
			 * written to halt the channel since the core is in
			 * device mode.
			 */
			dwc2_hc_halt(hsotg, qh->channel,
				     DWC2_HC_XFER_URB_DEQUEUE);
	}

	/*
	 * Free the QTD and clean up the associated QH. Leave the QH in the
	 * schedule if it has any remaining QTDs.
	 */
	if (hsotg->core_params->dma_desc_enable <= 0) {
		u8 in_process = urb_qtd->in_process;

		dwc2_hcd_qtd_unlink_and_free(hsotg, urb_qtd, qh);
		if (in_process) {
			dwc2_hcd_qh_deactivate(hsotg, qh, 0);
			qh->channel = NULL;
		} else if (list_empty(&qh->qtd_list)) {
			dwc2_hcd_qh_unlink(hsotg, qh);
		}
	} else {
		dwc2_hcd_qtd_unlink_and_free(hsotg, urb_qtd, qh);
	}

	return 0;
}


/*
 * Initializes dynamic portions of the DWC_otg HCD state
 *
 * Must be called with interrupt disabled and spinlock held
 */
void
dwc2_hcd_reinit(struct dwc2_hsotg *hsotg)
{
	struct dwc2_host_chan *chan, *chan_tmp;
	int num_channels;
	int i;

	hsotg->flags.d32 = 0;
	hsotg->non_periodic_qh_ptr = &hsotg->non_periodic_sched_active;

	if (hsotg->core_params->uframe_sched > 0) {
		hsotg->available_host_channels =
			hsotg->core_params->host_channels;
	} else {
		hsotg->non_periodic_channels = 0;
		hsotg->periodic_channels = 0;
	}

	/*
	 * Put all channels in the free channel list and clean up channel
	 * states
	 */
	list_for_each_entry_safe(chan, chan_tmp, &hsotg->free_hc_list,
				 hc_list_entry)
		list_del_init(&chan->hc_list_entry);

	num_channels = hsotg->core_params->host_channels;
	for (i = 0; i < num_channels; i++) {
		chan = hsotg->hc_ptr_array[i];
		list_add_tail(&chan->hc_list_entry, &hsotg->free_hc_list);
		dwc2_hc_cleanup(hsotg, chan);
	}

	/* Initialize the DWC core for host mode operation */
	dwc2_core_host_init(hsotg);
}

static void dwc2_hc_init_split(struct dwc2_hsotg *hsotg,
			       struct dwc2_host_chan *chan,
			       struct dwc2_qtd *qtd, struct dwc2_hcd_urb *urb)
{
	int hub_addr, hub_port;

	chan->do_split = 1;
	chan->xact_pos = qtd->isoc_split_pos;
	chan->complete_split = qtd->complete_split;
	dwc2_host_hub_info(hsotg, urb->priv, &hub_addr, &hub_port);
	chan->hub_addr = (u8)hub_addr;
	chan->hub_port = (u8)hub_port;
}

static void *dwc2_hc_init_xfer_data(struct dwc2_hsotg *hsotg,
			       struct dwc2_host_chan *chan,
			       struct dwc2_qtd *qtd, struct dwc2_hcd_urb *urb)
{
	if (hsotg->core_params->dma_enable > 0) {
		chan->xfer_dma = DMAADDR(urb->usbdma, urb->actual_length);

		/* For non-dword aligned case */
		if (hsotg->core_params->dma_desc_enable <= 0 &&
		    (chan->xfer_dma & 0x3))
			return (u8 *)urb->buf + urb->actual_length;
	} else {
		chan->xfer_buf = (u8 *)urb->buf + urb->actual_length;
	}

	return NULL;
}

static void *dwc2_hc_init_xfer(struct dwc2_hsotg *hsotg,
			       struct dwc2_host_chan *chan,
			       struct dwc2_qtd *qtd, struct dwc2_hcd_urb *urb)
{
	struct dwc2_hcd_iso_packet_desc *frame_desc;
	void *bufptr = NULL;

	switch (dwc2_hcd_get_pipe_type(&urb->pipe_info)) {
	case USB_ENDPOINT_XFER_CONTROL:
		chan->ep_type = USB_ENDPOINT_XFER_CONTROL;

		switch (qtd->control_phase) {
		case DWC2_CONTROL_SETUP:
			dev_vdbg(hsotg->dev, "  Control setup transaction\n");
			chan->do_ping = 0;
			chan->ep_is_in = 0;
			chan->data_pid_start = DWC2_HC_PID_SETUP;
			if (hsotg->core_params->dma_enable > 0)
				chan->xfer_dma = urb->setup_dma;
			else
				chan->xfer_buf = urb->setup_packet;
			chan->xfer_len = 8;
			break;

		case DWC2_CONTROL_DATA:
			dev_vdbg(hsotg->dev, "  Control data transaction\n");
			chan->data_pid_start = qtd->data_toggle;
			bufptr = dwc2_hc_init_xfer_data(hsotg, chan, qtd, urb);
			break;

		case DWC2_CONTROL_STATUS:
			/*
			 * Direction is opposite of data direction or IN if no
			 * data
			 */
			dev_vdbg(hsotg->dev, "  Control status transaction\n");
			if (urb->length == 0)
				chan->ep_is_in = 1;
			else
				chan->ep_is_in =
					dwc2_hcd_is_pipe_out(&urb->pipe_info);
			if (chan->ep_is_in)
				chan->do_ping = 0;
			chan->data_pid_start = DWC2_HC_PID_DATA1;
			chan->xfer_len = 0;
			if (hsotg->core_params->dma_enable > 0)
				chan->xfer_dma = hsotg->status_buf_dma;
			else
				chan->xfer_buf = hsotg->status_buf;
			break;
		}
		break;

	case USB_ENDPOINT_XFER_BULK:
		chan->ep_type = USB_ENDPOINT_XFER_BULK;
		bufptr = dwc2_hc_init_xfer_data(hsotg, chan, qtd, urb);
		break;

	case USB_ENDPOINT_XFER_INT:
		chan->ep_type = USB_ENDPOINT_XFER_INT;
		bufptr = dwc2_hc_init_xfer_data(hsotg, chan, qtd, urb);
		break;

	case USB_ENDPOINT_XFER_ISOC:
		chan->ep_type = USB_ENDPOINT_XFER_ISOC;
		if (hsotg->core_params->dma_desc_enable > 0)
			break;

		frame_desc = &urb->iso_descs[qtd->isoc_frame_index];
		frame_desc->status = 0;

		if (hsotg->core_params->dma_enable > 0) {
			chan->xfer_dma = urb->dma;
			chan->xfer_dma += frame_desc->offset +
					qtd->isoc_split_offset;
		} else {
			chan->xfer_buf = urb->buf;
			chan->xfer_buf += frame_desc->offset +
					qtd->isoc_split_offset;
		}

		chan->xfer_len = frame_desc->length - qtd->isoc_split_offset;

		/* For non-dword aligned buffers */
		if (hsotg->core_params->dma_enable > 0 &&
		    (chan->xfer_dma & 0x3))
			bufptr = (u8 *)urb->buf + frame_desc->offset +
					qtd->isoc_split_offset;

		if (chan->xact_pos == DWC2_HCSPLT_XACTPOS_ALL) {
			if (chan->xfer_len <= 188)
				chan->xact_pos = DWC2_HCSPLT_XACTPOS_ALL;
			else
				chan->xact_pos = DWC2_HCSPLT_XACTPOS_BEGIN;
		}
		break;
	}

	return bufptr;
}

static int dwc2_hc_setup_align_buf(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh,
				   struct dwc2_host_chan *chan, void *bufptr)
{
	u32 buf_size;

	if (chan->ep_type != USB_ENDPOINT_XFER_ISOC)
		buf_size = hsotg->core_params->max_transfer_size;
	else
		buf_size = 4096;

	if (!qh->dw_align_buf) {
		int err;

		qh->dw_align_buf = NULL;
		qh->dw_align_buf_dma = 0;
		err = usb_allocmem(&hsotg->hsotg_sc->sc_bus, buf_size, buf_size,
				   &qh->dw_align_buf_usbdma);
		if (!err) {
			struct usb_dma *ud = &qh->dw_align_buf_usbdma;

			qh->dw_align_buf = KERNADDR(ud, 0);
			qh->dw_align_buf_dma = DMAADDR(ud, 0);
		}
		if (!qh->dw_align_buf)
			return -ENOMEM;
	}

	if (!chan->ep_is_in && chan->xfer_len) {
		usb_syncmem(chan->xfer_usbdma, 0, buf_size,
			    BUS_DMASYNC_POSTWRITE);
 		memcpy(qh->dw_align_buf, bufptr, chan->xfer_len);
		usb_syncmem(chan->xfer_usbdma, 0, buf_size,
			    BUS_DMASYNC_PREWRITE);
	}

	chan->align_buf = qh->dw_align_buf_dma;
	return 0;
}

/**
 * dwc2_assign_and_init_hc() - Assigns transactions from a QTD to a free host
 * channel and initializes the host channel to perform the transactions. The
 * host channel is removed from the free list.
 *
 * @hsotg: The HCD state structure
 * @qh:    Transactions from the first QTD for this QH are selected and assigned
 *         to a free host channel
 */
static int dwc2_assign_and_init_hc(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
	struct dwc2_host_chan *chan;
	struct dwc2_hcd_urb *urb;
	struct dwc2_qtd *qtd;
	void *bufptr = NULL;

	if (dbg_qh(qh))
		dev_vdbg(hsotg->dev, "%s(%p,%p)\n", __func__, hsotg, qh);

	if (list_empty(&qh->qtd_list)) {
		dev_dbg(hsotg->dev, "No QTDs in QH list\n");
		return -ENOMEM;
	}

	if (list_empty(&hsotg->free_hc_list)) {
		dev_dbg(hsotg->dev, "No free channel to assign\n");
		return -ENOMEM;
	}

	chan = list_first_entry(&hsotg->free_hc_list, struct dwc2_host_chan,
				hc_list_entry);

	/* Remove host channel from free list */
	list_del_init(&chan->hc_list_entry);

	qtd = list_first_entry(&qh->qtd_list, struct dwc2_qtd, qtd_list_entry);
	urb = qtd->urb;
	qh->channel = chan;
	qtd->in_process = 1;

	/*
	 * Use usb_pipedevice to determine device address. This address is
	 * 0 before the SET_ADDRESS command and the correct address afterward.
	 */
	chan->dev_addr = dwc2_hcd_get_dev_addr(&urb->pipe_info);
	chan->ep_num = dwc2_hcd_get_ep_num(&urb->pipe_info);
	chan->speed = qh->dev_speed;
	chan->max_packet = dwc2_max_packet(qh->maxp);

	chan->xfer_started = 0;
	chan->halt_status = DWC2_HC_XFER_NO_HALT_STATUS;
	chan->error_state = (qtd->error_count > 0);
	chan->halt_on_queue = 0;
	chan->halt_pending = 0;
	chan->requests = 0;

	/*
	 * The following values may be modified in the transfer type section
	 * below. The xfer_len value may be reduced when the transfer is
	 * started to accommodate the max widths of the XferSize and PktCnt
	 * fields in the HCTSIZn register.
	 */

	chan->ep_is_in = (dwc2_hcd_is_pipe_in(&urb->pipe_info) != 0);
	if (chan->ep_is_in)
		chan->do_ping = 0;
	else
		chan->do_ping = qh->ping_state;

	chan->data_pid_start = qh->data_toggle;
	chan->multi_count = 1;

	if (urb->actual_length > urb->length &&
		!dwc2_hcd_is_pipe_in(&urb->pipe_info))
		urb->actual_length = urb->length;

	chan->xfer_len = urb->length - urb->actual_length;
	chan->xfer_count = 0;

	/* Set the split attributes if required */
	if (qh->do_split)
		dwc2_hc_init_split(hsotg, chan, qtd, urb);
	else
		chan->do_split = 0;

	/* Set the transfer attributes */
	bufptr = dwc2_hc_init_xfer(hsotg, chan, qtd, urb);

	/* Non DWORD-aligned buffer case */
	if (bufptr) {
		dev_vdbg(hsotg->dev, "Non-aligned buffer%p\n", bufptr);
		if (dwc2_hc_setup_align_buf(hsotg, qh, chan, bufptr)) {
			dev_err(hsotg->dev,
				"%s: Failed to allocate memory to handle non-dword aligned buffer\n",
				__func__);
			/* Add channel back to free list */
			chan->align_buf = 0;
			chan->multi_count = 0;
			list_add_tail(&chan->hc_list_entry,
				      &hsotg->free_hc_list);
			qtd->in_process = 0;
			qh->channel = NULL;
			return -ENOMEM;
		}
	} else {
		chan->align_buf = 0;
	}

	if (chan->ep_type == USB_ENDPOINT_XFER_INT ||
	    chan->ep_type == USB_ENDPOINT_XFER_ISOC)
		/*
		 * This value may be modified when the transfer is started
		 * to reflect the actual transfer length
		 */
		chan->multi_count = dwc2_hb_mult(qh->maxp);

	if (hsotg->core_params->dma_desc_enable > 0)
		chan->desc_list_addr = qh->desc_list_dma;

	dwc2_hc_init(hsotg, chan);
	chan->qh = qh;

	return 0;
}

/**
 * dwc2_hcd_select_transactions() - Selects transactions from the HCD transfer
 * schedule and assigns them to available host channels. Called from the HCD
 * interrupt handler functions.
 *
 * @hsotg: The HCD state structure
 *
 * Return: The types of new transactions that were assigned to host channels
 */
enum dwc2_transaction_type dwc2_hcd_select_transactions(
		struct dwc2_hsotg *hsotg)
{
	enum dwc2_transaction_type ret_val = DWC2_TRANSACTION_NONE;
	struct list_head *qh_ptr;
	struct dwc2_qh *qh;
	int num_channels;

#ifdef DWC2_DEBUG_SOF
	dev_vdbg(hsotg->dev, "  Select Transactions\n");
#endif

	/* Process entries in the periodic ready list */
	qh_ptr = hsotg->periodic_sched_ready.next;
	while (qh_ptr != &hsotg->periodic_sched_ready) {
		if (list_empty(&hsotg->free_hc_list))
			break;
		if (hsotg->core_params->uframe_sched > 0) {
			if (hsotg->available_host_channels <= 1)
				break;
			hsotg->available_host_channels--;
		}
		qh = list_entry(qh_ptr, struct dwc2_qh, qh_list_entry);
		if (dwc2_assign_and_init_hc(hsotg, qh))
			break;

		/*
		 * Move the QH from the periodic ready schedule to the
		 * periodic assigned schedule
		 */
		qh_ptr = qh_ptr->next;
		list_move(&qh->qh_list_entry, &hsotg->periodic_sched_assigned);
		ret_val = DWC2_TRANSACTION_PERIODIC;
	}

	/*
	 * Process entries in the inactive portion of the non-periodic
	 * schedule. Some free host channels may not be used if they are
	 * reserved for periodic transfers.
	 */
	num_channels = hsotg->core_params->host_channels;
	qh_ptr = hsotg->non_periodic_sched_inactive.next;
	while (qh_ptr != &hsotg->non_periodic_sched_inactive) {
		if (hsotg->core_params->uframe_sched <= 0 &&
		    hsotg->non_periodic_channels >= num_channels -
						hsotg->periodic_channels)
			break;
		if (list_empty(&hsotg->free_hc_list))
			break;
		qh = list_entry(qh_ptr, struct dwc2_qh, qh_list_entry);

		/*
		 * Check to see if this is a NAK'd retransmit, in which case
		 * ignore for retransmission. We hold off on bulk/control
		 * retransmissions to reduce NAK interrupt overhead for
		 * cheeky devices that just hold off using NAKs.
		 */
		if (qh->nak_frame != 0xffff &&
		    dwc2_full_frame_num(qh->nak_frame) ==
		    dwc2_full_frame_num(dwc2_hcd_get_frame_number(hsotg))) {
			qh_ptr = qh_ptr->next;
			continue;
		} else {
			qh->nak_frame = 0xffff;
		}

		if (hsotg->core_params->uframe_sched > 0) {
			if (hsotg->available_host_channels < 1)
				break;
			hsotg->available_host_channels--;
		}

		if (dwc2_assign_and_init_hc(hsotg, qh))
			break;

		/*
		 * Move the QH from the non-periodic inactive schedule to the
		 * non-periodic active schedule
		 */
		qh_ptr = qh_ptr->next;
		list_move(&qh->qh_list_entry,
			  &hsotg->non_periodic_sched_active);

		if (ret_val == DWC2_TRANSACTION_NONE)
			ret_val = DWC2_TRANSACTION_NON_PERIODIC;
		else
			ret_val = DWC2_TRANSACTION_ALL;

		if (hsotg->core_params->uframe_sched <= 0)
			hsotg->non_periodic_channels++;
	}

	return ret_val;
}

/**
 * dwc2_queue_transaction() - Attempts to queue a single transaction request for
 * a host channel associated with either a periodic or non-periodic transfer
 *
 * @hsotg: The HCD state structure
 * @chan:  Host channel descriptor associated with either a periodic or
 *         non-periodic transfer
 * @fifo_dwords_avail: Number of DWORDs available in the periodic Tx FIFO
 *                     for periodic transfers or the non-periodic Tx FIFO
 *                     for non-periodic transfers
 *
 * Return: 1 if a request is queued and more requests may be needed to
 * complete the transfer, 0 if no more requests are required for this
 * transfer, -1 if there is insufficient space in the Tx FIFO
 *
 * This function assumes that there is space available in the appropriate
 * request queue. For an OUT transfer or SETUP transaction in Slave mode,
 * it checks whether space is available in the appropriate Tx FIFO.
 *
 * Must be called with interrupt disabled and spinlock held
 */
static int dwc2_queue_transaction(struct dwc2_hsotg *hsotg,
				  struct dwc2_host_chan *chan,
				  u16 fifo_dwords_avail)
{
	int retval = 0;

	if (hsotg->core_params->dma_enable > 0) {
		if (hsotg->core_params->dma_desc_enable > 0) {
			if (!chan->xfer_started ||
			    chan->ep_type == USB_ENDPOINT_XFER_ISOC) {
				dwc2_hcd_start_xfer_ddma(hsotg, chan->qh);
				chan->qh->ping_state = 0;
			}
		} else if (!chan->xfer_started) {
			dwc2_hc_start_transfer(hsotg, chan);
			chan->qh->ping_state = 0;
		}
	} else if (chan->halt_pending) {
		/* Don't queue a request if the channel has been halted */
	} else if (chan->halt_on_queue) {
		dwc2_hc_halt(hsotg, chan, chan->halt_status);
	} else if (chan->do_ping) {
		if (!chan->xfer_started)
			dwc2_hc_start_transfer(hsotg, chan);
	} else if (!chan->ep_is_in ||
		   chan->data_pid_start == DWC2_HC_PID_SETUP) {
		if ((fifo_dwords_avail * 4) >= chan->max_packet) {
			if (!chan->xfer_started) {
				dwc2_hc_start_transfer(hsotg, chan);
				retval = 1;
			} else {
				retval = dwc2_hc_continue_transfer(hsotg, chan);
			}
		} else {
			retval = -1;
		}
	} else {
		if (!chan->xfer_started) {
			dwc2_hc_start_transfer(hsotg, chan);
			retval = 1;
		} else {
			retval = dwc2_hc_continue_transfer(hsotg, chan);
		}
	}

	return retval;
}

/*
 * Processes periodic channels for the next frame and queues transactions for
 * these channels to the DWC_otg controller. After queueing transactions, the
 * Periodic Tx FIFO Empty interrupt is enabled if there are more transactions
 * to queue as Periodic Tx FIFO or request queue space becomes available.
 * Otherwise, the Periodic Tx FIFO Empty interrupt is disabled.
 *
 * Must be called with interrupt disabled and spinlock held
 */
static void dwc2_process_periodic_channels(struct dwc2_hsotg *hsotg)
{
	struct list_head *qh_ptr;
	struct dwc2_qh *qh;
	u32 tx_status;
	u32 fspcavail;
	u32 gintmsk;
	int status;
	int no_queue_space = 0;
	int no_fifo_space = 0;
	u32 qspcavail;

	if (dbg_perio())
		dev_vdbg(hsotg->dev, "Queue periodic transactions\n");

	tx_status = DWC2_READ_4(hsotg, HPTXSTS);
	qspcavail = (tx_status & TXSTS_QSPCAVAIL_MASK) >>
		    TXSTS_QSPCAVAIL_SHIFT;
	fspcavail = (tx_status & TXSTS_FSPCAVAIL_MASK) >>
		    TXSTS_FSPCAVAIL_SHIFT;

	if (dbg_perio()) {
		dev_vdbg(hsotg->dev, "  P Tx Req Queue Space Avail (before queue): %d\n",
			 qspcavail);
		dev_vdbg(hsotg->dev, "  P Tx FIFO Space Avail (before queue): %d\n",
			 fspcavail);
	}

	qh_ptr = hsotg->periodic_sched_assigned.next;
	while (qh_ptr != &hsotg->periodic_sched_assigned) {
		tx_status = DWC2_READ_4(hsotg, HPTXSTS);
		qspcavail = (tx_status & TXSTS_QSPCAVAIL_MASK) >>
			    TXSTS_QSPCAVAIL_SHIFT;
		if (qspcavail == 0) {
			no_queue_space = 1;
			break;
		}

		qh = list_entry(qh_ptr, struct dwc2_qh, qh_list_entry);
		if (!qh->channel) {
			qh_ptr = qh_ptr->next;
			continue;
		}

		/* Make sure EP's TT buffer is clean before queueing qtds */
		if (qh->tt_buffer_dirty) {
			qh_ptr = qh_ptr->next;
			continue;
		}

		/*
		 * Set a flag if we're queuing high-bandwidth in slave mode.
		 * The flag prevents any halts to get into the request queue in
		 * the middle of multiple high-bandwidth packets getting queued.
		 */
		if (hsotg->core_params->dma_enable <= 0 &&
				qh->channel->multi_count > 1)
			hsotg->queuing_high_bandwidth = 1;

		fspcavail = (tx_status & TXSTS_FSPCAVAIL_MASK) >>
			    TXSTS_FSPCAVAIL_SHIFT;
		status = dwc2_queue_transaction(hsotg, qh->channel, fspcavail);
		if (status < 0) {
			no_fifo_space = 1;
			break;
		}

		/*
		 * In Slave mode, stay on the current transfer until there is
		 * nothing more to do or the high-bandwidth request count is
		 * reached. In DMA mode, only need to queue one request. The
		 * controller automatically handles multiple packets for
		 * high-bandwidth transfers.
		 */
		if (hsotg->core_params->dma_enable > 0 || status == 0 ||
		    qh->channel->requests == qh->channel->multi_count) {
			qh_ptr = qh_ptr->next;
			/*
			 * Move the QH from the periodic assigned schedule to
			 * the periodic queued schedule
			 */
			list_move(&qh->qh_list_entry,
				  &hsotg->periodic_sched_queued);

			/* done queuing high bandwidth */
			hsotg->queuing_high_bandwidth = 0;
		}
	}

	if (hsotg->core_params->dma_enable <= 0) {
		tx_status = DWC2_READ_4(hsotg, HPTXSTS);
		qspcavail = (tx_status & TXSTS_QSPCAVAIL_MASK) >>
			    TXSTS_QSPCAVAIL_SHIFT;
		fspcavail = (tx_status & TXSTS_FSPCAVAIL_MASK) >>
			    TXSTS_FSPCAVAIL_SHIFT;
		if (dbg_perio()) {
			dev_vdbg(hsotg->dev,
				 "  P Tx Req Queue Space Avail (after queue): %d\n",
				 qspcavail);
			dev_vdbg(hsotg->dev,
				 "  P Tx FIFO Space Avail (after queue): %d\n",
				 fspcavail);
		}

		if (!list_empty(&hsotg->periodic_sched_assigned) ||
		    no_queue_space || no_fifo_space) {
			/*
			 * May need to queue more transactions as the request
			 * queue or Tx FIFO empties. Enable the periodic Tx
			 * FIFO empty interrupt. (Always use the half-empty
			 * level to ensure that new requests are loaded as
			 * soon as possible.)
			 */
			gintmsk = DWC2_READ_4(hsotg, GINTMSK);
			gintmsk |= GINTSTS_PTXFEMP;
			DWC2_WRITE_4(hsotg, GINTMSK, gintmsk);
		} else {
			/*
			 * Disable the Tx FIFO empty interrupt since there are
			 * no more transactions that need to be queued right
			 * now. This function is called from interrupt
			 * handlers to queue more transactions as transfer
			 * states change.
			 */
			gintmsk = DWC2_READ_4(hsotg, GINTMSK);
			gintmsk &= ~GINTSTS_PTXFEMP;
			DWC2_WRITE_4(hsotg, GINTMSK, gintmsk);
		}
	}
}

/*
 * Processes active non-periodic channels and queues transactions for these
 * channels to the DWC_otg controller. After queueing transactions, the NP Tx
 * FIFO Empty interrupt is enabled if there are more transactions to queue as
 * NP Tx FIFO or request queue space becomes available. Otherwise, the NP Tx
 * FIFO Empty interrupt is disabled.
 *
 * Must be called with interrupt disabled and spinlock held
 */
static void dwc2_process_non_periodic_channels(struct dwc2_hsotg *hsotg)
{
	struct list_head *orig_qh_ptr;
	struct dwc2_qh *qh;
	u32 tx_status;
	u32 qspcavail;
	u32 fspcavail;
	u32 gintmsk;
	int status;
	int no_queue_space = 0;
	int no_fifo_space = 0;
	int more_to_do = 0;

	dev_vdbg(hsotg->dev, "Queue non-periodic transactions\n");

	tx_status = DWC2_READ_4(hsotg, GNPTXSTS);
	qspcavail = (tx_status & TXSTS_QSPCAVAIL_MASK) >>
		    TXSTS_QSPCAVAIL_SHIFT;
	fspcavail = (tx_status & TXSTS_FSPCAVAIL_MASK) >>
		    TXSTS_FSPCAVAIL_SHIFT;
	dev_vdbg(hsotg->dev, "  NP Tx Req Queue Space Avail (before queue): %d\n",
		 qspcavail);
	dev_vdbg(hsotg->dev, "  NP Tx FIFO Space Avail (before queue): %d\n",
		 fspcavail);

	/*
	 * Keep track of the starting point. Skip over the start-of-list
	 * entry.
	 */
	if (hsotg->non_periodic_qh_ptr == &hsotg->non_periodic_sched_active)
		hsotg->non_periodic_qh_ptr = hsotg->non_periodic_qh_ptr->next;
	orig_qh_ptr = hsotg->non_periodic_qh_ptr;

	/*
	 * Process once through the active list or until no more space is
	 * available in the request queue or the Tx FIFO
	 */
	do {
		tx_status = DWC2_READ_4(hsotg, GNPTXSTS);
		qspcavail = (tx_status & TXSTS_QSPCAVAIL_MASK) >>
			    TXSTS_QSPCAVAIL_SHIFT;
		if (hsotg->core_params->dma_enable <= 0 && qspcavail == 0) {
			no_queue_space = 1;
			break;
		}

		qh = list_entry(hsotg->non_periodic_qh_ptr, struct dwc2_qh,
				qh_list_entry);
		if (!qh->channel)
			goto next;

		/* Make sure EP's TT buffer is clean before queueing qtds */
		if (qh->tt_buffer_dirty)
			goto next;

		fspcavail = (tx_status & TXSTS_FSPCAVAIL_MASK) >>
			    TXSTS_FSPCAVAIL_SHIFT;
		status = dwc2_queue_transaction(hsotg, qh->channel, fspcavail);

		if (status > 0) {
			more_to_do = 1;
		} else if (status < 0) {
			no_fifo_space = 1;
			break;
		}
next:
		/* Advance to next QH, skipping start-of-list entry */
		hsotg->non_periodic_qh_ptr = hsotg->non_periodic_qh_ptr->next;
		if (hsotg->non_periodic_qh_ptr ==
				&hsotg->non_periodic_sched_active)
			hsotg->non_periodic_qh_ptr =
					hsotg->non_periodic_qh_ptr->next;
	} while (hsotg->non_periodic_qh_ptr != orig_qh_ptr);

	if (hsotg->core_params->dma_enable <= 0) {
		tx_status = DWC2_READ_4(hsotg, GNPTXSTS);
		qspcavail = (tx_status & TXSTS_QSPCAVAIL_MASK) >>
			    TXSTS_QSPCAVAIL_SHIFT;
		fspcavail = (tx_status & TXSTS_FSPCAVAIL_MASK) >>
			    TXSTS_FSPCAVAIL_SHIFT;
		dev_vdbg(hsotg->dev,
			 "  NP Tx Req Queue Space Avail (after queue): %d\n",
			 qspcavail);
		dev_vdbg(hsotg->dev,
			 "  NP Tx FIFO Space Avail (after queue): %d\n",
			 fspcavail);

		if (more_to_do || no_queue_space || no_fifo_space) {
			/*
			 * May need to queue more transactions as the request
			 * queue or Tx FIFO empties. Enable the non-periodic
			 * Tx FIFO empty interrupt. (Always use the half-empty
			 * level to ensure that new requests are loaded as
			 * soon as possible.)
			 */
			gintmsk = DWC2_READ_4(hsotg, GINTMSK);
			gintmsk |= GINTSTS_NPTXFEMP;
			DWC2_WRITE_4(hsotg, GINTMSK, gintmsk);
		} else {
			/*
			 * Disable the Tx FIFO empty interrupt since there are
			 * no more transactions that need to be queued right
			 * now. This function is called from interrupt
			 * handlers to queue more transactions as transfer
			 * states change.
			 */
			gintmsk = DWC2_READ_4(hsotg, GINTMSK);
			gintmsk &= ~GINTSTS_NPTXFEMP;
			DWC2_WRITE_4(hsotg, GINTMSK, gintmsk);
		}
	}
}

/**
 * dwc2_hcd_queue_transactions() - Processes the currently active host channels
 * and queues transactions for these channels to the DWC_otg controller. Called
 * from the HCD interrupt handler functions.
 *
 * @hsotg:   The HCD state structure
 * @tr_type: The type(s) of transactions to queue (non-periodic, periodic,
 *           or both)
 *
 * Must be called with interrupt disabled and spinlock held
 */
void dwc2_hcd_queue_transactions(struct dwc2_hsotg *hsotg,
				 enum dwc2_transaction_type tr_type)
{
#ifdef DWC2_DEBUG_SOF
	dev_vdbg(hsotg->dev, "Queue Transactions\n");
#endif
	/* Process host channels associated with periodic transfers */
	if ((tr_type == DWC2_TRANSACTION_PERIODIC ||
	     tr_type == DWC2_TRANSACTION_ALL) &&
	    !list_empty(&hsotg->periodic_sched_assigned))
		dwc2_process_periodic_channels(hsotg);

	/* Process host channels associated with non-periodic transfers */
	if (tr_type == DWC2_TRANSACTION_NON_PERIODIC ||
	    tr_type == DWC2_TRANSACTION_ALL) {
		if (!list_empty(&hsotg->non_periodic_sched_active)) {
			dwc2_process_non_periodic_channels(hsotg);
		} else {
			/*
			 * Ensure NP Tx FIFO empty interrupt is disabled when
			 * there are no non-periodic transfers to process
			 */
			u32 gintmsk = DWC2_READ_4(hsotg, GINTMSK);

			gintmsk &= ~GINTSTS_NPTXFEMP;
			DWC2_WRITE_4(hsotg, GINTMSK, gintmsk);
		}
	}
}

void
dwc2_conn_id_status_change(struct task *work)
{
	struct dwc2_hsotg *hsotg = container_of(work, struct dwc2_hsotg,
						wf_otg);
	u32 count = 0;
	u32 gotgctl;

	dev_dbg(hsotg->dev, "%s()\n", __func__);

	gotgctl = DWC2_READ_4(hsotg, GOTGCTL);
	dev_dbg(hsotg->dev, "gotgctl=%0x\n", gotgctl);
	dev_dbg(hsotg->dev, "gotgctl.b.conidsts=%d\n",
		!!(gotgctl & GOTGCTL_CONID_B));

	/* B-Device connector (Device Mode) */
	if (gotgctl & GOTGCTL_CONID_B) {
		/* Wait for switch to device mode */
		dev_dbg(hsotg->dev, "connId B\n");
		while (!dwc2_is_device_mode(hsotg)) {
			dev_info(hsotg->dev,
				 "Waiting for Peripheral Mode, Mode=%s\n",
				 dwc2_is_host_mode(hsotg) ? "Host" :
				 "Peripheral");
			usleep_range(20000, 40000);
			if (++count > 250)
				break;
		}
		if (count > 250)
			dev_err(hsotg->dev,
				"Connection id status change timed out\n");
		hsotg->op_state = OTG_STATE_B_PERIPHERAL;
		dwc2_core_init(hsotg, false);
		dwc2_enable_global_interrupts(hsotg);
	} else {
		/* A-Device connector (Host Mode) */
		dev_dbg(hsotg->dev, "connId A\n");
		while (!dwc2_is_host_mode(hsotg)) {
			dev_info(hsotg->dev, "Waiting for Host Mode, Mode=%s\n",
				 dwc2_is_host_mode(hsotg) ?
				 "Host" : "Peripheral");
			usleep_range(20000, 40000);
			if (++count > 250)
				break;
		}
		if (count > 250)
			dev_err(hsotg->dev,
				"Connection id status change timed out\n");
		hsotg->op_state = OTG_STATE_A_HOST;

		/* Initialize the Core for Host mode */
		dwc2_core_init(hsotg, false);
		dwc2_enable_global_interrupts(hsotg);
		dwc2_hcd_start(hsotg);
	}
}

void dwc2_wakeup_detected(void * data)
{
	struct dwc2_hsotg *hsotg = (struct dwc2_hsotg *)data;
	u32 hprt0;

	dev_dbg(hsotg->dev, "%s()\n", __func__);

	/*
	 * Clear the Resume after 70ms. (Need 20 ms minimum. Use 70 ms
	 * so that OPT tests pass with all PHYs.)
	 */
	hprt0 = dwc2_read_hprt0(hsotg);
	dev_dbg(hsotg->dev, "Resume: HPRT0=%0x\n", hprt0);
	hprt0 &= ~HPRT0_RES;
	DWC2_WRITE_4(hsotg, HPRT0, hprt0);
	dev_dbg(hsotg->dev, "Clear Resume: HPRT0=%0x\n",
		DWC2_READ_4(hsotg, HPRT0));

	dwc2_hcd_rem_wakeup(hsotg);

	/* Change to L0 state */
	hsotg->lx_state = DWC2_L0;
}

/* Must NOT be called with interrupt disabled or spinlock held */
static void dwc2_port_suspend(struct dwc2_hsotg *hsotg, u16 windex)
{
	unsigned long flags;
	u32 hprt0;
	u32 pcgctl;
	u32 gotgctl;

	dev_dbg(hsotg->dev, "%s()\n", __func__);

	spin_lock_irqsave(&hsotg->lock, flags);

	if (windex == hsotg->otg_port && dwc2_host_is_b_hnp_enabled(hsotg)) {
		gotgctl = DWC2_READ_4(hsotg, GOTGCTL);
		gotgctl |= GOTGCTL_HSTSETHNPEN;
		DWC2_WRITE_4(hsotg, GOTGCTL, gotgctl);
		hsotg->op_state = OTG_STATE_A_SUSPEND;
	}

	hprt0 = dwc2_read_hprt0(hsotg);
	hprt0 |= HPRT0_SUSP;
	DWC2_WRITE_4(hsotg, HPRT0, hprt0);

	/* Update lx_state */
	hsotg->lx_state = DWC2_L2;

	/* Suspend the Phy Clock */
	pcgctl = DWC2_READ_4(hsotg, PCGCTL);
	pcgctl |= PCGCTL_STOPPCLK;
	DWC2_WRITE_4(hsotg, PCGCTL, pcgctl);
	udelay(10);

	/* For HNP the bus must be suspended for at least 200ms */
	if (dwc2_host_is_b_hnp_enabled(hsotg)) {
		pcgctl = DWC2_READ_4(hsotg, PCGCTL);
		pcgctl &= ~PCGCTL_STOPPCLK;
		DWC2_WRITE_4(hsotg, PCGCTL, pcgctl);

		spin_unlock_irqrestore(&hsotg->lock, flags);

		usleep_range(200000, 250000);
	} else {
		spin_unlock_irqrestore(&hsotg->lock, flags);
	}
}

/* Handles hub class-specific requests */
int
dwc2_hcd_hub_control(struct dwc2_hsotg *hsotg, u16 typereq,
				u16 wvalue, u16 windex, char *buf, u16 wlength)
{
	usb_hub_descriptor_t *hub_desc;
	usb_port_status_t ps;
	int retval = 0;
	u32 hprt0;
	u32 port_status;
	u32 speed;
	u32 pcgctl;

	switch (typereq) {
	case ClearHubFeature:
		dev_dbg(hsotg->dev, "ClearHubFeature %1xh\n", wvalue);

		switch (wvalue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			/* Nothing required here */
			break;

		default:
			retval = -EINVAL;
			dev_err(hsotg->dev,
				"ClearHubFeature request %1xh unknown\n",
				wvalue);
		}
		break;

	case ClearPortFeature:
// 		if (wvalue != USB_PORT_FEAT_L1)
			if (!windex || windex > 1)
				goto error;
		switch (wvalue) {
		case USB_PORT_FEAT_ENABLE:
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_ENABLE\n");
			hprt0 = dwc2_read_hprt0(hsotg);
			hprt0 |= HPRT0_ENA;
			DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			break;

		case USB_PORT_FEAT_SUSPEND:
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_SUSPEND\n");
			DWC2_WRITE_4(hsotg, PCGCTL, 0);
			usleep_range(20000, 40000);

			hprt0 = dwc2_read_hprt0(hsotg);
			hprt0 |= HPRT0_RES;
			DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			hprt0 &= ~HPRT0_SUSP;
			usleep_range(100000, 150000);

			hprt0 &= ~HPRT0_RES;
			DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			break;

		case USB_PORT_FEAT_POWER:
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_POWER\n");
			hprt0 = dwc2_read_hprt0(hsotg);
			hprt0 &= ~HPRT0_PWR;
			DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			break;

		case USB_PORT_FEAT_INDICATOR:
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_INDICATOR\n");
			/* Port indicator not supported */
			break;

		case USB_PORT_FEAT_C_CONNECTION:
			/*
			 * Clears driver's internal Connect Status Change flag
			 */
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_C_CONNECTION\n");
			hsotg->flags.b.port_connect_status_change = 0;
			break;

		case USB_PORT_FEAT_C_RESET:
			/* Clears driver's internal Port Reset Change flag */
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_C_RESET\n");
			hsotg->flags.b.port_reset_change = 0;
			break;

		case USB_PORT_FEAT_C_ENABLE:
			/*
			 * Clears the driver's internal Port Enable/Disable
			 * Change flag
			 */
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_C_ENABLE\n");
			hsotg->flags.b.port_enable_change = 0;
			break;

		case USB_PORT_FEAT_C_SUSPEND:
			/*
			 * Clears the driver's internal Port Suspend Change
			 * flag, which is set when resume signaling on the host
			 * port is complete
			 */
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_C_SUSPEND\n");
			hsotg->flags.b.port_suspend_change = 0;
			break;

		case USB_PORT_FEAT_C_PORT_L1:
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_C_PORT_L1\n");
			hsotg->flags.b.port_l1_change = 0;
			break;

		case USB_PORT_FEAT_C_OVER_CURRENT:
			dev_dbg(hsotg->dev,
				"ClearPortFeature USB_PORT_FEAT_C_OVER_CURRENT\n");
			hsotg->flags.b.port_over_current_change = 0;
			break;

		default:
			retval = -EINVAL;
			dev_err(hsotg->dev,
				"ClearPortFeature request %1xh unknown or unsupported\n",
				wvalue);
		}
		break;

	case GetHubDescriptor:
		dev_dbg(hsotg->dev, "GetHubDescriptor\n");
		hub_desc = (usb_hub_descriptor_t *)buf;
		hub_desc->bDescLength = 9;
		hub_desc->bDescriptorType = 0x29;
		hub_desc->bNbrPorts = 1;
		USETW(hub_desc->wHubCharacteristics, 0x08);
		hub_desc->bPwrOn2PwrGood = 1;
		hub_desc->bHubContrCurrent = 0;
		hub_desc->DeviceRemovable[0] = 0;
		hub_desc->DeviceRemovable[1] = 0xff;
		break;

	case GetHubStatus:
		dev_dbg(hsotg->dev, "GetHubStatus\n");
		memset(buf, 0, 4);
		break;

	case GetPortStatus:
		dev_vdbg(hsotg->dev,
			 "GetPortStatus wIndex=0x%04x flags=0x%08x\n", windex,
			 hsotg->flags.d32);
		if (!windex || windex > 1)
			goto error;

		port_status = 0;
		if (hsotg->flags.b.port_connect_status_change)
			port_status |= USB_PORT_STAT_C_CONNECTION;
		if (hsotg->flags.b.port_enable_change)
			port_status |= USB_PORT_STAT_C_ENABLE;
		if (hsotg->flags.b.port_suspend_change)
			port_status |= USB_PORT_STAT_C_SUSPEND;
		if (hsotg->flags.b.port_l1_change)
			port_status |= USB_PORT_STAT_C_L1;
		if (hsotg->flags.b.port_reset_change)
			port_status |= USB_PORT_STAT_C_RESET;
		if (hsotg->flags.b.port_over_current_change) {
			dev_warn(hsotg->dev, "Overcurrent change detected\n");
			port_status |= USB_PORT_STAT_C_OVERCURRENT;
		}
		USETW(ps.wPortChange, port_status);

		dev_vdbg(hsotg->dev, "wPortChange=%04x\n", port_status);
		if (!hsotg->flags.b.port_connect_status) {
			/*
			 * The port is disconnected, which means the core is
			 * either in device mode or it soon will be. Just
			 * return 0's for the remainder of the port status
			 * since the port register can't be read if the core
			 * is in device mode.
			 */
			USETW(ps.wPortStatus, 0);
			memcpy(buf, &ps, sizeof(ps));
			break;
		}

		port_status = 0;
		hprt0 = DWC2_READ_4(hsotg, HPRT0);
		dev_vdbg(hsotg->dev, "  HPRT0: 0x%08x\n", hprt0);

		if (hprt0 & HPRT0_CONNSTS)
			port_status |= USB_PORT_STAT_CONNECTION;
		if (hprt0 & HPRT0_ENA)
			port_status |= USB_PORT_STAT_ENABLE;
		if (hprt0 & HPRT0_SUSP)
			port_status |= USB_PORT_STAT_SUSPEND;
		if (hprt0 & HPRT0_OVRCURRACT)
			port_status |= USB_PORT_STAT_OVERCURRENT;
		if (hprt0 & HPRT0_RST)
			port_status |= USB_PORT_STAT_RESET;
		if (hprt0 & HPRT0_PWR)
			port_status |= USB_PORT_STAT_POWER;

		speed = (hprt0 & HPRT0_SPD_MASK) >> HPRT0_SPD_SHIFT;
		if (speed == HPRT0_SPD_HIGH_SPEED)
			port_status |= USB_PORT_STAT_HIGH_SPEED;
		else if (speed == HPRT0_SPD_LOW_SPEED)
			port_status |= USB_PORT_STAT_LOW_SPEED;

		if (hprt0 & HPRT0_TSTCTL_MASK)
			port_status |= USB_PORT_STAT_TEST;
		/* USB_PORT_FEAT_INDICATOR unsupported always 0 */
		USETW(ps.wPortStatus, port_status);

		dev_vdbg(hsotg->dev, "wPortStatus=%04x\n", port_status);
		memcpy(buf, &ps, sizeof(ps));
		break;

	case SetHubFeature:
		dev_dbg(hsotg->dev, "SetHubFeature\n");
		/* No HUB features supported */
		break;

	case SetPortFeature:
		dev_dbg(hsotg->dev, "SetPortFeature\n");
		if (wvalue != USB_PORT_FEAT_TEST && (!windex || windex > 1))
			goto error;

		if (!hsotg->flags.b.port_connect_status) {
			/*
			 * The port is disconnected, which means the core is
			 * either in device mode or it soon will be. Just
			 * return without doing anything since the port
			 * register can't be written if the core is in device
			 * mode.
			 */
			break;
		}

		switch (wvalue) {
		case USB_PORT_FEAT_SUSPEND:
			dev_dbg(hsotg->dev,
				"SetPortFeature - USB_PORT_FEAT_SUSPEND\n");
			if (windex != hsotg->otg_port)
				goto error;
			dwc2_port_suspend(hsotg, windex);
			break;

		case USB_PORT_FEAT_POWER:
			dev_dbg(hsotg->dev,
				"SetPortFeature - USB_PORT_FEAT_POWER\n");
			hprt0 = dwc2_read_hprt0(hsotg);
			hprt0 |= HPRT0_PWR;
			DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			break;

		case USB_PORT_FEAT_RESET:
			hprt0 = dwc2_read_hprt0(hsotg);
			dev_dbg(hsotg->dev,
				"SetPortFeature - USB_PORT_FEAT_RESET\n");
			pcgctl = DWC2_READ_4(hsotg, PCGCTL);
			pcgctl &= ~(PCGCTL_ENBL_SLEEP_GATING | PCGCTL_STOPPCLK);
			DWC2_WRITE_4(hsotg, PCGCTL, pcgctl);
			/* ??? Original driver does this */
			DWC2_WRITE_4(hsotg, PCGCTL, 0);

			hprt0 = dwc2_read_hprt0(hsotg);
			/* Clear suspend bit if resetting from suspend state */
			hprt0 &= ~HPRT0_SUSP;

			/*
			 * When B-Host the Port reset bit is set in the Start
			 * HCD Callback function, so that the reset is started
			 * within 1ms of the HNP success interrupt
			 */
			if (!dwc2_hcd_is_b_host(hsotg)) {
				hprt0 |= HPRT0_PWR | HPRT0_RST;
				dev_dbg(hsotg->dev,
					"In host mode, hprt0=%08x\n", hprt0);
				DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			}

			/* Clear reset bit in 10ms (FS/LS) or 50ms (HS) */
			usleep_range(50000, 70000);
			hprt0 &= ~HPRT0_RST;
			DWC2_WRITE_4(hsotg, HPRT0, hprt0);
			hsotg->lx_state = DWC2_L0; /* Now back to On state */
			break;

		case USB_PORT_FEAT_INDICATOR:
			dev_dbg(hsotg->dev,
				"SetPortFeature - USB_PORT_FEAT_INDICATOR\n");
			/* Not supported */
			break;

		default:
			retval = -EINVAL;
			dev_err(hsotg->dev,
				"SetPortFeature %1xh unknown or unsupported\n",
				wvalue);
			break;
		}
		break;

	default:
error:
		retval = -EINVAL;
		dev_dbg(hsotg->dev,
			"Unknown hub control request: %1xh wIndex: %1xh wValue: %1xh\n",
			typereq, windex, wvalue);
		break;
	}

	return retval;
}

int dwc2_hcd_get_frame_number(struct dwc2_hsotg *hsotg)
{
	u32 hfnum = DWC2_READ_4(hsotg, HFNUM);

#ifdef DWC2_DEBUG_SOF
	dev_vdbg(hsotg->dev, "DWC OTG HCD GET FRAME NUMBER %d\n",
		 (hfnum & HFNUM_FRNUM_MASK) >> HFNUM_FRNUM_SHIFT);
#endif
	return (hfnum & HFNUM_FRNUM_MASK) >> HFNUM_FRNUM_SHIFT;
}

int dwc2_hcd_is_b_host(struct dwc2_hsotg *hsotg)
{
	return hsotg->op_state == OTG_STATE_B_HOST;
}

struct dwc2_hcd_urb *
dwc2_hcd_urb_alloc(struct dwc2_hsotg *hsotg, int iso_desc_count,
		   gfp_t mem_flags)
{
	struct dwc2_hcd_urb *urb;
	u32 size = sizeof(*urb) + iso_desc_count *
		   sizeof(struct dwc2_hcd_iso_packet_desc);

	urb = malloc(size, M_DEVBUF, M_ZERO | mem_flags);
	if (urb)
		urb->packet_count = iso_desc_count;
	return urb;
}

void
dwc2_hcd_urb_free(struct dwc2_hsotg *hsotg, struct dwc2_hcd_urb *urb,
    int iso_desc_count)
{

	u32 size = sizeof(*urb) + iso_desc_count *
		   sizeof(struct dwc2_hcd_iso_packet_desc);

	free(urb, M_DEVBUF, size);
}

void
dwc2_hcd_urb_set_pipeinfo(struct dwc2_hsotg *hsotg, struct dwc2_hcd_urb *urb,
			  u8 dev_addr, u8 ep_num, u8 ep_type, u8 ep_dir,
			  u16 mps)
{
	if (dbg_perio() ||
	    ep_type == USB_ENDPOINT_XFER_BULK ||
	    ep_type == USB_ENDPOINT_XFER_CONTROL)
		dev_dbg(hsotg->dev, "urb=%p, xfer=%p\n", urb, urb->priv);
	urb->pipe_info.dev_addr = dev_addr;
	urb->pipe_info.ep_num = ep_num;
	urb->pipe_info.pipe_type = ep_type;
	urb->pipe_info.pipe_dir = ep_dir;
	urb->pipe_info.mps = mps;
}

/*
 * NOTE: This function will be removed once the peripheral controller code
 * is integrated and the driver is stable
 */
void dwc2_hcd_dump_state(struct dwc2_hsotg *hsotg)
{
#ifdef DWC2_DEBUG
	struct dwc2_host_chan *chan;
	struct dwc2_hcd_urb *urb;
	struct dwc2_qtd *qtd;
	int num_channels;
	u32 np_tx_status;
	u32 p_tx_status;
	int i;

	num_channels = hsotg->core_params->host_channels;
	dev_dbg(hsotg->dev, "\n");
	dev_dbg(hsotg->dev,
		"************************************************************\n");
	dev_dbg(hsotg->dev, "HCD State:\n");
	dev_dbg(hsotg->dev, "  Num channels: %d\n", num_channels);

	for (i = 0; i < num_channels; i++) {
		chan = hsotg->hc_ptr_array[i];
		dev_dbg(hsotg->dev, "  Channel %d:\n", i);
		dev_dbg(hsotg->dev,
			"    dev_addr: %d, ep_num: %d, ep_is_in: %d\n",
			chan->dev_addr, chan->ep_num, chan->ep_is_in);
		dev_dbg(hsotg->dev, "    speed: %d\n", chan->speed);
		dev_dbg(hsotg->dev, "    ep_type: %d\n", chan->ep_type);
		dev_dbg(hsotg->dev, "    max_packet: %d\n", chan->max_packet);
		dev_dbg(hsotg->dev, "    data_pid_start: %d\n",
			chan->data_pid_start);
		dev_dbg(hsotg->dev, "    multi_count: %d\n", chan->multi_count);
		dev_dbg(hsotg->dev, "    xfer_started: %d\n",
			chan->xfer_started);
		dev_dbg(hsotg->dev, "    xfer_buf: %p\n", chan->xfer_buf);
		dev_dbg(hsotg->dev, "    xfer_dma: %08lx\n",
			(unsigned long)chan->xfer_dma);
		dev_dbg(hsotg->dev, "    xfer_len: %d\n", chan->xfer_len);
		dev_dbg(hsotg->dev, "    xfer_count: %d\n", chan->xfer_count);
		dev_dbg(hsotg->dev, "    halt_on_queue: %d\n",
			chan->halt_on_queue);
		dev_dbg(hsotg->dev, "    halt_pending: %d\n",
			chan->halt_pending);
		dev_dbg(hsotg->dev, "    halt_status: %d\n", chan->halt_status);
		dev_dbg(hsotg->dev, "    do_split: %d\n", chan->do_split);
		dev_dbg(hsotg->dev, "    complete_split: %d\n",
			chan->complete_split);
		dev_dbg(hsotg->dev, "    hub_addr: %d\n", chan->hub_addr);
		dev_dbg(hsotg->dev, "    hub_port: %d\n", chan->hub_port);
		dev_dbg(hsotg->dev, "    xact_pos: %d\n", chan->xact_pos);
		dev_dbg(hsotg->dev, "    requests: %d\n", chan->requests);
		dev_dbg(hsotg->dev, "    qh: %p\n", chan->qh);

		if (chan->xfer_started) {
			dev_dbg(hsotg->dev, "    hfnum: 0x%08x\n",
			    DWC2_READ_4(hsotg, HFNUM));
			dev_dbg(hsotg->dev, "    hcchar: 0x%08x\n",
			    DWC2_READ_4(hsotg, HCCHAR(i)));
			dev_dbg(hsotg->dev, "    hctsiz: 0x%08x\n",
			    DWC2_READ_4(hsotg, HCTSIZ(i)));
			dev_dbg(hsotg->dev, "    hcint: 0x%08x\n",
			    DWC2_READ_4(hsotg, HCINT(i)));
			dev_dbg(hsotg->dev, "    hcintmsk: 0x%08x\n",
			    DWC2_READ_4(hsotg, HCINTMSK(i)));
		}

		if (!(chan->xfer_started && chan->qh))
			continue;

		list_for_each_entry(qtd, &chan->qh->qtd_list, qtd_list_entry) {
			if (!qtd->in_process)
				break;
			urb = qtd->urb;
			dev_dbg(hsotg->dev, "    URB Info:\n");
			dev_dbg(hsotg->dev, "      qtd: %p, urb: %p\n",
				qtd, urb);
			if (urb) {
				dev_dbg(hsotg->dev,
					"      Dev: %d, EP: %d %s\n",
					dwc2_hcd_get_dev_addr(&urb->pipe_info),
					dwc2_hcd_get_ep_num(&urb->pipe_info),
					dwc2_hcd_is_pipe_in(&urb->pipe_info) ?
					"IN" : "OUT");
				dev_dbg(hsotg->dev,
					"      Max packet size: %d\n",
					dwc2_hcd_get_mps(&urb->pipe_info));
				dev_dbg(hsotg->dev,
					"      transfer_buffer: %p\n",
					urb->buf);
				dev_dbg(hsotg->dev,
					"      transfer_dma: %08lx\n",
					(unsigned long)urb->dma);
				dev_dbg(hsotg->dev,
					"      transfer_buffer_length: %d\n",
					urb->length);
				dev_dbg(hsotg->dev, "      actual_length: %d\n",
					urb->actual_length);
			}
		}
	}

	dev_dbg(hsotg->dev, "  non_periodic_channels: %d\n",
		hsotg->non_periodic_channels);
	dev_dbg(hsotg->dev, "  periodic_channels: %d\n",
		hsotg->periodic_channels);
	dev_dbg(hsotg->dev, "  periodic_usecs: %d\n", hsotg->periodic_usecs);
	np_tx_status = DWC2_READ_4(hsotg, GNPTXSTS);
	dev_dbg(hsotg->dev, "  NP Tx Req Queue Space Avail: %d\n",
		(np_tx_status & TXSTS_QSPCAVAIL_MASK) >> TXSTS_QSPCAVAIL_SHIFT);
	dev_dbg(hsotg->dev, "  NP Tx FIFO Space Avail: %d\n",
		(np_tx_status & TXSTS_FSPCAVAIL_MASK) >> TXSTS_FSPCAVAIL_SHIFT);
	p_tx_status = DWC2_READ_4(hsotg, HPTXSTS);
	dev_dbg(hsotg->dev, "  P Tx Req Queue Space Avail: %d\n",
		(p_tx_status & TXSTS_QSPCAVAIL_MASK) >> TXSTS_QSPCAVAIL_SHIFT);
	dev_dbg(hsotg->dev, "  P Tx FIFO Space Avail: %d\n",
		(p_tx_status & TXSTS_FSPCAVAIL_MASK) >> TXSTS_FSPCAVAIL_SHIFT);
	dwc2_hcd_dump_frrem(hsotg);

	dwc2_dump_global_registers(hsotg);
	dwc2_dump_host_registers(hsotg);
	dev_dbg(hsotg->dev,
		"************************************************************\n");
	dev_dbg(hsotg->dev, "\n");
#endif
}

/*
 * NOTE: This function will be removed once the peripheral controller code
 * is integrated and the driver is stable
 */
void dwc2_hcd_dump_frrem(struct dwc2_hsotg *hsotg)
{
#ifdef DWC2_DUMP_FRREM
	dev_dbg(hsotg->dev, "Frame remaining at SOF:\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->frrem_samples, hsotg->frrem_accum,
		hsotg->frrem_samples > 0 ?
		hsotg->frrem_accum / hsotg->frrem_samples : 0);
	dev_dbg(hsotg->dev, "\n");
	dev_dbg(hsotg->dev, "Frame remaining at start_transfer (uframe 7):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_7_samples,
		hsotg->hfnum_7_frrem_accum,
		hsotg->hfnum_7_samples > 0 ?
		hsotg->hfnum_7_frrem_accum / hsotg->hfnum_7_samples : 0);
	dev_dbg(hsotg->dev, "Frame remaining at start_transfer (uframe 0):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_0_samples,
		hsotg->hfnum_0_frrem_accum,
		hsotg->hfnum_0_samples > 0 ?
		hsotg->hfnum_0_frrem_accum / hsotg->hfnum_0_samples : 0);
	dev_dbg(hsotg->dev, "Frame remaining at start_transfer (uframe 1-6):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_other_samples,
		hsotg->hfnum_other_frrem_accum,
		hsotg->hfnum_other_samples > 0 ?
		hsotg->hfnum_other_frrem_accum / hsotg->hfnum_other_samples :
		0);
	dev_dbg(hsotg->dev, "\n");
	dev_dbg(hsotg->dev, "Frame remaining at sample point A (uframe 7):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_7_samples_a, hsotg->hfnum_7_frrem_accum_a,
		hsotg->hfnum_7_samples_a > 0 ?
		hsotg->hfnum_7_frrem_accum_a / hsotg->hfnum_7_samples_a : 0);
	dev_dbg(hsotg->dev, "Frame remaining at sample point A (uframe 0):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_0_samples_a, hsotg->hfnum_0_frrem_accum_a,
		hsotg->hfnum_0_samples_a > 0 ?
		hsotg->hfnum_0_frrem_accum_a / hsotg->hfnum_0_samples_a : 0);
	dev_dbg(hsotg->dev, "Frame remaining at sample point A (uframe 1-6):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_other_samples_a, hsotg->hfnum_other_frrem_accum_a,
		hsotg->hfnum_other_samples_a > 0 ?
		hsotg->hfnum_other_frrem_accum_a / hsotg->hfnum_other_samples_a
		: 0);
	dev_dbg(hsotg->dev, "\n");
	dev_dbg(hsotg->dev, "Frame remaining at sample point B (uframe 7):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_7_samples_b, hsotg->hfnum_7_frrem_accum_b,
		hsotg->hfnum_7_samples_b > 0 ?
		hsotg->hfnum_7_frrem_accum_b / hsotg->hfnum_7_samples_b : 0);
	dev_dbg(hsotg->dev, "Frame remaining at sample point B (uframe 0):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_0_samples_b, hsotg->hfnum_0_frrem_accum_b,
		(hsotg->hfnum_0_samples_b > 0) ?
		hsotg->hfnum_0_frrem_accum_b / hsotg->hfnum_0_samples_b : 0);
	dev_dbg(hsotg->dev, "Frame remaining at sample point B (uframe 1-6):\n");
	dev_dbg(hsotg->dev, "  samples %u, accum %llu, avg %llu\n",
		hsotg->hfnum_other_samples_b, hsotg->hfnum_other_frrem_accum_b,
		(hsotg->hfnum_other_samples_b > 0) ?
		hsotg->hfnum_other_frrem_accum_b / hsotg->hfnum_other_samples_b
		: 0);
#endif
}

struct wrapper_priv_data {
	struct dwc2_hsotg *hsotg;
};


void dwc2_host_start(struct dwc2_hsotg *hsotg)
{
// 	struct usb_hcd *hcd = dwc2_hsotg_to_hcd(hsotg);

// 	hcd->self.is_b_host = dwc2_hcd_is_b_host(hsotg);
	_dwc2_hcd_start(hsotg);
}

void dwc2_host_disconnect(struct dwc2_hsotg *hsotg)
{
//	struct usb_hcd *hcd = dwc2_hsotg_to_hcd(hsotg);

//	hcd->self.is_b_host = 0;
}


/*
 * Work queue function for starting the HCD when A-Cable is connected
 */
void
dwc2_hcd_start_func(struct task *work)
{
	struct dwc2_hsotg *hsotg = container_of(work, struct dwc2_hsotg,
						start_work.work);

	dev_dbg(hsotg->dev, "%s() %p\n", __func__, hsotg);
	dwc2_host_start(hsotg);
}

/*
 * Reset work queue function
 */
void
dwc2_hcd_reset_func(struct task *work)
{
	struct dwc2_hsotg *hsotg = container_of(work, struct dwc2_hsotg,
						reset_work.work);
	u32 hprt0;

	dev_dbg(hsotg->dev, "USB RESET function called\n");
	hprt0 = dwc2_read_hprt0(hsotg);
	hprt0 &= ~HPRT0_RST;
	DWC2_WRITE_4(hsotg, HPRT0, hprt0);
	hsotg->flags.b.port_reset_change = 1;

	dwc2_root_intr(hsotg->hsotg_sc);
}

/*
 * =========================================================================
 *  Linux HC Driver Functions
 * =========================================================================
 */

/*
 * Initializes the DWC_otg controller and its root hub and prepares it for host
 * mode operation. Activates the root port. Returns 0 on success and a negative
 * error code on failure.
 */


/*
 * Frees secondary storage associated with the dwc2_hsotg structure contained
 * in the struct usb_hcd field
 */
static void dwc2_hcd_free(struct dwc2_hsotg *hsotg)
{
	u32 ahbcfg;
	u32 dctl;
	int i;

	dev_dbg(hsotg->dev, "DWC OTG HCD FREE\n");

	/* Free memory for QH/QTD lists */
	dwc2_qh_list_free(hsotg, &hsotg->non_periodic_sched_inactive);
	dwc2_qh_list_free(hsotg, &hsotg->non_periodic_sched_active);
	dwc2_qh_list_free(hsotg, &hsotg->periodic_sched_inactive);
	dwc2_qh_list_free(hsotg, &hsotg->periodic_sched_ready);
	dwc2_qh_list_free(hsotg, &hsotg->periodic_sched_assigned);
	dwc2_qh_list_free(hsotg, &hsotg->periodic_sched_queued);

	/* Free memory for the host channels */
	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		struct dwc2_host_chan *chan = hsotg->hc_ptr_array[i];

		if (chan != NULL) {
			dev_dbg(hsotg->dev, "HCD Free channel #%i, chan=%p\n",
				i, chan);
			hsotg->hc_ptr_array[i] = NULL;
			free(chan, M_DEVBUF, sizeof(*chan));
		}
	}

	if (hsotg->core_params->dma_enable > 0) {
		if (hsotg->status_buf) {
			usb_freemem(&hsotg->hsotg_sc->sc_bus,
				    &hsotg->status_buf_usbdma);
			hsotg->status_buf = NULL;
		}
	} else {
		free(hsotg->status_buf, M_DEVBUF, DWC2_HCD_STATUS_BUF_SIZE);
		hsotg->status_buf = NULL;
	}

	ahbcfg = DWC2_READ_4(hsotg, GAHBCFG);

	/* Disable all interrupts */
	ahbcfg &= ~GAHBCFG_GLBL_INTR_EN;
	DWC2_WRITE_4(hsotg, GAHBCFG, ahbcfg);
	DWC2_WRITE_4(hsotg, GINTMSK, 0);

	if (hsotg->hw_params.snpsid >= DWC2_CORE_REV_3_00a) {
		dctl = DWC2_READ_4(hsotg, DCTL);
		dctl |= DCTL_SFTDISCON;
		DWC2_WRITE_4(hsotg, DCTL, dctl);
	}

	if (hsotg->wq_otg) {
		taskq_destroy(hsotg->wq_otg);
	}

	free(hsotg->core_params, M_DEVBUF, sizeof(*hsotg->core_params));
	hsotg->core_params = NULL;
	timeout_del(&hsotg->wkp_timer);
}

static void dwc2_hcd_release(struct dwc2_hsotg *hsotg)
{
	/* Turn off all host-specific interrupts */
	dwc2_disable_host_interrupts(hsotg);

	dwc2_hcd_free(hsotg);
}

/*
 * Sets all parameters to the given value.
 *
 * Assumes that the dwc2_core_params struct contains only integers.
 */
void dwc2_set_all_params(struct dwc2_core_params *params, int value)
{
	int *p = (int *)params;
	size_t size = sizeof(*params) / sizeof(*p);
	int i;

	for (i = 0; i < size; i++)
		p[i] = value;
}

/*
 * Initializes the HCD. This function allocates memory for and initializes the
 * static parts of the usb_hcd and dwc2_hsotg structures. It also registers the
 * USB bus with the core and calls the hc_driver->start() function. It returns
 * a negative error on failure.
 */
int dwc2_hcd_init(struct dwc2_hsotg *hsotg,
		  const struct dwc2_core_params *params)
{
	struct dwc2_host_chan *channel;
	int i, num_channels;
	int retval;

	dev_dbg(hsotg->dev, "DWC OTG HCD INIT\n");

	/* Detect config values from hardware */
	retval = dwc2_get_hwparams(hsotg);

	if (retval)
		return retval;

	retval = -ENOMEM;

	dev_dbg(hsotg->dev, "hcfg=%08x\n", DWC2_READ_4(hsotg, HCFG));

#ifdef CONFIG_USB_DWC2_TRACK_MISSED_SOFS
	hsotg->frame_num_array = malloc(sizeof(*hsotg->frame_num_array) *
					FRAME_NUM_ARRAY_SIZE, M_DEVBUF,
					M_ZERO | M_WAITOK);
	if (!hsotg->frame_num_array)
		goto error1;
	hsotg->last_frame_num_array = malloc(
			sizeof(*hsotg->last_frame_num_array) *
			FRAME_NUM_ARRAY_SIZE, M_DEVBUF, M_ZERO | M_WAITOK);
	if (!hsotg->last_frame_num_array)
		goto error1;
	hsotg->last_frame_num = HFNUM_MAX_FRNUM;
#endif

	hsotg->core_params = malloc(sizeof(*hsotg->core_params), M_DEVBUF,
				    M_ZERO | M_WAITOK);
	if (!hsotg->core_params)
		goto error1;

	dwc2_set_all_params(hsotg->core_params, -1);

	/* Validate parameter values */
	dwc2_set_parameters(hsotg, params);

	spin_lock_init(&hsotg->lock);

	/*
	 * Disable the global interrupt until all the interrupt handlers are
	 * installed
	 */
	dwc2_disable_global_interrupts(hsotg);

	/* Initialize the DWC_otg core, and select the Phy type */
	retval = dwc2_core_init(hsotg, true);
	if (retval)
		goto error2;

	/* Create new workqueue and init work */
	retval = -ENOMEM;
	hsotg->wq_otg = taskq_create("dwc2", 1, IPL_USB);
	if (hsotg->wq_otg == NULL) {
		dev_err(hsotg->dev, "Failed to create workqueue\n");
		goto error2;
	}

	timeout_set(&hsotg->wkp_timer, dwc2_wakeup_detected, hsotg);

	/* Initialize the non-periodic schedule */
	INIT_LIST_HEAD(&hsotg->non_periodic_sched_inactive);
	INIT_LIST_HEAD(&hsotg->non_periodic_sched_active);

	/* Initialize the periodic schedule */
	INIT_LIST_HEAD(&hsotg->periodic_sched_inactive);
	INIT_LIST_HEAD(&hsotg->periodic_sched_ready);
	INIT_LIST_HEAD(&hsotg->periodic_sched_assigned);
	INIT_LIST_HEAD(&hsotg->periodic_sched_queued);

	/*
	 * Create a host channel descriptor for each host channel implemented
	 * in the controller. Initialize the channel descriptor array.
	 */
	INIT_LIST_HEAD(&hsotg->free_hc_list);
	num_channels = hsotg->core_params->host_channels;
	memset(&hsotg->hc_ptr_array[0], 0, sizeof(hsotg->hc_ptr_array));

	for (i = 0; i < num_channels; i++) {
		channel = malloc(sizeof(*channel), M_DEVBUF, M_ZERO | M_WAITOK);
		if (channel == NULL)
			goto error3;
		channel->hc_num = i;
		hsotg->hc_ptr_array[i] = channel;
	}

	if (hsotg->core_params->uframe_sched > 0)
		dwc2_hcd_init_usecs(hsotg);

	/* Initialize hsotg start work */
	INIT_DELAYED_WORK(&hsotg->start_work, dwc2_hcd_start_func);

	/* Initialize port reset work */
	INIT_DELAYED_WORK(&hsotg->reset_work, dwc2_hcd_reset_func);

	/*
	 * Allocate space for storing data on status transactions. Normally no
	 * data is sent, but this space acts as a bit bucket. This must be
	 * done after usb_add_hcd since that function allocates the DMA buffer
	 * pool.
	 */
	hsotg->status_buf = NULL;
	if (hsotg->core_params->dma_enable > 0) {
		retval = usb_allocmem(&hsotg->hsotg_sc->sc_bus,
				      DWC2_HCD_STATUS_BUF_SIZE, 0,
				      &hsotg->status_buf_usbdma);
		if (!retval) {
			hsotg->status_buf = KERNADDR(&hsotg->status_buf_usbdma, 0);
			hsotg->status_buf_dma = DMAADDR(&hsotg->status_buf_usbdma, 0);
		}
	} else
		hsotg->status_buf = malloc(DWC2_HCD_STATUS_BUF_SIZE, M_DEVBUF,
					   M_ZERO | M_WAITOK);

	if (!hsotg->status_buf)
		goto error3;

	hsotg->otg_port = 1;
	hsotg->frame_list = NULL;
	hsotg->frame_list_dma = 0;
	hsotg->periodic_qh_count = 0;

	/* Initiate lx_state to L3 disconnected state */
	hsotg->lx_state = DWC2_L3;

 	_dwc2_hcd_start(hsotg);

	dwc2_hcd_dump_state(hsotg);

	dwc2_enable_global_interrupts(hsotg);

	return 0;

error3:
	dwc2_hcd_release(hsotg);
error2:
error1:
	free(hsotg->core_params, M_DEVBUF, sizeof(*hsotg->core_params));

#ifdef CONFIG_USB_DWC2_TRACK_MISSED_SOFS
	free(hsotg->last_frame_num_array, M_DEVBUF,
		sizeof(*hsotg->last_frame_num_array) * FRAME_NUM_ARRAY_SIZE);
	free(hsotg->frame_num_array, M_DEVBUF,
		sizeof(*hsotg->frame_num_array) * FRAME_NUM_ARRAY_SIZE);
#endif

	dev_err(hsotg->dev, "%s() FAILED, returning %d\n", __func__, retval);
	return retval;
}
