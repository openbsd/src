/*	$NetBSD: pdqvar.h,v 1.2 1995/08/19 04:35:22 cgd Exp $	*/

/*-
 * Copyright (c) 1995 Matt Thomas (thomas@lkg.dec.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * DEC PDQ FDDI Controller; PDQ O/S dependent definitions
 *
 * Written by Matt Thomas
 */

#ifndef _PDQ_OS_H
#define	_PDQ_OS_H

#define	PDQ_OS_TX_TIMEOUT		5	/* seconds */

#if defined(PDQTEST)
#include <pdq_os_test.h>
#elif defined(__FreeBSD__) || defined(__bsdi__) || defined(__NetBSD__)

#include <sys/param.h>
#ifndef M_MCAST
#include <sys/mbuf.h>
#endif /* M_CAST */
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#define	PDQ_USE_MBUFS
#define	PDQ_OS_PREFIX			"%s%d: "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name, pdq->pdq_unit

#define	PDQ_OS_PAGESIZE			NBPG
#define	PDQ_OS_USEC_DELAY(n)		DELAY(n)
#define	PDQ_OS_MEMZERO(p, n)		bzero((caddr_t)(p), (n))
#define	PDQ_OS_VA_TO_PA(p)		vtophys(p)
#define	PDQ_OS_MEMALLOC(n)		malloc(n, M_DEVBUF, M_NOWAIT)
#define	PDQ_OS_MEMFREE(p, n)		free((void *) p, M_DEVBUF)
#ifdef __FreeBSD__
#define	PDQ_OS_MEMALLOC_CONTIG(n)	vm_page_alloc_contig(n, 0, 0xffffffff, PAGE_SIZE)
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	kmem_free(kernel_map, (vm_offset_t) p, n)
#else
#define	PDQ_OS_MEMALLOC_CONTIG(n)	kmem_alloc(kernel_map, round_page(n))
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	kmem_free(kernel_map, (vm_offset_t) p, n)
#endif /* __FreeBSD__ */

#if !defined(PDQ_HWSUPPORT)
#if defined(__FreeBSD__)
#include <machine/cpufunc.h>
typedef void ifnet_ret_t;
typedef int ioctl_cmd_t;
#elif defined(__bsdi__)
#include <machine/inline.h>
typedef int ifnet_ret_t;
typedef int ioctl_cmd_t;
#elif defined(__NetBSD__)
typedef void ifnet_ret_t;
typedef u_long ioctl_cmd_t;
#endif
#define PDQ_OS_IORD_32(port)		inl(port)
#define PDQ_OS_IOWR_32(port, data)	outl(port, data)
#define PDQ_OS_IORD_8(port)		inb(port)
#define PDQ_OS_IOWR_8(port, data)	outb(port, data)

typedef struct {
#ifdef __bsdi__
    struct device sc_dev;		/* base device */
    struct isadev sc_id;		/* ISA device */
    struct intrhand sc_ih;		/* intrrupt vectoring */
    struct atshutdown sc_ats;		/* shutdown routine */
#elif defined(__NetBSD__)
    struct device sc_dev;		/* base device */
    void *sc_ih;			/* intrrupt vectoring */
    void *sc_ats;			/* shutdown hook */
#endif
    struct arpcom sc_ac;
    pdq_t *sc_pdq;
    unsigned sc_iobase;
} pdq_softc_t;

#define	sc_if		sc_ac.ac_if
#define	sc_bpf		sc_if.if_bpf

extern void pdq_ifreset(pdq_softc_t *sc);
extern void pdq_ifinit(pdq_softc_t *sc);
extern void pdq_ifwatchdog(pdq_softc_t *sc);
extern ifnet_ret_t pdq_ifstart(struct ifnet *ifp);
extern int pdq_ifioctl(struct ifnet *ifp, ioctl_cmd_t cmd, caddr_t data);
extern void pdq_ifattach(pdq_softc_t *sc, ifnet_ret_t (*ifinit)(int unit),
			 ifnet_ret_t (*ifwatchdog)(int unit));
#endif /* PDQ_HWSUPPORT */
#elif defined(DLPI_PDQ)
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/stream.h>

#define	PDQ_USE_STREAMS
#define	PDQ_OS_PREFIX			"%s board %d "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name, pdq->pdq_unit

