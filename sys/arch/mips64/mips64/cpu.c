/*	$OpenBSD: cpu.c,v 1.7 2004/09/09 22:11:38 pefo Exp $ */

/*
 * Copyright (c) 1997-2004 Opsycon AB (www.opsycon.se)
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>


int	cpumatch(struct device *, void *, void *);
void	cpuattach(struct device *, struct device *, void *);

int cpu_is_rm7k = 0;

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};
struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL, NULL, 0
};

int
cpumatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(ca->ca_name, cpu_cd.cd_name) != 0)
		return 0;
	if (cf->cf_unit >= MAX_CPUS)
		return 0;

	return 20;	/* Make CPU probe first */
}

void
cpuattach(struct device *parent, struct device *dev, void *aux)
{
	int cpuno = dev->dv_unit;

	printf(": ");

	switch (sys_config.cpu[cpuno].type) {
	case MIPS_R4000:
		if (CpuPrimaryInstCacheSize == 16384)
			printf("MIPS R4400 CPU");
		else
			printf("MIPS R4000 CPU");
		break;
	case MIPS_R5000:
		printf("MIPS R5000 CPU");
		break;
	case MIPS_R10000:
		printf("MIPS R10000 CPU");
		break;
	case MIPS_R4200:
		printf("NEC VR4200 CPU (ICE)");
		break;
	case MIPS_R4300:
		printf("NEC VR4300 CPU");
		break;
	case MIPS_R4100:
		printf("NEC VR41xx CPU");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion CPU");
		break;
	case MIPS_R4700:
		printf("QED R4700 Orion CPU");
		break;
	case MIPS_RM52XX:
		printf("PMC-Sierra RM52X0 CPU");
		break;
	case MIPS_RM7000:
		if (sys_config.cpu[cpuno].vers_maj < 2)
			printf("PMC-Sierra RM7000 CPU");
		else
			printf("PMC-Sierra RM7000A CPU");
		cpu_is_rm7k++;
		break;
	case MIPS_RM9000:
		printf("PMC-Sierra RM9000 CPU");
		break;
	default:
		printf("Unknown CPU type (0x%x)",sys_config.cpu[cpuno].type);
		break;
	}
	printf(" rev %d.%d %d MHz with ", sys_config.cpu[cpuno].vers_maj,
	    sys_config.cpu[cpuno].vers_min,
	    sys_config.cpu[cpuno].clock / 1000000);

	switch (sys_config.cpu[cpuno].fptype) {
	case MIPS_SOFT:
		printf("Software emulation float");
		break;
	case MIPS_R4010:
		printf("R4010 FPC");
		break;
	case MIPS_R10010:
		printf("R10000 FPU");
		break;
	case MIPS_R4210:
		printf("VR4200 FPC (ICE)");
		break;
	case MIPS_R4600:
		printf("R4600 Orion FPC");
		break;
	case MIPS_R4700:
		printf("R4700 Orion FPC");
		break;
	case MIPS_R5000:
		printf("R5000 based FPC");
		break;
	case MIPS_RM52XX:
		printf("RM52X0 FPC");
		break;
	case MIPS_RM7000:
		printf("RM7000 FPC");
		break;
	case MIPS_RM9000:
		printf("RM9000 FPC");
		break;
	case MIPS_UNKF1:
	default:
		printf("Unknown FPU type (0x%x)", sys_config.cpu[cpuno].fptype);
		break;
	}
	printf(" rev %d.%d\n", sys_config.cpu[cpuno].fpvers_maj,
	    sys_config.cpu[cpuno].fpvers_min);

	printf("cpu%d: cache L1-I %dKB", cpuno, CpuPrimaryInstCacheSize / 1024);
	printf(" D %dKB ", CpuPrimaryDataCacheSize / 1024);

	switch (CpuNWayCache) {
	case 2:
		printf("2 way");
		break;
	case 4:
		printf("4 way");
		break;
	default:
		printf("1 way");
		break;
	}

	if (CpuSecondaryCacheSize != 0) {
		switch (sys_config.cpu[cpuno].type) {
		case MIPS_RM7000:
		case MIPS_RM9000:
			printf(", L2 %dKB 4 way", CpuSecondaryCacheSize / 1024);
			break;
		default:
			printf(", L2 %dKB direct", CpuSecondaryCacheSize / 1024);
			break;
		}
	}
	if (CpuTertiaryCacheSize != 0)
		printf(", L3 %dKB direct", CpuTertiaryCacheSize / 1024);
	printf("\n");

#ifdef DEBUG
	printf("cpu%d: Setsize %d:%d\n", cpuno,
	    CpuPrimaryInstSetSize, CpuPrimaryDataSetSize);
	printf("cpu%d: Alias mask 0x%x\n", cpuno, CpuCacheAliasMask);
	printf("cpu%d: Config Register %x\n", cpuno, CpuConfigRegister);
	printf("cpu%d: Cache type %x\n", cpuno, CpuCacheType);
	if (sys_config.cpu[cpuno].fptype == MIPS_RM7000) {
		u_int tmp = CpuConfigRegister;

		printf("cpu%d: ", cpuno);
		printf("K0 = %1d  ",0x7 & tmp);
		printf("SE = %1d  ",0x1 & (tmp>>3));
		printf("DB = %1d  ",0x1 & (tmp>>4));
		printf("IB = %1d\n",0x1 & (tmp>>5));
		printf("cpu%d: ", cpuno);
		printf("DC = %1d  ",0x7 & (tmp>>6));
		printf("IC = %1d  ",0x7 & (tmp>>9));
		printf("TE = %1d  ",0x1 & (tmp>>12));
		printf("EB = %1d\n",0x1 & (tmp>>13));
		printf("cpu%d: ", cpuno);
		printf("EM = %1d  ",0x1 & (tmp>>14));
		printf("BE = %1d  ",0x1 & (tmp>>15));
		printf("TC = %1d  ",0x1 & (tmp>>17));
		printf("EW = %1d\n",0x3 & (tmp>>18));
		printf("cpu%d: ", cpuno);
		printf("TS = %1d  ",0x3 & (tmp>>20));
		printf("EP = %1d  ",0xf & (tmp>>24));
		printf("EC = %1d  ",0x7 & (tmp>>28));
		printf("SC = %1d\n",0x1 & (tmp>>31));
	}
	printf("cpu%d: Status Register %x\n", cpuno, CpuStatusRegister);
#endif
}
