/*	$OpenBSD: fwohci.c,v 1.11 2003/01/12 12:05:04 tdeval Exp $	*/
/*	$NetBSD: fwohci.c,v 1.54 2002/03/29 05:06:42 jmc Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IEEE1394 Open Host Controller Interface
 *	based on OHCI Specification 1.1 (January 6, 2000)
 * The first version to support network interface part is wrtten by
 * Atsushi Onoe <onoe@netbsd.org>.
 */

/*
 * The first version to support isochronous acquisition part is wrtten
 * by HAYAKAWA Koichi <haya@netbsd.org>.
 */

#include <sys/cdefs.h>
#ifdef	__KERNEL_RCSID
__KERNEL_RCSID(0, "$NetBSD: fwohci.c,v 1.54 2002/03/29 05:06:42 jmc Exp $");
#endif

#define	DOUBLEBUF 0
#define	NO_THREAD 0

#ifdef	__NetBSD__
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/socket.h>
#ifdef	__NetBSD__
#include <sys/callout.h>
#else
#include <sys/timeout.h>
#endif
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#ifdef	__OpenBSD__
#include <sys/endian.h>
#endif

#if	__NetBSD_Version__ >= 105010000 || !defined(__NetBSD__)
#include <uvm/uvm_extern.h>
#else
#include <vm/vm.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ieee1394/ieee1394reg.h>
#include <dev/ieee1394/fwohcireg.h>

#include <dev/ieee1394/ieee1394var.h>
#include <dev/ieee1394/fwohcivar.h>

const char * const ieee1394_speeds[] = { IEEE1394_SPD_STRINGS };
const char * const ieee1394_power[] = { IEEE1394_POW_STRINGS };

#if 0
int fwohci_dmamem_alloc(struct fwohci_softc *sc, int size,
    int alignment, bus_dmamap_t *mapp, caddr_t *kvap, int flags);
#endif
void fwohci_create_event_thread(void *);
void fwohci_thread_init(void *);

void fwohci_event_thread(struct fwohci_softc *);
void fwohci_event_dispatch(struct fwohci_softc *);
void fwohci_hw_init(struct fwohci_softc *);
void fwohci_power(int, void *);
void fwohci_shutdown(void *);

int  fwohci_desc_alloc(struct fwohci_softc *);
void fwohci_desc_free(struct fwohci_softc *);
struct fwohci_desc *fwohci_desc_get(struct fwohci_softc *, int);
void fwohci_desc_put(struct fwohci_softc *, struct fwohci_desc *, int);

int  fwohci_ctx_alloc(struct fwohci_softc *, struct fwohci_ctx **,
    int, int, int);
void fwohci_ctx_free(struct fwohci_softc *, struct fwohci_ctx *);
void fwohci_ctx_init(struct fwohci_softc *, struct fwohci_ctx *);

int  fwohci_buf_alloc(struct fwohci_softc *, struct fwohci_buf *);
void fwohci_buf_free(struct fwohci_softc *, struct fwohci_buf *);
void fwohci_buf_init_rx(struct fwohci_softc *);
void fwohci_buf_start_rx(struct fwohci_softc *);
void fwohci_buf_stop_rx(struct fwohci_softc *);
void fwohci_buf_stop_tx(struct fwohci_softc *);
void fwohci_buf_next(struct fwohci_softc *, struct fwohci_ctx *);
int  fwohci_buf_pktget(struct fwohci_softc *, struct fwohci_buf **,
    caddr_t *, int);
int  fwohci_buf_input(struct fwohci_softc *, struct fwohci_ctx *,
    struct fwohci_pkt *);
int  fwohci_buf_input_ppb(struct fwohci_softc *, struct fwohci_ctx *,
    struct fwohci_pkt *);

u_int8_t fwohci_phy_read(struct fwohci_softc *, u_int8_t);
void fwohci_phy_write(struct fwohci_softc *, u_int8_t, u_int8_t);
void fwohci_phy_busreset(struct fwohci_softc *);
void fwohci_phy_input(struct fwohci_softc *, struct fwohci_pkt *);

int  fwohci_handler_set(struct fwohci_softc *, int, u_int32_t, u_int32_t,
    u_int32_t, int (*)(struct fwohci_softc *, void *, struct fwohci_pkt *),
    void *);
int  fwohci_block_handler_set(struct fwohci_softc *, int, u_int32_t, u_int32_t,
    u_int32_t, int, int (*)(struct fwohci_softc *, void *, struct fwohci_pkt *),
    void *);

void fwohci_arrq_input(struct fwohci_softc *, struct fwohci_ctx *);
void fwohci_arrs_input(struct fwohci_softc *, struct fwohci_ctx *);
void fwohci_ir_input(struct fwohci_softc *, struct fwohci_ctx *);

int  fwohci_at_output(struct fwohci_softc *, struct fwohci_ctx *,
    struct fwohci_pkt *);
void fwohci_at_done(struct fwohci_softc *, struct fwohci_ctx *, int);
void fwohci_atrs_output(struct fwohci_softc *, int, struct fwohci_pkt *,
    struct fwohci_pkt *);

int  fwohci_guidrom_init(struct fwohci_softc *);
void fwohci_configrom_init(struct fwohci_softc *);
int  fwohci_configrom_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
void fwohci_selfid_init(struct fwohci_softc *);
int  fwohci_selfid_input(struct fwohci_softc *);

void fwohci_csr_init(struct fwohci_softc *);
int  fwohci_csr_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);

void fwohci_uid_collect(struct fwohci_softc *);
void fwohci_uid_req(struct fwohci_softc *, int);
int  fwohci_uid_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
int  fwohci_uid_lookup(struct fwohci_softc *, const u_int8_t *);
void fwohci_check_nodes(struct fwohci_softc *);

int  fwohci_if_inreg(struct device *, u_int32_t, u_int32_t,
    void (*)(struct device *, struct mbuf *));
int  fwohci_if_input(struct fwohci_softc *, void *, struct fwohci_pkt *);
int  fwohci_if_input_iso(struct fwohci_softc *, void *, struct fwohci_pkt *);
int  fwohci_if_output(struct device *, struct mbuf *,
    void (*)(struct device *, struct mbuf *));
int fwohci_if_setiso(struct device *, u_int32_t, u_int32_t, u_int32_t,
    void (*)(struct device *, struct mbuf *));
int  fwohci_read(struct ieee1394_abuf *);
int  fwohci_write(struct ieee1394_abuf *);
int  fwohci_read_resp(struct fwohci_softc *, void *, struct fwohci_pkt *);
int  fwohci_write_ack(struct fwohci_softc *, void *, struct fwohci_pkt *);
int  fwohci_read_multi_resp(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
int  fwohci_inreg(struct ieee1394_abuf *, int);
int  fwohci_unreg(struct ieee1394_abuf *, int);
int  fwohci_parse_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
#ifdef	__NetBSD__
int  fwohci_submatch(struct device *, struct cfdata *, void *);
#else
int  fwohci_submatch(struct device *, void *, void *);
#endif
u_int16_t fwohci_crc16(u_int32_t *, int);

#ifdef	FWOHCI_DEBUG
void fwohci_show_intr(struct fwohci_softc *, u_int32_t);
void fwohci_show_phypkt(struct fwohci_softc *, u_int32_t);

/* 1 is normal debug, 2 is verbose debug, 3 is complete (packet dumps). */

#include <sys/syslog.h>
extern int log_open;
int fwohci_oldlog;
#define	DPRINTF(x)	if (fwohcidebug) do {				\
	fwohci_oldlog = log_open; log_open = 1;				\
	addlog x; log_open = fwohci_oldlog;				\
} while (0)
#define	DPRINTFN(n,x)	if (fwohcidebug>(n)) do {			\
	fwohci_oldlog = log_open; log_open = 1;				\
	addlog x; log_open = fwohci_oldlog;				\
} while (0)
#ifdef	FW_MALLOC_DEBUG
#define	MPRINTF(x,y)	DPRINTF(("%s[%d]: %s 0x%08x\n",			\
			    __func__, __LINE__, (x), (u_int32_t)(y)))
#else	/* !FW_MALLOC_DEBUG */
#define	MPRINTF(x,y)
#endif	/* FW_MALLOC_DEBUG */

int	fwohcidebug = 0;
int	fwintr = 0;
caddr_t	fwptr = 0;
int	fwlen = 0;
struct fwohci_buf *fwbuf = NULL;
#else	/* FWOHCI_DEBUG */
#define	DPRINTF(x)
#define	DPRINTFN(n,x)
#define	MPRINTF(x,y)
#endif	/* ! FWOHCI_DEBUG */

#ifdef	__OpenBSD__
struct cfdriver fwohci_cd = {
	NULL, "fwohci", DV_DULL
};
#endif

int
fwohci_init(struct fwohci_softc *sc, const struct evcnt *ev)
{
	int i;
	u_int32_t val;
#if 0
	int error;
#endif

#ifdef	__NetBSD__
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, ev,
	    sc->sc_sc1394.sc1394_dev.dv_xname, "intr");

	evcnt_attach_dynamic(&sc->sc_isocnt, EVCNT_TYPE_MISC, ev,
	    sc->sc_sc1394.sc1394_dev.dv_xname, "iso");
	evcnt_attach_dynamic(&sc->sc_isopktcnt, EVCNT_TYPE_MISC, ev,
	    sc->sc_sc1394.sc1394_dev.dv_xname, "isopackets");
#endif

	/*
	 * Wait for reset completion.
	 */
	for (i = 0; i < OHCI_LOOP; i++) {
		val = OHCI_CSR_READ(sc, OHCI_REG_HCControlClear);
		if ((val & OHCI_HCControl_SoftReset) == 0)
			break;
		DELAY(10);
	}

	/*
	 * What dialect of OHCI is this device ?
	 */
	val = OHCI_CSR_READ(sc, OHCI_REG_Version);
	printf("%s: OHCI %u.%u", sc->sc_sc1394.sc1394_dev.dv_xname,
	    OHCI_Version_GET_Version(val), OHCI_Version_GET_Revision(val));

	LIST_INIT(&sc->sc_nodelist);

	if (fwohci_guidrom_init(sc) != 0) {
		printf("\n%s: fatal: no global UID ROM\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		return -1;
	}

	printf(", %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
	    sc->sc_sc1394.sc1394_guid[0], sc->sc_sc1394.sc1394_guid[1],
	    sc->sc_sc1394.sc1394_guid[2], sc->sc_sc1394.sc1394_guid[3],
	    sc->sc_sc1394.sc1394_guid[4], sc->sc_sc1394.sc1394_guid[5],
	    sc->sc_sc1394.sc1394_guid[6], sc->sc_sc1394.sc1394_guid[7]);

	/*
	 * Get the maximum link speed and receive size.
	 */
	val = OHCI_CSR_READ(sc, OHCI_REG_BusOptions);
	sc->sc_sc1394.sc1394_link_speed =
	    OHCI_BITVAL(val, OHCI_BusOptions_LinkSpd);
	if (sc->sc_sc1394.sc1394_link_speed < IEEE1394_SPD_MAX) {
		printf(", %s",
		    ieee1394_speeds[sc->sc_sc1394.sc1394_link_speed]);
	} else {
		printf(", unknown speed %u", sc->sc_sc1394.sc1394_link_speed);
	}

	/*
	 * MaxRec is encoded as log2(max_rec_octets)-1
	 */
	sc->sc_sc1394.sc1394_max_receive =
	    1 << (OHCI_BITVAL(val, OHCI_BusOptions_MaxRec) + 1);
	printf(", %u max_rec", sc->sc_sc1394.sc1394_max_receive);

	/*
	 * Count how many isochronous ctx we have.
	 */
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntMaskSet, ~0);
	val = OHCI_CSR_READ(sc, OHCI_REG_IsoRecvIntMaskClear);
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntMaskClear, ~0);
	for (i = 0; val != 0; val >>= 1) {
		if (val & 0x1)
			i++;
	}
	sc->sc_isoctx = i;
	printf(", %d iso_ctx", sc->sc_isoctx);

	printf("\n");

#if 0
	error = fwohci_dmamem_alloc(sc, OHCI_CONFIG_SIZE,
	    OHCI_CONFIG_ALIGNMENT, &sc->sc_configrom_map,
	    (caddr_t *) &sc->sc_configrom, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	return error;
#endif

	MALLOC(sc->sc_dying, int *, sizeof(int), M_DEVBUF, M_WAITOK);
	MPRINTF("MALLOC(DEVBUF)", sc->sc_dying);
	DPRINTF(("%s: sc_dying 0x%08x\n", __func__, (u_int32_t)sc->sc_dying));
	*sc->sc_dying = 0;
	sc->sc_nodeid = 0xFFFF;		/* Invalid. */

#ifdef	__NetBSD__
	kthread_create(fwohci_create_event_thread, sc);
#else
	if (initproc == NULL)
		kthread_create_deferred(fwohci_create_event_thread, sc);
	else
		/* Late binding, threads already running. */
		fwohci_create_event_thread(sc);
#endif

	return 0;
}

int
fwohci_if_setiso(struct device *self, u_int32_t channel, u_int32_t tag,
    u_int32_t direction, void (*handler)(struct device *, struct mbuf *))
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;
	int retval;
	int s;

	if (direction == 1) {
		return EIO;
	}

	s = splnet();
	retval = fwohci_handler_set(sc, IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK,
	    channel, tag, 0, fwohci_if_input_iso, handler);
	splx(s);

	if (!retval) {
		printf("%s: dummy iso handler set\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
	} else {
		printf("%s: dummy iso handler cannot set\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
	}

	return retval;
}

int
fwohci_intr(void *arg)
{
	struct fwohci_softc * const sc = arg;
#if 1
	int progress = 0;
#else
	int progress = (sc->sc_intmask != 0);
#endif
	u_int32_t intmask, iso;

	splassert(IPL_BIO);

#ifdef	FWOHCI_DEBUG
	//DPRINTFN(3,("%s:  in(%d)\n", __func__, fwintr));
	fwintr++;
#endif	/* FWOHCI_DEBUG */

#if 1
	for (;;) {
#endif
		intmask = OHCI_CSR_READ(sc, OHCI_REG_IntEventClear);

		/*
		 * On a bus reset, everything except bus reset gets
		 * cleared. That can't get cleared until the selfid
		 * phase completes (which happens outside the
		 * interrupt routines). So if just a bus reset is left
		 * in the mask and it's already in the sc_intmask,
		 * just return.
		 */

		if ((intmask == 0xFFFFFFFF) || (intmask == 0) ||
		    (progress && (intmask == OHCI_Int_BusReset) &&
		       (sc->sc_intmask & OHCI_Int_BusReset))) {

			if (intmask == 0xFFFFFFFF)
				config_detach(((struct device *)sc)
				    ->dv_parent, 0);

			if (progress) {
#if	NO_THREAD
				fwohci_event_dispatch(sc);
#else	/* NO_THREAD */
				wakeup(fwohci_event_thread);
#endif	/* NO_THREAD */
			}

#ifdef	FWOHCI_DEBUG
			--fwintr;
			//DPRINTFN(3,("%s: out(%d)\n", __func__, fwintr));
#endif	/* FWOHCI_DEBUG */

			return (progress);
		}
#if 1
		OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
		    intmask & ~OHCI_Int_BusReset);
#else
		DPRINTFN(2,("%s: IntEventClear(0x%08x) IntMaskClear(0x%08x)\n",
		    __func__,
		    intmask & ~OHCI_Int_BusReset,
		    intmask & ~OHCI_Int_BusReset & ~OHCI_Int_MasterEnable));
		OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear,
		    intmask & ~OHCI_Int_BusReset & ~OHCI_Int_MasterEnable);
		OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
		    intmask & ~OHCI_Int_BusReset);
#endif
#ifdef	FWOHCI_DEBUG
		if (fwohcidebug > 1)
			fwohci_show_intr(sc, intmask);
#endif	/* FWOHCI_DEBUG */

		if (intmask & OHCI_Int_BusReset) {
			/*
			 * According to OHCI spec 6.1.1 "busReset",
			 * all asynchronous transmit must be stopped before
			 * clearing BusReset. Moreover, the BusReset
			 * interrupt bit should not be cleared during the
			 * SelfID phase. Thus we turned off interrupt mask
			 * bit of BusReset instead until SelfID completion
			 * or SelfID timeout.
			 */
#if 0
			DPRINTFN(2,("%s: IntMaskSet(0x%08x)"
			    " IntMaskClear(0x%08x)\n", __func__,
			    intmask & ~OHCI_Int_BusReset, OHCI_Int_BusReset));
			OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet,
			    intmask & ~OHCI_Int_BusReset);
#endif
			intmask &= OHCI_Int_SelfIDComplete;
			OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear,
			    OHCI_Int_BusReset);
			sc->sc_intmask = OHCI_Int_BusReset;
		}
		sc->sc_intmask |= intmask;

		if (intmask & OHCI_Int_IsochTx) {
			iso = OHCI_CSR_READ(sc, OHCI_REG_IsoXmitIntEventClear);
#if 1
			OHCI_CSR_WRITE(sc, OHCI_REG_IsoXmitIntEventClear, iso);
#else
			OHCI_CSR_WRITE(sc, OHCI_REG_IsoXmitIntEventClear,
			    sc->sc_isotxrst);
#endif
		}
		if (intmask & OHCI_Int_IsochRx) {
#if	NO_THREAD
			int i;
			int asyncstream = 0;
#endif	/* NO_THREAD */

			iso = OHCI_CSR_READ(sc, OHCI_REG_IsoRecvIntEventClear);
#if 1
			OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntEventClear, iso);
#else
			OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntEventClear,
			    sc->sc_isorxrst);
#endif
#if	NO_THREAD
			for (i = 0; i < sc->sc_isoctx; i++) {
				if ((iso & (1<<i)) &&
				    sc->sc_ctx_ir[i] != NULL) {
					if (sc->sc_ctx_ir[i]->fc_type ==
					    FWOHCI_CTX_ISO_SINGLE) {
						asyncstream |= (1 << i);
						continue;
					}
					bus_dmamap_sync(sc->sc_dmat,
					    sc->sc_ddmamap,
					    0, sizeof(struct fwohci_desc) *
					    sc->sc_descsize,
					    BUS_DMASYNC_PREREAD);
					sc->sc_isocnt.ev_count++;

					fwohci_ir_input(sc, sc->sc_ctx_ir[i]);
				}
			}
			if (asyncstream != 0) {
				sc->sc_iso |= asyncstream;
			} else {
				/* All iso intr is pure isochronous. */
				sc->sc_intmask &= ~OHCI_Int_IsochRx;
			}
#else	/* NO_THREAD */
			sc->sc_iso |= iso;
#endif	/* NO_THREAD */
		}

		if (!progress) {
			sc->sc_intrcnt.ev_count++;
#if 1
			progress = 1;
#endif
		}
#if 1
	}
#else
#ifdef	FWOHCI_DEBUG
	--fwintr;
	//DPRINTF(("%s: out(%d)\n", __func__, fwintr));
#endif	/* FWOHCI_DEBUG */

	return (progress);
#endif
}

