/* $OpenBSD: smmu.c,v 1.21 2022/09/11 10:28:56 patrick Exp $ */
/*
 * Copyright (c) 2008-2009,2014-2016 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2021 Patrick Wildt <patrick@blueri.se>
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
#include <sys/pool.h>
#include <sys/atomic.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>

#include <uvm/uvm_extern.h>
#include <arm64/vmparam.h>
#include <arm64/pmap.h>

#include <dev/pci/pcivar.h>
#include <arm64/dev/smmuvar.h>
#include <arm64/dev/smmureg.h>

struct smmu_map_state {
	struct extent_region	sms_er;
	bus_addr_t		sms_dva;
	bus_size_t		sms_len;
	bus_size_t		sms_loaded;
};

struct smmuvp0 {
	uint64_t l0[VP_IDX0_CNT];
	struct smmuvp1 *vp[VP_IDX0_CNT];
};

struct smmuvp1 {
	uint64_t l1[VP_IDX1_CNT];
	struct smmuvp2 *vp[VP_IDX1_CNT];
};

struct smmuvp2 {
	uint64_t l2[VP_IDX2_CNT];
	struct smmuvp3 *vp[VP_IDX2_CNT];
};

struct smmuvp3 {
	uint64_t l3[VP_IDX3_CNT];
};

CTASSERT(sizeof(struct smmuvp0) == sizeof(struct smmuvp1));
CTASSERT(sizeof(struct smmuvp0) == sizeof(struct smmuvp2));
CTASSERT(sizeof(struct smmuvp0) != sizeof(struct smmuvp3));

uint32_t smmu_gr0_read_4(struct smmu_softc *, bus_size_t);
void smmu_gr0_write_4(struct smmu_softc *, bus_size_t, uint32_t);
uint32_t smmu_gr1_read_4(struct smmu_softc *, bus_size_t);
void smmu_gr1_write_4(struct smmu_softc *, bus_size_t, uint32_t);
uint32_t smmu_cb_read_4(struct smmu_softc *, int, bus_size_t);
void smmu_cb_write_4(struct smmu_softc *, int, bus_size_t, uint32_t);
uint64_t smmu_cb_read_8(struct smmu_softc *, int, bus_size_t);
void smmu_cb_write_8(struct smmu_softc *, int, bus_size_t, uint64_t);

void smmu_tlb_sync_global(struct smmu_softc *);
void smmu_tlb_sync_context(struct smmu_domain *);

struct smmu_domain *smmu_domain_lookup(struct smmu_softc *, uint32_t);
struct smmu_domain *smmu_domain_create(struct smmu_softc *, uint32_t);

void smmu_set_l1(struct smmu_domain *, uint64_t, struct smmuvp1 *);
void smmu_set_l2(struct smmu_domain *, uint64_t, struct smmuvp1 *,
    struct smmuvp2 *);
void smmu_set_l3(struct smmu_domain *, uint64_t, struct smmuvp2 *,
    struct smmuvp3 *);

int smmu_vp_lookup(struct smmu_domain *, vaddr_t, uint64_t **);
int smmu_vp_enter(struct smmu_domain *, vaddr_t, uint64_t **, int);

uint64_t smmu_fill_pte(struct smmu_domain *, vaddr_t, paddr_t,
    vm_prot_t, int, int);
void smmu_pte_update(struct smmu_domain *, uint64_t, uint64_t *);
void smmu_pte_remove(struct smmu_domain *, vaddr_t);

int smmu_enter(struct smmu_domain *, vaddr_t, paddr_t, vm_prot_t, int, int);
void smmu_map(struct smmu_domain *, vaddr_t, paddr_t, vm_prot_t, int, int);
void smmu_unmap(struct smmu_domain *, vaddr_t);
void smmu_remove(struct smmu_domain *, vaddr_t);

int smmu_load_map(struct smmu_domain *, bus_dmamap_t);
void smmu_unload_map(struct smmu_domain *, bus_dmamap_t);

int smmu_dmamap_create(bus_dma_tag_t , bus_size_t, int,
     bus_size_t, bus_size_t, int, bus_dmamap_t *);
void smmu_dmamap_destroy(bus_dma_tag_t , bus_dmamap_t);
int smmu_dmamap_load(bus_dma_tag_t , bus_dmamap_t, void *,
     bus_size_t, struct proc *, int);
int smmu_dmamap_load_mbuf(bus_dma_tag_t , bus_dmamap_t,
     struct mbuf *, int);
int smmu_dmamap_load_uio(bus_dma_tag_t , bus_dmamap_t,
     struct uio *, int);
int smmu_dmamap_load_raw(bus_dma_tag_t , bus_dmamap_t,
     bus_dma_segment_t *, int, bus_size_t, int);
void smmu_dmamap_unload(bus_dma_tag_t , bus_dmamap_t);

struct cfdriver smmu_cd = {
	NULL, "smmu", DV_DULL
};

int
smmu_attach(struct smmu_softc *sc)
{
	uint32_t reg;
	int i;

	SIMPLEQ_INIT(&sc->sc_domains);

	pool_init(&sc->sc_vp_pool, sizeof(struct smmuvp0), PAGE_SIZE, IPL_VM, 0,
	    "smmu_vp", NULL);
	pool_setlowat(&sc->sc_vp_pool, 20);
	pool_init(&sc->sc_vp3_pool, sizeof(struct smmuvp3), PAGE_SIZE, IPL_VM, 0,
	    "smmu_vp3", NULL);
	pool_setlowat(&sc->sc_vp3_pool, 20);

	reg = smmu_gr0_read_4(sc, SMMU_IDR0);
	if (reg & SMMU_IDR0_S1TS)
		sc->sc_has_s1 = 1;
	/*
	 * Marvell's 8040 does not support 64-bit writes, hence it
	 * is not possible to invalidate stage-2, because the ASID
	 * is part of the upper 32-bits and they'd be ignored.
	 */
	if (sc->sc_is_ap806)
		sc->sc_has_s1 = 0;
	if (reg & SMMU_IDR0_S2TS)
		sc->sc_has_s2 = 1;
	if (!sc->sc_has_s1 && !sc->sc_has_s2)
		return 1;
	if (reg & SMMU_IDR0_EXIDS)
		sc->sc_has_exids = 1;

	sc->sc_num_streams = 1 << SMMU_IDR0_NUMSIDB(reg);
	if (sc->sc_has_exids)
		sc->sc_num_streams = 1 << 16;
	sc->sc_stream_mask = sc->sc_num_streams - 1;
	if (reg & SMMU_IDR0_SMS) {
		sc->sc_num_streams = SMMU_IDR0_NUMSMRG(reg);
		if (sc->sc_num_streams == 0)
			return 1;
		sc->sc_smr = mallocarray(sc->sc_num_streams,
		    sizeof(*sc->sc_smr), M_DEVBUF, M_WAITOK | M_ZERO);
	}

	reg = smmu_gr0_read_4(sc, SMMU_IDR1);
	sc->sc_pagesize = 4 * 1024;
	if (reg & SMMU_IDR1_PAGESIZE_64K)
		sc->sc_pagesize = 64 * 1024;
	sc->sc_numpage = 1 << (SMMU_IDR1_NUMPAGENDXB(reg) + 1);

	/* 0 to NUMS2CB == stage-2, NUMS2CB to NUMCB == stage-1 */
	sc->sc_num_context_banks = SMMU_IDR1_NUMCB(reg);
	sc->sc_num_s2_context_banks = SMMU_IDR1_NUMS2CB(reg);
	if (sc->sc_num_s2_context_banks > sc->sc_num_context_banks)
		return 1;
	sc->sc_cb = mallocarray(sc->sc_num_context_banks,
	    sizeof(*sc->sc_cb), M_DEVBUF, M_WAITOK | M_ZERO);

	reg = smmu_gr0_read_4(sc, SMMU_IDR2);
	if (reg & SMMU_IDR2_VMID16S)
		sc->sc_has_vmid16s = 1;

	switch (SMMU_IDR2_IAS(reg)) {
	case SMMU_IDR2_IAS_32BIT:
		sc->sc_ipa_bits = 32;
		break;
	case SMMU_IDR2_IAS_36BIT:
		sc->sc_ipa_bits = 36;
		break;
	case SMMU_IDR2_IAS_40BIT:
		sc->sc_ipa_bits = 40;
		break;
	case SMMU_IDR2_IAS_42BIT:
		sc->sc_ipa_bits = 42;
		break;
	case SMMU_IDR2_IAS_44BIT:
		sc->sc_ipa_bits = 44;
		break;
	case SMMU_IDR2_IAS_48BIT:
	default:
		sc->sc_ipa_bits = 48;
		break;
	}
	switch (SMMU_IDR2_OAS(reg)) {
	case SMMU_IDR2_OAS_32BIT:
		sc->sc_pa_bits = 32;
		break;
	case SMMU_IDR2_OAS_36BIT:
		sc->sc_pa_bits = 36;
		break;
	case SMMU_IDR2_OAS_40BIT:
		sc->sc_pa_bits = 40;
		break;
	case SMMU_IDR2_OAS_42BIT:
		sc->sc_pa_bits = 42;
		break;
	case SMMU_IDR2_OAS_44BIT:
		sc->sc_pa_bits = 44;
		break;
	case SMMU_IDR2_OAS_48BIT:
	default:
		sc->sc_pa_bits = 48;
		break;
	}
	switch (SMMU_IDR2_UBS(reg)) {
	case SMMU_IDR2_UBS_32BIT:
		sc->sc_va_bits = 32;
		break;
	case SMMU_IDR2_UBS_36BIT:
		sc->sc_va_bits = 36;
		break;
	case SMMU_IDR2_UBS_40BIT:
		sc->sc_va_bits = 40;
		break;
	case SMMU_IDR2_UBS_42BIT:
		sc->sc_va_bits = 42;
		break;
	case SMMU_IDR2_UBS_44BIT:
		sc->sc_va_bits = 44;
		break;
	case SMMU_IDR2_UBS_49BIT:
	default:
		sc->sc_va_bits = 48;
		break;
	}

	printf(": %u CBs (%u S2-only)",
	    sc->sc_num_context_banks, sc->sc_num_s2_context_banks);
	if (sc->sc_is_qcom) {
		/*
		 * In theory we should check if bypass quirk is needed by
		 * modifying S2CR and re-checking if the value is different.
		 * This does not work on the last S2CR, but on the first,
		 * which is in use.  Revisit this once we have other QCOM HW.
		 */
		sc->sc_bypass_quirk = 1;
		printf(", bypass quirk");
		/*
		 * Create special context that is turned off.  This allows us
		 * to map a stream to a context bank where translation is not
		 * happening, and hence bypassed.
		 */
		sc->sc_cb[sc->sc_num_context_banks - 1] =
		    malloc(sizeof(struct smmu_cb), M_DEVBUF, M_WAITOK | M_ZERO);
		smmu_cb_write_4(sc, sc->sc_num_context_banks - 1,
		    SMMU_CB_SCTLR, 0);
		smmu_gr1_write_4(sc, SMMU_CBAR(sc->sc_num_context_banks - 1),
		    SMMU_CBAR_TYPE_S1_TRANS_S2_BYPASS);
	}
	printf("\n");

	/* Clear Global Fault Status Register */
	smmu_gr0_write_4(sc, SMMU_SGFSR, smmu_gr0_read_4(sc, SMMU_SGFSR));

	for (i = 0; i < sc->sc_num_streams; i++) {
		/* On QCOM HW we need to keep current streams running. */
		if (sc->sc_is_qcom && sc->sc_smr &&
		    smmu_gr0_read_4(sc, SMMU_SMR(i)) & SMMU_SMR_VALID) {
			reg = smmu_gr0_read_4(sc, SMMU_SMR(i));
			sc->sc_smr[i] = malloc(sizeof(struct smmu_smr),
			    M_DEVBUF, M_WAITOK | M_ZERO);
			sc->sc_smr[i]->ss_id = (reg >> SMMU_SMR_ID_SHIFT) &
			    SMMU_SMR_ID_MASK;
			sc->sc_smr[i]->ss_mask = (reg >> SMMU_SMR_MASK_SHIFT) &
			    SMMU_SMR_MASK_MASK;
			if (sc->sc_bypass_quirk) {
				smmu_gr0_write_4(sc, SMMU_S2CR(i),
				    SMMU_S2CR_TYPE_TRANS |
				    sc->sc_num_context_banks - 1);
			} else {
				smmu_gr0_write_4(sc, SMMU_S2CR(i),
				    SMMU_S2CR_TYPE_BYPASS | 0xff);
			}
			continue;
		}
#if 1
		/* Setup all streams to fault by default */
		smmu_gr0_write_4(sc, SMMU_S2CR(i), SMMU_S2CR_TYPE_FAULT);
#else
		/* For stream indexing, USFCFG bypass isn't enough! */
		smmu_gr0_write_4(sc, SMMU_S2CR(i), SMMU_S2CR_TYPE_BYPASS);
#endif
		/*  Disable all stream map registers */
		if (sc->sc_smr)
			smmu_gr0_write_4(sc, SMMU_SMR(i), 0);
	}

	for (i = 0; i < sc->sc_num_context_banks; i++) {
		/* Disable Context Bank */
		smmu_cb_write_4(sc, i, SMMU_CB_SCTLR, 0);
		/* Clear Context Bank Fault Status Register */
		smmu_cb_write_4(sc, i, SMMU_CB_FSR, SMMU_CB_FSR_MASK);
	}

	/* Invalidate TLB */
	smmu_gr0_write_4(sc, SMMU_TLBIALLH, ~0);
	smmu_gr0_write_4(sc, SMMU_TLBIALLNSNH, ~0);

	if (sc->sc_is_mmu500) {
		reg = smmu_gr0_read_4(sc, SMMU_SACR);
		if (SMMU_IDR7_MAJOR(smmu_gr0_read_4(sc, SMMU_IDR7)) >= 2)
			reg &= ~SMMU_SACR_MMU500_CACHE_LOCK;
		reg |= SMMU_SACR_MMU500_SMTNMB_TLBEN |
		    SMMU_SACR_MMU500_S2CRB_TLBEN;
		smmu_gr0_write_4(sc, SMMU_SACR, reg);
		for (i = 0; i < sc->sc_num_context_banks; i++) {
			reg = smmu_cb_read_4(sc, i, SMMU_CB_ACTLR);
			reg &= ~SMMU_CB_ACTLR_CPRE;
			smmu_cb_write_4(sc, i, SMMU_CB_ACTLR, reg);
		}
	}

	/* Enable SMMU */
	reg = smmu_gr0_read_4(sc, SMMU_SCR0);
	reg &= ~(SMMU_SCR0_CLIENTPD |
	    SMMU_SCR0_FB | SMMU_SCR0_BSU_MASK);
