/*	$OpenBSD: xboxreg.h,v 1.2 2003/06/02 18:40:59 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

struct xbox_errs {
	volatile	u_int32_t xe_errd;	/* error descriptor */
	volatile	u_int32_t xe_errva;	/* error virtual addr */
	volatile	u_int32_t xe_stat;	/* status */
	volatile	u_int32_t xe_ctl0;	/* control reg 0 */
};

struct xbox_regs {
	/* XA space */
	volatile	u_int32_t *xa_write0;	/* write0 reg */
	struct		xbox_errs *xa_errs;	/* errors */
	volatile	u_int32_t *xa_ctl0;	/* ctl0 reg */
	volatile	u_int32_t *xa_ctl1;	/* ctl1 reg */
	volatile	u_int32_t *xa_elua;
	volatile	u_int32_t *xa_ella;
	volatile	u_int32_t *xa_rsrv;	/* reserved */

	/* XB space */
	struct		xbox_errs *xb_errs;	/* errors */
	volatile	u_int32_t *xb_ctl0;	/* ctl0 reg */
	volatile	u_int32_t *xb_ctl1;	/* ctl1 reg */
	volatile	u_int32_t *xb_elua;
	volatile	u_int32_t *xb_ella;
	volatile	u_int32_t *xb_rsrv;	/* reserved */
};

#define	XAC_CTL0_OFFSET		0x100000	/* control reg 0 */
#define	XAC_CTL1_OFFSET		0x110000	/* control reg 1 */
#define	XAC_ELUA_OFFSET		0x120000
#define	XAC_ELLA_OFFSET		0x130000
#define	XAC_ERR_EN_OFFSET	0x140000	/* error enable */

#define	XBC_CTL0_OFFSET		0x500000	/* control reg 0 */
#define	XBC_CTL1_OFFSET		0x510000	/* control reg 1 */
#define	XBC_ELUA_OFFSET		0x520000
#define	XBC_ELLA_OFFSET		0x530000
#define	XBC_ERR_EN_OFFSET	0x540000	/* error enable */


/*
 * Control register 1
 */
#define	XBOX_CTL1_CDPTE1	0x8000	/* cable data parity test enb */
#define	XBOX_CTL1_CRTE		0x4000	/* cable rerun test enb */
#define	XBOX_CTL1_SRST		0x3000	/* software reset mask */
#define	XBOX_CTL1_SRST_XARS	0x1000
#define	XBOX_CTL1_SRST_CRES	0x2000
#define	XBOX_CTL1_SRST_HRES	0x3000
#define	XBOX_CTL1_DTE		0x0800	/* dvma test enb */
#define	XBOX_CTL1_CDPTE		0x0400	/* cable data parity test enb */
#define	XBOX_CTL1_ITE		0x0200	/* interrupt test enb */
#define	XBOX_CTL1_CDPTE0	0x0100	/* cable data parity test enb, dpr0 */
#define	XBOX_CTL1_ELDS		0x00c0	/* error log dvma size */
#define	XBOX_CTL1_XSSE		0x0020	/* expansion sbus slot select enb */
#define	XBOX_CTL1_XSBRE		0x0010	/* expansion sbus bus request enb */
#define	XBOX_CTL1_XSIE		0x0008	/* expansion sbus interrupt enb */
#define	XBOX_CTL1_ELDE		0x0004	/* error log dvma enable */
#define	XBOX_CTL1_CSIE		0x0002	/* cable serial interrupt enb */
#define	XBOX_CTL1_TRANSPARENT	0x0001	/* transparent mode enb */
