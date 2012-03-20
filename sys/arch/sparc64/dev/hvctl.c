/*	$OpenBSD: hvctl.c,v 1.2 2012/03/20 19:10:55 kettenis Exp $	*/
/*
 * Copyright (c) 2009, 2012 Mark Kettenis
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
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/hypervisor.h>
#include <machine/mdesc.h>

#include <uvm/uvm.h>

#include <sparc64/dev/cbusvar.h>
#include <sparc64/dev/ldcvar.h>

#define HVCTL_DEBUG
#ifdef HVCTL_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#include <sys/ioccom.h>

struct hv_io {
	uint64_t	hi_cookie;
	void		*hi_addr;
	size_t		hi_len;
};

#define HVIOCREAD	_IOW('h', 0, struct hv_io)
#define HVIOCWRITE	_IOW('h', 1, struct hv_io)

#define HVCTL_TX_ENTRIES	32
#define HVCTL_RX_ENTRIES	32

struct hvctl_softc {
	struct device	sc_dv;
	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t	sc_dmatag;

	void		*sc_tx_ih;
	void		*sc_rx_ih;
	uint64_t	sc_tx_sysino;
	uint64_t	sc_rx_sysino;

	struct ldc_conn	sc_lc;
};

int	hvctl_match(struct device *, void *, void *);
void	hvctl_attach(struct device *, struct device *, void *);

struct cfattach hvctl_ca = {
	sizeof(struct hvctl_softc), hvctl_match, hvctl_attach
};

struct cfdriver hvctl_cd = {
	NULL, "hvctl", DV_DULL
};

int	hvctl_tx_intr(void *);
int	hvctl_rx_intr(void *);

struct hvctl_softc *hvctl_sc;

int
hvctl_match(struct device *parent, void *match, void *aux)
{
	struct cbus_attach_args *ca = aux;

	if (strcmp(ca->ca_name, "hvctl") == 0)
		return (1);

	return (0);
}

void
hvctl_attach(struct device *parent, struct device *self, void *aux)
{
	struct hvctl_softc *sc = (struct hvctl_softc *)self;
	struct cbus_attach_args *ca = aux;
	struct ldc_conn *lc;

	sc->sc_bustag = ca->ca_bustag;
	sc->sc_dmatag = ca->ca_dmatag;

	if (cbus_intr_map(ca->ca_node, ca->ca_tx_ino, &sc->sc_tx_sysino) ||
	    cbus_intr_map(ca->ca_node, ca->ca_rx_ino, &sc->sc_rx_sysino)) {
		printf(": can't map interrupt\n");
		return;
	}
	printf(": ivec 0x%lx, 0x%lx", sc->sc_tx_sysino, sc->sc_rx_sysino);

	/*
	 * Un-configure queues before registering interrupt handlers,
	 * such that we dont get any stale LDC packets or events.
	 */
	hv_ldc_tx_qconf(ca->ca_id, 0, 0);
	hv_ldc_rx_qconf(ca->ca_id, 0, 0);

	sc->sc_tx_ih = bus_intr_establish(ca->ca_bustag, sc->sc_tx_sysino,
	    IPL_TTY, 0, hvctl_tx_intr, sc, sc->sc_dv.dv_xname);
	sc->sc_rx_ih = bus_intr_establish(ca->ca_bustag, sc->sc_rx_sysino,
	    IPL_TTY, 0, hvctl_rx_intr, sc, sc->sc_dv.dv_xname);
	if (sc->sc_tx_ih == NULL || sc->sc_rx_ih == NULL) {
		printf(", can't establish interrupt\n");
		return;
	}

	cbus_intr_setenabled(sc->sc_tx_sysino, INTR_DISABLED);
	cbus_intr_setenabled(sc->sc_rx_sysino, INTR_DISABLED);

	lc = &sc->sc_lc;
	lc->lc_id = ca->ca_id;
	lc->lc_sc = sc;

	lc->lc_txq = ldc_queue_alloc(sc->sc_dmatag, HVCTL_TX_ENTRIES);
	if (lc->lc_txq == NULL) {
		printf(", can't allocate tx queue\n");
		return;
	}

	lc->lc_rxq = ldc_queue_alloc(sc->sc_dmatag, HVCTL_RX_ENTRIES);
	if (lc->lc_rxq == NULL) {
		printf(", can't allocate rx queue\n");
		goto free_txqueue;
	}

	hvctl_sc = sc;

	printf(" channel %s\n", ca->ca_name);
	return;

#if 0
free_rxqueue:
	ldc_queue_free(sc->sc_dmatag, lc->lc_rxq);
#endif
free_txqueue:
	ldc_queue_free(sc->sc_dmatag, lc->lc_txq);
}

