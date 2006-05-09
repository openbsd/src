/*	$OpenBSD: vmevar.h,v 1.1.1.1 2006/05/09 18:17:37 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_AVIION_VME_H_
#define	_AVIION_VME_H_

typedef	u_int32_t vme_addr_t;

/*
 * Attachment information for VME devices.
 *
 * The driver is supposed to know in which space it lives if it got an
 * unspecified address (this is to simplify kernel configuration files).
 *
 * Drivers are also supposed to do their interrupt vector allocation
 * themselves.
 */
struct	vme_attach_args {
	/* address locators */
	vme_addr_t	vaa_addr_a16;
	vme_addr_t	vaa_addr_a24;
	vme_addr_t	vaa_addr_a32;
	/* interrupt level if specified */
	u_int		vaa_ipl;
};

int	vmeintr_allocate(u_int, int, u_int *);
#define	VMEINTR_ANY		0x00
#define	VMEINTR_CONTIGUOUS	0x01	/* allocate a contiguous range */
int	vmeintr_establish(u_int, struct intrhand *, const char *);

int	vmebus_get_bst(struct device *, u_int, u_int, bus_space_tag_t *);
#define	VME_A32			0x04	/* 100..000 */
#define	VME_A24			0x03	/* 011..000 */
#define	VME_A16			0x02	/* 010..000 */
#define	VME_D32			0x04	/* 100..000 */
#define	VME_D16			0x02	/* 010..000 */
#define	VME_D8			0x01	/* 001..000 */
void	vmebus_release_bst(struct device *, bus_space_tag_t);

#endif	/* _AVIION_VME_H_ */
