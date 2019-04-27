/*	$OpenBSD: if_wg.c,v 1.1 2019/04/27 05:14:33 dlg Exp $ */

/*
 * Copyright (c) 2018, 2019 David Gwynne <dlg@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/selinfo.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/rtable.h>
#include <netinet/in.h>

#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet/ip.h>
#include <net/route.h>
#include <netinet/in_pcb.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <crypto/chacha.h>
#include <crypto/poly1305.h>

#include <net/if_wg.h>

struct wg_data_hdr {
	uint32_t		 msg_type;
	uint32_t		 index;
	uint32_t		 counter_lo;
	uint32_t		 counter_hi;
} __packed __aligned(4);

CTASSERT(sizeof(struct chacha_key) == sizeof(struct wg_aead_key));

#define DPRINTF(_ifp, _a...)	do {					\
	if (ISSET((_ifp)->if_flags, IFF_DEBUG)) {			\
		printf("%s: ", (_ifp)->if_xname);			\
		printf(_a);						\
		printf("\n");						\
	}								\
} while (0)

struct wg_data_keys {
	TAILQ_ENTRY(wg_data_keys)
				 wk_entry;
	uint32_t		 wk_tx_idx;
	uint32_t		 wk_rx_idx;
	uint64_t		 wk_tx_seq;
	uint64_t		 wk_rx_seq;
	struct chacha_key	 wk_tx_key;
	struct chacha_key	 wk_rx_key;

	int			 wk_born;
	unsigned int		 wk_rekeyed;
};

TAILQ_HEAD(wg_data_keys_list, wg_data_keys);

struct wg_device;

struct wg_softc {
	unsigned int		 sc_unit; /* must be first */
	struct ifnet		 sc_if;

	RBT_ENTRY(wg_softc)	 sc_entry;
	TAILQ_ENTRY(wg_softc)	 sc_lentry;
	struct wg_device	*sc_device;

	struct file * volatile	 sc_fp;

	unsigned int		 sc_initiator;

	int			 sc_up_stamp;

	int			 sc_rk_stamp;
	TAILQ_ENTRY(wg_softc)	 sc_rk_entry;
	unsigned int		 sc_rk_onqueue;
	struct timeout		 sc_rk_timer;

	struct mbuf_queue	 sc_tx_queue;
	struct task		 sc_tx_task;

	int			 sc_tx_stamp;
	struct timeout		 sc_tx_timer;
	struct task		 sc_tx_keepalive;

	/* sc_data_keys is used by wg_start under the ifq serialiaser */
	struct wg_data_keys	*sc_tx_data_keys;
	/* the list is used in the rx path under the lock */
	struct rwlock		 sc_rx_data_keys_lk;
	struct wg_data_keys_list sc_rx_data_keys;

	int			 sc_rx_stamp;
	struct timeout		 sc_rx_timer;
};

RBT_HEAD(wg_if_tree, wg_softc);
TAILQ_HEAD(wg_if_list, wg_softc);

struct wg_device {
	dev_t			 wgd_dev; /* must be first */
	RBT_ENTRY(wg_device)	 wgd_entry;
	struct wg_if_list	 wgd_ifaces;

	struct mutex		 wgd_rk_mtx;
	struct wg_if_list	 wgd_rk_list;
	unsigned int		 wgd_rk_reading;

	struct selinfo		 wgd_rsel;
	struct selinfo		 wgd_wsel;
	struct mutex		 wgd_sel_mtx;
	int			 wgd_nbio;
};

RBT_HEAD(wg_dv_tree, wg_device);

struct {
	struct rwlock		wg_dv_lock;
	struct rwlock		wg_if_lock;
	struct wg_dv_tree	wg_dv_tree;
	struct wg_if_tree	wg_if_tree;
} wg_state = {
	RWLOCK_INITIALIZER("wgdevs"),
	RWLOCK_INITIALIZER("wgifs"),
	RBT_INITIALIZER(),
	RBT_INITIALIZER(),
};

static int	wg_filt_read(struct knote *, long);
static void	wg_filt_read_detach(struct knote *);
static int	wg_filt_write(struct knote *, long);
static void	wg_filt_write_detach(struct knote *);

static struct filterops wg_filtops_read = {
	1, NULL, wg_filt_read_detach, wg_filt_read
};

static struct filterops wg_filtops_write = {
	1, NULL, wg_filt_write_detach, wg_filt_write
};

static struct wg_device *
		wg_dev_lookup_locked(dev_t);
static struct wg_device *
		wg_dev_lookup(dev_t);

static inline int
		wg_if_cmp(const struct wg_softc *, const struct wg_softc *);
static inline int
		wg_dv_cmp(const struct wg_device *, const struct wg_device *);

RBT_PROTOTYPE(wg_if_tree, wg_softc, sc_entry, wg_if_cmp);
RBT_PROTOTYPE(wg_dv_tree, wg_device, wgd_entry, wg_dv_cmp);

struct wg_aead_ctx {
	struct chacha_stream
			chacha20;
	poly1305_state	poly1305;
	size_t		datalen;
};

struct wg_aead_tag {
	uint8_t		tag[WG_POLY1305_TAG_LEN];
} __packed __aligned(4);

static void	wg_aead_init(struct wg_aead_ctx *,
		    const struct chacha_key *, uint64_t);
static void	wg_aead_encrypt(struct wg_aead_ctx *, void *, size_t len);
static void	wg_aead_verify(struct wg_aead_ctx *, void *, size_t len);
static void	wg_aead_decrypt(struct wg_aead_ctx *, void *, size_t len);
static void	wg_aead_final(struct wg_aead_ctx *, struct wg_aead_tag *);

static int	wg_up(struct wg_softc *);
static int	wg_down(struct wg_softc *);

static struct mbuf *
		wg_input(void *, struct mbuf *, int);
static int	wg_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *rt);
static void	wg_start(struct ifqueue *);
static int	wg_ioctl(struct ifnet *, u_long, caddr_t);
static void	wg_send(void *);
static void	wg_link_state(struct wg_softc *, int);
static void	wg_link_up(struct wg_softc *);
static void	wg_link_down(struct wg_softc *, int);

static void	wg_rekey(struct wg_softc *, int);
static void	wg_send_keepalive(void *);
static void	wg_rekey_timer(void *);
static void	wg_tx_timer(void *);
static void	wg_rx_timer(void *);

void
wgattach(int n)
{

}