void
fwohci_create_event_thread(void *arg)
{
	struct fwohci_softc  *sc = arg;

#ifdef	__NetBSD__
	if (kthread_create1(fwohci_thread_init, sc, &sc->sc_event_thread, "%s",
	    sc->sc_sc1394.sc1394_dev.dv_xname))
#else
	if (kthread_create(fwohci_thread_init, sc, &sc->sc_event_thread, "%s",
	    sc->sc_sc1394.sc1394_dev.dv_xname))
#endif
	{
		printf("%s: unable to create event thread\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		panic("fwohci_create_event_thread");
	}
}

void
fwohci_thread_init(void *arg)
{
	struct fwohci_softc *sc = arg;
	int i;

	/*
	 * Allocate descriptors.
	 */
	if (fwohci_desc_alloc(sc)) {
		printf("%s: not enabling interrupts\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		kthread_exit(1);
	}

	/*
	 * Enable Link Power.
	 */

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_LPS);

	/*
	 * Allocate DMA Context.
	 */
	fwohci_ctx_alloc(sc, &sc->sc_ctx_arrq, OHCI_BUF_ARRQ_CNT,
	    OHCI_CTX_ASYNC_RX_REQUEST, FWOHCI_CTX_ASYNC);
	fwohci_ctx_alloc(sc, &sc->sc_ctx_arrs, OHCI_BUF_ARRS_CNT,
	    OHCI_CTX_ASYNC_RX_RESPONSE, FWOHCI_CTX_ASYNC);
	fwohci_ctx_alloc(sc, &sc->sc_ctx_atrq, 0,
	    OHCI_CTX_ASYNC_TX_REQUEST, FWOHCI_CTX_ASYNC);
	fwohci_ctx_alloc(sc, &sc->sc_ctx_atrs, 0,
	    OHCI_CTX_ASYNC_TX_RESPONSE, FWOHCI_CTX_ASYNC);
	sc->sc_ctx_ir = malloc(sizeof(sc->sc_ctx_ir[0]) * sc->sc_isoctx,
	    M_DEVBUF, M_WAITOK);
	MPRINTF("malloc(DEVBUF)", sc->sc_ctx_ir);
	for (i = 0; i < sc->sc_isoctx; i++)
		sc->sc_ctx_ir[i] = NULL;

	/*
	 * Allocate buffer for configuration ROM and SelfID buffer.
	 */
	fwohci_buf_alloc(sc, &sc->sc_buf_cnfrom);
	fwohci_buf_alloc(sc, &sc->sc_buf_selfid);

#ifdef	__NetBSD__
	callout_init(&sc->sc_selfid_callout);
#else
	bzero(&sc->sc_selfid_callout, sizeof(sc->sc_selfid_callout));
#endif

	sc->sc_sc1394.sc1394_ifinreg = fwohci_if_inreg;
	sc->sc_sc1394.sc1394_ifoutput = fwohci_if_output;
	sc->sc_sc1394.sc1394_ifsetiso = fwohci_if_setiso;

	/*
	 * Establish hooks for shutdown and suspend/resume.
	 */
	sc->sc_shutdownhook = shutdownhook_establish(fwohci_shutdown, sc);
	sc->sc_powerhook = powerhook_establish(fwohci_power, sc);

	sc->sc_sc1394.sc1394_if = config_found(&sc->sc_sc1394.sc1394_dev, "fw",
	    fwohci_print);

	/* Main loop. It's not coming back normally. */

	fwohci_event_thread(sc);
	printf("%s: event thread exited\n", __func__);

	if (sc->sc_uidtbl != NULL) {
		free(sc->sc_uidtbl, M_DEVBUF);
		MPRINTF("free(DEVBUF)", sc->sc_uidtbl);
		sc->sc_uidtbl = NULL;
	}

	fwohci_buf_free(sc, &sc->sc_buf_selfid);
	fwohci_buf_free(sc, &sc->sc_buf_cnfrom);

	free(sc->sc_ctx_ir, M_DEVBUF);
	MPRINTF("free(DEVBUF)", sc->sc_ctx_ir);
	sc->sc_ctx_ir = NULL;	/* XXX */
	fwohci_ctx_free(sc, sc->sc_ctx_atrs);
	fwohci_ctx_free(sc, sc->sc_ctx_atrq);
	fwohci_ctx_free(sc, sc->sc_ctx_arrs);
	fwohci_ctx_free(sc, sc->sc_ctx_arrq);

	fwohci_desc_free(sc);

	DPRINTF(("%s: waking up... 0x%08x\n", __func__,
	    (u_int32_t)sc->sc_dying));
	wakeup(sc->sc_dying);
	kthread_exit(0);
}

void
fwohci_event_thread(struct fwohci_softc *sc)
{
	int s;
#if !	NO_THREAD
	int i;
	uint32_t intmask, iso;
#endif	/* NO_THREAD */

	s = splbio();

	/*
	 * Initialize hardware registers.
	 */

	fwohci_hw_init(sc);

	/* Initial Bus Reset. */
	fwohci_phy_busreset(sc);
	splx(s);

	while (! *sc->sc_dying) {
#if !	NO_THREAD
		s = splbio();
		intmask = sc->sc_intmask;
		if (intmask == 0) {
#endif	/* NO_THREAD */
#if 1
			tsleep(fwohci_event_thread, PZERO, "fwohciev", 8);
#else
			tsleep(fwohci_event_thread, PZERO, "fwohciev", 0);
#endif
#if !	NO_THREAD
			splx(s);
			continue;
		}
		sc->sc_intmask = 0;
		splx(s);
		DPRINTFN(2, ("%s: treating interrupts 0x%08x\n", __func__,
		    intmask));

		if (intmask & OHCI_Int_BusReset) {
//			s = splbio();
			fwohci_buf_stop_tx(sc);
//			splx(s);
			if (sc->sc_uidtbl != NULL) {
				free(sc->sc_uidtbl, M_DEVBUF);
				MPRINTF("free(DEVBUF)", sc->sc_uidtbl);
				sc->sc_uidtbl = NULL;
			}

#ifdef	__NetBSD__
			callout_reset(&sc->sc_selfid_callout,
			    OHCI_SELFID_TIMEOUT,
			    (void (*)(void *))fwohci_phy_busreset, sc);
#else
			timeout_set(&sc->sc_selfid_callout,
			    (void (*)(void *))fwohci_phy_busreset, sc);
			timeout_add(&sc->sc_selfid_callout,
			    OHCI_SELFID_TIMEOUT);
#endif
			sc->sc_nodeid = 0xFFFF;	/* Indicate invalid. */
			sc->sc_rootid = 0;
			sc->sc_irmid = IEEE1394_BCAST_PHY_ID;
		}
		if (intmask & OHCI_Int_SelfIDComplete) {
			s = splbio();
			OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
			    OHCI_Int_BusReset);
			OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet,
			    OHCI_Int_BusReset);
			splx(s);
#ifdef	__NetBSD__
			callout_stop(&sc->sc_selfid_callout);
#else
			timeout_del(&sc->sc_selfid_callout);
#endif
			if (fwohci_selfid_input(sc) == 0) {
//				s = splbio();
				fwohci_buf_start_rx(sc);
//				splx(s);
				fwohci_uid_collect(sc);
			}
		}
		if (intmask & OHCI_Int_ReqTxComplete) {
//			s = splbio();
			fwohci_at_done(sc, sc->sc_ctx_atrq, 0);
//			splx(s);
		}
		if (intmask & OHCI_Int_RespTxComplete) {
//			s = splbio();
			fwohci_at_done(sc, sc->sc_ctx_atrs, 0);
//			splx(s);
		}
		if (intmask & OHCI_Int_RQPkt) {
			fwohci_arrq_input(sc, sc->sc_ctx_arrq);
		}
		if (intmask & OHCI_Int_RSPkt) {
			fwohci_arrs_input(sc, sc->sc_ctx_arrs);
		}
		if (intmask & OHCI_Int_IsochRx) {
			s = splbio();
			iso = sc->sc_iso;
			sc->sc_iso = 0;
			splx(s);
			for (i = 0; i < sc->sc_isoctx; i++) {
				if ((iso & (1 << i)) &&
				    sc->sc_ctx_ir[i] != NULL) {
					fwohci_ir_input(sc, sc->sc_ctx_ir[i]);
					sc->sc_isocnt.ev_count++;
				}
			}
		}
#if 0
		DPRINTF(("%s: IntMaskSet(0x%08x)\n",
		    __func__, intmask & ~OHCI_Int_BusReset));
		s = splbio();
//		OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
//		    intmask & ~OHCI_Int_BusReset);
		OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet,
		    intmask & ~OHCI_Int_BusReset);
		splx(s);
#endif
#endif	/* NO_THREAD */
	}
}

#if	NO_THREAD
void
fwohci_event_dispatch(struct fwohci_softc *sc)
{
	int i, s;
	u_int32_t intmask, iso;

	splassert(IPL_BIO);
	intmask = sc->sc_intmask;
	if (intmask == 0)
		return;

	sc->sc_intmask = 0;
	s = spl0();
	DPRINTFN(2, ("%s: treating interrupts 0x%08x\n", __func__, intmask));

	if (intmask & OHCI_Int_BusReset) {
//		s = splbio();
		fwohci_buf_stop_tx(sc);
//		splx(s);
		if (sc->sc_uidtbl != NULL) {
			free(sc->sc_uidtbl, M_DEVBUF);
			MPRINTF("free(DEVBUF)", sc->sc_uidtbl);
			sc->sc_uidtbl = NULL;
		}

#ifdef	__NetBSD__
		callout_reset(&sc->sc_selfid_callout,
		    OHCI_SELFID_TIMEOUT,
		    (void (*)(void *))fwohci_phy_busreset, sc);
#else
		timeout_set(&sc->sc_selfid_callout,
		    (void (*)(void *))fwohci_phy_busreset, sc);
		timeout_add(&sc->sc_selfid_callout,
		    OHCI_SELFID_TIMEOUT);
#endif
		sc->sc_nodeid = 0xFFFF;	/* Indicate invalid. */
		sc->sc_rootid = 0;
		sc->sc_irmid = IEEE1394_BCAST_PHY_ID;
	}
	if (intmask & OHCI_Int_SelfIDComplete) {
		splx(s);
		OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
		    OHCI_Int_BusReset);
		OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet,
		    OHCI_Int_BusReset);
		s = spl0();
#ifdef	__NetBSD__
		callout_stop(&sc->sc_selfid_callout);
#else
		timeout_del(&sc->sc_selfid_callout);
#endif
		if (fwohci_selfid_input(sc) == 0) {
//			s = splbio();
			fwohci_buf_start_rx(sc);
//			splx(s);
			fwohci_uid_collect(sc);
		}
	}
	if (intmask & OHCI_Int_ReqTxComplete) {
//		s = splbio();
		fwohci_at_done(sc, sc->sc_ctx_atrq, 0);
//		splx(s);
	}
	if (intmask & OHCI_Int_RespTxComplete) {
//		s = splbio();
		fwohci_at_done(sc, sc->sc_ctx_atrs, 0);
//		splx(s);
	}
	if (intmask & OHCI_Int_RQPkt) {
		fwohci_arrq_input(sc, sc->sc_ctx_arrq);
	}
	if (intmask & OHCI_Int_RSPkt) {
		fwohci_arrs_input(sc, sc->sc_ctx_arrs);
	}
	if (intmask & OHCI_Int_IsochRx) {
		splx(s);
		iso = sc->sc_iso;
		sc->sc_iso = 0;
		s = spl0();
		for (i = 0; i < sc->sc_isoctx; i++) {
			if ((iso & (1 << i)) &&
			    sc->sc_ctx_ir[i] != NULL) {
				fwohci_ir_input(sc, sc->sc_ctx_ir[i]);
				sc->sc_isocnt.ev_count++;
			}
		}
	}
#if 0
	DPRINTF(("%s: IntMaskSet(0x%08x)\n",
	    __func__, intmask & ~OHCI_Int_BusReset));
	s = splbio();
//	OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
//	    intmask & ~OHCI_Int_BusReset);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, intmask & ~OHCI_Int_BusReset);
	splx(s);
#endif
	splx(s);
}
#endif	/* NO_THREAD */


#if 0
int
fwohci_dmamem_alloc(struct fwohci_softc *sc, int size, int alignment,
    bus_dmamap_t *mapp, caddr_t *kvap, int flags)
{
	bus_dma_segment_t segs[1];
	int error, nsegs, steps;

	steps = 0;
	error = bus_dmamem_alloc(sc->sc_dmat, size, alignment, alignment,
	    segs, 1, &nsegs, flags);
	if (error)
		goto cleanup;
	MPRINTF("bus_dmamem_alloc", segs->ds_addr);

	steps = 1;
	error = bus_dmamem_map(sc->sc_dmat, segs, nsegs, segs[0].ds_len,
	    kvap, flags);
	if (error)
		goto cleanup;

	if (error == 0)
		error = bus_dmamap_create(sc->sc_dmat, size, 1, alignment,
		    size, flags, mapp);
	if (error)
		goto cleanup;
	MPRINTF("bus_dmamap_create", mapp);

	if (error == 0)
		error = bus_dmamap_load(sc->sc_dmat, *mapp, *kvap, size, NULL,
		    flags);
	if (error)
		goto cleanup;

 cleanup:
	switch (steps) {
	case 1:
		bus_dmamem_free(sc->sc_dmat, segs, nsegs);
		MPRINTF("bus_dmamem_free", segs->ds_addr);
	}

	return error;
}
#endif

int
fwohci_print(void *aux, const char *pnp)
{
	char *name = aux;

	if (pnp)
		printf("%s at %s", name, pnp);

	return UNCONF;
}

void
fwohci_hw_init(struct fwohci_softc *sc)
{
	int i;
	u_int32_t val;

	splassert(IPL_BIO);

	/*
	 * Software Reset.
	 */
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_SoftReset);
	for (i = 0; i < OHCI_LOOP; i++) {
		val = OHCI_CSR_READ(sc, OHCI_REG_HCControlClear);
		if ((val & OHCI_HCControl_SoftReset) == 0)
			break;
		DELAY(10);
	}

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_LPS);
	DELAY(100000);

	/*
	 * First, initialize CSRs with undefined value to default settings.
	 */
	val = OHCI_CSR_READ(sc, OHCI_REG_BusOptions);
	val |= OHCI_BusOptions_ISC | OHCI_BusOptions_CMC;
#if 1
	val |= OHCI_BusOptions_BMC | OHCI_BusOptions_IRMC;
	val |= OHCI_BusOptions_PMC;
#else
	val &= ~(OHCI_BusOptions_BMC | OHCI_BusOptions_IRMC);
	val &= ~(OHCI_BusOptions_PMC);
#endif
	OHCI_CSR_WRITE(sc, OHCI_REG_BusOptions, val);
	for (i = 0; i < sc->sc_isoctx; i++) {
		OHCI_SYNC_RX_DMA_WRITE(sc, i, OHCI_SUBREG_ContextControlClear,
		    ~0);
	}
	OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlClear, ~0);

	fwohci_configrom_init(sc);
	fwohci_selfid_init(sc);
	fwohci_buf_init_rx(sc);
	fwohci_csr_init(sc);

	/*
	 * Final CSR settings.
	 */
	OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlSet,
	    OHCI_LinkControl_CycleTimerEnable |
	    OHCI_LinkControl_RcvSelfID | OHCI_LinkControl_RcvPhyPkt);

#if 0
	OHCI_CSR_WRITE(sc, OHCI_REG_ATRetries, 0x00000888);	/*XXX*/
#else
	OHCI_CSR_WRITE(sc, OHCI_REG_ATRetries, 0xFFFF0FFF);	/*XXX*/
#endif

	/* Clear receive filter. */
	OHCI_CSR_WRITE(sc, OHCI_REG_IRMultiChanMaskHiClear, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IRMultiChanMaskLoClear, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_AsynchronousRequestFilterHiSet, 0x80000000);

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet,
	    OHCI_HCControl_ProgramPhyEnable);
#if 0
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear,
	    OHCI_HCControl_APhyEnhanceEnable);
#else
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet,
	    OHCI_HCControl_APhyEnhanceEnable);
#endif
#if BYTE_ORDER == BIG_ENDIAN
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet,
	    OHCI_HCControl_NoByteSwapData);
#else
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear,
	    OHCI_HCControl_NoByteSwapData);
#endif

	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_BusReset |
	    OHCI_Int_SelfIDComplete | OHCI_Int_IsochRx | OHCI_Int_IsochTx |
	    OHCI_Int_RSPkt | OHCI_Int_RQPkt | OHCI_Int_ARRS | OHCI_Int_ARRQ |
	    OHCI_Int_RespTxComplete | OHCI_Int_ReqTxComplete);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_CycleTooLong |
	    OHCI_Int_UnrecoverableError | OHCI_Int_CycleInconsistent |
	    OHCI_Int_LockRespErr | OHCI_Int_PostedWriteErr);
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoXmitIntMaskSet, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntMaskSet, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_MasterEnable);

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_LinkEnable);

	/*
	 * Start the receivers.
	 */
	fwohci_buf_start_rx(sc);
}

void
fwohci_power(int why, void *arg)
{
	struct fwohci_softc *sc = arg;
	int s;

	s = splbio();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		fwohci_shutdown(sc);
		break;
	case PWR_RESUME:
		fwohci_hw_init(sc);
		fwohci_phy_busreset(sc);
		break;
#ifdef	__NetBSD__
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
#endif
	}
	splx(s);
}

void
fwohci_shutdown(void *arg)
{
	struct fwohci_softc *sc = arg;
	u_int32_t val;
	int s;

	//splassert(IPL_BIO);

#ifdef	__NetBSD__
	callout_stop(&sc->sc_selfid_callout);
#else
	timeout_del(&sc->sc_selfid_callout);
#endif
	/* Disable all interrupt. */
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear, OHCI_Int_MasterEnable);
	fwohci_buf_stop_tx(sc);
	fwohci_buf_stop_rx(sc);
	s = splbio();
	val = OHCI_CSR_READ(sc, OHCI_REG_BusOptions);
	val &= ~(OHCI_BusOptions_BMC | OHCI_BusOptions_ISC |
		OHCI_BusOptions_CMC | OHCI_BusOptions_IRMC);
	OHCI_CSR_WRITE(sc, OHCI_REG_BusOptions, val);
	splx(s);
	fwohci_phy_busreset(sc);
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear, OHCI_HCControl_LinkEnable);
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear, OHCI_HCControl_LPS);
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_SoftReset);
}

/*
 * COMMON FUNCTIONS.
 */

/*
 * Read the PHY Register.
 */
u_int8_t
fwohci_phy_read(struct fwohci_softc *sc, u_int8_t reg)
{
	int i;
	u_int32_t val;

	//splassert(IPL_BIO);

	OHCI_CSR_WRITE(sc, OHCI_REG_PhyControl,
	    OHCI_PhyControl_RdReg |
	    OHCI_BITSET(reg, OHCI_PhyControl_RegAddr));
	for (i = 0; i < OHCI_LOOP; i++) {
		val = OHCI_CSR_READ(sc, OHCI_REG_PhyControl);
		if (!(val & OHCI_PhyControl_RdReg) &&
		    (val & OHCI_PhyControl_RdDone))
			break;
		DELAY(10);
	}
	return (OHCI_BITVAL(val, OHCI_PhyControl_RdData));
}

/*
 * Write the PHY Register.
 */
void
fwohci_phy_write(struct fwohci_softc *sc, u_int8_t reg, u_int8_t val)
{
	int i;

	//splassert(IPL_BIO);

	OHCI_CSR_WRITE(sc, OHCI_REG_PhyControl, OHCI_PhyControl_WrReg |
	    OHCI_BITSET(reg, OHCI_PhyControl_RegAddr) |
	    OHCI_BITSET(val, OHCI_PhyControl_WrData));
	for (i = 0; i < OHCI_LOOP; i++) {
		if (!(OHCI_CSR_READ(sc, OHCI_REG_PhyControl) &
		    OHCI_PhyControl_WrReg))
			break;
		DELAY(10);
	}
}

/*
 * Initiate Bus Reset.
 */
void
fwohci_phy_busreset(struct fwohci_softc *sc)
{
	int s;
	u_int8_t val;

	//splassert(IPL_BIO);

	s = splbio();
	OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
	    OHCI_Int_BusReset | OHCI_Int_SelfIDComplete);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_BusReset);
#ifdef	__NetBSD__
	callout_stop(&sc->sc_selfid_callout);
#else
	timeout_del(&sc->sc_selfid_callout);
#endif
	val = fwohci_phy_read(sc, 1);
	val = (val & 0x80) |			/* Preserve RHB (force root). */
	    0x40 |				/* Initiate Bus Reset. */
	    0x3F;				/* Default GAP count. */
	fwohci_phy_write(sc, 1, val);
	splx(s);
}

/*
 * PHY Packet.
 */
void
fwohci_phy_input(struct fwohci_softc *sc, struct fwohci_pkt *pkt)
{
	u_int32_t val;

	val = pkt->fp_hdr[1];
	if (val != ~pkt->fp_hdr[2]) {
		if (val == 0 && ((*pkt->fp_trail & 0x001F0000) >> 16) ==
		    OHCI_CTXCTL_EVENT_BUS_RESET) {
			DPRINTFN(1, ("%s: BusReset: 0x%08x\n", __func__,
			    pkt->fp_hdr[2]));
		} else {
			printf("%s: phy packet corrupted (0x%08x, 0x%08x)\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname, val,
			    pkt->fp_hdr[2]);
		}
		return;
	}
#ifdef	FWOHCI_DEBUG
	if (fwohcidebug > 1)
		fwohci_show_phypkt(sc, val);
#endif	/* FWOHCI_DEBUG */
}

/*
 * Descriptor for context DMA.
 */
int
fwohci_desc_alloc(struct fwohci_softc *sc)
{
	int error, mapsize, dsize;

	/*
	 * Allocate descriptor buffer.
	 */

	sc->sc_descsize = OHCI_BUF_ARRQ_CNT + OHCI_BUF_ARRS_CNT +
	    OHCI_BUF_ATRQ_CNT + OHCI_BUF_ATRS_CNT +
	    OHCI_BUF_IR_CNT * sc->sc_isoctx + 2;
	dsize = sizeof(struct fwohci_desc) * sc->sc_descsize;
	mapsize = howmany(sc->sc_descsize, NBBY);
#ifdef	M_ZERO
	sc->sc_descmap = malloc(mapsize, M_DEVBUF, M_WAITOK|M_ZERO);
	MPRINTF("malloc(DEVBUF)", sc->sc_descmap);
#else
	sc->sc_descmap = malloc(mapsize, M_DEVBUF, M_WAITOK);
	MPRINTF("malloc(DEVBUF)", sc->sc_descmap);
	bzero(sc->sc_descmap, mapsize);
#endif

#if 1	/* XXX Added when reorganizing dmamap stuff... */
	sc->sc_dnseg = 1;
#endif

	if ((error = bus_dmamap_create(sc->sc_dmat, dsize, sc->sc_dnseg,
	    dsize, 0, BUS_DMA_WAITOK, &sc->sc_ddmamap)) != 0) {
		printf("%s: unable to create descriptor buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_0;
	}
	MPRINTF("bus_dmamap_create", sc->sc_ddmamap);

	if ((error = bus_dmamem_alloc(sc->sc_dmat, dsize, PAGE_SIZE, 0,
	    &sc->sc_dseg, 1, &sc->sc_dnseg, BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to allocate descriptor buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_1;
	}
	MPRINTF("bus_dmamem_alloc", sc->sc_dseg.ds_addr);

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg,
	    dsize, (caddr_t *)&sc->sc_desc, BUS_DMA_COHERENT | BUS_DMA_WAITOK))
	    != 0) {
		printf("%s: unable to map descriptor buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_2;
	}

#if 0
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_ddmamap, sc->sc_desc,
	    dsize, NULL, BUS_DMA_WAITOK)) != 0) {
#else
	if ((error = bus_dmamap_load_raw(sc->sc_dmat, sc->sc_ddmamap,
	    &sc->sc_dseg, sc->sc_dnseg, dsize, BUS_DMA_WAITOK)) != 0) {
#endif
		printf("%s: unable to load descriptor buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_3;
	}

	return 0;

  fail_3:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_desc, dsize);
  fail_2:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg);
	MPRINTF("bus_dmamem_free", sc->sc_dseg.ds_addr);
  fail_1:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
	MPRINTF("bus_dmamap_destroy", sc->sc_ddmamap);
  fail_0:
	return error;
}

