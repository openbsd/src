
/*	$NetBSD: mba.c,v 1.1 1995/02/13 00:44:02 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

 /* All bugs are subject to removal without further notice */
		


/* mba.c - main mba routines, 930312/ragge */

#include "mba.h"
#include "nexus.h"
#include "vax/mba/mbavar.h"
#include "vax/mba/mbareg.h"

struct mba_ctrl mba_ctrl[NMBA];

extern mba_0(), mba_1(), mba_2(), mba_3();
int   (*mbaintv[4])() = { mba_0, mba_1, mba_2, mba_3 };
#if NMBA > 4
        Need to expand the table for more than 4 massbus adaptors
#endif

mbainterrupt(mba){

	if(mba_hd[mba].mh_mba->mba_sr&MBA_NED){
		printf("Adresserat icke existerande massbussenhet.\n");
		mba_hd[mba].mh_mba->mba_sr=MBA_NED+MBA_MCPE;
		return;
	}
	printf("Interrupt fr}n massbussadapter %d\n",mba);
	printf("mba_hd[mba]->mba_sr: %x\n",mba_hd[mba].mh_mba->mba_sr);
}

/*
 * mbafind() set up interrupt vectors for each found mba and calls 
 * config routines for hp disks, tu and mt tapes (currently only hp).
 */

mbafind(nexnum,nexaddr){
	struct mba_regs *mbr;
	struct mba_device *mi;

	mba_ctrl[nmba].mba_regs= (struct mba_regs *)nexaddr;
	mbr=&(mba_ctrl[nmba].mba_regs);
/*
 * Set up interruptvectors and enable interrupt
 */
	nex_vec_num(14,nexnum)=nex_vec_num(15,nexnum)=
                nex_vec_num(16,nexnum)=nex_vec_num(17,nexnum)=
		(caddr_t)mbaintv[nmba];
	mbr->mba_cr=MBCR_INIT;
	mbr->mba_cr=MBCR_IE;
/*
 * Loop thru all massbuss devices and check for existance
 */

	for(i=0;i<8;i++){
		if(!mbr->mba_drv[i].rmds&MBDS_DPR) continue;
/*
 * Device found; check if generated
 */
		for(mi = mbdinit; mi->driver; mi++) {
			if(mi->alive) continue; /* Already config'd */
		}
	}


}


