/*	$OpenBSD: cpu.c,v 1.5 2003/02/11 19:20:26 mickey Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
 * Copyright (c) 1997 RTMX Inc
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
 *	This product includes software developed under OpenBSD for RTMX Inc
 *	North Carolina, USA, by Per Fogelstrom, Opsycon AB, Sweden.
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

#include <machine/autoconf.h>

char cpu_model[80];
char machine[] = MACHINE;	/* cpu architecture */

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

void config_l2cr(void);

static void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	int cpu, pvr;
#ifdef OFW
	char name[32];
	int qhandle, phandle;
	unsigned int clock_freq = 0;
#endif

	__asm__ ("mfpvr %0" : "=r"(pvr));
	cpu = pvr >> 16;
	switch (cpu) {
	case 1:
		sprintf(cpu_model, "601");
		break;
	case 3:
		sprintf(cpu_model, "603");
		break;
	case 4:
		sprintf(cpu_model, "604");
		break;
	case 5:
		sprintf(cpu_model, "602");
		break;
	case 6:
		sprintf(cpu_model, "603e");
		break;
	case 7:
		sprintf(cpu_model, "603ev");
		break;
	case 8:
		sprintf(cpu_model, "750");
		break;
	case 9:
		sprintf(cpu_model, "604ev");
		break;
	case 12:
		sprintf(cpu_model, "7400(G4)");
		break;
	case 20:
		sprintf(cpu_model, "620");
		break;
	default:
		sprintf(cpu_model, "Version %x", cpu);
		break;
	}
	sprintf(cpu_model + strlen(cpu_model), " (Revision %x)", pvr & 0xffff);
	printf(": %s", cpu_model);

	/* This should only be executed on openfirmware systems... */
#ifdef OFW
	for (qhandle = OF_peer(0); qhandle; qhandle = phandle) {
                if (OF_getprop(qhandle, "device_type", name, sizeof name) >= 0
                    && !strcmp(name, "cpu")
                    && OF_getprop(qhandle, "clock-frequency",
                                  &clock_freq , sizeof clock_freq ) >= 0)
		{
			break;
		}
                if (phandle = OF_child(qhandle))
                        continue;
                while (qhandle) {
                        if (phandle = OF_peer(qhandle))
                                break;
                        qhandle = OF_parent(qhandle);
                }
	}

	if (clock_freq != 0) {
		/* Openfirmware stores clock in HZ, not MHz */
		clock_freq /= 1000000;
		printf(": %d MHz", clock_freq);

	}
#endif
	/* if processor is G3 or G4, configure l2 cache */ 
	if  ( (cpu == 8) || (cpu == 12) ) {
		config_l2cr();
	}
	printf("\n");


}

#define L2CR 1017

#define L2CR_L2E        0x80000000 /* 0: L2 enable */
#define L2CR_L2PE       0x40000000 /* 1: L2 data parity enable */
#define L2CR_L2SIZ      0x30000000 /* 2-3: L2 size */
#define  L2SIZ_RESERVED         0x00000000
#define  L2SIZ_256K             0x10000000
#define  L2SIZ_512K             0x20000000
#define  L2SIZ_1M       0x30000000
#define L2CR_L2CLK      0x0e000000 /* 4-6: L2 clock ratio */
#define  L2CLK_DIS              0x00000000 /* disable L2 clock */
#define  L2CLK_10               0x02000000 /* core clock / 1   */
#define  L2CLK_15               0x04000000 /*            / 1.5 */
#define  L2CLK_20               0x08000000 /*            / 2   */
#define  L2CLK_25               0x0a000000 /*            / 2.5 */
#define  L2CLK_30               0x0c000000 /*            / 3   */
#define L2CR_L2RAM      0x01800000 /* 7-8: L2 RAM type */
#define  L2RAM_FLOWTHRU_BURST   0x00000000
#define  L2RAM_PIPELINE_BURST   0x01000000
#define  L2RAM_PIPELINE_LATE    0x01800000
#define L2CR_L2DO       0x00400000 /* 9: L2 data-only.
                                      Setting this bit disables instruction
                                      caching. */
#define L2CR_L2I        0x00200000 /* 10: L2 global invalidate. */
#define L2CR_L2CTL      0x00100000 /* 11: L2 RAM control (ZZ enable).
                                      Enables automatic operation of the
                                      L2ZZ (low-power mode) signal. */
#define L2CR_L2WT       0x00080000 /* 12: L2 write-through. */
#define L2CR_L2TS       0x00040000 /* 13: L2 test support. */
#define L2CR_L2OH       0x00030000 /* 14-15: L2 output hold. */
#define L2CR_L2SL       0x00008000 /* 16: L2 DLL slow. */
#define L2CR_L2DF       0x00004000 /* 17: L2 differential clock. */
#define L2CR_L2BYP      0x00002000 /* 18: L2 DLL bypass. */
#define L2CR_L2IP       0x00000001 /* 31: L2 global invalidate in progress
                                      (read only). */
#ifdef L2CR_CONFIG
u_int l2cr_config = L2CR_CONFIG;
#else
u_int l2cr_config = 0;
#endif

void
config_l2cr()
{
	u_int l2cr, x;

	__asm __volatile ("mfspr %0, 1017" : "=r"(l2cr));

	/*
	 * Configure L2 cache if not enabled.
	 */
	if ((l2cr & L2CR_L2E) == 0 && l2cr_config != 0) {
		l2cr = l2cr_config;
		asm volatile ("mtspr 1017,%0" :: "r"(l2cr));

		/* Wait for L2 clock to be stable (640 L2 clocks). */
		delay(100);

		/* Invalidate all L2 contents. */
		l2cr |= L2CR_L2I;
		asm volatile ("mtspr 1017,%0" :: "r"(l2cr));
		do {
			asm volatile ("mfspr %0, 1017" : "=r"(x));
		} while (x & L2CR_L2IP);
				      
		/* Enable L2 cache. */
		l2cr &= ~L2CR_L2I;
		l2cr |= L2CR_L2E;
		asm volatile ("mtspr 1017,%0" :: "r"(l2cr));
	}

	if (l2cr & L2CR_L2E) {
		switch (l2cr & L2CR_L2SIZ) {
		case L2SIZ_256K:
			printf(": 256KB");
			break;
		case L2SIZ_512K:
			printf(": 512KB");
			break;
		case L2SIZ_1M:  
			printf(": 1MB");
			break;
		default:
			printf(": unknown size");
		}
#if 0
		switch (l2cr & L2CR_L2RAM) {
		case L2RAM_FLOWTHRU_BURST:
			printf(" Flow-through synchronous burst SRAM");
			break;
		case L2RAM_PIPELINE_BURST:
			printf(" Pipelined synchronous burst SRAM");
			break;
		case L2RAM_PIPELINE_LATE:
			printf(" Pipelined synchronous late-write SRAM");
			break;
		default:
			printf(" unknown type");
		}
		
		if (l2cr & L2CR_L2PE)
			printf(" with parity");  
#endif
		printf(" backside cache");
	} else
		printf(": L2 cache not enabled");
		
}
