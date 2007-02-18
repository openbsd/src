/* $OpenBSD: vesabios.c,v 1.4 2007/02/18 19:19:02 gwk Exp $ */

/*
 * Copyright (c) 2002, 2004
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <machine/frame.h>
#include <machine/kvm86.h>

#include <dev/vesa/vesabiosvar.h>
#include <dev/vesa/vesabiosreg.h>
#include <dev/vesa/vbe.h>

struct vbeinfoblock
{
	char VbeSignature[4];
	uint16_t VbeVersion;
	uint32_t OemStringPtr;
	uint32_t Capabilities;
	uint32_t VideoModePtr;
	uint16_t TotalMemory;
	uint16_t OemSoftwareRev;
	uint32_t OemVendorNamePtr, OemProductNamePtr, OemProductRevPtr;
	/* data area, in total max 512 bytes for VBE 2.0 */
} __packed;

#define FAR2FLATPTR(p) ((p & 0xffff) + ((p >> 12) & 0xffff0))

int vesabios_match(struct device *, void *, void *);
void vesabios_attach(struct device *, struct device *, void *);
int vesabios_print(void *, const char *);

int vbegetinfo(struct vbeinfoblock **);
void vbefreeinfo(struct vbeinfoblock *);
#ifdef VESABIOSVERBOSE
const char *mm2txt(unsigned int);
#endif

struct vesabios_softc *vesabios_softc;

struct cfattach vesabios_ca = {
	sizeof(struct vesabios_softc), vesabios_match, vesabios_attach
};

struct cfdriver vesabios_cd = {
	NULL, "vesabios", DV_DULL
};

int
vesabios_match(struct device *parent, void *match, void *aux)
{

	return (1);
}

int
vbegetinfo(struct vbeinfoblock **vip)
{
	unsigned char *buf;
	struct trapframe tf;
	int res, error;

	if ((buf = kvm86_bios_addpage(KVM86_CALL_TASKVA)) == NULL) {
		printf("vbegetinfo: kvm86_bios_addpage failed\n");
		return (ENOMEM);
	}

	memcpy(buf, "VBE2", 4);

	memset(&tf, 0, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_CTRLINFO;
	tf.tf_vm86_es = 0;
	tf.tf_edi = KVM86_CALL_TASKVA;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED) {
		printf("vbecall: res=%d, ax=%x\n", res, tf.tf_eax);
		error = ENXIO;
		goto out;
	}

	if (memcmp(((struct vbeinfoblock *)buf)->VbeSignature, "VESA", 4)) {
		error = EIO;
		goto out;
	}

	if (vip)
		*vip = (struct vbeinfoblock *)buf;
	return (0);

out:
	kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
	return (error);
}

void
vbefreeinfo(struct vbeinfoblock *vip)
{

	kvm86_bios_delpage(KVM86_CALL_TASKVA, vip);
}

int
vbeprobe(void)
{
	struct vbeinfoblock *vi;
	int found = 0;

	if (vbegetinfo(&vi))
		return (0);
	if (VBE_CTRLINFO_VERSION(vi->VbeVersion) > 1) {
		/* VESA bios is at least version 2.0 */
		found = 1;
	}
	vbefreeinfo(vi);
	return (found);
}

#ifdef VESABIOSVERBOSE
const char *
mm2txt(unsigned int mm)
{
	static char buf[30];
	static const char *names[] = {
		"Text mode",
		"CGA graphics",
		"Hercules graphics",
		"Planar",
		"Packed pixel",
		"Non-chain 4, 256 color",
		"Direct Color",
		"YUV"
	};

	if (mm < sizeof(names)/sizeof(names[0]))
		return (names[mm]);
	snprintf(buf, sizeof(buf), "unknown memory model %d", mm);
	return (buf);
}
#endif

