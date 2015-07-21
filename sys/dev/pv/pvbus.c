/*	$OpenBSD: pvbus.c,v 1.1 2015/07/21 03:38:22 reyk Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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

#if !defined(__i386__) && !defined(__amd64__)
#error pvbus(4) is currently only supported on i386 and amd64
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/socket.h>

#include <machine/specialreg.h>

#include <dev/rndvar.h>

#include <dev/pv/pvvar.h>
#include <dev/pv/pvreg.h>

#include "vmt.h"

int has_hv_cpuid = 0;

extern void rdrand(void *);

int	 pvbus_activate(struct device *, int);
int	 pvbus_match(struct device *, void *, void *);
void	 pvbus_attach(struct device *, struct device *, void *);
int	 pvbus_print(void *, const char *);

struct cfattach pvbus_ca = {
	sizeof(struct pvbus_softc),
	pvbus_match,
	pvbus_attach,
	NULL,
	pvbus_activate
};

struct cfdriver pvbus_cd = {
	NULL,
	"pvbus",
	DV_DULL
};

struct pvbus_type {
	const char	*signature;
	const char	*name;
	unsigned int	 type;
} pvbus_types[] = {
	{ "KVMKVMKVM\0\0\0",	"KVM",		PVBUS_KVM },
	{ "Microsoft Hv",	"Hyper-V",	PVBUS_HYPERV },
	{ "VMwareVMware",	"VMware",	PVBUS_VMWARE },
	{ "XenVMMXenVMM",	"Xen",		PVBUS_XEN },
	{ NULL }
};

struct pv_attach_args pvbus_devices[] = {
#if NVMT > 0
	{ "vmt",	PVBUS_VMWARE	},
#endif
	{ NULL }
};

int
pvbus_probe(void)
{
	/* Must be set in identcpu */
	if (!has_hv_cpuid)
		return (0);
	return (1);
}

int
pvbus_match(struct device *parent, void *match, void *aux)
{
	const char **busname = (const char **)aux;
	return (strcmp(*busname, pvbus_cd.cd_name) == 0);
}

void
pvbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct pvbus_softc *sc = (struct pvbus_softc *)self;
	struct pv_attach_args *pva;
	uint32_t reg0, base;
	union {
		uint32_t	regs[3];
		char		str[CPUID_HV_SIGNATURE_STRLEN];
	} r;
	int i;

	printf(":");

	for (base = CPUID_HV_SIGNATURE_START;
	    base < CPUID_HV_SIGNATURE_END;
	    base += CPUID_HV_SIGNATURE_STEP) {
		CPUID(base, reg0, r.regs[0], r.regs[1], r.regs[2]);
		for (i = 0; i < 4; i++) {
			/*
			 * Check if first 4 chars are printable ASCII as
			 * minimal validity check
			 */
			if (r.str[i] < 32 || r.str[i] > 126)
				goto out;
		}

		for (i = 0; pvbus_types[i].signature != NULL; i++) {
			if (memcmp(pvbus_types[i].signature, r.str,
			    CPUID_HV_SIGNATURE_STRLEN) != 0)
				continue;
			sc->pvbus_types |= pvbus_types[i].type;

			printf(" %s", pvbus_types[i].name);
		}
	}

 out:
	printf("\n");

#ifdef notyet
	/* XXX get hypervisor-specific features */
	if (sc->pvbus_types & PVBUS_KVM) {
		kvm_cpuid_base = base;
		CPUID(base + CPUID_OFFSET_KVM_FEATURES,
		    reg0, r.regs[0], r.regs[1], r.regs[2]);
		kvm_features = reg0;
	}
	if (sc->pvbus_types & PVBUS_HYPERV) {
		/* XXX */
	}
#endif

	/* Attach drivers */
	for (i = 0; pvbus_devices[i].pva_busname != NULL; i++) {
		pva = &pvbus_devices[i];
		pva->pva_types = sc->pvbus_types;
		if (sc->pvbus_types & pva->pva_type)
			config_found(self, &pva->pva_busname,
			    pvbus_print);
	}
}

int
pvbus_activate(struct device *self, int act)
{
	int rv = 0;

	switch (act) {
	case DVACT_SUSPEND:
		rv = config_activate_children(self, act);
		break;
	case DVACT_RESUME:
		rv = config_activate_children(self, act);
		break;
	case DVACT_POWERDOWN:
		rv = config_activate_children(self, act);
		break;
	default:
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

int
pvbus_print(void *aux, const char *pnp)
{
	struct pv_attach_args	*pva = aux;
	if (pnp)
		printf("%s at %s", pva->pva_busname, pnp);
	return (UNCONF);
}
