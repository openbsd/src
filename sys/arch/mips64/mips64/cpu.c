/*	$OpenBSD: cpu.c,v 1.1 2004/08/06 20:56:03 pefo Exp $ */

/*
 * Copyright (c) 1997-2003 Opsycon AB (www.opsycon.se)
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
 *	This product includes software developed by Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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


/* Definition of the driver for autoconfig. */
static int	cpumatch(struct device *, void *, void *);
static void	cpuattach(struct device *, struct device *, void *);

int cpu_is_rm7k = 0;

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};
struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL, NULL, 0
};

static int
cpumatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(ca->ca_name, cpu_cd.cd_name) != 0)
		return (0);

	return (20);	/* Make CPU probe first */
}

static void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{

	printf(": ");

	switch(sys_config.cpu.type) {

	case MIPS_R4000:
		if(CpuPrimaryInstCacheSize == 16384)
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
		if(sys_config.cpu.vers_maj < 2) {
			printf("PMC-Sierra RM7000 CPU");
		}	
		else {
			printf("PMC-Sierra RM7000A CPU");
		}	
		cpu_is_rm7k++;
		break;
	case MIPS_RM9000:
		printf("PMC-Sierra RM9000 CPU");
		break;
	default:
		printf("Unknown CPU type (0x%x)",sys_config.cpu.type);
		break;
	}
	printf(" Rev. %d.%d with ", sys_config.cpu.vers_maj, sys_config.cpu.vers_min);


	switch(fpu_id.cpu.cp_imp) {

	case MIPS_SOFT:
		printf("Software emulation float");
		break;
	case MIPS_R4010:
		printf("MIPS R4010 FPC");
		break;
	case MIPS_R10010:
		printf("MIPS R10000 FPU");
		break;
	case MIPS_R4210:
		printf("NEC VR4200 FPC (ICE)");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion FPC");
		break;
	case MIPS_R4700:
		printf("QED R4700 Orion FPC");
		break;
	case MIPS_R5000:
		printf("MIPS R5000 based FPC");
		break;
	case MIPS_RM52XX:
		printf("PMC-Sierra RM52X0 FPC");
		break;
	case MIPS_RM7000:
		printf("PMC-Sierra RM7000 FPC");
		break;
	case MIPS_RM9000:
		printf("PMC-Sierra RM9000 FPC");
		break;
	case MIPS_UNKF1:
	default:
		printf("Unknown FPU type (0x%x)", fpu_id.cpu.cp_imp);
		break;
	}
	printf(" Rev. %d.%d", fpu_id.cpu.cp_majrev, fpu_id.cpu.cp_minrev);
	printf("\n");

	printf("        CPU clock %dMhz\n",sys_config.cpu.clock/1000000);
	printf("        L1 Cache: I size %dkb(%d line),",
		CpuPrimaryInstCacheSize / 1024,
		CpuPrimaryInstCacheLSize);
	printf(" D size %dkb(%d line), ",
		CpuPrimaryDataCacheSize / 1024,
		CpuPrimaryDataCacheLSize);
	switch(CpuNWayCache) {
	case 2:
		printf("two way.\n");
		break;
	case 4:
		printf("four way.\n");
		break;
	default:	
		printf("direct mapped.\n");
		break;
	}
	if(CpuSecondaryCacheSize != 0) {
		switch(fpu_id.cpu.cp_imp) {
		case MIPS_RM7000:
			printf("        L2 Cache: Size %dkb, four way\n",
				CpuSecondaryCacheSize / 1024);
			break;

		default:
			printf("        L2 Cache: Size %dkb, direct mapped\n",
				CpuSecondaryCacheSize / 1024);
			break;
		}

	}
	if(CpuTertiaryCacheSize != 0) {
		printf("        L3 Cache: Size %dkb, direct mapped\n",
			CpuTertiaryCacheSize / 1024);
	}

#ifdef DEBUG
	printf("\tSetsize %d:%d\n", CpuPrimaryInstSetSize, CpuPrimaryDataSetSize);
	printf("\tAlias mask 0x%x\n", CpuCacheAliasMask);
	printf("\tConfig Register %x\n",CpuConfigRegister);
	printf("\tCache type %x\n", CpuCacheType);
	if(fpu_id.cpu.cp_imp == MIPS_RM7000) {
		u_int tmp;
		tmp = CpuConfigRegister;
		printf("\t\t\t");
		printf("K0 = %1d  ",0x7 & tmp);
		printf("SE = %1d  ",0x1 & (tmp>>3));
		printf("DB = %1d  ",0x1 & (tmp>>4));
		printf("IB = %1d\n",0x1 & (tmp>>5));
		printf("\t\t\t");
		printf("DC = %1d  ",0x7 & (tmp>>6));
		printf("IC = %1d  ",0x7 & (tmp>>9));
		printf("TE = %1d  ",0x1 & (tmp>>12));
		printf("EB = %1d\n",0x1 & (tmp>>13));
		printf("\t\t\t");
		printf("EM = %1d  ",0x1 & (tmp>>14));
		printf("BE = %1d  ",0x1 & (tmp>>15));
		printf("TC = %1d  ",0x1 & (tmp>>17));
		printf("EW = %1d\n",0x3 & (tmp>>18));
		printf("\t\t\t");
		printf("TS = %1d  ",0x3 & (tmp>>20));
		printf("EP = %1d  ",0xf & (tmp>>24));
		printf("EC = %1d  ",0x7 & (tmp>>28));
		printf("SC = %1d\n",0x1 & (tmp>>31));
	}
	printf("\tStatus Register %x\n",CpuStatusRegister);
#endif
}
