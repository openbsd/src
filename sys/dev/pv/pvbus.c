/*	$OpenBSD: pvbus.c,v 1.5 2015/07/29 17:08:46 mikeb Exp $	*/

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
int	 pvbus_search(struct device *, void *, void *);

void	 pvbus_kvm(struct pvbus_softc *, struct pvbus_hv *);
void	 pvbus_hyperv(struct pvbus_softc *, struct pvbus_hv *);
void	 pvbus_xen(struct pvbus_softc *, struct pvbus_hv *);

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
	void		(*init)(struct pvbus_softc *, struct pvbus_hv *);
} pvbus_types[PVBUS_MAX] = {
	{ "KVMKVMKVM\0\0\0",	"KVM",		pvbus_kvm },
	{ "Microsoft Hv",	"Hyper-V",	pvbus_hyperv },
	{ "VMwareVMware",	"VMware",	NULL },
	{ "XenVMMXenVMM",	"Xen",		pvbus_xen },
	{ "bhyve bhyve ",	"bhyve",	NULL }
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
	struct pvbus_hv *hv;
	uint32_t reg0, base;
	union {
		uint32_t	regs[3];
		char		str[CPUID_HV_SIGNATURE_STRLEN];
	} r;
	int i, cnt;

	printf(":");

	for (base = CPUID_HV_SIGNATURE_START, cnt = 0;
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

		for (i = 0; i < PVBUS_MAX; i++) {
			if (memcmp(pvbus_types[i].signature, r.str,
			    CPUID_HV_SIGNATURE_STRLEN) != 0)
				continue;
			hv = &sc->pvbus_hv[i];
			hv->hv_base = base;

			if (cnt++)
				printf(",");
			printf(" %s", pvbus_types[i].name);
			if (pvbus_types[i].init != NULL)
				(pvbus_types[i].init)(sc, hv);
		}
	}

 out:
	printf("\n");
	config_search(pvbus_search, self, sc);
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
pvbus_search(struct device *parent, void *arg, void *aux)
{
	struct pvbus_softc *sc = (struct pvbus_softc *)aux;
	struct cfdata		*cf = arg;
	struct pv_attach_args	 pva;

	pva.pva_busname = cf->cf_driver->cd_name;
	pva.pva_hv = sc->pvbus_hv;

	if (cf->cf_attach->ca_match(parent, cf, &pva) > 0)
		config_attach(parent, cf, &pva, pvbus_print);

	return (0);
}

int
pvbus_print(void *aux, const char *pnp)
{
	struct pv_attach_args	*pva = aux;
	if (pnp)
		printf("%s at %s", pva->pva_busname, pnp);
	return (UNCONF);
}

void
pvbus_kvm(struct pvbus_softc *sc, struct pvbus_hv *hv)
{
	uint32_t regs[4];

	CPUID(hv->hv_base + CPUID_OFFSET_KVM_FEATURES,
	    regs[0], regs[1], regs[2], regs[3]);
	hv->hv_features = regs[0];
}

void
pvbus_hyperv(struct pvbus_softc *sc, struct pvbus_hv *hv)
{
	uint32_t regs[4];

	CPUID(hv->hv_base + CPUID_OFFSET_HYPERV_FEATURES,
	    regs[0], regs[1], regs[2], regs[3]);
	hv->hv_features = regs[0];

	CPUID(hv->hv_base + CPUID_OFFSET_HYPERV_VERSION,
	    regs[0], regs[1], regs[2], regs[3]);
	hv->hv_version = regs[1];

	printf(" %u.%u.%u",
	    (regs[1] & HYPERV_VERSION_EBX_MAJOR_M) >>
	    HYPERV_VERSION_EBX_MAJOR_S,
	    (regs[1] & HYPERV_VERSION_EBX_MINOR_M) >>
	    HYPERV_VERSION_EBX_MINOR_S,
	    regs[0]);
}

void
pvbus_xen(struct pvbus_softc *sc, struct pvbus_hv *hv)
{
	uint32_t regs[4];

	CPUID(hv->hv_base + CPUID_OFFSET_XEN_VERSION,
	    regs[0], regs[1], regs[2], regs[3]);
	hv->hv_version = regs[0];

	printf(" %u.%u", regs[0] >> XEN_VERSION_MAJOR_S,
	    regs[0] & XEN_VERSION_MINOR_M);
}