int
wgopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct wg_device *wgd;
	int rv;

	rv = suser(p);
	if (rv != 0)
		return (rv);

	rv = rw_enter(&wg_state.wg_dv_lock, RW_WRITE | RW_INTR);
	if (rv != 0)
		return (rv);

	wgd = wg_dev_lookup_locked(dev);
	if (wgd != NULL) {
		rv = EBUSY;
		goto out;
	}

	wgd = malloc(sizeof(*wgd), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (wgd == NULL) {
		rv = ENOMEM;
		goto out;
	}
	wgd->wgd_dev = dev;
	memset(&wgd->wgd_rsel, 0, sizeof(wgd->wgd_rsel));
	memset(&wgd->wgd_wsel, 0, sizeof(wgd->wgd_wsel));
	mtx_init(&wgd->wgd_sel_mtx, IPL_SOFTNET);
	wgd->wgd_nbio = ISSET(flag, FNONBLOCK) ? IO_NDELAY : 0;
	TAILQ_INIT(&wgd->wgd_ifaces);

	mtx_init(&wgd->wgd_rk_mtx, IPL_SOFTNET);
	TAILQ_INIT(&wgd->wgd_rk_list);
	wgd->wgd_rk_reading = 0;

	if (RBT_INSERT(wg_dv_tree, &wg_state.wg_dv_tree, wgd) != NULL)
		panic("wg dev tree modified while lock was held");

out:
	rw_exit(&wg_state.wg_dv_lock);

	return (rv);
}

int
wgclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct wg_device *wgd;
	struct wg_softc *sc;
	struct file *fp;

	rw_enter(&wg_state.wg_dv_lock, RW_WRITE);
	wgd = wg_dev_lookup_locked(dev);
	KASSERTMSG(wgd != NULL, "wg device missing in close");

	RBT_REMOVE(wg_dv_tree, &wg_state.wg_dv_tree, wgd);
	rw_exit(&wg_state.wg_dv_lock);

	rw_enter(&wg_state.wg_if_lock, RW_WRITE);
	TAILQ_FOREACH(sc, &wgd->wgd_ifaces, sc_lentry) {
		struct ifnet *ifp = &sc->sc_if;

		if (ifp->if_index != 0) {
			NET_LOCK();
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				wg_down(sc);
			NET_UNLOCK();

			fp = sc->sc_fp;
			if (fp != NULL) {
				struct socket *so;
				struct inpcb *inp;
				int s;

				so = (struct socket *)fp->f_data;
				s = solock(so);
				inp = sotoinpcb(so);
				if (inp->inp_upcall != NULL) {
					inp->inp_upcall_arg = NULL;
					inp->inp_upcall = NULL;
				}
				sounlock(so, s);

				sc->sc_fp = NULL;
				FRELE(fp, p);
			}

			if_detach(ifp);
		}
		
		RBT_REMOVE(wg_if_tree, &wg_state.wg_if_tree, sc);
	}
	rw_exit(&wg_state.wg_if_lock);

	while ((sc = TAILQ_FIRST(&wgd->wgd_ifaces)) != NULL) {
		struct wg_data_keys *wk;

		TAILQ_REMOVE(&wgd->wgd_ifaces, sc, sc_lentry);

		while ((wk = TAILQ_FIRST(&sc->sc_rx_data_keys)) != NULL) {
			TAILQ_REMOVE(&sc->sc_rx_data_keys, wk, wk_entry);
			free(wk, M_DEVBUF, sizeof(*wk));
		}

		task_del(systq, &sc->sc_tx_task);
		mq_purge(&sc->sc_tx_queue);

		free(sc, M_DEVBUF, sizeof(*sc));
	}

	free(wgd, M_DEVBUF, sizeof(*wgd));

	return (0);
}

static struct wg_softc *
wg_if_lookup(struct wg_device *wgd, unsigned int unit)
{
	struct wg_softc *sc;

	sc = RBT_FIND(wg_if_tree, &wg_state.wg_if_tree,
	    (struct wg_softc *)&unit);
	if (sc == NULL || sc->sc_device != wgd)
		return (NULL);

	return (sc);
}

static int
wg_if_create(struct wg_device *wgd, unsigned int unit)
{
	struct wg_softc *sc, *osc;
	int error;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (sc == NULL)
		return (ENOMEM);

	sc->sc_unit = unit;
	sc->sc_device = wgd;
	sc->sc_initiator = 1;
	rw_init(&sc->sc_rx_data_keys_lk, "wgrxkeys");
	TAILQ_INIT(&sc->sc_rx_data_keys);

	mq_init(&sc->sc_tx_queue, 128, IPL_SOFTNET);
	task_set(&sc->sc_tx_task, wg_send, sc);
	task_set(&sc->sc_tx_keepalive, wg_send_keepalive, sc);

	timeout_set(&sc->sc_rk_timer, wg_rekey_timer, sc);
	timeout_set(&sc->sc_tx_timer, wg_tx_timer, sc);
	timeout_set(&sc->sc_rx_timer, wg_rx_timer, sc);

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		goto fail;

	osc = RBT_INSERT(wg_if_tree, &wg_state.wg_if_tree, sc);
	if (osc != NULL) {
		error = (osc->sc_device == wgd) ? EEXIST : EBUSY;
		goto unlock;
	}

	TAILQ_INSERT_TAIL(&wgd->wgd_ifaces, sc, sc_lentry);
	rw_exit(&wg_state.wg_if_lock);

	return (0);
unlock:
	rw_exit(&wg_state.wg_if_lock);
fail:
	free(sc, M_DEVBUF, sizeof(*sc));
	return (error);
}

static int
wg_if_destroy(struct wg_device *wgd, unsigned int unit)
{
	struct wg_softc *sc;
	int error;

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		goto fail;

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	if (sc->sc_if.if_index != 0) {
		error = EBUSY;
		goto unlock;
	}

	TAILQ_REMOVE(&wgd->wgd_ifaces, sc, sc_lentry);
	RBT_REMOVE(wg_if_tree, &wg_state.wg_if_tree, sc);
	rw_exit(&wg_state.wg_if_lock);

	task_del(systq, &sc->sc_tx_task);
	mq_purge(&sc->sc_tx_queue);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
fail:
	return (error);
}

