/*	$OpenBSD: cpuvar.h,v 1.5 2003/11/03 07:01:33 david Exp $	*/
/*	$NetBSD: cpuvar.h,v 1.2 1999/11/06 20:18:13 eeh Exp $ */

/*
 *  Copyright (c) 1996 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *         This product includes software developed by the NetBSD
 *         Foundation, Inc. and its contributors.
 *  4. Neither the name of The NetBSD Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _sparc64_cpuvar_h
#define _sparc64_cpuvar_h

#include <sys/device.h>

#include <sparc64/sparc64/cache.h>	/* for cacheinfo */

/*
 * The cpu_softc structure. This structure maintains information about one
 * currently installed CPU (there may be several of these if the machine
 * supports multiple CPUs, as on some Sun4m architectures). The information
 * in this structure supersedes the old "cpumod", "mmumod", and similar
 * fields.
 */

struct cpu_softc {
	struct device	dv;		/* generic device info */

	int		node;		/* PROM node for this CPU */

	/* CPU information */
	int		id;		/* Module ID for MP systems */
	int		bus;		/* 1 if CPU is on MBus */

/* XXX - of these, we currently use only cpu_type */
	int		arch;		/* Architecture: CPU_SUN4x */

	int		hz;		/* Clock speed */

	/* Cache information */
	struct cacheinfo	cacheinfo;	/* see cache.h */

	/*
	 * The following pointers point to processes that are somehow
	 * associated with this CPU--running on it, using its FPU,
	 * etc.
	 *
	 * XXXMP: much more needs to go here
	 */
	struct	proc 	*fpproc;		/* FPU owner */
};

/*
 * CPU architectures
 */
#define CPUARCH_UNKNOWN		0
#define CPUARCH_SUN4		1
#define CPUARCH_SUN4C		2
#define CPUARCH_SUN4M		3
#define	CPUARCH_SUN4D		4
#define CPUARCH_SUN4U		5

/*
 * CPU classes
 */
#define CPUCLS_UNKNOWN		0

/*
 * CPU busses
 */

#define CPU_NONE		0	/* No particular bus */
#define CPU_UPA			2	/* UPA bus attached */

/*
 * Related function prototypes
 */
void getcpuinfo(struct cpu_softc *sc, int node);
void mmu_install_tables(struct cpu_softc *);
void pmap_alloc_cpu(struct cpu_softc *);

#define cpuinfo	(*(struct cpu_softc *)CPUINFO_VA)

struct cpu_softc	**cpu_info;

#endif	/* _sparc_cpuvar_h */
