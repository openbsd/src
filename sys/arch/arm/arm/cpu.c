/*	$OpenBSD: cpu.c,v 1.47 2018/08/06 18:39:13 kettenis Exp $	*/
/*	$NetBSD: cpu.c,v 1.56 2004/04/14 04:01:49 bsh Exp $	*/


/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
 * All rights reserved.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpu.c
 *
 * Probing and configuration for the master CPU
 *
 * Created      : 10/10/95
 */

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/sched.h>
#include <sys/sysctl.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/fdt.h>

#include <arm/cpuconf.h>
#include <arm/undefined.h>
#include <arm/vfp.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

char cpu_model[256];
int cpu_node;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

void	identify_arm_cpu(struct device *, struct cpu_info *);
int	cpu_clockspeed(int *);

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	char buf[32];

	if (OF_getprop(faa->fa_node, "device_type", buf, sizeof(buf)) > 0 &&
	    strcmp(buf, "cpu") == 0)
		return 1;

	return 0;
}

void
cpu_attach(struct device *parent, struct device *dev, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct cpu_info *ci;

	if (dev->dv_unit == 0) {
		ci = curcpu();
		ci->ci_dev = dev;

		/* Get the CPU ID from coprocessor 15 */
		ci->ci_arm_cpuid = cpu_id();
		ci->ci_arm_cputype =
		    ci->ci_arm_cpuid & CPU_ID_CPU_MASK;
		ci->ci_arm_cpurev =
		    ci->ci_arm_cpuid & CPU_ID_REVISION_MASK;

		identify_arm_cpu(dev, ci);

		vfp_init();

		if (OF_getproplen(faa->fa_node, "clocks") > 0) {
			cpu_node = faa->fa_node;
			cpu_cpuspeed = cpu_clockspeed;
		}
	} else {
		printf(": not configured");
	}
}

enum cpu_class {
	CPU_CLASS_NONE,
	CPU_CLASS_ARMv7,
	CPU_CLASS_ARMv8
};

struct cpuidtab {
	u_int32_t	cpuid;
	enum		cpu_class cpu_class;
	const char	*cpu_name;
};

const struct cpuidtab cpuids[] = {
	{ CPU_ID_CORTEX_A5,	CPU_CLASS_ARMv7,	"ARM Cortex-A5" },
	{ CPU_ID_CORTEX_A7,	CPU_CLASS_ARMv7,	"ARM Cortex-A7" },
	{ CPU_ID_CORTEX_A8,	CPU_CLASS_ARMv7,	"ARM Cortex-A8" },
	{ CPU_ID_CORTEX_A9,	CPU_CLASS_ARMv7,	"ARM Cortex-A9" },
	{ CPU_ID_CORTEX_A12,	CPU_CLASS_ARMv7,	"ARM Cortex-A12" },
	{ CPU_ID_CORTEX_A15,	CPU_CLASS_ARMv7,	"ARM Cortex-A15" },
	{ CPU_ID_CORTEX_A17,	CPU_CLASS_ARMv7,	"ARM Cortex-A17" },

	{ CPU_ID_CORTEX_A32,	CPU_CLASS_ARMv8,	"ARM Cortex-A32" },
	{ CPU_ID_CORTEX_A35,	CPU_CLASS_ARMv8,	"ARM Cortex-A35" },
	{ CPU_ID_CORTEX_A53,	CPU_CLASS_ARMv8,	"ARM Cortex-A53" },
	{ CPU_ID_CORTEX_A55,	CPU_CLASS_ARMv8,	"ARM Cortex-A55" },
	{ CPU_ID_CORTEX_A57,	CPU_CLASS_ARMv8,	"ARM Cortex-A57" },
	{ CPU_ID_CORTEX_A72,	CPU_CLASS_ARMv8,	"ARM Cortex-A72" },
	{ CPU_ID_CORTEX_A73,	CPU_CLASS_ARMv8,	"ARM Cortex-A73" },
	{ CPU_ID_CORTEX_A75,	CPU_CLASS_ARMv8,	"ARM Cortex-A75" },

	{ 0, CPU_CLASS_NONE, NULL }
};