#if 1
	/* Disable bypass for unknown streams */
	reg |= SMMU_SCR0_USFCFG;
#else
	/* Enable bypass for unknown streams */
	reg &= ~SMMU_SCR0_USFCFG;
#endif
	reg |= SMMU_SCR0_GFRE | SMMU_SCR0_GFIE |
	    SMMU_SCR0_GCFGFRE | SMMU_SCR0_GCFGFIE |
	    SMMU_SCR0_VMIDPNE | SMMU_SCR0_PTM;
	if (sc->sc_has_exids)
		reg |= SMMU_SCR0_EXIDENABLE;
	if (sc->sc_has_vmid16s)
		reg |= SMMU_SCR0_VMID16EN;

	smmu_tlb_sync_global(sc);
	smmu_gr0_write_4(sc, SMMU_SCR0, reg);

	return 0;
}

int
smmu_global_irq(void *cookie)
{
	struct smmu_softc *sc = cookie;
	uint32_t reg;

	reg = smmu_gr0_read_4(sc, SMMU_SGFSR);
	if (reg == 0)
		return 0;

	printf("%s: SGFSR 0x%08x SGFSYNR0 0x%08x SGFSYNR1 0x%08x "
	    "SGFSYNR2 0x%08x\n", sc->sc_dev.dv_xname, reg,
	    smmu_gr0_read_4(sc, SMMU_SGFSYNR0),
	    smmu_gr0_read_4(sc, SMMU_SGFSYNR1),
	    smmu_gr0_read_4(sc, SMMU_SGFSYNR2));

	smmu_gr0_write_4(sc, SMMU_SGFSR, reg);

	return 1;
}