#define	PDQ_OS_PAGESIZE			PAGESIZE
#define	PDQ_OS_USEC_DELAY(n)		drv_usecwait(n)
#define	PDQ_OS_MEMZERO(p, n)		bzero((caddr_t)(p), (n))
#define	PDQ_OS_VA_TO_PA(p)		vtop((caddr_t)p, NULL)
#define	PDQ_OS_MEMALLOC(n)		kmem_zalloc(n, KM_NOSLEEP)
#define	PDQ_OS_MEMFREE(p, n)		kmem_free((caddr_t) p, n)
#define	PDQ_OS_MEMALLOC_CONTIG(n)	kmem_zalloc_physreq(n, decfddiphysreq_db, KM_NOSLEEP)
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	PDQ_OS_MEMFREE(p, n)

extern physreq_t *decfddiphysreq_db;
extern physreq_t *decfddiphysreq_mblk;

#define	PDQ_OS_DATABUF_ALLOC(b)		((void) (((b) = allocb_physreq(PDQ_OS_DATABUF_SIZE, BPRI_MED, decfddiphysreq_mblk)) && ((b)->b_wptr = (b)->b_rptr + PDQ_OS_DATABUF_SIZE)))

#define PDQ_OS_IORD_8(port)		inb(port)
#define PDQ_OS_IOWR_8(port, data)	outb(port, data)
#endif


#ifdef PDQ_USE_MBUFS
#define	PDQ_OS_DATABUF_SIZE			(MCLBYTES)
#define	PDQ_OS_DATABUF_FREE(b)			(m_freem(b))
#define	PDQ_OS_DATABUF_NEXT(b)			((b)->m_next)
#define	PDQ_OS_DATABUF_NEXT_SET(b, b1)		((b)->m_next = (b1))
#define	PDQ_OS_DATABUF_NEXTPKT(b)		((b)->m_nextpkt)
#define	PDQ_OS_DATABUF_NEXTPKT_SET(b, b1)	((b)->m_nextpkt = (b1))
#define	PDQ_OS_DATABUF_LEN(b)			((b)->m_len)
#define	PDQ_OS_DATABUF_LEN_SET(b, n)		((b)->m_len = (n))
/* #define	PDQ_OS_DATABUF_LEN_ADJ(b, n)		((b)->m_len += (n)) */
#define	PDQ_OS_DATABUF_PTR(b)			(mtod((b), pdq_uint8_t *))
#define	PDQ_OS_DATABUF_ADJ(b, n)		((b)->m_data += (n), (b)->m_len -= (n))
typedef struct mbuf PDQ_OS_DATABUF_T;

#define	PDQ_OS_DATABUF_ALLOC(b) do { \
    PDQ_OS_DATABUF_T *x_m0; \
    MGETHDR(x_m0, M_DONTWAIT, MT_DATA); \
    if (x_m0 != NULL) { \
	MCLGET(x_m0, M_DONTWAIT);	\
	if ((x_m0->m_flags & M_EXT) == 0) { \
	    m_free(x_m0); \
	    (b) = NULL; \
	} else { \
	    (b) = x_m0; \
	    x_m0->m_len = PDQ_OS_DATABUF_SIZE; \
	} \
    } else { \
	(b) = NULL; \
    } \
} while (0)
#define	PDQ_OS_DATABUF_RESET(b)	((b)->m_data = (b)->m_ext.ext_buf, (b)->m_len = MCLBYTES)
#endif /* PDQ_USE_MBUFS */

#ifdef PDQ_USE_STREAMS
#define	PDQ_OS_DATABUF_SIZE			(2048)
#define	PDQ_OS_DATABUF_FREE(b)			(freemsg(b))
#define	PDQ_OS_DATABUF_NEXT(b)			((b)->b_cont)
#define	PDQ_OS_DATABUF_NEXT_SET(b, b1)		((b)->b_cont = (b1))
#define	PDQ_OS_DATABUF_NEXTPKT(b)		((b)->b_next)
#define	PDQ_OS_DATABUF_NEXTPKT_SET(b, b1)	((b)->b_next = (b1))
#define	PDQ_OS_DATABUF_LEN(b)			((b)->b_wptr - (b)->b_rptr)
#define	PDQ_OS_DATABUF_LEN_SET(b, n)		((b)->b_wptr = (b)->b_rptr + (n))
/*#define	PDQ_OS_DATABUF_LEN_ADJ(b, n)		((b)->b_wptr += (n))*/
#define	PDQ_OS_DATABUF_PTR(b)			((pdq_uint8_t *) (b)->b_rptr)
#define	PDQ_OS_DATABUF_ADJ(b, n)		((b)->b_rptr += (n))
typedef mblk_t PDQ_OS_DATABUF_T;

