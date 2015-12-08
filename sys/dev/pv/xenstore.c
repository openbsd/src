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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/mutex.h>

#include <uvm/uvm_extern.h>

#include <dev/pv/xenreg.h>
#include <dev/pv/xenvar.h>

/*
 * The XenStore interface is a simple storage system that is a means of
 * communicating state and configuration data between the Xen Domain 0
 * and the various guest domains.  All configuration data other than
 * a small amount of essential information required during the early
 * boot process of launching a Xen aware guest, is managed using the
 * XenStore.
 *
 * The XenStore is ASCII string based, and has a structure and semantics
 * similar to a filesystem.  There are files and directories that are
 * able to contain files or other directories.  The depth of the hierachy
 * is only limited by the XenStore's maximum path length.
 *
 * The communication channel between the XenStore service and other
 * domains is via two, guest specific, ring buffers in a shared memory
 * area.  One ring buffer is used for communicating in each direction.
 * The grant table references for this shared memory are given to the
 * guest via HVM hypercalls.
 *
 * The XenStore communication relies on an event channel and thus
 * interrupts. Several Xen services depend on the XenStore, most
 * notably the XenBus used to discover and manage Xen devices.
 */

const struct
{
	const char		*xse_errstr;
	int			 xse_errnum;
} xs_errors[] = {
	{ "EINVAL",	EINVAL },
	{ "EACCES",	EACCES },
	{ "EEXIST",	EEXIST },
	{ "EISDIR",	EISDIR },
	{ "ENOENT",	ENOENT },
	{ "ENOMEM",	ENOMEM },
	{ "ENOSPC",	ENOSPC },
	{ "EIO",	EIO },
	{ "ENOTEMPTY",	ENOTEMPTY },
	{ "ENOSYS",	ENOSYS },
	{ "EROFS",	EROFS },
	{ "EBUSY",	EBUSY },
	{ "EAGAIN",	EAGAIN },
	{ "EISCONN",	EISCONN },
	{ NULL,		-1 },
};

struct xs_msghdr
{
	/* Message type */
	uint32_t		 xmh_type;
	/* Request identifier, echoed in daemon's response.  */
	uint32_t		 xmh_rid;
	/* Transaction id (0 if not related to a transaction). */
	uint32_t		 xmh_tid;
	/* Length of data following this. */
	uint32_t		 xmh_len;
	/* Generally followed by nul-terminated string(s). */
} __packed;

/*
 * A minimum output buffer size needed to store an error string.
 */
#define XS_ERR_PAYLOAD		16

/*
 * Although Xen source code implies that the limit is 4k, in practice
 * Mike has figured out that we can only send 2k bytes of payload w/o
 * receiving a ENOSPC.  We set it to an even smaller value however,
 * because there's no real need to use large buffers for anything.
 */
#define XS_MAX_PAYLOAD		1024

struct xs_msg {
	struct xs_msghdr	 xsm_hdr;
	int			 xsm_read;
	int			 xsm_dlen;
	uint8_t			*xsm_data;
	TAILQ_ENTRY(xs_msg)	 xsm_link;
};
TAILQ_HEAD(xs_msgq, xs_msg);

#define XS_RING_SIZE		1024

struct xs_ring {
	uint8_t			xsr_req[XS_RING_SIZE];
	uint8_t			xsr_rsp[XS_RING_SIZE];
	uint32_t		xsr_req_cons;
	uint32_t		xsr_req_prod;
	uint32_t		xsr_rsp_cons;
	uint32_t		xsr_rsp_prod;
} __packed;

#define XST_DELAY		1	/* in seconds */

/*
 * Container for all XenStore related state.
 */
struct xs_softc {
	struct xen_softc	*xs_sc;

	evtchn_port_t		 xs_port;
	xen_intr_handle_t	 xs_ih;

	struct xs_ring		*xs_ring;

