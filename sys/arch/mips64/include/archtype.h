/*	$OpenBSD: archtype.h,v 1.2 2004/08/10 20:28:13 deraadt Exp $	*/
/*
 * Copyright (c) 1997-2003 Opsycon AB, Sweden (www.opsycon.se)
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
 *	This product includes software developed Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _MIPS_ARCHTYPE_H_
#define _MIPS_ARCHTYPE_H_
/*
 * Define architectural identitys for the different Mips machines.
 */
#define	ARC_CLASS		0x0000	/* Arch class ARC */
#define	ACER_PICA_61		0x0001	/* Acer Labs Pica 61 */
#define	MAGNUM			0x0002	/* Mips MAGNUM R4000 */
#define	DESKSTATION_RPC44	0x0003	/* Deskstation xxx */
#define	DESKSTATION_TYNE	0x0004	/* Deskstation xxx */
#define	NKK_AQUARIUS		0x0005	/* NKK R4{67}00 PC */
#define NEC_R94			0x0006	/* NEC Magnum class */
#define	SNI_RM200		0x0007	/* Siemens Nixdorf RM200 */

#define	SGI_CLASS		0x0010	/* Silicon Graphics Class */
#define	SGI_CRIMSON		0x0011	/* Crimson */
#define	SGI_ONYX		0x0012	/* Onyx (!S model Challenge) */
#define	SGI_INDIGO		0x0013	/* Indigo */
#define	SGI_POWER		0x0014	/* POWER Challenge, POWER Onyx */
#define	SGI_INDY		0x0015	/* Indy, Indigo2, Challenge S */
#define	SGI_POWER10		0x0016	/* POWER Challenge R10k */
#define	SGI_POWERI		0x0017	/* POWER Indigo2 */
#define	SGI_O2			0x0018	/* O2/Moosehead */
#define	SGI_OCTANE		0x0019	/* Octane */

#define	ALGOR_CLASS		0x0020	/* Algorithmics Class */
#define	ALGOR_P4032		0x0021	/* ALGORITHMICS P-4032 */
#define	ALGOR_P5064		0x0022	/* ALGORITHMICS P-5064 */

#define	GALILEO_CLASS		0x0030	/* Galileo PCI based Class */
#define	GALILEO_G9		0x0031	/* Galileo GT-64011 Eval board */
#define GALILEO_EV64240		0x0032	/* Galileo EV64240 Eval board */
#define GALILEO_EV64340		0x0033	/* Galileo EV64340 Eval board */

#define	MOMENTUM_CLASS		0x0040	/* Momentum Inc Class */
#define	MOMENTUM_CP7000		0x0041	/* Momentum Ocelot */
#define	MOMENTUM_CP7000G	0x0042	/* Momentum Ocelot-G */
#define	MOMENTUM_JAGUAR		0x0043	/* Momentum Jaguar ATX */

#define	WG_CLASS		0x0050	/* Willowglen class */
#define	WG4308			0x0052	/* Willowglen 4308 LMD */
#define	WG4309			0x0053	/* Willowglen 4309 LMD */
#define	WG4409			0x0054	/* Willowglen 4409 LMD */
#define	WG8138			0x0055	/* Willowglen 8138 523x VME card */
#define	WG8168			0x0056	/* Willowglen 8168 5231 VME card */
#define	WG6000			0x0057	/* Willowglen CPU-6000 */
#define	WG7000			0x0058	/* Willowglen CPU-7000 */
#define	WG8200			0x0059	/* Willowglen CPU-8200 */
#define	WG8232			0x005a	/* Willowglen CPU-8232 */

#define	MISC_CLASS		0x00F0	/* Misc machines... */
#define	LAGUNA			0x00F1	/* Heurikon Laguna VME board */

#define	ARCHCLASS(n)	((n) & 0xf0)

#endif /* !_MIPS_ARCHTYPE_H_ */
