/*	$OpenBSD: mpt_ioctl.h,v 1.2 2004/10/22 05:01:05 marco Exp $	*/
/*
 * Copyright (c) 2004 Marco Peereboom
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _DEV_IC_MPT_IOCTL_H_
#define _DEV_IC_MPT_IOCTL_H_

#include <dev/ic/mpt_mpilib.h>

/* ioctl tunnel defines */
#define MPT_IOCTL_DUMMY _IOWR('B', 32, struct mpt_dummy)
struct mpt_dummy {
	void *cookie;
	int x;
};

/* structures are inside mpt_mpilib.h */
#define MPT_IOCTL_MFG0 _IOWR('B', 33, struct mpt_mfg0)
struct mpt_mfg0 {
	void *cookie;
	fCONFIG_PAGE_MANUFACTURING_0 cpm0;
};

#define MPT_IOCTL_MFG1 _IOWR('B', 34, struct _CONFIG_PAGE_MANUFACTURING_1)
#define MPT_IOCTL_MFG2 _IOWR('B', 35, struct _CONFIG_PAGE_MANUFACTURING_2)
#define MPT_IOCTL_MFG3 _IOWR('B', 36, struct _CONFIG_PAGE_MANUFACTURING_3)
#define MPT_IOCTL_MFG4 _IOWR('B', 37, struct _CONFIG_PAGE_MANUFACTURING_4)

#endif _DEV_IC_MPT_IOCTL_H_