struct cpu_classtab {
	const char	*class_name;
	const char	*class_option;
};

const char *cpu_classes[] = {
	"unknown",		/* CPU_CLASS_NONE */
	"ARMv7",		/* CPU_CLASS_ARMv7 */
	"ARMv8"			/* CPU_CLASS_ARMv8 */
};

/*
 * Report the type of the specified arm processor. This uses the generic and
 * arm specific information in the cpu structure to identify the processor.
 * The remaining fields in the cpu structure are filled in appropriately.
 */

static const char * const wtnames[] = {
	"wr-thru",
	"wr-back",
	"wr-back",
	"**unknown 3**",
	"**unknown 4**",
	"wr-back-lock",		/* XXX XScale-specific? */
	"wr-back-lock-A",
	"wr-back-lock-B",
	"**unknown 8**",
	"**unknown 9**",
	"**unknown 10**",
	"**unknown 11**",
	"**unknown 12**",
	"**unknown 13**",
	"**unknown 14**",
	"**unknown 15**"
};

void
identify_arm_cpu(struct device *dv, struct cpu_info *ci)
{
	u_int cpuid;
	enum cpu_class cpu_class = CPU_CLASS_NONE;
	int i;

	cpuid = ci->ci_arm_cpuid;

	if (cpuid == 0) {
		printf("Processor failed probe - no CPU ID\n");
		return;
	}

	for (i = 0; cpuids[i].cpuid != 0; i++)
		if (cpuids[i].cpuid == (cpuid & CPU_ID_CORTEX_MASK)) {
			cpu_class = cpuids[i].cpu_class;
			snprintf(cpu_model, sizeof(cpu_model),
			    "%s r%dp%d (%s)", cpuids[i].cpu_name,
			    (cpuid & CPU_ID_VARIANT_MASK) >> 20,
			    cpuid & CPU_ID_REVISION_MASK,
			    cpu_classes[cpu_class]);
			break;
		}

	if (cpuids[i].cpuid == 0)
		snprintf(cpu_model, sizeof(cpu_model),
		    "unknown CPU (ID = 0x%x)", cpuid);

	printf(": %s\n", cpu_model);

	printf("%s:", dv->dv_xname);

	switch (cpu_class) {
	case CPU_CLASS_ARMv7:
	case CPU_CLASS_ARMv8:
		if ((ci->ci_ctrl & CPU_CONTROL_DC_ENABLE) == 0)
			printf(" DC disabled");
		else
			printf(" DC enabled");
		if ((ci->ci_ctrl & CPU_CONTROL_IC_ENABLE) == 0)
			printf(" IC disabled");
		else
			printf(" IC enabled");
		break;
	default:
		break;
	}
	if ((ci->ci_ctrl & CPU_CONTROL_WBUF_ENABLE) == 0)
		printf(" WB disabled");
	else
		printf(" WB enabled");

	if (ci->ci_ctrl & CPU_CONTROL_LABT_ENABLE)
		printf(" LABT");
	else
		printf(" EABT");

	if (ci->ci_ctrl & CPU_CONTROL_BPRD_ENABLE)
		printf(" branch prediction enabled");

	printf("\n");

	/*
	 * Some ARM processors are vulnerable to branch target
	 * injection attacks.
	 */
	switch (cpuid & CPU_ID_CORTEX_MASK) {
	case CPU_ID_CORTEX_A5:
	case CPU_ID_CORTEX_A7:
	case CPU_ID_CORTEX_A32:
	case CPU_ID_CORTEX_A35:
	case CPU_ID_CORTEX_A53:
	case CPU_ID_CORTEX_A55:
		/* Not vulnerable; no need to flush. */
		ci->ci_flush_bp = cpufunc_nullop;
		break;
	case CPU_ID_CORTEX_A8:
	case CPU_ID_CORTEX_A9:
	case CPU_ID_CORTEX_A12:
	case CPU_ID_CORTEX_A17:
	case CPU_ID_CORTEX_A73:
	case CPU_ID_CORTEX_A75:
	default:
		/* Vulnerable; flush BP cache. */
		ci->ci_flush_bp = armv7_flush_bp;
		break;
	case CPU_ID_CORTEX_A15:
	case CPU_ID_CORTEX_A72:
		/*
		 * Vulnerable; BPIALL is "not effective" so must use
		 * ICIALLU and hope the firmware set the magic bit in
		 * the ACTLR that actually forces a BTB flush.
		 */
		ci->ci_flush_bp = cortex_a15_flush_bp;
		break;
	case CPU_ID_CORTEX_A57:
		/*
		 * Vulnerable; must disable and enable the MMU which
		 * can be done by a PSCI call on firmware with the
		 * appropriate fixes.  Punt for now.
		 */
		ci->ci_flush_bp = cpufunc_nullop;
		break;
	}

	/* Print cache info. */
	if (arm_picache_line_size == 0 && arm_pdcache_line_size == 0)
		goto skip_pcache;

	if (arm_pcache_unified) {
		printf("%s: %dKB/%dB %d-way %s unified cache\n",
		    dv->dv_xname, arm_pdcache_size / 1024,
		    arm_pdcache_line_size, arm_pdcache_ways,
		    wtnames[arm_pcache_type]);
	} else {
		printf("%s: %dKB(%db/l,%dway) I-cache, %dKB(%db/l,%dway) %s D-cache\n",
		    dv->dv_xname, arm_picache_size / 1024,
		    arm_picache_line_size, arm_picache_ways,
		    arm_pdcache_size / 1024, arm_pdcache_line_size, 
		    arm_pdcache_ways, wtnames[arm_pcache_type]);
	}

 skip_pcache:

	switch (cpu_class) {
	case CPU_CLASS_ARMv7:
	case CPU_CLASS_ARMv8:
		break;
	default:
		printf("%s: %s does not fully support this CPU."
		       "\n", dv->dv_xname, ostype);
		break;
	}
}

