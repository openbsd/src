/*	$NetBSD: sitara_cmreg.h,v 1.1 2013/04/17 15:04:39 bouyer Exp $	*/

/*
 * Copyright (c) 2013 Manuel Bouyer.  All rights reserved.
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

/* register definitions for the Control module found in the
 * Texas Instrument AM335x SOC
 */

#ifndef _OMAP2SCMREG_H
#define _OMAP2SCMREG_H

#define OMAP2SCM_REVISION	0x0000
#define SCM_REVISION_SCHEME(x)	(((x) & 0xc0000000) >> 30)
#define SCM_REVISION_FUNC(x)	(((x) & 0x0fff0000) >> 16)
#define SCM_REVISION_RTL(x)	(((x) & 0x0000f800) >> 11)
#define SCM_REVISION_MAJOR(x)	(((x) & 0x00000700) >>  8)
#define SCM_REVISION_CUSTOM(x)	(((x) & 0x000000c0) >>  6)
#define SCM_REVISION_MINOR(x)	(((x) & 0x0000001f) >>  0)

#define OMAP2SCM_MAC_ID0_LO	0x630
#define OMAP2SCM_MAC_ID0_HI	0x634

#endif /* _OMAP2SCMREG_H */
