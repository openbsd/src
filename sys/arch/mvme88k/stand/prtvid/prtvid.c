/*	$OpenBSD: prtvid.c,v 1.3 2004/12/27 15:23:46 drahn Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn <drahn@openbsd.org>
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

#include <stdio.h>
#define __DBINTERFACE_PRIVATE
#include <db.h>
#include "vid.h"

main(argc, argv)
	int argc;
	char *argv[];
{
	struct vid *pvid;
	struct cfg *pcfg;

	pvid = (struct vid *) malloc(sizeof (struct vid));

	fread(pvid, sizeof(struct vid), 1, stdin);

	if (BYTE_ORDER != BIG_ENDIAN)
		swabvid(pvid);

	printf("vid_id		%s	%x\n", pvid->vid_id,
	    (char *)&(pvid->vid_id[4]) - (char *)pvid);
	printf("vid_oss		%x	%x\n", pvid->vid_oss,
	    (char *)&(pvid->vid_oss) - (char *)pvid);
	printf("vid_osl		%x	%x\n", pvid->vid_osl,
	    (char *)&(pvid->vid_osl) - (char *)pvid);
	printf("vid_osa_u	%x	%x\n", pvid->vid_osa_u,
	    (char *)&(pvid->vid_osa_u) - (char *)pvid);
	printf("vid_osa_l	%x	%x\n", pvid->vid_osa_l,
	    (char *)&(pvid->vid_osa_l) - (char *)pvid);
	printf("vid_vd 		%x	%x\n", pvid->vid_vd, 
	    (char *)&(pvid->vid_vd) - (char *)pvid);
	printf("vid_cas		%x	%x\n", pvid->vid_cas,
	    (char *)&(pvid->vid_cas) - (char *)pvid);
	printf("vid_cal		%x	%x\n", pvid->vid_cal,
	    (char *)&(pvid->vid_cal) - (char *)pvid);
	printf("vid_moto	%s	%x\n", pvid->vid_mot,
	    (char *)&(pvid->vid_mot[0]) - (char *)pvid);

	free(pvid);

	pcfg = (struct cfg *) malloc(sizeof(struct cfg));

	fread(pcfg, sizeof(struct cfg), 1, stdin);

	if (BYTE_ORDER != BIG_ENDIAN)
		swabcfg(pcfg);

	printf("cfg_atm		%x	%x\n", pcfg->cfg_atm,
	    (char *)&(pcfg->cfg_atm) - (char *)(pcfg));
	printf("cfg_prm		%x	%x\n", pcfg->cfg_prm,
	    (char *)&(pcfg->cfg_prm) - (char *)(pcfg));
	printf("cfg_atw		%x	%x\n", pcfg->cfg_atw,
	    (char *)&(pcfg->cfg_atw) - (char *)(pcfg));
	printf("cfg_rec		%x	%x\n",(long)pcfg->cfg_rec,
	    (char *)&(pcfg->cfg_rec) - (char *)(pcfg));
	printf("cfg_spt		%x	%x\n", pcfg->cfg_spt,
	    (char *)&(pcfg->cfg_spt) - (char *)(pcfg));
	printf("cfg_hds		%x	%x\n", pcfg->cfg_hds,
	    (char *)&(pcfg->cfg_hds) - (char *)(pcfg));
	printf("cfg_trk		%x	%x\n", pcfg->cfg_trk,
	    (char *)&(pcfg->cfg_trk) - (char *)(pcfg));
	printf("cfg_ilv		%x	%x\n", pcfg->cfg_ilv,
	    (char *)&(pcfg->cfg_ilv) - (char *)(pcfg));
	printf("cfg_sof		%x	%x\n", pcfg->cfg_sof,
	    (char *)&(pcfg->cfg_sof) - (char *)(pcfg));
	printf("cfg_psm		%x	%x\n", pcfg->cfg_psm,
	    (char *)&(pcfg->cfg_psm) - (char *)(pcfg));
	printf("cfg_shd		%x	%x\n", pcfg->cfg_shd,
	    (char *)&(pcfg->cfg_shd) - (char *)(pcfg));
	printf("cfg_pcom	%x	%x\n", pcfg->cfg_pcom,
	    (char *)&(pcfg->cfg_pcom) - (char *)(pcfg));
	printf("cfg_ssr 	%x	%x\n", pcfg->cfg_ssr,
	    (char *)&(pcfg->cfg_ssr) - (char *)(pcfg));
	printf("cfg_rwcc	%x	%x\n", pcfg->cfg_rwcc,
	    (char *)&(pcfg->cfg_rwcc) - (char *)(pcfg));
	printf("cfg_ecc 	%x	%x\n", pcfg->cfg_ecc,
	    (char *)&(pcfg->cfg_ecc) - (char *)(pcfg));
	printf("cfg_eatm	%x	%x\n", pcfg->cfg_eatm,
	    (char *)&(pcfg->cfg_eatm) - (char *)(pcfg));
	printf("cfg_eprm	%x	%x\n", pcfg->cfg_eprm,
	    (char *)&(pcfg->cfg_eprm) - (char *)(pcfg));
	printf("cfg_eatw	%x	%x\n", pcfg->cfg_eatw,
	    (char *)&(pcfg->cfg_eatw) - (char *)(pcfg));
	printf("cfg_gpb1	%x	%x\n", pcfg->cfg_gpb1,
	    (char *)&(pcfg->cfg_gpb1) - (char *)(pcfg));
	printf("cfg_gpb2	%x	%x\n", pcfg->cfg_gpb2,
	    (char *)&(pcfg->cfg_gpb2) - (char *)(pcfg));
	printf("cfg_gpb3	%x	%x\n", pcfg->cfg_gpb3,
	    (char *)&(pcfg->cfg_gpb3) - (char *)(pcfg));
	printf("cfg_gpb4	%x	%x\n", pcfg->cfg_gpb4,
	    (char *)&(pcfg->cfg_gpb4) - (char *)(pcfg));
	printf("cfg_ssc		%x	%x\n", pcfg->cfg_ssc,
	    (char *)&(pcfg->cfg_ssc) - (char *)(pcfg));
	printf("cfg_runit	%x	%x\n", pcfg->cfg_runit,
	    (char *)&(pcfg->cfg_runit) - (char *)(pcfg));
	printf("cfg_rsvc1	%x	%x\n", pcfg->cfg_rsvc1,
	    (char *)&(pcfg->cfg_rsvc1) - (char *)(pcfg));
	printf("cfg_rsvc2	%x	%x\n", pcfg->cfg_rsvc2,
	    (char *)&(pcfg->cfg_rsvc2) - (char *)(pcfg));
}

swabvid(pvid)
	struct vid *pvid;
{
	M_32_SWAP(pvid->vid_oss);
	M_16_SWAP(pvid->vid_osl);
	M_16_SWAP(pvid->vid_osa_u);
	M_16_SWAP(pvid->vid_osa_l);
	M_32_SWAP(pvid->vid_cas);
}

swabcfg(pcfg)
	struct cfg *pcfg;
{
	printf("swapping cfg\n");

	M_16_SWAP(pcfg->cfg_atm);
	M_16_SWAP(pcfg->cfg_prm);
	M_16_SWAP(pcfg->cfg_atm);
	M_16_SWAP(pcfg->cfg_rec);
	M_16_SWAP(pcfg->cfg_trk);
	M_16_SWAP(pcfg->cfg_psm);
	M_16_SWAP(pcfg->cfg_shd);
	M_16_SWAP(pcfg->cfg_pcom);
	M_16_SWAP(pcfg->cfg_rwcc);
	M_16_SWAP(pcfg->cfg_ecc);
	M_16_SWAP(pcfg->cfg_eatm);
	M_16_SWAP(pcfg->cfg_eprm);
	M_16_SWAP(pcfg->cfg_eatw);
	M_16_SWAP(pcfg->cfg_rsvc1);
	M_16_SWAP(pcfg->cfg_rsvc2);
}