int
smmu_context_irq(void *cookie)
{
	struct smmu_cb_irq *cbi = cookie;
	struct smmu_softc *sc = cbi->cbi_sc;
	uint32_t reg;

	reg = smmu_cb_read_4(sc, cbi->cbi_idx, SMMU_CB_FSR);
	if ((reg & SMMU_CB_FSR_MASK) == 0)
		return 0;

	printf("%s: FSR 0x%08x FSYNR0 0x%08x FAR 0x%llx "
	    "CBFRSYNRA 0x%08x\n", sc->sc_dev.dv_xname, reg,
	    smmu_cb_read_4(sc, cbi->cbi_idx, SMMU_CB_FSYNR0),
	    smmu_cb_read_8(sc, cbi->cbi_idx, SMMU_CB_FAR),
	    smmu_gr1_read_4(sc, SMMU_CBFRSYNRA(cbi->cbi_idx)));

	smmu_cb_write_4(sc, cbi->cbi_idx, SMMU_CB_FSR, reg);

	return 1;
}

void
smmu_tlb_sync_global(struct smmu_softc *sc)
{
	int i;

	smmu_gr0_write_4(sc, SMMU_STLBGSYNC, ~0);
	for (i = 1000; i > 0; i--) {
		if ((smmu_gr0_read_4(sc, SMMU_STLBGSTATUS) &
		    SMMU_STLBGSTATUS_GSACTIVE) == 0)
			return;
	}

	printf("%s: global TLB sync timeout\n",
	    sc->sc_dev.dv_xname);
}

void
smmu_tlb_sync_context(struct smmu_domain *dom)
{
	struct smmu_softc *sc = dom->sd_sc;
	int i;

	smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_TLBSYNC, ~0);
	for (i = 1000; i > 0; i--) {
		if ((smmu_cb_read_4(sc, dom->sd_cb_idx, SMMU_CB_TLBSTATUS) &
		    SMMU_CB_TLBSTATUS_SACTIVE) == 0)
			return;
	}

	printf("%s: context TLB sync timeout\n",
	    sc->sc_dev.dv_xname);
}

uint32_t
smmu_gr0_read_4(struct smmu_softc *sc, bus_size_t off)
{
	uint32_t base = 0 * sc->sc_pagesize;

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_gr0_write_4(struct smmu_softc *sc, bus_size_t off, uint32_t val)
{
	uint32_t base = 0 * sc->sc_pagesize;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off, val);
}