void
fwohci_desc_free(struct fwohci_softc *sc)
{
	int dsize = sizeof(struct fwohci_desc) * sc->sc_descsize;

	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_desc, dsize);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg);
	MPRINTF("bus_dmamem_free", sc->sc_dseg.ds_addr);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
	MPRINTF("bus_dmamap_destroy", sc->sc_ddmamap);

	free(sc->sc_descmap, M_DEVBUF);
	MPRINTF("free(DEVBUF)", sc->sc_descmap);
	sc->sc_descmap = NULL;	/* XXX */
}

struct fwohci_desc *
fwohci_desc_get(struct fwohci_softc *sc, int ndesc)
{
	int i, n;

	assert(ndesc > 0);

	for (n = 0; n <= sc->sc_descsize - ndesc; n++) {
		for (i = 0; ; i++) {
			if (i == ndesc) {
				for (i = 0; i < ndesc; i++)
					setbit(sc->sc_descmap, n + i);
				return sc->sc_desc + n;
			}
			if (isset(sc->sc_descmap, n + i))
				break;
		}
	}
	return NULL;
}

void
fwohci_desc_put(struct fwohci_softc *sc, struct fwohci_desc *fd, int ndesc)
{
	int i, n;

	assert(ndesc > 0);

	n = fd - sc->sc_desc;
	for (i = 0; i < ndesc; i++, n++) {
#ifdef	DIAGNOSTIC
		if (isclr(sc->sc_descmap, n))
			panic("fwohci_desc_put: duplicated free");
#endif
		clrbit(sc->sc_descmap, n);
	}
}

/*
 * Asynchronous/Isochronous Transmit/Receive Context.
 */
int
fwohci_ctx_alloc(struct fwohci_softc *sc, struct fwohci_ctx **fcp,
    int bufcnt, int ctx, int ctxtype)
{
	int i, error;
	struct fwohci_ctx *fc;
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
#if DOUBLEBUF
	int buf2cnt;
#endif

#ifdef	M_ZERO
	MALLOC(fc, struct fwohci_ctx *, sizeof(*fc), M_DEVBUF, M_WAITOK|M_ZERO);
	MPRINTF("MALLOC(DEVBUF)", fc);
#else
	MALLOC(fc, struct fwohci_ctx *, sizeof(*fc), M_DEVBUF, M_WAITOK);
	MPRINTF("MALLOC(DEVBUF)", fc);
	bzero(fc, sizeof(*fc));
#endif
	LIST_INIT(&fc->fc_handler);
	TAILQ_INIT(&fc->fc_buf);
	fc->fc_ctx = ctx;
#ifdef	M_ZERO
	fc->fc_buffers = fb = malloc(sizeof(*fb) * bufcnt,
	    M_DEVBUF, M_WAITOK|M_ZERO);
	MPRINTF("malloc(DEVBUF)", fc->fc_buffers);
#else
	fc->fc_buffers = fb = malloc(sizeof(*fb) * bufcnt, M_DEVBUF, M_WAITOK);
	MPRINTF("malloc(DEVBUF)", fc->fc_buffers);
	bzero(fb, sizeof(*fb) * bufcnt);
#endif
	fc->fc_bufcnt = bufcnt;
	if (bufcnt == 0)		/* Asynchronous transmit... */
		goto ok;

#if DOUBLEBUF
	TAILQ_INIT(&fc->fc_buf2);	/* For isochronous. */
	if (ctxtype == FWOHCI_CTX_ISO_MULTI) {
		buf2cnt = bufcnt/2;
		bufcnt -= buf2cnt;
		if (buf2cnt == 0) {
			panic("cannot allocate iso buffer");
		}
	}
#endif
	for (i = 0; i < bufcnt; i++, fb++) {
		if ((error = fwohci_buf_alloc(sc, fb)) != 0)
			goto fail;
		if ((fd = fwohci_desc_get(sc, 1)) == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		fb->fb_desc = fd;
		fb->fb_daddr = sc->sc_ddmamap->dm_segs[0].ds_addr +
		    ((caddr_t)fd - (caddr_t)sc->sc_desc);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc,
		    sizeof(struct fwohci_desc), BUS_DMASYNC_PREWRITE);
		fd->fd_flags = OHCI_DESC_INPUT | OHCI_DESC_STATUS |
		    OHCI_DESC_INTR_ALWAYS | OHCI_DESC_BRANCH;
		fd->fd_reqcount = fb->fb_dmamap->dm_segs[0].ds_len;
		fd->fd_data = fb->fb_dmamap->dm_segs[0].ds_addr;
		TAILQ_INSERT_TAIL(&fc->fc_buf, fb, fb_list);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc,
		    sizeof(struct fwohci_desc), BUS_DMASYNC_POSTWRITE);
	}
#if DOUBLEBUF
	if (ctxtype == FWOHCI_CTX_ISO_MULTI) {
		for (i = bufcnt; i < bufcnt + buf2cnt; i++, fb++) {
			if ((error = fwohci_buf_alloc(sc, fb)) != 0)
				goto fail;
			if ((fd = fwohci_desc_get(sc, 1)) == NULL) {
				error = ENOBUFS;
				goto fail;
			}
			fb->fb_desc = fd;
			fb->fb_daddr = sc->sc_ddmamap->dm_segs[0].ds_addr +
			    ((caddr_t)fd - (caddr_t)sc->sc_desc);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
			    (caddr_t)fd - (caddr_t)sc->sc_desc,
			    sizeof(struct fwohci_desc), BUS_DMASYNC_PREWRITE);
			fd->fd_flags = OHCI_DESC_INPUT | OHCI_DESC_STATUS |
			    OHCI_DESC_INTR_ALWAYS | OHCI_DESC_BRANCH;
			fd->fd_reqcount = fb->fb_dmamap->dm_segs[0].ds_len;
			fd->fd_data = fb->fb_dmamap->dm_segs[0].ds_addr;
			TAILQ_INSERT_TAIL(&fc->fc_buf2, fb, fb_list);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
			    (caddr_t)fd - (caddr_t)sc->sc_desc,
			    sizeof(struct fwohci_desc), BUS_DMASYNC_POSTWRITE);
		}
	}
#endif /* DOUBLEBUF */
    ok:
	fc->fc_type = ctxtype;
	*fcp = fc;
	return 0;

  fail:
	while (i-- > 0) {
		fb--;
		if (fb->fb_desc)
			fwohci_desc_put(sc, fb->fb_desc, 1);
		fwohci_buf_free(sc, fb);
	}
	FREE(fc, M_DEVBUF);
	MPRINTF("FREE(DEVBUF)", fc);
	fc = NULL;	/* XXX */
	return error;
}

void
fwohci_ctx_free(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_buf *fb;
	struct fwohci_handler *fh;

#if DOUBLEBUF
	if ((fc->fc_type == FWOHCI_CTX_ISO_MULTI) &&
	    (TAILQ_FIRST(&fc->fc_buf) > TAILQ_FIRST(&fc->fc_buf2))) {
		struct fwohci_buf_s fctmp;

		fctmp = fc->fc_buf;
		fc->fc_buf = fc->fc_buf2;
		fc->fc_buf2 = fctmp;
	}
#endif
	while ((fh = LIST_FIRST(&fc->fc_handler)) != NULL)
		fwohci_handler_set(sc, fh->fh_tcode, fh->fh_key1, fh->fh_key2,
		    fh->fh_key3, NULL, NULL);
	while ((fb = TAILQ_FIRST(&fc->fc_buf)) != NULL) {
		TAILQ_REMOVE(&fc->fc_buf, fb, fb_list);
		if (fb->fb_desc)
			fwohci_desc_put(sc, fb->fb_desc, 1);
		fwohci_buf_free(sc, fb);
	}
#if	DOUBLEBUF
	while ((fb = TAILQ_FIRST(&fc->fc_buf2)) != NULL) {
		TAILQ_REMOVE(&fc->fc_buf2, fb, fb_list);
		if (fb->fb_desc)
			fwohci_desc_put(sc, fb->fb_desc, 1);
		fwohci_buf_free(sc, fb);
	}
#endif	/* DOUBLEBUF */
	free(fc->fc_buffers, M_DEVBUF);
	MPRINTF("free(DEVBUF)", fc->fc_buffers);
	fc->fc_buffers = NULL;	/* XXX */
	FREE(fc, M_DEVBUF);
	MPRINTF("FREE(DEVBUF)", fc);
	fc = NULL;	/* XXX */
}

void
fwohci_ctx_init(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_buf *fb, *nfb;
	struct fwohci_desc *fd;
	struct fwohci_handler *fh;
	int n;

	//splassert(IPL_BIO);

	TAILQ_FOREACH(fb, &fc->fc_buf, fb_list) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc,
		    sizeof(struct fwohci_desc), BUS_DMASYNC_PREWRITE);
		nfb = TAILQ_NEXT(fb, fb_list);
		fb->fb_off = 0;
		fd = fb->fb_desc;
		fd->fd_branch = (nfb != NULL) ? (nfb->fb_daddr | 1) : 0;
		fd->fd_rescount = fd->fd_reqcount;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc,
		    sizeof(struct fwohci_desc), BUS_DMASYNC_POSTWRITE);
	}

#if DOUBLEBUF
	TAILQ_FOREACH(fb, &fc->fc_buf2, fb_list) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc,
		    sizeof(struct fwohci_desc), BUS_DMASYNC_PREWRITE);
		nfb = TAILQ_NEXT(fb, fb_list);
		fb->fb_off = 0;
		fd = fb->fb_desc;
		fd->fd_branch = (nfb != NULL) ? (nfb->fb_daddr | 1) : 0;
		fd->fd_rescount = fd->fd_reqcount;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc,
		    sizeof(struct fwohci_desc), BUS_DMASYNC_POSTWRITE);
	}
#endif /* DOUBLEBUF */

	n = fc->fc_ctx;
	fb = TAILQ_FIRST(&fc->fc_buf);
	if (fc->fc_type != FWOHCI_CTX_ASYNC) {
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_CommandPtr,
		    fb->fb_daddr | 1);
		MPRINTF("OHCI_SUBREG_CommandPtr(SYNC_RX)", fb->fb_daddr);
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_ContextControlClear,
		    OHCI_CTXCTL_RX_BUFFER_FILL |
		    OHCI_CTXCTL_RX_CYCLE_MATCH_ENABLE |
		    OHCI_CTXCTL_RX_MULTI_CHAN_MODE |
		    OHCI_CTXCTL_RX_DUAL_BUFFER_MODE);
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_ContextControlSet,
		    OHCI_CTXCTL_RX_ISOCH_HEADER);
		if (fc->fc_type == FWOHCI_CTX_ISO_MULTI) {
			OHCI_SYNC_RX_DMA_WRITE(sc, n,
			    OHCI_SUBREG_ContextControlSet,
			    OHCI_CTXCTL_RX_BUFFER_FILL);
		}
		fh = LIST_FIRST(&fc->fc_handler);
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_ContextMatch,
		    (OHCI_CTXMATCH_TAG0 << fh->fh_key2) | fh->fh_key1);
	} else {
		OHCI_ASYNC_DMA_WRITE(sc, n, OHCI_SUBREG_CommandPtr,
		    fb->fb_daddr | 1);
		MPRINTF("OHCI_SUBREG_CommandPtr(ASYNC)", fb->fb_daddr);
	}
}

/*
 * DMA data buffer.
 */
int
fwohci_buf_alloc(struct fwohci_softc *sc, struct fwohci_buf *fb)
{
	int error;

	if (!fb->fb_nseg)
		fb->fb_nseg = 1;

	if ((error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, fb->fb_nseg,
	    PAGE_SIZE, 0, BUS_DMA_WAITOK, &fb->fb_dmamap)) != 0) {
		printf("%s: unable to create buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname,
		    error);
		goto fail_0;
	}
	MPRINTF("bus_dmamap_create", fb->fb_dmamap);

	if ((error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE,
	    PAGE_SIZE, &fb->fb_seg, 1, &fb->fb_nseg, BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to allocate buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_1;
	}
	MPRINTF("bus_dmamem_alloc", fb->fb_seg.ds_addr);

	if ((error = bus_dmamem_map(sc->sc_dmat, &fb->fb_seg,
	    fb->fb_nseg, PAGE_SIZE, &fb->fb_buf,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to map buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_2;
	}

#if 0
	if ((error = bus_dmamap_load(sc->sc_dmat, fb->fb_dmamap,
	    fb->fb_buf, PAGE_SIZE, NULL, BUS_DMA_WAITOK)) != 0) {
#else
	if ((error = bus_dmamap_load_raw(sc->sc_dmat, fb->fb_dmamap,
	    &fb->fb_seg, fb->fb_nseg, PAGE_SIZE, BUS_DMA_WAITOK)) != 0) {
#endif
		printf("%s: unable to load buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname,
		    error);
		goto fail_3;
	}

	return 0;

	bus_dmamap_unload(sc->sc_dmat, fb->fb_dmamap);
  fail_3:
	bus_dmamem_unmap(sc->sc_dmat, fb->fb_buf, PAGE_SIZE);
  fail_2:
	bus_dmamem_free(sc->sc_dmat, &fb->fb_seg, fb->fb_nseg);
	MPRINTF("bus_dmamem_free", fb->fb_seg.ds_addr);
  fail_1:
	bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
	MPRINTF("bus_dmamap_destroy", fb->fb_dmamap);
  fail_0:
	return error;
}

void
fwohci_buf_free(struct fwohci_softc *sc, struct fwohci_buf *fb)
{

	bus_dmamap_unload(sc->sc_dmat, fb->fb_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, fb->fb_buf, PAGE_SIZE);
	bus_dmamem_free(sc->sc_dmat, &fb->fb_seg, fb->fb_nseg);
	MPRINTF("bus_dmamem_free", fb->fb_seg.ds_addr);
	bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
	MPRINTF("bus_dmamap_destroy", fb->fb_dmamap);
}

void
fwohci_buf_init_rx(struct fwohci_softc *sc)
{
	int i;

	//splassert(IPL_BIO);

	/*
	 * Initialize for Asynchronous Receive Queue.
	 */
	fwohci_ctx_init(sc, sc->sc_ctx_arrq);
	fwohci_ctx_init(sc, sc->sc_ctx_arrs);

	/*
	 * Initialize for Isochronous Receive Queue.
	 */
	for (i = 0; i < sc->sc_isoctx; i++) {
		if (sc->sc_ctx_ir[i] != NULL)
			fwohci_ctx_init(sc, sc->sc_ctx_ir[i]);
	}
}

void
fwohci_buf_start_rx(struct fwohci_softc *sc)
{
	int i;

//	splassert(IPL_BIO);

	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_REQUEST,
	    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
	    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	for (i = 0; i < sc->sc_isoctx; i++) {
		if (sc->sc_ctx_ir[i] != NULL)
			OHCI_SYNC_RX_DMA_WRITE(sc, i,
			    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	}
}

void
fwohci_buf_stop_tx(struct fwohci_softc *sc)
{
	int i;

//	splassert(IPL_BIO);

	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_TX_REQUEST,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_TX_RESPONSE,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);

	/*
	 * Make sure the transmitter is stopped.
	 */
	for (i = 0; i < OHCI_LOOP; i++) {
		DELAY(10);
		if (OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_REQUEST,
		    OHCI_SUBREG_ContextControlClear) & OHCI_CTXCTL_ACTIVE)
			continue;
		if (OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear) & OHCI_CTXCTL_ACTIVE)
			continue;
		break;
	}

	/*
	 * Initialize for Asynchronous Transmit Queue.
	 */
	fwohci_at_done(sc, sc->sc_ctx_atrq, 1);
	fwohci_at_done(sc, sc->sc_ctx_atrs, 1);
}

void
fwohci_buf_stop_rx(struct fwohci_softc *sc)
{
	int i;

	//splassert(IPL_BIO);

	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_REQUEST,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	for (i = 0; i < sc->sc_isoctx; i++) {
		OHCI_SYNC_RX_DMA_WRITE(sc, i,
		    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	}
}

void
fwohci_buf_next(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_buf *fb, *tfb;

#if DOUBLEBUF
	if (fc->fc_type != FWOHCI_CTX_ISO_MULTI) {
#endif
		while ((fb = TAILQ_FIRST(&fc->fc_buf)) != NULL) {
			if (fc->fc_type != FWOHCI_CTX_ASYNC) {
				if (fb->fb_off == 0)
					break;
			} else {
				if (fb->fb_off != fb->fb_desc->fd_reqcount ||
				    fb->fb_desc->fd_rescount != 0)
					break;
			}
			TAILQ_REMOVE(&fc->fc_buf, fb, fb_list);
			fb->fb_desc->fd_rescount = fb->fb_desc->fd_reqcount;
			fb->fb_off = 0;
			fb->fb_desc->fd_branch = 0;
			tfb = TAILQ_LAST(&fc->fc_buf, fwohci_buf_s);
			tfb->fb_desc->fd_branch = fb->fb_daddr | 1;
			TAILQ_INSERT_TAIL(&fc->fc_buf, fb, fb_list);
		}
#if DOUBLEBUF
	} else {
		struct fwohci_buf_s fctmp;

		/* Cleaning buffer. */
		TAILQ_FOREACH(fb, &fc->fc_buf, fb_list) {
			fb->fb_off = 0;
			fb->fb_desc->fd_rescount = fb->fb_desc->fd_reqcount;
		}

		/* Rotating buffer. */
		fctmp = fc->fc_buf;
		fc->fc_buf = fc->fc_buf2;
		fc->fc_buf2 = fctmp;
	}
#endif
}

int
fwohci_buf_pktget(struct fwohci_softc *sc, struct fwohci_buf **fbp,
    caddr_t *pp, int reqlen)
{
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	int bufend, len = reqlen;
#ifdef	FWOHCI_DEBUG
	int i;
#endif	/* FWOHCI_DEBUG */

	fb = *fbp;
again:
	fd = fb->fb_desc;
	DPRINTFN(1, ("%s: desc %ld, off %d, req %d, res %d, len %d, avail %d",
	    __func__, (long)(fd - sc->sc_desc), fb->fb_off, fd->fd_reqcount,
	    fd->fd_rescount, len,
	    fd->fd_reqcount - fd->fd_rescount - fb->fb_off));
	bufend = fd->fd_reqcount - fd->fd_rescount;
	if (fb->fb_off >= bufend) {
		DPRINTFN(5, ("\n\tbuf %08x finish req %d res %d off %d",
		    fb->fb_desc->fd_data, fd->fd_reqcount, fd->fd_rescount,
		    fb->fb_off));
		if (fd->fd_rescount == 0) {
			*fbp = fb = TAILQ_NEXT(fb, fb_list);
			if (fb != TAILQ_END(fb))
				goto again;
		}
		return 0;
	}
	if (fb->fb_off + len > bufend)
		len = bufend - fb->fb_off;
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, fb->fb_off, len,
	    BUS_DMASYNC_POSTREAD);

#ifdef	FWOHCI_DEBUG
	for (i=0; i < (roundup(len, 4) / 4); i++) {
		if ((i % 8) == 0) DPRINTFN(5, ("\n   "));
		DPRINTFN(5, (" %08x",
		    ((u_int32_t *)(fb->fb_buf + fb->fb_off))[i]));
	}
#endif	/* FWOHCI_DEBUG */
	DPRINTF(("\n"));
	*pp = fb->fb_buf + fb->fb_off;
	fb->fb_off += roundup(len, 4);
	return len;
}

int
fwohci_buf_input(struct fwohci_softc *sc, struct fwohci_ctx *fc,
    struct fwohci_pkt *pkt)
{
	caddr_t p;
	struct fwohci_buf *fb;
	int len, count, i;

	bzero(pkt, sizeof(*pkt));
	pkt->fp_uio.uio_iov = pkt->fp_iov;
	pkt->fp_uio.uio_rw = UIO_WRITE;
	pkt->fp_uio.uio_segflg = UIO_SYSSPACE;

	/* Get first quadlet. */
	fb = TAILQ_FIRST(&fc->fc_buf);
	count = 4;
	len = fwohci_buf_pktget(sc, &fb, &p, count);
	if (len <= 0) {
		DPRINTFN(1, ("%s: no input for %d\n", __func__, fc->fc_ctx));
		return 0;
	}
	pkt->fp_hdr[0] = *(u_int32_t *)p;	/* XXX Alignment !!! */
	pkt->fp_tcode = (pkt->fp_hdr[0] & 0x000000F0) >> 4;
	switch (pkt->fp_tcode) {
	case IEEE1394_TCODE_WRITE_REQUEST_QUADLET:
	case IEEE1394_TCODE_READ_RESPONSE_QUADLET:
		pkt->fp_hlen = 12;
		pkt->fp_dlen = 4;
		break;
	case IEEE1394_TCODE_READ_REQUEST_DATABLOCK:
		pkt->fp_hlen = 16;
		pkt->fp_dlen = 0;
		break;
	case IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK:
	case IEEE1394_TCODE_READ_RESPONSE_DATABLOCK:
	case IEEE1394_TCODE_LOCK_REQUEST:
	case IEEE1394_TCODE_LOCK_RESPONSE:
		pkt->fp_hlen = 16;
		break;
	case IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK:
#ifdef	DIAGNOSTIC
		if (fc->fc_type == FWOHCI_CTX_ISO_MULTI)
#endif
		{
			pkt->fp_hlen = 4;
			pkt->fp_dlen = (pkt->fp_hdr[0] >> 16) & 0xFFFF;
			DPRINTFN(5, ("[%d]", pkt->fp_dlen));
			break;
		}
#ifdef	DIAGNOSTIC
		else {
			printf("%s: bad tcode: STREAM_DATA\n", __func__);
			return 0;
		}
#endif
	default:
		pkt->fp_hlen = 12;
		pkt->fp_dlen = 0;
		break;
	}

	/* Get header. */
	while (count < pkt->fp_hlen) {
		len = fwohci_buf_pktget(sc, &fb, &p, pkt->fp_hlen - count);
		if (len == 0) {
			printf("%s: malformed input 1: %d\n", __func__,
			    pkt->fp_hlen - count);
			return 0;
		}
#ifdef	FWOHCI_DEBUG
		fwptr = p; fwlen = len; fwbuf = fb;
#endif	/* FWOHCI_DEBUG */
		bcopy(p, (caddr_t)pkt->fp_hdr + count, len);
#ifdef	FWOHCI_DEBUG
		fwptr = NULL; fwlen = 0; fwbuf = NULL;
#endif	/* FWOHCI_DEBUG */
		count += len;
	}
	if (pkt->fp_hlen == 16 &&
	    pkt->fp_tcode != IEEE1394_TCODE_READ_REQUEST_DATABLOCK)
		pkt->fp_dlen = pkt->fp_hdr[3] >> 16;
	DPRINTFN(1, ("%s: tcode=0x%x, hlen=%d, dlen=%d\n", __func__,
	    pkt->fp_tcode, pkt->fp_hlen, pkt->fp_dlen));

	/* Get data. */
	count = 0;
	i = 0;
	while (i < 6 && count < pkt->fp_dlen) {
		len = fwohci_buf_pktget(sc, &fb,
		    (caddr_t *)&pkt->fp_iov[i].iov_base,
		    pkt->fp_dlen - count);
		if (len == 0) {
			printf("%s: malformed input 2: %d\n", __func__,
			    pkt->fp_dlen - count);
			return 0;
		}
		pkt->fp_iov[i++].iov_len = len;
		count += len;
	}
	pkt->fp_uio.uio_iovcnt = i;
	pkt->fp_uio.uio_resid = count;

	if (count < pkt->fp_dlen) {	/* Eat the remainder of the packet. */
		printf("%s: %d iov exhausted, %d bytes not gotten\n",
		    __func__, i, pkt->fp_dlen - count);
		while (count < pkt->fp_dlen) {
			len = fwohci_buf_pktget(sc, &fb,
			    (caddr_t *)&pkt->fp_trail,
			    ((pkt->fp_dlen - count) > sizeof(*pkt->fp_trail)) ?
			    sizeof(*pkt->fp_trail) : (pkt->fp_dlen - count));
			count += len;
		}
	}

	/* Get trailer. */
	len = fwohci_buf_pktget(sc, &fb, (caddr_t *)&pkt->fp_trail,
	    sizeof(*pkt->fp_trail));
	if (len <= 0) {
		printf("%s: malformed input 3: %d\n", __func__,
		    pkt->fp_hlen - count);
		return 0;
	}
	return 1;
}

int
fwohci_buf_input_ppb(struct fwohci_softc *sc, struct fwohci_ctx *fc,
    struct fwohci_pkt *pkt)
{
	caddr_t p;
	int len;
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;

	if (fc->fc_type ==  FWOHCI_CTX_ISO_MULTI) {
		return fwohci_buf_input(sc, fc, pkt);
	}

	bzero(pkt, sizeof(*pkt));
	pkt->fp_uio.uio_iov = pkt->fp_iov;
	pkt->fp_uio.uio_rw = UIO_WRITE;
	pkt->fp_uio.uio_segflg = UIO_SYSSPACE;

	TAILQ_FOREACH(fb, &fc->fc_buf, fb_list) {
		if (fb->fb_off == 0)
			break;
	}
	if (fb == NULL)
		return 0;

	fd = fb->fb_desc;
	len = fd->fd_reqcount - fd->fd_rescount;
	if (len == 0)
		return 0;
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, fb->fb_off, len,
	    BUS_DMASYNC_POSTREAD);

	p = fb->fb_buf;
	fb->fb_off += roundup(len, 4);
	if (len < 8) {
		printf("%s: malformed input 1: %d\n", __func__, len);
		return 0;
	}

	/*
	 * Get trailer first, may be bogus data unless status update
	 * in descriptor is set.
	 */
	pkt->fp_trail = (u_int32_t *)p;
	*pkt->fp_trail = (*pkt->fp_trail & 0xFFFF) | (fd->fd_status << 16);
	pkt->fp_hdr[0] = ((u_int32_t *)p)[1];
	pkt->fp_tcode = (pkt->fp_hdr[0] & 0x000000F0) >> 4;
#ifdef	DIAGNOSTIC
	if (pkt->fp_tcode != IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK) {
		printf("%s: bad tcode: 0x%x\n", __func__, pkt->fp_tcode);
		return 0;
	}
#endif
	pkt->fp_hlen = 4;
	pkt->fp_dlen = pkt->fp_hdr[0] >> 16;
	p += 8;
	len -= 8;
	if (pkt->fp_dlen != len) {
		printf("%s: malformed input 2: %d != %d\n", __func__,
		    pkt->fp_dlen, len);
		return 0;
	}
	DPRINTFN(1, ("%s: tcode=0x%x, hlen=%d, dlen=%d\n", __func__,
	    pkt->fp_tcode, pkt->fp_hlen, pkt->fp_dlen));
	pkt->fp_iov[0].iov_base = p;
	pkt->fp_iov[0].iov_len = len;
	pkt->fp_uio.uio_iovcnt = 1;
	pkt->fp_uio.uio_resid = len;
	return 1;
}