void
vesabios_attach(struct device *parent, struct device *self, void *aux)
{
	struct vesabios_softc *sc = (struct vesabios_softc *)self;
	struct vbeinfoblock *vi;
	unsigned char *buf;
	struct trapframe tf;
	int res;
	char name[256];
#define MAXMODES 60
	uint16_t modes[MAXMODES];
	int rastermodes[MAXMODES];
	int textmodes[MAXMODES];
	int nmodes, nrastermodes, ntextmodes, i;
	uint32_t modeptr;
	struct modeinfoblock *mi;

	if (vbegetinfo(&vi)) {
		printf("\n");
		panic("vesabios_attach: disappeared");
	}

	printf(": version %d.%d",
	    VBE_CTRLINFO_VERSION(vi->VbeVersion),
	    VBE_CTRLINFO_REVISION(vi->VbeVersion));


	res = kvm86_bios_read(FAR2FLATPTR(vi->OemVendorNamePtr),
	    name, sizeof(name));

	sc->sc_size = vi->TotalMemory * 65536;
	if (res > 0) {
		name[res - 1] = 0;
		printf(", %s", name);
		res = kvm86_bios_read(FAR2FLATPTR(vi->OemProductNamePtr),
				      name, sizeof(name));
		if (res > 0) {
			name[res - 1] = 0;
			printf(" %s", name);
		}
	}
	printf("\n");

	nmodes = 0;
	modeptr = FAR2FLATPTR(vi->VideoModePtr);
	while (nmodes < MAXMODES) {
		res = kvm86_bios_read(modeptr, (char *)&modes[nmodes], 2);
		if (res != 2 || modes[nmodes] == 0xffff)
			break;
		nmodes++;
		modeptr += 2;
	}

	vbefreeinfo(vi);
	if (nmodes == 0)
		return;

	nrastermodes = ntextmodes = 0;

	buf = kvm86_bios_addpage(KVM86_CALL_TASKVA);
	if (!buf) {
		printf("%s: kvm86_bios_addpage failed\n",
		    self->dv_xname);
		return;
	}
	for (i = 0; i < nmodes; i++) {

		memset(&tf, 0, sizeof(struct trapframe));
		tf.tf_eax = VBE_FUNC_MODEINFO;
		tf.tf_ecx = modes[i];
		tf.tf_vm86_es = 0;
		tf.tf_edi = KVM86_CALL_TASKVA;

		res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
		if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED) {
			printf("%s: vbecall: res=%d, ax=%x\n",
			    self->dv_xname, res, tf.tf_eax);
			printf("%s: error getting info for mode %04x\n",
			    self->dv_xname, modes[i]);
			continue;
		}
		mi = (struct modeinfoblock *)buf;
#ifdef VESABIOSVERBOSE
		printf("%s: VESA mode %04x: attributes %04x",
		       self->dv_xname, modes[i], mi->ModeAttributes);
#endif
		if (!(mi->ModeAttributes & 1)) {
#ifdef VESABIOSVERBOSE
			printf("\n");
#endif
			continue;
		}
		if (mi->ModeAttributes & 0x10) {
			/* graphics */
#ifdef VESABIOSVERBOSE
			printf(", %dx%d %dbbp %s\n",
			       mi->XResolution, mi->YResolution,
			       mi->BitsPerPixel, mm2txt(mi->MemoryModel));
#endif
			if (mi->ModeAttributes & 0x80) {
				/* flat buffer */
				rastermodes[nrastermodes++] = modes[i];
			}
		} else {
			/* text */
#ifdef VESABIOSVERBOSE
			printf(", text %dx%d\n",
			       mi->XResolution, mi->YResolution);
#endif
			if (!(mi->ModeAttributes & 0x20)) /* VGA compatible */
				textmodes[ntextmodes++] = modes[i];
		}
	}
	kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
	if (nrastermodes > 0) {
		sc->sc_modes =
		    (uint16_t *)malloc(sizeof(uint16_t)*nrastermodes,
			M_DEVBUF, M_NOWAIT);
		if (sc->sc_modes == NULL) {
			sc->sc_nmodes = 0;
			return;
		}
		sc->sc_nmodes = nrastermodes;
		for (i = 0; i < nrastermodes; i++)
			sc->sc_modes[i] = rastermodes[i];
	}
	vesabios_softc = sc;
}

int
vesabios_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	char *busname = aux;

	if (pnp)
		printf("%s at %s", busname, pnp);
	return (UNCONF);
}
