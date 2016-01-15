/*	$OpenBSD: xenvar.h,v 1.21 2016/01/15 18:20:41 mikeb Exp $	*/

/*
 * Copyright (c) 2015 Mike Belopuhov
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _XENVAR_H_
#define _XENVAR_H_

/* #define XEN_DEBUG */

#ifdef XEN_DEBUG
#define DPRINTF(x...)		printf(x)
#else
#define DPRINTF(x...)
#endif

struct xen_intsrc {
	SLIST_ENTRY(xen_intsrc)	  xi_entry;
	void			(*xi_handler)(void *);
	void			 *xi_arg;
	struct evcount		  xi_evcnt;
	evtchn_port_t		  xi_port;
	short			  xi_noclose;
	short			  xi_masked;
};

struct xen_gntent {
	SLIST_ENTRY(xen_gntent)	 ge_entry;
	grant_entry_t		*ge_table;
	grant_ref_t		 ge_start;
	short			 ge_reserved;
	short			 ge_next;
	short			 ge_free;
	struct mutex		 ge_mtx;
};

struct xen_gntmap {
	grant_ref_t		 gm_ref;
	paddr_t			 gm_paddr;
};

struct xen_softc {
	struct device		 sc_dev;
	uint32_t		 sc_base;
	void			*sc_hc;
	uint32_t		 sc_features;
#define  XENFEAT_CBVEC		(1<<8)

	struct shared_info	*sc_ipg;	/* HYPERVISOR_shared_info */

	uint32_t		 sc_flags;
#define  XSF_CBVEC		 0x0001
#define  XSF_UNPLUG_NIC		 0x0002		/* disable emul. NICs */
#define  XSF_UNPLUG_IDE		 0x0004		/* disable emul. primary IDE */
#define  XSF_UNPLUG_IDESEC	 0x0008		/* disable emul. sec. IDE */

	uint64_t		 sc_irq;	/* IDT vector number */
	struct evcount		 sc_evcnt;	/* upcall counter */
	SLIST_HEAD(, xen_intsrc) sc_intrs;

	SLIST_HEAD(, xen_gntent) sc_gnts;	/* grant table entries */
	int			 sc_gntcnt;	/* number of allocated frames */

	/*
	 * Xenstore
	 */
	struct xs_softc		*sc_xs;		/* xenstore softc */
};

extern struct xen_softc *xen_sc;

struct xen_attach_args {
	void			*xa_parent;
	char			 xa_name[16];
	char			 xa_node[64];
	char			 xa_backend[128];
	bus_dma_tag_t		 xa_dmat;
};

/*
 * Grant table references don't convey the information about an actual
 * offset of the data within the page, however Xen needs to know it.
 * We (ab)use bus_dma_segment's _ds_boundary member to store it.  Please
 * note that we don't save or restore it's original value atm because
 * neither i386 nor amd64 bus_dmamap_unload(9) code needs it.
 */
#define ds_offset		 _ds_boundary

/*
 *  Hypercalls
 */
#define XC_MEMORY		12
#define XC_VERSION		17
#define XC_GNTTAB		20
#define XC_EVTCHN		32
#define XC_HVM			34

int	xen_hypercall(struct xen_softc *, int, int, ...);
int	xen_hypercallv(struct xen_softc *, int, int, ulong *);

/*
 *  Interrupts
 */
typedef uint32_t xen_intr_handle_t;

void	xen_intr(void);
void	xen_intr_ack(void);
void	xen_intr_signal(xen_intr_handle_t);
int	xen_intr_establish(evtchn_port_t, xen_intr_handle_t *, void (*)(void *),
	    void *, char *);
int	xen_intr_disestablish(xen_intr_handle_t);
void	xen_intr_enable(void);
void	xen_intr_mask(xen_intr_handle_t);
int	xen_intr_unmask(xen_intr_handle_t);

/*
 *  XenStore
 */
#define XS_LIST			0x01
#define XS_READ			0x02
#define XS_TOPEN		0x06
#define XS_TCLOSE		0x07
#define XS_WRITE		0x0b
#define XS_RM			0x0d
#define XS_ERROR		0x10
#define XS_MAX			0x16

struct xs_transaction {
	uint32_t		 xst_id;
	uint32_t		 xst_flags;
#define XST_POLL		0x0001
	struct xs_softc		*xst_sc;
};

int	xs_cmd(struct xs_transaction *, int, const char *, struct iovec **,
	    int *);
int	xs_getprop(struct xen_attach_args *, const char *, char *, int);
int	xs_setprop(struct xen_attach_args *, const char *, char *, int);
void	xs_resfree(struct xs_transaction *, struct iovec *, int);

#endif	/* _XENVAR_H_ */