int
fwohci_handler_set(struct fwohci_softc *sc, int tcode,
    u_int32_t key1, u_int32_t key2, u_int32_t key3,
    int (*handler)(struct fwohci_softc *, void *, struct fwohci_pkt *),
    void *arg)
{
	struct fwohci_ctx *fc;
	struct fwohci_handler *fh;
	int i, j, s;

	if (tcode == IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK) {
		int isasync = key1 & OHCI_ASYNC_STREAM;

		key1 &= IEEE1394_ISOCH_MASK;
		j = sc->sc_isoctx;
		fh = NULL;
		for (i = 0; i < sc->sc_isoctx; i++) {
			if ((fc = sc->sc_ctx_ir[i]) == NULL) {
				if (j == sc->sc_isoctx)
					j = i;
				continue;
			}
			fh = LIST_FIRST(&fc->fc_handler);
			if (fh->fh_tcode == tcode &&
			    fh->fh_key1 == key1 && fh->fh_key2 == key2)
				break;
			fh = NULL;
		}
		if (fh == NULL) {
			if (handler == NULL)
				return 0;
			if (j == sc->sc_isoctx) {
				DPRINTF(("%s: no more free context\n",
				    __func__));
				return ENOMEM;
			}
			if ((fc = sc->sc_ctx_ir[j]) == NULL) {
				fwohci_ctx_alloc(sc, &fc, OHCI_BUF_IR_CNT, j,
				    isasync ? FWOHCI_CTX_ISO_SINGLE :
				    FWOHCI_CTX_ISO_MULTI);
				sc->sc_ctx_ir[j] = fc;
			}
		}
	} else {
		switch (tcode) {
		case IEEE1394_TCODE_WRITE_REQUEST_QUADLET:
		case IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK:
		case IEEE1394_TCODE_READ_REQUEST_QUADLET:
		case IEEE1394_TCODE_READ_REQUEST_DATABLOCK:
		case IEEE1394_TCODE_LOCK_REQUEST:
			fc = sc->sc_ctx_arrq;
			break;
		case IEEE1394_TCODE_WRITE_RESPONSE:
		case IEEE1394_TCODE_READ_RESPONSE_QUADLET:
		case IEEE1394_TCODE_READ_RESPONSE_DATABLOCK:
		case IEEE1394_TCODE_LOCK_RESPONSE:
			fc = sc->sc_ctx_arrs;
			break;
		default:
			return EIO;
		}
		LIST_FOREACH(fh, &fc->fc_handler, fh_list) {
			if (fh->fh_tcode == tcode &&
			    fh->fh_key1 == key1 &&
			    fh->fh_key2 == key2 &&
			    fh->fh_key3 == key3)
				break;
		}
	}
	if (handler == NULL) {
		if (fh != NULL) {
			LIST_REMOVE(fh, fh_list);
			FREE(fh, M_DEVBUF);
			MPRINTF("FREE(DEVBUF)", fh);
			fh = NULL;	/* XXX */
		}
		if (tcode == IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK) {
			OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
			sc->sc_ctx_ir[fc->fc_ctx] = NULL;
			fwohci_ctx_free(sc, fc);
		}
		DPRINTFN(1, ("%s: ctx %d, tcode %x, key 0x%x, 0x%x, 0x%x [NULL]\n",
		    __func__, fc->fc_ctx, tcode, key1, key2, key3));

		return 0;
	}
	s = splbio();
	if (fh == NULL) {
		MALLOC(fh, struct fwohci_handler *, sizeof(*fh),
		    M_DEVBUF, M_WAITOK);
		MPRINTF("MALLOC(DEVBUF)", fh);
		bzero(fh, sizeof(*fh));
	}
	fh->fh_tcode = tcode;
	fh->fh_key1 = key1;
	fh->fh_key2 = key2;
	fh->fh_key3 = key3;
	fh->fh_handler = handler;
	fh->fh_handarg = arg;

	if (fh->fh_list.le_prev == NULL)
		LIST_INSERT_HEAD(&fc->fc_handler, fh, fh_list);
	splx(s);

	DPRINTFN(1, ("%s: ctx %d, tcode %x, key 0x%x, 0x%x, 0x%x [%08x]\n",
	    __func__, fc->fc_ctx, tcode, key1, key2, key3, (u_int32_t)handler));

	if (tcode == IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK) {
		s = splbio();
		fwohci_ctx_init(sc, fc);
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
		splx(s);
		DPRINTFN(1, ("%s: SYNC desc %ld\n", __func__,
		    (long)(TAILQ_FIRST(&fc->fc_buf)->fb_desc - sc->sc_desc)));
	}
	return 0;
}

int
fwohci_block_handler_set(struct fwohci_softc *sc, int tcode,
    u_int32_t key1, u_int32_t key2, u_int32_t key3, int len,
    int (*handler)(struct fwohci_softc *, void *, struct fwohci_pkt *),
    void *arg)
{
	u_int32_t key3n = (key3 & 0xFFFF) | ((len & 0xFFFF) << 16);

	return (fwohci_handler_set(sc, tcode, key1, key2, key3n, handler, arg));
}

/*
 * Asynchronous Receive Requests input frontend.
 */
void
fwohci_arrq_input(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	u_int16_t srcid, datalen = 0;
	int rcode, tlabel;
	u_int32_t key1, key2;
	struct fwohci_handler *fh;
	struct fwohci_pkt pkt, res;

	/*
	 * Do not return if next packet is in the buffer, or the next
	 * packet cannot be received until the next receive interrupt.
	 */
	while (fwohci_buf_input(sc, fc, &pkt)) {
		switch(pkt.fp_tcode) {
		case OHCI_TCODE_PHY:
			fwohci_phy_input(sc, &pkt);
			continue; break;
		case IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK:
			datalen = pkt.fp_dlen;
			break;
		case IEEE1394_TCODE_READ_REQUEST_DATABLOCK:
			datalen = pkt.fp_hdr[3] >> 16;
			break;
		}
		key1 = pkt.fp_hdr[1] & 0xFFFF;
		key2 = pkt.fp_hdr[2];
		bzero(&res, sizeof(res));
		res.fp_uio.uio_rw = UIO_WRITE;
		res.fp_uio.uio_segflg = UIO_SYSSPACE;
		srcid = pkt.fp_hdr[1] >> 16;
		tlabel = (pkt.fp_hdr[0] & 0x0000FC00) >> 10;
		DPRINTFN(1, ("%s: tcode 0x%x, from 0x%04x, tlabel 0x%x, "
		    "hlen %d, dlen %d\n", __func__, pkt.fp_tcode,
		    srcid, tlabel, pkt.fp_hlen, pkt.fp_dlen));
		LIST_FOREACH(fh, &fc->fc_handler, fh_list) {
			if (pkt.fp_tcode == fh->fh_tcode &&
			    ((fh->fh_key3 & 0xFFFF) == OHCI_NodeId_NodeNumber ||
			     (srcid & OHCI_NodeId_NodeNumber) ==
			     (fh->fh_key3 & 0xFFFF)) &&
			    ((key1 == fh->fh_key1 && key2 == fh->fh_key2) ||
			     (datalen && key1 == fh->fh_key1 &&
			      key2 >= fh->fh_key2 &&
			      (key2 + datalen) <=
			       (fh->fh_key2 + (fh->fh_key3 >> 16)))))
			{
				DPRINTFN(5, ("%s: handler 0x%08x(0x%08x)\n",
				    __func__, (u_int32_t)(*fh->fh_handler),
				    (u_int32_t)(fh->fh_handarg)));
				rcode = (*fh->fh_handler)(sc, fh->fh_handarg,
				    &pkt);
				DPRINTFN(5, ("%s:  --> rcode %d\n", __func__,
				    rcode));
				break;
			}
		}
		if (fh == NULL) {
			rcode = IEEE1394_RCODE_ADDRESS_ERROR;
			DPRINTFN(1, ("%s: no listener: tcode 0x%x, "
			    "addr=0x%04x%08x\n", __func__, pkt.fp_tcode,
			    key1, key2));
		}
		if (((*pkt.fp_trail & 0x001F0000) >> 16) !=
		    OHCI_CTXCTL_EVENT_ACK_PENDING)
			continue;
		if (rcode != -1)
			fwohci_atrs_output(sc, rcode, &pkt, &res);
	}
	fwohci_buf_next(sc, fc);
	OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx, OHCI_SUBREG_ContextControlSet,
	    OHCI_CTXCTL_WAKE);
}


/*
 * Asynchronous Receive Response input frontend.
 */
void
fwohci_arrs_input(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_pkt pkt;
	struct fwohci_handler *fh;
	u_int16_t srcid;
	int rcode, tlabel;

	while (fwohci_buf_input(sc, fc, &pkt)) {
		srcid = pkt.fp_hdr[1] >> 16;
		rcode = (pkt.fp_hdr[1] & 0x0000F000) >> 12;
		tlabel = (pkt.fp_hdr[0] & 0x0000FC00) >> 10;
		DPRINTFN(1, ("%s: tcode 0x%x, from 0x%04x, tlabel 0x%x, "
		    "rcode 0x%x, hlen %d, dlen %d\n", __func__,
		    pkt.fp_tcode, srcid, tlabel, rcode, pkt.fp_hlen,
		    pkt.fp_dlen));
		LIST_FOREACH(fh, &fc->fc_handler, fh_list) {
			if (pkt.fp_tcode == fh->fh_tcode &&
			    (srcid & OHCI_NodeId_NodeNumber) == fh->fh_key1 &&
			    tlabel == fh->fh_key2) {
				(*fh->fh_handler)(sc, fh->fh_handarg, &pkt);
				LIST_REMOVE(fh, fh_list);
				FREE(fh, M_DEVBUF);
				MPRINTF("FREE(DEVBUF)", fh);
				fh = NULL;	/* XXX */
				break;
			}
		}
		if (fh == NULL)
			DPRINTFN(1, ("%s: no listener\n", __func__));
	}
	fwohci_buf_next(sc, fc);
	OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx, OHCI_SUBREG_ContextControlSet,
	    OHCI_CTXCTL_WAKE);
}

/*
 * Isochronous Receive input frontend.
 */
void
fwohci_ir_input(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	int rcode, chan, tag;
	struct iovec *iov;
	struct fwohci_handler *fh;
	struct fwohci_pkt pkt;

#if DOUBLEBUF
	if (fc->fc_type == FWOHCI_CTX_ISO_MULTI) {
		struct fwohci_buf *fb;
		int i;
		u_int32_t reg;

		/* Stop dma engine before read buffer. */
		reg = OHCI_SYNC_RX_DMA_READ(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlClear);
		DPRINTFN(5, ("%s: %08x =>", __func__, reg));
		if (reg & OHCI_CTXCTL_RUN) {
			OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
		}
		DPRINTFN(5, (" %08x\n", OHCI_SYNC_RX_DMA_READ(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlClear)));

		i = 0;
		while ((reg = OHCI_SYNC_RX_DMA_READ(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet)) & OHCI_CTXCTL_ACTIVE) {
			delay(10);
			if (++i > 10000) {
				printf("cannot stop dma engine 0x%08x\n", reg);
				return;
			}
		}

		/* Rotate dma buffer. */
		fb = TAILQ_FIRST(&fc->fc_buf2);
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx, OHCI_SUBREG_CommandPtr,
		    fb->fb_daddr | 1);
		MPRINTF("OHCI_SUBREG_CommandPtr(SYNC_RX)", fb->fb_daddr);
		/* Start dma engine. */
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
		OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntEventClear,
		    (1 << fc->fc_ctx));
	}
#endif

	while (fwohci_buf_input_ppb(sc, fc, &pkt)) {
		chan = (pkt.fp_hdr[0] & 0x00003F00) >> 8;
		tag  = (pkt.fp_hdr[0] & 0x0000C000) >> 14;
		DPRINTFN(1, ("%s: hdr 0x%08x, tcode 0x%0x, hlen %d, dlen %d\n",
		    __func__, pkt.fp_hdr[0], pkt.fp_tcode, pkt.fp_hlen,
		    pkt.fp_dlen));
		if (tag == IEEE1394_TAG_GASP) {
			/*
			 * The pkt with tag=3 is GASP format.
			 * Move GASP header to header part.
			 */
			if (pkt.fp_dlen < 8)
				continue;
			iov = pkt.fp_iov;
			/* Assuming pkt per buffer mode. */
			pkt.fp_hdr[1] = ntohl(((u_int32_t *)iov->iov_base)[0]);
			pkt.fp_hdr[2] = ntohl(((u_int32_t *)iov->iov_base)[1]);
			iov->iov_base = (caddr_t)iov->iov_base + 8;
			iov->iov_len -= 8;
			pkt.fp_hlen += 8;
			pkt.fp_dlen -= 8;
		}
		sc->sc_isopktcnt.ev_count++;
		LIST_FOREACH(fh, &fc->fc_handler, fh_list) {
			if (pkt.fp_tcode == fh->fh_tcode &&
			    chan == fh->fh_key1 && tag == fh->fh_key2) {
				rcode = (*fh->fh_handler)(sc, fh->fh_handarg,
				    &pkt);
				break;
			}
		}
#ifdef	FWOHCI_DEBUG
		if (fh == NULL) {
			DPRINTFN(1, ("%s: no handler\n", __func__));
		} else {
			DPRINTFN(1, ("%s: rcode %d\n", __func__, rcode));
		}
#endif	/* FWOHCI_DEBUG */
	}
	fwohci_buf_next(sc, fc);

	if (fc->fc_type == FWOHCI_CTX_ISO_SINGLE) {
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet,
		    OHCI_CTXCTL_WAKE);
	}
}

/*
 * Asynchronous Transmit common routine.
 */
int
fwohci_at_output(struct fwohci_softc *sc, struct fwohci_ctx *fc,
    struct fwohci_pkt *pkt)
{
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	struct mbuf *m, *m0;
	int i, ndesc, error, off, len;
	u_int32_t val;
#ifdef	FWOHCI_DEBUG
	struct iovec *iov;
#endif	/* FWOHCI_DEBUG */

	if ((sc->sc_nodeid & OHCI_NodeId_NodeNumber) == IEEE1394_BCAST_PHY_ID)
		/* We can't send anything during selfid duration */
		return EAGAIN;

#ifdef	FWOHCI_DEBUG
	DPRINTFN(1, ("%s: tcode 0x%x, hlen %d, dlen %d", __func__,
	    pkt->fp_tcode, pkt->fp_hlen, pkt->fp_dlen));
	for (i = 0; i < pkt->fp_hlen/4; i++)
		DPRINTFN(2, ("%s%08x", i?" ":"\n    ", pkt->fp_hdr[i]));
	DPRINTFN(2, (" $"));
	if (pkt->fp_uio.uio_iovcnt) {
		for (ndesc = 0, iov = pkt->fp_iov;
		     ndesc < pkt->fp_uio.uio_iovcnt; ndesc++, iov++) {
			for (i = 0; i < iov->iov_len; i++)
				DPRINTFN(2, ("%s%02x", (i%32)?((i%4)?"":" ")
							     :"\n    ",
				    ((u_int8_t *)iov->iov_base)[i]));
			DPRINTFN(2, (" $"));
		}
	}
	DPRINTFN(1, ("\n"));
#endif	/* FWOHCI_DEBUG */

	if ((m = pkt->fp_m) != NULL) {
		for (ndesc = 2; m != NULL; m = m->m_next)
			ndesc++;
		if (ndesc > OHCI_DESC_MAX) {
			m0 = NULL;
			ndesc = 2;
			for (off = 0; off < pkt->fp_dlen; off += len) {
				if (m0 == NULL) {
					MGETHDR(m0, M_DONTWAIT, MT_DATA);
					MPRINTF("MGETHDR", m0);
					if (m0 != NULL) {
#ifdef	__NetBSD__
						M_COPY_PKTHDR(m0, pkt->fp_m);
#else
						M_DUP_PKTHDR(m0, pkt->fp_m);
#endif
					}
					m = m0;
				} else {
					MGET(m->m_next, M_DONTWAIT, MT_DATA);
					MPRINTF("MGET", m->m_next);
					m = m->m_next;
				}
				if (m != NULL)
					MCLGET(m, M_DONTWAIT);
				if (m == NULL || (m->m_flags & M_EXT) == 0) {
					m_freem(m0);
					MPRINTF("m_freem", m0);
					return ENOMEM;
				}
				len = pkt->fp_dlen - off;
				if (len > m->m_ext.ext_size)
					len = m->m_ext.ext_size;
				m_copydata(pkt->fp_m, off, len,
				    mtod(m, caddr_t));
				m->m_len = len;
				ndesc++;
			}
			m_freem(pkt->fp_m);
			MPRINTF("m_freem", pkt->fp_m);
			pkt->fp_m = m0;
		}
	} else
		ndesc = 2 + pkt->fp_uio.uio_iovcnt;

	if (ndesc > OHCI_DESC_MAX)
		return ENOBUFS;

	if (fc->fc_bufcnt > 50)			/* XXX */
		return ENOBUFS;