	struct xs_msg		 xs_msgs[10];
	struct xs_msg		*xs_rmsg;

	struct xs_msgq		 xs_free;
	struct xs_msgq		 xs_reqs;
	struct xs_msgq		 xs_rsps;

	volatile uint		 xs_rid;

	const char		*xs_wchan;
	const char		*xs_rchan;

	struct mutex		 xs_reqlck;	/* request queue mutex */
	struct mutex		 xs_rsplck;	/* response queue mutex */
	struct mutex		 xs_frqlck;	/* free queue mutex */

	uint			 xs_rngsem;
};

struct xs_msg	*xs_get_msg(struct xs_softc *, int);
void		 xs_put_msg(struct xs_softc *, struct xs_msg *);
int		 xs_ring_get(struct xs_softc *, void *, size_t);
int		 xs_ring_put(struct xs_softc *, void *, size_t);
void		 xs_intr(void *);
int		 xs_output(struct xs_transaction *, uint8_t *, int);
int		 xs_start(struct xs_transaction *, struct xs_msg *,
		    struct iovec *, int);
struct xs_msg	*xs_reply(struct xs_transaction *, uint);
int		 xs_parse(struct xs_transaction *, struct xs_msg *,
		     struct iovec **, int *);

int
xs_attach(struct xen_softc *sc)
{
        struct xen_hvm_param xhv;
	struct xs_softc *xs;
	paddr_t pa;
	int i;

	if ((xs = malloc(sizeof(*xs), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		printf("%s: failed to allocate xenstore softc\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	sc->sc_xs = xs;
	xs->xs_sc = sc;

	/* Fetch event channel port */
	memset(&xhv, 0, sizeof(xhv));
	xhv.domid = DOMID_SELF;
	xhv.index = HVM_PARAM_STORE_EVTCHN;
	if (xen_hypercall(sc, hvm_op, 2, HVMOP_get_param, &xhv))
		goto fail_1;
	xs->xs_port = xhv.value;

	DPRINTF("%s: xenstore event channel %d\n", sc->sc_dev.dv_xname,
	    xs->xs_port);

	/* Fetch a frame number (PA) of a shared xenstore page */
	memset(&xhv, 0, sizeof(xhv));
	xhv.domid = DOMID_SELF;
	xhv.index = HVM_PARAM_STORE_PFN;
	if (xen_hypercall(sc, hvm_op, 2, HVMOP_get_param, &xhv))
		goto fail_1;
	pa = ptoa(xhv.value);
	/* Allocate a page of virtual memory */
	xs->xs_ring = km_alloc(PAGE_SIZE, &kv_any, &kp_none, &kd_nowait);
	if (xs->xs_ring == NULL)
		goto fail_1;
	/* Map in the xenstore page into our KVA */
	pa |= PMAP_NOCACHE;
	pmap_kenter_pa((vaddr_t)xs->xs_ring, pa, PROT_READ | PROT_WRITE);
	pmap_update(pmap_kernel());

	DPRINTF("%s: xenstore ring at va %p pa %#lx\n", sc->sc_dev.dv_xname,
	    xs->xs_ring, pa & ~PMAP_NOCACHE);

	if (xen_intr_establish(xs->xs_port, &xs->xs_ih, xs_intr, xs, "xs0"))
		goto fail_2;

	DPRINTF("%s: xenstore interrupt established for port %d\n",
	    sc->sc_dev.dv_xname, xs->xs_ih);

	xs->xs_wchan = "xswrite";
	xs->xs_rchan = "xsread";

	TAILQ_INIT(&xs->xs_free);
	TAILQ_INIT(&xs->xs_reqs);
	TAILQ_INIT(&xs->xs_rsps);
	for (i = 0; i < nitems(xs->xs_msgs); i++)
		TAILQ_INSERT_TAIL(&xs->xs_free, &xs->xs_msgs[i], xsm_link);

	mtx_init(&xs->xs_reqlck, IPL_NET);
	mtx_init(&xs->xs_rsplck, IPL_NET);
	mtx_init(&xs->xs_frqlck, IPL_NET);

	return (0);

 fail_2:
	pmap_kremove((vaddr_t)xs->xs_ring, PAGE_SIZE);
	pmap_update(pmap_kernel());
	km_free(xs->xs_ring, PAGE_SIZE, &kv_any, &kp_none);
	xs->xs_ring = NULL;
 fail_1:
	free(xs, sizeof(*xs), M_DEVBUF);
	sc->sc_xs = NULL;
	return (-1);
}

int
xs_resume(struct xen_softc *sc)
{
	struct xs_softc *xs = sc->sc_xs;

	xs->xs_ring->xsr_rsp_prod = xs->xs_ring->xsr_rsp_cons;

	if (xen_intr_disestablish(xs->xs_ih))
		return (-1);

	if (xen_intr_establish(xs->xs_port, &xs->xs_ih, xs_intr, xs, "xs0"))
		return (-1);

	DPRINTF("%s: xenstore interrupt established for port %d\n",
	    sc->sc_dev.dv_xname, xs->xs_ih);

	return (0);
}

static inline int
xs_sem_get(uint *semaphore)
{
	if (atomic_inc_int_nv(semaphore) != 1) {
		/* we're out of luck */
		if (atomic_dec_int_nv(semaphore) == 0)
			wakeup(semaphore);
		return (0);
	}
	return (1);
}

static inline void
xs_sem_put(uint *semaphore)
{
	if (atomic_dec_int_nv(semaphore) == 0)
		wakeup(semaphore);
}

struct xs_msg *
xs_get_msg(struct xs_softc *xs, int waitok)
{
	static const char *chan = "xsalloc";
	struct xs_msg *xsm;

	mtx_enter(&xs->xs_frqlck);
	for (;;) {
		xsm = TAILQ_FIRST(&xs->xs_free);
		if (xsm != NULL) {
			TAILQ_REMOVE(&xs->xs_free, xsm, xsm_link);
			break;
		}
		if (!waitok) {
			mtx_leave(&xs->xs_frqlck);
			delay(XST_DELAY * 1000 >> 2);
			mtx_enter(&xs->xs_frqlck);
		} else
			msleep(chan, &xs->xs_frqlck, PRIBIO, chan,
			    XST_DELAY * hz >> 2);
	}
	mtx_leave(&xs->xs_frqlck);
	return (xsm);
}

void
xs_put_msg(struct xs_softc *xs, struct xs_msg *xsm)
{
	memset(xsm, 0, sizeof(*xsm));
	mtx_enter(&xs->xs_frqlck);
	TAILQ_INSERT_TAIL(&xs->xs_free, xsm, xsm_link);
	mtx_leave(&xs->xs_frqlck);
}

int
xs_geterror(struct xs_msg *xsm)
{
	int i;

	for (i = 0; i < nitems(xs_errors); i++)
		if (strcmp(xs_errors[i].xse_errstr, xsm->xsm_data) == 0)
			break;
	return (xs_errors[i].xse_errnum);
}

static inline int
xs_ring_avail(struct xs_ring *xsr, int req)
{
	int cons = req ? xsr->xsr_req_cons : xsr->xsr_rsp_cons;
	int prod = req ? xsr->xsr_req_prod : xsr->xsr_rsp_prod;

	membar_consumer();
#ifdef XEN_DEBUG
	KASSERT(prod <= XS_RING_SIZE && cons < XS_RING_SIZE);
#endif
	if (prod > cons)
		return (prod - cons);
	else
		return (XS_RING_SIZE - cons + prod);
	return (0);
}

static inline void
xs_ring_reset(struct xs_softc *xs, int req)
{
	struct xs_ring *xsr = xs->xs_ring;

	if (req) {
		xsr->xsr_req_cons = 0;
		xsr->xsr_req_prod = 0;
	} else {
		xsr->xsr_rsp_prod = 0;
		xsr->xsr_rsp_cons = 0;
	}
	membar_producer();
}

int
xs_output(struct xs_transaction *xst, uint8_t *bp, int len)
{
	struct xs_softc *xs = xst->xst_sc;
	int chunk, s;

	while (len > 0) {
		chunk = xs_ring_put(xs, bp, MIN(len, XS_RING_SIZE));
		if (chunk < 0)
			return (-1);
		if (chunk > 0) {
			len -= chunk;
			bp += chunk;
			if (xs->xs_ring->xsr_req_prod < XS_RING_SIZE)
				continue;
		}
		/* Squeaky wheel gets the kick */
		xen_intr_signal(xs->xs_ih);
		/*
		 * chunk == 0: we need to wait for hv to consume
		 * what has already been written;
		 *
		 * Alternatively we have managed to fill the ring
		 * and must wait for HV to collect the data.
		 */
		while (xs->xs_ring->xsr_req_prod > xs->xs_ring->xsr_req_cons) {
			if (xst->xst_flags & XST_POLL) {
				delay(XST_DELAY * 1000 >> 2);
				s = splnet();
				xs_intr(xs);
				splx(s);
			} else
				tsleep(xs->xs_wchan, PRIBIO, xs->xs_wchan,
				    XST_DELAY * hz >> 2);
			membar_sync();
		}
		/* It's safe to do a reset here because cons == prod == 1024 */
		if (xs->xs_ring->xsr_req_prod == XS_RING_SIZE)
			xs_ring_reset(xs, 1);
	}
	return (0);
}

int
xs_start(struct xs_transaction *xst, struct xs_msg *xsm, struct iovec *iov,
    int iov_cnt)
{
	struct xs_softc *xs = xst->xst_sc;
	int i;

	while (!xs_sem_get(&xs->xs_rngsem)) {
		if (xst->xst_flags & XST_POLL)
			delay(XST_DELAY * 1000 >> 2);
		else
			tsleep(&xs->xs_rngsem, PRIBIO, "xsaccess",
			    XST_DELAY * hz >> 2);
	}

	/* Header */
	if (xs_output(xst, (uint8_t *)&xsm->xsm_hdr,
	    sizeof(xsm->xsm_hdr)) == -1) {
		printf("%s: failed to write the header\n", __func__);
		return (-1);
	}

	/* Data loop */
	for (i = 0; i < iov_cnt; i++) {
		if (xs_output(xst, iov[i].iov_base, iov[i].iov_len) == -1) {
			printf("%s: failed on iovec #%d len %ld\n", __func__,
			    i, iov[i].iov_len);
			return (-1);
		}
	}

	mtx_enter(&xs->xs_reqlck);
	TAILQ_INSERT_TAIL(&xs->xs_reqs, xsm, xsm_link);
	mtx_leave(&xs->xs_reqlck);

	xen_intr_signal(xs->xs_ih);

	xs_sem_put(&xs->xs_rngsem);

	return (0);
}

struct xs_msg *
xs_reply(struct xs_transaction *xst, uint rid)
{
	struct xs_softc *xs = xst->xst_sc;
	struct xs_msg *xsm;
	int s;

	mtx_enter(&xs->xs_rsplck);
	for (;;) {
		TAILQ_FOREACH(xsm, &xs->xs_rsps, xsm_link) {
			if (xsm->xsm_hdr.xmh_tid == xst->xst_id &&
			    xsm->xsm_hdr.xmh_rid == rid)
				break;
		}
		if (xsm != NULL) {
			TAILQ_REMOVE(&xs->xs_rsps, xsm, xsm_link);
			break;
		}
		if (xst->xst_flags & XST_POLL) {
			mtx_leave(&xs->xs_rsplck);
			delay(XST_DELAY * 1000 >> 2);
			s = splnet();
			xs_intr(xs);
			splx(s);
			mtx_enter(&xs->xs_rsplck);
		} else
			msleep(xs->xs_rchan, &xs->xs_rsplck, PRIBIO,
			    xs->xs_rchan, XST_DELAY * hz >> 2);
	}
	mtx_leave(&xs->xs_rsplck);
	return (xsm);
}

int
xs_ring_put(struct xs_softc *xs, void *src, size_t size)
{
	struct xs_ring *xsr = xs->xs_ring;
	int cons = xsr->xsr_req_cons;
	int prod = xsr->xsr_req_prod;
	int left = XS_RING_SIZE - prod;

	membar_consumer();
#ifdef XEN_DEBUG
	KASSERT(prod <= XS_RING_SIZE && cons < XS_RING_SIZE);
#endif
	if (size > XS_RING_SIZE)
		return (-1);
	if (cons > prod)
		size = MIN(size, cons - prod);
	else
		size = MIN(size, left);
	memcpy(&xsr->xsr_req[prod], src, size);
	membar_producer();
	xsr->xsr_req_prod += size; /* This never goes above the ring size */
	return (size);
}

int
xs_ring_get(struct xs_softc *xs, void *dst, size_t size)
{
	struct xs_ring *xsr = xs->xs_ring;
	int cons = xsr->xsr_rsp_cons;
	int prod = xsr->xsr_rsp_prod;
	int left = XS_RING_SIZE - cons;

	membar_consumer();
#ifdef XEN_DEBUG
	KASSERT(prod <= XS_RING_SIZE && cons < XS_RING_SIZE);
#endif
	if (size > XS_RING_SIZE)
		return (-1);
	if (prod == cons)
		return (0);
	if (prod > cons)
		size = MIN(size, prod - cons);
	else
		size = MIN(size, left);
	memcpy(dst, &xsr->xsr_rsp[cons], size);
	membar_producer();
	xsr->xsr_rsp_cons += size; /* This never goes above the ring size */
	return (size);
}

void
xs_intr(void *arg)
{
	struct xs_softc *xs = arg;
	struct xs_ring *xsr = xs->xs_ring;
	struct xen_softc *sc = xs->xs_sc;
	struct xs_msg *xsm = xs->xs_rmsg;
	struct xs_msghdr xmh;
	int avail, len;

	membar_sync();

	if (xsr->xsr_rsp_cons == xsr->xsr_rsp_prod)
		return;

	avail = xs_ring_avail(xsr, 0);

	/* Response processing */

	if (xs->xs_rmsg == NULL) {
		if (avail < sizeof(xmh)) {
			printf("%s: incomplete header: %d\n",
			    sc->sc_dev.dv_xname, avail);
			goto out;
		}
		avail -= sizeof(xmh);

		if (TAILQ_EMPTY(&xs->xs_reqs)) {
			printf("%s: missing requests\n", sc->sc_dev.dv_xname);
			goto out;
		}

		if ((len = xs_ring_get(xs, &xmh, sizeof(xmh))) != sizeof(xmh)) {
			printf("%s: message too short: %d\n",
			    sc->sc_dev.dv_xname, len);
			goto out;
		}

		TAILQ_FOREACH(xsm, &xs->xs_reqs, xsm_link) {
			if (xsm->xsm_hdr.xmh_rid == xmh.xmh_rid)
				break;
		}
		if (xsm == NULL) {
			printf("%s: received unexpected message id %u\n",
			    sc->sc_dev.dv_xname, xmh.xmh_rid);
			goto out;
		}

		memcpy(&xsm->xsm_hdr, &xmh, sizeof(xmh));
		xs->xs_rmsg = xsm;

		if (avail == 0)
			goto out;
	}

	if (xsm->xsm_hdr.xmh_len > xsm->xsm_dlen)
		panic("message too large: %d vs %d for type %d, rid %u",
		    xsm->xsm_hdr.xmh_len, xsm->xsm_dlen, xsm->xsm_hdr.xmh_type,
		    xsm->xsm_hdr.xmh_rid);

	len = MIN(xsm->xsm_hdr.xmh_len - xsm->xsm_read, avail);
	if ((len = xs_ring_get(xs, &xsm->xsm_data[xsm->xsm_read], len)) <= 0) {
		printf("%s: read failure %d\n", sc->sc_dev.dv_xname, len);
		goto out;
	}
	xsm->xsm_read += len;

	/* Notify reader that we've managed to read the whole message */
	if (xsm->xsm_read == xsm->xsm_hdr.xmh_len) {
		xs->xs_rmsg = NULL;
		mtx_enter(&xs->xs_rsplck);
		TAILQ_REMOVE(&xs->xs_reqs, xsm, xsm_link);
		TAILQ_INSERT_TAIL(&xs->xs_rsps, xsm, xsm_link);
		mtx_leave(&xs->xs_rsplck);
		wakeup(xs->xs_rchan);

		xs_ring_reset(xs, 0);
	}

	/* It's safe to do a reset here because cons == prod == 1024 */
	if (xs->xs_ring->xsr_rsp_prod == XS_RING_SIZE)
		xs_ring_reset(xs, 0);

 out:
	/* Wakeup sleeping writes (if any) */
	wakeup(xs->xs_wchan);
	xen_intr_signal(xs->xs_ih);
}

static inline int
xs_get_buf(struct xs_transaction *xst, struct xs_msg *xsm, int len)
{
	unsigned char *buf = NULL;

	buf = malloc(len, M_DEVBUF, M_ZERO | (xst->xst_flags & XST_POLL ?
	    M_NOWAIT : M_WAITOK));
	if (buf == NULL)
		return (-1);
	xsm->xsm_dlen = len;
	xsm->xsm_data = buf;
	return (0);
}

static inline void
xs_put_buf(struct xs_transaction *xst, struct xs_msg *xsm)
{
	free(xsm->xsm_data, M_DEVBUF, xsm->xsm_dlen);
	xsm->xsm_data = NULL;
}

void
xs_resfree(struct xs_transaction *xst, struct iovec *iov, int iov_cnt)
{
	int i;

	for (i = 0; i < iov_cnt; i++)
		free(iov[i].iov_base, M_DEVBUF, iov[i].iov_len);
	free(iov, M_DEVBUF, sizeof(struct iovec) * iov_cnt);
}

int
xs_parse(struct xs_transaction *xst, struct xs_msg *xsm, struct iovec **iov,
    int *iov_cnt)
{
	char *bp, *cp;
	int i, flags;

	flags = M_ZERO | (xst->xst_flags & XST_POLL ? M_NOWAIT : M_WAITOK);

	*iov_cnt = 0;
	/* Make sure that the data is NUL terminated */
	xsm->xsm_data[xsm->xsm_hdr.xmh_len - 1] = '\0';
	for (i = 0; i < xsm->xsm_hdr.xmh_len; i++)
		if (i > 0 && xsm->xsm_data[i] == '\0')
			(*iov_cnt)++;
	if (!*iov_cnt)
		return (0);
	*iov = mallocarray(*iov_cnt, sizeof(struct iovec), M_DEVBUF, flags);
	if (*iov == NULL)
		return (-1);
	bp = xsm->xsm_data;
	for (i = 0; i < *iov_cnt; i++) {
		for (cp = bp;
		     cp - (caddr_t)xsm->xsm_data < xsm->xsm_hdr.xmh_len; cp++)
			if (*cp == '\0')
				break;
		(*iov)[i].iov_len = cp - bp + 1;
		(*iov)[i].iov_base = malloc((*iov)[i].iov_len, M_DEVBUF, flags);
		if (!(*iov)[i].iov_base)
			goto cleanup;
		memcpy((*iov)[i].iov_base, bp, (*iov)[i].iov_len);
		bp = ++cp;
	}

	return (0);

 cleanup:
	xs_resfree(xst, *iov, *iov_cnt);
	return (ENOMEM);
}

int
xs_cmd(struct xs_transaction *xst, int cmd, const char *path,
    struct iovec **iov, int *iov_cnt)
{
	struct xs_softc *xs = xst->xst_sc;
	struct xs_msg *xsm;
	struct iovec ov[10];	/* output vector */
	int datalen = XS_ERR_PAYLOAD;
	int ov_cnt = 0;
	enum { READ, WRITE } mode = READ;
	int i, error = 0;

	if (cmd >= XS_MAX)
		return (-1);

	switch (cmd) {
	case XS_TRANSACTION_START:
		ov[0].iov_base = "";
		ov[0].iov_len = 1;
		ov_cnt++;
		break;
	case XS_TRANSACTION_END:
		mode = WRITE;
		break;
	case XS_MKDIR:
	case XS_RM:
	case XS_WRITE:
		mode = WRITE;
		/* FALLTHROUGH */
	default:
		if (mode == READ)
			datalen = XS_MAX_PAYLOAD;
		break;
	}

	if (path) {
		ov[ov_cnt].iov_base = (void *)path;
		ov[ov_cnt++].iov_len = strlen(path) + 1; /* +NUL */
	}

	if (mode == WRITE && iov && iov_cnt && *iov_cnt > 0) {
		for (i = 0; i < *iov_cnt && ov_cnt < nitems(ov);
		     i++, ov_cnt++) {
			ov[ov_cnt].iov_base = (*iov)[i].iov_base;
			ov[ov_cnt].iov_len = (*iov)[i].iov_len;
		}
		KASSERT(ov_cnt < nitems(ov));
	}

	xsm = xs_get_msg(xs, !(xst->xst_flags & XST_POLL));

	if (xs_get_buf(xst, xsm, datalen)) {
		xs_put_msg(xs, xsm);
		return (-1);
	}

	xsm->xsm_hdr.xmh_tid = xst->xst_id;
	xsm->xsm_hdr.xmh_type = cmd;
	xsm->xsm_hdr.xmh_rid = atomic_inc_int_nv(&xs->xs_rid);

	for (i = 0; i < ov_cnt; i++)
		xsm->xsm_hdr.xmh_len += ov[i].iov_len;

	if (xsm->xsm_hdr.xmh_len >= XS_MAX_PAYLOAD) {
		printf("%s: message type %d with payload above the limit\n",
		    xs->xs_sc->sc_dev.dv_xname, cmd);
		xs_put_buf(xst, xsm);
		xs_put_msg(xs, xsm);
		return (-1);
	}

	if (xs_start(xst, xsm, ov, ov_cnt)) {
		printf("%s: message type %d transmission failed\n",
		    xs->xs_sc->sc_dev.dv_xname, cmd);
		xs_put_buf(xst, xsm);
		xs_put_msg(xs, xsm);
		return (-1);
	}

	xsm = xs_reply(xst, xsm->xsm_hdr.xmh_rid);

	if (xsm->xsm_hdr.xmh_type == XS_ERROR) {
		error = xs_geterror(xsm);
		DPRINTF("%s: xenstore request %d error %s\n",
		    xs->xs_sc->sc_dev.dv_xname, cmd, xsm->xsm_data);
	} else if (mode == READ) {
		KASSERT(iov && iov_cnt);
		error = xs_parse(xst, xsm, iov, iov_cnt);
	}
#ifdef XEN_DEBUG
	else
		if (strcmp(xsm->xsm_data, "OK"))
			printf("%s: xenstore request %d failed: %s\n",
			    xs->xs_sc->sc_dev.dv_xname, cmd, xsm->xsm_data);
#endif

	xs_put_buf(xst, xsm);
	xs_put_msg(xs, xsm);

	return (error);
}
