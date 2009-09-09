/*	$OpenBSD: palm_var.h,v 1.2 2009/09/09 11:34:02 marex Exp $	*/
/*	$NetBSD: lubbock_var.h,v 1.1 2003/06/18 10:51:15 bsh Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EVBARM_PALM_VAR_H
#define _EVBARM_PALM_VAR_H

#ifndef __ASSEMBLER__
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/machine_reg.h>

extern int mach;

#define mach_is_palmtc	( mach == 918 ? 1 : 0 )
#define mach_is_palmt5	( mach == 917 ? 1 : 0 )
#define mach_is_palmtx	( mach == 885 ? 1 : 0 )
#define mach_is_palmld	( mach == 835 ? 1 : 0 )
#define mach_is_palmz72	( mach == 904 ? 1 : 0 )

static inline char *mach_name(void)
{
	if (mach_is_palmt5)		return "Palm Tungsten|T5";
	else if (mach_is_palmtc)	return "Palm Tungsten|C";
	else if (mach_is_palmtx)	return "Palm T|X";
	else if (mach_is_palmld)	return "Palm LifeDrive";
	else if (mach_is_palmz72)	return "Palm Zire72";
	else				return "Unknown";
}
#endif

#ifdef _KERNEL

#define SDRAM_START	0xa0000000
#define SDRAM_SIZE_MB	32
#define SDRAM_SIZE	(SDRAM_SIZE_MB * 1024 * 1024)

#endif

#endif /* _EVBARM_PALM_VAR_H */