#ifndef	PDQ_OS_DATABUF_ALLOC
#define	PDQ_OS_DATABUF_ALLOC(b)			((void) (((b) = allocb(PDQ_OS_DATABUF_SIZE, BPRI_MED)) && ((b)->b_wptr = (b)->b_rptr + PDQ_OS_DATABUF_SIZE)))
#endif /* PDQ_OS_DATABUF_ALLOC */
#endif /* PDQ_USE_STREAMS */

#define	PDQ_OS_TX_TRANSMIT		5

#define	PDQ_OS_DATABUF_ENQUEUE(q, b)	do { \
    PDQ_OS_DATABUF_NEXTPKT_SET(b, NULL); \
    if ((q)->q_tail == NULL) \
	(q)->q_head = (b); \
    else \
	PDQ_OS_DATABUF_NEXTPKT_SET(((PDQ_OS_DATABUF_T *)(q)->q_tail), b); \
    (q)->q_tail = (b); \
} while (0)

#define	PDQ_OS_DATABUF_DEQUEUE(q, b)	do { \
    if (((b) = (PDQ_OS_DATABUF_T *) (q)->q_head) != NULL) { \
	if (((q)->q_head = PDQ_OS_DATABUF_NEXTPKT(b)) == NULL) \
	    (q)->q_tail = NULL; \
	PDQ_OS_DATABUF_NEXTPKT_SET(b, NULL); \
    } \
} while (0)

extern void pdq_os_addr_fill(pdq_t *pdq, pdq_lanaddr_t *addrs, size_t numaddrs);
extern void pdq_os_receive_pdu(pdq_t *, PDQ_OS_DATABUF_T *pdu, size_t pdulen);
extern void pdq_os_restart_transmitter(pdq_t *pdq);
extern void pdq_os_transmit_done(pdq_t *pdq, PDQ_OS_DATABUF_T *pdu);

extern void pdq_print_fddi_chars(pdq_t *pdq, const pdq_response_status_chars_get_t *rsp);

extern void pdq_init_csrs(pdq_csrs_t *csrs, void *csrs_va, size_t csr_size);
extern void pdq_init_pci_csrs(pdq_pci_csrs_t *csrs, void *csrs_va, size_t csr_size);

extern void pdq_flush_databuf_queue(pdq_databuf_queue_t *q);

extern pdq_boolean_t pdq_do_port_control(const pdq_csrs_t * const csrs, pdq_uint32_t cmd);
extern void pdq_read_mla(const pdq_csrs_t * const csrs, pdq_lanaddr_t *hwaddr);
extern void pdq_read_fwrev(const pdq_csrs_t * const csrs, pdq_fwrev_t *fwrev);
extern pdq_boolean_t pdq_read_error_log(pdq_t *pdq, pdq_response_error_log_get_t *log_entry);
extern pdq_chip_rev_t pdq_read_chiprev(const pdq_csrs_t * const csrs);

extern void pdq_queue_commands(pdq_t *pdq);
extern void pdq_process_command_responses(pdq_t *pdq);
extern void pdq_process_unsolicited_events(pdq_t *pdq);

extern void pdq_process_received_data(pdq_t *pdq, pdq_rx_info_t *rx,
				      pdq_rxdesc_t *receives,
				      pdq_uint32_t completion_goal,
				      pdq_uint32_t ring_mask);

extern pdq_boolean_t pdq_queue_transmit_data(pdq_t *pdq, PDQ_OS_DATABUF_T *pdu);
extern void pdq_process_transmitted_data(pdq_t *pdq);
extern void pdq_flush_transmitter(pdq_t *pdq);


extern void pdq_hwreset(pdq_t *pdq);
extern pdq_state_t pdq_stop(pdq_t *pdq);
extern void pdq_run(pdq_t *pdq);

extern int pdq_interrupt(pdq_t *pdq);
extern pdq_t *pdq_initialize(void *csr_va, const char *name, int unit, void *ctx, pdq_type_t type);

#endif /* _PDQ_OS_H */
