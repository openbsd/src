/*	$OpenBSD: cpu.c,v 1.4 1998/09/15 10:50:12 pefo Exp $ */

/*
 * Copyright (c) 1997, 1998 Per Fogelstrom, Opsycon AB
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
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

	return (1);
}

static void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{

	printf(": ");

	switch(cpu_id.cpu.cp_imp) {

	case MIPS_R2000:
		printf("MIPS R2000 CPU");
		break;
	case MIPS_R3000:
		printf("MIPS R3000 CPU");
		break;
	case MIPS_R6000:
		printf("MIPS R6000 CPU");
		break;
	case MIPS_R4000:
		if(CpuPrimaryInstCacheSize == 16384)
			printf("MIPS R4400 CPU");
		else
			printf("MIPS R4000 CPU");
		break;
	case MIPS_R3LSI:
		printf("LSI Logic R3000 derivate");
		break;
	case MIPS_R6000A:
		printf("MIPS R6000A CPU");
		break;
	case MIPS_R3IDT:
		printf("IDT R3000 derivate");
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
	case MIPS_R8000:
		printf("MIPS R8000 Blackbird/TFP CPU");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion CPU");
		break;
	case MIPS_R4700:
		printf("QED R4700 Orion CPU");
		break;
	case MIPS_R3TOSH:
		printf("Toshiba R3000 based CPU");
		break;
	case MIPS_RM52X0:
		printf("QED RM52X0 CPU");
		break;
	default:
		printf("Unknown CPU type (0x%x)",cpu_id.cpu.cp_imp);
		break;
	}
	printf(" Rev. %d.%d with ", cpu_id.cpu.cp_majrev, cpu_id.cpu.cp_minrev);


	switch(fpu_id.cpu.cp_imp) {

	case MIPS_SOFT:
		printf("Software emulation float");
		break;
	case MIPS_R2360:
		printf("MIPS R2360 FPC");
		break;
	case MIPS_R2010:
		printf("MIPS R2010 FPC");
		break;
	case MIPS_R3010:
		printf("MIPS R3010 FPC");
		break;
	case MIPS_R6010:
		printf("MIPS R6010 FPC");
		break;
	case MIPS_R4010:
		printf("MIPS R4010 FPC");
		break;
	case MIPS_R31LSI:
		printf("FPC");
		break;
	case MIPS_R10010:
		printf("MIPS R10000 FPU");
		break;
	case MIPS_R4210:
		printf("NEC VR4200 FPC (ICE)");
		break;
	case MIPS_R8000:
		printf("MIPS R8000 Blackbird/TFP");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion FPC");
		break;
	case MIPS_R4700:
		printf("QED R4700 Orion FPC");
		break;
	case MIPS_R3TOSH:
		printf("Toshiba R3000 based FPC");
		break;
	case MIPS_R5000:
		printf("MIPS R5000 based FPC");
		break;
	case MIPS_RM52X0:
		printf("QED RM52X0 FPC");
		break;
	case MIPS_UNKF1:
	default:
		printf("Unknown FPU type (0x%x)", fpu_id.cpu.cp_imp);
		break;
	}
	printf(" Rev. %d.%d", fpu_id.cpu.cp_majrev, fpu_id.cpu.cp_minrev);
	printf("\n");

	printf("\tL1 Cache I size %dkb(%d line),",
		CpuPrimaryInstCacheSize / 1024,
		CpuPrimaryInstCacheLSize);
	printf(" D size %dkb(%d line), ",
		CpuPrimaryDataCacheSize / 1024,
		CpuPrimaryDataCacheLSize);
	if(CpuTwoWayCache)
		printf("two way.\n");
	else
		printf("direct mapped.\n");

	printf("\t%s snoop uncached cpu accesses.\n", l2cache_is_snooping ?
		"No L2 cache or L2 cache" : "L2 cache doesn't");
}
