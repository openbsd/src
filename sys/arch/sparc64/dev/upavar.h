/*	$NetBSD: upavar.h,v 1.2 2000/01/14 14:33:31 pk Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)upavar.h	8.1 (Berkeley) 6/11/93
 */

#ifndef _UPA_VAR_H
#define _UPA_VAR_H

/* Device register space description */
struct upa_reg {
	int64_t	ur_paddr;
	int64_t	ur_len;
};

/*
 * UPA bus variables.
 */
struct upadev {
	struct	device *ud_dev;		/* backpointer to generic */
	struct	upadev *ud_bchain;	/* forward link in bus chain */
	void	(*ud_reset) __P((struct device *));
};

/* variables per Upa */
struct upa_softc {
	struct	device uc_dev;		/* base device */
	bus_space_tag_t	uc_bustag;
	bus_dma_tag_t	uc_dmatag;
	int	uc_clockfreq;		/* clock frequency (in Hz) */
	struct	upadev *uc_sbdev;	/* list of all children */
};

/*
 * Upa driver attach arguments.
 */
struct upa_attach_args {
	bus_space_tag_t	ua_bustag;
	bus_dma_tag_t	ua_dmatag;
	char		*ua_name;	/* PROM node name */
	int		ua_node;	/* PROM handle */
	struct upa_reg	*ua_reg;	/* "reg" properties */
	int		ua_nreg;
	void*		*ua_address;	/* "address" properties */
	int		ua_naddress;
	int		*ua_interrupts;	/* "interrupts" properties */
	int		ua_ninterrupts;
	int		ua_pri;		/* priority (IPL) */
};

/* upa_attach() is also used from obio.c */
void	upa_attach __P((struct upa_softc *, char *, int,
			 const char * const *));
int	upa_print __P((void *, const char *));

int	upadev_match __P((struct cfdata *, void *));
void	upa_establish __P((struct upadev *, struct device *));

int	upa_setup_attach_args __P((
		struct upa_softc *,
		bus_space_tag_t,
		bus_dma_tag_t,
		int,			/*node*/
		struct upa_attach_args *));

#define upa_bus_map(t, bt, a, s, f, v, hp) \
	bus_space_map2(t, bt, a, s, f, v, hp)

#endif /* _UPA_VAR_H */