	fb = malloc(sizeof(*fb), M_DEVBUF, M_WAITOK);
	MPRINTF("malloc(DEVBUF)", fb);
	fb->fb_nseg = ndesc;
	fb->fb_desc = fwohci_desc_get(sc, ndesc);
	if (fb->fb_desc == NULL) {
		free(fb, M_DEVBUF);
		MPRINTF("free(DEVBUF)", fb);
		fb = NULL;	/* XXX */
		return ENOBUFS;
	}
	fb->fb_daddr = sc->sc_ddmamap->dm_segs[0].ds_addr +
	    ((caddr_t)fb->fb_desc - (caddr_t)sc->sc_desc);
	fb->fb_m = pkt->fp_m;
	fb->fb_callback = pkt->fp_callback;
	fb->fb_statuscb = pkt->fp_statuscb;
	fb->fb_statusarg = pkt->fp_statusarg;

	if (ndesc > 2) {
		if ((error = bus_dmamap_create(sc->sc_dmat, pkt->fp_dlen, ndesc,
		    PAGE_SIZE, 0, BUS_DMA_WAITOK, &fb->fb_dmamap)) != 0) {
			fwohci_desc_put(sc, fb->fb_desc, ndesc);
			free(fb, M_DEVBUF);
			MPRINTF("free(DEVBUF)", fb);
			fb = NULL;	/* XXX */
			return error;
		}
		MPRINTF("bus_dmamap_create", fb->fb_dmamap);

		if (pkt->fp_m != NULL)
			error = bus_dmamap_load_mbuf(sc->sc_dmat, fb->fb_dmamap,
			    pkt->fp_m, BUS_DMA_WAITOK);
		else
			error = bus_dmamap_load_uio(sc->sc_dmat, fb->fb_dmamap,
			    &pkt->fp_uio, BUS_DMA_WAITOK);
		if (error != 0) {
			bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
			MPRINTF("bus_dmamap_destroy", fb->fb_dmamap);
			fwohci_desc_put(sc, fb->fb_desc, ndesc);
			free(fb, M_DEVBUF);
			MPRINTF("free(DEVBUF)", fb);
			fb = NULL;	/* XXX */
			return error;
		}
		bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0, pkt->fp_dlen,
		    BUS_DMASYNC_PREWRITE);
	}

	fd = fb->fb_desc;
	fd->fd_flags = OHCI_DESC_IMMED;
	fd->fd_reqcount = pkt->fp_hlen;
	fd->fd_data = 0;
	fd->fd_branch = 0;
	fd->fd_status = 0;
	if (fc->fc_ctx == OHCI_CTX_ASYNC_TX_RESPONSE) {
		i = 3;				/* XXX: 3 sec */
		val = OHCI_CSR_READ(sc, OHCI_REG_IsochronousCycleTimer);
		fd->fd_timestamp = ((val >> 12) & 0x1FFF) |
		    ((((val >> 25) + i) & 0x7) << 13);
	} else
		fd->fd_timestamp = 0;

#if 1	/* XXX */
	bcopy(pkt->fp_hdr, fd + 1, pkt->fp_hlen);
#else
	bcopy(pkt->fp_hdr, fd->fd_immed, pkt->fp_hlen);
#endif

	if (ndesc > 2) {
		for (i = 0; i < ndesc - 2; i++) {
			fd = fb->fb_desc + 2 + i;
			fd->fd_flags = 0;
			fd->fd_reqcount = fb->fb_dmamap->dm_segs[i].ds_len;
			fd->fd_data = fb->fb_dmamap->dm_segs[i].ds_addr;
			fd->fd_branch = 0;
			fd->fd_status = 0;
			fd->fd_timestamp = 0;
		}
		bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0, pkt->fp_dlen,
		    BUS_DMASYNC_POSTWRITE);
	}
	fd->fd_flags |= OHCI_DESC_LAST | OHCI_DESC_BRANCH;
	fd->fd_flags |= OHCI_DESC_INTR_ALWAYS;

#ifdef	FWOHCI_DEBUG
	DPRINTFN(1, ("%s: desc %ld", __func__,
	    (long)(fb->fb_desc - sc->sc_desc)));
	for (i = 0; i < ndesc * 4; i++)
		DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ",
		    ((u_int32_t *)fb->fb_desc)[i]));
	DPRINTFN(1, ("\n"));
#endif	/* FWOHCI_DEBUG */

	val = OHCI_ASYNC_DMA_READ(sc, fc->fc_ctx,
	    OHCI_SUBREG_ContextControlClear);

	if (val & OHCI_CTXCTL_RUN) {
		if (fc->fc_branch == NULL) {
			OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
			goto run;
		}
		*fc->fc_branch = fb->fb_daddr | ndesc;
		OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_WAKE);
	} else {
  run:
		OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_CommandPtr, fb->fb_daddr | ndesc);
		MPRINTF("OHCI_SUBREG_CommandPtr(ASYNC)", fb->fb_daddr);
		OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	}
	fc->fc_branch = &fd->fd_branch;

	fc->fc_bufcnt++;
	TAILQ_INSERT_TAIL(&fc->fc_buf, fb, fb_list);
	pkt->fp_m = NULL;
	return 0;
}

void
fwohci_at_done(struct fwohci_softc *sc, struct fwohci_ctx *fc, int force)
{
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	struct fwohci_pkt pkt;
	int i;

//	splassert(IPL_BIO);

	while ((fb = TAILQ_FIRST(&fc->fc_buf)) != NULL) {
		fd = fb->fb_desc;
#ifdef	FWOHCI_DEBUG
		DPRINTFN(1, ("%s: %sdesc %ld (%d)", __func__,
		    force ? "force " : "", (long)(fd - sc->sc_desc),
		    fb->fb_nseg));
		for (i = 0; i < fb->fb_nseg * 4; i++)
			DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ",
			    ((u_int32_t *)fd)[i]));
		DPRINTFN(1, ("\n"));
#endif	/* FWOHCI_DEBUG */
		if (fb->fb_nseg > 2)
			fd += fb->fb_nseg - 1;
		if (!force && !(fd->fd_status & OHCI_CTXCTL_ACTIVE))
			break;
		TAILQ_REMOVE(&fc->fc_buf, fb, fb_list);
		if (fc->fc_branch == &fd->fd_branch) {
			OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
			fc->fc_branch = NULL;
			for (i = 0; i < OHCI_LOOP; i++) {
				if (!(OHCI_ASYNC_DMA_READ(sc, fc->fc_ctx,
				    OHCI_SUBREG_ContextControlClear) &
				    OHCI_CTXCTL_ACTIVE))
					break;
				DELAY(10);
			}
		}

		if (fb->fb_statuscb) {
			bzero(&pkt, sizeof(pkt));
			pkt.fp_status = fd->fd_status;
			bcopy(fd + 1, pkt.fp_hdr, sizeof(pkt.fp_hdr[0]));

			/* Indicate this is just returning the status bits. */
			pkt.fp_tcode = -1;
			(*fb->fb_statuscb)(sc, fb->fb_statusarg, &pkt);
			fb->fb_statuscb = NULL;
			fb->fb_statusarg = NULL;
		}
		fwohci_desc_put(sc, fb->fb_desc, fb->fb_nseg);
		if (fb->fb_nseg > 2) {
			bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
			MPRINTF("bus_dmamap_destroy", fb->fb_dmamap);
		}
		fc->fc_bufcnt--;
		if (fb->fb_callback) {
			(*fb->fb_callback)(sc->sc_sc1394.sc1394_if, fb->fb_m);
			fb->fb_callback = NULL;
		} else if (fb->fb_m != NULL) {
			m_freem(fb->fb_m);
			MPRINTF("m_freem", fb->fb_m);
		}
		free(fb, M_DEVBUF);
		MPRINTF("free(DEVBUF)", fb);
		fb = NULL;	/* XXX */
	}
}

/*
 * Asynchronous Transmit Reponse -- in response of request packet.
 */
void
fwohci_atrs_output(struct fwohci_softc *sc, int rcode, struct fwohci_pkt *req,
    struct fwohci_pkt *res)
{

	if (((*req->fp_trail & 0x001F0000) >> 16) !=
	    OHCI_CTXCTL_EVENT_ACK_PENDING)
		return;

	res->fp_hdr[0] = (req->fp_hdr[0] & 0x0000FC00) | 0x00000100;
	res->fp_hdr[1] = (req->fp_hdr[1] & 0xFFFF0000) | (rcode << 12);
	switch (req->fp_tcode) {
	case IEEE1394_TCODE_WRITE_REQUEST_QUADLET:
	case IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK:
		res->fp_tcode = IEEE1394_TCODE_WRITE_RESPONSE;
		res->fp_hlen = 12;
		break;
	case IEEE1394_TCODE_READ_REQUEST_QUADLET:
		res->fp_tcode = IEEE1394_TCODE_READ_RESPONSE_QUADLET;
		res->fp_hlen = 16;
		res->fp_dlen = 0;
		if (res->fp_uio.uio_iovcnt == 1 && res->fp_iov[0].iov_len == 4)
			res->fp_hdr[3] =
			    *(u_int32_t *)res->fp_iov[0].iov_base;
		res->fp_uio.uio_iovcnt = 0;
		break;
	case IEEE1394_TCODE_READ_REQUEST_DATABLOCK:
	case IEEE1394_TCODE_LOCK_REQUEST:
		if (req->fp_tcode == IEEE1394_TCODE_LOCK_REQUEST)
			res->fp_tcode = IEEE1394_TCODE_LOCK_RESPONSE;
		else
			res->fp_tcode = IEEE1394_TCODE_READ_RESPONSE_DATABLOCK;
		res->fp_hlen = 16;
		res->fp_dlen = res->fp_uio.uio_resid;
		res->fp_hdr[3] = res->fp_dlen << 16;
		break;
	}
	res->fp_hdr[0] |= (res->fp_tcode << 4);
	fwohci_at_output(sc, sc->sc_ctx_atrs, res);
}

/*
 * APPLICATION LAYER SERVICES.
 */

/*
 * Retrieve Global UID from GUID ROM.
 */
int
fwohci_guidrom_init(struct fwohci_softc *sc)
{
	int i, n, off;
	u_int32_t val1, val2;

	/*
	 * Extract the Global UID.
	 */
	val1 = OHCI_CSR_READ(sc, OHCI_REG_GUIDHi);
	val2 = OHCI_CSR_READ(sc, OHCI_REG_GUIDLo);

	if (val1 != 0 || val2 != 0) {
		sc->sc_sc1394.sc1394_guid[0] = (val1 >> 24) & 0xFF;
		sc->sc_sc1394.sc1394_guid[1] = (val1 >> 16) & 0xFF;
		sc->sc_sc1394.sc1394_guid[2] = (val1 >>  8) & 0xFF;
		sc->sc_sc1394.sc1394_guid[3] = (val1 >>  0) & 0xFF;
		sc->sc_sc1394.sc1394_guid[4] = (val2 >> 24) & 0xFF;
		sc->sc_sc1394.sc1394_guid[5] = (val2 >> 16) & 0xFF;
		sc->sc_sc1394.sc1394_guid[6] = (val2 >>  8) & 0xFF;
		sc->sc_sc1394.sc1394_guid[7] = (val2 >>  0) & 0xFF;
	} else {
		val1 = OHCI_CSR_READ(sc, OHCI_REG_Version);
		if ((val1 & OHCI_Version_GUID_ROM) == 0)
			return -1;
		OHCI_CSR_WRITE(sc, OHCI_REG_Guid_Rom, OHCI_Guid_AddrReset);
		for (i = 0; i < OHCI_LOOP; i++) {
			val1 = OHCI_CSR_READ(sc, OHCI_REG_Guid_Rom);
			if (!(val1 & OHCI_Guid_AddrReset))
				break;
			DELAY(10);
		}
		off = OHCI_BITVAL(val1, OHCI_Guid_MiniROM) + 4;
		val2 = 0;
		for (n = 0; n < off + sizeof(sc->sc_sc1394.sc1394_guid); n++) {
			OHCI_CSR_WRITE(sc, OHCI_REG_Guid_Rom,
			    OHCI_Guid_RdStart);
			for (i = 0; i < OHCI_LOOP; i++) {
				val1 = OHCI_CSR_READ(sc, OHCI_REG_Guid_Rom);
				if (!(val1 & OHCI_Guid_RdStart))
					break;
				DELAY(10);
			}
			if (n < off)
				continue;
			val1 = OHCI_BITVAL(val1, OHCI_Guid_RdData);
			sc->sc_sc1394.sc1394_guid[n - off] = val1;
			val2 |= val1;
		}
		if (val2 == 0)
			return -1;
	}
	return 0;
}

/*
 * Initialization for Configuration ROM (no DMA context).
 */

#define	CFR_MAXUNIT		20

typedef struct configromctx {
	u_int32_t	*ptr;
	int		 curunit;
	struct {
		u_int32_t	*start;
		int		 length;
		u_int32_t	*refer;
		int		 refunit;
	} unit[CFR_MAXUNIT];
} configromctx;

#define	CFR_PUT_DATA4(cfr, d1, d2, d3, d4)				\
	(*(cfr)->ptr++ = (((d1)<<24) | ((d2)<<16) | ((d3)<<8) | (d4)))

#define	CFR_PUT_DATA1(cfr, d)		(*(cfr)->ptr++ = (d))

#define	CFR_PUT_VALUE(cfr, key, d)	(*(cfr)->ptr++ = ((key)<<24) | (d))

#define	CFR_PUT_CRC(cfr, n)						\
	(*(cfr)->unit[n].start = ((cfr)->unit[n].length << 16) |	\
	    fwohci_crc16((cfr)->unit[n].start + 1, (cfr)->unit[n].length))

#define	CFR_START_UNIT(cfr, n)						\
do {									\
	if ((cfr)->unit[n].refer != NULL) {				\
		*(cfr)->unit[n].refer |=				\
		    (cfr)->ptr - (cfr)->unit[n].refer;			\
		CFR_PUT_CRC(cfr, (cfr)->unit[n].refunit);		\
	}								\
	(cfr)->curunit = (n);						\
	(cfr)->unit[n].start = (cfr)->ptr++;				\
} while (0)

#define	CFR_PUT_REFER(cfr, key, n)					\
do {									\
	(cfr)->unit[n].refer = (cfr)->ptr;				\
	(cfr)->unit[n].refunit = (cfr)->curunit;			\
	*(cfr)->ptr++ = (key) << 24;					\
} while (0)

#define	CFR_END_UNIT(cfr)						\
do {									\
	(cfr)->unit[(cfr)->curunit].length = (cfr)->ptr -		\
	    ((cfr)->unit[(cfr)->curunit].start + 1);			\
	CFR_PUT_CRC(cfr, (cfr)->curunit);				\
} while (0)

u_int16_t
fwohci_crc16(u_int32_t *ptr, int len)
{
	int shift;
	u_int32_t crc, sum, data;

	crc = 0;
	while (len-- > 0) {
		data = *ptr++;
		for (shift = 28; shift >= 0; shift -= 4) {
			sum = ((crc >> 12) ^ (data >> shift)) & 0x000F;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}
		crc &= 0xFFFF;
	}
	return crc;
}

void
fwohci_configrom_init(struct fwohci_softc *sc)
{
	int i, val;
	struct fwohci_buf *fb;
	u_int32_t *hdr;
	struct configromctx cfr;

	//splassert(IPL_BIO);

	fb = &sc->sc_buf_cnfrom;
	bzero(&cfr, sizeof(cfr));
	cfr.ptr = hdr = (u_int32_t *)fb->fb_buf;

	/* Headers. */
	CFR_START_UNIT(&cfr, 0);
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_BusId));
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_BusOptions));
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_GUIDHi));
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_GUIDLo));
	CFR_END_UNIT(&cfr);
	/* Copy info_length from crc_length. */
	*hdr |= (*hdr & 0x00FF0000) << 8;
	OHCI_CSR_WRITE(sc, OHCI_REG_ConfigROMhdr, *hdr);

	/* Root directory. */
	CFR_START_UNIT(&cfr, 1);
	CFR_PUT_VALUE(&cfr, 0x03, 0x00005E);	/* Vendor ID. */
	CFR_PUT_REFER(&cfr, 0x81, 2);		/* Textual descriptor offset. */
	CFR_PUT_VALUE(&cfr, 0x0C, 0x0083C0);	/* Node capability. */
						/* spt,64,fix,lst,drq */
#ifdef	INET
	CFR_PUT_REFER(&cfr, 0xD1, 3);		/* IPv4 unit directory. */
#endif	/* INET */
#ifdef	INET6
	CFR_PUT_REFER(&cfr, 0xD1, 4);		/* IPv6 unit directory. */
#endif	/* INET6 */
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 2);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* Textual descriptor. */
	CFR_PUT_DATA1(&cfr, 0);			/* Minimal ASCII. */
#ifdef	__NetBSD__
	CFR_PUT_DATA4(&cfr, 'N', 'e', 't', 'B');
	CFR_PUT_DATA4(&cfr, 'S', 'D', 0x00, 0x00);
#else
	CFR_PUT_DATA4(&cfr, 'O', 'p', 'e', 'n');
	CFR_PUT_DATA4(&cfr, 'B', 'S', 'D', 0x00);
#endif
	CFR_END_UNIT(&cfr);

#ifdef	INET
	/* IPv4 unit directory. */
	CFR_START_UNIT(&cfr, 3);
	CFR_PUT_VALUE(&cfr, 0x12, 0x00005E);	/* Unit spec ID. */
	CFR_PUT_REFER(&cfr, 0x81, 6);		/* Textual descriptor offset. */
	CFR_PUT_VALUE(&cfr, 0x13, 0x000001);	/* Unit sw version. */
	CFR_PUT_REFER(&cfr, 0x81, 7);		/* Textual descriptor offset. */
	CFR_PUT_REFER(&cfr, 0x95, 8);		/* unit location. */
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 6);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* Textual descriptor. */
	CFR_PUT_DATA1(&cfr, 0);			/* Minimal ASCII. */
	CFR_PUT_DATA4(&cfr, 'I', 'A', 'N', 'A');
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 7);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* Textual descriptor. */
	CFR_PUT_DATA1(&cfr, 0);			/* Minimal ASCII. */
	CFR_PUT_DATA4(&cfr, 'I', 'P', 'v', '4');
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 8);		/* Spec's valid addr range. */
	CFR_PUT_DATA1(&cfr, FW_FIFO_HI);
	CFR_PUT_DATA1(&cfr, (FW_FIFO_LO | 0x1));
	CFR_PUT_DATA1(&cfr, FW_FIFO_HI);
	CFR_PUT_DATA1(&cfr, FW_FIFO_LO);
	CFR_END_UNIT(&cfr);

#endif	/* INET */

#ifdef	INET6
	/* IPv6 unit directory. */
	CFR_START_UNIT(&cfr, 4);
	CFR_PUT_VALUE(&cfr, 0x12, 0x00005E);	/* Unit spec id. */
	CFR_PUT_REFER(&cfr, 0x81, 9);		/* Textual descriptor offset. */
	CFR_PUT_VALUE(&cfr, 0x13, 0x000002);	/* Unit sw version. */
						/* XXX: TBA by IANA */
	CFR_PUT_REFER(&cfr, 0x81, 10);		/* Textual descriptor offset. */
	CFR_PUT_REFER(&cfr, 0x95, 11);		/* Unit location. */
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 9);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* Textual descriptor. */
	CFR_PUT_DATA1(&cfr, 0);			/* Minimal ASCII. */
	CFR_PUT_DATA4(&cfr, 'I', 'A', 'N', 'A');
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 10);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* Textual descriptor. */
	CFR_PUT_DATA1(&cfr, 0);
	CFR_PUT_DATA4(&cfr, 'I', 'P', 'v', '6');
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 11);		/* Spec's valid addr range. */
	CFR_PUT_DATA1(&cfr, FW_FIFO_HI);
	CFR_PUT_DATA1(&cfr, (FW_FIFO_LO | 0x1));
	CFR_PUT_DATA1(&cfr, FW_FIFO_HI);
	CFR_PUT_DATA1(&cfr, FW_FIFO_LO);
	CFR_END_UNIT(&cfr);

#endif	/* INET6 */

	fb->fb_off = cfr.ptr - hdr;
#ifdef	FWOHCI_DEBUG
	DPRINTF(("%s: Config ROM:", sc->sc_sc1394.sc1394_dev.dv_xname));
	for (i = 0; i < fb->fb_off; i++)
		DPRINTF(("%s%08x", i&7?" ":"\n    ", hdr[i]));
	DPRINTF(("\n"));
#endif	/* FWOHCI_DEBUG */

	/*
	 * Make network byte order for DMA.
	 */
	for (i = 0; i < fb->fb_off; i++)
		HTONL(hdr[i]);
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0,
	    (caddr_t)cfr.ptr - fb->fb_buf, BUS_DMASYNC_PREWRITE);

	OHCI_CSR_WRITE(sc, OHCI_REG_ConfigROMmap,
	    fb->fb_dmamap->dm_segs[0].ds_addr);

	/* This register is only valid on OHCI 1.1. */
	val = OHCI_CSR_READ(sc, OHCI_REG_Version);
	if ((OHCI_Version_GET_Version(val) == 1) &&
	    (OHCI_Version_GET_Revision(val) == 1))
		OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet,
		    OHCI_HCControl_BIBImageValid);

	/* Just allow quad reads of the rom, from every nodes. */
	for (i = 0; i < fb->fb_off; i++)
		fwohci_handler_set(sc, IEEE1394_TCODE_READ_REQUEST_QUADLET,
		    CSR_BASE_HI, CSR_BASE_LO + CSR_CONFIG_ROM + (i * 4),
		    OHCI_NodeId_NodeNumber, fwohci_configrom_input, NULL);
}

int
fwohci_configrom_input(struct fwohci_softc *sc, void *arg,
    struct fwohci_pkt *pkt)
{
	struct fwohci_pkt res;
	u_int32_t loc, *rom;

