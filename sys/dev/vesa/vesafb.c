/*	$OpenBSD: vesafb.c,v 1.7 2009/11/11 00:01:34 fgsch Exp $	*/

/*-
 * Copyright (c) 2006 Jared D. McNeill <jmcneill@invisible.ca>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2006 Matthieu Herrb
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
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>

#include <dev/vesa/vesabiosreg.h>
#include <dev/vesa/vesabiosvar.h>
#include <dev/vesa/vbe.h>

#include <machine/frame.h>
#include <machine/kvm86.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <uvm/uvm_extern.h>


void vesafb_set_mode(struct vga_pci_softc *, int);
int vesafb_get_mode(struct vga_pci_softc *);
void vesafb_get_mode_info(struct vga_pci_softc *, int, struct modeinfoblock *);
void vesafb_set_palette(struct vga_pci_softc *, int, struct paletteentry *);
int vesafb_putcmap(struct vga_pci_softc *, struct wsdisplay_cmap *);
int vesafb_getcmap(struct vga_pci_softc *, struct wsdisplay_cmap *);
int vesafb_get_ddc_version(struct vga_pci_softc *);
int vesafb_get_ddc_info(struct vga_pci_softc *, struct edid *);

int
vesafb_get_ddc_version(struct vga_pci_softc *sc)
{
	struct trapframe tf;
	int res;

	bzero(&tf, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_DDC;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED)
		return 0;

	return VBECALL_SUCCESS(tf.tf_eax);
}


int
vesafb_get_ddc_info(struct vga_pci_softc *sc, struct edid *info)
{
	struct trapframe tf;
	unsigned char *buf;
	int res;

	if ((buf = kvm86_bios_addpage(KVM86_CALL_TASKVA)) == NULL) {
		printf("%s: kvm86_bios_addpage failed.\n",
		    sc->sc_dev.dv_xname);
		return 1;
	}

	bzero(&tf, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_DDC;
	tf.tf_ebx = VBE_DDC_GET;
	tf.tf_vm86_es = 0;
	tf.tf_edi = KVM86_CALL_TASKVA;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED) {
		kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
		return 1;
	}

	memcpy(info, buf, sizeof(struct edid));
	kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
	return VBECALL_SUCCESS(tf.tf_eax);
}

int
vesafb_get_mode(struct vga_pci_softc *sc)
{
	struct trapframe tf;
	int res;

	bzero(&tf, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_GETMODE;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED) {
		printf("%s: vbecall: res=%d, ax=%x\n",
		    sc->sc_dev.dv_xname, res, tf.tf_eax);
	}
	return tf.tf_ebx & 0xffff;
}

void
vesafb_get_mode_info(struct vga_pci_softc *sc, int mode,
    struct modeinfoblock *mi)
{
	struct trapframe tf;
	unsigned char *buf;
	int res;

	if ((buf = kvm86_bios_addpage(KVM86_CALL_TASKVA)) == NULL) {
		printf("%s: kvm86_bios_addpage failed.\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	memset(&tf, 0, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_MODEINFO;
	tf.tf_ecx = mode;
	tf.tf_vm86_es = 0;
	tf.tf_edi = KVM86_CALL_TASKVA;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED) {
		printf("%s: vbecall: res=%d, ax=%x\n",
		    sc->sc_dev.dv_xname, res, tf.tf_eax);
		printf("%s: error getting info for mode %05x\n",
		    sc->sc_dev.dv_xname, mode);
		kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
		return;
	}
	memcpy(mi, buf, sizeof(struct modeinfoblock));
	kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
}

void
vesafb_set_palette(struct vga_pci_softc *sc, int reg, struct paletteentry *pe)
{
	struct trapframe tf;
	int res;
	char *buf;

	if ((buf = kvm86_bios_addpage(KVM86_CALL_TASKVA)) == NULL) {
		printf("%s: kvm86_bios_addpage failed.\n",
		       sc->sc_dev.dv_xname);
		return;
	}

	memcpy(buf, pe, sizeof(struct paletteentry));

	/*
	 * this function takes 8 bit per palette as input, but we're
	 * working in 6 bit mode here
	 */
	pe = (struct paletteentry *)buf; 
	pe->Red >>= 2;
	pe->Green >>= 2;
	pe->Blue >>= 2;

	/* set palette */
	memset(&tf, 0, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_PALETTE;
	tf.tf_ebx = 0x0600; /* 6 bit per primary, set format */
	tf.tf_ecx = 1;
	tf.tf_edx = reg;
	tf.tf_vm86_es = 0;
	tf.tf_edi = KVM86_CALL_TASKVA;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED)
		printf("%s: vbecall: res=%d, ax=%x\n",
		    sc->sc_dev.dv_xname, res, tf.tf_eax);

	kvm86_bios_delpage(KVM86_CALL_TASKVA, buf);
	return;
}