uint32_t
smmu_gr1_read_4(struct smmu_softc *sc, bus_size_t off)
{
	uint32_t base = 1 * sc->sc_pagesize;

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_gr1_write_4(struct smmu_softc *sc, bus_size_t off, uint32_t val)
{
	uint32_t base = 1 * sc->sc_pagesize;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off, val);
}

uint32_t
smmu_cb_read_4(struct smmu_softc *sc, int idx, bus_size_t off)
{
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_cb_write_4(struct smmu_softc *sc, int idx, bus_size_t off, uint32_t val)
{
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off, val);
}

uint64_t
smmu_cb_read_8(struct smmu_softc *sc, int idx, bus_size_t off)
{
	uint64_t reg;
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	if (sc->sc_is_ap806) {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off + 4);
		reg <<= 32;
		reg |= bus_space_read_4(sc->sc_iot, sc->sc_ioh, base + off + 0);
		return reg;
	}

	return bus_space_read_8(sc->sc_iot, sc->sc_ioh, base + off);
}

void
smmu_cb_write_8(struct smmu_softc *sc, int idx, bus_size_t off, uint64_t val)
{
	uint32_t base;

	base = sc->sc_numpage * sc->sc_pagesize; /* SMMU_CB_BASE */
	base += idx * sc->sc_pagesize; /* SMMU_CBn_BASE */

	if (sc->sc_is_ap806) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off + 4,
		    val >> 32);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, base + off + 0,
		    val & 0xffffffff);
		return;
	}

	bus_space_write_8(sc->sc_iot, sc->sc_ioh, base + off, val);
}

bus_dma_tag_t
smmu_device_map(void *cookie, uint32_t sid, bus_dma_tag_t dmat)
{
	struct smmu_softc *sc = cookie;
	struct smmu_domain *dom;

	dom = smmu_domain_lookup(sc, sid);
	if (dom == NULL)
		return dmat;

	if (dom->sd_dmat == NULL) {
		dom->sd_dmat = malloc(sizeof(*dom->sd_dmat),
		    M_DEVBUF, M_WAITOK);
		memcpy(dom->sd_dmat, sc->sc_dmat,
		    sizeof(*dom->sd_dmat));
		dom->sd_dmat->_cookie = dom;
		dom->sd_dmat->_dmamap_create = smmu_dmamap_create;
		dom->sd_dmat->_dmamap_destroy = smmu_dmamap_destroy;
		dom->sd_dmat->_dmamap_load = smmu_dmamap_load;
		dom->sd_dmat->_dmamap_load_mbuf = smmu_dmamap_load_mbuf;
		dom->sd_dmat->_dmamap_load_uio = smmu_dmamap_load_uio;
		dom->sd_dmat->_dmamap_load_raw = smmu_dmamap_load_raw;
		dom->sd_dmat->_dmamap_unload = smmu_dmamap_unload;
		dom->sd_dmat->_flags |= BUS_DMA_COHERENT;
	}

	return dom->sd_dmat;
}

struct smmu_domain *
smmu_domain_lookup(struct smmu_softc *sc, uint32_t sid)
{
	struct smmu_domain *dom;

	SIMPLEQ_FOREACH(dom, &sc->sc_domains, sd_list) {
		if (dom->sd_sid == sid)
			return dom;
	}

	return smmu_domain_create(sc, sid);
}