	/* This will be used as an array index so size accordingly. */
	loc = pkt->fp_hdr[2] - (CSR_BASE_LO + CSR_CONFIG_ROM);
	if ((loc & 0x03) != 0) {
		/* Alignment error. */
		return IEEE1394_RCODE_ADDRESS_ERROR;
	}
	else
		loc /= 4;
	rom = (u_int32_t *)sc->sc_buf_cnfrom.fb_buf;

	DPRINTFN(1, ("%s: ConfigRom[0x%04x]: 0x%08x\n", __func__, loc,
	    ntohl(rom[loc])));

	bzero(&res, sizeof(res));
	res.fp_hdr[3] = rom[loc];
	fwohci_atrs_output(sc, IEEE1394_RCODE_COMPLETE, pkt, &res);
	return -1;
}

/*
 * SelfID buffer (no DMA context).
 */
void
fwohci_selfid_init(struct fwohci_softc *sc)
{
	struct fwohci_buf *fb;

	//splassert(IPL_BIO);

	fb = &sc->sc_buf_selfid;
#ifdef	DIAGNOSTIC
	if ((fb->fb_dmamap->dm_segs[0].ds_addr & 0x7FF) != 0)
		panic("fwohci_selfid_init: not aligned: %ld (%ld) %p",
		    (unsigned long)fb->fb_dmamap->dm_segs[0].ds_addr,
		    (unsigned long)fb->fb_dmamap->dm_segs[0].ds_len, fb->fb_buf);
#endif
	bzero(fb->fb_buf, fb->fb_dmamap->dm_segs[0].ds_len);
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0,
	    fb->fb_dmamap->dm_segs[0].ds_len, BUS_DMASYNC_PREREAD);

	OHCI_CSR_WRITE(sc, OHCI_REG_SelfIDBuffer,
	    fb->fb_dmamap->dm_segs[0].ds_addr);
}

int
fwohci_selfid_input(struct fwohci_softc *sc)
{
	int i;
	u_int32_t count, val, gen;
	u_int32_t *buf;

	buf = (u_int32_t *)sc->sc_buf_selfid.fb_buf;
	val = OHCI_CSR_READ(sc, OHCI_REG_SelfIDCount);
  again:
	if (val & OHCI_SelfID_Error) {
		printf("%s: SelfID Error\n", sc->sc_sc1394.sc1394_dev.dv_xname);
		return (-1);
	}
	count = OHCI_BITVAL(val, OHCI_SelfID_Size);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_buf_selfid.fb_dmamap,
	    0, count << 2, BUS_DMASYNC_POSTREAD);
	gen = OHCI_BITVAL(buf[0], OHCI_SelfID_Gen);

#ifdef	FWOHCI_DEBUG
	DPRINTFN(1, ("%s: SelfID: 0x%08x", sc->sc_sc1394.sc1394_dev.dv_xname,
	    val));
	for (i = 0; i < count; i++)
		DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ", buf[i]));
	DPRINTFN(1, ("\n"));
#endif	/* FWOHCI_DEBUG */

	for (i = 1; i < count; i += 2) {
		if (buf[i] != ~buf[i + 1])
			break;
#if	defined(FWOHCI_DEBUG)
		if (fwohcidebug > 2)
			fwohci_show_phypkt(sc, buf[i]);
#endif	/* FWOHCI_DEBUG */
		if (buf[i] & IEEE1394_SELFID_MORE_PACKETS)
			continue;	/* More pkt. */
		if (buf[i] & IEEE1394_SELFID_EXTENDED)
			continue;	/* Extended ID. */
		sc->sc_rootid = OHCI_BITVAL(buf[i], IEEE1394_PHY_ID);
		if ((buf[i] &
		     (IEEE1394_SELFID_LINK_ACTIVE|IEEE1394_SELFID_CONTENDER))
		    == (IEEE1394_SELFID_LINK_ACTIVE|IEEE1394_SELFID_CONTENDER))
			sc->sc_irmid = sc->sc_rootid;
	}

	val = OHCI_CSR_READ(sc, OHCI_REG_SelfIDCount);
	if (OHCI_BITVAL(val, OHCI_SelfID_Gen) != gen) {
		if (OHCI_BITVAL(val, OHCI_SelfID_Gen) !=
		    OHCI_BITVAL(buf[0], OHCI_SelfID_Gen))
			goto again;
		DPRINTF(("%s: SelfID Gen mismatch (%d, %d)\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, gen,
		    OHCI_BITVAL(val, OHCI_SelfID_Gen)));
		return (-1);
	}
	if (i != count) {
		printf("%s: SelfID corrupted (%d, 0x%08x, 0x%08x)\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, i, buf[i], buf[i + 1]);
#if 1
		if (i == 1 && buf[i] == 0 && buf[i + 1] == 0) {
			/*
			 * XXX: CXD3222 sometimes fails to DMA
			 * selfid packet??
			 */
			sc->sc_rootid = (count - 1) / 2 - 1;
			sc->sc_irmid = sc->sc_rootid;
		} else
#endif
		return (-1);
	}

	val = OHCI_CSR_READ(sc, OHCI_REG_NodeId);
	if ((val & OHCI_NodeId_IDValid) == 0) {
		sc->sc_nodeid = 0xFFFF;		/* Invalid. */
		printf("%s: nodeid is invalid\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		return (-1);
	}
	sc->sc_nodeid = val & 0xFFFF;

	DPRINTF(("%s: nodeid=0x%04x(%d), rootid=%d, irmid=%d\n",
	    sc->sc_sc1394.sc1394_dev.dv_xname, sc->sc_nodeid,
	    sc->sc_nodeid & OHCI_NodeId_NodeNumber, sc->sc_rootid,
	    sc->sc_irmid));

	if ((sc->sc_nodeid & OHCI_NodeId_NodeNumber) > sc->sc_rootid)
		return (-1);

	if ((sc->sc_nodeid & OHCI_NodeId_NodeNumber) == sc->sc_rootid)
		OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlSet,
		    OHCI_LinkControl_CycleMaster);
	else
		OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlClear,
		    OHCI_LinkControl_CycleMaster);
	return (0);
}

/*
 * Some CSRs are handled by driver.
 */
void
fwohci_csr_init(struct fwohci_softc *sc)
{
	int i;
	static u_int32_t csr[] = {
	    CSR_STATE_CLEAR, CSR_STATE_SET, CSR_SB_CYCLE_TIME,
	    CSR_SB_BUS_TIME, CSR_SB_BUSY_TIMEOUT, CSR_SB_BUS_MANAGER_ID,
	    CSR_SB_CHANNEL_AVAILABLE_HI, CSR_SB_CHANNEL_AVAILABLE_LO,
	    CSR_SB_BROADCAST_CHANNEL
	};

	//splassert(IPL_BIO);

	for (i = 0; i < sizeof(csr) / sizeof(csr[0]); i++) {
		fwohci_handler_set(sc, IEEE1394_TCODE_WRITE_REQUEST_QUADLET,
		    CSR_BASE_HI, CSR_BASE_LO + csr[i], OHCI_NodeId_NodeNumber,
		    fwohci_csr_input, NULL);
		fwohci_handler_set(sc, IEEE1394_TCODE_READ_REQUEST_QUADLET,
		    CSR_BASE_HI, CSR_BASE_LO + csr[i], OHCI_NodeId_NodeNumber,
		    fwohci_csr_input, NULL);
	}
	sc->sc_csr[CSR_SB_BROADCAST_CHANNEL] = 31;	/*XXX*/
}

int
fwohci_csr_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct fwohci_pkt res;
	u_int32_t reg;

	/*
	 * XXX Need to do special functionality other than just r/w...
	 */
	reg = pkt->fp_hdr[2] - CSR_BASE_LO;

	if ((reg & 0x03) != 0) {
		/* Alignment error. */
		return IEEE1394_RCODE_ADDRESS_ERROR;
	}
	DPRINTFN(1, ("%s: CSR[0x%04x]: 0x%08x", __func__, reg,
	    *(u_int32_t *)(&sc->sc_csr[reg])));
	if (pkt->fp_tcode == IEEE1394_TCODE_WRITE_REQUEST_QUADLET) {
		DPRINTFN(1, (" -> 0x%08x\n",
		    ntohl(*(u_int32_t *)pkt->fp_iov[0].iov_base)));
		*(u_int32_t *)&sc->sc_csr[reg] =
		    ntohl(*(u_int32_t *)pkt->fp_iov[0].iov_base);
	} else {
		DPRINTFN(1, ("\n"));
		res.fp_hdr[3] = htonl(*(u_int32_t *)&sc->sc_csr[reg]);
		res.fp_iov[0].iov_base = &res.fp_hdr[3];
		res.fp_iov[0].iov_len = 4;
		res.fp_uio.uio_resid = 4;
		res.fp_uio.uio_iovcnt = 1;
		fwohci_atrs_output(sc, IEEE1394_RCODE_COMPLETE, pkt, &res);
		return -1;
	}
	return IEEE1394_RCODE_COMPLETE;
}

/*
 * Mapping between nodeid and unique ID (EUI-64).
 *
 * Track old mappings and simply update their devices with the new id's when
 * they match an existing EUI. This allows proper renumeration of the bus.
 */
void
fwohci_uid_collect(struct fwohci_softc *sc)
{
	int i, count, val, phy_id;
	struct fwohci_uidtbl *fu;
	struct ieee1394_softc *iea;
	u_int32_t *selfid_buf;

	selfid_buf = (u_int32_t *)sc->sc_buf_selfid.fb_buf;

	LIST_FOREACH(iea, &sc->sc_nodelist, sc1394_node)
		iea->sc1394_node_id = 0xFFFF;

	if (sc->sc_uidtbl != NULL) {
		free(sc->sc_uidtbl, M_DEVBUF);
		MPRINTF("free(DEVBUF)", sc->sc_uidtbl);
		sc->sc_uidtbl = NULL;	/* XXX */
	}
#ifdef	M_ZERO
	sc->sc_uidtbl = malloc(sizeof(*fu) * (sc->sc_rootid + 1), M_DEVBUF,
	    M_NOWAIT|M_ZERO);	/* XXX M_WAITOK requires locks. */
	MPRINTF("malloc(DEVBUF)", sc->sc_uidtbl);
#else
	sc->sc_uidtbl = malloc(sizeof(*fu) * (sc->sc_rootid + 1), M_DEVBUF,
	    M_NOWAIT);		/* XXX M_WAITOK requires locks. */
	MPRINTF("malloc(DEVBUF)", sc->sc_uidtbl);
#endif
	if (sc->sc_uidtbl == NULL) {
		DPRINTF(("sc_uidtbl malloc failed."));
		return;
	}
#ifndef	M_ZERO
	bzero(sc->sc_uidtbl, sizeof(*fu) * (sc->sc_rootid + 1));
#endif

	/* Update each node's link speed from SelfID buf. */
	count = OHCI_BITVAL(OHCI_CSR_READ(sc, OHCI_REG_SelfIDCount),
	    OHCI_SelfID_Size);
	for (i = 1; i < count; i += 2) {
		if (selfid_buf[i] & IEEE1394_SELFID_EXTENDED)
			continue;	/* No link speed info in Extended ID. */
		phy_id = OHCI_BITVAL(selfid_buf[i], IEEE1394_PHY_ID);
		if (phy_id > sc->sc_rootid)
			continue;	/* Bogus !!! */
		val = OHCI_BITVAL(selfid_buf[i], IEEE1394_SELFID_SPEED);
		sc->sc_uidtbl[phy_id].fu_link_speed = val;
	}

	for (i = 0, fu = sc->sc_uidtbl; i <= sc->sc_rootid; i++, fu++) {
		if (i == (sc->sc_nodeid & OHCI_NodeId_NodeNumber)) {
			bcopy(sc->sc_sc1394.sc1394_guid, fu->fu_uid, 8);
			fu->fu_valid = 3;

			iea = (struct ieee1394_softc *)sc->sc_sc1394.sc1394_if;
			if (iea) {
				iea->sc1394_node_id = i;
				DPRINTF(("%s: Updating nodeid to %d\n",
				    iea->sc1394_dev.dv_xname,
				    iea->sc1394_node_id));
				if (iea->sc1394_callback.cb1394_busreset) {
					iea->sc1394_callback.cb1394_busreset(
					    iea);
				}
			}
		} else {
			fu->fu_valid = 0;
			fwohci_uid_req(sc, i);
		}
	}
	if (sc->sc_rootid == 0)
		fwohci_check_nodes(sc);
}

void
fwohci_uid_req(struct fwohci_softc *sc, int phyid)
{
	struct fwohci_pkt pkt;

	bzero(&pkt, sizeof(pkt));
	pkt.fp_tcode = IEEE1394_TCODE_READ_REQUEST_QUADLET;
	pkt.fp_hlen = 12;
	pkt.fp_dlen = 0;
	pkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) |
	    (pkt.fp_tcode << 4);
	pkt.fp_hdr[1] = ((0xFFC0 | phyid) << 16) | CSR_BASE_HI;
	pkt.fp_hdr[2] = CSR_BASE_LO + CSR_CONFIG_ROM + 12;
	fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESPONSE_QUADLET, phyid,
	    sc->sc_tlabel, OHCI_NodeId_NodeNumber, fwohci_uid_input, (void *)0);
	sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3F;
	fwohci_at_output(sc, sc->sc_ctx_atrq, &pkt);

	pkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) |
	    (pkt.fp_tcode << 4);
	pkt.fp_hdr[2] = CSR_BASE_LO + CSR_CONFIG_ROM + 16;
	fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESPONSE_QUADLET, phyid,
	    sc->sc_tlabel, OHCI_NodeId_NodeNumber, fwohci_uid_input, (void *)1);
	sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3F;
	fwohci_at_output(sc, sc->sc_ctx_atrq, &pkt);
}

int
fwohci_uid_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *res)
{
	struct fwohci_uidtbl *fu;
	struct ieee1394_softc *iea;
	struct ieee1394_attach_args fwa;
	int i, n, done, rcode, found;

	found = 0;

	n = (res->fp_hdr[1] >> 16) & OHCI_NodeId_NodeNumber;
	rcode = (res->fp_hdr[1] & 0x0000F000) >> 12;
	if (rcode != IEEE1394_RCODE_COMPLETE ||
	    sc->sc_uidtbl == NULL ||
	    n > sc->sc_rootid)
		return 0;
	fu = &sc->sc_uidtbl[n];
	if (arg == 0) {
		bcopy(res->fp_iov[0].iov_base, fu->fu_uid, 4);
		fu->fu_valid |= 0x1;
	} else {
		bcopy(res->fp_iov[0].iov_base, fu->fu_uid + 4, 4);
		fu->fu_valid |= 0x2;
	}
	if (fu->fu_valid == 0x3) {
		DPRINTFN(1, ("%s: Node %d, UID %02x:%02x:%02x:%02x:%02x:%02x:"
		    "%02x:%02x\n", __func__, n,
		    fu->fu_uid[0], fu->fu_uid[1], fu->fu_uid[2], fu->fu_uid[3],
		    fu->fu_uid[4], fu->fu_uid[5], fu->fu_uid[6], fu->fu_uid[7]));
		LIST_FOREACH(iea, &sc->sc_nodelist, sc1394_node) {
			if (memcmp(iea->sc1394_guid, fu->fu_uid, 8) == 0) {
				found = 1;
				iea->sc1394_node_id = n;
				iea->sc1394_link_speed = fu->fu_link_speed;
				DPRINTF(("%s: Updating nodeid to %d, speed %d\n",
				    iea->sc1394_dev.dv_xname,
				    iea->sc1394_node_id,
				    fu->fu_link_speed));
				if (iea->sc1394_callback.cb1394_busreset) {
					iea->sc1394_callback.cb1394_busreset(
					    iea);
				}
				break;
			}
		}
		if (!found) {
			strcpy(fwa.name, "fwnode");
			fwa.link_speed = fu->fu_link_speed;
			bcopy(fu->fu_uid, fwa.uid, 8);
			fwa.nodeid = n;
			fwa.read = fwohci_read;
			fwa.write = fwohci_write;
			fwa.inreg = fwohci_inreg;
			fwa.unreg = fwohci_unreg;
			iea = (struct ieee1394_softc *)
			    config_found_sm(&sc->sc_sc1394.sc1394_dev, &fwa,
			    fwohci_print, fwohci_submatch);
			if (iea != NULL) {
				DPRINTF(("%s: Update speed to %d.",
				    iea->sc1394_dev.dv_xname,
				    fu->fu_link_speed));
				iea->sc1394_link_speed = fu->fu_link_speed;
				LIST_INSERT_HEAD(&sc->sc_nodelist, iea,
				    sc1394_node);
			}
		}
	}
	done = 1;

	for (i = 0; i <= sc->sc_rootid; i++) {
		fu = &sc->sc_uidtbl[i];
		if (fu->fu_valid != 0x3) {
			done = 0;
			break;
		}
	}
	if (done)
		fwohci_check_nodes(sc);

	return 0;
}

void
fwohci_check_nodes(struct fwohci_softc *sc)
{
	struct device *detach = NULL;
	struct ieee1394_softc *iea;

	LIST_FOREACH(iea, &sc->sc_nodelist, sc1394_node) {

		/*
		 * Have to defer detachment until the next
		 * loop iteration since config_detach
		 * free's the softc and the loop iterator
		 * needs data from the softc to move
		 * forward.
		 */

		if (detach) {
//			config_detach_children(detach, 0);
			config_detach(detach, 0);
			detach = NULL;
		}
		if (iea->sc1394_node_id == 0xFFFF) {
			detach = (struct device *)iea;
			LIST_REMOVE(iea, sc1394_node);
		}
	}
	if (detach)  {
//		config_detach_children(detach, 0);
		config_detach(detach, 0);
	}
}

int
fwohci_uid_lookup(struct fwohci_softc *sc, const u_int8_t *uid)
{
	struct fwohci_uidtbl *fu;
	int n;
	static const u_int8_t bcast[] =
	    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	fu = sc->sc_uidtbl;
	if (fu == NULL) {
		if (memcmp(uid, bcast, sizeof(bcast)) == 0)
			return IEEE1394_BCAST_PHY_ID;
		fwohci_uid_collect(sc); /* Try to get. */
		return -1;
	}
	for (n = 0; n <= sc->sc_rootid; n++, fu++) {
		if (fu->fu_valid == 0x3 && memcmp(fu->fu_uid, uid, 8) == 0)
			return n;
	}
	if (memcmp(uid, bcast, sizeof(bcast)) == 0)
		return IEEE1394_BCAST_PHY_ID;
	for (n = 0, fu = sc->sc_uidtbl; n <= sc->sc_rootid; n++, fu++) {
		if (fu->fu_valid != 0x3) {
			/*
			 * XXX: Need timer before retransmission.
			 */
			fwohci_uid_req(sc, n);
		}
	}
	return -1;
}

/*
 * Functions to support network interface.
 */
int
fwohci_if_inreg(struct device *self, u_int32_t offhi, u_int32_t offlo,
    void (*handler)(struct device *, struct mbuf *))
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;

	fwohci_handler_set(sc, IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK,
	    offhi, offlo, OHCI_NodeId_NodeNumber,
	    handler ? fwohci_if_input : NULL, handler);
	fwohci_handler_set(sc, IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK,
	    (sc->sc_csr[CSR_SB_BROADCAST_CHANNEL] & IEEE1394_ISOCH_MASK) |
	    OHCI_ASYNC_STREAM, IEEE1394_TAG_GASP, 0,
	    handler ? fwohci_if_input : NULL, handler);
	return 0;
}

int
fwohci_if_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	int n, len;
	struct mbuf *m;
	struct iovec *iov;
	void (*handler)(struct device *, struct mbuf *) = arg;

#if	defined(FWOHCI_DEBUG) && !defined(SMALL_KERNEL)
	int i;
	DPRINTF(DBG_LOG|DBG_TIME|DBG_FUNC|DBG_L_V1,
	   ("tcode=0x%x, dlen=%d", pkt->fp_tcode, pkt->fp_dlen));
	for (i = 0; (DBG_FLAGS_VAR(fwohci) & DBG_L_BUFFER) &&
	     i < pkt->fp_hlen/4; i++)
		DPRINTF(DBG_LOG|DBG_NOLF|DBG_L_BUFFER,
		    ("%s %08x", i?"":"   ", pkt->fp_hdr[i]));
	DPRINTF(DBG_LOG|DBG_L_BUFFER, (" $"));
	if (pkt->fp_dlen) {
		for (n = 0, len = pkt->fp_dlen; len > 0; len -= i, n++){
			iov = &pkt->fp_iov[n];
			for (i = 0; (DBG_FLAGS_VAR(fwohci) & DBG_L_BUFFER) &&
			     i < iov->iov_len; i++)
				DPRINTF(DBG_LOG|DBG_NOLF|DBG_L_BUFFER,
				   ("%s%02x", i&31?i&3?"":" ":i?"\n    ":"    ",
				    ((u_int8_t *)iov->iov_base)[i]));
			DPRINTF(DBG_LOG|DBG_L_BUFFER, (" $"));
		}
	}
