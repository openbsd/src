/*	$OpenBSD: diovar.h,v 1.6 2004/09/29 07:35:52 miod Exp $	*/
/*	$NetBSD: diovar.h,v 1.3 1997/05/05 21:01:33 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Autoconfiguration definitions and prototypes for the DIO bus.
 */

/*
 * Arguments used to attach a device to the DIO bus.
 */
struct dio_attach_args {
	int	da_scode;		/* select code */
	int	da_size;		/* size of address space */
	u_int8_t da_id;			/* primary device id */
	u_int8_t da_secid;		/* secondary device id */
};

/*
 * This structure is used by the autoconfiguration code to lookup
 * the size of a DIO device (not all use one select code).
 */
struct dio_devdata {
	u_int8_t dd_id;			/* primary device id */
	u_int8_t dd_secid;		/* secondary device id */
	int	dd_nscode;		/* number of select codes */
};

/*
 * This structure is used by the autoconfiguration code to print
 * a textual description of a device.
 */
struct dio_devdesc {
	u_int8_t dd_id;			/* primary device id */
	u_int8_t dd_secid;		/* secondary device id */
	const char *dd_desc;		/* description */
};

#define	diocf_scode		cf_loc[0]

#define	DIO_UNKNOWN_SCODE	-1

#ifdef _KERNEL
void	*dio_scodetopa(int);
void	dio_intr_establish(struct isr *, const char *);
void	dio_intr_disestablish(struct isr *);
#endif /* _KERNEL */
