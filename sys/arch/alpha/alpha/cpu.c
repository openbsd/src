/*	$OpenBSD: cpu.c,v 1.6 1997/01/24 19:56:20 niklas Exp $	*/
/*	$NetBSD: cpu.c,v 1.16 1996/12/05 01:39:27 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

/* Definition of the driver for autoconfig. */
#ifdef __BROKEN_INDIRECT_CONFIG
int	cpumatch(struct device *, void *, void *);
#else
int	cpumatch(struct device *, struct cfdata *, void *);
#endif
void	cpuattach __P((struct device *, struct device *, void *));

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

int
cpumatch(parent, cfdata, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *cfdata;
#else
	struct cfdata *cfdata;
#endif
	void *aux;
{
	struct confargs *ca = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(ca->ca_name, cpu_cd.cd_name) != 0)
		return (0);

	return (1);
}

void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
        struct pcs *p;
	int needcomma;
	u_int32_t major, minor;

        p = (struct pcs*)((char *)hwrpb + hwrpb->rpb_pcs_off +
	    (dev->dv_unit * hwrpb->rpb_pcs_size));
	major = (p->pcs_proc_type & PCS_PROC_MAJOR) >> PCS_PROC_MAJORSHIFT;
	minor = (p->pcs_proc_type & PCS_PROC_MINOR) >> PCS_PROC_MINORSHIFT;

	printf(": ");
	switch (major) {
	case PCS_PROC_EV3:
		printf("EV3 (minor type 0x%x)", minor);
		break;

	case PCS_PROC_EV4:
		printf("21064 ");
		switch (minor) {
		case 0:
			printf("(pass 2 or 2.1)");
			break;
		case 1:
			printf("(pass 3)");
			break;
		default:
			printf("(unknown minor type 0x%x)", minor);
			break;
		}
		break;

	case PCS_PROC_SIMULATION:
		printf("simulation (minor type 0x%x)", minor);
		break;

	case PCS_PROC_LCA4:
		switch (minor) {
		case 0:
			printf("LCA family (reserved minor type)");
			break;
		case 1:
			printf("21066 (pass 1 or 1.1)");
			break;
		case 2:
			printf("21066 (pass 2)");
			break;
		case 3:
			printf("21068 (pass 1 or 1.1)");
			break;
		case 4:
			printf("21068 (pass 2)");
			break;
		case 5:
			printf("21066A (pass 1)");
			break;
		case 6:
			printf("21068A (pass 1)");
			break;
		default:
			printf("LCA family (unknown minor type 0x%x)", minor);
			break;
		}
		break;

	case PCS_PROC_EV5:
		printf("21164 ");
		switch (minor) {
		case 0:
			printf("(reserved minor type/pass 1)");
			break;
		case 1:
			printf("(pass 2 or 2.2)");
			break;
		case 2:
			printf("(pass 2.3)");
			break;
		case 3:
			printf("(pass 3)");
			break;
		case 4:
			printf("(pass 3.2)");
			break;
		case 5:
			printf("(pass 4)");
			break;
		default:
			printf("(unknown minor type 0x%x)", minor);
			break;
		}
		break;

	case PCS_PROC_EV45:
		printf("21064A ");
		switch (minor) {
		case 0:
			printf("(reserved minor type)");
			break;
		case 1:
			printf("(pass 1)");
			break;
		case 2:
			printf("(pass 1.1)");
			break;
		case 3:
			printf("(pass 2)");
			break;
		default:
			printf("(unknown minor type 0x%x)", minor);
			break;
		}
		break;

	case PCS_PROC_EV56:
		printf("21164A ");
		switch (minor) {
		case 0:
			printf("(reserved minor type)");
			break;
		case 1:
			printf("(pass 1)");
			break;
		case 2:
			printf("(pass 2)");
			break;
		default:
			printf("(unknown minor type 0x%x)", minor);
			break;
		}
		break;

	case PCS_PROC_EV6:
		printf("21264 ");
		switch (minor) {
		case 0:
			printf("(reserved minor type)");
			break;
		case 1:
			printf("(pass 1)");
			break;
		default:
			printf("(unknown minor type 0x%x)", minor);
			break;
		}
		break;

	case PCS_PROC_PCA56:
		printf("21164PC ");
		switch (minor) {
		case 0:
			printf("(reserved minor type)");
			break;
		case 1:
			printf("(pass 1)");
			break;
		default:
			printf("(unknown minor type 0x%x)", minor);
			break;
		}
		break;

	default:
		printf("UNKNOWN CPU TYPE (0x%x:0x%x)", major, minor);
		break;
	}
	printf("\n");

	/* XXX SHOULD CHECK ARCHITECTURE MASK, TOO */
	if (p->pcs_proc_var != 0) {
		printf("cpu%d: ", dev->dv_unit);

		needcomma = 0;
		if (p->pcs_proc_var & PCS_VAR_VAXFP) {
			printf("VAX FP support");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_IEEEFP) {
			printf("%sIEEE FP support", needcomma ? ", " : "");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_PE) {
			printf("%sPrimary Eligible", needcomma ? ", " : "");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_RESERVED)
			printf("%sreserved bits: 0x%lx", needcomma ? ", " : "",
			    p->pcs_proc_var & PCS_VAR_RESERVED);
		printf("\n");
	}

	/*
	 * Though we could (should?) attach the LCA cpus' PCI
	 * bus here there is no good reason to do so, and
	 * the bus attachment code is easier to understand
	 * and more compact if done the 'normal' way.
	 */
}