struct smmu_domain *
smmu_domain_create(struct smmu_softc *sc, uint32_t sid)
{
	struct smmu_domain *dom;
	uint32_t iovabits, reg;
	paddr_t pa;
	vaddr_t l0va;
	int i, start, end;

	dom = malloc(sizeof(*dom), M_DEVBUF, M_WAITOK | M_ZERO);
	mtx_init(&dom->sd_iova_mtx, IPL_VM);
	mtx_init(&dom->sd_pmap_mtx, IPL_VM);
	dom->sd_sc = sc;
	dom->sd_sid = sid;

	/* Prefer stage 1 if possible! */
	if (sc->sc_has_s1) {
		start = sc->sc_num_s2_context_banks;
		end = sc->sc_num_context_banks;
		dom->sd_stage = 1;
	} else {
		start = 0;
		end = sc->sc_num_context_banks;
		dom->sd_stage = 2;
	}

	for (i = start; i < end; i++) {
		if (sc->sc_cb[i] != NULL)
			continue;
		sc->sc_cb[i] = malloc(sizeof(struct smmu_cb),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		dom->sd_cb_idx = i;
		break;
	}
	if (i >= end) {
		printf("%s: out of context blocks, I/O device will fail\n",
		    sc->sc_dev.dv_xname);
		free(dom, M_DEVBUF, sizeof(*dom));
		return NULL;
	}

	/* Stream indexing is easy */
	dom->sd_smr_idx = sid;

	/* Stream mapping is a bit more effort */
	if (sc->sc_smr) {
		for (i = 0; i < sc->sc_num_streams; i++) {
			/* Take over QCOM SMRs */
			if (sc->sc_is_qcom && sc->sc_smr[i] != NULL &&
			    sc->sc_smr[i]->ss_dom == NULL &&
			    sc->sc_smr[i]->ss_id == sid &&
			    sc->sc_smr[i]->ss_mask == 0) {
				free(sc->sc_smr[i], M_DEVBUF,
				    sizeof(struct smmu_smr));
				sc->sc_smr[i] = NULL;
			}
			if (sc->sc_smr[i] != NULL)
				continue;
			sc->sc_smr[i] = malloc(sizeof(struct smmu_smr),
			    M_DEVBUF, M_WAITOK | M_ZERO);
			sc->sc_smr[i]->ss_dom = dom;
			sc->sc_smr[i]->ss_id = sid;
			sc->sc_smr[i]->ss_mask = 0;
			dom->sd_smr_idx = i;
			break;
		}

		if (i >= sc->sc_num_streams) {
			free(sc->sc_cb[dom->sd_cb_idx], M_DEVBUF,
			    sizeof(struct smmu_cb));
			sc->sc_cb[dom->sd_cb_idx] = NULL;
			free(dom, M_DEVBUF, sizeof(*dom));
			printf("%s: out of streams, I/O device will fail\n",
			    sc->sc_dev.dv_xname);
			return NULL;
		}
	}

	reg = SMMU_CBA2R_VA64;
	if (sc->sc_has_vmid16s)
		reg |= (dom->sd_cb_idx + 1) << SMMU_CBA2R_VMID16_SHIFT;
	smmu_gr1_write_4(sc, SMMU_CBA2R(dom->sd_cb_idx), reg);

	if (dom->sd_stage == 1) {
		reg = SMMU_CBAR_TYPE_S1_TRANS_S2_BYPASS |
		    SMMU_CBAR_BPSHCFG_NSH | SMMU_CBAR_MEMATTR_WB;
	} else {
		reg = SMMU_CBAR_TYPE_S2_TRANS;
		if (!sc->sc_has_vmid16s)
			reg |= (dom->sd_cb_idx + 1) << SMMU_CBAR_VMID_SHIFT;
	}
	smmu_gr1_write_4(sc, SMMU_CBAR(dom->sd_cb_idx), reg);

	if (dom->sd_stage == 1) {
		reg = SMMU_CB_TCR2_AS | SMMU_CB_TCR2_SEP_UPSTREAM;
		switch (sc->sc_ipa_bits) {
		case 32:
			reg |= SMMU_CB_TCR2_PASIZE_32BIT;
			break;
		case 36:
			reg |= SMMU_CB_TCR2_PASIZE_36BIT;
			break;
		case 40:
			reg |= SMMU_CB_TCR2_PASIZE_40BIT;
			break;
		case 42:
			reg |= SMMU_CB_TCR2_PASIZE_42BIT;
			break;
		case 44:
			reg |= SMMU_CB_TCR2_PASIZE_44BIT;
			break;
		case 48:
			reg |= SMMU_CB_TCR2_PASIZE_48BIT;
			break;
		}
		smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_TCR2, reg);
	}

	if (dom->sd_stage == 1)
		iovabits = sc->sc_va_bits;
	else
		iovabits = sc->sc_ipa_bits;
	/*
	 * Marvell's 8040 does not support 64-bit writes, hence we
	 * can only address 44-bits of VA space for TLB invalidation.
	 */
	if (sc->sc_is_ap806)
		iovabits = min(44, iovabits);
	if (iovabits >= 40)
		dom->sd_4level = 1;

	reg = SMMU_CB_TCR_TG0_4KB | SMMU_CB_TCR_T0SZ(64 - iovabits);
	if (dom->sd_stage == 1) {
		reg |= SMMU_CB_TCR_EPD1;
	} else {
		if (dom->sd_4level)
			reg |= SMMU_CB_TCR_S2_SL0_4KB_L0;
		else
			reg |= SMMU_CB_TCR_S2_SL0_4KB_L1;
		switch (sc->sc_pa_bits) {
		case 32:
			reg |= SMMU_CB_TCR_S2_PASIZE_32BIT;
			break;
		case 36:
			reg |= SMMU_CB_TCR_S2_PASIZE_36BIT;
			break;
		case 40:
			reg |= SMMU_CB_TCR_S2_PASIZE_40BIT;
			break;
		case 42:
			reg |= SMMU_CB_TCR_S2_PASIZE_42BIT;
			break;
		case 44:
			reg |= SMMU_CB_TCR_S2_PASIZE_44BIT;
			break;
		case 48:
			reg |= SMMU_CB_TCR_S2_PASIZE_48BIT;
			break;
		}
	}
	if (sc->sc_coherent)
		reg |= SMMU_CB_TCR_IRGN0_WBWA | SMMU_CB_TCR_ORGN0_WBWA |
		    SMMU_CB_TCR_SH0_ISH;
	else
		reg |= SMMU_CB_TCR_IRGN0_NC | SMMU_CB_TCR_ORGN0_NC |
		    SMMU_CB_TCR_SH0_OSH;
	smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_TCR, reg);

	if (dom->sd_4level) {
		while (dom->sd_vp.l0 == NULL) {
			dom->sd_vp.l0 = pool_get(&sc->sc_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t)dom->sd_vp.l0->l0; /* top level is l0 */
	} else {
		while (dom->sd_vp.l1 == NULL) {
			dom->sd_vp.l1 = pool_get(&sc->sc_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t)dom->sd_vp.l1->l1; /* top level is l1 */
	}
	pmap_extract(pmap_kernel(), l0va, &pa);

	if (dom->sd_stage == 1) {
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TTBR0,
		    (uint64_t)dom->sd_cb_idx << SMMU_CB_TTBR_ASID_SHIFT | pa);
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TTBR1,
		    (uint64_t)dom->sd_cb_idx << SMMU_CB_TTBR_ASID_SHIFT);
	} else
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TTBR0, pa);

	if (dom->sd_stage == 1) {
		smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_MAIR0,
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_nGnRnE, 0) |
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_nGnRE, 1) |
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_NC, 2) |
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_WB, 3));
		smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_MAIR1,
		    SMMU_CB_MAIR_MAIR_ATTR(SMMU_CB_MAIR_DEVICE_WT, 0));
	}

	reg = SMMU_CB_SCTLR_M | SMMU_CB_SCTLR_TRE | SMMU_CB_SCTLR_AFE |
	    SMMU_CB_SCTLR_CFRE | SMMU_CB_SCTLR_CFIE;
	if (dom->sd_stage == 1)
		reg |= SMMU_CB_SCTLR_ASIDPNE;
	smmu_cb_write_4(sc, dom->sd_cb_idx, SMMU_CB_SCTLR, reg);

	/* Point stream to context block */
	reg = SMMU_S2CR_TYPE_TRANS | dom->sd_cb_idx;
	if (sc->sc_has_exids && sc->sc_smr)
		reg |= SMMU_S2CR_EXIDVALID;
	smmu_gr0_write_4(sc, SMMU_S2CR(dom->sd_smr_idx), reg);

	/* Map stream idx to S2CR idx */
	if (sc->sc_smr) {
		reg = sid;
		if (!sc->sc_has_exids)
			reg |= SMMU_SMR_VALID;
		smmu_gr0_write_4(sc, SMMU_SMR(dom->sd_smr_idx), reg);
	}

	snprintf(dom->sd_exname, sizeof(dom->sd_exname), "%s:%x",
	    sc->sc_dev.dv_xname, sid);
	dom->sd_iovamap = extent_create(dom->sd_exname, 0,
	    (1LL << iovabits) - 1, M_DEVBUF, NULL, 0, EX_WAITOK |
	    EX_NOCOALESCE);

	/* Reserve first page (to catch NULL access) */
	extent_alloc_region(dom->sd_iovamap, 0, PAGE_SIZE, EX_WAITOK);

	SIMPLEQ_INSERT_TAIL(&sc->sc_domains, dom, sd_list);
	return dom;
}

