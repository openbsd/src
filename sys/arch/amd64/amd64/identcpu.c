/*	$OpenBSD: identcpu.c,v 1.1 2004/01/28 01:39:38 mickey Exp $	*/
/*	$NetBSD: identcpu.c,v 1.1 2003/04/26 18:39:28 fvdl Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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

#include <sys/types.h>
#include <sys/systm.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

/* sysctl wants this. */
char cpu_model[48];

void
identifycpu(struct cpu_info *ci)
{
	u_int64_t last_tsc;
	u_int32_t dummy, val;
	u_int32_t brand[12];

	CPUID(1, ci->ci_signature, val, dummy, ci->ci_feature_flags);
	CPUID(0x80000001, dummy, dummy, dummy, val);
	ci->ci_feature_flags |= val;

	CPUID(0x80000002, brand[0], brand[1], brand[2], brand[3]);
	CPUID(0x80000003, brand[4], brand[5], brand[6], brand[7]);
	CPUID(0x80000004, brand[8], brand[9], brand[10], brand[11]);

	strlcpy(cpu_model, (char *)brand, sizeof(cpu_model));
	if (cpu_model[0] == 0)
		strlcpy(cpu_model, "Opteron or Athlon 64", sizeof(cpu_model));

	last_tsc = rdtsc();
	delay(100000);
	ci->ci_tsc_freq = (rdtsc() - last_tsc) * 10;

	amd_cpu_cacheinfo(ci);

	printf("%s: %s", ci->ci_dev->dv_xname, cpu_model);

	if (ci->ci_tsc_freq != 0)
		printf(", %lu.%02lu MHz", (ci->ci_tsc_freq + 4999) / 1000000,
		    ((ci->ci_tsc_freq + 4999) / 10000) % 100);
	printf("\n");

	if ((ci->ci_feature_flags & CPUID_MASK1) != 0) {
		printf("%s: features: %b\n", ci->ci_dev->dv_xname, 
		    ci->ci_feature_flags, CPUID_FLAGS1);
	}
	if ((ci->ci_feature_flags & CPUID_MASK2) != 0) {
		printf("%s: features: %b\n", ci->ci_dev->dv_xname,
		    ci->ci_feature_flags, CPUID_EXT_FLAGS2);
	}
	if ((ci->ci_feature_flags & CPUID_MASK3) != 0) {
		printf("%s: features: %b\n", ci->ci_dev->dv_xname,
		    ci->ci_feature_flags, CPUID_EXT_FLAGS3);
	}

	x86_print_cacheinfo(ci);

	return;	/* TODO - warning to fix this ifdef later */
#ifdef notyet
	microtime_func = cc_microtime;
#endif
}

void
cpu_probe_features(struct cpu_info *ci)
{
	ci->ci_feature_flags = cpu_feature;
	ci->ci_signature = 0;
}