int
hvctl_tx_intr(void *arg)
{
	struct hvctl_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	uint64_t tx_head, tx_tail, tx_state;
	int err;

	err = hv_ldc_tx_get_state(lc->lc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK) {
		printf("%s: hv_ldc_tx_get_state %d\n", __func__, err);
		return (0);
	}

	if (tx_state != lc->lc_tx_state) {
		switch (tx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Tx link down\n"));
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Tx link up\n"));
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Tx link reset\n"));
			break;
		}
		lc->lc_tx_state = tx_state;
	}

	wakeup(lc->lc_rxq);
	return (1);
}

int
hvctl_rx_intr(void *arg)
{
	struct hvctl_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	uint64_t rx_head, rx_tail, rx_state;
	int err;

	err = hv_ldc_rx_get_state(lc->lc_id, &rx_head, &rx_tail, &rx_state);
	if (err != H_EOK) {
		printf("%s: hv_ldc_rx_get_state %d\n", __func__, err);
		return (0);
	}

	if (rx_state != lc->lc_rx_state) {
		switch (rx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Rx link down\n"));
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Rx link up\n"));
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Rx link reset\n"));
			break;
		}
		lc->lc_rx_state = rx_state;
		return (1);
	}

	if (rx_head == rx_tail)
		return (0);

	cbus_intr_setenabled(sc->sc_rx_sysino, INTR_DISABLED);
	wakeup(lc->lc_rxq);
	return (1);
}

cdev_decl(hvctl);

int
hvctlopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct hvctl_softc *sc;
	struct ldc_conn *lc;
	int err;

	sc = (struct hvctl_softc *)device_lookup(&hvctl_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);
	lc = &sc->sc_lc;

	err = hv_ldc_tx_qconf(lc->lc_id,
	    lc->lc_txq->lq_map->dm_segs[0].ds_addr, lc->lc_txq->lq_nentries);
	if (err != H_EOK)
		printf("%s: hv_ldc_tx_qconf %d\n", __func__, err);

	err = hv_ldc_rx_qconf(lc->lc_id,
	    lc->lc_rxq->lq_map->dm_segs[0].ds_addr, lc->lc_rxq->lq_nentries);
	if (err != H_EOK)
		printf("%d: hv_ldc_rx_qconf %d\n", __func__, err);

	device_unref(&sc->sc_dv);
	return (0);
}

int
hvctlclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct hvctl_softc *sc;

	sc = (struct hvctl_softc *)device_lookup(&hvctl_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);

	cbus_intr_setenabled(sc->sc_tx_sysino, INTR_DISABLED);
	cbus_intr_setenabled(sc->sc_rx_sysino, INTR_DISABLED);

	hv_ldc_tx_qconf(sc->sc_lc.lc_id, 0, 0);
	hv_ldc_rx_qconf(sc->sc_lc.lc_id, 0, 0);

	device_unref(&sc->sc_dv);
	return (0);
}

int
hvctlread(dev_t dev, struct uio *uio, int ioflag)
{
	struct hvctl_softc *sc;
	struct ldc_conn *lc;
	uint64_t rx_head, rx_tail, rx_state;
	int err, ret;
	int s;

	sc = (struct hvctl_softc *)device_lookup(&hvctl_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);
	lc = &sc->sc_lc;

	if (uio->uio_resid != 64) {
		device_unref(&sc->sc_dv);
		return (EINVAL);
	}

	s = spltty();
retry:
	err = hv_ldc_rx_get_state(lc->lc_id, &rx_head, &rx_tail, &rx_state);
	if (err != H_EOK) {
		splx(s);
		printf("%s: hv_ldc_rx_get_state %d\n", __func__, err);
		device_unref(&sc->sc_dv);
		return (EIO);
	}

	if (rx_state != LDC_CHANNEL_UP) {
		splx(s);
		device_unref(&sc->sc_dv);
		return (EIO);
	}

	DPRINTF(("rx head %llx, rx tail %llx\n", rx_head, rx_tail));

	if (rx_head == rx_tail) {
		cbus_intr_setenabled(sc->sc_rx_sysino, INTR_ENABLED);
		ret = tsleep(lc->lc_rxq, PWAIT | PCATCH, "hvrd", 0);
		if (ret) {
			splx(s);
			device_unref(&sc->sc_dv);
			return (ret);
		}
		goto retry;
	}
	splx(s);

	ret = uiomove(lc->lc_rxq->lq_va + rx_head, 64, uio);

	rx_head += 64;
	rx_head &= ((lc->lc_rxq->lq_nentries * 64) - 1);
	err = hv_ldc_rx_set_qhead(lc->lc_id, rx_head);
	if (err != H_EOK)
		printf("%s: hv_ldc_rx_set_qhead %d\n", __func__, err);

	device_unref(&sc->sc_dv);
	return (ret);
}

