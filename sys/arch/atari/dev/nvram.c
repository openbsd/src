/*	$NetBSD: nvram.c,v 1.1 1996/01/06 20:11:08 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Nvram driver
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/uio.h>

#include <machine/iomap.h>

#include <atari/dev/clockreg.h>
#include <atari/dev/nvramvar.h>

#include "nvr.h"

#define	MC_NVRAM_CSUM	(MC_NVRAM_START + MC_NVRAM_SIZE - 2)

#if NNVR > 0
static void	nvram_set_csum __P((u_char csum));
static int	nvram_csum_valid __P((u_char csum));
static u_char	nvram_csum __P((void));

/*
 * Auto config stuff....
 */
static void	nvr_attach __P((struct device *, struct device *, void *));
static int	nvr_match __P((struct device *, struct cfdata *, void *));

struct cfdriver nvrcd = {
	NULL, "nvr", (cfmatch_t)nvr_match, nvr_attach,
	DV_DULL, sizeof(struct nvr_softc), NULL, 0 };

/*ARGSUSED*/
static	int
nvr_match(pdp, cfp, auxp)
struct	device *pdp;
struct	cfdata *cfp;
void	*auxp;
{
	if (!strcmp((char *)auxp, "nvr"))
		return (1);
	return (0);
}

/*ARGSUSED*/
static void
nvr_attach(pdp, dp, auxp)
struct	device *pdp, *dp;
void	*auxp;
{
	struct nvr_softc	*nvr_soft;
	int			nreg;
	
	/*
	 * Check the validity of the NVram contents
	 */
	if (!nvram_csum_valid(nvram_csum())) {
		printf(": Invalid checksum - re-initialized");
		for (nreg = MC_NVRAM_START; nreg < MC_NVRAM_CSUM; nreg++)
			mc146818_write(RTC, nreg, 0);
		nvram_set_csum(nvram_csum());
	}
	nvr_soft = nvrcd.cd_devs[0];
	nvr_soft->nvr_flags = NVR_CONFIGURED;
	printf("\n");
}
/*
 * End of auto config stuff....
 */
#endif /* NNVR > 0 */

/*
 * Kernel internal interface
 */
int
nvr_get_byte(byteno)
int	byteno;
{
#if NNVR > 0
	struct nvr_softc	*nvr_soft;

	nvr_soft = nvrcd.cd_devs[0];
	if (!(nvr_soft->nvr_flags & NVR_CONFIGURED))
		return(NVR_INVALID);
	return (mc146818_read(RTC, byteno + MC_NVRAM_START) & 0xff);
#else
	return(NVR_INVALID);
#endif /* NNVR > 0 */
}

#if NNVR > 0

nvram_uio(uio)
struct uio	*uio;
{
	int			i;
	off_t			offset;
	int			nleft;
	u_char			buf[MC_NVRAM_CSUM - MC_NVRAM_START + 1];
	u_char			*p;
	struct nvr_softc	*nvr_soft;

	nvr_soft = nvrcd.cd_devs[0];
	if (!(nvr_soft->nvr_flags & NVR_CONFIGURED))
		return ENXIO;

#ifdef NV_DEBUG
	printf("Request to transfer %d bytes offset: %d, %s nvram\n",
				(long)uio->uio_resid, (long)uio->uio_offset,
				(uio->uio_rw == UIO_READ) ? "from" : "to");
#endif /* NV_DEBUG */

	offset = uio->uio_offset + MC_NVRAM_START;
	nleft  = uio->uio_resid;
	if (offset + nleft >= MC_NVRAM_CSUM) {
		if (offset == MC_NVRAM_CSUM)
			return (0);
		nleft = MC_NVRAM_CSUM - offset;
		if (nleft <= 0)
			return (EINVAL);
	}
#ifdef NV_DEBUG
	printf("Translated: offset = %d, bytes: %d\n", (long)offset, nleft);
#endif /* NV_DEBUG */

	if (uio->uio_rw == UIO_READ) {
		for (i = 0, p = buf; i < nleft; i++, p++)
			*p =  mc146818_read(RTC, offset + i);
	}
	if (i = uiomove(buf, nleft, uio))
		return (i);
	if (uio->uio_rw == UIO_WRITE) {
		for (i = 0, p = buf; i < nleft; i++, p++)
			mc146818_write(RTC, offset + i, *p);
		nvram_set_csum(nvram_csum());
	}
	return(0);
}

static u_char
nvram_csum()
{
	u_char	csum;
	int	nreg;
	
	for (csum = 0, nreg = MC_NVRAM_START; nreg < MC_NVRAM_CSUM; nreg++)
		csum += mc146818_read(RTC, nreg);
	return(csum);
}

static int
nvram_csum_valid(csum)
u_char	csum;
{
	if (((~csum & 0xff) != mc146818_read(RTC, MC_NVRAM_CSUM))
		|| (csum != mc146818_read(RTC, MC_NVRAM_CSUM + 1)))
		return 0;
	return 1;
}

static void
nvram_set_csum(csum)
u_char	csum;
{
	mc146818_write(RTC, MC_NVRAM_CSUM,    ~csum);
	mc146818_write(RTC, MC_NVRAM_CSUM + 1, csum);
}
#endif /* NNVR > 0 */
