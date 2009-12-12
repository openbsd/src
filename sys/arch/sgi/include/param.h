/*	$OpenBSD: param.h,v 1.5 2009/12/12 20:08:08 miod Exp $ */

/*
 * Copyright (c) 2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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

#ifndef _MACHINE_PARAM_H_
#define _MACHINE_PARAM_H_

/*
 * Machine dependent constants.
 */
#define	MACHINE		"sgi"
#define	_MACHINE	sgi
#define MACHINE_ARCH	"mips64"
#define _MACHINE_ARCH	mips64

#define MID_MACHINE	MID_MIPS64	/* None but has to be defined */

#ifdef _KERNEL
#if defined(CPU_R10000) && !defined(CPU_R5000) && !defined(CPU_RM7000)
#define	PAGE_SHIFT	14
#else
#define	PAGE_SHIFT	12
#endif
#endif	/* _KERNEL */

#include <mips64/param.h>

#endif /* _MACHINE_PARAM_H_ */
