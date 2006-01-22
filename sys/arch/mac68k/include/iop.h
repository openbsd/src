/*	$OpenBSD: iop.h,v 1.1 2006/01/22 13:53:16 miod Exp $	*/
/*	$NetBSD: iopreg.h,v 1.7 2005/12/11 12:18:03 christos Exp $	*/

/*
 * Copyright (c) 2000 Allen Briggs.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define SCC_IOP		0
#define ISM_IOP		1

#define IOP_CS_BYPASS	0x01
#define IOP_CS_AUTOINC	0x02
#define IOP_CS_RUN	0x04
#define IOP_CS_IRQ	0x08
#define IOP_CS_INT0	0x10
#define IOP_CS_INT1	0x20
#define IOP_CS_HWINT	0x40
#define IOP_CS_DMAINACT	0x80

#define IOP_RESET	(IOP_CS_DMAINACT | IOP_CS_AUTOINC)
#define IOP_BYPASS	\
		(IOP_CS_BYPASS | IOP_CS_AUTOINC | IOP_CS_RUN | IOP_CS_DMAINACT)
#define IOP_INTERRUPT	(IOP_CS_INT0 | IOP_CS_INT1)

typedef struct {
	volatile u_char	ram_hi;
	u_char		pad0;
	volatile u_char	ram_lo;
	u_char		pad1;
	volatile u_char	control_status;
	u_char		pad2[3];
	volatile u_char	data;
	u_char		pad3[23];
	union {
		struct {
			volatile u_char sccb_cmd;
			u_char		pad0;
			volatile u_char scca_cmd;
			u_char		pad1;
			volatile u_char sccb_data;
			u_char		pad2;
			volatile u_char scca_data;
			u_char		pad3;
		} scc;
		struct {
			volatile u_char wdata;
			u_char		pad0;
			/* etc... */
		} iwm;
	} bypass;
} IOPHW;

void	iop_serial_compatible(void);
