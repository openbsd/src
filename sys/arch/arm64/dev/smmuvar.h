/* $OpenBSD: smmuvar.h,v 1.7 2022/08/10 17:02:37 patrick Exp $ */
/*
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

struct smmu_softc;
struct smmu_domain {
	struct smmu_softc		*sd_sc;
	uint32_t			 sd_sid;
	bus_dma_tag_t			 sd_dmat;
	int				 sd_cb_idx;
	int				 sd_smr_idx;
	int				 sd_stage;
	int				 sd_4level;
	char				 sd_exname[32];
	struct extent			*sd_iovamap;
	union {
		struct smmuvp0 *l0;	/* virtual to physical table 4 lvl */
		struct smmuvp1 *l1;	/* virtual to physical table 3 lvl */
	} sd_vp;
	struct mutex			 sd_iova_mtx;
	struct mutex			 sd_pmap_mtx;
	SIMPLEQ_ENTRY(smmu_domain)	 sd_list;
};

struct smmu_cb {
};

struct smmu_cb_irq {
	struct smmu_softc		*cbi_sc;
	int				 cbi_idx;
};

struct smmu_smr {
	uint16_t mask;
	uint16_t id;
};

struct smmu_softc {
	struct device		  sc_dev;
	bus_space_tag_t		  sc_iot;
	bus_space_handle_t	  sc_ioh;
	bus_dma_tag_t		  sc_dmat;
	int			  sc_is_mmu500;
	int			  sc_is_ap806;
	int			  sc_is_qcom;
	int			  sc_bypass_quirk;
	size_t			  sc_pagesize;
	int			  sc_numpage;
	int			  sc_num_context_banks;
	int			  sc_num_s2_context_banks;
	int			  sc_has_s1;
	int			  sc_has_s2;
	int			  sc_has_exids;
	int			  sc_has_vmid16s;
	int			  sc_num_streams;
	uint16_t		  sc_stream_mask;
	int			  sc_ipa_bits;
	int			  sc_pa_bits;
	int			  sc_va_bits;
	struct smmu_smr		**sc_smr;
	struct smmu_cb		**sc_cb;
	int			  sc_coherent;
	struct pool		  sc_vp_pool;
	struct pool		  sc_vp3_pool;
	SIMPLEQ_HEAD(, smmu_domain) sc_domains;
};

int smmu_attach(struct smmu_softc *);
int smmu_global_irq(void *);
int smmu_context_irq(void *);
bus_dma_tag_t smmu_device_map(void *, uint32_t, bus_dma_tag_t);
void smmu_reserve_region(void *, uint32_t, bus_addr_t, bus_size_t);