void
vesafb_set_mode(struct vga_pci_softc *sc, int mode)
{
	struct trapframe tf;
	int res;

	bzero(&tf, sizeof(struct trapframe));
	tf.tf_eax = VBE_FUNC_SETMODE;
	tf.tf_ebx = mode;

	res = kvm86_bioscall(BIOS_VIDEO_INTR, &tf);
	if (res || VBECALL_SUPPORT(tf.tf_eax) != VBECALL_SUPPORTED) {
		printf("%s: vbecall: res=%d, ax=%x\n",
		    sc->sc_dev.dv_xname, res, tf.tf_eax);
		return;
	}
}

int
vesafb_find_mode(struct vga_pci_softc *sc, int width, int height, int bpp)
{
	struct modeinfoblock mi;
	int i;

	if (vesabios_softc == NULL || vesabios_softc->sc_nmodes == 0)
		return -1;
#ifdef VESABIOSVERBOSE
	printf("vesafb_find_mode %d %d %d\n", width, height, bpp);
#endif
	/* Choose a graphics mode */
	for (i = 0; i < vesabios_softc->sc_nmodes; i++) {
		vesafb_get_mode_info(sc, vesabios_softc->sc_modes[i], &mi);
		if (mi.XResolution == width &&
		    mi.YResolution == height &&
		    mi.BitsPerPixel == bpp) {
			sc->sc_width = mi.XResolution;
			sc->sc_height = mi.YResolution;
			sc->sc_depth = mi.BitsPerPixel;
			sc->sc_linebytes = mi.BytesPerScanLine;
			sc->sc_base = mi.PhysBasePtr;
			break;
		}
	}
	if (i == vesabios_softc->sc_nmodes)
		return -1;
	else
		return vesabios_softc->sc_modes[i] | 0x4000; /* flat */
}

int
vesafb_putcmap(struct vga_pci_softc *sc, struct wsdisplay_cmap *cm)
{
	struct paletteentry pe;
	u_int idx, cnt;
	u_char r[256], g[256], b[256];
	u_char *rp, *gp, *bp;
	int rv, i;

	idx = cm->index;
	cnt = cm->count;

	if (idx >= 256 || cnt > 256 - idx)
		return EINVAL;

	rv = copyin(cm->red, &r[idx], cnt);
	if (rv)
		return rv;
	rv = copyin(cm->green, &g[idx], cnt);
	if (rv)
		return rv;
	rv = copyin(cm->blue, &b[idx], cnt);
	if (rv)
		return rv;

	memcpy(&sc->sc_cmap_red[idx], &r[idx], cnt);
	memcpy(&sc->sc_cmap_green[idx], &g[idx], cnt);
	memcpy(&sc->sc_cmap_blue[idx], &b[idx], cnt);

	rp = &sc->sc_cmap_red[idx];
	gp = &sc->sc_cmap_green[idx];
	bp = &sc->sc_cmap_blue[idx];

	for (i = 0; i < cnt; i++) {
		pe.Blue = *bp;
		pe.Green = *gp;
		pe.Red = *rp;
		pe.Alignment = 0;
		vesafb_set_palette(sc, idx, &pe);
		idx++;
		rp++, gp++, bp++;
	}

	return 0;
}

int
vesafb_getcmap(struct vga_pci_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int idx, cnt;
	int rv;

	idx = cm->index;
	cnt = cm->count;

	if (idx >= 256 || cnt > 256 - idx)
		return EINVAL;

	rv = copyout(&sc->sc_cmap_red[idx], cm->red, cnt);
	if (rv)
		return rv;
	rv = copyout(&sc->sc_cmap_green[idx], cm->green, cnt);
	if (rv)
		return rv;
	rv = copyout(&sc->sc_cmap_blue[idx], cm->blue, cnt);
	if (rv)
		return rv;

	return 0;
}

static int
vesafb_getdepthflag(struct modeinfoblock *mi)
{
	int bpp, depth;

	depth = mi->RedMaskSize + mi->GreenMaskSize + mi->BlueMaskSize;
	bpp = mi->BitsPerPixel;
	switch (depth) {
	case 1:
		return WSDISPLAYIO_DEPTH_1;
	case 4:
		return WSDISPLAYIO_DEPTH_4;
	case 8:
		return WSDISPLAYIO_DEPTH_8;
	case 15:
		return WSDISPLAYIO_DEPTH_15;
	case 16:
		return WSDISPLAYIO_DEPTH_16;
	case 24:
		switch (bpp) {
		case 24:
			return WSDISPLAYIO_DEPTH_24_24;
		case 32:
			return WSDISPLAYIO_DEPTH_24_32;
		}
	}
	return 0;
}


int
vesafb_get_supported_depth(struct vga_pci_softc *sc)
{
	int i, depths;
	struct modeinfoblock mi;

	depths = 0;

	if (vesabios_softc == NULL || vesabios_softc->sc_nmodes == 0)
		return 0;

	for (i = 0; i < vesabios_softc->sc_nmodes; i++) {
		vesafb_get_mode_info(sc, vesabios_softc->sc_modes[i], &mi);
		depths |= vesafb_getdepthflag(&mi);
	}
	return depths;
}
