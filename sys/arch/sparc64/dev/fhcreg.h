/*	$OpenBSD: fhcreg.h,v 1.1 2004/09/24 20:47:39 jason Exp $	*/

/*
 * Copyright (c) 2004 Jason L. Wright (jason@thought.net).
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

#define	FHC_P_ID	0x00000000		/* ID */
#define	FHC_P_RCS	0x00000010		/* reset ctrl/status */
#define	FHC_P_CTRL	0x00000020		/* control */
#define	FHC_P_BSR	0x00000030		/* board status */
#define	FHC_P_ECC	0x00000040		/* ECC control */
#define	FHC_P_JCTRL	0x000000f0		/* JTAG control */

#define	FHC_I_IGN	0x00000000		/* IGN register */

#define	FHC_F_IMAP	0x00000000		/* fanfail intr map */
#define	FHC_F_ICLR	0x00000010		/* fanfail intr clr */

#define	FHC_S_IMAP	0x00000000		/* system intr map */
#define	FHC_S_ICLR	0x00000010		/* system intr clr */

#define	FHC_U_IMAP	0x00000000		/* uart intr map */
#define	FHC_U_ICLR	0x00000010		/* uart intr clr */

#define	FHC_T_IMAP	0x00000000		/* tod intr map */
#define	FHC_T_ICLR	0x00000010		/* tod intr clr */

struct fhc_intr_reg {
	u_int64_t imap;
	u_int64_t iclr;
};