int
cpu_clockspeed(int *freq)
{
	*freq = clock_get_frequency(cpu_node, NULL) / 1000000;
	return 0;
}

#ifdef MULTIPROCESSOR

void
cpu_boot_secondary_processors(void)
{
}

int
cpu_alloc_idle_pcb(struct cpu_info *ci)
{
	vaddr_t uaddr;
	struct pcb *pcb;
	struct trapframe *tf;

	/*
	 * Generate a kernel stack and PCB (in essence, a u-area) for the
	 * new CPU.
	 */
	uaddr = (vaddr_t)km_alloc(USPACE, &kv_any, &kp_zero, &kd_nowait);
	if (uaddr == 0) {
		printf("%s: unable to allocate idle stack\n",
		    __func__);
		return ENOMEM;
	}
	ci->ci_idle_pcb = pcb = (struct pcb *)uaddr;

	/*
	 * This code is largely derived from cpu_fork(), with which it
	 * should perhaps be shared.
	 */

	/* Copy the pcb */
	*pcb = proc0.p_addr->u_pcb;

	/* Set up the undefined stack for the process. */
	pcb->pcb_un.un_32.pcb32_und_sp = uaddr + USPACE_UNDEF_STACK_TOP;
	pcb->pcb_un.un_32.pcb32_sp = uaddr + USPACE_SVC_STACK_TOP;

	pcb->pcb_tf = tf =
	    (struct trapframe *)pcb->pcb_un.un_32.pcb32_sp - 1;
	*tf = *proc0.p_addr->u_pcb.pcb_tf;
	return 0;
}
#endif /* MULTIPROCESSOR */

void
intr_barrier(void *ih)
{
	sched_barrier(NULL);
}