static int
wg_if_attach(struct wg_device *wgd, unsigned int unit)
{
	struct wg_softc *sc;
	struct ifnet *ifp;
	int error;

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	if (sc->sc_if.if_index != 0) {
		error = EBUSY;
		goto unlock;
	}

	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "wg%u", sc->sc_unit);
	ifp->if_softc = sc;
	ifp->if_type = IFT_TUNNEL;
	ifp->if_mtu = 1280;
	ifp->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED|IFXF_MPSAFE;
	ifp->if_output = wg_output;
	ifp->if_qstart = wg_start;
	ifp->if_ioctl = wg_ioctl;
	ifp->if_rtrequest = p2p_rtrequest;
	ifp->if_link_state = LINK_STATE_DOWN;

	if_attach(ifp);
	if_alloc_sadl(ifp);

	if_addgroup(ifp, "wg");

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(uint32_t));
#endif

	KASSERT(ifp->if_index != 0);
	rw_exit(&wg_state.wg_if_lock);

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

static int
wg_if_detach(struct wg_device *wgd, unsigned int unit)
{
	struct wg_softc *sc;
	struct ifnet *ifp;
	int error;

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	if (sc->sc_if.if_index == 0) {
		error = ENXIO;
		goto unlock;
	}

	if (sc->sc_fp != NULL) {
		error = EBUSY;
		goto unlock;
	}

	NET_LOCK();
	if (ISSET(ifp->if_flags, IFF_RUNNING))
		wg_down(sc);
	NET_UNLOCK();

	if_detach(ifp);

	ifp->if_index = 0;
	rw_exit(&wg_state.wg_if_lock);

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

static void
wg_frele(struct file *fp, struct proc *p)
{
	struct socket *so;
	struct inpcb *inp;
	int s;

	so = (struct socket *)fp->f_data;
	inp = sotoinpcb(so);

	s = solock(so);
	inp->inp_upcall = NULL;
	inp->inp_upcall_arg = NULL;
	sounlock(so, s);

	FRELE(fp, p);
}

static int
wg_if_set_sock(struct wg_device *wgd, struct proc *p,
    const struct wg_if_sock *data)
{
	unsigned int unit = data->wg_unit;
	struct wg_softc *sc;
	struct ifnet *ifp;
	struct file *fp, *ofp;
	struct socket *so;
	struct inpcb *inp;
	int error;
	int s;

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}
	ifp = &sc->sc_if;

	if (ifp->if_index == 0) {
		error = ENXIO;
		goto unlock;
	}

	ofp = sc->sc_fp;

	error = getsock(p, data->wg_sock, &fp);
	if (error != 0)
		goto unlock;

	so = (struct socket *)fp->f_data;
	if (so->so_proto->pr_protocol != IPPROTO_UDP) {
		FRELE(fp, p);
		error = EPROTONOSUPPORT;
		goto unlock;
	}
	if (!ISSET(so->so_state, SS_ISCONNECTED)) {
		FRELE(fp, p);
		error = ENOTCONN;
		goto unlock;
	}

	s = solock(so);
	inp = sotoinpcb(so);
	if (inp->inp_upcall != NULL) {
		FRELE(fp, p);
		sounlock(so, s);
		error = EISCONN;
		goto unlock;
	}

	inp->inp_upcall_arg = sc;
	inp->inp_upcall = wg_input;
	sounlock(so, s);

	sc->sc_fp = fp;
	ifq_barrier(&ifp->if_snd);

	rw_exit(&wg_state.wg_if_lock);

	if (ofp)
		wg_frele(ofp, p);
	else {
		KASSERT(ifp->if_link_state == LINK_STATE_DOWN);
		wg_link_state(sc, LINK_STATE_KALIVE_DOWN);
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			if (sc->sc_initiator) {
				/* force a rekey */
				wg_rekey(sc, 1);
			}
		}
	}

	return (0);
		

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

