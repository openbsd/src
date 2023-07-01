/*	$OpenBSD: qcpas.c,v 1.2 2023/07/01 15:50:18 drahn Exp $	*/
/*
 * Copyright (c) 2023 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/atomic.h>
#include <sys/exec_elf.h>
#include <sys/task.h>

#include <machine/apmvar.h>
#include <machine/bus.h>
#include <machine/fdt.h>
#include <uvm/uvm_extern.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_power.h>
#include <dev/ofw/fdt.h>

#include "apm.h"

#define MDT_TYPE_MASK				(7 << 24)
#define MDT_TYPE_HASH				(2 << 24)
#define MDT_RELOCATABLE				(1 << 27)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct qcpas_dmamem {
	bus_dmamap_t		tdm_map;
	bus_dma_segment_t	tdm_seg;
	size_t			tdm_size;
	caddr_t			tdm_kva;
};
#define QCPAS_DMA_MAP(_tdm)	((_tdm)->tdm_map)
#define QCPAS_DMA_LEN(_tdm)	((_tdm)->tdm_size)
#define QCPAS_DMA_DVA(_tdm)	((_tdm)->tdm_map->dm_segs[0].ds_addr)
#define QCPAS_DMA_KVA(_tdm)	((void *)(_tdm)->tdm_kva)

struct qcpas_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
	int			sc_node;

	void			*sc_ih[6];

	paddr_t			sc_mem_phys;
	size_t			sc_mem_size;
	void			*sc_mem_region;
	vaddr_t			sc_mem_reloc;

	uint32_t		sc_pas_id;
	char			*sc_load_state;

	struct qcpas_dmamem	*sc_metadata;

	/* GLINK */
	volatile uint32_t	*sc_tx_tail;
	volatile uint32_t	*sc_tx_head;
	volatile uint32_t	*sc_rx_tail;
	volatile uint32_t	*sc_rx_head;

	uint32_t		sc_tx_off;
	uint32_t		sc_rx_off;

	uint8_t			*sc_tx_fifo;
	int			sc_tx_fifolen;
	uint8_t			*sc_rx_fifo;
	int			sc_rx_fifolen;
	void			*sc_glink_ih;

	struct mbox_channel	*sc_mc;

	struct task		sc_glink_rx;
	uint32_t		sc_glink_max_channel;
	TAILQ_HEAD(,qcpas_glink_channel) sc_glink_channels;
};

int	qcpas_match(struct device *, void *, void *);
void	qcpas_attach(struct device *, struct device *, void *);

const struct cfattach qcpas_ca = {
	sizeof (struct qcpas_softc), qcpas_match, qcpas_attach
};

struct cfdriver qcpas_cd = {
	NULL, "qcpas", DV_DULL
};

void	qcpas_mountroot(struct device *);
int	qcpas_map_memory(struct qcpas_softc *);
int	qcpas_mdt_init(struct qcpas_softc *, u_char *, size_t);
void	qcpas_glink_attach(struct qcpas_softc *, int);

struct qcpas_dmamem *
	qcpas_dmamem_alloc(struct qcpas_softc *, bus_size_t, bus_size_t);
void	qcpas_dmamem_free(struct qcpas_softc *, struct qcpas_dmamem *);

void	qcpas_intr_establish(struct qcpas_softc *, int, char *, void *);
int	qcpas_intr_wdog(void *);
int	qcpas_intr_fatal(void *);
int	qcpas_intr_ready(void *);
int	qcpas_intr_handover(void *);
int	qcpas_intr_stop_ack(void *);
int	qcpas_intr_shutdown_ack(void *);

int
qcpas_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "qcom,sc8280xp-adsp-pas");
}