void
smmu_reserve_region(void *cookie, uint32_t sid, bus_addr_t addr,
    bus_size_t size)
{
	struct smmu_softc *sc = cookie;
	struct smmu_domain *dom;

	dom = smmu_domain_lookup(sc, sid);
	if (dom == NULL)
		return;

	extent_alloc_region(dom->sd_iovamap, addr, size,
	    EX_WAITOK | EX_CONFLICTOK);
}

/* basically pmap follows */

/* virtual to physical helpers */
static inline int
VP_IDX0(vaddr_t va)
{
	return (va >> VP_IDX0_POS) & VP_IDX0_MASK;
}

static inline int
VP_IDX1(vaddr_t va)
{
	return (va >> VP_IDX1_POS) & VP_IDX1_MASK;
}

static inline int
VP_IDX2(vaddr_t va)
{
	return (va >> VP_IDX2_POS) & VP_IDX2_MASK;
}

static inline int
VP_IDX3(vaddr_t va)
{
	return (va >> VP_IDX3_POS) & VP_IDX3_MASK;
}

static inline uint64_t
VP_Lx(paddr_t pa)
{
	/*
	 * This function takes the pa address given and manipulates it
	 * into the form that should be inserted into the VM table.
	 */
	return pa | Lx_TYPE_PT;
}

void
smmu_set_l1(struct smmu_domain *dom, uint64_t va, struct smmuvp1 *l1_va)
{
	struct smmu_softc *sc = dom->sd_sc;
	uint64_t pg_entry;
	paddr_t l1_pa;
	int idx0;

	if (pmap_extract(pmap_kernel(), (vaddr_t)l1_va, &l1_pa) == 0)
		panic("%s: unable to find vp pa mapping %p", __func__, l1_va);

	if (l1_pa & (Lx_TABLE_ALIGN-1))
		panic("%s: misaligned L2 table", __func__);

	pg_entry = VP_Lx(l1_pa);

	idx0 = VP_IDX0(va);
	dom->sd_vp.l0->vp[idx0] = l1_va;
	dom->sd_vp.l0->l0[idx0] = pg_entry;
	membar_producer(); /* XXX bus dma sync? */
	if (!sc->sc_coherent)
		cpu_dcache_wb_range((vaddr_t)&dom->sd_vp.l0->l0[idx0],
		    sizeof(dom->sd_vp.l0->l0[idx0]));
}

void
smmu_set_l2(struct smmu_domain *dom, uint64_t va, struct smmuvp1 *vp1,
    struct smmuvp2 *l2_va)
{
	struct smmu_softc *sc = dom->sd_sc;
	uint64_t pg_entry;
	paddr_t l2_pa;
	int idx1;

	if (pmap_extract(pmap_kernel(), (vaddr_t)l2_va, &l2_pa) == 0)
		panic("%s: unable to find vp pa mapping %p", __func__, l2_va);

	if (l2_pa & (Lx_TABLE_ALIGN-1))
		panic("%s: misaligned L2 table", __func__);

	pg_entry = VP_Lx(l2_pa);

	idx1 = VP_IDX1(va);
	vp1->vp[idx1] = l2_va;
	vp1->l1[idx1] = pg_entry;
	membar_producer(); /* XXX bus dma sync? */
	if (!sc->sc_coherent)
		cpu_dcache_wb_range((vaddr_t)&vp1->l1[idx1],
		    sizeof(vp1->l1[idx1]));
}

void
smmu_set_l3(struct smmu_domain *dom, uint64_t va, struct smmuvp2 *vp2,
    struct smmuvp3 *l3_va)
{
	struct smmu_softc *sc = dom->sd_sc;
	uint64_t pg_entry;
	paddr_t l3_pa;
	int idx2;

	if (pmap_extract(pmap_kernel(), (vaddr_t)l3_va, &l3_pa) == 0)
		panic("%s: unable to find vp pa mapping %p", __func__, l3_va);

	if (l3_pa & (Lx_TABLE_ALIGN-1))
		panic("%s: misaligned L2 table", __func__);

	pg_entry = VP_Lx(l3_pa);

	idx2 = VP_IDX2(va);
	vp2->vp[idx2] = l3_va;
	vp2->l2[idx2] = pg_entry;
	membar_producer(); /* XXX bus dma sync? */
	if (!sc->sc_coherent)
		cpu_dcache_wb_range((vaddr_t)&vp2->l2[idx2],
		    sizeof(vp2->l2[idx2]));
}

int
smmu_vp_lookup(struct smmu_domain *dom, vaddr_t va, uint64_t **pl3entry)
{
	struct smmuvp1 *vp1;
	struct smmuvp2 *vp2;
	struct smmuvp3 *vp3;

	if (dom->sd_4level) {
		if (dom->sd_vp.l0 == NULL) {
			return ENXIO;
		}
		vp1 = dom->sd_vp.l0->vp[VP_IDX0(va)];
	} else {
		vp1 = dom->sd_vp.l1;
	}
	if (vp1 == NULL) {
		return ENXIO;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		return ENXIO;
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		return ENXIO;
	}

	if (pl3entry != NULL)
		*pl3entry = &(vp3->l3[VP_IDX3(va)]);

	return 0;
}