static int
wg_if_clr_sock(struct wg_device *wgd, struct proc *p, unsigned int unit)
{
	struct wg_softc *sc;
	struct file *fp;
	int error;

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	fp = sc->sc_fp;
	if (fp == NULL) {
		error = ENOTCONN;
		goto unlock;
	}

	sc->sc_fp = NULL;
	ifq_barrier(&sc->sc_if.if_snd);

	rw_exit(&wg_state.wg_if_lock);

	wg_frele(fp, p);
	wg_link_down(sc, LINK_STATE_DOWN);

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

static struct wg_data_keys *
wg_if_insert_data_keys(struct wg_softc *sc, struct wg_data_keys *wk)
{
	struct wg_data_keys *owk;

	TAILQ_FOREACH(owk, &sc->sc_rx_data_keys, wk_entry) {
		if (owk->wk_rx_idx == wk->wk_rx_idx ||
		    owk->wk_tx_idx == wk->wk_tx_idx)
			return (owk);
	}

	TAILQ_INSERT_HEAD(&sc->sc_rx_data_keys, wk, wk_entry);

	return (NULL);
}

static int
wg_if_add_keys(struct wg_device *wgd, const struct wg_if_data_keys *keys)
{
	struct wg_softc *sc;
	struct wg_data_keys *wk;
	unsigned int unit = keys->wg_unit;
	int error;

	wk = malloc(sizeof(*wk), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (wk == NULL)
		return (ENOMEM);

	wk->wk_tx_idx = keys->wg_tx_index;
	wk->wk_rx_idx = keys->wg_rx_index;
	wk->wk_tx_seq = 0;
	wk->wk_rx_seq = 0;
	memcpy(&wk->wk_tx_key, &keys->wg_tx_key, sizeof(wk->wk_tx_key));
	memcpy(&wk->wk_rx_key, &keys->wg_rx_key, sizeof(wk->wk_rx_key));

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		goto free;

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	rw_enter_write(&sc->sc_rx_data_keys_lk);
	if (wg_if_insert_data_keys(sc, wk) != NULL)
		error = EEXIST;
	rw_exit_write(&sc->sc_rx_data_keys_lk);
	if (error != 0)
		goto unlock;

	sc->sc_up_stamp = ticks;

	sc->sc_tx_data_keys = wk;
	/* make sure wg_start isn't using the old head */
	ifq_barrier(&sc->sc_if.if_snd);

	rw_exit(&wg_state.wg_if_lock);

	/* XXX locking */
	if (sc->sc_if.if_link_state == LINK_STATE_KALIVE_DOWN) {
		sc->sc_tx_stamp = ticks;
		wg_link_up(sc);
	}

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
free:
	free(wk, M_DEVBUF, sizeof(*wk));
	return (error);
}

static int
wg_if_clr_keys(struct wg_device *wgd, const struct wg_if_data_keys *keys)
{
	struct wg_softc *sc;
	struct wg_data_keys *wk;
	unsigned int unit = keys->wg_unit;
	int error;

	error = rw_enter(&wg_state.wg_if_lock, RW_WRITE | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	rw_enter_write(&sc->sc_rx_data_keys_lk);
	TAILQ_FOREACH(wk, &sc->sc_rx_data_keys, wk_entry) {
		if (wk->wk_rx_idx == keys->wg_rx_index &&
		    wk->wk_tx_idx == keys->wg_tx_index) {
			TAILQ_REMOVE(&sc->sc_rx_data_keys, wk, wk_entry);
			break;
		}
	}
	rw_exit_write(&sc->sc_rx_data_keys_lk);

	if (wk == NULL) {
		error = ENOENT;
		goto unlock;
	}

	if (sc->sc_tx_data_keys == wk) {
		sc->sc_tx_data_keys = TAILQ_FIRST(&sc->sc_rx_data_keys);;
		/* make sure wg_start isn't using the old head */
		ifq_barrier(&sc->sc_if.if_snd);
	}

	rw_exit(&wg_state.wg_if_lock);

	free(wk, M_DEVBUF, sizeof(*wk));

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

static int
wg_if_set_role(struct wg_device *wgd, const struct wg_if_role *role)
{
	struct wg_softc *sc;
	int error = 0;

	error = rw_enter(&wg_state.wg_if_lock, RW_READ | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, role->wg_unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	switch (role->wg_role) {
	case WG_DATA_INITIATOR:
		sc->sc_initiator = 1;
		break;
	case WG_DATA_RESPONDER:
		sc->sc_initiator = 0;
		break;
	default:
		error = EINVAL;
		break;
	}

	rw_exit(&wg_state.wg_if_lock);

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

static int
wg_if_get_role(struct wg_device *wgd, struct wg_if_role *role)
{
	struct wg_softc *sc;
	int error = 0;

	error = rw_enter(&wg_state.wg_if_lock, RW_READ | RW_INTR);
	if (error != 0)
		return (error);

	sc = wg_if_lookup(wgd, role->wg_unit);
	if (sc == NULL) {
		error = ESRCH;
		goto unlock;
	}

	role->wg_role = sc->sc_initiator ?
	    WG_DATA_INITIATOR : WG_DATA_RESPONDER;

	rw_exit(&wg_state.wg_if_lock);

	return (0);

unlock:
	rw_exit(&wg_state.wg_if_lock);
	return (error);
}

int
wgioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct wg_device *wgd = wg_dev_lookup(dev);

	switch (cmd) {
	case FIONBIO:
		wgd->wgd_nbio = *(int *)data ? IO_NDELAY : 0;
		break;
	case FIONREAD:
		*(int *)data = TAILQ_EMPTY(&wgd->wgd_rk_list) ?
		    0 : sizeof(struct wg_msg);
		break;

	case WGIFCREATE:
		return (wg_if_create(wgd, *(unsigned int *)data));
	case WGIFATTACH:
		return (wg_if_attach(wgd, *(unsigned int *)data));
	case WGIFDETACH:
		return (wg_if_detach(wgd, *(unsigned int *)data));
	case WGIFDESTROY:
		return (wg_if_destroy(wgd, *(unsigned int *)data));

	case WGIFSSOCK:
		return (wg_if_set_sock(wgd, p,
		    (const struct wg_if_sock *)data));
	case WGIFDSOCK:
		return (wg_if_clr_sock(wgd, p, *(unsigned int *)data));

	case WGIFADDKEYS:
		return (wg_if_add_keys(wgd,
		    (const struct wg_if_data_keys *)data));
	case WGIFDELKEYS:
		return (wg_if_clr_keys(wgd,
		    (const struct wg_if_data_keys *)data));

	case WGIFSROLE:
		return (wg_if_set_role(wgd, (struct wg_if_role *)data));
	case WGIFGROLE:
		return (wg_if_get_role(wgd, (struct wg_if_role *)data));

	default:
		return (ENOTTY);
	}

	return (0);
}

int
wgread(dev_t dev, struct uio *uio, int ioflag)
{
	struct wg_device *wgd = wg_dev_lookup(dev);
	struct wg_softc *sc;
	struct wg_msg msg;
	int error;

	if (uio->uio_resid < 0)
		return (EINVAL);

	ioflag |= wgd->wgd_nbio;

	mtx_enter(&wgd->wgd_rk_mtx);

	sc = TAILQ_FIRST(&wgd->wgd_rk_list);
	if (sc == NULL) {
		if (ioflag & IO_NDELAY) {
			mtx_leave(&wgd->wgd_rk_mtx);
			return (EWOULDBLOCK);
		}

		wgd->wgd_rk_reading++;
		do {
			error = msleep(&wgd->wgd_rk_list, &wgd->wgd_rk_mtx,
			    (PZERO + 1)|PCATCH, "wgread", 0);
			if (error != 0) {
				wgd->wgd_rk_reading--;
				mtx_leave(&wgd->wgd_rk_mtx);
				return (error);
			}

			sc = TAILQ_FIRST(&wgd->wgd_rk_list);
		} while (sc == NULL);
		wgd->wgd_rk_reading--;
	}

	sc->sc_rk_stamp = ticks;
	sc->sc_rk_onqueue = 0;
	TAILQ_REMOVE(&wgd->wgd_rk_list, sc, sc_rk_entry);

	msg.wg_unit = sc->sc_unit;
	msg.wg_type = WG_MSG_REKEY;
	DPRINTF(&sc->sc_if, "rekey");

	mtx_leave(&wgd->wgd_rk_mtx);

	return (uiomove(&msg, ulmin(uio->uio_resid, sizeof(msg)), uio));
}

int
wgwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (EOPNOTSUPP);
}

int
wgpoll(dev_t dev, int events, struct proc *p)
{
	struct wg_device *wgd = wg_dev_lookup(dev);
	int mevents, revents = 0;

	mevents = ISSET(events, POLLIN | POLLRDNORM);
	if (mevents) {
		if (!TAILQ_EMPTY(&wgd->wgd_rk_list))
			SET(revents, events & mevents);
		else
			selrecord(p, &wgd->wgd_rsel);
	}

	mevents = ISSET(events, POLLOUT | POLLWRNORM);
	if (mevents)
		SET(revents, events & mevents);

	return (revents);
}

int
wgkqfilter(dev_t dev, struct knote *kn)
{
	struct wg_device *wgd = wg_dev_lookup(dev);
	struct klist *klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &wgd->wgd_rsel.si_note;
		kn->kn_fop = &wg_filtops_read;
		break;
	case EVFILT_WRITE:
		klist = &wgd->wgd_wsel.si_note;
		kn->kn_fop = &wg_filtops_write;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = wgd;

	mtx_enter(&wgd->wgd_sel_mtx);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mtx_leave(&wgd->wgd_sel_mtx);

	return (0);
}

static void
wg_filt_read_detach(struct knote *kn)
{
	struct wg_device *wgd = kn->kn_hook;
	struct klist *klist = &wgd->wgd_rsel.si_note;

	mtx_enter(&wgd->wgd_sel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&wgd->wgd_sel_mtx);
}

static int
wg_filt_read(struct knote *kn, long hint)
{
	struct wg_device *wgd = kn->kn_hook;

	kn->kn_data = TAILQ_EMPTY(&wgd->wgd_rk_list) ?
	    0 : sizeof(struct wg_msg);

	return (kn->kn_data != 0);
}

static void
wg_filt_write_detach(struct knote *kn)
{
	struct wg_device *wgd = kn->kn_hook;
	struct klist *klist = &wgd->wgd_wsel.si_note;

	mtx_enter(&wgd->wgd_sel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&wgd->wgd_sel_mtx);
}

static int
wg_filt_write(struct knote *kn, long hint)
{
	kn->kn_data = 0;

	return (0);
}

static struct wg_device *
wg_dev_lookup_locked(dev_t dev)
{
	return (RBT_FIND(wg_dv_tree, &wg_state.wg_dv_tree,
	    (const struct wg_device *)&dev));
}

static struct wg_device *
wg_dev_lookup(dev_t dev)
{
	struct wg_device *wgd;

	rw_enter_read(&wg_state.wg_dv_lock);
	wgd = wg_dev_lookup_locked(dev);
	rw_exit_read(&wg_state.wg_dv_lock);

	return (wgd);
}

static inline int
wg_dv_cmp(const struct wg_device *a, const struct wg_device *b)
{
	if (a->wgd_dev > b->wgd_dev)
		return (1);
	if (a->wgd_dev < b->wgd_dev)
		return (-1);
	return (0);
}

static inline int
wg_if_cmp(const struct wg_softc *a, const struct wg_softc *b)
{
	if (a->sc_unit > b->sc_unit)
		return (1);
	if (a->sc_unit < b->sc_unit)
		return (-1);
	return (0);
}

RBT_GENERATE(wg_if_tree, wg_softc, sc_entry, wg_if_cmp);
RBT_GENERATE(wg_dv_tree, wg_device, wgd_entry, wg_dv_cmp);

#define WG_MS2TICKS(_m)	(((_m) * 1000) / tick)

static inline int
wg_ratecheck(int stamp, int interval)
{
	int diff;

	diff = ticks - stamp;
	return (diff >= interval);
}

static void
wg_rekey(struct wg_softc *sc, int force)
{
	struct wg_device *wgd = sc->sc_device;
	int tmo = WG_MS2TICKS(WG_REKEY_TIMEOUT);
	int wake = 0;
	int next = 0;

	if (sc->sc_rk_onqueue)
		return;

	if (!force && !wg_ratecheck(sc->sc_rk_stamp, tmo))
		return;

	mtx_enter(&wgd->wgd_rk_mtx);
	if (!sc->sc_rk_onqueue) {
		if (force || wg_ratecheck(sc->sc_rk_stamp, tmo)) {
			TAILQ_INSERT_TAIL(&wgd->wgd_rk_list, sc, sc_rk_entry);
			sc->sc_rk_onqueue = 1;

			wake = wgd->wgd_rk_reading;
		}
		next = 1;
	}
	mtx_leave(&wgd->wgd_rk_mtx);

	if (wake)
		wakeup(&wgd->wgd_rk_list);
	selwakeup(&wgd->wgd_rsel);

	if (next) {
		timeout_add_msec(&sc->sc_rk_timer,
		    WG_REKEY_TIMEOUT + arc4random_uniform(WG_REKEY_JITTER));
	}

	sc->sc_tx_stamp = ticks;
}

static int
wg_up(struct wg_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	SET(ifp->if_flags, IFF_RUNNING);

	if (ifp->if_link_state == LINK_STATE_KALIVE_DOWN) {
		if (sc->sc_initiator) {
			/* force a rekey */
			wg_rekey(sc, 1);
		}
	}

	return (0);
}

static void
wg_link_state(struct wg_softc *sc, int link_state)
{
	struct ifnet *ifp = &sc->sc_if;

	ifp->if_link_state = link_state;
	if_link_state_change(ifp);
}

static void
wg_rekey_timer(void *arg)
{
	struct wg_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	if (ifp->if_link_state == LINK_STATE_UP &&
	    wg_ratecheck(sc->sc_up_stamp, WG_MS2TICKS(WG_REKEY_MAX)))
		wg_link_down(sc, LINK_STATE_KALIVE_DOWN);

	wg_rekey(sc, 0);
}

static void
wg_link_up(struct wg_softc *sc)
{
	DPRINTF(&sc->sc_if, "link up");
	timeout_add_msec(&sc->sc_tx_timer, WG_KEEPALIVE + WG_REKEY_TIMEOUT);
	timeout_add_msec(&sc->sc_rx_timer, WG_KEEPALIVE);
	wg_link_state(sc, LINK_STATE_UP);
}

static void
wg_link_down(struct wg_softc *sc, int link_state)
{
	DPRINTF(&sc->sc_if, "link down");
	timeout_del(&sc->sc_rx_timer);
	timeout_del(&sc->sc_tx_timer);

	if (link_state == LINK_STATE_DOWN)
		timeout_del(&sc->sc_rk_timer);

	timeout_barrier(&sc->sc_rx_timer); /* implies tx and rk bar too */

	wg_link_state(sc, link_state);
}

static void
wg_rx_timer(void *arg)
{
	struct wg_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	const int tmo = WG_MS2TICKS(WG_KEEPALIVE);
	int diff;

	diff = ticks - sc->sc_rx_stamp;
	DPRINTF(ifp, "%s, tmo %d, diff %d = ticks %d - stamp %d", __func__,
	    tmo, diff, ticks, sc->sc_rx_stamp);
	if (diff >= tmo) {
		DPRINTF(ifp, "rx timer expired, sending keepalive");
		ifq_serialize(&ifp->if_snd, &sc->sc_tx_keepalive);
		diff = 0;
	}

	DPRINTF(ifp, "%s, timeout_add rx timer %d", __func__, tmo - diff);
	timeout_add(&sc->sc_rx_timer, tmo - diff);
}

static void
wg_tx_timer(void *arg)
{
	struct wg_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	const int tmo = WG_MS2TICKS(WG_KEEPALIVE + WG_REKEY_TIMEOUT);
	int diff;

	diff = ticks - sc->sc_tx_stamp;
	DPRINTF(ifp, "%s, tmo %d, diff %d = ticks %d - stamp %d", __func__,
	    tmo, diff, ticks, sc->sc_tx_stamp);
	if (diff >= tmo) {
		DPRINTF(ifp, "tx timer expired, sending rekey");
		wg_rekey(sc, 0);
		diff = 0;
	}

	DPRINTF(ifp, "%s, timeout_add tx timer %d", __func__, tmo - diff);
	timeout_add(&sc->sc_tx_timer, tmo - diff);
}

static int
wg_down(struct wg_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	CLR(ifp->if_flags, IFF_RUNNING);
	ifq_barrier(&ifp->if_snd);

	wg_link_down(sc, sc->sc_fp == NULL ?
	    LINK_STATE_DOWN : LINK_STATE_KALIVE_DOWN);

	return (0);
}

static int
wg_get_tunnel(struct wg_softc *sc, struct if_laddrreq *req)
{
	struct file *fp;
	struct socket *so;
	struct inpcb *inp;
//	int s;

	fp = sc->sc_fp;
	if (fp == NULL)
		return (EADDRNOTAVAIL);

	so = (struct socket *)fp->f_data;
//	s = solock(so);
	inp = sotoinpcb(so);

	if (inp->inp_flags & INP_IPV6) {
#ifdef INET6
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&req->addr;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		in6_recoverscope(sin6, &inp->inp_laddr6);
		sin6->sin6_port = inp->inp_lport;

		sin6 = (struct sockaddr_in6 *)&req->dstaddr;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		in6_recoverscope(sin6, &inp->inp_faddr6);
		sin6->sin6_port = inp->inp_fport;
#else /* INET6 */
		unhandled_af(AF_INET6);
#endif /* INET6 */
	} else {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&req->addr;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = inp->inp_laddr;
		sin->sin_port = inp->inp_lport;

		sin = (struct sockaddr_in *)&req->dstaddr;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = inp->inp_faddr;
		sin->sin_port = inp->inp_fport;
	}

//	sounlock(so, s);

	return (0);
}

static int
wg_get_rdomain(struct wg_softc *sc, struct ifreq *ifr)
{
	struct file *fp;
	struct socket *so;
	struct inpcb *inp;
//	int s;

	fp = sc->sc_fp;
	if (fp == NULL)
		return (EADDRNOTAVAIL);

	so = (struct socket *)fp->f_data;
//	s = solock(so);
	inp = sotoinpcb(so);

	ifr->ifr_rdomainid = inp->inp_rtableid;

//	sounlock(so, s);

	return (0);
}

static int
wg_get_ttl(struct wg_softc *sc, struct ifreq *ifr)
{
	struct file *fp;
	struct socket *so;
	struct inpcb *inp;
//	int s;

	fp = sc->sc_fp;
	if (fp == NULL)
		return (EADDRNOTAVAIL);

	so = (struct socket *)fp->f_data;
//	s = solock(so);
	inp = sotoinpcb(so);

	if (inp->inp_flags & INP_IPV6) {
#ifdef INET6
		ifr->ifr_ttl = inp->inp_ipv6.ip6_hlim;
#else /* INET6 */
		unhandled_af(AF_INET6);
#endif /* INET6 */
	} else {
		ifr->ifr_ttl = inp->inp_ip.ip_ttl;
	}

//	sounlock(so, s);

	return (0);
}

static int
wg_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wg_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		break;

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = wg_up(sc);
			else
				error = 0;
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = wg_down(sc);
		}
		break;

	case SIOCGLIFPHYADDR:
		error = wg_get_tunnel(sc, (struct if_laddrreq *)data);
		break;
	case SIOCGLIFPHYRTABLE:
		error = wg_get_rdomain(sc, ifr);
		break;
	case SIOCGLIFPHYTTL:
		error = wg_get_ttl(sc, ifr);
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static int
wg_decap(struct ifnet *ifp, struct mbuf *m0,
    const struct chacha_key *rxkey, uint64_t counter)
{
	struct wg_aead_ctx ctx;
	struct wg_aead_tag ptag, ctag;
	struct mbuf *mn, *m;
	unsigned int len = m0->m_pkthdr.len;
	unsigned int diff;
	int rv = 0;

	if (len < sizeof(ptag)) {
		/* must be long enough to contain the tag */
		ifp->if_iqdrops++;
		return (-1);
	}

	len -= sizeof(ptag);
	if (len % 16) {
		/* be padded */
		ifp->if_iqdrops++;
		return (-1);
	}

	wg_aead_init(&ctx, rxkey, counter);

	if (len) {
		/* m_apply, but different */
		mn = m0;
		do {
			m = mn;
			KASSERT(m != NULL);

			diff = min(m->m_len, len);
			if (diff) {
				wg_aead_verify(&ctx, m->m_data, diff);
				len -= diff;
			}

			mn = m->m_next;
		} while (len > 0);
	} else {
		m = m0;
		diff = 0;
	}

	m_copydata(m, diff, sizeof(ptag), (caddr_t)&ptag);

	wg_aead_final(&ctx, &ctag);
	if (memcmp(&ctag, &ptag, sizeof(ctag)) != 0) {
		/* mac didnt match */
		ifp->if_ierrors++;
		rv = -1;
		goto out;
	}

	/* chop the poly bit off */
	m_freem(m->m_next);
	m->m_next = NULL;
	m->m_len = diff;
	m0->m_pkthdr.len -= sizeof(ptag);

	if (len) {
		m = m0;
		do {
			wg_aead_decrypt(&ctx, m->m_data, m->m_len);
			m = m->m_next;
		} while (m != NULL);
	}

out:
	explicit_bzero(&ctx, sizeof(ctx));

	return (rv);
}

static struct wg_data_keys *
wg_match_rx_data_keys(struct wg_softc *sc, uint32_t index)
{
	struct wg_data_keys *wk;

	TAILQ_FOREACH(wk, &sc->sc_rx_data_keys, wk_entry) {
		if (wk->wk_rx_idx == index)
			return (wk);
	}

	return (NULL);
}

static void
wg_send_rekey(struct wg_softc *sc)
{

}

static struct mbuf *
wg_input(void *ctx, struct mbuf *m, int iphlen)
{
	struct wg_softc *sc = ctx;
	struct ifnet *ifp = &sc->sc_if;
	struct wg_data_hdr *hdr;
	int hlen = iphlen + sizeof(*hdr);
	struct wg_data_keys *wk;
	struct mbuf *n;
	uint64_t counter;
	uint64_t diff;
	int high;
	int rv;
	void (*input)(struct ifnet *, struct mbuf *);

	soassertlocked((struct socket *)sc->sc_fp->f_data);

	if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
		/* no point if we're not up */
		return (m);
	}

	if (TAILQ_EMPTY(&sc->sc_rx_data_keys)) {
		/* not set up with any keys */
		return (m);
	}

	if (m->m_pkthdr.len < hlen) {
		/* decline short packets */
		return (m);
	}

	if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (m == NULL) {
			/* oops */
			return (NULL);
		}
	}

	hdr = (struct wg_data_hdr *)(mtod(m, uint8_t *) + iphlen);
	if (hdr->msg_type != htole32(WG_MSG_DATA)) {
		/* we only handle data in the kernel */
		return (m);
	}

	counter = lemtoh32(&hdr->counter_lo) |
	    ((uint64_t)lemtoh32(&hdr->counter_hi) << 32);

	if (counter >= WG_REJECT_MSGS)
		goto drop;

	/* might be ours now */

	rw_enter_write(&sc->sc_rx_data_keys_lk);

	wk = wg_match_rx_data_keys(sc, hdr->index);
	if (wk == NULL) {
		/* not this connection */
		goto unlock;
	}

	/* avoid {under,over}flow */
	high = (counter >= wk->wk_rx_seq);
	if (high)
		diff = counter - wk->wk_rx_seq;
	else
		diff = wk->wk_rx_seq - counter;

	if (diff >= WG_MSGS_WINDOW)
		goto unlock;

	m_adj(m, hlen);

	rv = wg_decap(ifp, m, &wk->wk_rx_key, counter);

	if (rv != 0)
		goto drop;

	/* actually ours now according to auth, so move forward */
	if (high)
		wk->wk_rx_seq = counter;

	sc->sc_rx_stamp = ticks;

	rw_exit_write(&sc->sc_rx_data_keys_lk);

	if (counter >= WG_REKEY_MSGS)
		wg_send_rekey(sc);

	if (m->m_pkthdr.len == 0) {
		/* keepalive */
		return (NULL);
	}

	n = m;
	while (n->m_len == 0) {
		n = n->m_next;
		if (n == NULL) {
			ifp->if_ierrors++;
			goto drop;
		}
	}

	switch (*mtod(n, uint8_t *) >> 4) {
	case 4:
		input = ipv4_input;
		m->m_pkthdr.ph_family = AF_INET;
		break;

#ifdef INET6
	case 6:
		input = ipv6_input;
		m->m_pkthdr.ph_family = AF_INET6;
		break;
#endif
	default:
		ifp->if_noproto++;
		goto drop;
	}

	m->m_flags &= ~(M_MCAST|M_BCAST);
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	m->m_pkthdr.ph_flowid = 0;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;

#if NBPFILTER > 0
	{
		caddr_t if_bpf = ifp->if_bpf;
		if (if_bpf) {
			bpf_mtap_af(if_bpf, m->m_pkthdr.ph_family, m,
			    BPF_DIRECTION_IN);
		}
	}
#endif

	(*input)(ifp, m);

	return (NULL);

unlock:
	rw_exit_write(&sc->sc_rx_data_keys_lk);
drop:
	m_freem(m);
	return (NULL);
}

static int
wg_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct wg_softc *sc = ifp->if_softc;
	struct m_tag *mtag;
	int error = 0;

	if (!ISSET(ifp->if_flags, IFF_RUNNING) ||
	    sc->sc_fp == NULL ||
	    sc->sc_tx_data_keys == NULL) {
		error = ENETDOWN;
		goto drop;
	}

	switch (dst->sa_family) {
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
		break;
	default:
		error = EAFNOSUPPORT;
		goto drop;
	}

	/* Try to limit infinite recursion through misconfiguration. */
	for (mtag = m_tag_find(m, PACKET_TAG_GRE, NULL); mtag;
	    mtag = m_tag_find(m, PACKET_TAG_GRE, mtag)) {
		if (memcmp(mtag + 1, &ifp->if_index,
		    sizeof(ifp->if_index)) == 0) {
			error = EIO;
			goto drop;
		}
	}

	mtag = m_tag_get(PACKET_TAG_GRE, sizeof(ifp->if_index), M_NOWAIT);
	if (mtag == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	memcpy(mtag + 1, &ifp->if_index, sizeof(ifp->if_index));
	m_tag_prepend(m, mtag);

	m->m_pkthdr.ph_family = dst->sa_family;

	error = if_enqueue(ifp, m);
	if (error)
		ifp->if_oerrors++;
	return (error);
drop:
	m_freem(m);
	return (error);
}

static int
wg_encap(struct wg_softc *sc, struct wg_data_keys *wk, struct mbuf *m0)
{
	struct wg_aead_ctx ctx;
	struct wg_aead_tag *tag;
	struct mbuf *mn, *m;
	struct wg_data_hdr *hdr;
	uint64_t counter = wk->wk_tx_seq++;
	int padlen;

	wg_aead_init(&ctx, &wk->wk_tx_key, counter);

	mn = m0;
	do {
		m = mn;
		mn = m->m_next;

		if (m->m_len)
			wg_aead_encrypt(&ctx, mtod(m, void *), m->m_len);
	} while (mn != NULL);

	padlen = m0->m_pkthdr.len % 16;
	if (padlen) {
		uint8_t *zero;

		padlen = 16 - padlen;

		if (m_trailingspace(m) < padlen) {
			mn = m_get(M_DONTWAIT, MT_DATA);
			if (mn == NULL)
				goto drop;

			m->m_next = mn;
			m = mn;

			m->m_len = 0;
		}

		zero = mtod(m, uint8_t *) + m->m_len;
		memset(zero, 0, padlen);
		wg_aead_encrypt(&ctx, zero, padlen);

		m0->m_pkthdr.len += padlen;
		m->m_len += padlen;
	}

	if (m_trailingspace(m) < sizeof(*tag)) {
		mn = m_get(M_DONTWAIT, MT_DATA);
		if (mn == NULL)
			goto drop;

		m->m_next = mn;
		m = mn;

		m->m_len = 0;
	}

	tag = (struct wg_aead_tag *)(mtod(m, uint8_t *) + m->m_len);
	wg_aead_final(&ctx, tag);
	explicit_bzero(&ctx, sizeof(ctx));

	m0->m_pkthdr.len += sizeof(*tag);
	m->m_len += sizeof(*tag);

	m0 = m_prepend(m0, sizeof(*hdr), M_DONTWAIT);
	if (m0 == NULL)
		return (-1);

	hdr = mtod(m0, struct wg_data_hdr *);
	hdr->msg_type = htole32(WG_MSG_DATA);
	hdr->index = wk->wk_tx_idx;
	htolem32(&hdr->counter_lo, counter);
	htolem32(&hdr->counter_hi, counter >> 32);

	if (mq_enqueue(&sc->sc_tx_queue, m0) != 0)
		return (-1);

	task_add(systq, &sc->sc_tx_task);

	return (0);

drop:
	m_freem(m0);
	explicit_bzero(&ctx, sizeof(ctx));
	return (-1);
}

static void
wg_send_keepalive(void *v)
{
	struct wg_softc *sc = v;
	struct wg_data_keys *wk = sc->sc_tx_data_keys;
	struct mbuf *m;

	if (wk == NULL)
		return;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;

	m_align(m, sizeof(struct wg_aead_tag));
	m->m_pkthdr.len = m->m_len = 0;

	wg_encap(sc, wk, m);
}

static void
wg_send(void *v)
{
	struct wg_softc *sc = v;
	struct ifnet *ifp = &sc->sc_if;
	struct file *fp;
	struct socket *so;
	struct mbuf_list ml;
	struct mbuf *m;
	int error;
	int s;

	mq_delist(&sc->sc_tx_queue, &ml);
	if (ml_empty(&ml))
		return;

	fp = sc->sc_fp;
	if (fp == NULL || (so = (struct socket *)fp->f_data) == NULL) {
		ml_purge(&ml);
		return;
	}

	/* XXX this knows too much about how udp_usrreq works internally. */

	s = solock(so);
	while ((m = ml_dequeue(&ml)) != NULL) {
		error = udp_usrreq(so, PRU_SEND, m, NULL, NULL, NULL);
		if (error)
			ifp->if_oerrors++;
	}
	sounlock(so, s);

	sc->sc_tx_stamp = ticks;

}

static void
wg_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct wg_softc *sc = ifp->if_softc;
	struct wg_data_keys *wk = sc->sc_tx_data_keys;
	struct mbuf *m;

	if (sc->sc_fp == NULL || wk == NULL) {
		ifq_purge(ifq);
		return;
	}

	while ((m = ifq_dequeue(ifq)) != NULL) {
#if NBPFILTER
		{
			caddr_t if_bpf = ifp->if_bpf;
			if (if_bpf) {
				bpf_mtap_af(if_bpf, m->m_pkthdr.ph_family, m,
				    BPF_DIRECTION_OUT);
			}
		}
#endif

		if (!wg_encap(sc, wk, m)) {
			ifq->ifq_errors++;
			continue;
		}
	}
}

