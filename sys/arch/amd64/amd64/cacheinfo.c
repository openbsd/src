/*	$OpenBSD: cacheinfo.c,v 1.11 2022/07/12 04:46:00 jsg Exp $	*/

/*
 * Copyright (c) 2022 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/specialreg.h>

void
amd64_print_l1_cacheinfo(struct cpu_info *ci)
{
	u_int ways, linesize, totalsize;
	u_int eax, ebx, ecx, edx;

	if (ci->ci_pnfeatset < 0x80000006)
		return;

	CPUID(0x80000005, eax, ebx, ecx, edx);

	if (ecx == 0)
		return;

	/* L1D */
	linesize = ecx & 0xff;
	ways = (ecx >> 16) & 0xff;
	totalsize = (ecx >> 24) & 0xff; /* KB */

	printf("%s: ", ci->ci_dev->dv_xname);

	if (totalsize < 1024)
		printf("%dKB ", totalsize);
	else
		printf("%dMB ", totalsize >> 10);
	printf("%db/line ", linesize);

	switch (ways) {
	case 0x00:
		/* reserved */
		break;
	case 0x01:
		printf("direct-mapped");
		break;
	case 0xff:
		printf("fully associative");
		break;
	default:
		printf("%d-way", ways);
		break;
	}
	printf(" D-cache, ");

	/* L1C */
	linesize = edx & 0xff;
	ways = (edx >> 16) & 0xff;
	totalsize = (edx >> 24) & 0xff; /* KB */

	if (totalsize < 1024)
		printf("%dKB ", totalsize);
	else
		printf("%dMB ", totalsize >> 10);
	printf("%db/line ", linesize);

	switch (ways) {
	case 0x00:
		/* reserved */
		break;
	case 0x01:
		printf("direct-mapped");
		break;
	case 0xff:
		printf("fully associative");
		break;
	default:
		printf("%d-way", ways);
		break;
	}
	printf(" I-cache\n");
}

void
amd64_print_l2_cacheinfo(struct cpu_info *ci)
{
	u_int ways, linesize, totalsize;
	u_int eax, ebx, ecx, edx;

	if (ci->ci_pnfeatset < 0x80000006)
		return;

	CPUID(0x80000006, eax, ebx, ecx, edx);

	if (ecx == 0)
		return;

	printf("%s: ", ci->ci_dev->dv_xname);

	linesize = ecx & 0xff;
	ways = (ecx >> 12) & 0x0f;
	totalsize = ((ecx >> 16) & 0xffff); /* KB */

	if (totalsize < 1024)
		printf("%dKB ", totalsize);
	else
		printf("%dMB ", totalsize >> 10);
	printf("%db/line ", linesize);

	switch (ways) {
	case 0x00:
		printf("disabled");
		break;
	case 0x01:
		printf("direct-mapped");
		break;
	case 0x02:
	case 0x04:
		printf("%d-way", ways);
		break;
	case 0x06:
		printf("8-way");
		break;
	case 0x03:	
	case 0x05:
	case 0x09:
		/* reserved */
		break;
	case 0x07:
		/* see cpuid 4 sub-leaf 2 */
		break;
	case 0x08:
		printf("16-way");
		break;
	case 0x0a:
		printf("32-way");
		break;
	case 0x0c:
		printf("64-way");
		break;
	case 0x0d:
		printf("96-way");
		break;
	case 0x0e:
		printf("128-way");
		break;
	case 0x0f:
		printf("fully associative");
	}

	printf(" L2 cache\n");
}

void
intel_print_cacheinfo(struct cpu_info *ci, u_int fn)
{
	u_int ways, partitions, linesize, sets, totalsize;
	int type, level, leaf;
	u_int eax, ebx, ecx, edx;

	printf("%s: ", ci->ci_dev->dv_xname);

	for (leaf = 0; leaf < 10; leaf++) {
		CPUID_LEAF(fn, leaf, eax, ebx, ecx, edx);
		type =  eax & 0x1f;
		if (type == 0)
			break;
		level = (eax >> 5) & 7;
	
		ways = (ebx >> 22) + 1;
		linesize = (ebx & 0xfff) + 1;
		partitions =  ((ebx >> 12) & 0x3ff) + 1;
		sets = ecx + 1;

		totalsize = ways * linesize * partitions * sets;

		if (leaf > 0)
			printf(", ");

		if (totalsize < 1024*1024)
			printf("%dKB ", totalsize >> 10);
		else
			printf("%dMB ", totalsize >> 20);
		printf("%db/line %d-way ", linesize, ways);

		if (level == 1) {
			if (type == 1)
				printf("D");
			else if (type == 2)
				printf("I");
			else if (type == 3)
				printf("U");
			printf("-cache");
		} else {
			printf("L%d cache", level);
		}
		
	}
	printf("\n");
}

void
x86_print_cacheinfo(struct cpu_info *ci)
{
	uint64_t msr;

	if (strcmp(cpu_vendor, "GenuineIntel") == 0 &&
	    rdmsr_safe(MSR_MISC_ENABLE, &msr) == 0 &&
	    (msr & MISC_ENABLE_LIMIT_CPUID_MAXVAL) == 0) {
		intel_print_cacheinfo(ci, 4);
		return;
	}

	if (strcmp(cpu_vendor, "AuthenticAMD") == 0 &&
	    (ecpu_ecxfeature & CPUIDECX_TOPEXT)) {
		intel_print_cacheinfo(ci, 0x8000001d);
		return;
	}

	/* 0x80000005 / 0x80000006 */
	amd64_print_l1_cacheinfo(ci);
	amd64_print_l2_cacheinfo(ci);
}