#endif	/* FWOHCI_DEBUG */
	len = pkt->fp_dlen;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return IEEE1394_RCODE_COMPLETE;
	m->m_len = 16;
	if (len + m->m_len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			MPRINTF("m_freem", m);
			return IEEE1394_RCODE_COMPLETE;
		}
	}
	n = (pkt->fp_hdr[1] >> 16) & OHCI_NodeId_NodeNumber;
	if (sc->sc_uidtbl == NULL || n > sc->sc_rootid ||
	    sc->sc_uidtbl[n].fu_valid != 0x3) {
		printf("%s: packet from unknown node: phy id %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, n);
		m_freem(m);
		MPRINTF("m_freem", m);
		fwohci_uid_req(sc, n);
		return IEEE1394_RCODE_COMPLETE;
	}
	bcopy(sc->sc_uidtbl[n].fu_uid, mtod(m, caddr_t), 8);
	if (pkt->fp_tcode == IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK) {
		m->m_flags |= M_BCAST;
		mtod(m, u_int32_t *)[2] = mtod(m, u_int32_t *)[3] = 0;
	} else {
		mtod(m, u_int32_t *)[2] = htonl(pkt->fp_hdr[1]);
		mtod(m, u_int32_t *)[3] = htonl(pkt->fp_hdr[2]);
	}
	mtod(m, u_int8_t *)[8] = n;	/*XXX: Node id for debug. */
	mtod(m, u_int8_t *)[9] = OHCI_BITVAL(*pkt->fp_trail, OHCI_CTXCTL_SPD);

	m->m_pkthdr.rcvif = NULL;	/* set in child */
	m->m_pkthdr.len = len + m->m_len;
	/*
	 * We may use receive buffer by external mbuf instead of copy here.
	 * But asynchronous receive buffer must be operate in buffer fill
	 * mode, so that each receive buffer will shared by multiple mbufs.
	 * If upper layer doesn't free mbuf soon, e.g. application program
	 * is suspended, buffer must be reallocated.
	 * Isochronous buffer must be operate in packet buffer mode, and
	 * it is easy to map receive buffer to external mbuf. But it is
	 * used for broadcast/multicast only, and is expected not so
	 * performance sensitive for now.
	 * XXX: The performance may be important for multicast case,
	 * so we should revisit here later.
	 *						-- onoe
	 */
	n = 0;
	iov = pkt->fp_uio.uio_iov;
	while (len > 0) {
		bcopy(iov->iov_base, mtod(m, caddr_t) + m->m_len,
		    iov->iov_len);
		m->m_len += iov->iov_len;
		len -= iov->iov_len;
		iov++;
	}
	(*handler)(sc->sc_sc1394.sc1394_if, m);
	return IEEE1394_RCODE_COMPLETE;
}

int
fwohci_if_input_iso(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	int n, len;
	int chan, tag;
	struct mbuf *m;
	struct iovec *iov;
	void (*handler)(struct device *, struct mbuf *) = arg;
#if	defined(FWOHCI_DEBUG)
	int i;
#endif	/* FWOHCI_DEBUG */

	chan = (pkt->fp_hdr[0] & 0x00003F00) >> 8;
	tag  = (pkt->fp_hdr[0] & 0x0000C000) >> 14;
#ifdef	FWOHCI_DEBUG
	DPRINTFN(1, ("%s: tcode=0x%x, chan=%d, tag=%x, dlen=%d", __func__,
	    pkt->fp_tcode, chan, tag, pkt->fp_dlen));
	for (i = 0; i < pkt->fp_hlen/4; i++)
		DPRINTFN(2, ("%s%08x", i?" ":"\n\t", pkt->fp_hdr[i]));
	DPRINTFN(2, ("$"));
	if (pkt->fp_dlen) {
		for (n = 0, len = pkt->fp_dlen; len > 0; len -= i, n++){
			iov = &pkt->fp_iov[n];
			for (i = 0; i < iov->iov_len; i++)
				DPRINTFN(2, ("%s%02x",
				    (i%32)?((i%4)?"":" "):"\n\t",
				    ((u_int8_t *)iov->iov_base)[i]));
			DPRINTFN(2, ("$"));
		}
	}
	DPRINTFN(2, ("\n"));
#endif	/* FWOHCI_DEBUG */
	len = pkt->fp_dlen;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return IEEE1394_RCODE_COMPLETE;
	m->m_len = 16;
	if (m->m_len + len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			MPRINTF("m_freem", m);
			return IEEE1394_RCODE_COMPLETE;
		}
	}

	m->m_flags |= M_BCAST;

	if (tag == IEEE1394_TAG_GASP) {
		n = (pkt->fp_hdr[1] >> 16) & OHCI_NodeId_NodeNumber;
		if (sc->sc_uidtbl == NULL || n > sc->sc_rootid ||
		    sc->sc_uidtbl[n].fu_valid != 0x3) {
			printf("%s: packet from unknown node: phy id %d\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname, n);
			m_freem(m);
			MPRINTF("m_freem", m);
			return IEEE1394_RCODE_COMPLETE;
		}
		bcopy(sc->sc_uidtbl[n].fu_uid, mtod(m, caddr_t), 8);
		mtod(m, u_int32_t *)[2] = htonl(pkt->fp_hdr[1]);
		mtod(m, u_int32_t *)[3] = htonl(pkt->fp_hdr[2]);
		mtod(m, u_int8_t *)[8] = n;	/*XXX: node id for debug */
		mtod(m, u_int8_t *)[9] =
		    OHCI_BITVAL(*pkt->fp_trail, OHCI_CTXCTL_SPD);
	}
	mtod(m, u_int8_t *)[14] = chan;
	mtod(m, u_int8_t *)[15] = tag;


	m->m_pkthdr.rcvif = NULL;	/* set in child */
	m->m_pkthdr.len = len + m->m_len;
	/*
	 * We may use receive buffer by external mbuf instead of copy here.
	 * But asynchronous receive buffer must be operate in buffer fill
	 * mode, so that each receive buffer will shared by multiple mbufs.
	 * If upper layer doesn't free mbuf soon, e.g. application program
	 * is suspended, buffer must be reallocated.
	 * Isochronous buffer must be operate in packet buffer mode, and
	 * it is easy to map receive buffer to external mbuf. But it is
	 * used for broadcast/multicast only, and is expected not so
	 * performance sensitive for now.
	 * XXX: The performance may be important for multicast case,
	 * so we should revisit here later.
	 *						-- onoe
	 */
	n = 0;
	iov = pkt->fp_uio.uio_iov;
	while (len > 0) {
		bcopy(iov->iov_base, mtod(m, caddr_t) + m->m_len,
		    iov->iov_len);
		m->m_len += iov->iov_len;
		len -= iov->iov_len;
		iov++;
	}
	(*handler)(sc->sc_sc1394.sc1394_if, m);
	return IEEE1394_RCODE_COMPLETE;
}



int
fwohci_if_output(struct device *self, struct mbuf *m0,
    void (*callback)(struct device *, struct mbuf *))
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;
	struct fwohci_pkt pkt;
	u_int8_t *p;
	int n, error, spd, hdrlen, maxrec;
#ifdef	FWOHCI_DEBUG
	struct mbuf *m;
#endif	/* FWOHCI_DEBUG */

	p = mtod(m0, u_int8_t *);
	if (m0->m_flags & (M_BCAST | M_MCAST)) {
		spd = IEEE1394_SPD_S100;	/*XXX*/
		maxrec = 512;			/*XXX*/
		hdrlen = 8;
	} else {
		n = fwohci_uid_lookup(sc, p);
		if (n < 0) {
			printf("%s: nodeid unknown:"
			    " %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname,
			    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
			error = EHOSTUNREACH;
			goto end;
		}
		if (n == IEEE1394_BCAST_PHY_ID) {
			printf("%s: broadcast with !M_MCAST\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname);
#ifdef	FWOHCI_DEBUG
			DPRINTFN(2, ("packet:"));
			for (m = m0; m != NULL; m = m->m_next) {
				for (n = 0; n < m->m_len; n++)
					DPRINTFN(2, ("%s%02x", (n%32)?
					    ((n%4)?"":" "):"\n    ",
					    mtod(m, u_int8_t *)[n]));
				DPRINTFN(2, ("$"));
			}
			DPRINTFN(2, ("\n"));
#endif	/* FWOHCI_DEBUG */
			error = EHOSTUNREACH;
			goto end;
		}
		maxrec = 2 << p[8];
		spd = p[9];
		hdrlen = 0;
	}
	if (spd > sc->sc_sc1394.sc1394_link_speed) {
		DPRINTF(("%s: spd (%d) is faster than %d\n", __func__,
		    spd, sc->sc_sc1394.sc1394_link_speed));
		spd = sc->sc_sc1394.sc1394_link_speed;
	}
	if (maxrec > (512 << spd)) {
		DPRINTF(("%s: maxrec (%d) is larger for spd (%d)\n", __func__,
		    maxrec, spd));
		maxrec = 512 << spd;
	}
	while (maxrec > sc->sc_sc1394.sc1394_max_receive) {
		DPRINTF(("%s: maxrec (%d) is larger than %d\n", __func__,
		    maxrec, sc->sc_sc1394.sc1394_max_receive));
		maxrec >>= 1;
	}
	if (maxrec < 512) {
		DPRINTF(("%s: maxrec (%d) is smaller than minimum\n",
		    __func__, maxrec));
		maxrec = 512;
	}

	m_adj(m0, 16 - hdrlen);
	if (m0->m_pkthdr.len > maxrec) {
		DPRINTF(("%s: packet too big: hdr %d, pktlen %d, maxrec %d\n",
		    __func__, hdrlen, m0->m_pkthdr.len, maxrec));
		error = E2BIG;	/*XXX*/
		goto end;
	}

	bzero(&pkt, sizeof(pkt));
	pkt.fp_uio.uio_iov = pkt.fp_iov;
	pkt.fp_uio.uio_segflg = UIO_SYSSPACE;
	pkt.fp_uio.uio_rw = UIO_WRITE;
	if (m0->m_flags & (M_BCAST | M_MCAST)) {
		/* Construct GASP header. */
		p = mtod(m0, u_int8_t *);
		p[0] = sc->sc_nodeid >> 8;
		p[1] = sc->sc_nodeid & 0xFF;
		p[2] = 0x00; p[3] = 0x00; p[4] = 0x5E;
		p[5] = 0x00; p[6] = 0x00; p[7] = 0x01;
		pkt.fp_tcode = IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK;
		pkt.fp_hlen = 8;
		pkt.fp_hdr[0] = (spd << 16) | (IEEE1394_TAG_GASP << 14) |
		    ((sc->sc_csr[CSR_SB_BROADCAST_CHANNEL] &
		    OHCI_NodeId_NodeNumber) << 8);
		pkt.fp_hdr[1] = m0->m_pkthdr.len << 16;
	} else {
		pkt.fp_tcode = IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK;
		pkt.fp_hlen = 16;
		pkt.fp_hdr[0] = 0x00800100 | (sc->sc_tlabel << 10) |
		    (spd << 16);
		pkt.fp_hdr[1] =
		    (((sc->sc_nodeid & OHCI_NodeId_BusNumber) | n) << 16) |
		    (p[10] << 8) | p[11];
		pkt.fp_hdr[2] = (p[12]<<24) | (p[13]<<16) | (p[14]<<8) | p[15];
		pkt.fp_hdr[3] = m0->m_pkthdr.len << 16;
		sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3F;
	}
	pkt.fp_hdr[0] |= (pkt.fp_tcode << 4);
	pkt.fp_dlen = m0->m_pkthdr.len;
	pkt.fp_m = m0;
	pkt.fp_callback = callback;
	error = fwohci_at_output(sc, sc->sc_ctx_atrq, &pkt);
	m0 = pkt.fp_m;
  end:
	if (m0 != NULL) {
		if (callback) {
			(*callback)(sc->sc_sc1394.sc1394_if, m0);
		} else {
			m_freem(m0);
			MPRINTF("m_freem", m0);
		}
	}
	return error;
}

/*
 * High level routines to provide abstraction to attaching layers to
 * send/receive data.
 */

/*
 * These break down into 4 routines as follows:
 *
 * int fwohci_read(struct ieee1394_abuf *)
 *
 * This routine will attempt to read a region from the requested node.
 * A callback must be provided which will be called when either the completed
 * read is done or an unrecoverable error occurs. This is mainly a convenience
 * routine since it will encapsulate retrying a region as quadlet vs. block
 * reads and recombining all the returned data. This could also be done with a
 * series of write/inreg's for each packet sent.
 *
 * int fwohci_write(struct ieee1394_abuf *)
 *
 * The work horse main entry point for putting packets on the bus. This is the
 * generalized interface for fwnode/etc code to put packets out onto the bus.
 * It accepts all standard ieee1394 tcodes (XXX: only a few today) and
 * optionally will callback via a func pointer to the calling code with the
 * resulting ACK code from the packet. If the ACK code is to be ignored (i.e.
 * no cb) then the write routine will take care of free'ing the abuf since the
 * fwnode/etc code won't have any knowledge of when to do this. This allows for
 * simple one-off packets to be sent from the upper-level code without worrying
 * about a callback for cleanup.
 *
 * int fwohci_inreg(struct ieee1394_abuf *, int)
 *
 * This is very simple. It evals the abuf passed in and registers an internal
 * handler as the callback for packets received for that operation.
 * The integer argument specifies whether on a block read/write operation to
 * allow sub-regions to be read/written (in block form) as well.
 *
 * XXX: This whole structure needs to be redone as a list of regions and
 * operations allowed on those regions.
 *
 * int fwohci_unreg(struct ieee1394_abuf *, int)
 *
 * This simply unregisters the respective callback done via inreg for items
 * that only need to register an area for a one-time operation (like a status
 * buffer a remote node will write to when the current operation is done). The
 * int argument specifies the same behavior as inreg, except in reverse (i.e.
 * it unregisters).
 */

int
fwohci_read(struct ieee1394_abuf *ab)
{
	struct fwohci_pkt pkt;
	struct ieee1394_softc *sc = ab->ab_req;
	struct fwohci_softc *psc =
	    (struct fwohci_softc *)sc->sc1394_dev.dv_parent;
	struct fwohci_cb *fcb;
	u_int32_t high, lo;
	int rv, tcode;

	/* Have to have a callback when reading. */
	if (ab->ab_cb == NULL)
		return -1;

	MALLOC(fcb, struct fwohci_cb *, sizeof(*fcb), M_DEVBUF, M_WAITOK);
	MPRINTF("MALLOC(DEVBUF)", fcb);
	fcb->ab = ab;
	fcb->count = 0;
	fcb->abuf_valid = 1;

	high = ((ab->ab_addr & 0x0000FFFF00000000) >> 32);
	lo = (ab->ab_addr & 0x00000000FFFFFFFF);

	bzero(&pkt, sizeof(pkt));
	pkt.fp_hdr[1] = ((0xFFC0 | ab->ab_req->sc1394_node_id) << 16) | high;
	pkt.fp_hdr[2] = lo;
	pkt.fp_dlen = 0;

	if (ab->ab_length == 4) {
		pkt.fp_tcode = IEEE1394_TCODE_READ_REQUEST_QUADLET;
		tcode = IEEE1394_TCODE_READ_RESPONSE_QUADLET;
		pkt.fp_hlen = 12;
	} else {
		pkt.fp_tcode = IEEE1394_TCODE_READ_REQUEST_DATABLOCK;
		pkt.fp_hlen = 16;
		tcode = IEEE1394_TCODE_READ_RESPONSE_DATABLOCK;
		pkt.fp_hdr[3] = (ab->ab_length << 16);
	}
	pkt.fp_hdr[0] = 0x00000100 | (sc->sc1394_link_speed << 16) |
	    (psc->sc_tlabel << 10) | (pkt.fp_tcode << 4);

	pkt.fp_statusarg = fcb;
	pkt.fp_statuscb = fwohci_read_resp;

	rv = fwohci_handler_set(psc, tcode, ab->ab_req->sc1394_node_id,
	    psc->sc_tlabel, 0, fwohci_read_resp, fcb);
	if (rv)
		return rv;
	rv = fwohci_at_output(psc, psc->sc_ctx_atrq, &pkt);
	if (rv)
		fwohci_handler_set(psc, tcode, ab->ab_req->sc1394_node_id,
		    psc->sc_tlabel, 0, NULL, NULL);
	psc->sc_tlabel = (psc->sc_tlabel + 1) & 0x3F;
	fcb->count = 1;
	return rv;
}

int
fwohci_write(struct ieee1394_abuf *ab)
{
	struct fwohci_pkt pkt;
	struct ieee1394_softc *sc = ab->ab_req;
	struct fwohci_softc *psc =
	    (struct fwohci_softc *)sc->sc1394_dev.dv_parent;
	u_int32_t high, lo;
	int rv;

	if ((ab->ab_tcode == IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK &&
	     ab->ab_length > IEEE1394_MAX_REC(sc->sc1394_max_receive)) ||
	    ab->ab_length > IEEE1394_MAX_ASYNC(sc->sc1394_link_speed)) {
		DPRINTF(("%s: Packet too large: %d\n", __func__,
		    ab->ab_length));
		return E2BIG;
	}

	if (ab->ab_data && ab->ab_uio) 
		panic("Can't call with uio and data set");
	if ((ab->ab_data == NULL) && (ab->ab_uio == NULL))
		panic("One of either ab_data or ab_uio must be set");

	bzero(&pkt, sizeof(pkt));

	pkt.fp_tcode = ab->ab_tcode;
	if (ab->ab_data) {
		pkt.fp_uio.uio_iov = pkt.fp_iov;
		pkt.fp_uio.uio_segflg = UIO_SYSSPACE;
		pkt.fp_uio.uio_rw = UIO_WRITE;
	} else
		bcopy(ab->ab_uio, &pkt.fp_uio, sizeof(struct uio));

	pkt.fp_statusarg = ab;
	pkt.fp_statuscb = fwohci_write_ack;

	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_WRITE_RESPONSE:
		pkt.fp_hlen = 12;
	case IEEE1394_TCODE_READ_RESPONSE_QUADLET:
	case IEEE1394_TCODE_READ_RESPONSE_DATABLOCK:
		if (!pkt.fp_hlen)
			pkt.fp_hlen = 16;
		high = ab->ab_retlen;
		ab->ab_retlen = 0;
		lo = 0;
		pkt.fp_hdr[0] = 0x00000100 | (sc->sc1394_link_speed << 16) |
		    (ab->ab_tlabel << 10) | (pkt.fp_tcode << 4);
		break;
	default:
		pkt.fp_hlen = 16;
		high = ((ab->ab_addr & 0x0000FFFF00000000) >> 32);
		lo = (ab->ab_addr & 0x00000000FFFFFFFF);
		pkt.fp_hdr[0] = 0x00000100 | (sc->sc1394_link_speed << 16) |
		    (psc->sc_tlabel << 10) | (pkt.fp_tcode << 4);
		break;
	}

	pkt.fp_hdr[1] = ((0xFFC0 | ab->ab_req->sc1394_node_id) << 16) | high;
	pkt.fp_hdr[2] = lo;
	if (pkt.fp_hlen == 16) {
		if (ab->ab_length == 4) {
			pkt.fp_hdr[3] = ab->ab_data[0];
			pkt.fp_dlen = 0;
		}  else {
			pkt.fp_hdr[3] = (ab->ab_length << 16);
			pkt.fp_dlen = ab->ab_length;
			if (ab->ab_data) {
				pkt.fp_uio.uio_iovcnt = 1;
				pkt.fp_uio.uio_resid = ab->ab_length;
				pkt.fp_iov[0].iov_base = ab->ab_data;
				pkt.fp_iov[0].iov_len = ab->ab_length;
			}
		}
	}
	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_WRITE_RESPONSE:
	case IEEE1394_TCODE_READ_RESPONSE_QUADLET:
	case IEEE1394_TCODE_READ_RESPONSE_DATABLOCK:
		rv = fwohci_at_output(psc, psc->sc_ctx_atrs, &pkt);
		break;
	default:
		rv = fwohci_at_output(psc, psc->sc_ctx_atrq, &pkt);
		break;
	}
	return rv;
}

