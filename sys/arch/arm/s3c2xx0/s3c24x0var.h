/*	$OpenBSD: s3c24x0var.h,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
/* $NetBSD: s3c24x0var.h,v 1.3 2005/12/11 12:16:51 christos Exp $ */

/*
 * Copyright (c) 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_S3C24X0VAR_H_
#define _ARM_S3C24X0VAR_H_

#include <arm/s3c2xx0/s3c2xx0var.h>

struct s3c24x0_softc {
	struct s3c2xx0_softc  sc_sx;

	bus_space_handle_t  sc_timer_ioh; /* Timer control registers */
};

void	s3c24x0_clock_freq(struct s3c2xx0_softc *);
void	s3c24x0_clock_freq2(vaddr_t, int *, int *, int *);
void	*s3c24x0_intr_establish(int, int, int, s3c2xx0_irq_handler_t, void *);

#endif /* _ARM_S3C24X0VAR_H_ */