int
smmu_vp_enter(struct smmu_domain *dom, vaddr_t va, uint64_t **pl3entry,
    int flags)
{
	struct smmu_softc *sc = dom->sd_sc;
	struct smmuvp1 *vp1;
	struct smmuvp2 *vp2;
	struct smmuvp3 *vp3;

	if (dom->sd_4level) {
		vp1 = dom->sd_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL) {
			mtx_enter(&dom->sd_pmap_mtx);
			vp1 = dom->sd_vp.l0->vp[VP_IDX0(va)];
			if (vp1 == NULL) {
				vp1 = pool_get(&sc->sc_vp_pool,
				    PR_NOWAIT | PR_ZERO);
				if (vp1 == NULL) {
					mtx_leave(&dom->sd_pmap_mtx);
					return ENOMEM;
				}
				smmu_set_l1(dom, va, vp1);
			}
			mtx_leave(&dom->sd_pmap_mtx);
		}
	} else {
		vp1 = dom->sd_vp.l1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		mtx_enter(&dom->sd_pmap_mtx);
		vp2 = vp1->vp[VP_IDX1(va)];
		if (vp2 == NULL) {
			vp2 = pool_get(&sc->sc_vp_pool, PR_NOWAIT | PR_ZERO);
			if (vp2 == NULL) {
				mtx_leave(&dom->sd_pmap_mtx);
				return ENOMEM;
			}
			smmu_set_l2(dom, va, vp1, vp2);
		}
		mtx_leave(&dom->sd_pmap_mtx);
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		mtx_enter(&dom->sd_pmap_mtx);
		vp3 = vp2->vp[VP_IDX2(va)];
		if (vp3 == NULL) {
			vp3 = pool_get(&sc->sc_vp3_pool, PR_NOWAIT | PR_ZERO);
			if (vp3 == NULL) {
				mtx_leave(&dom->sd_pmap_mtx);
				return ENOMEM;
			}
			smmu_set_l3(dom, va, vp2, vp3);
		}
		mtx_leave(&dom->sd_pmap_mtx);
	}

	if (pl3entry != NULL)
		*pl3entry = &(vp3->l3[VP_IDX3(va)]);

	return 0;
}

uint64_t
smmu_fill_pte(struct smmu_domain *dom, vaddr_t va, paddr_t pa,
    vm_prot_t prot, int flags, int cache)
{
	uint64_t pted;

	pted = pa & PTE_RPGN;

	switch (cache) {
	case PMAP_CACHE_WB:
		break;
	case PMAP_CACHE_WT:
		break;
	case PMAP_CACHE_CI:
		break;
	case PMAP_CACHE_DEV_NGNRNE:
		break;
	case PMAP_CACHE_DEV_NGNRE:
		break;
	default:
		panic("%s: invalid cache mode", __func__);
	}

	pted |= cache;
	pted |= flags & (PROT_READ|PROT_WRITE|PROT_EXEC);
	return pted;
}

void
smmu_pte_update(struct smmu_domain *dom, uint64_t pted, uint64_t *pl3)
{
	struct smmu_softc *sc = dom->sd_sc;
	uint64_t pte, access_bits;
	uint64_t attr = 0;

	/* see mair in locore.S */
	switch (pted & PMAP_CACHE_BITS) {
	case PMAP_CACHE_WB:
		/* inner and outer writeback */
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_WB);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_WB);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_WT:
		/* inner and outer writethrough */
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_WT);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_WT);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_CI:
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_CI);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_CI);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_DEV_NGNRNE:
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_DEV_NGNRNE);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_DEV_NGNRNE);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_DEV_NGNRE:
		if (dom->sd_stage == 1)
			attr |= ATTR_IDX(PTE_ATTR_DEV_NGNRE);
		else
			attr |= ATTR_IDX(PTE_MEMATTR_DEV_NGNRE);
		attr |= ATTR_SH(SH_INNER);
		break;
	default:
		panic("%s: invalid cache mode", __func__);
	}

	access_bits = ATTR_PXN | ATTR_AF;
	if (dom->sd_stage == 1) {
		attr |= ATTR_nG;
		access_bits |= ATTR_AP(1);
		if ((pted & PROT_READ) &&
		    !(pted & PROT_WRITE))
			access_bits |= ATTR_AP(2);
	} else {
		if (pted & PROT_READ)
			access_bits |= ATTR_AP(1);
		if (pted & PROT_WRITE)
			access_bits |= ATTR_AP(2);
	}

	pte = (pted & PTE_RPGN) | attr | access_bits | L3_P;
	*pl3 = pte;
	membar_producer(); /* XXX bus dma sync? */
	if (!sc->sc_coherent)
		cpu_dcache_wb_range((vaddr_t)pl3, sizeof(*pl3));
}

void
smmu_pte_remove(struct smmu_domain *dom, vaddr_t va)
{
	/* put entry into table */
	/* need to deal with ref/change here */
	struct smmu_softc *sc = dom->sd_sc;
	struct smmuvp1 *vp1;
	struct smmuvp2 *vp2;
	struct smmuvp3 *vp3;

	if (dom->sd_4level)
		vp1 = dom->sd_vp.l0->vp[VP_IDX0(va)];
	else
		vp1 = dom->sd_vp.l1;
	if (vp1 == NULL) {
		panic("%s: missing the l1 for va %lx domain %p", __func__,
		    va, dom);
	}
	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		panic("%s: missing the l2 for va %lx domain %p", __func__,
		    va, dom);
	}
	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		panic("%s: missing the l3 for va %lx domain %p", __func__,
		    va, dom);
	}
	vp3->l3[VP_IDX3(va)] = 0;
	membar_producer(); /* XXX bus dma sync? */
	if (!sc->sc_coherent)
		cpu_dcache_wb_range((vaddr_t)&vp3->l3[VP_IDX3(va)],
		    sizeof(vp3->l3[VP_IDX3(va)]));
}

int
smmu_enter(struct smmu_domain *dom, vaddr_t va, paddr_t pa, vm_prot_t prot,
    int flags, int cache)
{
	uint64_t *pl3;

	if (smmu_vp_lookup(dom, va, &pl3) != 0) {
		if (smmu_vp_enter(dom, va, &pl3, flags))
			return ENOMEM;
	}

	if (flags & (PROT_READ|PROT_WRITE|PROT_EXEC))
		smmu_map(dom, va, pa, prot, flags, cache);

	return 0;
}

void
smmu_map(struct smmu_domain *dom, vaddr_t va, paddr_t pa, vm_prot_t prot,
    int flags, int cache)
{
	uint64_t *pl3;
	uint64_t pted;
	int ret;

	/* IOVA must already be allocated */
	ret = smmu_vp_lookup(dom, va, &pl3);
	KASSERT(ret == 0);

	/* Update PTED information for physical address */
	pted = smmu_fill_pte(dom, va, pa, prot, flags, cache);

	/* Insert updated information */
	smmu_pte_update(dom, pted, pl3);
}