int
fwohci_read_resp(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct fwohci_cb *fcb = arg;
	struct ieee1394_abuf *ab = fcb->ab;
	struct fwohci_pkt newpkt;
	u_int32_t *cur, high, lo;
	int i, tcode, rcode, status, rv;

	/*
	 * Both the ACK handling and normal response callbacks are handled here.
	 * The main reason for this is the various error conditions that can
	 * occur trying to block read some areas and the ways that gets reported
	 * back to calling station. This is a variety of ACK codes, responses,
	 * etc which makes it much more difficult to process if both aren't
	 * handled here.
	 */

	/* Check for status packet. */

	if (pkt->fp_tcode == -1) {
		status = pkt->fp_status & OHCI_DESC_STATUS_ACK_MASK;
		rcode = -1;
		tcode = (pkt->fp_hdr[0] >> 4) & 0xF;
		if ((status != OHCI_CTXCTL_EVENT_ACK_COMPLETE) &&
		    (status != OHCI_CTXCTL_EVENT_ACK_PENDING))
			DPRINTFN(2, ("%s: Got status packet: 0x%02x\n",
			    __func__, (unsigned int)status));
		fcb->count--;

		/*
		 * Got all the ack's back and the buffer is invalid (i.e. the
		 * callback has been called). Clean up.
		 */

		if (fcb->abuf_valid == 0) {
			if (fcb->count == 0) {
				FREE(fcb, M_DEVBUF);
				MPRINTF("FREE(DEVBUF)", fcb);
				fcb = NULL;	/* XXX */
			}
			return IEEE1394_RCODE_COMPLETE;
		}
	} else {
		status = -1;
		tcode = pkt->fp_tcode;
		rcode = (pkt->fp_hdr[1] & 0x0000F000) >> 12;
	}

	/*
	 * Some areas (like the config rom) want to be read as quadlets only.
	 *
	 * The current ideas to try are:
	 *
	 * Got an ACK_TYPE_ERROR on a block read.
	 *
	 * Got either RCODE_TYPE or RCODE_ADDRESS errors in a block read
	 * response.
	 *
	 * In all cases construct a new packet for a quadlet read and let
	 * multi_resp handle the iteration over the space.
	 */

	if (((status == OHCI_CTXCTL_EVENT_ACK_TYPE_ERROR) &&
	     (tcode == IEEE1394_TCODE_READ_REQUEST_DATABLOCK)) ||
	    (((rcode == IEEE1394_RCODE_TYPE_ERROR) ||
	      (rcode == IEEE1394_RCODE_ADDRESS_ERROR)) &&
	     (tcode == IEEE1394_TCODE_READ_RESPONSE_DATABLOCK))) {

		/* Read the area in quadlet chunks (internally track this). */

		bzero(&newpkt, sizeof(newpkt));

		high = ((ab->ab_addr & 0x0000FFFF00000000) >> 32);
		lo = (ab->ab_addr & 0x00000000FFFFFFFF);

		newpkt.fp_tcode = IEEE1394_TCODE_READ_REQUEST_QUADLET;
		newpkt.fp_hlen = 12;
		newpkt.fp_dlen = 0;
		newpkt.fp_hdr[1] =
		    ((0xFFC0 | ab->ab_req->sc1394_node_id) << 16) | high;
		newpkt.fp_hdr[2] = lo;
		newpkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) |
		    (newpkt.fp_tcode << 4);

		rv = fwohci_handler_set(sc,
		    IEEE1394_TCODE_READ_RESPONSE_QUADLET,
		    ab->ab_req->sc1394_node_id, sc->sc_tlabel, 0,
		    fwohci_read_multi_resp, fcb);
		if (rv) {
			(*ab->ab_cb)(ab, -1);
			goto cleanup;
		}
		newpkt.fp_statusarg = fcb;
		newpkt.fp_statuscb = fwohci_read_resp;
		rv = fwohci_at_output(sc, sc->sc_ctx_atrq, &newpkt);
		if (rv) {
			fwohci_handler_set(sc,
			    IEEE1394_TCODE_READ_RESPONSE_QUADLET,
			    ab->ab_req->sc1394_node_id, sc->sc_tlabel, 0,
			    NULL, NULL);
			(*ab->ab_cb)(ab, -1);
			goto cleanup;
		}
		fcb->count++;
		sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3F;
		return IEEE1394_RCODE_COMPLETE;
	} else if ((rcode != -1) || ((status != -1) &&
	    (status != OHCI_CTXCTL_EVENT_ACK_COMPLETE) &&
	    (status != OHCI_CTXCTL_EVENT_ACK_PENDING))) {

		/*
		 * Recombine all the iov data into 1 chunk for higher
		 * level code.
		 */

		if (rcode != -1) {
			cur = ab->ab_data;

			assert(pkt->fp_uio.uio_iovcnt > 0);

			for (i = 0; i < pkt->fp_uio.uio_iovcnt; i++) {
				/*
				 * Make sure and don't exceed the buffer
				 * allocated for return.
				 */
				if ((ab->ab_retlen + pkt->fp_iov[i].iov_len) >
				    ab->ab_length) {
					bcopy(pkt->fp_iov[i].iov_base, cur,
					    (ab->ab_length - ab->ab_retlen));
					ab->ab_retlen = ab->ab_length;
					break;
				}
				bcopy(pkt->fp_iov[i].iov_base, cur,
				    pkt->fp_iov[i].iov_len);
				(caddr_t)cur += pkt->fp_iov[i].iov_len;
				ab->ab_retlen += pkt->fp_iov[i].iov_len;
			}
			DPRINTF(("%s: retlen=%d\n", __func__, ab->ab_retlen));
		}
		if (status != -1)
			/* XXX: Need a complete tlabel interface. */
			for (i = 0; i < 64; i++)
				fwohci_handler_set(sc,
				    IEEE1394_TCODE_READ_RESPONSE_QUADLET,
				    ab->ab_req->sc1394_node_id, i, 0,
				    NULL, NULL);
		(*ab->ab_cb)(ab, rcode);
		goto cleanup;
	} else
		/* Good ack packet. */
		return IEEE1394_RCODE_COMPLETE;

	/* Can't get here unless ab->ab_cb has been called. */

 cleanup:
	fcb->abuf_valid = 0;
	if (fcb->count == 0) {
		FREE(fcb, M_DEVBUF);
		MPRINTF("FREE(DEVBUF)", fcb);
		fcb = NULL;
	}
	return IEEE1394_RCODE_COMPLETE;
}

int
fwohci_read_multi_resp(struct fwohci_softc *sc, void *arg,
    struct fwohci_pkt *pkt)
{
	struct fwohci_cb *fcb = arg;
	struct ieee1394_abuf *ab = fcb->ab;
	struct fwohci_pkt newpkt;
	u_int32_t high, lo;
	int rcode, rv;

	/*
	 * Bad return codes from the wire, just return what's already in the
	 * buf.
	 */

	/* Make sure a response packet didn't arrive after a bad ACK. */
	if (fcb->abuf_valid == 0)
		return IEEE1394_RCODE_COMPLETE;

	rcode = (pkt->fp_hdr[1] & 0x0000F000) >> 12;

	if (rcode) {
		(*ab->ab_cb)(ab, rcode);
		goto cleanup;
	}

	if ((ab->ab_retlen + pkt->fp_iov[0].iov_len) > ab->ab_length) {
		bcopy(pkt->fp_iov[0].iov_base,
		    ((char *)ab->ab_data + ab->ab_retlen),
		    (ab->ab_length - ab->ab_retlen));
		ab->ab_retlen = ab->ab_length;
	} else {
		bcopy(pkt->fp_iov[0].iov_base,
		    ((char *)ab->ab_data + ab->ab_retlen), 4);
		ab->ab_retlen += 4;
	}
	DPRINTF(("%s: retlen=%d\n", __func__, ab->ab_retlen));
	/* Still more, loop and read 4 more bytes. */
	if (ab->ab_retlen < ab->ab_length) {
		bzero(&newpkt, sizeof(newpkt));

		high = ((ab->ab_addr & 0x0000FFFF00000000) >> 32);
		lo = (ab->ab_addr & 0x00000000FFFFFFFF) + ab->ab_retlen;

		newpkt.fp_tcode = IEEE1394_TCODE_READ_REQUEST_QUADLET;
		newpkt.fp_hlen = 12;
		newpkt.fp_dlen = 0;
		newpkt.fp_hdr[1] =
		    ((0xFFC0 | ab->ab_req->sc1394_node_id) << 16) | high;
		newpkt.fp_hdr[2] = lo;
		newpkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) |
		    (newpkt.fp_tcode << 4);

		newpkt.fp_statusarg = fcb;
		newpkt.fp_statuscb = fwohci_read_resp;

		/*
		 * Bad return code. Just give up and return what's
		 * come in now.
		 */
		rv = fwohci_handler_set(sc,
		    IEEE1394_TCODE_READ_RESPONSE_QUADLET,
		    ab->ab_req->sc1394_node_id, sc->sc_tlabel, 0,
		    fwohci_read_multi_resp, fcb);
		if (rv)
			(*ab->ab_cb)(ab, -1);
		else {
			rv = fwohci_at_output(sc, sc->sc_ctx_atrq, &newpkt);
			if (rv) {
				fwohci_handler_set(sc,
				    IEEE1394_TCODE_READ_RESPONSE_QUADLET,
				    ab->ab_req->sc1394_node_id, sc->sc_tlabel,
				    0, NULL, NULL);
				(*ab->ab_cb)(ab, -1);
			} else {
				sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3F;
				fcb->count++;
				return IEEE1394_RCODE_COMPLETE;
			}
		}
	} else
		(*ab->ab_cb)(ab, IEEE1394_RCODE_COMPLETE);

 cleanup:
	/* Can't get here unless ab_cb has been called. */
	fcb->abuf_valid = 0;
	if (fcb->count == 0) {
		FREE(fcb, M_DEVBUF);
		MPRINTF("FREE(DEVBUF)", fcb);
		fcb = NULL;
	}
	return IEEE1394_RCODE_COMPLETE;
}

int
fwohci_write_ack(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct ieee1394_abuf *ab = arg;
	u_int16_t status;


	status = pkt->fp_status & OHCI_DESC_STATUS_ACK_MASK;
	if ((status != OHCI_CTXCTL_EVENT_ACK_COMPLETE) &&
	    (status != OHCI_CTXCTL_EVENT_ACK_PENDING))
		DPRINTF(("%s: Got status packet: 0x%02x\n", __func__,
		    (unsigned int)status));

	/* No callback means this level should free the buffers. */
	if (ab->ab_cb)
		(*ab->ab_cb)(ab, status);
	else {
		if (ab->ab_data) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;	/* XXX */
		}
		FREE(ab, M_1394DATA);
		MPRINTF("FREE(1394DATA)", ab);
		ab = NULL;	/* XXX */
	}
	return IEEE1394_RCODE_COMPLETE;
}

int
fwohci_inreg(struct ieee1394_abuf *ab, int allow)
{
	struct ieee1394_softc *sc = ab->ab_req;
	struct fwohci_softc *psc =
	    (struct fwohci_softc *)sc->sc1394_dev.dv_parent;
	u_int32_t high, lo;
	int rv;

	high = ((ab->ab_addr & 0x0000FFFF00000000) >> 32);
	lo = (ab->ab_addr & 0x00000000FFFFFFFF);

	rv = 0;
	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_READ_REQUEST_QUADLET:
	case IEEE1394_TCODE_WRITE_REQUEST_QUADLET:
		if (ab->ab_cb)
			rv = fwohci_handler_set(psc, ab->ab_tcode, high, lo,
			    sc->sc1394_node_id, fwohci_parse_input, ab);
		else
			fwohci_handler_set(psc, ab->ab_tcode, high, lo,
			    sc->sc1394_node_id, NULL, NULL);
		break;
	case IEEE1394_TCODE_READ_REQUEST_DATABLOCK:
	case IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK:
		if (allow) {
			if (ab->ab_cb)
				rv = fwohci_block_handler_set(psc, ab->ab_tcode,
				    high, lo, sc->sc1394_node_id, ab->ab_length,
				    fwohci_parse_input, ab);
			else
				fwohci_block_handler_set(psc, ab->ab_tcode,
				    high, lo, sc->sc1394_node_id, ab->ab_length,
				    NULL, NULL);
			/*
			 * XXX: Need something to indicate writing a smaller
			 * amount is ok.
			 */
			if (ab->ab_cb)
				ab->ab_data = (void *)1;
		} else {
			if (ab->ab_cb)
				rv = fwohci_handler_set(psc, ab->ab_tcode,
				    high, lo, sc->sc1394_node_id,
				    fwohci_parse_input, ab);
			else
				fwohci_handler_set(psc, ab->ab_tcode, high, lo,
				    sc->sc1394_node_id, NULL, NULL);
		}
		break;
	default:
		DPRINTF(("%s: Invalid registration tcode: %d\n", __func__,
		    ab->ab_tcode));
		return -1;
		break;
	}
	return rv;
}

int
fwohci_unreg(struct ieee1394_abuf *ab, int allow)
{
	void *save;
	int rv;

	save = ab->ab_cb;
	ab->ab_cb = NULL;
	rv = fwohci_inreg(ab, allow);
	ab->ab_cb = save;
	return rv;
}

int
fwohci_parse_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct ieee1394_abuf *ab = (struct ieee1394_abuf *)arg;
	u_int64_t addr;
	u_int32_t *cur;
	int i, count;

	ab->ab_tcode = (pkt->fp_hdr[0] >> 4) & 0xF;
	ab->ab_tlabel = (pkt->fp_hdr[0] >> 10) & 0x3F;
	addr = (((u_int64_t)(pkt->fp_hdr[1] & 0xFFFF) << 32) | pkt->fp_hdr[2]);

	DPRINTFN(3, ("%s: ab=0x%08x ab_cb=0x%08x\n\ttcode=%d tlabel=0x%02x"
	    " addr=%04x%08x\n", __func__, (u_int32_t)ab, (u_int32_t)ab->ab_cb,
	    ab->ab_tcode, ab->ab_tlabel, pkt->fp_hdr[1] & 0xffff,
	    pkt->fp_hdr[2]));

	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_READ_REQUEST_QUADLET:
		ab->ab_retlen = 4;
		break;
	case IEEE1394_TCODE_READ_REQUEST_DATABLOCK:
		ab->ab_retlen = (pkt->fp_hdr[3] >> 16) & 0xFFFF;
		if ((ab->ab_retlen > ab->ab_length) ||
		    ((addr + ab->ab_retlen) > (ab->ab_addr + ab->ab_length)))
			return IEEE1394_RCODE_ADDRESS_ERROR;

		if ((caddr_t)ab->ab_data > (caddr_t)1) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;
		}
		break;
	case IEEE1394_TCODE_WRITE_REQUEST_QUADLET:
		ab->ab_retlen = 4;
	case IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK:
		if (!ab->ab_retlen)
			ab->ab_retlen = (pkt->fp_hdr[3] >> 16) & 0xFFFF;
		else {
#ifdef	FWOHCI_DEBUG
			if (ab->ab_retlen != ((pkt->fp_hdr[3] >> 16) & 0xFFFF))
				DPRINTF(("%s: retlen(%d) <> pktlen(%d)\n",
				    __func__, ab->ab_retlen,
				    (pkt->fp_hdr[3] >> 16) & 0xFFFF));
#endif	/* FWOHCI_DEBUG */
#if 0
			ab->ab_retlen = (pkt->fp_hdr[3] >> 16) & 0xFFFF;
#endif
		}
		if ((ab->ab_retlen > ab->ab_length) ||
		    ((addr + ab->ab_retlen) > (ab->ab_addr + ab->ab_length)))
			return IEEE1394_RCODE_ADDRESS_ERROR;

		if ((caddr_t)ab->ab_data > (caddr_t)1) {
			free(ab->ab_data, M_1394DATA);
			MPRINTF("free(1394DATA)", ab->ab_data);
			ab->ab_data = NULL;
		}
		ab->ab_data = malloc(ab->ab_retlen, M_1394DATA, M_WAITOK);
		MPRINTF("malloc(1394DATA)", ab->ab_data);

		if (ab->ab_tcode == IEEE1394_TCODE_WRITE_REQUEST_QUADLET)
			ab->ab_data[0] = pkt->fp_hdr[3];
		else {
			count = 0;
			cur = ab->ab_data;

			assert(pkt->fp_uio.uio_iovcnt > 0);

			for (i = 0; i < pkt->fp_uio.uio_iovcnt; i++) {
				DPRINTFN(3, ("\t%d : bcopy(0x%08x, 0x%08x,"
				    " 0x%x)\n", i,
				    (u_int32_t)pkt->fp_iov[i].iov_base,
				    (u_int32_t)cur, pkt->fp_iov[i].iov_len));
				bcopy(pkt->fp_iov[i].iov_base, cur,
				    pkt->fp_iov[i].iov_len);
				(caddr_t)cur += pkt->fp_iov[i].iov_len;
				count += pkt->fp_iov[i].iov_len;
			}
			if (ab->ab_retlen != count)
				panic("Packet claims %d length "
				    "but %d bytes returned",
				    ab->ab_retlen, count);
		}
		break;
	default:
		panic("Got a callback for a tcode that wasn't requested: %d",
		    ab->ab_tcode);
		break;
	}
	ab->ab_addr = addr;
	ab->ab_cb(ab, IEEE1394_RCODE_COMPLETE);
	return -1;
}

#ifdef	__NetBSD__
int
fwohci_submatch(struct device *parent, struct cfdata *cf, void *aux)
#else
int
fwohci_submatch(struct device *parent, void *vcf, void *aux)
#endif
{
	struct ieee1394_attach_args *fwa = aux;
#ifdef	__OpenBSD__
	struct cfdata *cf = (struct cfdata *)vcf;
#endif

	/* Both halves must be filled in for a match. */
	if ((cf->fwbuscf_idhi == FWBUS_UNK_IDHI &&
	    cf->fwbuscf_idlo == FWBUS_UNK_IDLO) ||
	    (cf->fwbuscf_idhi == ntohl(*((u_int32_t *)&fwa->uid[0])) &&
	    cf->fwbuscf_idlo == ntohl(*((u_int32_t *)&fwa->uid[4]))))
		return ((*cf->cf_attach->ca_match)(parent, cf, aux));
	return 0;
}

int
fwohci_detach(struct fwohci_softc *sc, int flags)
{
	int rv = 0;

	*sc->sc_dying = 1;	/* Stop the event thread. */
	wakeup(fwohci_event_thread);
	DPRINTF(("%s: waiting 0x%08x\n", __func__, sc->sc_dying));
	tsleep(sc->sc_dying, PZERO, "detach", 3 * hz);
	DPRINTF(("%s: woken up...\n", __func__));
	FREE(sc->sc_dying, M_DEVBUF);
	MPRINTF("FREE(DEVBUF)", sc->sc_dying);
	sc->sc_dying = NULL;	/* XXX */

	if (sc->sc_sc1394.sc1394_if != NULL) {
		rv = config_detach_children(sc->sc_sc1394.sc1394_if, flags);
		rv |= config_detach(sc->sc_sc1394.sc1394_if, flags);
	}
	if (rv)
		return (rv);

#ifdef	__NetBSD__
	callout_stop(&sc->sc_selfid_callout);
#else
	timeout_del(&sc->sc_selfid_callout);
#endif

	if (sc->sc_powerhook != NULL)
		powerhook_disestablish(sc->sc_powerhook);
	if (sc->sc_shutdownhook != NULL)
		shutdownhook_disestablish(sc->sc_shutdownhook);

	return (rv);
}

int
fwohci_activate(struct device *self, enum devact act)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;
	int s, rv = 0;

	s = splhigh();
	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_sc1394.sc1394_if != NULL)
			rv = config_deactivate(sc->sc_sc1394.sc1394_if);
		break;
	}
	splx(s);

	return (rv);
}

#ifdef	FWOHCI_DEBUG
void
fwohci_show_intr(struct fwohci_softc *sc, u_int32_t intmask)
{

	DPRINTF(("%s: intmask=0x%08x:", sc->sc_sc1394.sc1394_dev.dv_xname,
	    intmask));
	if (intmask & OHCI_Int_CycleTooLong)
		DPRINTF((" CycleTooLong"));
	if (intmask & OHCI_Int_UnrecoverableError)
		DPRINTF((" UnrecoverableError"));
	if (intmask & OHCI_Int_CycleInconsistent)
		DPRINTF((" CycleInconsistent"));
	if (intmask & OHCI_Int_BusReset)
		DPRINTF((" BusReset"));
	if (intmask & OHCI_Int_SelfIDComplete)
		DPRINTF((" SelfIDComplete"));
	if (intmask & OHCI_Int_LockRespErr)
		DPRINTF((" LockRespErr"));
	if (intmask & OHCI_Int_PostedWriteErr)
		DPRINTF((" PostedWriteErr"));
	if (intmask & OHCI_Int_ReqTxComplete)
		DPRINTF((" ReqTxComplete(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_REQUEST,
		    OHCI_SUBREG_ContextControlClear)));
	if (intmask & OHCI_Int_RespTxComplete)
		DPRINTF((" RespTxComplete(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear)));
	if (intmask & OHCI_Int_ARRS)
		DPRINTF((" ARRS(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear)));
	if (intmask & OHCI_Int_ARRQ)
		DPRINTF((" ARRQ(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_REQUEST,
		    OHCI_SUBREG_ContextControlClear)));
	if (intmask & OHCI_Int_IsochRx)
		DPRINTF((" IsochRx(0x%08x)",
		    OHCI_CSR_READ(sc, OHCI_REG_IsoRecvIntEventClear)));
	if (intmask & OHCI_Int_IsochTx)
		DPRINTF((" IsochTx(0x%08x)",
		    OHCI_CSR_READ(sc, OHCI_REG_IsoXmitIntEventClear)));
	if (intmask & OHCI_Int_RQPkt)
		DPRINTF((" RQPkt(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_REQUEST,
		    OHCI_SUBREG_ContextControlClear)));
	if (intmask & OHCI_Int_RSPkt)
		DPRINTF((" RSPkt(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear)));
	DPRINTF(("\n"));
}

void
fwohci_show_phypkt(struct fwohci_softc *sc, u_int32_t val)
{
	u_int8_t key, phyid;

	key = OHCI_BITVAL(val, IEEE1394_PHY_TYPE);
	phyid = OHCI_BITVAL(val, IEEE1394_PHY_ID);
	DPRINTF(("%s: PHY packet from %d: ",
	    sc->sc_sc1394.sc1394_dev.dv_xname, phyid));
	switch (key) {
	case 0:
		DPRINTF(("PHY Config:"));
		if (val & IEEE1394_CONFIG_FORCE_ROOT)
			DPRINTF((" ForceRoot"));
		if (val & IEEE1394_CONFIG_SET_GAPCNT)
			DPRINTF((" Gap=%x",
			    OHCI_BITVAL(val, IEEE1394_CONFIG_GAPCNT)));
		DPRINTF(("\n"));
		break;
	case 1:
		DPRINTF(("Link-on\n"));
		break;
	case 2:
		DPRINTF(("SelfID:"));
		if (val & IEEE1394_SELFID_EXTENDED) {
			DPRINTF((" #%d",
			    OHCI_BITVAL(val, IEEE1394_SELFID_EXT_SEQ)));
		} else {
			if (val & IEEE1394_SELFID_LINK_ACTIVE)
				DPRINTF((" LinkActive"));
			DPRINTF((" Gap=%x",
			    OHCI_BITVAL(val, IEEE1394_SELFID_GAPCNT)));
			DPRINTF((" Spd=S%d",
			    100 << OHCI_BITVAL(val, IEEE1394_SELFID_SPEED)));
			DPRINTF((" Pow=%s", ieee1394_power[OHCI_BITVAL(val,
			     IEEE1394_SELFID_POWER)]));
			if (val & IEEE1394_SELFID_CONTENDER)
				DPRINTF((" Cont"));
			if (val & IEEE1394_SELFID_INITIATED_RESET)
				DPRINTF((" InitiateBusReset"));
		}
		if (val & IEEE1394_SELFID_MORE_PACKETS)
			DPRINTF((" +"));
		DPRINTF(("\n"));
		break;
	default:
		DPRINTF(("unknown: 0x%08x\n", val));
		break;
	}
}
#endif	/* FWOHCI_DEBUG */
