/*	$OpenBSD: vmevar.h,v 1.4 2010/04/21 19:33:47 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, Miodrag Vallat
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
 * VME address and data widths
 */

#define	VME_A32			0x04	/* 100..000 */
#define	VME_A24			0x03	/* 011..000 */
#define	VME_A16			0x02	/* 010..000 */
#define	VME_D32			0x04	/* 100..000 */
#define	VME_D16			0x02	/* 010..000 */
#define	VME_D8			0x01	/* 001..000 */

/*
 * VME address range
 */

struct vme_range {
	u_int		vr_width;
	vme_addr_t	vr_start;
	vme_addr_t	vr_end;
	paddr_t		vr_base;
};

/*
 * Attachment information for VME devices.
 *
 * Drivers are supposed to do their interrupt vector allocation
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

/*
 * There are 256 possible vectors for VME devices.
 * One or more vectors may be allocated by vmeintr_allocate(), and then
 * each vector is setup with vmeintr_establish(). Nothing is done to
 * prevent the vector to be used in-between, so make sure no interrupt
 * can occur between the vector allocation and the interrupt handler
 * registration.
 */
#define	NVMEINTR	256
extern intrhand_t vmeintr_handlers[NVMEINTR];

int	vmeintr_allocate(u_int, int, int, u_int *);
#define	VMEINTR_ANY		0x00	/* any vector will do */
#define	VMEINTR_CONTIGUOUS	0x01	/* allocate a contiguous range */
#define	VMEINTR_SHARED		0x00	/* sharing is ok */
#define	VMEINTR_EXCLUSIVE	0x02	/* do not share this vector */
void	vmeintr_disestablish(u_int, struct intrhand *);
int	vmeintr_establish(u_int, struct intrhand *, const char *);

/*
 * VME device drivers need to obtain their bus_space_tag_t with
 * vmebus_get_bst(), specifying the address and data width to use for
 * bus accesses.
 * Resources associated to the tag can be released with vmebus_release_bst()
 * when bus accesses are no longer necessary.
 */
int	vmebus_get_bst(struct device *, u_int, u_int, bus_space_tag_t *);
void	vmebus_release_bst(struct device *, bus_space_tag_t);

#endif	/* _AVIION_VME_H_ */
