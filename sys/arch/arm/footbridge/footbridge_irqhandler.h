/*	$OpenBSD: footbridge_irqhandler.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: footbridge_irqhandler.h,v 1.2 2002/11/03 21:43:31 chris Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FOOTBRIDGE_IRQHANDLER_H_
#define _FOOTBRIDGE_IRQHANDLER_H_

#ifndef _LOCORE
#include <sys/types.h>
#endif /* _LOCORE */

#include <machine/intr.h>

void	footbridge_intr_init(void);
void	*footbridge_intr_establish(int, int, int (*)(void *), void *);
void	footbridge_intr_disestablish(void *);

#ifdef _KERNEL
void *footbridge_intr_claim(int irq, int ipl, char *name, int (*func)(void *), void *arg);
void footbridge_intr_init(void);
void footbridge_intr_disestablish(void *cookie);
#endif	/* _KERNEL */

#endif	/* _FOOTBRIDGE_IRQHANDLER_H_ */

/* End of irqhandler.h */