static void
wg_aead_init(struct wg_aead_ctx *ctx, const struct chacha_key *key,
    uint64_t counter)
{
	uint8_t block0[CHACHA_BLOCKSIZE];

	chacha_stream_keysetup(&ctx->chacha20, key);

	/*
	 * AEAD-ChaCha20-Poly1305 uses the IETF construction with the 96bit
	 * nonce and 32 bit counter starting from 0. WireGuard uses this AEAD
	 * with the counter off the wire as the nonce. That nonce is padded
	 * with zeros on the left, so the layout of the chacha state is
	 * predictable. Set it up directly, rather than stuff bits just so
	 * they can be unstuffed straight away.
	 */
	ctx->chacha20.ctx.input[12] = 0; /* counter starts at 0 */
	ctx->chacha20.ctx.input[13] = 0; /* nonce padding is 0 */
	ctx->chacha20.ctx.input[14] = counter; /* the rest of the "nonce" */
	ctx->chacha20.ctx.input[15] = counter >> 32;
	ctx->chacha20.used = 0;

	/* AEAD-ChaCha20-Poly1305 uses the first block for the poly key */
	bzero(block0, sizeof(block0));
	chacha_stream_update(&ctx->chacha20, block0, block0, sizeof(block0));
	poly1305_init(&ctx->poly1305, block0);
	explicit_bzero(block0, sizeof(block0));

	/* data has no aad */

	ctx->datalen = 0;
}

static void
wg_aead_encrypt(struct wg_aead_ctx *ctx, void *mem, size_t len)
{
	chacha_stream_update(&ctx->chacha20, mem, mem, len);
	poly1305_update(&ctx->poly1305, mem, len);
	ctx->datalen += len;
}

static void
wg_aead_verify(struct wg_aead_ctx *ctx, void *mem, size_t len)
{
	poly1305_update(&ctx->poly1305, mem, len);
	ctx->datalen += len;
}

static void
wg_aead_decrypt(struct wg_aead_ctx *ctx, void *mem, size_t len)
{
	chacha_stream_update(&ctx->chacha20, mem, mem, len);
}

static void
wg_aead_final(struct wg_aead_ctx *ctx, struct wg_aead_tag *tag)
{
	uint8_t len[8];
	uint64_t j;
	unsigned int i;

	/* data has no aad, so aad len is 0 */
	memset(len, 0, sizeof(len));
	poly1305_update(&ctx->poly1305, len, sizeof(len));

	j = ctx->datalen;
	for (i = 0; i < sizeof(len); i++) {
		len[i] = j;
		j >>= 8;
	}
	poly1305_update(&ctx->poly1305, len, sizeof(len));

	poly1305_finish(&ctx->poly1305, tag->tag);
}