void
smmu_unmap(struct smmu_domain *dom, vaddr_t va)
{
	struct smmu_softc *sc = dom->sd_sc;
	int ret;

	/* IOVA must already be allocated */
	ret = smmu_vp_lookup(dom, va, NULL);
	KASSERT(ret == 0);

	/* Remove mapping from pagetable */
	smmu_pte_remove(dom, va);

	/* Invalidate IOTLB */
	if (dom->sd_stage == 1)
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TLBIVAL,
		    (uint64_t)dom->sd_cb_idx << 48 | va >> PAGE_SHIFT);
	else
		smmu_cb_write_8(sc, dom->sd_cb_idx, SMMU_CB_TLBIIPAS2L,
		    va >> PAGE_SHIFT);
}

void
smmu_remove(struct smmu_domain *dom, vaddr_t va)
{
	/* TODO: garbage collect page tables? */
}

int
smmu_load_map(struct smmu_domain *dom, bus_dmamap_t map)
{
	struct smmu_map_state *sms = map->_dm_cookie;
	u_long dva, maplen;
	int seg;

	maplen = 0;
	for (seg = 0; seg < map->dm_nsegs; seg++) {
		paddr_t pa = map->dm_segs[seg]._ds_paddr;
		psize_t off = pa - trunc_page(pa);
		maplen += round_page(map->dm_segs[seg].ds_len + off);
	}
	KASSERT(maplen <= sms->sms_len);

	dva = sms->sms_dva;
	for (seg = 0; seg < map->dm_nsegs; seg++) {
		paddr_t pa = map->dm_segs[seg]._ds_paddr;
		psize_t off = pa - trunc_page(pa);
		u_long len = round_page(map->dm_segs[seg].ds_len + off);

		map->dm_segs[seg].ds_addr = dva + off;

		pa = trunc_page(pa);
		while (len > 0) {
			smmu_map(dom, dva, pa,
			    PROT_READ | PROT_WRITE,
			    PROT_READ | PROT_WRITE, PMAP_CACHE_WB);

			dva += PAGE_SIZE;
			pa += PAGE_SIZE;
			len -= PAGE_SIZE;
			sms->sms_loaded += PAGE_SIZE;
		}
	}

	return 0;
}

void
smmu_unload_map(struct smmu_domain *dom, bus_dmamap_t map)
{
	struct smmu_map_state *sms = map->_dm_cookie;
	u_long len, dva;

	if (sms->sms_loaded == 0)
		return;

	dva = sms->sms_dva;
	len = sms->sms_loaded;

	while (len > 0) {
		smmu_unmap(dom, dva);

		dva += PAGE_SIZE;
		len -= PAGE_SIZE;
	}

	sms->sms_loaded = 0;

	smmu_tlb_sync_context(dom);
}

int
smmu_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamap)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;
	struct smmu_map_state *sms;
	bus_dmamap_t map;
	u_long dva, len;
	int error;

	error = sc->sc_dmat->_dmamap_create(sc->sc_dmat, size,
	    nsegments, maxsegsz, boundary, flags, &map);
	if (error)
		return error;

	sms = malloc(sizeof(*sms), M_DEVBUF, (flags & BUS_DMA_NOWAIT) ?
	     (M_NOWAIT|M_ZERO) : (M_WAITOK|M_ZERO));
	if (sms == NULL) {
		sc->sc_dmat->_dmamap_destroy(sc->sc_dmat, map);
		return ENOMEM;
	}

	/* Approximation of maximum pages needed. */
	len = round_page(size) + nsegments * PAGE_SIZE;

	/* Allocate IOVA, and a guard page at the end. */
	mtx_enter(&dom->sd_iova_mtx);
	error = extent_alloc_with_descr(dom->sd_iovamap, len + PAGE_SIZE,
	    PAGE_SIZE, 0, 0, EX_NOWAIT, &sms->sms_er, &dva);
	mtx_leave(&dom->sd_iova_mtx);
	if (error) {
		sc->sc_dmat->_dmamap_destroy(sc->sc_dmat, map);
		free(sms, M_DEVBUF, sizeof(*sms));
		return error;
	}

	sms->sms_dva = dva;
	sms->sms_len = len;

	while (len > 0) {
		error = smmu_enter(dom, dva, dva, PROT_READ | PROT_WRITE,
		    PROT_NONE, PMAP_CACHE_WB);
		KASSERT(error == 0); /* FIXME: rollback smmu_enter() */
		dva += PAGE_SIZE;
		len -= PAGE_SIZE;
	}

	map->_dm_cookie = sms;
	*dmamap = map;
	return 0;
}

void
smmu_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;
	struct smmu_map_state *sms = map->_dm_cookie;
	u_long dva, len;
	int error;

	if (sms->sms_loaded)
		smmu_dmamap_unload(t, map);

	dva = sms->sms_dva;
	len = sms->sms_len;

	while (len > 0) {
		smmu_remove(dom, dva);
		dva += PAGE_SIZE;
		len -= PAGE_SIZE;
	}

	mtx_enter(&dom->sd_iova_mtx);
	error = extent_free(dom->sd_iovamap, sms->sms_dva,
	    sms->sms_len + PAGE_SIZE, EX_NOWAIT);
	mtx_leave(&dom->sd_iova_mtx);
	KASSERT(error == 0);

	free(sms, M_DEVBUF, sizeof(*sms));
	sc->sc_dmat->_dmamap_destroy(sc->sc_dmat, map);
}

int
smmu_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;
	int error;

	error = sc->sc_dmat->_dmamap_load(sc->sc_dmat, map,
	    buf, buflen, p, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

int
smmu_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m0,
    int flags)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;
	int error;

	error = sc->sc_dmat->_dmamap_load_mbuf(sc->sc_dmat, map,
	    m0, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

int
smmu_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio,
    int flags)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;
	int error;

	error = sc->sc_dmat->_dmamap_load_uio(sc->sc_dmat, map,
	    uio, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

int
smmu_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;
	int error;

	error = sc->sc_dmat->_dmamap_load_raw(sc->sc_dmat, map,
	    segs, nsegs, size, flags);
	if (error)
		return error;

	error = smmu_load_map(dom, map);
	if (error)
		sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);

	return error;
}

void
smmu_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct smmu_domain *dom = t->_cookie;
	struct smmu_softc *sc = dom->sd_sc;

	smmu_unload_map(dom, map);
	sc->sc_dmat->_dmamap_unload(sc->sc_dmat, map);
}