int
hvctlwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct hvctl_softc *sc;
	struct ldc_conn *lc;
	uint64_t tx_head, tx_tail, tx_state;
	int err, ret;

	sc = (struct hvctl_softc *)device_lookup(&hvctl_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);
	lc = &sc->sc_lc;

	if (uio->uio_resid != 64) {
		device_unref(&sc->sc_dv);
		return (EINVAL);
	}

#if 0
retry:
#endif
	err = hv_ldc_tx_get_state(lc->lc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK) {
		printf("%s: hv_ldc_tx_get_state %d\n", __func__, err);
		device_unref(&sc->sc_dv);
		return (EIO);
	}

	if (tx_state != LDC_CHANNEL_UP) {
		device_unref(&sc->sc_dv);
		return (EIO);
	}

	DPRINTF(("tx head %llx, tx tail %llx\n", tx_head, tx_tail));

#if 0
	if ((tx_head == tx_tail)) {
		ret = tsleep(lc->lc_txq, PWAIT | PCATCH, "hvwr", 0);
		if (ret) {
			device_unref(&sc->sc_dv);
			return (ret);
		}
		goto retry;
	}
#endif

	ret = uiomove(lc->lc_txq->lq_va + tx_tail, 64, uio);

	tx_tail += 64;
	tx_tail &= ((lc->lc_txq->lq_nentries * 64) - 1);
	err = hv_ldc_tx_set_qtail(lc->lc_id, tx_tail);
	if (err != H_EOK) {
		printf("%s: hv_ldc_tx_set_qtail: %d\n", __func__, err);
		device_unref(&sc->sc_dv);
		return (EIO);
	}

	device_unref(&sc->sc_dv);
	return (ret);
}

int
hvctlioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct hvctl_softc *sc;
	struct ldc_conn *lc;
	struct hv_io *hi = (struct hv_io *)data;
	paddr_t pa, offset;
	psize_t nbytes;
	caddr_t buf;
	size_t size;
	int err;

	sc = (struct hvctl_softc *)device_lookup(&hvctl_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);
	lc = &sc->sc_lc;

	switch (cmd) {
	case HVIOCREAD:
	case HVIOCWRITE:
		break;
	default:
		device_unref(&sc->sc_dv);
		return (ENOTTY);
	}

	buf = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);

	switch(cmd) {
	case HVIOCREAD:
		size = hi->hi_len;
		offset = 0;
		while (size > 0) {
			pmap_extract(pmap_kernel(), (vaddr_t)buf, &pa);
			nbytes = min(PAGE_SIZE, size);
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_IN,
			    hi->hi_cookie + offset, pa, nbytes, &nbytes);
			if (err != H_EOK) {
				printf("hv_ldc_copy %d\n", err);
				free(buf, M_DEVBUF);
				device_unref(&sc->sc_dv);
				return (EINVAL);
			}
			err = copyout(buf, (caddr_t)hi->hi_addr + offset, nbytes);
			if (err) {
				free(buf, M_DEVBUF);
				device_unref(&sc->sc_dv);
				return (err);
			}
			size -= nbytes;
			offset += nbytes;
		}
		break;
	case HVIOCWRITE:
		size = hi->hi_len;
		offset = 0;
		while (size > 0) {
			pmap_extract(pmap_kernel(), (vaddr_t)buf, &pa);
			nbytes = min(PAGE_SIZE, size);
			err = copyin((caddr_t)hi->hi_addr + offset, buf, nbytes);
			if (err) {
				free(buf, M_DEVBUF);
				device_unref(&sc->sc_dv);
				return (err);
			}
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_OUT,
			    hi->hi_cookie + offset, pa, nbytes, &nbytes);
			if (err != H_EOK) {
				printf("hv_ldc_copy %d\n", err);
				free(buf, M_DEVBUF);
				device_unref(&sc->sc_dv);
				return (EINVAL);
			}
			size -= nbytes;
			offset += nbytes;
		}
		break;

	}

	free(buf, M_DEVBUF);

	device_unref(&sc->sc_dv);
	return (0);
}

int
hvctlpoll(dev_t dev, int events, struct proc *p)
{
	struct hvctl_softc *sc;
	struct ldc_conn *lc;
	uint64_t head, tail, state;
	int revents = 0;
	int err;

	sc = (struct hvctl_softc *)device_lookup(&hvctl_cd, minor(dev));
	if (sc == NULL)
		return (ENXIO);
	lc = &sc->sc_lc;

	if (events & (POLLIN | POLLRDNORM)) {
		err = hv_ldc_rx_get_state(lc->lc_id, &head, &tail, &state);

		if (err == 0 && state == LDC_CHANNEL_UP && head != tail)
			revents |= events & (POLLIN | POLLRDNORM);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		err = hv_ldc_tx_get_state(lc->lc_id, &head, &tail, &state);

		if (err == 0 && state == LDC_CHANNEL_UP && head != tail)
			revents |= events & (POLLOUT | POLLWRNORM);
	}

	return revents;
}
