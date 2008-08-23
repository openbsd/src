/*	$OpenBSD: sii_fwio.c,v 1.2 2008/08/23 12:40:22 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/cvax.h>

#include <uvm/uvm_extern.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>
#include <vax/mbus/fwioreg.h>
#include <vax/mbus/fwiovar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <vax/dec/siireg.h>
#include <vax/dec/siivar.h>

struct sii_fwio_softc {
	struct sii_softc sc_dev;
	u_char *sc_buf;
};

int	sii_fwio_match(struct device *, void *, void *);
void	sii_fwio_attach(struct device *, struct device *, void *);

struct cfattach sii_fwio_ca = {
	sizeof(struct sii_fwio_softc), sii_fwio_match, sii_fwio_attach
};

extern struct cfdriver sii_cd;

void	sii_fwio_copyfrombuf(void *, u_int, u_char *, int);
void	sii_fwio_copytobuf(void *, u_char *, u_int, int);
int	sii_fwio_intr(void *);

int
sii_fwio_match(struct device *parent, void *vcf, void *aux)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;

	return strcmp(faa->faa_dev, sii_cd.cd_name) == 0 ? 1 : 0;
}

void
sii_fwio_attach(struct device *parent, struct device *self, void *aux)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;
	struct sii_fwio_softc *sfc = (struct sii_fwio_softc *)self;
	struct sii_softc *sc = &sfc->sc_dev;
	unsigned int vec;

	vec = faa->faa_vecbase + FBIC_DEVIRQ0 * 4;
	printf(" vec %d", vec);

	/*
	 * Map registers.
	 */

	sc->sc_regs =
	    (SIIRegs *)vax_map_physmem(faa->faa_base + FWIO_SII_REG_OFFSET, 1);

	/*
	 * Map buffers.
	 */

	sfc->sc_buf = (u_char *)uvm_km_valloc(kernel_map, FWIO_SII_BUF_SIZE);
	if (sfc->sc_buf == NULL) {
		vax_unmap_physmem(faa->faa_base + FWIO_SII_REG_OFFSET, 1);
		printf(": can't map buffers\n");
		return;
	}

	ioaccess((vaddr_t)sfc->sc_buf, faa->faa_base + FWIO_SII_BUF_OFFSET,
	    FWIO_SII_BUF_SIZE >> VAX_PGSHIFT);

	sc->sii_copytobuf = sii_fwio_copytobuf;
	sc->sii_copyfrombuf = sii_fwio_copyfrombuf;

	/*
	 * Register interrupt handler.
	 */

	if (mbus_intr_establish(vec, IPL_BIO, sii_fwio_intr, sfc,
	    self->dv_xname) != 0) {
		vax_unmap_physmem(faa->faa_base + FWIO_SII_REG_OFFSET, 1);
		uvm_km_free(kernel_map, (vaddr_t)sfc->sc_buf,
		    FWIO_SII_BUF_SIZE);
		printf(": can't establish interrupt\n");
		return;
	}

	/*
	 * Complete attachment.
	 */
	sc->sc_hostid = *(uint8_t *)((vaddr_t)cvax_ssc_ptr + 0x4c0) & 07;
	sii_attach(sc);
}

int
sii_fwio_intr(void *v)
{
	struct sii_softc *sc = (struct sii_softc *)v;
	int rc;
	uint16_t csr;

	/*
	 * FBIC expects edge interrupts, while the sii does level
	 * interrupts. To avoid missing interrupts while servicing,
	 * we disable further device interrupts while servicing.
	 */
	csr = sc->sc_regs->csr;
	sc->sc_regs->csr = csr & ~SII_IE;

	rc = sii_intr(v);

	sc->sc_regs->csr = csr;

	return rc;
}

/*
 * Copy data between the fixed SCSI buffers. The sii driver only ``knows''
 * offsets inside the SCSI buffer.
 */

void
sii_fwio_copyfrombuf(void *v, u_int offs, u_char *dst, int len)
{
	struct sii_fwio_softc *sc = (struct sii_fwio_softc *)v;
	u_char *src = sc->sc_buf + offs;

	memcpy(dst, src, len);
}

void
sii_fwio_copytobuf(void *v, u_char *src, u_int offs, int len)
{
	struct sii_fwio_softc *sc = (struct sii_fwio_softc *)v;
	u_char *dst = sc->sc_buf + offs;

	memcpy(dst, src, len);
}