void
qcpas_attach(struct device *parent, struct device *self, void *aux)
{
	struct qcpas_softc *sc = (struct qcpas_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_dmat = faa->fa_dmat;
	sc->sc_node = faa->fa_node;

	if (OF_is_compatible(faa->fa_node, "qcom,sc8280xp-adsp-pas")) {
		sc->sc_pas_id = 1;
		sc->sc_load_state = "adsp";
	}
	if (OF_is_compatible(faa->fa_node, "qcom,sc8280xp-nsp0-pas")) {
		sc->sc_pas_id = 18;
	}
	if (OF_is_compatible(faa->fa_node, "qcom,sc8280xp-nsp1-pas")) {
		sc->sc_pas_id = 30;
	}

	qcpas_intr_establish(sc, 0, "wdog", qcpas_intr_wdog);
	qcpas_intr_establish(sc, 1, "fatal", qcpas_intr_fatal);
	qcpas_intr_establish(sc, 2, "ready", qcpas_intr_ready);
	qcpas_intr_establish(sc, 3, "handover", qcpas_intr_handover);
	qcpas_intr_establish(sc, 4, "stop-ack", qcpas_intr_stop_ack);
	qcpas_intr_establish(sc, 5, "shutdown-ack", qcpas_intr_shutdown_ack);

	printf("\n");

	config_mountroot(self, qcpas_mountroot);
}

extern int qcaoss_send(char *, size_t);

void
qcpas_mountroot(struct device *self)
{
	struct qcpas_softc *sc = (struct qcpas_softc *)self;
	char fwname[64];
	size_t fwlen;
	u_char *fw;
	int node, ret;

	if (qcpas_map_memory(sc) != 0)
		return;

	if (OF_getproplen(sc->sc_node, "firmware-name") <= 0)
		return;
	OF_getprop(sc->sc_node, "firmware-name", fwname, sizeof(fwname));
	fwname[sizeof(fwname) - 1] = '\0';

	if (loadfirmware(fwname, &fw, &fwlen) != 0) {
		printf("%s: failed to load %s\n",
		    sc->sc_dev.dv_xname, fwname);
		return;
	}

	if (sc->sc_load_state) {
		char buf[64];
		snprintf(buf, sizeof(buf),
		    "{class: image, res: load_state, name: %s, val: on}",
		    sc->sc_load_state);
		ret = qcaoss_send(buf, sizeof(buf));
		if (ret != 0) {
			printf("%s: failed to toggle load state\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	power_domain_enable_all(sc->sc_node);
	clock_enable(sc->sc_node, "xo");

	ret = qcpas_mdt_init(sc, fw, fwlen);
	free(fw, M_DEVBUF, fwlen);
	if (ret != 0) {
		printf("%s: failed to boot coprocessor\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	node = OF_getnodebyname(sc->sc_node, "glink-edge");
	if (node)
		qcpas_glink_attach(sc, node);
}

int
qcpas_map_memory(struct qcpas_softc *sc)
{
	uint32_t phandle, reg[4];
	size_t off;
	int node;

	phandle = OF_getpropint(sc->sc_node, "memory-region", 0);
	if (phandle == 0)
		return EINVAL;
	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return EINVAL;
	if (OF_getpropintarray(node, "reg", reg, sizeof(reg)) != sizeof(reg))
		return EINVAL;

	sc->sc_mem_phys = (uint64_t)reg[0] << 32 | reg[1];
	KASSERT((sc->sc_mem_phys & PAGE_MASK) == 0);
	sc->sc_mem_size = (uint64_t)reg[2] << 32 | reg[3];
	KASSERT((sc->sc_mem_size & PAGE_MASK) == 0);

	sc->sc_mem_region = km_alloc(sc->sc_mem_size, &kv_any, &kp_none,
	    &kd_nowait);
	if (!sc->sc_mem_region)
		return ENOMEM;

	for (off = 0; off < sc->sc_mem_size; off += PAGE_SIZE) {
		pmap_kenter_cache((vaddr_t)sc->sc_mem_region + off,
		    sc->sc_mem_phys + off, PROT_READ | PROT_WRITE,
		    PMAP_CACHE_DEV_NGNRNE);
	}

	return 0;
}

extern int qcscm_pas_init_image(uint32_t, paddr_t);
extern int qcscm_pas_mem_setup(uint32_t, paddr_t, size_t);
extern int qcscm_pas_auth_and_reset(uint32_t);

int
qcpas_mdt_init(struct qcpas_softc *sc, u_char *fw, size_t fwlen)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	paddr_t minpa = -1, maxpa = 0;
	int i, hashseg = 0, relocate = 0;
	int error;
	ssize_t off;

	ehdr = (Elf32_Ehdr *)fw;
	phdr = (Elf32_Phdr *)&ehdr[1];

	if (ehdr->e_phnum < 2 || phdr[0].p_type == PT_LOAD)
		return EINVAL;

	for (i = 0; i < ehdr->e_phnum; i++) {
		if ((phdr[i].p_flags & MDT_TYPE_MASK) == MDT_TYPE_HASH) {
			if (i > 0 && !hashseg)
				hashseg = i;
			continue;
		}
		if (phdr[i].p_type != PT_LOAD || phdr[i].p_memsz == 0)
			continue;
		if (phdr[i].p_flags & MDT_RELOCATABLE)
			relocate = 1;
		if (phdr[i].p_paddr < minpa)
			minpa = phdr[i].p_paddr;
		if (phdr[i].p_paddr + phdr[i].p_memsz > maxpa)
			maxpa =
			    roundup(phdr[i].p_paddr + phdr[i].p_memsz,
			    PAGE_SIZE);
	}

	if (!hashseg)
		return EINVAL;

	sc->sc_metadata = qcpas_dmamem_alloc(sc, phdr[0].p_filesz +
	    phdr[hashseg].p_filesz, PAGE_SIZE);
	if (sc->sc_metadata == NULL)
		return EINVAL;

	memcpy(QCPAS_DMA_KVA(sc->sc_metadata), fw, phdr[0].p_filesz);
	if (phdr[0].p_filesz + phdr[hashseg].p_filesz == fwlen) {
		memcpy(QCPAS_DMA_KVA(sc->sc_metadata) + phdr[0].p_filesz,
		    fw + phdr[0].p_filesz, phdr[hashseg].p_filesz);
	} else if (phdr[hashseg].p_offset + phdr[hashseg].p_filesz <= fwlen) {
		memcpy(QCPAS_DMA_KVA(sc->sc_metadata) + phdr[0].p_filesz,
		    fw + phdr[hashseg].p_offset, phdr[hashseg].p_filesz);
	} else {
		printf("%s: metadata split segment not supported\n",
		    sc->sc_dev.dv_xname);
		return EINVAL;
	}

	membar_producer();

	if (qcscm_pas_init_image(sc->sc_pas_id,
	    QCPAS_DMA_DVA(sc->sc_metadata)) != 0) {
		printf("%s: init image failed\n", sc->sc_dev.dv_xname);
		qcpas_dmamem_free(sc, sc->sc_metadata);
		return EINVAL;
	}

	if (qcscm_pas_mem_setup(sc->sc_pas_id,
	    sc->sc_mem_phys, maxpa - minpa) != 0) {
		printf("%s: mem setup failed\n", sc->sc_dev.dv_xname);
		qcpas_dmamem_free(sc, sc->sc_metadata);
		return EINVAL;
	}

	sc->sc_mem_reloc = relocate ? minpa : sc->sc_mem_phys;

	for (i = 0; i < ehdr->e_phnum; i++) {
		if ((phdr[i].p_flags & MDT_TYPE_MASK) == MDT_TYPE_HASH ||
		    phdr[i].p_type != PT_LOAD || phdr[i].p_memsz == 0)
			continue;
		off = phdr[i].p_paddr - sc->sc_mem_reloc;
		if (off < 0 || off + phdr[i].p_memsz > sc->sc_mem_size)
			return EINVAL;
		if (phdr[i].p_filesz > phdr[i].p_memsz)
			return EINVAL;

		if (phdr[i].p_filesz && phdr[i].p_offset < fwlen &&
		    phdr[i].p_offset + phdr[i].p_filesz <= fwlen) {
			memcpy(sc->sc_mem_region + off, fw + phdr[i].p_offset,
			    phdr[i].p_filesz);
		} else if (phdr[i].p_filesz) {
			printf("%s: firmware split segment not supported\n",
			    sc->sc_dev.dv_xname);
			return EINVAL;
		}

		if (phdr[i].p_memsz > phdr[i].p_filesz)
			memset(sc->sc_mem_region + off + phdr[i].p_filesz, 0,
			    phdr[i].p_memsz - phdr[i].p_filesz);
	}

	membar_producer();

	if (qcscm_pas_auth_and_reset(sc->sc_pas_id) != 0) {
		printf("%s: auth and reset failed\n", sc->sc_dev.dv_xname);
		qcpas_dmamem_free(sc, sc->sc_metadata);
		return EINVAL;
	}

	error = tsleep_nsec(sc, PWAIT, "qcpas", SEC_TO_NSEC(5));
	if (error) {
		printf("%s: failed to receive ready signal\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	/* XXX: free metadata ? */

	return 0;
}

struct qcpas_dmamem *
qcpas_dmamem_alloc(struct qcpas_softc *sc, bus_size_t size, bus_size_t align)
{
	struct qcpas_dmamem *tdm;
	int nsegs;

	tdm = malloc(sizeof(*tdm), M_DEVBUF, M_WAITOK | M_ZERO);
	tdm->tdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &tdm->tdm_map) != 0)
		goto tdmfree;

	if (bus_dmamem_alloc_range(sc->sc_dmat, size, align, 0,
	    &tdm->tdm_seg, 1, &nsegs, BUS_DMA_WAITOK, 0, 0xffffffff) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &tdm->tdm_seg, nsegs, size,
	    &tdm->tdm_kva, BUS_DMA_WAITOK | BUS_DMA_COHERENT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, tdm->tdm_map, tdm->tdm_kva, size,
	    NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	bzero(tdm->tdm_kva, size);

	return (tdm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, tdm->tdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &tdm->tdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, tdm->tdm_map);
tdmfree:
	free(tdm, M_DEVBUF, 0);

	return (NULL);
}

void
qcpas_dmamem_free(struct qcpas_softc *sc, struct qcpas_dmamem *tdm)
{
	bus_dmamem_unmap(sc->sc_dmat, tdm->tdm_kva, tdm->tdm_size);
	bus_dmamem_free(sc->sc_dmat, &tdm->tdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, tdm->tdm_map);
	free(tdm, M_DEVBUF, 0);
}

void
qcpas_intr_establish(struct qcpas_softc *sc, int i, char *name, void *handler)
{
	int idx;

	idx = OF_getindex(sc->sc_node, name, "interrupt-names");
	if (idx >= 0)
		sc->sc_ih[i] =
		    fdt_intr_establish_idx(sc->sc_node, idx, IPL_BIO,
		    handler, sc, sc->sc_dev.dv_xname);
}

int
qcpas_intr_wdog(void *cookie)
{
	return 0;
}

int
qcpas_intr_fatal(void *cookie)
{
	return 0;
}

int
qcpas_intr_ready(void *cookie)
{
	struct qcpas_softc *sc = cookie;

	wakeup(sc);
	return 0;
}

int
qcpas_intr_handover(void *cookie)
{
	return 0;
}

int
qcpas_intr_stop_ack(void *cookie)
{
	return 0;
}

int
qcpas_intr_shutdown_ack(void *cookie)
{
	return 0;
}

/* GLINK */

#define SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR	478
#define SMEM_GLINK_NATIVE_XPRT_FIFO_0		479
#define SMEM_GLINK_NATIVE_XPRT_FIFO_1		480

struct glink_msg {
	uint16_t cmd;
	uint16_t param1;
	uint32_t param2;
	uint8_t data[];
} __packed;

struct qcpas_glink_intent_pair {
	uint32_t size;
	uint32_t iid;
} __packed;

struct qcpas_glink_intent {
	TAILQ_ENTRY(qcpas_glink_intent) it_q;
	uint32_t it_id;
	uint32_t it_size;
	int it_inuse;
};

struct qcpas_glink_channel {
	TAILQ_ENTRY(qcpas_glink_channel) ch_q;
	struct qcpas_softc *ch_sc;
	struct qcpas_glink_protocol *ch_proto;
	uint32_t ch_rcid;
	uint32_t ch_lcid;
	uint32_t ch_max_intent;
	TAILQ_HEAD(,qcpas_glink_intent) ch_l_intents;
	TAILQ_HEAD(,qcpas_glink_intent) ch_r_intents;
};

#define GLINK_CMD_VERSION		0
#define GLINK_CMD_VERSION_ACK		1
#define  GLINK_VERSION				1
#define  GLINK_FEATURE_INTENT_REUSE		(1 << 0)
#define GLINK_CMD_OPEN			2
#define GLINK_CMD_CLOSE			3
#define GLINK_CMD_OPEN_ACK		4
#define GLINK_CMD_INTENT		5
#define GLINK_CMD_RX_DONE		6
#define GLINK_CMD_RX_INTENT_REQ		7
#define GLINK_CMD_RX_INTENT_REQ_ACK	8
#define GLINK_CMD_TX_DATA		9
#define GLINK_CMD_CLOSE_ACK		11
#define GLINK_CMD_TX_DATA_CONT		12
#define GLINK_CMD_READ_NOTIF		13
#define GLINK_CMD_RX_DONE_W_REUSE	14

void	qcpas_glink_recv(void *);
int	qcpas_glink_intr(void *);

void	qcpas_glink_tx(struct qcpas_softc *, uint8_t *, int);
void	qcpas_glink_tx_commit(struct qcpas_softc *);
void	qcpas_glink_rx(struct qcpas_softc *, uint8_t *, int);
void	qcpas_glink_rx_commit(struct qcpas_softc *);

void	qcpas_glink_send(void *, void *, int);

extern int qcsmem_alloc(int, int, int);
extern void *qcsmem_get(int, int, int *);

int	qcpas_pmic_rtr_init(void *);
int	qcpas_pmic_rtr_recv(void *, uint8_t *, int);
int	qcpas_pmic_rtr_apminfo(struct apm_power_info *);

struct qcpas_glink_protocol {
	char *name;
	int (*init)(void *cookie);
	int (*recv)(void *cookie, uint8_t *buf, int len);
} qcpas_glink_protocols[] = {
	{ "PMIC_RTR_ADSP_APPS", qcpas_pmic_rtr_init , qcpas_pmic_rtr_recv },
};

void
qcpas_glink_attach(struct qcpas_softc *sc, int node)
{
	uint32_t remote;
	uint32_t *descs;
	int size;

	remote = OF_getpropint(node, "qcom,remote-pid", -1);
	if (remote == -1)
		return;

	if (qcsmem_alloc(remote, SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR, 32) != 0 ||
	    qcsmem_alloc(remote, SMEM_GLINK_NATIVE_XPRT_FIFO_0, 16384) != 0)
		return;

	descs = qcsmem_get(remote, SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR, &size);
	if (descs == NULL || size != 32)
		return;

	sc->sc_tx_tail = &descs[0];
	sc->sc_tx_head = &descs[1];
	sc->sc_rx_tail = &descs[2];
	sc->sc_rx_head = &descs[3];

	sc->sc_tx_fifo = qcsmem_get(remote, SMEM_GLINK_NATIVE_XPRT_FIFO_0,
	    &sc->sc_tx_fifolen);
	if (sc->sc_tx_fifo == NULL)
		return;
	sc->sc_rx_fifo = qcsmem_get(remote, SMEM_GLINK_NATIVE_XPRT_FIFO_1,
	    &sc->sc_rx_fifolen);
	if (sc->sc_rx_fifo == NULL)
		return;

	sc->sc_mc = mbox_channel_idx(node, 0, NULL);
	if (sc->sc_mc == NULL)
		return;

	TAILQ_INIT(&sc->sc_glink_channels);
	task_set(&sc->sc_glink_rx, qcpas_glink_recv, sc);

	sc->sc_glink_ih = fdt_intr_establish(node, IPL_BIO,
	    qcpas_glink_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_glink_ih == NULL)
		return;

	/* Expect peer to send initial message */
}

void
qcpas_glink_rx(struct qcpas_softc *sc, uint8_t *buf, int len)
{
	uint32_t head, tail;
	int avail;

	head = *sc->sc_rx_head;
	tail = *sc->sc_rx_tail + sc->sc_rx_off;
	if (tail >= sc->sc_rx_fifolen)
		tail -= sc->sc_rx_fifolen;

	/* Checked by caller */
	KASSERT(head != tail);

	if (head >= tail)
		avail = head - tail;
	else
		avail = (sc->sc_rx_fifolen - tail) + head;

	/* Dumb, but should do. */
	KASSERT(avail >= len);

	while (len > 0) {
		*buf = sc->sc_rx_fifo[tail];
		tail++;
		if (tail >= sc->sc_rx_fifolen)
			tail -= sc->sc_rx_fifolen;
		buf++;
		sc->sc_rx_off++;
		len--;
	}
}

void
qcpas_glink_rx_commit(struct qcpas_softc *sc)
{
	uint32_t tail;

	tail = *sc->sc_rx_tail + roundup(sc->sc_rx_off, 8);
	if (tail >= sc->sc_rx_fifolen)
		tail -= sc->sc_rx_fifolen;

	membar_producer();
	*sc->sc_rx_tail = tail;
	sc->sc_rx_off = 0;
}

void
qcpas_glink_tx(struct qcpas_softc *sc, uint8_t *buf, int len)
{
	uint32_t head, tail;
	int avail;

	head = *sc->sc_tx_head + sc->sc_tx_off;
	if (head >= sc->sc_tx_fifolen)
		head -= sc->sc_tx_fifolen;
	tail = *sc->sc_tx_tail;

	if (head < tail)
		avail = tail - head;
	else
		avail = (sc->sc_rx_fifolen - head) + tail;

	/* Dumb, but should do. */
	KASSERT(avail >= len);

	while (len > 0) {
		sc->sc_tx_fifo[head] = *buf;
		head++;
		if (head >= sc->sc_tx_fifolen)
			head -= sc->sc_tx_fifolen;
		buf++;
		sc->sc_tx_off++;
		len--;
	}
}

void
qcpas_glink_tx_commit(struct qcpas_softc *sc)
{
	uint32_t head;

	head = *sc->sc_tx_head + roundup(sc->sc_tx_off, 8);
	if (head >= sc->sc_tx_fifolen)
		head -= sc->sc_tx_fifolen;

	membar_producer();
	*sc->sc_tx_head = head;
	sc->sc_tx_off = 0;
	mbox_send(sc->sc_mc, NULL, 0);
}

void
qcpas_glink_send(void *cookie, void *buf, int len)
{
	struct qcpas_glink_channel *ch = cookie;
	struct qcpas_softc *sc = ch->ch_sc;
	struct qcpas_glink_intent *it;
	struct glink_msg msg;
	uint32_t chunk_size, left_size;

	TAILQ_FOREACH(it, &ch->ch_r_intents, it_q) {
		if (!it->it_inuse)
			break;
		if (it->it_size < len)
			continue;
	}
	if (it == NULL) {
		printf("%s: all intents in use\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	it->it_inuse = 1;

	msg.cmd = GLINK_CMD_TX_DATA;
	msg.param1 = ch->ch_lcid;
	msg.param2 = it->it_id;

	chunk_size = len;
	left_size = 0;

	qcpas_glink_tx(sc, (char *)&msg, sizeof(msg));
	qcpas_glink_tx(sc, (char *)&chunk_size, sizeof(chunk_size));
	qcpas_glink_tx(sc, (char *)&left_size, sizeof(left_size));
	qcpas_glink_tx(sc, buf, len);
	qcpas_glink_tx_commit(sc);
}

void
qcpas_glink_recv_version(struct qcpas_softc *sc, uint32_t version,
    uint32_t features)
{
	struct glink_msg msg;

	if (version != GLINK_VERSION) {
		printf("%s: unsupported glink version %u\n",
		    sc->sc_dev.dv_xname, version);
		return;
	}

	msg.cmd = GLINK_CMD_VERSION_ACK;
	msg.param1 = GLINK_VERSION;
	msg.param2 = features & GLINK_FEATURE_INTENT_REUSE;

	qcpas_glink_tx(sc, (char *)&msg, sizeof(msg));
	qcpas_glink_tx_commit(sc);
}

void
qcpas_glink_recv_open(struct qcpas_softc *sc, uint32_t rcid, uint32_t namelen)
{
	struct qcpas_glink_protocol *proto = NULL;
	struct qcpas_glink_channel *ch;
	struct glink_msg msg;
	char *name;
	int i, err;

	name = malloc(namelen, M_TEMP, M_WAITOK);
	qcpas_glink_rx(sc, name, namelen);
	qcpas_glink_rx_commit(sc);

	TAILQ_FOREACH(ch, &sc->sc_glink_channels, ch_q) {
		if (ch->ch_rcid == rcid) {
			printf("%s: duplicate open for %s\n",
			    sc->sc_dev.dv_xname, name);
			free(name, M_TEMP, namelen);
			return;
		}
	}

	for (i = 0; i < nitems(qcpas_glink_protocols); i++) {
		if (strcmp(qcpas_glink_protocols[i].name, name) != 0)
			continue;
		proto = &qcpas_glink_protocols[i];
		break;
	}
	if (proto == NULL) {
		free(name, M_TEMP, namelen);
		return;
	}

	/* Assume we can leave HW dangling if proto init fails */
	err = proto->init(NULL);
	if (err) {
		free(name, M_TEMP, namelen);
		return;
	}

	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	ch->ch_sc = sc;
	ch->ch_proto = proto;
	ch->ch_rcid = rcid;
	ch->ch_lcid = ++sc->sc_glink_max_channel;
	TAILQ_INIT(&ch->ch_l_intents);
	TAILQ_INIT(&ch->ch_r_intents);
	TAILQ_INSERT_TAIL(&sc->sc_glink_channels, ch, ch_q);

	msg.cmd = GLINK_CMD_OPEN_ACK;
	msg.param1 = ch->ch_rcid;
	msg.param2 = 0;

	qcpas_glink_tx(sc, (char *)&msg, sizeof(msg));
	qcpas_glink_tx_commit(sc);

	msg.cmd = GLINK_CMD_OPEN;
	msg.param1 = ch->ch_lcid;
	msg.param2 = strlen(name) + 1;

	qcpas_glink_tx(sc, (char *)&msg, sizeof(msg));
	qcpas_glink_tx(sc, name, strlen(name) + 1);
	qcpas_glink_tx_commit(sc);

	free(name, M_TEMP, namelen);
}

void
qcpas_glink_recv_open_ack(struct qcpas_softc *sc, uint32_t lcid)
{
	struct qcpas_glink_channel *ch;
	struct glink_msg msg;
	struct qcpas_glink_intent_pair intent;
	int i;

	TAILQ_FOREACH(ch, &sc->sc_glink_channels, ch_q) {
		if (ch->ch_lcid == lcid)
			break;
	}
	if (ch == NULL) {
		printf("%s: unknown channel %u for OPEN_ACK\n",
		    sc->sc_dev.dv_xname, lcid);
		return;
	}

	/* Respond with default intent now that channel is open */
	for (i = 0; i < 5; i++) {
		struct qcpas_glink_intent *it;

		it = malloc(sizeof(*it), M_DEVBUF, M_WAITOK | M_ZERO);
		it->it_id = ++ch->ch_max_intent;
		it->it_size = 1024;
		TAILQ_INSERT_TAIL(&ch->ch_l_intents, it, it_q);

		msg.cmd = GLINK_CMD_INTENT;
		msg.param1 = ch->ch_lcid;
		msg.param2 = 1;
		intent.size = it->it_size;
		intent.iid = it->it_id;
	}

	qcpas_glink_tx(sc, (char *)&msg, sizeof(msg));
	qcpas_glink_tx(sc, (char *)&intent, sizeof(intent));
	qcpas_glink_tx_commit(sc);
}

void
qcpas_glink_recv_intent(struct qcpas_softc *sc, uint32_t rcid, uint32_t count)
{
	struct qcpas_glink_intent_pair *intents;
	struct qcpas_glink_channel *ch;
	struct qcpas_glink_intent *it;
	int i;

	intents = malloc(sizeof(*intents) * count, M_TEMP, M_WAITOK);
	qcpas_glink_rx(sc, (char *)intents, sizeof(*intents) * count);
	qcpas_glink_rx_commit(sc);

	TAILQ_FOREACH(ch, &sc->sc_glink_channels, ch_q) {
		if (ch->ch_rcid == rcid)
			break;
	}
	if (ch == NULL) {
		printf("%s: unknown channel %u for INTENT\n",
		    sc->sc_dev.dv_xname, rcid);
		free(intents, M_TEMP, sizeof(*intents) * count);
		return;
	}

	for (i = 0; i < count; i++) {
		it = malloc(sizeof(*it), M_DEVBUF, M_WAITOK | M_ZERO);
		it->it_id = intents[i].iid;
		it->it_size = intents[i].size;
		TAILQ_INSERT_TAIL(&ch->ch_r_intents, it, it_q);
	}

	free(intents, M_TEMP, sizeof(*intents) * count);
}

void
qcpas_glink_recv_tx_data(struct qcpas_softc *sc, uint32_t rcid, uint32_t liid)
{
	struct qcpas_glink_channel *ch;
	struct qcpas_glink_intent *it;
	struct glink_msg msg;
	uint32_t chunk_size, left_size;
	char *buf;

	qcpas_glink_rx(sc, (char *)&chunk_size, sizeof(chunk_size));
	qcpas_glink_rx(sc, (char *)&left_size, sizeof(left_size));
	qcpas_glink_rx_commit(sc);

	buf = malloc(chunk_size, M_TEMP, M_WAITOK);
	qcpas_glink_rx(sc, buf, chunk_size);
	qcpas_glink_rx_commit(sc);

	TAILQ_FOREACH(ch, &sc->sc_glink_channels, ch_q) {
		if (ch->ch_rcid == rcid)
			break;
	}
	if (ch == NULL) {
		printf("%s: unknown channel %u for TX_DATA\n",
		    sc->sc_dev.dv_xname, rcid);
		free(buf, M_TEMP, chunk_size);
		return;
	}

	TAILQ_FOREACH(it, &ch->ch_l_intents, it_q) {
		if (it->it_id == liid)
			break;
	}
	if (it == NULL) {
		printf("%s: unknown intent %u for TX_DATA\n",
		    sc->sc_dev.dv_xname, liid);
		free(buf, M_TEMP, chunk_size);
		return;
	}

	/* FIXME: handle message chunking */
	KASSERT(left_size == 0);

	ch->ch_proto->recv(ch, buf, chunk_size);
	free(buf, M_TEMP, chunk_size);

	if (!left_size) {
		msg.cmd = GLINK_CMD_RX_DONE_W_REUSE;
		msg.param1 = ch->ch_lcid;
		msg.param2 = it->it_id;

		qcpas_glink_tx(sc, (char *)&msg, sizeof(msg));
		qcpas_glink_tx_commit(sc);
	}
}

void
qcpas_glink_recv_rx_done(struct qcpas_softc *sc, uint32_t rcid, uint32_t riid,
    int reuse)
{
	struct qcpas_glink_channel *ch;
	struct qcpas_glink_intent *it;

	TAILQ_FOREACH(ch, &sc->sc_glink_channels, ch_q) {
		if (ch->ch_rcid == rcid)
			break;
	}
	if (ch == NULL) {
		printf("%s: unknown channel %u for RX_DONE\n",
		    sc->sc_dev.dv_xname, rcid);
		return;
	}

	TAILQ_FOREACH(it, &ch->ch_r_intents, it_q) {
		if (it->it_id == riid)
			break;
	}
	if (it == NULL) {
		printf("%s: unknown intent %u for RX_DONE\n",
		    sc->sc_dev.dv_xname, riid);
		return;
	}

	/* FIXME: handle non-reuse */
	KASSERT(reuse);

	KASSERT(it->it_inuse);
	it->it_inuse = 0;
}

void
qcpas_glink_recv(void *cookie)
{
	struct qcpas_softc *sc = cookie;
	struct glink_msg msg;

	while (*sc->sc_rx_tail != *sc->sc_rx_head) {
		membar_consumer();
		qcpas_glink_rx(sc, (uint8_t *)&msg, sizeof(msg));
		qcpas_glink_rx_commit(sc);

		switch (msg.cmd) {
		case GLINK_CMD_VERSION:
			qcpas_glink_recv_version(sc, msg.param1, msg.param2);
			break;
		case GLINK_CMD_OPEN:
			qcpas_glink_recv_open(sc, msg.param1, msg.param2);
			break;
		case GLINK_CMD_OPEN_ACK:
			qcpas_glink_recv_open_ack(sc, msg.param1);
			break;
		case GLINK_CMD_INTENT:
			qcpas_glink_recv_intent(sc, msg.param1, msg.param2);
			break;
		case GLINK_CMD_RX_INTENT_REQ:
			/* Nothing to do so far */
			break;
		case GLINK_CMD_TX_DATA:
			qcpas_glink_recv_tx_data(sc, msg.param1, msg.param2);
			break;
		case GLINK_CMD_RX_DONE:
			qcpas_glink_recv_rx_done(sc, msg.param1, msg.param2, 0);
			break;
		case GLINK_CMD_RX_DONE_W_REUSE:
			qcpas_glink_recv_rx_done(sc, msg.param1, msg.param2, 1);
			break;
		default:
			printf("%s: unknown cmd %u\n", __func__, msg.cmd);
			return;
		}
	}
}

int
qcpas_glink_intr(void *cookie)
{
	struct qcpas_softc *sc = cookie;

	task_add(systq, &sc->sc_glink_rx);
	return 1;
}

/* GLINK PMIC Router */

struct pmic_glink_hdr {
	uint32_t owner;
#define PMIC_GLINK_OWNER_BATTMGR	32778
#define PMIC_GLINK_OWNER_USBC		32779
#define PMIC_GLINK_OWNER_USBC_PAN	32780
	uint32_t type;
#define PMIC_GLINK_TYPE_REQ_RESP	1
#define PMIC_GLINK_TYPE_NOTIFY		2
	uint32_t opcode;
};

#define BATTMGR_OPCODE_BAT_STATUS		0x1
#define BATTMGR_OPCODR_REQUEST_NOTIFICATION	0x4
#define BATTMGR_OPCODE_NOTIF			0x7
#define BATTMGR_OPCODE_BAT_INFO			0x9
#define BATTMGR_OPCODE_BAT_DISCHARGE_TIME	0xc
#define BATTMGR_OPCODE_BAT_CHARGE_TIME		0xd

#define BATTMGR_NOTIF_BAT_PROPERTY		0x30
#define BATTMGR_NOTIF_USB_PROPERTY		0x32
#define BATTMGR_NOTIF_WLS_PROPERTY		0x34
#define BATTMGR_NOTIF_BAT_STATUS		0x80
#define BATTMGR_NOTIF_BAT_INFO			0x81

#define BATTMGR_CHEMISTRY_LEN			4
#define BATTMGR_STRING_LEN			128

struct battmgr_bat_info {
	uint32_t power_unit;
	uint32_t design_capacity;
	uint32_t last_full_capacity;
	uint32_t battery_tech;
	uint32_t design_voltage;
	uint32_t capacity_low;
	uint32_t capacity_warning;
	uint32_t cycle_count;
	uint32_t accuracy;
	uint32_t max_sample_time_ms;
	uint32_t min_sample_time_ms;
	uint32_t max_average_interval_ms;
	uint32_t min_averae_interval_ms;
	uint32_t capacity_granularity1;
	uint32_t capacity_granularity2;
	uint32_t swappable;
	uint32_t capabilities;
	char model_number[BATTMGR_STRING_LEN];
	char serial_number[BATTMGR_STRING_LEN];
	char battery_type[BATTMGR_STRING_LEN];
	char oem_info[BATTMGR_STRING_LEN];
	char battery_chemistry[BATTMGR_CHEMISTRY_LEN];
	char uid[BATTMGR_STRING_LEN];
	uint32_t critical_bias;
	uint8_t day;
	uint8_t month;
	uint16_t year;
	uint32_t battery_id;
};

struct battmgr_bat_status {
	uint32_t battery_state;
#define BATTMGR_BAT_STATE_DISCHARGE	(1 << 0)
#define BATTMGR_BAT_STATE_CHARGING	(1 << 1)
#define BATTMGR_BAT_STATE_CRITICAL_LOW	(1 << 2)
	uint32_t capacity;
	uint32_t rate;
	uint32_t battery_voltage;
	uint32_t power_state;
#define BATTMGR_PWR_STATE_AC_ON			(1 << 0)
	uint32_t charging_source;
#define BATTMGR_CHARGING_SOURCE_AC		1
#define BATTMGR_CHARGING_SOURCE_USB		2
#define BATTMGR_CHARGING_SOURCE_WIRELESS	3
	uint32_t temperature;
};

void
qcpas_pmic_rtr_battmgr_req_info(void *cookie)
{
	struct {
		struct pmic_glink_hdr hdr;
		uint32_t battery_id;
	} msg;

	msg.hdr.owner = PMIC_GLINK_OWNER_BATTMGR;
	msg.hdr.type = PMIC_GLINK_TYPE_REQ_RESP;
	msg.hdr.opcode = BATTMGR_OPCODE_BAT_INFO;
	msg.battery_id = 0;
	qcpas_glink_send(cookie, &msg, sizeof(msg));
}

void
qcpas_pmic_rtr_battmgr_req_status(void *cookie)
{
	struct {
		struct pmic_glink_hdr hdr;
		uint32_t battery_id;
	} msg;

	msg.hdr.owner = PMIC_GLINK_OWNER_BATTMGR;
	msg.hdr.type = PMIC_GLINK_TYPE_REQ_RESP;
	msg.hdr.opcode = BATTMGR_OPCODE_BAT_STATUS;
	msg.battery_id = 0;
	qcpas_glink_send(cookie, &msg, sizeof(msg));
}

#if NAPM > 0
struct apm_power_info qcpas_pmic_rtr_apm_power_info;
uint32_t qcpas_pmic_rtr_last_full_capacity;
#endif

int
qcpas_pmic_rtr_init(void *cookie)
{
#if NAPM > 0
	struct apm_power_info *info;

	info = &qcpas_pmic_rtr_apm_power_info;
	info->battery_state = APM_BATT_UNKNOWN;
	info->ac_state = APM_AC_UNKNOWN;
	info->battery_life = 0;
	info->minutes_left = -1;

	apm_setinfohook(qcpas_pmic_rtr_apminfo);
#endif
	return 0;
}

int
qcpas_pmic_rtr_recv(void *cookie, uint8_t *buf, int len)
{
	struct pmic_glink_hdr hdr;
	uint32_t notification;
	extern int hw_power;

	if (len < sizeof(hdr)) {
		printf("%s: pmic glink message too small\n",
		    __func__);
		return 0;
	}

	memcpy(&hdr, buf, sizeof(hdr));

	switch (hdr.owner) {
	case PMIC_GLINK_OWNER_BATTMGR:
		switch (hdr.opcode) {
		case BATTMGR_OPCODE_NOTIF:
			if (len - sizeof(hdr) != sizeof(uint32_t)) {
				printf("%s: invalid battgmr notification\n",
				    __func__);
				return 0;
			}
			memcpy(&notification, buf + sizeof(hdr),
			    sizeof(uint32_t));
			switch (notification) {
			case BATTMGR_NOTIF_BAT_INFO:
				qcpas_pmic_rtr_battmgr_req_info(cookie);
				/* FALLTHROUGH */
			case BATTMGR_NOTIF_BAT_STATUS:
			case BATTMGR_NOTIF_BAT_PROPERTY:
				qcpas_pmic_rtr_battmgr_req_status(cookie);
				break;
			default:
				printf("%s: unknown battmgr notification"
				    " 0x%02x\n", __func__, notification);
				break;
			}
			break;
		case BATTMGR_OPCODE_BAT_INFO: {
			struct battmgr_bat_info *bat;
			if (len - sizeof(hdr) != sizeof(*bat)) {
				printf("%s: invalid battgmr bat info\n",
				    __func__);
				return 0;
			}
			bat = malloc(sizeof(*bat), M_TEMP, M_WAITOK);
			memcpy((void *)bat, buf + sizeof(hdr), sizeof(*bat));
#if NAPM > 0
			qcpas_pmic_rtr_last_full_capacity =
			    bat->last_full_capacity;
#endif
			free(bat, M_TEMP, sizeof(*bat));
			break;
		}
		case BATTMGR_OPCODE_BAT_STATUS: {
			struct battmgr_bat_status *bat;
#if NAPM > 0
			struct apm_power_info *info;
#endif
			if (len - sizeof(hdr) != sizeof(*bat)) {
				printf("%s: invalid battgmr bat status\n",
				    __func__);
				return 0;
			}
#if NAPM > 0
			/* Needs BAT_INFO fist */
			if (!qcpas_pmic_rtr_last_full_capacity)
				return 0;
#endif
			bat = malloc(sizeof(*bat), M_TEMP, M_WAITOK);
			memcpy((void *)bat, buf + sizeof(hdr), sizeof(*bat));
#if NAPM > 0
			info = &qcpas_pmic_rtr_apm_power_info;
			info->battery_life = ((bat->capacity * 100) /
			    qcpas_pmic_rtr_last_full_capacity);
			if (info->battery_life > 50)
				info->battery_state = APM_BATT_HIGH;
			else if (info->battery_life > 25)
				info->battery_state = APM_BATT_LOW;
			else
				info->battery_state = APM_BATT_CRITICAL;
			if (bat->battery_state & BATTMGR_BAT_STATE_CHARGING)
				info->battery_state = APM_BATT_CHARGING;
			else if (bat->battery_state & BATTMGR_BAT_STATE_CRITICAL_LOW)
				info->battery_state = APM_BATT_CRITICAL;

			if (bat->power_state & BATTMGR_PWR_STATE_AC_ON) {
				info->ac_state = APM_AC_ON;
				hw_power = 1;
			} else {
				info->ac_state = APM_AC_OFF;
				hw_power = 0;
			}
#endif
			free(bat, M_TEMP, sizeof(*bat));
			break;
		}
		default:
			printf("%s: unknown battmgr opcode 0x%02x\n",
			    __func__, hdr.opcode);
			break;
		}
		break;
	default:
		printf("%s: unknown pmic glink owner 0x%04x\n",
		    __func__, hdr.owner);
		break;
	}

	return 0;
}

#if NAPM > 0
int
qcpas_pmic_rtr_apminfo(struct apm_power_info *info)
{
	memcpy(info, &qcpas_pmic_rtr_apm_power_info, sizeof(*info));

	return 0;
}
#endif
